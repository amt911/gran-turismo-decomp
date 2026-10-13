from pathlib import Path

import pytest

from tools.progress import LOAD_BASE, decompiled_functions, load_functions, report

FUNCTIONS_TSV = "28\t132\nac\t96\n331f60\t120\n332018\t344\n400000\t1000\n"


def write_src(root: Path, relative: str, text: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def test_load_functions_reads_hex_entries_and_decimal_sizes(tmp_path: Path):
    tsv = tmp_path / "functions.tsv"
    tsv.write_text(FUNCTIONS_TSV)
    assert load_functions(tsv) == {0x28: 132, 0xAC: 96, 0x331F60: 120, 0x332018: 344, 0x400000: 1000}


def test_decompiled_functions_finds_registrations_and_classifies_by_directory(tmp_path: Path):
    write_src(tmp_path, "libc/memset.cpp", "GT_DECOMP(0x08B36018u, gt::libc::memset, gt::ReturnKind::Word);\n")
    write_src(tmp_path, "libc/strlen.cpp", "GT_DECOMP(0x08B35F60u, gt::libc::strlen, gt::ReturnKind::Word);\n")
    write_src(tmp_path, "game/race.cpp", "GT_DECOMP(0x08804028u, gt::race::tick, gt::ReturnKind::None);\n")
    found = decompiled_functions(tmp_path)
    assert found == {0x332018: "sdk", 0x331F60: "sdk", 0x28: "game"}


def test_report_counts_functions_and_bytes_per_kind(tmp_path: Path):
    functions = {0x28: 132, 0xAC: 96, 0x331F60: 120, 0x332018: 344, 0x400000: 1000}
    lines = report(functions, {0x332018: "sdk", 0x331F60: "sdk", 0x28: "game"})
    text = "\n".join(lines)
    assert "functions: 3 / 5 decompiled" in text
    assert "bytes: 596 / 1692" in text
    assert "game: 1 functions, 132 bytes" in text
    assert "sdk: 2 functions, 464 bytes" in text


def test_report_flags_a_registration_that_is_not_a_function_entry():
    lines = report({0x28: 132}, {0x30: "game"})
    assert any("not a known function entry" in line and "0x08804030" in line for line in lines)


def test_load_base_matches_the_runtime_base():
    assert LOAD_BASE == 0x08804000


def test_decompiled_functions_rejects_an_address_below_the_load_base(tmp_path: Path):
    write_src(tmp_path, "libc/bad.cpp", "GT_DECOMP(0x00001000u, gt::libc::bad, gt::ReturnKind::Word);\n")
    with pytest.raises(ValueError):
        decompiled_functions(tmp_path)


def test_decompiled_functions_skips_a_commented_out_registration(tmp_path: Path):
    write_src(
        tmp_path,
        "game/race.cpp",
        "GT_DECOMP(0x08804028u, gt::race::tick, gt::ReturnKind::Void);\n"
        "// Not comparable yet (reads files inside a case), so not swapped in: "
        "GT_DECOMP(0x088040ACu, gt::race::load, gt::ReturnKind::Void);\n",
    )
    assert decompiled_functions(tmp_path) == {0x28: "game"}


def test_decompiled_functions_counts_template_registrations(tmp_path: Path):
    write_src(
        tmp_path,
        "game/as_class.cpp",
        "GT_DECOMP_AS_CLASS(0x08804028u, 0x08A76AB4u, 0x08804100u);\n"
        "// GT_DECOMP_AS_CLASS(0x088040ACu, 0x08A76AB4u, 0x08804100u);\n",
    )
    assert decompiled_functions(tmp_path) == {0x28: "game"}


def test_decompiled_functions_counts_single_argument_template_registrations(tmp_path: Path):
    write_src(tmp_path, "core/trivial.cpp", "GT_DECOMP_EMPTY(0x08804028u);\nGT_DECOMP_ZERO(0x088040ACu);\n")
    assert decompiled_functions(tmp_path) == {0x28: "game", 0xAC: "game"}


def test_decompiled_functions_counts_template_names_with_digits(tmp_path: Path):
    write_src(tmp_path, "core/leaves.cpp", "GT_DECOMP_RETURN_ARG0(0x08804028u);\n")
    assert decompiled_functions(tmp_path) == {0x28: "game"}


def test_decompiled_functions_counts_a_subsystems_shorthand_registrations(tmp_path: Path):
    # src/game/race/race_operator.cpp: #define RACE_OPERATOR(address, name, kind) GT_DECOMP(address, ...)
    write_src(
        tmp_path,
        "game/race/race_operator.cpp",
        "#define RACE_OPERATOR(address, name, kind) GT_DECOMP(address, gt::game::race_operator::name, kind)\n"
        "RACE_OPERATOR(0x08804028u, window_max, Word);\n"
        "// RACE_OPERATOR(0x088040ACu, set_window_max, Void);\n"
        "    GT_FIELD(guest::be16, race_type, 0x04);\n",
    )
    assert set(decompiled_functions(tmp_path)) == {0x08804028 - LOAD_BASE}
