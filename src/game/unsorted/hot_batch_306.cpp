// Hot batch 306: eighty-five and eighty-six-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (script bindings and property accessors, number formatting, qualified-name building,
// a sound-bank destructor, a table reset, a list compare and buffer concatenation).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_306 {
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

void func_0898071C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); G[19] = G[4]; G[18] = G[6]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(0xCu); G[4] = G[18]; call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(0xCu)); G[16] = frame.at(0x10u); if (G[17] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = G[17] + 0x120u; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0x10u)); if (G[19] != G[16]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[20] = frame.at(8u); G[4] = G[20]; call_here(rt, ctx, 0x0897E844u); G[19] = word(rt, frame.at(8u)); if (G[19] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[16]); if (G[17] == 0u) { G[4] = 0x08CA3390u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = frame.at(4u); G[5] = G[17]; G[3] = word(rt, G[2] + 0xCu); G[4] = G[16]; G[17] = G[19] + 0xF0u; call_here(rt, ctx, G[3]); G[4] = G[19] + 0x120u; G[5] = G[16]; call_here(rt, ctx, 0x08B855E0u); store8(G[17] + 0xC4u, G[18]); G[2] = 0x08D74FD0u; G[4] = word(rt, frame.at(4u)); G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } } G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); }  } }
void func_0898B53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); G[19] = G[4]; G[18] = G[6]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(0xCu); G[4] = G[18]; call_here(rt, ctx, 0x089896CCu); G[17] = word(rt, frame.at(0xCu)); G[16] = frame.at(0x10u); if (G[17] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = G[17] + 0x120u; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0x10u)); if (G[19] != G[16]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[20] = frame.at(8u); G[4] = G[20]; call_here(rt, ctx, 0x089896CCu); G[19] = word(rt, frame.at(8u)); if (G[19] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[16]); if (G[17] == 0u) { G[4] = 0x08CA39E8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = frame.at(4u); G[5] = G[17]; G[3] = word(rt, G[2] + 0xCu); G[4] = G[16]; G[17] = G[19] + 0xF0u; call_here(rt, ctx, G[3]); G[4] = G[19] + 0x120u; G[5] = G[16]; call_here(rt, ctx, 0x08B855E0u); store8(G[17] + 0xC4u, G[18]); G[2] = 0x08D74FD0u; G[4] = word(rt, frame.at(4u)); G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } } G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); }  } }
void func_08A1FCEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x98u), G[18]); put(rt, frame.at(0x90u), G[16]); put(rt, frame.at(0x9Cu), G[31]); put(rt, frame.at(0x94u), G[17]); G[18] = G[4]; G[16] = G[6]; if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = word(rt, G[6]); G[4] = 0x08CA0000u; if (G[17] == 0u) { G[4] = 0x08CAA480u; call_here(rt, ctx, 0x08A5BB7Cu); } call_here(rt, ctx, 0x08A82A5Cu); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A76F20u); const bool isf = G[2] != 0u; G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CAA480u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + (isf ? 0x40u : 0x3Cu)); G[17] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[4] = G[2]; G[5] = G[17]; G[6] = 0x80u; call_here(rt, ctx, 0x08ABE178u); G[16] = frame.at(4u); G[5] = G[17]; G[4] = G[16]; G[17] = frame.at(8u); G[6] = frame.at(1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0x8u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x98u)); G[16] = word(rt, frame.at(0x90u)); G[17] = word(rt, frame.at(0x94u)); }  } }
void func_08918998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC58E0u; G[17] = G[4]; G[16] = word(rt, G[4] + 8u); put(rt, G[17], G[2]); G[4] = G[16] + 0x2010u; if (G[16] != 0u) { G[2] = 0x08CC5950u; put(rt, G[16] + 0x2174u, G[2]); G[3] = 0x08CC2760u; G[2] = 0x08CC5978u; put(rt, G[16] + 0x216Cu, G[3]); G[3] = 0x08CBF428u; put(rt, G[16] + 0x2164u, G[2]); G[2] = 0x08CC5990u; put(rt, G[16] + 0x2160u, G[3]); put(rt, G[16] + 0x2010u, G[2]); call_here(rt, ctx, 0x08925AE4u); G[4] = G[16] + 0x1FD4u; call_here(rt, ctx, 0x08925B48u); G[4] = G[16] + 0x1D30u; call_here(rt, ctx, 0x0891C6B0u); G[4] = G[16] + 0x1B20u; call_here(rt, ctx, 0x0891A724u); G[2] = 0x08CC5B40u; put(rt, G[16] + 0x1920u, G[2]); G[4] = G[16] + 0x1920u; call_here(rt, ctx, 0x08925FF8u); G[2] = 0x08CC6020u; put(rt, G[16] + 0x1720u, G[2]); G[4] = G[16] + 0x1720u; call_here(rt, ctx, 0x08925FF8u); G[2] = 0x08CC59D0u; put(rt, G[16] + 0x1520u, G[2]); G[4] = G[16] + 0x1520u; call_here(rt, ctx, 0x08925FF8u); G[2] = 0x08CC5A28u; put(rt, G[16] + 0x1320u, G[2]); G[4] = G[16] + 0x1320u; call_here(rt, ctx, 0x08925FF8u); G[4] = G[16] + 0xF00u; call_here(rt, ctx, 0x0891A344u); G[2] = 0x08CC5D18u; put(rt, G[16] + 0xD00u, G[2]); G[4] = G[16] + 0xD00u; call_here(rt, ctx, 0x08925FF8u); G[4] = G[16] + 0xB00u; call_here(rt, ctx, 0x0891A5B8u); G[4] = G[16] + 0x900u; call_here(rt, ctx, 0x0891A4C4u); G[4] = G[16] + 0x4E0u; call_here(rt, ctx, 0x0891A344u); G[4] = G[16] + 0x290u; call_here(rt, ctx, 0x0891C588u); G[4] = G[16]; call_here(rt, ctx, 0x0891FDE8u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); } put(rt, G[17] + 8u, 0u); G[2] = 0x08CC5A98u; G[3] = 0x08CC5A80u; put(rt, G[17], G[2]); put(rt, G[17] + 0x1Cu, G[3]); put(rt, G[17] + 0xCu, 0u); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08805D9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); G[16] = 0x10000u; G[5] = 0u; G[6] = 0x10488u; G[16] = 0x10050u; G[17] = G[4]; call_here(rt, ctx, 0x08B36018u); G[2] = 0x48190000u; G[7] = G[17] + G[16]; G[9] = 0x481907FFu; G[12] = 0u; G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; G[8] = 10u; G[13] = 4u; G[14] = 90u; G[11] = 0u; G[10] = 0u; do { G[2] = G[11] + G[17]; G[2] += 0x5550u; G[6] = 0u; G[3] = G[10] + G[17]; G[18] = 0u; G[19] = 0u; do { G[6] += 1u; put(rt, G[3], G[4]); put(rt, G[3] + 4u, G[5]); put(rt, G[3] + 0x10u, G[18]); put(rt, G[3] + 0x14u, G[19]); put(rt, G[3] + 8u, G[9]); store8(G[3] + 0xCu, G[4]); store8(G[3] + 0xDu, G[4]); store8(G[3] + 0xEu, G[4]); store8(G[3] + 0xFu, G[4]); G[3] += 0x18u; put(rt, G[2] - 0xE0u, G[18]); put(rt, G[2] - 0xDCu, G[19]); put(rt, G[2] + 0x10u, G[18]); put(rt, G[2] + 0x14u, G[19]); put(rt, G[2] - 0xF0u, G[4]); put(rt, G[2] - 0xECu, G[5]); put(rt, G[2] - 0xE8u, G[4]); store8(G[2] - 0xE4u, G[4]); store8(G[2] - 0xE3u, G[4]); store8(G[2] - 0xE2u, G[4]); store8(G[2] - 0xE1u, G[4]); put(rt, G[2], G[4]); put(rt, G[2] + 4u, G[5]); put(rt, G[2] + 8u, G[4]); store8(G[2] + 0xCu, G[4]); store8(G[2] + 0xDu, G[4]); store8(G[2] + 0xEu, G[4]); store8(G[2] + 0xFu, G[4]); G[2] += 0x18u; } while (G[6] != G[8]); G[12] += 1u; put(rt, G[7], G[13]); G[10] += 0xF0u; G[11] += 0x1E0u; put(rt, G[7] + 4u, 0u); put(rt, G[7] + 8u, 0u); G[7] += 0xCu; } while (G[12] != G[14]); G[2] = 0xFD20u; G[4] = G[17] + G[2]; G[3] = 0u; G[2] = G[9]; G[6] = 0xFFFFFFFFu; G[5] = 0x66u; do { G[3] += 1u; put(rt, G[4], G[6]); put(rt, G[4] + 4u, G[2]); G[4] += 8u; } while (G[3] != G[5]); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_08AAECB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x34u), G[21]); G[2] = 0x08D70000u; G[19] = 0x08D74FD0u; G[2] = G[19] + 0xCu; G[17] = frame.at(0x10u); G[20] = G[5]; G[18] = G[4]; put(rt, G[4], G[2]); G[4] = G[17]; call_here(rt, ctx, 0x08AAE358u); G[16] = word(rt, frame.at(0x10u)); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); const bool parent = G[16] != 0u; G[16] = frame.at(4u); if (parent) { G[21] = frame.at(8u); G[4] = G[21]; G[5] = G[20]; call_here(rt, ctx, 0x08AAE358u); G[17] = word(rt, frame.at(8u)); G[16] = frame.at(0xCu); if (G[17] == 0u) { G[4] = 0x08CB39A4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08AAECB0u); G[4] = G[18]; G[5] = G[16]; call_here(rt, ctx, 0x08B89054u); G[4] = word(rt, frame.at(0xCu)); G[16] = G[4] - 0xCu; if (G[16] != G[19]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08CB39C8u; G[4] = G[18]; G[6] = 2u; call_here(rt, ctx, 0x08BEED1Cu); G[16] = frame.at(4u); } G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x08AAE224u); G[4] = G[18]; G[5] = G[16]; call_here(rt, ctx, 0x08B89054u); G[4] = word(rt, frame.at(4u)); G[16] = G[4] - 0xCu; G[2] = G[18]; if (G[19] != G[16]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[18]; } G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); G[21] = word(rt, frame.at(0x34u)); }  } }
void func_0881E458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[20] = G[4]; G[17] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(0xCu); G[4] = G[18]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(0xCu)); if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[4] = G[18]; G[18] = frame.at(8u); G[19] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[17]; G[4] = G[18]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = frame.at(0x18u); G[5] = G[16]; G[4] = G[19]; G[6] = frame.at(0x10u); G[7] = frame.at(0x14u); call_here(rt, ctx, 0x0881B950u); G[4] = G[18]; G[5] = 2u; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0x18u)); G[5] = word(rt, frame.at(0x10u)); G[16] = frame.at(4u); G[17] = word(rt, G[2] + 0x14u); G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x18u)); G[5] = word(rt, frame.at(0x14u)); G[4] = frame.at(0u); G[3] = word(rt, G[2] + 0x14u); G[16] = G[3] + 4u; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[16] != frame.at(0u)) { G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[5] = word(rt, frame.at(0x18u)); if (G[20] != G[18]) { G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_089185DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = true; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC58E0u; G[17] = G[4]; G[16] = word(rt, G[4] + 8u); put(rt, G[17], G[2]); G[4] = G[16] + 0x2010u; if (G[16] != 0u) { G[2] = 0x08CC5950u; put(rt, G[16] + 0x2174u, G[2]); G[3] = 0x08CC2760u; G[2] = 0x08CC5978u; put(rt, G[16] + 0x216Cu, G[3]); G[3] = 0x08CBF428u; put(rt, G[16] + 0x2164u, G[2]); G[2] = 0x08CC5990u; put(rt, G[16] + 0x2160u, G[3]); put(rt, G[16] + 0x2010u, G[2]); call_here(rt, ctx, 0x08925AE4u); G[4] = G[16] + 0x1FD4u; call_here(rt, ctx, 0x08925B48u); G[4] = G[16] + 0x1D30u; call_here(rt, ctx, 0x0891C6B0u); G[4] = G[16] + 0x1B20u; call_here(rt, ctx, 0x0891A724u); G[2] = 0x08CC5B40u; put(rt, G[16] + 0x1920u, G[2]); G[4] = G[16] + 0x1920u; call_here(rt, ctx, 0x08925FF8u); G[2] = 0x08CC6020u; put(rt, G[16] + 0x1720u, G[2]); G[4] = G[16] + 0x1720u; call_here(rt, ctx, 0x08925FF8u); G[2] = 0x08CC59D0u; put(rt, G[16] + 0x1520u, G[2]); G[4] = G[16] + 0x1520u; call_here(rt, ctx, 0x08925FF8u); G[2] = 0x08CC5A28u; put(rt, G[16] + 0x1320u, G[2]); G[4] = G[16] + 0x1320u; call_here(rt, ctx, 0x08925FF8u); G[4] = G[16] + 0xF00u; call_here(rt, ctx, 0x0891A344u); G[2] = 0x08CC5D18u; put(rt, G[16] + 0xD00u, G[2]); G[4] = G[16] + 0xD00u; call_here(rt, ctx, 0x08925FF8u); G[4] = G[16] + 0xB00u; call_here(rt, ctx, 0x0891A5B8u); G[4] = G[16] + 0x900u; call_here(rt, ctx, 0x0891A4C4u); G[4] = G[16] + 0x4E0u; call_here(rt, ctx, 0x0891A344u); G[4] = G[16] + 0x290u; call_here(rt, ctx, 0x0891C588u); G[4] = G[16]; call_here(rt, ctx, 0x0891FDE8u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); } put(rt, G[17] + 8u, 0u); G[2] = 0x08CC5A80u; put(rt, G[17] + 0x1Cu, G[2]); G[2] = 0x08CC5A98u; put(rt, G[17], G[2]); G[4] = G[17]; put(rt, G[17] + 0xCu, 0u); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  if (tail) jump(ctx, 0x08B64654u); } } }
void func_089B99E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CA5B6Cu; G[6] = frame.at(1u); G[20] = 0x089C0000u; G[19] = 0x089C0000u; G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089BA3DCu); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089B98C8u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[6] = 0x089B9794u; G[5] = 0x08CA5B78u; G[7] = 0u; call_here(rt, ctx, 0x08B8D51Cu); G[4] = G[18]; G[6] = 0x089B9700u; G[5] = 0x08CA5B80u; G[7] = 0u; call_here(rt, ctx, 0x08B8D51Cu); G[4] = G[18]; G[6] = 0x089B9794u; G[5] = 0x08CA5B88u; G[7] = 0u; call_here(rt, ctx, 0x08B8D51Cu); G[4] = G[18]; G[6] = 0x089B9700u; G[5] = 0x08CA5B94u; G[7] = 0u; call_here(rt, ctx, 0x08B8D51Cu);  G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_0883E9A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); G[18] = G[4]; G[16] = G[6]; if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = word(rt, G[6]); G[4] = 0x08CA0000u; if (G[17] == 0u) { G[4] = 0x08C9BD48u; call_here(rt, ctx, 0x08A5BB7Cu); } call_here(rt, ctx, 0x08A6BF34u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A76F20u); const bool str = G[2] != 0u; G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08C9BD48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); if (!str) { G[4] = G[16]; G[2] = word(rt, G[3] + 0x50u); G[16] = frame.at(8u); call_here(rt, ctx, G[2]); G[6] = G[2]; G[7] = G[3]; G[4] = G[16]; call_here(rt, ctx, 0x08A5DEE8u); if (G[18] != G[16]) { G[5] = word(rt, frame.at(8u)); G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[5] = G[16]; G[4] = frame.at(0xCu); G[2] = word(rt, G[3] + 0xCu); G[16] = frame.at(4u); call_here(rt, ctx, G[2]); G[4] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08930308u); G[6] = G[2]; G[7] = G[3]; G[4] = G[16]; call_here(rt, ctx, 0x08A5DEE8u); G[5] = word(rt, frame.at(4u)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
// 0x08A53D4C (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CAF5A0, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A5261C() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then 0x08B8850C(object, 0x08CF6F14, 0x08A54FA4), 0x08B8A004(object, 0x08CF6F24, 0x08A53C58) and 0x08B87108(object, 0x08CAF5AC,
// 0x08A53814); the two 8-byte pairs at 0x08C8E9C0 (frame +0x10) and 0x08C8E9B8 (frame +8) go to 0x08C1DCAC(object, 0x08CAF5B4, +0x10, +8).
void func_08A53D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x2Cu}, {18u, 0x28u}, {16u, 0x20u}, {17u, 0x24u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAF5A0u, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08A5261Cu, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A54FA4u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08A53C58u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAF5ACu, 0x08A53814u});
    auto copy_pair = [&](u32 source, u32 slot) {
        put(rt, frame.at(slot), word(rt, source));
        put(rt, frame.at(slot + 0x4u), word(rt, source + 0x4u));
    };
    copy_pair(0x08C8E9C0u, 0x10u);
    copy_pair(0x08C8E9B8u, 0x8u);
    call_set(rt, ctx, 0x08C1DCACu, {object, 0x08CAF5B4u, frame.at(0x10u), frame.at(0x8u)});
}
void func_08A1F8FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x98u), G[18]); put(rt, frame.at(0x90u), G[16]); put(rt, frame.at(0x9Cu), G[31]); put(rt, frame.at(0x94u), G[17]); G[18] = G[4]; G[16] = G[6]; if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = word(rt, G[6]); G[4] = 0x08CA0000u; if (G[17] == 0u) { G[4] = 0x08CAA480u; call_here(rt, ctx, 0x08A5BB7Cu); } call_here(rt, ctx, 0x08A82A5Cu); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A76F20u); const bool isf = G[2] != 0u; G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CAA480u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + (isf ? 0x40u : 0x3Cu)); G[17] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[4] = G[2]; if (isf) { G[5] = G[3]; G[6] = G[17]; G[7] = 0x80u; call_here(rt, ctx, 0x08ABE53Cu); } else { G[5] = G[17]; G[6] = 0x80u; call_here(rt, ctx, 0x08ABE760u); } G[16] = frame.at(4u); G[5] = G[17]; G[4] = G[16]; G[17] = frame.at(8u); G[6] = frame.at(1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0x8u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x98u)); G[16] = word(rt, frame.at(0x90u)); G[17] = word(rt, frame.at(0x94u)); }  } }
void func_089E7AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[19] = G[5]; G[5] = 0x08CF6ED8u; G[20] = 0x08CF0000u; G[21] = G[7]; G[18] = G[6]; G[17] = G[4]; call_here(rt, ctx, 0x08969188u); G[4] = 0x08CF575Cu; call_here(rt, ctx, 0x08B344E0u); G[4] = G[19]; G[16] = ~G[18]; call_here(rt, ctx, 0x089E6B00u); G[16] >>= 31; G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[16] &= G[2]; G[4] = G[19]; if (G[16] != 0u) { G[5] = G[18]; call_here(rt, ctx, 0x089E5814u); G[3] = G[2]; G[2] += 0xCu; if (G[17] != G[2]) { G[5] = word(rt, G[3] + 0xCu); G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = 0x08CF575Cu; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[17]; if (G[16] != 0u && G[21] != 0u) { G[2] = word(rt, G[17]); bool empty = G[2] == 0u; G[2] = G[17]; if (empty) { G[4] = frame.at(0u); G[5] = G[19]; call_here(rt, ctx, 0x089E5D98u); G[16] = word(rt, frame.at(0u)); G[20] = frame.at(4u); if (G[16] == 0u) { G[4] = 0x08CA7A08u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = G[16]; G[4] = G[20]; G[16] = frame.at(8u); call_here(rt, ctx, 0x08A77444u); G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x08969244u); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; G[5] = G[18]; G[6] = G[21]; G[7] = G[17]; call_here(rt, ctx, 0x089E778Cu); G[2] = G[17]; } } G[19] = word(rt, frame.at(0x1Cu)); G[20] = word(rt, frame.at(0x20u)); G[21] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A7C6D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); G[20] = frame.at(0x10u); G[4] = G[20]; G[18] = G[7]; G[16] = G[6]; call_here(rt, ctx, 0x08A7C3A4u); G[2] = 1u; if (G[16] == G[2]) { G[19] = word(rt, frame.at(0x10u)); if (G[19] == 0u) { G[4] = 0x08CB1360u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08CB1350u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[17] = frame.at(0xCu); G[5] = G[16]; G[3] = word(rt, G[2] + 0x5Cu); G[16] = frame.at(8u); G[4] = G[17]; call_here(rt, ctx, G[3]); G[4] = G[16]; call_here(rt, ctx, 0x08A76AA0u); G[4] = G[19]; G[5] = G[17]; G[6] = G[16]; call_here(rt, ctx, 0x08C2045Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A69D68u); } else { G[2] = 2u; G[19] = word(rt, frame.at(0x10u)); if (G[16] == G[2]) { if (G[19] == 0u) { G[4] = 0x08CB1360u; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[18]); if (G[17] == 0u) { G[4] = 0x08CB1350u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = frame.at(4u); G[5] = G[17]; G[3] = word(rt, G[2] + 0x5Cu); G[4] = G[16]; call_here(rt, ctx, G[3]); G[4] = frame.at(0u); G[5] = G[18] + 4u; call_here(rt, ctx, 0x08A76ABCu); G[4] = G[19]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08C2045Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); }  } }
void func_08A84B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x34u), G[17]); G[5] = 0x08CB176Cu; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A5CF74u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); G[2] = 0x08C8F364u; G[3] = 0x08C8F35Cu; G[8] = word(rt, G[2] + 4u); G[9] = word(rt, G[3] + 4u); G[2] = word(rt, 0x08C8F364u); G[3] = word(rt, 0x08C8F35Cu); G[5] = 0x08CB177Cu; put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[6] = frame.at(0x20u); G[7] = frame.at(0x18u); put(rt, frame.at(0x20u), G[2]); put(rt, frame.at(0x24u), G[8]); put(rt, frame.at(0x18u), G[3]); put(rt, frame.at(0x1Cu), G[9]); call_here(rt, ctx, 0x08C2A004u); G[2] = 0x08C8F354u; G[3] = 0x08C8F34Cu; G[8] = word(rt, G[2] + 4u); G[9] = word(rt, G[3] + 4u); G[2] = word(rt, 0x08C8F354u); G[3] = word(rt, 0x08C8F34Cu); G[5] = 0x08CB1784u; G[4] = G[18]; G[6] = frame.at(0x10u); G[7] = frame.at(0x8u); put(rt, frame.at(0x10u), G[2]); put(rt, frame.at(0x14u), G[8]); put(rt, frame.at(0x8u), G[3]); put(rt, frame.at(0xCu), G[9]); call_here(rt, ctx, 0x08C2A004u);  G[18] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x34u)); }  } }
void func_08A95220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x14u), G[17]); G[22] = G[5]; G[20] = G[7]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xD8u); G[18] = G[4]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[20]); G[19] = G[2]; if (G[16] == 0u) { G[4] = 0x08CB211Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xD8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[19] + G[2]; G[4] = frame.at(0u); G[21] = G[2]; call_here(rt, ctx, 0x08A951B8u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CB2180u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[18]); G[5] = word(rt, G[17]); G[4] = G[18]; G[2] = word(rt, G[3] + 0xDCu); G[16] = word(rt, G[5] + 0xE0u); G[5] = 0u; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = G[17]; G[5] = 0u; G[7] = G[19]; call_here(rt, ctx, G[16]); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CB2180u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = word(rt, G[20]); G[18] = word(rt, G[2] + 0xE0u); if (G[16] == 0u) { G[4] = 0x08CB211Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xDCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = G[17]; G[5] = G[19]; G[7] = G[21]; call_here(rt, ctx, G[18]); G[5] = word(rt, frame.at(0u)); if (G[22] != frame.at(0u)) { G[4] = G[22]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[17] = word(rt, frame.at(0x14u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_306
} // namespace gt::game

#define HOT306(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_306::name, gt::ReturnKind::kind)
HOT306(0x0898071Cu, func_0898071C, Word);
HOT306(0x0898B53Cu, func_0898B53C, Word);
HOT306(0x08A1FCECu, func_08A1FCEC, Word);
HOT306(0x08918998u, func_08918998, Word);
HOT306(0x08805D9Cu, func_08805D9C, Word);
HOT306(0x08AAECB0u, func_08AAECB0, Word);
HOT306(0x0881E458u, func_0881E458, Word);
HOT306(0x089185DCu, func_089185DC, Word);
HOT306(0x089B99E4u, func_089B99E4, Word);
HOT306(0x0883E9A8u, func_0883E9A8, Word);
HOT306(0x08A53D4Cu, func_08A53D4C, Word);
HOT306(0x08A1F8FCu, func_08A1F8FC, Word);
HOT306(0x089E7AF4u, func_089E7AF4, Word);
HOT306(0x08A7C6D4u, func_08A7C6D4, Word);
HOT306(0x08A84B28u, func_08A84B28, Word);
// HOT306(0x08A95220u, func_08A95220, Word);  // no comparable synthetic case yet
