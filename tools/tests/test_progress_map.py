from pathlib import Path

import pytest

from tools.progress_map import (
    Function,
    bars,
    classify,
    group_functions,
    main,
    render_html,
    render_svg,
    replace_between_markers,
    squarify,
)


def area(rect):
    return rect[2] * rect[3]


def test_squarify_fills_the_box_with_areas_proportional_to_the_sizes():
    sizes = [60, 30, 10]
    rects = squarify(sizes, 0, 0, 10, 10)
    assert len(rects) == 3
    assert sum(area(r) for r in rects) == pytest.approx(100)
    for size, rect in zip(sizes, rects):
        assert area(rect) == pytest.approx(size)
        x, y, w, h = rect
        assert 0 <= x and 0 <= y and x + w <= 10 + 1e-9 and y + h <= 10 + 1e-9


def test_squarify_keeps_rectangles_close_to_square():
    rects = squarify([1] * 16, 0, 0, 4, 4)
    for _, _, w, h in rects:
        assert max(w / h, h / w) < 2.5


def test_classify_tells_hand_written_from_template_registrations(tmp_path: Path):
    (tmp_path / "game").mkdir()
    (tmp_path / "game" / "a.cpp").write_text(
        "GT_DECOMP(0x08804028u, gt::f, gt::ReturnKind::Word);\n"
        "GT_DECOMP_THUNK(0x088040ACu, 0x08804028u);\n"
        "// GT_DECOMP(0x08804100u, gt::g, gt::ReturnKind::Word);\n"
    )
    assert classify(tmp_path) == {0x28: "hand", 0xAC: "template"}


def test_group_functions_splits_game_code_into_address_regions_and_keeps_the_sdk_apart():
    functions = [Function(0x28, 0x10, "hand", None), Function(0x40028, 0x10, None, None),
                 Function(0x330000, 0x10, None, None)]
    groups = group_functions(functions, region=0x40000, sdk_start=0x330000)
    assert [g.label for g in groups] == ["0x08804000", "0x08844000", "SDK"]
    assert [len(g.functions) for g in groups] == [1, 1, 1]


def test_bars_report_functions_bytes_and_names():
    functions = [Function(0x28, 30, "hand", "f"), Function(0x48, 10, "template", None),
                 Function(0x58, 60, None, "g")]
    text = bars(functions)
    assert "FUNCIONES" in text and "2 / 3" in text and "66.67 %" in text
    assert "BYTES" in text and "40 / 100" in text and "40.00 %" in text
    assert "CON NOMBRE" in text and "2 / 3" in text


def test_replace_between_markers_rewrites_only_the_marked_block():
    readme = "a\n<!-- progress:start -->\nold\n<!-- progress:end -->\nb\n"
    assert replace_between_markers(readme, "new") == "a\n<!-- progress:start -->\nnew\n<!-- progress:end -->\nb\n"
    with pytest.raises(ValueError):
        replace_between_markers("no markers", "new")


def test_render_svg_draws_one_rectangle_per_function():
    functions = [Function(0x28, 30, "hand", "f"), Function(0x48, 10, None, None)]
    groups = group_functions(functions, region=0x40000, sdk_start=0x330000)
    svg = render_svg(groups, width=400, height=300)
    assert svg.startswith("<svg") and svg.count('class="f"') == 2


def test_render_html_embeds_every_function_for_the_hover_and_can_skip_the_document_wrapper():
    functions = [Function(0x28, 30, "hand", "memcpy"), Function(0x48, 10, None, None)]
    groups = group_functions(functions, region=0x40000, sdk_start=0x330000)
    page = render_html(groups, document=True)
    assert page.startswith("<!doctype html>") and "<title>" in page and "memcpy" in page
    fragment = render_html(groups, document=False)
    assert not fragment.startswith("<!doctype") and fragment.lstrip().startswith("<title>")


def test_main_writes_the_map_the_page_and_the_readme_bars(tmp_path: Path):
    functions = tmp_path / "functions.tsv"
    functions.write_text("28\t132\nac\t96\n")
    src = tmp_path / "src" / "game"
    src.mkdir(parents=True)
    (src / "a.cpp").write_text("GT_DECOMP(0x08804028u, gt::f, gt::ReturnKind::Word);\n")
    symbols = tmp_path / "symbols.txt"
    symbols.write_text("gt::f = 0x08804028; // hand\n")
    readme = tmp_path / "README.md"
    readme.write_text("x\n<!-- progress:start -->\n<!-- progress:end -->\n")
    out = tmp_path / "out"
    assert main(["--functions", str(functions), "--src", str(tmp_path / "src"), "--symbols", str(symbols),
                 "--out", str(out), "--readme", str(readme)]) == 0
    assert (out / "progress_map.svg").read_text().count('class="f"') == 2
    assert "gt::f" in (out / "progress" / "index.html").read_text()
    assert "1 / 2" in readme.read_text()
