"""Compare the GPU GE backend's frames with the software rasterizer's, frame by frame.

Usage: .venv/bin/python -m tools.gpu_compare DIR [--report FILE] [--diff-dir DIR]

DIR holds the pairs `GT_GPU_DUMP` writes for one run: `soft_NNNNNN.ppm` (the software rasterizer, the
verified reference) and `gpu_NNNNNN.ppm` (the Vulkan backend, downsampled to the same size), both from
the same presented frame. A frame passes when its images agree within rounding: no pixel channel
differs by more than PIXEL_TOLERANCE, except at most MAX_DIFFERING_SHARE of the pixels, and no BLOCK x
BLOCK square differs entirely (a missing element or an opaque quad). A soft frame without its GPU twin
fails as missing. The thresholds are fixed: never widen them to make a run pass.

Exit code: 0 every frame passes, 1 some frame fails, 2 nothing to compare or unreadable input.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

PIXEL_TOLERANCE = 24  # 5-6-5 framebuffer quantisation (8) plus shader-vs-rasterizer rounding
MAX_DIFFERING_SHARE = 0.01
BLOCK = 8

_FRAME = re.compile(r"^(soft|gpu)_(\d{6})\.ppm$")


@dataclass(frozen=True)
class Image:
    width: int
    height: int
    pixels: bytes  # RGB, row-major


@dataclass(frozen=True)
class Result:
    differing: int
    total: int
    max_delta: int
    blocks: int

    @property
    def passed(self) -> bool:
        return self.blocks == 0 and self.differing <= MAX_DIFFERING_SHARE * self.total


def read_ppm(path: Path) -> Image:
    """A binary P6 image with maxval 255, as frame_dump.cpp writes it (no comments)."""
    data = Path(path).read_bytes()
    fields = data.split(maxsplit=4)
    if len(fields) < 4 or fields[0] != b"P6" or fields[3] != b"255":
        raise ValueError(f"{path}: not a P6/255 PPM")
    width, height = int(fields[1]), int(fields[2])
    header = data.index(b"255") + 4  # one whitespace byte follows maxval
    pixels = data[header:]
    if len(pixels) != width * height * 3:
        raise ValueError(f"{path}: {len(pixels)} bytes of pixels, expected {width * height * 3}")
    return Image(width, height, pixels)


def write_ppm(path: Path, image: Image) -> None:
    path.write_bytes(b"P6\n%d %d\n255\n" % (image.width, image.height) + image.pixels)


def _mask(soft: Image, gpu: Image) -> tuple[list[bool], int]:
    mask, max_delta = [], 0
    a, b = soft.pixels, gpu.pixels
    for i in range(0, len(a), 3):
        delta = max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
        max_delta = max(max_delta, delta)
        mask.append(delta > PIXEL_TOLERANCE)
    return mask, max_delta


def _full_blocks(mask: list[bool], width: int, height: int) -> int:
    """How many BLOCK x BLOCK windows (any position) differ in every pixel, via a summed-area table."""
    if width < BLOCK or height < BLOCK:
        return 0
    stride = width + 1
    table = [0] * (stride * (height + 1))
    for y in range(height):
        row = 0
        for x in range(width):
            row += mask[y * width + x]
            table[(y + 1) * stride + x + 1] = table[y * stride + x + 1] + row
    full = BLOCK * BLOCK
    count = 0
    for y in range(height - BLOCK + 1):
        for x in range(width - BLOCK + 1):
            s = (table[(y + BLOCK) * stride + x + BLOCK] - table[y * stride + x + BLOCK]
                 - table[(y + BLOCK) * stride + x] + table[y * stride + x])
            count += s == full
    return count


def compare(soft: Image, gpu: Image) -> Result:
    if (soft.width, soft.height) != (gpu.width, gpu.height):
        raise ValueError(f"size {soft.width}x{soft.height} vs {gpu.width}x{gpu.height}")
    mask, max_delta = _mask(soft, gpu)
    return Result(sum(mask), len(mask), max_delta, _full_blocks(mask, soft.width, soft.height))


def pairs(directory: Path) -> list[tuple[int, Path, Path | None]]:
    """(frame index, soft path, gpu path or None when the GPU frame is missing), by index."""
    soft: dict[int, Path] = {}
    gpu: dict[int, Path] = {}
    for path in Path(directory).iterdir():
        match = _FRAME.match(path.name)
        if match:
            (soft if match.group(1) == "soft" else gpu)[int(match.group(2))] = path
    return [(index, soft[index], gpu.get(index)) for index in sorted(soft)]


def _diff_image(soft: Image, gpu: Image) -> Image:
    mask, _ = _mask(soft, gpu)
    out = bytearray(len(soft.pixels))
    for i, differs in enumerate(mask):
        if differs:
            out[i * 3:i * 3 + 3] = b"\xff\x00\x00"
        else:
            out[i * 3:i * 3 + 3] = bytes(c // 3 for c in soft.pixels[i * 3:i * 3 + 3])
    return Image(soft.width, soft.height, bytes(out))


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--diff-dir", type=Path)
    args = parser.parse_args(argv)
    if not args.directory.is_dir():
        print(f"gpu_compare: no such directory {args.directory}", file=sys.stderr)
        return 2
    found = pairs(args.directory)
    if not found:
        print(f"gpu_compare: no soft_NNNNNN.ppm frames in {args.directory}", file=sys.stderr)
        return 2
    lines, worst, failed, missing = [], [], 0, 0
    try:
        for index, soft_path, gpu_path in found:
            if gpu_path is None:
                failed += 1
                missing += 1
                lines.append(f"{index:06d} FAIL missing")
                continue
            soft, gpu = read_ppm(soft_path), read_ppm(gpu_path)
            result = compare(soft, gpu)
            share = result.differing / result.total
            status = "pass" if result.passed else "FAIL"
            lines.append(f"{index:06d} {status} differing {share:.4f} max_delta {result.max_delta} "
                         f"blocks {result.blocks}")
            if not result.passed:
                failed += 1
                worst.append((share, result.blocks, index))
                if args.diff_dir is not None:
                    args.diff_dir.mkdir(parents=True, exist_ok=True)
                    write_ppm(args.diff_dir / f"diff_{index:06d}.ppm", _diff_image(soft, gpu))
    except ValueError as error:
        print(f"gpu_compare: {error}", file=sys.stderr)
        return 2
    if args.report is not None:
        args.report.write_text("\n".join(lines) + "\n")
    print(f"pairs {len(found)}, passed {len(found) - failed}, failed {failed} (missing {missing})")
    for share, blocks, index in sorted(worst, reverse=True)[:10]:
        print(f"  worst {index:06d}: differing {share:.4f}, blocks {blocks}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
