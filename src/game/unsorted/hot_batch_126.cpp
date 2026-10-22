// Hot batch 126: functions of 350-450 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <optional>

namespace gt::game {
namespace hot_batch_126 {
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
// A 0x2C-byte focus entry is plain data (11 words); its assignment copies them all.
void copy_entry(psprecomp::Runtime &rt, std::uint32_t to, std::uint32_t from) {
    for (std::uint32_t k = 0u; k != 0x2Cu; k += 4u) put(rt, to + k, word(rt, from + k));
}

} // namespace


// func_08A15F08(state, entries, node, index, f12 = x, f13 = y): collects the focus entries of a node tree for
// func_08A16708. Nothing (0) unless func_089660F4(node). A node func_08968C84 accepts is a leaf: its offset
// (func_0896A35C) and two rectangles (func_0896B260 / func_0896B410, moved by (x, y)) are clipped to the clip rectangle
// func_08A15534(state, &clip) answers (none: kept as they are); an empty clipped area falls through to the children,
// otherwise func_08A154E4(state, &a, &b) runs and the entry {node, state+0x48, a, b, index} is pushed onto `entries`
// (func_08C160CC when full); the focused node (state+0x34) also stores a / b at state+0x14 / +0x24. Answers 1.
// Other nodes (slot 0x19C and func_08968C38 true, a child count +0xF0 > 0): in mode 0 the node state+0x38 pushes its
// rectangle onto the vector state+0x3C (count state+0x48), and a clipping node (slot 0x198) pushes its rectangle with
// func_08A15DA8; each child (+0xF4, then func_08968A38) is collected at its offset, and the pushes are undone after
// (func_08A155A4, the vector's end and count). Answers whether a child answered 1.
void func_08A15F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = ctx.gpr[4];
    const std::uint32_t entries = ctx.gpr[5];
    const std::uint32_t node = ctx.gpr[6];
    const std::uint32_t index = ctx.gpr[7];
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    GuestFrame frame(ctx, 0xC0u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto answer = [&](std::uint32_t value) { ctx.gpr[2] = value; };
    if (call_set(rt, ctx, 0x089660F4u, {node}) == 0u) return answer(0u);
    if (call_set(rt, ctx, 0x08968C84u, {node}) != 0u) {
        (void)call_set(rt, ctx, 0x0896A35Cu, {node, 0u, 0u, frame.at(0u), frame.at(4u)});
        float right = x + f(frame.at(0u));
        float bottom = y + f(frame.at(4u));
        const std::uint32_t a = frame.at(0x28u), b = frame.at(0x38u), clip = frame.at(0x48u);
        const auto offset = [&](std::uint32_t rect) {
            put_float(rt, rect, x + f(rect));
            put_float(rt, rect + 4u, y + f(rect + 4u));
            put_float(rt, rect + 8u, x + f(rect + 8u));
            put_float(rt, rect + 0xCu, y + f(rect + 0xCu));
        };
        (void)call_set(rt, ctx, 0x0896B260u, {a, node});
        offset(a);
        (void)call_set(rt, ctx, 0x0896B410u, {b, node});
        offset(b);
        for (std::uint32_t k = 0u; k != 0x10u; k += 4u) put_float(rt, clip + k, 0.0f);
        bool add = true;
        if (call_set(rt, ctx, 0x08A15534u, {state, clip}) != 0u) {
            const float cx0 = f(clip), cy0 = f(clip + 4u), cx1 = f(clip + 8u), cy1 = f(clip + 0xCu);
            float left = cx0 < x ? x : cx0;
            float top = cy0 < y ? y : cy0;
            if (cx1 < right) right = cx1;
            if (cy1 < bottom) bottom = cy1;
            if (right <= left || bottom <= top) {
                right = left;
                bottom = top;
            }
            // a rectangle clipped the same way, collapsing to its corner when empty
            const auto clip_rect = [&](std::uint32_t rect) {
                float x0 = cx0 < f(rect) ? f(rect) : cx0;
                float y0 = cy0 < f(rect + 4u) ? f(rect + 4u) : cy0;
                float x1 = cx1 < f(rect + 8u) ? cx1 : f(rect + 8u);
                float y1 = cy1 < f(rect + 0xCu) ? cy1 : f(rect + 0xCu);
                if (x1 <= x0 || y1 <= y0) {
                    x1 = x0;
                    y1 = y0;
                }
                put_float(rt, rect, x0);
                put_float(rt, rect + 4u, y0);
                put_float(rt, rect + 8u, x1);
                put_float(rt, rect + 0xCu, y1);
            };
            clip_rect(a);
            clip_rect(b);
            add = 0.0f < right - left && 0.0f < bottom - top;
        }
        if (add) {
            (void)call_set(rt, ctx, 0x08A154E4u, {state, a, b});
            const std::uint32_t entry = frame.at(0x58u);
            put(rt, entry, node);
            put(rt, entry + 4u, word(rt, state + 0x48u));
            for (std::uint32_t k = 0u; k != 0x10u; k += 4u) put(rt, entry + 8u + k, word(rt, a + k));
            for (std::uint32_t k = 0u; k != 0x10u; k += 4u) put(rt, entry + 0x18u + k, word(rt, b + k));
            put(rt, entry + 0x28u, index);
            const std::uint32_t end = word(rt, entries + 4u);
            if (end != word(rt, entries + 8u)) {
                if (end != 0u) copy_entry(rt, end, entry);
                put(rt, entries + 4u, word(rt, entries + 4u) + 0x2Cu);
            } else {
                (void)call_set(rt, ctx, 0x08C160CCu, {entries, end, entry});
            }
            if (node == word(rt, state + 0x34u)) {
                for (std::uint32_t k = 0u; k != 0x10u; k += 4u) put(rt, state + 0x14u + k, word(rt, a + k));
                for (std::uint32_t k = 0u; k != 0x10u; k += 4u) put(rt, state + 0x24u + k, word(rt, b + k));
            }
            return answer(1u);
        }
    }
    const auto clips = [&] { return call_set(rt, ctx, word(rt, word(rt, node) + 0x198u), {node}) != 0u; };
    if (call_set(rt, ctx, word(rt, word(rt, node) + 0x19Cu), {node}) == 0u) return answer(0u);
    if (call_set(rt, ctx, 0x08968C38u, {node}) == 0u) return answer(0u);
    if (signed_of(word(rt, node + 0xF0u)) <= 0) return answer(0u);
    if (word(rt, state) == 0u) {
        if (node == word(rt, state + 0x38u)) {
            (void)call_set(rt, ctx, 0x0896A35Cu, {node, 0u, 0u, frame.at(4u), frame.at(0u)});
            const float right = x + f(frame.at(4u));
            const float bottom = y + f(frame.at(0u));
            const std::uint32_t rect = frame.at(0x18u);
            put_float(rt, rect, x);
            put_float(rt, rect + 4u, y);
            put_float(rt, rect + 8u, right);
            put_float(rt, rect + 0xCu, bottom);
            const std::uint32_t list = state + 0x3Cu;
            const std::uint32_t end = word(rt, list + 4u);
            if (end != word(rt, list + 8u)) {
                if (end != 0u) {
                    put_float(rt, end + 0xCu, bottom);
                    put_float(rt, end + 8u, right);
                    put_float(rt, end, x);
                    put_float(rt, end + 4u, y);
                }
                put(rt, list + 4u, word(rt, list + 4u) + 0x10u);
            } else {
                (void)call_set(rt, ctx, 0x08C16984u, {list, end, rect});
            }
            put(rt, state + 0x48u, word(rt, state + 0x48u) + 1u);
        }
        if (clips()) {
            (void)call_set(rt, ctx, 0x0896A35Cu, {node, 0u, 0u, frame.at(0u), frame.at(4u)});
            const std::uint32_t rect = frame.at(8u);
            put_float(rt, rect, x);
            put_float(rt, rect + 4u, y);
            put_float(rt, rect + 8u, x + f(frame.at(0u)));
            put_float(rt, rect + 0xCu, y + f(frame.at(4u)));
            (void)call_set(rt, ctx, 0x08A15DA8u, {state, rect});
        }
    }
    std::uint32_t found = 0u;
    std::uint32_t position = 0u;
    for (std::uint32_t child = word(rt, node + 0xF4u); child != 0u; ++position) {
        (void)call_set(rt, ctx, 0x0896A35Cu, {child, frame.at(4u), 0u, 0u, 0u});
        const float child_x = f(frame.at(4u)) + x;
        (void)call_set(rt, ctx, 0x0896A35Cu, {child, 0u, frame.at(4u), 0u, 0u});
        const float child_y = f(frame.at(4u)) + y;
        if (call_setf(rt, ctx, 0x08A15F08u, {state, entries, child, position}, {child_x, child_y}).v0 != 0u) found = 1u;
        child = call_set(rt, ctx, 0x08968A38u, {child});
    }
    if (word(rt, state) != 0u) return answer(found);
    if (clips()) (void)call_set(rt, ctx, 0x08A155A4u, {state});
    if (node == word(rt, state + 0x38u)) {
        put(rt, state + 0x40u, word(rt, state + 0x40u) - 0x10u);
        put(rt, state + 0x48u, word(rt, state + 0x48u) - 1u);
    }
    answer(found);
}

// func_0894C460(engine, spec, power, extra): fills an engine from its spec. engine+0xCE = 10 * (spec+0x120, else the
// byte +0xBC); engine+0xCC = 100 * (the byte +0xBD, else the last rpm byte), at least 100 * byte +0xBE + 200; an empty
// last torque entry takes the one before it. The n = byte +0xBF rpm bytes (+0xC0) become rad/s at engine+0xC
// (100 rpm * pi / 30). Torque at engine+0x6C: the spec's halfwords +0xD8 (below -1 read as unsigned) * 9.8 * scale,
// the scale a line from f21 to f21 + f22 over the rpm range, through the curve func_08960BE8 (made by func_08960A48
// from byte +0xC and D_08CDE094) when the signed byte +0xB is set (f21, f22 from it and D_08CDE096 / D_08CDE097,
// floored at the minimum D_08CDE095 %), else from +0x10C / +0x10A (0.01 / 0.001); times +0x108 * 1e-5 (else 0.01)
// and +0x7 % (unless 0 or 100). Then func_08946734(engine, n, rpm, torque, 0, 0), engine+0xD0 = +0xBA * 0.98 (* 0.9
// with D_08CDE690, else / the top rpm), func_0894C294(spec, engine, extra), each torque entry scaled by
// func_0894C240 / func_0894C178 at its rpm, and a positive `power` gives engine+0xDB = 1 and the limits +0xE0 / +0xDC.
void func_0894C460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t engine = ctx.gpr[4];
    const std::uint32_t spec = ctx.gpr[5];
    const std::int32_t power = signed_of(ctx.gpr[6]);
    GuestFrame frame(ctx, 0x70u);
    put(rt, frame.at(0x10u), ctx.gpr[7]);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto constant = [](std::uint32_t bits) { return std::bit_cast<float>(bits); };
    const auto lh = [&](std::uint32_t address) { return signed_of(sext16(half_u(rt, address))); };
    const std::int32_t limit = lh(spec + 0x120u);
    put16(rt, engine + 0xCEu, static_cast<std::uint32_t>((limit != 0 ? limit : static_cast<std::int32_t>(byte_u(rt, spec + 0xBCu))) * 10));
    const std::uint32_t top = byte_u(rt, spec + 0xBDu);
    put16(rt, engine + 0xCCu, (top != 0u ? top : byte_u(rt, spec + 0xBFu + byte_u(rt, spec + 0xBFu))) * 100u);
    const std::int32_t least = static_cast<std::int32_t>(byte_u(rt, spec + 0xBEu) * 100u) + 0xC8;
    if (static_cast<std::int32_t>(half_u(rt, engine + 0xCCu)) < least) put16(rt, engine + 0xCCu, static_cast<std::uint32_t>(least));
    const std::uint32_t n = byte_u(rt, spec + 0xBFu);
    const std::uint32_t last_torque = spec + (n - 1u) * 2u + 0xD8u;
    if (lh(last_torque) == 0) put16(rt, last_torque, half_u(rt, spec + n * 2u + 0xD4u));
    for (std::uint32_t i = 0u; static_cast<std::int32_t>(i) < static_cast<std::int32_t>(n); ++i) {
        put_float(rt, engine + 0xCu + 4u * i, psp_mul(static_cast<float>(static_cast<std::int32_t>(byte_u(rt, spec + 0xC0u + i) * 100u)), constant(0x3DD67750u)));
    }
    const std::int32_t shape = signed_of(sext8(byte_u(rt, spec + 0xBu)));
    float base, span;
    if (shape != 0) {
        const float amount = static_cast<float>(shape) / 100.0f;
        const float low = static_cast<float>(static_cast<std::int32_t>(byte_u(rt, 0x08CDE096u))) / -100.0f;
        const float high = static_cast<float>(static_cast<std::int32_t>(byte_u(rt, 0x08CDE097u))) / 100.0f;
        base = psp_mul(amount, low) + 1.0f;
        span = psp_mul(amount, high) + 1.0f;
    } else {
        base = 1.0f;
        const std::uint32_t low = half_u(rt, spec + 0x10Cu);
        if (low != 0u) base = psp_mul(static_cast<float>(static_cast<std::int32_t>(low)), constant(0x3C23D70Au));
        span = 1.0f;
        const std::uint32_t high = half_u(rt, spec + 0x10Au);
        if (high != 0u) span = psp_mul(static_cast<float>(static_cast<std::int32_t>(high)), constant(0x3A83126Fu));
    }
    float rpm_top = f(engine + 8u + 4u * n);
    const float rpm_low = f(engine + 0xCu);
    float scale = constant(0x3C23D70Au);
    const std::uint32_t factor = half_u(rt, spec + 0x108u);
    if (factor != 0u) scale = psp_mul(psp_mul(static_cast<float>(static_cast<std::int32_t>(factor)), constant(0x3A83126Fu)), scale);
    const std::uint32_t percent = byte_u(rt, spec + 7u);
    if (percent != 0u && percent != 0x64u) scale = psp_mul(scale, static_cast<float>(static_cast<std::int32_t>(percent)) / 100.0f);
    const float floor = psp_mul(scale, static_cast<float>(static_cast<std::int32_t>(byte_u(rt, 0x08CDE095u))) / 100.0f);
    const std::uint32_t curve = byte_u(rt, spec + 0xCu);
    (void)call_set(rt, ctx, 0x08960A48u, {frame.at(0u), curve < 3u ? curve : 0u, byte_u(rt, 0x08CDE094u)});
    if (static_cast<std::int32_t>(n) > 0) {
        rpm_top = rpm_top - rpm_low;
        span = span - base;
        for (std::uint32_t i = 0u; i != n; ++i) {
            const float rpm = f(engine + 0xCu + 4u * i);
            float torque;
            if (shape != 0) {
                const float at = call_setf(rt, ctx, 0x08960BE8u, {frame.at(0u), 0u}, {(rpm - rpm_low) / rpm_top}).f0;
                torque = psp_mul(scale, base + psp_mul(at, span));
                if (torque < floor) torque = floor;
            } else {
                torque = psp_mul(scale, base + psp_mul(span, rpm - rpm_low) / rpm_top);
            }
            std::int32_t raw = lh(spec + 0xD8u + 2u * i);
            if (raw < -1) raw += 0x10000;
            put_float(rt, engine + 0x6Cu + 4u * i, psp_mul(torque, psp_mul(static_cast<float>(raw), constant(0x411CCCCDu))));
        }
    }
    (void)call_set(rt, ctx, 0x08946734u, {engine, byte_u(rt, spec + 0xBFu), engine + 0xCu, engine + 0x6Cu, 0u, 0u});
    const float force = psp_mul(psp_mul(static_cast<float>(static_cast<std::int32_t>(half_u(rt, spec + 0xBAu))), constant(0x3DCCCCCDu)), constant(0x411CCCCDu));
    const float top_rpm = f(engine + 8u + 4u * byte_u(rt, spec + 0xBFu));
    put_float(rt, engine + 0xD0u, byte_u(rt, 0x08CDE690u) != 0u ? psp_mul(force, constant(0x3F666666u)) : force / top_rpm);
    (void)call_set(rt, ctx, 0x0894C294u, {spec, engine, word(rt, frame.at(0x10u))});
    // v0 after the loop: its last test (0), or the count when it is not positive
    ctx.gpr[2] = static_cast<std::uint32_t>(lh(engine));
    for (std::int32_t i = 0; i < lh(engine);) {
        const std::uint32_t rpm = sext16(byte_u(rt, spec + 0xC0u + static_cast<std::uint32_t>(i)) * 100u);
        const float numerator = call_setf(rt, ctx, 0x0894C240u, {engine, rpm, rpm, frame.at(0u)}, {}).f0;
        const float denominator = call_setf(rt, ctx, 0x0894C178u, {engine, rpm, rpm, frame.at(0u)}, {}).f0;
        const std::uint32_t slot = engine + 0x6Cu + 4u * static_cast<std::uint32_t>(i);
        put_float(rt, slot, psp_mul(numerator / denominator, f(slot)));
        ++i;
        ctx.gpr[2] = i < lh(engine) ? 1u : 0u;
    }
    put8(rt, engine + 0xDBu, 0u);
    put(rt, engine + 0xDCu, 0u);
    if (power <= 0) return;
    put8(rt, engine + 0xDBu, 1u);
    float limit_force = psp_mul(static_cast<float>(power), constant(0x411CCCCDu));
    limit_force = psp_mul(limit_force, constant(0x44330CCDu));
    limit_force = psp_mul(limit_force, constant(0x40C90FDBu));
    limit_force = limit_force / 60.0f;
    const std::uint32_t raw_share = byte_u(rt, 0x08CDE510u);
    const float share = psp_mul(static_cast<float>(static_cast<std::int32_t>(raw_share)), constant(0x3C23D70Au));
    put_float(rt, engine + 0xE0u, share);
    put_float(rt, engine + 0xDCu, limit_force / (psp_mul(share, -0.5f) + 1.0f));
    ctx.gpr[2] = raw_share;
}

