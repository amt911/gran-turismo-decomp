import os

import pytest

from tools import mod_camera_check as mc
from tools.tests.timer_fixture import has_timer, stamp

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
        (directory / f"frame_{index:06d}.ppm").write_bytes(data if has_timer(data) else stamp(data, index))  # a frame without a timer shows the game frame `index`
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
    write_run(tmp_path, "yaw_180", orbited())
    assert mc.main(["x", str(tmp_path), "yaw_0", "yaw_90", "yaw_180"]) == 0
    assert "PASS yaw_90" in capsys.readouterr().out
    write_run(tmp_path, "yaw_135", baseline_frames())
    assert mc.main(["x", str(tmp_path), "yaw_135"]) == 1
    assert "FAIL yaw_135" in capsys.readouterr().out


def test_frames_in_vblanks():
    log = "[frame-dump] frame 000030 vblank 100\n[frame-dump] frame 000060 vblank 200\n[frame-dump] frame 000090 vblank 300\n"
    assert mc.frames_in_vblanks(log, 150, 300) == [60]


WHITE = (250, 250, 250)


def with_timer(frame_bytes, tenths):
    """The frame with a lap-timer reading `tenths` (two different readings are different game frames)."""
    return stamp(frame_bytes, 1 + tenths)


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


def test_frames_pair_by_present_vblank_when_the_log_has_it(tmp_path):
    # a dropped present shifts the dump numbers of one run: dump 20 of the mod run is the baseline's dump 30 (same present vblank)
    (tmp_path / "b" / "frames").mkdir(parents=True)
    for n in (10, 20, 30):
        (tmp_path / "b" / "frames" / f"frame_{n:06d}.ppm").write_bytes(b"x")  # not frames: only the names matter here
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


TOGGLE_LOG = "[mods] orbit-camera -> OFF\n"
BEFORE = [200 + 10 * i for i in range(8)]
CAR = (240, 140)


def toggle_runs(tmp_path, count=40, lag=0, mod_picture=None, base_picture=None, mod_timer=None):
    """A baseline and a toggled run. Before the toggle the run is orbited (green sky); after it, `count` frames at present vblanks 2000, 2002, ...: the baseline
    shows game frame i + 1 at 2000 + 2i, the run shows game frame i + 1 - lag there (lag 1 = a step behind), or `mod_timer(i)`'s reading; the pictures
    are `base_picture` / `mod_picture` (default: both the plain car picture)."""
    base_picture = base_picture or (lambda i: make_frame(car_at=CAR))
    mod_picture = mod_picture or (lambda i: make_frame(car_at=CAR))
    base = {k: make_frame(car_at=CAR) for k in BEFORE}
    mod = {k: make_frame(car_at=(250, 135), sky=(30, 120, 30)) for k in BEFORE}
    for i in range(count):
        base[2000 + 2 * i] = with_timer(base_picture(i), i + 1)
        mod[2000 + 2 * i] = with_timer(mod_picture(i), mod_timer(i) if mod_timer else i + 1 - lag)
    return write_run(tmp_path, "tb", base), write_run(tmp_path, "tm", mod), [2000 + 2 * i for i in range(count)]


def judge(tmp_path, **kw):
    base, mod, after = toggle_runs(tmp_path, **kw)
    return mc.judge_toggle(base, mod, BEFORE, after, TOGGLE_LOG)


def patched(picture, x0, y0, size=6, colour=(250, 10, 10)):
    """`picture` bytes with a size x size patch at (x0, y0): a small edit."""
    buf = bytearray(picture)
    head = len(b"P6\n480 272\n255\n")
    for y in range(y0, y0 + size):
        for x in range(x0, x0 + size):
            buf[head + (y * mc.WIDTH + x) * 3:head + (y * mc.WIDTH + x) * 3 + 3] = bytes(colour)
    return bytes(buf)


def test_toggle_passes_when_the_mod_is_silent_after_it(tmp_path):
    ok, message = judge(tmp_path)
    assert ok, message
    assert "40/40 frames after it" in message


def test_toggle_pairs_a_frame_one_game_step_behind_by_its_timer(tmp_path):
    ok, message = judge(tmp_path, lag=1)   # the run shows, at each present vblank, the game frame the baseline showed 2 vblanks earlier
    assert ok, message


def test_toggle_does_not_pair_an_unrelated_frame(tmp_path):
    ok, message = judge(tmp_path, mod_timer=lambda i: 600 + i)   # game frames the baseline never showed
    assert not ok and "nothing proved" in message


