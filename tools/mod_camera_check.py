"""Verdict of tools/mod_camera_check.sh: compares the frames of gt_play runs with the orbit-camera mod (GT_MOD_CAMERA_FAKE=<yaw>)
against a baseline run without it (same pad route, so the same game state at the same frame number).

Frames are PPM (P6, 480x272). The chase route ends with three SELECT presses that cycle the view; the last one lands on the chase
view, where the car is seen from behind. The "chase frames" are the frames dumped from CHASE_AFTER_VBLANKS after the route's `script
done` line to the end of the run (frame -> vblank from the `[frame-dump]` lines of the baseline's run.log). Per yaw run:
  yaw 0     every common frame byte-identical to the baseline (the mod writes nothing at the identity);
  yaw != 0  on the chase frames the 3D area (rows above HUD_TOP) differs from the baseline, the HUD's speed digits (the mask of
            their cyan pixels in HUD_DIGITS) are identical (the strip itself is translucent over the 3D picture, so it is not
            compared pixel by pixel), and the car (orange pixels) is still there, its centroid within CENTRE_TOL_X / CENTRE_TOL_Y of
            the baseline's (the pivot is the car: the car does not swing out of the picture).
"""
import os
import re
import sys

WIDTH, HEIGHT = 480, 272
HUD_TOP = 232          # rows >= HUD_TOP are the bottom HUD strip (speed, gear, rev bar), translucent over the 3D picture
HUD_DIGITS = (200, 340, 234, 268)  # x0, x1, y0, y1 of the speed readout
CHASE_AFTER_VBLANKS = 150
SCRIPT_DONE = re.compile(r"script done at vblank (\d+)")
FRAME_DUMP = re.compile(r"\[frame-dump\] frame (\d+) vblank (\d+)")
CAR_WINDOW = (110, 330, 100, 232)  # x0, x1, y0, y1 where the player's car sits in the chase view (the orange TRD banner is right of x = 340)
CAR_MIN_PIXELS = 150   # fewer orange pixels = no car in this frame
CAR_KEEP_RATIO = 0.15  # the orbited frame keeps at least this share of the baseline's car pixels
CENTRE_TOL_X, CENTRE_TOL_Y = 48, 36
DIFF_MIN_FRACTION = 0.05   # share of 3D-area pixels that must change on a race frame at yaw != 0
DIFF_MIN_FRAMES = 0.8      # share of race frames that must differ that much
MIN_RACE_FRAMES = 6
MIN_CAR_FRAMES = 3

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
    """The player's car in the check's route is a red-brown/salmon body (bright in the sun, dark in shade): red clearly above green,
    green not below blue. Grey road, green grass, blue sky, the cyan HUD digits and dark windows do not match."""
    return r >= 24 and r - g >= 16 and g - b >= -4


def car_box(frame):
    """(count, centroid_x, centroid_y) of orange pixels in CAR_WINDOW; centroid None when there are none."""
    width, _height, pixels = frame
    x0, x1, y0, y1 = CAR_WINDOW
    count = sx = sy = 0
    for y in range(y0, y1):
        base = (y * width) * 3
        for x in range(x0, x1):
            i = base + x * 3
            if is_orange(pixels[i], pixels[i + 1], pixels[i + 2]):
                count += 1
                sx += x
                sy += y
    if count == 0:
        return 0, None, None
    return count, sx / count, sy / count


def diff_fraction(a, b, y0, y1):
    """Share of pixels in rows y0..y1 that differ (any channel)."""
    width = a[0]
    lo, hi = y0 * width * 3, y1 * width * 3
    pa, pb = a[2][lo:hi], b[2][lo:hi]
    if pa == pb:
        return 0.0
    n = len(pa) // 3
    differing = sum(1 for i in range(0, len(pa), 3) if pa[i:i + 3] != pb[i:i + 3])
    return differing / n


def frames_of(directory):
    out = {}
    for name in os.listdir(directory):
        match = FRAME.search(name)
        if match:
            out[int(match.group(1))] = os.path.join(directory, name)
    return out


def judge_identity(base_frames, mod_frames):
    """yaw 0: every common frame byte-identical. Returns (ok, message)."""
    common = sorted(set(base_frames) & set(mod_frames))
    if len(common) < MIN_RACE_FRAMES:
        return False, f"only {len(common)} common frames"
    for index in common:
        if read_ppm(base_frames[index])[2] != read_ppm(mod_frames[index])[2]:
            return False, f"frame {index} differs from the baseline at yaw 0 (the mod must write nothing at the identity)"
    return True, f"{len(common)} frames byte-identical to the baseline"


def digits_mask(frame):
    """Positions of the speed readout's cyan digit pixels (light cyan: little red, strong green and blue)."""
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


def chase_frame_indices(log_text):
    """Frame numbers dumped from CHASE_AFTER_VBLANKS after `script done` (empty when the log has no such line)."""
    done = SCRIPT_DONE.search(log_text)
    if done is None:
        return []
    start = int(done.group(1)) + CHASE_AFTER_VBLANKS
    return [int(f) for f, v in FRAME_DUMP.findall(log_text) if int(v) >= start]


