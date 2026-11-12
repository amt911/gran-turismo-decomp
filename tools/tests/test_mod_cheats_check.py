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


def pad_log(acc_inside=0.0, rows=True, keys=("chord", "close", "right", "right", "down", "right", "chord"), states=("opened", "closed"), frozen_after=True):
    out = []
    out.append(log(steps=range(800, 900, 2), acc=1.0))
    out.append(f"[padmenu] {keys[0]} at vblank 100")
    if "opened" in states:
        out.append("[padmenu] menu opened at vblank 100")
    out.append(log(steps=range(900, 960, 2), acc=acc_inside))
    for i, k in enumerate(keys[1:-1]):
        out.append(f"[padmenu] {k} at vblank {110 + i}")
        # the first Right opens Race (no row changes); the next two set slow motion, then (after Down) freeze AI
        rights_before = list(keys[1:-1])[:i].count("right")
        if rows and k == "right" and rights_before >= 1:
            out.append("[mod:cheats] ROW slow_motion -> 0.5x" if rights_before == 1 else "[mod:cheats] ROW freeze_ai -> ON")
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
    assert "keys" in mc.judge_pad(pad_log(keys=("chord", "down", "right", "down", "right", "chord")))[1]  # the old F9 list's keys
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


def test_callguest_judge():
    ok_line = "[mod:cheats] CALLGUEST status=0 v0=0\n"
    assert mc.judge_callguest(ok_line * 3)[0]
    assert "CALLGUEST lines" in mc.judge_callguest(ok_line)[1]
    assert not mc.judge_callguest(ok_line * 3 + "[mod:cheats] CALLGUEST status=2 v0=0\n")[0]
    assert not mc.judge_callguest(ok_line * 3 + "[mod:cheats] CALLGUEST status=0 v0=1\n")[0]


def test_toggle_judge():
    before = log(steps=range(2, 40, 2))
    ok, message = mc.judge_toggle(before + "\n[mods] cheats -> OFF\n")
    assert ok, message
    assert "no `[mods] cheats -> OFF`" in mc.judge_toggle(before)[1]
    assert "only" in mc.judge_toggle(log(steps=range(2, 8, 2)) + "\n[mods] cheats -> OFF\n")[1]
    assert "while switched off" in mc.judge_toggle(before + "\n[mods] cheats -> OFF\n" + log(steps=range(40, 44, 2)))[1]
    assert "while switched off" in mc.judge_toggle(before + "\n[mods] cheats -> OFF\n[mod:cheats] CALLGUEST status=0 v0=0\n")[1]


def phased(rates, per=30, base_spd=12.0):
    """Log lines whose distance per step per m/s follows `rates` (one per phase), phases OFF, 0.5x, 0.25x, OFF."""
    lines, st, x = [], 0, 0.0
    for name, rate in zip(("OFF", "0.5x", "0.25x", "OFF"), rates):
        for _ in range(per):
            st += 2
            x -= rate * 2 * base_spd
            lines.append(f"[mod:cheats] st={st} slow={name} freeze=0 P={x:.4f},250.000,2.000 spd={base_spd:.3f} acc=0.00 A0=-100.000,20.000,2.000")
    return "\n".join(lines)


def test_padoff_judge():
    ok, message = mc.judge_padoff(phased((0.033, 0.0165, 0.00825, 0.033)))
    assert ok, message
    assert "expected 0.5" in mc.judge_padoff(phased((0.033, 0.033, 0.00825, 0.033)))[1]
    assert "expected 1.0" in mc.judge_padoff(phased((0.033, 0.0165, 0.00825, 0.0165)))[1]  # still slow after OFF
    assert "phases" in mc.judge_padoff(phased((0.033, 0.0165, 0.00825, 0.033)).replace("0.25x", "0.5x"))[1]
    assert "measurable" in mc.judge_padoff(phased((0.033, 0.0165, 0.00825, 0.033), base_spd=1.0))[1]


def test_padreset_judge():
    text = good_pad()
    assert mc.judge_padreset(text)[0]
    assert "saw Select" in mc.judge_padreset(text + "\n[mod:reset-to-track] PADSEL up step=5 tap=1\n")[1]
    assert not mc.judge_padreset("")[0]
    longer = text.replace("[padmenu] down at vblank 110", "[padmenu] down at vblank 108\n[padmenu] down at vblank 110")
    assert mc.judge_pad(longer)[0]


