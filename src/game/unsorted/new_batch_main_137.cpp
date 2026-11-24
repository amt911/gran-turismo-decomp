// Main-session batch 137 (draft, unverified): six large float-heavy functions written as structured C from `tools.disasm`.
// No names: none has evidence. Floats are single precision as on the Allegrex FPU (psp_mul for the multiplies).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_137 {
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
// A call with float arguments in f12..: v0, v1 and f0 are the callee's afterwards.
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `trunc.w.s`: round toward zero, saturating (NaN and positive overflow give 0x7FFFFFFF).
s32 trunc_w_s(float value) {
    if (value != value) return 0x7FFFFFFF;
    if (value >= 2147483648.0f) return 0x7FFFFFFF;
    if (value <= -2147483648.0f) return static_cast<s32>(0x80000000u);
    return static_cast<s32>(value);
}

// --- 0x08810FAC (a0 = object, a1 = destination, a2, a3, t0, t1): draws a seven-digit counter and two small numbers into `destination`
// through 0x089DB078 / 0x089DAEA0 (glyph rows of the table at *(object + 0x224)); ends with a tail call to 0x089DB5CC.
// The table's float at +8 is a row count n; row i starts at table + ((i - (n - 1)) << 4) + 0x300.
u32 row_address(Runtime &rt, u32 table, s32 index) {
    const s32 count = trunc_w_s(float_at(rt, table + 8u));
    return table + (static_cast<u32>(index - (count - 1)) << 4) + 0x300u;
}
void glyph(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 slot, u32 row, float f14, float f15) {
    callf(rt, ctx, 0x089DB078u, {destination, slot}, {float_at(rt, row), float_at(rt, row + 4u), f14, f15});
}



float float_from(u32 bits) { return std::bit_cast<float>(bits); }
// The FPU's divide: 0/0 and inf/inf are the PSP's default NaN (0x7FC00000).
float psp_div(float a, float b) {
    if ((a == 0.0f && b == 0.0f) || (std::isinf(a) && std::isinf(b))) return float_from(0x7FC00000u);
    return a / b;
}
float field(Runtime &rt, u32 base, u32 offset) { return float_at(rt, base + offset); }
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }

