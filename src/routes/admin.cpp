#include "routes/admin.h"

#include <cctype>
#include <functional>

#include "util/json.h"
#include "util/log.h"

namespace oj::routes {

namespace {

using util::JsonValue;
using util::JsonObject;

void send_json(httplib::Response& res, int code, const JsonValue& v) {
    res.status = code;
    res.set_content(v.dump(), "application/json");
}

std::string extract_cookie(const httplib::Request& req, const std::string& name) {
    std::string prefix = name + "=";
    for (const auto& kv : req.headers) {
        std::string hk = kv.first;
        for (auto& c : hk) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (hk == "cookie") {
            size_t pos = 0;
            while (pos < kv.second.size()) {
                size_t end = kv.second.find(';', pos);
                if (end == std::string::npos) end = kv.second.size();
                std::string piece = kv.second.substr(pos, end - pos);
                size_t sp = piece.find_first_not_of(' ');
                if (sp != std::string::npos) piece = piece.substr(sp);
                if (piece.size() >= prefix.size() &&
                    piece.compare(0, prefix.size(), prefix) == 0) {
                    return piece.substr(prefix.size());
                }
                pos = end + 1;
            }
        }
    }
    return "";
}

enum class AdminGuardResult { Ok, NoSession, SessionExpired, NotAdmin };

AdminGuardResult check_admin(db::MySQLClient& db,
                            auth::SessionManager& sm,
                            const httplib::Request& req,
                            httplib::Response& res) {
    std::string tok = extract_cookie(req, "SESSION");
    if (tok.empty()) {
        send_json(res, 401,
            JsonValue(JsonObject{{"error", JsonValue("unauthenticated")}}));
        return AdminGuardResult::NoSession;
    }
    auto sess = sm.lookup(tok);
    if (!sess) {
        send_json(res, 401,
            JsonValue(JsonObject{{"error", JsonValue("session expired")}}));
        return AdminGuardResult::SessionExpired;
    }
    if (sess->role != "admin") {
        send_json(res, 403,
            JsonValue(JsonObject{{"error", JsonValue("admin only")}}));
        return AdminGuardResult::NotAdmin;
    }
    (void)db;
    return AdminGuardResult::Ok;
}

}  // namespace

void register_admin_routes(httplib::Server& srv,
                           db::MySQLClient& db,
                           auth::SessionManager& sm) {

    using AdminGuard = std::function<bool(const httplib::Request&, httplib::Response&)>;
    AdminGuard require_admin = [&db, &sm](const httplib::Request& req,
                                          httplib::Response& res) -> bool {
        return check_admin(db, sm, req, res) == AdminGuardResult::Ok;
    };

    srv.Post("/api/admin/problems",
             [require_admin, &db](const httplib::Request& req,
                                  httplib::Response& res) {
        if (!require_admin(req, res)) return;
        auto body = util::parse_json(req.body);
        if (body.type() != JsonValue::Type::Object || !body.obj()) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("invalid json")}}));
            return;
        }
        const auto& obj = *body.obj();

        auto get_str = [&](const char* k) -> std::string {
            auto it = obj.find(k);
            if (it == obj.end() || it->second.type() != JsonValue::Type::String)
                return "";
            return it->second.str_value();
        };
        auto get_int = [&](const char* k, int def) -> int {
            auto it = obj.find(k);
            if (it == obj.end()) return def;
            if (it->second.type() == JsonValue::Type::Int) return static_cast<int>(it->second.int_value());
            if (it->second.type() == JsonValue::Type::String) {
                try { return std::stoi(it->second.str_value()); } catch (...) { return def; }
            }
            return def;
        };

        std::string title = get_str("title");
        std::string desc  = get_str("description");
        std::string in_f  = get_str("input_format");
        std::string out_f = get_str("output_format");
        int tl = get_int("time_limit_ms", 1000);
        int ml = get_int("memory_limit_mb", 128);

        if (title.empty() || desc.empty()) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("title/description required")}}));
            return;
        }
        if (tl < 100 || tl > 10000) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("time_limit_ms must be in [100, 10000]")}}));
            return;
        }
        if (ml < 16 || ml > 1024) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("memory_limit_mb must be in [16, 1024]")}}));
            return;
        }

        auto it = obj.find("test_cases");
        if (it == obj.end() || it->second.type() != JsonValue::Type::Array ||
            !it->second.arr() || it->second.arr()->empty()) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("at least one test case")}}));
            return;
        }

        try {
            std::ostringstream sql;
            sql << "INSERT INTO problems (title, description, input_format, "
                << "output_format, time_limit_ms, memory_limit_mb) VALUES ('"
                << db.escape(title) << "','"
                << db.escape(desc)  << "','"
                << db.escape(in_f)  << "','"
                << db.escape(out_f) << "',"
                << tl << "," << ml << ")";
            uint64_t pid = db.insert(sql.str());

            int ord = 0;
            for (const auto& v : *it->second.arr()) {
                if (v.type() != JsonValue::Type::Object) continue;
                const auto& vo = *v.obj();
                std::string in_s  = vo.at("input").str_value();
                std::string out_s = vo.at("expected_output").str_value();
                std::ostringstream tc;
                tc << "INSERT INTO test_cases (problem_id, input, expected_output, ord) VALUES ("
                   << pid << ",'" << db.escape(in_s) << "','"
                   << db.escape(out_s) << "'," << ord << ")";
                db.exec(tc.str());
                ++ord;
            }

            send_json(res, 200,
                JsonValue(JsonObject{{"ok", JsonValue(true)},
                                      {"id", JsonValue(static_cast<long long>(pid))}}));
        } catch (const std::exception& e) {
            OJ_ERROR("POST /api/admin/problems: " << e.what());
            send_json(res, 500,
                JsonValue(JsonObject{{"error", JsonValue(e.what())}}));
        }
    });

    srv.Delete(R"(/api/admin/problems/(\d+))",
               [require_admin, &db](const httplib::Request& req,
                                    httplib::Response& res) {
        if (!require_admin(req, res)) return;
        int id = std::stoi(req.matches[1].str());
        try {
            auto rows = db.query("SELECT id FROM problems WHERE id=" + std::to_string(id));
            if (rows.empty()) {
                send_json(res, 404,
                    JsonValue(JsonObject{{"error", JsonValue("not found")}}));
                return;
            }
            db.exec("DELETE FROM problems WHERE id=" + std::to_string(id));
            send_json(res, 200,
                JsonValue(JsonObject{{"ok", JsonValue(true)}}));
        } catch (const std::exception& e) {
            OJ_ERROR("DELETE /api/admin/problems/" << id << ": " << e.what());
            send_json(res, 500,
                JsonValue(JsonObject{{"error", JsonValue(e.what())}}));
        }
    });
}

}  // namespace oj::routes
