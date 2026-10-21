// Hot batch 107: small functions the 11-route gate reaches for the first time, written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name. Several of them are listed longer than they are: their code ends
// at a tail jump and the rest belongs to the next function.
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
namespace hot_batch_107 {
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





// func_08C45EA8(a0, f12) -> the pair (a0, the float's bits) in v0 / v1 (through a stack word).
void func_08C45EA8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4];
    ctx.gpr[3] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
}
// func_08C45DC0(a0, f12) -> the pair (the float's bits, a0) in v0 / v1.
void func_08C45DC0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = ctx.gpr[4];
    ctx.gpr[2] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
}
// func_08AF6D14(p) -> whether the word at p is 0x74585333 (it adds 0x8BA7ACCD and tests for 0).
void func_08AF6D14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = 0x8BA7ACCDu;
    ctx.gpr[2] = word(rt, ctx.gpr[4]) + 0x8BA7ACCDu == 0u ? 1u : 0u;
}
// func_08C460C4(a0) -> sqrt of func_08BBBEC4(a0, a0) (f0).
void func_08C460C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x08BBBEC4u, {ctx.gpr[4], ctx.gpr[4]});
    ctx.fpr[0] = std::sqrt(ctx.fpr[0]);
}
// func_088D2D10(this) -> func_088DB56C(*this).
void func_088D2D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x088DB56Cu, {word(rt, ctx.gpr[4])});
}
// func_08893768(this) -> *this + (signed byte at *this) x 0x26D0 + 0xFD8 (madd through hi / lo).
void func_08893768(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = word(rt, ctx.gpr[4]);
    ctx.gpr[3] = 0x26D0u;
    ctx.gpr[4] = sext8(byte_u(rt, base));
    const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32 | base) +
        static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(ctx.gpr[4])) * 0x26D0);
    ctx.lo = static_cast<std::uint32_t>(accumulator);
    ctx.hi = static_cast<std::uint32_t>(accumulator >> 32);
    ctx.gpr[2] = ctx.lo + 0xFD8u;
}
// func_08AFD524(this, n): the half at +0xE6 = n + 1 when n + 1 is below it (signed); v0 = that comparison.
void func_08AFD524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t limit = sext16(half_u(rt, ctx.gpr[4] + 0xE6u));
    ctx.gpr[5] += 1u;
    ctx.gpr[2] = signed_of(ctx.gpr[5]) < signed_of(limit) ? 1u : 0u;
    if (ctx.gpr[2] != 0u) put16(rt, ctx.gpr[4] + 0xE6u, ctx.gpr[5]);
}
// func_088981E0(this) -> func_08898110(this + 0x20).
void func_088981E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x08898110u, {ctx.gpr[4] + 0x20u});
}
// func_088A13F4(this, i, value): the word array at this->+0x20 gets value at index i.
void func_088A13F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x20u);
    ctx.gpr[5] = (ctx.gpr[5] << 2) + ctx.gpr[2];
    put(rt, ctx.gpr[5], ctx.gpr[6]);
}
// func_088A41CC(this) -> func_088A5F00(this + 8).
void func_088A41CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x088A5F00u, {ctx.gpr[4] + 8u});
}
// func_089833AC(this) -> (the float at +0xA0 + 21) x the float at +0xC0.
void func_089833AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[1] = 0x41A80000u;
    ctx.fpr[1] = 21.0f;
    ctx.fpr[2] = float_at(rt, ctx.gpr[4] + 0xC0u);
    ctx.fpr[0] = psp_mul(float_at(rt, ctx.gpr[4] + 0xA0u) + ctx.fpr[1], ctx.fpr[2]);
}
// func_089178AC: the object D_08CFFAAC's word +0x18 = 1.
void func_089178AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = lui_of(0x08CFFAACu);
    ctx.gpr[4] = word(rt, 0x08CFFAACu);
    ctx.gpr[3] = 1u;
    put(rt, ctx.gpr[4] + 0x18u, 1u);
}
// func_088D4E08(this) -> whether the signed byte at (*this)+0x60 is not 0.
void func_088D4E08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = sext8(byte_u(rt, ctx.gpr[3] + 0x60u)) != 0u ? 1u : 0u;
}
// func_088FD9D0() -> the byte at +0x10 of what func_088FD9AC() returns.
void func_088FD9D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_set(rt, ctx, 0x088FD9ACu, {});
    ctx.gpr[2] = byte_u(rt, object + 0x10u);
}
// func_08917E80(this, a1, a2, i): continue in func_08927550 with a0 = the word at this + i x 4 + 8.
void func_08917E80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[7] = (ctx.gpr[7] << 2) + ctx.gpr[4];
    tail_call(ctx, 0x08927550u, {word(rt, ctx.gpr[7] + 8u)});
}
// func_0885EF0C(this): continue in func_08865F4C with (*this) + 0x3F58.
void func_0885EF0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08865F4Cu, {word(rt, ctx.gpr[4]) + 0x3F58u});
}
// func_08891544(this) -> func_088910C0(*this).
void func_08891544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x088910C0u, {word(rt, ctx.gpr[4])});
}
// func_088915E0(this): continue in func_0889111C with *this.
void func_088915E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x0889111Cu, {word(rt, ctx.gpr[4])});
}
// func_0889604C(this): the byte at +0xA8 = 1, then func_08895B80(this + 0x70).
void func_0889604C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 1u;
    put8(rt, ctx.gpr[4] + 0xA8u, 1u);
    tail_call(ctx, 0x08895B80u, {ctx.gpr[4] + 0x70u});
}
// func_088CEF38(this, value): +0x98 = 1, +0x10 = value.
void func_088CEF38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 1u;
    put(rt, ctx.gpr[4] + 0x98u, 1u);
    put(rt, ctx.gpr[4] + 0x10u, ctx.gpr[5]);
}
// func_088D61A4() -> whether func_088D6114() is not 0.
void func_088D61A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_set(rt, ctx, 0x088D6114u, {}) != 0u ? 1u : 0u;
}
// func_0885F43C(this): continue in func_0885F430 with *this.
void func_0885F43C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x0885F430u, {word(rt, ctx.gpr[4])});
}
// func_08891534(this): the byte at (*this)+0x11 = 1.
void func_08891534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = 1u;
    put8(rt, ctx.gpr[3] + 0x11u, 1u);
}

}  // namespace
}  // namespace hot_batch_107
}  // namespace gt::game

