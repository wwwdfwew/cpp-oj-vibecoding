#ifndef CPP_OJ_VIBECODING_ROUTES_AUTH_H
#define CPP_OJ_VIBECODING_ROUTES_AUTH_H

#include <httplib.h>

#include "auth/session.h"
#include "db/mysql_client.h"

namespace oj::routes {

void register_auth_routes(httplib::Server& srv,
                          db::MySQLClient& db,
                          auth::SessionManager& sm,
                          const std::string& bind_host);

}  // namespace oj::routes

#endif
