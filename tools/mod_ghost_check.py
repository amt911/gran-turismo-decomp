"""Verdict of tools/mod_ghost_check.sh: the ghost-any-mode mod (gt-mods repo) in headless gt_play runs of the `chase` pad route with frames dumped, against a run with the mod switched off.

The route cannot drive a lap, so the mod makes a lap boundary every N physics steps after the car first moves (GT_MOD_GHOST_FAKE_LAP=m90): the car creeps at 1-3 m/s before the race starts, lap 1 is
its first 90 steps and the ghost of lap 2 is the lap-1 pose at the same lap time, ~2 m behind the player (between the chase camera and the car). The mod logs
  `REC lapstep=<n> pos=x,y,z`           lap 1, every 10 steps
  `LAP n=<k> steps=<s> best=<b> new_best=<0|1> vb=<v>`
  `GHOST lapstep=<n> pose=x,y,z player=x,y,z`   from lap 2 on, every 10 steps
  `BOX vb=<v> bbox=x0,y0,x1,y1 corners=<n>`     the projected box of the ghost, once per 30 vblanks
Judged here:
  idle      the mod enabled with the ghost hidden (GT_MOD_GHOST_HIDE=1): every frame byte-identical to the switched-off run (nothing drawn, nothing written);
  ghost     (a) the laps are counted (>= 2 LAP lines, the first is a new best);
            (b) the ghost is where the best lap was at the same lap time: every GHOST pose equals the REC pose of the same lap step (REC_TOL m) and is behind the player (it LAGS: the pose of lap 1 at lap step t is
                where the player was t steps after the start, not now);
            (c) the box is drawn: BOX lines, the box wider than MIN_BOX_PX and on the screen; frames before the first lap ended are identical to the baseline's, frames with a BOX differ from it
                inside the logged box (+- BOX_SLACK px) and, outside it, only by the HUD's clock;
            (d) translucent: the changed pixels are a blend of the baseline pixel and the ghost colour (blend factor in BLEND_RANGE for at least BLEND_SHARE of them).
"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I does not put the script directory on the path
import mod_camera_check as cam  # noqa: E402

TAG = r"\[mod:ghost-any-mode\] "
NUM = r"-?(?:[\d.]+|nan|inf)"
VEC = rf"({NUM}),({NUM}),({NUM})"
REC = re.compile(TAG + rf"REC lapstep=(\d+) pos={VEC}")
LAP = re.compile(TAG + r"LAP n=(\d+) steps=(\d+) best=(\d+) new_best=([01]) vb=(\d+)")
GHOST = re.compile(TAG + rf"GHOST lapstep=(\d+) pose={VEC} player={VEC}")
BOX = re.compile(TAG + rf"BOX vb=(\d+) bbox=({NUM}),({NUM}),({NUM}),({NUM}) corners=(\d+)")

REC_TOL = 0.01
MIN_LAP_LINES = 2
MIN_GHOST_LINES = 4
MIN_BOX_LINES = 3
MIN_BOX_PX = 20
BOX_SLACK = 8
BLEND_RANGE = (0.15, 0.95)
BLEND_SHARE = 0.6
GHOST_COLOUR = (0x66, 0xE0, 0xFF)
MIN_PIXELS = 40            # changed pixels a frame must have to say anything about the blend
CLOCK_BOX = (460, 480, 250, 272)  # the HUD clock flips a digit at an instant that moves by a frame between two runs
MIN_BOX_FRAMES = 3


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def frames_of_run(run_dir):
    return cam.keyed_frames(os.path.join(run_dir, "frames"), read(os.path.join(run_dir, "run.log")))


def paired(base_dir, run_dir):
    a, b = frames_of_run(base_dir), frames_of_run(run_dir)
    return sorted((k, a[k], b[k]) for k in set(a) & set(b))


def judge_idle(base_dir, run_dir):
    items = paired(base_dir, run_dir)
    if len(items) < 20:
        return False, f"only {len(items)} frames in common with the switched-off run"
    for _, a, b in items:
        if open(a, "rb").read() != open(b, "rb").read():
            return False, f"a frame differs from the switched-off run with the ghost hidden (first at {os.path.basename(b)})"
    return True, f"{len(items)} frames byte-identical to the switched-off run"


def parse(text):
    rec = {int(m.group(1)): tuple(float(m.group(i)) for i in (2, 3, 4)) for m in REC.finditer(text)}
    laps = [(int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4) == "1", int(m.group(5))) for m in LAP.finditer(text)]
    ghost = [(int(m.group(1)), tuple(float(m.group(i)) for i in (2, 3, 4)), tuple(float(m.group(i)) for i in (5, 6, 7))) for m in GHOST.finditer(text)]
    boxes = [(int(m.group(1)), tuple(float(m.group(i)) for i in (2, 3, 4, 5)), int(m.group(6))) for m in BOX.finditer(text)]
    return rec, laps, ghost, boxes


def judge_laps_and_poses(text):
    rec, laps, ghost, boxes = parse(text)
    if len(laps) < MIN_LAP_LINES:
        return False, f"only {len(laps)} LAP lines (the fake lap boundary never came, or the mod never saw the car)", None
    if not laps[0][3]:
        return False, "the first lap was not kept as the best", None
    if len(ghost) < MIN_GHOST_LINES:
        return False, f"only {len(ghost)} GHOST lines after lap 1", None
    worst = 0.0
    for lapstep, pose, player in ghost:
        want = rec.get(lapstep)
        if want is None:
            continue
        worst = max(worst, math.dist(pose, want))
    compared = sum(1 for g in ghost if g[0] in rec)
    if compared < MIN_GHOST_LINES:
        return False, f"only {compared} GHOST lines have a REC pose of the same lap step to compare with", None
    if worst > REC_TOL:
        return False, f"a ghost pose is {worst:.3f} m from the lap-1 pose of the same lap step (limit {REC_TOL}): it is not replaying the recording", None
    # it lags: the player is ahead of where it was at this lap time; measured along the player's travel (the distance to the player grows with the lap)
    lag = [math.dist(pose, player) for _, pose, player in ghost]
    if min(lag) < 0.3:
        return False, f"the ghost sits on the player (distance {min(lag):.2f} m): it shows the current pose, not the recorded one", None
    return True, f"{len(laps)} laps, {compared} ghost poses equal to the recording (max {worst:.4f} m), the ghost trails the player by {min(lag):.1f}..{max(lag):.1f} m", (laps, boxes)


def changed_pixels(base, run, skip=()):
    """[(x, y, base_rgb, run_rgb)] of the pixels that differ, outside `skip` boxes (x0, x1, y0, y1)."""
    bw, bh, bp = cam.read_ppm(base)
    rw, rh, rp = cam.read_ppm(run)
    out = []
    for y in range(bh):
        for x in range(bw):
            if any(x0 <= x < x1 and y0 <= y < y1 for x0, x1, y0, y1 in skip):
                continue
            i = (y * bw + x) * 3
            if bp[i:i + 3] != rp[i:i + 3]:
                out.append((x, y, (bp[i], bp[i + 1], bp[i + 2]), (rp[i], rp[i + 1], rp[i + 2])))
    return out


def blend_factor(base, run):
    """The share of the way from the baseline pixel to the ghost colour the run's pixel is, averaged over the channels that differ enough to tell; None when none does."""
    factors = [(run[c] - base[c]) / (GHOST_COLOUR[c] - base[c]) for c in range(3) if abs(GHOST_COLOUR[c] - base[c]) >= 40]
    return sum(factors) / len(factors) if factors else None


