// Hot batch 249: sixty-five- and sixty-six-instruction functions no route reaches, written by hand from `tools.disasm` and
// verified by synthetic cases (variable-length reads, stream reads, destructors and handle-guarded calls).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_249 {
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
    if (!(value >= -2497483648.0f && value < 2497483648.0f)) return 0x7FFFFFFF;
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

void func_08C0C414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[17]); G[17] = G[4]; G[4] = G[8]; put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); G[18] = G[5]; put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[31]); G[16] = G[6]; call_here(rt, ctx, 0x08AA2A30u); G[19] = G[2]; G[5] = word(rt, G[17] + 0x18u); G[16] = word(rt, G[16]); G[6] = word(rt, G[17] + 0x14u); G[4] = sra(G[5], 1); G[3] = G[5] & 1u; G[7] = G[6]; G[4] = G[16] + G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4]); G[2] += G[6]; G[7] = word(rt, G[2]); } else { G[4] = sra(G[5], 1); G[4] = G[16] + G[4]; } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[17] + 0x20u); G[3] = sra(G[2], 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[17] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[17] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[19]; call_here(rt, ctx, G[2]); G[16] = frame.at(4u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x089FA280u); G[4] = frame.at(0u); G[5] = G[16]; call_here(rt, ctx, 0x08C0C384u); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x089F9B74u); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_08C6F250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); G[5] = 0u; G[23] = G[4]; G[4] = 4u; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[22]); G[22] = 0x08CB0000u; put(rt, frame.at(0x14u), G[21]); G[21] = G[6]; put(rt, frame.at(0x10u), G[20]); G[20] = G[11]; put(rt, frame.at(0xCu), G[19]); G[19] = G[9]; put(rt, frame.at(8u), G[18]); G[18] = G[8]; put(rt, frame.at(4u), G[17]); G[17] = 0u; put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B79794u); G[4] = G[2]; G[5] = 0x08CB5818u; G[16] = G[2]; call_here(rt, ctx, 0x08B35FD8u); G[4] = G[16]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B35F60u); G[4] = G[2] + 1u; call_here(rt, ctx, 0x08B6461Cu); G[4] = G[2]; G[5] = G[16]; G[17] = G[2]; call_here(rt, ctx, 0x08B7BB6Cu); G[5] = 0x08CB5818u; G[4] = 4u; call_here(rt, ctx, 0x08B79794u); } G[4] = G[23]; G[5] = G[21]; G[6] = G[20]; G[8] = G[18]; G[9] = G[19]; call_here(rt, ctx, 0x08B37640u); G[16] = G[2]; G[4] = 4u; G[5] = G[17]; if (G[17] != 0u) { call_here(rt, ctx, 0x08B79794u); G[4] = G[17]; call_here(rt, ctx, 0x08B61FC0u); } G[2] = G[16]; G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08C79B94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = G[5] + 4u; put(rt, frame.at(0x10u), G[20]); G[20] = G[5] + 8u; put(rt, frame.at(0xCu), G[19]); G[19] = G[6]; put(rt, frame.at(8u), G[18]); G[18] = G[7]; put(rt, frame.at(4u), G[17]); G[17] = G[4] + 4u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x18u), G[31]); G[10] = word(rt, G[5]); G[3] = word(rt, G[5] + 4u); G[11] = word(rt, G[5] + 0xCu); G[8] = word(rt, G[10] - 0xCu); G[2] = word(rt, G[3] - 0xCu); G[9] = word(rt, G[5] + 8u); G[8] = G[4] + G[8]; G[2] = G[4] + G[2]; put(rt, G[2], G[9]); put(rt, G[4], G[10]); G[4] = G[17]; put(rt, G[8], G[11]); call_here(rt, ctx, 0x08C771D4u); G[2] = word(rt, G[16]); G[5] = G[17]; G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; call_here(rt, ctx, 0x08C54A7Cu); G[4] = G[17]; G[5] = G[19]; G[6] = G[18] | 0x10u; call_here(rt, ctx, 0x08C7835Cu); if (G[2] != 0u) { G[2] = word(rt, G[16]); G[5] = 0u; G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; call_here(rt, ctx, 0x08C54CFCu); } else { G[2] = word(rt, G[16]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); G[5] |= 4u; call_here(rt, ctx, 0x08C54CFCu); } G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08C6F354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); G[5] = 0u; G[23] = G[4]; G[4] = 4u; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[22]); G[22] = 0x08CB0000u; put(rt, frame.at(0x14u), G[21]); G[21] = G[6]; put(rt, frame.at(0x10u), G[20]); G[20] = G[11]; put(rt, frame.at(0xCu), G[19]); G[19] = G[9]; put(rt, frame.at(8u), G[18]); G[18] = G[8]; put(rt, frame.at(4u), G[17]); G[17] = 0u; put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B79794u); G[4] = G[2]; G[5] = 0x08CB5818u; G[16] = G[2]; call_here(rt, ctx, 0x08B35FD8u); G[4] = G[16]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B35F60u); G[4] = G[2] + 1u; call_here(rt, ctx, 0x08B6461Cu); G[4] = G[2]; G[5] = G[16]; G[17] = G[2]; call_here(rt, ctx, 0x08B7BB6Cu); G[5] = 0x08CB5818u; G[4] = 4u; call_here(rt, ctx, 0x08B79794u); } G[4] = G[23]; G[5] = G[21]; G[6] = G[20]; G[8] = G[18]; G[9] = G[19]; call_here(rt, ctx, 0x08B37640u); G[16] = G[2]; G[4] = 4u; G[5] = G[17]; if (G[17] != 0u) { call_here(rt, ctx, 0x08B79794u); G[4] = G[17]; call_here(rt, ctx, 0x08B61FC0u); } G[2] = G[16]; G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08A6DB54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); G[19] = G[5]; put(rt, frame.at(0x28u), G[18]); G[18] = frame.at(8u); put(rt, frame.at(0x24u), G[17]); G[17] = G[4]; G[4] = 0x08CB06B8u; put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x20u), G[16]); G[16] = word(rt, G[7]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(4u); call_here(rt, ctx, G[2]); G[4] = word(rt, G[17] + 0x14u); G[2] = word(rt, frame.at(4u)); G[7] = frame.at(0x10u); G[16] = word(rt, G[4] - 0xCu); G[5] = G[2]; put(rt, frame.at(0xCu), G[16]); G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(0xCu); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[7] = G[2]; put(rt, frame.at(0x10u), G[17]); G[16] -= G[17]; G[6] = word(rt, G[7]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; G[4] = G[18]; G[16] = ~G[16]; G[5] = G[16] >> 31; call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[19]; if (G[19] != G[18]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_0881F9E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = 0u; put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[5]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x14u), G[17]); G[17] = G[6]; put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08C98EFCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[21] = G[2]; } G[2] = static_cast<std::int32_t>(G[17]) < 2 ? 1u : 0u; G[17] = 0u; if (G[2] == 0u) { G[16] = word(rt, G[18] + 4u); if (G[16] == 0u) { G[4] = 0x08C98EFCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[17] = G[2]; } G[5] = G[19]; G[4] = frame.at(0u); call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[16] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = G[20]; G[6] = G[21]; G[7] = G[17]; call_here(rt, ctx, 0x0881F5CCu); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0882A84C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; put(rt, frame.at(0x14u), G[17]); G[17] = G[7]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); if (G[6] == 0u) { G[4] = frame.at(0u); call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = frame.at(0u); G[20] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); G[19] = G[2]; if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xDCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); G[18] = G[2]; if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xD8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[7] = G[2]; G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; call_here(rt, ctx, 0x0885E68Cu); } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// 0x088B5414 (a0 = the command, a1 = its argument, both a command word of 1 or 0 and 0xFFFF): command 1 bumps the 64-bit counter at
// 0x08D75F48 / 0x08D75F4C (the carry added to the high word) and installs 0x08CC3288 in 0x08CE3034; when the counter wraps to zero and the
// byte at 0x08D75F50 is clear, 0x08B6441C(0x08D75F50) deciding, the record at 0x08D75F5C is filled (0x1001 at +0x18, 0x08D75F68 at +0xC and
// +0x14, zero at +0, +4, +8, +0x10, +0x1C, +0x20 and +0x24), 0x08B64494(0x08D75F50, 0x08D75F68) is called and 0x08B77B1C(0x088B4E94)
// tail-called. Command 0 with the same argument installs 0x08CC3200 instead. v0 keeps 0xFFFF until one of those takes over.
void func_088B5414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 command = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[8] = command;                          // t0
    if (command != 1u) {
        ctx.gpr[2] = 0xFFFFu;
        if (command != 0u) return;
        if (argument != 0xFFFFu) return;
        put(rt, 0x08CE3034u, 0x08CC3200u);
        ctx.gpr[2] = 0x08CC3200u;                  // the stored value is what the load left in v0
        return;
    }
    ctx.gpr[2] = 0xFFFFu;
    if (argument != 0xFFFFu) return;
    ctx.gpr[16] = 0x08D75F50u;
    const u32 low = word(rt, 0x08D75F48u);
    const u32 high = word(rt, 0x08D75F4Cu);
    const u32 next_low = low + 1u;
    const u32 next_high = (next_low < low ? 1u : 0u) + high;
    put(rt, 0x08CE3034u, 0x08CC3288u);
    put(rt, 0x08D75F48u, next_low);
    put(rt, 0x08D75F4Cu, next_high);
    ctx.gpr[2] = 0x08CC3288u;
    if (next_low != command) return;
    if (next_high != 0u) return;
    ctx.gpr[2] = 0x08D70000u;                      // the relocated `lui v0` half stays in v0
    if (signed_byte(rt, 0x08D75F50u) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {0x08D75F50u}) == 0u) return;
    put(rt, 0x08D75F5Cu + 0x18u, 0x1001u);
    put(rt, 0x08D75F5Cu + 0xCu, 0x08D75F68u);
    put(rt, 0x08D75F5Cu + 0x10u, 0u);
    put(rt, 0x08D75F5Cu + 0x4u, 0u);
    put(rt, 0x08D75F5Cu + 0x8u, 0u);
    put(rt, 0x08D75F5Cu + 0x14u, 0x08D75F68u);
    put(rt, 0x08D75F5Cu + 0x1Cu, 0u);
    put(rt, 0x08D75F5Cu + 0x20u, 0u);
    put(rt, 0x08D75F5Cu + 0x24u, 0u);
    put(rt, 0x08D75F5Cu, 0u);                      // the store of the call's delay slot
    call_set(rt, ctx, 0x08B64494u, {0x08D75F50u, 0x08D75F68u});
    tail_call(ctx, 0x08B77B1Cu, {0x088B4E94u});
}
void func_089A90B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); G[22] = G[4]; G[4] = G[6] << 3; put(rt, frame.at(0x14u), G[21]); G[21] = G[7]; put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(8u), G[18]); G[18] = G[6]; put(rt, frame.at(0u), G[16]); G[16] = G[5]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); call_here(rt, ctx, 0x08B6461Cu); G[20] = G[2]; if (G[18] != 0u) { G[2] = 1u; for (;;) { const bool more = G[2] != G[18]; G[2] += 1u; if (!more) break; } } if (static_cast<std::int32_t>(G[18]) <= 0) { G[2] = word(rt, G[22]); } else { G[17] = G[16]; G[19] = 0u; G[16] = G[20]; do { G[4] = G[16]; G[5] = G[17]; G[6] = G[21]; G[19] += 1u; call_here(rt, ctx, 0x08B4AC54u); G[16] += 8u; G[17] += 8u; } while (G[18] != G[19]); G[2] = word(rt, G[22]); } G[3] = 0x08C90000u; G[5] = word(rt, 0x08C8CC10u); G[10] = word(rt, G[2] + 0x10Cu); G[4] = G[22]; G[6] = G[18]; G[7] = G[20]; G[8] = 0u; G[9] = 0u; call_here(rt, ctx, G[10]); G[4] = G[20]; G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); if (G[4] != 0u) goto tail; } return; tail: jump(ctx, 0x08B61FC0u); } }
void func_089D04F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); G[2] = 0x08CC9D60u; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; G[18] = G[17] + 0x160u; put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, G[4], G[2]); G[2] = 0x08CC9FB8u; put(rt, G[17] + 0x38u, G[2]); G[4] += 0x188u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x184u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x180u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x17Cu; call_here(rt, ctx, 0x08A5B9ECu); G[16] = G[17] + 0x178u; if (G[18] != 0u) { const bool any = G[18] != G[16]; G[16] -= 0xCu; if (any) { G[19] = frame.at(1u); bool more; do { G[4] = word(rt, G[16]); G[5] = word(rt, G[16] + 4u); G[6] = G[19]; call_here(rt, ctx, 0x08C02614u); G[2] = word(rt, G[16]); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] -= G[2]; G[5] = sra(G[5], 2); G[5] <<= 2; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BC54u); more = G[18] != G[16]; G[16] -= 0xCu; } while (more); } } G[4] = G[17] + 0x154u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x150u); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[17]; call_here(rt, ctx, 0x08995714u); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// 0x08A123E8 (a0 = the object): the +0x38 holder replacement with the format 0x08CA9930, built at frame +4 by
// 0x08B85EB8(text, 0x08CA9930, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A76A94() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B878E8(object, 0x08CA997C, 0x08A122E0, 0x08A12218) and
// 0x08B878E8(object, 0x08CA9988, 0x08A12364, 0x08A12190) register two attributes. Returns the last call's result.
void func_08A123E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA9930u, frame.at(0x1u)});
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
    call_set(rt, ctx, 0x08B878E8u, {self, 0x08CA997Cu, 0x08A122E0u, 0x08A12218u});
    call_set(rt, ctx, 0x08B878E8u, {self, 0x08CA9988u, 0x08A12364u, 0x08A12190u});
}
void func_08A9DEE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CD48C0u; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, G[4], G[2]); G[4] = 1u; call_here(rt, ctx, 0x08A9DCB0u); G[4] = G[16] + 0x78u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x74u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x4Cu; call_here(rt, ctx, 0x08B28DC4u); G[3] = word(rt, G[16] + 0x48u); G[2] = 0x08D70000u; G[18] = 0x08D74FD0u; G[17] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[17] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, G[16] + 0x44u); G[5] = 0xFFFFFFFFu; G[17] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[17]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x38u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x34u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x2Cu; call_here(rt, ctx, 0x08C2F9F0u); G[4] = G[16] + 0x24u; call_here(rt, ctx, 0x08C2F9A4u); G[4] = G[16] + 0xCu; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C301B0u); G[4] = G[16]; call_here(rt, ctx, 0x08A5BB5Cu); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08AA4A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 1u; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); if (G[5] == G[2]) { G[17] = word(rt, G[6]); if (G[17] == 0u) { G[4] = 0x08CB2CD4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = frame.at(4u); G[5] = G[17]; G[3] = word(rt, G[2] + 0xCu); G[4] = G[16]; G[17] = frame.at(8u); call_here(rt, ctx, G[3]); G[5] = G[16]; G[4] = G[17]; G[16] = frame.at(0xCu); call_here(rt, ctx, 0x08AA4748u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0xCu)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[17] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = word(rt, frame.at(4u)); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, frame.at(4u)); } } else { G[4] = word(rt, frame.at(4u)); } G[16] = G[4] - 0xCu; if (G[17] != G[16]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08AA4B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 1u; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); if (G[5] == G[2]) { G[17] = word(rt, G[6]); if (G[17] == 0u) { G[4] = 0x08CB2CD4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = frame.at(4u); G[5] = G[17]; G[3] = word(rt, G[2] + 0xCu); G[4] = G[16]; G[17] = frame.at(8u); call_here(rt, ctx, G[3]); G[5] = G[16]; G[4] = G[17]; G[16] = frame.at(0xCu); call_here(rt, ctx, 0x08AA4850u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0xCu)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[17] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = word(rt, frame.at(4u)); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, frame.at(4u)); } } else { G[4] = word(rt, frame.at(4u)); } G[16] = G[4] - 0xCu; if (G[17] != G[16]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08B51400(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); auto& F = ctx.fpr; F[4] = float_at(rt, frame.at(0x10u)); F[10] = float_at(rt, frame.at(0x18u)); F[5] = float_at(rt, frame.at(0x1Cu)); F[9] = float_at(rt, frame.at(0x28u)); F[3] = float_at(rt, frame.at(0x2Cu)); put_float(rt, frame.at(8u), F[22]); F[7] = psp_mul(F[13], F[4]); F[1] = psp_mul(F[9], F[19]); F[22] = float_at(rt, frame.at(0x14u)); put_float(rt, frame.at(0xCu), F[23]); F[0] = psp_mul(F[12], F[17]); F[23] = psp_mul(F[16], F[13]); put_float(rt, frame.at(0u), F[20]); F[8] = psp_mul(F[10], F[3]); F[20] = psp_mul(F[9], F[5]); F[6] = psp_mul(F[12], F[22]); F[2] = psp_mul(F[3], F[18]); F[11] = float_at(rt, frame.at(0x24u)); put_float(rt, frame.at(4u), F[21]); F[0] = F[0] - F[23]; F[2] = F[2] - F[1]; F[21] = float_at(rt, frame.at(0x20u)); F[1] = psp_mul(F[5], F[18]); F[8] = F[8] - F[20]; F[6] = F[6] - F[7]; F[13] = psp_mul(F[13], F[21]); F[7] = psp_mul(F[10], F[19]); F[12] = psp_mul(F[12], F[11]); F[6] = psp_mul(F[6], F[2]); F[0] = psp_mul(F[0], F[8]); F[2] = psp_mul(F[16], F[22]); F[1] = F[1] - F[7]; F[12] = F[12] - F[13]; F[7] = psp_mul(F[17], F[4]); F[3] = psp_mul(F[3], F[14]); F[9] = psp_mul(F[9], F[15]); F[17] = psp_mul(F[17], F[21]); F[0] = F[0] - F[6]; F[12] = psp_mul(F[12], F[1]); F[3] = F[3] - F[9]; F[2] = F[2] - F[7]; F[16] = psp_mul(F[16], F[11]); F[5] = psp_mul(F[5], F[14]); F[10] = psp_mul(F[10], F[15]); F[22] = psp_mul(F[22], F[21]); F[0] = F[0] + F[12]; F[2] = psp_mul(F[2], F[3]); F[16] = F[16] - F[17]; F[5] = F[5] - F[10]; F[4] = psp_mul(F[4], F[11]); F[19] = psp_mul(F[19], F[14]); F[18] = psp_mul(F[18], F[15]); F[0] = F[0] + F[2]; F[4] = F[4] - F[22]; F[16] = psp_mul(F[16], F[5]); F[19] = F[19] - F[18]; F[23] = float_at(rt, frame.at(0xCu)); F[22] = float_at(rt, frame.at(8u)); F[0] = F[0] - F[16]; F[4] = psp_mul(F[4], F[19]); F[21] = float_at(rt, frame.at(4u)); F[20] = float_at(rt, frame.at(0u)); F[0] = F[0] + F[4]; } } }
void func_08AEC6F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); call_here(rt, ctx, 0x08AEC800u); G[2] = 0x1000u; G[5] = load16(G[16] + 0x30u); G[3] = sext(G[5], 16); if (G[3] == G[2]) { G[18] = 0x08D40000u; G[2] = 0x08D41AD0u; G[3] = word(rt, G[2] + 0x48Cu); G[4] = word(rt, G[2] + 0x484u); G[6] = 0u; goto cmp; } G[2] = 0x1001u; G[6] = 0u; if (G[3] != G[2]) goto out; G[18] = 0x08D40000u; G[2] = 0x08D41AD0u; G[3] = word(rt, G[2] + 0x488u); G[4] = word(rt, G[2] + 0x480u); cmp: G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; if (G[3] == 0u) goto out; G[4] = sext(G[5], 16); call_here(rt, ctx, 0x08C7AB48u); G[6] = 0u; if (static_cast<std::int32_t>(G[2]) < 0) goto out; store8(G[16] + 0x34u, G[2]); G[17] = 0x08D41AD0u; G[5] = 1u; G[4] = word(rt, G[17] + 0x474u); call_here(rt, ctx, 0x08C7AA20u); G[4] = sext(load16(G[16] + 0x30u), 16); G[2] = 0x1000u; if (G[4] == G[2]) { G[2] = word(rt, G[17] + 0x48Cu); G[2] += 1u; put(rt, G[17] + 0x48Cu, G[2]); } else { G[2] = 0x1001u; if (G[4] == G[2]) { G[2] = word(rt, G[17] + 0x488u); G[2] += 1u; put(rt, G[17] + 0x488u, G[2]); } } G[2] = 0x08D41AD0u; G[4] = word(rt, G[2] + 0x474u); G[5] = 1u; call_here(rt, ctx, 0x08C7AA08u); G[6] = 1u; out: G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); G[2] = G[6]; } } }
void func_08B44420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = sext(G[5], 16); G[2] = G[18] << 1; G[3] = static_cast<std::int32_t>(G[18]) < 0x20 ? 1u : 0u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[2] + G[4]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x10u), G[16]); if (G[3] == 0u) goto zero; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x90u); G[16] = sext(load16(G[19] + 0x948u), 16); call_here(rt, ctx, G[2]); G[4] = G[17]; if (G[16] == G[2]) goto zero; G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0xA0u); call_here(rt, ctx, G[2]); G[4] = G[17]; if (G[2] != 0u) { G[2] = sext(load16(G[19] + 0x948u), 16); if (G[2] != 0u) { G[2] = 0u; goto out; } G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0x90u); call_here(rt, ctx, G[2]); G[6] = sext(G[2], 16); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x08B40B48u); G[6] = sext(load16(G[19] + 0x948u), 16); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x08B43370u); G[2] = 1u; goto out; } G[3] = word(rt, G[17]); put(rt, frame.at(8u), 0u); put(rt, frame.at(0u), 0u); store16(frame.at(8u), G[18]); put(rt, frame.at(4u), 0u); G[2] = word(rt, G[3] + 0x90u); call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = 9u; G[6] = frame.at(0u); G[7] = 0xCu; G[8] = 1u; store16(frame.at(0xAu), G[2]); call_here(rt, ctx, 0x08B429D0u); goto out; zero: G[2] = 0u; out: G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08ACE5CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x2030u); put(rt, frame.at(0x2020u), G[20]); G[20] = 0xFFFFFFFFu; put(rt, frame.at(0x201Cu), G[19]); G[19] = G[5] + 0x80u; put(rt, frame.at(0x2014u), G[17]); G[17] = G[5]; put(rt, frame.at(0x2024u), G[31]); put(rt, frame.at(0x2018u), G[18]); put(rt, frame.at(0x2010u), G[16]); for (;;) { G[4] = G[19]; call_here(rt, ctx, 0x08B344E0u); G[16] = load8(G[17] + 0xEDu); G[2] = load8(G[17] + 0xECu); G[4] = G[19]; G[16] &= 0xFFu; G[18] = G[2] & 0xFFu; call_here(rt, ctx, 0x08B3457Cu); G[4] = G[17] + 0x28u; if (G[16] == 0u) break; if (G[18] != 0u) call_here(rt, ctx, 0x08B28C44u); G[4] = word(rt, G[17] + 0xA8u); call_here(rt, ctx, 0x08B2450Cu); G[5] = frame.at(0u); G[6] = 1u; if (G[2] == G[20]) break; G[4] = word(rt, G[17] + 0xA8u); call_here(rt, ctx, 0x08B2432Cu); ctx.fpr[12] = float_at(rt, G[17] + 0xD8u); G[4] = 0u; ctx.fpr[13] = ctx.fpr[12]; call_here(rt, ctx, 0x08AF0A74u); G[6] = word(rt, frame.at(0x2000u)); G[5] = frame.at(0u); G[7] = 1u; G[4] = 0u; call_here(rt, ctx, 0x08AF0B78u); G[4] = G[19]; call_here(rt, ctx, 0x08B344E0u); ctx.fpr[0] = float_at(rt, G[17] + 0xD8u); ctx.fpr[1] = float_at(rt, G[17] + 0xDCu); ctx.fpr[2] = 0.0f; G[2] = word(rt, frame.at(0x2004u)); ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[1]; G[4] = G[19]; put(rt, G[17] + 0xE0u, G[2]); const bool below = ctx.fpr[0] < ctx.fpr[2]; put_float(rt, G[17] + 0xD8u, ctx.fpr[0]); if (below) put_float(rt, G[17] + 0xDCu, ctx.fpr[2]); call_here(rt, ctx, 0x08B3457Cu); } G[20] = word(rt, frame.at(0x2020u)); G[19] = word(rt, frame.at(0x201Cu)); G[18] = word(rt, frame.at(0x2018u)); G[17] = word(rt, frame.at(0x2014u)); G[16] = word(rt, frame.at(0x2010u)); } } }
void func_08AC2324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = G[5]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); call_here(rt, ctx, 0x08AC2230u); if (G[2] == 0u) goto out; G[16] = word(rt, G[17] + 0x10u); G[6] = word(rt, G[17] + 0x24u); if (G[16] == 0u) { G[3] = word(rt, G[17] + 0xCu); G[2] = load16(G[3] + 4u); G[16] = G[3] + G[2]; } G[2] = 0x454F0000u; G[19] = 0x454F434Du; G[6] += G[18]; put(rt, G[17] + 0x24u, G[6]); goto check; body: G[2] = load16(G[16] + 4u); G[3] = word(rt, G[17] + 0x28u); G[2] <<= 16; G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[3] != 0u) { G[5] = G[16]; G[4] = frame.at(0u); if (G[2] != 0u) goto store; G[2] = word(rt, G[17] + 8u); if (G[2] != 0u) { call_here(rt, ctx, 0x08AC26A4u); G[4] = word(rt, G[17] + 8u); put(rt, G[17] + 0x14u, frame.at(0u)); G[6] = frame.at(0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x1Cu); G[5] = G[17]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x14u, 0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08AC26B4u); G[6] = word(rt, G[17] + 0x24u); } } if (G[18] == G[6]) { put(rt, G[17] + 0x10u, G[16]); G[2] = load8(G[16] + 6u); G[16] += G[2]; } else { G[3] = word(rt, G[17] + 0xCu); G[2] = load16(G[3] + 4u); G[16] = G[3] + G[2]; } check: { G[2] = word(rt, G[16]); const bool more = G[2] != G[19]; G[18] = G[6]; if (more) goto body; } store: put(rt, G[17] + 0x28u, G[6]); out: G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_249
} // namespace gt::game