// --- 0x0894E9C0 (a0 = scene object; *(a0 + 8) a table of floats, *(a0 + 0xC) the per-lane arrays): the per-frame update of the four lane items
// (object + 08955300 / 089554A8): a first pass stores the lane timing, a second moves each item towards a target with a speed and
// clamps it, a third (when the byte at 0x08CDE700 is set) integrates a second set of values, then each lane's pose is
// rebuilt (positions through 0x08B498AC, an angle through 0x0893C60C) and the last pass stores a vector length (0x08B49D7C).
// v0 = the last 0x08B49D7C call's result.
void func_0894E9C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 settings = word(rt, object + 8u);
    const u32 lanes = word(rt, object + 0xCu);
    constexpr float kPi = std::bit_cast<float>(0x40490FDBu);
    GuestFrame frame(ctx, 0x70u);
    float lead = 0.0f;                  // f20
    u32 has_lead = 0u;                  // s5
    if (call(rt, ctx, 0x08958904u, {object}) != 0u) {
        call(rt, ctx, 0x08958974u, {object});
        lead = callf(rt, ctx, 0x08958940u, {object}, {}).f0;
        has_lead = 1u;
    }
    for (u32 lane = 0u; lane != 4u; ++lane) {
        const u32 item = call(rt, ctx, 0x08955300u, {object, lane});
        const float value = callf(rt, ctx, 0x0894E87Cu, {lanes, lane, has_lead}, {field(rt, settings, 0x520u), lead}).f0;
        put_float(rt, item + 0x10u, value);
    }
    call(rt, ctx, 0x08950314u, {object});
    if (call(rt, ctx, 0x0894E714u) != 0u) {
        const u32 kTable = 0x08CF2F0Cu;
        const float half = 0.5f;
        for (u32 lane = 0u; lane != 4u; ++lane) {
            const u32 item = call(rt, ctx, 0x08955300u, {object, lane});
            const float speed = callf(rt, ctx, 0x0895549Cu, {object}, {}).f0;
            const u32 kind = byte_at(rt, 0x08CDE0E8u + byte_at(rt, item + 0xE9u));
            const bool direct = (kind ^ 1u) < 1u;
            const u32 pair = static_cast<u32>(static_cast<s32>(lane) >> 1) << 2;
            const u32 mode = call(rt, ctx, 0x0894E714u);
            float target;
            float position;
            if (mode != 2u) {
                if (direct) {
                    target = 0.0f;
                    position = field(rt, item, 0x14u);
                } else if (field(rt, item, 0x70u) == 0.0f) {
                    float lead_value = field(rt, item, 0x10u);
                    const u32 other = call(rt, ctx, 0x089551B0u, {object});
                    lead_value = lead_value - float_at(rt, other + (lane << 2) + 0x58u);
                    const float limit = float_at(rt, pair + kTable + 8u);
                    position = field(rt, item, 0x14u);
                    lead_value = psp_mul(lead_value, half);
                    const float negative = -limit;
                    target = position - lead_value;
                    if (target < negative) target = negative;
                    else if (limit < target) target = limit;
                } else if (field(rt, item, 0x30u) < 0.0f) {
                    put_float(rt, frame.at(0xCu), field(rt, item, 0x28u) - kPi);
                    target = psp_mul(callf(rt, ctx, 0x0893C60Cu, {frame.at(0xCu), item + 0x10u}, {}).f0, half);
                    position = field(rt, item, 0x14u);
                } else {
                    position = field(rt, item, 0x14u);
                    target = psp_mul(field(rt, item, 0x28u) - field(rt, item, 0x10u), half);
                }
            } else {
                if (direct) {
                    target = field(rt, item, 0x10u);
                    position = field(rt, item, 0x14u);
                } else if (field(rt, item, 0x70u) == 0.0f) {
                    position = field(rt, item, 0x14u);
                    target = position;
                } else if (field(rt, item, 0x30u) < 0.0f) {
                    put_float(rt, frame.at(0x8u), half);
                    put_float(rt, frame.at(0x0u), half);
                    put_float(rt, frame.at(0x4u), field(rt, item, 0x28u) - kPi);
                    target = callf(rt, ctx, 0x0893C6A8u, {frame.at(0x8u), frame.at(0x4u), frame.at(0x0u), item + 0x10u}, {}).f0;
                    position = field(rt, item, 0x14u);
                } else {
                    const float angle = field(rt, item, 0x28u);
                    const float base = field(rt, item, 0x10u);
                    position = field(rt, item, 0x14u);
                    target = psp_mul(angle + base, half);
                }
            }
            float delta = target - position;
            const u32 entry = pair + kTable;
            const float rate = field(rt, item, 0x18u);
            float gain = psp_mul(field(rt, entry, 0x10u), delta);
            const float damping = psp_mul(field(rt, entry, 0x18u), rate);
            float limit = field(rt, entry, 8u);
            float scale = 1.0f;
            const u32 style = word(rt, kTable + 4u);
            delta = gain - damping;
            if (style != 0u) {
                const float twice = field(rt, item, 0x58u) + field(rt, item, 0x58u);
                scale = psp_div(twice, float_at(rt, lanes + pair + 0x13D8u));
            }
            float acceleration;
            if (style == 2u) {
                delta = psp_mul(delta, scale);
                acceleration = psp_mul(delta, speed);
            } else if (style == 3u) {
                delta = psp_mul(delta, scale);
                limit = psp_mul(limit, scale);
                acceleration = psp_mul(delta, speed);
            } else if (style == 1u) {
                acceleration = psp_mul(delta, speed);
                limit = psp_mul(limit, scale);
            } else {
                acceleration = psp_mul(delta, speed);
            }
            acceleration = rate + acceleration;
            position = position + psp_mul(speed, acceleration);
            put_float(rt, item + 0x18u, acceleration);
            put_float(rt, item + 0x14u, position);
            if (call(rt, ctx, 0x0894E714u) == 2u) {
                const float reference = field(rt, item, 0x10u);
                const float offset = position - reference;
                if (limit < offset) {
                    put_float(rt, item + 0x14u, limit + reference);
                    put(rt, item + 0x18u, 0u);
                } else if (offset < -limit) {
                    put(rt, item + 0x18u, 0u);
                    put_float(rt, item + 0x14u, reference - limit);
                }
            } else if (limit < position) {
                put_float(rt, item + 0x14u, limit);
                put(rt, item + 0x18u, 0u);
            } else if (position < -limit) {
                put_float(rt, item + 0x14u, -limit);
                put(rt, item + 0x18u, 0u);
            }
        }
    }
    if (byte_at(rt, 0x08CDE700u) != 0u) {
        const u32 kLimits = 0x08CF35F0u;
        for (u32 lane = 0u; lane != 4u; ++lane) {
            const u32 item = call(rt, ctx, 0x08955300u, {object, lane});
            const u32 pair = static_cast<u32>(static_cast<s32>(lane) >> 1);
            const u32 row = lanes + (pair << 2);
            float f21 = psp_mul(field(rt, item, 0x74u), field(rt, row, 0x13F0u));
            float f20 = field(rt, row, 0x13E8u);
            call(rt, ctx, 0x08945ECCu, {object});
            f20 = f20 + callf(rt, ctx, 0x089468C0u, {0x08CF3614u}, {0.0f}).f0;
            float curve = callf(rt, ctx, 0x089468C0u, {0x08CF3630u}, {f20}).f0;
            bool settled = false;
            if (0.0f <= f21) {
                f21 = f21 - curve;
                if (f21 < 0.0f) { f21 = 0.0f; settled = true; }
            } else {
                f21 = f21 + curve;
                if (0.0f < f21) { f21 = 0.0f; settled = true; }
            }
            if (!settled && f21 != 0.0f) {
                curve = callf(rt, ctx, 0x089468C0u, {0x08CF3644u}, {f20}).f0;
                f21 = psp_mul(f21, curve);
                float bound = float_at(rt, kLimits + 4u);
                if (bound < f21) f21 = bound;
                else if (f21 < -bound) f21 = -bound;
                if (pair == 1u) f21 = -f21;
            }
            const float step = callf(rt, ctx, 0x0895549Cu, {object}, {}).f0;
            float f4 = field(rt, item, 0xF0u);
            const float spring = float_at(rt, kLimits + 8u);
            const float drag = float_at(rt, kLimits + 0xCu);
            const float error = f21 - f4;
            float velocity = field(rt, item, 0xF4u);
            float total = field(rt, item, 0x10u);
            const float drag_term = psp_mul(drag, velocity);
            const float spring_term = psp_mul(spring, error);
            velocity = velocity + psp_mul(step, spring_term - drag_term);
            const float moved = psp_mul(step, velocity);
            put_float(rt, item + 0xF4u, velocity);
            f4 = f4 + moved;
            total = f4 + total;
            put_float(rt, item + 0xF0u, f4);
            put_float(rt, item + 0x10u, total);
        }
    }
    u32 table = lanes;
    for (u32 lane = 0u; lane != 4u; ++lane) {
        const u32 source = call(rt, ctx, 0x08955300u, {object, lane});
        const u32 pose = call(rt, ctx, 0x089554A8u, {object, lane});
        const u32 mode = call(rt, ctx, 0x0894E714u);
        if (mode == 1u) put_float(rt, pose + 8u, field(rt, source, 0x10u) + field(rt, source, 0x14u));
        else if (mode != 2u) put_float(rt, pose + 8u, field(rt, source, 0x10u));
        else put_float(rt, pose + 8u, field(rt, source, 0x14u));
        callf(rt, ctx, 0x08B498ACu, {pose, pose + 4u}, {field(rt, source, 0x10u)});
        callf(rt, ctx, 0x08B498ACu, {pose + 0xCu, pose + 0x10u}, {field(rt, pose, 8u)});
        put_float(rt, pose + 0x14u, field(rt, table, 0x30u));
        put_float(rt, pose + 0x18u, field(rt, table, 0x40u));
        table += 4u;
        put(rt, source + 0x24u, 0u);
        if (field(rt, source, 0x70u) != 0.0f) {
            float angle = field(rt, pose, 8u);
            put_float(rt, frame.at(0x10u), angle);
            if (field(rt, source, 0x30u) < 0.0f) put_float(rt, frame.at(0x10u), angle + kPi);
            const float wrapped = callf(rt, ctx, 0x0893C60Cu, {frame.at(0x10u), source + 0x28u}, {}).f0;
            put_float(rt, frame.at(0x10u), wrapped);
            put_float(rt, source + 0x24u, wrapped);
        }
    }
    for (u32 lane = 0u; lane != 4u; ++lane) {
        const u32 source = call(rt, ctx, 0x08955300u, {object, lane});
        const u32 pose = call(rt, ctx, 0x089554A8u, {object, lane});
        const float vx = field(rt, pose, 0xCu);
        const float vy = field(rt, pose, 0x10u);
        const float scale = field(rt, source, 0x34u);
        const float base_x = field(rt, source, 0x2Cu);
        const float base_y = field(rt, source, 0x30u);
        const float x = psp_mul(vx, scale) + base_x;
        const float y = base_y - psp_mul(scale, vy);
        put_float(rt, source + 0x44u, x);
        const float length = callf(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(x, x) + psp_mul(y, y)}).f0;
        put_float(rt, source + 0x48u, y);
        put_float(rt, source + 0x40u, length);
    }
}


