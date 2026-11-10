#include "mini_json.hpp"

#include <cstdlib>
#include <string>

namespace gt::json {

const Value *Value::get(std::string_view key) const {
    if (kind != Kind::Object) return nullptr;
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == key) return &values[i];
    return nullptr;
}

namespace {

constexpr int kMaxDepth = 64;

struct Parser {
    std::string_view text;
    std::size_t at = 0;
    std::string &error;

    bool fail(const char *what) {
        if (error.empty()) error = std::string(what) + " at byte " + std::to_string(at);
        return false;
    }
    void skip_space() {
        while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\n' || text[at] == '\r')) ++at;
    }
    bool literal(std::string_view word) {
        if (text.substr(at, word.size()) != word) return false;
        at += word.size();
        return true;
    }
    static void append_utf8(std::string &out, unsigned code) {
        if (code < 0x80u) {
            out += static_cast<char>(code);
        } else if (code < 0x800u) {
            out += static_cast<char>(0xC0u | (code >> 6u));
            out += static_cast<char>(0x80u | (code & 0x3Fu));
        } else if (code < 0x10000u) {
            out += static_cast<char>(0xE0u | (code >> 12u));
            out += static_cast<char>(0x80u | ((code >> 6u) & 0x3Fu));
            out += static_cast<char>(0x80u | (code & 0x3Fu));
        } else {
            out += static_cast<char>(0xF0u | (code >> 18u));
            out += static_cast<char>(0x80u | ((code >> 12u) & 0x3Fu));
            out += static_cast<char>(0x80u | ((code >> 6u) & 0x3Fu));
            out += static_cast<char>(0x80u | (code & 0x3Fu));
        }
    }
    bool hex4(unsigned &code) {
        if (at + 4 > text.size()) return fail("short \\u escape");
        code = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text[at++];
            code <<= 4u;
            if (c >= '0' && c <= '9') code |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') code |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code |= static_cast<unsigned>(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool string(std::string &out) {
        ++at;  // the opening quote
        while (at < text.size()) {
            const char c = text[at++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20u) return fail("control character in string");
            if (c != '\\') {
                out += c;
                continue;
            }
            if (at >= text.size()) break;
            const char e = text[at++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned code = 0;
                if (!hex4(code)) return false;
                if (code >= 0xD800u && code < 0xDC00u) {  // a surrogate pair
                    unsigned low = 0;
                    if (!literal("\\u") || !hex4(low) || low < 0xDC00u || low >= 0xE000u) return fail("bad surrogate pair");
                    code = 0x10000u + ((code - 0xD800u) << 10u) + (low - 0xDC00u);
                } else if (code >= 0xDC00u && code < 0xE000u) {
                    return fail("lone low surrogate");
                }
                append_utf8(out, code);
                break;
            }
            default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool number(Value &out) {
        const std::size_t start = at;
        if (at < text.size() && text[at] == '-') ++at;
        if (at >= text.size() || text[at] < '0' || text[at] > '9') return fail("bad number");
        while (at < text.size() && ((text[at] >= '0' && text[at] <= '9') || text[at] == '.' || text[at] == 'e' ||
                                    text[at] == 'E' || text[at] == '+' || text[at] == '-'))
            ++at;
        const std::string digits(text.substr(start, at - start));
        char *end = nullptr;
        out.number = std::strtod(digits.c_str(), &end);
        if (end != digits.c_str() + digits.size()) return fail("bad number");
        out.kind = Value::Kind::Number;
        return true;
    }
    bool value(Value &out, int depth) {
        if (depth > kMaxDepth) return fail("nested too deep");
        skip_space();
        if (at >= text.size()) return fail("unexpected end");
        const char c = text[at];
        if (c == '{') {
            out.kind = Value::Kind::Object;
            ++at;
            skip_space();
            if (at < text.size() && text[at] == '}') { ++at; return true; }
            while (true) {
                skip_space();
                if (at >= text.size() || text[at] != '"') return fail("expected a key");
                std::string key;
                if (!string(key)) return false;
                skip_space();
                if (at >= text.size() || text[at] != ':') return fail("expected ':'");
                ++at;
                Value member;
                if (!value(member, depth + 1)) return false;
                out.keys.push_back(std::move(key));
                out.values.push_back(std::move(member));
                skip_space();
                if (at < text.size() && text[at] == ',') { ++at; continue; }
                if (at < text.size() && text[at] == '}') { ++at; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            out.kind = Value::Kind::Array;
            ++at;
            skip_space();
            if (at < text.size() && text[at] == ']') { ++at; return true; }
            while (true) {
                Value item;
                if (!value(item, depth + 1)) return false;
                out.array.push_back(std::move(item));
                skip_space();
                if (at < text.size() && text[at] == ',') { ++at; continue; }
                if (at < text.size() && text[at] == ']') { ++at; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            out.kind = Value::Kind::String;
            return string(out.string);
        }
        if (literal("true")) { out.kind = Value::Kind::Bool; out.boolean = true; return true; }
        if (literal("false")) { out.kind = Value::Kind::Bool; out.boolean = false; return true; }
        if (literal("null")) { out.kind = Value::Kind::Null; return true; }
        return number(out);
    }
};

} // namespace

std::optional<Value> parse(std::string_view text, std::string &error) {
    error.clear();
    Parser parser{text, 0, error};
    Value root;
    if (!parser.value(root, 0)) return std::nullopt;
    parser.skip_space();
    if (parser.at != text.size()) {
        parser.fail("trailing characters");
        return std::nullopt;
    }
    return root;
}

} // namespace gt::json
