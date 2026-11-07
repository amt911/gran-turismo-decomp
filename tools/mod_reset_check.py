"""Verdict of tools/mod_reset_check.sh: the reset-to-track mod (gt-mods repo) in a headless gt_play run.

The mod logs the player car's state (GT_MOD_RESET_LOG=1): `[mod:reset-to-track] t=<steps since the restore, -1 before> step=<n>
wheels=<0..4 on asphalt> up_z=<> spd=<m/s> pos=<x,y,z> yaw=<> rates=<5 floats> ring=<poses kept> restores=<n> acc=<accelerator> thr=<effective throttle> brk=<brake>` and one
`RESTORE step=<n> pos=<x,y,z> yaw=<>` line per reset. GT_MOD_RESET_FAKE=<step> holds the button from that player-car step, so no
controller is needed. The route (config/pad_routes/reset.pad) drives the car off the end of the straight into the grass/barrier.
Judged here:
  restore run   exactly one RESTORE; the car was off asphalt (< 4 wheels) before it; from SETTLE_STEPS after it to WINDOW_STEPS
                every state line has 4 wheels on asphalt and up.z > MIN_UP_Z; the car starts from rest, calm (every angular rate
                under MAX_CALM_RATE in the first CALM_STEPS), holds the restored heading (within MAX_YAW_DRIFT over the first
                YAW_STEPS) and drove (speed above MIN_DRIVE_SPEED);
  quiet run     the mod installed, no button: no RESTORE and every frame byte-identical to the run without the mod;
  toggle run    the F9 menu turned the mod off before the button: `-> OFF` logged, no RESTORE, frames identical to the baseline.
"""
import math
import os
import re
import sys


FRAME = re.compile(r"frame_(\d+)\.ppm$")
TAG = r"\[mod:reset-to-track\] "
STATE = re.compile(TAG + r"t=(-?\d+) step=(\d+) wheels=(\d) up_z=(-?[\d.]+|nan) spd=(-?[\d.]+|nan) pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) "
                   r"yaw=(-?[\d.]+) rates=((?:-?[\d.]+,){4}-?[\d.]+) ring=(\d+) restores=(\d+)(?: acc=(-?[\d.]+) thr=(-?[\d.]+) brk=(-?[\d.]+))?")
RESTORE = re.compile(TAG + r"RESTORE step=(\d+) pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) yaw=(-?[\d.]+)")
TOGGLE_LINE = re.compile(r"^\[mods\] \S+ -> OFF$", re.M)

SETTLE_STEPS = 30       # the ~10-step settle transient after a restore (research e1), with margin
WINDOW_STEPS = 600      # the car must keep driving this long
MIN_UP_Z = 0.9
YAW_STEPS = 120         # the heading is compared over this many steps after the restore
MAX_YAW_DRIFT = 0.03    # rad from the restored heading; a restore that keeps the spin drifts ~0.08 (the sabotage), one from rest ~0.005
CALM_STEPS = 10         # the first steps after the restore ...
MAX_CALM_RATE = 0.3     # ... have every angular rate below this (rad/s): zeroed rates stay under ~0.15, the crash's spin is ~0.65
MIN_DRIVE_SPEED = 3.0   # m/s reached at some point in the window: the car drives, it is not stuck
REST_SPEED = 1.0        # m/s right after the restore: it restarts from rest
MIN_MOVED = 5.0         # m between the car's position at the press and the restored one


# (the two helpers below mirror tools/mod_camera_check.py: the shell runs this file with `python -I`, which cannot import `tools`)
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


def frames_of(directory):
    out = {}
    for name in os.listdir(directory):
        match = FRAME.search(name)
        if match:
            out[int(match.group(1))] = os.path.join(directory, name)
    return out


def parse_states(text):
    out = []
    for m in STATE.finditer(text):
        t, step, wheels, up_z, spd, x, y, z, yaw, rates, ring, restores, acc, thr, brk = m.groups()
        out.append({"t": int(t), "step": int(step), "wheels": int(wheels), "up_z": float(up_z), "spd": float(spd),
                    "pos": (float(x), float(y), float(z)), "yaw": float(yaw), "rates": [float(r) for r in rates.split(",")],
                    "ring": int(ring), "restores": int(restores),
                    "acc": float(acc) if acc is not None else None, "thr": float(thr) if thr is not None else None, "brk": float(brk) if brk is not None else None})
    return out


def parse_restores(text):
    return [{"step": int(m.group(1)), "pos": (float(m.group(2)), float(m.group(3)), float(m.group(4))), "yaw": float(m.group(5))}
            for m in RESTORE.finditer(text)]


