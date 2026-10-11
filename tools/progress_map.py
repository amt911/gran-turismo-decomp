"""Decompilation progress map: every function of the executable as a rectangle sized by its bytes,
coloured by its state — the picture of what is left, for the README and an interactive page.

Usage: .venv/bin/python -m tools.progress_map [--functions recomp/analysis/functions.tsv] [--src src]
           [--symbols config/symbol_addrs.txt] [--out docs] [--readme README.md] [--fragment FILE]

Writes <out>/progress_map.svg (static treemap for the README), <out>/progress/index.html (the same map
with hover details and search) and the progress bars between the README's
<!-- progress:start --> / <!-- progress:end --> markers. --fragment also writes the page without its
<!doctype>/<html> wrapper (for hosts that add their own).

Functions are grouped by address: game code in 256 KiB regions, then the statically linked SDK and
libraries (from SDK_START, approximate: the first libc function). A function is "hand" when a plain
GT_DECOMP registers it, "template" when a GT_DECOMP_<KIND> instance does (tools.progress counts both).
Only addresses, sizes, our own symbol names and states go into the outputs — nothing of the game.
"""

from __future__ import annotations

import argparse
import html
import json
import re
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

from tools.progress import LOAD_BASE, load_functions

SDK_START = 0x330000          # ELF vaddr; about where libc starts (gt::libc::page_heap at 0x08B344E0)
REGION = 0x40000              # game code grouped in 256 KiB regions
MARK_START = "<!-- progress:start -->"
MARK_END = "<!-- progress:end -->"

_REGISTRATION = re.compile(r"^[ \t]*GT_DECOMP(_[A-Z0-9_]+)?\(\s*0x([0-9A-Fa-f]+)u?\s*[,)]", re.MULTILINE)
_SYMBOL = re.compile(r"^([^/\s][^=]*?) = 0x([0-9A-Fa-f]+);", re.MULTILINE)
_STATE_LABEL = {"hand": "decompilada a mano", "template": "decompilada (plantilla)", None: "pendiente"}


@dataclass(frozen=True)
class Function:
    entry: int            # ELF vaddr
    size: int
    state: str | None     # "hand", "template" or None (pending)
    name: str | None


@dataclass
class Group:
    label: str
    functions: list[Function] = field(default_factory=list)

    @property
    def size(self) -> int:
        return sum(f.size for f in self.functions)

    @property
    def done(self) -> int:
        return sum(f.size for f in self.functions if f.state)


# --- data ---------------------------------------------------------------------------------------

def classify(src: Path) -> dict[int, str]:
    """ELF vaddr -> "hand" | "template" for every active registration under `src`."""
    states: dict[int, str] = {}
    for path in sorted(src.rglob("*.cpp")):
        for match in _REGISTRATION.finditer(path.read_text()):
            states[int(match.group(2), 16) - LOAD_BASE] = "template" if match.group(1) else "hand"
    return states


def load_symbols(path: Path) -> dict[int, str]:
    if not path.exists():
        return {}
    return {int(m.group(2), 16) - LOAD_BASE: m.group(1).strip() for m in _SYMBOL.finditer(path.read_text())}


def group_functions(functions: Sequence[Function], *, region: int = REGION, sdk_start: int = SDK_START) -> list[Group]:
    groups: dict[str, Group] = {}
    for function in sorted(functions, key=lambda f: f.entry):
        label = "SDK" if function.entry >= sdk_start else f"0x{LOAD_BASE + function.entry // region * region:08X}"
        groups.setdefault(label, Group(label)).functions.append(function)
    return list(groups.values())


# --- layout -------------------------------------------------------------------------------------

def _worst(row: Sequence[float], side: float) -> float:
    total = sum(row)
    return max(max(side * side * r / (total * total), total * total / (side * side * r)) for r in row)


def squarify(sizes: Sequence[float], x: float, y: float, w: float, h: float) -> list[tuple[float, float, float, float]]:
    """Squarified treemap (Bruls, Huizing, van Wijk): one rectangle per size, in order, filling the box
    with areas proportional to the sizes and aspect ratios kept near 1."""
    total = sum(sizes)
    if total <= 0 or w <= 0 or h <= 0:
        return [(x, y, 0.0, 0.0) for _ in sizes]
    scale = w * h / total
    areas = [s * scale for s in sizes]
    rects: list[tuple[float, float, float, float]] = []
    i = 0
    while i < len(areas):
        side = min(w, h)
        row = [areas[i]]
        i += 1
        while i < len(areas) and _worst(row + [areas[i]], side) <= _worst(row, side):
            row.append(areas[i])
            i += 1
        row_total = sum(row)
        if w >= h:  # lay the row out as a column on the left
            column = row_total / h if h else 0.0
            offset = y
            for a in row:
                height = a / column if column else 0.0
                rects.append((x, offset, column, height))
                offset += height
            x, w = x + column, w - column
        else:       # a row along the top
            line = row_total / w if w else 0.0
            offset = x
            for a in row:
                width = a / line if line else 0.0
                rects.append((offset, y, width, line))
                offset += width
            y, h = y + line, h - line
    return rects


