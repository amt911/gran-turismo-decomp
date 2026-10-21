"""The differential gate of the decomp (docs/agents/decomp-workflow.md § Working in batches), as a tool.

Usage (from the repository root):
  .venv/bin/python -m tools.decomp_gate fast TAG --only 0xADDR,0xADDR...|@file   # listed functions, uncapped
  .venv/bin/python -m tools.decomp_gate full TAG                                 # everything, real time
  .venv/bin/python -m tools.decomp_gate sabotage TAG --file src/x.cpp --only ... [--also ...]  # s0 bump, fast gate
  .venv/bin/python -m tools.decomp_gate coverage TAG --only 0xADDR,...|@file     # fast gate + which branches it took
  .venv/bin/python -m tools.decomp_gate probe TAG --symbols FILE                 # call counts of FILE's functions (--decomp off)
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
import json
import shutil
import re
import subprocess
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, replace
from pathlib import Path

ROUTES = {
    "fast": (("race", 200), ("dealer", 100), ("garage", 103), ("buy", 110), ("options", 230),
             ("drift", 160), ("challenge", 140), ("dirt", 215), ("manual", 150),
             ("circuit", 200), ("challenge2", 200)),
    # Each full route runs until its pad script is done (checked) with ~10 % to spare; they run in parallel, so the
    # race (600 s) sets the gate's length. Measured script ends (vblanks / 60, 2026-10-20): buy 194, options 409,
    # drift 293, challenge 253, dirt ~380, manual 280, circuit 302, challenge2 271.
    "full": (("race", 600), ("dealer", 300), ("garage", 310), ("buy", 220), ("options", 470),
             ("drift", 330), ("challenge", 285), ("dirt", 420), ("manual", 315),
             ("circuit", 335), ("challenge2", 300)),
}
# A full route ends this many vblanks after its pad script is done (GT_PAD_EXIT_AFTER), not on the wall clock: the
# 11 parallel routes run slower than real time on a loaded machine and a wall-clock end cut their scripts short. The
# race's script ends at the start of the race; its tail is the rest of its 600 s of racing.
DEFAULT_TAIL = 600
FULL_TAIL = {"race": 23400}
# Heavy jobs share one systemd slice whose memory ceiling caps them all together (AGENTS.md: 6 GB on this
# machine), so two can run at once with the same guarantee a per-job cap plus a global lock gave serially.
SLICE = "gt-heavy.slice"
SLICE_LIMITS = ("MemoryHigh=5G", "MemoryMax=6G", "MemorySwapMax=0")
SLOTS = (Path("/tmp/claude-1000/gt-heavy.lock"), Path("/tmp/claude-1000/gt-heavy-2.lock"))
MAX_SABOTAGE_ROUNDS = 4  # a parent and its decompiled callees need one round each when they share every case
BUILD_JOBS = 8  # ~1.7 GB peak (0.86 GB measured at -j4), well inside the slice

_LINE = re.compile(
    r"^\[decomp\] (?P<name>\S+) @0x(?P<address>[0-9A-Fa-f]{8}): compared (?P<compared>\d+), equal (?P<equal>\d+), "
    r"mismatched (?P<mismatched>\d+), not comparable (?P<not_comparable>\d+) \(calls (?P<calls>\d+)"
    r"(?:.*?, nested in (?P<nested_equal>\d+) equal / (?P<nested_mismatched>\d+) mismatched cases)?",
    re.MULTILINE,
)
_MISMATCH_CASE = re.compile(r"^\[decomp\] mismatch case: 0x([0-9A-Fa-f]{8}) nested (\S+)$", re.MULTILINE)
_FIRST_MISMATCH = re.compile(r"^\[decomp\]   first mismatch: (.*)$")
PAD_DONE = "[pad] script done at vblank"  # GT_PAD_LOG=1: the route's pad script ran to its end (host/pad_script.cpp)
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
    nested_equal: int = 0       # ran inside another decompiled function's case that came out equal
    nested_mismatched: int = 0  # ... or mismatched


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
                                                  ("compared", "equal", "mismatched", "not_comparable", "calls")),
                                nested_equal=int(match["nested_equal"] or 0),
                                nested_mismatched=int(match["nested_mismatched"] or 0))
            continue
        first = _FIRST_MISMATCH.match(line)
        if first and last is not None and not stats[last].first_mismatch:
            stats[last] = replace(stats[last], first_mismatch=first[1])
    return stats


def parse_mismatch_cases(text: str) -> set[tuple[int, frozenset[int]]]:
    """Each distinct mismatching case of a run: the function whose case it was, and the decompiled functions
    that ran nested in its decompiled run."""
    return {(int(parent, 16), frozenset() if nested == "-" else frozenset(int(a, 16) for a in nested.split(",")))
            for parent, nested in _MISMATCH_CASE.findall(text)}


def attributed(sabotaged: set[int], cases: Iterable[tuple[int, frozenset[int]]]) -> set[int]:
    """The sabotaged functions a mismatch can be pinned on: those that were the only sabotaged function in
    some mismatching case (as its own function or nested in it). A case holding two sabotaged functions — a
    parent and its decompiled callee — proves neither."""
    found = set()
    for parent, nested in cases:
        culprits = ({parent} | nested) & sabotaged
        if len(culprits) == 1:
            found |= culprits
    return found


def next_round(ambiguous: set[int], cases: Iterable[tuple[int, frozenset[int]]]) -> set[int]:
    """The ambiguous functions to sabotage next, together: lowest address first, skipping any that shared a
    mismatching case with one already taken (they would make each other ambiguous again)."""
    cases = list(cases)
    taken: set[int] = set()
    for address in sorted(ambiguous):
        if not any(address in ({p} | n) and taken & ({p} | n) for p, n in cases):
            taken.add(address)
    return taken


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
                t.not_comparable + s.not_comparable, t.calls + s.calls, t.first_mismatch or s.first_mismatch,
                t.nested_equal + s.nested_equal, t.nested_mismatched + s.nested_mismatched)
    return total


def reached(s: Stats) -> bool:
    """Compared in its own cases, or ran nested in another decompiled function's equal case."""
    return s.compared > 0 or s.nested_equal > 0


