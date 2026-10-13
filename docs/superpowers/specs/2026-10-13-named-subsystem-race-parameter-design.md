# First named subsystem: typed guest structs + RaceParameter (design)

Status: chosen by the user on 2026-10-13 ("haz el 4", option 4 of the post-batch-65 proposal), written
without a separate review round at the user's request ("continúa hasta que yo te diga lo contrario").

## Goal

Turn decompiled code from "4506 loose `func_XXXXXXXX` reading `word(rt, x + 0x30)`" into source a
modder can read and change: named functions, typed structs with named fields, one file per subsystem.
Two deliverables:

1. **Typed guest structs** — the interop the Phase 3 spec left open ("decided in the Phase 3 spec",
   `docs/agents/decomp-workflow.md`): how decompiled code describes and accesses a game object that
   lives in guest memory while recompiled code still shares it.
2. **RaceParameter**, the first subsystem done that way: the race settings the Adhoc scripts pass to a
   race (launch speed and position, boost, penalties, delayed start, grid). Every name comes from
   evidence (Adhoc native names, `config/symbol_addrs.txt`); everything else keeps an address-derived
   name.

Success:
- `include/gt/guest.hpp` unit-tested (CTest), no behaviour change.
- `include/gt/game/race_parameter.hpp`: the layout with each named field's evidence next to it.
- `src/game/race/race_parameter.cpp`: the RaceParameter engine functions (already decompiled ones
  moved there and rewritten against the struct; new ones decompiled), registered and gated —
  differential gate 0 mismatches, sabotage detected — and named in `config/symbol_addrs.txt` with
  their evidence.
- A short modding note in the file header: where a mod would change race defaults.
- The recipe written into `docs/agents/decomp-workflow.md` so the next subsystems (CarParameter,
  SpecDB, Calendar) follow it.

## Typed guest structs (`include/gt/guest.hpp`)

Game objects stay in guest memory (32-bit PSP addresses, little-endian), shared with recompiled code.
Decompiled code describes them with **layout structs** and touches them through **views**:

```cpp
struct RaceParameter : gt::guest::Layout {
    GT_FIELD(std::uint8_t, use_launch_data, 0x60);         // evidence ...
    GT_ARRAY(std::uint8_t, launch_speed, 0x61, 1, 2);      // per player
    GT_ARRAY(gt::guest::be16, launch_position, 0x66, 2, 2);
};
gt::guest::View<RaceParameter> race(rt, address);
race.launch_speed(i) = value;            // store8(address + 0x61 + i, value)
std::uint16_t p = race.launch_position(i);  // big-endian load
```

- `View<T>` = (guest memory, guest address). `GT_FIELD(type, name, offset)` and
  `GT_ARRAY(type, name, offset, stride, count)` generate accessors returning a small proxy that loads
  on conversion and stores on assignment, through `GuestMemory` (so endianness and bounds behave as the
  recompiled code's accesses do). `be16`/`be32` mark the big-endian fields GT keeps (Adhoc data);
  signed and float types are plain `std::int8_t`/`float` etc.
- A field accessor of a struct type returns a `View` of it (nested structs); `View::address()` gives
  the raw address for `call_guest`.
- Nothing assumes host pointers; it compiles the same on every platform (the portability rule).
- Unknown fields are simply not declared; a field gets a name only with evidence, otherwise
  `unk_XX` if code must touch it.

Rejected: a fixed low host mapping or a 32-bit host build (platform-specific, and the recompiled code
already owns `GuestMemory`); raw offset constants per function (what we have; unreadable).

## RaceParameter

- **Evidence chain.** `MRaceParameter::setLaunchSpeed` (native, name from GT's own registration) calls
  `kRaceParameterCore` to get the engine object and then the engine function `func_088D4E3C(core, i,
  v)`, which stores `v` at `+0x61 + i` of the object at `core+0`. So `func_088D4E3C` is
  `RaceParameter::setLaunchSpeed` and `+0x61` is `launch_speed[]`. Each engine function and field is
  named only with such a chain (native → engine function → offset), written next to it.
- **Scope.** The engine functions reachable from the 25 `MRaceParameter::*` natives, one level deep
  (the natives themselves stay in `src/game/adhoc`), plus the struct they access. Functions already
  decompiled in `src/game/unsorted/hot_leaves.cpp` move to `src/game/race/race_parameter.cpp`
  (`GT_DECOMP` registration moves with them; behaviour unchanged); missing ones are decompiled.
- **Names in code.** `namespace gt::game::race_parameter { void set_launch_speed(Runtime&, Context&); }`
  with the original's address in the registration and in `config/symbol_addrs.txt`
  (`RaceParameter::setLaunchSpeed = 0x088D4E3C; // evidence`).

## Testing

- `host/tests/guest_view_test.cpp` (CTest `guest_view`), written first: field/array loads and stores at
  the right addresses, sizes, signedness, big-endian swap, float bits, nested views — over a real
  `psprecomp::GuestMemory`.
- RaceParameter functions: the usual gate — fast gate 0 mismatches, sabotage detected for every moved
  or new function, full gate on the three routes before the PR (moved functions must stay equal; a
  rewrite against the struct is a rewrite, so it is re-proven).

## Out of scope

- Other subsystems (next, same recipe).
- Renaming the natives' internal helpers that RaceParameter doesn't need.
- Replacing `word()`/offset code elsewhere — only code that is moved into a named subsystem is
  rewritten against layouts.
