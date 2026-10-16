"""CPU use per thread of a running process (Linux /proc), in percent of one core, every few seconds.

gt_play runs the game (the recompiled code and the HLE) on one thread and the GE lists on another, and the perf log only
times the GE thread; this says which thread is the one at 100 % when the frame rate drops (docs/PRUEBAS_PORTATIL.md).

Usage: .venv/bin/python -m tools.thread_cpu [--pid PID | --name gt_play] [--every 5] [--count 0]
  --name finds the process by its /proc comm (never this tool's own process); --count 0 runs until it exits.
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from collections.abc import Sequence
from pathlib import Path

Sample = dict[int, tuple[str, int]]


def sample(pid: int, proc: Path = Path("/proc")) -> Sample:
    """utime + stime ticks per thread id, with the thread's name."""
    threads: Sample = {}
    for task in (proc / str(pid) / "task").iterdir():
        try:
            text = (task / "stat").read_text()
        except OSError:
            continue  # the thread ended between the listing and the read
        name = text[text.index("(") + 1 : text.rindex(")")]
        fields = text[text.rindex(")") + 2 :].split()
        threads[int(task.name)] = (name, int(fields[11]) + int(fields[12]))
    return threads


def usage(before: Sample, after: Sample, seconds: float, ticks_per_second: int) -> list[tuple[str, int, float]]:
    """(name, tid, percent of one core) for the threads alive at the second sample, busiest first."""
    rows = []
    for tid, (name, ticks) in after.items():
        start = before.get(tid, (name, 0))[1]
        rows.append((name, tid, 100.0 * (ticks - start) / ticks_per_second / seconds))
    return sorted(rows, key=lambda row: (-row[2], row[1]))


def format_usage(rows: Sequence[tuple[str, int, float]]) -> str:
    return "\n".join(f"{percent:6.1f}%  {name} ({tid})" for name, tid, percent in rows)


def find_pid(name: str, proc: Path = Path("/proc")) -> int | None:
    own = os.getpid()
    for entry in proc.iterdir():
        if not entry.name.isdigit() or int(entry.name) == own:
            continue
        try:
            if (entry / "comm").read_text().strip() == name:
                return int(entry.name)
        except OSError:
            continue
    return None


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--pid", type=int)
    target.add_argument("--name")
    parser.add_argument("--every", type=float, default=5.0)
    parser.add_argument("--count", type=int, default=0)
    args = parser.parse_args(argv)
    pid = args.pid if args.pid is not None else find_pid(args.name)
    if pid is None:
        print(f"thread_cpu: no process named {args.name}", file=sys.stderr)
        return 2
    ticks = os.sysconf("SC_CLK_TCK")
    windows = 0
    try:
        before = sample(pid)
        while args.count == 0 or windows < args.count:
            time.sleep(args.every)
            after = sample(pid)
            windows += 1
            print(f"-- {windows * args.every:.0f} s", flush=True)
            print(format_usage([row for row in usage(before, after, args.every, ticks) if row[2] >= 1.0]), flush=True)
            before = after
    except FileNotFoundError:
        pass  # the process exited
    return 0


if __name__ == "__main__":
    sys.exit(main())
