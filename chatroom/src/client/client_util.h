#pragma once

#include "common/nonblock.h"
#include "common/unique_fd.h"
#include "net/buffer.h"
#include "net/codec.h"
#include "protocol/kv_message.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

namespace chat {

inline UniqueFd connect_blocking(const std::string& host, int port, std::string& error) {
    UniqueFd fd(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (!fd.valid()) {
        error = std::strerror(errno);
        return UniqueFd();
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        error = "invalid host";
        return UniqueFd();
    }
    if (::connect(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        error = std::strerror(errno);
        return UniqueFd();
    }
    return fd;
}

inline bool write_all_fd(int fd, const std::string& data, std::string& error) {
    std::size_t off = 0;
    while (off < data.size()) {
        ssize_t n = ::send(fd, data.data() + off, data.size() - off, MSG_NOSIGNAL);
        if (n > 0) {
            off += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        error = std::strerror(errno);
        return false;
    }
    return true;
}

inline bool send_message_fd(int fd, const KvMessage& msg, std::string& error) {
    return write_all_fd(fd, encode_frame(serialize_kv(msg)), error);
}

inline bool recv_message_fd(int fd, KvMessage& msg, std::string& error) {
    Buffer input;
    char buf[8192];
    for (;;) {
        std::string body;
        if (try_decode_frame(input, body, error)) return parse_kv(body, msg, error);
        if (!error.empty()) return false;
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            input.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            error = "peer closed";
            return false;
        }
        if (errno == EINTR) continue;
        error = std::strerror(errno);
        return false;
    }
}

class BlockingMessageReader {
public:
    explicit BlockingMessageReader(int fd) : fd_(fd) {}

    bool recv(KvMessage& msg, std::string& error) {
        char buf[8192];
        for (;;) {
            std::string body;
            if (try_decode_frame(input_, body, error)) return parse_kv(body, msg, error);
            if (!error.empty()) return false;
            ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
            if (n > 0) {
                input_.append(buf, static_cast<std::size_t>(n));
                continue;
            }
            if (n == 0) {
                error = "peer closed";
                return false;
            }
            if (errno == EINTR) continue;
            error = std::strerror(errno);
            return false;
        }
    }

private:
    int fd_;
    Buffer input_;
};

}  // namespace chat
