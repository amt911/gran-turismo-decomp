// New batch main-66 (draft, unverified): hand-written structured C from `tools.disasm`. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_66 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// s0-s2 are callee-saved: the original keeps its values in them and restores the caller's.
struct SavedS0S2 {
    AllegrexContext &ctx;
    u32 s0, s1, s2;
    explicit SavedS0S2(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]), s2(c.gpr[18]) {}
    ~SavedS0S2() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
        ctx.gpr[18] = s2;
    }
};

// The ll/sc loops are a plain read-modify-write on one core; returns the count before.
u32 bump_down(Runtime &rt, u32 address) {
    const u32 before = word(rt, address);
    put(rt, address, before - 1u);
    return before;
}

// The last strong reference calls the object's vtable slot 2 (+8), the last weak one slot 3 (+0xC). v0 = 1 (the sc flag) when no call is made.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object) {
    ctx.gpr[2] = 1u;
    if (bump_down(rt, object + 4u) != 1u) return;
    call(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    ctx.gpr[2] = 1u;
    if (bump_down(rt, object + 8u) != 1u) return;
    call(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// One half of func_08873ADC: `holder` is the double-buffer owner (this[0xC] or this[0x14]); returns false when nothing was done.
// The buffer selected by byte 0x25 holds two (strong+weak counted) slots at +0x10 + 8 * k: slot k has the object at +8.
// A pending (other) buffer whose word at +0x18 has bit 0 set is swapped in: with bit 1 set the other slot is emptied, otherwise the
// current one is emptied and byte 0x25 flips; the owner's word at +0x24 then loses bit 16.
bool swap_in(Runtime &rt, AllegrexContext &ctx, u32 this_holder_field_address, u32 owner) {
    // owner = *(this + field); its half at +0x26 bit 0 says a buffer is pending.
    if ((rt.memory().load16(owner + 0x26u) & 1u) == 0u) return false;
    const u32 selector = rt.memory().load8(owner + 0x25u);
    const u32 other = word(rt, owner + ((1u - selector) << 3) + 0x14u);
    if (other == 0u) return false;
    const u32 flags = word(rt, other + 0x18u);
    if ((flags & 1u) == 0u) return false;
    const bool take_other = (flags & 2u) != 0u;
    const u32 slot = owner + ((take_other ? 1u - selector : selector) << 3) + 0x10u;
    const u32 object = word(rt, slot + 8u);
    put(rt, slot + 4u, 0u);
    put(rt, slot + 8u, 0u);
    if (object != 0u) release_counted(rt, ctx, object);
    if (!take_other) {
        const u32 again = word(rt, this_holder_field_address);
        rt.memory().store8(again + 0x25u, 1u - rt.memory().load8(again + 0x25u));
    }
    return true;
}

void func_08873ADC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    bool tail = false;
    {
        SavedS0S2 saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self;
        ctx.gpr[18] = argument;
        if (swap_in(rt, ctx, self + 0xCu, word(rt, self + 0xCu))) {
            const u32 owner = word(rt, self + 0xCu);
            put(rt, owner + 0x24u, word(rt, owner + 0x24u) & ~(1u << 16));
            call(rt, ctx, 0x0886BAF8u, {argument});
        }
        const u32 owner = word(rt, self + 0x14u);
        if (swap_in(rt, ctx, self + 0x14u, owner)) {
            put(rt, owner + 0x24u, word(rt, owner + 0x24u) & ~(1u << 16));
            tail = true;
        } else {
            // The original reaches its return with v0 = 1 when the pending bit was clear (the delay slot of that branch), else 0.
            ctx.gpr[2] = ((rt.memory().load16(owner + 0x26u) & 1u) == 0u) ? 1u : 0u;
        }
    }
    if (tail) tail_call(ctx, 0x0886BAF8u, {argument});
}

} // namespace
} // namespace new_batch_main_66
} // namespace gt::game

#define NEW66(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_66::name, gt::ReturnKind::kind)
NEW66(0x08873ADCu, func_08873ADC, Word);
