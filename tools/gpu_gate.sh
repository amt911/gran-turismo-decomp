#!/usr/bin/env bash
# GPU gate (docs/superpowers/specs/2026-10-12-gpu-ge-backend-design.md): runs gt_play headless on the
# three pad routes with the Vulkan GE backend AND the software rasterizer drawing the same frames, dumps
# the pairs (GT_GPU_DUMP) and compares them with tools/gpu_compare.py.
#
# Usage: tools/gpu_gate.sh BUILD_DIR OUT_DIR [SCALE] [ROUTES]
#   SCALE   InternalScale for the GPU (default 1: the pixel-equality gate; >1 only for speed and visual checks); the GPU frame is box-downsampled to 480x272.
#   ROUTES  space-separated route:seconds (default "race:600 dealer:300 garage:310").
# Exit code: 0 every route passes, 1 some route fails, 2 a run or the setup failed.
# The dumps are game output: OUT_DIR must stay out of git (use recomp/analysis/gpu/...).
# One route at a time, each inside the memory cgroup and the shared heavy-job lock.
set -u
build=${1:?build dir}
out=${2:?output dir}
scale=${3:-1}
routes=${4:-"race:600 dealer:300 garage:310"}
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
[ -x "$build/gt_play" ] || { echo "gpu_gate: no $build/gt_play" >&2; exit 2; }
mkdir -p "$out"
ini="$out/gt_play.ini"
cat > "$ini" <<EOF
[Rendering]
Backend=Vulkan
NativeGE=true
InternalResolutionMode=Scale
InternalScale=$scale
EOF
status=0
for entry in $routes; do
  route=${entry%:*}; seconds=${entry#*:}
  pad=$(grep -v '^#' "config/pad_routes/$route.pad") || { echo "gpu_gate: no pad route $route" >&2; exit 2; }
  rm -rf "${out:?}/$route" "${out:?}/$route-diff"
  flock /tmp/claude-1000/gt-heavy.lock systemd-run --user --scope --quiet -p MemoryHigh=5G -p MemoryMax=6G \
    -p MemorySwapMax=0 -- env SDL_VIDEODRIVER=offscreen PSPRECOMP_CONFIG="$ini" \
    PSPRECOMP_GE_GPU_DUAL=1 GT_FRAMESKIP=0 GT_GPU_NO_DROP=1 GT_PAD="$pad" GT_GPU_DUMP="$out/$route" \
    GT_FRAME_EVERY="${GT_FRAME_EVERY:-30}" timeout $((seconds + 300)) "$build/gt_play" --max-seconds "$seconds" \
    > "$out/$route.log" 2>&1
  rc=$?
  if [ $rc -ne 0 ]; then echo "gpu_gate: $route run exited $rc (log $out/$route.log)" >&2; status=2; continue; fi
  echo "== $route"
  .venv/bin/python -m tools.gpu_compare "$out/$route" --report "$out/$route.txt" --diff-dir "$out/$route-diff"
  rc=$?
  [ $rc -eq 2 ] && status=2
  [ $rc -eq 1 ] && [ $status -eq 0 ] && status=1
done
exit $status
