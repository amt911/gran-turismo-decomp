import os

import pytest

from tools import mod_cheats_check as mc


def sim(t):
    """Player (speed, distance) and AI distance after t steps of game time past the start (0.05 m/s per step, capped)."""
    t = max(0.0, t)
    cap = 35.0
    t_cap = cap / 0.05
    if t <= t_cap:
        return 0.05 * t, 0.025 * t * t, 0.02 * t * t
    return cap, 0.025 * t_cap * t_cap + cap * (t - t_cap), 0.02 * t * t


def log(factor=1.0, freeze=False, name="OFF", steps=range(2, 1800, 2), acc=1.0, ai_still=None, ais=3, start=750):
    lines = []
    for st in steps:
        t = start + (st - start) * factor if st > start else st
        speed, d, dai = sim(t - start)
        a = []
        for i in range(ais):
            dd = 0.0 if (freeze or ai_still) else dai * (1 + 0.1 * i)
            a.append(f" A{i}={-100 - 10 * i - dd:.3f},{20 + 5 * i:.3f},2.000")
        lines.append(f"[mod:cheats] st={st} slow={name} freeze={1 if freeze else 0} P={-300 - d:.3f},250.000,2.000 spd={speed:.3f} acc={acc:.2f}{''.join(a)}")
    return "\n".join(lines)


def test_parse_lines():
    cars = mc.parse_lines(log(steps=[10, 12]))
    assert [c.st for c in cars] == [10, 12] and len(cars[0].ai) == 3 and cars[0].acc == 1.0 and cars[0].slow == "OFF"
    assert mc.parse_lines("junk") == []
    one = mc.parse_lines("[mod:cheats] st=5 slow=0.5x freeze=1 P=1.5,2.5,3.5 spd=7.25 acc=0.00")[0]
    assert one.freeze and one.pos == (1.5, 2.5, 3.5) and one.ai == []


def test_crossing_interpolates_and_misses():
    cars = mc.parse_lines(log())
    step, pos = mc.crossing(cars, 10.0)
    assert 195 + 750 - 750 <= step - 750 + 750 and abs((step - 750) - 200) < 3  # 0.05 m/s per step: 10 m/s after 200 steps
    assert mc.crossing(cars, 99.0) is None


def test_slowmo_passes_for_a_scaled_clock():
    for factor, name in ((0.5, "0.5x"), (0.25, "0.25x")):
        ok, message = mc.judge_slowmo(log(), log(factor, name=name, steps=range(2, 4000, 2)), factor)
        assert ok, message


def test_slowmo_fails_when_nothing_is_slowed():
    ok, message = mc.judge_slowmo(log(), log(1.0, name="0.5x"), 0.5)
    assert not ok and "clock is not scaled" in message


def test_slowmo_fails_when_only_the_player_is_slowed():
    slow = log(0.5, name="0.5x", steps=range(2, 4000, 2))
    # AI cars that keep their normal pace: same distance at the same step as idle
    idle_ai = mc.parse_lines(log())
    lines = []
    for c, raw in zip(mc.parse_lines(slow), slow.split("\n")):
        base = next((x for x in idle_ai if x.st == c.st), idle_ai[-1])
        ai = "".join(f" A{i}={p[0]:.3f},{p[1]:.3f},{p[2]:.3f}" for i, p in enumerate(base.ai))
        lines.append(raw[:raw.index(" A0=")] + ai)
    ok, message = mc.judge_slowmo(log(), "\n".join(lines), 0.5)
    assert not ok and "AI car" in message


def test_slowmo_fails_when_the_physics_differs():
    # the speed->distance curve differs: the slow run climbs twice as fast per distance (distances halved)
    squeezed = []
    for line in log(0.5, name="0.5x", steps=range(2, 4000, 2)).split("\n"):
        c = mc.parse_lines(line)[0]
        squeezed.append(line.replace(f"P={c.pos[0]:.3f}", f"P={-300 + (c.pos[0] + 300) * 0.5:.3f}"))
    ok, message = mc.judge_slowmo(log(), "\n".join(squeezed), 0.5)
    assert not ok and "curve" in message


