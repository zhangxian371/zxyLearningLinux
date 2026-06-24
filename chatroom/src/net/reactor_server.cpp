#include "net/reactor_server.h"

#include "common/log.h"
#include "common/nonblock.h"
#include "net/codec.h"
#include "protocol/kv_message.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

namespace chat {
namespace {

constexpr std::uint32_t kListenTag = 1;
constexpr std::uint32_t kWakeTag = 2;
constexpr std::size_t kMaxInputBuffer = 2 * 1024 * 1024;
constexpr std::size_t kMaxOutputBuffer = 2 * 1024 * 1024;
constexpr auto kIdleTimeout = std::chrono::seconds(1800);

std::string errno_string(const char* what) {
    return std::string(what) + ": " + std::strerror(errno);
}

std::string peer_string(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    std::ostringstream os;
    os << ip << ":" << ntohs(addr.sin_port);
    return os.str();
}

}  // namespace

ReactorServer::ReactorServer(std::string host, int port, std::size_t worker_threads)
    : host_(std::move(host)), port_(port), workers_(worker_threads, 4096) {}

ReactorServer::~ReactorServer() {
    stop();
}

bool ReactorServer::start() {
    if (!setup_listener() || !setup_epoll() || !setup_wakeup()) return false;
    add_fd(listen_fd_.get(), EPOLLIN | EPOLLET, reinterpret_cast<void*>(static_cast<uintptr_t>(kListenTag)));
    add_fd(wake_fd_.get(), EPOLLIN | EPOLLET, reinterpret_cast<void*>(static_cast<uintptr_t>(kWakeTag)));
    running_ = true;
    log_info("chat_server listening on " + host_ + ":" + std::to_string(port_));
    return true;
}

void ReactorServer::stop() {
    running_ = false;
    wakeup();
    workers_.stop();
}

void ReactorServer::loop() {
    constexpr int kMaxEvents = 128;
    std::vector<epoll_event> events(kMaxEvents);
    while (running_) {
        int n = ::epoll_wait(epoll_fd_.get(), events.data(), kMaxEvents, 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            log_error(errno_string("epoll_wait"));
            break;
        }
        for (int i = 0; i < n; ++i) {
            auto tag = reinterpret_cast<uintptr_t>(events[i].data.ptr);
            if (tag == kListenTag) {
                accept_loop();
            } else if (tag == kWakeTag) {
                handle_wakeup();
            } else {
                handle_connection(static_cast<Connection*>(events[i].data.ptr), events[i].events);
            }
        }
        drain_tasks();
        reap_idle();
    }
}

bool ReactorServer::setup_listener() {
    UniqueFd fd(::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
    if (!fd.valid()) {
        log_error(errno_string("socket"));
        return false;
    }
    int yes = 1;
    ::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        log_error("invalid listen host: " + host_);
        return false;
    }
    if (::bind(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        log_error(errno_string("bind"));
        return false;
    }
    if (::listen(fd.get(), 128) < 0) {
        log_error(errno_string("listen"));
        return false;
    }
    listen_fd_ = std::move(fd);
    return true;
}

bool ReactorServer::setup_epoll() {
    epoll_fd_.reset(::epoll_create1(EPOLL_CLOEXEC));
    if (!epoll_fd_.valid()) {
        log_error(errno_string("epoll_create1"));
        return false;
    }
    return true;
}

bool ReactorServer::setup_wakeup() {
    wake_fd_.reset(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    if (!wake_fd_.valid()) {
        log_error(errno_string("eventfd"));
        return false;
    }
    return true;
}

void ReactorServer::add_fd(int fd, std::uint32_t events, void* ptr) {
    epoll_event ev{};
    ev.events = events;
    ev.data.ptr = ptr;
    if (::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, fd, &ev) < 0) log_error(errno_string("epoll_ctl add"));
}

void ReactorServer::mod_fd(int fd, std::uint32_t events, void* ptr) {
    epoll_event ev{};
    ev.events = events;
    ev.data.ptr = ptr;
    if (::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_MOD, fd, &ev) < 0) log_error(errno_string("epoll_ctl mod"));
}

void ReactorServer::del_fd(int fd) {
    ::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr);
}

void ReactorServer::accept_loop() {
    for (;;) {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        int fd = ::accept4(listen_fd_.get(), reinterpret_cast<sockaddr*>(&addr), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            log_error(errno_string("accept4"));
            break;
        }

        auto conn = std::make_unique<Connection>();
        conn->id = next_conn_id_++;
        conn->fd.reset(fd);
        conn->peer = peer_string(addr);
        conn->last_active = std::chrono::steady_clock::now();
        Connection* raw = conn.get();
        connections_[conn->id] = std::move(conn);
        add_fd(fd, EPOLLIN | EPOLLET | EPOLLRDHUP, raw);
        log_info("accepted conn=" + std::to_string(raw->id) + " peer=" + raw->peer);
    }
}

