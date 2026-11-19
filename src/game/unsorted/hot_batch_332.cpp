// Hot batch 332: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_332 {
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
// `wsbw`: the bytes of the word reversed (these records are big-endian).
u32 wsbw(u32 value) { return __builtin_bswap32(value); }
u32 signed_half(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}
} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

// 0x0888FC28 (a0 = a header record, or 0): 1 when the record holds the values the game derives now, else 0 (0 for a null record too). The values come from calls with no
// arguments: 0x088D4800 first (a0 stays the record), then s2 = 0x0888F390, s3 = 0x08894020, s4 = 0x088A1978, s5 = 0x088A6C78, s6 = 0x088A1954, s7 = 0x088D06A4, fp = 0x08898874(the
// byte-swapped word at +0xD8), s0 = 0x088975C8 and 0x08898230 (kept at frame +0 and +4). The record's big-endian words are then compared: +0 with 0x52504C59 ("RPLY"), 0x0888FC14(record, 0) non-zero,
// 0x088A364C(.., 0) not below the word at +0x10, s2 with +0x98, s3 with +0x9C, s4 with +0xA8, s5 with +0xB8, s6 with +0xC0, s7 with +0xC4, fp with +0xE4 (only when that one is not 0),
// s0 with +0xC8, the frame +4 value with +0xD0 and the frame +0 value with +0xD4; every one must be equal (the +0x10 test: not above) for v0 = 1.
void func_0888FC28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}});
    ctx.gpr[17] = record;
    auto done = [&](u32 result) { ctx.gpr[5] = result; ctx.gpr[2] = result; };
    if (record == 0u) return done(0u);
    const u32 stored = word(rt, record + 0xD8u);
    ctx.gpr[16] = stored;
    call_here(rt, ctx, 0x088D4800u);
    const u32 s2 = (call_here(rt, ctx, 0x0888F390u), ctx.gpr[2]);
    ctx.gpr[18] = s2;
    const u32 s3 = (call_here(rt, ctx, 0x08894020u), ctx.gpr[2]);
    ctx.gpr[19] = s3;
    const u32 s4 = (call_here(rt, ctx, 0x088A1978u), ctx.gpr[2]);
    ctx.gpr[20] = s4;
    const u32 s5 = (call_here(rt, ctx, 0x088A6C78u), ctx.gpr[2]);
    ctx.gpr[21] = s5;
    ctx.gpr[16] = wsbw(stored);
    const u32 s6 = (call_here(rt, ctx, 0x088A1954u), ctx.gpr[2]);
    ctx.gpr[22] = s6;
    const u32 s7 = (call_here(rt, ctx, 0x088D06A4u), ctx.gpr[2]);
    ctx.gpr[23] = s7;
    ctx.gpr[4] = ctx.gpr[16];
    const u32 fp = (call_here(rt, ctx, 0x08898874u), ctx.gpr[2]);
    ctx.gpr[30] = fp;
    const u32 s0 = (call_here(rt, ctx, 0x088975C8u), ctx.gpr[2]);
    ctx.gpr[16] = s0;
    call_here(rt, ctx, 0x08898230u);
    const u32 clock = ctx.gpr[2];
    put(rt, frame.at(0x4u), clock);
    put(rt, frame.at(0x0u), clock);
    ctx.gpr[2] = 0x52504C59u;
    ctx.gpr[3] = wsbw(word(rt, record + 0x0u));
    ctx.gpr[5] = 0u;
    if (ctx.gpr[3] != 0x52504C59u) return done(0u);
    if (call_set(rt, ctx, 0x0888FC14u, {record, 0u}) == 0u) return done(0u);
    ctx.gpr[5] = 0u;
    call_here(rt, ctx, 0x088A364Cu);
    ctx.gpr[3] = wsbw(word(rt, record + 0x10u));
    if (static_cast<s32>(ctx.gpr[2]) < static_cast<s32>(ctx.gpr[3])) return done(0u);
    const std::pair<u32, u32> checks[] = {{s2, 0x98u}, {s3, 0x9Cu}, {s4, 0xA8u}, {s5, 0xB8u}, {s6, 0xC0u}, {s7, 0xC4u}};
    for (const auto &check : checks) {
        ctx.gpr[2] = wsbw(word(rt, record + check.second));
        if (check.first != ctx.gpr[2]) return done(0u);
    }
    ctx.gpr[2] = wsbw(word(rt, record + 0xE4u));
    if (ctx.gpr[2] != 0u && fp != ctx.gpr[2]) return done(0u);
    ctx.gpr[2] = wsbw(word(rt, record + 0xC8u));
    if (s0 != ctx.gpr[2]) return done(0u);
    ctx.gpr[2] = wsbw(word(rt, record + 0xD0u));
    ctx.gpr[3] = word(rt, frame.at(0x4u));
    if (ctx.gpr[3] != ctx.gpr[2]) return done(0u);
    ctx.gpr[2] = wsbw(word(rt, record + 0xD4u));
    ctx.gpr[3] = word(rt, frame.at(0x0u));
    ctx.gpr[5] = 1u;
    done(ctx.gpr[3] == ctx.gpr[2] ? 1u : 0u);
}
void func_088A76F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[17] = G[5]; G[16] = G[4]; G[3] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[5] + 0x4u); G[3] = ((G[3] & 0xFFu) << 24u) | ((G[3] & 0xFF00u) << 8u) | ((G[3] & 0xFF0000u) >> 8u) | ((G[3] & 0xFF000000u) >> 24u); G[4] = ((G[2] & 0xFFu) << 24u) | ((G[2] & 0xFF00u) << 8u) | ((G[2] & 0xFF0000u) >> 8u) | ((G[2] & 0xFF000000u) >> 24u); { const bool tk = G[3] != G[4]; put(rt, frame.at(0x0u), G[2]); if (tk) goto L_088A7888; } G[3] = sext(load8(G[16] + 0x8u), 8); G[2] = sext(load8(G[5] + 0x8u), 8); { const bool tk = G[3] != G[2]; G[6] = 0u; if (tk) goto L_088A788C; } G[3] = sext(load8(G[16] + 0x9u), 8); G[2] = sext(load8(G[5] + 0x9u), 8); { const bool tk = G[3] != G[2]; if (tk) goto L_088A7890; } G[3] = sext(load8(G[16] + 0xAu), 8); G[2] = sext(load8(G[5] + 0xAu), 8); { const bool tk = G[3] != G[2]; if (tk) goto L_088A7894; } G[3] = sext(load8(G[16] + 0xBu), 8); G[2] = sext(load8(G[5] + 0xBu), 8); { const bool tk = G[3] != G[2]; if (tk) goto L_088A7894; } G[20] = 0u; G[21] = 0x3u; G[19] = G[5] + 0x38u; G[18] = G[16] + 0x38u; G[4] = G[18]; L_088A778C: ; G[5] = G[19]; call_here(rt, ctx, 0x08932494u); G[20] = G[20] + 0x1u; G[18] = G[18] + 0x180u; { const bool tk = G[2] == 0u; G[19] = G[19] + 0x180u; if (tk) goto L_088A7888; } { const bool tk = G[20] != G[21]; G[4] = G[18]; if (tk) goto L_088A778C; } G[4] = G[16] + 0xCu; G[5] = G[17] + 0xCu; call_here(rt, ctx, 0x0895832Cu); { const bool tk = G[2] == 0u; G[6] = 0u; if (tk) goto L_088A788C; } G[3] = sext(load8(G[16] + 0x2Cu), 8); G[2] = sext(load8(G[17] + 0x2Cu), 8); { const bool tk = G[3] != G[2]; if (tk) goto L_088A7890; } G[3] = sext(load8(G[16] + 0x2Du), 8); G[2] = sext(load8(G[17] + 0x2Du), 8); if (G[3] != G[2]) { goto L_088A7894; } G[3] = sext(load8(G[16] + 0x2Eu), 8); G[2] = sext(load8(G[17] + 0x2Eu), 8); if (G[3] != G[2]) { goto L_088A7894; } G[3] = sext(load8(G[16] + 0x2Fu), 8); G[2] = sext(load8(G[17] + 0x2Fu), 8); if (G[3] != G[2]) { goto L_088A7894; } G[2] = word(rt, G[17] + 0x30u); G[3] = word(rt, G[16] + 0x30u); G[4] = ((G[2] & 0xFFu) << 24u) | ((G[2] & 0xFF00u) << 8u) | ((G[2] & 0xFF0000u) >> 8u) | ((G[2] & 0xFF000000u) >> 24u); G[3] = ((G[3] & 0xFFu) << 24u) | ((G[3] & 0xFF00u) << 8u) | ((G[3] & 0xFF0000u) >> 8u) | ((G[3] & 0xFF000000u) >> 24u); { const bool tk = G[3] != G[4]; put(rt, frame.at(0x0u), G[2]); if (tk) goto L_088A788C; } G[4] = load16(G[17] + 0x4B8u); G[3] = load16(G[16] + 0x4B8u); G[2] = ((G[4] & 0x00FF00FFu) << 8u) | ((G[4] & 0xFF00FF00u) >> 8u); G[3] = ((G[3] & 0x00FF00FFu) << 8u) | ((G[3] & 0xFF00FF00u) >> 8u); { const bool tk = G[3] != G[2]; store16(frame.at(0x0u), G[4]); if (tk) goto L_088A788C; } G[4] = load16(G[17] + 0x4BAu); G[3] = load16(G[16] + 0x4BAu); G[2] = ((G[4] & 0x00FF00FFu) << 8u) | ((G[4] & 0xFF00FF00u) >> 8u); G[3] = ((G[3] & 0x00FF00FFu) << 8u) | ((G[3] & 0xFF00FF00u) >> 8u); { const bool tk = G[3] != G[2]; store16(frame.at(0x0u), G[4]); if (tk) goto L_088A788C; } G[4] = load16(G[17] + 0x4BCu); G[3] = load16(G[16] + 0x4BCu); G[2] = ((G[4] & 0x00FF00FFu) << 8u) | ((G[4] & 0xFF00FF00u) >> 8u); G[3] = ((G[3] & 0x00FF00FFu) << 8u) | ((G[3] & 0xFF00FF00u) >> 8u); { const bool tk = G[3] != G[2]; store16(frame.at(0x0u), G[4]); if (tk) goto L_088A788C; } G[4] = load16(G[17] + 0x4BEu); G[3] = load16(G[16] + 0x4BEu); G[2] = ((G[4] & 0x00FF00FFu) << 8u) | ((G[4] & 0xFF00FF00u) >> 8u); G[3] = ((G[3] & 0x00FF00FFu) << 8u) | ((G[3] & 0xFF00FF00u) >> 8u); { const bool tk = G[3] != G[2]; store16(frame.at(0x0u), G[4]); if (tk) goto L_088A788C; } G[3] = word(rt, G[16] + 0x4D0u); G[2] = word(rt, G[17] + 0x4D0u); { const bool tk = G[3] == G[2]; G[6] = 0x1u; if (tk) goto L_088A788C; } L_088A7888: ; G[6] = 0u; L_088A788C: ; L_088A7890: ; L_088A7894: ; G[2] = G[6]; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x088AD4C4 (a0 = the object, a1 = the output pair of floats, a2 = a 4-float vector, a3 = another record): works out a pair of floats. The object's floats at +0x3E4 (negated) and +0x3E0
// and the vector are kept on the frame, 0x08B4D0C8 and 0x08B4AD90 turn them into a pair (frame +0x10); that pair goes through 0x08B4ABEC with the object's current entry (+0x284 + index
// x 40, the index at +0x3E8) and, scaled by -1 / the float at +0x430, into 0x088AB868(frame +0x18, vector, entry). A float s = 1 / (float at +0x42C) + the scaled value x 0x08B4ABCC(frame
// +0x18, pair). When s is not 0 the fourth argument f12 is -(constant at 0x08CDD108 + 1) x 0x08B4ABCC(a3, pair) / s, else 0. 0x08B4A560(pair copy) leaves the output pair.
void func_088AD4C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], output = ctx.gpr[5], vector = ctx.gpr[6], extra = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x50u}, {23u, 0x4Cu}, {22u, 0x48u}, {21u, 0x44u}, {20u, 0x40u}, {19u, 0x3Cu}, {18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u},
                                          {55u, 0x6Cu}, {53u, 0x64u}, {52u, 0x60u}, {54u, 0x68u}});
    auto &F = ctx.fpr;
    const u32 s5 = frame.at(0x18u), s4 = frame.at(0x10u), s3 = frame.at(0x8u), s2 = frame.at(0x20u);
    ctx.gpr[23] = output;
    ctx.gpr[22] = extra;
    ctx.gpr[21] = s5;
    ctx.gpr[20] = s4;
    ctx.gpr[19] = s3;
    ctx.gpr[18] = s2;
    ctx.gpr[17] = vector;
    ctx.gpr[16] = self;
    ctx.gpr[1] = 0x3F800000u;
    F[23] = 1.0f;
    F[0] = float_at(rt, self + 0x3E4u);
    F[3] = float_at(rt, vector + 0x8u);
    F[4] = float_at(rt, vector + 0x4u);
    F[0] = -F[0];
    F[5] = float_at(rt, vector + 0xCu);
    F[1] = float_at(rt, self + 0x3E0u);
    F[2] = float_at(rt, vector + 0x0u);
    ctx.gpr[4] = s2;
    put_float(rt, frame.at(0x28u), F[3]);
    put_float(rt, frame.at(0x24u), F[4]);
    put_float(rt, frame.at(0x2Cu), F[5]);
    put_float(rt, frame.at(0xCu), F[1]);
    put_float(rt, frame.at(0x20u), F[2]);
    put_float(rt, frame.at(0x8u), F[0]);
    F[22] = float_at(rt, 0x08CDD108u);
    call_set(rt, ctx, 0x08B4D0C8u, {s2});
    call_set(rt, ctx, 0x08B4AD90u, {s4, s2, s3});
    // the object's current entry: +0x284 + index x 40
    auto entry = [&] {
        const u32 index = word(rt, self + 0x3E8u);
        return (index << 3u) + (index << 5u) + self + 0x284u;
    };
    call_set(rt, ctx, 0x08B4ABECu, {entry(), s4});
    F[2] = float_at(rt, self + 0x430u);
    ctx.gpr[1] = 0xBF800000u;
    F[1] = -1.0f;
    const u32 current = entry();
    F[1] = F[1] / F[2];
    F[21] = psp_mul(F[0], F[1]);
    call_set(rt, ctx, 0x088AB868u, {s5, vector, current});
    F[20] = float_at(rt, self + 0x42Cu);
    F[20] = F[23] / F[20];
    call_set(rt, ctx, 0x08B4ABCCu, {s5, s3});
    F[21] = psp_mul(F[21], F[0]);
    F[12] = std::bit_cast<float>(0u);
    ctx.gpr[4] = extra;
    F[20] = F[20] + F[21];
    ctx.set_fpu_condition(F[20] == F[12]);
    ctx.gpr[5] = s3;
    if (!ctx.fpu_condition()) {
        call_set(rt, ctx, 0x08B4ABCCu, {extra});
        F[1] = F[22] + F[23];
        F[1] = -F[1];
        F[1] = psp_mul(F[1], F[0]);
        F[12] = F[1] / F[20];
    }
    F[0] = float_at(rt, frame.at(0x8u));
    F[1] = float_at(rt, frame.at(0xCu));
    ctx.gpr[4] = frame.at(0x0u);
    put_float(rt, frame.at(0x0u), F[0]);
    put_float(rt, frame.at(0x4u), F[1]);
    call_set(rt, ctx, 0x08B4A560u, {frame.at(0x0u)});
    F[1] = float_at(rt, frame.at(0x4u));
    F[0] = float_at(rt, frame.at(0x0u));
    put_float(rt, output + 0x4u, F[1]);
    put_float(rt, output + 0x0u, F[0]);
}
void func_088EC61C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x14u), G[31]); G[20] = G[5]; G[19] = 0x4u; G[18] = G[6] + 0x4E8u; G[17] = G[6]; G[16] = 0x2u; goto L_088EC664; L_088EC650: ; G[2] = word(rt, G[17] + 0x4E8u); { const bool tk = G[2] == G[19]; G[4] = 0x1F4u; if (tk) goto L_088EC684; } call_here(rt, ctx, 0x08B2E2B8u); L_088EC664: ; G[3] = word(rt, G[18] + 0x0u); { const bool tk = G[3] != 0u; G[2] = G[16]; if (tk) goto L_088EC67C; } put(rt, G[18] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_088EC664; } L_088EC67C: ; { const bool tk = G[3] != 0u; if (tk) goto L_088EC650; } L_088EC684: ; G[4] = word(rt, G[17] + 0x538u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x74u); { const std::uint32_t tg = G[2]; G[5] = G[20]; call_here(rt, ctx, tg); } G[19] = G[2]; G[5] = word(rt, G[17] + 0x4ECu); G[2] = word(rt, G[17] + 0x4F0u); { const bool tk = G[5] != G[2]; G[3] = G[5] << 5; if (tk) goto L_088EC79C; } goto L_088EC700; L_088EC6B4: ; G[2] = word(rt, G[2] + 0x4u); if (G[19] != G[2]) { G[2] = G[5] + 0x1u; goto L_088EC6D0; } put(rt, G[6] + 0x0u, 0u); L_088EC6C4: ; goto L_088EC700; L_088EC6CC: ; G[2] = G[5] + 0x1u; L_088EC6D0: ; G[3] = sra(G[2], 31); G[3] = G[3] >> 30; G[2] = G[2] + G[3]; G[2] = G[2] & 0x3u; G[5] = G[2] - G[3]; G[4] = word(rt, G[17] + 0x4F0u); G[2] = G[5] << 3; G[3] = G[5] << 5; G[2] = G[2] + G[3]; G[2] = G[17] + G[2]; { const bool tk = G[4] != G[5]; G[6] = G[2] + 0x4u; if (tk) goto L_088EC6B4; } L_088EC700: ; { const bool tk = G[19] == 0u; if (tk) goto L_088EC774; } G[4] = sext(load8(G[19] + 0x0u), 8); { const bool tk = static_cast<std::int32_t>(G[4]) < 0; G[2] = static_cast<std::int32_t>(G[4]) < 4 ? 1u : 0u; if (tk) goto L_088EC774; } { const bool tk = G[2] == 0u; if (tk) goto L_088EC774; } G[2] = word(rt, G[17] + 0xB8u); { const bool tk = G[2] == 0u; G[16] = G[17] + 0xC4u; if (tk) goto L_088EC734; } { const std::uint32_t tg = G[2]; G[5] = word(rt, G[17] + 0xBCu); call_here(rt, ctx, tg); } G[16] = G[17] + 0xC4u; L_088EC734: ; G[4] = G[16]; call_here(rt, ctx, 0x08B344E0u); G[2] = sext(load8(G[19] + 0x0u), 8); G[4] = word(rt, G[17] + 0x11Cu); G[3] = 0xFFFFFFFFu; G[2] = G[2] << 1; G[2] = G[2] + G[4]; store16(G[2] + 0x0u, G[3]); G[4] = G[16]; G[2] = word(rt, G[17] + 0x11Cu); store8(G[2] + 0x1Au, 0u); call_here(rt, ctx, 0x08B3457Cu); G[4] = word(rt, G[17] + 0x118u); G[5] = sext(load8(G[19] + 0x0u), 8); call_here(rt, ctx, 0x088EEE98u); put(rt, G[2] + 0x0u, 0u); L_088EC774: ; G[2] = word(rt, G[18] + 0x0u); G[2] = G[2] ^ 0x2u; G[2] = 0u < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; if (tk) goto L_088EC7BC; } put(rt, G[18] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] != 0u; if (tk) goto L_088EC7BC; } goto L_088EC774; L_088EC79C: ; G[2] = G[5] << 3; G[2] = G[2] + G[3]; G[2] = G[17] + G[2]; G[3] = word(rt, G[2] + 0x4u); { const bool tk = G[19] != G[3]; G[6] = G[2] + 0x4u; if (tk) goto L_088EC6CC; } put(rt, G[6] + 0x0u, 0u); goto L_088EC6C4; L_088EC7BC: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08ACA144 (a0 = a code): maps a code to the 0x8000xxxx code the game reports, by a tree of signed comparisons. 0x80550901..0x80550902 and 0x80550906: 0x80001028; 0x80550903: 0x8000102B;
// 0x80550904, 0x80550905, 0x80550907..0x80550909: 0x80001029; 0x80550910: 0x8000102A; 0x80550911: 0x8000102B; 0x80550912, 0x80550916: 0x80001028; 0x80550913, 0x80550917: 0x8000102C;
// 0x80550914: 0x8000102D; 0x80550915: 0x8000102E; 0x80020193, 0x80020320, 0x80020323, 0x80020324: 0x80001015; 0x80020321: 0x80001016; 0x80020325: 0x80001017; 0x80000001: 0x8000102F;
// 0x80010002: 0x80001014; every other code: 0x80001063. v0 (and a2) is the result, a1 holds 0x80550000.
void func_08ACA144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const s32 code = static_cast<s32>(ctx.gpr[4]);
    const s32 base = static_cast<s32>(0x80550000u);
    const u32 error = 0x80000000u;
    ctx.gpr[5] = 0x80550000u;
    u32 result = error | 0x1063u;
    auto done = [&](u32 low) {
        result = error | low;
        ctx.gpr[6] = result;
        ctx.gpr[2] = result;
    };
    if (base + 0x905 < code) {
        ctx.gpr[2] = 0x80550000u | 0x912u;
        if (code == base + 0x912) return done(0x1028u);
        if (base + 0x912 < code) {
            ctx.gpr[3] = 0x80550000u | 0x915u;
            if (code == base + 0x915) return done(0x102Eu);
            if (base + 0x915 < code) {
                if (code == base + 0x916) return done(0x1028u);
                return done(code == base + 0x917 ? 0x102Cu : 0x1063u);
            }
            if (code == base + 0x913) return done(0x102Cu);
            return done(code == base + 0x914 ? 0x102Du : 0x1063u);
        }
        if (base + 0x909 < code) {
            ctx.gpr[3] = 0x80550000u | 0x910u;
            if (code == base + 0x910) return done(0x102Au);
            return done(code == base + 0x911 ? 0x102Bu : 0x1063u);
        }
        if (!(code < base + 0x907)) return done(0x1029u);
        return done(0x1028u);
    }
    if (!(code < base + 0x904)) return done(0x1029u);
    const s32 low = static_cast<s32>(0x80020000u);
    if (low + 0x324 < code) {
        if (base + 0x902 < code) return done(0x102Bu);
        if (!(code < base + 0x901)) return done(0x1028u);
        return done(code == low + 0x325 ? 0x1017u : 0x1063u);
    }
    if (!(code < low + 0x323)) return done(0x1015u);
    if (code == low + 0x193) return done(0x1015u);
    if (low + 0x193 < code) {
        if (code == low + 0x320) return done(0x1015u);
        return done(code == low + 0x321 ? 0x1016u : 0x1063u);
    }
    ctx.gpr[3] = error;
    if (code == static_cast<s32>(0x80000001u)) return done(0x102Fu);
    if (code != static_cast<s32>(0x80010002u)) return done(0x1063u);
    done(0x1014u);
}
// 0x08B7EF8C (a0 = a character, a1 = a stream record): `ungetc`. -1 as the character is refused. The thread's reent record (0x08B796C4; its +0x38 clear calls 0x08B78430(record)) is
// fetched first. The stream's flags (halfword +0xC) lose bit 0x20; without bit 4 the stream needs bit 0x10 (else -1), and with bit 8 as well 0x08B7807C(stream) must return 0 (the words +8 and +0x18
// are cleared, bit 8 dropped); bit 4 is then set. With a push-back buffer (+0x30) the byte goes in front of the read pointer (+0), growing the count (+4), after making room with
// 0x08B7EE5C(stream) when the count is not below the buffer size (+0x34). Without one a byte equal to the one before the read pointer (the stream's own buffer, +0x10 as its start) just steps the
// pointer back; otherwise the small buffer inside the record (+0x40..+0x42) takes it, the old pointer / count being kept at +0x38 / +0x3C. v0 = the character as a byte, or -1.
void func_08B7EF8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 character = ctx.gpr[4], stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}});
    ctx.gpr[17] = character;
    ctx.gpr[16] = stream;
    ctx.gpr[2] = 0xFFFFFFFFu;
    auto fail = [&] {
        ctx.gpr[17] = 0xFFFFFFFFu;
        ctx.gpr[2] = 0xFFFFFFFFu;
    };
    if (character == 0xFFFFFFFFu) return fail();
    call_here(rt, ctx, 0x08B796C4u);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x38u);
    if (ctx.gpr[3] == 0u) {
        call_here(rt, ctx, 0x08B796C4u);
        call_set(rt, ctx, 0x08B78430u, {ctx.gpr[2]});
    }
    u32 flags = signed_half(rt, stream + 0xCu) & ~0x20u;
    ctx.gpr[4] = flags & 0xFFFFu;
    store16(stream + 0xCu, flags);
    ctx.gpr[5] = flags;
    if ((flags & 0x4u) == 0u) {
        if ((flags & 0x10u) == 0u) return fail();
        if ((flags & 0x8u) != 0u) {
            if (call_set(rt, ctx, 0x08B7807Cu, {stream}) != 0u) return fail();
            const u32 fresh = load16(stream + 0xCu);
            put(rt, stream + 0x8u, 0u);
            flags = fresh & ~0x8u;
            store16(stream + 0xCu, flags);
            ctx.gpr[5] = flags;
            put(rt, stream + 0x18u, 0u);
        }
        store16(stream + 0xCu, flags | 0x4u);
    }
    const u32 byte = character & 0xFFu;
    ctx.gpr[17] = byte;
    if (word(rt, stream + 0x30u) != 0u) {
        if (!(static_cast<s32>(word(rt, stream + 0x4u)) < static_cast<s32>(word(rt, stream + 0x34u)))) {
            if (call_set(rt, ctx, 0x08B7EE5Cu, {stream}) != 0u) return fail();
        }
        const u32 pointer = word(rt, stream + 0x0u);
        put(rt, stream + 0x0u, pointer - 1u);
        store8(pointer - 1u, byte);
        put(rt, stream + 0x4u, word(rt, stream + 0x4u) + 1u);
        ctx.gpr[2] = byte;
        return;
    }
    const u32 limit = word(rt, stream + 0x10u), pointer = word(rt, stream + 0x0u);
    if (limit != 0u && limit < pointer && byte == load8(pointer - 1u)) {
        put(rt, stream + 0x0u, pointer - 1u);
        put(rt, stream + 0x4u, word(rt, stream + 0x4u) + 1u);
        ctx.gpr[2] = byte;
        return;
    }
    const u32 count = word(rt, stream + 0x4u);
    put(rt, stream + 0x4u, 1u);
    put(rt, stream + 0x34u, 3u);
    put(rt, stream + 0x3Cu, count);
    put(rt, stream + 0x38u, pointer);
    put(rt, stream + 0x30u, stream + 0x40u);
    put(rt, stream + 0x0u, stream + 0x42u);
    store8(stream + 0x42u, byte);
    ctx.gpr[2] = byte;
}
void func_08BAFD74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0x0u), G[16]); G[8] = G[6]; G[19] = G[4]; G[18] = G[7]; G[16] = word(rt, G[4] + 0x0u); G[3] = word(rt, G[16] + 0xFFFFFFF4u); G[2] = G[3] < G[5] ? 1u : 0u; { const bool tk = G[2] == 0u; G[17] = G[5]; if (tk) goto L_08BAFDB8; } G[4] = 0x08CA0000u; G[4] = 0x08CA09E0u; call_here(rt, ctx, 0x08B5AEDCu); L_08BAFDB8: ; G[2] = 0x3FFF0000u; G[2] = G[2] | 0xFFFCu; G[2] = G[2] - G[3]; G[2] = G[2] < G[7] ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = G[6] < G[16] ? 1u : 0u; if (tk) goto L_08BAFDDC; } G[4] = 0x08CA0000u; G[4] = 0x08CA09E0u; call_here(rt, ctx, 0x08B5ADDCu); L_08BAFDDC: ; { const bool tk = G[2] != 0u; G[4] = G[19]; if (tk) goto L_08BAFDF8; } G[2] = G[16] + G[3]; G[2] = G[2] < G[6] ? 1u : 0u; if (G[2] == 0u) { G[2] = word(rt, G[16] + 0xFFFFFFFCu); goto L_08BAFF00; } G[4] = G[19]; L_08BAFDF8: ; G[5] = G[17]; G[7] = G[8]; G[8] = G[18]; G[6] = 0u; tail = 0x08B94540u; goto exit_; L_08BAFE24: ; { const bool tk = G[18] != G[2]; G[4] = G[16]; if (tk) goto L_08BAFE38; } G[2] = load8(G[5] + 0x0u); store8(G[16] + 0x0u, G[2]); goto L_08BAFF10; L_08BAFE38: ; G[6] = G[18]; call_here(rt, ctx, 0x08B79A88u); G[2] = G[19]; goto L_08BAFF14; L_08BAFE48: ; { const bool tk = G[2] != 0u; G[17] = G[16] - G[5]; if (tk) goto L_08BAFE7C; } G[2] = 0x1u; { const bool tk = G[18] != G[2]; G[4] = G[16]; if (tk) goto L_08BAFE68; } G[2] = load8(G[3] + 0x0u); store8(G[16] + 0x0u, G[2]); goto L_08BAFF10; L_08BAFE68: ; G[5] = G[3]; G[6] = G[18]; call_here(rt, ctx, 0x08B79A88u); G[2] = G[19]; goto L_08BAFF14; L_08BAFE7C: ; G[2] = 0x1u; { const bool tk = G[17] != G[2]; G[4] = G[16]; if (tk) goto L_08BAFE94; } G[2] = load8(G[5] + 0x0u); store8(G[16] + 0x0u, G[2]); goto L_08BAFE9C; L_08BAFE94: ; G[6] = G[17]; call_here(rt, ctx, 0x08B79A88u); L_08BAFE9C: ; G[6] = G[18] - G[17]; G[2] = 0x1u; G[5] = G[16] + G[18]; { const bool tk = G[6] != G[2]; G[4] = G[16] + G[17]; if (tk) goto L_08BAFEBC; } G[2] = load8(G[5] + 0x0u); store8(G[4] + 0x0u, G[2]); goto L_08BAFF10; L_08BAFEBC: ; call_here(rt, ctx, 0x08B79A88u); G[2] = G[19]; goto L_08BAFF14; L_08BAFECC: ; G[6] = 0u; G[7] = G[18]; G[16] = G[8] - G[16]; call_here(rt, ctx, 0x08B943A0u); G[2] = word(rt, G[19] + 0x0u); G[5] = G[2] + G[16]; G[3] = G[5] + G[18]; G[16] = G[2] + G[17]; G[2] = G[16] < G[3] ? 1u : 0u; if (G[2] == 0u) { G[2] = 0x1u; goto L_08BAFE24; } G[2] = G[5] < G[16] ? 1u : 0u; goto L_08BAFE48; L_08BAFF00: ; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[19]; if (tk) goto L_08BAFDF8; } G[5] = G[17]; goto L_08BAFECC; L_08BAFF10: ; G[2] = G[19]; L_08BAFF14: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0898AB30 (a0 = the destination string, a2 = a mode, a3 = a holder; other modes do nothing): a work object is made on the frame (+0x10, 0x089896CC; a null one calls 0x08A5BB7C(0x08CA39F0)).
// Mode 0: the destination takes the text 0x089893DC(object) gives (0x08A6D2D4 into frame +0xC, then 0x08A5BAD8(destination, text) unless it is that slot) and both are released (0x08A5B9EC).
// Mode 1: with a holder (+0 of a3; a null object calls 0x08A5BB7C(0x08CA39E8)) its virtual +0xC makes a text (frame +8) that 0x0898949C(object, text) takes; with none the
// fixed text 0x08C8C0C4 (0x08B85EB8) is used instead. The text is released (count drop 0x08B60804, free 0x08B8542C of a header that is not the empty one 0x08D74FD0), then the work object.
void func_0898AB30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], mode = ctx.gpr[6], holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x24u}, {16u, 0x20u}, {31u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}});
    ctx.gpr[17] = destination;
    ctx.gpr[16] = holder;
    const u32 empty = 0x08D74FD0u;
    // releases the counted string at `slot`
    auto release = [&](u32 slot, u32 pad) {
        const u32 data = word(rt, slot);
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        if (header == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, pad});
    };
    if (mode == 0u) {
        const u32 work = frame.at(0x10u);
        ctx.gpr[18] = work;
        call_set(rt, ctx, 0x089896CCu, {work});
        const u32 object = word(rt, work);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
        const u32 text = call_set(rt, ctx, 0x089893DCu, {object});
        const u32 slot = frame.at(0xCu);
        ctx.gpr[16] = slot;
        call_set(rt, ctx, 0x08A6D2D4u, {slot, text});
        if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
        call_set(rt, ctx, 0x08A5B9ECu, {work});
        return;
    }
    if (mode != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 work = frame.at(0x10u);
    ctx.gpr[19] = work;
    call_set(rt, ctx, 0x089896CCu, {work});
    u32 source = word(rt, holder + 0x0u);
    ctx.gpr[17] = source;
    const u32 object = word(rt, frame.at(0x10u));
    ctx.gpr[18] = object;
    if (source != 0u) {
        if (object == 0u) {
            call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
            source = word(rt, holder + 0x0u);
            ctx.gpr[17] = source;
        }
        if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39E8u});
        const u32 text = frame.at(0x8u);
        ctx.gpr[16] = text;
        call_set(rt, ctx, word(rt, word(rt, source) + 0xCu), {text, source});
        call_set(rt, ctx, 0x0898949Cu, {object, text});
        release(text, frame.at(0x1u));
    } else {
        ctx.gpr[17] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
        const u32 text = frame.at(0x4u);
        ctx.gpr[16] = text;
        call_set(rt, ctx, 0x08B85EB8u, {text, 0x08C8C0C4u, frame.at(0x2u)});
        call_set(rt, ctx, 0x0898949Cu, {object, text});
        release(text, frame.at(0x0u));
    }
    call_set(rt, ctx, 0x08A5B9ECu, {work});
}
void func_08990D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[18] = G[5]; G[17] = G[4]; G[19] = G[17] + 0x130u; G[16] = 0u; put_float(rt, frame.at(0x30u), F[20]); call_here(rt, ctx, 0x08990A1Cu); G[4] = G[18]; G[20] = G[2]; call_here(rt, ctx, 0x089AD4B8u); G[4] = G[19]; G[5] = G[18]; call_here(rt, ctx, 0x08983F74u); G[3] = word(rt, G[17] + 0x220u); G[2] = word(rt, G[17] + 0x224u); { const bool tk = G[3] == G[2]; put(rt, frame.at(0x0u), G[3]); if (tk) goto L_08990DF8; } G[2] = G[3] + G[17]; L_08990DCC: ; G[3] = sext(load8(G[2] + 0x428u), 8); G[4] = G[17]; G[5] = frame.at(0u); G[6] = 0x80u; G[7] = 0x1u; G[16] = G[16] + G[3]; call_here(rt, ctx, 0x0898D9E8u); G[3] = word(rt, frame.at(0x0u)); G[2] = word(rt, G[17] + 0x224u); { const bool tk = G[3] != G[2]; G[2] = G[3] + G[17]; if (tk) goto L_08990DCC; } L_08990DF8: ; put(rt, G[17] + 0x4A8u, G[16]); G[4] = G[17]; G[5] = 0u; G[6] = 0u; G[7] = 0u; G[8] = frame.at(0u); G[16] = 0x08CF0000u; call_here(rt, ctx, 0x0896A35Cu); G[2] = word(rt, 0x08CF68E8u); G[1] = 0x41600000u; F[20] = std::bit_cast<float>(G[1]); G[4] = G[17]; F[2] = float_at(rt, G[2] + 0x68u); F[0] = float_at(rt, G[2] + 0x24u); G[5] = 0u; F[2] = F[2] + F[20]; G[6] = 0u; G[7] = 0u; G[8] = frame.at(0u); F[2] = psp_mul(F[2], F[0]); F[0] = float_at(rt, frame.at(0x0u)); F[0] = F[0] / F[2]; F[1] = std::bit_cast<float>(trunc_to_word(F[0])); put_float(rt, G[17] + 0x4ACu, F[1]); call_here(rt, ctx, 0x0896A35Cu); G[2] = word(rt, 0x08CF68E8u); F[0] = float_at(rt, G[17] + 0x4ACu); G[3] = word(rt, G[17] + 0x4B4u); F[1] = float_at(rt, G[2] + 0x68u); F[3] = float_at(rt, G[2] + 0x24u); F[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[1] = F[1] + F[20]; F[0] = float_at(rt, frame.at(0x0u)); F[1] = psp_mul(F[1], F[3]); F[2] = psp_mul(F[2], F[1]); F[0] = F[0] - F[2]; { const bool tk = G[3] == 0u; put_float(rt, G[17] + 0x4B0u, F[0]); if (tk) goto L_08990EA4; } G[2] = 0x1u; { const bool tk = G[3] != G[2]; F[1] = std::bit_cast<float>(G[20]); if (tk) goto L_08990EF8; } G[2] = load8(G[17] + 0x4C0u); goto L_08990EB4; L_08990EA4: ; G[4] = G[17]; call_here(rt, ctx, 0x0898DDB8u); goto L_08990EF8; L_08990EB4: ; F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); F[1] = float_at(rt, G[17] + 0x4B8u); F[1] = F[1] + F[0]; { const bool tk = G[2] == 0u; put_float(rt, G[17] + 0x4B8u, F[1]); if (tk) goto L_08990ED4; } G[2] = word(rt, G[17] + 0x4BCu); G[2] = G[20] + G[2]; put(rt, G[17] + 0x4BCu, G[2]); L_08990ED4: ; G[4] = G[17]; G[5] = 0u; G[6] = 0u; G[7] = 0u; G[8] = frame.at(0u); call_here(rt, ctx, 0x0896A35Cu); F[12] = float_at(rt, frame.at(0x0u)); G[4] = G[17]; call_here(rt, ctx, 0x0898DC1Cu); L_08990EF8: ; G[4] = G[19]; call_here(rt, ctx, 0x08983440u); G[4] = G[18]; call_here(rt, ctx, 0x089A8EB0u); F[20] = float_at(rt, frame.at(0x30u)); goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0899AAD0 (a0 = 1 and a1 = 0xFFFF; otherwise nothing is done and v0 is a0's 1 or 0xFFFF): registers a static object. 0x08A92114(0x08CF4634, 0x08CA45F4, 0x130, 0x0899A900,
// 0x0899B490, 0x0899A924) and 0x0897D534(0x08CF4624, 0x08CA45F4, 0x0899B204, 0x0899B1C0) set it up, then six values of the 8-byte table words 0x08C8C3A8 down to 0x08C8C380 are copied to the frame (+0x28 down to +0)
// and each registered with 0x08BF3490(slot, name, &copy, 0x08CF4624): slots 0x08CF46A0 .. 0x08CF4650 (steps of 0x10), names 0x08CA4600, 0x08CA4610, 0x08CA4620, 0x08CA4630, 0x08CA4644, 0x08CA4650.
// v0 = the last call's result.
void func_0899AAD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    ctx.gpr[2] = 0x1u;
    if (first != 1u) return;
    ctx.gpr[2] = 0xFFFFu;
    if (second != 0xFFFFu) return;
    ctx.gpr[17] = 0x004A0000u;
    call_set(rt, ctx, 0x08A92114u, {0x08CF4634u, 0x08CA45F4u, 0x130u, 0x0899A900u, 0x0899B490u, 0x0899A924u});
    ctx.gpr[16] = 0x00020000u;
    call_set(rt, ctx, 0x0897D534u, {0x08CF4624u, 0x08CA45F4u, 0x0899B204u, 0x0899B1C0u});
    struct Entry {
        u32 table, slot, name, offset;
    };
    const Entry entries[6] = {
        {0x08C8C3A8u, 0x08CF46A0u, 0x08CA4600u, 0x28u}, {0x08C8C3A0u, 0x08CF4690u, 0x08CA4610u, 0x20u}, {0x08C8C398u, 0x08CF4680u, 0x08CA4620u, 0x18u},
        {0x08C8C390u, 0x08CF4670u, 0x08CA4630u, 0x10u}, {0x08C8C388u, 0x08CF4660u, 0x08CA4644u, 0x08u}, {0x08C8C380u, 0x08CF4650u, 0x08CA4650u, 0x00u},
    };
    for (const Entry &entry : entries) {
        const u32 low = word(rt, entry.table + 0x0u), high = word(rt, entry.table + 0x4u);
        put(rt, frame.at(entry.offset) + 0x0u, low);
        put(rt, frame.at(entry.offset) + 0x4u, high);
        call_set(rt, ctx, 0x08BF3490u, {entry.slot, entry.name, frame.at(entry.offset), 0x08CF4624u, low, high});
    }
}
void func_089AD2F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[31]); auto &F = ctx.fpr; G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x089A8EA8u); { const bool tk = G[2] == 0u; if (tk) goto L_089AD4A8; } G[4] = word(rt, G[17] + 0x5Cu); G[4] = G[4] + 0x1u; { const bool tk = static_cast<std::int32_t>(G[4]) <= 0; put(rt, G[17] + 0x5Cu, G[4]); if (tk) goto L_089AD414; } G[5] = word(rt, G[17] + 0x58u); G[2] = G[4] << 4; F[3] = float_at(rt, G[16] + 0x0u); G[2] = G[5] + G[2]; F[2] = float_at(rt, G[2] + 0xFFFFFFF0u); G[3] = G[2] + 0xFFFFFFF0u; ctx.set_fpu_condition(F[2] < F[3]); { const bool tk = ctx.fpu_condition(); F[7] = F[3]; if (tk) goto L_089AD358; } F[7] = F[2]; L_089AD358: ; F[4] = float_at(rt, G[16] + 0x4u); F[5] = float_at(rt, G[3] + 0x4u); ctx.set_fpu_condition(F[5] < F[4]); { const bool tk = ctx.fpu_condition(); F[6] = F[4]; if (tk) goto L_089AD374; } F[6] = F[5]; L_089AD374: ; F[0] = float_at(rt, G[3] + 0x8u); F[1] = float_at(rt, G[16] + 0x8u); F[2] = F[2] + F[0]; F[0] = F[3] + F[1]; ctx.set_fpu_condition(F[2] < F[0]); if (!ctx.fpu_condition()) { F[2] = F[0]; goto L_089AD394; } L_089AD394: ; F[0] = float_at(rt, G[3] + 0xCu); F[1] = float_at(rt, G[16] + 0xCu); F[0] = F[5] + F[0]; F[1] = F[4] + F[1]; ctx.set_fpu_condition(F[0] < F[1]); { const bool tk = ctx.fpu_condition(); F[2] = F[2] - F[7]; if (tk) goto L_089AD3B8; } F[0] = F[1]; L_089AD3B8: ; F[1] = std::bit_cast<float>(0u); ctx.set_fpu_condition(F[2] <= F[1]); { const bool tk = ctx.fpu_condition(); F[0] = F[0] - F[6]; if (tk) goto L_089AD3DC; } ctx.set_fpu_condition(F[0] <= F[1]); if (!ctx.fpu_condition()) { put_float(rt, frame.at(0x0u), F[7]); goto L_089AD3E8; } L_089AD3DC: ; F[0] = F[1]; F[2] = F[1]; put_float(rt, frame.at(0x0u), F[7]); L_089AD3E8: ; put_float(rt, frame.at(0x4u), F[6]); put_float(rt, frame.at(0x8u), F[2]); { const bool tk = G[2] == frame.at(0u); put_float(rt, frame.at(0xCu), F[0]); if (tk) goto L_089AD460; } put_float(rt, G[2] + 0xCu, F[0]); put_float(rt, G[2] + 0x0u, F[7]); put_float(rt, G[2] + 0x4u, F[6]); put_float(rt, G[2] + 0x8u, F[2]); G[4] = word(rt, G[17] + 0x5Cu); G[5] = word(rt, G[17] + 0x58u); goto L_089AD460; L_089AD414: ; G[3] = word(rt, G[17] + 0x58u); G[2] = G[4] << 4; G[2] = G[3] + G[2]; if (G[2] == G[16]) { G[3] = word(rt, G[17] + 0x0u); goto L_089AD44C; } F[0] = float_at(rt, G[16] + 0xCu); F[1] = float_at(rt, G[16] + 0x0u); F[2] = float_at(rt, G[16] + 0x4u); F[3] = float_at(rt, G[16] + 0x8u); put_float(rt, G[2] + 0xCu, F[0]); put_float(rt, G[2] + 0x0u, F[1]); put_float(rt, G[2] + 0x4u, F[2]); put_float(rt, G[2] + 0x8u, F[3]); G[3] = word(rt, G[17] + 0x0u); L_089AD44C: ; G[2] = word(rt, G[3] + 0x13Cu); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[4] = word(rt, G[17] + 0x5Cu); G[5] = word(rt, G[17] + 0x58u); L_089AD460: ; G[2] = G[4] << 4; G[5] = G[5] + G[2]; F[0] = float_at(rt, G[5] + 0x8u); F[1] = std::bit_cast<float>(0u); ctx.set_fpu_condition(F[1] < F[0]); { const bool tk = !ctx.fpu_condition(); if (tk) goto L_089AD4A8; } F[0] = float_at(rt, G[5] + 0xCu); ctx.set_fpu_condition(F[1] < F[0]); if (!ctx.fpu_condition()) { goto L_089AD4AC; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x144u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } L_089AD4A8: ; L_089AD4AC: ; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089BA75C (a0 = 1 and a1 = 0xFFFF; otherwise nothing is done and v0 is a0's 1 or 0xFFFF): registers a static object. 0x08A92114(0x08CF4CD0, 0x08CA5C28, 0x64, 0x089BA3F0,
// 0x089BE7E4, 0x089BA414) and 0x0897D534(0x08CF4CC0, 0x08CA5C28, 0x089BCE00, 0x089BCC98) set it up, then six values of the 8-byte table words 0x08C8CA30 down to 0x08C8CA08 are copied to the frame (+0x28 down to +0)
// and each registered with 0x08BFE6E0(slot, name, &copy, 0x08CF4CC0): slots 0x08CF4D3C, 0x08CF4D2C, 0x08CF4D1C, 0x08CF4D0C, 0x08CF4CFC, 0x08CF4CEC, names 0x08CA5C30, 0x08CA5C38, 0x08CA5C44, 0x08CA5C50, 0x08CA5C60, 0x08CA5C70.
// v0 = the last call's result.
void func_089BA75C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    ctx.gpr[2] = 0x1u;
    if (first != 1u) return;
    ctx.gpr[2] = 0xFFFFu;
    if (second != 0xFFFFu) return;
    ctx.gpr[17] = 0x004A0000u;
    call_set(rt, ctx, 0x08A92114u, {0x08CF4CD0u, 0x08CA5C28u, 0x64u, 0x089BA3F0u, 0x089BE7E4u, 0x089BA414u});
    ctx.gpr[16] = 0x00020000u;
    call_set(rt, ctx, 0x0897D534u, {0x08CF4CC0u, 0x08CA5C28u, 0x089BCE00u, 0x089BCC98u});
    struct Entry {
        u32 table, slot, name, offset;
    };
    const Entry entries[6] = {
        {0x08C8CA30u, 0x08CF4D3Cu, 0x08CA5C30u, 0x28u}, {0x08C8CA28u, 0x08CF4D2Cu, 0x08CA5C38u, 0x20u}, {0x08C8CA20u, 0x08CF4D1Cu, 0x08CA5C44u, 0x18u},
        {0x08C8CA18u, 0x08CF4D0Cu, 0x08CA5C50u, 0x10u}, {0x08C8CA10u, 0x08CF4CFCu, 0x08CA5C60u, 0x08u}, {0x08C8CA08u, 0x08CF4CECu, 0x08CA5C70u, 0x00u},
    };
    for (const Entry &entry : entries) {
        const u32 low = word(rt, entry.table + 0x0u), high = word(rt, entry.table + 0x4u);
        put(rt, frame.at(entry.offset) + 0x0u, low);
        put(rt, frame.at(entry.offset) + 0x4u, high);
        call_set(rt, ctx, 0x08BFE6E0u, {entry.slot, entry.name, frame.at(entry.offset), 0x08CF4CC0u, low, high});
    }
}
void func_089CDA4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[23]); G[22] = frame.at(0x4u); G[16] = 0x08CF0000u; G[21] = 0x08CF0000u; G[20] = G[5]; G[5] = 0x08CF6ED8u; G[17] = G[4]; G[4] = G[22]; G[19] = G[6]; G[18] = G[17] + 0x154u; call_here(rt, ctx, 0x08969188u); G[4] = 0x08CF5134u; G[30] = word(rt, G[17] + 0x198u); call_here(rt, ctx, 0x08B344E0u); { const bool tk = G[22] == G[18]; G[2] = 0xFFFFFFFFu; if (tk) goto L_089CDAC4; } G[5] = word(rt, G[17] + 0x154u); G[4] = G[22]; call_here(rt, ctx, 0x08A5BAD8u); G[2] = 0xFFFFFFFFu; L_089CDAC4: ; put(rt, G[17] + 0x198u, G[2]); G[5] = 0x08CF6ED8u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08969188u); { const bool tk = G[18] == frame.at(0u); G[5] = word(rt, frame.at(0x0u)); if (tk) goto L_089CDAE4; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_089CDAE4: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B3457Cu); G[16] = word(rt, frame.at(0x4u)); if (G[16] == 0u) { G[2] = word(rt, G[19] + 0x0u); goto L_089CDB38; } { const bool tk = G[20] == 0u; G[4] = G[16]; if (tk) goto L_089CDB14; } G[5] = G[20]; call_here(rt, ctx, 0x089BA4D8u); G[16] = word(rt, frame.at(0x4u)); L_089CDB14: ; if (G[16] != 0u) { G[4] = G[16]; goto L_089CDB2C; } G[4] = 0x08CA0000u; G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089CDB2C: ; G[5] = 0u; call_here(rt, ctx, 0x089BA6CCu); G[2] = word(rt, G[19] + 0x0u); L_089CDB38: ; { const bool tk = G[2] == 0u; G[23] = frame.at(0x8u); if (tk) goto L_089CDBA0; } G[4] = G[23]; G[5] = G[19]; call_here(rt, ctx, 0x08969178u); G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[4] = G[16]; goto L_089CDB68; } G[4] = 0x08CA0000u; G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089CDB68: ; G[5] = G[17] + 0xE8u; call_here(rt, ctx, 0x089BA6CCu); { const bool tk = G[20] == 0u; G[16] = word(rt, frame.at(0x8u)); if (tk) goto L_089CDB98; } if (G[16] != 0u) { G[4] = G[16]; goto L_089CDB90; } G[4] = 0x08CA0000u; G[4] = 0x08CA6984u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089CDB90: ; G[5] = G[20]; call_here(rt, ctx, 0x089BAA60u); L_089CDB98: ; G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); L_089CDBA0: ; G[4] = 0x08CF5134u; call_here(rt, ctx, 0x08B344E0u); { const bool tk = G[18] == G[19]; G[2] = 0x1u; if (tk) goto L_089CDBC0; } G[5] = word(rt, G[19] + 0x0u); G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); G[2] = 0x1u; L_089CDBC0: ; G[4] = 0x08CF0000u; store8(G[17] + 0x1C4u, G[2]); G[4] = 0x08CF5134u; put(rt, G[17] + 0x198u, G[30]); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[22] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[30] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[23] = word(rt, frame.at(0x2Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_089E5E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[23]); G[22] = frame.at(0x4u); G[16] = 0x08CF0000u; G[21] = 0x08CF0000u; G[20] = G[5]; G[5] = 0x08CF6ED8u; G[17] = G[4]; G[4] = G[22]; G[19] = G[6]; G[18] = G[17] + 0x15Cu; call_here(rt, ctx, 0x08969188u); G[4] = 0x08CF575Cu; G[30] = word(rt, G[17] + 0x19Cu); call_here(rt, ctx, 0x08B344E0u); { const bool tk = G[22] == G[18]; G[2] = 0xFFFFFFFFu; if (tk) goto L_089E5F08; } G[5] = word(rt, G[17] + 0x15Cu); G[4] = G[22]; call_here(rt, ctx, 0x08A5BAD8u); G[2] = 0xFFFFFFFFu; L_089E5F08: ; put(rt, G[17] + 0x19Cu, G[2]); G[5] = 0x08CF6ED8u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08969188u); { const bool tk = G[18] == frame.at(0u); G[5] = word(rt, frame.at(0x0u)); if (tk) goto L_089E5F28; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_089E5F28: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = 0x08CF575Cu; call_here(rt, ctx, 0x08B3457Cu); G[16] = word(rt, frame.at(0x4u)); if (G[16] == 0u) { G[2] = word(rt, G[19] + 0x0u); goto L_089E5F7C; } { const bool tk = G[20] == 0u; G[4] = G[16]; if (tk) goto L_089E5F58; } G[5] = G[20]; call_here(rt, ctx, 0x089BA4D8u); G[16] = word(rt, frame.at(0x4u)); L_089E5F58: ; if (G[16] != 0u) { G[4] = G[16]; goto L_089E5F70; } G[4] = 0x08CA0000u; G[4] = 0x08CA7A08u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089E5F70: ; G[5] = 0u; call_here(rt, ctx, 0x089BA6CCu); G[2] = word(rt, G[19] + 0x0u); L_089E5F7C: ; { const bool tk = G[2] == 0u; G[23] = frame.at(0x8u); if (tk) goto L_089E5FE4; } G[4] = G[23]; G[5] = G[19]; call_here(rt, ctx, 0x08969178u); G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[4] = G[16]; goto L_089E5FAC; } G[4] = 0x08CA0000u; G[4] = 0x08CA7A08u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089E5FAC: ; G[5] = G[17] + 0xE8u; call_here(rt, ctx, 0x089BA6CCu); { const bool tk = G[20] == 0u; G[16] = word(rt, frame.at(0x8u)); if (tk) goto L_089E5FDC; } if (G[16] != 0u) { G[4] = G[16]; goto L_089E5FD4; } G[4] = 0x08CA0000u; G[4] = 0x08CA7A08u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089E5FD4: ; G[5] = G[20]; call_here(rt, ctx, 0x089BAA60u); L_089E5FDC: ; G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); L_089E5FE4: ; G[4] = 0x08CF575Cu; call_here(rt, ctx, 0x08B344E0u); { const bool tk = G[18] == G[19]; G[2] = 0x1u; if (tk) goto L_089E6004; } G[5] = word(rt, G[19] + 0x0u); G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); G[2] = 0x1u; L_089E6004: ; G[4] = 0x08CF0000u; store8(G[17] + 0x1DCu, G[2]); G[4] = 0x08CF575Cu; put(rt, G[17] + 0x19Cu, G[30]); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[22] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[30] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[23] = word(rt, frame.at(0x2Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08A819B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[16] = word(rt, G[5] + 0x0u); { const bool tk = G[16] != 0u; G[17] = G[4]; if (tk) goto L_08A819F8; } G[4] = 0x08CB0000u; G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A819F8: ; G[2] = word(rt, G[16] + 0x0u); G[21] = frame.at(0x8u); G[5] = G[16]; G[3] = word(rt, G[2] + 0xA8u); G[4] = G[21]; { const std::uint32_t tg = G[3]; G[23] = 0x08CB0000u; call_here(rt, ctx, tg); } G[22] = frame.at(0xCu); G[30] = 0x08CB0000u; G[19] = frame.at(0x4u); G[20] = frame.at(0x10u); goto L_08A81AE4; L_08A81A28: ; G[4] = 0x08CB1604u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[16] + 0x14u); L_08A81A34: ; G[4] = G[22]; G[5] = G[5] + 0x4u; call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(0xCu)); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A81A58; } G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A81A58: ; G[5] = G[16]; G[4] = G[19]; G[2] = word(rt, G[3] + 0x84u); { const std::uint32_t tg = G[2]; G[6] = 0u; call_here(rt, ctx, tg); } G[4] = G[20]; G[5] = G[17]; G[6] = G[19]; call_here(rt, ctx, 0x08A818A0u); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x10u)); { const bool tk = G[2] == 0u; G[16] = word(rt, frame.at(0xCu)); if (tk) goto L_08A81AD4; } G[2] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[18] = word(rt, G[2] + 0xA0u); if (tk) goto L_08A81AA4; } G[4] = 0x08CB15A0u; call_here(rt, ctx, 0x08A5BB7Cu); L_08A81AA4: ; G[3] = word(rt, G[16] + 0x0u); G[5] = G[16]; G[4] = frame.at(0u); G[2] = word(rt, G[3] + 0x84u); { const std::uint32_t tg = G[2]; G[6] = 0x1u; call_here(rt, ctx, tg); } G[4] = G[17]; G[5] = G[20]; { const std::uint32_t tg = G[18]; G[6] = frame.at(0u); call_here(rt, ctx, tg); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); L_08A81AD4: ; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); L_08A81AE4: ; G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[4] = word(rt, G[16] + 0x14u); goto L_08A81AFC; } G[4] = 0x08CB1604u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = word(rt, G[16] + 0x14u); L_08A81AFC: ; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; if (tk) goto L_08A81B3C; } G[4] = word(rt, G[16] + 0x14u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x8u)); if (G[16] != 0u) { G[5] = word(rt, G[16] + 0x14u); goto L_08A81A34; } goto L_08A81A28; L_08A81B3C: ; G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x24u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C2B298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[21] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[16] + 0x8u); { const bool tk = G[4] == G[2]; G[18] = G[6]; if (tk) goto L_08C2B364; } { const bool tk = G[4] == 0u; G[5] = G[4] + 0xFFFFFFFCu; if (tk) goto L_08C2B2E4; } call_here(rt, ctx, 0x08AAE3A8u); G[4] = word(rt, G[16] + 0x4u); L_08C2B2E4: ; G[4] = G[4] + 0x4u; G[20] = frame.at(0x4u); put(rt, G[16] + 0x4u, G[4]); G[5] = G[18]; G[4] = G[20]; call_here(rt, ctx, 0x08AAE3A8u); G[3] = word(rt, G[16] + 0x4u); G[17] = G[3] + 0xFFFFFFF8u; G[2] = G[17] - G[21]; G[19] = sra(G[2], 2); { const bool tk = static_cast<std::int32_t>(G[19]) <= 0; G[4] = G[3] + 0xFFFFFFFCu; if (tk) goto L_08C2B344; } G[16] = G[4]; G[18] = 0u; G[16] = G[16] + 0xFFFFFFFCu; L_08C2B320: ; G[17] = G[17] + 0xFFFFFFFCu; if (G[16] == G[17]) { G[18] = G[18] + 0x1u; goto L_08C2B33C; } G[5] = word(rt, G[17] + 0x0u); G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); G[18] = G[18] + 0x1u; L_08C2B33C: ; { const bool tk = G[19] != G[18]; G[16] = G[16] + 0xFFFFFFFCu; if (tk) goto L_08C2B320; } L_08C2B344: ; { const bool tk = G[21] == G[20]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_08C2B354; } G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); L_08C2B354: ; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); goto L_08C2B438; L_08C2B364: ; G[2] = word(rt, G[16] + 0x0u); G[3] = 0x3FFF0000u; G[3] = G[3] | 0xFFFFu; G[2] = G[4] - G[2]; G[2] = sra(G[2], 2); { const bool tk = G[2] != G[3]; G[4] = 0x08CB0000u; if (tk) goto L_08C2B388; } G[4] = 0x08CB1CA0u; call_here(rt, ctx, 0x08B5ADDCu); L_08C2B388: ; { const bool tk = G[2] == 0u; G[3] = 0x1u; if (tk) goto L_08C2B3A0; } G[3] = G[2] << 1; G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFCu; if (tk) goto L_08C2B3A4; } L_08C2B3A0: ; G[20] = G[3] << 2; L_08C2B3A4: ; G[4] = G[20]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = word(rt, G[16] + 0x0u); G[5] = G[21]; G[6] = G[2]; G[7] = frame.at(0x2u); G[19] = G[2]; call_here(rt, ctx, 0x08C2B234u); { const bool tk = G[2] == 0u; G[17] = G[2]; if (tk) goto L_08C2B3DC; } G[5] = G[18]; G[4] = G[2]; call_here(rt, ctx, 0x08AAE3A8u); L_08C2B3DC: ; G[5] = word(rt, G[16] + 0x4u); G[6] = G[17] + 0x4u; G[4] = G[21]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C2B234u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C2AD88u); G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[16] + 0x8u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (tk) goto L_08C2B424; } G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C2B424: ; G[2] = G[19] + G[20]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C2B438: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C3548C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[21] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[16] + 0x8u); { const bool tk = G[4] == G[2]; G[18] = G[6]; if (tk) goto L_08C35558; } { const bool tk = G[4] == 0u; G[5] = G[4] + 0xFFFFFFFCu; if (tk) goto L_08C354D8; } call_here(rt, ctx, 0x08AAE214u); G[4] = word(rt, G[16] + 0x4u); L_08C354D8: ; G[4] = G[4] + 0x4u; G[20] = frame.at(0x4u); put(rt, G[16] + 0x4u, G[4]); G[5] = G[18]; G[4] = G[20]; call_here(rt, ctx, 0x08AAE214u); G[3] = word(rt, G[16] + 0x4u); G[17] = G[3] + 0xFFFFFFF8u; G[2] = G[17] - G[21]; G[19] = sra(G[2], 2); { const bool tk = static_cast<std::int32_t>(G[19]) <= 0; G[4] = G[3] + 0xFFFFFFFCu; if (tk) goto L_08C35538; } G[16] = G[4]; G[18] = 0u; G[16] = G[16] + 0xFFFFFFFCu; L_08C35514: ; G[17] = G[17] + 0xFFFFFFFCu; if (G[16] == G[17]) { G[18] = G[18] + 0x1u; goto L_08C35530; } G[5] = word(rt, G[17] + 0x0u); G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); G[18] = G[18] + 0x1u; L_08C35530: ; { const bool tk = G[19] != G[18]; G[16] = G[16] + 0xFFFFFFFCu; if (tk) goto L_08C35514; } L_08C35538: ; { const bool tk = G[21] == G[20]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_08C35548; } G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); L_08C35548: ; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); goto L_08C3562C; L_08C35558: ; G[2] = word(rt, G[16] + 0x0u); G[3] = 0x3FFF0000u; G[3] = G[3] | 0xFFFFu; G[2] = G[4] - G[2]; G[2] = sra(G[2], 2); { const bool tk = G[2] != G[3]; G[4] = 0x08CB0000u; if (tk) goto L_08C3557C; } G[4] = 0x08CB398Cu; call_here(rt, ctx, 0x08B5ADDCu); L_08C3557C: ; { const bool tk = G[2] == 0u; G[3] = 0x1u; if (tk) goto L_08C35594; } G[3] = G[2] << 1; G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFCu; if (tk) goto L_08C35598; } L_08C35594: ; G[20] = G[3] << 2; L_08C35598: ; G[4] = G[20]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = word(rt, G[16] + 0x0u); G[5] = G[21]; G[6] = G[2]; G[7] = frame.at(0x2u); G[19] = G[2]; call_here(rt, ctx, 0x08C35428u); { const bool tk = G[2] == 0u; G[17] = G[2]; if (tk) goto L_08C355D0; } G[5] = G[18]; G[4] = G[2]; call_here(rt, ctx, 0x08AAE214u); L_08C355D0: ; G[5] = word(rt, G[16] + 0x4u); G[6] = G[17] + 0x4u; G[4] = G[21]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C35428u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C35154u); G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[16] + 0x8u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (tk) goto L_08C35618; } G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C35618: ; G[2] = G[19] + G[20]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C3562C: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C3BBB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[21] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[16] + 0x8u); { const bool tk = G[4] == G[2]; G[18] = G[6]; if (tk) goto L_08C3BC84; } { const bool tk = G[4] == 0u; G[5] = G[4] + 0xFFFFFFFCu; if (tk) goto L_08C3BC04; } call_here(rt, ctx, 0x08AB6F6Cu); G[4] = word(rt, G[16] + 0x4u); L_08C3BC04: ; G[4] = G[4] + 0x4u; G[20] = frame.at(0x4u); put(rt, G[16] + 0x4u, G[4]); G[5] = G[18]; G[4] = G[20]; call_here(rt, ctx, 0x08AB6F6Cu); G[3] = word(rt, G[16] + 0x4u); G[17] = G[3] + 0xFFFFFFF8u; G[2] = G[17] - G[21]; G[19] = sra(G[2], 2); { const bool tk = static_cast<std::int32_t>(G[19]) <= 0; G[4] = G[3] + 0xFFFFFFFCu; if (tk) goto L_08C3BC64; } G[16] = G[4]; G[18] = 0u; G[16] = G[16] + 0xFFFFFFFCu; L_08C3BC40: ; G[17] = G[17] + 0xFFFFFFFCu; if (G[16] == G[17]) { G[18] = G[18] + 0x1u; goto L_08C3BC5C; } G[5] = word(rt, G[17] + 0x0u); G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); G[18] = G[18] + 0x1u; L_08C3BC5C: ; { const bool tk = G[19] != G[18]; G[16] = G[16] + 0xFFFFFFFCu; if (tk) goto L_08C3BC40; } L_08C3BC64: ; { const bool tk = G[21] == G[20]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_08C3BC74; } G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); L_08C3BC74: ; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); goto L_08C3BD58; L_08C3BC84: ; G[2] = word(rt, G[16] + 0x0u); G[3] = 0x3FFF0000u; G[3] = G[3] | 0xFFFFu; G[2] = G[4] - G[2]; G[2] = sra(G[2], 2); { const bool tk = G[2] != G[3]; G[4] = 0x08CB0000u; if (tk) goto L_08C3BCA8; } G[4] = 0x08CB425Cu; call_here(rt, ctx, 0x08B5ADDCu); L_08C3BCA8: ; { const bool tk = G[2] == 0u; G[3] = 0x1u; if (tk) goto L_08C3BCC0; } G[3] = G[2] << 1; G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFCu; if (tk) goto L_08C3BCC4; } L_08C3BCC0: ; G[20] = G[3] << 2; L_08C3BCC4: ; G[4] = G[20]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = word(rt, G[16] + 0x0u); G[5] = G[21]; G[6] = G[2]; G[7] = frame.at(0x2u); G[19] = G[2]; call_here(rt, ctx, 0x08C3BB54u); { const bool tk = G[2] == 0u; G[17] = G[2]; if (tk) goto L_08C3BCFC; } G[5] = G[18]; G[4] = G[2]; call_here(rt, ctx, 0x08AB6F6Cu); L_08C3BCFC: ; G[5] = word(rt, G[16] + 0x4u); G[6] = G[17] + 0x4u; G[4] = G[21]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C3BB54u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C3BB10u); G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[16] + 0x8u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (tk) goto L_08C3BD44; } G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C3BD44: ; G[2] = G[19] + G[20]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C3BD58: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08874338 (a0 = the object): constructor of an object holding two shared pointers (counted references at +0xC / +4 and +0x14 / +0x18) to two new sub-objects: the first +0x28 bytes (0x08B62344(0x28)), the second
// +0x68. Each sub-object gets a vtable (the final ones 0x08CBFC10 and 0x08CBFAF0; they pass through 0x08CBFC50 and 0x08CBFB30 first), two words filled with the empty-string header 0x08D752A4 at +0xC, four
// zero words at +0x14, zero bytes at +0x24 / +0x25 and bit 16 of +0x24 cleared. A 0x10-byte control block (vtable 0x08CBFC90 / 0x08CBFBB0, counts 1 and 1, the pointer) is stored beside the pointer and
// 0x08B8E380(control slot, pointer, pointer) records it: a call for the first one, the tail call for the second.
void func_08874338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 tail = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {20u, 0x10u}, {19u, 0xCu}, {31u, 0x14u}, {17u, 0x4u}, {16u, 0x0u}});
        ctx.gpr[18] = self;
        ctx.gpr[19] = self + 0xCu;
        put(rt, self + 0x0u, 0u);
        put(rt, self + 0x4u, 0u);
        put(rt, self + 0x8u, 0u);
        const u32 empty = 0x08D75298u + 0xCu;
        ctx.gpr[20] = empty;
        // one sub-object of `size` bytes: made, filled and stored into the pointer slot; its control block follows
        auto build = [&](u32 size, u32 first_vtable, u32 final_vtable, u32 control_vtable, u32 slot) {
            const u32 object = call_set(rt, ctx, 0x08B62344u, {size});
            ctx.gpr[17] = object;
            put(rt, object + 0x0u, first_vtable);
            put(rt, object + 0x4u, 0u);
            put(rt, object + 0x8u, 0u);
            put(rt, object + 0xCu, empty);
            put(rt, object + 0x10u, empty);
            for (u32 offset = 0x14u; offset < 0x24u; offset += 4u) put(rt, object + offset, 0u);
            store8(object + 0x24u, 0u);
            store8(object + 0x25u, 0u);
            const u32 flags = word(rt, object + 0x24u);
            put(rt, object + 0x0u, final_vtable);
            put(rt, object + 0x24u, flags & ~0x10000u);
            ctx.gpr[16] = slot + 0x4u;
            put(rt, slot + 0x0u, object);
            const u32 control = call_set(rt, ctx, 0x08B62344u, {0x10u});
            put(rt, control + 0x0u, control_vtable);
            put(rt, control + 0x4u, 1u);
            put(rt, control + 0x8u, 1u);
            put(rt, control + 0xCu, object);
            put(rt, slot + 0x4u, control);
            ctx.gpr[7] = 1u;
            ctx.gpr[2] = control;
            return object;
        };
        const u32 first = build(0x28u, 0x08CBFC50u, 0x08CBFC10u, 0x08CBFC90u, self + 0xCu);
        call_set(rt, ctx, 0x08B8E380u, {self + 0x10u, first, first});
        ctx.gpr[18] = self + 0x14u;
        const u32 second = build(0x68u, 0x08CBFB30u, 0x08CBFAF0u, 0x08CBFBB0u, self + 0x14u);
        ctx.gpr[4] = self + 0x18u;
        ctx.gpr[5] = second;
        ctx.gpr[6] = second;
        tail = 0x08B8E380u;
    }
    jump(ctx, tail);
}
// 0x08880468 (a0 = the owner, a1 = which listener slot (0 or 1; anything else does nothing), a2 = a new listener or 0): replaces a listener kept in the owner's block (+0xC): slot 0 is at +4 of
// the block, slot 1 at +8. The current listener's virtual +0x2C is called and the slot cleared; a new one (virtual +0x18 on it gives the handle stored in the slot) is then given
// its kind through 0x0887BD94(7 or 0xF) and its own virtual +8, told its owner by its virtual +0x14(owner) and named by its virtual +0x18(0x08BA4D20 or 0x08BA4D28). The last
// step is a tail call of the listener's virtual +0x28 (a0 = the listener).
void func_08880468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4], which = ctx.gpr[5], listener = ctx.gpr[6];
    u32 tail = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {17u, 0x4u}, {31u, 0xCu}, {16u, 0x0u}});
        ctx.gpr[18] = owner;
        ctx.gpr[17] = listener;
        if (which != 0u && which != 1u) {
            ctx.gpr[2] = 1u;
            return;
        }
        const u32 slot = which == 0u ? 0x4u : 0x8u, kind = which == 0u ? 0x7u : 0xFu, name = which == 0u ? 0x08BA4D20u : 0x08BA4D28u;
        // the listener now in the slot
        auto current = [&] { return word(rt, word(rt, owner + 0xCu) + slot); };
        u32 old = current();
        ctx.gpr[4] = old;
        if (old != 0u) {
            call_set(rt, ctx, word(rt, word(rt, old) + 0x2Cu), {old});
            put(rt, word(rt, owner + 0xCu) + slot, 0u);
        }
        if (listener == 0u) return;
        const u32 block = word(rt, owner + 0xCu);
        ctx.gpr[16] = block;
        const u32 handle = call_set(rt, ctx, word(rt, word(rt, listener) + 0x18u), {listener});
        put(rt, block + slot, handle);
        const u32 held = current();
        ctx.gpr[17] = held;
        if (held == 0u) return;
        const u32 set_kind = word(rt, word(rt, held) + 0x8u);
        ctx.gpr[16] = set_kind;
        const u32 kind_value = call_set(rt, ctx, 0x0887BD94u, {kind});
        call_set(rt, ctx, set_kind, {held, kind_value});
        u32 now = current();
        call_set(rt, ctx, word(rt, word(rt, now) + 0x14u), {now, owner});
        now = current();
        call_set(rt, ctx, word(rt, word(rt, now) + 0x18u), {now, name});
        now = current();
        ctx.gpr[4] = now;
        ctx.gpr[25] = word(rt, word(rt, now) + 0x28u);
        tail = ctx.gpr[25];
    }
    jump(ctx, tail);
}
void func_08880BAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; G[4] = word(rt, G[4] + 0x4u); if (G[4] == 0u) { G[4] = word(rt, G[16] + 0x8u); goto L_08880BE4; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x2Cu); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } put(rt, G[16] + 0x4u, 0u); G[4] = word(rt, G[16] + 0x8u); L_08880BE4: ; { const bool tk = G[4] == 0u; if (tk) goto L_08880C00; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x2Cu); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } put(rt, G[16] + 0x8u, 0u); L_08880C00: ; G[4] = G[16]; call_here(rt, ctx, 0x08880874u); G[17] = word(rt, G[16] + 0x34u); { const bool tk = G[17] == 0u; G[3] = G[17] + 0x4u; if (tk) goto L_08880C70; } L_08880C14: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08880C14; } if (G[4] != G[2]) { G[17] = word(rt, G[16] + 0x18u); goto L_08880C74; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[3] = G[17] + 0x8u; L_08880C44: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08880C44; } if (G[4] != G[2]) { G[17] = word(rt, G[16] + 0x18u); goto L_08880C74; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } L_08880C70: ; G[17] = word(rt, G[16] + 0x18u); L_08880C74: ; G[2] = 0x08CC0000u; G[2] = 0x08CBF368u; put(rt, G[16] + 0x20u, G[2]); { const bool tk = G[17] == 0u; G[16] = G[16] + 0xCu; if (tk) goto L_08880CE8; } G[3] = G[17] + 0x4u; L_08880C8C: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08880C8C; } if (G[4] != G[2]) { G[16] = word(rt, G[16] + 0x4u); goto L_08880CEC; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[3] = G[17] + 0x8u; L_08880CBC: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08880CBC; } if (G[4] != G[2]) { G[16] = word(rt, G[16] + 0x4u); goto L_08880CEC; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } L_08880CE8: ; G[16] = word(rt, G[16] + 0x4u); L_08880CEC: ; { const bool tk = G[16] == 0u; G[3] = G[16] + 0x4u; if (tk) goto L_08880D5C; } L_08880CF4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08880CF4; } { const bool tk = G[4] != G[2]; if (tk) goto L_08880D60; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_08880D24: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08880D24; } { const bool tk = G[4] != G[2]; if (tk) goto L_08880D60; } G[2] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; goto exit_; L_08880D5C: ; L_08880D60: ; goto exit_;  exit_: ; G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089012D4 (a0 = the tree, a1 = a node reference {+0 index, +4 sign}, a2 = a callback, a3 = its argument, f12 / f13 / f14 / f15 = a query area: left, top, right, bottom): walks the entries of a node (the
// node table at +0x88, 20 bytes each; its word at +0x10 holds the first entry index in its low 24 bits and the count above them) and calls the callback (a2)(node, a3) for each entry
// (index list at +0xA8 of halfwords, nodes at +0xA0, 16 bytes each) that is not clearly outside the area: a point's side bits are 1 = left of the area (x < f12), 2 = right (f14 < x), 4 = above (z < f13),
// 8 = below (f15 < z), and the entry's two points (the node's x, z at +0 / +8 and the one it links to by the halfword at +0xC, which is relative when negative) must not share a side bit.
// Nothing is done for a null callback or a negative sign.
void func_089012D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 tree = ctx.gpr[4], reference = ctx.gpr[5], callback = ctx.gpr[6], argument = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x14u}, {20u, 0x10u}, {17u, 0x4u}, {55u, 0x2Cu}, {54u, 0x28u}, {53u, 0x24u}, {52u, 0x20u}, {31u, 0x18u}, {19u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}});
    auto &F = ctx.fpr;
    ctx.gpr[21] = argument;
    ctx.gpr[20] = callback;
    ctx.gpr[17] = tree;
    F[23] = F[12];
    F[22] = F[13];
    F[21] = F[14];
    F[20] = F[15];
    if (callback == 0u) return;
    if (static_cast<s32>(word(rt, reference + 0x4u)) < 0) return;
    const u32 index = word(rt, reference + 0x0u);
    const u32 entry = index * 20u + word(rt, tree + 0x88u);
    ctx.gpr[2] = entry;
    u32 packed = word(rt, entry + 0x10u);
    ctx.gpr[4] = packed >> 24u;
    packed &= 0x00FFFFFFu;
    ctx.gpr[3] = packed;
    const u32 end = packed + (ctx.gpr[4]);
    ctx.gpr[4] = end;
    if (!(static_cast<s32>(packed) < static_cast<s32>(end))) return;
    const u32 count = end - packed;
    ctx.gpr[19] = count;
    u32 offset = packed << 1u;
    ctx.gpr[16] = offset;
    // the side bits of a point (x, z) against the area
    auto sides = [&](float x, float z) {
        u32 bits = x < F[23] ? 1u : 0u;
        if (F[21] < x) bits |= 2u;
        if (z < F[22]) bits |= 4u;
        if (F[20] < z) bits |= 8u;
        return bits;
    };
    for (u32 done = 0u; done != count;) {
        ctx.gpr[18] = done;
        const u32 list = word(rt, tree + 0xA8u) + offset;
        ctx.gpr[2] = list;
        const u32 nodes = word(rt, tree + 0xA0u);
        const u32 node = nodes + (signed_half(rt, list) << 4u);
        ctx.gpr[4] = node;
        F[0] = float_at(rt, node + 0x0u);
        F[1] = float_at(rt, node + 0x8u);
        const u32 here = sides(F[0], F[1]);
        ctx.gpr[5] = here;
        bool inside = here == 0u;
        if (!inside) {
            const u32 link = signed_half(rt, node + 0xCu);
            u32 other = node + 0x10u;
            if (static_cast<s32>(link) < 0) other += link << 4u;
            ctx.gpr[3] = other;
            F[0] = float_at(rt, other + 0x0u);
            F[1] = float_at(rt, other + 0x8u);
            const u32 there = sides(F[0], F[1]);
            ctx.gpr[2] = there & here;
            inside = (there & here) == 0u;
        }
        if (inside) call_set(rt, ctx, callback, {node, argument});
        ++done;
        offset += 2u;
        ctx.gpr[16] = offset;
    }
}
// 0x08909184 (a0 = a polygon {+0 total plane count, +2 first-group count, +4 halfword plane indices}, a1 = a direction (3 floats), a2 = the plane table, 16 bytes each: a, b, c, d; leaf): the value
// n . direction + d of each plane in turn (a x dir.x + b x dir.y + c x dir.z + d). The first group's values are minimised (f7 starts at the largest float 0x7F7FFFFF); as soon as any plane's value
// is below 0 that value is returned (f0). When all pass the minimum of the first group is returned (f0 = f7), the second group (the planes after the first group) being tested only for a value below 0.
void func_08909184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 polygon = ctx.gpr[4], direction = ctx.gpr[5], planes = ctx.gpr[6];
    auto &F = ctx.fpr;
    u32 first_group = load16(polygon + 0x2u);
    ctx.gpr[8] = first_group;
    F[7] = std::bit_cast<float>(0x7F7FFFFFu);
    u32 done = 0u;
    ctx.gpr[7] = 0u;
    u32 cursor = polygon;
    // the plane value; `first_style` is the register use of the two entry blocks (they keep a x ... in different registers)
    auto evaluate = [&](u32 index, bool first_style) {
        const u32 plane = planes + (index << 4u);
        ctx.gpr[2] = plane;
        if (first_style) {
            F[6] = float_at(rt, direction + 0x0u);
            F[5] = float_at(rt, direction + 0x4u);
            F[0] = float_at(rt, plane + 0x0u);
            F[2] = float_at(rt, plane + 0x4u);
            F[4] = float_at(rt, direction + 0x8u);
            F[0] = psp_mul(F[0], F[6]);
            F[2] = psp_mul(F[2], F[5]);
            F[1] = float_at(rt, plane + 0x8u);
            F[3] = float_at(rt, plane + 0xCu);
            F[1] = psp_mul(F[1], F[4]);
            F[0] = F[0] + F[2];
            F[0] = F[0] + F[1];
        } else {
            F[0] = float_at(rt, plane + 0x4u);
            F[1] = float_at(rt, plane + 0x0u);
            F[2] = float_at(rt, plane + 0x8u);
            F[0] = psp_mul(F[0], F[5]);
            F[1] = psp_mul(F[1], F[6]);
            F[2] = psp_mul(F[2], F[4]);
            F[3] = float_at(rt, plane + 0xCu);
            F[0] = F[0] + F[1];
            F[0] = F[0] + F[2];
        }
        F[1] = std::bit_cast<float>(0u);
        F[0] = F[0] + F[3];
        return F[0] < F[1];
    };
    if (static_cast<s32>(first_group) > 0) {
        ctx.gpr[2] = load16(polygon + 0x4u);
        if (evaluate(ctx.gpr[2], true)) return;
        cursor = polygon;
        ctx.gpr[3] = cursor;
        F[7] = std::bit_cast<float>(0x7F7FFFFFu);
        for (;;) {
            if (F[0] < F[7]) F[7] = F[0];
            ++done;
            ctx.gpr[7] = done;
            if (first_group == done) break;
            if (evaluate(load16(cursor + 0x6u), false)) return;
            cursor += 2u;
            ctx.gpr[3] = cursor;
        }
    }
    const u32 total = load16(polygon + 0x0u);
    ctx.gpr[8] = total;
    ctx.gpr[2] = static_cast<s32>(done) < static_cast<s32>(total) ? 1u : 0u;
    if (!(static_cast<s32>(done) < static_cast<s32>(total))) {
        F[0] = F[7];
        return;
    }
    cursor = (done << 1u) + polygon;
    ctx.gpr[3] = cursor;
    if (evaluate(load16(cursor + 0x4u), true)) return;
    cursor += 6u;
    const u32 remaining = total - done;
    ctx.gpr[4] = remaining;
    u32 counted = done;
    for (;;) {
        ++counted;
        ctx.gpr[5] = counted;
        ctx.gpr[2] = remaining + done;
        if (counted == remaining + done) {
            F[0] = F[7];
            return;
        }
        if (evaluate(load16(cursor + 0x0u), false)) return;
        cursor += 2u;
    }
}
// 0x08949730 (a0 = n, a1 = a halfword array in[0..n], a2 = output A, a3 = output B; leaf): smooths and bounds a polyline of n + 1 halfword samples. The ends are scaled (x / 16, toward zero): B[1] = 19 x in[1] / 16 and
// A[n] = 13 x in[n] / 16, A[0] = 13 x in[0] / 16, B[0] = 19 x in[0] / 16. For n of 2 or more the inner values are blended with the multiply by 0x66666667 (division by 5 toward zero):
// A[k] = (3 x in[k] + 2 x in[k+1]) / 5 and B[k+1] = (2 x in[k] + 3 x in[k+1]) / 5, k = 1 .. n - 1. Then (for n above 0) A[k] is lowered to in[k] and B[k] raised to in[k] where they do not bound it, k = 1 .. n.
void func_08949730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 n = ctx.gpr[4], in = ctx.gpr[5], a_out = ctx.gpr[6], b_out = ctx.gpr[7];
    ctx.gpr[14] = n;
    ctx.gpr[15] = in;
    // x scaled by `factor` / 16, toward zero
    auto scaled = [&](u32 value, u32 factor) {
        const u32 product = value * factor;
        ctx.gpr[2] = static_cast<s32>(product) < 0 ? 1u : 0u;
        return sra(static_cast<s32>(product) < 0 ? product + 0xFu : product, 4u);
    };
    auto half = [&](u32 address) { return signed_half(rt, address); };
    store16(b_out + 0x2u, scaled(half(in + 0x2u), 19u));
    if (!(static_cast<s32>(n) < 2)) {
        ctx.gpr[11] = 0x66666667u;
        // a value divided by 5 toward zero (the multiply leaves its 64-bit product in lo / hi)
        auto fifth = [&](u32 value) {
            const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(value)) * static_cast<s32>(0x66666667u);
            ctx.lo = static_cast<u32>(product);
            ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
            return sra(ctx.hi, 1u) - sra(value, 31u);
        };
        for (u32 k = 0u; k < n - 1u; ++k) {
            const u32 first = half(in + 2u * k + 0x2u), second = half(in + 2u * k + 0x4u);
            store16(a_out + 2u * k + 0x2u, fifth(first * 3u + second * 2u));
            store16(b_out + 2u * k + 0x4u, fifth(first * 2u + second * 3u));
        }
    }
    store16(a_out + (n << 1u), scaled(half(in + (n << 1u)), 13u));
    if (static_cast<s32>(n) > 0) {
        for (u32 k = 0u; k < n; ++k) {
            const u32 sample = half(in + 2u * k + 0x2u);
            if (static_cast<s32>(sample) < static_cast<s32>(half(a_out + 2u * k + 0x2u))) store16(a_out + 2u * k + 0x2u, sample);
            if (static_cast<s32>(half(b_out + 2u * k + 0x2u)) < static_cast<s32>(sample)) store16(b_out + 2u * k + 0x2u, sample);
        }
    }
    const u32 first_sample = half(in + 0x0u);
    store16(a_out + 0x0u, scaled(first_sample, 13u));
    ctx.gpr[4] = first_sample;
    const u32 last = scaled(first_sample, 19u);
    ctx.gpr[3] = last;
    store16(b_out + 0x0u, last);
}
void func_08A386E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); auto &F = ctx.fpr; G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[21] = G[5]; G[19] = G[4]; G[17] = G[7]; put_float(rt, frame.at(0x44u), F[21]); { const bool tk = G[6] != 0u; put_float(rt, frame.at(0x40u), F[20]); if (tk) goto L_08A3881C; } G[18] = frame.at(0x14u); G[4] = G[18]; G[5] = G[7]; call_here(rt, ctx, 0x08A87CD8u); G[16] = word(rt, frame.at(0x14u)); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A38750; } G[4] = 0x08CB0000u; G[4] = 0x08CAD2A4u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A38750: ; G[4] = G[16]; G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[20] = frame.at(0x10u); call_here(rt, ctx, tg); } G[4] = G[18]; F[21] = F[0]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[17] + 0x4u; G[4] = G[20]; call_here(rt, ctx, 0x08A87CD8u); G[16] = word(rt, frame.at(0x10u)); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A38794; } G[4] = 0x08CB0000u; G[4] = 0x08CAD2A4u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A38794: ; G[4] = G[16]; G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[17] = frame.at(0x8u); call_here(rt, ctx, tg); } G[4] = G[20]; F[20] = F[0]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[21]; G[4] = G[17]; call_here(rt, ctx, 0x08A38038u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; F[12] = F[21]; if (tk) goto L_08A387D8; } G[4] = 0x08CB0000u; G[4] = 0x08CAD29Cu; call_here(rt, ctx, 0x08A5BB7Cu); F[12] = F[21]; L_08A387D8: ; F[13] = F[20]; G[4] = G[16] + 0x14u; G[16] = frame.at(0xCu); call_here(rt, ctx, 0x08B1EFBCu); F[12] = F[0]; G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0xCu)); if (tk) goto L_08A38804; } G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); L_08A38804: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); goto L_08A38884; L_08A3881C: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A38038u); G[16] = word(rt, frame.at(0x0u)); { const bool tk = G[16] != 0u; F[12] = std::bit_cast<float>(0u); if (tk) goto L_08A38840; } G[4] = 0x08CB0000u; G[4] = 0x08CAD29Cu; call_here(rt, ctx, 0x08A5BB7Cu); F[12] = std::bit_cast<float>(0u); L_08A38840: ; G[1] = 0x3F800000u; F[13] = std::bit_cast<float>(G[1]); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08B1EFBCu); F[12] = F[0]; G[16] = frame.at(0x4u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_08A38870; } G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); L_08A38870: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); L_08A38884: ; F[21] = float_at(rt, frame.at(0x44u)); F[20] = float_at(rt, frame.at(0x40u)); goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x34u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_332
} // namespace gt::game

