// New batch, main session 18: the two functions of this batch's address list that no `src/` file defines, written as
// structured C by hand from `tools.disasm`. No names: none has evidence yet. Not gated yet (draft).
//
// The other 78 addresses of the list are not here:
//   75 already have a hand-written body in `src/game/unsorted/hot_batch_*.cpp` (one of them, 08AEC61C, in
//      `hot_leaves.cpp`), emitter style with the registration commented out, so writing them again would only add a
//      second body for the same address;
//   088187AC is `MCalendar_dayToWeekDay` in `src/game/calendar/calendar.cpp` (registration commented out there);
//   08B63AF8 is skipped: it is a C++ exception-cleanup fragment whose branches leave the recorded 220-byte window
//      (targets 08B63BDC / 08B63BE4) and whose last `jal 0x08B62260` has no epilogue after it — the real function is
//      longer than the size the batch was built with;
//   08B61E98 is skipped: its allocation-failure path calls 0x08B6459C, which loads the handler at 0x08CE0E54 and calls
//      it forever if it returns (a fatal abort). A synthetic case with a large a0 reaches it, and the harness cannot
//      survive that.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_18 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does (FINDINGS, batch 178).
// func_08C78868 guards its own `div` with `break 0, 7`, which the original never survives; the trap is not modelled.
void divide(AllegrexContext &ctx, u32 dividend, u32 divisor) {
    const auto a = static_cast<s32>(dividend);
    const auto b = static_cast<s32>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = 0u - dividend; ctx.hi = 0u; return; }
    ctx.lo = static_cast<u32>(a / b);
    ctx.hi = static_cast<u32>(a % b);
}

// 0x08B01070 (a0 = self, a1 = container): a walk of the container's array of 84-byte records. self+4 (signed half) is
// the index of the first record, `word(container+0x60) + 84 * index`; while that record is non-zero its own +0x52
// (signed half, negative = none) names the next one, read before func_08B014BC runs, and the callee (which takes
// container, record only; the asm leaves a2 = 84 * next and a3 = next in place) rewrites record+0x52 from the
// container's +0x6C. When the walk runs out self+4 = -1 and v0 = -1; an index below zero leaves v0 = that index and
// writes nothing. The eleven stack words the original builds at sp+4..sp+0x2C are dead (the callee never reads them)
// and one of them is uninitialised stack.
void func_08B01070(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 container = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const s32 first = signed_half(rt, self + 4u);
    if (first < 0) {
        ctx.gpr[2] = static_cast<u32>(first);
        return;
    }
    u32 record = word(rt, container + 0x60u) + 84u * static_cast<u32>(first);
    while (record != 0u) {
        const s32 next = signed_half(rt, record + 0x52u);
        u32 following = 0u;
        if (next >= 0) following = word(rt, container + 0x60u) + 84u * static_cast<u32>(next);
        call(rt, ctx, 0x08B014BCu, {container, record, 84u * static_cast<u32>(next), static_cast<u32>(next)});
        record = following;
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    put16(rt, self + 4u, 0xFFFFFFFFu);
}

// 0x08C78868 (a0 = self): a signed delta, or -1. Unless self+0x2C has bit 3 the answer is -1; otherwise the monitor at
// self+0x24 is entered through 0x08B61534 and a zero answer is -1 too. The delta starts as self+0xC - self+8 (both
// words, wrapping). self+0x90 must hold an object (without it 0x08B5A9FC builds a 4-byte exception object and throws
// it, with a0 = 0 as the branch leaves it, and never returns); its virtual +0x14 runs and a negative
// answer returns the delta, then its virtual +0x20 divides 0x08B61A08(self+0x24) by that answer and delta + quotient
// is the result. The second virtual reloads self+0x90, as the asm does.
void func_08C78868(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if ((word(rt, self + 0x2Cu) & 8u) == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 monitor = self + 0x24u;
    if (call(rt, ctx, 0x08B61534u, {monitor}) == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 delta = word(rt, self + 0xCu) - word(rt, self + 0x8u);
    const u32 object = word(rt, self + 0x90u);
    if (object == 0u) {
        (void)call(rt, ctx, 0x08B5A9FCu, {0u});  // does not return in the original
        return;
    }
    if (static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, object) + 0x14u), {object})) < 0) {
        ctx.gpr[2] = delta;
        return;
    }
    const u32 amount = call(rt, ctx, 0x08B61A08u, {monitor});
    const u32 target = word(rt, self + 0x90u);
    const u32 divisor = call_indirect(rt, ctx, word(rt, word(rt, target) + 0x20u), {target});
    divide(ctx, amount, divisor);
    ctx.gpr[2] = delta + ctx.lo;
}

} // namespace
} // namespace new_batch_main_18
} // namespace gt::game

#define NEW18(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_18::name, gt::ReturnKind::kind)
NEW18(0x08B01070u, func_08B01070, Word);
NEW18(0x08C78868u, func_08C78868, Word);
