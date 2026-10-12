# GPU GE backend for GT — upscaled, fluid rendering (design)

Status: approved by the user in conversation (2026-10-12), after testing `gt_play` and reporting that
raising the resolution does nothing, the picture stays pixelated, and the frameskip is choppy on a
recent PC. Chosen order: GPU first, PSMF video audio afterwards (separate task).

## Goal

`gt_play` draws GT's GE command lists on the GPU at a configurable internal resolution (×2–×4). It runs
at full speed (about 120 vblank waits/s, no frameskip) on this PC, and its images match the verified
software rasterizer apart from resolution and small rounding.

**Done** means:
- The GPU gate (below) is green on the three pad routes: race, dealer and garage.
- `GT_PERF_LOG` in the race shows about 120 vblanks/s and 0 skipped frames at ×1, ×2 and ×4.
- `NativeGE=true` ships as the default.

## What exists (evidence: code read on 2026-10-12)

- **The decoder.** One GE list decoder feeds both renderers. `sceGeListEnQueue` (`host/gt_kernel.cpp`)
  calls `execute_ge_list_rendered` (`host/gt_ge_exec.cpp`, our fork), which calls `render_ge_primitive`
  in lcs-recomp's `ge_renderer.cpp` (shared). That function decodes vertices, transform and lighting,
  and textures. Then either the software rasterizer writes guest VRAM, or the Vulkan backend
  (`ge_gpu_backend_vulkan.cpp`, ~3,450 lines, MIT) receives a draw descriptor.
- **The Vulkan backend** keeps one color+depth image per framebuffer address. It has an internal-
  resolution setting (`[Rendering] InternalResolutionMode` / `InternalScale` 1–8), which `gt_play.ini`
  does not set today. `[Display] Scale` only sizes the window, so with the software path the image
  is always 480×272, upscaled bilinearly.
- **Gaps that probably explain "draws GT wrong"** (from the code, not yet measured):
  - Only five blend factor combinations, LCS's. Every other blend draws opaque.
  - GPU-rendered pixels never reach guest RAM. Block transfers, CPU reads, and textures that sample
    inside a render target at an offset see stale data.
  - LCS shortcuts: a widescreen/HUD shrink, a menu scale, a packed vertex fast path (type 0x115), the
    "LCSNative" app name, and `view_distance` (GTA camera fields).
  - Bezier/spline are missing in both renderers. They are only a gap if GT draws them.
- **Initialization.** The backend chooses a device with or without a surface (`graphics_family(...,
  need_present)`), so a headless device is plausible.

## Design

### Architecture
- **The decoder stays.** `gt_ge_exec.cpp` and `ge_renderer.cpp` decode as today; only the drawing side
  changes.
- **GT fixes are patches.** They are lcs-recomp patches (`tools/patches/lcs-recomp/0019-…`), like 0001–0018.
  A fix that grows too large becomes a fork of ours in `host/`, as `gt_ge_exec.cpp` did. Our code
  keeps MIT-compatible provenance. PPSSPP's GPU (GPL) is not ported.
- **Fixes, in order of likelihood.** Each one is confirmed by measurement (the phase 1 report) before
  it is written:
  1. **Blending.** Every GE blend equation and factor GT uses (fixed A/B colours included), mapped to
     Vulkan blend state or a shader path.
  2. **Write-back to guest memory, only where GT needs it.** When a GPU-rendered target is read as a
     texture at an offset, block-transferred, or read by the CPU, its pixels are downsampled to 1× and
     written back so the game sees what a PSP would. Detected per target address, never done every
     frame for every target.
  3. **LCS shortcuts removed for GT.** HUD/widescreen shrink and menu scale off, app name changed,
     `view_distance` not exposed. The packed vertex path stays only if it is generic.
- **Headless GPU mode.** `gt_native` can run the Vulkan backend without a window (no surface, no
  swapchain), so the GPU gate runs unattended.

### Verification — the GPU gate
- **`GT_GPU_DUMP=<dir>`** (+ `GT_FRAME_EVERY`) writes the GPU's display target, downsampled to 480×272,
  at the same frames `GT_FRAME_DUMP` uses for the software path.
- **`tools/gpu_compare.py`** (pytest, TDD, coverage ≥ 80 %) compares GPU dumps with software dumps frame
  by frame:
  - A per-pixel tolerance for rounding, plus a cap on the share of differing pixels.
  - A missing element or an opaque block fails.
  - It prints the worst frames and writes difference images to a gitignored directory.
- **The gate is shown failing once** before it is trusted: blending is sabotaged, and the gate must go
  red.
- **Both builds are compared on the same route and frames.** The software rasterizer is the reference
  (verified against PPSSPP), and the GE input is the same, so frame N is comparable.
- **Performance.** `GT_PERF_LOG=1` on the race route at ×1, ×2 and ×4 must show vblanks ≈120/s and
  skipped/frameskip 0.

### Configuration
- `[Rendering] NativeGE` stays `false` until the gate is green on the three routes, then becomes `true`.
- `InternalResolutionMode=Scale`, `InternalScale=2` by default, changeable from the F10 menu.
- The GTA-only "render distance" option is hidden for GT.
- With the GPU, the frameskip (patch 0018) is off unless measurement shows it is still needed.

### Fallback
- **Vulkan fails to start.** A terminal warning, and the software rasterizer stays, as today.
- **A draw kind the gate shows cannot yet be matched.** That kind goes to the software path (render
  into guest VRAM, then upload) and gets a FINDINGS entry, instead of shipping a wrong image.

## Phases

Each phase is one PR with the GPU gate, the boot check and a manual test plan.
1. **Measure.** Headless GPU mode, `GT_GPU_DUMP`, and `tools/gpu_compare.py` with tests. The output
   is the first per-route report of what differs. No rendering fixes yet.
2. **Blending and LCS shortcuts.** Blending fixes, LCS shortcuts removed, F10 resolution option.
3. **Write-back.** Write-back and offset render-to-texture, if phase 1 shows GT needs them.
4. **Speed and default.** Performance measurement at ×1/×2/×4, frameskip off with the GPU,
   `NativeGE=true` by default. README, COMPILAR and ROADMAP updated.

## Out of scope
- PSMF video audio: next task, its own design.
- Porting PPSSPP's renderer (GPL).
- Shader-level enhancements: MSAA/SMAA, texture filtering beyond what the GE asks for.

## Risks
- **No headless Vulkan.** If a device without a surface fails on this NVIDIA driver, the gate needs a
  hidden window. Measured in phase 1, and the fallback is a hidden SDL window.
- **GT reads render targets more than expected.** Then write-back costs more time per frame; it is
  measured in phase 3.
- **Rounding.** Differences between the shader and the software rasterizer may need a tolerance tuned
  on real frames. The tolerance is fixed once, written down, and never widened to make a failure
  pass.