def detected(s: Stats) -> bool:
    """A mismatch in its own cases or in a case it ran nested in (a sabotage shows up either way)."""
    return s.mismatched > 0 or s.nested_mismatched > 0


def summarize(total: dict[int, Stats]) -> Summary:
    return Summary(functions=len(total),
                   compared=sum(1 for s in total.values() if reached(s)),
                   mismatched=sum(1 for s in total.values() if detected(s)),
                   never_compared=sum(1 for s in total.values() if not reached(s)))


# A guest word in the kernel region (0x08000000-0x087FFFFF): compared by every case (the whole RAM is), never
# touched by GT's user-mode code under the HLE kernel, so a bump there survives any caller.
SABOTAGE_WORD = 0x08000F00


_DEFINE = re.compile(r"^#define ([A-Z][A-Z0-9_]*)\(([^)]*)\)((?:[^\n]*\\\n)*[^\n]*)", re.MULTILINE)
_INSTANCE = re.compile(r"^[ \t]*([A-Z][A-Z0-9_]*)\(\s*(0x[0-9A-Fa-f]{8})u?\s*(?:,([^;\n]*))?\)\s*;", re.MULTILINE)
_DIRECT = re.compile(r"\bGT_DECOMP\(\s*[^,]+,\s*(?:::)?gt::(?:\w+::)*(\w+)")
_CALL = re.compile(r"\b([A-Z][A-Z0-9_]*)\(([^()]*)\)")


def _split_args(text: str) -> list[str]:
    return [a.strip() for a in text.split(",")] if text.strip() else []


