// Main-session batch 1792 (draft, UNVERIFIED): functions written by hand from `tools.disasm` as structured C, started
// from batch 1618 (commit a4f38afa) and repaired against the asm. Delay slots are read as "before the branch / call",
// `v0` ends as the asm leaves it on every exit path, and every callee-saved register the original keeps a value in
// stays untouched: the work lives in C++ locals, never in ctx.gpr[16..23], so nothing has to be saved or restored.
// No names: none has evidence beyond what its comment says.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1792 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A `jal`: afterwards v0 holds the callee's result, as the instruction after the call sees it.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// 0x0885F22C (a0 = holder, f12 = a value): the word at +0 of a0 is the object; f12 is stored at its +0x4634 (in the
// `bc1t` delay slot, so always) and f20 is 0 when f12 is negative, 4.0 (0x4080) when it is at most 1, and f12 * 4.0
// otherwise (`bc1fl` runs its delay slot only when it does not branch). 0x0885F0B8(holder) then gets f20 + 1.0 at
// +0x18. v0 = that call's result; f20 is the caller's again (restored from the frame).

// 0x08993E5C (a0 = this, a1 = a parameter): 0x08A0D658(this, a1), then vtable 0x08CC89D0 at +0 and the table
// 0x08CC8C28 at +0x38 (stored in the `jal` delay slot, before the callee), the member at +0x120 built by
// 0x08A17B10(self + 0x120, a1) with its word = a1, and words +0x134..+0x14C zeroed (+0x14C first).
// v0 = 0x08CC8C28: 0x08A17B10 is a leaf that never writes v0, so the caller's word survives the call.
void func_08993E5C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    call(rt, ctx, 0x08A0D658u, {self, argument});
    put(rt, self, 0x08CC89D0u);
    put(rt, self + 0x38u, 0x08CC8C28u);
    call(rt, ctx, 0x08A17B10u, {self + 0x120u, argument});
    put(rt, self + 0x14Cu, 0u);
    put(rt, self + 0x134u, 0u);
    put(rt, self + 0x138u, 0u);
    put(rt, self + 0x13Cu, 0u);
    put(rt, self + 0x140u, 0u);
    put(rt, self + 0x144u, 0u);
    put(rt, self + 0x148u, 0u);
    ctx.gpr[2] = 0x08CC8C28u;
}

} // namespace

#define NEW1792(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1792::name, gt::ReturnKind::kind)
NEW1792(0x08993E5Cu, func_08993E5C, Word);

} // namespace new_batch_main_1792
} // namespace gt::game
