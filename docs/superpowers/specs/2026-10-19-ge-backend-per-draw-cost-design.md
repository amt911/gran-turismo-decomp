# GE backend: the fixed cost of every draw

Status: **direction approved by the user on 2026-10-19** (chat: "venga, me parece bien, continua"; the proposal was to
cut the work every draw repeats before the batch merge, measured on the laptop). Bounded change inside the backend's
accumulate call and the fast path's input builder; the implementation steps below are the plan.

## Problem (measured on the laptop, 2026-10-19)

- Ryzen 7 PRO 5850U in its power-saver profile (CPU capped at 1.9 GHz), plugged in, race route, `MaxInternalScale=2`:
  the GE thread runs at **78-83 % of a core** (`lists` 720 ms/s, ~690 ns per draw, ~1.05 M draws/s) and the game
  waits for it **29 %** of the time, with 7-9 frames/s skipped. The GPU spends ~2.1 ms per frame: it is not the limit.
- 91 % of draws already join the batch before them (28.3 M draws → 2.57 M batches on the desktop race route; the rest
  split on the descriptor 80 %, the transform 17 %, framebuffer feedback 3 %). The cost is the fixed work each draw
  does *before* it joins.
- Line profile on the laptop (gperftools, `-g` on the two GE files): `ge_gpu_backend_accumulate_hardware_triangles`
  7.7 % of all samples, of which **2.1 % is one line**: `Batch box{}` (patch 0044) zero-builds a whole ~600-byte batch
  (descriptor + transform) per draw to hold four floats; vertex/index `push_back` ~1.1 %; `extend_uv_box` ~0.8 %.
  `fast_strip_inputs` 3.8 % (vertices zeroed then filled, indices pushed one by one). The merge test copies the
  352-byte descriptor to compare it (memtrace: 352-byte moves ~480 k/s in accumulate, ~420 k/s in the fast path).

## Goal

Cut the GE thread's per-draw cost by **at least 10 %** (ns per draw, same route, same machine) with **identical
output**: the same vertices, indices, batches and uniforms reach Vulkan.

## Design (each item alone keeps the output identical)

1. **UV box without a batch.** A four-float `UvBox` (plus its valid flag) replaces `Batch box{}`; `extend_uv_box` and
   `merge_box` take it. Same arithmetic, same order.
2. **Appends without per-element growth.** Vertices and indices are appended with one `resize` and direct writes
   (the overflow check on indices stays, before anything is written, as today's rollback leaves no trace).
3. **Merge test without the copy.** The descriptor compare skips the four fields allowed to differ (vertex count,
   texture level mode/offset/selected level) by comparing the bytes around them, instead of copying the 352 bytes
   into a scratch descriptor first. Equal iff today's test is equal (padding is zero in both, patch 0046).
4. **One target lookup.** `framebuffer_feedback` reuses the `sampled_target` lookup made at the top of the call
   (no draw or target is registered in between).
5. **Fast-path inputs written once.** `fast_strip_inputs` writes each vertex field once (no `out = {}` first: every
   field `GeGpuVertex` has is either set or left at its default, so the default-initialised fields are set from a
   template vertex) and takes the strip indices from a constant table for counts up to 8 (longer strips build them
   as today).

## Verification

- **Identical output, A/B:** a temporary hash of everything the frame uploads (vertices, indices, each batch's
  descriptor, transform, counts and UV box) per GE frame, printed on the race route, before and after; the two
  sequences must match (the route is deterministic: two `--decomp off` runs give identical import traces).
- **Gates:** `tools/gpu_gate.sh` (GPU vs software GE images) and `GT_GE_FASTPATH_VERIFY=1` / `GT_GE_REVISION_VERIFY=1`
  with 0 mismatches; `ctest`.
- **Speed:** `ns per draw` on the desktop with two cores pinned and on the laptop (power-saver, plugged in), race
  route, before and after; kept only if it improves.

## Out of scope

Fewer locks per draw (one per list), skipping the merge test on a revision hit, multithreaded decoding: larger
changes, for a later spec if this one is not enough.

## Result (2026-10-19)

- **Identical output:** a temporary dual run executed the new and the pre-0051 accumulate on every draw of the race route,
  with rollback between them: 16.8 M calls, 0 differences in the appended vertices, indices, batches and the eight
  counters it touches. Sabotaging the new code (one vertex moved) made it report mismatches.
  `GT_GE_FASTPATH_VERIFY=1` checked 11.6 M fast draws against the general path (0 mismatches), and
  `GT_GE_REVISION_VERIFY=1` found 0 mismatches. The one behaviour change is unseen: an index past the draw's
  vertices used to leave the indices before it in the buffer, which no batch referenced; now nothing is appended.
- **GPU gate:** dealer 591/591, garage 611/611. Race 344 passing, the same 344 as the code before the change (836
  pairs, 344 passing). The race fails on mip levels by design (FINDINGS, 2026-10-15).
- **Speed (race route, ns per draw, median of the race windows):**
  - desktop, two cores pinned, x4: 289 → 280 (−3 %);
  - laptop in its performance profile, x2, two A/B rounds: 292-293 → 277-281 (−4.5 %).
- **The 10 % goal was not met.** The change is kept: it is a consistent gain with identical output. The laptop had
  switched from power-saver to performance between the first measurement (690 ns per draw, 29 % GE wait) and the
  A/B. In performance mode the race skips 0.3 frames/s with either build.
