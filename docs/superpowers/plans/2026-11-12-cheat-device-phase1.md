# Cheat device, phase 1 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The approved cheat device (spec below) running in `gt_play`: one menu opened with F9 / SELECT+START, categories of
entries from the host and the mods, actions with keyboard and pad hotkeys the player rebinds from the menu, drawn in the
existing lcs text overlay.

**Architecture:** The pure model already exists (`host/cheat_device/menu.*`, `bindings.*`, PR #569). This plan adds the file IO
of the bindings, a `Device` that builds the tree from the host and the mods and owns the bindings, a delegated page in the lcs
overlay (patch 0063) so the device drives its own navigation, mod API v4 (`add_action`, `add_entry`), the host-side hotkey
dispatch (keyboard events, pad hotkeys with hold-back), and the migration of three mods in `amt911/gt-mods`.

**Tech Stack:** C++20 (host), C99 mod ABI (`include/gt_mod/gt_mod_api.h`), lcs-recomp patches (`tools/patches/lcs-recomp/`),
CTest, bash in-game checks (`tools/mod_*_check.sh`).

**Spec:** `docs/superpowers/specs/2026-11-11-cheat-device-design.md` (approved by the user 2026-11-12, open questions answered
"as proposed": SELECT+START stays the default, the device replaces the F9 MODS page, no extra cheats in v1).

## Global Constraints

- Conflicts are refused, never "last one wins" (spec §2): `BindingTable` already refuses; nothing in the wiring may bypass it.
- `device.open` defaults: keyboard **F9**, pad **SELECT+START**; it is `required` (never left with no binding).
- A pad binding of a single race button needs a modifier or a hold time (already in `BindingTable::set_pad`).
- Pad buttons of any binding are held back from the game up to **8 vblanks** (today's `PadMenuConfig::hold_back`); keyboard
  hotkeys never reach the game.
- `bindings.toml` lives next to the mods folder, written atomically (tmp + rename), only non-default bindings written; a damaged
  file loads the defaults and warns.
- Action callbacks run on the vblank tick (API v2 main-thread rules), never on the window or pad thread.
- With the device closed and no hotkey pressed, frames are byte-identical to no mod (`tools/mods_together_check.sh`).
- Mod API: appended only (v1–v3 prefix unchanged); v4 functions refuse a mod that declared `api < 4`.
- In a replay race cheats show `(replay: recorded)` and do not change.

## Review Focus

1. A mod that declares an action whose default clashes with another mod's: the second mod still loads, its action exists
   unbound, and the log names both (test in Task 4: `add_action` with a clashing default returns 0, action present, binding empty).
2. `bindings.toml` written while the game crashes mid-write: the old file survives (Task 1: `save_file` to a read-only dir fails
   and leaves the old file byte-identical).
3. The player switches a mod off: its actions' hotkeys stop firing but keep their saved bindings (Task 4 test).
4. Holding SELECT for reset (SELECT@30) while SELECT+START opens the device: SELECT+START wins, reset does not fire (already in
   `PadHotkeys`; Task 5 checks the wiring with the real `PadMenuFilter` hold-back).
5. Rebinding capture while a race runs: the game sees none of the captured buttons (Task 5 in-game `--hotkey` check).

---

### Task 1: bindings.toml file IO

**Files:**
- Modify: `host/cheat_device/bindings.hpp`, `host/cheat_device/bindings.cpp`
- Test: `host/tests/cheat_device_test.cpp`

**Interfaces:**
- Produces: `bool BindingTable::save_file(const std::filesystem::path &path, std::string &error) const;`
  `void BindingTable::load_file(const std::filesystem::path &path, std::vector<std::string> &warnings);` (missing file = no
  warning; unreadable = warning, defaults kept).

- [ ] Test `test_file_round_trip`: set nitro key Alt+F6, `save_file(tmp/"bindings.toml")`, fresh table + defaults +
  `load_file` → nitro key `{"F6", kAlt}`; the file has no `.tmp` sibling left.
- [ ] Test `test_failed_save_keeps_old_file`: save once, make the dir read-only, change a binding, `save_file` returns false
  with `error` set, file bytes unchanged.
- [ ] Run `ctest --test-dir build -R cheat_device` → the two new tests FAIL (link error).
- [ ] Implement: write `to_text()` to `<path>.tmp`, `fsync`, `rename`; load reads the file into `load_text`.
- [ ] Run → PASS. Commit `feat(host): cheat device bindings.toml load/save (atomic)`.

### Task 2: lcs overlay delegated page (patch 0063)

**Files:**
- Create: `tools/patches/lcs-recomp/0063-host-overlay-delegated-page.patch`
- Modify (through the patch): `lcs/host/lcs_display_menu.{hpp,cpp}`, `tests/lcs_host_menu_test.cpp`; `tools/setup.sh` patch list.

**Interfaces:**
- Produces in `lcs/host/lcs_display_menu.hpp`:
  `struct HostExtraPage { ...existing...; bool (*key)(HostSettingsKey key); void (*view)(HostSettingsView &view); };`
  When `key` is set the overlay calls it for Up/Down/Left/Right/Activate/Close while the extra page is shown, and closes the
  overlay when it returns false; when `view` is set it fills the view instead of the row window. Old pages (null) unchanged.
  `HostSettingsView::rows` grows to **8** rows (`kViewRows = 8`); the main page keeps using 5.

- [ ] Test in `lcs_host_menu_test.cpp`: a delegated page sees Up then Close; returning false from Close closes the overlay;
  the old MODS-style page still scrolls 5 rows.
- [ ] Run the lcs test target → FAIL.
- [ ] Implement in the patch; both renderers (`display_window_sdl.cpp`, `display_window.cpp`) draw `kViewRows` rows, box height
  from the row count.
- [ ] Run → PASS; `tools/setup.sh` re-applies cleanly on a fresh `tools/src/lcs-recomp`. Commit
  `feat(tools): lcs overlay page that drives its own navigation (patch 0063)`.

### Task 3: Device (tree + bindings + overlay adapter)

**Files:**
- Create: `host/cheat_device/device.hpp`, `host/cheat_device/device.cpp`
- Modify: `host/play_backends.cpp` (register the device page instead of `kModsPage`), `CMakeLists.txt`
- Test: `host/tests/cheat_device_test.cpp`

**Interfaces:**
- Consumes: `Navigator`, `Entry`, `BindingTable`, `PadCapture` (#569), Task 1, Task 2.
- Produces (namespace `gt::cheat_device`):
  `void add_category_entry(const std::string &category, Entry entry);` (host and mods; categories created on first use in the
  spec order Race, Car, Money & garage, Camera, Mods, Controls, Settings);
  `BindingTable &bindings();`
  `bool handle_key(lcs::HostSettingsKey key);` (Close = `Navigator::back()`, false at the root);
  `void fill_view(lcs::HostSettingsView &view);` (title = `Navigator::path()`, 8 rows `"<title padded> <value>"`, footer
  `"SELECT+START / F9 CLOSE"` from the current open bindings);
  `void open_at(const std::string &category);` (F9 opens on Mods, as today).
  The **Mods** category holds one Toggle per mod (`gt::mods::menu` rows) and a Submenu per mod with its v2 rows.
  The **Controls** category holds one Submenu per action: `Keyboard <binding>` (Enter = capture next key), `Pad <binding>`
  (Enter = `PadCapture`), `Clear`, `Reset to default`; a refused binding shows the error text in the row for 3 s.

- [ ] Tests: a device with two mods' rows shows `Cheats > Mods` first; Controls → nitro → Pad → capture L+TRIANGLE from
  `PadCapture` polls → row shows `L+TRIANGLE`; capture SELECT+START for nitro → row shows `is already Open the cheat device`
  and the binding is unchanged; holding CIRCLE 30 vblanks during a capture cancels it (binding unchanged); Close at the root
  returns false.
- [ ] Run → FAIL. Implement. Run → PASS.
- [ ] Build `gt_play`; `tools/mod_cheats_check.sh` (existing, default flags) still PASS with the device as the F9 page.
- [ ] Commit `feat(host): cheat device replaces the F9 MODS page`.

### Task 4: Mod API v4 (`add_action`, `add_entry`) and dispatch

**Files:**
- Modify: `include/gt_mod/gt_mod_api.h`, `host/mod_services.{hpp,cpp}`, `host/mod_loader.cpp` (accept `api = 4`),
  `docs/MODS.md`
- Test: `host/tests/mod_services_test.cpp` (existing) or new `host/tests/mod_actions_test.cpp`

**Interfaces:**
- Produces in the ABI (appended after `car_set_state`, `GT_MOD_API_VERSION_4 4`):
  `typedef void (*GtActionFn)(GtMod *self, GtModCtx *ctx);`
  `int (*add_action)(GtMod *self, const char *id, const char *label, const char *category, GtActionFn fn,
  const char *default_key, const char *default_pad);` (strings in `parse_key` / `parse_pad` syntax, "" = none; init only;
  0 = ok; a clashing default → 0, action registered unbound, `[mods]` log names both owners);
  `int (*add_entry)(GtMod *self, const char *category, const char *id, GtMenuLabelFn label, GtMenuActivateFn activate);`
  (a v2-style row placed in a category instead of the mod's submenu).
  Host: `void gt::mods::services::queue_action(const std::string &action_id);` (any thread) and the vblank tick runs the queued
  actions of active mods in order, once each.

- [ ] Tests: v3 mod calling `add_action` → refused; v4 mod → action `cheats.nitro` in `bindings()` with the defaults; a second
  mod with the same default key → registered, unbound, warning; queued action of a switched-off mod does not run but its
  binding survives `to_text()`.
- [ ] Run → FAIL. Implement. Run → PASS. Commit `feat(host): mod API v4 actions and category entries`.

### Task 5: Hotkeys in gt_play (keyboard + pad, hold-back)

**Files:**
- Modify: `host/play_backends.cpp`, `host/pad_menu.{hpp,cpp}`
- Test: `host/tests/pad_menu_test.cpp`

**Interfaces:**
- Consumes: `PadHotkeys`, `bindings()`, `queue_action`.
- Produces: `PadMenuConfig::held_back_buttons` (the union of the buttons of every pad binding, refreshed when bindings change)
  replacing the single `chord` for the hold-back; `device.open` fires through `PadHotkeys` like any action. Keyboard: the SDL
  key-down handler maps `SDL_GetKeyName` + Ctrl/Shift/Alt to `KeyBinding`, `by_key` → `queue_action` (F9 → `device.open`).

- [ ] Test: with nitro on L+TRIANGLE, a lone L press is held back ≤ 8 polls then given late with its real length; L+TRIANGLE
  completes → the game never sees L or TRIANGLE; SELECT+START with reset on SELECT@30 → only `device.open`.
- [ ] Run → FAIL. Implement. Run → PASS.
- [ ] In game: new flags in `tools/mod_cheats_check.sh`: `--device` (chord, walk to Race → Slow motion, 0.5x, close; slow
  motion measured), `--hotkey` (bind nitro to L+TRIANGLE through the capture by a pad route; distance gain measured),
  `--rebind-conflict` (bind reset's key to nitro: refused, log line). Sabotages `--sabotage-hotkey-ignored`,
  `--sabotage-conflict-allowed` must FAIL.
- [ ] `tools/mods_together_check.sh` PASS (device closed, no hotkey: frames identical to no mod).
- [ ] Commit `feat(host): cheat device hotkeys (keyboard, pad with hold-back)`.

### Task 6: gt-mods migration (repo `amt911/gt-mods`)

**Files:**
- Modify: `cheats/src/*.cpp` + `cheats/mod.toml` (`api = 4`), `reset-to-track/src/{mod.cpp,input_source.cpp}` +
  `mod.toml`, `orbit-camera/src/mod.cpp` + `mod.toml`, CI `SDK_REF` to the decomp main that has Tasks 1–5.

**Interfaces:**
- Consumes: v4 `add_action` / `add_entry`.
- Actions: `cheats.nitro` (Ctrl+N, L+TRIANGLE), `cheats.jump` (Ctrl+J, R+TRIANGLE), `reset-to-track.reset` (R, SELECT@30),
  `orbit-camera.invert`, `orbit-camera.reset_view` (no default pad). Entries: slow motion, freeze AI, AI speed, moon gravity →
  Race; money ±1M → Money & garage. reset-to-track drops `reset_keys`, `SelectGate` and its own Select reading (the host holds
  the button back now); its in-game check runs the reset through the action. In a replay (role 3 car present, no live
  human) the cheats entries' value reads `(replay: recorded)` and `activate` does nothing (unit test in `cheats/tests`).

- [ ] Each mod's unit tests updated and passing; `tools/mod_{cheats,reset,camera}_check.sh` PASS with `--decomp off` and `on`;
  sabotages still FAIL.
- [ ] PR in gt-mods; release after the user's go (publishing is the user's call).

### Task 7: Docs and spec status

- Modify: `docs/MODS.md` (device, hotkeys, `bindings.toml`, API v4), spec status line → approved 2026-11-12, `README.md` mods line.
- Commit `docs: cheat device phase 1`.
