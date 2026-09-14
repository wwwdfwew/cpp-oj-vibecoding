#include "util/json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace oj::util {

namespace {

void skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

std::string parse_string(const std::string& s, size_t& i) {
    if (s[i] != '"') throw std::runtime_error("expected string");
    ++i;
    std::string out;
    while (i < s.size() && s[i] != '"') {
        char c = s[i++];
        if (c == '\\' && i < s.size()) {
            char esc = s[i++];
            switch (esc) {
                case '"':  out += '"'; break;
                case '\\': out += '\\'; break;
                case '/':  out += '/'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    if (i + 4 > s.size()) throw std::runtime_error("bad unicode escape");
                    unsigned code = static_cast<unsigned>(std::stoul(s.substr(i, 4), nullptr, 16));
                    i += 4;
                    if (code < 0x80) {
                        out += static_cast<char>(code);
                    } else if (code < 0x800) {
                        out += static_cast<char>(0xC0 | (code >> 6));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (code >> 12));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default: out += esc;
            }
        } else {
            out += c;
        }
    }
    if (i >= s.size()) throw std::runtime_error("unterminated string");
    ++i;
    return out;
}

JsonValue parse_value(const std::string& s, size_t& i);

JsonValue parse_array(const std::string& s, size_t& i) {
    ++i;
    JsonArray arr;
    skip_ws(s, i);
    if (i < s.size() && s[i] == ']') { ++i; return JsonValue(std::move(arr)); }
    while (true) {
        skip_ws(s, i);
        arr.push_back(parse_value(s, i));
        skip_ws(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == ']') { ++i; break; }
        throw std::runtime_error("expected ',' or ']'");
    }
    return JsonValue(std::move(arr));
}

JsonValue parse_object(const std::string& s, size_t& i) {
    ++i;
    JsonObject obj;
    skip_ws(s, i);
    if (i < s.size() && s[i] == '}') { ++i; return JsonValue(std::move(obj)); }
    while (true) {
        skip_ws(s, i);
        std::string key = parse_string(s, i);
        skip_ws(s, i);
        if (i >= s.size() || s[i] != ':') throw std::runtime_error("expected ':'");
        ++i;
        skip_ws(s, i);
        obj.emplace(std::move(key), parse_value(s, i));
        skip_ws(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') { ++i; break; }
        throw std::runtime_error("expected ',' or '}'");
    }
    return JsonValue(std::move(obj));
}

JsonValue parse_value(const std::string& s, size_t& i) {
    skip_ws(s, i);
    if (i >= s.size()) throw std::runtime_error("unexpected eof");
    char c = s[i];
    if (c == '"') return JsonValue(parse_string(s, i));
    if (c == '{') return parse_object(s, i);
    if (c == '[') return parse_array(s, i);
    if (c == 't' || c == 'f') {
        if (s.compare(i, 4, "true") == 0) { i += 4; return JsonValue(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return JsonValue(false); }
    }
    if (c == 'n') {
        if (s.compare(i, 4, "null") == 0) { i += 4; return JsonValue(nullptr); }
    }
    size_t start = i;
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        while (i < s.size()) {
            char d = s[i];
            if (std::isdigit(static_cast<unsigned char>(d)) || d == '-' || d == '+' ||
                d == '.' || d == 'e' || d == 'E') {
                ++i;
            } else break;
        }
        std::string num = s.substr(start, i - start);
        if (num.find('.') != std::string::npos || num.find('e') != std::string::npos ||
            num.find('E') != std::string::npos) {
            return JsonValue(std::stod(num));
        }
        return JsonValue(static_cast<long long>(std::stoll(num)));
    }
    throw std::runtime_error("invalid json token");
}

std::string dump_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += "\"";
    return out;
}

std::string dump_value(const JsonValue& v) {
    switch (v.type()) {
        case JsonValue::Type::Null:   return "null";
        case JsonValue::Type::Bool:   return v.bool_value() ? "true" : "false";
        case JsonValue::Type::Int:    return std::to_string(v.int_value());
        case JsonValue::Type::Double: {
            std::ostringstream oss; oss << v.double_value(); return oss.str();
        }
        case JsonValue::Type::String: return dump_string(v.str_value());
        case JsonValue::Type::Array: {
            const auto& a = v.arr();
            std::string out = "[";
            for (size_t i = 0; i < a->size(); ++i) {
                if (i) out += ",";
                out += dump_value((*a)[i]);
            }
            out += "]";
            return out;
        }
        case JsonValue::Type::Object: {
            const auto& o = v.obj();
            std::string out = "{";
            bool first = true;
            for (const auto& [k, val] : *o) {
                if (!first) out += ",";
                first = false;
                out += dump_string(k);
                out += ":";
                out += dump_value(val);
            }
            out += "}";
            return out;
        }
    }
    return "null";
}

}  // namespace

std::string JsonValue::dump() const { return dump_value(*this); }

JsonValue parse_json(const std::string& text) {
    size_t i = 0;
    skip_ws(text, i);
    JsonValue v = parse_value(text, i);
    skip_ws(text, i);
    return v;
}

}  // namespace oj::util
