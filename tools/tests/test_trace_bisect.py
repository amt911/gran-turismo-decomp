"""Tests for tools/trace_bisect.py: first divergence of two traces and the halving search."""

from __future__ import annotations

import pytest

from tools import trace_bisect as bisect


def test_first_divergence_is_none_for_equal_traces():
    assert bisect.first_divergence(["a", "b"], ["a", "b"]) is None


def test_first_divergence_finds_the_first_differing_line():
    assert bisect.first_divergence(["a", "b", "c"], ["a", "x", "c"]) == 1


def test_first_divergence_counts_a_shorter_trace_where_it_ends():
    assert bisect.first_divergence(["a", "b", "c"], ["a", "b"]) == 2


def test_ddmin_finds_a_single_culprit():
    calls = []

    def diverges(subset):
        calls.append(len(subset))
        return 7 in subset

    assert bisect.ddmin(list(range(16)), diverges) == [7]
    assert len(calls) < 16  # halving, not one run per function


def test_ddmin_keeps_two_functions_that_diverge_only_together():
    def diverges(subset):
        return 3 in subset and 9 in subset

    assert sorted(bisect.ddmin(list(range(12)), diverges)) == [3, 9]


def test_ddmin_with_a_set_that_cannot_shrink_returns_it():
    assert sorted(bisect.ddmin([1, 2, 3], lambda subset: len(subset) == 3)) == [1, 2, 3]


def _ppm(*pixels: int) -> bytes:
    return b"P6\n2 1\n255\n" + bytes(pixels)


def test_ppm_mean_of_a_black_image_is_zero():
    assert bisect.ppm_mean(_ppm(0, 0, 0, 0, 0, 0)) == 0.0


def test_ppm_mean_is_the_average_brightness():
    assert bisect.ppm_mean(_ppm(255, 255, 255, 0, 0, 0)) == 0.5


def test_ppm_mean_rejects_other_formats():
    import pytest

    with pytest.raises(ValueError):
        bisect.ppm_mean(b"P5\n1 1\n255\n\x00")


def test_first_frame_after_picks_the_first_dump_at_or_after_the_vblank():
    log = "[frame-dump] frame 000030 vblank 100\n[frame-dump] frame 000060 vblank 220\n[frame-dump] frame 000090 vblank 340\n"
    assert bisect.first_frame_after(log, 200) == 60
    assert bisect.first_frame_after(log, 341) is None


def test_crashed_recognises_an_uncaught_exception_and_the_runtime_error():
    assert bisect.crashed("x\nterminate called after throwing an instance of 'psprecomp::Error'\n")
    assert bisect.crashed("gt_native error: Guest memory access outside PSP RAM\n")
    assert not bisect.crashed("Runtime stopped: Wall-clock limit reached at 0x08B2ACDC\n")


def _ppm4(*pixels: int) -> bytes:  # 2x2 image
    return b"P6\n2 2\n255\n" + bytes(pixels)


def test_ppm_box_diff_is_zero_for_equal_images():
    image = _ppm4(*range(12))
    assert bisect.ppm_box_diff(image, image, (0, 0, 2, 2)) == 0.0


def test_ppm_box_diff_is_the_mean_absolute_difference_inside_the_box():
    white = _ppm4(*[255] * 12)
    # right column black: pixels (1,0) and (1,1)
    half = _ppm4(255, 255, 255, 0, 0, 0, 255, 255, 255, 0, 0, 0)
    assert bisect.ppm_box_diff(white, half, (0, 0, 2, 2)) == 0.5
    assert bisect.ppm_box_diff(white, half, (1, 0, 1, 2)) == 1.0
    assert bisect.ppm_box_diff(white, half, (0, 0, 1, 2)) == 0.0


def test_ppm_box_diff_rejects_images_of_different_sizes():
    import pytest

    with pytest.raises(ValueError):
        bisect.ppm_box_diff(_ppm4(*[0] * 12), _ppm(0, 0, 0, 0, 0, 0), (0, 0, 1, 1))


def test_parse_box_reads_x_y_w_h():
    assert bisect.parse_box("10,20,30,40") == (10, 20, 30, 40)


def test_ppm_box_mean_is_the_brightness_inside_the_box():
    half = _ppm4(255, 255, 255, 0, 0, 0, 255, 255, 255, 0, 0, 0)
    assert bisect.ppm_box_mean(half, (0, 0, 2, 2)) == 0.5
    assert bisect.ppm_box_mean(half, (1, 0, 1, 2)) == 0.0


