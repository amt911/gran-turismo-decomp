# Mod draw API (overlay, scene grade, extra car pass) — design

**Status:** draft, waiting for the user's approval. This is the shared host piece behind three mods the user asked for on 2026-11-18 ("Me interesan todos"): telemetry, weather / time of day and the ghost in any mode. Decisions marked *(Claude)*.

## Why

Mods can read and write game state, but they cannot draw. Today the host's F9/F10 overlay and the cheat device panel are host-only (lcs `host_settings`, `lcs_host_style`). Telemetry needs text and graphs, weather needs a scene tint and rain, and the ghost needs a second, translucent car. All three have to go through named host calls (`docs/superpowers/specs/2026-11-10-named-mod-sdk-design.md`), never GE words or raw addresses.

## Goals (API 8)

1. **2D overlay:**
   - `draw_text(x, y, size, rgba, text)`, `draw_rect(x, y, w, h, rgba)` and `draw_line(x0, y0, x1, y1, width, rgba)`, in a 480×272 logical space scaled to the output.
   - Called from a mod's `on_overlay(self, fn)` callback, once per presented frame.
   - Rasterized on the host with the patch 0070 picture thread and rasterizer (same font, sharp at any resolution).
   - Drawn after the game frame, under the menus.
2. **Scene grade:**
   - `scene_grade(r, g, b, brightness, saturation)` applies to the game image only (not the HUD) where possible.
   - First version: a full-frame grade in the present pass (Vulkan shader, SDL texture colour mod), with the HUD tinted too.
   - Research step 2 decides whether the game's HUD draws can be excluded (they come after the 3D scene in the display list).
3. **Extra car pass:**
   - `draw_car_pose(car, position, basis, alpha)` draws the car's own model again at another pose in the same frame.
   - Translucency uses the game's ghost path (stencil EQUAL ref 0x80 screen-door, FINDINGS) or a host blend.
   - Implemented by re-issuing the car-draw leaf with a different world matrix (research step 3).

## Non-goals

- 3D drawing for mods beyond whole car models.
- Mods writing GE lists.
- Fonts other than the host UI font.

## Approach *(Claude)*

- **Overlay commands:** each frame, mods append to a host-side command buffer (thread-safe, cleared per frame). The overlay picture is rasterized from the buffer on the patch 0070 worker and uploaded only when it changes; telemetry changes every frame, which is acceptable at overlay size.
- **Scene grade:** a uniform in the present pass, neutral by default, so it is byte-identical when no mod sets it. The software GE backend applies the same grade on the final framebuffer, keeping the GPU and software paths comparable.
- **Extra car:**
  - At the car-draw hook, the host records the model's draw-list span (the deformation mod already finds it with `draw_car_model` / `model_runs`) and replays it with the new WORLD matrix: view × the new model matrix, since GT folds the view into WORLD on the CPU (FACTS).
  - Translucency comes from the stencil ref the game uses for its ghost.

## Step 0 — research (FACTS before code)

1. The overlay thread: draw from the present thread using the 0070 worker; measure the cost at 60 Hz with about 200 primitives.
2. The HUD split: find in the F8 GE trace where the 3D scene ends and the HUD begins (a projection switch or depth-test off) so the grade can stop there.
3. Car replay: re-issue a car's draw span with a shifted WORLD matrix in a probe and compare frames. Check the stencil ghost path for translucency.

## Testing

- **Host unit:** command buffer, rasterization of each primitive (pixel checks), a neutral grade that is identity.
- **In game:**
  - a probe mod drawing text, a rectangle and a line shows them at the expected screen spots (frame dump);
  - grade (0.5, 0.5, 0.5) halves the 3D pixels (within tolerance);
  - `draw_car_pose` with an offset shows a second car exactly the offset away (screen box).
- **Sabotages:** no overlay flush (text check fails); grade ignored (darkening check fails).
- **Byte-identical frames without any draw call:** `mods_together` with the new API linked but unused.
