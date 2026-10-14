"""Members of an instruction-shape family and the constants each one carries in its OWN words.

`tools.shapes` groups functions whose instructions match once immediates and jump targets are masked. A decompiled
member of such a family becomes a template (one body, a table row per address). This tool finds the rest of the
family — the functions of the same masked shape that are not decompiled yet and that the routes reach — and decodes,
from each member's own instruction words and the ELF relocations, what makes it different: the static addresses built
by `lui`/`addiu` pairs (or reached by a `lui` + load/store), the `li`/`ori` immediates and every `jal`/`j` target. A
row of the template's table can then be generated and checked instead of read by hand from generated code.

Values are *relocation-checked*: a `lui`/`addiu` pair is an address only when the ELF has R_MIPS_HI16 / R_MIPS_LO16
on it (the raw words hold the ELF vaddr; the runtime address is that plus the load base 0x08804000); without
relocations the pair is a plain constant (a float bit pattern, a mask). A `jal`/`j` is a call target with R_MIPS_26.

Usage (from the repository root; reads extracted/EBOOT.elf, recomp/analysis/functions.tsv, recomp/generated, src/):
  # 1. which functions to probe (writes "c_ADDR = 0xADDR; // insns" for the pending, emitted ones):
  .venv/bin/python -m tools.family_members --write-probe-symbols PATH [--max-insns 70] [--exclude-ref REF ...]
  #    then run them with `.venv/bin/python -m tools.decomp_gate probe TAG --symbols PATH` (five routes, --decomp off)
  # 2. the families with a decompiled template and reached pending members:
  .venv/bin/python -m tools.family_members --list --probe recomp/analysis/probe_TAG_*.log [--min-reached 3]
  # 3. one family: the members and the values of each (template = any member's runtime address):
  .venv/bin/python -m tools.family_members 0x08C379F8 --probe LOGS... [-v] [--cpp pair_call_a]
  # 4. check the rows already written in a source file (a path, or REF:PATH for a branch) against the words:
  .venv/bin/python -m tools.family_members --check origin/decomp/hot-batch-69:src/game/unsorted/hot_batch_69.cpp
  #    The rows of one wrapper are one family: argument i of every row must be the member's own value at the SAME
  #    word position (the position most rows take argument i from), so swapped, missing or extra arguments fail.
  #    A row whose arguments are not plain literals is counted as unparsed and fails unless --allow-unparsed.
  #    A wrapper with a single row has nothing to compare columns with: its arguments are checked in word order.

`--exclude-ref REF` also treats the registrations in `src/` at a git ref as done (the open batches' branches).
Output of a member: `func_ADDR  hits=N  <values that differ from the template>`; `-v` adds every value of the member
with the word index it comes from. `!!` lines flag words that differ from the template without a value explaining
them (a stack offset, a field offset, a register: the member is NOT the template with other constants).
"""

from __future__ import annotations

import argparse
import re
import struct
import subprocess
import sys
from collections.abc import Callable, Iterable, Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path

from tools.progress import LOAD_BASE
from tools.shapes import clusters, normalize

R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16 = 2, 4, 5, 6
SHT_PRX_RELOC = 0x700000A0
_M = 0xFFFFFFFF

_STORES_AND_FPU = {0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x2F, 0x31, 0x32, 0x35, 0x36, 0x39, 0x3A, 0x3D, 0x3E}
_LOADS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}
_ACCESS_OPS = _LOADS | {0x28, 0x29, 0x2A, 0x2B, 0x2E}  # integer loads and stores whose offset is a plain immediate
_BRANCH_OPS = {0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17}
_CALLEE_SAVED = {16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30}  # s0-s7, gp, sp, fp (EABI32)
_ALU_IMMEDIATE_OPS = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E}
_DISPLACEMENT_OPS = _ACCESS_OPS | {0x30, 0x31, 0x38, 0x39}


class ShapeMismatch(ValueError):
    """Two members do not decode to the same sequence of values (they are not the same shape)."""


