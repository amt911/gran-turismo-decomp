"""Bisect which decompiled functions make `--decomp on` leave `--decomp off`'s import trace.

Both traces are the import-call order of the same route (`gt_native --trace`); a run is *good* when its first
`--lines` trace lines equal the `off` baseline's, so the criterion does not depend on timing (the pad route, the
frame the menu opens at, ...). Usage:

  .venv/bin/python -m tools.trace_bisect --pad 3300:START --lines 20000 --seconds 8 [--functions ADDR,ADDR,...]
  (--pad @config/pad_routes/race.pad reads a route file)

`--black-after V` bisects another symptom instead: the set "diverges" when the first frame dump at or after vblank
V is completely black (the `on` build drew nothing where `off` shows a menu). `--frame-diff V [--box x,y,w,h]`
bisects a picture difference: the set "diverges" when the first frame dump at or after vblank V differs from `off`'s
by more than `--tolerance` (mean absolute difference inside the box). `--crash` bisects a run that dies
(`terminate called`, a guest memory fault...): the set "diverges" when the run's log says so.

With no --functions, every function the harness registers is the starting set (read from a `--decomp diff` run).
The bisection keeps halving the set that still diverges (`ddmin`-style: a half that diverges alone replaces the
set; when neither half does, the divergence needs members of both and the set is reported as is).
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections.abc import Callable, Sequence
from pathlib import Path


def first_divergence(a: Sequence[str], b: Sequence[str]) -> int | None:
    """Index of the first line where the traces differ (a shorter trace differs where it ends), None when equal."""
    for index, (left, right) in enumerate(zip(a, b)):
        if left != right:
            return index
    return None if len(a) == len(b) else min(len(a), len(b))


def ddmin(items: Sequence[int], diverges: Callable[[Sequence[int]], bool],
          evaluate: Callable[[list[Sequence[int]]], list[bool]] | None = None) -> list[int]:
    """Smallest set (as far as halving finds) of `items` that still makes `diverges` true.

    Precondition: diverges(items). Tries each half; when a half diverges alone it replaces the set. When neither
    half does, tries removing each quarter; when nothing can be removed the set is returned. `evaluate`, when given,
    judges a whole level's subsets (or complements) at once -- in parallel -- and the first diverging one in order is
    taken, so the result is the sequential one."""
    judge = evaluate or (lambda subsets: [diverges(subset) for subset in subsets])

    def first(subsets: list[list[int]]) -> int | None:
        if evaluate is None:  # sequential: stop at the first hit, as before
            for index, subset in enumerate(subsets):
                if diverges(subset):
                    return index
            return None
        verdicts = judge(subsets)
        return next((i for i, v in enumerate(verdicts) if v), None)

    current = list(items)
    granularity = 2
    while len(current) >= 2:
        chunk = max(1, len(current) // granularity)
        subsets = [current[i:i + chunk] for i in range(0, len(current), chunk)]
        complements = []
        if granularity > 2:
            complements = [c for c in ([x for j, s in enumerate(subsets) if j != index for x in s]
                                       for index in range(len(subsets))) if c]
        if evaluate is not None and complements:  # one batch: a subset hit still wins over a complement hit
            verdicts = evaluate(subsets + complements)
            hit = next((i for i, v in enumerate(verdicts[:len(subsets)]) if v), None)
            if hit is not None:
                current, granularity = subsets[hit], 2
                continue
            hit = next((i for i, v in enumerate(verdicts[len(subsets):]) if v), None)
            if hit is not None:
                current, granularity = complements[hit], max(granularity - 1, 2)
                continue
        else:
            hit = first(subsets)
            if hit is not None:
                current, granularity = subsets[hit], 2
                continue
            if complements:
                hit = first(complements)
                if hit is not None:
                    current, granularity = complements[hit], max(granularity - 1, 2)
                    continue
        if granularity >= len(current):
            break
        granularity = min(len(current), granularity * 2)
    return current


def narrow_window(items: Sequence[int], diverges: Callable[[Sequence[int]], bool]) -> tuple[int, int]:
    """The shortest window items[lo:hi] that still diverges, by two binary searches (shortest prefix, then shortest
    suffix of it). For a symptom that needs many members at once, where ddmin degenerates to one run per member
    (every fifth is needed, every quarter...). Precondition: diverges(items); assumes adding members never cures it."""
    lo, hi = 1, len(items)  # shortest prefix items[:hi]
    while lo < hi:
        mid = (lo + hi) // 2
        if diverges(items[:mid]):
            hi = mid
        else:
            lo = mid + 1
    end = hi
    lo, hi = 0, end - 1  # largest start with items[start:end] diverging
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if diverges(items[mid:end]):
            lo = mid
        else:
            hi = mid - 1
    return lo, end


def ppm_mean(data: bytes) -> float:
    """Mean brightness (0..1) of a binary PPM (P6) image; 0.0 means completely black."""
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    if match is None:
        raise ValueError("not a P6 image")
    pixels = data[match.end():]
    return sum(pixels) / (len(pixels) * 255) if pixels else 0.0


def _ppm_pixels(data: bytes) -> tuple[int, int, bytes]:
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    if match is None:
        raise ValueError("not a P6 image")
    return int(match.group(1)), int(match.group(2)), data[match.end():]


def ppm_box_diff(a: bytes, b: bytes, box: tuple[int, int, int, int]) -> float:
    """Mean absolute difference (0..1) of two same-size P6 images inside box (x, y, w, h)."""
    wa, ha, pa = _ppm_pixels(a)
    wb, hb, pb = _ppm_pixels(b)
    if (wa, ha) != (wb, hb):
        raise ValueError(f"image sizes differ: {wa}x{ha} vs {wb}x{hb}")
    x, y, w, h = box
    total = 0
    for row in range(y, y + h):
        start = (row * wa + x) * 3
        end = start + w * 3
        total += sum(abs(p - q) for p, q in zip(pa[start:end], pb[start:end]))
    count = w * h * 3
    return total / (count * 255) if count else 0.0


def ppm_box_mean(data: bytes, box: tuple[int, int, int, int]) -> float:
    """Mean brightness (0..1) of a P6 image inside box (x, y, w, h)."""
    width, height, pixels = _ppm_pixels(data)
    black = b"P6\n%d %d\n255\n" % (width, height) + bytes(len(pixels))
    return ppm_box_diff(black, data, box)


def script_done_vblank(log: str) -> int | None:
    """The vblank the pad script finished at (`[pad] script done at vblank N`), None when it never did."""
    match = re.search(r"^\[pad\] script done at vblank (\d+)", log, re.MULTILINE)
    return int(match.group(1)) if match else None


def parse_box(text: str) -> tuple[int, int, int, int]:
    x, y, w, h = (int(t) for t in text.split(","))
    return x, y, w, h


def first_frame_after(log: str, vblank: int) -> int | None:
    """Number of the first frame dump logged at or after `vblank`, None when there is none."""
    for m in re.finditer(r"^\[frame-dump\] frame (\d+) vblank (\d+)", log, re.MULTILINE):
        if int(m.group(2)) >= vblank:
            return int(m.group(1))
    return None


MAX_ENV_STRING = 131072  # Linux MAX_ARG_STRLEN: one "NAME=value" string longer than this and exec fails


def decomp_only_value(only: Sequence[int]) -> str:
    """GT_DECOMP_ONLY for `only`: hex without 0x (9 bytes an address), refused past the kernel's per-string cap."""
    value = ",".join(f"{a:08X}" for a in only)
    if len("GT_DECOMP_ONLY=") + len(value) + 1 > MAX_ENV_STRING:
        raise ValueError(f"{len(only)} addresses do not fit in one environment string")
    return value


