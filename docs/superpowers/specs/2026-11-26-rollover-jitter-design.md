# Rollover: the car shakes while the simulation drives it — design

Status: approved by the user 2026-11-26 ("vale, sigue"); phase 1 corrected the same day after reading FACTS (a draw-time write does not show). Follows `2026-11-18-rollover-physics-design.md`
(rollover 0.5.0, `style = "realistic"`).

## Problem (user report 2026-11-26: "la física de rollovers tiembla mucho")

While the rigid-body simulation drives the player's car, the car shakes. Measured on the `rollover` route
(`tools/mod_rollover_check.sh --sim --decomp on`, gt-mods `0efc6ea`, 118 simulated steps):

- **Every step lands off the written pose.** At the start of each step the car is `kd` = 0.27 m on average (max 0.52 m)
  from the pose the mod wrote, and this happens in 109 of the 118 steps.
- **The game moves it after the mod writes it.** The mod writes the simulated pose (position, axes, velocity, rates)
  before the game's own step, and that step moves the car again:
  - **Airborne:** `kd / (|v| dt)` = 1.02. The game integrates the velocity once more.
  - **Touching the ground:** 1.73. The game's suspension also pushes the car.
- **What is drawn.** The game draws the moved pose, and the next step snaps the car back to the simulation, so the car
  shakes by about one step of motion per frame.
- **Why the check passed.** The check accepted this, because its `pose` judge allows "within a step of motion".

The rotation shows the same pattern: `kup` is non-zero in the same steps.

## Design

### Phase 1 — write the simulation's pose after the game's step (mod only, no host change)

A write at `GT_HOOK_CAR_DRAW` does not work. FACTS "Rollover physics research", observation 1, shows that the camera
and the car's draw matrix are built from the state after the world step, before the car-draw hook runs, and that a
draw-time write leaves the frame byte-identical. The mod's world-step hook already calls `call_original` (the game's
step) and can act after it returns, so while the simulation drives, the mod's world-step hook works like this:

1. **Before `call_original`:** as today, it simulates one step and writes the simulated pose, so the game's step starts
   from it and its contacts and suspension see the right pose.
2. **Right after `call_original`:**
   - It reads what the game's step did. That reading is the velocity change the barrier-hit detection uses (`sim_hit_dv`),
     and the `kd`/`kup` the log measures. Both move here from the start of the next step.
   - Then it writes the simulated pose again, with the same `car_set_pose`.

The state the camera and the renderer read is then the simulation's pose. The next step starts from it as well
(`kd` about 0).

**Expected:** a gap of 0 m between the pose after the step and the simulated one, so the drawn car is the simulated car.

### Phase 2 — only if phase 1 leaves a visible effect (host API v8, research first)

The game's step still moves the car between draws, so anything computed inside that step from the car's pose can
still jitter. That includes the chase camera if it follows the stepped pose, the wheel contact records and the sound.

If phase 1 leaves such a visible jitter, the host would expose `car_hold(car, on)`. It skips the game's integration and
suspension for that car while the simulation drives. It is a hook on the per-car step, researched like `tilt_free`
(the dormant "overturned car" machine at `0x08956930` already skips the force pipeline: FACTS "Rollovers"). It is not
designed before phase 1 is measured.

## Checks (in-game, `tools/mod_rollover_check.sh --sim`)

- **New judge `poststep`.** While the simulation drives, the pose the next step starts from (`kd`) equals the
  simulated pose: position within 1 cm and up vector within 0.001.
  - **Sabotage `--sabotage-nopost`:** no post-step write. The judge must fail, with `kd` near today's 0.27 m.
- **Existing judges.** The `barrier` and `pose` judges and the others keep passing.
- **Frames.** A frame-to-frame comparison over the simulated steps measures the car's on-screen displacement against
  the simulation's motion, as the visible-jitter number before and after.
- **Coexistence.** Every mod is enabled (`--others`), because hooks chain per address.

## Out of scope

- The `arcade` style. Separately, its route run crashed under `--decomp on` on 2026-11-26 (a guest access outside RAM
  at the end of the route), and that is being bisected as a decompiled-code bug, not this design.
- Tuning the mass, friction or bounce.
