import struct
from pathlib import Path

import pytest

from tools import family_members as fm
from tools.progress import LOAD_BASE

# --- a tiny assembler: synthetic instruction words, no game bytes ---------------------------------------------------

ZERO, V0, V1, A0, A1, A2, A3, T0, T1, T2, T3, S0, SP, RA = 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 29, 31
NOP = 0
JR_RA = 0x03E00008


def i_type(op: int, rs: int, rt: int, imm: int) -> int:
    return op << 26 | rs << 21 | rt << 16 | (imm & 0xFFFF)


def lui(rt: int, imm: int) -> int:
    return i_type(0x0F, 0, rt, imm)


def addiu(rt: int, rs: int, imm: int) -> int:
    return i_type(0x09, rs, rt, imm)


def ori(rt: int, rs: int, imm: int) -> int:
    return i_type(0x0D, rs, rt, imm)


def lw(rt: int, offset: int, base: int) -> int:
    return i_type(0x23, base, rt, offset)


def sw(rt: int, offset: int, base: int) -> int:
    return i_type(0x2B, base, rt, offset)


def beq(rs: int, rt: int, offset: int) -> int:
    return i_type(0x04, rs, rt, offset)


def jal(vaddr: int) -> int:
    return 0x0C000000 | (vaddr >> 2)


def j(vaddr: int) -> int:
    return 0x08000000 | (vaddr >> 2)


def move(rd: int, rs: int) -> int:  # addu rd, rs, zero
    return rs << 21 | rd << 11 | 0x21


def pack(*words: int) -> bytes:
    return struct.pack(f"<{len(words)}I", *words)


# A 'static pair' member: v1 = &static; two loads through it; a relocated call; the constants are relocated
# (HI16 at 0, LO16 at 1, the call at 5).
def pair_member(static: int, callee: int) -> list[int]:
    return [lui(V1, (static + 0x8000) >> 16), addiu(V1, V1, static & 0xFFFF), lw(T0, 0, V1), lw(T1, 4, V1),
            addiu(A2, SP, 16), jal(callee), NOP, JR_RA, NOP]


PAIR_RELOCS = {0: fm.R_MIPS_HI16, 1: fm.R_MIPS_LO16, 5: fm.R_MIPS_26}
BASE = 0x08804000


def values(words, relocs=None, address=0x1000):
    return fm.member_values(words, address, relocs or {})


# --- decoding the words of one member --------------------------------------------------------------------------------

def test_lui_addiu_pair_with_relocations_resolves_to_a_runtime_address():
    vs = values(pair_member(0x4D8500, 0x300), PAIR_RELOCS)
    const = [v for v in vs if v.kind == "address"]
    assert [(v.index, v.value, v.sources) for v in const] == [(1, 0x4D8500 + LOAD_BASE, (0, 1))]


def test_jal_target_is_the_word_times_four_plus_the_load_base_when_relocated():
    vs = values(pair_member(0x4D8500, 0x300), PAIR_RELOCS)
    call = [v for v in vs if v.kind == "call"]
    assert [(v.index, v.value, v.sources) for v in call] == [(5, 0x300 + LOAD_BASE, (5,))]


def test_unrelocated_lui_addiu_is_a_plain_constant_not_an_address():
    words = [lui(V0, 0x3F80), addiu(V0, V0, 0), JR_RA, NOP]
    vs = values(words)
    assert [(v.kind, v.value) for v in vs] == [("const", 0x3F800000)]


def test_negative_low_half_borrows_from_the_high_half():
    words = [lui(V0, 0x0001), addiu(V0, V0, -0x10), JR_RA, NOP]
    assert [v.value for v in values(words)] == [0x0000FFF0]


def test_ori_completes_a_constant_too():
    words = [lui(V0, 0x0012), ori(V0, V0, 0x3456), JR_RA, NOP]
    assert [(v.kind, v.value) for v in values(words)] == [("const", 0x123456)]


def test_li_is_an_immediate_and_a_follow_up_addiu_on_it_is_not_an_address():
    words = [addiu(V0, ZERO, 168), addiu(V0, V0, 1), JR_RA, NOP]
    vs = values(words)
    assert [(v.kind, v.index, v.value) for v in vs] == [("imm", 0, 168), ("imm", 1, 1)]


def test_li_with_ori_is_an_immediate():
    assert [(v.kind, v.value) for v in values([ori(V0, ZERO, 0x8000), JR_RA, NOP])] == [("imm", 0x8000)]