def crashed(log: str) -> bool:
    """Whether a run's stderr shows the process dying on an uncaught error (not a normal stop)."""
    return "terminate called" in log or "gt_native error:" in log


FAULT = re.compile(r"Guest memory access outside PSP RAM/EDRAM at (0x[0-9A-Fa-f]+)")


def crash_summary(logs: Sequence[str]) -> tuple[bool, str]:
    """--crash --repeat: a subset crashes when any of its runs died. The summary counts them and names the fault addresses."""
    died = [log for log in logs if crashed(log)]
    for log in logs:  # a run that exits at once (bad GT_PAD, missing file) never played: an error, never "ok"
        if log not in died and "Wall-clock limit reached" not in log:
            first = next((line for line in log.splitlines() if line.strip()), "(empty log)")
            raise RuntimeError(f"a run neither crashed nor reached its time limit: {first}")
    faults = sorted({m.group(1).upper().replace("0X", "0x") for log in died for m in FAULT.finditer(log)})
    text = f"{len(died)}/{len(logs)} crashed" + (f" ({', '.join(faults)})" if faults else "")
    return bool(died), text


def require_functions(functions: list[int], log: str) -> list[int]:
    """The registered list, refused when empty: the listing run died before registering anything (its log says why)."""
    if not functions:
        first = next((line for line in log.splitlines() if line.startswith("GT_PAD") or "error" in line), "see list.log")
        raise RuntimeError(f"no registered functions in the listing run: {first}")
    return functions


