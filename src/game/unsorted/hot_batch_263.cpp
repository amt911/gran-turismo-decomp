// Hot batch 263: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (jump tables on a code, a bit-by-bit callback dispatch, a float formula around two calls, a resource open
// and calls with a zeroed float argument).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_263 {
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

// 0x08918BB8 (a0 = the object; the tail call's result is this function's): 0x08926384 clears seven of the object's members with
// f12 = 0.0 (the ones at +0x20C, +0x268, +0x2C4, +0x2F0, +0x31C, +0x348 and +0x374) and the eighth, +0x3A0, is the tail call.
void func_08918BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.fpr[12] = 0.0f;
        call_set(rt, ctx, 0x08926384u, {object + 0x20Cu});
        const u32 members[] = {0x268u, 0x2C4u, 0x2F0u, 0x31Cu, 0x348u, 0x374u};
        for (const u32 offset : members) {
            ctx.fpr[12] = 0.0f;
            call_set(rt, ctx, 0x08926384u, {object + offset});
        }
    }
    ctx.fpr[12] = 0.0f;
    ctx.gpr[4] = object + 0x3A0u;
    jump(ctx, 0x08926384u);
}
// 0x08926140 (f12 = a value; f0 = the result): half of the value, turned to radians by 0x3C8EFA35, goes to 0x08B66604; its answer r
// gives 1.0 / r * 0x3ED1F42C * 0x4013CD46, and 0x08B65F0C(1.0 / that) gives s, so the answer is (s + s) * 0x42652EE1.
void func_08926140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.fpr[0] = std::bit_cast<float>(0x3F000000u);
    ctx.fpr[1] = ctx.fpr[12] * ctx.fpr[0];
    ctx.fpr[12] = std::bit_cast<float>(0x3C8EFA35u);
    ctx.fpr[12] = ctx.fpr[1] * ctx.fpr[12];
    call_set(rt, ctx, 0x08B66604u, {});
    ctx.fpr[12] = std::bit_cast<float>(0x3F800000u);
    ctx.fpr[0] = ctx.fpr[12] / ctx.fpr[0];
    ctx.fpr[1] = std::bit_cast<float>(0x3ED1F42Cu);
    ctx.fpr[2] = std::bit_cast<float>(0x4013CD46u);
    ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[1];
    ctx.fpr[0] = ctx.fpr[0] * ctx.fpr[2];
    ctx.fpr[12] = ctx.fpr[12] / ctx.fpr[0];
    call_set(rt, ctx, 0x08B65F0Cu, {});
    ctx.fpr[1] = ctx.fpr[0] + ctx.fpr[0];
    ctx.fpr[0] = std::bit_cast<float>(0x42652EE1u);
    ctx.fpr[0] = ctx.fpr[1] * ctx.fpr[0];
}
// 0x08B59118 (a0 = the object, a1 = a bit mask; v0 = the last handler's result, or whatever v0 held when no bit is set): the object's
// word +0x6C loses the masked bits, and each masked bit that was set calls its handler with the object as a0, highest bit first. The
// handler of bit p is the word at 0x08CE0E54 - (32 - p) * 4, i.e. the table whose entry of bit 31 ends at 0x08CE0E54.
void func_08B59118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 mask = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {16u, 0x0u}, {31u, 0xCu}, {17u, 0x4u}});
    ctx.gpr[18] = object;
    u32 pending = word(rt, object + 0x6Cu) & mask;
    if (pending == 0u) return;
    put(rt, object + 0x6Cu, word(rt, object + 0x6Cu) & ~mask);
    u32 table = 0x08CE0E54u;  // s1: each handler takes 4 more off it
    while (pending != 0u) {
        const u32 highest = std::countl_zero(pending);
        const u32 entry = table - highest * 4u;
        const u32 handler = word(rt, entry - 0x4u);
        table = entry - 0x4u;
        ctx.gpr[16] = pending;
        pending = (pending << highest) << 1u;
        call_set(rt, ctx, handler, {object});
    }
    ctx.gpr[2] = 32u;  // the `clz $v0, $s0` of the loop's last delay slot, with s0 = 0
}
// 0x08949D40 (a0 = a record whose word +0xC is a state and word +8 an object; f0 = the state's byte +0x270 as a value, 0.0 for a
// byte of 0xC or more): entry 0 gives the object's float +0x600, entry 1 its +0x5FC, entry 2 the mean of those two, entry 3 the
// +0x5FC plus the state's float +0x278 times (+0x5FC - +0x600), any other entry 0.0.
void func_08949D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 state = word(rt, ctx.gpr[4] + 0xCu), object = word(rt, ctx.gpr[4] + 0x8u);
    ctx.fpr[0] = 0.0f;
    const u32 index = load8(state + 0x270u);
    const u32 table = state + 0x270u;
    if (index >= 0xCu) return;
    switch (word(rt, 0x08C8B3B8u + index * 4u)) {
    case 0x08949D78u: ctx.fpr[0] = float_at(rt, object + 0x600u); break;
    case 0x08949D80u: ctx.fpr[0] = float_at(rt, object + 0x5FCu); break;
    case 0x08949D88u:
        ctx.fpr[0] = float_at(rt, object + 0x600u);
        ctx.fpr[1] = float_at(rt, object + 0x5FCu);
        ctx.fpr[2] = std::bit_cast<float>(0x3F000000u);
        ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0];
        ctx.fpr[0] = ctx.fpr[1] * ctx.fpr[2];
        break;
    case 0x08949DA4u:
        ctx.fpr[2] = float_at(rt, object + 0x600u);
        ctx.fpr[0] = float_at(rt, object + 0x5FCu);
        ctx.fpr[1] = float_at(rt, table + 0x8u);
        ctx.fpr[0] = ctx.fpr[0] - ctx.fpr[2];
        ctx.fpr[1] = ctx.fpr[1] * ctx.fpr[0];
        ctx.fpr[0] = ctx.fpr[2] + ctx.fpr[1];
        break;
    default: break;
    }
}
void func_08B2F774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[5] < 0xEu ? 1u : 0u; G[3] = 0x08C90000u; if (G[2] == 0u) { G[2] = 0u; return; } G[2] = G[5] << 2; G[3] = 0x08C93A78u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); if (G[4] == 0x08B2F798u) G[2] = 0xDu; else if (G[4] >= 0x08B2F7A0u && G[4] <= 0x08B2F800u && ((G[4] - 0x08B2F7A0u) & 7u) == 0u) G[2] = (G[4] - 0x08B2F7A0u) >> 3; else G[2] = 0u; } }
void func_0890AD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); G[19] = 0x08CF0000u; put(rt, frame.at(8u), G[18]); G[18] = G[5]; put(rt, frame.at(4u), G[17]); G[17] = G[6]; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x10u), G[31]); put(rt, G[5], 0u); G[5] = G[6]; G[2] = word(rt, 0x08CE9F68u); G[4] = G[2]; if (G[2] == 0u) G[16] = 0u; else { call_here(rt, ctx, 0x08AB7EC4u); G[3] = G[2]; G[5] = G[17]; if (G[2] == 0u) G[16] = 0u; else { G[2] = 1u; put(rt, G[16], G[2]); G[2] = 0x20u; put(rt, G[16] + 4u, G[3]); put(rt, G[16] + 8u, G[2]); G[4] = word(rt, 0x08CE9F68u); call_here(rt, ctx, 0x08AB7F5Cu); put(rt, G[18], G[2]); } } G[2] = G[16]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
// 0x08B56E20 (a0 = the object, a1 = an index below 0x10; v0 = the value the index's entry at 0x08C95674 names): the object's word
// +0xD00, +0xD04, +0xD08 or +0xD0C, or its +0x700 or +0x8C0; index 4 names 0x08B56C34, then 0x08B56C94, 0x08B56F34, 0x08B56CF4,
// 0x08B56A88, 0x08B56D4C, 0x08B56DC0 and 0x08B56B10, each the tail call with the object as a0. Any other index gives 0.
void func_08B56E20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    if (index >= 0x10u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 entry = word(rt, 0x08C95674u + index * 4u);
    switch (entry) {
    case 0x08B56E50u: ctx.gpr[2] = word(rt, object + 0xD00u); return;
    case 0x08B56E58u: ctx.gpr[2] = word(rt, object + 0xD04u); return;
    case 0x08B56E60u: ctx.gpr[2] = word(rt, object + 0xD08u); return;
    case 0x08B56E68u: ctx.gpr[2] = word(rt, object + 0xD0Cu); return;
    case 0x08B56E80u: ctx.gpr[2] = object + 0x700u; return;
    case 0x08B56EB8u: ctx.gpr[2] = object + 0x8C0u; return;
    default: break;
    }
    u32 target = 0u;
    switch (entry) {
    case 0x08B56E70u: target = 0x08B56C34u; break;
    case 0x08B56E78u: target = 0x08B56C94u; break;
    case 0x08B56E88u: target = 0x08B56F34u; break;
    case 0x08B56E90u: target = 0x08B56CF4u; break;
    case 0x08B56E98u: target = 0x08B56A88u; break;
    case 0x08B56EA0u: target = 0x08B56D4Cu; break;
    case 0x08B56EA8u: target = 0x08B56DC0u; break;
    case 0x08B56EB0u: target = 0x08B56B10u; break;
    default:
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[4] = object;
    jump(ctx, target);
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_263
} // namespace gt::game

#define HOT263(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_263::name, gt::ReturnKind::kind)
HOT263(0x08918BB8u, func_08918BB8, Word);
HOT263(0x08926140u, func_08926140, Float);
HOT263(0x08B59118u, func_08B59118, Word);
HOT263(0x08949D40u, func_08949D40, Float);
HOT263(0x08B2F774u, func_08B2F774, Word);
HOT263(0x0890AD68u, func_0890AD68, Word);
HOT263(0x08B56E20u, func_08B56E20, Word);
