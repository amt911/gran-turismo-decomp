from tools import contract_audit as audit

UNIT = """\
L_08800000:
    ctx.gpr[31] = (0x08800010u);
    ctx.gpr[4] = (ctx.gpr[16] + 0u);
    ctx.pc = 0x08900000u;
    return;
L_08800010:
    { const bool branch_taken = ctx.gpr[2] == 0u;
      if (branch_taken) {
          goto L_08800020;
      }
    }
L_08800020:
    ctx.gpr[31] = (0x08800030u);
    goto L_08900100;
L_08800030:
    ctx.gpr[2] = (ctx.gpr[17] + 0u);
    aot_mem.aot_store32(ctx.gpr[29] + static_cast<std::uint32_t>(4), ctx.gpr[2]);
    ctx.gpr[31] = (0x08800040u);
    if (rt.invoke_chained_direct<&recomp_unit_0001_entry, 1u, 2u, 0x08900200u>(ctx, &aot_mem) && ctx.pc == 0x08800040u) goto L_08800040;
    return;
L_08800040:
    aot_mem.aot_store32(ctx.gpr[16] + static_cast<std::uint32_t>(0), ctx.gpr[3]);
    ctx.gpr[31] = (0x08800050u);
    ctx.pc = 0x08900300u;
    return;
L_08800050:
    // nop
    ctx.gpr[3] = (ctx.gpr[3] + 1u);
"""

SOURCE = """\
GT_DECOMP(0x08900000u, gt::game::reads_v0_caller, gt::ReturnKind::Void);
GT_DECOMP_NESTED(0x08900100u, gt::game::killed, gt::ReturnKind::Void);
SPEC_DB(0x08900200u, get_two, Word);
// GT_DECOMP(0x08900300u, gt::game::commented, gt::ReturnKind::Word);
GT_DECOMP(0x08900400u, gt::game::passes_on, gt::ReturnKind::Word);
"""


def test_registrations_read_kind_and_nested_flag_and_skip_commented_ones():
    regs = audit.registrations(SOURCE)
    assert regs[0x08900000] == audit.Registration("reads_v0_caller", "Void", False)
    assert regs[0x08900100] == audit.Registration("killed", "Void", True)
    assert regs[0x08900200] == audit.Registration("get_two", "Word", False)
    assert 0x08900300 not in regs


def test_call_sites_find_returns_after_pc_goto_and_chained_calls():
    assert audit.call_sites(UNIT) == [(0x08900000, 0x08800010), (0x08900100, 0x08800030),
                                      (0x08900200, 0x08800040), (0x08900300, 0x08800050)]


def test_reads_after_return_follows_the_block_until_a_write_or_control_flow():
    assert audit.reads_after(UNIT, 0x08800010, 2)        # branch on v0
    assert not audit.reads_after(UNIT, 0x08800030, 2)    # v0 written first
    assert audit.reads_after(UNIT, 0x08800040, 3)        # v1 stored
    assert audit.reads_after(UNIT, 0x08800050, 3)        # v1 = v1 + 1 reads it
    assert not audit.reads_after(UNIT, 0x08800050, 2)


def test_uncompared_registers_of_each_kind():
    assert audit.UNCOMPARED["Void"] == (2, 3) and audit.UNCOMPARED["Word"] == (3,)
    assert audit.UNCOMPARED["DoubleWord"] == ()


def test_original_callers_that_read_an_uncompared_register_are_reported():
    found = audit.original_readers({"u.cpp": UNIT}, audit.registrations(SOURCE))
    assert found == {0x08900000: [("v0", 0x08800010)], 0x08900200: [("v1", 0x08800040)]}


DECOMPILED = """\
constexpr std::uint32_t kOther = 0x08900000u;
void a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t token = call_guest(rt, ctx, kOther, {});
    (void)call_guest(rt, ctx, 0x08900000u, {});
    call_guest(rt, ctx, 0x08900000u, {});
    if (token != 0u) (void)call_guest(rt, ctx, 0x08900000u, {});
}
void passes_on(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = call_guest(rt, ctx, 0x08900100u, {});
}
"""


def test_decompiled_callers_that_use_a_void_result_are_reported():
    regs = audit.registrations(SOURCE)
    assert audit.decompiled_readers({"a.cpp": DECOMPILED}, regs) == {0x08900000: ["a.cpp: const std::uint32_t "
                                                                                 "token = call_guest(rt, ctx, kOther, {});"],
                                                                    0x08900100: ["a.cpp: ctx.gpr[2] = call_guest(rt, ctx, "
                                                                                 "0x08900100u, {});"]}


def test_a_shorthand_macro_defined_as_gt_decomp_nested_registers_nested_functions():
    source = ("#define RACE_OPERATOR(address, name, kind) GT_DECOMP_NESTED(address, gt::game::race_operator::name, "
              "ReturnKind::kind)\n"
              "RACE_OPERATOR(0x0885C188u, get_account_car_parameter, Void);\n"
              "#define OTHER(address, name, kind) GT_DECOMP(address, gt::game::other::name, ReturnKind::kind)\n"
              "OTHER(0x08800000u, plain, Word);\n")
    regs = audit.registrations(source)
    assert regs[0x0885C188] == audit.Registration("get_account_car_parameter", "Void", True)
    assert regs[0x08800000] == audit.Registration("plain", "Word", False)