// --- 0x088C5BF0 (a0 = owner, a1 = a "NLDA" track block, a2 = a context for 0x088C5834 / 0x088C58F0, a3 = an optional object with a vtable
// (+8 count, +0xC fetch a point)): for each of the ten mode bytes at 0x08CDD1AC..0x08CDD1B6 it fetches the mode's table (0x088C544C),
// walks its 64-byte entries fixing their positions, spacing and turn angles, and stores the results. v0 = 1 once a mode 6 table was
// handled, else 0 (also 0 for a null or foreign block).
void func_088C5BF0(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 scene = ctx.gpr[6];
    const u32 probe = ctx.gpr[7];
    GuestFrame frame(ctx, 0xD0u);
    const float frame_time = float_at(rt, owner + 8u);
    if (source == 0u) { ctx.gpr[2] = 0u; return; }
    if (word(rt, source) != 0x4E4C4441u) { ctx.gpr[2] = 0u; return; }
    constexpr float kPi = std::bit_cast<float>(0x40490FDBu);
    call(rt, ctx, 0x088C75C4u, {source, source});
    put(rt, owner + 0xCu, word(rt, source + 0x14u));
    const float half_time = psp_mul(frame_time, 0.5f);
    put(rt, owner + 0x10u, word(rt, source + 0x18u));
    u32 result = 0u;
    for (u32 mode_address = 0x08CDD1ACu; mode_address != 0x08CDD1B6u; ++mode_address) {
        const u32 mode = byte_at(rt, mode_address);
        const u32 table = call(rt, ctx, 0x088C544Cu, {owner, mode});
        if (table == 0u) continue;
        const s32 count = static_cast<s32>(word(rt, table));
        if (count <= 0) {
            put(rt, word(rt, owner + 0x10u) + (mode << 2), 0u);
            continue;
        }
        const bool track = ((mode ^ 4u) < 1u);
        float previous_distance = 0.0f;   // f25: the previous entry's e[0x14]
        float previous_x = 0.0f;          // f1
        float previous_z = 0.0f;          // f2
        float spacing = 1000.0f;          // f24
        bool wrapped = false;             // sp5C
        bool descending = false;          // sp60
        for (s32 index = 0; index < count; ++index) {
            const u32 entry = table + (static_cast<u32>(index) << 6) + 0x40u;
            float x = float_at(rt, entry + 4u);   // f23
            float z = float_at(rt, entry + 8u);   // f22
            put(rt, frame.at(0x14u), 0u);
            if (index > 0) {
                const float dx = x - previous_x;
                const float dz = z - previous_z;
                const float distance = callf(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(dx, dx) + psp_mul(dz, dz)}).f0;
                const float reach = spacing + (psp_mul(distance, std::bit_cast<float>(0x3DCCCCCDu)) + 1.0f);
                put(rt, frame.at(0x28u), 0xFFFFFFFFu);
                put(rt, frame.at(0x2Cu), 0xFFFFFFFFu);
                float answer = callf(rt, ctx, 0x088C5834u, {scene, frame.at(0x28u), frame.at(0x18u)}, {x, z, reach}).f0;
                s32 whole = trunc_w_s(answer);
                if (word(rt, frame.at(0x18u)) != 0u) {
                    answer = callf(rt, ctx, 0x088C5834u, {scene, frame.at(0x28u), frame.at(0x18u)}, {x, z, reach + 100.0f}).f0;
                    whole = trunc_w_s(answer);
                    if (word(rt, frame.at(0x18u)) != 0u) whole = trunc_w_s(spacing + 3.0f);
                }
                spacing = static_cast<float>(whole + 2);
            }
            const u32 kind = byte_at(rt, entry);
            const u32 stored_a = word(rt, entry + 0x38u);   // s2
            const u32 stored_b = word(rt, entry + 0x3Cu);   // s3
            if (kind != 0u) rt.memory().store8(entry, float_at(rt, entry + 0x18u) > 0.0f ? 1u : 2u);
            float ref_z = z;      // f20
            float ref_x = x;      // f21
            if (probe != 0u && track && index >= 3 && !((count - 4) < index)) {
                const u32 total = static_cast<u32>(call_indirect(rt, ctx, word(rt, word(rt, probe) + 8u), {probe}));
                if (static_cast<s32>(total) > 0) {
                    float best = 100000.0f;
                    s32 best_index = -1;
                    u32 candidate = 0u;
                    do {
                        const u32 found = call_indirect(rt, ctx, word(rt, word(rt, probe) + 0xCu), {probe, candidate, frame.at(0x30u)});
                        if (found != 0u) {
                            const float rx = float_at(rt, frame.at(0x30u)) - x;
                            const float rz = -float_at(rt, frame.at(0x38u)) - z;
                            const float d = callf(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(rx, rx) + psp_mul(rz, rz)}).f0;
                            if (!(5.0f < d)) {
                                if (d < best) { best = d; best_index = static_cast<s32>(candidate); }
                                else if (best_index == -1) { best = d; best_index = static_cast<s32>(candidate); }
                            }
                        }
                        ++candidate;
                    } while (candidate != total);
                    if (best_index != -1) {
                        call_indirect(rt, ctx, word(rt, word(rt, probe) + 0xCu), {probe, static_cast<u32>(best_index), frame.at(0x30u)});
                        ref_x = float_at(rt, frame.at(0x30u));
                        put_float(rt, entry + 4u, ref_x);
                        ref_z = -float_at(rt, frame.at(0x38u));
                        put_float(rt, entry + 8u, ref_z);
                        put_float(rt, frame.at(0x10u), float_at(rt, frame.at(0x3Cu)) + kPi);
                        put_float(rt, entry + 0x24u, callf(rt, ctx, 0x0893C54Cu, {frame.at(0x10u)}, {}).f0);
                    }
                }
            }
            const u32 lane_object = word(rt, owner + 4u);
            put_float(rt, frame.at(0x38u), ref_x);
            put_float(rt, frame.at(0x40u), -ref_z);
            put_float(rt, frame.at(0x3Cu), spacing);
            put(rt, frame.at(0x30u), stored_a);
            put(rt, frame.at(0x34u), stored_b);
            put_float(rt, entry + 0x14u, call_indirect(rt, ctx, word(rt, word(rt, lane_object) + 8u), {lane_object, frame.at(0x30u)}, {}).f0);
            put(rt, entry + 0xCu, 0u);
            put(rt, entry + 0x1Cu, 0u);
            put(rt, entry + 0x20u, 0u);
            rt.memory().store8(entry + 0x12u, 0u);
            rt.memory().store8(entry + 0x13u, 0u);
            put(rt, frame.at(0x0u), stored_a);
            put(rt, frame.at(0x4u), stored_b);
            const u32 resolved = callf(rt, ctx, 0x088C58F0u,
                                            {scene, frame.at(0x18u), frame.at(0x1Cu), frame.at(0x20u), frame.at(0x24u), frame.at(0x28u), frame.at(0x14u)},
                                            {float_at(rt, entry + 4u), float_at(rt, entry + 8u), spacing, float_at(rt, entry + 0x24u)}).v0;
            if (resolved != 0u) {
                spacing = float_at(rt, frame.at(0x18u));
                put_float(rt, entry + 0xCu, spacing);
                put(rt, entry + 0x1Cu, word(rt, frame.at(0x1Cu)));
                put(rt, entry + 0x20u, word(rt, frame.at(0x20u)));
                rt.memory().store8(entry + 0x12u, word(rt, frame.at(0x24u)) & 0xFFu);
                rt.memory().store8(entry + 0x13u, word(rt, frame.at(0x28u)) & 0xFFu);
            }
            float distance = float_at(rt, entry + 0x14u);
            if (track) {
                if (distance < half_time) {
                    distance = frame_time + distance;
                    wrapped = true;
                    put_float(rt, entry + 0x14u, distance);
                } else if (wrapped) {
                    distance = frame_time + distance;
                    put_float(rt, entry + 0x14u, distance);
                }
            }
            if (distance <= previous_distance) descending = true;
            previous_x = x;
            previous_z = z;
            previous_distance = distance;
        }
        // Second pass: segment lengths and turn angles between each entry and the next (the last wraps to the first).
        const s32 last = count - 1;
        for (s32 index = 0; index < count;) {
            const u32 entry = table + (static_cast<u32>(index) << 6) + 0x40u;
            const s32 next_index = index < last ? index + 1 : 0;
            ++index;
            const u32 next = table + (static_cast<u32>(next_index) << 6) + 0x40u;
            const float dx = float_at(rt, next + 4u) - float_at(rt, entry + 4u);
            rt.memory().store8(entry + 0x28u, 0u);
            const float dz = float_at(rt, next + 8u) - float_at(rt, entry + 8u);
            rt.memory().store8(entry + 0x29u, 0u);
            rt.memory().store8(entry + 0x2Au, 0u);
            rt.memory().store8(entry + 0x2Bu, 0u);
            const float length = callf(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(dx, dx) + psp_mul(dz, dz)}).f0;
            const u32 kind = byte_at(rt, entry);
            put_float(rt, entry + 0x2Cu, length);
            if (((kind - 1u) & 0xFFu) < 2u) {
                float slope = float_at(rt, entry + 0x18u);
                if (slope < 0.0f) slope = -slope;
                float other = float_at(rt, next + 0x24u);
                float own = float_at(rt, entry + 0x24u);
                float turn;
                if (kind == 1u) {
                    if (other < own) own = own - std::bit_cast<float>(0x40C90FDBu);
                    turn = other - own;
                } else {
                    if (own < other) other = other - std::bit_cast<float>(0x40C90FDBu);
                    turn = other - own;
                }
                if (turn < 0.0f) turn = -turn;
                put_float(rt, entry + 0x2Cu, psp_mul(slope, turn));
            }
            const float rise = float_at(rt, next + 0xCu) - float_at(rt, entry + 0xCu);
            const float arc = float_at(rt, entry + 0x2Cu);
            put_float(rt, entry + 0x30u, rise);
            if (0.0f < arc) put_float(rt, entry + 0x34u, callf(rt, ctx, 0x08B4A430u, {}, {rise, arc}).f0);
        }
        {
            const u32 previous_of_first = static_cast<u32>(last);
            u32 entry = table + 0x40u;
            for (s32 index = 0; index < count; ++index, entry += 0x40u) {
                const u32 before = index > 0 ? static_cast<u32>(index - 1) : previous_of_first;
                const float before_value = float_at(rt, table + (before << 6) + 0x74u);
                const float step = float_at(rt, entry + 0x34u) - before_value;
                if (step < std::bit_cast<float>(0xBC8EFA35u)) rt.memory().store8(entry + 0x28u, 1u);
            }
        }
        if (descending) {
            if (((mode - 1u) < 2u) || mode == 7u) put(rt, word(rt, owner + 0x10u) + (mode << 2), 0u);
        }
        if (mode == 6u) result = 1u;
    }
    ctx.gpr[2] = result;
}