@dataclass(frozen=True)
class Value:
    """One per-member constant. `sources` are the word indices it is built from (`uncovered` uses them)."""
    index: int      # word index of the instruction that completes it
    kind: str       # address | const | imm | disp | hi | access | call | tail | rawcall | rawtail
    value: int
    sources: tuple[int, ...]


@dataclass
class _Reg:
    value: int            # the raw (ELF-vaddr) value
    origin: str           # hi (bare lui) | addr (hi + low part) | const (hi + ori) | small (li)
    sources: tuple[int, ...]
    relocated: bool
    segment: int = 0      # base of the segment the relocation points into (added to the offset the words hold)


def _simm(word: int) -> int:
    imm = word & 0xFFFF
    return imm - 0x10000 if imm & 0x8000 else imm


def _written(word: int) -> tuple[int, ...]:
    """The general registers an instruction overwrites (an over-approximation for the odd opcodes)."""
    op, rt, rd = word >> 26, (word >> 16) & 31, (word >> 11) & 31
    if op == 0:
        funct = word & 0x3F
        if funct in (0x08, 0x0C, 0x0D, 0x0F, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B):  # jr, syscall, break, sync, mthi/lo, mult/div
            return ()
        return (rd,)
    if op == 1:
        return (31,) if rt in (0x10, 0x11) else ()  # bltzal, bgezal
    if op == 3:
        return (31,)
    if op == 2 or op in _BRANCH_OPS or op in _STORES_AND_FPU:
        return ()
    if 0x08 <= op <= 0x0F or op in _LOADS or op in (0x30, 0x38):
        return (rt,)
    return (rt, rd)


def _branch_target(word: int, index: int) -> int | None:
    op, rs = word >> 26, (word >> 21) & 31
    if op in _BRANCH_OPS or op == 1 or (op in (0x11, 0x12) and rs == 8):
        return index + 1 + _simm(word)
    return None


def _type(relocs: Mapping[int, int], index: int) -> int:
    return relocs.get(index, 0) & 0xFF