def test_slowmo_rejects_thin_or_wrong_logs():
    assert mc.judge_slowmo("", "", 0.5)[0] is False
    assert "too few" in mc.judge_slowmo(log(steps=range(2, 20, 2)), log(0.5, name="0.5x"), 0.5)[1]
    assert "cheat on" in mc.judge_slowmo(log(name="0.5x"), log(0.5, name="0.5x"), 0.5)[1]
    assert "barely" in mc.judge_slowmo(log(), log(0.5, name="OFF"), 0.5)[1]
    slow_cars = log(0.5, name="0.5x").replace("spd=", "spd=0.0").replace("nan", "nan")
    assert not mc.judge_slowmo(log(), slow_cars.replace("P=-", "P=nan,-", 1), 0.5)[0]
    never_fast = log(steps=range(2, 800, 2))  # the car has barely started
    assert "fast enough" in mc.judge_slowmo(never_fast, log(0.5, name="0.5x", steps=range(2, 800, 2)), 0.5)[1]


def test_freeze_passes_and_fails():
    ok, message = mc.judge_freeze(log(), log(freeze=True))
    assert ok, message
    # AI cars that keep moving
    moving = log().replace("freeze=0", "freeze=1")
    ok, message = mc.judge_freeze(log(), moving)
    assert not ok and "moved while frozen" in message
    # the idle run's AI does not move: the check could not tell
    ok, message = mc.judge_freeze(log(ai_still=True), log(freeze=True))
    assert not ok and "could not tell" in message
    # a player that does not drive
    parked = "\n".join(line.replace("P=-", "P=-0") for line in log(freeze=True).split("\n"))
    assert mc.judge_freeze(log(), log(freeze=True, steps=range(2, 100, 2)))[0] is False
    assert mc.judge_freeze(log(), log(freeze=True).replace("freeze=1", "freeze=0"))[1].startswith("the frozen run barely")
    no_ai = log(freeze=True, ais=0)
    assert "no AI car" in mc.judge_freeze(log(), no_ai)[1]
    assert not parked == ""  # (kept: a parked player is covered by the 'did not race' message below)
    still_player = "\n".join(line.replace(line.split(" P=")[1].split(" spd")[0], "-300.000,250.000,2.000") for line in log(freeze=True).split("\n"))
    assert "did not race" in mc.judge_freeze(log(), still_player)[1]
    assert mc.judge_freeze("", "")[0] is False


def pad_log(acc_inside=0.0, rows=True, keys=("chord", "down", "right", "down", "right", "chord"), states=("opened", "closed"), frozen_after=True):
    out = []
    out.append(log(steps=range(800, 900, 2), acc=1.0))
    out.append(f"[padmenu] {keys[0]} at vblank 100")
    if "opened" in states:
        out.append("[padmenu] menu opened at vblank 100")
    out.append(log(steps=range(900, 960, 2), acc=acc_inside))
    for i, k in enumerate(keys[1:-1]):
        out.append(f"[padmenu] {k} at vblank {110 + i}")
        if rows and k == "right":
            out.append("[mod:cheats] ROW slow_motion -> 0.5x" if i == 1 else "[mod:cheats] ROW freeze_ai -> ON")
    out.append(log(steps=range(960, 1000, 2), acc=acc_inside, name="0.5x", freeze=True))
    out.append(f"[padmenu] {keys[-1]} at vblank 200")
    if "closed" in states:
        out.append("[padmenu] menu closed at vblank 200")
    out.append(log(steps=range(1000, 1100, 2), acc=1.0, name="0.5x", freeze=frozen_after))
    return "\n".join(out)


def good_pad():
    return pad_log()


def test_pad_passes_with_the_expected_sequence():
    ok, message = mc.judge_pad(good_pad())
    assert ok, message


