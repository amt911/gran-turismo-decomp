"""Seeds for the functions the recompiler never emits (docs/superpowers/specs/2026-11-19-emit-unreferenced-functions-design.md).

`psp_recomp --auto` emits what its CFG discovery reaches; a function nothing points at (dead code, a vtable slot without a
relocation, a target of `j` only) is not emitted, so no decompiled version of it can be registered or gated. This tool picks the
functions to add as extra seeds (the list is committed: `config/extra_seeds.txt`, ELF vaddrs, no game bytes) and checks a
regeneration against the one before it.

  .venv/bin/python -m tools.extra_seeds select --phase A --baseline recomp/generated_before
  .venv/bin/python -m tools.extra_seeds verify --before recomp/generated_before --tsv-before recomp/analysis/functions.tsv
  .venv/bin/python -m tools.extra_seeds noop --seed 0x08805000

Phase A: functions with reference evidence (a `jal`/`j` anywhere in `.text`, a `lui`+`addiu` address, a raw data word). Phase B: the
rest, which need a stack-frame prologue and no branch into their middle. Every candidate is refused when the function before it in
`functions.tsv` falls into it (a wrong Ghidra boundary or a shared tail), when it lies strictly inside an emitted function, or when a
conditional branch of an emitted function targets it: seeding it would add a second entry into code that is not a function.
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
REASON_INSIDE_EMITTED = "inside-emitted-function"
REASON_BRANCH_TARGET = "branch-target-of-emitted-function"

_NOP = 0
_JR_RA = 0x03E00008
_REGISTERED = re.compile(r"runtime\.register_function\(0x([0-9A-Fa-f]{8})u")
_LABEL = re.compile(r"^\s*L_([0-9A-Fa-f]{8}):")
_CHAINED_ID = re.compile(r"(invoke_chained_direct<&\w+, \d+u, )\d+u(, 0x)")
_TABLE_LINE = re.compile(r"^\s*(case \d+u: goto L_[0-9A-Fa-f]{8};|[\d, ]+,?)\s*$")
_GOTO = re.compile(r"^\s*goto L_([0-9A-Fa-f]{8});\s*$")


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
_LOADS_TO_RT = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}


def _writes(word: int, reg: int) -> bool:
    """True when the instruction writes general register `reg` (R-type rd, I-type rt for ALU immediates and loads, jal's ra)."""
    op = word >> 26
    if op == 0:
        funct = word & 0x3F
        if funct in (0x08, 0x18, 0x19, 0x1A, 0x1B, 0x11, 0x13, 0x0D):  # jr, mult/div, mthi, mtlo, break: no rd
            return False
        return (word >> 11) & 31 == reg
    if op == 3:
        return reg == 31
    return op in _LOADS_TO_RT and (word >> 16) & 31 == reg


def references(words: Sequence[int], base: int, elf_words: Iterable[int], functions: Mapping[int, int]) -> dict[int, Evidence]:
    """Reference evidence per function start (ELF vaddr): `j`/`jal` sites, `lui` sites of an address built with a following
    addiu/load/store within ten instructions (the scan stops where the register is written), and the number of raw words of the
    file equal to the address. A reference from inside the function itself (recursion, a loop's own `j`) is not evidence."""
    built: dict[int, _Builder] = defaultdict(_Builder)
    wanted = set(functions)

    def inside(address: int, site: int) -> bool:
        return address <= site < address + functions[address]

    for i, w in enumerate(words):
        op = w >> 26
        site = base + 4 * i
        if op in (2, 3):
            target = (w & 0x3FFFFFF) << 2
            if target in wanted and not inside(target, site):
                built[target].callers.add(site)
        elif op == 0x0F:
            reg, high = (w >> 16) & 31, (w & 0xFFFF) << 16
            for j in range(i + 1, min(i + 10, len(words))):
                v = words[j]
                if v >> 26 in _MEMORY_OPS and (v >> 21) & 31 == reg:
                    low = v & 0xFFFF
                    low -= 0x10000 if low & 0x8000 else 0
                    address = (high + low) & 0xFFFFFFFF
                    if address in wanted and not inside(address, site):
                        built[address].lui.add(site)
                if _writes(v, reg):
                    break
    for w in elf_words:
        if w in wanted:
            built[w].data += 1
    return {a: Evidence(frozenset(b.callers), frozenset(b.lui), b.data) for a, b in built.items()}


def _unconditional(word: int) -> bool:
    op = word >> 26
    return (word == _JR_RA or (word & 0xFC1FFFFF) == 0x00000008 or op == 2
            or (op == 4 and (word >> 21) & 31 == (word >> 16) & 31))


def falls_into(words: Sequence[int]) -> bool:
    """True when a function's code runs on into the next address: its last instruction (trailing nops aside) is not the delay slot
    of a `jr`, a `j` or an unconditional branch."""
    body = list(words)
    while body and body[-1] == _NOP:
        body.pop()
    # the branch is the last or the one before the last non-nop word (its delay slot may itself be a nop that was stripped)
    return not any(_unconditional(w) for w in body[-2:])


def _has_prologue(words: Sequence[int]) -> bool:
    return any(w >> 16 == 0x27BD and w & 0x8000 for w in words[:3])


def _is_conditional_branch(w: int) -> bool:
    op = w >> 26
    return op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (op == 1 and (w >> 16) & 31 in (0, 1, 2, 3, 0x10, 0x11))


def _branch_target(w: int, site: int) -> int:
    offset = w & 0xFFFF
    offset -= 0x10000 if offset & 0x8000 else 0
    return site + 4 + 4 * offset


def _branch_targets(words: Sequence[int], base: int) -> set[int]:
    return {_branch_target(w, base + 4 * i) for i, w in enumerate(words) if _is_conditional_branch(w)}


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
            if _is_conditional_branch(w):
                farthest = max(farthest, i + 1 + (_branch_target(w, 0) // 4 - 1))
            if _unconditional(w) and farthest <= i + 1:
                end = min(len(body), i + 2)
                break
        sizes[address] = 4 * end
    return sizes


def select(words: Sequence[int], base: int, functions: Mapping[int, int], emitted: set[int], elf_words: Iterable[int],
           phase: str) -> tuple[list[int], dict[int, str]]:
    """(seeds, {rejected address: reason}) for `phase` "A" (reference evidence) or "B" (none). Addresses are ELF vaddrs; `emitted` is
    the set of function entries of a baseline generation WITHOUT seeds."""
    if phase not in ("A", "B"):
        raise ValueError(f"unknown phase {phase!r}")
    evidence = references(words, base, elf_words, functions)
    starts = sorted(functions)
    extent = effective_sizes(words, base, {a: functions[a] for a in starts if a in emitted})
    emitted_starts = sorted(extent)
    emitted_branch_targets: set[int] = set()
    for a in emitted_starts:
        start = (a - base) // 4
        emitted_branch_targets |= _branch_targets(words[start:start + extent[a] // 4], a)
    outside_branch_targets = _branch_targets(words, base)

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
        at = bisect.bisect_left(emitted_starts, address)
        if at > 0 and address < emitted_starts[at - 1] + extent[emitted_starts[at - 1]]:
            rejected[address] = REASON_INSIDE_EMITTED
            continue
        if address in emitted_branch_targets:
            rejected[address] = REASON_BRANCH_TARGET
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
            if any(address < t < address + functions[address] for t in outside_branch_targets):
                rejected[address] = REASON_ENTERED_BY_BRANCH
                continue
        seeds.append(address)
    return seeds, rejected


def read_seed_file(path: Path) -> list[int]:
    """ELF vaddrs of a seeds file (hex, `#` comments, blank lines); a missing file is an error, never an empty list."""
    if not path.is_file():
        raise FileNotFoundError(f"seeds file {path} not found")
    out = []
    for number, line in enumerate(path.read_text().splitlines(), 1):
        text = line.split("#", 1)[0].strip()
        if text:
            try:
                out.append(int(text, 16))
            except ValueError:
                raise ValueError(f"{path}:{number}: not a hex address: {text!r}") from None
    return out


def write_seeds(path: Path, seeds: Iterable[int]) -> None:
    """The committed list: ELF vaddrs, sorted, no game bytes."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("# extra function seeds for psp_recomp (tools.extra_seeds select): ELF vaddrs of functions nothing reaches\n"
                    + "".join(f"{a:#010x}\n" for a in sorted(set(seeds))))


def merge_with_previous(new: Iterable[int], previous: Iterable[int]) -> list[int]:
    """The list never shrinks silently: a regeneration that already emits the previous seeds must not drop them."""
    return sorted(set(new) | set(previous))


def _generated_sets(directory: Path) -> tuple[set[int], set[int]]:
    registered: set[int] = set()
    labels: set[int] = set()
    for unit in sorted(directory.glob("generated_unit_*.cpp")):
        for line in unit.read_text().split("\n"):
            m = _LABEL.match(line)
            if m:
                labels.add(int(m.group(1), 16))
            registered.update(int(x, 16) for x in _REGISTERED.findall(line))
    return registered, labels


def blocks(directory: Path) -> dict[int, list[str]]:
    """The code of every block label of a generated directory, normalised: the dispatch tables, the case lines and the numeric ids of
    chained direct calls (they renumber when a unit gains labels) are left out; a block ends at its function's closing brace."""
    out: dict[int, list[str]] = {}
    for unit in sorted(directory.glob("generated_unit_*.cpp")):
        current: list[str] | None = None
        for line in unit.read_text().split("\n"):
            if line.startswith("}"):
                current = None
                continue
            if _TABLE_LINE.match(line):
                continue
            m = _LABEL.match(line)
            if m:
                current = out.setdefault(int(m.group(1), 16), [])
            if current is not None:
                current.append(_CHAINED_ID.sub(r"\1#\2", line))
    for lines in out.values():
        while lines and not lines[-1].strip():
            lines.pop()
    return out


def body_changes(before: Mapping[int, list[str]], after: Mapping[int, list[str]]) -> list[int]:
    """Baseline blocks whose code changed in a way a new label does not explain. A block that was split by a seed reads, after, as a
    prefix followed by `goto L_new;` (or by nothing) with the rest under the new label: following the added labels must give the
    baseline body again."""
    def expand(label: int, seen: frozenset[int] = frozenset()) -> list[str]:
        lines = list(after[label])
        if lines and (m := _GOTO.match(lines[-1])) and int(m.group(1), 16) not in before and int(m.group(1), 16) in after and int(m.group(1), 16) not in seen:
            added = int(m.group(1), 16)
            return lines[:-1] + expand(added, seen | {label})[1:]  # drop the added block's own label line
        return lines

    changed = []
    for label, lines in before.items():
        if label not in after:
            continue  # reported as a disappeared block
        if after[label] != lines and expand(label) != lines:
            changed.append(label)
    return sorted(changed)


def verify(before: Path, after: Path, tsv_before: Path, tsv_after: Path, seeds: Iterable[int],
           functions: Mapping[int, int]) -> list[str]:
    """Problems (empty = pass) of a regeneration with seeds against the one without: every registered function entry and every block
    label of `before` is still there with the same code (a split by a new label aside), the function list is byte-identical, and no seed
    lies inside another function. `seeds` and `functions` use the address space of the generated units."""
    errors: list[str] = []
    registered_before, labels_before = _generated_sets(before)
    registered_after, labels_after = _generated_sets(after)
    for a in sorted(registered_before - registered_after):
        errors.append(f"function entry {a:#010x} disappeared")
    for a in sorted(labels_before - labels_after):
        errors.append(f"block {a:#010x} disappeared")
    for a in body_changes(blocks(before), blocks(after)):
        errors.append(f"block {a:#010x} changed its code")
    if tsv_before.read_bytes() != tsv_after.read_bytes():
        errors.append("functions.tsv changed")
    for seed in sorted(set(seeds)):
        for start, size in functions.items():
            if start < seed < start + size:
                errors.append(f"seed {seed:#010x} splits the function at {start:#010x}")
    return errors


def identical_dirs(a: Path, b: Path) -> list[str]:
    """Differences between two generated directories (names and bytes), ignoring the stamp and the seeds file itself."""
    def names(d: Path) -> set[str]:
        return {p.name for p in d.iterdir() if p.is_file() and not p.name.startswith(".") and p.name != "extra_seeds.txt"}

    names_a, names_b = names(a), names(b)
    problems = [f"only in {a}: {n}" for n in sorted(names_a - names_b)] + [f"only in {b}: {n}" for n in sorted(names_b - names_a)]
    problems += [f"{n} differs" for n in sorted(names_a & names_b) if (a / n).read_bytes() != (b / n).read_bytes()]
    return problems


# --- the command line (thin glue over the functions above; the file readers are the only untested parts) ---------------------

def load_text(elf_path: Path):  # pragma: no cover - parses the user's dump (TextImage is covered by tools.family_members' tests)
    from tools.family_members import TextImage

    elf = elf_path.read_bytes()
    image = TextImage.from_elf(elf)
    words = struct.unpack(f"<{len(image.text) // 4}I", image.text[:len(image.text) // 4 * 4])
    elf_words = struct.unpack(f"<{len(elf) // 4}I", elf[:len(elf) // 4 * 4])
    return words, image.base, elf_words


def run_select(elf: Path, functions_tsv: Path, baseline: Path, phase: str, out: Path, loader=load_text) -> tuple[list[int], dict[int, str]]:
    """Seeds for `phase`, with `emitted` taken from a seedless baseline generation, merged with the seeds already in `out`."""
    from tools.progress import load_functions

    words, base, elf_words = loader(elf)
    functions = {a: s for a, s in load_functions(functions_tsv).items() if base <= a < base + 4 * len(words)}
    registered, _ = _generated_sets(baseline)
    seeds, rejected = select(words, base, functions, {a - LOAD_BASE for a in registered}, elf_words, phase)
    previous = read_seed_file(out) if out.is_file() else []
    merged = merge_with_previous(seeds, previous)
    write_seeds(out, merged)
    return merged, rejected


def run_verify(before: Path, after: Path, seeds_file: Path, functions_tsv: Path, tsv_before: Path, elf: Path, loader=load_text) -> list[str]:
    from tools.progress import load_functions

    words, base, _ = loader(elf)
    inside = {a: s for a, s in load_functions(functions_tsv).items() if base <= a < base + 4 * len(words)}
    functions = {LOAD_BASE + a: s for a, s in effective_sizes(words, base, inside).items()}
    seeds = [LOAD_BASE + a for a in read_seed_file(seeds_file)]
    return verify(before, after, tsv_before, functions_tsv, seeds, functions)


def main(argv: Sequence[str] | None = None) -> int:
    from tools.recomp import SEEDS_SOURCE

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    s = sub.add_parser("select")
    s.add_argument("--phase", choices=("A", "B"), required=True)
    s.add_argument("--baseline", type=Path, required=True, help="a generation WITHOUT seeds (tools.recomp --no-extra-seeds)")
    s.add_argument("--out", type=Path, default=SEEDS_SOURCE)
    s.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    s.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    v = sub.add_parser("verify")
    v.add_argument("--before", type=Path, required=True)
    v.add_argument("--after", type=Path, default=Path("recomp/generated"))
    v.add_argument("--seeds", type=Path, default=SEEDS_SOURCE)
    v.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    v.add_argument("--tsv-before", type=Path, required=True, help="a snapshot of functions.tsv taken with the baseline")
    v.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    n = sub.add_parser("noop", help="seeding an already emitted function must leave the generated output byte-identical")
    n.add_argument("--seed", type=lambda t: int(t, 16), required=True, help="runtime address of an emitted function")
    args = parser.parse_args(argv)
    if args.command == "select":
        seeds, rejected = run_select(args.elf, args.functions, args.baseline, args.phase, args.out)
        print(f"phase {args.phase}: {len(seeds)} seeds in {args.out}; rejected {len(rejected)}")
        for address, reason in sorted(rejected.items()):
            print(f"  rejected {LOAD_BASE + address:#010x} {reason}")
        return 0
    if args.command == "verify":
        errors = run_verify(args.before, args.after, args.seeds, args.functions, args.tsv_before, args.elf)
        for e in errors:
            print(f"FAIL: {e}")
        print("verify: " + ("ok" if not errors else f"{len(errors)} problems"))
        return 1 if errors else 0
    return run_noop(args.seed)


def run_noop(seed: int) -> int:  # pragma: no cover - runs psp_recomp twice (minutes, 4 GB)
    from tools.recomp import RECOMP_BIN, REPO, decomp_boundaries, regenerate

    plain, seeded = REPO / "recomp/noop_plain", REPO / "recomp/noop_seeded"
    boundaries = decomp_boundaries(REPO / "src")
    regenerate(REPO / "extracted/EBOOT.elf", plain, RECOMP_BIN, boundaries=boundaries)
    regenerate(REPO / "extracted/EBOOT.elf", seeded, RECOMP_BIN, boundaries=boundaries, seeds=[seed])
    problems = identical_dirs(plain, seeded)
    for p in problems:
        print(f"FAIL: {p}")
    print("noop: " + ("byte-identical" if not problems else f"{len(problems)} differences"))
    return 1 if problems else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
