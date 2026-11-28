// New batch (main session 2277): functions that mismatched the original in the gate and are drafted again from
// tools.disasm. Draft-only: not gate-verified yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2277 {
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

// What the loader leaves in a register whose `lui` was relocated to the high half of `address` (the carry of a low
// half at or above 0x8000 folded in).
u32 lui_of(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// 0x08B20C00(): walks the list at the global 0x08D74F44 (the relocated `lui` leaves v0 = 0x08D70000 when the list is
// empty). Each node's +4 is an object whose virtual at +8 runs with the caller's a0 and with v0 = that object, not the
// target: the asm is `jalr $v1`, so a virtual that leaves v0 alone answers the object.
void func_08B20C00(Runtime &rt, AllegrexContext &ctx) {
    u32 node = word(rt, 0x08D74F44u);
    if (node == 0u) {
        ctx.gpr[2] = lui_of(0x08D74F44u);
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    for (;;) {
        const u32 body = word(rt, node + 4u);
        const u32 entry = word(rt, body + 8u);
        ctx.gpr[2] = body;
        ctx.gpr[3] = entry;
        const GuestResult result = call_guest(rt, ctx, entry, {ctx.gpr[4]}, {});
        ctx.gpr[2] = result.v0;
        ctx.gpr[3] = result.v1;
        ctx.fpr[0] = result.f0;
        node = word(rt, node);
        if (node == 0u) break;
    }
}

} // namespace
} // namespace new_batch_main_2277
} // namespace gt::game

#define NEW2277(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2277::name, gt::ReturnKind::kind)
NEW2277(0x08B20C00u, func_08B20C00, Word);
