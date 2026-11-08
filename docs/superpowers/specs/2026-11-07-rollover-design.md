# Rollovers — research result and design

**Status:** draft for the user's approval (queue item 6, "rollover research"). Decisions marked *(Claude)*.

## Finding (research 2026-11-07, worktree gt-reset, runs e3a/e3b)

GT PSP cars can't roll over **by design**:
- `0x0893E0FC` clamps the two tilt angles (`K+0x6FC`, `K+0x700`) to ±0.611 rad (35°) and zeroes their rates, unless the word
  at `0x08CF2F08` (read by `0x0894CCB8`) is non-zero. Its setter `0x0894CC9C` has no caller in the binary and world init
  clears it: a dormant switch.
- `0x08956EF4` resets contact normals with y < 0.5 to straight up; suspension spring torque pulls the car back (a 6 rad/s
  roll kick peaked at 0.34 rad).
- Forcing roll rate `+0x6F4 = 12` for 40 steps: flag 0 → never past the clamp (up.z ≥ 0.82); **flag 1 → the car tumbled
  upside down (up.z −0.94) and drove on upright afterwards**, no crash, no roof contact. Flag 1 also wakes a dormant flip
  state machine in `0x08956930` (byte `+0x74B` 06→02, halfword `+0x74C` = 120 countdown).

So rollovers need (1) the flag set and (2) a roll impulse, because the springs restore the car otherwise.

## Proposed mod (`rollover`, opt-in, off by default) *(Claude)*

- Set the word at `0x08CF2F08` to 1 while the mod is active (restore 0 when toggled off).
- On a hard side impact or curb hit (lateral velocity change above a threshold in one step, tuned in game), add a roll-rate
  impulse proportional to it, capped. No impulse on normal driving.
- Pairs with reset-to-track (an upside-down car needs a recover button).

## Risks

The dormant flip state machine was never shipped behaviour: its countdown and what happens at 0 are not understood; there is
no roof collision (the car "drives on" after tumbling). First step: decompile/read `0x08956930` and `0x0894CCB8` paths
before building.

## Testing

Unit: impulse threshold/cap. In game: scripted wall hit → tilt past 35°, then the car recovers or stays upside down
without a crash, `--decomp off`/`on`; mod off → frames byte-identical; sabotage: impulse without the flag must not roll.

## Open questions (the user's)

1. Do you want rollovers at all, given the game never had them (no roof collision, unknown flip logic)?
2. Realistic (only big impacts) or arcade/fun (easier)?

## Answers (user, 2026-11-07)

Wanted ("sí, lo quiero"), in every mode. Built after reset-to-track (needs it to recover). First step stays: read
`0x08956930` / `0x0894CCB8` before tuning the impulse.
