import os

import pytest

from tools import mod_weather_check as mw


def ppm(width=480, height=272, pixel=(100, 120, 140)):
    return b"P6\n%d %d\n255\n" % (width, height) + bytes(pixel) * (width * height)


def graded_ppm(pixel, grade):
    return ppm(pixel=mw.apply_grade(pixel, grade))


def run_dir(tmp_path, name, frames, grade_lines=()):
    d = tmp_path / name
    (d / "frames").mkdir(parents=True)
    log = list(grade_lines)
    for i, data in enumerate(frames):
        (d / "frames" / f"frame_{i:06d}.ppm").write_bytes(data)
        log.append(f"[frame-dump] frame {i} vblank {i * 30 + 5} present_vblank {i * 30}")
    (d / "run.log").write_text("\n".join(log))
    return str(d)


def grade_line(vb, steps, grade, mode=3, rain=0):
    return "[mod:weather] GRADE vb=%d steps=%d mode=%d rain=%d rgb=%.4f,%.4f,%.4f bright=%.4f sat=%.4f" % (vb, steps, mode, rain, *grade)


def test_apply_grade_neutral_and_known_values():
    assert mw.apply_grade((10, 200, 255), mw.NEUTRAL) == (10, 200, 255)
    assert mw.apply_grade((200, 200, 200), (1, 1, 1, 0.5, 1)) == (100, 100, 100)
    assert mw.apply_grade((255, 0, 0), (1, 1, 1, 1, 0)) == (54, 54, 54)  # desaturated to its Rec.709 luma
    assert mw.apply_grade((255, 255, 255), (2, 2, 2, 2, 1)) == (255, 255, 255)  # clamped
    assert mw.combine((1, 2, 3, 4, 5), (2, 2, 2, 2, 2)) == (2, 4, 6, 8, 10)
    assert mw.rain_grade(0) == mw.NEUTRAL and mw.rain_grade(3) == (0.86, 0.91, 1.0, 0.82, 0.62)


def test_idle_needs_identical_frames(tmp_path):
    same = [ppm(pixel=(i, 5, 5)) for i in range(25)]
    base, ok = run_dir(tmp_path, "off", same), run_dir(tmp_path, "idle", same)
    assert mw.judge_idle(base, ok)[0]
    changed = run_dir(tmp_path, "idle2", same[:24] + [ppm(pixel=(1, 1, 1))])
    assert not mw.judge_idle(base, changed)[0]
    assert not mw.judge_idle(base, run_dir(tmp_path, "idle3", same[:5]))[0]


def preset_runs(tmp_path, name, applied=True):
    base_frames = [ppm(pixel=(60 + 4 * i, 90, 120)) for i in range(25)]
    grade = mw.PRESETS[name]
    run_frames = [graded_ppm((60 + 4 * i, 90, 120), grade) if applied else base_frames[i] for i in range(25)]
    lines = [grade_line(0, 0, mw.NEUTRAL), grade_line(30, 1, grade)]
    return run_dir(tmp_path, "off", base_frames), run_dir(tmp_path, name, run_frames, lines)


def test_preset_passes_when_the_frames_are_graded_and_fails_when_not(tmp_path):
    base, run = preset_runs(tmp_path, "dusk")
    ok, detail = mw.judge_preset(base, run, "dusk")
    assert ok, detail
    base2, run2 = preset_runs(tmp_path / "s", "dusk", applied=False)  # the sabotage: the grade is never applied
    assert not mw.judge_preset(base2, run2, "dusk")[0]
    base3, run3 = preset_runs(tmp_path / "t", "sunset")  # graded, but the judge is told dusk
    assert not mw.judge_preset(base3, run3, "dusk")[0]


def test_preset_needs_a_logged_race(tmp_path):
    base, run = preset_runs(tmp_path, "dusk")
    open(os.path.join(run, "run.log"), "a").close()
    text = open(os.path.join(run, "run.log")).read().replace("steps=1", "steps=0")
    open(os.path.join(run, "run.log"), "w").write(text)
    assert not mw.judge_preset(base, run, "dusk")[0]


def test_progressive_must_darken_monotonically(tmp_path):
    base_frames = [ppm(pixel=(120, 120, 120)) for _ in range(30)]
    ratios = [1.0 - 0.4 * i / 29 for i in range(30)]
    run_frames = [graded_ppm((120, 120, 120), (1, 1, 1, r, 1)) for r in ratios]
    base = run_dir(tmp_path, "off", base_frames)
    run = run_dir(tmp_path, "progressive", run_frames, [grade_line(0, 5, (1, 1, 1, 1, 1)), grade_line(30, 6, (1, 1, 1, 0.99, 1))])
    ok, detail = mw.judge_progressive(base, run)
    assert ok, detail
    flat = run_dir(tmp_path / "f", "progressive", base_frames, [grade_line(30, 6, (1, 1, 1, 0.99, 1))])
    assert not mw.judge_progressive(base, flat)[0]  # no darkening
    zigzag = run_dir(tmp_path / "z", "progressive", [graded_ppm((120, 120, 120), (1, 1, 1, r, 1)) for r in ratios[:10] + ratios[:1] * 5 + ratios[15:]], [grade_line(30, 6, (1, 1, 1, 0.99, 1))])
    assert not mw.judge_progressive(base, zigzag)[0]  # it brightens again


def test_rain_needs_streak_pixels_over_the_rain_grade(tmp_path):
    base_frames = [ppm(pixel=(80, 80, 80)) for _ in range(25)]
    grade = mw.rain_grade(3)
    plain = mw.apply_grade((80, 80, 80), grade)

    def with_streaks(share):
        data = bytearray(graded_ppm((80, 80, 80), grade))
        width = 480
        n = int(share * 160 * 91)  # sampled pixels are one in STRIDE x STRIDE
        count = 0
        for y in range(0, 272, mw.STRIDE):
            for x in range(0, 480, mw.STRIDE):
                if count < n:
                    i = len(b"P6\n480 272\n255\n") + (y * width + x) * 3
                    data[i:i + 3] = bytes((plain[0] + 90, plain[1] + 90, plain[2] + 90))
                    count += 1
        return bytes(data)

    base = run_dir(tmp_path, "off", base_frames)
    lines = [grade_line(30, 1, grade, rain=3)]
    assert mw.judge_rain(base, run_dir(tmp_path, "rain", [with_streaks(0.02)] * 25, lines), 3)[0]
    assert not mw.judge_rain(base, run_dir(tmp_path / "n", "rain", [with_streaks(0.0)] * 25, lines), 3)[0]   # nothing drawn
    assert not mw.judge_rain(base, run_dir(tmp_path / "m", "rain", [with_streaks(0.5)] * 25, lines), 3)[0]   # far too much


def test_main(tmp_path):
    base, run = preset_runs(tmp_path, "dusk")
    assert mw.main(["x", str(tmp_path), "dusk"]) == 0
    assert mw.main(["x", str(tmp_path), "nope"]) == 2
