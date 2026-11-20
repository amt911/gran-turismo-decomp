// Hot batch 270: sixty- and sixty-one-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (sign tests on three probes, per-axis setters, a display-list push of two vectors,
// heap adjustment on float keys, element copies with nested vectors, formatted pushes and handle-guarded calls).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_270 {
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

void func_089011E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put_float(rt, frame.at(0x18u), F[22]); F[22] = F[12]; F[12] = F[14]; put_float(rt, frame.at(0x10u), F[20]); F[20] = F[13]; G[5] = 0u; put(rt, frame.at(4u), G[31]); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put_float(rt, frame.at(0x1Cu), F[23]); put_float(rt, frame.at(0x14u), F[21]); F[21] = F[14]; call_here(rt, ctx, 0x089010A0u); F[12] = F[20]; F[13] = F[22]; G[4] = G[16]; G[5] = 0u; F[23] = F[0]; call_here(rt, ctx, 0x089010A0u); F[12] = F[21]; F[13] = F[22]; G[4] = G[16]; G[5] = 0u; F[20] = F[0]; call_here(rt, ctx, 0x089010A0u); F[1] = 0.0f; F[2] = F[1]; bool done = false; if (F[1] < F[23]) { G[2] = 1u; if (F[20] < F[1] && F[1] <= F[0]) done = true; } if (!done) { G[2] = 0u; if (F[23] < F[2]) { G[2] = 0xFFFFFFFFu; if (!(F[2] < F[20] && F[0] <= F[2])) G[2] = 0u; } } G[16] = word(rt, frame.at(0u)); F[23] = float_at(rt, frame.at(0x1Cu)); F[22] = float_at(rt, frame.at(0x18u)); F[21] = float_at(rt, frame.at(0x14u)); F[20] = float_at(rt, frame.at(0x10u)); } } }
void func_0895EB5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); auto &F = ctx.fpr; put(rt, frame.at(0x10u), G[20]); G[20] = 4u; put(rt, frame.at(0xCu), G[19]); G[19] = 0u; put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); G[17] = G[4]; put(rt, frame.at(0u), G[16]); G[16] = 0u; put_float(rt, frame.at(0x24u), F[21]); F[21] = 0.0f; put_float(rt, frame.at(0x20u), F[20]); G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); put(rt, frame.at(0x14u), G[31]); G[2] = word(rt, G[4]); G[18] = G[2] + 0xE4u; do { G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08955300u); G[3] = G[2]; G[2] = load8(G[2] + 0xEDu); G[1] = 0x3F800000u; F[2] = std::bit_cast<float>(G[1]); if (G[2] == 0u) { F[1] = float_at(rt, G[18] + 0x138u); F[0] = float_at(rt, G[3] + 0xA4u); F[0] = F[0] * F[1]; if (F[21] < F[0]) { F[2] = F[20] - F[0]; G[19] = 1u; } } else { G[2] = load8(G[3] + 0xE9u); G[2] = G[2] << 2; G[2] += G[18]; F[0] = float_at(rt, G[2] + 0x74u); if (F[0] < F[20]) { F[2] = F[0]; G[19] = 1u; } } G[16] += 1u; put_float(rt, G[3] + 0xA8u, F[2]); } while (G[16] != G[20]); G[2] = G[19]; G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); F[21] = float_at(rt, frame.at(0x24u)); F[20] = float_at(rt, frame.at(0x20u)); } } }
// 0x08979C60 (a0 = the object): when the object's byte +0x198 is set, 0x08A76AAC(frame +8, 0x08CF6ED8) and 0x08A76AA0(frame +0xC) make two
// slots, 0x08977260(object) gives a value that 0x089AB244(frame +4, frame) turns into frame +4, and frame +0xC takes frame +4's value through
// 0x08A5BAD8 when they differ, frame +4 being released. Then each of the object's four members (+0x19C, four words) gets his virtual slot 0xCC
// (the word at his vtable's +0xCC) with (member, frame +8, 1, frame +0xC); frame +0xC and frame +8 are released.
void func_08979C60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}});
    ctx.gpr[16] = self;
    if (load8(self + 0x198u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 source = frame.at(0x8u), slot = frame.at(0xCu), result = frame.at(0x4u);
    ctx.gpr[20] = source;
    ctx.gpr[18] = slot;
    ctx.gpr[17] = result;
    call_set(rt, ctx, 0x08A76AACu, {source, 0x08CF6ED8u});
    call_set(rt, ctx, 0x08A76AA0u, {slot});
    const u32 value = call_set(rt, ctx, 0x08977260u, {self});
    put(rt, frame.at(0x0u), value);                // the store of the call's delay slot
    call_set(rt, ctx, 0x089AB244u, {result, frame.at(0x0u)});
    if (slot != result) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, result)});
    call_set(rt, ctx, 0x08A5B9ECu, {result});
    ctx.gpr[19] = 4u;
    ctx.gpr[17] = 0u;
    for (u32 index = 0u; index < 4u; ++index) {
        const u32 field = self + 0x19Cu + 4u * index;
        const u32 member = word(rt, field);
        ctx.gpr[16] = field;
        if (member == 0u) continue;
        call_set(rt, ctx, word(rt, word(rt, member) + 0xCCu), {member, source, 1u, slot});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, 0x08A5B9ECu, {source});
}
// 0x08A17014 (a1 = the object, f12-f15 = four floats): each of the four floats is given to the object through one of its essential-value
// setters: 0x08966FB0(object) deciding, 0x0896A9C4(object, frame, 0, 0, 0) takes f12 (stored at frame +0 as the call's first argument);
// 0x08966FEC(object) deciding, the same call takes f13 at frame +0; 0x08967028(object) deciding, f14 goes at frame +0 with the call's a3
// pointing there; 0x08967064(object) deciding, f15 goes at frame +0 with a t0 pointing there. f20-f23 hold the four floats meanwhile.
void func_08A17014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[5];
    const float first = ctx.fpr[12], second = ctx.fpr[13], third = ctx.fpr[14], fourth = ctx.fpr[15];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {55u, 0x2Cu}, {54u, 0x28u}, {53u, 0x24u}, {52u, 0x20u}, {31u, 0x14u}});
    const u32 value = frame.at(0x0u);
    ctx.gpr[16] = self;
    ctx.fpr[23] = fourth;
    ctx.fpr[22] = third;
    ctx.fpr[21] = second;
    ctx.fpr[20] = first;
    if (call_set(rt, ctx, 0x08966FB0u, {self}) != 0u) {
        put_float(rt, value, first);               // the store of the call's delay slot
        call_set(rt, ctx, 0x0896A9C4u, {self, value, 0u, 0u, 0u});
    }
    if (call_set(rt, ctx, 0x08966FECu, {self}) != 0u) {
        put_float(rt, value, second);
        call_set(rt, ctx, 0x0896A9C4u, {self, 0u, value, 0u, 0u});
    }
    if (call_set(rt, ctx, 0x08967028u, {self}) != 0u) {
        put_float(rt, value, third);
        call_set(rt, ctx, 0x0896A9C4u, {self, 0u, 0u, value, 0u});
    }
    if (call_set(rt, ctx, 0x08967064u, {self}) != 0u) {
        put_float(rt, value, fourth);
        call_set(rt, ctx, 0x0896A9C4u, {self, 0u, 0u, 0u, value});
    }
}
void func_08A7E360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[22]); G[22] = 0x08D00000u; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = 0x08CFF450u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[18] + 0x20u; put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x24u), G[21]); G[21] = frame.at(4u); put(rt, frame.at(0x20u), G[20]); G[20] = G[6]; put(rt, frame.at(0x10u), G[16]); call_here(rt, ctx, 0x08B344E0u); G[4] = G[19]; G[5] = G[17]; call_here(rt, ctx, 0x08BFE7D0u); G[16] = G[2]; G[2] = G[18] + 0x24u; G[4] = frame.at(0u); G[5] = G[17]; bool found = false; if (G[2] != G[16]) { G[3] = word(rt, G[16] + 0x10u); G[2] = word(rt, G[17]); found = G[3] == G[2]; } if (!found) { call_here(rt, ctx, 0x08A6972Cu); G[4] = G[21]; G[5] = G[20]; call_here(rt, ctx, 0x08A5D424u); G[4] = G[19]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08BFEBACu); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); } G[5] = G[18]; G[6] = G[17]; G[7] = G[20]; G[4] = 0x08CF705Cu; call_here(rt, ctx, 0x08AAD93Cu); G[4] = 0x08CFF450u; call_here(rt, ctx, 0x08B3457Cu); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08AC0674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(8u), G[18]); G[18] = G[4]; G[4] = 0x138u; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); G[21] = G[6]; put(rt, frame.at(0x10u), G[20]); G[20] = G[5]; put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B62344u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08B0E008u); G[17] = G[16] + 0xF8u; G[2] = 0x08CD64B8u; put(rt, G[16], G[2]); G[4] = G[17]; call_here(rt, ctx, 0x08B0CDC8u); G[2] = 0x08AC0290u; put(rt, G[17] + 0x18u, G[2]); G[2] = 0x08CD64E8u; put(rt, G[17] + 0x1Cu, 0u); G[4] = G[16]; G[5] = G[18]; put(rt, G[16] + 0xF8u, G[2]); G[19] = G[16]; put(rt, G[16] + 0x130u, 0u); put(rt, G[16] + 0x134u, 0u); call_here(rt, ctx, 0x08B0EA48u); G[4] = G[16]; if (G[2] != 0u) { G[2] = 0x08AC0248u; G[3] = 1u; put(rt, G[16] + 0x30u, G[2]); store8(G[16] + 0x68u, G[3]); put(rt, G[16] + 0x130u, G[20]); put(rt, G[16] + 0x134u, G[21]); put(rt, G[16] + 0x34u, 0u); } else { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 4u); G[19] = 0u; call_here(rt, ctx, G[2]); } G[2] = G[19]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08AC35F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); G[2] = 0u; G[3] = 0u; put(rt, frame.at(8u), G[18]); G[18] = G[5]; put(rt, frame.at(4u), G[17]); G[17] = G[6]; put(rt, frame.at(0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, G[5], 0u); put(rt, G[5] + 8u, G[2]); put(rt, G[5] + 0xCu, G[3]); store8(G[5] + 0x58u, 0u); G[2] = word(rt, G[4]); G[16] = G[4]; auto finish = [&] { G[2] = G[3]; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); }; if (static_cast<std::int32_t>(G[2]) < 0) { call_here(rt, ctx, 0x08AC2D10u); G[3] = 0u; if (G[2] == 0u) { finish(); return; } } G[3] = word(rt, G[16] + 8u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; const bool lt = G[2] != 0u; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; if (lt) { G[4] = G[16]; call_here(rt, ctx, 0x08AC2D10u); G[3] = 0u; if (G[2] == 0u) { finish(); return; } G[3] = word(rt, G[16] + 8u); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; } bool fail = false; if (G[2] != 0u) { G[5] = G[17] - G[3]; G[4] = G[16]; call_here(rt, ctx, 0x08AC3570u); fail = G[2] == 0u; } if (!fail) { G[4] = word(rt, G[16]); G[5] = G[18]; call_here(rt, ctx, 0x08C7A858u); fail = static_cast<std::int32_t>(G[2]) < 0; } if (fail) { G[4] = G[16]; call_here(rt, ctx, 0x08AC2CDCu); G[3] = 0u; } else { G[3] = 1u; if (G[2] != 0u) { G[2] = word(rt, G[16] + 8u); G[2] += 1u; put(rt, G[16] + 8u, G[2]); } else G[3] = 0u; } finish(); } } }
// 0x08AC963C (a0 = the target, a1 = a second argument, a2 = an array of words, a3 = the count, t0 = a flag byte in the low bits, t1 = a
// value): for each of the count array words, 0x08ADC4E0(second, word, frame, frame +4, frame +8) looks it up and, when it is found, the frame
// words build the flags word: 0x20000 when the first is 0, else 0x10000 when it is exactly 2 and 0 otherwise (its xor with 2 being the t0 the
// callee sees), the flag byte shifted up 24, the second frame word shifted left 8 and the third in t1; that word and the running position
// go to 0x08AD372C(target, word, position, flags).
void func_08AC963C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4], second = ctx.gpr[5], source = ctx.gpr[6], count = ctx.gpr[7];
    const u32 flags = ctx.gpr[8], base = ctx.gpr[9];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[22] = target;
    ctx.gpr[21] = second;
    ctx.gpr[20] = base;
    ctx.gpr[19] = count;
    if (static_cast<s32>(count) <= 0) return;
    u32 entry = source;
    const u32 high = flags << 24;
    u32 index = 0u;
    ctx.gpr[17] = entry;
    ctx.gpr[30] = high;
    ctx.gpr[18] = index;
    ctx.gpr[23] = frame.at(0x8u);
    while (true) {
        const u32 value = word(rt, entry);
        ctx.gpr[16] = value;
        const u32 found = call_set(rt, ctx, 0x08ADC4E0u, {second, value, frame.at(0x0u), frame.at(0x4u), frame.at(0x8u)});
        entry += 0x4u;
        const u32 position = index + base;
        ++index;
        ctx.gpr[17] = entry;
        ctx.gpr[18] = index;
        if (found != 0u) {
            const u32 kind = word(rt, frame.at(0x0u));
            const u32 shifted = word(rt, frame.at(0x4u)) << 8;
            const u32 tail = word(rt, frame.at(0x8u));
            u32 kind_flag = 0x20000u;
            if (kind != 0u) kind_flag = (kind ^ 2u) != 0u ? 0u : 0x10000u;
            ctx.gpr[8] = kind ^ 2u;
            ctx.gpr[9] = tail;
            call_set(rt, ctx, 0x08AD372Cu, {target, value, position, high | kind_flag | shifted | tail});
        }
        if (count == index) break;
    }
}
void func_08AF8750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, frame.at(0x14u), G[31]); G[2] = load8(G[4] + 7u); G[4] = G[2]; bool done = false; if (G[2] != 0u) { G[5] = word(rt, G[16] + 0x28u); call_here(rt, ctx, 0x08B543B4u); if (G[2] == 0u) { G[2] = 0u; done = true; } } if (!done) { G[16] = word(rt, G[16] + 0x2Cu); F[0] = 0.0f; G[2] = 1u; if (G[16] != 0u) { F[1] = float_at(rt, G[16] + 0x18u); G[5] = 0x08C92A5Cu; G[4] = frame.at(0u); if (!(F[1] == F[0])) { call_here(rt, ctx, 0x08B5A6F0u); F[0] = float_at(rt, G[16]); F[1] = float_at(rt, frame.at(0u)); F[2] = float_at(rt, frame.at(4u)); F[1] = F[1] - F[0]; put_float(rt, frame.at(0u), F[1]); F[3] = F[1] * F[1]; F[0] = float_at(rt, G[16] + 4u); F[2] = F[2] - F[0]; F[0] = float_at(rt, frame.at(8u)); put_float(rt, frame.at(4u), F[2]); F[4] = F[2] * F[2]; F[5] = float_at(rt, G[16] + 8u); F[3] = F[3] + F[4]; F[0] = F[0] - F[5]; F[4] = F[0] * F[0]; put_float(rt, frame.at(8u), F[0]); F[3] = F[3] + F[4]; F[5] = float_at(rt, G[16] + 0xCu); F[4] = float_at(rt, G[16] + 0x10u); F[6] = float_at(rt, G[16] + 0x14u); F[3] = std::sqrt(F[3]); F[2] = F[2] * F[4]; F[1] = F[1] * F[5]; F[0] = F[0] * F[6]; F[4] = float_at(rt, G[16] + 0x18u); F[1] = F[1] + F[2]; F[1] = F[1] + F[0]; F[3] = F[3] * F[4]; G[2] = F[3] < F[1] ? 0u : 1u; } } } G[16] = word(rt, frame.at(0x10u)); } } }
void func_08B40950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); G[22] = G[9] & 0xFFu; put(rt, frame.at(0x14u), G[21]); G[21] = G[6]; put(rt, frame.at(0x10u), G[20]); G[20] = G[5]; put(rt, frame.at(0xCu), G[19]); G[19] = G[8]; put(rt, frame.at(8u), G[18]); G[18] = G[7]; put(rt, frame.at(4u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0u), G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xA4u); G[17] = G[4]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[3] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0x90u); G[16] = load16(G[20]); call_here(rt, ctx, G[2]); G[5] = G[21]; G[6] = G[18]; G[7] = G[19]; if (G[16] == G[2]) { G[4] = word(rt, G[17] + 0xB0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x98u); call_here(rt, ctx, G[2]); G[3] = 1u; } else { G[2] = word(rt, G[20] + 8u); G[5] = G[21]; G[6] = G[18]; G[8] = G[2]; G[9] = G[22]; G[3] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { G[4] = word(rt, G[17] + 0xB0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x68u); call_here(rt, ctx, G[2]); G[3] = G[2] != 0u ? 1u : 0u; } } } G[2] = G[3]; G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B556C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); auto &F = ctx.fpr; G[2] = 0x08D70000u; put(rt, frame.at(4u), G[17]); G[17] = 0x08D6CE00u; G[3] = word(rt, G[17] + 0x6Cu); put(rt, frame.at(8u), G[18]); G[18] = G[5]; G[5] = G[3]; put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = G[17]; put(rt, frame.at(0xCu), G[31]); if (G[3] != 0u) call_here(rt, ctx, 0x08B59118u); G[3] = word(rt, G[17] + 0x60u); G[10] = word(rt, G[17] + 0x2C4u); G[2] = 1u; G[5] = G[3] - 0x20u; put(rt, G[17] + 0x60u, G[5]); G[9] = G[3] - 0x10u; G[4] = 0x04000000u; put(rt, G[3] - 0x20u, G[10]); G[4] = (G[4] & ~0x00070000u) | ((G[2] & 7u) << 16); G[8] = 0x10000000u; F[0] = float_at(rt, G[16]); G[2] = G[5] >> 24; G[8] = (G[8] & ~0x00FF0000u) | ((G[2] & 0xFFu) << 16); put_float(rt, G[5] + 4u, F[0]); G[2] = 0x19Cu; G[6] = 0x12000000u; F[0] = float_at(rt, G[16] + 4u); G[6] = (G[6] & 0xFF000000u) | (G[2] & 0x00FFFFFFu); G[7] = 0x01000000u; put_float(rt, G[5] + 8u, F[0]); G[7] = (G[7] & 0xFF000000u) | (G[5] & 0x00FFFFFFu); F[1] = float_at(rt, G[16] + 8u); put(rt, G[3] - 0x10u, G[10]); G[3] = 2u; G[4] = (G[4] & 0xFFFF0000u) | (G[3] & 0xFFFFu); put_float(rt, G[5] + 0xCu, F[1]); F[0] = float_at(rt, G[18]); put_float(rt, G[9] + 4u, F[0]); F[1] = float_at(rt, G[18] + 4u); put_float(rt, G[9] + 8u, F[1]); F[0] = float_at(rt, G[18] + 8u); put_float(rt, G[9] + 0xCu, F[0]); G[2] = word(rt, G[17] + 0x5Cu); put(rt, G[2], G[8]); G[3] = G[2] + 0x10u; put(rt, G[2] + 4u, G[7]); put(rt, G[2] + 8u, G[6]); put(rt, G[2] + 0xCu, G[4]); put(rt, G[17] + 0x5Cu, G[3]); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B96B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x440u); G[2] = frame.at(0x428u); put(rt, frame.at(0x410u), G[16]); G[16] = frame.at(0xCu); put(rt, frame.at(0x418u), G[18]); G[18] = frame.at(4u); put(rt, frame.at(0x414u), G[17]); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0x428u), G[6]); G[6] = G[2]; put(rt, frame.at(0x41Cu), G[31]); put(rt, frame.at(0x42Cu), G[7]); put(rt, frame.at(0x430u), G[8]); put(rt, frame.at(0x434u), G[9]); put(rt, frame.at(0x438u), G[10]); put(rt, frame.at(0x43Cu), G[11]); put(rt, frame.at(8u), G[2]); call_here(rt, ctx, 0x08B373ECu); G[5] = G[16]; G[4] = G[18]; G[6] = frame.at(1u); call_here(rt, ctx, 0x08B93CD8u); G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[17] + 8u); G[6] = G[18]; G[4] = G[17]; G[5] = G[3]; if (G[3] != G[2]) { G[4] = G[3]; G[5] = G[18]; if (G[3] != 0u) { call_here(rt, ctx, 0x08B9432Cu); G[3] = word(rt, G[17] + 4u); } G[3] += 4u; put(rt, G[17] + 4u, G[3]); } else call_here(rt, ctx, 0x08B96974u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D75298u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B93554u); } G[18] = word(rt, frame.at(0x418u)); G[17] = word(rt, frame.at(0x414u)); G[16] = word(rt, frame.at(0x410u)); } } }
void func_08BDEF64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = G[5]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[6]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); if (G[4] != G[5]) { G[2] = 0xB6DB0000u; G[22] = 0xB6DB6DB7u; do { G[19] = G[17] + 4u; G[18] = G[16] + 4u; if (G[16] != 0u) { G[2] = load16(G[17]); G[4] = 0u; G[20] = 0u; store16(G[16], G[2]); G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[19] + 4u); put(rt, G[16] + 4u, 0u); G[2] -= G[3]; G[2] = sra(G[2], 2); const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[22])); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); put(rt, G[18] + 4u, 0u); G[3] = ctx.lo; put(rt, G[18] + 8u, 0u); if (G[3] != 0u) { G[20] = G[2] << 2; G[4] = G[20]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } put(rt, G[18] + 4u, G[4]); G[2] = G[4] + G[20]; G[6] = G[4]; put(rt, G[18], G[4]); G[7] = frame.at(0u); G[5] = word(rt, G[19] + 4u); G[4] = word(rt, G[19]); put(rt, G[18] + 8u, G[2]); call_here(rt, ctx, 0x08BDEAD8u); put(rt, G[18] + 4u, G[2]); } G[17] += 0x10u; G[16] += 0x10u; } while (G[21] != G[17]); } G[2] = G[16]; G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08C3E6F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; G[11] = G[5]; put(rt, frame.at(0x10u), G[31]); G[2] = G[11] << 1; G[9] = G[6]; F[0] = float_at(rt, G[7] + 4u); G[5] = word(rt, G[7]); G[6] = G[2] + 2u; G[3] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[9]) ? 1u : 0u; put_float(rt, frame.at(4u), F[0]); G[8] = G[4]; G[7] = G[11]; put(rt, frame.at(0u), G[5]); if (G[3] != 0u) { G[10] = G[11]; do { G[4] = G[6] - 1u; G[3] = G[6] << 3; G[2] = G[4] << 3; G[2] += G[8]; G[3] += G[8]; F[1] = float_at(rt, G[2] + 4u); F[0] = float_at(rt, G[3] + 4u); G[7] = G[6]; if (F[0] < F[1]) G[7] = G[4]; else G[2] = G[3]; G[4] = word(rt, G[2] + 4u); G[5] = word(rt, G[2]); G[3] = G[7] << 1; G[2] = (G[10] << 3) + G[8]; G[6] = G[3] + 2u; G[3] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[9]) ? 1u : 0u; G[10] = G[7]; put(rt, G[2] + 4u, G[4]); put(rt, G[2], G[5]); } while (G[3] != 0u); } G[5] = G[7]; if (G[9] == G[6]) { G[6] -= 1u; G[2] = (G[6] << 3) + G[8]; G[4] = word(rt, G[2] + 4u); G[5] = word(rt, G[2]); G[3] = (G[7] << 3) + G[8]; put(rt, G[3] + 4u, G[4]); G[7] = G[6]; put(rt, G[3], G[5]); G[5] = G[7]; } G[4] = G[8]; G[7] = frame.at(0u); G[6] = G[11]; call_here(rt, ctx, 0x08C3E628u); } } }
void func_0883D648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x1B0u); put(rt, frame.at(0x19Cu), G[19]); G[19] = frame.at(0x10u); put(rt, frame.at(0x1A0u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x198u), G[18]); G[18] = G[4]; G[4] = G[19]; put(rt, frame.at(0x194u), G[17]); G[17] = frame.at(8u); put(rt, frame.at(0x190u), G[16]); G[16] = G[6]; put(rt, frame.at(0x1A4u), G[31]); if (static_cast<std::int32_t>(G[5]) > 0) { call_here(rt, ctx, 0x0893600Cu); G[4] = G[16]; call_here(rt, ctx, 0x0883B848u); G[7] = G[3]; G[6] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08935B10u); G[4] = G[19]; call_here(rt, ctx, 0x08936184u); G[5] = G[2]; G[6] = frame.at(1u); G[4] = G[20]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[17]; G[5] = G[20]; call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; call_here(rt, ctx, 0x08932448u); } G[20] = word(rt, frame.at(0x1A0u)); G[19] = word(rt, frame.at(0x19Cu)); G[18] = word(rt, frame.at(0x198u)); G[17] = word(rt, frame.at(0x194u)); G[16] = word(rt, frame.at(0x190u)); } } }
void func_0885F844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); G[19] = G[5]; put(rt, frame.at(8u), G[18]); G[18] = G[4]; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[2] = word(rt, G[4]); G[17] = word(rt, G[2] + 0x5ACu); G[4] = G[17]; call_here(rt, ctx, 0x088D9FACu); if (G[2] != 0u) { G[4] = G[17]; call_here(rt, ctx, 0x088DA0BCu); if (G[2] == 0u) { G[16] = 0u; if (G[19] != 0u) { while (true) { G[4] = G[17]; call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] = G[16]; G[4] = G[17]; if (G[2] == 0u) break; call_here(rt, ctx, 0x088DC90Cu); G[3] = G[16] << 4; G[4] = G[16] << 6; G[5] = G[3] + G[4]; G[3] = 1u; G[4] = G[18]; if (G[2] == G[3]) { G[2] = word(rt, G[18]); G[2] = G[5] + G[2]; G[6] = word(rt, G[2] + 0x4158u); G[7] = word(rt, G[2] + 0x415Cu); call_here(rt, ctx, 0x0885E41Cu); G[6] = sext(load8(G[2] + 0x16u), 8); G[4] = G[19]; G[5] = G[16]; call_here(rt, ctx, 0x088D4D04u); } G[16] += 1u; } G[5] = G[19]; tail = true; } } } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } if (tail) jump(ctx, 0x088D6B6Cu); } }
void func_088AB5D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); auto &F = ctx.fpr; put(rt, frame.at(0x5Cu), G[19]); G[1] = 0x3F000000u; F[5] = std::bit_cast<float>(G[1]); G[19] = G[5]; put(rt, frame.at(0x58u), G[18]); G[5] = frame.at(0x14u); G[18] = G[6]; put(rt, frame.at(0x50u), G[16]); G[6] = frame.at(8u); G[16] = G[4]; put(rt, frame.at(0x60u), G[31]); put(rt, frame.at(0x54u), G[17]); G[9] = word(rt, G[4] + 0x14u); G[8] = word(rt, G[4] + 0x10u); G[17] = word(rt, G[4] + 0x1Cu); bool go = true; if (static_cast<std::int32_t>(G[9]) < 0) { F[0] = float_at(rt, G[4] + 8u); F[1] = float_at(rt, G[4]); F[2] = float_at(rt, G[4] + 4u); F[4] = float_at(rt, G[4] + 0xCu); F[1] = F[1] + F[0]; F[3] = float_at(rt, G[4] + 0x18u); F[2] = F[2] + F[4]; G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); F[3] = F[3] + F[0]; F[1] = F[1] * F[5]; F[2] = F[2] * F[5]; G[2] = 0xFFFFFFFFu; G[4] = G[17]; put(rt, frame.at(0x48u), G[2]); put_float(rt, frame.at(8u), F[1]); put_float(rt, frame.at(0xCu), F[3]); put_float(rt, frame.at(0x10u), F[2]); put(rt, frame.at(0x44u), 0u); call_here(rt, ctx, 0x088FAB84u); G[6] = word(rt, frame.at(0x48u)); if (G[2] == 0u) go = false; else { G[4] = word(rt, frame.at(0x44u)); G[2] = 0u; G[9] = G[6]; G[8] = G[2] | G[4]; } } if (go) { F[15] = float_at(rt, G[16] + 0xCu); F[12] = float_at(rt, G[16]); F[13] = float_at(rt, G[16] + 4u); F[14] = float_at(rt, G[16] + 8u); G[4] = G[17]; G[6] = G[19]; G[7] = G[18]; G[5] = frame.at(0u); put(rt, frame.at(4u), G[9]); put(rt, frame.at(0u), G[8]); call_here(rt, ctx, 0x088FACE8u); } G[19] = word(rt, frame.at(0x5Cu)); G[18] = word(rt, frame.at(0x58u)); G[17] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x50u)); } } }
// 0x088CFCD4 (a0 = the object): 0x088DA788(object's word at +0, object's +0x10 word) gives a state whose words +0x14 and +0x18 must be
// equal and whose float at +0 must not be below the one at +0xC, else 0 is returned. Then 0x088FE2FC(object's +0xC word) gives an object
// (null returns 0); with the object's byte +0x95 set and the state's +0x18 word at least 2, 0x088FE518(object's +0xC word) gives a value
// that 0x088FB144(value, f12 = 0.0, f13 = state[+4], f14 = state[+0]) tests; otherwise 0x088FDC18(object) gives f12 instead. Returns 1 when
// that test is positive, 0 otherwise.
void func_088CFCD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}});
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x088DA788u, {word(rt, self), word(rt, self + 0x10u)});
    const u32 state = ctx.gpr[2];
    ctx.gpr[17] = state;
    ctx.gpr[2] = 0u;
    if (word(rt, state + 0x14u) != word(rt, state + 0x18u)) return;
    if (float_at(rt, state) < float_at(rt, state + 0xCu)) return;
    const u32 found = call_set(rt, ctx, 0x088FE2FCu, {word(rt, self + 0xCu)});
    if (found == 0u) return;
    ctx.gpr[18] = found;
    u32 result = 0u;
    if (load8(self + 0x95u) != 0u) {
        if (word(rt, state + 0x18u) < 2u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 object = call_set(rt, ctx, 0x088FE518u, {word(rt, self + 0xCu)});
        ctx.fpr[14] = float_at(rt, state);
        ctx.fpr[12] = 0.0f;
        ctx.fpr[13] = float_at(rt, state + 0x4u);
        result = call_set(rt, ctx, 0x088FB144u, {object}) > 0u ? 1u : 0u;
    } else {
        const u32 object = call_set(rt, ctx, 0x088FE518u, {word(rt, self + 0xCu)});
        ctx.gpr[16] = object;
        call_set(rt, ctx, 0x088FDC18u, {found});
        ctx.fpr[12] = ctx.fpr[0];
        ctx.fpr[14] = float_at(rt, state);
        ctx.fpr[13] = float_at(rt, state + 0x4u);
        result = call_set(rt, ctx, 0x088FB144u, {object}) > 0u ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_270
} // namespace gt::game

#define HOT270(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_270::name, gt::ReturnKind::kind)
HOT270(0x089011E4u, func_089011E4, Word);
HOT270(0x0895EB5Cu, func_0895EB5C, Word);
HOT270(0x08979C60u, func_08979C60, Word);
HOT270(0x08A17014u, func_08A17014, Word);
HOT270(0x08A7E360u, func_08A7E360, Word);
HOT270(0x08AC0674u, func_08AC0674, Word);
// HOT270(0x08AC35F0u, func_08AC35F0, Word);  // no comparable synthetic case yet
HOT270(0x08AC963Cu, func_08AC963C, Word);
HOT270(0x08AF8750u, func_08AF8750, Word);
HOT270(0x08B40950u, func_08B40950, Word);
HOT270(0x08B556C0u, func_08B556C0, Word);
HOT270(0x08B96B54u, func_08B96B54, Word);
HOT270(0x08BDEF64u, func_08BDEF64, Word);
HOT270(0x08C3E6F0u, func_08C3E6F0, Word);
HOT270(0x0883D648u, func_0883D648, Word);
HOT270(0x0885F844u, func_0885F844, Word);
HOT270(0x088AB5D0u, func_088AB5D0, Word);
HOT270(0x088CFCD4u, func_088CFCD4, Word);
