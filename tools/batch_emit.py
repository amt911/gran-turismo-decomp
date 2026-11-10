"""Register-level drafts of Allegrex functions for the synthetic-case batches (docs/agents/decomp-workflow.md).

`pool` lists the pending functions (no `void func_X(` definition and no registration line, commented ones included, in
`src/`) whose instructions this emitter supports. `emit` prints one draft per function: the same register operations as
the original, one C++ statement per instruction (labels and gotos for branches, delay slots applied), to be read against
`tools.disasm` and proven by the synthetic cases. It is a reading aid that produces a starting point, never the proof:
the gate and `tools.literal_check` / `tools.frame_audit` / `tools.sp_loads` / `tools.arg_reads` are.

Refused on purpose (with the reason): coprocessor and VFPU instructions, unaligned loads (`lwl`/`lwr`), `jr` to a
register other than ra, linking branches, a branch in a delay slot, a branch into a delay slot, a `lui` high half with no
matching low half, `sp` used after the frame is released on the same path.

Usage (from the repository root):
  .venv/bin/python -m tools.batch_emit pool [--min N] [--max N] [--limit N]
  .venv/bin/python -m tools.batch_emit emit 0xADDR...
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path

LOAD_BASE = 0x08804000
_LINE = re.compile(r"^\s+([0-9A-F]{8}): ([0-9A-F]{8})\s+\S+.*?(?:\[(HI|LO)\](?: = 0x([0-9A-F]{8}))?)?\s*$")
_JUMP = re.compile(r"^\s+([0-9A-F]{8}): [0-9A-F]{8}\s+(j|jal)\s.*-> (?:func_)?([0-9A-F]{8})\s*$")
_REGISTRATION = re.compile(r"^\s*(?://\s*)?[A-Z_][A-Z0-9_]*\(\s*0x([0-9A-Fa-f]{8})u?\b")
_DEFINITION = re.compile(r"\bvoid\s+func_([0-9A-Fa-f]{8})\(")


class Refused(Exception):
    """The function uses something this emitter does not model."""


def hexa(value: int) -> str:
    """A 32-bit literal as the disassembly prints it: addresses and other wide values as eight digits."""
    value &= 0xFFFFFFFF
    return f"0x{value:08X}u" if value > 0xFFFFFF else f"0x{value:X}u"


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def done_addresses(src: Path) -> set[int]:
    """Addresses defined (`void func_X(`) or registered (`NAME(0xADDRu,`, commented lines included) anywhere under `src`."""
    done: set[int] = set()
    for path in src.rglob("*.cpp"):
        for line in path.read_text(errors="replace").split("\n"):
            for match in _DEFINITION.finditer(line):
                done.add(int(match[1], 16))
            match = _REGISTRATION.match(line)
            if match:
                done.add(int(match[1], 16))
    return done


class Insn:
    __slots__ = ("pc", "word", "kind", "target")

    def __init__(self, pc: int, word: int, kind: str | None, target: int | None) -> None:
        self.pc, self.word, self.kind, self.target = pc, word, kind, target

    op = property(lambda self: self.word >> 26)
    rs = property(lambda self: (self.word >> 21) & 31)
    rt = property(lambda self: (self.word >> 16) & 31)
    rd = property(lambda self: (self.word >> 11) & 31)
    sa = property(lambda self: (self.word >> 6) & 31)
    funct = property(lambda self: self.word & 63)
    imm = property(lambda self: self.word & 0xFFFF)


def parse_disasm(text: str) -> tuple[int, list[Insn], dict[int, int], dict[int, int]]:
    """Address, instructions, `j` targets and `jal` targets (by pc) out of `tools.disasm ADDR`'s output."""
    instructions: list[Insn] = []
    jumps: dict[int, int] = {}
    calls: dict[int, int] = {}
    address = 0
    for line in text.split("\n"):
        if line.startswith("== func_"):
            address = int(line[8:16], 16)
            continue
        match = _LINE.match(line)
        if not match:
            continue
        instructions.append(Insn(int(match[1], 16), int(match[2], 16), match[3], int(match[4], 16) if match[4] else None))
        jump = _JUMP.match(line)
        if jump:
            (jumps if jump[2] == "j" else calls)[int(jump[1], 16)] = int(jump[3], 16)
    return address, instructions, jumps, calls


