# Car scratches (paint damage) — design

**Status:** draft, waiting for the user's approval. The user asked for it on 2026-11-16 ("Quiero también pintar arañazos si
puede ser, ya lo probaré yo") and skipped the design questions ("Saltatelas porfa"), so every decision below marked
*(Claude)* is Claude's call. The user will test it in game.

## Why

Deformation 0.2.0 (gt-mods `v2026.11.15.2`) moves the body vertices, but from the chase camera the dent changes only about
1.4k pixels: the game's lighting ignores the normals, so a crushed panel looks almost the same as a flat one. Paint damage
changes *colour* where the car was hit, so it is what makes a crash visible.

## Goals

- After an impact, the player car shows **scratches and scuffs on the paint around the hit point**: lighter streaks of
  bare-metal or primer colour plus a darker scuffed area. The damage grows with impact strength and adds up during the race.
- It works **together with the crush** in the `deformation` mod. Both use the same impact and contact point.
- It follows the deformation rules the user already chose (2026-11-14):
  - the player car only (AI cars of the same model stay clean);
  - the damage lasts for the race only (restored on race end or when the mod is switched off; nothing is saved with the car).
- Mod off → byte-identical to no mod, as with every other mod.

## Non-goals

- Scratches on glass, lights, wheels or tyres.
- Damage stored in the save or the garage.
- Damage on AI cars.
- Physics changes.
- Decals with authored art. The marks are generated procedurally, so no new game data is needed and no image files go into
  the repo.

## Anti-goals (failure even if it "works")

- Mods poke raw guest addresses. Everything goes through named SDK calls (`docs/superpowers/specs/2026-11-10-named-mod-sdk-design.md`).
- Frame rate drops on the laptop. The re-upload or patch cost must be paid only on frames where new damage is added,
  never every frame.
- AI cars of the same model get scratched.
- The original texture is changed in place without a copy.

## Facts we start from (FACTS / mesh_blob)

- Car model parts are GE "blobs" (BASE, VADR, VTYPE, PRIM, RET). The player car's container entries CALL texture or
  material blocks (`0x090E4xxx`, `0x090D4xxx`; FACTS § player container).
- There are two vertex formats on the body:
  - `0x122`: u16 UV + s8 normal + s16 position, so it is **textured**.
  - `0x130`: 16-bit **5650 vertex colour** + s8 normal + s16 position, so it has **no UV**.
- `0x118` / `0x11A` carry 4444 colour.
- The SDK (API v5) already has these calls, which the deformation mod uses: `model_runs`, `run_vertex`,
  `run_set_vertex`, `mesh_copy`, `draw_mesh_redirect`, `draw_matrix` and `car_impact` (whose contact point is in the body frame).
- The host decodes vertices from RAM every draw, so a vertex edit shows on the next frame. Textures go through the
  lcs-recomp texture cache (patches `0027` whole-texture signature and `0058` slot reuse). Whether an edit to texture
  bytes in guest memory is picked up, and at what cost, is **not yet measured**.

## Approach *(Claude)*

There are two damage channels, chosen per run by its vertex format.

1. **Vertex-coloured runs (`0x130`, `0x118`, `0x11A`).** Damage is drawn into the vertex colour of the run's copy (the
   player car already draws from copied blobs, so AI cars are untouched). Vertices near the hit are blended towards a
   scuff colour (darker, less saturated). A few hash-chosen vertices go towards a light "bare metal" colour, which gives
   scratch specks. This is coarse, one colour per vertex, but cheap and needs no texture work.
2. **Textured runs (`0x122`).** The texture is copied on write:
   - On the first damage, the run's texture (found through its material block: TBP0, format, size and, if paletted,
     CLUT) is copied to guest memory the mod owns.
   - The copied material block is pointed at the copy, so AI cars keep the original texture.
   - Scratches are painted into the copy: short streaks at random angles, generated from a seed and placed at the UVs of
     the vertices the dent touched, plus a scuffed area.
   - If the texture is paletted (CLUT4/CLUT8), painting picks the nearest palette entry to the wanted colour. The
     palette itself is never changed, because other texels share it.

The marks are seeded from the hit position, like the crumple noise in 0.2.0, so a replay draws the same marks.

### SDK additions (named, in the host; mods never see GE words)

| Call | What it does |
|---|---|
| `run_color` / `run_set_color(blob, run, index, rgba)` | Vertex colour of a run, converted from and to the run's own format. |
| `run_uv(blob, run, index, uv[2])` | Texture coordinate of a vertex, in texels of its texture. |
| `run_texture(blob, run, GtTextureInfo *out)` | Width, height, format and whether it is paletted, for the texture a run draws with. |
| `texture_copy(blob, run, *handle)` | Copy-on-write of that texture into the player's copied blob. |
| `texture_paint(handle, x, y, rgba, alpha)` | Writes one texel, with palette matching. The host batches the writes. |
| `texture_commit(handle)` | Tells the renderer the copy changed, so it is re-uploaded **once**. |
| `texture_release(handle)` | Frees the copy and points the blob back at the original. |

The exact signatures are left to the plan. The research step below may move work between the host and the SDK.

## Step 1 — research (before any code; result recorded in `docs/FACTS.md`)

1. Which runs of the player car's body show the paint: `0x122` textured, `0x130` vertex-coloured, or both. Measure by
   recolouring each run once and counting changed pixels.
2. For the textured ones:
   - where TBP0, TPSM, TSIZE and CLUT are set (inside the CALLed material block or earlier);
   - whether the texture lives in RAM or VRAM;
   - whether AI cars of the same model share it.
3. Whether the lcs texture cache notices changed texture bytes, and what one re-upload costs on the laptop-class path.
   If it does not notice, `texture_commit` needs a host hook that drops that cache entry.

**Fallback *(Claude)*:** if textures cannot be copied cleanly (shared across models, or VRAM-resident with no RAM
source), ship channel 1 only (vertex colours) and record why.

## Behaviour and settings *(Claude)*

- Part of the `deformation` mod (one toggle, F9 / cheat-device row). New setting `scratches = true`. Defaults:
  - damage radius = the dent radius (2.0);
  - scuff strength proportional to `dv_magnitude` and capped;
  - up to 12 streaks per hit;
  - per-run damage capped so the car never turns fully grey.
- Restore on race end or toggle off: the colours are put back and the texture copies released, following the existing
  `on_switch_off` path.

## Testing

- **Unit (host):**
  - colour conversion 5650 ⇄ RGBA, 4444 ⇄ RGBA;
  - UV decode;
  - palette nearest-match;
  - copy, then texture paint, then release restores the original bytes.
- **Unit (mod):** streak placement is deterministic for a seed; strength 0 paints nothing; the cap holds.
- **In game:** `tools/mod_deformation_check.sh` gains a colour check on the same route and frame as the dent check. It
  needs:
  - a change in hue or brightness inside the player car's screen box after the hit, many more changed pixels than the
    1.4k the dent alone gives;
  - an AI car of the same model unchanged;
  - mod off → byte-identical;
  - `--decomp off` and `on`.
- **Sabotage:** paint alpha 0 must fail the "changed after hit" check.
- **Perf:** frame-time trace on the hit route. No frame-time spike beyond one frame per hit.
- Images go to the user privately and are never committed.

## Left to the builder

- Streak shape and colours, within "lighter streaks plus darker scuff".
- Batching of texel writes.
- How the copied texture is allocated (same allocator as `mesh_copy`).
