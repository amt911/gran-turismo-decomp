// New batch (main-session batch 1036): three long functions an earlier draft left unverified (two of them skipped twice as
// too hard), written as structured C from `tools.disasm`.
//  0x08AFE3F8 — three 3x3 products through the math library; the first one goes through a temporary when its result would
//               overwrite one of its operands.
//  0x088C0738 — the start-light / countdown state machine of a driving task: the bearing of the target from the car, how
//               many wheels count as raised, the phase bytes of the state record, and the light handed through the input
//               object's virtual +0x24 into 0x088BF978.
//  0x089DCE68 — rebuilds one of two vectors of 16-byte elements from a holder object's children (the +0x7C count decides
//               the size, the +0x54 floats of the children fill the elements).
// Delay slots: plain branches run their delay slot on both paths, the branch-likely ones (beql/bnel/bc1tl/bc1fl) only when
// taken; the two "stale value" reads that this creates are modelled where they happen (088C0738 +0xA54 and +0xB08).
// v0 is what the original leaves on every exit path (the last call's result, or the -1 the state machine returns).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1036 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call that leaves the callee's v0 / v1 / f0 in the context like the original's jal does.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
         std::initializer_list<float> floats = {}) {
    return call_full(rt, ctx, target, arguments, floats).v0;
}

// `lw v1, 0(object); lw v0, slot(v1); jalr v0`: v0 starts as the target, afterwards it is the callee's.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    return call_indirect(rt, ctx, word(rt, word(rt, object) + slot), arguments);
}

// `((a0 * b0 + 0.0f) + a1 * b1) + a2 * b2`, the order the original's unrolled loops use.
float dot3(float a0, float b0, float a1, float b1, float a2, float b2) {
    float sum = psp_mul(a0, b0);
    const float second = psp_mul(a1, b1);
    sum += 0.0f;
    const float third = psp_mul(a2, b2);
    sum += second;
    sum += third;
    return sum;
}

// --- 0x08AFE3F8 ---
// (a0 = object, a1 = output 3x3, a2 = a source). The math library returns two matrices (+0 and +4 of one record); their
// product (left columns 4 apart, right rows 0x10 apart) is built with the asm's three-term dot, then 0x08C467DC fills the
// second operand at sp+0xC0 from word(word(sp+0x74)) and the product of that with the first one is written to a1.
void func_08AFE3F8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x150u);
    const u32 object = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 product_target = frame.at(0x9Cu);

    const u32 source = call(rt, ctx, 0x08C45EC0u, {ctx.gpr[6]});
    const u32 first = word(rt, source);
    const u32 second = word(rt, source + 4u);
    const u32 third = word(rt, source + 8u);
    const u32 fourth = word(rt, source + 0xCu);
    put(rt, product_target, first);
    put(rt, product_target + 4u, second);
    put(rt, product_target + 8u, third);
    put(rt, product_target + 0xCu, fourth);
    call(rt, ctx, 0x08C460E4u, {frame.at(0x78u), product_target, second, third, fourth});

    const u32 scene = call(rt, ctx, 0x08C45D74u, {word(rt, object + 0xD8u) + 0x30u});
    const GuestResult pair = call_full(rt, ctx, 0x08C45FA4u, {frame.at(0x78u), scene});
    put(rt, frame.at(0x68u), pair.v0);
    put(rt, frame.at(0x6Cu), pair.v1);
    put(rt, frame.at(0u), call(rt, ctx, 0x08C45F9Cu, {frame.at(0x78u)}));
    const GuestResult operands = call_full(rt, ctx, 0x08C45DACu, {frame.at(0x68u), frame.at(0u)});
    put(rt, frame.at(0x70u), operands.v0);
    put(rt, frame.at(0x74u), operands.v1);
    const u32 left = word(rt, operands.v0);
    const u32 right = word(rt, operands.v0 + 4u);

    // The product goes to a temporary when its target would overwrite one of its operands (sp+0x9C compared with both).
    const bool aliased = product_target == left || product_target == right;
    const u32 product = aliased ? frame.at(0xE4u) : product_target;
    for (u32 row = 0; row < 3u; ++row) {
        for (u32 column = 0; column < 3u; ++column) {
            const u32 a = left + row * 4u;
            const u32 b = right + column * 0x10u;
            put_float(rt, product + row * 4u + column * 0xCu,
                      dot3(float_at(rt, a), float_at(rt, b), float_at(rt, a + 0xCu), float_at(rt, b + 4u),
                           float_at(rt, a + 0x18u), float_at(rt, b + 8u)));
        }
    }
    if (aliased) {
        for (u32 index = 0; index < 9u; ++index) put(rt, product_target + index * 4u, word(rt, product + index * 4u));
    }

    ctx.gpr[2] = frame.at(0x70u);
    ctx.gpr[3] = word(rt, frame.at(0x74u));
    call(rt, ctx, 0x08C467DCu, {frame.at(0xC0u), word(rt, word(rt, frame.at(0x74u)))});

    // out = the first product times the matrix just built at sp+0xC0 (both packed: rows 0xC apart, columns 4).
    float result[9];
    for (u32 row = 0; row < 3u; ++row) {
        for (u32 column = 0; column < 3u; ++column) {
            const u32 a = product_target + row * 4u;
            const u32 b = frame.at(0xC0u) + column * 0xCu;
            result[row + column * 3u] = dot3(float_at(rt, a), float_at(rt, b), float_at(rt, a + 0xCu), float_at(rt, b + 4u),
                                             float_at(rt, a + 0x18u), float_at(rt, b + 8u));
        }
    }
    for (u32 row = 0; row < 3u; ++row) {
        for (u32 column = 0; column < 3u; ++column) {
            put_float(rt, out + row * 4u + column * 0xCu, result[row + column * 3u]);
        }
    }
}

