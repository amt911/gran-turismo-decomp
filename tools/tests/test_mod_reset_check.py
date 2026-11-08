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
        lines.append(state(t, 1900 + t, spd=min(18.0, 0.01 + t * 0.05), pos=(-300.0 + t * 0.1, 250.0, 0.0), restores=1, ring=1, **kw))
    return "\n".join(lines)


def test_parse_states_and_restores():
    text = good_log()
    states = mr.parse_states(text)
    assert states[0]["t"] == -1 and states[0]["wheels"] == 4 and states[3]["wheels"] == 0
    assert states[-1]["t"] == 660 and states[-1]["rates"] == [0, 0, 0, 0, 0] and states[-1]["restores"] == 1
    restores = mr.parse_restores(text)
    assert restores == [{"step": 1900, "pos": (-300.0, 250.0, 0.0), "yaw": pytest.approx(1.57)}]
    assert mr.parse_states("garbage") == [] and mr.parse_restores("garbage") == []


def test_parse_states_with_the_new_fields():
    line = state(5, 10).replace(" ring=", " vel=-35.00,0.00,0.10 fwdcos=0.99 ring=")
    s = mr.parse_states(line)[0]
    assert s["fwdcos"] == 0.99 and s["ring"] == 3


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
    assert not ok and "a rewind of" in message


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


def ring_lines(n, start=100, ring=5):
    return "\n".join(state(-1, start + i, ring=ring, spd=20.0) for i in range(n))


def test_no_restore_judge():
    quiet = ring_lines(12)
    ok, message = mr.judge_no_restore(quiet)
    assert ok and "the mod ran" in message
    ok, message = mr.judge_no_restore(good_log())
    assert not ok and "1 RESTORE" in message


def test_quiet_run_must_show_the_mod_ran():
    ok, message = mr.judge_no_restore("")
    assert not ok and "did not run" in message
    assert not mr.judge_no_restore(ring_lines(12, ring=0))[0]   # state lines, but never a pose in the ring
    assert not mr.judge_no_restore(ring_lines(9))[0]


def test_toggle_run_needs_lines_before_and_none_after():
    off = "[mods] reset-to-track -> OFF"
    good = ring_lines(12) + "\n" + off + "\n[pad] script done"
    assert mr.judge_no_restore(good, toggled=True)[0]
    ok, message = mr.judge_no_restore(ring_lines(12), toggled=True)
    assert not ok and "-> OFF" in message
    ok, message = mr.judge_no_restore(ring_lines(12) + "\n" + off + "\n" + ring_lines(2, start=900), toggled=True)
    assert not ok and "after the toggle" in message
    ok, message = mr.judge_no_restore(ring_lines(3) + "\n" + off, toggled=True)
    assert not ok and "did not run" in message  # toggled before the mod did anything


def test_nan_is_an_error_not_a_dropped_line():
    bad = ring_lines(12) + "\n" + state(-1, 500, up_z=float("nan"))
    assert "nan" in bad
    assert len(mr.parse_states(bad)) == 13  # parsed, not skipped
    ok, message = mr.judge_no_restore(bad)
    assert not ok and "nan/inf" in message
    ok, message = mr.judge_restore(good_log() + "\n" + state(-1, 10, spd=float("inf")))
    assert not ok and "nan/inf" in message


def test_rewind_distance_follows_the_logged_speed():
    # restored only 20 m back although the car was doing 35 m/s (a rewind of 0.57 s): fails; 60 m back (1.7 s) passes
    def log(back):
        lines = [state(-1, 1800 + i * 30, wheels=4 if i < 2 else 0, spd=35.0, pos=(-440.0 - i, 250.0, 2.0)).replace("t=-1", "t=-1") for i in range(4)]
        lines.append(restore_line(step=1900))
        for t in range(0, 661, 5):
            lines.append(state(t, 1900 + t, spd=min(18.0, 0.01 + t * 0.05), pos=(-440.0 + back + t * 0.1, 250.0, 0.0), restores=1, ring=1))
        return "\n".join(lines)
    assert not mr.judge_restore(log(20.0))[0]
    assert mr.judge_restore(log(60.0))[0]


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
    make_run(tmp_path, "baseline_nosel", "", same)
    make_run(tmp_path, "reset", good_log(), {})
    make_run(tmp_path, "quiet", ring_lines(12), same)
    make_run(tmp_path, "toggle", ring_lines(12) + "\n[mods] reset-to-track -> OFF\n", same)
    out = str(tmp_path)
    assert mr.main(["x", out, "restore"]) == 0
    assert mr.main(["x", out, "quiet"]) == 0
    assert mr.main(["x", out, "toggle"]) == 0
    make_run(tmp_path, "toggle", ring_lines(12) + "\nno toggle line\n", same)
    assert mr.main(["x", out, "toggle"]) == 1
    make_run(tmp_path, "quiet", good_log(), same)
    assert mr.main(["x", out, "quiet"]) == 1
    make_run(tmp_path, "reset", "nothing", {})
    assert mr.main(["x", out, "restore"]) == 1
    printed = capsys.readouterr().out
    assert "PASS reset" in printed and "FAIL reset" in printed


