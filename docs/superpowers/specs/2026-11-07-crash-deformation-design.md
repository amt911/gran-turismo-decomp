# Crash deformation — design

**Status:** approved by the user on 2026-11-14 with the answers below (queue item 7, "deformation spike"). Decisions marked *(Claude)*.

## Finding (spike 2026-11-07, worktree gt-deform, probes recomp/analysis/deform/probe1, probe2)

Feasible as a gt-mods mod, no host change. Proof: displacing vertices of the player car's body in guest RAM at step 12000
crushed the rear in the next frames (side-by-side `probe2/png/cmp_019500.png`, 162 frames differ, runs deterministic).

- Each model is a static GE "blob" (BASE, VADR, VTYPE, PRIM, RET + its own vertices) called through the leaf
  `0x08B550A8(blob)`. Car vertex formats `0x122` (stride 14, position s16 at +8), `0x130` and `0x112` (stride 12, position
  at +6); s16 positions, scale in the world matrix; hardware transform, no skinning/morphing.
- Player car in chase view: two body blobs in the static heap (1973 and 1493 vertices in that run; addresses are per run —
  found at run time, never hard-coded); small parts (mirrors, wheels) are VRAM-cache copies; blobs stay unchanged all race.
- The host decodes vertices from RAM every draw (`PSPRECOMP_GE_VERTEX_CACHE=0` in gt_play, FINDINGS), so an edit shows on
  the next frame with no invalidation.
- Blobs are shared per model: an in-place edit dents every car of that model.
- No damage/collision function found yet. Impact can be read from the step hook `0x0894DD9C`: the player car's velocity
  change per step (`K+0x6CC..`) in the body frame (basis `+0x1C/28/34`) gives strength and direction for car-car and wall.

## Behaviour *(Claude)*

- Mod `deformation`, F9 toggle. On an impact above a threshold, push the body vertices near the impact point inwards,
  proportional to strength, capped (no inside-out); accumulate for the race; restore the original vertices on race end /
  toggle off.
- **Player car only (copy-on-write)**: copy its blobs to free guest memory, fix their absolute VADRs, redirect that car's
  `0x08B550A8` argument — so AI cars of the same model stay clean.
- Cosmetic only: the collision mesh and physics are unchanged. Normals left as they are in v1 (shading looked acceptable).
- Body blobs only in v1; glass, lights and wheels later.

## Testing

Unit: impact detection from Δv, dent falloff/cap, VADR rewrite of a copied blob. In game (`--decomp off`/`on`, a route with
a wall hit): frames identical to no mod before the hit, the player car's screen box changes after it, an AI car of the same
model unchanged; mod off → byte-identical. Sabotage: a dent with zero strength must fail "changed after hit".

## The user's decisions (2026-11-14)

1. **Player car only** (copy-on-write): AI cars of the same model stay intact.
2. **Dents last for the race only**: restored on race end / toggle off; nothing is saved with the car.
3. **Find the real collision code first** (Ghidra pass for exact contact points); **if nothing usable is found, fall back
   to the Δv approximation** described above. The research result (found / not found, with evidence) is recorded in
   `docs/FACTS.md` before the mod is built.
