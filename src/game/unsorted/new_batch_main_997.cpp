// Main-session batch 997: hand-written structured C from `tools/disasm`, draft only (no gate run here).
// Input shapes: /home/andres/repos/gt-ops/shapes/shapes_main997.txt.
//
// Left out on purpose (both were skipped twice before; the branch structure was not decoded with enough
// confidence to write them faithfully):
//   0x08B30660  a table insert that builds its argument block with `subu sp, sp, v0` (a dynamic alloca) and
//               unaligned swl/swr stores at sp+0..+0xB for the import 0x08C7AEC0; the stack block has no fixed
//               offset, so the frame audit and GuestFrame cannot describe it.
//   0x08C51058  a 167-instruction leaf: a lower_bound-style descent over a custom 12-byte key record
//               ({byte at +0, char * at +4, signed length at +8}) with four interleaved length-then-bytes
//               compare loops and a second phase that shares its loop tails; transcribing it needs a label
//               machine, not structured C.
//
// The six below keep the original's v0 on every exit path, every argument register a callee reads, the delay
// slot of a `jal` before the call, and the frame size of the original's prologue.
// literal_check reports four derived literals the asm computes instead of loading: the 0x48 table stride and copy
// size (0x0891EE68: `sll x,6` + `sll x,3`), the 0x7C node stride (0x08AD2438: `sll x,7` - `sll x,2`) and the two
// sign masks of 0x08AD28A8 (`ins` 0,0,31 and `ins` 0,31,1).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_997 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 / v1 / f0 hold what the callee left, as after the original's `jal`.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
         std::initializer_list<float> floats = {}) {
    return call_keep(rt, ctx, target, arguments, floats).v0;
}
// `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
GuestResult virtual_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments,
                         std::initializer_list<float> floats = {}) {
    const u32 function = word(rt, word(rt, object) + slot);
    return call_keep(rt, ctx, function, arguments, floats);
}
[[nodiscard]] u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
[[nodiscard]] s32 signed_of(u32 value) { return static_cast<s32>(value); }
[[nodiscard]] s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline float fbits(u32 bits) { return std::bit_cast<float>(bits); }
inline u32 bits_of(float value) { return std::bit_cast<u32>(value); }
// trunc.w.s: truncation toward zero, saturating outside the signed range like the FPU.
u32 trunc_word(float value) {
    if (!(value < 2147483648.0f)) return 0x7FFFFFFFu;
    if (value <= -2147483648.0f) return 0x80000000u;
    return static_cast<u32>(static_cast<s32>(value));
}

