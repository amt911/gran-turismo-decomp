"""tools.arg_reads: the argument registers a callee reads before writing, and the calls that pass fewer."""

from tools import arg_reads

CALLEE = [
    "  08000000: 27BDFFE0  addiu       $sp, $sp, -0x20",
    "  08000004: 00A02021  addu        $a0, $a1, $zero",  # reads a1, writes a0
    "  08000008: AFA60010  sw          $a2, 0x10($sp)",  # a store reads its register
    "  0800000C: 8C870004  lw          $a3, 0x4($a0)",  # a0 was written above, a3 only written
    "  08000010: 00E04021  addu        $t0, $a3, $zero",  # t0 written
    "  08000014: 01004821  addu        $t1, $t0, $zero",
    "  08000018: 0120502B  sltu        $t2, $t1, $t1",
    "  0800001C: 03E00008  jr          $ra",
]


def test_registers_read_before_they_are_written_are_the_arguments():
    assert arg_reads.arg_reads(CALLEE) == {5, 6}  # a1 and a2


def test_a_conditional_move_reads_its_destination():
    assert arg_reads.arg_reads(["  08000000: 0043300A  movz        $a2, $v0, $v1"]) == {6}


def test_a_register_written_by_a_load_is_not_an_argument_afterwards():
    lines = ["  08000000: 8CE40000  lw          $a0, 0x0($a3)", "  08000004: 00801021  addu        $v0, $a0, $zero"]
    assert arg_reads.arg_reads(lines) == {7}


SOURCE = """
void func_08000010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const std::uint32_t r = call_set(rt, ctx, 0x08B36170u, {first, word(rt, x + 4u), shorter});
    call_set(rt, ctx, slot_of(rt, o, 0xCu), {o});
    (void)call_set(rt, ctx, 0x08C34990u, {tree, 0u, parent, value});
    (void)call_set(rt, ctx, 0x08A76AA0u, {});
}
"""


def test_calls_with_a_literal_address_are_counted_by_their_top_level_arguments():
    calls = arg_reads.literal_calls(SOURCE)
    assert [(address, passed) for address, passed, _line in calls] == [(0x08A5B9EC, 1), (0x08B36170, 3), (0x08C34990, 4), (0x08A76AA0, 0)]


def test_missing_arguments_are_the_read_registers_beyond_the_passed_ones():
    assert arg_reads.missing({5, 6, 7}, 2) == [6, 7]  # a2 and a3 are not passed
    assert arg_reads.missing({4, 5}, 2) == []
    assert arg_reads.missing({8}, 4) == [8]  # t0 is the fifth argument register
    assert arg_reads.missing({8}, 5) == []


FORWARDER = [
    "  08000000: 0810E93A  j           func_43A4E8  -> func_08C3E4E8",
    "  08000004: 00000000  nop",
]


def test_a_short_callee_that_ends_in_a_jump_is_a_forwarder_to_its_target():
    assert arg_reads.forwarder_target(FORWARDER) == 0x08C3E4E8
    assert arg_reads.forwarder_target(CALLEE) is None  # ends in jr, not j


def test_a_long_callee_with_a_jump_inside_is_not_a_forwarder():
    long_body = ["  08000000: 08100000  j           func_400000  -> func_08400000"] + ["  08000004: 00000000  nop"] * 12
    assert arg_reads.forwarder_target(long_body) is None


def test_a_forwarder_also_reads_what_its_target_reads_except_what_it_writes_first():
    # the forwarder moves a0 into a1 (writes a1, reads a0); the target reads a0, a1, a2
    forwarder = [
        "  08000000: 00802821  addu        $a1, $a0, $zero",
        "  08000004: 0810E93A  j           func_43A4E8  -> func_08C3E4E8",
    ]
    assert arg_reads.with_forwarded(forwarder, {4, 5, 6}) == {4, 6}  # a1 is written by the forwarder, a2 is the hidden third argument
    assert arg_reads.with_forwarded(FORWARDER, {4, 5, 6}) == {4, 5, 6}
