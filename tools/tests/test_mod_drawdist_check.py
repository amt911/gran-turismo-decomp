import pytest

from tools import mod_drawdist_check as mc

ORANGE = (210, 90, 40)
GREY = (90, 90, 90)
LIGHT = (200, 200, 200)
CYAN = (170, 228, 245)
HEADER = b"P6\n480 272\n255\n"


def make_frame(car_at=(240, 150), hud=CYAN, far_block=False, near_block=False, car_size=(60, 40), sky=GREY):
    """480x272: grey picture, orange car, speed digits; far_block lightens a band near the horizon, near_block one near the road."""
    buf = bytearray()
    for y in range(mc.HEIGHT):
        for x in range(mc.WIDTH):
            colour = sky
            if y >= mc.HUD_TOP:
                colour = hud if 250 <= x < 290 and 240 <= y < 256 else GREY
            else:
                if car_at and abs(x - car_at[0]) < car_size[0] // 2 and abs(y - car_at[1]) < car_size[1] // 2:
                    colour = ORANGE
                if far_block and 20 <= y < 40:
                    colour = LIGHT
                if near_block and mc.NEAR_ROWS[0] + 5 <= y < mc.NEAR_ROWS[0] + 25 and x < 400:
                    colour = LIGHT
            buf += bytes(colour)
    return HEADER + bytes(buf)


def run(tmp_path, label, frames):
    directory = tmp_path / label
    directory.mkdir(parents=True, exist_ok=True)
    for index, data in frames.items():
        (directory / f"frame_{index:06d}.ppm").write_bytes(data)
    return mc.frames_of(str(directory))


def seven(**kw):
    return {i: make_frame(**kw) for i in range(1, 8)}


def test_read_ppm_roundtrip_and_errors(tmp_path):
    path = tmp_path / "a.ppm"
    path.write_bytes(make_frame())
    assert mc.read_ppm(str(path))[:2] == (480, 272)
    path.write_bytes(b"P5 1 1 255 x")
    with pytest.raises(ValueError):
        mc.read_ppm(str(path))
    path.write_bytes(HEADER + b"short")
    with pytest.raises(ValueError):
        mc.read_ppm(str(path))


def test_car_box_digits_and_diff_fraction(tmp_path):
    frame = (480, 272, make_frame()[len(HEADER):])
    count, cx, cy = mc.car_box(frame)
    assert count > mc.CAR_MIN_PIXELS and abs(cx - 240) < 2 and abs(cy - 150) < 2
    assert mc.car_box((480, 272, make_frame(car_at=None)[len(HEADER):]))[0] == 0
    assert len(mc.digits_mask(frame)) == 40 * 16
    assert mc.digits_mask((480, 272, make_frame(hud=GREY)[len(HEADER):])) == []
    far = (480, 272, make_frame(far_block=True)[len(HEADER):])
    assert mc.diff_fraction(frame, frame, 0, 272) == 0.0
    assert 0.0 < mc.diff_fraction(frame, far, 0, mc.HUD_TOP) < 0.1
    assert mc.diff_fraction(frame, far, *mc.NEAR_ROWS) == 0.0


def test_log_parsing():
    log = "[frame-dump] frame 30 vblank 100\n[pad] script done at vblank 1000\n[frame-dump] frame 600 vblank 1100\n[frame-dump] frame 660 vblank 1200\n"
    assert mc.race_frame_indices(log) == [660]
    assert mc.race_frame_indices("[frame-dump] frame 30 vblank 100\n") == []
    assert mc.frames_in_vblanks(log, 1000, 1150) == [600]


def test_identity(tmp_path):
    base = run(tmp_path, "b", seven())
    assert mc.judge_identity(base, run(tmp_path, "m", seven()))[0]
    changed = seven()
    changed[3] = make_frame(far_block=True)
    ok, message = mc.judge_identity(base, run(tmp_path, "m2", changed))
    assert not ok and "frame 3" in message
    assert not mc.judge_identity(base, run(tmp_path, "m3", {1: make_frame()}))[0]


def test_far_passes_when_only_the_far_band_changes(tmp_path):
    base = run(tmp_path, "b", seven())
    ok, message = mc.judge_far(base, run(tmp_path, "m", seven(far_block=True)), list(range(1, 8)))
    assert ok, message


