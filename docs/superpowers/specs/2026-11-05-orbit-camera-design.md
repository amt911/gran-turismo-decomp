# Orbit camera mod (right stick) — design

**Status:** the user asked for "right stick moves the camera" (2026-11-04) and, after seeing the first frames, "also in third
person" and then "like Gran Turismo 7" (2026-11-05). Decisions marked *(Claude)* are Claude's; the user reviews them on the PR.

## Intent

While driving, the right stick swings the camera around the player's car (chase view) or looks around from the driver's seat
(bumper / cockpit / hood views), and springs back when released — as in Gran Turismo 7. A gt-mods mod
(`orbit-camera`), loaded by the mod loader; no game code changes.

## How the game draws the view (evidence: camera spikes 1–2, 2026-11-05; goes into docs/FACTS.md with the PR)

- The GE never receives a view matrix (VIEW uploads are identity). The game folds view × model on the CPU and uploads it per
  object as the WORLD matrix (cmd 0x3A/0x3B), with a perspective projection (`m[11] = -1`) for 3D and ortho for the HUD.
- `0x08B59508` (GU class method, vtable slot at `0x08CE0DD8`, reached through the dispatcher in `--decomp off` and `on`) uploads
  the world matrix: with `this = a0` (`0x08D6CE00`), if `read16(this+0x2E0+0xD18) & 1`, world = `word(this+0x2E0+0xD00)`,
  projection = `word(this+0x2E0+0xD04)`.
- Camera: `word(0x09413E00+0x1FA4)` (active camera of the race scene), `+0x10` = camera-to-world matrix (basis rows,
  translation = eye), row vectors. Player car world position: `word(car+8)+0x10` (x, y, z; Y-up form `(x, z, -y)`), car and
  player from the FACTS predicate (world = a0 of `0x0894DD9C`).
- The scene address `0x09413E00` is one run's value: the mod must take it from code (who stores it) or validate it; a fixed
  address is acceptable only with the evidence written next to it *(builder: find the pointer chain)*.

## Behaviour

- Pre-hook `0x08B59508`: for a perspective upload while the player's car is live, `M' = M · T(-p) · R_y(yaw) · R_x(pitch) · T(p)`
  with `p` the car's position in view space (`p[r] = dot(row_r, carpos - eye)`), call the original, restore the 16 floats. Ortho
  (HUD) uploads are never touched.
- Pivot *(Claude)*: the car in every view. In chase it orbits the car; in bumper/cockpit the pivot is ~at the eye, which
  degenerates into looking around from the seat — one rule, no view detection needed.
- Input — **Gran Turismo 7 style** (user, 2026-11-05: "como Gran Turismo 7, que orbite alrededor del coche, sin subir y bajar
  mucho, y que si se pulsa o se echa el stick para atrás, que se vea la cámara trasera"):
  - right stick X → orbit yaw around the car, target = stick direction: the stick's angle picks the look direction (right =
    +90°, left = -90°), so the camera is where the stick points *(Claude: GT7 maps the stick direction to the look direction)*;
  - stick pulled back (down past 0.7) **or clicked (R3)** → rear view, yaw 180°;
  - **no pitch from the stick** (fixed at the game's own height; a `pitch_deg` setting, default 0, for a small constant tilt);
  - dead zone 0.25; smoothed toward the target with a 0.08 s time constant, back to 0° on release *(Claude)*;
  - keyboard *(Claude)*: `look_left_keys` / `look_right_keys` / `look_back_keys` (defaults chosen by the builder after checking
    `config/gt_play.ini` bindings for free keys), same targets (-90 / +90 / 180).
  All settings in `mod.toml`.
- Only while driving: the player predicate holds and `0x0894DD9C` ran within the last 10 frames *(Claude)*. Not in replays
  (`entry+0xA8`), menus, garage, showroom. With no stick and no keys the mod writes nothing (identity: frames byte-identical to
  without the mod).

## Testing

- Unit: orbit matrix math (identity at 0°, pivot fixed point, ortho untouched), input smoothing, settings parsing.
- In-game (`tools/mod_camera_check.sh`, frames): the `chase.pad` route (race + three SELECT presses) with
  `GT_MOD_CAMERA_FAKE=<yaw>` (degrees, 180 = rear view); PASS = frames with the fake angle differ from the without-mod run in the 3D area and the
  car's screen position stays centred (car pixel box within tolerance), the HUD rows identical; fake 0 = identical to
  without-mod. Both `--decomp off` and `on`. Sabotage: pivot at the eye must fail the centred-car check.
- **Open bug to settle before merge:** in `--decomp on` the orbited car rendered black (spike 2). Compare an on-mode baseline
  first; if the mod causes it (lighting uses the world matrix), fix it (e.g. leave the matrices the lighting reads untouched).

## Non-goals

Free camera / photo mode, changing the chase distance, replay cameras, mouse look.