def member_values(words: Sequence[int], address: int, relocs: Mapping[int, int],
                  segments: Sequence[int] = ()) -> list[Value]:
    """The per-member constants of a function: `address` is its ELF vaddr, `relocs` maps a word index to the
    r_info of the relocation on that word (its low byte is the type, bits 16-23 the segment the address is relative
    to) and `segments` holds each segment's base vaddr (the p_vaddr of the program headers). Straight-line register
    tracking; what a call, a branch target or another write makes unknown is forgotten, so a value is only reported
    when its words really build it."""
    def segment_base(index: int) -> int:
        number = (relocs.get(index, 0) >> 16) & 0xFF
        return segments[number] if number < len(segments) else 0

    end = address + 4 * len(words)
    targets = {t for i, w in enumerate(words) if (t := _branch_target(w, i)) is not None}
    regs: dict[int, _Reg] = {}
    values: list[Value] = []
    clear_at: set[int] = set()
    hi_words: dict[int, int] = {}  # lui index -> its value, until something consumes it
    for i, word in enumerate(words):
        if i in targets:
            regs.clear()
        elif i in clear_at:  # after a call or a jump only the callee-saved registers survive
            regs = {r: v for r, v in regs.items() if r in _CALLEE_SAVED}
        op, rs, rt, imm = word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 0xFFFF
        base = regs.get(rs) if rs else None
        new: tuple[int, _Reg] | None = None
        if base is not None and base.origin in ("hi", "addr", "const"):
            consumes = op in _ALU_IMMEDIATE_OPS or op in _DISPLACEMENT_OPS
            if consumes:
                hi_words.pop(base.sources[0], None)
        if op == 0x0F:
            new = (rt, _Reg((imm << 16) & _M, "hi", (i,), _type(relocs, i) == R_MIPS_HI16, segment_base(i)))
            hi_words[i] = (imm << 16) & _M
        elif op in (0x08, 0x09):
            simm = _simm(word)
            if base is not None and base.origin in ("hi", "addr"):
                raw = (base.value + simm) & _M
                relocated = base.relocated and (base.origin == "addr" or _type(relocs, i) == R_MIPS_LO16)
                sources = base.sources + (i,)
                values.append(_complete(i, raw, relocated, sources, base.segment))
                new = (rt, _Reg(raw, "addr", sources, relocated, base.segment))
            else:
                values.append(Value(i, "imm", simm, (i,)))
                if rs == 0:
                    new = (rt, _Reg(simm & _M, "small", (i,), False))
        elif op == 0x0D:
            if base is not None and base.origin in ("hi", "addr", "const"):
                raw = base.value | imm
                sources = base.sources + (i,)
                values.append(Value(i, "const", raw, sources))
                new = (rt, _Reg(raw, "const", sources, False))
            else:
                values.append(Value(i, "imm", imm, (i,)))
                if rs == 0:
                    new = (rt, _Reg(imm, "small", (i,), False))
        elif op in _ALU_IMMEDIATE_OPS:  # slti, sltiu, andi, xori
            values.append(Value(i, "imm", _simm(word) if op in (0x0A, 0x0B) else imm, (i,)))
        elif op in _DISPLACEMENT_OPS:
            if base is not None and base.origin == "hi" and op in _ACCESS_OPS:
                raw = (base.value + _simm(word)) & _M
                relocated = base.relocated and _type(relocs, i) == R_MIPS_LO16
                values.append(Value(i, "access", (raw + base.segment + LOAD_BASE) & _M if relocated else raw,
                                    base.sources + (i,)))
            elif base is None or base.origin == "small":
                values.append(Value(i, "disp", _simm(word), (i,)))
        elif op in (2, 3):
            target = (word & 0x3FFFFFF) << 2  # j/jal keep bits 31-28 of the delay slot's address: right for addresses < 0x10000000 (all of PDIAPP's ELF vaddrs)
            clear_at.add(i + 2)
            if op == 2 and address <= target < end:
                pass  # a jump inside the function (a loop, a shared tail): control flow, not a per-member value
            else:
                relocated = _type(relocs, i) == R_MIPS_26
                kind = ("call" if op == 3 else "tail") if relocated else ("rawcall" if op == 3 else "rawtail")
                values.append(Value(i, kind, (target + segment_base(i) + LOAD_BASE) & _M if relocated else target, (i,)))
        elif op == 0 and (word & 0x3F) in (0x08, 0x09):  # jr, jalr
            clear_at.add(i + 2)
        for register in _written(word):
            regs.pop(register, None)
        if new is not None and new[0]:
            regs[new[0]] = new[1]
    values.extend(Value(index, "hi", value, (index,)) for index, value in hi_words.items())
    return sorted(values, key=lambda v: v.index)


def _complete(index: int, raw: int, relocated: bool, sources: tuple[int, ...], segment: int) -> Value:
    if relocated:
        return Value(index, "address", (raw + segment + LOAD_BASE) & _M, sources)
    return Value(index, "const", raw, sources)


def columns(template: Sequence[Value], members: Sequence[Sequence[Value]]) -> list[int]:
    """Positions (in the value lists) of the family's real parameters: where some member differs from the template,
    minus the positions that hold the same number in every member as an earlier one (a static address that two
    instructions both use is one parameter). Every member must line up with the template (ShapeMismatch otherwise)."""
    for member in members:
        row(template, member)
    varying = [p for p in range(len(template)) if any(m[p].value != template[p].value for m in members)]
    kept: list[int] = []
    for p in varying:
        if not any(all(m[p].value == m[q].value for m in members) and template[p].value == template[q].value for q in kept):
            kept.append(p)
    return kept


def parameters(template: Sequence[Value], members: Mapping[int, Sequence[Value]]
               ) -> tuple[list[int], dict[int, list[Value]], list[int]]:
    """(columns, {member: its values at those columns}, members that do not line up with the template)."""
    lined, stray = {}, []
    shape = [(v.kind, v.index) for v in template]
    for address, values in members.items():
        if [(v.kind, v.index) for v in values] == shape:
            lined[address] = values
        else:
            stray.append(address)
    cols = columns(template, list(lined.values()))
    return cols, {a: [v[p] for p in cols] for a, v in lined.items()}, stray


