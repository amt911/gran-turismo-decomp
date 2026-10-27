// Hot batch 133: functions of 500+ instructions the 11-route gate reaches, written by hand from `tools.disasm`;
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
namespace hot_batch_133 {
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

// A script class's method table: func_08B86FB0(*holder, name, method) for each pair, *holder read again each time (an
// empty one first reported through func_08A5BB7C(report)); with `tail` the last registration is the function's tail jump.
inline void register_methods(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder,
                             std::uint32_t report, const std::uint32_t (*methods)[2], std::size_t count, bool tail) {
    for (std::size_t index = 0u; index < count; ++index) {
        const std::uint32_t object = word(rt, holder);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {report});
        if (tail && index + 1u == count) return tail_call(ctx, 0x08B86FB0u, {object, methods[index][0], methods[index][1]});
        (void)call_set(rt, ctx, 0x08B86FB0u, {object, methods[index][0], methods[index][1]});
    }
}

// The original's unsigned-to-float conversion: halves a value with the top bit set (keeping the low bit) and doubles
// the converted result.
inline float unsigned_to_float(std::uint32_t value) {
    if (signed_of(value) >= 0) return static_cast<float>(signed_of(value));
    const float half = static_cast<float>(signed_of((value >> 1) | (value & 1u)));
    return half + half;
}


// func_0892E51C's section parsers scale profile values by percentages with the compiler's multiply-high divisions: the
// signed / 100 (0x51EB851F, >> 5, minus the sign), the signed / 1000 (0x10624DD3, >> 6) and the unsigned / 100 of a
// non-negative product (the high word shifted right logically).
inline std::uint32_t high_word(std::uint32_t value, std::uint32_t magic) {
    return static_cast<std::uint32_t>((static_cast<std::int64_t>(signed_of(value)) * signed_of(magic)) >> 32);
}
// `sra t, product, 31; subu v, v, t`: the sign word (0 or -1) is subtracted, so a negative quotient gets +1 (truncation
// toward zero). It was subtracted as a 0 / 1 bit, 2 off for every negative product (synthetic cases, 2026-10-27).
inline std::uint32_t signed_percent(std::uint32_t product) {
    return static_cast<std::uint32_t>(signed_of(high_word(product, 0x51EB851Fu)) >> 5) -
           static_cast<std::uint32_t>(signed_of(product) >> 31);
}
inline std::uint32_t signed_permille(std::uint32_t product) {
    return static_cast<std::uint32_t>(signed_of(high_word(product, 0x10624DD3u)) >> 6) -
           static_cast<std::uint32_t>(signed_of(product) >> 31);
}
inline std::uint32_t unsigned_percent(std::uint32_t product) { return high_word(product, 0x51EB851Fu) >> 5; }

// The two limits at out+0x10A / +0x10C scaled by the section's halfwords at `first` and `first` + 2 (percent).
inline void scale_limits(psprecomp::Runtime &rt, std::uint32_t out, std::uint32_t buffer, std::uint32_t first) {
    put16(rt, out + 0x10Au, signed_percent(half_u(rt, out + 0x10Au) * half_u(rt, buffer + first)));
    put16(rt, out + 0x10Cu, signed_percent(half_u(rt, out + 0x10Cu) * half_u(rt, buffer + first + 2u)));
}

// out+0xBA / +0xB8 scaled by the section's bytes +4 / +3 (unsigned percent), the signed out+0x174 / +0x176 by its bytes
// +5 / +6 (percent).
inline void scale_handling(psprecomp::Runtime &rt, std::uint32_t out, std::uint32_t buffer) {
    put16(rt, out + 0xBAu, unsigned_percent(half_u(rt, out + 0xBAu) * byte_u(rt, buffer + 4u)));
    put16(rt, out + 0xB8u, unsigned_percent(half_u(rt, out + 0xB8u) * byte_u(rt, buffer + 3u)));
    put16(rt, out + 0x174u, signed_percent(sext16(half_u(rt, out + 0x174u)) * byte_u(rt, buffer + 5u)));
    put16(rt, out + 0x176u, signed_percent(sext16(half_u(rt, out + 0x176u)) * byte_u(rt, buffer + 6u)));
}

// The sixteen levels out+0xC0..+0xCF raised by the section's byte `step`, out+0xBD by its byte `bonus`, then the limits
// scaled by its halfwords at `first`.
inline void raise_levels(psprecomp::Runtime &rt, std::uint32_t out, std::uint32_t buffer, std::uint32_t step, std::uint32_t bonus,
                         std::uint32_t first) {
    for (std::uint32_t index = 0u; index < 16u; ++index)
        put8(rt, out + 0xC0u + index, byte_u(rt, out + 0xC0u + index) + byte_u(rt, buffer + step));
    put8(rt, out + 0xBDu, byte_u(rt, out + 0xBDu) + byte_u(rt, buffer + bonus));
    scale_limits(rt, out, buffer, first);
}

// The shape of func_0892E12C / func_0892E2A4: the section holds six more 64-bit offsets (+8..+0x34, copied to the frame);
// three times a pair of sub-sections is loaded (frame +0x48 and +4) and func_0892DD3C(source, block, first, second)
// fills the next 0xC0-byte block of out from `base`; then the section at the offset +0 is loaded to the frame and four
// bytes go out: its byte +2, byte +3, the low byte of its halfword +0, and the constant 4.
inline void load_blocks(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t source,
                        std::uint32_t out, std::uint32_t buffer, std::uint32_t base, const std::uint32_t (&targets)[4]) {
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    for (std::uint32_t index = 0u; index < 12u; ++index) put(rt, sp(0x18u + index * 4u), word(rt, buffer + 8u + index * 4u));
    const auto load = [&](std::uint32_t offsets, std::uint32_t into) {
        const std::uint32_t handle = call_set(rt, ctx, 0x089363FCu, {});
        (void)call_set(rt, ctx, 0x08936584u, {handle, ctx.gpr[5], word(rt, offsets), word(rt, offsets + 4u), into});
    };
    for (std::uint32_t block = 0u; block < 3u; ++block) {
        load(sp(0x18u + block * 8u), sp(0x48u));
        load(sp(0x30u + block * 8u), sp(4u));
        (void)call_set(rt, ctx, 0x0892DD3Cu, {source, out + base + block * 0xC0u, sp(0x48u), sp(4u)});
    }
    load(buffer, sp(0u));
    put8(rt, out + targets[0], byte_u(rt, sp(2u)));
    put8(rt, out + targets[1], byte_u(rt, sp(3u)));
    put8(rt, out + targets[2], half_u(rt, sp(0u)));
    put8(rt, out + targets[3], 4u);
}

} // namespace

