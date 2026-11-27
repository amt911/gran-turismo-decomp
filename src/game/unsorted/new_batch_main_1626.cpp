// Main-session batch 1626: drafted functions (structured C from tools.disasm). Draft only, unverified: the gate decides.
// 0x08984608: constructor-shaped init of the object at a0 (vtable word, sub-objects through 0x08964224 / 0x0896418C, a
//   0x20-byte allocation through 0x08A5BC20 whose pointers are stored in the object).
// 0x08A14D94: a list insert/lookup pass on a0 (asserts through 0x08A5BB7C when a pointer is null; the list sentinel is 0x08D74FD0).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1626 {
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

// 0x08984608 (a0 = object): the vtable, then the sub-objects at +0x10, +0x30, +0x50, +0x80 and the pointers at +0xCC / +0xE4.
// v0 on exit = the second allocation + 0x20 (stored at +0xE8 as well).
void func_08984608(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    const u32 sp = ctx.gpr[29];
    put(rt, self, 0x08CCE158u);
    call(rt, ctx, 0x08964224u, {sp + 0x30u, 0xFFD4D4D4u});
    call(rt, ctx, 0x0896418Cu, {self + 0x10u, sp + 0x30u});
    put(rt, self + 0x20u, 0xFFFFFFFFu);
    call(rt, ctx, 0x08B85EB8u, {self + 0x30u, 0x08CA38CCu, sp + 2u});
    put(rt, self + 0x3Cu, 4u);
    put8(rt, self + 0x34u, 1u);
    put8(rt, self + 0x35u, 1u);
    put8(rt, self + 0x38u, 1u);
    put_float(rt, self + 0x40u, 1.0f);
    put_float(rt, self + 0x44u, 1.0f);
    put8(rt, self + 0x36u, 0u);
    put8(rt, self + 0x37u, 0u);
    put8(rt, self + 0x39u, 0u);
    put(rt, self + 0x48u, 0u);
    call(rt, ctx, 0x08964224u, {sp + 0x20u, 0xFF000000u});
    call(rt, ctx, 0x0896418Cu, {self + 0x50u, sp + 0x20u});
    put(rt, self + 0x60u, 0xFFFFFFFFu);
    put(rt, self + 0x70u, 0u);
    call(rt, ctx, 0x08964224u, {sp + 0x10u, 0xFF000000u});
    call(rt, ctx, 0x0896418Cu, {self + 0x80u, sp + 0x10u});
    put(rt, self + 0x90u, 0xFFFFFFFFu);
    put8(rt, self + 0xA9u, 1u);
    put_float(rt, self + 0xC0u, 1.0f);
    put8(rt, self + 0xA8u, 1u);
    put_float(rt, self + 0xBCu, 1.0f);
    put(rt, self + 0xC8u, 0xFFFFFFFFu);
    put(rt, self + 0xA0u, 0u);
    put(rt, self + 0xA4u, 0u);
    put(rt, self + 0xACu, 0u);
    put(rt, self + 0xB0u, 0u);
    put(rt, self + 0xB4u, 0u);
    put(rt, self + 0xB8u, 0u);
    put8(rt, self + 0xC4u, 0u);
    put(rt, self + 0xCCu, 0u);
    put(rt, self + 0xD0u, 0u);
    put(rt, self + 0xD4u, 0u);
    put(rt, sp + 8u, 0u);  // delay slot of the call below
    const u32 first = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    put(rt, self + 0xD4u, first + 0x20u);
    put(rt, self + 0xCCu, first);
    put(rt, self + 0xD0u, first);  // delay slot of the call below
    call(rt, ctx, 0x08BEF0ECu, {first, 8u, sp + 8u, sp + 1u});
    put(rt, self + 0xE4u, 0u);
    put(rt, self + 0xD0u, word(rt, self + 0xCCu) + 0x20u);
    put(rt, self + 0xDCu, 0u);
    put(rt, self + 0xE0u, 0xFFFFFFFFu);
    put(rt, self + 0xD8u, 0u);
    put(rt, self + 0xE8u, 0u);
    put(rt, self + 0xECu, 0u);
    put(rt, sp + 4u, 0u);  // delay slot of the call below
    const u32 second = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    put(rt, self + 0xE4u, second);
    put(rt, self + 0xE8u, second);
    put(rt, self + 0xECu, second + 0x20u);  // delay slot of the call below
    call(rt, ctx, 0x08BEF0ECu, {second, 8u, sp + 4u, sp});
    const u32 result = word(rt, self + 0xE4u) + 0x20u;
    put(rt, self + 0xE8u, result);
    ctx.gpr[2] = result;
}

// 0x08A14D94 (a0 = list owner, a1 = key): the sentinel 0x08D74FD0 ends the list at +0x10D / +0x18 / +0x104 of the owner.
// v0 on exit = the last callee's v0.

}  // namespace
}  // namespace new_batch_main_1626
}  // namespace gt::game

#define NEW1626(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1626::name, gt::ReturnKind::kind)
NEW1626(0x08984608u, func_08984608, Word);