def frames_in_vblanks(log_text, lo, hi):
    """Frame numbers dumped at a vblank in [lo, hi)."""
    return [int(f) for f, v in FRAME_DUMP.findall(log_text) if lo <= int(v) < hi]


TOGGLE_MARGIN = 150  # vblanks around the toggle that are not judged (the frame in flight, the list already built)
TOGGLE_LINE = re.compile(r"^\[mods\] \S+ -> OFF$", re.M)


def judge_toggle(base_frames, mod_frames, before, after, run_log):
    """The F9 menu turned the mod off mid-chase (GT_MOD_MENU_TEST_TOGGLE): frames before differ from the baseline (orbited), frames
    after are byte-identical to it (an inactive mod's hooks pass straight to the original). Returns (ok, message)."""
    if TOGGLE_LINE.search(run_log) is None:
        return False, "no `[mods] <id> -> OFF` line in the run log: the menu did not toggle the mod"
    before = [i for i in before if i in base_frames and i in mod_frames]
    after = [i for i in after if i in base_frames and i in mod_frames]
    if len(before) < MIN_CAR_FRAMES or len(after) < MIN_CAR_FRAMES:
        return False, f"too few frames around the toggle ({len(before)} before, {len(after)} after)"
    orbited = sum(1 for i in before if diff_fraction(read_ppm(base_frames[i]), read_ppm(mod_frames[i]), 0, HUD_TOP) >= DIFF_MIN_FRACTION)
    if orbited < DIFF_MIN_FRAMES * len(before):
        return False, f"before the toggle only {orbited}/{len(before)} frames are orbited"
    for i in after:
        if read_ppm(base_frames[i])[2] != read_ppm(mod_frames[i])[2]:
            return False, f"frame {i} after the toggle differs from the baseline: the mod still edits while inactive"
    return True, f"{orbited}/{len(before)} frames orbited before the toggle, {len(after)} frames identical to the baseline after it"


def judge_orbit(base_frames, mod_frames, indices):
    """yaw != 0 on the frames `indices` (those present in both runs). Returns (ok, message)."""
    frames = [i for i in indices if i in base_frames and i in mod_frames]
    if len(frames) < MIN_RACE_FRAMES:
        return False, f"only {len(frames)} chase frames (the run is too short or the log has no `script done`)"
    differing = car_frames = 0
    problems = []
    for index in frames:
        base, mod = read_ppm(base_frames[index]), read_ppm(mod_frames[index])
        if diff_fraction(base, mod, 0, HUD_TOP) >= DIFF_MIN_FRACTION:
            differing += 1
        if digits_mask(base) != digits_mask(mod):
            problems.append(f"frame {index}: the HUD's speed digits changed")
        count, cx, cy = car_box(base)
        if count >= CAR_MIN_PIXELS:
            car_frames += 1
            mcount, mx, my = car_box(mod)
            if mcount < CAR_KEEP_RATIO * count or mx is None:
                problems.append(f"frame {index}: the car left the picture ({mcount} orange pixels, baseline {count})")
            elif abs(mx - cx) > CENTRE_TOL_X or abs(my - cy) > CENTRE_TOL_Y:
                problems.append(f"frame {index}: the car moved to ({mx:.0f},{my:.0f}), baseline ({cx:.0f},{cy:.0f})")
    if car_frames < MIN_CAR_FRAMES:
        return False, f"the car is visible on only {car_frames} baseline chase frames (need {MIN_CAR_FRAMES})"
    if differing < DIFF_MIN_FRAMES * len(frames):
        return False, f"the 3D area changed on only {differing}/{len(frames)} chase frames: the mod did not orbit"
    if problems:
        return False, f"{len(problems)} problems, first: {problems[0]}"
    return True, f"{len(frames)} chase frames ({car_frames} with the car): 3D differs on {differing}, HUD digits identical, car centred"


def main(argv):
    out = argv[1]
    base = frames_of(os.path.join(out, "baseline", "frames"))
    with open(os.path.join(out, "baseline", "run.log"), errors="replace") as handle:
        indices = chase_frame_indices(handle.read())
    failed = False
    args = argv[2:]
    toggle_at = None
    if "--toggle-at" in args:
        at = args.index("--toggle-at")
        toggle_at = int(args[at + 1])
        del args[at:at + 2]
    base_log = open(os.path.join(out, "baseline", "run.log"), errors="replace").read()
    for label in args:
        yaw = float(label.split("_", 1)[1])
        mod = frames_of(os.path.join(out, label, "frames"))
        if label.startswith("toggle_"):
            run_log = open(os.path.join(out, label, "run.log"), errors="replace").read()
            start = int(SCRIPT_DONE.search(base_log).group(1)) + CHASE_AFTER_VBLANKS
            before = frames_in_vblanks(base_log, start, toggle_at - TOGGLE_MARGIN)
            after = frames_in_vblanks(base_log, toggle_at + TOGGLE_MARGIN, 10 ** 9)
            ok, message = judge_toggle(base, mod, before, after, run_log)
        else:
            ok, message = judge_identity(base, mod) if yaw == 0 else judge_orbit(base, mod, indices)
        print(("PASS " if ok else "FAIL ") + f"{label}: {message}")
        failed = failed or not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
