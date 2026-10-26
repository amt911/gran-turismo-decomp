// Hot batch 161: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_161 {
namespace {

using namespace gt::game::hot;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 of the callee stay in the registers.
inline GuestResult call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                            std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// bound_node(out, _, first, second, third) with the member's vtable: the same bound-call node as batch 104's
// func_08BF3EA4 (its template, named there). func_08A69B3C(sp + 8); a 0x2C-byte object from func_08A5BC20(0x2C, 4)
// initialised by func_08A98220(object, sp + 8) gets the vtable and the three two-word pairs first / second / third at
// +0x14 / +0x1C / +0x24; func_08A69D68(sp + 8) releases the temporary; the object goes into a counted pointer on the
// stack (func_08A5D414), func_08A7C5C8(out, pointer) takes it and func_08A5B9EC releases the pointer (its v0 back).
// The original tests first[0] against 0x08B867AC and first[1] against 0 on the way, and stores the same two words on
// every path.
void bound_node(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t first = ctx.gpr[6], second = ctx.gpr[7], third = ctx.gpr[8];
    put(rt, frame.at(0x10u), ctx.gpr[4]);
    const std::uint32_t f0 = word(rt, first), f1 = word(rt, first + 4u);
    const std::uint32_t s0 = word(rt, second), t0 = word(rt, third), s1 = word(rt, second + 4u);
    const std::uint32_t t1 = word(rt, third + 4u);
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(8u)});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u}).v0;
    call_set(rt, ctx, 0x08A98220u, {object, frame.at(8u)});
    put(rt, object, vtable);
    put(rt, object + 0x18u, f1);
    put(rt, object + 0x14u, f0);
    put(rt, object + 0x20u, s1);
    put(rt, object + 0x1Cu, s0);
    put(rt, object + 0x28u, t1);
    put(rt, object + 0x24u, t0);
    ctx.gpr[6] = 0x08B80000u;
    ctx.gpr[3] = 0x08B867ACu;
    ctx.gpr[2] = (f0 == 0x08B867ACu && f1 == 0u) ? 0u : f1;
    call_set(rt, ctx, 0x08A69D68u, {frame.at(8u), f0});
    put(rt, frame.at(0u), object);
    call_set(rt, ctx, 0x08A5D414u, {frame.at(4u), frame.at(0u)});
    call_set(rt, ctx, 0x08A7C5C8u, {word(rt, frame.at(0x10u)), frame.at(4u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}

} // namespace

// Template of its family (a pending member; the other members are the batch 161 rows below).
void func_08B8C6BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CBE9C8u); }

