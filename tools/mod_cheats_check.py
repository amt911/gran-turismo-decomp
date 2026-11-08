"""Verdict of tools/mod_cheats_check.sh: the cheats mod (gt-mods repo) in headless gt_play runs.

The mod logs the cars once per N physics steps (GT_MOD_CHEATS_LOG=1): `[mod:cheats] st=<player-car step> slow=<OFF|0.5x|0.25x> freeze=<0|1>
P=x,y,z spd=<m/s> acc=<accelerator K+0x530> A0=x,y,z A1=... A2=...` (P the player's car, A<i> the AI cars in slot order), plus `ROW <row> -> <value>`
when a menu row changed a cheat. The host logs the pad menu: `[padmenu] <key> at vblank N` and `[padmenu] menu opened|closed at vblank N`.
Judged here (modes of the command line):
  identity  frames byte-identical to a run without the mod (the mod installed but disabled; or enabled with every cheat off);
  slowmo F  against an idle run (mod on, cheats off): the same physics at F times the clock. The player's speed->distance curve is the same (the
            car covers the same ground to reach a speed), the steps it takes to climb between two speeds are 1/F times as many, and an AI car has
            covered clearly less ground at the same step;
  freeze    every AI car's position is constant while the player's moves, and the idle run shows the AI cars do move (the check can fail);
  pad       the SELECT+START chord opens the menu, Down/Right change Slow motion then Freeze AI, the chord closes it; while it is open the game
            sees no throttle (acc 0) and before and after it does; the cheats chosen in the menu then act (slow motion on, AI frozen).
"""
from __future__ import annotations

import math
import os
import re
import sys
from dataclasses import dataclass, field

try:
    from tools.mod_rollover_check import frames_of, judge_identity
except ImportError:  # run as a script under `python -I`: no cwd on the path
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from tools.mod_rollover_check import frames_of, judge_identity

NUM = r"-?(?:\d+\.?\d*|nan|inf)"
VEC = rf"({NUM}),({NUM}),({NUM})"
LINE = re.compile(rf"\[mod:cheats\] st=(\d+) slow=(\S+) freeze=([01]) P={VEC} spd=({NUM}) acc=({NUM})((?: A\d+={VEC})*)")
AI = re.compile(rf"A\d+={VEC}")
ROW = re.compile(r"\[mod:cheats\] ROW (\w+) -> (\S+)")
PADMENU = re.compile(r"\[padmenu\] (\w+) at vblank (\d+)")
PADMENU_STATE = re.compile(r"\[padmenu\] menu (opened|closed) at vblank (\d+)")
SCRIPT_DONE = re.compile(r"script done at vblank \d+")

MIN_LINES = 40            # a run must log this many lines for a judgement to mean anything
MIN_RACING_SPEED = 1.0    # m/s: the car is driving
PLAYER_MOVED = 50.0       # m: the player went at least this far (freeze run)
AI_STILL = 0.05           # m: an AI car that is "frozen" moves less than this over the whole run
AI_MOVES = 30.0           # m: in the idle run an AI car moves at least this far (so that a no-op freeze is caught)
CURVE_TOLERANCE = 0.20    # relative (plus CURVE_SLACK m) difference of the speed->distance curve (the throttle ramp is not slowed: ~15% measured)
CURVE_SLACK = 1.5
TIME_TOLERANCE = 0.25     # relative difference of the steps to climb between two speeds, against 1/F of the idle run's (10% at 0.5x, 20% at 0.25x measured: the engine and throttle ramp are not slowed; a no-op is off by 50-75%)
AI_SLOWER = {0.5: 0.6, 0.25: 0.35}  # an AI car's ground at the same step, relative to the idle run's, at most
MIN_SPEED_SPAN = 4.0      # m/s between the lowest and the highest speed level of the curve test
ACC_DOWN = 0.05           # accelerator while the menu is open (the throttle is hidden from the game)
ACC_UP = 0.5              # accelerator with the throttle held
ACC_FULL = 0.9            # accelerator once the throttle is fully down again
MIN_WINDOW_LINES = 5
SETTLE_LINES = 25         # log lines (2 steps each) the accelerator takes to follow a pad change (it is smoothed: ~20 lines measured, 2026-11-08)


