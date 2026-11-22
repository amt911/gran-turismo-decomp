"""Verdict of tools/mod_damage_check.sh: the mechanical-damage mod (gt-mods repo) in a headless gt_play run.

The mod logs one line per player-car physics step (GT_MOD_DAMAGE_LOG=1):
  `[mod:mechanical-damage] S st=<step> mv=<step the accelerator was first pressed, -1 before> vb=<vblank> v=<speed m/s> thr=<pedal before>/<pedal after the step> yaw=<yaw rate> pos=x,y,z role=<1 live, 3 replay>`
plus `HIT step=.. dv=.. contact=(..) strength=.. -> engine=.. drive=.. align=..`, `REPAIR`, `RACE live|replay`. The route (config/pad_routes/rollover.pad) holds the accelerator
from the start; the check's fake hit (GT_MOD_DAMAGE_FAKE=m30:<dv>:<f|b|r|l>) lands 30 steps after the accelerator is first pressed, so the car is accelerating from rest when it happens.
Steps are counted from the car's first movement (`k`) so runs of different real-time pacing pair up.
Judged here:
  idle      the mod enabled with no hit: no HIT line before the barrier, no effect (thr in == thr out every step), and the frames before the first HIT/last logged step identical to the switched-off run;
  power     (front hit) the acceleration at the same speed (a table of the undamaged run, per speed bin, so gear changes cancel) is POWER_RATIO of it, as a median over the steps after the hit;
  repair    (front hit, repair at REPAIR_K) the acceleration is lower during the damage, and after the repair it is back to the undamaged car's at the same speed;
  cap       (hard rear hit) the speed after the hit never exceeds the severe-damage cap while the undamaged run goes past it;
  pull      (hard side hit) the mean yaw rate after the hit is the signed bias: right hit pulls right (negative yaw rate), left hit left, 0.02..0.1 rad/s from the undamaged run;
  replay    (circuit route) the same HIT live and in the replay, and the replayed car's speed at REPLAY_KS within REPLAY_SPEED_TOL of the live one (the car must be moving);
  toggle    the mod switched off from the F9 menu logs nothing afterwards.
"""
import math
import os
import re
import statistics
import sys

TAG = r"\[mod:mechanical-damage\] "
NUM = r"-?(?:[\d.]+|nan|inf)"
STATE = re.compile(TAG + rf"S st=(\d+) mv=(-?\d+) vb=(\d+) v=({NUM}) thr=({NUM})/({NUM}) yaw=({NUM}) pos=({NUM}),({NUM}),({NUM}) role=(\d)")
HIT = re.compile(TAG + rf"HIT step=(\d+) dv=({NUM}) contact=\(({NUM}) ({NUM}) ({NUM})\) strength=({NUM}) -> engine=({NUM}) drive=({NUM}) align=({NUM})")
SCRIPT_DONE = re.compile(r"script done at vblank (\d+)")
FRAME_DUMP = re.compile(r"\[frame-dump\] frame (\d+) vblank (\d+)")
TOGGLE_LINE = re.compile(r"^\[mods\] (\S+) -> OFF$", re.M)
FRAME = re.compile(r"frame_(\d+)\.ppm$")

