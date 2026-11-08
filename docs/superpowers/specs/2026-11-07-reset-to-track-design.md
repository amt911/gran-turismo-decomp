# Reset to track — design

**Status:** draft for the user's approval (queue item 5). Decisions marked *(Claude)*; open questions at the end.

## Intent

One button puts the player's car back on the road, upright and facing the right way, like "recover car" in modern racing
games — for when it is stuck off-track or spun into a wall. A gt-mods mod (`reset-to-track`), F9-menu toggle, no game code
changes.

## What the game does (evidence: research 2026-11-07, worktree gt-reset, probe hook on 0x0894DD9C, runs e1–e3b, r1–r3)

- `0x0894DD9C(world)` steps every car once per step. Player car = `word(world+4) + i*0x2100 + 0x2010`, picked with the
  orbit-camera predicate (driver entry vtable `0x08CC1F60`, `+0xA8 == 0`, `+0x6C != 0`).
- Body state `K = word(car+8)`, Z-up: position `K+0x10/14/18`; Euler angles `+0x6FC`, `+0x700` (tilt), `+0x704` (yaw);
  basis rows `+0x1C/+0x28/+0x34` rebuilt from the angles by `0x0893C3E4` (called from `0x0893E0FC`); velocity `+0x6CC..`,
  speed `+0x6C4`; rates `+0x6E4/6E8` (Euler) and `+0x6F0/6F4/6F8` (body).
- Wheel contact records `word(car+0x10) + 0x38*i`: word `+0x2C` = 0 on asphalt, `0x0300` off track (one surface observed).
- No racing line / checkpoint list found; no in-game "recover car" routine to call (only license-test failure and pit logic).
- Test e1: pose saved at step 1000, restored at step 2300 with velocities and rates zeroed → the car restarted from rest and
  drove normally 700+ steps; ~10-step settle transient.

## Behaviour

- Hook `0x0894DD9C` (pre). Keep a ring of poses, one every ~15 steps, recorded only when the pose is "good": all four
  wheels on asphalt (`+0x2C == 0`), up vector z > 0.9, speed > ~5 m/s, `+0x74B` bit 2 clear *(Claude)*.
- On the button, restore the good pose from ~2–3 s before *(Claude: needs no track geometry and keeps the heading)*:
  write position, the three angles and the basis rows; zero velocity, speed and all rates; start from rest *(Claude)*.
- Cooldown ~1 s between resets; nothing happens if the ring is empty (e.g. right after the start) *(Claude; a nearest-
  asphalt search on the collision mesh is a possible v2)*.
- Off in replays and while paused; F9 toggle; `active` flag honoured.
- Button *(Claude, provisional)*: pad Select held 0.5 s during a race, keyboard R. To be confirmed by the user.

## Testing

- Unit: ring logic (only good poses stored, age selection), cooldown, settings parsing.
- In game (pad route that drives off the track at the end of the straight, then presses the button; `--decomp off` and
  `on`): after the reset all wheels report asphalt, up.z > 0.9, the car drives on for ≥ 600 steps; without pressing,
  frames byte-identical to no mod. Sabotage: restoring without zeroing the rates must fail (car not settled within N steps),
  restoring the newest pose (off track) must fail the asphalt check.

## Not yet known

Lap/timing consequences (does a reset count as a shortcut?), AI collisions at the restore point, replay recording.

## Open questions (the user's)

1. Button: pad combo (Select held? L+R?), keyboard key?
2. Restart from rest (tested) or keep some speed?
3. Allowed everywhere, or not in Time Trial / license tests (it could be a shortcut)?

## Approved (user, 2026-11-07)

Approved. Button: pad Select held 0.5 s (keyboard R); restart **from rest**; allowed everywhere, Time Trial included.

## Implementation notes (2026-11-07)

- Mod `reset-to-track` in gt-mods (`reset-to-track/`), settings `hold_ms` (500), `cooldown_ms` (1000), `rewind_ms` (2500, range 250..5500, the
  ring keeps 24 poses = 6 s), `reset_keys` (`R`). Button: first SDL controller's Select (BACK) or the key. A reset keeps the restored pose as the
  ring's only entry, so a second press (after the cooldown) goes back to the same place.
- Durations are in physics steps (60/s, `1000/60` ms each), not vblanks: the game runs ~0.48 steps per vblank (FACTS).
- Select is also the game's camera-view button. A guest function reads the pad buffer and is hookable (`0x08C7ABF0`, FACTS), so the mod hides a held Select
  while racing: a press shorter than `hold_ms` is replayed as a tap on release (the camera still cycles), a longer one resets and the view does not change.
- Review round: poses also need a low yaw rate and velocity along the heading; the ring is emptied on a position jump, a long gap and a new car, and poses
  older than 8 s are never restored; gear/rpm are not restored (not located), laps/timer are not rewound (documented in docs/MODS.md and the README).
- In-game check: `tools/mod_reset_check.sh` (route `reset`, the route's real SELECT hold is the button).
  Passes in `--decomp off` and `on`; sabotages (rates not zeroed, newest pose) and no-mod fail; the F9 toggle gives no reset.