#define HOT107(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_107::name, gt::ReturnKind::kind)

// HOT107REGS
HOT107(0x08C45EA8u, func_08C45EA8, DoubleWord);
HOT107(0x08C45DC0u, func_08C45DC0, DoubleWord);
HOT107(0x08AF6D14u, func_08AF6D14, Word);
HOT107(0x08C460C4u, func_08C460C4, Float);
HOT107(0x088D2D10u, func_088D2D10, Word);
HOT107(0x08893768u, func_08893768, Word);
HOT107(0x08AFD524u, func_08AFD524, Word);
HOT107(0x088981E0u, func_088981E0, Word);
HOT107(0x088A13F4u, func_088A13F4, Word);
HOT107(0x088A41CCu, func_088A41CC, Word);
HOT107(0x089833ACu, func_089833AC, Float);
HOT107(0x089178ACu, func_089178AC, Word);
HOT107(0x088D4E08u, func_088D4E08, Word);
HOT107(0x088FD9D0u, func_088FD9D0, Word);
HOT107(0x08917E80u, func_08917E80, Word);
HOT107(0x0885EF0Cu, func_0885EF0C, Word);
HOT107(0x08891544u, func_08891544, Word);
HOT107(0x088915E0u, func_088915E0, Word);
HOT107(0x0889604Cu, func_0889604C, Word);
HOT107(0x088CEF38u, func_088CEF38, Word);
HOT107(0x088D61A4u, func_088D61A4, Word);
HOT107(0x0885F43Cu, func_0885F43C, Word);
HOT107(0x08891534u, func_08891534, Word);
