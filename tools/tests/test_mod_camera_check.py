import os

import pytest

from tools import mod_camera_check as mc

ORANGE = (210, 90, 40)
GREY = (90, 90, 90)
CYAN = (170, 228, 245)  # the speed digits


def ppm(pixels):
    return b"P6\n480 272\n255\n" + bytes(pixels)


def make_frame(car_at=None, hud=CYAN, sky=GREY, car_size=(60, 40)):
    """A 480x272 frame: grey picture, an orange car box centred at car_at, grey HUD strip with a block of speed digits in colour `hud`."""
    buf = bytearray()
    for y in range(mc.HEIGHT):
        for x in range(mc.WIDTH):
            colour = sky
            if y >= mc.HUD_TOP:
                colour = hud if 250 <= x < 290 and 240 <= y < 256 else GREY
            if car_at and y < mc.HUD_TOP and abs(x - car_at[0]) < car_size[0] // 2 and abs(y - car_at[1]) < car_size[1] // 2:
                colour = ORANGE
            buf += bytes(colour)
    return ppm(buf)


def write_run(tmp_path, label, frames):
    directory = tmp_path / label / "frames"
    directory.mkdir(parents=True)
    for index, data in frames.items():
        (directory / f"frame_{index:06d}.ppm").write_bytes(data)
    return mc.frames_of(str(directory))


def test_read_ppm_roundtrip_and_errors(tmp_path):
    path = tmp_path / "a.ppm"
    path.write_bytes(make_frame())
    w, h, pixels = mc.read_ppm(str(path))
    assert (w, h, len(pixels)) == (480, 272, 480 * 272 * 3)
    path.write_bytes(b"P5 1 1 255 x")
    with pytest.raises(ValueError):
        mc.read_ppm(str(path))
    path.write_bytes(b"P6\n480 272\n255\nshort")
    with pytest.raises(ValueError):
        mc.read_ppm(str(path))


def test_car_box_finds_the_centroid_and_ignores_the_hud():
    count, cx, cy, _w, _h = mc.car_box((480, 272, make_frame(car_at=(240, 140))[len(b"P6\n480 272\n255\n"):]))
    assert count > mc.CAR_MIN_PIXELS and abs(cx - 240) < 2 and abs(cy - 140) < 2
    assert mc.car_box((480, 272, make_frame()[len(b"P6\n480 272\n255\n"):]))[0] == 0
    # orange in the HUD strip does not count
    count, _, _, _, _ = mc.car_box((480, 272, make_frame(hud=ORANGE)[len(b"P6\n480 272\n255\n"):]))
    assert count == 0


def test_digits_mask_sees_the_digits_only():
    body = len(b"P6\n480 272\n255\n")
    assert len(mc.digits_mask((480, 272, make_frame()[body:]))) == 40 * 16
    assert mc.digits_mask((480, 272, make_frame(hud=GREY)[body:])) == []


def test_chase_frame_indices_from_the_log():
    log = "[frame-dump] frame 000030 vblank 100\n[pad] script done at vblank 1000\n[frame-dump] frame 000600 vblank 1100\n[frame-dump] frame 000660 vblank 1200\n"
    assert mc.chase_frame_indices(log) == [660]
    assert mc.chase_frame_indices("[frame-dump] frame 000030 vblank 100\n") == []


BROWN = (120, 70, 40)  # a red-brown track / dirt


def brown_ground(frame_bytes, y0=130, y1=232):
    """The frame with a brown ground over the whole width in rows y0..y1 (header kept)."""
    head = len(b"P6\n480 272\n255\n")
    buf = bytearray(frame_bytes)
    for y in range(y0, y1):
        for x in range(mc.WIDTH):
            i = head + (y * mc.WIDTH + x) * 3
            buf[i:i + 3] = bytes(BROWN)
    return bytes(buf)


def test_brown_ground_is_not_the_car(tmp_path):
    base = write_run(tmp_path, "base", baseline_frames())
    # a frame with NO car at all, only brown ground: must fail (count and extent far above the car's, car box not car-sized)
    no_car = {i: brown_ground(make_frame(sky=(30, 120, 30))) for i in baseline_frames()}
    ok, message = mc.judge_orbit(base, write_run(tmp_path, "nocar", no_car), INDICES)
    assert not ok and "not car-sized" in message
    # the car in place plus the brown ground: still not the car
    both = {i: brown_ground(make_frame(car_at=(250, 135), sky=(30, 120, 30))) for i in baseline_frames()}
    assert not mc.judge_orbit(base, write_run(tmp_path, "both", both), INDICES)[0]


