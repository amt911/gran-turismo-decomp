// Mod hook table: wraps recompiled/decompiled guest functions by address for native mods (include/gt_mod/gt_mod_api.h).
#pragma once

#include "gt_mod/gt_mod_api.h"

#include "psprecomp/allegrex_context.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

namespace psprecomp {
class Runtime;
}

struct GtMod;

// What a hook (or a vblank tick) receives: the runtime and CPU state it runs against, the address it wraps and its link in that
// address's chain. `tick` is true only inside a vblank tick (the one place `call_guest` is allowed).
struct GtModCtx {
    psprecomp::Runtime *rt;
    psprecomp::AllegrexContext *ctx;
    std::uint32_t address;
    std::size_t link;
    bool tick = false;
    const GtMod *mod = nullptr;  // the mod whose hook (or tick) this is
};

// The loader's (and the tests') definition of the opaque GtMod of the C API.
struct GtMod {
    std::string id;
    std::map<std::string, std::string> settings;
    // Gate of every hook this mod owns: read once at the entry of each hooked call (acquire); false = the call goes straight
    // to the original. Written by `set_active` (the mod menu, from the window thread).
    std::atomic<bool> active{true};
    // The `api =` of its mod.toml (1 or 2): the version the mod is told, and the gate of the v2 functions.
    int api_version = GT_MOD_API_VERSION;
    std::filesystem::path manifest;  // its mod.toml (the menu rewrites `enabled =` there); empty in tests
};

namespace gt::mods {

struct HookOwner {
    std::string mod_id;
    GtMod *mod = nullptr;
};

// Hooks on one guest address form a chain, outermost first (install order): the trampoline runs the first active hook, its
// call_original runs the next active one, the last one's runs the game's function; inactive hooks are skipped. Fails (error
// filled) if the address has no function or `owner.mod` already hooks it.
bool install_hook(psprecomp::Runtime &rt, std::uint32_t address, GtHookFn fn, HookOwner owner, std::string &error);

// Turns the hooks of `mod` on or off (release store); takes effect at the next hooked call.
void set_active(GtMod *mod, bool on);

// Removes every hook `mod` installed, from any place in its chain; an address left with no hook gets its function back
// (used when a mod's init fails).
void remove_hooks_for(psprecomp::Runtime &rt, const GtMod *mod);

// Runtime that `mod_api().hook` installs into (the loader calls this before gt_mod_init).
void bind_runtime(psprecomp::Runtime &rt);

// The function table handed to mods, with `version` set to what the mod declared (1 or 2; anything else gets 2). Same functions;
// the v2 ones refuse a mod whose GtMod::api_version is 1.
const GtModApi &mod_api(int version = GT_MOD_API_VERSION_2);

// Forgets every hook (the trampolines stay registered in whatever Runtime they were installed in; tests use a fresh one).
void clear_hooks_for_tests();

} // namespace gt::mods
