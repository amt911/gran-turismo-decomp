from tools import mods_together_check as m

IDS = ["analog-pedals", "draw-distance", "orbit-camera"]
GOOD = "\n".join([
    "[mods] loaded: analog-pedals draw-distance orbit-camera skipped:",
    "[mod:draw-distance] far_scale=2.500",
    "[mod:draw-distance] world depth call from ra=08AF0000",
    "[mod:analog-pedals] step=60 player thr=0.5000 brk=0.3000 eff=0.5000 ai thr=1.0000 brk=0.0000",
    "[mod:orbit-camera] step=60 yaw=90.00 scene=0 global=0 pivot=0.00,0.00,0.00 uploads=10 edits=4 rej(flags/ortho/pivot)=0/0/0",
    "script done at vblank 18800",
])


def test_all_loaded_acted_and_done_passes():
    assert m.judge(GOOD, IDS) == (True, [])


def test_hook_conflict_skip_fails():
    log = GOOD.replace("orbit-camera skipped:", "skipped: [orbit-camera: hook conflict: x]")
    ok, problems = m.judge(log, IDS)
    assert not ok
    assert any("not loaded: orbit-camera" in p for p in problems)
    assert any(p.startswith("skipped:") for p in problems)


def test_no_loaded_line_fails():
    ok, problems = m.judge("script done at vblank 1\n", ["x"])
    assert not ok and "no `[mods] loaded:`" in problems[0]


def test_route_not_finished_fails():
    ok, problems = m.judge(GOOD.replace("script done at vblank 18800", ""), IDS)
    assert not ok and any("script done" in p for p in problems)


def test_mod_that_did_not_act_fails():
    ok, problems = m.judge(GOOD.replace("edits=4", "edits=0"), IDS)
    assert not ok and problems == [p for p in problems if p.startswith("orbit-camera:")]
    ok, _ = m.judge(GOOD.replace("thr=0.5000 brk=0.3000 eff", "thr=1.0000 brk=0.0000 eff"), IDS)
    assert not ok
    ok, _ = m.judge(GOOD.replace("world depth call", "something"), IDS)
    assert not ok
    assert m.judge(GOOD.replace("thr=0.5000 brk=0.3000 eff", "thr=0.4999 brk=0.3000 eff"), IDS) == (True, [])


def test_mod_without_rule_only_has_to_load():
    log = GOOD.replace("orbit-camera skipped:", "orbit-camera new-mod skipped:")
    assert m.judge(log, IDS + ["new-mod"]) == (True, [])


def test_main_exit_codes(tmp_path, capsys):
    p = tmp_path / "run.log"
    p.write_text(GOOD)
    assert m.main([str(p), *IDS]) == 0
    assert "PASS" in capsys.readouterr().out
    p.write_text("nothing")
    assert m.main([str(p), *IDS]) == 1
    assert m.main([str(tmp_path / "missing.log"), "x"]) == 2
    assert m.main([str(p)]) == 2


def test_reset_to_track_must_show_the_replayed_tap():
    ids = IDS + ["reset-to-track"]
    good = GOOD.replace("orbit-camera skipped:", "orbit-camera reset-to-track skipped:") + "\n[mod:reset-to-track] PADSEL up step=620 tap=1"
    assert m.judge(good, ids) == (True, [])
    ok, problems = m.judge(good.replace("tap=1", "tap=0"), ids)  # a swallowed press: the camera would not change
    assert not ok and problems[0].startswith("reset-to-track: no log line")