def layout(groups: Sequence[Group], width: float, height: float, header: float = 14.0):
    """[(group, (x, y, w, h), [(function, (x, y, w, h)), ...]), ...]"""
    placed = []
    order = sorted(groups, key=lambda g: g.size, reverse=True)
    for group, box in zip(order, squarify([g.size for g in order], 0, 0, width, height)):
        gx, gy, gw, gh = box
        top = header if gh > 3 * header else 0.0
        functions = sorted(group.functions, key=lambda f: f.size, reverse=True)
        inner = squarify([f.size for f in functions], gx + 1, gy + top, max(gw - 2, 0), max(gh - top - 1, 0))
        placed.append((group, box, list(zip(functions, inner))))
    return placed


# --- text ---------------------------------------------------------------------------------------

def _bar(fraction: float, width: int = 40) -> str:
    filled = round(fraction * width)
    return "█" * filled + "░" * (width - filled)


def bars(functions: Sequence[Function]) -> str:
    total_n = len(functions)
    total_b = sum(f.size for f in functions)
    done_n = sum(1 for f in functions if f.state)
    done_b = sum(f.size for f in functions if f.state)
    named = sum(1 for f in functions if f.name)
    rows = [("FUNCIONES", done_n, total_n), ("BYTES", done_b, total_b), ("CON NOMBRE", named, total_n)]
    lines = [f"{label:<11} {_bar(n / d if d else 0.0)} {100 * n / d if d else 0.0:6.2f} %  {n} / {d}"
             for label, n, d in rows]
    return "```text\n" + "\n".join(lines) + "\n```"


def replace_between_markers(text: str, block: str) -> str:
    start, end = text.find(MARK_START), text.find(MARK_END)
    if start < 0 or end < start:
        raise ValueError(f"the markers {MARK_START} / {MARK_END} are missing")
    return text[: start + len(MARK_START)] + "\n" + block + "\n" + text[end:]


# --- rendering ----------------------------------------------------------------------------------

_FILL = {"hand": "#3fa65a", "template": "#2a9d9d", None: "#4a5260"}


def render_svg(groups: Sequence[Group], *, width: int = 1200, height: int = 720) -> str:
    functions = [f for g in groups for f in g.functions]
    done = sum(1 for f in functions if f.state)
    done_b = sum(f.size for f in functions if f.state)
    total_b = sum(f.size for f in functions) or 1
    top = 40
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height + top}" '
        f'viewBox="0 0 {width} {height + top}" font-family="sans-serif">',
        f'<rect width="{width}" height="{height + top}" fill="#1d2129"/>',
        '<text x="6" y="16" fill="#e6e8ec" font-size="13" font-weight="bold">'
        'Gran Turismo (PSP) — progreso de la decompilación</text>',
        f'<text x="6" y="32" fill="#a9b0bc" font-size="10">Funciones: {done} / {len(functions)} '
        f'({100 * done / max(len(functions), 1):.2f} %) · bytes: {done_b} / {total_b} ({100 * done_b / total_b:.2f} %) · '
        'cada rectángulo es una función, por tamaño en bytes · verde = a mano, turquesa = plantilla, '
        'gris = pendiente</text>',
        f'<g transform="translate(0,{top})">',
    ]
    for group, (gx, gy, gw, gh), placed in layout(groups, width, height):
        parts.append(f'<rect x="{gx:.1f}" y="{gy:.1f}" width="{gw:.1f}" height="{gh:.1f}" fill="#262b35" '
                     'stroke="#12151b" stroke-width="1"/>')
        for function, (x, y, w, h) in placed:
            parts.append(f'<rect class="f" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" '
                         f'fill="{_FILL[function.state]}"/>')
        if gh > 42:
            share = 100 * group.done / group.size if group.size else 0.0
            label = "SDK y librerías" if group.label == "SDK" else group.label
            parts.append(f'<text x="{gx + 3:.1f}" y="{gy + 10:.1f}" fill="#e6e8ec" font-size="9">'
                         f'{label} {share:.1f} %</text>')
    parts.append("</g></svg>")
    return "\n".join(parts) + "\n"