def pad_lines(*events):
    return "\n".join(f"[mod:reset-to-track] PADSEL {k} step={s}" + (f" tap={tap}" if tap is not None else "") for k, s, tap in events)


def test_padsel_parsing_and_hidden_judge():
    text = pad_lines(("down", 1851, None), ("up", 1890, 0))
    assert mr.parse_padsel(text) == [("down", 1851, None), ("up", 1890, False)]
    assert mr.judge_select_hidden(text)[0]
    ok, message = mr.judge_select_hidden(pad_lines(("down", 1851, None), ("up", 1860, 1)))
    assert not ok and "replayed" in message
    assert not mr.judge_select_hidden("")[0]
    assert not mr.judge_select_hidden(pad_lines(("down", 1, None), ("up", 40, 0), ("down", 50, None), ("up", 90, 0)))[0]


def test_restore_judge_includes_the_select_check_when_pad_lines_exist():
    assert mr.judge_restore(good_log() + "\n" + pad_lines(("down", 1851, None), ("up", 1890, 0)))[0]
    ok, message = mr.judge_restore(good_log() + "\n" + pad_lines(("down", 1851, None), ("up", 1890, 1)))
    assert not ok and "tap" in message


def test_taps_judge():
    taps = pad_lines(("down", 10, None), ("up", 16, 1), ("down", 30, None), ("up", 36, 1), ("down", 50, None), ("up", 56, 1))
    assert mr.judge_taps(taps, 3)[0]
    assert not mr.judge_taps(taps, 4)[0]
    assert not mr.judge_taps(taps.replace("tap=1", "tap=0", 1), 3)[0]
    ok, message = mr.judge_taps(taps + "\n" + restore_line(), 3)
    assert not ok and "reset the car" in message


def test_main_tap_mode(tmp_path, capsys):
    frames = {i: 7 for i in range(30, 60)}
    log = "\n".join(f"[frame-dump] frame {i:06d} vblank {1000 + i * 10}" for i in frames) + "\n[pad] script done at vblank 1000\n"
    make_run(tmp_path, "baseline_chase", log, frames)
    make_run(tmp_path, "tap", pad_lines(("down", 10, None), ("up", 16, 1), ("down", 30, None), ("up", 36, 1), ("down", 50, None), ("up", 56, 1)), frames)
    assert mr.main(["x", str(tmp_path), "tap"]) == 0
    make_run(tmp_path, "tap", pad_lines(("down", 10, None), ("up", 16, 1)), frames)
    assert mr.main(["x", str(tmp_path), "tap"]) == 1
    make_run(tmp_path, "tap", pad_lines(("down", 10, None), ("up", 16, 1), ("down", 30, None), ("up", 36, 1), ("down", 50, None), ("up", 56, 1)), {**frames, 59: 9})
    assert mr.main(["x", str(tmp_path), "tap"]) == 0   # a small difference (a one-step timing shift) is the same view
    make_run(tmp_path, "tap", pad_lines(("down", 10, None), ("up", 16, 1), ("down", 30, None), ("up", 36, 1), ("down", 50, None), ("up", 56, 1)), {**frames, 59: 200})
    assert mr.main(["x", str(tmp_path), "tap"]) == 1   # another camera view
    assert "PASS tap" in capsys.readouterr().out


def test_judge_same_view(tmp_path):
    base = write_frames(tmp_path, "b", {i: 100 for i in range(8)})
    near = write_frames(tmp_path, "n", {i: 130 for i in range(8)})
    far = write_frames(tmp_path, "f", {**{i: 100 for i in range(7)}, 7: 250})
    assert mr.judge_same_view(base, near)[0]
    ok, message = mr.judge_same_view(base, far)
    assert not ok and "frame 7" in message and "another camera view" in message
    assert "common frames" in mr.judge_same_view(base, {0: near[0]})[1]


