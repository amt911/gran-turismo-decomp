// Mod hook table: wraps recompiled/decompiled guest functions by address for native mods (include/gt_mod/gt_mod_api.h).
#pragma once

#include "gt_mod/gt_mod_api.h"

#include <cstdint>
#include <map>
#include <string>

namespace psprecomp {
class Runtime;
}

// The loader's (and the tests') definition of the opaque GtMod of the C API.
struct GtMod {
    std::string id;
    std::map<std::string, std::string> settings;
};

namespace gt::mods {

struct HookOwner {
    std::string mod_id;
    GtMod *mod = nullptr;
};

// One hook per guest address across all mods. Fails (error filled) if the address has no function or already has a hook
// (the error names both mod ids). On success the function at `address` is replaced by a trampoline that runs `fn`.
bool install_hook(psprecomp::Runtime &rt, std::uint32_t address, GtHookFn fn, HookOwner owner, std::string &error);

// Returns (and clears) the error of the last install_hook refused because the address was already hooked; empty if none.
std::string take_hook_conflict();

// Runtime that `mod_api().hook` installs into (the loader calls this before gt_mod_init).
void bind_runtime(psprecomp::Runtime &rt);

// The function table handed to mods.
const GtModApi &mod_api();

// Forgets every hook (the trampolines stay registered in whatever Runtime they were installed in; tests use a fresh one).
void clear_hooks_for_tests();

} // namespace gt::mods
