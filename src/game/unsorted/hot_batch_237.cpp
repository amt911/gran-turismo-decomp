// Hot batch 237: fifty-seven-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (handle-guarded calls through temporaries, check chains, destructors, table interpolation and sums).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_237 {
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
    if (!(value >= -2377483648.0f && value < 2377483648.0f)) return 0x7FFFFFFF;
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

// 0x08934238 (a0 = the object; v0 = 1 when all twelve checks answer non-zero, 0 as soon as one answers 0): the checks are 0x089338C4,
// 0x089339D4, 0x08933A64, 0x08933B0C, 0x08933BB4, 0x08933C5C, 0x08933EB0, 0x08933FB8, 0x08934090, 0x089324DC, 0x08934120 and
// 0x089341A0, each with the object as a0.
void func_08934238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    ctx.gpr[16] = object;
    const u32 checks[] = {0x089338C4u, 0x089339D4u, 0x08933A64u, 0x08933B0Cu, 0x08933BB4u, 0x08933C5Cu,
                          0x08933EB0u, 0x08933FB8u, 0x08934090u, 0x089324DCu, 0x08934120u, 0x089341A0u};
    u32 result = 0u;
    for (const u32 check : checks) {
        if (call_set(rt, ctx, check, {object}) == 0u) {
            result = 0u;
            break;
        }
        result = 1u;
    }
    ctx.gpr[2] = result;
}
void func_08827AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[17] = frame.at(4u); G[4] = G[17]; call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C99B4Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[17]; G[17] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x0885C2C0u); G[16] = G[2]; G[2] = 0xFFFFFFFFu; G[4] = G[17]; if (G[16] != G[2]) { G[5] = G[16]; call_here(rt, ctx, 0x0885C164u); } G[5] = G[16] + 1u; G[5] = G[5] != 0u ? 1u : 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[5] = word(rt, frame.at(0u)); if (G[19] != frame.at(0u)) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0882A954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[2] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[19] = 0u; if (G[2] == 0u) { G[16] = word(rt, G[7] + 4u); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[19] = G[2]; } G[5] = G[17]; G[4] = frame.at(0u); call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08C90000u; if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; G[6] = G[19]; call_here(rt, ctx, 0x0885F82Cu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_088EC1A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[31]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[16] = G[4]; call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; G[5] = 1u; call_here(rt, ctx, 0x08B099A4u); G[4] = word(rt, G[16] + 0x538u); G[5] = G[2] << 6; G[2] <<= 2; G[3] = word(rt, G[4]); G[17] = G[5] - G[2]; G[2] = word(rt, G[3] + 0xBCu); call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[4] = word(rt, G[16] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xA0u); call_here(rt, ctx, G[2]); G[4] = G[16]; if (G[2] != 0u) { G[2] = word(rt, G[16]); G[3] = word(rt, G[2] + 0xA8u); call_here(rt, ctx, G[3]); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xACu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 0x56Cu); G[5] = 6u; } else { G[3] = word(rt, G[16] + 0x56Cu); G[4] = G[16]; G[2] = G[17] < G[3] ? 1u : 0u; G[5] = 5u; if (G[2] != 0u) { G[2] = word(rt, G[16]); G[3] = word(rt, G[2] + 0x10u); call_here(rt, ctx, G[3]); G[4] = 0x1Eu; call_here(rt, ctx, 0x08B0A998u); G[3] = word(rt, G[16] + 0x56Cu); G[5] = 0u; } } G[2] = G[3] + 1u; put(rt, G[16] + 0x56Cu, G[2]); G[2] = G[5]; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08936FAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CC67B8u; put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put(rt, frame.at(8u), G[18]); G[18] = 0x2Cu; put(rt, frame.at(4u), G[17]); G[17] = 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x10u), G[31]); put(rt, G[4], G[2]); G[2] = word(rt, G[16] + 0x8Cu); for (;;) { G[17] += 1u; G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } G[2] = word(rt, G[16] + 0x13Cu); G[16] += 4u; G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } if (G[17] == G[18]) break; G[2] = word(rt, G[16] + 0x8Cu); } G[4] = word(rt, G[19] + 0x1ECu); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = word(rt, G[19] + 0x1F0u); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = G[19] + 0x2D0u; call_here(rt, ctx, 0x08938FCCu); G[4] = G[19] + 0x1F4u; call_here(rt, ctx, 0x08938FCCu); G[4] = G[19]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08B64654u); } }
void func_089FB48C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[19]); G[19] = frame.at(0xCu); put(rt, frame.at(0x38u), G[18]); G[18] = G[5]; put(rt, frame.at(0x34u), G[17]); G[17] = G[4]; put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x40u), G[31]); G[16] = G[6]; call_here(rt, ctx, 0x089F9BFCu); G[4] = G[17] << 2; G[3] = word(rt, G[2]); G[2] = G[17] << 4; G[2] -= G[4]; G[3] += G[2]; G[4] = G[3]; G[5] = G[16]; if (G[3] != G[16]) call_here(rt, ctx, 0x08C0E654u); call_here(rt, ctx, 0x089F9E54u); G[16] = G[2]; G[5] = G[18]; G[4] = G[19]; call_here(rt, ctx, 0x08B88F44u); G[4] = G[16]; G[5] = G[19]; put(rt, frame.at(0x10u), G[17]); call_here(rt, ctx, 0x08C0DEF8u); G[5] = 0xFFFFFFFFu; G[6] = word(rt, frame.at(0xCu)); put(rt, frame.at(0x24u), G[3]); G[16] = G[6] - 0xCu; G[3] = load8(frame.at(0x24u)); put(rt, frame.at(4u), G[2]); G[4] = G[6] - 4u; put(rt, frame.at(0x20u), G[2]); G[2] = 0x08D74FD0u; store8(frame.at(8u), G[3]); if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_08A083F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; G[4] = G[20]; put(rt, frame.at(0x10u), G[16]); G[16] = G[6]; put(rt, frame.at(0x18u), G[18]); G[18] = 0xFFFFFFFFu; put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[17] = G[7]; call_here(rt, ctx, 0x08A08228u); { const bool none = static_cast<std::int32_t>(G[16]) <= 0; G[16] = word(rt, frame.at(4u)); if (!none) { G[16] = word(rt, G[17]); G[4] = 0x08CA90B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; G[16] = word(rt, frame.at(4u)); } } G[4] = 0x08CA90C0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[18]; G[2] = word(rt, G[3] + 0x1B0u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[12] = ctx.fpr[0]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// 0x08A4076C (a0 = the object; v0 = the last call's result): a text built by 0x08B85EB8(frame +4, 0x08CADA70, frame +1) is given to
// the object's +0 object's virtual +0x108 with the object, then released as a counted string (header = data - 0xC; unless it is the
// empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, frame) frees it). 0x08A76A94
// gives a new object (0x08A5BA08 on it), the old one at +0x38 is released (0x08A5B930), and 0x08B8D51C(object, 0x08CADAF8, 0x08A3FA58, 0)
// ends the call.
void func_08A4076C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 target = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = target;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CADA70u, frame.at(0x1u)});
    call_set(rt, ctx, target, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) <= 0)
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
    }
    const u32 created = call_set(rt, ctx, 0x08A76A94u, {});
    if (created != 0u) call_set(rt, ctx, 0x08A5BA08u, {created});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, object + 0x38u, created);
    call_set(rt, ctx, 0x08B8D51Cu, {object, 0x08CADAF8u, 0x08A3FA58u, 0u});
}
void func_08A449F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[4] = frame.at(0u); G[19] = G[7]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { call_here(rt, ctx, 0x08A449B8u); G[17] = word(rt, frame.at(0u)); G[4] = 0x08CADE28u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[17]); G[16] = word(rt, G[19]); G[4] = 0x08CADDF4u; G[20] = word(rt, G[2] + 0xE0u); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xDCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[16] = word(rt, G[19]); G[18] = G[2]; G[4] = 0x08CADDF4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xD8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[7] = G[2]; G[4] = G[17]; G[6] = G[18]; G[5] = 0u; call_here(rt, ctx, G[20]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08AE7CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[31]); G[2] = G[5]; G[6] = 0u; put(rt, frame.at(8u), G[18]); G[18] = G[4]; put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[17] = word(rt, G[5] + 4u); G[23] = word(rt, G[5] + 8u); G[16] = word(rt, G[2] + 0xCu); G[5] = G[17]; G[22] = word(rt, G[2] + 0x20u); G[19] = word(rt, G[2] + 0x14u); G[20] = word(rt, G[2] + 0x18u); G[21] = word(rt, G[2] + 0x1Cu); G[30] = word(rt, G[2] + 0x10u); call_here(rt, ctx, 0x08AE753Cu); G[4] = G[23]; call_here(rt, ctx, 0x08ADC1CCu); G[16] -= 0x3E8u; G[4] = G[18]; G[5] = G[17]; G[7] = G[16]; G[8] = G[19]; G[9] = G[20]; G[10] = G[21]; G[11] = G[22]; G[6] = G[23]; if (G[2] != 0u) { call_here(rt, ctx, 0x08AE7618u); G[4] = G[23]; if (G[2] != 0u) store16(G[2] + 0x40u, G[30]); call_here(rt, ctx, 0x08ADC230u); } G[30] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08AF0E9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[5] = G[4] & 0xFFFu; G[6] = sra(G[5], 3); G[2] = 0x08CE0000u; G[3] = G[6] << 1; G[2] = 0x08CE019Cu; G[4] = static_cast<std::int32_t>(G[5]) < 0x400 ? 1u : 0u; G[7] = static_cast<std::int32_t>(G[5]) < 0x800 ? 1u : 0u; G[3] += G[2]; G[8] = G[5] & 7u; if (G[4] != 0u) { G[2] = sext(load16(G[3] + 2u), 16); G[5] = sext(load16(G[3]), 16); goto blend; } G[2] = 0x08CE0000u; G[4] = 0x08CE019Cu; G[9] = static_cast<std::int32_t>(G[5]) < 0xC00 ? 1u : 0u; if (G[7] != 0u) { G[3] = 0xFFu; G[2] = 0x100u; G[3] -= G[6]; G[2] -= G[6]; G[2] <<= 1; G[3] <<= 1; G[2] += G[4]; G[3] += G[4]; G[5] = sext(load16(G[2]), 16); G[2] = sext(load16(G[3]), 16); goto blend; } G[3] = G[6] << 1; G[2] = 0x08CE019Cu; G[5] = G[3] + G[2]; G[4] = 0x1FFu; G[3] = 0x200u; G[4] -= G[6]; G[3] -= G[6]; G[2] = 0x08CE019Cu; G[3] <<= 1; G[4] <<= 1; G[4] += G[2]; G[3] += G[2]; if (G[9] != 0u) { G[2] = sext(load16(G[5] - 0x1FEu), 16); G[3] = sext(load16(G[5] - 0x200u), 16); G[2] = 0u - G[2]; G[5] = 0u - G[3]; } else { G[2] = sext(load16(G[3]), 16); G[3] = sext(load16(G[4]), 16); G[5] = 0u - G[2]; G[2] = 0u - G[3]; } blend: G[2] -= G[5]; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[8]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[2] = ctx.lo; G[2] = sra(G[2], 3); G[2] = G[5] + G[2]; } }
void func_08B061D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(4u), G[17]); G[17] = G[5]; G[5] += 0x38u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put_float(rt, frame.at(0x1Cu), ctx.fpr[23]); ctx.fpr[23] = ctx.fpr[13]; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(8u), G[18]); G[18] = G[6]; put_float(rt, frame.at(0x24u), ctx.fpr[25]); ctx.fpr[25] = ctx.fpr[12]; put_float(rt, frame.at(0x20u), ctx.fpr[24]); ctx.fpr[24] = ctx.fpr[14]; put_float(rt, frame.at(0x18u), ctx.fpr[22]); put_float(rt, frame.at(0x14u), ctx.fpr[21]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); call_here(rt, ctx, 0x08BBBEC4u); G[5] = G[17] + 0x50u; G[4] = G[16] + 0xCu; ctx.fpr[21] = ctx.fpr[0]; call_here(rt, ctx, 0x08BBBEC4u); G[4] = G[16] + 0x18u; G[5] = G[18] + 0x38u; ctx.fpr[22] = ctx.fpr[0]; call_here(rt, ctx, 0x08BBBEC4u); G[4] = G[16] + 0x24u; G[5] = G[18] + 0x50u; ctx.fpr[20] = ctx.fpr[0]; call_here(rt, ctx, 0x08BBBEC4u); ctx.fpr[1] = -ctx.fpr[23]; ctx.fpr[21] = ctx.fpr[21] + ctx.fpr[22]; ctx.fpr[20] = ctx.fpr[20] + ctx.fpr[0]; { const bool below = ctx.fpr[24] < ctx.fpr[1]; ctx.fpr[21] = ctx.fpr[21] + ctx.fpr[20]; if (!below) ctx.fpr[1] = ctx.fpr[24]; } if (!(ctx.fpr[1] < ctx.fpr[23])) ctx.fpr[1] = ctx.fpr[23]; ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[25]); ctx.fpr[0] = ctx.fpr[21] - ctx.fpr[0]; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); ctx.fpr[25] = float_at(rt, frame.at(0x24u)); ctx.fpr[24] = float_at(rt, frame.at(0x20u)); ctx.fpr[23] = float_at(rt, frame.at(0x1Cu)); ctx.fpr[22] = float_at(rt, frame.at(0x18u)); ctx.fpr[21] = float_at(rt, frame.at(0x14u)); ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } } }
void func_08B15CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x150u); put(rt, frame.at(0x14Cu), G[31]); put(rt, frame.at(0x148u), G[18]); G[18] = G[4]; put(rt, frame.at(0x144u), G[17]); G[17] = G[6]; put(rt, frame.at(0x140u), G[16]); G[16] = G[5]; call_here(rt, ctx, 0x08B0CE58u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B15C84u); G[5] = G[16]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0EA48u); G[16] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[17] + 4u); G[3] = word(rt, G[17]); G[4] = G[16]; put(rt, frame.at(0x124u), G[2]); G[2] = 1u; put(rt, frame.at(0x120u), G[3]); store8(frame.at(0x10Cu), 1u); store8(frame.at(0x128u), 0u); put(rt, G[16] + 0x7Cu, 0u); call_here(rt, ctx, 0x08B0DE3Cu); G[6] = G[16] + 0xA8u; G[7] = G[18]; G[8] = G[16] + 0xD8u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6] + 8u); G[3] = word(rt, G[6]); G[5] = word(rt, G[6] + 4u); G[4] = G[16]; put(rt, G[7] + 8u, G[2]); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[5]); call_here(rt, ctx, 0x08B0EA58u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08B159F4u); G[2] = G[18]; G[18] = word(rt, frame.at(0x148u)); G[17] = word(rt, frame.at(0x144u)); G[16] = word(rt, frame.at(0x140u)); } } }
void func_08B73CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xEC0u); put(rt, frame.at(0xEB4u), G[17]); G[6] = 0x4E8u; G[17] = G[5]; put(rt, frame.at(0xEB0u), G[16]); G[5] = 0u; G[16] = G[4]; G[4] = frame.at(0u); put(rt, frame.at(0xEB8u), G[31]); G[16] += 1u; call_here(rt, ctx, 0x08B36018u); G[4] = frame.at(0u); G[5] = frame.at(0x4E8u); put(rt, frame.at(0x4D0u), G[16]); call_here(rt, ctx, 0x08B7394Cu); G[3] = word(rt, frame.at(0xE90u)); if (G[2] != 0u) { G[17] = 0u; goto done; } G[2] = 2u; G[4] = G[17]; if (G[3] == G[2]) { G[17] = 0u; goto done; } G[3] = G[17]; G[5] = frame.at(0x4ECu); G[7] = frame.at(0xE84u); G[6] = 1u; G[8] = 2u; for (;;) { G[2] = sext(load8(G[5]), 8); store8(G[4] + 0x4E0u, G[2]); if (G[2] == G[6] || G[2] == G[8]) { G[2] = word(rt, G[5] - 4u); put(rt, G[3] + 0x10u, G[2]); } else { put(rt, G[3] + 0x10u, 0u); } G[5] += 8u; G[4] += 1u; const bool end = G[7] == G[5]; G[3] += 4u; if (end) break; } G[2] = word(rt, frame.at(0xE84u)); G[3] = word(rt, frame.at(0xE88u)); put(rt, G[17] + 8u, G[2]); G[2] = word(rt, frame.at(0xEA4u)); store16(G[17] + 0x4DCu, G[3]); store16(G[17] + 0x4DEu, G[2]); G[3] = word(rt, frame.at(0x4E4u)); G[2] = word(rt, frame.at(0xEACu)); put(rt, G[17] + 0xCu, G[3]); put(rt, G[17] + 4u, G[2]); done: G[2] = G[17]; G[17] = word(rt, frame.at(0xEB4u)); G[16] = word(rt, frame.at(0xEB0u)); } } }
void func_08BF5450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[9] = word(rt, G[4]); G[2] = 0xCCCCCCCDu; G[3] = G[5] - G[9]; G[3] = sra(G[3], 3); { GuestFrame frame(ctx, 0x10u); { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[17] = ctx.lo; put(rt, frame.at(8u), G[31]); G[8] = word(rt, G[4] + 4u); G[7] = word(rt, G[4] + 8u); G[16] = G[4]; if (G[8] != G[7] && G[8] == G[5]) { G[2] = G[8] + 0x28u; if (G[8] != 0u) { G[2] = word(rt, G[6] + 4u); ctx.fpr[0] = float_at(rt, G[6] + 0xCu); ctx.fpr[1] = float_at(rt, G[6] + 0x10u); G[3] = word(rt, G[6] + 8u); ctx.fpr[2] = float_at(rt, G[6] + 0x14u); ctx.fpr[3] = float_at(rt, G[6] + 0x18u); ctx.fpr[4] = float_at(rt, G[6] + 0x1Cu); put(rt, G[8] + 4u, G[2]); G[2] = 0x08CCE6E0u; put(rt, G[8] + 8u, G[3]); put_float(rt, G[8] + 0xCu, ctx.fpr[0]); put_float(rt, G[8] + 0x10u, ctx.fpr[1]); put_float(rt, G[8] + 0x14u, ctx.fpr[2]); put_float(rt, G[8] + 0x18u, ctx.fpr[3]); put_float(rt, G[8] + 0x1Cu, ctx.fpr[4]); ctx.fpr[0] = float_at(rt, G[6] + 0x24u); ctx.fpr[1] = float_at(rt, G[6] + 0x20u); put(rt, G[8], G[2]); put_float(rt, G[8] + 0x24u, ctx.fpr[0]); put_float(rt, G[8] + 0x20u, ctx.fpr[1]); G[8] = word(rt, G[4] + 4u); G[9] = word(rt, G[4]); G[2] = G[8] + 0x28u; } put(rt, G[16] + 4u, G[2]); } else { call_here(rt, ctx, 0x08BF5120u); G[9] = word(rt, G[16]); } G[3] = G[17] << 5; G[2] = G[17] << 3; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); G[2] += G[3]; G[2] = G[9] + G[2]; } } }
void func_08C7A14C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(4u), G[17]); G[17] = G[4] + 0xB0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = G[17]; put(rt, frame.at(8u), G[18]); G[18] = G[16] + 0xCu; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); call_here(rt, ctx, 0x08B65660u); G[2] = 0x08CDC420u; G[3] = word(rt, G[2] + 8u); G[6] = word(rt, G[2] + 0x10u); G[7] = word(rt, G[2] + 4u); store8(G[17] + 0x74u, 0u); G[4] = word(rt, G[3] - 0xCu); put(rt, G[16] + 4u, 0u); G[4] = G[16] + G[4]; G[3] = word(rt, G[6] - 0xCu); G[5] = word(rt, G[7] - 0xCu); G[8] = word(rt, G[2] + 0x18u); G[6] = word(rt, G[2] + 0xCu); G[7] = word(rt, G[2] + 0x14u); G[3] = G[16] + G[3]; G[2] = 0x08CDC454u; G[5] = G[16] + G[5]; put(rt, G[4], G[6]); G[4] = G[18]; put(rt, G[3] + 8u, G[7]); G[3] = 0x08CDC47Cu; put(rt, G[5], G[8]); put(rt, G[16], G[2]); G[2] = 0x08CDC468u; put(rt, G[16] + 0xB0u, G[3]); put(rt, G[16] + 8u, G[2]); put(rt, G[17] + 0x70u, 0u); store8(G[17] + 0x75u, 0u); put(rt, G[17] + 0x78u, 0u); put(rt, G[17] + 0x7Cu, 0u); put(rt, G[17] + 0x80u, 0u); put(rt, G[17] + 0x84u, 0u); call_here(rt, ctx, 0x08C771D4u); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x08C54A7Cu); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08C56804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; G[6] = 1u; put(rt, frame.at(0x14u), G[17]); G[17] = sext(G[5], 8); G[5] = G[16]; put(rt, frame.at(0x18u), G[31]); put(rt, G[4] + 4u, 0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08C55FDCu); G[2] = load8(frame.at(0u)); { const bool bad = G[2] == 0u; G[2] = G[16]; if (bad) goto done; } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] - 0xCu); G[2] = G[16] + G[2]; G[4] = word(rt, G[2] + 0x78u); if (G[4] == 0u) { G[4] = word(rt, G[3] - 0xCu); goto fail; } G[2] = word(rt, G[4] + 4u); G[3] = word(rt, G[4] + 8u); G[2] = G[2] < G[3] ? 1u : 0u; G[5] = sext(G[17], 8); if (G[2] != 0u) { G[2] = sext(load8(G[3] - 1u), 8); if (G[2] == G[5]) { G[2] = G[3] - 1u; put(rt, G[4] + 8u, G[2]); G[3] = load8(G[3] - 1u); goto check; } } G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x2Cu); G[5] &= 0xFFu; call_here(rt, ctx, G[2]); G[3] = G[2]; check: G[2] = 0xFFFFFFFFu; { const bool eof = G[3] == G[2]; G[2] = G[16]; if (!eof) goto done; } G[3] = word(rt, G[16]); G[4] = word(rt, G[3] - 0xCu); fail: G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); G[5] |= 1u; call_here(rt, ctx, 0x08C54CFCu); G[2] = G[16]; done: G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_237
} // namespace gt::game

