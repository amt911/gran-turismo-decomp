"""tools/mod_draw_check.py: the verdicts on synthetic frames (the probe's shapes drawn where the check looks, a frame without them, a graded copy)."""
import os

from tools import mod_draw_check as mdc
from tools.tests.timer_fixture import stamp

HEAD = b"P6\n480 272\n255\n"
GAME = (100, 120, 140)


def frame(shapes=True, text=True, red=(255, 0, 0), game=GAME):
    """A 480x272 picture of one grey-blue colour, with the probe's shapes drawn as the host would draw them."""
    px = [list(game) for _ in range(480 * 272)]

    def put(x, y, c):
        px[y * 480 + x] = list(c)
    if shapes:
        x0, y0, x1, y1 = mdc.RED_RECT
        for y in range(y0, y1):
            for x in range(x0, x1):
                put(x, y, red)
        x0, y0, x1, y1 = mdc.BLUE_RECT
        for y in range(y0, y1):
            for x in range(x0, x1):
                put(x, y, (game[0] // 2, game[1] // 2, 127 + game[2] // 2))
        (ax, ay), (bx, by) = mdc.LINE
        for step in range(0, 141):
            x = int(ax + step)
            y = int(round(ay + (by - ay) * step / (bx - ax)))
            for yy in (y - 1, y, y + 1):
                put(x, yy, (0, 255, 0))
    if text:
        x0, y0, _x1, _y1 = mdc.TEXT_BOX
        for i in range(mdc.MIN_TEXT_PIXELS + 40):
            put(x0 + 4 + i % 100, y0 + 6 + i // 100, (255, 255, 255))
    return HEAD + bytes(v for p in px for v in p)


def write_run(tmp_path, label, frames, log):
    directory = tmp_path / label / "frames"
    directory.mkdir(parents=True)
    for index, data in frames.items():
        (directory / f"frame_{index:06d}.ppm").write_bytes(data)
    (tmp_path / label / "run.log").write_text(log)


def log_for(indices, done=100):
    return f"[pad] script done at vblank {done}\n" + "".join(f"[frame-dump] frame {i:06d} vblank {done + 200 + i * 30} present_vblank {done + 200 + i * 30}\n" for i in indices)


def read(data):
    return 480, 272, data[len(HEAD):]


def test_a_correct_picture_has_no_problem():
    assert mdc.draw_frame_problem(read(frame()), read(frame(shapes=False, text=False))) is None


def test_each_shape_is_checked():
    base = read(frame(shapes=False, text=False))
    assert "red rect" in mdc.draw_frame_problem(read(frame(red=(250, 0, 0))), base)
    assert "red rect" in mdc.draw_frame_problem(read(frame(shapes=False)), base)
    no_text = frame(text=False)
    assert "text" in mdc.draw_frame_problem(read(no_text), base)
    # a baseline that already has near-white pixels in the text box: the text must add its own
    white = (255, 255, 255)
    assert "text" in mdc.draw_frame_problem(read(frame(game=white, text=False)), read(frame(shapes=False, text=False, game=white)))


def test_judge_draw_counts_the_good_frames(tmp_path):
    indices = list(range(10))
    log = log_for(indices)
    write_run(tmp_path, "baseline", {i: frame(shapes=False, text=False) for i in indices}, log)
    write_run(tmp_path, "draw", {i: frame() for i in indices}, log)
    from tools import mod_camera_check as cam
    base = cam.keyed_frames(str(tmp_path / "baseline" / "frames"), log)
    mod = cam.keyed_frames(str(tmp_path / "draw" / "frames"), log)
    keys = sorted(mod)
    ok, message = mdc.judge_draw(base, mod, keys)
    assert ok and "10/10" in message
    broken = {i: (frame() if i < 5 else frame(shapes=False)) for i in indices}
    write_run(tmp_path, "draw2", broken, log)
    mod2 = cam.keyed_frames(str(tmp_path / "draw2" / "frames"), log)
    ok, message = mdc.judge_draw(base, mod2, keys)
    assert not ok and "first problem" in message
    ok, message = mdc.judge_draw(base, mod, keys[:2])
    assert not ok and "only 2" in message


def timed(data, n):
    return stamp(data, n)


def test_judge_grade_pairs_by_lap_timer(tmp_path):
    from tools import mod_camera_check as cam
    indices = list(range(10))
    log = log_for(indices)
    base_frames = {i: timed(frame(shapes=False, text=False, game=(100 + i, 120, 140)), i + 1) for i in indices}
    def halve(data):  # what scene_grade(0.5, ...) does to a whole picture, lap timer included
        return HEAD + bytes(int(v * 0.5 + 0.5) for v in data[len(HEAD):])
    halved = {i: halve(base_frames[i]) for i in indices}
    write_run(tmp_path, "baseline", base_frames, log)
    write_run(tmp_path, "grade", halved, log)
    write_run(tmp_path, "nograde", base_frames, log)
    base = cam.keyed_frames(str(tmp_path / "baseline" / "frames"), log)
    keys = sorted(base)
    ok, message = mdc.judge_grade(base, cam.keyed_frames(str(tmp_path / "grade" / "frames"), log), keys)
    assert ok, message
    ok, message = mdc.judge_grade(base, cam.keyed_frames(str(tmp_path / "nograde" / "frames"), log), keys)
    assert not ok and "half the baseline" in message
    ok, message = mdc.judge_grade(base, cam.keyed_frames(str(tmp_path / "grade" / "frames"), log), keys[:2])
    assert not ok and "nothing proved" in message


def test_main_verdict_and_exit_code(tmp_path, capsys):
    indices = list(range(10))
    log = log_for(indices)
    write_run(tmp_path, "baseline", {i: timed(frame(shapes=False, text=False), i + 1) for i in indices}, log)
    write_run(tmp_path, "idle", {i: timed(frame(shapes=False, text=False), i + 1) for i in indices}, log)
    write_run(tmp_path, "draw", {i: timed(frame(), i + 1) for i in indices}, log)
    assert mdc.main(["x", str(tmp_path), "idle", "draw"]) == 0
    out = capsys.readouterr().out
    assert "PASS idle" in out and "PASS draw" in out
    write_run(tmp_path, "bogus", {}, log)
    assert mdc.main(["x", str(tmp_path), "bogus"]) == 1
    assert "FAIL bogus: unknown label" in capsys.readouterr().out
    assert mdc.read_log(str(tmp_path / "missing.log")) == ""