_PAGE = """<title>GT decomp map</title>
<meta name="description" content="Gran Turismo (PSP): every function of the executable, by size, and how much is decompiled.">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@500;700&family=JetBrains+Mono:wght@400;600&display=swap">
<style>
/* Layout: a control strip (title, bars, legend, search) over one full-width treemap canvas. */
:root {
  --bg: #f3f4f6; --panel: #ffffff; --fg: #1b1f27; --muted: #5d6573; --line: #d5d9e0;
  --hand: #2f9a4c; --template: #23918f; --todo: #b9bec8; --frame: #8d94a1; --hi: #e0a300;
  --display: "Barlow Semi Condensed", "Arial Narrow", system-ui, sans-serif;
  --mono: "JetBrains Mono", ui-monospace, "SFMono-Regular", Menlo, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #15181e; --panel: #1d2129; --fg: #e6e8ec; --muted: #9aa2af; --line: #2c323d;
  --hand: #3fa65a; --template: #2a9d9d; --todo: #454c59; --frame: #0f1217; --hi: #ffc531; color-scheme: dark } }
:root[data-theme="dark"] {
  --bg: #15181e; --panel: #1d2129; --fg: #e6e8ec; --muted: #9aa2af; --line: #2c323d;
  --hand: #3fa65a; --template: #2a9d9d; --todo: #454c59; --frame: #0f1217; --hi: #ffc531; color-scheme: dark }
body { background: var(--bg); color: var(--fg); font-family: var(--display); }
main { padding-inline: 16px; padding-block: 16px 24px; max-width: 1400px; margin: 0 auto; display: grid; gap: 14px; }
h1 { font-size: 1.6rem; font-weight: 700; margin: 0; letter-spacing: .01em; text-wrap: balance; }
.sub { color: var(--muted); margin: 2px 0 0; font-size: .95rem; }
.strip { display: flex; flex-wrap: wrap; gap: 12px 28px; align-items: end; }
.bars { font-family: var(--mono); font-size: .78rem; display: grid; gap: 4px; min-width: 0; }
.bar { display: grid; grid-template-columns: 7.5em minmax(80px, 260px) auto; gap: 10px; align-items: center; }
.bar span:first-child { letter-spacing: .06em; color: var(--muted); }
.track { height: 9px; background: var(--todo); border-radius: 2px; overflow: hidden; }
.track i { display: block; height: 100%; background: var(--hand); }
.bar b { font-weight: 600; font-variant-numeric: tabular-nums; white-space: nowrap; }
.legend { display: flex; flex-wrap: wrap; gap: 6px 16px; font-size: .9rem; color: var(--muted); }
.legend span { display: inline-flex; align-items: center; gap: 6px; }
.sw { width: 12px; height: 12px; border-radius: 2px; display: inline-block; }
label { font-size: .85rem; color: var(--muted); display: grid; gap: 4px; }
input { font: inherit; font-family: var(--mono); font-size: .85rem; padding: 6px 8px; border: 1px solid var(--line);
  border-radius: 4px; background: var(--panel); color: var(--fg); width: min(260px, 70vw); }
input:focus-visible { outline: 2px solid var(--hi); outline-offset: 1px; }
.map { position: relative; background: var(--frame); border-radius: 4px; overflow: hidden; }
canvas { display: block; width: 100%; touch-action: manipulation; }
#tip { position: absolute; pointer-events: none; background: var(--panel); color: var(--fg); border: 1px solid var(--line);
  border-radius: 4px; padding: 8px 10px; font-family: var(--mono); font-size: .75rem; line-height: 1.5;
  box-shadow: 0 4px 14px rgb(0 0 0 / .25); max-width: min(340px, 80vw); overflow-wrap: anywhere; }
#tip strong { font-family: var(--display); font-size: .95rem; }
.note { color: var(--muted); font-size: .85rem; max-width: 70ch; margin: 0; }
</style>
<main>
  <header>
    <h1>Gran Turismo (PSP) · mapa de la decompilación</h1>
    <p class="sub">UCES-01245 v2.00 · ejecutable PDIAPP · cada rectángulo es una función, con área proporcional a sus bytes</p>
  </header>
  <div class="strip">
    <div class="bars" id="bars"></div>
    <div class="legend">
      <span><i class="sw" style="background:var(--hand)"></i>decompilada a mano</span>
      <span><i class="sw" style="background:var(--template)"></i>decompilada (plantilla)</span>
      <span><i class="sw" style="background:var(--todo)"></i>pendiente</span>
    </div>
    <label for="q">Buscar función o dirección
      <input id="q" type="search" placeholder="crc32, 0x08A76F20…" autocomplete="off">
    </label>
  </div>
  <div class="map" id="map"><canvas id="c"></canvas><div id="tip" hidden></div></div>
  <p class="note">Grupos: el código del juego en tramos de 256 KiB por dirección, y aparte el SDK y las librerías de
  Sony enlazadas estáticamente (frontera aproximada). Cada función decompilada se ha comparado con la original en el
  harness diferencial. Generado por <code>tools/progress_map.py</code>; solo contiene direcciones, tamaños y nombres
  propios del proyecto.</p>
</main>
<script>
const DATA = __DATA__;
(() => {
  const canvas = document.getElementById("c"), map = document.getElementById("map"), tip = document.getElementById("tip");
  const q = document.getElementById("q"), ctx = canvas.getContext("2d");
  const F = DATA.functions, G = DATA.groups, W = DATA.width, H = DATA.height;
  const names = DATA.names, states = ["pendiente", "decompilada a mano", "decompilada (plantilla)"];
  let scale = 1, hover = -1, matches = null;
  const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
  const hex = a => "0x" + (a >>> 0).toString(16).toUpperCase().padStart(8, "0");
  document.getElementById("bars").innerHTML = DATA.bars.map(([label, n, d]) =>
    `<div class="bar"><span>${label}</span><span class="track"><i style="width:${(100 * n / d).toFixed(2)}%"></i></span>` +
    `<b>${(100 * n / d).toFixed(2)} % · ${n.toLocaleString("es")} / ${d.toLocaleString("es")}</b></div>`).join("");
  function draw() {
    const colors = [css("--todo"), css("--hand"), css("--template")], frame = css("--frame"), hi = css("--hi");
    const fg = css("--fg"), dpr = window.devicePixelRatio || 1, cw = map.clientWidth;
    scale = cw / W;
    canvas.width = Math.round(cw * dpr); canvas.height = Math.round(H * scale * dpr);
    canvas.style.height = (H * scale) + "px";
    ctx.setTransform(dpr * scale, 0, 0, dpr * scale, 0, 0);
    ctx.fillStyle = frame; ctx.fillRect(0, 0, W, H);
    const gap = 0.6 / scale;
    for (let i = 0; i < F.length; i++) {
      const f = F[i]; if (f[2] < 0.05 || f[3] < 0.05) continue;
      ctx.fillStyle = matches && !matches.has(i) ? frame : colors[f[4]];
      ctx.fillRect(f[0], f[1], Math.max(f[2] - gap, 0.05), Math.max(f[3] - gap, 0.05));
    }
    if (matches) { ctx.strokeStyle = hi; ctx.lineWidth = 1.5 / scale;
      for (const i of matches) { const f = F[i]; ctx.strokeRect(f[0], f[1], Math.max(f[2], 2 / scale), Math.max(f[3], 2 / scale)); } }
    ctx.font = `600 ${10 / scale}px ${css("--mono")}`; ctx.textBaseline = "top";
    for (const g of G) { if (g.h * scale < 40 || g.w * scale < 80) continue;
      ctx.fillStyle = frame; ctx.fillRect(g.x, g.y, g.w, 13 / scale + 1);
      ctx.fillStyle = fg; ctx.fillText(`${g.label} · ${g.share.toFixed(1)} %`, g.x + 3 / scale, g.y + 2 / scale); }
    if (hover >= 0) { const f = F[hover]; ctx.strokeStyle = hi; ctx.lineWidth = 2 / scale;
      ctx.strokeRect(f[0], f[1], Math.max(f[2], 2 / scale), Math.max(f[3], 2 / scale)); }
  }
  function find(px, py) {
    const x = px / scale, y = py / scale;
    for (const g of G) if (x >= g.x && x < g.x + g.w && y >= g.y && y < g.y + g.h)
      for (let i = g.first; i < g.last; i++) { const f = F[i];
        if (x >= f[0] && x < f[0] + f[2] && y >= f[1] && y < f[1] + f[3]) return i; }
    return -1;
  }
  function show(e) {
    const r = canvas.getBoundingClientRect(), i = find(e.clientX - r.left, e.clientY - r.top);
    if (i !== hover) { hover = i; draw(); }
    if (i < 0) { tip.hidden = true; return; }
    const f = F[i], name = f[7] >= 0 ? names[f[7]] : "func_" + hex(f[5]).slice(2);
    tip.innerHTML = `<strong>${name.replace(/[<&]/g, c => c === "<" ? "&lt;" : "&amp;")}</strong><br>${hex(f[5])} · ${f[6]} bytes<br>` +
      `${states[f[4]]} · grupo ${G[f[8]].label}`;
    tip.hidden = false;
    const mr = map.getBoundingClientRect(); let tx = e.clientX - mr.left + 14, ty = e.clientY - mr.top + 14;
    if (tx + tip.offsetWidth > mr.width) tx = Math.max(4, e.clientX - mr.left - tip.offsetWidth - 14);
    if (ty + tip.offsetHeight > mr.height) ty = Math.max(4, e.clientY - mr.top - tip.offsetHeight - 14);
    tip.style.left = tx + "px"; tip.style.top = ty + "px";
  }
  canvas.addEventListener("mousemove", show);
  canvas.addEventListener("click", show);
  canvas.addEventListener("mouseleave", () => { hover = -1; tip.hidden = true; draw(); });
  q.addEventListener("input", () => {
    const t = q.value.trim().toLowerCase();
    if (!t) { matches = null; draw(); return; }
    matches = new Set();
    for (let i = 0; i < F.length; i++) { const f = F[i], name = f[7] >= 0 ? names[f[7]].toLowerCase() : "";
      if (name.includes(t) || hex(f[5]).toLowerCase().includes(t)) matches.add(i); }
    draw();
  });
  new ResizeObserver(draw).observe(map);
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", draw);
  draw();
})();
</script>
"""


