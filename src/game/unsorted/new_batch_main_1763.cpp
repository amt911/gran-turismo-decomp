// New batch main-1763 (main-session batch 1763). Structured C from tools.disasm, drafted by a fast model, unverified:
// the scripts verify it later. 0x08A63A6C is a redraft of the batch-1621 body, which mismatched in the gate: the calls now
// carry back v1 and f0 (a1 too, as the callee leaves it) instead of only v0, the loop-condition delay slot's a1 = self
// reaches the element virtuals, and the indirect calls leave v0 = the element's vtable, v1 = the slot's target, as the asm's
// `lw v0, 0(a0); lw v1, N(v0); jalr v1` does (0x08A63A6C 240 bytes).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1763 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A call with the registers as they stand: v0 / v1 / f0 back as the callee left them, and a1 too (the next callee reads
// it when the asm does not set it again).
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}

// 0x08A63A6C (a0 = self): destructor. Stores the vtable 0x08CCB980, then for as long as the vector at +0x24 (begin) / +0x28
// (end) is not empty takes the last element, runs its virtual +0xC and then +4 (both with a1 = self from the loop-condition
// delay slot, the second with a1 as the first left it) and steps the end back by 4. Then releases *(self+0x3C) through
// 0x08A72B94, the smart pointers at +0x54 and +0x4C, destroys the range with 0x08C1FBC4 (the frame as its scratch), frees
// the vector storage (0x08A5BC54 with the byte size when the begin is not null), releases *(self+0x1C) and finishes with
// 0x08A780B0(self). v0 = that last call's return.
void func_08A63A6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 vector = self + 0x24u;
    put(rt, self, 0x08CCB980u);
    for (;;) {
        const u32 end = word(rt, vector + 4u);
        const u32 remaining = static_cast<u32>(static_cast<s32>(end - word(rt, vector)) >> 2);
        ctx.gpr[3] = end;    // v1: the loop head's load of the end pointer
        ctx.gpr[2] = remaining;
        ctx.gpr[5] = self;   // a1: the branch delay slot, live for the virtuals and the call after the loop
        if (remaining == 0u) break;
        const u32 element = word(rt, end - 4u);
        const u32 table = word(rt, element);
        const u32 destroy = word(rt, table + 0xCu);
        ctx.gpr[2] = table;
        ctx.gpr[3] = destroy;
        const GuestResult destroyed = call_full(rt, ctx, destroy, {element});
        const u32 second_table = word(rt, element);
        const u32 release = word(rt, second_table + 4u);
        ctx.gpr[5] = destroyed.a1;
        ctx.gpr[2] = second_table;
        ctx.gpr[3] = release;
        call_full(rt, ctx, release, {element});
        put(rt, vector + 4u, word(rt, vector + 4u) - 4u);
    }
    const u32 owned = word(rt, self + 0x3Cu);
    call_full(rt, ctx, 0x08A72B94u, {owned, self});
    call_full(rt, ctx, 0x08A5B9ECu, {self + 0x54u});
    call_full(rt, ctx, 0x08A5B9ECu, {self + 0x4Cu});
    call_full(rt, ctx, 0x08C1FBC4u, {word(rt, self + 0x24u), word(rt, vector + 4u), frame.at(0u)});
    const u32 base = word(rt, self + 0x24u);
    const u32 count = static_cast<u32>(static_cast<s32>(word(rt, vector + 8u) - base) >> 2);
    ctx.gpr[2] = count;
    if (base != 0u) {
        ctx.gpr[5] = count << 2;
        call_full(rt, ctx, 0x08A5BC54u, {base, count << 2, 4u});
    }
    const u32 buffer = word(rt, self + 0x1Cu);
    if (buffer != 0u) call_full(rt, ctx, 0x08A5B930u, {buffer});
    call_full(rt, ctx, 0x08A780B0u, {self});
}

}  // namespace
}  // namespace new_batch_main_1763
}  // namespace gt::game

#define NEW1763(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1763::name, gt::ReturnKind::kind)
NEW1763(0x08A63A6Cu, func_08A63A6C, Word);
