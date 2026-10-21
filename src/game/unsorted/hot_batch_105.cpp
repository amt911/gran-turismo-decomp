// Hot batch 105: functions of GT's graphics library region (0x08B40000-0x08B70000) the routes reach, written by hand from
// `tools.disasm`; role words describe access shapes, none is a game name. VFPU state the original leaves is left too
// (include/gt/vfpu.hpp; the harness compares the VFPU since #258).
//
// Every call passes the argument registers the original sets before the jal (`call_set`); the delay slot of a `jal`
// runs before the call; v0 is set as the original leaves it on every path; frames that callees read are reproduced
// (`GuestFrame`, offsets as in the original), the dead stack is not.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_105 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// The value `lui` leaves for an address built by a relocated lui/addiu (or lui/load/store) pair: the high half, rounded up when the low half is negative.
inline constexpr std::uint32_t lui_of(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
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
// A virtual call the way the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0. A callee that returns without setting v0 hands the function's address on to the caller's code, so both registers are set like the original sets them.
inline GuestResult call_virtual_full(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                     std::initializer_list<std::uint32_t> arguments) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline std::uint32_t call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                  std::initializer_list<std::uint32_t> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}




// The reference-count steps of the shared-object holder: `ll/sc` on a word of guest memory (single-threaded here, the host never preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful `sc`); true when the old value was 1 (the count reached zero).
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// One shared object of func_08B8FE44's elements: its use count (+4) goes down; when that was the last use the virtual slot 8 runs, then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (count_down(rt, ctx, object + 4u)) {
        (void)call_virtual_full(rt, ctx, object, 8u, {object});
        if (count_down(rt, ctx, object + 8u)) (void)call_virtual_full(rt, ctx, object, 0xCu, {object});
    }
}

// The end of a destructor that releases its last shared object with a tail jump: `jr t9` to the object's virtual slot 0xC with a0 = the object (v0 = its vtable, t9 = the slot's function), after the frame is gone.
inline void tail_virtual_slot_c(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object});
}

// ---- functions ----





// func_08B4E724: tail jump to func_08B4E700 with the caller's registers.
void func_08B4E724(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08B4E700u, {}); }

// func_08B4D0C8(v): swaps the floats at v + 4 and v + 8 (f0 / f1 keep the two loaded values).
void func_08B4D0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t v = ctx.gpr[4];
    ctx.fpr[0] = float_at(rt, v + 8u);
    ctx.fpr[1] = float_at(rt, v + 4u);
    put_float(rt, v + 4u, ctx.fpr[0]);
    put_float(rt, v + 8u, ctx.fpr[1]);
}

// func_08B4ABCC(a, b) -> a.x * b.x + a.y * b.y (two-component dot product, in f0).
void func_08B4ABCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    ctx.fpr[3] = float_at(rt, b + 4u);
    ctx.fpr[2] = float_at(rt, b);
    ctx.fpr[1] = psp_mul(float_at(rt, a + 4u), ctx.fpr[3]);
    ctx.fpr[0] = psp_mul(float_at(rt, a), ctx.fpr[2]) + ctx.fpr[1];
}

// func_08B4AC54(out, a, b): out = (a.x * b.x, a.y * b.y); each component is stored before the next is loaded.
void func_08B4AC54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    put_float(rt, out, psp_mul(float_at(rt, a), float_at(rt, b)));
    ctx.fpr[1] = float_at(rt, b + 4u);
    ctx.fpr[0] = psp_mul(float_at(rt, a + 4u), ctx.fpr[1]);
    put_float(rt, out + 4u, ctx.fpr[0]);
}

// func_08B4AD90(a0, a1, v): copies the two floats of v to a frame and calls func_08B4AC9C(a0, a1, frame).
void func_08B4AD90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t v = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[0] = float_at(rt, v + 4u);
    ctx.fpr[1] = float_at(rt, v);
    put_float(rt, frame.at(4u), ctx.fpr[0]);
    put_float(rt, frame.at(0u), ctx.fpr[1]);  // the jal's delay slot
    (void)call_set(rt, ctx, 0x08B4AC9Cu, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
}

// func_08B572C8(mode, f12 = value): unless mode is 2, appends the GE command 0x5B with the value's float bits >> 8
// to the graphics library's display list (state 0x08D6CE00: write pointer +0x5C).
void func_08B572C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[6] = kGuState;
    ctx.gpr[5] = 0x5B000000u;
    if (ctx.gpr[4] == 2u) {
        ctx.gpr[2] = 2u;
        return;
    }
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
    const std::uint32_t list = word(rt, kGuListPointer);
    ctx.gpr[7] = bits;
    ctx.gpr[3] = list;
    ctx.gpr[2] = bits >> 8;
    ctx.gpr[4] = list + 4u;
    ctx.gpr[5] = 0x5B000000u | (ctx.gpr[2] & 0x00FFFFFFu);
    put(rt, list, ctx.gpr[5]);
    put(rt, kGuListPointer, list + 4u);
}

// func_08B4BB00(out, axis, f12 = angle): func_08B498AC(&s, &c) with angle * 0.5 gives s and c (in a frame);
// out = (axis.x * s, axis.y * s, axis.z * s, c), stored in the original's order.
void func_08B4BB00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], axis = ctx.gpr[5];
    ctx.fpr[0] = 0.5f;
    ctx.fpr[12] = psp_mul(ctx.fpr[12], 0.5f);
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)});
    ctx.fpr[2] = float_at(rt, frame.at(0u));
    ctx.fpr[3] = float_at(rt, frame.at(4u));
    put_float(rt, out, psp_mul(float_at(rt, axis), ctx.fpr[2]));
    ctx.fpr[0] = psp_mul(float_at(rt, axis + 4u), ctx.fpr[2]);
    put_float(rt, out + 4u, ctx.fpr[0]);
    ctx.fpr[1] = float_at(rt, axis + 8u);
    put_float(rt, out + 12u, ctx.fpr[3]);
    ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[2]);
    put_float(rt, out + 8u, ctx.fpr[1]);
}

