from pathlib import Path

import pytest

from tools import synth_corpus as sc

GOOD = "08810010 12 3 reg:1=imm:0xFFFFFFFF word:0x80=live:1 block:3=77"


def test_valid_recipe_accepts_all_value_kinds():
    for line in (GOOD, "08810010 0 1", "08810010 5 1 reg:7=arena:0x40", "08810010 5 1 word:0x4=class:2"):
        assert sc.valid_recipe(line), line


@pytest.mark.parametrize("line", [
    "", "08810010 12", "8810010 12 3", "08810010 12 3 reg:8=imm:1", "08810010 12 3 word:0x3=imm:1", "08810010 12 3 bogus:1=imm:1",
    "08810010 12 3 reg:1=imm:zz", "08810010 12 3 reg:1=arena:0x3", "0881001G 12 3", "08810010 x 3", "08810010 12 3 reg:1=imm:0x1FFFFFFFF",
])
def test_invalid_recipes_are_rejected(line):
    assert not sc.valid_recipe(line)


def test_address_of_reads_the_first_field():
    assert sc.address_of(GOOD) == 0x08810010


def test_from_log_takes_only_valid_corpus_lines():
    log = f"noise\n[corpus] {GOOD}\n[corpus] garbage line\n[decomp] other\n[corpus] 08810020 1 1\n"
    assert sc.from_log(log) == [GOOD, "08810020 1 1"]


def test_merge_dedupes_keeps_order_and_caps_per_function():
    old = ["08810010 1 1", "08810010 2 1"]
    new = ["08810010 2 1", "08810010 3 1", "08810010 4 1", "08810020 1 1"]
    merged = sc.merge(old, new, cap=3)
    assert [l for l in merged if l.startswith("08810010")] == ["08810010 1 1", "08810010 2 1", "08810010 3 1"]
    assert "08810020 1 1" in merged


def test_write_and_read_round_trip(tmp_path):
    lines = [GOOD, "08810020 1 1"]
    sc.write_corpus(tmp_path, lines)
    assert (tmp_path / "08810010.txt").read_text().startswith(f"# layout {sc.LAYOUT_VERSION}\n")
    assert sorted(sc.read_corpus(tmp_path)) == sorted(lines)


def test_read_skips_other_layouts_and_bad_lines(tmp_path):
    (tmp_path / "08810010.txt").write_text(f"# layout {sc.LAYOUT_VERSION + 1}\n{GOOD}\n")
    (tmp_path / "08810020.txt").write_text(f"# layout {sc.LAYOUT_VERSION}\n08810020 1 1\nnot a recipe\n")
    assert sc.read_corpus(tmp_path) == ["08810020 1 1"]


def test_read_of_a_missing_directory_is_empty(tmp_path):
    assert sc.read_corpus(tmp_path / "nope") == []


def test_main_merges_logs_into_the_directory(tmp_path):
    log = tmp_path / "run.log"
    log.write_text(f"[corpus] {GOOD}\n")
    out = tmp_path / "corpus"
    assert sc.main(["merge", str(out), str(log)]) == 0
    assert sc.read_corpus(out) == [GOOD]
    assert sc.main(["merge", str(out), str(log)]) == 0  # idempotent
    assert sc.read_corpus(out) == [GOOD]