def test_car_must_keep_its_size_and_place(tmp_path):
    base = write_run(tmp_path, "base", baseline_frames())
    tiny = {i: make_frame(car_at=(240, 140), car_size=(20, 12), sky=(30, 120, 30)) for i in baseline_frames()}
    assert "left the picture" in mc.judge_orbit(base, write_run(tmp_path, "tiny", tiny), INDICES)[1]
    off = {i: make_frame(car_at=(240 + 55, 140), sky=(30, 120, 30)) for i in baseline_frames()}  # 55 px aside: above the 45 px slack
    assert "moved" in mc.judge_orbit(base, write_run(tmp_path, "off", off), INDICES)[1]


def test_main_requires_the_yaw_zero_control(tmp_path, capsys):
    write_run(tmp_path, "baseline", baseline_frames())
    (tmp_path / "baseline" / "run.log").write_text("[pad] script done at vblank 1000\n" + "".join(f"[frame-dump] frame {i:06d} vblank {2000 + i}\n" for i in INDICES))
    write_run(tmp_path, "yaw_90", orbited())
    assert mc.main(["x", str(tmp_path), "yaw_90"]) == 1
    assert "determinism control" in capsys.readouterr().out


def test_diff_fraction(tmp_path):
    a = mc.read_ppm(str(_write(tmp_path, "a", make_frame())))
    b = mc.read_ppm(str(_write(tmp_path, "b", make_frame(car_at=(240, 140)))))
    assert mc.diff_fraction(a, a, 0, 272) == 0.0
    assert 0.0 < mc.diff_fraction(a, b, 0, mc.HUD_TOP) < 0.1
    assert mc.diff_fraction(a, b, mc.HUD_TOP, 272) == 0.0


def _write(tmp_path, name, data):
    path = tmp_path / f"{name}.ppm"
    path.write_bytes(data)
    return path


INDICES = [10 * i for i in range(1, 9)]


def baseline_frames():
    return {10 * i: make_frame(car_at=(240, 140)) for i in range(1, 9)}


def test_identity_passes_and_fails(tmp_path):
    base = write_run(tmp_path, "base", baseline_frames())
    same = write_run(tmp_path, "same", baseline_frames())
    assert mc.judge_identity(base, same)[0]
    changed = baseline_frames()
    changed[50] = make_frame(car_at=(300, 140))
    ok, message = mc.judge_identity(base, write_run(tmp_path, "changed", changed))
    assert not ok and "frame 50" in message
    assert not mc.judge_identity(base, write_run(tmp_path, "few", {10: make_frame()}))[0]


def orbited(car_at=(250, 135), hud=CYAN, sky=(30, 120, 30)):
    return {10 * i: make_frame(car_at=car_at, hud=hud, sky=sky) for i in range(1, 9)}


def test_orbit_passes_when_the_car_stays_centred(tmp_path):
    base = write_run(tmp_path, "base", {i: make_frame(car_at=(240, 140)) for i in range(10, 90, 10)})
    mod = write_run(tmp_path, "mod", orbited())
    ok, message = mc.judge_orbit(base, mod, INDICES)
    assert ok, message


def test_orbit_fails_when_the_car_leaves_the_picture(tmp_path):
    # sabotage shape: pivot at the eye, the car swings out of the picture
    base = write_run(tmp_path, "base", baseline_frames())
    mod = write_run(tmp_path, "mod", {i: make_frame(sky=(30, 120, 30)) for i in baseline_frames()})
    ok, message = mc.judge_orbit(base, mod, INDICES)
    assert not ok and "left the picture" in message


def test_orbit_fails_when_the_car_moves_off_centre(tmp_path):
    base = write_run(tmp_path, "base", baseline_frames())
    mod = write_run(tmp_path, "mod", orbited(car_at=(298, 140)))
    ok, message = mc.judge_orbit(base, mod, INDICES)
    assert not ok and "moved" in message


def test_orbit_fails_when_nothing_changed(tmp_path):
    base = write_run(tmp_path, "base", baseline_frames())
    mod = write_run(tmp_path, "mod", baseline_frames())
    ok, message = mc.judge_orbit(base, mod, INDICES)
    assert not ok and "did not orbit" in message


