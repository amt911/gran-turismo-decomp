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
  callguest the read-only native the mod calls from the vblank tick answered 0 (call_guest works in the running game);
  toggle    the mod switched off from the F9 menu mid-race: it ran before, logged nothing after, every frame identical to no mod;
  padoff    Slow motion OFF -> 0.5x -> 0.25x -> OFF from the pad: distance per step per m/s follows the factor and is normal again after OFF;
  padreset  `pad` with reset-to-track also enabled and the chord staggered by 3 vblanks: reset-to-track never saw Select;
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
# cheats >= 0.4.0 append ` since=<steps since the player's car first moved, -1 before> rep=<0 live|1 replay> ai=<AI speed>`
REPLAY_LINE = re.compile(rf"\[mod:cheats\] st=\d+ slow=(\S+) freeze=([01]) P={VEC} spd={NUM} acc={NUM}((?: A\d+={VEC})*) since=(-?\d+) rep=([01]) ai=(\S+)")

MAX_REPLAY_DRIFT = 0.5    # m: a replayed car (player or AI) against the live one, paired by the steps since the player first moved
MIN_LINES = 40            # a run must log this many lines for a judgement to mean anything
MIN_RACING_SPEED = 1.0    # m/s: the car is driving
PLAYER_MOVED = 50.0       # m: the player went at least this far (freeze run)
AI_STILL = 0.05           # m: an AI car that is "frozen" moves less than this over the whole run
AI_MOVES = 30.0           # m: in the idle run an AI car moves at least this far (so that a no-op freeze is caught)
CURVE_TOLERANCE = {0.5: 0.25, 0.25: 0.35}  # relative (plus CURVE_SLACK m) difference of the speed->distance curve; measured 0.15 / 0.25 (`--slow-only`: 3 runs per mode, off and on, identical: the game is deterministic; off vs on differ by < 0.01)
CURVE_SLACK = 1.5
TIME_TOLERANCE = {0.5: 0.25, 0.25: 0.35}  # relative difference of the steps to climb between two speeds against 1/F of the idle run's; measured 0.10 / 0.21 (the engine and throttle ramp are not slowed), a no-op gives 0.50 / 0.75
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
        if abs(d_slow - d_idle) > CURVE_TOLERANCE[factor] * d_idle + CURVE_SLACK:
            return False, (f"speed->distance curve differs: from {levels[0]:.1f} to {levels[k]:.1f} m/s the slow run covers {d_slow:.1f} m, "
                           f"the idle run {d_idle:.1f} m (the physics is not the same, only slower)")
        notes.append(f"{d_slow:.1f}/{d_idle:.1f} m")
    # (b) the steps to climb from the first level to the last are 1/factor as many
    n_idle = idle_x[2][0] - idle_x[0][0]
    n_slow = slow_x[2][0] - slow_x[0][0]
    want = n_idle / factor
    if abs(n_slow - want) > TIME_TOLERANCE[factor] * want:
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


def measure_slowmo(idle_text: str, slow_text: str, factor: float):
    """The numbers judge_slowmo compares with its tolerances: {curve: worst relative difference of the speed->distance curve, time: relative
    difference of the steps to climb from 1/F of the idle run's, ai: the AI car's ground relative to the idle run's}; None when a log is unusable."""
    idle, slow = parse_lines(idle_text), parse_lines(slow_text)
    if len(idle) < MIN_LINES or len(slow) < MIN_LINES:
        return None
    top = 0.9 * min(max(c.spd for c in idle), max(c.spd for c in slow))
    levels = [top * f for f in (0.35, 0.7, 1.0)]
    ix, sx = [crossing(idle, v) for v in levels], [crossing(slow, v) for v in levels]
    if any(x is None for x in ix + sx):
        return None
    curve = max(abs(dist(sx[0][1], sx[k][1]) - dist(ix[0][1], ix[k][1])) / dist(ix[0][1], ix[k][1]) for k in (1, 2))
    want = (ix[2][0] - ix[0][0]) / factor
    common = min(idle[-1].st, slow[-1].st)
    a_idle = next((c for c in reversed(idle) if c.st <= common and c.ai), None)
    a_slow = next((c for c in reversed(slow) if c.st <= common and c.ai), None)
    first_idle, first_slow = next((c for c in idle if c.ai), None), next((c for c in slow if c.ai), None)
    if None in (a_idle, a_slow, first_idle, first_slow):
        return None
    ai = dist(first_slow.ai[0], a_slow.ai[0]) / max(dist(first_idle.ai[0], a_idle.ai[0]), 1e-9)
    return {"curve": curve, "time": abs((sx[2][0] - sx[0][0]) - want) / want, "ai": ai}


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
    # The cheat device (2026-11-12): it opens on Mods; Close goes up to the categories (cursor on Race), Right opens Race (CROSS is the
    # held throttle), Right sets slow motion, Down, Right sets freeze AI. Another mod installed changes nothing (its rows are in Mods).
    want = ["chord", "close", "right", "right", "down", "right", "chord"]
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
        return False, (f"too few log lines around the menu (before {len(before)}, open {len(inside)}, after {len(after)}); "
                       "if none were logged while it was open the game may have paused: it saw the chord's Start")
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
    return True, (f"chord opened the device, Circle/Right reached Race, Right/Down/Right set slow motion 0.5x and Freeze AI on, chord closed it; accelerator {max(c.acc for c in inside):.2f} while "
                  f"open ({len(inside)} lines), {min(c.acc for c in before):.2f}+ before and {max(c.acc for c in after):.2f} after; {ai_count} AI cars still")