// ---------------------------------------------------------------------------------------------------
// 0x0889B810 (a0 = self, a1 = flag byte): a per-frame pass over the list o = self[0x58][0x10] (nothing when
// self[0x58], o or self[0xB88] is null). Four 0x10-byte entries at frame +0xC start {0, 1.0f, 0, byte 0}; the
// 0xA8 virtual of self[0xB04] runs for i = 0..3 with (object, entry i, i), and when no entry's first float is
// above 0 nothing more happens (v0 = the last call's). Otherwise a stack record {0x08CC2260, owner, 0} is linked
// (owner = self[0xB88][0x68], owner+4 = the record) and for each of o[0x38] elements e (0x18 bytes at
// o[0x3C] + 0x18 * k) whose entry k & 3 has a first float above 0: 0x08B569A0, 0x08B56364(e[0], e[4], e[8]),
// a = 0x08B66D54(e[0x10]) * 180 / pi - 90, b = 0x08B66FBC(e[0xC], e[0x14]), 0x08B5606C(b * 180 / pi); with
// flag 0, 0x08B56128(a), 0x08B55FB0(entry[8]) and the scale is D_08CE2FE0[8], otherwise D_08CE2FE0[0xC]; then
// 0x08B562E0(size, size, size) with size = scale * entry[4], record[8] = owner[0x1C] = the height, 0x08B54FB8(0x6D),
// 0x08AFA170(self[0xB88], self[0xB8C + 4 * flag], 0, 0), 0x08B58874(0x6D) and 0x08B56940(). At the end
// owner+4 = 0, record[0] = 0x08CC2260 and v0 = 0x08AF86DC(record).
void func_0889B810(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0xA0u);
    ctx.gpr[5] = flag;
    const u32 holder = word(rt, self + 0x58u);
    ctx.gpr[2] = holder;
    if (holder == 0u) return;
    const u32 list = word(rt, holder + 0x10u);
    if (list == 0u) return;  // v0 = the holder word loaded above
    const u32 objects = word(rt, self + 0xB88u);
    ctx.gpr[2] = objects;
    if (objects == 0u) return;
    const u32 entries = frame.at(0x0Cu);
    for (u32 index = 0u; index != 4u; ++index) {
        const u32 entry = entries + (index << 4u);
        put(rt, entry, 0u);
        put_float(rt, entry + 4u, 1.0f);
        put(rt, entry + 8u, 0u);
        put8(rt, entry + 0x0Cu, 0u);
    }
    bool any = false;
    for (u32 index = 0u; index != 4u; ++index) {
        const u32 entry = entries + (index << 4u);
        const u32 object = word(rt, self + 0xB04u);
        (void)virtual_call(rt, ctx, object, 0xA8u, {object, entry, index});
        if (0.0f < float_at(rt, entry)) any = true;
    }
    if (!any) return;
    const u32 owner = word(rt, word(rt, self + 0xB88u) + 0x68u);
    const u32 record = frame.at(0u);
    put(rt, record, 0x08CC2260u);
    put(rt, record + 4u, owner);
    put(rt, record + 8u, 0u);
    put(rt, owner + 4u, record);
    s32 count = signed_of(word(rt, list + 0x38u));
    if (count > 0) {
        const float half_turn = fbits(0x43340000u), pi = fbits(0x40490FDBu), ninety = fbits(0x42B40000u);
        u32 offset = 0u;
        for (u32 index = 0u; signed_of(index) < count; ++index) {
            const u32 entry = entries + ((index & 3u) << 4u);
            const float height = float_at(rt, entry);
            if (!(height <= 0.0f)) {
                const u32 element = word(rt, list + 0x3Cu) + offset;
                (void)call(rt, ctx, 0x08B569A0u);
                (void)call_keep(rt, ctx, 0x08B56364u, {},
                                {float_at(rt, element), float_at(rt, element + 4u), float_at(rt, element + 8u)});
                const float raw = call_keep(rt, ctx, 0x08B66D54u, {}, {float_at(rt, element + 0x10u)}).f0;
                const float angle = psp_mul(raw, half_turn) / pi - ninety;
                const float other =
                    call_keep(rt, ctx, 0x08B66FBCu, {}, {float_at(rt, element + 0x0Cu), float_at(rt, element + 0x14u)}).f0;
                (void)call_keep(rt, ctx, 0x08B5606Cu, {}, {psp_mul(other, half_turn) / pi});
                float scale;
                if (flag == 0u) {
                    (void)call_keep(rt, ctx, 0x08B56128u, {}, {angle});
                    (void)call_keep(rt, ctx, 0x08B55FB0u, {}, {float_at(rt, entry + 8u)});
                    scale = float_at(rt, 0x08CE2FE0u + 8u);
                } else {
                    scale = float_at(rt, 0x08CE2FE0u + 0x0Cu);
                }
                const float size = psp_mul(scale, float_at(rt, entry + 4u));
                (void)call_keep(rt, ctx, 0x08B562E0u, {}, {size, size, size});
                put_float(rt, record + 8u, height);
                put_float(rt, owner + 0x1Cu, height);
                (void)call(rt, ctx, 0x08B54FB8u, {0x6Du});
                (void)call(rt, ctx, 0x08AFA170u,
                           {word(rt, self + 0xB88u), word(rt, self + (flag << 2u) + 0xB8Cu), 0u, 0u});
                (void)call(rt, ctx, 0x08B58874u, {0x6Du});
                (void)call(rt, ctx, 0x08B56940u);
                count = signed_of(word(rt, list + 0x38u));
            }
            offset += 0x18u;
        }
    }
    put(rt, owner + 4u, 0u);
    put(rt, record, 0x08CC2260u);
    (void)call(rt, ctx, 0x08AF86DCu, {record});
}

