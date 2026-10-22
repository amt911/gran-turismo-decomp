// Hot batch 122: functions of 400-700 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
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
namespace hot_batch_122 {
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


// func_08903B30(mesh, hit, from, to, cell) -> 1 when the segment from->to crosses a triangle of collision cell `cell`
// (mesh+0x7C, 16-byte {count +0 (s16), triangle index list +8}), else 0. Triangles: mesh+0x78, 12-byte {vertex indices
// +0/+2/+4, attribute bytes +6/+7}; vertices: mesh+0x74, 16-byte {x, y, z, bytes +0xE/+0xF}. A triangle counts when
// the three triple products of the segment with its edges seen from `from` (u, v, w) are all <= 0 and the plane
// fraction lies in [0, 1] (0 for a segment parallel to it); the nearest one wins. The record gets the point (kept in the
// triangle's bounding box), the attributes (+6 & 0x1F, +7), the vertices' bytes +0xE (/255) and +0xF & 0x7F (/127)
// interpolated by u and v and clamped to [0, 1], the unit normal +0x1C, plane offset +0x28, fraction +0x2C, triangle
// +0x30 and cell +0x34.
void func_08903B30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    struct Vec {
        float x, y, z;
    };
    const std::uint32_t mesh = ctx.gpr[4];
    const std::uint32_t hit = ctx.gpr[5];
    const std::uint32_t from = ctx.gpr[6];
    const std::uint32_t to = ctx.gpr[7];
    const std::uint32_t cell_index = ctx.gpr[8];
    const auto at = [&](std::uint32_t address) {
        return Vec{float_at(rt, address), float_at(rt, address + 4u), float_at(rt, address + 8u)};
    };
    const std::uint32_t cell = word(rt, mesh + 0x7Cu) + cell_index * 16u;
    const Vec p = at(from);
    const Vec end = at(to);
    const float dx = end.x - p.x;
    const float dy = end.y - p.y;
    const float dz = end.z - p.z;
    const std::int32_t count = signed_of(sext16(half_u(rt, cell)));
    ctx.gpr[2] = 0u;
    if (count <= 0) return;
    const std::uint32_t triangles = word(rt, mesh + 0x78u);
    const std::uint32_t vertices = word(rt, mesh + 0x74u);
    const auto vertex = [&](std::uint32_t triangle, std::uint32_t corner) {
        return vertices + half_u(rt, triangles + triangle * 12u + corner * 2u) * 16u;
    };
    const auto along = [&](float x, float y, float z) { return (psp_mul(dx, x) + psp_mul(dy, y)) + psp_mul(dz, z); };
    float best = std::bit_cast<float>(0x7F7FFFFFu);
    std::int32_t best_triangle = -1;
    float best_u = 0.0f;
    float best_v = 0.0f;
    float best_w = 0.0f;
    Vec normal{0.0f, 0.0f, 0.0f};
    std::uint32_t list = word(rt, cell + 8u);
    for (std::int32_t i = 0; i < count; ++i, list += 2u) {
        const std::uint32_t triangle = half_u(rt, list);
        const Vec va = at(vertex(triangle, 0u));
        const Vec vb = at(vertex(triangle, 1u));
        const Vec a{va.x - p.x, va.y - p.y, va.z - p.z};
        const Vec b{vb.x - p.x, vb.y - p.y, vb.z - p.z};
        const float u = along(psp_mul(a.y, b.z) - psp_mul(a.z, b.y), psp_mul(a.z, b.x) - psp_mul(a.x, b.z),
                              psp_mul(a.x, b.y) - psp_mul(a.y, b.x));
        if (0.0f < u) continue;
        const Vec vc = at(vertex(triangle, 2u));
        const Vec c{vc.x - p.x, vc.y - p.y, vc.z - p.z};
        const float v = along(psp_mul(a.z, c.y) - psp_mul(a.y, c.z), psp_mul(a.x, c.z) - psp_mul(a.z, c.x),
                              psp_mul(a.y, c.x) - psp_mul(a.x, c.y));
        if (0.0f < v) continue;
        const float w = along(psp_mul(b.y, c.z) - psp_mul(b.z, c.y), psp_mul(b.z, c.x) - psp_mul(b.x, c.z),
                              psp_mul(b.x, c.y) - psp_mul(b.y, c.x));
        if (0.0f < w) continue;
        const Vec e0{vb.x - va.x, vb.y - va.y, vb.z - va.z};
        const Vec e1{vc.x - va.x, vc.y - va.y, vc.z - va.z};
        const Vec n{psp_mul(e0.y, e1.z) - psp_mul(e0.z, e1.y), psp_mul(e0.z, e1.x) - psp_mul(e0.x, e1.z),
                    psp_mul(e0.x, e1.y) - psp_mul(e0.y, e1.x)};
        const float denominator = along(n.x, n.y, n.z);
        float fraction = 0.0f;
        if (!(denominator == 0.0f)) {
            const float numerator = (psp_mul(a.x, n.x) + psp_mul(a.y, n.y)) + psp_mul(a.z, n.z);
            if (0.0f < numerator || numerator < denominator) continue;
            fraction = numerator / denominator;
        }
        if (!(fraction < best)) continue;
        best = fraction;
        best_triangle = static_cast<std::int32_t>(triangle);
        best_u = u;
        best_v = v;
        best_w = w;
        normal = n;
    }
    if (best_triangle < 0) return;
    const std::uint32_t triangle = static_cast<std::uint32_t>(best_triangle);
    put_float(rt, hit, p.x + psp_mul(dx, best));
    const float y0 = float_at(rt, from + 4u);
    put_float(rt, hit + 4u, y0 + psp_mul(best, float_at(rt, to + 4u) - y0));
    put(rt, hit + 0x34u, cell_index);
    const float z0 = float_at(rt, from + 8u);
    const float z1 = float_at(rt, to + 8u);
    put(rt, hit + 0x30u, triangle);
    put_float(rt, hit + 8u, z0 + psp_mul(best, z1 - z0));
    const std::uint32_t corner_a = vertex(triangle, 0u);
    const std::uint32_t corner_b = vertex(triangle, 1u);
    const std::uint32_t corner_c = vertex(triangle, 2u);
    for (std::uint32_t axis = 0u; axis < 12u; axis += 4u) {
        const float fa = float_at(rt, corner_a + axis);
        const float fb = float_at(rt, corner_b + axis);
        const float fc = float_at(rt, corner_c + axis);
        float low;
        float high;
        if (fa < fb) {
            low = fa < fc ? fa : fc;
            high = fb < fc ? fc : fb;
        } else {
            low = fb < fc ? fb : fc;
            high = fa < fc ? fc : fa;
        }
        float value = float_at(rt, hit + axis);
        if (high < value) value = high;
        if (low < value) low = value;
        put_float(rt, hit + axis, low);
    }
    const std::uint32_t record = triangles + triangle * 12u;
    put(rt, hit + 0xCu, byte_u(rt, record + 6u) & 0x1Fu);
    put(rt, hit + 0x10u, byte_u(rt, record + 7u));
    const float scale = 1.0f / (best_v + (best_u + best_w));
    const auto blend = [&](std::uint32_t offset, std::uint32_t mask, std::uint32_t step_bits) {
        const float step = std::bit_cast<float>(step_bits);
        const auto weight = [&](std::uint32_t corner) {
            return psp_mul(static_cast<float>(signed_of(byte_u(rt, corner + offset) & mask)), step);
        };
        const float wa = weight(corner_a);
        const float wb = weight(corner_b);
        const float wc = weight(corner_c);
        float value = wa + psp_mul(scale, psp_mul(best_v, wb - wa) + psp_mul(wc - wa, best_u));
        if (value < 0.0f) {
            value = 0.0f;
        } else if (!(value < 1.0f)) {
            value = 1.0f;
        }
        return value;
    };
    put_float(rt, hit + 0x14u, blend(0xEu, 0xFFu, 0x3B808081u));
    put_float(rt, hit + 0x18u, blend(0xFu, 0x7Fu, 0x3C010204u));
    const float inverse = 1.0f / std::sqrt((psp_mul(normal.x, normal.x) + psp_mul(normal.y, normal.y)) + psp_mul(normal.z, normal.z));
    const float nx = psp_mul(normal.x, inverse);
    put_float(rt, hit + 0x24u, psp_mul(normal.z, inverse));
    put_float(rt, hit + 0x1Cu, nx);
    put_float(rt, hit + 0x20u, psp_mul(normal.y, inverse));
    const Vec origin = at(corner_a);
    const float offset = (psp_mul(nx, origin.x) + psp_mul(float_at(rt, hit + 0x20u), origin.y)) +
                         psp_mul(float_at(rt, hit + 0x24u), origin.z);
    put_float(rt, hit + 0x2Cu, best);
    put_float(rt, hit + 0x28u, -offset);
    ctx.gpr[2] = 1u;
}


