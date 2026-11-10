"""Verdict of tools/mod_pedals_check.sh: reads the analog-pedals mod's log lines from a gt_play run.

The mod asks for throttle 0.5 and brake 0.3; it writes the input-record values whose game response (docs/FACTS.md: accelerator
x^8 then a 10 % floor, brake a 10 % floor) is exactly those pedals. Pass when, after the first SETTLE_STEPS physics steps, the
player's pedals (car+8+0x530 accelerator, +0x538 brake) are within TOLERANCE of 0.5 / 0.3 on every sample and the AI car's pedals
are not the player's on every sample (the injection is player-only).

Line format (mod.cpp): `[mod:analog-pedals] step=N player thr=T brk=B eff=E ai thr=T brk=B`.
"""
import re
import sys

LINE = re.compile(r"\[mod:analog-pedals\] step=(\d+) player thr=([-\d.]+) brk=([-\d.]+) eff=([-\d.]+) ai thr=([-\d.]+) brk=([-\d.]+)")
SETTLE_STEPS = 600
MIN_SAMPLES = 5
EXPECT_THROTTLE = 0.5
EXPECT_BRAKE = 0.3
TOLERANCE = 0.02


def judge(text, want_throttle=EXPECT_THROTTLE, want_brake=EXPECT_BRAKE):
    """Returns (ok, message)."""
    rows = [(a, b, c, e, f) for a, b, c, _eff, e, f in (tuple(float(x) for x in m.groups()) for m in LINE.finditer(text))]
    rows = [r for r in rows if r[0] > SETTLE_STEPS]
    if len(rows) < MIN_SAMPLES:
        return False, f"only {len(rows)} settled mod log lines (need {MIN_SAMPLES}): the mod did not run or no race was reached"
    bad = [r for r in rows if abs(r[1] - want_throttle) > TOLERANCE or abs(r[2] - want_brake) > TOLERANCE]
    if bad:
        return False, (f"player pedals off target on {len(bad)}/{len(rows)} samples, e.g. step {bad[0][0]:.0f}: "
                       f"thr={bad[0][1]} (want {want_throttle}) brk={bad[0][2]} (want {want_brake})")
    ai_same = [r for r in rows if abs(r[3] - r[1]) <= TOLERANCE and abs(r[4] - r[2]) <= TOLERANCE]
    if len(ai_same) == len(rows):
        return False, "the AI car shows the player's pedals on every sample (the hooks are not player-only)"
    return True, f"{len(rows)} samples: player thr={rows[-1][1]:.4f} brk={rows[-1][2]:.4f}, AI thr={rows[-1][3]:.4f} brk={rows[-1][4]:.4f}"


TOGGLE_LINE = re.compile(r"^\[mods\] \S+ -> OFF$", re.M)


def judge_toggle(text):
    """The mod menu turned the mod off mid-run (`[mods] <id> -> OFF`, GT_MOD_MENU_TEST_TOGGLE): the mod's pedal log must show the
    injection (judge) before that line and no mod line at all after it -- an inactive mod's code never runs, so its log stops.
    Returns (ok, message)."""
    cut = TOGGLE_LINE.search(text)
    if cut is None:
        return False, "no `[mods] <id> -> OFF` toggle line in the log: the menu did not toggle the mod"
    before, after = text[: cut.start()], text[cut.end() :]
    ok, message = judge(before)
    if not ok:
        return False, "before the toggle: " + message
    later = LINE.findall(after)
    if later:
        return False, f"the mod still ran after the toggle ({len(later)} log lines after it): the hooks ignored `active`"
    return True, f"injected before the toggle ({message}); no mod activity after it"


REPLAY_LINE = re.compile(r"\[mod:analog-pedals\] replay step=(\d+) thr=([-\d.]+) brk=([-\d.]+) eff=([-\d.]+)(?: speed=([-\d.]+))?")
REPLAY_THROTTLE = 0.6  # --replay runs with GT_MOD_PEDALS_FAKE=0.6,0 on circuit_analog (no digital throttle at all)
REPLAY_MOVED = 5.0     # the replayed car's speed (car+8+0x6C4) must pass this at least once


def judge_replay(text, want_throttle=REPLAY_THROTTLE):
    """--replay: the race was driven by the mod's analog throttle alone, then its replay played. The live samples must show
    the throttle (judge), and the replay must replay it: the replayed human car (entry +0xA8 set) shows the throttle on every
    sample and moves. Before mod 0.2.0 the replay recorded only digital presses: thr 0, speed 0. Returns (ok, message)."""
    ok, message = judge(text, want_throttle, 0.0)
    if not ok:
        return False, "live: " + message
    rows = [tuple(float(x) if x is not None else None for x in m.groups()) for m in REPLAY_LINE.finditer(text)]
    if len(rows) < MIN_SAMPLES:
        return False, f"only {len(rows)} replay log lines (need {MIN_SAMPLES}): the route did not reach the replay"
    bad = [r for r in rows if abs(r[1] - want_throttle) > TOLERANCE]
    if bad:
        return False, (f"replay throttle off on {len(bad)}/{len(rows)} samples, e.g. replay step {bad[0][0]:.0f}: "
                       f"thr={bad[0][1]} (want {want_throttle}): the replay did not record the analog throttle")
    if any(r[4] is None for r in rows):
        return False, "the replay lines carry no speed (analog-pedals < 0.2.0 does not log it): cannot tell the car moved"
    top = max(r[4] for r in rows)
    if top < REPLAY_MOVED:
        return False, f"the replayed car never moved (top speed {top:.2f} < {REPLAY_MOVED})"
    return True, f"live: {message}; replay: {len(rows)} samples thr={rows[-1][1]:.4f}, top speed {top:.2f}"


def main(argv):
    with open(argv[1], errors="replace") as handle:
        text = handle.read()
    mode = argv[2] if len(argv) > 2 else ""
    ok, message = judge_toggle(text) if mode == "--toggle" else judge_replay(text) if mode == "--replay" else judge(text)
    print(("PASS: " if ok else "FAIL: ") + message)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
