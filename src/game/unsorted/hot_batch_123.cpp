// Hot batch 123: functions of 400-700 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace gt::game {
namespace hot_batch_123 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline std::int32_t allegrex_div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}
} // namespace


// func_089223B8(self, target): one step of the camera shake applied to `target` (func_08B4F3B0 pitch,
// func_08B4F2E4 yaw, both in degrees). The car object (self+8)->0x244 is asked through its virtual slots: for each of
// its count (+0x8C) wheels that touch (+0x9C), the slope (+0xA4, folded around pi/2) and bump (+0xA8, weight 0.7 in mode
// 1, else 0.3) make a roughness sum; with a random draw below 0.5 (the generator func_08B1ED60(0x08CF28A8) /
// func_08B1EFF8) it feeds two random kicks filtered by func_08926A14 at self+0x10 / +0x3C, scaled by the speed (+0x14,
// /120 between 10 and 120). Mode 1 (self+0xC) adds the slip kicks (+0xAC / +0xB0 / +0xB4, filters +0xC0 / +0xEC),
// modes 1 and 2 the engine shake (+0xB8, filters func_08926EA0 at +0x118 / +0x174, weights 0.9 or 0.1), and mode 2
// the body sway: the orientation (+0x24) relative to self+0x1D0, slerped by the step at 0x08CDDD68, drives three pairs
// of filters (self+0x1E0 / +0x264, 0x2C apart, through func_08B67418 of the scaled g-forces) and is stored back.
void func_089223B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x110u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto car = [&] { return word(rt, word(rt, self + 8u) + 0x244u); };
    // `object->slot(...)`: lw v1, 0(object); lw v0, slot(v1); jalr v0
    const auto vcall = [&](std::uint32_t object, std::uint32_t slot, std::initializer_list<std::uint32_t> arguments,
                           std::initializer_list<float> float_arguments) {
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
        return call_setf(rt, ctx, ctx.gpr[2], arguments, float_arguments);
    };
    // the same with the table in v0 and the function in v1
    const auto vcall2 = [&](std::uint32_t object, std::uint32_t slot, std::initializer_list<std::uint32_t> arguments,
                            std::initializer_list<float> float_arguments) {
        ctx.gpr[2] = word(rt, object);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
        return call_setf(rt, ctx, ctx.gpr[3], arguments, float_arguments);
    };
    const auto random = [&] {
        const std::uint32_t generator = call_set(rt, ctx, 0x08B1ED60u, {0x08CF28A8u});
        return call_setf(rt, ctx, 0x08B1EFF8u, {generator}, {}).f0;
    };
    const auto copy4 = [&](std::uint32_t to, std::uint32_t from) {
        for (std::uint32_t k = 0u; k < 16u; k += 4u) put(rt, to + k, word(rt, from + k));
    };
    const float weight = word(rt, self + 0xCu) == 1u ? std::bit_cast<float>(0x3F333333u) : std::bit_cast<float>(0x3E99999Au);
    const float quarter_turn = std::bit_cast<float>(0x3FC90FDBu);
    float roughness = 0.0f;
    for (std::uint32_t index = 0u;; ++index) {
        const std::uint32_t count = vcall(car(), 0x8Cu, {car()}, {}).v0;
        ctx.gpr[5] = index;
        if (!(signed_of(index) < signed_of(count))) break;
        if (vcall(car(), 0x9Cu, {car(), index}, {}).v0 == 0u) continue;
        float slope = std::fabs(vcall(car(), 0xA4u, {car(), index}, {}).f0 / quarter_turn);
        if (1.0f < slope) slope = 2.0f - slope;
        slope = slope + roughness;
        const float bump = vcall(car(), 0xA8u, {car(), index}, {}).f0;
        roughness = slope + std::fabs(psp_mul(weight, bump));
    }
    float kick_x = 0.0f;
    float kick_y = 0.0f;
    if (random() < 0.5f) {
        kick_x = psp_mul(psp_mul(roughness, random() - 0.5f), 250.0f);
        kick_y = psp_mul(psp_mul(roughness, random() - 0.5f), 250.0f);
    }
    const float ground_x = call_setf(rt, ctx, 0x08926A14u, {self + 0x10u}, {kick_x}).f0;
    const float ground_y = call_setf(rt, ctx, 0x08926A14u, {self + 0x3Cu}, {kick_y}).f0;
    const float speed = vcall(car(), 0x14u, {car()}, {}).f0;
    float scale = std::bit_cast<float>(0x3DAAAAABu);
    if (!(speed < 10.0f)) scale = 120.0f < speed ? 1.0f : speed / 120.0f;
    const float yaw_ground = psp_mul(psp_mul(ground_y, 8.0f), scale);
    const float pitch_ground = psp_mul(ground_x + ground_x, scale);
    float pitch_slip = 0.0f;
    float yaw_slip = 0.0f;
    if (word(rt, self + 0xCu) == 1u) {
        vcall(car(), 0xACu, {car(), frame.at(0u), frame.at(4u)}, {});
        float ratio = 0.0f;
        if (!(f(frame.at(4u)) == 0.0f)) ratio = f(frame.at(0u)) / f(frame.at(4u));
        vcall(car(), 0xB0u, {car(), frame.at(8u), frame.at(0xCu), frame.at(0x10u), frame.at(0x14u), frame.at(0x18u)}, {});
        const float product = psp_mul(f(frame.at(0xCu)), f(frame.at(0x18u)));
        float slip = 0.0f;
        if (1.0f < vcall(car(), 0xB4u, {car()}, {}).f0) {
            slip = psp_mul(ratio, std::fabs(product / vcall(car(), 0xB4u, {car()}, {}).f0));
        }
        const float first = random();
        const float second = random();
        const float slip_x = psp_mul(slip, first - 0.5f);
        const float slip_y = psp_mul(slip, second - 0.5f);
        pitch_slip = psp_mul(call_setf(rt, ctx, 0x08926A14u, {self + 0xC0u}, {psp_mul(slip_x, 250.0f)}).f0,
                             std::bit_cast<float>(0x3CF5C28Fu));
        yaw_slip = psp_mul(call_setf(rt, ctx, 0x08926A14u, {self + 0xECu}, {psp_mul(slip_y, 250.0f)}).f0,
                           std::bit_cast<float>(0x3DCCCCCDu));
    }
    float pitch_engine = 0.0f;
    float yaw_engine = 0.0f;
    if (word(rt, self + 0xCu) != 0u) {
        float pitch_weight = std::bit_cast<float>(0x3F666666u);
        float yaw_weight = pitch_weight;
        const float engine = vcall(car(), 0xB8u, {car()}, {}).f0;
        if (word(rt, self + 0xCu) != 2u) {
            pitch_weight = 0.1f;
            yaw_weight = 0.1f;
        }
        const float first = random();
        const float second = random();
        const float shake_x = psp_mul(engine, first - 0.5f);
        const float shake_y = psp_mul(engine, second - 0.5f);
        pitch_engine = call_setf(rt, ctx, 0x08926EA0u, {self + 0x118u}, {psp_mul(pitch_weight, shake_x), 1.0f}).f0;
        yaw_engine = call_setf(rt, ctx, 0x08926EA0u, {self + 0x174u}, {psp_mul(yaw_weight, shake_y), 1.0f}).f0;
    }
    float pitch_sway = 0.0f;
    float yaw_sway = 0.0f;
    if (word(rt, self + 0xCu) == 2u) {
        const std::uint32_t object = car();
        const std::uint32_t rest = self + 0x1D0u;
        const std::uint32_t forces = frame.at(0x58u);
        vcall2(object, 0x24u, {forces, object}, {});
        copy4(frame.at(0x38u), forces);
        call_set(rt, ctx, 0x08B4A8D4u, {frame.at(0x38u), frame.at(0x38u), rest});
        copy4(frame.at(0x48u), frame.at(0x38u));
        call_setf(rt, ctx, 0x08B4A6D4u, {frame.at(0x48u)}, {f(0x08CDDD68u)});
        copy4(frame.at(0x78u), frame.at(0x48u));
        for (std::uint32_t axis = 0u; axis < 3u; ++axis) {
            const std::uint32_t force = forces + axis * 4u;
            const std::uint32_t turn = frame.at(0x78u) + axis * 4u;
            const std::uint32_t lean = frame.at(0x68u) + axis * 4u;
            const std::uint32_t twist = frame.at(0x88u) + axis * 4u;
            const float g = f(force) / std::bit_cast<float>(0x411CF5C3u);
            put_float(rt, lean, g);
            const float lean_value = call_setf(rt, ctx, 0x08B67418u, {}, {std::fabs(g) + 1.0f}).f0;
            put_float(rt, lean, lean_value);
            if (f(force) < 0.0f) put_float(rt, lean, -lean_value);
            const float twist_value = call_setf(rt, ctx, 0x08B67418u, {}, {psp_mul(std::fabs(f(turn)), 0.5f) + 1.0f}).f0;
            put_float(rt, twist, twist_value);
            if (f(turn) < 0.0f) put_float(rt, twist, -twist_value);
            const std::uint32_t first_filter = self + 0x1E0u + axis * 0x2Cu;
            const float sway = vcall(first_filter, 0x14u, {first_filter},
                                     {psp_mul(f(lean), std::bit_cast<float>(0x3E32B8C2u))}).f0;
            const std::uint32_t second_filter = self + 0x264u + axis * 0x2Cu;
            const float roll = vcall2(second_filter, 0x14u, {second_filter},
                                      {psp_mul(f(twist), std::bit_cast<float>(0x3CD67750u))}).f0;
            put_float(rt, frame.at(0x1Cu) + axis * 4u, sway + roll);
        }
        pitch_sway = f(frame.at(0x1Cu));
        yaw_sway = f(frame.at(0x20u));
        vcall2(object, 0x24u, {frame.at(0x28u), object}, {});
        put_float(rt, rest, f(frame.at(0x28u)));
        put_float(rt, rest + 0xCu, f(frame.at(0x34u)));
        put_float(rt, rest + 4u, f(frame.at(0x2Cu)));
        put_float(rt, rest + 8u, f(frame.at(0x30u)));
    }
    const float degree = std::bit_cast<float>(0x3C8EFA35u);
    call_setf(rt, ctx, 0x08B4F3B0u, {target}, {psp_mul(pitch_sway + (pitch_engine + (pitch_slip + pitch_ground)), degree)});
    call_setf(rt, ctx, 0x08B4F2E4u, {target}, {psp_mul(yaw_sway + (yaw_engine + (yaw_slip + yaw_ground)), degree)});
}


// func_08B6F518(a0:a1 = n, a2:a3 = d) -> v0:v1: signed 64-bit division, libgcc's divdi3 shape over the same unsigned
// core as func_08B702B4: the magnitudes are divided and the quotient negated when exactly one sign is negative; a zero
// divisor takes the core's `d0 = 1 / d0` step, which the Allegrex divu answers with 0xFFFFFFFF.
void func_08B6F518(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    std::uint64_t n = (std::uint64_t{ctx.gpr[5]} << 32) | ctx.gpr[4];
    std::uint64_t d = (std::uint64_t{ctx.gpr[7]} << 32) | ctx.gpr[6];
    bool negative = false;
    if (signed_of(ctx.gpr[5]) < 0) {
        negative = !negative;
        n = 0u - n;
    }
    if (signed_of(ctx.gpr[7]) < 0) {
        negative = !negative;
        d = 0u - d;
    }
    std::uint64_t q = n / (d != 0u ? d : 0xFFFFFFFFu);
    if (negative) q = 0u - q;
    ctx.gpr[2] = static_cast<std::uint32_t>(q);
    ctx.gpr[3] = static_cast<std::uint32_t>(q >> 32);
}


// func_08C28D70(tree, hint, value) -> iterator: libstdc++'s insert_unique with a position hint for a red-black tree
// (header at tree+4: leftmost +0xC, rightmost +0x10, count +0x14; node key at +0x10) whose keys are script values.
// `less(a, b)` asks the comparator object (tree->0)->0x2C: both values are copied into a two-value argument array
// (func_08A76AA0 / kAssign on the stack), its slot 0xCC called with (result, 2, arguments) and the result's slot 0x30
// read as the answer; the temporaries are destroyed in reverse (kDestroy), and a missing object reports
// func_08A5BB7C(0x08CB15A0). The cases are libstdc++'s: append after the rightmost, insert before the hint or after it
// (func_08B5F174 / func_08B5F100 step the iterator), keep the hint for an equal key, else the plain insert_unique
// func_08C28AC0; nodes are linked by func_08C28918(tree, at, parent, value).
void func_08C28D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4];
    const std::uint32_t hint = ctx.gpr[5];
    const std::uint32_t value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t result = frame.at(0u);
    const std::uint32_t arguments = frame.at(0x1Cu);
    const auto checked = [&](std::uint32_t object) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
    };
    const auto less = [&](std::uint32_t left, std::uint32_t right) {
        const std::uint32_t comparator_owner = word(rt, tree);
        call_set(rt, ctx, 0x08A76AA0u, {result});
        call_set(rt, ctx, 0x08A76AA0u, {arguments});
        call_set(rt, ctx, 0x08A76AA0u, {arguments + 4u});
        if (arguments != left) call_set(rt, ctx, kAssign, {arguments, word(rt, left)});
        if (arguments + 4u != right) call_set(rt, ctx, kAssign, {arguments + 4u, word(rt, right)});
        const std::uint32_t comparator = word(rt, comparator_owner + 0x2Cu);
        checked(comparator);
        ctx.gpr[2] = word(rt, comparator);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + 0xCCu);
        call_set(rt, ctx, ctx.gpr[3], {comparator, result, 2u, arguments});
        const std::uint32_t answer_object = word(rt, result);
        checked(answer_object);
        ctx.gpr[3] = word(rt, answer_object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x30u);
        const std::uint32_t answer = call_set(rt, ctx, ctx.gpr[2], {answer_object});
        call_set(rt, ctx, kDestroy, {arguments + 4u});
        call_set(rt, ctx, kDestroy, {arguments});
        call_set(rt, ctx, kDestroy, {result});
        return answer != 0u;
    };
    const auto link = [&](std::uint32_t at, std::uint32_t parent) {
        ctx.gpr[2] = call_set(rt, ctx, 0x08C28918u, {tree, at, parent, value});
    };
    const auto insert_plain = [&] { ctx.gpr[2] = call_set(rt, ctx, 0x08C28AC0u, {tree, value}); };
    if (hint == tree + 4u) {
        if (word(rt, tree + 0x14u) != 0u && less(word(rt, tree + 0x10u) + 0x10u, value)) return link(0u, word(rt, tree + 0x10u));
        return insert_plain();
    }
    if (less(value, hint + 0x10u)) {
        if (word(rt, tree + 0xCu) == hint) return link(hint, hint);
        const std::uint32_t before = call_set(rt, ctx, 0x08B5F174u, {hint, hint});
        if (!less(before + 0x10u, value)) return insert_plain();
        if (word(rt, before + 0xCu) != 0u) return link(hint, hint);
        return link(0u, before);
    }
    if (!less(hint + 0x10u, value)) {
        ctx.gpr[2] = hint;
        return;
    }
    if (word(rt, tree + 0x10u) == hint) return link(0u, hint);
    const std::uint32_t after = call_set(rt, ctx, 0x08B5F100u, {hint});
    if (!less(value, after + 0x10u)) return insert_plain();
    if (word(rt, hint + 0xCu) != 0u) return link(after, after);
    link(0u, hint);
}


