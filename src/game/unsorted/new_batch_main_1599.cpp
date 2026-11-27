// New batch, main session 1599: functions the recompiler emitted that no file had decompiled yet, written by hand as
// structured C from `tools.disasm`. Draft only: the differential gate (fast, sabotage, synthetic) is run by the
// verifier, not here.
//
// Conventions: every `call_set` / `call_setf` writes the argument registers the original sets before the `jal`
// (a0-a3, t0-t3, f12-f15); the delay slot of a `jal` runs BEFORE the call. `v0` is set as the original leaves it on
// every exit path. The dead stack is not reproduced; the frames callees read or write are (`GuestFrame`, size = the
// original's `addiu sp, sp, -N`).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1599 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// Sets the argument registers a0.. and f12.., calls, and leaves v0 / v1 / f0 as the callee returns them.
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_setf(rt, ctx, address, arguments, {}).v0;
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// 0x08AF5830 (a0 = the out matrix, a1 = a position, a2 = the source vector, a3 = the axis vector or 0, t0 = an axis
// order 0..5): builds a translation matrix from a1 at sp + 0x50 (0x08B4E9D4) and the second one at sp + 0x10 from a2
// through 0x08B4B114, then sends the second to the out matrix. With a3 and an order under 6, the three axis setters
// 0x08B4F2E4 (+0), 0x08B4F3B0 (+4) and 0x08B4F47C (+8) follow in that order's permutation (the jump table at
// 0x08C9274C). v0 = the last call's result (or whether the order was in range when a3 is 0).
void func_08AF5830(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 vector = ctx.gpr[6];
    const u32 axis = ctx.gpr[7];
    const u32 order = ctx.gpr[8];
    GuestFrame frame(ctx, 0xB0u);
    const u32 sp = frame.at(0u);
    const float one = 1.0f;
    put(rt, sp + 0x80u, word(rt, position));
    put(rt, sp + 0x84u, word(rt, position + 4u));
    put(rt, sp + 0x88u, word(rt, position + 8u));
    put_float(rt, sp + 0x8Cu, one);
    put_float(rt, sp + 0x50u, one);
    put_float(rt, sp + 0x64u, one);
    put_float(rt, sp + 0x78u, one);
    for (const u32 offset : {0x60u, 0x70u, 0x54u, 0x74u, 0x58u, 0x68u, 0x5Cu, 0x6Cu, 0x7Cu}) put(rt, sp + offset, 0u);
    call_set(rt, ctx, 0x08B4E9D4u, {out, sp + 0x50u});
    put(rt, sp, word(rt, vector));
    put(rt, sp + 4u, word(rt, vector + 4u));
    put(rt, sp + 8u, word(rt, vector + 8u));
    put(rt, sp + 0xCu, word(rt, vector + 0xCu));
    call_setf(rt, ctx, 0x08B4B114u, {sp, sp + 0x10u}, {0.0f, 0.0f, 0.0f});
    call_set(rt, ctx, 0x08B4E9D4u, {out, sp + 0x10u});
    if (axis == 0u) {
        ctx.gpr[2] = order < 6u ? 1u : 0u;
        return;
    }
    if (order >= 6u) {
        ctx.gpr[2] = 0u;
        return;
    }
    static constexpr u32 kSetters[3] = {0x08B4F2E4u, 0x08B4F3B0u, 0x08B4F47Cu};
    static constexpr u32 kOffsets[3] = {0u, 4u, 8u};
    static constexpr unsigned kOrders[6][3] = {{2u, 1u, 0u}, {0u, 2u, 1u}, {1u, 0u, 2u},
                                              {1u, 2u, 0u}, {2u, 0u, 1u}, {0u, 1u, 2u}};
    for (unsigned step = 0; step < 3u; ++step) {
        const unsigned slot = kOrders[order][step];
        call_setf(rt, ctx, kSetters[slot], {out}, {float_at(rt, axis + kOffsets[slot])});
    }
}

// 0x088D51BC (a0 = the holder of the source blob, a1 = the destination): 0x088D47EC gives the blob's size, the
// destination is zeroed (0x08B36018) when that is longer than the 0x116 bytes copied, the blob (the word the holder
// points at) is copied and v0 = destination + size.
void func_088D51BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 size = call_set(rt, ctx, 0x088D47ECu, {holder});
    if (static_cast<s32>(size) > 0x116) call_set(rt, ctx, 0x08B36018u, {destination, 0u, size});
    const u32 source = word(rt, holder);
    for (u32 i = 0; i < 0x116u; ++i) put8(rt, destination + i, byte_u(rt, source + i));
    ctx.gpr[2] = destination + size;
}

}  // namespace
}  // namespace new_batch_main_1599
}  // namespace gt::game

#define MAIN1599(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1599::name, gt::ReturnKind::kind)
MAIN1599(0x08AF5830u, func_08AF5830, Word);
MAIN1599(0x088D51BCu, func_088D51BC, Word);
