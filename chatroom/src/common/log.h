#pragma once

#include <mutex>
#include <string>

namespace chat {

enum class LogLevel { Info, Warn, Error };

void log(LogLevel level, const std::string& message);
void log_info(const std::string& message);
void log_warn(const std::string& message);
void log_error(const std::string& message);

}  // namespace chat