def test_script_done_vblank_reads_the_pad_log():
    assert bisect.script_done_vblank("x\n[pad] script done at vblank 16954\n") == 16954
    assert bisect.script_done_vblank("no route end") is None


def test_decomp_only_value_is_hex_without_prefix():
    assert bisect.decomp_only_value([0x0884D9F0, 0x08800000]) == "0884D9F0,08800000"


def test_decomp_only_value_refuses_a_list_past_the_kernel_cap():
    import pytest

    bisect.decomp_only_value(list(range(13000)))  # 3/4 of today's set fits
    with pytest.raises(ValueError):
        bisect.decomp_only_value(list(range(15000)))


def test_narrow_window_finds_the_span_of_a_cumulative_symptom():
    items = list(range(100))
    # diverges when the set holds both 30 and 70 (and everything else is irrelevant)
    calls = []

    def diverges(subset):
        calls.append(len(subset))
        return 30 in subset and 70 in subset

    lo, hi = bisect.narrow_window(items, diverges)
    assert items[lo:hi] == list(range(30, 71))
    assert len(calls) <= 16  # two binary searches


def test_narrow_window_of_a_single_culprit_is_that_item():
    lo, hi = bisect.narrow_window(list(range(64)), lambda s: 41 in s)
    assert (lo, hi) == (41, 42)


def test_ddmin_with_a_batch_evaluator_matches_the_sequential_result():
    batches = []

    def evaluate(subsets):
        batches.append(len(subsets))
        return [3 in s and 9 in s for s in subsets]

    sequential = bisect.ddmin(list(range(12)), lambda s: 3 in s and 9 in s)
    parallel = bisect.ddmin(list(range(12)), lambda s: 3 in s and 9 in s, evaluate=evaluate)
    assert sorted(parallel) == sorted(sequential) == [3, 9]
    assert max(batches) > 1  # subsets of a level went out together


def test_run_env_disables_mods_unless_kept():
    env = bisect.run_env({"HOME": "/h"}, "3300:START", keep_mods=False)
    assert env["GT_NO_MODS"] == "1" and env["GT_PAD"] == "3300:START" and env["LCS_UNCAPPED"] == "1"
    kept = bisect.run_env({"HOME": "/h", "GT_MODS_DIR": "/m"}, "3300:START", keep_mods=True)
    assert "GT_NO_MODS" not in kept and kept["GT_MODS_DIR"] == "/m"


def test_crash_summary_counts_runs_and_names_the_faults():
    crash = "terminate called after throwing an instance of 'psprecomp::Error'\n  what():  Guest memory access outside PSP RAM/EDRAM at 0x3EEE1474\n"
    other = "terminate called after throwing an instance of 'psprecomp::Error'\n  what():  Guest memory access outside PSP RAM/EDRAM at 0x00000000\n"
    ok = "Runtime stopped: Wall-clock limit reached at 0x08B2ACDC\n"
    died, text = bisect.crash_summary([ok, crash, other, ok])
    assert died and text == "2/4 crashed (0x00000000, 0x3EEE1474)"
    died, text = bisect.crash_summary([ok, ok])
    assert not died and text == "0/2 crashed"
    died, text = bisect.crash_summary(["gt_native error: something else\n"])
    assert died and text == "1/1 crashed"


def test_crash_summary_refuses_a_run_that_never_played():
    # A run that exits at once (a bad GT_PAD, a missing file) neither crashed nor reached its window: counting it as
    # "ok" would bisect on runs that never ran. It is an error, named with the log's first line.
    ok = "Runtime stopped: Wall-clock limit reached at 0x08B2ACDC\n"
    early = "GT_PAD: 'x.pad': expected <vblank>:<BUTTON> or WAIT:<signal>=<value>\n"
    with pytest.raises(RuntimeError, match="neither crashed nor reached its time limit.*GT_PAD: 'x.pad'"):
        bisect.crash_summary([ok, early])


def test_registered_functions_list_must_not_be_empty():
    with pytest.raises(RuntimeError, match="no registered functions"):
        bisect.require_functions([], "GT_PAD: bad\n")
    assert bisect.require_functions([0x08804000], "") == [0x08804000]