def _macro_function(macro: str, args: Sequence[str], macros: dict, depth: int = 0) -> str | None:
    """The function a template-instance macro registers, following the macros it expands into."""
    if depth > 4 or macro not in macros:
        return None
    params, body = macros[macro]
    env = dict(zip(params, args))
    direct = _DIRECT.search(body)
    if direct:
        return env.get(direct[1], direct[1])
    for call in _CALL.finditer(body):
        if call[1] != macro and call[1] in macros:
            found = _macro_function(call[1], [env.get(a, a) for a in _split_args(call[2])], macros, depth + 1)
            if found:
                return found
    return None


def template_functions(source: str) -> dict[int, str]:
    """Template instances registered in `source` (GT_DECOMP_CONST(0x..u, 3u) and the like): address -> the
    shared body (a function of the file) their macro registers. They have no name of their own, so a
    sabotage has to go into that body — which every instance of it shares."""
    macros = {m[1]: (_split_args(m[2]), m[3].replace("\\\n", " ")) for m in _DEFINE.finditer(source)}
    found = {}
    for m in _INSTANCE.finditer(source):
        if m[1] in macros:
            function = _macro_function(m[1], [m[2], *_split_args(m[3] or "")], macros)
            if function:
                found[int(m[2], 16)] = function
    return found


def sabotage_targets(source: str, targets: set[int]) -> tuple[list[str], set[int], list[int]]:
    """The bodies to sabotage for `targets`, every registration those bumps reach, and the targets with no body
    the tool can find. A body may be shared — a template's instances, or a named registration of the same
    function — and every registration of it is sabotaged, so attribution must count them all as culprits."""
    function_of = {**template_functions(source), **registered_names(source)}
    names = sorted({function_of[a] for a in targets if a in function_of})
    affected = set(targets) | {a for a, function in function_of.items() if function in names}
    unresolved = sorted(a for a in targets if a not in function_of)
    return names, affected, unresolved


def sabotage(source: str, names: Sequence[str]) -> tuple[str, list[str]]:
    """Inserts, as the first statement of each named function, a bump of s0 and of SABOTAGE_WORD; returns the
    names not found. s0 alone isn't enough for a function reached only nested: a caller that saves and restores
    s0 erases the bump (measured 2026-10-13); a RAM word survives it."""
    missing = []
    for name in names:
        pattern = re.compile(r"^(void " + re.escape(name) +
                             r"\(psprecomp::Runtime &)(\w*)(, psprecomp::AllegrexContext &)(\w*)(\) \{)", re.MULTILINE)

        def bump(match: re.Match) -> str:
            rt = match[2] or "sabotage_rt"
            ctx = match[4] or "ctx"  # an unnamed parameter is named for the bump
            return (f"{match[1]}{rt}{match[3]}{ctx}{match[5]} {ctx}.gpr[16] += 1u; "
                    f"{rt}.memory().store32(0x{SABOTAGE_WORD:08X}u, {rt}.memory().load32(0x{SABOTAGE_WORD:08X}u) + 1u);")

        source, count = pattern.subn(bump, source, count=1)
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
        if kind == "full":  # ends a tail after the script is done; the wall clock is only a cap (FULL_TAIL)
            tail = FULL_TAIL.get(route, DEFAULT_TAIL)
            parts.append(f'( GT_PAD_EXIT_AFTER={tail} GT_PAD_LOG=1 GT_PAD="$(grep -v \'^#\' config/pad_routes/{route}.pad)" '
                         f"timeout {2 * seconds + 120} {binary} --max-seconds {2 * seconds} --decomp diff > {log}.log 2>&1; "
                         f"echo $? > {log}.exit ) &")
            continue
        parts.append(f'( GT_PAD_LOG=1 GT_PAD="$(grep -v \'^#\' config/pad_routes/{route}.pad)" timeout 1000 {binary} '
                     f"--max-seconds {seconds} --decomp diff > {log}.log 2>&1; echo $? > {log}.exit ) &")
    return " ".join(parts) + " wait"