def row(template: Sequence[Value], member: Sequence[Value]) -> list[Value]:
    """The member's values that differ from the template's, in word order."""
    if [(v.kind, v.index) for v in template] != [(v.kind, v.index) for v in member]:
        raise ShapeMismatch("the member decodes to a different sequence of values than the template")
    return [m for t, m in zip(template, member) if t.value != m.value]


def uncovered(template: Sequence[int], member: Sequence[int], values: Iterable[Value],
              template_address: int | None = None, member_address: int | None = None) -> list[int]:
    """Word indices where the member differs from the template and no value explains it. With both addresses,
    a `j`/`jal` whose target is the same distance from its function's start in both counts as explained (the
    relocated internal jumps of a clamp step)."""
    explained = {s for v in values for s in v.sources}
    out = []
    for i, (t, m) in enumerate(zip(template, member)):
        if t == m or i in explained:
            continue
        if (t >> 26) in (2, 3) and (t >> 26) == (m >> 26) and template_address is not None and member_address is not None:
            if ((m & 0x3FFFFFF) << 2) - member_address == ((t & 0x3FFFFFF) << 2) - template_address:
                continue
        out.append(i)
    return out


# --- finding the members -----------------------------------------------------------------------------------------------

def find_members(code: Mapping[int, bytes], template: int, reached: Iterable[int] | None, done: Iterable[int]) -> list[int]:
    """Functions of the template's shape, not done, and (when `reached` is given) reached; the template excluded."""
    shape = normalize(code[template])
    skip, seen = set(done), None if reached is None else set(reached)
    return [a for a in sorted(code)
            if a != template and a not in skip and (seen is None or a in seen) and normalize(code[a]) == shape]


@dataclass(frozen=True)
class Family:
    template: int
    pending: list[int]
    done: list[int] = field(default_factory=list)


def families(code: Mapping[int, bytes], reached: Iterable[int] | None, done: Iterable[int], min_reached: int,
             need_template: bool = True) -> list[Family]:
    """Shape clusters with at least `min_reached` reached pending members (besides the template), biggest first.
    The template is the first decompiled member; without one (need_template False) the first pending member."""
    skip, seen = set(done), None if reached is None else set(reached)
    rows = []
    for group in clusters(code):
        pending = [a for a in group if a not in skip and (seen is None or a in seen)]
        templates = [a for a in group if a in skip]
        if not templates:
            if need_template or not pending:
                continue
            templates, pending = pending[:1], pending[1:]
        if len(pending) >= min_reached:
            rows.append(Family(templates[0], pending, templates))
    return sorted(rows, key=lambda f: (-len(f.pending), f.template))


def reached_from_logs(texts: Iterable[str]) -> dict[int, int]:
    """ELF vaddr -> hits summed over the probe logs (`[probe] 0xADDR hits=N ...` lines); functions with no hit are left out."""
    hits: dict[int, int] = {}
    for text in texts:
        for match in re.finditer(r"\[probe\] 0x([0-9A-Fa-f]{8}) hits=(\d+)", text):
            if int(match[2]):
                address = int(match[1], 16) - LOAD_BASE
                hits[address] = hits.get(address, 0) + int(match[2])
    return hits


def probe_symbols(sizes: Mapping[int, int], done: Iterable[int], emitted: Iterable[int], max_insns: int) -> str:
    """A GT_PROBE_SYMBOLS file naming every pending, recompiled function of at most `max_insns` instructions."""
    skip, present = set(done), set(emitted)
    lines = []
    for address in sorted(sizes):
        if address in present and address not in skip and sizes[address] // 4 <= max_insns:
            runtime = address + LOAD_BASE
            lines.append(f"c_{runtime:08X} = 0x{runtime:08X}; // {sizes[address] // 4}\n")
    return "".join(lines)


# --- rows of a template table: generated and checked -------------------------------------------------------------------

def _literal(value: Value) -> str:
    if value.kind == "imm":
        return f"{value.value}u" if value.value >= 0 else f"0x{value.value & _M:08X}u"
    return f"0x{value.value & _M:08X}u"


