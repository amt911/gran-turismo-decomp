// Hot batch 223: sixty-one- to sixty-four-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_223 {
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
    if (!(value >= -2237483648.0f && value < 2237483648.0f)) return 0x7FFFFFFF;
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

void func_08B9C7D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75798u), 8); G[18] = 0x08D75798u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D757DCu; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D757DCu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D757DCu; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_0898037C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0897E844u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x200u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3390u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x200u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0899C558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CA482Cu; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089B9E60u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x0899CD50u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CF6F24u; G[6] = 0x0899CE20u; call_here(rt, ctx, 0x08B8A004u); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_08BA0394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D757C8u), 8); G[18] = 0x08D757C8u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D757E8u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D757E8u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D757E8u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA04F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75790u), 8); G[18] = 0x08D75790u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D757F4u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D757F4u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D757F4u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA0654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75708u), 8); G[18] = 0x08D75708u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D75800u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D75800u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D75800u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA07B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75780u), 8); G[18] = 0x08D75780u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D7580Cu; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D7580Cu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D7580Cu; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA0914(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75778u), 8); G[18] = 0x08D75778u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D75818u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D75818u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D75818u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA0A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75770u), 8); G[18] = 0x08D75770u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D75824u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D75824u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D75824u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA0BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75768u), 8); G[18] = 0x08D75768u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D75830u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D75830u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D75830u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA0E34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75788u), 8); G[18] = 0x08D75788u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D75848u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D75848u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D75848u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA0F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75760u), 8); G[18] = 0x08D75760u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D75854u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D75854u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D75854u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA10F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D75758u), 8); G[18] = 0x08D75758u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D75860u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D75860u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D75860u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BA1254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D757B8u), 8); G[18] = 0x08D757B8u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D7586Cu; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D7586Cu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D7586Cu; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BBE2D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D762D8u), 8); G[18] = 0x08D762D8u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D76388u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D76388u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D76388u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BC9F18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76380u), 8); G[18] = 0x08D76380u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D76394u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D76394u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D76394u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 8u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCA078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76378u), 8); G[18] = 0x08D76378u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763A0u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763A0u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763A0u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 8u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCA1D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76370u), 8); G[18] = 0x08D76370u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763ACu; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763ACu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763ACu; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCA338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76368u), 8); G[18] = 0x08D76368u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763B8u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763B8u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763B8u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCA498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76360u), 8); G[18] = 0x08D76360u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763C4u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763C4u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763C4u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCA5F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76358u), 8); G[18] = 0x08D76358u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763D0u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763D0u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763D0u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCA758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76350u), 8); G[18] = 0x08D76350u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763DCu; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763DCu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763DCu; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCA8B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76348u), 8); G[18] = 0x08D76348u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763E8u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763E8u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763E8u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCAA18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76340u), 8); G[18] = 0x08D76340u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D763F4u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D763F4u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D763F4u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCAB78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D76338u), 8); G[18] = 0x08D76338u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D76400u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D76400u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D76400u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCACD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D762D0u), 8); G[18] = 0x08D762D0u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D7640Cu; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D7640Cu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D7640Cu; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCAE38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D762C8u), 8); G[18] = 0x08D762C8u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D76418u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D76418u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D76418u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCAF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D762B8u), 8); G[18] = 0x08D762B8u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D76424u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D76424u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D76424u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCB0F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D762B0u), 8); G[18] = 0x08D762B0u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D76430u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D76430u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D76430u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08BCB258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D762C0u), 8); G[18] = 0x08D762C0u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D7643Cu; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D7643Cu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D7643Cu; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 4u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08C4E6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; if (G[4] == 0u) { G[2] = 0u; } else { G[4] = word(rt, G[16]); G[2] = 0x08D70000u; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); G[17] = G[2]; } else { G[3] = sext(load8(0x08D75290u), 8); G[17] = 0x08D75290u; if (G[3] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] == 0u) { G[17] = 0x08D75878u; } else { G[3] = 0x08D70000u; G[2] = 0x08D75878u; G[4] = G[17]; put(rt, G[2] + 4u, 0u); G[17] = G[2]; put(rt, G[2] + 8u, 0u); put(rt, 0x08D75878u, 0u); call_here(rt, ctx, 0x08B64494u); } } else { G[17] = 0x08D75878u; } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D77378u), 8); G[18] = 0x08D77378u; G[19] = 0x08D70000u; if (G[3] == 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B6441Cu); if (G[2] != 0u) { G[2] = 0x08D77380u; G[4] = G[18]; put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); put(rt, 0x08D77380u, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D77380u; if (G[17] != G[2]) { G[2] = 0u; } else { G[2] = word(rt, G[16]); G[2] += 8u; } } G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } }
void func_08981438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0897E844u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1ACu); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3390u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1ACu, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898152C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0897E844u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1B0u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3390u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1B0u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08981810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0897E844u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x138u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3390u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x138u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08981B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0897E844u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x160u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3390u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x160u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898C258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089896CCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1ACu); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x089896CCu); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA39E8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1ACu, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898C34C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089896CCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1B0u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x089896CCu); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA39E8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1B0u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898C630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089896CCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x138u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x089896CCu); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA39E8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x138u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898C948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089896CCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x160u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x089896CCu); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA39E8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x160u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898F26C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0898E170u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1ECu); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0898E170u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3C7Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1ECu, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898F360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0898E170u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1F0u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0898E170u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3C7Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1F0u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898F644(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0898E170u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x178u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0898E170u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3C7Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x178u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_0898F95C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0898E170u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1A0u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0898E170u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3C7Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1A0u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_089954A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x08995460u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA4294u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x128u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[4] = frame.at(0u); call_here(rt, ctx, 0x08995460u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA4294u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA4208u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x128u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_089F0F5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CA8058u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089FD934u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089F1468u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CF6F24u; G[6] = 0x089F15E4u; call_here(rt, ctx, 0x08B8A004u); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_089FD1BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CA878Cu; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089BA3DCu); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089FCE88u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CF6F24u; G[6] = 0x089FD0D8u; call_here(rt, ctx, 0x08B8A004u); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
// 0x08A0EF38 (a0 = the object): replaces the object's +0x38 holder with a fresh one built from a format text. The text is formatted at
// frame +4 by 0x08B85EB8(text, 0x08CA95E0, frame +1) and handed with the object to the object's virtual slot 0x108 (the word the vtable's
// +0x108 points at); the text is released at once (a counted string: its header is word - 0xC, the empty one 0x08D74FD0 is skipped,
// 0x08B60804(word - 4, -1) drops the count and at 0 the header is freed by 0x08B8542C(header, frame)). 0x08A76A94() makes the new holder
// (released straight away, 0x08A5BA08, when it is null), the old holder at +0x38 is released (0x08A5B930) and the new one stored; then
// 0x08B8850C(object, 0x08CF6F14, 0x08A0EE9C) and 0x08B87108(object, 0x08CA95EC, 0x08A0EBF4) register it. Returns the last call's result.
void func_08A0EF38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA95E0u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A0EE9Cu});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CA95ECu, 0x08A0EBF4u});
}
// 0x08A11D04 (a0 = the object): the same replacement of the object's +0x38 holder as 0x08A0EF38, with its own constants: the text is
// formatted at frame +4 by 0x08B85EB8(text, 0x08CA98D8, frame +1) and given with the object to the virtual slot 0x108 (word at the
// vtable's +0x108). The counted text is released at once (header word - 0xC; the empty header 0x08D74FD0 skips it; 0x08B60804(word - 4, -1)
// drops the count, 0x08B8542C(header, frame) frees the header when the count reaches 0). 0x08965418() makes the holder (released by
// 0x08A5BA08 when null), the old holder at +0x38 is released by 0x08A5B930 and the new one stored; then
// 0x08B8850C(object, 0x08CF6F14, 0x08A11C60) and 0x08B8A004(object, 0x08CF6F24, 0x08A11A98) register it. Returns the last call's result.
void func_08A11D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA98D8u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08965418u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A11C60u});
    call_set(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x08A11A98u});
}
// 0x08A442DC (a0 = the object): the +0x38 holder replacement again, with the format 0x08CADD98 built at frame +4 by
// 0x08B85EB8(text, 0x08CADD98, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and at 0 the header freed by
// 0x08B8542C(header, frame)). 0x08A76A94() makes the holder (released by 0x08A5BA08 when null), the old holder is released by 0x08A5B930 and
// the new one stored; the two registrations are 0x08B86FB0(object, 0x08CADDA0, 0x08A4412C) and 0x08B86FB0(object, 0x08CADDA8, 0x08A43F60).
// Returns the last call's result.
void func_08A442DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CADD98u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B86FB0u, {self, 0x08CADDA0u, 0x08A4412Cu});
    call_set(rt, ctx, 0x08B86FB0u, {self, 0x08CADDA8u, 0x08A43F60u});
}
// 0x08A55774 (a0 = the object): the +0x38 holder replacement with the format 0x08CAF654, built at frame +4 by
// 0x08B85EB8(text, 0x08CAF654, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A5261C() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x08A555F0) and 0x08B8A004(object, 0x08CF6F24, 0x08A55398) register
// it. Returns the last call's result.
void func_08A55774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAF654u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A5261Cu, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A555F0u});
    call_set(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x08A55398u});
}
// 0x08A58CE8 (a0 = the object): the +0x38 holder replacement with the format 0x08CAF8D8, built at frame +4 by
// 0x08B85EB8(text, 0x08CAF8D8, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A5261C() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; the first registration is 0x08BFD550(object, 0x08CF6F14, 0x08A59014), the second
// 0x08B8A004(object, 0x08CF6F24, 0x08A58C2C). Returns the last call's result.
void func_08A58CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAF8D8u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A5261Cu, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08BFD550u, {self, 0x08CF6F14u, 0x08A59014u});
    call_set(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x08A58C2Cu});
}
// 0x08A8964C (a0 = the object): the +0x38 holder replacement with the format 0x08CB1A40, built at frame +4 by
// 0x08B85EB8(text, 0x08CB1A40, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A719AC() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x08A89528) and 0x08B87108(object, 0x08CB1A4C, 0x08A89750) register
// it. Returns the last call's result.
void func_08A8964C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB1A40u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A719ACu, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A89528u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CB1A4Cu, 0x08A89750u});
}
// 0x08AB4EE8 (a0 = the object): the +0x38 holder replacement with the format 0x08CB3E70, built at frame +4 by
// 0x08B85EB8(text, 0x08CB3E70, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A76A94() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; both registrations are 0x08B87108(object, 0x08CB3E78, 0x08AB4E18) and
// 0x08B87108(object, 0x08CB3E80, 0x08AB4CEC). Returns the last call's result.
void func_08AB4EE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB3E70u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CB3E78u, 0x08AB4E18u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CB3E80u, 0x08AB4CECu});
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_223
} // namespace gt::game

