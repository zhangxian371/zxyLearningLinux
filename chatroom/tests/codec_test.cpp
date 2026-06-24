#include "net/codec.h"

#include <cassert>
#include <iostream>

int main() {
    chat::Buffer input;
    std::string frame = chat::encode_frame("hello");
    input.append(frame);
    std::string body;
    std::string error;
    assert(chat::try_decode_frame(input, body, error));
    assert(body == "hello");
    assert(error.empty());
    assert(input.empty());

    input.append(frame.data(), 3);
    assert(!chat::try_decode_frame(input, body, error));
    assert(error.empty());
    input.append(frame.data() + 3, frame.size() - 3);
    assert(chat::try_decode_frame(input, body, error));
    assert(body == "hello");
    std::cout << "codec_test passed\n";
    return 0;
}

