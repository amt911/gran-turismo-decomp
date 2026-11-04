#include "mod_manifest.hpp"

#include <cctype>
#include <charconv>
#include <set>

namespace gt::mods {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

// Parses the value part of a line: either a "quoted string" (no escapes; '#' allowed inside) or a bare token cut at
// a '#' comment. `is_string` tells the caller which one it got. False + `why` on a malformed value.
bool parse_value(std::string_view v, std::string &out, bool &is_string, std::string &why) {
    v = trim(v);
    if (v.empty()) {
        why = "missing value";
        return false;
    }
    if (v.front() == '"') {
        const size_t close = v.find('"', 1);
        if (close == std::string_view::npos) {
            why = "unterminated string";
            return false;
        }
        const std::string_view rest = trim(v.substr(close + 1));
        if (!rest.empty() && rest.front() != '#') {
            why = "unexpected text after string";
            return false;
        }
        out.assign(v.substr(1, close - 1));
        is_string = true;
        return true;
    }
    const size_t hash = v.find('#');
    out.assign(trim(v.substr(0, hash)));
    is_string = false;
    if (out.empty()) {
        why = "missing value";
        return false;
    }
    return true;
}

} // namespace

std::optional<Manifest> parse_manifest(std::string_view text, std::string &error) {
    Manifest m;
    bool in_settings = false, have_id = false, have_library = false, have_api = false;
    int line_no = 0;
    std::set<std::string> seen_top, seen_settings;  // a repeated key is an error, never "last one wins"
    auto fail = [&](const std::string &msg) -> std::optional<Manifest> {
        error = (line_no ? "line " + std::to_string(line_no) + ": " : std::string()) + msg;
        return std::nullopt;
    };

    while (!text.empty()) {
        const size_t nl = text.find('\n');
        std::string_view line = trim(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
        ++line_no;
        if (line.empty() || line.front() == '#') continue;

        if (line.front() == '[') {
            const size_t close = line.find(']');
            if (close == std::string_view::npos) return fail("unterminated table header");
            const std::string_view rest = trim(line.substr(close + 1));
            if (!rest.empty() && rest.front() != '#') return fail("unexpected text after table header");
            if (trim(line.substr(1, close - 1)) != "settings") return fail("unknown table (only [settings] exists)");
            in_settings = true;
            continue;
        }

        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) return fail("expected 'key = value'");
        const std::string key(trim(line.substr(0, eq)));
        if (key.empty()) return fail("empty key");
        std::string value, why;
        bool is_string = false;
        if (!parse_value(line.substr(eq + 1), value, is_string, why)) return fail(key + ": " + why);

        if (!(in_settings ? seen_settings : seen_top).insert(key).second) return fail("duplicate key '" + key + "'");
        if (in_settings) {
            m.settings[key] = value;
            continue;
        }
        auto want_string = [&](std::string &dst) -> bool {
            if (!is_string) return false;
            dst = value;
            return true;
        };
        if (key == "id" || key == "name" || key == "version" || key == "library") {
            std::string &dst = key == "id" ? m.id : key == "name" ? m.name : key == "version" ? m.version : m.library;
            if (!want_string(dst)) return fail(key + " must be a \"string\"");
            have_id |= key == "id";
            have_library |= key == "library";
        } else if (key == "api") {
            int n = 0;
            const auto r = std::from_chars(value.data(), value.data() + value.size(), n);
            if (is_string || r.ec != std::errc() || r.ptr != value.data() + value.size()) return fail("api must be an integer");
            m.api = n;
            have_api = true;
        } else if (key == "enabled") {
            if (is_string || (value != "true" && value != "false")) return fail("enabled must be true or false");
            m.enabled = value == "true";
        } else {
            return fail("unknown key '" + key + "'");
        }
    }

    line_no = 0;
    if (have_id && m.id.empty()) return fail("'id' must not be empty");
    if (have_library && m.library.empty()) return fail("'library' must not be empty");
    if (!have_id) return fail("missing required key 'id'");
    if (!have_library) return fail("missing required key 'library'");
    if (!have_api) return fail("missing required key 'api'");
    return m;
}

} // namespace gt::mods
