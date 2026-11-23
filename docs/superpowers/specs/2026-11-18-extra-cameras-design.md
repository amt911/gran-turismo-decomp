# Extra cameras mod — design

**Status:** approved by the user on 2026-11-18 ("me parece bien, continua"). User request 2026-11-18 (idea: bonnet, wheel and helicopter cameras in replays). Decisions *(Claude)*.

## Goals

- **New race views,** added to the game's SELECT cycle as extra entries after the game's own four (chase, bumper, cockpit, hood), or selected from the cheat device:
  - **Wheel cam:** fixed to the body at the front-left wheel, low, looking forward.
  - **Roof cam:** above the roof, looking forward and slightly down.
  - **TV cam:** a trackside camera that follows the car from fixed points.
- **Replays:** a helicopter camera that follows the car from above and behind with smoothing, cycled with the replay's camera button.
- Uses the named camera accessors the SDK spec planned ("camera view matrix, read and write", `2026-11-10-named-mod-sdk-design.md`). orbit-camera migrates to them too, so it stops using raw addresses.

## Non-goals

- Free-fly camera editing.
- Photo mode.
- Changing the game's own replay director.

## Approach *(Claude)*

- **SDK:**
  - `camera_get(&eye, &basis)` and `camera_set(eye, basis)`, valid in a new `GT_HOOK_CAMERA` point (the view load the orbit-camera hooks today: 0x08B568DC / 0x08B59210 behind names);
  - a `replay_active()` predicate.
- **Views:** offsets in the car body frame from `car_state`. The helicopter camera is a critically damped spring to a point above and behind the car.
- **Near plane:** check that close views (wheel) don't clip the car body; adjust the near plane if the SDK can.

## Step 0 — research

1. Run orbit-camera's hooks during a replay: is the replay camera the same object? Do cuts go through the hook?
2. Replay-state detection with evidence (role 3 / `+0xA8`, measured on several replays).

## Testing

- **In game:**
  - each view puts the eye at the expected body-frame offset (from the WORLD matrices of the frame);
  - the replay helicopter camera stays above the car;
  - with no extra view selected, frames are byte-identical.
- **Sabotage:** the offset is ignored, so the eye-position check fails.
