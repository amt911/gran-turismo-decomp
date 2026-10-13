import struct

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
