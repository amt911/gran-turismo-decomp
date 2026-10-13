"""The differential gate of the decomp (docs/agents/decomp-workflow.md § Working in batches), as a tool.

Usage (from the repository root):
  .venv/bin/python -m tools.decomp_gate fast TAG --only 0xADDR,0xADDR...|@file   # listed functions, uncapped
  .venv/bin/python -m tools.decomp_gate full TAG                                 # everything, real time
  .venv/bin/python -m tools.decomp_gate sabotage TAG --file src/x.cpp --only ...  # s0 bump, fast gate, restore
  .venv/bin/python -m tools.decomp_gate report TAG                               # re-read a run's logs

`fast` and `full` regenerate the recompiled code (tools.recomp: registrations are boundaries), build, and
run the three pad routes in parallel with `build/gt_native --decomp diff`, all inside the shared heavy-job
slice whose 6 GB ceiling caps all heavy jobs together, at most two at once (AGENTS.md). Logs go to recomp/analysis/gt_diff_<TAG>_<route>.log.

A function is verified when a run compared it (compared > 0) with 0 mismatches, and its sabotage — `s0`
bumped at entry (`ctx.gpr[16] += 1u`), which every comparison sees — was detected. The full gate is the
regression check over everything; script-driven functions are often reached only by the faster `fast` runs.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, replace
from pathlib import Path

ROUTES = {
    "fast": (("race", 200), ("dealer", 100), ("garage", 103)),
    "full": (("race", 600), ("dealer", 300), ("garage", 310)),
}
# Heavy jobs share one systemd slice whose memory ceiling caps them all together (AGENTS.md: 6 GB on this
# machine), so two can run at once with the same guarantee a per-job cap plus a global lock gave serially.
SLICE = "gt-heavy.slice"
SLICE_LIMITS = ("MemoryHigh=5G", "MemoryMax=6G", "MemorySwapMax=0")
SLOTS = (Path("/tmp/claude-1000/gt-heavy.lock"), Path("/tmp/claude-1000/gt-heavy-2.lock"))
BUILD_JOBS = 8  # ~1.7 GB peak (0.86 GB measured at -j4), well inside the slice

_LINE = re.compile(
    r"^\[decomp\] (?P<name>\S+) @0x(?P<address>[0-9A-Fa-f]{8}): compared (?P<compared>\d+), equal (?P<equal>\d+), "
    r"mismatched (?P<mismatched>\d+), not comparable (?P<not_comparable>\d+) \(calls (?P<calls>\d+)",
    re.MULTILINE,
)
_FIRST_MISMATCH = re.compile(r"^\[decomp\]   first mismatch: (.*)$")
_STOP = re.compile(r"^Runtime stopped: (.*)$", re.MULTILINE)
# An active registration: GT_DECOMP(0x..u, ns::name, ...) or a subsystem's shorthand NAME(0x..u, name, ...).
_REGISTRATION = re.compile(r"^[ \t]*[A-Z][A-Z0-9_]*\(\s*0x([0-9A-Fa-f]{8})u?\s*,\s*([A-Za-z_][\w:<>]*)", re.MULTILINE)


@dataclass(frozen=True)
class Stats:
    name: str
    compared: int = 0
    equal: int = 0
    mismatched: int = 0
    not_comparable: int = 0
    calls: int = 0
    first_mismatch: str = ""


@dataclass(frozen=True)
class Summary:
    functions: int
    compared: int
    mismatched: int
    never_compared: int


def parse_report(text: str) -> dict[int, Stats]:
    """Per-function results of one route's `--decomp diff` report."""
    stats: dict[int, Stats] = {}
    last: int | None = None
    for line in text.splitlines():
        match = _LINE.match(line)
        if match:
            last = int(match["address"], 16)
            stats[last] = Stats(match["name"], *(int(match[k]) for k in
                                                  ("compared", "equal", "mismatched", "not_comparable", "calls")))
            continue
        first = _FIRST_MISMATCH.match(line)
        if first and last is not None and not stats[last].first_mismatch:
            stats[last] = replace(stats[last], first_mismatch=first[1])
    return stats


def not_installed(text: str) -> int:
    return text.count("not installed")


def stop_reason(text: str) -> str:
    match = _STOP.search(text)
    return match[1].strip() if match else ""


def combine(reports: Iterable[dict[int, Stats]]) -> dict[int, Stats]:
    """The routes' results summed per function."""
    total: dict[int, Stats] = {}
    for report in reports:
        for address, s in report.items():
            t = total.get(address)
            total[address] = s if t is None else Stats(
                t.name, t.compared + s.compared, t.equal + s.equal, t.mismatched + s.mismatched,
                t.not_comparable + s.not_comparable, t.calls + s.calls, t.first_mismatch or s.first_mismatch)
    return total


def summarize(total: dict[int, Stats]) -> Summary:
    return Summary(functions=len(total),
                   compared=sum(1 for s in total.values() if s.compared > 0),
                   mismatched=sum(1 for s in total.values() if s.mismatched > 0),
                   never_compared=sum(1 for s in total.values() if s.compared == 0))


