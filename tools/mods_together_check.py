"""Judge of tools/mods_together_check.sh: every gt-mods mod installed and enabled at once, one gt_play run.

PASS when the run's log shows
  - `[mods] loaded:` naming every installed mod id and an empty `skipped:` list (no hook conflict, no init failure),
  - `script done at vblank N` (the pad route ran to its end: no crash, no stall), and
  - for every mod with a rule in ACTED, a log line proving that mod acted while the others were loaded too.
A mod id without a rule only has to load; add its rule here when it gets a log line worth asserting.

  python -m tools.mods_together_check RUN_LOG ID [ID ...]      exit 0 pass, 1 fail, 2 usage/unreadable log
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

LOADED = re.compile(r"^\[mods\] loaded:(?P<loaded>[^\n]*?) skipped:(?P<skipped>[^\n]*)$", re.M)
SCRIPT_DONE = re.compile(r"script done at vblank \d+")

# Per mod id: the log line that proves the mod did its job in the run (the shell script sets each mod's fake/log env).
ACTED: dict[str, re.Pattern[str]] = {
    # GT_MOD_PEDALS_FAKE=0.5,0.3 + GT_MOD_PEDALS_LOG=1: the player's pedals follow the fake input (to 1e-4: the game's
    # record quantisation gives 0.4999)
    "analog-pedals": re.compile(r"^\[mod:analog-pedals\] step=[1-9]\d* player thr=0\.(?:4999|5000|5001) brk=0\.(?:2999|3000|3001)", re.M),
    # GT_MOD_CAMERA_FAKE=90 + GT_MOD_CAMERA_LOG=1: the camera turned and world uploads were edited
    "orbit-camera": re.compile(r"^\[mod:orbit-camera\] step=\d+ yaw=-?90\.00 .* edits=[1-9]\d*", re.M),
    # GT_MOD_DRAWDIST_LOG=1: the world pass was matched
    "draw-distance": re.compile(r"^\[mod:draw-distance\] world depth call from ra=", re.M),
    # GT_MOD_RESET_LOG=1: the chase route's short SELECT presses were hidden from the game and replayed as taps (no car is
    # moving fast enough on that route for a reset, so the pad hook is what acts)
    "reset-to-track": re.compile(r"^\[mod:reset-to-track\] PADSEL up step=\d+ tap=1", re.M),
    # GT_MOD_ROLLOVER_FAKE=<step>:5 + GT_MOD_ROLLOVER_LOG=1: a fake 5 m/s lateral velocity change at that step is answered with a roll impulse
    # GT_MOD_CHEATS_SLOWMO=0.5 + GT_MOD_CHEATS_FREEZE_AI=1 + GT_MOD_CHEATS_LOG=1: the car log says both cheats are on (and the AI cars are still)
    "cheats": re.compile(r"^\[mod:cheats\] st=[1-9]\d* slow=0\.5x freeze=1 P=", re.M),
    "rollover": re.compile(r"^\[mod:rollover\] IMPULSE st=[1-9]\d* held=-?[1-9]\d*\.\d\d", re.M),
}


def judge(log: str, ids: list[str]) -> tuple[bool, list[str]]:
    """(passed, one line per problem) for a run log and the installed mod ids."""
    problems: list[str] = []
    m = LOADED.search(log)
    if m is None:
        problems.append("no `[mods] loaded:` line (mods not loaded at all)")
    else:
        loaded = m.group("loaded").split()
        missing = [i for i in ids if i not in loaded]
        if missing:
            problems.append(f"not loaded: {' '.join(missing)}")
        if m.group("skipped").strip():
            problems.append(f"skipped:{m.group('skipped')}")
    if not SCRIPT_DONE.search(log):
        problems.append("no `script done` (the route did not finish)")
    for mod_id in ids:
        rule = ACTED.get(mod_id)
        if rule is not None and not rule.search(log):
            problems.append(f"{mod_id}: no log line proving it acted ({rule.pattern})")
    return not problems, problems


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: python -m tools.mods_together_check RUN_LOG ID [ID ...]", file=sys.stderr)
        return 2
    try:
        log = Path(argv[0]).read_text(errors="replace")
    except OSError as e:
        print(f"mods_together_check: {e}", file=sys.stderr)
        return 2
    ok, problems = judge(log, argv[1:])
    for p in problems:
        print(f"  {p}")
    print(f"mods_together_check: {'PASS' if ok else 'FAIL'} ({len(argv) - 1} mods: {' '.join(argv[1:])})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