// func_0893D9AC(car): one suspension step for the four wheels (func_08955300(car, i); axle i/2 has its parameters at
// state+0x360 + 0x74 * axle, state = car+0xC, constants = car+8).
// 1. Each wheel's +0xE4 gets a load-transfer term built from the axle's state+0x10 / +0x28 values, the wheel geometry
//    of func_0893C780 and the constants +0x34.. / +0x6CC...
// 2. The wheel travel +0xCC integrates with the time step func_0895549C over the spring force +0xDC, damper force +0xE0
//    (per the axle's mass +0x40) and gravity (constants +0x3C * 9.8), clamped to the axle's range [+4, +8]. Above the
//    ground height +0xC8 the wheel is airborne: rate +0xD4 kept within [-1, 1], damper func_0893D6A4, no load. On it
//    the travel is held at the ground, the rate follows the ground (constants +0x4F8), the spring and bump stop
//    (+0x14, +0x3C below +0) and the damper (with the bump term +0x48, plus func_089584F4 when state+0x140E is set)
//    give the load +0x54 (zero below 0.0098). Byte +0xE8 records the contact change (0 none, 1 kept, 2 landed, 3 left).
//    func_0893D064 and func_0893D0F4 then fill +0xC and +0x1C from the travel past the axle's +0x10.
// 3. Each axle's anti-roll term (by +0x5C: clamped constants +0x720 x +0x50 or a squared spring +0x38) and the
//    roll / pitch terms (constants +0x6B8, +0x6A4, +0x688 / +0x68C, mode byte 0x08CDE508+0x185) give each wheel's +0xD8.
void func_0893D9AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t constants = word(rt, car + 8u);
    const std::uint32_t state = word(rt, car + 0xCu);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto wheel_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x08955300u, {car, index}); };
    const auto axle_of = [&](std::uint32_t index) { return state + 0x360u + (index >> 1) * 0x74u; };
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t wheel = wheel_of(i);
        const std::uint32_t pair = state + (i >> 1) * 4u;
        float lateral = psp_mul(f(pair + 0x28u), f(constants + 0x6F4u));
        const float longitudinal = psp_mul(f(pair + 0x10u), f(constants + 0x6F0u));
        lateral = (i & 1u) != 0u ? 0.0f - lateral : lateral + 0.0f;
        const float shift = i < 2u ? lateral + longitudinal : lateral - longitudinal;
        const std::uint32_t geometry = call_set(rt, ctx, 0x0893C780u, {car, i});
        const float x = f(constants + 0x6CCu) + psp_mul(shift, f(constants + 0x34u));
        const float y = f(constants + 0x6D0u) + psp_mul(shift, f(constants + 0x38u));
        const float z = f(constants + 0x6D4u) + psp_mul(shift, f(constants + 0x3Cu));
        put_float(rt, wheel + 0xE4u,
                  (psp_mul(x, f(geometry + 0x18u)) - psp_mul(f(geometry + 0x20u), y)) + psp_mul(z, f(geometry + 0x1Cu)));
    }
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t wheel = wheel_of(i);
        const std::uint32_t axle_index = i >> 1;
        const std::uint32_t axle = axle_of(i);
        const std::uint32_t params = word(rt, car + 8u);
        const float high = f(axle + 8u);
        const float ground = f(wheel + 0xC8u);
        const float old_load = f(wheel + 0x54u);
        float spring = f(axle + 0x14u);
        const float step = call_setf(rt, ctx, 0x0895549Cu, {car}, {}).f0;
        const float accel = (f(wheel + 0xDCu) + f(wheel + 0xE0u)) / f(axle + 0x40u) + psp_mul(f(params + 0x3Cu), 9.8f);
        float rate = f(wheel + 0xD4u);
        const float travel = f(wheel + 0xCCu);
        float position = travel + psp_mul(step, rate + psp_mul(step, psp_mul(accel, 0.5f)));
        if (high < position) {
            position = high;
        } else if (position < f(axle + 4u)) {
            position = f(axle + 4u);
        }
        float load;
        if (position < ground) {
            put_float(rt, wheel + 0xDCu, psp_mul(spring, -position));
            if (position == high) {
                rate = 0.0f;
                put_float(rt, wheel + 0xD4u, rate);
            } else {
                // the asm adds the step and the acceleration here
                rate = rate + (step + accel);
                put_float(rt, wheel + 0xD4u, rate);
                if (1.0f < rate) {
                    rate = 1.0f;
                    put_float(rt, wheel + 0xD4u, rate);
                } else if (rate < -1.0f) {
                    rate = -1.0f;
                    put_float(rt, wheel + 0xD4u, rate);
                }
            }
            put_float(rt, wheel + 0xE0u, call_setf(rt, ctx, 0x0893D6A4u, {axle, axle_index}, {rate}).f0);
            put_float(rt, wheel + 0xCCu, position);
            put_float(rt, wheel + 0x54u, 0.0f);
            load = 0.0f;
        } else {
            float held = f(axle + 4u);
            if (!(ground < held)) held = ground;
            const float compression = held - f(axle);
            spring = psp_mul(spring, -held);
            if (compression < 0.0f) spring = spring + psp_mul(psp_mul(compression, compression), f(axle + 0x3Cu));
            float follow = psp_mul(f(params + 0x4F8u), ground - travel);
            put_float(rt, wheel + 0xD4u, follow);
            float damper = call_setf(rt, ctx, 0x0893D6A4u, {axle, axle_index}, {follow}).f0;
            if (follow < 0.0f && compression < 0.0f) {
                if (follow < -1.0f) follow = -1.0f;
                damper = damper - psp_mul(follow, f(axle + 0x48u));
            }
            if (byte_u(rt, word(rt, car + 0xCu) + 0x140Eu) != 0u) {
                damper = damper + call_setf(rt, ctx, 0x089584F4u, {car, i}, {held}).f0;
            }
            put_float(rt, wheel + 0xCCu, held);
            put_float(rt, wheel + 0xDCu, spring);
            put_float(rt, wheel + 0xE0u, damper);
            load = (damper + spring) + f(axle + 0x44u);
            put_float(rt, wheel + 0x54u, load);
            if (load < 0.0f) put_float(rt, wheel + 0x54u, 0.0f);
            load = f(wheel + 0x54u);
            if (load < std::bit_cast<float>(0x3C20902Fu)) {
                load = 0.0f;
                put_float(rt, wheel + 0x54u, 0.0f);
            }
        }
        std::uint32_t contact;
        if (0.0f < old_load) {
            contact = load == 0.0f ? 3u : 1u;
        } else {
            contact = load <= 0.0f ? 0u : 2u;
        }
        put8(rt, wheel + 0xE8u, contact);
        const float past = f(wheel + 0xCCu) - f(axle + 0x10u);
        put_float(rt, wheel + 0xCu, call_setf(rt, ctx, 0x0893D064u, {state, i}, {f(constants + 0x520u), past}).f0);
        put_float(rt, wheel + 0x1Cu, call_setf(rt, ctx, 0x0893D0F4u, {state, i}, {f(constants + 0x520u), past}).f0);
    }
    float roll[2];
    for (std::uint32_t axle_index = 0u; axle_index < 2u; ++axle_index) {
        const std::uint32_t axle = state + 0x360u + axle_index * 0x74u;
        const std::uint32_t left = wheel_of(axle_index * 2u);
        const std::uint32_t right = wheel_of(axle_index * 2u + 1u);
        float difference = f(left + 0xCCu) - f(right + 0xCCu);
        float limit = f(constants + 0x720u);
        if (byte_u(rt, axle + 0x5Cu) == 1u) {
            const float gain = f(axle + 0x50u);
            difference = psp_mul(difference, f(axle + 0x38u));
            if (0.0f < gain) {
                if (4.9f < limit) {
                    limit = 4.9f;
                } else if (limit < -4.9f) {
                    limit = -4.9f;
                }
                difference = difference - psp_mul(limit, gain);
            }
        } else if (0.0f <= difference) {
            difference = psp_mul(difference, psp_mul(difference, f(axle + 0x38u)));
        } else {
            difference = psp_mul(difference, psp_mul(difference, -f(axle + 0x38u)));
        }
        roll[axle_index] = difference;
    }
    const std::uint32_t mode_address = 0x08CDE508u + 0x185u;
    float pitch = 0.0f;
    if (byte_u(rt, mode_address) == 0u) pitch = psp_mul(f(state + 0x54u), -f(constants + 0x6B8u));
    const float yaw = psp_mul(f(constants + 0x6A4u), f(state + 0x58u));
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t wheel = wheel_of(i);
        const std::uint32_t axle_index = i >> 1;
        const bool odd = (i & 1u) != 0u;
        float value = f(wheel + 0xDCu);
        if (f(wheel + 0x54u) == 0.0f) value = -f(state + 0x360u + axle_index * 0x74u + 0x44u);
        value = odd ? value + roll[axle_index] : value - roll[axle_index];
        value = i < 2u ? yaw + (value - psp_mul(f(constants + 0x688u), 0.5f))
                       : (value - psp_mul(f(constants + 0x68Cu), 0.5f)) - yaw;
        const std::uint32_t mode = byte_u(rt, mode_address);
        if (mode == 1u) {
            const std::uint32_t axle = state + axle_index * 0x74u;
            const std::uint32_t side = state + axle_index * 4u;
            const float term = psp_mul((((f(wheel + 0xC8u) - f(axle + 0x36Cu)) + f(side + 0x318u)) + f(state + 0x13F8u)) /
                                           f(side + 0x28u),
                                       f(wheel + 0x80u));
            value = odd ? value + term : value - term;
        } else if (mode != 2u) {
            value = odd ? value - pitch : value + pitch;
        }
        put_float(rt, wheel + 0xD8u, value);
    }
    ctx.gpr[2] = 4u;
}


