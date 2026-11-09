"""Verdict of tools/mod_camera_check.sh: compares the frames of gt_play runs with the orbit-camera mod (GT_MOD_CAMERA_FAKE=<yaw>)
against a baseline run without it (same pad route, so the same game state at the same frame number).

Frames are PPM (P6, 480x272). The chase route ends with three SELECT presses that cycle the view; the last one lands on the chase
view, where the car is seen from behind. The "chase frames" are the frames dumped from CHASE_AFTER_VBLANKS after the route's `script
done` line to the end of the run (frame -> vblank from the `[frame-dump]` lines of the baseline's run.log). Per yaw run:
  yaw 0     every common frame byte-identical to the baseline (the mod writes nothing at the identity);
  yaw != 0  on the chase frames the 3D area (rows above HUD_TOP) differs from the baseline, the HUD's speed digits (the mask of
            their cyan pixels in HUD_DIGITS) are identical (the strip itself is translucent over the 3D picture, so it is not
            compared pixel by pixel), and the car (orange pixels) is still there, its centroid within CENTRE_TOL_X / CENTRE_TOL_Y of
            the baseline's (the pivot is the car: the car does not swing out of the picture). The car check runs from
            CAR_CENTRE_MIN_YAW up: beside the car (yaw 90) the camera may sit beyond the trackside wall, which the game draws (and
            which hides the car) since the orbit is applied to the layer view (the wall is no longer culled for the unorbited heading).
Two real-time runs can slip one game step apart (the lap timer shows it), and the slip lasts to the end of the run: frames whose
lap-timer pixels (TIMER) differ from the baseline's are different game frames and are not compared pixel by pixel (identity, HUD
digits, after the toggle); at least MIN_RACE_FRAMES (identity, HUD digits) / MIN_TOGGLE_AFTER and TOGGLE_MIN_SHARE (after the toggle) must be on the
same game frame. After the toggle: frames from TOGGLE_MARGIN vblanks on, each paired (lap timer, PAIR_WINDOW) with a baseline frame, byte-identical to it
outside CAR_WINDOW and CLOCK_BOX (the HUD clock), at most TOGGLE_CAR_NOISE of CAR_WINDOW different (the car sprite flickers).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I does not put the script directory on the path
import lap_timer  # noqa: E402

WIDTH, HEIGHT = 480, 272
HUD_TOP = 232          # rows >= HUD_TOP are the bottom HUD strip (speed, gear, rev bar), translucent over the 3D picture
HUD_DIGITS = (200, 340, 234, 268)  # x0, x1, y0, y1 of the speed readout
CHASE_AFTER_VBLANKS = 150
SCRIPT_DONE = re.compile(r"script done at vblank (\d+)")
FRAME_DUMP = re.compile(r"\[frame-dump\] frame (\d+) vblank (\d+)(?: present_vblank (\d+))?")
CAR_WINDOW = (110, 330, 100, 232)  # x0, x1, y0, y1 where the player's car sits in the chase view (the orange TRD banner is right of x = 340)
CAR_MIN_PIXELS = 150   # fewer orange pixels = no car in this frame
CAR_RATIO = (0.2, 3.0)   # the orbited frame's car pixel count over the baseline's (a side view shows more body, a far car less)
CAR_EXTENT_RATIO = (0.3, 2.5)  # same for the width and height of the car's pixel cloud (a track-sized brown area is far above it)
CENTRE_TOL_X, CENTRE_TOL_Y = 45, 40  # a side view (yaw 90) moves the colour centroid by ~40 px
DIFF_MIN_FRACTION = 0.05   # share of 3D-area pixels that must change on a race frame at yaw != 0
DIFF_MIN_FRAMES = 0.8      # share of race frames that must differ that much
MIN_RACE_FRAMES = 6
CAR_BAD_FRAMES = 0.1       # share of car frames allowed outside the car tolerances (the chase camera wobbles, shade changes the colours)
MIN_CAR_FRAMES = 3
CAR_CENTRE_MIN_YAW = 135  # below this the camera sits beside the car, where the game now draws (and the car is hidden by) the trackside wall
TIMER = lap_timer.TIMER  # the lap timer box; its strokes identify the game frame (tools/lap_timer.py)
PAIR_WINDOW = 4  # present vblanks around a frame inside which its baseline partner is looked for (the run may be a game step ahead or behind)
TOGGLE_CAR_NOISE = 0.03   # share of the CAR_WINDOW pixels that may differ after the toggle on the same game frame (the car's rear sprite flickers by a few levels);
                          # everything outside CAR_WINDOW (sky, track, HUD, corners) must be byte-identical: an inactive mod writes nothing
CLOCK_BOX = (460, 480, 250, 272)  # x0, x1, y0, y1 of the HUD clock ("AM 00:02"): its digit flips at an instant that moves by a frame between two runs (the clock is
                                   # not locked to the game frame: it flipped one frame apart on same-lap-timer pairs, every 7 200 vblanks); excluded from the byte check
MIN_TOGGLE_AFTER = 30     # frames after the toggle that must pair with a baseline frame (a count) ...
TOGGLE_MIN_SHARE = 0.5    # ... and this share of the usable frames after the toggle: edits that break the pairing cannot pass on a handful of pairs
MIN_TOGGLE_BEFORE = 6     # orbited frames needed before the toggle

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
    return r >= 32 and r - g >= 20 and g - b >= 0


def car_box(frame):
    """(count, centroid_x, centroid_y, width, height) of the car-coloured pixels in CAR_WINDOW (width/height = the 5th..95th percentile
    span of the pixels, so a stray pixel does not stretch it); centroid None and zeros when there are none."""
    width, _height, pixels = frame
    x0, x1, y0, y1 = CAR_WINDOW
    xs, ys = [], []
    for y in range(y0, y1):
        base = (y * width) * 3
        for x in range(x0, x1):
            i = base + x * 3
            if is_orange(pixels[i], pixels[i + 1], pixels[i + 2]):
                xs.append(x)
                ys.append(y)
    count = len(xs)
    if count == 0:
        return 0, None, None, 0, 0
    xs.sort()
    ys.sort()
    lo, hi = count // 20, max(count * 19 // 20 - 1, count // 20)
    return count, sum(xs) / count, sum(ys) / count, xs[hi] - xs[lo] + 1, ys[hi] - ys[lo] + 1


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


def _key(frame, present_vblank):
    """A frame's key: the vblank its present was requested at when the log carries it, else its dump number (old logs)."""
    return int(present_vblank) if present_vblank else int(frame)