@dataclass
class Car:
    st: int
    slow: str
    freeze: bool
    pos: tuple[float, float, float]
    spd: float
    acc: float
    ai: list[tuple[float, float, float]] = field(default_factory=list)


def parse_lines(text: str) -> list[Car]:
    out = []
    for m in LINE.finditer(text):
        g = m.groups()
        vals = [float(x) for x in g[3:6]] + [float(g[6]), float(g[7])]
        ai = [tuple(float(x) for x in a.groups()) for a in AI.finditer(g[8] or "")]
        out.append(Car(int(g[0]), g[1], g[2] == "1", (vals[0], vals[1], vals[2]), vals[3], vals[4], ai))
    return out


def dist(a, b) -> float:
    return math.dist(a, b)


def finite(c: Car) -> bool:
    return all(math.isfinite(v) for v in (*c.pos, c.spd, c.acc)) and all(math.isfinite(v) for p in c.ai for v in p)


def crossing(lines: list[Car], speed: float):
    """(step, position) where the player's speed first reaches `speed`, interpolated between the two lines around it; None if it never does."""
    for prev, cur in zip(lines, lines[1:]):
        if prev.spd < speed <= cur.spd:
            t = (speed - prev.spd) / (cur.spd - prev.spd)
            pos = tuple(a + t * (b - a) for a, b in zip(prev.pos, cur.pos))
            return prev.st + t * (cur.st - prev.st), pos
    return None


def judge_slowmo(idle_text: str, slow_text: str, factor: float):
    """The slow run is the idle run with the clock scaled by `factor` (0.5 or 0.25). Returns (ok, message)."""
    idle, slow = parse_lines(idle_text), parse_lines(slow_text)
    if len(idle) < MIN_LINES or len(slow) < MIN_LINES:
        return False, f"too few log lines (idle {len(idle)}, slow {len(slow)}; need {MIN_LINES})"
    if not all(finite(c) for c in idle + slow):
        return False, "a non-finite number in the log"
    name = {0.5: "0.5x", 0.25: "0.25x"}[factor]
    if any(c.slow != "OFF" for c in idle):
        return False, "the idle run has a cheat on"
    if sum(1 for c in slow if c.slow == name) < MIN_LINES:
        return False, f"the slow run barely logged slow={name}"
    top = 0.9 * min(max(c.spd for c in idle), max(c.spd for c in slow))
    if top < MIN_SPEED_SPAN + 1.0:
        return False, f"the cars never got fast enough to compare (top common speed {top:.1f} m/s)"
    levels = [top * f for f in (0.35, 0.7, 1.0)]
    idle_x = [crossing(idle, v) for v in levels]
    slow_x = [crossing(slow, v) for v in levels]
    if any(x is None for x in idle_x + slow_x):
        return False, "a speed level was never reached in one of the runs"
    # (a) the same ground for the same speed gained: distance from the first level to each later one
    notes = []
    for k in (1, 2):
        d_idle = dist(idle_x[0][1], idle_x[k][1])
        d_slow = dist(slow_x[0][1], slow_x[k][1])
        if abs(d_slow - d_idle) > CURVE_TOLERANCE * d_idle + CURVE_SLACK:
            return False, (f"speed->distance curve differs: from {levels[0]:.1f} to {levels[k]:.1f} m/s the slow run covers {d_slow:.1f} m, "
                           f"the idle run {d_idle:.1f} m (the physics is not the same, only slower)")
        notes.append(f"{d_slow:.1f}/{d_idle:.1f} m")
    # (b) the steps to climb from the first level to the last are 1/factor as many
    n_idle = idle_x[2][0] - idle_x[0][0]
    n_slow = slow_x[2][0] - slow_x[0][0]
    want = n_idle / factor
    if abs(n_slow - want) > TIME_TOLERANCE * want:
        return False, (f"climbing {levels[0]:.1f} -> {levels[2]:.1f} m/s took {n_slow:.0f} steps, expected {want:.0f} (= {n_idle:.0f} / {factor}); "
                       f"the clock is not scaled by {factor}")
    # (c) the AI cars are slowed too: less ground at the same step
    common = min(idle[-1].st, slow[-1].st)
    a_idle = next((c for c in reversed(idle) if c.st <= common and c.ai), None)
    a_slow = next((c for c in reversed(slow) if c.st <= common and c.ai), None)
    if a_idle is None or a_slow is None or a_idle.st == a_slow.st and False:
        return False, "no AI car in the log"
    first_idle, first_slow = next(c for c in idle if c.ai), next(c for c in slow if c.ai)
    moved_idle = dist(first_idle.ai[0], a_idle.ai[0])
    moved_slow = dist(first_slow.ai[0], a_slow.ai[0])
    if moved_idle < AI_MOVES:
        return False, f"the idle run's AI car moved only {moved_idle:.1f} m: no way to tell it was slowed"
    if moved_slow > AI_SLOWER[factor] * moved_idle:
        return False, f"the AI car covered {moved_slow:.1f} m at step ~{common} against {moved_idle:.1f} m idle: not slowed to {factor}x (limit {AI_SLOWER[factor]:.2f})"
    return True, (f"clock x{factor}: same speed->distance curve ({', '.join(notes)} slow/idle), {n_slow:.0f} steps to climb vs {n_idle:.0f}/{factor} = {want:.0f}, "
                  f"AI car {moved_slow:.0f} m vs {moved_idle:.0f} m idle")