def cpp_row(runtime_address: int, wrapper: str, values: Sequence[Value]) -> str:
    """The one-line wrapper instance the batch files use: `void func_X(rt, ctx) { wrapper(rt, ctx, values...); }`."""
    args = "".join(f", {_literal(v)}" for v in values)
    return (f"void func_{runtime_address:08X}(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) "
            f"{{ {wrapper}(rt, ctx{args}); }}")


_LITERAL = r"(?:0[xX][0-9A-Fa-f]+|\d+)[uU]?"
_HEAD = r"^void func_([0-9A-Fa-f]{8})\([^)]*\)\s*\{\s*(\w+)\(rt, ctx"
_ROW = re.compile(_HEAD + rf"((?:,\s*{_LITERAL})*)\);\s*\}}", re.MULTILINE)
_ANY_ROW = re.compile(_HEAD + r"((?:,[^;]*)?)\);\s*\}", re.MULTILINE)


def parse_all_rows(source: str) -> list[tuple[int, str, list[int] | None]]:
    """(runtime address, wrapper, numeric arguments) of every one-line wrapper instance in a source text; the
    arguments are None when any of them is not a plain hex/decimal literal (-1, a symbol, an expression): such a row
    cannot be compared and the caller has to say so instead of dropping it."""
    out: list[tuple[int, str, list[int] | None]] = []
    for m in _ANY_ROW.finditer(source):
        strict = _ROW.fullmatch(m[0])
        args = None if strict is None else [int(a.strip().rstrip("uU"), 0) for a in strict[3].split(",") if a.strip()]
        out.append((int(m[1], 16), m[2], args))
    return out


def parse_rows(source: str) -> list[tuple[int, str, list[int]]]:
    """(runtime address, wrapper, numeric arguments) of every one-line wrapper instance with literal arguments."""
    return [(a, w, args) for a, w, args in parse_all_rows(source) if args is not None]


def check_row(args: Sequence[int], expected: Sequence[int]) -> str | None:
    """None when the row has exactly the expected values, argument i against column i (modulo 2**32); otherwise
    what is wrong: the count, or the first argument that is not its column's value."""
    if len(args) != len(expected):
        return f"{len(args)} arguments, the wrapper's table has {len(expected)} columns"
    for position, (arg, want) in enumerate(zip(args, expected)):
        if arg & _M != want & _M:
            return f"argument {position} is 0x{arg & _M:08X}, the member's own words give 0x{want & _M:08X}"
    return None


def column_map(rows: Sequence[tuple[Sequence[int], Sequence[Value]]], width: int) -> list[int]:
    """For each of the `width` arguments, the value position (in the members' decoded lists, which all line up) that
    holds that argument in the most rows: the column the wrapper's table takes it from. The first such position wins
    a tie, so two columns that no row tells apart still compare positionally."""
    chosen = []
    for k in range(width):
        hits: dict[int, int] = {}
        for args, values in rows:
            for p, v in enumerate(values):
                if v.value & _M == args[k] & _M:
                    hits[p] = hits.get(p, 0) + 1
        chosen.append(min(hits, key=lambda p: (-hits[p], p)) if hits else -1)
    return chosen


@dataclass
class CheckResult:
    ok: int = 0
    bad: list[tuple[int, str]] = field(default_factory=list)
    unknown: list[int] = field(default_factory=list)      # no argument to compare, or not a function of the ELF
    unparsed: list[int] = field(default_factory=list)     # arguments that are not plain literals: NOT compared
    single: list[int] = field(default_factory=list)       # only row of its wrapper: checked in word order only


