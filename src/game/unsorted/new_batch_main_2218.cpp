// Main-session batch 2218 (fix pass over a batch-2091 draft that mismatched at RAM 0x092B95xx).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2218 {
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

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// Build the object in a0 (vtable + two intrusive sub-lists), hand the two heap-built 0x38-byte list holders to
// func_08A76AB4, copy eight words out of the global 0x08C8CC74 into the object, then walk the global node chain
// returned by func_0897D4AC(0) (head at 0x08CF40A0) calling each node's virtual at +4; v0 is the last such call's.
//
// The old draft differed in two delay slots: the pointer stored at self+0x3C / +0x40 is s1, captured in the *delay*
// slot of the func_08A7E914 call (so it is the 0x38-byte allocation, n1/n2), not the constructor's return (that is
// object+0x20 because func_08A7E914 ends with `addiu v0, s0, 0x20`); and the chain's `lw v0, 0x4(node); jalr v0` must
// start the callee with v0 = its own address, so it goes through call_indirect, not a plain call.
void func_089C9834(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    const u32 self = ctx.gpr[4];
    const u32 s2 = sp + 0x8u;

    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBB888u);
    const u32 v1 = self + 0x14u;
    const u32 a3 = self + 0x18u;
    const u32 a2 = self + 0x30u;
    put(rt, v1 + 0x10u, a3);
    put(rt, v1 + 0xCu, a3);
    put(rt, v1 + 0x14u, 0u);
    put(rt, v1 + 0x4u, 0u);
    put(rt, v1 + 0x8u, 0u);
    put(rt, self + 0x30u, a2);
    put(rt, self + 0x2Cu, 0u);
    put(rt, a2 + 0x4u, a2);  // delay slot of the call below
    call(rt, ctx, 0x08977AE4u, {self + 0x38u, 0x08CF6ED8u});

    call(rt, ctx, 0x08A69B3Cu, {s2, 0x08CA63ACu});
    const u32 n1 = call(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    call(rt, ctx, 0x08A7E914u, {n1, s2});
    put(rt, sp + 0xCu, n1);  // delay slot of the call below; s1 = the allocation, set before the ctor ran
    call(rt, ctx, 0x08A76AB4u, {self + 0x3Cu, sp + 0xCu});
    call(rt, ctx, 0x08A69D68u, {s2});

    call(rt, ctx, 0x08A69B3Cu, {sp, 0x08CA63BCu});
    const u32 n2 = call(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    call(rt, ctx, 0x08A7E914u, {n2, sp});
    put(rt, sp + 0x4u, n2);  // delay slot of the call below
    call(rt, ctx, 0x08A76AB4u, {self + 0x40u, sp + 0x4u});
    call(rt, ctx, 0x08A69D68u, {sp});

    const u32 g = 0x08C8CC74u;
    const u32 t_a2 = word(rt, g + 0x1Cu);
    const u32 t_a3 = word(rt, g);
    const u32 t_t0 = word(rt, g + 0x4u);
    const u32 t_t1 = word(rt, g + 0x8u);
    const u32 t_t2 = word(rt, g + 0xCu);
    const u32 t_t3 = word(rt, g + 0x10u);
    const u32 t_t4 = word(rt, g + 0x14u);
    const u32 t_t5 = word(rt, g + 0x18u);
    const u32 block = self + 0x48u;
    const u32 list = self + 0x4Cu;
    put8(rt, self + 0x46u, 1u);
    put8(rt, self + 0x44u, 0u);
    put8(rt, self + 0x45u, 0u);
    put8(rt, self + 0x47u, 0u);
    put(rt, block + 0x10u, list);
    put(rt, block + 0x14u, 0u);
    put(rt, block + 0x4u, 0u);
    put(rt, block + 0x8u, 0u);
    put(rt, block + 0xCu, list);
    put(rt, self + 0x7Cu, t_a2);
    put(rt, self + 0x60u, t_a3);
    put(rt, self + 0x64u, t_t0);
    put(rt, self + 0x68u, t_t1);
    put(rt, self + 0x6Cu, t_t2);
    put(rt, self + 0x70u, t_t3);
    put(rt, self + 0x74u, t_t4);
    put(rt, self + 0x78u, t_t5);

    u32 node = call(rt, ctx, 0x0897D4ACu, {0u});
    while (node != 0u) {
        call_indirect(rt, ctx, word(rt, node + 0x4u), {});
        node = word(rt, node + 0xCu);
    }
}

} // namespace
} // namespace new_batch_main_2218
} // namespace gt::game

#define NEW2218(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2218::name, gt::ReturnKind::kind)
NEW2218(0x089C9834u, func_089C9834, Word);
