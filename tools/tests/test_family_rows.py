"""tools.family_rows: registration rows of a macro family, mapped from the template's own row."""

import pytest

from tools import family_rows as fr


def test_each_argument_comes_from_the_value_the_template_has_at_that_place():
    # GT_DECOMP_FORWARD(entry, offset, target): the words give (target, offset); the row must keep the macro's order.
    rows, problems = fr.macro_rows("GT_DECOMP_FORWARD(0x0885E314u, 0, 0x088669B4u);", [0x088669B4, 0],
                                   {0x0885BBE8: [0x0885E414, 4]})
    assert rows == ["GT_DECOMP_FORWARD(0x0885BBE8u, 4, 0x0885E414u);"] and problems == []


def test_an_argument_no_value_explains_is_a_constant_of_the_family():
    rows, _ = fr.macro_rows("GT_DECOMP_AS_CLASS(0x088088C8u, 0x08A76AB4u, 0x0880881Cu);", [0x0880881C],
                            {0x08900000: [0x08901234]})
    assert rows == ["GT_DECOMP_AS_CLASS(0x08900000u, 0x08A76AB4u, 0x08901234u);"]


def test_a_value_that_sits_at_two_places_of_the_template_is_ambiguous():
    rows, problems = fr.macro_rows("M(0x08800000u, 0x10u);", [0x10, 0x10], {0x08800100: [0x10, 0x20]})
    assert rows == [] and "ambiguous" in problems[0]


def test_a_member_with_other_values_than_the_template_is_refused():
    rows, problems = fr.macro_rows("M(0x08800000u, 0x10u);", [0x10, 0x30], {0x08800100: [0x10]})
    assert rows == [] and "0x08800100" in problems[0]


def test_literals_keep_their_form():
    rows, _ = fr.macro_rows("GT_DECOMP_STRIDE_WALK(0x08B891ACu, 0x20u);", [0x20], {0x08B89000: [0x40]})
    assert rows == ["GT_DECOMP_STRIDE_WALK(0x08B89000u, 0x40u);"]
    rows, _ = fr.macro_rows("GT_DECOMP_SET_FIELD(0x0889867Cu, 60);", [60], {0x08898000: [12]})
    assert rows == ["GT_DECOMP_SET_FIELD(0x08898000u, 12);"]


def test_the_template_row_is_found_in_a_source_by_its_entry():
    source = "// GT_DECOMP_EMPTY(0x08805B88u);\nGT_DECOMP_GETTER(0x08808518u, 0x08CE28FCu);\nx\n"
    assert fr.template_row(source, 0x08808518) == "GT_DECOMP_GETTER(0x08808518u, 0x08CE28FCu);"
    assert fr.template_row(source, 0x08805B88) is None  # commented out
    with pytest.raises(ValueError):
        fr.template_row("A(0x08808518u);\nB(0x08808518u);\n", 0x08808518)
