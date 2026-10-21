// Hot batch 108: functions of 15-22 instructions the 11-route gate reaches, written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
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
namespace hot_batch_108 {
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





// A virtual call `object->slot(object, ...)`: the vtable read as `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object,
                              std::uint32_t slot, std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}

// func_08925CD4(this) -> the low byte of func_08B21410(e->+0x60->+0xF4, a1, e->+0x60, this->+0xC) for the element
// e = this->+4 + this->+0xC x 0x268 (madd through hi / lo).
void func_08925CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[3] = 0x268u;
    const std::uint32_t base = word(rt, self + 4u);
    ctx.gpr[7] = word(rt, self + 0xCu);
    const std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32 | base) +
        static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(ctx.gpr[7])) * 0x268);
    ctx.lo = static_cast<std::uint32_t>(accumulator);
    ctx.hi = static_cast<std::uint32_t>(accumulator >> 32);
    ctx.gpr[6] = word(rt, ctx.lo + 0x60u);
    ctx.gpr[2] = call_set(rt, ctx, 0x08B21410u, {word(rt, ctx.gpr[6] + 0xF4u)}) & 0xFFu;
}

// func_08BAE2B0(out, a, b): out = a x b (cross product), stored z, x, y; every load comes before the stores.
void func_08BAE2B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    const float bx = float_at(rt, b), ax = float_at(rt, a), ay = float_at(rt, a + 4u), bz = float_at(rt, b + 8u);
    const float az = float_at(rt, a + 8u), by = float_at(rt, b + 4u);
    ctx.fpr[7] = psp_mul(ay, bx);
    ctx.fpr[4] = psp_mul(az, bx);
    ctx.fpr[0] = psp_mul(by, ax);
    ctx.fpr[1] = psp_mul(ay, bz);
    ctx.fpr[3] = psp_mul(bz, ax);
    ctx.fpr[2] = psp_mul(az, by);
    ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[7];
    ctx.fpr[4] = ctx.fpr[4] - ctx.fpr[3];
    ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2];
    ctx.fpr[5] = by;
    ctx.fpr[6] = bz;
    put_float(rt, out + 8u, ctx.fpr[0]);
    put_float(rt, out, ctx.fpr[1]);
    put_float(rt, out + 4u, ctx.fpr[4]);
}

// func_089E4BC4(this, out, flag): out = {0, 0, x, 0, ...} with x = this->+0x18 and y = this->+0x1C laid out in one of two
// orders: flag set -> +0x10 x, +0x14 y, +0x18 0, +0x1C y; clear -> +0x10 0, +0x14 y, +0x18 x, +0x1C y.
void func_089E4BC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5];
    const std::uint32_t x = word(rt, self + 0x18u);
    ctx.gpr[6] &= 0xFFu;
    const std::uint32_t y = word(rt, self + 0x1Cu);
    ctx.set_fpr_bits(1u, x);
    ctx.set_fpr_bits(0u, y);
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, x);
    put(rt, out + 0xCu, 0u);
    if (ctx.gpr[6] != 0u) {
        put(rt, out + 0x1Cu, y);
        put(rt, out + 0x10u, x);
        put(rt, out + 0x14u, y);
        put(rt, out + 0x18u, 0u);
    } else {
        put(rt, out + 0x1Cu, y);
        put(rt, out + 0x18u, x);
        put(rt, out + 0x10u, 0u);
        put(rt, out + 0x14u, y);
    }
}

// func_088BBDB8(this): with a frame object f: func_088C50B8(&f, this->+0x1C, the byte s->+0x10), then
// func_088C50C4(&f, func_088C5324(&f, f.word0 == byte s->+0x11 ? 0 : byte s->+0x11)), s = this->+0x24.
void func_088BBDB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t s = word(rt, self + 0x24u);
    const std::uint32_t f = frame.at(0u);
    (void)call_set(rt, ctx, 0x088C50B8u, {f, word(rt, self + 0x1Cu), byte_u(rt, s + 0x10u)});
    ctx.gpr[5] = byte_u(rt, s + 0x11u);
    ctx.gpr[2] = word(rt, f) ^ ctx.gpr[5];
    if (ctx.gpr[2] == 0u) ctx.gpr[5] = 0u;
    const std::uint32_t result = call_set(rt, ctx, 0x088C5324u, {f, ctx.gpr[5]});
    (void)call_set(rt, ctx, 0x088C50C4u, {f, result});
}

// func_08BBBEF8(out, v): out = v / sqrt(func_08BBBEC4(v)) through func_08BBBECC(out, v, &scale).
void func_08BBBEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], v = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08BBBEC4u, {v});
    ctx.fpr[0] = std::sqrt(ctx.fpr[0]);
    ctx.gpr[1] = 0x3F800000u;
    ctx.fpr[1] = 1.0f / ctx.fpr[0];
    put_float(rt, frame.at(0u), ctx.fpr[1]);
    (void)call_set(rt, ctx, 0x08BBBECCu, {out, v, frame.at(0u)});
}

// func_089032FC(out, obj, index) -> out, filled from the 12-byte entry *index of obj->+0x78: out[0] = obj->+0x84,
// out[1] = obj->+0x8C + (word8 & 0xFFFFFF) x 2, out[2] = out[1] + (word8 >> 24) x 2.
void func_089032FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], obj = ctx.gpr[5];
    const std::uint32_t index = word(rt, ctx.gpr[6]);
    const std::uint32_t entry = (index << 4) - (index << 2) + word(rt, obj + 0x78u);
    ctx.gpr[7] = word(rt, obj + 0x84u);
    const std::uint32_t packed = word(rt, entry + 8u);
    put(rt, out, ctx.gpr[7]);
    const std::uint32_t first = word(rt, obj + 0x8Cu) + ((packed & 0x00FFFFFFu) << 1);
    ctx.gpr[6] = first;
    ctx.gpr[3] = first + ((packed >> 24) << 1);
    ctx.gpr[2] = out;
    put(rt, out + 8u, ctx.gpr[3]);
    put(rt, out + 4u, first);
}