def probe_shell(binary: Path, tag: str, symbols: Path, logs: Path, kind: str = "fast", scale: float = 1.0) -> str:
    """One shell line running the fast routes in parallel with every function recompiled (`--decomp off`) and
    the functions named in `symbols` probed (GT_PROBE_SYMBOLS: `name = 0xADDR;` lines), one log per route. The
    logs say which functions the routes call: `python -m tools.family_members --probe` reads them."""
    parts = []
    for route, seconds in ROUTES[kind]:
        log = logs / f"probe_{tag}_{route}.log"
        if kind == "full":  # like the full gate: a tail after the pad script, the wall clock only a cap
            tail = FULL_TAIL.get(route, DEFAULT_TAIL)
            cap = round(2 * seconds * scale)
            parts.append(f'( GT_PAD_EXIT_AFTER={tail} GT_PAD_LOG=1 GT_PROBE_SYMBOLS={symbols} LCS_UNCAPPED=1 '
                         f'GT_PAD="$(grep -v \'^#\' config/pad_routes/{route}.pad)" timeout {cap + 120} {binary} '
                         f"--max-seconds {cap} --decomp off > {log} 2>&1; echo $? > {log.with_suffix('.exit')} ) &")
            continue
        parts.append(f'( GT_PAD_LOG=1 GT_PROBE_SYMBOLS={symbols} LCS_UNCAPPED=1 GT_PAD="$(grep -v \'^#\' config/pad_routes/{route}.pad)" '
                     f"timeout 1000 {binary} --max-seconds {round(seconds * scale)} --decomp off > {log} 2>&1; echo $? > {log.with_suffix('.exit')} ) &")
    return " ".join(parts) + " wait"


PAD_UNFINISHED = ("the pad script did not finish within --max-seconds (the route stopped before its last press: it covers less "
                  "than it should; lengthen ROUTES or shorten the route)")


def probe_problem(text: str, exit_code: str, require_done: bool = False) -> str | None:
    """Why a probe route's log can't be trusted (like `route_problem` for the gate), or None: a timeout, a crash or a
    run that left no exit code means the hit counts are partial, and a family built on them would miss members."""
    if exit_code == "124":
        return "timed out (killed by timeout; the hit counts are partial)"
    if exit_code == "?":
        return "no exit code (the run did not finish)"
    if exit_code != "0":
        return f"exit {exit_code}"
    if require_done and PAD_DONE not in text:
        return PAD_UNFINISHED
    return None


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


def route_problem(text: str, exit_code: str, require_done: bool = False) -> str | None:
    """Why a route's run can't be trusted, or None. Exit 2 is gt_native's "mismatches found", which the
    per-function lines already report; a timeout, a crash or a missing report fails on its own."""
    if exit_code == "124":
        return "timed out (killed by timeout; no trustworthy report)"
    if not _LINE.search(text):
        return "no report (the run ended before printing its results)"
    if exit_code not in ("0", "2"):
        return f"exit {exit_code}"
    if require_done and PAD_DONE not in text:
        return PAD_UNFINISHED
    return None


FLOOR_FILE = Path("config/decomp_gate_floor.txt")


def compared_floor_problem(compared: int, floor: int) -> str | None:
    """Why a full gate's compared total is too low, or None. A function that installs fine and passes its own cases
    can still stop other code from running (FINDINGS: func_08A9DD14 cut ~300 compared functions with 0 mismatches),
    and the only sign is fewer functions compared. Run-to-run noise on the real-time routes is ~40; the tolerance is
    the larger of 50 and 1 % of the floor."""
    tolerance = max(50, floor // 100)
    if compared >= floor - tolerance:
        return None
    return (f"{compared} compared, below the floor {floor} (tolerance {tolerance}): an installed function may stop "
            f"other code from running (FINDINGS) - bisect with GT_DECOMP_ONLY, never blame load without an idle re-run")


def read_floor(path: Path) -> int | None:
    """The floor recorded in `path` ("compared N" line), or None when there is none."""
    if not path.exists():
        return None
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] == "compared" and parts[1].isdigit():
            return int(parts[1])
    return None


