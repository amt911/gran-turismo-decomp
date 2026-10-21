"""tools.frame_pacing: summaries of a GT_FRAME_LOG run (synthetic logs only, no game data)."""

from __future__ import annotations

import pytest

from tools import frame_pacing as fp

HEADER = "# gt-frame-log v1\n"


def present(wall_us, vblank, new=1, gpu_ready=1, skip=0, dur_us=500):
    return f"P {wall_us} {vblank} {vblank * 16683} {new} {gpu_ready} {skip} {dur_us}\n"


def vblank(wall_us, vb, lag_us=0, sleep_us=1000, skipped=0, late=0):
    return f"V {wall_us} {vb} {vb * 16683} {lag_us} {sleep_us} {skipped} {late}\n"


def test_percentile_interpolates_between_ranks():
    assert fp.percentile([10, 20, 30, 40], 50) == 25.0
    assert fp.percentile([10, 20, 30, 40], 0) == 10.0
    assert fp.percentile([10, 20, 30, 40], 100) == 40.0
    assert fp.percentile([7], 99) == 7.0


def test_percentile_of_nothing_is_an_error():
    with pytest.raises(ValueError):
        fp.percentile([], 50)


def test_parse_reads_presents_and_vblanks_and_ignores_comments():
    text = HEADER + present(1000, 1) + vblank(900, 1, lag_us=-200) + "\n# note\n" + present(18000, 2, new=0)
    log = fp.parse(text)
    assert [p.wall_us for p in log.presents] == [1000, 18000]
    assert log.presents[1].new is False
    assert log.vblanks[0].lag_us == -200


def test_parse_rejects_a_malformed_row_with_its_line_number():
    with pytest.raises(ValueError, match="line 2"):
        fp.parse(HEADER + "P 1 2 3\n")


def test_parse_rejects_a_foreign_header():
    with pytest.raises(ValueError, match="gt-frame-log"):
        fp.parse("# something else\nP 1 2 3 4 5 6 7\n")


def test_intervals_count_only_new_frames():
    log = fp.parse(HEADER + present(0, 1) + present(16000, 2, new=0) + present(17000, 3) + present(50000, 4))
    assert fp.new_frame_intervals_us(log) == [17000, 33000]


def test_summary_shares_and_percentiles():
    # 8 smooth frames at 16.7 ms, then a 40 ms stall, then one 20 ms
    walls = [0]
    for gap in [16700] * 8 + [40000, 20000]:
        walls.append(walls[-1] + gap)
    log = fp.parse(HEADER + "".join(present(w, i + 1) for i, w in enumerate(walls)))
    s = fp.summarize(log)
    assert s.frames == 11
    assert s.intervals == 10
    assert s.p50_ms == pytest.approx(16.7)
    assert s.max_ms == pytest.approx(40.0)
    assert s.over_17_5_share == pytest.approx(0.2)   # the 40 ms and the 20 ms
    assert s.over_34_share == pytest.approx(0.1)     # the 40 ms
    assert s.longest_stall_at_s == pytest.approx(8 * 0.0167 + 0.040)  # where the longest gap ends


def test_summary_counts_repeats_and_skips():
    log = fp.parse(HEADER + present(0, 1) + present(1, 2, new=0) + present(2, 3, skip=1) + present(3, 4))
    s = fp.summarize(log)
    assert s.repeats == 1
    assert s.skipped_frames == 1
    assert s.presents == 4


def test_summary_vblank_lag():
    log = fp.parse(HEADER + present(0, 1) + present(20000, 2)
                   + vblank(0, 1, lag_us=-300) + vblank(16000, 2, lag_us=9000, skipped=2, late=1)
                   + vblank(33000, 3, lag_us=500))
    s = fp.summarize(log)
    assert s.vblank_lag_max_ms == pytest.approx(9.0)
    assert s.vblanks_skipped == 2
    assert s.vblanks_late == 1
    assert s.vblank_waits == 3


def test_summary_of_too_short_a_run_is_an_error():
    with pytest.raises(ValueError, match="at least two"):
        fp.summarize(fp.parse(HEADER + present(0, 1)))


def test_window_cuts_load_time_off_both_ends():
    log = fp.parse(HEADER + "".join(present(t * 1_000_000, t + 1) for t in range(10))
                   + vblank(500_000, 1) + vblank(5_500_000, 2))
    cut = fp.window(log, start_s=2.0, end_s=6.0)
    assert [p.wall_us for p in cut.presents] == [2_000_000, 3_000_000, 4_000_000, 5_000_000]
    assert len(cut.vblanks) == 1


def test_histogram_buckets_and_total():
    hist = fp.histogram_ms([5.0, 16.0, 16.9, 18.0, 33.0, 70.0, 400.0])
    assert dict(hist)["<8"] == 1
    assert dict(hist)["15-18"] == 2
    assert dict(hist)["18-22"] == 1
    assert dict(hist)["30-36"] == 1
    assert dict(hist)["60-90"] == 1
    assert dict(hist)[">=150"] == 1
    assert sum(n for _, n in hist) == 7


def test_report_mentions_the_key_numbers():
    log = fp.parse(HEADER + "".join(present(t * 16700, t + 1) for t in range(60)))
    text = fp.report(fp.summarize(log), name="race")
    assert "race" in text and "p99" in text and "17.5" in text and "longest stall" in text


def test_main_prints_a_report_per_file(tmp_path, capsys):
    path = tmp_path / "run.log"
    path.write_text(HEADER + "".join(present(t * 16700, t + 1) for t in range(30)))
    assert fp.main([str(path), "--skip-s", "0"]) == 0
    assert "intervals" in capsys.readouterr().out


def test_main_json_is_machine_readable(tmp_path, capsys):
    import json
    path = tmp_path / "run.log"
    path.write_text(HEADER + "".join(present(t * 16700, t + 1) for t in range(30)))
    assert fp.main([str(path), "--json"]) == 0
    data = json.loads(capsys.readouterr().out)
    assert data[0]["frames"] == 30


def test_main_reports_a_bad_file_without_a_traceback(tmp_path, capsys):
    path = tmp_path / "bad.log"
    path.write_text("nope\n")
    assert fp.main([str(path)]) == 2
    assert "bad.log" in capsys.readouterr().err


def display(wall_us, present_id):
    return f"D {wall_us} {present_id}\n"


def test_display_rows_give_the_pacing_the_screen_shows():
    """D rows (VK_KHR_present_wait completions) are the screen-side truth: presents can be evenly spaced while the
    compositor shows them unevenly."""
    text = HEADER + present(0, 1) + present(16683, 2) + present(33366, 3)
    text += display(20000, 1) + display(36700, 2) + display(70000, 3)
    s = fp.summarize(fp.parse(text))
    assert s.displays == 3
    assert s.display_p50_ms == pytest.approx(25.0)
    assert s.display_max_ms == pytest.approx(33.3)
    assert s.display_over_17_5_share == pytest.approx(0.5)


def test_without_display_rows_the_display_figures_are_zero():
    s = fp.summarize(fp.parse(HEADER + present(0, 1) + present(16683, 2)))
    assert s.displays == 0 and s.display_p99_ms == 0.0


def test_window_keeps_display_rows_in_range():
    log = fp.parse(HEADER + present(0, 1) + display(1_000, 1) + display(5_000_000, 2))
    assert [d.present_id for d in fp.window(log, 1.0).displays] == [2]
