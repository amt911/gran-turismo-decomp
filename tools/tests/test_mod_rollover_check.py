import math

import pytest

from tools import mod_rollover_check as mr


def state(st, up=(0.0, 0.0, 1.0), tilt=(0.0, 0.0), flag=0, pos=(-300.0, 250.0, 2.0), vel=(-30.0, 0.0, 0.0), lat=(0.0, 1.0, 0.0), rate=0.0,
          held=0, imp=0, flipcalls=100):
    f = lambda v: ",".join(f"{x:.3f}" for x in v)  # noqa: E731
    return (f"[mod:rollover] st={st} imp={imp} since={-1 if imp == 0 else 3} pos={f(pos)} vel={f(vel)} fwd=-1.000,0.000,0.000 lat={f(lat)} up={f(up)} "
            f"tilt={tilt[0]:.3f},{tilt[1]:.3f} rate={rate:.2f} flag={flag:02X}/0 held={held}({held * 0.3:.2f}) flipmachine_calls={flipcalls}")


def impulse(st=1864, held=12.0):
    return f"[mod:rollover] IMPULSE st={st} held={held:.2f}"


def good_log(held=12.0, end_up=1.0, end_speed=1.0, **kw):
    """70 quiet lines, an impulse at the crash, a roll toward +lat for held > 0 (up tips toward +lat), then settling."""
    sign = 1.0 if held > 0 else -1.0
    lines = [state(750 + i * 15, **kw) for i in range(70)]
    lines.append(impulse(held=held))
    for i, ang in enumerate((0.3, 0.8, 1.4, 2.2, 3.0, 2.0, 1.0, 0.3)):
        # the body's own lateral axis turns with the car (always perpendicular to up); the judge reads the tip against the pre-hit axis
        lat = (0.0, math.cos(ang), -sign * math.sin(ang))
        lines.append(state(1864 + i * 3, up=(0.0, sign * math.sin(ang), math.cos(ang)), tilt=(0.0, ang), lat=lat, imp=1, **kw))
    lines.append(state(1864 + 450, up=(0.0, 0.0, end_up), imp=1, vel=(end_speed, 0, 0), flipcalls=kw.get("flipcalls", 100)))
    return "\n".join(lines)


def test_parse_states_and_impulses():
    s = mr.parse_states(state(10, up=(0.1, 0.2, 0.9), flag=2) + "\n" + impulse(5, -3.5))[0]
    assert s["st"] == 10 and s["up"] == (0.1, 0.2, 0.9) and s["flag"] == 2 and s["flipcalls"] == 100
    assert mr.parse_impulses(impulse(5, -3.5)) == [{"st": 5, "held": -3.5}]
    assert mr.parse_states("garbage") == [] and mr.parse_impulses("garbage") == []
    old = state(1).replace(" flipmachine_calls=100", "")
    assert mr.parse_states(old)[0]["flipcalls"] is None


def test_good_roll_passes_both_signs():
    for held in (12.0, -12.0):
        ok, message = mr.judge_roll(good_log(held=held))
        assert ok, message
    assert "flip machine dormant" in message


def test_upside_down_at_rest_passes():
    assert mr.judge_roll(good_log(end_up=-0.9, end_speed=0.5))[0]


def test_fails_without_state_or_impulse():
    assert "did not run" in mr.judge_roll("nothing")[1]
    quiet = "\n".join(state(750 + i * 15) for i in range(70))
    assert "no IMPULSE" in mr.judge_roll(quiet)[1]


def test_nan_fails():
    log = good_log().replace("up=0.000,0.000,1.000", "up=0.000,0.000,nan", 1)
    assert "nan" in mr.judge_roll(log)[1]


def test_impulse_in_normal_driving_fails():
    log = good_log().replace(impulse(), impulse(900))
    assert "before the crash" in mr.judge_roll(log)[1]


def test_too_few_quiet_lines_and_a_dirty_quiet_line_fail():
    lines = good_log().splitlines()
    assert "only" in mr.judge_roll("\n".join(lines[40:]))[1]
    dirty = list(lines)
    dirty[10] = state(900, up=(0.0, 0.5, 0.8))
    assert "not a normal-driving state" in mr.judge_roll("\n".join(dirty))[1]
    dirty[10] = state(900, flag=2)
    assert "not a normal-driving state" in mr.judge_roll("\n".join(dirty))[1]
    dirty[10] = state(900, tilt=(0.0, 0.5))
    assert "not a normal-driving state" in mr.judge_roll("\n".join(dirty))[1]


def test_no_roll_fails_the_clamp_only_case():
    # the game's own clamp: tilt <= 0.611 rad, up.z >= 0.82: the mod's impulse without the flag looks like this
    lines = good_log().splitlines()
    for i, line in enumerate(lines):
        if "imp=1" in line and "st=1864" in line or ("imp=1" in line and "up=0.000,0.000,1.000" not in line):
            lines[i] = state(1864 + i, up=(0.0, 0.5, 0.85), tilt=(0.0, 0.58), imp=1)
    ok, message = mr.judge_roll("\n".join(lines))
    assert not ok and "35 deg clamp" in message


