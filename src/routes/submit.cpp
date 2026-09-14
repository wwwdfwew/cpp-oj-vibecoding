#include "routes/submit.h"

#include <cctype>

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

}  // namespace

void register_submit_routes(httplib::Server& srv,
                            db::MySQLClient& db,
                            auth::SessionManager& sm,
                            judge::Judge& judge) {
  (void)db;  // Judge 内部持有自己的 db 引用,此处保留 db 参数

  srv.Post("/api/submit", [&sm, &judge](const httplib::Request& req,
                                       httplib::Response& res) {
    // 必须登录才能提交
    std::string tok = extract_cookie(req, "SESSION");
    auto sess = tok.empty() ? std::nullopt : sm.lookup(tok);
    if (!sess) {
      send_json(res, 401,
          JsonValue(JsonObject{{"error", JsonValue("请先登录后再提交代码")}}));
      return;
    }

    // 限制请求体大小:64 KiB 代码 + JSON 包装开销
    if (req.body.size() > 128 * 1024) {
      send_json(res, 413,
          JsonValue(JsonObject{{"error", JsonValue("代码过长(超过 64KB)")}}));
      return;
    }

    auto body = util::parse_json(req.body);
    if (body.type() != JsonValue::Type::Object || !body.obj()) {
      send_json(res, 400,
          JsonValue(JsonObject{{"error", JsonValue("请求格式错误")}}));
      return;
    }
    const auto& obj = *body.obj();
    auto itp = obj.find("problem_id");
    auto itc = obj.find("code");
    if (itp == obj.end() || itc == obj.end() ||
        itp->second.type() != JsonValue::Type::Int ||
        itc->second.type() != JsonValue::Type::String) {
      send_json(res, 400,
          JsonValue(JsonObject{{"error", JsonValue("缺少 problem_id 或 code")}}));
      return;
    }

    int problem_id = static_cast<int>(itp->second.int_value());
    const std::string& code = itc->second.str_value();
    if (code.empty()) {
      send_json(res, 400,
          JsonValue(JsonObject{{"error", JsonValue("代码为空")}}));
      return;
    }

    try {
      auto jr = judge.run(problem_id, code);
      JsonObject out{
        {"status", JsonValue(jr.status == judge::JudgeStatus::AC
                              ? "AC" : "WA")},
        {"message", JsonValue(jr.message)},
      };
      if (!jr.compile_error.empty()) {
        out["compile_error"] = JsonValue(jr.compile_error);
      }
      if (jr.failed_case >= 0) {
        out["failed_case"] = JsonValue(jr.failed_case);
      }
      send_json(res, 200, JsonValue(std::move(out)));
    } catch (const std::exception& e) {
      OJ_ERROR("POST /api/submit (user=" << sess->username << "): " << e.what());
      send_json(res, 500,
          JsonValue(JsonObject{{"error", JsonValue(e.what())}}));
    }
  });
}

}  // namespace oj::routes