// func_08957C3C(car): steps the aid selected by state+0x140F (state = car+0xC; nothing when 0). Its on/off memory is
// constants+0x60E (constants = car+8; +0x60F for the second channel of mode 4); the thresholds are the speed
// constants+0x6C4 against state+0x1410 (low) / +0x1414 (high), the pedal constants+0x538, and for mode 4 the
// acceleration constants+0x6B8 / 9.8. The targets (two words from 0x08C8B49C unless a mode sets them) are approached
// by constants+0x610 (and +0x614 for mode 4's second channel) at the rate state+0x1418 (doubled for the second) per
// time step func_0895549C, without overshooting. Mode 2 takes its target from func_08957AD4(car, speed).
void func_08957C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    std::uint32_t state = word(rt, car + 0xCu);
    const std::uint32_t mode = byte_u(rt, state + 0x140Fu);
    if (mode == 0u) return;
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    float target[2] = {f(0x08C8B49Cu), f(0x08C8B4A0u)};
    const std::uint32_t constants = word(rt, car + 8u);
    const std::uint32_t latch = constants + 0x60Eu;
    const auto speed = [&] { return f(constants + 0x6C4u); };
    const auto pedal = [&] { return f(constants + 0x538u); };
    const auto set_latch = [&](std::uint32_t value) {
        put8(rt, latch, value);
        state = word(rt, car + 0xCu);
        return value;
    };
    constexpr float kTenKmh = std::bit_cast<float>(0x4031C71Du);
    std::uint32_t channels = 1u;
    switch (mode) {
    case 1u: {
        std::uint32_t on = byte_u(rt, latch);
        const float high = f(state + 0x1414u);
        const float low = f(state + 0x1410u);
        if (on == 0u) {
            if (high <= speed()) on = set_latch(1u);
        } else if (speed() < low) {
            on = set_latch(0u);
        }
        target[0] = on == 1u ? 1.0f : 0.0f;
        break;
    }
    case 2u:
        target[0] = call_setf(rt, ctx, 0x08957AD4u, {car}, {speed()}).f0;
        break;
    case 4u: {
        std::uint32_t on = byte_u(rt, latch);
        const float low = f(state + 0x1410u);
        const float high = f(state + 0x1414u);
        if (on == 0u) {
            if (high <= speed() && 0.5f <= pedal()) {
                put8(rt, latch, 1u);
                on = 1u;
            }
        } else if (speed() < low || pedal() < 0.1f) {
            put8(rt, latch, 0u);
            on = 0u;
        }
        if (on == 1u) target[0] = 1.0f;
        std::uint32_t side = byte_u(rt, constants + 0x60Fu);
        const float lateral = f(constants + 0x6B8u) / 9.8f;
        if (side == 0u) {
            if (0.5f <= lateral) {
                side = 1u;
                put8(rt, constants + 0x60Fu, side);
            } else if (lateral <= -0.5f) {
                side = 2u;
                put8(rt, constants + 0x60Fu, side);
            }
        } else if (side == 1u) {
            if (lateral < 0.2f) {
                side = 0u;
                put8(rt, constants + 0x60Fu, side);
            }
        } else if (-0.2f < lateral) {
            side = 0u;
            put8(rt, constants + 0x60Fu, side);
        }
        state = word(rt, car + 0xCu);
        if (side == 1u) {
            target[1] = 1.0f;
        } else if (side == 2u) {
            target[1] = -1.0f;
        }
        channels = 2u;
        break;
    }
    case 5u: {
        std::uint32_t on = byte_u(rt, latch);
        const float high = f(state + 0x1414u);
        const float low = f(state + 0x1410u);
        if (on == 0u) {
            if (high <= speed() && 0.5f <= pedal()) on = set_latch(1u);
        } else if (speed() < low) {
            on = set_latch(0u);
        } else if (pedal() < 0.1f) {
            on = set_latch(0u);
        }
        if (on == 1u) target[0] = 1.0f;
        break;
    }
    case 6u: {
        std::uint32_t phase = byte_u(rt, latch);
        const float high = f(state + 0x1414u);
        const float low = f(state + 0x1410u);
        if (phase == 0u) {
            if (!(speed() < kTenKmh) && !(pedal() < 0.5f)) {
                phase = set_latch(2u);
            } else if (high <= speed()) {
                phase = set_latch(1u);
            }
        } else if (phase == 1u) {
            if (!(speed() < kTenKmh) && !(pedal() < 0.5f)) {
                phase = set_latch(2u);
            } else if (speed() < low) {
                phase = set_latch(0u);
            }
        } else if (phase == 2u) {
            if (pedal() < 0.1f) {
                phase = low <= speed() ? set_latch(1u) : set_latch(0u);
            } else if (speed() < std::bit_cast<float>(0x3F0E38E4u)) {
                phase = set_latch(0u);
            }
        }
        target[0] = phase == 2u ? 1.0f : phase == 1u ? std::bit_cast<float>(0x3E1D89D9u) : 0.0f;
        break;
    }
    case 8u: {
        std::uint32_t phase = byte_u(rt, latch);
        const float low = f(state + 0x1410u);
        const float high = f(state + 0x1414u);
        const float now = speed();
        if (phase == 0u) {
            if (low <= now) phase = set_latch(1u);
        } else if (phase == 1u) {
            if (now < low - kTenKmh) {
                phase = set_latch(0u);
            } else if (high < now) {
                phase = set_latch(2u);
            }
        } else if (now < high - kTenKmh) {
            phase = set_latch(1u);
        }
        target[0] = phase == 1u ? 1.0f : 0.0f;
        break;
    }
    default:
        break;
    }
    std::uint32_t output = constants + 0x610u;
    for (std::uint32_t channel = 0u; channel < channels; ++channel, output += 4u) {
        if (channel != 0u) state = word(rt, car + 0xCu);
        ctx.gpr[2] = 1u;
        float rate = f(state + 0x1418u);
        if (channel == 1u) rate = rate + rate;
        const float current = f(output);
        const float goal = target[channel];
        if (current < goal) {
            const float next = current + psp_mul(rate, call_setf(rt, ctx, 0x0895549Cu, {car}, {}).f0);
            put_float(rt, output, next);
            if (goal < next) put_float(rt, output, goal);
        } else if (goal < current) {
            const float next = current - psp_mul(rate, call_setf(rt, ctx, 0x0895549Cu, {car}, {}).f0);
            put_float(rt, output, next);
            if (next < goal) put_float(rt, output, goal);
        }
    }
}