def test_orbit_fails_when_the_hud_changed(tmp_path):
    base = write_run(tmp_path, "base", baseline_frames())
    mod = write_run(tmp_path, "mod", orbited(hud=(10, 10, 200)))
    ok, message = mc.judge_orbit(base, mod, INDICES)
    assert not ok and "HUD" in message


def test_orbit_fails_without_a_car_in_the_baseline(tmp_path):
    base = write_run(tmp_path, "base", {i: make_frame() for i in range(10, 90, 10)})
    mod = write_run(tmp_path, "mod", orbited())
    ok, message = mc.judge_orbit(base, mod, INDICES)
    assert not ok and "baseline chase frames" in message


def test_orbit_fails_on_too_few_frames(tmp_path):
    base = write_run(tmp_path, "base", baseline_frames())
    mod = write_run(tmp_path, "mod", orbited())
    ok, message = mc.judge_orbit(base, mod, INDICES[:2])
    assert not ok and "only 2 chase frames" in message


def test_main_prints_verdicts_and_returns_status(tmp_path, capsys):
    write_run(tmp_path, "baseline", baseline_frames())
    (tmp_path / "baseline" / "run.log").write_text("[pad] script done at vblank 1000\n" + "".join(f"[frame-dump] frame {i:06d} vblank {2000 + i}\n" for i in INDICES))
    write_run(tmp_path, "yaw_0", baseline_frames())
    write_run(tmp_path, "yaw_90", orbited())
    assert mc.main(["x", str(tmp_path), "yaw_0", "yaw_90"]) == 0
    assert "PASS yaw_90" in capsys.readouterr().out
    write_run(tmp_path, "yaw_180", baseline_frames())
    assert mc.main(["x", str(tmp_path), "yaw_180"]) == 1
    assert "FAIL yaw_180" in capsys.readouterr().out


def test_judge_toggle(tmp_path):
    idx = [10 * i for i in range(1, 17)]
    base = write_run(tmp_path, "base", {i: make_frame(car_at=(240, 140)) for i in idx})
    mixed = {i: (make_frame(car_at=(250, 135), sky=(30, 120, 30)) if i <= 80 else make_frame(car_at=(240, 140))) for i in idx}  # 8 orbited, then baseline
    mod = write_run(tmp_path, "mod", mixed)
    log = "[mods] orbit-camera -> OFF\n"
    before, after = idx[:8], idx[9:]
    ok, message = mc.judge_toggle(base, mod, before, after, log)
    assert ok, message
    assert not mc.judge_toggle(base, mod, before, after, "")[0]  # no toggle line
    assert "still edits" in mc.judge_toggle(base, mod, before, [30] + after, log)[1]  # frame 30 is orbited
    assert "only 0/" in mc.judge_toggle(base, mod, idx[8:14], after, log)[1]  # nothing orbited before
    assert "too few" in mc.judge_toggle(base, mod, before[:5], after, log)[1]  # 5 frames before (< 6)
    assert "too few" in mc.judge_toggle(base, mod, before, after[:2], log)[1]


def test_frames_in_vblanks():
    log = "[frame-dump] frame 000030 vblank 100\n[frame-dump] frame 000060 vblank 200\n[frame-dump] frame 000090 vblank 300\n"
    assert mc.frames_in_vblanks(log, 150, 300) == [60]


def test_main_toggle_label(tmp_path, capsys):
    idx = [10 * i for i in range(1, 15)]
    frames = {i: make_frame(car_at=(240, 140)) for i in idx}
    write_run(tmp_path, "baseline", frames)
    (tmp_path / "baseline" / "run.log").write_text("[pad] script done at vblank 1000\n" + "".join(f"[frame-dump] frame {i:06d} vblank {1000 + 10 * i}\n" for i in idx))
    mixed = {i: (make_frame(car_at=(250, 135), sky=(30, 120, 30)) if i <= 90 else frames[i]) for i in idx}
    write_run(tmp_path, "toggle_90", mixed)
    (tmp_path / "toggle_90" / "run.log").write_text("[mods] orbit-camera -> OFF\n")
    # chase start = 1000 + 150; toggle at vblank 2000, margin 150: before = vblank in [1150, 1850) (frames 10..40... ), after >= 2150
    assert mc.main(["x", str(tmp_path), "toggle_90", "--toggle-at", "1900"]) == 0
    assert "PASS toggle_90" in capsys.readouterr().out