def test_toggle_fails_when_the_mod_keeps_orbiting(tmp_path):
    ok, message = judge(tmp_path, mod_picture=lambda i: make_frame(car_at=(250, 135), sky=(30, 120, 30)))
    assert not ok and "still edits" in message


def test_toggle_fails_on_a_small_edit_outside_the_car_window(tmp_path):
    # a corner pixel patch, a HUD patch: byte-identical outside the car window is required
    for x0, y0 in ((5, 5), (300, 245), (440, 20)):
        ok, message = judge(tmp_path / f"p{x0}", mod_picture=lambda i, x0=x0, y0=y0: patched(make_frame(car_at=CAR), x0, y0))
        assert not ok and "still edits" in message, (x0, y0, message)


def test_toggle_ignores_the_hud_clock_digit_only(tmp_path):
    # the clock at the bottom right flips a frame apart between two runs: not the mod. A patch anywhere else in the HUD corner is.
    ok, message = judge(tmp_path / "clock", mod_picture=lambda i: patched(make_frame(car_at=CAR), 464, 255, size=6))
    assert ok, message
    ok, message = judge(tmp_path / "next", mod_picture=lambda i: patched(make_frame(car_at=CAR), 452, 255, size=6))  # just left of the clock
    assert not ok and "still edits" in message


def test_toggle_tolerates_a_flicker_inside_the_car_window_only(tmp_path):
    flicker = lambda i: make_frame(car_at=CAR, car_size=(62, 40))   # a few hundred pixels at the car's edge, inside CAR_WINDOW
    ok, message = judge(tmp_path / "a", mod_picture=flicker)
    assert ok, message
    big = lambda i: patched(make_frame(car_at=CAR), 150, 120, size=60)   # 3600 px inside CAR_WINDOW: more than TOGGLE_CAR_NOISE of it
    ok, message = judge(tmp_path / "b", mod_picture=big)
    assert not ok and "still edits" in message


def test_toggle_floor_is_a_count_and_a_share_of_the_usable_frames(tmp_path):
    ok, message = judge(tmp_path / "few", count=mc.MIN_TOGGLE_AFTER - 1)
    assert not ok and "too few" in message
    # 100 usable frames after the toggle, only 35 of them pair (the run's timers are far from the baseline's for the rest): a count that passes,
    # a share that does not
    ok, message = judge(tmp_path / "share", count=100, mod_timer=lambda i: i + 1 if i < 35 else 700 + i)
    assert not ok and "35/100" in message


def test_toggle_ignores_frozen_and_missing_timers(tmp_path):
    ok, message = judge(tmp_path / "frozen", mod_timer=lambda i: 7)            # the same reading every frame: frozen, identifies nothing
    assert not ok and "nothing proved" in message
    base, _mod, after = toggle_runs(tmp_path / "base")
    directory = tmp_path / "menu" / "frames"                                   # a run whose frames show no timer at all (a menu)
    directory.mkdir(parents=True)
    for k, data in {**{k: make_frame(car_at=(250, 135), sky=(30, 120, 30)) for k in BEFORE}, **{k: make_frame(car_at=CAR) for k in after}}.items():
        (directory / f"frame_{k:06d}.ppm").write_bytes(data)
    assert not has_timer((directory / f"frame_{after[0]:06d}.ppm").read_bytes())
    ok, message = mc.judge_toggle(base, mc.frames_of(str(directory)), BEFORE, after, TOGGLE_LOG)
    assert not ok and "nothing proved" in message


def test_toggle_needs_the_toggle_line_orbited_frames_before_and_enough_after(tmp_path):
    base, mod, after = toggle_runs(tmp_path)
    assert not mc.judge_toggle(base, mod, BEFORE, after, "")[0]
    assert "orbited" in mc.judge_toggle(base, base, BEFORE, after, TOGGLE_LOG)[1]  # nothing orbited before the toggle
    assert "too few" in mc.judge_toggle(base, mod, BEFORE[:5], after, TOGGLE_LOG)[1]


def test_main_toggle_label(tmp_path, capsys):
    toggle_runs(tmp_path)
    os.makedirs(tmp_path / "baseline"), os.makedirs(tmp_path / "toggle_90")
    os.rename(tmp_path / "tb" / "frames", tmp_path / "baseline" / "frames")
    os.rename(tmp_path / "tm" / "frames", tmp_path / "toggle_90" / "frames")
    keys = sorted(set(BEFORE) | {2000 + 2 * i for i in range(40)})
    dumps = "".join(f"[frame-dump] frame {k:06d} vblank {k + 40} present_vblank {k}\n" for k in keys)
    (tmp_path / "baseline" / "run.log").write_text("[pad] script done at vblank 0\n" + dumps)
    (tmp_path / "toggle_90" / "run.log").write_text(TOGGLE_LOG + dumps)
    # chase frames start at vblank 150; the toggle at 1000 with a margin of 150: before = [150, 850), after = vblank >= 1150
    # (the pad has no frames between 100 and 180 apart from BEFORE: 100..170, so the labels are shifted by the margin)
    assert mc.main(["x", str(tmp_path), "toggle_90", "--toggle-at", "1000"]) == 0
    assert "PASS toggle_90" in capsys.readouterr().out


