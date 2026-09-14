#ifndef CPP_OJ_VIBECODING_ROUTES_ADMIN_H
#define CPP_OJ_VIBECODING_ROUTES_ADMIN_H

#include <httplib.h>

#include "auth/session.h"
#include "db/mysql_client.h"

namespace oj::routes {

void register_admin_routes(httplib::Server& srv,
                           db::MySQLClient& db,
                           auth::SessionManager& sm);

}  // namespace oj::routes

#endif