DEVICE_BOUND = re.compile(r"\[device\] bound cheats\.nitro pad (\S+)")
DEVICE_REFUSED = re.compile(r"\[device\] refused cheats\.nitro pad (\S+): (.*) at vblank \d+")
DEVICE_ACTION = re.compile(r"\[device\] action cheats\.nitro at vblank \d+")


NITRO_ON = re.compile(r"\[mod:cheats\] ROW nitro -> ON")


def judge_hotkey(text: str):
    """--hotkey: the device's capture bound nitro to R+SQUARE, then that combination in the race fired the action and the mod switched nitro on
    (nitro adds force at full throttle; the check proves the hotkey path, the cheat's physics is the cheats mod's own concern)."""
    bound = DEVICE_BOUND.search(text)
    if bound is None or bound.group(1) != "R+SQUARE":
        return False, "nitro not bound to R+SQUARE through the device's capture (no `[device] bound cheats.nitro pad R+SQUARE`)"
    closed = [m.start() for m in PADMENU_STATE.finditer(text) if m.group(1) == "closed"]
    fired = [m.start() for m in DEVICE_ACTION.finditer(text) if closed and m.start() > closed[-1]]
    if not fired:
        return False, "the hotkey never fired nitro after the device closed"
    if len(fired) != 1:
        return False, f"one press fired nitro {len(fired)} times"
    switched = [m.start() for m in NITRO_ON.finditer(text) if m.start() > fired[0]]
    if not switched:
        return False, "the action fired but the mod never switched nitro on (`ROW nitro -> ON` after it)"
    return True, "capture bound nitro to R+SQUARE; R+SQUARE in the race fired the action once and the mod switched nitro on"


def judge_conflict(text: str):
    """--rebind-conflict: capturing SELECT+START (the device's open combination) for nitro is refused, naming its owner, and nothing is bound."""
    if DEVICE_BOUND.search(text) is not None:
        return False, "the device's own combination was not refused: nitro got bound to it"
    refused = DEVICE_REFUSED.search(text)
    if refused is None or refused.group(1) != "SELECT+START":
        return False, "the capture of SELECT+START was not refused (no `[device] refused cheats.nitro pad SELECT+START`)"
    if "Open the cheat device" not in refused.group(2):
        return False, f"the refusal does not name the owner: {refused.group(2)!r}"
    return True, f"SELECT+START for nitro refused: {refused.group(2)}"


CALLGUEST = re.compile(r"\[mod:cheats\] CALLGUEST status=(-?\d+) v0=(\d+)")
TOGGLE_OFF = re.compile(r"^\[mods\] cheats -> OFF$", re.M)
PADSEL = re.compile(r"\[mod:reset-to-track\] (?:PADSEL|RESTORE)")
MIN_CALLGUEST = 3
MIN_PRE_TOGGLE_LINES = 10
RATE_MIN_SPEED = 3.0      # m/s: below it the displacement per step is too small to measure
RATE_SETTLE_LINES = 6     # lines after a row change before its phase is measured
RATE_TOLERANCE = {0.5: 0.12, 0.25: 0.08, 1.0: 0.06}  # absolute difference of (distance per step per m/s) / the idle value from the phase's factor