def check_rows(source: str, words_of: Callable[[int], Sequence[int] | None],
               relocs_of: Callable[[int], Mapping[int, int]], segments: Sequence[int] = ()) -> CheckResult:
    """Every wrapper row of `source` against the words of its own function (`words_of(runtime address)`).

    The rows of one wrapper are one family: the table's columns are the positions where the members' decoded values
    differ (`columns`, as `--cpp` prints them), and argument i of a row must equal the member's own value at
    column i. A row with another number of arguments, or a function that does not decode to the family's shape,
    fails; a row whose arguments are not literals is counted in `unparsed`."""
    result = CheckResult()
    families: dict[str, list[tuple[int, list[int], list[Value]]]] = {}
    for runtime, wrapper, args in parse_all_rows(source):
        if args is None:
            result.unparsed.append(runtime)
            continue
        words = words_of(runtime)
        if words is None or not args:
            result.unknown.append(runtime)
            continue
        values = member_values(words, runtime - LOAD_BASE, relocs_of(runtime), segments)
        families.setdefault(wrapper, []).append((runtime, args, values))
    for rows in families.values():
        shapes = [[(v.kind, v.index) for v in values] for _, _, values in rows]
        common = max(shapes, key=shapes.count)
        counts = [len(args) for _, args, _ in rows]
        width = max(set(counts), key=lambda n: (counts.count(n), n))
        for (runtime, _, _), shape in zip(rows, shapes):
            if shape != common:
                result.bad.append((runtime, "its words do not decode to the same sequence of values as the other rows of the wrapper"))
        lined = [r for r, shape in zip(rows, shapes) if shape == common]
        for runtime, args, _ in lined:
            if len(args) != width:
                result.bad.append((runtime, f"{len(args)} arguments, the other rows of the wrapper have {width}"))
        rated = [(runtime, args, values) for runtime, args, values in lined if len(args) == width]
        if len(rated) == 1:  # one row: no other row says which word feeds which column; word order is all there is
            runtime, args, values = rated[0]
            result.single.append(runtime)
            cols = None
        else:
            cols = column_map([(args, values) for _, args, values in rated], width)
        for runtime, args, values in rated:
            if cols is None:
                pool = [v.value & _M for v in values]
                position, message = 0, None
                for arg in args:
                    try:
                        position = pool.index(arg & _M, position) + 1
                    except ValueError:
                        message = f"argument 0x{arg & _M:08X} is not (in word order) among the member's words"
                        break
            else:
                message = None
                if -1 in cols:
                    message = f"argument {cols.index(-1)} is no value of the member's words"
                else:
                    message = check_row(args, [values[p].value for p in cols])
            if message is None:
                result.ok += 1
            else:
                result.bad.append((runtime, message))
    result.bad.sort()
    return result


# --- the ELF -------------------------------------------------------------------------------------------------------------

