# Playable build — window, input, audio (design)

Status: decided in unattended mode (2026-10-10); the user asked for a way to *play* it ("usar luego
algún motor tipo Vulkan… para que pueda jugarlo").

## Goal

A second executable, `gt_play`, that opens a window and runs GT in real time with keyboard/gamepad
input and sound — while `gt_native` stays the headless, scriptable build the differential harness
and the boot checks use. Same generated code, same HLE, same decompiled functions: only the host
backends differ.

## What exists

- Our host implements lcs-recomp's backend interface headlessly (`host/headless_backends.cpp`):
  window/input (`display_window_*`, `GT_PAD`), audio output (dropped), GPU backend (always "no
  GPU": the software rasterizer `ge_renderer.cpp` draws into guest VRAM), overlay/menu/language
  hooks, and `precise_sleep_until` (real sleeps — vblank pacing is already real time).
- The pinned lcs-recomp (MIT) ships real implementations of the same interface:
  `display_window_sdl.cpp` (SDL2 window, input, present), `ge_gpu_backend_vulkan.cpp` (+ shaders),
  `lcs_audio_output.cpp`, `lcs_controls.cpp`, `lcs_frame_limit.cpp`, `lcs_render_config.cpp`,
  `lcs_fps_overlay.cpp`, `lcs_menu.cpp`/`lcs_display_menu.cpp`. SDL2, Vulkan and glslang are
  installed here.

## Design

1. **CMake:** everything both builds share becomes a static library `gt_host` (main, HLE, harness,
   decompiled sources as an OBJECT library so `GT_DECOMP` registrars are kept). `gt_native` =
   `gt_host` + `headless_backends.cpp`. `gt_play` = `gt_host` + the lcs-recomp real backends, compiled
   from the pinned checkout (not vendored), + a small `host/play_backends.cpp` of ours for the hooks
   GT must not get from LCS (language patches, LCS menu commands).
2. **Step 1 — window + software frames:** present the guest framebuffer the software rasterizer
   already produces (the same pixels the frame dumps show), keyboard/gamepad mapped to the PSP pad,
   SDL audio output. Correct first; speed second.
3. **Step 2 — Vulkan GE backend:** enable lcs-recomp's Vulkan backend for GT (it was written for LCS:
   expect GT-specific fixes as `tools/patches/lcs-recomp/` patches), for speed and upscaling.
4. Building `gt_play` is optional (`-DGT_PLAY=ON`, default ON when SDL2 and Vulkan are found) so the
   headless build keeps working on a machine without them.

## Testing

- `gt_native` behaviour unchanged: race-path diff (0 mismatches) and the boot frame check.
- `gt_play`: builds; `--max-seconds` run with `SDL_VIDEODRIVER=offscreen` (no display needed)
  reaches the race with the same scripted `GT_PAD` input; frames presented counted in the log.
- Manual: run it, play — the PR's test plan says how.

## Out of scope

Save-game loading (white screen, separate bug), online/ad hoc, widescreen hacks.
