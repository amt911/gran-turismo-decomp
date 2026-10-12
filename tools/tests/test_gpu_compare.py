from pathlib import Path

import pytest

from tools.gpu_compare import Image, compare, main, pairs, read_ppm


def write_ppm(path: Path, width: int, height: int, pixels: bytes) -> Path:
    path.write_bytes(b"P6\n%d %d\n255\n" % (width, height) + pixels)
    return path


def solid(width: int, height: int, rgb: tuple[int, int, int]) -> Image:
    return Image(width, height, bytes(rgb) * (width * height))


def test_read_ppm_roundtrip(tmp_path: Path):
    path = write_ppm(tmp_path / "a.ppm", 2, 1, bytes([1, 2, 3, 4, 5, 6]))
    image = read_ppm(path)
    assert (image.width, image.height, image.pixels) == (2, 1, bytes([1, 2, 3, 4, 5, 6]))


def test_read_ppm_rejects_truncated(tmp_path: Path):
    path = write_ppm(tmp_path / "a.ppm", 4, 4, bytes(3))
    with pytest.raises(ValueError):
        read_ppm(path)


def test_read_ppm_rejects_other_formats(tmp_path: Path):
    path = tmp_path / "a.ppm"
    path.write_bytes(b"P3\n1 1\n255\n0 0 0\n")
    with pytest.raises(ValueError):
        read_ppm(path)


def test_identical_images_pass():
    image = solid(16, 16, (10, 20, 30))
    result = compare(image, image)
    assert result.differing == 0 and result.blocks == 0 and result.passed


def test_rounding_within_tolerance_passes():
    result = compare(solid(16, 16, (100, 100, 100)), solid(16, 16, (120, 80, 120)))
    assert result.differing == 0 and result.passed


def test_missing_block_fails_even_below_the_share():
    soft = solid(160, 160, (255, 255, 255))
    gpu = bytearray(soft.pixels)
    for y in range(8, 16):
        for x in range(8, 16):
            gpu[(y * 160 + x) * 3:(y * 160 + x) * 3 + 3] = b"\0\0\0"
    result = compare(soft, Image(160, 160, bytes(gpu)))
    assert result.differing == 64
    assert result.differing / result.total < 0.01
    assert result.blocks >= 1 and not result.passed


def test_scattered_differences_over_share_fail():
    soft = solid(100, 100, (50, 50, 50))
    gpu = bytearray(soft.pixels)
    for i in range(0, 100 * 100, 50):  # 2 % of the pixels, never a full 8x8 block
        gpu[i * 3] = 150
    result = compare(soft, Image(100, 100, bytes(gpu)))
    assert result.blocks == 0 and result.differing == 200 and not result.passed


def test_size_mismatch_raises():
    with pytest.raises(ValueError):
        compare(solid(2, 2, (0, 0, 0)), solid(3, 2, (0, 0, 0)))


def test_pairs_reports_missing_gpu_frame(tmp_path: Path):
    write_ppm(tmp_path / "soft_000000.ppm", 1, 1, bytes(3))
    write_ppm(tmp_path / "gpu_000000.ppm", 1, 1, bytes(3))
    write_ppm(tmp_path / "soft_000003.ppm", 1, 1, bytes(3))
    found = pairs(tmp_path)
    assert [(index, gpu is None) for index, _, gpu in found] == [(0, False), (3, True)]


def test_cli_exit_codes(tmp_path: Path):
    good = tmp_path / "good"
    good.mkdir()
    write_ppm(good / "soft_000000.ppm", 1, 1, bytes(3))
    write_ppm(good / "gpu_000000.ppm", 1, 1, bytes(3))
    assert main([str(good)]) == 0
    bad = tmp_path / "bad"
    bad.mkdir()
    write_ppm(bad / "soft_000000.ppm", 1, 1, bytes(3))
    assert main([str(bad)]) == 1
    assert main([str(tmp_path / "nowhere")]) == 2
    empty = tmp_path / "empty"
    empty.mkdir()
    assert main([str(empty)]) == 2


def test_cli_writes_report_and_diffs(tmp_path: Path):
    frames = tmp_path / "frames"
    frames.mkdir()
    white = bytes([255]) * (16 * 16 * 3)
    write_ppm(frames / "soft_000000.ppm", 16, 16, white)
    write_ppm(frames / "gpu_000000.ppm", 16, 16, white)
    write_ppm(frames / "soft_000030.ppm", 16, 16, white)
    write_ppm(frames / "gpu_000030.ppm", 16, 16, bytes(16 * 16 * 3))
    report, diffs = tmp_path / "report.txt", tmp_path / "diffs"
    assert main([str(frames), "--report", str(report), "--diff-dir", str(diffs)]) == 1
    lines = report.read_text().splitlines()
    assert len(lines) == 2 and lines[0].startswith("000000 pass") and lines[1].startswith("000030 FAIL")
    assert [p.name for p in diffs.iterdir()] == ["diff_000030.ppm"]
    diff = read_ppm(diffs / "diff_000030.ppm")
    assert diff.pixels[:3] == bytes([255, 0, 0])
