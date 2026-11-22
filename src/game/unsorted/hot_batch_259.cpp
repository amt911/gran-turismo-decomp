// Hot batch 259: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (tail calls with an adjusted argument, delay loops in closed form, float setters that raise a dirty flag and a
// scaled truncation).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_259 {
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

// A tail call whose first argument the thunk computes: a0 = `argument`, then a jump to the shared body.
void tail_with_a0(psprecomp::AllegrexContext &ctx, u32 argument, u32 target) {
    ctx.gpr[4] = argument;
    jump(ctx, target);
}

// 0x08C01DDC / 0x08C249F8 / 0x08C24A18: the distance from a0 to a1 plus one; v0 is written only when they differ (otherwise it keeps what
// the caller left in it).
void distance_plus_one(psprecomp::AllegrexContext &ctx) {
    const u32 from = ctx.gpr[4];
    const u32 to = ctx.gpr[5];
    const u32 distance = to - from;
    ctx.gpr[4] = distance;
    if (from != to) ctx.gpr[2] = distance + 1u;
}

// 0x08965BE8 / 0x08965C14: a float setter for the field at a0 + `offset`; a changed value (a NaN counts as changed) is stored and bit 2
// of the flag word at +0xE4 is raised (v0 = the new flag word; unchanged: v0 is not touched, f0 holds the old value).
void set_float_and_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset) {
    const u32 object = ctx.gpr[4];
    const float value = ctx.fpr[12];
    const float old_value = float_at(rt, object + offset);
    ctx.fpr[0] = old_value;
    if (!(old_value == value)) {
        const u32 flags = word(rt, object + 0xE4u) | 4u;
        ctx.gpr[2] = flags;
        put_float(rt, object + offset, value);
        put(rt, object + 0xE4u, flags);
    }
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

void func_0889367C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_with_a0(ctx, word(rt, ctx.gpr[4]), 0x08893258u); }
void func_08896EE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; tail_with_a0(ctx, ctx.gpr[4] + 8u, 0x08896ED8u); }
void func_088B18B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; jump(ctx, 0x088B18B0u); }
void func_089260F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_with_a0(ctx, word(rt, ctx.gpr[4] + 0x1E0u), 0x0892779Cu); }
void func_089272F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; tail_with_a0(ctx, ctx.gpr[4] + 0x1FD4u, 0x08925BA4u); }
void func_08AC3384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_with_a0(ctx, word(rt, ctx.gpr[4]), 0x08AC328Cu); }
void func_08B097A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; ctx.gpr[5] &= 0xFFu; jump(ctx, 0x08B09520u); }
void func_08B1BE30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; tail_with_a0(ctx, ctx.gpr[5], 0x08B1BE28u); }
void func_08B2DBE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; tail_with_a0(ctx, 6u, 0x08B2DBD8u); }
void func_08C53F8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; tail_with_a0(ctx, ctx.gpr[4] + 0x24u, 0x08B6166Cu); }
void func_08B64344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0x08D70000u; ctx.gpr[5] = 1u; tail_with_a0(ctx, word(rt, 0x08D721B8u), 0x08C7AAC0u); }
void func_089961CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0xFFFFFFFFu; put(rt, ctx.gpr[4] + 0x20u, 0u); }
void func_08AC69F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { constexpr u32 kScaleBits = 0x472C4400u; ctx.gpr[1] = kScaleBits; ctx.fpr[0] = std::bit_cast<float>(kScaleBits); ctx.fpr[12] = ctx.fpr[12] * ctx.fpr[0]; const u32 scaled = static_cast<u32>(trunc_w_s(ctx.fpr[12])); ctx.fpr[1] = std::bit_cast<float>(scaled); put(rt, ctx.gpr[4] + 0x5A4u, scaled); } }
void func_08ACCD54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[5]; G[7] = word(rt, G[7]); G[5] = word(rt, G[5] + 0xB8u); G[4] = word(rt, G[2] + 0xB0u); jump(ctx, 0x08C7AD78u); } }
// 0x088E89CC: sets the two flag bytes at +0x13 and +0x14 of the object, then jumps into 0x088D08B0 with the
// sub-object at +0x30 as a0 (tail call; v0 = the object, v1 = 1 as the asm leaves them).
void func_088E89CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    ctx.gpr[2] = object;
    ctx.gpr[3] = 1u;
    store8(object + 0x14u, 1u);
    store8(object + 0x13u, 1u);
    tail_call(ctx, 0x088D08B0u, {object + 0x30u});
}
void func_08C01DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; distance_plus_one(ctx); }
void func_08C249F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; distance_plus_one(ctx); }
void func_08C24A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; distance_plus_one(ctx); }
void func_08965BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_float_and_flag(rt, ctx, 0x70u); }
void func_08965C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_float_and_flag(rt, ctx, 0x74u); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_259
} // namespace gt::game

#define HOT259(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_259::name, gt::ReturnKind::kind)
HOT259(0x0889367Cu, func_0889367C, Word);
HOT259(0x08896EE0u, func_08896EE0, Word);
HOT259(0x088B18B8u, func_088B18B8, Word);
HOT259(0x089260F4u, func_089260F4, Word);
HOT259(0x089272F4u, func_089272F4, Word);
HOT259(0x08AC3384u, func_08AC3384, Word);
HOT259(0x08B097A4u, func_08B097A4, Word);
HOT259(0x08B1BE30u, func_08B1BE30, Word);
HOT259(0x08B2DBE8u, func_08B2DBE8, Word);
HOT259(0x08C53F8Cu, func_08C53F8C, Word);
HOT259(0x08B64344u, func_08B64344, Word);
HOT259(0x089961CCu, func_089961CC, Word);
HOT259(0x08AC69F4u, func_08AC69F4, Word);
// HOT259(0x08ACCD54u, func_08ACCD54, Word);  // no comparable synthetic case yet
HOT259(0x088E89CCu, func_088E89CC, Word);
HOT259(0x08C01DDCu, func_08C01DDC, Word);
HOT259(0x08C249F8u, func_08C249F8, Word);
HOT259(0x08C24A18u, func_08C24A18, Word);
HOT259(0x08965BE8u, func_08965BE8, Word);
HOT259(0x08965C14u, func_08965C14, Word);
