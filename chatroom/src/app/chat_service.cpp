#include "app/chat_service.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <sstream>

namespace chat {
namespace {

bool missing(const KvMessage& msg, const std::string& key) {
    return msg.fields.find(key) == msg.fields.end() || msg.get(key).empty();
}

}  // namespace

ServiceResult ChatService::handle(std::uint64_t conn_id, const std::string& peer, const KvMessage& request) {
    const std::string type = request.get("type");
    if (type == "REGISTER") return handle_register(conn_id, request);
    if (type == "LOGIN") return handle_login(conn_id, peer, request);
    if (type == "AUTH") return handle_auth(conn_id, request);
    if (type == "JOIN") return handle_join(conn_id, request);
    if (type == "SEND") return handle_send(conn_id, request);
    if (type == "DM") return handle_dm(conn_id, request);
    if (type == "HISTORY") return handle_history(conn_id, request);
    return single(conn_id, response_for(request, false, "BAD_TYPE", "unknown command type"));
}

void ChatService::disconnect(std::uint64_t conn_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = conn_to_user_.find(conn_id);
    if (it == conn_to_user_.end()) return;
    int user_id = it->second;
    conn_to_user_.erase(it);
    auto jt = user_to_conn_.find(user_id);
    if (jt != user_to_conn_.end() && jt->second == conn_id) user_to_conn_.erase(jt);
}

KvMessage ChatService::response_for(const KvMessage& request, bool ok, const std::string& code, const std::string& message) const {
    return make_response(request.get_int("seq", 0), ok, code, message);
}

ServiceResult ChatService::single(std::uint64_t conn_id, const KvMessage& response) const {
    ServiceResult result;
    result.outbound.push_back({conn_id, response});
    return result;
}

int ChatService::authed_user_locked(std::uint64_t conn_id) const {
    auto it = conn_to_user_.find(conn_id);
    return it == conn_to_user_.end() ? 0 : it->second;
}

std::string ChatService::hash_password(const std::string& password, const std::string& salt) const {
    return std::to_string(std::hash<std::string>{}(salt + ":" + password));
}

std::string ChatService::make_token(int user_id) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream os;
    os << user_id << "-" << now << "-" << rng();
    return os.str();
}

std::string ChatService::user_name_locked(int user_id) const {
    auto it = names_by_id_.find(user_id);
    return it == names_by_id_.end() ? "unknown" : it->second;
}

