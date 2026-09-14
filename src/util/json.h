#ifndef CPP_OJ_VIBECODING_UTIL_JSON_H
#define CPP_OJ_VIBECODING_UTIL_JSON_H

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace oj::util {

class JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

class JsonValue {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    JsonValue() : type_(Type::Null) {}
    JsonValue(std::nullptr_t) : type_(Type::Null) {}
    JsonValue(bool b) : type_(Type::Bool), bool_(b) {}
    JsonValue(int i) : type_(Type::Int), int_(i) {}
    JsonValue(long long i) : type_(Type::Int), int_(i) {}
    JsonValue(double d) : type_(Type::Double), double_(d) {}
    JsonValue(const char* s) : type_(Type::String), str_(s) {}
    JsonValue(std::string s) : type_(Type::String), str_(std::move(s)) {}
    JsonValue(JsonArray a) : type_(Type::Array), arr_(std::make_shared<JsonArray>(std::move(a))) {}
    JsonValue(JsonObject o) : type_(Type::Object), obj_(std::make_shared<JsonObject>(std::move(o))) {}

    Type type() const { return type_; }

    // 内部访问器(供 parse/dump 使用)。
    bool                       bool_value() const   { return bool_; }
    long long                  int_value()  const   { return int_; }
    double                     double_value() const { return double_; }
    const std::string&         str_value()  const   { return str_; }
    const std::shared_ptr<JsonArray>&  arr() const { return arr_; }
    const std::shared_ptr<JsonObject>& obj() const { return obj_; }

    std::string dump() const;

private:
    Type type_;
    bool bool_ = false;
    long long int_ = 0;
    double double_ = 0.0;
    std::string str_;
    std::shared_ptr<JsonArray> arr_;
    std::shared_ptr<JsonObject> obj_;
};

JsonValue parse_json(const std::string& text);

}  // namespace oj::util

#endif
