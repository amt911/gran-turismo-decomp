# Draw distance mod — design

**Status:** approved by the user (2026-11-06) and implemented (2026-11-07; gt-mods `draw-distance`, check `tools/mod_drawdist_check.sh`).
The user asked for a draw-distance mod (2026-11-04, queue item 2). Decisions marked *(Claude)* are Claude's.

## Intent

In races the far scenery is cut off at a fixed distance: buildings, hills and grandstands pop in. A gt-mods mod
(`draw-distance`), loaded by the mod loader and switchable in the F9 menu, pushes that cut-off further away. No game code changes.

## What the game does (evidence: recomp/analysis/drawdist/findings.md, 2026-11-06)

- The race world projection gets its depth range from `func_08AF4B4C(matrix, near, far)`, called by `func_0890E41C(self, ...)`
  with near = `self+4` (~1.3) and far = `self+0xC` (**1200**). `self` is the scene's fog/sky settings object, filled per scene
  by `func_089002E8` (scene data, not a constant in the binary).
- The game does **not** cull objects by that far plane: one race frame's GE list is identical (7643 draws, 45823 vertices) with
  far 1200 and 8000. The geometry beyond 1200 is already submitted and only clipped. Raising far shows it without extra CPU work
  in the game; the GPU draws more pixels.
- Measured: far 8000 shows much more scenery, far 300 removes it, so `self+0xC` is the live knob. Caveat: at 8000 the frame
  composition looked different (grandstand smaller) — not understood yet; must be explained or avoided before shipping.
- Race fog runs 50..5800 and changing it changed no frame (fog seems off or ignored in races): no fog matching needed now.
- Separate CPU distance culling or geometry LOD: not found (texture LOD only). Out of scope for v1.

## Behaviour

- ~~Pre-hook `0x0890E41C`: multiply `self+0xC` for the call, restore after.~~ **Superseded (2026-11-07, measured):** `call_original`
  returns before the rest of `0x0890E41C` runs (its call to `0x08AF4B4C` ran after the hook's restore: no frame changed). The hook is
  on **`0x08AF4B4C`** (a leaf; return addresses `0x0890E4B4` / `0x0890E4D0` = the two paths of `0x0890E41C`): after the original wrote
  m10/m14, recover near/far from them and rewrite them for `far * far_scale`. Scene data is never written, so there is nothing to restore
  and toggling off in the F9 menu takes effect on the next call. The reflection pass (caller `0x0889C014`, far 100) is not touched.
- `far_scale` in `mod.toml`, default **2.5** (1200 → 3000) *(Claude: a moderate default; 8000 changed the composition)*; range
  1.0–6.0, clamped. 1.0 = identical frames to without the mod.
- Only the race world pass: the sky pass (near ~20–33, far ~30000) and the car reflection pass (0.1/100) use other paths and
  are not touched (verified by frames: sky identical).
- Toggling off in the F9 menu mid-race restores the original far on the next frame.

## Testing

- Unit: scale/clamp/restore logic, settings parsing.
- In-game (`tools/mod_drawdist_check.sh`, frames, the chase route, `--decomp off` and `on`): with `far_scale` 2.5 the 3D area
  differs from without-mod frames only beyond the old far plane (a depth-based check: pixels whose depth was at the far plane);
  the HUD rows and the car's screen box identical (composition unchanged); `far_scale` 1.0 → byte-identical frames.
  Sabotage: a mod that also changes near (self+4) must fail the composition check.
- Laptop (power-saver): `GT_FRAME_LOG` pacing with and without the mod, same route — the extra GPU pixels must not cost
  frames (user's target: power-saver runs well).

## As built (2026-11-07)

- **The depth-based check became row and pixel thresholds.** The harness dumps colour frames only, so "pixels whose depth was at the far
  plane" is not available; `tools/mod_drawdist_check.py` instead requires: scale 1.0 byte-identical; at 2.5 at least 30 % of the race
  frames change (far scenery is in view on ~50 % of the route's frames), at most 5 % of the 3D area on any frame (measured worst
  0.1 %), at most 0.1 % of the near-field rows 190..232 (measured worst 0.01 %: depth speckle), HUD digits identical, the car in place.
  Sabotage: near x (6 x far_scale) and a milder near x 1.3 both fail it.
- **Depth-range limit.** The world pass is recognised by its depth range (near < 5, far 300..6000), because the caller's return
  address differs with `--decomp on`. A course whose far is outside 300..6000, or a perspective view with such a range, is left alone
  (one log line says so). **Replays and showroom/garage 3D views were not probed**: they may use the same builder and be scaled, or
  have another range and be skipped.
- **Laptop (power-saver) pacing was not measured.**

## Non-goals

Geometry LOD changes, adding objects the game does not submit, fog redesign, replays/menus.

## Settled before building (2026-11-06, Claude)

- The "composition change" at far 8000 was not one: frame 9000 of dd_base vs dd_pfar differs in 9 pixels of 130,560, scattered
  on the grandstand — depth-precision speckles (a larger far/near ratio spreads the depth buffer thinner). The check must
  therefore allow a small speckle count; the default 2.5 keeps the ratio far lower than 8000.
- Approved by the user 2026-11-06 ("me parece bien"), default `far_scale` 2.5.
