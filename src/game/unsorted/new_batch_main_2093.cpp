// New batch main 2093: re-drafts of three functions the gate mismatched in batches 1128, 1927 and 1928, written from
// `tools.disasm` (main session, draft only, not verified by the differential gate yet).
//
// What changed from the old drafts is the return value on each exit path: v0 is the callee's v0 when the last thing
// before the exit is a call (call_full leaves the callee's v0 / v1 / f0 in the context), or the value the original
// computed last. The delay slot of a `jal` / branch runs before the call / branch; a branch-likely's delay slot runs
// only when the branch is taken. Callee-saved registers are never written here (locals stand in), so they keep the
// caller's values like the original's restored ones. Structured C: named locals, `G[n]` only at the call boundary.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2093 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose v0 / v1 / f0 stay in the context as the callee left them (the original's `jal` then reads them).
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).f0;
}
s32 signed_of(u32 value) { return static_cast<s32>(value); }
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------
// 0x08AFD7B0 (a0 = object, f12 / f13 = two scales): 0x08BAC230(object) and 0x08C46020(&object, object + 0x38) only
// hand back their arguments (v0 = the frame slot, v1 = object + 0x38); the first four floats of the object (the first
// three negated) become a rotation (0x08BAC284) whose scaled axis is accumulated into object + 0x80. The magnitude
// from the object's +0xD8 points and that rotation's three floats (0x08BBBEC4) becomes f22; a zero stops here and
// v0 is that call's v0. The working path ends on 0x08C46508's v0.

