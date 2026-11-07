"""Verdict of tools/mod_drawdist_check.sh: compares gt_play runs with the draw-distance mod (GT_MOD_DRAWDIST_SCALE=<s>) against a
baseline run without it (same pad route, so the same game state at the same frame number).

Frames are PPM (P6, 480x272). The race frames are those dumped from RACE_AFTER_VBLANKS after the route's `script done` line (frame ->
vblank from the `[frame-dump]` lines of the baseline's run.log). Per run:
  scale 1.0  every common frame byte-identical to the baseline (the mod writes nothing at the identity);
  scale > 1  on the race frames the 3D area (rows above HUD_TOP) changes (the far scenery shows) on at least FAR_MIN_FRAMES of the
             frames, but only a bounded share of it (FAR_MAX_FRACTION: the composition is the same picture), the near field (the rows
             NEAR_ROWS: road and the player's car, all well inside the old far plane) stays identical but for depth speckles
             (NEAR_MAX_FRACTION), the HUD's speed digits are identical, and the car (orange pixels) is where it was.
  toggle     the F9 menu turned the mod off mid-race: frames before it differ from the baseline (the mod edited), frames after it are
             byte-identical (an inactive mod's hooks pass straight to the original).
"""
import os
import re
import sys

WIDTH, HEIGHT = 480, 272
HUD_TOP = 232                      # rows >= HUD_TOP are the bottom HUD strip
HUD_DIGITS = (200, 340, 234, 268)  # x0, x1, y0, y1 of the speed readout
NEAR_ROWS = (190, 232)             # the road in front of / under the car, in the 3D area
RACE_AFTER_VBLANKS = 150
SCRIPT_DONE = re.compile(r"script done at vblank (\d+)")
FRAME_DUMP = re.compile(r"\[frame-dump\] frame (\d+) vblank (\d+)")
CAR_WINDOW = (110, 330, 100, 232)
CAR_MIN_PIXELS = 150
CAR_KEEP_RATIO = 0.9               # the far-plane frame keeps at least this share of the baseline's car pixels (and at most 1/ratio)
CENTRE_TOL = 3
FAR_MIN_FRACTION = 0.0005          # share of 3D-area pixels that must change on a frame to count as "the far scenery changed"
FAR_MIN_FRAMES = 0.3          # share of race frames that must change that much (the far scenery is in view on some of the route's views only; ~50 % measured)
FAR_MAX_FRACTION = 0.35            # more than this share of the 3D area changing is a different picture, not a farther one
NEAR_MAX_FRACTION = 0.02           # depth speckles allowed in the near field (a larger far/near ratio thins the depth buffer)
MIN_RACE_FRAMES = 6
MIN_CAR_FRAMES = 3
TOGGLE_MARGIN = 150                # vblanks around the toggle that are not judged
TOGGLE_LINE = re.compile(r"^\[mods\] \S+ -> OFF$", re.M)
FRAME = re.compile(r"frame_(\d+)\.ppm$")


def read_ppm(path):
    with open(path, "rb") as handle:
        data = handle.read()
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    if match is None:
        raise ValueError(f"{path}: not a P6 PPM")
    width, height = int(match.group(1)), int(match.group(2))
    pixels = data[match.end():]
    if len(pixels) != width * height * 3:
        raise ValueError(f"{path}: size mismatch")
    return width, height, pixels


def is_orange(r, g, b):
    """The player's car: red clearly above green, green not below blue (grey road, green grass, blue sky, cyan HUD digits do not match)."""
    return r >= 24 and r - g >= 16 and g - b >= -4


def car_box(frame):
    """(count, centroid_x, centroid_y) of orange pixels in CAR_WINDOW; centroid None when there are none."""
    width, _height, pixels = frame
    x0, x1, y0, y1 = CAR_WINDOW
    count = sx = sy = 0
    for y in range(y0, y1):
        base = y * width * 3
        for x in range(x0, x1):
            i = base + x * 3
            if is_orange(pixels[i], pixels[i + 1], pixels[i + 2]):
                count += 1
                sx += x
                sy += y
    if count == 0:
        return 0, None, None
    return count, sx / count, sy / count


def digits_mask(frame):
    """Positions of the speed readout's cyan digit pixels."""
    width, _height, pixels = frame
    x0, x1, y0, y1 = HUD_DIGITS
    out = []
    for y in range(y0, y1):
        base = y * width * 3
        for x in range(x0, x1):
            i = base + x * 3
            r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
            if 100 < r < 205 and g > 200 and b > 215:
                out.append((x, y))
    return out


def diff_fraction(a, b, y0, y1):
    """Share of pixels in rows y0..y1 that differ (any channel)."""
    width = a[0]
    lo, hi = y0 * width * 3, y1 * width * 3
    pa, pb = a[2][lo:hi], b[2][lo:hi]
    if pa == pb:
        return 0.0
    n = len(pa) // 3
    return sum(1 for i in range(0, len(pa), 3) if pa[i:i + 3] != pb[i:i + 3]) / n


def frames_of(directory):
    out = {}
    for name in os.listdir(directory):
        match = FRAME.search(name)
        if match:
            out[int(match.group(1))] = os.path.join(directory, name)
    return out


def race_frame_indices(log_text):
    """Frame numbers dumped from RACE_AFTER_VBLANKS after `script done` (empty when the log has no such line)."""
    done = SCRIPT_DONE.search(log_text)
    if done is None:
        return []
    start = int(done.group(1)) + RACE_AFTER_VBLANKS
    return [int(f) for f, v in FRAME_DUMP.findall(log_text) if int(v) >= start]


def frames_in_vblanks(log_text, lo, hi):
    return [int(f) for f, v in FRAME_DUMP.findall(log_text) if lo <= int(v) < hi]