def test_load_through_a_bare_lui_resolves_its_address_with_the_low_half_relocation():
    words = [lui(V0, 0x4D), lw(V1, -0x7B00, V0), JR_RA, NOP]
    vs = values(words, {0: fm.R_MIPS_HI16, 1: fm.R_MIPS_LO16})
    assert [(v.kind, v.index, v.value, v.sources) for v in vs] == [("access", 1, 0x4C8500 + LOAD_BASE, (0, 1))]


def test_a_load_through_a_completed_address_adds_no_second_value():
    vs = values(pair_member(0x4D8500, 0x300), PAIR_RELOCS)
    assert [v.kind for v in vs] == ["address", "imm", "call"]  # the imm is the frame address, addiu a2, sp, 16


def test_offsets_off_unknown_registers_are_displacements_and_immediates():
    words = [lw(V0, 12, A0), sw(V0, 8, A0), addiu(SP, SP, -32), JR_RA, NOP]
    assert [(v.kind, v.value) for v in values(words)] == [("disp", 12), ("disp", 8), ("imm", -32)]


def test_a_call_clears_what_the_caller_saved_registers_held_after_its_delay_slot():
    words = [lui(T0, 0x4D), jal(0x300), NOP, lw(V0, 4, T0), JR_RA, NOP]
    assert [(v.kind, v.index) for v in values(words, {1: fm.R_MIPS_26})] == [("hi", 0), ("call", 1), ("disp", 3)]


def test_a_callee_saved_register_survives_a_call():
    words = [lui(S0, 0x4D), jal(0x300), NOP, lw(V0, 4, S0), JR_RA, NOP]
    vs = values(words, {0: fm.R_MIPS_HI16, 1: fm.R_MIPS_26, 3: fm.R_MIPS_LO16})
    assert [(v.kind, v.index) for v in vs] == [("call", 1), ("access", 3)]


def test_the_delay_slot_of_a_call_still_sees_the_registers():
    words = [lui(V0, 0x4D), jal(0x300), addiu(A1, V0, 0x20), JR_RA, NOP]
    vs = values(words, {0: fm.R_MIPS_HI16, 1: fm.R_MIPS_26, 2: fm.R_MIPS_LO16})
    assert sorted((v.kind, v.index) for v in vs) == [("address", 2), ("call", 1)]


def test_a_branch_target_forgets_the_registers():
    words = [lui(V1, 0x4D), beq(A0, ZERO, 1), NOP, addiu(V1, V1, 4), JR_RA, NOP]
    # index 3 is the branch target: v1 is not known there (the other path may not have run the lui)
    assert [(v.kind, v.index) for v in values(words)] == [("hi", 0), ("imm", 3)]


def test_overwriting_a_register_forgets_its_value():
    words = [lui(V0, 0x4D), move(V0, A0), addiu(V0, V0, 4), JR_RA, NOP]
    assert [(v.kind, v.value) for v in values(words)] == [("hi", 0x4D0000), ("imm", 4)]


def test_a_load_into_a_register_forgets_its_value():
    words = [lui(V0, 0x4D), lw(V0, 0, A0), addiu(V0, V0, 4), JR_RA, NOP]
    assert [v.kind for v in values(words)] == ["hi", "disp", "imm"]


def test_a_store_does_not_forget_the_stored_register():
    words = [lui(V0, 0x4D), sw(V0, 0, A0), addiu(V0, V0, 4), JR_RA, NOP]
    assert [(v.kind, v.value) for v in values(words)] == [("disp", 0), ("const", 0x4D0004)]


def test_a_j_out_of_the_function_is_a_tail_target():
    vs = fm.member_values([j(0x300), NOP, JR_RA, NOP], 0x1000, {0: fm.R_MIPS_26})
    assert [(v.kind, v.index, v.value) for v in vs] == [("tail", 0, 0x300 + LOAD_BASE)]


def test_j_inside_the_function_is_internal_and_not_reported():
    vs = fm.member_values([j(0x310), NOP, JR_RA, NOP, JR_RA, NOP], 0x300, {0: fm.R_MIPS_26})
    assert vs == []


def test_unrelocated_jal_is_reported_as_a_raw_call():
    vs = values([jal(0x300), NOP, JR_RA, NOP])
    assert [(v.kind, v.value) for v in vs] == [("rawcall", 0x300)]


def test_jr_and_jalr_are_not_calls_with_a_known_target():
    words = [0x0080F809, NOP, 0x00800008, NOP]  # jalr a0 ; jr a0
    assert values(words) == []


# --- aligning a member with its template -----------------------------------------------------------------------------

def test_row_holds_only_the_values_that_differ_from_the_template():
    template = values(pair_member(0x4D8500, 0x300), PAIR_RELOCS)
    other = values(pair_member(0x4D8600, 0x300), PAIR_RELOCS)
    row = fm.row(template, other)
    assert [(v.kind, v.value) for v in row] == [("address", 0x4D8600 + LOAD_BASE)]


