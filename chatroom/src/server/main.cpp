#include "net/reactor_server.h"

#include <csignal>
#include <cstdlib>
#include <iostream>

namespace {

chat::ReactorServer* g_server = nullptr;

void on_signal(int) {
    if (g_server) g_server->stop();
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    int port = 8080;
    std::size_t workers = 4;

    if (argc >= 2) port = std::atoi(argv[1]);
    if (argc >= 3) workers = static_cast<std::size_t>(std::atoi(argv[2]));

    chat::ReactorServer server(host, port, workers);
    g_server = &server;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (!server.start()) return 1;
    server.loop();
    return 0;
}

