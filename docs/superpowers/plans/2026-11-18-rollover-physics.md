# Realistic rollover physics — implementation plan

Spec: `docs/superpowers/specs/2026-11-18-rollover-physics-design.md` (approved 2026-11-18). Order fixed by the user: research, host SDK (API 7),
gt-mods `rollover` 0.5.0, in-game checks. Stop and report if research shows the approach cannot work (no improvised redesign).

## Task 0 — research (probe mods, throwaway; results to `docs/FACTS.md` "Rollover physics research (2026-11-18)")
1. Draw-point pose: a probe on `GT_HOOK_CAR_DRAW` writes a rolled/raised pose (a) before the step only, (b) before the step and before the draw; frames
   (`GT_FRAME_EVERY`, headless) compared. Pass = (b) shows the pose and (a) does not or shows the game's own step on top.
2. Road query at arbitrary points: `call_guest` of `fn = word(word(obj)+8)` from the vblank tick, at the wheel record's own (x,y) (must equal the record's
   height) and at offsets; frames with the query run vs not run must be identical (the FINDINGS note: "not frame-neutral from a tick").
   Fails -> `ground_height` is dropped (spec: only if safe), the mod keeps the wheel-record plane.
3. Rate <-> angular velocity: log the basis change per step against `+0x6E4..+0x6F8` while the game steers a free pose; decide what `car_set_pose`
   writes into the rates (zero is the proven choice: FACTS "Restoring a pose").
4. Barrier geometry: not found by the 2026-11-14 static pass (FACTS "Collision code"); no new Ghidra pass (not cheap). Phase 2 stays out.
5. Euler convention: from the asm of `0x0893C3E4` (read, then proven by a round trip in a host unit test).

## Task 1 — host SDK API 7 (branch `feat/sdk-rollover-physics`, worktree `../gt-roll-host`)
TDD, `host/tests/*` + ctest. Pure functions in `host/game_state.{hpp,cpp}`, the API glue in `host/mod_services.cpp`, the table in `include/gt_mod/gt_mod_api.h`
(`GT_MOD_API_VERSION_7`, the v7 functions refuse a mod that declared api < 7; tail of `GtModApi` only), `host/mod_hooks.cpp` table, `docs/MODS.md`.
- `car_dimensions(self, ctx, car, GtCarDimensions*)`: wheelbase, track, front/rear axle offsets, radius, axle midpoint (car record `word(car+0xC)`).
- `car_wheel_contacts(self, ctx, car, GtWheelContact out[4])`: x, y, ground height, surface type, `valid` (byte +0x2C == 0) from the wheel records.
- `car_set_pose(self, ctx, car, position, basis, velocity, angular_velocity)`: position, basis rows, Euler angles derived from the basis
  (`basis_to_euler`, pitch clamped inside +-90 deg), velocity, speed, the rates written for the angular velocity (research 3) — one call, mods never see offsets.
- `tilt_free(self, ctx, car, on)`: the named form of the flag trick (host-side hooks on `0x0894CCB8` / `0x08956DC8` / `0x08945F6C`, as the 0.4.0 mod does).
- `ground_height(...)`: only if research 2 says safe.
Tests: `basis_to_euler` round trip over a grid of angles including |pitch| near 90 deg; `car_set_pose` -> `car_state` round trip on a fake guest memory;
dimensions / contacts decoding; refusal of api < 7 callers.

## Task 2 — gt-mods `rollover` 0.5.0 (branch `feat/rollover-physics`, worktree `../gt-mods-rollphys`)
- `physics_core.{hpp,cpp}`: quaternion rigid body, box hull (8 corners + 4 edge midpoints), semi-implicit Euler at 1/60 s, sequential impulses (8 iterations),
  restitution (0 below 1 m/s), Coulomb friction, Baumgarte with a cap, angular damping. No game dependency. ctest: free fall, rest with no creep,
  corner drop tips over, slide stops by mu, bounce <= e^2 h, spin decays, quaternion <-> basis <-> Euler round trips (incl. +-90 deg pitch).
- `takeover.{hpp,cpp}` (pure): takeover / handback / reset-detect state machine from the spec's triggers; tests per trigger and no-trigger in normal driving.
- `mod.cpp`: `style = "realistic"` (default; API 7 named calls only) and `style = "arcade"` (0.4.0 path, kept as is). Realistic: pre-step hook writes
  the simulated pose with `car_set_pose` while `tilt_free` is on; the draw hook writes it again; barrier hit = `car_impact` dv as an impulse at the contact point;
  handback by the spec's rule; reset (position jump > 3 m) ends it. `mod.toml`: `api = 7`, new settings (`style`, `mass`, `restitution`, `friction`).
- Version 0.5.0, README, CI `SDK_REF` bump once the host PR is merged (user merges host PRs).

## Task 3 — in-game checks (`tools/mod_rollover_check.sh` + `.py` with pytest, host repo)
Modes on the existing `rollover` route plus a drop: barrier hit energy budget (peak height < v_z0^2/2g + 0.3 m, hull never > 0.05 m below ground, no
"peonza": |w_yaw| bounded while up.z < 0.3), bounce <= 1.2 e^2 h (a 2 m drop by a fake impulse), handback (throttle response vs a no-mod run), replay (pose within
0.5 m at matched steps). Sabotages that must FAIL: restitution 1.0, no friction, no draw-point write, takeover off. `--others` coexistence (deformation,
reset-to-track, cheats). `--seconds 300`; CPU lock for the real-time runs; job.sh/waitjob.sh for anything long.

## Verification and delivery
Host: ctest + pytest of the judge; gt-mods: ctest. Each task ends with the evidence (counts, PASS/FAIL lines). PRs are not merged by me; the gt-mods PR
waits for the host merge to bump `SDK_REF`.

## Outcome of Task 0 (2026-11-18; details and evidence in `docs/FACTS.md` "Rollover physics research")
1. **The draw-point write is not enough: it changes nothing on screen** (frames byte-identical with and without it; the renderer and the camera read K after the world step, before the car-draw hook). The plan
   adapts inside the spec's own alternative: the pose is written **before** the step with the clamp lifted (`tilt_free`), so the game's step leaves it almost as written; no car-draw write; the in-game check
   measures the step's perturbation (`kd`) and its sabotage is "the pose is never sent". No post-step hook was looked for.
2. `ground_height` is dropped (call_guest is tick-only and not frame-neutral: FINDINGS); the plane under the four wheels stays.
3. Rates: `car_set_pose` writes zero; `angular_velocity` is validated only.
4. Barrier geometry: no new pass; the game's `dv` is the input.
5. API deviation from the spec table: `car_dimensions` / `car_wheel_contacts` take `world` (the handle is K; the wheel records hang off the car object found through the world's slots, as `car_impact` does).
6. Takeover deviation: the hit trigger takes the car at the impulse (the simulation decides whether it tips) instead of waiting for tilt > 35 degrees, which the game's own physics never reaches after a barrier hit.