// func_0889F3A8(self, source, extended): fills self from the getters of `source` (virtual slots) after
// func_088CDC48: an id pair (+0x54 -> +0x50/+0x54), thirteen floats (+0x58..), two halves (+0xAB4/+0xAB6) and the packed
// flag words +0xAB8 / +0xABC (one getter per bit field), a vector pair (+0x2C into the stack, stored with x negated at
// +0x90..), then up to 4 / 4 / 4 / 4 / 32 / 8 entries whose counts are packed into +0xABC: floats +0xB8 (+0x74) and +0xC8
// (+0x9C), records +0x138 (+0xA8), parts +0x178 / +0x458 copied by func_088A84B0 / func_088CDD7C (+0x40 / +0x44) with
// floats +0x4E8 (+0xA4), 32-byte entries +0x4F0 (+0xF8) and 48-byte blocks +0x8F8 from the sub-object at source+4; a
// byte +0xAC0 (+0x100), +0x8C into +0xA0, +0x110 into +0xA78, and with `extended` the 24-bit +0xAC0 field (+0x108), byte
// +0xAC4 (+0x10C) and the 16-character name +0xAC5 (+0x104 through func_08B7C830).
void func_0889F3A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    const std::uint32_t extended = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x70u);
    call_set(rt, ctx, 0x088CDC48u, {});
    // object->slot(arguments), the table left in register `table_register` as the original loads it
    const auto method = [&](std::uint32_t object, std::uint32_t slot, std::initializer_list<std::uint32_t> arguments,
                            unsigned table_register) {
        const std::uint32_t table = word(rt, object);
        ctx.gpr[table_register] = table;
        return call_setf(rt, ctx, word(rt, table + slot), arguments, {});
    };
    const auto get = [&](std::uint32_t slot, unsigned table_register = 3u) { return method(source, slot, {source}, table_register); };
    const auto insert = [&](std::uint32_t address, std::uint32_t value, unsigned position, unsigned size) {
        const std::uint32_t mask = (size == 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << position;
        put(rt, address, (word(rt, address) & ~mask) | ((value << position) & mask));
    };
    const auto field = [&](std::uint32_t address, unsigned position, unsigned size) {
        return (word(rt, address) >> position) & ((1u << size) - 1u);
    };
    const auto capped = [](std::uint32_t value, std::int32_t limit) {
        return signed_of(value) < limit ? value : static_cast<std::uint32_t>(limit);
    };
    const GuestResult id = get(0x54u);
    put(rt, self + 0x50u, id.v0);
    put(rt, self + 0x54u, id.v1);
    constexpr std::uint32_t kFloats[][2] = {{0x58u, 0x58u}, {0x5Cu, 0x5Cu}, {0x60u, 0x60u}, {0x64u, 0x64u}, {0x68u, 0x68u},
                                            {0x6Cu, 0x6Cu}, {0x70u, 0x70u}, {0x7Cu, 0x74u}, {0x90u, 0x78u}, {0xE0u, 0x7Cu},
                                            {0xE4u, 0x80u}, {0xE8u, 0x84u}, {0xECu, 0x88u}};
    for (const auto &entry : kFloats) put_float(rt, self + entry[1], get(entry[0]).f0);
    put16(rt, self + 0xAB4u, get(0x80u).v0);
    put16(rt, self + 0xAB6u, get(0x84u).v0);
    const std::uint32_t flags = self + 0xAB8u;
    const std::uint32_t counts = self + 0xABCu;
    insert(flags, get(0x98u).v0, 0u, 10u);
    insert(flags, get(0x94u, 5u).v0, 25u, 1u);
    insert(flags, get(0xB4u, 5u).v0, 10u, 4u);
    insert(flags, get(0xB8u, 5u).v0, 14u, 7u);
    insert(flags, get(0xBCu, 5u).v0, 21u, 4u);
    for (unsigned bit = 26u; bit < 32u; ++bit) insert(flags, get(0xC0u + (bit - 26u) * 4u, 5u).v0, bit, 1u);
    insert(counts, get(0xD8u, 5u).v0, 0u, 1u);
    insert(counts, get(0xDCu, 5u).v0, 1u, 1u);
    insert(counts, get(0xFCu, 5u).v0, 2u, 1u);
    put_float(rt, self + 0x8Cu, get(0xF0u).f0);
    method(source, 0x2Cu, {source, frame.at(0x30u), frame.at(0x3Cu)}, 3u);
    put_float(rt, self + 0x98u, float_at(rt, frame.at(0x3Cu)));
    put_float(rt, self + 0x94u, float_at(rt, frame.at(0x44u)));
    put_float(rt, self + 0x90u, -float_at(rt, frame.at(0x38u)));
    put_float(rt, self + 0x9Cu, float_at(rt, frame.at(0x40u)));
    insert(counts, capped(get(0x78u).v0, 4), 3u, 3u);
    for (std::uint32_t index = 0u; index < field(counts, 3u, 3u); ++index) {
        put_float(rt, self + 0xB8u + index * 4u, method(source, 0x74u, {source, index}, 3u).f0);
    }
    insert(counts, capped(get(0xA0u).v0, 4), 6u, 3u);
    for (std::uint32_t index = 0u; index < field(counts, 6u, 3u); ++index) {
        put_float(rt, self + 0xC8u + index * 4u, method(source, 0x9Cu, {source, index}, 3u).f0);
    }
    insert(counts, capped(get(0xACu).v0, 4), 9u, 4u);
    for (std::uint32_t index = 0u; index < field(counts, 9u, 4u); ++index) {
        method(source, 0xA8u, {source, self + 0x138u + index * 0x10u, index}, 3u);
    }
    insert(counts, capped(get(0x3Cu).v0, 4), 13u, 3u);
    for (std::uint32_t index = 0u; index < field(counts, 13u, 3u); ++index) {
        const std::uint32_t part = method(source, 0x40u, {source, index}, 3u).v0;
        if (part != 0u) call_set(rt, ctx, 0x088A84B0u, {self + 0x178u + index * 0xB8u, part});
        const std::uint32_t shape = method(source, 0x44u, {source, index}, 3u).v0;
        if (shape != 0u) call_set(rt, ctx, 0x088CDD7Cu, {self + 0x458u + index * 0x24u, shape});
        put_float(rt, self + 0x4E8u + index * 4u, method(source, 0xA4u, {source, index}, 3u).f0);
    }
    insert(counts, capped(get(0xF4u).v0, 0x20), 16u, 6u);
    for (std::uint32_t index = 0u; index < field(counts, 16u, 6u); ++index) {
        const std::uint32_t entry = self + 0x4F0u + index * 0x20u;
        put8(rt, entry + 0x24u, method(source, 0xF8u, {source, index, entry + 8u, entry + 0x18u}, 3u).v0);
    }
    const std::uint32_t sub = source + 4u;
    insert(counts, capped(method(sub, 8u, {sub}, 3u).v0, 8), 22u, 4u);
    for (std::uint32_t index = 0u; index < field(counts, 22u, 4u); ++index) {
        const std::uint32_t block = method(sub, 0xCu, {sub, index}, 3u).v0;
        const std::uint32_t to = self + 0x8F8u + index * 0x30u;
        for (std::uint32_t offset = 0u; offset < 0x30u; ++offset) put8(rt, to + offset, byte_u(rt, block + offset));
    }
    put8(rt, self + 0xAC0u, get(0x100u).v0);
    method(source, 0x8Cu, {source, self + 0xA0u}, 2u);
    method(source, 0x110u, {source, self + 0xA78u}, 6u);
    if (extended == 0u) return;
    insert(self + 0xAC0u, get(0x108u).v0, 8u, 24u);
    put8(rt, self + 0xAC4u, get(0x10Cu, 5u).v0);
    const std::uint32_t name = get(0x104u).v0;
    call_set(rt, ctx, 0x08B7C830u, {self + 0xAC5u, name, 0x10u});
}