// ---------------------------------------------------------------------------------------------------------------
// 0x0887C0A8 (a0 = state, a1 = owner): two 0..1 lookups from owner+0x60's +0xF0 table become the floats at +8 / +0xC;
// then owner+0x60's +0xF4 word is diffed against the state's old +0x18 (changed bits at +0x20, shared at +0x1C), the
// per-bit counters at +0x34..+0x40 are bumped, and two deltas are squared, clamped to 0.25 and signed into the
// floats at +0x10 / +0x14. v0 ends as the second delta: the original's `movz` sign is -1 on the first pass and +1 on
// the second, and v0 is the `mflo` product of that sign and (high - low).
void func_0887C0A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 table = word(rt, word(rt, owner + 0x60u) + 0xF0u);
    const u32 first = call(rt, ctx, 0x08B21590u, {table, 0u});
    put_float(rt, self + 0x08u, call_float(rt, ctx, 0x0887BF80u, {first, 0x7Fu, 0x7Fu, 0x28u}));
    const u32 second = call(rt, ctx, 0x08B21590u, {table, 1u});
    put_float(rt, self + 0x0Cu, call_float(rt, ctx, 0x0887BF80u, {second, 0x7Fu, 0x7Fu, 0x28u}));
    const u32 previous = word(rt, self + 0x18u);
    const u32 flags = call(rt, ctx, 0x08B214D8u, {word(rt, word(rt, owner + 0x60u) + 0xF4u), 0u});
    const u32 changed = previous ^ flags;
    const u32 shared = flags & changed;
    put(rt, self + 0x20u, changed & ~flags);
    put(rt, self + 0x18u, flags);
    put(rt, self + 0x1Cu, shared);
    put(rt, self + 0x24u, 0u);
    // Three flag words: a set bit for either side forces the slot to 0x1C and marks the word at +0x24; a slot that only
    // this side owns counts down to zero and is then set to 9.
    for (u32 i = 0u; i < 3u; ++i) {
        const u32 mask = word(rt, 0x08C8983Cu + 4u * i);
        const u32 own = mask & flags;
        if ((shared & mask) != 0u) {
            put(rt, self + 0x24u, word(rt, self + 0x24u) | own);
            put(rt, self + 0x28u + 4u * i, 0x1Cu);
        } else if (own != 0u) {
            const u32 left = word(rt, self + 0x28u + 4u * i) - 1u;
            put(rt, self + 0x28u + 4u * i, left);
            if (signed_of(left) <= 0) {
                put(rt, self + 0x24u, word(rt, self + 0x24u) | own);
                put(rt, self + 0x28u + 4u * i, 9u);
            }
        }
    }
    // Bits 4..7 of the flags: bump the counter at +0x34..+0x40, capped at 60 (signed `min`).
    for (u32 bit = 4u; bit < 8u; ++bit) {
        const u32 address = self + 0x34u + 4u * (bit - 4u);
        u32 value = 0u;
        if ((flags & (1u << bit)) != 0u) {
            const s32 bumped = signed_of(word(rt, address) + 1u);
            value = static_cast<u32>(std::min(bumped, 0x3C));
        }
        put(rt, address, value);
    }
    // Two deltas of the counter pairs: the first is low - high, the second high - low (the movz sign); squared,
    // clamped to 0.25 from below and given the delta's sign.
    s32 last = 0;
    for (u32 i = 0u; i < 2u; ++i) {
        const u32 high = word(rt, self + 0x40u - 4u * i);
        const u32 low = word(rt, self + 0x38u - 4u * i);
        const s32 raw = signed_of(high - low);
        const s32 delta = (i == 0u) ? -raw : raw;
        last = delta;
        put(rt, self + 0x10u + 4u * i, 0u);
        if (delta != 0) {
            float value = static_cast<float>(delta) / 60.0f;
            value = psp_mul(value, value);
            if (value < 0.25f) value = 0.25f;
            put_float(rt, self + 0x10u + 4u * i, delta < 0 ? -value : value);
        }
    }
    ctx.gpr[2] = static_cast<u32>(last);
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08950438 (a0 = self, a1 / a2 / a3 = flag bytes, t0 = state word): reads the block at (self + 0xC) + 0x270, runs
// func_08949E8C(self) (its v0 is the time), func_0895534C (count), func_08955960 (a float) and func_0895596C (another
// float), then sets the state word and picks the flag bytes from time windows of 1000 / 300 / 500 / 1000 units and the
// two floats (0.2f / 0.7f). The original returns 1 on every path (v0 is set to 1 before each exit).
void func_08950438(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag_a = ctx.gpr[5];
    const u32 flag_b = ctx.gpr[6];
    const u32 flag_c = ctx.gpr[7];
    const u32 state = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);

    const u32 block = word(rt, self + 0x0Cu);
    const GuestResult updated = call_full(rt, ctx, 0x08949E8Cu, {self});
    const u32 base = block + 0x270u;
    const u32 mode = byte_u(rt, base + 0x10u);
    const u32 time = updated.v0;
    const GuestResult counted = call_full(rt, ctx, 0x0895534Cu, {self});
    const u32 count = counted.v0;
    const GuestResult first_float = call_full(rt, ctx, 0x08955960u, {self});
    const GuestResult second_float = call_full(rt, ctx, 0x0895596Cu, {self});
    const float f20 = first_float.f0;
    const float f21 = second_float.f0;

    if (state != 0u) put(rt, state, 1u);

    u32 window = 0x0001869Fu;
    if (count != 0u && mode != count && call(rt, ctx, 0x08955760u, {self}) != 0u)
        window = call(rt, ctx, 0x0895DD1Cu, {self, count});

    const bool small = signed_of(count) < 2;
    u32 threshold = 0u;
    if (!small) {
        threshold = call(rt, ctx, 0x0895DCB8u, {self, count});
    } else if (count == 1u) {
        if (signed_of(window - 0x3E8u) < signed_of(time) && 0.2f < f20) put8(rt, flag_a, count);
        if (signed_of(time) < signed_of(0xFFFE7579u)) put8(rt, flag_b, 1u);
        ctx.gpr[2] = 1u;
        return;
    } else {
        threshold = 0xFFFE7961u;
    }

    if (signed_of(window - 0x12Cu) < signed_of(time) && 0.7f < f20) put8(rt, flag_a, 1u);

    // f0 is 0.0 here on every path (the original's `mtc1 $zero` delay slots), so the tests below compare against 0.
    if (0.0f < f20) {
        if (signed_of(time) < signed_of(threshold)) put8(rt, flag_b, 1u);
    } else if (f21 == 0.0f) {
        if (signed_of(time) < signed_of(threshold + 0x1F4u)) put8(rt, flag_b, 1u);
    } else {
        if (signed_of(time) < signed_of(threshold + 0x3E8u)) put8(rt, flag_b, 1u);
        if (!small) put8(rt, flag_c, 1u);
    }
    ctx.gpr[2] = 1u;
}

} // namespace
} // namespace new_batch_main_2093
} // namespace gt::game

#define NEW2093(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2093::name, gt::ReturnKind::kind)
NEW2093(0x0887C0A8u, func_0887C0A8, Word);
NEW2093(0x08950438u, func_08950438, Word);
