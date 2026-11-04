"""Frame pacing of a gt_play run: per-presented-frame timing from a GT_FRAME_LOG file.

The averages in GT_PERF_LOG ("presented 59.9/s") hide uneven pacing; this reads the per-frame log instead.

  GT_FRAME_LOG=run.framelog GT_NO_MODS=1 build/gt_play ...        write the log at exit (off by default; GT_NO_MODS=1: a mods/ dir must not change what is measured)
  python -m tools.frame_pacing run.framelog [more.framelog ...] [--skip-s 30] [--end-s 200] [--json]

Log format (text, one row per line, `#` lines ignored; times are wall-clock microseconds since the run began):
  P wall_us vblank guest_us new gpu_ready skip dur_us   one per present_frame call (host/gt_kernel.cpp)
      new: the game flipped its framebuffer since the last present (a new picture), gpu_ready: the GPU backend
      closed a frame, skip: the frame was drawn with the frameskip on, dur_us: time spent inside present_frame
  V wall_us vblank guest_us lag_us sleep_us skipped late   one per new guest vblank period (first wait only)
      lag_us: wall time past the vblank's due time (negative: early), sleep_us: time slept for it,
      skipped: guest vblanks jumped over to catch up, late: the pacer's host_late flag (frameskip trigger)
  D wall_us present_id   one per present the screen showed (VK_KHR_present_wait completed; Vulkan with a window)
      the screen-side truth: presents can leave evenly and still reach the screen unevenly
  S kind begin_us end_us tag   one per timed span: W = the game thread waiting for the GE worker (tag: 0 queue,
      1 sceGeDrawSync, 2 sceGeListSync), L = the GE worker executing one display list (tag 0), F = the game flipped
      (sceDisplaySetFrameBuf; begin = end, tag = the guest thread's uid), B = a guest thread reached a vblank wait (tag = 2 * thread uid + 1 for the period's
      first wait, the one that presents, + 0 otherwise)
No game data is in a log: only timestamps and counters.
"""

from __future__ import annotations

import argparse
import bisect
import json
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

HEADER = "# gt-frame-log v1"
_EDGES_MS = (8, 12, 15, 18, 22, 30, 36, 45, 60, 90, 150)
_LABELS = ("<8", "8-12", "12-15", "15-18", "18-22", "22-30", "30-36", "36-45", "45-60", "60-90", "90-150", ">=150")


@dataclass(frozen=True)
class Present:
    wall_us: int
    vblank: int
    guest_us: int
    new: bool
    gpu_ready: bool
    skip: bool
    dur_us: int


@dataclass(frozen=True)
class Vblank:
    wall_us: int
    vblank: int
    guest_us: int
    lag_us: int
    sleep_us: int
    skipped: int
    late: bool


@dataclass(frozen=True)
class Display:
    wall_us: int
    present_id: int


@dataclass(frozen=True)
class Span:
    kind: str
    begin_us: int
    end_us: int
    tag: int


@dataclass
class Log:
    presents: list[Present] = field(default_factory=list)
    vblanks: list[Vblank] = field(default_factory=list)
    displays: list[Display] = field(default_factory=list)
    spans: list[Span] = field(default_factory=list)


def percentile(values: list[float], pct: float) -> float:
    """Linear-interpolated percentile (pct in 0..100)."""
    if not values:
        raise ValueError("percentile of an empty list")
    ordered = sorted(values)
    rank = (len(ordered) - 1) * pct / 100.0
    low = int(rank)
    high = min(low + 1, len(ordered) - 1)
    return float(ordered[low] + (ordered[high] - ordered[low]) * (rank - low))


def parse(text: str) -> Log:
    lines = text.splitlines()
    if not lines or lines[0].strip() != HEADER:
        raise ValueError(f"not a gt-frame-log file (first line must be {HEADER!r})")
    log = Log()
    for number, line in enumerate(lines[1:], start=2):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        try:
            if fields[0] == "P" and len(fields) == 8:
                w, v, g, new, ready, skip, dur = (int(x) for x in fields[1:])
                log.presents.append(Present(w, v, g, bool(new), bool(ready), bool(skip), dur))
            elif fields[0] == "V" and len(fields) == 8:
                w, v, g, lag, sleep, skipped, late = (int(x) for x in fields[1:])
                log.vblanks.append(Vblank(w, v, g, lag, sleep, skipped, bool(late)))
            elif fields[0] == "D" and len(fields) == 3:
                log.displays.append(Display(int(fields[1]), int(fields[2])))
            elif fields[0] == "S" and len(fields) == 5:
                begin, end, tag = (int(x) for x in fields[2:])
                if end < begin:
                    raise ValueError("span ends before it begins")
                log.spans.append(Span(fields[1], begin, end, tag))
            else:
                raise ValueError("unknown or short row")
        except ValueError as error:
            raise ValueError(f"line {number}: {error}: {line!r}") from None
    return log