def keyed_frames(directory, log_text):
    """{key: path} of a run's frames. With `present_vblank` in the run's log the key is that vblank, so two runs pair the same game
    frame even when one of them dropped a present (dump numbers count presents: one drop shifts every later number). Without it (an
    old build, no log) the key is the dump number. The `vblank` label itself is no key: it is read when the GE worker dumps, which
    lags the game under load (docs/FINDINGS.md, 2026-11-09)."""
    frames = frames_of(directory)
    keys = {int(f): int(p) for f, _v, p in FRAME_DUMP.findall(log_text or "") if p}
    if not keys:
        return frames
    return {keys[n]: path for n, path in frames.items() if n in keys}


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
    base_timers, mod_timers = run_timers({i: base_frames[i] for i in common}), run_timers({i: mod_frames[i] for i in common})
    same = 0
    for index in common:
        if not lap_timer.same_game_frame(base_timers[index], mod_timers[index]):
            continue
        same += 1
        if read_ppm(base_frames[index])[2] != read_ppm(mod_frames[index])[2]:
            return False, f"frame {index} differs from the baseline at yaw 0 (the mod must write nothing at the identity)"
    if same < MIN_RACE_FRAMES:  # a count, not a share: a one-step slip between two real-time runs persists to the end of the run
        return False, f"only {same}/{len(common)} frames show the baseline's same game frame (lap timer): nothing proved"
    return True, f"{same}/{len(common)} frames on the same game frame byte-identical to the baseline (the rest slipped a step)"


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