def test_calm_window_can_be_narrowed_to_the_restore_step():
    # a car reset out of a tumble wobbles on its springs: 0.9 rad/s at t=10, calm again by t=15 (the rollover check's `recover` run)
    lines = [l.replace("rates=0.00,0.00,0.00,0.00,0.00", "rates=0.13,0.91,0.13,0.91,0.06") if " t=5 " in l or " t=10 " in l else l for l in good_log().splitlines()]
    log = "\n".join(lines)
    assert not mr.judge_restore(log)[0]
    assert mr.judge_restore(log, calm_steps=0)[0]
    at_restore = [l.replace("rates=0.00,0.00,0.00,0.00,0.00", "rates=0.10,-1.17,0.10,-1.17,0.06") if " t=0 " in l else l for l in log.splitlines()]
    assert not mr.judge_restore("\n".join(at_restore), calm_steps=0)[0]  # --decomp on logs the first step's wobble at t=0
    assert mr.judge_restore("\n".join(at_restore), calm_steps=-1)[0]
    zeroed_late = [l.replace("rates=0.00,0.00,0.00,0.00,0.00", "rates=0.50,0.00,0.00,0.00,0.00") if " t=0 " in l else l for l in good_log().splitlines()]
    assert not mr.judge_restore("\n".join(zeroed_late), calm_steps=0)[0]  # still fails when the restore step itself keeps a rate


def test_main_restore_takes_the_calm_window(tmp_path):
    (tmp_path / "reset").mkdir()
    lines = [l.replace("rates=0.00,0.00,0.00,0.00,0.00", "rates=0.13,0.91,0.13,0.91,0.06") if " t=5 " in l else l for l in good_log().splitlines()]
    (tmp_path / "reset" / "run.log").write_text("\n".join(lines))
    assert mr.main(["x", str(tmp_path), "restore"]) == 1
    assert mr.main(["x", str(tmp_path), "restore", "0"]) == 0
    assert mr.main(["x", str(tmp_path), "restore", "-1"]) == 0


# --- --multi: every restore stays on the asphalt for 4 s, and the look-ahead rule ran ------------------------------------------------
def ahead_line(step, ok, checked, rejected):
    return f"[mod:reset-to-track] AHEAD step={step} pos=1.00,2.00 yaw=1.000 ok={ok} checked={checked} rejected={rejected}"


def multi_log(restores=(2000, 2500, 3000), bad=None, rejected=3, ahead=True):
    """`bad` = (restore index, wheels): from t=100 on that restore's samples have that many wheels on asphalt."""
    lines = []
    for n, step in enumerate(restores, 1):
        lines.append(state(-1, step - 5, restores=n - 1).replace("t=-1", "t=-1"))
        lines.append(restore_line(step))
        for t in range(1, 361, 5):
            wheels = bad[1] if bad and bad[0] == n and t >= 100 else 4
            lines.append(state(t, step + t, wheels=wheels, restores=n))
    if ahead:
        lines.append(ahead_line(1990, 0, 20, rejected))
    return "\n".join(lines)


def test_multi_good_run_passes():
    ok, message = mr.judge_multi(multi_log())
    assert ok, message
    assert "3 restores" in message and "rejected 3 of 20" in message


def test_multi_a_restore_that_leaves_the_road_fails():
    ok, message = mr.judge_multi(multi_log(bad=(2, 0)))
    assert not ok and "restore 2" in message and "out of bounds" in message


def test_multi_a_tipped_car_after_a_restore_fails():
    text = multi_log().replace("up_z=1.000", "up_z=0.500", 40)
    assert not mr.judge_multi(text)[0]


def test_multi_needs_restores_and_the_rule_to_have_run():
    assert "need at least" in mr.judge_multi(multi_log(restores=(2000,)))[1]
    assert "no AHEAD line" in mr.judge_multi(multi_log(ahead=False))[1]
    assert "rejected none" in mr.judge_multi(multi_log(rejected=0))[1]
    assert not mr.judge_multi("nothing")[0]


def test_multi_a_short_log_after_a_restore_fails():
    lines = [l for l in multi_log(restores=(2000, 2500)).splitlines() if not ("restores=2" in l and "t=" in l and int(l.split("t=")[1].split()[0]) > 60)]
    assert "follows the car only" in mr.judge_multi("\n".join(lines))[1]


def test_multi_nan_is_an_error():
    assert "nan" in mr.judge_multi(multi_log().replace("up_z=1.000", "up_z=nan", 1))[1]


def test_main_multi_mode(tmp_path, capsys):
    (tmp_path / "reset").mkdir()
    (tmp_path / "reset" / "run.log").write_text(multi_log())
    assert mr.main(["x", str(tmp_path), "multi"]) == 0
    assert capsys.readouterr().out.startswith("PASS multi:")
    (tmp_path / "reset" / "run.log").write_text(multi_log(bad=(1, 2)))
    assert mr.main(["x", str(tmp_path), "multi"]) == 1
