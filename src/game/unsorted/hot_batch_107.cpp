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

// Byte-reversed word (wsbw).
std::uint32_t swapped(std::uint32_t value) { return __builtin_bswap32(value); }

// A tail call through the vtable of `object` (its slot at `slot`), as `lw v0, 0(object); lw t9, slot(v0); jr t9`.
void tail_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[25] = word(rt, ctx.gpr[2] + slot);
    tail_call(ctx, ctx.gpr[25], {object});
}

// The `call target(this, a1, k); return this` shape (`addiu a2, zero, k` before the jal).
void call_k_return_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t k) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[6] = k;
    (void)call_set(rt, ctx, target, {self});
    ctx.gpr[2] = self;
}
void func_08BB0EA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 1u); }
void func_08BB1898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 0xB4u); }
void func_08BB18C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 0xC8u); }
void func_08BB18F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 0xA8u); }
void func_08BB1694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 1u); }
void func_08BB0ED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 8u); }
void func_08BB0EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 1u); }
void func_08BB1668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 4u); }
void func_08BB16C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088BA534u, 1u); }
void func_08BB8B40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088F19DCu, 1u); }
void func_08BB8B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088F19DCu, 1u); }
void func_08BB8B6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088F19DCu, 4u); }
void func_08BB8D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088F19DCu, 8u); }
void func_08BB8E40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_k_return_self(rt, ctx, 0x088F19DCu, 0x40u); }

