// Hot batch 275: sixty-three- and sixty-four-instruction functions no route reaches, written by hand from
// `tools.disasm` and verified by synthetic cases (element copies with nested vectors, a vector erase over 48-byte
// records, a lock-guarded counter, a stream reset, a header-checked block copy and per-entry pair setup).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_275 {
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

void func_08C10EE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); auto &F = ctx.fpr; put(rt, frame.at(0x18u), G[22]); G[7] = G[4]; G[22] = G[5]; put(rt, frame.at(0u), G[16]); G[16] = G[6]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); if (G[4] != G[5]) { G[2] = 0x08CD0000u; G[23] = 0x08CCFF88u; G[21] = G[4]; do { G[18] = G[16] + 0x20u; G[17] = G[7] + 0x20u; G[4] = G[18]; G[5] = G[17]; G[20] = G[7] + 0x40u; G[19] = G[16] + 0x40u; if (G[16] != 0u) { G[2] = word(rt, G[7] + 4u); G[3] = word(rt, G[7] + 8u); F[0] = float_at(rt, G[7] + 0xCu); F[1] = float_at(rt, G[7] + 0x10u); F[2] = float_at(rt, G[7] + 0x14u); F[3] = float_at(rt, G[7] + 0x18u); F[4] = float_at(rt, G[7] + 0x1Cu); put(rt, G[16] + 4u, G[2]); put(rt, G[16] + 8u, G[3]); put_float(rt, G[16] + 0xCu, F[0]); put_float(rt, G[16] + 0x10u, F[1]); put_float(rt, G[16] + 0x14u, F[2]); put_float(rt, G[16] + 0x18u, F[3]); put_float(rt, G[16] + 0x1Cu, F[4]); put(rt, G[16], G[23]); call_here(rt, ctx, 0x0896418Cu); G[4] = G[19]; G[2] = word(rt, G[17] + 0x10u); G[5] = G[20]; put(rt, G[18] + 0x10u, G[2]); call_here(rt, ctx, 0x0896418Cu); G[2] = word(rt, G[20] + 0x10u); put(rt, G[19] + 0x10u, G[2]); } G[7] = G[21] + 0x60u; G[21] = G[7]; G[16] += 0x60u; } while (G[7] != G[22]); } G[2] = G[16]; G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B34890(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = 0xFFFFFFFFu; put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); G[17] = G[5] & 0xFFu; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); call_here(rt, ctx, 0x08B2E6DCu); G[18] = G[2]; if (G[17] != 0u) { call_here(rt, ctx, 0x08B2E0B0u); G[21] = G[2]; } call_here(rt, ctx, 0x08C7A888u); G[3] = word(rt, G[16] + 0x20u); G[4] = G[2]; if (G[3] == 0u) { G[2] = 1u; G[19] = 1u; G[20] = 1u; put(rt, G[16] + 0x18u, G[18]); put(rt, G[16] + 0x20u, G[2]); } else { G[2] = word(rt, G[16] + 0x18u); G[19] = 0u; G[20] = 0u; if (G[18] == G[2]) { G[2] = G[3] + 1u; put(rt, G[16] + 0x20u, G[2]); G[19] = 1u; G[20] = 1u; } } if (G[17] != 0u && G[19] != 0u) { G[2] = word(rt, G[16] + 0x24u); if (static_cast<std::int32_t>(G[2]) < 0) { put(rt, G[16] + 0x24u, G[3]); put(rt, G[16] + 0x1Cu, G[21]); } } call_here(rt, ctx, 0x08C7A8A8u); G[2] = G[20]; if (G[17] != 0u && G[19] == 0u) { G[4] = G[21]; call_here(rt, ctx, 0x08B2E0A4u); G[2] = G[20]; } G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08BE5A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(0x18u), G[18]); G[2] = 0xAAAAAAABu; put(rt, frame.at(0x14u), G[17]); G[18] = G[6]; G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; G[5] = G[4] + 4u; put(rt, frame.at(0x1Cu), G[31]); G[3] = word(rt, G[4] + 4u); G[3] -= G[6]; G[3] = sra(G[3], 4); const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); G[9] = ctx.lo; G[4] = G[16]; if (static_cast<std::int32_t>(G[9]) > 0) { G[8] = 0u; G[7] = G[16] + 0x2Cu; G[6] += 0x2Cu; do { F[0] = float_at(rt, G[6] - 0x20u); F[1] = float_at(rt, G[6] - 0x1Cu); G[2] = word(rt, G[6] - 0x28u); G[3] = word(rt, G[6] - 0x24u); F[2] = float_at(rt, G[6] - 0x18u); F[3] = float_at(rt, G[6] - 0x14u); F[4] = float_at(rt, G[6] - 0x10u); put(rt, G[7] - 0x28u, G[2]); G[8] += 1u; G[4] += 0x30u; put(rt, G[7] - 0x24u, G[3]); put_float(rt, G[7] - 0x20u, F[0]); put_float(rt, G[7] - 0x1Cu, F[1]); put_float(rt, G[7] - 0x18u, F[2]); put_float(rt, G[7] - 0x14u, F[3]); put_float(rt, G[7] - 0x10u, F[4]); F[0] = float_at(rt, G[6] - 0xCu); put_float(rt, G[7] - 0xCu, F[0]); F[1] = float_at(rt, G[6] - 8u); put_float(rt, G[7] - 8u, F[1]); F[0] = float_at(rt, G[6] - 4u); put_float(rt, G[7] - 4u, F[0]); F[1] = float_at(rt, G[6]); G[6] += 0x30u; put_float(rt, G[7], F[1]); G[7] += 0x30u; } while (G[9] != G[8]); } G[5] = word(rt, G[5]); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BE58F0u); G[3] = word(rt, G[17] + 4u); G[2] = (G[18] - G[16]) & ~0xFu; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08C78130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[17] = G[4] + 0x24u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = G[17]; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(8u), G[18]); call_here(rt, ctx, 0x08B61534u); bool ok = false; if (G[2] != 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08C76B7Cu); G[18] = G[2] < 1u ? 1u : 0u; G[4] = G[16]; put(rt, G[16] + 0x2Cu, 0u); store8(G[16] + 0x8Cu, 0u); call_here(rt, ctx, 0x08C76AFCu); G[4] = G[17]; G[2] = word(rt, G[16] + 0x78u); G[3] = word(rt, G[16] + 0x30u); G[5] = word(rt, G[16] + 0x34u); G[6] = word(rt, G[16] + 0x38u); G[7] = word(rt, G[16] + 0x3Cu); G[8] = word(rt, G[16] + 0x40u); G[9] = word(rt, G[16] + 0x44u); put(rt, G[16] + 0xCu, G[2]); put(rt, G[16] + 0x60u, G[3]); put(rt, G[16] + 0x64u, G[5]); put(rt, G[16] + 0x68u, G[6]); put(rt, G[16] + 0x6Cu, G[7]); put(rt, G[16] + 0x70u, G[8]); put(rt, G[16] + 0x74u, G[9]); store8(G[16] + 0x81u, 0u); store8(G[16] + 0x82u, 0u); put(rt, G[16] + 4u, G[2]); put(rt, G[16] + 8u, G[2]); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0x10u, 0u); put(rt, G[16] + 0x18u, 0u); put(rt, G[16] + 0x48u, G[3]); put(rt, G[16] + 0x4Cu, G[5]); put(rt, G[16] + 0x50u, G[6]); put(rt, G[16] + 0x54u, G[7]); put(rt, G[16] + 0x58u, G[8]); put(rt, G[16] + 0x5Cu, G[9]); call_here(rt, ctx, 0x08B61674u); ok = G[2] != 0u && G[18] == 0u; } G[2] = ok ? G[16] : 0u; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_0885D110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[22]); G[22] = G[4]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[4] = word(rt, G[4]); G[2] = word(rt, G[4] + 4u); if (static_cast<std::int32_t>(G[2]) > 0) { G[21] = 0u; G[20] = 0u; G[23] = 0xFFFFFFFFu; do { G[2] = word(rt, G[4]); G[17] = G[2] + G[20]; G[3] = load8(G[17] + 0x16u); bool go = false; if (G[3] != 0u) { G[18] = word(rt, G[17] + 0x1Cu); go = G[18] != G[23]; } if (go) { G[16] = word(rt, G[22] + 4u); G[4] = G[17]; call_here(rt, ctx, 0x0885D54Cu); G[6] = G[2]; G[4] = G[16]; G[5] = G[18]; G[19] = 0u; call_here(rt, ctx, 0x0885EC80u); G[16] = 2u; G[5] = G[19]; do { G[4] = G[17]; call_here(rt, ctx, 0x0885D56Cu); G[7] = G[2]; G[6] = G[19]; G[5] = G[18]; G[19] += 1u; if (G[2] != 0u) { G[4] = word(rt, G[22] + 4u); call_here(rt, ctx, 0x0885ED58u); } G[5] = G[19]; } while (G[19] != G[16]); G[4] = word(rt, G[22]); } G[2] = word(rt, G[4] + 4u); G[21] += 1u; G[2] = static_cast<std::int32_t>(G[21]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[20] += 0x188u; } while (G[2] != 0u); } G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_088D30B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0xCu), G[31]); G[2] = word(rt, G[4]); G[18] = G[2] + 4u; call_here(rt, ctx, 0x088D2B34u); G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[3]); G[17] = word(rt, G[2] + 8u); call_here(rt, ctx, 0x088B1330u); if (G[2] != 0u) { call_here(rt, ctx, 0x088B1330u); G[3] = word(rt, G[2]); G[4] = G[2]; G[2] = word(rt, G[3] + 8u); G[5] = 0u; call_here(rt, ctx, G[2]); if (G[2] != 0u) { call_here(rt, ctx, 0x088B1330u); G[3] = word(rt, G[2]); G[4] = G[2]; G[2] = word(rt, G[3] + 8u); G[5] = 0u; call_here(rt, ctx, G[2]); G[17] = G[2]; } } G[16] = 0u; if (G[17] != 0u) { G[4] = G[18]; call_here(rt, ctx, 0x088D6358u); G[3] = 3u; if (G[2] == G[3]) { G[3] = load8(G[17] + 0x1Au); G[2] = G[3] << 2; G[16] = G[2] + G[3]; } else G[16] = load8(G[17] + 0x24u); } G[4] = G[18]; call_here(rt, ctx, 0x088D4FA8u); G[5] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { G[4] = G[18]; call_here(rt, ctx, 0x088D4FA8u); G[5] = 0u - G[2]; } G[4] = G[18]; call_here(rt, ctx, 0x088D4E34u); if (G[2] == 0u) { G[2] = 0x50u; if (G[16] == 0u) G[16] = G[2]; } else G[16] = G[2]; G[2] = G[16]; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08A065A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x90u); auto &F = ctx.fpr; put(rt, frame.at(0x84u), G[31]); put(rt, frame.at(0x80u), G[20]); G[20] = frame.at(0x10u); put(rt, frame.at(0x7Cu), G[19]); put(rt, frame.at(0x78u), G[18]); G[18] = 0x08CCFF88u; put(rt, frame.at(0x74u), G[17]); G[17] = G[4]; G[19] = G[17] + 0xA8u; put(rt, frame.at(0x70u), G[16]); call_here(rt, ctx, 0x089D8E2Cu); G[2] = 0x08CC71A0u; put(rt, G[17], G[2]); G[2] = 0x08CC738Cu; put(rt, G[17] + 0x38u, G[2]); G[5] = 4u; G[4] = 0u; put(rt, G[17] + 0xA8u, 0u); put(rt, G[19] + 4u, 0u); put(rt, G[19] + 8u, 0u); call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; put(rt, G[17] + 0xA8u, G[2]); G[4] = G[20]; put(rt, G[19] + 4u, G[2]); put(rt, G[19] + 8u, G[2]); call_here(rt, ctx, 0x089D76A8u); G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); G[2] = 0xFFFFFFFFu; G[4] = G[16]; G[6] = G[20]; G[7] = frame.at(0u); G[5] = 0u; put(rt, frame.at(0x60u), G[2]); put(rt, frame.at(0x40u), G[2]); put_float(rt, frame.at(0x50u), F[0]); put(rt, frame.at(0x10u), G[18]); for (const std::uint32_t off : {0x3Cu, 0x38u, 0x34u, 0x30u, 0x5Cu, 0x58u, 0x54u}) put_float(rt, frame.at(off), F[0]); call_here(rt, ctx, 0x08BE78E8u); G[4] = G[20]; put(rt, frame.at(0x10u), G[18]); call_here(rt, ctx, 0x089D6684u); G[2] = word(rt, G[17] + 0xA8u); put(rt, G[19] + 4u, G[2]); put(rt, G[17] + 0xB4u, 0u); G[20] = word(rt, frame.at(0x80u)); G[19] = word(rt, frame.at(0x7Cu)); G[18] = word(rt, frame.at(0x78u)); G[17] = word(rt, frame.at(0x74u)); G[16] = word(rt, frame.at(0x70u)); } } }
void func_08A26F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(4u), G[17]); G[17] = G[4]; put(rt, frame.at(0xCu), G[19]); G[19] = G[5] & 0xFFu; put(rt, frame.at(8u), G[18]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B344E0u); G[18] = load8(G[17] + 0x2Cu); if (G[18] != 0u) { call_here(rt, ctx, 0x08B0D4B8u); G[16] = 0u - G[2]; if (G[16] == 0u) { G[2] = load8(G[17] + 0x2Du); G[16] = G[2] != 0u ? 0u : 0xFFFFFFFEu; } } else { G[16] = 0u; if (G[19] == 0u) { call_here(rt, ctx, 0x08B0D678u); G[1] = 0x42C80000u; F[1] = std::bit_cast<float>(G[1]); F[0] = F[0] * F[1]; G[1] = 0x461C4000u; F[2] = std::bit_cast<float>(G[1]); G[2] = 0x000F423Fu; F[0] = F[0] * F[2]; G[3] = trunc_w_s(F[0]); F[0] = std::bit_cast<float>(G[3]); G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? G[3] : G[2]; put(rt, G[17] + 0x34u, G[3]); } } G[4] = G[17]; call_here(rt, ctx, 0x08B3457Cu); G[2] = word(rt, G[17] + 0x28u); if (G[2] != 0u) { bool tail = false; if (G[19] != 0u) { call_here(rt, ctx, 0x08B0D828u); tail = true; } else tail = G[18] != 0u; if (tail) { G[4] = G[17]; call_here(rt, ctx, 0x08A26ED8u); } } if (G[16] == 0u) G[16] = word(rt, G[17] + 0x34u); G[2] = G[16]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08AC406C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); auto lu = [&](std::uint32_t a) { return load8(a) | (load8(a + 1u) << 8) | (load8(a + 2u) << 16) | (load8(a + 3u) << 24); }; G[6] = static_cast<std::int32_t>(G[6]) < 0x118 ? 1u : 0u; put(rt, frame.at(0u), G[31]); G[8] = G[4]; G[3] = 0u; if (G[6] == 0u) { G[3] = word(rt, G[5]); G[2] = 0x53423031u; bool magic = G[3] == G[2]; G[3] = 0u; if (magic) { G[7] = G[4] + 0xC94u; G[2] = (G[5] | G[7]) & 3u; G[6] = G[5]; G[9] = G[5] + 0x110u; const bool un = G[2] != 0u; do { if (un) { G[2] = lu(G[6]); G[3] = lu(G[6] + 4u); G[4] = lu(G[6] + 8u); G[5] = lu(G[6] + 0xCu); } else { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); } put(rt, G[7], G[2]); G[6] += 0x10u; put(rt, G[7] + 4u, G[3]); G[7] += 0x10u; put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[9]); if (un) { G[2] = lu(G[6]); G[3] = lu(G[6] + 4u); put(rt, G[7], G[2]); put(rt, G[7] + 4u, G[3]); } else { G[2] = word(rt, G[6] + 4u); G[3] = word(rt, G[6]); put(rt, G[7] + 4u, G[2]); put(rt, G[7], G[3]); } store8(G[8] + 0xD2Bu, 0u); G[5] = G[8] + 0xCACu; G[4] = G[8] + 0x9DCu; call_here(rt, ctx, 0x08AC3DDCu); G[3] = G[2] != 0u ? 1u : 0u; } } G[2] = G[3]; } } }
// 0x08AE8EAC (a0 = the object; v0 = the frame word): with the signed byte at +0x36 above zero its entries at +0x38, 2 bytes apart, are walked: a non-negative
// one goes through 0x08AE9EB8(entry) and its answer is or-ed into the frame word at sp (the byte at +0x36 is read again after each call). Bit 1 of the frame word
// then also sets bit 6. When the byte at +0x36 is above zero and the entry at +0x38 is not negative, the object's word +0x28 being set runs 0x08AE9EFC(entry)
// and 0x08AE1D6C(object's word +0x10's +0x10's +8, +0x54, +0x58, +0x2C, +0x30, +0x3C, that answer, sp), whose answer is stored at +0x3C and returned.
void func_08AE8EAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {31u, 0x1Cu}, {16u, 0x10u}});
    ctx.gpr[18] = self;
    ctx.gpr[17] = 0u;
    u32 bits = 0u;
    put(rt, frame.at(0x0u), 0u);
    u32 count = load8(self + 0x36u);
    if (static_cast<s32>(sext(count, 8)) > 0) {
        u32 cursor = self + 0x38u;
        ctx.gpr[16] = cursor;
        u32 entry = sext(load16(cursor), 16);
        for (;;) {
            ctx.gpr[17] += 1u;
            cursor += 2u;
            ctx.gpr[16] = cursor;
            if (static_cast<s32>(entry) >= 0) {
                const u32 value = call_set(rt, ctx, 0x08AE9EB8u, {entry});
                bits = word(rt, frame.at(0x0u)) | value;
                put(rt, frame.at(0x0u), bits);
                count = load8(self + 0x36u);
            }
            if (!(static_cast<s32>(ctx.gpr[17]) < static_cast<s32>(sext(count, 8)))) break;
            entry = sext(load16(cursor), 16);
        }
    }
    bits = word(rt, frame.at(0x0u));
    if ((bits & 2u) != 0u) {
        bits |= 0x40u;
        put(rt, frame.at(0x0u), bits);
    }
    if (static_cast<s32>(sext(count, 8)) > 0) {
        const u32 head = sext(load16(self + 0x38u), 16);
        if (static_cast<s32>(head) >= 0 && word(rt, self + 0x28u) != 0u) {
            const u32 produced = call_set(rt, ctx, 0x08AE9EFCu, {head});
            const u32 inner = word(rt, word(rt, self + 0x10u) + 0x10u);
            const u32 first_arg = word(rt, inner + 0x8u);
            call_set(rt, ctx, 0x08AE1D6Cu,
                     {first_arg, word(rt, self + 0x54u), word(rt, self + 0x58u), word(rt, self + 0x2Cu), word(rt, self + 0x30u), word(rt, self + 0x3Cu),
                      produced, frame.at(0x0u)});
            put(rt, self + 0x3Cu, ctx.gpr[2]);
            return;
        }
    }
    ctx.gpr[2] = bits;
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_275
} // namespace gt::game

#define HOT275(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_275::name, gt::ReturnKind::kind)
// HOT275(0x08C10EE8u, func_08C10EE8, Word);  // no comparable synthetic case yet
HOT275(0x08B34890u, func_08B34890, Word);
// HOT275(0x08BE5A78u, func_08BE5A78, Word);  // no comparable synthetic case yet
HOT275(0x08C78130u, func_08C78130, Word);
// HOT275(0x0885D110u, func_0885D110, Word);  // no comparable synthetic case yet
// HOT275(0x088D30B0u, func_088D30B0, Word);  // no comparable synthetic case yet
HOT275(0x08A065A0u, func_08A065A0, Word);
// HOT275(0x08A26F3Cu, func_08A26F3C, Word);  // no comparable synthetic case yet
HOT275(0x08AC406Cu, func_08AC406C, Word);
HOT275(0x08AE8EACu, func_08AE8EAC, Word);
