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
  multi run     (`multi`, the reset_multi route) at least MULTI_MIN_RESTORES restores; after every one the car stays on asphalt with
                up.z > MIN_UP_Z from SETTLE_STEPS to LEAVE_STEPS (4 s) with no steering (the route steers only far later); each RESTORE line carries the straight-ahead run of the pose (`run=` >= MIN_RUN m: the rule picked it);
  quiet run     the mod installed, no button: no RESTORE and every frame byte-identical to the run without the mod;
  toggle run    the F9 menu turned the mod off before the button: `-> OFF` logged, no RESTORE, frames identical to the baseline.
"""
import math
import os
import re
import sys


FRAME = re.compile(r"frame_(\d+)\.ppm$")
TAG = r"\[mod:reset-to-track\] "
NUM = r"-?(?:[\d.]+|nan|inf)"
STATE = re.compile(TAG + rf"t=(-?\d+) step=(\d+) wheels=(\d) up_z=({NUM}) spd=({NUM}) pos=({NUM}),({NUM}),({NUM}) "
                   rf"yaw=({NUM}) rates=((?:{NUM},){{4}}{NUM})(?: vel={NUM},{NUM},{NUM} fwdcos=({NUM}))? ring=(\d+) restores=(\d+)"
                   rf"(?: acc=({NUM}) thr=({NUM}) brk=({NUM}))?(?: pad=\d+/\d+/\d+)?")
PADSEL = re.compile(TAG + r"PADSEL (down|up) step=(\d+)(?: tap=(\d))?")
RESTORE = re.compile(TAG + r"RESTORE step=(\d+) pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) yaw=(-?[\d.]+)(?: run=(-?[\d.]+))?")
SCRIPT_DONE = re.compile(r"script done at vblank (\d+)")
FRAME_DUMP = re.compile(r"\[frame-dump\] frame (\d+) vblank (\d+)")
TAP_PRESSES = 3            # the chase route's three SELECT presses
TAIL_AFTER_VBLANKS = 150   # frames dumped this long after the route's last press: the camera view has settled
LEAVE_STEPS = 240       # --multi: no restore may leave the asphalt within 4 s (the route does not steer that early after a restore)
MIN_RUN = 15.0          # --multi: the straight-ahead run (m) of every restored pose, as the mod logs it (`run=`); the rule off logs -1
MULTI_MIN_RESTORES = 2  # --multi: fewer restores than this and the check proves nothing
TOGGLE_LINE = re.compile(r"^\[mods\] \S+ -> OFF$", re.M)

SETTLE_STEPS = 30       # the ~10-step settle transient after a restore (research e1), with margin
WINDOW_STEPS = 600      # the car must keep driving this long
MIN_UP_Z = 0.9
YAW_STEPS = 120         # the heading is compared over this many steps after the restore
MAX_YAW_DRIFT = 0.03    # rad from the restored heading; a restore that keeps the spin drifts ~0.08 (the sabotage), one from rest ~0.005
CALM_STEPS = 10         # the first steps after the restore ...
MAX_CALM_RATE = 0.45    # ... have every angular rate below this (rad/s): the car dropped on its springs after a zeroed restore peaks at 0.25 (--decomp off)
                        # to 0.37 (--decomp on, another restore pose: the two modes follow different timelines on a pad route, FINDINGS), the
                        # crash's spin that the sabotage leaves is >= 0.6 (see MAX_ANY_RATE); the restore step's own line (what the mod writes) cannot be
                        # judged alone: --decomp on logs the first integration step at t=0
MAX_ANY_RATE = 0.6      # no angular rate at or above this at any time after the restore: the spin of the crash itself
SETTLED_STEP = 60       # from here ...
SETTLED_END = 120       # ... to here (the car is not yet driving hard: steering rates reach 0.09 later) ...
MAX_SETTLED_RATE = 0.1  # ... the suspension transient has decayed: every rate is below this (0.04 at most in the runs, both modes)
MIN_DRIVE_SPEED = 15.0  # m/s reached with the throttle held after the restore: a car restarted in the wrong gear does not get here
REST_SPEED = 1.0        # m/s right after the restore: it restarts from rest
REWIND_S = 1.5          # the restored pose is at least this many seconds of travel back (at the speed the car had)
LOOKBACK_STEPS = 300    # the speeds logged this far before the restore give the expected distance
MIN_RING_LINES = 10     # state lines with a non-empty ring that show the mod ran (quiet and toggle runs)


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
        t, step, wheels, up_z, spd, x, y, z, yaw, rates, fwdcos, ring, restores, acc, thr, brk = m.groups()
        out.append({"t": int(t), "step": int(step), "wheels": int(wheels), "up_z": float(up_z), "spd": float(spd),
                    "pos": (float(x), float(y), float(z)), "yaw": float(yaw), "rates": [float(r) for r in rates.split(",")],
                    "ring": int(ring), "restores": int(restores), "fwdcos": float(fwdcos) if fwdcos is not None else None,
                    "acc": float(acc) if acc is not None else None, "thr": float(thr) if thr is not None else None, "brk": float(brk) if brk is not None else None})
    return out


def parse_restores(text):
    return [{"step": int(m.group(1)), "pos": (float(m.group(2)), float(m.group(3)), float(m.group(4))), "yaw": float(m.group(5)),
             "run": float(m.group(6)) if m.group(6) is not None else None}
            for m in RESTORE.finditer(text)]


def wrap_pi(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def parse_padsel(text):
    """[(kind, step, tap)] for the mod's `PADSEL down|up` lines (a held Select hidden from the game; tap = replayed on release)."""
    return [(m.group(1), int(m.group(2)), None if m.group(3) is None else m.group(3) == "1") for m in PADSEL.finditer(text)]


