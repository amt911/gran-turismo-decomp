#!/usr/bin/env bash
# GPU cost of a race with the Vulkan GE backend: runs gt_play headless on the race pad route (fresh save) with
# GT_PERF_LOG and prints the medians, over the race's last windows, of the GPU's own time per frame (Vulkan
# timestamps), GE frames per second and the GE wait. Use it to measure a backend change against the one before.
#
# Usage: tools/gpu_perf.sh BUILD_DIR OUT_DIR [SCALE] [SECONDS] [WINDOWS]
#   SCALE    InternalScale (default 8: a big target keeps a fast GPU from idling at low clocks)
#   SECONDS  run length (default 220: the race route is racing from ~130 s)
#   WINDOWS  how many of the last 5-second windows to summarise (default 12)
# One run, inside the memory cgroup and the shared heavy-job lock. OUT_DIR stays out of git.
set -u
export GT_NO_MODS=1  # verification runs never load mods (a mods/ dir next to gt_play would change what is measured)
build=${1:?build dir}
out=${2:?output dir}
scale=${3:-8}
seconds=${4:-220}
windows=${5:-12}
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
[ -x "$build/gt_play" ] || { echo "gpu_perf: no $build/gt_play" >&2; exit 2; }
rm -rf "${out:?}" && mkdir -p "$out/savedata"
cat > "$out/gt_play.ini" <<EOF
[Rendering]
Backend=Vulkan
NativeGE=true
InternalResolutionMode=Scale
InternalScale=$scale
EOF
pad=$(grep -v '^#' config/pad_routes/race.pad)
flock /tmp/claude-1000/gt-heavy.lock systemd-run --user --scope --quiet -p MemoryHigh=5G -p MemoryMax=6G \
  -p MemorySwapMax=0 -- env SDL_VIDEODRIVER=offscreen PSPRECOMP_CONFIG="$out/gt_play.ini" GT_SAVEDATA_DIR="$out/savedata" \
  GT_PERF_LOG=1 ${GPU_PERF_ENV:-} GT_PAD="$pad" timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" > "$out/run.log" 2>&1
rc=$?
[ $rc -eq 0 ] || { echo "gpu_perf: run exited $rc (log $out/run.log)" >&2; exit 2; }
python3 - "$out/run.log" "$windows" <<'EOF'
import re, statistics, sys
text = open(sys.argv[1]).read().splitlines()
n = int(sys.argv[2])
gpu = [float(m.group(1)) for l in text if (m := re.search(r'GPU ([0-9.]+) ms/frame', l))][-n:]
ge = [float(m.group(1)) for l in text if (m := re.search(r'GE frames ([0-9.]+)/s', l))][-n:]
wait = [float(m.group(1)) for l in text if (m := re.search(r'GE wait ([0-9.]+)%', l))][-n:]
if not gpu or not ge or not wait:
    sys.exit("gpu_perf: no race [perf] lines in the log")
print(f"GPU {statistics.median(gpu):.3f} ms/frame (median of {len(gpu)}; min {min(gpu):.3f} max {max(gpu):.3f}) | "
      f"GE frames {statistics.median(ge):.1f}/s | GE wait {statistics.median(wait):.1f}%")
EOF
