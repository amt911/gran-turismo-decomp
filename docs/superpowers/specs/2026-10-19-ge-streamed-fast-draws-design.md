# GE: streamed fast draws (one backend handshake per run of like draws)

Status: **approved by the user on 2026-10-19** (chat: "haz todo", after the proposal "rediseño estructural del hilo de
la GE: un candado por lista, vértices directos al formato final, saltar la comparación de agrupado cuando el estado no
ha cambiado; 20-40 % estimado"). Follows `2026-10-16-ge-fast-path-design.md` and
`2026-10-19-ge-backend-per-draw-cost-design.md`.

## Problem (measured)

- Laptop in power-saver (1.9 GHz), race route, after #236: the GE thread decodes ~1 M draws/s at ~550 ns each
  (lists ~550 ms/s); the game waits for it 8.3 % of the time. On the desktop: ~280 ns per draw.
- 94 % of race draws take the quad-strip fast path (patch 0049) and 91 % of all draws join the batch before them
  (2.57 M batches from 28.3 M draws). Yet every fast draw still does the whole backend handshake: it reads the write
  clock under the backend's recursive mutex, builds a 352-byte descriptor and a transform copy, decodes into 56-byte
  `GeGpuVertex`es, then `accumulate` locks the mutex again, looks up the texture target in a hash map, recomputes the
  LOD, compares the descriptor (352 bytes) and the transform with the last batch's, converts each vertex to the
  40-byte `UploadVertex`, extends the UV box and bumps eight counters.
- For a run of fast draws with the same revision, texture level and write clock, all of that is the same answer every
  time except the vertices.

## Goal

At least **20 %** less GE time per draw on the race route (ns per draw, same machine, desktop with two cores pinned
and the laptop in power-saver), with **identical output**: the same vertices, indices, batches, uniforms and
counters reach Vulkan.

## Design

1. **A streaming handle.** After a fast draw has gone through `accumulate` and ended up in the last batch (opened or
   joined), the backend returns a small handle: the batch's index and generation, the LOD it computed, whether the
   UV box applies, and the backend's write clock / frame epoch at that moment. The fast path keeps it next to its
   cache (same revision, layout, dual pass and 0xC8 value).
2. **Append without the handshake.** A next fast draw whose cache key and handle still match calls
   `ge_gpu_backend_append_strip(handle, decoded vertices)`: one lock, a check that the handle is still current (same
   batch generation, batch still last, same write clock and epoch; otherwise it answers "stale" and the draw takes the
   ordinary path), then the vertices go straight into `UploadVertex` form, the strip indices from the table, the batch
   counts, UV box and counters as `accumulate` would update them.
3. **Write clock without the mutex.** The clock becomes an atomic, read lock-free at the start of each draw.
4. **Decode straight to the upload format** on the streamed path (no `GeGpuVertex` in between).

## Verification

- **Permanent dual mode** `GT_GE_STREAM_VERIFY=1`: every streamed draw is also run through the ordinary path on a
  rolled-back copy of the backend's buffers and compared byte for byte (appended vertices, indices, last batch,
  counters), as the temporary check of patch 0051 did; a sabotaged build must report mismatches.
- `GT_GE_FASTPATH_VERIFY=1` and `GT_GE_REVISION_VERIFY=1` stay at 0 mismatches; GPU gate pass counts as on main.
- **Speed:** A/B of ns per draw and GE wait, desktop (2 cores) and laptop (power-saver, binaries from /tmp).

## Out of scope

Running the game and the GE in parallel (the game may rewrite what the GE still reads); multithreaded decoding.

## Result (2026-10-19): not merged

- **Built and verified:** patch 0052 on branch `perf/ge-streamed-fast-draws`, kept in
  `tools/patches/unapplied/0052-ge-streamed-fast-draws.patch` there. `GT_GE_STREAM_VERIFY=1` found 0 differences over
  31.4 M draws; sabotaging the stream (one vertex moved) was caught on every draw. With the texture level out of the
  key (each append brings its level and the LOD is computed from it, as accumulate does), 85 % of fast draws streamed.
- **Speed:**
  - laptop in power-saver, race route, two rounds each: 507-508 → 493-504 ns per draw (−1 to −3 %), GE thread
    71 % → 69-70 %;
  - desktop: noisy under load, also small.
- **Why so little:** a line profile of the streamed build on the laptop shows the GE thread's cost spread over many
  small per-draw pieces:
  - `render_ge_primitive_pass` 5.4 % (including the fast path's own checks);
  - the command loop 4.9 %;
  - `hw_job_prepare` 4.8 % (the draws that are not strips);
  - the stream 6.6 %;
  - the prescan 2.6 %.
  The backend handshake the stream removes was not the bottleneck.
- **Decision:** the goal was ≥ 20 %. About 300 lines and a verify mode for 1-3 % is not worth keeping, and the laptop
  already skips only ~0.1 frames/s in power-saver after #236. A larger gain needs a different structure (decoding
  the lists on more than one core, or the decompiled renderer calling the GPU directly).
