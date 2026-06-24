#include "protocol/kv_message.h"

#include <cctype>
#include <sstream>

namespace chat {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool valid_key(const std::string& key) {
    if (key.empty()) return false;
    for (char c : key) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

}  // namespace

std::string KvMessage::get(const std::string& key, const std::string& fallback) const {
    auto it = fields.find(key);
    return it == fields.end() ? fallback : it->second;
}

int KvMessage::get_int(const std::string& key, int fallback) const {
    auto it = fields.find(key);
    if (it == fields.end()) return fallback;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return fallback;
    }
}

void KvMessage::set(const std::string& key, const std::string& value) {
    fields[key] = value;
}

void KvMessage::set_int(const std::string& key, int value) {
    fields[key] = std::to_string(value);
}

std::string percent_encode(const std::string& input) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : input) {
        bool keep = std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':' || c == '/';
        if (keep) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

bool percent_decode(const std::string& input, std::string& output) {
    output.clear();
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] != '%') {
            output.push_back(input[i]);
            continue;
        }
        if (i + 2 >= input.size()) return false;
        int hi = hex_value(input[i + 1]);
        int lo = hex_value(input[i + 2]);
        if (hi < 0 || lo < 0) return false;
        output.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
    }
    return true;
}

std::string serialize_kv(const KvMessage& msg) {
    std::ostringstream os;
    for (const auto& [key, value] : msg.fields) {
        os << key << '=' << percent_encode(value) << '\n';
    }
    os << '\n';
    return os.str();
}

bool parse_kv(const std::string& body, KvMessage& msg, std::string& error) {
    msg.fields.clear();
    error.clear();

    std::istringstream is(body);
    std::string line;
    while (std::getline(is, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            error = "missing '='";
            return false;
        }
        std::string key = line.substr(0, pos);
        if (!valid_key(key)) {
            error = "invalid key";
            return false;
        }
        std::string decoded;
        if (!percent_decode(line.substr(pos + 1), decoded)) {
            error = "invalid percent encoding";
            return false;
        }
        msg.fields[key] = decoded;
    }
    return true;
}

KvMessage make_response(int seq, bool ok, const std::string& code, const std::string& message) {
    KvMessage msg;
    msg.set_int("seq", seq);
    msg.set("ok", ok ? "1" : "0");
    msg.set("code", code);
    msg.set("message", message);
    return msg;
}

}  // namespace chat

