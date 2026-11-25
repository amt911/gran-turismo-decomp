// New batch main 1010: functions drafted from `tools.disasm` as structured C (main session). A draft batch, not
// verified by the differential gate yet.
//
// Drafted: 0x08AE3158 (allocate and fill a course/field node), 0x08C49F74 (copy 0xF0-byte records),
// 0x08B6A1AC (acosf), 0x088C711C (walk a piecewise path to a distance). Skipped after reading the asm:
// 0x08AFFECC (the gate saw five floats of one object differ only in the sign bit — a NaN-sign difference from an FPU
// operation whose PSP rounding/NaN rule the draft did not pin down), 0x08B2811C (an LZ77 match search whose two
// repetition loops could not be pinned down by hand), 0x08C5C160 (a large state machine whose arguments arrive in
// t0-t3 with a0 overwritten in the prologue).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1010 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call that leaves v0 / v1 / f0 as the callee left them (several of these read the callee's v1 or f0 next).
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                        std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_result(rt, ctx, target, arguments).v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats = {}) {
    return call_result(rt, ctx, target, arguments, floats).f0;
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// --- 0x08AE3158 (self = a0, user = a1, column = a2, mode = a3, count = t0) ---------------------------------------------
// Takes the first node of the static list at 0x08D4095C whose +8 is not negative, unlinks it, and for mode 0 / 1
// allocates a slot through 0x08AE69F4(a0 = mode, a1 = self->+0x10) with the scale 0x08CDE620 / 500000.0 divided by
// count; every other mode only relinks the node at the end of the list. The node is then filled from self and the
// 0x08CDFC70.. tables and linked in front of self + 0x14, setting bit 1 of self->+0xC. A missing node or a failed
// allocation returns 0; the debug log 0x08AD83B8 is gated by word(0x08CDFB3C). v0 = the node, or 0.
void func_08AE3158(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 user = ctx.gpr[5];
    const u32 column = ctx.gpr[6];
    const u32 mode = ctx.gpr[7];
    const u32 count = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);
    const u32 first = word(rt, 0x08D4095Cu + 4u);
    // `slti v0, key, 0; movz s0, v1, v0`: the node is kept (s0 = first) when the key is not negative, else s0 stays 0.
    const u32 node = static_cast<s32>(word(rt, first + 8u)) >= 0 ? first : 0u;
    if (node == 0u) {
        if ((word(rt, 0x08CDFB3Cu) & 2u) != 0u)
            (void)call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB44u), 0x08C9145Cu, 0x1B2u, 0x08C914E8u, count});
        ctx.gpr[2] = 0u;
        return;
    }
    if (mode != 0u && mode != 1u) {
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u)
            (void)call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08D3FB40u), 0x08C9145Cu, 0x1C3u, 0x08C914BCu, mode});
        const u32 previous = word(rt, node);
        if (previous != node) {
            const u32 next = word(rt, node + 4u);
            put(rt, previous + 4u, next);
            put(rt, next, previous);
            put(rt, node + 4u, node);
            put(rt, node, node);
        }
        const u32 last = word(rt, 0x08D4095Cu);
        put(rt, node + 4u, 0x08D4095Cu);
        put(rt, last + 4u, node);
        put(rt, node, last);
        put(rt, 0x08D4095Cu, node);
        ctx.gpr[2] = 0u;
        return;
    }
    // The scale: mode 0 divides the table constant at 0x08CDE620, mode 1 the literal 500000.0, both by count as a
    // signed int (the asm's `mtc1 t0` / `cvt.s.w`).
    const float divisor = static_cast<float>(static_cast<s32>(count));
    const float scale = mode == 0u ? float_at(rt, 0x08CDE620u) / divisor : 500000.0f / divisor;
    const u32 slot = call(rt, ctx, 0x08AE69F4u, {mode, word(rt, self + 0x10u)});
    if (static_cast<s32>(slot) < 0) {
        if ((word(rt, 0x08CDFB3Cu) & 2u) != 0u)
            (void)call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB44u), 0x08C9145Cu, 0x1C9u, 0x08C914D8u, count});
        ctx.gpr[2] = 0u;
        return;
    }
    put16(rt, node + 0x1Au, slot);
    put(rt, node + 8u, user);
    put(rt, node + 0x10u, self);
    put(rt, node + 0x14u, 0u);
    put16(rt, node + 0x3Eu, count);
    put16(rt, node + 0x18u, 0xFFFFFFFFu);
    put16(rt, node + 0x1Cu, 0xFFFFFFFFu);
    put16(rt, node + 0x1Eu, 0xFFFFFFFFu);
    put16(rt, node + 0x20u, 0xFFFFFFFFu);
    put8(rt, node + 0x3Du, mode);
    put8(rt, node + 0x3Cu, column);
    put(rt, node + 0x24u, word(rt, self + 0x10u) + 0xA8u);
    // The three table reads below are indexed by ((column << 1) + mode) << 2, the other four by column << 2.
    put(rt, node + 0x28u, word(rt, 0x08CDFC70u + ((column << 1u) + mode) * 4u));
    put(rt, node + 0x2Cu, word(rt, 0x08CDFC80u + column * 4u));
    put(rt, node + 0x30u, word(rt, 0x08CDFC88u + column * 4u));
    put(rt, node + 0x34u, word(rt, 0x08CDFC90u + column * 4u));
    put_float(rt, node + 0x50u, scale);
    put(rt, node + 0x38u, word(rt, 0x08CDFC98u + column * 4u));
    put(rt, node + 0x40u, 0u);
    put(rt, node + 0x44u, 0u);
    put(rt, node + 0x48u, 0u);
    put(rt, node + 0x4Cu, 0u);
    put8(rt, node + 0x54u, 0u);
    put8(rt, node + 0x55u, 0u);
    put16(rt, node + 0x56u, 0u);
    put16(rt, node + 0x58u, 0u);
    put8(rt, node + 0x5Au, 0u);
    put(rt, node + 0x5Cu, 0u);
    const u32 previous = word(rt, node);
    put(rt, node + 0x6Cu, 0xFFFFFFFFu);
    put8(rt, node + 0x89u, 0xFFFFFFFFu);
    put8(rt, node + 0x88u, 0xFFFFFFFFu);
    put(rt, node + 0x60u, 0u);
    put(rt, node + 0x64u, 0u);
    put(rt, node + 0x68u, 0u);
    put8(rt, node + 0x84u, 0u);
    put8(rt, node + 0x85u, 0u);
    put8(rt, node + 0x86u, 0u);
    put8(rt, node + 0x87u, 0u);
    if (previous != node) {
        const u32 next = word(rt, node + 4u);
        put(rt, previous + 4u, next);
        put(rt, next, previous);
        put(rt, node + 4u, node);
        put(rt, node, node);
    }
    const u32 sentinel = self + 0x14u;
    const u32 first_node = word(rt, sentinel);
    put(rt, node + 4u, sentinel);
    put(rt, first_node + 4u, node);
    put(rt, node, first_node);
    put(rt, self + 0xCu, word(rt, self + 0xCu) | 2u);
    put(rt, sentinel, node);
    ctx.gpr[2] = node;
}

