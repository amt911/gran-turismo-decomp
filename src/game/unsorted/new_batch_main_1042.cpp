// Batch main-1042: functions drafted from tools.disasm (structured C, hand-written bodies).
//   - 0x088BA628: fills its destination record (a0) from the source record (a1): byte flags, scaled bytes -> floats, then
//     ten 0x08946734 vector calls (each writes four/two/six entries) and one tail call. Straight-line, no branches on floats.
//   - 0x089823F8: a 21-entry state machine (byte at a0+0x206 selects a jump-table case; 7..9 and 13..19 fall through to
//     the tail). Each case reads/writes the floats of the object at 0x1EC..0x23C, some call 0x089A8E8C with a1, and all end
//     in the two 0x08B29034 calls (a0+0x21C with the value at 0x214, a0+0x228 with the value at 0x218).
// Skipped: 0x08B67E04 — 491 instructions of 64-bit (v0/v1 pair) arithmetic that goes through seven helpers
// (0x08B71394, 0x08B71328, 0x08B71A90, 0x08B6E268, 0x08B712B0, 0x08B71720, 0x08B69EBC) with constants from the table at
// 0x08CB6640..0x08CB66C4, normalising against 0x3FEFFFFF/0x3FDFFFFF/0x3E3FFFFF. Without the gate a single mistyped
// operand would surface only in the callee's writes; left for a pass that can compare it.
//
// Input shapes for the synthetic generator: 0x088BA628's destination is 0x2C0 bytes (memset 0x2BC, last vector write at
// 0x2B0+3*4); 0x089823F8's object runs to 0x23C (state at 0x206, floats 0x1EC..0x23C), and its +0x1EC tag is compared with
// 2/3/4/5/0x64. `config/synth_shapes.txt` takes one struct per address, so the other argument registers fall back to the
// generator's own arena layout.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1042 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A nested call: v0 / v1 / f0 are the callee's, as after the `jal`.
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                  std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return callf(rt, ctx, target, arguments).v0;
}

