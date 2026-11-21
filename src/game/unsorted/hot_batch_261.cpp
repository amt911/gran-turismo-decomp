// Hot batch 261: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (a float wrap into a range, clamped float setters with dirty flags, a jump table on a code, a range search over
// float pairs, a retry loop and calls through saved registers).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_261 {
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

void func_089010A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; G[5] = G[5] & 0xFFu; F[12] = F[12] - F[13]; if (G[5] == 0u) { F[1] = float_at(rt, G[4] + 0x14u); G[1] = 0x3F000000u; F[0] = std::bit_cast<float>(G[1]); F[0] = F[1] * F[0]; const bool above = F[0] < F[12]; F[2] = std::bit_cast<float>(std::bit_cast<std::uint32_t>(F[0]) ^ 0x80000000u); if (above) F[12] = F[12] - F[1]; else if (F[12] < F[2]) F[12] = F[12] + F[1]; } F[0] = F[12]; } }
void func_08930244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[31]); put(rt, frame.at(4u), G[17]); G[17] = G[5]; put(rt, frame.at(0u), G[16]); G[16] = G[4]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x08936578u); G[3] = 0u; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); G[5] = 0x28u; G[3] = G[5]; } }
void func_0894D5F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[31]); G[2] = word(rt, G[4] + 8u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) { G[4] = 0u; G[2] = G[4]; return; } call_here(rt, ctx, 0x0894CF40u); G[3] = G[2]; G[2] = word(rt, G[2] + 4u); G[4] = 1u; if (G[2] == 0u) G[4] = 0u; else put(rt, G[3] + 8u, 0u); G[2] = G[4]; } }
void func_0895FF70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = load16(G[4] + 0x30u); G[5] = 1u; if (G[2] == 0u) { G[2] = 0x08CE0000u; G[3] = load8(0x08CDE70Du); if (G[3] == 0u) G[5] = 0u; else { G[2] = load8(G[4] + 0x32u); if (G[2] == 0u) { G[2] = 0x08CF0000u; G[3] = load8(0x08CF3650u); if (G[3] != 0u) G[5] = 0u; else { G[2] = load8(G[4] + 0x33u); if (G[2] == 0u) G[5] = 0u; } } } } G[2] = G[5]; } }
void func_0898D854(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[6] = word(rt, G[4] + 0x4BCu); G[2] = word(rt, G[4] + 0x4A8u); G[3] = word(rt, G[4] + 0x4ACu); G[7] = 0u; G[2] = G[2] - G[6]; G[2] = G[2] - G[3]; G[3] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = G[6] + G[2]; G[5] = G[6] + G[5]; put(rt, G[4] + 0x4BCu, G[3] == 0u ? G[2] : G[5]); G[2] = 1u; G[7] = 1u; store8(G[4] + 0x4C0u, G[2]); } G[2] = G[7]; } }
void func_089963EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[17] = 2u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(8u), G[31]); for (;;) { G[4] = G[16]; call_here(rt, ctx, 0x0899639Cu); if (G[2] == G[17]) break; call_here(rt, ctx, 0x08B1A844u); G[4] = 1u; call_here(rt, ctx, 0x089C4558u); } G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08B1A174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[4] = G[4] + 0x10u; jump(ctx, 0x08B1A16Cu); } }
void func_08B52320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[4] < 0xBu ? 1u : 0u; G[3] = 0x08C90000u; if (G[2] == 0u) { G[2] = 0u; return; } G[2] = G[4] << 2; G[3] = 0x08C955B8u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B52344u: G[2] = 8u; break; case 0x08B52354u: G[2] = 0x10u; break; case 0x08B5235Cu: G[2] = 0x20u; break; case 0x08B52364u: G[2] = 4u; break; default: G[2] = 0u; break; } } }
void func_08965C40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; F[0] = 0.0f; G[5] = G[4]; if (F[12] < F[0]) F[12] = F[0]; F[0] = float_at(rt, G[5] + 0x78u); G[2] = 0x00100000u; if (!(F[12] == F[0])) { G[4] = word(rt, G[5] + 0xE0u); G[3] = word(rt, G[5] + 0xE4u); G[4] = G[4] | G[2]; G[3] = G[3] | 4u; put_float(rt, G[5] + 0x78u, F[12]); put(rt, G[5] + 0xE4u, G[3]); put(rt, G[5] + 0xE0u, G[4]); } } }
void func_08965C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; F[0] = 0.0f; G[5] = G[4]; if (F[12] < F[0]) F[12] = F[0]; F[0] = float_at(rt, G[5] + 0x7Cu); G[2] = 0x00100000u; if (!(F[12] == F[0])) { G[4] = word(rt, G[5] + 0xE0u); G[3] = word(rt, G[5] + 0xE4u); G[4] = G[4] | G[2]; G[3] = G[3] | 4u; put_float(rt, G[5] + 0x7Cu, F[12]); put(rt, G[5] + 0xE4u, G[3]); put(rt, G[5] + 0xE0u, G[4]); } } }
void func_0892B9DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = G[5]; put(rt, frame.at(4u), G[31]); G[5] = G[16]; call_here(rt, ctx, 0x08B2235Cu); G[2] = word(rt, G[16] + 0x2Cu); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[5] = word(rt, G[16] + 0x260u); G[3] = word(rt, G[2] + 0xCu); G[5] = G[5] + 0x40u; call_here(rt, ctx, G[3]); } G[2] = word(rt, G[16] + 0x260u); put(rt, G[2], 0u); G[16] = word(rt, frame.at(0u)); } }
void func_08A6872C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = 0x10u; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); G[17] = G[5]; call_here(rt, ctx, 0x08A5BC14u); G[18] = G[2]; G[4] = G[2]; G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x08A68700u); G[2] = G[18]; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08918C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; G[6] = word(rt, G[5] + 0x20u); G[2] = 0u; if (static_cast<std::int32_t>(G[6]) <= 0) return; G[5] = word(rt, G[5] + 0x24u); G[4] = 0u; F[0] = float_at(rt, G[5] + 4u); for (;;) { G[4] = G[4] + 1u; G[2] = G[5]; const bool low = F[0] <= F[12]; G[3] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[5] = G[5] + 0x7Cu; if (low) { F[0] = float_at(rt, G[2] + 8u); if (F[12] < F[0]) return; } if (G[3] == 0u) break; F[0] = float_at(rt, G[5] + 4u); } G[2] = 0u; } }
void func_089190BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); auto &F = ctx.fpr; put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = G[5]; G[5] = G[6]; put(rt, frame.at(8u), G[31]); put(rt, frame.at(4u), G[17]); G[17] = G[7]; call_here(rt, ctx, 0x088DF13Cu); G[5] = word(rt, G[2] + 0x18u); F[12] = float_at(rt, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08918FB0u); put_float(rt, G[17], F[0]); F[12] = F[0]; G[4] = G[16]; G[5] = word(rt, G[16] + 0x1F8u); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08918C84u); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_261
} // namespace gt::game

#define HOT261(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_261::name, gt::ReturnKind::kind)
HOT261(0x089010A0u, func_089010A0, Float);
HOT261(0x08930244u, func_08930244, Word);
HOT261(0x0894D5F0u, func_0894D5F0, Word);
HOT261(0x0895FF70u, func_0895FF70, Word);
HOT261(0x0898D854u, func_0898D854, Word);
HOT261(0x089963ECu, func_089963EC, Word);
// HOT261(0x08B1A174u, func_08B1A174, Word);  // no comparable synthetic case yet
HOT261(0x08B52320u, func_08B52320, Word);
HOT261(0x08965C40u, func_08965C40, Word);
HOT261(0x08965C90u, func_08965C90, Word);
// HOT261(0x0892B9DCu, func_0892B9DC, Word);  // no comparable synthetic case yet
HOT261(0x08A6872Cu, func_08A6872C, Word);
HOT261(0x08918C84u, func_08918C84, Word);
// HOT261(0x089190BCu, func_089190BC, Word);  // no comparable synthetic case yet