def judge_select_hidden(text):
    """The restore run's Select (the route holds it): exactly one press, hidden, and released without a tap (a reset, not a camera change)."""
    events = parse_padsel(text)
    downs = [e for e in events if e[0] == "down"]
    ups = [e for e in events if e[0] == "up"]
    if len(downs) != 1 or len(ups) != 1:
        return False, f"{len(downs)} Select presses and {len(ups)} releases seen by the pad hook (expected 1 and 1)"
    if ups[0][2]:
        return False, "the held Select was replayed to the game as a tap: the camera view would change"
    return True, f"Select held from step {downs[0][1]} to {ups[0][1]}, hidden from the game, no tap"


def judge_taps(text, expected):
    """The chase route's short Select presses: each one hidden while down and replayed as a tap on release (camera cycling keeps working)."""
    ups = [e for e in parse_padsel(text) if e[0] == "up"]
    if len(ups) != expected or not all(e[2] for e in ups):
        return False, f"{len(ups)} releases, {sum(1 for e in ups if e[2])} replayed as taps (expected {expected}, all taps)"
    if parse_restores(text):
        return False, "a short Select press reset the car"
    return True, f"{expected} short presses, each replayed as a tap, no reset"


def non_finite(states):
    return [s for s in states if not all(math.isfinite(v) for v in (s["up_z"], s["spd"], s["yaw"], *s["pos"], *s["rates"]))]


