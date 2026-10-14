#!/usr/bin/env bash
# CPU profile of a race with the Vulkan GE backend: runs gt_play headless on the race pad route (fresh save) under
# gperftools' CPU profiler, samples the race only (SECONDS_FROM to SECONDS_FROM + 40), and prints the samples by
# library and the hottest functions of gt_play. Build with `-g` on the sources you want resolved to lines.
#
# Usage: tools/ge_profile.sh BUILD_DIR OUT_DIR [SCALE] [SECONDS_FROM]
#   SCALE         InternalScale (default 1)
#   SECONDS_FROM  when the race is under way on the race route (default 170)
# Needs /usr/lib/libprofiler.so (gperftools). One run, inside the memory cgroup. OUT_DIR stays out of git.
set -u
build=${1:?build dir}
out=${2:?output dir}
scale=${3:-1}
from=${4:-170}
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
[ -x "$build/gt_play" ] || { echo "ge_profile: no $build/gt_play" >&2; exit 2; }
[ -e /usr/lib/libprofiler.so ] || { echo "ge_profile: gperftools' libprofiler.so is missing" >&2; exit 2; }
rm -rf "${out:?}" && mkdir -p "$out/savedata"
printf '[Rendering]\nBackend=Vulkan\nNativeGE=true\nInternalResolutionMode=Scale\nInternalScale=%s\n' "$scale" > "$out/gt_play.ini"
pad=$(grep -v '^#' config/pad_routes/race.pad)
systemd-run --user --scope --quiet -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- env SDL_VIDEODRIVER=offscreen \
  PSPRECOMP_CONFIG="$out/gt_play.ini" GT_SAVEDATA_DIR="$out/savedata" GT_PAD="$pad" LD_PRELOAD=/usr/lib/libprofiler.so \
  CPUPROFILE="$PWD/$out/race.prof" CPUPROFILESIGNAL=12 CPUPROFILE_FREQUENCY=1000 \
  "$build/gt_play" --max-seconds $((from + 50)) > "$out/run.log" 2>&1 &
sleep 3
pid=$(pgrep -n -x gt_play) || { echo "ge_profile: gt_play did not start" >&2; exit 2; }
sleep "$((from - 3))"; kill -12 "$pid"; sleep 40; kill -12 "$pid"; wait
python3 - "$out/race.prof.0" "$build/gt_play" <<'EOF'
import collections, re, struct, subprocess, sys
data = open(sys.argv[1], 'rb').read()
words = lambda off, n: struct.unpack_from(f'<{n}Q', data, off)
off, self_count, total = 40, collections.Counter(), 0
while True:
    count, depth = words(off, 2); off += 16
    pcs = words(off, depth); off += depth * 8
    if count == 0 and depth == 1 and pcs[0] == 0:
        break
    total += count
    self_count[pcs[0]] += count
maps = []
for line in data[off:].decode(errors='replace').splitlines():
    m = re.match(r'([0-9a-f]+)-([0-9a-f]+) (\S+) ([0-9a-f]+) \S+ \S+ +(.*)', line)
    if m:
        maps.append((int(m.group(1), 16), int(m.group(2), 16), int(m.group(4), 16), m.group(5)))
binary = sys.argv[2].split('/')[-1]
base = next(a for a, b, o, name in maps if name.endswith(binary) and o == 0)
libs = collections.Counter()
for pc, count in self_count.items():
    libs[next((name.split('/')[-1] for a, b, o, name in maps if a <= pc < b), '?')] += count
print('samples', total)
for name, count in libs.most_common(5):
    print(f'{100 * count / total:5.1f}% {name}')
mine = [pc for pc in self_count if any(a <= pc < b and name.endswith(binary) for a, b, o, name in maps)]
names = subprocess.run(['addr2line', '-f', '-C', '-e', sys.argv[2]] + [hex(pc - base) for pc in mine],
                       capture_output=True, text=True).stdout.splitlines()
by_function = collections.Counter()
for i, pc in enumerate(mine):
    by_function[names[2 * i].replace('(anonymous namespace)', 'anon').split('(')[0][:80]] += self_count[pc]
print('-- hottest functions of', binary)
for name, count in by_function.most_common(20):
    print(f'{100 * count / total:5.1f}% {name}')
EOF
