// New batch (main session 2069): a redraft of 0x089614D8 from tools.disasm, structured C. Not gate-verified yet.
// The two earlier drafts both wrote +4 from word(+0xC) as the asm says and both mismatched the same single byte
// (self+5, 01 > 04) on the same case: the value they stored never varied, but the virtual the flush calls after it
// saw a different $a1. call_guest discards the callee's registers and hot_common's call_indirect gives back only v0
// (or v1/f0), so $a1 in the decompiled caller stayed the entry 4 instead of the 1 the mutex wrapper leaves, which is
// what the callee received in the original. Here $a1 is carried along the call chain (GuestResult::a1 is documented
// for exactly this) and passed to every virtual, so the callee reads what the asm leaves it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2069 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
void store8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }

// `lw v0, slot(vtable); jalr v0` with a0 = object: the asm leaves $a1 / $a2 from the previous call, so they are passed
// on, and $a1 afterwards is the callee's (a virtual that never writes it keeps it), which the next call receives.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, u32 object, u32 &a1) {
    const GuestResult result = call_indirect(rt, ctx, target, {object, a1, 0u}, {});
    a1 = result.a1;
    return result.v0;
}

// The intrusive release: the strong count at +4 drops by one and, when it was the last, the object's vtable slot +8
// runs; then the weak count at +8 drops and its vtable slot +0xC runs when that was the last.
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object, u32 &a1) {
    if (object == 0u) return;
    const u32 strong = word(rt, object + 4u) - 1u;
    put(rt, object + 4u, strong);
    if (strong != 0u) return;
    (void)call_virtual(rt, ctx, word(rt, word(rt, object) + 8u), object, a1);
    const u32 weak = word(rt, object + 8u) - 1u;
    put(rt, object + 8u, weak);
    if (weak != 0u) return;
    (void)call_virtual(rt, ctx, word(rt, word(rt, object) + 0xCu), object, a1);
}

// 0x089614D8: nothing to do when the dirty byte at +0x14 is 0 (v0 keeps the lbu, i.e. 0). Under the mutex at +0x18 the
// pending pointer at +0xC becomes current at +4, the +0x10 pointer is published over the +0x8 one (a reference for the
// new, a release for the old), the dirty byte clears and the object's vtable slot +8 runs; when that answers 0 the
// pending pointers are dropped, and the mutex is released as a tail call.
void func_089614D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    if (rt.memory().load8(self + 0x14u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    GuestFrame frame(ctx, 0x20u);
    const u32 mutex = self + 0x18u;
    (void)call(rt, ctx, 0x08B344E0u, {mutex});
    u32 a1 = 1u;  // func_08B344E0 sets $a1 = 1 / $a2 = 0 itself before the jump to the lock import
    const u32 current = word(rt, self + 0xCu);
    const u32 next = word(rt, self + 0x10u);
    const u32 published = word(rt, self + 8u);
    put(rt, self + 4u, current);
    if (next != published) {
        if (next != 0u) put(rt, next + 4u, word(rt, next + 4u) + 1u);
        release_object(rt, ctx, published, a1);
        put(rt, self + 8u, next);
    }
    const u32 target = word(rt, word(rt, self) + 8u);
    store8(rt, self + 0x14u, 0u);
    if (call_virtual(rt, ctx, target, self, a1) == 0u) {
        const u32 dropped = word(rt, self + 0x10u);
        put(rt, self + 0xCu, 0u);
        put(rt, self + 0x10u, 0u);
        release_object(rt, ctx, dropped, a1);
    }
    tail_call(ctx, 0x08B3457Cu, {mutex});
}

} // namespace
} // namespace new_batch_main_2069
} // namespace gt::game

#define NEW2069(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2069::name, gt::ReturnKind::kind)
NEW2069(0x089614D8u, func_089614D8, Word);
