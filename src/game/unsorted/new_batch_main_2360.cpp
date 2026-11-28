// New batch main 2360: functions drafted by the delegate for the main session (unverified draft; the verifier gates them).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2360 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// --- 184-byte function ------------------------------------------------------------------------------------
// 0x0885DF24 (a0 = object with a member at +0x5B4 and a table at +0x5BC, a1/a2/a3/t0 = the descriptor,
// t1 = a byte): func_08896CD8(a0 + 0x5BC, a3, a1, t0, t1) creates the node; on success its word +4 is a2,
// func_088F9480(a0's +0x5B4) runs with a1 = node, a2 = a1, a3 = t0 still in the registers, func_0889717C(a3)
// gates func_088F8924(node), and the byte at +0x50 is t1. The argument registers the asm leaves set are passed
// explicitly, because the callees read them.
void func_0885DF24(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const u32 flag = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    const u32 node = call(rt, ctx, 0x08896CD8u, {self + 0x5BCu, third, first, fourth, flag});
    if (node == 0u) return;
    put(rt, node + 4u, second);
    call(rt, ctx, 0x088F9480u, {word(rt, self + 0x5B4u), node, first, fourth});
    if (call(rt, ctx, 0x0889717Cu, {third, node, first, fourth}) != 0u) call(rt, ctx, 0x088F8924u, {node});
    rt.memory().store8(node + 0x50u, static_cast<std::uint8_t>(flag));
}

}  // namespace
}  // namespace new_batch_main_2360
}  // namespace gt::game

#define NEW2360(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2360::name, gt::ReturnKind::kind)
NEW2360(0x0885DF24u, func_0885DF24, Word);
