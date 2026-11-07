import pytest

from tools import mod_reset_check as mr


def state(t, step, wheels=4, up_z=1.0, spd=10.0, pos=(0.0, 0.0, 0.0), yaw=1.57, rates=(0, 0, 0, 0, 0), ring=3, restores=0):
    r = ",".join(f"{x:.2f}" for x in rates)
    return (f"[mod:reset-to-track] t={t} step={step} wheels={wheels} up_z={up_z:.3f} spd={spd:.2f} "
            f"pos={pos[0]:.2f},{pos[1]:.2f},{pos[2]:.2f} yaw={yaw:.3f} rates={r} ring={ring} restores={restores}")


def restore_line(step=1900, pos=(-300.0, 250.0, 0.0), yaw=1.57):
    return f"[mod:reset-to-track] RESTORE step={step} pos={pos[0]:.2f},{pos[1]:.2f},{pos[2]:.2f} yaw={yaw:.3f}"


def good_log(**kw):
    """A passing run: off-track samples before, a restore, 5-step samples to t=660 on asphalt, speed rising to 12 m/s."""
    lines = [state(0, 1800 + i * 30, wheels=4 if i < 2 else 0, pos=(-440.0 - i, 250.0, 2.0), spd=30.0, restores=0) .replace("t=0", "t=-1")
             for i in range(4)]
    lines.append(restore_line())
    for t in range(0, 661, 5):
        lines.append(state(t, 1900 + t, spd=min(12.0, 0.01 + t * 0.05), pos=(-300.0 + t * 0.1, 250.0, 0.0), restores=1, ring=1, **kw))
    return "\n".join(lines)


def test_parse_states_and_restores():
    text = good_log()
    states = mr.parse_states(text)
    assert states[0]["t"] == -1 and states[0]["wheels"] == 4 and states[3]["wheels"] == 0
    assert states[-1]["t"] == 660 and states[-1]["rates"] == [0, 0, 0, 0, 0] and states[-1]["restores"] == 1
    restores = mr.parse_restores(text)
    assert restores == [{"step": 1900, "pos": (-300.0, 250.0, 0.0), "yaw": pytest.approx(1.57)}]
    assert mr.parse_states("garbage") == [] and mr.parse_restores("garbage") == []


def test_parse_states_accepts_nan():
    assert mr.parse_states(state(0, 1, up_z=float("nan")))[0]["up_z"] != mr.parse_states(state(0, 1))[0]["up_z"]


def test_good_run_passes():
    ok, message = mr.judge_restore(good_log())
    assert ok, message
    assert "4/4 wheels" in message


def test_no_restore_or_two_fail():
    assert not mr.judge_restore("nothing")[0]
    assert "2 RESTORE" in mr.judge_restore(good_log() + "\n" + restore_line(2000))[1]


def test_premise_the_car_was_off_the_asphalt():
    lines = good_log().splitlines()
    lines = [l.replace("wheels=0", "wheels=4") for l in lines]
    ok, message = mr.judge_restore("\n".join(lines))
    assert not ok and "premise" in message


def test_log_too_short():
    lines = [l for l in good_log().splitlines() if "t=6" not in l or "t=60 " in l]
    ok, message = mr.judge_restore("\n".join(lines))
    assert not ok and "follows the car only" in message


def test_nothing_restored_when_the_car_did_not_move():
    # the t=0 sample is at the pre-restore position: moved ~0
    lines = []
    for l in good_log().splitlines():
        if "t=0 " in l:
            l = l.replace("pos=-300.00,250.00,0.00", "pos=-441.00,250.00,2.00")
        lines.append(l)
    ok, message = mr.judge_restore("\n".join(lines))
    assert not ok and "moved only" in message


def test_not_from_rest_fails():
    lines = [l.replace("t=0 step=1900 wheels=4 up_z=1.000 spd=0.01", "t=0 step=1900 wheels=4 up_z=1.000 spd=7.22") for l in good_log().splitlines()]
    ok, message = mr.judge_restore("\n".join(lines))
    assert not ok and "not from rest" in message


def test_a_wheel_off_asphalt_after_the_settle_time_fails_but_not_before():
    lines = good_log().splitlines()
    early = [l.replace("wheels=4", "wheels=2") if " t=20 " in l else l for l in lines]
    assert mr.judge_restore("\n".join(early))[0]  # the settle transient is allowed
    late = [l.replace("wheels=4", "wheels=3") if " t=200 " in l else l for l in lines]
    ok, message = mr.judge_restore("\n".join(late))
    assert not ok and "t=200" in message and "3/4" in message


def test_a_tipped_car_fails():
    lines = [l.replace("up_z=1.000", "up_z=0.850") if " t=300 " in l else l for l in good_log().splitlines()]
    assert not mr.judge_restore("\n".join(lines))[0]


