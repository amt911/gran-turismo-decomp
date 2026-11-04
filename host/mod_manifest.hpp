// Mod manifest: a small TOML subset (`key = value`, "strings", integers, true/false, `#` comments, one [settings] table).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace gt::mods {

struct Manifest {
    std::string id, name, version, library;
    int api = 0;
    bool enabled = false;
    std::map<std::string, std::string> settings; // raw values; a quoted string is stored without its quotes
};

// Required keys: id, library, api. On failure returns nullopt and puts a one-line reason (with line number) in `error`.
std::optional<Manifest> parse_manifest(std::string_view text, std::string &error);

} // namespace gt::mods