// ---------------------------------------------------------------------------------------------------
// 0x0891EE68 (a0 = output, a1 = the pose state, a2 = flag byte): a small state machine on state[0x21C].
// 0: progress = state[0x218] / 180; when state[0x224] is negative it is cleared, a random half is drawn
// (0x08B1ED60(0x08CF28A8), 0x08B1EFF8 of it, doubled, truncated) and kept at state[0x228] (or 1 with the flag,
// which also makes the 180 check use the elapsed time again); 180 <= elapsed then clears state[0x220] and bumps
// the state. 1: 0x220/0xC0 set once, progress = (elapsed - 180) / 120, 0x224 = 1 and, at 300, state 2.
// 2: 0x220/0xC0 set once, progress = (elapsed - 300) / 180, 0x224 = 2 and, at 480, 0x089281E4(state[0x1E0], 3)
// then state 3. Any other state: progress 0. Progress is capped at 1, the 0x48-byte table entry
// (0x08CF22A4 when state[0x228] == 1, else 0x08CF237C, at 0x48 * state[0x224]) is copied to the frame and
// 0x0891E2A8(output, state, frame, progress) runs; v0 = output.
void func_0891EE68(Runtime &rt, AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const u32 state = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x80u);
    float progress = 0.0f;
    const u32 mode = word(rt, state + 0x21Cu);
    if (mode == 1u) {
        if (word(rt, state + 0x220u) == 0u) {
            put8(rt, state + 0xC0u, 1u);
            put(rt, state + 0x220u, 1u);
        }
        const float elapsed = float_at(rt, state + 0x218u);
        progress = (elapsed - 180.0f) / 120.0f;
        put(rt, state + 0x224u, mode);
        if (300.0f <= elapsed) {
            put(rt, state + 0x21Cu, 2u);
            put(rt, state + 0x220u, 0u);
        }
    } else if (mode == 2u) {
        if (word(rt, state + 0x220u) == 0u) {
            put(rt, state + 0x220u, 1u);
            put8(rt, state + 0xC0u, 1u);
        }
        const float elapsed = float_at(rt, state + 0x218u);
        progress = (elapsed - 300.0f) / 180.0f;
        put(rt, state + 0x224u, mode);
        if (480.0f <= elapsed) {
            (void)call(rt, ctx, 0x089281E4u, {word(rt, state + 0x1E0u), 3u});
            put(rt, state + 0x220u, 0u);
            put(rt, state + 0x21Cu, word(rt, state + 0x21Cu) + 1u);
        }
    } else if (mode != 0u) {
        progress = 0.0f;
    } else {
        const float elapsed = float_at(rt, state + 0x218u);
        const s32 pending = signed_of(word(rt, state + 0x224u));
        progress = elapsed / 180.0f;
        float compare = elapsed;
        if (pending < 0) {
            put(rt, state + 0x224u, 0u);
            const u32 seed = call(rt, ctx, 0x08B1ED60u, {0x08CF28A8u});
            float drawn = call_keep(rt, ctx, 0x08B1EFF8u, {seed}).f0;
            drawn = drawn + drawn;
            const u32 truncated = trunc_word(drawn);
            compare = fbits(truncated);  // the truncated integer's bits are what the 180 check sees
            put(rt, state + 0x228u, truncated);
            if (flag != 0u) {
                compare = elapsed;
                put(rt, state + 0x228u, 1u);
            }
        }
        put(rt, state + 0x224u, 0u);
        if (180.0f <= compare) {
            put(rt, state + 0x220u, 0u);
            put(rt, state + 0x21Cu, word(rt, state + 0x21Cu) + 1u);
        }
    }
    if (1.0f < progress) progress = 1.0f;
    const u32 table = word(rt, state + 0x228u) == 1u ? 0x08CF22A4u : 0x08CF237Cu;
    const u32 entry = table + word(rt, state + 0x224u) * 0x48u;
    const u32 buffer = frame.at(0u);
    for (u32 offset = 0u; offset != 0x48u; offset += 4u) put(rt, buffer + offset, word(rt, entry + offset));
    (void)call(rt, ctx, 0x0891E2A8u, {output, state, buffer}, {progress});
    ctx.gpr[2] = output;
}

