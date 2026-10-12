# GPU GE backend — Phase 1 (measure) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the GPU gate. One `gt_play` run, headless, writes the Vulkan backend's image and the software rasterizer's image for the same frame. A tested tool compares the two. The run on the three pad routes produces the first per-route report of what the GPU draws wrong.

**Architecture:** The pieces already exist:
- `gt_play` runs headless with `SDL_VIDEODRIVER=offscreen`. The Vulkan backend then falls back to CPU readback (measured 2026-10-12: `[vulkan] … using the CPU readback`).
- `PSPRECOMP_GE_GPU_SKIP_SOFTWARE_RASTER=0` keeps the software rasterizer drawing into guest VRAM while the GPU also draws (lcs-recomp `ge_renderer.cpp`, `software_raster_skipped`).

This phase adds three pieces:
- **Dump.** `GT_GPU_DUMP=<dir>` in `present_frame` (`host/gt_kernel.cpp`) writes `gpu_NNNNNN.ppm`, downsampled to 480×272, and `soft_NNNNNN.ppm` for the same presented frame.
- **Comparison.** `tools/gpu_compare.py` compares the pairs.
- **Runner.** `tools/gpu_gate.sh` runs the three routes and the comparison.

**Tech Stack:** C++20 (host), Python 3 + pytest (tools), lcs-recomp Vulkan backend (MIT), SDL2 offscreen.

**Spec:** `docs/superpowers/specs/2026-10-12-gpu-ge-backend-design.md`

## Global Constraints

- **Memory cgroup.** Every heavy job runs inside `systemd-run --user --scope --quiet -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 --`, under `flock /tmp/claude-1000/gt-heavy.lock`, because the decomp agent shares the machine.
- **This worktree** is `/home/andres/repos/gt-gpu` (branch `feat/gpu-ge-backend`). It has its own `build-gpu/` and `recomp/`, and its own copy of `tools/src/lcs-recomp`. `extracted/` and `.venv` are symlinks to the main checkout.
- **No game data in git.** The dumps go to `recomp/analysis/gpu/`, which is ignored.
- **`tools/` Python** is written with TDD, pytest, coverage ≥ 80 % (`docs/agents/testing.md`).
- **Comparison thresholds** are fixed once, written in the tool, and never widened to make a failure pass.
- **lcs-recomp changes**, if any, are patches in `tools/patches/lcs-recomp/` (next: 0019). Never edit the checkout by hand.

## Review Focus

- **No GPU frame.** A frame where the GPU produced nothing (readback empty) must be reported as missing, not silently skipped. That is the "missing element" case.
- **Size and format.** A GPU image larger than 480×272 (InternalScale 2–4) must be box-downsampled, not cropped. A software framebuffer in 16-bit formats must be decoded the same way `frame_dump` does.
- **Pairing.** Pairs must come from the same presented frame (one counter). An off-by-one between `gpu_` and `soft_` would make every comparison noise.
- **Gate proven.** The gate must be shown failing on a deliberate sabotage before its first green result is trusted.
- **Tool robustness.** A truncated or unreadable PPM in the tool gives a clear error, not a crash.

---

### Task 1: Worktree build

**Files:** none committed. This builds `build-gpu/` in the worktree.

- [ ] **Step 1: Apply our patches to the worktree's lcs-recomp copy and configure.**
  The copy came from the main checkout with patches 0001–0018 applied (marker `.gt-patches`).
  Run: `cd /home/andres/repos/gt-gpu && tools/setup.sh` (idempotent: it must say "patches up to date").
  Expected: `patches up to date`.
- [ ] **Step 2: Generate recomp/ and build.**
  Run: `flock /tmp/claude-1000/gt-heavy.lock systemd-run --user --scope --quiet -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- sh -c '.venv/bin/python -m tools.recomp >/dev/null && cmake -S . -B build-gpu -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-gpu -j8 >/tmp/claude-1000/gpubuild.log 2>&1'; tail -3 /tmp/claude-1000/gpubuild.log`
  Expected: the last line links `gt_play`.
