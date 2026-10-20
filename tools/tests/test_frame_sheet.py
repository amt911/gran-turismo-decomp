"""Tests for tools/frame_sheet.py: picking frame dumps by vblank and the seek events of a run log."""

from __future__ import annotations

from tools import frame_sheet as sheet

LOG = """\
[io] seek 3 3f800 vblank 12
[frame-dump] frame 000060 vblank 120
[io] seek 131 1cf2a980 vblank 6739
[io] read 131 4096 -> 4096
[frame-dump] frame 000120 vblank 240
[io] seek 132 3152ea00 vblank 6740
[io] seek 133 1cdb9140 vblank 7931
[frame-dump] frame 000180 vblank 7990
[pad] WAIT seek=484943168 satisfied at vblank 8000 (waited 69)
"""


def test_parse_frames_reads_number_and_vblank():
    assert sheet.parse_frames(LOG) == [(60, 120), (120, 240), (180, 7990)]


def test_parse_seeks_reads_vblank_and_offset():
    assert sheet.parse_seeks(LOG) == [(12, 0x3F800), (6739, 0x1CF2A980), (6740, 0x3152EA00), (7931, 0x1CDB9140)]


def test_pick_takes_the_dump_nearest_each_wanted_vblank():
    frames = sheet.parse_frames(LOG)
    assert sheet.pick(frames, [100, 300, 8000]) == [(60, 120), (120, 240), (180, 7990)]


def test_pick_skips_repeats():
    frames = sheet.parse_frames(LOG)
    assert sheet.pick(frames, [100, 110]) == [(60, 120)]


def test_clusters_group_seeks_by_gap():
    seeks = sheet.parse_seeks(LOG)
    assert sheet.clusters(seeks, 25, min_offset=0x1000000) == [(6739, [0x1CF2A980, 0x3152EA00]), (7931, [0x1CDB9140])]


def test_waits_reads_the_satisfied_lines():
    assert sheet.parse_waits(LOG) == [(8000, 69, "seek=484943168")]