def test_heading_drift_fails_and_wrap_is_handled():
    lines = []
    for l in good_log().splitlines():
        t = mr.STATE.search(l)
        if t and 60 <= int(t.group(1)) <= 120:
            l = l.replace("yaw=1.570", "yaw=1.650")  # 0.08 rad off the restored heading (the rates-not-zeroed sabotage)
        lines.append(l)
    ok, message = mr.judge_restore("\n".join(lines))
    assert not ok and "drifted" in message
    assert mr.wrap_pi(2 * 3.14159 - 0.1) == pytest.approx(-0.1, abs=1e-3)
    # restored heading +3.1, car at -3.1: 0.083 rad apart across the wrap, not 6.2 (still too much) ...
    wrapped = good_log().replace("yaw=1.570", "yaw=3.100").replace("yaw=1.57 ", "yaw=3.10 ")
    assert mr.judge_restore(wrapped)[0]
    flipped = [l.replace("yaw=3.100", "yaw=-3.180") if " t=70 " in l else l for l in wrapped.splitlines()]
    assert mr.judge_restore("\n".join(flipped))[0]  # -3.180 is 3.103 mod 2*pi: 0.003 rad off, not 6.3
    assert not mr.judge_restore("\n".join(l.replace("yaw=3.100", "yaw=-3.100") if " t=70 " in l else l for l in wrapped.splitlines()))[0]


def test_rates_not_zeroed_fails():
    lines = [l.replace("rates=0.00,0.00,0.00,0.00,0.00", "rates=-0.11,-0.21,-0.08,-0.26,0.64") if " t=0 " in l or " t=5 " in l else l
             for l in good_log().splitlines()]
    ok, message = mr.judge_restore("\n".join(lines))
    assert not ok and "not zeroed" in message


def test_asphalt_is_judged_before_the_distance():
    """The newest-pose sabotage restores where the car is: wheels off asphalt AND almost no move; the asphalt message wins."""
    lines = []
    for l in good_log().splitlines():
        if " t=" in l and " t=-1" not in l:
            l = l.replace("wheels=4", "wheels=0").replace("pos=-300.00", "pos=-441.00")
        lines.append(l)
    ok, message = mr.judge_restore("\n".join(lines))
    assert not ok and "wheels on asphalt" in message


def test_a_car_that_never_drives_fails():
    import re
    slow = re.sub(r"(t=\d+ step=\d+ wheels=4 up_z=1.000 spd=)[\d.]+", r"\g<1>0.50", good_log())
    ok, message = mr.judge_restore(slow)
    assert not ok and "does not drive" in message


def test_no_restore_judge():
    assert mr.judge_no_restore(good_log().replace("RESTORE", "x"))[0]
    ok, message = mr.judge_no_restore(good_log())
    assert not ok and "1 RESTORE" in message


def frame(value):
    return b"P6\n480 272\n255\n" + bytes([value]) * (480 * 272 * 3)


def write_frames(tmp_path, label, data):
    d = tmp_path / label / "frames"
    d.mkdir(parents=True, exist_ok=True)
    for i, v in data.items():
        (d / f"frame_{i:06d}.ppm").write_bytes(frame(v))
    return mr.frames_of(str(d))


def test_identity(tmp_path):
    same = {i: 7 for i in range(8)}
    a, b = write_frames(tmp_path, "a", same), write_frames(tmp_path, "b", same)
    assert mr.judge_identity(a, b)[0]
    c = write_frames(tmp_path, "c", {**same, 5: 9})
    ok, message = mr.judge_identity(a, c)
    assert not ok and "frame 5" in message
    d = write_frames(tmp_path, "d", {0: 7, 1: 7})
    assert "common frames" in mr.judge_identity(a, d)[1]


def make_run(tmp_path, label, log, frames):
    write_frames(tmp_path, label, frames)
    (tmp_path / label / "run.log").write_text(log)


def test_main_modes(tmp_path, capsys):
    same = {i: 7 for i in range(8)}
    make_run(tmp_path, "baseline", "", same)
    make_run(tmp_path, "reset", good_log(), {})
    make_run(tmp_path, "quiet", "ok", same)
    make_run(tmp_path, "toggle", "[mods] reset-to-track -> OFF\n", same)
    out = str(tmp_path)
    assert mr.main(["x", out, "restore"]) == 0
    assert mr.main(["x", out, "quiet"]) == 0
    assert mr.main(["x", out, "toggle"]) == 0
    make_run(tmp_path, "toggle", "no toggle line\n", same)
    assert mr.main(["x", out, "toggle"]) == 1
    make_run(tmp_path, "quiet", good_log(), same)
    assert mr.main(["x", out, "quiet"]) == 1
    make_run(tmp_path, "reset", "nothing", {})
    assert mr.main(["x", out, "restore"]) == 1
    printed = capsys.readouterr().out
    assert "PASS reset" in printed and "FAIL reset" in printed