// ---------------------------------------------------------------------------------------------------
// 0x08AD28A8 (a0 = self, a1 = a record {vtable, inner}): the record's 0xC virtual slot gets a speed. The
// record's 8 virtual runs with (record, 0, inner[0x54]); record[0x10] = inner[0x30] and the result of
// 0x08AD11E4(self[0x210], self[0x234], inner[0x20]) goes to record[0x14]. A cross product
// (0x08B4AA68(frame, inner[0x20], self[0x210])) is normalised (0x08B4A720, sqrt, 1/len, 0x08B4A5E8) and its
// length scales f21. With sqrt(x^2 + y^2) / self[0x254] below 1 the height becomes
// copysign(sqrt(1 - (x / self[0x254])^2), -y) (sign kept from -y), then abs when self[0x27A] is clear;
// 0x08B4A430(x / self[0x254], that) * 2/pi goes to record[0x18]. When inner[0x64] < 1 the speed
// inner[0x60] is scaled by inner[0x64] lerped towards 1 by 0x08B49F3C(0x08B4A9A0(frame, inner[0x38])) between
// inner[0x68] and inner[0x6C]. The record's 0xC virtual with (record, 0, speed) runs, f21 is raised to
// inner[0x70] and 0x08AD23E4(self, f21, self[0x260]) * self[0x244] goes through the 0xC virtual again with 1.

