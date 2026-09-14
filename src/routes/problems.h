#ifndef CPP_OJ_VIBECODING_ROUTES_PROBLEMS_H
#define CPP_OJ_VIBECODING_ROUTES_PROBLEMS_H

#include <httplib.h>

#include "db/mysql_client.h"

namespace oj::routes {

void register_problem_routes(httplib::Server& srv, db::MySQLClient& db);

}  // namespace oj::routes

#endif