def test_tilt_angle_alone_counts_past_the_clamp():
    lines = good_log().splitlines()
    for i, line in enumerate(lines):
        if "imp=1" in line and "up=0.000,0.000,1.000" not in line:
            lines[i] = state(1864 + i, up=(0.0, 0.7, 0.7), tilt=(0.0, 0.8), imp=1)
    assert mr.judge_roll("\n".join(lines))[0]


def test_wrong_side_fails():
    log = good_log(held=12.0).replace(impulse(held=12.0), impulse(held=-12.0))
    assert "not toward the side that was hit" in mr.judge_roll(log)[1]


def test_flip_machine_woken_fails():
    lines = good_log().splitlines()
    lines[-3] = state(2200, flag=6, imp=1)
    assert "flip state machine woke" in mr.judge_roll("\n".join(lines))[1]


def test_height_jump_fails():
    lines = good_log().splitlines()
    lines[-4] = state(1883, imp=1, pos=(-300.0, 250.0, 9.0))
    assert "jumped" in mr.judge_roll("\n".join(lines))[1]


def test_blind_dispatch_check_fails_and_can_be_waived():
    log = good_log(flipcalls=0)
    assert "never fired" in mr.judge_roll(log)[1]
    assert mr.judge_roll(log, expected_flipcalls=False)[0]
    assert "never fired" in mr.judge_roll(good_log().replace(" flipmachine_calls=100", ""))[1]


def test_end_state_and_short_log_fail():
    assert "neither upright" in mr.judge_roll(good_log(end_up=0.3))[1]
    assert "neither upright" in mr.judge_roll(good_log(end_up=-0.9, end_speed=20.0))[1]
    short = "\n".join(good_log().splitlines()[:-1])
    assert "log ends" in mr.judge_roll(short)[1]


def test_no_state_after_the_impulse():
    lines = [state(750 + i * 15) for i in range(70)] + [impulse(1864)]
    assert "no state lines after" in mr.judge_roll("\n".join(lines))[1]


RESET = "[mod:reset-to-track] RESTORE step=1885 pos=-300.00,250.00,0.00 yaw=1.570"


def recover_log(held=12.0):
    lines = [state(750 + i * 15) for i in range(70)] + [impulse(1864, held)]
    lines += [state(1864 + i * 3, up=(0.0, 0.5, -0.2), tilt=(0.0, 2.0), imp=1) for i in range(8)]
    lines.append(RESET)
    lines += [state(1920 + i * 30, imp=1) for i in range(20)]
    return "\n".join(lines)


def test_recover_passes_and_fails():
    log = recover_log()
    ok, message = mr.judge_recover(log, log)
    assert ok, message
    assert not mr.judge_recover(log.replace(RESET, ""), log.replace(RESET, ""))[0]
    assert "no impulse before" in mr.judge_recover(log.replace(impulse(1864, 12.0), impulse(1990, 12.0)), log)[1]
    assert "not upside down" in mr.judge_recover(log.replace("up=0.000,0.500,-0.200", "up=0.000,0.100,0.990").replace("tilt=0.000,2.000", "tilt=0.000,0.100"), log)[1]
    assert "not upside down" in mr.judge_recover(log.replace("up=0.000,0.500,-0.200", "up=0.000,0.800,0.300"), log)[1]  # rolled, but not past 90 degrees
    lines = log.splitlines()
    lines[81] = state(1950, imp=1, rate=1.5)  # a roll rate that never settles after the reset
    assert "not settled" in mr.judge_recover("\n".join(lines), log)[1]
    assert "not upright after" in mr.judge_recover(log.replace("up=0.000,0.000,1.000", "up=0.000,0.000,0.100"), log)[1]
    assert "not upright after" in mr.judge_recover("\n".join(log.splitlines()[:-20]), log)[1]


def test_quiet_and_toggle():
    quiet = "\n".join(state(750 + i * 15) for i in range(30))
    assert mr.judge_quiet("[mods] loaded: rollover skipped:")[0]
    assert not mr.judge_quiet(quiet)[0]  # disabled: it must not log
    assert "IMPULSE" in mr.judge_quiet(impulse())[1]
    toggled = quiet + "\n[mods] rollover -> OFF\n"
    ok, message = mr.judge_quiet(toggled, toggled=True)
    assert ok, message
    assert "no `[mods]" in mr.judge_quiet(quiet, toggled=True)[1]
    assert "only 3" in mr.judge_quiet("\n".join(quiet.splitlines()[:3]) + "\n[mods] rollover -> OFF\n", toggled=True)[1]
    assert "after the toggle" in mr.judge_quiet(toggled + state(9999), toggled=True)[1]