def judge_callguest(text: str):
    """call_guest from the vblank tick ran in the game: the read-only native 0x0894CCB8 (the rollover flag reader) answered 0 through the dispatcher."""
    calls = [(int(m.group(1)), int(m.group(2))) for m in CALLGUEST.finditer(text)]
    if len(calls) < MIN_CALLGUEST:
        return False, f"{len(calls)} CALLGUEST lines (need {MIN_CALLGUEST}): the tick did not run or call_guest was not exercised"
    bad = [c for c in calls if c != (0, 0)]
    if bad:
        return False, f"call_guest from the tick gave {bad[0]} (status, v0), expected (0, 0)"
    return True, f"call_guest from the vblank tick ran the game's flag reader {len(calls)} times: status 0, v0 0"


def judge_toggle(text: str):
    """Switched off from the F9 menu mid-race: the mod ran before the toggle (car log), logged nothing after it."""
    cut = TOGGLE_OFF.search(text)
    if cut is None:
        return False, "no `[mods] cheats -> OFF` line: the menu did not toggle the mod"
    before = sum(1 for m in LINE.finditer(text) if m.start() < cut.start())
    after = sum(1 for m in LINE.finditer(text) if m.start() > cut.start()) + sum(1 for m in CALLGUEST.finditer(text) if m.start() > cut.start())
    if before < MIN_PRE_TOGGLE_LINES:
        return False, f"only {before} car lines before the toggle (need {MIN_PRE_TOGGLE_LINES}): the mod did not run"
    if after:
        return False, f"{after} mod log lines after the toggle: the mod ran while switched off"
    return True, f"the mod ran ({before} car lines) and was silent after the toggle"


def step_rate(prev: Car, cur: Car):
    """Distance covered per physics step per m/s of speed between two log lines, or None when the car is too slow to tell."""
    steps = cur.st - prev.st
    speed = (prev.spd + cur.spd) / 2
    if steps <= 0 or speed < RATE_MIN_SPEED:
        return None
    return dist(prev.pos, cur.pos) / steps / speed