// ---------------------------------------------------------------------------------------------------
// 0x08AD2438 (a0 = self, a1 = flag, f12 unused, f13 = dt): the frame's turn is clamped to [0, 1] * pi/2 and
// 0x08B49CC8 gives f21. When f21 <= 0 the 0x7C-byte nodes of the list at self[0x2A4] (level = the signed half
// at self[0x2AA], child level at node[0xE]) have the object at node[0x1C] cleared (+0x1F byte, +8, +0xC words).
// Above 0 each node is brought up to date (0x08AD1F9C or 0x08AD203C through node[0x44], node[0x14/+0x18] take
// node[0x44/+0x48], the child at node[0x1C] gets its +0x21 byte and, when present, its +0x1E/+0x1F flags, the
// turn scaled by node[0x4C] — and when that is not zero, by the length of the cross product of node[0x20] with
// self[0x210] against node[0x70], with node[0x74]'s abs at child+8 — at child[+0xC]/[+8]).
void func_08AD2438(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    float delta = ctx.fpr[13];
    if (delta < 0.0f) delta = 0.0f;
    const float turn = delta < 1.0f ? psp_mul(delta, fbits(0x3FC90FDBu)) : fbits(0x3FC90FDBu);
    const float strength = call_keep(rt, ctx, 0x08B49CC8u, {}, {turn}).f0;
    if (strength <= 0.0f) {
        s32 level = signed_half(rt, self + 0x2A4u + 0x06u);
        while (level >= 0) {
            const u32 node = self + 0x2A4u + static_cast<u32>(level) * 0x7Cu + 0x0Cu;  // the stride is the asm's (level << 7) - (level << 2)
            if (node == 0u) break;
            const u32 child = word(rt, node + 0x1Cu);
            if (child != 0u) {
                put8(rt, child + 0x1Fu, 0u);
                put(rt, child + 0x08u, 0u);
                put(rt, child + 0x0Cu, 0u);
            }
            level = signed_half(rt, node + 0x0Eu);
        }
        return;
    }
    const auto child_body = [&](u32 child, u32 node) {
        const u32 high = byte_u(rt, node + 0x59u), low = byte_u(rt, node + 0x58u), shared = byte_u(rt, node + 0x5Cu);
        u32 combined = high & (low | ~(high >> 1u));
        u32 second = high & 1u;
        if (shared != 0u) second = (combined & 1u) | (byte_u(rt, child + 0x1Fu) != 0u ? 1u : 0u);
        put8(rt, child + 0x1Eu, low & 1u);
        put8(rt, child + 0x1Fu, second);
        const float momentum = psp_mul(strength, float_at(rt, node + 0x20u + 0x2Cu));
        if (momentum == 0.0f) {
            put_float(rt, child + 0x0Cu, momentum);
            put_float(rt, child + 0x08u, 0.0f);
            return;
        }
        put_float(rt, child + 0x0Cu, momentum);
        (void)call(rt, ctx, 0x08B4AA68u, {frame.at(0u), node + 0x20u, self + 0x210u});
        const float squared = call_keep(rt, ctx, 0x08B4A720u, {frame.at(0u)}).f0;
        float length = std::sqrt(squared);
        const float limit = float_at(rt, node + 0x70u);
        if (length < limit) length = limit;
        const float final = psp_mul(momentum, limit / length);
        put_float(rt, child + 0x0Cu, final);
        put_float(rt, child + 0x08u, std::fabs(psp_mul(final, float_at(rt, node + 0x74u))));
    };
    s32 level = signed_half(rt, self + 0x2A4u + 0x06u);
    while (level >= 0) {
        const u32 node = self + 0x2A4u + static_cast<u32>(level) * 0x7Cu + 0x0Cu;  // the stride is the asm's (level << 7) - (level << 2)
        const u32 target = word(rt, node + 0x44u);
        const bool same = word(rt, node + 0x14u) == target && word(rt, node + 0x18u) == word(rt, node + 0x44u + 0x04u);
        u32 child = same ? word(rt, node + 0x1Cu) : 0u;
        if (!same) {
            if (target != 0u) (void)call(rt, ctx, 0x08AD1F9Cu, {self, node, node + 0x44u, flag});
            else (void)call(rt, ctx, 0x08AD203Cu, {self, node, flag});
            const u32 filled = word(rt, node + 0x44u);
            const u32 paired = word(rt, node + 0x44u + 0x04u);
            child = word(rt, node + 0x1Cu);
            put(rt, node + 0x14u, filled);
            put(rt, node + 0x18u, paired);
            if (child == 0u) {
                level = signed_half(rt, node + 0x0Eu);
                continue;
            }
            put8(rt, child + 0x21u, byte_u(rt, node + 0x5Cu) != 0u ? 1u : 0u);
        }
        if (child != 0u) child_body(child, node);
        level = signed_half(rt, node + 0x0Eu);
    }
}

// ---------------------------------------------------------------------------------------------------
// 0x089DEBD4 (a0 = self, a1 = a holder whose first word is an object): the object's 0x7C virtual answers a
// count and the 0x140 word's bit 7 (xor 0x80, then shifted) picks one of the 12-byte vectors at
// self + 0xE0 + 0xC * bit + 0xC (begin, end, capacity). A zero count empties that vector (its three words
// cleared, 0x08BF0BF0(begin, end) and 0x08B35254(begin) when it was non-empty). Otherwise it is resized to the
// count (0x08BF1080(vector, begin + 8 * count) when it already holds enough, else
// 0x08BF1768(vector, end, count - held, frame + 0x10)) and for each index two floats are fetched: the object's
// 0x84 virtual fills a frame temporary with (source, index), that temporary's 0x84 fills a second with (source, 0)
// whose 0x54 virtual answers the float at the element's +0 (0x08A5B9EC releases them); then the first temporary's
// 0x84 (source, 1) and 0x54 give the element's +4. Both temporaries are released each round.

