// Hot batch 284: seventy-three-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (list drains, constructors, array fills and two-argument setters).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_284 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
// `ins value, source, position, 1`: the bit at `position` replaced by source's lowest bit.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position) {
    return (value & ~(1u << position)) | ((source & 1u) << position);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
std::int32_t trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}
// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// The same inside a frame of `bytes` (`addiu sp, sp, -bytes; sw ra; jal; ...; jr ra`).
void call_with(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t bytes) {
    GuestFrame frame(ctx, bytes);
    call_here(rt, ctx, target);
}

using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Where the prologue of a function stores a callee-saved register: `reg` 0-31 is a general register, 32-63 a floating one (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};

// The callee-saved registers (s0-s7, fp, f20-f31) a function keeps values in, and the stores of its prologue: the rewrite stores them where the asm
// does (a callee can read the stack below its caller's frame), sets the registers as the asm does because callees can read them too, and gives
// the caller's values back when it returns.
class SavedRegisters {
public:
    SavedRegisters(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        fp_ = ctx.gpr[30];
        for (unsigned i = 0; i < 12; ++i) fpr_[i] = ctx.fpr[20 + i];
        for (const SavedSlot &slot : slots) {
            if (slot.reg < 32u) put(rt, frame.at(slot.offset), ctx.gpr[slot.reg]);
            else put_float(rt, frame.at(slot.offset), ctx.fpr[slot.reg - 32u]);
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
        for (unsigned i = 0; i < 12; ++i) ctx_.fpr[20 + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

u32 signed_byte(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
u32 signed_half(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0897B5C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x14u), G[31]); G[20] = G[4] + 0x20u; G[18] = G[4]; G[4] = G[20]; G[19] = G[5]; G[17] = G[18] + 0x18u; G[16] = 0u; call_here(rt, ctx, 0x08B344E0u); G[2] = word(rt, G[18] + 0x18u); if (G[2] != G[17]) { G[16] = word(rt, G[17] + 4u); G[2] = G[16] + 8u; if (G[19] != G[2]) { G[5] = word(rt, G[16] + 8u); G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[16] = word(rt, G[17] + 4u); G[4] = G[16]; call_here(rt, ctx, 0x08B5C094u); G[4] = G[16] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[2] = word(rt, G[18] + 0x18u); G[18] = 0x08CA0000u; bool stop = false; if (G[17] != G[2]) { do { G[16] = word(rt, G[19]); G[4] = 0x08CA2F7Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[6] = word(rt, G[17] + 4u); G[4] = G[16]; G[2] = word(rt, G[3] + 0x110u); G[5] = word(rt, G[6] + 8u); call_here(rt, ctx, G[2]); if (G[2] == 0u) { G[16] = 1u; stop = true; break; } G[16] = word(rt, G[17] + 4u); G[4] = G[16]; call_here(rt, ctx, 0x08B5C094u); G[4] = G[16] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0xCu; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[2] = word(rt, G[17]); } while (G[2] != G[17]); } if (!stop) G[16] = 1u; } G[4] = G[20]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_089A3744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[19] = G[5]; G[18] = G[4]; G[16] = G[6]; call_here(rt, ctx, 0x0896F1BCu); G[17] = G[2]; if (G[2] == 0u) { G[6] = word(rt, G[16] + 0x24u); G[2] = 1u; if (G[6] == G[2]) { G[4] = G[18]; call_here(rt, ctx, 0x089A3130u); G[6] = G[18]; G[4] = frame.at(0u); G[5] = G[19]; if (G[2] != 0u) G[6] = G[2]; call_here(rt, ctx, 0x08A11BF0u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA4D34u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x108u); G[17] = 0u; call_here(rt, ctx, G[2]); G[3] = 2u; G[4] = frame.at(0u); if (G[2] != 0u) G[17] = G[3]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 2u; if (G[6] == G[2]) { G[4] = G[18]; call_here(rt, ctx, 0x089A3130u); G[6] = G[18]; G[4] = frame.at(0u); G[5] = G[19]; if (G[2] != 0u) G[6] = G[2]; call_here(rt, ctx, 0x08A0BE48u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA4D44u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x108u); call_here(rt, ctx, G[2]); G[3] = 2u; G[4] = frame.at(0u); if (G[2] != 0u) G[17] = G[3]; call_here(rt, ctx, 0x08A5B9ECu); } } } G[2] = G[17]; G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_089A5BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; call_here(rt, ctx, 0x089A2528u); G[2] = 0x08CBAD20u; put(rt, G[16], G[2]); G[2] = 0x08CBAF78u; G[6] = frame.at(0u); put(rt, G[16] + 0x38u, G[2]); G[4] = G[16] + 0x128u; G[5] = 0x08CA4F10u; put(rt, G[16] + 0x120u, 0u); put(rt, G[16] + 0x124u, 0u); call_here(rt, ctx, 0x08B85EB8u); G[4] = 0x08D74FDCu; G[2] = word(rt, G[16] + 0x138u); G[2] &= ~0xFu; G[2] |= 0x10u; G[2] &= ~(3u << 5); put(rt, G[16] + 0x138u, G[2]); G[3] = G[2]; put(rt, G[16] + 0x130u, G[4]); G[7] = G[16] + 0x13Cu; put(rt, G[16] + 0x12Cu, G[4]); G[5] = 0x08CF6ED8u; G[4] = G[16] + 0x150u; put(rt, G[16] + 0x134u, 0u); G[6] = load8(0x08CF38F0u); G[2] = 0x08CF0000u; put(rt, G[16] + 0x13Cu, G[7]); G[3] = (G[3] & ~(1u << 7)) | ((G[6] & 1u) << 7); G[3] |= 0x100u; G[3] &= ~(1u << 9); put(rt, G[16] + 0x138u, G[3]); put(rt, G[7] + 4u, G[7]); put(rt, G[16] + 0x144u, 0u); put(rt, G[16] + 0x148u, 0u); put(rt, G[16] + 0x14Cu, 0u); call_here(rt, ctx, 0x089F09DCu); G[2] = G[16] + 0x154u; G[3] = G[16] + 0x16Cu; G[6] = G[16] + 0x158u; G[7] = G[16] + 0x170u; put(rt, G[2] + 0x14u, 0u); G[4] = G[16]; G[5] = 0u; put(rt, G[2] + 0x10u, G[6]); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 0xCu, G[6]); put(rt, G[3] + 0x10u, G[7]); put(rt, G[3] + 4u, 0u); put(rt, G[3] + 0x14u, 0u); put(rt, G[3] + 8u, 0u); put(rt, G[3] + 0xCu, G[7]); G[2] = word(rt, G[16] + 0x110u); G[2] |= 1u; put(rt, G[16] + 0x110u, G[2]); call_here(rt, ctx, 0x0896A824u); G[2] = word(rt, G[16] + 0x60u); G[2] |= 8u; put(rt, G[16] + 0x60u, G[2]); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x089F12C4 (a0 = the object, a1 = a value, a2 = a count, a3 = an array of counted texts): builds the object — 0x089FD978(object, a1) first, its vtable at +0,
// 0x08A76ABC(object + 0x1C, a1), and the three words at +0x20 / +0x24 / +0x28 zeroed. 0x08A5BC20(count * 4, 4) makes an array that +0x20 and +0x24 point at,
// +0x28 at its end; 0x08A76AA0(sp + 4) prepares a slot and 0x08B85AF4(array, count, sp + 4, sp) turns the texts into the array, then sp + 4 is released. With a
// count above 0 each of the array's words, 4 bytes apart, takes the array of texts' entry unless it is the same pointer (0x08A5BAD8), the array being read
// again from the object's +0x20 for every step.
void func_089F12C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], first = ctx.gpr[5], count = ctx.gpr[6], source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame,
                         {{31u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[23] = count;
    ctx.gpr[22] = self;
    ctx.gpr[21] = 0u;
    ctx.gpr[19] = source;
    ctx.gpr[20] = source;
    const u32 holder = self + 0x20u;
    ctx.gpr[18] = holder;
    const u32 bytes = count << 2u;
    ctx.gpr[17] = bytes;
    ctx.gpr[16] = first;
    call_set(rt, ctx, 0x089FD978u, {self, first});
    put(rt, self, 0x08CBA5F8u);
    call_set(rt, ctx, 0x08A76ABCu, {self + 0x1Cu, first});
    put(rt, self + 0x20u, 0u);
    ctx.gpr[16] = frame.at(0x4u);
    put(rt, holder + 0x4u, 0u);
    put(rt, holder + 0x8u, 0u);
    const u32 block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    ctx.gpr[19] = block;
    put(rt, holder + 0x8u, block + bytes);
    put(rt, self + 0x20u, block);
    put(rt, holder + 0x4u, block);
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x4u)});
    call_set(rt, ctx, 0x08B85AF4u, {block, count, frame.at(0x4u), frame.at(0x0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
    ctx.gpr[19] = word(rt, self + 0x20u);
    put(rt, holder + 0x4u, bytes + ctx.gpr[19]);
    if (static_cast<s32>(count) > 0) {
        for (;;) {
            const u32 slot = (ctx.gpr[21] << 2u) + ctx.gpr[19];
            ctx.gpr[21] += 1u;
            if (ctx.gpr[20] != slot) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, ctx.gpr[20])});
            const bool more = count != ctx.gpr[21];
            ctx.gpr[20] += 4u;
            if (!more) break;
            ctx.gpr[19] = word(rt, self + 0x20u);
        }
    }
}
void func_08A03ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[1] = 0x3F800000u; F[0] = 1.0f; G[2] = 0xFFFFFFFFu; G[22] = G[5]; put_float(rt, G[4], F[0]); put(rt, G[4] + 0x10u, G[2]); put_float(rt, G[4] + 0xCu, F[0]); put_float(rt, G[4] + 8u, F[0]); put_float(rt, G[4] + 4u, F[0]); G[16] = word(rt, G[5]); G[19] = G[4]; if (G[16] == 0u) { G[4] = 0x08CA8D38u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x7Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[20] = G[2]; if (G[2] != 0u) { G[23] = frame.at(0u); G[18] = 0u; G[21] = 0x08CB0000u; G[17] = G[19]; do { G[16] = word(rt, G[22]); G[4] = 0x08CA8D38u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = G[16]; G[6] = G[18]; G[3] = word(rt, G[2] + 0x84u); G[4] = frame.at(0u); G[18] += 1u; call_here(rt, ctx, G[3]); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA8D38u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17], F[0]); G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); G[17] += 4u; } while (G[20] != G[18]); } G[2] = G[19]; G[22] = word(rt, frame.at(0x28u)); G[19] = word(rt, frame.at(0x1Cu)); G[23] = word(rt, frame.at(0x2Cu)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }
void func_08A27FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x60u), G[31]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); G[16] = frame.at(8u); G[5] = 0xEu; G[23] = G[4]; G[4] = G[16]; G[20] = frame.at(4u); call_here(rt, ctx, 0x08ABD5ECu); G[19] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A98A70u); G[18] = G[16]; if (static_cast<std::int32_t>(G[19]) > 0) { G[22] = frame.at(0u); G[17] = 0u; G[21] = 0x08CB0000u; do { G[2] = word(rt, frame.at(4u)); G[4] = 0x08CAB890u; G[16] = G[2] + 0x14u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[18]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[16] + 8u); G[4] = G[16]; G[5] = G[3]; G[6] = frame.at(0u); if (G[3] == G[2]) call_here(rt, ctx, 0x08B8A03Cu); else { G[4] = G[3]; G[5] = frame.at(0u); if (G[3] != 0u) { call_here(rt, ctx, 0x08A76ABCu); G[3] = word(rt, G[16] + 4u); } G[3] += 4u; put(rt, G[16] + 4u, G[3]); } G[17] += 1u; G[4] = G[22]; G[18] += 4u; call_here(rt, ctx, 0x08A5B9ECu); } while (G[19] != G[17]); } G[5] = word(rt, frame.at(4u)); if (G[23] != G[20]) { G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x5Cu)); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); }  } }
void func_08A36240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[18] = G[5]; G[17] = G[7]; put_float(rt, frame.at(0x20u), F[20]); if (static_cast<std::int32_t>(G[6]) > 0) { G[2] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[1] = 0x3F800000u; F[20] = 1.0f; if (G[2] == 0u) { G[16] = word(rt, G[7] + 4u); if (G[16] == 0u) { G[4] = 0x08CACEB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); F[20] = F[0]; } G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08CACEB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[4] = frame.at(8u); G[2] = word(rt, G[3] + 0xCu); G[17] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x08A354ECu); G[16] = word(rt, frame.at(4u)); F[12] = F[20]; if (G[16] == 0u) { G[4] = 0x08CACF04u; call_here(rt, ctx, 0x08A5BB7Cu); } F[12] = F[20]; G[5] = word(rt, frame.at(8u)); G[4] = G[16]; call_here(rt, ctx, 0x08A34FB8u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } F[20] = float_at(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }
void func_08A392C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[2] = 1u; G[18] = G[5]; G[17] = G[7]; put_float(rt, frame.at(0x24u), F[21]); put_float(rt, frame.at(0x20u), F[20]); if (G[6] == G[2]) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } F[21] = 0.0f; G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); F[20] = F[0]; } else { G[2] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[1] = 0x3F800000u; F[20] = 1.0f; F[21] = 0.0f; if (G[2] == 0u) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); F[21] = F[0]; if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); F[20] = F[0]; } } G[5] = G[18]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A391F0u); G[16] = word(rt, frame.at(0u)); F[12] = F[21]; if (G[16] == 0u) { G[4] = 0x08CAD378u; call_here(rt, ctx, 0x08A5BB7Cu); } F[12] = F[21]; F[13] = F[20]; G[4] = G[16]; call_here(rt, ctx, 0x08A38F98u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); F[21] = float_at(rt, frame.at(0x24u)); F[20] = float_at(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_284
} // namespace gt::game

#define HOT284(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_284::name, gt::ReturnKind::kind)
// HOT284(0x0897B5C0u, func_0897B5C0, Word);  // no comparable synthetic case yet
HOT284(0x089A3744u, func_089A3744, Word);
HOT284(0x089A5BCCu, func_089A5BCC, Word);
HOT284(0x089F12C4u, func_089F12C4, Word);
// HOT284(0x08A03ED8u, func_08A03ED8, Word);  // no comparable synthetic case yet
HOT284(0x08A27FB0u, func_08A27FB0, Word);
HOT284(0x08A36240u, func_08A36240, Word);
// HOT284(0x08A392C8u, func_08A392C8, Word);  // no comparable synthetic case yet