def median(values):
    values = sorted(values)
    return values[len(values) // 2] if values else None


def judge_padoff(text: str):
    """The pad route that steps Slow motion OFF -> 0.5x -> 0.25x -> OFF: the distance per step per m/s follows the factor and is normal again after OFF."""
    cars = parse_lines(text)
    # group consecutive lines by phase (a run of the same slow label), skipping the settling lines of each phase
    phases = []
    for c in cars:
        if not phases or phases[-1][0] != c.slow:
            phases.append((c.slow, []))
        phases[-1][1].append(c)
    names = [p[0] for p in phases]
    if names != ["OFF", "0.5x", "0.25x", "OFF"]:
        return False, f"slow-motion phases {names}, expected OFF, 0.5x, 0.25x, OFF"
    measured = []
    for label, lines in phases:
        rs = [step_rate(a, b) for a, b in zip(lines[RATE_SETTLE_LINES:], lines[RATE_SETTLE_LINES + 1:])]
        measured.append(median([r for r in rs if r is not None]))
    if any(m is None for m in measured):
        return False, "a phase has no measurable lines (car too slow or phase too short)"
    base = measured[0]
    notes = []
    for (label, _), r, want in zip(phases, measured, (1.0, 0.5, 0.25, 1.0)):
        ratio = r / base
        if abs(ratio - want) > RATE_TOLERANCE[want]:
            return False, f"in the {label} phase the distance per step per m/s is {ratio:.2f} of normal, expected {want} (+-{RATE_TOLERANCE[want]})"
        notes.append(f"{label} {ratio:.2f}")
    return True, "distance per step per m/s relative to normal: " + ", ".join(notes) + " (back to normal after OFF)"


def judge_padreset(text: str):
    """The pad run with reset-to-track also enabled and the chord pressed with a stagger: the menu works and the game (reset-to-track) saw no Select."""
    ok, message = judge_pad(text)
    if not ok:
        return False, message
    seen = PADSEL.findall(text)
    if seen:
        return False, f"reset-to-track saw Select ({len(seen)} PADSEL/RESTORE lines): the staggered chord leaked a Select"
    return True, message + "; reset-to-track saw no Select"


def read(path: str) -> str:
    with open(path, errors="replace") as handle:
        return handle.read()


def report(label: str, ok: bool, message: str) -> int:
    print(("PASS " if ok else "FAIL ") + f"{label}: {message}")
    return 0 if ok else 1


def judge_replay(text):
    """The replay re-simulates the race from the recorded pad stream; cheats 0.4.0 records its controls per step live and plays them back
    there. Paired by the steps since the player's car first moved (a replay counts its steps from another point of the countdown): the same
    controls at every paired step, at least one cheat on, and every car (player and AI) within MAX_REPLAY_DRIFT of where it was live."""
    marker = text.find("REPLAY start")
    if marker < 0:
        return False, "no REPLAY start line: the mod never saw the player's car in its replay"
    def by_since(part, rep):
        out = {}
        for m in REPLAY_LINE.finditer(part):
            g = m.groups()
            since = int(g[-3])
            if since < 0 or int(g[-2]) != rep:
                continue
            ais = [tuple(float(x) for x in a.groups()) for a in AI.finditer(g[5])]
            out[since] = {"ctl": (g[0], g[1], g[-1]), "P": tuple(float(x) for x in g[2:5]), "A": ais}
        return out
    live, rep = by_since(text[:marker], 0), by_since(text[marker:], 1)
    common = sorted(set(live) & set(rep))
    if len(common) < MIN_LINES:
        return False, f"only {len(common)} steps logged both live and in the replay"
    differ = [k for k in common if live[k]["ctl"] != rep[k]["ctl"]]
    if differ:
        k = differ[0]
        return False, f"controls differ at +{k}: live {live[k]['ctl']}, replay {rep[k]['ctl']} ({len(differ)} steps)"
    on = [k for k in common if live[k]["ctl"] != ("OFF", "0", "1x")]
    if not on:
        return False, "no cheat was on in the compared steps: nothing to replay"
    worst = (0.0, "player", 0)
    for k in common:
        worst = max(worst, (math.dist(live[k]["P"], rep[k]["P"]), "player", k))
        for i, (a, b) in enumerate(zip(live[k]["A"], rep[k]["A"])):
            worst = max(worst, (math.dist(a, b), f"A{i}", k))
    if worst[0] > MAX_REPLAY_DRIFT:
        return False, f"the replayed {worst[1]} drifts {worst[0]:.2f} m from the live one at +{worst[2]} (limit {MAX_REPLAY_DRIFT})"
    return True, f"{len(common)} steps paired, the same controls ({len(on)} with a cheat on), largest drift {worst[0]:.2f} m ({worst[1]} at +{worst[2]})"


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
    if mode == "measure":  # measure LABEL FACTOR: print the numbers judge_slowmo compares (to set the tolerances from real run-to-run spread)
        m = measure_slowmo(log("idle"), log(label), float(argv[4]))
        print(f"MEASURE {label} factor={argv[4]} " + ("unusable log" if m is None else " ".join(f"{k}={v:.4f}" for k, v in m.items())))
        return 0 if m is not None else 1
    if mode == "slowmo":
        factor = float(argv[4])
        if factor not in AI_SLOWER:
            print("slowmo factor must be 0.5 or 0.25", file=sys.stderr)
            return 2
        ok, message = judge_slowmo(log("idle"), log(label), factor)
        return report(f"slowmo {factor} ({label})", ok, message)
    if mode == "replay":
        ok, message = judge_replay(log(label))
        return report(f"replay ({label})", ok, message)
    if mode == "freeze":
        ok, message = judge_freeze(log("idle"), log(label))
        return report(f"freeze ({label})", ok, message)
    if mode in ("padoff", "padreset", "callguest", "toggle"):
        judge = {"padoff": judge_padoff, "padreset": judge_padreset, "callguest": judge_callguest, "toggle": judge_toggle}[mode]
        if mode == "toggle":
            ok, message = judge(log(label))
            if ok:
                ok2, message2 = judge_identity(frames_of(os.path.join(out, "baseline", "frames")), frames_of(os.path.join(out, label, "frames")))
                ok, message = ok2, message + "; " + message2 + " (the whole run)"
            return report(f"{mode} ({label})", ok, message)
        ok, message = judge(log(label))
        return report(f"{mode} ({label})", ok, message)
    if mode in ("hotkey", "conflict"):
        ok, message = (judge_hotkey if mode == "hotkey" else judge_conflict)(log(label))
        return report(f"{mode} ({label})", ok, message)
    if mode == "pad":
        ok, message = judge_pad(log(label))
        return report(f"pad ({label})", ok, message)
    print(f"unknown mode {mode}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