def test_far_fails_when_nothing_changes(tmp_path):
    base = run(tmp_path, "b", seven())
    ok, message = mc.judge_far(base, run(tmp_path, "m", seven()), list(range(1, 8)))
    assert not ok and "did not move" in message


def test_far_fails_when_the_near_field_changes(tmp_path):
    base = run(tmp_path, "b", seven())
    ok, message = mc.judge_far(base, run(tmp_path, "m", seven(far_block=True, near_block=True)), list(range(1, 8)))
    assert not ok and "near field" in message


def test_far_fails_when_the_car_or_hud_change(tmp_path):
    base = run(tmp_path, "b", seven())
    ok, message = mc.judge_far(base, run(tmp_path, "m", seven(far_block=True, car_at=(300, 170))), list(range(1, 8)))
    assert not ok and "car" in message
    ok, message = mc.judge_far(base, run(tmp_path, "m2", seven(far_block=True, hud=GREY)), list(range(1, 8)))
    assert not ok and "digits" in message
    ok, message = mc.judge_far(base, run(tmp_path, "m3", seven(far_block=True, car_size=(30, 20))), list(range(1, 8)))
    assert not ok and "car changed" in message


def test_far_fails_on_a_different_picture_and_on_too_few_frames(tmp_path):
    base = run(tmp_path, "b", seven())
    ok, message = mc.judge_far(base, run(tmp_path, "m", seven(sky=LIGHT, car_at=(240, 150))), list(range(1, 8)))
    assert not ok and "different picture" in message
    assert "only 2" in mc.judge_far(base, base, [1, 2])[1]
    nocar = run(tmp_path, "nc", seven(car_at=None))
    ok, message = mc.judge_far(nocar, run(tmp_path, "nc2", seven(car_at=None, far_block=True)), list(range(1, 8)))
    assert not ok and "car is visible" in message


def test_toggle(tmp_path):
    base = run(tmp_path, "b", seven())
    mod = seven(far_block=True)
    for i in (5, 6, 7):
        mod[i] = make_frame()
    log = "[mods] draw-distance -> OFF\n"
    ok, message = mc.judge_toggle(base, run(tmp_path, "m", mod), [1, 2, 3, 4], [5, 6, 7], log)
    assert ok, message
    assert "no `[mods]" in mc.judge_toggle(base, run(tmp_path, "m", mod), [1, 2, 3, 4], [5, 6, 7], "")[1]
    assert "too few" in mc.judge_toggle(base, run(tmp_path, "m", mod), [1], [5, 6, 7], log)[1]
    still = seven(far_block=True)
    assert "still edits" in mc.judge_toggle(base, run(tmp_path, "s", still), [1, 2, 3, 4], [5, 6, 7], log)[1]
    never = seven()
    assert "only 0/4" in mc.judge_toggle(base, run(tmp_path, "n", never), [1, 2, 3, 4], [5, 6, 7], log)[1]


def test_main_end_to_end(tmp_path, capsys):
    out = tmp_path / "out"
    for label, frames in (("baseline", seven()), ("scale_1", seven()), ("scale_2.5", seven(far_block=True)), ("toggle_2.5", {**seven(far_block=True), **{i: make_frame() for i in (5, 6, 7)}})):
        directory = out / label / "frames"
        directory.mkdir(parents=True)
        for index, data in frames.items():
            (directory / f"frame_{index:06d}.ppm").write_bytes(data)
    (out / "baseline" / "run.log").write_text("[pad] script done at vblank 0\n" + "".join(f"[frame-dump] frame {i} vblank {150 + i * 400}\n" for i in range(1, 8)))
    (out / "toggle_2.5" / "run.log").write_text("[mods] draw-distance -> OFF\n")
    assert mc.main(["x", str(out), "scale_1", "scale_2.5"]) == 0
    assert mc.main(["x", str(out), "toggle_2.5", "--toggle-at", "1750"]) == 0
    text = capsys.readouterr().out
    assert text.count("PASS") == 3
    (out / "scale_2.5" / "frames" / "frame_000002.ppm").write_bytes(make_frame())
    assert mc.main(["x", str(out), "scale_2.5"]) == 0  # 6 of 7 frames still change: above FAR_MIN_FRAMES
    assert mc.main(["x", str(out), "scale_1"]) == 0
    (out / "scale_1" / "frames" / "frame_000002.ppm").write_bytes(make_frame(far_block=True))
    assert mc.main(["x", str(out), "scale_1"]) == 1