// func_08B4F71C(v) -> length of v (func_08B4A744); when it is not 0, v is negated in place and func_08B4A6A0(v, 1 / length)
// runs on it (f0 = the length either way; a NaN length is not 0, as c.eq.s).
void func_08B4F71C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t v = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08B4A744u, {v});
    const float length = ctx.fpr[0];
    ctx.fpr[0] = 0.0f;
    ctx.gpr[4] = v;  // the branch's delay slot, both paths
    if (length == 0.0f) {
        ctx.fpr[0] = length;
        return;
    }
    ctx.fpr[12] = 1.0f / length;
    ctx.fpr[0] = -float_at(rt, v);
    ctx.fpr[1] = -float_at(rt, v + 4u);
    ctx.fpr[2] = -float_at(rt, v + 8u);
    put_float(rt, v, ctx.fpr[0]);
    put_float(rt, v + 4u, ctx.fpr[1]);
    put_float(rt, v + 8u, ctx.fpr[2]);  // the jal's delay slot
    (void)call_set(rt, ctx, 0x08B4A6A0u, {v});
    ctx.fpr[0] = length;
}

// func_08B4C0F8(out, f12 = a, f13 = b): func_08B498AC gives (s1, c1) for a * 0.5 and (s2, c2) for b * 0.5 in a frame;
// out = (s1 * c2, c1 * s2, -(s1 * s2), c2 * c1), stored at +0, +0xC, +4, +8 in the original's order.
void func_08B4C0F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const float b = ctx.fpr[13];
    ctx.fpr[12] = psp_mul(ctx.fpr[12], 0.5f);
    GuestFrame frame(ctx, 0x30u);
    (void)call_set(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)});
    ctx.fpr[12] = psp_mul(b, 0.5f);
    (void)call_set(rt, ctx, 0x08B498ACu, {frame.at(8u), frame.at(0xCu)});
    const float s1 = float_at(rt, frame.at(0u)), c1 = float_at(rt, frame.at(4u));
    const float s2 = float_at(rt, frame.at(8u)), c2 = float_at(rt, frame.at(0xCu));
    ctx.fpr[4] = s2;
    ctx.fpr[3] = -psp_mul(s1, s2);
    ctx.fpr[2] = psp_mul(s1, c2);
    ctx.fpr[0] = psp_mul(c2, c1);
    ctx.fpr[1] = psp_mul(c1, s2);
    put_float(rt, out, ctx.fpr[2]);
    put_float(rt, out + 0xCu, ctx.fpr[0]);
    put_float(rt, out + 4u, ctx.fpr[1]);
    put_float(rt, out + 8u, ctx.fpr[3]);
}

// func_08B54288: loads the vector at 0x08CB6A80 into VFPU C000 and tail-calls func_08B54104(a0, the words at
// 0x08D6DDE0 / 0x08D6DDE4, f12 = the float at 0x08CE0DD0).
void func_08B54288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    gt::vfpu::load_column(rt, ctx, 0u, 0u, 0x08CB6A80u);
    ctx.gpr[2] = kGuState;
    ctx.gpr[6] = word(rt, kGuState + 0xFE4u);
    ctx.gpr[3] = lui_of(0x08CE0DD0u);
    ctx.fpr[12] = float_at(rt, 0x08CE0DD0u);
    ctx.gpr[5] = word(rt, kGuState + 0xFE0u);
    tail_call(ctx, 0x08B54104u, {});
}

// func_08B58EE4: closes the graphics library's display list. func_08B59ED0(0x08D6DE70), then at the state block
// (0x08D6CE00): +0x2D4 = +0x2D0; the GE commands FINISH (0x0F) and END (0x0C) are written at the write pointer, which moves
// past them; +0x2D0 = the next entry of the list it points to ((+0x2D0)->+4), and when there is one, its +0xC
// object gets the two words that point the GE at the new write pointer (0x10 with the high byte, 0x08 with the low 24
// bits). When +0x64 is 0 it tail-calls the import at 0x08C7AAD8 (after vflush / sync, which a host does not need).
void func_08B58EE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x08B59ED0u, {kGuBlock2});
    const std::uint32_t state = kGuState;
    ctx.gpr[6] = state;
    const std::uint32_t list = word(rt, state + 0x5Cu);
    put(rt, state + 0x2D4u, word(rt, state + 0x2D0u));
    const std::uint32_t next = list + 8u;
    put(rt, list, 0x0F000000u);
    put(rt, list + 4u, 0x0C000000u);
    ctx.gpr[4] = list;
    ctx.gpr[5] = next;
    ctx.gpr[3] = next >> 24;
    ctx.gpr[8] = 0x10000000u | ((next >> 24) & 0xFFu) << 16;
    ctx.gpr[7] = 0x08000000u | (next & 0x00FFFFFFu);
    std::uint32_t entry = word(rt, word(rt, state + 0x2D0u) + 4u);
    put(rt, state + 0x2D0u, entry);
    if (entry != 0u) {
        const std::uint32_t target = word(rt, entry + 0xCu);
        put(rt, target + 4u, ctx.gpr[7]);
        put(rt, target, ctx.gpr[8]);
        entry = target;
    }
    ctx.gpr[2] = word(rt, state + 0x64u);
    put(rt, state + 0x5Cu, next);
    if (ctx.gpr[2] != 0u) return;
    tail_call(ctx, 0x08C7AAD8u, {});
}

// func_08B5730C(mode, kind, value): unless mode is 2 or kind is past 4, appends GE commands with value to the display
// list, by kind (jump table at 0x08C956B4, read from the ELF: 0x08B57360, 0x08B57384, 0x08B573DC, 0x08B5733C,
// 0x08B573B8): 0 -> 0x55 | value; 1 -> 0x56 | value, 0x58 | value >> 24; 2 -> 0x55 | value, 0x56 | value,
// 0x58 | value >> 24; 3 -> 0x54 | value; 4 -> 0x57 | value (each with value's low 24 bits).
void func_08B5730C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t kind = ctx.gpr[5], value = ctx.gpr[6];
    ctx.gpr[2] = 2u;
    ctx.gpr[8] = value;
    if (ctx.gpr[4] == 2u) return;
    ctx.gpr[2] = kind < 5u ? 1u : 0u;
    ctx.gpr[3] = lui_of(0x08C956B4u);
    if (kind >= 5u) return;
    static constexpr std::uint32_t kTargets[5] = {0x08B57360u, 0x08B57384u, 0x08B573DCu, 0x08B5733Cu, 0x08B573B8u};
    ctx.gpr[4] = kTargets[kind];
    const std::uint32_t list = word(rt, kGuListPointer);
    const std::uint32_t low = value & 0x00FFFFFFu;
    const auto one = [&](std::uint32_t command) {  // kinds 0, 3, 4
        ctx.gpr[3] = kGuState;
        ctx.gpr[5] = list;
        ctx.gpr[2] = command | low;
        ctx.gpr[4] = list + 4u;
        put(rt, list, ctx.gpr[2]);
        put(rt, kGuListPointer, list + 4u);
    };
    switch (kind) {
    case 0u: one(0x55000000u); break;
    case 3u: one(0x54000000u); break;
    case 4u: one(0x57000000u); break;
    case 1u:
        ctx.gpr[5] = kGuState;
        ctx.gpr[4] = list;
        ctx.gpr[2] = 0x56000000u | low;
        ctx.gpr[3] = 0x58000000u | (value >> 24);
        ctx.gpr[6] = list + 8u;
        put(rt, list, ctx.gpr[2]);
        put(rt, list + 4u, ctx.gpr[3]);
        put(rt, kGuListPointer, list + 8u);
        break;
    default:  // 2
        ctx.gpr[6] = kGuState;
        ctx.gpr[5] = list;
        ctx.gpr[2] = 0x55000000u | low;
        ctx.gpr[3] = 0x56000000u | low;
        ctx.gpr[4] = 0x58000000u | (value >> 24);
        ctx.gpr[7] = list + 12u;
        put(rt, list, ctx.gpr[2]);
        put(rt, list + 4u, ctx.gpr[3]);
        put(rt, list + 8u, ctx.gpr[4]);
        put(rt, kGuListPointer, list + 12u);
        break;
    }
}

