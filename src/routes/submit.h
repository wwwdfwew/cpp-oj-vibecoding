#ifndef CPP_OJ_VIBECODING_ROUTES_SUBMIT_H
#define CPP_OJ_VIBECODING_ROUTES_SUBMIT_H

#include <httplib.h>

#include "auth/session.h"
#include "db/mysql_client.h"
#include "judge/judge.h"

namespace oj::routes {

void register_submit_routes(httplib::Server& srv,
                            db::MySQLClient& db,
                            auth::SessionManager& sm,
                            judge::Judge& judge);

}  // namespace oj::routes

#endif
