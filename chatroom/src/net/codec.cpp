#include "net/codec.h"

#include <arpa/inet.h>
#include <cstring>

namespace chat {

std::string encode_frame(const std::string& body) {
    std::uint32_t n = static_cast<std::uint32_t>(body.size());
    std::uint32_t be = htonl(n);
    std::string out;
    out.resize(4);
    std::memcpy(out.data(), &be, 4);
    out += body;
    return out;
}

bool try_decode_frame(Buffer& input, std::string& body, std::string& error) {
    body.clear();
    error.clear();
    if (input.readable() < 4) return false;

    std::uint32_t be = 0;
    std::memcpy(&be, input.peek(), 4);
    std::uint32_t len = ntohl(be);
    if (len > kMaxFrameSize) {
        error = "frame too large";
        return false;
    }
    if (input.readable() < 4 + len) return false;

    input.consume(4);
    body = input.take(len);
    return true;
}

}  // namespace chat