// func_08945664(car, control): traction control (constants = car+8, state = car+0xC, drivetrain = state+0x270).
// constants+0x731 takes func_08945630(control); with no active state there (or when func_089557BC(car) says so, or
// +0x5F9 is clear) it only tail-calls func_08945654(constants). Otherwise the driven wheels (by drivetrain+0: 1 front,
// 0 and 5-7 rear, else all) give the largest slip speed (func_08B49CC8 of the wheel speed against the target in byte
// 0x08CDE508+0x167 when allowed, else the contact velocity of func_089554A8), the lowest grip ratio +0x3C (with its tyre
// +0x164 and wheel +0x38 terms) and the load-weighted torque sum; these, the throttle +0x534, the boost bytes
// 0x08CDE508+0x190.. and the controller's gains (+0x20 .. +0x2C) give a cut factor that scales +0x534. Above 100 km/h
// the factor drives a spring filter (+0x570 / +0x574) instead and +0x534 follows it when lower. +0x731 becomes 2 when
// the cut engages.
void func_08945664(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    const std::uint32_t control = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const std::uint32_t constants = word(rt, car + 8u);
    const std::uint32_t active = call_set(rt, ctx, 0x08945630u, {control});
    const std::uint32_t state = word(rt, car + 0xCu);
    const std::int32_t kind = signed_of(sext8(byte_u(rt, constants + 3u)));
    put8(rt, constants + 0x731u, active);
    const std::uint32_t drivetrain = state + 0x270u;
    const bool assisted = kind == 2 || byte_u(rt, constants + 0xEu) != 0u;
    const std::uint32_t opponent = call_set(rt, ctx, 0x0894D050u, {word(rt, car)});
    std::uint32_t follower = 0u;
    std::uint32_t lead = 0u;
    if (!assisted) {
        if (signed_of(call_set(rt, ctx, 0x089445D8u, {car, 1u})) > 0) {
            follower = call_set(rt, ctx, 0x089444F8u, {car, 1u, 0u, 0u});
            if (follower == 0u) put8(rt, constants + 0x731u, 0u);
        }
        lead = opponent;
    }
    bool manual;
    if (byte_u(rt, constants + 0x5F8u) == 0u) {
        manual = true;
    } else {
        manual = opponent == 0u && byte_u(rt, constants + 0x69Du) == 4u;
    }
    const auto finish = [&] { tail_call(ctx, 0x08945654u, {constants}); };
    if (byte_u(rt, constants + 0x731u) == 0u && !manual) return finish();
    if (call_set(rt, ctx, 0x089557BCu, {car}) != 0u) return finish();
    const bool limit_slip = lead == 0u && !manual && !assisted;
    ctx.gpr[2] = lead;
    if (f(constants + 0x57Cu) <= 0.0f) return;
    const std::uint32_t enabled = byte_u(rt, constants + 0x5F9u);
    ctx.gpr[2] = enabled;
    if (enabled == 0u) return finish();
    if (!(0.0f < f(constants + 0x534u))) return;
    float grip = 1.0f;
    float tyre = 1.0f;
    float wheel_term = 1.0f;
    float slip = 0.0f;
    float torque = 0.0f;
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t layout = byte_u(rt, drivetrain);
        bool driven;
        if (layout == 1u) {
            driven = index < 2u;
        } else if (layout == 0u || layout - 5u <= 2u) {
            driven = index >= 2u;
        } else {
            driven = true;
        }
        if (!driven) continue;
        const std::uint32_t wheel = call_set(rt, ctx, 0x08955300u, {car, index});
        const std::uint32_t tyre_state = call_set(rt, ctx, 0x0894268Cu, {state, index, byte_u(rt, wheel + 0xE9u)});
        const float ratio = f(wheel + 0x3Cu);
        bool loaded;
        if (limit_slip) {
            float speed;
            if (byte_u(rt, tyre_state + 0x251u) == 0u) {
                const float factor = call_setf(rt, ctx, 0x08B49CC8u, {car}, {f(wheel + 0x24u)}).f0;
                speed = psp_mul(factor, psp_mul(static_cast<float>(signed_of(byte_u(rt, 0x08CDE508u + 0x167u))),
                                                std::bit_cast<float>(0x3E8E38E4u)));
            } else {
                const std::uint32_t velocity = call_set(rt, ctx, 0x089554A8u, {car, index});
                speed = psp_mul(f(velocity + 0x10u), f(wheel + 0x2Cu)) + psp_mul(f(velocity + 0xCu), f(wheel + 0x30u));
            }
            if (speed < 0.0f) speed = -speed;
            loaded = 0.0f < f(wheel + 0x54u);
            if (loaded && slip < speed) slip = speed;
        } else {
            loaded = 0.0f < f(wheel + 0x54u);
        }
        if (loaded && ratio < grip) {
            grip = ratio;
            tyre = f(tyre_state + 0x164u);
            wheel_term = f(wheel + 0x38u);
        }
        torque = torque + psp_mul(f(wheel + 0x58u), f(drivetrain + (index >> 1) * 4u + 0xA8u));
    }
    if (!manual) tyre = psp_mul(tyre, f(control + 0x2Cu));
    constexpr std::uint32_t kRace = 0x08CDE508u;
    if (byte_u(rt, kRace + 0x190u) != 0u) {
        const float unit = std::bit_cast<float>(0x4031C71Du);
        const float speed = f(constants + 0x6C4u);
        const float start = psp_mul(static_cast<float>(signed_of(byte_u(rt, kRace + 0x191u))), unit);
        if (start < speed) {
            const float end = psp_mul(static_cast<float>(signed_of(byte_u(rt, kRace + 0x192u))), unit);
            float boost = psp_mul(static_cast<float>(signed_of(byte_u(rt, kRace + 0x193u))), std::bit_cast<float>(0x3C23D70Au));
            if (speed < end) boost = psp_mul(boost, (speed - start) / (end - start));
            tyre = psp_mul(tyre, boost + 1.0f);
        }
    }
    const std::uint32_t gear_side = byte_u(rt, constants + 0x5F8u);
    float drive = psp_mul(f(constants + 0x6ACu), f(drivetrain + byte_u(rt, drivetrain + 2u) * 4u + 0xA8u)) + torque;
    if (gear_side == 0u) drive = -drive;
    float force = drive / f(drivetrain + gear_side * 4u + 0x58u);
    if (call_set(rt, ctx, 0x0895BBC4u, {drivetrain}) != 0u && byte_u(rt, constants + 0x5F8u) != 0u) {
        force = drive / psp_mul(f(drivetrain + 0x5Cu), f(constants + 0x5F0u));
    }
    const float limit = f(constants + 0x57Cu);
    const float demand = force / limit;
    const float gain = psp_mul(manual ? 3.0f : f(control + 0x20u), wheel_term);
    float cut;
    if (lead != 0u) {
        const std::uint32_t rpm = half_u(rt, state + 0x23Cu);
        const std::int32_t excess =
            signed_of((rpm + half_u(rt, drivetrain + 0x36u)) >> 1) - signed_of(sext16(half_u(rt, constants + 0x58Eu)));
        cut = psp_mul(psp_mul(f(control + 0x20u), 5.0f), static_cast<float>(excess)) / static_cast<float>(signed_of(rpm));
    } else {
        cut = demand + psp_mul(gain, grip - tyre);
    }
    if (cut < 0.0f) {
        cut = 0.0f;
    } else if (1.0f < cut) {
        cut = 1.0f;
    }
    const float reserve = f(constants + 0x580u);
    const float floor = reserve / (limit + reserve);
    const float span = 1.0f - floor;
    float factor = floor + psp_mul(cut, span);
    std::uint32_t v0 = limit_slip ? 1u : 0u;
    if (limit_slip) {
        const float threshold = f(control + 0x28u);
        float capped = 1.0f;
        if (threshold < slip) {
            float scale;
            if (follower != 0u) {
                scale = 1.0f - psp_mul(slip - threshold, call_setf(rt, ctx, 0x0894F1A0u, {follower}, {}).f0);
            } else {
                scale = 1.0f - psp_mul(slip - threshold, f(control + 0x24u));
            }
            if (scale < 0.0f) scale = 0.0f;
            v0 = 0x08CF0000u;
            float part = psp_mul(scale, span);
            if (word(rt, 0x08CF2EA0u) != 0u) {
                scale = psp_mul(scale, psp_mul(demand, f(0x08CF2E9Cu)));
                if (1.0f < scale) scale = 1.0f;
                part = psp_mul(scale, span);
            }
            capped = floor + part;
        }
        if (capped < factor) factor = capped;
    }
    if (f(constants + 0x6C4u) < std::bit_cast<float>(0x41DE38E4u)) {
        if (factor < 1.0f && byte_u(rt, constants + 0x731u) != 0u) put8(rt, constants + 0x731u, 2u);
        put_float(rt, constants + 0x534u, psp_mul(factor, f(constants + 0x534u)));
        return finish();
    }
    const float throttle = f(constants + 0x534u);
    float position = f(constants + 0x570u);
    float velocity = f(constants + 0x574u);
    const float sixtieth = std::bit_cast<float>(0x3C88AB7Cu);
    velocity = velocity + psp_mul(psp_mul(psp_mul(psp_mul(factor, throttle), 1.1f) - position, 500.0f) + psp_mul(velocity, -50.0f), sixtieth);
    put_float(rt, constants + 0x574u, velocity);
    position = position + psp_mul(velocity, sixtieth);
    put_float(rt, constants + 0x570u, position);
    if (throttle < position) {
        put_float(rt, constants + 0x570u, throttle);
        put(rt, constants + 0x574u, 0u);
    } else if (position < 0.0f) {
        put_float(rt, constants + 0x574u, 0.0f);
        put_float(rt, constants + 0x570u, 0.0f);
    }
    const float filtered = f(constants + 0x570u);
    if (filtered < throttle) {
        if (byte_u(rt, constants + 0x731u) == 0u) {
            v0 = 0u;
        } else {
            v0 = 2u;
            put8(rt, constants + 0x731u, 2u);
        }
        put_float(rt, constants + 0x534u, f(constants + 0x570u));
    }
    ctx.gpr[2] = v0;
}


