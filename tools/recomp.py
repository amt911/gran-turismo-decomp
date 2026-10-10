"""Regenerate recomp/generated/ (PSPRecomp C++ for the verified EBOOT) when its inputs changed.

Usage: .venv/bin/python -m tools.recomp [--elf PATH] [--out DIR]

The output is derived from the user's game executable: it lives in the gitignored recomp/ and is
never committed.

Every active GT_DECOMP registration under src/ is passed to psp_recomp as a boundary: fixed jumps and
calls to it go through the dispatcher even inside one generated unit, so the decompiled function
replaces the original at every call site (patch 0014; without it same-unit calls jumped straight
into the recompiled code).
"""

from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
from collections.abc import Callable, Iterable, Sequence
from pathlib import Path

from tools.progress import LOAD_BASE as PROGRESS_LOAD_BASE
from tools.progress import decompiled_functions

REPO = Path(__file__).resolve().parent.parent
LOAD_BASE = 0x08804000
# Generated translation-unit span. Measured 2026-10-09 (clang -O1): the default 0x20000 gives units up
# to 3.9 MB needing 4.4 GB / 233 s each; 0x8000 gives <= 1 MB units at 372 MB / 12 s (see FACTS).
UNIT_SPAN = 0x8000
RECOMP_BIN = REPO / "tools/src/lcs-recomp/out/tools/psp_recomp"
STAMP = ".inputs.sha1"
REGISTRY = "generated_registry.cpp"
BOUNDARIES = "decomp_boundaries.txt"

Runner = Callable[[Sequence[str]], None]


class RecompError(Exception):
    """Inputs for the recompiler are missing."""


def run(cmd: Sequence[str]) -> None:
    subprocess.run(list(cmd), check=True, stdout=subprocess.DEVNULL)


def _inputs_digest(elf: Path, recomp_bin: Path, load_base: int, unit_span: int, listing: str) -> str:
    h = hashlib.sha1()
    for path in (elf, recomp_bin):
        h.update(path.read_bytes())
    h.update(f"{load_base:#010x} {unit_span:#x}".encode())
    h.update(listing.encode())
    return h.hexdigest()


def decomp_boundaries(src: Path) -> list[int]:
    """Runtime addresses of the active GT_DECOMP registrations under `src`, sorted."""
    return sorted(PROGRESS_LOAD_BASE + vaddr for vaddr in decompiled_functions(src))


def regenerate(elf: Path, out_dir: Path, recomp_bin: Path, load_base: int = LOAD_BASE,
               unit_span: int = UNIT_SPAN, boundaries: Iterable[int] = (), runner: Runner = run) -> bool:
    """Run psp_recomp unless out_dir already holds output for exactly these inputs. True if it ran."""
    if not elf.is_file():
        raise RecompError(f"{elf} not found: run `.venv/bin/python -m tools.extract` first")
    if not recomp_bin.is_file():
        raise RecompError(f"{recomp_bin} not found: run tools/setup.sh first")
    listing = "".join(f"{address:#010x}\n" for address in sorted(boundaries))
    digest = _inputs_digest(elf, recomp_bin, load_base, unit_span, listing)
    stamp = out_dir / STAMP
    if (stamp.is_file() and stamp.read_text().strip() == digest
            and (out_dir / REGISTRY).is_file()):
        return False
    stamp.unlink(missing_ok=True)
    cmd = [str(recomp_bin), str(elf), "--auto", str(out_dir), f"{load_base:#010x}", f"{unit_span:#x}"]
    if listing:
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / BOUNDARIES).write_text(listing)
        cmd.append(str(out_dir / BOUNDARIES))
    runner(cmd)
    stamp.write_text(digest + "\n")
    return True


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--elf", type=Path, default=REPO / "extracted/EBOOT.elf")
    parser.add_argument("--out", type=Path, default=REPO / "recomp/generated")
    args = parser.parse_args(argv)
    try:
        ran = regenerate(args.elf, args.out, RECOMP_BIN, boundaries=decomp_boundaries(REPO / "src"))
    except RecompError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as e:
        print(f"error: psp_recomp exited with status {e.returncode}", file=sys.stderr)
        return 1
    print(f"{'regenerated' if ran else 'up to date'}: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