// func_08831648(-, event) -> 1 when the event was sent: hands a game event (type at event+0, fields from +8) to the
// script side. The target and source objects are the live ones behind 0x08CE2B60 / 0x08CE2B5C (read under the lock
// 0x08CE2B64; none -> 0). An argument array of 8 values (func_08A98C1C(array, 8)) is filled per type by the jump table at
// 0x08C87FF0: each field becomes a script value in its own frame slot (bytes func_08A97A6C, words func_08A85B1C, 64-bit
// func_08A5DEE8 (signed) / func_08A83160 (unsigned high 0), floats func_08A87EEC(value, f12), a string from +0x18 by
// func_08B85EB8 + func_08A6D2D4), assigned into the next element (func_08A5BAD8, skipped when the same) and destroyed;
// then func_0882D4BC(target, source, event name, array) sends it. Types 30-32 and 40+ send nothing (0). The array is
// destroyed last.
void func_08831648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t event = ctx.gpr[5];
    GuestFrame frame(ctx, 0x180u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    constexpr std::uint32_t kGuard = 0x08CE2B64u;
    const auto live = [&](std::uint32_t global) {
        (void)call_set(rt, ctx, kLock, {kGuard});
        const std::uint32_t object = word(rt, global);
        const std::uint32_t result = (object != 0u && word(rt, object + 4u) != 0u) ? object : 0u;
        (void)call_set(rt, ctx, kUnlock, {kGuard});
        return result;
    };
    ctx.gpr[2] = 0u;
    const std::uint32_t target = live(0x08CE2B60u);
    if (target == 0u) return;
    const std::uint32_t source = live(0x08CE2B5Cu);
    if (source == 0u) return;
    const std::uint32_t args = sp(0x130u);
    (void)call_set(rt, ctx, 0x08A98C1Cu, {args, 8u});
    std::uint32_t next = 0u;
    // The converted value in `temp` goes into the next element of the array.
    const auto store = [&](std::uint32_t temp) {
        const std::uint32_t element = word(rt, word(rt, args) + 0x14u) + next * 4u;
        ++next;
        if (element != temp) (void)call_set(rt, ctx, kAssign, {element, word(rt, temp)});
        (void)call_set(rt, ctx, kDestroy, {temp});
    };
    const auto field = [&](std::uint32_t offset) { return word(rt, event + offset); };
    const auto byte_value = [&](std::uint32_t temp, std::uint32_t offset) {
        (void)call_set(rt, ctx, 0x08A97A6Cu, {sp(temp), byte_u(rt, event + offset)});
        store(sp(temp));
    };
    const auto word_value = [&](std::uint32_t temp, std::uint32_t offset) {
        (void)call_set(rt, ctx, 0x08A85B1Cu, {sp(temp), field(offset)});
        store(sp(temp));
    };
    const auto long_value = [&](std::uint32_t temp, std::uint32_t low, std::uint32_t high) {
        (void)call_set(rt, ctx, 0x08A5DEE8u, {sp(temp), ctx.gpr[5], low, high});
        store(sp(temp));
    };
    const auto ulong_value = [&](std::uint32_t temp, std::uint32_t offset) {
        (void)call_set(rt, ctx, 0x08A83160u, {sp(temp), ctx.gpr[5], field(offset), 0u});
        store(sp(temp));
    };
    const auto float_value = [&](std::uint32_t temp, float value) {
        (void)call_setf(rt, ctx, 0x08A87EECu, {sp(temp)}, {value});
        store(sp(temp));
    };
    const auto string_value = [&](std::uint32_t text, std::uint32_t temp, std::uint32_t tag, std::uint32_t release_frame) {
        (void)call_set(rt, ctx, 0x08B85EB8u, {sp(text), event + 0x18u, sp(tag)});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {sp(temp), sp(text)});
        store(sp(temp));
        release_string(rt, ctx, sp(text), 0x08D74FD0u, 0x08B8542Cu, sp(release_frame));
    };
    const auto sext = [](std::uint32_t value) { return static_cast<std::uint32_t>(signed_of(value) >> 31); };
    std::uint32_t name = 0u;
    switch (field(0u) < 40u ? field(0u) : 30u) {
    case 0: byte_value(0x12Cu, 8u); byte_value(0x128u, 9u); name = 0x08C9B200u; break;
    case 1: name = 0x08C9B20Cu; break;
    case 2:
        word_value(0x124u, 8u); byte_value(0x120u, 0xCu); long_value(0x11Cu, field(0x10u), field(0x14u));
        string_value(0x114u, 0x118u, 3u, 1u); name = 0x08C9B220u; break;
    case 3:
        word_value(0x110u, 8u); byte_value(0x10Cu, 0xCu); long_value(0x108u, field(0x10u), field(0x14u));
        string_value(0x100u, 0x104u, 2u, 0u); name = 0x08C9B22Cu; break;
    case 4: name = 0x08C9B238u; break;
    case 5: name = 0x08C9B248u; break;
    case 6: float_value(0xFCu, unsigned_to_float(field(8u)) / std::bit_cast<float>(0x447A0000u)); name = 0x08C9B258u; break;
    case 7: word_value(0xF8u, 8u); name = 0x08C9B26Cu; break;
    case 8: word_value(0xF4u, 8u); name = 0x08C9B278u; break;
    case 9: word_value(0xECu, 8u); name = 0x08C9B298u; break;
    case 10: long_value(0xF0u, field(8u), field(0xCu)); name = 0x08C9B28Cu; break;
    case 11: byte_value(0xE8u, 8u); word_value(0xE4u, 0xCu); name = 0x08C9B2A8u; break;
    case 12: word_value(0xE0u, 8u); word_value(0xDCu, 0x10u); word_value(0xD8u, 0xCu); byte_value(0xD4u, 0x14u); name = 0x08C9B2B8u; break;
    case 13: word_value(0xD0u, 8u); word_value(0xCCu, 0xCu); byte_value(0xC8u, 0x10u); byte_value(0xC4u, 0x11u); name = 0x08C9B2C4u; break;
    case 14: word_value(0xC0u, 8u); word_value(0xBCu, 0xCu); word_value(0xB8u, 0x10u); name = 0x08C9B2D0u; break;
    case 15: word_value(0xB4u, 8u); word_value(0xB0u, 0xCu); name = 0x08C9B2DCu; break;
    case 16: word_value(0xACu, 8u); name = 0x08C9B2F0u; break;
    case 17: word_value(0xA8u, 8u); name = 0x08C9B304u; break;
    case 18: word_value(0xA4u, 8u); word_value(0xA0u, 0xCu); word_value(0x9Cu, 0x10u); word_value(0x98u, 0x14u); name = 0x08C9B31Cu; break;
    case 19: {
        const float first = float_at(rt, event + 0x14u);
        const float second = float_at(rt, event + 0x18u);
        word_value(0x94u, 8u); word_value(0x90u, 0xCu); word_value(0x8Cu, 0x10u);
        float_value(0x88u, first); float_value(0x84u, second); name = 0x08C9B32Cu; break;
    }
    case 20: word_value(0x80u, 8u); word_value(0x7Cu, 0xCu); name = 0x08C9B340u; break;
    case 21: ulong_value(0x78u, 8u); ulong_value(0x74u, 0xCu); name = 0x08C9B35Cu; break;
    case 22: ulong_value(0x70u, 8u); ulong_value(0x6Cu, 0xCu); name = 0x08C9B368u; break;
    case 23: word_value(0x68u, 0x10u); word_value(0x64u, 8u); word_value(0x60u, 0xCu); name = 0x08C9B378u; break;
    case 24: word_value(0x5Cu, 8u); word_value(0x58u, 0xCu); name = 0x08C9B390u; break;
    case 25: name = 0x08C9B3A4u; break;
    case 26: name = 0x08C9B41Cu; break;
    case 27: name = 0x08C9B3B8u; break;
    case 28: word_value(0x54u, 8u); word_value(0x50u, 0xCu); name = 0x08C9B3CCu; break;
    case 29: word_value(0x18u, 8u); word_value(0x14u, 0xCu); word_value(0x10u, 0x10u); name = 0x08C9B440u; break;
    case 33: word_value(0x34u, 8u); name = 0x08C9B3ECu; break;
    case 34: word_value(0x30u, 8u); name = 0x08C9B3F4u; break;
    case 35: word_value(0x2Cu, 8u); name = 0x08C9B400u; break;
    case 36:
        word_value(0x4Cu, 8u); word_value(0x48u, 0xCu); word_value(0x44u, 0x10u); word_value(0x40u, 0x14u);
        word_value(0x3Cu, 0x18u); word_value(0x38u, 0x1Cu); name = 0x08C9B3DCu; break;
    case 37: word_value(0x28u, 8u); word_value(0x24u, 0xCu); byte_value(0x20u, 0x10u); byte_value(0x1Cu, 0x11u); name = 0x08C9B40Cu; break;
    case 38:
        long_value(0xCu, field(8u), field(0xCu)); long_value(8u, field(0x10u), sext(field(0x10u)));
        long_value(4u, field(0x14u), sext(field(0x14u))); name = 0x08C9B450u; break;
    case 39: name = 0x08C9B430u; break;
    default: break;  // 30-32 (and 40+): nothing to send
    }
    std::uint32_t sent = 0u;
    if (name != 0u) {
        (void)call_set(rt, ctx, 0x0882D4BCu, {target, source, name, args});
        sent = 1u;
    }
    (void)call_set(rt, ctx, kDestroy, {args});
    ctx.gpr[2] = sent;
}