def sabotage(source: str, names: Sequence[str]) -> tuple[str, list[str]]:
    """Inserts `ctx.gpr[16] += 1u;` as the first statement of each named function; returns the names not found."""
    missing = []
    for name in names:
        pattern = re.compile(r"^(void " + re.escape(name) +
                             r"\(psprecomp::Runtime &\w*, psprecomp::AllegrexContext &ctx\) \{)", re.MULTILINE)
        source, count = pattern.subn(r"\1 ctx.gpr[16] += 1u;", source, count=1)
        if count == 0:
            missing.append(name)
    return source, missing


def registered_names(source: str) -> dict[int, str]:
    """Address -> function name (last path component) of the active registrations in a source file."""
    names = {}
    for match in _REGISTRATION.finditer(source):
        name = match[2].split("::")[-1]
        if name.isidentifier():
            names[int(match[1], 16)] = name
    return names


def parse_only(text: str) -> list[int]:
    """Addresses from "0xA, B ..." or "@file" (one or more per line)."""
    if text.startswith("@"):
        text = Path(text[1:]).read_text()
    out = []
    for token in re.split(r"[\s,]+", text.strip()):
        if token:
            try:
                out.append(int(token, 16))
            except ValueError as error:
                raise ValueError(f"not a hex address: {token}") from error
    return out


def route_shell(binary: Path, kind: str, tag: str, logs: Path) -> str:
    """One shell line running the three routes in parallel, each logging its report and exit code."""
    parts = []
    for route, seconds in ROUTES[kind]:
        log = logs / f"gt_diff_{tag}_{route}"
        parts.append(f'( GT_PAD="$(grep -v \'^#\' config/pad_routes/{route}.pad)" timeout 1000 {binary} '
                     f"--max-seconds {seconds} --decomp diff > {log}.log 2>&1; echo $? > {log}.exit ) &")
    return " ".join(parts) + " wait"


def clear_run(tag: str, logs: Path) -> None:
    """Removes a tag's logs and exit codes from an earlier run, so a run that dies early can't be read
    with an old run's results."""
    for route, _ in ROUTES["full"]:
        for suffix in (".log", ".exit"):
            (logs / f"gt_diff_{tag}_{route}{suffix}").unlink(missing_ok=True)


def slice_limits() -> list[str]:
    """Sets (runtime only, nothing persistent) the shared ceiling of every heavy job."""
    return ["systemctl", "--user", "set-property", "--runtime", SLICE, *SLICE_LIMITS]


def heavy(command: Sequence[str]) -> list[str]:
    """`command` in a scope inside the shared heavy-job slice."""
    return ["systemd-run", "--user", "--scope", "--quiet", f"--slice={SLICE}", "--", *command]


class HeavySlot:
    """One of len(locks) slots for a heavy job: at most that many run at once (flock on one file each).

    An exclusive slot holds every lock, for a step too big to share the ceiling: tools.recomp peaks at
    3.8 GB, so beside a gate or a second recomp it pushes the slice into throttling, and throttled gates
    time out on their real-time routes (measured 2026-10-13).
    """

    def __init__(self, locks: Sequence[Path] = SLOTS, exclusive: bool = False):
        self._locks = list(locks)
        self._exclusive = exclusive
        self._files: list = []

    @staticmethod
    def _try(path: Path):
        import fcntl
        path.parent.mkdir(parents=True, exist_ok=True)
        handle = open(path, "a")  # noqa: SIM115 - kept open while the slot is held
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            handle.close()
            return None
        return handle

    def acquire(self, block: bool = True) -> bool:
        import time
        while True:
            if self._exclusive and block:
                return self._acquire_all_in_order()
            if self._exclusive:
                held = []
                for path in self._locks:
                    handle = self._try(path)
                    if handle is None:
                        break
                    held.append(handle)
                if len(held) == len(self._locks):
                    self._files = held
                    return True
                for handle in held:
                    handle.close()
                return False
            else:
                for path in self._locks:
                    handle = self._try(path)
                    if handle is not None:
                        self._files = [handle]
                        return True
            if not block:
                return False
            time.sleep(2)

    def _acquire_all_in_order(self) -> bool:
        """Blocks on each lock in turn, always in the same order, keeping what it has: two exclusive
        waiters can't deadlock (both wait on the first lock) and none releases and retries (no livelock).
        A shared job may still take a later lock meanwhile; the wait is then one job long."""
        import fcntl
        for path in self._locks:
            path.parent.mkdir(parents=True, exist_ok=True)
            handle = open(path, "a")  # noqa: SIM115 - kept open while the slot is held
            fcntl.flock(handle, fcntl.LOCK_EX)
            self._files.append(handle)
        return True

    def release(self) -> None:
        for handle in self._files:
            handle.close()  # closing drops the flock
        self._files = []


