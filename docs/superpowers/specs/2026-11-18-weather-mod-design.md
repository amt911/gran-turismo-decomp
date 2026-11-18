# Weather and time of day mod — design

**Status:** draft, waiting for approval. User request 2026-11-18 (idea: dynamic weather / time of day in a race, rain or a progressive sunset). Decisions *(Claude)*.

## Goals

- **Time of day:**
  - a race can be set to a light preset (noon / late afternoon / sunset / dusk), or a progressive mode where the light moves from one preset to the next over the race;
  - built on the scene grade of the mod draw API (colour, brightness, saturation), plus the game's own fog or light values if research finds per-track tables.
- **Rain (visual only):**
  - screen-space rain streaks and drops (overlay API), a darker, desaturated grade, and an optional lower far plane (draw-distance's approach);
  - grip reduction is a separate setting, off by default (a scaling of velocity changes in turns, like the cheats mod's state edits).
- Settings in the cheat device (Race page) and mod.toml.

## Non-goals

- Real wet-road reflections or puddles.
- Weather in menus.
- Changing the game's track lighting data on disk.

## Approach *(Claude)*

- **Phase 1:** grade presets plus progressive interpolation by race time (the world-step count).
- **Phase 2:** if research finds the game's per-track fog or light tables, drive them instead of a screen grade. The result is better: sky and fog shift, not only a tint.
- **Rain:** particles in screen space, seeded and deterministic, moving with the camera's yaw rate for parallax.

## Step 0 — research

1. The F8 GE trace on a race frame: fog commands, light and material state, the vertex colours of the track and sky draws. Is there a per-track lighting table in guest memory? (FACTS)
2. Whether lowering the far plane during rain looks right with draw-distance installed (they must coexist).

## Testing

- **In game:**
  - preset "dusk" lowers the mean brightness of the 3D area by the preset's factor (±5 %);
  - progressive mode changes monotonically over a race;
  - rain shows streaks (overlay pixels) and a desaturated grade;
  - mod off is byte-identical.
- **Sabotage:** grade not applied, so the darkening check fails.
