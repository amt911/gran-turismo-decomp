"""Synthetic lap-timer glyphs for the checker tests: `stamp(ppm bytes, value)` draws digit strokes (white, 2 px wide, 11 px tall, black outline) in
the TIMER box; two different values give two different masks, two equal values equal ones; two strokes are always lit so the mask is usable."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import lap_timer as lt  # noqa: E402

HEAD = len(b"P6\n480 272\n255\n")
WIDTH = 480


def _put(buf, x, y, colour):
    i = HEAD + (y * WIDTH + x) * 3
    buf[i:i + 3] = bytes(colour)


def stamp(data, value):
    buf = bytearray(data)
    x0, _x1, y0, _y1 = lt.TIMER
    columns = [n for n in range(10) if (value >> n) & 1] + [10, 11]
    for n in columns:
        gx = x0 + 10 + 7 * n
        for y in range(y0 + 1, y0 + 14):
            for x in range(gx - 1, gx + 3):
                _put(buf, x, y, (0, 0, 0))
        for y in range(y0 + 2, y0 + 13):
            for x in range(gx, gx + 2):
                _put(buf, x, y, (255, 255, 255))
    return bytes(buf)


def has_timer(data):
    width, height = 480, 272
    return len(lt.timer_mask((width, height, data[HEAD:]))) >= lt.MIN_PIXELS
