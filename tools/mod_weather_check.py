"""Verdict of tools/mod_weather_check.sh: the weather mod (gt-mods repo) in headless gt_play runs of the `chase` route, frames dumped, against a run with the mod switched off.

The host's scene grade is c' = clamp(mix(luma(c * tint), c * tint, saturation) * brightness) per channel with c in 0..1 and luma = Rec.709 (include/gt_mod/gt_mod_api.h), and frame dumps apply it,
so a graded frame can be PREDICTED from the baseline frame of the same game frame (two runs of the same route are deterministic: the frames of the same present are byte-identical, the camera and
telemetry checks rely on it). The mod logs `GRADE vb=<v> steps=<n> mode=<m> rain=<r> rgb=r,g,b bright=<b> sat=<s>` whenever its grade changes and `RAIN vb=.. level=.. streaks=..`.
Judged here:
  idle         preset noon, rain 0: every common frame byte-identical to the switched-off run (the neutral grade changes nothing, nothing is drawn);
  preset       (dusk) the race frames equal the baseline frames graded with the preset's numbers: at least GRADE_SHARE of the sampled pixels within GRADE_TOL per channel, and the mean brightness of the
               3D area is the predicted fraction of the baseline's (+-BRIGHT_TOL);
  progressive  the mean brightness of the 3D area, as a fraction of the baseline's, never rises (+-MONO_TOL) from the first race frame to the last and ends below END_RATIO of it;
  rain         the frames equal the baseline graded with the rain's numbers EXCEPT at the streaks: between RAIN_MIN and RAIN_MAX of the sampled pixels are brighter than predicted by RAIN_LUMA or more
               (the streaks), the rest within GRADE_TOL.
"""
import os
import re
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I does not put the script directory on the path
import mod_camera_check as cam  # noqa: E402

WIDTH, HEIGHT = 480, 272
HUD_TOP = 232                     # rows below are the HUD strip: the 3D area is above
STRIDE = 3                        # one pixel in 3 x 3 is judged
GRADE_TOL = 3                     # per channel, 8-bit levels (rounding of the dump's grade and of the prediction)
GRADE_SHARE = 0.985               # of the sampled pixels
BRIGHT_TOL = 0.04                 # on the brightness ratio of the 3D area
MONO_TOL = 0.03
END_RATIO = 0.80
RAIN_LUMA = 22                    # a streak pixel is at least this much brighter than predicted
RAIN_MIN, RAIN_MAX = 0.004, 0.15  # share of the sampled pixels that are streak pixels (heavy rain: 260 streaks of ~1 x 14 px = ~1 % of the picture)
SAMPLE_FRAMES = 12
MIN_FRAMES = 6
GRADE = re.compile(r"\[mod:weather\] GRADE vb=(\d+) steps=(\d+) mode=(\d+) rain=(\d+) rgb=([\d.]+),([\d.]+),([\d.]+) bright=([\d.]+) sat=([\d.]+)")
RAIN = re.compile(r"\[mod:weather\] RAIN vb=(\d+) level=(\d+) streaks=(\d+)")


def clamp01(v):
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)


def apply_grade(pixel, grade):
    """The host's scene grade of one (r, g, b) 8-bit pixel; grade = (r, g, b, brightness, saturation)."""
    tr, tg, tb, bright, sat = grade
    r, g, b = pixel[0] / 255.0 * tr, pixel[1] / 255.0 * tg, pixel[2] / 255.0 * tb
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    out = []
    for c in (r, g, b):
        out.append(int(round(clamp01((luma + (c - luma) * sat) * bright) * 255.0)))
    return tuple(out)


def combine(a, b):
    return tuple(x * y for x, y in zip(a, b))


NEUTRAL = (1.0, 1.0, 1.0, 1.0, 1.0)
PRESETS = {"afternoon": (1.06, 0.97, 0.88, 0.97, 1.05), "sunset": (1.20, 0.82, 0.62, 0.88, 1.10), "dusk": (0.62, 0.70, 1.00, 0.60, 0.85)}


def rain_grade(level):
    k = min(max(level, 0), 3) / 3.0
    target = (0.86, 0.91, 1.00, 0.82, 0.62)
    return tuple(1.0 + (t - 1.0) * k for t in target)


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def grades(text):
    return [{"vb": int(m.group(1)), "steps": int(m.group(2)), "mode": int(m.group(3)), "rain": int(m.group(4)), "grade": tuple(float(m.group(i)) for i in range(5, 10))} for m in GRADE.finditer(text)]


def race_start(text):
    """The vblank the mod first applied a non-neutral grade (a race is on screen), else the first GRADE with steps > 0."""
    rows = grades(text)
    for r in rows:
        if r["steps"] > 0 and r["grade"] != NEUTRAL:
            return r["vb"]
    for r in rows:
        if r["steps"] > 0:
            return r["vb"]
    return None


def paired(base_dir, run_dir, from_vblank=None):
    """[(key, base_path, run_path)] of the frames both runs dumped for the same present, from `from_vblank` on (the present key)."""
    a = cam.keyed_frames(os.path.join(base_dir, "frames"), read(os.path.join(base_dir, "run.log")))
    b = cam.keyed_frames(os.path.join(run_dir, "frames"), read(os.path.join(run_dir, "run.log")))
    keys = sorted(k for k in set(a) & set(b) if from_vblank is None or k >= from_vblank)
    return [(k, a[k], b[k]) for k in keys]


def sample(items, count=SAMPLE_FRAMES):
    if len(items) <= count:
        return items
    step = (len(items) - 1) / (count - 1)
    return [items[int(round(i * step))] for i in range(count)]