def judge_restore(text, calm_steps=CALM_STEPS):
    """The restore run's log. `calm_steps`: how many steps after the restore every rate must stay calm (0 = only the restore
    step's own log line, -1 = never: the rollover check uses -1, a car reset out of a tumble wobbles on its springs for ~15 steps).
    Returns (ok, message)."""
    restores, states = parse_restores(text), parse_states(text)
    if non_finite(states):
        return False, f"nan/inf in the state log at step {non_finite(states)[0]['step']}"
    if len(restores) != 1:
        return False, f"{len(restores)} RESTORE lines (expected exactly 1)"
    rs = restores[0]
    ok_sel, msg_sel = judge_select_hidden(text) if PADSEL.search(text) else (True, "")  # fake-button runs have no pad lines
    if not ok_sel:
        return False, msg_sel
    before = [s for s in states if s["t"] < 0 and s["step"] < rs["step"]]
    after = [s for s in states if s["t"] >= 0]
    if not before or not any(s["wheels"] < 4 for s in before[-6:]):
        return False, "the car was not off the asphalt before the restore (the premise of the check)"
    if not after or after[-1]["t"] < WINDOW_STEPS:
        return False, f"the log follows the car only {after[-1]['t'] if after else 0} steps after the restore (need {WINDOW_STEPS})"
    spin = max((abs(r) for s in after if s["t"] <= calm_steps for r in s["rates"]), default=0.0)
    if spin > MAX_CALM_RATE:
        return False, f"an angular rate of {spin:.2f} rad/s in the first {calm_steps} steps (max {MAX_CALM_RATE}): the rates were not zeroed"
    wild = max(((abs(r), s["t"]) for s in after if s["t"] <= WINDOW_STEPS for r in s["rates"]), default=(0.0, 0))
    if calm_steps > 0 and wild[0] >= MAX_ANY_RATE:
        return False, f"an angular rate of {wild[0]:.2f} rad/s at t={wild[1]} (the crash's spin level, max {MAX_ANY_RATE}): the rates were not zeroed"
    late = max(((abs(r), s["t"]) for s in after if SETTLED_STEP <= s["t"] <= SETTLED_END for r in s["rates"]), default=(0.0, 0))
    if calm_steps > 0 and late[0] >= MAX_SETTLED_RATE:
        return False, f"an angular rate of {late[0]:.2f} rad/s at t={late[1]} (the transient must have decayed below {MAX_SETTLED_RATE} from t={SETTLED_STEP})"
    for s in after:
        if s["t"] > WINDOW_STEPS:
            break
        if s["t"] >= SETTLE_STEPS and (s["wheels"] != 4 or not s["up_z"] > MIN_UP_Z):
            return False, f"t={s['t']}: {s['wheels']}/4 wheels on asphalt, up.z {s['up_z']:.3f} (need 4 and > {MIN_UP_Z})"
    last = before[-1]
    moved = math.dist(last["pos"], after[0]["pos"])
    recent = [s["spd"] for s in before if s["step"] >= rs["step"] - LOOKBACK_STEPS]
    needed = REWIND_S * max(recent, default=0.0)
    if moved < max(needed, 1.0):
        return False, f"the car moved {moved:.1f} m, a rewind of {REWIND_S} s at the logged {max(recent, default=0.0):.1f} m/s is {needed:.1f} m"
    if after[0]["spd"] > REST_SPEED:
        return False, f"the car restarts at {after[0]['spd']:.2f} m/s, not from rest"
    drift = max((abs(wrap_pi(s["yaw"] - rs["yaw"])) for s in after if s["t"] <= YAW_STEPS), default=0.0)
    if drift > MAX_YAW_DRIFT:
        return False, f"the heading drifted {drift:.3f} rad from the restored one in the first {YAW_STEPS} steps (max {MAX_YAW_DRIFT}): the car is not settled"
    top = max(s["spd"] for s in after if s["t"] <= WINDOW_STEPS)
    if top < MIN_DRIVE_SPEED:
        return False, f"the car never exceeded {top:.2f} m/s after the restore: it does not drive"
    n = sum(1 for s in after if SETTLE_STEPS <= s["t"] <= WINDOW_STEPS)
    return True, (f"restored at step {rs['step']} ({moved:.0f} m back, from rest); {n} samples t={SETTLE_STEPS}..{WINDOW_STEPS}: 4/4 wheels on asphalt, "
                  f"up.z > {MIN_UP_Z}, heading drift {drift:.3f} rad, top speed {top:.1f} m/s")