// func_0889BAB0(self, scene): draws the car model self+4 (`car`; its body car->0x20->8->0x68, model list `list`): only
// when func_08899B38(car->0xB04) allows it and the visibility (car->0xB04 slot 0x48) is positive (func_08899BE4 below
// 0.9). Its transform (self slot 0x10 into the stack), the scene's two light queries func_08AF27C0 and the colours
// (func_08BAC0D0) set up a light object (stack, table 0x08CC23D0, given to the second light's slot 8). The ground plane
// (car->0xB04 slot 0x20) gives the shadow: projected through func_08AF52E4 when in front, drawn flattened with the
// shadow matrices (func_08B56364 .. func_08AFA170 pass 1), then the body pass 2 in the car's colours (func_0889B810).
// The scene's fog (func_08AF2924 / func_0889A19C) picks the final pass; the four wheels' dirty flags are set, and with
// self+8 and a reflection list car->0x68 the environment pass (func_08AF4B4C, blend states func_08B55AAC.., passes 3
// and 4) runs, else pass 0 or 5. The state is restored (func_08AF7DE8, func_08B54FB8(0x71)) and the light released
// (slot 0xC).
void func_0889BAB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t scene = ctx.gpr[5];
    GuestFrame frame(ctx, 0x180u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto car = [&] { return word(rt, self + 4u); };
    const auto vcall = [&](std::uint32_t object, std::uint32_t slot, std::initializer_list<std::uint32_t> arguments) {
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
        return call_setf(rt, ctx, ctx.gpr[2], arguments, {});
    };
    const auto call = [&](std::uint32_t function, std::initializer_list<std::uint32_t> arguments,
                          std::initializer_list<float> float_arguments = {}) {
        return call_setf(rt, ctx, function, arguments, float_arguments);
    };
    if (call(0x08899B38u, {word(rt, car() + 0xB04u)}).v0 == 0u) return;
    const std::uint32_t body_owner = word(rt, car() + 0x20u);
    ctx.gpr[2] = body_owner;
    if (body_owner == 0u) return;
    const std::uint32_t body = word(rt, body_owner + 8u);
    if (body == 0u) return;
    const std::uint32_t list = word(rt, body + 0x68u);
    if (list == 0u) return;
    if (vcall(word(rt, car() + 0xB04u), 0x48u, {word(rt, car() + 0xB04u)}).f0 <= 0.0f) return;
    if (vcall(word(rt, car() + 0xB04u), 0x48u, {word(rt, car() + 0xB04u)}).f0 < 0.9f) call(0x08899BE4u, {});
    const std::uint32_t transform = frame.at(0x28u);
    {
        ctx.gpr[2] = word(rt, self);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x10u);
        call_set(rt, ctx, ctx.gpr[3], {self, transform});
    }
    const bool lit = call(0x08AF27C0u, {scene}).v0 != 0u;
    const std::uint32_t light = call(0x08AF27C0u, {scene}).v0;
    put(rt, frame.at(0x130u), 0u);
    const GuestResult tint = vcall(word(rt, car() + 0xB04u), 0x4Cu, {word(rt, car() + 0xB04u)});
    const std::uint32_t colour = frame.at(0x70u);
    const std::uint32_t white = frame.at(0x88u);
    put(rt, frame.at(0x20u), tint.v0);
    put(rt, frame.at(0x120u), tint.v0);
    put(rt, frame.at(0x24u), tint.v1);
    put(rt, frame.at(0x124u), tint.v1);
    put_float(rt, frame.at(0x14u), 0.0f);
    put_float(rt, frame.at(0x1Cu), 0.0f);
    put_float(rt, frame.at(0x18u), 0.0f);
    put_float(rt, frame.at(0x10u), 1.0f);
    call(0x08BAC0D0u, {colour, frame.at(0x1Cu), frame.at(0x18u), frame.at(0x14u), frame.at(0x10u)});
    for (std::uint32_t offset = 0u; offset < 0x10u; offset += 4u) put_float(rt, frame.at(offset), 1.0f);
    call(0x08BAC0D0u, {white, frame.at(0xCu), frame.at(8u), frame.at(4u), frame.at(0u)});
    const float tint_level = f(frame.at(0x20u));
    const std::uint32_t light_object = frame.at(0x40u);
    put_float(rt, frame.at(0x84u), 1.0f);
    put(rt, light_object, 0x08CC23D0u);
    put(rt, light_object + 8u, word(rt, frame.at(0x24u)));
    put(rt, light_object + 0xCu, colour);
    put_float(rt, frame.at(0x80u), 1.0f);
    put(rt, light_object + 4u, word(rt, frame.at(0x20u)));
    if (light != 0u) {
        put(rt, frame.at(0x130u), 1u);
        ctx.gpr[2] = word(rt, light);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + 8u);
        call_set(rt, ctx, ctx.gpr[3], {light, scene, light_object});
    }
    const std::uint32_t projection = frame.at(0x98u);
    put(rt, light_object, 0x08CC23E0u);
    call(0x08B56790u, {1u, projection, 0u, 0u});
    const std::uint32_t plane = frame.at(0x50u);
    vcall(word(rt, car() + 0xB04u), 0x20u, {word(rt, car() + 0xB04u), plane});
    const std::uint32_t point = frame.at(0x34u);
    put_float(rt, point + 8u, f(frame.at(0x30u)));
    put_float(rt, point, f(transform));
    put_float(rt, point + 4u,
              -((psp_mul(f(plane), f(transform)) + psp_mul(f(plane + 8u), f(frame.at(0x30u)))) + f(plane + 0xCu)) /
                  f(plane + 4u));
    const float depth = call(0x08AF52E4u, {plane, point, projection}).f0;
    const std::uint32_t matrix = frame.at(0xD8u);
    if (0.0f < depth && !(f(plane + 4u) == 0.0f)) {
        call(0x08B5894Cu, {}, {psp_mul(-depth, f(0x08CE2FE4u))});
        if (!(tint_level == 0.0f)) {
            call(0x08B569A0u, {});
            call(0x08B56364u, {}, {0.0f, std::bit_cast<float>(0x3C23D70Au), 0.0f});
            const std::uint32_t flat = frame.at(0x60u);
            call(0x08B55140u, {0x65u, 0xDu, flat});
            call(0x08AF5394u, {matrix, flat, plane});
            call(0x08B566BCu, {matrix});
            call(0x08B566BCu, {car() + 0x460u});
            call(0x08B52E00u, {}, {f(colour), f(colour + 4u), f(colour + 8u), f(colour + 0xCu)});
            call(0x08AFA170u, {word(rt, list), 1u, list, 0u});
            call(0x08B56940u, {});
        }
        call(0x08B569A0u, {});
        call(0x08B56364u, {}, {0.0f, std::bit_cast<float>(0x3C23D70Au), 0.0f});
        for (std::uint32_t offset = 0u; offset < 0x40u; offset += 4u) put(rt, matrix + offset, word(rt, 0x08CDD0C0u + offset));
        const float height = f(plane + 4u);
        put_float(rt, frame.at(0x10Cu), -f(plane + 0xCu) / height);
        put_float(rt, frame.at(0xDCu), -f(plane) / height);
        put_float(rt, frame.at(0xFCu), -f(plane + 8u) / height);
        call(0x08B566BCu, {matrix});
        call(0x08B566BCu, {car() + 0x460u});
        const float fade = vcall(word(rt, car() + 0xB04u), 0xECu, {word(rt, car() + 0xB04u)}).f0;
        call(0x08B52E00u, {}, {f(colour), f(colour + 4u), f(colour + 8u), psp_mul(fade, f(colour + 0x14u))});
        call(0x08AFA170u, {word(rt, list), 2u, list, 0u});
        call(0x08B52E00u, {}, {f(white), f(white + 4u), f(white + 8u), f(white + 0xCu)});
        call(0x0889B810u, {car(), 1u});
        call(0x08B52E00u, {}, {1.0f, 1.0f, 1.0f, 1.0f});
        call(0x08B56940u, {});
        call(0x08B5894Cu, {}, {0.0f});
    }
    call(0x08B569A0u, {});
    call(0x08B566BCu, {car() + 0x460u});
    bool foggy = true;
    if (call(0x08AF2924u, {scene}).v0 == 0u) {
        const std::uint32_t target = car();
        const float distance = call(0x08AF2B64u, {scene}).f0;
        foggy = call(0x0889A19Cu, {target, 1u}, {distance}).v0 != 0u;
    }
    call(0x0889B394u, {car(), word(rt, car() + 0xB04u)});
    if (lit) call(0x08B52DDCu, {}, {f(frame.at(0x80u)), f(frame.at(0x80u)), f(frame.at(0x80u)), 1.0f});
    const std::uint32_t wheels = word(rt, car() + 0x58u);
    const std::uint32_t flags = wheels != 0u ? word(rt, wheels + 0x10u) : 0u;
    put8(rt, car() + 0x149u, 1u);
    put8(rt, car() + 0x21Du, 1u);
    put8(rt, car() + 0x2F1u, 1u);
    put8(rt, car() + 0x3C5u, 1u);
    if (byte_u(rt, self + 8u) != 0u && word(rt, car() + 0x68u) != 0u) {
        call(0x08B56A00u, {1u});
        call(0x08B569A0u, {});
        call(0x08AF4B4Cu, {projection}, {0.1f, 100.0f});
        call(0x08B568DCu, {projection});
        call(0x08B56A00u, {0u});
        call(0x08B58960u, {}, {0.0f, std::bit_cast<float>(0x3C23D70Au)});
        call(0x08AFA170u, {word(rt, car() + 0x68u), 0u, list, 0u});
        call(0x08B5A838u, {}, {1.0f, 1.0f, 1.0f, 1.0f});
        constexpr std::uint32_t kStates[][2] = {{3u, 1u}, {4u, 1u}, {5u, 1u}, {6u, 1u}, {0u, 0u}, {8u, 0u}, {9u, 0u}};
        for (const auto &state : kStates) call(0x08B55AACu, {state[0], state[1]});
        const std::uint32_t texture = call(0x08B58E1Cu, {}).v0;
        call(0x08B55AF8u, {0u, texture, 1u, 0x200u});
        call(0x08B55924u, {0u, 0x200u, 0x200u});
        call(0x08B55820u, {}, {std::bit_cast<float>(0x3D72FC95u), std::bit_cast<float>(0xBD651EB8u)});
        for (std::uint32_t pass = 0u; pass < 2u; ++pass) {
            call(0x08B557E0u, {}, {pass == 0u ? std::bit_cast<float>(0x3E9E0000u) : std::bit_cast<float>(0x3F210000u),
                                   std::bit_cast<float>(0x3CE51EB8u)});
            call(0x08AFA170u, {word(rt, car() + 0x68u), pass + 3u, list, 0u});
        }
        if (flags != 0u && (half_u(rt, flags) & 0x40u) != 0u) call(0x0889A3C0u, {car()});
        call(0x08B58960u, {}, {0.0f, 1.0f});
        call(0x08B56A00u, {1u});
        call(0x08B56940u, {});
        call(0x08B56A00u, {0u});
    } else {
        call(0x08AFA170u, {word(rt, list), foggy ? 5u : 0u, list, 0u});
        call(0x0889A3C0u, {car()});
        call(0x08B52E00u, {}, {f(white), f(white + 4u), f(white + 8u), f(white + 0xCu)});
        call(0x0889B810u, {car(), 0u});
    }
    call(0x08B56940u, {});
    if (lit) call(0x08B52DDCu, {}, {1.0f, 1.0f, 1.0f, 1.0f});
    call(0x08AF7DE8u, {0u});
    call(0x08B54FB8u, {0x71u});
    ctx.gpr[2] = word(rt, frame.at(0x130u));
    if (ctx.gpr[2] != 0u) vcall(light, 0xCu, {light, scene});
}


// func_089FBC08(self, parent): lays out the children of a box widget (func_089673F4; otherwise func_08A01C30 only).
// The box's padding (func_08A0D048: left, right, top, bottom) and size (func_0896A35C) give the free area; each child
// accepted by func_08A00448 reads its position, size (func_0896B7AC), margins (func_08966B20), fill (func_0896726C),
// stretch (func_089672C8), alignment (func_08967380) and spacing (func_08966C70) into the stack, learns from
// func_089FBB78 whether the next sibling (func_08968A38) shares half the gap, and is placed by the box's direction
// (func_089670A0): 0 top to bottom, 1 left to right, 2 right to left, 3 bottom to top; on the stacking axis the cursor
// advances by the child's extent and gap, on the other the child is stretched, aligned or kept inside the area.
// func_0896A9C4 applies what changed (func_08966FB0 .. func_08967064 say which) and func_08A01B7C finishes the child.
void func_089FBC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t parent = ctx.gpr[5];
    GuestFrame frame(ctx, 0x90u);
    if (call_set(rt, ctx, 0x089673F4u, {self}) == 0u) {
        call_set(rt, ctx, 0x08A01C30u, {self, parent});
        return;
    }
    const std::uint32_t x = frame.at(8u);
    const std::uint32_t y = frame.at(0xCu);
    const std::uint32_t w = frame.at(0x10u);
    const std::uint32_t h = frame.at(0x14u);
    const auto fl = [&](std::uint32_t offset) { return float_at(rt, frame.at(offset)); };
    const auto in = [&](std::uint32_t offset) { return static_cast<float>(signed_of(word(rt, frame.at(offset)))); };
    const auto sum = [&](std::uint32_t a, std::uint32_t b) {
        return static_cast<float>(signed_of(word(rt, frame.at(a)) + word(rt, frame.at(b))));
    };
    const auto flag = [&](std::uint32_t offset) { return byte_u(rt, frame.at(offset)) != 0u; };
    call_set(rt, ctx, 0x08A0D048u, {self, frame.at(0x28u), frame.at(0x2Cu), frame.at(0x30u), frame.at(0x34u)});
    call_set(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, w, h});
    float right = float_at(rt, w) - in(0x2Cu);
    float bottom = float_at(rt, h) - in(0x34u);
    float left = in(0x28u);
    float top = in(0x30u);
    // the cross axis: stretched (fill + stretch), aligned in the free span (fill), or kept inside the area
    const auto cross_x = [&](float child_w) {
        if (flag(0u)) {
            const float span = right - left;
            const float slack = span - child_w;
            const float at = left + in(0x28u);
            put_float(rt, x, at);
            if (flag(2u)) {
                put_float(rt, w, span - sum(0x28u, 0x2Cu));
            } else {
                put_float(rt, x, at + psp_mul(slack, fl(0x18u)));
            }
            return;
        }
        if (0.0f < (right - left) - child_w) {
            const float low = left + in(0x28u);
            if (float_at(rt, x) < low) {
                put_float(rt, x, low);
            } else {
                const float high = (right - in(0x2Cu)) - float_at(rt, w);
                if (high < float_at(rt, x)) put_float(rt, x, high);
            }
        } else {
            put_float(rt, x, left + in(0x28u));
        }
    };
    const auto cross_y = [&](float child_h) {
        if (flag(1u)) {
            const float span = bottom - top;
            const float slack = span - child_h;
            const float at = top + in(0x30u);
            put_float(rt, y, at);
            if (flag(3u)) {
                put_float(rt, h, span - sum(0x30u, 0x34u));
            } else {
                put_float(rt, y, at + psp_mul(slack, fl(0x1Cu)));
            }
            return;
        }
        if (0.0f < (bottom - top) - child_h) {
            const float low = top + in(0x30u);
            if (float_at(rt, y) < low) {
                put_float(rt, y, low);
            } else {
                const float high = (bottom - in(0x34u)) - float_at(rt, h);
                if (high < float_at(rt, y)) put_float(rt, y, high);
            }
        } else {
            put_float(rt, y, top + in(0x30u));
        }
    };
    for (std::uint32_t child = word(rt, self + 0xF4u); child != 0u; child = call_set(rt, ctx, 0x08968A38u, {child})) {
        if (call_set(rt, ctx, 0x08A00448u, {self, child}) == 0u) {
            call_set(rt, ctx, 0x08A01B7Cu, {self, parent, child});
            continue;
        }
        call_set(rt, ctx, 0x0896A35Cu, {child, x, y, 0u, 0u});
        call_set(rt, ctx, 0x0896B7ACu, {child, w, h});
        call_set(rt, ctx, 0x08966B20u, {child, frame.at(0x28u), frame.at(0x2Cu), frame.at(0x30u), frame.at(0x34u)});
        const float child_h = sum(0x30u, 0x34u) + fl(0x14u);
        const float child_w = sum(0x28u, 0x2Cu) + fl(0x10u);
        call_set(rt, ctx, 0x0896726Cu, {child, frame.at(0u), frame.at(1u)});
        call_set(rt, ctx, 0x089672C8u, {child, frame.at(2u), frame.at(3u)});
        call_set(rt, ctx, 0x08967380u, {child, frame.at(0x18u), frame.at(0x1Cu)});
        call_set(rt, ctx, 0x08966C70u, {child, frame.at(0x20u), frame.at(0x24u)});
        put8(rt, frame.at(4u), 0u);
        put8(rt, frame.at(5u), 0u);
        const std::uint32_t next = call_set(rt, ctx, 0x08968A38u, {child});
        call_set(rt, ctx, 0x089FBB78u, {self, next, frame.at(4u), frame.at(5u)});
        const std::uint32_t direction = call_set(rt, ctx, 0x089670A0u, {child});
        if (direction == 0u || direction == 3u) {
            cross_x(child_w);
            float gap = 0.0f;
            if (flag(1u)) {
                gap = (bottom - top) - fl(0x24u);
                if (flag(5u)) gap = psp_mul(gap, 0.5f);
                const float at = direction == 0u ? top + in(0x30u) : (top + (fl(0x24u) - child_h)) + in(0x30u);
                put_float(rt, y, at);
                if (flag(3u)) {
                    put_float(rt, h, gap - sum(0x30u, 0x34u));
                } else {
                    put_float(rt, y, at + psp_mul(gap, fl(0x1Cu)));
                }
            } else {
                put_float(rt, y, direction == 0u ? top + in(0x30u) : (bottom - child_h) + in(0x30u));
            }
            if (direction == 0u) {
                top = top + (gap + child_h);
            } else {
                bottom = bottom - (gap + child_h);
            }
        } else if (direction == 1u || direction == 2u) {
            float gap = 0.0f;
            if (flag(0u)) {
                gap = (right - left) - fl(0x20u);
                if (flag(4u)) gap = psp_mul(gap, 0.5f);
                const float at = direction == 1u ? left + in(0x28u) : (left + (fl(0x20u) - child_w)) + in(0x28u);
                put_float(rt, x, at);
                if (flag(2u)) {
                    put_float(rt, w, gap - sum(0x28u, 0x2Cu));
                } else {
                    put_float(rt, x, at + psp_mul(gap, fl(0x18u)));
                }
            } else {
                put_float(rt, x, direction == 1u ? left + in(0x28u) : (right - child_w) + in(0x28u));
            }
            cross_y(child_h);
            if (direction == 1u) {
                left = left + (gap + child_w);
            } else {
                right = right - (gap + child_w);
            }
        }
        const std::uint32_t move_x = call_set(rt, ctx, 0x08966FB0u, {child}) != 0u ? x : 0u;
        const std::uint32_t move_y = call_set(rt, ctx, 0x08966FECu, {child}) != 0u ? y : 0u;
        const std::uint32_t size_w = call_set(rt, ctx, 0x08967028u, {child}) != 0u ? w : 0u;
        const std::uint32_t size_h = call_set(rt, ctx, 0x08967064u, {child}) != 0u ? h : 0u;
        call_set(rt, ctx, 0x0896A9C4u, {child, move_x, move_y, size_w, size_h});
        call_set(rt, ctx, 0x08A01B7Cu, {self, parent, child});
    }
}


