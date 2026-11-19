"""Verdict of tools/mod_cameras_check.sh: the extra-cameras mod (gt-mods repo) in a headless gt_play run.

The mod logs (GT_MOD_CAMS_LOG=1), once per vblank, `[mod:extra-cameras] CAM vb=<v> view=<wheel|roof|tv|heli> replay=<0|1> eye=x,y,z fwd=x,y,z car=x,y,z carfwd=x,y,z up=<z of the up row>`
(physics frame, Z up) and, for every draw of a part of the player's car, `DRAW seen=(x y z) expected=(x y z)`: `seen` is the translation row of that part's WORLD matrix (the car's
origin in the view space the GAME rendered with, `draw_matrix`), `expected` is where the pose the mod set puts the car's body origin (OpenGL view space: x right, y up, -z forward).
Judged here:
  idle      the mod enabled with no extra view selected: every frame byte-identical to the run with the mod switched off;
  view      (wheel | roof | tv) CAM lines exist for the view; the eye is where the view puts it relative to the car (distance range per view); the game drew the car where the pose says:
            the median over the DRAW lines of |seen - expected| per component under DRAW_TOL (the car's model origin is not exactly its body origin); and the frames differ from the idle run's;
  heli      (replay) the replay's CAM lines say `heli`, replay=1, and the eye stays above the car (>= HELI_MIN_UP m, after HELI_SETTLE_S of lines) and behind it.
"""
import math
import os
import re
import statistics
import sys

TAG = r"\[mod:extra-cameras\] "
NUM = r"-?(?:[\d.]+|nan|inf)"
VEC = rf"({NUM}),({NUM}),({NUM})"
CAM = re.compile(TAG + rf"CAM vb=(\d+) view=(\w+) replay=([01]) eye={VEC} fwd={VEC} car={VEC} carfwd={VEC} up=({NUM})")
DRAW = re.compile(TAG + rf"DRAW seen=\(({NUM}) ({NUM}) ({NUM})\) expected=\(({NUM}) ({NUM}) ({NUM})\)")
FRAME_DUMP = re.compile(r"\[frame-dump\] frame (\d+) vblank (\d+)")
FRAME = re.compile(r"frame_(\d+)\.ppm$")

DRAW_TOL = 1.0                      # m, per component, on the median
MIN_DRAWS = 20
MIN_CAMS = 50
DIST_RANGE = {"wheel": (0.5, 4.0), "roof": (0.8, 2.5), "tv": (5.0, 100.0)}  # eye-to-car-origin distance
HELI_MIN_UP = 4.0                   # m above the car, once settled
HELI_SETTLE_LINES = 120             # CAM lines (one per vblank) before the spring has settled
HELI_MIN_BEHIND = 3.0               # m behind along the car's heading


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def cams(text, view=None):
    rows = []
    for m in CAM.finditer(text):
        g = m.groups()
        row = {"vb": int(g[0]), "view": g[1], "replay": int(g[2]), "eye": tuple(map(float, g[3:6])), "fwd": tuple(map(float, g[6:9])), "car": tuple(map(float, g[9:12])),
               "carfwd": tuple(map(float, g[12:15])), "up": float(g[15])}
        if view is None or row["view"] == view:
            rows.append(row)
    return rows


def draws(text):
    return [(tuple(map(float, m.groups()[:3])), tuple(map(float, m.groups()[3:]))) for m in DRAW.finditer(text)]


def frame_table(d):
    table = {}
    index = {int(f): int(v) for f, v in FRAME_DUMP.findall(read(os.path.join(d, "run.log")))}
    frames_dir = os.path.join(d, "frames")
    if not os.path.isdir(frames_dir):
        return table
    for name in os.listdir(frames_dir):
        m = FRAME.search(name)
        if m and int(m.group(1)) in index:
            table[index[int(m.group(1))]] = os.path.join(frames_dir, name)
    return table


def compare_frames(base_dir, run_dir):
    """(identical count, differing count, first differing vblank) over the vblanks both runs dumped."""
    a, b = frame_table(base_dir), frame_table(run_dir)
    common = sorted(set(a) & set(b))
    same = diff = 0
    first = None
    for v in common:
        if open(a[v], "rb").read() == open(b[v], "rb").read():
            same += 1
        else:
            diff += 1
            first = v if first is None else first
    return same, diff, first


