# GE: replaying GT's static model blobs instead of re-decoding them

Status: **follows stage 4 of the approved no-emulation plan** (`2026-10-19-no-emulation-design.md`, approved
2026-10-19: "lo apruebo todo"; the user asked on 2026-10-20 to keep going without stopping, "no pares hasta que yo te
lo diga"). Written 2026-10-20 from measurements; the implementation plan is the "Steps" section.

## Problem (measured)

- **Where the draws come from.** A race frame's GE list holds ~12.4k PRIMs, 97 % of them inside ~300 CALLed model blobs:
  a command list plus its own vertices, static, built at load time (FINDINGS 2026-10-20).
- **Where the time goes.** On the desktop with two cores, race route, 97-98 % of the GE thread's list time is spent
  inside those CALLs (from the CALL at depth 0 to its RET), and 99.5 % of race PRIMs are there.
- **The laptop in power-saver.** The GE thread runs at ~74 % of a core and ~540 ns per draw; the game waits for it 6-30 %
  of the time.
- **Blobs are re-decoded every frame although nothing in them changed.** The probe of 2026-10-20:
  - the content (words up to RET + 64 vertex bytes per PRIM) repeats in ~100 % of calls;
  - the entry state differs in 87-100 %, but only in registers that alternate per frame (framebuffer `0x9C`, dither
    `0xE2-0xE5`), in light registers (`0x5F-0x8F`, irrelevant to unlit draws) and in registers the blob rewrites before
    reading them;
  - with a key of "content + the entry registers the blob reads before writing them, minus matrices, minus light
    registers when no PRIM of the blob is lit", keeping two entries per target, **59-67 % of race CALLs hit, carrying
    71-84 % of the race's PRIMs**;
  - lit blobs (5-30 % of calls) are left out: their vertex colours depend on the matrices.

- **What the replay can remove.** The `render_ge_primitive` calls inside blobs take 81-84 % of the race's list time
  (accumulate included); the command walk is the rest. The replay keeps the walk and the accumulate calls and removes
  the per-draw decode and fast-path work of hit blobs, an estimated 30-45 % of the list time.

## Goal

At least **40 % less GE list time** per race frame (ms/s of `lists`, same route, desktop with two cores and the
laptop in power-saver), with **identical output**: the same backend calls with the same arguments, in the same order,
as the ordinary decode.

## Design

1. **Recording, in `host/gt_ge_exec.cpp`.** For a depth-0 CALL into a blob that has no excluded command (block
   transfer `0xEA`, matrix data `0x2A-0x3F`, bounding box `0x05`, JUMP, END, FINISH, SIGNAL) and no lit PRIM, the
   command loop runs as today and also records:
   - the key: the content hash (words, and the vertex bytes each PRIM reads, from its count and stride) and the entry
     registers the blob reads before writing them, minus matrices and light registers;
   - the effects: for each PRIM, either "live" (it went through the general path: textures, keys, targets) with the
     register file at that PRIM, or "fast" (it only called `ge_gpu_backend_accumulate_hardware_triangles`) with that
     call's vertices, indices and per-draw fields;
   - the exit state: the register file, the vertex and index addresses, BASE and OFFSET after RET.
   Two entries per target (GT alternates two framebuffers and dither matrices per frame).
2. **Replay.** A later depth-0 CALL to the same target with the same key replays instead of decoding.
   - **Live PRIMs** run through `render_ge_primitive` exactly as before, with the recorded register file and the
     current matrices and transform state. That keeps every texture, key, target and fast-path-cache side effect.
   - **Fast PRIMs** are re-issued as the same backend accumulate calls, with the descriptor and transform of the live
     PRIM before them (as the fast path does) and the recorded vertices. The renderer's fast-path entry is extended to
     take pre-decoded vertices.
   - The exit state is applied at the end.
   - The key covers everything a fast draw's backend arguments depend on except the matrices, and the matrices reach
     them only through the transform, which is rebuilt from the current state for each live PRIM, as live does.
3. **Invalidation.** The key is recomputed on every CALL. Hashing the vertex bytes costs ~1.2 MB per race frame, so a
   cheaper validity check may replace it later, but only behind the verify mode below. A frame with the frameskip on,
   the dual GPU/software pass, or `GT_GE_NO_REPLAY=1` decodes normally.

## Verification

- **`GT_GE_REPLAY_VERIFY=1`, permanent.** Every hit is also decoded the ordinary way. The backend accumulate calls of
  both are captured (arguments byte for byte, and order) and compared; the ordinary result is kept. Mismatches are
  counted in the perf log.
- **Sabotage.** One recorded vertex moved, and one key register dropped from the key. Each must make the verify mode
  report mismatches.
- **GPU gate:** race 923/923 frames, dealer and garage unchanged. CTest and the existing verify modes stay at 0.
- **Speed:** A/B of `lists` ms/s and ns per draw, desktop with two cores and the laptop in power-saver (binaries
  from /tmp, nothing left on the laptop). Kept only if it meets the goal or the user agrees to a smaller gain.

## Steps

1. Recording plus verify-only mode: hits are re-decoded and compared, nothing is replayed yet. This proves the key
   and the recording.
2. Replay of fast PRIMs and live PRIMs. Verify mode on the whole race route, then sabotage.
3. GPU gate, then A/B on both machines.
4. FINDINGS and docs, then the PR.

## Out of scope

Lit blobs, blobs with matrix or transfer commands, a native model format (stage 4's later part), and multithreaded
decoding.
