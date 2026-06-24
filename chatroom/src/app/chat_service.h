#pragma once

#include "protocol/kv_message.h"

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace chat {

struct OutboundMessage {
    std::uint64_t conn_id{0};
    KvMessage message;
};

struct ServiceResult {
    std::vector<OutboundMessage> outbound;
};

class ChatService {
public:
    ServiceResult handle(std::uint64_t conn_id, const std::string& peer, const KvMessage& request);
    void disconnect(std::uint64_t conn_id);

private:
    struct User {
        int id{0};
        std::string name;
        std::string password_hash;
    };

    struct StoredMessage {
        int id{0};
        int room_id{0};
        int from_user{0};
        int to_user{0};
        std::string from_name;
        std::string content;
    };

    KvMessage response_for(const KvMessage& request, bool ok, const std::string& code, const std::string& message) const;
    ServiceResult single(std::uint64_t conn_id, const KvMessage& response) const;
    int authed_user_locked(std::uint64_t conn_id) const;
    std::string hash_password(const std::string& password, const std::string& salt) const;
    std::string make_token(int user_id);
    std::string user_name_locked(int user_id) const;

    ServiceResult handle_register(std::uint64_t conn_id, const KvMessage& request);
    ServiceResult handle_login(std::uint64_t conn_id, const std::string& peer, const KvMessage& request);
    ServiceResult handle_auth(std::uint64_t conn_id, const KvMessage& request);
    ServiceResult handle_join(std::uint64_t conn_id, const KvMessage& request);
    ServiceResult handle_send(std::uint64_t conn_id, const KvMessage& request);
    ServiceResult handle_dm(std::uint64_t conn_id, const KvMessage& request);
    ServiceResult handle_history(std::uint64_t conn_id, const KvMessage& request);

    mutable std::mutex mu_;
    int next_user_id_{1};
    int next_message_id_{1};
    std::unordered_map<std::string, User> users_by_name_;
    std::unordered_map<int, std::string> names_by_id_;
    std::unordered_map<std::string, int> token_to_user_;
    std::unordered_map<std::uint64_t, int> conn_to_user_;
    std::unordered_map<int, std::uint64_t> user_to_conn_;
    std::unordered_map<int, std::set<int>> room_members_;
    std::vector<StoredMessage> messages_;
    std::unordered_map<std::string, int> login_attempts_;
};

}  // namespace chat

