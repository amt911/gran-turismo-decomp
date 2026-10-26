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


def test_a_value_at_two_places_of_the_template_is_carried_by_the_one_members_change():
    rows, problems = fr.macro_rows("M(0x08800000u, 0x10u);", [0x10, 0x10], {0x08800100: [0x10, 0x20]})
    assert rows == ["M(0x08800100u, 0x20u);"] and problems == []


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


def test_a_wrapper_row_is_mapped_like_a_macro_row_and_gets_its_registration():
    head = "void func_08970758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) "
    template = head + "{ visit_property(rt, ctx, 0x08CCDA08u, 180u); }"
    registration = "GT_DECOMP(0x08970758u, gt::game::func_08970758, gt::ReturnKind::Void);"
    rows, regs, problems = fr.wrapper_rows(template, registration, [180, 0x08CCDA08], {0x089707F8: [172, 0x08CCDA08]})
    assert problems == []
    assert rows == ["void func_089707F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) "
                    "{ visit_property(rt, ctx, 0x08CCDA08u, 172u); }"]
    assert regs == ["GT_DECOMP(0x089707F8u, gt::game::func_089707F8, gt::ReturnKind::Void);"]


def test_a_wrapper_template_and_its_registration_are_found_in_a_source():
    source = ("void func_08970758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { visit_property(rt, ctx, 1u); }\n"
              "HOT9(0x08970758u, func_08970758, Void);\n")
    row, registration = fr.wrapper_template(source, 0x08970758)
    assert row.startswith("void func_08970758(") and registration == "HOT9(0x08970758u, func_08970758, Void);"


def test_a_wrapper_row_wins_over_its_own_registration_read_as_a_macro():
    """GT_DECOMP(0xADDRu, gt::game::func_ADDR, ...) looks like a macro row; taken as one, every member pointed at the
    template's function (2026-10-25)."""
    source = ("void func_08970758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { visit_property(rt, ctx, 1u); }\n"
              "GT_DECOMP(0x08970758u, gt::game::func_08970758, gt::ReturnKind::Void);\n")
    kind, found = fr.pick_template(source, 0x08970758)
    assert kind == "wrapper" and found[1].startswith("GT_DECOMP(")
    kind, found = fr.pick_template("GT_DECOMP_EMPTY(0x08805B88u);\n", 0x08805B88)
    assert kind == "macro" and found == "GT_DECOMP_EMPTY(0x08805B88u);"


def test_a_registration_that_names_a_function_is_not_a_family_macro():
    """HOT104(0x08BF3EA4u, func_08BF3EA4, Word) registers one C++ body with its own constants; copying it to the members
    would run the template's constants at the members' addresses."""
    rows, problems = fr.macro_rows("HOT104(0x08BF3EA4u, func_08BF3EA4, Word);", [1], {0x08B8C6BC: [2]})
    assert rows == [] and "names a function" in problems[0]
    rows, problems = fr.macro_rows("GT_DECOMP(0x08970758u, gt::game::func_08970758, gt::ReturnKind::Void);", [1],
                                   {0x089707F8: [2]})
    assert rows == [] and "names a function" in problems[0]


def test_a_named_argument_other_than_true_or_false_is_a_function_too():
    rows, problems = fr.macro_rows("RACE_OPERATOR(0x0885BC90u, set_with_ghost, Void);", [1], {0x0885C368: [2]})
    assert rows == [] and "names a function" in problems[0]
    rows, _ = fr.macro_rows("GT_DECOMP_CAST(0x0880881Cu, 0x08808524u, 0x08A76AA0u, false);", [0x08808524],
                            {0x08809D10: [0x08809B28]})
    assert rows == ["GT_DECOMP_CAST(0x08809D10u, 0x08809B28u, 0x08A76AA0u, false);"]


def test_a_member_whose_value_no_argument_carries_differs_is_refused():
    """A row passes only the template's arguments: a member that differs at a value position no argument uses would run
    with the template's constant there (ptmf_tail_arg / clamp_step take no argument, 2026-10-25)."""
    rows, problems = fr.macro_rows("M(0x08800000u, 0x10u);", [0x10, 0x99], {0x08800100: [0x20, 0x99], 0x08800200: [0x30, 0x77]})
    assert rows == ["M(0x08800100u, 0x20u);"] and "0x08800200" in problems[0] and "position 1" in problems[0]


def test_an_ambiguous_argument_takes_the_one_position_that_varies():
    rows, problems = fr.macro_rows("M(0x08800000u, 8u);", [8, 9, 8], {0x08800100: [8, 9, 0xC], 0x08800200: [8, 9, 0x10]})
    assert problems == [] and rows == ["M(0x08800100u, 12u);", "M(0x08800200u, 16u);"]


def test_an_ambiguous_argument_no_member_varies_at_is_the_same_everywhere():
    rows, problems = fr.macro_rows("M(0x08800000u, 8u);", [8, 8], {0x08800100: [8, 8]})
    assert problems == [] and rows == ["M(0x08800100u, 8u);"]


def test_an_argument_two_varying_positions_could_carry_is_still_refused():
    rows, problems = fr.macro_rows("M(0x08800000u, 8u);", [8, 8], {0x08800100: [4, 12]})
    assert rows == [] and "ambiguous" in problems[0]


def test_positions_that_always_agree_are_one_argument():
    """copy_range's stride is two addiu immediates (source and destination advance together)."""
    rows, problems = fr.macro_rows("M(0x08800000u, 4u);", [4, 4, 7], {0x08800100: [8, 8, 7], 0x08800200: [0xC, 0xC, 7]})
    assert problems == [] and rows == ["M(0x08800100u, 8u);", "M(0x08800200u, 12u);"]
    rows, problems = fr.macro_rows("M(0x08800000u, 4u);", [4, 4, 7], {0x08800100: [8, 0xC, 7]})
    assert rows == [] and "ambiguous" in problems[0]


def test_a_cast_literal_is_a_value_and_keeps_its_cast():
    # GT_DECOMP_DEREF_FLOAT(entry, offset, static_cast<std::uint32_t>(k)): the cast wraps a plain number.
    rows, problems = fr.macro_rows("GT_DECOMP_DEREF_FLOAT(0x088A42A8u, 484, static_cast<std::uint32_t>(1048));",
                                   [484, 1048], {0x0885F07C: [12, 16]})
    assert rows == ["GT_DECOMP_DEREF_FLOAT(0x0885F07Cu, 12, static_cast<std::uint32_t>(16));"] and problems == []


def test_a_negative_value_is_written_as_its_32_bit_word():
    # guarded_zero's third argument is a sign-extended 16-bit offset (0xFFFF933C): a member's -0x6C94 is 0xFFFF936C.
    rows, problems = fr.macro_rows("GT_DECOMP_X(0x08B9C1F8u, 0xFFFF933Cu);", [-0x6CC4], {0x08B9C130: [-0x6C94]})
    assert rows == ["GT_DECOMP_X(0x08B9C130u, 0xFFFF936Cu);"] and problems == []


def test_a_plain_decimal_argument_is_written_signed():
    # GT_DECOMP_CONST(entry, k) brace-initialises a std::int32_t: -1 must not come out as 4294967295 (narrowing).
    rows, problems = fr.macro_rows("GT_DECOMP_CONST(0x088666E8u, 1);", [1], {0x0886B2C4: [0xFFFFFFFF]})
    assert rows == ["GT_DECOMP_CONST(0x0886B2C4u, -1);"] and problems == []
