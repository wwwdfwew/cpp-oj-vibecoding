#ifndef CPP_OJ_VIBECODING_DB_MYSQL_CLIENT_H
#define CPP_OJ_VIBECODING_DB_MYSQL_CLIENT_H

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <mysql/mysql.h>

#include "util/config.h"

namespace oj::db {

class MySQLClient {
public:
    explicit MySQLClient(const util::Config& cfg);
    ~MySQLClient();

    MySQLClient(const MySQLClient&) = delete;
    MySQLClient& operator=(const MySQLClient&) = delete;

    // Execute a write statement. Throws std::runtime_error on error.
    void exec(const std::string& sql);

    // Execute a SELECT; rows[row][col] as std::string.
    std::vector<std::vector<std::string>> query(const std::string& sql);

    // Insert and return auto-increment id (for INSERT ... statements).
    uint64_t insert(const std::string& sql);

    // Escape a string for safe inlining.
    std::string escape(const std::string& s);

    const util::Config& config() const { return cfg_; }

private:
    util::Config cfg_;
    MYSQL* conn_ = nullptr;
    std::mutex mu_;

    void connect();
    void reconnect();
};

}  // namespace oj::db

#endif
