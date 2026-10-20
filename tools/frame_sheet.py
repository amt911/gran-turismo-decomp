"""Route-authoring aid: contact sheets of the frame dumps of a run, and the file seeks that identify its screens.

Usage (after `tools/pad_route.sh <route> <seconds> <outdir>` with GT_IO_LOG=1 and GT_FRAME_EVERY=N):
  python -m tools.frame_sheet seeks <outdir>/run.log [--from V] [--to V]      page loads: GT.VOL offsets by vblank
  python -m tools.frame_sheet sheet <outdir> V1,V2,... [--out sheet.png]       the frame nearest each vblank, labelled
  python -m tools.frame_sheet waits <outdir>/run.log                           the WAIT lines of a run

Frame dumps are game output: keep <outdir> under recomp/analysis/ (ignored by git), never commit them.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

_FRAME = re.compile(r"^\[frame-dump\] frame (\d+) vblank (\d+)", re.MULTILINE)
_SEEK = re.compile(r"^\[io\] seek \d+ ([0-9a-f]+) vblank (\d+)", re.MULTILINE)
_WAIT = re.compile(r"^\[pad\] WAIT (\S+) satisfied at vblank (\d+) \(waited (\d+)\)", re.MULTILINE)


def parse_frames(text: str) -> list[tuple[int, int]]:
    """(frame number, vblank) of every frame dump the run logged."""
    return [(int(m.group(1)), int(m.group(2))) for m in _FRAME.finditer(text)]


def parse_seeks(text: str) -> list[tuple[int, int]]:
    """(vblank, absolute offset) of every seek logged with GT_IO_LOG=1."""
    return [(int(m.group(2)), int(m.group(1), 16)) for m in _SEEK.finditer(text)]


def parse_waits(text: str) -> list[tuple[int, int, str]]:
    """(vblank satisfied, vblanks waited, condition) of every WAIT the run logged (GT_PAD_LOG=1)."""
    return [(int(m.group(2)), int(m.group(3)), m.group(1)) for m in _WAIT.finditer(text)]


def pick(frames: list[tuple[int, int]], wanted: list[int]) -> list[tuple[int, int]]:
    """The dumped frame nearest each wanted vblank, without repeats."""
    out: list[tuple[int, int]] = []
    for vblank in wanted:
        nearest = min(frames, key=lambda frame: abs(frame[1] - vblank))
        if nearest not in out:
            out.append(nearest)
    return out


def clusters(seeks: list[tuple[int, int]], gap: int, min_offset: int = 0) -> list[tuple[int, list[int]]]:
    """Seeks grouped into loads: a new group starts when `gap` vblanks passed without one. Seeks below `min_offset`
    (the volume's table of contents, near its start) are left out. Each group is (first vblank, offsets in order)."""
    groups: list[tuple[int, list[int]]] = []
    previous = None
    for vblank, offset in seeks:
        if offset < min_offset:
            continue
        if previous is None or vblank - previous > gap:
            groups.append((vblank, []))
        groups[-1][1].append(offset)
        previous = vblank
    return groups


def _cmd_seeks(args: argparse.Namespace) -> int:  # pragma: no cover - prints
    seeks = [s for s in parse_seeks(Path(args.log).read_text(errors="replace")) if args.start <= s[0] <= args.stop]
    for vblank, offsets in clusters(seeks, args.gap, min_offset=args.min_offset):
        print(f"{vblank}: " + " ".join(f"{o:x}" for o in offsets[: args.per_line]))
    return 0


def _cmd_sheet(args: argparse.Namespace) -> int:  # pragma: no cover - runs ImageMagick
    out_dir = Path(args.outdir)
    frames = parse_frames((out_dir / "run.log").read_text(errors="replace"))
    chosen = pick(frames, [int(v) for v in args.vblanks.split(",")])
    files = []
    for number, vblank in chosen:
        files += ["-label", f"v{vblank}", str(out_dir / "frames" / f"frame_{number:06d}.ppm")]
    out = Path(args.out) if args.out else out_dir / "sheet.png"
    subprocess.run(["magick", "montage", *files, "-tile", f"{args.columns}x", "-geometry", "240x136+2+2",
                    "-pointsize", "14", str(out)], check=True)
    print(out)
    return 0


def _cmd_waits(args: argparse.Namespace) -> int:  # pragma: no cover - prints
    for vblank, waited, condition in parse_waits(Path(args.log).read_text(errors="replace")):
        print(f"vblank {vblank:>6}  waited {waited:>5}  {condition}")
    return 0


def main(argv: list[str] | None = None) -> int:  # pragma: no cover - CLI wiring
    parser = argparse.ArgumentParser(prog="tools.frame_sheet", description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    seeks = sub.add_parser("seeks")
    seeks.add_argument("log")
    seeks.add_argument("--from", dest="start", type=int, default=0)
    seeks.add_argument("--to", dest="stop", type=int, default=10**9)
    seeks.add_argument("--gap", type=int, default=25)
    seeks.add_argument("--min-offset", type=lambda s: int(s, 0), default=0x1000000)
    seeks.add_argument("--per-line", type=int, default=12)
    sheet = sub.add_parser("sheet")
    sheet.add_argument("outdir")
    sheet.add_argument("vblanks")
    sheet.add_argument("--out")
    sheet.add_argument("--columns", type=int, default=6)
    waits = sub.add_parser("waits")
    waits.add_argument("log")
    args = parser.parse_args(argv)
    return {"seeks": _cmd_seeks, "sheet": _cmd_sheet, "waits": _cmd_waits}[args.command](args)


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