// func_08B4FE74(out, u, v): the rotation (a quaternion, through func_08B4BB00) that takes u to v. d = dot(u, v)
// (func_08B4A9A0); when d + 1 > 1.9999 (0x3FFFFCB9) out is the identity (0, 0, 0, 1); when d + 1 > 0 the axis is
// cross(u, v) (func_08B4A9D0) scaled by 1 / sqrt(func_08B4A720(axis)) (func_08B4A5E8) and the angle is
// func_08B49F3C(dot(u, v) clamped to [-1, 1]); otherwise (opposite vectors, or NaN) the axis is func_08B4F678 of v and
// (1, 0, 0), and the angle pi (0x40490FDB). Frame offsets as in the original (axis at +0xC, the cross product at +0x18).
void func_08B4FE74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], u = ctx.gpr[5], v = ctx.gpr[6];
    constexpr float kOne = 1.0f;
    GuestFrame frame(ctx, 0x60u);
    (void)call_set(rt, ctx, 0x08B4A9A0u, {u, v});
    const float d = ctx.fpr[0] + kOne;
    ctx.fpr[1] = d;
    const bool same = std::bit_cast<float>(0x3FFFFCB9u) < d;
    ctx.fpr[0] = 0.0f;  // the bc1f's delay slot, both paths
    if (same) {
        put_float(rt, out + 0xCu, kOne);
        put(rt, out, 0u);
        put(rt, out + 4u, 0u);
        put(rt, out + 8u, 0u);
        return;
    }
    const std::uint32_t axis = frame.at(0xCu);
    ctx.gpr[6] = v;  // the bc1f's delay slot, both paths
    if (0.0f < d) {
        const std::uint32_t cross = frame.at(0x18u);
        (void)call_set(rt, ctx, 0x08B4A9D0u, {cross, u, v});
        ctx.fpr[0] = float_at(rt, cross);
        ctx.fpr[1] = float_at(rt, cross + 4u);
        put_float(rt, axis, ctx.fpr[0]);
        ctx.fpr[0] = float_at(rt, cross + 8u);
        put_float(rt, axis + 4u, ctx.fpr[1]);
        put_float(rt, axis + 8u, ctx.fpr[0]);  // the jal's delay slot
        (void)call_set(rt, ctx, 0x08B4A720u, {axis});
        ctx.fpr[0] = std::sqrt(ctx.fpr[0]);
        ctx.fpr[12] = kOne / ctx.fpr[0];  // the jal's delay slot
        (void)call_set(rt, ctx, 0x08B4A5E8u, {axis});
        (void)call_set(rt, ctx, 0x08B4A9A0u, {u, v});
        ctx.fpr[1] = -1.0f;
        float angle_input = ctx.fpr[0];
        if (angle_input < -1.0f) angle_input = -1.0f;
        else if (!(angle_input < kOne)) angle_input = kOne;  // bc1fl: the delay slot runs only when taken
        ctx.fpr[0] = angle_input;
        ctx.fpr[12] = angle_input;
        (void)call_set(rt, ctx, 0x08B49F3Cu, {});
        ctx.fpr[12] = ctx.fpr[0];
    } else {
        const std::uint32_t reference = frame.at(0u);
        put_float(rt, reference + 8u, ctx.fpr[0]);
        put_float(rt, reference + 4u, ctx.fpr[0]);
        put_float(rt, reference, kOne);  // the jal's delay slot
        (void)call_set(rt, ctx, 0x08B4F678u, {axis, v, reference});
        ctx.fpr[0] = std::bit_cast<float>(0x40490FDBu);
        ctx.fpr[12] = ctx.fpr[0];
    }
    (void)call_set(rt, ctx, 0x08B4BB00u, {out, axis});
}

// func_08B4E648(m): m = the 4x4 identity, built in VFPU M000 (vmidt.q, left there) and stored column by column in the
// original's order (C030, C000, C010, C020).
void func_08B4E648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t m = ctx.gpr[4];
    gt::vfpu::matrix_init(ctx, 0u, 4u, 3u);
    gt::vfpu::store_quad(rt, ctx, 3u, m + 0x30u);
    gt::vfpu::store_quad(rt, ctx, 0u, m);
    gt::vfpu::store_quad(rt, ctx, 1u, m + 0x10u);
    gt::vfpu::store_quad(rt, ctx, 2u, m + 0x20u);
}

// func_08B4E734(to, from): copies a 4x4 matrix through VFPU M000 (left loaded), stored C030, C000, C010, C020.
void func_08B4E734(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t to = ctx.gpr[4], from = ctx.gpr[5];
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, column, from + 0x10u * column);
    gt::vfpu::store_quad(rt, ctx, 3u, to + 0x30u);
    gt::vfpu::store_quad(rt, ctx, 0u, to);
    gt::vfpu::store_quad(rt, ctx, 1u, to + 0x10u);
    gt::vfpu::store_quad(rt, ctx, 2u, to + 0x20u);
}

// func_08B57E40(f12..f15): puts the four floats in VFPU S000..S003 (C000) and tail-calls func_08B57DF4.
void func_08B57E40(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
    ctx.gpr[3] = std::bit_cast<std::uint32_t>(ctx.fpr[13]);
    ctx.gpr[4] = std::bit_cast<std::uint32_t>(ctx.fpr[14]);
    ctx.gpr[5] = std::bit_cast<std::uint32_t>(ctx.fpr[15]);
    gt::vfpu::move_to(ctx, ctx.gpr[2], 0x00u);
    gt::vfpu::move_to(ctx, ctx.gpr[3], 0x20u);
    gt::vfpu::move_to(ctx, ctx.gpr[4], 0x40u);
    gt::vfpu::move_to(ctx, ctx.gpr[5], 0x60u);
    tail_call(ctx, 0x08B57DF4u, {});
}