#define HOT223(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_223::name, gt::ReturnKind::kind)
HOT223(0x08B9C7D4u, func_08B9C7D4, Word);
HOT223(0x0898037Cu, func_0898037C, Word);
HOT223(0x0899C558u, func_0899C558, Word);
HOT223(0x08BA0394u, func_08BA0394, Word);
HOT223(0x08BA04F4u, func_08BA04F4, Word);
HOT223(0x08BA0654u, func_08BA0654, Word);
HOT223(0x08BA07B4u, func_08BA07B4, Word);
HOT223(0x08BA0914u, func_08BA0914, Word);
HOT223(0x08BA0A74u, func_08BA0A74, Word);
HOT223(0x08BA0BD4u, func_08BA0BD4, Word);
HOT223(0x08BA0E34u, func_08BA0E34, Word);
HOT223(0x08BA0F94u, func_08BA0F94, Word);
HOT223(0x08BA10F4u, func_08BA10F4, Word);
HOT223(0x08BA1254u, func_08BA1254, Word);
HOT223(0x08BBE2D0u, func_08BBE2D0, Word);
HOT223(0x08BC9F18u, func_08BC9F18, Word);
HOT223(0x08BCA078u, func_08BCA078, Word);
HOT223(0x08BCA1D8u, func_08BCA1D8, Word);
HOT223(0x08BCA338u, func_08BCA338, Word);
HOT223(0x08BCA498u, func_08BCA498, Word);
HOT223(0x08BCA5F8u, func_08BCA5F8, Word);
HOT223(0x08BCA758u, func_08BCA758, Word);
HOT223(0x08BCA8B8u, func_08BCA8B8, Word);
HOT223(0x08BCAA18u, func_08BCAA18, Word);
HOT223(0x08BCAB78u, func_08BCAB78, Word);
HOT223(0x08BCACD8u, func_08BCACD8, Word);
HOT223(0x08BCAE38u, func_08BCAE38, Word);
HOT223(0x08BCAF98u, func_08BCAF98, Word);
HOT223(0x08BCB0F8u, func_08BCB0F8, Word);
HOT223(0x08BCB258u, func_08BCB258, Word);
HOT223(0x08C4E6B0u, func_08C4E6B0, Word);
HOT223(0x08981438u, func_08981438, Word);
HOT223(0x0898152Cu, func_0898152C, Word);
HOT223(0x08981810u, func_08981810, Word);
HOT223(0x08981B28u, func_08981B28, Word);
HOT223(0x0898C258u, func_0898C258, Word);
HOT223(0x0898C34Cu, func_0898C34C, Word);
HOT223(0x0898C630u, func_0898C630, Word);
HOT223(0x0898C948u, func_0898C948, Word);
HOT223(0x0898F26Cu, func_0898F26C, Word);
HOT223(0x0898F360u, func_0898F360, Word);
HOT223(0x0898F644u, func_0898F644, Word);
HOT223(0x0898F95Cu, func_0898F95C, Word);
HOT223(0x089954A0u, func_089954A0, Word);
HOT223(0x089F0F5Cu, func_089F0F5C, Word);
HOT223(0x089FD1BCu, func_089FD1BC, Word);
HOT223(0x08A0EF38u, func_08A0EF38, Word);
HOT223(0x08A11D04u, func_08A11D04, Word);
HOT223(0x08A442DCu, func_08A442DC, Word);
HOT223(0x08A55774u, func_08A55774, Word);
HOT223(0x08A58CE8u, func_08A58CE8, Word);
HOT223(0x08A8964Cu, func_08A8964C, Word);
HOT223(0x08AB4EE8u, func_08AB4EE8, Word);
