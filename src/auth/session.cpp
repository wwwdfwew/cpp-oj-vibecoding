#include "auth/session.h"

#include "db/mysql_client.h"

#include <openssl/rand.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace oj::auth {

namespace {

std::string mysql_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\0': out += "\\0";  break;
            default:   out += c;
        }
    }
    return out;
}

int64_t now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

std::string SessionManager::random_token() {
    std::vector<unsigned char> buf(32);
    if (RAND_bytes(buf.data(), 32) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    std::ostringstream oss;
    for (auto b : buf) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    }
    return oss.str();
}

SessionManager::SessionManager(db::MySQLClient& db) : db_(db) {}

std::string SessionManager::create(int user_id, int ttl_seconds) {
    std::string token = random_token();
    int64_t exp = now_unix() + ttl_seconds;

    std::ostringstream sql;
    sql << "INSERT INTO sessions (token, user_id, expires_at) VALUES ('"
        << mysql_escape(token) << "', " << user_id << ", FROM_UNIXTIME(" << exp << "))";
    db_.exec(sql.str());
    return token;
}

std::optional<Session> SessionManager::lookup(const std::string& token) {
    if (token.empty()) return std::nullopt;
    std::ostringstream sql;
    sql << "SELECT s.user_id, u.username, u.role, UNIX_TIMESTAMP(s.expires_at) "
        << "FROM sessions s JOIN users u ON u.id = s.user_id "
        << "WHERE s.token = '" << mysql_escape(token) << "' LIMIT 1";
    auto rows = db_.query(sql.str());
    if (rows.empty()) return std::nullopt;
    int64_t exp = std::stoll(rows[0][3]);
    if (exp <= now_unix()) return std::nullopt;
    Session s;
    s.token = token;
    s.user_id = std::stoi(rows[0][0]);
    s.username = rows[0][1];
    s.role = rows[0][2];
    s.expires_at_unix = exp;
    return s;
}

void SessionManager::destroy(const std::string& token) {
    if (token.empty()) return;
    std::ostringstream sql;
    sql << "DELETE FROM sessions WHERE token = '" << mysql_escape(token) << "'";
    db_.exec(sql.str());
}

void SessionManager::purge_expired() {
    db_.exec("DELETE FROM sessions WHERE expires_at < NOW()");
}

}  // namespace oj::auth
