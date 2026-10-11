from pathlib import Path

from tools.progress_map import cell_coverage, colour, main, render_svg


def test_cell_coverage_is_the_decompiled_share_of_the_function_bytes_in_each_cell():
    # cell 0: a 0x40-byte function, decompiled; cell 1: 0x20 decompiled out of 0x80; cell 2: nothing.
    functions = {0x000: 0x40, 0x100: 0x20, 0x120: 0x60}
    cells = cell_coverage(functions, {0x000, 0x100}, start=0x0, end=0x300, cell_size=0x100)
    assert cells == [1.0, 0.25, None]


def test_cell_coverage_splits_a_function_that_crosses_a_cell_boundary():
    cells = cell_coverage({0xC0: 0x80}, {0xC0}, start=0x0, end=0x200, cell_size=0x100)
    assert cells == [1.0, 1.0]
    cells = cell_coverage({0xC0: 0x80}, set(), start=0x0, end=0x200, cell_size=0x100)
    assert cells == [0.0, 0.0]


def test_colour_goes_from_grey_to_full_colour_and_marks_empty_cells():
    assert colour(None, sdk=False) == "#f2f2f2"
    assert colour(0.0, sdk=False) == "#d0d0d0"
    assert colour(1.0, sdk=False) == "#2e9e44"
    assert colour(1.0, sdk=True) == "#2f6db5"
    assert colour(0.5, sdk=False) not in {"#d0d0d0", "#2e9e44"}


def test_render_svg_draws_one_rect_per_cell_and_the_legend():
    svg = render_svg([1.0, 0.0, None, 0.5], columns=2, sdk_from=2, summary="2 / 4 functions")
    assert svg.startswith("<svg")
    assert svg.count('class="cell"') == 4
    assert "2 / 4 functions" in svg
    assert "#2e9e44" in svg and "#d0d0d0" in svg


def test_main_writes_the_map_from_the_function_list_and_the_registrations(tmp_path: Path):
    functions = tmp_path / "functions.tsv"
    functions.write_text("28\t132\nac\t96\n")
    src = tmp_path / "src" / "game"
    src.mkdir(parents=True)
    (src / "a.cpp").write_text("GT_DECOMP(0x08804028u, gt::f, gt::ReturnKind::Word);\n")
    out = tmp_path / "map.svg"
    assert main(["--functions", str(functions), "--src", str(tmp_path / "src"), "--out", str(out)]) == 0
    text = out.read_text()
    assert "1 / 2" in text and text.count('class="cell"') > 0
