// Hot batch 330: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
// Sixteen of them (0x08B797F0, 0x08806FA0, 0x0883F73C, 0x088E4C28, 0x089B5E70, 0x08A15BA8, 0x08AEBB0C, 0x08B9CC90, 0x08C779C8, 0x0882E624, 0x08866DF8,
// 0x08921AF0, 0x089BD694, 0x089DDCC4, 0x089DDEA4, 0x08A9A448) are rewritten as structured C from the disassembly (structured rewrites, batch 79); the others keep their emitter bodies.
// 0x08B81B70 was also rewritten but no run (route or synthetic) ever compared it, so it keeps its emitter body.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_330 {
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
// `trunc.w.s` as the recompiled baseline does it: NaN and +inf give 0x7FFFFFFF, -inf 0x80000000, finite values
// truncate toward zero and clamp to the int32 range.
std::uint32_t trunc_to_word(float value) {
    if (std::isnan(value)) return 0x7FFFFFFFu;
    const double t = std::trunc(static_cast<double>(value));
    if (t >= 2147483647.0) return 0x7FFFFFFFu;
    if (t <= -2147483648.0) return 0x80000000u;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(t));
}
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

void func_08A38540(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); auto &F = ctx.fpr; G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[20] = G[5]; G[19] = G[4]; G[17] = G[7]; put_float(rt, frame.at(0x44u), F[21]); { const bool tk = G[6] != 0u; put_float(rt, frame.at(0x40u), F[20]); if (tk) goto L_08A38664; } G[18] = frame.at(0x14u); G[4] = G[18]; G[5] = G[7]; call_here(rt, ctx, 0x08A85940u); G[16] = word(rt, frame.at(0x14u)); { const bool tk = G[16] != 0u; if (tk) goto L_08A385A0; } G[4] = 0x08CB0000u; G[4] = 0x08CAD294u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A385A0: ; G[4] = G[16]; call_here(rt, ctx, 0x08A855E8u); G[4] = G[18]; G[18] = frame.at(0x10u); F[21] = std::bit_cast<float>(G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[17] + 0x4u; G[4] = G[18]; call_here(rt, ctx, 0x08A85940u); G[16] = word(rt, frame.at(0x10u)); { const bool tk = G[16] != 0u; if (tk) goto L_08A385DC; } G[4] = 0x08CB0000u; G[4] = 0x08CAD294u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A385DC: ; G[4] = G[16]; call_here(rt, ctx, 0x08A855E8u); G[17] = frame.at(0x8u); G[4] = G[18]; F[20] = std::bit_cast<float>(G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[20]; G[4] = G[17]; call_here(rt, ctx, 0x08A38038u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; F[12] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[21]))); if (tk) goto L_08A3861C; } G[4] = 0x08CB0000u; G[4] = 0x08CAD29Cu; call_here(rt, ctx, 0x08A5BB7Cu); F[12] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[21]))); L_08A3861C: ; F[13] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[20]))); G[4] = G[16] + 0x14u; G[16] = frame.at(0xCu); call_here(rt, ctx, 0x08B1EFBCu); F[0] = std::bit_cast<float>(trunc_to_word(F[0])); G[4] = G[16]; G[5] = bits_of(F[0]); call_here(rt, ctx, 0x08A85B1Cu); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0xCu)); if (tk) goto L_08A3864C; } G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); L_08A3864C: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); goto L_08A386C4; L_08A38664: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A38038u); G[16] = word(rt, frame.at(0x0u)); { const bool tk = G[16] != 0u; G[4] = 0x08CB0000u; if (tk) goto L_08A38680; } G[4] = 0x08CAD29Cu; call_here(rt, ctx, 0x08A5BB7Cu); L_08A38680: ; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08B1EEF0u); G[16] = frame.at(0x4u); G[3] = 0u; G[6] = G[2]; G[7] = G[3]; G[4] = G[16]; call_here(rt, ctx, 0x08A83160u); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_08A386B0; } G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); L_08A386B0: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); L_08A386C4: ; F[21] = float_at(rt, frame.at(0x44u)); F[20] = float_at(rt, frame.at(0x40u)); goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08A61018(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x10u), G[16]); G[17] = G[5]; G[21] = 0x08CB0000u; G[20] = G[17] + 0x18u; G[19] = G[4]; G[18] = G[17] + 0x14u; call_here(rt, ctx, 0x08A76AA0u); G[16] = word(rt, G[17] + 0x18u); goto L_08A61190; L_08A61058: ; { const bool tk = G[18] == G[2]; if (tk) goto L_08A61070; } G[5] = word(rt, G[16] + 0x8u); G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); G[16] = word(rt, G[17] + 0x18u); L_08A61070: ; G[4] = G[16]; call_here(rt, ctx, 0x08B5C094u); G[4] = G[16] + 0x8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0xCu; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); G[16] = word(rt, G[17] + 0x14u); { const bool tk = G[16] != 0u; G[4] = G[16]; if (tk) goto L_08A610A8; } G[4] = 0x08CB01E8u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_08A610A8: ; G[5] = 0u; call_here(rt, ctx, 0x08A63F1Cu); G[3] = G[2]; G[2] = 0x1u; if (G[3] == G[2]) { G[16] = word(rt, G[17] + 0x14u); goto L_08A61138; } if (G[3] == 0u) { G[16] = word(rt, G[17] + 0x14u); goto L_08A61160; } G[2] = 0x2u; if (G[3] != G[2]) { G[16] = word(rt, G[17] + 0x18u); goto L_08A61190; } G[4] = 0xCu; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0x8u; { const bool tk = G[4] == 0u; G[16] = G[2]; if (tk) goto L_08A610F4; } G[5] = G[18]; call_here(rt, ctx, 0x08A61BD0u); L_08A610F4: ; G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x08B5C078u); G[16] = word(rt, G[17] + 0x14u); { const bool tk = G[16] != 0u; if (tk) goto L_08A61114; } G[4] = 0x08CB01E8u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A61114: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76AA0u); G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08A61E3Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17] + 0x18u); goto L_08A61190; L_08A61138: ; { const bool tk = G[16] != 0u; if (tk) goto L_08A61148; } G[4] = 0x08CB01E8u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A61148: ; G[4] = G[16]; call_here(rt, ctx, 0x08A6158Cu); if (G[19] != G[2]) { G[5] = word(rt, G[2] + 0x0u); goto L_08A61184; } G[16] = word(rt, G[17] + 0x18u); goto L_08A61190; L_08A61160: ; { const bool tk = G[16] != 0u; if (tk) goto L_08A61170; } G[4] = 0x08CB01E8u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A61170: ; G[4] = G[16]; call_here(rt, ctx, 0x08A615C4u); if (G[19] == G[2]) { G[16] = word(rt, G[17] + 0x18u); goto L_08A61190; } G[5] = word(rt, G[2] + 0x0u); L_08A61184: ; G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); G[16] = word(rt, G[17] + 0x18u); L_08A61190: ; { const bool tk = G[16] != G[20]; G[2] = G[16] + 0x8u; if (tk) goto L_08A61058; } G[2] = G[19]; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08A6EB58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[17] = word(rt, G[5] + 0x0u); { const bool tk = G[17] != 0u; G[18] = G[4]; if (tk) goto L_08A6EB84; } G[4] = 0x08CB0000u; G[4] = 0x08CB06B8u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A6EB84: ; G[2] = word(rt, G[17] + 0x0u); G[16] = frame.at(0x8u); G[5] = G[17]; G[3] = word(rt, G[2] + 0xCu); G[4] = G[16]; { const std::uint32_t tg = G[3]; G[17] = frame.at(0xCu); call_here(rt, ctx, tg); } G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08B88F44u); G[5] = 0x08CB0000u; G[5] = 0x08CB07C4u; G[4] = G[17]; G[6] = 0xFFFFFFFFu; G[7] = 0x1u; call_here(rt, ctx, 0x08C008DCu); G[5] = G[2]; G[2] = 0xFFFFFFFFu; { const bool tk = G[5] == G[2]; G[2] = word(rt, frame.at(0xCu)); if (tk) goto L_08A6EBFC; } G[2] = word(rt, G[2] + 0xFFFFFFF4u); G[3] = G[2] < G[5] ? 1u : 0u; { const bool tk = G[3] != 0u; G[2] = G[2] - G[5]; if (tk) goto L_08A6EC34; } G[3] = G[2] < 0x2u ? 1u : 0u; G[6] = 0x1u; if (G[3] != 0u) G[6] = G[2]; G[4] = G[17]; G[7] = 0u; call_here(rt, ctx, 0x08B8634Cu); L_08A6EBFC: ; G[5] = 0x08CB0000u; G[5] = 0x08CB07C8u; G[4] = G[17]; G[6] = 0xFFFFFFFFu; G[7] = 0x1u; call_here(rt, ctx, 0x08C008DCu); G[5] = G[2]; G[2] = 0xFFFFFFFFu; { const bool tk = G[5] == G[2]; G[2] = word(rt, frame.at(0xCu)); if (tk) goto L_08A6EC58; } G[2] = word(rt, G[2] + 0xFFFFFFF4u); G[3] = G[2] < G[5] ? 1u : 0u; { const bool tk = G[3] == 0u; G[2] = G[2] - G[5]; if (tk) goto L_08A6EC40; } L_08A6EC34: ; G[4] = 0x08CB0000u; G[4] = 0x08CB0740u; call_here(rt, ctx, 0x08B5AEDCu); L_08A6EC40: ; G[3] = G[2] < 0x2u ? 1u : 0u; G[6] = 0x1u; if (G[3] != 0u) G[6] = G[2]; G[4] = G[17]; G[7] = 0u; call_here(rt, ctx, 0x08B8634Cu); L_08A6EC58: ; G[16] = frame.at(0x4u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_08A6EC78; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_08A6EC78: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; if (G[16] == G[17]) { G[4] = word(rt, frame.at(0x8u)); goto L_08A6ECBC; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = word(rt, frame.at(0x8u)); if (tk) goto L_08A6ECBC; } G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, frame.at(0x8u)); L_08A6ECBC: ; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[17] == G[16]; if (tk) goto L_08A6ECEC; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) goto L_08A6ECEC; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_08A6ECEC: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08AC4CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x18u), G[18]); G[21] = G[4]; G[20] = G[5]; G[19] = G[7]; G[17] = G[8]; G[16] = G[6]; { const bool tk = G[8] == 0u; if (tk) goto L_08AC4CF4; } store8(G[8] + 0x0u, 0u); L_08AC4CF4: ; G[5] = frame.at(0u); G[6] = frame.at(0x4u); G[7] = frame.at(0x8u); put(rt, frame.at(0x0u), 0u); G[18] = 0xFFFFFFFFu; put(rt, frame.at(0x4u), 0u); put(rt, frame.at(0x8u), 0u); call_here(rt, ctx, 0x08C7AD00u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[3] = word(rt, frame.at(0x0u)); if (tk) goto L_08AC4E38; } { const bool tk = G[3] == 0u; G[4] = word(rt, frame.at(0x4u)); if (tk) goto L_08AC4E2C; } if (G[4] == 0u) { G[18] = 0u; goto L_08AC4E38; } G[2] = G[3] < G[16] ? 1u : 0u; G[7] = G[19] + G[16]; { const bool tk = G[2] != 0u; G[4] = G[4] + G[3]; if (tk) goto L_08AC4E34; } G[2] = G[3] < G[7] ? 1u : 0u; if (G[2] == 0u) { G[18] = 0xFFFFFFFFu; goto L_08AC4E38; } G[2] = G[4] < G[16] ? 1u : 0u; if (G[2] != 0u) { G[18] = 0xFFFFFFFFu; goto L_08AC4E38; } G[2] = G[7] < G[4] ? 1u : 0u; if (G[2] != 0u) { G[18] = 0xFFFFFFFFu; goto L_08AC4E38; } G[3] = word(rt, G[20] + 0x0u); G[2] = word(rt, G[3] + 0x1Cu); { const std::uint32_t tg = G[2]; G[4] = G[20]; call_here(rt, ctx, tg); } G[8] = word(rt, frame.at(0x8u)); G[6] = word(rt, G[2] + 0xB8u); { const bool tk = static_cast<std::int32_t>(G[8]) < 0; G[7] = word(rt, G[2] + 0xBCu); if (tk) goto L_08AC4E34; } G[9] = sra(G[8], 31); G[2] = G[9] < G[7] ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = word(rt, frame.at(0x4u)); if (tk) goto L_08AC4DA8; } if (G[9] != G[7]) { G[18] = 0xFFFFFFFFu; goto L_08AC4E38; } G[2] = G[8] < G[6] ? 1u : 0u; if (G[2] == 0u) { G[18] = 0xFFFFFFFFu; goto L_08AC4E38; } G[2] = word(rt, frame.at(0x4u)); L_08AC4DA8: ; if (static_cast<std::int32_t>(G[2]) <= 0) { G[18] = 0u; goto L_08AC4E38; } G[3] = word(rt, G[20] + 0x0u); G[6] = G[8]; G[7] = G[9]; G[2] = word(rt, G[3] + 0x14u); G[4] = G[20]; { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[18] = 0xFFFFFFFFu; if (tk) goto L_08AC4E38; } G[3] = word(rt, G[20] + 0x0u); G[5] = word(rt, frame.at(0x0u)); G[6] = word(rt, frame.at(0x4u)); G[2] = word(rt, G[3] + 0x10u); { const std::uint32_t tg = G[2]; G[4] = G[20]; call_here(rt, ctx, tg); } { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[18] = G[2]; if (tk) goto L_08AC4E34; } { const bool tk = G[17] == 0u; G[2] = word(rt, frame.at(0x4u)); if (tk) goto L_08AC4E0C; } G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08AC4E0C; } store8(G[17] + 0x0u, G[2]); L_08AC4E0C: ; { const bool tk = static_cast<std::int32_t>(G[18]) <= 0; G[4] = G[21]; if (tk) goto L_08AC4E38; } G[5] = G[18]; call_here(rt, ctx, 0x08C7ACC8u); { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[2] = G[18]; if (tk) goto L_08AC4E3C; } G[18] = 0xFFFFFFFFu; goto L_08AC4E38; L_08AC4E2C: ; G[18] = 0u; goto L_08AC4E38; L_08AC4E34: ; G[18] = 0xFFFFFFFFu; L_08AC4E38: ; G[2] = G[18]; L_08AC4E3C: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B6065C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x0u), G[16]); G[18] = G[5]; G[3] = word(rt, G[4] + 0x8u); G[2] = word(rt, G[4] + 0xCu); G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] == 0u; G[17] = G[4]; if (tk) goto L_08B607EC; } G[4] = load8(G[3] + 0x0u); G[19] = 0u; L_08B60698: ; G[20] = 0xFFFFFFFFu; L_08B6069C: ; { const bool tk = G[4] == G[20]; G[2] = G[19]; if (tk) goto L_08B60708; } G[3] = word(rt, G[17] + 0xCu); L_08B606A8: ; G[5] = word(rt, G[17] + 0x8u); G[16] = G[3] - G[5]; G[2] = G[16] < 0x2u ? 1u : 0u; if (G[2] != 0u) { G[3] = word(rt, G[18] + 0x14u); goto L_08B60728; } G[3] = word(rt, G[18] + 0x0u); G[4] = G[18]; G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t tg = G[2]; G[6] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x8u); G[4] = G[2] < G[16] ? 1u : 0u; G[19] = G[19] + G[2]; G[3] = G[3] + G[2]; { const bool tk = G[4] != 0u; put(rt, G[17] + 0x8u, G[3]); if (tk) goto L_08B60704; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x24u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[4] = G[2]; if (G[4] != G[20]) { G[3] = word(rt, G[17] + 0xCu); goto L_08B606A8; } L_08B60704: ; G[2] = G[19]; L_08B60708: ; goto exit_; L_08B60728: ; G[2] = word(rt, G[18] + 0x18u); G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] == 0u; G[5] = sext(G[4] & 0xFFu, 8); if (tk) goto L_08B607A0; } store8(G[3] + 0x0u, G[5]); G[5] = G[5] & 0xFFu; G[2] = word(rt, G[18] + 0x14u); G[2] = G[2] + 0x1u; put(rt, G[18] + 0x14u, G[2]); L_08B6074C: ; { const bool tk = G[5] == G[20]; G[2] = G[19]; if (tk) goto L_08B60708; } G[3] = word(rt, G[17] + 0x8u); G[2] = word(rt, G[17] + 0xCu); G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] == 0u; G[19] = G[19] + 0x1u; if (tk) goto L_08B607D4; } G[2] = G[3] + 0x1u; G[4] = load8(G[3] + 0x0u); put(rt, G[17] + 0x8u, G[2]); L_08B60774: ; G[2] = G[4] + 0x1u; { const bool tk = G[2] == 0u; G[5] = 0xFFFFFFFFu; if (tk) goto L_08B60798; } G[3] = word(rt, G[17] + 0x8u); G[2] = word(rt, G[17] + 0xCu); G[2] = G[3] < G[2] ? 1u : 0u; if (G[2] == 0u) { G[3] = word(rt, G[17] + 0x0u); goto L_08B607BC; } G[5] = load8(G[3] + 0x0u); L_08B60798: ; G[4] = G[5]; goto L_08B6069C; L_08B607A0: ; G[3] = word(rt, G[18] + 0x0u); G[5] = G[5] & 0xFFu; G[2] = word(rt, G[3] + 0x34u); { const std::uint32_t tg = G[2]; G[4] = G[18]; call_here(rt, ctx, tg); } G[5] = G[2]; goto L_08B6074C; L_08B607BC: ; G[2] = word(rt, G[3] + 0x24u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[5] = G[2]; G[4] = G[5]; goto L_08B6069C; L_08B607D4: ; G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x28u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[4] = G[2]; goto L_08B60774; L_08B607EC: ; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x24u); { const std::uint32_t tg = G[2]; G[19] = 0u; call_here(rt, ctx, tg); } G[4] = G[2]; goto L_08B60698;  exit_: ; G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B797F0 (a0 = the stream object; v0 = the flags it leaves or 0x400 / 0 / the flags | 1): flags are the halfword at +0xC (2 = the buffer is the object's own
// 0x43-byte area: fields +0x14 = 1, +0x10 and +0 = object + 0x43, v0 = 1). Otherwise the descriptor at +0xE (negative = none) is statted through 0x08B78EC8(owner,
// descriptor, frame) with owner from 0x08B796C4 (0x800 is set in the flags; a regular file (mode 0x8000) with the default write hook 0x08B7B804 at +0x28 sets +0x4C = 0x400
// and 0x400 in the flags instead; a character device (0x2000) is "interactive"). A 0x400-byte buffer comes from 0x08B35DAC(owner, 0x400): none = the small area (flags | 2,
// v0 = those flags); one = installed at +0x10 / +0 with size 0x400 at +0x14, flag 0x80, the owner's word +0x3C = 0x08B785A4, and for an interactive device
// 0x08B779E8(descriptor) decides flag 1; v0 = 0x400, 0 or the flags | 1.
void func_08B797F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x40u}, {31u, 0x4Cu}, {18u, 0x48u}, {17u, 0x44u}});
    const auto flags = [&]() -> u32 { return load16(stream + 0xCu); };
    const auto set_flags = [&](u32 value) { store16(stream + 0xCu, static_cast<std::uint16_t>(value)); };
    const auto small_area = [&](u32 value) {
        put(rt, stream + 0x14u, 1u);
        put(rt, stream + 0x10u, stream + 0x43u);
        put(rt, stream, stream + 0x43u);
        ctx.gpr[2] = value;
    };
    if ((flags() & 2u) != 0u) {
        small_area(1u);
        return;
    }
    const u32 descriptor = signed_half(rt, stream + 0xEu);
    u32 interactive = 0u;
    bool regular_file = false;
    if (static_cast<s32>(descriptor) >= 0) {
        const u32 owner = call_set(rt, ctx, 0x08B796C4u, {});
        const u32 status = call_set(rt, ctx, 0x08B78EC8u, {owner, signed_half(rt, stream + 0xEu), frame.at(0u)});
        if (static_cast<s32>(status) >= 0) {
            const u32 mode = word(rt, frame.at(4u)) & 0xF000u;
            interactive = mode == 0x2000u ? 1u : 0u;
            regular_file = mode == 0x8000u;
        }
    }
    if (regular_file && word(rt, stream + 0x28u) == 0x08B7B804u) {
        put(rt, stream + 0x4Cu, 0x400u);
        set_flags(flags() | 0x400u);
    } else {
        set_flags(flags() | 0x800u);
    }
    const u32 owner = call_set(rt, ctx, 0x08B796C4u, {});
    const u32 buffer = call_set(rt, ctx, 0x08B35DACu, {owner, 0x400u});
    if (buffer == 0u) {
        const u32 value = flags() | 2u;
        set_flags(value);
        small_area(value);
        return;
    }
    const u32 owner_again = call_set(rt, ctx, 0x08B796C4u, {});
    const u32 value = flags() | 0x80u;
    put(rt, owner_again + 0x3Cu, 0x08B785A4u);
    ctx.gpr[2] = 0x400u;
    set_flags(value);
    put(rt, stream + 0x10u, buffer);
    put(rt, stream + 0x14u, 0x400u);
    put(rt, stream, buffer);
    if (interactive == 0u) return;
    if (call_set(rt, ctx, 0x08B779E8u, {signed_half(rt, stream + 0xEu)}) == 0u) return;
    set_flags(flags() | 1u);
    ctx.gpr[2] = flags();
}
void func_08C28918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x10u), G[16]); G[21] = G[6]; G[19] = G[4] + 0x4u; G[18] = G[7]; G[17] = G[4]; { const bool tk = G[5] != 0u; if (tk) goto L_08C28A3C; } { const bool tk = G[6] == G[19]; G[4] = frame.at(0u); if (tk) goto L_08C28A3C; } G[23] = frame.at(0x4u); G[22] = word(rt, G[17] + 0x0u); G[20] = G[21] + 0x10u; call_here(rt, ctx, 0x08A76AA0u); G[16] = G[23]; G[30] = frame.at(0xCu); G[4] = G[16]; L_08C2897C: ; G[16] = G[16] + 0x4u; call_here(rt, ctx, 0x08A76AA0u); { const bool tk = G[16] != G[30]; G[4] = G[16]; if (tk) goto L_08C2897C; } { const bool tk = G[23] == G[18]; G[4] = frame.at(0x8u); if (tk) goto L_08C289A4; } G[5] = word(rt, G[18] + 0x0u); G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); G[4] = frame.at(0x8u); L_08C289A4: ; if (G[4] == G[20]) { G[16] = word(rt, G[22] + 0x2Cu); goto L_08C289B8; } G[5] = word(rt, G[20] + 0x0u); call_here(rt, ctx, 0x08A5BAD8u); G[16] = word(rt, G[22] + 0x2Cu); L_08C289B8: ; if (G[16] != 0u) { G[2] = word(rt, G[16] + 0x0u); goto L_08C289D0; } G[4] = 0x08CB0000u; G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x0u); L_08C289D0: ; G[4] = G[16]; G[5] = frame.at(0u); G[3] = word(rt, G[2] + 0xCCu); G[6] = 0x2u; { const std::uint32_t tg = G[3]; G[7] = G[23]; call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x0u)); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08C28A04; } G[4] = 0x08CB0000u; G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08C28A04: ; G[4] = G[16]; G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t tg = G[2]; G[16] = G[30]; call_here(rt, ctx, tg); } G[22] = G[2]; G[16] = G[16] + 0xFFFFFFFCu; L_08C28A1C: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); if (G[16] != G[23]) { G[16] = G[16] + 0xFFFFFFFCu; goto L_08C28A1C; } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = G[22] == 0u; G[20] = 0u; if (tk) goto L_08C28A40; } L_08C28A3C: ; G[20] = 0x1u; L_08C28A40: ; G[4] = 0x18u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0x10u; { const bool tk = G[4] == 0u; G[16] = G[2]; if (tk) goto L_08C28A6C; } G[5] = G[18]; call_here(rt, ctx, 0x08A76ABCu); G[5] = G[18] + 0x4u; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08A76ABCu); L_08C28A6C: ; G[4] = G[20]; G[6] = G[21]; G[7] = G[19]; G[5] = G[16]; call_here(rt, ctx, 0x08B5F2B8u); G[3] = word(rt, G[17] + 0x14u); G[2] = G[16]; G[3] = G[3] + 0x1u; put(rt, G[17] + 0x14u, G[3]); goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08806FA0 (a0 = destination string, a1 = key, a2 = count, a3 = pointer to a holder; v0 = 0 for an index of 90 or more, the negative index, or the last release's v0):
// with a positive count the holder's object (null is reported through 0x08A5BB7C and then used anyway) answers virtual +0x50; 0x08930204(that, the answer's v1) gives an
// index (negative: nothing, 90 or more: v0 = 0). Otherwise the key is wrapped by 0x08A57854, the wrapped object's virtual +0x9C is called with a new 0x18-byte object
// (vtable 0x08CB6B88, a string at +0x14 from 0x08A6972C) and the object's virtual +0x84 turns the result and the index into a string that replaces the destination
// (0x08A5BAD8 unless it already is the destination); the four temporaries are released with 0x08A5B9EC.
void func_08806FA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x34u}, {17u, 0x24u}, {31u, 0x40u}, {23u, 0x3Cu}, {22u, 0x38u}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {16u, 0x20u}});
    if (static_cast<s32>(ctx.gpr[6]) <= 0) return;
    const auto checked = [&](u32 object, u32 message) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    };
    const u32 holder = word(rt, ctx.gpr[7]);
    checked(holder, 0x08C97DA8u);
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x50u), {holder});
    const u32 index = call_set(rt, ctx, 0x08930204u, {ctx.gpr[2], ctx.gpr[3]});
    if (static_cast<s32>(index) < 0) return;
    const bool in_range = static_cast<s32>(index) < 90;
    ctx.gpr[2] = in_range ? 1u : 0u;
    if (!in_range) return;
    const u32 wrapped = frame.at(0xCu);
    call_set(rt, ctx, 0x08A57854u, {wrapped, key});
    const u32 target = word(rt, wrapped);
    checked(target, 0x08C97DD8u);
    const u32 method = word(rt, word(rt, target) + 0x9Cu);
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08C97E48u});
    const u32 object = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call_set(rt, ctx, 0x08A76ACCu, {object});
    put(rt, object, 0x08CB6B88u);
    const u32 pair = frame.at(0x8u);
    call_set(rt, ctx, 0x08A6972Cu, {object + 0x14u, frame.at(0u)});
    put(rt, frame.at(0x4u), object);
    call_set(rt, ctx, 0x08A76AB4u, {pair, frame.at(0x4u)});
    const u32 result = frame.at(0x10u);
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    call_indirect(rt, ctx, method, {result, target, pair});
    const u32 produced = word(rt, result);
    checked(produced, 0x08C97DA8u);
    const u32 text = frame.at(0x14u);
    call_indirect(rt, ctx, word(rt, word(rt, produced) + 0x84u), {text, produced, index});
    if (destination != text) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, text)});
    call_set(rt, ctx, 0x08A5B9ECu, {text});
    call_set(rt, ctx, 0x08A5B9ECu, {result});
    call_set(rt, ctx, 0x08A5B9ECu, {pair});
    call_set(rt, ctx, 0x08A5B9ECu, {wrapped});
}
// 0x0883F73C (a0 = destination string, a1 = count, a2 = pointer to a pair of holders; v0 from the last call, unchanged for a count below 2): the first holder's
// object is compared (0x08A76F20) with the 0x08A6BF34 object. Equal: its virtual +0xC fills a string whose 0x08930308 value is the key, the string's buffer released
// (0x08B60804(buffer - 4, -1) not positive: 0x08B8542C(buffer - 0xC, frame) unless it is the shared empty one 0x08D74FD0); different: the holder's virtual +0x50 gives the key
// (v0 / v1). 0x08931B74(state) starts a lookup of that key with 0x08931D70; when it finds one the second holder's virtual +0x3C result is formatted with 0x0883F508 into a
// string that replaces the destination (0x08A5BAD8 unless it already is the destination), released with 0x08A5B9EC; the state is closed with 0x08931C38 either way.
void func_0883F73C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 holders = ctx.gpr[6];
    const bool small = static_cast<s32>(ctx.gpr[5]) < 2;
    ctx.gpr[5] = small ? 1u : 0u;
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x40u}, {19u, 0x3Cu}, {31u, 0x44u}, {18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    if (small) return;
    const auto checked = [&](u32 object) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    };
    u32 first = word(rt, holders);
    checked(first);
    const u32 reference = call_set(rt, ctx, 0x08A6BF34u, {});
    u32 key;
    u32 key_extra;
    if (call_set(rt, ctx, 0x08A76F20u, {first, reference}) != 0u) {
        first = word(rt, holders);
        checked(first);
        call_indirect(rt, ctx, word(rt, word(rt, first) + 0xCu), {frame.at(0x8u), first});
        key = call_set(rt, ctx, 0x08930308u, {word(rt, frame.at(0x8u))});
        key_extra = ctx.gpr[3];
        const u32 buffer = word(rt, frame.at(0x8u));
        const u32 shared = buffer - 0xCu;
        if (shared != 0x08D74FD0u && static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu})) <= 0)
            call_set(rt, ctx, 0x08B8542Cu, {shared, frame.at(0u)});
    } else {
        first = word(rt, holders);
        checked(first);
        call_indirect(rt, ctx, word(rt, word(rt, first) + 0x50u), {first});
        key = ctx.gpr[2];
        key_extra = ctx.gpr[3];
    }
    const u32 state = frame.at(0x10u);
    call_set(rt, ctx, 0x08931B74u, {state});
    if (call_set(rt, ctx, 0x08931D70u, {state, ctx.gpr[5], key, key_extra}) == 0u) {
        call_set(rt, ctx, 0x08931C38u, {state});
        return;
    }
    const u32 second = word(rt, holders + 4u);
    checked(second);
    call_indirect(rt, ctx, word(rt, word(rt, second) + 0x3Cu), {second});
    const u32 text = frame.at(0x4u);
    call_set(rt, ctx, 0x0883F508u, {text, state, ctx.gpr[2]});
    if (destination != text) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, text)});
    call_set(rt, ctx, 0x08A5B9ECu, {text});
    call_set(rt, ctx, 0x08931C38u, {state});
}
// 0x088E4C28 (a0 = the owner, a1 / t0 = the halfwords stored at +0 / +2, a2:a3 = the 64-bit key (low:high), t1 = 16 words of payload; t0 = -1 does nothing; ends with
// the tail call 0x08B3457C(table + 0x1C8) after the table is changed): the owner's table at +0x128 holds a 0x50-byte record (halfword -1 = free; its key at +8 / +0xC) and
// a second record at +0x178. The first record is replaced when it is free or when its key differs from the new one and the owner's byte +0x52A equals "the
// stored key is smaller" (a smaller or equal stored key is replaced only in that ordering); after 0x08B344E0(table + 0x1C8) the second record is made the copy of the first
// (with the flag bytes +0x528 / +0x529 = 1) when it is free or the ordering byte differs from "the second record's key is greater than the new one".
void func_088E4C28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner_holder = ctx.gpr[4];
    const u32 half = ctx.gpr[5];
    const u32 low = ctx.gpr[6];
    const u32 high = ctx.gpr[7];
    const u32 mode = ctx.gpr[8];
    const u32 payload = ctx.gpr[9];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x14u}, {20u, 0x10u}, {18u, 0x8u}});
    if (mode == 0xFFFFFFFFu) return;
    const u32 owner = word(rt, owner_holder);
    const u32 first = owner + 0x128u;
    const u32 second = owner + 0x178u;
    // v0 as the original's `sltu` pair leaves it: a smaller high word, else whether the low word is smaller.
    const auto stored_smaller = [&](u32 stored_low, u32 stored_high) {
        ctx.gpr[2] = stored_high < high ? 1u : (stored_low < low ? 1u : 0u);
        return stored_high < high || (stored_high == high && stored_low < low);
    };
    if (signed_half(rt, first) != 0xFFFFFFFFu) {
        const u32 stored_low = word(rt, first + 8u);
        const u32 stored_high = word(rt, first + 0xCu);
        if (stored_low == low && stored_high == high) return;
        const u32 ordering = load8(owner + 0x52Au);
        if (ordering != (stored_smaller(stored_low, stored_high) ? 1u : 0u)) return;
    }
    store16(first, static_cast<std::uint16_t>(half));
    store16(first + 2u, static_cast<std::uint16_t>(mode));
    put(rt, first + 4u, 0u);
    put(rt, first + 8u, low);
    put(rt, first + 0xCu, high);
    for (u32 i = 0; i < 16u; ++i) put(rt, first + 0x10u + 4u * i, word(rt, payload + 4u * i));
    call_set(rt, ctx, 0x08B344E0u, {word(rt, owner_holder) + 0x1C8u});
    u32 table = word(rt, owner_holder);
    bool copy = true;
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (signed_half(rt, second) != 0xFFFFFFFFu) {
        const u32 stored_low = word(rt, second + 8u);
        const u32 stored_high = word(rt, second + 0xCu);
        if (stored_low == low && stored_high == high) {
            copy = false;
        } else {
            table = word(rt, owner_holder);
            const u32 ordering = load8(table + 0x52Au);
            ctx.gpr[2] = high < stored_high ? 1u : (low < stored_low ? 1u : 0u);
            const bool greater = high < stored_high || (high == stored_high && low < stored_low);
            if (ordering == (greater ? 1u : 0u)) copy = false;
        }
    }
    if (copy) {
        for (u32 i = 0; i < 20u; ++i) put(rt, second + 4u * i, word(rt, first + 4u * i));
        store8(word(rt, owner_holder) + 0x528u, 1u);
        store8(word(rt, owner_holder) + 0x529u, 1u);
        ctx.gpr[2] = word(rt, owner_holder);
        table = word(rt, owner_holder);
    }
    ctx.gpr[4] = table + 0x1C8u;
    jump(ctx, 0x08B3457Cu);
}
// 0x089B5E70 (a0 = the object, a1 = the step; v0 = 1 when the cursor moved to the position a search found, 0 when it was restored, or the plain 0x08B2068C result): without
// the object's cursor (+0x124 zero or the byte +0x158 zero) the list at +0x138 simply advances with 0x08B2068C(list, step). Otherwise, when it advances, the next position
// is searched forward (0x089B5BD8(object, old head) for a positive step, 0x089B5B3C(object) otherwise): a position (not negative) is taken with 0x08B207A4(list, position),
// else the old head is put back. Then 0x089B5E10 gives a node and 0x08969B98 its bounds; the scroll pair at +0x168 / +0x174 (the zero word at +0x168 and +0x16C,
// the new offset in +0x170 / +0x174) is updated when the node's top lies below the window start (+0x160) or its bottom beyond the window end (+0x164).
void func_089B5E70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 step = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x24u}, {16u, 0x20u}, {31u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}});
    const u32 list = object + 0x138u;
    if (word(rt, object + 0x124u) == 0u || load8(object + 0x158u) == 0u) {
        ctx.gpr[4] = list;
        ctx.gpr[2] = call_set(rt, ctx, 0x08B2068Cu, {list, step});
        return;
    }
    const u32 old_head = word(rt, object + 0x138u);
    u32 moved = 0u;
    if (call_set(rt, ctx, 0x08B2068Cu, {list, ctx.gpr[5]}) != 0u) {
        u32 position;
        if (static_cast<s32>(step) > 0) position = call_set(rt, ctx, 0x089B5BD8u, {object, word(rt, object + 0x138u)});
        else position = call_set(rt, ctx, 0x089B5B3Cu, {object, word(rt, object + 0x138u)});
        if (static_cast<s32>(position) >= 0) {
            call_set(rt, ctx, 0x08B207A4u, {list, position, ctx.gpr[6]});
            moved = 1u;
        } else {
            call_set(rt, ctx, 0x08B207A4u, {list, old_head, ctx.gpr[6]});
        }
    }
    ctx.gpr[2] = moved;
    const u32 node = call_set(rt, ctx, 0x089B5E10u, {object});
    if (node == 0u) {
        ctx.gpr[2] = moved;
        return;
    }
    const auto bounds = [&](u32 target, u32 at) {
        call_set(rt, ctx, 0x08969B98u, {target, frame.at(at), frame.at(at + 4u), frame.at(at + 8u), frame.at(at + 0xCu)});
    };
    const u32 scroll = object + 0x168u;
    bounds(node, 0x0u);
    const float top = float_at(rt, frame.at(0x4u));
    const float window_start = float_at(rt, object + 0x160u);
    const float bottom = float_at(rt, frame.at(0xCu));
    if (top < window_start) {
        bounds(word(rt, object + 0x120u), 0x10u);
        const float other = float_at(rt, frame.at(0x14u));
        put(rt, scroll + 4u, 0u);
        put_float(rt, scroll + 8u, other);
        const float start_again = float_at(rt, object + 0x160u);
        put(rt, object + 0x168u, 0u);
        put_float(rt, object + 0x174u, (start_again - float_at(rt, frame.at(0x4u))) + other);
    } else if (float_at(rt, object + 0x164u) < top + bottom) {
        ctx.gpr[2] = moved;
        bounds(word(rt, object + 0x120u), 0x10u);
        const float height = float_at(rt, frame.at(0xCu));
        const float other = float_at(rt, frame.at(0x14u));
        const float node_top = float_at(rt, frame.at(0x4u));
        put_float(rt, scroll + 8u, other);
        put(rt, scroll + 4u, 0u);
        const float window_end = float_at(rt, object + 0x164u);
        put(rt, object + 0x168u, 0u);
        put_float(rt, object + 0x174u, other - ((node_top + height) - window_end));
    }
    ctx.gpr[2] = moved;
}
// 0x08A15BA8 (a1 / a2 = pointers to two entries; v0 = 1 when the first sorts before the second, 0 otherwise): the owner of each entry (0x08965CE0 of its first word) is
// compared. The same owner: the signed words at +0x28, then the unsigned words at +0. Different owners: the bounds of the owners (0x08969B98 into top / left pairs) are
// compared top first (frame +4 against +0xC), then left (+0 against +8); a tie there repeats the comparison with the bounds of the entries themselves and ends on the
// unsigned words at +0. Equal floats (or NaN) fall to the next key.
void func_08A15BA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x28u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {21u, 0x24u}, {20u, 0x20u}});
    const u32 owner_first = call_set(rt, ctx, 0x08965CE0u, {word(rt, first)});
    const u32 owner_second = call_set(rt, ctx, 0x08965CE0u, {word(rt, second)});
    const auto by_word = [&]() -> u32 { return word(rt, first) < word(rt, second) ? 1u : 0u; };
    if (owner_first == owner_second) {
        const s32 a = static_cast<s32>(word(rt, first + 0x28u));
        const s32 b = static_cast<s32>(word(rt, second + 0x28u));
        if (a < b) { ctx.gpr[2] = 1u; return; }
        if (b < a) { ctx.gpr[2] = 0u; return; }
        ctx.gpr[2] = by_word();
        return;
    }
    const u32 near_pair = frame.at(0u);   // top at +4, left at +0
    const u32 far_pair = frame.at(8u);
    const auto bounds = [&](u32 target, u32 at) { call_set(rt, ctx, 0x08969B98u, {target, at, at + 4u, 0u, 0u}); };
    // Both rounds compare the same four floats; `final` says what a tie on all of them answers.
    const auto compare = [&](bool final) -> bool {
        const float top_a = float_at(rt, frame.at(0x4u));
        const float top_b = float_at(rt, frame.at(0xCu));
        if (top_a < top_b) { ctx.gpr[2] = 1u; return true; }
        if (top_b < top_a) { ctx.gpr[2] = 0u; return true; }
        const float left_a = float_at(rt, frame.at(0u));
        const float left_b = float_at(rt, frame.at(0x8u));
        if (left_a < left_b) { ctx.gpr[2] = 1u; return true; }
        if (left_b < left_a) { ctx.gpr[2] = 0u; return true; }
        if (final) ctx.gpr[2] = by_word();
        else ctx.gpr[2] = 0u;
        return final;
    };
    bounds(owner_first, near_pair);
    bounds(owner_second, far_pair);
    if (compare(false)) return;
    bounds(word(rt, first), near_pair);
    bounds(word(rt, second), far_pair);
    compare(true);
}
// 0x08AEBB0C (a0 = the task record, a1 / a2 / a3 / t0 / t1 / t2 = fields +4 / +8 / +0x14 / +0x10 / +0x18 / +0x1C; v0 = 1 when the thread started, 0 otherwise): the record gets
// its fields and +0xC = -1; a buffer of t0 * 8 + 0x40 bytes (0x08AD84B4(size, 1)) goes to +0x20 with its 64-aligned end at +0x24, and a thread is created with
// 0x08C7AA18(name 0x08C91FC8, entry 0x08AEBD10, a2, the priority word 0x08D41B64, 0, 0) into +0xC and started with 0x08C7A9D0(thread, 0x28, record): started sets +0 = 1 and
// answers 1. Each failure (no buffer, no thread, no start) reports through 0x08AD83B8 when the debug flag word 0x08CDFB3C has bit 0 (the file, the line 0x42A / 0x440 / 0x43A
// and the message differ), frees the buffer with 0x08AD85C8 when there is one (clearing +0x20 / +0x24) and deletes a created thread with 0x08C7AAA0 (+0xC = -1), answering 0.
void func_08AEBB0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    const u32 name_argument = ctx.gpr[6];
    const u32 count = ctx.gpr[8];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {17u, 0x4u}, {31u, 0x8u}});
    put(rt, record + 4u, ctx.gpr[5]);
    put(rt, record + 0xCu, 0xFFFFFFFFu);
    put(rt, record + 0x14u, ctx.gpr[7]);
    put(rt, record + 0x18u, ctx.gpr[9]);
    put(rt, record + 0x1Cu, ctx.gpr[10]);
    put(rt, record + 8u, name_argument);
    put(rt, record + 0x10u, count);
    const auto report = [&](u32 line, u32 message) {
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91F50u, line, message, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
    };
    const u32 buffer = call_set(rt, ctx, 0x08AD84B4u, {(count << 3) + 0x40u, 1u});
    put(rt, record + 0x20u, buffer);
    if (buffer != 0u) {
        put(rt, record + 0x24u, (buffer + 0x3Fu) & ~0x3Fu);
        const u32 thread = call_set(rt, ctx, 0x08C7AA18u, {0x08C91FC8u, 0x08AEBD10u, name_argument, word(rt, 0x08D41B64u), 0u, 0u});
        put(rt, record + 0xCu, thread);
        if (static_cast<s32>(thread) < 0) {
            report(0x43Au, 0x08C91FF4u);
        } else {
            if (call_set(rt, ctx, 0x08C7A9D0u, {thread, 0x28u, record}) == 0u) {
                put(rt, record, 1u);
                ctx.gpr[3] = 1u;
                ctx.gpr[2] = 1u;
                return;
            }
            report(0x440u, 0x08C91FD8u);
        }
    } else {
        report(0x42Au, 0x08C91FACu);
    }
    if (word(rt, record + 0x20u) != 0u) {
        call_set(rt, ctx, 0x08AD85C8u, {word(rt, record + 0x20u)});
        put(rt, record + 0x24u, 0u);
        put(rt, record + 0x20u, 0u);
    }
    const u32 thread = word(rt, record + 0xCu);
    if (static_cast<s32>(thread) >= 0) {
        call_set(rt, ctx, 0x08C7AAA0u, {thread});
        put(rt, record + 0xCu, 0xFFFFFFFFu);
    }
    ctx.gpr[3] = 0u;
    ctx.gpr[2] = 0u;
}
void func_08B81B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); G[16] = G[5]; G[5] = G[6]; G[18] = G[4]; G[4] = G[16]; G[17] = G[6]; call_here(rt, ctx, 0x08B81B0Cu); { const bool tk = G[2] == 0u; G[4] = G[18]; if (tk) goto L_08B81CD0; } { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[19] = 0u; if (tk) goto L_08B81D08; } L_08B81BB0: ; G[5] = word(rt, G[16] + 0x4u); G[4] = G[18]; call_here(rt, ctx, 0x08B82260u); G[12] = G[2]; G[10] = word(rt, G[16] + 0x10u); G[2] = word(rt, G[17] + 0x10u); G[8] = G[16] + 0x14u; G[3] = G[10] << 2; G[2] = G[2] << 2; G[6] = G[17] + 0x14u; put(rt, G[12] + 0xCu, G[19]); G[13] = G[8] + G[3]; G[11] = G[6] + G[2]; G[7] = G[12] + 0x14u; G[9] = 0u; L_08B81BEC: ; G[2] = word(rt, G[8] + 0x0u); G[4] = word(rt, G[6] + 0x0u); G[6] = G[6] + 0x4u; G[5] = G[2] & 0xFFFFu; G[3] = G[4] & 0xFFFFu; G[5] = G[5] - G[3]; G[5] = G[5] + G[9]; G[4] = G[4] >> 16; G[2] = G[2] >> 16; G[2] = G[2] - G[4]; G[3] = sra(G[5], 16); G[2] = G[2] + G[3]; G[4] = G[6] < G[11] ? 1u : 0u; store16(G[7] + 0x2u, G[2]); G[8] = G[8] + 0x4u; G[9] = sra(G[2], 16); store16(G[7] + 0x0u, G[5]); { const bool tk = G[4] != 0u; G[7] = G[7] + 0x4u; if (tk) goto L_08B81BEC; } G[2] = G[8] < G[13] ? 1u : 0u; if (G[2] == 0u) { G[2] = word(rt, G[7] + 0xFFFFFFFCu); goto L_08B81C7C; } L_08B81C44: ; G[2] = word(rt, G[8] + 0x0u); G[8] = G[8] + 0x4u; G[5] = G[8] < G[13] ? 1u : 0u; G[4] = G[2] & 0xFFFFu; G[4] = G[4] + G[9]; G[2] = G[2] >> 16; G[3] = sra(G[4], 16); G[2] = G[2] + G[3]; store16(G[7] + 0x2u, G[2]); G[9] = sra(G[2], 16); store16(G[7] + 0x0u, G[4]); { const bool tk = G[5] != 0u; G[7] = G[7] + 0x4u; if (tk) goto L_08B81C44; } G[2] = word(rt, G[7] + 0xFFFFFFFCu); L_08B81C7C: ; { const bool tk = G[2] == 0u; G[3] = G[7] + 0xFFFFFFFCu; if (tk) goto L_08B81CA8; } L_08B81C84: ; put(rt, G[12] + 0x10u, G[10]); L_08B81C88: ; G[2] = G[12]; goto exit_; L_08B81CA8: ; G[3] = G[3] + 0xFFFFFFFCu; G[2] = word(rt, G[3] + 0x0u); { const bool tk = G[2] != 0u; G[10] = G[10] + 0xFFFFFFFFu; if (tk) goto L_08B81C84; } G[3] = G[3] + 0xFFFFFFFCu; G[2] = word(rt, G[3] + 0x0u); { const bool tk = G[2] == 0u; G[10] = G[10] + 0xFFFFFFFFu; if (tk) goto L_08B81CA8; } put(rt, G[12] + 0x10u, G[10]); goto L_08B81C88; L_08B81CD0: ; G[5] = 0u; call_here(rt, ctx, 0x08B82260u); G[12] = G[2]; G[2] = 0x1u; put(rt, G[12] + 0x10u, G[2]); G[2] = G[12]; put(rt, G[12] + 0x14u, 0u); goto exit_; L_08B81D08: ; G[2] = G[17]; G[19] = 0x1u; G[17] = G[16]; G[16] = G[2]; goto L_08B81BB0;  exit_: ; G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[19] = word(rt, frame.at(0xCu)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B9CC90 (a0 = the object, a1 = the string argument; v0 = 0 or the last virtual's result): 0x08B9BD68 gives the source (none: return) and 0x08B19154(list, source) fills a
// list on the frame (+0x34, its count at +0x38). For each index the object's virtual +0xC is asked (object, a slot word on the frame, the word at +8 of the list's
// element holder +0x40); an answer makes a string from the argument (0x08B9CC58, 0x08B196B8) and a second string through the holder's virtual +0xC and that object's virtual +0xC
// (0x08B18A58 / 0x08B18BF0 combine them; every temporary goes through 0x08B18A98). The list's cursor 0x08B18CEC then decides: more elements release the slot word through
// its virtual +4 (when set) and go on; none releases it the same way and ends.
void func_08B9CC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x5Cu}, {17u, 0x54u}, {31u, 0x70u}, {23u, 0x6Cu}, {22u, 0x68u}, {21u, 0x64u}, {20u, 0x60u}, {18u, 0x58u}, {16u, 0x50u}});
    const u32 source = call_set(rt, ctx, 0x08B9BD68u, {});
    if (source == 0u) return;
    const u32 list = frame.at(0x34u);
    const u32 slot = frame.at(0u);
    const u32 text = frame.at(0x10u);
    const u32 joined = frame.at(0x1Cu);
    const u32 piece = frame.at(0x4u);
    const u32 combined = frame.at(0x28u);
    call_set(rt, ctx, 0x08B19154u, {list, source});
    const u32 count = word(rt, frame.at(0x38u));
    ctx.gpr[2] = count;
    if (static_cast<s32>(count) <= 0) return;
    const auto release_slot = [&](u32 value) { call_indirect(rt, ctx, word(rt, word(rt, value) + 4u), {value}); };
    for (s32 index = 0;;) {
        const u32 vtable = word(rt, object);
        const u32 holder = word(rt, frame.at(0x40u));
        put(rt, slot, 0u);
        const u32 holder_word = word(rt, holder + 8u);
        const u32 found = call_indirect(rt, ctx, word(rt, vtable + 0xCu), {object, slot, holder_word});
        if (found != 0u) {
            call_set(rt, ctx, 0x08B9CC58u, {text, argument});
            call_set(rt, ctx, 0x08B196B8u, {joined, text, word(rt, word(rt, frame.at(0x40u)) + 8u), 0u});
            call_set(rt, ctx, 0x08B18A98u, {text});
            const u32 element = word(rt, frame.at(0x40u));
            const u32 first = call_indirect(rt, ctx, word(rt, word(rt, element) + 0xCu), {element});
            call_indirect(rt, ctx, word(rt, word(rt, first) + 0xCu), {piece, first, slot});
            call_set(rt, ctx, 0x08B18A58u, {combined, piece});
            call_set(rt, ctx, 0x08B18A98u, {piece});
            call_set(rt, ctx, 0x08B18BF0u, {joined, combined});
            call_set(rt, ctx, 0x08B18A98u, {combined});
            call_set(rt, ctx, 0x08B18A98u, {joined});
        }
        if (call_set(rt, ctx, 0x08B18CECu, {list}) == 0u) {
            const u32 value = word(rt, slot);
            if (value != 0u) release_slot(value);
            return;
        }
        const u32 value = word(rt, slot);
        if (value != 0u) release_slot(value);
        ++index;
        const u32 limit = word(rt, frame.at(0x38u));
        ctx.gpr[2] = index < static_cast<s32>(limit) ? 1u : 0u;
        if (index >= static_cast<s32>(limit)) return;
    }
}
void func_08C252FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[21] = G[5]; G[19] = G[4]; { const bool tk = G[5] == G[4]; if (tk) goto L_08C2547C; } G[2] = word(rt, G[4] + 0x8u); G[18] = word(rt, G[5] + 0x4u); G[16] = word(rt, G[5] + 0x0u); G[4] = word(rt, G[4] + 0x0u); G[3] = G[18] - G[16]; G[2] = G[2] - G[4]; G[20] = sra(G[3], 2); G[2] = sra(G[2], 2); G[2] = G[2] < G[20] ? 1u : 0u; if (G[2] == 0u) { G[6] = word(rt, G[19] + 0x4u); goto L_08C253C4; } G[20] = G[20] << 2; G[4] = G[20]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[6] = G[2]; G[4] = G[16]; G[5] = G[18]; G[7] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C250A0u); G[4] = word(rt, G[19] + 0x0u); G[5] = word(rt, G[19] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08C24F04u); G[4] = word(rt, G[19] + 0x0u); G[2] = word(rt, G[19] + 0x8u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (tk) goto L_08C253B0; } G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C253B0: ; G[2] = G[17] + G[20]; G[3] = G[20]; put(rt, G[19] + 0x8u, G[2]); put(rt, G[19] + 0x0u, G[17]); goto L_08C25470; L_08C253C4: ; G[2] = G[6] - G[4]; G[2] = sra(G[2], 2); G[3] = G[2] < G[20] ? 1u : 0u; { const bool tk = G[3] != 0u; G[22] = G[2]; if (tk) goto L_08C2541C; } { const bool tk = static_cast<std::int32_t>(G[20]) <= 0; G[17] = G[4]; if (tk) goto L_08C25404; } G[18] = 0u; L_08C253E4: ; G[4] = G[17]; G[5] = G[16]; G[18] = G[18] + 0x1u; call_here(rt, ctx, 0x08A6A23Cu); G[16] = G[16] + 0x4u; { const bool tk = G[20] != G[18]; G[17] = G[17] + 0x4u; if (tk) goto L_08C253E4; } G[6] = word(rt, G[19] + 0x4u); L_08C25404: ; G[5] = G[6]; G[4] = G[17]; G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08C24F98u); G[3] = G[20] << 2; goto L_08C25470; L_08C2541C: ; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[17] = G[4]; if (tk) goto L_08C25454; } G[18] = 0u; L_08C25428: ; G[4] = G[17]; G[5] = G[16]; G[18] = G[18] + 0x1u; call_here(rt, ctx, 0x08A6A23Cu); G[16] = G[16] + 0x4u; { const bool tk = G[22] != G[18]; G[17] = G[17] + 0x4u; if (tk) goto L_08C25428; } G[16] = word(rt, G[21] + 0x0u); G[18] = word(rt, G[21] + 0x4u); G[4] = word(rt, G[19] + 0x0u); G[6] = word(rt, G[19] + 0x4u); L_08C25454: ; G[4] = G[6] - G[4]; G[4] = (G[4] & ~0x3u) | ((0u << 0) & 0x3u); G[4] = G[16] + G[4]; G[5] = G[18]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C250A0u); G[3] = G[20] << 2; L_08C25470: ; G[2] = word(rt, G[19] + 0x0u); G[2] = G[2] + G[3]; put(rt, G[19] + 0x4u, G[2]); L_08C2547C: ; G[2] = G[19]; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08C779C8 (a0 = the output buffer object, a1 = the character, -1 = only flush; v0 = -1 on failure, else 0 for -1 and the character otherwise): the mode word +0x2C needs bit 0x10 and
// the failure byte +0x81 must be clear. The put area is +0x10 (next) .. +0x14 (end of the written part): with room (next < end) the character is stored at the end, which
// grows by one, and the written part [next, end) is handed to the sink 0x08C773A4(object, next, length); a flush request hands it over unchanged. With no room the buffer is
// the area of +0x78 / +0x7C (size < 2: the character alone goes to the sink through the frame, byte +0x82 = 1; otherwise the area is installed at +4 / +8 / +0xC / +0x10 / +0x14 with
// +0x18 = last byte, +0x82 = 1 and the character is stored first). After a successful flush the area is reset to +0x78 (or to nothing when the mode lost 0x10 or the size is below 2).
void func_08C779C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 character = ctx.gpr[5];
    const bool flush_only = character + 1u == 0u;
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x18u}});
    const auto result = [&]() { ctx.gpr[2] = flush_only ? 0u : character; };
    const auto failure = [&]() { ctx.gpr[2] = 0xFFFFFFFFu; };
    if ((word(rt, object + 0x2Cu) & 0x10u) == 0u) { failure(); return; }
    if (load8(object + 0x81u) != 0u) { failure(); return; }
    const u32 next = word(rt, object + 0x10u);
    u32 end = word(rt, object + 0x14u);
    if (next < end) {
        if (!flush_only) {
            store8(end, static_cast<std::uint8_t>(character));
            end = word(rt, object + 0x14u) + 1u;
            put(rt, object + 0x14u, end);
            const u32 start = word(rt, object + 0x10u);
            if (call_set(rt, ctx, 0x08C773A4u, {object, start, end - start}) == 0u) { failure(); return; }
        } else if (call_set(rt, ctx, 0x08C773A4u, {object, next, end - next}) == 0u) {
            failure();
            return;
        }
        const u32 area = word(rt, object + 0x78u);
        const u32 has_mode = word(rt, object + 0x2Cu) & 0x10u;
        put(rt, object + 4u, area);
        put(rt, object + 8u, area);
        put(rt, object + 0xCu, area);
        const u32 size = word(rt, object + 0x7Cu);
        if (has_mode != 0u && size >= 2u) {
            put(rt, object + 0x18u, area + size - 1u);
            put(rt, object + 0x14u, area);
            put(rt, object + 0x10u, area);
        } else {
            put(rt, object + 0x18u, 0u);
            put(rt, object + 0x14u, 0u);
            put(rt, object + 0x10u, 0u);
        }
        result();
        return;
    }
    const u32 size = word(rt, object + 0x7Cu);
    if (size < 2u) {
        store8(frame.at(0u), static_cast<std::uint8_t>(character));
        if (!flush_only && call_set(rt, ctx, 0x08C773A4u, {object, frame.at(0u), 1u}) == 0u) { failure(); return; }
        result();
        store8(object + 0x82u, 1u);
        return;
    }
    const u32 area = word(rt, object + 0x78u);
    store8(object + 0x82u, 1u);
    put(rt, object + 0x18u, area + size - 1u);
    put(rt, object + 4u, area);
    put(rt, object + 8u, area);
    put(rt, object + 0xCu, area);
    put(rt, object + 0x14u, area);
    put(rt, object + 0x10u, area);
    if (!flush_only) {
        store8(area, static_cast<std::uint8_t>(character));
        put(rt, object + 0x14u, word(rt, object + 0x14u) + 1u);
    }
    result();
}
// 0x0882E624 (a0 = destination string, a1 = text, a2 = count, a3 = pointer to six holders; v0 from the last call, unchanged for a count below 6): each holder's object (a null one is reported
// through 0x08A5BB7C and then used anyway) answers its virtual +0x3C; the answers are the first value (kept), three bytes at frame +9 / +0xA / +0xB and two halfwords at frame
// +0xC / +0xE. A string is built from the text (0x088297A8), formatted with 0x08860AD8(0x088294D0(its first word), the first value, frame + 8) and released (0x08A5B9EC), then
// 0x08A85B1C(frame, 1) rebuilds the result that replaces the destination (0x08A5BAD8 unless it already is the destination) and is released.
void func_0882E624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 holders = ctx.gpr[7];
    const bool small = static_cast<s32>(ctx.gpr[6]) < 6;
    ctx.gpr[6] = small ? 1u : 0u;
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {31u, 0x24u}, {20u, 0x20u}, {16u, 0x10u}});
    if (small) return;
    const auto answer = [&](u32 index, u32 message) -> u32 {
        const u32 object = word(rt, holders + 4u * index);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    };
    const u32 value = answer(0u, 0x08C9A2B4u);
    store8(frame.at(9u), static_cast<std::uint8_t>(answer(1u, 0x08C9A2B4u)));
    store8(frame.at(0xAu), static_cast<std::uint8_t>(answer(2u, 0x08C9A2B4u)));
    store8(frame.at(0xBu), static_cast<std::uint8_t>(answer(3u, 0x08C9A2B4u)));
    store16(frame.at(0xCu), static_cast<std::uint16_t>(answer(4u, 0x08C9A2B4u)));
    const u32 half = answer(5u, 0x08C9A2B4u);
    const u32 built = frame.at(0x4u);
    store16(frame.at(0xEu), static_cast<std::uint16_t>(half));
    call_set(rt, ctx, 0x088297A8u, {built, text});
    const u32 head = word(rt, built);
    if (head == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 format = call_set(rt, ctx, 0x088294D0u, {head});
    call_set(rt, ctx, 0x08860AD8u, {format, value, frame.at(8u)});
    call_set(rt, ctx, 0x08A5B9ECu, {built});
    const u32 result = frame.at(0u);
    call_set(rt, ctx, 0x08A85B1Cu, {result, 1u});
    if (destination != result) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, result)});
    call_set(rt, ctx, 0x08A5B9ECu, {result});
}
// 0x08866DF8 (a0 = the object, f12 = the scale; v0 from the last call, unchanged without the +0x5AC child): the child's record (+0x5C8, holding an object at +0x1C and a name area at +0x1C
// of itself) is shown: unless the child's byte +0x5B7 is set the area is registered first (0x088FBB70 / 0x08867DE8). A parameter block on the frame (the three tables
// at +0x44A0 / +0x44DC / +0x44BC, the 0x088FC1B4 value, zeros and the scale at +0x24) is put through 0x08889D90 / 0x08889E74; with a pending request word +0x3F98 (none: end) its
// owner +0x3F9C (when set) gets 0x0890FDA4 with the scale. The record's object is asked virtual +8 (the area): when it holds, virtual +0xC (area, 0, 0) gives a second object whose
// virtual +8 decides whether its virtual +0xC(0) result (else 0) goes as the last argument of 0x0888A058(list, frame, 0x45D4 area, result).
void func_08866DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x34u}, {52u, 0x50u}, {31u, 0x48u}, {21u, 0x44u}, {20u, 0x40u}, {19u, 0x3Cu}, {18u, 0x38u}, {16u, 0x30u}});
    const u32 child = word(rt, object + 0x5ACu);
    if (child == 0u) return;
    const float scale = ctx.fpr[12];
    ctx.fpr[20] = scale;
    const u32 record = word(rt, child + 0x5C8u);
    const u32 area = object + 0x45D4u;
    const u32 source = object + 0x458Cu;
    const u32 name = record + 0x1Cu;
    if (load8(child + 0x5B7u) == 0u) {
        const u32 value = call_set(rt, ctx, 0x088FBB70u, {source});
        call_set(rt, ctx, 0x08867DE8u, {object, area, name, value});
    }
    const u32 list = object + 0x4584u;
    const u32 level = call_set(rt, ctx, 0x088FC1B4u, {source});
    const u32 block = frame.at(8u);
    put(rt, frame.at(0x14u), level);
    put(rt, frame.at(8u), object + 0x44A0u);
    put(rt, frame.at(0xCu), object + 0x44DCu);
    put(rt, frame.at(0x10u), object + 0x44BCu);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    put_float(rt, frame.at(0x24u), scale);
    put(rt, frame.at(0x28u), 0u);
    store8(frame.at(0x2Cu), 0u);
    store8(frame.at(0x2Du), 0u);
    call_set(rt, ctx, 0x08889D90u, {list, block, area});
    call_set(rt, ctx, 0x08889E74u, {list, block, area});
    u32 request = word(rt, object + 0x3F98u);
    if (request == 0u) return;
    const u32 owner = word(rt, object + 0x3F9Cu);
    if (owner != 0u) {
        ctx.fpr[12] = scale;
        call_set(rt, ctx, 0x0890FDA4u, {owner, request, area, name});
        request = word(rt, object + 0x3F98u);
    }
    put(rt, frame.at(0u), request);
    put(rt, frame.at(4u), name);
    u32 last = 0u;
    const u32 holder = word(rt, record + 0x1Cu);
    if (call_indirect(rt, ctx, word(rt, holder + 8u), {name}) != 0u) {
        const u32 second = call_indirect(rt, ctx, word(rt, word(rt, record + 0x1Cu) + 0xCu), {name, 0u, 0u});
        if (second != 0u) {
            if (call_indirect(rt, ctx, word(rt, word(rt, second) + 8u), {second}) != 0u) {
                last = call_indirect(rt, ctx, word(rt, word(rt, second) + 0xCu), {second, 0u});
            }
        }
    }
    ctx.gpr[2] = last;
    call_set(rt, ctx, 0x0888A058u, {list, frame.at(0u), area, last});
}
// 0x08921AF0 (a0 = the output record, a1 = the owner, a2 = the kind; v0 = the record): the record is cleared (four words +0 .. +0xC and the pair at +0x10 / +0x14), +0x18 = 50.0 and then
// the owner's float +0x1F8, +0x1C = the default flags word 0x08C8A87C. 0x0892074C(owner, kind) = 1 with a kind below 30 asks the owner's object at +0x244 (virtual +0xC8, record
// on the frame) and copies its six floats (+0 / +4 / +8 / +0xC, +0x10 / +0x14) and the scale at +0x18 into the record; kind 6 then nudges +8 by 3.4, +4 by 0.8 and sets +0xC to -0.1;
// a scale of 0 or less is the owner's +0x1F8 again. Otherwise a kind below 30 is checked with 0x089200F4(owner, kind): equal to 1 sets +4 = 1.0, the flags word
// (0x08C8A87C without bit 0, | 0x14) at +0x1C and clears +0 / +8.
void func_08921AF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    const u32 kind = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x34u}, {18u, 0x28u}, {16u, 0x20u}, {20u, 0x30u}, {19u, 0x2Cu}, {17u, 0x24u}, {31u, 0x38u}});
    const u32 flags = word(rt, 0x08C8A87Cu);
    const auto from_bits = [](u32 bits) { return std::bit_cast<float>(bits); };
    put(rt, record, 0u);
    put(rt, record + 4u, 0u);
    put(rt, record + 8u, 0u);
    put(rt, record + 0xCu, 0u);
    put(rt, record + 0x10u, 0u);
    put(rt, record + 0x14u, 0u);
    put_float(rt, record + 0x18u, from_bits(0x42480000u));
    put(rt, record + 0x1Cu, flags);
    put_float(rt, record + 0x18u, float_at(rt, owner + 0x1F8u));
    const u32 state = call_set(rt, ctx, 0x0892074Cu, {owner, kind});
    ctx.gpr[2] = record;
    if (state == 1u) {
        if (static_cast<s32>(kind) >= 0x1E) return;
        const u32 object = word(rt, owner + 0x244u);
        const u32 shape = frame.at(0u);
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xC8u), {shape, object, kind});
        const float scale = float_at(rt, shape + 0x18u);
        put_float(rt, record + 8u, float_at(rt, shape + 8u));
        put_float(rt, record + 4u, float_at(rt, shape + 4u));
        put_float(rt, record + 0xCu, float_at(rt, shape + 0xCu));
        put_float(rt, record, float_at(rt, shape));
        put_float(rt, record + 0x14u, float_at(rt, shape + 0x14u));
        put_float(rt, record + 0x10u, float_at(rt, shape + 0x10u));
        put_float(rt, record + 0x18u, scale);
        if (kind == 6u) {
            const float z = float_at(rt, record + 8u);
            const float x = float_at(rt, record + 4u);
            put_float(rt, record + 0xCu, from_bits(0xBDCCCCCDu));
            put_float(rt, record + 8u, z + from_bits(0x4059999Au));
            put_float(rt, record + 4u, x + from_bits(0x3F4CCCCDu));
        }
        if (float_at(rt, record + 0x18u) <= 0.0f) put_float(rt, record + 0x18u, float_at(rt, owner + 0x1F8u));
        ctx.gpr[2] = record;
        return;
    }
    if (static_cast<s32>(kind) >= 0x1E) return;
    ctx.gpr[4] = owner;
    if (call_set(rt, ctx, 0x089200F4u, {owner, kind}) == 1u) {
        put_float(rt, record + 4u, from_bits(0x3F800000u));
        put(rt, record + 0x1Cu, (flags & ~1u) | 0x14u);
        put(rt, record, 0u);
        put(rt, record + 8u, 0u);
    }
    ctx.gpr[2] = record;
}
// 0x089BD694 (a0 = the object; v0 from the last call, unchanged when there is nothing to do): with a node list at +0x5C whose first lookup 0x08BFE8D8 does not return the list's
// own head (+4), the entry (lookup + 0x14) is turned into a record (0x089FCC10 into frame +8) whose string +0x68 is copied (0x08A5D424) to frame +0xC; when that holds an object,
// its virtual +0x1C(object, frame + 0x10, frame + 4 string) fills a second record, which 0x089BA4D8(record, 0x089BD668(object)) and 0x089B8F38(record's +0x48, first record, record)
// complete; the first record's +0x68 string is then set to the constant 0x08CF6ED8 (0x08A5D414, copied back with 0x08A5BAD8 unless it is the same string). Every temporary
// string goes through 0x08A5B9EC; each null record is reported through 0x08A5BB7C and used anyway.
void func_089BD694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x2Cu}, {31u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    const auto checked = [&](u32 pointer, u32 message) {
        if (pointer == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    };
    const u32 list = word(rt, object + 0x5Cu);
    if (list == 0u) return;
    const u32 lookup = call_set(rt, ctx, 0x08BFE8D8u, {list, ctx.gpr[5]});
    if (word(rt, object + 0x5Cu) + 4u == lookup) return;
    const u32 first_slot = frame.at(8u);
    const u32 text_slot = frame.at(0xCu);
    const u32 record_slot = frame.at(0x10u);
    call_set(rt, ctx, 0x089FCC10u, {first_slot, lookup + 0x14u});
    const u32 first = word(rt, first_slot);
    checked(first, 0x08CA5CE4u);
    call_set(rt, ctx, 0x08A5D424u, {text_slot, first + 0x68u});
    if (word(rt, text_slot) != 0u) {
        call_set(rt, ctx, 0x089BAC14u, {record_slot});
        const u32 target = word(rt, text_slot);
        checked(target, 0x08CA5CA0u);
        const u32 method = word(rt, word(rt, target) + 0x1Cu);
        const u32 string = frame.at(4u);
        call_set(rt, ctx, 0x08A76AA0u, {string});
        call_indirect(rt, ctx, method, {target, record_slot, string});
        call_set(rt, ctx, 0x08A5B9ECu, {string});
        const u32 record = word(rt, record_slot);
        checked(record, 0x08CA5CB0u);
        const u32 key = call_set(rt, ctx, 0x089BD668u, {object});
        call_set(rt, ctx, 0x089BA4D8u, {record, key});
        u32 again = word(rt, record_slot);
        const u32 owner = again;
        checked(again, 0x08CA5CB0u);
        if (again == 0u) again = word(rt, record_slot);
        call_set(rt, ctx, 0x089B8F38u, {word(rt, owner + 0x48u), word(rt, first_slot), again});
        const u32 holder = word(rt, first_slot);
        checked(holder, 0x08CA5CE4u);
        call_set(rt, ctx, 0x08A5D414u, {frame.at(0u), 0x08CF6ED8u});
        if (holder + 0x68u != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {holder + 0x68u, word(rt, frame.at(0u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        call_set(rt, ctx, 0x08A5B9ECu, {record_slot});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {text_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {first_slot});
}
// 0x089DDCC4 (a0 = the output list, a1 = the object; v0 = the output list): the object's flag bit 7 of +0x140 selects one of two 12-byte descriptors from +0xE0 (the list of
// 8-byte entries at object + 12 * bit + 0xEC .. +0xF0, the float table at that first word). A string list of that many entries is created on the frame
// (0x08A98C1C(list, count)); for each entry a list of two strings is created, its two strings are the float pair of the entry formatted with 0x08A87EEC (assigned to the
// string slots at +0x14 and +0x14 + 4 of the pair, 0x08A5BAD8 unless it already is the target), and the pair itself becomes entry i of the list (+0x14 + 4 * i). The list is
// then copied into the output (0x08A76ABC) and released (0x08A5B9EC); every temporary goes through 0x08A5B9EC.
void func_089DDCC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {23u, 0x2Cu}, {20u, 0x20u}, {17u, 0x14u}, {31u, 0x34u}, {22u, 0x28u}, {21u, 0x24u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}});
    const u32 list = frame.at(8u);
    const u32 pair = frame.at(0xCu);
    const u32 text = frame.at(4u);
    const u32 other = frame.at(0u);
    const auto descriptor = [&]() -> u32 { return object + 12u * ((word(rt, object + 0x140u) >> 7) & 1u); };
    const u32 first_entry = word(rt, descriptor() + 0xE0u + 0xCu);
    const u32 count = static_cast<u32>((static_cast<s32>(word(rt, descriptor() + 0xE0u + 0x10u) - first_entry)) >> 3);
    call_set(rt, ctx, 0x08A98C1Cu, {list, count});
    if (count != 0u) {
        u32 i = 0u;
        do {
            const u32 offset = i << 3;
            call_set(rt, ctx, 0x08A98C1Cu, {pair, 2u});
            const u32 floats = word(rt, descriptor() + 0xECu) + offset;
            const u32 slot = word(rt, word(rt, pair) + 0x14u);
            ctx.fpr[12] = float_at(rt, floats);
            call_set(rt, ctx, 0x08A87EECu, {text, floats});
            if (slot != text) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, text)});
            call_set(rt, ctx, 0x08A5B9ECu, {text});
            const u32 pair_value = word(rt, pair);
            const u32 slot_b = word(rt, pair_value + 0x14u) + 4u;
            const u32 floats_b = offset + word(rt, descriptor() + 0xECu);
            ctx.fpr[12] = float_at(rt, floats_b + 4u);
            call_set(rt, ctx, 0x08A87EECu, {other, pair_value});
            if (slot_b != other) call_set(rt, ctx, 0x08A5BAD8u, {slot_b, word(rt, other)});
            call_set(rt, ctx, 0x08A5B9ECu, {other});
            const u32 target = (i << 2) + word(rt, word(rt, list) + 0x14u);
            ++i;
            if (target != pair) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, pair)});
            call_set(rt, ctx, 0x08A5B9ECu, {pair});
        } while (i < count);
    }
    call_set(rt, ctx, 0x08A76ABCu, {output, list});
    call_set(rt, ctx, 0x08A5B9ECu, {list});
    ctx.gpr[2] = output;
}
// 0x089DDEA4 (a0 = the output list, a1 = the object; v0 = the output list): the object's flag bit 8 of +0x140 selects one of two 12-byte descriptors from +0x100 (the list of
// 8-byte entries at object + 12 * bit + 0x104 .. +0x108, the float table at that first word). A string list of that many entries is created on the frame
// (0x08A98C1C(list, count)); for each entry a list of two strings is created, its two strings are the float pair of the entry formatted with 0x08A87EEC (assigned to the
// string slots at +0x14 and +0x14 + 4 of the pair, 0x08A5BAD8 unless it already is the target), and the pair itself becomes entry i of the list (+0x14 + 4 * i). The list is
// then copied into the output (0x08A76ABC) and released (0x08A5B9EC); every temporary goes through 0x08A5B9EC.
void func_089DDEA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {23u, 0x2Cu}, {20u, 0x20u}, {17u, 0x14u}, {31u, 0x34u}, {22u, 0x28u}, {21u, 0x24u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}});
    const u32 list = frame.at(8u);
    const u32 pair = frame.at(0xCu);
    const u32 text = frame.at(4u);
    const u32 other = frame.at(0u);
    const auto descriptor = [&]() -> u32 { return object + 12u * ((word(rt, object + 0x140u) >> 8) & 1u); };
    const u32 first_entry = word(rt, descriptor() + 0x104u);
    const u32 count = static_cast<u32>((static_cast<s32>(word(rt, descriptor() + 0x100u + 8u) - first_entry)) >> 3);
    call_set(rt, ctx, 0x08A98C1Cu, {list, count});
    if (count != 0u) {
        u32 i = 0u;
        do {
            const u32 offset = i << 3;
            call_set(rt, ctx, 0x08A98C1Cu, {pair, 2u});
            const u32 floats = word(rt, descriptor() + 0x104u) + offset;
            const u32 slot = word(rt, word(rt, pair) + 0x14u);
            ctx.fpr[12] = float_at(rt, floats);
            call_set(rt, ctx, 0x08A87EECu, {text, floats});
            if (slot != text) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, text)});
            call_set(rt, ctx, 0x08A5B9ECu, {text});
            const u32 pair_value = word(rt, pair);
            const u32 slot_b = word(rt, pair_value + 0x14u) + 4u;
            const u32 floats_b = offset + word(rt, descriptor() + 0x104u);
            ctx.fpr[12] = float_at(rt, floats_b + 4u);
            call_set(rt, ctx, 0x08A87EECu, {other, pair_value});
            if (slot_b != other) call_set(rt, ctx, 0x08A5BAD8u, {slot_b, word(rt, other)});
            call_set(rt, ctx, 0x08A5B9ECu, {other});
            const u32 target = (i << 2) + word(rt, word(rt, list) + 0x14u);
            ++i;
            if (target != pair) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, pair)});
            call_set(rt, ctx, 0x08A5B9ECu, {pair});
        } while (i < count);
    }
    call_set(rt, ctx, 0x08A76ABCu, {output, list});
    call_set(rt, ctx, 0x08A5B9ECu, {list});
    ctx.gpr[2] = output;
}
void func_08A6D698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[21] = G[5]; G[20] = G[4]; G[19] = G[7]; G[16] = word(rt, G[7] + 0x0u); { const bool tk = G[16] != 0u; G[18] = G[4] + 0x14u; if (tk) goto L_08A6D6E0; } G[4] = 0x08CB0000u; G[4] = 0x08CB06B8u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A6D6E0: ; G[2] = word(rt, G[16] + 0x0u); G[5] = G[16]; G[3] = word(rt, G[2] + 0xCu); { const std::uint32_t tg = G[3]; G[4] = frame.at(0xCu); call_here(rt, ctx, tg); } G[5] = word(rt, frame.at(0xCu)); G[4] = G[18]; G[6] = 0u; G[7] = word(rt, G[5] + 0xFFFFFFF4u); call_here(rt, ctx, 0x08B8D750u); G[4] = word(rt, frame.at(0xCu)); G[17] = G[2]; G[2] = 0x08D70000u; G[22] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[22]; G[2] = 0xFFFFFFFFu; if (tk) goto L_08A6D748; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0xFFFFFFFFu; if (tk) goto L_08A6D748; } G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0xFFFFFFFFu; L_08A6D748: ; if (G[17] == G[2]) { G[17] = 0u; goto L_08A6D7C0; } G[16] = word(rt, G[19] + 0x0u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A6D76C; } G[4] = 0x08CB0000u; G[4] = 0x08CB06B8u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A6D76C: ; G[4] = frame.at(0x4u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[5] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[20] + 0x14u); G[2] = word(rt, frame.at(0x4u)); G[4] = word(rt, G[3] + 0xFFFFFFF4u); G[3] = G[4] < G[17] ? 1u : 0u; { const bool tk = G[3] == 0u; G[6] = word(rt, G[2] + 0xFFFFFFF4u); if (tk) goto L_08A6D7A0; } G[4] = 0x08CB0000u; G[4] = 0x08CB0740u; call_here(rt, ctx, 0x08B5AEDCu); L_08A6D7A0: ; G[3] = G[4] - G[17]; G[2] = G[6] < G[3] ? 1u : 0u; G[5] = G[17]; if (G[2] == 0u) G[6] = G[3]; G[4] = G[18]; G[7] = 0u; call_here(rt, ctx, 0x08B8634Cu); G[17] = 0x1u; L_08A6D7C0: ; G[16] = frame.at(0x8u); G[5] = G[18]; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[21] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_08A6D7E0; } G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); L_08A6D7E0: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = G[17] == 0u; if (tk) goto L_08A6D824; } G[4] = word(rt, frame.at(0x4u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[22] == G[16]; if (tk) goto L_08A6D828; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) goto L_08A6D824; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_08A6D824: ; L_08A6D828: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[22] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A9A448 (a0 = destination string, a2 = the kind, a3 = pointer to the holders; v0 = 2 for a kind other than 1 and 2, else the last call's): a string pair (0x08A98674 into
// the frame; a null result is reported through 0x08A5BB7C and used anyway) and the answers of the holders' objects' virtual +0x3C are combined with 0x08A99728(string, pair
// first, answer a, answer b): kind 1 takes the first holder's answer and the pair's size ((+0x18 - +0x14) >> 2) as the second argument, kind 2 takes both holders' answers.
// The result replaces the destination (0x08A5BAD8 unless it already is the destination) and both strings are released (0x08A5B9EC).
void func_08A9A448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 kind = ctx.gpr[6];
    const u32 holders = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {16u, 0x10u}});
    ctx.gpr[2] = 1u;
    if (kind != 1u && kind != 2u) {
        ctx.gpr[2] = 2u;
        return;
    }
    const auto checked = [&](u32 pointer, u32 message) {
        if (pointer == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    };
    const auto answer = [&](u32 index) -> u32 {
        const u32 object = word(rt, holders + 4u * index);
        checked(object, 0x08CB2328u);
        return call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    };
    const u32 pair = kind == 1u ? frame.at(0xCu) : frame.at(0u);
    const u32 result = kind == 1u ? frame.at(8u) : frame.at(4u);
    call_set(rt, ctx, 0x08A98674u, {pair, ctx.gpr[5]});
    const u32 first = word(rt, pair);
    checked(first, 0x08CB23CCu);
    const u32 a = answer(0u);
    u32 b;
    if (kind == 1u) {
        const u32 owner = word(rt, pair);
        checked(owner, 0x08CB23CCu);
        const u32 begin = word(rt, owner + 0x14u);
        const u32 end = word(rt, owner + 0x18u);
        b = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
        call_set(rt, ctx, 0x08A99728u, {result, first, a, b});
    } else {
        b = answer(1u);
        call_set(rt, ctx, 0x08A99728u, {result, first, a, b});
    }
    if (destination != result) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, result)});
    call_set(rt, ctx, 0x08A5B9ECu, {result});
    call_set(rt, ctx, 0x08A5B9ECu, {pair});
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_330
} // namespace gt::game

