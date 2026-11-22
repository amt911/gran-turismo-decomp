// Main-session batch 133 (drafted, unverified). Structured C from tools.disasm; no names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_133 {
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
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
void put_half(Runtime &rt, u32 a, u32 value) { rt.memory().store16(a, static_cast<std::uint16_t>(value)); }

float bits_float(u32 bits) { return std::bit_cast<float>(bits); }
u32 float_word(float value) { return std::bit_cast<u32>(value); }

// 0x08B6E920 (f12 = x): the shape of fdlibm's expm1f. The constants are floats in .rodata (0x08CB6568..0x08CB65B8). The result is f0 (= f12).
// The tail (label 0x08B6E9FC) takes the reduced argument `hi` (f12), its correction `lo` (f7) and the exponent `k` (a0).
void func_08B6E920(Runtime &rt, AllegrexContext &ctx) {
    float x = ctx.fpr[12];
    const u32 raw = std::bit_cast<u32>(x);
    const u32 magnitude = raw & 0x7FFFFFFFu;
    const u32 sign = raw & 0x80000000u;
    float lo = ctx.fpr[7];
    s32 k = 0;
    auto finish = [&](float result) {
        ctx.fpr[12] = result;
        ctx.fpr[0] = result;
    };
    // 0 = reduce by ln2 (label 0x08B6E974), 1 = k and hi/lo are set (label 0x08B6E9FC), 2 = k == 0 (label 0x08B6E9F4)
    int stage;
    if (magnitude <= 0x4195B843u) {
        if (magnitude > 0x3EB17218u) {
            if (magnitude > 0x3F851591u) {
                stage = 0;
            } else if (sign != 0u) {
                float t = x + float_at(rt, 0x08CB6574u);
                x = t - float_at(rt, 0x08CB657Cu);
                lo = (t - x) - float_at(rt, 0x08CB657Cu);
                k = -1;
                stage = 1;
            } else {
                float t = x - float_at(rt, 0x08CB6574u);
                x = t - float_at(rt, 0x08CB6578u);
                lo = (t - x) - float_at(rt, 0x08CB6578u);
                k = 1;
                stage = 1;
            }
        } else if (magnitude > 0x32FFFFFFu) {
            stage = 2;
        } else {
            const float t = x + float_at(rt, 0x08CB6590u);
            finish(x - (t - t));
            return;
        }
    } else {
        if (magnitude > 0x7F800000u) {
            finish(x + x);
            return;
        }
        if (magnitude == 0x7F800000u) {
            finish(sign == 0u ? x : float_at(rt, 0x08CB6568u));
            return;
        }
        if (sign != 0u) {
            if (x + float_at(rt, 0x08CB6570u) < 0.0f) {
                finish(float_at(rt, 0x08CB6568u));
                return;
            }
            stage = 0;
        } else if (magnitude > 0x42B17217u) {
            finish(float_at(rt, 0x08CB656Cu));
            return;
        } else {
            stage = 0;
        }
    }
    if (stage == 0) {
        float scaled = psp_mul(x, float_at(rt, 0x08CB6580u));
        const float half = sign == 0u ? float_at(rt, 0x08CB6584u) : float_at(rt, 0x08CB6588u);
        scaled = half + scaled;
        k = static_cast<s32>(scaled);
        const float kf = static_cast<float>(k);
        const float hi_term = psp_mul(kf, float_at(rt, 0x08CB658Cu));
        const float lo_term = psp_mul(kf, float_at(rt, 0x08CB6578u));
        const float t = x + hi_term;
        x = t - lo_term;
        lo = (t - x) - lo_term;
        stage = 1;
    }
    const float half = float_at(rt, 0x08CB6584u);
    const float one = float_at(rt, 0x08CB65A8u);
    const float hfx = psp_mul(x, half);
    const float hxs = psp_mul(x, hfx);
    float r1 = psp_mul(hxs, float_at(rt, 0x08CB6594u)) + float_at(rt, 0x08CB6598u);
    r1 = psp_mul(hxs, r1) - float_at(rt, 0x08CB659Cu);
    r1 = psp_mul(hxs, r1) + float_at(rt, 0x08CB65A0u);
    r1 = psp_mul(hxs, r1) - float_at(rt, 0x08CB65A4u);
    r1 = psp_mul(hxs, r1) + one;
    const float t3 = float_at(rt, 0x08CB65ACu) - psp_mul(hfx, r1);
    const float denom = float_at(rt, 0x08CB65B0u) - psp_mul(x, t3);
    const float e = psp_mul(hxs, (r1 - t3) / denom);
    if (k == 0) {
        finish(x - (psp_mul(x, e) - hxs));
        return;
    }
    float corrected = psp_mul(x, e - lo) - lo;
    const float shifted = corrected - hxs;
    if (k == -1) {
        finish(psp_mul(x - shifted, half) - half);
        return;
    }
    if (k == 1) {
        if (x < float_at(rt, 0x08CB65B4u)) {
            finish(psp_mul(shifted - (x + half), float_at(rt, 0x08CB65B8u)));
        } else {
            const float twice = (x - shifted) + (x - shifted);
            finish(twice + one);
        }
        return;
    }
    const u32 exponent = static_cast<u32>(k) << 23;
    if (static_cast<u32>(k) + 1u >= 0x3Au) {
        const float y = one - (shifted - x);
        finish(bits_float(float_word(y) + exponent) - one);
    } else if (k < 0x17) {
        const s32 shifted_bits = static_cast<s32>(0x01000000) >> (static_cast<u32>(k) & 31u);
        const float y = bits_float(0x3F800000u - static_cast<u32>(shifted_bits)) - (shifted - x);
        finish(bits_float(float_word(y) + exponent));
    } else {
        const float scale = bits_float(static_cast<u32>(0x7F - k) << 23);
        const float y = (x - (shifted + scale)) + one;
        finish(bits_float(float_word(y) + exponent));
    }
}

