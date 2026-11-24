// Hot batch 277: sixty-five-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (a corner quad from four axes, a bounded per-entry fit, a constructor over a temporary, a binary
// search over strings, a filtered list collect, a size-class lookup, a quaternion product and a retried draw).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_277 {
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

void func_0895469C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(0x10u), G[20]); G[20] = 4u; put(rt, frame.at(8u), G[18]); G[18] = G[4]; put(rt, frame.at(0u), G[16]); G[16] = 0u; put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); G[17] = word(rt, G[4] + 8u); G[19] = word(rt, G[4] + 0xCu); G[5] = G[16]; do { G[4] = G[18]; call_here(rt, ctx, 0x08955300u); G[4] = G[2]; G[2] = sra(G[16], 1); G[5] = G[2] << 2; G[3] = G[5] + G[19]; F[2] = float_at(rt, G[3] + 0x28u); F[0] = float_at(rt, G[17] + 0x28u); F[1] = float_at(rt, G[17] + 0x2Cu); G[2] = G[16] & 1u; F[0] = F[0] * F[2]; F[1] = F[2] * F[1]; if (G[2] == 0u) { F[0] = -F[0]; F[1] = -F[1]; } put_float(rt, G[4], F[0]); put_float(rt, G[4] + 4u, F[1]); G[2] = G[5] + G[19]; F[0] = float_at(rt, G[2] + 0x10u); F[2] = float_at(rt, G[17] + 0x1Cu); F[1] = float_at(rt, G[17] + 0x20u); G[3] = static_cast<std::int32_t>(G[16]) < 2 ? 1u : 0u; F[2] = F[2] * F[0]; F[3] = F[0] * F[1]; F[0] = float_at(rt, G[4]); F[1] = float_at(rt, G[4] + 4u); if (G[3] != 0u) { F[0] = F[2] + F[0]; F[1] = F[3] + F[1]; } else { F[0] = F[0] - F[2]; F[1] = F[1] - F[3]; } put_float(rt, G[4], F[0]); put_float(rt, G[4] + 4u, F[1]); G[16] += 1u; G[5] = G[16]; } while (G[16] != G[20]); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_089D9A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); auto &F = ctx.fpr; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); G[17] = 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(8u), G[18]); put_float(rt, frame.at(0x20u), F[20]); put(rt, G[4] + 0x94u, 0u); call_here(rt, ctx, 0x089D83F4u); G[3] = word(rt, G[16]); put_float(rt, G[16] + 0x98u, F[0]); G[2] = word(rt, G[3] + 0x1C8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[19] = G[2]; put(rt, G[16] + 0xA4u, 0u); bool reload = true; if (static_cast<std::int32_t>(G[2]) > 0) { while (true) { F[20] = float_at(rt, G[16] + 0x98u); G[3] = word(rt, G[16]); G[2] = load8(G[16] + 0x8Cu); put_float(rt, G[16] + 0x94u, F[20]); G[4] = G[16]; G[5] = G[17]; G[18] = word(rt, G[3] + 0x1D4u); if (G[2] != 0u) { G[2] = word(rt, G[3] + 0x1C8u); call_here(rt, ctx, G[2]); G[2] -= G[17]; G[5] = G[2] - 1u; } G[4] = G[16]; call_here(rt, ctx, G[18]); F[0] = float_at(rt, G[2] + 0xCu); F[1] = float_at(rt, G[2] + 0x10u); F[2] = float_at(rt, G[2] + 0x14u); F[3] = float_at(rt, G[16] + 0x90u); F[0] = F[0] + F[1]; F[0] = F[0] + F[2]; F[0] = F[20] + F[0]; const bool over = F[3] < F[0]; put_float(rt, G[16] + 0x98u, F[0]); if (over) break; G[17] = word(rt, G[16] + 0xA4u); G[17] += 1u; G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[19]) ? 1u : 0u; put(rt, G[16] + 0xA4u, G[17]); if (G[2] == 0u) { reload = false; break; } } } if (reload) G[17] = word(rt, G[16] + 0xA4u); if (G[19] == G[17]) { G[2] = G[17] - 1u; put(rt, G[16] + 0xA4u, G[2]); } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); F[20] = float_at(rt, frame.at(0x20u)); } } }
// 0x089F967C (a0 = the object): builds the object — 0x089D8E2C(object) first, then its vtable at +0, 0x08CC7E24 at +0x38 and the three words at +0xA8 /
// +0xAC / +0xB0 zeroed. 0x08A5BC20(0, 4) makes a block that is stored at all three (+0xA8, +0xAC and +0xB0). 0x089D76A8(sp + 4) fills a frame block whose
// following word is 0x08CCFC90, 0x089FA09C(sp + 0x24) and 0x089FA09C(sp + 0x34) fill two more, and 0x08BE834C(block, 0, sp + 4, sp) combines them. Then
// 0x089F9B74(sp + 0x34) and 0x089F9B74(sp + 0x24) run (each with sp + 4 = 0x08CCFC90 again), 0x089D6684(sp + 4) releases the block, the object's +0xAC word
// takes the block again and +0xB4 is zeroed. v0 = the object's word +0xA8.
void func_089F967C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x6Cu}, {22u, 0x68u}, {21u, 0x64u}, {20u, 0x60u}, {19u, 0x5Cu}, {18u, 0x58u}, {17u, 0x54u}, {16u, 0x50u}});
    const u32 block_a = frame.at(0x24u), block_b = frame.at(0x34u), block_c = frame.at(0x4u);
    ctx.gpr[22] = block_a;
    ctx.gpr[21] = block_b;
    ctx.gpr[20] = block_c;
    ctx.gpr[18] = self;
    const u32 inner = self + 0xA8u;
    ctx.gpr[19] = inner;
    ctx.gpr[17] = 0x08CCFC90u;
    call_set(rt, ctx, 0x089D8E2Cu, {self});
    put(rt, self, 0x08CC7C38u);
    put(rt, self + 0x38u, 0x08CC7E24u);
    put(rt, inner, 0u);
    put(rt, inner + 0x4u, 0u);
    put(rt, inner + 0x8u, 0u);
    const u32 made = call_set(rt, ctx, 0x08A5BC20u, {0u, 4u});
    put(rt, inner, made);
    ctx.gpr[16] = made;
    put(rt, inner + 0x4u, made);
    put(rt, inner + 0x8u, made);
    call_set(rt, ctx, 0x089D76A8u, {block_c});
    put(rt, frame.at(0x4u), ctx.gpr[17]);
    call_set(rt, ctx, 0x089FA09Cu, {block_a});
    call_set(rt, ctx, 0x089FA09Cu, {block_b});
    call_set(rt, ctx, 0x08BE834Cu, {made, 0u, block_c, frame.at(0x0u)});
    put(rt, frame.at(0x4u), ctx.gpr[17]);
    call_set(rt, ctx, 0x089F9B74u, {block_b});
    call_set(rt, ctx, 0x089F9B74u, {block_a});
    call_set(rt, ctx, 0x089D6684u, {block_c});
    put(rt, inner + 0x4u, word(rt, self + 0xA8u));
    put(rt, self + 0xB4u, 0u);
    ctx.gpr[2] = word(rt, self + 0xA8u);
}
void func_08ADD6FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[30]); G[2] = 0u; G[30] = G[6]; put(rt, frame.at(0x1Cu), G[23]); G[23] = G[7]; put(rt, frame.at(0x14u), G[21]); G[21] = G[5]; put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[20] = word(rt, G[4]); G[19] = G[4]; if (G[20] != 0u) { G[2] = word(rt, G[4] + 8u); G[17] = G[2] - 1u; G[18] = 0u; bool found = false; if (static_cast<std::int32_t>(G[17]) >= 0) { G[22] = 0x08C90E14u; G[3] = G[17] - G[18]; while (true) { G[3] = sra(G[3], 1); G[4] = word(rt, G[19] + 0xCu); G[16] = G[18] + G[3]; G[2] = G[16] << 2; G[2] += G[4]; G[3] = word(rt, G[2]); G[5] = G[21]; G[4] = G[20] + G[3]; if (G[3] == 0u) G[4] = G[22]; call_here(rt, ctx, 0x08B35FD8u); if (static_cast<std::int32_t>(G[2]) > 0) G[17] = G[16] - 1u; else { const bool less = static_cast<std::int32_t>(G[2]) < 0; G[18] = G[16] + 1u; if (!less) { put(rt, G[30], G[16]); G[2] = word(rt, G[19] + 0x10u); found = true; break; } } G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[18]) ? 1u : 0u; G[3] = G[17] - G[18]; if (G[2] != 0u) break; } } if (!found) { G[4] = word(rt, G[19] + 4u); G[2] = 0u; if (static_cast<std::int32_t>(G[4]) >= 0) { G[2] = word(rt, G[23]); if (static_cast<std::int32_t>(G[2]) < 0) put(rt, G[23], G[4]); G[2] = 0u; } } } G[30] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08ADE3A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(4u), G[17]); G[17] = G[4]; G[4] = G[5]; put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); G[20] = G[5]; put(rt, frame.at(0xCu), G[19]); G[19] = 0u; put(rt, frame.at(8u), G[18]); G[18] = G[6]; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08AD8BD0u); G[3] = 0x08AE0000u; G[21] = G[2] != 0u ? 0x08AD8AF0u : 0x08AD8A84u; G[2] = 0x08D40000u; G[4] = word(rt, 0x08D4067Cu); G[16] = 0u; G[3] = word(rt, G[4] + 8u); G[3] = static_cast<std::int32_t>(G[3]) < 0 ? 1u : 0u; if (G[3] == 0u) G[16] = G[4]; G[2] = G[19] << 2; if (G[16] != 0u) { G[17] = G[2] + G[17]; do { G[2] = load8(G[16] + 0x8Bu); G[2] &= 2u; if (G[2] == 0u) { bool test = true; if (G[18] == 0u) { G[2] = sext(load8(G[16] + 0xE9u), 8); test = static_cast<std::int32_t>(G[2]) <= 0; } if (test) { G[2] = word(rt, G[16] + 0x94u); G[4] = G[20]; G[5] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, G[21]); if (G[2] != 0u) { put(rt, G[17], G[16]); G[19] += 1u; G[17] += 4u; } } } } G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[16] = G[3]; } while (G[3] != 0u); } G[2] = G[19]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B3A480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x44u), G[21]); G[21] = G[5] + 4u; put(rt, frame.at(0x40u), G[20]); G[20] = G[4]; G[4] = G[5]; put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x48u), G[31]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x30u), G[16]); G[18] = load16(G[5] + 2u); G[2] = G[18] - 8u; G[17] = G[2]; G[3] = G[18] - 0x10u; G[2] = G[2] < 8u ? 1u : 0u; G[5] = G[3] < 0x38u ? 1u : 0u; G[19] = G[17]; if (G[2] == 0u) { G[19] = 0u; G[17] = 0u; if (G[5] != 0u) { G[2] = 0x24924925u; const std::uint64_t p = static_cast<std::uint64_t>(G[3]) * G[2]; ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(p >> 32); G[2] = ctx.hi; G[3] -= G[2]; G[3] = G[3] >> 1; G[2] += G[3]; G[17] = G[2] >> 2; G[19] = G[17]; } } call_here(rt, ctx, 0x08B21894u); G[4] = frame.at(0u); G[16] = G[2]; call_here(rt, ctx, 0x08B217A0u); G[2] = G[18] - 0x10u; G[2] = G[2] < 0x38u ? 1u : 0u; G[7] = 0x96u; if (G[2] == 0u) G[7] = 0u; G[2] = 0x4Bu; G[5] = G[16]; G[4] = frame.at(0u); G[6] = 0x4Bu; G[8] = 0u; store8(frame.at(4u), G[2]); if (G[19] != 0u) call_here(rt, ctx, 0x08B217ACu); G[5] = G[17]; G[6] = G[21]; G[4] = G[20]; G[7] = frame.at(0u); G[8] = 0u; call_here(rt, ctx, 0x08B215B8u); G[2] = G[20]; G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_08B4B6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; F[0] = float_at(rt, G[5]); F[4] = float_at(rt, G[6]); F[1] = float_at(rt, G[6] + 0xCu); F[2] = float_at(rt, G[5] + 0xCu); F[5] = float_at(rt, G[5] + 8u); F[1] = F[1] * F[0]; F[2] = F[2] * F[4]; F[3] = float_at(rt, G[6] + 4u); F[4] = float_at(rt, G[6] + 8u); F[0] = float_at(rt, G[5] + 4u); F[3] = F[3] * F[5]; F[1] = F[1] + F[2]; F[0] = F[0] * F[4]; F[1] = F[1] - F[3]; F[1] = F[1] + F[0]; put_float(rt, G[4], F[1]); F[1] = float_at(rt, G[5] + 4u); F[4] = float_at(rt, G[6] + 4u); F[0] = float_at(rt, G[6] + 0xCu); F[2] = float_at(rt, G[5] + 0xCu); F[5] = float_at(rt, G[5] + 8u); F[0] = F[0] * F[1]; F[2] = F[2] * F[4]; F[3] = float_at(rt, G[6]); F[4] = float_at(rt, G[6] + 8u); F[1] = float_at(rt, G[5]); F[3] = F[3] * F[5]; F[0] = F[0] + F[2]; F[1] = F[1] * F[4]; F[0] = F[0] + F[3]; F[0] = F[0] - F[1]; put_float(rt, G[4] + 4u, F[0]); F[0] = float_at(rt, G[6] + 4u); F[4] = float_at(rt, G[5] + 4u); F[1] = float_at(rt, G[5]); F[2] = float_at(rt, G[6]); F[5] = float_at(rt, G[5] + 8u); F[1] = F[1] * F[0]; F[2] = F[2] * F[4]; F[3] = float_at(rt, G[6] + 0xCu); F[4] = float_at(rt, G[6] + 8u); F[0] = float_at(rt, G[5] + 0xCu); F[3] = F[3] * F[5]; F[1] = F[1] - F[2]; F[0] = F[0] * F[4]; F[1] = F[1] + F[3]; F[1] = F[1] + F[0]; put_float(rt, G[4] + 8u, F[1]); F[1] = float_at(rt, G[6] + 0xCu); F[4] = float_at(rt, G[6]); F[0] = float_at(rt, G[5] + 0xCu); F[2] = float_at(rt, G[5]); F[3] = float_at(rt, G[5] + 4u); F[0] = F[0] * F[1]; F[2] = F[2] * F[4]; F[5] = float_at(rt, G[6] + 4u); F[1] = float_at(rt, G[5] + 8u); F[4] = float_at(rt, G[6] + 8u); F[0] = F[0] - F[2]; F[3] = F[3] * F[5]; F[1] = F[1] * F[4]; F[0] = F[0] - F[3]; F[0] = F[0] - F[1]; put_float(rt, G[4] + 0xCu, F[0]); } }
void func_08C44434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = G[5]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[8]; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x14u), G[17]); G[17] = G[7]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); auto restore = [&] { G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }; G[2] = load16(G[4] + 0x10u); G[2] = G[6] < G[2] ? 1u : 0u; G[16] = G[6]; if (G[2] == 0u) { G[2] = 0u; restore(); return; } G[4] = word(rt, G[4] + 0x30u); G[3] = G[6] << 4; G[2] = G[6] << 6; G[2] -= G[3]; G[4] += G[2]; G[3] = word(rt, G[4] + 0x18u); put(rt, frame.at(0u), G[3]); if (G[3] == 0u) { G[2] = 0u; restore(); return; } if (G[7] == 0u) G[17] = word(rt, G[18] + 0x68u); G[3] = load8(G[4] + 0x13u); if (G[3] != 0u) { G[5] = word(rt, G[17] + 0x28u); G[2] = G[5] & 1u; if (G[2] == 0u) { G[2] = G[5] & 2u; G[5] = word(rt, G[4] + 0x14u); G[4] = G[3]; call_here(rt, ctx, G[2] != 0u ? 0x08B542ACu : 0x08B543B4u); if (G[2] == 0u) { G[2] = 0u; restore(); return; } } } G[4] = G[18]; do { G[5] = G[20]; G[6] = G[16]; G[7] = G[17]; G[8] = frame.at(0u); G[9] = G[19]; call_here(rt, ctx, 0x08C437F8u); G[4] = G[18]; } while (G[2] != 0u); G[2] = 1u; restore(); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_277
} // namespace gt::game

#define HOT277(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_277::name, gt::ReturnKind::kind)
HOT277(0x0895469Cu, func_0895469C, Word);
// HOT277(0x089D9A84u, func_089D9A84, Word);  // no comparable synthetic case yet
HOT277(0x089F967Cu, func_089F967C, Word);
HOT277(0x08ADD6FCu, func_08ADD6FC, Word);
HOT277(0x08ADE3A8u, func_08ADE3A8, Word);
HOT277(0x08B3A480u, func_08B3A480, Word);
HOT277(0x08B4B6B0u, func_08B4B6B0, Word);
HOT277(0x08C44434u, func_08C44434, Word);
