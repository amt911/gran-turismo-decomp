// Hot batch 244: sixty-two-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (handler registrations, polling waits, vector negations and static initialisers).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_244 {
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
    if (!(value >= -2447483648.0f && value < 2447483648.0f)) return 0x7FFFFFFF;
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

void func_088061D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[16]); G[16] = word(rt, G[4]); G[4] = 0x08C97DCCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x64u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = 0x08C97DF0u; G[4] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B86E98u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); const std::uint32_t names[3] = {0x08C97DFCu, 0x08C97E08u, 0x08C97E10u}; const std::uint32_t handlers[3] = {0x08806408u, 0x08806558u, 0x08806628u}; for (std::uint32_t i = 0u; i < 3u; ++i) { G[16] = word(rt, G[17]); G[4] = 0x08C97DCCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = names[i]; G[6] = handlers[i]; G[4] = G[16]; call_here(rt, ctx, 0x08B87108u); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0885FD54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); G[2] = 1u; G[5] = 0u; put(rt, frame.at(0xCu), G[19]); G[19] = G[7]; put(rt, frame.at(8u), G[18]); G[18] = G[6]; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(4u), G[17]); G[3] = word(rt, G[4]); G[17] = word(rt, G[3] + 0x5ACu); store8(G[3] + 0x3FDBu, 1u); G[4] = G[17]; G[20] = G[17] + 4u; call_here(rt, ctx, 0x088D7158u); for (;;) { G[4] = G[16]; call_here(rt, ctx, 0x0885EBDCu); G[4] = 0x3E80u; if (G[2] == 0u) break; call_here(rt, ctx, 0x08B2E2B8u); } G[4] = G[17]; call_here(rt, ctx, 0x088D6E9Cu); for (;;) { G[4] = G[16]; call_here(rt, ctx, 0x0885E9B8u); G[4] = 0x3E80u; if (G[2] == 0u) break; call_here(rt, ctx, 0x08B2E2B8u); } call_here(rt, ctx, 0x08B1C47Cu); G[4] = G[20]; G[6] = G[18]; G[7] = G[19]; G[8] = 0u; call_here(rt, ctx, 0x088D5ED0u); G[4] = G[17]; call_here(rt, ctx, 0x088DE9CCu); G[4] = G[17]; if (G[2] != 0u) { G[5] = G[20]; call_here(rt, ctx, 0x088D9FB4u); G[4] = G[17]; } G[5] = 1u; call_here(rt, ctx, 0x088D8154u); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); G[2] = 1u; } } }
void func_08876F98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); G[16] = frame.at(0x10u); put(rt, frame.at(0x28u), G[18]); G[18] = frame.at(4u); put(rt, frame.at(0x24u), G[17]); G[17] = G[4]; put(rt, frame.at(0x2Cu), G[31]); G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[6] = G[16]; G[4] = G[17]; G[5] = G[18]; G[7] = 3u; call_here(rt, ctx, 0x088755E4u); G[2] = word(rt, frame.at(0x18u)); G[2] = static_cast<std::int32_t>(G[2]) < 3 ? 1u : 0u; G[4] = word(rt, frame.at(0x10u)); if (G[2] == 0u) { G[16] = 0x08CA0000u; G[5] = 0x08CA0070u; G[4] = G[17]; G[6] = 0x08CA0074u; call_here(rt, ctx, 0x088756F0u); G[4] = G[17]; if (G[2] != 0u) { G[5] = 0x08CA0074u; call_here(rt, ctx, 0x088757B0u); ctx.fpr[2] = float_at(rt, frame.at(4u)); if (G[2] != 0u) { ctx.fpr[1] = float_at(rt, frame.at(8u)); ctx.fpr[0] = float_at(rt, frame.at(0xCu)); ctx.fpr[2] = -ctx.fpr[2]; ctx.fpr[1] = -ctx.fpr[1]; ctx.fpr[0] = -ctx.fpr[0]; G[4] = G[2]; G[5] = G[18]; put_float(rt, frame.at(4u), ctx.fpr[2]); put_float(rt, frame.at(8u), ctx.fpr[1]); put_float(rt, frame.at(0xCu), ctx.fpr[0]); call_here(rt, ctx, 0x088723B8u); } } G[4] = word(rt, frame.at(0x10u)); } G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_088C7C40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); G[2] = 1u; put(rt, frame.at(4u), G[31]); put(rt, frame.at(0u), G[16]); if (G[4] == G[2]) { G[2] = 0xFFFFu; if (G[5] == G[2]) { G[16] = 0x08CE0000u; G[2] = 0x08CE30D4u; G[3] = 0xFFFFFFFFu; G[4] = 0x08CE30F0u; put(rt, G[2] + 0x10u, G[3]); put(rt, G[2] + 0x18u, 0u); put(rt, G[2] + 4u, 0u); store8(G[2] + 8u, 0u); put(rt, G[2] + 0xCu, G[3]); put(rt, G[2] + 0x14u, 0u); put(rt, 0x08CE30D4u, 0u); call_here(rt, ctx, 0x08B28DCCu); G[4] = 3u; G[5] = 0xFFFFFFFFu; G[3] = 2u; G[2] = 4u; for (;;) { G[2] -= 1u; while (G[2] != 0u) G[2] -= 1u; G[3] -= 1u; { const bool again = G[3] != 0u; G[2] = 4u; if (again) continue; } G[4] -= 1u; { const bool again = G[4] != G[5]; G[3] = 2u; if (again) continue; } break; } G[2] = 0x8Bu; G[3] = 0xFFFFFFFFu; G[2] -= 1u; for (;;) { const bool again = G[2] != G[3]; G[2] -= 1u; if (!again) break; } G[2] = 0x28u; G[2] -= 1u; for (;;) { const bool again = G[2] != 0u; G[2] -= 1u; if (!again) break; } G[2] = 0x08CE30D4u; G[3] = 1u; put(rt, G[2] + 0xAC8u, G[3]); put(rt, G[2] + 0xACCu, 0u); put(rt, G[2] + 0xAC4u, G[3]); } } else { G[2] = 0xFFFFu; if (G[4] == 0u && G[5] == G[2]) { G[4] = 0x08CE30F0u; G[16] = word(rt, frame.at(0u)); goto tail; } } G[16] = word(rt, frame.at(0u)); } return; tail: jump(ctx, 0x08B28DC4u); } }
void func_088DE8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[18]); G[3] = G[5] << 5; G[2] = G[5] << 7; put(rt, frame.at(4u), G[17]); G[18] = G[5]; G[2] -= G[3]; put(rt, frame.at(0u), G[16]); G[2] -= G[18]; G[2] <<= 3; put(rt, frame.at(0xCu), G[31]); G[5] = word(rt, G[4] + 0xC0u); G[17] = G[5] + G[2]; G[3] = word(rt, G[17] + 4u); G[2] = 3u; G[16] = G[4]; if (G[3] == G[2]) { G[2] = 0u; goto done; } G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x088DE24Cu); { const bool none = G[2] == 0u; G[2] = 0u; if (none) goto done; } G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x088DB3CCu); G[4] = word(rt, G[16] + 0x640u); if (G[4] != 0u) { G[2] = word(rt, G[17] + 0x1Cu); if (G[2] == 0u) { G[5] = G[18]; call_here(rt, ctx, 0x088E46BCu); } } G[2] = load8(G[17] + 0x2B4u); G[4] = G[16]; if (G[2] != 0u) { G[2] = 3u; put(rt, G[17] + 0x2C8u, G[2]); G[6] = word(rt, G[16] + 0x90u); G[7] = word(rt, G[16] + 0x94u); call_here(rt, ctx, 0x088D9C24u); put(rt, G[17] + 0x2D0u, G[2]); put(rt, G[17] + 0x2D4u, G[3]); store8(G[17] + 0x2B4u, 0u); G[4] = G[16]; } G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x088D9BBCu); G[4] = word(rt, G[16] + 0x14Cu); G[2] = 1u; if (G[4] != 0u) { G[5] = G[18]; call_here(rt, ctx, 0x088F8A20u); G[2] = 1u; } done: G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_0897E594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 1u; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); if (G[4] == G[2]) { G[4] = 0x08CF4100u; G[2] = 0xFFFFu; G[16] = 0x08CA0000u; G[17] = 0x08CF0000u; if (G[5] == G[2]) { call_here(rt, ctx, 0x08B28DCCu); G[4] = 0x08CF40D4u; G[7] = 0x0897E298u; G[8] = 0x0897F128u; G[9] = 0x0897E2BCu; G[5] = 0x08CA33B8u; G[6] = 0x240u; call_here(rt, ctx, 0x08A92114u); G[6] = 0x0897FCCCu; G[7] = 0x0897FC88u; G[5] = 0x08CA33B8u; G[4] = 0x08CF40C4u; call_here(rt, ctx, 0x0897D534u); G[3] = 0x08C90000u; G[2] = 0x08C8BF18u; G[9] = word(rt, G[2] + 4u); G[8] = word(rt, 0x08C8BF18u); G[4] = 0x08CF40F0u; G[5] = 0x08CA33C8u; G[7] = 0x08CF40C4u; G[6] = frame.at(0u); put(rt, frame.at(0u), G[8]); put(rt, frame.at(4u), G[9]); call_here(rt, ctx, 0x08BEEA60u); } } else { const bool zero = G[4] == 0u; G[4] = 0x08CF0000u; if (zero) { G[2] = 0xFFFFu; G[4] = 0x08CF4100u; if (G[5] == G[2]) { G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto tail; } } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } return; tail: jump(ctx, 0x08B28DC4u); } }
void func_089ADB38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089AB370u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA50C4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16] + 0x14u); G[4] = frame.at(0u); G[5] = load8(G[2] + 0x200u); G[5] = G[5] != 0u ? 1u : 0u; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[18] = frame.at(4u); if (static_cast<std::int32_t>(G[6]) > 0) { G[4] = G[18]; call_here(rt, ctx, 0x089AB370u); G[17] = word(rt, frame.at(4u)); if (G[17] == 0u) { G[4] = 0x08CA50C4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); G[17] = word(rt, G[17] + 0x14u); if (G[16] == 0u) { G[4] = 0x08CA50BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[2] = G[2] != 0u ? 1u : 0u; store8(G[17] + 0x200u, G[2]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_089ADD40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089AB370u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA50C4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x089775D8u); ctx.fpr[12] = ctx.fpr[0]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[18] = frame.at(4u); if (static_cast<std::int32_t>(G[6]) > 0) { G[4] = G[18]; call_here(rt, ctx, 0x089AB370u); G[17] = word(rt, frame.at(4u)); if (G[17] == 0u) { G[4] = 0x08CA50C4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); G[17] = word(rt, G[17] + 0x14u); if (G[16] == 0u) { G[4] = 0x08CA50BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[12] = ctx.fpr[0]; G[4] = G[17]; call_here(rt, ctx, 0x08977610u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// 0x08A7111C (a0 = the object, a1 = the destination buffer, a2 = a count): 0x08A71064(object, destination, count) is tried first and a nonzero
// answer returns 0. Otherwise the object's buffer at +0x18 is copied backwards into the destination one byte at a time, at most count bytes; when
// the buffer empties first, the object's next node at +0x14 (0x08A5BB7C(0x08CB09F4) when null) runs its virtual +0x110(node, destination +
// written, count - written) and the total written is returned.
void func_08A7111C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], destination = ctx.gpr[5], count = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {31u, 0x14u}, {20u, 0x10u}, {16u, 0x0u}});
    ctx.gpr[19] = destination;
    ctx.gpr[18] = object;
    ctx.gpr[17] = count;
    if (call_set(rt, ctx, 0x08A71064u, {object, destination, count}) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (count == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 buffer = object + 0x18u;
    u32 written = 0u;
    while (word(rt, buffer + 0x4u) != word(rt, buffer)) {
        const u32 cursor = word(rt, buffer + 0x4u);
        const u32 byte = load8(cursor - 0x1u);
        put(rt, buffer + 0x4u, cursor - 0x1u);
        store8(destination + written, byte);
        ++written;
        if (count == written) {
            ctx.gpr[2] = written;
            return;
        }
    }
    const u32 node = word(rt, object + 0x14u);
    u32 vtable;
    if (node != 0u) {
        vtable = word(rt, node);
    } else {
        call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB09F4u});
        vtable = word(rt, node);
    }
    const u32 result = call_set(rt, ctx, word(rt, vtable + 0x110u), {node, destination + written, count - written});
    ctx.gpr[2] = written + result;
}
void func_08A986F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); G[23] = 0x08CB0000u; put(rt, frame.at(0x18u), G[22]); G[22] = G[4]; put(rt, frame.at(0x14u), G[21]); G[21] = G[9]; put(rt, frame.at(0x10u), G[20]); G[20] = G[8]; put(rt, frame.at(0xCu), G[19]); G[19] = G[7]; put(rt, frame.at(8u), G[18]); G[18] = G[6]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[17] = 0xFFFFFFFFu; while (G[21] != G[20]) { G[3] = G[21] - G[20]; G[2] = G[3] >> 31; G[2] += G[3]; G[2] = sra(G[2], 1); G[3] = word(rt, G[22] + 0x14u); G[17] = G[20] + G[2]; G[4] = G[17] << 2; G[3] += G[4]; G[16] = word(rt, G[3]); G[4] = 0x08CB2328u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[19] < G[3] ? 1u : 0u; if (G[2] == G[18] && G[3] == G[19]) { G[2] = G[17]; goto done; } if (G[4] != 0u) { G[21] = G[17]; G[17] = 0xFFFFFFFFu; continue; } G[4] = G[18] < G[2] ? 1u : 0u; if (G[19] == G[3] && G[4] != 0u) { G[21] = G[17]; G[17] = 0xFFFFFFFFu; continue; } G[20] = G[17] + 1u; G[17] = 0xFFFFFFFFu; } G[2] = G[17]; done: G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08AA17F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 1u; put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[16]); if (G[5] == G[2]) { G[16] = word(rt, G[6]); G[4] = 0x08CB28D0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[5] = word(rt, frame.at(8u)); G[4] = G[17]; G[6] = frame.at(2u); call_here(rt, ctx, 0x08B85EB8u); G[7] = 0x08C8FBDCu; G[4] = 0x08CB2A14u; G[5] = 0x08CB2A78u; G[8] = G[17]; G[6] = 0xA3u; call_here(rt, ctx, 0x08A5CEB8u); G[3] = word(rt, frame.at(4u)); G[17] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[17]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, frame.at(8u)); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[17] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08AE9630(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[3] = G[4] < 0x20u ? 1u : 0u; G[2] = G[5] < 3u ? 1u : 0u; G[3] ^= 1u; G[2] ^= 1u; G[14] = G[5]; G[13] = G[7]; { GuestFrame frame(ctx, 0x10u); G[24] = G[4]; G[8] = G[4]; G[11] = G[7]; G[4] = G[6]; G[3] |= G[2]; G[10] = G[6]; put(rt, frame.at(0u), G[31]); G[5] = 0x08C91AB4u; G[9] = G[14]; G[6] = 0xB2u; G[7] = 0x08C91ABCu; G[12] = static_cast<std::int32_t>(G[13]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; if (G[3] != 0u) goto report; G[2] = 0x08D40000u; G[25] = 0x08D40A38u; G[3] = G[14] << 4; G[2] = G[3] + G[25]; if (G[12] != 0u) goto report; G[3] = word(rt, G[2] + 0xCu); G[12] = word(rt, G[2] + 0x10u); G[15] = G[3] + G[4]; G[4] = static_cast<std::int32_t>(G[15]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[2] = static_cast<std::int32_t>(G[12]) < static_cast<std::int32_t>(G[15]) ? 1u : 0u; G[13] = G[3] + G[13]; G[3] = static_cast<std::int32_t>(G[13]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[12] = static_cast<std::int32_t>(G[12]) < static_cast<std::int32_t>(G[13]) ? 1u : 0u; G[4] |= G[2]; G[3] |= G[12]; if (G[4] != 0u) goto report; G[2] = G[13] - G[15]; G[4] = G[14] << 5; G[12] = G[2] + 1u; if (G[3] != 0u) goto report; G[2] = G[4] + G[24]; G[2] <<= 4; G[2] += G[25]; put(rt, G[2] + 0x98u, G[15]); put(rt, G[2] + 0x90u, G[13]); put(rt, G[2] + 0x94u, G[12]); put(rt, G[2] + 0x8Cu, G[15]); goto done; report: G[3] = 0x08CE0000u; G[2] = word(rt, 0x08CDFB3Cu) & 1u; if (G[2] != 0u) { G[2] = 0x08CE0000u; G[4] = word(rt, 0x08CDFB40u); call_here(rt, ctx, 0x08AD83B8u); } done:; } } }
// Stream input of one byte: clears the object's +4 "read" flag, runs the sentry, then -- when the sentry's
// byte is set -- takes the next character either from the stream's read buffer at +0x78 (advancing its
// pointer, answering 0xFFFFFFFF at the end) or through its virtual reader at vtable + 0x28, storing a
// non-EOF character at the destination and setting +4. Finally the stream state word at the base + 0x14 is
// OR-ed with 4 when nothing was read, and with 2/4 as the read ended, and handed to 0x08C54CFC; returns this.
void func_08C57324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {31u, 0x18u}});
    ctx.gpr[16] = object;
    ctx.gpr[17] = destination;
    put(rt, object + 0x4u, 0u);
    call_set(rt, ctx, 0x08C55FDCu, {frame.at(0u), object, 1u});
    u32 state = 0u;
    if (load8(frame.at(0u)) != 0u) {
        const u32 buffer = word(rt, object + word(rt, word(rt, object) - 0xCu) + 0x78u);
        const u32 current = word(rt, buffer + 0x8u);
        const u32 limit = word(rt, buffer + 0xCu);
        u32 character = 0u;
        if (current < limit) {
            character = load8(current);
            put(rt, buffer + 0x8u, current + 1u);
        } else {
            const u32 reader = word(rt, word(rt, buffer) + 0x28u);
            character = call_set(rt, ctx, reader, {buffer});
        }
        if (character == 0xFFFFFFFFu) {
            state = 2u;
        } else {
            put(rt, object + 0x4u, 1u);
            store8(destination, character);
        }
    }
    if (word(rt, object + 0x4u) == 0u) state |= 4u;
    if (state != 0u) {
        const u32 base = object + word(rt, word(rt, object) - 0xCu);
        call_set(rt, ctx, 0x08C54CFCu, {base, word(rt, base + 0x14u) | state});
    }
    ctx.gpr[2] = object;
}
void func_08B15268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x90u); put(rt, frame.at(0x8Cu), G[31]); put(rt, frame.at(0x88u), G[18]); G[18] = G[4]; put(rt, frame.at(0x80u), G[16]); G[16] = frame.at(0x40u); put(rt, frame.at(0x84u), G[17]); G[17] = word(rt, G[6] + 0x50u); call_here(rt, ctx, 0x08B0CE58u); G[4] = G[17]; call_here(rt, ctx, 0x08B17A98u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08B1837Cu); G[8] = frame.at(0u); G[6] = frame.at(0x70u); do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[8], G[2]); G[16] += 0x10u; G[8] += 0x10u; put(rt, G[8] - 0xCu, G[3]); put(rt, G[8] - 8u, G[4]); put(rt, G[8] - 4u, G[5]); } while (G[16] != G[6]); G[2] = word(rt, G[16] + 8u); G[3] = word(rt, G[16]); G[4] = word(rt, G[16] + 4u); G[7] = frame.at(0u); put(rt, G[8] + 8u, G[2]); G[6] = G[18]; G[9] = frame.at(0x30u); put(rt, G[8], G[3]); put(rt, G[8] + 4u, G[4]); do { G[2] = word(rt, G[7]); G[3] = word(rt, G[7] + 4u); G[4] = word(rt, G[7] + 8u); G[5] = word(rt, G[7] + 0xCu); put(rt, G[6], G[2]); G[7] += 0x10u; G[6] += 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 8u, G[4]); put(rt, G[6] - 4u, G[5]); } while (G[7] != G[9]); G[2] = word(rt, G[7] + 8u); G[3] = word(rt, G[7]); G[5] = word(rt, G[7] + 4u); G[4] = G[17]; put(rt, G[6] + 8u, G[2]); put(rt, G[6], G[3]); put(rt, G[6] + 4u, G[5]); call_here(rt, ctx, 0x08B17AA8u); G[2] = G[18]; G[18] = word(rt, frame.at(0x88u)); G[17] = word(rt, frame.at(0x84u)); G[16] = word(rt, frame.at(0x80u)); } } }
// 0x08B4173C (a0 = the object, a3 = a record whose +8 word is a signed selector, t0 = a mode, t2 = a selector to match, t3 = a value for a3):
// modes other than 0xC return 0xC. Otherwise the object's table at +0x454 is walked: byte +0x400 + i and word +0x200 + 4i are read for i below
// word +0x480, and the first nonzero byte whose word equals the selector leads to a chain of two pointers whose +0x16 halfword clears the
// frame's byte +0xA when nonzero. 0x08B3F098(object + 0x8DC) then runs and its answer, when nonzero, goes to 0x08B40950(object, answer, 0x2715,
// frame, 0xC, 1).
void func_08B4173C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], record = ctx.gpr[7], mode = ctx.gpr[8], selector = ctx.gpr[10], replacement = ctx.gpr[11];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x14u}});
    ctx.gpr[16] = object;
    ctx.gpr[7] = replacement;
    if (mode != 0xCu) {
        ctx.gpr[2] = 0xCu;
        return;
    }
    const u32 table = object + 0x454u;
    const u32 key = sext(load16(record + 0x8u), 16);
    const u32 count = word(rt, table + 0x480u);
    put(rt, frame.at(0x8u), 0u);
    store8(frame.at(0xAu), 1u);
    put(rt, frame.at(0x0u), 0u);
    put(rt, frame.at(0x4u), 0u);
    store16(frame.at(0x8u), key);
    for (u32 index = 0; index != count; ++index) {
        if (load8(table + 0x400u + index) == 0u) continue;
        if (word(rt, table + 0x200u + index * 4u) != selector) continue;
        const u32 slot = word(rt, table + index * 4u);
        if (slot == 0u) break;
        const u32 entry = word(rt, slot);
        if (entry == 0u) break;
        if (sext(load16(entry + 0x16u), 16) != 0u) store8(frame.at(0xAu), 0u);
        break;
    }
    const u32 found = call_set(rt, ctx, 0x08B3F098u, {object + 0x8DCu});
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08B40950u, {object, found, 0x2715u, frame.at(0x0u), 0xCu, 0x1u});
}
void func_08AFC360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); G[30] = G[6]; put(rt, frame.at(0x28u), G[22]); G[22] = 0u; put(rt, frame.at(0x20u), G[20]); G[20] = G[5]; put(rt, frame.at(0x1Cu), G[19]); G[19] = 0u; put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0u), G[4]); if (static_cast<std::int32_t>(G[5]) > 0) { do { G[21] = G[22] + 1u; G[2] = static_cast<std::int32_t>(G[21]) < static_cast<std::int32_t>(G[20]) ? 1u : 0u; G[17] = G[21]; if (G[2] != 0u) { G[23] = G[20]; G[3] = word(rt, frame.at(0u)); do { G[2] = sra(G[19], 3); G[7] = G[17]; G[16] = G[3] + G[2]; G[3] = G[19] & 7u; G[2] = 1u; G[4] = load8(G[16]); G[3] = G[2] << G[3]; G[18] = sext(G[3], 8); G[2] = ~G[18]; G[2] &= G[4]; store8(G[16], G[2]); G[4] = G[20]; G[5] = G[30]; G[6] = G[22]; call_here(rt, ctx, 0x08AFC230u); G[17] += 1u; G[19] += 1u; if (G[2] == 0u) { G[2] = load8(G[16]); G[2] = G[18] | G[2]; store8(G[16], G[2]); } G[3] = word(rt, frame.at(0u)); } while (G[23] != G[17]); G[22] = G[21]; continue; } G[22] = G[21]; } while (G[20] != G[21]); } G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08BDA784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[3] = G[5] - 1u; { GuestFrame frame(ctx, 0x40u); G[2] = G[3] >> 31; G[2] += G[3]; put(rt, frame.at(0x1Cu), G[19]); G[3] = G[5] << 3; G[19] = G[4]; G[4] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); G[22] = G[6]; put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); G[18] = G[19] + G[3]; put(rt, frame.at(0x14u), G[17]); G[17] = sra(G[2], 1); put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x20u), G[20]); store8(frame.at(0u), G[8]); G[21] = word(rt, G[7]); G[23] = word(rt, G[7] + 4u); if (G[4] != 0u) { for (;;) { G[2] = G[17] << 3; G[18] = G[19] + G[2]; G[20] = word(rt, G[18]); G[5] = G[21]; G[4] = G[20]; call_here(rt, ctx, 0x08B35FD8u); G[4] = G[17] - 1u; G[3] = G[4] >> 31; G[5] = G[16] << 3; G[3] += G[4]; G[5] += G[19]; G[4] = sra(G[3], 1); if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = G[16] << 3; G[18] = G[19] + G[2]; break; } G[2] = word(rt, G[18] + 4u); G[3] = static_cast<std::int32_t>(G[22]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; G[16] = G[17]; put(rt, G[5] + 4u, G[2]); G[17] = G[4]; put(rt, G[5], G[20]); if (G[3] == 0u) break; } } put(rt, G[18], G[21]); put(rt, G[18] + 4u, G[23]); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_244
} // namespace gt::game

#define HOT244(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_244::name, gt::ReturnKind::kind)
HOT244(0x088061D8u, func_088061D8, Word);
// HOT244(0x0885FD54u, func_0885FD54, Word);  // no comparable synthetic case yet
HOT244(0x08876F98u, func_08876F98, Word);
HOT244(0x088C7C40u, func_088C7C40, Word);
HOT244(0x088DE8D4u, func_088DE8D4, Word);
HOT244(0x0897E594u, func_0897E594, Word);
HOT244(0x089ADB38u, func_089ADB38, Word);
HOT244(0x089ADD40u, func_089ADD40, Word);
HOT244(0x08A7111Cu, func_08A7111C, Word);
HOT244(0x08A986F4u, func_08A986F4, Word);
HOT244(0x08AA17F8u, func_08AA17F8, Word);
HOT244(0x08AE9630u, func_08AE9630, Word);
HOT244(0x08C57324u, func_08C57324, Word);
// HOT244(0x08B15268u, func_08B15268, Word);  // no comparable synthetic case yet
HOT244(0x08B4173Cu, func_08B4173C, Word);
HOT244(0x08AFC360u, func_08AFC360, Word);
HOT244(0x08BDA784u, func_08BDA784, Word);