// 0x08B00E90 (a0 = object with the halfword cursor at +0xEC, a1 = container): walks the cursor list. The frame holds copies of an iterator
// (sp+0x00..0x7C, only 0x60/0x62 and 0x80..0x8C are read again), the loop state lives in sp+0x84/0x88.
// A node with bit 0 clear and a failing +0xE2 mask test is handed to 0x08C46854 (out: sp+0x90..0x9C) and then to 0x08B016F4.
// v0 = the cursor halfword stored back to +0xEC; a negative cursor returns at once with v0 = that cursor. t1 is the delay-slot leftover
// of the original's loop test (its value on the first pass is the caller's).
void func_08B00E90(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 container = ctx.gpr[5];
    GuestFrame frame(ctx, 0xB0u);
    const s32 cursor = signed_half(rt, object + 0xECu);
    ctx.gpr[2] = static_cast<u32>(cursor);
    if (cursor < 0) return;
    const u32 table = word(rt, container + 0x44u);
    const u32 stack_arg = word(rt, frame.at(0x50u));
    const u32 saved_b = word(rt, frame.at(0x20u));
    const u32 saved_c = word(rt, frame.at(0x24u));
    const u32 end = container + 0x44u;
    const u32 start = table + (static_cast<u32>(cursor) << 3);
    put(rt, frame.at(0x10u), stack_arg);
    put(rt, frame.at(0x00u), saved_b);
    put(rt, frame.at(0x04u), saved_c);
    put(rt, frame.at(0x68u), end);
    put(rt, frame.at(0x5Cu), 0u);
    put(rt, frame.at(0x58u), start);
    put(rt, frame.at(0x54u), end);
    put(rt, frame.at(0x30u), stack_arg);
    put(rt, frame.at(0x34u), end);
    put(rt, frame.at(0x38u), start);
    put(rt, frame.at(0x3Cu), 0u);
    put(rt, frame.at(0x80u), stack_arg);
    put(rt, frame.at(0x84u), end);
    put(rt, frame.at(0x88u), start);
    put(rt, frame.at(0x8Cu), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    put(rt, frame.at(0x28u), 0u);
    put(rt, frame.at(0x70u), saved_b);
    put(rt, frame.at(0x74u), saved_c);
    put(rt, frame.at(0x78u), 0u);
    put(rt, frame.at(0x7Cu), 0u);
    put(rt, frame.at(0x14u), end);
    put(rt, frame.at(0x18u), start);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x08u), 0u);
    put(rt, frame.at(0x0Cu), 0u);
    put(rt, frame.at(0x64u), 0u);
    s32 index = -1;
    if (start != 0u) index = static_cast<std::int16_t>(static_cast<s32>(start - table) >> 3);
    put_half(rt, frame.at(0x62u), static_cast<u32>(-1));
    u32 slot = 0u;
    put_half(rt, frame.at(0x60u), static_cast<u32>(index));
    const u32 iterator_end = word(rt, frame.at(0x68u));
    if (index >= 0) slot = word(rt, iterator_end) + (static_cast<u32>(index) << 3);
    put(rt, frame.at(0x84u), iterator_end);
    put(rt, frame.at(0x80u), word(rt, frame.at(0x40u)));
    put(rt, frame.at(0x88u), slot);
    put(rt, frame.at(0x4Cu), 0u);
    put(rt, frame.at(0x48u), slot);
    put(rt, frame.at(0x44u), iterator_end);
    put(rt, frame.at(0x8Cu), 0u);
    u32 t1 = ctx.gpr[9];
    u32 node = word(rt, frame.at(0x88u));
    while (node != 0u) {
        bool advance = true;
        if ((rt.memory().load16(node) & 1u) == 0u) {
            const s32 kind = signed_half(rt, node + 2u);
            const u32 base = word(rt, container);
            const u32 mask = rt.memory().load16(object + 0xE2u);
            const u32 entry = static_cast<u32>(kind) * 0xF0u + base;
            if ((rt.memory().load16(entry + 0xE2u) & mask & 4u) == 0u) {
                ctx.gpr[6] = frame.at(0x80u);
                call(rt, ctx, 0x08C46854u, {frame.at(0x90u), frame.at(0x60u), frame.at(0x80u), mask, base});
                const u32 out0 = word(rt, frame.at(0x90u));
                const u32 out1 = word(rt, frame.at(0x94u));
                put(rt, frame.at(0x80u), out0);
                put(rt, frame.at(0x84u), out1);
                const u32 out2 = word(rt, frame.at(0x98u));
                const u32 out3 = word(rt, frame.at(0x9Cu));
                put(rt, frame.at(0x88u), out2);
                put(rt, frame.at(0x8Cu), out3);
                call(rt, ctx, 0x08B016F4u, {container, node, ctx.gpr[6], ctx.gpr[7]});
                node = word(rt, frame.at(0x88u));
                advance = false;
            } else {
                put(rt, frame.at(0x8Cu), node);
            }
        } else {
            put(rt, frame.at(0x8Cu), node);
        }
        if (advance) {
            const s32 link = signed_half(rt, node + 6u);
            if (link >= 0) {
                const u32 head = word(rt, word(rt, frame.at(0x84u)));
                t1 = head + (static_cast<u32>(link) << 3);
            }
            put(rt, frame.at(0x88u), t1);
            node = word(rt, frame.at(0x88u));
        }
        t1 = 0u;
    }
    ctx.gpr[9] = t1;
    ctx.gpr[2] = static_cast<u32>(signed_half(rt, frame.at(0x60u)));
    put_half(rt, object + 0xECu, ctx.gpr[2]);
}

} // namespace
} // namespace new_batch_main_133
} // namespace gt::game

#define NEW_MAIN133(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_133::name, gt::ReturnKind::kind)
NEW_MAIN133(0x08B6E920u, func_08B6E920, Float);
NEW_MAIN133(0x08B00E90u, func_08B00E90, Word);