// --- 0x088B5ED4 (a0 = rect record of eight floats + outputs at +0x20..+0x3C, a1 = matrix-like record, f12/f13 = two scales): the rect's
// 2x2 cross and dot products pick one of eight placements (the jump table at 0x08C89D9C: 0-3 the general cases, which also store 8 at
// rect + 0x40, 4-7 the axis-aligned ones); each shifts the rect's corners by (m0, m8) and the scaled axis vectors A, B, C, D and stores
// the eight outputs. All reads happen before the stores of the same field. v0 ends as the jump-table slot address.
void func_088B5ED4(Runtime &rt, AllegrexContext &ctx) {
    const u32 rect = ctx.gpr[4];
    const u32 basis = ctx.gpr[5];
    const float scale_a = ctx.fpr[12];
    const float scale_b = ctx.fpr[13];
    const float r0 = float_at(rt, rect + 0u);
    const float r4 = float_at(rt, rect + 4u);
    const float r8 = float_at(rt, rect + 8u);
    const float rc = float_at(rt, rect + 0xCu);
    const float basis_x = float_at(rt, basis + 0x20u);
    const float basis_y = float_at(rt, basis + 0x28u);
    const float dx = r0 - r8;
    const float dy = r4 - rc;
    const float cross = psp_mul(dy, basis_x) - psp_mul(dx, basis_y);
    const float dot = psp_mul(dx, basis_x) + psp_mul(dy, basis_y);
    u32 selector;
    if (cross == 0.0f) {
        selector = (0.0f < dot) ? 4u : 5u;
    } else if (dot == 0.0f) {
        selector = (0.0f < cross) ? 6u : 7u;
    } else {
        const u32 half = (0.0f < dot) ? 0u : 2u;
        const u32 side = (cross <= 0.0f) ? 1u : 0u;
        put(rt, rect + 0x40u, 8u);
        selector = half | side;
    }
    const float D = psp_mul(basis_y, scale_b);
    const float B = psp_mul(scale_a, basis_y);
    const float C = psp_mul(basis_x, scale_b);
    const float m8 = float_at(rt, basis + 8u);
    const float A = psp_mul(basis_x, scale_a);
    const float m0 = float_at(rt, basis + 0u);
    const float r10 = float_at(rt, rect + 0x10u);
    const float r14 = float_at(rt, rect + 0x14u);
    const float r18 = float_at(rt, rect + 0x18u);
    const float r1c = float_at(rt, rect + 0x1Cu);
    ctx.gpr[2] = 0x08C89D9Cu + (selector << 2);
    switch (selector) {
    case 0: {
        put_float(rt, rect + 0x30U, (r18 - m0) - C);
        put_float(rt, rect + 0x38U, A + (r18 - m0));
        put_float(rt, rect + 0x3CU, B + (r1c - m8));
        put_float(rt, rect + 0x34U, (r1c - m8) - D);
        put_float(rt, rect + 0x28U, (r10 - m0) - C);
        put_float(rt, rect + 0x2CU, (r14 - m8) - D);
        put_float(rt, rect + 0x20U, (m0 + r10) - C);
        put_float(rt, rect + 0x24U, (m8 + r14) - D);
        put_float(rt, rect + 0x18U, (m0 + r8) - C);
        put_float(rt, rect + 0x1CU, (m8 + rc) - D);
        put_float(rt, rect + 0x10U, A + (m0 + r8));
        put_float(rt, rect + 0x14U, B + (m8 + rc));
        put_float(rt, rect + 0x8U, A + (m0 + r0));
        put_float(rt, rect + 0xCU, B + (m8 + r4));
        put_float(rt, rect + 0x0U, A + (r0 - m0));
        put_float(rt, rect + 0x4U, B + (r4 - m8));
        break;
    }
    case 1: {
        put_float(rt, rect + 0x30U, A + (r18 - m0));
        put_float(rt, rect + 0x38U, A + (m0 + r18));
        put_float(rt, rect + 0x3CU, B + (m8 + r1c));
        put_float(rt, rect + 0x34U, B + (r1c - m8));
        put_float(rt, rect + 0x28U, A + (r10 - m0));
        put_float(rt, rect + 0x2CU, B + (r14 - m8));
        put_float(rt, rect + 0x20U, (r10 - m0) - C);
        put_float(rt, rect + 0x24U, (r14 - m8) - D);
        put_float(rt, rect + 0x18U, (r8 - m0) - C);
        put_float(rt, rect + 0x1CU, (rc - m8) - D);
        put_float(rt, rect + 0x10U, (m0 + r8) - C);
        put_float(rt, rect + 0x14U, (m8 + rc) - D);
        put_float(rt, rect + 0x8U, (m0 + r0) - C);
        put_float(rt, rect + 0xCU, (m8 + r4) - D);
        put_float(rt, rect + 0x0U, A + (m0 + r0));
        put_float(rt, rect + 0x4U, B + (m8 + r4));
        break;
    }
    case 2: {
        put_float(rt, rect + 0x30U, (m0 + r18) - C);
        put_float(rt, rect + 0x38U, (r18 - m0) - C);
        put_float(rt, rect + 0x3CU, (r1c - m8) - D);
        put_float(rt, rect + 0x34U, (m8 + r1c) - D);
        put_float(rt, rect + 0x28U, (m0 + r10) - C);
        put_float(rt, rect + 0x2CU, (m8 + r14) - D);
        put_float(rt, rect + 0x20U, A + (m0 + r10));
        put_float(rt, rect + 0x24U, B + (m8 + r14));
        put_float(rt, rect + 0x18U, A + (m0 + r8));
        put_float(rt, rect + 0x1CU, B + (m8 + rc));
        put_float(rt, rect + 0x10U, A + (r8 - m0));
        put_float(rt, rect + 0x14U, B + (rc - m8));
        put_float(rt, rect + 0x8U, A + (r0 - m0));
        put_float(rt, rect + 0xCU, B + (r4 - m8));
        put_float(rt, rect + 0x0U, (r0 - m0) - C);
        put_float(rt, rect + 0x4U, (r4 - m8) - D);
        break;
    }
    case 3: {
        put_float(rt, rect + 0x30U, A + (m0 + r18));
        put_float(rt, rect + 0x38U, (m0 + r18) - C);
        put_float(rt, rect + 0x3CU, (m8 + r1c) - D);
        put_float(rt, rect + 0x34U, B + (m8 + r1c));
        put_float(rt, rect + 0x28U, A + (m0 + r10));
        put_float(rt, rect + 0x2CU, B + (m8 + r14));
        put_float(rt, rect + 0x20U, A + (r10 - m0));
        put_float(rt, rect + 0x24U, B + (r14 - m8));
        put_float(rt, rect + 0x18U, A + (r8 - m0));
        put_float(rt, rect + 0x1CU, B + (rc - m8));
        put_float(rt, rect + 0x10U, (r8 - m0) - C);
        put_float(rt, rect + 0x14U, (rc - m8) - D);
        put_float(rt, rect + 0x8U, (r0 - m0) - C);
        put_float(rt, rect + 0xCU, (r4 - m8) - D);
        put_float(rt, rect + 0x0U, (m0 + r0) - C);
        put_float(rt, rect + 0x4U, (m8 + r4) - D);
        break;
    }
    case 4: {
        put_float(rt, rect + 0x0U, r0 + (m0 + A));
        put_float(rt, rect + 0x4U, r4 + (m8 + B));
        put_float(rt, rect + 0x8U, r8 + (m0 - C));
        put_float(rt, rect + 0xCU, rc + (m8 - D));
        put_float(rt, rect + 0x10U, r10 + ((-m0) - C));
        put_float(rt, rect + 0x1CU, r1c + (B - m8));
        put_float(rt, rect + 0x14U, r14 + ((-m8) - D));
        put_float(rt, rect + 0x18U, r18 + (A - m0));
        break;
    }
    case 5: {
        put_float(rt, rect + 0x0U, r0 + ((-m0) - C));
        put_float(rt, rect + 0x4U, r4 + ((-m8) - D));
        put_float(rt, rect + 0x8U, r8 + (A - m0));
        put_float(rt, rect + 0xCU, rc + (B - m8));
        put_float(rt, rect + 0x10U, r10 + (m0 + A));
        put_float(rt, rect + 0x1CU, r1c + (m8 - D));
        put_float(rt, rect + 0x14U, r14 + (m8 + B));
        put_float(rt, rect + 0x18U, r18 + (m0 - C));
        break;
    }
    case 6: {
        put_float(rt, rect + 0x0U, r0 + (A - m0));
        put_float(rt, rect + 0x4U, r4 + (B - m8));
        put_float(rt, rect + 0x8U, r8 + (m0 + A));
        put_float(rt, rect + 0xCU, rc + (m8 + B));
        put_float(rt, rect + 0x10U, r10 + (m0 - C));
        put_float(rt, rect + 0x1CU, r1c + ((-m8) - D));
        put_float(rt, rect + 0x14U, r14 + (m8 - D));
        put_float(rt, rect + 0x18U, r18 + ((-m0) - C));
        break;
    }
    case 7: {
        put_float(rt, rect + 0x0U, r0 + (m0 - C));
        put_float(rt, rect + 0x4U, r4 + (m8 - D));
        put_float(rt, rect + 0x8U, r8 + ((-m0) - C));
        put_float(rt, rect + 0xCU, rc + ((-m8) - D));
        put_float(rt, rect + 0x10U, r10 + (A - m0));
        put_float(rt, rect + 0x1CU, r1c + (m8 + B));
        put_float(rt, rect + 0x14U, r14 + (B - m8));
        put_float(rt, rect + 0x18U, r18 + (m0 + A));
        break;
    }
    default: break;
    }
}


