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


_CAST = re.compile(r"(?P<head>static_cast<std::u?int32_t>\()(?P<body>[^()]*)\)")


def _parse_literal(text: str) -> int | None:
    cast = _CAST.fullmatch(text.strip())
    match = re.fullmatch(r"(0x[0-9A-Fa-f]+|\d+)u?", cast.group("body") if cast else text.strip())
    return int(match.group(1), 0) & _M if match else None


def _format_like(model: str, value: int) -> str:
    model = model.strip()
    cast = _CAST.fullmatch(model)
    if cast:  # a cast around a number keeps its cast
        return f"{cast.group('head')}{_format_like(cast.group('body'), value)})"
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
    named = [a.strip() for a in args if _parse_literal(a) is None and a.strip() not in ("true", "false")]
    if named:
        return [], [f"{template.strip()} names a function: one C++ body with its own constants, not a family macro"]
    plan: list[int | None] = []
    also: list[int] = []  # per argument: the value position it comes from, or None for a constant
    for arg in args:
        literal = _parse_literal(arg)
        places = [] if literal is None else [i for i, v in enumerate(template_values) if v & _M == literal]
        if len(places) > 1:  # the one place where members differ carries it; none differing: any place is the same
            varying = [i for i in places if any(len(v) == len(template_values) and v[i] & _M != literal
                                                for v in members.values())]
            agree = all(len({v[i] & _M for i in varying}) == 1 for v in members.values() if len(v) == len(template_values))
            if len(varying) > 1 and not agree:
                return [], [f"argument {arg.strip()} is ambiguous: the template has it at positions {places}, "
                            f"and members differ at {varying}"]
            places = varying or places[:1]
        plan.append(places[0] if places else None)
        also.extend(places[1:])  # agreeing positions of the same argument: carried by it too
    rows, problems = [], []
    for address, values in sorted(members.items()):
        if len(values) != len(template_values):
            problems.append(f"0x{address:08X}: {len(values)} values, the template has {len(template_values)}")
            continue
        carried = {place for place in plan if place is not None} | set(also)
        differs = [i for i, (v, t) in enumerate(zip(values, template_values)) if i not in carried and v & _M != t & _M]
        if differs:
            problems.append(f"0x{address:08X}: differs from the template at value position {differs[0]}, which no argument "
                            f"carries (its row would run the template's constant there)")
            continue
        out = [_format_like(arg, values[place]) if place is not None else arg.strip() for arg, place in zip(args, plan)]
        rows.append(f"{match.group('macro')}(0x{address:08X}u{''.join(', ' + a for a in out)});")
    return rows, problems


_WRAPPER = re.compile(r"^void func_(?P<entry>[0-9A-Fa-f]{8})\((?P<params>[^)]*)\)\s*\{\s*(?P<wrapper>\w+)\(rt, ctx"
                      r"(?P<args>(?:,[^;]*)?)\);\s*\}\s*$")


def wrapper_rows(template: str, registration: str, template_values: Sequence[int],
                 members: Mapping[int, Sequence[int]]) -> tuple[list[str], list[str], list[str]]:
    """The members' one-line wrapper rows and registrations, mapped like macro_rows from the template's own row."""
    match = _WRAPPER.match(template.strip())
    if not match:
        return [], [], [f"not a one-line wrapper row: {template.strip()}"]
    entry = match.group("entry").upper()
    as_macro = f"W(0x{entry}u{match.group('args')});"
    rows, problems = macro_rows(as_macro, template_values, members)
    out_rows, out_regs = [], []
    for row in rows:
        address = row[2:12]  # "W(0xADDRESSu..."
        member = address[2:].upper()
        args = row[row.index("u", 3) + 1:-2]
        out_rows.append(f"void func_{member}({match.group('params')}) {{ {match.group('wrapper')}(rt, ctx{args}); }}")
        out_regs.append(registration.replace(f"0x{entry}u", f"0x{member}u").replace(f"func_{entry}", f"func_{member}"))
    return out_rows, out_regs, problems


def wrapper_template(source: str, entry: int) -> tuple[str, str]:
    """The template's one-line wrapper row and its (active) registration line in `source`."""
    name = f"func_{entry:08X}"
    rows = [line.strip() for line in source.splitlines() if (m := _WRAPPER.match(line.strip())) and m.group("entry").upper() == f"{entry:08X}"]
    regs = [line.strip() for line in source.splitlines()
            if f"0x{entry:08X}u" in line and name in line and not line.strip().startswith(("//", "void "))]
    if len(rows) != 1 or len(regs) != 1:
        raise ValueError(f"{name}: {len(rows)} wrapper rows and {len(regs)} registrations")
    return rows[0], regs[0]


def pick_template(source: str, entry: int) -> tuple[str, str | tuple[str, str]]:
    """("wrapper", (row, registration)) when `entry` has a one-line wrapper row, else ("macro", row) — a wrapper's own
    registration (GT_DECOMP(0x...u, gt::...::func_..., ...)) must never be read as a macro row."""
    try:
        return "wrapper", wrapper_template(source, entry)
    except ValueError:
        row = template_row(source, entry)
        if row is None:
            raise ValueError(f"0x{entry:08X}: no wrapper row and no macro registration") from None
        return "macro", row


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
    try:
        kind, found = pick_template(args.file.read_text(), runtime)
    except ValueError as error:
        print(f"{args.file}: {error}", file=sys.stderr)
        return 2
    wrapper = found if kind == "wrapper" else None
    row = found if kind == "macro" else None
    image = fm.TextImage.from_elf(Path("extracted/EBOOT.elf").read_bytes())
    sizes = load_functions(Path("recomp/analysis/functions.tsv"))
    inside, _ = split_inside(sizes, image.base, len(image.text))
    emitted = recompiled_functions(u.read_text() for u in sorted(Path("recomp/generated").glob("generated_unit_*.cpp")))
    scope = {a: n for a, n in inside.items() if a in emitted}
    code = {a: image.text[a - image.base:a - image.base + n] for a, n in scope.items()}
    template = runtime - LOAD_BASE

    def decoded(address: int) -> tuple[list[int], list]:
        words, relocs = image.function(address, sizes[address])
        return words, fm.member_values(words, address, relocs, image.segments)

    def values(address: int) -> list[int]:
        return [v.value for v in decoded(address)[1]]

    t_words = decoded(template)[0]
    members = fm.find_members(code, template, None, set(done_functions(Path("src"))))
    member_values = {}
    for address in members:
        words, decoded_values = decoded(address)
        left = fm.uncovered(t_words, words, decoded_values, template, address)
        if left:  # a register or an offset differs that no value carries: not the template with other constants
            print(f"!! 0x{address + LOAD_BASE:08X}: words {left} differ from the template with no value explaining them")
            continue
        member_values[address + LOAD_BASE] = [v.value for v in decoded_values]
    if wrapper is not None:
        rows, regs, problems = wrapper_rows(wrapper[0], wrapper[1], values(template), member_values)
        rows = rows + [""] + regs
    else:
        rows, problems = macro_rows(row, values(template), member_values)
    for problem in problems:
        print(f"!! {problem}")
    print("\n".join(rows))
    return 1 if problems and not rows else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
