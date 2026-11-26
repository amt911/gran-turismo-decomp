// Main-session batch 1149 (3 functions, draft only, unverified): structured C from tools.disasm.
// Re-drafts of functions that mismatched in batch 1123 / 1128 (fix_info), fixed against the asm:
//   0x08ADB0D8: v0 is the result of the second call (0x08AD8C5C), not of the last one (0x08AEB82C).
//   0x08B1F414: v0 = this+0x10 (addiu before the second call) and the callees leave it; the draft never set it.
//   0x08AFD7B0: v0 = the leaf 0x08BBBEC4 gets this->+0xD8 loaded into v0 first (lw v0,0xD8(s2)); the function ends
//               with the v0 of its last call (0x08C46508), not with the frame slot.
// Left out (cause not found in the asm, see the batch notes): 0x08AB59A8 and 0x08AB62EC.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1149 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 = the callee's v0, as the next instruction of the original sees it.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A guest call whose v0 / v1 / f0 stay in the context like the original's: the next instruction sees them.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with the integer argument registers set in order from a0 (a0-a3, t0-t3).
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// The same with the float argument registers set in order from f12 (f12-f19).
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : floats) {
        if (freg <= 19u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, floats);
}

// 0x08ADB0D8: call 0x08AEB808(); r = call 0x08AD8C5C(0x08D40610, 0); call 0x08AEB82C(); v0 = r.

// 0x08B1F414 (a0 = this): two embedded self-pointers and two calls to 0x08B34DA8 on this+8 and this+0x18, then
// this+0x20 = this+0x24 = 0 and a tail call 0x08B34DA8 on this+0x28. v0 = this+0x10 from before the second call.
void func_08B1F414(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, self + 0x20u);
    put(rt, self + 4u, 0u);
    call(rt, ctx, 0x08B34DA8u, {self + 8u});
    ctx.gpr[2] = self + 0x10u;  // addiu v0, s0, 0x10
    put(rt, self + 0x10u, self + 0x20u);
    put(rt, self + 0x14u, 0u);
    call(rt, ctx, 0x08B34DA8u, {self + 0x18u});
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    tail_call(ctx, 0x08B34DA8u, {self + 0x28u});
}

// 0x08AFD7B0 (a0 = object, f12 / f13 = two scales): 0x08BAC230(object) and 0x08C46020(&object, object + 0x38) only
// hand back their arguments (v0 = the frame slot, v1 = object + 0x38); the first four floats of the object (the first
// three negated) become a rotation (0x08BAC284) whose scaled axis is accumulated into object + 0x80. The magnitude
// from the object's +0xD8 points and that rotation's three floats (0x08BBBEC4) becomes f22; a zero stops here. The
// answer is v0 of the last call the original makes on the path taken (0x08BBBEC4 on a zero magnitude, 0x08C46508
// otherwise): the leaf callees leave v0 as they find it, so the draft keeps every callee's v0.

} // namespace
} // namespace new_batch_main_1149
} // namespace gt::game

#define NEWM1149(address, name) GT_DECOMP(address, gt::game::new_batch_main_1149::name, gt::ReturnKind::Word)
NEWM1149(0x08B1F414u, func_08B1F414);
