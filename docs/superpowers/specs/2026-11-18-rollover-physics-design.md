# Realistic rollover physics — design

**Status:** shipped 2026-11-18 (host #665, gt-mods #32); the sections below were corrected to what shipped (see "What shipped differs" at the end). Approved by the user on 2026-11-18 ("me parece bien, continua"). The user asked for this on 2026-11-18: "me gustaría que se implementara una física para los rollovers, algo de 0 que sea realista, que si rebota el coche en la pista, que rebote de forma realista". They skip design questions as a rule, so every decision below marked *(Claude)* is Claude's call.

## Why

`rollover` 0.4.0 lifts the game's 35° tilt clamp and pushes a held roll rate on a barrier hit. It does not simulate a tumble. The game's own wheel and suspension pipeline keeps integrating the whole time, a "body box" pushes the roof out of the ground, and the pitch is pinned at 35° to avoid the gimbal lock ("peonza").

The user still sees unrealistic flips and bounces. A tumbling car needs its own rigid-body motion: gravity, the impacts of the body corners against the ground and barriers, restitution and friction. It should not be a scripted rate on top of a suspension model that thinks the car is on its wheels.

## Goals

- While the car is tumbling or airborne after a crash, its motion comes from a rigid-body simulation in the mod:
  - mass, inertia, gravity;
  - impulse contacts of the body's corners and edges with the track surface (restitution and Coulomb friction);
  - when the geometry is available, contacts with barriers.
- Bounces look physical: a car dropped on its roof loses most of its vertical speed, slides, and may roll over a corner. It does not hop high or spin like a top.
- The game keeps full control while the car is on its wheels, so normal driving is unchanged.
- The mod hands the car to the game smoothly once it is back on 3–4 wheels with low angular speed. If it lands on its roof or side, the car stays there until reset-to-track, as in real life. *(Claude)*
- Replays show the same tumble (deterministic, fixed step).
- Coexists with deformation (dents keep working from the same impacts), reset-to-track (a reset ends the simulation) and the other mods.

## Non-goals

- AI cars tumbling. Player car only, like deformation. *(Claude; can be extended later)*
- Damage to suspension or wheels.
- Exact real-world calibration per car model.
- Changing the game's tyre or suspension model for normal driving.

## Anti-goals

- Raw guest addresses in the mod: every new access goes through named SDK calls in the host.
- Hopping or "peonza" regressions, cars falling through the road, sudden 6 m jumps (the flip-machine trap, FACTS).
- Frame-time spikes. One rigid body with 8 to 12 contact points per step is microseconds.

## What the game gives us (FACTS / research 2026-11-18)

- **Physics step:** `0x0894DD9C(world)`, fixed at about 60 Hz of game time, one call per car. A pre-step hook exists (`GT_HOOK_WORLD_STEP`). There is no post-step point yet.
- **Body state K** (= `word(car+8)`), Z-up:
  - position `+0x10..18`;
  - basis rows forward `+0x1C`, right `+0x28`, up `+0x34`;
  - Euler angles pitch `+0x6FC`, roll `+0x700`, yaw `+0x704`;
  - velocity `+0x6CC..D4`, speed `+0x6C4`;
  - rates `+0x6E4/E8/F0/F4/F8`.
  - The body is rebuilt from the Euler angles by `0x0893C3E4`.
  - The rates are not clean body-axis components (FACTS 640).
- **Car record:** wheelbase, track, axle offsets and a radius. No mass, inertia or body box are known.
- **Ground:**
  - Each wheel's contact record holds the ground height under it. These stay valid when the car is upside down.
  - The road query (vertical ray) can be called with a scratch record. Whether that is safe at arbitrary points is untested.
  - No barrier or wall geometry has been found yet.
- **Traps:**
  - The flip machine `0x08956930`, if woken, sinks the car and then jumps it 6.5 m.
  - The ground clamp `0x0894E200` holds z above the mean ground height while the tilt flag is set.
  - The game keeps applying suspension forces while the car is upside down.

## Approach *(Claude)*

### 1. A pure rigid-body core (in the mod, unit-tested)

`physics_core.{hpp,cpp}`: a box hull with half-extents from the car record (length from the wheelbase plus overhangs, width from the track, height a tunable fraction). Its mass m, default 1300 kg, is a setting; the inertia tensor is the box's.

- **State:** position, a quaternion orientation, linear and angular velocity in world space.
- **Integration:** a fixed dt of 1/60 s, semi-implicit Euler, gravity 9.81 m/s².
- **Contacts:**
  - the 8 hull corners plus 4 edge midpoints against a ground surface;
  - sequential impulses, 8 iterations;
  - normal impulse with restitution e (0.15 by default; 0 below 1 m/s, to avoid jitter);
  - Coulomb friction μ (0.7 on asphalt, 0.5 on grass or gravel from the surface type);
  - Baumgarte position correction, with a cap.
- **Damping:** angular damping for rolling resistance and air drag.

This layer has no game dependency. Its tests (ctest) pin:
- free fall;
- a box dropped flat that comes to rest, with no creep;
- a box dropped on a corner that tips over;
- a box sliding to a stop by μ;
- a restitution bounce height of e² × the drop height;
- a spin that decays.

### 2. Takeover and handback in the game

- **Takeover.** It triggers when any of these hold:
  - a crash impulse above a threshold (the same `car_impact` event deformation uses) that leaves the car with roll or pitch beyond about 35° and growing;
  - all four wheels off the ground with an angular rate above a threshold;
  - up.z below 0.5.

  The rigid body then starts from the game's current state (position, basis converted to a quaternion, velocity, angular velocity estimated from the basis change over the last step), and the tilt flag stays lifted.
- **Each step while active (pre-step hook):**
  1. Integrate the rigid body one step.
  2. Write the result back: position, basis, the Euler angles derived from it, velocity, speed, and zero rates. The game's own step then sees a car exactly where the simulation put it.
  3. Before the car is drawn (the `GT_HOOK_CAR_DRAW` point), write the pose again, so what is shown is the simulation's pose and not the game's suspension step on top of it.

  Research step 1 confirms whether the draw point is enough, or whether a post-step host hook is needed.
- **Ground surface.** Each step, a plane is fitted through the four wheel ground heights, as today. With SDK `ground_height(x, y)` (research step 2), each hull contact point queries its own ground height and local slope instead.
- **Barriers.**
  - **Phase 1:** a barrier hit stays an impulse from the measured velocity change, applied to the rigid body at the contact point deformation already computes. The real impulse from the game's collision becomes the rigid body's input instead of a scripted roll rate.
  - **Phase 2:** real barrier geometry, if research finds it.
- **Handback.** It triggers when up.z is above 0.85, at least 3 hull-bottom contacts touch, and |ω| is below 0.5 rad/s for 10 steps. The mod then writes the final pose with zero rates, sets the tilt flag back to the game's, and lets the suspension settle. Reset-to-track's measured transient is about 10 steps.
- **Upside down at rest.** The car stays there. A reset (the reset-to-track action) ends the simulation; a position jump above 3 m is treated as a reset.
- **Determinism.** No wall-clock input. Triggers are state events, not step counts. In a replay the same events re-trigger from the same states. The cheats mod's replay recorder pattern is the fallback if a check shows divergence.

### 3. SDK additions (named, in the host)

| Call | Purpose |
|---|---|
| `car_dimensions(car, &out)` | Wheelbase, track, axle offsets, radius. |
| `car_wheel_contacts(car, out[4])` | Ground height and surface type per wheel. |
| `car_set_pose(car, position, basis, velocity, angular_velocity)` | Writes the body consistently: Euler angles from the basis, rates zeroed or converted. Mods never touch `+0x6FC`. |
| `tilt_free(car, on)` | The named form of today's flag trick. |
| `ground_height(x, y, z_from, &height, &surface)` | Only if research step 2 shows the road query is safe from a hook. |

`rollover` moves to API 7. The current `rollover` 0.4.0 behaviour stays available as `style = "arcade"`; the new simulation is `style = "realistic"`, the default. *(Claude)*

## Step 0 — research (recorded in FACTS before any code)

1. Is writing the pose at `GT_HOOK_CAR_DRAW` enough to show the simulated pose? Measure frames with a fixed pose written before the step only, against before the step plus before the draw.
2. Is the road query safe at arbitrary (x, y) from the world-step hook? Measure: compare with the wheel records' own heights, and check that the game's frames stay identical with the query run but its result ignored.
3. The angular velocity conversion: measure the game's rates against the basis change per step, so takeover and handback match exactly.
4. Barrier geometry: a short Ghidra pass on the collision code that produces the barrier velocity change. Phase 2 starts only if something usable is found.

## Testing

- **Unit (mod):** the rigid-body tests above, plus quaternion to basis to Euler round trips at many angles (including near ±90° pitch, which caused the "peonza").
- **Host unit:** the new SDK calls (car_set_pose round trip).
- **In game** (`tools/mod_rollover_check.sh`, new modes on the existing barrier route plus a new drop route):
  - **Barrier hit at about 90 km/h:** the takeover fires. The car tumbles or leans with an energy budget: peak height under (v_z0² / 2g) + 0.3 m, and no point of the hull more than 0.05 m below the ground. It comes to rest or is handed back, with no "peonza": |ω_yaw| stays bounded while up.z is below 0.3.
  - **Bounce realism:** a drop from about 2 m (forced with a fake impulse) rebounds to at most 1.2 × e² × the drop height.
  - **Handback:** once upright, normal driving resumes (the speed response to the throttle matches a no-mod run within tolerance).
  - **Replay:** the tumble re-plays the same (pose within 0.5 m at matched steps).
- **Sabotages that must fail:**
  - restitution 1.0 (the bounce check fails);
  - no friction (the slide-to-stop check fails);
  - no draw-point write (frames show the game's step);
  - takeover disabled (the tumble check fails).
- **Coexistence:** `--others` with deformation, reset-to-track and cheats on.

## Left to the builder

- The exact hull proportions and the default parameters, within the ranges above.
- The contact solver details (iteration count, slop).
- How the barrier impulse is turned into a contact (point and normal from deformation's contact estimate).


## What shipped differs from the text above (2026-11-18)

- **No car-draw write.** Research step 1 measured that a pose write at `GT_HOOK_CAR_DRAW` changes nothing on screen (frames byte-identical): the renderer and the camera read the car after the world step, before that hook. The pose is written **before** the step with the clamp lifted by `tilt_free`, so the game's step leaves it almost as written (rates zero); what is drawn is the simulation's pose plus one step of the game's own motion. No post-step hook was found or needed. Step 3 of "Each step while active" and the draw-point sabotage are replaced by "the pose is never sent" (`--sabotage-nopose`) and the `kd` pose check.
- **No `ground_height`.** `call_guest` is tick-only and the road query is not frame-neutral from a tick (FINDINGS); the ground is a plane through the four wheel records, frozen from takeover to handback/reset.
- **`car_dimensions` / `car_wheel_contacts` take `world`** as well as the car handle (like `car_impact`); `car_set_pose` zeroes the five rate fields and does not store `angular_velocity`; it refuses right-handed bases.
- **Takeover at the hit impulse**, not after tilt > 35 degrees and growing: the game's own physics never tips a car past the clamp after a barrier hit, so the simulation takes it at the impulse (`takeover_dv`) and decides itself whether it tips. Other triggers as written (flipped, airborne and spinning).
- **While simulating**, the game's own velocity change **along the ground** (above `sim_hit_dv`) is a barrier hit; the part along the ground normal is the wheels answering and is ignored. A position jump above 3 m or an up row 0.3 away from the written one ends the simulation (reset).
- **Hull floor** from the origin's height over the wheels' ground at takeover (0.35 m default), deterministic from the state, so live and replay agree.
- **Energy budget** in the barrier check is the total mechanical energy (never above the one right after the hit impulse), which includes the rotational part the literal "peak height" text ignores.