// --- 0x088C0738 ---
// (a0 = self, a1 = target: {x, y, "passed" word at +0x18}). Word(self+8) is the car (byte +3 the mode, byte +0xE, the
// four wheels at +0x15D + 0x118*i, floats +0x10/+0x14, +0x1C/+0x20 and +0x6C4), word(self+0x24) the state record (the
// countdown bytes +0x6D/+0x6E/+0x6F, the phase bytes +0xF/+0x10, the byte +0x11, the float +0x44 and the word +8),
// word(self+0x1C) the input object, word(self+0x2C) the parameters and word(self+0x18) the gate. v0 = 0 / 4 on the two
// early exits, -1 when the state machine ends without a light, otherwise 0x088BF978's answer.
void func_088C0738(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    const u32 self = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    const u32 car = word(rt, self + 8u);
    const u32 state = word(rt, self + 0x24u);
    const float kSmallBearing = std::bit_cast<float>(0x3EB2B8C2u);  // 0.34906584
    const float kHalfBearing = std::bit_cast<float>(0x3F490FDBu);   // 0.7853982
    const float kTwoPower31 = std::bit_cast<float>(0x4F000000u);    // 2147483648.0

    // Byte +3 of the car is the mode; a mode other than 2 with byte +0xE clear only resets the light counters.
    if (static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(car + 3u))) != 2 &&
        rt.memory().load8(car + 0xEu) == 0u) {
        rt.memory().store8(state + 0x6Fu, 0u);
        rt.memory().store8(state + 0x6Du, 0u);
        rt.memory().store8(state + 0x6Eu, 0u);
        ctx.gpr[2] = 0u;
        return;
    }
    // Phases 3 and 4 leave the light at 4 and reset the countdown.
    const u32 mode = rt.memory().load8(state + 0x10u);
    if (mode == 3u || mode == 4u) {
        rt.memory().store8(state + 0x6Fu, 0u);
        rt.memory().store8(state + 0x6Du, 0u);
        rt.memory().store8(state + 0x6Eu, 0u);
        ctx.gpr[2] = 4u;
        return;
    }

    // 0xCF0: the bearing of the target from the car, then how many of the four wheels count as raised.
    const float turn = call_full(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, target), float_at(rt, target + 4u)}).f0;
    put_float(rt, frame.at(8u), turn);
    const float bearing = call_full(rt, ctx, 0x0893C60Cu, {frame.at(8u), word(rt, self + 0x2Cu) + 0x10u}).f0;
    const float bearing_abs = std::fabs(bearing);
    u32 grounded = 4u;
    {
        u32 wheel = car;
        u32 raised = rt.memory().load8(wheel + 0x15Du);
        u32 remaining = 4u;
        for (;;) {
            --remaining;
            wheel += 0x118u;
            grounded -= raised != 0u ? 1u : 0u;
            if (remaining == 0u) break;
            raised = rt.memory().load8(wheel + 0x15Du);
        }
    }

    // The common tail at 0xAE4: one tick off +0x6F; from a counter of 60 on, the light is kept alive (0xB08: the car's
    // speed at +0x6C4, the spans of the parameter object word(self), two bearings and the three 0x08B4/B49 helpers set
    // the byte +0x6F); then the light through the input object's virtual +0x24 into 0x088BF978(handler, flag, self), an
    // absent light (-1) alone returning -1. The counter below 60 does not skip the hand-over: only the light does.
    const auto finish = [&](u32 tail_light, u32 tail_flag, u32 tail_counter) {
        const u32 tick = rt.memory().load8(state + 0x6Fu);
        if (tick != 0u) rt.memory().store8(state + 0x6Fu, static_cast<std::uint8_t>(tick - 1u));
        if ((tail_counter & 0xFFu) >= 0x3Cu) {
            const u32 scene_now = call(rt, ctx, 0x088B1A0Cu, {word(rt, self)});
            if (!((scene_now - 2u) < 2u) && 0.0f < float_at(rt, state + 0x44u)) {
                const u32 input = word(rt, self + 0x1Cu);
                if (call_indirect(rt, ctx, word(rt, word(rt, input) + 0x18u), {input, 6u}) != 0u) {
                    const u32 block = frame.at(0x10u);
                    put(rt, block, 5u);
                    put(rt, block + 4u, 0u);
                    put(rt, block + 8u, 6u);
                    put(rt, block + 0xCu, rt.memory().load8(state + 0x11u));
                    const u32 element = call(rt, ctx, 0x088DF13Cu, {word(rt, self + 0xCu), word(rt, self + 0x10u)});
                    put_float(rt, block + 0x10u, float_at(rt, element));
                    put_float(rt, block + 0x14u, 0.0f);
                    put_float(rt, frame.at(0x38u), 0.0f);
                    put_float(rt, frame.at(0x3Cu), 0.0f);
                    put_float(rt, frame.at(0x40u), 0.0f);
                    put_float(rt, frame.at(0x44u), 1.0f);
                    put_float(rt, frame.at(0x48u), 0.0f);
                    put_float(rt, frame.at(0x4Cu), 0.0f);
                    const u32 outer = call_indirect(rt, ctx, word(rt, word(rt, input) + 0x1Cu), {input});
                    call_indirect(rt, ctx, word(rt, word(rt, outer) + 8u), {outer, input, block});
                    const u32 car_now = word(rt, self + 8u);
                    const float offset_y = float_at(rt, frame.at(0x38u)) - float_at(rt, car_now + 0x10u);
                    const float offset_z = float_at(rt, frame.at(0x3Cu)) - float_at(rt, car_now + 0x14u);
                    const float distance = psp_mul(offset_y, offset_y) + psp_mul(offset_z, offset_z);
                    const float first = call_full(rt, ctx, 0x08B4A430u, {},
                                                  {-float_at(rt, frame.at(0x44u)), float_at(rt, frame.at(0x48u))})
                                             .f0;
                    const u32 car_second = word(rt, self + 8u);
                    put_float(rt, frame.at(4u), first);
                    const float second = call_full(rt, ctx, 0x08B4A430u, {},
                                                   {-float_at(rt, car_second + 0x1Cu), float_at(rt, car_second + 0x20u)})
                                             .f0;
                    put_float(rt, frame.at(0u), second);
                    const float difference = call_full(rt, ctx, 0x0893C60Cu, {frame.at(4u), frame.at(0u)}).f0;
                    const u32 base = word(rt, self);
                    float angle = float_at(rt, base + 0x78u);
                    if (angle <= 0.0f) angle = kHalfBearing;
                    const float span = float_at(rt, base + 0x18u);
                    const float covered = call_full(rt, ctx, 0x08B49CC8u, {}, {angle}).f0;
                    const float share = span / covered;
                    const float shaped = call_full(rt, ctx, 0x08B499B8u, {}, {difference}).f0;
                    const u32 car_last = word(rt, self + 8u);
                    const float remaining = 1.0f - shaped;
                    const float speed = float_at(rt, car_last + 0x6C4u);
                    const float squared = psp_mul(speed, speed);
                    const float margin = psp_mul(squared, 0.08f);
                    const float reached = share * remaining + 2.0f;
                    const float limit = reached + margin;
                    if (distance <= 2.0f) {
                        rt.memory().store8(state + 0x6Fu, 0u);
                    } else if (distance <= psp_mul(limit, limit)) {
                        float ticks = psp_mul(squared, 0.005f);
                        ticks += 1.0f;
                        ticks = psp_mul(ticks, 60.0f);
                        u32 count = ctx.fpu_float_to_word(ticks, 1u);
                        if (kTwoPower31 <= ticks) {
                            count = ctx.fpu_float_to_word(ticks - kTwoPower31, 1u) | 0x80000000u;
                        }
                        rt.memory().store8(state + 0x6Fu, static_cast<std::uint8_t>(count));
                    }
                }
            }
        }
        ctx.gpr[2] = 0xFFFFFFFFu;
        if (tail_light == 0xFFFFFFFFu) return;
        const u32 input = word(rt, self + 0x1Cu);
        const u32 handler = call_indirect(rt, ctx, word(rt, word(rt, input) + 0x24u), {input, tail_light});
        ctx.gpr[2] = call(rt, ctx, 0x088BF978u, {handler, tail_flag, self});
    };

    // 0x7DC: scenes 2 and 3 reset the countdown and end with the light absent.
    const u32 scene = call(rt, ctx, 0x088B1A0Cu, {word(rt, self)});
    if ((scene - 2u) < 2u) {
        rt.memory().store8(state + 0x6Eu, 0u);
        rt.memory().store8(state + 0x6Du, 0u);
        finish(0xFFFFFFFFu, 0u, 0u);
        return;
    }
    // 0xA54: the countdown is running; the seconds at +0x6E advance (past 59 they wrap to 180) and the light follows the
    // grounding, the bearing and the phase byte +0xF.
    if (rt.memory().load8(state + 0x6Du) != 0u) {
        const u32 current = rt.memory().load8(state + 0x6Eu);
        u32 counter = current;
        if (current != 0xFFu) {
            const u32 next = (current + 1u) & 0xFFu;
            counter = next;
            if (next < 0x3Cu) {
                rt.memory().store8(state + 0x6Eu, static_cast<std::uint8_t>(next));
            } else {
                // addiu a0, zero, -0x4C: the count wraps to 180 (the byte store keeps its low byte).
                counter = 0xFFFFFFB4u;
                rt.memory().store8(state + 0x6Eu, static_cast<std::uint8_t>(counter));
            }
        }
        u32 light = 0xFFFFFFFFu;
        if (grounded == 4u && !(kSmallBearing <= bearing_abs)) {
            if (rt.memory().load8(state + 0xFu) != 1u) {
                rt.memory().store8(state + 0x6Du, 0u);
                const u32 next_mode = rt.memory().load8(state + 0x10u);
                light = next_mode == 9u ? 2u : next_mode == 8u ? 1u : 6u;
            }
        }
        finish(light, 0u, counter);
        return;
    }
    // 0x814: no countdown yet; the light stays off unless the car is far from the target with a large bearing.
    rt.memory().store8(state + 0x6Eu, 0u);
    if (grounded != 0u) {
        if (static_cast<s32>(word(rt, state + 8u)) < 0x3A98) {
            finish(0xFFFFFFFFu, 0u, 0u);
            return;
        }
        if (bearing_abs <= kSmallBearing) {
            finish(0xFFFFFFFFu, 0u, 0u);
            return;
        }
        const float x = float_at(rt, target);
        const float y = float_at(rt, target + 4u);
        if (!(400.0f < psp_mul(x, x) + psp_mul(y, y))) {
            finish(0xFFFFFFFFu, 0u, 0u);
            return;
        }
    }
    // 0x878: the input object answers both of its virtual +0x18; the destination cells the two virtual +0x8 fill decide
    // `inside`, and a near enough pass at the gate (word(self+0x18), 1.0f at +8) marks the target as passed.
    u32 inside = bearing < 0.0f ? 1u : 0u;
    {
        const u32 input = word(rt, self + 0x1Cu);
        if (call_indirect(rt, ctx, word(rt, word(rt, input) + 0x18u), {input, 9u}) != 0u &&
            call_indirect(rt, ctx, word(rt, word(rt, input) + 0x18u), {input, 8u}) != 0u) {
            const u32 block = frame.at(0x10u);
            put(rt, block, 1u);
            put(rt, block + 4u, 0u);
            put(rt, block + 8u, 9u);
            put(rt, block + 0xCu, rt.memory().load8(state + 0x11u));
            const u32 element = call(rt, ctx, 0x088DF13Cu, {word(rt, self + 0xCu), word(rt, self + 0x10u)});
            put_float(rt, block + 0x10u, float_at(rt, element));
            put_float(rt, block + 0x14u, 0.0f);
            put_float(rt, frame.at(0x38u), 0.0f);
            put_float(rt, frame.at(0x3Cu), 0.0f);
            put_float(rt, frame.at(0x40u), 0.0f);
            const u32 outer = call_indirect(rt, ctx, word(rt, word(rt, input) + 0x1Cu), {input});
            call_indirect(rt, ctx, word(rt, word(rt, outer) + 8u), {outer, input, block});
            const u32 car_near = word(rt, self + 8u);
            const float near_y = float_at(rt, frame.at(0x38u)) - float_at(rt, car_near + 0x10u);
            const float near_z = float_at(rt, frame.at(0x3Cu)) - float_at(rt, car_near + 0x14u);
            put(rt, block + 8u, 8u);
            const u32 outer_second = call_indirect(rt, ctx, word(rt, word(rt, input) + 0x1Cu), {input});
            call_indirect(rt, ctx, word(rt, word(rt, outer_second) + 8u), {outer_second, input, block});
            const u32 car_far = word(rt, self + 8u);
            const float far_z = float_at(rt, frame.at(0x3Cu)) - float_at(rt, car_far + 0x14u);
            const float far_y = float_at(rt, frame.at(0x38u)) - float_at(rt, car_far + 0x10u);
            const float near = psp_mul(near_y, near_y) + psp_mul(near_z, near_z);
            const float far = psp_mul(far_y, far_y) + psp_mul(far_z, far_z);
            inside = near < far ? 1u : 0u;
            const u32 gate = word(rt, self + 0x18u);
            if (1.0f < float_at(rt, gate + 8u)) {
                if (0.0f < psp_mul(near_y, far_y) + psp_mul(near_z, far_z)) put(rt, target + 0x18u, 1u);
            }
        }
    }
    // 0xA04: the light only comes on past 45 degrees of bearing; the phase 1/2 then picks 9 or 8.
    if (!(kHalfBearing < bearing_abs)) {
        finish(0xFFFFFFFFu, 0u, rt.memory().load8(state + 0x6Eu));
        return;
    }
    const u32 marker = inside != 0u ? 1u : 2u;
    rt.memory().store8(state + 0x6Du, static_cast<std::uint8_t>(marker));
    finish(marker == 1u ? 9u : 8u, 1u, rt.memory().load8(state + 0x6Eu));
}

