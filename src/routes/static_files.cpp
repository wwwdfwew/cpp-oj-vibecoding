#include "routes/static_files.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "util/log.h"

namespace oj::routes {

namespace fs = std::filesystem;

namespace {

std::string read_text_file(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::string mime_for(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".mjs")  return "application/javascript; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".txt")  return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

}  // namespace

void register_static_routes(httplib::Server& srv, const std::string& web_root) {
    fs::path root(web_root);
    if (!fs::exists(root)) {
        OJ_WARN("web_root does not exist: " << web_root);
    }

    // 1) 将 /static/* 下的文件从 web_root/static 提供出去(优先)
    fs::path static_dir = root / "static";
    if (fs::exists(static_dir) && fs::is_directory(static_dir)) {
        srv.set_mount_point("/static", static_dir.string());
    }

    // 2) 在根路径以及 SPA 回退场景下提供 index.html
    fs::path index_html = root / "index.html";

    // 按值捕获,确保本函数返回后 lambda 不会悬空。
    auto serve_index = [index_html](httplib::Response& res) {
        if (fs::exists(index_html)) {
            res.set_content(read_text_file(index_html), mime_for(index_html));
        } else {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
        }
    };

    srv.Get("/", [serve_index](const httplib::Request&,
                               httplib::Response& res) {
        serve_index(res);
    });

    // SPA 路由的兜底处理:非 /api/* 的请求全部回退到 index.html。
    // 使用 ".+" 是为了让 "/" 仍然交给上方的显式路由处理。
    srv.Get(".+", [root, serve_index](const httplib::Request& req,
                                      httplib::Response& res) {
        const std::string& p = req.path;
        if (p.rfind("/api/", 0) == 0) {
            res.status = 404;
            res.set_content("{\"error\":\"no route\"}", "application/json");
            return;
        }
        fs::path candidate = root / p.substr(1);
        std::error_code ec;
        auto canonical_root = fs::weakly_canonical(root, ec);
        if (!ec && fs::exists(candidate) && fs::is_regular_file(candidate)) {
            auto canon_file = fs::weakly_canonical(candidate, ec);
            if (!ec) {
                std::string cs = canon_file.string();
                std::string cr = canonical_root.string();
                if (cs.rfind(cr, 0) == 0) {
                    res.set_content(read_text_file(canon_file),
                                     mime_for(canon_file));
                    return;
                }
            }
        }
        serve_index(res);
    });
}

}  // namespace oj::routes