// func_08AF9500(this) -> this->+0x30 + 48 x func_08AF94CC(this), or 0 when that index is negative.
void func_08AF9500(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t index = call_set(rt, ctx, 0x08AF94CCu, {self});
    ctx.gpr[4] = (index << 6) - (index << 4);
    ctx.gpr[3] = 0u;
    if (static_cast<std::int32_t>(index) >= 0) ctx.gpr[3] = word(rt, self + 0x30u) + ctx.gpr[4];
    ctx.gpr[2] = ctx.gpr[3];
}

// func_088B1658(this, value): (*this)->virtual +0x30(*this, this->+4, frame {0x08CC3040, 6, value}).
void func_088B1658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(8u), ctx.gpr[5]);
    put(rt, frame.at(4u), 6u);
    put(rt, frame.at(0u), 0x08CC3040u);
    ctx.gpr[8] = self;
    const std::uint32_t object = word(rt, self);
    ctx.gpr[7] = object;
    (void)call_virtual_at(rt, ctx, object, 0x30u, {object, word(rt, self + 4u), frame.at(0u)});
}

// func_08AA27A4(this, holder) -> this, after (*holder)->virtual +0xC(this, *holder); a null *holder first reports
// through func_08A5BB7C(0x08CB2B50).
void func_08AA27A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = word(rt, ctx.gpr[5]);
    ctx.gpr[4] = 0x08CB2B50u;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2B50u});
    (void)call_virtual_at(rt, ctx, object, 0xCu, {self, object});
    ctx.gpr[2] = self;
}

// func_08A0F938(this, p) -> func_08B20A54(&f, *p, 0) after func_08B20920(&f, 0x08C8E004); its low 4 bits go to bits
// 20-23 of this->+0x60.
void func_08A0F938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], p = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08B20920u, {frame.at(0u), 0x08C8E004u});
    const std::uint32_t value = call_set(rt, ctx, 0x08B20A54u, {frame.at(0u), word(rt, p), 0u});
    ctx.gpr[3] = (word(rt, self + 0x60u) & ~(0xFu << 20)) | (value & 0xFu) << 20;
    put(rt, self + 0x60u, ctx.gpr[3]);
}

// func_088DA7AC(a0, a1, a2, a3): func_088D06AC(func_088DA788(a0, a1, a2, a3), a2, the low byte of a3).
void func_088DA7AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t keep = ctx.gpr[6], low = ctx.gpr[7] & 0xFFu;
    std::uint32_t result = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_set(rt, ctx, 0x088DA788u, {});
    }
    tail_call(ctx, 0x088D06ACu, {result, keep, low});
}

// func_089EE140(this, a1): func_089E119C(this, 0x08CF6ED8), then func_089EE028(this, a1).
void func_089EE140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], second = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_set(rt, ctx, 0x089E119Cu, {self, 0x08CF6ED8u});
    }
    tail_call(ctx, 0x089EE028u, {self, second});
}

// func_08894930(this, a1, base) -> base + func_08898230() after this->+0x54's virtual +0x50.
void func_08894930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x54u);
    (void)call_virtual_at(rt, ctx, object, 0x50u, {object});
    ctx.gpr[2] = base + call_set(rt, ctx, 0x08898230u, {});
}

// func_088B4568(this, a1): a chain of func_08BB1898 / 18C4 / 18F0 / 0EA4 on a frame word {a1}, with this + 0x3C, + 0xF0,
// + 0x1B8 and + 0x3FC, each on what the previous returned.
void func_088B4568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), ctx.gpr[5]);
    std::uint32_t chain = call_set(rt, ctx, 0x08BB1898u, {frame.at(0u), self + 0x3Cu});
    chain = call_set(rt, ctx, 0x08BB18C4u, {chain, self + 0xF0u});
    chain = call_set(rt, ctx, 0x08BB18F0u, {chain, self + 0x1B8u});
    (void)call_set(rt, ctx, 0x08BB0EA4u, {chain, self + 0x3FCu});
}

}  // namespace
}  // namespace hot_batch_108
}  // namespace gt::game

#define HOT108(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_108::name, gt::ReturnKind::kind)

// HOT108REGS
HOT108(0x08925CD4u, func_08925CD4, Word);
HOT108(0x08BAE2B0u, func_08BAE2B0, Word);
HOT108(0x089E4BC4u, func_089E4BC4, Word);
HOT108(0x088BBDB8u, func_088BBDB8, Word);
HOT108(0x08BBBEF8u, func_08BBBEF8, Word);
HOT108(0x089032FCu, func_089032FC, Word);
HOT108(0x08AF9500u, func_08AF9500, Word);
HOT108(0x088B1658u, func_088B1658, Word);
HOT108(0x08AA27A4u, func_08AA27A4, Word);
HOT108(0x08A0F938u, func_08A0F938, Word);
HOT108(0x088DA7ACu, func_088DA7AC, Word);
HOT108(0x089EE140u, func_089EE140, Word);
HOT108(0x08894930u, func_08894930, Word);
HOT108(0x088B4568u, func_088B4568, Word);