def judge_multi(text):
    """The reset_multi route: several restores, none of which puts the car off the asphalt again within LEAVE_STEPS steps, and the
    look-ahead rule rejected at least one pose (so it is the rule, not luck, that keeps the restores on the road). Returns (ok, message)."""
    restores, states = parse_restores(text), parse_states(text)
    if non_finite(states):
        return False, f"nan/inf in the state log at step {non_finite(states)[0]['step']}"
    if len(restores) < MULTI_MIN_RESTORES:
        return False, f"{len(restores)} RESTORE lines (need at least {MULTI_MIN_RESTORES}: the route presses SELECT four times)"
    for index, rs in enumerate(restores):
        window = [s for s in states if s["t"] >= 0 and rs["step"] <= s["step"] <= rs["step"] + LEAVE_STEPS and s["restores"] == index + 1]
        late = [s for s in window if s["t"] >= SETTLE_STEPS]
        if not late or late[-1]["t"] < LEAVE_STEPS - 15:
            return False, f"restore {index + 1} (step {rs['step']}): the log follows the car only {late[-1]['t'] if late else 0} steps (need ~{LEAVE_STEPS})"
        for s in late:
            if s["wheels"] != 4 or not s["up_z"] > MIN_UP_Z:
                return False, (f"restore {index + 1} (step {rs['step']}) put the car out of bounds: t={s['t']}: {s['wheels']}/4 wheels on asphalt, "
                               f"up.z {s['up_z']:.3f}, within {LEAVE_STEPS} steps and with no steering")
    runs = [r["run"] for r in restores]
    if any(r is None or r < MIN_RUN for r in runs):
        return False, f"a restore did not use the straight-ahead rule (RESTORE run= values {runs}, need >= {MIN_RUN} m each)"
    return True, (f"{len(restores)} restores, each followed by {LEAVE_STEPS} steps on 4/4 asphalt, up.z > {MIN_UP_Z}; the poses restored had a straight run of "
                  f"{', '.join(f'{r:.0f}' for r in runs)} m")


def judge_no_restore(text, toggled=False):
    """A run where the mod must not reset, and must be shown to have run: no RESTORE line, nan/inf-free state lines; at least MIN_RING_LINES
    with a ring in use (the mod recorded poses) before the toggle (`toggled`: the `[mods] <id> -> OFF` line) and none after it.
    Returns (ok, message)."""
    restores, states = parse_restores(text), parse_states(text)
    if restores:
        return False, f"{len(restores)} RESTORE lines (the mod must not reset here), first at step {restores[0]['step']}"
    if non_finite(states):
        return False, f"nan/inf in the state log at step {non_finite(states)[0]['step']}"
    cut = TOGGLE_LINE.search(text)
    if toggled and cut is None:
        return False, "no `[mods] <id> -> OFF` line in the run log: the menu did not toggle the mod"
    ringed = after = 0
    for m in STATE.finditer(text):
        if cut is not None and m.start() > cut.start():
            after += 1
        elif int(m.group(12)) > 0:
            ringed += 1
    if ringed < MIN_RING_LINES:
        return False, f"only {ringed} state lines with poses in the ring before the end/toggle (need {MIN_RING_LINES}): the mod did not run"
    if toggled and after:
        return False, f"{after} mod state lines after the toggle: the mod ran while switched off"
    return True, f"no reset; the mod ran ({ringed} state lines with poses)" + (", silent after the toggle" if toggled else "")


def judge_identity(base_frames, mod_frames, min_frames=6):
    """Every common frame byte-identical. Returns (ok, message)."""
    common = sorted(set(base_frames) & set(mod_frames))
    if len(common) < min_frames:
        return False, f"only {len(common)} common frames"
    for index in common:
        if read_ppm(base_frames[index])[2] != read_ppm(mod_frames[index])[2]:
            return False, f"frame {index} differs from the run without the mod"
    return True, f"{len(common)} frames byte-identical to the run without the mod"


VIEW_Y0, VIEW_Y1, VIEW_BLOCK = 50, 232, 16   # the 3D picture (rows below the lap/timer HUD, above the speed strip), in 16 px blocks
MAX_VIEW_DISTANCE = 90.0   # max over blocks of the mean-level difference: the same view moved by ~1 step (a car, scenery) stays under ~45
                           # (measured), another camera view is above ~140