// func_088B859C(from, context, to, map, out): adds to the list `out` (func_08BB5D50(out, &entry)) the nodes of `map` a
// path from node key `from` may go to next. Node keys are 4 bytes {group, index, angle} looked up by func_088B6698(map,
// &key) (a node starts with its x, y floats); groups are 0x48-byte records from map+0xA4 (count map+0x188; +0 the
// first node's x, y, +0x40 its node count, nodes 8 bytes apart). When func_088B69AC(map, from node, to node) says the
// two are not linked, the single entry {0xFF, 1, angle from `from` to `to`} goes out (angles by func_088B5E24(&delta)),
// after func_088B6C1C(map, context, from, to node) agrees when `from` is in a group. Otherwise, within the margins
// x in [-0.1 - map+0x84, map+0x80 + 0.1] and above `from`'s y: for `from`'s own group its previous and next node
// (wrapping; func_088B6908 must refuse the link and func_088B6C1C agree), for every other group the two nodes that reach
// furthest either way across the line to the group's middle (func_088B69AC refusing the link, func_088B6C1C agreeing).
// A free `from` (group 0xFF) takes the extremes of every group, kept only when their angle is within 0x71B of its own.
void func_088B859C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t from = ctx.gpr[4];
    const std::uint32_t context = ctx.gpr[5];
    const std::uint32_t to = ctx.gpr[6];
    const std::uint32_t map = ctx.gpr[7];
    const std::uint32_t out = ctx.gpr[8];
    GuestFrame frame(ctx, 0x100u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    put(rt, sp(0x98u), context);
    put(rt, sp(0x9Cu), out);
    const auto key = [&](std::uint32_t at, std::uint32_t group, std::uint32_t index, std::uint32_t angle) {
        put8(rt, sp(at), group);
        put8(rt, sp(at + 1u), index);
        put16(rt, sp(at + 2u), angle);
    };
    const auto find = [&](std::uint32_t at) { return call_set(rt, ctx, 0x088B6698u, {map, sp(at)}); };
    const auto x = [&](std::uint32_t node) { return float_at(rt, node); };
    const auto y = [&](std::uint32_t node) { return float_at(rt, node + 4u); };
    // The angle from `origin` to `target` (their difference written at frame `at`).
    const auto angle = [&](std::uint32_t at, std::uint32_t target, std::uint32_t origin) {
        put_float(rt, sp(at), x(target) - x(origin));
        put_float(rt, sp(at + 4u), y(target) - y(origin));
        return call_set(rt, ctx, 0x088B5E24u, {sp(at)});
    };
    const auto emit = [&](std::uint32_t at) { (void)call_set(rt, ctx, 0x08BB5D50u, {out, sp(at)}); };
    const std::uint32_t group0 = byte_u(rt, from);

    key(0x3Cu, byte_u(rt, from), byte_u(rt, from + 1u), half_u(rt, from + 2u));
    const std::uint32_t from_node = find(0x3Cu);
    key(0x38u, byte_u(rt, to), byte_u(rt, to + 1u), half_u(rt, to + 2u));
    const std::uint32_t to_node = find(0x38u);
    if (call_set(rt, ctx, 0x088B69ACu, {map, from_node, to_node}) == 0u) {
        const std::uint32_t to_group = byte_u(rt, to), to_index = byte_u(rt, to + 1u), to_angle = half_u(rt, to + 2u);
        if (group0 != 0xFFu) {
            key(0x34u, to_group, to_index, to_angle);
            const std::uint32_t node = find(0x34u);
            if (call_set(rt, ctx, 0x088B6C1Cu, {map, context, from, node}) == 0u) return;
        }
        key(0x2Cu, to_group, to_index, to_angle);
        const std::uint32_t target = find(0x2Cu);
        key(0x28u, group0, byte_u(rt, from + 1u), half_u(rt, from + 2u));
        const std::uint32_t origin = find(0x28u);
        const std::uint32_t direction = angle(0x78u, target, origin);
        key(0x30u, 0xFFu, 1u, direction);
        emit(0x30u);
        return;
    }
    const float low = std::bit_cast<float>(0xBDCCCCCDu) - float_at(rt, map + 0x84u);   // -0.1 - margin
    const float high = float_at(rt, map + 0x80u) + std::bit_cast<float>(0x3DCCCCCDu);  // margin + 0.1
    const std::uint32_t groups = map + 0xA4u;
    const auto outside = [&](std::uint32_t node, float floor) { return y(node) <= floor || x(node) < low || high < x(node); };
    // The middle of a group: halfway between its first node and its node count / 2.
    const auto middle = [&](std::uint32_t group, float &mx, float &my) {
        const std::uint32_t mid = group + (word(rt, group + 0x40u) >> 1) * 8u;
        my = psp_mul(y(group) + y(mid), 0.5f);
        mx = psp_mul(x(group) + x(mid), 0.5f);
    };

    if (group0 != 0xFFu) {
        key(0x24u, group0, byte_u(rt, from + 1u), half_u(rt, from + 2u));
        const std::uint32_t self = find(0x24u);
        const std::uint32_t count = word(rt, map + 0x188u);
        put(rt, sp(0x94u), count);
        const float floor = y(self);
        if (count == 0u) return;
        for (std::uint32_t g = 0u; g != count; ++g) {
            const std::uint32_t group = groups + g * 0x48u;
            if (g == byte_u(rt, from)) {
                // the previous and the next node of the own group
                const std::uint32_t index = byte_u(rt, from + 1u);
                const std::uint32_t previous = (index != 0u ? index : word(rt, group + 0x40u)) - 1u;
                key(0x40u, g, previous, 0u);
                key(0x20u, g, previous, 0u);
                const std::uint32_t before = find(0x20u);
                if (!outside(before, floor) && call_set(rt, ctx, 0x088B6908u, {map, self, before, g}) == 0u &&
                    call_set(rt, ctx, 0x088B6C1Cu, {map, context, from, before}) != 0u) {
                    put16(rt, sp(0x42u), angle(0x70u, before, self));
                    emit(0x40u);
                }
                const std::uint32_t now = byte_u(rt, from + 1u);
                const std::uint32_t next = now == word(rt, group + 0x40u) - 1u ? 0u : (now + 1u) & 0xFFu;
                key(0x1Cu, g, next, 0u);
                key(0x44u, g, next, 0u);
                const std::uint32_t after = find(0x1Cu);
                if (!outside(after, floor) && call_set(rt, ctx, 0x088B6908u, {map, self, after, g}) == 0u &&
                    call_set(rt, ctx, 0x088B6C1Cu, {map, context, from, after}) != 0u) {
                    put16(rt, sp(0x46u), angle(0x68u, after, self));
                    emit(0x44u);
                }
                continue;
            }
            // another group: the nodes furthest either way across the line to its middle
            float mx = 0.0f, my = 0.0f;
            middle(group, mx, my);
            const float dy = my - y(self);
            const float dx = mx - x(self);
            const float nx = -dy;
            const std::uint32_t nodes = word(rt, group + 0x40u);
            if (nodes == 0u) continue;
            float least = 1.0f, most = -1.0f;
            std::uint32_t least_at = 0xFFFFFFFFu, most_at = 0xFFFFFFFFu;
            for (std::uint32_t index = 0u; index < word(rt, group + 0x40u); ++index) {
                key(0x18u, g, index, 0u);
                const std::uint32_t node = find(0x18u);
                if (outside(node, floor)) continue;
                if (call_set(rt, ctx, 0x088B69ACu, {map, self, node}) != 0u) continue;
                if (call_set(rt, ctx, 0x088B6C1Cu, {map, context, from, node}) == 0u) continue;
                const float across = psp_mul(nx, x(node) - x(self)) + psp_mul(dx, y(node) - y(self));
                if (across < least) { least = across; least_at = index; }
                if (most < across) { most = across; most_at = index; }
            }
            put(rt, sp(0x90u), least_at);
            put(rt, sp(0x8Cu), most_at);
            if (least_at != 0xFFFFFFFFu) {
                key(0x14u, g, least_at, 0u);
                key(0x44u, g, least_at, 0u);
                const std::uint32_t node = find(0x14u);
                put16(rt, sp(0x46u), angle(0x60u, node, self));
                emit(0x44u);
            }
            if (most_at != 0xFFFFFFFFu) {
                key(0x10u, g, most_at, 0u);
                key(0x44u, g, most_at, 0u);
                const std::uint32_t node = find(0x10u);
                put16(rt, sp(0x46u), angle(0x58u, node, self));
                emit(0x44u);
            }
        }
        return;
    }
    // a free `from`: the extremes of every group, within 0x71B of its own angle
    key(0xCu, group0, byte_u(rt, from + 1u), half_u(rt, from + 2u));
    const std::uint32_t self = find(0xCu);
    const float floor = y(self);
    const std::uint32_t count = word(rt, map + 0x188u);
    put(rt, sp(0x88u), count);
    if (count == 0u) return;
    for (std::uint32_t g = 0u; g != count; ++g) {
        const std::uint32_t group = groups + g * 0x48u;
        float mx = 0.0f, my = 0.0f;
        middle(group, mx, my);
        const float dy = my - y(self);
        const float dx = mx - x(self);
        const float nx = -dy;
        if (word(rt, group + 0x40u) == 0u) continue;
        float least = 1.0f, most = -1.0f;
        std::uint32_t least_at = 0xFFFFFFFFu, most_at = 0xFFFFFFFFu;
        for (std::uint32_t index = 0u; index < word(rt, group + 0x40u); ++index) {
            key(8u, g, index, 0u);
            const std::uint32_t node = find(8u);
            if (outside(node, floor)) continue;
            if (call_set(rt, ctx, 0x088B69ACu, {map, self, node}) != 0u) continue;
            const float across = psp_mul(nx, x(node) - x(self)) + psp_mul(dx, y(node) - y(self));
            if (across < least) { least = across; least_at = index; }
            if (most < across) { most = across; most_at = index; }
        }
        put(rt, sp(0x84u), least_at);
        put(rt, sp(0x80u), most_at);
        const auto keep = [&](std::uint32_t slot, std::uint32_t index, std::uint32_t delta_at) {
            key(slot, g, index, 0u);
            key(0x44u, g, index, 0u);
            const std::uint32_t node = find(slot);
            const std::uint32_t direction = angle(delta_at, node, self);
            const std::int32_t turn = static_cast<std::int16_t>(direction - half_u(rt, from + 2u));
            if (turn < 0x71C && turn >= -0x71B) {
                put16(rt, sp(0x46u), direction);
                emit(0x44u);
            }
        };
        if (least_at != 0xFFFFFFFFu) keep(4u, least_at, 0x50u);
        if (most_at != 0xFFFFFFFFu) keep(0u, most_at, 0x48u);
    }
}