def disassemble(address: int) -> str:
    return subprocess.run([sys.executable, "-m", "tools.disasm", f"{address:08X}"], capture_output=True, text=True,
                          check=True).stdout


def reg(number: int) -> str:
    return "0u" if number == 0 else f"G[{number}]"


def branch_target(instruction: Insn) -> int:
    return (instruction.pc + 4 + signed16(instruction.imm) * 4) & 0xFFFFFFFF


_LOADS = {0x20: "sext(load8({a}), 8u)", 0x24: "load8({a})", 0x21: "sext(load16({a}), 16u)", 0x25: "load16({a})",
          0x23: "word(rt, {a})"}
_STORES = {0x28: "store8({a}, {v});", 0x29: "store16({a}, {v});", 0x2B: "put(rt, {a}, {v});"}
_BRANCH_IMM = {0x04: ("==", False), 0x05: ("!=", False), 0x14: ("==", True), 0x15: ("!=", True)}
_ALU = {0x21: "{rs} + {rt}", 0x23: "{rs} - {rt}", 0x24: "{rs} & {rt}", 0x25: "{rs} | {rt}", 0x26: "{rs} ^ {rt}",
        0x27: "~({rs} | {rt})",
        0x2A: "static_cast<std::int32_t>({rs}) < static_cast<std::int32_t>({rt}) ? 1u : 0u",
        0x2B: "{rs} < {rt} ? 1u : 0u",
        0x2C: "static_cast<std::uint32_t>(std::max(static_cast<std::int32_t>({rs}), static_cast<std::int32_t>({rt})))",
        0x2D: "static_cast<std::uint32_t>(std::min(static_cast<std::int32_t>({rs}), static_cast<std::int32_t>({rt})))"}