// func_08946EC0(car): brake assist and wheel drive force (constants = car+8, state = car+0xC, settings = car->0 +
// 0x64). While func_0895EA38 / func_0895EA44 of the settings enable it (and constants+0xC bit 0x10 is clear), each
// wheel's slip +0xB4 (EA38) or +0xB0 (EA44) is mapped through the settings' piecewise thresholds to a grip factor
// +0x5C (multiplied in when both are on) and, unless the other mode wins, an indicator byte +0xB9; the factor is then
// held at least at the tyre's +0x68 (func_0894268C). Then, per wheel, the tyre (state+0x4A0 + 0x268 * slot from
// state+0x1318) sets the contact record of func_089554A8 (+0x20 slot % 3, +0x21 flags) and the drive force +0x58 is the
// load +0x54 scaled by func_08946C58 / func_089467AC, the surface (+0x228 when the geometry of func_0893C780 has bit
// 0x40 in +0x2F), the grip factor, +0xA8 (when func_0895EB5C allows), the axle's state+0x1350 and the tyre grip at
// +0x8 + 4 * compound; the steering terms adjust the contact's +0x14.
void func_08946EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto trunc = [&](float value) { return ctx.fpu_float_to_word(value, 1u); };
    const std::uint32_t settings = word(rt, car) + 0x64u;
    const std::uint32_t constants = word(rt, car + 8u);
    const std::uint32_t state = word(rt, car + 0xCu);
    const auto slip_mode = [&] { return call_set(rt, ctx, 0x0895EA38u, {settings}) != 0u; };
    const auto lock_mode = [&] { return call_set(rt, ctx, 0x0895EA44u, {settings}) != 0u; };
    const auto wheel_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x08955300u, {car, index}); };
    constexpr float kFine = std::bit_cast<float>(0x427F999Au);
    constexpr float kCoarse = std::bit_cast<float>(0x41FF3333u);
    bool grip_scaled = false;
    if ((slip_mode() || lock_mode()) && (byte_u(rt, constants + 0xCu) & 0x10u) == 0u) {
        if (slip_mode()) {
            for (std::uint32_t index = 0u; index < 4u; ++index) {
                const std::uint32_t wheel = wheel_of(index);
                const float slip = f(wheel + 0xB4u);
                const float top = f(settings + 0x18u);
                std::uint32_t indicator;
                if (top <= slip) {
                    put_float(rt, wheel + 0xB4u, top);
                    put_float(rt, wheel + 0x5Cu, 1.0f - f(settings + 0x1Cu));
                    if (lock_mode()) continue;
                    indicator = 0x7Fu;
                } else if (f(settings + 0x10u) <= slip) {
                    const float from = f(settings + 0x10u);
                    const float t = (slip - from) / (top - from);
                    const float base = f(settings + 0x14u);
                    put_float(rt, wheel + 0x5Cu, (1.0f - base) - psp_mul(t, f(settings + 0x1Cu) - base));
                    if (lock_mode()) continue;
                    indicator = 1.0f <= t ? 0x7Fu : trunc(psp_mul(t, kFine)) + 0x40u;
                } else if (f(settings + 8u) <= slip) {
                    const float from = f(settings + 8u);
                    const float t = (slip - from) / (f(settings + 0x10u) - from);
                    const float base = f(settings + 0xCu);
                    put_float(rt, wheel + 0x5Cu, (1.0f - base) - psp_mul(t, f(settings + 0x14u) - base));
                    if (lock_mode()) continue;
                    indicator = 1.0f <= t ? 0x40u : trunc(psp_mul(t, kCoarse)) + 0x20u;
                } else if (0.0f <= slip) {
                    const float t = slip / f(settings + 8u);
                    put_float(rt, wheel + 0x5Cu, 1.0f - psp_mul(t, f(settings + 0xCu)));
                    if (lock_mode()) continue;
                    indicator = 1.0f <= t ? 0x20u : trunc(psp_mul(t, kCoarse));
                } else if (slip < f(settings)) {
                    put_float(rt, wheel + 0x5Cu, 1.0f);
                    if (lock_mode()) continue;
                    indicator = 0x80u;
                } else if (lock_mode()) {
                    put_float(rt, wheel + 0x5Cu, 1.0f);
                    if (lock_mode()) continue;
                    indicator = 0x80u;
                } else {
                    const float t = f(wheel + 0xB4u) / f(settings);
                    put_float(rt, wheel + 0x5Cu, 1.0f - psp_mul(t, f(settings + 4u)));
                    indicator = 1.0f <= t ? 0xC1u : 0u - trunc(psp_mul(t, kFine));
                }
                put8(rt, wheel + 0xB9u, indicator);
            }
        }
        if (lock_mode()) {
            for (std::uint32_t index = 0u; index < 4u; ++index) {
                const std::uint32_t wheel = wheel_of(index);
                const float slip = f(wheel + 0xB0u);
                const float start = f(settings + 0x58u);
                float factor;
                std::uint32_t indicator;
                if (start < slip) {
                    float t = (slip - start) / (f(settings + 0x60u) - start);
                    float scaled;
                    if (1.0f < t) {
                        scaled = kFine;
                        t = 1.0f;
                    } else {
                        scaled = psp_mul(t, kFine);
                    }
                    const float base = f(settings + 0x5Cu);
                    indicator = trunc(scaled) + 0x40u;
                    factor = (1.0f - base) - psp_mul(t, f(settings + 0x64u) - base);
                } else if (0.0f <= slip) {
                    const float t = slip / start;
                    indicator = trunc(psp_mul(t, kFine));
                    factor = 1.0f - psp_mul(t, f(settings + 0x5Cu));
                } else {
                    const float low = f(settings + 0x50u);
                    factor = 1.0f;
                    indicator = 0x80u;
                    if (low <= slip) {
                        const float t = slip / low;
                        indicator = 0u - trunc(psp_mul(t, kFine));
                        factor = 1.0f - psp_mul(t, f(settings + 0x54u));
                    }
                }
                put8(rt, wheel + 0xB9u, indicator);
                if (slip_mode()) factor = psp_mul(factor, f(wheel + 0x5Cu));
                put_float(rt, wheel + 0x5Cu, factor);
            }
        }
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            const std::uint32_t tyre = call_set(rt, ctx, 0x0894268Cu, {state, index, byte_u(rt, wheel + 0xE9u)});
            const float least = f(tyre + 0x68u);
            if (f(wheel + 0x5Cu) < least) put_float(rt, wheel + 0x5Cu, least);
        }
        grip_scaled = true;
    }
    const bool use_side = byte_u(rt, constants + 9u) == 1u && call_set(rt, ctx, 0x0895EB5Cu, {car}) != 0u;
    const float toe = f(constants + 0x700u) - f(constants + 0x70Cu);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t axle = index >> 1;
        const std::uint32_t wheel = wheel_of(index);
        const std::uint32_t contact = call_set(rt, ctx, 0x089554A8u, {car, index});
        const std::uint32_t slot = byte_u(rt, state + axle * 24u + byte_u(rt, wheel + 0xE9u) + 0x1318u);
        put8(rt, contact + 0x20u, slot % 3u);
        const std::uint32_t tyre = state + slot * 0x268u + 0x4A0u;
        const std::uint32_t grip = tyre + 8u;
        put8(rt, contact + 0x21u, (byte_u(rt, grip + 0x251u) << 1) | byte_u(rt, grip + 0x250u));
        float force = f(wheel + 0x54u);
        const float shaped = call_setf(rt, ctx, 0x08946C58u, {state, index}, {force}).f0;
        force = psp_mul(force, call_setf(rt, ctx, 0x089467ACu, {tyre + 0x22Cu}, {shaped}).f0);
        const std::uint32_t geometry = call_set(rt, ctx, 0x0893C780u, {car, index});
        if ((byte_u(rt, geometry + 0x2Fu) & 0x40u) != 0u) force = psp_mul(force, f(word(rt, car) + 0x228u));
        if (grip_scaled) force = psp_mul(force, f(wheel + 0x5Cu));
        if (use_side) force = psp_mul(force, f(wheel + 0xA8u));
        force = psp_mul(force, f(state + axle * 4u + 0x1350u));
        const bool even = (index & 1u) == 0u;
        float steer = (even ? -toe : toe) + f(wheel + 0x1Cu);
        if (index < 2u) {
            const float turn = psp_mul(f(wheel + 0x10u), f(state + 0x1394u));
            steer = even ? steer + turn : steer - turn;
        }
        float camber = psp_mul(steer, f(state + axle * 4u + 0x4A0u));
        if (1.0f < camber) {
            camber = 1.0f;
        } else if (camber < -1.0f) {
            camber = -1.0f;
        }
        const float shift = psp_mul(camber, psp_mul(f(state + axle * 4u + 0x328u), 0.5f));
        put_float(rt, contact + 0x14u, even ? f(contact + 0x14u) - shift : shift + f(contact + 0x14u));
        const float curve = call_setf(rt, ctx, 0x089467ACu, {state + axle * 12u + 0x448u}, {steer}).f0;
        put_float(rt, wheel + 0x58u, psp_mul(curve, psp_mul(force, f(grip + byte_u(rt, wheel + 0xE9u) * 4u + 8u))));
    }
    ctx.gpr[2] = 4u;
}