// func_0892F0A8(out, source): unpacks seven sections of `source` (the 64-bit offsets at source+0, +0x48, +0x78, +0x68,
// +0x38, +0x40 and the optional +0xF8; loaded like func_0892E51C's into a frame buffer by func_08936584) into `out`, every
// field widened to a word with its own width and sign. The +0xF8 section, when present, also sets the byte out+0x300 to
// 1; absent (offset -1), out+0x300 and out+0x304..+0x314 are zeroed.
void func_0892F0A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    enum class Width { kWord, kSigned16, kUnsigned16, kSigned8, kUnsigned8 };
    struct Field {
        Width width;
        std::uint32_t from;  // in the loaded section
        std::uint32_t to;    // in `out`
    };
    static constexpr Field kSection0[] = {
        {Width::kWord, 0x4u, 0x4u}, {Width::kSigned16, 0xCu, 0xCu}, {Width::kUnsigned8, 0x13u, 0x1Cu},
        {Width::kUnsigned8, 0x14u, 0x20u}, {Width::kUnsigned8, 0x15u, 0x24u}, {Width::kUnsigned8, 0x16u, 0x28u},
        {Width::kUnsigned8, 0x17u, 0x2Cu}, {Width::kUnsigned8, 0x18u, 0x30u}, {Width::kWord, 0x8u, 0x8u},
        {Width::kSigned16, 0xEu, 0x10u}, {Width::kSigned16, 0x10u, 0x14u}, {Width::kUnsigned8, 0x12u, 0x18u},
        {Width::kWord, 0x0u, 0x0u},
    };
    static constexpr Field kSection48[] = {
        {Width::kSigned16, 0x0u, 0x38u}, {Width::kSigned16, 0x2u, 0x3Cu}, {Width::kSigned16, 0x4u, 0x40u},
        {Width::kSigned16, 0x6u, 0x44u}, {Width::kSigned16, 0x8u, 0x48u}, {Width::kSigned16, 0xAu, 0x4Cu},
        {Width::kUnsigned16, 0xCu, 0x50u}, {Width::kSigned8, 0x1Au, 0x54u}, {Width::kUnsigned8, 0x1Bu, 0x58u},
        {Width::kUnsigned8, 0x1Cu, 0x5Cu}, {Width::kUnsigned8, 0x1Du, 0x60u}, {Width::kUnsigned8, 0x1Eu, 0x64u},
        {Width::kUnsigned8, 0x1Fu, 0x68u}, {Width::kUnsigned8, 0x20u, 0x6Cu}, {Width::kSigned8, 0x21u, 0x70u},
        {Width::kSigned8, 0x22u, 0x74u}, {Width::kUnsigned8, 0x23u, 0x78u}, {Width::kUnsigned8, 0x24u, 0x7Cu},
        {Width::kUnsigned8, 0x25u, 0x80u}, {Width::kUnsigned8, 0x26u, 0x84u}, {Width::kUnsigned8, 0x27u, 0x88u},
        {Width::kUnsigned8, 0x28u, 0x8Cu}, {Width::kUnsigned8, 0x29u, 0x90u}, {Width::kUnsigned8, 0x2Au, 0x94u},
        {Width::kUnsigned8, 0x2Bu, 0x98u}, {Width::kUnsigned8, 0x2Cu, 0x9Cu}, {Width::kUnsigned8, 0x2Du, 0xA0u},
        {Width::kUnsigned8, 0x2Eu, 0xA4u}, {Width::kUnsigned8, 0x2Fu, 0xA8u}, {Width::kUnsigned8, 0x30u, 0xACu},
        {Width::kUnsigned8, 0x31u, 0xB0u}, {Width::kUnsigned8, 0x32u, 0xB4u}, {Width::kSigned8, 0x33u, 0xB8u},
        {Width::kSigned8, 0x34u, 0xBCu}, {Width::kSigned8, 0x35u, 0xC0u}, {Width::kSigned8, 0x36u, 0xC4u},
        {Width::kSigned8, 0x37u, 0xC8u}, {Width::kSigned8, 0x38u, 0xCCu}, {Width::kUnsigned8, 0x39u, 0xD0u},
        {Width::kUnsigned8, 0x3Au, 0xD4u}, {Width::kUnsigned8, 0x3Bu, 0xD8u}, {Width::kUnsigned8, 0x3Cu, 0xDCu},
        {Width::kUnsigned8, 0x3Du, 0xE0u}, {Width::kUnsigned8, 0x3Eu, 0xE4u}, {Width::kUnsigned8, 0x3Fu, 0xE8u},
        {Width::kUnsigned8, 0x40u, 0xECu}, {Width::kUnsigned8, 0x41u, 0xF0u}, {Width::kUnsigned8, 0x42u, 0xF4u},
        {Width::kUnsigned8, 0x43u, 0xF8u}, {Width::kUnsigned8, 0x44u, 0xFCu}, {Width::kUnsigned8, 0x45u, 0x100u},
        {Width::kUnsigned8, 0x46u, 0x104u}, {Width::kUnsigned8, 0x47u, 0x108u}, {Width::kUnsigned8, 0x48u, 0x10Cu},
        {Width::kUnsigned8, 0x49u, 0x110u}, {Width::kUnsigned8, 0x4Au, 0x114u}, {Width::kUnsigned8, 0x4Bu, 0x118u},
        {Width::kUnsigned8, 0x4Cu, 0x11Cu}, {Width::kUnsigned8, 0x4Du, 0x120u}, {Width::kUnsigned8, 0x4Eu, 0x124u},
        {Width::kUnsigned8, 0x4Fu, 0x128u}, {Width::kUnsigned8, 0x50u, 0x12Cu}, {Width::kUnsigned8, 0x51u, 0x130u},
        {Width::kUnsigned8, 0x52u, 0x134u}, {Width::kUnsigned8, 0x53u, 0x138u}, {Width::kUnsigned8, 0x54u, 0x13Cu},
        {Width::kUnsigned8, 0x55u, 0x140u}, {Width::kUnsigned8, 0x56u, 0x144u}, {Width::kUnsigned8, 0x57u, 0x148u},
        {Width::kUnsigned8, 0x58u, 0x14Cu}, {Width::kUnsigned8, 0x59u, 0x150u}, {Width::kUnsigned8, 0x5Au, 0x154u},
        {Width::kUnsigned8, 0x5Bu, 0x158u}, {Width::kUnsigned8, 0x5Cu, 0x15Cu}, {Width::kUnsigned8, 0x5Du, 0x160u},
        {Width::kUnsigned8, 0x5Eu, 0x164u}, {Width::kUnsigned8, 0x5Fu, 0x168u}, {Width::kUnsigned8, 0x60u, 0x16Cu},
        {Width::kUnsigned8, 0x61u, 0x170u}, {Width::kUnsigned8, 0x62u, 0x174u}, {Width::kUnsigned8, 0x63u, 0x178u},
        {Width::kUnsigned8, 0x64u, 0x17Cu}, {Width::kUnsigned8, 0x65u, 0x180u}, {Width::kUnsigned8, 0x66u, 0x184u},
        {Width::kUnsigned8, 0x67u, 0x188u}, {Width::kUnsigned8, 0x6Fu, 0x1A8u}, {Width::kUnsigned8, 0x70u, 0x1ACu},
        {Width::kUnsigned8, 0x71u, 0x1B0u}, {Width::kUnsigned8, 0x72u, 0x1B4u}, {Width::kUnsigned8, 0x73u, 0x1B8u},
        {Width::kUnsigned8, 0x74u, 0x1BCu}, {Width::kUnsigned8, 0x75u, 0x1C0u}, {Width::kUnsigned8, 0x6Eu, 0x1A4u},
        {Width::kUnsigned8, 0x68u, 0x18Cu}, {Width::kUnsigned8, 0x69u, 0x190u}, {Width::kUnsigned8, 0x6Au, 0x194u},
        {Width::kUnsigned8, 0x6Bu, 0x198u}, {Width::kUnsigned8, 0x6Cu, 0x19Cu}, {Width::kUnsigned8, 0x6Du, 0x1A0u},
        {Width::kUnsigned8, 0x76u, 0x1C4u}, {Width::kUnsigned8, 0x77u, 0x1C8u}, {Width::kUnsigned8, 0x78u, 0x1CCu},
        {Width::kUnsigned8, 0x7Au, 0x1D4u}, {Width::kUnsigned8, 0x7Cu, 0x1DCu}, {Width::kUnsigned8, 0x7Bu, 0x1D8u},
        {Width::kUnsigned8, 0x7Du, 0x1E0u}, {Width::kUnsigned8, 0x7Eu, 0x1E4u}, {Width::kUnsigned8, 0x79u, 0x1D0u},
    };
    static constexpr Field kSection78[] = {
        {Width::kUnsigned8, 0xCu, 0x204u}, {Width::kUnsigned8, 0xDu, 0x208u}, {Width::kUnsigned8, 0xEu, 0x20Cu},
        {Width::kUnsigned8, 0xFu, 0x210u}, {Width::kUnsigned8, 0x10u, 0x214u}, {Width::kUnsigned8, 0x11u, 0x218u},
        {Width::kUnsigned8, 0x12u, 0x21Cu}, {Width::kUnsigned16, 0x0u, 0x1E8u}, {Width::kUnsigned16, 0x2u, 0x1ECu},
        {Width::kUnsigned16, 0x4u, 0x1F0u}, {Width::kUnsigned16, 0x6u, 0x1F4u}, {Width::kUnsigned16, 0x8u, 0x1F8u},
        {Width::kUnsigned8, 0xAu, 0x1FCu}, {Width::kUnsigned8, 0xBu, 0x200u}, {Width::kSigned8, 0x13u, 0x220u},
        {Width::kSigned8, 0x14u, 0x224u}, {Width::kUnsigned8, 0x15u, 0x228u}, {Width::kUnsigned8, 0x17u, 0x230u},
        {Width::kUnsigned8, 0x19u, 0x238u}, {Width::kUnsigned8, 0x18u, 0x234u}, {Width::kUnsigned8, 0x1Au, 0x23Cu},
        {Width::kUnsigned8, 0x1Bu, 0x240u}, {Width::kUnsigned8, 0x1Cu, 0x244u}, {Width::kUnsigned8, 0x16u, 0x22Cu},
    };
    static constexpr Field kSection68[] = {
        {Width::kUnsigned8, 0x0u, 0x248u}, {Width::kUnsigned8, 0x27u, 0x254u}, {Width::kUnsigned8, 0x26u, 0x250u},
        {Width::kUnsigned8, 0x25u, 0x24Cu},
    };
    static constexpr Field kSection38[] = {
        {Width::kUnsigned16, 0x0u, 0x258u}, {Width::kUnsigned16, 0x6u, 0x264u}, {Width::kUnsigned8, 0xEu, 0x274u},
        {Width::kUnsigned8, 0xFu, 0x278u}, {Width::kUnsigned8, 0x10u, 0x27Cu}, {Width::kUnsigned8, 0x11u, 0x280u},
        {Width::kUnsigned8, 0x12u, 0x284u}, {Width::kUnsigned8, 0x13u, 0x288u}, {Width::kUnsigned8, 0x14u, 0x28Cu},
        {Width::kUnsigned16, 0x4u, 0x260u}, {Width::kUnsigned16, 0x8u, 0x268u}, {Width::kUnsigned16, 0xAu, 0x26Cu},
        {Width::kUnsigned16, 0xCu, 0x270u}, {Width::kUnsigned16, 0x2u, 0x25Cu},
    };
    static constexpr Field kSection40[] = {
        {Width::kUnsigned16, 0x0u, 0x290u}, {Width::kUnsigned16, 0x2u, 0x294u}, {Width::kUnsigned16, 0x4u, 0x298u},
        {Width::kUnsigned16, 0x6u, 0x29Cu}, {Width::kUnsigned16, 0x8u, 0x2A0u}, {Width::kUnsigned16, 0xAu, 0x2A4u},
        {Width::kUnsigned16, 0xCu, 0x2A8u}, {Width::kUnsigned16, 0xEu, 0x2ACu}, {Width::kUnsigned16, 0x10u, 0x2B0u},
        {Width::kUnsigned16, 0x12u, 0x2B4u}, {Width::kUnsigned16, 0x14u, 0x2B8u}, {Width::kUnsigned16, 0x16u, 0x2BCu},
        {Width::kUnsigned16, 0x18u, 0x2C0u}, {Width::kUnsigned16, 0x1Au, 0x2C4u}, {Width::kUnsigned16, 0x1Cu, 0x2C8u},
        {Width::kUnsigned16, 0x1Eu, 0x2CCu}, {Width::kUnsigned16, 0x20u, 0x2D0u}, {Width::kUnsigned8, 0x22u, 0x2D4u},
        {Width::kUnsigned8, 0x23u, 0x2D8u}, {Width::kUnsigned8, 0x24u, 0x2DCu}, {Width::kUnsigned8, 0x25u, 0x2E0u},
        {Width::kUnsigned8, 0x26u, 0x2E4u}, {Width::kUnsigned8, 0x27u, 0x2E8u}, {Width::kUnsigned8, 0x28u, 0x2ECu},
        {Width::kUnsigned8, 0x29u, 0x2F0u}, {Width::kSigned8, 0x2Au, 0x2F4u}, {Width::kSigned8, 0x2Bu, 0x2F8u},
        {Width::kUnsigned8, 0x2Cu, 0x2FCu},
    };
    static constexpr Field kSectionF8[] = {
        {Width::kUnsigned8, 0x5u, 0x314u}, {Width::kUnsigned16, 0x0u, 0x304u}, {Width::kUnsigned8, 0x2u, 0x308u},
        {Width::kUnsigned8, 0x3u, 0x30Cu}, {Width::kUnsigned8, 0x4u, 0x310u},
    };
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[5];
    GuestFrame frame(ctx, 0xC0u);
    const std::uint32_t buffer = frame.at(0u);
    const auto read = [&](const Field &field) -> std::uint32_t {
        const std::uint32_t at = buffer + field.from;
        switch (field.width) {
        case Width::kWord: return word(rt, at);
        case Width::kSigned16: return sext16(half_u(rt, at));
        case Width::kUnsigned16: return half_u(rt, at);
        case Width::kSigned8: return sext8(byte_u(rt, at));
        case Width::kUnsigned8: return byte_u(rt, at);
        }
        return 0u;
    };
    const auto unpack = [&](std::uint32_t offset, const Field *fields, std::size_t count) {
        const std::uint32_t handle = call_set(rt, ctx, 0x089363FCu, {});
        (void)call_set(rt, ctx, 0x08936584u, {handle, ctx.gpr[5], word(rt, source + offset), word(rt, source + offset + 4u), buffer});
        std::uint32_t values[128];
        for (std::size_t index = 0u; index < count; ++index) values[index] = read(fields[index]);
        for (std::size_t index = 0u; index < count; ++index) put(rt, out + fields[index].to, values[index]);
    };
    unpack(0x0u, kSection0, std::size(kSection0));
    unpack(0x48u, kSection48, std::size(kSection48));
    unpack(0x78u, kSection78, std::size(kSection78));
    unpack(0x68u, kSection68, std::size(kSection68));
    unpack(0x38u, kSection38, std::size(kSection38));
    unpack(0x40u, kSection40, std::size(kSection40));
    if (word(rt, source + 0xF8u) == 0xFFFFFFFFu && word(rt, source + 0xFCu) == 0xFFFFFFFFu) {
        put(rt, out + 0x300u, 0u);
        for (const std::uint32_t at : {0x314u, 0x304u, 0x308u, 0x30Cu, 0x310u}) put(rt, out + at, 0u);
        ctx.gpr[2] = out + 0x300u;
        return;
    }
    const std::uint32_t handle = call_set(rt, ctx, 0x089363FCu, {});
    (void)call_set(rt, ctx, 0x08936584u, {handle, ctx.gpr[5], word(rt, source + 0xF8u), word(rt, source + 0xFCu), buffer});
    std::uint32_t values[std::size(kSectionF8)];
    for (std::size_t index = 0u; index < std::size(kSectionF8); ++index)
        values[index] = read(kSectionF8[index]);
    put8(rt, out + 0x300u, 1u);
    for (std::size_t index = 0u; index < std::size(kSectionF8); ++index) put(rt, out + kSectionF8[index].to, values[index]);
    ctx.gpr[2] = 1u;
}