#define HOT237(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_237::name, gt::ReturnKind::kind)
HOT237(0x08934238u, func_08934238, Word);
HOT237(0x08827AB4u, func_08827AB4, Word);
HOT237(0x0882A954u, func_0882A954, Word);
HOT237(0x088EC1A8u, func_088EC1A8, Word);
// HOT237(0x08936FACu, func_08936FAC, Word);  // no comparable synthetic case yet
// HOT237(0x089FB48Cu, func_089FB48C, Word);  // no comparable synthetic case yet
// HOT237(0x08A083F0u, func_08A083F0, Word);  // no comparable synthetic case yet
HOT237(0x08A4076Cu, func_08A4076C, Word);
HOT237(0x08A449F8u, func_08A449F8, Word);
HOT237(0x08AE7CD8u, func_08AE7CD8, Word);
HOT237(0x08AF0E9Cu, func_08AF0E9C, Word);
HOT237(0x08B061D8u, func_08B061D8, Float);
HOT237(0x08B15CB8u, func_08B15CB8, Word);
HOT237(0x08B73CB8u, func_08B73CB8, Word);
HOT237(0x08BF5450u, func_08BF5450, Word);
HOT237(0x08C7A14Cu, func_08C7A14C, Word);
HOT237(0x08C56804u, func_08C56804, Word);
