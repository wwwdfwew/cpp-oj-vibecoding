#include "routes/problems.h"

#include "util/json.h"
#include "util/log.h"

namespace oj::routes {

namespace {

using util::JsonValue;

void send_json(httplib::Response& res, int code, const JsonValue& v) {
    res.status = code;
    res.set_content(v.dump(), "application/json");
}

}  // namespace

void register_problem_routes(httplib::Server& srv, db::MySQLClient& db) {
    srv.Get("/api/problems", [&](const httplib::Request&,
                                 httplib::Response& res) {
        try {
            auto rows = db.query(
                "SELECT id, title, time_limit_ms, memory_limit_mb "
                "FROM problems ORDER BY id ASC");
            util::JsonArray arr;
            for (auto& r : rows) {
                util::JsonObject obj{
                    {"id",              JsonValue(std::stoi(r[0]))},
                    {"title",           JsonValue(r[1])},
                    {"time_limit_ms",   JsonValue(std::stoi(r[2]))},
                    {"memory_limit_mb", JsonValue(std::stoi(r[3]))},
                };
                arr.push_back(JsonValue(std::move(obj)));
            }
            send_json(res, 200, JsonValue(std::move(arr)));
        } catch (const std::exception& e) {
            OJ_ERROR("GET /api/problems: " << e.what());
            send_json(res, 500,
                JsonValue(util::JsonObject{{"error", JsonValue(e.what())}}));
        }
    });

    srv.Get(R"(/api/problems/(\d+))",
            [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1].str());
        try {
            auto prows = db.query(
                "SELECT id, title, description, input_format, output_format, "
                "time_limit_ms, memory_limit_mb "
                "FROM problems WHERE id=" + std::to_string(id));
            if (prows.empty()) {
                send_json(res, 404,
                    JsonValue(util::JsonObject{{"error", JsonValue("not found")}}));
                return;
            }
            // 仅把前若干个测试用例作为"样例"暴露给用户。
            // 判题时仍会运行所有用例 —— 这样做的目的是既不泄露隐藏测试,
            // 又能在用例输入很大时让响应体保持较小。
            auto crows = db.query(
                "SELECT input, expected_output FROM test_cases "
                "WHERE problem_id=" + std::to_string(id) +
                " ORDER BY ord ASC, id ASC LIMIT 2");

            util::JsonArray samples;
            for (auto& r : crows) {
                samples.push_back(JsonValue(util::JsonObject{
                    {"input",           JsonValue(r[0])},
                    {"expected_output", JsonValue(r[1])},
                }));
            }

            util::JsonObject obj{
                {"id",              JsonValue(std::stoi(prows[0][0]))},
                {"title",           JsonValue(prows[0][1])},
                {"description",     JsonValue(prows[0][2])},
                {"input_format",    JsonValue(prows[0][3])},
                {"output_format",   JsonValue(prows[0][4])},
                {"time_limit_ms",   JsonValue(std::stoi(prows[0][5]))},
                {"memory_limit_mb", JsonValue(std::stoi(prows[0][6]))},
                {"samples",         JsonValue(std::move(samples))},
            };
            send_json(res, 200, JsonValue(std::move(obj)));
        } catch (const std::exception& e) {
            OJ_ERROR("GET /api/problems/" << id << ": " << e.what());
            send_json(res, 500,
                JsonValue(util::JsonObject{{"error", JsonValue(e.what())}}));
        }
    });
}

}  // namespace oj::routes