// func_089193B8(self, car, unused, cut): plays the replay camera self along its current shot. Without a shot list
// self+0x400 or with none active the shot comes from func_089190BC (when func_088DCD90(car, cut) and self+0x1F8
// allow); a negative `cut` with no timer self+0x1FC instead resets the chase camera self+0x204 (func_088D2BC8 /
// func_0891A6A0 / func_0892632C). A new shot (vs self+0x200) restarts it (func_08918BB8, random sway self+0x3F8). The
// progress through the shot (timer, frame count self+0x1F4 or fixed by the shot's mode bits 0x30) samples the shot's
// curves (func_08907A44 with the smoothing byte 0x08CF212C, func_08918CDC): position, two spring pairs (filtered by
// func_08926384 / func_0892690C), the target (or the car's own, slot 0x38, raised by 0x08CDDBB4), roll and field of
// view (fitted to the target's distance with bit 0x100). The view angles (pitch +0x60, yaw +0x64, roll +0x68, fov
// +0x80) follow the spring filters for tracking shots (bits 0xC; mode 4 leads the pitch through func_089260FC), get the
// sway, then the far plane +0x84, lens +0x8C, view matrix self+0x10 (func_08B4E648 .. func_08B4F47C) and the listener
// self+0x1E4 are rebuilt; +0x410 gets the flags of 0x08C8A87C (bit 0x80 with shot bit 0x1000).
void func_089193B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t car = ctx.gpr[5];
    const std::uint32_t cut = ctx.gpr[7];
    GuestFrame frame(ctx, 0x160u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto call = [&](std::uint32_t function, std::initializer_list<std::uint32_t> arguments,
                          std::initializer_list<float> float_arguments = {}) {
        return call_setf(rt, ctx, function, arguments, float_arguments);
    };
    constexpr float kDegree = std::bit_cast<float>(0x3C8EFA35u);
    constexpr float kPi = std::bit_cast<float>(0x40490FDBu);
    constexpr float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
    const std::uint32_t director = call(0x088DCD90u, {car, cut}).v0;
    for (std::uint32_t offset : {0x64u, 0x68u, 0x6Cu, 0x70u, 0x84u, 0x88u, 0x8Cu, 0x90u, 0x94u, 0x98u, 0x9Cu, 0xA0u}) {
        put(rt, sp(offset), 0u);
    }
    std::uint32_t shot = 0u;
    bool have_shot = false;
    const std::uint32_t list = word(rt, self + 0x400u);
    ctx.gpr[2] = list;
    if (list != 0u) {
        const std::uint32_t timer = word(rt, self + 0x1FCu);
        if (timer == 0u) {
            if (signed_of(cut) < 0) {
                const std::uint32_t chase = word(rt, self + 0x204u);
                put(rt, chase + 8u, car);
                put_float(rt, self + 0x208u, f(self + 0x208u) + 0.2f);
                const float speed = call(0x088D2BC8u, {word(rt, car + 0x61Cu)}, {f(self + 0x208u)}).f0;
                put_float(rt, self + 0x80u, 38.0f);
                put_float(rt, self + 0x208u, speed);
                call(0x0891A6A0u, {word(rt, self + 0x204u)}, {speed, 0.0f});
                call(0x0892632Cu, {word(rt, self + 0x204u), self + 0x10u});
                return;
            }
        } else {
            const float now = f(self + 0x40Cu) + f(timer + 0x14u);
            put_float(rt, sp(0u), now);
            shot = call(0x08918C84u, {self}, {now}).v0;
            put_float(rt, self + 0x40Cu, f(self + 0x40Cu) + 0.5f);
            const std::uint32_t span = word(rt, self + 0x1FCu);
            if (f(span + 0x18u) - f(span + 0x14u) < f(self + 0x40Cu)) put(rt, self + 0x40Cu, 0u);
            ctx.gpr[2] = word(rt, self + 0x400u);
            have_shot = ctx.gpr[2] != 0u;
        }
    }
    if (!have_shot) {
        if (director == 0u) return;
        ctx.gpr[2] = word(rt, self + 0x1F8u);
        if (ctx.gpr[2] == 0u) return;
        shot = call(0x089190BCu, {self, car, cut, sp(0u)}).v0;
    }
    if (shot == 0u) {
        put8(rt, self + 0xCCu, 0u);
        return;
    }
    bool refresh = false;
    bool fresh = false;
    if (shot != word(rt, self + 0x200u)) {
        put(rt, self + 0x200u, shot);
        call(0x08918BB8u, {self});
        put8(rt, self + 0xC0u, 1u);
        const std::uint32_t generator = call(0x08B1ED60u, {0x08CF28A8u}).v0;
        put_float(rt, self + 0x3F8u, call(0x08B1EFF8u, {generator}).f0);
        refresh = true;
        fresh = true;
    }
    if (byte_u(rt, self + 0x3FCu) == 1u) {
        put8(rt, self + 0x3FCu, 0u);
        refresh = true;
    }
    float t;
    const std::uint32_t timing = word(rt, shot) & 0x30u;
    if (timing == 0u) {
        t = (f(sp(0u)) - f(shot + 4u)) / (f(shot + 8u) - f(shot + 4u));
    } else if (timing == 0x10u) {
        t = psp_mul(static_cast<float>(signed_of(word(rt, self + 0x1F4u))) / f(shot + 8u), 60.0f);
    } else {
        t = 0.0f;
    }
    put8(rt, self + 0x408u, 0u);
    if (t < 0.0f) put8(rt, self + 0x408u, 1u);
    if (1.0f < t) put8(rt, self + 0x408u, 1u);
    if (t < 0.0f) {
        t = 0.0f;
    } else if (1.0f < t) {
        t = 1.0f;
    }
    if (fresh && 0.5f < t) fresh = false;
    const auto curve = [&](std::uint32_t offset) {
        return call(0x08907A44u, {word(rt, shot + offset), byte_u(rt, 0x08CF212Cu)}, {t}).f0;
    };
    const float px = curve(0x34u);
    const float py = curve(0x38u);
    const float pz = curve(0x3Cu);
    put_float(rt, sp(0x6Cu), pz);
    put_float(rt, sp(0x64u), px);
    put_float(rt, sp(0x68u), py);
    put(rt, sp(0x70u), 0u);
    call(0x08918CDCu, {sp(0x54u), self, shot + 0x64u, 2u}, {t});
    for (std::uint32_t k = 0u; k < 16u; k += 4u) put(rt, sp(0x84u) + k, word(rt, sp(0x54u) + k));
    call(0x08918CDCu, {sp(0x44u), self, shot + 0x50u, 2u}, {t});
    for (std::uint32_t k = 0u; k < 16u; k += 4u) put(rt, sp(0x94u) + k, word(rt, sp(0x44u) + k));
    if (f(sp(0x84u)) < 0.0f) put_float(rt, sp(0x84u), 0.0f);
    if (f(sp(0x88u)) < 0.0f) put_float(rt, sp(0x88u), 0.0f);
    const float fov = curve(0x4Cu);
    const float fov_low = f(shot + 0x5Cu);
    const float fov_high = f(shot + 0x60u);
    const std::uint32_t springs[4] = {self + 0x31Cu, self + 0x348u, self + 0x374u, self + 0x3A0u};
    const std::uint32_t spring_values[4] = {sp(0x84u), sp(0x88u), sp(0x94u), sp(0x98u)};
    if (refresh) {
        for (int k = 0; k < 4; ++k) call(0x08926384u, {springs[k]}, {f(spring_values[k])});
    }
    for (int k = 0; k < 4; ++k) put_float(rt, spring_values[k], call(0x0892690Cu, {springs[k]}, {f(spring_values[k])}).f0);
    put_float(rt, self + 0x50u, f(sp(0x64u)));
    put_float(rt, self + 0x54u, f(sp(0x68u)));
    put_float(rt, self + 0x58u, f(sp(0x6Cu)));
    put_float(rt, self + 0x5Cu, f(sp(0x70u)));
    const std::uint32_t target = sp(0xA4u);
    const std::uint32_t mode = word(rt, shot);
    if (((mode & 0x200u) != 0u && fresh) || (mode & 0xCu) == 0u) {
        const float tx = curve(0x40u);
        const float ty = curve(0x44u);
        const float tz = curve(0x48u);
        put_float(rt, target + 8u, tz);
        put_float(rt, target, tx);
        put_float(rt, target + 4u, ty);
        put(rt, target + 0xCu, 0u);
    } else {
        ctx.gpr[3] = word(rt, director);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x38u);
        const std::uint32_t own = call_set(rt, ctx, ctx.gpr[2], {director});
        put_float(rt, target, f(own));
        put_float(rt, target + 4u, f(own + 4u) + f(0x08CDDBB4u));
        put_float(rt, target + 8u, f(own + 8u));
        put_float(rt, target + 0xCu, f(own + 0xCu));
    }
    if ((word(rt, shot) & 0x400u) != 0u) put_float(rt, self + 0x3F8u, 1.0f);
    const std::uint32_t view = self + 0x10u;
    call(0x08918D88u, {self, view, shot});
    const std::uint32_t offset_vector = sp(4u);
    for (std::uint32_t k = 0u; k < 16u; k += 4u) put(rt, offset_vector + k, word(rt, sp(0x64u) + k));
    call(0x08B4A8D4u, {offset_vector, offset_vector, target});
    put_float(rt, self + 0xA8u, std::sqrt(call(0x08B4A744u, {offset_vector}).f0));
    const std::uint32_t look = sp(0x34u);
    for (std::uint32_t k = 0u; k < 16u; k += 4u) put(rt, look + k, word(rt, target + k));
    call(0x08B4A8D4u, {look, look, sp(0x64u)});
    const std::uint32_t direction = sp(0x74u);
    for (std::uint32_t k = 0u; k < 16u; k += 4u) put(rt, direction + k, word(rt, look + k));
    const float distance = std::sqrt(call(0x08B4A744u, {direction}).f0);
    const float length = std::sqrt(call(0x08B4A744u, {direction}).f0);
    call(0x08B4A6A0u, {direction}, {1.0f / length});
    put_float(rt, self + 0x60u, call(0x08B66E88u, {}, {f(direction + 4u)}).f0);
    put_float(rt, self + 0x64u, call(0x08B66FBCu, {}, {f(direction), f(direction + 8u)}).f0 - kPi);
    const float sway = f(self + 0x3F8u);
    call(0x08918CDCu, {sp(0x24u), self, shot + 0x6Cu, 1u}, {t});
    put_float(rt, self + 0x68u, psp_mul(sway, psp_mul(f(sp(0x24u)), kDegree)));
    const float yaw = f(self + 0x64u);
    if (kPi < yaw) {
        put_float(rt, self + 0x64u, yaw - kTwoPi);
    } else if (yaw < -kPi) {
        put_float(rt, self + 0x64u, yaw + kTwoPi);
    }
    if ((word(rt, shot) & 0x100u) != 0u) {
        const float half = call(0x08B65F0Cu, {}, {1.0f / ((distance + distance) / fov)}).f0;
        const float degrees = psp_mul(half + half, std::bit_cast<float>(0x42652EE1u));
        put_float(rt, self + 0x80u, degrees);
        if (degrees < fov_low) put_float(rt, self + 0x80u, fov_low);
        float clamped = f(self + 0x80u);
        if (fov_high < clamped) {
            clamped = fov_high;
            put_float(rt, self + 0x80u, fov_high);
        }
        put_float(rt, self + 0x80u, call(0x08926140u, {}, {clamped}).f0);
    } else {
        put_float(rt, self + 0x80u, fov);
    }
    if ((word(rt, shot) & 0xCu) != 0u) {
        const std::uint32_t angles[4] = {self + 0x20Cu, self + 0x268u, self + 0x2C4u, self + 0x2F0u};
        const std::uint32_t fields[4] = {self + 0x60u, self + 0x64u, self + 0x68u, self + 0x80u};
        if (refresh) {
            for (int k = 0; k < 4; ++k) call(0x08926384u, {angles[k]}, {f(fields[k])});
        }
        put_float(rt, fields[0], call(0x08926EA0u, {angles[0]}, {f(fields[0]), f(sp(0x84u))}).f0);
        put_float(rt, fields[1], call(0x08926EA0u, {angles[1]}, {f(fields[1]), f(sp(0x88u))}).f0);
        put_float(rt, fields[2], call(0x0892690Cu, {angles[2]}, {f(fields[2])}).f0);
        put_float(rt, fields[3], call(0x0892690Cu, {angles[3]}, {f(fields[3])}).f0);
    }
    if ((word(rt, shot) & 0xCu) == 4u) {
        const float pitch = f(self + 0x60u);
        const float lead = call(0x089260FCu, {self}, {pitch, psp_mul(f(self + 0x80u), kDegree), f(0x08CDDBB0u)}).f0;
        put_float(rt, self + 0x60u, pitch + lead);
    }
    const float scale = f(0x08C8A88Cu);
    put_float(rt, self + 0x64u,
              f(self + 0x64u) + psp_mul(psp_mul(psp_mul(f(sp(0x94u)), f(self + 0x80u)), 0.5f), kDegree));
    put_float(rt, self + 0x60u,
              f(self + 0x60u) - psp_mul(psp_mul(psp_mul(f(sp(0x98u)), f(self + 0x80u)), 0.5f), kDegree));
    const float near_limit = f(self + 0x414u);
    const float angle = psp_mul(f(self + 0x80u), kDegree);
    const float far = psp_mul(near_limit, scale) / call(0x08B66604u, {}, {angle}).f0;
    put_float(rt, self + 0x84u, far);
    if (far < near_limit) put_float(rt, self + 0x84u, near_limit);
    if (1000.0f < f(self + 0x84u)) put_float(rt, self + 0x84u, 1000.0f);
    const float tangent = call(0x08B66604u, {}, {psp_mul(angle, 0.5f)}).f0;
    const float lens = psp_mul(f(0x08C8A888u) / (tangent + tangent), f(0x08C8A880u));
    put(rt, self + 0x8Cu, ctx.fpu_float_to_word(lens, 1u));
    call(0x08B4E648u, {view});
    const std::uint32_t matrix = sp(0xC0u);
    for (std::uint32_t k = 0u; k < 0x40u; k += 4u) put(rt, matrix + k, 0u);
    put_float(rt, matrix + 0x30u, f(self + 0x50u));
    put_float(rt, matrix + 0x34u, f(self + 0x54u));
    put_float(rt, matrix + 0x38u, f(self + 0x58u));
    put_float(rt, matrix, 1.0f);
    put_float(rt, matrix + 0x14u, 1.0f);
    put_float(rt, matrix + 0x28u, 1.0f);
    put_float(rt, matrix + 0x3Cu, 1.0f);
    call(0x08B4E9D4u, {view, matrix});
    call(0x08B4F3B0u, {view}, {f(self + 0x64u)});
    call(0x08B4F2E4u, {view}, {f(self + 0x60u)});
    call(0x08B4F47Cu, {view}, {-f(self + 0x68u)});
    put_float(rt, sp(0x20u), 1.0f);
    put(rt, sp(0x14u), 0u);
    put(rt, sp(0x18u), 0u);
    put(rt, sp(0x1Cu), 0u);
    call(0x08B4A774u, {self + 0x1E4u, view, sp(0x14u)});
    const std::uint32_t flags = word(rt, 0x08C8A87Cu);
    put(rt, self + 0x410u, flags);
    // the delay slot of the last branch: v0 = flags | 0x80 on both paths
    ctx.gpr[2] = flags | 0x80u;
    if ((word(rt, shot) & 0x1000u) != 0u) put(rt, self + 0x410u, ctx.gpr[2]);
}