- [ ] **Step 3: Headless GPU smoke test.**
  Run: `printf '[Rendering]\nBackend=Vulkan\nNativeGE=true\nInternalResolutionMode=Scale\nInternalScale=2\n' > /tmp/claude-1000/gpu_gate.ini` and then `flock … systemd-run … -- env SDL_VIDEODRIVER=offscreen PSPRECOMP_CONFIG=/tmp/claude-1000/gpu_gate.ini build-gpu/gt_play --max-seconds 15 2>&1 | grep -v h264 | head`
  Expected: `[vulkan] … using the CPU readback`, then `Runtime stopped: Wall-clock limit reached`.

### Task 2: `tools/gpu_compare.py` (TDD)

**Files:**
- Create: `tools/gpu_compare.py`
- Create: `tools/tests/test_gpu_compare.py`

**Interfaces:**
- Produces: `python -m tools.gpu_compare DIR [--report FILE] [--diff-dir DIR]`. The exit code is 0 when every pair passes, 1 when any fails, and 2 on a usage or IO error.
- Library functions:
  - `read_ppm(path) -> Image` (`Image` = dataclass with `width`, `height`, `pixels: bytes` RGB).
  - `compare(soft: Image, gpu: Image) -> Result`, with `Result` = dataclass `differing: int`, `total: int`, `max_delta: int`, `blocks: int`, `passed: bool`.
  - `pairs(directory) -> list[tuple[int, Path, Path | None]]`.
- Thresholds (constants):
  - `PIXEL_TOLERANCE = 24`: max per-channel |Δ| counted as equal. It covers 5-6-5 quantisation (8) plus shader-versus-rasterizer rounding.
  - `MAX_DIFFERING_SHARE = 0.01`: at most 1 % of pixels differ.
  - `BLOCK = 8`: an 8×8 block where every pixel differs counts as a block. Any block fails, because it is a missing element or an opaque quad.

- [ ] **Step 1: Write the failing tests** in `tools/tests/test_gpu_compare.py`, using pytest and `tmp_path`:
  - `test_read_ppm_roundtrip`: write a 2×1 P6 file, read width, height and pixels back.
  - `test_read_ppm_rejects_truncated`: a header claiming 4×4 with 3 bytes of data raises `ValueError`.
  - `test_identical_images_pass`: `compare(a, a)` has `differing == 0` and `passed is True`.
  - `test_rounding_within_tolerance_passes`: every channel off by 20 passes.
  - `test_missing_block_fails`: one 8×8 black square in a white image fails with `blocks >= 1`, even though the share is below 1 %.
  - `test_scattered_differences_over_share_fail`: 2 % of pixels off by 100, scattered (no full block), fails on the share.
  - `test_pairs_reports_missing_gpu_frame`: `soft_000003.ppm` without `gpu_000003.ppm` gives `(3, soft, None)`.
  - `test_cli_exit_codes`: one passing directory gives exit 0, one failing gives 1, a nonexistent directory gives 2. Call `main([...])` directly.
  - `test_cli_writes_report_and_diffs`: `--report` writes one line per frame. `--diff-dir` writes `diff_NNNNNN.ppm` for failing frames only.
- [ ] **Step 2: Run the tests and watch them fail.**
  Run: `.venv/bin/pytest tools/tests/test_gpu_compare.py -q`
  Expected: errors because `tools.gpu_compare` does not exist.
- [ ] **Step 3: Implement `tools/gpu_compare.py`.**
  - `read_ppm` parses `P6\n<w> <h>\n255\n` (comments not needed, since our writer emits none) and checks that the data length is w·h·3.
  - `compare` counts pixels with any channel |Δ| > tolerance, then scans 8×8 blocks.
  - A missing GPU frame is a failure, reported as `missing`.
  - The CLI prints a summary: `pairs N, passed P, failed F (missing M)`, plus the 10 worst frames by differing share.
  - The diff image is red where the pixels differ and the soft image dimmed elsewhere.