HIT_K = 30            # the fake hit lands this many steps after the accelerator is first pressed
POWER_FROM = HIT_K + 5
POWER_TO = HIT_K + 170   # steps (since the accelerator was pressed) over which the acceleration is compared
POWER_RATIO = (0.45, 0.80)   # damaged / undamaged acceleration AT THE SAME SPEED, median over the steps, for the front hit (dv 25: engine 0.95 -> engine power x 0.62)
REPAIR_K = 200        # the repair run's repair, in steps after the accelerator is first pressed
AFTER_REPAIR_FROM = REPAIR_K + 15
AFTER_REPAIR_TO = REPAIR_K + 200
REPAIR_RATIO_BACK = (0.93, 1.07)   # after the repair the acceleration at the same speed is the undamaged one again
BIN = 0.5             # m/s: speed bins of the undamaged run's acceleration table
MIN_BASE_GAIN = 0.02  # m/s per step: bins where the undamaged car hardly accelerates (a gear change, the top speed) say nothing
CAP = 25.0            # cap_low of the shipped mod.toml (a hard rear hit: drive 1.0)
CAP_SLACK = 0.5       # m/s over the cap one step can show (the cap is applied after each step)
CAP_PREMISE = 27.0    # the undamaged car must exceed this speed in the window, or the cap would not be tested
CAP_FROM = HIT_K + 5
CAP_TO = 900
PULL_BIAS = (0.02, 0.10)   # rad/s: mean yaw rate after the hit minus the undamaged run's, at full alignment damage (bias 0.06)
PULL_FROM = HIT_K + 20
PULL_TO = HIT_K + 170
REPLAY_KS = (HIT_K + 30, HIT_K + 70, HIT_K + 120)   # steps since the accelerator was pressed at which live and replay speeds are compared: before the circuit route's first real collision (~220 steps after the hit)
REPLAY_SPEED_TOL = 1.0
REPLAY_MIN_SPEED = 3.0   # the car must be moving for the comparison to say anything


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def states(text, role=None):
    out = []
    for m in STATE.finditer(text):
        st, mv, vb, v, tin, tout, yaw, x, y, z, r = m.groups()
        if role is not None and int(r) != role:
            continue
        out.append({"st": int(st), "mv": int(mv), "vb": int(vb), "v": float(v), "tin": float(tin), "tout": float(tout), "yaw": float(yaw), "pos": (float(x), float(y), float(z)), "role": int(r)})
    return out


def by_k(rows):
    """{k: row} with k = steps since the accelerator was first pressed (rows before it are left out)."""
    return {r["st"] - r["mv"]: r for r in rows if r["mv"] >= 0}


def hits(text):
    return [{"step": int(m.group(1)), "dv": float(m.group(2)), "contact": (float(m.group(3)), float(m.group(4)), float(m.group(5))), "strength": float(m.group(6)),
             "engine": float(m.group(7)), "drive": float(m.group(8)), "align": float(m.group(9))} for m in HIT.finditer(text)]


def accel_table(rows):
    """{speed bin: median acceleration (m/s per step)} of an undamaged run, from consecutive steps."""
    bins = {}
    ordered = sorted(rows.items())
    for (k0, r0), (k1, r1) in zip(ordered, ordered[1:]):
        if k1 == k0 + 1 and r0["tin"] > 0.5:
            bins.setdefault(int(r0["v"] / BIN), []).append(r1["v"] - r0["v"])
    return {b: statistics.median(v) for b, v in bins.items() if len(v) >= 3}


def accel_ratio(base_rows, run_rows, k_from, k_to):
    """Median over the steps in [k_from, k_to) of (the run's acceleration) / (the undamaged run's at the same speed); None when fewer than 20 steps are comparable."""
    table = accel_table(base_rows)
    ratios = []
    for k in range(k_from, k_to):
        r0, r1 = run_rows.get(k), run_rows.get(k + 1)
        if r0 is None or r1 is None or r0["tin"] <= 0.5:
            continue
        ref = table.get(int(r0["v"] / BIN))
        if ref is not None and ref >= MIN_BASE_GAIN:
            ratios.append((r1["v"] - r0["v"]) / ref)
    return statistics.median(ratios) if len(ratios) >= 20 else None


def judge_idle(base_dir, idle_dir):
    text = read(os.path.join(idle_dir, "run.log"))
    rows = states(text)
    if len(rows) < 500:
        return False, f"the mod logged only {len(rows)} steps (it never saw the player's car)"
    hs = hits(text)
    first_hit_vb = None
    if hs:
        # frames from the first real hit on may differ: the barrier hit at step ~1850 is a real impact above the threshold
        first = hs[0]["step"]
        first_hit_vb = next((r["vb"] for r in rows if r["st"] >= first), None)
        if any(r["mv"] >= 0 and r["st"] - r["mv"] < 1000 for r in rows if r["st"] == first):
            return False, f"a hit at step {first} before the barrier (normal driving must do nothing)"
    pre = [r for r in rows if first_hit_vb is None or r["vb"] < first_hit_vb]
    bad = [r for r in pre if r["tin"] != r["tout"]]
    # the pedal ramp moves the throttle inside the step (in != out is normal); damage must not: compare with the undamaged run's pedal trace instead
    del bad
    frames_ok, detail = compare_frames(base_dir, idle_dir, first_hit_vb if first_hit_vb is not None else rows[-1]["vb"])
    if not frames_ok:
        return False, detail
    return True, f"{len(rows)} steps, {len(hs)} hit(s) (first at vblank {first_hit_vb}); {detail}"