// func_08B4A774(out, m, v): out = m x v for a 4x4 matrix and a 4-vector (vtfm4.q C010, E100, C000: m in M100, v in
// C000, the result in C010), stored with svl.q / svr.q (unaligned out).
void func_08B4A774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], m = ctx.gpr[5], v = ctx.gpr[6];
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 4u + column, m + 0x10u * column);
    for (std::uint32_t row = 0u; row < 4u; ++row) gt::vfpu::load_scalar(rt, ctx, row * 0x20u, v + 4u * row);
    gt::vfpu::transform(ctx, 1u, 0x24u, 0u, 4u, 4u);
    gt::vfpu::store_quad_left(rt, ctx, 1u, out + 0xCu);
    gt::vfpu::store_quad_right(rt, ctx, 1u, out);
}

// func_08B4ACE0(out, m, p): out = m x (p, 1) for a 4x4 matrix and a point (vhtfm4.q C010, E100, C000); the first three
// lanes are stored (+8, +0, +4, in the original's order).
void func_08B4ACE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], m = ctx.gpr[5], p = ctx.gpr[6];
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 4u + column, m + 0x10u * column);
    for (std::uint32_t row = 0u; row < 3u; ++row) gt::vfpu::load_scalar(rt, ctx, row * 0x20u, p + 4u * row);
    gt::vfpu::transform(ctx, 1u, 0x24u, 0u, 4u, 3u);
    gt::vfpu::store_scalar(rt, ctx, 0x41u, out + 8u);
    gt::vfpu::store_scalar(rt, ctx, 0x01u, out);
    gt::vfpu::store_scalar(rt, ctx, 0x21u, out + 4u);
}

// func_08B4E994(out, a, b): out = a x b for 4x4 matrices: a in M200, b in M000, vmmul.q M100, E200, M000, the result
// stored C130, C100, C110, C120 (all left in the VFPU).
void func_08B4E994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 8u + column, a + 0x10u * column);
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, column, b + 0x10u * column);
    gt::vfpu::matrix_multiply(ctx, 4u, 0x28u, 0u, 4u);
    gt::vfpu::store_quad(rt, ctx, 7u, out + 0x30u);
    gt::vfpu::store_quad(rt, ctx, 4u, out);
    gt::vfpu::store_quad(rt, ctx, 5u, out + 0x10u);
    gt::vfpu::store_quad(rt, ctx, 6u, out + 0x20u);
}

// func_08B54474(a, b): a in VFPU C000 (S000..S002), b in C010 (S010..S012), then func_08B55BC0(0x08D6D0E0).
void func_08B54474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    for (std::uint32_t row = 0u; row < 3u; ++row) gt::vfpu::load_scalar(rt, ctx, row * 0x20u, a + 4u * row);
    for (std::uint32_t row = 0u; row < 3u; ++row) gt::vfpu::load_scalar(rt, ctx, 1u + row * 0x20u, b + 4u * row);
    (void)call_set(rt, ctx, 0x08B55BC0u, {kGuCommandBlock});
}

// func_08B5A838(f12..f15 = r, g, b, a): the colour clamped to [0, 1], scaled by 255, rounded and packed into bytes
// (vsat0, vfim 255, vscl, vf2in 23, vi2uc), stored as the word at 0x08D6D0C4.
void func_08B5A838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
    ctx.gpr[3] = std::bit_cast<std::uint32_t>(ctx.fpr[13]);
    ctx.gpr[4] = std::bit_cast<std::uint32_t>(ctx.fpr[14]);
    ctx.gpr[5] = std::bit_cast<std::uint32_t>(ctx.fpr[15]);
    gt::vfpu::move_to(ctx, ctx.gpr[2], 0x01u);
    gt::vfpu::move_to(ctx, ctx.gpr[3], 0x21u);
    gt::vfpu::move_to(ctx, ctx.gpr[4], 0x41u);
    gt::vfpu::move_to(ctx, ctx.gpr[5], 0x61u);
    ctx.gpr[2] = lui_of(0x08D6D0C4u);
    gt::vfpu::unary(ctx, 4u, 0u, 1u, 4u);                  // vsat0.q C000, C010
    gt::vfpu::load_half(ctx, 1u, 0x5BF8u);                 // vfim.s S010, 255
    gt::vfpu::scale(ctx, 0u, 0u, 1u, 4u);                  // vscl.q C000, C000, S010
    gt::vfpu::float_to_int(ctx, 16u, 0u, 0u, 23u, 4u);     // vf2in.q C000, C000, 23
    gt::vfpu::pack_ints(ctx, 0u, 0u, 0u, 4u);              // vi2uc.q S000, C000
    gt::vfpu::store_scalar(rt, ctx, 0u, 0x08D6D0C4u);
}

// func_08B580F0(f12 = angle, s_out, c_out): the angle times 0x3C360B61 (2 / pi) reduced by a multiple of 4 (vf2iz of a
// quarter, times 4) and turned into sin and cos by vrot.p [S, C]; S000 to s_out, S001 to c_out.
void func_08B580F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
    ctx.gpr[2] = 0x3C360B61u;
    gt::vfpu::move_to(ctx, ctx.gpr[3], 0x00u);
    gt::vfpu::move_to(ctx, ctx.gpr[2], 0x20u);
    gt::vfpu::binary(ctx, gt::vfpu::Binary::Mul, 0x40u, 0x00u, 0x20u, 1u);  // vmul.s S002, S000, S001
    gt::vfpu::load_half(ctx, 0x00u, 0x3400u);                                // vfim.s S000, 0.25
    gt::vfpu::binary(ctx, gt::vfpu::Binary::Mul, 0x20u, 0x40u, 0x00u, 1u);  // vmul.s S001, S002, S000
    gt::vfpu::float_to_int(ctx, 17u, 0x20u, 0x20u, 0u, 1u);                  // vf2iz.s S001, S001, 0
    gt::vfpu::int_to_float(ctx, 0x20u, 0x20u, 0u, 1u);                       // vi2f.s S001, S001, 0
    gt::vfpu::load_half(ctx, 0x00u, 0x4400u);                                // vfim.s S000, 4
    gt::vfpu::binary(ctx, gt::vfpu::Binary::Mul, 0x20u, 0x20u, 0x00u, 1u);  // vmul.s S001, S001, S000
    gt::vfpu::binary(ctx, gt::vfpu::Binary::Sub, 0x40u, 0x40u, 0x20u, 1u);  // vsub.s S002, S002, S001
    gt::vfpu::rotation(ctx, 0u, 0x40u, 2u, 1u);                              // vrot.p C000, S002, [S, C]
    gt::vfpu::store_scalar(rt, ctx, 0x00u, ctx.gpr[4]);
    gt::vfpu::store_scalar(rt, ctx, 0x20u, ctx.gpr[5]);
}

