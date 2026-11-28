// Main-session batch 1778 (one function, drafted from tools.disasm).
// 0x08869900: the previous draft (batch 1619) failed the gate on RAM 1.1 MB below the entry sp
// (0x09EBDF60 / 0x09EBDFD0 held `this` originally, the global record 0x08D691D8 in the draft) --
// those stores are not in this function's asm: every store here is at +0x1A0..+0x1C4 of a0, and
// every callee receives exactly the register the asm sets, so the body below keeps that order and
// the argument shapes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1778 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

struct Saved {
    AllegrexContext &ctx;
    u32 s0;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~Saved() { ctx.gpr[16] = s0; }
};

// 0x08869900 (a0 = this): the sub-objects at +0x28, +0x74, +0x80, +0x90 are constructed in order
// (the +0x80 one with a1 = a2 = a3 = 0), the word 0x08CD70F8 lands at +0x1A0, the three words at
// +0x1A4/+0x1A8/+0x1AC are zeroed (the last store sits in the 0x08B8EA8C delay slot, which takes
// +0x1A4 and a1 = 8), +0x1B0 is set up by 0x0886A658 and the byte at +0x1C4 becomes 1. v0 = 1.
void func_08869900(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x08B28DCCu, {self});
        call(rt, ctx, 0x088B07B8u, {self + 0x28u});
        call(rt, ctx, 0x0887AF08u, {self + 0x74u});
        call(rt, ctx, 0x08B04D98u, {self + 0x80u, 0u, 0u, 0u});
        call(rt, ctx, 0x0890BD30u, {self + 0x90u});
        put(rt, self + 0x1A0u, 0x08CD70F8u);
        const u32 block = self + 0x1A4u;
        put(rt, block, 0u);
        put(rt, block + 4u, 0u);
        put(rt, block + 8u, 0u);
        call(rt, ctx, 0x08B8EA8Cu, {block, 8u});
        call(rt, ctx, 0x0886A658u, {self + 0x1B0u});
        rt.memory().store8(self + 0x1C4u, static_cast<std::uint8_t>(1u));
        ctx.gpr[2] = 1u;
    }
}

} // namespace
} // namespace new_batch_main_1778
} // namespace gt::game

#define NEW1778(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1778::name, gt::ReturnKind::kind)

NEW1778(0x08869900u, func_08869900, Word);
