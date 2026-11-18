# Mechanical damage mod — design

**Status:** draft, waiting for approval. User request 2026-11-18 (idea: mechanical damage tied to deformation: less power or misaligned steering after strong hits). Decisions *(Claude)*.

## Goals

- Hard impacts (the same `car_impact` events deformation uses) add damage to three parts, by where the hit landed:
  - front: steering alignment and engine;
  - rear: drive;
  - sides: alignment.
- **Effects:**
  - **Engine damage:** the throttle is scaled down (up to −40 % at full damage) through `car_set_state`.
  - **Misaligned steering:** a small constant yaw-rate bias, so the car pulls to one side, up to about 0.06 rad/s.
  - **Severe damage:** a top-speed cap.
- Damage lasts for the race (like dents) and is shown in the cheat device as percentages, with a "repair" action.
- The thresholds are tuned so that normal racing contact does nothing.

## Non-goals

- Damage saved with the car.
- Damage to AI cars.
- Visual damage (deformation and scratches cover that).

## Approach *(Claude)*

- A pure damage model (unit-tested): an accumulator per part, fed with impact strength × direction weights.
- A world-step hook applies the effects each step.
- **Replays:** the impacts happen again in the replay re-simulation from the same states, so the damage rebuilds identically. If a check shows divergence, use the cheats recorder pattern.
- If research finds them, named SDK accessors for engine power and steering replace the throttle and yaw approximations.

## Step 0 — research

1. Do throttle writes from the world-step hook survive the game's pedal ramp (`car->constants+0x530`)? Is a yaw-rate bias stable in `--decomp off` / `on`? (A probe.)
2. Optional: locate the engine-power and steering parameters in the live car (Ghidra), for a later faithful version.

## Testing

- **Unit:** accumulation and direction weights; zero below the threshold.
- **In game (barrier route):**
  - after a front hit, top speed and acceleration drop by the expected fraction (distance per step at full throttle);
  - with no hit, frames are byte-identical to no mod;
  - the "repair" action restores normal acceleration.
- **Sabotage:** effect scale 0, so the speed-drop check fails.