def test_row_raises_when_the_values_do_not_line_up():
    template = values(pair_member(0x4D8500, 0x300), PAIR_RELOCS)
    other = values([lui(V0, 1), JR_RA, NOP])
    with pytest.raises(fm.ShapeMismatch):
        fm.row(template, other)


def test_uncovered_differences_flag_words_no_value_explains():
    t = pair_member(0x4D8500, 0x300)
    m = list(t)
    m[2] = lw(T0, 8, V1)  # a different field offset: not a relocated constant
    covered = values(m, PAIR_RELOCS)
    assert fm.uncovered(t, m, covered) == [2]


def test_uncovered_is_empty_when_only_relocated_constants_differ():
    t, m = pair_member(0x4D8500, 0x300), pair_member(0x4D8610, 0x400)
    assert fm.uncovered(t, m, values(m, PAIR_RELOCS)) == []


# --- finding the members ---------------------------------------------------------------------------------------------

def code_of(**members) -> dict[int, bytes]:
    return {int(k.removeprefix("f"), 16): pack(*w) for k, w in members.items()}


def test_members_are_same_shape_functions_not_done_and_reached():
    code = code_of(f100=pair_member(0x4D8500, 0x300), f200=pair_member(0x4D8600, 0x400),
                   f300=pair_member(0x4D8700, 0x500), f400=[JR_RA, NOP], f500=pair_member(0x4D8800, 0x600))
    got = fm.find_members(code, template=0x100, reached={0x200, 0x300, 0x500}, done={0x500})
    assert got == [0x200, 0x300]


def test_members_with_reached_none_lists_every_pending_member():
    code = code_of(f100=pair_member(0x4D8500, 0x300), f200=pair_member(0x4D8600, 0x400))
    assert fm.find_members(code, template=0x100, reached=None, done=set()) == [0x200]


def test_the_template_is_never_its_own_member():
    code = code_of(f100=pair_member(0x4D8500, 0x300))
    assert fm.find_members(code, 0x100, None, set()) == []


def test_the_template_must_be_known():
    with pytest.raises(KeyError):
        fm.find_members({}, 0x100, None, set())


def test_families_lists_shapes_with_a_done_member_and_their_reached_pending_ones():
    code = code_of(f100=pair_member(0x4D8500, 0x300), f200=pair_member(0x4D8600, 0x400),
                   f300=pair_member(0x4D8700, 0x500), f400=[JR_RA, NOP], f500=[JR_RA, NOP], f600=[JR_RA, NOP])
    rows = fm.families(code, reached={0x200, 0x300, 0x400, 0x500}, done={0x100}, min_reached=2)
    assert [(r.template, r.pending) for r in rows] == [(0x100, [0x200, 0x300])]


def test_families_ignore_clusters_without_a_done_template_unless_asked():
    code = code_of(f400=[JR_RA, NOP], f500=[JR_RA, NOP], f600=[JR_RA, NOP])
    assert fm.families(code, {0x400, 0x500, 0x600}, set(), 2) == []
    rows = fm.families(code, {0x400, 0x500, 0x600}, set(), 2, need_template=False)
    assert [(r.template, r.pending) for r in rows] == [(0x400, [0x500, 0x600])]


# --- probe logs, symbols -----------------------------------------------------------------------------------------------

def test_reached_from_probe_logs_sums_hits_over_routes_and_ignores_unhit():
    race = "[probe] 0x08805FD0 hits=3 first=#0 c_08805FD0\n[probe] 0x08805FD4 hits=0 c_x\nnoise\n"
    buy = "[probe] 0x08805FD0 hits=2 first=#1 c_08805FD0\n[probe] 0x08805FE0 hits=1 first=#2 c_08805FE0\n"
    assert fm.reached_from_logs([race, buy]) == {0x08805FD0 - LOAD_BASE: 5, 0x08805FE0 - LOAD_BASE: 1}


def test_probe_symbols_name_every_candidate_of_at_most_the_given_size():
    sizes = {0x100: 8, 0x200: 400, 0x300: 8}
    text = fm.probe_symbols(sizes, done={0x300}, emitted={0x100, 0x200, 0x300}, max_insns=10)
    assert text == f"c_{0x100 + LOAD_BASE:08X} = 0x{0x100 + LOAD_BASE:08X}; // 2\n"


def test_probe_symbols_leave_out_what_the_recompiler_never_emitted():
    text = fm.probe_symbols({0x100: 8}, done=set(), emitted=set(), max_insns=10)
    assert text == ""


