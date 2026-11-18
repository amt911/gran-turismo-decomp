# Best-lap ghost in any mode — design

**Status:** draft, waiting for approval. User request 2026-11-18 (idea: a ghost of your best lap in any mode, not only Time Trial). Decisions *(Claude)*.

## Goals

- In any race mode (arcade race, championship race, practice, drift), the player's best lap of the session is recorded and shown as a translucent ghost car on the next laps.
- The player car's pose is recorded every step (position, basis) with the lap's start and finish.
- The ghost is drawn with the mod draw API's `draw_car_pose(car, pose, alpha)`, using the game's ghost translucency where possible.
- Optional: best laps are saved per track and car in the mod's own folder (setting `persist = true`), and loaded when the same track and car are raced again.

## Non-goals

- Changing the game's Time Trial ghost.
- Ghosts of AI cars.
- Collisions with the ghost.

## Approach *(Claude)*

- **Lap detection:** a named host event, `lap_crossed(car)`, found in research step 1, or a fallback crossing test of the start line plane, recorded at the first lap.
- **Recording:** a ring of poses per lap (60 Hz × lap time; about 1–2 MB for a long lap). The best lap is kept by time.
- **Playback:** interpolate the pose at the current lap time and draw the car model at it (draw API).
- **Determinism and replays:** the ghost is visual only. In a replay it is drawn from the same recording.

## Step 0 — research

1. A lap start/finish event in the game (the lap counter field or a function), with evidence, behind a named SDK event.
2. `draw_car_pose` from the draw API spec (its research step 3) works for the player car's own model.

## Testing

- **In game:**
  - after lap 1, a second car appears at the lap-1 pose for the same lap time (screen box check against the recording);
  - it is translucent (pixel blend check);
  - mod off is byte-identical.
- **Sabotage:** drawing the current pose instead of the recorded one, so the "ghost lags the player" check fails.
