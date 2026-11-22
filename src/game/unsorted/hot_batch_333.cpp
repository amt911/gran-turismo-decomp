// Hot batch 333: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
// Fourteen of them (0x08B6E74C, 0x08BFA724, 0x08A94DE0, 0x08B7B0CC, 0x0894C294, 0x08BFA134, 0x0894AA7C, 0x0883E058, 0x089B00BC, 0x08844D1C, 0x08890788,
// 0x08AEE934, 0x088205BC, 0x0883FD7C) are rewritten as structured C from the disassembly (structured rewrites, batch 83); the others keep their emitter
// bodies: 0x08AEAB24 was also rewritten but the synthetic gate compares it once, in a run where its sabotage is not pinned, and the rest are never compared.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_333 {
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

void func_088D10E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); auto &F = ctx.fpr; G[18] = G[6] + 0x4u; G[16] = G[4]; G[4] = G[5]; G[22] = G[7]; G[21] = G[8]; G[20] = G[6]; G[19] = G[5]; call_here(rt, ctx, 0x088D201Cu); G[4] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x088D62FCu); G[3] = 0x1u; if (G[2] != G[3]) { G[6] = word(rt, G[16] + 0x70u); goto L_088D1150; } G[2] = word(rt, G[16] + 0x14u); if (static_cast<std::int32_t>(G[2]) <= 0) { put(rt, G[16] + 0x70u, G[17]); goto L_088D114C; } L_088D114C: ; G[6] = word(rt, G[16] + 0x70u); L_088D1150: ; G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x088D0974u); put(rt, G[16] + 0x74u, G[2]); G[4] = G[18]; G[5] = 0u; call_here(rt, ctx, 0x088D6218u); F[0] = std::bit_cast<float>(G[2]); G[17] = load8(G[16] + 0x24u); G[2] = 0x1u; { const bool tk = G[17] != G[2]; F[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); if (tk) goto L_088D1284; } G[2] = word(rt, G[16] + 0x10u); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; F[0] = std::bit_cast<float>(0u); if (tk) goto L_088D1284; } ctx.set_fpu_condition(F[1] < F[0]); { const bool tk = !ctx.fpu_condition(); if (tk) goto L_088D1288; } F[12] = float_at(rt, G[16] + 0x28u); G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x088D0C80u); G[18] = G[2]; G[2] = load8(G[19] + 0x0u); if (G[2] == 0u) { G[2] = load8(G[16] + 0x34u); goto L_088D11E0; } G[6] = word(rt, G[16] + 0x70u); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x088D0974u); G[3] = word(rt, G[16] + 0x194u); put(rt, G[16] + 0x78u, G[2]); G[2] = G[2] + G[3]; put(rt, G[16] + 0x194u, G[2]); G[2] = load8(G[16] + 0x34u); L_088D11E0: ; if (G[2] != G[17]) { put(rt, G[16] + 0x70u, G[18]); goto L_088D1214; } G[2] = word(rt, G[16] + 0x2Cu); if (G[2] != 0u) { put(rt, G[16] + 0x70u, G[18]); goto L_088D1214; } G[5] = G[21]; G[6] = G[18]; G[4] = G[16]; call_here(rt, ctx, 0x088D0934u); G[4] = G[19]; call_here(rt, ctx, 0x088D201Cu); put(rt, G[16] + 0x140u, G[2]); put(rt, G[16] + 0x70u, G[18]); L_088D1214: ; G[4] = G[19]; G[5] = G[22]; call_here(rt, ctx, 0x088D1F88u); { const bool tk = G[2] == 0u; if (tk) goto L_088D1288; } G[4] = G[20]; G[5] = G[22]; call_here(rt, ctx, 0x088DD0E4u); { const bool tk = G[2] != 0u; if (tk) goto L_088D1288; } G[3] = word(rt, G[16] + 0x78u); G[2] = word(rt, G[16] + 0x7Cu); G[2] = G[3] < G[2] ? 1u : 0u; if (G[2] == 0u) { goto L_088D128C; } G[2] = load8(G[16] + 0x198u); if (G[2] != 0u) { goto L_088D128C; } { const bool tk = static_cast<std::int32_t>(G[21]) < 0; put(rt, G[16] + 0x7Cu, G[3]); if (tk) goto L_088D1284; } G[4] = G[16]; G[5] = 0u; L_088D126C: ; G[2] = word(rt, G[4] + 0xC0u); G[5] = G[5] + 0x1u; G[3] = static_cast<std::int32_t>(G[21]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; put(rt, G[4] + 0x80u, G[2]); { const bool tk = G[3] == 0u; G[4] = G[4] + 0x4u; if (tk) goto L_088D126C; } L_088D1284: ; L_088D1288: ; L_088D128C: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089B00BC (a0 = an owner): under the lock at 0x08CF4A04 (0x08B3453C / 0x08B3457C; nothing happens when the lock is not taken) it fetches two
// handles of the owner (0x089AA48C -> A, 0x089AB068 -> B; each is referenced with 0x08A5BA08 when it exists). Whichever of the two has
// a non-null +4 is "live". When the live ones differ: a live A is copied to a temporary (0x089F5948) and passed to 0x0897783C with the owner's
// +0x14, then B (live or not) is handed to 0x089B0044(owner, live B), and a live B goes through the same copy (0x089F5F9C -> 0x0897783C).
// Both handles are released (0x08A5B930) at the end. v0 = the unlock call's result (0 when the lock was not taken).
void func_089B00BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {19u, 0x1Cu}, {31u, 0x28u}, {20u, 0x20u}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    const u32 lock = 0x08CF4A04u;
    ctx.gpr[21] = 0x08CF0000u;
    ctx.gpr[19] = owner;
    if (call_set(rt, ctx, 0x08B3453Cu, {lock}) == 0u) return;
    const u32 handle_a = call_set(rt, ctx, 0x089AA48Cu, {owner});
    ctx.gpr[18] = handle_a;
    if (handle_a != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle_a});
    const u32 handle_b = call_set(rt, ctx, 0x089AB068u, {owner});
    ctx.gpr[17] = handle_b;
    if (handle_b != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle_b});
    // A handle counts only when it is non-null and its +4 is set.
    const bool live_a = handle_a != 0u && word(rt, handle_a + 0x4u) != 0u;
    const bool live_b = handle_b != 0u && word(rt, handle_b + 0x4u) != 0u;
    if ((live_a ? handle_a : 0u) != (live_b ? handle_b : 0u)) {
        if (live_a) {
            ctx.gpr[20] = word(rt, owner + 0x14u);
            ctx.gpr[6] = handle_a;
            ctx.gpr[16] = frame.at(0x4u);
            call_set(rt, ctx, 0x089F5948u, {frame.at(0x4u), owner, handle_a});
            call_set(rt, ctx, 0x0897783Cu, {ctx.gpr[20], frame.at(0x4u)});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
        }
        call_set(rt, ctx, 0x089B0044u, {owner, live_b ? handle_b : 0u});
        if (live_b) {
            ctx.gpr[6] = handle_b;
            ctx.gpr[16] = word(rt, owner + 0x14u);
            call_set(rt, ctx, 0x089F5F9Cu, {frame.at(0x0u), owner, handle_b});
            call_set(rt, ctx, 0x0897783Cu, {ctx.gpr[16], frame.at(0x0u)});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
        }
    }
    if (handle_b != 0u) call_set(rt, ctx, 0x08A5B930u, {handle_b});
    if (handle_a != 0u) call_set(rt, ctx, 0x08A5B930u, {handle_a});
    call_set(rt, ctx, 0x08B3457Cu, {lock});
}
void func_08B7AE14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CE0000u; G[3] = word(rt, 0x08CE0E60u); { const bool tk = G[3] == G[4]; G[17] = G[4]; if (tk) goto L_08B7AF28; } G[6] = word(rt, G[4] + 0x40u); { const bool tk = G[6] == 0u; G[5] = G[6]; if (tk) goto L_08B7AE68; } G[2] = word(rt, G[4] + 0x44u); put(rt, G[6] + 0x4u, G[2]); G[2] = 0x1u; G[3] = word(rt, G[4] + 0x44u); G[2] = G[2] << (G[3] & 31u); put(rt, G[6] + 0x8u, G[2]); call_here(rt, ctx, 0x08B82318u); L_08B7AE68: ; G[5] = word(rt, G[17] + 0x48u); if (G[5] == 0u) { G[3] = word(rt, G[17] + 0x4Cu); goto L_08B7AE8C; } L_08B7AE74: ; G[16] = word(rt, G[5] + 0x0u); G[4] = G[17]; call_here(rt, ctx, 0x08B82318u); { const bool tk = G[16] != 0u; G[5] = G[16]; if (tk) goto L_08B7AE74; } G[3] = word(rt, G[17] + 0x4Cu); L_08B7AE8C: ; if (G[3] == 0u) { G[5] = word(rt, G[17] + 0x148u); goto L_08B7AEE0; } G[18] = 0u; G[19] = 0x3Cu; G[2] = G[3] + G[18]; L_08B7AEA0: ; G[5] = word(rt, G[2] + 0x0u); if (G[5] == 0u) { G[18] = G[18] + 0x4u; goto L_08B7AEC8; } L_08B7AEAC: ; G[16] = word(rt, G[5] + 0x0u); G[4] = G[17]; call_here(rt, ctx, 0x08B354A4u); { const bool tk = G[16] != 0u; G[5] = G[16]; if (tk) goto L_08B7AEAC; } G[3] = word(rt, G[17] + 0x4Cu); G[18] = G[18] + 0x4u; L_08B7AEC8: ; { const bool tk = G[18] != G[19]; G[2] = G[3] + G[18]; if (tk) goto L_08B7AEA0; } G[5] = G[3]; G[4] = G[17]; call_here(rt, ctx, 0x08B354A4u); G[5] = word(rt, G[17] + 0x148u); L_08B7AEE0: ; if (G[5] == 0u) { G[5] = word(rt, G[17] + 0x54u); goto L_08B7AF0C; } G[18] = G[17] + 0x14Cu; if (G[5] == G[18]) { G[5] = word(rt, G[17] + 0x54u); goto L_08B7AF0C; } L_08B7AEF4: ; G[16] = word(rt, G[5] + 0x0u); G[4] = G[17]; call_here(rt, ctx, 0x08B354A4u); { const bool tk = G[18] != G[16]; G[5] = G[16]; if (tk) goto L_08B7AEF4; } G[5] = word(rt, G[17] + 0x54u); L_08B7AF0C: ; if (G[5] == 0u) { G[2] = word(rt, G[17] + 0x38u); goto L_08B7AF20; } G[4] = G[17]; call_here(rt, ctx, 0x08B354A4u); G[2] = word(rt, G[17] + 0x38u); L_08B7AF20: ; if (G[2] != 0u) { G[2] = word(rt, G[17] + 0x3Cu); goto L_08B7AF48; } L_08B7AF28: ; goto exit_; L_08B7AF48: ; G[20] = G[17] + 0x25Cu; { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } if (G[20] == 0u) { G[5] = word(rt, G[17] + 0x25Cu); goto L_08B7AFB4; } G[19] = word(rt, G[20] + 0x4u); L_08B7AF60: ; G[2] = G[19] + 0xFFFFFFFFu; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[16] = word(rt, G[20] + 0x8u); if (tk) goto L_08B7AFA4; } G[18] = 0u; L_08B7AF70: ; G[2] = load16(G[16] + 0xCu); G[2] = G[2] & 0x80u; { const bool tk = G[2] == 0u; G[18] = G[18] + 0x1u; if (tk) goto L_08B7AF9C; } G[2] = word(rt, G[16] + 0x10u); G[4] = G[17]; { const bool tk = G[2] == 0u; G[5] = G[2]; if (tk) goto L_08B7AF9C; } call_here(rt, ctx, 0x08B354A4u); put(rt, G[16] + 0x10u, 0u); L_08B7AF9C: ; { const bool tk = G[19] != G[18]; G[16] = G[16] + 0x5Cu; if (tk) goto L_08B7AF70; } L_08B7AFA4: ; G[20] = word(rt, G[20] + 0x0u); if (G[20] != 0u) { G[19] = word(rt, G[20] + 0x4u); goto L_08B7AF60; } G[5] = word(rt, G[17] + 0x25Cu); L_08B7AFB4: ; { const bool tk = G[5] == 0u; if (tk) goto L_08B7AF28; } G[4] = G[17]; tail = 0x08B7ADCCu; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x4u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0894C294 (a0 = the source record, a1 = the destination): turns five byte fields of the source (+0x113..+0x11B) into the destination's
// float / half / byte fields. A level (+0xFC) of byte(+0x113) / 10 is kept only while the bytes at +0x116, +0x118 and +0x11A are all nonzero,
// else everything is cleared. Otherwise two channels are filled: the first from +0x114/+0x116/+0x118/+0x11A (+0xD8, +0xD4, +0xE4, +0xEC, +0xF4),
// the second from +0x115/+0x117/+0x119/+0x11B (+0xD9, +0xD6, +0xE8, +0xF0, +0xF8) when those three are nonzero. A rate is value / (100 * count)^2
// with the count a signed half. The leaf leaves its temporaries in v0, v1, a0, t0, at and f0-f4 the way the asm does.
void func_0894C294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 src = ctx.gpr[4], dst = ctx.gpr[5];
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    const float zero = 0.0f;
    // `count * 100` as a signed half (the asm builds it with shifts and adds).
    auto scaled = [](u32 value) { return sext((value * 100u) & 0xFFFFu, 16); };
    auto byte_as_float = [](u32 value) { return static_cast<float>(static_cast<s32>(value)); };
    ctx.gpr[6] = src;
    const u32 level_byte = load8(src + 0x113u);
    u32 v0 = 0u;
    bool active = false;
    if (level_byte != 0u) {
        ctx.gpr[1] = 0x3DCCCCCDu;
        ctx.fpr[1] = tenth;
        put_float(rt, dst + 0xFCu, psp_mul(byte_as_float(level_byte), tenth));
        if (load8(src + 0x116u) != 0u && load8(src + 0x118u) != 0u) {
            v0 = load8(src + 0x11Au);
            active = v0 != 0u;
        }
    }
    if (!active) put(rt, dst + 0xFCu, 0u);
    const float level = float_at(rt, dst + 0xFCu);
    ctx.fpr[0] = level;
    ctx.fpr[3] = zero;
    if (level == zero) {
        put_float(rt, dst + 0xF8u, zero);
        store16(dst + 0xD6u, 0u);
        store8(dst + 0xD8u, 0u);
        put_float(rt, dst + 0xE4u, zero);
        put_float(rt, dst + 0xECu, zero);
        put_float(rt, dst + 0xF4u, zero);
        store16(dst + 0xD4u, 0u);
        store8(dst + 0xD9u, 0u);
        put_float(rt, dst + 0xE8u, zero);
        put_float(rt, dst + 0xF0u, zero);
        ctx.gpr[2] = v0;
        return;
    }
    // First channel.
    const u32 kind1 = load8(src + 0x114u);
    store8(dst + 0xD8u, kind1 < 3u ? kind1 : 0u);
    ctx.gpr[3] = kind1 < 3u ? 1u : 0u;
    ctx.gpr[1] = 0x3DCCCCCDu;
    ctx.fpr[4] = tenth;
    const float a_scale = psp_mul(byte_as_float(load8(src + 0x118u)), tenth);
    put_float(rt, dst + 0xE4u, a_scale);
    const u32 b11a = load8(src + 0x11Au);
    put_float(rt, dst + 0xECu, psp_mul(byte_as_float(b11a), tenth));
    const u32 b116 = load8(src + 0x116u);
    put_float(rt, dst + 0xF8u, zero);
    const u32 count1 = scaled(b116);
    store16(dst + 0xD4u, count1);
    store8(dst + 0xD9u, 0u);
    float square = psp_mul(byte_as_float(count1), byte_as_float(count1));
    put_float(rt, dst + 0xE8u, zero);
    float rate = a_scale / square;
    put_float(rt, dst + 0xF0u, zero);
    store16(dst + 0xD6u, 0u);
    put_float(rt, dst + 0xF4u, rate);
    const u32 b119 = load8(src + 0x119u), b11b = load8(src + 0x11Bu), b117 = load8(src + 0x117u);
    const u32 sum = b119 + b11b + b117;
    ctx.gpr[7] = b11b;
    ctx.fpr[1] = std::bit_cast<float>(count1);
    ctx.fpr[2] = rate;
    ctx.fpr[0] = square;
    if (sum == 0u || b119 == 0u || b117 == 0u || b11b == 0u) {
        ctx.gpr[2] = sum;
        ctx.gpr[3] = b117;
        ctx.gpr[4] = b119;
        return;
    }
    // Second channel.
    const u32 kind2 = load8(src + 0x115u);
    store8(dst + 0xD9u, kind2 < 3u ? kind2 : 0u);
    const float b_scale = psp_mul(byte_as_float(b119), tenth);
    put_float(rt, dst + 0xE8u, b_scale);
    put_float(rt, dst + 0xF0u, psp_mul(byte_as_float(b11b), tenth));
    const u32 count2 = scaled(b117);
    store16(dst + 0xD6u, count2);
    square = psp_mul(byte_as_float(count2), byte_as_float(count2));
    rate = b_scale / square;
    put_float(rt, dst + 0xF8u, rate);
    ctx.gpr[2] = count2;
    ctx.gpr[3] = (b117 * 20u) << 2;
    ctx.gpr[4] = b117 << 4;
    ctx.fpr[0] = square;
    ctx.fpr[1] = std::bit_cast<float>(count2);
    ctx.fpr[2] = rate;
}
void func_089CDDF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[23]); G[22] = frame.at(0x4u); G[16] = 0x08CF0000u; G[21] = 0x08CF0000u; G[20] = G[5]; G[5] = 0x08CF6ED8u; G[18] = G[4]; G[4] = G[22]; G[19] = G[6]; G[17] = G[18] + 0x188u; call_here(rt, ctx, 0x08969188u); G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); { const bool tk = G[22] == G[17]; G[5] = 0x08CF6ED8u; if (tk) goto L_089CDE68; } G[5] = word(rt, G[18] + 0x188u); G[4] = G[22]; call_here(rt, ctx, 0x08A5BAD8u); G[5] = 0x08CF6ED8u; L_089CDE68: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08969188u); { const bool tk = G[17] == frame.at(0u); G[5] = word(rt, frame.at(0x0u)); if (tk) goto L_089CDE80; } G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); L_089CDE80: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B3457Cu); G[16] = word(rt, frame.at(0x4u)); if (G[16] == 0u) { G[2] = word(rt, G[19] + 0x0u); goto L_089CDED4; } { const bool tk = G[20] == 0u; G[4] = G[16]; if (tk) goto L_089CDEB0; } G[5] = G[20]; call_here(rt, ctx, 0x089BA4D8u); G[16] = word(rt, frame.at(0x4u)); L_089CDEB0: ; if (G[16] != 0u) { G[4] = G[16]; goto L_089CDEC8; } G[4] = 0x08CA0000u; G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089CDEC8: ; G[5] = 0u; call_here(rt, ctx, 0x089BA6CCu); G[2] = word(rt, G[19] + 0x0u); L_089CDED4: ; { const bool tk = G[2] == 0u; G[23] = frame.at(0x8u); if (tk) goto L_089CDF60; } G[4] = G[23]; G[5] = G[19]; call_here(rt, ctx, 0x08969178u); G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[4] = G[16]; goto L_089CDF04; } G[4] = 0x08CA0000u; G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089CDF04: ; G[5] = G[18] + 0xE8u; call_here(rt, ctx, 0x089BA6CCu); G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[2] = word(rt, G[16] + 0xE0u); goto L_089CDF28; } G[4] = 0x08CA0000u; G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0xE0u); L_089CDF28: ; G[2] = (G[2] & ~0x4000u) | ((0u << 14) & 0x4000u); { const bool tk = G[20] == 0u; put(rt, G[16] + 0xE0u, G[2]); if (tk) goto L_089CDF58; } G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[4] = G[16]; goto L_089CDF50; } G[4] = 0x08CA0000u; G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089CDF50: ; G[5] = G[20]; call_here(rt, ctx, 0x089BAA60u); L_089CDF58: ; G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); L_089CDF60: ; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); if (G[17] == G[19]) { G[5] = word(rt, G[17] + 0x0u); goto L_089CDF80; } G[5] = word(rt, G[19] + 0x0u); G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); G[5] = word(rt, G[17] + 0x0u); L_089CDF80: ; G[4] = G[20]; call_here(rt, ctx, 0x089AB114u); G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B3457Cu); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[22] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[23] = word(rt, frame.at(0x2Cu)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A94DE0 (a1 = the target's source, a2 = the argument count, a3 = the arguments): with one argument that answers its virtual +0xD4, the
// target (0x08A94DA0 into the frame) gets the argument's two values (virtuals +0xDC(0) and +0xD8) through its own virtual +0xE0(target, 0, a,
// b); with two arguments, the target's +0x20 = the first one's virtual +0x3C and 0x08A948A8(target, the second one's +0x3C). Missing values
// report 0x08CB211C (argument) / 0x08CB2180 (target); the frame handle is destroyed (0x08A5B9EC).
void func_08A94DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 source = ctx.gpr[5], count = ctx.gpr[6], arguments = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {16u, 0x10u}});
    ctx.gpr[18] = arguments;
    ctx.gpr[17] = source;
    // virtual `offset` of a non-null object (0x08A5BB7C(missing) first when it is null), with `extra` as a1 when given.
    auto ask = [&](u32 object, u32 offset, u32 missing, bool with_zero) {
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {missing});
        const u32 method = word(rt, word(rt, object) + offset);
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = method;
        return with_zero ? call_set(rt, ctx, method, {object, 0u}) : call_set(rt, ctx, method, {object});
    };
    auto target_of = [&](u32 slot) {
        const u32 target = word(rt, slot);
        ctx.gpr[17] = target;
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2180u});
        return target;
    };
    if (count == 1u) {
        if (ask(word(rt, arguments), 0xD4u, 0x08CB211Cu, false) == 0u) return;
        call_set(rt, ctx, 0x08A94DA0u, {frame.at(0u), source});
        const u32 target = target_of(frame.at(0x0u));
        const u32 apply = word(rt, word(rt, target) + 0xE0u);
        ctx.gpr[20] = apply;
        const u32 a = ask(word(rt, arguments), 0xDCu, 0x08CB211Cu, true);
        ctx.gpr[19] = a;
        const u32 b = ask(word(rt, arguments), 0xD8u, 0x08CB211Cu, false);
        call_set(rt, ctx, apply, {target, 0u, a, b});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        return;
    }
    ctx.gpr[2] = 2u;
    if (count != 2u) return;
    const u32 slot = frame.at(0x4u);
    ctx.gpr[19] = slot;
    call_set(rt, ctx, 0x08A94DA0u, {slot, source});
    u32 target = target_of(slot);
    put(rt, target + 0x20u, ask(word(rt, arguments + 0x0u), 0x3Cu, 0x08CB211Cu, false));
    target = target_of(slot);
    const u32 second = ask(word(rt, arguments + 0x4u), 0x3Cu, 0x08CB211Cu, false);
    call_set(rt, ctx, 0x08A948A8u, {target, second});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
}
void func_08B122C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x1B0u); put(rt, frame.at(0x1A0u), G[30]); put(rt, frame.at(0x194u), G[21]); put(rt, frame.at(0x18Cu), G[19]); put(rt, frame.at(0x188u), G[18]); put(rt, frame.at(0x1A4u), G[31]); put(rt, frame.at(0x19Cu), G[23]); put(rt, frame.at(0x198u), G[22]); put(rt, frame.at(0x190u), G[20]); put(rt, frame.at(0x184u), G[17]); put(rt, frame.at(0x180u), G[16]); G[30] = frame.at(0x10u); G[19] = 0u; G[18] = 0u; put(rt, frame.at(0x170u), G[4]); call_here(rt, ctx, 0x08C7A850u); { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[21] = G[2]; if (tk) goto L_08B12320; } G[18] = G[2]; G[19] = sra(G[2], 31); goto L_08B12458; L_08B12318: ; G[16] = G[22]; goto L_08B12440; L_08B12320: ; G[4] = G[21]; L_08B12324: ; G[5] = G[30]; call_here(rt, ctx, 0x08C7A858u); G[20] = frame.at(0x68u); G[5] = 0x08CB0000u; G[16] = G[2]; G[5] = 0x08CB4B58u; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[20]; if (tk) goto L_08B12440; } G[3] = word(rt, frame.at(0x10u)); G[2] = 0x1000u; G[3] = G[3] & 0xF000u; { const bool tk = G[3] != G[2]; G[2] = 0x2000u; if (tk) goto L_08B12408; } call_here(rt, ctx, 0x08B35FD8u); G[5] = 0x08CB0000u; G[5] = 0x08CB4B5Cu; { const bool tk = G[2] == 0u; G[4] = G[20]; if (tk) goto L_08B12320; } call_here(rt, ctx, 0x08B35FD8u); { const bool tk = G[2] == 0u; G[4] = word(rt, frame.at(0x170u)); if (tk) goto L_08B12320; } call_here(rt, ctx, 0x08B35F60u); G[4] = G[20]; G[16] = G[2]; call_here(rt, ctx, 0x08B35F60u); G[16] = G[16] + G[2]; G[16] = G[16] + 0x4u; G[4] = G[16]; call_here(rt, ctx, 0x08B2B5E0u); G[5] = word(rt, frame.at(0x170u)); put(rt, frame.at(0x4u), G[16]); G[4] = frame.at(0u); G[17] = G[2]; put(rt, frame.at(0x0u), G[2]); put(rt, frame.at(0x8u), 0u); store8(G[2] + 0x0u, 0u); call_here(rt, ctx, 0x08B23928u); G[5] = G[20]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B23A70u); G[4] = word(rt, frame.at(0x0u)); call_here(rt, ctx, 0x08B122C4u); G[22] = G[2]; G[4] = G[17]; G[23] = G[3]; call_here(rt, ctx, 0x08B35254u); G[4] = G[18] + G[22]; G[2] = G[4] < G[18] ? 1u : 0u; G[5] = G[19] + G[23]; { const bool tk = static_cast<std::int32_t>(G[23]) < 0; G[5] = G[2] + G[5]; if (tk) goto L_08B12318; } G[18] = G[4]; G[19] = G[5]; goto L_08B12320; L_08B12408: ; if (G[3] != G[2]) { G[4] = G[21]; goto L_08B12324; } G[2] = word(rt, frame.at(0x18u)); G[3] = 0u; G[9] = G[19] + G[3]; G[6] = G[2] + 0x1FFu; G[2] = G[6]; G[2] = (G[2] & ~0x1FFu) | ((0u << 0) & 0x1FFu); G[8] = G[18] + G[2]; G[4] = G[8] < G[18] ? 1u : 0u; G[9] = G[4] + G[9]; G[18] = G[8]; G[19] = G[9]; goto L_08B12320; L_08B12440: ; G[4] = G[21]; call_here(rt, ctx, 0x08C7A860u); { const bool tk = static_cast<std::int32_t>(G[16]) >= 0; G[2] = G[18]; if (tk) goto L_08B1245C; } G[18] = G[16]; G[19] = sra(G[16], 31); L_08B12458: ; G[2] = G[18]; L_08B1245C: ; G[3] = G[19]; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x1A0u)); G[21] = word(rt, frame.at(0x194u)); G[19] = word(rt, frame.at(0x18Cu)); G[18] = word(rt, frame.at(0x188u)); G[23] = word(rt, frame.at(0x19Cu)); G[22] = word(rt, frame.at(0x198u)); G[20] = word(rt, frame.at(0x190u)); G[17] = word(rt, frame.at(0x184u)); G[16] = word(rt, frame.at(0x180u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B7B0CC (a0 = a FILE): newlib's __srefill. The library is set up first if needed (0x08B796C4 = _REENT, +0x38 = __sdidinit,
// 0x08B78430 = __sinit). At EOF (0x20), or not readable (no __SRD 4 nor __SRW 0x10), it returns -1; a writing stream is flushed (0x08B7807C)
// and switched to reading. A pending ungetc buffer (+0x30, not the inline one at +0x40) is freed (0x08B354A4) and its saved count / pointer
// (+0x3C / +0x38) resumed. Otherwise the buffer is made (0x08B797F0) if missing, line- or un-buffered streams flush the others
// (0x08B794F4(_GLOBAL_REENT, 0x08B7B0A8)), and the stream's read hook (+0x20(cookie +0x1C, base +0x10, size +0x14)) refills it: 0 bytes set
// EOF, an error __SERR (0x40); both return -1. v0 = 0 on success.
void func_08B7B0CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 fp = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    ctx.gpr[16] = fp;
    auto give = [&](u32 v) {
        ctx.gpr[4] = v;
        ctx.gpr[2] = v;
    };
    const u32 reent = call_set(rt, ctx, 0x08B796C4u, {fp});
    ctx.gpr[3] = word(rt, reent + 0x38u);
    if (word(rt, reent + 0x38u) == 0u) {
        const u32 again = call_set(rt, ctx, 0x08B796C4u, {fp});
        call_set(rt, ctx, 0x08B78430u, {again});
    }
    u32 flags = load16(fp + 0xCu);
    ctx.gpr[3] = flags & 0xFFFFu;
    put(rt, fp + 0x4u, 0u);
    if ((flags & 0x20u) != 0u) {
        give(0xFFFFFFFFu);
        return;
    }
    bool resumed_buffer = false;
    if ((flags & 0x4u) == 0u) {
        if ((flags & 0x10u) == 0u) {
            give(0xFFFFFFFFu);
            return;
        }
        if ((flags & 0x8u) != 0u) {
            if (call_set(rt, ctx, 0x08B7807Cu, {fp}) != 0u) {
                give(0xFFFFFFFFu);
                return;
            }
            const u32 now = load16(fp + 0xCu);
            put(rt, fp + 0x8u, 0u);
            const u32 cleared = now & ~0x8u;
            store16(fp + 0xCu, cleared);
            put(rt, fp + 0x18u, 0u);
            store16(fp + 0xCu, cleared | 0x4u);
        } else {
            store16(fp + 0xCu, flags | 0x4u);
        }
    } else {
        const u32 ub = word(rt, fp + 0x30u);
        ctx.gpr[3] = ub;
        if (ub != 0u) {
            if (ub != fp + 0x40u) {
                const u32 r = call_set(rt, ctx, 0x08B796C4u, {fp});
                call_set(rt, ctx, 0x08B354A4u, {r, word(rt, fp + 0x30u)});
            }
            const u32 saved_count = word(rt, fp + 0x3Cu);
            put(rt, fp + 0x30u, 0u);
            put(rt, fp + 0x4u, saved_count);
            if (saved_count != 0u) {
                put(rt, fp + 0x0u, word(rt, fp + 0x38u));
                give(0u);
                return;
            }
            resumed_buffer = true;
        }
    }
    (void)resumed_buffer;
    if (word(rt, fp + 0x10u) == 0u) call_set(rt, ctx, 0x08B797F0u, {fp});
    if ((load16(fp + 0xCu) & 0x3u) != 0u) call_set(rt, ctx, 0x08B794F4u, {word(rt, 0x08CE0E60u), 0x08B7B0A8u});
    const u32 base = word(rt, fp + 0x10u);
    put(rt, fp + 0x0u, base);
    const u32 read = word(rt, fp + 0x20u);
    ctx.gpr[2] = read;
    const u32 got = call_set(rt, ctx, read, {word(rt, fp + 0x1Cu), base, word(rt, fp + 0x14u)});
    ctx.gpr[3] = got;
    put(rt, fp + 0x4u, got);
    u32 bits = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(load16(fp + 0xCu)))) & ~0x2000u;
    store16(fp + 0xCu, bits);
    if (static_cast<s32>(got) > 0) {
        give(0u);
        return;
    }
    if (got != 0u) {
        bits |= 0x40u;
        store16(fp + 0xCu, bits);
        put(rt, fp + 0x4u, 0u);
    } else {
        bits |= 0x20u;
        store16(fp + 0xCu, bits);
    }
    give(0xFFFFFFFFu);
}
// 0x08BFA134 (a0 = first, a1 = middle, a2 = last, a3 = len1, t0 = len2, t1 = buffer, t2 = buffer size; elements are 8 bytes): std::__rotate_adaptive.
// With the shorter second half fitting the buffer (len2 < len1, size >= len2) it is parked in the buffer, the first half moved up to `last`
// and the parked half put at `first`; with the first half fitting (len1 <= size) it is parked, the second half moved down and the parked half
// put at the end; otherwise the elements are rotated in place (0x08BF95F0). v0 = the new position of the old first element.
void func_08BFA134(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], middle = ctx.gpr[5], last = ctx.gpr[6];
    const s32 len1 = static_cast<s32>(ctx.gpr[7]), len2 = static_cast<s32>(ctx.gpr[8]);
    const u32 buffer = ctx.gpr[9];
    const s32 buffer_size = static_cast<s32>(ctx.gpr[10]);
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0xCu}});
    ctx.gpr[11] = last;
    ctx.gpr[18] = first;
    ctx.gpr[17] = last;
    ctx.gpr[16] = middle;
    auto element_copy = [&](u32 from, u32 to) {
        const u32 low = word(rt, from), high = word(rt, from + 4u);
        put(rt, to, low);
        put(rt, to + 4u, high);
    };
    // Moves `count` elements up from `from` to `to`; returns the end of the destination.
    auto copy_up = [&](u32 from, u32 to, s32 count) {
        for (s32 i = 0; i < count; ++i, from += 8u, to += 8u) element_copy(from, to);
        return to;
    };
    // Moves `count` elements ending at `from_end` to end at `to_end`, last one first; returns the start of the destination.
    auto copy_down = [&](u32 from_end, u32 to_end, s32 count) {
        for (s32 i = 0; i < count; ++i) {
            from_end -= 8u;
            to_end -= 8u;
            element_copy(from_end, to_end);
        }
        return to_end;
    };
    const bool second_parks = len2 < len1 && !(buffer_size < len2);
    const bool first_parks = !second_parks && !(buffer_size < len1);
    if (second_parks) {
        const u32 buffer_end = copy_up(middle, buffer, static_cast<s32>(last - middle) >> 3);
        copy_down(middle, last, static_cast<s32>(middle - first) >> 3);
        const u32 end = copy_up(buffer, first, static_cast<s32>(buffer_end - buffer) >> 3);
        ctx.gpr[2] = end;
        return;
    }
    if (first_parks) {
        const u32 buffer_end = copy_up(first, buffer, static_cast<s32>(middle - first) >> 3);
        copy_up(middle, first, static_cast<s32>(last - middle) >> 3);
        const u32 start = copy_down(buffer_end, last, static_cast<s32>(buffer_end - buffer) >> 3);
        ctx.gpr[2] = start;
        return;
    }
    call_set(rt, ctx, 0x08BF95F0u, {first, middle, last});
    ctx.gpr[2] = first + ((last - middle) & ~0x7u);
}
void func_08C501E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); auto &F = ctx.fpr; G[21] = word(rt, G[8] + 0x0u); G[20] = word(rt, G[8] + 0x4u); G[3] = G[5] < G[21] ? 1u : 0u; G[2] = G[5] - G[20]; G[23] = G[5]; if (G[3] == 0u) G[23] = G[2]; G[22] = G[23] + 0xFFFFFFFFu; G[2] = 0u; G[30] = G[7]; G[16] = 0u; put(rt, frame.at(0x10u), G[4]); put(rt, frame.at(0x14u), G[6]); put(rt, frame.at(0x4u), 0u); put(rt, frame.at(0x8u), 0u); { const bool tk = G[22] == 0u; put(rt, frame.at(0xCu), 0u); if (tk) goto L_08C50258; } G[16] = G[22] << 1; G[4] = G[16]; call_here(rt, ctx, 0x08B35DA0u); L_08C50258: ; G[3] = G[2]; G[4] = G[2]; G[5] = G[22]; G[2] = G[2] + G[16]; G[6] = frame.at(0x2u); G[7] = frame.at(0x1u); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x4u), G[3]); put(rt, frame.at(0x8u), G[3]); store16(frame.at(0x2u), 0u); call_here(rt, ctx, 0x08C4FD44u); G[3] = word(rt, frame.at(0x4u)); G[2] = G[16] + G[3]; { const bool tk = G[22] == 0u; put(rt, frame.at(0x8u), G[2]); if (tk) goto L_08C50304; } G[2] = G[22] << 1; G[17] = G[2] + 0xFFFFFFFEu; G[16] = G[23]; G[18] = 0u; goto L_08C502AC; L_08C502A8: ; G[3] = word(rt, frame.at(0x4u)); L_08C502AC: ; G[4] = word(rt, frame.at(0x14u)); G[19] = G[17] + G[3]; G[18] = G[18] + 0x1u; call_here(rt, ctx, 0x08B1EF70u); F[2] = std::bit_cast<float>(G[16]); G[3] = G[16] & 0x1u; G[2] = G[16] >> 1; F[1] = F[0]; G[17] = G[17] + 0xFFFFFFFEu; G[3] = G[3] | G[2]; { const bool tk = static_cast<std::int32_t>(G[16]) >= 0; F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[2]))); if (tk) goto L_08C502E8; } F[2] = std::bit_cast<float>(G[3]); F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[2]))); F[0] = F[0] + F[0]; L_08C502E8: ; F[0] = psp_mul(F[1], F[0]); G[16] = G[16] + 0xFFFFFFFFu; F[0] = std::bit_cast<float>(trunc_to_word(F[0])); G[2] = bits_of(F[0]); { const bool tk = G[22] != G[18]; store16(G[19] + 0x0u, G[2]); if (tk) goto L_08C502A8; } G[3] = word(rt, frame.at(0x4u)); L_08C50304: ; G[2] = G[23] < 0x2u ? 1u : 0u; G[16] = 0x1u; { const bool tk = G[2] == 0u; G[17] = 0u; if (tk) goto L_08C50320; } G[5] = word(rt, frame.at(0x8u)); goto L_08C5035C; L_08C5031C: ; G[3] = word(rt, frame.at(0x4u)); L_08C50320: ; G[2] = G[3] + G[17]; G[6] = load16(G[2] + 0x0u); G[4] = G[16] < G[21] ? 1u : 0u; G[5] = G[16] + G[20]; if (G[4] != 0u) G[5] = G[16]; G[4] = word(rt, frame.at(0x10u)); G[3] = G[6] < G[21] ? 1u : 0u; G[2] = G[6] + G[20]; if (G[3] == 0u) G[6] = G[2]; { const std::uint32_t tg = G[30]; G[16] = G[16] + 0x1u; call_here(rt, ctx, tg); } { const bool tk = G[23] != G[16]; G[17] = G[17] + 0x2u; if (tk) goto L_08C5031C; } G[3] = word(rt, frame.at(0x4u)); G[5] = word(rt, frame.at(0x8u)); L_08C5035C: ; G[4] = G[3]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08C4FC40u); G[4] = word(rt, frame.at(0x4u)); { const bool tk = G[4] == 0u; if (tk) goto L_08C50380; } call_here(rt, ctx, 0x08B35254u); L_08C50380: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[30] = word(rt, frame.at(0x40u)); G[16] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0883E058 (a0 = the output string handle, a1 = a count, a2 = a pointer to an object pointer): with a1 > 0 the object (virtual +0xC) fills
// a string at sp+0x14 from the text at sp+0x10 (0x08B85EB8 builds it, 0x08B8D750 tries to append; when that fails the string is rebuilt through
// 0x08B8D96C, after a length check that raises 0x08B5AEDC); the result goes to the output handle (0x08A6D2D4 copies, 0x08A5BAD8 assigns when
// it is another handle, 0x08A5B9EC destroys the temporary). The three strings are released the library way: the representation sits 0xC
// bytes below the text, its count at -4 (0x08B60804 adds -1; when it drops to 0 or less 0x08B8542C frees it), except the shared empty one
// (0x08D74FD0). The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_0883E058(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4], count = ctx.gpr[5], object_slot = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x28u}, {16u, 0x20u}, {31u, 0x2Cu}, {17u, 0x24u}});
    ctx.gpr[18] = out;
    ctx.gpr[16] = object_slot;
    if (static_cast<s32>(count) <= 0) return;
    const u32 empty = 0x08D74FD0u;
    const u32 pending = frame.at(0x10u), second = frame.at(0x14u);
    // Drops one reference to the string whose text is `text`; `slot` is the scratch address the free routine gets.
    auto release = [&](u32 text, u32 slot) {
        const u32 rep = text - 0xCu;
        ctx.gpr[16] = rep;
        if (rep == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {text - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {rep, slot});
    };
    // Gives the string at `temporary` to the output handle and destroys it.
    auto hand_over = [&](u32 temporary) {
        if (out != temporary) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, temporary)});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    };
    call_set(rt, ctx, 0x08B85EB8u, {pending, 0x08C9C4A8u, frame.at(0x3u)});
    const u32 object = word(rt, ctx.gpr[16]);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    ctx.gpr[17] = second;
    call_set(rt, ctx, word(rt, word(rt, object) + 0xCu), {second, object});
    const u32 text = word(rt, pending);
    ctx.gpr[16] = text;
    const u32 length = call_set(rt, ctx, 0x08B35F60u, {text});
    const u32 appended = call_set(rt, ctx, 0x08B8D750u, {second, text, 0u, length});
    if (appended != 0u) {
        ctx.gpr[16] = frame.at(0x4u);
        call_set(rt, ctx, 0x08A6D2D4u, {frame.at(0x4u), second});
        hand_over(frame.at(0x4u));
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[17] = empty;
    } else {
        const u32 pending_length = word(rt, word(rt, pending) - 0xCu);
        const u32 second_length = word(rt, word(rt, second) - 0xCu);
        ctx.gpr[6] = pending_length;
        ctx.gpr[2] = second_length < pending_length ? 1u : 0u;
        ctx.gpr[16] = frame.at(0x8u);
        if (second_length < pending_length) call_set(rt, ctx, 0x08B5AEDCu, {0x08C9C4B0u});
        ctx.gpr[17] = frame.at(0xCu);
        call_set(rt, ctx, 0x08B8D96Cu, {frame.at(0x8u), second, ctx.gpr[6], 0xFFFFFFFFu});
        call_set(rt, ctx, 0x08A6D2D4u, {frame.at(0xCu), frame.at(0x8u)});
        hand_over(frame.at(0xCu));
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[17] = empty;
        release(word(rt, frame.at(0x8u)), frame.at(0x2u));
    }
    release(word(rt, second), frame.at(0x1u));
    release(word(rt, pending), frame.at(0x0u));
}
void func_088B243C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[20] = G[4] + 0x310u; G[16] = G[4]; G[5] = G[16]; G[4] = G[20]; call_here(rt, ctx, 0x088B3DE8u); G[5] = load16(G[20] + 0x10u); G[4] = word(rt, G[16] + 0x2DCu); G[16] = G[16] + 0x2DCu; { const bool tk = G[5] == 0u; put(rt, frame.at(0x4u), G[16]); if (tk) goto L_088B24D0; } G[17] = G[4]; G[18] = 0u; G[2] = word(rt, G[20] + 0x0u); L_088B249C: ; G[4] = G[17]; G[17] = G[17] + 0x400u; G[2] = G[18] + G[2]; G[3] = sext(load8(G[2] + 0x0u), 8); { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[18] = G[18] + 0x1u; if (tk) goto L_088B24C0; } call_here(rt, ctx, 0x088B4740u); G[5] = load16(G[20] + 0x10u); L_088B24C0: ; G[2] = G[5] & 0xFFFFu; G[2] = G[18] < G[2] ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[20] + 0x0u); goto L_088B249C; } L_088B24D0: ; G[2] = word(rt, frame.at(0x4u)); G[23] = G[5] & 0xFFFFu; { const bool tk = G[23] == 0u; G[30] = word(rt, G[2] + 0x0u); if (tk) goto L_088B2578; } G[3] = G[23] + 0xFFFFFFFFu; goto L_088B25CC; L_088B24E8: ; G[6] = word(rt, G[20] + 0x0u); G[2] = G[6] + G[19]; G[3] = sext(load8(G[2] + 0x0u), 8); { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[18] = G[19] + 0x1u; if (tk) goto L_088B255C; } G[2] = G[18] < G[23] ? 1u : 0u; { const bool tk = G[2] == 0u; G[3] = word(rt, frame.at(0x0u)); if (tk) goto L_088B2560; } G[2] = G[19] << 10; G[3] = G[18] << 10; G[22] = G[30] + G[2]; G[17] = G[30] + G[3]; G[16] = G[18]; G[21] = G[23] - G[18]; goto L_088B2528; L_088B2524: ; G[6] = word(rt, G[20] + 0x0u); L_088B2528: ; G[3] = G[6] + G[16]; G[2] = G[19] + G[6]; G[7] = sext(load8(G[2] + 0x0u), 8); G[6] = sext(load8(G[3] + 0x0u), 8); G[16] = G[16] + 0x1u; G[5] = G[17]; { const bool tk = G[7] != G[6]; G[4] = G[22]; if (tk) goto L_088B2550; } call_here(rt, ctx, 0x088B476Cu); L_088B2550: ; G[2] = G[18] + G[21]; { const bool tk = G[16] != G[2]; G[17] = G[17] + 0x400u; if (tk) goto L_088B2524; } L_088B255C: ; G[3] = word(rt, frame.at(0x0u)); L_088B2560: ; G[2] = G[18] < G[3] ? 1u : 0u; if (G[2] != 0u) { G[19] = G[18]; goto L_088B24E8; } G[2] = word(rt, frame.at(0x4u)); G[5] = load16(G[20] + 0x10u); G[30] = word(rt, G[2] + 0x0u); L_088B2578: ; G[2] = G[5] & 0xFFFFu; L_088B257C: ; { const bool tk = G[2] == 0u; if (tk) goto L_088B25E0; } G[16] = G[30]; G[17] = 0u; G[2] = word(rt, G[20] + 0x0u); L_088B2590: ; G[4] = G[16]; G[16] = G[16] + 0x400u; G[2] = G[17] + G[2]; G[3] = sext(load8(G[2] + 0x0u), 8); { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[17] = G[17] + 0x1u; if (tk) goto L_088B25B4; } call_here(rt, ctx, 0x088B45BCu); G[5] = load16(G[20] + 0x10u); L_088B25B4: ; G[2] = G[5] & 0xFFFFu; G[2] = G[17] < G[2] ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[20] + 0x0u); goto L_088B2590; } goto L_088B25E0; L_088B25CC: ; put(rt, frame.at(0x0u), G[3]); { const bool tk = G[3] != 0u; G[19] = 0u; if (tk) goto L_088B24E8; } G[2] = G[5] & 0xFFFFu; goto L_088B257C; L_088B25E0: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x20u)); G[16] = word(rt, frame.at(0x10u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0894AA7C (a0 = an object, a1 = a flag byte): reads a float (0x0895549C -> f20, the time step) and updates the state record at object+8. With the
// flag set it only clears the record's byte +0xF. Otherwise that byte becomes "active": set when +0xD is nonzero; clear when +0xE is nonzero or
// the signed byte +3 is 2; else only when the float +0x6C4 is strictly inside +-0.27777 (0x3E8E38E4), all four values 0x08955300(object, i) + 0x70
// are zero (a "still" state) and one of +0x538 / +0x53C is positive. A still, pressed state clears nine accumulators (+0x6C8 ... +0x6C4); otherwise two
// are advanced: +0x6CC[i] += dt * (+0x6B4) * (+0x1C[i]) + dt * (+0x6B8) * (+0x28[i]) for i = 0, 1. v0 = what the asm leaves there: the
// call's result with the flag set, the stored byte after a clear, the pointer past the second element after the update.
void func_0894AA7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {16u, 0x0u}, {31u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {52u, 0x20u}, {53u, 0x24u}});
    ctx.gpr[18] = object;
    ctx.gpr[16] = flag;
    const u32 state = word(rt, object + 0x8u);
    ctx.gpr[17] = state;
    call_set(rt, ctx, 0x0895549Cu, {object});
    const float dt = ctx.fpr[0];
    ctx.fpr[20] = dt;
    if (flag != 0u) {
        store8(state + 0xFu, 0u);
        return;
    }
    const float limit = std::bit_cast<float>(0x3E8E38E4u);
    u32 v0;       // the byte stored at +0xF
    bool still;   // the asm's v1: clear the accumulators instead of advancing them
    if (load8(state + 0xDu) != 0u) {
        still = true;
        v0 = 1u;
    } else if (load8(state + 0xEu) != 0u || sext(load8(state + 0x3u), 8) == 2u) {
        still = false;
        v0 = 0u;
    } else {
        const float lean = float_at(rt, state + 0x6C4u);
        still = false;
        v0 = 0u;
        if (-limit < lean && lean < limit) {
            ctx.fpr[21] = 0.0f;
            ctx.gpr[19] = 4u;
            still = true;
            for (u32 i = 0; i < 4u; ++i) {
                ctx.gpr[16] = i;
                const u32 record = call_set(rt, ctx, 0x08955300u, {object, i});
                if (float_at(rt, record + 0x70u) != 0.0f) {
                    still = false;
                    break;
                }
            }
            const bool pressed = 0.0f < float_at(rt, state + 0x538u) || 0.0f < float_at(rt, state + 0x53Cu);
            // The branch delay slot of the asm clears v1 whenever the "still" test falls through, so not pressed means no clear either.
            still = still && pressed;
            v0 = still ? 1u : 0u;
        }
    }
    store8(state + 0xFu, v0);
    if (still) {
        const u32 accumulators[] = {0x6C8u, 0x6F8u, 0x6CCu, 0x6D0u, 0x6A4u, 0x6B4u, 0x6B8u, 0x6C0u, 0x6C4u};
        for (const u32 offset : accumulators) put(rt, state + offset, 0u);
        ctx.gpr[2] = v0;
        return;
    }
    const float side_a = psp_mul(dt, float_at(rt, state + 0x6B4u));
    const float side_b = psp_mul(dt, float_at(rt, state + 0x6B8u));
    u32 element = state;
    for (u32 i = 0; i < 2u; ++i, element += 4u) {
        const float step = psp_mul(side_a, float_at(rt, element + 0x1Cu)) + psp_mul(side_b, float_at(rt, element + 0x28u));
        put_float(rt, element + 0x6CCu, float_at(rt, element + 0x6CCu) + step);
    }
    ctx.gpr[2] = element;
}
void func_08B5C7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[19] = G[5]; G[2] = word(rt, G[4] + 0x0u); G[3] = word(rt, G[5] + 0x0u); { const bool tk = G[2] == G[3]; G[18] = G[4]; if (tk) goto L_08B5C91C; } G[16] = word(rt, G[2] + 0x10u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; if (tk) goto L_08B5C8E8; } G[17] = word(rt, G[3] + 0x10u); G[5] = word(rt, G[17] + 0x0u); if (G[5] == 0u) { goto L_08B5C8EC; } call_here(rt, ctx, 0x08B35FD8u); { const bool tk = G[2] != 0u; if (tk) goto L_08B5C8E8; } G[2] = word(rt, G[16] + 0x4u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x4u); goto L_08B5C914; } G[5] = G[18]; L_08B5C848: ; G[4] = frame.at(0x8u); call_here(rt, ctx, 0x08B5C4B0u); G[5] = G[19]; G[4] = frame.at(0x4u); call_here(rt, ctx, 0x08B5C4B0u); G[18] = word(rt, frame.at(0x4u)); G[19] = word(rt, frame.at(0x8u)); G[2] = frame.at(0xCu); G[17] = word(rt, G[18] + 0xFFFFFFF4u); G[16] = word(rt, G[19] + 0xFFFFFFF4u); G[7] = frame.at(0x10u); put(rt, frame.at(0x10u), G[17]); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[7] = G[2]; put(rt, frame.at(0xCu), G[16]); G[4] = G[19]; G[5] = G[18]; G[6] = word(rt, G[7] + 0x0u); G[22] = 0x08D70000u; G[16] = G[16] - G[17]; call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; G[20] = G[18] + 0xFFFFFFF4u; G[21] = 0x08D77498u; { const bool tk = G[20] != G[21]; G[16] = G[16] < 0x1u ? 1u : 0u; if (tk) goto L_08B5C974; } L_08B5C8B0: ; G[17] = G[19] + 0xFFFFFFF4u; { const bool tk = G[21] != G[17]; G[3] = G[16]; if (tk) goto L_08B5C94C; } L_08B5C8BC: ; G[2] = G[3]; goto exit_; L_08B5C8E8: ; L_08B5C8EC: ; G[3] = 0u; G[2] = G[3]; goto exit_; L_08B5C914: ; { const bool tk = G[2] != 0u; G[5] = G[18]; if (tk) goto L_08B5C848; } L_08B5C91C: ; G[3] = 0x1u; G[2] = G[3]; goto exit_; L_08B5C94C: ; G[4] = G[19] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[3] = G[16]; if (tk) goto L_08B5C8BC; } G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C74884u); G[3] = G[16]; goto L_08B5C8BC; L_08B5C974: ; G[4] = G[18] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[20]; if (tk) goto L_08B5C990; } G[19] = word(rt, frame.at(0x8u)); goto L_08B5C8B0; L_08B5C990: ; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08C74884u); G[19] = word(rt, frame.at(0x8u)); goto L_08B5C8B0;  exit_: ; G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0883FD7C (a0 = the output handle, a1 = a count, a2 = a pointer to an object pointer): with a1 > 0 a list builder (0x08A98A70 into sp+0xC,
// 0x0893600C into sp+0x90) is prepared and the object (virtual +0x3C) gives a type; 0x08931090 gives n entries and for each index i
// (0x0893119C, 0x08935B88, 0x08935C2C) whose type equals the object's one, a text is built (0x08B85EB8 into sp+4) and appended to the list
// handle's vector (+0x14: end +4, capacity +8; appended in place through 0x08A76ABC, or by 0x08B8A03C when full), the temporary string is released
// the library way (representation 0xC below the text, count at -4 changed with 0x08B60804, freed with 0x08B8542C unless it is the shared empty
// string 0x08D74FD0). Finally the list handle goes to the output handle (0x08A5BAD8) and the builder is torn down (0x08932448, 0x08A5B9EC).
// The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_0883FD7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4], count = ctx.gpr[5], object_slot = ctx.gpr[6];
    GuestFrame frame(ctx, 0x250u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x220u}, {31u, 0x244u}, {30u, 0x240u}, {23u, 0x23Cu}, {22u, 0x238u}, {21u, 0x234u}, {20u, 0x230u}, {19u, 0x22Cu}, {18u, 0x228u}, {17u, 0x224u}});
    ctx.gpr[16] = object_slot;
    put(rt, frame.at(0x210u), out);
    if (static_cast<s32>(count) <= 0) return;
    const u32 empty = 0x08D74FD0u;
    const u32 list = frame.at(0xCu), builder = frame.at(0x90u), text = frame.at(0x10u), name = frame.at(0x4u), temp = frame.at(0x8u);
    ctx.gpr[23] = list;
    ctx.gpr[20] = builder;
    call_set(rt, ctx, 0x08A98A70u, {list});
    call_set(rt, ctx, 0x0893600Cu, {builder});
    u32 object = word(rt, ctx.gpr[16]);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    const u32 kind = call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    ctx.gpr[22] = kind;
    const u32 entries = call_set(rt, ctx, 0x08931090u, {});
    ctx.gpr[21] = entries;
    if (static_cast<s32>(entries) > 0) {
        ctx.gpr[19] = text;
        ctx.gpr[30] = temp;
        u32 index = 0u;
        ctx.gpr[18] = 0u;
        do {
            ctx.gpr[6] = 0x80u;
            call_set(rt, ctx, 0x0893119Cu, {index, text});
            call_set(rt, ctx, 0x08935B88u, {builder, text});
            if (call_set(rt, ctx, 0x08935C2Cu, {builder}) == kind) {
                const u32 owner = word(rt, list);
                ctx.gpr[17] = owner;
                ctx.gpr[6] = frame.at(0x1u);
                if (owner == 0u) {
                    call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C42Cu});
                    ctx.gpr[6] = frame.at(0x1u);
                }
                call_set(rt, ctx, 0x08B85EB8u, {name, text, ctx.gpr[6]});
                const u32 vector = owner + 0x14u;
                ctx.gpr[16] = vector;
                call_set(rt, ctx, 0x08A6D2D4u, {temp, name});
                u32 end = word(rt, vector + 0x4u);
                if (end == word(rt, vector + 0x8u)) {
                    call_set(rt, ctx, 0x08B8A03Cu, {vector, word(rt, owner + 0x18u), temp});
                } else {
                    if (end != 0u) {
                        call_set(rt, ctx, 0x08A76ABCu, {end, temp});
                        end = word(rt, vector + 0x4u);
                    }
                    put(rt, vector + 0x4u, end + 0x4u);
                }
                call_set(rt, ctx, 0x08A5B9ECu, {temp});
                // Releases the string `name` the library way.
                const u32 rep = word(rt, name) - 0xCu;
                ctx.gpr[16] = rep;
                ctx.gpr[2] = empty;
                if (rep != empty) {
                    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {word(rt, name) - 0x4u, 0xFFFFFFFFu})) <= 0) {
                        call_set(rt, ctx, 0x08B8542Cu, {rep, frame.at(0x0u)});
                    }
                }
            }
            ++index;
            ctx.gpr[18] = index;
        } while (entries != index);
        ctx.gpr[6] = 0x80u;
    }
    const u32 target = word(rt, frame.at(0x210u));
    if (target != list) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, list)});
    call_set(rt, ctx, 0x08932448u, {builder});
    call_set(rt, ctx, 0x08A5B9ECu, {list});
}
// 0x08844D1C (a0 = the output handle, a2 = a count, a3 = a pointer to an object pointer): with a2 > 0 a lookup object is built (0x088447E8 into a
// temporary, converted by 0x08844634), the object behind a3 (virtual +0x3C) selects a record (0x08893D38) of three floats and a byte (+0xC);
// the floats go to a vector handle (0x08A98C1C(handle, 3); each element is set through 0x08A87EEC with the float in f12 and copied into
// the handle's elements +0x14[0..2] with 0x08A5BAD8 when it is another handle), then the byte goes into the second handle's +0x14 + 4
// through 0x08A85B1C, and the output handle takes the second handle (0x08A5BAD8). The temporaries are destroyed with 0x08A5B9EC.
// The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_08844D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4], count = ctx.gpr[6], holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x30u}, {17u, 0x24u}, {31u, 0x34u}, {19u, 0x2Cu}, {18u, 0x28u}, {16u, 0x20u}});
    ctx.gpr[20] = out;
    ctx.gpr[17] = holder;
    if (static_cast<s32>(count) <= 0) return;
    const u32 lookup = frame.at(0x10u), vector = frame.at(0x14u), result = frame.at(0x18u);
    ctx.gpr[19] = lookup;
    call_set(rt, ctx, 0x088447E8u, {lookup});
    u32 object = word(rt, lookup);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C948u});
    ctx.gpr[18] = call_set(rt, ctx, 0x08844634u, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {lookup});
    object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9C908u});
    ctx.gpr[19] = vector;
    ctx.gpr[17] = frame.at(0xCu);
    const u32 key = call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    const u32 record = call_set(rt, ctx, 0x08893D38u, {ctx.gpr[18], key});
    ctx.gpr[18] = record;
    call_set(rt, ctx, 0x08A98C1Cu, {vector, 3u});
    // Sets element `index` (a float of the record) into the vector handle through the temporary at `temp`.
    auto set_element = [&](u32 temp, u32 index) {
        ctx.fpr[12] = float_at(rt, record + index * 4u);
        const u32 element = word(rt, word(rt, vector) + 0x14u) + index * 4u;
        ctx.gpr[17] = temp;
        ctx.gpr[16] = element;
        call_set(rt, ctx, 0x08A87EECu, {temp});
        if (element != temp) call_set(rt, ctx, 0x08A5BAD8u, {element, word(rt, temp)});
    };
    set_element(frame.at(0xCu), 0u);
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    set_element(frame.at(0x8u), 1u);
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
    set_element(frame.at(0x4u), 2u);
    ctx.gpr[17] = result;
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
    call_set(rt, ctx, 0x08A98C1Cu, {result, 2u});
    u32 inner = word(rt, result);
    if (word(rt, inner + 0x14u) != vector) {
        call_set(rt, ctx, 0x08A5BAD8u, {word(rt, inner + 0x14u), word(rt, vector)});
        inner = word(rt, result);
    }
    const u32 slot = word(rt, inner + 0x14u) + 0x4u;
    ctx.gpr[16] = slot;
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0x0u), sext(load8(record + 0xCu), 8)});
    if (slot != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
    if (out != result) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
    call_set(rt, ctx, 0x08A5B9ECu, {result});
    call_set(rt, ctx, 0x08A5B9ECu, {vector});
}
// 0x08890788 (a0 = an object holding a pointer to a big-endian table header): two tables of records, at header +0xE8 (count, pointer; 0x20-byte
// records, handled by 0x0888FE08) and +0xF0 (0x10-byte records, 0x0888FE40). Each record is handed to its routine, which returns the next
// record's address and writes the bytes it consumed to the frame; from the second record on, the previous record's +8 first gets the result
// of 0x08B79BAC(previous, +8, +0xC) (stored back big-endian), and the second table does so for the first table's last record too. The
// header's +0xC (a big-endian size) is reduced by the consumed total; a header whose +4 is zero is initialised first (0x08890068) and
// finished afterwards (0x08890554). v0 = the new +0xC (big-endian) unless the finishing call ran.
void func_08890788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{23u, 0x2Cu}, {22u, 0x28u}, {31u, 0x30u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    auto swap = [](u32 value) { return __builtin_bswap32(value); };
    const u32 header = word(rt, object);
    ctx.gpr[23] = 0u;
    ctx.gpr[20] = header;
    ctx.gpr[22] = object;
    if (swap(word(rt, header + 0x4u)) == 0u) {
        ctx.gpr[23] = 1u;
        call_set(rt, ctx, 0x08890068u, {object});
    }
    u32 total = 0u;
    u32 current = 0u;
    // One table: `count_at` is the offset of its count, `stride` and `routine` those of its records.
    auto walk = [&](u32 count_at, u32 stride, u32 routine) {
        const u32 table = header + count_at;
        const s32 count = static_cast<s32>(swap(word(rt, table)));
        u32 cursor = swap(word(rt, table + 0x4u));
        ctx.gpr[21] = table;
        ctx.gpr[16] = cursor;
        if (count <= 0) return;
        for (s32 index = 0; index < count; ++index) {
            if (index > 0) ctx.gpr[18] = static_cast<u32>(index) + 1u;
            if (index > 0 || current != 0u) {
                const u32 hi = swap(word(rt, cursor + 0x8u)), lo = swap(word(rt, cursor + 0xCu));
                call_set(rt, ctx, 0x08B79BACu, {current, hi, lo});
                put(rt, cursor + 0x8u, swap(current));
            }
            current = call_set(rt, ctx, routine, {cursor, frame.at(0x0u)});
            ctx.gpr[17] = current;
            total += word(rt, frame.at(0x0u));
            ctx.gpr[19] = total;
            cursor += stride;
            ctx.gpr[16] = cursor;
        }
        ctx.gpr[18] = static_cast<u32>(count) + 1u;
    };
    ctx.gpr[19] = 0u;
    ctx.gpr[17] = 0u;
    walk(0xE8u, 0x20u, 0x0888FE08u);
    walk(0xF0u, 0x10u, 0x0888FE40u);
    const u32 size = swap(swap(word(rt, header + 0xCu)) - total);
    put(rt, header + 0xCu, size);
    ctx.gpr[2] = size;
    if (ctx.gpr[23] != 0u) call_set(rt, ctx, 0x08890554u, {object});
}
void func_08AEAB24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[23] = G[7]; G[22] = G[9]; G[21] = G[6]; G[20] = G[5]; G[19] = G[8]; G[16] = G[4]; L_08AEAB64: ; G[3] = load8(G[16] + 0x0u); G[4] = G[19]; G[2] = sext(G[3] & 0xFFu, 8); { const bool tk = G[2] == 0u; G[5] = G[2]; if (tk) goto L_08AEAB94; } call_here(rt, ctx, 0x08B7BA50u); if (G[2] == 0u) { G[3] = load8(G[16] + 0x0u); goto L_08AEAB90; } G[16] = G[16] + 0x1u; goto L_08AEAB64; L_08AEAB90: ; G[2] = sext(G[3] & 0xFFu, 8); L_08AEAB94: ; G[17] = G[16]; G[18] = 0u; { const bool tk = G[2] == 0u; G[16] = G[20]; if (tk) goto L_08AEAC14; } G[5] = G[2]; G[21] = G[21] + 0xFFFFFFFFu; L_08AEABAC: ; G[4] = G[22]; call_here(rt, ctx, 0x08B7BA50u); { const bool tk = G[2] != 0u; G[4] = G[23]; if (tk) goto L_08AEACEC; } G[5] = sext(load8(G[17] + 0x0u), 8); call_here(rt, ctx, 0x08B7BA50u); G[6] = G[17] + 0x1u; G[3] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[21]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[5] = G[17] + 0x2u; if (tk) goto L_08AEACE4; } if (G[16] == 0u) { G[2] = sext(load8(G[17] + 0x0u), 8); goto L_08AEACD4; } G[18] = G[18] + 0x1u; { const bool tk = G[3] == 0u; G[4] = G[17]; if (tk) goto L_08AEACD0; } G[3] = load8(G[17] + 0x0u); G[2] = sext(G[3] & 0xFFu, 8); if (static_cast<std::int32_t>(G[2]) < 0) { store8(G[16] + 0x0u, G[3]); goto L_08AEACC4; } L_08AEABF8: ; G[2] = load8(G[4] + 0x0u); G[17] = G[4] + 0x1u; store8(G[16] + 0x0u, G[2]); G[16] = G[16] + 0x1u; L_08AEAC08: ; G[2] = sext(load8(G[17] + 0x0u), 8); { const bool tk = G[2] != 0u; G[5] = G[2]; if (tk) goto L_08AEABAC; } L_08AEAC14: ; { const bool tk = G[20] == 0u; G[18] = G[20]; if (tk) goto L_08AEAC30; } store8(G[16] + 0x0u, 0u); G[2] = sext(load8(G[20] + 0x0u), 8); { const bool tk = G[2] != 0u; G[16] = G[20]; if (tk) goto L_08AEAC8C; } store8(G[18] + 0x1u, 0u); L_08AEAC30: ; G[16] = G[17]; L_08AEAC34: ; G[2] = sext(load8(G[16] + 0x0u), 8); G[4] = G[19]; { const bool tk = G[2] == 0u; G[5] = G[2]; if (tk) goto L_08AEAC5C; } call_here(rt, ctx, 0x08B7BA50u); { const bool tk = G[2] == 0u; G[2] = G[16]; if (tk) goto L_08AEAC60; } G[16] = G[16] + 0x1u; goto L_08AEAC34; L_08AEAC5C: ; G[2] = G[16]; L_08AEAC60: ; goto exit_; L_08AEAC8C: ; G[5] = sext(load8(G[16] + 0x0u), 8); G[4] = G[19]; call_here(rt, ctx, 0x08B7BA50u); G[3] = sext(load8(G[16] + 0x0u), 8); G[5] = G[16] + 0x1u; G[4] = G[16] + 0x2u; G[3] = static_cast<std::int32_t>(G[3]) < 0 ? 1u : 0u; if (G[3] == 0u) G[4] = G[5]; if (G[2] == 0u) G[18] = G[16]; G[2] = sext(load8(G[4] + 0x0u), 8); { const bool tk = G[2] != 0u; G[16] = G[4]; if (tk) goto L_08AEAC8C; } store8(G[18] + 0x1u, 0u); goto L_08AEAC30; L_08AEACC4: ; G[4] = G[17] + 0x1u; G[16] = G[16] + 0x1u; goto L_08AEABF8; L_08AEACD0: ; G[2] = sext(load8(G[17] + 0x0u), 8); L_08AEACD4: ; G[17] = G[5]; G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[17] = G[6]; goto L_08AEAC08; L_08AEACE4: ; G[17] = G[6]; goto L_08AEAC14; L_08AEACEC: ; G[2] = 0x08C90000u; G[17] = 0x08C91D1Cu; goto L_08AEAC14;  exit_: ; G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08AEE934 (a0 = an object, a1 = the address of a descriptor word, a2 = an offset, a3 = a buffer, t0 = a length, t1 = a pointer to a word that
// stops the loop once nonzero): until that word is set, and while the object answers 0x08AEE5A0 (and 0x08AEE608), it seeks the descriptor
// to the offset (0x08C7A880, 64-bit position, whence 0) and reads `length` bytes into the buffer (0x08C7A830, repeated until all arrived or an
// error). A short or failed read, with the debug flag (bit 0 of 0x08CDFB3C) set, is reported (0x08AD83B8); a negative result lets the object
// try to recover (0x08AEE5C8 / 0x08AEE8C0, wait 10000 us with 0x08C7A988, descriptor set to -1 and reopened by 0x08AEE744) before 0x08AEE6E0
// finishes the round. v0 = what the asm leaves: the stop word, or the last call's / length leftovers on the early exits.
void func_08AEE934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], slot = ctx.gpr[5], offset = ctx.gpr[6], buffer = ctx.gpr[7], length = ctx.gpr[8], stop_word = ctx.gpr[9];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {31u, 0x34u}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[30] = offset;
    ctx.gpr[23] = buffer;
    ctx.gpr[22] = slot;
    ctx.gpr[21] = object;
    ctx.gpr[20] = stop_word;
    ctx.gpr[19] = length;
    const bool reads = static_cast<s32>(0u) < static_cast<s32>(length);
    ctx.gpr[2] = reads ? 1u : 0u;
    if (word(rt, stop_word) != 0u) return;
    put(rt, frame.at(0x4u), ctx.gpr[2]);
    for (;;) {
        ctx.gpr[4] = object;
        if (call_set(rt, ctx, 0x08AEE5A0u, {object}) != 0u) {
            if (call_set(rt, ctx, 0x08AEE608u, {ctx.gpr[4]}) == 0u) return;
        }
        const u32 descriptor = word(rt, slot);
        ctx.gpr[18] = descriptor;
        call_set(rt, ctx, 0x08C7A880u, {descriptor, ctx.gpr[5], offset, static_cast<u32>(static_cast<s32>(offset) >> 31), 0u});
        u32 moved = ctx.gpr[2];
        ctx.gpr[17] = moved;
        if (static_cast<s32>(ctx.gpr[3]) >= 0) {
            ctx.gpr[2] = word(rt, frame.at(0x4u));
            ctx.gpr[16] = 0u;
            moved = 0u;
            ctx.gpr[17] = 0u;
            if (ctx.gpr[2] != 0u) {
                u32 done = 0u;
                u32 remaining = length;
                for (;;) {
                    const u32 got = call_set(rt, ctx, 0x08C7A830u, {descriptor, buffer + done, remaining});
                    done += got;
                    ctx.gpr[16] = done;
                    moved = got;
                    ctx.gpr[17] = got;
                    if (static_cast<s32>(got) < 0) break;
                    remaining = length - done;
                    ctx.gpr[2] = remaining;
                    if (!(static_cast<s32>(done) < static_cast<s32>(length))) {
                        moved = done;
                        ctx.gpr[17] = done;
                        break;
                    }
                }
            }
        }
        ctx.gpr[16] = 0x08CE0000u;
        if (moved == length) return;
        ctx.gpr[2] = 0x08C90000u;
        if ((word(rt, 0x08CDFB3Cu) & 0x1u) != 0u) {
            call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08C97640u), 0x08C91F50u, 0xC28u, 0x08C92524u, object, moved});
        }
        if (static_cast<s32>(moved) < 0) {
            ctx.gpr[6] = stop_word;
            if (call_set(rt, ctx, 0x08AEE5C8u, {object}) != 0u) {
                call_set(rt, ctx, 0x08AEE8C0u, {object, word(rt, slot)});
                call_set(rt, ctx, 0x08C7A988u, {0x2710u});
                const u32 old = word(rt, slot);
                ctx.gpr[17] = old;
                put(rt, slot, 0xFFFFFFFFu);
                call_set(rt, ctx, 0x08AEE744u, {object, slot, frame.at(0x0u), stop_word});
                if ((word(rt, 0x08CDFB3Cu) & 0x4u) != 0u) call_set(rt, ctx, 0x08AD8464u, {0x08C92510u, old, word(rt, slot)});
            }
        }
        call_set(rt, ctx, 0x08AEE6E0u, {stop_word});
        const u32 stop = word(rt, stop_word);
        ctx.gpr[2] = stop;
        if (stop != 0u) return;
    }
}
// 0x08B6E74C (a0:a1 = a double x, a2 = n): ldexp / scalbn on the soft-float library (0x08B71394 multiplies, 0x08B71328 adds, 0x08B6EE10 is
// copysign). Subnormals are first scaled by 2^54 (0x08CB68E0); inf / nan return x + x; an exponent that overflows (or n > 50000) gives
// copysign(huge) x huge (0x08CB68F0), one below -53 (or n < -50000) copysign(tiny) x tiny (0x08CB68E8); a result that turns subnormal is
// built 54 higher and scaled by 2^-54 (0x08CB68F8). v0:v1 = the result.
void func_08B6E74C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 lo = ctx.gpr[4], hi = ctx.gpr[5];
    const u32 n = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x8u}, {17u, 0x4u}});
    ctx.gpr[16] = n;
    auto mul = [&](u32 a_lo, u32 a_hi, u32 b_lo, u32 b_hi) {
        lo = call_set(rt, ctx, 0x08B71394u, {a_lo, a_hi, b_lo, b_hi});
        hi = ctx.gpr[3];
    };
    auto done = [&] {
        ctx.gpr[2] = lo;
        ctx.gpr[3] = hi;
    };
    auto signed_scale = [&](u32 c_lo, u32 c_hi) {  // copysign(c, x) x c
        ctx.gpr[16] = c_lo;
        ctx.gpr[17] = c_hi;
        const u32 t_lo = call_set(rt, ctx, 0x08B6EE10u, {c_lo, c_hi, lo, hi});
        mul(t_lo, ctx.gpr[3], c_lo, c_hi);
        done();
    };
    const u32 huge_lo = word(rt, 0x08CB68F0u), huge_hi = word(rt, 0x08CB68F4u);
    s32 k = static_cast<s32>((hi >> 20) & 0x7FFu);
    ctx.gpr[7] = static_cast<u32>(k);
    if (k == 0) {
        if (((hi & 0x7FFFFFFFu) | lo) == 0u) {
            ctx.gpr[2] = lo;
            done();
            return;
        }
        mul(lo, hi, word(rt, 0x08CB68E0u), word(rt, 0x08CB68E4u));
        k = static_cast<s32>((hi >> 20) & 0x7FFu) - 54;
        if (static_cast<s32>(n) < static_cast<s32>(0xFFFF3CB0u)) {
            mul(lo, hi, word(rt, 0x08CB68E8u), word(rt, 0x08CB68ECu));
            done();
            return;
        }
    }
    if (k == 0x7FF) {
        lo = call_set(rt, ctx, 0x08B71328u, {lo, hi, lo, hi});
        hi = ctx.gpr[3];
        done();
        return;
    }
    k += static_cast<s32>(n);
    if (!(k < 2047)) {
        signed_scale(huge_lo, huge_hi);
        return;
    }
    if (k > 0) {
        hi = (hi & ~0x7FF00000u) | (static_cast<u32>(k) << 20);
        done();
        return;
    }
    if (k < -53) {
        if (static_cast<s32>(0xC350u) < static_cast<s32>(n)) signed_scale(huge_lo, huge_hi);
        else signed_scale(word(rt, 0x08CB68E8u), word(rt, 0x08CB68ECu));
        return;
    }
    hi = (hi & ~0x7FF00000u) | (static_cast<u32>(k + 54) << 20);
    mul(lo, hi, word(rt, 0x08CB68F8u), word(rt, 0x08CB68FCu));
    done();
}
// 0x08BFA724 (a0 = first, a1 = middle, a2 = last, a3 = len1, t0 = len2): std::__merge_without_buffer over 8-byte elements (an object
// pointer and a tie-break word), ordered by the object's +0x134 then the second word. Two elements are swapped in place; otherwise the longer
// half is split, the cut found in the other with lower_bound (0x08BFA370) or upper_bound (0x08BFA300), the middle rotated (0x08BF95F0), the
// left part merged by recursion and the right part by looping.
void func_08BFA724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}});
    for (;;) {
        const u32 first = ctx.gpr[4], middle = ctx.gpr[5], last = ctx.gpr[6], len1 = ctx.gpr[7], len2 = ctx.gpr[8];
        ctx.gpr[19] = first;
        ctx.gpr[18] = len2;
        ctx.gpr[17] = len1;
        ctx.gpr[16] = middle;
        put(rt, frame.at(0x0u), last);
        if (len1 == 0u || len2 == 0u) return;
        if (len1 + len2 == 2u) {
            ctx.gpr[2] = static_cast<s32>(len2) < static_cast<s32>(len1) ? 1u : 0u;
            const u32 pm = word(rt, middle), pf = word(rt, first);
            const s32 km = static_cast<s32>(word(rt, pm + 0x134u)), kf = static_cast<s32>(word(rt, pf + 0x134u));
            ctx.gpr[6] = pm;
            ctx.gpr[5] = pf;
            ctx.gpr[4] = static_cast<u32>(km);
            ctx.gpr[3] = static_cast<u32>(kf);
            bool swap = km < kf;
            ctx.gpr[2] = swap ? 1u : 0u;
            if (!swap) {
                ctx.gpr[2] = kf < km ? 1u : 0u;
                if (kf < km) return;
                const u32 m2 = word(rt, middle + 0x4u), f2 = word(rt, first + 0x4u);
                ctx.gpr[3] = m2;
                ctx.gpr[4] = f2;
                swap = static_cast<s32>(m2) < static_cast<s32>(f2);
                ctx.gpr[2] = swap ? 1u : 0u;
                if (!swap) return;
            }
            const u32 m2 = word(rt, middle + 0x4u), f2 = word(rt, first + 0x4u);
            ctx.gpr[3] = m2;
            ctx.gpr[4] = f2;
            put(rt, first + 0x4u, m2);
            put(rt, first + 0x0u, pm);
            put(rt, middle + 0x0u, pf);
            put(rt, middle + 0x4u, f2);
            return;
        }
        u32 cut1, cut2, len11, len22, moved;
        if (static_cast<s32>(len2) < static_cast<s32>(len1)) {
            len11 = sra((len1 >> 31) + len1, 1);
            cut1 = first + (len11 << 3);
            cut2 = call_set(rt, ctx, 0x08BFA370u, {middle, word(rt, frame.at(0x0u)), cut1});
            len22 = sra(cut2 - middle, 3);
            moved = len22;
        } else {
            len22 = sra((len2 >> 31) + len2, 1);
            cut2 = middle + (len22 << 3);
            cut1 = call_set(rt, ctx, 0x08BFA300u, {first, middle, cut2});
            moved = sra(cut2 - middle, 3);
            len11 = sra(cut1 - first, 3);
        }
        ctx.gpr[30] = len11;
        ctx.gpr[22] = len22;
        ctx.gpr[23] = moved;
        ctx.gpr[20] = cut1;
        ctx.gpr[21] = cut2;
        const u32 new_middle = cut1 + (moved << 3);
        ctx.gpr[16] = new_middle;
        call_set(rt, ctx, 0x08BF95F0u, {cut1, middle, cut2, middle});
        ctx.gpr[9] = new_middle;
        call_set(rt, ctx, 0x08BFA724u, {first, cut1, new_middle, len11, len22, new_middle});
        ctx.gpr[4] = new_middle;
        ctx.gpr[5] = cut2;
        ctx.gpr[6] = word(rt, frame.at(0x0u));
        ctx.gpr[7] = len1 - len11;
        ctx.gpr[8] = len2 - len22;
        ctx.gpr[9] = new_middle;
    }
}
// 0x088205BC (a0 = the output handle, a1 = a mode, a2 = a pointer to three object pointers): with a mode of at least 3, objects 1..3 give a
// float (virtual +0x54 of the first, through f12 to 0x0881FC44 which returns a record, 0 stops here), a base (virtual +0x3C of the second) and
// a count (virtual +0x3C of the third, handed to the vector handle at sp+4 with 0x08A98C1C). The base plus the record's +0xC is clamped into
// [0, N - 1] (N = 0x08CE2AD0, the table at 0x08CE2ACC of 16-byte entries) and, for each of `count` entries from there (index clamped to the table),
// 0x08930290(entry +0 / +4) is converted to a float, passed through 0x08B1ECFC and truncated to a word; 0x0884CC18 builds an element from the
// entry and that word and it goes into the vector handle's +0x14[i] (0x08A5BAD8 when it is another handle). Finally the handle goes to the
// output handle. The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_088205BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4], mode = ctx.gpr[5], objects = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {17u, 0x14u}, {31u, 0x34u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}});
    ctx.gpr[30] = out;
    ctx.gpr[17] = objects;
    if (static_cast<s32>(mode) < 3) return;
    // The object behind pointer slot `slot_at`; a null one is reported first (0x08A5BB7C).
    auto fetch = [&](u32 slot_at) {
        const u32 object = word(rt, slot_at);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99294u});
        return object;
    };
    u32 object = fetch(objects);
    call_set(rt, ctx, word(rt, word(rt, object) + 0x54u), {object});
    ctx.fpr[12] = ctx.fpr[0];
    const u32 record = call_set(rt, ctx, 0x0881FC44u, {});
    ctx.gpr[18] = record;
    if (record == 0u) return;
    object = fetch(objects + 0x4u);
    const u32 base = call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    ctx.gpr[20] = base;
    object = fetch(objects + 0x8u);
    const u32 handle = frame.at(0x4u);
    ctx.gpr[22] = handle;
    ctx.gpr[23] = 0x08CE0000u;
    const u32 count = call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    ctx.gpr[19] = count;
    call_set(rt, ctx, 0x08A98C1Cu, {handle, count});
    const u32 table_size = word(rt, 0x08CE2AD0u);
    s32 first = static_cast<s32>(base + word(rt, record + 0xCu));
    if (first < 0) first = 0;
    if (!(first < static_cast<s32>(table_size))) first = static_cast<s32>(table_size - 1u);
    ctx.gpr[18] = static_cast<u32>(first);
    if (static_cast<s32>(count) > 0) {
        ctx.gpr[21] = frame.at(0x0u);
        ctx.gpr[20] = 0x08CE0000u;
        for (u32 i = 0; i < count; ++i) {
            ctx.gpr[17] = i;
            s32 index = static_cast<s32>(i) + first;
            if (!(index < static_cast<s32>(table_size))) index = static_cast<s32>(table_size - 1u);
            const u32 entry = (static_cast<u32>(index) << 4) + word(rt, 0x08CE2ACCu);
            ctx.gpr[16] = entry;
            const u32 raw = call_set(rt, ctx, 0x08930290u, {word(rt, entry + 0x0u), word(rt, entry + 0x4u)});
            ctx.fpr[0] = std::bit_cast<float>(raw);
            ctx.fpr[12] = static_cast<float>(static_cast<s32>(raw));
            call_set(rt, ctx, 0x08B1ECFCu, {});
            const u32 truncated = trunc_to_word(ctx.fpr[0]);
            ctx.fpr[0] = std::bit_cast<float>(truncated);
            const u32 slot = word(rt, word(rt, handle) + 0x14u) + (i << 2);
            ctx.gpr[16] = slot;
            call_set(rt, ctx, 0x0884CC18u, {frame.at(0x0u), ctx.gpr[5], word(rt, entry + 0x0u), word(rt, entry + 0x4u), truncated});
            ctx.gpr[17] = i + 1u;
            if (slot != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x0u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
        }
    }
    if (out != handle) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, handle)});
    call_set(rt, ctx, 0x08A5B9ECu, {handle});
}
void func_088567F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xD0u); put(rt, frame.at(0xACu), G[19]); put(rt, frame.at(0xB0u), G[20]); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); put(rt, frame.at(0xA0u), G[16]); put(rt, frame.at(0xB4u), G[31]); auto &F = ctx.fpr; G[19] = frame.at(0xCu); G[20] = G[4]; G[4] = G[19]; G[18] = G[7]; G[17] = G[6]; put_float(rt, frame.at(0xC0u), F[20]); call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(0xCu)); { const bool tk = G[16] != 0u; if (tk) goto L_08856844; } G[4] = 0x08CA0000u; G[4] = 0x08C9E188u; call_here(rt, ctx, 0x08A5BB7Cu); L_08856844: ; G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[2]; G[5] = frame.at(0x10u); G[6] = frame.at(0x14u); call_here(rt, ctx, 0x088A5A48u); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = static_cast<std::int32_t>(G[17]) <= 0; F[12] = float_at(rt, frame.at(0x10u)); if (tk) goto L_0885689C; } G[16] = word(rt, G[18] + 0x0u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08856888; } G[4] = 0x08CA0000u; G[4] = 0x08C9E14Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08856888: ; G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } { const bool tk = G[2] != 0u; F[12] = float_at(rt, frame.at(0x10u)); if (tk) goto L_088568E0; } L_0885689C: ; G[16] = 0x08D70000u; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48F04u); F[20] = F[0]; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48C30u); G[19] = frame.at(0x18u); F[20] = std::bit_cast<float>(trunc_to_word(F[20])); G[6] = 0x08CA0000u; G[6] = 0x08C9E518u; G[8] = G[2]; G[7] = bits_of(F[20]); G[4] = G[19]; G[5] = 0x80u; call_here(rt, ctx, 0x08B3767Cu); G[16] = frame.at(0x4u); goto L_08856938; L_088568E0: ; G[16] = 0x08D70000u; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48F04u); F[20] = F[0]; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48C30u); G[4] = 0x08D6950Cu; F[20] = std::bit_cast<float>(trunc_to_word(F[20])); G[17] = word(rt, frame.at(0x14u)); G[18] = G[2]; call_here(rt, ctx, 0x08B48CFCu); G[7] = bits_of(F[20]); G[19] = frame.at(0x18u); G[6] = 0x08CA0000u; G[6] = 0x08C9E520u; G[8] = G[18]; G[9] = G[17]; G[10] = G[2]; G[4] = G[19]; G[5] = 0x80u; call_here(rt, ctx, 0x08B3767Cu); G[16] = frame.at(0x4u); L_08856938: ; G[5] = G[19]; G[4] = G[16]; G[17] = frame.at(0x8u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[20] == G[17]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_08856968; } G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); L_08856968: ; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; if (tk) goto L_088569AC; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) goto L_088569AC; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_088569AC: ; F[20] = float_at(rt, frame.at(0xC0u)); goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xACu)); G[20] = word(rt, frame.at(0xB0u)); G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08856AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xD0u); put(rt, frame.at(0xACu), G[19]); put(rt, frame.at(0xB0u), G[20]); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); put(rt, frame.at(0xA0u), G[16]); put(rt, frame.at(0xB4u), G[31]); auto &F = ctx.fpr; G[19] = frame.at(0xCu); G[20] = G[4]; G[4] = G[19]; G[18] = G[7]; G[17] = G[6]; put_float(rt, frame.at(0xC0u), F[20]); call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(0xCu)); { const bool tk = G[16] != 0u; if (tk) goto L_08856B28; } G[4] = 0x08CA0000u; G[4] = 0x08C9E188u; call_here(rt, ctx, 0x08A5BB7Cu); L_08856B28: ; G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[2]; G[5] = frame.at(0x10u); G[6] = frame.at(0x14u); call_here(rt, ctx, 0x088A5A6Cu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = static_cast<std::int32_t>(G[17]) <= 0; F[12] = float_at(rt, frame.at(0x10u)); if (tk) goto L_08856B80; } G[16] = word(rt, G[18] + 0x0u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08856B6C; } G[4] = 0x08CA0000u; G[4] = 0x08C9E14Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08856B6C: ; G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } { const bool tk = G[2] != 0u; F[12] = float_at(rt, frame.at(0x10u)); if (tk) goto L_08856BC4; } L_08856B80: ; G[16] = 0x08D70000u; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48F54u); F[20] = F[0]; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48CB8u); G[19] = frame.at(0x18u); F[20] = std::bit_cast<float>(trunc_to_word(F[20])); G[6] = 0x08CA0000u; G[6] = 0x08C9E518u; G[8] = G[2]; G[7] = bits_of(F[20]); G[4] = G[19]; G[5] = 0x80u; call_here(rt, ctx, 0x08B3767Cu); G[16] = frame.at(0x4u); goto L_08856C1C; L_08856BC4: ; G[16] = 0x08D70000u; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48F54u); F[20] = F[0]; G[4] = 0x08D6950Cu; call_here(rt, ctx, 0x08B48CB8u); G[4] = 0x08D6950Cu; F[20] = std::bit_cast<float>(trunc_to_word(F[20])); G[17] = word(rt, frame.at(0x14u)); G[18] = G[2]; call_here(rt, ctx, 0x08B48CFCu); G[7] = bits_of(F[20]); G[19] = frame.at(0x18u); G[6] = 0x08CA0000u; G[6] = 0x08C9E520u; G[8] = G[18]; G[9] = G[17]; G[10] = G[2]; G[4] = G[19]; G[5] = 0x80u; call_here(rt, ctx, 0x08B3767Cu); G[16] = frame.at(0x4u); L_08856C1C: ; G[5] = G[19]; G[4] = G[16]; G[17] = frame.at(0x8u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[20] == G[17]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_08856C4C; } G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); L_08856C4C: ; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; if (tk) goto L_08856C90; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) goto L_08856C90; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_08856C90: ; F[20] = float_at(rt, frame.at(0xC0u)); goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xACu)); G[20] = word(rt, frame.at(0xB0u)); G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088E44BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[19] = G[4]; G[4] = word(rt, G[4] + 0x0u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x34u); { const std::uint32_t tg = G[2]; G[16] = G[5]; call_here(rt, ctx, tg); } if (G[2] == 0u) { G[4] = word(rt, G[19] + 0x4u); goto L_088E4518; } G[4] = word(rt, G[19] + 0x0u); G[2] = word(rt, G[4] + 0x530u); { const bool tk = G[2] != 0u; G[2] = 0x3u; if (tk) goto L_088E4668; } G[3] = 0x1u; put(rt, G[4] + 0x530u, G[2]); goto L_088E466C; L_088E4518: ; if (G[4] == 0u) { G[2] = word(rt, G[19] + 0x0u); goto L_088E453C; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x34u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } { const bool tk = G[2] != 0u; G[3] = 0x1u; if (tk) goto L_088E466C; } G[2] = word(rt, G[19] + 0x0u); L_088E453C: ; G[20] = word(rt, G[2] + 0xA8u); G[4] = G[20]; call_here(rt, ctx, 0x08B089F8u); { const bool tk = G[2] != 0u; G[21] = 0u; if (tk) goto L_088E4568; } { const bool tk = G[16] == 0u; if (tk) goto L_088E4568; } G[4] = word(rt, G[19] + 0x0u); G[5] = G[16]; call_here(rt, ctx, 0x088ED3F4u); G[21] = load8(G[16] + 0x81u); L_088E4568: ; call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B09A7Cu); G[4] = word(rt, G[19] + 0x0u); call_here(rt, ctx, 0x088ECAC8u); G[4] = word(rt, G[19] + 0x4u); if (G[4] == 0u) { G[17] = word(rt, G[19] + 0x0u); goto L_088E4598; } call_here(rt, ctx, 0x088ECAC8u); G[17] = word(rt, G[19] + 0x0u); L_088E4598: ; G[4] = G[20]; G[2] = word(rt, G[17] + 0x0u); G[18] = word(rt, G[2] + 0x24u); call_here(rt, ctx, 0x08B08A94u); G[16] = G[2]; call_here(rt, ctx, 0x088D4808u); G[6] = G[2]; G[4] = G[17]; { const std::uint32_t tg = G[18]; G[5] = G[16]; call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; G[3] = 0u; if (tk) goto L_088E466C; } G[18] = word(rt, G[19] + 0x4u); { const bool tk = G[18] == 0u; G[4] = G[20]; if (tk) goto L_088E4600; } G[2] = word(rt, G[18] + 0x0u); G[17] = word(rt, G[2] + 0x24u); call_here(rt, ctx, 0x08B08A94u); G[16] = G[2]; call_here(rt, ctx, 0x088D4808u); G[6] = G[2]; G[4] = G[18]; { const std::uint32_t tg = G[17]; G[5] = G[16]; call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; G[3] = 0u; if (tk) goto L_088E466C; } L_088E4600: ; G[4] = word(rt, G[19] + 0x0u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x2Cu); { const std::uint32_t tg = G[2]; G[5] = G[21]; call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; G[3] = 0u; if (tk) goto L_088E466C; } G[4] = word(rt, G[19] + 0x4u); if (G[4] == 0u) { G[2] = word(rt, G[19] + 0x0u); goto L_088E4644; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x2Cu); { const std::uint32_t tg = G[2]; G[5] = G[21]; call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; G[3] = 0u; if (tk) goto L_088E466C; } G[2] = word(rt, G[19] + 0x0u); L_088E4644: ; G[3] = 0x1u; put(rt, G[2] + 0x530u, G[3]); G[2] = word(rt, G[19] + 0x4u); if (G[2] != 0u) { put(rt, G[2] + 0x530u, G[3]); goto L_088E4658; } L_088E4658: ; G[4] = G[20]; call_here(rt, ctx, 0x08B089DCu); G[3] = 0x1u; goto L_088E466C; L_088E4668: ; G[3] = 0x1u; L_088E466C: ; G[2] = G[3]; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[16] = word(rt, frame.at(0x0u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_333
} // namespace gt::game

#define HOT333(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_333::name, gt::ReturnKind::kind)
HOT333(0x088D10E4u, func_088D10E4, Word);
HOT333(0x089B00BCu, func_089B00BC, Word);
// HOT333(0x08B7AE14u, func_08B7AE14, Word);  // no comparable synthetic case yet
HOT333(0x0894C294u, func_0894C294, Word);
// HOT333(0x089CDDF8u, func_089CDDF8, Word);  // no comparable synthetic case yet
HOT333(0x08A94DE0u, func_08A94DE0, Word);
// HOT333(0x08B122C4u, func_08B122C4, Word);  // no comparable synthetic case yet
HOT333(0x08B7B0CCu, func_08B7B0CC, Word);
HOT333(0x08BFA134u, func_08BFA134, Word);
HOT333(0x08C501E0u, func_08C501E0, Word);
HOT333(0x0883E058u, func_0883E058, Word);
HOT333(0x088B243Cu, func_088B243C, Word);
HOT333(0x0894AA7Cu, func_0894AA7C, Word);
HOT333(0x08B5C7D0u, func_08B5C7D0, Word);
HOT333(0x0883FD7Cu, func_0883FD7C, Word);
HOT333(0x08844D1Cu, func_08844D1C, Word);
HOT333(0x08890788u, func_08890788, Word);
HOT333(0x08AEAB24u, func_08AEAB24, Word);
HOT333(0x08AEE934u, func_08AEE934, Word);
HOT333(0x08B6E74Cu, func_08B6E74C, Word);
HOT333(0x08BFA724u, func_08BFA724, Word);
HOT333(0x088205BCu, func_088205BC, Word);
// HOT333(0x088567F4u, func_088567F4, Word);  // no comparable synthetic case yet
// HOT333(0x08856AD8u, func_08856AD8, Word);  // no comparable synthetic case yet
// HOT333(0x088E44BCu, func_088E44BC, Word);  // no comparable synthetic case yet