def timed_run(tmp_path, label, picture, offset=0, same_timer=None):
    return write_run(tmp_path, label, {i: with_timer(picture(i), same_timer if same_timer is not None else i // 10 + offset) for i in baseline_frames()})


def test_orbit_digits_must_be_compared_on_enough_frames(tmp_path):
    base = timed_run(tmp_path, "b", lambda i: make_frame(car_at=(240, 140)))
    orbit = lambda i: make_frame(car_at=(250, 135), sky=(30, 120, 30))
    ok, message = mc.judge_orbit(base, timed_run(tmp_path, "m", orbit), INDICES)
    assert ok, message
    # the run is a game step off on every frame: the pictures differ, no digit changed, but none could be compared: nothing proved
    ok, message = mc.judge_orbit(base, timed_run(tmp_path, "slip", orbit, offset=100), INDICES)
    assert not ok and "digits were compared on only 0/8" in message
    # a few compared frames are not enough either
    few = {i: with_timer(orbit(i), i // 10 if i <= 30 else 100 + i // 10) for i in baseline_frames()}
    ok, message = mc.judge_orbit(base, write_run(tmp_path, "few", few), INDICES)
    assert not ok and "compared on only 3/8" in message


def test_the_not_orbiting_build_fails_even_when_the_run_is_a_step_off(tmp_path):
    base = timed_run(tmp_path, "b", lambda i: make_frame(car_at=(240, 140)))
    same = lambda i: make_frame(car_at=(240, 140))
    ok, message = mc.judge_orbit(base, timed_run(tmp_path, "m", same), INDICES)
    assert not ok and "did not orbit" in message
    ok, message = mc.judge_orbit(base, timed_run(tmp_path, "s", same, offset=100), INDICES)   # the mod does nothing and the timers are off
    assert not ok


def test_identity_does_not_count_frozen_timers(tmp_path):
    same = lambda i: make_frame(car_at=(240, 140))
    base = timed_run(tmp_path, "b", same, same_timer=5)   # the timer never moves (the countdown before GO): identifies no game frame
    ok, message = mc.judge_identity(base, timed_run(tmp_path, "m", same, same_timer=5))
    assert not ok and "nothing proved" in message


def test_main_refuses_a_run_without_a_wide_yaw(tmp_path, capsys):
    write_run(tmp_path, "baseline", baseline_frames())
    (tmp_path / "baseline" / "run.log").write_text("[pad] script done at vblank 1000\n" + "".join(f"[frame-dump] frame {i:06d} vblank {2000 + i}\n" for i in INDICES))
    write_run(tmp_path, "yaw_0", baseline_frames())
    write_run(tmp_path, "yaw_90", orbited())
    assert mc.main(["x", str(tmp_path), "yaw_0", "yaw_90"]) == 1
    assert "no yaw >= 135" in capsys.readouterr().out


def test_frame_selection_uses_the_present_vblank_not_the_dump_time_label():
    log = ("[pad] script done at vblank 1000\n"
           "[frame-dump] frame 000010 vblank 2000 present_vblank 1100\n"      # the label is late (the GE worker lagged): the present was early
           "[frame-dump] frame 000020 vblank 1200 present_vblank 1300\n")
    assert mc.chase_frame_indices(log) == [1300]                               # chase = present_vblank >= 1150
    assert mc.frames_in_vblanks(log, 1250, 1400) == [1300]


def test_the_timer_gate_does_not_depend_on_the_scene_behind_it(tmp_path):
    # the orbit puts a bright sky behind the timer (rows 33-50 are 3D picture): the same game frame must still be recognised, and the digits compared
    base = timed_run(tmp_path, "b", lambda i: make_frame(car_at=(240, 140)))
    bright = lambda i: make_frame(car_at=(250, 135), sky=(250, 250, 250))
    ok, message = mc.judge_orbit(base, timed_run(tmp_path, "m", bright), INDICES)
    assert ok, message
    assert "HUD digits identical" in message