// func_08B5436C: copies the 4x4 matrix func_08B56C34(0x08D6D0E0) returns to the state block's +0x1290, through VFPU
// M000 (left loaded); v0 = the state block (0x08D6CE00).
void func_08B5436C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t matrix = call_set(rt, ctx, 0x08B56C34u, {kGuCommandBlock});
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, column, matrix + 0x10u * column);
    ctx.gpr[2] = kGuState;
    for (std::uint32_t column = 0u; column < 4u; ++column)
        gt::vfpu::store_quad(rt, ctx, column, kGuState + 0x1290u + 0x10u * column);
}

// The current matrix of the state block's stack: the word at +0xFE0 + 4 x (the index at +0xFF0). v0 / v1 / a2 keep
// the pointers the original leaves.
std::uint32_t current_matrix(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = kGuState;
    ctx.gpr[2] = (word(rt, kGuState + 0xFF0u) << 2) + kGuState;
    ctx.gpr[6] = word(rt, ctx.gpr[2] + 0xFE0u);
    return ctx.gpr[6];
}

// func_08B5A7A8(out, p): out = current matrix x (p, 1) (vtfm4.q C010, E100, C000 with S003 = 1), stored svl / svr.
void func_08B5A7A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], p = ctx.gpr[5];
    const std::uint32_t matrix = current_matrix(rt, ctx);
    for (std::uint32_t row = 0u; row < 3u; ++row) gt::vfpu::load_scalar(rt, ctx, row * 0x20u, p + 4u * row);
    gt::vfpu::vector_init(ctx, 0x60u, 1u, true);
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 4u + column, matrix + 0x10u * column);
    gt::vfpu::transform(ctx, 1u, 0x24u, 0u, 4u, 4u);
    gt::vfpu::store_quad_left(rt, ctx, 1u, out + 0xCu);
    gt::vfpu::store_quad_right(rt, ctx, 1u, out);
}

// func_08B5A7F0(out, v): out = current matrix x v (a 4-vector), stored svl / svr.
void func_08B5A7F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], v = ctx.gpr[5];
    const std::uint32_t matrix = current_matrix(rt, ctx);
    for (std::uint32_t row = 0u; row < 4u; ++row) gt::vfpu::load_scalar(rt, ctx, row * 0x20u, v + 4u * row);
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 4u + column, matrix + 0x10u * column);
    gt::vfpu::transform(ctx, 1u, 0x24u, 0u, 4u, 4u);
    gt::vfpu::store_quad_left(rt, ctx, 1u, out + 0xCu);
    gt::vfpu::store_quad_right(rt, ctx, 1u, out);
}

// func_08B5A75C(out, p): out = the first three lanes of current matrix x (p, 1).
void func_08B5A75C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], p = ctx.gpr[5];
    const std::uint32_t matrix = current_matrix(rt, ctx);
    for (std::uint32_t row = 0u; row < 3u; ++row) gt::vfpu::load_scalar(rt, ctx, row * 0x20u, p + 4u * row);
    gt::vfpu::vector_init(ctx, 0x60u, 1u, true);
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 4u + column, matrix + 0x10u * column);
    gt::vfpu::transform(ctx, 1u, 0x24u, 0u, 4u, 4u);
    gt::vfpu::store_scalar(rt, ctx, 0x01u, out);
    gt::vfpu::store_scalar(rt, ctx, 0x21u, out + 4u);
    gt::vfpu::store_scalar(rt, ctx, 0x41u, out + 8u);
}

// func_08B5A668(a0, out, v, transpose, t0 = flag): out = M x v for the 4x4 matrix func_08B56FAC(0x08D6D0E0, a0,
// (t0 & 0xFF) != 0) returns, M taken as stored (vtfm4.q C010, M100, C000, in the bnez's delay slot, so always) and,
// when the low byte of `transpose` is 0, transposed after it (E100), as the asm does.
void func_08B5A668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], out = ctx.gpr[5], v = ctx.gpr[6];
    const std::uint32_t transpose = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[8] &= 0xFFu;
    const std::uint32_t matrix = call_set(rt, ctx, 0x08B56FACu, {kGuCommandBlock, first, ctx.gpr[8] != 0u ? 1u : 0u});
    for (std::uint32_t column = 0u; column < 4u; ++column) gt::vfpu::load_quad(rt, ctx, 4u + column, matrix + 0x10u * column);
    for (std::uint32_t row = 0u; row < 4u; ++row) gt::vfpu::load_scalar(rt, ctx, row * 0x20u, v + 4u * row);
    gt::vfpu::transform(ctx, 1u, 0x04u, 0u, 4u, 4u);
    if (transpose == 0u) gt::vfpu::transform(ctx, 1u, 0x24u, 0u, 4u, 4u);
    gt::vfpu::store_quad_left(rt, ctx, 1u, out + 0xCu);
    gt::vfpu::store_quad_right(rt, ctx, 1u, out);
}