def timer_mask(frame):
    return lap_timer.timer_mask(frame)


def same_game_frame(base, mod):
    """Two frames whose lap-timer strokes are the same, and usable (enough strokes: not a menu)."""
    a, b = lap_timer.timer_mask(base), lap_timer.timer_mask(mod)
    return len(a) >= lap_timer.MIN_PIXELS and a == b


def run_timers(frames):
    """{key: lap-timer mask or None} of a run's frames (None: no timer on screen, or frozen: it identifies nothing)."""
    return lap_timer.run_timers(frames, read_ppm)


def chase_frame_indices(log_text):
    """Frame numbers dumped from CHASE_AFTER_VBLANKS after `script done` (empty when the log has no such line)."""
    done = SCRIPT_DONE.search(log_text)
    if done is None:
        return []
    start = int(done.group(1)) + CHASE_AFTER_VBLANKS
    return [_key(f, p) for f, v, p in FRAME_DUMP.findall(log_text) if int(p or v) >= start]


def frames_in_vblanks(log_text, lo, hi):
    """Frame numbers dumped at a vblank in [lo, hi)."""
    return [_key(f, p) for f, v, p in FRAME_DUMP.findall(log_text) if lo <= int(p or v) < hi]


TOGGLE_MARGIN = 600  # vblanks around the toggle that are not judged: the frame in flight, and the settling of the picture (no colour clear: pixels drawn by blending keep
                     # the orbited frames' content until they are redrawn; measured: byte-identical outside the car window from ~500 vblanks after the toggle on)
TOGGLE_LINE = re.compile(r"^\[mods\] \S+ -> OFF$", re.M)


def pair_by_timer(base_timers, key, mod_mask):
    """The baseline key within PAIR_WINDOW of `key` (the nearest first) whose lap-timer strokes equal `mod_mask`, or None. The timer identifies
    the game frame (1/60 s): a run a step ahead or behind the baseline at the same present vblank still pairs with the baseline frame showing the
    same game frame; a frame of another moment, a missing timer (None) or a frozen one pairs with nothing."""
    if mod_mask is None:
        return None
    for distance in sorted(range(-PAIR_WINDOW, PAIR_WINDOW + 1), key=abs):
        if lap_timer.same_game_frame(base_timers.get(key + distance), mod_mask):
            return key + distance
    return None


def diff_outside_window(a, b):
    """(pixels differing outside CAR_WINDOW and CLOCK_BOX, share of the CAR_WINDOW pixels that differ) of two frames."""
    width, height, pa = a
    pb = b[2]
    if pa == pb:
        return 0, 0.0
    x0, x1, y0, y1 = CAR_WINDOW
    outside = inside = 0
    for y in range(height):
        base = y * width * 3
        if pa[base:base + width * 3] == pb[base:base + width * 3]:
            continue  # most rows are identical: only the rows that differ are walked pixel by pixel
        for x in range(width):
            i = base + x * 3
            if pa[i:i + 3] != pb[i:i + 3]:
                if x0 <= x < x1 and y0 <= y < y1:
                    inside += 1
                elif not (CLOCK_BOX[0] <= x < CLOCK_BOX[1] and CLOCK_BOX[2] <= y < CLOCK_BOX[3]):
                    outside += 1
    return outside, inside / ((x1 - x0) * (y1 - y0))