// Batch 161 rows (tools.family_rows).
void func_08B8C838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CBE938u); }
void func_08BEA0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCDCD8u); }
void func_08BEA244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCDC00u); }
void func_08BEA3C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCDB70u); }
void func_08BEA53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCDA98u); }
void func_08BEA858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCDC90u); }
void func_08BEAA6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCDB28u); }
void func_08BEE27C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE0B0u); }
void func_08BEFF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE258u); }
void func_08BF00C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE1C8u); }
void func_08BF1B9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE450u); }
void func_08BF1D18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE408u); }
void func_08BF1E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE330u); }
void func_08BF2010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE2E8u); }
void func_08BF3848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE4B0u); }
void func_08BF43B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE698u); }
void func_08BF5CF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE880u); }
void func_08BF5E6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE838u); }
void func_08BF5FE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE7F0u); }
void func_08BF7F60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE958u); }
void func_08BF80DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCE8C8u); }
void func_08BFAF4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEC90u); }
void func_08BFB0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEC00u); }
void func_08BFB244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEBB8u); }
void func_08BFB3C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEB70u); }
void func_08BFD5E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEE38u); }
void func_08BFD760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEDF0u); }
void func_08BFEE30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEF68u); }
void func_08BFEFAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCEF20u); }
void func_08BFFF08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF0C8u); }
void func_08C00084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF080u); }
void func_08C02DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF3F8u); }
void func_08C02F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF3B0u); }
void func_08C030B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF368u); }
void func_08C03608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF2D8u); }
void func_08C04964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF580u); }
void func_08C04AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF538u); }
void func_08C04C5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF4F0u); }
void func_08C04DD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF460u); }
void func_08C05038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF4A8u); }
void func_08C06AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF6E8u); }
void func_08C06C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF6A0u); }
void func_08C06DB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF658u); }
void func_08C08198(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF868u); }
void func_08C08314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF820u); }
void func_08C08490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF7D8u); }
void func_08C09964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF9A0u); }
void func_08C09AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF958u); }
void func_08C09C5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCF910u); }
void func_08C0B6BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCFC00u); }
void func_08C0B838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCFBB8u); }
void func_08C0F31C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCFDD8u); }
void func_08C0F928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCFE20u); }
void func_08C102A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CCFEF8u); }
void func_08C12674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD0158u); }
void func_08C127F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD0110u); }
void func_08C1296C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD0080u); }
void func_08C14B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD0350u); }
void func_08C14CA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD0308u); }
void func_08C14E20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD02C0u); }
void func_08C1DB8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD1F68u); }
void func_08C1E9A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD22D0u); }
void func_08C29EE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD4228u); }
void func_08C2CA48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD4418u); }
void func_08C36CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD5690u); }
void func_08C36E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bound_node(rt, ctx, 0x08CD5648u); }

} // namespace hot_batch_161
} // namespace gt::game

