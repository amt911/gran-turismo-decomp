import math

from tools import mod_cameras_check as mc


def cam(vb, view="roof", replay=0, eye=(0.0, 0.0, 2.25), car=(0.0, 0.0, 1.0), carfwd=(0.0, 1.0, 0.0)):
    f = lambda v: ",".join(f"{x:.3f}" for x in v)  # noqa: E731
    return f"[mod:extra-cameras] CAM vb={vb} view={view} replay={replay} eye={f(eye)} fwd=0.000,0.990,-0.140 car={f(car)} carfwd={f(carfwd)} up=0.990"


def draw(seen, expected):
    return "[mod:extra-cameras] DRAW seen=(%.3f %.3f %.3f) expected=(%.3f %.3f %.3f)" % (*seen, *expected)


def write(d, name, lines, frames=None):
    run = d / name
    (run / "frames").mkdir(parents=True, exist_ok=True)
    log = list(lines)
    for i, data in enumerate(frames or []):
        (run / "frames" / f"frame_{i:06d}.ppm").write_bytes(data)
        log.append(f"[frame-dump] frame {i} vblank {i * 30}")
    (run / "run.log").write_text("\n".join(log))
    return str(run)


def good_view_log(view="roof", seen_off=0.0, eye=(0.0, 0.0, 2.25)):
    lines = [cam(i, view, eye=eye) for i in range(80)]
    lines += [draw((0.0 + seen_off, -1.25, -0.2), (0.0, -1.25, -0.2)) for _ in range(30)]
    return lines


def test_idle_needs_identical_frames(tmp_path):
    base = write(tmp_path, "off", [], [bytes([i]) for i in range(30)])
    same = write(tmp_path, "idle", [], [bytes([i]) for i in range(30)])
    assert mc.judge_idle(base, same)[0]
    other = write(tmp_path, "idle2", [], [bytes([i]) for i in range(29)] + [b"x"])
    ok, detail = mc.judge_idle(base, other)
    assert not ok and "1 of 30" in detail
    few = write(tmp_path, "idle3", [], [bytes([i]) for i in range(5)])
    assert not mc.judge_idle(base, few)[0]


def test_view_passes_when_the_game_draws_where_the_pose_says(tmp_path):
    run = write(tmp_path, "roof", good_view_log())
    assert mc.judge_view("roof", run, None)[0]


def test_view_fails_when_the_draw_is_elsewhere(tmp_path):
    run = write(tmp_path, "roof", good_view_log(seen_off=3.0))  # the sabotage: the game's own camera, not the pose
    ok, detail = mc.judge_view("roof", run, None)
    assert not ok and "from where the roof pose" in detail


def test_view_fails_on_distance_few_lines_and_no_draws(tmp_path):
    assert not mc.judge_view("roof", write(tmp_path, "a", good_view_log(eye=(0.0, 0.0, 40.0))), None)[0]
    assert not mc.judge_view("roof", write(tmp_path, "b", good_view_log()[:10]), None)[0]
    assert not mc.judge_view("roof", write(tmp_path, "c", good_view_log()[:80]), None)[0]  # no DRAW lines
    assert not mc.judge_view("wheel", write(tmp_path, "d", good_view_log("roof")), None)[0]  # CAM lines of another view


def test_view_needs_frames_to_differ_from_idle(tmp_path):
    base = write(tmp_path, "idle", [], [bytes([i]) for i in range(30)])
    same = write(tmp_path, "roof", good_view_log(), [bytes([i]) for i in range(30)])
    assert not mc.judge_view("roof", same, base)[0]
    changed = write(tmp_path, "roof2", good_view_log(), [bytes([i + 100]) for i in range(30)])
    assert mc.judge_view("roof", changed, base)[0]


def heli_log(up=9.0, behind=14.0, replay=1, n=300):
    return [cam(i, "heli", replay=replay, eye=(0.0, -behind, 1.0 + up)) for i in range(n)]


def test_heli(tmp_path):
    assert mc.judge_heli(write(tmp_path, "h1", heli_log()))[0]
    assert not mc.judge_heli(write(tmp_path, "h2", heli_log(up=1.0)))[0]       # comes down to the car
    assert not mc.judge_heli(write(tmp_path, "h3", heli_log(behind=0.5)))[0]   # not behind
    assert not mc.judge_heli(write(tmp_path, "h4", heli_log(replay=0)))[0]     # outside the replay
    assert not mc.judge_heli(write(tmp_path, "h5", heli_log(n=50)))[0]         # never took over


def test_main(tmp_path, capsys):
    write(tmp_path, "roof", good_view_log())
    assert mc.main(["x", str(tmp_path), "roof"]) == 0
    assert mc.main(["x", str(tmp_path), "nope"]) == 2
    assert math.isfinite(mc.DRAW_TOL)
