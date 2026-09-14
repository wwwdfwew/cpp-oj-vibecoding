#ifndef CPP_OJ_VIBECODING_ROUTES_STATIC_FILES_H
#define CPP_OJ_VIBECODING_ROUTES_STATIC_FILES_H

#include <httplib.h>

#include <string>

namespace oj::routes {

// 在 `web_root` 下挂载静态文件服务,并对未知路径回退到 index.html
// (以保证 pushState 路由正常工作)。
void register_static_routes(httplib::Server& srv, const std::string& web_root);

}  // namespace oj::routes

#endif
