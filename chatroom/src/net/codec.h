#pragma once

#include "net/buffer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace chat {

constexpr std::uint32_t kMaxFrameSize = 1024 * 1024;

std::string encode_frame(const std::string& body);
bool try_decode_frame(Buffer& input, std::string& body, std::string& error);

}  // namespace chat