def registered_functions(log: str) -> list[int]:  # pragma: no cover - trivially a regex over a log
    return [int(m.group(1), 16) for m in re.finditer(r"^\[decomp\] \S+ @0x([0-9A-Fa-f]{8}):", log, re.MULTILINE)]


KEEP_MODS = False  # --keep-mods: bisect a fault that needs the installed mods (hooks on decompiled functions)


def run_env(base: dict, pad: str, keep_mods: bool) -> dict:
    """The environment of one bisection run: the pad route, uncapped, and no mods unless --keep-mods."""
    env = dict(base, GT_PAD=pad, GT_PAD_LOG="1", LCS_UNCAPPED="1")
    if keep_mods:
        env.pop("GT_NO_MODS", None)
    else:
        env["GT_NO_MODS"] = "1"
    return env


def _run(binary: Path, pad: str, seconds: int, lines: int, trace: Path, only: Sequence[int] | None,
         mode: str, frames: Path | None = None, savedata: Path | None = None) -> list[str]:  # pragma: no cover - runs the game
    env = run_env(dict(os.environ), pad, KEEP_MODS)
    if savedata is not None:  # parallel runs (--repeat) must not share a savedata folder
        savedata.mkdir(parents=True, exist_ok=True)
        env["GT_SAVEDATA_DIR"] = str(savedata)
    if frames is not None:
        env.update(GT_FRAME_DUMP=str(frames), GT_FRAME_EVERY="30")
    if only is not None:
        env["GT_DECOMP_ONLY"] = decomp_only_value(only)
    command = ["systemd-run", "--user", "--scope", "--quiet", "--slice=gt-heavy.slice", "--", str(binary),
               "--max-seconds", str(seconds), "--decomp", mode]
    if lines > 0:  # --lines 0: no import trace (the run stops when the trace reaches its limit)
        command += ["--trace", str(trace), "--trace-limit", str(lines)]
    else:
        trace.write_text("")
    log = trace.with_suffix(".log")
    with log.open("w") as err:
        subprocess.run(command, env=env, stdout=err, stderr=subprocess.STDOUT, check=False)  # stdout carries "Runtime stopped: ..."
    return trace.read_text().splitlines()[:lines]


def _frame_at(args: argparse.Namespace, only: Sequence[int] | None, mode: str) -> bytes:  # pragma: no cover - runs the game
    """The first frame dump at or after --frame-diff's vblank of one run (b"" when the run made none)."""
    frames = args.workdir / f"frames_{mode}"
    frames.mkdir(parents=True, exist_ok=True)
    for old in frames.glob("*.ppm"):
        old.unlink()
    trace = args.workdir / f"{mode}.trace"
    _run(args.binary, args.pad, args.seconds, args.lines, trace, only, mode, frames)
    number = first_frame_after(trace.with_suffix(".log").read_text(errors="replace"), args.frame_diff)
    path = frames / f"frame_{number:06d}.ppm" if number is not None else None
    return path.read_bytes() if path is not None and path.exists() else b""


