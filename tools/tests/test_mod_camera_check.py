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
    count, cx, cy = mc.car_box((480, 272, make_frame(car_at=(240, 140))[len(b"P6\n480 272\n255\n"):]))
    assert count > mc.CAR_MIN_PIXELS and abs(cx - 240) < 2 and abs(cy - 140) < 2
    assert mc.car_box((480, 272, make_frame()[len(b"P6\n480 272\n255\n"):]))[0] == 0
    # orange in the HUD strip does not count
    count, _, _ = mc.car_box((480, 272, make_frame(hud=ORANGE)[len(b"P6\n480 272\n255\n"):]))
    assert count == 0


def test_digits_mask_sees_the_digits_only():
    body = len(b"P6\n480 272\n255\n")
    assert len(mc.digits_mask((480, 272, make_frame()[body:]))) == 40 * 16
    assert mc.digits_mask((480, 272, make_frame(hud=GREY)[body:])) == []


def test_chase_frame_indices_from_the_log():
    log = "[frame-dump] frame 000030 vblank 100\n[pad] script done at vblank 1000\n[frame-dump] frame 000600 vblank 1100\n[frame-dump] frame 000660 vblank 1200\n"
    assert mc.chase_frame_indices(log) == [660]
    assert mc.chase_frame_indices("[frame-dump] frame 000030 vblank 100\n") == []


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
    mod = write_run(tmp_path, "mod", orbited(car_at=(340, 140)))
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
