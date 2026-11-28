// New batch (main session 2396): one function requeued after it mismatched the gate, written as structured C from
// `tools.disasm`. Draft only: the differential gate decides whether it lands.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2396 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0, v1, f0 and a1 hold what the callee left, as after the original's `jal` (the next
// call can read the a1 a callee leaves behind).
u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
// The same for a `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address.
u32 call_target(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_keep(rt, ctx, target, arguments);
}

// 0x089BA420 (a0 = object, a1 = a second object read through its vtable): when the object's virtual +0x178 answers
// non-zero, the second object's word +0x17C is called with (a1 = the second object's vtable word, the virtual's
// second answer); then the object's virtual +0x16C, likewise, ends in a tail call of the second object's +0x170
// slot. The second `+0x178` / `+0x16C` call is reached with a1 = `lw a1, 0x0(s2)` (the vtable word), not the object;
// and the `jalr $s0` handler call runs with v0 and a1 both the virtual's answer, not v0 = the target.
void func_089BA420(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (call_target(rt, ctx, word(rt, word(rt, self) + 0x178u), {self, other}) != 0u) {
        const u32 other_vtable = word(rt, other);
        const u32 handler = word(rt, other_vtable + 0x17Cu);
        const u32 argument = call_target(rt, ctx, word(rt, word(rt, self) + 0x178u), {self, other_vtable});
        call_keep(rt, ctx, handler, {other, argument});
    }
    if (call_target(rt, ctx, word(rt, word(rt, self) + 0x16Cu), {self}) != 0u) {
        const u32 other_vtable = word(rt, other);
        const u32 handler = word(rt, other_vtable + 0x170u);
        const u32 argument = call_target(rt, ctx, word(rt, word(rt, self) + 0x16Cu), {self, other_vtable});
        tail_call(ctx, handler, {other, argument});
    }
}

} // namespace
} // namespace new_batch_main_2396
} // namespace gt::game

#define NEW2396(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2396::name, gt::ReturnKind::kind)
NEW2396(0x089BA420u, func_089BA420, Word);
