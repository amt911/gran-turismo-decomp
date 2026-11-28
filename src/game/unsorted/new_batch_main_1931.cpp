// New batch main 1931 (main-session draft, unverified): structured C from tools.disasm.
// The gate has not run on these; the synthetic gate and sabotage decide what stays.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1931 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// 0x089344D8 (254 insns, frame 0x100). a0 is a record whose first two words are a 64-bit key.
// The key is resolved through 0x089363FC / 0x08936584 / 0x089368DC into the frame, and the record's
// words are then compared with the resolved copy. Result: 0 when the key is all-ones or the final
// lookup fails; 1 on the first mismatching word; otherwise the byte at +0x15E (nonzero gives 1).
void func_089344D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x100u);

    // First resolution: writes the 64-bit result to the frame at +0 (t0 = sp).
    const u32 first = call(rt, ctx, 0x089363FCu, {record});
    ctx.gpr[8] = frame.at(0u);
    call(rt, ctx, 0x08936584u, {first, a1, word(rt, record), word(rt, record + 4u)});
    const u32 key_lo = word(rt, frame.at(0u));
    const u32 key_hi = word(rt, frame.at(4u));
    if (key_lo == 0xFFFFFFFFu && key_hi == 0xFFFFFFFFu) {
        ctx.gpr[2] = 0u;
        return;
    }

    // Second resolution: 0x089368DC must answer nonzero.
    const u32 second = call(rt, ctx, 0x089363FCu, {first});
    const u32 found = call(rt, ctx, 0x089368DCu, {second, a1, key_lo, key_hi});
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }

    // Third resolution: writes its 64-bit result to the frame at +0x20 (t0 = sp + 0x20).
    const u32 third = call(rt, ctx, 0x089363FCu, {second});
    ctx.gpr[8] = frame.at(0x20u);
    call(rt, ctx, 0x08936584u, {third, a1, key_lo, key_hi});

    // (record offset, frame offset) pairs: the record's word must equal the resolved copy's word.
    static const u32 kPairs[][2] = {
        {0x18u, 0x20u},   {0x1Cu, 0x24u},   {0x20u, 0x28u},   {0x24u, 0x2Cu},   {0x28u, 0x48u},   {0x2Cu, 0x4Cu},
        {0x30u, 0x78u},   {0x34u, 0x7Cu},   {0x38u, 0x68u},   {0x3Cu, 0x6Cu},   {0x40u, 0x70u},   {0x44u, 0x74u},
        {0x48u, 0x30u},   {0x4Cu, 0x34u},   {0x50u, 0xD8u},   {0x54u, 0xDCu},   {0x68u, 0x60u},   {0x6Cu, 0x64u},
        {0x70u, 0x58u},   {0x74u, 0x5Cu},   {0x78u, 0x50u},   {0x7Cu, 0x54u},   {0x80u, 0x90u},   {0x84u, 0x94u},
        {0x88u, 0x98u},   {0x8Cu, 0x9Cu},   {0x90u, 0xA0u},   {0x94u, 0xA4u},   {0x98u, 0xA8u},   {0x9Cu, 0xACu},
        {0xA0u, 0x80u},   {0xA4u, 0x84u},   {0xA8u, 0x88u},   {0xACu, 0x8Cu},   {0xB0u, 0xC8u},   {0xB4u, 0xCCu},
        {0xB8u, 0xC0u},   {0xBCu, 0xC4u},   {0xC0u, 0xD0u},   {0xC4u, 0xD4u},   {0xC8u, 0xB8u},   {0xCCu, 0xBCu},
        {0xD0u, 0xB0u},   {0xD4u, 0xB4u},   {0xD8u, 0x38u},   {0xDCu, 0x3Cu},   {0xE0u, 0x40u},   {0xE4u, 0x44u},
    };
    for (const auto &pair : kPairs) {
        if (word(rt, record + pair[0]) != word(rt, frame.at(pair[1]))) {
            ctx.gpr[2] = 1u;
            return;
        }
    }

    // The tail chain: record +0xF0 must be -1 and each later word must equal the one before it.
    u32 previous = word(rt, record + 0xF0u);
    if (previous != 0xFFFFFFFFu) {
        ctx.gpr[2] = 1u;
        return;
    }
    for (const u32 offset : {0xF4u, 0xF8u, 0xFCu, 0x100u, 0x104u}) {
        const u32 value = word(rt, record + offset);
        if (value != previous) {
            ctx.gpr[2] = 1u;
            return;
        }
        previous = value;
    }
    ctx.gpr[2] = rt.memory().load8(record + 0x15Eu) != 0u ? 1u : 0u;
}

} // namespace
} // namespace new_batch_main_1931
} // namespace gt::game

#define NEW1931(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1931::name, gt::ReturnKind::kind)
NEW1931(0x089344D8u, func_089344D8, Word);