def frame_table(d):
    """{vblank: path} of the frames a run dumped."""
    table = {}
    log = read(os.path.join(d, "run.log"))
    index = {int(f): int(v) for f, v in FRAME_DUMP.findall(log)}
    frames_dir = os.path.join(d, "frames")
    if not os.path.isdir(frames_dir):
        return table
    for name in os.listdir(frames_dir):
        m = FRAME.search(name)
        if m and int(m.group(1)) in index:
            table[index[int(m.group(1))]] = os.path.join(frames_dir, name)
    return table


def compare_frames(base_dir, run_dir, until_vb):
    a, b = frame_table(base_dir), frame_table(run_dir)
    common = sorted(v for v in set(a) & set(b) if v < until_vb)
    if len(common) < 20:
        return False, f"only {len(common)} frames before vblank {until_vb} in common with the switched-off run"
    for v in common:
        if open(a[v], "rb").read() != open(b[v], "rb").read():
            return False, f"frame at vblank {v} differs from the switched-off run before the first hit (vblank {until_vb})"
    return True, f"{len(common)} frames before vblank {until_vb} byte-identical to the switched-off run"


def judge_power(base_text, run_text):
    base, run = by_k(states(base_text)), by_k(states(run_text))
    hs = hits(run_text)
    if not hs or hs[0]["engine"] < 0.5:
        return False, f"no front hit logged (hits {hs[:1]})"
    ratio = accel_ratio(base, run, POWER_FROM, POWER_TO)
    if ratio is None:
        return False, "too few comparable steps (the runs do not overlap in speed)"
    ok = POWER_RATIO[0] <= ratio <= POWER_RATIO[1]
    return ok, f"acceleration at the same speed after the hit: {ratio:.2f} of the undamaged car's (need {POWER_RATIO[0]}..{POWER_RATIO[1]}), engine damage {hs[0]['engine']:.2f}"


def judge_repair(base_text, run_text):
    base, run = by_k(states(base_text)), by_k(states(run_text))
    if "REPAIR" not in run_text:
        return False, "no REPAIR line"
    during = accel_ratio(base, run, POWER_FROM, REPAIR_K)
    after = accel_ratio(base, run, AFTER_REPAIR_FROM, AFTER_REPAIR_TO)
    if during is None or after is None:
        return False, f"cannot compare (during {during}, after {after})"
    ok = during <= POWER_RATIO[1] and REPAIR_RATIO_BACK[0] <= after <= REPAIR_RATIO_BACK[1]
    return ok, f"acceleration while damaged {during:.2f} of the undamaged car's (need <= {POWER_RATIO[1]}), after the repair {after:.2f} (need {REPAIR_RATIO_BACK[0]}..{REPAIR_RATIO_BACK[1]})"


def judge_cap(base_text, run_text):
    base, run = by_k(states(base_text)), by_k(states(run_text))
    hs = hits(run_text)
    if not hs or hs[0]["drive"] < 0.99:
        return False, f"no full rear hit logged ({hs[:1]})"
    base_max = max((r["v"] for k, r in base.items() if CAP_FROM <= k <= CAP_TO), default=0.0)
    run_max = max((r["v"] for k, r in run.items() if CAP_FROM <= k <= CAP_TO), default=0.0)
    if base_max < CAP_PREMISE:
        return False, f"the undamaged run only reaches {base_max:.1f} m/s: the cap is not tested"
    ok = run_max <= CAP + CAP_SLACK
    return ok, f"top speed after the hit {run_max:.1f} m/s (cap {CAP}), the undamaged car reaches {base_max:.1f}"


def judge_pull(base_text, run_text, sign):
    base, run = by_k(states(base_text)), by_k(states(run_text))
    hs = hits(run_text)
    if not hs or abs(hs[0]["align"]) < 0.99:
        return False, f"no full side hit logged ({hs[:1]})"
    def mean_yaw(rows):
        vals = [r["yaw"] for k, r in rows.items() if PULL_FROM <= k <= PULL_TO]
        return sum(vals) / len(vals) if vals else None
    mb, mr = mean_yaw(base), mean_yaw(run)
    if mb is None or mr is None:
        return False, "no steps in the window"
    diff = -sign * (mr - mb)  # a positive yaw rate turns the car to its left (FACTS): a hit from the right pulls to the right = a negative yaw rate
    ok = PULL_BIAS[0] <= diff <= PULL_BIAS[1]
    return ok, f"mean yaw rate {mr:+.4f} against {mb:+.4f} undamaged: pull {diff:.4f} rad/s toward the {'right' if sign > 0 else 'left'} hit (need {PULL_BIAS[0]}..{PULL_BIAS[1]})"