// func_0892C9F0(source, out, buffer): one of func_0892E51C's section parsers: copies 3 fields of the loaded section into `out` (a leaf).
void func_0892C9F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put8(rt, out + 0x178u, byte_u(rt, buffer + 0x3u));
    put8(rt, out + 0x179u, byte_u(rt, buffer + 0x4u));
    put8(rt, out + 0x17Au, byte_u(rt, buffer + 0x5u));
}

// func_0892CA0C(source, out, buffer): one of func_0892E51C's section parsers: copies 40 fields of the loaded section into `out` (a leaf).
void func_0892CA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put8(rt, out + 0x182u, 0x6u);
    put8(rt, out + 0x28u, byte_u(rt, source + 0x155u));
    put8(rt, out + 0x19Cu, byte_u(rt, buffer + 0xDu));
    put8(rt, out + 0x19Du, byte_u(rt, buffer + 0xEu));
    put8(rt, out + 0x19Eu, byte_u(rt, buffer + 0xFu));
    put8(rt, out + 0x19Fu, byte_u(rt, buffer + 0x10u));
    put8(rt, out + 0x1A0u, byte_u(rt, buffer + 0x11u));
    put8(rt, out + 0x1A1u, byte_u(rt, buffer + 0x12u));
    put8(rt, out + 0x1A2u, byte_u(rt, buffer + 0x13u));
    put8(rt, out + 0x1A3u, byte_u(rt, buffer + 0x14u));
    put8(rt, out + 0x1A4u, byte_u(rt, buffer + 0x15u));
    put8(rt, out + 0x1A5u, byte_u(rt, buffer + 0x16u));
    put8(rt, out + 0x1A6u, byte_u(rt, buffer + 0x17u));
    put8(rt, out + 0x180u, 0x6u);
    put8(rt, out + 0x1A7u, byte_u(rt, buffer + 0x18u));
    put8(rt, out + 0x184u, byte_u(rt, buffer + 0x1u));
    put8(rt, out + 0x185u, byte_u(rt, buffer + 0x2u));
    put8(rt, out + 0x186u, byte_u(rt, buffer + 0x3u));
    put8(rt, out + 0x187u, byte_u(rt, buffer + 0x4u));
    put8(rt, out + 0x188u, byte_u(rt, buffer + 0x5u));
    put8(rt, out + 0x189u, byte_u(rt, buffer + 0x6u));
    put8(rt, out + 0x18Au, byte_u(rt, buffer + 0x7u));
    put8(rt, out + 0x18Bu, byte_u(rt, buffer + 0x8u));
    put8(rt, out + 0x18Cu, byte_u(rt, buffer + 0x9u));
    put8(rt, out + 0x18Du, byte_u(rt, buffer + 0xAu));
    put8(rt, out + 0x18Eu, byte_u(rt, buffer + 0xBu));
    put8(rt, out + 0x181u, 0x6u);
    put8(rt, out + 0x18Fu, byte_u(rt, buffer + 0xCu));
    put8(rt, out + 0x190u, byte_u(rt, buffer + 0x19u));
    put8(rt, out + 0x191u, byte_u(rt, buffer + 0x1Au));
    put8(rt, out + 0x192u, byte_u(rt, buffer + 0x1Bu));
    put8(rt, out + 0x193u, byte_u(rt, buffer + 0x1Cu));
    put8(rt, out + 0x194u, byte_u(rt, buffer + 0x1Du));
    put8(rt, out + 0x195u, byte_u(rt, buffer + 0x1Eu));
    put8(rt, out + 0x196u, byte_u(rt, buffer + 0x1Fu));
    put8(rt, out + 0x197u, byte_u(rt, buffer + 0x20u));
    put8(rt, out + 0x198u, byte_u(rt, buffer + 0x21u));
    put8(rt, out + 0x199u, byte_u(rt, buffer + 0x22u));
    put8(rt, out + 0x19Au, byte_u(rt, buffer + 0x23u));
    put8(rt, out + 0x19Bu, byte_u(rt, buffer + 0x24u));
}

