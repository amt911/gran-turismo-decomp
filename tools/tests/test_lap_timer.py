"""tools/lap_timer.py: the lap timer as the identifier of a game frame. Synthetic frames only."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import lap_timer as lt  # noqa: E402

WIDTH, HEIGHT = 480, 272


def frame(strokes, background=(60, 80, 100), outline=True, glyph_columns=None):
    """A frame with `strokes` vertical digit strokes (2 px wide, 11 px tall, white, black outline) in the timer box over `background`;
    `background` may be a function (x, y) -> colour."""
    pixels = bytearray()
    x0, _x1, y0, _y1 = lt.TIMER
    cells = {}
    for n in range(strokes):
        gx = x0 + 10 + 7 * (glyph_columns[n] if glyph_columns else n)
        for y in range(y0 + 2, y0 + 13):
            for x in range(gx, gx + 2):
                cells[(x, y)] = (255, 255, 255)
            if outline:
                cells[(gx - 1, y)] = (0, 0, 0)
                cells[(gx + 2, y)] = (0, 0, 0)
        if outline:
            for x in range(gx - 1, gx + 3):
                cells[(x, y0 + 1)] = (0, 0, 0)
                cells[(x, y0 + 13)] = (0, 0, 0)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            colour = cells.get((x, y)) or (background(x, y) if callable(background) else background)
            pixels += bytes(colour)
    return (WIDTH, HEIGHT, bytes(pixels))


def test_the_mask_is_the_digit_strokes_whatever_is_behind_the_timer():
    dark = lt.timer_mask(frame(5, background=(10, 10, 10)))
    sky = lt.timer_mask(frame(5, background=(250, 250, 250)))            # a white sky / wall behind the whole box
    banner = lt.timer_mask(frame(5, background=lambda x, y: (255, 255, 255) if (x // 20) % 2 else (20, 20, 20)))  # a banner with big blocks
    gradient = lt.timer_mask(frame(5, background=lambda x, y: (200 + x % 56, 230 + y % 26, 240)))  # a bright shaded sky
    assert len(dark) >= lt.MIN_PIXELS
    assert dark == sky == banner == gradient


def test_a_bright_scene_alone_is_no_timer():
    assert lt.timer_mask(frame(0, background=(250, 250, 250))) == ()      # no strokes: a menu, a loading screen
    assert lt.timer_mask(frame(0, background=lambda x, y: (255, 255, 255) if x % 9 < 2 else (0, 0, 0))) == ()  # thin vertical railings taller than a glyph


def test_different_digits_are_different_masks():
    assert lt.timer_mask(frame(5)) != lt.timer_mask(frame(5, glyph_columns=[0, 1, 2, 3, 5]))


def test_run_timers_rejects_empty_and_frozen_timers(tmp_path):
    frames = {10: frame(0), 20: frame(5), 30: frame(5), 40: frame(5, glyph_columns=[0, 1, 2, 3, 5]), 50: frame(5, glyph_columns=[0, 1, 2, 4, 5]),
              60: frame(5, glyph_columns=[0, 1, 3, 4, 5])}
    timers = lt.run_timers({k: k for k in frames}, lambda k: frames[k])
    assert timers[10] is None                       # no timer on screen
    assert timers[20] is None and timers[30] is None  # the same mask twice in a row: frozen
    assert timers[40] is not None and timers[50] is not None and timers[60] is not None


def test_same_game_frame_needs_two_usable_equal_masks():
    mask = lt.timer_mask(frame(5))
    assert lt.same_game_frame(mask, mask)
    assert not lt.same_game_frame(None, None)
    assert not lt.same_game_frame(mask, None)
    assert not lt.same_game_frame(mask, lt.timer_mask(frame(5, glyph_columns=[0, 1, 2, 3, 5])))
