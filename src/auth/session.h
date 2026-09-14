#ifndef CPP_OJ_VIBECODING_AUTH_SESSION_H
#define CPP_OJ_VIBECODING_AUTH_SESSION_H

#include <cstdint>
#include <optional>
#include <string>

namespace oj::db { class MySQLClient; }

namespace oj::auth {

struct Session {
    std::string token;
    int         user_id = 0;
    std::string username;
    std::string role;  // "user" | "admin"
    int64_t     expires_at_unix = 0;
};

class SessionManager {
public:
    explicit SessionManager(db::MySQLClient& db);

    // Generate a new session for user_id; returns the token (64-char hex).
    std::string create(int user_id, int ttl_seconds = 7 * 24 * 3600);

    // Look up a session by token; returns nullopt if missing or expired.
    std::optional<Session> lookup(const std::string& token);

    // Delete a session by token.
    void destroy(const std::string& token);

    // Best-effort cleanup of expired rows.
    void purge_expired();

    static std::string random_token();

private:
    db::MySQLClient& db_;
};

}  // namespace oj::auth

#endif
