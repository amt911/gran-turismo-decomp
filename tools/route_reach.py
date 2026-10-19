"""What a new pad route adds: the functions its probe log reached that the other routes did not.

Probe logs come from `GT_PROBE_SYMBOLS=<file> gt_native --decomp off` (docs/agents/decomp-workflow.md): one
`[probe] 0xADDR hits=N ...` line per probed function that was entered. Usage:

  .venv/bin/python -m tools.route_reach --baseline recomp/analysis/probe_T_race.log ... \\
      --new drift=recomp/analysis/probe_T_drift.log --new challenge=recomp/analysis/probe_T_challenge.log

prints, per new route, the functions it reached, how many the baseline routes never reached, and how many no
earlier new route reached either, then the totals. A log of a run that timed out or was killed is refused: its
hit counts are partial (the `<log>.exit` file next to it holds the exit code when `tools.pad_route.sh` wrote it).
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path

_PROBE = re.compile(r"^\[probe\] 0x([0-9A-Fa-f]{8}) hits=(\d+)", re.MULTILINE)


@dataclass(frozen=True)
class Row:
    name: str
    reached: int
    new_vs_baseline: int
    new_vs_previous: int  # not reached by the baseline nor by an earlier new route


def reached(log: str) -> set[int]:
    """Addresses of the probed functions the run entered (hits > 0)."""
    return {int(m.group(1), 16) for m in _PROBE.finditer(log) if int(m.group(2)) > 0}


def load(path: Path) -> set[int]:
    return reached(path.read_text(errors="replace"))


def problem(exit_code: str) -> str | None:
    """Why a run's hit counts can't be trusted, or None: a timeout, a WAIT that failed (97) or a crash."""
    code = exit_code.strip()
    if code == "0":
        return None
    if code == "124":
        return "timed out (killed by timeout; the hit counts are partial)"
    if code == "?":
        return "no exit code (the run did not finish)"
    return f"exit {code}"


def union(baseline: set[int], routes: Iterable[set[int]]) -> set[int]:
    total = set(baseline)
    for route in routes:
        total |= route
    return total


def additions(baseline: set[int], routes: Sequence[tuple[str, set[int]]]) -> list[Row]:
    rows = []
    seen = set(baseline)
    for name, functions in routes:
        rows.append(Row(name, len(functions), len(functions - baseline), len(functions - seen)))
        seen |= functions
    return rows


def _exit_of(log: Path) -> str:
    exit_file = log.with_suffix(".exit")
    return exit_file.read_text() if exit_file.exists() else "0"  # a log without an exit file: trust it


def main(argv: list[str] | None = None) -> int:  # pragma: no cover - CLI wiring
    parser = argparse.ArgumentParser(prog="tools.route_reach", description=__doc__.splitlines()[0])
    parser.add_argument("--baseline", nargs="+", type=Path, required=True)
    parser.add_argument("--new", action="append", required=True, metavar="NAME=LOG")
    args = parser.parse_args(argv)
    for log in [*args.baseline, *(Path(item.split("=", 1)[1]) for item in args.new)]:
        why = problem(_exit_of(log))
        if why is not None:
            print(f"{log}: {why}", file=sys.stderr)
            return 2
    base: set[int] = set()
    for log in args.baseline:
        base |= load(log)
    routes = [(item.split("=", 1)[0], load(Path(item.split("=", 1)[1]))) for item in args.new]
    print(f"baseline routes reach {len(base)} functions")
    for row in additions(base, routes):
        print(f"{row.name:>10}: reaches {row.reached}, {row.new_vs_baseline} not reached by the baseline, "
              f"{row.new_vs_previous} not reached by the baseline or an earlier new route")
    print(f"all together: {len(union(base, [functions for _, functions in routes]))} functions")
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
