#include "common/log.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace chat {
namespace {

std::mutex g_log_mu;

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

std::string now_string() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream os;
    os << std::put_time(&tm, "%F %T");
    return os.str();
}

}  // namespace

void log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_log_mu);
    std::cerr << now_string() << " [" << level_name(level) << "]"
              << " tid=" << std::this_thread::get_id()
              << " " << message << "\n";
}

void log_info(const std::string& message) { log(LogLevel::Info, message); }
void log_warn(const std::string& message) { log(LogLevel::Warn, message); }
void log_error(const std::string& message) { log(LogLevel::Error, message); }

}  // namespace chat