def test_measure_and_main_modes(tmp_path, capsys):
    m = mc.measure_slowmo(log(), log(0.5, name="0.5x", steps=range(2, 4000, 2)), 0.5)
    assert m is not None and m["curve"] < 0.01 and m["time"] < 0.02 and m["ai"] < 0.5
    assert mc.measure_slowmo("", "", 0.5) is None
    for name, text in (("idle", log()), ("slowm", log(0.5, name="0.5x", steps=range(2, 4000, 2))), ("callguest", "[mod:cheats] CALLGUEST status=0 v0=0\n" * 3),
                       ("toggle", log(steps=range(2, 40, 2)) + "\n[mods] cheats -> OFF\n"), ("padoff", phased((0.033, 0.0165, 0.00825, 0.033))),
                       ("padreset", good_pad())):
        (tmp_path / name).mkdir()
        (tmp_path / name / "run.log").write_text(text)
    out = str(tmp_path)
    assert mc.main(["x", out, "measure", "slowm", "0.5"]) == 0 and "MEASURE" in capsys.readouterr().out
    assert mc.main(["x", out, "callguest", "callguest"]) == 0
    assert mc.main(["x", out, "padoff", "padoff"]) == 0
    assert mc.main(["x", out, "padreset", "padreset"]) == 0
    make = tmp_path / "t"
    make.mkdir()
    base = tmp_path / "baseline" / "frames"
    base.mkdir(parents=True)
    (tmp_path / "toggle" / "frames").mkdir()
    for i in range(25):
        write_ppm(base / f"frame_{i * 30:06d}.ppm", 3)
        write_ppm(tmp_path / "toggle" / "frames" / f"frame_{i * 30:06d}.ppm", 3)
    assert mc.main(["x", out, "toggle", "toggle"]) == 0
    write_ppm(tmp_path / "toggle" / "frames" / "frame_000030.ppm", 4)
    assert mc.main(["x", out, "toggle", "toggle"]) == 1
    (tmp_path / "unusable").mkdir()
    (tmp_path / "unusable" / "run.log").write_text("")
    assert mc.main(["x", out, "measure", "unusable", "0.5"]) == 1


def replay_run(rep_slow="0.5x", drift=0.0, marker=True, shift=-14, ai_drift=0.0):
    """Live race then its replay (cheats 0.4.0): a line per step with the steps since the player first moved; slow motion from since 100."""
    def phase(rep, slow_from, dx, adx, off):
        out = []
        for since in range(-30, 400):
            st = since + 200 + (off if rep else 0)
            slow = (rep_slow if rep else "0.5x") if since >= slow_from else "OFF"
            px = -300.0 - max(since, 0) * 0.5 + dx * (since > 0)
            out.append(f"[mod:cheats] st={st} slow={slow} freeze=0 P={px:.3f},250.000,2.000 spd=10.000 acc=1.00 "
                       f"A0={-100 - max(since, 0) * 0.4 + adx * (since > 0):.3f},20.000,2.000 since={since if since >= 0 else -1} rep={1 if rep else 0} ai=1x")
        return out
    lines = phase(False, 100, 0.0, 0.0, 0)
    if marker:
        lines.append("[mod:cheats] REPLAY start tape=400")
        lines += phase(True, 100, drift, ai_drift, shift)
    return "\n".join(lines)


def test_replay_same_controls_and_paths_pass():
    ok, message = mc.judge_replay(replay_run(drift=0.1))
    assert ok, message


def test_replay_missing_fails():
    ok, message = mc.judge_replay(replay_run(marker=False))
    assert not ok and "REPLAY" in message


def test_replay_without_the_cheat_fails():
    ok, message = mc.judge_replay(replay_run(rep_slow="OFF"))
    assert not ok and "controls" in message


def test_replay_drift_fails_player_and_ai():
    ok, message = mc.judge_replay(replay_run(drift=mc.MAX_REPLAY_DRIFT + 0.2))
    assert not ok and "player" in message
    ok, message = mc.judge_replay(replay_run(ai_drift=mc.MAX_REPLAY_DRIFT + 0.2))
    assert not ok and "A0" in message


def test_replay_main_mode(tmp_path):
    (tmp_path / "replay").mkdir()
    (tmp_path / "replay" / "run.log").write_text(replay_run())
    assert mc.main(["x", str(tmp_path), "replay", "replay"]) == 0


def hotkey_log(bound=True, fired=True, acc_after=1.0):
    out = ["[padmenu] chord at vblank 100", "[padmenu] menu opened at vblank 100"]
    if bound:
        out.append("[device] bound cheats.nitro pad R+SQUARE at vblank 150")
    out += ["[padmenu] chord at vblank 200", "[padmenu] menu closed at vblank 200", log(steps=range(800, 900, 2), acc=0.0)]
    if fired:
        out.append("[device] action cheats.nitro at vblank 300")
    out.append(log(steps=range(900, 1000, 2), acc=acc_after))
    return "\n".join(out)


def test_hotkey_judge():
    ok, message = mc.judge_hotkey(hotkey_log())
    assert ok, message
    assert "not bound" in mc.judge_hotkey(hotkey_log(bound=False))[1]
    assert "never fired" in mc.judge_hotkey(hotkey_log(fired=False))[1]
    assert "full throttle" in mc.judge_hotkey(hotkey_log(acc_after=0.0))[1]
    assert "before" in mc.judge_hotkey(hotkey_log().replace("acc=0.00", "acc=1.00"))[1]


def test_conflict_judge():
    refused = ("[padmenu] chord at vblank 100\n[device] refused cheats.nitro pad SELECT+START: SELECT+START is already Open the cheat device "
               "at vblank 150\n[padmenu] chord at vblank 200")
    ok, message = mc.judge_conflict(refused)
    assert ok, message
    assert "not refused" in mc.judge_conflict("[device] bound cheats.nitro pad SELECT+START at vblank 150")[1]
    assert "not refused" in mc.judge_conflict("")[1]
    assert "owner" in mc.judge_conflict(refused.replace("Open the cheat device", "something"))[1]
