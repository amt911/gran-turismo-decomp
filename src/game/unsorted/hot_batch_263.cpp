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

void func_08918BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { GuestFrame frame(ctx, 0x10u); F[12] = 0.0f; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(4u), G[31]); G[4] = G[4] + 0x20Cu; call_here(rt, ctx, 0x08926384u); for (const std::uint32_t off : {0x268u, 0x2C4u, 0x2F0u, 0x31Cu, 0x348u, 0x374u}) { F[12] = 0.0f; G[4] = G[16] + off; call_here(rt, ctx, 0x08926384u); } F[12] = 0.0f; G[4] = G[16] + 0x3A0u; G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08926384u); } }
void func_08926140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x10u); G[1] = 0x3F000000u; F[0] = std::bit_cast<float>(G[1]); F[1] = F[12] * F[0]; G[1] = 0x3C8EFA35u; F[12] = std::bit_cast<float>(G[1]); put(rt, frame.at(0u), G[31]); F[12] = F[1] * F[12]; call_here(rt, ctx, 0x08B66604u); G[1] = 0x3F800000u; F[12] = std::bit_cast<float>(G[1]); F[0] = F[12] / F[0]; G[1] = 0x3ED1F42Cu; F[1] = std::bit_cast<float>(G[1]); G[1] = 0x4013CD46u; F[2] = std::bit_cast<float>(G[1]); F[0] = F[0] * F[1]; F[0] = F[0] * F[2]; F[12] = F[12] / F[0]; call_here(rt, ctx, 0x08B65F0Cu); F[1] = F[0] + F[0]; G[1] = 0x42652EE1u; F[0] = std::bit_cast<float>(G[1]); F[0] = F[1] * F[0]; } }
void func_08B59118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto clz = [](std::uint32_t x) { return x == 0u ? 32u : static_cast<std::uint32_t>(__builtin_clz(x)); }; GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[18]); put(rt, frame.at(0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(4u), G[17]); G[3] = word(rt, G[4] + 0x6Cu); G[16] = G[3] & G[5]; G[18] = G[4]; if (G[16] != 0u) { G[2] = ~G[5] & G[3]; G[3] = 0x08CE0000u; put(rt, G[4] + 0x6Cu, G[2]); G[17] = 0x08CE0E54u; G[2] = clz(G[16]); do { G[3] = G[2] << 2; G[3] = G[17] - G[3]; G[5] = word(rt, G[3] - 4u); G[2] = G[16] << (G[2] & 31u); G[16] = G[2] << 1; G[4] = G[18]; G[17] = G[3] - 4u; call_here(rt, ctx, G[5]); G[2] = clz(G[16]); } while (G[16] != 0u); } G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_08949D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; G[3] = word(rt, G[4] + 0xCu); G[5] = word(rt, G[4] + 8u); F[0] = 0.0f; G[4] = load8(G[3] + 0x270u); G[2] = G[4] < 0xCu ? 1u : 0u; G[6] = G[3] + 0x270u; if (G[2] == 0u) return; G[2] = G[4] << 2; G[3] = 0x08C8B3B8u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08949D78u: F[0] = float_at(rt, G[5] + 0x600u); break; case 0x08949D80u: F[0] = float_at(rt, G[5] + 0x5FCu); break; case 0x08949D88u: F[0] = float_at(rt, G[5] + 0x600u); F[1] = float_at(rt, G[5] + 0x5FCu); G[1] = 0x3F000000u; F[2] = std::bit_cast<float>(G[1]); F[1] = F[1] + F[0]; F[0] = F[1] * F[2]; break; case 0x08949DA4u: F[2] = float_at(rt, G[5] + 0x600u); F[0] = float_at(rt, G[5] + 0x5FCu); F[1] = float_at(rt, G[6] + 8u); F[0] = F[0] - F[2]; F[1] = F[1] * F[0]; F[0] = F[2] + F[1]; break; default: break; } } }
void func_08B2F774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[5] < 0xEu ? ctx.gpr[5] : 0u;
}
// 0x0890AD68 (a0 = the object, a1 = a word pointer, a2 = a third value; v0 = the object or 0): the word at a1 is cleared; the global at 0x08CE9F68
// (none: 0) goes to 0x08AB7EC4(global, a2) for a handle, and a handle makes the object's word +0 = 1, +4 = the handle and +8 = 0x20, the word at a1
// = 0x08AB7F5C(word(0x08CE9F68), a2), and answers the object; anything else answers 0.
void func_0890AD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], word_pointer = ctx.gpr[5], third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x10u}});
    ctx.gpr[18] = word_pointer;
    ctx.gpr[17] = third;
    ctx.gpr[16] = object;
    put(rt, word_pointer, 0u);
    const u32 global = word(rt, 0x08CE9F68u);
    u32 result = 0u;
    if (global != 0u) {
        const u32 handle = call_set(rt, ctx, 0x08AB7EC4u, {global, third});
        if (handle != 0u) {
            put(rt, object, 0x1u);
            put(rt, object + 0x4u, handle);
            put(rt, object + 0x8u, 0x20u);
            put(rt, word_pointer, call_set(rt, ctx, 0x08AB7F5Cu, {word(rt, 0x08CE9F68u), third}));
            result = object;
        }
    }
    ctx.gpr[2] = result;
}
// 0x08B2F774: the 14-entry jump table at 0x08C93A78 (read from the ELF's .rodata) sends a1 below 14 to the handler
// that returns its own index — entry 13 goes to 0x08B2F798, which returns 13 — so the function returns a1 itself
// when it is below 14, and 0 otherwise.
void func_08B56E20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[5] < 0x10u ? 1u : 0u; G[6] = G[4]; if (G[2] == 0u) { G[2] = 0u; return; } G[3] = 0x08C95674u; G[2] = (G[5] << 2) + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B56E50u: G[2] = word(rt, G[6] + 0xD00u); return; case 0x08B56E58u: G[2] = word(rt, G[6] + 0xD04u); return; case 0x08B56E60u: G[2] = word(rt, G[6] + 0xD08u); return; case 0x08B56E68u: G[2] = word(rt, G[6] + 0xD0Cu); return; case 0x08B56E80u: G[2] = G[6] + 0x700u; return; case 0x08B56EB8u: G[2] = G[6] + 0x8C0u; return; default: break; } std::uint32_t target = 0u; switch (G[4]) { case 0x08B56E70u: target = 0x08B56C34u; break; case 0x08B56E78u: target = 0x08B56C94u; break; case 0x08B56E88u: target = 0x08B56F34u; break; case 0x08B56E90u: target = 0x08B56CF4u; break; case 0x08B56E98u: target = 0x08B56A88u; break; case 0x08B56EA0u: target = 0x08B56D4Cu; break; case 0x08B56EA8u: target = 0x08B56DC0u; break; case 0x08B56EB0u: target = 0x08B56B10u; break; default: G[2] = 0u; return; } G[4] = G[6]; jump(ctx, target); } }

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