class Emitter:
    def __init__(self, address: int, instructions: Sequence[Insn], jumps: dict[int, int], calls: dict[int, int]) -> None:
        self.address = address
        self.insns = list(instructions)
        self.index = {i.pc: n for n, i in enumerate(self.insns)}
        self.end = self.insns[-1].pc + 4 if self.insns else address
        self.jumps, self.calls = jumps, calls
        self.frame_bytes = 0
        self.released = False
        self.known: dict[int, int] = {}  # register -> the relocated high half it holds (until written or a label)
        self.labels: set[int] = set()
        self.delay_slots: set[int] = set()

    # --- structure -------------------------------------------------------------------------------------------------
    @staticmethod
    def is_control(i: Insn) -> bool:
        if i.op in (0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17):
            return True
        if i.op == 0x11 and i.rs == 8:
            raise Refused("cop1 branch")
        return i.op == 0x00 and i.funct in (0x08, 0x09)

    def intra_target(self, i: Insn) -> int | None:
        if i.op == 0x02:
            target = self.jumps.get(i.pc)
            if target is None:
                raise Refused(f"unresolved jump at {i.pc:08X}")
            return target if self.address <= target < self.end else None
        if i.op in (0x01, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17):
            return branch_target(i)
        return None

    def scan(self) -> None:
        n = 0
        while n < len(self.insns):
            i = self.insns[n]
            if self.is_control(i):
                self.delay_slots.add(i.pc + 4)
                if n + 1 >= len(self.insns):
                    raise Refused("control transfer at the end")
                if self.is_control(self.insns[n + 1]):
                    raise Refused(f"branch in the delay slot at {i.pc + 4:08X}")
                target = self.intra_target(i)
                if target is not None:
                    self.labels.add(target)
                n += 2
                continue
            n += 1
        if self.labels & self.delay_slots:
            raise Refused("branch into a delay slot")
        for target in self.labels:
            if target not in self.index:
                raise Refused(f"branch target {target:08X} outside the function")
        for n, i in enumerate(self.insns):
            if i.op == 0x09 and i.rs == 29 and i.rt == 29 and signed16(i.imm) < 0:
                self.frame_bytes = -signed16(i.imm)
                if any(j.rs == 29 and j.op in (0x09, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B) for j in self.insns[:n]):
                    raise Refused("sp used before the frame is open")
                break

    # --- addresses -------------------------------------------------------------------------------------------------
    def sp_offset(self, i: Insn) -> str:
        if not self.frame_bytes:
            raise Refused("sp used without a frame")
        if self.released:
            raise Refused("sp used after the frame is released")
        if signed16(i.imm) < 0:
            raise Refused("negative sp displacement")
        return f"frame.at({hexa(signed16(i.imm))})"

    def address_of(self, i: Insn) -> str:
        if i.rs == 29:
            return self.sp_offset(i)
        if i.kind == "LO":
            target = self.lo_value(i)
            if self.known.get(i.rs) == (target + 0x8000) & 0xFFFF0000:
                return hexa(target)
            return f"{reg(i.rs)} + sext({hexa(target)}, 16u)"
        displacement = signed16(i.imm)
        base = reg(i.rs)
        if displacement < 0:
            return f"{base} - {hexa(-displacement)}"
        return f"{base} + {hexa(displacement)}"

    @staticmethod
    def lo_value(i: Insn) -> int:
        if i.target is None:
            raise Refused(f"low half without an address at {i.pc:08X}")
        return i.target

    def hi_address(self, i: Insn) -> int:
        for later in self.insns[self.index[i.pc] + 1:]:
            if later.kind == "LO" and later.rs == i.rt and later.target is not None:
                return later.target
        raise Refused(f"high half without a low half at {i.pc:08X}")

    # --- one instruction -------------------------------------------------------------------------------------------
    @staticmethod
    def written(i: Insn) -> int | None:
        """The general register an instruction writes, if any."""
        if i.op == 0x00:
            return None if i.funct in (0x08, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B) else i.rd
        if i.op == 0x1F:
            return i.rd if i.funct == 0x20 else i.rt
        if 0x08 <= i.op <= 0x0F or 0x20 <= i.op <= 0x26:
            return i.rt
        if i.op == 0x03:
            return 31
        return None

    def simple(self, i: Insn) -> str:
        """The statement of a non-control instruction (tracks which registers still hold a relocated high half)."""
        text = self.simple_text(i)
        written = self.written(i)
        if written is not None:
            self.known.pop(written, None)
        if i.op == 0x0F and i.kind == "HI" and i.rt:
            self.known[i.rt] = (self.hi_address(i) + 0x8000) & 0xFFFF0000
        return text

    def simple_text(self, i: Insn) -> str:
        if i.word == 0:
            return ""
        op = i.op
        rs, rt = reg(i.rs), reg(i.rt)
        if op == 0x00:
            return self.special(i, rs, rt, f"G[{i.rd}]")
        if op == 0x1F:
            return self.special3(i, rs, f"G[{i.rt}]", f"G[{i.rd}]")
        if op in _LOADS:
            return "" if i.rt == 0 else f"G[{i.rt}] = {_LOADS[op].format(a=self.address_of(i))};"
        if op in _STORES:
            return _STORES[op].format(a=self.address_of(i), v=rt)
        immediate = signed16(i.imm)
        if op == 0x09 and i.rt == 29 and i.rs == 29:
            if immediate > 0:
                self.released = True
            return ""
        if i.rt == 0 and op in (0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F):
            return ""
        dest = f"G[{i.rt}]"
        if op == 0x09:
            if i.rs == 29:
                return f"{dest} = {self.sp_offset(i)};"
            if i.kind == "LO":
                target = self.lo_value(i)
                if self.known.get(i.rs) == (target + 0x8000) & 0xFFFF0000:
                    return f"{dest} = {hexa(target)};"
                return f"{dest} = {rs} + sext({hexa(target)}, 16u);"
            if i.rs == 0:
                return f"{dest} = {hexa(immediate)};"
            if immediate < 0:
                return f"{dest} = {rs} - {hexa(-immediate)};"
            return f"{dest} = {rs} + {hexa(immediate)};"
        if op == 0x0A:
            literal = f"static_cast<std::int32_t>({hexa(immediate)})" if immediate < 0 else f"{immediate:#x}"
            return f"{dest} = static_cast<std::int32_t>({rs}) < {literal} ? 1u : 0u;"
        if op == 0x0B:
            return f"{dest} = {rs} < {hexa(immediate)} ? 1u : 0u;"
        if op == 0x0C:
            return f"{dest} = {rs} & {hexa(i.imm)};"
        if op == 0x0D:
            return f"{dest} = {hexa(i.imm)};" if i.rs == 0 else f"{dest} = {rs} | {hexa(i.imm)};"
        if op == 0x0E:
            return f"{dest} = {rs} ^ {hexa(i.imm)};"
        if op == 0x0F:
            if i.kind == "HI":
                address = self.hi_address(i)
                return f"{dest} = high_half({hexa(address)});"
            return f"{dest} = {hexa(i.imm << 16)};"
        raise Refused(f"opcode {op:#x} at {i.pc:08X}")

    def special(self, i: Insn, rs: str, rt: str, rd: str) -> str:
        f = i.funct
        if f in (0x0A, 0x0B):  # movz / movn
            return "" if i.rd == 0 else f"if ({rt} {'==' if f == 0x0A else '!='} 0u) {rd} = {rs};"
        if f in (0x18, 0x19):  # mult / multu
            if f == 0x18:
                product = f"static_cast<std::int64_t>(static_cast<std::int32_t>({rs})) * static_cast<std::int32_t>({rt})"
            else:
                product = f"static_cast<std::uint64_t>({rs}) * {rt}"
            return (f"{{ const auto product = {product}; ctx.lo = static_cast<std::uint32_t>(product); "
                    f"ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); }}")
        if f == 0x1A:
            return f"divide(ctx, {rs}, {rt});"
        if f == 0x1B:
            return (f"if ({rt} == 0u) {{ ctx.lo = 0xFFFFFFFFu; ctx.hi = {rs}; }} "
                    f"else {{ ctx.lo = {rs} / {rt}; ctx.hi = {rs} % {rt}; }}")
        if f in (0x10, 0x12):
            return "" if i.rd == 0 else f"{rd} = ctx.{'hi' if f == 0x10 else 'lo'};"
        if f in (0x11, 0x13):
            return f"ctx.{'hi' if f == 0x11 else 'lo'} = {rs};"
        if i.rd == 0:
            return ""
        if f == 0x00:
            return f"{rd} = {rt} << {i.sa}u;"
        if f == 0x02:
            if i.rs == 1:
                raise Refused("rotr")
            return f"{rd} = {rt} >> {i.sa}u;"
        if f == 0x03:
            return f"{rd} = sra({rt}, {i.sa}u);"
        if f == 0x04:
            return f"{rd} = {rt} << ({rs} & 31u);"
        if f == 0x06:
            if i.sa:
                raise Refused("rotrv")
            return f"{rd} = {rt} >> ({rs} & 31u);"
        if f == 0x07:
            return f"{rd} = sra({rt}, {rs} & 31u);"
        if f in (0x21, 0x25) and i.rt == 0:
            return f"{rd} = {rs};"
        if f in (0x21, 0x25) and i.rs == 0:
            return f"{rd} = {rt};"
        if f in _ALU:
            return f"{rd} = {_ALU[f].format(rs=rs, rt=rt)};"
        raise Refused(f"special {f:#x} at {i.pc:08X}")

    def special3(self, i: Insn, rs: str, rt: str, rd: str) -> str:
        f = i.funct
        if i.rt == 0 and f in (0x00, 0x04):
            return ""
        if f == 0x00:  # ext rt, rs, pos, size
            return f"{rt} = ext({rs}, {i.sa}u, {i.rd + 1}u);"
        if f == 0x04:  # ins rt, rs, pos, size
            return f"{rt} = ins({rt}, {rs}, {i.sa}u, {i.rd - i.sa + 1}u);"
        if f == 0x20 and i.rd and i.sa in (0x10, 0x18):
            return f"{rd} = sext({reg(i.rt)}, {8 if i.sa == 0x10 else 16}u);"
        raise Refused(f"special3 {f:#x} at {i.pc:08X}")

    # --- control ---------------------------------------------------------------------------------------------------
    @staticmethod
    def label(pc: int) -> str:
        return f"L_{pc:08X}"

    def control(self, i: Insn, delay: Insn) -> str:
        text = self.simple(delay)
        if i.op in (0x03,) or (i.op == 0x00 and i.funct == 0x09):
            self.known.clear()
        op = i.op
        rs, rt = reg(i.rs), reg(i.rt)
        if op == 0x03:
            return f"{text} call_here(rt, ctx, {hexa(self.calls[i.pc])});".lstrip()
        if op == 0x00 and i.funct == 0x09:
            if i.rd != 31:
                raise Refused("jalr with a link register other than ra")
            return f"{{ const std::uint32_t callee = {rs}; {text} call_here(rt, ctx, callee); }}"
        if op == 0x00:
            if i.rs != 31:
                raise Refused("jr to a register other than ra (jump table or tail jump)")
            self.released = False
            return f"{text} goto exit_;".lstrip()
        if op == 0x02:
            target = self.jumps[i.pc]
            self.released = False
            if self.address <= target < self.end:
                return f"{text} goto {self.label(target)};".lstrip()
            return f"{text} tail = {hexa(target)}; goto exit_;".lstrip()
        if op == 0x01:
            conditions = {0x00: "< 0", 0x01: ">= 0", 0x02: "< 0", 0x03: ">= 0"}
            if i.rt not in conditions:
                raise Refused(f"regimm branch {i.rt:#x} (linking)")
            condition, likely = f"static_cast<std::int32_t>({rs}) {conditions[i.rt]}", i.rt in (2, 3)
        elif op in (0x06, 0x07, 0x16, 0x17):
            condition = f"static_cast<std::int32_t>({rs}) {'<=' if op in (0x06, 0x16) else '>'} 0"
            likely = op in (0x16, 0x17)
        else:
            sign, likely = _BRANCH_IMM[op]
            if i.rs == i.rt:
                condition = "true" if sign == "==" else "false"
            else:
                condition = f"{rs} {sign} {rt}"
        target = self.label(branch_target(i))
        if likely:
            return f"if ({condition}) {{ {text} goto {target}; }}"
        if not text:
            return f"if ({condition}) goto {target};"
        return f"{{ const bool tk = {condition}; {text} if (tk) goto {target}; }}"

    # --- the function ----------------------------------------------------------------------------------------------
    def emit(self) -> str:
        self.scan()
        parts: list[str] = []
        n = 0
        while n < len(self.insns):
            i = self.insns[n]
            if i.pc in self.labels:
                self.released = False
                self.known.clear()
                parts.append(f"{self.label(i.pc)}: ;")
            if self.is_control(i):
                parts.append(self.control(i, self.insns[n + 1]))
                n += 2
                continue
            parts.append(self.simple(i))
            n += 1
        body = " ".join(p for p in parts if p)
        frame = f"GuestFrame frame(ctx, {hexa(self.frame_bytes)}); " if self.frame_bytes else ""
        return (f"void func_{self.address:08X}(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {{ "
                f"{{ std::uint32_t tail = 0u; {{ {{ {frame}{body} goto exit_; exit_: ; }} }} "
                f"if (tail != 0u) jump(ctx, tail); }} }}")


