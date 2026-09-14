#include "routes/auth.h"

#include <cctype>
#include <functional>

#include "auth/password.h"
#include "util/json.h"
#include "util/log.h"
#include "util/strutil.h"

namespace oj::routes {

namespace {

using util::JsonValue;
using util::JsonObject;
using util::parse_json;

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) return false;
    }
    return true;
}

std::string extract_cookie(const httplib::Request& req, const std::string& name) {
    std::string prefix = name + "=";
    for (const auto& kv : req.headers) {
        if (!iequals(kv.first, "Cookie")) continue;
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
    return "";
}

void send_json(httplib::Response& res, int code, const JsonValue& v) {
    res.status = code;
    res.set_content(v.dump(), "application/json");
}

using LoginFn = std::function<void(const httplib::Request&, httplib::Response&, bool)>;

}  // namespace

void register_auth_routes(httplib::Server& srv,
                          db::MySQLClient& db,
                          auth::SessionManager& sm,
                          const std::string& bind_host) {
    (void)bind_host;

    LoginFn handle_login = [&db, &sm](const httplib::Request& req,
                                      httplib::Response& res,
                                      bool admin_only) {
        auto body = parse_json(req.body);
        if (body.type() != JsonValue::Type::Object || !body.obj()) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("invalid json")}}));
            return;
        }
        const auto& obj = *body.obj();
        auto itu = obj.find("username");
        auto itp = obj.find("password");
        if (itu == obj.end() || itp == obj.end() ||
            itu->second.type() != JsonValue::Type::String ||
            itp->second.type() != JsonValue::Type::String) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("username/password required")}}));
            return;
        }
        std::string username = itu->second.str_value();
        std::string password = itp->second.str_value();

        std::string esc_user = db.escape(username);
        auto rows = db.query(
            "SELECT id, password_hash, salt, role FROM users WHERE username='" +
            esc_user + "' LIMIT 1");
        if (rows.empty()) {
            send_json(res, 401,
                JsonValue(JsonObject{{"error", JsonValue("invalid credentials")}}));
            return;
        }
        int user_id = std::stoi(rows[0][0]);
        std::string hash = rows[0][1];
        std::string salt = rows[0][2];
        std::string role = rows[0][3];

        if (admin_only && role != "admin") {
            send_json(res, 401,
                JsonValue(JsonObject{{"error", JsonValue("admin only")}}));
            return;
        }

        if (!auth::verify_password(salt, password, hash)) {
            send_json(res, 401,
                JsonValue(JsonObject{{"error", JsonValue("invalid credentials")}}));
            return;
        }

        std::string old = extract_cookie(req, "SESSION");
        if (!old.empty()) sm.destroy(old);

        std::string token = sm.create(user_id);
        res.set_header("Set-Cookie",
            "SESSION=" + token + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800");
        send_json(res, 200,
            JsonValue(JsonObject{{"ok", JsonValue(true)},
                                  {"role", JsonValue(role)}}));
    };

    srv.Post("/api/login", [handle_login](const httplib::Request& req,
                                          httplib::Response& res) {
        handle_login(req, res, /*admin_only=*/false);
    });

    srv.Post("/api/admin/login", [handle_login](const httplib::Request& req,
                                                httplib::Response& res) {
        handle_login(req, res, /*admin_only=*/true);
    });

    srv.Post("/api/logout", [&sm](const httplib::Request& req,
                                  httplib::Response& res) {
        std::string tok = extract_cookie(req, "SESSION");
        if (!tok.empty()) sm.destroy(tok);
        res.set_header("Set-Cookie",
            "SESSION=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
        send_json(res, 200, JsonValue(JsonObject{{"ok", JsonValue(true)}}));
    });

    // SPEC: 普通用户注册(MVP 后续开放)
    srv.Post("/api/register", [&db, &sm](const httplib::Request& req,
                                         httplib::Response& res) {
        auto body = parse_json(req.body);
        if (body.type() != JsonValue::Type::Object || !body.obj()) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("invalid json")}}));
            return;
        }
        const auto& obj = *body.obj();
        auto itu = obj.find("username");
        auto itp = obj.find("password");
        if (itu == obj.end() || itp == obj.end() ||
            itu->second.type() != JsonValue::Type::String ||
            itp->second.type() != JsonValue::Type::String) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("用户名和密码必填")}}));
            return;
        }
        std::string username = itu->second.str_value();
        std::string password = itp->second.str_value();
        if (username.size() < 3 || username.size() > 32) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("用户名长度需在 3-32 个字符之间")}}));
            return;
        }
        if (password.size() < 6 || password.size() > 128) {
            send_json(res, 400,
                JsonValue(JsonObject{{"error", JsonValue("密码长度需在 6-128 个字符之间")}}));
            return;
        }
        for (char c : username) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) {
                send_json(res, 400,
                    JsonValue(JsonObject{{"error", JsonValue("用户名仅允许字母、数字、下划线和连字符")}}));
                return;
            }
        }

        std::string esc_user = db.escape(username);
        auto exist = db.query(
            "SELECT id FROM users WHERE username='" + esc_user + "' LIMIT 1");
        if (!exist.empty()) {
            send_json(res, 409,
                JsonValue(JsonObject{{"error", JsonValue("该用户名已被占用")}}));
            return;
        }

        std::string salt = auth::random_salt_hex();
        std::string hash = auth::hash_password(salt, password);
        std::string esc_salt = db.escape(salt);
        std::string esc_hash = db.escape(hash);
        try {
            db.exec(
                "INSERT INTO users (username, password_hash, salt, role) "
                "VALUES ('" + esc_user + "', '" + esc_hash + "', '" +
                esc_salt + "', 'user')");
        } catch (const std::exception& e) {
            OJ_ERROR("register insert failed: " << e.what());
            send_json(res, 500,
                JsonValue(JsonObject{{"error", JsonValue("注册失败，请稍后再试")}}));
            return;
        }

        auto rows = db.query(
            "SELECT id FROM users WHERE username='" + esc_user + "' LIMIT 1");
        if (rows.empty()) {
            send_json(res, 500,
                JsonValue(JsonObject{{"error", JsonValue("注册失败，请稍后再试")}}));
            return;
        }
        int user_id = std::stoi(rows[0][0]);

        std::string old = extract_cookie(req, "SESSION");
        if (!old.empty()) sm.destroy(old);
        std::string token = sm.create(user_id);
        res.set_header("Set-Cookie",
            "SESSION=" + token + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800");
        send_json(res, 200,
            JsonValue(JsonObject{{"ok", JsonValue(true)},
                                  {"username", JsonValue(username)},
                                  {"role", JsonValue("user")}}));
    });

    // GET /api/me — 查询当前会话(用户名 + 角色),未登录返回 401
    srv.Get("/api/me", [&sm](const httplib::Request& req,
                             httplib::Response& res) {
        std::string tok = extract_cookie(req, "SESSION");
        auto s = tok.empty() ? std::nullopt : sm.lookup(tok);
        if (!s) {
            send_json(res, 401,
                JsonValue(JsonObject{{"error", JsonValue("未登录")}}));
            return;
        }
        send_json(res, 200,
            JsonValue(JsonObject{{"ok", JsonValue(true)},
                                  {"username", JsonValue(s->username)},
                                  {"role", JsonValue(s->role)}}));
    });
}

}  // namespace oj::routes
