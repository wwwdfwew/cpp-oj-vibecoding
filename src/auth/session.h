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

    // 为指定 user_id 创建新会话,返回 token(64 位十六进制字符串)。
    std::string create(int user_id, int ttl_seconds = 7 * 24 * 3600);

    // 按 token 查询会话;不存在或已过期时返回 nullopt。
    std::optional<Session> lookup(const std::string& token);

    // 按 token 删除会话。
    void destroy(const std::string& token);

    // 尽力而为地清理已过期的会话记录。
    void purge_expired();

    static std::string random_token();

private:
    db::MySQLClient& db_;
};

}  // namespace oj::auth

#endif