// --- 0x089DCE68 ---
// The message the null checks pass to 0x08A5BB7C (an empty function in this build: the check only survives as the
// argument load).
constexpr u32 kNullMessage = 0x08CA74F8u;

// The +0x54 float of child `index` of `owner` (built through the vtable's +0x84 into the frame slot at `slot_offset`).
// The float is stored before the child is released (the store sits in the release call's delay slot).
void child_float(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 owner, u32 slot_offset, u32 index,
                 u32 destination) {
    const u32 slot = frame.at(slot_offset);
    call_virtual(rt, ctx, owner, 0x84u, {slot, owner, index});
    const u32 child = word(rt, slot);
    if (child == 0u) call(rt, ctx, 0x08A5BB7Cu, {kNullMessage});
    const GuestResult value = call_indirect(rt, ctx, word(rt, word(rt, child) + 0x54u), {child}, {});
    put_float(rt, frame.at(destination), value.f0);
    call(rt, ctx, 0x08A5B9ECu, {slot});
}

// std::vector<16-byte element>::resize through the two helpers the original calls: erase the tail past `count` when the
// vector is longer (0x08C06570(vector, begin + count*16, end)), otherwise insert `count - size` copies of the 16 bytes at
// `fill` at the end (0x08C0729C(vector, end, n, fill)).
void vector_resize(Runtime &rt, AllegrexContext &ctx, u32 vector, u32 count, u32 fill) {
    const u32 begin = word(rt, vector + 0xCu);
    const u32 end = word(rt, vector + 0x10u);
    const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 4);
    if (count < size) call(rt, ctx, 0x08C06570u, {vector, begin + (count << 4), end});
    else call(rt, ctx, 0x08C0729Cu, {vector, end, count - size, fill});
}

