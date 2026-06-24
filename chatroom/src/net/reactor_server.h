#pragma once

#include "app/chat_service.h"
#include "base/thread_pool.h"
#include "common/unique_fd.h"
#include "net/buffer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace chat {

class ReactorServer {
public:
    ReactorServer(std::string host, int port, std::size_t worker_threads);
    ~ReactorServer();

    bool start();
    void stop();
    void loop();

private:
    struct Connection {
        std::uint64_t id{0};
        UniqueFd fd;
        std::string peer;
        Buffer input;
        Buffer output;
        bool closing{false};
        std::chrono::steady_clock::time_point last_active;
    };

    bool setup_listener();
    bool setup_epoll();
    bool setup_wakeup();
    void add_fd(int fd, std::uint32_t events, void* ptr);
    void mod_fd(int fd, std::uint32_t events, void* ptr);
    void del_fd(int fd);

    void accept_loop();
    void handle_connection(Connection* conn, std::uint32_t events);
    void read_loop(Connection& conn);
    void write_loop(Connection& conn);
    void close_connection(std::uint64_t id);
    void submit_frame(std::uint64_t conn_id, const std::string& peer, const std::string& body);
    void send_to(std::uint64_t conn_id, const KvMessage& msg);
    void queue_task(std::function<void()> task);
    void drain_tasks();
    void wakeup();
    void handle_wakeup();
    void reap_idle();
    void update_interest(Connection& conn);

    std::string host_;
    int port_;
    UniqueFd listen_fd_;
    UniqueFd epoll_fd_;
    UniqueFd wake_fd_;
    ThreadPool workers_;
    ChatService service_;
    std::atomic<bool> running_{false};
    std::uint64_t next_conn_id_{1};
    std::unordered_map<std::uint64_t, std::unique_ptr<Connection>> connections_;

    std::mutex task_mu_;
    std::vector<std::function<void()>> pending_tasks_;
};

}  // namespace chat

