// Hot batch 105: functions of GT's graphics library region (0x08B40000-0x08B70000) the routes reach, written by hand from
// `tools.disasm`; role words describe access shapes, none is a game name. VFPU state the original leaves is left too
// (include/gt/vfpu.hpp; the harness compares the VFPU since #258).
//
// Every call passes the argument registers the original sets before the jal (`call_set`); the delay slot of a `jal`
// runs before the call; v0 is set as the original leaves it on every path; frames that callees read are reproduced
// (`GuestFrame`, offsets as in the original), the dead stack is not.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
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
// HOT105(0x08B599E0u, func_08B599E0, Word);  // not written: it waits for vblank (0x08C7AC18) a0 times, which a case refuses