- [ ] **Step 4: Run the tests and coverage.**
  Run: `.venv/bin/pytest tools/tests/test_gpu_compare.py -q && .venv/bin/pytest --cov --cov-fail-under=80 -q`
  Expected: all pass; total coverage ≥ 80 %.
- [ ] **Step 5: Commit.** `feat(tools): gpu_compare — compare GPU and software GE frames (tests N/N)`

### Task 3: `GT_GPU_DUMP` in `present_frame`

**Files:**
- Modify: `host/frame_dump.hpp` and `host/frame_dump.cpp`. Split out `write_guest_ppm(runtime, path, frame_buffer, buffer_width, pixel_format, width, height)` (the existing body) and add `downsample_rgba_box(std::span<const std::byte> rgba, w, h, out_w, out_h) -> std::vector<std::uint8_t>` (RGB) and `write_rgb_ppm(path, rgb, w, h)`. `frame_dump` keeps its behaviour by calling `write_guest_ppm`.
- Modify: `host/gt_kernel.cpp` `present_frame`. In the branch that presents a GPU frame (`!rgba.empty()`), when `GT_GPU_DUMP` is set, every `GT_FRAME_EVERY`-th GPU-presented frame writes `gpu_NNNNNN.ppm`, downsampled to `present_width × present_height`, and `soft_NNNNNN.ppm` from the guest framebuffer, with one counter. When the GPU branch did not present for a frame the software path presents, the frame writes `soft_NNNNNN.ppm` only, so it is reported as missing.
- Create: `host/tests/frame_dump_test.cpp`. Modify: `CMakeLists.txt` (test target `frame_dump_tests`, `add_test(NAME frame_dump …)`).

- [ ] **Step 1: Write the failing C++ test** for `downsample_rgba_box`, in the style of the other host tests (plain `main`, assert-style checks):
  - A 4×2 RGBA image of two 2×2 blocks, (0,0,0,255) and (200,100,50,255), downsampled to 2×1, gives exactly {0,0,0, 200,100,50}.
  - A 960×544 constant image downsampled to 480×272 gives the same constant.
  - A source that is not an integer multiple (e.g. 5×3 → 2×1) averages the covered pixels without reading out of bounds. Check the result against hand-computed values.
- [ ] **Step 2: Build the test and watch it fail.**
  Run: `cmake --build build-gpu --target frame_dump_tests` (under the lock and cgroup).
  Expected: a compile or link error because `downsample_rgba_box` is undefined.
- [ ] **Step 3: Implement** `downsample_rgba_box`, `write_rgb_ppm`, `write_guest_ppm` and the `present_frame` hook. The counter is a function-local static next to the hook. The directory is created once. Images are game output, so the comment says to keep them out of git.
- [ ] **Step 4: Run** `ctest --test-dir build-gpu -R frame_dump --output-on-failure`. Expected: passes. Then run the full build: `gt_native` and `gt_play` link.
- [ ] **Step 5: Smoke run.** Run `env SDL_VIDEODRIVER=offscreen PSPRECOMP_CONFIG=/tmp/claude-1000/gpu_gate.ini PSPRECOMP_GE_GPU_SKIP_SOFTWARE_RASTER=0 GT_GPU_DUMP=recomp/analysis/gpu/smoke GT_FRAME_EVERY=30 build-gpu/gt_play --max-seconds 20` (lock and cgroup), then `ls recomp/analysis/gpu/smoke | head`.
  Expected: both `gpu_` and `soft_` files, the same indices, 480×272.
- [ ] **Step 6: Boot check.** `gt_native` still runs the race route unchanged: `build-gpu/gt_native --max-seconds 60` with `GT_PAD` from `config/pad_routes/race.pad` and exit 0. Its frame dumps (`GT_FRAME_DUMP`) are byte-identical to the main checkout's build for the same run settings, because the refactor must not change them.
- [ ] **Step 7: Commit.** `feat(host): GT_GPU_DUMP — paired GPU/software frames per presented frame`

### Task 4: `tools/gpu_gate.sh` and the sabotage proof