def block_means(frame):
    width, _height, pixels = frame
    out = []
    for by in range(VIEW_Y0, VIEW_Y1 - VIEW_BLOCK + 1, VIEW_BLOCK):
        for bx in range(0, width - VIEW_BLOCK + 1, VIEW_BLOCK):
            total = 0
            for y in range(by, by + VIEW_BLOCK, 2):
                total += sum(pixels[(y * width + bx) * 3:(y * width + bx + VIEW_BLOCK) * 3])
            out.append(total / (VIEW_BLOCK // 2 * VIEW_BLOCK * 3))
    return out


def judge_same_view(base_frames, mod_frames, min_frames=6):
    """Frames of the camera view reached after the presses: close to the run without the mod (a one-step timing shift is allowed, the
    mod replays a tap on release). Returns (ok, message)."""
    common = sorted(set(base_frames) & set(mod_frames))
    if len(common) < min_frames:
        return False, f"only {len(common)} common frames"
    worst = 0.0
    for index in common:
        a, b = block_means(read_ppm(base_frames[index])), block_means(read_ppm(mod_frames[index]))
        worst = max(worst, max(abs(x - y) for x, y in zip(a, b)))
        if worst > MAX_VIEW_DISTANCE:
            return False, f"frame {index}: the picture differs from the run without the mod by {worst:.0f} (max {MAX_VIEW_DISTANCE:.0f}): another camera view"
    return True, f"{len(common)} frames show the same view as the run without the mod (largest block difference {worst:.0f})"


POST_ROUTE_VBLANKS = 2500  # identity frames are compared up to this long after `script done`: the first same-build divergence measured after the route is at +2829 (docs/FINDINGS.md)


def frames_until_route_end(log, frames, margin=POST_ROUTE_VBLANKS):
    """The frames dumped up to `margin` vblanks after the route's `script done` line. After the route the car idles and the game runs on its own: those frames
    differ between two runs of the same build , so they say nothing about the mod."""
    done = SCRIPT_DONE.search(log)
    if done is None:
        raise ValueError("no `script done` line in the run log: the route did not finish, nothing to compare")
    limit = int(done.group(1)) + margin
    keep = {int(f) for f, v in FRAME_DUMP.findall(log) if int(v) <= limit}
    return {i: p for i, p in frames.items() if i in keep}


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def main(argv):
    out, mode = argv[1], argv[2]
    if mode == "restore":
        calm = int(argv[3]) if len(argv) > 3 else CALM_STEPS  # optional: the calm window in steps (see judge_restore)
        ok, message = judge_restore(read(os.path.join(out, "reset", "run.log")), calm)
        print(("PASS " if ok else "FAIL ") + f"reset: {message}")
        return 0 if ok else 1
    if mode == "multi":
        ok, message = judge_multi(read(os.path.join(out, "reset", "run.log")))
        print(("PASS " if ok else "FAIL ") + f"multi: {message}")
        return 0 if ok else 1
    if mode == "tap":
        log = read(os.path.join(out, "tap", "run.log"))
        base_log = read(os.path.join(out, "baseline_chase", "run.log"))
        ok1, m1 = judge_taps(log, TAP_PRESSES)
        done = SCRIPT_DONE.search(base_log)
        start = int(done.group(1)) + TAIL_AFTER_VBLANKS if done else 10 ** 12
        tail = [int(f) for f, v in FRAME_DUMP.findall(base_log) if int(v) >= start]
        base, mod = frames_of(os.path.join(out, "baseline_chase", "frames")), frames_of(os.path.join(out, "tap", "frames"))
        ok2, m2 = judge_same_view({i: base[i] for i in tail if i in base}, {i: mod[i] for i in tail if i in mod})
        ok = ok1 and ok2
        print(("PASS " if ok else "FAIL ") + f"tap: {m1}; after the presses (the camera view reached): {m2}")
        return 0 if ok else 1
    base = frames_of(os.path.join(out, "baseline_nosel" if mode == "quiet" else "baseline", "frames"))
    label = "quiet" if mode == "quiet" else "toggle"
    log = read(os.path.join(out, label, "run.log"))
    ok1, m1 = judge_no_restore(log, toggled=(mode == "toggle"))
    try:
        ok2, m2 = judge_identity(frames_until_route_end(log, base), frames_until_route_end(log, frames_of(os.path.join(out, label, "frames"))))
    except ValueError as error:
        ok2, m2 = False, str(error)
    ok = ok1 and ok2
    print(("PASS " if ok else "FAIL ") + f"{label}: {m1}; {m2}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