def raise_floor(path: Path, compared: int) -> int:
    """Raises the floor in `path` to `compared` when that is higher (a ratchet: it never goes down); returns it."""
    current = read_floor(path)
    floor = compared if current is None else max(current, compared)
    path.write_text("# The full gate's compared total (tools.decomp_gate full fails below it minus a tolerance).\n"
                    "# Raised with `tools.decomp_gate full TAG --raise-floor` after a passing full gate; never lowered.\n"
                    f"compared {floor}\n")
    return floor


def coverage_left_configured(cmake_cache: str) -> bool:
    """Whether build/CMakeCache.txt still instruments sources for coverage (a coverage run killed before it could
    put the build back): a normal gate would then build them instrumented."""
    match = re.search(r"^GT_COVERAGE_FILES:[A-Z]+=(.*)$", cmake_cache, re.MULTILINE)
    return bool(match and match[1].strip())


def _run_gate(kind: str, tag: str, only: list[int] | None,  # pragma: no cover - drives the build
              extra_env: dict[str, str] | None = None, raise_floor_after: bool = False) -> int:
    env = None
    if only is not None or extra_env:
        import os
        env = dict(os.environ, **(extra_env or {}))
        if only is not None:
            env.update(GT_DECOMP_ONLY=",".join(f"0x{a:08X}" for a in only), LCS_UNCAPPED="1")
    clear_run(tag, Path("recomp/analysis"))
    cache = Path("build/CMakeCache.txt")
    if not (extra_env or {}).get("LLVM_PROFILE_FILE") and cache.exists() and coverage_left_configured(cache.read_text()):
        print("warning: build/ still instruments sources for coverage (a coverage run was killed?); reconfigure with "
              "`cmake -S . -B build -DGT_COVERAGE_FILES=` before trusting timings")
    if subprocess.run([sys.executable, "-m", "tools.contract_audit"], stdout=subprocess.DEVNULL).returncode == 1:
        print("contract audit: a GT_DECOMP_NESTED function has a caller reading a register its ReturnKind leaves "
              "uncompared (.venv/bin/python -m tools.contract_audit)")
        return 2
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
        return _gate_steps(kind, tag, env, raise_floor_after)
    finally:
        slot.release()


def _gate_steps(kind: str, tag: str, env: dict[str, str] | None,  # pragma: no cover - drives the build
                raise_floor_after: bool = False) -> int:
    build = subprocess.run(heavy(["cmake", "--build", "build", f"-j{BUILD_JOBS}"]), capture_output=True, text=True)
    if build.returncode != 0:
        print("\n".join(line for line in build.stdout.splitlines() if "error" in line)[:2000] or "build failed")
        return 2
    subprocess.run(heavy(["sh", "-c", route_shell(Path("build/gt_native"), kind, tag, Path("recomp/analysis"))]),
                   env=env)
    return _report(tag, check_floor=kind == "full" and env is None, raise_floor_after=raise_floor_after)


def _probe_run(tag: str, symbols: Path, kind: str = "fast", scale: float = 1.0) -> int:  # pragma: no cover - runs the game
    subprocess.run(slice_limits(), check=True)
    slot = HeavySlot()
    slot.acquire()
    for stale in Path("recomp/analysis").glob(f"probe_{tag}_*"):
        stale.unlink()  # an earlier run's logs and exit codes must not stand in for this one
    try:
        subprocess.run(heavy(["sh", "-c", probe_shell(Path("build/gt_native"), tag, symbols.resolve(), Path("recomp/analysis"), kind, scale)]))
    finally:
        slot.release()
    logs = sorted(Path("recomp/analysis").glob(f"probe_{tag}_*.log"))
    print(f"{len(logs)} probe logs: " + " ".join(str(p) for p in logs))
    status = 0 if len(logs) == len(ROUTES[kind]) else 2
    for log in logs:
        exit_file = log.with_suffix(".exit")
        problem = probe_problem(log.read_text(errors="replace"), exit_file.read_text().strip() if exit_file.exists() else "?",
                               require_done=kind == "full")
        if problem:
            print(f"  {log.name}: {problem}")
            status = 1
    return status


