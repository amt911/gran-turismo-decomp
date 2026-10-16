# GE fast path: GT's common draw without the per-draw rebuilds

Status: **approved by the user on 2026-10-16** (chat: "el pr 214 me parece bien, continúa"). Plan: `docs/superpowers/plans/2026-10-16-ge-fast-path.md`.

## Problem (measured)

- On the laptop on battery the GE thread spends ~560 ms/s in `lists` during a race and the game thread waits for it
  ~40 % of the time, in `sceGeDrawSync` (perf log, patch 0038 and the wait split of #213); frames are skipped to keep
  the game's speed. Two cores pinned on the desktop: `lists` 450-500 ms/s, GE wait ~27 % (x4, race route).
- The cost is per draw, not per vertex: ~660,000 draws/s, ~14 vertices each; caching the geometry made it slower
  (FINDINGS, 2026-10-15). CPU profile of a race (two cores, 2026-10-16): `hw_job_prepare` 14 %,
  `render_ge_primitive_pass` 13 %, `ge_gpu_backend_accumulate_hardware_triangles` 9 %, libc 15 % (copies,
  compares), texture-map lookups ~4 %, descriptor/transform/lighting helpers ~5 %.
- **What GT draws** (probe over 42 M race draws): 94 % are triangle strips of at most 8 vertices (quads), not
  indexed, textured, with no CPU lighting, no texture-coordinate generation and no flat shading, in two vertex formats:
  `0x112` (16-bit UV, 565 colour, 16-bit position: 84 %) and `0x11A` (the same with 4444 colour: 10 %). None takes the
  existing packed fast path (format `0x115`, 0 %).
- **How much state changes between draws** (probe over 63 M race draws, registers other than the vertex/index
  address, PRIM and the list-flow commands): **nothing at all before 39 % of draws**; before 58 % only the texture
  level register `0xC8` (GT's per-object LOD offset) and sometimes the texture address/size/CLUT registers; any other
  register before 3.4 %. The matrices change before ~1 % of draws.
- **What runs per draw today** (read-only code map, 2026-10-16), whatever changed:
  - the 352-byte draw descriptor is rebuilt from ~60 registers: its cache is keyed by a state revision that
    `host/gt_ge_exec.cpp` never passes (always 0), so it never hits; the same for the lighting cache;
  - 3-4 texture-map lookups (`texture_signature_needed`, `finalize_texture_key`, `texture_needed`,
    `texture_available`, again in accumulate) and 7-9 locks of the backend's recursive mutex;
  - the hardware transform rebuilt (14 float24 decodes, three matrix compares);
  - each vertex decoded into a 56-byte `GeGpuVertex`, "finalized" (fields the upload drops), then converted to the
    40-byte `UploadVertex`; strip indices pushed one by one;
  - the batch-merge test copies and compares 352 + 180 bytes and builds two ~600-byte `Batch` objects.

## Goal

Cut the GE thread's per-draw CPU cost for GT's common draw by **at least 30 %** (`lists` ms/s, two cores pinned,
x4 race route), with **identical output**: the same vertices, indices, batches and uniforms reach Vulkan as today.

## Design

Two pieces, each useful alone, built and measured in this order.

### 1. A state revision from the list executor (`host/gt_ge_exec.cpp`, ours)

- Before storing a command word, the executor compares it with the stored value. A changed register bumps a
  **draw-state revision**, except the registers that are per draw or carried elsewhere: vertex and index address
  (0x01, 0x02), PRIM (0x04), BASE/OFFSET (0x10, 0x13), list flow (0x08-0x0F) and the texture level `0xC8` (its three
  derived descriptor fields are recomputed every draw, cheaply, as 0039 already carries the LOD in the vertices).
  Matrix and lighting registers bump the revision too (the transform and lighting caches key on it).
- PRIM passes the revision to `render_ge_primitive` (the parameter exists). The descriptor cache, the lighting cache
  and the transform cache then hit on ~97 % of draws.
- **Verify mode** `GT_GE_REVISION_VERIFY=1`: on every hit the descriptor, transform and lighting are also rebuilt and
  compared byte for byte; a difference is counted in the perf log and names the register that was missed.

### 2. A fast path for GT's quad strips (lcs-recomp patch, renderer + Vulkan backend)

Taken when: hardware transform, primitive 4 (strip), not indexed, no CPU lighting, no UV generation, no morph or
skinning, no flat shading, texture ready or untextured, a simple vertex format (16-bit or float UV, any colour
format, 16-bit or float position, no normal/weights), and the vertex bytes contiguous in guest memory. Everything
else takes today's path unchanged.

- **One backend call per draw** (`ge_gpu_backend_accumulate_fast_strip`), one lock: it receives the draw-state
  revision, the per-draw LOD value, the raw vertex bytes and the layout.
- **Per-revision cache in the backend**: texture key, texture entry (`Texture*`), target, `vertex_lod`, the
  `sampled_texture_ready` flag and the hardware transform are resolved once per revision (and re-resolved when the
  texture epoch or target set changes) instead of once per draw.
- **Decode straight into `UploadVertex`** in the frame's vertex array: no `GeGpuVertex`, no finalize pass, no
  intermediate vectors. Strip-to-list indices written directly (2 triangles per quad, alternating winding as today).
- **Merge by revision**: a draw joins the previous batch when the revision, the transform and the batch kind are
  the same, an integer compare instead of the 352 + 180-byte memcmps (the merge rule of 0036 is unchanged: same
  state, triangle lists, in order). The UV range of 0044 is extended in the same loop.
- **Verify mode** `GT_GE_FASTPATH_VERIFY=1`: every fast draw also runs the old path into a scratch copy; vertices,
  indices and the batch fields are compared and mismatches counted in the perf log.

### Not in scope

- The other 6 % of draws (lit cars, environment maps, 2D) keep today's path; a later step if the profile shows it.
- Multithreaded decode (measured worse, patch 0021), geometry caching (measured worse, 2026-10-15).
- The stopgap "steady 30 fps" option (plan item 3) — separate, small, after this.

## Verification

- `GT_GE_REVISION_VERIFY=1` and `GT_GE_FASTPATH_VERIFY=1` over the race, dealer and garage routes: **0 mismatches**.
- GPU gate x1: per-route pass counts equal to the run before the change on the same machine state (today: race 315/498,
  dealer 498/498, garage 596/596 with 0045).
- Two cores pinned (`taskset -c 0,1 tools/gpu_perf.sh build OUT 4 220`), same session A/B: `lists` ms/s, GE wait and
  `GE frames/s`, before and after; the goal is `lists` -30 % or better.
- The full differential gate (the executor change touches `gt_native` too: the software path ignores the revision,
  so its output must not change).
- The laptop: a battery log (`GT_PERF_LOG=1`) for the user to compare.

## Risks

- A register missed by the revision gives a stale descriptor (wrong texture, blend, depth): the revision is bumped by
  *any* change except an explicit short list, and verify mode catches the rest.
- The fast path drifts from the old path over time: verify mode is kept, and the gate runs both.