// func_0892CD18(source, out, buffer): one of func_0892E51C's section parsers: copies 11 fields of the loaded section into `out` (a leaf).
void func_0892CD18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put8(rt, out + 0x12Cu, byte_u(rt, buffer + 0x12u));
    put8(rt, out + 0x12Du, byte_u(rt, buffer + 0x13u));
    put8(rt, out + 0x12Au, byte_u(rt, source + 0x126u));
    put8(rt, out + 0x128u, byte_u(rt, buffer + 0x10u));
    put8(rt, out + 0x129u, byte_u(rt, buffer + 0x11u));
    put16(rt, out + 0xBAu, half_u(rt, buffer + 0x2u));
    put16(rt, out + 0x174u, half_u(rt, buffer + 0x4u));
    put16(rt, out + 0x176u, half_u(rt, buffer + 0x6u));
    put16(rt, out + 0x164u, half_u(rt, buffer + 0x8u));
    put16(rt, out + 0x166u, half_u(rt, buffer + 0xAu));
    put16(rt, out + 0xB8u, half_u(rt, buffer));
}

// func_0892CDD4(source, out, buffer): one of func_0892E51C's section parsers: copies 8 fields of the loaded section into `out` (a leaf).
void func_0892CDD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put8(rt, out + 0x168u, byte_u(rt, buffer + 0x3u));
    put8(rt, out + 0x169u, byte_u(rt, buffer + 0xFu));
    put8(rt, out + 0x16Au, byte_u(rt, source + 0x148u));
    put8(rt, out + 0x16Bu, byte_u(rt, source + 0x149u));
    put8(rt, out + 0x16Cu, byte_u(rt, source + 0x14Au));
    put8(rt, out + 0x16Du, byte_u(rt, source + 0x14Bu));
    put8(rt, out + 0x16Eu, byte_u(rt, source + 0x14Cu));
    put8(rt, out + 0x16Fu, byte_u(rt, source + 0x14Du));
}

// func_0892D700(source, out, buffer): one of func_0892E51C's section parsers: copies 2 fields of `source` into `out` (a leaf).
void func_0892D700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    put8(rt, out + 0x5u, byte_u(rt, source + 0x153u));
    put8(rt, out + 0x6u, byte_u(rt, source + 0x154u));
}

// func_0892D71C(source, out, buffer): one of func_0892E51C's section parsers: copies 2 fields of the loaded section into `out` (a leaf).
void func_0892D71C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put8(rt, out + 0x11Du, byte_u(rt, source + 0x15Du));
    put16(rt, out + 0x11Eu, half_u(rt, buffer));
}

// func_0892CE18(source, out, buffer): a section parser of func_0892E51C: out+0x20 (signed) scaled by the section's
// halfword +0 per mille, out+0x24 by its byte +5 percent.
void func_0892CE18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put16(rt, out + 0x20u, signed_permille(sext16(half_u(rt, out + 0x20u)) * half_u(rt, buffer)));
    put8(rt, out + 0x24u, unsigned_percent(byte_u(rt, out + 0x24u) * byte_u(rt, buffer + 5u)));
}

// func_0892CE74(source, out, buffer): func_0892CE18's scaling (with the byte +0x17) and 17 more fields copied from the
// section and from source+0x129 / +0x12A.
void func_0892CE74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put16(rt, out + 0x20u, signed_permille(sext16(half_u(rt, out + 0x20u)) * half_u(rt, buffer)));
    put8(rt, out + 0x24u, unsigned_percent(byte_u(rt, out + 0x24u) * byte_u(rt, buffer + 0x17u)));
    put8(rt, out + 0x42u, byte_u(rt, buffer + 0xCu));
    put16(rt, out + 0x1Cu, half_u(rt, buffer + 2u));
    put16(rt, out + 0x1Eu, half_u(rt, buffer + 4u));
    put8(rt, out + 0x43u, byte_u(rt, source + 0x129u));
    put8(rt, out + 0x44u, byte_u(rt, source + 0x12Au));
    for (std::uint32_t index = 0u; index < 4u; ++index) put8(rt, out + 0x96u + index, byte_u(rt, buffer + 0x13u + index));
    for (std::uint32_t index = 0u; index < 5u; ++index) put8(rt, out + 0x4Cu + index, byte_u(rt, buffer + 0x18u + index));
}

// func_0892CF40, func_0892D108, func_0892D164, func_0892D5EC, func_0892D648, func_0892D6A4 (source, out, buffer): six
// identical section parsers of func_0892E51C: the limits out+0x10A / +0x10C scaled by the section's halfwords +0 / +2.
void func_0892CF40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_limits(rt, ctx.gpr[5], ctx.gpr[6], 0u); }
void func_0892D108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_limits(rt, ctx.gpr[5], ctx.gpr[6], 0u); }
void func_0892D164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_limits(rt, ctx.gpr[5], ctx.gpr[6], 0u); }
void func_0892D5EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_limits(rt, ctx.gpr[5], ctx.gpr[6], 0u); }
void func_0892D648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_limits(rt, ctx.gpr[5], ctx.gpr[6], 0u); }
void func_0892D6A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_limits(rt, ctx.gpr[5], ctx.gpr[6], 0u); }

// func_0892D32C(source, out, buffer): the limits scaled by the section's halfwords +4 / +6, the byte +9 to out+0x113,
// its bytes +0x10 / +0x11 added to out+0xBD / +0xBE, and six bytes of source+0x12B.. to out+0x116..+0x11B (in the order
// 0x118, 0x116, 0x11A, 0x119, 0x117, 0x11B).
void func_0892D32C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    const std::uint32_t bonus_a = byte_u(rt, out + 0xBDu);
    const std::uint32_t bonus_b = byte_u(rt, out + 0xBEu);
    scale_limits(rt, out, buffer, 4u);
    put8(rt, out + 0x113u, byte_u(rt, buffer + 9u));
    put8(rt, out + 0xBDu, bonus_a + byte_u(rt, buffer + 0x10u));
    put8(rt, out + 0xBEu, bonus_b + byte_u(rt, buffer + 0x11u));
    static constexpr std::uint32_t kTargets[] = {0x118u, 0x116u, 0x11Au, 0x119u, 0x117u, 0x11Bu};
    for (std::uint32_t index = 0u; index < 6u; ++index) put8(rt, out + kTargets[index], byte_u(rt, source + 0x12Bu + index));
}

// func_0892D3E0(source, out, buffer): out+0xBA / +0xB8 scaled by the section's bytes +4 / +3 (unsigned percent) and the
// signed out+0x174 / +0x176 by its bytes +5 / +6 (percent).
void func_0892D3E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scale_handling(rt, ctx.gpr[5], ctx.gpr[6]); }

// func_0892D47C(source, out, buffer): func_0892D3E0's scaling, then the section's byte +7 to out+0x161.
void func_0892D47C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    scale_handling(rt, ctx.gpr[5], ctx.gpr[6]);
    put8(rt, ctx.gpr[5] + 0x161u, byte_u(rt, ctx.gpr[6] + 7u));
}

// func_0892D520(source, out, buffer): out+0xBA scaled by the section's byte +3 (unsigned percent), the signed out+0x174,
// +0x176, +0x164, +0x166 by its bytes +4 .. +7 (percent).
void func_0892D520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put16(rt, out + 0xBAu, unsigned_percent(half_u(rt, out + 0xBAu) * byte_u(rt, buffer + 3u)));
    static constexpr std::uint32_t kTargets[] = {0x174u, 0x176u, 0x164u, 0x166u};
    for (std::uint32_t index = 0u; index < 4u; ++index)
        put16(rt, out + kTargets[index], signed_percent(sext16(half_u(rt, out + kTargets[index])) * byte_u(rt, buffer + 4u + index)));
}

// func_0892CD74(source, out, buffer): copies the section's halfword +0x1E and byte +0x29, source's 12 halfwords at
// +0x108 to out+0x138.., its +0x120 / +0x122 and the section's bytes +0x24 / +0x23 (out+0x156 cleared).
void func_0892CD74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    put16(rt, out + 0x152u, half_u(rt, buffer + 0x1Eu));
    put8(rt, out + 0x154u, byte_u(rt, buffer + 0x29u));
    for (std::uint32_t index = 0u; index < 12u; ++index) put16(rt, out + 0x138u + index * 2u, half_u(rt, source + 0x108u + index * 2u));
    put16(rt, out + 0x150u, half_u(rt, source + 0x120u));
    put8(rt, out + 0x157u, byte_u(rt, source + 0x122u));
    put8(rt, out + 0x156u, 0u);
    put8(rt, out + 0x155u, byte_u(rt, buffer + 0x24u));
    put8(rt, out + 0x158u, byte_u(rt, buffer + 0x23u));
}