// func_0893E0FC(car): body motion for one step (constants = car+8, state = car+0xC, dt = func_0895549C(car)). Each
// wheel's vertical load change (contact +4 x travel rates +0x88/+0x8C less +0x48, scaled per axle by state+0x318 and
// state+0x160 / +0x168 or +0x10 by the suspension byte 0x08CDE508+0x159 / +0x15A) is collected; unless func_0893E0F4
// stops here, the pitch (constants+0x6F0, within +-pi/2) and roll (+0x6F4, within +-4pi) rates integrate the per-axle
// spring / damper sums (with the anti-roll term when byte 0x08CDE508+0x185 is 2), through func_08959AC0 /
// func_08959B0C for kind 4, unless func_089557BC says the car is held. Then func_08946598, the angles +0x6FC / +0x700
// integrate +0x6E4 / +0x6E8 (kept within +-0.611 unless func_0894CCB8), func_0893C3E4 updates the orientation, and the
// heave acceleration +0x6BC and velocity +0x6CC.. follow from the summed forces and gravity: by the vertical axis
// +0x34 when byte 0x08CDE508+0x160 is 0, else by each wheel's push along +0x64 (or its geometry for mode 2) with the
// mode-7 ride-height term state+0x141C (and func_089599F0 for kind 4), adding the load changes to +0x54 in mode 2.
void func_0893E0FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto wheel_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x08955300u, {car, index}); };
    constexpr std::uint32_t kRace = 0x08CDE508u;
    const std::uint32_t constants = word(rt, car + 8u);
    const std::uint32_t state = word(rt, car + 0xCu);
    const float dt = call_setf(rt, ctx, 0x0895549Cu, {car}, {}).f0;
    const std::uint32_t axles = word(rt, car + 0xCu);
    float load[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const std::uint32_t kind_source = word(rt, car + 8u);
    const bool alternate = signed_of(sext8(byte_u(rt, kind_source + 3u))) == 2 || byte_u(rt, kind_source + 0xEu) != 0u;
    const bool per_wheel = byte_u(rt, kRace + (alternate ? 0x15Au : 0x159u)) == 2u;
    float heave = 0.0f;
    float pitch_sum = 0.0f;
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        const std::uint32_t contact = call_set(rt, ctx, 0x089554A8u, {car, index});
        const std::uint32_t side = axles + (index >> 1) * 4u;
        const float change = psp_mul(psp_mul(f(contact + 4u), f(wheel + 0x88u) + f(wheel + 0x8Cu)) - f(wheel + 0x48u),
                                     f(side + 0x318u));
        pitch_sum = pitch_sum + psp_mul(change, f(side + 0x160u));
        const float value = per_wheel ? psp_mul(-change, f(side + 0x168u)) : change / f(side + 0x10u);
        if (index < 2u) {
            heave = heave + value;
            load[index] = value;
        } else {
            heave = heave - value;
            load[index] = -value;
        }
    }
    if (call_set(rt, ctx, 0x0893E0F4u, {car}) != 0u) return;
    if (call_set(rt, ctx, 0x089557BCu, {car}) == 0u) {
        float sum[2] = {0.0f, 0.0f};
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            sum[index >> 1] = f(wheel_of(index) + 0xD8u) + sum[index >> 1];
        }
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            float damper = f(wheel_of(index) + 0xE0u);
            if (f(wheel_of(index) + 0x54u) == 0.0f) damper = 0.0f;
            sum[index >> 1] = damper + sum[index >> 1];
        }
        float pitch = f(constants + 0x6F0u) +
                      psp_mul(dt, (psp_mul(sum[0], f(state + 0x150u)) - psp_mul(sum[1], f(state + 0x154u))) + pitch_sum);
        if (signed_of(sext8(byte_u(rt, constants + 3u))) == 4) pitch = call_setf(rt, ctx, 0x08959AC0u, {car}, {pitch}).f0;
        const float quarter_turn = std::bit_cast<float>(0x3FC90FDBu);
        if (quarter_turn < pitch) {
            pitch = quarter_turn;
        } else if (pitch < -quarter_turn) {
            pitch = -quarter_turn;
        }
        put_float(rt, constants + 0x6F0u, pitch);
        sum[0] = 0.0f;
        sum[1] = 0.0f;
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            const std::uint32_t axle = index >> 1;
            const float spring = f(wheel + 0xD8u);
            sum[axle] = (index & 1u) == 0u ? spring + sum[axle] : sum[axle] - spring;
            if (byte_u(rt, kRace + 0x185u) == 2u) {
                const float lift = ((f(wheel + 0xC8u) - f(state + axle * 0x74u + 0x36Cu)) + f(state + axle * 4u + 0x318u)) +
                                   f(state + 0x13F8u);
                sum[axle] = sum[axle] - psp_mul(psp_mul(psp_mul(f(wheel + 0x80u), lift), f(wheel + 0xD0u)), 0.5f);
            }
        }
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            float damper = f(wheel_of(index) + 0xE0u);
            if (f(wheel_of(index) + 0x54u) == 0.0f) damper = 0.0f;
            const std::uint32_t axle = index >> 1;
            sum[axle] = (index & 1u) == 0u ? damper + sum[axle] : sum[axle] - damper;
        }
        const std::uint32_t held = word(rt, car + 8u);
        float roll = f(constants + 0x6F4u) + psp_mul(dt, psp_mul(sum[0], f(state + 0x158u)) + psp_mul(sum[1], f(state + 0x15Cu)));
        call_setf(rt, ctx, 0x0895549Cu, {car}, {});
        const float full_turn = std::bit_cast<float>(0x41490FDBu);
        if (full_turn < roll) {
            roll = full_turn;
        } else if (roll < -full_turn) {
            roll = -full_turn;
        }
        if (signed_of(sext8(byte_u(rt, held + 3u))) == 4) roll = call_setf(rt, ctx, 0x08959B0Cu, {car}, {roll}).f0;
        put_float(rt, held + 0x6F4u, roll);
    }
    call_set(rt, ctx, 0x08946598u, {car});
    put_float(rt, constants + 0x6FCu, f(constants + 0x6FCu) + psp_mul(dt, f(constants + 0x6E4u)));
    put_float(rt, constants + 0x700u, f(constants + 0x700u) + psp_mul(dt, f(constants + 0x6E8u)));
    // a0 is whatever func_08946598 left there
    if (call_set(rt, ctx, 0x0894CCB8u, {}) == 0u) {
        const float tilt = std::bit_cast<float>(0x3F1C61AAu);
        for (std::uint32_t offset = 0u; offset < 8u; offset += 4u) {
            const float angle = f(constants + 0x6FCu + offset);
            if (tilt < angle) {
                put_float(rt, constants + 0x6FCu + offset, tilt);
                put(rt, constants + 0x6E4u + offset, 0u);
            } else if (angle < -tilt) {
                put_float(rt, constants + 0x6FCu + offset, -tilt);
                put(rt, constants + 0x6E4u + offset, 0u);
            }
        }
    }
    const std::uint32_t up = constants + 0x34u;
    call_set(rt, ctx, 0x0893C3E4u,
             {constants + 0x1Cu, constants + 0x28u, up, constants + 0x6FCu, constants + 0x700u, constants + 0x704u});
    const std::uint32_t push_mode = byte_u(rt, kRace + 0x160u);
    if (byte_u(rt, kRace + 0x186u) != 0u) {
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            const std::uint32_t axle = index >> 1;
            const float lift = (((f(wheel + 0xC8u) - f(state + axle * 0x74u + 0x36Cu)) + f(state + axle * 4u + 0x318u)) +
                                f(state + 0x13F8u)) /
                               f(state + axle * 4u + 0x28u);
            const float part = psp_mul(psp_mul(psp_mul(lift, f(wheel + 0x80u)), f(wheel + 0xD0u)), 0.5f);
            if ((index & 1u) == 0u) {
                heave = heave + part;
                load[index] = part + load[index];
            } else {
                heave = heave - part;
                load[index] = load[index] - part;
            }
        }
    }
    if (push_mode == 0u) {
        float total = 0.0f;
        for (std::uint32_t index = 0u; index < 4u; ++index) total = total + f(wheel_of(index) + 0xD8u);
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            float damper = f(wheel_of(index) + 0xE0u);
            if (f(wheel_of(index) + 0x54u) == 0.0f) damper = 0.0f;
            total = total + damper;
        }
        const float accel = (heave + total) / f(constants + 0x5DCu);
        put_float(rt, constants + 0x6BCu, accel + psp_mul(f(constants + 0x3Cu), -9.8f));
        const float step = psp_mul(dt, accel);
        for (std::uint32_t offset = 0u; offset < 12u; offset += 4u) {
            put_float(rt, constants + 0x6CCu + offset, f(constants + 0x6CCu + offset) + psp_mul(step, f(constants + 0x34u + offset)));
        }
        put_float(rt, constants + 0x6D4u, f(constants + 0x6D4u) + psp_mul(dt, -9.8f));
        ctx.gpr[2] = constants + 0xCu;
        return;
    }
    float force[3] = {0.0f, 0.0f, 0.0f};
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        float own[3];
        const float *axis;
        float shared[3];
        if (push_mode == 2u) {
            const std::uint32_t geometry = call_set(rt, ctx, 0x0893C780u, {car, index});
            own[0] = f(geometry + 0x18u);
            own[1] = -f(geometry + 0x20u);
            own[2] = f(geometry + 0x1Cu);
            axis = own;
        } else {
            for (std::uint32_t k = 0u; k < 3u; ++k) shared[k] = f(constants + 0x64u + k * 4u);
            axis = shared;
        }
        float along = 0.0f;
        for (std::uint32_t k = 0u; k < 3u; ++k) along = along + psp_mul(axis[k], f(up + k * 4u));
        const std::uint32_t wheel = wheel_of(index);
        float excess = f(wheel + 0x54u) - f(state + 0x360u + (index >> 1) * 0x74u + 0x44u);
        if (excess < 0.0f) excess = 0.0f;
        const float push = psp_mul(excess, along);
        for (std::uint32_t k = 0u; k < 3u; ++k) force[k] = force[k] + psp_mul(push, axis[k]);
    }
    float lift = heave - (f(constants + 0x688u) + f(constants + 0x68Cu));
    if (byte_u(rt, state + 0x140Fu) == 7u) {
        const float gap = (f(constants + 0x18u) - f(constants + 0x48u)) - (f(state + 0x5Cu) + 0.1f);
        const float ride = f(state + 0x141Cu);
        if (gap <= 0.0f) {
            lift = lift - ride;
        } else if (gap <= 0.1f) {
            lift = lift - psp_mul(ride, psp_mul(gap, -10.0f) + 1.0f);
        }
    }
    for (std::uint32_t k = 0u; k < 3u; ++k) force[k] = force[k] + psp_mul(lift, f(up + k * 4u));
    for (std::uint32_t k = 0u; k < 3u; ++k) force[k] = force[k] / f(constants + 0x5DCu);
    force[2] = force[2] - 9.8f;
    float vertical = 0.0f;
    for (std::uint32_t k = 0u; k < 3u; ++k) {
        put_float(rt, constants + 0x6CCu + k * 4u, f(constants + 0x6CCu + k * 4u) + psp_mul(dt, force[k]));
        vertical = vertical + psp_mul(force[k], f(up + k * 4u));
    }
    if (signed_of(sext8(byte_u(rt, constants + 3u))) == 4) {
        put_float(rt, constants + 0x6D4u, call_setf(rt, ctx, 0x089599F0u, {car}, {f(constants + 0x6D4u)}).f0);
    }
    put_float(rt, constants + 0x6BCu, vertical);
    ctx.gpr[2] = per_wheel ? 1u : 0u;
    if (!per_wheel) return;
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        float value = f(wheel + 0x54u) + load[index];
        put_float(rt, wheel + 0x54u, value);
        if (value < 0.0f) {
            value = 0.0f;
            put_float(rt, wheel + 0x54u, value);
        }
        if (value < std::bit_cast<float>(0x3C20902Fu)) put(rt, wheel + 0x54u, 0u);
    }
}


