# Named mod SDK: mods without raw guest addresses

**Status:** draft for the user's approval. The direction was approved on 2026-11-10 ("empieza por 2"). Decisions marked
*(Claude)* are mine.

## Why

Today every mod hard-codes guest addresses: hook targets (`0x0894DD9C` world step, `0x08948B78` input clamp, …), data
(`0x08F6AC00` balance, `world+4` car slots, `car+8` body, `+0x6C4` speed, …) and object tests (vtable `0x08CC1F60` =
human driver). The same lookups are copied across mods: `find_player_car` exists in 4 of them. At the end of the decomp the
emulated PSP memory goes away (AGENTS.md "End state"), and every one of those addresses dies with it.

## Goal

Mods use only named calls and named hook points from the SDK. The host implements them with the researched addresses
today, with the evidence written next to each one, and can reimplement them on typed C++ structs later without changing
any mod. A wrong address gets fixed in one place, and the mod source reads as what it does.

## Non-goals

- Changing what any mod does: the checks of every mod must pass unchanged, before and after its port.
- Removing the raw `read32`/`hook(address)` API. It stays for research and probes. A shipped mod in gt-mods must not use it
  (enforced by a check, below).

## Design *(Claude)*

- **API v3**, appended to `GtModApi` (the v1/v2 prefix is unchanged; a v3 function refuses a mod that declared `api < 3`).
- **Named hook points.** `hook_point(self, GT_HOOK_WORLD_STEP, fn)` and so on: an enum mapped to the addresses in the host.
  Initial set, from what the mods hook today:
  - `WORLD_STEP` (`0x0894DD9C`)
  - `INPUT_FILL` (`0x08896780`)
  - `INPUT_COPY` (`0x0892B4B0`)
  - `INPUT_CLAMP` (`0x08948B78`)
  - the camera hooks used by orbit-camera
  - the road and draw-distance hooks
  The full list is built from a grep of the gt-mods sources. Inside a hook, the context accessors stay as they are.
- **Named state, read and write:**
  - `money_get(ctx, &value)` and `money_set(ctx, value)`: the cell checks (high word 0, +8 word) live in the host.
  - `player_car(ctx, world)` returns an opaque car handle, or 0: the human, live, non-replay car. It replaces the four
    copies of `find_player_car`.
  - `car_count`, `car_at`, `car_is_ai`.
  - `car_state(ctx, car, GtCarState *out)` and `car_set_state(ctx, car, const GtCarState *)`. `GtCarState` holds the
    position, the basis rows, the velocity, the speed, the angular rates and the pedals: the fields the mods touch today.
  - The camera view matrix, read and write.
  - Handles are `uint32_t` today and stay opaque to mods.
- **Order:**
  1. Money: the cheats mod switches to `money_get`/`money_set`, and its check must still pass.
  2. Player car and car state: the pedals, cheats, reset and rollover mods.
  3. Hook points: every mod.
  4. Camera: orbit-camera and draw-distance.
  Each step is one host PR and one gt-mods PR, and every mod's in-game check passes before and after.
- **Guard:** a check in gt-mods CI fails when a mod's source has a hex literal in the guest range `0x08800000`–`0x0A000000`
  outside an allowlisted comment. That way no new raw address slips back in.
- **Docs:** `docs/MODS.md` documents each accessor next to its evidence (the FACTS entry).

## Testing

- Host unit tests for every accessor, against a guest memory image built in the test (money checks, the player-car
  predicate, state round trip). A sabotage of each check must fail its test.
- Every mod's existing in-game check, plus `mods_together_check`, before and after the port of that mod.
