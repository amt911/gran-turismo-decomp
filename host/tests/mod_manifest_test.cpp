// Unit tests for gt::mods::parse_manifest (host/mod_manifest.cpp). Plain executable, run by CTest.
#include "mod_manifest.hpp"

#include <cstdio>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    using gt::mods::parse_manifest;
    std::string err;

    {
        auto m = parse_manifest("id = \"analog\"\nname = \"Analog Pedals\"\nversion = \"1.2\"\n"
                                "library = \"analog.so\"\napi = 1\nenabled = true\n"
                                "[settings]\ndeadzone = 0.15\ncurve = \"cubic\"\n",
                                err);
        check(m.has_value(), "parse_full_manifest: parses");
        if (m) {
            check(m->id == "analog" && m->name == "Analog Pedals" && m->version == "1.2", "full: strings");
            check(m->library == "analog.so" && m->api == 1 && m->enabled, "full: library/api/enabled");
            check(m->settings.size() == 2, "full: two settings");
            check(m->settings["deadzone"] == "0.15", "full: raw numeric setting");
            check(m->settings["curve"] == "cubic", "full: string setting unquoted");
        }
    }
    {
        err.clear();
        auto m = parse_manifest("library = \"a.so\"\napi = 1\n", err);
        check(!m.has_value(), "missing_id_is_error: nullopt");
        check(err.find("id") != std::string::npos, "missing_id_is_error: message mentions id");
    }
    {
        auto m = parse_manifest("id = \"x\"\nlibrary = \"x.so\"\napi = 1\n", err);
        check(m.has_value() && !m->enabled, "enabled_defaults_false");
    }
    {
        auto m = parse_manifest("# header\n\n  id = \"x\"   # trailing\nlibrary = \"x#.so\"\n\napi = 1\n", err);
        check(m.has_value(), "comments_and_blank_lines_ignored: parses");
        if (m) check(m->id == "x" && m->library == "x#.so", "comments: '#' inside a string kept");
    }
    {
        err.clear();
        auto m = parse_manifest("id = \"x\"\nlibrary = \"x.so\"\napi = 1\nbogus = 3\n", err);
        check(!m.has_value(), "unknown_top_level_key_is_error: nullopt");
        check(err.find("bogus") != std::string::npos, "unknown key: message names it");
    }
    {
        err.clear();
        check(!parse_manifest("id = \"x\"\nlibrary = \"x.so\"\n", err) && err.find("api") != std::string::npos,
              "missing api is error");
        err.clear();
        check(!parse_manifest("id \"x\"\n", err).has_value(), "line without '=' is error");
        check(!parse_manifest("id = \"x\nlibrary = \"x.so\"\napi = 1\n", err).has_value(), "unterminated string is error");
        check(!parse_manifest("id = \"x\"\nlibrary = \"x.so\"\napi = one\n", err).has_value(), "non-integer api is error");
        check(!parse_manifest("id = \"x\"\nlibrary = \"x.so\"\napi = 1\nenabled = yes\n", err).has_value(),
              "non-bool enabled is error");
        check(!parse_manifest("id = \"x\"\nlibrary = \"x.so\"\napi = 1\n[other]\n", err).has_value(), "unknown table is error");
    }
    if (failures == 0) std::puts("mod_manifest: all checks passed");
    return failures == 0 ? 0 : 1;
}
