// New batch main-148 (main session, drafted by DeepSeek): large float-heavy functions the recompiler emitted that no src/ file
// defines. Written by hand from `tools.disasm` as structured C with named locals; guest memory goes through rt.memory().
// A draft: not yet verified by the differential gate. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_148 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s16 = std::int16_t;
using s8 = std::int8_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

u32 u8_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 s8_at(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
u32 u16_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 s16_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

float constant(u32 bits) { return std::bit_cast<float>(bits); }
// cvt.s.w of a signed integer word.
float to_float(s32 value) { return static_cast<float>(value); }
// trunc.w.s: the FPU saturates (NaN and overflow give 0x7FFFFFFF); the host cast would be undefined there.
s32 trunc_s(float value) {
    if (!(value < 2147483648.0f)) return 0x7FFFFFFF;
    if (value < -2147483648.0f) return static_cast<s32>(0x80000000u);
    return static_cast<s32>(value);
}

// --- 0x0895077C ------------------------------------------------------------------------------------------------------
// a0 = A (a state record), a1 = B (the output record), a2 = C (a settings record, only +0xCE is read). Fills B from A: the
// gear/wheel table at B+0x58.., the spring/damper values at B+0xA8.., and the clamps at B+0x10/+0x34/+0x38/+0x48.
void func_0895077C(Runtime &rt, AllegrexContext &ctx) {
    const u32 A = ctx.gpr[4];
    const u32 B = ctx.gpr[5];
    const u32 C = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = frame.at(0);
    const float k_gravity = constant(0x411CCCCDu);  // 9.8
    const float k_hundredth = constant(0x3C23D70Au); // 0.01
    const float k_tenth = constant(0x3DCCCCCDu);     // 0.1
    const float k_milli = constant(0x3A83126Fu);     // 0.001

    call(rt, ctx, 0x089515C4u, {A, B, B + 1u, B + 2u, B + 3u, B + 8u, B + 0xCu});
    put8(rt, B + 0x16u, u8_at(rt, A + 0x12Cu));
    put8(rt, B + 0x17u, u8_at(rt, A + 0x12Du));
    call(rt, ctx, 0x089443BCu, {B + 0x16u, B + 0x17u});
    for (u32 i = 0; i < 2u; ++i) {
        const u32 value = call(rt, ctx, 0x0895064Cu, {static_cast<u32>(s8_at(rt, A + 0x168u + i))});
        put8(rt, B + 0x14u + i, value);
    }
    call(rt, ctx, 0x08950714u, {u8_at(rt, B), sp});
    for (u32 i = 0; i < 2u; ++i) {
        if (u8_at(rt, sp + i) == 0u) {
            if (u8_at(rt, B + 0x14u + i) != 0u) put8(rt, B + 0x14u + i, 0u);
        } else {
            if (u8_at(rt, B + 0x14u + i) == 0u) put8(rt, B + 0x14u + i, 2u);
        }
    }
    // Per wheel pair: three scaled floats at B+0x1C/+0x24/+0x2C chosen by the mode byte at B+0x14+i.
    for (u32 i = 0; i < 2u; ++i) {
        const u32 mode = u8_at(rt, B + 0x14u + i);
        if (mode >= 8u) continue;
        const u32 out = B + 4u * i;
        const float x = to_float(static_cast<s32>(u8_at(rt, A + 0x16Au + i)));
        const float y = to_float(static_cast<s32>(u8_at(rt, A + 0x16Cu + i)));
        const float z = to_float(static_cast<s32>(u8_at(rt, A + 0x16Eu + i)));
        switch (mode) {
        case 0u:
        case 1u:
        case 2u:
            put(rt, out + 0x1Cu, 0u);
            put(rt, out + 0x24u, 0u);
            put(rt, out + 0x2Cu, 0u);
            break;
        case 3u:
        case 4u:
        case 6u:
            put_float(rt, out + 0x1Cu, x * k_gravity);
            put_float(rt, out + 0x24u, y * k_hundredth);
            put_float(rt, out + 0x2Cu, z * k_hundredth);
            break;
        case 5u:
            put_float(rt, out + 0x1Cu, x * x);
            put(rt, out + 0x2Cu, 0u);
            put_float(rt, out + 0x24u, (y * k_tenth) * k_gravity);
            break;
        default: // 7
            put_float(rt, out + 0x1Cu, x * k_hundredth);
            put_float(rt, out + 0x24u, y * k_hundredth);
            put_float(rt, out + 0x2Cu, z * k_hundredth);
            break;
        }
    }

    // B+0x10 (the last wheel index in use): from A+0x155 unless A+0x156/+0x157 ask for a rebuild of the halfword list at A+0x138.
    const u32 flag_156 = u8_at(rt, A + 0x156u);
    const u32 flag_157 = u8_at(rt, A + 0x157u);
    u32 value_155 = u8_at(rt, A + 0x155u);
    bool rebuild;
    if (flag_156 != 0u) {
        rebuild = flag_157 != 0u;
    } else if (flag_157 == 0u) {
        rebuild = false;
    } else if (value_155 < 3u) {
        rebuild = false;
    } else {
        rebuild = s16_at(rt, A + 0x13Au) == 0;
    }
    if (rebuild) {
        const u32 argument = flag_157 == 0xFFu ? 0xFFu : flag_157 * 10u;
        for (u32 i = 0; i < 10u; ++i) put16(rt, A + 0x138u + 2u * i, 0xFFFFu);
        call(rt, ctx, 0x089494D4u, {A, argument, A + 0x138u, 0u});
        value_155 = u8_at(rt, A + 0x155u);
    }
    put8(rt, B + 0x10u, value_155);

    // B+0x11 from A+0x158 (5 becomes 2), then both clamped against each other.
    put8(rt, B + 0x11u, u8_at(rt, A + 0x158u));
    if (u8_at(rt, A + 0x158u) == 5u) put8(rt, B + 0x11u, 2u);
    u32 count = u8_at(rt, B + 0x10u);   // s0
    u32 kind = u8_at(rt, B + 0x11u);    // a0
    const u32 count_before = count;
    if (count_before >= 10u) {
        count = 9u;
        put8(rt, B + 0x10u, 9u);
        kind = u8_at(rt, B + 0x11u);
    } else if (count_before == 0u) {
        count = 1u;
        put8(rt, B + 0x10u, 1u);
        kind = u8_at(rt, B + 0x11u);
    } else {
        kind = u8_at(rt, B + 0x11u);
        bool done = false;
        if (kind == 3u && count_before != 1u) {
            count = 1u;
            put8(rt, B + 0x10u, 1u);
            done = true;
        }
        if (!done && u8_at(rt, A + 0x158u) == 1u && count_before < 2u) {
            put8(rt, B + 0x11u, 0u);
            kind = 0u;
        }
    }
    put8(rt, B + 0x13u, 0u);
    if (kind < 9u && (((1u << (kind & 31u)) & 0x166u) != 0u)) put8(rt, B + 0x13u, 1u);

    // Stiffness table B+0x58+4k, k = 0..count: 4000 (or 3500 for k = 0) when A's halfword k is below 100, else the halfword; all * 0.001.
    const s32 base_half = s16_at(rt, A + 0x150u);
    const float base = (base_half < 500) ? constant(0x457A0000u) : to_float(base_half);
    const float scale = base * k_milli;  // f20
    for (u32 k = 0; k <= count; ++k) {
        const s32 half = s16_at(rt, A + 0x138u + 2u * k);
        float value;
        if (half < 100) {
            if (k >= 2u) {
                count = k - 1u;
                put8(rt, B + 0x10u, count);
                break;
            }
            value = (k == 1u ? constant(0x457A0000u) : constant(0x455AC000u)) * k_milli;
        } else {
            value = to_float(half) * k_milli;
        }
        put_float(rt, B + 0x58u + 4u * k, scale * value);
    }
    const float first = float_at(rt, B + 0x58u);
    const float negated = -first;
    put_float(rt, B + 0x58u, negated);

    const u32 mode_154 = u8_at(rt, A + 0x154u);
    if (mode_154 != 0u && mode_154 != 4u) {
        const s32 half = s16_at(rt, A + 0x152u);
        if (half >= 500) {
            float factor = to_float(half) * k_milli;
            if (mode_154 == 2u) {
                factor = factor / scale;
                put_float(rt, B + 0x58u, negated * factor);
                for (u32 k = 5u; k <= count; ++k) put_float(rt, B + 0x58u + 4u * k, factor * float_at(rt, B + 0x58u + 4u * k));
            } else if (mode_154 == 3u) {
                factor = factor / scale;
                put_float(rt, B + 0x58u, negated * factor);
                for (u32 k = 3u; k < 5u; ++k) put_float(rt, B + 0x58u + 4u * k, factor * float_at(rt, B + 0x58u + 4u * k));
            } else if (mode_154 == 1u) {
                for (u32 k = 0; k <= count; ++k) put_float(rt, B + 0x58u + 4u * k, factor * float_at(rt, B + 0x58u + 4u * k));
            }
        }
    }

    // The two axle records: func_089514A4 fills B+0xA8/+0xB8/+0xC0 (+4 each), the reciprocal goes to B+0xB0.
    for (u32 i = 0; i < 2u; ++i) {
        call(rt, ctx, 0x089514A4u, {A, i, B + 0xA8u + 4u * i, B + 0xB8u + 4u * i, B + 0xC0u + 4u * i});
        const float spring = float_at(rt, B + 0xA8u + 4u * i);
        if (!(spring == 0.0f)) put_float(rt, B + 0xB0u + 4u * i, 1.0f / spring);
    }
    put8(rt, B + 4u, 0u);
    if (u8_at(rt, B) == 5u) {
        if (!(float_at(rt, B + 0xA8u) == float_at(rt, B + 0xACu))) put8(rt, B + 4u, 1u);
    }
    if (u8_at(rt, A + 0x154u) == 4u) put8(rt, B + 4u, 1u);

    // Two ratios from A's halfwords at +0x174/+0x176 (* 0.01), kept at sp+4/+8.
    for (u32 i = 0; i < 2u; ++i) put_float(rt, sp + 4u + 4u * i, to_float(s16_at(rt, A + 0x174u + 2u * i)) * k_hundredth);
    for (u32 i = 0; i < 2u; ++i) {
        const float ratio = float_at(rt, sp + 4u + 4u * i);
        if (!(ratio == 0.0f)) {
            const float spring = float_at(rt, B + 0xA8u + 4u * i);
            const float inverse = float_at(rt, B + 0xB0u + 4u * i);
            const float scaled = spring / ratio;
            const float middle = ratio * inverse;
            put_float(rt, B + 0xE8u + 4u * i, inverse * middle);
            put_float(rt, B + 0xD0u + 4u * i, scaled);
        }
    }
    const float period = to_float(static_cast<s32>(u16_at(rt, A + 0xB8u))) * k_milli;
    put_float(rt, B + 0x18u, period);
    if (u16_at(rt, A + 0xB8u) != 0u) put_float(rt, B + 0xE4u, 1.0f / period);
    const u32 flag_04 = u8_at(rt, B + 4u);
    for (u32 i = 0; i < 2u; ++i) {
        float weight = to_float(s16_at(rt, A + 0x164u + 2u * i)) * k_hundredth;
        if (flag_04 != 0u) {
            const u32 other = u8_at(rt, B + 2u);
            if (other != i) weight = weight * (float_at(rt, B + 0xA8u + 4u * other) / float_at(rt, B + 0xA8u + 4u * i));
        }
        const float ratio = float_at(rt, sp + 4u + 4u * i);
        const float twice = ratio + ratio;
        put_float(rt, B + 0xD8u + 4u * i, (weight * scale) + twice);
    }
    const float sum = float_at(rt, B + 0xD8u) + float_at(rt, B + 0xDCu);
    put_float(rt, B + 0xE0u, 1.0f / sum);
    count = u8_at(rt, B + 0x10u);
    put16(rt, B + 0x34u, u8_at(rt, A + 0x160u) * 10u);
    put16(rt, B + 0x36u, u8_at(rt, A + 0xBEu) * 100u);
    for (u32 k = 0; k <= count; ++k) {
        const float divisor = float_at(rt, B + 0xA8u + 4u * u8_at(rt, B + 2u));
        put_float(rt, B + 0x80u + 4u * k, float_at(rt, B + 0x58u + 4u * k) / divisor);
    }
    if (call(rt, ctx, 0x08950770u, {B}) != 0u) {
        put16(rt, B + 0x34u, u16_at(rt, C + 0xCEu));
    } else {
        const s32 lowest = static_cast<s32>(u16_at(rt, B + 0x34u)) - 100;
        const s32 limit = static_cast<s32>(u16_at(rt, C + 0xCEu));
        if (lowest < limit) put16(rt, B + 0x34u, static_cast<u32>(limit + 100));
    }

    if (count >= 2u) {
        const u32 height = u16_at(rt, B + 0x36u);
        const u32 start = u16_at(rt, B + 0x34u);
        const float height_scaled = to_float(static_cast<s32>(height)) * constant(0x3DD67750u);
        const float k_sixty = constant(0x42700000u);
        const float k_two_pi = constant(0x40C90FDBu);
        const float k_ratio = constant(0x4031C71Du);
        const s32 floor_limit = static_cast<s32>(start + 0x1F4u);
        for (u32 j = 0; j + 2u <= count; ++j) {  // n - 1 iterations: the original tests a3 = j + 2 < n after each
            const u32 slot = B + 4u * j;
            put16(rt, B + 0x38u + 2u * j, height);
            const bool flagged = u8_at(rt, 0x08CDE508u + 0x15Eu) != 0u;
            float numerator = height_scaled * float_at(rt, slot + 0x60u);
            numerator = numerator / float_at(rt, slot + 0x5Cu);
            numerator = (numerator * k_sixty) / k_two_pi;
            s32 target = trunc_s(numerator);
            if (flagged && j == 0u) {
                const s32 sum_start = target + static_cast<s32>(start);
                target = (sum_start + static_cast<s32>(static_cast<u32>(sum_start) >> 31)) >> 1;
            }
            float drop = float_at(rt, slot + 0x88u) * k_ratio;
            drop = (drop * k_sixty) / k_two_pi;
            const s32 low = target - trunc_s(drop);
            put16(rt, B + 0x48u + 2u * j, static_cast<u32>(low > floor_limit ? low : floor_limit));
        }
    }
    if (call(rt, ctx, 0x0895BBC4u, {B}) != 0u) {
        count = u8_at(rt, B + 0x10u);
        if (count >= 2u) {
            for (u32 j = 0; j + 2u <= count; ++j) {
                put_float(rt, B + 0x60u + 4u * j, float_at(rt, B + 0x60u + 4u * j) / float_at(rt, B + 0x5Cu));
            }
        }
    }
    call(rt, ctx, 0x0893EB08u, {B});
    if (u8_at(rt, A + 1u) == 5u) {
        put8(rt, B + 0x12u, 4u);
        ctx.gpr[2] = 4u;
    } else {
        ctx.gpr[2] = 5u;
    }
}

} // namespace
} // namespace new_batch_main_148
} // namespace gt::game

#define NEW148(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_148::name, gt::ReturnKind::kind)
NEW148(0x0895077Cu, func_0895077C, Void);
