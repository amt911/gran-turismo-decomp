"""tools.literal_check: a literal typed wrongly is found, the lui / ori pair and the call targets are known."""

from tools import literal_check

SOURCE = """\
// comment 0xDEAD0000 is ignored
void func_08000010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call(0x08C49F24u, 0x01111111u, 0x38u);  // 0x99999999 in a comment
}

[[maybe_unused]] void func_08000040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    call(0x11111111u);
}
"""

LINES = {
    0x08000010: [
        "  08000010: 3C020111  lui         $v0, 0x111",
        "  08000014: 34421111  ori         $v0, $v0, 0x1111",
        "  08000018: 0C000000  jal         func_000000  -> func_08C49F24",
        "  0800001C: 24050038  addiu       $a1, $zero, 0x38",
    ],
    0x08000040: [
        "  08000040: 3C020111  lui         $v0, 0x111",
        "  08000044: 34421111  ori         $v0, $v0, 0x1111",
    ],
}


def test_function_literals_strip_comments_and_small_values():
    found = literal_check.function_literals(SOURCE)
    assert found[0x08000010] == {0x08C49F24, 0x01111111, 0x38}
    assert found[0x08000040] == {0x11111111}


def test_the_typed_digit_batch_90_got_wrong_is_reported():
    missing = literal_check.check(SOURCE, lambda address: LINES[address])
    assert missing == [(0x08000040, 0x11111111)]


def test_negative_immediates_and_allow_list():
    assert literal_check.disassembly_values(["  0: 0  addiu       $a1, $a1, -0x4"]) >= {4, 0xFFFFFFFC}
    assert literal_check.disassembly_values(["  0: 0  addiu       $a1, $zero, -4"]) >= {0xFFFFFFFC}
    allowed = literal_check.check(SOURCE, lambda address: LINES[address], {0x08000040: {0x11111111}})
    assert allowed == []


def test_a_one_line_wrapper_does_not_swallow_the_next_function():
    source = "void func_08000010(a &rt, b &ctx) { wrapper<1u>(rt, ctx, 0x08C49F24u); }\nvoid func_08000040(a &rt, b &ctx) { wrapper<1u>(rt, ctx, 0x08C49F74u); }\n"
    assert literal_check.function_literals(source) == {0x08000010: {0x08C49F24}, 0x08000040: {0x08C49F74}}


def test_a_bare_lui_is_a_float_constant():
    assert 0x3F800000 in literal_check.pair_values(["  08000010: 3C013F80  lui         $at, 0x3F80"])