// func_0892CF9C(source, out, buffer): the levels out+0xC0.. raised by the section's byte +7, out+0xBD by +8, the limits
// scaled by its halfwords +0 / +2.
void func_0892CF9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { raise_levels(rt, ctx.gpr[5], ctx.gpr[6], 7u, 8u, 0u); }

// func_0892D1C0(source, out, buffer): func_0892CF9C with the section's bytes +9 / +0xA and halfwords +4 / +6.
void func_0892D1C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { raise_levels(rt, ctx.gpr[5], ctx.gpr[6], 9u, 0xAu, 4u); }

// func_0892DBB4(source, out, buffer): out+0x1A8 / +0x1A9 from func_089324F8(source, b, c, a, d) with the section's bytes
// +4, +5, +3 and source+0x127, then +8, +9, +7 and source+0x128.
void func_0892DBB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    put8(rt, out + 0x1A8u, call_set(rt, ctx, 0x089324F8u, {source, byte_u(rt, buffer + 4u), byte_u(rt, buffer + 5u),
                                                           byte_u(rt, buffer + 3u), byte_u(rt, source + 0x127u)}));
    put8(rt, out + 0x1A9u, call_set(rt, ctx, 0x089324F8u, {source, byte_u(rt, buffer + 8u), byte_u(rt, buffer + 9u),
                                                           byte_u(rt, buffer + 7u), byte_u(rt, source + 0x128u)}));
}

// func_0892D730(source, out, buffer): the section's byte +5 to out+0x1C2, out+0x1C0 from func_089324F8(source, +9, +0xA,
// +8, source+0x14E or 1 when that is 0), its bytes +4 / +6 to out+0x1C1 / +0x1C3.
void func_0892D730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    put8(rt, out + 0x1C2u, byte_u(rt, buffer + 5u));
    const std::uint32_t rank = byte_u(rt, source + 0x14Eu);
    put8(rt, out + 0x1C0u, call_set(rt, ctx, 0x089324F8u, {source, byte_u(rt, buffer + 9u), byte_u(rt, buffer + 0xAu),
                                                           byte_u(rt, buffer + 8u), rank != 0u ? rank : 1u}));
    put8(rt, out + 0x1C1u, byte_u(rt, buffer + 4u));
    put8(rt, out + 0x1C3u, byte_u(rt, buffer + 6u));
}

// func_0892DC20(source, out, buffer): copies the section's fields to out (+0xE / +0x10 with 0x7C added; +0x1A / +0x1B
// plus source+0x15E); when func_08932D40(source) / func_08932DA0(source) answer 6..10, its bytes +0x18 / +0x19 also go
// to out+0x1E2 / +0x1E4, respectively out+0x1E3 / +0x1E5.
void func_0892DC20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const auto b = [&](std::uint32_t offset) { return byte_u(rt, buffer + offset); };
    const auto h = [&](std::uint32_t offset) { return half_u(rt, buffer + offset); };
    put8(rt, out + 0x22u, b(0x14u));
    put8(rt, out + 0x2Au, b(0xEu) + 0x7Cu);
    put8(rt, out + 0x2Bu, b(0x10u) + 0x7Cu);
    put16(rt, out + 0x1Au, h(4u));
    put16(rt, out + 0x20u, h(6u));
    put8(rt, out + 0x24u, b(0x15u));
    put8(rt, out + 0x27u, b(0x16u));
    put16(rt, out + 0x10u, h(0u));
    put16(rt, out + 0x3Au, h(6u));
    put8(rt, out + 2u, b(0x17u));
    put8(rt, out + 0x38u, b(0x1Au) + byte_u(rt, source + 0x15Eu));
    put8(rt, out + 0x39u, b(0x1Bu) + byte_u(rt, source + 0x15Eu));
    if (call_set(rt, ctx, 0x08932D40u, {source, b(0x17u)}) - 6u < 5u) {
        put8(rt, out + 0x1E2u, b(0x18u));
        put8(rt, out + 0x1E4u, b(0x19u));
    }
    if (call_set(rt, ctx, 0x08932DA0u, {source}) - 6u < 5u) {
        put8(rt, out + 0x1E3u, b(0x18u));
        put8(rt, out + 0x1E5u, b(0x19u));
    }
}

// func_0892D794(source, out, buffer): out+0x1BA..+0x1BF from four func_089324F8(source, b, c, a, d) answers and two
// copied bytes (d = source+0x14F or 1, +0x150, +0x151 or 1, +0x152), and out+0x1B8 packing the low two bits of the
// section's bytes +3, +9, +0xE, +0x14.
void func_0892D794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const auto b = [&](std::uint32_t offset) { return byte_u(rt, buffer + offset); };
    const auto or_one = [](std::uint32_t value) { return value != 0u ? value : 1u; };
    put8(rt, out + 0x1BAu, call_set(rt, ctx, 0x089324F8u, {source, b(5u), b(6u), b(4u), or_one(byte_u(rt, source + 0x14Fu))}));
    put8(rt, out + 0x1BBu, b(8u));
    put8(rt, out + 0x1BCu, call_set(rt, ctx, 0x089324F8u, {source, b(0xBu), b(0xCu), b(0xAu), byte_u(rt, source + 0x150u)}));
    put8(rt, out + 0x1BDu, call_set(rt, ctx, 0x089324F8u, {source, b(0x10u), b(0x11u), b(0xFu), or_one(byte_u(rt, source + 0x151u))}));
    put8(rt, out + 0x1BEu, b(0x13u));
    put8(rt, out + 0x1BFu, call_set(rt, ctx, 0x089324F8u, {source, b(0x16u), b(0x17u), b(0x15u), byte_u(rt, source + 0x152u)}));
    put8(rt, out + 0x1B8u, ((b(0x14u) & 3u) << 6) | ((b(0xEu) & 3u) << 4) | ((b(9u) & 3u) << 2) | (b(3u) & 3u));
}

// func_0892CB48(source, out, buffer): the section's 24 halfwords +0x24.. to out+0xD8.., its 24 bytes +0x5A.. to the
// levels out+0xC0.., its byte +0x74 times 10 to out+0x108, and single fields to out+0x160, +0x120, +0xBD..+0xBF, +0x110,
// +0x111.
void func_0892CB48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    for (std::uint32_t index = 0u; index < 24u; ++index) put16(rt, out + 0xD8u + index * 2u, half_u(rt, buffer + 0x24u + index * 2u));
    for (std::uint32_t index = 0u; index < 24u; ++index) put8(rt, out + 0xC0u + index, byte_u(rt, buffer + 0x5Au + index));
    put16(rt, out + 0x108u, byte_u(rt, buffer + 0x74u) * 10u);
    put8(rt, out + 0x160u, byte_u(rt, buffer + 0x58u));
    put16(rt, out + 0x120u, half_u(rt, buffer + 0x22u));
    put8(rt, out + 0xBDu, byte_u(rt, buffer + 0x57u));
    put8(rt, out + 0xBEu, byte_u(rt, buffer + 0x56u));
    put8(rt, out + 0xBFu, byte_u(rt, buffer + 0x59u));
    put8(rt, out + 0x110u, byte_u(rt, buffer + 0x72u));
    put8(rt, out + 0x111u, byte_u(rt, buffer + 0x73u));
}

// func_0892E424(source, out, buffer): four floats of the section (+4, +8, +0xC, +0x10) to the halfwords out+0x16, +0x18,
// +0x12, +0x14 as unsigned integers (the compiler's float-to-unsigned: values from 2^31 up are shifted down, truncated
// and get the top bit back), the float +0x14 times 10000 truncated to out+0x40, the halfwords +0x30 / +0x32 copied.
void func_0892E424(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    const float two_31 = std::bit_cast<float>(0x4F000000u);
    const auto to_unsigned = [&](float value) {
        if (two_31 <= value) return ctx.fpu_float_to_word(value - two_31, 1u) | 0x80000000u;
        return ctx.fpu_float_to_word(value, 1u);
    };
    put16(rt, out + 0x16u, to_unsigned(float_at(rt, buffer + 4u)));
    put16(rt, out + 0x18u, to_unsigned(float_at(rt, buffer + 8u)));
    put16(rt, out + 0x12u, to_unsigned(float_at(rt, buffer + 0xCu)));
    put16(rt, out + 0x14u, to_unsigned(float_at(rt, buffer + 0x10u)));
    put16(rt, out + 0x40u, ctx.fpu_float_to_word(psp_mul(float_at(rt, buffer + 0x14u), std::bit_cast<float>(0x461C4000u)), 1u));
    put16(rt, out + 0x34u, half_u(rt, buffer + 0x30u));
    put16(rt, out + 0x36u, half_u(rt, buffer + 0x32u));
}

// func_0892E12C(source, out, buffer): load_blocks into out+0x450.., the last bytes to out+0x1CB, +0x1C9, +0x1CD, +0x1CF.
void func_0892E12C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4], out = ctx.gpr[5], buffer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    load_blocks(rt, ctx, frame, source, out, buffer, 0x450u, {0x1CBu, 0x1C9u, 0x1CDu, 0x1CFu});
}