// --- 0x08C49F74 (a0 = source, a1 = end, a2 = destination) --------------------------------------------------------------
// Copies 0xF0-byte records: `(source == end)` returns the destination as it came in, otherwise every record is copied
// word by word (a null destination skips the copy but still advances both pointers) until source == end, and v0 is the
// advanced destination.

// --- 0x08B6A1AC (f12 = x) -> f0 = acos(x) ------------------------------------------------------------------------------
// The libm acosf, branch for branch: |x| == 1 answers 0 or pi, |x| > 1 the guest's (x - x) / (x - x), |x| > 0.49999997
// the square-root form (0x08B6C020) with a Newton correction over the root's truncated mantissa, and |x| > 0x23000000
// the direct polynomial in x * x; smaller magnitudes answer pi / 2 from 0x08CB62CC. Every constant is a table word.
void func_08B6A1AC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const float x = ctx.fpr[12];
    const u32 bits = std::bit_cast<u32>(x);
    // `ins v1, zero, 31, 1`: the magnitude, with the sign bit cleared.
    const u32 magnitude = bits << 1u >> 1u;
    const float one = float_at(rt, 0x08CB62F8u);
    if (magnitude == 0x3F800000u) {
        ctx.fpr[0] = static_cast<s32>(bits) > 0 ? 0.0f : float_at(rt, 0x08CB62C8u);
        return;
    }
    if (magnitude > 0x3F800000u) {
        // `sub.s f0, f12, f12` then `div.s f0, f0, f0`: the guest's invalid operation, kept as the FPU provides it.
        const float difference = x - x;
        ctx.fpr[0] = difference / difference;
        return;
    }
    const float half = float_at(rt, 0x08CB6304u);
    const float c1 = float_at(rt, 0x08CB62D0u);
    const float c2 = float_at(rt, 0x08CB62D4u);
    const float c3 = float_at(rt, 0x08CB62E8u);
    const float c4 = float_at(rt, 0x08CB62D8u);
    const float c5 = float_at(rt, 0x08CB62ECu);
    const float c6 = float_at(rt, 0x08CB62DCu);
    const float c7 = float_at(rt, 0x08CB62F0u);
    const float c8 = float_at(rt, 0x08CB62E0u);
    const float c9 = float_at(rt, 0x08CB62F4u);
    const float c10 = float_at(rt, 0x08CB62E4u);
    const float tiny = float_at(rt, 0x08CB62FCu);
    if (magnitude > 0x3EFFFFFFu) {
        // |x| in (0.49999997, 1): t = (1 - x) / 2 or (x + 1) / 2 on the sign, then the polynomial in t over the root.
        float t;
        if (static_cast<s32>(bits) >= 0) {
            t = (one - x) * half;
        } else {
            t = (x + one) * half;
        }
        const float root = call_float(rt, ctx, 0x08B6C020u, {}, {t});
        float p = t * c1;
        p = p + c2;
        p = t * p;
        p = p - c4;
        p = t * p;
        p = p + c6;
        p = t * p;
        p = p - c8;
        p = t * p;
        p = p + c10;
        p = t * p;
        float q = t * c3;
        q = q - c5;
        q = t * q;
        q = q + c7;
        q = t * q;
        q = q - c9;
        q = t * q;
        q = q + one;
        if (static_cast<s32>(bits) >= 0) {
            // `ins v0, zero, 0, 12`: the root with its low twelve mantissa bits cleared.
            const float truncated = std::bit_cast<float>((std::bit_cast<u32>(root) >> 12u) << 12u);
            const float square = truncated * truncated;
            const float sum = root + truncated;
            float correction = t - square;
            correction = correction / sum;
            float scaled = p / q;
            scaled = root * scaled;
            correction = correction + scaled;
            const float total = truncated + correction;
            ctx.fpr[0] = total + total;
            return;
        }
        float scaled = p / q;
        scaled = root * scaled;
        scaled = scaled - tiny;
        float total = root + scaled;
        total = total * float_at(rt, 0x08CB6308u);
        ctx.fpr[0] = total + float_at(rt, 0x08CB630Cu);
        return;
    }
    if (magnitude <= 0x23000000u) {
        ctx.fpr[0] = float_at(rt, 0x08CB62CCu);
        return;
    }
    // |x| in (1.4e-18, 0.49999997]: the polynomial in x * x, then x - (tiny - x * (s * p / q)).
    const float square = x * x;
    float p = square * c1;
    p = p + c2;
    p = square * p;
    p = p - c4;
    p = square * p;
    p = p + c6;
    p = square * p;
    p = p - c8;
    p = square * p;
    p = p + c10;
    float q = square * c3;
    q = q - c5;
    q = square * q;
    q = q + c7;
    q = square * q;
    q = q - c9;
    q = square * q;
    q = q + one;
    float quotient = square * p;
    quotient = quotient / q;
    quotient = x * quotient;
    float rest = tiny - quotient;
    rest = x - rest;
    ctx.fpr[0] = float_at(rt, 0x08CB6300u) - rest;
}

