// Mod API v2 services (include/gt_mod/gt_mod_api.h): menu rows on the F9 MODS page, the per-vblank tick and call_guest.
// Registration happens at load time only (while a mod's gt_mod_init runs, between begin_init and end_init); after that the
// registries are read-only, so the window thread (menu rendering) and the guest threads read them without a lock.
#pragma once

#include "mod_hooks.hpp"

#include <cstddef>
#include <cstdint>

namespace psprecomp {
class Runtime;
}

namespace gt::mods::services {

constexpr std::size_t kMaxRowIdLength = 31;
constexpr std::uint32_t kMaxGuestArguments = 8;

// The loader brackets each mod's gt_mod_init with these: the v2 registration functions work only for that mod, in between.
void begin_init(GtMod *mod);
void end_init();

// Implementations behind GtModApi's add_menu_row / on_vblank / call_guest (0 = ok, non-zero = refused; the reason goes to stderr).
int api_add_menu_row(GtMod *self, const char *id, GtMenuLabelFn label, GtMenuActivateFn activate);
int api_on_vblank(GtMod *self, GtVblankFn fn);
int api_call_guest(GtModCtx *ctx, std::uint32_t address, const std::uint32_t *args, std::uint32_t nargs, std::uint32_t *v0_out);

// Rows added by mods, in load order (all of mod 1's rows, then mod 2's...).
std::size_t menu_row_count();
// The row's text into out (NUL-terminated, cut to capacity-1). A row of a switched-off mod reads "<id> mod OFF". False: out of range.
bool menu_row_text(std::size_t index, char *out, std::size_t capacity);
// Enter (0), Left (-1) or Right (+1) on the row. False when out of range or the mod is switched off (the mod's function is not called).
bool menu_row_activate(std::size_t index, int direction);
// The mod's id of the row (for logs); empty when out of range.
const std::string &menu_row_mod_id(std::size_t index);

// Runs every active mod's vblank tick with a ctx that may call_guest. Called by the kernel once per game vblank (`vblank` = the count).
void run_vblank_tick(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint64_t vblank);
// True when any loaded mod registered a menu row or a tick (the host installs the pad menu and the tick only then).
bool any_registered();
// Behind GtModApi::is_menu_open / vblank_index. The host sets the query (gt_play: lcs::host_settings_open); null = never open.
void set_menu_open_query(bool (*query)());
int api_is_menu_open(GtMod *self);
std::uint64_t api_vblank_index(GtMod *self);

// A failed init: forget this mod's rows and tick.
void remove_for(const GtMod *mod);
// Tests: forget everything.
void clear_for_tests();

}  // namespace gt::mods::services
