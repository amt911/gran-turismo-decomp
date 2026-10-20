"""Tests for tools/trace_bisect.py: first divergence of two traces and the halving search."""

from __future__ import annotations

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
