# Mod draw API (API 8) — implementation plan

Spec: `docs/superpowers/specs/2026-11-18-mod-draw-api-design.md` (approved 2026-11-18). Branch `feat/mod-draw-api`, TDD, one commit per part.
Version number: API 8 (API 7 is the rollover-physics SDK on `feat/sdk-rollover-physics`, unmerged): this branch appends the v8 functions
at the end of `GtModApi`; the loader here rejects `api = 7` until that branch lands, and whoever merges second resolves the table / loader lines.

## Design decisions (deviations from the spec's wording are listed)

- `on_overlay(self, fn)`: `fn(GtMod *self, GtModCtx *ctx)` runs on the vblank tick thread right after every mod's tick (ctx may
  read game state; the draw calls are valid only inside it). The calls append to a per-callback command list; at the end of the
  pass the list is handed (one lock, swap) to the lcs host. The spec said "once per presented frame": the game's vblank is the
  rate the game state changes at, and the present thread must not call into mods (it is not a guest tick). The list persists
  until the next pass replaces it; a pass with no draws clears it.
- Rasterizer and picture worker live in the lcs tree (patch 0071, next to 0070's worker): they reuse the UI font and the
  anti-aliased `Canvas` of `lcs_host_style.cpp`. The picture is rasterized at the size of the shown game rectangle (480x272 logical).
- Present: Vulkan `kPresentModDraw` layer (a fifth set) over the game rectangle, drawn before the F9/F10 panels (under the menus); SDL
  path: a texture over the same rectangle.
- `scene_grade(r, g, b, brightness, saturation)`: stored per mod (the product over the active mods), neutral = identity. Applied in
  the Vulkan present shader (push constants; `grade_on == 0` skips the math: byte-identical), in the SDL path by texture colour mod
  (saturation is not expressible there: documented), and on the CPU for frame dumps (`GT_FRAME_DUMP` writes the composite so checks see it).
- `draw_car_pose`: only if Step 0 item 3 shows the replay works.

## Steps

0. Research (FACTS with evidence): overlay cost (~200 primitives), where the HUD starts in the GE list, car draw span replay.
1. Command buffer + rasterizer (lcs patch 0071) + host `mod_draw` + API v8 `on_overlay/draw_text/draw_rect/draw_line`. Tests: pixel checks of each
   primitive, buffer limits, per-mod switch-off clears, the C header check. Present integration (Vulkan, SDL). Frame dump composite.
2. `scene_grade`: pure `apply_grade` (tests: identity byte-exact, 0.5 halves), shader + push constants, SDL colour mod, dump.
3. `draw_car_pose` (if research allows).
4. In-game probe mod (outside the repo build: tools/mod_draw_check.sh builds it from `tools/mod_draw_probe/`), frame checks, sabotages.
5. Docs (MODS.md, FACTS, README), PR.