// func_08B498AC(f12 = angle, s_out, c_out): sine and cosine of the angle by a polynomial (coefficients at 0x08CB69B0) on
// the VFPU, the angle first reduced against pi/2, pi, 1/pi and 2pi (vcst). One VFPU instruction a line, prefixes
// (vpfxt) included, as the asm runs them; S000 is stored to s_out and S001 to c_out.
void func_08B498AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    using B = v::Binary;
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
    v::constant(ctx, 0x01u, 1u, 8u);            // vcst.s S010, VFPU_PI_2
    v::load_half(ctx, 0x40u, 0x3800u);          // vfim.s S002, 0.5
    v::constant(ctx, 0x21u, 1u, 9u);            // vcst.s S011, VFPU_PI
    v::move_to(ctx, ctx.gpr[2], 0x20u);         // mtv v0, S001
    v::constant(ctx, 0x41u, 1u, 6u);            // vcst.s S012, VFPU_1_PI
    v::constant(ctx, 0x61u, 1u, 15u);           // vcst.s S013, VFPU_2PI
    v::binary(ctx, B::Sub, 0x00u, 0x20u, 0x01u, 1u);  // vsub.s S000, S001, S010
    v::binary(ctx, B::Mul, 0x41u, 0x41u, 0x40u, 1u);  // vmul.s S012, S012, S002
    ctx.gpr[2] = lui_of(0x08CB69B0u);
    v::load_quad(rt, ctx, 0x02u, 0x08CB69B0u);  // lv.q C020
    v::unary(ctx, 1u, 0x00u, 0x00u, 2u);         // vabs.p C000, C000
    v::scale(ctx, 0x40u, 0x00u, 0x41u, 2u);      // vscl.p C002, C000, S012
    v::float_to_int(ctx, 17u, 0x40u, 0x40u, 0u, 2u);  // vf2iz.p C002, C002, 0
    v::int_to_float(ctx, 0x40u, 0x40u, 0u, 2u);  // vi2f.p C002, C002, 0
    v::scale(ctx, 0x40u, 0x40u, 0x61u, 2u);      // vscl.p C002, C002, S013
    v::prefix(ctx, 1u, 0xE5u);                   // vpfxt Y, Y, Z, W
    v::binary(ctx, B::Sub, 0x00u, 0x00u, 0x01u, 2u);  // vsub.p C000, C000, C010
    v::binary(ctx, B::Sub, 0x00u, 0x00u, 0x40u, 2u);  // vsub.p C000, C000, C002
    v::unary(ctx, 1u, 0x00u, 0x00u, 2u);         // vabs.p C000, C000
    v::prefix(ctx, 1u, 0xE0u);                   // vpfxt X, X, Z, W
    v::binary(ctx, B::Sub, 0x00u, 0x00u, 0x01u, 2u);  // vsub.p C000, C000, C010
    v::binary(ctx, B::Mul, 0x01u, 0x00u, 0x00u, 2u);  // vmul.p C010, C000, C000
    v::scale(ctx, 0x40u, 0x01u, 0x62u, 2u);      // vscl.p C002, C010, S023
    v::binary(ctx, B::Mul, 0x41u, 0x01u, 0x00u, 2u);  // vmul.p C012, C010, C000
    v::prefix(ctx, 1u, 0xE0u);                   // vpfxt X, X, Z, W
    v::binary(ctx, B::Add, 0x40u, 0x40u, 0x42u, 2u);  // vadd.p C002, C002, C022
    v::binary(ctx, B::Mul, 0x40u, 0x40u, 0x01u, 2u);  // vmul.p C002, C002, C010
    v::prefix(ctx, 1u, 0xE5u);                   // vpfxt Y, Y, Z, W
    v::binary(ctx, B::Add, 0x40u, 0x40u, 0x02u, 2u);  // vadd.p C002, C002, C020
    v::binary(ctx, B::Mul, 0x40u, 0x40u, 0x01u, 2u);  // vmul.p C002, C002, C010
    v::prefix(ctx, 1u, 0xE0u);                   // vpfxt X, X, Z, W
    v::binary(ctx, B::Add, 0x40u, 0x40u, 0x02u, 2u);  // vadd.p C002, C002, C020
    v::binary(ctx, B::Mul, 0x40u, 0x40u, 0x41u, 2u);  // vmul.p C002, C002, C012
    v::binary(ctx, B::Add, 0x00u, 0x40u, 0x00u, 2u);  // vadd.p C000, C002, C000
    v::store_scalar(rt, ctx, 0x00u, ctx.gpr[4]);
    v::store_scalar(rt, ctx, 0x20u, ctx.gpr[5]);
}

// The shared start of func_08B4F2E4 / func_08B4F3B0 / func_08B4F47C (their first 38 instructions): the matrix at a0 in
// VFPU M200, and the sine and cosine of f12 in C100 by the polynomial of func_08B498AC (coefficients at `coefficients`).
void rotation_start(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t coefficients,
                    bool load_matrix = true) {
    namespace v = gt::vfpu;
    using B = v::Binary;
    const std::uint32_t m = ctx.gpr[4];
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
    v::constant(ctx, 0x00u, 1u, 8u);            // vcst.s S000, VFPU_PI_2
    v::load_half(ctx, 0x44u, 0x3800u);          // vfim.s S102, 0.5
    v::constant(ctx, 0x20u, 1u, 9u);            // vcst.s S001, VFPU_PI
    v::move_to(ctx, ctx.gpr[2], 0x24u);         // mtv v0, S101
    v::constant(ctx, 0x40u, 1u, 6u);            // vcst.s S002, VFPU_1_PI
    v::constant(ctx, 0x60u, 1u, 15u);           // vcst.s S003, VFPU_2PI
    v::binary(ctx, B::Sub, 0x04u, 0x24u, 0x00u, 1u);  // vsub.s S100, S101, S000
    v::binary(ctx, B::Mul, 0x40u, 0x40u, 0x44u, 1u);  // vmul.s S002, S002, S102
    ctx.gpr[2] = lui_of(coefficients);
    if (load_matrix)
        for (std::uint32_t column = 0u; column < 4u; ++column) v::load_quad(rt, ctx, 8u + column, m + 0x10u * column);
    v::load_quad(rt, ctx, 0x01u, coefficients);  // lv.q C010
    v::unary(ctx, 1u, 0x04u, 0x04u, 2u);         // vabs.p C100, C100
    v::scale(ctx, 0x44u, 0x04u, 0x40u, 2u);      // vscl.p C102, C100, S002
    v::float_to_int(ctx, 17u, 0x44u, 0x44u, 0u, 2u);  // vf2iz.p C102, C102, 0
    v::int_to_float(ctx, 0x44u, 0x44u, 0u, 2u);  // vi2f.p C102, C102, 0
    v::scale(ctx, 0x44u, 0x44u, 0x60u, 2u);      // vscl.p C102, C102, S003
    v::prefix(ctx, 1u, 0xE5u);                   // vpfxt Y, Y, Z, W
    v::binary(ctx, B::Sub, 0x04u, 0x04u, 0x00u, 2u);  // vsub.p C100, C100, C000
    v::binary(ctx, B::Sub, 0x04u, 0x04u, 0x44u, 2u);  // vsub.p C100, C100, C102
    v::unary(ctx, 1u, 0x04u, 0x04u, 2u);         // vabs.p C100, C100
    v::prefix(ctx, 1u, 0xE0u);                   // vpfxt X, X, Z, W
    v::binary(ctx, B::Sub, 0x04u, 0x04u, 0x00u, 2u);  // vsub.p C100, C100, C000
    v::binary(ctx, B::Mul, 0x00u, 0x04u, 0x04u, 2u);  // vmul.p C000, C100, C100
    v::scale(ctx, 0x44u, 0x00u, 0x61u, 2u);      // vscl.p C102, C000, S013
    v::binary(ctx, B::Mul, 0x40u, 0x00u, 0x04u, 2u);  // vmul.p C002, C000, C100
    v::prefix(ctx, 1u, 0xE0u);                   // vpfxt X, X, Z, W
    v::binary(ctx, B::Add, 0x44u, 0x44u, 0x41u, 2u);  // vadd.p C102, C102, C012
    v::binary(ctx, B::Mul, 0x44u, 0x44u, 0x00u, 2u);  // vmul.p C102, C102, C000
    v::prefix(ctx, 1u, 0xE5u);                   // vpfxt Y, Y, Z, W
    v::binary(ctx, B::Add, 0x44u, 0x44u, 0x01u, 2u);  // vadd.p C102, C102, C010
    v::binary(ctx, B::Mul, 0x44u, 0x44u, 0x00u, 2u);  // vmul.p C102, C102, C000
    v::prefix(ctx, 1u, 0xE0u);                   // vpfxt X, X, Z, W
    v::binary(ctx, B::Add, 0x44u, 0x44u, 0x01u, 2u);  // vadd.p C102, C102, C010
    v::binary(ctx, B::Mul, 0x44u, 0x44u, 0x40u, 2u);  // vmul.p C102, C102, C002
}

