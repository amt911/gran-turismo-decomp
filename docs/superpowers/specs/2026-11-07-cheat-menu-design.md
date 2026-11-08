# Cheat menu — design

**Status:** draft for the user's approval (queue item 4, "GTA VCS Cheat Device style"). Decisions marked *(Claude)*;
open questions at the end. Research 2026-11-07 (worktree gt-cheat, read-only probes on a copy of the save).

## Intent

An in-game list of cheats the player toggles or fires, like the PSP "Cheat Device" for GTA VCS. Race-time cheats first;
cheats that touch the save only once their path is proven safe.

## What the game and host do (evidence)

- **F9 menu:** the lcs host's settings overlay, not ImGui. `host/play_backends.cpp` registers one
  `lcs::HostExtraPage{"MODS", count, row(index,char*), activate(index)}`; `host/mod_menu.cpp` lists mods only,
  keyboard only. The mod API v1 (`include/gt_mod/gt_mod_api.h`) cannot add rows, call guest functions outside a hook, or
  tick per vblank.
- **Race-time state:** `0x0894DD9C(world)` steps all cars each vblank; body state `K = word(car+8)`, velocity `+0x6CC..`,
  speed `+0x6C4`; player car predicate as in the reset-to-track spec. Driver entry mode tells AI from player.
- **Cars:** `MCarCollection::addCar` (`0x0881CDC4`, native, 3rd arg = count > 0) is the game's own path; the collection's
  record array at `+0x30` holds owned counts (low nibble) per car type (FACTS). Where the title-screen all-cars code is
  checked: not found (likely Adhoc script).
- **Money:** no native accessor; the profile is Adhoc-owned and the save is scrambled (`func_08C4FE98`). A u64 999,999,999
  appears at two heap addresses (`0x08F6AC00`, `0x09069E3C`, same trailing words): probably live copy + mirror, unproven.
- **Power/weight:** spec data under `CarParameter+0x4C8`; the field the physics step reads is not found yet.

## Proposal

1. **Mod API v2 (host change, part of this spec)** *(Claude)*: `add_menu_row(id, label_fn, activate_fn)` so a mod adds rows
   to the F9 page (Left/Right/Enter); `on_vblank` tick; `call_guest(addr, args…)` allowed only from that main-thread tick.
   Keyboard first; a pad chord to open the menu later.
2. **Mod `cheats` v1** *(Claude)*:
   - Slow motion (scale the player's and AI cars' step through velocity scaling; 0.5×/0.25×) — race only.
   - Freeze AI (AI cars skip their step) — race only.
   - Unlock all cars, through `MCarCollection::addCar` for each car code (`MSpecDB::getCarCodeByIndex`,
     `getCarLabelCount`), from the garage/menu, never mid-race; takes effect in the profile, saved by the game's own save.
3. **Later (v2), after probes:** money (prove the path: second balance, find the script object), power/weight multipliers,
   unlock tracks/events.

## Testing

- Unit: row registration, value cycling, call_guest refused outside the tick.
- In game: slow motion halves distance per 600 steps; freeze AI keeps AI positions constant; all-cars → `hasCar` true for
  every code, save → reload → still owned (on a save copy, `GT_SAVEDATA_DIR`). Mod off → frames byte-identical. Sabotage:
  a no-op cheat must fail each check.
- Differential gate + boot check for the host API change.

## Non-goals

Online/adhoc play, editing the save file offline, PS2-style memory-code lists.

## Open questions (the user's)

1. OK to extend the mod API (host change) with menu rows, a vblank tick and `call_guest`?
2. Which cheats matter most to you? (money, all cars, slow motion, freeze AI, power, tracks…)
3. Should cheats that touch the save make a backup copy of the save first (proposed: yes, always)?
4. Pad access to the menu needed, or keyboard is enough?

## Answers (user, 2026-11-07)

Approved: mod API v2 (menu rows, vblank tick, `call_guest`); **all** cheats wanted, money included (so the money probe runs
first, and money ships only once its path is proven); every cheat that touches the save makes a backup copy first; the menu
must work with the **pad** too, not only the keyboard.

## Implementation note (2026-11-08, piece 1)

The spec's "velocity scaling" (slow motion) and "AI cars skip their step" (freeze) were not built literally: the world step is a fixed step with no dt to scale, and a hook cannot
observe the step's result in `--decomp off` (FACTS "Cheats"). Both are the same operation instead: at each call's entry the mod writes `base + f * (state - base)` over every car's kinematic
state (f = 0.5 / 0.25, or 0 for a frozen AI car). That also keeps accelerations consistent, which a plain velocity multiplier does not. Time Trial ghosts: untested (a ghost may be treated like an AI car).
