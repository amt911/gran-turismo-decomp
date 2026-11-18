// Hot batch 326: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <initializer_list>
#include <cmath>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_326 {
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

void insert_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool after);
void destroy_handles(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool base);

void func_089AA1C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x80u); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x64u), G[31]); put(rt, frame.at(0x60u), G[30]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x40u), G[16]); auto &F = ctx.fpr; put_float(rt, frame.at(0x70u), F[20]); F[20] = F[12]; G[2] = 0x3u; G[3] = 0xFFFFFFFFu; G[23] = G[4]; G[22] = G[7]; G[19] = G[5]; G[18] = G[6]; G[17] = G[8]; G[2] = G[2] + 0xFFFFFFFFu; L_089AA214: ; { const bool tk = G[2] != G[3]; G[2] = G[2] + 0xFFFFFFFFu; if (tk) goto L_089AA214; } G[4] = word(rt, G[17] + 0x0u); { const bool tk = G[4] == 0u; put(rt, frame.at(0x30u), 0u); if (tk) goto L_089AA24C; } G[16] = frame.at(0x10u); G[5] = G[16]; G[6] = 0u; call_here(rt, ctx, 0x089E4BC4u); G[2] = 0x1u; store8(G[23] + 0x118u, G[2]); put(rt, frame.at(0x30u), G[16]); G[2] = word(rt, G[17] + 0x0u); put(rt, G[23] + 0x11Cu, G[2]); L_089AA24C: ; G[2] = 0x2u; G[2] = G[2] + 0xFFFFFFFFu; L_089AA254: ; { const bool tk = G[2] != 0u; G[2] = G[2] + 0xFFFFFFFFu; if (tk) goto L_089AA254; } G[3] = word(rt, G[23] + 0x0u); G[30] = G[18] + 0xFFFFFFFFu; G[2] = word(rt, G[3] + 0x150u); { const std::uint32_t tg = G[2]; G[4] = G[23]; call_here(rt, ctx, tg); } if (static_cast<std::int32_t>(G[30]) <= 0) { G[3] = word(rt, G[23] + 0x0u); goto L_089AA2F4; } G[20] = G[19]; G[21] = 0x2u; G[19] = 0u; L_089AA284: ; G[16] = G[20]; G[17] = 0u; G[18] = frame.at(0u); L_089AA290: ; G[4] = G[18]; G[5] = G[16]; { const bool tk = G[19] == 0u; G[6] = G[22]; if (tk) goto L_089AA2B8; } { const bool tk = G[17] != 0u; F[0] = float_at(rt, frame.at(0x8u)); if (tk) goto L_089AA2B8; } F[1] = float_at(rt, frame.at(0xCu)); put_float(rt, frame.at(0x0u), F[0]); put_float(rt, frame.at(0x4u), F[1]); goto L_089AA2C0; L_089AA2B8: ; call_here(rt, ctx, 0x08B4AC54u); L_089AA2C0: ; G[17] = G[17] + 0x1u; G[18] = G[18] + 0x8u; { const bool tk = G[17] != G[21]; G[16] = G[16] + 0x8u; if (tk) goto L_089AA290; } F[12] = F[20]; G[6] = word(rt, frame.at(0x30u)); G[4] = G[23]; G[5] = frame.at(0u); G[19] = G[19] + 0x1u; call_here(rt, ctx, 0x089A9E58u); { const bool tk = G[30] != G[19]; G[20] = G[20] + 0x8u; if (tk) goto L_089AA284; } G[3] = word(rt, G[23] + 0x0u); L_089AA2F4: ; G[2] = word(rt, G[3] + 0x154u); { const std::uint32_t tg = G[2]; G[4] = G[23]; call_here(rt, ctx, tg); } F[20] = float_at(rt, frame.at(0x70u)); goto exit_;  exit_: ; G[23] = word(rt, frame.at(0x5Cu)); G[22] = word(rt, frame.at(0x58u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[30] = word(rt, frame.at(0x60u)); G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08A816C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[21] = G[5]; G[20] = G[4] + 0x18u; G[19] = frame.at(0x4u); G[18] = frame.at(0x8u); G[17] = word(rt, G[4] + 0x20u); goto L_08A81808; L_08A816FC: ; put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); put(rt, frame.at(0x14u), 0u); call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[4] = G[19]; G[2] = G[2] + 0x8u; put(rt, frame.at(0x14u), G[2]); put(rt, frame.at(0xCu), G[16]); put(rt, frame.at(0x10u), G[16]); call_here(rt, ctx, 0x08A76AA0u); G[4] = G[16]; G[5] = 0x2u; G[6] = G[19]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08B85AF4u); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0xCu)); G[3] = G[17] + 0x10u; G[2] = G[16] + 0x8u; G[4] = G[16]; { const bool tk = G[16] == G[3]; put(rt, frame.at(0x10u), G[2]); if (tk) goto L_08A81768; } G[5] = word(rt, G[17] + 0x10u); call_here(rt, ctx, 0x08A5BAD8u); G[16] = word(rt, frame.at(0xCu)); L_08A81768: ; G[2] = G[16] + 0x4u; G[3] = G[17] + 0x14u; { const bool tk = G[2] == G[3]; G[4] = G[2]; if (tk) goto L_08A81780; } G[5] = word(rt, G[17] + 0x14u); call_here(rt, ctx, 0x08A5BAD8u); L_08A81780: ; G[4] = G[18]; call_here(rt, ctx, 0x08A76AA0u); G[16] = word(rt, G[21] + 0x0u); G[4] = 0x08CB0000u; { const bool tk = G[16] != 0u; G[4] = 0x08CB15A0u; if (tk) goto L_08A817A0; } call_here(rt, ctx, 0x08A5BB7Cu); L_08A817A0: ; G[2] = word(rt, G[16] + 0x0u); G[7] = word(rt, frame.at(0xCu)); G[5] = G[18]; G[3] = word(rt, G[2] + 0xCCu); G[6] = 0x2u; { const std::uint32_t tg = G[3]; G[4] = G[16]; call_here(rt, ctx, tg); } G[4] = G[17]; call_here(rt, ctx, 0x08B5F100u); G[4] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[5] = word(rt, frame.at(0x10u)); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B856B0u); G[2] = word(rt, frame.at(0xCu)); G[5] = word(rt, frame.at(0x14u)); G[6] = 0x4u; G[4] = G[2]; G[5] = G[5] - G[2]; G[5] = sra(G[5], 2); { const bool tk = G[2] == 0u; G[5] = G[5] << 2; if (tk) goto L_08A81808; } call_here(rt, ctx, 0x08A5BC54u); L_08A81808: ; G[5] = 0x4u; { const bool tk = G[17] != G[20]; G[4] = 0x8u; if (tk) goto L_08A816FC; } goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088A045C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x50u), G[31]); auto &F = ctx.fpr; G[2] = G[5] << 5; G[2] = G[2] + 0x4F0u; G[18] = G[4]; G[5] = frame.at(0u); G[4] = frame.at(0x20u); G[3] = word(rt, G[18] + 0xCu); G[8] = word(rt, G[18] + 0x8u); put(rt, frame.at(0x30u), G[6]); G[17] = G[3] + G[2]; G[19] = G[8] + G[2]; put(rt, frame.at(0x34u), G[7]); G[16] = G[17] + 0x8u; G[7] = G[19] + 0x8u; G[3] = load8(G[7] + 0x1Cu); G[2] = load8(G[16] + 0x1Cu); { const bool tk = G[3] == G[2]; G[6] = frame.at(0x10u); if (tk) goto L_088A04D8; } G[4] = frame.at(0x30u); G[5] = G[16]; call_here(rt, ctx, 0x08BAD978u); G[5] = G[17] + 0x18u; G[4] = frame.at(0x34u); call_here(rt, ctx, 0x08BAD950u); G[2] = load8(G[16] + 0x1Cu); goto L_088A05B8; L_088A04D8: ; F[3] = float_at(rt, G[7] + 0xCu); F[1] = float_at(rt, G[7] + 0x4u); F[2] = float_at(rt, G[7] + 0x8u); F[0] = float_at(rt, G[19] + 0x8u); put_float(rt, frame.at(0x4u), F[1]); put_float(rt, frame.at(0x0u), F[0]); put_float(rt, frame.at(0x8u), F[2]); put_float(rt, frame.at(0xCu), F[3]); F[1] = float_at(rt, G[16] + 0x4u); F[2] = float_at(rt, G[16] + 0x8u); F[3] = float_at(rt, G[16] + 0xCu); F[0] = float_at(rt, G[17] + 0x8u); F[12] = float_at(rt, G[18] + 0x14u); put_float(rt, frame.at(0x14u), F[1]); put_float(rt, frame.at(0x10u), F[0]); put_float(rt, frame.at(0x18u), F[2]); put_float(rt, frame.at(0x1Cu), F[3]); call_here(rt, ctx, 0x08B4FBBCu); G[3] = G[19] + 0x18u; F[0] = float_at(rt, frame.at(0x20u)); G[2] = word(rt, frame.at(0x30u)); G[4] = G[17] + 0x18u; put_float(rt, G[2] + 0x0u, F[0]); F[1] = float_at(rt, frame.at(0x24u)); G[2] = word(rt, frame.at(0x30u)); put_float(rt, G[2] + 0x4u, F[1]); F[0] = float_at(rt, frame.at(0x28u)); G[2] = word(rt, frame.at(0x30u)); put_float(rt, G[2] + 0x8u, F[0]); F[1] = float_at(rt, frame.at(0x2Cu)); G[2] = word(rt, frame.at(0x30u)); put_float(rt, G[2] + 0xCu, F[1]); F[4] = float_at(rt, G[18] + 0x14u); F[3] = float_at(rt, G[18] + 0x10u); F[2] = float_at(rt, G[19] + 0x18u); F[0] = float_at(rt, G[17] + 0x18u); G[2] = word(rt, frame.at(0x34u)); F[2] = psp_mul(F[3], F[2]); F[0] = psp_mul(F[4], F[0]); F[2] = F[2] + F[0]; put_float(rt, G[2] + 0x0u, F[2]); F[1] = float_at(rt, G[3] + 0x4u); F[0] = float_at(rt, G[4] + 0x4u); G[2] = word(rt, frame.at(0x34u)); F[1] = psp_mul(F[3], F[1]); F[0] = psp_mul(F[4], F[0]); F[1] = F[1] + F[0]; put_float(rt, G[2] + 0x4u, F[1]); F[0] = float_at(rt, G[3] + 0x8u); F[1] = float_at(rt, G[4] + 0x8u); G[2] = word(rt, frame.at(0x34u)); F[3] = psp_mul(F[3], F[0]); F[4] = psp_mul(F[4], F[1]); F[3] = F[3] + F[4]; put_float(rt, G[2] + 0x8u, F[3]); G[2] = load8(G[16] + 0x1Cu); L_088A05B8: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x088BD0C4 (a0 = the object, f12 = a value): the state record (+0x24) not in state 6 first gets 0x088BCA5C / 0x088BC000(f12) and +0x28 = 0.
// Then 0x088BC9A8(object+4) decides: a hit with flag +0x6F clear sets bit 3 of +0x2 and +0x2C = 0; otherwise a state other than 6 puts 0.5 in
// +0x2C, and state 6 lowers +0x28 / raises +0x2C towards values from 0x088BCB80 faded by the countdown +0x52 (below 30, x 1/30 a step).
void func_088BD0C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {16u, 0x0u}, {31u, 0xCu}, {17u, 0x4u}, {52u, 0x10u}});
    const u32 inner = object + 0x4u, state = word(rt, object + 0x24u);
    ctx.gpr[2] = 6u;
    ctx.gpr[18] = inner;
    ctx.gpr[16] = object;
    ctx.gpr[17] = state;
    ctx.fpr[20] = ctx.fpr[12];
    if (load8(state + 0xA5u) != 6u) {
        call_here(rt, ctx, 0x088BCA5Cu);
        ctx.fpr[12] = ctx.fpr[20];
        call_set(rt, ctx, 0x088BC000u, {object});
        put(rt, state + 0x28u, 0u);
    }
    const u32 hit = call_set(rt, ctx, 0x088BC9A8u, {inner});
    if (hit != 0u && load8(state + 0x6Fu) == 0u) {
        const u32 bits = load16(state + 0x2u) | 0x8u;
        put(rt, state + 0x2Cu, 0u);
        store16(state + 0x2u, bits);
        ctx.gpr[2] = bits;  // v0: the halfword just stored
        return;
    }
    ctx.gpr[2] = 6u;
    if (load8(state + 0xA5u) != 6u) {
        put_float(rt, state + 0x2Cu, 0.5f);
        return;
    }
    u32 count = load8(state + 0x52u);
    ctx.gpr[2] = count;
    float rise = std::bit_cast<float>(0x3F4CCCCDu), lower = 0.0f;
    if (count != 0u) {
        ctx.fpr[12] = float_at(rt, word(rt, inner + 0x4u) + 0x6C4u) - std::bit_cast<float>(0x3E8E38E4u);
        call_set(rt, ctx, 0x088BCB80u, {inner, 0u});
        count = load8(state + 0x52u);
        lower = ctx.fpr[0];
        rise = 0.0f;
        if (count < 0x1Eu) {
            const float fade = psp_mul(static_cast<float>(static_cast<s32>(count)), std::bit_cast<float>(0x3D08AB7Cu));
            rise = psp_mul(1.0f - fade, std::bit_cast<float>(0x3F4CCCCDu));
            lower = psp_mul(lower, fade);
        }
        store8(state + 0x52u, count - 1u);
        ctx.gpr[2] = count - 1u;
    }
    ctx.set_fpu_condition(lower < float_at(rt, state + 0x28u));
    if (ctx.fpu_condition()) put_float(rt, state + 0x28u, lower);
    ctx.set_fpu_condition(float_at(rt, state + 0x2Cu) < rise);
    if (ctx.fpu_condition()) put_float(rt, state + 0x2Cu, rise);
}
// 0x089E8ADC (a0 = the object): a constructor. The base (0x0899A930) is built, the two vtables set (+0, +0x38), the fields from +0x12C to
// +0x1DC given their first values (scales 1.0 / 0.5 / 0.2, sizes 16.0, handles -1), the members at +0x15C / +0x190 (0x08969188 with
// 0x08CF6ED8) and +0x184..+0x18C (0x08A76AA0) built, and 0x089689C8(object, 1) ends it as a tail call.
void func_089E8ADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x8u}, {17u, 0x4u}});
        ctx.gpr[16] = self;
        call_set(rt, ctx, 0x0899A930u, {self});
        const u32 flags = (word(rt, self + 0x150u) | 0x1u) & ~0x1Eu;
        put(rt, self + 0x0u, 0x08CC93B0u);
        put(rt, self + 0x38u, 0x08CC9608u);
        ctx.gpr[17] = 0x08CF0000u;
        put_float(rt, self + 0x134u, std::bit_cast<float>(0x3E4CCCCDu));
        put(rt, self + 0x150u, flags);
        put(rt, self + 0x130u, 1u);
        put_float(rt, self + 0x13Cu, 1.0f);
        put_float(rt, self + 0x144u, 0.5f);
        put(rt, self + 0x12Cu, 1u);
        put_float(rt, self + 0x138u, 1.0f);
        put_float(rt, self + 0x140u, 0.5f);
        put(rt, self + 0x148u, 0u);
        put(rt, self + 0x14Cu, 0u);
        put(rt, self + 0x154u, 0u);
        put(rt, self + 0x158u, 0u);
        ctx.gpr[2] = 0x08CC9608u;
        ctx.gpr[3] = flags;
        ctx.fpr[0] = std::bit_cast<float>(0x3E4CCCCDu);
        ctx.fpr[1] = 1.0f;
        ctx.fpr[2] = 0.5f;
        call_set(rt, ctx, 0x08969188u, {self + 0x15Cu, 0x08CF6ED8u, 1u});
        put_float(rt, self + 0x164u, 16.0f);
        put_float(rt, self + 0x160u, 16.0f);
        ctx.fpr[0] = 16.0f;
        for (u32 i = 0u; i < 2u; ++i) {
            const u32 at = self + 0x168u + 0xCu * i;
            put(rt, at + 0x0u, 0u);
            put(rt, at + 0x4u, 0u);
            put(rt, at + 0x8u, 0u);
        }
        ctx.gpr[2] = self + 0x180u;
        ctx.gpr[3] = 0u;
        put(rt, self + 0x180u, 0u);
        call_set(rt, ctx, 0x08A76AA0u, {self + 0x184u});
        call_set(rt, ctx, 0x08A76AA0u, {self + 0x188u});
        call_set(rt, ctx, 0x08A76AA0u, {self + 0x18Cu});
        call_set(rt, ctx, 0x08969188u, {self + 0x190u, 0x08CF6ED8u});
        const u32 bits = word(rt, self + 0xE0u) & ~0x1000000u;
        put(rt, self + 0x1D4u, 0xFFFFFFFFu);
        put(rt, self + 0x1E0u, 0x08D74FDCu);
        put(rt, self + 0xE0u, bits);
        put(rt, self + 0x194u, 0u);
        put(rt, self + 0x198u, 0u);
        for (u32 at = 0x19Cu; at <= 0x1B0u; at += 4u) put(rt, self + at, 0xFFFFFFFFu);
        put(rt, self + 0x1B8u, 0u);
        put(rt, self + 0x1BCu, 0xFFFFFFFFu);
        put(rt, self + 0x1C0u, 0u);
        put(rt, self + 0x1C4u, 0xFFFFFFFFu);
        put(rt, self + 0x1C8u, 0u);
        put(rt, self + 0x1CCu, 0xFFFFFFFFu);
        put(rt, self + 0x1D0u, 0u);
        put(rt, self + 0x1D8u, 0u);
        store8(self + 0x1DCu, 0u);
        ctx.gpr[2] = 0x08D74FDCu;
        ctx.gpr[3] = 0xFFFFFFFFu;
        ctx.gpr[4] = self;
        ctx.gpr[5] = 1u;
        ctx.gpr[6] = bits;
    }
    jump(ctx, 0x089689C8u);
}
// 0x08A00E2C (a0 = the object): the bounding box of the children (from +0xF4, next by 0x08968A38) that have flag 0x1000 in +0xE0: each one's
// rectangle (its virtual +0x1A8, then 0x0896A9C4 into the frame) widens a running min / extent; the result (width, height) goes to the object
// with 0x0896A9C4(object, 0, 0, &height, &width).
void func_08A00E2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x24u}, {31u, 0x2Cu}, {18u, 0x28u}, {16u, 0x20u}, {55u, 0x3Cu}, {54u, 0x38u}, {53u, 0x34u}, {52u, 0x30u}});
    auto &F = ctx.fpr;
    u32 child = word(rt, object + 0xF4u);
    const u32 height = frame.at(0x4u), width = frame.at(0u);
    ctx.gpr[16] = child;
    ctx.gpr[17] = object;
    ctx.gpr[18] = height;
    F[22] = 0.0f;
    F[23] = 0.0f;
    if (child != 0u) {
        F[21] = 0.0f;
        F[20] = 0.0f;
        while (child != 0u) {
            if ((word(rt, child + 0xE0u) & 0x1000u) != 0u) {
                put(rt, frame.at(0x10u), 0u);
                put(rt, frame.at(0x14u), 0u);
                put(rt, frame.at(0x18u), 0u);
                put(rt, frame.at(0x1Cu), 0u);
                const u32 method = word(rt, word(rt, child) + 0x1A8u);
                ctx.gpr[2] = word(rt, child);
                ctx.gpr[3] = method;
                call_set(rt, ctx, method, {child, frame.at(0x18u), frame.at(0x1Cu)});
                put(rt, frame.at(0x0u), 0u);
                put(rt, frame.at(0x4u), 0u);
                call_set(rt, ctx, 0x0896A9C4u, {child, width, height, 0u, 0u});
                F[1] = float_at(rt, frame.at(0x10u));
                ctx.set_fpu_condition(F[20] < F[1]);
                F[5] = ctx.fpu_condition() ? F[20] : F[1];
                F[3] = float_at(rt, frame.at(0x14u));
                ctx.set_fpu_condition(F[21] < F[3]);
                F[4] = ctx.fpu_condition() ? F[21] : F[3];
                F[2] = F[23] + F[20];
                F[0] = F[1] + float_at(rt, frame.at(0x18u));
                ctx.set_fpu_condition(F[2] < F[0]);
                if (ctx.fpu_condition()) F[2] = F[0];
                F[1] = F[21] + F[22];
                F[0] = F[3] + float_at(rt, frame.at(0x1Cu));
                ctx.set_fpu_condition(F[1] < F[0]);
                if (ctx.fpu_condition()) F[1] = F[0];
                F[23] = F[2] - F[5];
                F[22] = F[1] - F[4];
                F[21] = F[4];
                F[20] = F[5];
            }
            child = call_set(rt, ctx, 0x08968A38u, {child});
            ctx.gpr[16] = child;
        }
    }
    put_float(rt, height, F[23]);
    put_float(rt, width, F[22]);
    call_set(rt, ctx, 0x0896A9C4u, {object, 0u, 0u, height, width});
}
void func_08AE6268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); auto &F = ctx.fpr; G[23] = 0x08CB0000u; G[22] = 0x08D40000u; G[21] = G[5]; G[19] = G[4]; G[17] = 0u; G[3] = word(rt, G[4] + 0x18u); G[18] = word(rt, G[4] + 0x10u); G[2] = word(rt, G[3] + 0x8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[17] = G[3]; { const bool tk = G[17] == 0u; G[6] = G[18]; if (tk) goto L_08AE63A4; } L_08AE62C0: ; F[0] = float_at(rt, G[6] + 0xFCu); F[1] = float_at(rt, 0x08CB6130u); G[2] = word(rt, G[17] + 0x4Cu); G[7] = word(rt, G[17] + 0x4u); F[0] = psp_mul(F[0], F[1]); G[20] = 0u; G[3] = word(rt, G[7] + 0x8u); G[16] = 0x08D40930u; F[1] = std::bit_cast<float>(trunc_to_word(F[0])); G[3] = static_cast<std::int32_t>(G[3]) < 0 ? 1u : 0u; if (G[3] == 0u) G[20] = G[7]; G[5] = G[17]; G[4] = bits_of(F[1]); G[6] = G[21]; G[2] = G[2] - G[4]; put(rt, G[17] + 0x4Cu, G[2]); G[2] = load8(G[18] + 0xE4u); G[2] = G[2] & 0xF0u; store8(G[17] + 0x84u, G[2]); G[3] = load8(G[18] + 0xE5u); G[3] = G[3] & 0xF0u; store8(G[17] + 0x85u, G[3]); G[2] = load8(G[18] + 0xE4u); G[2] = G[2] & 0xFu; store8(G[17] + 0x86u, G[2]); G[3] = load8(G[18] + 0xE5u); G[3] = G[3] & 0xFu; store8(G[17] + 0x87u, G[3]); G[2] = load8(G[18] + 0xE6u); store8(G[17] + 0x88u, G[2]); G[2] = 0x1u; G[3] = load8(G[18] + 0xE7u); store8(G[17] + 0x89u, G[3]); put(rt, G[16] + 0x44u, G[2]); G[3] = word(rt, G[17] + 0x28u); { const std::uint32_t tg = G[3]; G[4] = G[19]; call_here(rt, ctx, tg); } put(rt, G[16] + 0x44u, 0u); G[4] = sext(load16(G[17] + 0x1Au), 16); call_here(rt, ctx, 0x08AE8658u); G[3] = G[2] & 0x2u; G[2] = G[2] & 0x40u; if (G[2] == 0u) { G[2] = sext(load8(G[17] + 0x55u), 8); goto L_08AE6380; } G[2] = word(rt, G[19] + 0xCu); G[2] = G[2] | 0x40u; put(rt, G[19] + 0xCu, G[2]); G[2] = sext(load8(G[17] + 0x55u), 8); L_08AE6380: ; if (G[2] == 0u) { G[17] = G[20]; goto L_08AE6394; } { const bool tk = G[3] == 0u; G[4] = G[17]; if (tk) goto L_08AE63D0; } G[17] = G[20]; L_08AE6394: ; { const bool tk = G[20] == 0u; if (tk) goto L_08AE63A8; } G[6] = word(rt, G[19] + 0x10u); goto L_08AE62C0; L_08AE63A4: ; L_08AE63A8: ; goto exit_; L_08AE63D0: ; G[17] = G[20]; call_here(rt, ctx, 0x08AE34A8u); goto L_08AE6394;  exit_: ; G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B9A0C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_handles(rt, ctx, false); }
void func_088C896C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x7Cu), G[19]); put(rt, frame.at(0x78u), G[18]); put(rt, frame.at(0x70u), G[16]); put(rt, frame.at(0x80u), G[31]); put(rt, frame.at(0x74u), G[17]); auto &F = ctx.fpr; G[1] = 0x41200000u; F[0] = std::bit_cast<float>(G[1]); G[2] = 0xFFFFFFFFu; G[6] = frame.at(0x18u); G[18] = frame.at(0x24u); G[16] = G[5]; G[19] = frame.at(0xCu); put_float(rt, frame.at(0x90u), F[20]); G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); F[1] = float_at(rt, G[5] + 0x4u); F[3] = float_at(rt, G[5] + 0x0u); F[2] = float_at(rt, G[5] + 0x8u); F[1] = F[1] + F[0]; put(rt, frame.at(0x64u), G[2]); G[5] = frame.at(0x30u); put_float(rt, frame.at(0x18u), F[3]); put_float(rt, frame.at(0x20u), F[2]); put_float(rt, frame.at(0x1Cu), F[1]); put(rt, frame.at(0x60u), 0u); G[4] = word(rt, G[4] + 0x4u); call_here(rt, ctx, 0x088D2D10u); { const bool tk = G[2] == 0u; G[4] = G[18]; if (tk) goto L_088C8A6C; } F[2] = float_at(rt, frame.at(0x34u)); F[3] = float_at(rt, frame.at(0x38u)); F[0] = float_at(rt, frame.at(0x30u)); F[4] = float_at(rt, frame.at(0x50u)); F[5] = float_at(rt, frame.at(0x54u)); F[1] = float_at(rt, frame.at(0x4Cu)); put_float(rt, G[16] + 0x4u, F[2]); put_float(rt, G[16] + 0x8u, F[3]); put_float(rt, G[16] + 0x0u, F[0]); put_float(rt, frame.at(0x24u), F[1]); put_float(rt, frame.at(0x28u), F[4]); put_float(rt, frame.at(0x2Cu), F[5]); call_here(rt, ctx, 0x08B4A720u); F[0] = std::sqrt(F[0]); G[4] = G[18]; F[12] = F[20] / F[0]; call_here(rt, ctx, 0x08B4A5E8u); F[0] = float_at(rt, frame.at(0x24u)); F[1] = float_at(rt, frame.at(0x28u)); G[4] = G[19]; put_float(rt, frame.at(0xCu), F[0]); G[1] = 0x3D230000u; G[1] = G[1] | 0xD70Au; F[12] = std::bit_cast<float>(G[1]); F[0] = float_at(rt, frame.at(0x2Cu)); put_float(rt, frame.at(0x10u), F[1]); put_float(rt, frame.at(0x14u), F[0]); call_here(rt, ctx, 0x08B4A5E8u); G[4] = G[16]; G[5] = G[16]; G[6] = G[19]; call_here(rt, ctx, 0x08B4AA34u); goto L_088C8ACC; L_088C8A6C: ; put(rt, frame.at(0x24u), 0u); put_float(rt, frame.at(0x28u), F[20]); put(rt, frame.at(0x2Cu), 0u); call_here(rt, ctx, 0x08B4A720u); F[0] = std::sqrt(F[0]); G[4] = G[18]; F[12] = F[20] / F[0]; call_here(rt, ctx, 0x08B4A5E8u); F[0] = float_at(rt, frame.at(0x24u)); F[1] = float_at(rt, frame.at(0x28u)); G[4] = frame.at(0u); put_float(rt, frame.at(0x0u), F[0]); G[1] = 0x3D230000u; G[1] = G[1] | 0xD70Au; F[12] = std::bit_cast<float>(G[1]); F[0] = float_at(rt, frame.at(0x2Cu)); put_float(rt, frame.at(0x4u), F[1]); put_float(rt, frame.at(0x8u), F[0]); call_here(rt, ctx, 0x08B4A5E8u); G[4] = G[16]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B4AA34u); L_088C8ACC: ; F[20] = float_at(rt, frame.at(0x90u)); goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x7Cu)); G[18] = word(rt, frame.at(0x78u)); G[16] = word(rt, frame.at(0x70u)); G[17] = word(rt, frame.at(0x74u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089167E8 (a0 = the object): with a source (+4) and the flag +8 set, inside 0x08B344E0 / 0x08B3457C on +0x38 (the second as the tail call):
// +0x10 = 0x088D2BC8(source; +0x10); +0x18 moves towards it by the source's step (0x088D2C28) when they are more than half a step apart, then is
// blended in double precision as +0x18 + (+0x10 - +0x18) * 1.27 (0x08B70F28 to double, 0x08B71394 multiply, 0x08B71328 add, 0x08B71EC8 back),
// and 0x089167B4 follows.
void func_089167E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    u32 tail = 0u;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {31u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}, {53u, 0x24u}, {52u, 0x20u}});
        ctx.gpr[2] = word(rt, object + 0x4u);
        ctx.gpr[18] = object;
        if (ctx.gpr[2] == 0u) return;
        const u32 lock = object + 0x38u;
        ctx.gpr[2] = load8(object + 0x8u);
        ctx.gpr[19] = lock;
        ctx.fpr[21] = 0.5f;
        ctx.gpr[4] = lock;
        if (ctx.gpr[2] == 0u) return;
        call_set(rt, ctx, 0x08B344E0u, {lock});
        ctx.fpr[12] = float_at(rt, object + 0x10u);
        call_set(rt, ctx, 0x088D2BC8u, {word(rt, object + 0x4u)});
        put_float(rt, object + 0x10u, ctx.fpr[0]);
        ctx.fpr[20] = ctx.fpr[0] - float_at(rt, object + 0x18u);
        call_set(rt, ctx, 0x088D2C28u, {word(rt, object + 0x4u)});
        ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[21]);
        ctx.fpr[20] = std::fabs(ctx.fpr[20]);
        ctx.set_fpu_condition(ctx.fpr[0] < ctx.fpr[20]);
        if (ctx.fpu_condition()) {
            ctx.fpr[20] = float_at(rt, object + 0x10u);
            call_set(rt, ctx, 0x088D2C28u, {word(rt, object + 0x4u)});
            ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[21]);
            ctx.set_fpu_condition(ctx.fpr[20] < ctx.fpr[0]);
            const bool lower = ctx.fpu_condition();
            ctx.fpr[20] = float_at(rt, object + 0x18u);
            call_set(rt, ctx, 0x088D2C28u, {word(rt, object + 0x4u)});
            ctx.fpr[20] = lower ? ctx.fpr[20] - ctx.fpr[0] : ctx.fpr[20] + ctx.fpr[0];
            put_float(rt, object + 0x18u, ctx.fpr[20]);
        }
        ctx.fpr[20] = float_at(rt, object + 0x18u);
        ctx.fpr[12] = ctx.fpr[20];
        const u32 base_lo = call_set(rt, ctx, 0x08B70F28u, {}), base_hi = ctx.gpr[3];
        ctx.gpr[16] = base_lo;
        ctx.gpr[17] = base_hi;
        ctx.fpr[12] = float_at(rt, object + 0x10u) - ctx.fpr[20];
        const u32 gap_lo = call_set(rt, ctx, 0x08B70F28u, {}), gap_hi = ctx.gpr[3];
        const u32 scaled_lo = call_set(rt, ctx, 0x08B71394u, {gap_lo, gap_hi, 0x47AE147Bu, 0x3FB47AE1u}), scaled_hi = ctx.gpr[3];
        const u32 sum_lo = call_set(rt, ctx, 0x08B71328u, {base_lo, base_hi, scaled_lo, scaled_hi}), sum_hi = ctx.gpr[3];
        call_set(rt, ctx, 0x08B71EC8u, {sum_lo, sum_hi});
        put_float(rt, object + 0x18u, ctx.fpr[0]);
        call_set(rt, ctx, 0x089167B4u, {object});
        ctx.gpr[4] = lock;
        tail = 0x08B3457Cu;
    }
    jump(ctx, tail);
}
// 0x08A06780 / 0x08A068FC share this body: (a0 = the owner, a1 = an index). An index inside the owner's count (its virtual +0x1C8) inserts a
// default 96-byte record (0x089D76A8 builds it on the frame: vtable 0x08CCFF88, scales 1.0, handles -1) into the vector at owner+0xA8 with
// 0x08C11A30 at that record (`after`: the one after it); otherwise the record is appended: copied in place when there is room (two
// 0x0896418C sub-copies), else through 0x08C11600. The frame's record is destroyed by 0x089D6684.
void insert_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool after) {
    const u32 owner = ctx.gpr[4], index = ctx.gpr[5];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x64u}, {16u, 0x60u}, {31u, 0x70u}, {19u, 0x6Cu}, {18u, 0x68u}});
    ctx.gpr[17] = owner;
    ctx.gpr[16] = index;
    if (static_cast<s32>(index) >= 0) {
        const u32 method = word(rt, word(rt, owner) + 0x1C8u);
        ctx.gpr[3] = word(rt, owner);
        const u32 count = call_set(rt, ctx, method, {});
        ctx.gpr[2] = static_cast<s32>(index) < static_cast<s32>(count) ? 1u : 0u;
        if (static_cast<s32>(index) < static_cast<s32>(count)) {
            const u32 at = word(rt, owner + 0xA8u) + index * 96u;
            ctx.gpr[2] = index * 96u;
            ctx.gpr[3] = index << 5;
            call_set(rt, ctx, 0x08C11A30u, {owner + 0xA8u, after ? at + 0x60u : at, at});
            return;
        }
    }
    const u32 vec = owner + 0xA8u, temp = frame.at(0u);
    ctx.gpr[18] = vec;
    call_set(rt, ctx, 0x089D76A8u, {temp});
    u32 end = word(rt, vec + 0x4u);
    const u32 capacity_end = word(rt, vec + 0x8u);
    ctx.gpr[2] = capacity_end;
    ctx.gpr[19] = 0x08CD0000u;
    ctx.fpr[0] = 1.0f;
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[7] = 0x08CCFF88u;
    put_float(rt, frame.at(0x40u), 1.0f);
    put(rt, frame.at(0x50u), 0xFFFFFFFFu);
    put(rt, frame.at(0x0u), 0x08CCFF88u);
    for (u32 at = 0x20u; at <= 0x2Cu; at += 4u) put_float(rt, frame.at(at), 1.0f);
    put(rt, frame.at(0x30u), 0xFFFFFFFFu);
    for (u32 at = 0x44u; at <= 0x4Cu; at += 4u) put_float(rt, frame.at(at), 1.0f);
    if (end == capacity_end) {
        call_set(rt, ctx, 0x08C11600u, {vec, end, temp});
    } else {
        ctx.fpr[0] = float_at(rt, frame.at(0xCu));
        if (end != 0u) {
            put_float(rt, end + 0xCu, ctx.fpr[0]);
            put(rt, end + 0x4u, word(rt, frame.at(0x4u)));
            put_float(rt, end + 0x10u, float_at(rt, frame.at(0x10u)));
            put_float(rt, end + 0x14u, float_at(rt, frame.at(0x14u)));
            put(rt, end + 0x8u, word(rt, frame.at(0x8u)));
            put_float(rt, end + 0x18u, float_at(rt, frame.at(0x18u)));
            put_float(rt, end + 0x1Cu, float_at(rt, frame.at(0x1Cu)));
            put(rt, end + 0x0u, 0x08CCFF88u);
            ctx.gpr[17] = end + 0x20u;
            ctx.gpr[16] = end + 0x40u;
            ctx.fpr[1] = float_at(rt, frame.at(0x18u));
            ctx.fpr[0] = float_at(rt, frame.at(0x1Cu));
            call_set(rt, ctx, 0x0896418Cu, {end + 0x20u, frame.at(0x20u)});
            put(rt, end + 0x30u, word(rt, frame.at(0x30u)));
            call_set(rt, ctx, 0x0896418Cu, {end + 0x40u, frame.at(0x40u)});
            put(rt, end + 0x50u, word(rt, frame.at(0x50u)));
            end = word(rt, vec + 0x4u);
        }
        put(rt, vec + 0x4u, end + 0x60u);
    }
    ctx.gpr[2] = 0x08CCFF88u;
    put(rt, temp, 0x08CCFF88u);
    call_set(rt, ctx, 0x089D6684u, {temp});
}

