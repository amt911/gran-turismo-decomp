// Native mod loader: finds mod directories (`mod.toml` + shared library), dlopens them and runs gt_mod_init.
// Only gt_play uses it (gt::frontend_load_mods, host/frontend.hpp); `GT_NO_MODS=1` disables it (verification tools set it); see include/gt_mod/gt_mod_api.h.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct GtMod;

namespace psprecomp {
class Runtime;
}

namespace gt::mods {

enum class ModState { Active, Inactive, Error };

// One mod directory, as the mod menu lists it. `mod` is null for Error.
struct ModEntry {
    std::string name;  // manifest name, else id, else the directory name
    ModState state = ModState::Error;
    std::string reason;  // Inactive: "disabled"; Error: why it did not load
    std::filesystem::path manifest;  // <dir>/mod.toml
    GtMod *mod = nullptr;
};

struct LoadReport {
    std::vector<std::string> loaded; // mod ids of the active mods
    std::vector<std::string> skipped; // "<dir>: <reason>" (inactive mods too: "<dir>: disabled")
    std::vector<ModEntry> entries;  // every directory examined, in order
};

// Loads every `<dir>/<mod>/mod.toml`. A mod with `enabled = false` is loaded and initialised but left inactive (its hooks pass
// to the original; the mod menu can turn it on). A broken or failing mod is skipped (reported as Error); `fatal` is
// set only when two mods hook the same address (startup must stop). A missing or empty `dir` loads nothing.
LoadReport load_mods(psprecomp::Runtime &rt, const std::filesystem::path &dir, std::string &fatal);

// `$GT_MODS_DIR` if set and non-empty, else `<directory of the executable>/mods` (/proc/self/exe, argv0 as fallback).
std::filesystem::path mods_dir(const char *argv0);

} // namespace gt::mods