// func_0894DD9C(world): one physics step for the world's cars (count world+8; func_0894CF40(world, i)->8 says the
// slot is live, func_0894CFF8(world, i) gives the car; constants = car+8). Per car: func_08948184, the controller
// world+0x10 (slots 0x1C active, 8 mode, 0xC input, 0x10 / 0x24 / 0x28 assists, 0x30 output) fills a stack input
// record (func_08948AE4 / func_08948B78) that drives the car through func_08952B68, func_08952A5C or func_089525A0 (kind
// 4 and mode 2), with the gear request +0x732 / +0x733 from func_0895DD80 below the controller's speed; then (except
// kind 4) the drive chain func_0894E9C0, func_08946EC0, func_0894A15C, func_0894E738, func_0894F6C8, func_0894B310, and
// func_08954FF8 / func_0893AE44. After the world hooks (+0x20: func_089590A4 / func_08958F20) and func_089596D4 per
// car, the recorder world+0x18 either stores each car's position, velocity, angles and step (slot 0x18 record, slot
// 0x1C commit) and plays them back (angles kept within +-4pi, a held car nudged above its floor), or the positions and
// heading integrate their velocities. Then the aids (func_08954B78 for state+0x52 = 8, func_08957C3C, func_08956930),
// func_08956EF4, the suspension trio (func_0893D188, func_08956DC8, func_0895F3BC), func_0895F7C0(world), and the
// outputs (func_0895EC4C, func_08960D4C, controller slot 0x30 with func_08949AF4, func_08957154).
void func_0894DD9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t world = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto count = [&] { return signed_of(word(rt, world + 8u)); };
    const auto live = [&](std::uint32_t index) { return word(rt, call_set(rt, ctx, 0x0894CF40u, {world, index}) + 8u) != 0u; };
    const auto car_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x0894CFF8u, {world, index}); };
    const auto vcall = [&](std::uint32_t object, std::uint32_t slot, std::initializer_list<std::uint32_t> arguments) {
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
        return call_setf(rt, ctx, ctx.gpr[2], arguments, {});
    };
    const auto each = [&](auto &&body) {
        for (std::uint32_t index = 0u; signed_of(index) < count(); ++index) {
            if (live(index)) body(index);
        }
    };
    const std::uint32_t input = frame.at(0x20u);
    for (std::uint32_t index = 0u; signed_of(index) < count(); ++index) {
        if (!live(index)) continue;
        const std::uint32_t car = car_of(index);
        const std::uint32_t constants = word(rt, car + 8u);
        call_set(rt, ctx, 0x08948184u, {car});
        put8(rt, constants + 0xDu, 0u);
        if (word(rt, world + 0x10u) != 0u && vcall(word(rt, world + 0x10u), 0x1Cu, {word(rt, world + 0x10u), index}).v0 != 0u) {
            put8(rt, constants + 0xDu, 1u);
        }
        call_setf(rt, ctx, 0x08958AD8u, {car}, {f(constants + 0x6C4u)});
        call_set(rt, ctx, 0x08948AE4u, {input});
        for (std::uint32_t offset = 4u; offset < 0x20u; offset += 4u) put(rt, frame.at(offset), 0u);
        put_float(rt, frame.at(0x18u), 1.0f);
        const std::uint32_t drive = word(rt, car + 0x14u);
        put_float(rt, drive + 8u, 1.0f);
        put8(rt, constants + 0xEu, 0u);
        const std::uint32_t controller = word(rt, world + 0x10u);
        const auto gear_request = [&] {
            vcall(word(rt, world + 0x10u), 0x24u, {word(rt, world + 0x10u), index, constants + 0x764u});
            put(rt, frame.at(0u), 0u);
            const float limit = vcall(word(rt, world + 0x10u), 0x28u, {word(rt, world + 0x10u), index, frame.at(0u)}).f0;
            std::uint32_t gear = 0xFFFFFFFFu;
            if (limit < f(constants + 0x6C4u)) gear = sext8(call_setf(rt, ctx, 0x0895DD80u, {car, 1u}, {limit}).v0);
            put8(rt, constants + 0x732u, gear);
            put8(rt, constants + 0x733u, word(rt, frame.at(0u)));
        };
        if (controller == 0u) {
            call_set(rt, ctx, 0x08952B68u, {car, input});
        } else {
            const std::uint32_t mode = vcall(controller, 8u, {controller, index}).v0;
            if (mode == 0u) {
                if (signed_of(sext8(byte_u(rt, constants + 3u))) == 4) {
                    call_set(rt, ctx, 0x089525A0u, {car});
                } else {
                    vcall(word(rt, world + 0x10u), 0xCu, {word(rt, world + 0x10u), index, input});
                    call_set(rt, ctx, 0x08948B78u, {input});
                    vcall(word(rt, world + 0x10u), 0x10u, {word(rt, world + 0x10u), index, frame.at(4u)});
                    gear_request();
                    call_set(rt, ctx, 0x08952B68u, {car, input});
                }
            } else if (mode == 1u) {
                vcall(word(rt, world + 0x10u), 0x10u, {word(rt, world + 0x10u), index, frame.at(4u)});
                const float grip = f(frame.at(0x18u));
                if (sext8(byte_u(rt, constants + 3u)) == 0u) {
                    put8(rt, constants + 0xEu, 1u);
                } else {
                    put_float(rt, drive + 8u, grip);
                    if (grip == 0.0f) put_float(rt, drive + 8u, 1.0f);
                }
                gear_request();
                call_set(rt, ctx, 0x08952A5Cu, {car, frame.at(4u)});
            } else if (mode == 2u) {
                call_set(rt, ctx, 0x089525A0u, {car});
            }
        }
        if (signed_of(sext8(byte_u(rt, word(rt, car + 8u) + 3u))) != 4) {
            for (std::uint32_t function : {0x0894E9C0u, 0x08946EC0u, 0x0894A15Cu, 0x0894E738u, 0x0894F6C8u, 0x0894B310u}) {
                call_set(rt, ctx, function, {car});
            }
        }
        call_set(rt, ctx, 0x08954FF8u, {car});
        call_set(rt, ctx, 0x0893AE44u, {car});
    }
    if (word(rt, world + 0x20u) != 0u) {
        call_set(rt, ctx, 0x089590A4u, {world});
        if (word(rt, world + 0x20u) != 0u) call_set(rt, ctx, 0x08958F20u, {world});
    }
    {
        std::int32_t total = count();
        for (std::uint32_t index = 0u; signed_of(index) < total; ++index) {
            if (!live(index)) continue;
            call_set(rt, ctx, 0x089596D4u, {world, index});
            total = count();
        }
    }
    const std::uint32_t recorder = word(rt, world + 0x18u);
    if (recorder != 0u) {
        for (std::uint32_t index = 0u; signed_of(index) < count(); ++index) {
            if (!live(index)) continue;
            const std::uint32_t car = car_of(index);
            const std::uint32_t constants = word(rt, car + 8u);
            const std::uint32_t record = vcall(word(rt, world + 0x18u), 0x18u, {word(rt, world + 0x18u), index}).v0;
            for (std::uint32_t k = 0u; k < 12u; k += 4u) {
                put_float(rt, record + k, f(constants + 0x10u + k));
                put_float(rt, record + 0xCu + k, f(constants + 0x6CCu + k));
            }
            put_float(rt, record + 0x20u, f(constants + 0x704u));
            put_float(rt, record + 0x1Cu, f(constants + 0x700u));
            put_float(rt, record + 0x18u, f(constants + 0x6FCu));
            put_float(rt, record + 0x2Cu, f(constants + 0x6F8u));
            put_float(rt, record + 0x28u, f(constants + 0x6F4u));
            put_float(rt, record + 0x24u, f(constants + 0x6F0u));
            put(rt, record + 0x30u, word(rt, constants + 0x690u));
            put(rt, record + 0x34u, word(rt, constants + 0x694u));
            put_float(rt, record + 0x38u, call_setf(rt, ctx, 0x0895549Cu, {car}, {}).f0);
        }
        vcall(word(rt, world + 0x18u), 0x1Cu, {word(rt, world + 0x18u)});
        constexpr float kFullTurn = std::bit_cast<float>(0x41490FDBu);
        for (std::uint32_t index = 0u; signed_of(index) < count(); ++index) {
            if (!live(index)) continue;
            const std::uint32_t car = car_of(index);
            const std::uint32_t constants = word(rt, car + 8u);
            const std::uint32_t record = vcall(word(rt, world + 0x18u), 0x18u, {word(rt, world + 0x18u), index}).v0;
            for (std::uint32_t k = 0u; k < 12u; k += 4u) {
                put_float(rt, constants + 0x10u + k, f(record + k));
                put_float(rt, constants + 0x6CCu + k, f(record + 0xCu + k));
            }
            put_float(rt, constants + 0x704u, f(record + 0x20u));
            put_float(rt, constants + 0x6F8u, f(record + 0x2Cu));
            // a0 still points 12 bytes into the record after the copy loop
            if (call_set(rt, ctx, 0x0894CCB8u, {record + 0xCu}) != 0u && call_set(rt, ctx, 0x089557ACu, {car}) != 0u &&
                f(constants + 0x18u) < f(constants + 0x48u)) {
                put(rt, constants + 0x6D4u, 0u);
                put_float(rt, constants + 0x18u, f(constants + 0x48u) + std::bit_cast<float>(0x38D1B718u));
            }
            for (std::uint32_t field : {0x6F8u, 0x6F4u, 0x6F0u}) {
                const float angle = f(constants + field);
                if (kFullTurn < angle) {
                    put_float(rt, constants + field, kFullTurn);
                } else if (angle < -kFullTurn) {
                    put_float(rt, constants + field, -kFullTurn);
                }
            }
        }
    } else {
        each([&](std::uint32_t index) {
            const std::uint32_t car = car_of(index);
            const std::uint32_t constants = word(rt, car + 8u);
            const float dt = call_setf(rt, ctx, 0x0895549Cu, {car}, {}).f0;
            for (std::uint32_t k = 0u; k < 12u; k += 4u) {
                put_float(rt, constants + 0x10u + k, f(constants + 0x10u + k) + psp_mul(dt, f(constants + 0x6CCu + k)));
            }
            put_float(rt, constants + 0x704u, f(constants + 0x704u) + psp_mul(dt, f(constants + 0x6F8u)));
        });
    }
    if (count() > 0) {
        each([&](std::uint32_t index) {
            const std::uint32_t car = car_of(index);
            if (byte_u(rt, word(rt, car + 0xCu) + 0x52u) == 8u) call_set(rt, ctx, 0x08954B78u, {car});
            call_set(rt, ctx, 0x08957C3Cu, {car});
            call_set(rt, ctx, 0x08956930u, {car});
        });
        if (count() > 0) {
            each([&](std::uint32_t index) { call_set(rt, ctx, 0x08956EF4u, {car_of(index)}); });
            if (count() > 0) {
                each([&](std::uint32_t index) {
                    const std::uint32_t car = car_of(index);
                    call_set(rt, ctx, 0x0893D188u, {car});
                    call_set(rt, ctx, 0x08956DC8u, {car});
                    call_set(rt, ctx, 0x0895F3BCu, {car});
                });
            }
        }
    }
    call_set(rt, ctx, 0x0895F7C0u, {world});
    if (count() <= 0) return;
    for (std::uint32_t index = 0u; signed_of(index) < count(); ++index) {
        if (!live(index)) continue;
        const std::uint32_t car = car_of(index);
        call_set(rt, ctx, 0x0895EC4Cu, {car});
        call_set(rt, ctx, 0x08960D4Cu, {car});
        const std::uint32_t controller = word(rt, world + 0x10u);
        if (controller != 0u) {
            const std::uint32_t output = word(rt, word(rt, controller) + 0x30u);
            const std::uint32_t state = call_set(rt, ctx, 0x08949AF4u, {car});
            call_set(rt, ctx, output, {controller, index, state});
        }
        call_set(rt, ctx, 0x08957154u, {car});
    }
    ctx.gpr[2] = 0u;
}


// func_08B708C4(a0:a1 = n, a2:a3 = d) -> v0:v1: unsigned 64-bit remainder, libgcc's umoddi3 shape over the same core as
// func_08B702B4 (the remainder is the normalised remainder shifted back); a zero divisor takes the core's `d0 = 1 / d0`
// step, which the Allegrex divu answers with 0xFFFFFFFF, so n is then reduced modulo 0xFFFFFFFF.
void func_08B708C4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const std::uint64_t n = (std::uint64_t{ctx.gpr[5]} << 32) | ctx.gpr[4];
    const std::uint64_t d = (std::uint64_t{ctx.gpr[7]} << 32) | ctx.gpr[6];
    const std::uint64_t r = n % (d != 0u ? d : 0xFFFFFFFFu);
    ctx.gpr[2] = static_cast<std::uint32_t>(r);
    ctx.gpr[3] = static_cast<std::uint32_t>(r >> 32);
}