void func_08A06780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { insert_record(rt, ctx, false); }
// 0x08AE430C (a0 = the object, a1 = key 1, a2 = key 2, a3 = the mode): walks the groups of the list at 0x08D40938 (next at +4) and each group's
// entries (from +0x18, next at +4; a node whose +8 is negative ends a list). An entry matches when its group's header (+0x10) has the object's
// id (+0x24 -> s16) and both keys, or when the entry itself does (+0x18 id, +0x1E key 1, +0x20 key 2). A match is marked (+0x55 = 1); modes 1
// and 0 also report it first through 0x08AE85E8(entry +0x1A, &code) with code 8 or 11.
void func_08AE430C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], key1 = ctx.gpr[5], key2 = ctx.gpr[6], mode = ctx.gpr[7];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x70u}, {18u, 0x58u}, {17u, 0x54u}, {31u, 0x74u}, {23u, 0x6Cu}, {22u, 0x68u}, {21u, 0x64u}, {20u, 0x60u}, {19u, 0x5Cu}, {16u, 0x50u}});
    // A node pointer, or 0 when the node's +8 is negative (the end of a list); v0 = that test, as the asm leaves it.
    auto live = [&](u32 node) {
        ctx.gpr[2] = static_cast<s32>(word(rt, node + 0x8u)) < 0 ? 1u : 0u;
        return ctx.gpr[2] == 0u ? node : 0u;
    };
    ctx.gpr[30] = key1;
    ctx.gpr[17] = mode;
    put(rt, frame.at(0x40u), key2);
    const u32 list = word(rt, 0x08D40938u);
    ctx.gpr[3] = list;
    ctx.gpr[8] = word(rt, object + 0x24u);
    u32 group = live(list);
    const u32 id = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(load16(ctx.gpr[8]))));
    ctx.gpr[22] = id;
    while (group != 0u) {
        ctx.gpr[18] = group;
        const u32 header = word(rt, group + 0x10u);
        bool group_match = false;
        if (header != 0u && signed_half(rt, header + 0x12u) == id && word(rt, header + 0x14u) == key1)
            group_match = (word(rt, header + 0x18u) ^ word(rt, frame.at(0x40u))) == 0u;
        ctx.gpr[21] = group_match ? 1u : 0u;
        u32 entry = live(word(rt, group + 0x18u));
        if (entry != 0u) {
            ctx.gpr[19] = 1u;
            ctx.gpr[20] = static_cast<s32>(mode) < 2 ? 1u : 0u;
            ctx.gpr[23] = 8u;
            while (entry != 0u) {
                ctx.gpr[16] = entry;
                const bool match = group_match || (signed_half(rt, entry + 0x18u) == id && signed_half(rt, entry + 0x1Eu) == key1 &&
                                                   signed_half(rt, entry + 0x20u) == word(rt, frame.at(0x40u)));
                if (match) {
                    bool report = false;
                    if (mode == 1u) {
                        put(rt, frame.at(0x0u), 8u);
                        report = true;
                    } else if (static_cast<s32>(mode) < 2 && mode == 0u) {
                        ctx.gpr[2] = 0xBu;
                        put(rt, frame.at(0x0u), 0xBu);
                        report = true;
                    }
                    if (report) call_set(rt, ctx, 0x08AE85E8u, {signed_half(rt, entry + 0x1Au), frame.at(0u)});
                    store8(entry + 0x55u, 1u);
                }
                entry = live(word(rt, entry + 0x4u));
            }
        }
        group = live(word(rt, group + 0x4u));
    }
}
void func_08AEE744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = 0xFFFFFFFFu; G[22] = G[5]; G[21] = G[6]; G[20] = G[7]; G[17] = G[4]; put(rt, G[5] + 0x0u, G[2]); put(rt, G[6] + 0x0u, 0u); G[2] = word(rt, G[7] + 0x0u); { const bool tk = G[2] != 0u; G[2] = 0x08D40000u; if (tk) goto L_08AEE848; } G[23] = 0x08D41AD0u; G[30] = 0x08CE0000u; L_08AEE7A0: ; G[4] = G[17]; call_here(rt, ctx, 0x08AEE5A0u); if (G[2] == 0u) { G[5] = word(rt, G[23] + 0x534u); goto L_08AEE7C4; } call_here(rt, ctx, 0x08AEE608u); { const bool tk = G[2] == 0u; if (tk) goto L_08AEE84C; } G[5] = word(rt, G[23] + 0x534u); L_08AEE7C4: ; G[4] = G[17]; G[6] = 0u; G[5] = G[5] | 0x1u; call_here(rt, ctx, 0x08C7A870u); G[16] = G[2]; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[9] = G[2]; if (tk) goto L_08AEE81C; } G[4] = G[2]; G[6] = 0u; G[7] = 0u; G[8] = 0x2u; call_here(rt, ctx, 0x08C7A880u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; put(rt, G[21] + 0x0u, G[2]); if (tk) goto L_08AEE8B0; } G[4] = G[16]; G[6] = 0u; G[7] = 0u; G[8] = 0u; call_here(rt, ctx, 0x08C7A880u); G[18] = G[2]; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[9] = G[16]; if (tk) goto L_08AEE8A0; } L_08AEE81C: ; { const bool tk = static_cast<std::int32_t>(G[9]) >= 0; put(rt, G[22] + 0x0u, G[9]); if (tk) goto L_08AEE848; } G[2] = word(rt, 0x08CDFB3Cu); G[2] = G[2] & 0x1u; { const bool tk = G[2] != 0u; G[2] = 0x08CE0000u; if (tk) goto L_08AEE878; } L_08AEE834: ; G[4] = G[20]; call_here(rt, ctx, 0x08AEE6E0u); G[2] = word(rt, G[20] + 0x0u); { const bool tk = G[2] == 0u; if (tk) goto L_08AEE7A0; } L_08AEE848: ; L_08AEE84C: ; goto exit_; L_08AEE878: ; G[4] = word(rt, 0x08CDFB40u); G[2] = 0x08C90000u; G[5] = 0x08C91F50u; G[2] = 0x08C90000u; G[6] = 0xBFEu; G[7] = 0x08C924E4u; G[8] = G[17]; call_here(rt, ctx, 0x08AD83B8u); goto L_08AEE834; L_08AEE8A0: ; G[4] = G[16]; call_here(rt, ctx, 0x08C7A840u); G[9] = G[18]; goto L_08AEE81C; L_08AEE8B0: ; G[4] = G[16]; call_here(rt, ctx, 0x08C7A840u); G[9] = word(rt, G[21] + 0x0u); goto L_08AEE81C;  exit_: ; G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x4u)); G[30] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B41954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[19] = G[5] + 0x1Au; G[16] = G[4] + 0x8DCu; G[21] = G[5]; G[5] = G[19]; G[20] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08B3F144u); if (G[2] == 0u) { G[3] = word(rt, G[20] + 0x0u); goto L_08B419B8; } G[2] = word(rt, G[2] + 0x28u); G[2] = G[2] + 0xFFFFFFFDu; G[2] = G[2] < 0x2u ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = 0u; if (tk) goto L_08B41AA4; } G[3] = word(rt, G[20] + 0x0u); L_08B419B8: ; G[2] = word(rt, G[3] + 0xA0u); { const std::uint32_t tg = G[2]; G[4] = G[20]; call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; G[5] = G[19]; if (tk) goto L_08B41A34; } G[6] = word(rt, G[21] + 0x0u); G[4] = G[20]; G[7] = 0u; G[8] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B402E4u); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[16]; if (tk) goto L_08B41AA0; } G[5] = G[2]; call_here(rt, ctx, 0x08B3EE8Cu); G[16] = G[2]; G[2] = 0x4u; put(rt, G[16] + 0x28u, G[2]); G[4] = word(rt, G[20] + 0xB0u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x20u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } G[6] = word(rt, G[20] + 0x920u); G[7] = word(rt, G[20] + 0x924u); G[4] = G[2] + G[6]; G[8] = G[4] < G[2] ? 1u : 0u; G[5] = G[3] + G[7]; G[5] = G[8] + G[5]; put(rt, G[16] + 0x48u, G[4]); put(rt, G[16] + 0x4Cu, G[5]); goto L_08B41A90; L_08B41A34: ; G[4] = G[16]; call_here(rt, ctx, 0x08B3F3C8u); { const bool tk = G[2] == 0u; G[17] = G[2]; if (tk) goto L_08B41A90; } G[22] = 0x3u; G[23] = 0x4u; G[18] = word(rt, G[17] + 0x0u); L_08B41A50: ; G[5] = G[19]; G[16] = G[18] + 0x12u; G[4] = G[16]; call_here(rt, ctx, 0x08B3F980u); G[4] = G[16]; { const bool tk = G[2] == 0u; G[5] = G[19]; if (tk) goto L_08B41A84; } G[2] = word(rt, G[21] + 0x0u); put(rt, G[18] + 0x8u, G[2]); call_here(rt, ctx, 0x08B3F7CCu); G[2] = word(rt, G[18] + 0x28u); if (G[2] != G[22]) { put(rt, G[18] + 0x28u, G[23]); goto L_08B41A84; } L_08B41A84: ; G[17] = word(rt, G[17] + 0x8u); if (G[17] != 0u) { G[18] = word(rt, G[17] + 0x0u); goto L_08B41A50; } L_08B41A90: ; G[4] = G[20]; call_here(rt, ctx, 0x08B402ACu); G[2] = 0x1u; goto L_08B41AA4; L_08B41AA0: ; G[2] = 0u; L_08B41AA4: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[16] = word(rt, frame.at(0x0u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08C2EDC4 (a0 = first, a1 = middle, a2 = last, a3 = a comparer with a virtual "less" at +8): a partial sort of word-sized elements. The heap
// over [first, middle) is built with 0x08C2EB94 (adjust, on a copy made by 0x08A76ABC and destroyed by 0x08A5B9EC); every later element the
// comparer puts before the top replaces it (0x08A5BAD8 assigns) and is sifted down; 0x08C2ECE8 sorts the heap at the end.
void func_08C2EDC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], middle = ctx.gpr[5], last = ctx.gpr[6], less = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x28u}, {30u, 0x30u}, {23u, 0x2Cu}, {21u, 0x24u}, {20u, 0x20u}, {31u, 0x34u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    const u32 count = sra(middle - first, 2);
    ctx.gpr[22] = count;
    ctx.gpr[30] = last;
    ctx.gpr[23] = middle;
    ctx.gpr[21] = less;
    ctx.gpr[20] = first;
    if (static_cast<s32>(count) >= 2) {
        const u32 top = count - 2u;
        const u32 parent = sra((top >> 31) + top, 1);
        ctx.gpr[18] = parent;
        const u32 copy = frame.at(0x4u);
        ctx.gpr[19] = copy;
        u32 at = first + (parent << 2);
        for (u32 done = 0u;; ++done) {
            ctx.gpr[16] = at;
            ctx.gpr[17] = done;
            call_set(rt, ctx, 0x08A76ABCu, {copy, at});
            call_set(rt, ctx, 0x08C2EB94u, {first, parent - done, count, copy, less});
            call_set(rt, ctx, 0x08A5B9ECu, {copy});
            at -= 4u;
            ctx.gpr[16] = at;
            if (parent == done) break;
        }
    }
    if (middle < last) {
        const u32 held = frame.at(0x8u), top_copy = frame.at(0u);
        ctx.gpr[17] = held;
        u32 at = middle;
        for (; at < last; at += 4u) {
            ctx.gpr[16] = at;
            const u32 test = word(rt, word(rt, less) + 0x8u);
            ctx.gpr[2] = test;
            ctx.gpr[3] = word(rt, less);
            if (call_set(rt, ctx, test, {less, at, first}) == 0u) {
                ctx.gpr[4] = held;
                ctx.gpr[5] = at;
                continue;
            }
            call_set(rt, ctx, 0x08A76ABCu, {held, at});
            if (at != first) call_set(rt, ctx, 0x08A5BAD8u, {at, word(rt, first)});
            call_set(rt, ctx, 0x08A76ABCu, {top_copy, held});
            call_set(rt, ctx, 0x08C2EB94u, {first, 0u, count, top_copy, less});
            call_set(rt, ctx, 0x08A5B9ECu, {top_copy});
            call_set(rt, ctx, 0x08A5B9ECu, {held});
        }
        ctx.gpr[16] = at;
        ctx.gpr[5] = at;
    }
    call_set(rt, ctx, 0x08C2ECE8u, {first, middle, less});
}
// 0x088C93C4 (a0 = the object, a1 = a source): four values 0x088D6218(source, i) go to the object's four 0x84-byte blocks (+0xC4) as floats; the
// first one, when not negative, also feeds 0x088C85E4 twice (blocks +0x44 and +0x84). Then values 0 and 1 decide the ending: both
// present (>= 0) tail-call 0x088C8AE8(object; f12, f13), only value 0 tail-calls 0x088C9088(object; f12), none returns.
void func_088C93C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], source = ctx.gpr[5];
    auto as_float = [](u32 value) { return static_cast<float>(static_cast<s32>(value)); };
    u32 tail = 0u;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x14u}, {52u, 0x20u}});
        ctx.fpr[20] = 0.0f;
        ctx.gpr[20] = 4u;
        ctx.gpr[19] = source;
        ctx.gpr[18] = object;
        u32 block = object;
        for (u32 i = 0u; i < 4u; ++i) {
            ctx.gpr[17] = i;
            const u32 value = call_set(rt, ctx, 0x088D6218u, {source, i});
            ctx.fpr[1] = std::bit_cast<float>(value);
            ctx.fpr[0] = as_float(value);
            put_float(rt, block + 0xC4u, ctx.fpr[0]);
            block += 0x84u;
            ctx.gpr[16] = block;
            if (i != 0u) continue;
            ctx.fpr[0] = float_at(rt, object + 0xC4u);
            ctx.gpr[5] = object + 0x44u;
            ctx.gpr[6] = 1u;
            ctx.set_fpu_condition(ctx.fpr[20] <= ctx.fpr[0]);
            ctx.fpr[12] = ctx.fpr[0];
            ctx.gpr[4] = object;
            if (!ctx.fpu_condition()) continue;
            call_set(rt, ctx, 0x088C85E4u, {object, object + 0x44u, 1u});
            ctx.fpr[12] = float_at(rt, object + 0xC4u);
            call_set(rt, ctx, 0x088C85E4u, {object, object + 0x84u, 0u});
        }
        bool both = false;
        if (static_cast<s32>(call_set(rt, ctx, 0x088D6218u, {source, 0u})) >= 0)
            both = static_cast<s32>(call_set(rt, ctx, 0x088D6218u, {source, 1u})) >= 0;
        if (both) {
            const u32 first = call_set(rt, ctx, 0x088D6218u, {source, 0u});
            ctx.fpr[20] = std::bit_cast<float>(first);
            const u32 second = call_set(rt, ctx, 0x088D6218u, {source, 1u});
            ctx.fpr[0] = std::bit_cast<float>(second);
            ctx.fpr[12] = as_float(first);
            ctx.gpr[4] = object;
            ctx.fpr[13] = as_float(second);
            tail = 0x088C8AE8u;
        } else if (static_cast<s32>(call_set(rt, ctx, 0x088D6218u, {source, 0u})) >= 0) {
            const u32 first = call_set(rt, ctx, 0x088D6218u, {source, 0u});
            ctx.fpr[1] = std::bit_cast<float>(first);
            ctx.gpr[4] = object;
            ctx.fpr[12] = as_float(first);
            tail = 0x088C9088u;
        }
    }
    if (tail != 0u) jump(ctx, tail);
}
void func_088DB990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[19] = G[4]; G[4] = 0xFFFFFFFFu; G[20] = G[6]; G[18] = G[5]; { const bool tk = G[19] == 0u; if (tk) goto L_088DB9E8; } G[21] = G[6] + 0xC8u; G[6] = sext(load8(G[19] + 0x0u), 8); G[3] = G[6] << 5; G[2] = G[6] << 7; G[2] = G[2] - G[3]; G[2] = G[2] - G[6]; G[3] = 0xFFFFFFFFu; { const bool tk = G[6] != G[3]; G[2] = G[2] << 3; if (tk) goto L_088DBA10; } L_088DB9E8: ; G[25] = word(rt, G[20] + 0x118u); tail = G[25]; goto exit_; L_088DBA10: ; G[17] = word(rt, G[20] + 0xC0u); G[6] = word(rt, G[18] + 0xCu); G[5] = word(rt, G[5] + 0x8u); G[17] = G[17] + G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088E7E94u); G[5] = word(rt, G[18] + 0x0u); G[7] = word(rt, G[18] + 0x14u); G[6] = G[19]; G[4] = G[17]; call_here(rt, ctx, 0x088E7F94u); G[3] = sext(load8(G[19] + 0x0u), 8); G[16] = word(rt, G[20] + 0x77Cu); G[4] = G[21]; G[2] = G[3] << 6; G[3] = G[3] << 3; G[2] = G[2] - G[3]; G[16] = G[16] + G[2]; call_here(rt, ctx, 0x08B344E0u); G[5] = sext(load8(G[19] + 0x0u), 8); G[4] = G[21]; put(rt, G[16] + 0x20u, G[5]); G[6] = G[5] << 5; G[2] = G[5] << 7; G[3] = word(rt, G[20] + 0xC0u); G[2] = G[2] - G[6]; G[2] = G[2] - G[5]; G[2] = G[2] << 3; G[3] = G[3] + G[2]; put(rt, G[16] + 0x1Cu, G[3]); G[2] = word(rt, G[20] + 0x118u); put(rt, G[16] + 0x24u, G[2]); G[2] = word(rt, G[18] + 0x0u); G[3] = word(rt, G[18] + 0x4u); G[5] = word(rt, G[18] + 0x8u); G[9] = word(rt, G[18] + 0x18u); G[6] = word(rt, G[18] + 0xCu); G[7] = word(rt, G[18] + 0x10u); G[8] = word(rt, G[18] + 0x14u); put(rt, G[16] + 0x0u, G[2]); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x4u, G[3]); G[3] = 0xFFFFFFFFu; put(rt, G[16] + 0x8u, G[5]); put(rt, G[16] + 0x28u, G[20]); put(rt, G[16] + 0xCu, G[6]); put(rt, G[16] + 0x10u, G[7]); put(rt, G[16] + 0x14u, G[8]); put(rt, G[16] + 0x18u, G[9]); put(rt, G[16] + 0x30u, G[2]); put(rt, G[16] + 0x34u, G[3]); store8(G[16] + 0x2Cu, 0u); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[17]; G[5] = 0u; tail = 0x088E8A84u; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[21] = word(rt, frame.at(0x14u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A04354 (a0 = the result, a1 = the argument count, a2 = the arguments): with at least 3 arguments, the first two are read as colours
// (0x08A03E58 into the frame) and the third's virtual +0x54 gives the weight t; the result is the colour t * first + (1 - t) * second
// (0x08A042F4 builds it, 0x08A5BAD8 assigns it). A missing value reports through 0x08A5BB7C.
void func_08A04354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[5], arguments = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x3Cu}, {16u, 0x30u}, {31u, 0x48u}, {21u, 0x44u}, {20u, 0x40u}, {18u, 0x38u}, {17u, 0x34u}, {52u, 0x50u}});
    auto &F = ctx.fpr;
    ctx.gpr[5] = static_cast<s32>(count) < 3 ? 1u : 0u;
    ctx.gpr[19] = result;
    ctx.gpr[16] = arguments;
    if (static_cast<s32>(count) < 3) return;
    const u32 first = frame.at(0x4u), second = frame.at(0x8u), made = frame.at(0u);
    ctx.gpr[21] = first;
    ctx.gpr[20] = second;
    call_set(rt, ctx, 0x08A03E58u, {first, arguments});
    call_set(rt, ctx, 0x08A03E58u, {second, arguments + 0x4u});
    u32 weight_object = word(rt, arguments + 0x8u);
    ctx.gpr[16] = weight_object;
    if (weight_object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D38u});
    const u32 method = word(rt, word(rt, weight_object) + 0x54u);
    ctx.gpr[3] = word(rt, weight_object);
    ctx.gpr[2] = method;
    call_set(rt, ctx, method, {weight_object});
    const u32 a = word(rt, first);
    ctx.gpr[18] = a;
    F[20] = F[0];
    if (a == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D40u});
    const u32 b = word(rt, second);
    ctx.gpr[16] = b;
    ctx.gpr[17] = a + 0x20u;
    if (b == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D40u});
    const float t = F[20], rest = 1.0f - t;
    const float x = psp_mul(t, float_at(rt, a + 0x20u)) + psp_mul(rest, float_at(rt, b + 0x20u));
    const float y = psp_mul(t, float_at(rt, a + 0x24u)) + psp_mul(rest, float_at(rt, b + 0x24u));
    const float z = psp_mul(t, float_at(rt, a + 0x28u)) + psp_mul(rest, float_at(rt, b + 0x28u));
    const float w = psp_mul(t, float_at(rt, a + 0x2Cu)) + psp_mul(rest, float_at(rt, b + 0x2Cu));
    F[0] = psp_mul(rest, float_at(rt, b + 0x28u));
    F[1] = psp_mul(rest, float_at(rt, b + 0x24u));
    F[2] = z;
    F[3] = y;
    F[4] = x;
    F[5] = psp_mul(rest, float_at(rt, b + 0x20u));
    F[6] = psp_mul(rest, float_at(rt, b + 0x2Cu));
    F[7] = w;
    F[8] = float_at(rt, b + 0x28u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    put_float(rt, frame.at(0x10u), x);
    put_float(rt, frame.at(0x14u), y);
    put_float(rt, frame.at(0x18u), z);
    put_float(rt, frame.at(0x1Cu), w);
    put(rt, frame.at(0x20u), 0xFFFFFFFFu);
    call_set(rt, ctx, 0x08A042F4u, {made, frame.at(0x10u)});
    if (result != made) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, made)});
    call_set(rt, ctx, 0x08A5B9ECu, {made});
    call_set(rt, ctx, 0x08A5B9ECu, {second});
    call_set(rt, ctx, 0x08A5B9ECu, {first});
}
void func_08A068FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { insert_record(rt, ctx, true); }
// 0x08A2CB48 (a0 = the result, a1 = the argument count, a2 = the arguments): with two arguments, each one's string (its virtual +0xC into the
// frame) is converted (0x08C1A254 / 0x08C1A18C and 0x08C1A28C / 0x08C1A1B8 on a scratch object, 0x08C1A2E4 / 0x08C1A2F4 end them); a non-zero
// first value gives the result 0x08A44D9C(first, second, 0) (0x08A5BAD8 assigns it). The two strings are released (shared count 0x08B60804,
// freed by 0x08B8542C unless it is the empty string 0x08D74FD0).
void func_08A2CB48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[5], arguments = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x4Cu}, {17u, 0x44u}, {31u, 0x50u}, {18u, 0x48u}, {16u, 0x40u}});
    ctx.gpr[5] = static_cast<s32>(count) < 2 ? 1u : 0u;
    ctx.gpr[19] = result;
    ctx.gpr[17] = arguments;
    if (static_cast<s32>(count) < 2) return;
    auto text_of = [&](u32 argument, u32 out, u32 scratch) {
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC140u});
        const u32 method = word(rt, word(rt, argument) + 0xCu);
        ctx.gpr[2] = word(rt, argument);
        ctx.gpr[3] = method;
        ctx.gpr[16] = scratch;
        call_set(rt, ctx, method, {out, argument});
    };
    const u32 first_text = frame.at(0x8u), second_text = frame.at(0xCu);
    text_of(word(rt, arguments + 0x0u), first_text, frame.at(0x28u));
    call_set(rt, ctx, 0x08C1A254u, {frame.at(0x28u), word(rt, first_text)});
    const u32 first = call_set(rt, ctx, 0x08C1A18Cu, {frame.at(0x28u)});
    ctx.gpr[18] = first;
    call_set(rt, ctx, 0x08C1A2E4u, {frame.at(0x28u)});
    text_of(word(rt, arguments + 0x4u), second_text, frame.at(0x10u));
    call_set(rt, ctx, 0x08C1A28Cu, {frame.at(0x10u), word(rt, second_text)});
    const u32 second = call_set(rt, ctx, 0x08C1A1B8u, {frame.at(0x10u)});
    ctx.gpr[17] = second;
    call_set(rt, ctx, 0x08C1A2F4u, {frame.at(0x10u)});
    const u32 made = frame.at(0x4u);
    ctx.gpr[16] = made;
    if (first != 0u) {
        call_set(rt, ctx, 0x08A44D9Cu, {made, first, second, 0u});
        if (result != made) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, made)});
        call_set(rt, ctx, 0x08A5B9ECu, {made});
    }
    const u32 empty = 0x08D74FD0u;
    ctx.gpr[2] = 0x08D70000u;
    ctx.gpr[17] = empty;
    auto release = [&](u32 text_slot, u32 tag) {
        const u32 data = word(rt, text_slot);
        ctx.gpr[4] = data;
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        if (header == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, tag});
    };
    release(second_text, frame.at(0x1u));
    release(first_text, frame.at(0u));
}
void func_08B0BB3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x58u), G[18]); put(rt, frame.at(0x54u), G[17]); put(rt, frame.at(0x5Cu), G[31]); put(rt, frame.at(0x50u), G[16]); G[18] = G[4]; G[2] = load8(G[4] + 0x8u); { const bool tk = G[2] == 0u; G[17] = G[6]; if (tk) goto L_08B0BB8C; } G[7] = word(rt, G[4] + 0x10u); G[2] = word(rt, G[4] + 0xCu); G[3] = 0x1u; G[4] = G[6]; G[6] = frame.at(0x18u); put(rt, frame.at(0x18u), G[3]); put(rt, frame.at(0x1Cu), G[2]); put(rt, frame.at(0x20u), G[7]); call_here(rt, ctx, 0x08B0C9ACu); G[2] = word(rt, G[17] + 0x4u); goto L_08B0BBA0; L_08B0BB8C: ; G[4] = G[6]; G[7] = 0u; G[6] = 0u; call_here(rt, ctx, 0x08B0C99Cu); G[2] = word(rt, G[17] + 0x4u); L_08B0BBA0: ; { const bool tk = G[2] != 0u; G[3] = 0u; if (tk) goto L_08B0BCA0; } G[2] = frame.at(0x24u); G[16] = 0x10u; G[4] = G[17]; G[5] = frame.at(0x10u); put(rt, frame.at(0x10u), G[2]); put(rt, frame.at(0x14u), G[16]); call_here(rt, ctx, 0x08B0C650u); G[2] = static_cast<std::int32_t>(G[2]) < 16 ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = 0x33620000u; if (tk) goto L_08B0BC04; } G[3] = word(rt, frame.at(0x24u)); G[2] = G[2] | 0x7067u; G[3] = ((G[3] & 0xFFu) << 24u) | ((G[3] & 0xFF00u) << 8u) | ((G[3] & 0xFF0000u) >> 8u) | ((G[3] & 0xFF000000u) >> 24u); { const bool tk = G[3] != G[2]; G[2] = frame.at(0x34u); if (tk) goto L_08B0BC04; } G[4] = G[17]; G[5] = frame.at(0x8u); put(rt, frame.at(0x8u), G[2]); put(rt, frame.at(0xCu), G[16]); call_here(rt, ctx, 0x08B0C650u); G[2] = static_cast<std::int32_t>(G[2]) < 16 ? 1u : 0u; if (G[2] == 0u) { G[5] = word(rt, frame.at(0x3Cu)); goto L_08B0BC14; } L_08B0BC04: ; G[4] = G[17]; call_here(rt, ctx, 0x08B0C778u); G[3] = 0u; goto L_08B0BCA0; L_08B0BC14: ; G[4] = 0x10u; G[5] = ((G[5] & 0xFFu) << 24u) | ((G[5] & 0xFF00u) << 8u) | ((G[5] & 0xFF0000u) >> 8u) | ((G[5] & 0xFF000000u) >> 24u); call_here(rt, ctx, 0x08B2B4D8u); G[16] = G[2]; G[5] = word(rt, frame.at(0x28u)); G[6] = word(rt, frame.at(0x2Cu)); G[7] = word(rt, frame.at(0x30u)); G[8] = word(rt, frame.at(0x34u)); G[9] = word(rt, frame.at(0x38u)); G[10] = word(rt, frame.at(0x3Cu)); G[11] = word(rt, frame.at(0x40u)); G[2] = word(rt, frame.at(0x24u)); put(rt, G[16] + 0x4u, G[5]); G[3] = G[16] + 0x20u; G[4] = G[17]; put(rt, G[16] + 0x8u, G[6]); G[5] = frame.at(0u); put(rt, G[16] + 0xCu, G[7]); put(rt, G[16] + 0x10u, G[8]); put(rt, G[16] + 0x14u, G[9]); put(rt, G[16] + 0x18u, G[10]); put(rt, G[16] + 0x1Cu, G[11]); put(rt, G[16] + 0x0u, G[2]); G[2] = word(rt, frame.at(0x3Cu)); put(rt, frame.at(0x0u), G[3]); G[2] = ((G[2] & 0xFFu) << 24u) | ((G[2] & 0xFF00u) << 8u) | ((G[2] & 0xFF0000u) >> 8u) | ((G[2] & 0xFF000000u) >> 24u); G[2] = G[2] + 0xFFFFFFE0u; put(rt, frame.at(0x4u), G[2]); call_here(rt, ctx, 0x08B0C650u); G[4] = G[16]; G[5] = G[16]; call_here(rt, ctx, 0x08B0B1F8u); put(rt, G[18] + 0x4u, G[16]); G[3] = 0x1u; put(rt, G[18] + 0x0u, 0u); L_08B0BCA0: ; G[2] = G[3]; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x58u)); G[17] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x50u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B4665C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[20] = frame.at(0x8u); G[19] = G[5]; G[17] = G[4]; G[4] = G[20]; G[18] = G[17] + 0x8Du; G[16] = G[6] & 0xFFu; call_here(rt, ctx, 0x08B3FA98u); G[4] = word(rt, G[19] + 0x0u); G[9] = 0x2400u; G[5] = G[18]; G[6] = frame.at(0x4u); G[7] = 0u; G[8] = 0x1u; put(rt, frame.at(0x4u), G[9]); call_here(rt, ctx, 0x08C7AE30u); G[3] = G[2]; G[2] = 0x80410000u; G[2] = G[2] | 0x709u; { const bool tk = G[3] == G[2]; if (tk) goto L_08B467C0; } if (static_cast<std::int32_t>(G[3]) < 0) { goto L_08B467C4; } if (G[16] != 0u) { goto L_08B467C4; } G[4] = G[20]; G[5] = G[19] + 0x1Au; call_here(rt, ctx, 0x08B3F7CCu); G[2] = word(rt, G[17] + 0x4u); { const bool tk = G[2] == 0u; if (tk) goto L_08B467C0; } G[2] = word(rt, G[19] + 0x34u); if (G[2] == 0u) { G[9] = word(rt, frame.at(0x4u)); goto L_08B46730; } G[6] = word(rt, frame.at(0x4u)); G[4] = G[18] + G[2]; G[5] = G[18]; call_here(rt, ctx, 0x08B79BACu); G[5] = word(rt, G[19] + 0x40u); G[6] = word(rt, G[19] + 0x34u); G[4] = G[18]; call_here(rt, ctx, 0x08B79A88u); G[3] = word(rt, G[19] + 0x34u); G[9] = word(rt, frame.at(0x4u)); put(rt, G[19] + 0x34u, 0u); G[9] = G[3] + G[9]; put(rt, frame.at(0x4u), G[9]); L_08B46730: ; G[16] = 0u; G[8] = G[18] + G[16]; L_08B46738: ; G[2] = load8(G[8] + 0x0u); G[9] = G[9] - G[16]; G[6] = G[8]; store8(frame.at(0x0u), G[2]); G[4] = G[17]; G[3] = load8(G[8] + 0x1u); store8(frame.at(0x1u), G[3]); G[2] = load16(frame.at(0x0u)); G[2] = G[2] + 0x1Cu; G[2] = G[2] & 0xFFFFu; G[7] = G[2]; G[16] = G[16] + G[2]; G[2] = static_cast<std::int32_t>(G[9]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[5] = G[20]; if (tk) goto L_08B4679C; } G[2] = word(rt, G[19] + 0x34u); if (G[2] != 0u) { goto L_08B467C0; } G[4] = word(rt, G[19] + 0x40u); put(rt, G[19] + 0x34u, G[9]); G[5] = G[8]; G[6] = G[9]; call_here(rt, ctx, 0x08B79A88u); goto L_08B467C0; L_08B4679C: ; G[2] = word(rt, G[17] + 0x0u); G[3] = word(rt, G[2] + 0xB0u); { const std::uint32_t tg = G[3]; call_here(rt, ctx, tg); } G[9] = word(rt, frame.at(0x4u)); G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[9]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[8] = G[18] + G[16]; if (tk) goto L_08B46738; } L_08B467C0: ; L_08B467C4: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B99974 / 0x08B9A0C0 share this body: a destructor (a0 = the object). The string at +0x30 is released (shared count 0x08B60804 on its
// header, freed by 0x08B93554 unless it is the empty string 0x08D75298); the two handles at +0x14..+0x24 (pairs, last first) drop a use and a
// weak count (ll / sc decrements) calling the held object's virtual +8 / +0xC when each reaches zero; the two strings at +0xC / +0x10 are released
// last first; the vtables 0x08CBFCF8, 0x08CBFD38, 0x08CBFB70 are set on the way. `base`: 0x08B64654 destroys the base class after.
void destroy_handles(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool base) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {20u, 0x20u}, {16u, 0x10u}, {31u, 0x28u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}});
    const u32 empty = 0x08D75298u;
    ctx.gpr[21] = empty;
    ctx.gpr[20] = self;
    auto release = [&](u32 data, u32 tag) {
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        ctx.gpr[4] = data - 0x4u;
        if (header == empty) return;
        const u32 left = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        ctx.gpr[4] = header;
        ctx.gpr[5] = tag;
        if (static_cast<s32>(left) > 0) return;
        call_set(rt, ctx, 0x08B93554u, {header, tag});
    };
    put(rt, self + 0x0u, 0x08CBFCF8u);
    release(word(rt, self + 0x30u), frame.at(0x1u));
    put(rt, self + 0x0u, 0x08CBFD38u);
    const u32 handles = self + 0x14u;
    ctx.gpr[19] = handles;
    ctx.gpr[18] = 1u;
    // ll / sc: the decrement always succeeds here (nothing else runs); true when the count was 1.
    auto drop = [&](u32 address) {
        const u32 old = word(rt, address);
        put(rt, address, old - 1u);
        ctx.gpr[4] = old;
        ctx.gpr[2] = 1u;
        return old == 1u;
    };
    u32 pair = self + 0x24u;
    for (;;) {
        const bool done = pair == handles;
        pair -= 8u;
        ctx.gpr[17] = pair;
        if (done) break;
        const u32 held = word(rt, pair + 0x4u);
        ctx.gpr[16] = held;
        ctx.gpr[3] = held + 0x4u;
        if (held == 0u) continue;
        if (!drop(held + 0x4u)) continue;
        const u32 dispose = word(rt, word(rt, held) + 0x8u);
        ctx.gpr[3] = word(rt, held);
        ctx.gpr[2] = dispose;
        call_set(rt, ctx, dispose, {held});
        ctx.gpr[3] = held + 0x8u;
        if (!drop(held + 0x8u)) continue;
        const u32 destroy = word(rt, word(rt, held) + 0xCu);
        ctx.gpr[3] = word(rt, held);
        ctx.gpr[2] = destroy;
        call_set(rt, ctx, destroy, {held});
    }
    const u32 strings = self + 0xCu;
    ctx.gpr[18] = strings;
    for (u32 at = handles; at != strings;) {
        const u32 slot = at - 0x4u;
        ctx.gpr[17] = slot;
        ctx.gpr[2] = word(rt, slot);
        ctx.gpr[5] = 0xFFFFFFFFu;
        release(word(rt, slot), frame.at(0u));
        at = slot;
        ctx.gpr[3] = at;
    }
    ctx.gpr[2] = 0x08CBFB70u;
    put(rt, self + 0x0u, 0x08CBFB70u);
    if (base) call_set(rt, ctx, 0x08B64654u, {self});
}

