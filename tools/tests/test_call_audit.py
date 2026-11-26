"""Tests for tools/call_audit.py (spec docs/superpowers/specs/2026-11-26-call-arity-audit-design.md)."""

from pathlib import Path

import pytest

from tools import call_audit as ca

BASE = 0x08804000  # runtime = ELF vaddr + BASE


def addiu(rt, rs, imm):
    return (0x09 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)


def addu(rd, rs, rt):
    return (rs << 21) | (rt << 16) | (rd << 11) | 0x21


def lw(rt, off, base):
    return (0x23 << 26) | (base << 21) | (rt << 16) | (off & 0xFFFF)


def jal(runtime):
    return (0x03 << 26) | (((runtime - BASE) >> 2) & 0x03FFFFFF)


def j(runtime):
    return (0x02 << 26) | (((runtime - BASE) >> 2) & 0x03FFFFFF)


JR_RA = (31 << 21) | 0x08
NOP = 0
A0, A1, A2, A3, T0, S0, S1, V0 = 4, 5, 6, 7, 8, 16, 17, 2


def words_of_map(functions):
    return lambda address: functions.get(address)


# --- register use ---------------------------------------------------------------------------------------------------------

def test_reads_writes_of_common_instructions():
    assert ca.reads_writes(addu(V0, A0, A1)) == ({A0, A1}, {V0})
    assert ca.reads_writes(addiu(A1, S0, 8)) == ({S0}, {A1})
    assert ca.reads_writes(lw(A0, 4, S1)) == ({S1}, {A0})
    store = (0x2B << 26) | (S0 << 21) | (A2 << 16)
    assert ca.reads_writes(store) == ({S0, A2}, set())
    assert ca.reads_writes(JR_RA) == ({31}, set())
    assert ca.reads_writes(jal(0x08900000)) == (set(), {31})
    lui = (0x0F << 26) | (A0 << 16) | 0x1234
    assert ca.reads_writes(lui) == (set(), {A0})


def test_reads_writes_of_less_common_encodings():
    sll = (A1 << 16) | (A0 << 11)                       # sll a0, a1, 0
    assert ca.reads_writes(sll) == ({A1}, {A0})
    assert ca.reads_writes((A0 << 21) | (A1 << 16) | 0x18) == ({A0, A1}, set())          # mult a0, a1
    assert ca.reads_writes((V0 << 11) | 0x10) == (set(), {V0})                          # mfhi v0
    assert ca.reads_writes((0x01 << 26) | (A2 << 21)) == ({A2}, set())                  # bltz a2
    assert ca.reads_writes((0x06 << 26) | (A3 << 21)) == ({A3}, set())                  # blez a3
    assert ca.reads_writes((0x04 << 26) | (A0 << 21) | (A1 << 16)) == ({A0, A1}, set())  # beq a0, a1
    assert ca.reads_writes((0x22 << 26) | (S0 << 21) | (A0 << 16)) == ({S0, A0}, {A0})   # lwl a0, 0(s0)
    assert ca.reads_writes((0x11 << 26) | (0 << 21) | (A0 << 16)) == (set(), {A0})      # mfc1 a0
    assert ca.reads_writes((0x11 << 26) | (4 << 21) | (A0 << 16)) == ({A0}, set())      # mtc1 a0
    assert ca.reads_writes((0x31 << 26) | (S0 << 21)) == ({S0}, set())                  # lwc1 f0, 0(s0)
    assert ca.reads_writes((0x1F << 26) | (A0 << 21) | (A1 << 16)) == ({A0}, {A1})      # ext a1, a0
    assert ca.reads_writes((0x1F << 26) | (A0 << 21) | (A1 << 16) | 4) == ({A0, A1}, {A1})  # ins a1, a0
    assert ca.reads_writes((0x1F << 26) | (A1 << 16) | (V0 << 11) | 0x20) == ({A1}, {V0})  # seb v0, a1
    assert ca.reads_writes((0x12 << 26) | (3 << 21) | (A0 << 16)) == (set(), {A0})      # mfv a0
    assert ca.reads_writes((0x12 << 26) | (7 << 21) | (A0 << 16)) == ({A0}, set())      # mtv a0
    assert ca.reads_writes((0x34 << 26) | (A0 << 21)) == (set(), set())                 # a VFPU op: no GPR
    assert ca.reads_writes((0x36 << 26) | (S0 << 21)) == ({S0}, set())                  # lv.q: base register
    assert ca.reads_writes((0x30 << 26) | (V0 << 21) | (A1 << 16)) == ({V0}, {A1})      # ll a1, 0(v0)
    assert ca.reads_writes((0x38 << 26) | (V0 << 21) | (A1 << 16)) == ({V0, A1}, {A1})  # sc a1, 0(v0)