def window(log: Log, start_s: float = 0.0, end_s: float | None = None) -> Log:
    """Rows with start_s <= wall time < end_s (seconds since the run began): drops loading and shutdown."""
    lo = int(start_s * 1_000_000)
    hi = None if end_s is None else int(end_s * 1_000_000)

    def keep(wall_us: int) -> bool:
        return wall_us >= lo and (hi is None or wall_us < hi)

    return Log([p for p in log.presents if keep(p.wall_us)], [v for v in log.vblanks if keep(v.wall_us)],
               [d for d in log.displays if keep(d.wall_us)], [s for s in log.spans if keep(s.begin_us)])


def new_frame_intervals_us(log: Log) -> list[int]:
    """Wall time between consecutive presents that showed a new picture."""
    walls = [p.wall_us for p in log.presents if p.new]
    return [b - a for a, b in zip(walls, walls[1:])]


def miss_breakdown(log: Log, long_ms: float = 25.0) -> dict:
    """Per new-frame interval, the game thread's time waiting for the GE (W spans) and the GE's list time (L spans),
    clipped to the interval: medians for the long intervals (a refresh missed) and the normal ones."""
    walls = [p.wall_us for p in log.presents if p.new]
    groups: dict[str, dict[str, list[float]]] = {"long": {"W": [], "L": []}, "normal": {"W": [], "L": []}}
    by_kind = {kind: sorted((s for s in log.spans if s.kind == kind), key=lambda s: s.begin_us) for kind in ("W", "L")}
    begins = {kind: [s.begin_us for s in spans] for kind, spans in by_kind.items()}
    longest = {kind: max((s.end_us - s.begin_us for s in spans), default=0) for kind, spans in by_kind.items()}
    for a, b in zip(walls, walls[1:]):
        group = groups["long" if (b - a) / 1000.0 > long_ms else "normal"]
        for kind, spans in by_kind.items():
            first = bisect.bisect_left(begins[kind], a - longest[kind])
            last = bisect.bisect_left(begins[kind], b)
            inside = sum(max(0, min(s.end_us, b) - max(s.begin_us, a)) for s in spans[first:last])
            group[kind].append(inside / 1000.0)

    def median(values: list[float]) -> float:
        return percentile(values, 50) if values else 0.0

    return {"long_frames": len(groups["long"]["W"]), "normal_frames": len(groups["normal"]["W"]),
            "long_wait_ms": median(groups["long"]["W"]), "normal_wait_ms": median(groups["normal"]["W"]),
            "long_ge_ms": median(groups["long"]["L"]), "normal_ge_ms": median(groups["normal"]["L"])}


def histogram_ms(intervals_ms: list[float]) -> list[tuple[str, int]]:
    counts = [0] * len(_LABELS)
    for value in intervals_ms:
        bucket = 0
        while bucket < len(_EDGES_MS) and value >= _EDGES_MS[bucket]:
            bucket += 1
        counts[bucket] += 1
    return list(zip(_LABELS, counts))


@dataclass
class Summary:
    presents: int
    frames: int          # presents that showed a new picture
    repeats: int         # presents that repeated the last picture
    skipped_frames: int  # presents of frames drawn with the frameskip on
    intervals: int
    p50_ms: float
    p90_ms: float
    p99_ms: float
    max_ms: float
    over_17_5_share: float
    over_34_share: float
    longest_stall_at_s: float
    fps: float
    present_dur_p99_ms: float
    present_dur_max_ms: float
    vblank_waits: int
    vblank_lag_p50_ms: float
    vblank_lag_p99_ms: float
    vblank_lag_max_ms: float
    vblanks_skipped: int
    vblanks_late: int
    histogram: list[tuple[str, int]]
    displays: int = 0             # D rows: presents the screen showed
    display_p50_ms: float = 0.0   # intervals between them
    display_p99_ms: float = 0.0
    display_max_ms: float = 0.0
    display_over_17_5_share: float = 0.0
    display_histogram: list[tuple[str, int]] = field(default_factory=list)