# --- the written rows: generating and checking them --------------------------------------------------------------------

def test_cpp_row_is_a_one_line_wrapper_call_with_the_row_values_in_word_order():
    t = values(pair_member(0x4D8500, 0x300), PAIR_RELOCS)
    m = values(pair_member(0x4D8600, 0x400), PAIR_RELOCS)
    line = fm.cpp_row(0x1234 + LOAD_BASE, "pair_call", fm.row(t, m))
    assert line == (f"void func_{0x1234 + LOAD_BASE:08X}(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) "
                    f"{{ pair_call(rt, ctx, 0x{0x4D8600 + LOAD_BASE:08X}u, 0x{0x400 + LOAD_BASE:08X}u); }}")


def test_parse_rows_reads_wrapper_calls_with_hex_and_decimal_arguments():
    src = ("void func_08BB186C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) "
           "{ call_with_constant(rt, ctx, 0x088BA424u, 168u); }\n"
           "void func_08C401C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }\n"
           "// void func_08000000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { nope(rt, ctx, 1u); }\n")
    assert fm.parse_rows(src) == [(0x08BB186C, "call_with_constant", [0x088BA424, 168]),
                                  (0x08C401C4, "clamp_step", [])]


def row_src(runtime: int, wrapper: str, *args: str) -> str:
    joined = "".join(f", {a}" for a in args)
    return (f"void func_{runtime:08X}(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) "
            f"{{ {wrapper}(rt, ctx{joined}); }}\n")


def hexu(value: int) -> str:
    return f"0x{value + LOAD_BASE:08X}u"


def pair_check(rows: dict[int, tuple[int, int]], lines: str):
    """check_rows over pair members: `rows` maps an ELF vaddr to the (static, callee) its words build."""
    words = {a: pair_member(s, c) for a, (s, c) in rows.items()}
    relocs = {a: PAIR_RELOCS for a in words}
    return fm.check_rows(lines, lambda a: words.get(a - LOAD_BASE), lambda a: relocs.get(a - LOAD_BASE, {}))


FAMILY = {0x1000: (0x4D8500, 0x300), 0x2000: (0x4D8600, 0x400), 0x3000: (0x4D8700, 0x500)}


def family_lines(*overrides: tuple[int, str, str]) -> str:
    """One row per FAMILY member (static, callee in word order); an override replaces the row of that address."""
    swap = {a: (x, y) for a, x, y in overrides}
    out = ""
    for a, (s, c) in FAMILY.items():
        x, y = swap.get(a, (hexu(s), hexu(c)))
        out += row_src(a + LOAD_BASE, "f", x, y)
    return out


def test_check_row_accepts_exactly_the_expected_values_in_column_order():
    assert fm.check_row([0x4D8500 + LOAD_BASE, 0x300 + LOAD_BASE], [0x4D8500 + LOAD_BASE, 0x300 + LOAD_BASE]) is None


def test_check_row_names_the_argument_and_column_that_differ():
    message = fm.check_row([0x4D8500 + LOAD_BASE, 0x401], [0x4D8500 + LOAD_BASE, 0x300 + LOAD_BASE])
    assert "argument 1" in message and "0x00000401" in message


def test_check_row_rejects_swapped_arguments():
    assert fm.check_row([0x300 + LOAD_BASE, 0x4D8500 + LOAD_BASE], [0x4D8500 + LOAD_BASE, 0x300 + LOAD_BASE]) is not None


def test_check_row_rejects_a_missing_or_an_extra_argument():
    expected = [1, 2]
    assert "1 argument" in fm.check_row([1], expected) and "2" in fm.check_row([1], expected)
    assert "3 argument" in fm.check_row([1, 2, 3], expected)


def test_check_row_compares_signed_values_modulo_2_32():
    assert fm.check_row([0xFFFFFFFF], [-1 & 0xFFFFFFFF]) is None


def test_check_rows_accepts_a_family_whose_rows_match_their_words():
    result = pair_check(FAMILY, family_lines())
    assert (result.ok, result.bad, result.unparsed) == (3, [], [])


def test_check_rows_fails_a_row_whose_arguments_are_swapped():
    swapped = (0x2000, hexu(0x400), hexu(0x4D8600))
    result = pair_check(FAMILY, family_lines(swapped))
    assert result.ok == 2 and [a for a, _ in result.bad] == [0x2000 + LOAD_BASE]


def test_check_rows_fails_a_row_with_fewer_or_more_arguments():
    words = {a: pair_member(s, c) for a, (s, c) in FAMILY.items()}
    short = family_lines().replace(f", {hexu(0x500)}", "")
    long = family_lines().replace(f", {hexu(0x500)}", f", {hexu(0x500)}, 0x1u")
    for source in (short, long):
        result = fm.check_rows(source, lambda a: words.get(a - LOAD_BASE), lambda a: PAIR_RELOCS)
        assert [a for a, _ in result.bad] == [0x3000 + LOAD_BASE], source


