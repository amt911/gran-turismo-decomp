"""Verdict of tools/mod_draw_check.sh: frames of gt_play runs with the draw-API probe mod (tools/mod_draw_probe) against a baseline run
with the probe installed but switched off (same pad route, so the same game state at the same frame number).

Labels (one run each):
  idle   the probe on, drawing nothing, no grade: every frame on the same game frame (lap timer) byte-identical to the baseline
         (the new API linked and unused changes no pixel);
  draw   the probe's overlay on: on the race frames the red rect is exactly red over its whole area, the line's centre is exactly green,
         the half-transparent blue rect is bluish over the game, the text draws white pixels inside its line box (and the same box in the
         baseline has few);
  grade  scene_grade(0.5, 0.5, 0.5, 1, 1) at init: frames paired with the baseline by lap timer are half as bright, pixel by pixel (+-2).
Frames are PPM (P6, 480x272), keyed by present vblank (tools/mod_camera_check.py keyed_frames).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I does not put the script directory on the path
import lap_timer  # noqa: E402
import mod_camera_check as cam  # noqa: E402

RED_RECT = (40, 40, 140, 100)       # x0, y0, x1, y1 (exclusive) of the opaque red rect
BLUE_RECT = (200, 200, 260, 240)    # half-transparent blue
TEXT_BOX = (198, 98, 340, 132)      # GT DRAW 0123 at (200, 100), size 24
LINE = ((300.0, 200.0), (440.0, 250.0))
MIN_TEXT_PIXELS = 120               # near-white pixels the text must add inside TEXT_BOX
MIN_GOOD_SHARE = 0.9                # share of the race frames that must pass every draw check
MIN_FRAMES = 6
GRADE = 0.5
GRADE_TOL = 2                       # per channel, in levels (rounding in the baseline-to-graded mapping)
GRADE_MIN_SHARE = 0.995             # share of pixels within the tolerance, per paired frame
MIN_GRADE_PAIRS = 6


def pixel(frame, x, y):
    w, _h, px = frame
    i = (y * w + x) * 3
    return px[i], px[i + 1], px[i + 2]


def near_white(p):
    return p[0] >= 235 and p[1] >= 235 and p[2] >= 235


def count_text(frame):
    x0, y0, x1, y1 = TEXT_BOX
    return sum(1 for y in range(y0, y1) for x in range(x0, x1) if near_white(pixel(frame, x, y)))


def draw_frame_problem(frame, baseline_frame):
    """None when every shape is where the probe put it, else what is wrong."""
    x0, y0, x1, y1 = RED_RECT
    bad = sum(1 for y in range(y0, y1) for x in range(x0, x1) if pixel(frame, x, y) != (255, 0, 0))
    if bad:
        return f"red rect: {bad} of {(x1 - x0) * (y1 - y0)} pixels are not exactly red"
    (ax, ay), (bx, by) = LINE
    for step in range(1, 14):  # centre of the line, away from the caps
        x = int(ax + (bx - ax) * step / 14)
        y = int(round(ay + (by - ay) * step / 14))
        if not any(pixel(frame, x, yy) == (0, 255, 0) for yy in (y - 1, y, y + 1)):
            return f"line: no exactly green pixel at x={x} around y={y}"
    x0, y0, x1, y1 = BLUE_RECT
    blue = 0
    for y in range(y0 + 2, y1 - 2):
        for x in range(x0 + 2, x1 - 2):
            r, g, b = pixel(frame, x, y)
            if b >= 127 and r <= 128 and g <= 128:
                blue += 1
    if blue < 0.97 * (x1 - x0 - 4) * (y1 - y0 - 4):
        return f"half-transparent rect: only {blue} bluish pixels"
    text, base_text = count_text(frame), count_text(baseline_frame) if baseline_frame is not None else 0
    if text < base_text + MIN_TEXT_PIXELS:
        return f"text: {text} near-white pixels in its box (baseline {base_text}), expected {MIN_TEXT_PIXELS} more"
    return None


def judge_draw(base, mod, indices):
    keys = [k for k in indices if k in mod]
    if len(keys) < MIN_FRAMES:
        return False, f"only {len(keys)} race frames"
    good, first = 0, None
    base_keys = sorted(base)
    for k in keys:
        near = min(base_keys, key=lambda b: abs(b - k)) if base_keys else None
        problem = draw_frame_problem(cam.read_ppm(mod[k]), cam.read_ppm(base[near]) if near is not None else None)
        if problem is None:
            good += 1
        elif first is None:
            first = f"frame {k}: {problem}"
    share = good / len(keys)
    if share < MIN_GOOD_SHARE:
        return False, f"{good}/{len(keys)} race frames show the shapes (need {MIN_GOOD_SHARE:.0%}); first problem: {first}"
    return True, f"{good}/{len(keys)} race frames show the red rect exactly, the green line, the blue rect and the text"


def read_doubled(path):
    """A graded frame with every channel doubled (clamped): the lap timer's white strokes (255 -> 128 under the grade) read as white again, so the frame can be
    paired with the baseline by its timer."""
    width, height, pixels = cam.read_ppm(path)
    return width, height, bytes(min(255, v * 2) for v in pixels)


def judge_grade(base, mod, indices):
    base_timers = cam.run_timers({k: base[k] for k in base})
    mod_timers = lap_timer.run_timers({k: mod[k] for k in mod}, read_doubled)
    pairs = 0
    for k in sorted(set(indices) & set(mod)):
        partner = cam.pair_by_timer(base_timers, k, mod_timers.get(k))
        if partner is None:
            continue
        a, b = cam.read_ppm(base[partner])[2], cam.read_ppm(mod[k])[2]
        within = sum(1 for i in range(len(a)) if abs(round(a[i] * GRADE) - b[i]) <= GRADE_TOL)
        if within / len(a) < GRADE_MIN_SHARE:
            return False, f"frame {k} (baseline {partner}): only {within / len(a):.3%} of the channels are within +-{GRADE_TOL} of half the baseline"
        pairs += 1
    if pairs < MIN_GRADE_PAIRS:
        return False, f"only {pairs} frames pair with a baseline frame (lap timer): nothing proved"
    return True, f"{pairs} frames on the same game frame are half the baseline's brightness (+-{GRADE_TOL}, >= {GRADE_MIN_SHARE:.1%} of channels)"


def read_log(path):
    return open(path, errors="replace").read() if os.path.exists(path) else ""


def main(argv):
    out = argv[1]
    base_log = read_log(os.path.join(out, "baseline", "run.log"))
    base = cam.keyed_frames(os.path.join(out, "baseline", "frames"), base_log)
    indices = cam.chase_frame_indices(base_log)
    failed = False
    for label in argv[2:]:
        run_log = read_log(os.path.join(out, label, "run.log"))
        mod = cam.keyed_frames(os.path.join(out, label, "frames"), run_log)
        if label == "idle":
            ok, message = cam.judge_identity(base, mod)
        elif label == "draw":
            ok, message = judge_draw(base, mod, indices)
        elif label == "grade":
            ok, message = judge_grade(base, mod, indices)
        else:
            ok, message = False, "unknown label"
        print(("PASS " if ok else "FAIL ") + f"{label}: {message}")
        failed = failed or not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