**Files:**
- Create: `tools/gpu_gate.sh`. Usage: `tools/gpu_gate.sh BUILD_DIR OUT_DIR [SCALE]`. For race:600, dealer:300 and garage:310 (the routes `natcheck2` uses) it runs `gt_play` offscreen with `NativeGE=true`, `InternalScale=SCALE` (default 2), `PSPRECOMP_GE_GPU_SKIP_SOFTWARE_RASTER=0`, `GT_PAD` from `config/pad_routes/<route>.pad`, `GT_GPU_DUMP=OUT_DIR/<route>` and `GT_FRAME_EVERY=30`. It runs one route at a time, under the cgroup and the lock. Then it runs `python -m tools.gpu_compare OUT_DIR/<route> --report OUT_DIR/<route>.txt --diff-dir OUT_DIR/<route>-diff`. It exits non-zero if any route fails.

- [ ] **Step 1: Prove the gate can fail.** Make a temporary local edit to the worktree's `tools/src/lcs-recomp/lcs/host/vulkan/ge.frag` that outputs a constant colour (never committed). Rebuild and run only the race route.
  Expected: `gpu_compare` fails nearly every frame. Revert with `git -C tools/src/lcs-recomp checkout -- lcs/host/vulkan/ge.frag` and re-apply the patch marker check (`tools/setup.sh`). Write down the failing summary line.
- [ ] **Step 2: Run the real measurement** at scale 2 on all three routes: `tools/gpu_gate.sh build-gpu recomp/analysis/gpu/base 2`.
  Expected: it fails, as the spec predicts. Keep the per-route summaries and look at the worst-frame diff images.
- [ ] **Step 3: Classify the failures.** For each of the 10 worst frames per route, find the draws involved. Use `GT_GE_TRACE` / `host/ge_trace.cpp`, or log the blend state and texture address in a temporary local patch. Classify each as one of: blending (factor or equation outside the 5 variants), render-to-texture or write-back, LCS shortcut (HUD/widescreen), or other. This is measurement only; no fixes.
- [ ] **Step 4: Write the report.** Add a FINDINGS entry, "What the Vulkan GE backend draws wrong on GT", with the per-route counts and the classification. Update ROADMAP: the GPU item gets a "Fase 1 (medir)" sub-item done with the numbers, and the phase 2–4 items listed. In COMPILAR, add `tools/gpu_gate.sh` under the gt_play notes as the GPU check. In AGENTS.md § Commands, add `tools/gpu_gate.sh` to the runnable list.
- [ ] **Step 5: Commit, push and open the PR.** Commit as `feat(tools): GPU gate — gpu_gate.sh + first report on GT (race/dealer/garage)`. Push `feat/gpu-ge-backend` and open a PR with "How to test manually":
  - Setup, extract, recomp, build.
  - `tools/gpu_gate.sh build-gpu recomp/analysis/gpu/base 2`, with the expected summary lines (red, as measured).
  - `ctest` and pytest results.
  - The boot check.

## After Phase 1

Phases 2–4 (blending and LCS shortcuts, write-back, speed and default) get their own plan written from this report. The spec's list of fixes is a hypothesis, and the report decides which ones are real and in what order.

## Rulings during execution

- **Task 3:** the correctness gate compares at `InternalScale=1`. At ×2 the downsampled GPU frame
  anti-aliases every edge (text, sprite borders) that the software path draws hard. A smoke run's
  legal-notice frame differed in 12 % of its pixels with identical content. That is an artefact of the
  scale, not a rendering error. ×2–×4 are used for the performance numbers and for visual checks, not for
  pixel equality. Cost if wrong: a scale-specific bug (e.g. a scissor not scaled) escapes the gate, so
  phase 4 adds one ×2 run whose block count (missing elements) must stay 0.
- **Task 3:** `host/tests/image_ppm_test.cpp` first used an `initializer_list` inside a conditional in a
  range-for. The backing array dies at the end of the full-expression, so it passed at -O0 and failed at
  -O3. The test was fixed; the implementation was unchanged.
