#!/usr/bin/env python3
"""Judge of tools/mod_deformation_check.sh: frames of the run without the mod (baseline) against the run with it.

  mod_deformation_check.py OUT deform     the hit, then a dent: frames before the hit byte-identical to the baseline (mod switched off), frames after it
                                          differ from the zero-strength run (the same mod, depth_per_ms = 0; real time shifts the race after the hit)
  mod_deformation_check.py OUT identity   the mod installed but switched off: every frame byte-identical to the baseline
Exit 0 = pass, 1 = fail."""
import os
import re
import sys

FRAME = re.compile(r"frame_(\d+)\.ppm$")
DUMP = re.compile(r"\[frame-dump\] frame (\d+) vblank (\d+)")
HIT = re.compile(r"\[mod:deformation\] HIT step=(\d+) vblank=(\d+)")
MIN_DIFF_PIXELS = 200  # a dent moves hundreds of pixels; render noise would be zero (runs are deterministic)


def read_ppm(path):
    with open(path, "rb") as handle:
        data = handle.read()
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    if match is None:
        raise ValueError(f"{path}: not a P6 PPM")
    return data[match.end():]


def frames(directory):
    out = {}
    for name in os.listdir(directory):
        match = FRAME.search(name)
        if match:
            out[int(match.group(1))] = os.path.join(directory, name)
    return out


def differing_pixels(a, b):
    return sum(1 for i in range(0, min(len(a), len(b)), 3) if a[i:i + 3] != b[i:i + 3]) + abs(len(a) - len(b)) // 3


def main():
    out, mode = sys.argv[1], sys.argv[2]
    base = frames(os.path.join(out, "baseline", "frames"))
    run_dir = os.path.join(out, "identity" if mode == "identity" else "run")
    run = frames(os.path.join(run_dir, "frames"))
    log = open(os.path.join(run_dir, "run.log"), errors="replace").read()
    common = sorted(set(base) & set(run))
    if len(common) < 10:
        print(f"FAIL: only {len(common)} common frames")
        return 1
    if mode == "identity":
        for index in common:
            if read_ppm(base[index]) != read_ppm(run[index]):
                print(f"FAIL: frame {index} differs from the baseline with the mod switched off")
                return 1
        print(f"ok: {len(common)} frames byte-identical to the baseline with the mod switched off")
        return 0
    hit = HIT.search(log)
    if hit is None:
        print("FAIL: no HIT line (the car never hit the barrier)")
        return 1
    hit_vblank = int(hit.group(2))
    vblank_of = {int(m.group(1)): int(m.group(2)) for m in DUMP.finditer(log)}
    before = [i for i in common if i in vblank_of and vblank_of[i] < hit_vblank - 30]
    after = [i for i in common if i in vblank_of and vblank_of[i] > hit_vblank + 60]
    if len(before) < 5 or len(after) < 3:
        print(f"FAIL: too few frames around the hit (before {len(before)}, after {len(after)}; hit at vblank {hit_vblank})")
        return 1
    for index in before:
        if read_ppm(base[index]) != read_ppm(run[index]):
            print(f"FAIL: frame {index} (vblank {vblank_of[index]}) differs from the baseline before the hit")
            return 1
    zero_dir = os.path.join(out, "zero")
    if not os.path.isdir(os.path.join(zero_dir, "frames")):
        print("FAIL: no zero-strength run to compare the frames after the hit with")
        return 1
    zero = frames(os.path.join(zero_dir, "frames"))
    zero_vblank = {int(m.group(1)): int(m.group(2)) for m in DUMP.finditer(open(os.path.join(zero_dir, "run.log"), errors="replace").read())}
    after = [i for i in after if i in zero and zero_vblank.get(i) == vblank_of[i]]
    if len(after) < 3:
        print("FAIL: too few frames after the hit shared with the zero-strength run")
        return 1
    changed = [(differing_pixels(read_ppm(zero[i]), read_ppm(run[i])), i) for i in after]
    best = max(changed)
    if best[0] < MIN_DIFF_PIXELS:
        print(f"FAIL: changed after hit: no frame after the hit differs from the zero-strength run by {MIN_DIFF_PIXELS} pixels (max {best[0]})")
        return 1
    print(f"ok: {len(before)} frames before the hit identical; after it {sum(1 for c in changed if c[0] >= MIN_DIFF_PIXELS)}/{len(after)} frames differ "
          f"(most: {best[0]} pixels, frame {best[1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