def draft(address: int) -> str:
    """The C++ draft of the function at `address`; `Refused` when it uses what the emitter does not model."""
    start, instructions, jumps, calls = parse_disasm(disassemble(address))
    return Emitter(start, instructions, jumps, calls).emit()


_HELPERS = {
    "sext": """std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}""",
    "high_half(": """// The `lui` half of an address whose low half is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }""",
    "ext(": """// `ext rt, rs, position, size`: the `size` bits of rs from `position`.
constexpr std::uint32_t ext(std::uint32_t value, unsigned position, unsigned size) {
    return (value >> position) & (size >= 32u ? 0xFFFFFFFFu : (1u << size) - 1u);
}""",
    "ins(": """// `ins rt, rs, position, size`: rt's `size` bits from `position` replaced by rs's low bits.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position, unsigned size) {
    const std::uint32_t mask = (size >= 32u ? 0xFFFFFFFFu : (1u << size) - 1u) << position;
    return (value & ~mask) | ((source << position) & mask);
}""",
    "sra(": """std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }""",
    "jump(": """void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }""",
    "divide(": """// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}""",
    "call_here(": """// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}""",
}


def generate(batch: int, functions: Sequence[str], intro: str) -> str:
    """The whole `hot_batch_N.cpp`: the helpers the bodies use, the bodies and the registrations."""
    body = "\n".join(functions)
    helpers = "\n".join(text for key, text in _HELPERS.items() if key in body)
    addresses = [re.match(r"void func_([0-9A-F]{8})", f)[1] for f in functions]
    lines = [
        f"// Hot batch {batch}: {intro}",
        "// drafted from `tools.disasm` with a register-level emitter (tools/batch_emit.py: one statement per instruction, labels and",
        "// gotos for branches), read against the asm and verified by synthetic cases.",
        "// No names: none has evidence.",
        '#include "decomp_registry.hpp"', '#include "gt/game/hot_common.hpp"', '#include "guest_call.hpp"', "",
        "#include <algorithm>", "#include <cstdint>", "#include <utility>", "",
        "namespace gt::game {", f"namespace hot_batch_{batch} {{", "namespace {", "", "using namespace gt::game::hot;", "",
        helpers, "", "} // namespace", "",
        "#define G ctx.gpr", "#define load8(address) rt.memory().load8(address)",
        "#define load16(address) rt.memory().load16(address)",
        "#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))",
        "#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))", "",
        *functions, "", "#undef G", "#undef load8", "#undef load16", "#undef store8", "#undef store16", "",
        f"}} // namespace hot_batch_{batch}", "} // namespace gt::game", "",
        f"#define HOT{batch}(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_{batch}::name, gt::ReturnKind::kind)",
        *[f"HOT{batch}(0x{a}u, func_{a}, Word);" for a in addresses], ""]
    return "\n".join(lines)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.batch_emit", description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    pool = sub.add_parser("pool")
    pool.add_argument("--min", type=int, default=60)
    pool.add_argument("--max", type=int, default=400)
    pool.add_argument("--limit", type=int, default=60)
    pool.add_argument("--src", type=Path, default=Path("src"))
    pool.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    emit = sub.add_parser("emit")
    emit.add_argument("addresses", nargs="+")
    gen = sub.add_parser("gen")
    gen.add_argument("batch", type=int)
    gen.add_argument("addresses", nargs="+")
    gen.add_argument("--intro", default="functions no route reaches")
    gen.add_argument("--out", type=Path)
    args = parser.parse_args(argv)
    if args.command == "gen":
        drafts = []
        for text in args.addresses:
            try:
                drafts.append(draft(int(text, 16)))
            except Refused as reason:
                print(f"// REFUSED {text}: {reason}", file=sys.stderr)
        source = generate(args.batch, drafts, args.intro)
        (args.out or Path(f"src/game/unsorted/hot_batch_{args.batch}.cpp")).write_text(source)
        return 0
    if args.command == "emit":
        status = 0
        for text in args.addresses:
            address = int(text, 16)
            try:
                print(draft(address))
            except Refused as reason:
                print(f"// REFUSED {address:08X}: {reason}", file=sys.stderr)
                status = 1
        return status
    done = done_addresses(args.src)
    found = 0
    for line in args.functions_tsv.read_text().splitlines():
        offset, size = line.split("\t")[:2]
        address, count = int(offset, 16) + LOAD_BASE, int(size) // 4
        if not args.min <= count <= args.max or address in done:
            continue
        try:
            draft(address)
        except Refused:
            continue
        print(f"{address:08X} {count}")
        found += 1
        if found >= args.limit:
            break
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
