#pragma once

#include <map>
#include <string>

namespace chat {

struct KvMessage {
    std::map<std::string, std::string> fields;

    std::string get(const std::string& key, const std::string& fallback = "") const;
    int get_int(const std::string& key, int fallback = 0) const;
    void set(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
};

std::string percent_encode(const std::string& input);
bool percent_decode(const std::string& input, std::string& output);
std::string serialize_kv(const KvMessage& msg);
bool parse_kv(const std::string& body, KvMessage& msg, std::string& error);

KvMessage make_response(int seq, bool ok, const std::string& code, const std::string& message);

}  // namespace chat

