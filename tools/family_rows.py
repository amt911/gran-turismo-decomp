"""Registration rows for the pending members of a macro family, mapped from the template's own row.

`tools.family_members` decodes each member's values (constants, addresses, call targets) in word order. A family whose
template is registered with a macro (`GT_DECOMP_FORWARD(0x0885E314u, 0, 0x088669B4u);`) gets one row per member: each
argument of the template's row is the value the template has at some word position, and the member's argument is its
own value at that position — so a macro whose arguments are in another order than the words (FORWARD: target, then
offset in the words) still gets its rows right. An argument no value explains is a constant of the family and is
copied. A template value found at two positions, or a member whose values do not line up, is refused, never guessed.

Usage (from the repository root; reads what tools.family_members reads):
  .venv/bin/python -m tools.family_rows 0x0885E314 --file src/core/accessors.cpp
prints the rows (and a `!!` line per refused member) for the file's batch section; the gate (synthetic cases) and the
sabotage then decide whether each row is right.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path

_ROW = re.compile(r"^(?P<macro>[A-Z_][A-Z0-9_]*)\((?P<entry>0x[0-9A-Fa-f]{8})u(?P<args>(?:,[^;]*)?)\);\s*$")
_M = 0xFFFFFFFF


def _parse_literal(text: str) -> int | None:
    match = re.fullmatch(r"(0x[0-9A-Fa-f]+|\d+)u?", text.strip())
    return int(match.group(1), 0) & _M if match else None


def _format_like(model: str, value: int) -> str:
    model = model.strip()
    suffix = "u" if model.endswith("u") else ""
    body = model[:-1] if suffix else model
    if body.lower().startswith("0x"):
        digits = len(body) - 2
        return f"0x{value:0{digits}X}{suffix}" if digits == 8 else f"0x{value:X}{suffix}"
    return f"{value}{suffix}"


def macro_rows(template: str, template_values: Sequence[int],
               members: Mapping[int, Sequence[int]]) -> tuple[list[str], list[str]]:
    """The members' rows of `template` (a registration line), or the problems that stop them."""
    match = _ROW.match(template.strip())
    if not match:
        return [], [f"not a one-line macro registration: {template.strip()}"]
    args = [a for a in match.group("args").split(",")[1:]] if match.group("args") else []
    plan: list[int | None] = []  # per argument: the value position it comes from, or None for a constant
    for arg in args:
        literal = _parse_literal(arg)
        places = [] if literal is None else [i for i, v in enumerate(template_values) if v & _M == literal]
        if len(places) > 1:
            return [], [f"argument {arg.strip()} is ambiguous: the template has it at positions {places}"]
        plan.append(places[0] if places else None)
    rows, problems = [], []
    for address, values in sorted(members.items()):
        if len(values) != len(template_values):
            problems.append(f"0x{address:08X}: {len(values)} values, the template has {len(template_values)}")
            continue
        out = [_format_like(arg, values[place]) if place is not None else arg.strip() for arg, place in zip(args, plan)]
        rows.append(f"{match.group('macro')}(0x{address:08X}u{''.join(', ' + a for a in out)});")
    return rows, problems


def template_row(source: str, entry: int) -> str | None:
    """The active (not commented) one-line registration of `entry` in `source`."""
    found = [line.strip() for line in source.splitlines()
             if (m := _ROW.match(line.strip())) and int(m.group("entry"), 16) == entry]
    if len(found) > 1:
        raise ValueError(f"0x{entry:08X} is registered {len(found)} times")
    return found[0] if found else None


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - reads the ELF and the tree
    from tools import family_members as fm
    from tools.progress import LOAD_BASE, load_functions
    from tools.shapes import done_functions, recompiled_functions, split_inside

    parser = argparse.ArgumentParser(prog="tools.family_rows", description=__doc__.splitlines()[0])
    parser.add_argument("template", help="runtime address of the registered template")
    parser.add_argument("--file", type=Path, required=True, help="the source that registers the template")
    args = parser.parse_args(argv)
    runtime = int(args.template, 16)
    row = template_row(args.file.read_text(), runtime)
    if row is None:
        print(f"no active registration of 0x{runtime:08X} in {args.file}", file=sys.stderr)
        return 2
    image = fm.TextImage.from_elf(Path("extracted/EBOOT.elf").read_bytes())
    sizes = load_functions(Path("recomp/analysis/functions.tsv"))
    inside, _ = split_inside(sizes, image.base, len(image.text))
    emitted = recompiled_functions(u.read_text() for u in sorted(Path("recomp/generated").glob("generated_unit_*.cpp")))
    scope = {a: n for a, n in inside.items() if a in emitted}
    code = {a: image.text[a - image.base:a - image.base + n] for a, n in scope.items()}
    template = runtime - LOAD_BASE

    def values(address: int) -> list[int]:
        words, relocs = image.function(address, sizes[address])
        return [v.value for v in fm.member_values(words, address, relocs, image.segments)]

    members = fm.find_members(code, template, None, set(done_functions(Path("src"))))
    rows, problems = macro_rows(row, values(template), {a + LOAD_BASE: values(a) for a in members})
    for problem in problems:
        print(f"!! {problem}")
    print("\n".join(rows))
    return 1 if problems and not rows else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