void func_08B99974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_handles(rt, ctx, true); }
// 0x08C44CD8: function-local statics, each behind its guard byte (0x08B6441C acquires, 0x08B64494 releases): the object at 0x08D772D8
// (vtable 0x08CD7994, link 0x08D772E4) needs 0x08D772E4 = 0x08D772AC (vtable 0x08CD7988, link 0x08D772B8), which needs 0x08D772B8 =
// 0x08D7729C (vtable 0x08CD7988, link 0x08D772A8). v0 = 0x08D772D8.
void func_08C44CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    // True when the guard's static still has to be built (its byte clear and the guard acquired).
    auto first_time = [&](u32 guard) {
        ctx.gpr[4] = guard;
        if (signed_byte(rt, guard) != 0u) return false;
        return call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u;
    };
    auto build = [&](u32 object, u32 vtable, u32 link, u32 guard) {
        put(rt, object + 0x8u, vtable);
        put(rt, object + 0x4u, link);
        put(rt, object + 0x0u, 0u);
        ctx.gpr[2] = object;
        ctx.gpr[3] = vtable;
        ctx.gpr[5] = link;
        call_set(rt, ctx, 0x08B64494u, {guard});
    };
    ctx.gpr[16] = 0x08D70000u;
    if (first_time(0x08D74F98u)) {
        ctx.gpr[17] = 0x08D70000u;
        if (first_time(0x08D74FA8u)) {
            ctx.gpr[18] = 0x08D74FC8u;
            ctx.gpr[19] = 0x08D70000u;
            if (first_time(0x08D74FC8u)) build(0x08D7729Cu, 0x08CD7988u, 0x08D772A8u, 0x08D74FC8u);
            ctx.gpr[3] = 0x08D7729Cu;
            put(rt, 0x08D772B8u, 0x08D7729Cu);
            call_set(rt, ctx, 0x08B64494u, {0x08D74FA8u});
        }
        ctx.gpr[17] = 0x08D74FA0u;
        ctx.gpr[18] = 0x08D70000u;
        if (first_time(0x08D74FA0u)) build(0x08D772ACu, 0x08CD7988u, 0x08D772B8u, 0x08D74FA0u);
        ctx.gpr[3] = 0x08D772ACu;
        put(rt, 0x08D772E4u, 0x08D772ACu);
        call_set(rt, ctx, 0x08B64494u, {0x08D74F98u});
    }
    ctx.gpr[16] = 0x08D74F90u;
    ctx.gpr[17] = 0x08D70000u;
    if (first_time(0x08D74F90u)) build(0x08D772D8u, 0x08CD7994u, 0x08D772E4u, 0x08D74F90u);
    ctx.gpr[2] = 0x08D772D8u;
}
void func_08C49524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x4Cu), G[23]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x50u), G[30]); put(rt, frame.at(0x48u), G[22]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); auto &F = ctx.fpr; G[2] = G[5] - G[4]; G[23] = G[5]; G[20] = sra(G[2], 1); G[3] = static_cast<std::int32_t>(G[20]) < 2 ? 1u : 0u; G[19] = G[4]; G[30] = word(rt, G[7] + 0x0u); G[22] = word(rt, G[7] + 0x4u); put(rt, frame.at(0x20u), G[6]); put(rt, frame.at(0xCu), G[22]); { const bool tk = G[3] != 0u; put(rt, frame.at(0x8u), G[30]); if (tk) goto L_08C495C4; } G[2] = G[20] + 0xFFFFFFFEu; G[3] = G[2] >> 31; G[3] = G[3] + G[2]; G[18] = sra(G[3], 1); G[2] = G[18] << 1; G[16] = G[4] + G[2]; G[17] = 0u; G[21] = frame.at(0x8u); L_08C4959C: ; G[7] = sext(load16(G[16] + 0x0u), 16); G[5] = G[18] - G[17]; G[4] = G[19]; G[6] = G[20]; G[8] = G[21]; call_here(rt, ctx, 0x08C4935Cu); { const bool tk = G[18] == G[17]; G[16] = G[16] + 0xFFFFFFFEu; if (tk) goto L_08C495C4; } G[17] = G[17] + 0x1u; goto L_08C4959C; L_08C495C4: ; G[3] = word(rt, frame.at(0x20u)); G[2] = G[23] < G[3] ? 1u : 0u; { const bool tk = G[2] == 0u; G[4] = G[19]; if (tk) goto L_08C49660; } G[3] = G[23]; G[16] = G[23]; G[17] = G[30] << 2; L_08C495E0: ; G[3] = sext(load16(G[3] + 0x0u), 16); G[9] = sext(load16(G[19] + 0x0u), 16); G[4] = word(rt, G[22] + 0x0u); G[5] = G[3] << 8; G[6] = G[9] << 4; G[3] = G[3] << 4; G[2] = G[9] << 8; G[5] = G[5] - G[3]; G[2] = G[2] - G[6]; G[2] = G[4] + G[2]; G[4] = G[4] + G[5]; G[4] = G[4] + G[17]; G[2] = G[2] + G[17]; F[1] = float_at(rt, G[4] + 0x98u); F[0] = float_at(rt, G[2] + 0x98u); G[4] = G[19]; G[5] = 0u; ctx.set_fpu_condition(F[1] < F[0]); G[6] = G[20]; { const bool tk = !ctx.fpu_condition(); G[8] = frame.at(0u); if (tk) goto L_08C49648; } G[7] = sext(load16(G[16] + 0x0u), 16); put(rt, frame.at(0x4u), G[22]); put(rt, frame.at(0x0u), G[30]); store16(G[16] + 0x0u, G[9]); call_here(rt, ctx, 0x08C4935Cu); L_08C49648: ; G[4] = word(rt, frame.at(0x20u)); G[16] = G[16] + 0x2u; G[2] = G[16] < G[4] ? 1u : 0u; { const bool tk = G[2] != 0u; G[3] = G[16]; if (tk) goto L_08C495E0; } G[4] = G[19]; L_08C49660: ; G[5] = G[23]; G[6] = frame.at(0x10u); put(rt, frame.at(0x10u), G[30]); put(rt, frame.at(0x14u), G[22]); call_here(rt, ctx, 0x08C49480u); goto exit_;  exit_: ; G[23] = word(rt, frame.at(0x4Cu)); G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[30] = word(rt, frame.at(0x50u)); G[22] = word(rt, frame.at(0x48u)); G[21] = word(rt, frame.at(0x44u)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_326
} // namespace gt::game