void ReactorServer::handle_connection(Connection* conn, std::uint32_t events) {
    if (!conn || !connections_.count(conn->id)) return;
    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) conn->closing = true;
    if (events & EPOLLIN) read_loop(*conn);
    if (connections_.count(conn->id) && (events & EPOLLOUT)) write_loop(*conn);
    if (connections_.count(conn->id)) update_interest(*conn);
}

void ReactorServer::read_loop(Connection& conn) {
    char buf[64 * 1024];
    for (;;) {
        ssize_t n = ::recv(conn.fd.get(), buf, sizeof(buf), 0);
        if (n > 0) {
            conn.last_active = std::chrono::steady_clock::now();
            conn.input.append(buf, static_cast<std::size_t>(n));
            if (conn.input.capacity_bytes() > kMaxInputBuffer) {
                conn.closing = true;
                return;
            }
            for (;;) {
                std::string body;
                std::string error;
                bool got = try_decode_frame(conn.input, body, error);
                if (!error.empty()) {
                    send_to(conn.id, make_response(0, false, "BAD_FRAME", error));
                    conn.closing = true;
                    return;
                }
                if (!got) break;
                submit_frame(conn.id, conn.peer, body);
            }
            continue;
        }
        if (n == 0) {
            conn.closing = true;
            return;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        log_warn(errno_string("recv"));
        conn.closing = true;
        return;
    }
}

void ReactorServer::write_loop(Connection& conn) {
    while (!conn.output.empty()) {
        ssize_t n = ::send(conn.fd.get(), conn.output.peek(), conn.output.readable(), MSG_NOSIGNAL);
        if (n > 0) {
            conn.last_active = std::chrono::steady_clock::now();
            conn.output.consume(static_cast<std::size_t>(n));
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        conn.closing = true;
        return;
    }
}

void ReactorServer::close_connection(std::uint64_t id) {
    auto it = connections_.find(id);
    if (it == connections_.end()) return;
    del_fd(it->second->fd.get());
    service_.disconnect(id);
    log_info("closed conn=" + std::to_string(id));
    connections_.erase(it);
}

void ReactorServer::submit_frame(std::uint64_t conn_id, const std::string& peer, const std::string& body) {
    KvMessage request;
    std::string error;
    if (!parse_kv(body, request, error)) {
        send_to(conn_id, make_response(0, false, "BAD_REQUEST", error));
        return;
    }

    bool ok = workers_.submit([this, conn_id, peer, request] {
        ServiceResult result = service_.handle(conn_id, peer, request);
        queue_task([this, result = std::move(result)] {
            for (const auto& out : result.outbound) send_to(out.conn_id, out.message);
        });
    });
    if (!ok) send_to(conn_id, make_response(request.get_int("seq", 0), false, "BUSY", "server queue is full"));
}

void ReactorServer::send_to(std::uint64_t conn_id, const KvMessage& msg) {
    auto it = connections_.find(conn_id);
    if (it == connections_.end()) return;
    std::string frame = encode_frame(serialize_kv(msg));
    it->second->output.append(frame);
    if (it->second->output.capacity_bytes() > kMaxOutputBuffer) {
        it->second->closing = true;
        return;
    }
    update_interest(*it->second);
}

void ReactorServer::queue_task(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(task_mu_);
        pending_tasks_.push_back(std::move(task));
    }
    wakeup();
}

void ReactorServer::drain_tasks() {
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(task_mu_);
        tasks.swap(pending_tasks_);
    }
    for (auto& task : tasks) task();
}

void ReactorServer::wakeup() {
    if (!wake_fd_.valid()) return;
    std::uint64_t one = 1;
    ssize_t n = ::write(wake_fd_.get(), &one, sizeof(one));
    (void)n;
}

void ReactorServer::handle_wakeup() {
    for (;;) {
        std::uint64_t value = 0;
        ssize_t n = ::read(wake_fd_.get(), &value, sizeof(value));
        if (n > 0) continue;
        if (n < 0 && errno == EINTR) continue;
        break;
    }
    drain_tasks();
}

void ReactorServer::reap_idle() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::uint64_t> close_ids;
    for (const auto& [id, conn] : connections_) {
        if (conn->closing && conn->output.empty()) close_ids.push_back(id);
        else if (now - conn->last_active > kIdleTimeout) close_ids.push_back(id);
    }
    for (std::uint64_t id : close_ids) close_connection(id);
}

void ReactorServer::update_interest(Connection& conn) {
    if (!connections_.count(conn.id)) return;
    if (conn.closing && conn.output.empty()) {
        close_connection(conn.id);
        return;
    }
    std::uint32_t events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (!conn.output.empty()) events |= EPOLLOUT;
    mod_fd(conn.fd.get(), events, &conn);
}

}  // namespace chat