// func_0892E2A4(source, out, buffer): load_blocks into out+0x210.., the last bytes to out+0x1CA, +0x1C8, +0x1CC, +0x1CE.
void func_0892E2A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4], out = ctx.gpr[5], buffer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    load_blocks(rt, ctx, frame, source, out, buffer, 0x210u, {0x1CAu, 0x1C8u, 0x1CCu, 0x1CEu});
}

// func_0892D898(source, out, buffer): the per-corner settings: 44 fields copied from the section and from source+0x131..
// +0x13D, ten func_089324F8(source, b, c, a, d) answers (in pairs sharing `a`, with d = source+0x13E .. +0x147), the
// section's byte +0x7A, source's halfwords +0x134 / +0x136 unless the section's byte +0x7D is set and its bytes +0x13A /
// +0x13B (widened) unless its byte +0x7B is set, then the byte +0x90 and source's halfwords +0x164 / +0x166.
void func_0892D898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    put8(rt, out + 0x92u, byte_u(rt, buffer + 0x39u));
    put8(rt, out + 0x93u, byte_u(rt, buffer + 0x3Au));
    put8(rt, out + 0x94u, byte_u(rt, buffer + 0x3Du));
    put8(rt, out + 0x95u, byte_u(rt, buffer + 0x3Eu));
    put8(rt, out + 0x5Au, byte_u(rt, buffer + 0x48u));
    put8(rt, out + 0x72u, byte_u(rt, buffer + 0x49u));
    put8(rt, out + 0x1D0u, byte_u(rt, buffer + 0x23u));
    put8(rt, out + 0x1D1u, byte_u(rt, buffer + 0x24u));
    put8(rt, out + 0x1D2u, byte_u(rt, buffer + 0x25u));
    put8(rt, out + 0x1D3u, byte_u(rt, buffer + 0x26u));
    put8(rt, out + 0x1D8u, byte_u(rt, buffer + 0x27u));
    put8(rt, out + 0x1D9u, byte_u(rt, buffer + 0x28u));
    put8(rt, out + 0x1DAu, byte_u(rt, buffer + 0x29u));
    put8(rt, out + 0x1DBu, byte_u(rt, buffer + 0x2Au));
    put8(rt, out + 0x1D4u, byte_u(rt, buffer + 0x2Bu));
    put8(rt, out + 0x1D5u, byte_u(rt, buffer + 0x2Cu));
    put8(rt, out + 0x1D6u, byte_u(rt, buffer + 0x2Du));
    put8(rt, out + 0x1D7u, byte_u(rt, buffer + 0x2Eu));
    put8(rt, out + 0x1DCu, byte_u(rt, buffer + 0x2Fu));
    put8(rt, out + 0x1DDu, byte_u(rt, buffer + 0x30u));
    put8(rt, out + 0x1DEu, byte_u(rt, buffer + 0x31u));
    put8(rt, out + 0x1DFu, byte_u(rt, buffer + 0x32u));
    put8(rt, out + 0x90u, byte_u(rt, buffer + 0x21u));
    put8(rt, out + 0x91u, byte_u(rt, buffer + 0x22u));
    put8(rt, out + 0x5Bu, byte_u(rt, buffer + 0x70u));
    put8(rt, out + 0x73u, byte_u(rt, buffer + 0x71u));
    put8(rt, out + 0x5Cu, byte_u(rt, buffer + 0x4Cu));
    put8(rt, out + 0x5Eu, byte_u(rt, buffer + 0x4Eu));
    put8(rt, out + 0x60u, byte_u(rt, buffer + 0x50u));
    put8(rt, out + 0x62u, byte_u(rt, buffer + 0x52u));
    put8(rt, out + 0x74u, byte_u(rt, buffer + 0x4Du));
    put8(rt, out + 0x76u, byte_u(rt, buffer + 0x4Fu));
    put8(rt, out + 0x78u, byte_u(rt, buffer + 0x51u));
    put8(rt, out + 0x7Au, byte_u(rt, buffer + 0x53u));
    put16(rt, out + 0x66u, byte_u(rt, buffer + 0x3Bu));
    put16(rt, out + 0x7Eu, byte_u(rt, buffer + 0x3Cu));
    put8(rt, out + 0x65u, byte_u(rt, buffer + 0x4Au));
    put8(rt, out + 0x7Du, byte_u(rt, buffer + 0x4Bu));
    put8(rt, out + 0x88u, byte_u(rt, source + 0x131u));
    put8(rt, out + 0x89u, byte_u(rt, source + 0x132u));
    put8(rt, out + 0x8Eu, byte_u(rt, source + 0x138u));
    put8(rt, out + 0x8Fu, byte_u(rt, source + 0x139u));
    put8(rt, out + 0x8Cu, byte_u(rt, source + 0x13Cu));
    put8(rt, out + 0x8Du, byte_u(rt, source + 0x13Du));
    const auto b = [&](std::uint32_t offset) { return byte_u(rt, buffer + offset); };
    const auto rate = [&](std::uint32_t target, std::uint32_t first, std::uint32_t second, std::uint32_t shared, std::uint32_t own) {
        put8(rt, out + target, call_set(rt, ctx, 0x089324F8u, {source, b(first), b(second), b(shared), byte_u(rt, source + own)}));
    };
    rate(0x5Du, 0x55u, 0x56u, 0x54u, 0x13Eu);
    rate(0x5Fu, 0x58u, 0x59u, 0x54u, 0x13Fu);
    rate(0x61u, 0x5Cu, 0x5Du, 0x5Bu, 0x13Eu);
    rate(0x63u, 0x5Fu, 0x60u, 0x5Bu, 0x13Fu);
    rate(0x75u, 0x63u, 0x64u, 0x62u, 0x142u);
    rate(0x77u, 0x66u, 0x67u, 0x62u, 0x143u);
    rate(0x79u, 0x6Au, 0x6Bu, 0x69u, 0x144u);
    rate(0x7Bu, 0x6Du, 0x6Eu, 0x69u, 0x145u);
    rate(0x59u, 0x73u, 0x74u, 0x72u, 0x146u);
    rate(0x71u, 0x77u, 0x78u, 0x76u, 0x147u);
    put8(rt, out + 0xA8u, b(0x7Au));
    if (b(0x7Du) == 0u) {
        put16(rt, out + 0x9Au, half_u(rt, source + 0x134u));
        put16(rt, out + 0x9Cu, half_u(rt, source + 0x136u));
    }
    if (b(0x7Bu) == 0u) {
        put16(rt, out + 0x68u, byte_u(rt, source + 0x13Au));
        put16(rt, out + 0x80u, byte_u(rt, source + 0x13Bu));
    }
    put8(rt, out + 0x29u, b(0x90u));
    put16(rt, out + 0x6Au, half_u(rt, source + 0x164u));
    put16(rt, out + 0x82u, half_u(rt, source + 0x166u));
}

} // namespace hot_batch_133
} // namespace gt::game

#define HOT133(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_133::name, gt::ReturnKind::kind)
HOT133(0x08831648u, func_08831648, Word);
HOT133(0x088B859Cu, func_088B859C, Void);
HOT133(0x0892F0A8u, func_0892F0A8, Word);
HOT133(0x0892C9F0u, func_0892C9F0, Void);
HOT133(0x0892CA0Cu, func_0892CA0C, Void);
HOT133(0x0892CD18u, func_0892CD18, Void);
HOT133(0x0892CDD4u, func_0892CDD4, Void);
HOT133(0x0892D700u, func_0892D700, Void);
// HOT133(0x0892D71Cu, func_0892D71C, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
// HOT133(0x0892CE18u, func_0892CE18, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
HOT133(0x0892CE74u, func_0892CE74, Void);
// HOT133(0x0892CF40u, func_0892CF40, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
// HOT133(0x0892D108u, func_0892D108, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
// HOT133(0x0892D164u, func_0892D164, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
HOT133(0x0892D5ECu, func_0892D5EC, Void);
// HOT133(0x0892D648u, func_0892D648, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
// HOT133(0x0892D6A4u, func_0892D6A4, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
HOT133(0x0892D32Cu, func_0892D32C, Void);
// HOT133(0x0892D3E0u, func_0892D3E0, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
HOT133(0x0892D47Cu, func_0892D47C, Void);
// HOT133(0x0892D520u, func_0892D520, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
// HOT133(0x0892CF9Cu, func_0892CF9C, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
// HOT133(0x0892D1C0u, func_0892D1C0, Void);  // batch 133: compared 0 in the fast gate (an optional profile section the routes do not load)
HOT133(0x0892DBB4u, func_0892DBB4, Void);
HOT133(0x0892D730u, func_0892D730, Void);
HOT133(0x0892DC20u, func_0892DC20, Void);
HOT133(0x0892D794u, func_0892D794, Void);
HOT133(0x0892CB48u, func_0892CB48, Void);
HOT133(0x0892E424u, func_0892E424, Void);
HOT133(0x0892E12Cu, func_0892E12C, Void);
HOT133(0x0892E2A4u, func_0892E2A4, Void);
HOT133(0x0892D898u, func_0892D898, Void);
HOT133(0x0892CD74u, func_0892CD74, Void);