// func_0894B310(car): tyre forces and the clutch for one step (course grip = car->0 + 0xE4, constants = car+8, state
// = car+0xC, drivetrain = state+0x270). The loaded wheels' slip speed (tyre +0x251: contact velocity x state+0x134C,
// else func_08949B9C with its sign in the stack) is averaged over the front axle into +0x6B0; the clutch state +0x5F9
// (0 off, 1 slipping, 2 / 3 launch) follows the speed, pedals and drivetrain (func_0893EB40, func_0895BBC4 ..
// func_0895BBB4, func_0894A0DC); the drive share +0x90 (+0x84 x state+0x13A0, rear plus +0x53C x state+0x13A8) and
// contact flags +0x53 are set; the rolling drag +0x6A0 comes from the surface grip (func_089467AC of the speed, x +0x4C
// on grass); each wheel's friction ellipse (+0x44 / +0x48 through func_089468C0, sharpened by the surface's grip
// boost) and longitudinal / lateral limits (+0x64 / +0x68), the drive forces +0x88.. by direction, the slip curve
// (+0x6C / +0x74 / +0x7C) and the circle limit (func_08B49D7C) are applied, nearly stopped cars balance their drive
// forces, and the gearbox / engine passes run (func_0894AC4C .. func_0894AA7C).
void func_0894B310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto wheel_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x08955300u, {car, index}); };
    const auto contact_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x089554A8u, {car, index}); };
    const auto tyre_of = [&](std::uint32_t state, std::uint32_t wheel, std::uint32_t index) {
        return call_set(rt, ctx, 0x0894268Cu, {state, index, byte_u(rt, wheel + 0xE9u)});
    };
    const auto curve = [&](std::uint32_t table, float x) { return call_setf(rt, ctx, 0x089467ACu, {table}, {x}).f0; };
    const auto ellipse = [&](std::uint32_t table, float x) { return call_setf(rt, ctx, 0x089468C0u, {table}, {x}).f0; };
    const auto grip_table = [&](std::uint32_t settings, std::uint32_t course, std::uint32_t same_offset, std::uint32_t by_kind) {
        if (signed_of(sext8(byte_u(rt, settings + 3u))) == 2 || byte_u(rt, settings + 0xEu) != 0u) return course + same_offset;
        return course + sext8(byte_u(rt, settings + 8u)) * 4u + by_kind;
    };
    const std::uint32_t sign = frame.at(0u);
    float slip[4];
    float direction[4];
    float average = 0.0f;
    int loaded = 0;
    {
        const std::uint32_t state = word(rt, car + 0xCu);
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            const std::uint32_t contact = contact_of(index);
            slip[index] = 0.0f;
            direction[index] = 0.0f;
            if (f(wheel + 0x70u) == 0.0f) continue;
            const std::uint32_t tyre = tyre_of(state, wheel, index);
            float speed;
            if (byte_u(rt, tyre + 0x251u) != 0u) {
                speed = psp_mul(psp_mul(f(contact + 0x10u), f(wheel + 0x2Cu)) + psp_mul(f(contact + 0xCu), f(wheel + 0x30u)),
                                f(state + 0x134Cu));
                put_float(rt, sign, 1.0f);
                if (speed < 0.0f) {
                    speed = -speed;
                    put_float(rt, sign, -1.0f);
                }
            } else {
                speed = call_setf(rt, ctx, 0x08949B9Cu, {sign}, {f(wheel + 0x24u)}).f0;
                if (f(wheel + 0x30u) < 0.0f) put_float(rt, sign, -f(sign));
            }
            slip[index] = speed;
            direction[index] = f(sign);
            if (index < 2u) {
                average = average + speed;
                ++loaded;
            }
        }
    }
    if (loaded >= 2) average = average / static_cast<float>(loaded);
    put_float(rt, word(rt, car + 8u) + 0x6B0u, average);
    const std::uint32_t state = word(rt, car + 0xCu);
    const std::uint32_t constants = word(rt, car + 8u);
    put(rt, constants + 0x5E8u, 0u);
    put(rt, constants + 0x5E4u, 0u);
    if (call_set(rt, ctx, 0x0893EB40u, {state + 0x270u}) != 0u) {
        put8(rt, constants + 0x5F9u, f(constants + 0x5ECu) == 0.0f ? 0u : 1u);
    } else {
        const std::uint32_t settings = word(rt, car + 8u);
        const std::uint32_t drivetrain = word(rt, car + 0xCu) + 0x270u;
        const auto set_clutch = [&](std::uint32_t value) {
            put8(rt, settings + 0x5F9u, value);
            return value;
        };
        std::uint32_t clutch = byte_u(rt, settings + 0x5F9u);
        constexpr float kTenKmh = std::bit_cast<float>(0x4031C71Du);
        if (clutch == 0u) {
            if (!(f(settings + 0x5ECu) == 0.0f)) {
                const float speed = f(settings + 0x6C4u);
                bool engage;
                if (!(speed <= kTenKmh) && byte_u(rt, settings + 0x5F8u) != 0u) {
                    engage = true;
                } else if (-kTenKmh <= speed) {
                    engage = false;
                } else {
                    engage = byte_u(rt, settings + 0x5F8u) == 0u;
                }
                if (engage) {
                    if (call_set(rt, ctx, 0x0895BBC4u, {drivetrain}) == 0u) {
                        clutch = 0.25f < f(settings + 0x534u) ? set_clutch(2u) : byte_u(rt, settings + 0x5F9u);
                    } else if (call_set(rt, ctx, 0x0895BBA4u, {drivetrain}) != 0u) {
                        clutch = set_clutch(2u);
                    } else if (call_set(rt, ctx, 0x0895BBB4u, {drivetrain}) == 0u) {
                        clutch = byte_u(rt, settings + 0x5F9u);
                    } else {
                        clutch = set_clutch(byte_u(rt, settings + 0x5F8u) < 2u ? 2u : 1u);
                    }
                } else if (signed_of(half_u(rt, drivetrain + 0x34u)) < signed_of(sext16(half_u(rt, settings + 0x58Eu)))) {
                    clutch = set_clutch(2u);
                } else {
                    put(rt, settings + 0x5ECu, 0u);
                }
            }
        } else if (clutch == 1u) {
            if (f(settings + 0x5ECu) == 0.0f) {
                clutch = set_clutch(0u);
            } else if (call_set(rt, ctx, 0x0895BBC4u, {drivetrain}) == 0u) {
                clutch = byte_u(rt, settings + 0x5F9u);
            } else {
                const float launch = call_setf(rt, ctx, 0x0894A0DCu, {car}, {}).f0;
                clutch = byte_u(rt, settings + 0x5F9u);
                put_float(rt, settings + 0x5E8u, launch);
            }
        }
        if (((clutch - 2u) & 0xFFu) < 2u) {
            bool cut = true;
            if (byte_u(rt, settings + 0xEu) == 0u) {
                const std::uint32_t transmission = byte_u(rt, settings + 4u);
                if (transmission == 3u || transmission == 6u) cut = false;
            }
            put_float(rt, settings + 0x5E4u, f(settings + 0x590u));
            const float launch = call_setf(rt, ctx, 0x0894A0DCu, {car}, {}).f0;
            put_float(rt, settings + 0x5E8u, launch);
            if (cut && byte_u(rt, settings + 0x5F9u) == 3u) {
                if (launch < f(settings + 0x5E4u)) {
                    put(rt, settings + 0x534u, 0u);
                } else {
                    put_float(rt, settings + 0x534u, 1.0f);
                }
            }
        }
    }
    if (byte_u(rt, word(rt, car + 0xCu) + 0x281u) == 4u) put8(rt, constants + 0x5F9u, 1u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        const std::uint32_t again = wheel_of(index);
        put_float(rt, wheel + 0x90u, psp_mul(f(again + 0x84u), f(state + (index >> 1) * 4u + 0x13A0u)));
    }
    const float rear_share = psp_mul(f(constants + 0x53Cu), f(state + 0x13A8u));
    for (std::uint32_t index = 2u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        put_float(rt, wheel + 0x90u, rear_share + f(wheel + 0x90u));
    }
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        std::uint32_t flags = f(wheel + 0x90u) == 0.0f ? 0u : 2u;
        if (f(wheel + 0x54u) == 0.0f) flags |= 1u;
        put8(rt, wheel + 0x53u, flags);
    }
    const std::uint32_t course = word(rt, car) + 0xE4u;
    const std::uint32_t settings = word(rt, car + 8u);
    put_float(rt, settings + 0x6A4u, 0.0f);
    put_float(rt, settings + 0x6B4u, 0.0f);
    put_float(rt, settings + 0x6A0u, 0.0f);
    float grip_load = 0.0f;
    bool grass = false;
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        grip_load = grip_load + psp_mul(f(wheel + 0x54u), f(course + byte_u(rt, wheel + 0xE9u) * 4u + 0x148u));
        if ((byte_u(rt, call_set(rt, ctx, 0x0893C780u, {car, index}) + 0x2Fu) & 0x40u) != 0u) grass = true;
    }
    const float speed = f(settings + 0x6C4u);
    if (0.0f < speed) {
        put_float(rt, settings + 0x6A0u, psp_mul(grip_load, -curve(course, speed)));
    } else {
        put_float(rt, settings + 0x6A0u, psp_mul(grip_load, curve(course, -speed)));
    }
    if (grass) put_float(rt, settings + 0x6A0u, psp_mul(f(settings + 0x6A0u), f(course + 0x4Cu)));
    const std::uint32_t tyre_state = word(rt, car + 0xCu);
    float boost_front;
    float boost_rear;
    {
        const std::uint32_t settings_now = word(rt, car + 8u);
        const std::uint32_t grip_course = word(rt, car) + 0xE4u;
        if (signed_of(sext8(byte_u(rt, settings_now + 3u))) == 2 || byte_u(rt, settings_now + 0xEu) != 0u) {
            boost_rear = f(grip_course + 0x70u);
            boost_front = f(grip_course + 0x6Cu);
        } else {
            const std::uint32_t row = grip_course + sext8(byte_u(rt, settings_now + 8u)) * 4u;
            boost_rear = f(row + 0x60u);
            boost_front = f(row + 0x58u);
        }
    }
    const bool boosted = 0.0f < boost_front || 0.0f < boost_rear;
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        const std::uint32_t contact = contact_of(index);
        const std::uint32_t tyre = tyre_of(tyre_state, wheel, index);
        const float load = f(wheel + 0x40u);
        if (0.0f < load) {
            const float scale = psp_mul(ellipse(tyre + 0xBCu, f(contact + 0x1Cu)), f(wheel + 0x58u)) / load;
            const float lateral = psp_mul(scale, f(wheel + 0x48u));
            put_float(rt, wheel + 0x48u, lateral);
            put_float(rt, wheel + 0x44u, psp_mul(scale, f(wheel + 0x44u)));
            if (boosted) {
                const std::uint32_t mode = byte_u(rt, contact + 0x20u);
                const float boost = mode == 1u ? boost_rear : boost_front;
                if ((mode == 0u || mode == 1u) && 0.0f < boost) {
                    const float factor = boost + 1.0f;
                    const float cx = f(contact + 0x10u);
                    const float cz = f(contact + 0xCu);
                    const float longitudinal = f(wheel + 0x44u);
                    const float along = psp_mul(psp_mul(cx, longitudinal) + psp_mul(lateral, cz), factor);
                    const float across = psp_mul(lateral, cx) - psp_mul(cz, longitudinal);
                    put_float(rt, wheel + 0x44u, psp_mul(cx, along) - psp_mul(across, cz));
                    put_float(rt, wheel + 0x48u, psp_mul(cx, across) + psp_mul(cz, along));
                }
            }
        }
        put_float(rt, wheel + 0x64u, ellipse(tyre + 0xF8u, f(wheel + 0x3Cu)));
        put_float(rt, wheel + 0x68u, ellipse(tyre + 0x1ACu, f(contact + 0x1Cu)));
    }
    {
        const std::uint32_t settings_now = word(rt, car + 8u);
        const float side_boost = f(grip_table(settings_now, word(rt, car) + 0xE4u, 0x68u, 0x50u));
        if (0.0f < side_boost) {
            const float factor = side_boost + 1.0f;
            for (std::uint32_t index = 0u; index < 4u; ++index) {
                const std::uint32_t wheel = wheel_of(index);
                if (byte_u(rt, contact_of(index) + 0x20u) != 1u) put_float(rt, wheel + 0x68u, psp_mul(factor, f(wheel + 0x68u)));
            }
        }
    }
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        float drive = psp_mul(f(wheel + 0x64u), f(wheel + 0x58u));
        if (signed_of(sext8(byte_u(rt, wheel + 0x52u))) < 0) drive = -drive;
        float forward = drive;
        float backward = 0.0f;
        if (!(signed_of(sext8(byte_u(rt, wheel + 0x51u))) > 0)) {
            backward = drive;
            forward = 0.0f;
        }
        put_float(rt, wheel + 0x88u, forward);
        put_float(rt, wheel + 0x8Cu, backward);
        put_float(rt, wheel + 0x94u, forward);
        put_float(rt, wheel + 0x98u, backward);
    }
    {
        const std::uint32_t slip_state = word(rt, car + 0xCu);
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            const std::uint32_t tyre = tyre_of(slip_state, wheel, index);
            const float load = f(wheel + 0x70u);
            if (load == 0.0f) {
                put(rt, wheel + 0x78u, 0u);
                put(rt, wheel + 0x74u, 0u);
                continue;
            }
            const float shape = curve(tyre + 0x6Cu, slip[index]);
            const float grip = f(wheel + 0x58u);
            put(rt, wheel + 0x78u, 0u);
            put_float(rt, wheel + 0x6Cu, shape);
            put_float(rt, wheel + 0x7Cu, psp_mul(psp_mul(load, 0.0f), grip));
            put_float(rt, wheel + 0x74u, psp_mul(psp_mul(psp_mul(direction[index], shape), load), grip));
        }
    }
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        const float grip = f(wheel + 0x58u);
        if (!(0.0f < grip)) continue;
        const float limit = psp_mul(grip, f(wheel + 0x68u));
        const float drive = f(wheel + 0x88u) + f(wheel + 0x8Cu);
        const float side = f(wheel + 0x74u);
        const float total = call_setf(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(drive, drive) + psp_mul(side, side)}).f0;
        if (limit < total) {
            const float scale = limit / total;
            put_float(rt, wheel + 0x74u, psp_mul(scale, f(wheel + 0x74u)));
            put_float(rt, wheel + 0x88u, psp_mul(scale, f(wheel + 0x88u)));
            put_float(rt, wheel + 0x8Cu, psp_mul(scale, f(wheel + 0x8Cu)));
            put_float(rt, wheel + 0x98u, psp_mul(scale, f(wheel + 0x98u)));
            put_float(rt, wheel + 0x94u, psp_mul(scale, f(wheel + 0x94u)));
        }
    }
    const float crawl = f(word(rt, car + 8u) + 0x6C4u);
    const float slow = std::bit_cast<float>(0x3F555556u);
    if (!(crawl <= -slow) && !(slow <= crawl)) {
        bool balanced[4];
        float total = 0.0f;
        float driven = 0.0f;
        int count = 0;
        const float still = std::bit_cast<float>(0x3E8E38E4u);
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            const float grip = f(wheel + 0x58u);
            balanced[index] = false;
            const float rate = f(wheel + 0x34u);
            total = total + grip;
            if (still < rate || rate < -still || f(wheel + 0x90u) == 0.0f || grip == 0.0f) continue;
            driven = driven + grip;
            ++count;
            balanced[index] = true;
        }
        if (count != 0) {
            const float share = total / driven;
            for (std::uint32_t index = 0u; index < 4u; ++index) {
                if (!balanced[index]) continue;
                const std::uint32_t wheel = wheel_of(index);
                const float push = psp_mul(psp_mul(share, f(wheel + 0xA0u)), f(wheel + 0x58u));
                put_float(rt, wheel + 0x8Cu, f(wheel + 0x8Cu) - push);
                put_float(rt, wheel + 0x98u, f(wheel + 0x98u) - push);
            }
        }
    }
    for (std::uint32_t function : {0x0894AC4Cu, 0x0895BFC4u, 0x0895C140u, 0x0895C394u, 0x0895D7CCu}) call_set(rt, ctx, function, {car});
    if (call_set(rt, ctx, 0x0893EB40u, {word(rt, car + 0xCu) + 0x270u}) != 0u) {
        call_set(rt, ctx, 0x08940120u, {car});
        call_set(rt, ctx, 0x0894A01Cu, {car});
    } else if (call_set(rt, ctx, 0x0895BBA4u, {word(rt, car + 0xCu) + 0x270u}) != 0u ||
               (call_set(rt, ctx, 0x0895BBB4u, {word(rt, car + 0xCu) + 0x270u}) == 0u &&
                byte_u(rt, word(rt, car + 0xCu) + 0x52u) == 2u)) {
        call_set(rt, ctx, 0x0894A600u, {car});
        call_set(rt, ctx, 0x0894A01Cu, {car});
    } else {
        call_set(rt, ctx, 0x0894A600u, {car});
        call_set(rt, ctx, 0x08949F44u, {car});
    }
    call_set(rt, ctx, 0x08949DE0u, {car, 0u});
    float support = 0.0f;
    for (std::uint32_t index = 0u; index < 4u; ++index) support = support + f(wheel_of(index) + 0x54u);
    const std::uint32_t airborne = support == 0.0f ? 1u : 0u;
    call_set(rt, ctx, 0x0894AE54u, {car, airborne});
    call_set(rt, ctx, 0x0894AA7Cu, {car, airborne});
}


