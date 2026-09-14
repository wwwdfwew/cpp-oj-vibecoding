#ifndef CPP_OJ_VIBECODING_UTIL_CONFIG_H
#define CPP_OJ_VIBECODING_UTIL_CONFIG_H

#include <cstdint>
#include <string>

namespace oj::util {

struct Config {
    std::string http_host      = "0.0.0.0";
    uint16_t    http_port      = 8088;

    std::string db_host        = "localhost";
    uint16_t    db_port        = 3306;
    std::string db_user        = "root";
    std::string db_pass;
    std::string db_name        = "oj";

    std::string web_root       = "./web";

    std::string gxx_path       = "/usr/bin/g++";

    std::string bind_addr() const;

    static Config from_env();
};

}  // namespace oj::util

#endif
