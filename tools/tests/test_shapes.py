import struct

import pytest

from tools import shapes


def words(*ws: int) -> bytes:
    return struct.pack(f"<{len(ws)}I", *ws)


LW_V0_8_A0 = 0x8C820008      # lw v0, 8(a0)
LW_V0_12_A0 = 0x8C82000C     # lw v0, 12(a0)
LW_V1_8_A0 = 0x8C830008      # lw v1, 8(a0)
JR_RA = 0x03E00008           # jr ra
NOP = 0x00000000
JAL_A = 0x0C000100           # jal 0x400
JAL_B = 0x0C000200           # jal 0x800
BEQ_FWD_2 = 0x10400002       # beq v0, zero, +2
BEQ_FWD_3 = 0x10400003       # beq v0, zero, +3


def test_normalize_masks_immediates_and_jump_targets_but_keeps_registers():
    assert shapes.normalize(words(LW_V0_8_A0)) == shapes.normalize(words(LW_V0_12_A0))
    assert shapes.normalize(words(LW_V0_8_A0)) != shapes.normalize(words(LW_V1_8_A0))
    assert shapes.normalize(words(JAL_A)) == shapes.normalize(words(JAL_B))


def test_normalize_keeps_branch_offsets_so_control_flow_must_match():
    assert shapes.normalize(words(BEQ_FWD_2)) != shapes.normalize(words(BEQ_FWD_3))


def test_clusters_group_functions_of_one_shape_largest_first():
    code = {0x100: words(LW_V0_8_A0, JR_RA, NOP), 0x200: words(LW_V0_12_A0, JR_RA, NOP),
            0x300: words(LW_V1_8_A0, JR_RA, NOP), 0x400: words(LW_V0_8_A0, JR_RA, NOP)}
    clusters = shapes.clusters(code)
    assert clusters[0] == [0x100, 0x200, 0x400]
    assert clusters[1] == [0x300]


def test_report_lists_pending_members_and_the_decompiled_siblings_whose_template_fits():
    clusters = [[0x100, 0x200, 0x400], [0x300], [0x500, 0x600]]
    rows = shapes.report(clusters, sizes={0x100: 12, 0x200: 12, 0x300: 12, 0x400: 12, 0x500: 8, 0x600: 8},
                         done={0x200: "gt::core::word_getter"}, min_pending=2)
    assert rows == [
        shapes.Row(pending=[0x100, 0x400], size=12, done={0x200: "gt::core::word_getter"}),
        shapes.Row(pending=[0x500, 0x600], size=8, done={}),
    ]


def test_text_section_of_a_minimal_elf():
    # ELF32 LE with a section table holding a null section, .text and .shstrtab
    shstr = b"\0.text\0.shstrtab\0"
    text = words(JR_RA, NOP)
    text_off = 52
    shstr_off = text_off + len(text)
    sh_off = shstr_off + len(shstr)
    header = (b"\x7fELF\x01\x01\x01" + b"\0" * 9 +
              struct.pack("<HHIIIIIHHHHHH", 0xFFA0, 8, 1, 0, 0, sh_off, 0, 52, 0, 0, 40, 3, 2))
    sections = (struct.pack("<10I", *[0] * 10) +
                struct.pack("<10I", 1, 1, 6, 0x28, text_off, len(text), 0, 0, 4, 0) +
                struct.pack("<10I", 7, 3, 0, 0, shstr_off, len(shstr), 0, 0, 1, 0))
    elf = header + text + shstr + sections
    assert shapes.text_section(elf) == (0x28, text)



@pytest.mark.parametrize("op", [0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x20, 0x21, 0x22, 0x23, 0x24,
                                0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x35,
                                0x36, 0x38, 0x39, 0x3A, 0x3D, 0x3E])
def test_every_immediate_opcode_has_its_immediate_masked(op):
    a = (op << 26) | (4 << 21) | (2 << 16) | 0x0010
    b = (op << 26) | (4 << 21) | (2 << 16) | 0x0020
    assert shapes.normalize(words(a)) == shapes.normalize(words(b))


@pytest.mark.parametrize("op", [0x01, 0x04, 0x05, 0x06, 0x07, 0x11, 0x14, 0x15, 0x16, 0x17, 0x34])
def test_branches_and_vfpu_arithmetic_keep_their_low_bits(op):
    a = (op << 26) | (4 << 21) | (2 << 16) | 0x0010
    b = (op << 26) | (4 << 21) | (2 << 16) | 0x0020
    assert shapes.normalize(words(a)) != shapes.normalize(words(b))


def test_text_section_without_text_raises():
    shstr = b"\0.data\0.shstrtab\0"
    sh_off = 52 + len(shstr)
    header = (b"\x7fELF\x01\x01\x01" + b"\0" * 9 +
              struct.pack("<HHIIIIIHHHHHH", 0xFFA0, 8, 1, 0, 0, sh_off, 0, 52, 0, 0, 40, 3, 2))
    sections = (struct.pack("<10I", *[0] * 10) + struct.pack("<10I", 1, 1, 3, 0, 52, 0, 0, 0, 4, 0) +
                struct.pack("<10I", 7, 3, 0, 0, 52, len(shstr), 0, 0, 1, 0))
    with pytest.raises(ValueError):
        shapes.text_section(header + shstr + sections)


def test_split_inside_keeps_functions_within_the_section_and_counts_the_rest():
    inside, outside = shapes.split_inside({0x28: 8, 0x30: 8, 0x40: 8}, base=0x28, length=0x10)
    assert inside == {0x28: 8, 0x30: 8} and outside == 1


def test_done_counts_template_registrations_too(tmp_path):
    (tmp_path / "game").mkdir()
    (tmp_path / "game" / "a.cpp").write_text("GT_DECOMP_CONSTRUCT(0x08804100u, 0x08804200u, 0x08CC0000u);\n"
                                             "GT_DECOMP(0x08804300u, gt::game::f, gt::ReturnKind::Word);\n")
    assert set(shapes.done_functions(tmp_path)) == {0x100, 0x300}


def test_recompiled_functions_are_the_registered_entries_as_elf_addresses():
    unit = ("    runtime.register_function(0x0891A484u, &recomp_unit_0034, \"recomp_unit_0034\");\n"
            "    ctx.pc = 0x0891A454u;\n")
    assert shapes.recompiled_functions([unit]) == {0x0891A484 - 0x08804000}