// func_0895C394(car): engine torque into the differentials for one step (constants = car+8, state = car+0xC,
// drivetrain = state+0x270 with layout +0, driven axle +2, centre type +3/+4, gear ratios +0x58.., axle inertia
// +0xD8 / +0xDC). It flags the driven (bit 8) and coupled (bit 4) wheels, takes the engine torque (func_0893F088, the
// launch / gearbox models func_0895BD2C .. func_089479EC, the clutch states 1 / 3 and the slip boost against the rev
// difference +0x5E4 / +0x5E8) and integrates the engine speed +0x590 (floored at the idle state+0x23E rpm). The axles'
// reaction torques (wheel +0x9C, the centre ratio +0xA8 / +0xB0) and the layout pick the coupling: open, locked,
// viscous (func_0895B60C), or one of the limited-slip models (func_089448D4, the tables at 0x08CDE0A8.., func_0894A520,
// the yaw state +0x278 / +0x27C) that give a lock torque, 98000 meaning locked. Locked states +0x5FA integrate the
// shared spin +0x5FC / +0x600 (func_0895C0C8 clears reversals); open ones split the torque +0x604 / +0x608 by layout
// and integrate each axle; spins that cross over are merged by inertia. Finally the clutch torque filter +0x5AC / +0x5B0
// follows the drive torque (+0x5A8).
void func_0895C394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto wheel_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x08955300u, {car, index}); };
    const std::uint32_t settings = word(rt, car + 8u);
    const float dt = call_setf(rt, ctx, 0x0895549Cu, {car}, {}).f0;
    const std::uint32_t state = word(rt, car + 0xCu);
    const std::uint32_t drive = state + 0x270u;
    constexpr float kLocked = std::bit_cast<float>(0x47BF6800u);
    const auto flag_axle = [&](std::uint32_t axle, std::uint32_t bit) {
        for (std::uint32_t index = axle * 2u; index < axle * 2u + 2u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            put8(rt, wheel + 0x53u, byte_u(rt, wheel + 0x53u) | bit);
        }
    };
    if (byte_u(rt, settings + 0x5F9u) != 0u) {
        const std::uint32_t driven = byte_u(rt, drive + 2u);
        flag_axle(driven, 8u);
        if (byte_u(rt, drive + 3u) == 2u) flag_axle(1u - driven, 4u);
    }
    float torque = 0.0f;
    if (call_set(rt, ctx, 0x0893EB40u, {drive}) != 0u) {
        const float engine = call_setf(rt, ctx, 0x0893F088u, {car}, {}).f0;
        if (byte_u(rt, settings + 0x5F9u) != 0u) torque = engine;
    } else if (byte_u(rt, settings + 0x5F9u) != 0u) {
        float produced;
        if (call_set(rt, ctx, 0x0895BBC4u, {drive}) != 0u) {
            produced = call_setf(rt, ctx, 0x0895BD2Cu, {car}, {}).f0;
        } else if (byte_u(rt, drive) == 7u) {
            const float boost = call_setf(rt, ctx, 0x08947AD8u, {car}, {}).f0;
            const float base = call_setf(rt, ctx, 0x089491A0u, {car}, {boost}).f0;
            produced = base + call_setf(rt, ctx, 0x08947F70u, {car}, {}).f0;
        } else if (byte_u(rt, drive) == 4u) {
            const float throttle = f(settings + 0x534u);
            const float base = call_setf(rt, ctx, 0x089491A0u, {car}, {throttle}).f0;
            produced = base + call_setf(rt, ctx, 0x089479ECu, {sext16(half_u(rt, settings + 0x58Eu))}, {throttle}).f0;
        } else {
            produced = call_setf(rt, ctx, 0x089491A0u, {car}, {f(settings + 0x534u)}).f0;
        }
        const std::uint32_t clutch = byte_u(rt, settings + 0x5F9u);
        torque = produced;
        if (clutch != 1u) {
            float load;
            if (clutch == 3u) {
                const std::uint32_t box = byte_u(rt, drive + 0x11u);
                if (box == 6u) {
                    load = f(settings + 0x534u) == 0.0f ? f(settings + 0x57Cu) : f(settings + 0x580u);
                } else if (box == 8u || box == 9u) {
                    load = f(settings + 0x534u) == 0.0f ? f(settings + 0x57Cu) : 0.0f;
                } else {
                    load = 0.0f;
                }
            } else {
                load = psp_mul(f(settings + 0x57Cu), std::bit_cast<float>(0x3D4CCCCDu));
                if (0.0f < produced) {
                    float gap = f(settings + 0x5E4u) - f(settings + 0x5E8u);
                    if (gap < 0.0f) gap = -gap;
                    const float over = gap - std::bit_cast<float>(0x42517084u);
                    float boost = 0.0f;
                    if (!(over < 0.0f)) boost = over / std::bit_cast<float>(0x44517084u);
                    float factor = boost + 1.0f;
                    if (std::bit_cast<float>(0x3F900000u) <= factor) factor = std::bit_cast<float>(0x3F900000u);
                    load = load + psp_mul(produced, factor);
                }
            }
            if (f(settings + 0x5E4u) < f(settings + 0x5E8u)) load = -load;
            const float rpm = f(settings + 0x590u) + psp_mul(dt, psp_mul(produced - load, f(drive + 0xE4u)));
            put_float(rt, settings + 0x590u, rpm);
            const float idle = psp_mul(static_cast<float>(signed_of(half_u(rt, state + 0x23Eu))), std::bit_cast<float>(0x3DD67750u));
            if (rpm < idle) put_float(rt, settings + 0x590u, idle);
            torque = load;
        }
    }
    if (byte_u(rt, drive) != 7u) put_float(rt, settings + 0x5F4u, torque < 0.0f ? -torque : torque);
    std::uint32_t gear = byte_u(rt, settings + 0x5F8u);
    if (call_set(rt, ctx, 0x0895BBB4u, {drive}) != 0u) gear = std::min(gear, 1u);
    const float axle_torque = psp_mul(torque, f(drive + gear * 4u + 0x58u));
    float reaction[2] = {0.0f, 0.0f};
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t axle = index >> 1;
        const float before = reaction[axle];
        reaction[axle] = before + f(wheel_of(index) + 0x9Cu);
    }
    if (byte_u(rt, drive + 4u) != 0u) {
        const std::uint32_t driven = byte_u(rt, drive + 2u);
        const std::uint32_t other = 1u - driven;
        reaction[other] = psp_mul(reaction[other], psp_mul(f(drive + other * 4u + 0xA8u), f(drive + driven * 4u + 0xB0u)));
    }
    const float front = reaction[0];
    const float rear = reaction[1];
    float total = front + rear;
    const std::uint32_t layout = byte_u(rt, drive);
    float lock = 0.0f;
    float viscous = 0.0f;
    bool coupled = false;
    std::uint32_t mode = 0u;
    bool use_lock = false;
    float lock_torque = 0.0f;
    switch (layout) {
    case 0u:
    case 7u: total = rear; break;
    case 1u: total = front; break;
    case 2u: mode = 1u; break;
    case 3u: mode = 2u; break;
    case 10u: {
        put_float(rt, frame.at(0u), 0.0f);
        float engage;
        if (call_set(rt, ctx, 0x0895B60Cu, {car, frame.at(0u)}) != 0u) {
            engage = f(frame.at(0u));
        } else {
            float steer = f(settings + 0x520u);
            const float brake = f(settings + 0x530u);
            const float pedal = f(settings + 0x538u);
            if (steer < 0.0f) steer = -steer;
            float cut = psp_mul(steer, std::bit_cast<float>(0x40747645u));
            if (1.0f < cut) cut = 1.0f;
            put_float(rt, frame.at(0u), brake);
            if (brake < pedal) put_float(rt, frame.at(0u), pedal);
            engage = f(frame.at(0u)) - cut;
            put_float(rt, frame.at(0u), engage);
            if (engage < 0.0f) {
                engage = 0.0f;
                put_float(rt, frame.at(0u), 0.0f);
            }
        }
        viscous = psp_mul(f(drive + 0xCu), engage);
        coupled = true;
        if (byte_u(rt, settings + 0x5FAu) == 0u) {
            mode = 2u;
        } else {
            float split = front - rear;
            if (split < 0.0f) split = -split;
            if (viscous < split) {
                mode = 2u;
                put8(rt, settings + 0x5FAu, 0u);
            } else {
                mode = 3u;
            }
        }
        break;
    }
    case 4u:
    case 5u:
    case 6u:
    case 8u:
    case 9u:
    case 11u: {
        use_lock = true;
        const float ratio = f(drive + 8u);
        float result = kLocked;
        bool to_result = false;
        if (layout == 4u) {
            const float share = call_setf(rt, ctx, 0x089448D4u, {car}, {ratio}).f0;
            if (share == 1.0f) {
                to_result = true;
            } else if (!(f(settings + 0x53Cu) <= 0.2f)) {
                result = 0.0f;
            } else if (axle_torque <= 0.0f) {
                result = kLocked;
            } else if (half_u(rt, settings + 0x5F8u) != 0x100u) {
                result = psp_mul(axle_torque, share);
            }
        } else if (layout == 5u) {
            const std::uint32_t lsd_state = word(rt, car + 0xCu);
            const std::uint32_t kind = byte_u(rt, lsd_state + 0x271u);
            const std::uint32_t model = byte_u(rt, 0x08CDE0C8u + kind);
            const std::uint32_t lsd = word(rt, car + 8u);
            float sense;
            if (model == 1u) {
                sense = f(lsd + 0x6C0u);
            } else if (model == 0u) {
                sense = psp_mul(f(lsd + 0x6B8u), 4.0f);
                sense = sense / call_setf(rt, ctx, 0x0894A520u, {car}, {}).f0;
            } else if (model == 2u) {
                sense = psp_mul(psp_mul(f(lsd + 0x6C4u), f(lsd + 0x560u)), 4.0f);
                sense = sense / call_setf(rt, ctx, 0x0894A520u, {car}, {}).f0;
            } else if (model == 3u) {
                sense = psp_mul(psp_mul(f(lsd + 0x6C4u), f(lsd + 0x560u)), 4.0f);
                sense = sense / call_setf(rt, ctx, 0x0894A520u, {car}, {}).f0;
                sense = psp_mul(f(lsd + 0x6C0u), f(0x08CDE0B0u + kind * 4u)) + psp_mul(sense, f(0x08CDE0A8u + kind * 4u));
            } else {
                sense = 0.0f;
            }
            const bool one_way = psp_mul(sense, f(lsd + 0x520u)) < 0.0f && byte_u(rt, 0x08CF2F84u + kind) != 0u;
            if (sense < 0.0f) sense = -sense;
            const float threshold = f(0x08CDE0C0u + kind * 4u);
            float preload = 1.0f;
            if (!(sense <= threshold) && !one_way) {
                const float floor = f(0x08CDE0B8u + kind * 4u);
                if (1.0f <= sense) {
                    preload = floor;
                } else {
                    preload = 1.0f - psp_mul(1.0f - floor, sense - threshold) / (1.0f - threshold);
                }
            }
            float spin = f(lsd + 0x600u) - f(lsd + 0x5FCu);
            if (spin < 0.0f) spin = -spin;
            const float yaw = psp_mul(spin, f(lsd_state + 0x278u));
            float signed_torque = byte_u(rt, lsd + 0x5F8u) == 0u ? -axle_torque : axle_torque;
            const float yaw_lock = f(lsd_state + 0x27Cu);
            bool locked = false;
            bool done = false;
            result = psp_mul(preload, yaw);
            if (byte_u(rt, 0x08CDE0CCu + kind) != 0u && signed_torque < 0.0f) {
                result = -kLocked;
                to_result = true;
                done = true;
            }
            if (!done && !(yaw_lock == 0.0f)) {
                const float sum = rear + (signed_torque + front);
                if (half_u(rt, lsd + 0x5F8u) == 0x100u || byte_u(rt, lsd + 0x5F9u) == 0u) {
                    locked = true;
                } else if (preload == 1.0f) {
                    const std::uint32_t clutch = byte_u(rt, lsd + 0x5F9u);
                    if (clutch == 2u || 0.75f < f(lsd + 0x538u)) {
                        locked = true;
                    } else if (0.0f < signed_torque) {
                        locked = 0.0f < sum;
                    } else if (signed_torque < 0.0f) {
                        locked = sum < 0.0f;
                    }
                }
                if (locked) {
                    result = kLocked;
                    done = true;
                }
            }
            if (!done) {
                if (1.0f <= result) {
                    if (!(yaw_lock == 0.0f)) {
                        result = kLocked;
                        to_result = true;
                    } else {
                        result = psp_mul(psp_mul(axle_torque, 1.0f), 0.5f);
                    }
                } else if (result == kLocked || result == -kLocked) {
                    to_result = true;
                } else {
                    result = psp_mul(psp_mul(axle_torque, result), 0.5f);
                }
            }
        } else if (layout == 6u) {
            if (!(f(settings + 0x53Cu) <= 0.2f)) {
                result = psp_mul(psp_mul(0.0f, axle_torque), 0.5f);
            } else if (half_u(rt, settings + 0x5F8u) == 0x100u) {
                result = kLocked;
            } else {
                float steer = f(settings + 0x520u);
                if (steer < 0.0f) steer = -steer;
                const float open = 1.0f - psp_mul(steer, ratio);
                if (open < 0.0f) {
                    result = psp_mul(psp_mul(0.0f, axle_torque), 0.5f);
                } else if (!(open < std::bit_cast<float>(0x3F7D70A4u))) {
                    result = kLocked;
                } else if (open == kLocked || open == -kLocked) {
                    result = open;
                } else {
                    result = psp_mul(psp_mul(open, axle_torque), 0.5f);
                }
            }
        } else if (layout == 8u || layout == 9u) {
            result = psp_mul(axle_torque, ratio);
        } else {
            float slip = f(settings + 0x564u) - std::bit_cast<float>(0x3D9374BCu);
            const float scaled = psp_mul(axle_torque, ratio);
            if (slip < 0.0f) slip = 0.0f;
            slip = slip - f(settings + 0x534u);
            float ramp;
            bool use_ramp = true;
            if (slip < 0.0f) {
                ramp = 1.0f;
            } else if (1.0f < slip) {
                const float over = slip - 1.0f;
                const float band = std::bit_cast<float>(0x3E4CCCD0u);
                use_ramp = false;
                result = band <= over ? 0.0f : psp_mul(scaled, (band - over) / band);
            } else {
                ramp = 1.0f - slip;
            }
            if (use_ramp) {
                const float share = ramp / std::bit_cast<float>(0x3E99999Au);
                if (1.0f <= share) {
                    result = kLocked;
                    to_result = true;
                } else {
                    result = psp_mul(axle_torque, psp_mul(ratio, 1.0f - share) + psp_mul(share, 0.5f));
                }
            }
        }
        (void)to_result;
        lock_torque = result;
        break;
    }
    default: break;
    }
    if (use_lock) {
        const std::uint32_t driven = byte_u(rt, drive + 2u);
        bool locked_state = byte_u(rt, settings + 0x5FAu) != 0u;
        bool free_spin = false;
        if (locked_state) {
            if (lock_torque == kLocked) {
                mode = 3u;
                flag_axle(1u - driven, 4u);
                free_spin = true;
            } else if (lock_torque == -kLocked) {
                put8(rt, settings + 0x5FAu, 0u);
                locked_state = false;
            } else {
                float slip = (reaction[1u - driven] - reaction[driven]) - axle_torque;
                if (slip < 0.0f) slip = -slip;
                const float limit = lock_torque < 0.0f ? -lock_torque : lock_torque;
                if (limit < slip) {
                    put8(rt, settings + 0x5FAu, 0u);
                    locked_state = false;
                } else {
                    mode = 3u;
                    flag_axle(1u - driven, 4u);
                    free_spin = true;
                }
            }
        }
        if (!free_spin) {
            float coupling = 0.0f;
            if (!(lock_torque == -kLocked)) {
                if (lock_torque == kLocked) {
                    coupling = f(drive + 0xCu);
                } else {
                    coupling = lock_torque < 0.0f ? -lock_torque : lock_torque;
                }
            }
            if (!(lock_torque == -kLocked) && !(coupling == 0.0f)) {
                mode = 1u;
                flag_axle(1u - byte_u(rt, drive + 2u), 4u);
            } else {
                total = reaction[byte_u(rt, drive + 2u)];
                mode = 0u;
            }
            lock = coupling;
        }
        viscous = 0.0f;
        coupled = false;
    }
    float input = 0.0f;
    if (call_set(rt, ctx, 0x0893EB40u, {drive}) == 0u && byte_u(rt, settings + 0x5F9u) == 1u) {
        std::uint32_t gear_index = byte_u(rt, settings + 0x5F8u);
        if (call_set(rt, ctx, 0x0895BBB4u, {drive}) != 0u && !(signed_of(gear_index) < 2)) gear_index = 1u;
        input = psp_mul(f(drive + 0x18u), f(drive + gear_index * 4u + 0x58u));
        if (byte_u(rt, settings + 0x5F8u) == 0u) input = -input;
        if (call_set(rt, ctx, 0x0895BBC4u, {drive}) != 0u) input = psp_mul(input, f(settings + 0x5F0u));
    }
    float spin_gap = 0.0f;
    if (mode == 3u) {
        const float spin = f(settings + 0x5FCu);
        const float inertia = input + (f(drive + 0xD8u) + f(drive + 0xDCu));
        float value = psp_mul(dt, (rear + (axle_torque + front)) / inertia) + spin;
        put_float(rt, settings + 0x5FCu, value);
        if (call_set(rt, ctx, 0x0895C0C8u, {car}) != 0u) {
            value = f(settings + 0x5FCu);
            if (psp_mul(spin, value) < 0.0f || spin == 0.0f) {
                value = 0.0f;
                put_float(rt, settings + 0x5FCu, 0.0f);
            }
        } else {
            value = f(settings + 0x5FCu);
        }
        const float half = psp_mul(axle_torque, 0.5f);
        put_float(rt, settings + 0x600u, value);
        put_float(rt, settings + 0x608u, half);
        put_float(rt, settings + 0x604u, half);
    } else if (mode == 2u) {
        const float inertia = input + (f(drive + 0xD8u) + f(drive + 0xDCu));
        const float mean = psp_mul(f(settings + 0x5FCu) + f(settings + 0x600u), 0.5f);
        float shared = mean + psp_mul(dt, (rear + (axle_torque + front)) / inertia);
        float split_spin;
        if (call_set(rt, ctx, 0x0895C0C8u, {car}) != 0u) {
            if (psp_mul(mean, shared) < 0.0f || mean == 0.0f) shared = 0.0f;
        }
        split_spin = f(settings + 0x5FCu) - f(settings + 0x600u);
        float couple;
        if (coupled) {
            couple = 0.0f < split_spin ? viscous : -viscous;
        } else {
            couple = psp_mul(split_spin, f(drive + 8u));
        }
        const float half_split = psp_mul(split_spin, 0.5f);
        float delta = psp_mul(dt, psp_mul((front - rear) - couple, f(drive + 0xE0u))) + half_split;
        const float half = psp_mul(axle_torque, 0.5f);
        if (psp_mul(half_split, delta) < 0.0f) {
            delta = 0.0f;
            put8(rt, settings + 0x5FAu, 1u);
        }
        put_float(rt, settings + 0x608u, half);
        put_float(rt, settings + 0x5FCu, shared + delta);
        put_float(rt, settings + 0x600u, shared - delta);
        put_float(rt, settings + 0x604u, half);
    } else {
        const std::uint32_t inertia_front = frame.at(4u);
        const std::uint32_t inertia_rear = frame.at(8u);
        const float d8 = f(drive + 0xD8u);
        const float dc = f(drive + 0xDCu);
        if (layout < 12u) {
            switch (layout) {
            case 1u:
                put(rt, inertia_rear, 0u);
                put_float(rt, inertia_front, input + d8);
                put_float(rt, settings + 0x604u, axle_torque);
                put(rt, settings + 0x608u, 0u);
                break;
            case 2u: {
                put_float(rt, inertia_front, input + d8);
                put_float(rt, inertia_rear, dc);
                const float couple = psp_mul(f(drive + 8u), f(settings + 0x5FCu) - f(settings + 0x600u));
                put_float(rt, settings + 0x608u, couple);
                put_float(rt, settings + 0x604u, axle_torque - couple);
                break;
            }
            case 0u:
            case 7u:
                put(rt, inertia_front, 0u);
                put_float(rt, inertia_rear, input + dc);
                put_float(rt, settings + 0x608u, axle_torque);
                put(rt, settings + 0x604u, 0u);
                break;
            case 4u:
            case 5u:
            case 6u:
            case 8u:
                put_float(rt, inertia_front, d8);
                put_float(rt, inertia_rear, input + dc);
                if (f(settings + 0x600u) < f(settings + 0x5FCu)) {
                    put_float(rt, settings + 0x608u, lock + axle_torque);
                    put_float(rt, settings + 0x604u, -lock);
                } else {
                    put_float(rt, settings + 0x604u, lock);
                    put_float(rt, settings + 0x608u, axle_torque - lock);
                }
                break;
            case 9u:
            case 11u:
                put_float(rt, inertia_front, input + d8);
                put_float(rt, inertia_rear, dc);
                if (f(settings + 0x600u) < f(settings + 0x5FCu)) {
                    put_float(rt, settings + 0x608u, lock);
                    put_float(rt, settings + 0x604u, axle_torque - lock);
                } else {
                    put_float(rt, settings + 0x604u, lock + axle_torque);
                    put_float(rt, settings + 0x608u, -lock);
                }
                break;
            default: break;
            }
        }
        if (mode == 1u) spin_gap = f(settings + 0x5FCu) - f(settings + 0x600u);
        for (std::uint32_t axle = 0u; axle < 2u; ++axle) {
            if (byte_u(rt, drive + 0x14u + axle) == 0u) continue;
            const std::uint32_t spin_address = settings + 0x5FCu + axle * 4u;
            const float inertia = f(frame.at(4u) + axle * 4u);
            const float old = f(spin_address);
            float step = 0.0f;
            if (!(inertia == 0.0f)) {
                step = psp_mul(dt, (f(settings + 0x604u + axle * 4u) + reaction[axle]) / inertia);
            }
            put_float(rt, spin_address, step + old);
            const std::uint32_t first = axle * 2u;
            if (f(wheel_of(first) + 0x90u) == 0.0f && f(wheel_of(first + 1u) + 0x90u) == 0.0f) continue;
            if (psp_mul(old, f(spin_address)) < 0.0f || old == 0.0f) put_float(rt, spin_address, 0.0f);
        }
        if (mode == 1u) {
            const float now = f(settings + 0x5FCu) - f(settings + 0x600u);
            const bool crossed = spin_gap <= 0.0f ? (spin_gap < 0.0f && 0.0f <= now) : now <= 0.0f;
            if (crossed) {
                const std::uint32_t driven = byte_u(rt, drive + 2u);
                const std::uint32_t other = 1u - driven;
                const float own = f(drive + driven * 4u + 0xD8u) + psp_mul(f(drive + byte_u(rt, settings + 0x5F8u) * 4u + 0x58u), f(drive + 0x18u));
                float far = f(drive + other * 4u + 0xD8u);
                if (byte_u(rt, drive + 4u) != 0u) far = psp_mul(far, f(drive + other * 4u + 0xA8u)) / f(drive + driven * 4u + 0xA8u);
                const float merged = (psp_mul(own, f(settings + 0x5FCu + driven * 4u)) + psp_mul(far, f(settings + 0x5FCu + other * 4u))) / (far + own);
                put_float(rt, settings + 0x5FCu, merged);
                put_float(rt, settings + 0x600u, merged);
                const std::uint32_t kind = byte_u(rt, drive);
                if (((kind - 4u) & 0xFFu) < 3u || kind == 9u || kind == 10u || kind == 11u) put8(rt, settings + 0x5FAu, 1u);
            }
        }
    }
    float drag = 0.0f;
    ctx.gpr[2] = 0u;
    if (((byte_u(rt, settings + 0x5F9u) - 1u) & 0xFFu) < 2u) {
        std::uint32_t gear_index = byte_u(rt, settings + 0x5F8u);
        float ratio;
        if (call_set(rt, ctx, 0x0895BBC4u, {drive}) != 0u) {
            if (call_set(rt, ctx, 0x0895BBB4u, {drive}) != 0u) gear_index = std::min(gear_index, 1u);
            ratio = psp_mul(f(drive + gear_index * 4u + 0x58u), f(settings + 0x5F0u));
        } else {
            ratio = f(drive + gear_index * 4u + 0x58u);
        }
        ctx.gpr[2] = drive + gear_index * 4u;
        drag = -total / ratio;
        if (psp_mul(drag, f(settings + 0x5A8u)) < 0.0f) drag = 0.0f;
    }
    const float position = f(settings + 0x5ACu);
    float velocity = f(settings + 0x5B0u);
    const float sixtieth = std::bit_cast<float>(0x3C88AB7Cu);
    velocity = velocity + psp_mul(psp_mul(drag - position, 1000.0f) + psp_mul(velocity, -50.0f), sixtieth);
    put_float(rt, settings + 0x5B0u, velocity);
    put_float(rt, settings + 0x5ACu, position + psp_mul(velocity, sixtieth));
}

} // namespace hot_batch_123
} // namespace gt::game

#define HOT123(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_123::name, gt::ReturnKind::kind)
HOT123(0x089223B8u, func_089223B8, Word);
HOT123(0x08B6F518u, func_08B6F518, DoubleWord);
HOT123(0x08C28D70u, func_08C28D70, Word);
HOT123(0x0889F3A8u, func_0889F3A8, Word);
HOT123(0x0889BAB0u, func_0889BAB0, Word);
HOT123(0x089FBC08u, func_089FBC08, Word);
HOT123(0x089193B8u, func_089193B8, Word);
HOT123(0x0894DD9Cu, func_0894DD9C, Word);
// HOT123(0x08B708C4u, func_08B708C4, DoubleWord);  // batch 123: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT123(0x0894B310u, func_0894B310, Word);
HOT123(0x0895C394u, func_0895C394, Word);
