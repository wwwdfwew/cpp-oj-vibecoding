#include "util/config.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace oj::util {

namespace {

std::string env_or(const char* key, const std::string& fallback) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : fallback;
}

uint16_t env_int(const char* key, uint16_t fallback) {
    const char* v = std::getenv(key);
    if (!v || !*v) return fallback;
    try {
        int n = std::stoi(v);
        if (n <= 0 || n > 65535) return fallback;
        return static_cast<uint16_t>(n);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

std::string Config::bind_addr() const {
    std::ostringstream oss;
    oss << http_host << ':' << http_port;
    return oss.str();
}

Config Config::from_env() {
    Config c;
    c.http_host = env_or("OJ_HTTP_HOST", c.http_host);
    c.http_port = env_int("OJ_HTTP_PORT", c.http_port);

    c.db_host = env_or("OJ_DB_HOST", c.db_host);
    c.db_port = env_int("OJ_DB_PORT", c.db_port);
    c.db_user = env_or("OJ_DB_USER", c.db_user);
    c.db_pass = env_or("OJ_DB_PASS", c.db_pass);
    c.db_name = env_or("OJ_DB_NAME", c.db_name);

    c.web_root = env_or("OJ_WEB_ROOT", c.web_root);
    c.gxx_path = env_or("OJ_GXX_PATH", c.gxx_path);
    return c;
}

}  // namespace oj::util
