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

    // 执行写语句;出错时抛出 std::runtime_error。
    void exec(const std::string& sql);

    // 执行 SELECT 查询;rows[row][col] 以 std::string 形式返回。
    std::vector<std::vector<std::string>> query(const std::string& sql);

    // 执行 INSERT 并返回自增主键(用于 INSERT ... 语句)。
    uint64_t insert(const std::string& sql);

    // 对字符串进行转义,用于安全地拼接到 SQL 中。
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