// 0x088BA628 (a0 = destination record, a1 = source record): clears 0x2BC bytes of the destination, copies the three
// leading bytes, then scales the source's bytes into the destination's floats and runs the 0x08946734 vector builders.
void func_088BA628(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    const auto byte = [&](u32 offset) { return static_cast<float>(rt.memory().load8(src + offset)); };
    constexpr float k100 = 100.0f;
    constexpr float kMinus100 = -100.0f;
    constexpr float kTenth = 0.1f;
    constexpr float kHundredth = 0.01f;
    const float inverse36 = std::bit_cast<float>(0x3E8E38E4u);  // the 0x3E8E38E4 constant of the 0x62/0x63 entries

    (void)call(rt, ctx, 0x08B36018u, {dst, 0u, 0x2BCu});
    rt.memory().store8(dst + 0u, rt.memory().load8(src + 0u));
    rt.memory().store8(dst + 1u, rt.memory().load8(src + 1u));
    rt.memory().store8(dst + 2u, rt.memory().load8(src + 2u));
    rt.memory().store8(dst + 9u, 0u);
    put(rt, dst + 0x7Cu, 0u);
    put(rt, dst + 0x84u, 0u);
    put(rt, dst + 0x8Cu, 0u);
    put(rt, dst + 0x88u, 0u);
    put_float(rt, dst + 0x4u, byte(3u) / k100);

    if (rt.memory().load8(src + 0x28u) != 0u) {
        rt.memory().store8(dst + 9u, 1u);
        put_float(rt, dst + 0x7Cu, byte(0x2Bu) * kTenth);
        put_float(rt, dst + 0x84u, byte(0x2Cu) * kTenth);
    }
    u32 flags = rt.memory().load8(dst + 9u);
    if (rt.memory().load8(src + 0x29u) != 0u) {
        flags |= 2u;
        rt.memory().store8(dst + 9u, flags);
    }
    if (rt.memory().load8(src + 0x2Au) != 0u) {
        flags |= 4u;
        rt.memory().store8(dst + 9u, flags);
    }
    if ((flags & 0xFFu) != 0u) {
        put_float(rt, dst + 0x88u, byte(0x2Du) * kTenth);
        put_float(rt, dst + 0x8Cu, byte(0x2Eu) * kTenth);
    }
    const u32 spread = rt.memory().load8(src + 0x30u);
    rt.memory().store8(dst + 0xCu, spread);
    if (spread != 0u) {
        put_float(rt, dst + 0x90u, byte(0x31u) * kHundredth);
        put_float(rt, dst + 0x94u, byte(0x32u) * kHundredth);
    }

    rt.memory().store8(dst + 0xDu, rt.memory().load8(src + 0x34u));
    put_float(rt, dst + 0x98u, byte(0x35u) * kHundredth);
    rt.memory().store8(dst + 0x8u, rt.memory().load8(src + 0x24u));
    put_float(rt, dst + 0x78u, byte(0x25u) * kTenth);
    put_float(rt, dst + 0x14u, byte(0x9u));
    put_float(rt, dst + 0x18u, byte(0xAu));
    put_float(rt, dst + 0x1Cu, byte(0xBu) * kTenth);
    put_float(rt, dst + 0x20u, byte(0xCu) * kTenth);
    put_float(rt, dst + 0x24u, byte(0xDu) * kTenth);
    put_float(rt, dst + 0x28u, byte(0xEu) * kTenth);
    put_float(rt, dst + 0x30u, byte(0x10u) * kTenth);
    put_float(rt, dst + 0x34u, byte(0x11u) * kTenth);
    put_float(rt, dst + 0x38u, byte(0x12u) * kTenth);
    put_float(rt, dst + 0x3Cu, byte(0x13u) * kTenth);
    put_float(rt, dst + 0x40u, byte(0x14u) * kTenth);
    put_float(rt, dst + 0x44u, byte(0x15u) * kTenth);
    put_float(rt, dst + 0x48u, byte(0x16u) * kTenth);
    rt.memory().store8(dst + 0xEu, 0u);
    put_float(rt, dst + 0x60u, byte(0x1Cu) * kHundredth);

    put_float(rt, dst + 0x9Cu, byte(0x39u) / k100);
    put_float(rt, dst + 0xA0u, byte(0x3Au) / k100);
    put_float(rt, dst + 0xA4u, byte(0x3Bu) / kMinus100);
    put_float(rt, dst + 0xA8u, byte(0x3Cu) / kMinus100);
    put_float(rt, dst + 0xACu, byte(0x3Du) / k100);
    put_float(rt, dst + 0xB0u, byte(0x3Eu) / k100);
    (void)call(rt, ctx, 0x08946734u, {dst + 0xB4u, 2u, dst + 0x9Cu, dst + 0xA4u, 0u, 0u});
    (void)call(rt, ctx, 0x08946734u, {dst + 0xC0u, 2u, dst + 0x9Cu, dst + 0xACu, 0u, 0u});

    rt.memory().store8(dst + 0xFu, 0u);
    put_float(rt, dst + 0xCCu, byte(0x40u) / k100);
    put_float(rt, dst + 0xD0u, byte(0x41u) / k100);
    put_float(rt, dst + 0xD4u, byte(0x42u) / kMinus100);
    put_float(rt, dst + 0xD8u, byte(0x43u) / kMinus100);
    put_float(rt, dst + 0xDCu, byte(0x44u) / k100);
    put_float(rt, dst + 0xE0u, byte(0x45u) / k100);
    (void)call(rt, ctx, 0x08946734u, {dst + 0xE4u, 2u, dst + 0xCCu, dst + 0xD4u, 0u, 0u});
    (void)call(rt, ctx, 0x08946734u, {dst + 0xF0u, 2u, dst + 0xCCu, dst + 0xDCu, 0u, 0u});

    put_float(rt, dst + 0xFCu, byte(0x46u) / k100);
    const u32 entries = rt.memory().load8(src + 0x47u);
    for (u32 i = 0u; i < entries; ++i) {
        put_float(rt, dst + 0x100u + 4u * i, byte(0x48u + i) / k100);
        put_float(rt, dst + 0x130u + 4u * i, byte(0x54u + i) / k100);
    }
    (void)call(rt, ctx, 0x08946734u, {dst + 0x160u, entries, dst + 0x100u, dst + 0x130u, 0u, 0u});

    put_float(rt, dst + 0x198u, byte(0x60u) / k100);
    put_float(rt, dst + 0x19Cu, byte(0x61u) / k100);
    put_float(rt, dst + 0x1A0u, byte(0x62u) * inverse36);
    put_float(rt, dst + 0x1A4u, byte(0x63u) * inverse36);
    put_float(rt, dst + 0x1A8u, byte(0x64u) / k100);
    put_float(rt, dst + 0x1ACu, byte(0x65u) / k100);
    for (u32 i = 0u; i < 6u; ++i) {
        put_float(rt, dst + 0x1B0u + 4u * i, byte(0x66u + i) / k100);
        put_float(rt, dst + 0x1C8u + 4u * i, byte(0x6Cu + i) / k100);
        put_float(rt, dst + 0x1E0u + 4u * i, byte(0x72u + i) / k100);
        put_float(rt, dst + 0x1F8u + 4u * i, byte(0x78u + i) / k100);
        put_float(rt, dst + 0x210u + 4u * i, byte(0x7Eu + i) / k100);
    }
    (void)call(rt, ctx, 0x08946734u, {dst + 0x228u, 6u, dst + 0x1B0u, dst + 0x1C8u, 0u, 0u});
    (void)call(rt, ctx, 0x08946734u, {dst + 0x234u, 6u, dst + 0x1B0u, dst + 0x1E0u, 0u, 0u});
    (void)call(rt, ctx, 0x08946734u, {dst + 0x240u, 6u, dst + 0x1B0u, dst + 0x1F8u, 0u, 0u});
    (void)call(rt, ctx, 0x08946734u, {dst + 0x24Cu, 6u, dst + 0x1B0u, dst + 0x210u, 0u, 0u});

    put_float(rt, dst + 0x258u, byte(0x84u) / k100);
    for (u32 i = 0u; i < 4u; ++i) {
        put_float(rt, dst + 0x25Cu + 4u * i, byte(0x88u + i) / k100);
        put_float(rt, dst + 0x26Cu + 4u * i, byte(0x8Cu + i) / k100);
    }
    (void)call(rt, ctx, 0x08946734u, {dst + 0x27Cu, 4u, dst + 0x25Cu, dst + 0x26Cu, 0u, 0u});

    put_float(rt, dst + 0x288u, byte(0x86u) / k100);
    put_float(rt, dst + 0x28Cu, byte(0x87u) / k100);
    for (u32 i = 0u; i < 4u; ++i) {
        put_float(rt, dst + 0x290u + 4u * i, byte(0x90u + i) / k100);
        put_float(rt, dst + 0x2A0u + 4u * i, byte(0x94u + i) / k100);
    }
    tail_call(ctx, 0x08946734u, {dst + 0x2B0u, 4u, dst + 0x290u, dst + 0x2A0u, 0u, 0u});
}

// 0x089823F8 (a0 = object, a1 = argument passed on to the two worker calls, f12/f13/f14 = rates): one step of the object's
// 21-state machine. The byte at +0x206 selects the case (values >= 0x15, including negative bytes, jump straight to the
// tail), +0x1EC is a variant tag, +0x204/+0x1FC are flags, and +0x214/+0x218 decay or grow by the argument's rate.
}  // namespace
}  // namespace new_batch_main_1042
}  // namespace gt::game

#define NEW1042(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1042::name, gt::ReturnKind::kind)
NEW1042(0x088BA628u, func_088BA628, Word);