def wrap_pi(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def judge_restore(text):
    """The restore run's log. Returns (ok, message)."""
    restores, states = parse_restores(text), parse_states(text)
    if len(restores) != 1:
        return False, f"{len(restores)} RESTORE lines (expected exactly 1)"
    rs = restores[0]
    before = [s for s in states if s["t"] < 0 and s["step"] < rs["step"]]
    after = [s for s in states if s["t"] >= 0]
    if not before or not any(s["wheels"] < 4 for s in before[-6:]):
        return False, "the car was not off the asphalt before the restore (the premise of the check)"
    if not after or after[-1]["t"] < WINDOW_STEPS:
        return False, f"the log follows the car only {after[-1]['t'] if after else 0} steps after the restore (need {WINDOW_STEPS})"
    for s in after:
        if s["t"] > WINDOW_STEPS:
            break
        if s["t"] >= SETTLE_STEPS and (s["wheels"] != 4 or not s["up_z"] > MIN_UP_Z):
            return False, f"t={s['t']}: {s['wheels']}/4 wheels on asphalt, up.z {s['up_z']:.3f} (need 4 and > {MIN_UP_Z})"
    last = before[-1]
    moved = math.dist(last["pos"], after[0]["pos"])
    if moved < MIN_MOVED:
        return False, f"the car moved only {moved:.1f} m (needs {MIN_MOVED}): nothing was restored"
    if after[0]["spd"] > REST_SPEED:
        return False, f"the car restarts at {after[0]['spd']:.2f} m/s, not from rest"
    spin = max((abs(r) for s in after if s["t"] <= CALM_STEPS for r in s["rates"]), default=0.0)
    if spin > MAX_CALM_RATE:
        return False, f"an angular rate of {spin:.2f} rad/s in the first {CALM_STEPS} steps (max {MAX_CALM_RATE}): the rates were not zeroed"
    drift = max((abs(wrap_pi(s["yaw"] - rs["yaw"])) for s in after if s["t"] <= YAW_STEPS), default=0.0)
    if drift > MAX_YAW_DRIFT:
        return False, f"the heading drifted {drift:.3f} rad from the restored one in the first {YAW_STEPS} steps (max {MAX_YAW_DRIFT}): the car is not settled"
    top = max(s["spd"] for s in after if s["t"] <= WINDOW_STEPS)
    if top < MIN_DRIVE_SPEED:
        return False, f"the car never exceeded {top:.2f} m/s after the restore: it does not drive"
    n = sum(1 for s in after if SETTLE_STEPS <= s["t"] <= WINDOW_STEPS)
    return True, (f"restored at step {rs['step']} ({moved:.0f} m back, from rest); {n} samples t={SETTLE_STEPS}..{WINDOW_STEPS}: 4/4 wheels on asphalt, "
                  f"up.z > {MIN_UP_Z}, heading drift {drift:.3f} rad, top speed {top:.1f} m/s")


def judge_no_restore(text):
    """A run where the mod must not reset: no RESTORE line. Returns (ok, message)."""
    restores = parse_restores(text)
    if restores:
        return False, f"{len(restores)} RESTORE lines (the mod must not reset here), first at step {restores[0]['step']}"
    return True, "no reset"


def judge_identity(base_frames, mod_frames, min_frames=6):
    """Every common frame byte-identical. Returns (ok, message)."""
    common = sorted(set(base_frames) & set(mod_frames))
    if len(common) < min_frames:
        return False, f"only {len(common)} common frames"
    for index in common:
        if read_ppm(base_frames[index])[2] != read_ppm(mod_frames[index])[2]:
            return False, f"frame {index} differs from the run without the mod"
    return True, f"{len(common)} frames byte-identical to the run without the mod"


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def main(argv):
    out, mode = argv[1], argv[2]
    if mode == "restore":
        ok, message = judge_restore(read(os.path.join(out, "reset", "run.log")))
        print(("PASS " if ok else "FAIL ") + f"reset: {message}")
        return 0 if ok else 1
    base = frames_of(os.path.join(out, "baseline", "frames"))
    label = "quiet" if mode == "quiet" else "toggle"
    log = read(os.path.join(out, label, "run.log"))
    ok1, m1 = judge_no_restore(log)
    ok2, m2 = judge_identity(base, frames_of(os.path.join(out, label, "frames")))
    ok = ok1 and ok2
    if mode == "toggle" and TOGGLE_LINE.search(log) is None:
        ok, m1 = False, "no `[mods] <id> -> OFF` line in the run log: the menu did not toggle the mod"
    print(("PASS " if ok else "FAIL ") + f"{label}: {m1}; {m2}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
