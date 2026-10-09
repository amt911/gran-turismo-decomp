from pathlib import Path

import pytest

from tools.checksums import ChecksumError, load_checksums, sha1_of, verify

SHA1_ABC = "a9993e364706816aba3e25717850c26c9cd0d89d"  # FIPS 180 test vector for b"abc"
SHA1_EMPTY = "da39a3ee5e6b4b0d3255bfef95601890afd80709"


def test_load_checksums_reads_sha1sum_format_and_skips_comments(tmp_path: Path):
    f = tmp_path / "c.sha1"
    f.write_text(f"# comment\n\n{SHA1_ABC.upper()}  iso\n{SHA1_EMPTY}  eboot.elf\n")
    assert load_checksums(f) == {"iso": SHA1_ABC, "eboot.elf": SHA1_EMPTY}


def test_load_checksums_rejects_malformed_line_with_line_number(tmp_path: Path):
    f = tmp_path / "c.sha1"
    f.write_text(f"{SHA1_ABC}  iso\nnot-a-hash  eboot.elf\n")
    with pytest.raises(ChecksumError, match=r"c\.sha1:2"):
        load_checksums(f)


def test_sha1_of_hashes_file_contents(tmp_path: Path):
    f = tmp_path / "x"
    f.write_bytes(b"abc")
    assert sha1_of(f) == SHA1_ABC


def test_verify_passes_on_match(tmp_path: Path):
    f = tmp_path / "x"
    f.write_bytes(b"")
    verify(f, SHA1_EMPTY, "eboot.elf")


def test_verify_reports_expected_and_actual_on_mismatch(tmp_path: Path):
    f = tmp_path / "x"
    f.write_bytes(b"abc")
    with pytest.raises(ChecksumError) as e:
        verify(f, SHA1_EMPTY, "iso")
    assert SHA1_EMPTY in str(e.value) and SHA1_ABC in str(e.value) and "iso" in str(e.value)