def test_pad_fails_for_each_missing_piece():
    assert "keys" in mc.judge_pad("")[1]
    assert "keys" in mc.judge_pad(good_pad().replace("[padmenu] chord at vblank 100", ""))[1]
    assert "opened then closed" in mc.judge_pad(good_pad().replace("[padmenu] menu opened at vblank 100", ""))[1]
    assert "menu rows changed" in mc.judge_pad(good_pad().replace("[mod:cheats] ROW freeze_ai -> ON", ""))[1]
    assert "game saw the throttle" in mc.judge_pad(good_pad().replace("acc=0.00", "acc=1.00"))[1]
    assert "not held before" in mc.judge_pad(good_pad().replace("acc=1.00", "acc=0.00"))[1]


def test_pad_throttle_must_return_and_cheats_must_act():
    text = good_pad()
    head, tail = text.split("[padmenu] menu closed at vblank 200")
    assert "did not come back" in mc.judge_pad(head + "[padmenu] menu closed at vblank 200" + tail.replace("acc=1.00", "acc=0.00"))[1]
    assert "slow=0.5x freeze=1" in mc.judge_pad(head + "[padmenu] menu closed at vblank 200" + tail.replace("slow=0.5x", "slow=OFF"))[1]
    assert "too few" in mc.judge_pad(good_pad().split("[padmenu] menu closed at vblank 200")[0] + "[padmenu] menu closed at vblank 200")[1]


def test_pad_ai_must_stay_still_after_freeze():
    text = good_pad()
    head, tail = text.split("[padmenu] menu closed at vblank 200")
    # AI cars that move after the menu closed although freeze=1 is logged
    moving_tail = "\n".join(line.replace("A0=-100.000", f"A0=-{100 + i * 3}.000") for i, line in enumerate(tail.split("\n")))
    ok, message = mc.judge_pad(head + "[padmenu] menu closed at vblank 200" + moving_tail)
    assert not ok and "AI car moved" in message


def write_ppm(path, value):
    with open(path, "wb") as fh:
        fh.write(b"P6\n2 2\n255\n" + bytes([value]) * 12)


def make_run(tmp_path, base_val, mod_val, label="idle", done=True):
    for name, val in (("baseline", base_val), (label, mod_val)):
        frames = tmp_path / name / "frames"
        frames.mkdir(parents=True)
        for i in range(8):
            write_ppm(frames / f"frame_{i * 30:06d}.ppm", val)
    (tmp_path / label / "run.log").write_text("[pad] script done at vblank 5\n" if done else "")
    return str(tmp_path)


def test_main_identity(tmp_path, capsys):
    out = make_run(tmp_path, 7, 7)
    assert mc.main(["x", out, "identity", "idle"]) == 0 and "PASS" in capsys.readouterr().out


def test_main_identity_differs_and_not_done(tmp_path, capsys):
    out = make_run(tmp_path, 7, 8)
    assert mc.main(["x", out, "identity", "idle"]) == 1 and "FAIL" in capsys.readouterr().out
    other = tmp_path / "b"
    other.mkdir()
    out = make_run(other, 7, 7, done=False)
    assert mc.main(["x", out, "identity", "idle"]) == 1


def test_main_modes(tmp_path, capsys):
    (tmp_path / "idle").mkdir()
    (tmp_path / "idle" / "run.log").write_text(log())
    (tmp_path / "slow05").mkdir()
    (tmp_path / "slow05" / "run.log").write_text(log(0.5, name="0.5x", steps=range(2, 4000, 2)))
    (tmp_path / "freeze").mkdir()
    (tmp_path / "freeze" / "run.log").write_text(log(freeze=True))
    (tmp_path / "pad").mkdir()
    (tmp_path / "pad" / "run.log").write_text(good_pad())
    out = str(tmp_path)
    assert mc.main(["x", out, "slowmo", "slow05", "0.5"]) == 0
    assert mc.main(["x", out, "freeze", "freeze"]) == 0
    assert mc.main(["x", out, "pad", "pad"]) == 0
    assert mc.main(["x", out, "slowmo", "slow05", "0.3"]) == 2
    assert mc.main(["x", out, "nonsense", "pad"]) == 2
    assert mc.main(["x"]) == 2
    assert capsys.readouterr().out.count("PASS") == 3