def render_html(groups: Sequence[Group], *, document: bool, width: int = 1000, height: int = 620) -> str:
    names: list[str] = []
    name_index: dict[str, int] = {}
    rows, group_rows = [], []
    for g_index, (group, (gx, gy, gw, gh), placed) in enumerate(layout(groups, width, height)):
        first = len(rows)
        for function, (x, y, w, h) in placed:
            n = -1
            if function.name:
                n = name_index.setdefault(function.name, len(names))
                if n == len(names):
                    names.append(function.name)
            state = {"hand": 1, "template": 2}.get(function.state, 0)
            rows.append([round(x, 2), round(y, 2), round(w, 2), round(h, 2), state, LOAD_BASE + function.entry,
                         function.size, n, g_index])
        label = "SDK y librerías" if group.label == "SDK" else group.label
        group_rows.append({"label": label, "x": round(gx, 2), "y": round(gy, 2), "w": round(gw, 2), "h": round(gh, 2),
                           "share": 100 * group.done / group.size if group.size else 0.0, "first": first, "last": len(rows)})
    functions = [f for g in groups for f in g.functions]
    total_b = sum(f.size for f in functions)
    data = {"width": width, "height": height, "functions": rows, "groups": group_rows, "names": names,
            "bars": [["FUNCIONES", sum(1 for f in functions if f.state), len(functions)],
                     ["BYTES", sum(f.size for f in functions if f.state), total_b],
                     ["CON NOMBRE", sum(1 for f in functions if f.name), len(functions)]]}
    page = _PAGE.replace("__DATA__", json.dumps(data, separators=(",", ":")).replace("</", "<\\/"))
    if document:
        return ('<!doctype html>\n<html lang="es">\n<head>\n<meta charset="utf-8">\n'
                '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
                + page.replace("<main>", "</head>\n<body>\n<main>", 1) + "</body>\n</html>\n")
    return page


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    parser.add_argument("--src", type=Path, default=Path("src"))
    parser.add_argument("--symbols", type=Path, default=Path("config/symbol_addrs.txt"))
    parser.add_argument("--out", type=Path, default=Path("docs"))
    parser.add_argument("--readme", type=Path, default=Path("README.md"))
    parser.add_argument("--fragment", type=Path, help="also write the page without its document wrapper")
    args = parser.parse_args(argv)
    sizes = load_functions(args.functions)
    states = classify(args.src)
    symbols = load_symbols(args.symbols)
    functions = [Function(entry, size, states.get(entry), symbols.get(entry)) for entry, size in sizes.items()]
    groups = group_functions(functions)
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "progress_map.svg").write_text(render_svg(groups))
    (args.out / "progress").mkdir(exist_ok=True)
    (args.out / "progress" / "index.html").write_text(render_html(groups, document=True))
    if args.fragment:
        args.fragment.write_text(render_html(groups, document=False))
    if args.readme.exists():
        args.readme.write_text(replace_between_markers(args.readme.read_text(), bars(functions)))
    done = sum(1 for f in functions if f.state)
    print(f"{done} / {len(functions)} functions decompiled; wrote {args.out / 'progress_map.svg'}, "
          f"{args.out / 'progress' / 'index.html'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
