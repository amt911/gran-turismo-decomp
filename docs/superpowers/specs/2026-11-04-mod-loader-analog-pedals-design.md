# Mod loader and true-analog pedals

Status: approved by the user on 2026-11-04 ("lo apruebo, sigue").

## Why

The user plays GT on PC with an Xbox One controller and misses what every PC racing game has: **throttle and brake
on the analog triggers, both truly analog** (user, 2026-11-04: "quiero que ambos sean analógicos, true analog
ambos"). It must also work on a keyboard, like a conventional PC racing game.

The user also set *how*: **"no toques el código, que sea como Dusklight o similar, que se cargue mediante parches"**.
Improvements are mods loaded from outside, not edits to the game's source (decompiled `src/` or the recompiled
baseline). Dusklight (Twilight Princess PC port) is the reference: mods are separate bundles; native C++ mods hook
game functions through the port's mod services. The user allowed a new repository for the mods.

This is the first of four requested improvements (analog pedals, right-stick camera, draw distance, > 60 fps). The
loader is built so the others can be mods later; they are **not** part of this design.

## Goals

1. **Mod loader** in the main repo: at startup `gt_play` loads native mods (shared libraries) from a mods directory
   and lets each one hook game functions by guest address.
2. **Mod SDK**: one public header (plus what it needs) that a mod compiles against.
3. **First mod, `analog-pedals`**, in a new repository `gt-mods`: right trigger = throttle 0..1, left trigger = brake
   0..1, both analog; keyboard keys give full throttle / brake through an adjustable ramp.

## Non-goals

- Right-stick camera, draw distance, high frame rate (later mods, later designs).
- Script mods (Luau or similar), an in-game mod browser, update checks, an in-game settings menu.
- Windows / macOS loading (Linux `dlopen` first; the API is written so `LoadLibrary` can follow).
- Rebinding the rest of the controls.

## Anti-goals (a failure even if it "works")

- Editing game code to get the feature: no change in `src/`, no patch of `recomp/` output. The only main-repo change is
  the loader + SDK (host layer, ours).
- A mod changing anything when it is **off** or **not installed**: with no mods the game, the differential gate and the
  boot checks behave exactly as today.
