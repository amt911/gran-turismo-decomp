"""Verdict of tools/mod_pedals_check.sh: reads the analog-pedals mod's log lines from a gt_play run.

The mod injects throttle 0.5 and brake 0.3 into the player's input record (analog flags + s16 fields). The game turns the
record into pedal floats with its own response: the accelerator goes through a response curve (0.5 -> 0.1035, measured
2026-11-04) and the brake through `x + (1 - x) * 0.1` (0.3 -> 0.37, constant at 0x08CDE108 = 0.1). Pass when, after the
first SETTLE_STEPS physics steps, the player's pedals (car+8+0x530 accelerator, +0x538 brake) sit at those responses on
every sample and the AI car's pedals are not the player's on every sample (the injection is player-only).

Line format (mod.cpp): `[mod:analog-pedals] step=N player thr=T brk=B eff=E ai thr=T brk=B`.
"""
import re
import sys

LINE = re.compile(r"\[mod:analog-pedals\] step=(\d+) player thr=([-\d.]+) brk=([-\d.]+) eff=([-\d.]+) ai thr=([-\d.]+) brk=([-\d.]+)")
SETTLE_STEPS = 600
MIN_SAMPLES = 5
EXPECT_THROTTLE = 0.1035  # game response to an injected 0.5
EXPECT_BRAKE = 0.37  # game response to an injected 0.3
TOLERANCE = 0.01


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


def main(argv):
    with open(argv[1], errors="replace") as handle:
        ok, message = judge(handle.read())
    print(("PASS: " if ok else "FAIL: ") + message)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