#define HOT161(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_161::name, gt::ReturnKind::kind)
// HOT161(0x08B8C6BCu, func_08B8C6BC, Word);  // no comparable synthetic case yet
// HOT161(0x08B8C838u, func_08B8C838, Word);  // no comparable synthetic case yet
// HOT161(0x08BEA0C8u, func_08BEA0C8, Word);  // no comparable synthetic case yet
// HOT161(0x08BEA244u, func_08BEA244, Word);  // no comparable synthetic case yet
// HOT161(0x08BEA3C0u, func_08BEA3C0, Word);  // no comparable synthetic case yet
// HOT161(0x08BEA53Cu, func_08BEA53C, Word);  // no comparable synthetic case yet
// HOT161(0x08BEA858u, func_08BEA858, Word);  // no comparable synthetic case yet
// HOT161(0x08BEAA6Cu, func_08BEAA6C, Word);  // no comparable synthetic case yet
HOT161(0x08BEE27Cu, func_08BEE27C, Word);
HOT161(0x08BEFF44u, func_08BEFF44, Word);
// HOT161(0x08BF00C0u, func_08BF00C0, Word);  // no comparable synthetic case yet
HOT161(0x08BF1B9Cu, func_08BF1B9C, Word);
HOT161(0x08BF1D18u, func_08BF1D18, Word);
HOT161(0x08BF1E94u, func_08BF1E94, Word);
// HOT161(0x08BF2010u, func_08BF2010, Word);  // no comparable synthetic case yet
// HOT161(0x08BF3848u, func_08BF3848, Word);  // no comparable synthetic case yet
HOT161(0x08BF43B0u, func_08BF43B0, Word);
HOT161(0x08BF5CF0u, func_08BF5CF0, Word);
// HOT161(0x08BF5E6Cu, func_08BF5E6C, Word);  // no comparable synthetic case yet
// HOT161(0x08BF5FE8u, func_08BF5FE8, Word);  // no comparable synthetic case yet
// HOT161(0x08BF7F60u, func_08BF7F60, Word);  // no comparable synthetic case yet
HOT161(0x08BF80DCu, func_08BF80DC, Word);
HOT161(0x08BFAF4Cu, func_08BFAF4C, Word);
HOT161(0x08BFB0C8u, func_08BFB0C8, Word);
HOT161(0x08BFB244u, func_08BFB244, Word);
// HOT161(0x08BFB3C0u, func_08BFB3C0, Word);  // no comparable synthetic case yet
// HOT161(0x08BFD5E4u, func_08BFD5E4, Word);  // no comparable synthetic case yet
// HOT161(0x08BFD760u, func_08BFD760, Word);  // no comparable synthetic case yet
HOT161(0x08BFEE30u, func_08BFEE30, Word);
// HOT161(0x08BFEFACu, func_08BFEFAC, Word);  // no comparable synthetic case yet
HOT161(0x08BFFF08u, func_08BFFF08, Word);
// HOT161(0x08C00084u, func_08C00084, Word);  // no comparable synthetic case yet
// HOT161(0x08C02DC0u, func_08C02DC0, Word);  // no comparable synthetic case yet
HOT161(0x08C02F3Cu, func_08C02F3C, Word);
// HOT161(0x08C030B8u, func_08C030B8, Word);  // no comparable synthetic case yet
HOT161(0x08C03608u, func_08C03608, Word);
HOT161(0x08C04964u, func_08C04964, Word);
// HOT161(0x08C04AE0u, func_08C04AE0, Word);  // no comparable synthetic case yet
// HOT161(0x08C04C5Cu, func_08C04C5C, Word);  // no comparable synthetic case yet
// HOT161(0x08C04DD8u, func_08C04DD8, Word);  // no comparable synthetic case yet
// HOT161(0x08C05038u, func_08C05038, Word);  // no comparable synthetic case yet
// HOT161(0x08C06AC0u, func_08C06AC0, Word);  // no comparable synthetic case yet
// HOT161(0x08C06C3Cu, func_08C06C3C, Word);  // no comparable synthetic case yet
HOT161(0x08C06DB8u, func_08C06DB8, Word);
// HOT161(0x08C08198u, func_08C08198, Word);  // no comparable synthetic case yet
// HOT161(0x08C08314u, func_08C08314, Word);  // no comparable synthetic case yet
// HOT161(0x08C08490u, func_08C08490, Word);  // no comparable synthetic case yet
HOT161(0x08C09964u, func_08C09964, Word);
// HOT161(0x08C09AE0u, func_08C09AE0, Word);  // no comparable synthetic case yet
// HOT161(0x08C09C5Cu, func_08C09C5C, Word);  // no comparable synthetic case yet
// HOT161(0x08C0B6BCu, func_08C0B6BC, Word);  // no comparable synthetic case yet
HOT161(0x08C0B838u, func_08C0B838, Word);
// HOT161(0x08C0F31Cu, func_08C0F31C, Word);  // no comparable synthetic case yet
// HOT161(0x08C0F928u, func_08C0F928, Word);  // no comparable synthetic case yet
// HOT161(0x08C102A8u, func_08C102A8, Word);  // no comparable synthetic case yet
HOT161(0x08C12674u, func_08C12674, Word);
// HOT161(0x08C127F0u, func_08C127F0, Word);  // no comparable synthetic case yet
HOT161(0x08C1296Cu, func_08C1296C, Word);
// HOT161(0x08C14B28u, func_08C14B28, Word);  // no comparable synthetic case yet
HOT161(0x08C14CA4u, func_08C14CA4, Word);
HOT161(0x08C14E20u, func_08C14E20, Word);
// HOT161(0x08C1DB8Cu, func_08C1DB8C, Word);  // no comparable synthetic case yet
// HOT161(0x08C1E9A0u, func_08C1E9A0, Word);  // no comparable synthetic case yet
// HOT161(0x08C29EE4u, func_08C29EE4, Word);  // no comparable synthetic case yet
HOT161(0x08C2CA48u, func_08C2CA48, Word);
// HOT161(0x08C36CE8u, func_08C36CE8, Word);  // no comparable synthetic case yet
HOT161(0x08C36E64u, func_08C36E64, Word);