// func_089AC618(drawer, points, transform, style, f12 = count, f13 = width, f14 / f15 = end cap length / ratio,
// f16 / f17 = start cap length / ratio): draws a thick polyline. Slot 0x150 begins; a texture style+0 is bound
// (func_089E4BC4(texture, sp+0x18, 0), drawer+0x118 = 1, +0x11C = texture) and its binding passed to the quads. Nothing
// more for count 0. Point 0 and then each next point go through func_08B4AC54(out, point, transform) (the last one
// first moved to the fraction of count past its index), and each non-empty segment is drawn as a 4-vertex quad through
// slot 0x10C(drawer, D_08C8CC14, 4, quad, binding, 0), half the width (+-1, a pixel outward) across it. The first
// segment also builds a start cap and the last an end cap (when style+8 / +4 is set; a zero length means half the
// width), drawn after the line with their own textures. It ends with func_089A8C24(drawer) and slot 0x154.
void func_089AC618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t drawer = ctx.gpr[4];
    const std::uint32_t points = ctx.gpr[5];
    const std::uint32_t transform = ctx.gpr[6];
    const std::uint32_t style = ctx.gpr[7];
    const float count = ctx.fpr[12];
    const float width = ctx.fpr[13];
    float end_length = ctx.fpr[14];
    const float end_ratio = ctx.fpr[15];
    float start_length = ctx.fpr[16];
    const float start_ratio = ctx.fpr[17];
    GuestFrame frame(ctx, 0x110u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto slot = [&](std::uint32_t offset) { return word(rt, word(rt, drawer) + offset); };
    (void)call_virtual_at(rt, ctx, drawer, 0x150u, {drawer});
    std::uint32_t binding = 0u;
    if (word(rt, style) != 0u) {
        (void)call_set(rt, ctx, 0x089E4BC4u, {word(rt, style), sp(0x18u), 0u});
        put8(rt, drawer + 0x118u, 1u);
        binding = sp(0x18u);
        put(rt, drawer + 0x11Cu, word(rt, style));
    }
    if (count == 0.0f) {
        ctx.gpr[2] = 0u;  // the last of the inlined construction loops
        return;
    }
    std::int32_t segments = ctx.fpu_float_to_word(count, 1u);
    if (static_cast<float>(segments) != count || count == 1.0f) ++segments;
    const auto draw = [&](std::uint32_t quad, std::uint32_t texture) {
        ctx.gpr[3] = word(rt, drawer);  // `lw v1, 0(drawer); lw t2, 0x10C(v1); jalr t2`
        ctx.gpr[10] = slot(0x10Cu);
        (void)call_set(rt, ctx, ctx.gpr[10], {drawer, word(rt, 0x08C8CC14u), 4u, quad, texture, 0u});
    };
    if (segments > 0) {
        bool first = true;
        const float half = psp_mul(width, 0.5f);
        (void)call_set(rt, ctx, 0x08B4AC54u, {sp(0u), points, transform});
        const std::uint32_t quad = sp(0x78u);
        for (std::int32_t i = 1; i < segments; ++i) {
            const std::uint32_t point = points + 8u * static_cast<std::uint32_t>(i);
            (void)call_set(rt, ctx, 0x08B4AC54u, {sp(8u), point, transform});
            if (i == segments - 1) {
                const float t = count - static_cast<float>(i);
                const float x0 = f(point - 8u), y0 = f(point - 4u);
                put_float(rt, sp(0x14u), y0 + psp_mul(t, f(point + 4u) - y0));
                put_float(rt, sp(0x10u), x0 + psp_mul(t, f(point) - x0));
                (void)call_set(rt, ctx, 0x08B4AC54u, {sp(8u), sp(0x10u), transform});
            }
            const float px = f(sp(0u)), py = f(sp(4u)), cx = f(sp(8u)), cy = f(sp(0xCu));
            const float dx = cx - px;
            const float dy = cy - py;
            const float length = std::sqrt(psp_mul(dx, dx) + psp_mul(dy, dy));
            if (length == 0.0f) continue;
            const float scale = half / length;
            const float across_y = psp_mul(dy, scale);
            const float across_x = psp_mul(dx, scale);
            const float out = 0.0f <= across_y ? across_y + 1.0f : across_y - 1.0f;
            if (first) {
                put_float(rt, quad + 8u, px - out);
                put_float(rt, quad, out + px);
                put_float(rt, quad + 4u, py - across_x);
                put_float(rt, quad + 0xCu, across_x + py);
            }
            put_float(rt, quad + 0x18u, cx - out);
            put_float(rt, quad + 0x10u, out + cx);
            put_float(rt, quad + 0x14u, cy - across_x);
            put_float(rt, quad + 0x1Cu, across_x + cy);
            draw(quad, binding);
            if (first && word(rt, style + 8u) != 0u) {
                if (start_length == 0.0f) start_length = half;
                const float a = psp_mul(start_length, -(dx / length));
                const float b = psp_mul(start_length, -(dy / length));
                const float a2 = psp_mul(a, start_ratio);
                const float b2 = psp_mul(b, start_ratio);
                const float ax = a + px;
                const float by = b + py;
                const float cap[8] = {b2 + px, py - a2, px - b2, a2 + py, b2 + ax, by - a2, ax - b2, a2 + by};
                for (std::uint32_t k = 0u; k != 8u; ++k) put_float(rt, sp(0x58u + 4u * k), cap[k]);
            }
            if (i == segments - 1 && word(rt, style + 4u) != 0u) {
                if (end_length == 0.0f) end_length = half;
                const float b = psp_mul(end_length, dy / length);
                const float a = psp_mul(end_length, dx / length);
                const float by = b + cy;
                const float ax = a + cx;
                const float b2 = psp_mul(b, end_ratio);
                const float a2 = psp_mul(a, end_ratio);
                const float cap[8] = {b2 + cx, cy - a2, cx - b2, a2 + cy, b2 + ax, by - a2, ax - b2, a2 + by};
                for (std::uint32_t k = 0u; k != 8u; ++k) put_float(rt, sp(0x38u + 4u * k), cap[k]);
            }
            first = false;
            put_float(rt, sp(0u), cx);
            put_float(rt, sp(4u), cy);
            for (std::uint32_t k = 0u; k != 0x10u; k += 4u) put(rt, quad + k, word(rt, quad + 0x10u + k));
        }
    }
    // the caps: style+8 at sp+0x58, then style+4 at sp+0x38, each with its own texture binding at sp+0x18
    for (const auto &[texture_offset, cap] : {std::pair{8u, 0x58u}, std::pair{4u, 0x38u}}) {
        if (word(rt, style + texture_offset) == 0u) continue;
        put8(rt, drawer + 0x118u, 1u);
        const std::uint32_t texture = word(rt, style + texture_offset);
        if (texture == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50A4u});
        (void)call_set(rt, ctx, 0x089E4BC4u, {texture, sp(0x18u), 0u});
        put(rt, drawer + 0x11Cu, word(rt, style + texture_offset));
        draw(sp(cap), sp(0x18u));
    }
    (void)call_set(rt, ctx, 0x089A8C24u, {drawer});
    (void)call_virtual_at(rt, ctx, drawer, 0x154u, {drawer});  // v0 = the slot's address when it leaves v0 alone
}

} // namespace hot_batch_126
} // namespace gt::game

#define HOT126(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_126::name, gt::ReturnKind::kind)
HOT126(0x08A15F08u, func_08A15F08, Word);
// HOT126(0x0894C460u, func_0894C460, Word);  // batch 126: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT126(0x089AC618u, func_089AC618, Word);
