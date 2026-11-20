// Hot batch 305: eighty-three to eighty-five-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (visitor-name copies, script bindings, number formatting, a bounded counter,
// paired object setup, field lookups, lazy children and a virtual dispatcher).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_305 {
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

void func_089E3AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x38u), G[31]); G[2] = 0x08D70000u; G[21] = frame.at(4u); G[20] = frame.at(8u); G[19] = G[4] + 0x1ACu; G[4] = G[5]; G[18] = 0x08D74FD0u; G[2] = 0xFFFFFFFFu; G[3] = G[18] + 0xCu; G[17] = G[5]; G[6] = word(rt, G[5]); put(rt, frame.at(0x14u), G[2]); G[2] = 0x08CCD9D8u; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(8u), G[3]); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x18u), G[20]); G[2] = word(rt, G[6] + 8u); G[5] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0x10u)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(0xCu), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(2u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = G[20]; G[5] = G[17]; G[4] = G[21]; call_here(rt, ctx, 0x089ECF44u); G[4] = G[19]; G[5] = G[21]; call_here(rt, ctx, 0x08B855E0u); G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, frame.at(0x8u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); }  G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_089E3C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x38u), G[31]); G[2] = 0x08D70000u; G[21] = frame.at(4u); G[20] = frame.at(8u); G[19] = G[4] + 0x1A8u; G[4] = G[5]; G[18] = 0x08D74FD0u; G[2] = 0xFFFFFFFFu; G[3] = G[18] + 0xCu; G[17] = G[5]; G[6] = word(rt, G[5]); put(rt, frame.at(0x14u), G[2]); G[2] = 0x08CCD9D8u; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(8u), G[3]); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x18u), G[20]); G[2] = word(rt, G[6] + 8u); G[5] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0x10u)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(0xCu), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(2u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = G[20]; G[5] = G[17]; G[4] = G[21]; call_here(rt, ctx, 0x089ECF44u); G[4] = G[19]; G[5] = G[21]; call_here(rt, ctx, 0x08B855E0u); G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, frame.at(0x8u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); }  G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_089F0304(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x38u), G[31]); G[2] = 0x08D70000u; G[21] = frame.at(4u); G[20] = frame.at(8u); G[19] = G[4]; G[4] = G[5]; G[18] = 0x08D74FD0u; G[2] = 0xFFFFFFFFu; G[3] = G[18] + 0xCu; G[17] = G[5]; G[6] = word(rt, G[5]); put(rt, frame.at(0x14u), G[2]); G[2] = 0x08CCD9D8u; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(8u), G[3]); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x18u), G[20]); G[2] = word(rt, G[6] + 8u); G[5] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0x10u)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(0xCu), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(2u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = G[20]; G[5] = G[17]; G[4] = G[21]; call_here(rt, ctx, 0x089ECF44u); G[4] = G[19]; G[5] = G[21]; call_here(rt, ctx, 0x089F0054u); G[2] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, frame.at(0x8u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); }  G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_089F0450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x38u), G[31]); G[2] = 0x08D70000u; G[21] = frame.at(4u); G[20] = frame.at(8u); G[19] = G[4]; G[4] = G[5]; G[18] = 0x08D74FD0u; G[2] = 0xFFFFFFFFu; G[3] = G[18] + 0xCu; G[17] = G[5]; G[6] = word(rt, G[5]); put(rt, frame.at(0x14u), G[2]); G[2] = 0x08CCD9D8u; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(8u), G[3]); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x18u), G[20]); G[2] = word(rt, G[6] + 8u); G[5] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0x10u)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(0xCu), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(2u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = G[20]; G[5] = G[17]; G[4] = G[21]; call_here(rt, ctx, 0x089ECF44u); G[4] = G[19]; G[5] = G[21]; call_here(rt, ctx, 0x089EFEB4u); G[2] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, frame.at(0x8u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); }  G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08A03354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x38u), G[31]); G[2] = 0x08D70000u; G[21] = frame.at(4u); G[20] = frame.at(8u); G[19] = G[4] + 0x104u; G[4] = G[5]; G[18] = 0x08D74FD0u; G[2] = 0xFFFFFFFFu; G[3] = G[18] + 0xCu; G[17] = G[5]; G[6] = word(rt, G[5]); put(rt, frame.at(0x14u), G[2]); G[2] = 0x08CCD9D8u; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(8u), G[3]); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x18u), G[20]); G[2] = word(rt, G[6] + 8u); G[5] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0x10u)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(0xCu), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(2u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = G[20]; G[5] = G[17]; G[4] = G[21]; call_here(rt, ctx, 0x089ECF44u); G[4] = G[19]; G[5] = G[21]; call_here(rt, ctx, 0x08B855E0u); G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, frame.at(0x8u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); }  G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08A1FBA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x98u), G[18]); put(rt, frame.at(0x90u), G[16]); put(rt, frame.at(0x9Cu), G[31]); put(rt, frame.at(0x94u), G[17]); G[18] = G[4]; G[16] = G[6]; if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = word(rt, G[6]); G[4] = 0x08CA0000u; if (G[17] == 0u) { G[4] = 0x08CAA480u; call_here(rt, ctx, 0x08A5BB7Cu); } call_here(rt, ctx, 0x08A82A5Cu); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A76F20u); G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CAA480u; call_here(rt, ctx, 0x08A5BB7Cu); } if (G[2] != 0u) { G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x40u); G[17] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[4] = G[2]; } else { G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[17] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[3] = sra(G[2], 31); G[4] = G[2]; } G[5] = G[3]; G[6] = G[17]; G[7] = 0x80u; G[16] = frame.at(4u); call_here(rt, ctx, 0x08ABE8B8u); G[5] = G[17]; G[4] = G[16]; G[17] = frame.at(8u); G[6] = frame.at(1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0x8u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x98u)); G[16] = word(rt, frame.at(0x90u)); G[17] = word(rt, frame.at(0x94u)); }  } }
void func_08806408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[20] = G[4]; G[17] = G[5]; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08C97DA8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[18] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = G[17]; G[4] = G[18]; G[19] = G[2]; call_here(rt, ctx, 0x08A57854u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C97DD8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16] + 0x1Cu); G[17] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[16] + 0x18u); if (G[16] == 0u) { G[4] = 0x08C97DBCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[16] + 0xCu); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, G[17]); G[5] = word(rt, G[17] + 4u); if (G[2] != 0u) { G[5] = G[19] + G[5]; G[2] = static_cast<std::int32_t>(G[5]) < 100 ? 1u : 0u; put(rt, G[17] + 4u, G[5]); if (G[2] == 0u) { G[5] = 100u; put(rt, G[17] + 4u, G[5]); put(rt, G[17], 0u); } else { G[2] = static_cast<std::int32_t>(G[5]) < 60 ? 1u : 0u; if (G[2] == 0u) { G[2] = 1u; put(rt, G[17], G[2]); } else { G[2] = static_cast<std::int32_t>(G[5]) < 30 ? 1u : 0u; if (G[2] == 0u) { G[2] = 2u; put(rt, G[17], G[2]); } else { G[2] = static_cast<std::int32_t>(G[5]) < 15 ? 1u : 0u; bool lo = G[2] != 0u; G[2] = 3u; if (!lo) put(rt, G[17], G[2]); } } } } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[20] != frame.at(0u)) { G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08826E68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[6] = static_cast<std::int32_t>(G[6]) < 3 ? 1u : 0u; G[21] = G[4]; G[17] = G[7]; if (G[6] == 0u) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C99B4Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[18]; G[22] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); G[20] = G[2]; if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[19] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = G[17] + 8u; G[4] = G[19]; G[18] = G[2]; call_here(rt, ctx, 0x0884D660u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9A194u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0884D3D8u); G[16] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; G[5] = G[20]; G[6] = G[18]; G[7] = G[16]; call_here(rt, ctx, 0x0885C1D0u); G[4] = frame.at(0u); G[5] = 0u; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[17] = word(rt, frame.at(0x14u)); G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x089D64B8 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CA6E2C, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08965418() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then the registrations: 0x08B8A004(object, 0x08CF6F24, 0x089D5E94), 0x08B8D51C(object, 0x08CA6E38, 0x089D60F4, 0),
// 0x08B878E8(object, 0x08CA6E44, 0x089D6190, 0x089D6190), 0x08B878E8(object, 0x08CA6E4C, 0x089D6284, 0x089D6284) and 0x08B8D51C(object, 0x08CA6E54, 0x089D6070, 0).
void func_089D64B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA6E2Cu, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08965418u, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x089D5E94u});
    call_set(rt, ctx, 0x08B8D51Cu, {object, 0x08CA6E38u, 0x089D60F4u, 0u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CA6E44u, 0x089D6190u, 0x089D6190u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CA6E4Cu, 0x089D6284u, 0x089D6284u});
    call_set(rt, ctx, 0x08B8D51Cu, {object, 0x08CA6E54u, 0x089D6070u, 0u});
}
// 0x08A0DDC0 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CA9574, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A003B4() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then the registrations: 0x08B8850C(object, 0x08CF6F14, 0x08A0DA0C) and 0x08B878E8(object, name, handler, handler) with the names
// 0x08CA9494 / 0x08A0DA4C, 0x08CA94A4 / 0x08A0DB48, 0x08CA94B4 / 0x08A0DC44 and 0x08CA94C4 / 0x08A0D870.
void func_08A0DDC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA9574u, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08A003B4u, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A0DA0Cu});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CA9494u, 0x08A0DA4Cu, 0x08A0DA4Cu});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CA94A4u, 0x08A0DB48u, 0x08A0DB48u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CA94B4u, 0x08A0DC44u, 0x08A0DC44u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CA94C4u, 0x08A0D870u, 0x08A0D870u});
}
void func_088E3FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); G[20] = 0u; call_here(rt, ctx, 0x08B092F0u); G[16] = G[2]; call_here(rt, ctx, 0x08B09308u); G[17] = G[2]; if (G[16] == 0u) { G[16] = G[2]; G[20] = 1u; G[17] = 0u; } G[2] = 0x10u; if (G[16] == G[2]) { G[2] = 0x100u; G[4] = 0x5C0u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = 1u; call_here(rt, ctx, 0x088EB088u); G[19] = G[16]; } else { G[2] = 0x100u; G[19] = 0u; if (G[16] == G[2]) { G[4] = 0x5C0u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x088EAA7Cu); G[19] = G[16]; } } G[2] = 0x10u; if (G[17] == G[2]) { G[2] = 0x100u; G[4] = 0x5C0u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = 1u; call_here(rt, ctx, 0x088EB088u); G[18] = G[16]; } else { G[2] = 0x100u; G[18] = 0u; if (G[17] == G[2]) { G[4] = 0x5C0u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x088EAA7Cu); G[18] = G[16]; } } if (G[19] != 0u) { G[2] = 1u; if (G[20] != 0u) put(rt, G[19] + 0x534u, G[2]); } G[2] = 2u; if (G[18] != 0u) put(rt, G[18] + 0x534u, G[2]); G[16] = 0u; if (G[19] != 0u) { G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[4] = G[2]; G[5] = G[19]; G[6] = G[18]; G[16] = G[2]; call_here(rt, ctx, 0x088E4114u); } G[2] = G[16]; G[20] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_089CF584(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); G[18] = G[5]; G[5] = G[6]; G[17] = G[4]; G[4] = G[18]; G[20] = G[7]; G[19] = G[6]; call_here(rt, ctx, 0x089CE09Cu); G[5] = 0x08CF6ED8u; G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08969188u); G[2] = G[17]; if (G[16] != 0u) { G[16] = 0x08CF0000u; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); G[4] = G[18]; G[5] = G[19]; call_here(rt, ctx, 0x089CE1F4u); G[3] = G[2]; G[2] += 0xCu; if (G[17] != G[2]) { G[5] = word(rt, G[3] + 0xCu); G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[17]; if (G[20] != 0u) { G[2] = word(rt, G[17]); bool empty = G[2] == 0u; G[2] = G[17]; if (empty) { G[4] = frame.at(0u); G[5] = G[18]; call_here(rt, ctx, 0x089CD49Cu); G[16] = word(rt, frame.at(0u)); G[21] = frame.at(4u); if (G[16] == 0u) { G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = G[16]; G[4] = G[21]; G[16] = frame.at(8u); call_here(rt, ctx, 0x08A77444u); G[4] = G[16]; G[5] = G[21]; call_here(rt, ctx, 0x08969244u); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; G[5] = G[19]; G[6] = G[20]; G[7] = G[17]; call_here(rt, ctx, 0x089CF330u); G[2] = G[17]; } } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); }  } }
void func_0882736C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[6] = static_cast<std::int32_t>(G[6]) < 3 ? 1u : 0u; G[21] = G[4]; G[17] = G[7]; if (G[6] == 0u) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C99B4Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[18]; G[22] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); G[20] = G[2]; if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[19] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = G[17] + 8u; G[4] = G[19]; G[18] = G[2]; call_here(rt, ctx, 0x0884D660u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9A194u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0884D3D8u); G[16] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; G[5] = G[20]; G[6] = G[18]; G[7] = G[16]; call_here(rt, ctx, 0x0885C250u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[17] = word(rt, frame.at(0x14u)); G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0880B184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[2] = G[4]; G[20] = G[4]; G[5] = G[6]; G[3] = 0u; G[4] = 4u; do { G[3] += 1u; put(rt, G[2] + 0xCu, 0u); G[2] += 4u; } while (G[3] != G[4]); G[16] = frame.at(0xCu); G[4] = G[16]; call_here(rt, ctx, 0x08969244u); G[17] = word(rt, frame.at(0xCu)); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = frame.at(8u); G[16] = frame.at(0x10u); put(rt, G[20] + 0xCu, G[17]); G[5] = 0x08C98400u; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = G[16]; call_here(rt, ctx, 0x08969160u); G[6] = G[18]; G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A7BFD0u); G[17] = word(rt, frame.at(0x10u)); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); put(rt, G[20] + 0x10u, G[17]); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[18] = frame.at(4u); G[19] = word(rt, G[20] + 0x10u); G[5] = 0x08C98408u; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = G[16]; call_here(rt, ctx, 0x08969160u); G[6] = G[18]; G[5] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08A7BFD0u); G[17] = word(rt, frame.at(0x10u)); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); put(rt, G[20] + 0x14u, G[17]); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[5] = 0x08C98410u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69B3Cu); G[4] = G[16]; call_here(rt, ctx, 0x08969160u); G[6] = frame.at(0u); G[5] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08A7BFD0u); G[17] = word(rt, frame.at(0x10u)); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); put(rt, G[20] + 0x18u, G[17]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[2] = 1u; store8(G[20] + 8u, G[2]); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_0887BB3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = word(rt, G[4] + 0x24u); G[2] = (G[2] >> 30) & 1u; G[17] = G[4]; bool go = true; if (G[2] == 0u) { G[2] = word(rt, G[4] + 0x1Cu); G[3] = word(rt, G[4] + 0x20u); G[4] = word(rt, G[4] + 0x18u); put(rt, G[17] + 0x10u, G[2]); put(rt, G[17] + 0x14u, G[3]); put(rt, G[17] + 0xCu, G[4]); if (static_cast<std::int32_t>(G[4]) <= 0) { go = false; } else { G[18] = 0u; G[16] = G[17] + 0x2Cu; do { G[5] = word(rt, G[17] + 0x10u); G[6] = word(rt, G[17] + 0x14u); G[4] = G[16]; G[18] += 1u; call_here(rt, ctx, 0x0887B980u); G[4] = word(rt, G[17] + 0xCu); G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; G[16] += 0x38u; } while (G[2] != 0u); G[2] = word(rt, G[17] + 0x24u); G[3] = 0x40000000u; G[2] |= G[3]; put(rt, G[17] + 0x24u, G[2]); } } else { G[4] = word(rt, G[4] + 0xCu); } if (go && static_cast<std::int32_t>(G[4]) > 0) { G[2] = word(rt, G[17]); if (G[2] == 0u) { G[4] = 0x64u; call_here(rt, ctx, 0x08B62344u); G[5] = word(rt, G[17] + 0xCu); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x088A31B4u); put(rt, G[17], G[16]); G[2] = word(rt, G[16]); G[3] = word(rt, G[2] + 8u); G[4] = G[16]; call_here(rt, ctx, G[3]); G[4] = word(rt, G[17]); G[5] = 0u; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x68u); G[6] = 0u; call_here(rt, ctx, G[2]); } } G[2] = word(rt, G[17] + 4u); if (G[2] == 0u) { G[4] = 0x44u; call_here(rt, ctx, 0x08B62344u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x088982C4u); G[2] = word(rt, G[17]); put(rt, G[17] + 4u, G[16]); G[4] = G[16]; G[5] = word(rt, G[2] + 0x54u); call_here(rt, ctx, 0x08898664u); G[4] = word(rt, G[17] + 4u); G[2] = word(rt, G[4]); G[25] = word(rt, G[2] + 0x14u); G[5] = 0u; G[6] = 0u; G[7] = 0u; tail = true; } G[17] = word(rt, frame.at(0x4u)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, G[25]); } } }
void func_088DCBD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x0u), G[16]); G[21] = G[7]; G[20] = G[6]; G[19] = G[8]; G[18] = G[5]; G[16] = word(rt, G[8] + 0x134u); G[17] = G[4]; if (G[16] != 0u) { G[2] = 0xFFFFFFFFu; G[4] = G[16]; G[5] = G[17]; if (G[18] == G[2]) { G[2] = word(rt, G[16]); G[25] = word(rt, G[2] + 0x1Cu); tail = true; } else { G[6] = G[18]; G[7] = G[7] != 0u ? 1u : 0u; if (G[20] == 0u) { G[2] = word(rt, G[16]); G[25] = word(rt, G[2] + 0x24u); tail = true; } else { G[4] = G[8]; call_here(rt, ctx, 0x088DC90Cu); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; G[7] = G[20]; G[8] = G[21]; if (G[2] == 0u) { G[2] = word(rt, G[16]); G[25] = word(rt, G[2] + 0x20u); } else { G[4] = G[19]; G[5] = G[17]; G[6] = G[18]; G[7] = 0u; G[25] = 0x088D74DCu; } tail = true; } } } G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, G[25]); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_305
} // namespace gt::game

#define HOT305(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_305::name, gt::ReturnKind::kind)
HOT305(0x089E3AC0u, func_089E3AC0, Word);
HOT305(0x089E3C0Cu, func_089E3C0C, Word);
HOT305(0x089F0304u, func_089F0304, Word);
HOT305(0x089F0450u, func_089F0450, Word);
HOT305(0x08A03354u, func_08A03354, Word);
HOT305(0x08A1FBA0u, func_08A1FBA0, Word);
HOT305(0x08806408u, func_08806408, Word);
HOT305(0x08826E68u, func_08826E68, Word);
HOT305(0x089D64B8u, func_089D64B8, Word);
HOT305(0x08A0DDC0u, func_08A0DDC0, Word);
HOT305(0x088E3FB8u, func_088E3FB8, Word);
HOT305(0x089CF584u, func_089CF584, Word);
HOT305(0x0882736Cu, func_0882736C, Word);
// HOT305(0x0880B184u, func_0880B184, Word);  // no comparable synthetic case yet
HOT305(0x0887BB3Cu, func_0887BB3C, Word);
HOT305(0x088DCBD4u, func_088DCBD4, Word);