def test_check_rows_compares_each_argument_with_its_own_column_not_with_any_word():
    # 0x400 is a value of member 0x2000 (its callee) but the second column holds the callee, not the static
    wrong = (0x2000, hexu(0x4D8600), hexu(0x4D8600))
    result = pair_check(FAMILY, family_lines(wrong))
    assert [a for a, _ in result.bad] == [0x2000 + LOAD_BASE]


def test_check_rows_still_compares_by_position_when_two_columns_are_equal_in_a_member():
    words = {a: pair_member(s, c) for a, (s, c) in FAMILY.items()}
    words[0x3000] = pair_member(0x4D8700, 0x4D8700)  # static address and callee decode to the same number
    equal = (0x3000, hexu(0x4D8700), hexu(0x4D8700))
    result = fm.check_rows(family_lines(equal), lambda a: words.get(a - LOAD_BASE), lambda a: PAIR_RELOCS)
    assert result.ok == 3 and result.bad == []


def test_check_rows_counts_arguments_that_are_not_literals_as_unparsed_never_skipped():
    for argument in ("-1", "kStatic", "0x10u + 4u"):
        lines = family_lines((0x2000, argument, hexu(0x400)))
        result = pair_check(FAMILY, lines)
        assert result.unparsed == [0x2000 + LOAD_BASE], argument
        assert result.ok == 2 and result.bad == []


def test_check_rows_checks_a_lone_row_in_word_order_and_reports_it_as_alone():
    good = row_src(0x1000 + LOAD_BASE, "f", hexu(0x4D8500), hexu(0x300))
    swapped = row_src(0x1000 + LOAD_BASE, "f", hexu(0x300), hexu(0x4D8500))
    for source, bad in ((good, 0), (swapped, 1)):
        result = pair_check({0x1000: (0x4D8500, 0x300)}, source)
        assert (len(result.bad), result.single) == (bad, [0x1000 + LOAD_BASE])


def test_check_rows_ignores_rows_of_functions_it_does_not_know_and_rows_without_arguments():
    words = {0x1000: pair_member(0x4D8500, 0x300)}
    src = (row_src(0x1000 + LOAD_BASE, "f", hexu(0x4D8500), hexu(0x300)) + row_src(0x9000 + LOAD_BASE, "f", "1u")
           + row_src(0x3000 + LOAD_BASE, "g"))
    result = fm.check_rows(src, lambda a: words.get(a - LOAD_BASE), lambda a: PAIR_RELOCS)
    assert result.ok == 1 and result.unknown == [0x9000 + LOAD_BASE, 0x3000 + LOAD_BASE]


def test_check_rows_fails_a_row_that_does_not_line_up_with_the_others_of_its_wrapper():
    words = {a: pair_member(s, c) for a, (s, c) in FAMILY.items()}
    words[0x3000] = [lui(V1, 0), addiu(V1, V1, 5), JR_RA, NOP]
    result = fm.check_rows(family_lines(), lambda a: words.get(a - LOAD_BASE), lambda a: PAIR_RELOCS)
    assert result.ok == 2 and [a for a, _ in result.bad] == [0x3000 + LOAD_BASE]


# --- the real file formats -----------------------------------------------------------------------------------------------

def test_relocations_are_read_from_a_prx_rel_section():
    # r_offset (a vaddr), r_info (type in the low byte)
    section = struct.pack("<IIII", 0x30, 0x10005, 0x34, 0x10006)
    assert fm.parse_relocations(section) == {0x30: 0x10005, 0x34: 0x10006}


def build_elf(text: bytes, rel: bytes, text_addr: int = 0x28) -> bytes:
    """A minimal ELF32-LE with .text, a .rel.text of type 0x700000a0 and .shstrtab."""
    shstr = b"\0.text\0.rel.text\0.shstrtab\0"
    name = {n: shstr.index(b"\0" + n + b"\0") + 1 for n in (b".text", b".rel.text", b".shstrtab")}
    body = text + rel + shstr
    text_off, rel_off, str_off = 0x40, 0x40 + len(text), 0x40 + len(text) + len(rel)
    shoff = (0x40 + len(body) + 3) & ~3
    header = bytearray(0x40)
    header[:4] = b"\x7fELF"
    struct.pack_into("<I", header, 32, shoff)
    struct.pack_into("<HHH", header, 46, 40, 4, 3)
    sections = [(0,) * 10,
                (name[b".text"], 1, 6, text_addr, text_off, len(text), 0, 0, 4, 0),
                (name[b".rel.text"], 0x700000A0, 0, 0, rel_off, len(rel), 0, 1, 4, 8),
                (name[b".shstrtab"], 3, 0, 0, str_off, len(shstr), 0, 0, 1, 0)]
    out = bytes(header) + body
    out += b"\0" * (shoff - len(out))
    return out + b"".join(struct.pack("<10I", *s) for s in sections)