def max_drift(lines: list[Car], index: int) -> float:
    pts = [c.ai[index] for c in lines if len(c.ai) > index]
    return max((dist(pts[0], p) for p in pts), default=0.0)


def judge_freeze(idle_text: str, frozen_text: str):
    idle, frozen = parse_lines(idle_text), parse_lines(frozen_text)
    if len(idle) < MIN_LINES or len(frozen) < MIN_LINES:
        return False, f"too few log lines (idle {len(idle)}, frozen {len(frozen)}; need {MIN_LINES})"
    if not all(finite(c) for c in idle + frozen):
        return False, "a non-finite number in the log"
    if sum(1 for c in frozen if c.freeze) < MIN_LINES:
        return False, "the frozen run barely logged freeze=1"
    frozen = [c for c in frozen if c.freeze]  # the AI cars may have been driving before the cheat started
    ai_count = min((len(c.ai) for c in frozen), default=0)
    if ai_count == 0:
        return False, "no AI car in the log"
    moved_player = max(dist(frozen[0].pos, c.pos) for c in frozen)
    if moved_player < PLAYER_MOVED:
        return False, f"the player moved only {moved_player:.1f} m: the run did not race"
    drifts = [max_drift(frozen, i) for i in range(ai_count)]
    if any(d > AI_STILL for d in drifts):
        return False, "an AI car moved while frozen (max drift per car: " + ", ".join(f"{d:.2f} m" for d in drifts) + ")"
    idle_moved = max_drift(idle, 0)
    if idle_moved < AI_MOVES:
        return False, f"in the idle run the AI car moved only {idle_moved:.1f} m: the check could not tell a freeze from nothing"
    return True, (f"{ai_count} AI cars still (max drift {max(drifts):.3f} m) while the player went {moved_player:.0f} m; "
                  f"the same AI car moves {idle_moved:.0f} m when not frozen")


