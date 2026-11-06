// Mod manifest: a small TOML subset (`key = value`, "strings", integers, true/false, `#` comments, one [settings] table).
#pragma once

#include <filesystem>
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

// Sets the root-level `enabled = true|false` of a mod.toml in place: the existing line keeps its indentation and trailing
// comment; every other line is untouched. A missing key is added after the last root key (before the first [table]).
// The new text goes to `<manifest>.tmp` and is renamed over the file (never a truncated manifest); a read-only file fails. False + `error` on any I/O problem.
bool write_enabled(const std::filesystem::path &manifest, bool enabled, std::string &error);

} // namespace gt::mods
