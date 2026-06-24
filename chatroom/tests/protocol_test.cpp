#include "protocol/kv_message.h"

#include <cassert>
#include <iostream>

int main() {
    chat::KvMessage msg;
    msg.set_int("seq", 7);
    msg.set("type", "SEND");
    msg.set("content", "hello world\nx=y");
    std::string body = chat::serialize_kv(msg);

    chat::KvMessage parsed;
    std::string error;
    assert(chat::parse_kv(body, parsed, error));
    assert(parsed.get_int("seq") == 7);
    assert(parsed.get("type") == "SEND");
    assert(parsed.get("content") == "hello world\nx=y");
    std::cout << "protocol_test passed\n";
    return 0;
}

