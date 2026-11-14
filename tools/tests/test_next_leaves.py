import struct
from pathlib import Path

import pytest

from tools import next_leaves
from tools.next_leaves import Leaf

JR_RA = 0x03E00008
NOP = 0x00000000
JAL = 0x0C000100
J = 0x08000100
JALR_V0 = 0x0040F809       # jalr v0 (rd = ra)
JR_V0 = 0x00400008         # jr v0 (a jump table or a tail call)
LW = 0x8C820008            # lw v0, 8(a0)
SW = 0xAC820008            # sw v0, 8(a0)
BEQ = 0x10400002           # beq v0, zero, +2
ADDIU = 0x24420001         # addiu v0, v0, 1


def pack(*words: int) -> bytes:
    return struct.pack(f"<{len(words)}I", *words)


def test_a_function_without_calls_or_far_jumps_is_a_leaf():
    assert next_leaves.is_leaf([LW, ADDIU, SW, JR_RA, NOP])


@pytest.mark.parametrize("call", [JAL, JALR_V0, J, JR_V0])
def test_calls_tail_calls_and_indirect_jumps_make_a_function_not_a_leaf(call):
    assert not next_leaves.is_leaf([LW, call, NOP, JR_RA, NOP], entry=0x8000)


def test_a_j_inside_the_function_is_control_flow_not_a_call():
    inside = 0x08000000 | (0x8010 >> 2)           # j 0x8010, inside the 5 words at 0x8000
    assert next_leaves.is_leaf([LW, inside, NOP, JR_RA, NOP], entry=0x8000)
    assert next_leaves.direct_calls([LW, inside, NOP, JR_RA, NOP], entry=0x8000) == ()


def test_summary_counts_memory_accesses_and_branches():
    leaf = next_leaves.summarize(0x100, [LW, BEQ, NOP, SW, ADDIU, JR_RA, NOP])
    assert leaf == Leaf(entry=0x100, insns=7, loads=1, stores=1, branches=1)


def test_taken_addresses_cover_registrations_and_definitions_but_not_mentions():
    source = "\n".join([
        "GT_DECOMP(0x08804028u, gt::a, gt::ReturnKind::Word);",
        "HOT218(0x08A36364u, func_08A36364, Word);",
        "// HOT218(0x089F9234u, func_089F9234, Word);  // no comparable synthetic case yet",
        "// the text mentions 0x08804999 and 0x08804AAAu without registering it",
        "    const u32 x = 0x08804BBBu;",
        "void func_08A00000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { }",
        "[[maybe_unused]] void func_08A00100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {",
        "    func_08A00200(rt, ctx);",
    ])
    assert next_leaves.taken_addresses(source) == {0x08804028, 0x08A36364, 0x089F9234, 0x08A00000, 0x08A00100}


def test_candidates_keep_emitted_untaken_leaves_in_size_bounds_and_order_by_size():
    code = {
        0x100: pack(LW, JR_RA, NOP),                    # leaf, 3 insns
        0x200: pack(LW, ADDIU, SW, JR_RA, NOP),         # leaf, 5 insns
        0x300: pack(LW, JAL, NOP, JR_RA, NOP),          # calls
        0x400: pack(LW, SW, JR_RA, NOP),                # taken
        0x500: pack(LW, SW, ADDIU, JR_RA, NOP),         # never emitted
        0x600: pack(*([ADDIU] * 30), JR_RA, NOP),       # too large
        0x700: pack(JR_RA, NOP),                        # too small
    }
    found = next_leaves.candidates(code, emitted={0x100, 0x200, 0x300, 0x400, 0x600, 0x700}, taken={0x400},
                                   min_insns=3, max_insns=20)
    assert [leaf.entry for leaf in found] == [0x100, 0x200]


def test_candidates_skip_the_addresses_other_agents_hold():
    code = {0x100: pack(LW, JR_RA, NOP), 0x200: pack(LW, SW, JR_RA, NOP)}
    found = next_leaves.candidates(code, emitted={0x100, 0x200}, taken=set(), skip={0x100}, min_insns=3, max_insns=20)
    assert [leaf.entry for leaf in found] == [0x200]


def test_read_skip_file_ignores_blank_lines_and_comments(tmp_path: Path):
    skip = tmp_path / "skip.txt"
    skip.write_text("0x08804028\n\n# another agent\n08804100  trailing words\n")
    assert next_leaves.read_skip_file(skip) == {0x08804028 - 0x08804000, 0x08804100 - 0x08804000}


def test_format_row_prints_the_runtime_address_and_the_shape_counts():
    row = next_leaves.format_row(Leaf(entry=0x28, insns=9, loads=2, stores=1, branches=3))
    assert row == "0x08804028  9 insns  2 loads  1 stores  3 branches"


JAL_0x1000 = 0x0C000400    # jal 0x1000 (an ELF vaddr: the target field is not relocated)
JAL_0x2000 = 0x0C000800


def test_direct_calls_lists_the_elf_vaddr_of_each_jal_once_in_order():
    assert next_leaves.direct_calls([LW, JAL_0x2000, NOP, JAL_0x1000, NOP, JAL_0x2000, NOP, JR_RA, NOP]) == (0x2000, 0x1000)


def test_has_indirect_jump_flags_jalr_and_jr_through_another_register_but_not_direct_calls_or_j():
    assert not next_leaves.has_indirect_jump([JAL_0x1000, NOP, J, NOP, JR_RA, NOP])
    for word in (JALR_V0, JR_V0):
        assert next_leaves.has_indirect_jump([word, NOP])


def test_a_tail_call_is_a_callee():
    assert next_leaves.direct_calls([LW, J, NOP], entry=0x8000) == (0x400,)


def test_ready_functions_call_only_functions_that_are_taken():
    code = {
        0x100: pack(LW, JR_RA, NOP),                              # leaf
        0x200: pack(LW, JAL_0x1000, NOP, JR_RA, NOP),             # calls 0x1000
        0x300: pack(LW, JAL_0x2000, NOP, JR_RA, NOP),             # calls 0x2000
        0x400: pack(LW, JALR_V0, NOP, JR_RA, NOP),                # indirect
        0x500: pack(JAL_0x1000, NOP, JAL_0x2000, NOP, JR_RA, NOP),  # one callee taken, one not
    }
    found = next_leaves.candidates(code, emitted=set(code), taken={0x1000}, min_insns=3, max_insns=20, ready=True)
    assert [leaf.entry for leaf in found] == [0x100, 0x200]
    assert next_leaves.candidates(code, emitted=set(code), taken={0x1000}, min_insns=3, max_insns=20) == [
        next_leaves.summarize(0x100, [LW, JR_RA, NOP])]


def test_format_row_names_the_callees_of_a_ready_function():
    row = next_leaves.format_row(Leaf(entry=0x28, insns=9, loads=2, stores=1, branches=3, calls=(0x1000,)))
    assert row.endswith("calls 0x08805000")
