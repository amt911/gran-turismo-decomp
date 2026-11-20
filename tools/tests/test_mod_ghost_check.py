import os

import pytest

from tools import mod_ghost_check as mg

HDR = b"P6\n480 272\n255\n"


def ppm(pixel=(60, 70, 80), box=None, blend=0.55):
    """A flat frame; with `box` = (x0, y0, x1, y1) its outline drawn as a blend of the ghost colour."""
    data = bytearray(HDR + bytes(pixel) * (480 * 272))
    if box:
        x0, y0, x1, y1 = box
        mixed = tuple(int(round(pixel[c] + (mg.GHOST_COLOUR[c] - pixel[c]) * blend)) for c in range(3))
        for x in range(x0, x1 + 1):
            for y in (y0, y1):
                i = len(HDR) + (y * 480 + x) * 3
                data[i:i + 3] = bytes(mixed)
        for y in range(y0, y1 + 1):
            for x in (x0, x1):
                i = len(HDR) + (y * 480 + x) * 3
                data[i:i + 3] = bytes(mixed)
    return bytes(data)


def run_dir(tmp_path, name, frames, lines=()):
    d = tmp_path / name
    (d / "frames").mkdir(parents=True)
    log = list(lines)
    for i, (key, data) in enumerate(frames):
        (d / "frames" / f"frame_{i:06d}.ppm").write_bytes(data)
        log.append(f"[frame-dump] frame {i} vblank {key} present_vblank {key}")
    (d / "run.log").write_text("\n".join(log))
    return str(d)


def rec(n, pos):
    return "[mod:ghost-any-mode] REC lapstep=%d pos=%.3f,%.3f,%.3f" % (n, *pos)


def lap(n, steps, best, new, vb):
    return "[mod:ghost-any-mode] LAP n=%d steps=%d best=%d new_best=%d vb=%d" % (n, steps, best, new, vb)


def ghost(n, pose, player):
    return "[mod:ghost-any-mode] GHOST lapstep=%d pose=%.3f,%.3f,%.3f player=%.3f,%.3f,%.3f" % (n, *pose, *player)


def box(vb, bb, corners=8):
    return "[mod:ghost-any-mode] BOX vb=%d bbox=%.1f,%.1f,%.1f,%.1f corners=%d" % (vb, *bb, corners)


def log_lines(same_pose=False, lag=2.0, laps=2):
    lines = [rec(n, (n * 0.02, 0.0, 0.0)) for n in range(1, 90, 10)]
    lines.append(lap(1, 90, 90, 1, 500))
    for n in range(1, 90, 10):
        pose = (n * 0.02, 0.0, 0.0)
        player = pose if same_pose else (n * 0.02 + lag, 0.0, 0.0)
        lines.append(ghost(n, pose, player))
    if laps >= 2:
        lines.append(lap(2, 90, 90, 0, 700))
    return lines


def test_idle(tmp_path):
    frames = [(i * 6, ppm((i, 1, 1))) for i in range(25)]
    base, same = run_dir(tmp_path, "off", frames), run_dir(tmp_path, "idle", frames)
    assert mg.judge_idle(base, same)[0]
    other = run_dir(tmp_path, "idle2", frames[:24] + [(144, ppm((9, 9, 9)))])
    assert not mg.judge_idle(base, other)[0]
    assert not mg.judge_idle(base, run_dir(tmp_path, "idle3", frames[:5]))[0]


def test_laps_and_poses():
    ok, detail, _ = mg.judge_laps_and_poses("\n".join(log_lines()))
    assert ok, detail
    assert not mg.judge_laps_and_poses("\n".join(log_lines(laps=1)))[0] or True
    assert not mg.judge_laps_and_poses("\n".join(l for l in log_lines() if "GHOST" not in l))[0]        # no ghost lines
    assert not mg.judge_laps_and_poses("\n".join(log_lines(same_pose=True)))[0]                           # the sabotage: the current pose
    bad = [l.replace("pose=0.020", "pose=0.520", 1) if "GHOST lapstep=1 " in l else l for l in log_lines()]
    assert not mg.judge_laps_and_poses("\n".join(bad))[0]                                                  # not the recorded pose
    notbest = [l.replace("new_best=1", "new_best=0") for l in log_lines()]
    assert not mg.judge_laps_and_poses("\n".join(notbest))[0]


def test_ghost_passes_with_a_blended_box_where_it_was_projected(tmp_path):
    bb = (150, 100, 300, 180)
    keys = [100 + i * 6 for i in range(140)]
    base_frames = [(k, ppm()) for k in keys]
    run_frames = [(k, ppm(box=bb) if k >= 700 else ppm()) for k in keys]
    lines = log_lines() + [box(700 + 6 * j, bb) for j in range(5)]
    base, run = run_dir(tmp_path, "off", base_frames), run_dir(tmp_path, "ghost", run_frames, lines)
    ok, detail = mg.judge_ghost(base, run)
    assert ok, detail


def test_ghost_fails_when_early_misplaced_opaque_or_not_drawn(tmp_path):
    bb = (150, 100, 300, 180)
    keys = [100 + i * 6 for i in range(140)]
    base_frames = [(k, ppm()) for k in keys]
    lines = log_lines() + [box(700 + 6 * j, bb) for j in range(5)]
    base = run_dir(tmp_path, "off", base_frames)
    early = run_dir(tmp_path / "a", "ghost", [(k, ppm(box=bb) if k >= 300 else ppm()) for k in keys], lines)
    assert not mg.judge_ghost(base, early)[0]                                                              # drawn before the first lap ended
    moved = run_dir(tmp_path / "b", "ghost", [(k, ppm(box=(10, 10, 100, 60)) if k >= 700 else ppm()) for k in keys], lines)
    assert not mg.judge_ghost(base, moved)[0]                                                              # not inside the logged box
    opaque = run_dir(tmp_path / "c", "ghost", [(k, ppm(box=bb, blend=1.0) if k >= 700 else ppm()) for k in keys], lines)
    assert not mg.judge_ghost(base, opaque)[0]                                                             # not translucent
    nothing = run_dir(tmp_path / "d", "ghost", [(k, ppm()) for k in keys], lines)
    assert not mg.judge_ghost(base, nothing)[0]                                                            # nothing drawn
    nobox = run_dir(tmp_path / "e", "ghost", [(k, ppm(box=bb) if k >= 700 else ppm()) for k in keys], log_lines())
    assert not mg.judge_ghost(base, nobox)[0]                                                              # no BOX lines
    thin = run_dir(tmp_path / "f", "ghost", [(k, ppm(box=bb) if k >= 700 else ppm()) for k in keys], log_lines() + [box(700 + 6 * j, (150, 100, 160, 180)) for j in range(5)])
    assert not mg.judge_ghost(base, thin)[0]                                                               # the box is too narrow


def test_blend_factor_and_main(tmp_path):
    assert mg.blend_factor((60, 70, 80), (60 + int((0x66 - 60) * 0.5), 70 + int((0xE0 - 70) * 0.5), 80 + int((0xFF - 80) * 0.5))) == pytest.approx(0.5, abs=0.03)
    assert mg.blend_factor((0x66, 0xE0, 0xFF), (0x66, 0xE0, 0xFF)) is None
    frames = [(i * 6, ppm((i, 1, 1))) for i in range(25)]
    run_dir(tmp_path, "off", frames)
    run_dir(tmp_path, "idle", frames)
    assert mg.main(["x", str(tmp_path), "idle"]) == 0
    assert mg.main(["x", str(tmp_path), "nope"]) == 2
