"""Tests for tools/gate_coverage.py: choosing the files to instrument and reading llvm-cov's export."""

from __future__ import annotations

import re
from pathlib import Path

from tools import gate_coverage as cov

REGISTRATION = re.compile(r"^[ \t]*[A-Z][A-Z0-9_]*\(\s*0x([0-9A-Fa-f]{8})u?\s*,", re.MULTILINE)


def region(line, col, count, kind=0):
    return [line, col, line, col + 5, count, 0, 0, kind]


def branch(line, col, taken, not_taken):
    return [line, col, line, col + 4, taken, not_taken, 0, 0, 4]


def function(name, branches=(), regions=(), file="src/game/x.cpp"):
    return {"name": name, "filenames": [file], "branches": list(branches), "regions": list(regions), "count": 1}


def export(*functions):
    return {"data": [{"functions": list(functions)}]}


def test_source_files_returns_the_files_registering_a_wanted_address(tmp_path: Path):
    (tmp_path / "a.cpp").write_text("HOT(0x08000000u, f, Word);\nHOT(0x08000004u, g, Word);\n")
    (tmp_path / "b.cpp").write_text("HOT(0x08000008u, h, Word);\n")
    (tmp_path / "c.cpp").write_text("// nothing\n")
    assert cov.source_files([0x08000004], tmp_path, REGISTRATION) == [tmp_path / "a.cpp"]
    assert cov.source_files([0x08000004, 0x08000008], tmp_path, REGISTRATION) == [tmp_path / "a.cpp", tmp_path / "b.cpp"]
    assert cov.source_files([0x09000000], tmp_path, REGISTRATION) == []


def test_cmake_argument_is_relative_to_the_root_and_semicolon_separated(tmp_path: Path):
    files = [tmp_path / "src" / "a.cpp", tmp_path / "src" / "game" / "b.cpp"]
    assert cov.cmake_files_argument(files, tmp_path) == "-DGT_COVERAGE_FILES=src/a.cpp;src/game/b.cpp"


def test_function_label_finds_the_address_in_a_mangled_or_plain_name():
    name, address = cov.function_label("gt::game::hot_batch_61::func_08B54760(psprecomp::Runtime&, psprecomp::AllegrexContext&)")
    assert name == "gt::game::hot_batch_61::func_08B54760" and address == 0x08B54760
    name, address = cov.function_label("gt::game::hot_batch_61::tree_find(psprecomp::Runtime&)")
    assert name.endswith("tree_find") and address is None


def test_a_branch_outcome_with_a_zero_count_is_untaken():
    doc = export(function("ns::func_08000000(int)", branches=[branch(10, 8, 5, 0), branch(12, 8, 0, 7)]))
    [f] = cov.uncovered(doc, ["src/game/x.cpp"])
    assert f.address == 0x08000000 and f.branches == 2
    assert f.untaken == [(10, 8, "false"), (12, 8, "true")]
    assert not f.complete


def test_a_region_that_never_ran_is_reported_and_other_region_kinds_are_ignored():
    doc = export(function("ns::func_08000000(int)", regions=[region(5, 1, 3), region(9, 4, 0), region(9, 9, 0, kind=4)]))
    [f] = cov.uncovered(doc, ["src/game/x.cpp"])
    assert f.unexecuted == [(9, 4)] and f.untaken == []


def test_a_fully_covered_function_is_complete_and_formats_as_such():
    doc = export(function("ns::func_08000000(int)", branches=[branch(3, 3, 1, 1)], regions=[region(2, 1, 1)]))
    [f] = cov.uncovered(doc, ["src/game/x.cpp"])
    assert f.complete
    assert cov.format_report([f]) == "0x08000000 ns::func_08000000: all 1 branches taken, every region ran"


def test_copies_of_a_function_are_merged_so_an_outcome_taken_anywhere_counts_as_taken():
    first = function("ns::shared(int)", branches=[branch(4, 4, 1, 0)])
    second = function("ns::shared(int)", branches=[branch(4, 4, 0, 3)])
    [f] = cov.uncovered(export(first, second), ["src/game/x.cpp"])
    assert f.untaken == [] and f.branches == 1


def test_functions_of_other_files_are_left_out():
    doc = export(function("ns::func_08000000(int)", branches=[branch(1, 1, 0, 0)], file="host/other.cpp"))
    assert cov.uncovered(doc, ["src/game/x.cpp"]) == []


def test_format_lists_untaken_outcomes_and_unrun_regions_by_position():
    doc = export(function("ns::func_08000010(int)", branches=[branch(10, 8, 5, 0)], regions=[region(11, 2, 0)]))
    text = cov.format_report(cov.uncovered(doc, ["src/game/x.cpp"]))
    assert text == "0x08000010 ns::func_08000010: branches never false at 10:8; regions never run at 11:2"


def test_demangle_names_drops_the_file_prefix_and_replaces_each_name():
    doc = export(function("hot.cpp:_ZN2ns4funcEv"), function("_ZN2ns5otherEv"))
    seen = []
    cov.demangle_names(doc, lambda names: seen.append(names) or [n.replace("_ZN2ns4funcEv", "ns::func()").replace("_ZN2ns5otherEv", "ns::other()") for n in names])
    assert seen == [["_ZN2ns4funcEv", "_ZN2ns5otherEv"]]
    assert [f["name"] for f in doc["data"][0]["functions"]] == ["ns::func()", "ns::other()"]
