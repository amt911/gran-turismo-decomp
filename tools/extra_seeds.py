"""Seeds for the functions the recompiler never emits (docs/superpowers/specs/2026-11-19-emit-unreferenced-functions-design.md).

`psp_recomp --auto` emits what its CFG discovery reaches; a function nothing points at (dead code, a vtable slot without a
relocation, a target of `j` only) is not emitted, so no decompiled version of it can be registered or gated. This tool picks the
functions to add as extra seeds and checks a regeneration against the one before it.

  .venv/bin/python -m tools.extra_seeds select --phase A [--out recomp/extra_seeds.txt]
  .venv/bin/python -m tools.extra_seeds verify --before recomp/generated_before --after recomp/generated

Phase A: functions with reference evidence (a `jal`/`j` anywhere in `.text`, a `lui`+`addiu` address, a raw data word). Phase B: the
rest, which need a stack-frame prologue and no branch into their middle. Every candidate is refused when the function before it in
`functions.tsv` falls into it (a wrong Ghidra boundary or a shared tail): seeding it would add a second entry into code that is not
a function.
"""

from __future__ import annotations

import argparse
import bisect
import re
import struct
import sys
from collections import defaultdict
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path

LOAD_BASE = 0x08804000
REASON_NO_EVIDENCE = "no-evidence"
REASON_SHARED_TAIL = "shared-tail"
REASON_ENTERED_BY_BRANCH = "entered-by-branch"

_NOP = 0
_JR_RA = 0x03E00008
_REGISTERED = re.compile(r"runtime\.register_function\(0x([0-9A-Fa-f]{8})u")
_LABEL = re.compile(r"\bL_([0-9A-Fa-f]{8}):")


@dataclass
class Evidence:
    callers: frozenset[int] = frozenset()
    lui_sites: frozenset[int] = frozenset()
    data_words: int = 0

    @property
    def referenced(self) -> bool:
        return bool(self.callers or self.lui_sites or self.data_words)


@dataclass
class _Builder:
    callers: set[int] = field(default_factory=set)
    lui: set[int] = field(default_factory=set)
    data: int = 0


_MEMORY_OPS = {0x09, 0x23, 0x2B, 0x20, 0x24, 0x21, 0x25, 0x28, 0x29, 0x31, 0x39}  # addiu and the loads/stores


def references(words: Sequence[int], base: int, elf_words: Iterable[int], functions: Mapping[int, int]) -> dict[int, Evidence]:
    """Reference evidence per function start (ELF vaddr): `j`/`jal` sites, `lui` sites of an address built with a following
    addiu/load/store within ten instructions, and the number of raw words of the file equal to the address."""
    built: dict[int, _Builder] = defaultdict(_Builder)
    wanted = set(functions)
    for i, w in enumerate(words):
        op = w >> 26
        site = base + 4 * i
        if op in (2, 3):
            target = (w & 0x3FFFFFF) << 2
            if target in wanted:
                built[target].callers.add(site)
        elif op == 0x0F:
            reg, high = (w >> 16) & 31, (w & 0xFFFF) << 16
            for j in range(i + 1, min(i + 10, len(words))):
                v = words[j]
                if v >> 26 in _MEMORY_OPS and (v >> 21) & 31 == reg:
                    low = v & 0xFFFF
                    low -= 0x10000 if low & 0x8000 else 0
                    address = (high + low) & 0xFFFFFFFF
                    if address in wanted:
                        built[address].lui.add(site)
    for w in elf_words:
        if w in wanted:
            built[w].data += 1
    return {a: Evidence(frozenset(b.callers), frozenset(b.lui), b.data) for a, b in built.items()}


def falls_into(words: Sequence[int]) -> bool:
    """True when a function's code runs on into the next address: its last instruction (trailing nops aside) is not the delay slot
    of a `jr`, a `j` or an unconditional branch."""
    body = list(words)
    while body and body[-1] == _NOP:
        body.pop()
    # the branch is the last or the one before the last non-nop word (its delay slot may itself be a nop that was stripped)
    return not any(_unconditional(w) for w in body[-2:])


