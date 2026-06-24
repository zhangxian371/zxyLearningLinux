#include "client/client_util.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

chat::KvMessage req(int seq, const std::string& type) {
    chat::KvMessage msg;
    msg.set_int("seq", seq);
    msg.set("type", type);
    return msg;
}

bool roundtrip(int fd, chat::BlockingMessageReader& reader, const chat::KvMessage& out, chat::KvMessage& in) {
    std::string err;
    if (!chat::send_message_fd(fd, out, err)) return false;
    return reader.recv(in, err);
}

void client_worker(const std::string& host, int port, int id, int messages, std::atomic<int>& ok, std::atomic<int>& fail) {
    std::string err;
    chat::UniqueFd fd = chat::connect_blocking(host, port, err);
    if (!fd.valid()) {
        ++fail;
        return;
    }
    int seq = 1;
    chat::BlockingMessageReader reader(fd.get());
    std::string name = "bench" + std::to_string(id);
    std::string pass = "pw" + std::to_string(id);
    chat::KvMessage resp;

    auto reg = req(seq++, "REGISTER");
    reg.set("name", name);
    reg.set("password", pass);
    roundtrip(fd.get(), reader, reg, resp);

    auto login = req(seq++, "LOGIN");
    login.set("name", name);
    login.set("password", pass);
    if (!roundtrip(fd.get(), reader, login, resp) || resp.get("ok") != "1") {
        ++fail;
        return;
    }
    std::string token = resp.get("token");

    auto auth = req(seq++, "AUTH");
    auth.set("token", token);
    if (!roundtrip(fd.get(), reader, auth, resp) || resp.get("ok") != "1") {
        ++fail;
        return;
    }

    auto join = req(seq++, "JOIN");
    join.set_int("room_id", 1);
    if (!roundtrip(fd.get(), reader, join, resp) || resp.get("ok") != "1") {
        ++fail;
        return;
    }

    for (int i = 0; i < messages; ++i) {
        auto send = req(seq++, "SEND");
        send.set_int("room_id", 1);
        send.set("content", "hello from " + name + " #" + std::to_string(i));
        std::string e;
        if (!chat::send_message_fd(fd.get(), send, e)) {
            ++fail;
            return;
        }
        ++ok;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 8080;
    int clients = 100;
    int messages = 10;
    if (argc >= 2) clients = std::atoi(argv[1]);
    if (argc >= 3) messages = std::atoi(argv[2]);
    if (argc >= 4) port = std::atoi(argv[3]);

    std::atomic<int> ok{0};
    std::atomic<int> fail{0};
    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < clients; ++i) {
        threads.emplace_back(client_worker, host, port, i, messages, std::ref(ok), std::ref(fail));
    }
    for (auto& t : threads) t.join();
    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    std::cout << "clients=" << clients
              << " messages_per_client=" << messages
              << " sent=" << ok.load()
              << " fail=" << fail.load()
              << " seconds=" << seconds
              << " msg_per_sec=" << (seconds > 0 ? ok.load() / seconds : 0)
              << "\n";
    return fail.load() == 0 ? 0 : 1;
}
