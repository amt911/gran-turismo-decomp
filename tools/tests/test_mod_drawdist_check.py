import pytest

from tools import mod_drawdist_check as mc
from tools.tests.timer_fixture import has_timer, stamp

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
                if far_block and 20 <= y < 24 and x < 100:
                    colour = LIGHT
                if near_block and mc.NEAR_ROWS[0] + 5 <= y < mc.NEAR_ROWS[0] + 10 and x < 60:
                    colour = LIGHT
            buf += bytes(colour)
    return HEADER + bytes(buf)


def run(tmp_path, label, frames):
    directory = tmp_path / label
    directory.mkdir(parents=True, exist_ok=True)
    for index, data in frames.items():
        (directory / f"frame_{index:06d}.ppm").write_bytes(data if has_timer(data) else stamp(data, index))
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
    n = 50
    base = run(tmp_path, "b", {i: make_frame() for i in range(1, n + 1)})
    mod = {i: make_frame(far_block=True) if i <= 6 else make_frame() for i in range(1, n + 1)}
    before, after = [1, 2, 3, 4], list(range(10, n + 1))
    log = "[mods] draw-distance -> OFF\n"
    ok, message = mc.judge_toggle(base, run(tmp_path, "m", mod), before, after, log)
    assert ok, message
    assert "no `[mods]" in mc.judge_toggle(base, run(tmp_path, "m", mod), before, after, "")[1]
    assert "too few" in mc.judge_toggle(base, run(tmp_path, "m", mod), [1], after, log)[1]
    assert "too few" in mc.judge_toggle(base, run(tmp_path, "m", mod), before, after[:mc.MIN_TOGGLE_AFTER - 1], log)[1]  # a floor, not 3 frames
    still = {i: make_frame(far_block=True) for i in range(1, n + 1)}
    assert "still edits" in mc.judge_toggle(base, run(tmp_path, "s", still), before, after, log)[1]
    never = {i: make_frame() for i in range(1, n + 1)}
    assert "only 0/4" in mc.judge_toggle(base, run(tmp_path, "n", never), before, after, log)[1]


MOD_LINES = "[mod:draw-distance] far_scale=2.500\n[mod:draw-distance] world depth call from ra=00000160\n"


def make_out(tmp_path, with_mod_lines=True):
    n = 60
    plain = {i: make_frame() for i in range(1, n + 1)}
    out = tmp_path / "out"
    for label, frames in (("baseline", plain), ("scale_1", plain), ("scale_2.5", {i: make_frame(far_block=True) for i in range(1, n + 1)}),
                          ("toggle_2.5", {i: make_frame(far_block=True) if i <= 20 else make_frame() for i in range(1, n + 1)})):
        directory = out / label / "frames"
        directory.mkdir(parents=True)
        for index, data in frames.items():
            (directory / f"frame_{index:06d}.ppm").write_bytes(stamp(data, index))
        if label != "baseline":
            extra = "[mods] draw-distance -> OFF\n" if label.startswith("toggle_") else ""
            (out / label / "run.log").write_text((MOD_LINES if with_mod_lines else "") + extra)
    (out / "baseline" / "run.log").write_text("[pad] script done at vblank 0\n" + "".join(f"[frame-dump] frame {i} vblank {150 + i * 40}\n" for i in range(1, n + 1)))
    return out


def test_judge_mod_loaded():
    assert mc.judge_mod_loaded(MOD_LINES)[0]
    assert "not loaded" in mc.judge_mod_loaded("[mods] loaded: x\n")[1]
    assert "never matched" in mc.judge_mod_loaded("[mod:draw-distance] far_scale=1.000\n")[1]


def test_main_end_to_end(tmp_path, capsys):
    out = make_out(tmp_path)
    assert mc.main(["x", str(out), "scale_1", "scale_2.5"]) == 0
    assert mc.main(["x", str(out), "toggle_2.5", "--toggle-at", "1000"]) == 0
    assert capsys.readouterr().out.count("PASS") == 3
    (out / "scale_2.5" / "frames" / "frame_000010.ppm").write_bytes(stamp(make_frame(), 10))
    assert mc.main(["x", str(out), "scale_2.5"]) == 0  # all but one frame still change: above FAR_MIN_FRAMES
    assert mc.main(["x", str(out), "scale_1"]) == 0
    (out / "scale_1" / "frames" / "frame_000010.ppm").write_bytes(stamp(make_frame(far_block=True), 10))
    assert mc.main(["x", str(out), "scale_1"]) == 1


def test_main_fails_when_the_mod_did_not_load(tmp_path, capsys):
    out = make_out(tmp_path, with_mod_lines=False)
    assert mc.main(["x", str(out), "scale_1"]) == 1  # byte-identical frames alone are not a pass
    assert "not loaded" in capsys.readouterr().out


def test_main_errors_on_no_labels(tmp_path, capsys):
    out = make_out(tmp_path)
    assert mc.main(["x", str(out)]) == 2
    assert mc.main(["x", str(out), "--toggle-at", "1750"]) == 2
    assert "no run labels" in capsys.readouterr().err


def test_main_toggle_without_script_done_is_a_verdict_not_a_traceback(tmp_path, capsys):
    out = make_out(tmp_path)
    (out / "baseline" / "run.log").write_text("[frame-dump] frame 1 vblank 550\n")
    assert mc.main(["x", str(out), "toggle_2.5", "--toggle-at", "1000"]) == 1
    assert "script done" in capsys.readouterr().out
    (out / "baseline" / "run.log").write_text("[pad] script done at vblank 0\n")
    assert mc.main(["x", str(out), "toggle_2.5"]) == 1  # no --toggle-at either