# --- callee parameters ----------------------------------------------------------------------------------------------------

def test_callee_parameters_are_registers_read_before_written():
    callee = 0x08900000
    funcs = {callee: [addu(V0, A0, A1), JR_RA, NOP]}
    assert ca.callee_params(callee, words_of_map(funcs)) == frozenset({A0, A1})


def test_a_written_register_is_not_a_parameter():
    callee = 0x08900000
    funcs = {callee: [addiu(A1, 0, 5), addu(V0, A0, A1), JR_RA, NOP]}
    assert ca.callee_params(callee, words_of_map(funcs)) == frozenset({A0})


def test_parameters_pass_through_to_a_nested_call_and_a_tail_call():
    inner, middle, tail = 0x08900000, 0x08900100, 0x08900200
    funcs = {inner: [addu(V0, A2, 0), JR_RA, NOP],
             middle: [jal(inner), NOP, JR_RA, NOP],
             tail: [j(inner), NOP]}
    assert ca.callee_params(middle, words_of_map(funcs)) == frozenset({A2})
    assert ca.callee_params(tail, words_of_map(funcs)) == frozenset({A2})


def test_a_register_read_after_a_call_is_not_a_parameter():
    callee, other = 0x08900000, 0x08900100
    funcs = {callee: [jal(other), NOP, addu(V0, A0, 0), JR_RA, NOP], other: [JR_RA, NOP]}
    assert ca.callee_params(callee, words_of_map(funcs)) == frozenset()   # a0 is clobbered by the call


def test_unknown_or_recursive_callees_do_not_loop():
    a, b = 0x08900000, 0x08900100
    funcs = {a: [addu(V0, A0, 0), jal(b), NOP, JR_RA, NOP], b: [jal(a), NOP, JR_RA, NOP]}
    assert ca.callee_params(a, words_of_map(funcs)) == frozenset({A0})
    assert ca.callee_params(0x08A00000, words_of_map(funcs)) == frozenset(ca.ARGS[:2])   # unknown: reads a0-a1


def test_a_tail_call_to_an_import_stub_reads_what_the_wrapper_did_not_write():
    wrapper = 0x08900000
    funcs = {wrapper: [addiu(A1, 0, 0), j(0x08C7A988), NOP]}                      # the stub has no asm here
    assert ca.callee_params(wrapper, words_of_map(funcs)) == frozenset({A0})


# --- caller windows -------------------------------------------------------------------------------------------------------

def test_call_sites_record_argument_registers_written_since_the_previous_call():
    t1, t2 = 0x08900000, 0x08900100
    words = [addiu(A1, S0, 8), jal(t1), addiu(A0, S1, 0),   # a1 before, a0 in the delay slot
             addiu(A2, 0, 1), jal(t2), NOP]                 # only a2 counts for t2
    assert ca.call_sites(words) == [(t1, frozenset({A0, A1})), (t2, frozenset({A2}))]


# --- C side ---------------------------------------------------------------------------------------------------------------

def test_c_calls_count_the_first_argument_list():
    body = ("call(rt, ctx, 0x08B2E2B8u);\n"
            "x = call(rt, ctx, 0x08B855E0u, {0x08CF6244u, frame.at(0x8u)});\n"
            "callf(rt, ctx, 0x08B71CE0u, {a}, {1.0f, 2.0f});\n"
            "call_virtual(rt, ctx, target, target, {object});\n")
    assert ca.c_calls(body) == [("call", 0x08B2E2B8, 0), ("call", 0x08B855E0, 2), ("callf", 0x08B71CE0, 1)]


def test_assigned_registers():
    assert ca.assigned_regs("G[5] = 1u; ctx.gpr[6] = G[2]; if (G[7] == 0u) {}") == {5, 6}


def test_function_bodies_one_line_and_multi_line():
    source = ("void func_08800010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 1u; }\n"
              "void func_08800020(Runtime &rt, AllegrexContext &ctx) {\n    call(rt, ctx, 0x08900000u);\n}\n")
    bodies = ca.function_bodies(source)
    assert set(bodies) == {0x08800010, 0x08800020}
    assert "call(rt, ctx, 0x08900000u)" in bodies[0x08800020]