def ppm(path, value, size=(4, 4)):
    path.write_bytes(b"P6\n%d %d\n255\n" % size + bytes([value]) * (size[0] * size[1] * 3))


def test_identity_and_frames(tmp_path):
    a, b = tmp_path / "a", tmp_path / "b"
    a.mkdir()
    b.mkdir()
    for i in range(8):
        ppm(a / f"frame_{i:05d}.ppm", i)
        ppm(b / f"frame_{i:05d}.ppm", i)
    (a / "other.txt").write_text("x")
    assert sorted(mr.frames_of(str(a))) == list(range(8))
    ok, message = mr.judge_identity(mr.frames_of(str(a)), mr.frames_of(str(b)))
    assert ok and "8 frames" in message
    ppm(b / "frame_00003.ppm", 99)
    assert "frame 3 differs" in mr.judge_identity(mr.frames_of(str(a)), mr.frames_of(str(b)))[1]
    assert "only 0" in mr.judge_identity({}, {})[1]
    (a / "bad.ppm").write_bytes(b"nope")
    with pytest.raises(ValueError):
        mr.read_ppm(str(a / "bad.ppm"))
    (a / "short.ppm").write_bytes(b"P6\n4 4\n255\nxx")
    with pytest.raises(ValueError):
        mr.read_ppm(str(a / "short.ppm"))


def make_out(tmp_path, roll=None, recover=None, quiet=None, quiet_label="disabled"):
    for label, text in (("roll", roll), ("recover", recover), (quiet_label, quiet)):
        if text is not None:
            (tmp_path / label).mkdir(exist_ok=True)
            (tmp_path / label / "run.log").write_text(text)
    return str(tmp_path)


def test_main_roll_and_recover(tmp_path, capsys):
    out = make_out(tmp_path, roll=good_log(), recover=recover_log())
    assert mr.main(["x", out, "roll"]) == 0
    assert "PASS roll" in capsys.readouterr().out
    assert mr.main(["x", out, "recover"]) == 0
    (tmp_path / "roll" / "run.log").write_text("[mods] loaded: rollover skipped:")
    assert mr.main(["x", out, "roll"]) == 1
    assert "FAIL roll" in capsys.readouterr().out
    (tmp_path / "recover" / "run.log").write_text("nothing")
    assert mr.main(["x", out, "recover"]) == 1


def dump_lines(n=40, first_vblank=16000, step=60):
    """A run log's frame dump lines (frame i at vblank first_vblank + i * step) and the route's end."""
    return "\n".join(f"[frame-dump] frame {i:06d} vblank {first_vblank + i * step}" for i in range(n)) + f"\n[pad] script done at vblank {first_vblank + (n - 1) * step}\n"


def test_frames_between_and_toggle_frames(tmp_path):
    log = dump_lines()
    assert mr.frames_between(log, 16300, 16500) == [5, 6, 7, 8]
    a, b = tmp_path / "a", tmp_path / "b"
    a.mkdir()
    b.mkdir()
    for i in range(40):
        ppm(a / f"frame_{i:06d}.ppm", i)
        ppm(b / f"frame_{i:06d}.ppm", i if i >= 8 else 99)  # differs only before the toggle window
    base, mod = mr.frames_of(str(a)), mr.frames_of(str(b))
    ok, message = mr.judge_toggle_frames(log, base, mod, 16000 + 8 * 60 - mr.TOGGLE_MARGIN)
    assert ok and "after the toggle" in message
    ok, message = mr.judge_toggle_frames(log, base, mod, 16000)  # a window that includes frames 4..: they differ
    assert not ok and "differs" in message
    assert not mr.judge_toggle_frames("no end", base, mod, 16000)[0]
    ok, message = mr.judge_toggle_frames("[decomp] gt::game::x: replaced\n" + log, base, mod, 16000)  # --decomp on: not compared, even though they differ
    assert ok and "not compared" in message


@pytest.mark.parametrize("mode", ["disabled", "toggle"])
def test_main_identity_modes(tmp_path, capsys, mode):
    text = "[mods] loaded:" if mode == "disabled" else "\n".join(state(750 + i * 15) for i in range(30)) + "\n[mods] rollover -> OFF\n" + dump_lines()
    out = make_out(tmp_path, quiet=text, quiet_label=mode)
    for label in ("baseline", mode):
        (tmp_path / label / "frames").mkdir(parents=True, exist_ok=True)
        for i in range(40):
            ppm(tmp_path / label / "frames" / f"frame_{i:06d}.ppm", i)
    args = ["x", out, mode] + (["15000"] if mode == "toggle" else [])
    assert mr.main(args) == 0
    assert f"PASS {mode}" in capsys.readouterr().out
    ppm(tmp_path / mode / "frames" / "frame_000030.ppm", 200)
    assert mr.main(args) == 1