def parse_relocations(section: bytes) -> dict[int, int]:
    """r_offset (an ELF vaddr) -> r_info for a PRX `.rel.*` section (8-byte Elf32_Rel entries). The low byte of
    r_info is the type; bits 16-23 name the segment an address is relative to."""
    return {off: info for off, info in struct.iter_unpack("<II", section[: len(section) // 8 * 8])}


def _sections(elf: bytes) -> list[tuple[bytes, tuple[int, ...]]]:
    shoff, = struct.unpack_from("<I", elf, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 46)
    raw = [struct.unpack_from("<10I", elf, shoff + i * shentsize) for i in range(shnum)]
    names = raw[shstrndx]
    return [(elf[names[4] + s[0]:elf.index(b"\0", names[4] + s[0])], s) for s in raw]


def elf_relocations(elf: bytes) -> dict[int, int]:
    """The relocations of `.text` (vaddr -> r_info)."""
    for name, s in _sections(elf):
        if name == b".rel.text" and s[1] == SHT_PRX_RELOC:
            return parse_relocations(elf[s[4]:s[4] + s[5]])
    raise ValueError("no .rel.text section")


@dataclass
class TextImage:
    """`.text` with its relocations: serves the words of one function."""
    base: int
    text: bytes
    relocs: dict[int, int]
    segments: list[int] = field(default_factory=list)  # p_vaddr of each program header

    @classmethod
    def from_elf(cls, elf: bytes) -> TextImage:
        for name, s in _sections(elf):
            if name == b".text":
                phoff, = struct.unpack_from("<I", elf, 28)
                phentsize, phnum = struct.unpack_from("<HH", elf, 42)
                segments = [struct.unpack_from("<I", elf, phoff + i * phentsize + 8)[0] for i in range(phnum)]
                return cls(s[3], elf[s[4]:s[4] + s[5]], elf_relocations(elf), segments)
        raise ValueError("no .text section")

    def function(self, vaddr: int, size: int) -> tuple[list[int], dict[int, int]]:
        """(words, {word index: r_info}) of the function at ELF vaddr `vaddr`, `size` bytes."""
        start = vaddr - self.base
        words = list(struct.unpack(f"<{size // 4}I", self.text[start:start + size // 4 * 4]))
        relocs = {i: self.relocs[vaddr + 4 * i] for i in range(len(words)) if vaddr + 4 * i in self.relocs}
        return words, relocs


# --- command line ---------------------------------------------------------------------------------------------------------

def _git_show(ref_path: str) -> str:  # pragma: no cover - runs git
    return subprocess.run(["git", "show", ref_path], capture_output=True, text=True, check=True).stdout


def _registrations_at(ref: str) -> set[int]:  # pragma: no cover - runs git
    from tools.progress import registered_addresses
    paths = subprocess.run(["git", "ls-tree", "-r", "--name-only", ref, "src"], capture_output=True, text=True,
                           check=True).stdout.split()
    return {a - LOAD_BASE for path in paths if path.endswith(".cpp") for a in registered_addresses(_git_show(f"{ref}:{path}"))}


def _vaddr(text: str) -> int:
    value = int(text, 0)
    return value - LOAD_BASE if value >= LOAD_BASE else value


def _describe(values: Iterable[Value]) -> str:
    return " ".join(_literal(v) for v in values)


def main(argv: Sequence[str] | None = None) -> int:
    from tools.progress import load_functions
    from tools.shapes import done_functions, recompiled_functions, split_inside

    parser = argparse.ArgumentParser(prog="tools.family_members", description=__doc__.splitlines()[0])
    parser.add_argument("template", nargs="?", help="runtime address (0x08...) of any member of the family")
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    parser.add_argument("--src", type=Path, default=Path("src"))
    parser.add_argument("--no-src", action="store_true", help="count nothing in src/ as done")
    parser.add_argument("--recompiled", type=Path, default=Path("recomp/generated"))
    parser.add_argument("--no-generated", action="store_true", help="do not drop the functions the recompiler never emitted")
    parser.add_argument("--exclude-ref", action="append", default=[], metavar="REF",
                        help="also count the registrations in src/ at this git ref as done (repeatable)")
    parser.add_argument("--probe", nargs="*", default=[], type=Path, metavar="LOG",
                        help="probe logs (GT_PROBE_SYMBOLS); only reached functions are members")
    parser.add_argument("--list", action="store_true", help="list the families with a template and reached members")
    parser.add_argument("--min-reached", type=int, default=3)
    parser.add_argument("--any-template", action="store_true", help="--list: families with no decompiled member too")
    parser.add_argument("--write-probe-symbols", type=Path, metavar="FILE")
    parser.add_argument("--max-insns", type=int, default=70)
    parser.add_argument("--check", metavar="[REF:]PATH", help="check the wrapper rows of a source file against the words")
    parser.add_argument("--allow-unparsed", action="store_true",
                        help="--check: do not fail on rows whose arguments are not literals (they are still counted)")
    parser.add_argument("--cpp", metavar="WRAPPER", help="print each member as a one-line wrapper instance")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args(argv)

    image = TextImage.from_elf(args.elf.read_bytes())
    sizes = load_functions(args.functions)
    inside, outside = split_inside(sizes, image.base, len(image.text))
    if outside:
        print(f"{outside} functions lie outside .text (ignored)", file=sys.stderr)
    done: set[int] = set() if args.no_src else set(done_functions(args.src))
    for ref in args.exclude_ref:
        done |= _registrations_at(ref)
    emitted = None
    if not args.no_generated:
        units = sorted(args.recompiled.glob("generated_unit_*.cpp"))
        if units:
            emitted = recompiled_functions(u.read_text() for u in units)
        else:
            print(f"warning: no generated units in {args.recompiled}: code nothing reaches is counted too", file=sys.stderr)

    if args.check:
        text = _git_show(args.check) if ":" in args.check and not Path(args.check).exists() else Path(args.check).read_text()

        def words_of(runtime: int) -> list[int] | None:
            vaddr = runtime - LOAD_BASE
            return image.function(vaddr, sizes[vaddr])[0] if vaddr in sizes else None

        def relocs_of(runtime: int) -> dict[int, int]:
            vaddr = runtime - LOAD_BASE
            return image.function(vaddr, sizes[vaddr])[1]

        result = check_rows(text, words_of, relocs_of, image.segments)
        print(f"{result.ok} rows match their own words; {len(result.bad)} do not; {len(result.unparsed)} unparsed "
              f"(arguments that are not literals, NOT compared); {len(result.unknown)} without arguments / unknown function; "
              f"{len(result.single)} alone in their wrapper (checked in word order only)")
        for runtime, message in result.bad:
            print(f"  BAD func_{runtime:08X}: {message}")
        for runtime in result.unparsed:
            print(f"  UNPARSED func_{runtime:08X}")
        return 1 if result.bad or (result.unparsed and not args.allow_unparsed) else 0

    if args.write_probe_symbols:
        text = probe_symbols(sizes, done, emitted if emitted is not None else inside, args.max_insns)
        args.write_probe_symbols.write_text(text)
        print(f"{text.count(chr(10))} functions written to {args.write_probe_symbols}")
        return 0

    scope = inside if emitted is None else {a: n for a, n in inside.items() if a in emitted}
    code = {a: image.text[a - image.base:a - image.base + n] for a, n in scope.items()}
    hits = reached_from_logs(p.read_text() for p in args.probe) if args.probe else None
    reached = None if hits is None else set(hits)

    if args.list:
        for family in families(code, reached, done, args.min_reached, need_template=not args.any_template)[:60]:
            print(f"{len(family.pending):4d} x {sizes[family.template] // 4:3d} insns  template 0x{family.template + LOAD_BASE:08X}"
                  f"  e.g. 0x{family.pending[0] + LOAD_BASE:08X}")
        return 0

    if not args.template:
        parser.error("a template address, --list, --check or --write-probe-symbols is required")
    template = _vaddr(args.template)
    if template not in code:
        print(f"0x{template + LOAD_BASE:08X} is not a known (recompiled) function", file=sys.stderr)
        return 2
    members = find_members(code, template, reached, done)
    t_words, t_relocs = image.function(template, sizes[template])
    t_values = member_values(t_words, template, t_relocs, image.segments)
    print(f"template func_{template + LOAD_BASE:08X} ({len(t_words)} insns): {_describe(t_values)}")
    print(f"{len(members)} {'reached ' if reached is not None else ''}pending members")
    problems = 0
    loaded = {}
    for address in members:
        words, relocs = image.function(address, sizes[address])
        loaded[address] = (words, member_values(words, address, relocs, image.segments))
    cols, rows, stray = parameters(t_values, {a: v for a, (_, v) in loaded.items()})
    for address in stray:
        print(f"!! func_{address + LOAD_BASE:08X}: values do not line up with the template: {_describe(loaded[address][1])}")
        problems += 1
    for address, params in rows.items():
        words, values = loaded[address]
        extra = f"  hits={hits[address]}" if hits is not None else ""
        if args.cpp:
            print(cpp_row(address + LOAD_BASE, args.cpp, params))
        else:
            print(f"func_{address + LOAD_BASE:08X}{extra}  {_describe(params)}")
        if args.verbose:
            for v in values:
                print(f"      [{v.index:3d}] {v.kind:8s} 0x{v.value & _M:08X}  words {list(v.sources)}")
        left = uncovered(t_words, words, values, template, address)
        if left:
            problems += 1
            print(f"!! func_{address + LOAD_BASE:08X}: words {left} differ from the template with no value explaining them")
    return 1 if problems else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