// --- 0x08B6975C: argument reduction of a double (a0 = low word, a1 = high word) modulo pi/2 (the fdlibm __ieee754_rem_pio2 shape) with
// the double arithmetic done by the soft-float helpers: 0x08B712B0 / 0x08B71328 (the two add-class ops), 0x08B71394 (multiply),
// 0x08B71DC8 (to int), 0x08B71CE0 (from int), 0x08B6E268 (absolute value), 0x08B71980 (compare), 0x08B6C518 (large-argument reduction).
// a2 = output pair (y0 at +0, y1 at +8, each as low/high words); v0 = the quadrant count (negated for a negative argument).
struct Dbl {
    u32 lo;
    u32 hi;
};
Dbl dbl_op(Runtime &rt, AllegrexContext &ctx, u32 target, Dbl a, Dbl b) {
    const GuestResult r = callf(rt, ctx, target, {a.lo, a.hi, b.lo, b.hi}, {});
    return {r.v0, r.v1};
}
// `ext rd, rs, 20, 11`: the biased exponent field of a double's high word.
s32 exponent_of(u32 high) { return static_cast<s32>((high << 1) >> 21); }
Dbl dbl_const(Runtime &rt, u32 address) { return {word(rt, address), word(rt, address + 4u)}; }
void put_dbl(Runtime &rt, u32 address, Dbl value) {
    put(rt, address, value.lo);
    put(rt, address + 4u, value.hi);
}

