"""The bookkeeping of a batch verified by synthetic cases (docs/agents/decomp-workflow.md).

A batch's new registrations are compared with `tools.decomp_gate synth --only`; the ones with no comparable case keep
their rows with the registration commented, then `sabotage --synth` runs file by file and the undetected ones are
commented the same way. Only the batch's own addresses are ever edited (a cleanup that edited "from the batch marker
on" once deleted rows it did not own).

Usage (from the repository root):
  .venv/bin/python -m tools.synth_batch TAG --addresses FILE [--cases 64]
FILE lists the batch's registrations, one 0xADDRESS per line. Prints what was compared, commented and pinned.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections.abc import Iterable, Sequence
from pathlib import Path

_REGISTRATION = re.compile(r"^(?P<line>[A-Z_][A-Z0-9_]*\((?P<address>0x[0-9A-Fa-f]{8})u.*)$")
_REPORT = re.compile(r"@(0x[0-9A-F]{8}): compared (\d+),")
_UNDETECTED = re.compile(r"^sabotage: \d+/\d+ detected; undetected: \[(.*)\]\s*$", re.M)


def comment_registrations(source: str, addresses: Iterable[str], reason: str) -> tuple[str, int]:
    """`source` with the active registration lines of `addresses` commented (`// LINE  // reason`), and how many."""
    wanted = {a.upper().replace("0X", "0x") for a in addresses}
    out, count = [], 0
    for line in source.split("\n"):
        match = _REGISTRATION.match(line)
        if match and match.group("address").upper().replace("0X", "0x") in wanted:
            out.append(f"// {line}  // {reason}")
            count += 1
        else:
            out.append(line)
    return "\n".join(out), count


def never_compared(report: str) -> set[str]:
    """The functions a gate report lists with no compared case."""
    return {address for address, compared in _REPORT.findall(report) if compared == "0"}


_MISMATCHED = re.compile(r"@(0x[0-9A-F]{8}): compared \d+, equal \d+, mismatched (\d+),")


def mismatched(report: str) -> set[str]:
    """The functions a gate report lists with a mismatching case: never verified, whatever their sabotage does."""
    return {address for address, count in _MISMATCHED.findall(report) if count != "0"}


def report_ok(report: str) -> bool:
    """The synthetic run finished and printed its per-function report (a failed build or a refused start prints neither)."""
    return "Synthetic cases done" in report and _REPORT.search(report) is not None


def sabotage_ran(sabotage_output: str) -> bool:
    """The sabotage printed its summary (it did not stop on a build that failed)."""
    return _UNDETECTED.search(sabotage_output) is not None


def undetected(sabotage_output: str) -> set[str]:
    """The functions a sabotage run left undetected."""
    found: set[str] = set()
    for match in _UNDETECTED.finditer(sabotage_output):
        found |= set(re.findall(r"0x[0-9A-Fa-f]{8}", match.group(1)))
    return found


def _files_of(addresses: set[str]) -> dict[Path, list[str]]:  # pragma: no cover - reads the tree
    files: dict[Path, list[str]] = {}
    for path in sorted(Path("src").rglob("*.cpp")):
        for line in path.read_text().split("\n"):
            match = _REGISTRATION.match(line)
            if match and match.group("address") in addresses:
                files.setdefault(path, []).append(match.group("address"))
    return files


def _comment(addresses: set[str], reason: str) -> int:  # pragma: no cover - edits the tree
    total = 0
    for path in _files_of(addresses):
        text, count = comment_registrations(path.read_text(), addresses, reason)
        path.write_text(text)
        total += count
    return total


def synth_log(tag: str, routes) -> Path:
    """The report of the synthetic gate: the log of its one route (dealer, or GT_SYNTH_ROUTE's)."""
    return Path(f"recomp/analysis/gt_diff_{tag}_{routes[0][0]}.log")


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - runs the gates
    parser = argparse.ArgumentParser(prog="tools.synth_batch", description=__doc__.splitlines()[0])
    parser.add_argument("tag")
    parser.add_argument("--addresses", type=Path, required=True)
    parser.add_argument("--cases", type=int, default=64)
    args = parser.parse_args(argv)
    batch = {line.strip() for line in args.addresses.read_text().split() if line.strip()}
    # the gate takes the CPU lock around its routes only (GT_GATE_CPU_LOCK; its recomp and build never hold it)
    gate = [sys.executable, "-m", "tools.decomp_gate"]
    subprocess.run(gate + ["synth", args.tag, "--cases", str(args.cases), "--only", ",".join(sorted(batch))],
                   env=dict(os.environ, GT_GATE_CPU_LOCK=f"synth batch {args.tag}"))
    from tools.decomp_gate import ROUTES
    log = synth_log(args.tag, ROUTES["synth"])
    report = log.read_text(errors="replace") if log.exists() else ""
    if not report_ok(report):
        print(f"the synthetic run did not finish ({log}): nothing edited")
        return 2
    never = never_compared(report) & batch
    print(f"synthetic: {len(batch) - len(never)} of {len(batch)} compared; "
          f"{_comment(never, 'no comparable synthetic case yet')} registrations commented")
    differ = mismatched(report) & batch
    if differ:
        print(f"MISMATCHED (commented, to fix): {' '.join(sorted(differ))}")
        _comment(differ, "a synthetic case mismatched: to fix")
    kept = batch - never - differ
    missed: set[str] = set()
    for path, addresses in _files_of(kept).items():
        run = subprocess.run(gate + ["sabotage", f"{args.tag}x", "--synth", "--cases", str(args.cases), "--file", str(path),
                                     "--only", ",".join(sorted(addresses))], capture_output=True, text=True)
        if not sabotage_ran(run.stdout):
            print(f"{path}: the sabotage did not run: stopping, nothing more edited")
            return 2
        missed |= undetected(run.stdout)
        print(f"{path}: {''.join(l for l in run.stdout.splitlines(keepends=True) if l.startswith('sabotage:')).strip()}")
    _comment(missed, "compared equal, but the synthetic sabotage went undetected: not verified")
    print(f"verified: {len(kept) - len(missed)} of {len(batch)} ({len(differ)} mismatched)")
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