def test_elf_relocations_come_from_the_rel_text_section():
    text = pack(*pair_member(0x4D8500, 0x300))
    rel = struct.pack("<IIII", 0x28, 5, 0x2C, 6)
    elf = build_elf(text, rel)
    assert fm.elf_relocations(elf) == {0x28: 5, 0x2C: 6}


def test_function_words_and_relocations_are_cut_out_of_the_text_section():
    text = pack(*pair_member(0x4D8500, 0x300))
    rel = struct.pack("<IIIIII", 0x28, 5, 0x2C, 6, 0x28 + 5 * 4, 4)
    elf = build_elf(text, rel)
    image = fm.TextImage.from_elf(elf)
    words, relocs = image.function(0x28, 9 * 4)
    assert words == pair_member(0x4D8500, 0x300)
    assert relocs == {0: fm.R_MIPS_HI16, 1: fm.R_MIPS_LO16, 5: fm.R_MIPS_26}


def test_main_lists_members_with_their_values(tmp_path: Path, capsys):
    words = {0x28: pair_member(0x4D8500, 0x300), 0x4C: pair_member(0x4D8600, 0x400)}
    text = pack(*words[0x28], *words[0x4C])
    rel = struct.pack("<" + "II" * 6, 0x28, 5, 0x2C, 6, 0x28 + 20, 4, 0x4C, 5, 0x50, 6, 0x4C + 20, 4)
    elf = tmp_path / "x.elf"
    elf.write_bytes(build_elf(text, rel))
    tsv = tmp_path / "functions.tsv"
    tsv.write_text("28\t36\n4c\t36\n")
    rc = fm.main([f"0x{0x28 + LOAD_BASE:08X}", "--elf", str(elf), "--functions", str(tsv), "--no-src", "--no-generated"])
    out = capsys.readouterr().out
    assert rc == 0
    assert f"func_{0x4C + LOAD_BASE:08X}" in out
    assert f"0x{0x4D8600 + LOAD_BASE:08X}u" in out and f"0x{0x400 + LOAD_BASE:08X}u" in out


# --- the other command-line modes ------------------------------------------------------------------------------------

@pytest.fixture
def family_files(tmp_path: Path):
    """Three pair members at 0x28, 0x4c, 0x70 and a lone function at 0x94, as an ELF + functions.tsv."""
    members = [(0x28, 0x4D8500, 0x300), (0x4C, 0x4D8600, 0x400), (0x70, 0x4D8700, 0x500)]
    text, rel = b"", b""
    for address, static, callee in members:
        text += pack(*pair_member(static, callee))
        rel += struct.pack("<6I", address, 5, address + 4, 6, address + 20, 4)
    text += pack(JR_RA, NOP)
    elf = tmp_path / "x.elf"
    elf.write_bytes(build_elf(text, rel))
    tsv = tmp_path / "functions.tsv"
    tsv.write_text("28\t36\n4c\t36\n70\t36\n94\t8\n")
    return ["--elf", str(elf), "--functions", str(tsv), "--no-src", "--no-generated"], tmp_path


def test_main_cpp_prints_one_wrapper_line_per_member(family_files, capsys):
    common, _ = family_files
    assert fm.main([f"0x{0x28 + LOAD_BASE:08X}", *common, "--cpp", "pair_call"]) == 0
    out = capsys.readouterr().out
    assert f"void func_{0x4C + LOAD_BASE:08X}(" in out and "{ pair_call(rt, ctx, 0x08CCC600u, 0x08804400u); }" not in out
    assert f"{{ pair_call(rt, ctx, 0x{0x4D8600 + LOAD_BASE:08X}u, 0x{0x400 + LOAD_BASE:08X}u); }}" in out


def test_main_reached_filter_uses_the_probe_logs(family_files, capsys):
    common, tmp_path = family_files
    log = tmp_path / "probe.log"
    log.write_text(f"[probe] 0x{0x70 + LOAD_BASE:08X} hits=4 first=#0 c\n[probe] 0x{0x4C + LOAD_BASE:08X} hits=0 c\n")
    assert fm.main([f"0x{0x28 + LOAD_BASE:08X}", *common, "--probe", str(log), "-v"]) == 0
    out = capsys.readouterr().out
    assert "1 reached pending members" in out and f"func_{0x70 + LOAD_BASE:08X}  hits=4" in out
    assert f"func_{0x4C + LOAD_BASE:08X}" not in out and "address" in out