// Their shared end: C030 = (0, 0, 0, 1), M100 = E200 x M000 (vmmul.q), stored back to the matrix at a0.
void rotation_end(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t m = ctx.gpr[4];
    gt::vfpu::unit(ctx, 0x03u, 4u);              // vidt.q C030
    gt::vfpu::matrix_multiply(ctx, 4u, 0x28u, 0u, 4u);
    gt::vfpu::store_quad(rt, ctx, 7u, m + 0x30u);
    gt::vfpu::store_quad(rt, ctx, 4u, m);
    gt::vfpu::store_quad(rt, ctx, 5u, m + 0x10u);
    gt::vfpu::store_quad(rt, ctx, 6u, m + 0x20u);
}

// func_08B4F2E4(m, f12 = angle): m = m rotated by the angle about the first axis (rows (1, 0, 0), (0, c, s),
// (0, -s, c) built by vidt and source prefixes; coefficients at 0x08CB6A10).
void func_08B4F2E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    rotation_start(rt, ctx, 0x08CB6A10u);
    v::unit(ctx, 0x00u, 4u);                     // vidt.q C000
    v::binary(ctx, v::Binary::Add, 0x04u, 0x44u, 0x04u, 2u);  // vadd.p C100, C102, C100
    v::prefix(ctx, 0u, 0x09004u);                // vpfxs 0, Y, X, 0
    v::unary(ctx, 0u, 0x01u, 0x04u, 4u);         // vmov.q C010, C100
    v::prefix(ctx, 0u, 0x29010u);                // vpfxs 0, -X, Y, 0
    v::unary(ctx, 0u, 0x02u, 0x04u, 4u);         // vmov.q C020, C100
    rotation_end(rt, ctx);
}

// func_08B4F3B0(m, f12 = angle): the same about the second axis (coefficients at 0x08CB6A20).
void func_08B4F3B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    rotation_start(rt, ctx, 0x08CB6A20u);
    v::binary(ctx, v::Binary::Add, 0x04u, 0x44u, 0x04u, 2u);  // vadd.p C100, C102, C100
    v::prefix(ctx, 0u, 0x4A001u);                // vpfxs
    v::unary(ctx, 0u, 0x00u, 0x04u, 4u);         // vmov.q C000, C100
    v::unit(ctx, 0x01u, 4u);                     // vidt.q C010
    v::prefix(ctx, 0u, 0x0A010u);                // vpfxs
    v::unary(ctx, 0u, 0x02u, 0x04u, 4u);         // vmov.q C020, C100
    rotation_end(rt, ctx);
}

// func_08B4F47C(m, f12 = angle): the same about the third axis (coefficients at 0x08CB6A30).
void func_08B4F47C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    rotation_start(rt, ctx, 0x08CB6A30u);
    v::binary(ctx, v::Binary::Add, 0x04u, 0x44u, 0x04u, 2u);  // vadd.p C100, C102, C100
    v::prefix(ctx, 0u, 0x0C001u);                // vpfxs
    v::unary(ctx, 0u, 0x00u, 0x04u, 4u);         // vmov.q C000, C100
    v::prefix(ctx, 0u, 0x1C004u);                // vpfxs
    v::unary(ctx, 0u, 0x01u, 0x04u, 4u);         // vmov.q C010, C100
    v::unit(ctx, 0x02u, 4u);                     // vidt.q C020
    rotation_end(rt, ctx);
}

// func_08B4F174(m, f12 = angle): m = the rotation by the angle about the second axis, built directly (the start of
// func_08B4F3B0 without its input matrix; coefficients at 0x08CB69F0), stored C030, C000, C010, C020.
void func_08B4F174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    const std::uint32_t m = ctx.gpr[4];
    rotation_start(rt, ctx, 0x08CB69F0u, false);
    v::binary(ctx, v::Binary::Add, 0x04u, 0x44u, 0x04u, 2u);  // vadd.p C100, C102, C100
    v::prefix(ctx, 0u, 0x4A001u);                // vpfxs Y, 0, -X, 0
    v::unary(ctx, 0u, 0x00u, 0x04u, 4u);         // vmov.q C000, C100
    v::unit(ctx, 0x01u, 4u);                     // vidt.q C010
    v::prefix(ctx, 0u, 0x0A010u);                // vpfxs X, 0, Y, 0
    v::unary(ctx, 0u, 0x02u, 0x04u, 4u);         // vmov.q C020, C100
    v::unit(ctx, 0x03u, 4u);                     // vidt.q C030
    v::store_quad(rt, ctx, 3u, m + 0x30u);
    v::store_quad(rt, ctx, 0u, m);
    v::store_quad(rt, ctx, 1u, m + 0x10u);
    v::store_quad(rt, ctx, 2u, m + 0x20u);
}

