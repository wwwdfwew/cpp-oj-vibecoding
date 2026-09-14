#ifndef CPP_OJ_VIBECODING_ROUTES_STATIC_FILES_H
#define CPP_OJ_VIBECODING_ROUTES_STATIC_FILES_H

#include <httplib.h>

#include <string>

namespace oj::routes {

// Mount static file server for `web_root` and a SPA fallback that serves
// index.html for unknown paths (so pushState routing works).
void register_static_routes(httplib::Server& srv, const std::string& web_root);

}  // namespace oj::routes

#endif