#define HOT330(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_330::name, gt::ReturnKind::kind)
// HOT330(0x08A38540u, func_08A38540, Word);  // no comparable synthetic case yet
// HOT330(0x08A61018u, func_08A61018, Word);  // no comparable synthetic case yet
// HOT330(0x08A6EB58u, func_08A6EB58, Word);  // no comparable synthetic case yet
// HOT330(0x08AC4CB8u, func_08AC4CB8, Word);  // no comparable synthetic case yet
// HOT330(0x08B6065Cu, func_08B6065C, Word);  // no comparable synthetic case yet
HOT330(0x08B797F0u, func_08B797F0, Word);
HOT330(0x08C28918u, func_08C28918, Word);
HOT330(0x08806FA0u, func_08806FA0, Word);
HOT330(0x0883F73Cu, func_0883F73C, Word);
HOT330(0x088E4C28u, func_088E4C28, Word);
HOT330(0x089B5E70u, func_089B5E70, Word);
HOT330(0x08A15BA8u, func_08A15BA8, Word);
HOT330(0x08AEBB0Cu, func_08AEBB0C, Word);
HOT330(0x08B81B70u, func_08B81B70, Word);
HOT330(0x08B9CC90u, func_08B9CC90, Word);
// HOT330(0x08C252FCu, func_08C252FC, Word);  // no comparable synthetic case yet
HOT330(0x08C779C8u, func_08C779C8, Word);
HOT330(0x0882E624u, func_0882E624, Word);
HOT330(0x08866DF8u, func_08866DF8, Word);
HOT330(0x08921AF0u, func_08921AF0, Word);
HOT330(0x089BD694u, func_089BD694, Word);
HOT330(0x089DDCC4u, func_089DDCC4, Word);
HOT330(0x089DDEA4u, func_089DDEA4, Word);
// HOT330(0x08A6D698u, func_08A6D698, Word);  // no comparable synthetic case yet
HOT330(0x08A9A448u, func_08A9A448, Word);
