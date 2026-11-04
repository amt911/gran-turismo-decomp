#!/usr/bin/env bash
# Profile-guided build of gt_play (clang): an instrumented build in build-pgo-gen/, one race-route run that records the
# profile (headless, fresh save), the merged profile in recomp/analysis/pgo/gt_play.profdata, then BUILD_DIR configured
# with -DGT_PGO_PROFILE and built. Everything heavy runs inside the memory cgroup. The profile is derived from running
# the user's game and stays out of git.
#
# Usage: tools/pgo.sh [BUILD_DIR] [SECONDS]
#   BUILD_DIR  the build to optimise (default build)
#   SECONDS    length of the profiling run (default 220: menus and the first race)
set -euo pipefail
export GT_NO_MODS=1  # profile runs never load mods (a mods/ dir next to gt_play would change what is measured)
build=${1:-build}
seconds=${2:-220}
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"
command -v clang++ > /dev/null && command -v llvm-profdata > /dev/null || { echo "pgo: needs clang++ and llvm-profdata" >&2; exit 2; }
slice() { systemd-run --user --scope --quiet -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- "$@"; }
profiles=recomp/analysis/pgo
rm -rf "$profiles" && mkdir -p "$profiles"
echo "pgo: instrumented build (build-pgo-gen)"
cmake -S . -B build-pgo-gen -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DGT_PGO_GENERATE=ON -DGT_PGO_PROFILE= > /dev/null
slice cmake --build build-pgo-gen -j6 --target gt_play > "$profiles/build-gen.log" 2>&1 || { tail -5 "$profiles/build-gen.log"; exit 1; }
echo "pgo: profiling run (${seconds} s of the race route)"
GPU_PERF_ENV="LLVM_PROFILE_FILE=$repo/$profiles/%p.profraw" tools/gpu_perf.sh build-pgo-gen recomp/analysis/pgo-run 2 "$seconds" \
  > "$profiles/run.log" 2>&1 || { echo "pgo: the profiling run failed (recomp/analysis/pgo-run/run.log)" >&2; exit 1; }
llvm-profdata merge -o "$profiles/gt_play.profdata" "$profiles"/*.profraw
echo "pgo: optimised build ($build, -DGT_PGO_PROFILE=$repo/$profiles/gt_play.profdata)"
cmake -S . -B "$build" -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DGT_PGO_GENERATE=OFF -DGT_PGO_PROFILE="$repo/$profiles/gt_play.profdata" > /dev/null
slice cmake --build "$build" -j6 > "$profiles/build-use.log" 2>&1 || { tail -5 "$profiles/build-use.log"; exit 1; }
echo "pgo: done; $build/gt_play is optimised with the profile (re-run tools/pgo.sh after large changes)"
