# GPU path: a geometry cache for draws whose vertex bytes repeat — built, measured slower, not merged

Approved by the user on 2026-10-15 (chat: "Sí, adelante"), as a short spec + implementation + review in its own PR.

## Problem (measured)

- On the laptop (Ryzen 7 PRO 5850U on battery) the GE thread spends up to **530 ms/s in `lists`** during a race
  (`GT_PERF_LOG`, 0038's split): decoding every draw's vertices on the CPU, converting them for the GPU and building
  their triangle indices. The game thread waits for it (`GE wait` 35–40 %) and the frameskip drops 20–30 frames/s.
- **99.8 % of the vertices** decoded in a race frame are byte-identical to the previous time the same draw ran
  (probe over 30 s of the race route, 2026-10-15): the track and the car models are fixed data; the game only
  changes the matrices, which the GPU path already applies on the GPU.
- lcs's own static vertex cache (`PSPRECOMP_GE_VERTEX_CACHE=1`) is slower than decoding (lists 621 vs 400 ms/s on
  two cores): it hashes slowly, caches the pre-conversion vertices and still runs everything after the decode.

## Design

A cache of the **final per-draw output** of the hardware-transform path: the `UploadVertex` array (already
converted, with its LOD) and the triangle index array the backend appends for the draw, plus the primitive and
indexed flag the batch was built with.

- **Key** (renderer, 64-bit, a fast 4-lane hash): the draw's contiguous vertex bytes, its index bytes when indexed,
  the vertex format (`layout.type`), index type, count, primitive, decode count and first index, flat shading,
  whether the texture was ready (the debug colour of the finalize step), and the material colour registers
  (0x55, 0x58) when the format has no vertex colour. The backend adds the per-draw LOD (`vertex_lod`).
- **Eligible draws only**: contiguous decode with the bytes in guest memory, no CPU lighting
  (`cpu_lighting_effective`), no texture-coordinate generation, one morph target, no skinning. Everything a decoded
  vertex depends on is then in the key (read from `decode_model_vertex_for_gpu` and its 0115 fast path).
- **Hit**: the backend appends the cached vertices and indices to the frame's arrays and builds the batch with the
  same code as a miss, so the 0036 merging, the descriptor, the transform uniforms and the texture all come from the
  current draw as before. The decode, finalize, strip conversion and per-vertex conversion are skipped.
- **Miss**: the normal path runs; the backend stores what it appended under the key.
- **Bounds**: at most 64 MB of entries; entries unused for 120 frames go first, then everything if still over.
- **Verify mode** `GT_GPU_GEOMETRY_CACHE_VERIFY=1`: on a hit the normal path runs anyway and the appended data is
  compared with the entry; mismatches are counted in the perf log. A missing key input shows up there.
- **Off switch** `GT_GPU_GEOMETRY_CACHE=0`.

## Verification

- Verify mode over the race, dealer and garage routes: 0 mismatches.
- GPU gate x1: per-window means and pass counts as before (within run-to-run noise).
- Two cores pinned (`tools/gpu_perf.sh`): `lists` ms/s before and after; GE wait; and the cache's hit rate in the
  perf log.

## Out of scope

Keeping the geometry resident on the GPU (no per-frame copy and upload): a later step if the copy shows up.

## Outcome (2026-10-15): built and verified exact, but slower — not merged

Implemented as specified (lcs-recomp, uncommitted): verify mode over the race gave **0 mismatches** across ~1M cached
draws per second, so the key is complete. But two cores pinned, race: `lists` **404 → 556 ms/s** with the cache on
(GE wait 20 → 34 %). A race draw has ~14 vertices: decoding reads 10-14 compact bytes per vertex from guest memory,
while a hit copies 40-byte converted vertices out of a ~30 MB table scattered across the heap and adds a lookup per
draw. The per-vertex work was never the cost; the fixed cost of each of ~660,000 draws per second (~600 ns on the
desktop) is. Keeping the geometry resident on the GPU would remove the copy too, but the measured ceiling (decode,
conversion and upload together are ~17 % of the process) does not justify that complexity before the per-draw
overhead is cut.
