"""Flag decompiled calls that pass fewer argument registers than the original sets for a callee that reads them,
and reads of v1 after a helper that returns only v0.

Spec: docs/superpowers/specs/2026-11-26-call-arity-audit-design.md. `call_guest` runs the callee on a copy of the
caller's context, so a missing argument is the caller's stale register, not zero: the callee computes something
plausible, and a callee that sleeps is never compared at all. Two such bugs broke play on 2026-11-26 (FINDINGS).
A lint, not a gate: the asm analysis is linear in address order and approximate, so a hit is a review item.

    .venv/bin/python -m tools.call_audit [FILE.cpp ...]     # default: every src/**/*.cpp
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Callable, Sequence
from pathlib import Path

from tools.family_members import TextImage
from tools.progress import LOAD_BASE, load_functions

ARGS = (4, 5, 6, 7, 8, 9, 10, 11)  # a0-a3, t0-t3 (EABI)
NAMES = {4: "a0", 5: "a1", 6: "a2", 7: "a3", 8: "t0", 9: "t1", 10: "t2", 11: "t3"}
MAX_DEPTH = 8  # nested callees followed when a callee passes a register through

WordsOf = Callable[[int], Sequence[int] | None]


# --- asm ------------------------------------------------------------------------------------------------------------------

def reads_writes(word: int) -> tuple[set[int], set[int]]:
    """(GPRs read, GPRs written) by one Allegrex instruction; $zero is never reported."""
    op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
    reads: set[int] = set()
    writes: set[int] = set()
    if op == 0x00:
        funct = word & 0x3F
        if funct in (0x00, 0x02, 0x03):                    # sll, srl/rotr, sra
            reads, writes = {rt}, {rd}
        elif funct == 0x08:                                 # jr
            reads = {rs}
        elif funct == 0x09:                                 # jalr
            reads, writes = {rs}, {rd}
        elif funct in (0x0C, 0x0D, 0x0F):                   # syscall, break, sync
            pass
        elif funct in (0x10, 0x12):                         # mfhi, mflo
            writes = {rd}
        elif funct in (0x11, 0x13):                         # mthi, mtlo
            reads = {rs}
        elif funct in (0x16, 0x17):                         # clz, clo
            reads, writes = {rs}, {rd}
        elif 0x18 <= funct <= 0x1F:                         # mult/div and Allegrex madd/msub
            reads = {rs, rt}
        elif funct in (0x0A, 0x0B):                         # movz, movn: rd keeps its value when not moved
            reads, writes = {rs, rt, rd}, {rd}
        else:                                               # add .. nor, slt, sltu, max, min, shifts by register
            reads, writes = {rs, rt}, {rd}
    elif op == 0x01:                                        # REGIMM: bltz, bgez, bltzal ...
        reads = {rs}
    elif op == 0x02:                                        # j
        pass
    elif op == 0x03:                                        # jal
        writes = {31}
    elif op in (0x04, 0x05, 0x14, 0x15):                    # beq, bne, beql, bnel
        reads = {rs, rt}
    elif op in (0x06, 0x07, 0x16, 0x17):                    # blez, bgtz (+likely)
        reads = {rs}
    elif op == 0x0F:                                        # lui
        writes = {rt}
    elif 0x08 <= op <= 0x0E:                                # addi .. xori
        reads, writes = {rs}, {rt}
    elif op in (0x22, 0x26):                                # lwl, lwr merge into rt
        reads, writes = {rs, rt}, {rt}
    elif 0x20 <= op <= 0x25:                                # lb .. lhu
        reads, writes = {rs}, {rt}
    elif 0x28 <= op <= 0x2E:                                # sb .. swr
        reads = {rs, rt}
    elif op == 0x30:                                        # ll
        reads, writes = {rs}, {rt}
    elif op == 0x38:                                        # sc: stores rt, then writes the success flag to it
        reads, writes = {rs, rt}, {rt}
    elif op == 0x1F:                                        # SPECIAL3: ext, ins, seb/seh/wsbh/bitrev
        funct = word & 0x3F
        if funct == 0x00:
            reads, writes = {rs}, {rt}
        elif funct == 0x04:
            reads, writes = {rs, rt}, {rt}
        else:
            reads, writes = {rt}, {rd}
    elif op == 0x11:                                        # COP1
        if rs in (0, 2):                                    # mfc1, cfc1
            writes = {rt}
        elif rs in (4, 6):                                  # mtc1, ctc1
            reads = {rt}
    elif op == 0x12:                                        # VFPU moves
        if rs == 3:                                         # mfv / mfvc
            writes = {rt}
        elif rs == 7:                                       # mtv / mtvc
            reads = {rt}
    elif op in (0x31, 0x39, 0x32, 0x3A, 0x35, 0x36, 0x3D, 0x3E):   # lwc1/swc1, lv.s/sv.s, lvl/lvr/svl/svr, lv.q/sv.q
        reads = {rs}
    reads.discard(0)
    writes.discard(0)
    return reads, writes


def _target(word: int) -> int:
    return ((word & 0x03FFFFFF) << 2) + LOAD_BASE


def callee_params(address: int, words_of: WordsOf, _memo: dict[int, frozenset[int]] | None = None,
                  _depth: int = 0) -> frozenset[int]:
    """Argument registers the function at `address` reads before writing them (passed-through ones included)."""
    memo = {} if _memo is None else _memo
    if address in memo:
        return memo[address]
    memo[address] = frozenset()  # a recursive call sees no parameters instead of looping
    words = words_of(address)
    if not words:
        # An import stub (sceKernelDelayThread behind func_08B2E2B8's tail call) or code outside .text: unknown, so
        # assume it reads a0-a1: a pass-through wrapper like func_08B2E2B8 hands its first arguments on. Three- and
        # four-argument imports are missed (precision over recall: a0-a3 flagged loop and copy temporaries).
        memo[address] = frozenset(ARGS[:2])
        return memo[address]
    written: set[int] = set()
    params: set[int] = set()

    def use(word: int) -> None:
        r, w = reads_writes(word)
        if word >> 26 == 0x2B and (word >> 21) & 31 == 29:   # sw reg, off($sp): a spill (varargs), not a use
            r = r - {(word >> 16) & 31}
        params.update(x for x in r if x in ARGS and x not in written)
        written.update(w)

    i = 0
    while i < len(words):
        word = words[i]
        if word >> 26 in (0x02, 0x03):                      # j / jal: the delay slot runs first
            if i + 1 < len(words):
                use(words[i + 1])
            nested_unknown = word >> 26 == 0x03 and not words_of(_target(word))   # its caller sets its own registers
            local = word >> 26 == 0x02 and address <= _target(word) < address + 4 * len(words)   # a jump, not a tail call
            if _depth < MAX_DEPTH and not nested_unknown and not local:
                inner = callee_params(_target(word), words_of, memo, _depth + 1)
                params.update(x for x in inner if x not in written)
            if not local:
                written.update(ARGS)                        # caller-saved: clobbered by the call
            i += 2
            continue
        use(word)
        i += 1
    memo[address] = frozenset(params)
    return memo[address]


def _branch_targets(words: Sequence[int]) -> set[int]:
    """Word indices that some branch of the function jumps to (code reachable from elsewhere)."""
    targets = set()
    for i, word in enumerate(words):
        op = word >> 26
        if op in (0x01, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17) or (op == 0x11 and (word >> 21) & 31 == 8):
            offset = word & 0xFFFF
            targets.add(i + 1 + (offset - 0x10000 if offset & 0x8000 else offset))
    return targets


def _ends_block(word: int) -> bool:
    """j, jr, or b (beq $zero, $zero): what follows is reached only through a branch target."""
    op = word >> 26
    return op == 0x02 or (op == 0 and word & 0x3F == 0x08) or (op == 0x04 and (word >> 16) & 0x3FF == 0)


def call_sites(words: Sequence[int]) -> list[tuple[int, frozenset[int]]]:
    """(target, argument registers written on the straight-line path to it, delay slot included) for each `jal`.

    The window starts after the previous call, at a branch target (other paths join there) or after a jump."""
    sites = []
    window: set[int] = set()
    targets = _branch_targets(words)
    i = 0
    while i < len(words):
        word = words[i]
        if i in targets:
            window = set()
        if _ends_block(word):
            i += 2
            window = set()
            continue
        if word >> 26 == 0x03:
            if i + 1 < len(words):
                window.update(reads_writes(words[i + 1])[1])
            sites.append((_target(word), frozenset(x for x in window if x in ARGS)))
            window = set()
            i += 2
            continue
        if word >> 26 == 0 and word & 0x3F == 0x09:     # jalr (a virtual call): its arguments are not the next call's
            window = set()
            i += 2
            continue
        window.update(reads_writes(word)[1])
        i += 1
    return sites


# --- C --------------------------------------------------------------------------------------------------------------------

_START = re.compile(r"^void func_([0-9A-F]{8})\([^)]*\) \{(.*)$", re.MULTILINE)
_CALL = re.compile(r"\b(\w+)\(rt, ctx, 0x([0-9A-Fa-f]{8})u?")
_ANY_CALL = re.compile(r"\b(\w+)\(rt, ctx,")
_ASSIGN = re.compile(r"(?:\bG|ctx\.gpr)\[(\d+)\]\s*=(?!=)")
_V1_READ = re.compile(r"(?:\bG|ctx\.gpr)\[3\](?!\s*=[^=])")
_HELPER = re.compile(r"^\w+ (\w+)\(Runtime &rt, AllegrexContext &ctx, u32 target[^)]*\) \{\n(.*?)^\}",
                     re.MULTILINE | re.DOTALL)


def function_bodies(source: str) -> dict[int, str]:
    """Runtime address -> body text of each `void func_XXXXXXXX(...)` (one-line or until the next column-0 `}`)."""
    bodies = {}
    for match in _START.finditer(source):
        rest = match.group(2)
        if rest.rstrip().endswith("}") and rest.count("{") < rest.count("}"):
            bodies[int(match.group(1), 16)] = rest
            continue
        end = source.find("\n}\n", match.end())
        bodies[int(match.group(1), 16)] = source[match.end():end if end >= 0 else len(source)]
    return bodies


def _count(arguments: str | None) -> int:
    if arguments is None or not arguments.strip():
        return 0
    depth, count = 0, 1
    for ch in arguments:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


CALL_HELPERS = frozenset({"call", "call_guest", "callf", "call_g", "call_keep", "call_virtual", "call_void"})
_ARG_HELPER = re.compile(r"^\w+ (\w+)\(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32>",
                         re.MULTILINE)


def call_helpers(source: str) -> frozenset[str]:
    """Names that place their argument list in a0.. and call: the common ones plus those `source` defines.
    Family helpers (`fetch_singleton_string(rt, ctx, 0x...)`) set the registers themselves and are not counted."""
    return CALL_HELPERS | frozenset(_ARG_HELPER.findall(source))


def c_calls(body: str, helpers: frozenset[str] = CALL_HELPERS) -> list[tuple[str, int, int]]:
    """(helper, constant target, registers in its first argument list) of each call helper to a literal address."""
    calls = []
    for m in _CALL.finditer(body):
        if m.group(1) in helpers:
            calls.append((m.group(1), int(m.group(2), 16), _count(_argument_list(body, m.end()))))
    return calls


def _argument_list(text: str, position: int) -> str | None:
    """The text inside the `{...}` that follows `, ` at `position` (braces nested in the arguments allowed)."""
    rest = text[position:].lstrip()
    if not rest.startswith(","):
        return None
    rest = rest[1:].lstrip()
    if not rest.startswith("{"):
        return None
    depth = 0
    for i, ch in enumerate(rest):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return rest[1:i]
    return None


def assigned_regs(body: str) -> set[int]:
    return {int(n) for n in _ASSIGN.findall(body)}


def v0_only_helpers(source: str) -> set[str]:
    """Call helpers defined in `source` that hand back v0 and drop v1."""
    return {m.group(1) for m in _HELPER.finditer(source)
            if "call_guest(" in m.group(2) and "v1" not in m.group(2) and "gpr[3]" not in m.group(2)}


# --- the two rules --------------------------------------------------------------------------------------------------------

def missing_arguments(address: int, body: str, words_of: WordsOf, memo: dict[int, frozenset[int]] | None = None,
                      helpers: frozenset[str] = CALL_HELPERS) -> list[tuple[int, str, int]]:
    """(target, register, C argument count) for each argument the original sets, the callee reads and C omits."""
    words = words_of(address)
    if not words:
        return []
    memo = {} if memo is None else memo
    c_counts: dict[int, list[int]] = {}
    for _, target, count in c_calls(body, helpers):
        c_counts.setdefault(target, []).append(count)
    sites = call_sites(words)
    asm_counts = {target: sum(1 for t, _ in sites if t == target) for target, _ in sites}
    seen: dict[int, int] = {}
    assigned = assigned_regs(body)
    found = []
    for target, written in sites:
        if target not in c_counts:
            continue
        index = seen.get(target, 0)
        seen[target] = index + 1
        counts = c_counts[target]
        # As many C calls as jal sites: pair them in order (a first call with one argument and a second with two
        # are both right); otherwise compare with the C call that passes the fewest.
        passed = counts[index] if len(counts) == asm_counts[target] else min(counts)
        needed = written & callee_params(target, words_of, memo)
        for reg in sorted(needed):
            if ARGS.index(reg) >= passed and reg not in assigned and (target, NAMES[reg], passed) not in found:
                found.append((target, NAMES[reg], passed))
    return found


def lost_v1(body: str, helpers: set[str]) -> list[str]:
    """Lines that read v1 after a call through a v0-only helper, with no assignment to v1 in between."""
    hits = []
    pending = False
    for line in body.split("\n"):
        code = line.split("//")[0]
        if pending and _V1_READ.search(code) and not re.search(r"(?:\bG|ctx\.gpr)\[3\]\s*=(?!=)", code):
            hits.append(line.strip())
            pending = False
        if re.search(r"(?:\bG|ctx\.gpr)\[3\]\s*=(?!=)", code):
            pending = False
        for call in _ANY_CALL.finditer(code):              # literal or computed target (call_virtual)
            pending = call.group(1) in helpers
    return hits


# --- command line ---------------------------------------------------------------------------------------------------------

def _load_words_of(elf: Path, functions_tsv: Path) -> WordsOf:  # pragma: no cover - needs the user's dump
    image = TextImage.from_elf(elf.read_bytes())
    sizes = load_functions(functions_tsv)

    def words_of(address: int) -> Sequence[int] | None:
        vaddr = address - LOAD_BASE
        size = sizes.get(vaddr)
        if not size or vaddr < image.base or vaddr - image.base + size > len(image.text):
            return None  # an import stub or anything else outside .text: no asm to read
        return image.function(vaddr, size)[0]
    return words_of


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.call_audit", description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    args = parser.parse_args(argv)
    words_of = _load_words_of(args.elf, args.functions_tsv)
    files = args.files or sorted(Path("src").rglob("*.cpp"))
    memo: dict[int, frozenset[int]] = {}
    total = 0
    for path in files:
        source = path.read_text()
        helpers = v0_only_helpers(source)
        callers = call_helpers(source)
        for address, body in sorted(function_bodies(source).items()):
            for target, reg, count in missing_arguments(address, body, words_of, memo, callers):
                print(f"{path}: func_{address:08X} -> 0x{target:08X}: asm sets {reg} (the callee reads it), C passes {count}")
                total += 1
            for line in lost_v1(body, helpers):
                print(f"{path}: func_{address:08X}: reads v1 after a v0-only call: {line}")
                total += 1
    print(f"call audit: {len(files)} files, {total} findings")
    return 1 if total else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
