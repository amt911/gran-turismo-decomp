# GE: static quad strips live on the GPU (a vertex arena)

Status: **implemented, measured, not merged (2026-10-20)**: no gain on the laptop; see *Result*. Asked for by the user
on 2026-10-20 ("relanza todo, que vaya rápido ... mejora todo a nivel gráfico para
que vaya hasta en un microondas"). Stage 4 of the approved no-emulation plan; it replaces the on-hold blob replay
(`2026-10-20-ge-blob-replay-design.md`, revised estimate). The implementation plan is the "Steps" section.

## Problem (measured)

- 97 % of a race frame's PRIMs are replays of static model blobs (FINDINGS 2026-10-20), and ~90 % of all draws take the
  quad-strip fast path (patch 0049).
- **Per-vertex work dominates the GE thread.** Removing only the per-draw backend handshake gained 1-3 % (patch 0052),
  because each strip's vertices are still decoded, converted to the 40-byte `UploadVertex` (with the LOD and UV box),
  appended and copied into the upload buffer, every frame: ~74k vertices, ~3 MB per race frame.
- The laptop in power-saver spends ~540 ns per draw on the GE thread, which runs at ~74 % of a core.

## Design

1. **Arena.** The geometry buffer is split into a static arena of `UploadVertex`es, which persists across frames and is
   copied to the GPU only where it changed, followed by the frame's vertices and indices as today. Batches index either
   region through `vkCmdDrawIndexed`'s vertex offset, and a frame batch's offset includes the arena's size.
2. **Static strip cache (renderer fast path).** A fast-path strip is looked up by (vertex address, count, layout type).
   - **Hit:** the entry is valid when the strip's raw guest bytes equal the recorded ones (`memcmp`), and its LOD inputs
     (texture level fields, internal scale, LOD bias), material colour (for layouts without vertex colour) and UV
     transform equal the recorded ones. The backend then adds the draw with the entry's arena range: the same merge rule
     as `ge_gpu_backend_accumulate_hardware_triangles` (same descriptor except the per-draw fields, same transform,
     contiguous ranges in the same region), the strip indices appended relative to the batch, the recorded UV box merged,
     and the same counters.
   - **Miss:** the ordinary fast path runs, and the backend also copies the appended `UploadVertex`es into the arena
     (on the next frame the strip is a hit).
3. **Invalidation.** The whole arena is dropped when it fills, when the internal resolution changes (the LOD depends on
   it), and on `GT_GE_NO_ARENA=1`. A changed strip simply fails the byte compare and is re-recorded.

## Verification

- **`GT_GE_ARENA_VERIFY=1`, permanent.** Every hit also decodes the strip the ordinary way and compares the
  `UploadVertex`es it would append with the arena's, the UV box and the batch result. The ordinary result is used.
- **Sabotage.** One arena vertex moved, and the byte compare skipped. Each must be reported.
- **GPU gate:** race 923/923, dealer and garage unchanged. CTest, plus the fast-path and revision verify modes at 0.
- **Speed:** ns per draw and GE time, A/B on the desktop with two cores and on the laptop in power-saver (binaries from
  /tmp, nothing left on the laptop).

## Steps

1. Backend: the arena region in the geometry buffer, entries, and a static draw entry point (with unit-level checks
   where they fit).
2. Renderer: the lookup, record and hit in the fast path, plus the verify mode.
3. Verify run on the race route, sabotage, GPU gate.
4. A/B on both machines, FINDINGS, PR.

## Result (2026-10-20): correct, no faster, not merged

- **Correct.** On the race route with `GT_GE_ARENA_VERIFY=1`, ~99.9 % of fast draws hit the arena, with 0 level misses,
  0 resets, ~210k arena vertices, and 0 mismatches over ~4 M verified draws per 5 s window. Both sabotages were reported:
  a moved vertex gave 45,796 mismatches, a skipped byte compare 34,239. The second one is caught by strips the game
  rewrites in place, such as the dynamic buffers at 0x08D201C0 and 0x08D401C0, so the byte compare is needed. CTest
  passed 15/15. The GPU gate was stopped once the A/B showed no gain.
- **No faster.** On the laptop in power-saver (binaries from /tmp, user's ini), the race windows cost 662-668 ns per draw
  with the arena against 658-665 without it. Later windows were 570-600 against 548-582, 3-5 % worse. Both builds held 60
  new images/s.
- **Why.** A gperftools profile of the arena build puts 12 % of all samples in `ge_gpu_backend_static_strip_draw`, plus
  2.9 % in `draw_arena_strip`. A hit is a lookup in a hash of ~30k strips and a compare with a signature stored on the
  heap: random cache misses. They replace a sequential decode of bytes the CPU already had in cache. The per-vertex
  work this spec removes was never the cost, and the blob census's "per-vertex append" estimate did not hold on the
  target machine.
- The code is on branch `perf/ge-static-arena` (patch `tools/patches/unapplied/0053-ge-static-strip-arena.patch`).