#define HOT249(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_249::name, gt::ReturnKind::kind)
// HOT249(0x08C0C414u, func_08C0C414, Word);  // no comparable synthetic case yet
HOT249(0x08C6F250u, func_08C6F250, Word);
// HOT249(0x08C79B94u, func_08C79B94, Word);  // no comparable synthetic case yet
HOT249(0x08C6F354u, func_08C6F354, Word);
// HOT249(0x08A6DB54u, func_08A6DB54, Word);  // no comparable synthetic case yet
// HOT249(0x0881F9E4u, func_0881F9E4, Word);  // no comparable synthetic case yet
HOT249(0x0882A84Cu, func_0882A84C, Word);
HOT249(0x088B5414u, func_088B5414, Word);
HOT249(0x089A90B4u, func_089A90B4, Word);
// HOT249(0x089D04F4u, func_089D04F4, Word);  // no comparable synthetic case yet
HOT249(0x08A123E8u, func_08A123E8, Word);
// HOT249(0x08A9DEE4u, func_08A9DEE4, Word);  // no comparable synthetic case yet
HOT249(0x08AA4A54u, func_08AA4A54, Word);
HOT249(0x08AA4B5Cu, func_08AA4B5C, Word);
HOT249(0x08B51400u, func_08B51400, Float);
HOT249(0x08AEC6F8u, func_08AEC6F8, Word);
HOT249(0x08B44420u, func_08B44420, Word);
HOT249(0x08ACE5CCu, func_08ACE5CC, Word);
HOT249(0x08AC2324u, func_08AC2324, Word);
