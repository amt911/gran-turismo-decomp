// Hot batch 150: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_150 {
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

// store_half(this, value) with the member's offset: the halfword at this + offset = value.
void store_half(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    rt.memory().store16(ctx.gpr[4] + offset, static_cast<std::uint16_t>(ctx.gpr[5]));
}

// call_with_zero(this) with the member's callee: callee(this->+0x14->+4, &zero) (a zero word on the stack); its v0 back.
void call_with_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    const std::uint32_t inner = word(rt, ctx.gpr[4] + 0x14u);
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), 0u);
    ctx.gpr[2] = inner;
    call_set(rt, ctx, callee, {word(rt, inner + 4u), frame.at(0u)});
}

// fill_halves(dest, n, source): n halfwords from dest get *source; a null dest is skipped (dest still advances).
void fill_halves(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t n = ctx.gpr[5], source = ctx.gpr[6];
    std::uint32_t done = 0u;
    if (n != 0u) {
        do {
            ++done;
            if (dest != 0u) {
                const std::uint32_t half = rt.memory().load16(source);
                ctx.gpr[2] = half;
                rt.memory().store16(dest, static_cast<std::uint16_t>(half));
            }
            dest += 2u;
        } while (n != done);
    }
    ctx.gpr[3] = done;
    ctx.gpr[4] = dest;
}

// build_then_apply(this, ...) with the member's builder and applier: builder(&tmp, a1-a3 as they came), then
// applier(this, &tmp); its v0 back.
void build_then_apply(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t build, std::uint32_t apply) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, build, {frame.at(0u)});
    call_set(rt, ctx, apply, {self, frame.at(0u)});
}

// choose_tail(this, a, b, c) with the member's target: target(a, c, b, c) when **this is set, else target(a, b, c, c)
// (tail call; v0 = *this, v1 = **this, t0 = a as the original leaves them).
void choose_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7];
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = word(rt, ctx.gpr[2]);
    ctx.gpr[8] = a;
    if (ctx.gpr[3] != 0u) tail_call(ctx, target, {a, c, b, c});
    else tail_call(ctx, target, {a, b, c, c});
}

// reset_then_base(this) with the member's vtable: *this = vtable; func_08B61BC8(this + 8); func_08B5C280(this) (tail).
void reset_then_base(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = vtable;
    put(rt, self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        call_set(rt, ctx, 0x08B61BC8u, {self + 8u});
    }
    tail_call(ctx, 0x08B5C280u, {self});
}

} // namespace

// Templates of their families (the members are the batch 150 rows below).
void func_089518F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_half(rt, ctx, 0u); }
void func_08A6981C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_zero(rt, ctx, 0x08A9F9CCu); }
void func_08B8A684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_halves(rt, ctx); }
void func_08B4C8E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_then_apply(rt, ctx, 0x08B4C804u, 0x08B4B858u); }
void func_08A17C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { choose_tail(rt, ctx, 0x08966DA4u); }
void func_08C5A5F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_then_base(rt, ctx, 0x08CDB870u); }

// Batch 150 rows (tools.family_rows).
void func_08951904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_half(rt, ctx, 2u); }
void func_08A72E44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_half(rt, ctx, 20u); }
void func_08AAFD04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_half(rt, ctx, 20u); }
void func_08C46DA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_half(rt, ctx, 238u); }
void func_08A69844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_zero(rt, ctx, 0x08A9FCD8u); }
void func_08A698C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_zero(rt, ctx, 0x08A9F9CCu); }
void func_08A698EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_zero(rt, ctx, 0x08A9FCD8u); }
void func_08A6996C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_zero(rt, ctx, 0x08A9FB90u); }
void func_08B8A7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_halves(rt, ctx); }
void func_08C4AE20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_halves(rt, ctx); }
void func_08C4B13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_halves(rt, ctx); }
void func_08C4FD44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_halves(rt, ctx); }
void func_08B4CA2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_then_apply(rt, ctx, 0x08B4C94Cu, 0x08B4B858u); }
void func_08B4CB74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_then_apply(rt, ctx, 0x08B4CA94u, 0x08B4B858u); }
void func_08B4CCBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_then_apply(rt, ctx, 0x08B4CBDCu, 0x08B4B858u); }
void func_08B4CE04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_then_apply(rt, ctx, 0x08B4CD24u, 0x08B4B858u); }
void func_08A17CAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { choose_tail(rt, ctx, 0x0896B7ACu); }
void func_08A17D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { choose_tail(rt, ctx, 0x08967380u); }
void func_08A17D50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { choose_tail(rt, ctx, 0x0896726Cu); }
void func_08A17E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { choose_tail(rt, ctx, 0x089672C8u); }
void func_08C5A648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_then_base(rt, ctx, 0x08CDB870u); }
void func_08C5AA34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_then_base(rt, ctx, 0x08CDB7C0u); }
void func_08C5AA88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_then_base(rt, ctx, 0x08CDB7C0u); }
void func_08C5FAA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_then_base(rt, ctx, 0x08CDB7C0u); }
void func_08C5FAFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_then_base(rt, ctx, 0x08CDB7C0u); }

} // namespace hot_batch_150
} // namespace gt::game

#define HOT150(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_150::name, gt::ReturnKind::kind)
HOT150(0x089518F4u, func_089518F4, Word);
HOT150(0x08A6981Cu, func_08A6981C, Word);
HOT150(0x08B8A684u, func_08B8A684, Word);
HOT150(0x08B4C8E4u, func_08B4C8E4, Word);
HOT150(0x08A17C80u, func_08A17C80, Word);
HOT150(0x08C5A5F4u, func_08C5A5F4, Word);
HOT150(0x08951904u, func_08951904, Word);
HOT150(0x08A72E44u, func_08A72E44, Word);
HOT150(0x08AAFD04u, func_08AAFD04, Word);
HOT150(0x08C46DA4u, func_08C46DA4, Word);
HOT150(0x08A69844u, func_08A69844, Word);
HOT150(0x08A698C4u, func_08A698C4, Word);
HOT150(0x08A698ECu, func_08A698EC, Word);
HOT150(0x08A6996Cu, func_08A6996C, Word);
HOT150(0x08B8A7D0u, func_08B8A7D0, Word);
HOT150(0x08C4AE20u, func_08C4AE20, Word);
HOT150(0x08C4B13Cu, func_08C4B13C, Word);
HOT150(0x08C4FD44u, func_08C4FD44, Word);
HOT150(0x08B4CA2Cu, func_08B4CA2C, Word);
HOT150(0x08B4CB74u, func_08B4CB74, Word);
HOT150(0x08B4CCBCu, func_08B4CCBC, Word);
HOT150(0x08B4CE04u, func_08B4CE04, Word);
HOT150(0x08A17CACu, func_08A17CAC, Word);
HOT150(0x08A17D24u, func_08A17D24, Word);
HOT150(0x08A17D50u, func_08A17D50, Word);
// HOT150(0x08A17E90u, func_08A17E90, Word);  // no comparable synthetic case yet
HOT150(0x08C5A648u, func_08C5A648, Word);
HOT150(0x08C5AA34u, func_08C5AA34, Word);
HOT150(0x08C5AA88u, func_08C5AA88, Word);
HOT150(0x08C5FAA8u, func_08C5FAA8, Word);
HOT150(0x08C5FAFCu, func_08C5FAFC, Word);