void func_08B6975C(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kSub = 0x08B712B0u;
    constexpr u32 kAdd = 0x08B71328u;
    constexpr u32 kMul = 0x08B71394u;
    const Dbl x{ctx.gpr[4], ctx.gpr[5]};
    const u32 out = ctx.gpr[6];
    const s32 sign_word = static_cast<s32>(x.hi);
    const u32 ix = (x.hi << 1) >> 1;   // `ins s4, zero, 31, 1`: the sign bit cleared
    GuestFrame frame(ctx, 0x70u);
    s32 quadrant = 0;
    const auto finish = [&]() { ctx.gpr[2] = static_cast<u32>(quadrant); };
    if (!(0x3FE921FBu < ix)) {
        put_dbl(rt, out, x);
        put(rt, out + 8u, 0u);
        put(rt, out + 0xCu, 0u);
        finish();
        return;
    }
    if (!(0x4002D97Bu < ix)) {
        // |x| < 3 pi / 4: one step of pi/2 off the argument
        if (sign_word > 0) {
            const Dbl z = dbl_op(rt, ctx, kSub, x, dbl_const(rt, 0x08CB67C8u));
            quadrant = 1;
            if (ix == 0x3FF921FBu) {
                const Dbl first = dbl_op(rt, ctx, kSub, z, dbl_const(rt, 0x08CB67D8u));
                const Dbl second = dbl_op(rt, ctx, kSub, first, dbl_const(rt, 0x08CB67E0u));
                put_dbl(rt, out, second);
                const Dbl tail = dbl_op(rt, ctx, kSub, first, second);
                put_dbl(rt, out + 8u, dbl_op(rt, ctx, kSub, tail, dbl_const(rt, 0x08CB67E0u)));
            } else {
                const Dbl small = dbl_const(rt, 0x08CB67D0u);
                const Dbl reduced = dbl_op(rt, ctx, kSub, z, small);
                put_dbl(rt, out, reduced);
                const Dbl tail = dbl_op(rt, ctx, kSub, z, reduced);
                put_dbl(rt, out + 8u, dbl_op(rt, ctx, kSub, tail, small));
            }
        } else {
            const Dbl z = dbl_op(rt, ctx, kAdd, x, dbl_const(rt, 0x08CB67C8u));
            quadrant = -1;
            if (ix == 0x3FF921FBu) {
                const Dbl first = dbl_op(rt, ctx, kAdd, z, dbl_const(rt, 0x08CB67D8u));
                const Dbl second = dbl_op(rt, ctx, kAdd, first, dbl_const(rt, 0x08CB67E0u));
                put_dbl(rt, out, second);
                const Dbl tail = dbl_op(rt, ctx, kSub, first, second);
                put_dbl(rt, out + 8u, dbl_op(rt, ctx, kAdd, tail, dbl_const(rt, 0x08CB67E0u)));
            } else {
                const Dbl small = dbl_const(rt, 0x08CB67D0u);
                const Dbl reduced = dbl_op(rt, ctx, kAdd, z, small);
                put_dbl(rt, out, reduced);
                const Dbl tail = dbl_op(rt, ctx, kSub, z, reduced);
                put_dbl(rt, out + 8u, dbl_op(rt, ctx, kAdd, tail, small));
            }
        }
        finish();
        return;
    }
    if (!(0x413921FBu < ix)) {
        // medium: n = round(|x| * 2/pi), then subtract n * pi/2 in up to three pieces
        const GuestResult magnitude_result = callf(rt, ctx, 0x08B6E268u, {x.lo, x.hi}, {});
        const Dbl magnitude{magnitude_result.v0, magnitude_result.v1};
        const Dbl scaled = dbl_op(rt, ctx, kMul, magnitude, dbl_const(rt, 0x08CB67E8u));
        const Dbl rounded = dbl_op(rt, ctx, kAdd, scaled, dbl_const(rt, 0x08CB67F0u));
        const u32 whole = call(rt, ctx, 0x08B71DC8u, {rounded.lo, rounded.hi});
        const GuestResult as_double = callf(rt, ctx, 0x08B71CE0u, {whole}, {});
        quadrant = static_cast<s32>(whole);
        const Dbl count{as_double.v0, as_double.v1};
        put_dbl(rt, frame.at(0x30u), count);
        const Dbl piece = dbl_op(rt, ctx, kMul, count, dbl_const(rt, 0x08CB67F8u));
        Dbl remainder = dbl_op(rt, ctx, kAdd, magnitude, piece);
        Dbl tail_term = dbl_op(rt, ctx, kMul, count, dbl_const(rt, 0x08CB67D0u));
        const bool refine = quadrant >= 32 || ix == word(rt, 0x08C96180u + (static_cast<u32>(quadrant) << 2) - 4u);
        if (!refine) {
            put_dbl(rt, out, dbl_op(rt, ctx, kSub, remainder, tail_term));
        } else {
            Dbl y0 = dbl_op(rt, ctx, kSub, remainder, tail_term);
            put_dbl(rt, out, y0);
            const s32 exponent = static_cast<s32>(ix) >> 20;
            if (!(exponent - exponent_of(y0.hi) < 0x11)) {
                const Dbl second_piece = dbl_op(rt, ctx, kMul, count, dbl_const(rt, 0x08CB67D8u));
                const Dbl w2 = dbl_op(rt, ctx, kSub, remainder, second_piece);
                put_dbl(rt, frame.at(0x20u), w2);
                const Dbl second_tail = dbl_op(rt, ctx, kMul, count, dbl_const(rt, 0x08CB67E0u));
                const Dbl step1 = dbl_op(rt, ctx, kSub, remainder, w2);
                const Dbl step2 = dbl_op(rt, ctx, kSub, step1, second_piece);
                const Dbl step3 = dbl_op(rt, ctx, kSub, second_tail, step2);
                y0 = dbl_op(rt, ctx, kSub, w2, step3);
                put_dbl(rt, out, y0);
                remainder = w2;
                tail_term = step3;
                if (!(exponent - exponent_of(y0.hi) < 0x32)) {
                    const Dbl third_piece = dbl_op(rt, ctx, kMul, count, dbl_const(rt, 0x08CB6800u));
                    const Dbl w3 = dbl_op(rt, ctx, kSub, w2, third_piece);
                    const Dbl third_tail = dbl_op(rt, ctx, kMul, count, dbl_const(rt, 0x08CB6808u));
                    const Dbl s1 = dbl_op(rt, ctx, kSub, w2, w3);
                    const Dbl s2 = dbl_op(rt, ctx, kSub, s1, third_piece);
                    const Dbl s3 = dbl_op(rt, ctx, kSub, third_tail, s2);
                    y0 = dbl_op(rt, ctx, kSub, w3, s3);
                    put_dbl(rt, out, y0);
                    remainder = w3;
                    tail_term = s3;
                }
            }
        }
        const Dbl stored{word(rt, out), word(rt, out + 4u)};
        const Dbl delta = dbl_op(rt, ctx, kSub, remainder, stored);
        const Dbl y1 = dbl_op(rt, ctx, kSub, delta, tail_term);
        put_dbl(rt, out + 8u, y1);
        if (sign_word < 0) {
            quadrant = -quadrant;
            put(rt, out + 0xCu, y1.hi ^ 0x80000000u);
            put(rt, out + 8u, y1.lo);
            put(rt, out, stored.lo);
            put(rt, out + 4u, stored.hi ^ 0x80000000u);
        }
        finish();
        return;
    }
    if (!(0x7FEFFFFFu < ix)) {
        // large: split the double into three 24-bit chunks and hand them to the big reduction
        const s32 exponent = static_cast<s32>((ix >> 20)) - 0x416;
        const Dbl scale = dbl_const(rt, 0x08CB6810u);
        Dbl value{x.lo, ix - (static_cast<u32>(exponent) << 20)};
        for (u32 index = 0u; index != 2u; ++index) {
            const u32 whole = call(rt, ctx, 0x08B71DC8u, {value.lo, value.hi});
            const GuestResult as_double = callf(rt, ctx, 0x08B71CE0u, {whole}, {});
            const Dbl chunk{as_double.v0, as_double.v1};
            put_dbl(rt, frame.at(index * 8u), chunk);
            const Dbl rest = dbl_op(rt, ctx, kSub, value, chunk);
            value = dbl_op(rt, ctx, kMul, rest, scale);
        }
        put_dbl(rt, frame.at(0x10u), value);
        u32 pointer = frame.at(0x10u);
        u32 terms = 3u;
        for (;;) {
            const u32 zero_test = call(rt, ctx, 0x08B71980u, {word(rt, pointer), word(rt, pointer + 4u), 0u, 0u});
            pointer -= 8u;
            if (zero_test != 0u) break;
            terms -= 1u;
        }
        quadrant = static_cast<s32>(callf(rt, ctx, 0x08B6C518u, {frame.at(0u), out, static_cast<u32>(exponent), terms, 2u, 0x08C96078u}, {}).v0);
        if (sign_word < 0) {
            quadrant = -quadrant;
            put(rt, out + 0xCu, word(rt, out + 0xCu) ^ 0x80000000u);
            put(rt, out + 4u, word(rt, out + 4u) ^ 0x80000000u);
        }
        finish();
        return;
    }
    // infinity or NaN: x - x
    const Dbl nan = dbl_op(rt, ctx, kSub, x, x);
    put_dbl(rt, out, nan);
    put_dbl(rt, out + 8u, nan);
    finish();
}