def judge_identity(base_frames, mod_frames):
    """scale 1.0: every common frame byte-identical. Returns (ok, message)."""
    common = sorted(set(base_frames) & set(mod_frames))
    if len(common) < MIN_RACE_FRAMES:
        return False, f"only {len(common)} common frames"
    for index in common:
        if read_ppm(base_frames[index])[2] != read_ppm(mod_frames[index])[2]:
            return False, f"frame {index} differs from the baseline at scale 1.0 (the mod must write nothing at the identity)"
    return True, f"{len(common)} frames byte-identical to the baseline"


def judge_far(base_frames, mod_frames, indices):
    """scale > 1 on the frames `indices` (those present in both runs). Returns (ok, message)."""
    frames = [i for i in indices if i in base_frames and i in mod_frames]
    if len(frames) < MIN_RACE_FRAMES:
        return False, f"only {len(frames)} race frames (the run is too short or the log has no `script done`)"
    changed = car_frames = 0
    worst_far = worst_near = 0.0
    problems = []
    for index in frames:
        base, mod = read_ppm(base_frames[index]), read_ppm(mod_frames[index])
        far = diff_fraction(base, mod, 0, HUD_TOP)
        near = diff_fraction(base, mod, *NEAR_ROWS)
        worst_far, worst_near = max(worst_far, far), max(worst_near, near)
        if far >= FAR_MIN_FRACTION:
            changed += 1
        if far > FAR_MAX_FRACTION:
            problems.append(f"frame {index}: {far:.1%} of the 3D area changed (a different picture, not a farther one)")
        if near > NEAR_MAX_FRACTION:
            problems.append(f"frame {index}: {near:.1%} of the near field changed (limit {NEAR_MAX_FRACTION:.0%})")
        if digits_mask(base) != digits_mask(mod):
            problems.append(f"frame {index}: the HUD's speed digits changed")
        count, cx, cy = car_box(base)
        if count >= CAR_MIN_PIXELS:
            car_frames += 1
            mcount, mx, my = car_box(mod)
            if mx is None or mcount < CAR_KEEP_RATIO * count or mcount > count / CAR_KEEP_RATIO:
                problems.append(f"frame {index}: the car changed ({mcount} orange pixels, baseline {count})")
            elif abs(mx - cx) > CENTRE_TOL or abs(my - cy) > CENTRE_TOL:
                problems.append(f"frame {index}: the car moved to ({mx:.1f},{my:.1f}), baseline ({cx:.1f},{cy:.1f})")
    if car_frames < MIN_CAR_FRAMES:
        return False, f"the car is visible on only {car_frames} baseline race frames (need {MIN_CAR_FRAMES})"
    if changed < FAR_MIN_FRAMES * len(frames):
        return False, f"the 3D area changed on only {changed}/{len(frames)} race frames: the mod did not move the far plane"
    if problems:
        return False, f"{len(problems)} problems, first: {problems[0]}"
    return True, (f"{len(frames)} race frames ({car_frames} with the car): far scenery changed on {changed} "
                  f"(worst {worst_far:.1%} of the 3D area), near field worst {worst_near:.2%}, HUD digits identical, car in place")


def judge_toggle(base_frames, mod_frames, before, after, run_log):
    """The menu turned the mod off mid-race. Returns (ok, message)."""
    if TOGGLE_LINE.search(run_log) is None:
        return False, "no `[mods] <id> -> OFF` line in the run log: the menu did not toggle the mod"
    before = [i for i in before if i in base_frames and i in mod_frames]
    after = [i for i in after if i in base_frames and i in mod_frames]
    if len(before) < MIN_CAR_FRAMES or len(after) < MIN_CAR_FRAMES:
        return False, f"too few frames around the toggle ({len(before)} before, {len(after)} after)"
    edited = sum(1 for i in before if diff_fraction(read_ppm(base_frames[i]), read_ppm(mod_frames[i]), 0, HUD_TOP) >= FAR_MIN_FRACTION)
    if edited < FAR_MIN_FRAMES * len(before):
        return False, f"before the toggle only {edited}/{len(before)} frames differ from the baseline"
    for i in after:
        if read_ppm(base_frames[i])[2] != read_ppm(mod_frames[i])[2]:
            return False, f"frame {i} after the toggle differs from the baseline: the mod still edits while inactive"
    return True, f"{edited}/{len(before)} frames edited before the toggle, {len(after)} frames identical to the baseline after it"


def main(argv):
    out = argv[1]
    base = frames_of(os.path.join(out, "baseline", "frames"))
    with open(os.path.join(out, "baseline", "run.log"), errors="replace") as handle:
        base_log = handle.read()
    indices = race_frame_indices(base_log)
    args = argv[2:]
    toggle_at = None
    if "--toggle-at" in args:
        at = args.index("--toggle-at")
        toggle_at = int(args[at + 1])
        del args[at:at + 2]
    failed = False
    for label in args:
        mod = frames_of(os.path.join(out, label, "frames"))
        if label.startswith("toggle_"):
            with open(os.path.join(out, label, "run.log"), errors="replace") as handle:
                run_log = handle.read()
            start = int(SCRIPT_DONE.search(base_log).group(1)) + RACE_AFTER_VBLANKS
            before = frames_in_vblanks(base_log, start, toggle_at - TOGGLE_MARGIN)
            after = frames_in_vblanks(base_log, toggle_at + TOGGLE_MARGIN, 10 ** 9)
            ok, message = judge_toggle(base, mod, before, after, run_log)
        else:
            scale = float(label.split("_", 1)[1])
            ok, message = judge_identity(base, mod) if scale == 1.0 else judge_far(base, mod, indices)
        print(("PASS " if ok else "FAIL ") + f"{label}: {message}")
        failed = failed or not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
