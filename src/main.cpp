// cpp-oj-vibecoding — main entry
// SPEC §2.1 / §3 Phase 2.5
#include <httplib.h>

#include <csignal>
#include <iostream>
#include <memory>

#include "auth/session.h"
#include "db/mysql_client.h"
#include "judge/judge.h"
#include "routes/admin.h"
#include "routes/auth.h"
#include "routes/problems.h"
#include "routes/static_files.h"
#include "routes/submit.h"
#include "util/config.h"
#include "util/log.h"

namespace {
httplib::Server* g_server = nullptr;

void on_signal(int sig) {
    OJ_INFO("received signal " << sig << ", shutting down");
    if (g_server) g_server->stop();
}
}

int main(int argc, char** argv) {
    using namespace oj;
    auto cfg = util::Config::from_env();
    if (const char* lvl = std::getenv("OJ_LOG")) {
        std::string s(lvl);
        if      (s == "DEBUG") util::log_set_level(util::LogLevel::Debug);
        else if (s == "INFO")  util::log_set_level(util::LogLevel::Info);
        else if (s == "WARN")  util::log_set_level(util::LogLevel::Warn);
        else if (s == "ERROR") util::log_set_level(util::LogLevel::Error);
    } else {
        util::log_set_level(util::LogLevel::Info);
    }

    OJ_INFO("cpp-oj-vibecoding starting, bind=" << cfg.bind_addr()
            << ", web_root=" << cfg.web_root);

    std::unique_ptr<db::MySQLClient> db_ptr;
    try {
        db_ptr = std::make_unique<db::MySQLClient>(cfg);
    } catch (const std::exception& e) {
        OJ_ERROR("DB init failed: " << e.what());
        return 1;
    }
    auto& db = *db_ptr;

    auth::SessionManager sm(db);
    try {
        sm.purge_expired();
    } catch (const std::exception& e) {
        OJ_WARN("purge_expired failed (non-fatal): " << e.what());
    }

    judge::Judge judge(db, cfg.gxx_path);

    httplib::Server srv;
    g_server = &srv;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    routes::register_auth_routes(srv, db, sm, cfg.http_host);
    routes::register_problem_routes(srv, db);
    routes::register_submit_routes(srv, db, sm, judge);
    routes::register_admin_routes(srv, db, sm);
    routes::register_static_routes(srv, cfg.web_root);

    srv.set_keep_alive_max_count(20);
    srv.set_read_timeout(30, 0);
    srv.set_write_timeout(30, 0);

    if (!srv.listen(cfg.http_host.c_str(), cfg.http_port)) {
        OJ_ERROR("listen failed on " << cfg.bind_addr());
        return 1;
    }
    OJ_INFO("server stopped");
    return 0;
}