def _dark_box_mean(args: argparse.Namespace, only: Sequence[int] | None, tag: str) -> float:  # pragma: no cover - runs the game
    """--dark-box's region brightness in the frame --after-script vblanks after the pad script ended.

    A run that never gets there (the route timed out under load, the game died) is inconclusive, not dark: it is
    retried, and after three tries the bisection stops instead of chasing the wrong symptom. Each try's log is kept.
    """
    run_dir = args.workdir / tag  # one per run: --jobs runs several at once
    frames = run_dir / "frames_on"
    frames.mkdir(parents=True, exist_ok=True)
    trace = run_dir / "on.trace"
    for attempt in range(3):
        for old in frames.glob("*.ppm"):
            old.unlink()
        _run(args.binary, args.pad, args.seconds, args.lines, trace, only, "on", frames)
        log = trace.with_suffix(".log").read_text(errors="replace")
        (args.workdir / f"{tag}_try{attempt}.log").write_text(log)
        done = script_done_vblank(log)
        number = first_frame_after(log, done + args.after_script) if done is not None else None
        path = frames / f"frame_{number:06d}.ppm" if number is not None else None
        if path is not None and path.exists():
            return ppm_box_mean(path.read_bytes(), args.dark_box)
        print(f"  {tag}: inconclusive try {attempt} (" + ("crashed" if crashed(log) else "no frame after the route")
              + ")", flush=True)
    raise SystemExit(f"{tag}: three inconclusive runs; see {args.workdir}/{tag}_try*.log")