def judge_ghost(base_dir, run_dir):
    text = read(os.path.join(run_dir, "run.log"))
    ok, detail, extra = judge_laps_and_poses(text)
    if not ok:
        return False, detail
    laps, boxes = extra
    if len(boxes) < MIN_BOX_LINES:
        return False, f"only {len(boxes)} BOX lines: the ghost was never drawn (no player camera, or it is off screen)"
    wide = [b for b in boxes if b[1][2] - b[1][0] >= MIN_BOX_PX and b[2] >= 4]
    if not wide:
        return False, "the projected box is never wider than the minimum or never has four corners in front of the camera"
    first_lap_vb = laps[0][4]
    items = paired(base_dir, run_dir)
    if len(items) < 20:
        return False, f"only {len(items)} frames in common with the switched-off run"
    for key, a, b in items:
        if key < first_lap_vb - 2 and open(a, "rb").read() != open(b, "rb").read():
            return False, f"a frame before the first lap ended (present {key} < {first_lap_vb}) differs from the baseline: the ghost is drawn too early"
    by_vb = {key: (a, b) for key, a, b in items}
    inside = blended = judged = 0
    frames = 0
    for vb, (x0, y0, x1, y1), corners in boxes:
        near = [k for k in by_vb if abs(k - vb) <= 2]
        if not near or x1 - x0 < MIN_BOX_PX:
            continue
        a, b = by_vb[near[0]]
        changed = changed_pixels(a, b, skip=[CLOCK_BOX])
        if len(changed) < MIN_PIXELS:
            continue
        frames += 1
        cx0, cy0, cx1, cy1 = min(p[0] for p in changed), min(p[1] for p in changed), max(p[0] for p in changed), max(p[1] for p in changed)
        if cx0 >= x0 - BOX_SLACK and cy0 >= max(y0, 0) - BOX_SLACK and cx1 <= x1 + BOX_SLACK and cy1 <= min(y1, 271) + BOX_SLACK:
            inside += 1
        for _x, _y, base, run in changed:
            f = blend_factor(base, run)
            if f is not None:
                judged += 1
                blended += BLEND_RANGE[0] <= f <= BLEND_RANGE[1]
    if frames < MIN_BOX_FRAMES:
        return False, f"only {frames} frames with a BOX show changed pixels against the baseline (need {MIN_BOX_FRAMES}): {detail}"
    if inside < frames:
        return False, f"{frames - inside} of {frames} frames changed pixels outside the logged box (+-{BOX_SLACK} px): the overlay is not where the box was projected"
    share = blended / judged if judged else 0.0
    if share < BLEND_SHARE:
        return False, f"only {share * 100:.0f} % of the changed pixels are a translucent blend with the ghost colour (need {BLEND_SHARE * 100:.0f} %)"
    return True, f"{detail}; {frames} frames show the box inside its logged bounds, {share * 100:.0f} % of {judged} changed pixels are a blend ({BLEND_RANGE[0]}..{BLEND_RANGE[1]}) with the ghost colour"


def main(argv):
    out, mode = argv[1], argv[2]
    d = lambda label: os.path.join(out, label)  # noqa: E731
    if mode == "idle":
        results = [judge_idle(d("off"), d("idle"))]
    elif mode == "ghost":
        results = [judge_ghost(d("off"), d("ghost"))]
    else:
        print(f"mod_ghost_check: unknown judge {mode}", file=sys.stderr)
        return 2
    ok = True
    for good, detail in results:
        print(f"mod_ghost_check: {mode}: {'ok' if good else 'FAIL'}: {detail}")
        ok = ok and good
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
