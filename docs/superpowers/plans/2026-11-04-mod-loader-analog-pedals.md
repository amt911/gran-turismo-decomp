# Mod loader and true-analog pedals — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `gt_play` loads native mods that hook game functions by guest address, and a first mod (`analog-pedals`, new repo `gt-mods`) gives true-analog throttle and brake on the controller triggers and ramped keyboard pedals.

**Architecture:** A C-ABI SDK header (`include/gt_mod/gt_mod_api.h`) is the only thing a mod sees. In the host, a manifest parser, a hook table (one trampoline keyed by `ctx.pc`, like `host/diff_harness.cpp`'s, installed with `Runtime::register_function` after the decomp registrations) and a `dlopen` loader, compiled into `gt_play` only. The mod is its own CMake project in `gt-mods`: a pure input-mapping library with unit tests, plus two hooks (0x08948B78 throttle record, 0x08952B68 brake pedal).

**Tech Stack:** C++20 host (clang, as `docs/COMPILAR.md`), C ABI for the SDK, `dlopen`, SDL2 (already a `gt_play` dependency), CMake + ctest.

**Spec:** `docs/superpowers/specs/2026-11-04-mod-loader-analog-pedals-design.md` (approved 2026-11-04).

## Global Constraints

- No change in `src/`, no patch of `recomp/` output; main-repo changes are host layer, SDK header, CMake, docs.
- With no mods, or `analog-pedals` installed with `enabled = false`, `gt_play` behaves exactly as today; `gt_native`, the gate, `--decomp diff` and synthetic cases never load mods.
- One hook per guest address across all mods; a second one stops startup naming both mod ids.
- `analog-pedals` defaults: `enabled = false`, `deadzone = 0.05`, `curve = 1.0`, `ramp_ms = 100`, throttle keys Up/W, brake keys Down/S; controller and keyboard combine by maximum.
- Throttle: record byte 0 |= 0x04, record +0xA (s16) = `round(throttle × 4096)`, always written for the player car (also 0). Brake: record +8 = 0x1000 if brake > 0 else 0, and float `car+8+0x530` = brake, written after 0x08952B68 returns. Player car only.
- Test seam: `GT_MOD_PEDALS_FAKE=<throttle>,<brake>` replaces SDL input; unset = no effect.
- `gt-mods` is a **private** GitHub repo; nothing game-derived in it. Builds and heavy runs inside `gt-heavy.slice` (AGENTS.md); clang builds.
- Commits: Conventional Commits, English, trailers from the session.

## Review Focus

1. A mods dir with a broken entry (missing library, bad `mod.toml`, wrong `api`) — expect that mod skipped with a one-line reason, the game still starts (Task 3 test `loader_skips_broken_mods`).
2. The hooked address is also a registered decompiled function under `--decomp on` — expect `call_original` to run the decompiled one (`ctx.pc` must still equal the address when the original is called) (Task 2 test `hook_wraps_previous_registration`).
3. Trigger at rest reporting a small non-zero value — expect 0 output under the dead zone, and full scale reached at the top (Task 4 tests).
4. AI cars and replay/demo playback while the mod is on — expect untouched (Task 6 in-game check prints an AI car's pedals too).
5. Keyboard key held across a pause/menu — expect the ramp not to jump to full on resume (ramp is time-based on hook calls; Task 4 test `ramp_is_capped_per_step`).

---

### Task 1: SDK header and manifest parser

**Files:**
- Create: `include/gt_mod/gt_mod_api.h`
- Create: `host/mod_manifest.hpp`, `host/mod_manifest.cpp`
- Test: `host/tests/mod_manifest_test.cpp`; Modify: `CMakeLists.txt` (test target `mod_manifest_tests`, `add_test(NAME mod_manifest …)`)

**Interfaces:**
- Produces (C, `gt_mod_api.h`): `#define GT_MOD_API_VERSION 1`;
  opaque `typedef struct GtModCtx GtModCtx;` `typedef struct GtMod GtMod;`
  `typedef void (*GtHookFn)(GtModCtx *ctx);`
  `typedef struct GtModApi { uint32_t version; int (*hook)(GtMod *, uint32_t address, GtHookFn); void (*call_original)(GtModCtx *); uint32_t (*gpr)(GtModCtx *, unsigned reg); void (*set_gpr)(GtModCtx *, unsigned reg, uint32_t); uint8_t (*read8)(GtModCtx *, uint32_t); uint16_t (*read16)(GtModCtx *, uint32_t); uint32_t (*read32)(GtModCtx *, uint32_t); float (*readf)(GtModCtx *, uint32_t); void (*write8)(GtModCtx *, uint32_t, uint8_t); void (*write16)(GtModCtx *, uint32_t, uint16_t); void (*write32)(GtModCtx *, uint32_t, uint32_t); void (*writef)(GtModCtx *, uint32_t, float); const char *(*config)(GtMod *, const char *key); void (*log)(GtMod *, const char *message); } GtModApi;`
  entry point a mod exports: `int gt_mod_init(const GtModApi *api, GtMod *self);` (0 = ok).
  Header must compile as C99 and C++ (`extern "C"` guard).
- Produces (C++, `mod_manifest.hpp`, namespace `gt::mods`): `struct Manifest { std::string id, name, version, library; int api = 0; bool enabled = false; std::map<std::string, std::string> settings; };` `std::optional<Manifest> parse_manifest(std::string_view text, std::string &error);`
  Format: TOML subset — `key = value` lines, `"strings"`, integers, `true/false`, `#` comments, one `[settings]` table whose keys go to `settings` as raw strings. Required: `id`, `library`, `api`.

- [ ] **Step 1: Write failing tests** `parse_full_manifest` (all fields + two settings → values equal), `missing_id_is_error` (nullopt, error mentions `id`), `enabled_defaults_false`, `comments_and_blank_lines_ignored`, `unknown_top_level_key_is_error`; plus a C compile check: a `host/tests/mod_api_c_check.c` that includes the header, built as a C object in the test target.
- [ ] **Step 2: Run** `ctest --test-dir build -R mod_manifest` → FAIL (not built).
- [ ] **Step 3: Implement** header and `parse_manifest` (hand-written line parser, no new dependency).
- [ ] **Step 4: Run** `ctest --test-dir build -R mod_manifest` → PASS.
- [ ] **Step 5: Commit** `feat(host): mod SDK header and manifest parser`.

### Task 2: Hook table

**Files:**
- Create: `host/mod_hooks.hpp`, `host/mod_hooks.cpp`
- Test: `host/tests/mod_hooks_test.cpp`; Modify: `CMakeLists.txt` (`mod_hooks_tests`, links `psprecomp_core`)

**Interfaces:**
- Consumes: `gt_mod_api.h` (Task 1).
- Produces (namespace `gt::mods`): `struct HookOwner { std::string mod_id; GtMod *mod; };`
  `bool install_hook(psprecomp::Runtime &rt, std::uint32_t address, GtHookFn fn, HookOwner owner, std::string &error);` — fails if the address has no function (`rt.function_at == nullptr`) or already has a mod hook (error names both mod ids).
  `const GtModApi &mod_api();` — the table handed to mods; `call_original` restores `ctx.pc = address` then runs the saved `RecompiledFunction`.
  `void clear_hooks_for_tests();`
  `struct GtModCtx` (defined in `mod_hooks.cpp`): `{ psprecomp::Runtime *rt; psprecomp::AllegrexContext *ctx; std::uint32_t address; }`.
  One native trampoline registered with `rt.register_function(address, &trampoline, "mod:<id>")`, looking the hook up by `ctx.pc` (as `trampoline_body` in `host/diff_harness.cpp`).
  Return to the caller: the trampoline does what a registered function does today (the original sets `ctx.pc = ra`); if the hook never calls `call_original`, the trampoline sets `ctx.pc = ctx.gpr[31]` itself.

- [ ] **Step 1: Write failing tests** on a `psprecomp::Runtime rt(1 << 20)` with a fake original registered at 0x08900000 that writes v0 = a0 + 1:
  `hook_wraps_original` (hook sets a0 = 41, calls original, checks v0 == 42, sets v0 = 7 → caller sees 7);
  `second_hook_same_address_refused` (error contains both ids);
  `hook_on_unknown_address_refused`;
  `hook_wraps_previous_registration` (a second fake registered over the first before the hook — `call_original` runs the second, with `ctx.pc == 0x08900000` inside it);
  `hook_without_call_original_returns_to_ra`.
- [ ] **Step 2: Run** `ctest --test-dir build -R mod_hooks` → FAIL.
- [ ] **Step 3: Implement** `mod_hooks.cpp`.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** `feat(host): mod hook table`.

### Task 3: Loader, wired into gt_play only

**Files:**
- Create: `host/mod_loader.hpp`, `host/mod_loader.cpp`; `host/tests/mods/test_mod.c` (+ its `mod.toml` generated by CMake into a test dir)
- Test: `host/tests/mod_loader_test.cpp`
- Modify: `host/main.cpp` (after `gt::install_decompiled(...)`), `CMakeLists.txt` (`gt_play` gets `GT_ENABLE_MODS=1` and links `${CMAKE_DL_LIBS}`; `gt_native` does not), `config/gt_play.ini` only if a key is needed (prefer none).

**Interfaces:**
- Consumes: Tasks 1–2.
- Produces: `struct LoadReport { std::vector<std::string> loaded, skipped; };` (skipped entries are `"<dir>: <reason>"`)
  `LoadReport load_mods(psprecomp::Runtime &rt, const std::filesystem::path &dir, std::string &fatal);` — returns with `fatal` set only for a hook conflict (startup must stop); everything else is a skip.
  `std::filesystem::path mods_dir(const char *argv0);` — `$GT_MODS_DIR` if set, else `<dir of executable>/mods`.
  In `main.cpp`, under `#ifdef GT_ENABLE_MODS`: refuse to load (one stderr line, continue without mods) when `options.decomp == Diff` or synthetic mode is on; otherwise `load_mods`, print `[mods] loaded: … skipped: …`, and exit with status 3 on `fatal`.
- `test_mod.c` hooks 0x08900000 and adds 100 to v0 after `call_original`; its `mod.toml` settings key `bonus = "100"` is read through `api->config`.

- [ ] **Step 1: Write failing tests** `loads_enabled_mod_and_hooks` (fake original + test mod → v0 = a0 + 1 + 100), `disabled_mod_not_loaded`, `loader_skips_broken_mods` (dirs: no `mod.toml`; bad TOML; missing `.so`; `api = 2` → four skips, game state untouched), `two_mods_same_address_is_fatal`, `empty_or_missing_dir_loads_nothing`.
- [ ] **Step 2: Run** `ctest --test-dir build -R mod_loader` → FAIL.
- [ ] **Step 3: Implement** loader + `main.cpp` wiring.
- [ ] **Step 4: Run** all host tests (`ctest --test-dir build`) → PASS; `gt_play` with no `mods/` dir boots to the save_garage reference state as on main (`tools/verdict.sh` boot line or the boot check of `docs/agents/runtime-verification.md`).
- [ ] **Step 5: Commit** `feat(host): load native mods in gt_play`; open the main-repo PR (with **How to test manually**) — it can merge before the mod exists.

### Task 4: `gt-mods` repo and the pedal input mapping

**Files (new repo `gt-mods`, created private with `gh repo create amt911/gt-mods --private`):**
- Create: `README.md` (what it is, needs a `gran-turismo-decomp` checkout, how to build/install, licence MIT), `CMakeLists.txt` (cache var `GT_SDK_DIR`, default `../gran-turismo-decomp/include`; ctest), `.gitignore`, `analog-pedals/mod.toml` (spec defaults, `enabled = false`), `analog-pedals/src/pedal_input.hpp/.cpp`
- Test: `analog-pedals/tests/pedal_input_test.cpp`

**Interfaces:**
- Produces: `struct PedalSettings { float deadzone = 0.05f, curve = 1.0f; int ramp_ms = 100; };`
  `float trigger_to_pedal(std::int16_t raw, const PedalSettings &);` — raw SDL trigger 0..32767 → 0..1: below `deadzone` → 0, then rescaled to 0..1 and raised to `curve`.
  `class KeyRamp { public: float step(bool held, int elapsed_ms, int ramp_ms); };` — moves towards 1 (held) or 0 at `elapsed_ms / ramp_ms` per step, `ramp_ms == 0` → instant, `elapsed_ms` capped at 50 per step.
  `float combine(float pad, float keys);` → max.
  `std::optional<std::pair<float,float>> parse_fake(const char *env);` — `"0.5,0.3"` → {0.5, 0.3}; malformed or out of 0..1 → nullopt.

- [ ] **Step 1: Write failing tests**: `deadzone_zeroes_rest_noise` (raw 1000 → 0), `full_press_is_one` (32767 → 1.0), `linear_midpoint` (curve 1, raw at the middle of the live range → 0.5 ± 0.001), `curve_two_squares` (0.5 → 0.25), `ramp_reaches_one_after_ramp_ms`, `ramp_zero_is_instant`, `ramp_is_capped_per_step` (one step of 5000 ms moves at most 50/ramp_ms), `release_ramps_down`, `combine_takes_max`, `fake_env_parses_and_rejects`.
- [ ] **Step 2: Run** `ctest --test-dir build` in `gt-mods` → FAIL.
- [ ] **Step 3: Implement** `pedal_input.cpp`.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit + push** `feat(analog-pedals): input mapping` to `gt-mods` main (new private repo; pushing the user-approved new repo is part of creating it).

### Task 5: Find the player car (research, read-only)

**Files:** Modify: `docs/FACTS.md` (main repo) — a "Player car / input record" entry with the evidence.

**Interfaces:**
- Produces: a predicate the hooks use, written down with evidence: for 0x08948B78 (args: a0 = record) and 0x08952B68 (a0 = car, a1 = record), how to tell the human-driven car from AI and replay/demo playback (start from the `world+0x10` controller and "mode 0" in `func_0894DD9C`; the caller's frame/registers available in the hook are a0–a3 only). If 0x08948B78 cannot tell, hook `func_0894DD9C`'s other callee that has the car (state which) — still not touching game code.

- [ ] **Step 1:** Read `func_0894DD9C` and callers in `tools.disasm`; write the predicate with addresses as evidence in FACTS.
- [ ] **Step 2:** Confirm at runtime with `GT_PROBE` on the race route (one player car, AI cars): the predicate is true for exactly one car per frame in a race, false in a replay.
- [ ] **Step 3: Commit** `docs(facts): how the per-car step tells the player car`.

### Task 6: The `analog-pedals` hooks and the in-game check

**Files (gt-mods):** Create: `analog-pedals/src/mod.cpp` (exports `gt_mod_init`), `analog-pedals/src/input_source.hpp/.cpp` (SDL or fake). Modify: `CMakeLists.txt` (shared library `analog_pedals`, links SDL2), `README.md` (install: copy `analog-pedals/` with the `.so` into `<build>/mods/`, set `enabled = true`).
**Files (main repo):** `tools/mod_pedals_check.sh` (or a pytest in `tools/tests/` if it fits the existing route tooling better) + `docs/FINDINGS.md`/`docs/FACTS.md` entries, `AGENTS.md` § Commands (the new runnable command), `README.md` mention.

**Interfaces:**
- Consumes: Task 4 functions, Task 5 predicate, `GtModApi`.
- `input_source`: `struct PedalInput { float throttle, brake; }; PedalInput read_input(const PedalSettings &, int elapsed_ms);` — `parse_fake(getenv("GT_MOD_PEDALS_FAKE"))` when set; else SDL: first `SDL_GameController` (its own handle) `SDL_CONTROLLER_AXIS_TRIGGERRIGHT/LEFT` through `trigger_to_pedal`, keyboard via `SDL_GetKeyboardState` through `KeyRamp`, `combine`. Thread choice (SDL read at hook time vs. a snapshot) is decided here: SDL's controller/keyboard state getters read cached state, so reading at hook time is allowed **only if** `gt_play` pumps events on another thread and a 10-minute race shows no crash/race report; otherwise add a per-frame snapshot service to the SDK (bump `GT_MOD_API_VERSION`).
- Hooks: `0x08948B78` → `call_original`, then if player: byte0 |= 0x04, s16 +0xA = round(t×4096). `0x08952B68` → `call_original`, then if player: s16 record +8 = brake > 0 ? 0x1000 : 0, float `car+8+0x530` = brake.

- [ ] **Step 1: Write the in-game check first**: run headless `gt_play` on the race route (as `tools/gpu_gate.sh` does) with `GT_MODS_DIR` set to the built mod, `enabled = true`, `GT_MOD_PEDALS_FAKE=0.5,0.3`, and `GT_PROBE` (or a mod log line every 60 hook calls, enabled by `GT_MOD_PEDALS_LOG=1`) printing the player car's `+0x538`, `+0x530` and one AI car's. Pass = player `+0x538` within 0.02 of 0.5 after settling and `+0x530` == 0.3; AI values not constant at 0.5/0.3. Run → FAIL (no mod).
- [ ] **Step 2: Implement** `mod.cpp`, `input_source.cpp`.
- [ ] **Step 3: Run** the check → PASS. **Sabotage**: write the throttle to +0xC instead of +0xA → check FAILs; restore.
- [ ] **Step 4: Off = identical**: same route with the mod installed and `enabled = false` → `[mods] skipped` line, boot/route result identical to no mods dir.
- [ ] **Step 5: Replay measurement**: save a replay of a run with fake brake 0.3, play it back, record whether brake replays as 0.3 or on/off; write the result in FINDINGS and tell the user (spec risk).
- [ ] **Step 6: Check** HUD pedal bar, ABS/traction aids with brake 0.3 (a screenshot of the HUD for the user, not committed); note anything odd in FINDINGS.
- [ ] **Step 7: Commit** both repos (`feat(analog-pedals): true-analog throttle and brake hooks`; main repo `test(tools): in-game check for the analog-pedals mod`), push `gt-mods`, open the main-repo PR with **How to test manually** (build both, install the mod, drive with the Xbox controller and the keyboard).
