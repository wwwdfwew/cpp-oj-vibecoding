#include "db/mysql_client.h"

#include <stdexcept>

#include "util/log.h"

namespace oj::db {

MySQLClient::MySQLClient(const util::Config& cfg) : cfg_(cfg) {
    mysql_library_init(0, nullptr, nullptr);
    connect();
}

MySQLClient::~MySQLClient() {
    if (conn_) mysql_close(conn_);
}

void MySQLClient::connect() {
    conn_ = mysql_init(nullptr);
    if (!conn_) throw std::runtime_error("mysql_init failed");

    unsigned int timeout = 5;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(conn_, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(conn_, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    bool reconnect = true;
    mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (!mysql_real_connect(conn_,
                            cfg_.db_host.c_str(),
                            cfg_.db_user.c_str(),
                            cfg_.db_pass.c_str(),
                            cfg_.db_name.c_str(),
                            cfg_.db_port,
                            nullptr, 0)) {
        std::string err = mysql_error(conn_);
        mysql_close(conn_);
        conn_ = nullptr;
        throw std::runtime_error("mysql_real_connect: " + err);
    }
    OJ_INFO("MySQL connected to " << cfg_.db_user << "@" << cfg_.db_host
            << ":" << cfg_.db_port << "/" << cfg_.db_name);
}

void MySQLClient::reconnect() {
    if (conn_) mysql_close(conn_);
    conn_ = nullptr;
    connect();
}

std::string MySQLClient::escape(const std::string& s) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!conn_) throw std::runtime_error("mysql not connected");
    std::string out;
    out.resize(s.size() * 2 + 1);
    unsigned long n = mysql_real_escape_string(conn_, out.data(),
                                               s.data(), s.size());
    out.resize(n);
    return out;
}

void MySQLClient::exec(const std::string& sql) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!conn_) throw std::runtime_error("mysql not connected");
    if (mysql_real_query(conn_, sql.data(), sql.size()) != 0) {
        std::string err = mysql_error(conn_);
        OJ_ERROR("mysql exec failed: " << err << " | sql=" << sql);
        throw std::runtime_error("mysql exec: " + err);
    }
    MYSQL_RES* res = mysql_store_result(conn_);
    if (res) mysql_free_result(res);
}

uint64_t MySQLClient::insert(const std::string& sql) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!conn_) throw std::runtime_error("mysql not connected");
    if (mysql_real_query(conn_, sql.data(), sql.size()) != 0) {
        std::string err = mysql_error(conn_);
        OJ_ERROR("mysql insert failed: " << err << " | sql=" << sql);
        throw std::runtime_error("mysql insert: " + err);
    }
    return mysql_insert_id(conn_);
}

std::vector<std::vector<std::string>> MySQLClient::query(const std::string& sql) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!conn_) throw std::runtime_error("mysql not connected");
    std::vector<std::vector<std::string>> rows;
    if (mysql_real_query(conn_, sql.data(), sql.size()) != 0) {
        std::string err = mysql_error(conn_);
        OJ_ERROR("mysql query failed: " << err << " | sql=" << sql);
        throw std::runtime_error("mysql query: " + err);
    }
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) {
        if (mysql_field_count(conn_) == 0) return rows;
        throw std::runtime_error(std::string("mysql_store_result: ") + mysql_error(conn_));
    }
    unsigned int cols = mysql_num_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        unsigned long* lens = mysql_fetch_lengths(res);
        std::vector<std::string> r;
        r.reserve(cols);
        for (unsigned int i = 0; i < cols; ++i) {
            if (row[i]) r.emplace_back(row[i], lens[i]);
            else r.emplace_back();
        }
        rows.push_back(std::move(r));
    }
    mysql_free_result(res);
    return rows;
}

}  // namespace oj::db