def judge_pad(text: str):
    """The pad run: chord, navigation, values, hiding of the throttle, effects after closing."""
    keys = [(m.group(1), int(m.group(2))) for m in PADMENU.finditer(text) if m.group(1) not in ("menu",)]
    names = [k for k, _ in keys]
    want = ["chord", "down", "right", "down", "right", "chord"]
    if names != want:
        return False, f"pad menu keys {names}, expected {want} (the chord may not have fired, or a key was lost)"
    states = [(m.group(1), m.start()) for m in PADMENU_STATE.finditer(text)]
    if [s for s, _ in states] != ["opened", "closed"]:
        return False, f"menu state changes {[s for s, _ in states]}, expected opened then closed"
    opened, closed = states[0][1], states[1][1]
    rows = [(m.group(1), m.group(2), m.start()) for m in ROW.finditer(text)]
    if [(r, v) for r, v, _ in rows] != [("slow_motion", "0.5x"), ("freeze_ai", "ON")]:
        return False, f"menu rows changed {[(r, v) for r, v, _ in rows]}, expected slow_motion -> 0.5x then freeze_ai -> ON"
    if not all(opened < pos < closed for _, _, pos in rows):
        return False, "a row changed outside the time the menu was open"
    cars = [(m.start(), c) for m, c in zip(LINE.finditer(text), parse_lines(text))]
    before = [c for pos, c in cars if pos < opened and c.spd > MIN_RACING_SPEED][-5:]  # the five lines just before the chord: the throttle is down
    inside = [c for pos, c in cars if opened < pos < closed][SETTLE_LINES:]
    after = [c for pos, c in cars if pos > closed]
    if len(before) < 3 or len(inside) < MIN_WINDOW_LINES or len(after) < 3:
        return False, f"too few log lines around the menu (before {len(before)}, open {len(inside)}, after {len(after)})"
    if min(c.acc for c in before) < ACC_UP:
        return False, "the throttle is not held before the menu opens (the route did not drive)"
    if max(c.acc for c in inside) > ACC_DOWN:
        return False, f"the game saw the throttle while the menu was open (accelerator {max(c.acc for c in inside):.2f})"
    if max(c.acc for c in after) < ACC_FULL:  # the route still holds the throttle for a while after the menu closes
        return False, "the throttle did not come back after the menu closed"
    freeze_at = rows[1][2]
    frozen = [c for pos, c in cars if pos > freeze_at]
    if not frozen or not all(c.freeze and c.slow == "0.5x" for c in frozen):
        return False, "after the menu's choices the log does not show slow=0.5x freeze=1"
    ai_count = min((len(c.ai) for c in frozen), default=0)
    if ai_count == 0 or any(max_drift(frozen, i) > AI_STILL for i in range(ai_count)):
        return False, "an AI car moved after Freeze AI was switched on in the menu"
    return True, (f"chord opened the menu, Down/Right set slow motion 0.5x and Freeze AI on, chord closed it; accelerator {max(c.acc for c in inside):.2f} while "
                  f"open ({len(inside)} lines), {min(c.acc for c in before):.2f}+ before and {max(c.acc for c in after):.2f} after; {ai_count} AI cars still")


def read(path: str) -> str:
    with open(path, errors="replace") as handle:
        return handle.read()


def report(label: str, ok: bool, message: str) -> int:
    print(("PASS " if ok else "FAIL ") + f"{label}: {message}")
    return 0 if ok else 1


def main(argv: list[str]) -> int:
    if len(argv) < 4:
        print("usage: mod_cheats_check.py OUT identity LABEL | slowmo LABEL FACTOR | freeze LABEL | pad LABEL", file=sys.stderr)
        return 2
    out, mode, label = argv[1], argv[2], argv[3]
    log = lambda name: read(os.path.join(out, name, "run.log"))  # noqa: E731
    if mode == "identity":
        if SCRIPT_DONE.search(log(label)) is None:
            return report(f"identity {label}", False, "no `script done` line")
        ok, message = judge_identity(frames_of(os.path.join(out, "baseline", "frames")), frames_of(os.path.join(out, label, "frames")))
        return report(f"identity {label}", ok, message)
    if mode == "slowmo":
        factor = float(argv[4])
        if factor not in AI_SLOWER:
            print("slowmo factor must be 0.5 or 0.25", file=sys.stderr)
            return 2
        ok, message = judge_slowmo(log("idle"), log(label), factor)
        return report(f"slowmo {factor} ({label})", ok, message)
    if mode == "freeze":
        ok, message = judge_freeze(log("idle"), log(label))
        return report(f"freeze ({label})", ok, message)
    if mode == "pad":
        ok, message = judge_pad(log(label))
        return report(f"pad ({label})", ok, message)
    print(f"unknown mode {mode}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
