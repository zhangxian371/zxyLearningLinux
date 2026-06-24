#include "client/client_util.h"

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

void print_message(const chat::KvMessage& msg) {
    std::cout << "< ";
    for (const auto& [k, v] : msg.fields) {
        std::cout << k << "=" << v << " ";
    }
    std::cout << "\n";
}

chat::KvMessage base(int& seq, const std::string& type) {
    chat::KvMessage msg;
    msg.set_int("seq", seq++);
    msg.set("type", type);
    return msg;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <host> <port>\n";
        return 2;
    }
    std::string error;
    chat::UniqueFd fd = chat::connect_blocking(argv[1], std::atoi(argv[2]), error);
    if (!fd.valid()) {
        std::cerr << "connect: " << error << "\n";
        return 1;
    }

    std::atomic<bool> running{true};
    chat::BlockingMessageReader message_reader(fd.get());
    std::thread reader([&] {
        while (running) {
            chat::KvMessage msg;
            std::string err;
            if (!message_reader.recv(msg, err)) {
                if (running) std::cerr << "recv: " << err << "\n";
                running = false;
                break;
            }
            print_message(msg);
        }
    });

    int seq = 1;
    std::cout << "commands:\n"
              << "  /register <name> <password>\n"
              << "  /login <name> <password>\n"
              << "  /auth <token>\n"
              << "  /join <room_id>\n"
              << "  /send <room_id> <text>\n"
              << "  /dm <user> <text>\n"
              << "  /history <room_id> [limit]\n"
              << "  /quit\n";

    std::string line;
    while (running && std::getline(std::cin, line)) {
        if (line == "/quit") break;
        std::istringstream is(line);
        std::string cmd;
        is >> cmd;
        chat::KvMessage msg;

        if (cmd == "/register") {
            std::string name, pass;
            is >> name >> pass;
            msg = base(seq, "REGISTER");
            msg.set("name", name);
            msg.set("password", pass);
        } else if (cmd == "/login") {
            std::string name, pass;
            is >> name >> pass;
            msg = base(seq, "LOGIN");
            msg.set("name", name);
            msg.set("password", pass);
        } else if (cmd == "/auth") {
            std::string token;
            is >> token;
            msg = base(seq, "AUTH");
            msg.set("token", token);
        } else if (cmd == "/join") {
            int room = 1;
            is >> room;
            msg = base(seq, "JOIN");
            msg.set_int("room_id", room);
        } else if (cmd == "/send") {
            int room = 1;
            is >> room;
            std::string text;
            std::getline(is, text);
            if (!text.empty() && text[0] == ' ') text.erase(0, 1);
            msg = base(seq, "SEND");
            msg.set_int("room_id", room);
            msg.set("content", text);
        } else if (cmd == "/dm") {
            std::string to;
            is >> to;
            std::string text;
            std::getline(is, text);
            if (!text.empty() && text[0] == ' ') text.erase(0, 1);
            msg = base(seq, "DM");
            msg.set("to", to);
            msg.set("content", text);
        } else if (cmd == "/history") {
            int room = 1;
            int limit = 20;
            is >> room;
            if (!(is >> limit)) limit = 20;
            msg = base(seq, "HISTORY");
            msg.set_int("room_id", room);
            msg.set_int("limit", limit);
        } else {
            std::cout << "unknown command\n";
            continue;
        }

        if (!chat::send_message_fd(fd.get(), msg, error)) {
            std::cerr << "send: " << error << "\n";
            break;
        }
    }

    running = false;
    ::shutdown(fd.get(), SHUT_RDWR);
    if (reader.joinable()) reader.join();
    return 0;
}
