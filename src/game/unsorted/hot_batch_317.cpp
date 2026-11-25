// Hot batch 317: functions no route reaches (straight-line code, forward branches and simple loops), drafted from
// `tools.disasm` with a register-level emitter (pipeline/bodyx.py), read against the asm and verified by
// synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_317 {
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

void func_08BDAA80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[22] = G[5] - G[4]; G[2] = static_cast<std::int32_t>(G[22]) < 16 ? 1u : 0u; G[30] = G[6]; G[23] = G[5]; G[19] = G[4]; { const bool tk = G[2] != 0u; store8(frame.at(0x10u), G[7]); if (!tk) { G[20] = sra(G[22], 3); G[2] = G[20] + 0xFFFFFFFEu; G[3] = G[2] >> 31; G[3] = G[3] + G[2]; G[17] = sra(G[3], 1); G[2] = G[17] << 3; G[16] = G[2] + G[4]; G[18] = 0u; G[21] = frame.at(0x8u); while (true) { G[3] = word(rt, G[16] + 0x4u); G[2] = word(rt, G[16] + 0x0u); G[8] = G[16]; G[5] = G[17] - G[18]; G[4] = G[19]; G[6] = G[20]; G[7] = G[21]; put(rt, frame.at(0x8u), G[2]); G[16] = G[16] + 0xFFFFFFF8u; put(rt, frame.at(0xCu), G[3]); call_here(rt, ctx, 0x08BDA87Cu); { const bool tk = G[17] != G[18]; G[18] = G[18] + 0x1u; if (!tk) break; } } } } G[2] = G[23] < G[30] ? 1u : 0u; { const bool tk = G[2] == 0u; G[4] = G[19]; if (!tk) { G[20] = sra(G[22], 3); G[16] = G[23]; G[18] = word(rt, G[16] + 0x0u); while (true) { G[17] = word(rt, G[19] + 0x0u); G[4] = G[18]; G[5] = G[17]; call_here(rt, ctx, 0x08B35FD8u); G[4] = G[19]; G[5] = 0u; G[6] = G[20]; G[7] = frame.at(0u); { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[8] = G[16]; if (!tk) { G[2] = word(rt, G[19] + 0x4u); G[3] = word(rt, G[16] + 0x4u); put(rt, G[16] + 0x0u, G[17]); put(rt, G[16] + 0x4u, G[2]); put(rt, frame.at(0x4u), G[3]); put(rt, frame.at(0x0u), G[18]); call_here(rt, ctx, 0x08BDA87Cu); } } G[16] = G[16] + 0x8u; G[2] = G[16] < G[30] ? 1u : 0u; { const bool tk = G[2] != 0u; if (!tk) break; G[18] = word(rt, G[16] + 0x0u); } } G[4] = G[19]; } } G[5] = G[23]; G[6] = G[16]; call_here(rt, ctx, 0x08BDA9C8u);  G[22] = word(rt, frame.at(0x38u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[19] = word(rt, frame.at(0x2Cu)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_0883E3E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x80u); put(rt, frame.at(0x6Cu), G[23]); put(rt, frame.at(0x60u), G[20]); put(rt, frame.at(0x58u), G[18]); put(rt, frame.at(0x74u), G[31]); put(rt, frame.at(0x70u), G[30]); put(rt, frame.at(0x68u), G[22]); put(rt, frame.at(0x64u), G[21]); put(rt, frame.at(0x5Cu), G[19]); put(rt, frame.at(0x54u), G[17]); put(rt, frame.at(0x50u), G[16]); G[23] = G[4]; G[20] = frame.at(0xCu); call_here(rt, ctx, 0x08931098u); G[18] = G[2]; G[4] = G[20]; G[5] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); { const bool tk = static_cast<std::int32_t>(G[18]) <= 0; G[17] = 0u; if (!tk) { G[21] = frame.at(0x10u); G[30] = 0x08CA0000u; G[22] = frame.at(0x4u); G[19] = frame.at(0x8u); G[4] = G[17]; while (true) { G[5] = G[21]; G[6] = 0x40u; call_here(rt, ctx, 0x089311B4u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[16] = word(rt, frame.at(0xCu)); if (!tk) { { const bool tk = G[16] != 0u; G[4] = 0x08C9C42Cu; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[6] = frame.at(0x1u); G[4] = G[22]; G[5] = G[21]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[19]; G[5] = G[22]; call_here(rt, ctx, 0x08A6D2D4u); G[2] = word(rt, G[16] + 0x14u); G[3] = G[17] << 2; G[2] = G[2] + G[3]; { const bool tk = G[2] == G[19]; G[4] = G[2]; if (!tk) { G[5] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x4u)); G[4] = G[2] + 0xFFFFFFFCu; G[16] = G[2] + 0xFFFFFFF4u; G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; G[5] = 0xFFFFFFFFu; if (!tk) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x08B8542Cu); } } } } } } G[17] = G[17] + 0x1u; { const bool tk = G[18] != G[17]; G[4] = G[17]; if (!tk) break; } } } } { const bool tk = G[23] == G[20]; G[5] = word(rt, frame.at(0xCu)); if (!tk) { G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu);  G[23] = word(rt, frame.at(0x6Cu)); G[20] = word(rt, frame.at(0x60u)); G[18] = word(rt, frame.at(0x58u)); G[30] = word(rt, frame.at(0x70u)); G[22] = word(rt, frame.at(0x68u)); G[21] = word(rt, frame.at(0x64u)); G[19] = word(rt, frame.at(0x5Cu)); G[17] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x50u)); }  } }
void func_08AC1934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[21] = G[5]; G[17] = G[6]; put(rt, G[5] + 0x0u, 0u); put(rt, G[5] + 0x4u, 0u); G[2] = load16(G[6] + 0x0u); G[3] = load16(G[6] + 0x2u); { const bool tk = static_cast<std::int32_t>(G[3]) <= 0; G[16] = G[2] + G[6]; if (!tk) { G[2] = 0x54520000u; G[22] = G[2] | 0x4754u; G[19] = 0u; G[18] = frame.at(0x8u); G[20] = frame.at(0xCu); G[23] = frame.at(0x4u); G[5] = G[16]; while (true) { G[4] = G[18]; call_here(rt, ctx, 0x08AC26A4u); G[4] = G[18]; call_here(rt, ctx, 0x08AC2690u); G[19] = G[19] + 0x1u; G[4] = G[20]; { const bool tk = G[2] != G[22]; G[5] = 0u; if (!tk) { call_here(rt, ctx, 0x08AC26D4u); G[5] = G[18]; G[6] = 0u; G[4] = G[23]; call_here(rt, ctx, 0x08AC2730u); G[2] = word(rt, frame.at(0x4u)); G[4] = G[23]; put(rt, frame.at(0xCu), G[2]); call_here(rt, ctx, 0x08AC26DCu); G[4] = G[20]; call_here(rt, ctx, 0x08AC2864u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[4] = G[20]; if (!tk) { call_here(rt, ctx, 0x08AC2900u); put(rt, G[21] + 0x0u, G[2]); } } G[5] = G[18]; G[6] = 0x1u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08AC2730u); G[2] = word(rt, frame.at(0x0u)); G[4] = frame.at(0u); put(rt, frame.at(0xCu), G[2]); call_here(rt, ctx, 0x08AC26DCu); G[4] = G[20]; call_here(rt, ctx, 0x08AC2864u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[4] = G[20]; if (!tk) { call_here(rt, ctx, 0x08AC2900u); put(rt, G[21] + 0x4u, G[2]); } } G[4] = G[20]; call_here(rt, ctx, 0x08AC26DCu); } } G[2] = load8(G[16] + 0x6u); G[4] = G[18]; G[16] = G[16] + G[2]; call_here(rt, ctx, 0x08AC26B4u); G[2] = load16(G[17] + 0x2u); G[2] = static_cast<std::int32_t>(G[19]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[5] = G[16]; if (!tk) break; } } } }  G[21] = word(rt, frame.at(0x24u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); }  } }
// One 0x1C-byte record of the table walks below (0x08BDE2F0, 0x08BDE18C ...): `record` takes the source's first word, then two spans.
// Each span is half the difference of two source words (arithmetic shift), allocated by 0x08B35DA0 when non-zero (0 otherwise) and
// filled by 0x08BDD8D8 (first) or 0x08BDD910 (second); the scratch words sp+1 / sp+0 are the calls' last argument.
void fill_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, u32 record, u32 source) {
    put(rt, record, word(rt, source));
    const u32 first_span = sra(word(rt, source + 0x8u) - word(rt, source + 0x4u), 1);
    put(rt, record + 0x8u, 0u);
    put(rt, record + 0x4u, 0u);
    put(rt, record + 0xCu, 0u);
    const u32 first = first_span != 0u ? call_set(rt, ctx, 0x08B35DA0u, {first_span << 1}) : 0u;
    put(rt, record + 0x8u, first);
    put(rt, record + 0x4u, first);
    const u32 first_fill = call_set(rt, ctx, 0x08BDD8D8u, {word(rt, source + 0x4u), word(rt, source + 0x8u), first, frame.at(0x1u)});
    put(rt, record + 0xCu, first_fill);
    put(rt, record + 0x8u, first_fill);
    const u32 second_span = sra(word(rt, source + 0x14u) - word(rt, source + 0x10u), 1);
    put(rt, record + 0x14u, 0u);
    put(rt, record + 0x10u, 0u);
    put(rt, record + 0x18u, 0u);
    const u32 second = second_span != 0u ? call_set(rt, ctx, 0x08B35DA0u, {second_span << 1}) : 0u;
    put(rt, record + 0x14u, second);
    put(rt, record + 0x10u, second);
    const u32 second_fill = call_set(rt, ctx, 0x08BDD910u, {word(rt, source + 0x10u), word(rt, source + 0x14u), second, frame.at(0x0u)});
    put(rt, record + 0x18u, second_fill);
    put(rt, record + 0x14u, second_fill);
}
// 0x08BDE70C (a0 = the source records, a1 = their end, a2 = the destination records; v0 = the destination after the walk): same walk as
// 0x08BDE18C with a 0x40-byte frame (source a0 to a1 in steps of 0x1C, destinations filled by fill_record). Equal bounds return a2 at once.
void func_08BDE70C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); G[30] = G[5]; G[18] = G[4]; G[17] = G[6]; { const bool tk = G[4] == G[5]; if (!tk) { while (true) { G[19] = G[18] + 0x4u; G[21] = G[18] + 0x10u; G[16] = G[17] + 0x4u; { const bool tk = G[17] == 0u; G[20] = G[17] + 0x10u; if (!tk) { G[2] = word(rt, G[18] + 0x0u); G[4] = 0u; G[22] = 0u; put(rt, G[17] + 0x0u, G[2]); G[23] = 0u; G[3] = word(rt, G[18] + 0x4u); G[2] = word(rt, G[19] + 0x4u); put(rt, G[17] + 0x4u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[16] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[16] + 0x8u, 0u); if (!tk) { G[22] = G[2] << 1; G[4] = G[22]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[16] + 0x4u, G[4]); G[2] = G[4] + G[22]; G[6] = G[4]; put(rt, G[16] + 0x0u, G[4]); G[7] = frame.at(0x1u); G[4] = word(rt, G[19] + 0x0u); G[5] = word(rt, G[19] + 0x4u); put(rt, G[16] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD8D8u); put(rt, G[16] + 0x4u, G[2]); G[4] = 0u; G[3] = word(rt, G[18] + 0x10u); G[2] = word(rt, G[21] + 0x4u); put(rt, G[17] + 0x10u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[20] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[20] + 0x8u, 0u); if (!tk) { G[23] = G[2] << 1; G[4] = G[23]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[20] + 0x4u, G[4]); G[2] = G[4] + G[23]; G[6] = G[4]; put(rt, G[20] + 0x0u, G[4]); G[7] = frame.at(0u); G[5] = word(rt, G[21] + 0x4u); G[4] = word(rt, G[21] + 0x0u); put(rt, G[20] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD910u); put(rt, G[20] + 0x4u, G[2]); } } G[18] = G[18] + 0x1Cu; { const bool tk = G[30] != G[18]; G[17] = G[17] + 0x1Cu; if (!tk) break; } } } } G[2] = G[17];  G[30] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x08ABED84 (a0 = the object; v0 = bit 15 of the last node's word +4, or the result of 0x08B3457C when that node was handled): walks the node list that 0x08ABF868(object + 0x144) starts, each node's successor
// (0x08ABF6F0(list, node)) read first. A node whose word +4 has bit 15 set is unlinked (0x08ABF7EC(list, node)) and put back in its pool, under the lock 0x08B344E0(object + 0x11C) ... 0x08B3457C(object + 0x11C): the pool is the
// 16-byte record at object + 0x1C + (byte +4 of the node) * 16 (word +4 = a free count, +8 = a counted array of 0x20-byte elements whose first word is a vtable, +0xC = a stack of free nodes). The node is pushed on
// the stack (count - 1, stored at stack + 4 * count); when the count reaches 0 the record is torn down: count cleared, the array's elements destroyed from the last down (vtable slot 0, 0x08B61FC0 frees the block at array - 4) and
// the stack freed (0x08B61FC0), both words cleared.
void func_08ABED84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {16u, 0x0u}, {17u, 0x4u}, {31u, 0x1Cu}, {22u, 0x18u}, {21u, 0x14u}, {19u, 0xCu}, {18u, 0x8u}});
    const u32 list = self + 0x144u;
    ctx.gpr[20] = list;
    ctx.gpr[16] = self;
    u32 node = call_set(rt, ctx, 0x08ABF868u, {list});
    ctx.gpr[17] = node;
    if (node == 0u) return;
    const u32 lock = self + 0x11Cu, pools = self + 0x1Cu;
    ctx.gpr[21] = lock;
    ctx.gpr[22] = pools;
    for (;;) {
        const u32 next = call_set(rt, ctx, 0x08ABF6F0u, {list, node});
        ctx.gpr[19] = next;
        const u32 flag = (word(rt, node + 0x4u) >> 15) & 0x1u;
        ctx.gpr[2] = flag;
        if (flag != 0u) {
            call_set(rt, ctx, 0x08ABF7ECu, {list, node});
            ctx.gpr[16] = load8(node + 0x4u);
            call_set(rt, ctx, 0x08B344E0u, {lock});
            const u32 pool = pools + (load8(node + 0x4u) << 4);
            ctx.gpr[16] = load8(node + 0x4u) << 4;
            ctx.gpr[18] = pool;
            const u32 free_count = word(rt, pool + 0x4u) - 1u;
            const u32 stack = word(rt, pool + 0xCu);
            put(rt, pool + 0x4u, free_count);
            put(rt, stack + (free_count << 2), node);
            if (word(rt, pool + 0x4u) == 0u) {
                put(rt, pool + 0x4u, 0u);
                if (word(rt, pool + 0xCu) != 0u) {
                    const u32 array = word(rt, pool + 0x8u);
                    if (array != 0u) {
                        const u32 end = array + (word(rt, array - 0x4u) << 5);
                        u32 element = end - 0x20u;
                        ctx.gpr[16] = end;
                        ctx.gpr[17] = element;
                        if (array != end) {
                            for (;;) {
                                const u32 destructor = word(rt, word(rt, element));
                                ctx.gpr[16] = element;
                                const u32 previous = element - 0x20u;
                                ctx.gpr[17] = previous;
                                call_set(rt, ctx, destructor, {element});
                                if (ctx.gpr[16] == word(rt, pool + 0x8u)) break;
                                element = previous;
                            }
                        }
                        call_set(rt, ctx, 0x08B61FC0u, {ctx.gpr[16] - 0x4u});
                        const u32 stack_block = word(rt, pool + 0xCu);
                        if (stack_block != 0u) call_set(rt, ctx, 0x08B61FC0u, {stack_block});
                        put(rt, pool + 0xCu, 0u);
                        put(rt, pool + 0x8u, 0u);
                    }
                }
            }
            call_set(rt, ctx, 0x08B3457Cu, {lock});
        }
        ctx.gpr[17] = next;
        node = next;
        if (next == 0u) break;
    }
}
// 0x08BDE2F0 (a0 = the destination records, a1 = the count, a2 = the source record; v0 = the count on return): for each of the
// count steps the destination record advances by 0x1C, and a non-null one is filled from the source (fill_record). A zero count returns at once.
void func_08BDE2F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x44u}, {30u, 0x40u}, {23u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    const u32 count = ctx.gpr[5];
    put(rt, frame.at(0x10u), count);
    if (count == 0u) return;
    u32 record = ctx.gpr[4];
    const u32 source = ctx.gpr[6];
    ctx.gpr[20] = source;
    u32 step = 0u;
    do {
        step += 1u;
        ctx.gpr[30] = step;
        if (record != 0u) fill_record(rt, ctx, frame, record, source);
        record += 0x1Cu;
    } while (word(rt, frame.at(0x10u)) != step);
    ctx.gpr[2] = count;
}
// 0x08BDE5B0 (a0 = the destination records, a1 = the count, a2 = the source record; v0 = the count on return): same table walk as
// 0x08BDE2F0 (fixed source record, count steps of 0x1C, non-null destinations filled by fill_record). A zero count returns at once.
void func_08BDE5B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x44u}, {30u, 0x40u}, {23u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    const u32 count = ctx.gpr[5];
    put(rt, frame.at(0x10u), count);
    if (count == 0u) return;
    u32 record = ctx.gpr[4];
    const u32 source = ctx.gpr[6];
    ctx.gpr[20] = source;
    u32 step = 0u;
    do {
        step += 1u;
        ctx.gpr[30] = step;
        if (record != 0u) fill_record(rt, ctx, frame, record, source);
        record += 0x1Cu;
    } while (word(rt, frame.at(0x10u)) != step);
    ctx.gpr[2] = count;
}
void func_0883FF50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); G[23] = G[4]; G[4] = frame.at(0xCu); G[21] = frame.at(0x10u); put(rt, frame.at(0xCu), 0u); call_here(rt, ctx, 0x0892C890u); G[4] = G[21]; G[5] = word(rt, frame.at(0xCu)); G[16] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0xCu)); { const bool tk = G[2] == 0u; G[18] = G[16]; if (!tk) { G[20] = frame.at(0x8u); G[19] = 0u; G[22] = frame.at(0x4u); G[30] = 0x08CA0000u; G[4] = word(rt, G[18] + 0x0u); while (true) { G[18] = G[18] + 0x8u; call_here(rt, ctx, 0x0892C880u); G[17] = word(rt, frame.at(0x10u)); G[16] = G[2]; { const bool tk = G[17] != 0u; G[4] = 0x08C9C42Cu; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[5] = G[16]; G[6] = frame.at(0x1u); G[4] = G[22]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[20]; G[5] = G[22]; call_here(rt, ctx, 0x08A6D2D4u); G[2] = word(rt, G[17] + 0x14u); G[3] = G[19] << 2; G[19] = G[19] + 0x1u; G[2] = G[2] + G[3]; { const bool tk = G[2] == G[20]; G[4] = G[2]; if (!tk) { G[5] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x4u)); G[4] = G[2] + 0xFFFFFFFCu; G[16] = G[2] + 0xFFFFFFF4u; G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; G[5] = 0xFFFFFFFFu; if (!tk) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x08B8542Cu); } } } } G[2] = word(rt, frame.at(0xCu)); G[2] = G[19] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; if (!tk) break; G[4] = word(rt, G[18] + 0x0u); } } } } { const bool tk = G[23] == G[21]; G[5] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu);  G[23] = word(rt, frame.at(0x3Cu)); G[21] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x20u)); G[30] = word(rt, frame.at(0x40u)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); }  } }
void func_08840390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); G[23] = G[4]; G[4] = frame.at(0xCu); G[21] = frame.at(0x10u); put(rt, frame.at(0xCu), 0u); call_here(rt, ctx, 0x0892C8C8u); G[4] = G[21]; G[5] = word(rt, frame.at(0xCu)); G[16] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0xCu)); { const bool tk = G[2] == 0u; G[18] = G[16]; if (!tk) { G[20] = frame.at(0x8u); G[19] = 0u; G[22] = frame.at(0x4u); G[30] = 0x08CA0000u; G[4] = word(rt, G[18] + 0x0u); while (true) { G[18] = G[18] + 0x8u; call_here(rt, ctx, 0x0892C8B8u); G[17] = word(rt, frame.at(0x10u)); G[16] = G[2]; { const bool tk = G[17] != 0u; G[4] = 0x08C9C42Cu; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[5] = G[16]; G[6] = frame.at(0x1u); G[4] = G[22]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[20]; G[5] = G[22]; call_here(rt, ctx, 0x08A6D2D4u); G[2] = word(rt, G[17] + 0x14u); G[3] = G[19] << 2; G[19] = G[19] + 0x1u; G[2] = G[2] + G[3]; { const bool tk = G[2] == G[20]; G[4] = G[2]; if (!tk) { G[5] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x4u)); G[4] = G[2] + 0xFFFFFFFCu; G[16] = G[2] + 0xFFFFFFF4u; G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; G[5] = 0xFFFFFFFFu; if (!tk) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x08B8542Cu); } } } } G[2] = word(rt, frame.at(0xCu)); G[2] = G[19] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; if (!tk) break; G[4] = word(rt, G[18] + 0x0u); } } } } { const bool tk = G[23] == G[21]; G[5] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu);  G[23] = word(rt, frame.at(0x3Cu)); G[21] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x20u)); G[30] = word(rt, frame.at(0x40u)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); }  } }
// 0x08BDE18C (a0 = the source records, a1 = their end, a2 = the destination records; v0 = the destination after the walk): walks the
// source from a0 to a1 in steps of 0x1C and fills the destination record of each step (fill_record; null destinations skipped). Equal
// bounds return a2 at once.
void func_08BDE18C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[21] = G[4]; G[17] = G[6]; { const bool tk = G[5] == G[4]; put(rt, frame.at(0x10u), G[5]); if (!tk) { G[18] = G[4]; while (true) { G[19] = G[18] + 0x4u; G[22] = G[18] + 0x10u; G[21] = G[21] + 0x1Cu; G[16] = G[17] + 0x4u; { const bool tk = G[17] == 0u; G[20] = G[17] + 0x10u; if (!tk) { G[2] = word(rt, G[18] + 0x0u); G[4] = 0u; G[23] = 0u; put(rt, G[17] + 0x0u, G[2]); G[30] = 0u; G[3] = word(rt, G[18] + 0x4u); G[2] = word(rt, G[19] + 0x4u); put(rt, G[17] + 0x4u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[16] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[16] + 0x8u, 0u); if (!tk) { G[23] = G[2] << 1; G[4] = G[23]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[16] + 0x4u, G[4]); G[2] = G[4] + G[23]; G[6] = G[4]; put(rt, G[16] + 0x0u, G[4]); G[7] = frame.at(0x1u); G[4] = word(rt, G[19] + 0x0u); G[5] = word(rt, G[19] + 0x4u); put(rt, G[16] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD8D8u); put(rt, G[16] + 0x4u, G[2]); G[4] = 0u; G[3] = word(rt, G[18] + 0x10u); G[2] = word(rt, G[22] + 0x4u); put(rt, G[17] + 0x10u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[20] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[20] + 0x8u, 0u); if (!tk) { G[30] = G[2] << 1; G[4] = G[30]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[20] + 0x4u, G[4]); G[2] = G[4] + G[30]; G[6] = G[4]; put(rt, G[20] + 0x0u, G[4]); G[7] = frame.at(0u); G[5] = word(rt, G[22] + 0x4u); G[4] = word(rt, G[22] + 0x0u); put(rt, G[20] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD910u); put(rt, G[20] + 0x4u, G[2]); } } G[2] = word(rt, frame.at(0x10u)); G[18] = G[21]; { const bool tk = G[21] != G[2]; G[17] = G[17] + 0x1Cu; if (!tk) break; } } } } G[2] = G[17];  G[21] = word(rt, frame.at(0x34u)); G[17] = word(rt, frame.at(0x24u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
// 0x08BDE44C (a0 = the source records, a1 = their end, a2 = the destination records; v0 = the destination after the walk): same walk as
// 0x08BDE18C (source a0 to a1 in steps of 0x1C, destinations filled by fill_record). Equal bounds return a2 at once.
void func_08BDE44C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[21] = G[4]; G[17] = G[6]; { const bool tk = G[5] == G[4]; put(rt, frame.at(0x10u), G[5]); if (!tk) { G[18] = G[4]; while (true) { G[19] = G[18] + 0x4u; G[22] = G[18] + 0x10u; G[21] = G[21] + 0x1Cu; G[16] = G[17] + 0x4u; { const bool tk = G[17] == 0u; G[20] = G[17] + 0x10u; if (!tk) { G[2] = word(rt, G[18] + 0x0u); G[4] = 0u; G[23] = 0u; put(rt, G[17] + 0x0u, G[2]); G[30] = 0u; G[3] = word(rt, G[18] + 0x4u); G[2] = word(rt, G[19] + 0x4u); put(rt, G[17] + 0x4u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[16] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[16] + 0x8u, 0u); if (!tk) { G[23] = G[2] << 1; G[4] = G[23]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[16] + 0x4u, G[4]); G[2] = G[4] + G[23]; G[6] = G[4]; put(rt, G[16] + 0x0u, G[4]); G[7] = frame.at(0x1u); G[4] = word(rt, G[19] + 0x0u); G[5] = word(rt, G[19] + 0x4u); put(rt, G[16] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD8D8u); put(rt, G[16] + 0x4u, G[2]); G[4] = 0u; G[3] = word(rt, G[18] + 0x10u); G[2] = word(rt, G[22] + 0x4u); put(rt, G[17] + 0x10u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[20] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[20] + 0x8u, 0u); if (!tk) { G[30] = G[2] << 1; G[4] = G[30]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[20] + 0x4u, G[4]); G[2] = G[4] + G[30]; G[6] = G[4]; put(rt, G[20] + 0x0u, G[4]); G[7] = frame.at(0u); G[5] = word(rt, G[22] + 0x4u); G[4] = word(rt, G[22] + 0x0u); put(rt, G[20] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD910u); put(rt, G[20] + 0x4u, G[2]); } } G[2] = word(rt, frame.at(0x10u)); G[18] = G[21]; { const bool tk = G[21] != G[2]; G[17] = G[17] + 0x1Cu; if (!tk) break; } } } } G[2] = G[17];  G[21] = word(rt, frame.at(0x34u)); G[17] = word(rt, frame.at(0x24u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
// 0x08BDEAD8 (a0 = the source records, a1 = their end, a2 = the destination records; v0 = the destination after the walk): same walk as
// 0x08BDE18C (source a0 to a1 in steps of 0x1C, destinations filled by fill_record). Equal bounds return a2 at once.
void func_08BDEAD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[21] = G[4]; G[17] = G[6]; { const bool tk = G[5] == G[4]; put(rt, frame.at(0x10u), G[5]); if (!tk) { G[18] = G[4]; while (true) { G[19] = G[18] + 0x4u; G[22] = G[18] + 0x10u; G[21] = G[21] + 0x1Cu; G[16] = G[17] + 0x4u; { const bool tk = G[17] == 0u; G[20] = G[17] + 0x10u; if (!tk) { G[2] = word(rt, G[18] + 0x0u); G[4] = 0u; G[23] = 0u; put(rt, G[17] + 0x0u, G[2]); G[30] = 0u; G[3] = word(rt, G[18] + 0x4u); G[2] = word(rt, G[19] + 0x4u); put(rt, G[17] + 0x4u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[16] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[16] + 0x8u, 0u); if (!tk) { G[23] = G[2] << 1; G[4] = G[23]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[16] + 0x4u, G[4]); G[2] = G[4] + G[23]; G[6] = G[4]; put(rt, G[16] + 0x0u, G[4]); G[7] = frame.at(0x1u); G[4] = word(rt, G[19] + 0x0u); G[5] = word(rt, G[19] + 0x4u); put(rt, G[16] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD8D8u); put(rt, G[16] + 0x4u, G[2]); G[4] = 0u; G[3] = word(rt, G[18] + 0x10u); G[2] = word(rt, G[22] + 0x4u); put(rt, G[17] + 0x10u, 0u); G[2] = G[2] - G[3]; G[2] = sra(G[2], 1); put(rt, G[20] + 0x4u, 0u); { const bool tk = G[2] == 0u; put(rt, G[20] + 0x8u, 0u); if (!tk) { G[30] = G[2] << 1; G[4] = G[30]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } } put(rt, G[20] + 0x4u, G[4]); G[2] = G[4] + G[30]; G[6] = G[4]; put(rt, G[20] + 0x0u, G[4]); G[7] = frame.at(0u); G[5] = word(rt, G[22] + 0x4u); G[4] = word(rt, G[22] + 0x0u); put(rt, G[20] + 0x8u, G[2]); call_here(rt, ctx, 0x08BDD910u); put(rt, G[20] + 0x4u, G[2]); } } G[2] = word(rt, frame.at(0x10u)); G[18] = G[21]; { const bool tk = G[21] != G[2]; G[17] = G[17] + 0x1Cu; if (!tk) break; } } } } G[2] = G[17];  G[21] = word(rt, frame.at(0x34u)); G[17] = word(rt, frame.at(0x24u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08C295D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[2] = G[4] + 0x4u; G[22] = G[5]; G[21] = G[4]; G[18] = word(rt, G[4] + 0x8u); { const bool tk = G[18] == 0u; put(rt, frame.at(0x10u), G[2]); if (!tk) { G[20] = frame.at(0x4u); G[23] = frame.at(0xCu); G[30] = frame.at(0x8u); while (true) { G[4] = frame.at(0u); G[19] = word(rt, G[21] + 0x0u); G[17] = G[18] + 0x10u; call_here(rt, ctx, 0x08A76AA0u); G[16] = G[20]; G[4] = G[16]; while (true) { G[16] = G[16] + 0x4u; call_here(rt, ctx, 0x08A76AA0u); { const bool tk = G[16] != G[23]; G[4] = G[16]; if (!tk) break; } } { const bool tk = G[20] == G[22]; if (!tk) { G[5] = word(rt, G[22] + 0x0u); G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); } } { const bool tk = G[30] == G[17]; if (tk) { G[16] = word(rt, G[19] + 0x2Cu); } else { G[5] = word(rt, G[17] + 0x0u); G[4] = G[30]; call_here(rt, ctx, 0x08A5BAD8u); G[16] = word(rt, G[19] + 0x2Cu); } } { const bool tk = G[16] != 0u; if (tk) { G[2] = word(rt, G[16] + 0x0u); } else { G[2] = 0x08CB0000u; G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[5] = frame.at(0u); G[3] = word(rt, G[2] + 0xCCu); G[6] = 0x2u; { const std::uint32_t tg = G[3]; G[7] = G[20]; call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x0u)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[2] = 0x08CB0000u; G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t tg = G[2]; G[16] = G[23]; call_here(rt, ctx, tg); } G[17] = G[2]; G[16] = G[16] + 0xFFFFFFFCu; while (true) { G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = G[20] != G[16]; if (!tk) break; G[16] = G[16] + 0xFFFFFFFCu; } } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = G[17] == 0u; if (tk) { G[2] = word(rt, G[18] + 0xCu); } else { G[2] = word(rt, G[18] + 0x8u); put(rt, frame.at(0x10u), G[18]); } } { const bool tk = G[2] != 0u; G[18] = G[2]; if (!tk) break; } } } } G[2] = word(rt, frame.at(0x10u));  G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08860FF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xC0u); put(rt, frame.at(0xB8u), G[22]); put(rt, frame.at(0xB4u), G[21]); put(rt, frame.at(0xB0u), G[20]); put(rt, frame.at(0xACu), G[19]); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); put(rt, frame.at(0xA0u), G[16]); put(rt, frame.at(0xBCu), G[31]); G[3] = G[5] << 6; G[2] = G[5] << 4; G[2] = G[2] + G[3]; G[2] = G[2] + 0x4120u; G[5] = 0u; G[20] = G[4]; G[19] = frame.at(0x40u); G[21] = G[7]; G[22] = frame.at(0xA0u); G[17] = G[6]; G[16] = G[19]; G[8] = word(rt, G[4] + 0x0u); G[4] = frame.at(0u); G[18] = G[8] + G[2]; call_here(rt, ctx, 0x088A79ACu); G[4] = G[16]; while (true) { G[16] = G[16] + 0x30u; call_here(rt, ctx, 0x08897B8Cu); { const bool tk = G[22] != G[16]; G[4] = G[16]; if (!tk) break; } } G[4] = word(rt, G[18] + 0x44u); G[5] = word(rt, G[18] + 0x48u); G[16] = frame.at(0x8u); G[6] = G[16]; G[7] = frame.at(0u); G[8] = G[19]; call_here(rt, ctx, 0x08866510u); G[3] = sext(load8(frame.at(0x12u)), 8); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = G[17] << 4; if (!tk) { G[2] = G[3] + 0x1u; store8(frame.at(0x12u), G[2]); G[17] = G[3]; G[2] = G[17] << 4; } } G[4] = G[17] << 6; G[4] = G[4] - G[2]; G[4] = G[19] + G[4]; G[5] = G[21]; call_here(rt, ctx, 0x08897470u); G[3] = load8(G[18] + 0x18u); { const bool tk = G[3] != 0u; G[2] = 0x1u; if (!tk) { G[4] = word(rt, G[20] + 0x0u); G[2] = sext(load16(G[18] + 0x1Eu), 16); G[5] = G[17]; G[7] = word(rt, G[4] + 0x3F50u); G[3] = G[2] << 6; G[2] = G[2] << 3; G[3] = G[3] - G[2]; G[6] = word(rt, G[7] + 0x0u); G[4] = G[3] << 3; G[4] = G[4] - G[3]; G[4] = G[6] + G[4]; G[6] = G[21]; call_here(rt, ctx, 0x0885D868u); G[2] = 0x1u; } } store8(frame.at(0x11u), G[2]); G[6] = G[16]; G[7] = frame.at(0u); G[5] = word(rt, G[18] + 0x48u); G[4] = word(rt, G[18] + 0x44u); G[8] = sext(load8(frame.at(0x12u)), 8); G[9] = G[19]; G[16] = G[22]; call_here(rt, ctx, 0x088661B8u); G[16] = G[16] + 0xFFFFFFD0u; while (true) { G[4] = G[16]; call_here(rt, ctx, 0x088978B4u); { const bool tk = G[19] != G[16]; G[16] = G[16] + 0xFFFFFFD0u; if (!tk) break; } } G[4] = frame.at(0u); call_here(rt, ctx, 0x088A7548u);  G[22] = word(rt, frame.at(0xB8u)); G[21] = word(rt, frame.at(0xB4u)); G[20] = word(rt, frame.at(0xB0u)); G[19] = word(rt, frame.at(0xACu)); G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); }  } }
void func_08A50400(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x64u), G[17]); put(rt, frame.at(0x68u), G[31]); put(rt, frame.at(0x60u), G[16]); G[2] = 0x08CE0000u; G[6] = 0x08CDF72Cu; G[10] = frame.at(0x4u); G[7] = G[10]; G[17] = G[4]; G[8] = G[6] + 0x20u; while (true) { G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[7] = G[7] + 0x10u; put(rt, G[7] + 0xFFFFFFF4u, G[3]); put(rt, G[7] + 0xFFFFFFF8u, G[4]); { const bool tk = G[6] != G[8]; put(rt, G[7] + 0xFFFFFFFCu, G[5]); if (!tk) break; } } G[3] = word(rt, G[6] + 0x8u); G[4] = word(rt, G[6] + 0x0u); G[5] = word(rt, G[6] + 0x4u); G[2] = 0x08CE0000u; G[6] = 0x08CDF700u; G[16] = frame.at(0x30u); put(rt, G[7] + 0x8u, G[3]); G[8] = G[16]; G[9] = G[6] + 0x20u; put(rt, G[7] + 0x0u, G[4]); put(rt, G[7] + 0x4u, G[5]); put(rt, frame.at(0x4u), G[17]); while (true) { G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[8] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[8] = G[8] + 0x10u; put(rt, G[8] + 0xFFFFFFF4u, G[3]); put(rt, G[8] + 0xFFFFFFF8u, G[4]); { const bool tk = G[6] != G[9]; put(rt, G[8] + 0xFFFFFFFCu, G[5]); if (!tk) break; } } G[3] = word(rt, G[6] + 0x0u); G[2] = word(rt, G[6] + 0x8u); G[6] = word(rt, G[6] + 0x4u); put(rt, G[8] + 0x0u, G[3]); G[4] = G[10]; G[5] = frame.at(0u); put(rt, G[8] + 0x8u, G[2]); put(rt, G[8] + 0x4u, G[6]); put(rt, frame.at(0x30u), G[17]); call_here(rt, ctx, 0x08C7AC98u); G[3] = word(rt, frame.at(0x0u)); { const bool tk = G[3] != 0u; put(rt, G[17] + 0x0u, G[2]); if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C7AC98u); G[3] = word(rt, frame.at(0x0u)); { const bool tk = G[3] != 0u; put(rt, G[17] + 0x4u, G[2]); if (!tk) { call_here(rt, ctx, 0x08A4F4D8u); G[4] = 0x14u; call_here(rt, ctx, 0x08B62344u); G[5] = 0x08CB0000u; G[16] = G[2]; G[4] = G[2]; G[5] = 0x08CAF384u; call_here(rt, ctx, 0x08A4FD08u); { const bool tk = G[16] == 0u; put(rt, G[17] + 0x8u, G[16]); if (!tk) { G[5] = word(rt, G[17] + 0x0u); G[4] = G[16]; G[6] = 0u; call_here(rt, ctx, 0x08A4FB80u); { const bool tk = G[2] == 0u; if (!tk) { G[4] = word(rt, G[17] + 0x8u); G[5] = word(rt, G[17] + 0x0u); G[6] = 0u; call_here(rt, ctx, 0x08A4FA18u); } } } } G[4] = 0x60u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08A50EC0u); { const bool tk = G[16] == 0u; put(rt, G[17] + 0xCu, G[16]); if (!tk) { G[5] = word(rt, G[17] + 0x4u); G[4] = G[16]; call_here(rt, ctx, 0x08A511E0u); } } } } } }  G[17] = word(rt, frame.at(0x64u)); G[16] = word(rt, frame.at(0x60u)); }  } }
void func_089C7F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[16] = word(rt, G[5] + 0x0u); { const bool tk = G[16] != 0u; G[22] = G[4]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08CA65FCu; call_here(rt, ctx, 0x08A5BB7Cu); } } G[23] = word(rt, G[16] + 0x44u); { const bool tk = G[23] == 0u; G[21] = frame.at(0xCu); if (!tk) { G[20] = frame.at(0x8u); G[19] = frame.at(0x10u); G[30] = frame.at(0x4u); G[18] = word(rt, G[23] + 0xCu); while (true) { { const bool tk = G[18] == 0u; if (tk) { G[23] = word(rt, G[23] + 0x18u); } else { G[5] = G[18]; while (true) { G[4] = G[21]; call_here(rt, ctx, 0x08A77444u); G[4] = G[19]; G[5] = G[21]; call_here(rt, ctx, 0x089BB3A8u); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, G[22] + 0x40u); G[2] = 0x08CA0000u; { const bool tk = G[17] != 0u; G[4] = 0x08CA6384u; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[16] = word(rt, frame.at(0x10u)); G[2] = 0x08CA0000u; { const bool tk = G[16] != 0u; G[4] = 0x08CA6444u; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08A7BCC0u); G[16] = G[2]; G[4] = G[20]; G[5] = G[19]; call_here(rt, ctx, 0x089BAC2Cu); G[5] = G[16]; G[6] = G[20]; G[4] = G[17]; call_here(rt, ctx, 0x08C00A44u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x10u)); G[2] = 0x08C90000u; G[5] = 0x08C8CC2Cu; G[6] = frame.at(0x1u); G[4] = G[30]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[22]; G[6] = G[16]; G[5] = 0x22u; G[7] = G[30]; call_here(rt, ctx, 0x089C79F0u); G[2] = word(rt, frame.at(0x4u)); G[4] = G[2] + 0xFFFFFFFCu; G[16] = G[2] + 0xFFFFFFF4u; G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; G[5] = 0xFFFFFFFFu; if (!tk) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, G[18] + 0x54u); { const bool tk = G[18] != 0u; G[5] = G[18]; if (!tk) break; } } G[23] = word(rt, G[23] + 0x18u); } } { const bool tk = G[23] != 0u; if (!tk) break; G[18] = word(rt, G[23] + 0xCu); } } } }  G[22] = word(rt, frame.at(0x38u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_089C99DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[18] = frame.at(0x8u); G[16] = G[4]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CC0000u; G[3] = G[16] + 0x14u; G[7] = G[16] + 0x18u; G[6] = G[16] + 0x30u; G[2] = 0x08CBB888u; G[5] = 0x08CF0000u; put(rt, G[16] + 0x0u, G[2]); G[4] = G[16] + 0x38u; G[5] = 0x08CF6ED8u; put(rt, G[3] + 0x10u, G[7]); put(rt, G[3] + 0xCu, G[7]); put(rt, G[3] + 0x14u, 0u); put(rt, G[3] + 0x4u, 0u); put(rt, G[3] + 0x8u, 0u); put(rt, G[16] + 0x30u, G[6]); put(rt, G[16] + 0x2Cu, 0u); put(rt, G[6] + 0x4u, G[6]); call_here(rt, ctx, 0x08977AE4u); G[5] = 0x08CA0000u; G[5] = 0x08CA63ACu; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0xCu); G[4] = G[16] + 0x3Cu; put(rt, frame.at(0xCu), G[17]); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[5] = 0x08CA0000u; G[4] = frame.at(0u); G[5] = 0x08CA63BCu; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0x4u); G[4] = G[16] + 0x40u; put(rt, frame.at(0x4u), G[17]); call_here(rt, ctx, 0x08A76AB4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[3] = 0x08C90000u; G[2] = 0x08C8CC74u; G[6] = word(rt, G[2] + 0x1Cu); G[7] = word(rt, 0x08C8CC74u); G[8] = word(rt, G[2] + 0x4u); G[9] = word(rt, G[2] + 0x8u); G[10] = word(rt, G[2] + 0xCu); G[11] = word(rt, G[2] + 0x10u); G[12] = word(rt, G[2] + 0x14u); G[13] = word(rt, G[2] + 0x18u); G[3] = G[16] + 0x48u; G[5] = G[16] + 0x4Cu; G[2] = 0x1u; store8(G[16] + 0x46u, G[2]); G[4] = 0u; store8(G[16] + 0x44u, 0u); store8(G[16] + 0x45u, 0u); store8(G[16] + 0x47u, 0u); put(rt, G[3] + 0x10u, G[5]); put(rt, G[3] + 0x14u, 0u); put(rt, G[3] + 0x4u, 0u); put(rt, G[3] + 0x8u, 0u); put(rt, G[3] + 0xCu, G[5]); put(rt, G[16] + 0x7Cu, G[6]); put(rt, G[16] + 0x60u, G[7]); put(rt, G[16] + 0x64u, G[8]); put(rt, G[16] + 0x68u, G[9]); put(rt, G[16] + 0x6Cu, G[10]); put(rt, G[16] + 0x70u, G[11]); put(rt, G[16] + 0x74u, G[12]); put(rt, G[16] + 0x78u, G[13]); call_here(rt, ctx, 0x0897D4ACu); { const bool tk = G[2] == 0u; G[16] = G[2]; if (!tk) { G[2] = word(rt, G[16] + 0x4u); while (true) { { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } G[16] = word(rt, G[16] + 0xCu); { const bool tk = G[16] != 0u; if (!tk) break; G[2] = word(rt, G[16] + 0x4u); } } } }  G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_088400B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); G[30] = G[4]; G[4] = frame.at(0x10u); G[22] = frame.at(0x14u); put(rt, frame.at(0x10u), 0u); call_here(rt, ctx, 0x0892C890u); G[4] = G[22]; G[5] = word(rt, frame.at(0x10u)); G[16] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0x10u)); { const bool tk = G[2] == 0u; G[17] = G[16]; if (!tk) { G[21] = frame.at(0xCu); G[20] = frame.at(0x4u); G[19] = frame.at(0x18u); G[18] = 0u; G[23] = frame.at(0x8u); while (true) { G[4] = word(rt, G[17] + 0x0u); call_here(rt, ctx, 0x0892C880u); G[4] = G[19]; G[5] = 0x2u; G[16] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0x18u)); G[5] = G[16]; G[4] = G[23]; G[16] = word(rt, G[2] + 0x14u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08B85EB8u); G[4] = G[21]; G[5] = G[23]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[16] == G[21]; G[5] = word(rt, frame.at(0xCu)); if (!tk) { G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x8u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[2] = word(rt, frame.at(0x18u)); if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = word(rt, frame.at(0x18u)); if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[2] = word(rt, frame.at(0x18u)); } } } } G[5] = word(rt, G[17] + 0x0u); G[4] = G[20]; G[3] = word(rt, G[2] + 0x14u); G[16] = G[3] + 0x4u; call_here(rt, ctx, 0x08A85B1Cu); { const bool tk = G[16] == G[20]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x14u)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x14u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C9C42Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x14u); } } G[2] = G[18] << 2; G[4] = G[3] + G[2]; { const bool tk = G[4] == G[19]; if (!tk) { G[5] = word(rt, frame.at(0x18u)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x10u)); G[18] = G[18] + 0x1u; G[2] = G[18] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = G[17] + 0x8u; if (!tk) break; } } } } { const bool tk = G[30] == G[22]; G[5] = word(rt, frame.at(0x14u)); if (!tk) { G[4] = G[30]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu);  G[30] = word(rt, frame.at(0x40u)); G[22] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x3Cu)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); }  } }
void func_088404F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); G[30] = G[4]; G[4] = frame.at(0x10u); G[22] = frame.at(0x14u); put(rt, frame.at(0x10u), 0u); call_here(rt, ctx, 0x0892C8C8u); G[4] = G[22]; G[5] = word(rt, frame.at(0x10u)); G[16] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0x10u)); { const bool tk = G[2] == 0u; G[17] = G[16]; if (!tk) { G[21] = frame.at(0xCu); G[20] = frame.at(0x4u); G[19] = frame.at(0x18u); G[18] = 0u; G[23] = frame.at(0x8u); while (true) { G[4] = word(rt, G[17] + 0x0u); call_here(rt, ctx, 0x0892C8B8u); G[4] = G[19]; G[5] = 0x2u; G[16] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0x18u)); G[5] = G[16]; G[4] = G[23]; G[16] = word(rt, G[2] + 0x14u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08B85EB8u); G[4] = G[21]; G[5] = G[23]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[16] == G[21]; G[5] = word(rt, frame.at(0xCu)); if (!tk) { G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x8u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[2] = word(rt, frame.at(0x18u)); if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = word(rt, frame.at(0x18u)); if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[2] = word(rt, frame.at(0x18u)); } } } } G[5] = word(rt, G[17] + 0x0u); G[4] = G[20]; G[3] = word(rt, G[2] + 0x14u); G[16] = G[3] + 0x4u; call_here(rt, ctx, 0x08A85B1Cu); { const bool tk = G[16] == G[20]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x14u)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x14u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C9C42Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x14u); } } G[2] = G[18] << 2; G[4] = G[3] + G[2]; { const bool tk = G[4] == G[19]; if (!tk) { G[5] = word(rt, frame.at(0x18u)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x10u)); G[18] = G[18] + 0x1u; G[2] = G[18] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = G[17] + 0x8u; if (!tk) break; } } } } { const bool tk = G[30] == G[22]; G[5] = word(rt, frame.at(0x14u)); if (!tk) { G[4] = G[30]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu);  G[30] = word(rt, frame.at(0x40u)); G[22] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x3Cu)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); }  } }
// 0x089A7A38 (a0 = the object, a1 = a value; v0 = the last call's): 0x089A717C(object, value) must give non-zero and 0x089A6EA4(object) must give non-zero. A handle on the frame (+0x10) is set by 0x089A6FCC(handle, object); its target (a null one is
// not called) gets the virtual +0x1B8 and +0x19C calls (a null second read calls the assert 0x08A5BB7C(0x08CA4FA4)). With the object's word +0x28 set: two empty handles (+0x14, then +0x18 / +0x1C: 0x08A76AA0) and two copies
// (0x08A76AAC(+0xC, +8) with +8 = the value, then 0x08A5BAD8(+0x18, +0xC's word); 0x08A76AAC(+4, +0) with +0 = the object, then 0x08A5BAD8(+0x1C, +4's word) unless it is the same handle) are made and released (0x08A5B9EC); the word's virtual +0xCC is
// called with (word, +0x14, 2, +0x18) (a null word calls the assert 0x08A5BB7C(0x08CA4F34)) and the handles +0x1C, +0x18 and +0x14 are released. At the end 0x089A6EF0(object, 0), 0x089A6F84(object, 0) and 0x08A5B9EC(+0x10).
void func_089A7A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x2Cu}, {17u, 0x24u}, {31u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {18u, 0x28u}, {16u, 0x20u}});
    ctx.gpr[19] = value;
    ctx.gpr[17] = object;
    if (call_set(rt, ctx, 0x089A717Cu, {object, value}) == 0u) return;
    if (call_set(rt, ctx, 0x089A6EA4u, {object}) == 0u) return;
    const u32 handle = frame.at(0x10u);
    ctx.gpr[22] = handle;
    call_set(rt, ctx, 0x089A6FCCu, {handle, object});
    const u32 filled = word(rt, handle);
    if (filled != 0u) {
        call_set(rt, ctx, word(rt, word(rt, filled) + 0x1B8u), {filled});
        const u32 target = word(rt, handle);
        ctx.gpr[16] = target;
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4FA4u});
        call_set(rt, ctx, word(rt, word(rt, target) + 0x19Cu), {target});
    }
    if (word(rt, object + 0x28u) != 0u) {
        const u32 record = frame.at(0x14u), first = frame.at(0x18u), end = frame.at(0x20u);
        ctx.gpr[20] = record;
        ctx.gpr[18] = first;
        ctx.gpr[21] = end;
        call_set(rt, ctx, 0x08A76AA0u, {record});
        for (u32 slot = first; slot != end; slot += 0x4u) {
            ctx.gpr[16] = slot + 0x4u;
            call_set(rt, ctx, 0x08A76AA0u, {slot});
        }
        const u32 copy = frame.at(0xCu), second = frame.at(0x4u);
        ctx.gpr[16] = copy;
        put(rt, frame.at(0x8u), value);
        call_set(rt, ctx, 0x08A76AACu, {copy, frame.at(0x8u)});
        if (first != copy) call_set(rt, ctx, 0x08A5BAD8u, {first, word(rt, copy)});
        ctx.gpr[16] = second;
        call_set(rt, ctx, 0x08A5B9ECu, {copy});
        put(rt, frame.at(0x0u), object);
        call_set(rt, ctx, 0x08A76AACu, {second, frame.at(0u)});
        if (frame.at(0x1Cu) != second) call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x1Cu), word(rt, second)});
        call_set(rt, ctx, 0x08A5B9ECu, {second});
        const u32 member = word(rt, object + 0x28u);
        ctx.gpr[16] = member;
        if (member == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4F34u});
        call_set(rt, ctx, word(rt, word(rt, member) + 0xCCu), {member, record, 2u, first});
        for (u32 slot = end - 0x4u;; slot -= 0x4u) {
            ctx.gpr[16] = slot;
            call_set(rt, ctx, 0x08A5B9ECu, {slot});
            if (slot == first) break;
        }
        call_set(rt, ctx, 0x08A5B9ECu, {record});
    }
    call_set(rt, ctx, 0x089A6EF0u, {object, 0u});
    call_set(rt, ctx, 0x089A6F84u, {object, 0u});
    call_set(rt, ctx, 0x08A5B9ECu, {handle});
}
// 0x089A7F7C (a0 = the object, a1 = a value; v0 = the last call's): 0x089A717C(object, value) must give non-zero and 0x089A6EA4(object) must not give 2. A handle on the frame (+0x10) is set by 0x089A7024(handle, object); its target (a null one is
// not called) gets the virtual +0x1B8 and +0x19C calls (a null second read calls the assert 0x08A5BB7C(0x08CA4FA4)). With the object's word +0x30 set: two empty handles (+0x14, then +0x18 / +0x1C: 0x08A76AA0) and two copies
// (0x08A76AAC(+0xC, +8) with +8 = the value, then 0x08A5BAD8(+0x18, +0xC's word); 0x08A76AAC(+4, +0) with +0 = the object, then 0x08A5BAD8(+0x1C, +4's word) unless it is the same handle) are made and released (0x08A5B9EC); the word's virtual +0xCC is
// called with (word, +0x14, 2, +0x18) (a null word calls the assert 0x08A5BB7C(0x08CA4F34)) and the handles +0x1C, +0x18 and +0x14 are released. At the end 0x089A6EF0(object, 2), 0x089A6F84(object, 0) and 0x08A5B9EC(+0x10).
void func_089A7F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x2Cu}, {17u, 0x24u}, {31u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {18u, 0x28u}, {16u, 0x20u}});
    ctx.gpr[19] = value;
    ctx.gpr[17] = object;
    if (call_set(rt, ctx, 0x089A717Cu, {object, value}) == 0u) return;
    if (call_set(rt, ctx, 0x089A6EA4u, {object}) == 2u) return;
    const u32 handle = frame.at(0x10u);
    ctx.gpr[22] = handle;
    call_set(rt, ctx, 0x089A7024u, {handle, object});
    const u32 filled = word(rt, handle);
    if (filled != 0u) {
        call_set(rt, ctx, word(rt, word(rt, filled) + 0x1B8u), {filled});
        const u32 target = word(rt, handle);
        ctx.gpr[16] = target;
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4FA4u});
        call_set(rt, ctx, word(rt, word(rt, target) + 0x19Cu), {target});
    }
    if (word(rt, object + 0x30u) != 0u) {
        const u32 record = frame.at(0x14u), first = frame.at(0x18u), end = frame.at(0x20u);
        ctx.gpr[20] = record;
        ctx.gpr[18] = first;
        ctx.gpr[21] = end;
        call_set(rt, ctx, 0x08A76AA0u, {record});
        for (u32 slot = first; slot != end; slot += 0x4u) {
            ctx.gpr[16] = slot + 0x4u;
            call_set(rt, ctx, 0x08A76AA0u, {slot});
        }
        const u32 copy = frame.at(0xCu), second = frame.at(0x4u);
        ctx.gpr[16] = copy;
        put(rt, frame.at(0x8u), value);
        call_set(rt, ctx, 0x08A76AACu, {copy, frame.at(0x8u)});
        if (first != copy) call_set(rt, ctx, 0x08A5BAD8u, {first, word(rt, copy)});
        ctx.gpr[16] = second;
        call_set(rt, ctx, 0x08A5B9ECu, {copy});
        put(rt, frame.at(0x0u), object);
        call_set(rt, ctx, 0x08A76AACu, {second, frame.at(0u)});
        if (frame.at(0x1Cu) != second) call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x1Cu), word(rt, second)});
        call_set(rt, ctx, 0x08A5B9ECu, {second});
        const u32 member = word(rt, object + 0x30u);
        ctx.gpr[16] = member;
        if (member == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4F34u});
        call_set(rt, ctx, word(rt, word(rt, member) + 0xCCu), {member, record, 2u, first});
        for (u32 slot = end - 0x4u;; slot -= 0x4u) {
            ctx.gpr[16] = slot;
            call_set(rt, ctx, 0x08A5B9ECu, {slot});
            if (slot == first) break;
        }
        call_set(rt, ctx, 0x08A5B9ECu, {record});
    }
    call_set(rt, ctx, 0x089A6EF0u, {object, 2u});
    call_set(rt, ctx, 0x089A6F84u, {object, 0u});
    call_set(rt, ctx, 0x08A5B9ECu, {handle});
}
// 0x08A10820 (a0 = the object, a1 = a key; v0 = the last call's): under the lock 0x08B344E0(0x08CF61B0) ... 0x08B3457C(0x08CF61B0). With the member at +0x6C set: two empty handles on the frame (+0x20, +0x24: 0x08A76AA0) and two
// copies (0x08A76AAC(+0x18, +0x14) with +0x14 = the key, then 0x08A5BAD8(+0x20, +0x18's word); 0x08A76AAC(+0x10, +0xC) with +0xC = the object, then 0x08A5BAD8(+0x24, +0x10's word) unless it is the same handle) are made and released
// (0x08A5B9EC); a handle +0x1C is emptied and the member's virtual +0xCC is called with (member, +0x1C, 2, +0x20) (a null member calls the assert 0x08A5BB7C(0x08CA9714)); the handles +0x1C, +0x24 and +0x20 are released.
// With the member at +0x64 set: 0x08A98C1C(+0x1C, 1) gives an entry whose word +0x14 takes a copy of the object (0x08A76AAC(+8, +4), 0x08A5BAD8(entry word, +8's word) when different); then 0x0899C97C(+0, key, object, object + 0x64, +0x1C)
// and 0x089A9324(key, +0) run and the handles +0, +0x1C and +8 are released.
void func_08A10820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x48u}, {18u, 0x38u}, {20u, 0x40u}, {31u, 0x4Cu}, {21u, 0x44u}, {19u, 0x3Cu}, {17u, 0x34u}, {16u, 0x30u}});
    const u32 lock = 0x08CF61B0u;
    ctx.gpr[22] = high_half(lock);
    ctx.gpr[18] = object;
    ctx.gpr[20] = key;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    if (word(rt, object + 0x6Cu) != 0u) {
        const u32 first = frame.at(0x20u), end = frame.at(0x28u);
        ctx.gpr[19] = first;
        ctx.gpr[21] = end;
        for (u32 slot = first; slot != end; slot += 0x4u) {
            ctx.gpr[16] = slot + 0x4u;
            call_set(rt, ctx, 0x08A76AA0u, {slot});
        }
        const u32 copy = frame.at(0x18u);
        ctx.gpr[16] = copy;
        put(rt, frame.at(0x14u), key);
        call_set(rt, ctx, 0x08A76AACu, {copy, frame.at(0x14u)});
        if (first != copy) call_set(rt, ctx, 0x08A5BAD8u, {first, word(rt, copy)});
        const u32 second = frame.at(0x10u);
        ctx.gpr[16] = second;
        call_set(rt, ctx, 0x08A5B9ECu, {copy});
        put(rt, frame.at(0xCu), object);
        call_set(rt, ctx, 0x08A76AACu, {second, frame.at(0xCu)});
        if (frame.at(0x24u) != second) call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x24u), word(rt, second)});
        const u32 result_handle = frame.at(0x1Cu);
        ctx.gpr[17] = result_handle;
        call_set(rt, ctx, 0x08A5B9ECu, {second});
        call_set(rt, ctx, 0x08A76AA0u, {result_handle});
        const u32 member = word(rt, object + 0x6Cu);
        ctx.gpr[16] = member;
        if (member == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA9714u});
        const u32 virtual_call = word(rt, word(rt, member) + 0xCCu);
        call_set(rt, ctx, virtual_call, {member, result_handle, 2u, first});
        call_set(rt, ctx, 0x08A5B9ECu, {result_handle});
        for (u32 slot = end - 0x4u;; slot -= 0x4u) {
            ctx.gpr[16] = slot;
            call_set(rt, ctx, 0x08A5B9ECu, {slot});
            if (slot == first) break;
        }
    }
    if (word(rt, object + 0x64u) != 0u) {
        const u32 entry_handle = frame.at(0x1Cu), copy = frame.at(0x8u);
        ctx.gpr[19] = entry_handle;
        call_set(rt, ctx, 0x08A98C1Cu, {entry_handle, 1u});
        const u32 entry = word(rt, entry_handle);
        ctx.gpr[17] = copy;
        const u32 slot_word = word(rt, entry + 0x14u);
        ctx.gpr[16] = slot_word;
        put(rt, frame.at(0x4u), object);
        call_set(rt, ctx, 0x08A76AACu, {copy, frame.at(0x4u)});
        if (slot_word != copy) call_set(rt, ctx, 0x08A5BAD8u, {slot_word, word(rt, copy)});
        call_set(rt, ctx, 0x08A5B9ECu, {copy});
        call_set(rt, ctx, 0x0899C97Cu, {frame.at(0u), key, object, object + 0x64u, entry_handle});
        call_set(rt, ctx, 0x089A9324u, {key, frame.at(0u)});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        call_set(rt, ctx, 0x08A5B9ECu, {entry_handle});
    }
    call_set(rt, ctx, 0x08B3457Cu, {lock});
}
// 0x08A10A88 (a0 = the object, a1 = a key; v0 = the last call's): under the lock 0x08B344E0(0x08CF61B0) ... 0x08B3457C(0x08CF61B0). With the member at +0x70 set: two empty handles on the frame (+0x20, +0x24: 0x08A76AA0) and two
// copies (0x08A76AAC(+0x18, +0x14) with +0x14 = the key, then 0x08A5BAD8(+0x20, +0x18's word); 0x08A76AAC(+0x10, +0xC) with +0xC = the object, then 0x08A5BAD8(+0x24, +0x10's word) unless it is the same handle) are made and released
// (0x08A5B9EC); a handle +0x1C is emptied and the member's virtual +0xCC is called with (member, +0x1C, 2, +0x20) (a null member calls the assert 0x08A5BB7C(0x08CA9714)); the handles +0x1C, +0x24 and +0x20 are released.
// With the member at +0x68 set: 0x08A98C1C(+0x1C, 1) gives an entry whose word +0x14 takes a copy of the object (0x08A76AAC(+8, +4), 0x08A5BAD8(entry word, +8's word) when different); then 0x0899C97C(+0, key, object, object + 0x64, +0x1C)
// and 0x089A9324(key, +0) run and the handles +0, +0x1C and +8 are released.
void func_08A10A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x48u}, {18u, 0x38u}, {20u, 0x40u}, {31u, 0x4Cu}, {21u, 0x44u}, {19u, 0x3Cu}, {17u, 0x34u}, {16u, 0x30u}});
    const u32 lock = 0x08CF61B0u;
    ctx.gpr[22] = high_half(lock);
    ctx.gpr[18] = object;
    ctx.gpr[20] = key;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    if (word(rt, object + 0x70u) != 0u) {
        const u32 first = frame.at(0x20u), end = frame.at(0x28u);
        ctx.gpr[19] = first;
        ctx.gpr[21] = end;
        for (u32 slot = first; slot != end; slot += 0x4u) {
            ctx.gpr[16] = slot + 0x4u;
            call_set(rt, ctx, 0x08A76AA0u, {slot});
        }
        const u32 copy = frame.at(0x18u);
        ctx.gpr[16] = copy;
        put(rt, frame.at(0x14u), key);
        call_set(rt, ctx, 0x08A76AACu, {copy, frame.at(0x14u)});
        if (first != copy) call_set(rt, ctx, 0x08A5BAD8u, {first, word(rt, copy)});
        const u32 second = frame.at(0x10u);
        ctx.gpr[16] = second;
        call_set(rt, ctx, 0x08A5B9ECu, {copy});
        put(rt, frame.at(0xCu), object);
        call_set(rt, ctx, 0x08A76AACu, {second, frame.at(0xCu)});
        if (frame.at(0x24u) != second) call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x24u), word(rt, second)});
        const u32 result_handle = frame.at(0x1Cu);
        ctx.gpr[17] = result_handle;
        call_set(rt, ctx, 0x08A5B9ECu, {second});
        call_set(rt, ctx, 0x08A76AA0u, {result_handle});
        const u32 member = word(rt, object + 0x70u);
        ctx.gpr[16] = member;
        if (member == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA9714u});
        const u32 virtual_call = word(rt, word(rt, member) + 0xCCu);
        call_set(rt, ctx, virtual_call, {member, result_handle, 2u, first});
        call_set(rt, ctx, 0x08A5B9ECu, {result_handle});
        for (u32 slot = end - 0x4u;; slot -= 0x4u) {
            ctx.gpr[16] = slot;
            call_set(rt, ctx, 0x08A5B9ECu, {slot});
            if (slot == first) break;
        }
    }
    if (word(rt, object + 0x68u) != 0u) {
        const u32 entry_handle = frame.at(0x1Cu), copy = frame.at(0x8u);
        ctx.gpr[19] = entry_handle;
        call_set(rt, ctx, 0x08A98C1Cu, {entry_handle, 1u});
        const u32 entry = word(rt, entry_handle);
        ctx.gpr[17] = copy;
        const u32 slot_word = word(rt, entry + 0x14u);
        ctx.gpr[16] = slot_word;
        put(rt, frame.at(0x4u), object);
        call_set(rt, ctx, 0x08A76AACu, {copy, frame.at(0x4u)});
        if (slot_word != copy) call_set(rt, ctx, 0x08A5BAD8u, {slot_word, word(rt, copy)});
        call_set(rt, ctx, 0x08A5B9ECu, {copy});
        call_set(rt, ctx, 0x0899C97Cu, {frame.at(0u), key, object, object + 0x68u, entry_handle});
        call_set(rt, ctx, 0x089A9324u, {key, frame.at(0u)});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        call_set(rt, ctx, 0x08A5B9ECu, {entry_handle});
    }
    call_set(rt, ctx, 0x08B3457Cu, {lock});
}
void func_08C3308C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x50u), G[30]); put(rt, frame.at(0x4Cu), G[23]); put(rt, frame.at(0x48u), G[22]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); G[4] = G[5]; G[20] = G[5]; G[5] = frame.at(0x10u); put(rt, frame.at(0x20u), G[6]); call_here(rt, ctx, 0x08C319D8u); G[4] = frame.at(0x14u); call_here(rt, ctx, 0x08A813F4u); G[2] = word(rt, frame.at(0x10u)); { const bool tk = G[2] == 0u; G[3] = word(rt, frame.at(0x20u)); if (!tk) { G[23] = frame.at(0xCu); G[19] = frame.at(0x1Cu); G[22] = 0u; G[30] = frame.at(0x18u); G[21] = frame.at(0x4u); G[16] = word(rt, G[20] + 0x4u); while (true) { G[2] = 0x08D70000u; G[4] = 0x08CB0000u; G[2] = 0x08D74FDCu; G[22] = G[22] + 0x1u; G[4] = 0x08CB30B0u; { const bool tk = G[16] != 0u; put(rt, frame.at(0x18u), G[2]); if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[5] = G[30]; G[4] = G[16]; call_here(rt, ctx, 0x08C313B4u); G[4] = G[19]; call_here(rt, ctx, 0x08A76AA0u); G[4] = G[20]; G[5] = G[19]; call_here(rt, ctx, 0x08C32AD4u); G[18] = word(rt, frame.at(0x14u)); G[4] = 0x08CB0000u; { const bool tk = G[18] != 0u; G[4] = 0x08CB3148u; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[2] = word(rt, G[18] + 0x0u); G[4] = G[21]; G[5] = G[30]; G[17] = word(rt, G[2] + 0xA0u); call_here(rt, ctx, 0x08A69654u); G[5] = 0x4u; G[4] = 0x18u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08A76ACCu); G[3] = 0x08CB0000u; G[2] = 0x08CB6B88u; put(rt, G[16] + 0x0u, G[2]); G[4] = G[16] + 0x14u; G[5] = G[21]; call_here(rt, ctx, 0x08A6972Cu); G[5] = frame.at(0x8u); G[4] = G[23]; put(rt, frame.at(0x8u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[21]; call_here(rt, ctx, 0x08A69D68u); G[5] = G[23]; G[6] = G[19]; { const std::uint32_t tg = G[17]; G[4] = G[18]; call_here(rt, ctx, tg); } G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x18u)); G[4] = G[2] + 0xFFFFFFFCu; G[16] = G[2] + 0xFFFFFFF4u; G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; { const bool tk = G[2] == G[16]; G[5] = 0xFFFFFFFFu; if (!tk) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x08B8542Cu); } } } } G[2] = word(rt, frame.at(0x10u)); G[2] = G[22] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; if (!tk) break; G[16] = word(rt, G[20] + 0x4u); } } G[3] = word(rt, frame.at(0x20u)); } } G[2] = frame.at(0x14u); { const bool tk = G[3] == G[2]; G[5] = word(rt, frame.at(0x14u)); if (!tk) { G[4] = G[3]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = frame.at(0x14u); call_here(rt, ctx, 0x08A5B9ECu);  G[20] = word(rt, frame.at(0x40u)); G[30] = word(rt, frame.at(0x50u)); G[23] = word(rt, frame.at(0x4Cu)); G[22] = word(rt, frame.at(0x48u)); G[21] = word(rt, frame.at(0x44u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
void func_088E4168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x240u); put(rt, frame.at(0x224u), G[21]); put(rt, frame.at(0x214u), G[17]); put(rt, frame.at(0x230u), G[31]); put(rt, frame.at(0x22Cu), G[23]); put(rt, frame.at(0x228u), G[22]); put(rt, frame.at(0x220u), G[20]); put(rt, frame.at(0x21Cu), G[19]); put(rt, frame.at(0x218u), G[18]); put(rt, frame.at(0x210u), G[16]); G[21] = G[5]; G[17] = G[4]; G[2] = word(rt, G[4] + 0x0u); G[3] = word(rt, G[2] + 0x0u); G[4] = G[2]; G[23] = word(rt, G[2] + 0xACu); G[2] = word(rt, G[3] + 0x44u); { const std::uint32_t tg = G[2]; G[5] = frame.at(0u); call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; G[16] = frame.at(0x8u); if (!tk) { G[6] = word(rt, frame.at(0x0u)); G[7] = 0u; G[4] = G[23]; G[5] = 0x34u; call_here(rt, ctx, 0x088EF438u); G[2] = sext(load8(frame.at(0x4u)), 8); G[4] = G[23]; G[5] = 0x35u; G[6] = G[2]; G[7] = sra(G[2], 31); call_here(rt, ctx, 0x088EF438u); G[2] = sext(load8(frame.at(0x5u)), 8); G[4] = G[23]; G[5] = 0x36u; G[6] = G[2]; G[7] = sra(G[2], 31); call_here(rt, ctx, 0x088EF438u); G[2] = sext(load8(frame.at(0x6u)), 8); G[4] = G[23]; G[5] = 0x37u; G[6] = G[2]; G[7] = sra(G[2], 31); call_here(rt, ctx, 0x088EF438u); G[2] = sext(load8(frame.at(0x7u)), 8); G[4] = G[23]; G[5] = 0x38u; G[6] = G[2]; G[7] = sra(G[2], 31); call_here(rt, ctx, 0x088EF438u); G[16] = frame.at(0x8u); } } G[4] = G[16]; G[5] = 0u; G[6] = 0x200u; call_here(rt, ctx, 0x08B36018u); G[4] = word(rt, G[17] + 0x0u); G[5] = G[16]; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x48u); { const std::uint32_t tg = G[2]; G[6] = 0xFFFFFFFFu; call_here(rt, ctx, tg); } { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[22] = G[2]; if (!tk) { G[19] = G[21] + 0x100u; G[20] = G[21] + 0x200u; G[18] = 0u; G[17] = 0x08CA0000u; while (true) { G[4] = G[21]; call_here(rt, ctx, 0x08B35F60u); G[6] = word(rt, G[16] + 0x0u); G[7] = word(rt, G[16] + 0x4u); G[5] = 0x08CA0AB8u; G[4] = G[21] + G[2]; call_here(rt, ctx, 0x08B37640u); G[4] = G[19]; call_here(rt, ctx, 0x08B35F60u); G[6] = word(rt, G[16] + 0x0u); G[7] = word(rt, G[16] + 0x8u); G[5] = 0x08CA0AB8u; G[4] = G[19] + G[2]; call_here(rt, ctx, 0x08B37640u); G[4] = G[20]; call_here(rt, ctx, 0x08B35F60u); G[6] = word(rt, G[16] + 0x0u); G[7] = word(rt, G[16] + 0xCu); G[4] = G[20] + G[2]; G[5] = 0x08CA0AB8u; G[18] = G[18] + 0x1u; call_here(rt, ctx, 0x08B37640u); { const bool tk = G[22] != G[18]; G[16] = G[16] + 0x10u; if (!tk) break; } } G[4] = G[21]; call_here(rt, ctx, 0x08B35F60u); G[2] = G[21] + G[2]; G[4] = G[19]; store8(G[2] + 0xFFFFFFFFu, 0u); call_here(rt, ctx, 0x08B35F60u); G[2] = G[19] + G[2]; G[4] = G[20]; store8(G[2] + 0xFFFFFFFFu, 0u); call_here(rt, ctx, 0x08B35F60u); G[2] = G[20] + G[2]; store8(G[2] + 0xFFFFFFFFu, 0u); G[6] = G[21]; G[5] = 0x2Bu; G[4] = G[23]; call_here(rt, ctx, 0x088EF430u); G[4] = G[23]; G[6] = G[19]; G[5] = 0x2Cu; call_here(rt, ctx, 0x088EF430u); G[4] = G[23]; G[6] = G[20]; G[5] = 0x2Du; call_here(rt, ctx, 0x088EF430u); } }  G[21] = word(rt, frame.at(0x224u)); G[17] = word(rt, frame.at(0x214u)); G[23] = word(rt, frame.at(0x22Cu)); G[22] = word(rt, frame.at(0x228u)); G[20] = word(rt, frame.at(0x220u)); G[19] = word(rt, frame.at(0x21Cu)); G[18] = word(rt, frame.at(0x218u)); G[16] = word(rt, frame.at(0x210u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_317
} // namespace gt::game

#define HOT317(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_317::name, gt::ReturnKind::kind)
HOT317(0x08BDAA80u, func_08BDAA80, Word);
HOT317(0x0883E3E0u, func_0883E3E0, Word);
HOT317(0x08AC1934u, func_08AC1934, Word);
// HOT317(0x08BDE70Cu, func_08BDE70C, Word);  // no comparable synthetic case yet
HOT317(0x08ABED84u, func_08ABED84, Word);
HOT317(0x08BDE2F0u, func_08BDE2F0, Word);
HOT317(0x08BDE5B0u, func_08BDE5B0, Word);
HOT317(0x0883FF50u, func_0883FF50, Word);
HOT317(0x08840390u, func_08840390, Word);
// HOT317(0x08BDE18Cu, func_08BDE18C, Word);  // no comparable synthetic case yet
// HOT317(0x08BDE44Cu, func_08BDE44C, Word);  // no comparable synthetic case yet
// HOT317(0x08BDEAD8u, func_08BDEAD8, Word);  // no comparable synthetic case yet
HOT317(0x08C295D0u, func_08C295D0, Word);
// HOT317(0x08860FF8u, func_08860FF8, Word);  // no comparable synthetic case yet
// HOT317(0x08A50400u, func_08A50400, Word);  // no comparable synthetic case yet
HOT317(0x089C7F40u, func_089C7F40, Word);
HOT317(0x089C99DCu, func_089C99DC, Word);
HOT317(0x088400B4u, func_088400B4, Word);
HOT317(0x088404F4u, func_088404F4, Word);
HOT317(0x089A7A38u, func_089A7A38, Word);
HOT317(0x089A7F7Cu, func_089A7F7C, Word);
HOT317(0x08A10820u, func_08A10820, Word);
HOT317(0x08A10A88u, func_08A10A88, Word);
// HOT317(0x08C3308Cu, func_08C3308C, Word);  // no comparable synthetic case yet
// HOT317(0x088E4168u, func_088E4168, Word);  // no comparable synthetic case yet
