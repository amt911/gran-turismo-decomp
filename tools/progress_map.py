"""Decompilation progress map: the executable's code as a grid of cells, each coloured by how much of it
is decompiled — the picture of what is left, linked from the README.

Usage: .venv/bin/python -m tools.progress_map [--functions recomp/analysis/functions.tsv] [--src src]
                                              [--out docs/progress_map.svg]

Each cell covers CELL_SIZE bytes of .text; its colour is the decompiled share of the bytes of the
functions in it (grey = none, full colour = all). Cells from SDK_START on are the statically linked
SDK and libraries (libc, GU, sound...), drawn in blue — the boundary is approximate (the first libc
function), the counts in tools.progress classify by source directory instead. The SVG holds only
addresses and percentages, nothing of the game.
"""

from __future__ import annotations

import argparse
from collections.abc import Mapping, Sequence
from pathlib import Path

from tools.progress import LOAD_BASE, decompiled_functions, load_functions

TEXT_START = 0x28        # .text, ELF vaddrs (docs/FACTS.md)
TEXT_END = 0x4767DC
SDK_START = 0x330000     # about where libc starts (gt::libc::page_heap at 0x08B344E0)
CELL_SIZE = 0x800        # 2 KiB per cell
COLUMNS = 64
CELL_PIXELS = 12

_EMPTY = "#f2f2f2"
_NONE = (0xD0, 0xD0, 0xD0)
_GAME = (0x2E, 0x9E, 0x44)
_SDK = (0x2F, 0x6D, 0xB5)


def cell_coverage(functions: Mapping[int, int], decompiled: set[int], *, start: int, end: int,
                  cell_size: int) -> list[float | None]:
    """Per cell of [start, end): decompiled function bytes / function bytes (None: no function)."""
    count = (end - start + cell_size - 1) // cell_size
    total = [0] * count
    done = [0] * count
    for entry, size in functions.items():
        first, last = max(entry, start), min(entry + size, end)
        while first < last:
            cell = (first - start) // cell_size
            cell_end = min(start + (cell + 1) * cell_size, last)
            total[cell] += cell_end - first
            if entry in decompiled:
                done[cell] += cell_end - first
            first = cell_end
    return [done[i] / total[i] if total[i] else None for i in range(count)]


def colour(fraction: float | None, *, sdk: bool) -> str:
    """Grey for nothing decompiled, the region's colour for everything, a blend in between."""
    if fraction is None:
        return _EMPTY
    full = _SDK if sdk else _GAME
    mixed = (round(n + (f - n) * fraction) for n, f in zip(_NONE, full))
    return "#" + "".join(f"{c:02x}" for c in mixed)


def render_svg(cells: Sequence[float | None], *, columns: int, sdk_from: int, summary: str) -> str:
    rows = (len(cells) + columns - 1) // columns
    width = columns * CELL_PIXELS
    top = 44
    height = top + rows * CELL_PIXELS + 34
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}" font-family="sans-serif" font-size="12">',
        f'<rect width="{width}" height="{height}" fill="#ffffff"/>',
        f'<text x="0" y="16" font-weight="bold">Gran Turismo (PSP) — decompilation map</text>',
        f'<text x="0" y="34">{summary}</text>',
    ]
    for i, fraction in enumerate(cells):
        x = (i % columns) * CELL_PIXELS
        y = top + (i // columns) * CELL_PIXELS
        parts.append(f'<rect class="cell" x="{x}" y="{y}" width="{CELL_PIXELS - 1}" height="{CELL_PIXELS - 1}" '
                     f'fill="{colour(fraction, sdk=i >= sdk_from)}"/>')
    legend_y = top + rows * CELL_PIXELS + 20
    legend = [("game, decompiled", colour(1.0, sdk=False)), ("SDK / libraries, decompiled", colour(1.0, sdk=True)),
              ("not decompiled", colour(0.0, sdk=False)), ("no code", _EMPTY)]
    x = 0
    for label, fill in legend:
        parts.append(f'<rect x="{x}" y="{legend_y - 10}" width="10" height="10" fill="{fill}"/>')
        parts.append(f'<text x="{x + 14}" y="{legend_y}">{label}</text>')
        x += 14 + 8 * len(label) + 16
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    parser.add_argument("--src", type=Path, default=Path("src"))
    parser.add_argument("--out", type=Path, default=Path("docs/progress_map.svg"))
    args = parser.parse_args(argv)
    functions = load_functions(args.functions)
    decompiled = {entry for entry in decompiled_functions(args.src) if entry in functions}
    cells = cell_coverage(functions, decompiled, start=TEXT_START, end=TEXT_END, cell_size=CELL_SIZE)
    total_bytes = sum(functions.values())
    done_bytes = sum(functions[e] for e in decompiled)
    summary = (f"{len(decompiled)} / {len(functions)} functions ({100 * len(decompiled) / len(functions):.2f} %) · "
               f"{done_bytes} / {total_bytes} bytes ({100 * done_bytes / total_bytes:.2f} %) · "
               f"one cell = {CELL_SIZE // 1024} KiB of code at runtime 0x{LOAD_BASE + TEXT_START:08X}…")
    sdk_from = (SDK_START - TEXT_START) // CELL_SIZE
    args.out.write_text(render_svg(cells, columns=COLUMNS, sdk_from=sdk_from, summary=summary))
    print(f"wrote {args.out} ({len(cells)} cells)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