// ---------------------------------------------------------------------------------------------------
// 0x089DC564 (a0 = destination, a1 = the argument of 0x089DB3B4, a2 = mode, a3 = source): the temporary built by
// 0x089DB3B4(frame + 0x14, a1). Mode 0 copies the four floats of 0x089DA8FC(temporary[0]) into a frame vector:
// for each index a shared value is built at frame +0x10/0xC/8/4 from the float (0x08A87EEC), the vector's old
// cell (its +0x14 word for index 0, that word plus 4 * index otherwise) is released (0x08A5BAD8(old, temp[0]))
// when it is not the temporary, and the temporary is released (0x08A5B9EC); the vector is then assigned to the
// destination (release 0x08A5BAD8 unless it is the frame vector, then 0x08A5B9EC). Mode 1 fills the four floats of
// frame +0x20 from the 0x54 virtual of the 0x84 virtual of a3's object for each index below its 0x7C count (and
// below 4) and hands them to 0x089DB5CC(a3's object, frame + 0x20). Any other mode only releases the temporary.
void func_089DC564(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    (void)call(rt, ctx, 0x089DB3B4u, {frame.at(0x14u), ctx.gpr[5]});
    if (mode == 0u) {
        u32 object = word(rt, frame.at(0x14u));
        if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CA752Cu});
        const u32 values = call(rt, ctx, 0x089DA8FCu, {object});
        const u32 vector = frame.at(0x20u);
        for (u32 index = 0u; index != 4u; ++index) {
            const u32 temporary = frame.at(0x10u - index * 4u);
            const float value = float_at(rt, values + index * 4u);
            const u32 base = word(rt, vector);
            const u32 previous = index == 0u ? word(rt, base + 0x14u) : word(rt, base + 0x14u) + index * 4u;
            (void)call(rt, ctx, 0x08A87EECu, {temporary}, {value});
            if (previous != temporary) (void)call(rt, ctx, 0x08A5BAD8u, {previous, word(rt, temporary)});
            (void)call(rt, ctx, 0x08A5B9ECu, {temporary});
        }
        if (destination != vector) (void)call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, vector)});
        (void)call(rt, ctx, 0x08A5B9ECu, {vector});
    } else if (mode == 1u) {
        u32 object = word(rt, source);
        if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
        const u32 count = virtual_call(rt, ctx, object, 0x7Cu, {object}).v0;
        const u32 buffer = frame.at(0x20u);
        put_float(rt, buffer, 1.0f);
        put_float(rt, buffer + 4u, 1.0f);
        put_float(rt, buffer + 8u, 1.0f);
        put_float(rt, buffer + 0x0Cu, 1.0f);
        put(rt, buffer + 0x10u, 0xFFFFFFFFu);
        for (u32 index = 0u; index != count && index != 4u; ++index) {
            u32 current = word(rt, source);
            if (current == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
            (void)virtual_call(rt, ctx, current, 0x84u, {frame.at(0u), current, index});
            current = word(rt, frame.at(0u));
            if (current == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
            const float value = call_keep(rt, ctx, word(rt, word(rt, current) + 0x54u), {current}).f0;
            put_float(rt, buffer + index * 4u, value);
            (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
        u32 built = word(rt, frame.at(0x14u));
        if (built == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CA752Cu});
        (void)call(rt, ctx, 0x089DB5CCu, {built, buffer});
    }
    (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
}

} // namespace
} // namespace new_batch_main_997
} // namespace gt::game

#define NEW997(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_997::name, gt::ReturnKind::kind)
NEW997(0x0889B810u, func_0889B810, Word);
NEW997(0x0891EE68u, func_0891EE68, Word);
NEW997(0x08AD2438u, func_08AD2438, Void);
NEW997(0x089DC564u, func_089DC564, Void);
