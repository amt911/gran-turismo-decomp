"""Return-contract audit of the decompiled functions: registers a caller reads after the call that the
registration's ReturnKind leaves uncompared.

A differential case compares only the return registers its ReturnKind names (host/diff_compare.hpp). A
function registered `Void` whose original leaves a value in v0 that callers read passes its own cases and
still breaks its callers (docs/FINDINGS.md). Such a function can't run decompiled nested in other cases
(`GT_DECOMP_NESTED`) until its contract is fixed. This tool lists them, from two kinds of caller:

- original callers, in the recompiled code (recomp/generated): after the call returns, the block reads
  the register before writing it;
- decompiled callers, in src/: a `call_guest(...)` whose result is used, of a function registered `Void`.

Both scans are lower bounds: a read after a branch, or through an indirect call, isn't seen.

Usage (from the repository root): .venv/bin/python -m tools.contract_audit
"""

from __future__ import annotations

import re
import sys
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

# Return registers a case doesn't compare, per ReturnKind (gpr numbers: v0 = 2, v1 = 3).
UNCOMPARED = {"Void": (2, 3), "Word": (3,), "DoubleWord": (), "Float": (2, 3)}
_NAMES = {2: "v0", 3: "v1"}

_REGISTRATION = re.compile(
    r"^[ \t]*(?P<macro>[A-Z][A-Z0-9_]*)\(\s*0x(?P<address>[0-9A-Fa-f]{8})u?\s*,\s*(?P<name>[\w:<>]+)\s*,\s*"
    r"(?:gt::)?(?:ReturnKind::)?(?P<kind>Void|Word|DoubleWord|Float)\b", re.MULTILINE)
_CALL = re.compile(
    r"ctx\.gpr\[31\] = \(0x(?P<ret>[0-9A-Fa-f]{8})u\);\n"
    r"(?:[^\n]*\n){0,3}?"
    r"[ \t]*(?:ctx\.pc = 0x(?P<pc>[0-9A-Fa-f]{8})u;|goto L_(?P<goto>[0-9A-Fa-f]{8});|"
    r"if \(rt\.invoke_chained_direct<[^>]*0x(?P<chained>[0-9A-Fa-f]{8})u>)")
_LABEL = re.compile(r"^L_([0-9A-Fa-f]{8}):", re.MULTILINE)
_CONSTANT = re.compile(r"constexpr\s+std::uint32_t\s+(\w+)\s*=\s*0x([0-9A-Fa-f]{8})u")
_GUEST_CALL = re.compile(r"call_guest\(rt,\s*ctx,\s*(0x[0-9A-Fa-f]{8}u|\w+)")


@dataclass(frozen=True)
class Registration:
    name: str
    kind: str
    nested: bool


def registrations(source: str) -> dict[int, Registration]:
    """Active registrations in a source file: address -> (short name, ReturnKind, GT_DECOMP_NESTED)."""
    return {int(m["address"], 16): Registration(m["name"].split("::")[-1], m["kind"], m["macro"] == "GT_DECOMP_NESTED")
            for m in _REGISTRATION.finditer(source)}


def call_sites(unit: str) -> list[tuple[int, int]]:
    """(callee, return address) of each direct call in a generated unit."""
    return [(int(m["pc"] or m["goto"] or m["chained"], 16), int(m["ret"], 16)) for m in _CALL.finditer(unit)]


def reads_after(unit: str, return_address: int, reg: int) -> bool:
    """Whether the block at `return_address` reads gpr `reg` before writing it or leaving the block."""
    label = re.search(rf"^L_{return_address:08X}:\n", unit, re.MULTILINE)
    if label is None:
        return False
    use = re.compile(rf"ctx\.gpr\[{reg}\]")
    for line in unit[label.end():].split("\n"):
        code = line.split("//")[0].strip()
        if not code:
            continue
        if _LABEL.match(code) or code.startswith(("goto", "return", "if (", "}")):
            return False
        target, assign, value = code.partition(" = ")
        if assign and target == f"ctx.gpr[{reg}]":
            return bool(use.search(value))
        if use.search(code):
            return True
    return False


def original_readers(units: Mapping[str, str], regs: Mapping[int, Registration]) -> dict[int, list[tuple[str, int]]]:
    """Registered functions whose original callers read a register their ReturnKind leaves uncompared."""
    found: dict[int, list[tuple[str, int]]] = {}
    for unit in units.values():
        for callee, ret in call_sites(unit):
            reg = regs.get(callee)
            if reg is None:
                continue
            for r in UNCOMPARED[reg.kind]:
                if reads_after(unit, ret, r):
                    found.setdefault(callee, []).append((_NAMES[r], ret))
    return found


def decompiled_readers(sources: Mapping[str, str], regs: Mapping[int, Registration]) -> dict[int, list[str]]:
    """Functions registered Void whose result a decompiled caller uses (`x = call_guest(...)`)."""
    constants = {name: int(value, 16) for text in sources.values() for name, value in _CONSTANT.findall(text)}
    found: dict[int, list[str]] = {}
    for path, text in sources.items():
        for line in text.split("\n"):
            code = line.strip()
            if code.startswith("call_guest"):
                continue
            for m in _GUEST_CALL.finditer(code):
                if code[:m.start()].rstrip().endswith("(void)"):
                    continue  # result discarded
                token = m[1]
                address = int(token[:-1], 16) if token.startswith("0x") else constants.get(token)
                reg = regs.get(address) if address is not None else None
                if reg is not None and reg.kind == "Void":
                    found.setdefault(address, []).append(f"{path}: {code}")
    return found


def main(argv: list[str] | None = None) -> int:  # pragma: no cover - reads the tree
    src = Path("src")
    sources = {str(p): p.read_text() for p in sorted(src.rglob("*.cpp")) + sorted(src.rglob("*.hpp"))}
    regs: dict[int, Registration] = {}
    for text in sources.values():
        regs.update(registrations(text))
    units = {str(p): p.read_text() for p in sorted(Path("recomp/generated").glob("generated_unit_*.cpp"))}
    if not units:
        print("recomp/generated is empty: run `.venv/bin/python -m tools.recomp` first", file=sys.stderr)
        return 2
    original = original_readers(units, regs)
    decompiled = decompiled_readers(sources, regs)
    flagged = sorted(set(original) | set(decompiled))
    for address in flagged:
        reg = regs[address]
        print(f"0x{address:08X} {reg.kind:10s} {'NESTED ' if reg.nested else ''}{reg.name}")
        for name, ret in original.get(address, [])[:3]:
            print(f"    original caller reads {name} after the call (returns to 0x{ret:08X})")
        for use in decompiled.get(address, [])[:3]:
            print(f"    decompiled caller uses the result: {use}")
    nested_flagged = [a for a in flagged if regs[a].nested]
    print(f"{len(flagged)} of {len(regs)} registrations have a caller reading an uncompared return register; "
          f"{len(nested_flagged)} of them are GT_DECOMP_NESTED")
    return 1 if nested_flagged else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