#define HOT326(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_326::name, gt::ReturnKind::kind)
// HOT326(0x089AA1C0u, func_089AA1C0, Word);  // no comparable synthetic case yet
// HOT326(0x08A816C4u, func_08A816C4, Word);  // no comparable synthetic case yet
HOT326(0x088A045Cu, func_088A045C, Word);
HOT326(0x088BD0C4u, func_088BD0C4, Word);
HOT326(0x089E8ADCu, func_089E8ADC, Word);
HOT326(0x08A00E2Cu, func_08A00E2C, Word);
// HOT326(0x08AE6268u, func_08AE6268, Word);  // no comparable synthetic case yet
HOT326(0x08B9A0C0u, func_08B9A0C0, Word);
// HOT326(0x088C896Cu, func_088C896C, Word);  // no comparable synthetic case yet
HOT326(0x089167E8u, func_089167E8, Word);
HOT326(0x08A06780u, func_08A06780, Word);
HOT326(0x08AE430Cu, func_08AE430C, Word);
HOT326(0x08AEE744u, func_08AEE744, Word);
HOT326(0x08B41954u, func_08B41954, Word);
HOT326(0x08C2EDC4u, func_08C2EDC4, Word);
HOT326(0x088C93C4u, func_088C93C4, Word);
HOT326(0x088DB990u, func_088DB990, Word);
HOT326(0x08A04354u, func_08A04354, Word);
HOT326(0x08A068FCu, func_08A068FC, Word);
HOT326(0x08A2CB48u, func_08A2CB48, Word);
// HOT326(0x08B0BB3Cu, func_08B0BB3C, Word);  // no comparable synthetic case yet
// HOT326(0x08B4665Cu, func_08B4665C, Word);  // no comparable synthetic case yet
HOT326(0x08B99974u, func_08B99974, Word);
HOT326(0x08C44CD8u, func_08C44CD8, Word);
HOT326(0x08C49524u, func_08C49524, Word);