def route_problem(text: str, exit_code: str) -> str | None:
    """Why a route's run can't be trusted, or None. Exit 2 is gt_native's "mismatches found", which the
    per-function lines already report; a timeout, a crash or a missing report fails on its own."""
    if exit_code == "124":
        return "timed out (killed by timeout; no trustworthy report)"
    if not _LINE.search(text):
        return "no report (the run ended before printing its results)"
    if exit_code not in ("0", "2"):
        return f"exit {exit_code}"
    return None


def _run_gate(kind: str, tag: str, only: list[int] | None) -> int:  # pragma: no cover - drives the build
    env = None
    if only is not None:
        import os
        env = dict(os.environ, GT_DECOMP_ONLY=",".join(f"0x{a:08X}" for a in only), LCS_UNCAPPED="1")
    clear_run(tag, Path("recomp/analysis"))
    subprocess.run(slice_limits(), check=True)
    recomp = HeavySlot(exclusive=True)  # the 3.8 GB step runs alone
    recomp.acquire()
    try:
        recomp_ok = subprocess.run(heavy([sys.executable, "-m", "tools.recomp"]),
                                   stdout=subprocess.DEVNULL).returncode == 0
    finally:
        recomp.release()
    if not recomp_ok:
        print("recomp failed")
        return 2
    slot = HeavySlot()
    slot.acquire()
    try:
        return _gate_steps(kind, tag, env)
    finally:
        slot.release()


def _gate_steps(kind: str, tag: str, env: dict[str, str] | None) -> int:  # pragma: no cover - drives the build
    build = subprocess.run(heavy(["cmake", "--build", "build", f"-j{BUILD_JOBS}"]), capture_output=True, text=True)
    if build.returncode != 0:
        print("\n".join(line for line in build.stdout.splitlines() if "error" in line)[:2000] or "build failed")
        return 2
    subprocess.run(heavy(["sh", "-c", route_shell(Path("build/gt_native"), kind, tag, Path("recomp/analysis"))]),
                   env=env)
    return _report(tag)


def _report(tag: str) -> int:  # pragma: no cover - reads the run's files
    reports = []
    status = 0
    for route, _ in ROUTES["full"]:
        log = Path(f"recomp/analysis/gt_diff_{tag}_{route}.log")
        text = log.read_text(errors="replace") if log.exists() else ""
        exit_file = log.with_suffix(".exit")
        code = exit_file.read_text().strip() if exit_file.exists() else "?"
        report = parse_report(text)
        s = summarize(report)
        print(f"{route}: exit {code}, {stop_reason(text)}; {s.functions} functions, {s.compared} compared, "
              f"{s.mismatched} mismatched, {not_installed(text)} not installed")
        reports.append(report)
        problem = route_problem(text, code)
        if problem:
            print(f"  {route}: {problem}")
        if s.mismatched or not_installed(text) or problem:
            status = 1
    total = combine(reports)
    s = summarize(total)
    print(f"all routes: {s.functions} functions, {s.compared} compared, {s.mismatched} mismatched, "
          f"{s.never_compared} never compared")
    for address, st in sorted(total.items()):
        if st.mismatched:
            print(f"  MISMATCH {st.name} @0x{address:08X}: {st.first_mismatch}")
    return status


def _sabotage_run(tag: str, file: Path, only: list[int]) -> int:  # pragma: no cover - edits, runs, restores
    original = file.read_text()
    names = [n for a, n in registered_names(original).items() if a in set(only)]
    sabotaged, missing = sabotage(original, names)
    if missing:
        print("not found in", file, ":", ", ".join(missing))
    try:
        file.write_text(sabotaged)
        ran = _run_gate("fast", tag, only)
    finally:
        file.write_text(original)
    if ran == 2:  # recomp or build failed: nothing ran, so nothing was detected
        print("sabotage: the sabotaged build did not run")
        return 2
    total = combine(parse_report(Path(f"recomp/analysis/gt_diff_{tag}_{r}.log").read_text(errors="replace"))
                    for r, _ in ROUTES["full"] if Path(f"recomp/analysis/gt_diff_{tag}_{r}.log").exists())
    undetected = [f"0x{a:08X}" for a in only if total.get(a) is None or total[a].mismatched == 0]
    print(f"sabotage: {len(only) - len(undetected)}/{len(only)} detected; undetected: {undetected}")
    return 0 if not undetected else 1


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - CLI wiring
    parser = argparse.ArgumentParser(prog="tools.decomp_gate", description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    fast = sub.add_parser("fast")
    fast.add_argument("tag")
    fast.add_argument("--only", required=True)
    full = sub.add_parser("full")
    full.add_argument("tag")
    sab = sub.add_parser("sabotage")
    sab.add_argument("tag")
    sab.add_argument("--file", type=Path, required=True)
    sab.add_argument("--only", required=True)
    rep = sub.add_parser("report")
    rep.add_argument("tag")
    args = parser.parse_args(argv)
    if args.command == "fast":
        return _run_gate("fast", args.tag, parse_only(args.only))
    if args.command == "full":
        return _run_gate("full", args.tag, None)
    if args.command == "sabotage":
        return _sabotage_run(args.tag, args.file, parse_only(args.only))
    return _report(args.tag)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
