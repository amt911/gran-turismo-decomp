// New batch, main session 1926 (draft, unverified): two functions drafted from `tools.disasm`, structured C.
// Both release shared references. A reference is a block whose count sits at +4 (+8 for the second count) and whose
// vtable slots +8 / +0xC are the deleting calls. The `ll`/`sc` counts are modelled as one read-modify-write, the way
// the guest runs them on one thread. v0 is kept as the original leaves it on every path (the last atomic leaves 1, a
// call leaves its answer). No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1926 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// ll/sc add: returns the old word and leaves 1 in v0 (the sc result, then the `addiu v0,1` of the test).
u32 atomic_add(Runtime &rt, AllegrexContext &ctx, u32 address, u32 delta) {
    const u32 old = word(rt, address);
    put(rt, address, old + delta);
    ctx.gpr[2] = 1u;
    return old;
}

// Drops one reference held through a slot. Last reference of the count at +4: call the vtable slot +8, then drop the
// count at +8; if that was the last one too, call the slot +0xC. Nothing happens for a null object.
void drop_reference(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (atomic_add(rt, ctx, object + 4u, 0xFFFFFFFFu) != 1u) return;
    (void)call(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (atomic_add(rt, ctx, object + 8u, 0xFFFFFFFFu) != 1u) return;
    (void)call(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

} // namespace

// 0x08961784: when the flag at +0x48 is set, clears it, runs the virtual at slot +8 of self and, unless that answered
// non-zero, drops the reference at +0x10 (its slots +0xC/+0x10 are cleared first). The other reference, at +0x44,
// is replaced by the one at +0x10 read before the call. Tail-calls 0x08B3457C with self+0x18.
void func_08961784(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 block = self + 0x18u;
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[2] = rt.memory().load8(self + 0x48u);
        if (ctx.gpr[2] != 0u) {
            tail = true;
            (void)call(rt, ctx, 0x08B344E0u, {block});
            ctx.gpr[2] = word(rt, self + 0x0Cu);
            put(rt, self + 0x40u, ctx.gpr[2]);
            const u32 next = word(rt, self + 0x10u);
            const u32 slot = self + 0x44u;
            if (next != word(rt, slot)) {
                if (next != 0u) (void)atomic_add(rt, ctx, next + 4u, 1u);
                drop_reference(rt, ctx, word(rt, slot));
                put(rt, slot, next);
            }
            rt.memory().store8(self + 0x48u, static_cast<std::uint8_t>(0u));
            const u32 vtable = word(rt, self);
            if (call(rt, ctx, word(rt, vtable + 8u), {self}) == 0u) {
                const u32 held = word(rt, self + 0x10u);
                put(rt, self + 0x0Cu, 0u);
                put(rt, self + 0x10u, 0u);
                drop_reference(rt, ctx, held);
            }
        }
    }
    if (tail) tail_call(ctx, 0x08B3457Cu, {block});
}

// 0x088809E8: destructor. Clears the two owned objects at +4 and +8 (virtual slot +0x2C on each), calls 0x08880874
// on self, drops the references at +0x34 and +0x18, stores the vtable 0x08CBF368 at +0x20, then drops the reference
// at +0x10 and tail-calls its slot +0xC when that was the last one. v0 is the vtable word on that tail.

} // namespace new_batch_main_1926
} // namespace gt::game

#define NEW1926(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1926::name, gt::ReturnKind::kind)
NEW1926(0x08961784u, func_08961784, Word);