def judge_toggle(base_frames, mod_frames, before, after, run_log):
    """The F9 menu turned the mod off mid-chase (GT_MOD_MENU_TEST_TOGGLE): frames before differ from the baseline (orbited); each frame after it that
    pairs with a baseline frame showing the same game frame (lap timer, within PAIR_WINDOW present vblanks) is byte-identical to it outside CAR_WINDOW and CLOCK_BOX
    and within TOGGLE_CAR_NOISE inside CAR_WINDOW (an inactive mod's hooks pass straight to the original). At least MIN_TOGGLE_AFTER and TOGGLE_MIN_SHARE of the
    usable frames after the toggle must pair. Returns (ok, message)."""
    if TOGGLE_LINE.search(run_log) is None:
        return False, "no `[mods] <id> -> OFF` line in the run log: the menu did not toggle the mod"
    before = [i for i in before if i in base_frames and i in mod_frames]
    after = [i for i in after if i in mod_frames]
    if len(before) < MIN_TOGGLE_BEFORE or len(after) < MIN_TOGGLE_AFTER:
        return False, f"too few frames around the toggle ({len(before)} before, {len(after)} after; need {MIN_TOGGLE_BEFORE} / {MIN_TOGGLE_AFTER})"
    orbited = sum(1 for i in before if diff_fraction(read_ppm(base_frames[i]), read_ppm(mod_frames[i]), 0, HUD_TOP) >= DIFF_MIN_FRACTION)
    if orbited < DIFF_MIN_FRAMES * len(before):
        return False, f"before the toggle only {orbited}/{len(before)} frames are orbited"
    base_timers, mod_timers = run_timers(base_frames), run_timers(mod_frames)
    usable = [i for i in after if mod_timers.get(i) is not None]
    same = 0
    for i in usable:
        partner = pair_by_timer(base_timers, i, mod_timers[i])
        if partner is None:
            continue
        same += 1
        outside, inside = diff_outside_window(read_ppm(base_frames[partner]), read_ppm(mod_frames[i]))
        if outside > 0 or inside > TOGGLE_CAR_NOISE:
            return False, (f"frame {i} after the toggle differs from the baseline ({outside} px outside the car window and the HUD clock, {inside:.1%} of it inside): "
                           "the mod still edits while inactive")
    if same < MIN_TOGGLE_AFTER or same < TOGGLE_MIN_SHARE * len(usable):
        return False, (f"after the toggle only {same}/{len(usable)} usable frames pair with a baseline frame on the same game frame "
                       f"(need {MIN_TOGGLE_AFTER} and {TOGGLE_MIN_SHARE:.0%}): nothing proved")
    return True, f"{orbited}/{len(before)} frames orbited before the toggle, {same}/{len(usable)} frames after it on the same game frame byte-identical outside the car window and the HUD clock"


def judge_orbit(base_frames, mod_frames, indices, yaw=180.0):
    """yaw != 0 on the frames `indices` (those present in both runs). Returns (ok, message)."""
    frames = [i for i in indices if i in base_frames and i in mod_frames]
    if len(frames) < MIN_RACE_FRAMES:
        return False, f"only {len(frames)} chase frames (the run is too short or the log has no `script done`)"
    differing = car_frames = digits_compared = 0
    problems, car_problems = [], []
    base_timers, mod_timers = run_timers({i: base_frames[i] for i in frames}), run_timers({i: mod_frames[i] for i in frames})
    for index in frames:
        base, mod = read_ppm(base_frames[index]), read_ppm(mod_frames[index])
        if diff_fraction(base, mod, 0, HUD_TOP) >= DIFF_MIN_FRACTION:
            differing += 1
        if lap_timer.same_game_frame(base_timers[index], mod_timers[index]):  # the HUD digits are compared only on the same game frame
            digits_compared += 1
            if digits_mask(base) != digits_mask(mod):
                problems.append(f"frame {index}: the HUD's speed digits changed")
        count, cx, cy, bw, bh = car_box(base)
        if count >= CAR_MIN_PIXELS and abs(yaw) >= CAR_CENTRE_MIN_YAW:
            car_frames += 1
            mcount, mx, my, mw, mh = car_box(mod)
            if mx is None or mcount < CAR_RATIO[0] * count:
                car_problems.append(f"frame {index}: the car left the picture ({mcount} car-coloured pixels, baseline {count})")
            elif mcount > CAR_RATIO[1] * count or not (CAR_EXTENT_RATIO[0] * bw <= mw <= CAR_EXTENT_RATIO[1] * bw
                                                         and CAR_EXTENT_RATIO[0] * bh <= mh <= CAR_EXTENT_RATIO[1] * bh):
                car_problems.append(f"frame {index}: the car-coloured area is not car-sized ({mcount} px, {mw}x{mh}; baseline {count} px, {bw}x{bh})")
            elif abs(mx - cx) > CENTRE_TOL_X or abs(my - cy) > CENTRE_TOL_Y:
                car_problems.append(f"frame {index}: the car moved to ({mx:.0f},{my:.0f}), baseline ({cx:.0f},{cy:.0f})")
    if car_frames < MIN_CAR_FRAMES and abs(yaw) >= CAR_CENTRE_MIN_YAW:
        return False, f"the car is visible on only {car_frames} baseline chase frames (need {MIN_CAR_FRAMES})"
    if digits_compared < MIN_RACE_FRAMES:
        return False, f"the HUD digits were compared on only {digits_compared}/{len(frames)} chase frames (same game frame by the lap timer; need {MIN_RACE_FRAMES}): nothing proved"
    if differing < DIFF_MIN_FRAMES * len(frames):
        return False, f"the 3D area changed on only {differing}/{len(frames)} chase frames: the mod did not orbit"
    if len(car_problems) > CAR_BAD_FRAMES * car_frames:
        return False, f"the car is off on {len(car_problems)}/{car_frames} car frames (allowed {CAR_BAD_FRAMES:.0%}), first: {car_problems[0]}"
    if problems:
        return False, f"{len(problems)} problems, first: {problems[0]}"
    return True, (f"{len(frames)} chase frames ({car_frames} with the car): 3D differs on {differing}, HUD digits identical, "
                  f"car centred on {car_frames - len(car_problems)}/{car_frames}")