// One step of func_08B542AC's test: the transformed point in C200 (w in its last lane) is compared with -w and w
// (vslt / vsge with prefixes), the per-axis outside flags gather in C110 / C100 (vmax), their minimum goes to C200 and
// vcmp.t ez sets condition bit 4.
void frustum_step(psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    v::prefix(ctx, 1u, 0xFFu);                   // vpfxt W, W, W, W
    v::compare3(ctx, 0x07u, 0x08u, 0x08u, 4u, 7u);  // vslt.q C130, C200, C200
    v::prefix(ctx, 1u, 0x700FFu);                // vpfxt -W, -W, -W, W
    v::compare3(ctx, 0x06u, 0x08u, 0x08u, 4u, 6u);  // vsge.q C120, C200, C200
    v::min_max(ctx, 0x05u, 0x05u, 0x07u, 3u, true); // vmax.t C110, C110, C130
}

// func_08B542AC(count, points): each of `count` points (3 floats each) is transformed by the matrix at 0x08D6E090
// (vhtfm4 with E000) and compared with its w; the per-plane flags are folded with vmax / vmin and vcmp.t ez sets
// condition bit 4. Returns 1 as soon as bit 4 is clear (bvf, tested before each point after the first), else 0 - by its
// shape a view-volume test (inferred, not named).
void func_08B542AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    std::uint32_t points = ctx.gpr[5];
    ctx.gpr[2] = 0x08D6E090u;
    for (std::uint32_t column = 0u; column < 4u; ++column) v::load_quad(rt, ctx, column, 0x08D6E090u + 0x10u * column);
    v::load_scalar(rt, ctx, 0x09u, points);
    v::load_scalar(rt, ctx, 0x29u, points + 4u);
    v::load_scalar(rt, ctx, 0x49u, points + 8u);
    v::compare(ctx, 0x05u, 0x05u, 1u, 4u);       // vcmp.s tr, S110, S110
    v::vector_init(ctx, 0x05u, 4u, false);       // vzero.q C110
    v::transform(ctx, 0x08u, 0x20u, 0x09u, 4u, 3u);  // vhtfm4.q C200, E000, C210
    std::uint32_t left = ctx.gpr[4] - 2u;
    v::vector_init(ctx, 0x04u, 4u, false);       // vzero.q C100 (the bltz's delay slot)
    const auto finish = [&](std::uint32_t result) {
        ctx.gpr[4] = left;
        ctx.gpr[5] = points;
        ctx.gpr[2] = result;
    };
    if (static_cast<std::int32_t>(left) >= 0) {
        bool more = true;
        while (more) {
            if (!v::condition(ctx, 4u)) return finish(1u);
            frustum_step(ctx);
            v::load_scalar(rt, ctx, 0x09u, points + 0xCu);
            v::min_max(ctx, 0x04u, 0x04u, 0x06u, 3u, true);   // vmax.t C100, C100, C120
            v::load_scalar(rt, ctx, 0x29u, points + 0x10u);
            v::load_scalar(rt, ctx, 0x49u, points + 0x14u);
            v::min_max(ctx, 0x08u, 0x05u, 0x04u, 3u, false);  // vmin.t C200, C110, C100
            points += 0xCu;
            v::compare(ctx, 0x08u, 0x08u, 3u, 8u);            // vcmp.t ez, C200, C200
            v::transform(ctx, 0x08u, 0x20u, 0x09u, 4u, 3u);   // vhtfm4.q C200, E000, C210
            more = left != 0u;
            left -= 1u;  // the bnez's delay slot
        }
        if (!v::condition(ctx, 4u)) return finish(1u);
    }
    frustum_step(ctx);
    v::min_max(ctx, 0x04u, 0x04u, 0x06u, 3u, true);   // vmax.t C100, C100, C120
    v::min_max(ctx, 0x08u, 0x05u, 0x04u, 3u, false);  // vmin.t C200, C110, C100
    v::compare(ctx, 0x08u, 0x08u, 3u, 8u);            // vcmp.t ez, C200, C200
    finish(v::condition(ctx, 4u) ? 0u : 1u);
}

}  // namespace
}  // namespace hot_batch_105
}  // namespace gt::game

#define HOT105(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_105::name, gt::ReturnKind::kind)

// HOT105REGS
HOT105(0x08B4E724u, func_08B4E724, Word);
HOT105(0x08B4D0C8u, func_08B4D0C8, Word);
HOT105(0x08B4ABCCu, func_08B4ABCC, Float);
HOT105(0x08B4AC54u, func_08B4AC54, Word);
HOT105(0x08B4AD90u, func_08B4AD90, Word);
HOT105(0x08B572C8u, func_08B572C8, Word);
HOT105(0x08B4BB00u, func_08B4BB00, Word);
HOT105(0x08B4F71Cu, func_08B4F71C, Float);
HOT105(0x08B4C0F8u, func_08B4C0F8, Word);
HOT105(0x08B54288u, func_08B54288, Word);
HOT105(0x08B58EE4u, func_08B58EE4, Word);
HOT105(0x08B5730Cu, func_08B5730C, Word);
HOT105(0x08B4FE74u, func_08B4FE74, Word);
HOT105(0x08B4E648u, func_08B4E648, Word);
HOT105(0x08B4E734u, func_08B4E734, Word);
HOT105(0x08B57E40u, func_08B57E40, Word);
HOT105(0x08B4A774u, func_08B4A774, Word);
HOT105(0x08B4ACE0u, func_08B4ACE0, Word);
HOT105(0x08B4E994u, func_08B4E994, Word);
HOT105(0x08B54474u, func_08B54474, Word);
HOT105(0x08B5A838u, func_08B5A838, Word);
HOT105(0x08B580F0u, func_08B580F0, Word);
HOT105(0x08B5436Cu, func_08B5436C, Word);
HOT105(0x08B5A7A8u, func_08B5A7A8, Word);
HOT105(0x08B5A7F0u, func_08B5A7F0, Word);
HOT105(0x08B5A75Cu, func_08B5A75C, Word);
HOT105(0x08B5A668u, func_08B5A668, Word);
HOT105(0x08B498ACu, func_08B498AC, Word);
HOT105(0x08B4F2E4u, func_08B4F2E4, Word);
HOT105(0x08B4F3B0u, func_08B4F3B0, Word);
HOT105(0x08B4F47Cu, func_08B4F47C, Word);
HOT105(0x08B4F174u, func_08B4F174, Word);
HOT105(0x08B542ACu, func_08B542AC, Word);
// HOT105(0x08B5A344u, func_08B5A344, Word);  // not written: the graphics library one-time init (11 calls, at boot), which registers GE callbacks through imports a case refuses
// HOT105(0x08B599E0u, func_08B599E0, Word);  // not written: it waits for vblank (0x08C7AC18) a0 times, which a case refuses