def test_main_list_shows_families_with_a_decompiled_template(family_files, capsys, monkeypatch):
    common, tmp_path = family_files
    monkeypatch.setattr("tools.shapes.done_functions", lambda src: {0x28: "t"})
    assert fm.main(["--list", "--min-reached", "2", *[a for a in common if a != "--no-src"]]) == 0
    assert f"template 0x{0x28 + LOAD_BASE:08X}" in capsys.readouterr().out


def test_main_writes_probe_symbols(family_files, capsys):
    common, tmp_path = family_files
    out = tmp_path / "syms.txt"
    assert fm.main(["--write-probe-symbols", str(out), "--max-insns", "9", *common]) == 0
    assert out.read_text().count("\n") == 4 and f"c_{0x28 + LOAD_BASE:08X} = " in out.read_text()


def test_main_check_exit_code_follows_the_rows(family_files, capsys):
    common, tmp_path = family_files
    good = f"void func_{0x28 + LOAD_BASE:08X}(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {{ f(rt, ctx, 0x{0x4D8500 + LOAD_BASE:08X}u, 0x{0x300 + LOAD_BASE:08X}u); }}\n"
    rows = tmp_path / "rows.cpp"
    rows.write_text(good)
    assert fm.main(["--check", str(rows), *common]) == 0
    rows.write_text(good.replace("0x300", "0x301").replace(f"{0x300 + LOAD_BASE:08X}", f"{0x301 + LOAD_BASE:08X}"))
    assert fm.main(["--check", str(rows), *common]) == 1
    assert "BAD" in capsys.readouterr().out


def test_main_flags_a_member_whose_other_words_differ(family_files, capsys):
    common, tmp_path = family_files
    elf = Path(common[1])
    data = bytearray(elf.read_bytes())
    # the 'lw t1, 4(v1)' of the second member becomes 'lw t1, 8(v1)': a field offset, not a relocated constant
    text_offset = 0x40 + 0x24 + 3 * 4
    struct.pack_into("<I", data, text_offset, lw(T1, 8, V1))
    elf.write_bytes(bytes(data))
    assert fm.main([f"0x{0x28 + LOAD_BASE:08X}", *common]) == 1
    assert "no value explaining" in capsys.readouterr().out


def test_main_rejects_an_unknown_template(family_files, capsys):
    common, _ = family_files
    assert fm.main(["0x0880ABCD", *common]) == 2


def test_uncovered_accepts_the_same_relative_internal_jump():
    t = [j(0x110), NOP]
    m = [j(0x210), NOP]
    assert fm.uncovered(t, m, [], 0x100, 0x200) == []
    assert fm.uncovered(t, m, []) == [0]


def test_uncovered_reports_a_jump_that_is_not_relative():
    assert fm.uncovered([j(0x110)], [j(0x220)], [], 0x100, 0x200) == [0]


def test_a_variable_family_without_a_template_lists_members_after_the_first(family_files, capsys):
    rows = fm.families({1: pack(JR_RA, NOP), 2: pack(JR_RA, NOP), 3: pack(JR_RA, NOP)}, None, {2}, 1)
    assert rows == [fm.Family(2, [1, 3], [2])]


def test_an_address_relocated_against_the_data_segment_adds_that_segments_base():
    info = {0: 0x10000 | fm.R_MIPS_HI16, 1: 0x10000 | fm.R_MIPS_LO16, 5: fm.R_MIPS_26}
    vs = fm.member_values(pair_member(0x1000, 0x300), 0x1000, info, segments=[0, 0x4D8500])
    assert [(v.kind, v.value) for v in vs if v.kind != "imm"] == [("address", 0x1000 + 0x4D8500 + LOAD_BASE),
                                                                ("call", 0x300 + LOAD_BASE)]


def test_an_access_through_a_bare_lui_uses_the_segment_of_its_relocation():
    info = {0: 0x10000 | fm.R_MIPS_HI16, 1: 0x10000 | fm.R_MIPS_LO16}
    vs = fm.member_values([lui(V0, 0x10), lw(V1, 8, V0), JR_RA, NOP], 0x1000, info, segments=[0, 0x4D8500])
    assert [(v.kind, v.value) for v in vs] == [("access", 0x100008 + 0x4D8500 + LOAD_BASE)]


