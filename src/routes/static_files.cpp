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

    // 1) Serve files under /static/* from web_root/static (preferred)
    fs::path static_dir = root / "static";
    if (fs::exists(static_dir) && fs::is_directory(static_dir)) {
        srv.set_mount_point("/static", static_dir.string());
    }

    // 2) Serve index.html at root and as SPA fallback
    fs::path index_html = root / "index.html";

    // Capture by VALUE so lambdas outliving this function don't dangle.
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

    // Catch-all for SPA routes: anything not /api/*, fall back to index.html.
    // Use ".+" so that "/" is handled by the explicit route above.
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