def main(argv: list[str] | None = None) -> int:  # pragma: no cover - CLI wiring
    parser = argparse.ArgumentParser(prog="tools.trace_bisect", description=__doc__.splitlines()[0])
    parser.add_argument("--binary", type=Path, default=Path("build/gt_native"))
    parser.add_argument("--pad", default="3300:START")
    parser.add_argument("--seconds", type=int, default=8)
    parser.add_argument("--lines", type=int, default=20000)
    parser.add_argument("--functions", default="", help="comma-separated hex addresses; default: every registered one")
    parser.add_argument("--crash", action="store_true", help="bisect 'the run dies' instead")
    parser.add_argument("--repeat", type=int, default=1,
                        help="--crash: runs per subset, at once, each with its own savedata (a flaky crash counts when any run dies)")
    parser.add_argument("--keep-mods", action="store_true",
                        help="runs keep the installed mods (GT_MODS_DIR): a fault that needs a mod's hook")
    parser.add_argument("--black-after", type=int, default=0, help="bisect 'black frame at/after this vblank' instead")
    parser.add_argument("--frame-diff", type=int, default=0,
                        help="bisect 'the frame at/after this vblank differs from off inside --box' instead")
    parser.add_argument("--dark-box", type=parse_box, default=None,
                        help="bisect 'region x,y,w,h is darker than --dark-below' in the frame --after-script vblanks after the pad script ends")
    parser.add_argument("--dark-below", type=float, default=0.075)
    parser.add_argument("--after-script", type=int, default=600)
    parser.add_argument("--box", type=parse_box, default=None, help="x,y,w,h of the --frame-diff region (default: whole frame)")
    parser.add_argument("--tolerance", type=float, default=0.02, help="--frame-diff: mean abs difference that counts as differing")
    parser.add_argument("--jobs", type=int, default=1, help="--dark-box: runs of one ddmin level at once")
    parser.add_argument("--narrow", action="store_true",
                        help="find the shortest diverging window (prefix, then suffix) instead of ddmin")
    parser.add_argument("--start-set", type=Path, default=None, help="file of hex addresses to start from (one per line)")
    parser.add_argument("--workdir", type=Path, default=Path("recomp/analysis/bisect"))
    args = parser.parse_args(argv)
    global KEEP_MODS
    KEEP_MODS = args.keep_mods
    if args.pad.startswith("@"):  # @file: a route file (comment lines ignored)
        args.pad = "".join(line for line in Path(args.pad[1:]).read_text().splitlines() if not line.startswith("#"))
    args.workdir.mkdir(parents=True, exist_ok=True)
    baseline: list[str] = []
    reference = b""
    if args.frame_diff:
        reference = _frame_at(args, None, "off")
        print(f"baseline frame: {len(reference)} bytes")
    elif not args.black_after and not args.crash and not args.dark_box:
        baseline = _run(args.binary, args.pad, args.seconds, args.lines, args.workdir / "off.trace", None, "off")
        print(f"baseline: {len(baseline)} lines")
    if args.functions:
        functions = [int(t, 16) for t in args.functions.split(",")]
    else:
        listing = args.workdir / "list.log"
        env = dict(os.environ, GT_PAD=args.pad, LCS_UNCAPPED="1")
        with listing.open("w") as out:
            subprocess.run([str(args.binary), "--max-seconds", "2", "--decomp", "diff"], env=env, stdout=out,
                           stderr=subprocess.STDOUT, check=False)
        functions = require_functions(registered_functions(listing.read_text()), listing.read_text())
    runs = 0

    def is_black(subset: Sequence[int]) -> bool:
        frames = args.workdir / "frames"
        for old in frames.glob("*.ppm"):
            old.unlink()
        _run(args.binary, args.pad, args.seconds, args.lines, args.workdir / "on.trace", subset, "on", frames)
        number = first_frame_after((args.workdir / "on.log").read_text(errors="replace"), args.black_after)
        if number is None:
            return True  # no frame at all counts as black
        return ppm_mean((frames / f"frame_{number:06d}.ppm").read_bytes()) == 0.0

    def diverges(subset: Sequence[int]) -> bool:
        nonlocal runs
        runs += 1
        if len(subset) == registered_count:
            # The whole set is the default (GT_DECOMP_ONLY unset): ~16k addresses would exceed Linux's 128 KiB cap
            # on one environment string (MAX_ARG_STRLEN) and the run would not even start.
            subset = None  # type: ignore[assignment]
            count = len(functions)
        else:
            count = len(subset)
        if args.crash:
            from concurrent.futures import ThreadPoolExecutor

            def one(k: int) -> str:
                trace = args.workdir / f"on{k}.trace"
                _run(args.binary, args.pad, args.seconds, args.lines, trace, subset, "on", savedata=args.workdir / f"savedata{k}")
                return trace.with_suffix(".log").read_text(errors="replace")

            with ThreadPoolExecutor(max_workers=max(1, args.repeat)) as pool:
                logs = list(pool.map(one, range(max(1, args.repeat))))
            died, text = crash_summary(logs)
            print(f"run {runs}: {count} functions -> " + ("CRASH " if died else "ok ") + text, flush=True)
            return died
        if args.dark_box:
            mean = _dark_box_mean(args, subset, f"run{runs}")
            print(f"run {runs}: {count} functions -> box mean {mean:.4f}", flush=True)
            return mean < args.dark_below
        if args.frame_diff:
            image = _frame_at(args, subset, "on")
            box = args.box or (0, 0, *_ppm_pixels(reference)[:2])
            diff = ppm_box_diff(reference, image, box) if image else 1.0
            print(f"run {runs}: {count} functions -> diff {diff:.4f}", flush=True)
            return diff > args.tolerance
        if args.black_after:
            black = is_black(subset)
            print(f"run {runs}: {count} functions -> " + ("BLACK" if black else "drawn"), flush=True)
            return black
        trace = _run(args.binary, args.pad, args.seconds, args.lines, args.workdir / "on.trace", subset, "on")
        at = first_divergence(baseline, trace)
        print(f"run {runs}: {count} functions -> " + ("same" if at is None else f"differs at import {at}"), flush=True)
        return at is not None

    registered_count = len(functions)  # that many = every function: GT_DECOMP_ONLY stays unset
    if args.start_set is not None:
        functions = [int(t, 16) for t in args.start_set.read_text().split()]
    if args.narrow:
        lo, hi = narrow_window(functions, diverges)
        print(f"window [{lo}, {hi}) of {len(functions)}: " + ",".join(f"0x{a:08X}" for a in functions[lo:hi]))
        return 0
    if not diverges(functions):
        print("the full set does not diverge within the window: nothing to bisect")
        return 0
    evaluate = None
    if args.dark_box and args.jobs > 1:
        from concurrent.futures import ThreadPoolExecutor

        def evaluate(subsets: list[Sequence[int]]) -> list[bool]:
            nonlocal runs
            tags = []
            for subset in subsets:
                runs += 1
                tags.append((f"run{runs}", None if len(subset) == registered_count else subset, len(subset)))
            with ThreadPoolExecutor(max_workers=args.jobs) as pool:
                means = list(pool.map(lambda t: _dark_box_mean(args, t[1], t[0]), tags))
            for (tag, _, count), mean in zip(tags, means):
                print(f"{tag}: {count} functions -> box mean {mean:.4f}", flush=True)
            return [mean < args.dark_below for mean in means]

    found = ddmin(functions, diverges, evaluate)
    print("minimal diverging set: " + ",".join(f"0x{a:08X}" for a in found))
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