def summarize(log: Log) -> Summary:
    new_walls = [p.wall_us for p in log.presents if p.new]
    if len(new_walls) < 2:
        raise ValueError("need at least two new frames to measure an interval")
    gaps = [b - a for a, b in zip(new_walls, new_walls[1:])]
    ms = [g / 1000.0 for g in gaps]
    worst = max(range(len(gaps)), key=gaps.__getitem__)
    span_s = (new_walls[-1] - new_walls[0]) / 1_000_000
    durs = [p.dur_us / 1000.0 for p in log.presents]
    lags = [v.lag_us / 1000.0 for v in log.vblanks]
    return Summary(
        presents=len(log.presents),
        frames=len(new_walls),
        repeats=sum(1 for p in log.presents if not p.new),
        skipped_frames=sum(1 for p in log.presents if p.skip),
        intervals=len(gaps),
        p50_ms=percentile(ms, 50), p90_ms=percentile(ms, 90), p99_ms=percentile(ms, 99), max_ms=max(ms),
        over_17_5_share=sum(1 for m in ms if m > 17.5) / len(ms),
        over_34_share=sum(1 for m in ms if m > 34.0) / len(ms),
        longest_stall_at_s=(new_walls[worst + 1] - new_walls[0]) / 1_000_000,
        fps=len(gaps) / span_s if span_s > 0 else 0.0,
        present_dur_p99_ms=percentile(durs, 99), present_dur_max_ms=max(durs),
        vblank_waits=len(log.vblanks),
        vblank_lag_p50_ms=percentile(lags, 50) if lags else 0.0,
        vblank_lag_p99_ms=percentile(lags, 99) if lags else 0.0,
        vblank_lag_max_ms=max(lags) if lags else 0.0,
        vblanks_skipped=sum(v.skipped for v in log.vblanks),
        vblanks_late=sum(1 for v in log.vblanks if v.late),
        histogram=histogram_ms(ms),
        **display_figures(log),
    )


def display_figures(log: Log) -> dict:
    walls = sorted(d.wall_us for d in log.displays)
    gaps = [(b - a) / 1000.0 for a, b in zip(walls, walls[1:])]
    if not gaps:
        return {"displays": len(walls)}
    return {"displays": len(walls), "display_p50_ms": percentile(gaps, 50), "display_p99_ms": percentile(gaps, 99),
            "display_max_ms": max(gaps), "display_over_17_5_share": sum(1 for g in gaps if g > 17.5) / len(gaps),
            "display_histogram": histogram_ms(gaps)}


def report(s: Summary, name: str = "run") -> str:
    bars = " ".join(f"{label}:{n}" for label, n in s.histogram)
    return "\n".join([
        f"== {name}: {s.frames} new frames ({s.fps:.1f}/s), {s.repeats} repeats, {s.skipped_frames} frameskipped, "
        f"{s.intervals} intervals",
        f"interval ms: p50 {s.p50_ms:.2f}  p90 {s.p90_ms:.2f}  p99 {s.p99_ms:.2f}  max {s.max_ms:.2f}",
        f"share > 17.5 ms: {100 * s.over_17_5_share:.1f}%   share > 34 ms: {100 * s.over_34_share:.1f}%",
        f"longest stall: {s.max_ms:.1f} ms, ending {s.longest_stall_at_s:.1f} s into the window",
        f"present_frame ms: p99 {s.present_dur_p99_ms:.2f}  max {s.present_dur_max_ms:.2f}",
        f"vblank lag ms (first wait of each): p50 {s.vblank_lag_p50_ms:.2f}  p99 {s.vblank_lag_p99_ms:.2f}  "
        f"max {s.vblank_lag_max_ms:.2f}  ({s.vblank_waits} waits, {s.vblanks_late} late, {s.vblanks_skipped} vblanks skipped)",
        f"histogram (ms): {bars}",
    ] + ([
        f"screen ({s.displays} shown): interval ms p50 {s.display_p50_ms:.2f}  p99 {s.display_p99_ms:.2f}  "
        f"max {s.display_max_ms:.2f}  share > 17.5 ms {100 * s.display_over_17_5_share:.1f}%",
        "screen histogram (ms): " + " ".join(f"{label}:{n}" for label, n in s.display_histogram),
    ] if s.displays > 1 else []))


def miss_report(b: dict) -> str:
    return (f"missed refreshes: {b['long_frames']} long frames (median waiting for the GE {b['long_wait_ms']:.1f} ms, "
            f"GE lists {b['long_ge_ms']:.1f} ms) vs {b['normal_frames']} normal ({b['normal_wait_ms']:.1f} ms, "
            f"{b['normal_ge_ms']:.1f} ms)")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Per-frame pacing summary of GT_FRAME_LOG files")
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--skip-s", type=float, default=0.0, help="ignore the first N seconds (loading)")
    parser.add_argument("--end-s", type=float, default=None, help="ignore everything from this second on")
    parser.add_argument("--json", action="store_true", help="print the summaries as JSON")
    args = parser.parse_args(argv)
    summaries = []
    for path in args.logs:
        try:
            log = window(parse(path.read_text()), args.skip_s, args.end_s)
            summary = summarize(log)
        except (OSError, ValueError) as error:
            print(f"{path}: {error}", file=sys.stderr)
            return 2
        summaries.append((path, summary, miss_breakdown(log) if log.spans else None))
    if args.json:
        print(json.dumps([{"file": str(p), **asdict(s), **({"miss_breakdown": b} if b else {})} for p, s, b in summaries],
                         indent=2))
    else:
        print("\n\n".join(report(s, p.name) + (f"\n{miss_report(b)}" if b else "") for p, s, b in summaries))
    return 0


if __name__ == "__main__":
    sys.exit(main())