- Silent conflicts: two hooks on one address must refuse to start, naming both mods (project rule "no silent last one
  wins").
- Shipping game data or recompiled code in `gt-mods`.

## What we know about the game (evidence)

From a read-only pass over the asm on 2026-11-04 (addresses are runtime, = ELF vaddr + 0x08804000). **Observed** =
read in the asm or in already-decompiled code; **inferred** = reading of the asm not yet checked at runtime.

- Pad read: `sceCtrlReadBufferPositive` stub 0x08C7ABF0; its caller 0x08B333C8 stores buttons at `this+0x10`, analog
  X/Y at `this+0x14/+0x15` (observed).
- Per-car step `func_0894DD9C` builds a 0x20-byte **input record** on the stack for each car: 0x08948AE4 zeroes it,
  the controller at `world+0x10` fills it through its vtable slot 0xC, **0x08948B78 clamps it** (steer +4 to ±0x2000,
  s16 fields +8, +0xA, +0xC, +0xE, +0x10 to 0..0x1000), then **0x08952B68(car, record)** consumes it (observed).
  Which function is the slot-0xC filler is unknown.
- Throttle: 0x08952B68 reads record **+0xA**; when bit **0x04 of record byte 0** is set it takes `+0xA / 4096` as
  the target and ramps the pedal `car+8+0x538` (0..1) towards it; when clear, +0xA is on/off (observed,
  0x08954208–0x08954298). So the game already has an **analog throttle path**.
- Brake: the pedal is `car+8+0x530` (0..1); its ramp (0x08953FE4–0x089541F8) only tests record +8 non-zero and moves
  at fixed rates — **digital** (inferred). `car+8+0x534` (effective throttle) is derived from +0x538 and +0x530 later
  in the step (`func_0894FCA8`, `func_08947CF0`) (observed). Traction aids read +0x530 later too (inferred).
- Replays serialize the same record layout (0x088951FC, 0x088955D4) (observed; interaction not studied).

## Design

### Loader (main repo, `host/`)

- `gt_play` (the SDL build) scans `mods/` next to the executable — or `GT_MODS_DIR` — at startup, before the game's
  first instruction. Each subdirectory with a `mod.toml` (id, name, version, `api` version, library file, `enabled`)
  is a mod. Disabled or missing → skipped. `gt_native` and the gate never load mods (they keep `GT_MODS_DIR` unset
  and ignore `mods/`), so verification is untouched.
- The loader `dlopen`s the library and calls its exported `gt_mod_init(const GtModApi *api, GtMod *self)`. A wrong
  `api` version → refused with a message, game continues without that mod.
- **Hooks**: `api->hook(self, address, fn)` installs a native function at a guest address with the same signature
  decompiled functions have (`void(Runtime&, AllegrexContext&)`). Inside it, `api->call_original(ctx)` runs what was
  there before (the recompiled function, or the decompiled one if registered) with the current registers, so a mod
  can wrap: change inputs, call the original, change outputs. One hook per address across all mods; a second one
  stops the startup with both mod ids.
  The builder reuses the registry/dispatch the decomp harness already has (`host/decomp_registry.hpp`, `diff_harness`)
  rather than a second dispatch path.
- SDK services available to a mod: guest memory read/write (typed helpers, as in `include/`), the mod's own config
  values from `mod.toml`, a log prefixed with the mod id. The SDK is a header under `include/gt_mod/` with a version
  number; `gt-mods` gets it as a pinned dependency (git submodule or fetched by commit — builder's call).

### Mod `analog-pedals` (repo `gt-mods`)

- **Input**: reads the Xbox controller's trigger axes and the keyboard itself through SDL2 (its own
  `SDL_GameController` handle; SDL reference-counts opens), so the host input code — part of the fetched
  `lcs-recomp`, which today turns the triggers into on/off buttons above 8192 — stays untouched. Trigger value →
  dead zone → response curve → 0..1. Keyboard: throttle/brake keys ramp 0 → 1 over `ramp_ms` (default 100 ms; 0 =
  instant). Controller and keyboard combine by taking the larger value.
- **Throttle** (game's own analog path): wrap **0x08948B78** — call the original, then, for the player's car only, set
  bit 0x04 of record byte 0 and write `round(throttle × 4096)` to record +0xA. The game's ramp, clamping and replay
  recording then see an ordinary analog throttle.
- **Brake** (true analog, user's choice): the game has no analog brake path, so wrap **0x08952B68** — call the
  original, then, for the player's car only, overwrite `car+8+0x530` with the brake value, before 0x0894E9C0 and the
  rest of the drive chain read it. Keep record +8 non-zero whenever brake > 0 so the brake-state byte (`car+0x50B`)
  and the `car+0x50E` "pressed" bits stay consistent.
- **Only the player's car**: the hooks must touch the human-driven car and never AI cars or replay/demo playback.
  How to tell them apart (the `world+0x10` controller, "mode 0" in `func_0894DD9C`) is the first research item of the
  plan.
- **Settings** (`mod.toml`): `enabled` (default **false**), `deadzone` (default 0.05), `curve` (exponent, default
  1.0 = linear), `ramp_ms` (100), key bindings (defaults: Up/W throttle, Down/S brake).

## Verification

- **Off = identical**: with no mods dir, and with `analog-pedals` installed but disabled, the full differential gate and
  the boot check give the same results as `main` (they run without mods anyway; the check is that `gt_play` with a
  disabled mod reaches the same reference state).
- **Loader tests** (ctest): a test mod hooks an address and wraps it; two mods on one address refuse to start; wrong
  API version is refused; a mod with `enabled=false` is not loaded.
- **Mod tests** (in `gt-mods`): the input mapping (dead zone, curve, ramp, max of sources) as unit tests.
- **In game**: a scripted race (existing pad routes or a new one) with a synthetic trigger value — e.g. throttle held at
  0.5 — checks that `car+8+0x538` settles at 0.5 and brake 0.3 gives `+0x530` = 0.3, for the player car only, AI cars
  unchanged. Sabotage: break the hook (write the wrong field) and see the check fail.
- **Manual**: the user drives with the Xbox controller and with the keyboard.

## Answers to the builder check (2026-11-04)

- **Hooks reach only callers that go through the dispatcher** (corrected 2026-11-04 after Task 5/6 measurements).
  A call inside one recompiled unit that the recompiler turned into a `goto` / direct jump never consults the function
  table, so a hook on its target does not fire: `0x08952B68` is reached that way from `0x0894DD9C` in the default
  `--decomp off` build (`generated_unit_0041.cpp:5338`; `GT_PROBE` hits 0). Check a hook point with `GT_PROBE=<addr>` in
  both `--decomp off` and `--decomp on` before relying on it (docs/FINDINGS.md). Original text, true for those callers:
  recompiled code calls other functions through the runtime's address dispatch:
  decompiled functions already replace recompiled ones with `runtime.register_function(address, …)` and
  `runtime.function_at(address)` gives the one that was there (`host/diff_harness.cpp`, install loop). The loader uses
  the same two calls; no recompiler or `recomp/` change. 0x08948B78 is a function entry (`func_08948B78`, 53
  instructions), as is 0x08952B68.
- **Hook vs a decompiled registration** (Claude's call): mods install **after** the decomp registrations, so
  `call_original` runs whatever is at the address then — the decompiled function under `--decomp on`, the recompiled
  one otherwise. A hook on a decompiled address is allowed. Mods are never loaded together with `--decomp diff`, synthetic
  cases or the gate (the loader refuses that combination), so the harness never compares a hooked function.
- **SDK binary interface** (Claude's call): a **stable C ABI** — `gt_mod_api.h`, plain C structs of function pointers
  and opaque handles: read/write guest memory, get/set a guest register, `call_original`, config lookup, log. No
  `Runtime`/`AllegrexContext`/`lcs-recomp` headers leak into the SDK; the API version number is the compatibility
  check. A thin C++ convenience header on top is fine.
- **The game's own digital input while the mod is on** (Claude's call): for the player's car the mod **always
  overwrites** the throttle field (+0xA with bit 0x04 set, also when the value is 0) and the brake (record +8 = 0x1000
  when brake > 0 else 0, and `+0x530` = the analog value), so a trigger that the host also turns into a button press
  cannot add a second, digital input. Other buttons and the rest of the record are left as the game filled them.
- **Test seam** (Claude's call): `GT_MOD_PEDALS_FAKE=<throttle>,<brake>` (only read when set) replaces the SDL input
  with fixed values, for the in-game check. Unset → no effect.
- **Threads** (builder): the game runs on its own thread while SDL events are pumped elsewhere; the builder decides
  between reading SDL state at hook time (if proven safe) and a per-frame input snapshot the loader takes on the SDL
  thread and hands to mods.

## Risks / open points for the builder

- **Replays and ghosts**: the brake override is not in the record, so a saved replay may play the brake back as
  on/off. Measure it; if it differs, say so to the user (a known limit) rather than hide it.
- The traction aids, ABS and the HUD pedal bar may assume the digital ramp; check they behave with intermediate brake
  values.
- 0x08948B78 / 0x08952B68 may be decompiled by the decomp batches later; the hook must work whether the function at
  the address is recompiled or decompiled (that is what `call_original` is for).

## Decided here (Claude's calls — the user can override)

- Loader in the main repo's host layer, mods in a separate repo `gt-mods`, created **private** on GitHub (publishing is
  the user's call).
- Native C++ mods only, Linux first, TOML manifest, mods off by default.
- Keyboard ramp 100 ms, controller + keyboard combined by maximum.
- **Measured mapping (Task 6, 2026-11-04): the spec's pedal names above are swapped.** Record `+8` (analog flag `0x02`) is the
  accelerator -> `car+8+0x530`; `+0xA` (flag `0x04`) is the brake -> `+0x538`. The record carries both analog values, so the mod
  injects both there and hooks only `0x0894DD9C` and `0x08948B78`; the game's own response (accelerator curve, 10 % brake floor)
  applies, and replays do not keep the injected values (docs/FACTS.md, docs/FINDINGS.md).