def test_a_segment_number_outside_the_table_adds_nothing():
    info = {0: 0x50000 | fm.R_MIPS_HI16, 1: 0x50000 | fm.R_MIPS_LO16}
    vs = fm.member_values([lui(V0, 0x10), addiu(V0, V0, 4), JR_RA, NOP], 0x1000, info, segments=[0])
    assert [v.value for v in vs] == [0x100004 + LOAD_BASE]


def test_text_image_reads_the_program_headers_vaddrs(tmp_path):
    elf = bytearray(build_elf(pack(JR_RA, NOP), struct.pack("<II", 0x28, 5)))
    # append two program headers after the section headers and point the ELF header at them
    phoff = len(elf)
    for vaddr in (0, 0x4D8500):
        elf += struct.pack("<8I", 1, 0, vaddr, 0, 0, 0, 0, 0)
    struct.pack_into("<I", elf, 28, phoff)
    struct.pack_into("<HH", elf, 42, 32, 2)
    assert fm.TextImage.from_elf(bytes(elf)).segments == [0, 0x4D8500]


def test_a_lui_nothing_consumes_is_a_high_half_value():
    vs = values([lui(V0, 0x3F80), 0x44820000, JR_RA, NOP])  # lui; mtc1 v0, f0
    assert [(v.kind, v.value) for v in vs] == [("hi", 0x3F800000)]


def test_compare_and_logic_immediates_are_values_too():
    words = [i_type(0x0A, A0, V0, -5), i_type(0x0C, A0, V1, 0xFF), JR_RA, NOP]
    assert [(v.kind, v.value) for v in values(words)] == [("imm", -5), ("imm", 255)]


def test_li_with_zero_base_and_negative_immediate_is_kept_signed():
    assert [(v.kind, v.value) for v in values([addiu(V0, ZERO, -1), JR_RA, NOP])] == [("imm", -1)]


def V(index, value, kind="address"):
    return fm.Value(index, kind, value, (index,))


def test_columns_keep_the_positions_where_some_member_differs_even_if_one_equals_the_template():
    template = [V(1, 10), V(2, 12, "imm"), V(3, 99, "call")]
    members = [[V(1, 11), V(2, 12, "imm"), V(3, 99, "call")], [V(1, 12), V(2, 8, "imm"), V(3, 99, "call")]]
    assert fm.columns(template, members) == [0, 1]


def test_columns_drop_a_position_that_always_repeats_an_earlier_one():
    template = [V(1, 10), V(2, 10), V(3, 5, "imm")]
    members = [[V(1, 20), V(2, 20), V(3, 5, "imm")], [V(1, 30), V(2, 30), V(3, 5, "imm")]]
    assert fm.columns(template, members) == [0]


def test_columns_keep_a_position_that_repeats_an_earlier_one_only_sometimes():
    template = [V(1, 10), V(2, 10)]
    members = [[V(1, 20), V(2, 20)], [V(1, 30), V(2, 31)]]
    assert fm.columns(template, members) == [0, 1]


def test_parameters_return_each_members_values_at_the_columns_and_the_strays():
    template = [V(1, 10), V(3, 5, "call")]
    good = {0x10: [V(1, 11), V(3, 5, "call")], 0x20: [V(1, 12), V(3, 5, "call")]}
    cols, rows, stray = fm.parameters(template, {**good, 0x30: [V(2, 1)]})
    assert cols == [0] and [v.value for v in rows[0x20]] == [12] and stray == [0x30]


def test_main_check_fails_swapped_arguments_and_unparsed_rows_unless_allowed(family_files, capsys):
    common, tmp_path = family_files
    rows = tmp_path / "rows.cpp"
    lines = [row_src(0x28 + LOAD_BASE, "f", hexu(0x4D8500), hexu(0x300)),
             row_src(0x4C + LOAD_BASE, "f", hexu(0x4D8600), hexu(0x400)),
             row_src(0x70 + LOAD_BASE, "f", hexu(0x4D8700), hexu(0x500))]
    rows.write_text("".join(lines))
    assert fm.main(["--check", str(rows), *common]) == 0
    rows.write_text("".join(lines[:2]) + row_src(0x70 + LOAD_BASE, "f", hexu(0x500), hexu(0x4D8700)))
    assert fm.main(["--check", str(rows), *common]) == 1
    assert "BAD func_08804070" in capsys.readouterr().out
    rows.write_text("".join(lines[:2]) + row_src(0x70 + LOAD_BASE, "f", "-1", hexu(0x500)))
    assert fm.main(["--check", str(rows), *common]) == 1
    out = capsys.readouterr().out
    assert "1 unparsed" in out and "UNPARSED func_08804070" in out
    assert fm.main(["--check", str(rows), "--allow-unparsed", *common]) == 0