// The vector teardown: read the three fields, zero them, hand the old storage back (the element loop 0x08C06684 and the
// heap free 0x08B35254, only when the storage existed).
void vector_destroy(Runtime &rt, AllegrexContext &ctx, u32 vector) {
    const u32 begin = word(rt, vector + 0xCu);
    const u32 end = word(rt, vector + 0x10u);
    put(rt, vector + 0xCu, 0u);
    put(rt, vector + 0x10u, 0u);
    put(rt, vector + 0x14u, 0u);
    ctx.gpr[2] = vector;  // the asm's v0 is still the vector at this call
    call(rt, ctx, 0x08C06684u, {begin, end});
    if (begin != 0u) call(rt, ctx, 0x08B35254u, {begin});
}

// (a0 = sequence object, a1 = pointer to its holder). The object owns two vectors of 16-byte values at +0x11C and
// +0x128; bit 9 of (a0+0x140 ^ 0x200) selects one. The holder's vtable +0x7C gives the element count and the +0x78 slot
// of element 0 decides how the values are built:
//  - 0: the vector is destroyed;
//  - +0x78 count 0: one element holding the +0x54 float of up to four children (+0x84 virtual), the rest 1.0f;
//  - otherwise: `count` elements, each holding three (child kind 3) or four (kind 4) +0x54 floats of its children, the
//    missing one 1.0f. A kind that is neither keeps the previous values.

} // namespace
} // namespace new_batch_main_1036
} // namespace gt::game

#define NEW1036(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1036::name, gt::ReturnKind::kind)
NEW1036(0x08AFE3F8u, func_08AFE3F8, Word);
NEW1036(0x088C0738u, func_088C0738, Word);