def compare(base_path, run_path, grade):
    """(share within tolerance, share of streak pixels, mean 3D brightness of the baseline, of the run, of the baseline graded with `grade`) over the sampled pixels."""
    bw, bh, bp = cam.read_ppm(base_path)
    rw, rh, rp = cam.read_ppm(run_path)
    ok = streak = total = 0
    sum_base = sum_run = sum_want = n3d = 0.0
    for y in range(0, bh, STRIDE):
        row = y * bw
        for x in range(0, bw, STRIDE):
            i = (row + x) * 3
            base = (bp[i], bp[i + 1], bp[i + 2])
            run = (rp[i], rp[i + 1], rp[i + 2])
            want = apply_grade(base, grade)
            total += 1
            if max(abs(want[c] - run[c]) for c in range(3)) <= GRADE_TOL:
                ok += 1
            elif sum(run) - sum(want) >= 3 * RAIN_LUMA:
                streak += 1
            if y < HUD_TOP:
                sum_base += sum(base) / 3.0
                sum_run += sum(run) / 3.0
                sum_want += sum(want) / 3.0
                n3d += 1
    return ok / total, streak / total, sum_base / max(n3d, 1), sum_run / max(n3d, 1), sum_want / max(n3d, 1)


def judge_idle(base_dir, run_dir):
    items = paired(base_dir, run_dir)
    if len(items) < 20:
        return False, f"only {len(items)} frames in common with the switched-off run"
    for _, a, b in items:
        if open(a, "rb").read() != open(b, "rb").read():
            return False, f"a frame differs from the switched-off run with the neutral preset (first at {os.path.basename(b)}): the mod must change nothing"
    return True, f"{len(items)} frames byte-identical to the switched-off run"


def judge_preset(base_dir, run_dir, name):
    text = read(os.path.join(run_dir, "run.log"))
    start = race_start(text)
    if start is None:
        return False, "the mod never logged a race grade"
    want = PRESETS[name]
    items = sample(paired(base_dir, run_dir, start + 10))
    if len(items) < MIN_FRAMES:
        return False, f"only {len(items)} race frames in common with the baseline"
    shares, ratios, expected = [], [], []
    for _, a, b in items:
        share, _streak, mb, mr, mw_ = compare(a, b, want)
        shares.append(share)
        ratios.append(mr / mb if mb > 0 else 1.0)
        expected.append(mw_ / mb if mb > 0 else 1.0)
    predicted = statistics.median(expected)
    measured = statistics.median(ratios)
    ok = min(shares) >= GRADE_SHARE and abs(measured - predicted) <= BRIGHT_TOL
    return ok, (f"{name}: {len(items)} race frames, the worst has {min(shares) * 100:.1f} % of its pixels within {GRADE_TOL} levels of the baseline graded with the preset (need >= {GRADE_SHARE * 100:.1f} %); "
                f"3D brightness {measured:.3f} of the baseline's, predicted {predicted:.3f} (+-{BRIGHT_TOL})")


def judge_progressive(base_dir, run_dir):
    text = read(os.path.join(run_dir, "run.log"))
    start = race_start(text)
    if start is None:
        return False, "the mod never logged a race grade"
    items = sample(paired(base_dir, run_dir, start + 5), 16)
    if len(items) < MIN_FRAMES:
        return False, f"only {len(items)} race frames in common with the baseline"
    ratios = []
    for _, a, b in items:
        _s, _r, mb, mr, _w = compare(a, b, NEUTRAL)
        ratios.append(mr / mb if mb > 0 else 1.0)
    rose = max((later - earlier for earlier, later in zip(ratios, ratios[1:])), default=0.0)
    ok = rose <= MONO_TOL and ratios[-1] <= END_RATIO and ratios[0] >= ratios[-1] + 0.05
    return ok, (f"{len(items)} race frames, brightness as a fraction of the baseline's: first {ratios[0]:.3f}, last {ratios[-1]:.3f} (need <= {END_RATIO}), largest rise between samples {rose:.3f} (limit {MONO_TOL}); "
                f"{[round(r, 2) for r in ratios]}")


def judge_rain(base_dir, run_dir, level):
    text = read(os.path.join(run_dir, "run.log"))
    start = race_start(text)
    if start is None:
        return False, "the mod never logged a race grade"
    items = sample(paired(base_dir, run_dir, start + 10))
    if len(items) < MIN_FRAMES:
        return False, f"only {len(items)} race frames in common with the baseline"
    want = rain_grade(level)
    shares, streaks = [], []
    for _, a, b in items:
        share, streak, _mb, _mr, _w = compare(a, b, want)
        shares.append(share)
        streaks.append(streak)
    typical = statistics.median(streaks)
    rest = statistics.median(shares)
    ok = RAIN_MIN <= typical <= RAIN_MAX and rest >= 1.0 - RAIN_MAX - 0.01
    return ok, (f"rain level {level}: median {typical * 100:.2f} % of the pixels are streak pixels (need {RAIN_MIN * 100:.1f}..{RAIN_MAX * 100:.0f} %), {rest * 100:.1f} % match the rain grade; "
                f"{len(RAIN.findall(text))} RAIN lines")


def main(argv):
    out, mode = argv[1], argv[2]
    d = lambda label: os.path.join(out, label)  # noqa: E731
    if mode == "idle":
        results = [judge_idle(d("off"), d("idle"))]
    elif mode in PRESETS:
        results = [judge_preset(d("off"), d(mode), mode)]
    elif mode == "progressive":
        results = [judge_progressive(d("off"), d("progressive"))]
    elif mode == "rain":
        results = [judge_rain(d("off"), d("rain"), 3)]
    else:
        print(f"mod_weather_check: unknown judge {mode}", file=sys.stderr)
        return 2
    ok = True
    for good, detail in results:
        print(f"mod_weather_check: {mode}: {'ok' if good else 'FAIL'}: {detail}")
        ok = ok and good
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
