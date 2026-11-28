// New batch main-1627 (draft, unverified). Structured C from tools.disasm. Drafted: 0x089B1124, 0x08A8D308.
// Not drafted (asm not read through, too large for this pass): 0x08B758A4 (0x1420 frame, FP save/restore), 0x08C64D48,
// 0x08C67154, 0x08AFF7E0, 0x08C67404, 0x089B14F0, 0x08C67BD8. No old draft commit was named.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1627 {
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

// 0x089B1124 (a0 = object, a1 = flag word): zero-initialises the object's fields, sets the vtable 0x08CBB598 and
// the defaults, calls the sub-initialisers and two allocators (func_331840 = 0x08B35840). v0 = the last allocation.
void func_089B1124(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];

    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self + 0x00u, 0x08CBB598u);
    put(rt, self + 0x14u, arg);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x5Cu, 0xFFFFFFFFu);
    put8(rt, self + 0x60u, 1u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x30u, 0u);
    put8(rt, self + 0x3Cu, 0u);
    for (u32 off = 0x40u; off <= 0x58u; off += 4u) put(rt, self + off, 0u);
    put(rt, self + 0x64u, 0u);
    call(rt, ctx, 0x089A7644u, {self + 0x68u, 0u});

    put(rt, self + 0x70u, 5u);
    put8(rt, self + 0x6Eu, 1u);
    put8(rt, self + 0x6Cu, 0u);
    put8(rt, self + 0x6Du, 0u);
    put(rt, self + 0x74u, 0u);
    put(rt, self + 0x7Cu, 0u);
    put(rt, self + 0x78u, 0u);
    put(rt, self + 0x88u, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x94u, 0u);
    put(rt, self + 0x90u, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, self + 0x8Cu, 0u);

    const float f1 = float_at(rt, 0x08CDE764u);
    const float f0 = float_at(rt, 0x08CDE760u);
    put(rt, self + 0xA4u, 0u);
    put_float(rt, self + 0xACu, f0);
    put_float(rt, self + 0xA8u, f1);
    put(rt, self + 0xA0u, 0u);
    put(rt, self + 0xB4u, 0xFFFFFFFFu);
    put(rt, self + 0xB0u, 0u);
    call(rt, ctx, 0x089B4108u, {self + 0xB8u, self + 0x74u, self + 0x80u, self + 0x8Cu});

    const float half = std::bit_cast<float>(0x3F000000u);
    const float neg22_5 = std::bit_cast<float>(0xC1B40000u);
    const float one = std::bit_cast<float>(0x3F800000u);
    put(rt, self + 0xBCu, 0u);
    put(rt, self + 0xC0u, 0u);
    put(rt, self + 0xC4u, 0u);
    put(rt, self + 0xD4u, 0u);
    put(rt, self + 0xCCu, 0u);
    put(rt, self + 0xD0u, 0u);

    put_float(rt, self + 0xD8u, half);
    put_float(rt, self + 0xDCu, half);
    put_float(rt, self + 0xE0u, neg22_5);
    put(rt, self + 0xC8u, 0u);
    put_float(rt, self + 0xE4u, float_at(rt, 0x08CDE764u) * half);
    put_float(rt, self + 0x110u, std::bit_cast<float>(0x3C888889u));
    put_float(rt, self + 0xE8u, float_at(rt, 0x08CDE760u) * half);
    put(rt, self + 0x108u, 0u);
    put(rt, self + 0x10Cu, 0u);
    put(rt, self + 0x124u, 2u);
    put8(rt, self + 0xF8u, 1u);
    put8(rt, self + 0xF9u, 1u);
    put8(rt, self + 0x120u, 1u);
    put(rt, self + 0xECu, 0u);
    put(rt, self + 0xF0u, 0u);
    put(rt, self + 0xF4u, 0u);
    put8(rt, self + 0xFAu, 0u);
    put8(rt, self + 0xFBu, 0u);
    put8(rt, self + 0xFCu, 0u);
    put8(rt, self + 0xFDu, 0u);
    put(rt, self + 0x114u, 0u);
    put8(rt, self + 0x118u, 0u);
    put(rt, self + 0x11Cu, 0u);

    const u32 t0Block = self + 0x12Cu;
    put(rt, t0Block + 0x8u, 0u);
    put(rt, t0Block + 0x4u, 0u);
    put(rt, t0Block, 0u);
    put8(rt, self + 0x138u, 1u);
    put(rt, self + 0x13Cu, 0u);
    put(rt, self + 0x140u, 0u);

    const u32 v3Block = self + 0x140u;
    put(rt, v3Block + 0xCu, 0u);
    put(rt, v3Block + 0x4u, 0u);
    put(rt, v3Block + 0x8u, 0u);
    put8(rt, self + 0x150u, 0u);
    put8(rt, self + 0x151u, 0u);
    put8(rt, self + 0x152u, 0u);

    const u32 a2Block = self + 0x160u;
    put_float(rt, a2Block + 0x4u, one);
    put_float(rt, a2Block + 0xCu, one);
    put_float(rt, a2Block + 0x8u, one);
    put_float(rt, self + 0x160u, one);
    put(rt, self + 0x170u, 0u);
    put(rt, self + 0x174u, 0u);
    const u32 a3Block = self + 0x178u;
    put(rt, self + 0x178u, a3Block);
    put(rt, a3Block + 0x4u, a3Block);
    put(rt, self + 0x58u, call(rt, ctx, 0x08B35840u, {0x10u, 0x400u}));
    put(rt, self + 0xB0u, call(rt, ctx, 0x08B35840u, {0x10u, 0x1000u}));
    // v0 is the second allocation's result (no epilogue write).
}

// 0x08A8D308 (a0 = this, a1 = list, a2 = pointer to a pointer, a3 = arg, t0 = arg): picks or allocates a node and links it.
// Returns this (v0 = a0 on every exit).

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1627::name, gt::ReturnKind::kind)
NEWM(0x089B1124u, func_089B1124, Word);

} // namespace
} // namespace new_batch_main_1627
} // namespace gt::game