// func_0895FFBC(car, aids, active): the per-wheel drive / brake factors +0x84 (course = car->0, constants = car+8,
// state = car+0xC). When func_08944588(car) is clear each factor only takes func_08944768(car, axle, 1.0) (capped at 1).
// Otherwise (unless func_089557BC(car)) each wheel starts from its tyre's +0x168 (func_0894268C), blended toward the
// course grip (+0x3B4 / +0x3B8, through func_08946B38 / func_089446C0) for tyres with +0x250 set; per-axle factors come
// from the steering (constants+0x520 against course +0x3C8 / +0x3C0, only with func_0894D050 and byte 0x08CDE639) and
// state+0x13BC (through func_08944768 unless byte 0x08CF2B28). With `active` the aid record (TCS: +0x30/+0x31 with the
// slip thresholds +0..+0xF; stability: +0x32/+0x33 with +0x10..+0x1F) cuts or brakes single wheels from
// func_0895B668's brake requests and the throttle / brake pedals (+0x538 / +0x530, slips +0x558 / +0x550), may cut the
// throttle +0x534 (0x08CF2F90), and sets +0x730 (2 engaged, 1 armed by func_0895FF70). Finally +0x84 is clamped to 1
// and reshaped by the wheel's slip against the tyre curve (0.001 instead of 0).
void func_0895FFBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t car = ctx.gpr[4];
    const std::uint32_t aids = ctx.gpr[5];
    const std::uint32_t active = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0xD0u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto wheel_of = [&](std::uint32_t index) { return call_set(rt, ctx, 0x08955300u, {car, index}); };
    const std::uint32_t course = word(rt, car);
    const std::uint32_t constants = word(rt, car + 8u);
    const std::uint32_t state = word(rt, car + 0xCu);
    const auto tyre_of = [&](std::uint32_t wheel, std::uint32_t index) {
        return call_set(rt, ctx, 0x0894268Cu, {state, index, byte_u(rt, wheel + 0xE9u)});
    };
    if (call_set(rt, ctx, 0x08944588u, {car}) == 0u) {
        for (std::uint32_t index = 0u; index < 4u; ++index) {
            const std::uint32_t wheel = wheel_of(index);
            const float factor = f(wheel + 0x84u);
            const float value = psp_mul(factor, call_setf(rt, ctx, 0x08944768u, {car, index >> 1}, {1.0f}).f0);
            put_float(rt, wheel + 0x84u, value);
            if (1.0f < value) put_float(rt, wheel + 0x84u, 1.0f);
        }
        return;
    }
    if (call_set(rt, ctx, 0x089557BCu, {car}) != 0u) return;
    float steer_axle[2] = {1.0f, 1.0f};
    float grip_axle[2] = {1.0f, 1.0f};
    float cut[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float lock[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float brake[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float grip[4];
    for (std::uint32_t index = 0u; index < 4u; ++index) grip[index] = f(tyre_of(wheel_of(index), index) + 0x168u);
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        const std::uint32_t tyre = tyre_of(wheel, index);
        if (byte_u(rt, tyre + 0x250u) != 1u) continue;
        float target = 0.0f;
        std::uint32_t source = 2u;
        if (signed_of(sext8(byte_u(rt, constants + 3u))) == 2 || byte_u(rt, constants + 0xEu) != 0u) {
            source = 1u;
            target = psp_mul(call_setf(rt, ctx, 0x08946B38u, {tyre + 0x1ACu}, {f(wheel + 0x6Cu)}).f0, f(course + 0x3B4u));
        }
        if (!(target < grip[index])) continue;
        float blend = f(course + (index >> 1) * 4u + 0x3B8u);
        blend = blend + call_setf(rt, ctx, 0x089446C0u, {car}, {blend}).f0;
        if (source != 2u) {
            grip[index] = psp_mul(target, blend) + psp_mul(1.0f - blend, grip[index]);
            continue;
        }
        if (blend == 0.0f) continue;
        const std::uint32_t contact = call_set(rt, ctx, 0x089554A8u, {car, index});
        float speed = psp_mul(f(contact + 0x10u), f(wheel + 0x2Cu)) + psp_mul(f(contact + 0xCu), f(wheel + 0x30u));
        if (speed < 0.0f) speed = -speed;
        float amount = psp_mul(speed, std::bit_cast<float>(0x3D9374BCu));
        if (1.0f < amount) amount = 1.0f;
        float keep = 1.0f - psp_mul(amount, blend);
        if (1.0f < keep) {
            keep = 1.0f;
        } else if (keep < 0.0f) {
            keep = 0.0f;
        }
        grip[index] = psp_mul(keep, grip[index]);
    }
    if (call_set(rt, ctx, 0x0894D050u, {word(rt, car)}) != 0u && byte_u(rt, 0x08CDE639u) != 0u) {
        float steering = f(constants + 0x520u);
        if (steering < 0.0f) steering = -steering;
        const float full = f(course + 0x3C8u);
        const float share = full <= steering ? 1.0f : steering / full;
        for (std::uint32_t axle = 0u; axle < 2u; ++axle) {
            steer_axle[axle] = psp_mul(1.0f - psp_mul(share, f(course + 0x3C0u + axle * 4u)), steer_axle[axle]);
        }
    }
    const std::uint32_t settings = word(rt, car + 8u);
    const std::uint32_t axle_state = word(rt, car + 0xCu);
    for (std::uint32_t axle = 0u; axle < 2u; ++axle) {
        float value = f(axle_state + 0x13BCu + axle * 4u);
        if (byte_u(rt, 0x08CF2B28u) == 0u) value = call_setf(rt, ctx, 0x08944768u, {car, axle}, {value}).f0;
        grip_axle[axle] = psp_mul(value, grip_axle[axle]);
    }
    if (active != 0u) {
        bool assisted = true;
        bool armed = true;
        if (!(signed_of(sext8(byte_u(rt, settings + 3u))) == 2 || byte_u(rt, settings + 0xEu) != 0u)) {
            assisted = false;
            armed = signed_of(call_set(rt, ctx, 0x089445D8u, {car, 3u})) > 0 &&
                    signed_of(call_set(rt, ctx, 0x089444F8u, {car, 3u, 0u, 0u})) > 0;
        }
        const float drive_slip = f(settings + 0x558u);
        const float brake_slip = f(settings + 0x550u);
        const std::uint32_t lead = 0.0f < f(settings + 0x6F8u) ? 0u : 1u;
        const std::uint32_t other = 1u - lead;
        std::uint32_t engaged = 0u;
        if (f(settings + 0x538u) == 0.0f) {
            if (call_set(rt, ctx, 0x0895B668u, {car, frame.at(0u), frame.at(4u), frame.at(8u), frame.at(0xCu)}) != 0u) {
                const float first = float_at(rt, frame.at(0u));
                brake[lead] = brake[lead] + first;
                if (word(rt, frame.at(8u)) != 0u) brake[lead + 2u] = first + brake[lead + 2u];
                const float second = float_at(rt, frame.at(4u));
                brake[other] = brake[other] + second;
                if (word(rt, frame.at(0xCu)) != 0u) brake[other + 2u] = second + brake[other + 2u];
            }
            if (byte_u(rt, aids + 0x30u) != 0u && armed) {
                const float push = psp_mul(drive_slip - f(aids + 4u), f(aids));
                if (0.0f < push) {
                    if (byte_u(rt, aids + 0xCu) != 0u) brake[other] = push + brake[other];
                    engaged = 1u;
                    if (byte_u(rt, aids + 0xDu) != 0u) brake[other + 2u] = push + brake[other + 2u];
                }
            }
            if ((byte_u(rt, 0x08CDE70Cu) != 0u || f(settings + 0x530u) == 0.0f) && byte_u(rt, aids + 0x32u) != 0u && armed) {
                const float push = psp_mul(brake_slip - f(aids + 0x14u), f(aids + 0x10u));
                if (0.0f < push) {
                    if (byte_u(rt, 0x08CDE70Du) != 0u) engaged |= 2u;
                    if (byte_u(rt, aids + 0x1Cu) != 0u) brake[lead] = push + brake[lead];
                    if (byte_u(rt, aids + 0x1Du) != 0u) brake[lead + 2u] = push + brake[lead + 2u];
                }
            }
        } else {
            if (byte_u(rt, aids + 0x31u) != 0u && armed) {
                float keep = 1.0f - psp_mul(drive_slip - 0.1f, f(aids + 8u));
                bool apply = true;
                if (keep < 0.0f) {
                    keep = 0.0f;
                } else if (!(keep < 1.0f)) {
                    apply = false;
                }
                if (apply) {
                    if (byte_u(rt, aids + 0xEu) != 0u) {
                        cut[lead] = psp_mul(keep, cut[lead]);
                        lock[lead] = psp_mul(keep, lock[lead]);
                    }
                    engaged = 1u;
                    if (byte_u(rt, aids + 0xFu) != 0u) {
                        cut[lead + 2u] = psp_mul(cut[lead + 2u], keep);
                        lock[lead + 2u] = psp_mul(lock[lead + 2u], keep);
                    }
                }
            }
            if (byte_u(rt, aids + 0x33u) != 0u && armed) {
                float keep = 1.0f - psp_mul(brake_slip - 0.1f, f(aids + 0x18u));
                bool apply = true;
                if (keep < 0.0f) {
                    keep = 0.0f;
                } else if (!(keep < 1.0f)) {
                    apply = false;
                }
                if (apply) {
                    if (byte_u(rt, 0x08CDE70Du) != 0u && byte_u(rt, 0x08CF3650u) == 0u) engaged |= 2u;
                    if (byte_u(rt, aids + 0x1Eu) != 0u) {
                        cut[other] = psp_mul(cut[other], keep);
                        lock[other] = psp_mul(lock[other], keep);
                    }
                    if (byte_u(rt, aids + 0x1Fu) != 0u) {
                        cut[other + 2u] = psp_mul(cut[other + 2u], keep);
                        lock[other + 2u] = psp_mul(lock[other + 2u], keep);
                    }
                }
            }
        }
        if ((engaged & 2u) != 0u && byte_u(rt, 0x08CDE68Cu) != 0u && !assisted) {
            float keep = 1.0f - psp_mul(brake_slip, f(0x08CF2F90u));
            if (keep < 0.0f) keep = 0.0f;
            put_float(rt, settings + 0x534u, psp_mul(keep, f(settings + 0x534u)));
        }
        if (engaged != 0u) {
            put8(rt, settings + 0x730u, 2u);
        } else if (call_set(rt, ctx, 0x0895FF70u, {aids}) != 0u && armed) {
            put8(rt, settings + 0x730u, 1u);
        }
    }
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        const std::uint32_t axle = index >> 1;
        const float factor = psp_mul(steer_axle[axle], f(wheel + 0x84u));
        put_float(rt, wheel + 0x84u, factor);
        grip[index] = psp_mul(psp_mul(grip_axle[axle], grip[index]), cut[index]);
        const float value = psp_mul(lock[index], f(wheel + 0x84u)) + brake[index];
        put_float(rt, wheel + 0x84u, value);
        if (1.0f < value) put_float(rt, wheel + 0x84u, 1.0f);
    }
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t wheel = wheel_of(index);
        const std::uint32_t axle_base = state + (index >> 1) * 4u;
        float curve = f(axle_base + 0x13B4u);
        float limit = psp_mul(f(wheel + 0x58u), f(axle_base + 0x13C4u));
        const float factor = f(wheel + 0x84u);
        if (byte_u(rt, 0x08CF2B28u) == 1u) limit = call_setf(rt, ctx, 0x08944768u, {car, index >> 1}, {limit}).f0;
        if (factor == 0.0f) continue;
        const float scale = psp_mul(curve, f(wheel + 0x38u));
        float slip = f(wheel + 0x3Cu);
        const std::uint32_t tyre = tyre_of(wheel, index);
        if (byte_u(rt, tyre + 0x250u) != 0u) {
            const float slide = psp_mul(f(wheel + 0x40u), f(state + 0x1348u));
            slip = slip < 0.0f ? -slide : slide;
        }
        float value = psp_mul(limit, 1.0f - psp_mul(scale, slip - grip[index]));
        if (value < 0.0f) {
            value = 0.0f;
        } else if (1.0f < value) {
            value = 1.0f;
        }
        const float result = psp_mul(value, factor);
        put_float(rt, wheel + 0x84u, result == 0.0f ? std::bit_cast<float>(0x3A83126Fu) : result);
    }
    ctx.gpr[2] = 4u;
}