def judge_replay(text):
    live, rep = states(text, 1), states(text, 3)
    if not rep:
        return False, "the mod never saw the player's car in its replay (no role 3 steps)"
    hs = hits(text)
    marker = text.find("RACE replay")
    live_hits = hits(text[:marker]) if marker >= 0 else hs
    rep_hits = hits(text[marker:]) if marker >= 0 else []
    if not live_hits or not rep_hits:
        return False, f"hits live {len(live_hits)}, in the replay {len(rep_hits)}: the damage must be rebuilt in the replay"
    if [round(h["dv"], 2) for h in live_hits[:1]] != [round(h["dv"], 2) for h in rep_hits[:1]] or round(live_hits[0]["engine"], 2) != round(rep_hits[0]["engine"], 2):
        return False, f"the hit differs: live {live_hits[0]}, replay {rep_hits[0]}"
    lk, rk = by_k(live), by_k(rep)
    if any(k not in lk or k not in rk for k in REPLAY_KS):
        return False, f"steps {REPLAY_KS} after the accelerator is missing (live {[k in lk for k in REPLAY_KS]}, replay {[k in rk for k in REPLAY_KS]})"
    if lk[REPLAY_KS[-1]]["v"] < REPLAY_MIN_SPEED:
        return False, f"the live car is at {lk[REPLAY_KS[-1]]['v']:.2f} m/s at step {REPLAY_KS[-1]}: too slow for the comparison to mean anything"
    diffs = [abs(lk[k]["v"] - rk[k]["v"]) for k in REPLAY_KS]
    ok = max(diffs) <= REPLAY_SPEED_TOL
    return ok, (f"the same hit live and in the replay (engine {live_hits[0]['engine']:.2f}); speeds at steps {REPLAY_KS}: live {[round(lk[k]['v'], 2) for k in REPLAY_KS]}, "
                f"replay {[round(rk[k]['v'], 2) for k in REPLAY_KS]} m/s (max |diff| {max(diffs):.2f}, limit {REPLAY_SPEED_TOL})")


def judge_toggle(text):
    m = TOGGLE_LINE.search(text)
    if not m:
        return False, "no `-> OFF` line: the toggle never fired"
    before, after = text[:m.start()], text[m.end():]
    if not states(before):
        return False, "the mod did not run before the toggle"
    if STATE.search(after) or HIT.search(after):
        return False, "the mod kept running after it was switched off"
    return True, f"{len(states(before))} steps before the toggle, nothing after"


def judge_sanity(text):
    if re.search(TAG + r".*\b(?:nan|inf)", text):
        return False, "nan or inf in the mod's lines"
    return True, "no nan"


def main(argv):
    out, mode = argv[1], argv[2]
    p = lambda label: read(os.path.join(out, label, "run.log"))
    if mode == "idle":
        results = [judge_idle(os.path.join(out, "off"), os.path.join(out, "idle"))]
    elif mode == "power":
        results = [judge_power(p("idle"), p("front")), judge_sanity(p("front"))]
    elif mode == "repair":
        results = [judge_repair(p("idle"), p("repair"))]
    elif mode == "cap":
        results = [judge_cap(p("idle"), p("rear"))]
    elif mode == "pull":
        results = [judge_pull(p("idle"), p("right"), 1), judge_pull(p("idle"), p("left"), -1)]
    elif mode == "replay":
        results = [judge_replay(p("replay"))]
    elif mode == "toggle":
        results = [judge_toggle(p("toggle"))]
    else:
        print(f"mod_damage_check: unknown judge {mode}", file=sys.stderr)
        return 2
    ok = True
    for good, detail in results:
        print(f"mod_damage_check: {mode}: {'ok' if good else 'FAIL'}: {detail}")
        ok = ok and good
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