WHITE = (250, 250, 250)


def with_timer(frame_bytes, tenths):
    """The frame with a lap-timer reading `tenths` (two different readings are different game frames)."""
    return stamp(frame_bytes, 1 + tenths)


def test_a_pacing_slip_is_not_judged_but_a_real_change_is(tmp_path):
    base = run(tmp_path, "base", {i: with_timer(make_frame(), i) for i in range(1, 9)})
    far = {i: with_timer(make_frame(far_block=True), i) for i in range(1, 9)}
    far[8] = with_timer(make_frame(car_at=(300, 120), sky=LIGHT), 20)  # one step later: another picture, not judged
    ok, message = mc.judge_far(base, run(tmp_path, "slip", far), list(range(1, 9)))
    assert ok, message
    far[8] = with_timer(make_frame(car_at=(300, 120), sky=LIGHT), 8)  # same game frame, another picture: judged, fails
    assert not mc.judge_far(base, run(tmp_path, "real", far), list(range(1, 9)))[0]


def test_far_and_identity_fail_when_no_frame_is_the_same_game_frame(tmp_path):
    base = run(tmp_path, "base", {i: with_timer(make_frame(), 1) for i in range(1, 9)})
    other = run(tmp_path, "other", {i: with_timer(make_frame(far_block=True), 2) for i in range(1, 9)})
    ok, message = mc.judge_far(base, other, list(range(1, 9)))
    assert not ok and "same game frame" in message
    ok, message = mc.judge_identity(base, other)
    assert not ok and "same game frame" in message


def test_identity_skips_a_slipped_frame(tmp_path):
    frames = {i: with_timer(make_frame(), i) for i in range(1, 9)}
    base = run(tmp_path, "base", frames)
    slipped = dict(frames)
    slipped[8] = with_timer(make_frame(car_at=(300, 120)), 20)
    assert mc.judge_identity(base, run(tmp_path, "slip", slipped))[0]


def test_toggle_skips_slipped_frames_after_it(tmp_path):
    idx = list(range(1, 61))
    base = run(tmp_path, "base", {i: with_timer(make_frame(), i) for i in idx})
    mixed = {i: with_timer(make_frame(far_block=True) if i <= 6 else make_frame(), i) for i in idx}
    mixed[50] = with_timer(make_frame(car_at=(300, 120)), 300)  # slipped after the toggle: not judged
    log = "[mods] draw-distance -> OFF\n"
    ok, message = mc.judge_toggle(base, run(tmp_path, "mod", mixed), idx[:6], idx[10:], log)
    assert ok, message
    mixed[40] = with_timer(make_frame(far_block=True), 40)  # same game frame and still edited
    assert "still edits" in mc.judge_toggle(base, run(tmp_path, "still", mixed), idx[:6], idx[10:], log)[1]


def test_toggle_floor_is_a_share_and_frozen_timers_count_for_nothing(tmp_path):
    idx = list(range(1, 101))
    base = run(tmp_path, "base", {i: with_timer(make_frame(), i) for i in idx})
    log = "[mods] draw-distance -> OFF\n"
    # 100 frames after the toggle but only 35 on the baseline's game frames (the rest of the run is far off): above the count, below the share
    off = {i: with_timer(make_frame(far_block=True) if i <= 6 else make_frame(), i if i < 45 else 500 + i) for i in idx}
    ok, message = mc.judge_toggle(base, run(tmp_path, "off", off), idx[:6], idx[8:], log)
    assert not ok and "nothing proved" in message
    frozen = {i: with_timer(make_frame(far_block=True) if i <= 6 else make_frame(), 7) for i in idx}   # a stopped timer identifies nothing
    ok, message = mc.judge_toggle(run(tmp_path, "fb", {i: with_timer(make_frame(), 7) for i in idx}), run(tmp_path, "fr", frozen), idx[:6], idx[8:], log)
    assert not ok and ("nothing proved" in message or "only 0/" in message)


def test_identity_does_not_count_a_frozen_or_missing_timer(tmp_path):
    frames = {i: with_timer(make_frame(), 7) for i in range(1, 12)}      # the same reading on every frame
    ok, message = mc.judge_identity(run(tmp_path, "a", frames), run(tmp_path, "b", frames))
    assert not ok and "nothing proved" in message


def test_race_frame_selection_uses_the_present_vblank():
    log = "[pad] script done at vblank 0\n[frame-dump] frame 000001 vblank 900 present_vblank 100\n[frame-dump] frame 000002 vblank 200 present_vblank 400\n"
    assert mc.race_frame_indices(log) == [400]
    assert mc.frames_in_vblanks(log, 300, 500) == [400]


def test_frames_pair_by_present_vblank_when_the_log_has_it(tmp_path):
    (tmp_path / "b").mkdir()
    for n in (1, 2):
        (tmp_path / "b" / f"frame_{n:06d}.ppm").write_bytes(b"x")  # only the names matter here
    log = "[frame-dump] frame 000001 vblank 101 present_vblank 100\n[frame-dump] frame 000002 vblank 205 present_vblank 200\n"
    assert set(mc.keyed_frames(str(tmp_path / "b"), log)) == {100, 200}
    assert set(mc.keyed_frames(str(tmp_path / "b"), "[frame-dump] frame 000001 vblank 101\n")) == {1, 2}
    log += "[pad] script done at vblank 0\n"
    assert mc.race_frame_indices("[pad] script done at vblank 0\n" + log) == [200]