def _report(tag: str, check_floor: bool = False, raise_floor_after: bool = False) -> int:  # pragma: no cover - reads the run's files
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
        problem = route_problem(text, code, require_done=check_floor)
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
    floor = read_floor(FLOOR_FILE) if check_floor else None  # the full gate only: a fast gate compares a subset
    if floor is not None and (problem := compared_floor_problem(s.compared, floor)):
        print(f"  FLOOR: {problem}")
        status = 1
    if check_floor and raise_floor_after and status == 0:
        print(f"floor: compared {raise_floor(FLOOR_FILE, s.compared)} ({FLOOR_FILE})")
    return status


def _sabotage_round(tag: str, file: Path, only: list[int], targets: set[int],  # pragma: no cover - runs
                    kind: str = "fast"):
    """Sabotages `targets` (with every function of `only` installed), runs the fast gate, restores the file.
    Returns the run's mismatching cases and every registration the bumps reached (template instances share a
    body), or None when the sabotaged build didn't run."""
    original = file.read_text()
    names, affected, unresolved = sabotage_targets(original, targets)
    if unresolved:
        print("no body to sabotage in", file, "for:", ", ".join(f"0x{a:08X}" for a in unresolved),
              "(bump it by hand: a C++ template function, or a macro defined in a header)")
    sabotaged, missing = sabotage(original, names)
    if missing:
        print("not found in", file, ":", ", ".join(missing))
    try:
        file.write_text(sabotaged)
        ran = _run_gate(kind, tag, only)
    finally:
        file.write_text(original)
    if ran == 2:  # recomp or build failed: nothing ran, so nothing was detected
        return None
    logs = [Path(f"recomp/analysis/gt_diff_{tag}_{r}.log") for r, _ in ROUTES["full"]]
    return set().union(*(parse_mismatch_cases(log.read_text(errors="replace")) for log in logs if log.exists())), affected


def _sabotage_run(tag: str, file: Path, only: list[int],  # pragma: no cover - edits, runs, restores
                  also: Sequence[int] = (), kind: str = "fast") -> int:
    """Sabotages `only`; `also` is installed too (their callers or callees) without being sabotaged. `kind` "full"
    runs the sabotaged build on the full gate, for functions only the full routes reach."""
    installed = sorted(set(only) | set(also))
    targets = set(only)
    seen: set[tuple[int, frozenset[int]]] = set()
    found: set[int] = set()
    for round_number in range(1, MAX_SABOTAGE_ROUNDS + 1):
        result = _sabotage_round(f"{tag}s{round_number}", file, installed, targets, kind)
        if result is None:
            print("sabotage: the sabotaged build did not run")
            return 2
        cases, affected = result
        # only this round's cases (they hold only this round's sabotage); every sabotaged registration counts as
        # a possible culprit, a target's template siblings included
        new = attributed(affected, cases) & targets
        found |= new
        seen |= cases
        print(f"sabotage round {round_number}: {len(targets)} sabotaged, {len(new)} pinned")
        ambiguous = {a for a in set(only) - found if any(a == p or a in n for p, n in seen)}
        targets = next_round(ambiguous, seen)
        if not targets:
            break
    undetected = [f"0x{a:08X}" for a in only if a not in found]
    print(f"sabotage: {len(only) - len(undetected)}/{len(only)} detected; undetected: {undetected}")
    return 0 if not undetected else 1