// --- 0x08B76AC0 (a0 = an index object, a1 = a key): looks `key` up in the object's table of ranges, (re)building the table first when
// the packed header word at +0x10 says it is stale. obj+0x10: bit 0 = built, bit 1 = list-of-lists, bit 2 = packed ranges, bits 3-10 =
// the packed kind, bits 11.. = the entry count; obj+0xC = the table (+0 next, +4 count, +8.. entry pointers), obj+0 = the lowest key.
// The rebuild merges two freshly allocated tables (0x08B35DA0) with the comparator chosen from the flags (0x08B773C0 / 0x08B760A8 /
// 0x08B76080), then the search is a binary search over the entries (start at +8, size at +0xC). v0 = the entry found, or 0.
void func_08B76AC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x80u);
    const u32 sentinel = 0x08D72CF0u;
    const auto abort_path = [&]() { call(rt, ctx, 0x08B77B04u); };
    const auto lookup = [&](u32 flags) {
        u32 found = 0u;
        if (flags & 1u) {
            if (flags & 4u) {
                const u32 table = word(rt, object + 0xCu);
                const u32 high_start = word(rt, table + 4u);
                if (high_start != 0u) {
                    u32 low = 0u;
                    u32 high = high_start;
                    do {
                        const u32 mid = (low + high) >> 1;
                        const u32 entry = word(rt, table + 8u + (mid << 2));
                        const u32 packed = call(rt, ctx, 0x08B75F68u, {entry + 4u - word(rt, entry + 4u)});
                        const u32 kind = (packed << 24) >> 24;
                        const u32 info = call(rt, ctx, 0x08B75EDCu, {kind, object});
                        const u32 start_at = call(rt, ctx, 0x08B75DA0u, {kind, info, entry + 8u, frame.at(4u)});
                        call(rt, ctx, 0x08B75DA0u, {kind & 0xFu, 0u, start_at, frame.at(0u)});
                        const u32 start = word(rt, frame.at(4u));
                        if (key < start) {
                            high = mid;
                        } else {
                            if (key < start + word(rt, frame.at(0u))) { found = entry; break; }
                            low = mid + 1u;
                        }
                    } while (low < high);
                }
            } else if ((flags & 0x7F8u) != 0u) {
                const u32 table = word(rt, object + 0xCu);
                const u32 kind = (flags << 21) >> 24;
                const u32 info = call(rt, ctx, 0x08B75EDCu, {kind, object});
                const u32 total = word(rt, table + 4u);
                if (total != 0u) {
                    u32 low = 0u;
                    u32 high = total;
                    do {
                        const u32 mid = (low + high) >> 1;
                        const u32 entry = word(rt, table + 8u + (mid << 2));
                        const u32 start_at = call(rt, ctx, 0x08B75DA0u, {kind, info, entry + 8u, frame.at(0u)});
                        call(rt, ctx, 0x08B75DA0u, {kind & 0xFu, 0u, start_at, frame.at(4u)});
                        const u32 start = word(rt, frame.at(0u));
                        if (key < start) {
                            high = mid;
                        } else {
                            if (key < start + word(rt, frame.at(4u))) { found = entry; break; }
                            low = mid + 1u;
                        }
                    } while (low < high);
                }
            } else {
                const u32 table = word(rt, object + 0xCu);
                u32 high = word(rt, table + 4u);
                if (high != 0u) {
                    u32 low = 0u;
                    do {
                        const u32 mid = (low + high) >> 1;
                        const u32 entry = word(rt, table + 8u + (mid << 2));
                        const u32 start = word(rt, entry + 8u);
                        if (key < start) {
                            high = mid;
                        } else {
                            if (key < start + word(rt, entry + 0xCu)) { found = entry; break; }
                            low = mid + 1u;
                        }
                    } while (low < high);
                }
            }
        } else if (flags & 2u) {
            for (u32 list = word(rt, object + 0xCu); word(rt, list) != 0u; list += 4u) {
                found = call(rt, ctx, 0x08B764ACu, {object, word(rt, list), key});
                if (found != 0u) break;
            }
        } else {
            found = call(rt, ctx, 0x08B764ACu, {object, word(rt, object + 0xCu), key});
        }
        ctx.gpr[2] = found;
    };
    const u32 entry_flags = word(rt, object + 0x10u);
    put(rt, frame.at(0x30u), key);
    if (entry_flags & 1u) {
        lookup(entry_flags);
        return;
    }
    u32 count = entry_flags >> 11;
    put(rt, frame.at(0x2Cu), count);
    if (count == 0u) {
        // recount the entries of the list-of-lists (or of the single table), then build when something is there
        u32 total;
        if (entry_flags & 2u) {
            total = 0u;
            for (u32 list = word(rt, object + 0xCu); word(rt, list) != 0u; list += 4u) total += call(rt, ctx, 0x08B7634Cu, {object, word(rt, list)});
            ctx.gpr[2] = total;
        } else {
            total = call(rt, ctx, 0x08B7634Cu, {object, word(rt, object + 0xCu)});
        }
        put(rt, frame.at(0x2Cu), total);
        put(rt, object + 0x10u, (word(rt, object + 0x10u) & 0x7FFu) | (total << 11));
        const u32 stored = word(rt, object + 0x10u);
        if (total != (stored >> 11)) put(rt, object + 0x10u, stored & 0x7FFu);
        count = total;
        if (count == 0u) {
            if (key < word(rt, object)) { ctx.gpr[2] = 0u; return; }
            lookup(word(rt, object + 0x10u));
            return;
        }
    }
    // Build the merged table for `count` entries.
    const u32 bytes = (count << 2) + 8u;
    const u32 first = call(rt, ctx, 0x08B35DA0u, {bytes});
    put(rt, frame.at(8u), first);
    if (first != 0u) {
        put(rt, first + 4u, 0u);
        const u32 second = call(rt, ctx, 0x08B35DA0u, {bytes});
        put(rt, frame.at(0xCu), second);
        if (second != 0u) put(rt, second + 4u, 0u);
        if (word(rt, object + 0x10u) & 2u) {
            for (u32 list = word(rt, object + 0xCu); word(rt, list) != 0u; list += 4u) call(rt, ctx, 0x08B76638u, {object, frame.at(8u), word(rt, list)});
        } else {
            call(rt, ctx, 0x08B76638u, {object, frame.at(8u), word(rt, object + 0xCu)});
        }
        u32 chain = word(rt, frame.at(8u));
        put(rt, frame.at(0x28u), chain);
        if (chain != 0u && word(rt, chain + 4u) != word(rt, frame.at(0x2Cu))) { abort_path(); return; }
        const u32 flags = word(rt, object + 0x10u);
        const u32 comparator = (flags & 4u) ? 0x08B773C0u : ((flags & 0x7F8u) != 0u ? 0x08B760A8u : 0x08B76080u);
        put(rt, frame.at(0x1Cu), comparator);
        u32 merged = word(rt, frame.at(0xCu));
        put(rt, frame.at(0x24u), merged);
        if (merged == 0u) {
            call(rt, ctx, 0x08B76274u, {object, comparator, chain});
        } else {
            u32 kept = 0u;      // t0
            u32 moved = 0u;     // t1
            const u32 entries = word(rt, chain + 4u);
            put(rt, frame.at(0x20u), entries);
            if (entries != 0u) {
                // Link every entry to the previous smaller one (a chain of A's entries in B's slots).
                u32 placed = 0u;
                u32 link = sentinel;
                for (;;) {
                    put(rt, merged + 8u + (placed << 2), link);
                    const u32 current = placed++;
                    link = chain + 8u + (current << 2);
                    if (placed == entries) break;
                    for (;;) {
                        if (link == sentinel) break;
                        const s32 order = static_cast<s32>(call_indirect(rt, ctx, comparator, {object, word(rt, chain + 8u + (placed << 2)), word(rt, link)}));
                        if (order >= 0) break;
                        const u32 slot = merged + (((link - (chain + 8u)) >> 2) << 2);
                        link = word(rt, slot + 8u);
                        put(rt, slot + 8u, 0u);
                        if (link == sentinel) break;
                    }
                }
                // Split the entries: those whose slot stayed linked remain in A, the others move to B.
                u32 scan_b = merged;
                u32 scan_a = chain;
                u32 write_a = chain + 8u;
                u32 write_b = merged + 8u;
                u32 index = 0u;
                do {
                    const u32 slot = word(rt, scan_b + 8u);
                    const u32 value = word(rt, scan_a + 8u);
                    ++index;
                    if (slot != 0u) {
                        ++kept;
                        put(rt, write_a, value);
                        write_a += 4u;
                    } else {
                        ++moved;
                        put(rt, write_b, value);
                        write_b += 4u;
                    }
                    scan_b += 4u;
                    scan_a += 4u;
                } while (word(rt, frame.at(0x20u)) != index);
                chain = word(rt, frame.at(8u));
                merged = word(rt, frame.at(0xCu));
            }
            put(rt, word(rt, frame.at(0x28u)) + 4u, kept);
            put(rt, word(rt, frame.at(0x24u)) + 4u, moved);
            if (word(rt, chain + 4u) + word(rt, merged + 4u) != word(rt, frame.at(0x2Cu))) { abort_path(); return; }
            call(rt, ctx, 0x08B76274u, {object, word(rt, frame.at(0x1Cu)), merged});
            merged = word(rt, frame.at(0xCu));
            const u32 merged_count = word(rt, merged + 4u);
            if (merged_count != 0u) {
                const u32 target = word(rt, frame.at(8u));
                u32 sorted = word(rt, target + 4u);
                u32 position = merged + (merged_count << 2) + 4u;
                u32 down = merged_count;
                u32 done = 0u;
                do {
                    down -= 1u;
                    const u32 item = word(rt, position);
                    put(rt, frame.at(0x18u), item);
                    if (sorted != 0u) {
                        u32 source = target + (sorted << 2) + 4u;
                        u32 sink = target + ((sorted + down) << 2) + 8u;
                        u32 walk = sorted;
                        u32 shifted = 0u;
                        for (;;) {
                            const u32 next = walk - 1u;
                            const s32 order = static_cast<s32>(call_indirect(rt, ctx, word(rt, frame.at(0x1Cu)), {object, word(rt, source), item}));
                            if (order <= 0) { sorted = walk; break; }
                            put(rt, sink, word(rt, source));
                            ++shifted;
                            source -= 4u;
                            sink -= 4u;
                            if (sorted == shifted) { sorted = next; break; }
                            walk = next;
                        }
                    }
                    put(rt, target + ((sorted + down) << 2) + 8u, item);
                    ++done;
                    position -= 4u;
                } while (done != merged_count);
                put(rt, target + 4u, word(rt, target + 4u) + word(rt, merged + 4u));
            }
            call(rt, ctx, 0x08B35254u, {merged});
        }
        const u32 table = word(rt, frame.at(8u));
        put(rt, table, word(rt, object + 0xCu));
        put(rt, object + 0xCu, table);
        put(rt, object + 0x10u, word(rt, object + 0x10u) | 1u);
    }
    if (key < word(rt, object)) { ctx.gpr[2] = 0u; return; }
    lookup(word(rt, object + 0x10u));
}

} // namespace
} // namespace new_batch_main_137
} // namespace gt::game

#define NEW137(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_137::name, gt::ReturnKind::kind)
NEW137(0x0894E9C0u, func_0894E9C0, Word);
NEW137(0x088C5BF0u, func_088C5BF0, Word);
NEW137(0x088B5ED4u, func_088B5ED4, Word);
NEW137(0x08B6975Cu, func_08B6975C, Word);
NEW137(0x08B76AC0u, func_08B76AC0, Word);