// --- 0x08B2811C (self = a0, input = a1, available = a2) -----------------------------------------------------------------
// One LZ77 step. self:+0x10 is the 0x8000-byte ring, +0x1C the ring position, +0x14 the s16 hash-chain heads (per input
// byte, the last ring position that byte was written at), +0x18 the chain (4 bytes per node), +0x30 the search budget.
// A candidate at ring position `node` is compared with the input for at most (pos - node) + 1 bytes; when the whole
// candidate matched and the input repeats its first byte the comparison runs on across the repetition. The longest
// match wins, but only for 3 bytes or more: below that the first byte goes out as a literal. v0 = the length emitted,
// or -1 when the ring refused a push.
void func_08B2811C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 input = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const s32 available = static_cast<s32>(ctx.gpr[6]);
    const u32 first = rt.memory().load8(input);
    // The tail every exit shares: push `count` input bytes through 0x08B272D4 and answer count.
    const auto copy_matched = [&](s32 count) -> s32 {
        u32 index = 0u;
        do {
            const u32 value = rt.memory().load8(input + index);
            index += 1u;
            (void)call(rt, ctx, 0x08B272D4u, {self, value});
        } while (count != static_cast<s32>(index));
        return count;
    };
    // The literal: the first byte pushed as itself, one counter bump, then it is copied; v0 = 1.
    const auto emit_literal = [&]() -> s32 {
        if (call(rt, ctx, 0x08B273A0u, {self, first}) == 0u) return -1;
        (void)call(rt, ctx, 0x08B2740Cu, {self, first});
        return copy_matched(1);
    };
    if (available < 3) {
        ctx.gpr[2] = static_cast<u32>(emit_literal());
        return;
    }
    const u32 heads = word(rt, self + 0x14u);
    s32 node = static_cast<std::int16_t>(rt.memory().load16(heads + 2u * first));
    if (node < 0) {
        ctx.gpr[2] = static_cast<u32>(emit_literal());
        return;
    }
    const s32 limit = available < 0x102 ? available : 0x102;
    const u32 ring = word(rt, self + 0x10u);
    const u32 position = rt.memory().load16(self + 0x1Cu);
    u32 budget = word(rt, self + 0x30u);
    s32 best = 0;
    u32 best_node = 0u;
    bool emit_token = false;
    for (;;) {
        u32 distance = position - static_cast<u32>(node);
        if (static_cast<s32>(distance) < 0) distance += 0x8000u;
        const u32 length = static_cast<u32>(distance) + 1u;
        const s32 candidate = static_cast<s32>(length) < limit ? static_cast<s32>(length) : limit;
        s32 remaining = candidate - 1;
        u32 index = (static_cast<u32>(node) + 1u) & 0x7FFFu;
        s32 matched;
        if (remaining == 0) {
            matched = candidate;
        } else if (rt.memory().load8(input + 1u) != rt.memory().load8(ring + index)) {
            matched = candidate - remaining;
        } else {
            u32 walk = input + 1u;
            const u32 end = walk + static_cast<u32>(remaining);
            for (;;) {
                index = (index + 1u) & 0x7FFFu;
                walk += 1u;
                remaining -= 1;
                if (end == walk) {
                    matched = candidate - remaining;
                    break;
                }
                if (rt.memory().load8(ring + index) != rt.memory().load8(walk)) {
                    matched = candidate - remaining;
                    break;
                }
            }
        }
        if (remaining == 0) {
            // The whole candidate matched: the input may repeat its own first byte and run on.
            const u32 rest = static_cast<u32>(limit) - length;
            if (rest != 0u && rt.memory().load8(input + length) == first) {
                u32 scan = input + length;
                u32 pattern = input;
                u32 left = rest;
                u32 count = 0u;
                const u32 stop = rest - 1u;
                for (;;) {
                    scan += 1u;
                    pattern += 1u;
                    left -= 1u;
                    if (stop == count) break;
                    if (rt.memory().load8(scan) != rt.memory().load8(pattern)) break;
                    count += 1u;
                }
                matched += static_cast<s32>(rest - left);
            }
        }
        budget -= static_cast<u32>(matched);
        if (matched > best) {
            best = matched;
            best_node = static_cast<u32>(node);
            if (matched >= 0x102) {
                emit_token = true;
                break;
            }
        }
        if (static_cast<s32>(budget) < 0) break;
        node = static_cast<std::int16_t>(rt.memory().load16(word(rt, self + 0x18u) + 4u * static_cast<u32>(node)));
        if (node < 0) break;
    }
    if (!emit_token && best < 3) {
        ctx.gpr[2] = static_cast<u32>(emit_literal());
        return;
    }
    // The length symbol 0x101 + table[best - 3], its extra bits, then the distance symbol from the binary search over
    // the 0x1E two-halfword ranges at 0x08C93900 and its extra bits; the matched bytes follow.
    const u32 length_class = rt.memory().load8(0x08C93800u + static_cast<u32>(best) - 3u);
    const u32 symbol = length_class + 0x101u;
    const u32 length_bits = rt.memory().load8(0x08C937C0u + length_class);
    if (call(rt, ctx, 0x08B273A0u, {self, symbol}) == 0u) {
        ctx.gpr[2] = static_cast<u32>(-1);
        return;
    }
    (void)call(rt, ctx, 0x08B2740Cu, {self, symbol});
    if (length_bits != 0u) {
        // `sllv a1, 1, s4; a1 -= 1`: sllv takes the shift amount modulo 32.
        const u32 extra = ((1u << (length_bits & 31u)) - 1u) & (static_cast<u32>(best) - 3u);
        if (call(rt, ctx, 0x08B273A0u, {self, extra}) == 0u) {
            ctx.gpr[2] = static_cast<u32>(-1);
            return;
        }
    }
    const u32 distance = ((rt.memory().load16(self + 0x1Cu) - best_node) & 0x7FFFu) + 1u;
    s32 low = 0;
    s32 high = 0x1E;
    u32 distance_class = 0u;
    for (;;) {
        const s32 middle = static_cast<std::int8_t>((low + high) >> 1);
        if (distance < rt.memory().load16(0x08C93900u + 2u * static_cast<u32>(middle))) {
            high = middle;
            continue;
        }
        if (distance < rt.memory().load16(0x08C93900u + 2u * static_cast<u32>(middle) + 2u)) {
            distance_class = static_cast<u32>(middle) & 0xFFu;
            break;
        }
        low = middle + 1;
    }
    if (call(rt, ctx, 0x08B273A0u, {self, distance_class}) == 0u) {
        ctx.gpr[2] = static_cast<u32>(-1);
        return;
    }
    const u32 distance_bits = rt.memory().load8(0x08C937E0u + distance_class);
    (void)call(rt, ctx, 0x08B27428u, {self, distance_class});
    if (distance_bits != 0u) {
        const u32 extra = ((1u << (distance_bits & 31u)) - 1u) & (distance - 1u);
        if (call(rt, ctx, 0x08B273A0u, {self, extra}) == 0u) {
            ctx.gpr[2] = static_cast<u32>(-1);
            return;
        }
    }
    ctx.gpr[2] = static_cast<u32>(copy_matched(best));
}

// --- 0x088C711C (a1 = path, a2 = state) --------------------------------------------------------------------------------
// Walks a piecewise path (a virtual resolution 0x08C50B8/0x08C50C4 over a node list) up to the distance at
// state->+0x14 along it, and fills state:+0x18/+0x1C/+0x20/+0x24, +0x34/+0x38/+0x3C when the state flags ask for them
// (bit 1 the segment pair, bit 4 the transform, bits 1/4/0/2 the interpolation). v0 = the index it stopped at.
// a0 is unused. The two branch-likely instructions (bc1tl / bc1fl) nullify their delay slot when they fall through,
// which is why the "fits inside this segment" path keeps the old pair and position 0 on the iteration limit.

} // namespace
} // namespace new_batch_main_1010
} // namespace gt::game

#define NEW1010(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1010::name, gt::ReturnKind::kind)
NEW1010(0x08AE3158u, func_08AE3158, Word);
NEW1010(0x08B6A1ACu, func_08B6A1AC, Float);
NEW1010(0x08B2811Cu, func_08B2811C, Word);
