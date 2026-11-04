// Native mod loader: finds mod directories (`mod.toml` + shared library), dlopens them and runs gt_mod_init.
// Only gt_play uses it (gt::frontend_load_mods, host/frontend.hpp); `GT_NO_MODS=1` disables it (verification tools set it); see include/gt_mod/gt_mod_api.h.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace psprecomp {
class Runtime;
}

namespace gt::mods {

struct LoadReport {
    std::vector<std::string> loaded; // mod ids
    std::vector<std::string> skipped; // "<dir>: <reason>"
};

// Loads every `<dir>/<mod>/mod.toml` that is enabled. A broken, disabled or failing mod is skipped (reported); `fatal` is
// set only when two mods hook the same address (startup must stop). A missing or empty `dir` loads nothing.
LoadReport load_mods(psprecomp::Runtime &rt, const std::filesystem::path &dir, std::string &fatal);

// `$GT_MODS_DIR` if set and non-empty, else `<directory of the executable>/mods` (/proc/self/exe, argv0 as fallback).
std::filesystem::path mods_dir(const char *argv0);

} // namespace gt::mods