def _coverage_run(tag: str, only: list[int]) -> int:  # pragma: no cover - reconfigures, builds, runs
    """The fast gate over `only` with the sources that register it built with LLVM coverage; prints, per function,
    the branches no compared case took and the code regions that never ran. The build is put back afterwards."""
    from tools import gate_coverage
    files = gate_coverage.source_files(only, Path("src"), _REGISTRATION)
    if not files:
        print("coverage: none of those addresses is registered in src/")
        return 2
    directory = Path("build/coverage").resolve()
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    configure = ["cmake", "-S", ".", "-B", "build"]
    subprocess.run([*configure, gate_coverage.cmake_files_argument(files, Path.cwd())], check=True, stdout=subprocess.DEVNULL)
    try:
        status = _run_gate("fast", tag, only, {"LLVM_PROFILE_FILE": str(directory / "%p.profraw")})
        raw = sorted(directory.glob("*.profraw"))
        if not raw:
            print("coverage: the run wrote no profile (did it end before the executable exited normally?)")
            return 2
        if len(raw) < len(ROUTES["fast"]):
            print(f"warning: only {len(raw)} of {len(ROUTES['fast'])} routes wrote a profile (a route killed by its "
                  "timeout writes none): the report covers fewer runs than the gate")
        merged = directory / "merged.profdata"
        subprocess.run(["llvm-profdata", "merge", "-sparse", *map(str, raw), "-o", str(merged)], check=True)
        exported = subprocess.run(["llvm-cov", "export", "build/gt_native", f"-instr-profile={merged}", *map(str, files)],
                                  check=True, capture_output=True, text=True).stdout
        document = json.loads(exported)
        gate_coverage.demangle_names(document, lambda names: subprocess.run(
            ["llvm-cxxfilt"], input="\n".join(names), capture_output=True, text=True, check=True).stdout.splitlines())
        functions = gate_coverage.uncovered(document, map(str, files))
        wanted = set(only)
        print(gate_coverage.format_report([f for f in functions if f.address is None or f.address in wanted]))
        return status
    finally:
        subprocess.run([*configure, "-DGT_COVERAGE_FILES="], stdout=subprocess.DEVNULL)


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - CLI wiring
    parser = argparse.ArgumentParser(prog="tools.decomp_gate", description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    fast = sub.add_parser("fast")
    fast.add_argument("tag")
    fast.add_argument("--only", required=True)
    full = sub.add_parser("full")
    full.add_argument("tag")
    full.add_argument("--raise-floor", action="store_true",
                      help=f"after a passing gate, raise the compared floor in {FLOOR_FILE} to this run's total")
    sab = sub.add_parser("sabotage")
    sab.add_argument("tag")
    sab.add_argument("--file", type=Path, required=True)
    sab.add_argument("--only", required=True)
    sab.add_argument("--also", default="", help="installed but not sabotaged (e.g. the decompiled callers of "
                                                "functions reached only nested)")
    sab.add_argument("--full", action="store_true", help="run the sabotaged build on the full gate (functions only the "
                                                         "full routes reach)")
    cov = sub.add_parser("coverage", help="fast gate with LLVM branch coverage of the sources that register --only")
    cov.add_argument("tag")
    cov.add_argument("--only", required=True)
    probe = sub.add_parser("probe", help="run the fast routes with the decomp off and --symbols probed (call counts)")
    probe.add_argument("tag")
    probe.add_argument("--symbols", type=Path, required=True)
    probe.add_argument("--full", action="store_true", help="the full gate's route lengths, and a route whose pad script "
                                                           "did not finish is a failure (the logs say everything a route reaches)")
    probe.add_argument("--scale", type=float, default=1.0, help="multiply every route's --max-seconds (a loaded machine "
                                                                "runs fewer vblanks per second; a route that does not finish covers less)")
    rep = sub.add_parser("report")
    rep.add_argument("tag")
    args = parser.parse_args(argv)
    if args.command == "fast":
        return _run_gate("fast", args.tag, parse_only(args.only))
    if args.command == "full":
        return _run_gate("full", args.tag, None, raise_floor_after=args.raise_floor)
    if args.command == "probe":
        return _probe_run(args.tag, args.symbols, "full" if args.full else "fast", args.scale)
    if args.command == "coverage":
        return _coverage_run(args.tag, parse_only(args.only))
    if args.command == "sabotage":
        return _sabotage_run(args.tag, args.file, parse_only(args.only), parse_only(args.also) if args.also else (),
                             kind="full" if args.full else "fast")
    return _report(args.tag, check_floor=True)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