def test_v0_only_helpers_are_found_from_their_definitions():
    source = ("u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {\n"
              "    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);\n    return ctx.gpr[2];\n}\n"
              "GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {\n"
              "    const GuestResult result = call_guest(rt, ctx, target, arguments, {});\n"
              "    ctx.gpr[3] = result.v1;\n    return result;\n}\n")
    assert ca.v0_only_helpers(source) == {"call"}


# --- the two rules --------------------------------------------------------------------------------------------------------

CALLER = 0x08800100
TARGET = 0x08900000


def _funcs(caller_words):
    return words_of_map({CALLER: caller_words, TARGET: [addu(V0, A0, A1), JR_RA, NOP]})


def test_missing_argument_is_reported():
    words = [addiu(A1, S0, 8), jal(TARGET), addiu(A0, 0, 0x44), JR_RA, NOP]
    body = "call(rt, ctx, 0x08900000u, {0x44u});"
    assert ca.missing_arguments(CALLER, body, _funcs(words)) == [(TARGET, "a1", 1)]


def test_all_arguments_passed_is_clean():
    words = [addiu(A1, S0, 8), jal(TARGET), addiu(A0, 0, 0x44), JR_RA, NOP]
    assert ca.missing_arguments(CALLER, "call(rt, ctx, 0x08900000u, {0x44u, s + 8u});", _funcs(words)) == []


def test_a_register_the_body_sets_itself_is_not_missing():
    words = [addiu(A1, S0, 8), jal(TARGET), addiu(A0, 0, 0x44), JR_RA, NOP]
    assert ca.missing_arguments(CALLER, "G[5] = s + 8u; call(rt, ctx, 0x08900000u, {0x44u});", _funcs(words)) == []


def test_a_register_the_callee_does_not_read_is_not_missing():
    words = [addiu(A2, S0, 8), jal(TARGET), addiu(A0, 0, 0x44), JR_RA, NOP]
    assert ca.missing_arguments(CALLER, "call(rt, ctx, 0x08900000u, {0x44u});", _funcs(words)) == []


def test_lost_v1_after_a_v0_only_helper():
    body = "const u32 low = call(rt, ctx, 0x088D9C24u, {a});\nconst u32 high = ctx.gpr[3];\n"
    assert ca.lost_v1(body, {"call"}) == ["const u32 high = ctx.gpr[3];"]
    assert ca.lost_v1(body.replace("call(", "call_keep("), {"call"}) == []
    assert ca.lost_v1("call(rt, ctx, 0x1u);\nctx.gpr[3] = 4u;\nx = ctx.gpr[3];\n", {"call"}) == []


# --- command line ---------------------------------------------------------------------------------------------------------

def test_main_reports_and_exits_1(tmp_path, monkeypatch, capsys):
    src = tmp_path / "b.cpp"
    src.write_text("u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {\n"
                   "    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);\n    return ctx.gpr[2];\n}\n"
                   "void func_08800100(Runtime &rt, AllegrexContext &ctx) {\n"
                   "    call(rt, ctx, 0x08900000u, {0x44u});\n    u32 h = ctx.gpr[3];\n}\n")
    funcs = {CALLER: [addiu(A1, S0, 8), jal(TARGET), addiu(A0, 0, 0x44), JR_RA, NOP],
             TARGET: [addu(V0, A0, A1), JR_RA, NOP]}
    monkeypatch.setattr(ca, "_load_words_of", lambda elf, tsv: words_of_map(funcs))
    assert ca.main([str(src)]) == 1
    out = capsys.readouterr().out
    assert "func_08800100 -> 0x08900000: asm sets a1 (the callee reads it), C passes 1" in out
    assert "func_08800100: reads v1 after a v0-only call: u32 h = ctx.gpr[3];" in out
    assert "call audit: 1 files, 2 findings" in out