def judge_idle(base_dir, idle_dir):
    same, diff, first = compare_frames(base_dir, idle_dir)
    if same + diff < 20:
        return False, f"only {same + diff} frames in common with the switched-off run"
    if diff:
        return False, f"{diff} of {same + diff} frames differ from the switched-off run (first at vblank {first}) with no extra view selected"
    return True, f"{same} frames byte-identical to the switched-off run"


def judge_view(view, run_dir, base_dir):
    text = read(os.path.join(run_dir, "run.log"))
    rows = cams(text, view)
    if len(rows) < MIN_CAMS:
        return False, f"only {len(rows)} CAM lines for the {view} view (need {MIN_CAMS}): the camera function never set a pose"
    lo, hi = DIST_RANGE[view]
    dist = [math.dist(r["eye"], r["car"]) for r in rows]
    med = statistics.median(dist)
    if not lo <= med <= hi:
        return False, f"median eye-to-car distance {med:.2f} m outside {lo}..{hi} for the {view} view"
    d = draws(text)
    if len(d) < MIN_DRAWS:
        return False, f"only {len(d)} DRAW lines (need {MIN_DRAWS}): the game's own matrices could not be read"
    err = [statistics.median(abs(s[i] - e[i]) for s, e in d) for i in range(3)]
    if max(err) > DRAW_TOL:
        return False, f"the game drew the car {err[0]:.2f}/{err[1]:.2f}/{err[2]:.2f} m (x/y/z, median) from where the {view} pose puts it (limit {DRAW_TOL}): {len(d)} draws"
    msg = f"{len(rows)} CAM lines, eye {med:.2f} m from the car, the game drew the car within {max(err):.2f} m of the pose over {len(d)} draws"
    if base_dir is not None and os.path.isdir(os.path.join(base_dir, "frames")):
        same, diff, first = compare_frames(base_dir, run_dir)
        if diff < 5:
            return False, f"the frames hardly differ from the idle run ({diff} of {same + diff})"
        msg += f"; {diff} of {same + diff} frames differ from the idle run"
    return True, msg


def judge_heli(run_dir):
    text = read(os.path.join(run_dir, "run.log"))
    rows = cams(text, "heli")
    if len(rows) < HELI_SETTLE_LINES + 30:
        return False, f"only {len(rows)} heli CAM lines: the replay camera never took over"
    if any(r["replay"] != 1 for r in rows):
        return False, "a heli line outside the replay"
    settled = rows[HELI_SETTLE_LINES:]
    up = min(r["eye"][2] - r["car"][2] for r in settled)
    behind = min(sum((r["car"][i] - r["eye"][i]) * r["carfwd"][i] for i in range(2)) for r in settled)
    if up < HELI_MIN_UP:
        return False, f"the helicopter eye comes down to {up:.2f} m above the car (need >= {HELI_MIN_UP})"
    if behind < HELI_MIN_BEHIND:
        return False, f"the eye is only {behind:.2f} m behind the car along its heading (need >= {HELI_MIN_BEHIND})"
    return True, f"{len(rows)} replay lines, the eye stays >= {up:.1f} m above and >= {behind:.1f} m behind the car"


def main(argv):
    out, mode = argv[1], argv[2]
    d = lambda label: os.path.join(out, label)  # noqa: E731
    if mode == "idle":
        results = [judge_idle(d("off"), d("idle"))]
    elif mode in ("wheel", "roof", "tv"):
        results = [judge_view(mode, d(mode), d("idle") if os.path.isdir(d("idle")) else None)]
    elif mode == "heli":
        results = [judge_heli(d("heli"))]
    else:
        print(f"mod_cameras_check: unknown judge {mode}", file=sys.stderr)
        return 2
    ok = True
    for good, detail in results:
        print(f"mod_cameras_check: {mode}: {'ok' if good else 'FAIL'}: {detail}")
        ok = ok and good
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