def _unconditional(word: int) -> bool:
    op = word >> 26
    return (word == _JR_RA or (word & 0xFC1FFFFF) == 0x00000008 or op == 2
            or (op == 4 and (word >> 21) & 31 == (word >> 16) & 31))


def _has_prologue(words: Sequence[int]) -> bool:
    return any(w >> 16 == 0x27BD and w & 0x8000 for w in words[:3])


def _branch_targets(words: Sequence[int], base: int) -> set[int]:
    targets = set()
    for i, w in enumerate(words):
        op = w >> 26
        if op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (op == 1 and (w >> 16) & 31 in (0, 1, 2, 3, 0x10, 0x11)):
            offset = w & 0xFFFF
            offset -= 0x10000 if offset & 0x8000 else 0
            targets.add(base + 4 * i + 4 + 4 * offset)
    return targets


def select(words: Sequence[int], base: int, functions: Mapping[int, int], emitted: set[int], elf_words: Iterable[int],
           phase: str) -> tuple[list[int], dict[int, str]]:
    """(seeds, {rejected address: reason}) for `phase` "A" (reference evidence) or "B" (none). Addresses are ELF vaddrs."""
    if phase not in ("A", "B"):
        raise ValueError(f"unknown phase {phase!r}")
    evidence = references(words, base, elf_words, functions)
    starts = sorted(functions)
    branch_targets = _branch_targets(words, base)

    def body(address: int) -> Sequence[int]:
        start = (address - base) // 4
        return words[start:start + functions[address] // 4]

    seeds: list[int] = []
    rejected: dict[int, str] = {}
    for address in starts:
        if address in emitted:
            continue
        referenced = address in evidence and evidence[address].referenced
        if referenced != (phase == "A"):
            continue
        index = bisect.bisect_left(starts, address)
        if index > 0:
            previous = starts[index - 1]
            if previous + functions[previous] == address and falls_into(body(previous)):
                rejected[address] = REASON_SHARED_TAIL
                continue
        if phase == "B":
            if not _has_prologue(body(address)):
                rejected[address] = REASON_NO_EVIDENCE
                continue
            if any(address < t < address + functions[address] for t in branch_targets):
                rejected[address] = REASON_ENTERED_BY_BRANCH
                continue
        seeds.append(address)
    return seeds, rejected


def effective_sizes(words: Sequence[int], base: int, functions: Mapping[int, int]) -> dict[int, int]:
    """The size of each function as its code shows it: up to the first unconditional return/jump (and its delay slot) that no earlier
    branch jumps over. Ghidra's sizes can overlap (a two-instruction tail-jump thunk listed with the size of the function after it);
    a seed inside such an overlap is not a split of the thunk."""
    sizes = {}
    for address, size in functions.items():
        start = (address - base) // 4
        body = words[start:start + size // 4]
        farthest = 0  # index of the farthest branch target seen so far
        end = len(body)
        for i, w in enumerate(body):
            op = w >> 26
            if op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (op == 1 and (w >> 16) & 31 in (0, 1, 2, 3, 0x10, 0x11)):
                offset = w & 0xFFFF
                offset -= 0x10000 if offset & 0x8000 else 0
                farthest = max(farthest, i + 1 + offset)
            if _unconditional(w) and farthest <= i + 1:
                end = min(len(body), i + 2)
                break
        sizes[address] = 4 * end
    return sizes


def write_seeds(path: Path, seeds: Iterable[int], load_base: int = LOAD_BASE) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("# extra seeds (tools.extra_seeds): runtime addresses, derived from the user's dump, never committed\n"
                    + "".join(f"{load_base + a:#010x}\n" for a in sorted(set(seeds))))


def _generated_sets(directory: Path) -> tuple[set[int], set[int]]:
    registered: set[int] = set()
    labels: set[int] = set()
    for unit in sorted(directory.glob("generated_unit_*.cpp")):
        text = unit.read_text()
        registered |= {int(m, 16) for m in _REGISTERED.findall(text)}
        labels |= {int(m, 16) for m in _LABEL.findall(text)}
    return registered, labels


def verify(before: Path, after: Path, tsv_before: Path, tsv_after: Path, seeds: Iterable[int],
           functions: Mapping[int, int]) -> list[str]:
    """Problems (empty = pass) of a regeneration with seeds against the one without: every registered function entry and every block
    label of `before` is still there, the function list is byte-identical, and no seed lies inside another function. `seeds` and
    `functions` use the address space of the generated units."""
    errors: list[str] = []
    registered_before, labels_before = _generated_sets(before)
    registered_after, labels_after = _generated_sets(after)
    for a in sorted(registered_before - registered_after):
        errors.append(f"function entry {a:#010x} disappeared")
    for a in sorted(labels_before - labels_after):
        errors.append(f"block {a:#010x} disappeared")
    if tsv_before.read_bytes() != tsv_after.read_bytes():
        errors.append("functions.tsv changed")
    for seed in sorted(set(seeds)):
        for start, size in functions.items():
            if start < seed < start + size:
                errors.append(f"seed {seed:#010x} splits the function at {start:#010x}")
    return errors


def _main_select(args: argparse.Namespace) -> int:  # pragma: no cover - reads the user's dump
    from tools.family_members import TextImage
    from tools.progress import load_functions
    from tools.recomp import SEEDS_SOURCE

    elf = args.elf.read_bytes()
    image = TextImage.from_elf(elf)
    words = struct.unpack(f"<{len(image.text) // 4}I", image.text[:len(image.text) // 4 * 4])
    functions = {a: s for a, s in load_functions(args.functions).items() if image.base <= a < image.base + len(image.text)}
    elf_words = struct.unpack(f"<{len(elf) // 4}I", elf[:len(elf) // 4 * 4])
    registered, _ = _generated_sets(args.generated)
    emitted = {a - LOAD_BASE for a in registered}
    seeds, rejected = select(words, image.base, functions, emitted, elf_words, args.phase)
    write_seeds(args.out or SEEDS_SOURCE, seeds)
    reasons: dict[str, int] = defaultdict(int)
    for reason in rejected.values():
        reasons[reason] += 1
    print(f"phase {args.phase}: {len(seeds)} seeds written to {args.out or SEEDS_SOURCE}; rejected {dict(reasons)}")
    for address, reason in sorted(rejected.items()):
        print(f"  rejected {LOAD_BASE + address:#010x} {reason}")
    return 0


def _main_verify(args: argparse.Namespace) -> int:  # pragma: no cover - reads generated output
    from tools.progress import load_functions

    seeds = [int(line.split("#")[0].strip(), 16) for line in args.seeds.read_text().splitlines() if line.split("#")[0].strip()]
    from tools.family_members import TextImage

    elf = args.elf.read_bytes()
    image = TextImage.from_elf(elf)
    words = struct.unpack(f"<{len(image.text) // 4}I", image.text[:len(image.text) // 4 * 4])
    inside = {a: s for a, s in load_functions(args.functions).items() if image.base <= a < image.base + len(image.text)}
    functions = {LOAD_BASE + a: s for a, s in effective_sizes(words, image.base, inside).items()}
    tsv_before = args.tsv_before or args.functions
    errors = verify(args.before, args.after, tsv_before, args.functions, seeds, functions)
    registered_before, _ = _generated_sets(args.before)
    registered_after, _ = _generated_sets(args.after)
    print(f"function entries: {len(registered_before)} before, {len(registered_after)} after (+{len(registered_after - registered_before)})")
    for e in errors:
        print(f"FAIL: {e}")
    print("verify: " + ("ok" if not errors else f"{len(errors)} problems"))
    return 1 if errors else 0


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - command line
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    s = sub.add_parser("select")
    s.add_argument("--phase", choices=("A", "B"), required=True)
    s.add_argument("--out", type=Path)
    s.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    s.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    s.add_argument("--generated", type=Path, default=Path("recomp/generated"))
    v = sub.add_parser("verify")
    v.add_argument("--before", type=Path, required=True)
    v.add_argument("--after", type=Path, default=Path("recomp/generated"))
    v.add_argument("--seeds", type=Path, default=Path("recomp/extra_seeds.txt"))
    v.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    v.add_argument("--tsv-before", type=Path)
    v.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    args = parser.parse_args(argv)
    return _main_select(args) if args.command == "select" else _main_verify(args)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