ServiceResult ChatService::handle_register(std::uint64_t conn_id, const KvMessage& request) {
    if (missing(request, "name") || missing(request, "password")) {
        return single(conn_id, response_for(request, false, "BAD_REQUEST", "name and password are required"));
    }
    const std::string name = request.get("name");
    const std::string password = request.get("password");
    if (name.size() > 64 || password.size() > 128) {
        return single(conn_id, response_for(request, false, "BAD_REQUEST", "name or password too long"));
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (users_by_name_.count(name)) {
        return single(conn_id, response_for(request, false, "DUP_USER", "user already exists"));
    }
    User user;
    user.id = next_user_id_++;
    user.name = name;
    user.password_hash = hash_password(password, "study-salt-" + name);
    users_by_name_[name] = user;
    names_by_id_[user.id] = name;
    KvMessage resp = response_for(request, true, "OK", "registered");
    resp.set_int("user_id", user.id);
    return single(conn_id, resp);
}

ServiceResult ChatService::handle_login(std::uint64_t conn_id, const std::string& peer, const KvMessage& request) {
    if (missing(request, "name") || missing(request, "password")) {
        return single(conn_id, response_for(request, false, "BAD_REQUEST", "name and password are required"));
    }

    std::lock_guard<std::mutex> lock(mu_);
    int attempts = login_attempts_[peer];
    if (attempts > 20) {
        return single(conn_id, response_for(request, false, "RATE_LIMIT", "too many login attempts from this peer"));
    }

    auto it = users_by_name_.find(request.get("name"));
    if (it == users_by_name_.end()) {
        ++login_attempts_[peer];
        return single(conn_id, response_for(request, false, "AUTH_FAILED", "bad name or password"));
    }
    std::string expected = hash_password(request.get("password"), "study-salt-" + it->second.name);
    if (expected != it->second.password_hash) {
        ++login_attempts_[peer];
        return single(conn_id, response_for(request, false, "AUTH_FAILED", "bad name or password"));
    }

    login_attempts_[peer] = 0;
    std::string token = make_token(it->second.id);
    token_to_user_[token] = it->second.id;
    KvMessage resp = response_for(request, true, "OK", "login ok");
    resp.set("token", token);
    resp.set_int("user_id", it->second.id);
    return single(conn_id, resp);
}

ServiceResult ChatService::handle_auth(std::uint64_t conn_id, const KvMessage& request) {
    if (missing(request, "token")) {
        return single(conn_id, response_for(request, false, "BAD_REQUEST", "token is required"));
    }
    std::lock_guard<std::mutex> lock(mu_);
    auto it = token_to_user_.find(request.get("token"));
    if (it == token_to_user_.end()) {
        return single(conn_id, response_for(request, false, "AUTH_FAILED", "invalid token"));
    }
    conn_to_user_[conn_id] = it->second;
    user_to_conn_[it->second] = conn_id;
    KvMessage resp = response_for(request, true, "OK", "authenticated");
    resp.set_int("user_id", it->second);
    resp.set("name", user_name_locked(it->second));
    return single(conn_id, resp);
}

ServiceResult ChatService::handle_join(std::uint64_t conn_id, const KvMessage& request) {
    int room_id = request.get_int("room_id", 1);
    std::lock_guard<std::mutex> lock(mu_);
    int user_id = authed_user_locked(conn_id);
    if (!user_id) return single(conn_id, response_for(request, false, "UNAUTH", "authenticate first"));
    room_members_[room_id].insert(user_id);
    KvMessage resp = response_for(request, true, "OK", "joined room");
    resp.set_int("room_id", room_id);
    return single(conn_id, resp);
}

ServiceResult ChatService::handle_send(std::uint64_t conn_id, const KvMessage& request) {
    if (missing(request, "content")) {
        return single(conn_id, response_for(request, false, "BAD_REQUEST", "content is required"));
    }
    int room_id = request.get_int("room_id", 1);
    std::string content = request.get("content");
    if (content.size() > 4096) {
        return single(conn_id, response_for(request, false, "BAD_REQUEST", "content too long"));
    }

    std::lock_guard<std::mutex> lock(mu_);
    int user_id = authed_user_locked(conn_id);
    if (!user_id) return single(conn_id, response_for(request, false, "UNAUTH", "authenticate first"));
    room_members_[room_id].insert(user_id);

    StoredMessage stored;
    stored.id = next_message_id_++;
    stored.room_id = room_id;
    stored.from_user = user_id;
    stored.from_name = user_name_locked(user_id);
    stored.content = content;
    messages_.push_back(stored);

    ServiceResult result;
    result.outbound.push_back({conn_id, response_for(request, true, "OK", "message stored")});

    KvMessage event;
    event.set("type", "ROOM_MESSAGE");
    event.set_int("message_id", stored.id);
    event.set_int("room_id", room_id);
    event.set_int("from_user", user_id);
    event.set("from_name", stored.from_name);
    event.set("content", content);

    for (int member : room_members_[room_id]) {
        auto it = user_to_conn_.find(member);
        if (it != user_to_conn_.end()) result.outbound.push_back({it->second, event});
    }
    return result;
}

ServiceResult ChatService::handle_dm(std::uint64_t conn_id, const KvMessage& request) {
    if (missing(request, "to") || missing(request, "content")) {
        return single(conn_id, response_for(request, false, "BAD_REQUEST", "to and content are required"));
    }
    std::lock_guard<std::mutex> lock(mu_);
    int from_id = authed_user_locked(conn_id);
    if (!from_id) return single(conn_id, response_for(request, false, "UNAUTH", "authenticate first"));

    auto user_it = users_by_name_.find(request.get("to"));
    if (user_it == users_by_name_.end()) {
        return single(conn_id, response_for(request, false, "NO_SUCH_USER", "target user not found"));
    }
    int to_id = user_it->second.id;

    StoredMessage stored;
    stored.id = next_message_id_++;
    stored.from_user = from_id;
    stored.to_user = to_id;
    stored.from_name = user_name_locked(from_id);
    stored.content = request.get("content");
    messages_.push_back(stored);

    ServiceResult result;
    result.outbound.push_back({conn_id, response_for(request, true, "OK", "dm stored")});

    auto online = user_to_conn_.find(to_id);
    if (online != user_to_conn_.end()) {
        KvMessage event;
        event.set("type", "DM_MESSAGE");
        event.set_int("message_id", stored.id);
        event.set_int("from_user", from_id);
        event.set("from_name", stored.from_name);
        event.set("content", stored.content);
        result.outbound.push_back({online->second, event});
    }
    return result;
}

ServiceResult ChatService::handle_history(std::uint64_t conn_id, const KvMessage& request) {
    int room_id = request.get_int("room_id", 1);
    int limit = std::max(1, std::min(50, request.get_int("limit", 20)));

    std::lock_guard<std::mutex> lock(mu_);
    int user_id = authed_user_locked(conn_id);
    if (!user_id) return single(conn_id, response_for(request, false, "UNAUTH", "authenticate first"));

    KvMessage resp = response_for(request, true, "OK", "history");
    int count = 0;
    for (auto it = messages_.rbegin(); it != messages_.rend() && count < limit; ++it) {
        if (it->room_id != room_id) continue;
        std::string prefix = "msg" + std::to_string(count) + "_";
        resp.set_int(prefix + "id", it->id);
        resp.set(prefix + "from", it->from_name);
        resp.set(prefix + "content", it->content);
        ++count;
    }
    resp.set_int("count", count);
    return single(conn_id, resp);
}

}  // namespace chat