// func_08BBBECC(out, v, s): out = v x *s (three floats).
void func_08BBBECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], v = ctx.gpr[5];
    const float s = float_at(rt, ctx.gpr[6]);
    ctx.fpr[2] = s;
    ctx.fpr[1] = psp_mul(s, float_at(rt, v));
    put_float(rt, out, ctx.fpr[1]);
    ctx.fpr[0] = psp_mul(s, float_at(rt, v + 4u));
    put_float(rt, out + 4u, ctx.fpr[0]);
    ctx.fpr[1] = float_at(rt, v + 8u);
    ctx.fpr[2] = psp_mul(s, ctx.fpr[1]);
    put_float(rt, out + 8u, ctx.fpr[2]);
}
// func_08925CA0(this) -> func_08B2213C(this->+4 + this->+0xC x 0x268, a1, this->+0xC) (madd through hi / lo).
void func_08925CA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[3] = self;
    ctx.gpr[2] = 0x268u;
    const std::uint32_t base = word(rt, self + 4u);
    ctx.gpr[6] = word(rt, self + 0xCu);
    const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32 | base) +
        static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(ctx.gpr[6])) * 0x268);
    ctx.lo = static_cast<std::uint32_t>(accumulator);
    ctx.hi = static_cast<std::uint32_t>(accumulator >> 32);
    (void)call_set(rt, ctx, 0x08B2213Cu, {ctx.lo});
}
// func_088A3E80(this) -> 0 when this->+0x18 is null, else the sign bit of the byte at +0x2F of func_0893C780(it).
void func_088A3E80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[3] = 0u;
    const std::uint32_t member = word(rt, ctx.gpr[4] + 0x18u);
    ctx.gpr[4] = member;
    if (member != 0u) {
        const std::uint32_t object = call_set(rt, ctx, 0x0893C780u, {member});
        ctx.gpr[3] = sext8(byte_u(rt, object + 0x2Fu)) >> 31;
    }
    ctx.gpr[2] = ctx.gpr[3];
}
// func_08AF2724() -> 0, or (when func_08AF26E4() is not null) func_08AF1460(it).
void func_08AF2724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        object = call_set(rt, ctx, 0x08AF26E4u, {});
        ctx.gpr[4] = object;
    }
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08AF1460u, {object});
}
// func_088CFA54(this) -> this->+0xB0 unless +0x9C is set and the mode at +0x12C is neither 2 nor 3; then
// func_088CFA28(this).
void func_088CFA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[3] = self;
    ctx.gpr[2] = word(rt, self + 0x9Cu);
    if (ctx.gpr[2] != 0u) {
        ctx.gpr[5] = word(rt, self + 0x12Cu);
        ctx.gpr[2] = 3u;
        if (ctx.gpr[5] != 3u) {
            ctx.gpr[2] = 2u;
            if (ctx.gpr[5] != 2u) return tail_call(ctx, 0x088CFA28u, {});
        }
    }
    ctx.gpr[2] = word(rt, self + 0xB0u);
}
// func_088CEFE8(this) -> the float at +0xDC when the mode at +0x12C is 2 or 3, else the one at +0xE4.
void func_088CEFE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x12Cu) - 2u < 2u ? 1u : 0u;
    ctx.set_fpr_bits(0u, word(rt, ctx.gpr[4] + (ctx.gpr[2] != 0u ? 0xDCu : 0xE4u)));
}
// func_08C4ACD0(v, s): v x= *s (three floats; *s is read again after each store, as the asm does).
void func_08C4ACD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t v = ctx.gpr[4], s = ctx.gpr[5];
    ctx.fpr[1] = float_at(rt, s);
    ctx.fpr[0] = float_at(rt, v);
    ctx.fpr[2] = float_at(rt, v + 4u);
    ctx.fpr[3] = float_at(rt, v + 8u);
    ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]);
    put_float(rt, v, ctx.fpr[0]);
    ctx.fpr[1] = float_at(rt, s);
    ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[1]);
    put_float(rt, v + 4u, ctx.fpr[2]);
    ctx.fpr[0] = float_at(rt, s);
    ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[0]);
    put_float(rt, v + 8u, ctx.fpr[3]);
}
// func_0894F1A0(i) -> the float table at 0x08CF2F2C, entry i - 1 (entry 9 when i - 1 is not below 10).
void func_0894F1A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t index = ctx.gpr[4] - 1u;
    ctx.gpr[3] = index < 10u ? 1u : 0u;
    if (ctx.gpr[3] == 0u) index = 9u;
    ctx.gpr[2] = 0x08CF2F2Cu;
    ctx.gpr[4] = (index << 2) + 0x08CF2F2Cu;
    ctx.set_fpr_bits(0u, word(rt, ctx.gpr[4]));
}
// func_08BBBE68(to, from): copies four floats.
void func_08BBBE68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t bits = word(rt, ctx.gpr[5] + 4u * i);
        ctx.set_fpr_bits((i & 1u) == 0u ? 1u : 0u, bits);
        put(rt, ctx.gpr[4] + 4u * i, bits);
    }
}
// func_08BAE364(to, from): to = -from (three floats).
void func_08BAE364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (std::uint32_t i = 0u; i < 3u; ++i) {
        const float value = -float_at(rt, ctx.gpr[5] + 4u * i);
        ctx.fpr[(i & 1u) == 0u ? 1u : 0u] = value;
        put_float(rt, ctx.gpr[4] + 4u * i, value);
    }
}
// func_08ABB5F0(holder): when holder and *holder are set, the virtual +0x38 of *holder (a1 = holder).
void func_08ABB5F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    ctx.gpr[5] = holder;
    if (holder == 0u) return;
    ctx.gpr[2] = word(rt, holder);
    ctx.gpr[4] = ctx.gpr[2];
    if (ctx.gpr[2] == 0u) return;
    tail_virtual(rt, ctx, ctx.gpr[2], 0x38u);
}
// func_08AD58D8(this): sets the vtable 0x08CD6EA0, func_08AD5834(this), then func_08AD7DF8(this).
void func_08AD58D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0x08CD6EA0u;
        put(rt, self, 0x08CD6EA0u);
        (void)call_set(rt, ctx, 0x08AD5834u, {self});
    }
    tail_call(ctx, 0x08AD7DF8u, {self});
}
// func_088DAF90(this) -> -1 unless this->+0x14 is set and the byte at +0x1E is not 0; then func_088FE620(+0x14).
void func_088DAF90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[3] = self;
    ctx.gpr[4] = word(rt, self + 0x14u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (ctx.gpr[4] == 0u) return;
    ctx.gpr[3] = byte_u(rt, self + 0x1Eu);
    if (ctx.gpr[3] == 0u) return;
    tail_call(ctx, 0x088FE620u, {});
}
// func_08A04C14(this) -> func_08A105B8(this), then this->+0xB4 = 0.
void func_08A04C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x08A105B8u, {self});
    put(rt, self + 0xB4u, 0u);
}
// func_08BE9070(this) -> this, after func_08AA27A4(this).
void func_08BE9070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x08AA27A4u, {self});
    ctx.gpr[2] = self;
}
// func_088A2240(this, a1, base) -> base + func_088A1954() after func_0894D6D0(this->+0x54).
void func_088A2240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x0894D6D0u, {word(rt, ctx.gpr[4] + 0x54u)});
    ctx.gpr[2] = base + call_set(rt, ctx, 0x088A1954u, {});
}
// func_088D08B8(this, flag): the byte at +0x25 = 1; when the low byte of flag is 1, +0x27 = 1 and +0x74 = +0x78.
void func_088D08B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 1u;
    ctx.gpr[5] &= 0xFFu;
    ctx.gpr[3] = 1u;
    put8(rt, ctx.gpr[4] + 0x25u, 1u);
    if (ctx.gpr[5] != 1u) return;
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x78u);
    put8(rt, ctx.gpr[4] + 0x27u, ctx.gpr[5]);
    put(rt, ctx.gpr[4] + 0x74u, ctx.gpr[2]);
}
// func_088221BC(this) -> this, after func_08A53224(this, &(the word at 0x08CE2B14)) with the word copied to a frame.
void func_088221BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = lui_of(0x08CE2B14u);
    ctx.gpr[3] = word(rt, 0x08CE2B14u);
    put(rt, frame.at(0u), ctx.gpr[3]);
    (void)call_set(rt, ctx, 0x08A53224u, {self, frame.at(0u)});
    ctx.gpr[2] = self;
}
// func_08BABD2C(a0, p): the signed half at p += func_08894D88(a0, p).
void func_08BABD2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[5];
    const std::uint32_t before = sext16(half_u(rt, p));
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t delta = call_set(rt, ctx, 0x08894D88u, {ctx.gpr[4], p});
    put16(rt, p, before + delta);
}
// func_08ABCBEC(holder, flag): when holder and *holder are set, the virtual +0x24 of *holder (a1 = holder, a2 = flag's
// low byte).
void func_08ABCBEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    ctx.gpr[6] = ctx.gpr[5] & 0xFFu;
    if (holder == 0u) return;
    ctx.gpr[2] = word(rt, holder);
    ctx.gpr[5] = holder;
    ctx.gpr[4] = ctx.gpr[2];
    if (ctx.gpr[2] == 0u) return;
    tail_virtual(rt, ctx, ctx.gpr[2], 0x24u);
}
// func_08895074 / func_08895098 / func_088950BC() -> the byte-reversed word at +0x18 / +0xC / +8 of func_08894B68().
void byte_reversed_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_set(rt, ctx, 0x08894B68u, {});
    ctx.gpr[2] = swapped(word(rt, object + offset));
}
void func_08895074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_reversed_member(rt, ctx, 0x18u); }
void func_08895098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_reversed_member(rt, ctx, 0xCu); }
void func_088950BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { byte_reversed_member(rt, ctx, 8u); }
// func_088D4D24(this): clears the four bytes at (*this)+0x58.
void func_088D4D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[5] = 4u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        ctx.gpr[2] = word(rt, ctx.gpr[4]) + i;
        put8(rt, ctx.gpr[2] + 0x58u, 0u);
    }
    ctx.gpr[3] = 4u;
}
// func_08890478(a0, a1): func_088D57A0(a1, the byte-reversed word at +0xA0 of func_0888F550(a0, a1)).
void func_08890478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[5];
    std::uint32_t object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        object = call_set(rt, ctx, 0x0888F550u, {ctx.gpr[4], second});
    }
    tail_call(ctx, 0x088D57A0u, {second, swapped(word(rt, object + 0xA0u))});
}
// func_08896564(a0, a1, base) -> base + func_08898230(a0, a1, base).
void func_08896564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = base + call_set(rt, ctx, 0x08898230u, {ctx.gpr[4], ctx.gpr[5], base});
}
// func_08945EF8(this, k) -> of this->+8's floats a = +0x724, b = +0x728: k 0 -> a - b, k 1 -> b, else a.
void func_08945EF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t k = ctx.gpr[5];
    ctx.gpr[3] = word(rt, ctx.gpr[4] + 8u);
    const std::uint32_t object = ctx.gpr[3];
    if (k == 0u) {
        ctx.fpr[1] = float_at(rt, object + 0x728u);
        ctx.fpr[0] = float_at(rt, object + 0x724u) - ctx.fpr[1];
        return;
    }
    ctx.gpr[2] = 1u;
    ctx.set_fpr_bits(0u, word(rt, object + (k != 1u ? 0x724u : 0x728u)));
}
// func_088D61E0() -> func_088D61C4() as a float (cvt.s.w).
void func_088D61E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t value = call_set(rt, ctx, 0x088D61C4u, {});
    ctx.set_fpr_bits(1u, value);
    ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(value));
}
// func_088FDA60() -> the byte-reversed word func_088FD9AC() points at.
void func_088FDA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_set(rt, ctx, 0x088FD9ACu, {});
    ctx.gpr[2] = swapped(word(rt, object));
}
// func_088A19CC(a0, base) -> base + func_088A1978(a0, base).
void func_088A19CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = base + call_set(rt, ctx, 0x088A1978u, {ctx.gpr[4], base});
}
// func_088DA030(this, base) -> base + func_088D9FD8() after func_088D2098(this->+0x618).
void func_088DA030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_set(rt, ctx, 0x088D2098u, {word(rt, ctx.gpr[4] + 0x618u)});
    ctx.gpr[2] = base + call_set(rt, ctx, 0x088D9FD8u, {});
}
// func_088DA088(this): when this->+0x5CC is set, its virtual +0x48.
void func_088DA088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4] + 0x5CCu);
    ctx.gpr[4] = ctx.gpr[2];
    if (ctx.gpr[2] == 0u) return;
    tail_virtual(rt, ctx, ctx.gpr[2], 0x48u);
}
// func_088F9420(this, from): copies the two words at from (any alignment) to this + 0x24 / + 0x28 (lwl/lwr, swl/swr).
void func_088F9420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], from = ctx.gpr[5];
    const auto copy_word = [&](std::uint32_t to, std::uint32_t at) {
        for (std::uint32_t i = 0u; i < 4u; ++i) put8(rt, to + i, byte_u(rt, at + i));
    };
    ctx.gpr[8] = rt.memory().load8(from) | rt.memory().load8(from + 1u) << 8u | rt.memory().load8(from + 2u) << 16u |
                 static_cast<std::uint32_t>(rt.memory().load8(from + 3u)) << 24u;
    copy_word(self + 0x24u, from);
    ctx.gpr[3] = rt.memory().load8(from + 4u) | rt.memory().load8(from + 5u) << 8u |
                 rt.memory().load8(from + 6u) << 16u | static_cast<std::uint32_t>(rt.memory().load8(from + 7u)) << 24u;
    copy_word(self + 0x28u, from + 4u);
    ctx.gpr[2] = from + 8u;
    ctx.gpr[5] = self + 0x24u;
    ctx.gpr[6] = from + 4u;
    ctx.gpr[7] = self + 0x28u;
}
// func_088FDD68() -> (f0) the byte-reversed word at +8 of func_088FD9AC().
void func_088FDD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_set(rt, ctx, 0x088FD9ACu, {});
    ctx.gpr[3] = swapped(word(rt, object + 8u));
    ctx.set_fpr_bits(0u, ctx.gpr[3]);
}
// func_088CEF14(this, a, b, c, d): +0..+0xC = a, b, c, d; +0x124, +0xA0 and the bytes +0xA4, +0xA5 cleared; then
// func_088CEA34(this).
void func_088CEF14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, ctx.gpr[5]);
    put(rt, self + 4u, ctx.gpr[6]);
    put(rt, self + 8u, ctx.gpr[7]);
    put(rt, self + 0xCu, ctx.gpr[8]);
    put(rt, self + 0x124u, 0u);
    put(rt, self + 0xA0u, 0u);
    put8(rt, self + 0xA4u, 0u);
    put8(rt, self + 0xA5u, 0u);
    tail_call(ctx, 0x088CEA34u, {});
}
// func_08AC0C38(a0, a1, pair, flag): func_08AC0AAC(a0, a1, frame, flag's low byte) with frame = {0, 0, pair[0], pair[1]}.
void func_08AC0C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pair = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[7] &= 0xFFu;
    ctx.gpr[3] = word(rt, pair + 4u);
    ctx.gpr[2] = word(rt, pair);
    put(rt, frame.at(0xCu), ctx.gpr[3]);
    put(rt, frame.at(8u), ctx.gpr[2]);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    (void)call_set(rt, ctx, 0x08AC0AACu, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
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
// HOT107(0x089833ACu, func_089833AC, Float);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x089178ACu, func_089178AC, Word);
// HOT107(0x088D4E08u, func_088D4E08, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x088FD9D0u, func_088FD9D0, Word);
// HOT107(0x08917E80u, func_08917E80, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x0885EF0Cu, func_0885EF0C, Word);
HOT107(0x08891544u, func_08891544, Word);
HOT107(0x088915E0u, func_088915E0, Word);
HOT107(0x0889604Cu, func_0889604C, Word);
HOT107(0x088CEF38u, func_088CEF38, Word);
HOT107(0x088D61A4u, func_088D61A4, Word);
// HOT107(0x0885F43Cu, func_0885F43C, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT107(0x08891534u, func_08891534, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x08BB0EA4u, func_08BB0EA4, Word);
HOT107(0x08BB1898u, func_08BB1898, Word);
HOT107(0x08BB18C4u, func_08BB18C4, Word);
HOT107(0x08BB18F0u, func_08BB18F0, Word);
HOT107(0x08BB1694u, func_08BB1694, Word);
HOT107(0x08BB0ED0u, func_08BB0ED0, Word);
HOT107(0x08BB0EFCu, func_08BB0EFC, Word);
HOT107(0x08BB1668u, func_08BB1668, Word);
HOT107(0x08BB16C0u, func_08BB16C0, Word);
// HOT107(0x08BB8B40u, func_08BB8B40, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT107(0x08BB8B98u, func_08BB8B98, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT107(0x08BB8B6Cu, func_08BB8B6C, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT107(0x08BB8D28u, func_08BB8D28, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT107(0x08BB8E40u, func_08BB8E40, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x08BBBECCu, func_08BBBECC, Word);
HOT107(0x08925CA0u, func_08925CA0, Word);
HOT107(0x088A3E80u, func_088A3E80, Word);
HOT107(0x08AF2724u, func_08AF2724, Word);
HOT107(0x088CFA54u, func_088CFA54, Word);
HOT107(0x088CEFE8u, func_088CEFE8, Float);
HOT107(0x08C4ACD0u, func_08C4ACD0, Word);
HOT107(0x0894F1A0u, func_0894F1A0, Float);
HOT107(0x08BBBE68u, func_08BBBE68, Word);
HOT107(0x08BAE364u, func_08BAE364, Word);
HOT107(0x08ABB5F0u, func_08ABB5F0, Word);
// HOT107(0x08AD58D8u, func_08AD58D8, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x088DAF90u, func_088DAF90, Word);
HOT107(0x08A04C14u, func_08A04C14, Word);
HOT107(0x08BE9070u, func_08BE9070, Word);
HOT107(0x088A2240u, func_088A2240, Word);
HOT107(0x088D08B8u, func_088D08B8, Word);
HOT107(0x088221BCu, func_088221BC, Word);
HOT107(0x08BABD2Cu, func_08BABD2C, Word);
HOT107(0x08ABCBECu, func_08ABCBEC, Word);
HOT107(0x08895074u, func_08895074, Word);
HOT107(0x08895098u, func_08895098, Word);
HOT107(0x088950BCu, func_088950BC, Word);
HOT107(0x088D4D24u, func_088D4D24, Word);
HOT107(0x08890478u, func_08890478, Word);
HOT107(0x08896564u, func_08896564, Word);
HOT107(0x08945EF8u, func_08945EF8, Float);
// HOT107(0x088D61E0u, func_088D61E0, Float);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x088FDA60u, func_088FDA60, Word);
HOT107(0x088A19CCu, func_088A19CC, Word);
HOT107(0x088DA030u, func_088DA030, Word);
HOT107(0x088DA088u, func_088DA088, Word);
HOT107(0x088F9420u, func_088F9420, Word);
// HOT107(0x088FDD68u, func_088FDD68, Float);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
// HOT107(0x088CEF14u, func_088CEF14, Word);  // batch 107: compared 0 in the full gate (reached by the probe, but no case of it was comparable)
HOT107(0x08AC0C38u, func_08AC0C38, Word);