// func_08AADB10(selector) -> the selected call's result (0 for a selector of 67 or more): a dispatcher through the
// jump table at 0x08C8FEA8. Most entries call one function with constant arguments (0, 3, the objects 0x08CF6FDC /
// 0x08CF6FD0, or nothing, when the callee sees a0 still holding the entry's address); the others build a temporary on
// the stack and destroy it afterwards: a copy of 0x08CF6FDC (func_08A75BDC / func_08A75A88), a value from 0x08CF6ED8
// (func_08A90470 / func_08A5B9EC) or a string from the literal 0x08C8FEA4 (func_08B85EB8, released like
// release_string).
void func_08AADB10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t selector = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    if (selector >= 0x43u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[4] = word(rt, 0x08C8FEA8u + selector * 4u);
    constexpr std::uint32_t kState = 0x08CF6FDCu;
    constexpr std::uint32_t kOther = 0x08CF6FD0u;
    const auto call = [&](std::uint32_t function, std::initializer_list<std::uint32_t> arguments) {
        return call_set(rt, ctx, function, arguments);
    };
    const auto with_copy = [&](std::uint32_t offset, std::uint32_t function, bool state_first, bool state_after) {
        const std::uint32_t object = frame.at(offset);
        call(0x08A75BDCu, {object, kState});
        std::uint32_t result;
        if (state_first) {
            result = call(function, {kState, object});
        } else if (state_after) {
            result = call(function, {object, kState, kState});
        } else {
            result = call(function, {object});
        }
        call(0x08A75A88u, {object});
        ctx.gpr[2] = result;
    };
    const auto with_value = [&](std::uint32_t offset, std::uint32_t function, bool state_first) {
        const std::uint32_t object = frame.at(offset);
        call(0x08A90470u, {object, 0x08CF6ED8u});
        const std::uint32_t result = state_first ? call(function, {kState, object}) : call(function, {object});
        call(0x08A5B9ECu, {object});
        ctx.gpr[2] = result;
    };
    const auto with_text = [&](std::uint32_t offset, std::uint32_t function, std::uint32_t scratch, std::uint32_t release_scratch) {
        const std::uint32_t object = frame.at(offset);
        call(0x08B85EB8u, {object, 0x08C8FEA4u, frame.at(scratch)});
        const std::uint32_t result = call(function, {object});
        release_string(rt, ctx, object, 0x08D74FD0u, 0x08B8542Cu, frame.at(release_scratch));
        ctx.gpr[2] = result;
    };
    switch (selector) {
    case 0: call(0x08A64CC8u, {0u}); break;
    case 1: call(0x08A64D88u, {}); break;
    case 2: call(0x08A64B7Cu, {kState}); break;
    case 3: with_copy(0x70u, 0x08A64E8Cu, false, false); break;
    case 4: call(0x08A64FF0u, {kOther}); break;
    case 5: call(0x08A65154u, {kOther}); break;
    case 6: call(0x08A652B0u, {0u}); break;
    case 7: with_copy(0x64u, 0x08A653F0u, true, false); break;
    case 8: call(0x08A65584u, {}); break;
    case 9: call_setf(rt, ctx, 0x08A65680u, {}, {0.0f}); break;
    case 10: with_value(0x18u, 0x08A64A4Cu, true); break;
    case 11: with_copy(0x58u, 0x08A657E0u, false, true); break;
    case 12: call(0x08A6598Cu, {0u}); break;
    case 13: call(0x08A65A54u, {0u}); break;
    case 14: call(0x08A65B1Cu, {0u}); break;
    case 15: call(0x08A65BE4u, {0u}); break;
    case 16: call(0x08A65CACu, {0u}); break;
    case 17: call(0x08A648D0u, {kState}); break;
    case 18: call(0x08A65D74u, {0u}); break;
    case 19: call(0x08A65E3Cu, {0u}); break;
    case 20: with_value(0x14u, 0x08A647A0u, true); break;
    case 21: with_copy(0x4Cu, 0x08A65F0Cu, false, false); break;
    case 22: call(0x08A66060u, {}); break;
    case 23: call(0x08A66110u, {}); break;
    case 24: call(0x08A66204u, {}); break;
    case 25: call(0x08A66300u, {0u}); break;
    case 26: call(0x08A663C0u, {}); break;
    case 27: call(0x08A66478u, {3u}); break;
    case 28: call(0x08A64624u, {kState}); break;
    case 29: with_text(0x10u, 0x08A66548u, 3u, 1u); break;
    case 30: call(0x08A66750u, {0u}); break;
    case 31: call(0x08A66810u, {}); break;
    case 32: call(0x08A668C8u, {0u}); break;
    case 33: call(0x08A66998u, {kOther}); break;
    case 34: call(0x08A66AFCu, {kOther}); break;
    case 35: with_copy(0x40u, 0x08A66C60u, false, false); break;
    case 36: with_copy(0x34u, 0x08A66E04u, false, false); break;
    case 37: with_copy(0x28u, 0x08A66F68u, false, false); break;
    case 38: with_copy(0x1Cu, 0x08A6710Cu, false, false); break;
    case 39: with_text(0xCu, 0x08A67270u, 2u, 0u); break;
    case 40: with_value(8u, 0x08A67480u, false); break;
    case 41: with_value(4u, 0x08A675E4u, false); break;
    case 42: call(0x08A67740u, {0u}); break;
    case 43: call(0x08A67810u, {0u, 0u}); break;
    case 44: call(0x08A678E0u, {}); break;
    case 45: call(0x08A679E4u, {0u, 0u}); break;
    case 46: call(0x08A67ABCu, {0u}); break;
    case 47: call(0x08A67B7Cu, {}); break;
    case 48: call(0x08A67C80u, {kState}); break;
    case 49: call(0x08A67DE4u, {0u, 0u}); break;
    case 50: call(0x08A67EBCu, {0u}); break;
    case 51: call(0x08A67F7Cu, {}); break;
    case 52: call(0x08A68070u, {}); break;
    case 53: call(0x08A68120u, {}); break;
    case 54: call(0x08A68214u, {}); break;
    case 55: call(0x08A68310u, {3u}); break;
    case 56: call(0x08A683D0u, {}); break;
    case 57: call(0x08A68480u, {}); break;
    case 58: call(0x08A6857Cu, {0u}); break;
    case 59: call(0x08A6864Cu, {0u, 0u}); break;
    case 60: call(0x08A6872Cu, {0u, 0u}); break;
    case 61: call(0x08A687FCu, {}); break;
    case 62: call(0x08A688F0u, {}); break;
    case 63: call(0x08A689ECu, {0u}); break;
    case 64: call(0x08A68AB4u, {0u}); break;
    case 65: call(0x08A68B7Cu, {0u}); break;
    default: call(0x08A68C3Cu, {}); break;
    }
}

} // namespace hot_batch_122
} // namespace gt::game

#define HOT122(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_122::name, gt::ReturnKind::kind)
HOT122(0x08903B30u, func_08903B30, Word);
HOT122(0x0893D9ACu, func_0893D9AC, Word);
HOT122(0x08957C3Cu, func_08957C3C, Word);
HOT122(0x08945664u, func_08945664, Word);
HOT122(0x08946EC0u, func_08946EC0, Word);
HOT122(0x0893E0FCu, func_0893E0FC, Word);
HOT122(0x0895FFBCu, func_0895FFBC, Word);
HOT122(0x08AADB10u, func_08AADB10, Word);