WHITE = (250, 250, 250)


def with_timer(frame_bytes, tenths):
    """The frame with a lap-timer reading drawn as white pixels in TIMER: `tenths` lit columns (two frames with different counts
    are different game frames)."""
    head = len(b"P6\n480 272\n255\n")
    buf = bytearray(frame_bytes)
    x0, _x1, y0, y1 = mc.TIMER
    for y in range(y0, y1):
        for x in range(x0, x0 + tenths):
            i = head + (y * mc.WIDTH + x) * 3
            buf[i:i + 3] = bytes(WHITE)
    return bytes(buf)


def test_same_game_frame_reads_the_lap_timer(tmp_path):
    a = mc.read_ppm(str(_write(tmp_path, "a", with_timer(make_frame(), 3))))
    b = mc.read_ppm(str(_write(tmp_path, "b", with_timer(make_frame(car_at=(240, 140)), 3))))
    c = mc.read_ppm(str(_write(tmp_path, "c", with_timer(make_frame(), 4))))
    assert mc.same_game_frame(a, b)          # the picture differs, the timer does not: same game frame
    assert not mc.same_game_frame(a, c)      # one step of pacing slip between two real-time runs


def test_identity_skips_a_pacing_slip_but_not_a_real_change(tmp_path):
    base_frames = {i: with_timer(make_frame(car_at=(240, 140)), i // 10) for i in INDICES}
    base = write_run(tmp_path, "base", base_frames)
    slipped = dict(base_frames)
    slipped[50] = with_timer(make_frame(car_at=(300, 140)), 9)  # the run slipped one step at frame 50: other timer, other picture
    assert mc.judge_identity(base, write_run(tmp_path, "slip", slipped))[0]
    changed = dict(base_frames)
    changed[50] = with_timer(make_frame(car_at=(300, 140)), 5)  # same game frame, different picture: the mod wrote something
    ok, message = mc.judge_identity(base, write_run(tmp_path, "changed", changed))
    assert not ok and "frame 50" in message


def test_identity_fails_when_most_frames_are_not_the_same_game_frame(tmp_path):
    # a vacuous pass is not a pass: a run whose timer never matches the baseline compares nothing
    base = write_run(tmp_path, "base", {i: with_timer(make_frame(), 1) for i in INDICES})
    other = write_run(tmp_path, "other", {i: with_timer(make_frame(car_at=(300, 140)), 2) for i in INDICES})
    ok, message = mc.judge_identity(base, other)
    assert not ok and "same game frame" in message


def test_orbit_skips_the_car_check_beside_the_car(tmp_path):
    # yaw 90: the camera sits beside the car, where the trackside wall may hide it; the car check runs from CAR_CENTRE_MIN_YAW up
    base = write_run(tmp_path, "base", baseline_frames())
    hidden = write_run(tmp_path, "hidden", {i: make_frame(sky=(30, 120, 30)) for i in baseline_frames()})
    ok, message = mc.judge_orbit(base, hidden, INDICES, yaw=90.0)
    assert ok, message
    assert not mc.judge_orbit(base, hidden, INDICES, yaw=180.0)[0]
    assert not mc.judge_orbit(base, write_run(tmp_path, "same", baseline_frames()), INDICES, yaw=90.0)[0]  # still must orbit


def test_toggle_tolerates_sprite_noise_but_not_an_orbit(tmp_path):
    idx = [10 * i for i in range(1, 17)]
    base = write_run(tmp_path, "base", {i: make_frame(car_at=(240, 140)) for i in idx})
    flicker = make_frame(car_at=(240, 140), car_size=(62, 40))  # a few hundred pixels at the car's edge: < TOGGLE_NOISE
    mixed = {i: (make_frame(car_at=(250, 135), sky=(30, 120, 30)) if i <= 80 else flicker) for i in idx}
    log = "[mods] orbit-camera -> OFF\n"
    ok, message = mc.judge_toggle(base, write_run(tmp_path, "mod", mixed), idx[:8], idx[9:], log)
    assert ok, message
    still = {i: make_frame(car_at=(250, 135), sky=(30, 120, 30)) for i in idx}
    assert "still edits" in mc.judge_toggle(base, write_run(tmp_path, "still", still), idx[:8], idx[9:], log)[1]


def test_frames_pair_by_present_vblank_when_the_log_has_it(tmp_path):
    # a dropped present shifts the dump numbers of one run: dump 20 of the mod run is the baseline's dump 30 (same present vblank)
    write_run(tmp_path, "b", {10: b"x", 20: b"y", 30: b"z"})
    log = "[frame-dump] frame 000010 vblank 101 present_vblank 100\n[frame-dump] frame 000020 vblank 205 present_vblank 200\n"
    keyed = mc.keyed_frames(str(tmp_path / "b" / "frames"), log)
    assert keyed == {100: str(tmp_path / "b" / "frames" / "frame_000010.ppm"), 200: str(tmp_path / "b" / "frames" / "frame_000020.ppm")}
    old_log = "[frame-dump] frame 000010 vblank 101\n"  # an old build's log: the dump numbers stay the keys
    assert set(mc.keyed_frames(str(tmp_path / "b" / "frames"), old_log)) == {10, 20, 30}
    assert set(mc.keyed_frames(str(tmp_path / "b" / "frames"), None)) == {10, 20, 30}  # no run.log


def test_indices_are_keys_when_the_log_has_present_vblanks():
    log = ("[frame-dump] frame 000030 vblank 101 present_vblank 100\n[pad] script done at vblank 1000\n"
           "[frame-dump] frame 000600 vblank 1101 present_vblank 1100\n[frame-dump] frame 000660 vblank 1203 present_vblank 1200\n")
    assert mc.chase_frame_indices(log) == [1200]
    assert mc.frames_in_vblanks(log, 1100, 1300) == [1100, 1200]


def _toggle_runs(tmp_path, base_keys_and_timers, mod_keys_and_timers):
    base = write_run(tmp_path, "tb", {k: with_timer(make_frame(car_at=(240, 140)), t) for k, t in base_keys_and_timers})
    # the first 8 mod frames (keys below 1000) are orbited, the rest show the baseline's picture
    mod = write_run(tmp_path, "tm", {k: (make_frame(car_at=(250, 135), sky=(30, 120, 30)) if k < 1000 else with_timer(make_frame(car_at=(240, 140)), t))
                                    for k, t in mod_keys_and_timers})
    return base, mod


def test_toggle_pairs_a_one_step_offset_frame_by_its_timer(tmp_path):
    before = [(100 + 10 * i, 1) for i in range(8)]
    base_after = [(2000, 5), (2002, 6), (2004, 7), (2006, 8), (2008, 9)]
    # the mod run is one step behind: at present vblank 2003 it shows the game frame the baseline showed at 2002, etc.
    mod_after = [(2003, 6), (2005, 7), (2007, 8)]
    base, mod = _toggle_runs(tmp_path, before + base_after, before + mod_after)
    ok, message = mc.judge_toggle(base, mod, [k for k, _ in before], [k for k, _ in mod_after], "[mods] orbit-camera -> OFF\n")
    assert ok, message
    assert "3/3 frames after it" in message


def test_toggle_does_not_pair_an_unrelated_frame_and_keeps_the_floor(tmp_path):
    before = [(100 + 10 * i, 1) for i in range(8)]
    base_after = [(2000, 5), (2002, 6), (2004, 7)]
    far_in_time = [(2003, 20), (2005, 21), (2007, 22)]   # timers the baseline never showed within PAIR_WINDOW
    base, mod = _toggle_runs(tmp_path, before + base_after, before + far_in_time)
    ok, message = mc.judge_toggle(base, mod, [k for k, _ in before], [k for k, _ in far_in_time], "[mods] orbit-camera -> OFF\n")
    assert not ok and "nothing proved" in message
    # a frame whose timer matches only a baseline frame outside the window does not pair either
    assert mc.pair_by_timer(base, 2003 + 2 * mc.PAIR_WINDOW, mc.read_ppm(mod[2003])) is None
    # ... and an offset frame that still differs from its partner (the mod keeps editing) is caught
    edited = write_run(tmp_path, "te", {k: (make_frame(car_at=(250, 135), sky=(30, 120, 30)) if k < 1000 else with_timer(make_frame(car_at=(250, 135), sky=(30, 120, 30)), t))
                                        for k, t in before + [(2003, 6), (2005, 7), (2007, 7)]})
    ok, message = mc.judge_toggle(base, edited, [k for k, _ in before], [2003, 2005, 2007], "[mods] orbit-camera -> OFF\n")
    assert not ok and "still edits" in message