def main(argv):
    out = argv[1]
    with open(os.path.join(out, "baseline", "run.log"), errors="replace") as handle:
        base_log = handle.read()
    base = keyed_frames(os.path.join(out, "baseline", "frames"), base_log)
    indices = chase_frame_indices(base_log)
    failed = False
    args = argv[2:]
    toggle_at = None
    if "--toggle-at" in args:
        at = args.index("--toggle-at")
        toggle_at = int(args[at + 1])
        del args[at:at + 2]
    orbit_labels = [label for label in args if not label.startswith("toggle_")]
    if orbit_labels and "yaw_0" not in orbit_labels:  # the determinism control: yaw 0 must be byte-identical to the baseline
        print("FAIL: no yaw_0 run among the labels (the determinism control is mandatory)")
        failed = True
    # the car-centring check (the pivot) runs from CAR_CENTRE_MIN_YAW up only: a run without such a yaw would pass a broken pivot
    if orbit_labels and not any(abs(float(label.split("_", 1)[1])) >= CAR_CENTRE_MIN_YAW for label in orbit_labels):
        print(f"FAIL: no yaw >= {CAR_CENTRE_MIN_YAW:g} among the labels (the car-centring check needs one)")
        failed = True
    for label in args:
        yaw = float(label.split("_", 1)[1])
        log_path = os.path.join(out, label, "run.log")
        run_log = ""
        if os.path.exists(log_path):
            with open(log_path, errors="replace") as handle:
                run_log = handle.read()
        mod = keyed_frames(os.path.join(out, label, "frames"), run_log)
        if label.startswith("toggle_"):
            start = int(SCRIPT_DONE.search(base_log).group(1)) + CHASE_AFTER_VBLANKS
            before = frames_in_vblanks(base_log, start, toggle_at - TOGGLE_MARGIN)
            after = frames_in_vblanks(run_log if FRAME_DUMP.search(run_log) else base_log, toggle_at + TOGGLE_MARGIN, 10 ** 9)  # the run's own frames: they pair by timer
            ok, message = judge_toggle(base, mod, before, after, run_log)
        else:
            ok, message = judge_identity(base, mod) if yaw == 0 else judge_orbit(base, mod, indices, yaw)
        print(("PASS " if ok else "FAIL ") + f"{label}: {message}")
        failed = failed or not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
