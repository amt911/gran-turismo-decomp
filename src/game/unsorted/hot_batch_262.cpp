// Hot batch 262: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (guarded static initialisers, a destructor over an empty array, a jump table that picks a string, a float
// pass-through and a box built around a point).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_262 {
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

// 0x08A64D88 (v0 = the object's address 0x08CFF288): the once-flag byte at 0x08CFF270 is read when zero 0x08B6441C(0x08CFF270) decides whether
// to initialise; then 0x08A64D64(0x08CFF288) builds the object, 0x08B64494(0x08CFF270) takes the flag and 0x08B77B1C(0x08A64E14) registers its
// destructor. Every path returns the object's address.
void func_08A64D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}});
    const u32 once_flag = 0x08CFF270u;
    const u32 object = 0x08CFF288u;
    ctx.gpr[16] = once_flag;
    if (signed_byte(rt, once_flag) == 0u && call_set(rt, ctx, 0x08B6441Cu, {once_flag}) != 0u) {
        call_set(rt, ctx, 0x08A64D64u, {object});
        call_set(rt, ctx, 0x08B64494u, {once_flag});
        call_set(rt, ctx, 0x08B77B1Cu, {0x08A64E14u});
    }
    ctx.gpr[2] = object;
}
// 0x08A66110 (v0 = the object's address 0x08CFF298): the once-flag byte at 0x08CFF260 is read when zero 0x08B6441C(0x08CFF260) decides whether
// to initialise; then 0x08A660EC(0x08CFF298) builds the object, 0x08B64494(0x08CFF260) takes the flag and 0x08B77B1C(0x08A6619C) registers its
// destructor. Every path returns the object's address.
void func_08A66110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}});
    const u32 once_flag = 0x08CFF260u;
    const u32 object = 0x08CFF298u;
    ctx.gpr[16] = once_flag;
    if (signed_byte(rt, once_flag) == 0u && call_set(rt, ctx, 0x08B6441Cu, {once_flag}) != 0u) {
        call_set(rt, ctx, 0x08A660ECu, {object});
        call_set(rt, ctx, 0x08B64494u, {once_flag});
        call_set(rt, ctx, 0x08B77B1Cu, {0x08A6619Cu});
    }
    ctx.gpr[2] = object;
}
// 0x08A66204 (v0 = the object's address 0x08CFF2A0): the once-flag byte at 0x08CFF258 is read when zero 0x08B6441C(0x08CFF258) decides whether
// to initialise; then 0x08A661E0(0x08CFF2A0) builds the object, 0x08B64494(0x08CFF258) takes the flag and 0x08B77B1C(0x08A66290) registers its
// destructor. Every path returns the object's address.
void func_08A66204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}});
    const u32 once_flag = 0x08CFF258u;
    const u32 object = 0x08CFF2A0u;
    ctx.gpr[16] = once_flag;
    if (signed_byte(rt, once_flag) == 0u && call_set(rt, ctx, 0x08B6441Cu, {once_flag}) != 0u) {
        call_set(rt, ctx, 0x08A661E0u, {object});
        call_set(rt, ctx, 0x08B64494u, {once_flag});
        call_set(rt, ctx, 0x08B77B1Cu, {0x08A66290u});
    }
    ctx.gpr[2] = object;
}
// 0x08A68C3C (v0 = the object's address 0x08CFF2E8): the once-flag byte at 0x08CFF210 is read when zero 0x08B6441C(0x08CFF210) decides whether
// to initialise; then 0x08A68C18(0x08CFF2E8) builds the object, 0x08B64494(0x08CFF210) takes the flag and 0x08B77B1C(0x08A68CC8) registers its
// destructor. Every path returns the object's address.
void func_08A68C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}});
    const u32 once_flag = 0x08CFF210u;
    const u32 object = 0x08CFF2E8u;
    ctx.gpr[16] = once_flag;
    if (signed_byte(rt, once_flag) == 0u && call_set(rt, ctx, 0x08B6441Cu, {once_flag}) != 0u) {
        call_set(rt, ctx, 0x08A68C18u, {object});
        call_set(rt, ctx, 0x08B64494u, {once_flag});
        call_set(rt, ctx, 0x08B77B1Cu, {0x08A68CC8u});
    }
    ctx.gpr[2] = object;
}
// 0x08B469E0 (a0 = the object): stores 0x08CDAAB8 at +0 and calls 0x08B46880(object); when the word at +0x489C is neither null nor
// the address +0x494C, it spins through the asm's eight-step wait loop (v0 ends at -1, dead before the next call), then calls
// 0x08B28DC4(object + 0x48) and tail-calls 0x08B486A8(object).
void func_08B469E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    ctx.gpr[16] = self;
    put(rt, self, 0x08CDAAB8u);
    call_set(rt, ctx, 0x08B46880u, {self});
    const u32 first = self + 0x489Cu;
    const u32 second = self + 0x494Cu;
    if (first != 0u && first != second) {
        // the wait loop: v0 counts 8 down to -1, which the next call overwrites
        u32 wait = 0x8u;
        while (wait != 0u) wait -= 1u;
    }
    call_set(rt, ctx, 0x08B28DC4u, {self + 0x48u});
    tail_call(ctx, 0x08B486A8u, {self});
}
void func_088D2BC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put_float(rt, frame.at(0x10u), F[20]); F[20] = F[12]; put(rt, frame.at(4u), G[31]); G[4] = word(rt, G[4]); call_here(rt, ctx, 0x088DB5C4u); F[0] = F[20]; if (G[2] == 0u) { G[16] = word(rt, frame.at(0u)); F[20] = float_at(rt, frame.at(0x10u)); return; } G[4] = word(rt, G[16]); call_here(rt, ctx, 0x088DB5C4u); F[12] = F[20]; G[16] = word(rt, frame.at(0u)); F[20] = float_at(rt, frame.at(0x10u)); G[4] = G[2]; } jump(ctx, 0x088FB0C8u); } }
// 0x08B48D68 (a0 = the object; v0 = a rodata address): the object's code at +0x10 (0..4) indexes the five-entry table at 0x08C95448, whose
// handlers each return one rodata address: 0 -> 0x08CB56D4, 1 and 2 -> 0x08CB56DC, 3 -> 0x08CB56E0, 4 -> 0x08CB56C4; any other code returns 0x08CB56D0.
void func_08B48D68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 code = word(rt, ctx.gpr[4] + 0x10u);
    if (code >= 5u) {
        ctx.gpr[2] = 0x08CB56D0u;
        return;
    }
    // the same five entries the jump table at 0x08C95448 holds, in index order
    const u32 results[5] = {0x08CB56D4u, 0x08CB56DCu, 0x08CB56DCu, 0x08CB56E0u, 0x08CB56C4u};
    ctx.gpr[2] = results[code];
}
void func_08899B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[31]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x10u); G[5] = frame.at(0u); call_here(rt, ctx, G[2]); F[1] = float_at(rt, frame.at(0u)); F[2] = float_at(rt, frame.at(4u)); F[3] = float_at(rt, frame.at(8u)); G[1] = 0x41000000u; F[0] = std::bit_cast<float>(G[1]); F[5] = F[1] + F[0]; F[6] = F[3] + F[0]; F[4] = F[2] + F[0]; F[1] = F[1] - F[0]; F[2] = F[2] - F[0]; F[3] = F[3] - F[0]; G[4] = frame.at(0xCu); G[5] = frame.at(0x1Cu); put_float(rt, frame.at(0xCu), F[1]); put_float(rt, frame.at(0x10u), F[2]); put_float(rt, frame.at(0x14u), F[3]); put_float(rt, frame.at(0x1Cu), F[5]); put_float(rt, frame.at(0x20u), F[4]); put_float(rt, frame.at(0x24u), F[6]); call_here(rt, ctx, 0x08B54474u); G[2] = G[2] != 0u ? 1u : 0u; } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_262
} // namespace gt::game

#define HOT262(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_262::name, gt::ReturnKind::kind)
HOT262(0x08A64D88u, func_08A64D88, Word);
HOT262(0x08A66110u, func_08A66110, Word);
HOT262(0x08A66204u, func_08A66204, Word);
HOT262(0x08A68C3Cu, func_08A68C3C, Word);
HOT262(0x08B469E0u, func_08B469E0, Word);
// HOT262(0x088D2BC8u, func_088D2BC8, Float);  // no comparable synthetic case yet
HOT262(0x08B48D68u, func_08B48D68, Word);
HOT262(0x08899B38u, func_08899B38, Word);
