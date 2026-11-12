# Cheat device — design

**Status:** approved by the user on 2026-11-12 ("apruebo la spec"; the open questions below as proposed: SELECT+START stays,
the device replaces the F9 MODS page, no extra cheat in v1). Asked for by the user, 2026-11-11: "los cheats me gustaría que fuera a lo mod menu de videojuego, como
el cheat device del GTA Vice City Stories, que tenga una combinación de teclado y mando y que sea configurable para cambiar los
hotkeys con el mando y el teclado"). Decisions marked *(Claude)* were taken during unattended work and are open to change.
Builds on the cheat menu spec (`2026-11-07-cheat-menu-design.md`), the mod API v2/v3 (`docs/MODS.md`) and the pad menu
(`host/pad_menu.cpp`).

## Intent

One in-game menu for everything the player can switch on: a list of cheats grouped in categories, opened with a button
combination on the pad or a key on the keyboard, driven entirely with the pad or the keyboard, like a homebrew cheat device.
Every cheat that is an action (jump, nitro on/off, slow motion step, reset to track) can also get its own **hotkey**, on the
keyboard and on the pad, and the player rebinds them from the menu itself.

## What exists today (evidence)

- **F9 MODS page** (`host/mod_menu.cpp`, `host/play_backends.cpp`): one row per mod (ON/OFF), then the rows mods add with
  `add_menu_row` (API v2). Five rows visible, text only, inside lcs-recomp's settings overlay (`HostExtraPage`, patch 0061).
- **Pad chord** SELECT+START (`GT_PAD_MENU_CHORD`) opens the same page; the filter hides the pad from the game while the
  overlay is open and holds chord buttons back 8 vblanks (MODS.md "Menú con el mando", measured: no pair is free in a race).
- **Keys are read by mods themselves**: reset-to-track reads `reset_keys` (default R) with `SDL_GetKeyboardState` and Select
  from the pad buffer (`reset-to-track/src/input_source.cpp`); cheats has no hotkeys. Two mods can grab the same key and nobody
  notices: a silent "last one wins" in the player's hands.

## Proposal

### 1. The menu model (host, UI-agnostic) *(Claude)*

A pure C++ model in `host/cheat_device/` (no SDL, no guest memory; unit-tested):

- **Pages and entries.** A tree: root → categories (`Race`, `Car`, `Money & garage`, `Camera`, `Mods`, `Controls`,
  `Settings`) → entries. Entry kinds: `toggle` (ON/OFF), `choice` (one of N values, Left/Right), `action` (fires once,
  Enter), `number` (stepped value with limits), `submenu`, `info` (read-only text, e.g. money balance).
- **Navigation:** Up/Down, Left/Right change a value, Enter fires/opens, Back goes up one level, the open combination closes
  from anywhere. Repeat on held Up/Down (18 then every 4 vblanks, as today). The cursor position per page is remembered.
- **State shown live:** every entry's text comes from its owner on each draw (as `label_fn` today).
- **Mods page:** the current F9 list (mod ON/OFF, ERROR with reason) becomes one category; F9 opens the device on it.

### 2. Actions and hotkeys (host owns the input) *(Claude)*

- **Mod API v4 `add_action(self, id, label, category, fn, default_key, default_pad)`**: a mod declares an action; the host
  shows it in the menu, binds it, and calls `fn` (on the vblank tick, main-thread rules of API v2) when its hotkey fires.
  A mod no longer reads the keyboard for its actions: reset-to-track's `reset_keys` and its Select hold move to an action
  (`reset`, default keyboard R, pad: hold SELECT 0.5 s — the host's binding supports "hold" as a trigger).
- **Bindings** are `keyboard` (a key plus optional Ctrl/Shift/Alt) and `pad` (a combination of up to 3 buttons, each either
  *press* or *hold N ms*). Stored in one file the player owns, `bindings.toml` next to the mods folder, written atomically
  (tmp + rename, as `mod.toml`). Defaults come from the mods; a row in the file overrides them.
- **Rebinding from the menu:** Controls → pick an action → "Press the new combination…" captures the next keyboard key or pad
  combination (pad: buttons held together within 8 vblanks, released = done; Back-hold cancels). Each action shows both
  bindings; "Clear" and "Reset to default" per action and for all.
- **Conflicts are refused, never "last one wins":** a binding already used by another action (or by the menu's own open
  combination) is shown in red with the owner's name and not saved until the player clears the other one. Pad bindings
  that are a single button the game uses in a race (cross, square, R, L, D-pad) need a modifier (another held button) or a
  hold time: the capture says so and refuses.
- **What the game sees:** pad buttons of a binding are hidden from the game the way the chord is today (held back up to 8
  vblanks; if the combination does not complete, the game gets them late with their real length). Keyboard hotkeys never
  reach the game (the game has no keyboard).

### 3. Opening the device

- Defaults: **keyboard F9**, **pad SELECT+START** (today's chord, the measured best), both rebindable from Controls like
  any action (the open combination is itself an action, `device.open`, which cannot be cleared: at least one way in stays).
- In a replay the device opens too, but race cheats show `(replay: recorded)` and do not change (cheats 0.4.0 replays what
  was recorded live).

### 4. Drawing *(Claude)*

The model is drawn by a small view layer. **Phase 1** draws it in the existing lcs settings overlay (text rows, a title
line with the category path, more than 5 visible rows, highlighted cursor, ON in green / OFF in grey), so the device ships
without waiting for the new UI. **Phase 2** draws the same model with the standalone app's toolkit (Dear ImGui, spec
`2026-11-11-standalone-app-design.md`): translucent panel, category icons, a hint bar with the buttons of the current
binding scheme. The model and its tests do not change between the two.

### 5. Cheats in the first version

From the existing `cheats` mod (API v2 rows become entries/actions): slow motion (choice), freeze AI (toggle), AI speed
(choice), nitro (toggle + hotkey), moon gravity (toggle), jump (action + hotkey), money ±1M (number, with the save backup
already in place). From reset-to-track: reset (action + hotkey). From orbit-camera: invert, reset view (actions). The
"later" list of the cheat-menu spec (power/weight, all cars and colours, unlock tracks) lands as entries once each is
proven in game.

## Testing

- **Unit (host/tests):** navigation (tree, Back, cursor memory, repeat), value cycling, binding parse/print round trip,
  conflict refusal (same key twice, open combination taken, single race button refused), capture state machine (pad
  combination within the window, cancel), `bindings.toml` atomic write and recovery from a damaged file (defaults + warning).
- **In game (tools/mod_cheats_check.sh, new flags):** `--device` opens the device with the default chord, walks to Race →
  Slow motion, sets 0.5x, closes, and the measured slow motion holds; `--hotkey` binds nitro to L+TRIANGLE through the
  capture (scripted pad route) and checks the distance gain; `--rebind-conflict` tries to bind reset's key to nitro and
  checks it is refused. Each has a sabotage that must fail (hotkey ignored, conflict allowed).
- **Coexistence:** `tools/mods_together_check.sh` with every mod on, and the frames with the device closed and no hotkey
  pressed byte-identical to no mod (as today).
- Differential gate + boot check for the host change.

## Non-goals

Memory-address code lists (PS2/PSP "codes"), online play, cheats outside the native code's reach (Adhoc scripts).

## Open questions (the user's)

1. Default open combination: keep SELECT+START (measured), or do you prefer another (e.g. L+R+SELECT)?
2. Should the device replace the F9 MODS page entirely (proposed: yes, Mods is one category inside it)?
3. Any cheat you want in the first version that is not in §5?