def test_main_clean_exits_0(tmp_path, monkeypatch, capsys):
    src = tmp_path / "b.cpp"
    src.write_text("void func_08800100(Runtime &rt, AllegrexContext &ctx) {\n    call(rt, ctx, 0x08900000u, {1u, 2u});\n}\n")
    funcs = {CALLER: [addiu(A1, S0, 8), jal(TARGET), addiu(A0, 0, 0x44), JR_RA, NOP],
             TARGET: [addu(V0, A0, A1), JR_RA, NOP]}
    monkeypatch.setattr(ca, "_load_words_of", lambda elf, tsv: words_of_map(funcs))
    assert ca.main([str(src)]) == 0
    assert "call audit: 1 files, 0 findings" in capsys.readouterr().out


@pytest.mark.needs_dump
def test_the_two_bugs_of_2026_11_26_are_found():
    elf, tsv = Path("extracted/EBOOT.elf"), Path("recomp/analysis/functions.tsv")
    if not elf.exists() or not tsv.exists():
        pytest.skip("needs the user's dump")
    words_of = ca._load_words_of(elf, tsv)
    hang = "const u32 low = call(rt, ctx, 0x088D9C24u, {s, h, n, m});\nwhile (1) { call(rt, ctx, 0x08B2E2B8u); }\n"
    assert (0x08B2E2B8, "a0", 0) in ca.missing_arguments(0x088D8918, hang, words_of)
    logo = "call(rt, ctx, 0x08B855E0u, {0x08CF6244u});\n"
    assert (0x08B855E0, "a1", 1) in ca.missing_arguments(0x08A17818, logo, words_of)


# --- precision (2026-11-26: the first run over src flagged 2,448 calls) ---------------------------------------------------

def test_family_helpers_are_not_calls_without_arguments():
    body = "fetch_singleton_string(rt, ctx, 0x0885D010u);\ncall(rt, ctx, 0x08900000u, {1u});"
    assert ca.c_calls(body) == [("call", 0x08900000, 1)]
    source = "u32 call_x(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {}\n"
    assert "call_x" in ca.call_helpers(source)
    assert ca.c_calls("call_x(rt, ctx, 0x08900000u, {1u});", ca.call_helpers(source)) == [("call_x", 0x08900000, 1)]


def test_the_window_restarts_at_a_branch_target_and_after_a_jump():
    t1 = 0x08900000
    beq_back = (0x04 << 26) | 1                             # beq zero, zero, +1 (unconditional, ends the block)
    words = [addiu(A1, S0, 8), beq_back, NOP, addiu(A2, 0, 1), jal(t1), NOP]
    assert ca.call_sites(words) == [(t1, frozenset({A2}))]  # a1 was set on another path
    bne = (0x05 << 26) | (A0 << 21) | 1                     # bne a0, zero, +1 -> index 3 is a branch target
    words = [addiu(A1, S0, 8), bne, NOP, addiu(A2, 0, 1), jal(t1), NOP]
    assert ca.call_sites(words) == [(t1, frozenset({A2}))]


def test_a_spill_of_an_argument_to_the_stack_is_not_a_use():
    callee = 0x08900000
    spill = (0x2B << 26) | (29 << 21) | (A3 << 16) | 0x1C   # sw a3, 0x1C(sp)
    funcs = {callee: [spill, addu(V0, A0, 0), JR_RA, NOP]}
    assert ca.callee_params(callee, words_of_map(funcs)) == frozenset({A0})


def test_an_argument_list_with_a_nested_call_and_line_breaks():
    body = ("call(rt, ctx, 0x0887EE40u,\n     {self, call(rt, ctx, 0x08AC2900u, {frame.at(0x14u)}), load8(h + 0x50u),\n"
            "      load8(h + 0x51u)});\n")
    assert ca.c_calls(body) == [("call", 0x0887EE40, 4), ("call", 0x08AC2900, 1)]
    assert ca.c_calls("call(rt, ctx, 0x08900000u, {1u") == [("call", 0x08900000, 0)]   # unterminated: no list


def test_a_nested_call_to_an_unknown_function_reads_nothing_of_its_caller():
    callee = 0x08900000
    funcs = {callee: [addiu(A0, 0, 1), jal(0x08C7A988), NOP, JR_RA, NOP]}
    assert ca.callee_params(callee, words_of_map(funcs)) == frozenset()


def test_a_jump_inside_the_function_is_not_a_tail_call():
    callee = 0x08900000
    funcs = {callee: [j(callee + 12), NOP, NOP, addu(V0, A0, 0), JR_RA, NOP]}   # j to its own 4th word
    assert ca.callee_params(callee, words_of_map(funcs)) == frozenset({A0})
