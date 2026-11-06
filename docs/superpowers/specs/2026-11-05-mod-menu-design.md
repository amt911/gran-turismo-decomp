# In-game mod menu — design

**Status:** the user chose "an in-game overlay that turns mods on and off on the spot, Dusklight-style" (2026-11-05). Every
other decision below is Claude's call, marked *(Claude)*; the user reviews it on the PR.

## Intent

A player turns a mod on or off from inside `gt_play` without editing `mod.toml` or restarting, and sees which mods are installed
and whether they loaded. It is the front end of the mod loader (PR #471, `docs/MODS.md`); mods are still loaded from
`GT_MODS_DIR` / `mods/` and never touch game code.

## Goals

- A **MODS** page drawn by the existing host settings overlay, opened with **F9** *(Claude: F10's five rows are all used and the
  overlay images are sized for them; a separate key keeps every renderer's layout)*; Up/Down select, Left/Right/Enter toggle,
  F9/Escape close.
- One row per mod found: its name and `ON` / `OFF`; mods that failed to load are listed as `ERROR` (not toggleable) with the
  loader's reason shortened to the row width.
- Toggling takes effect **at once**: a disabled mod's hooks pass straight to the original function.
- The choice is **saved** to that mod's `mod.toml` (`enabled = true|false`), so it survives a restart.
- All four overlay renderers show it: SDL renderer (`paint_host_settings`), the Vulkan/DX12 rasterizer
  (`rasterize_settings_overlay`, 512x256) and the Win32 GDI panel (`display_window.cpp`). DX12/Win32 cannot be built here
  (Linux): their change is kept to drawing the title/footer strings and is said to be untested on the PR.

## Non-goals

- Editing a mod's own settings (deadzone, curve…) in the menu *(Claude: shown read-only is also out; v1 only toggles. Settings
  need a "config changed" callback in the SDK — a later API version)*.
- Controller navigation of the F10 overlay (it is keyboard-only today; unchanged) *(Claude)*.
- Loading a mod that was not present at start, or unloading a library (`dlclose`) at run time.
- `gt_native` (no window, no mods).

## Anti-goals

- A disabled mod changing anything in the game. Off must be byte-for-byte the game without that mod's hooks (proved in a check).
- A toggle racing the game thread (a torn read of the flag, a hook half-installed).
- Losing the user's other `mod.toml` lines or comments when saving.

## Design

1. **Loader** *(Claude)*: a mod whose manifest says `enabled = false` is now **loaded and initialised but inactive**, instead of
   skipped (needed to turn it on later without installing hooks from another thread). `GtMod` gains `std::atomic<bool> active`
   and the path of its `mod.toml`. `LoadReport` becomes a list of entries {id or directory name, state loaded-active /
   loaded-inactive / error, reason, manifest path, `GtMod*`}; the startup `[mods]` log line keeps its current text so existing
   checks still match. A hook conflict stays fatal as today.
   Consequence to document: a disabled mod's `gt_mod_init` runs (it may open a controller, read settings). `GT_NO_MODS=1` still
   loads nothing.
2. **Hooks**: the trampoline reads the owner's `active` **once at entry** (acquire) and keeps that value for the whole call, so a
   toggle mid-call never gives a half-modded call. Inactive → `Hook.original(rt, ctx)` with `ctx.pc` left at the hooked address
   and the same post-call handling as `call_original`; no mod code runs. Active → as now. The toggle is a release store from the
   window/event thread.
3. **Menu model** (host side, ours): `host/mod_menu.{hpp,cpp}` exposes the list (name, state ON/OFF/ERROR, reason) and
   `toggle(index)`, which flips `active` and rewrites `enabled =` in that mod's `mod.toml` (the line replaced in place; added
   under the first table if missing; a write failure is logged and shown as `SAVE FAILED` on the row, the toggle still applies).
4. **lcs-recomp patch** `tools/patches/lcs-recomp/0059-host-overlay-extra-page.patch` (MIT, applied by `tools/setup.sh` like the
   others): `HostSettingsView` gains `title` and `footer` strings (main page: `HOST SETTINGS` / `F10 CLOSE`, unchanged look);
   `host_settings_set_extra_page(provider)` registers a page {title, row count, row text(i), toggle(i)}; F9 opens/closes it
   (no provider → F9 does nothing, so lcs-recomp's own builds are unchanged). The page keeps 5 visible rows and scrolls a
   window over the list with the selection. Each renderer draws `view.title` / `view.footer` instead of the constants.
5. `gt_play` registers the provider from `frontend_load_mods` (host/play_backends.cpp).

## Testing

- Unit (ctest): manifest rewrite (line replaced, comments kept, missing key added, read-only file → error); menu model toggles;
  trampoline inactive → original runs (fake runtime, as the current mod hook tests do); lcs menu test extended for paging.
- In-game: `tools/mod_pedals_check.sh` gains `--toggle-off`: the mod is loaded with `enabled = true`, and a test-only env
  `GT_MOD_MENU_TEST_TOGGLE=<mod id>@<seconds>` (a gt_play thread calling the same `toggle` the menu calls) turns it off mid-race; pedals must be injected before and **not** after
  (player pedals return to the route's own). Sabotage: the trampoline ignoring `active` must make it fail.
- `enabled = false` at start: same `script done at vblank` as `--without-mod` (the existing `--disabled` check).

## Left to the builder

Row layout and colours (match the existing overlay), the text shown while the overlay has no mods (`NO MODS INSTALLED`), how the
reason is shortened.