#define HOT332(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_332::name, gt::ReturnKind::kind)
HOT332(0x0888FC28u, func_0888FC28, Word);
HOT332(0x088A76F8u, func_088A76F8, Word);
HOT332(0x088AD4C4u, func_088AD4C4, Word);
// HOT332(0x088EC61Cu, func_088EC61C, Word);  // no comparable synthetic case yet
HOT332(0x08ACA144u, func_08ACA144, Word);
HOT332(0x08B7EF8Cu, func_08B7EF8C, Word);
// HOT332(0x08BAFD74u, func_08BAFD74, Word);  // no comparable synthetic case yet
HOT332(0x0898AB30u, func_0898AB30, Word);
// HOT332(0x08990D6Cu, func_08990D6C, Word);  // no comparable synthetic case yet
HOT332(0x0899AAD0u, func_0899AAD0, Word);
HOT332(0x089AD2F8u, func_089AD2F8, Word);
HOT332(0x089BA75Cu, func_089BA75C, Word);
HOT332(0x089CDA4Cu, func_089CDA4C, Word);
HOT332(0x089E5E90u, func_089E5E90, Word);
// HOT332(0x08A819B4u, func_08A819B4, Word);  // no comparable synthetic case yet
// HOT332(0x08C2B298u, func_08C2B298, Word);  // no comparable synthetic case yet
// HOT332(0x08C3548Cu, func_08C3548C, Word);  // no comparable synthetic case yet
HOT332(0x08C3BBB8u, func_08C3BBB8, Word);
HOT332(0x08874338u, func_08874338, Word);
HOT332(0x08880468u, func_08880468, Word);
// HOT332(0x08880BACu, func_08880BAC, Word);  // no comparable synthetic case yet
HOT332(0x089012D4u, func_089012D4, Word);
HOT332(0x08909184u, func_08909184, Word);
HOT332(0x08949730u, func_08949730, Word);
// HOT332(0x08A386E8u, func_08A386E8, Word);  // no comparable synthetic case yet
