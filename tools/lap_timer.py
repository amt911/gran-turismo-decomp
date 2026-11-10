"""The race lap timer as the identifier of a game frame (shared by tools/mod_camera_check.py and tools/mod_drawdist_check.py).

Two real-time runs of one pad route can be a game step apart for the rest of the run (docs/FINDINGS.md, 2026-11-09); the lap timer
(1/60 s resolution) tells which game frame a dumped picture shows. It is drawn over the 3D picture without a backing: white digit
strokes with a dark outline. Everything behind it (bright sky, a white wall, a banner) changes with the scene, and the scene is
exactly what the mods change, so a mask of "bright pixels" would differ whenever the mod works and the gate would skip the frames
that matter most. The mask is therefore only the digit STROKES: white pixels in a short horizontal run (<= MAX_RUN_X) and a short
vertical run (<= MAX_RUN_Y) inside TIMER. A sky or a wall is a run as long as the box; a stroke is bounded by the glyph outline.

A timer is usable as an identifier only when the mask has at least MIN_PIXELS pixels (a menu, a loading screen has none) and is not
frozen (the same mask on the neighbouring dumped frame: the countdown before GO reads 0'00.000, a finished race stops it); frozen
or missing timers identify nothing, so they never count toward a floor and never pair.
"""

TIMER = (205, 295, 33, 50)  # x0, x1, y0, y1 of the lap timer
MIN_PIXELS = 30
MAX_RUN_X = 4
MAX_RUN_Y = 12
WHITE = 235  # every channel above this


def _runs_short(lit, width, height, along_x, limit):
    """Cells of `lit` (a flat width x height list of bools) that belong to a run of lit cells along x (or y) no longer than `limit`."""
    out = [False] * (width * height)
    outer, inner = (height, width) if along_x else (width, height)
    for o in range(outer):
        i = 0
        while i < inner:
            index = (o * width + i) if along_x else (i * width + o)
            if not lit[index]:
                i += 1
                continue
            j = i
            while j < inner and lit[(o * width + j) if along_x else (j * width + o)]:
                j += 1
            if j - i <= limit:
                for k in range(i, j):
                    out[(o * width + k) if along_x else (k * width + o)] = True
            i = j
    return out


def timer_mask(frame):
    """Sorted tuple of (x, y) of the digit-stroke pixels of the lap timer in `frame` = (width, height, rgb bytes)."""
    width, _height, pixels = frame
    x0, x1, y0, y1 = TIMER
    w, h = x1 - x0, y1 - y0
    lit = [min(pixels[((y0 + y) * width + x0 + x) * 3:((y0 + y) * width + x0 + x) * 3 + 3]) > WHITE for y in range(h) for x in range(w)]
    horizontal = _runs_short(lit, w, h, True, MAX_RUN_X)
    vertical = _runs_short(lit, w, h, False, MAX_RUN_Y)
    return tuple((x0 + i % w, y0 + i // w) for i in range(w * h) if horizontal[i] and vertical[i])


def run_timers(frames, read):
    """{key: mask or None} for a run's frames ({key: path}); `read(path)` gives the frame. None = the timer identifies nothing there
    (too few stroke pixels, or the same mask as a neighbouring dumped frame: frozen)."""
    keys = sorted(frames)
    masks = {key: timer_mask(read(frames[key])) for key in keys}
    out = {}
    for n, key in enumerate(keys):
        mask = masks[key]
        frozen = (n > 0 and masks[keys[n - 1]] == mask) or (n + 1 < len(keys) and masks[keys[n + 1]] == mask)
        out[key] = mask if len(mask) >= MIN_PIXELS and not frozen else None
    return out


def same_game_frame(a, b):
    """Two usable masks (not None) that are equal: both frames show the same game frame."""
    return a is not None and b is not None and a == b
