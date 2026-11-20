// Hot batch 286: seventy-three and seventy-four-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (heap sifts, a walk over stream tables, setters and a clamped steering vector).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_286 {
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

void func_08B76150(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = G[7] << 1; G[18] = G[2] + 1u; G[3] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[8]) ? 1u : 0u; G[23] = G[5]; G[22] = G[4]; G[20] = G[8]; G[19] = G[6]; if (G[3] != 0u) { G[21] = G[7]; while (true) { G[17] = G[18] + 1u; G[5] = G[18] << 2; G[3] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[20]) ? 1u : 0u; G[4] = G[22]; G[16] = G[19] + G[5]; if (G[3] != 0u) { G[5] = word(rt, G[16]); G[6] = word(rt, G[16] + 4u); call_here(rt, ctx, G[23]); G[3] = G[17] << 2; if (static_cast<std::int32_t>(G[2]) < 0) { G[18] = G[17]; G[16] = G[19] + G[3]; } } G[2] = G[21] << 2; G[17] = G[2] + G[19]; G[5] = word(rt, G[17]); G[6] = word(rt, G[16]); G[4] = G[22]; G[21] = G[18]; call_here(rt, ctx, G[23]); G[3] = G[18] << 1; G[3] += 1u; G[18] = G[3]; G[4] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[20]) ? 1u : 0u; if (static_cast<std::int32_t>(G[2]) >= 0) break; G[3] = word(rt, G[17]); G[2] = word(rt, G[16]); put(rt, G[17], G[2]); put(rt, G[16], G[3]); if (G[4] == 0u) break; } } G[18] = word(rt, frame.at(0x8u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[21] = word(rt, frame.at(0x14u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B794F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[22] = 0x08CE0000u; G[2] = word(rt, 0x08CE0E60u); G[21] = G[5]; G[20] = 0u; if (G[2] != G[4]) { G[19] = G[4] + 0x25Cu; while (G[19] != 0u) { G[18] = word(rt, G[19] + 4u); G[2] = G[18] - 1u; G[16] = word(rt, G[19] + 8u); if (static_cast<std::int32_t>(G[2]) >= 0) { G[17] = 0u; do { G[2] = sext(load16(G[16] + 0xCu), 16); G[4] = G[16]; G[17] += 1u; G[16] += 0x5Cu; if (G[2] != 0u) { call_here(rt, ctx, G[21]); G[20] |= G[2]; } } while (G[18] != G[17]); } G[19] = word(rt, G[19]); } } G[2] = word(rt, 0x08CE0E60u); G[19] = G[2] + 0x25Cu; while (G[19] != 0u) { G[18] = word(rt, G[19] + 4u); G[2] = G[18] - 1u; G[16] = word(rt, G[19] + 8u); if (static_cast<std::int32_t>(G[2]) >= 0) { G[17] = 0u; do { G[2] = sext(load16(G[16] + 0xCu), 16); G[4] = G[16]; G[17] += 1u; G[16] += 0x5Cu; if (G[2] != 0u) { call_here(rt, ctx, G[21]); G[20] |= G[2]; } } while (G[18] != G[17]); } G[19] = word(rt, G[19]); } G[2] = G[20]; G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B92850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); G[13] = G[5]; G[2] = G[13] << 1; G[11] = G[6]; G[5] = word(rt, G[7] + 4u); F[0] = float_at(rt, G[7]); G[6] = G[2] + 2u; G[3] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[11]) ? 1u : 0u; put(rt, frame.at(4u), G[5]); G[9] = G[4]; G[7] = G[13]; put_float(rt, frame.at(0u), F[0]); if (G[3] != 0u) { G[12] = G[13]; do { G[8] = G[6] - 1u; G[3] = G[6] << 3; G[2] = G[8] << 3; G[4] = G[2] + G[9]; G[3] += G[9]; F[1] = float_at(rt, G[4]); F[0] = float_at(rt, G[3]); G[5] = G[6]; if (F[0] < F[1]) { G[7] = word(rt, G[4] + 4u); G[5] = G[8]; } else if (F[1] < F[0]) { G[7] = word(rt, G[3] + 4u); G[4] = G[3]; } else { G[7] = word(rt, G[4] + 4u); G[10] = word(rt, G[3] + 4u); G[2] = static_cast<std::int32_t>(G[10]) < static_cast<std::int32_t>(G[7]) ? 1u : 0u; if (G[2] != 0u) G[5] = G[8]; else { G[4] = G[3]; G[7] = G[10]; } } G[4] = word(rt, G[4]); G[3] = G[5] << 1; G[2] = G[12] << 3; G[6] = G[3] + 2u; G[2] += G[9]; G[3] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[11]) ? 1u : 0u; G[12] = G[5]; put(rt, G[2] + 4u, G[7]); put(rt, G[2], G[4]); } while (G[3] != 0u); G[7] = G[5]; } G[5] = G[7]; if (G[11] == G[6]) { G[6] -= 1u; G[2] = G[6] << 3; G[2] += G[9]; G[4] = word(rt, G[2] + 4u); G[5] = word(rt, G[2]); G[3] = G[7] << 3; G[3] += G[9]; put(rt, G[3] + 4u, G[4]); G[7] = G[6]; put(rt, G[3], G[5]); G[5] = G[7]; } G[4] = G[9]; G[7] = frame.at(0u); G[6] = G[13]; call_here(rt, ctx, 0x08B92790u);  }  } } }
void func_08C2E1DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); G[19] = frame.at(4u); G[21] = frame.at(0xCu); G[20] = G[4]; G[18] = G[6]; G[17] = G[5]; G[16] = G[19]; do { G[4] = G[16]; G[16] += 4u; call_here(rt, ctx, 0x08A76AA0u); } while (G[21] != G[16]); G[4] = G[16]; G[4] = frame.at(8u); if (G[19] != G[17]) { G[5] = word(rt, G[17]); G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); G[4] = frame.at(8u); } if (G[4] != G[18]) { G[5] = word(rt, G[18]); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76AA0u); G[16] = word(rt, G[20] + 4u); if (G[16] == 0u) { G[4] = 0x08CB2328u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[4] = G[16]; G[5] = frame.at(0u); G[3] = word(rt, G[2] + 0xCCu); G[6] = 2u; G[7] = G[19]; call_here(rt, ctx, G[3]); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CB2328u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x30u); G[16] = G[21]; call_here(rt, ctx, G[2]); G[4] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] -= 4u; do { G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); const bool more = G[19] != G[16]; G[16] -= 4u; if (!more) break; } while (true); G[2] = G[17]; G[19] = word(rt, frame.at(0x1Cu)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08C4935C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); G[2] = G[5] << 1; G[24] = G[5]; G[5] = G[2] + 2u; G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[13] = G[6]; G[10] = G[4]; G[7] = sext(G[7] & 0xFFFFu, 16); const std::uint32_t extra = word(rt, G[8]); G[25] = word(rt, G[8] + 4u); G[3] = G[24]; if (G[2] != 0u) { G[15] = G[24]; G[9] = G[5]; G[14] = extra << 2; while (true) { G[12] = G[5] - 1u; G[2] = G[5] << 1; G[3] = G[12] << 1; G[3] += G[10]; G[2] += G[10]; G[11] = load16(G[2]); G[8] = load16(G[3]); G[6] = word(rt, G[25]); G[3] = sext(G[11], 16); G[4] = sext(G[8], 16); G[5] = G[3] << 8; G[2] = G[4] << 8; G[3] <<= 4; G[4] <<= 4; G[5] -= G[3]; G[2] -= G[4]; G[2] = G[6] + G[2]; G[6] += G[5]; G[6] += G[14]; G[2] += G[14]; F[1] = float_at(rt, G[6] + 0x98u); F[0] = float_at(rt, G[2] + 0x98u); if (F[1] < F[0]) G[9] = G[12]; else G[8] = G[11]; G[2] = G[9] << 1; G[3] = G[15] << 1; G[5] = G[2] + 2u; G[3] += G[10]; G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[13]) ? 1u : 0u; G[15] = G[9]; store16(G[3], G[8]); if (G[2] == 0u) break; G[9] = G[5]; } G[3] = G[9]; } G[8] = frame.at(0u); if (G[13] == G[5]) { G[5] -= 1u; G[2] = G[5] << 1; G[2] += G[10]; G[4] = load16(G[2]); G[3] <<= 1; G[3] += G[10]; store16(G[3], G[4]); G[3] = G[5]; } G[4] = G[10]; G[5] = G[3]; G[6] = G[24]; put(rt, frame.at(0u), extra); put(rt, frame.at(4u), G[25]); call_here(rt, ctx, 0x08C4929Cu);  }  } } }
void func_08C7240C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[31]); G[16] = G[4]; G[4] = frame.at(0u); G[17] = G[5]; G[5] = G[16]; G[18] = G[6]; call_here(rt, ctx, 0x08C70314u); G[2] = load8(frame.at(0u)); if (G[2] != 0u) { G[3] = word(rt, G[16]); G[5] = G[17]; G[2] = word(rt, G[3] - 0xCu); G[2] = G[16] + G[2]; G[4] = word(rt, G[2] + 0x78u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x30u); G[6] = G[18]; call_here(rt, ctx, G[2]); if (G[18] != G[2]) { G[2] = word(rt, G[16]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); G[5] |= 1u; call_here(rt, ctx, 0x08C54CFCu); } } G[3] = word(rt, frame.at(4u)); G[2] = word(rt, G[3]); G[4] = word(rt, G[2] - 0xCu); G[3] += G[4]; G[2] = word(rt, G[3] + 0xCu); G[2] &= 0x2000u; if (G[2] != 0u) { call_here(rt, ctx, 0x08B62318u); const bool pending = G[2] != 0u; G[2] = word(rt, frame.at(4u)); if (!pending) { G[3] = word(rt, G[2]); G[4] = word(rt, G[3] - 0xCu); G[2] += G[4]; G[4] = word(rt, G[2] + 0x78u); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x18u); call_here(rt, ctx, G[2]); G[3] = 0xFFFFFFFFu; if (G[2] == G[3]) { G[4] = word(rt, frame.at(4u)); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] - 0xCu); G[4] += G[3]; G[5] = word(rt, G[4] + 0x14u); G[5] |= 1u; call_here(rt, ctx, 0x08C54CFCu); } } } } G[2] = G[16]; G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); }  } }
void func_08C79CD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x18u), G[31]); G[16] = G[4] + 0xA8u; G[21] = 0x08CE0000u; G[17] = G[4]; G[4] = G[16]; G[20] = G[6]; G[19] = G[5]; G[18] = G[17] + 4u; call_here(rt, ctx, 0x08B65660u); G[5] = 0x08CDC358u; G[3] = word(rt, G[5] + 4u); store8(G[16] + 0x74u, 0u); G[2] = word(rt, G[3] - 0xCu); G[4] = word(rt, G[5] + 8u); put(rt, G[16] + 0x70u, 0u); G[2] = G[17] + G[2]; put(rt, G[2], G[4]); G[2] = 0x08CDC374u; put(rt, G[17], G[2]); G[2] = 0x08CDC388u; put(rt, G[17] + 0xA8u, G[2]); G[4] = G[18]; store8(G[16] + 0x75u, 0u); put(rt, G[16] + 0x78u, 0u); put(rt, G[16] + 0x7Cu, 0u); put(rt, G[16] + 0x80u, 0u); put(rt, G[16] + 0x84u, 0u); call_here(rt, ctx, 0x08C771D4u); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x08C54A7Cu); G[4] = G[18]; G[5] = G[19]; G[6] = G[20] | 0x10u; call_here(rt, ctx, 0x08C7835Cu); const bool opened = G[2] != 0u; G[2] = word(rt, G[17]); G[4] = word(rt, G[2] - 0xCu); G[4] = G[17] + G[4]; if (opened) G[5] = 0u; else { G[5] = word(rt, G[4] + 0x14u); G[5] |= 4u; } call_here(rt, ctx, 0x08C54CFCu); G[16] = word(rt, frame.at(0x0u)); G[21] = word(rt, frame.at(0x14u)); G[17] = word(rt, frame.at(0x4u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); }  } }
void func_0882E134(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[21] = G[4]; G[19] = G[7]; G[17] = G[6]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[18]; G[20] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = static_cast<std::int32_t>(G[17]) < 2 ? 1u : 0u; G[17] = 0u; if (G[2] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; G[5] = G[19] + 4u; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9A340u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = word(rt, G[19]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[20]; G[6] = G[17]; call_here(rt, ctx, 0x08860FD8u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0883BD4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x130u); put(rt, frame.at(0x120u), G[20]); put(rt, frame.at(0x11Cu), G[19]); put(rt, frame.at(0x118u), G[18]); put(rt, frame.at(0x114u), G[17]); put(rt, frame.at(0x110u), G[16]); put(rt, frame.at(0x124u), G[31]); G[2] = 0x08C90000u; G[3] = 0x08C8821Cu; G[20] = frame.at(4u); G[19] = frame.at(0xCu); G[18] = G[4]; G[4] = frame.at(0x18u); G[17] = frame.at(8u); G[16] = G[6]; G[6] = 0xF4u; if (static_cast<std::int32_t>(G[5]) > 0) { G[7] = word(rt, G[3] + 8u); G[2] = word(rt, 0x08C8821Cu); G[3] = word(rt, G[3] + 4u); G[5] = 0u; put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(0x14u), G[7]); call_here(rt, ctx, 0x08B36018u); G[16] = word(rt, G[16]); G[4] = 0x08C9BD48u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x4Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[2]; G[5] = G[19]; G[6] = 0x100u; call_here(rt, ctx, 0x0893119Cu); G[5] = G[19]; G[6] = frame.at(1u); G[4] = G[20]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[17]; G[5] = G[20]; call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[20] = word(rt, frame.at(0x120u)); G[19] = word(rt, frame.at(0x11Cu)); G[18] = word(rt, frame.at(0x118u)); G[17] = word(rt, frame.at(0x114u)); G[16] = word(rt, frame.at(0x110u)); }  } }
void func_0885DBF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x0u), G[16]); G[2] = 1u; G[22] = G[6]; G[19] = G[7]; G[18] = G[5] & 0xFFu; G[17] = G[4]; store8(G[4] + 0x16u, G[2]); if (G[6] != 0u) { G[16] = G[4] + 0x50u; G[5] = G[6]; G[4] = G[16]; call_here(rt, ctx, 0x088A6C18u); G[5] = G[18]; G[4] = G[16]; store8(G[17] + 0x15u, 0u); call_here(rt, ctx, 0x088A5938u); G[5] = G[16]; G[4] = G[17] + 0x120u; call_here(rt, ctx, 0x088A6C18u); } else store8(G[4] + 0x15u, G[2]); G[20] = G[19]; if (G[19] != 0u) { G[18] = G[17]; G[19] = G[17]; G[21] = 0u; G[30] = 0xFFFFFFFFu; G[23] = 2u; G[16] = G[17] + 0x58u; do { G[5] = word(rt, G[20]); bool used = false; if (G[5] != 0u && G[22] != 0u) { G[4] = G[16]; G[21] += 1u; call_here(rt, ctx, 0x08897470u); G[2] = 1u; store8(G[18] + 0x40u, G[2]); used = true; } if (!used) { if (G[5] != 0u) G[4] = G[16]; store8(G[18] + 0x40u, 0u); G[21] += 1u; } put(rt, G[19] + 0x44u, G[30]); G[16] += 0x30u; G[20] += 4u; G[19] += 4u; G[18] += 1u; } while (G[21] != G[23]); } G[2] = 0xFFFFFFFFu; put(rt, G[17] + 0x24u, G[2]); put(rt, G[17] + 0x1Cu, G[2]); put(rt, G[17] + 0x20u, G[2]); G[22] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[30] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_0887D3BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[17] = G[4]; G[16] = word(rt, G[4] + 0x54u); put(rt, G[4] + 0x50u, 0u); put(rt, G[4] + 0x54u, 0u); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[3] = word(rt, G[17] + 0x58u); G[2] = load8(G[3] + 0x25u); G[2] <<= 3; G[2] += 0x10u; G[3] += G[2]; G[16] = word(rt, G[3] + 8u); put(rt, G[3] + 4u, 0u); put(rt, G[3] + 8u, 0u); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = true; } } } G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, G[25]); } }
// 0x088ACA78 (a0 = the object, a1 = the first point, a2 = the second point, a3 = three floats out): 0x08B4A708(first) measures the first point (kept in f20);
// when it is not zero 0x08B4ABCC(second, first) refines it and the ratio (its float / f20) scales the first point. The difference from the second point, the
// negated object's float at +0x44 as a third component, make a vector whose squared length (plus itself as a dot product) is rooted: zero leaves the three
// floats zero, otherwise 1 / the root scaled by sqrt(f20) * 0x08CDD100, capped at the object's +0x42C float, multiplies the three components into the output
// (+8 the negated height, +0 the second difference, +4 the first).
void func_088ACA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], first = ctx.gpr[5], second = ctx.gpr[6], out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x10u}, {52u, 0x20u}});
    ctx.gpr[19] = self;
    ctx.gpr[18] = out;
    ctx.gpr[17] = second;
    ctx.gpr[16] = first;
    ctx.fpr[0] = 0.0f;
    ctx.fpr[20] = call_set(rt, ctx, 0x08B4A708u, {first});
    float scale = 0.0f;
    if (!(ctx.fpr[20] == 0.0f)) {
        call_set(rt, ctx, 0x08B4ABCCu, {second, first});
        scale = ctx.fpr[0] / ctx.fpr[20];
    }
    const float high_first = float_at(rt, first + 0x4u);
    const float low_first = float_at(rt, first);
    const float high_second = float_at(rt, second + 0x4u);
    const float scaled_high = psp_mul(scale, high_first);
    const float scaled_low = psp_mul(scale, low_first);
    const float low_second = float_at(rt, second);
    const float height = float_at(rt, self + 0x44u);
    const float d_high = scaled_high - high_second;
    const float d_low = scaled_low - low_second;
    const float neg_height = -height;
    const float q_high = psp_mul(d_high, d_high);
    const float q_low = psp_mul(d_low, d_low);
    const float q_height = psp_mul(neg_height, neg_height);
    const float root_length = std::sqrt(ctx.fpr[20]);
    const float sum = q_low + q_high + q_height;
    const bool zero = sum == 0.0f;
    const float root = std::sqrt(sum);
    if (zero) {
        put_float(rt, out + 0x8u, 0.0f);
        put_float(rt, out, 0.0f);
        put_float(rt, out + 0x4u, 0.0f);
        return;
    }
    const float inverse = 1.0f / root;
    const float limit = float_at(rt, self + 0x42Cu);
    float magnitude = psp_mul(inverse, psp_mul(root_length, float_at(rt, 0x08CDD100u)));
    if (limit < magnitude) magnitude = limit;
    const float x = psp_mul(magnitude, neg_height);
    const float y = psp_mul(magnitude, d_low);
    const float z = psp_mul(magnitude, d_high);
    put_float(rt, out + 0x8u, x);
    put_float(rt, out, y);
    put_float(rt, out + 0x4u, z);
}
void func_08937574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x120u); put(rt, frame.at(0x108u), G[18]); put(rt, frame.at(0x104u), G[17]); put(rt, frame.at(0x114u), G[31]); put(rt, frame.at(0x110u), G[20]); put(rt, frame.at(0x10Cu), G[19]); put(rt, frame.at(0x100u), G[16]); G[18] = G[4]; G[4] = frame.at(0u); G[17] = G[5]; G[5] = G[18] + 0xCu; G[20] = 0x08CA0000u; call_here(rt, ctx, 0x08B7BB6Cu); G[3] = G[17] << 2; G[2] = 0x08CDDE40u; G[2] = G[3] + G[2]; G[5] = word(rt, G[2]); G[4] = frame.at(0u); G[19] = G[3] + G[18]; call_here(rt, ctx, 0x08B7B9ACu); G[4] = 0xCu; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08937E04u); G[5] = G[17]; G[4] = G[18]; put(rt, G[19] + 0x8Cu, G[16]); call_here(rt, ctx, 0x08936498u); G[6] = 0x08CA21D0u; G[4] = G[18]; G[5] = frame.at(0u); if (G[2] != 0u) { call_here(rt, ctx, 0x089372C8u); G[5] = G[2]; G[4] = 0x10u; G[16] = G[2]; call_here(rt, ctx, 0x08937AFCu); G[3] = word(rt, G[18] + 8u); G[17] = G[2]; G[2] = 0x08CDDE0Cu; G[3] <<= 2; G[3] += G[2]; G[5] = word(rt, G[3]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B7B9ACu); G[4] = frame.at(0u); G[5] = 0x08CA21D0u; call_here(rt, ctx, 0x08B7B9ACu); G[4] = word(rt, G[19] + 0x8Cu); G[6] = G[17]; G[7] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08937F60u); } else { G[5] = 0x08CA21D0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B7B9ACu); G[4] = word(rt, G[19] + 0x8Cu); G[5] = frame.at(0u); call_here(rt, ctx, 0x08937EBCu); } G[18] = word(rt, frame.at(0x108u)); G[17] = word(rt, frame.at(0x104u)); G[20] = word(rt, frame.at(0x110u)); G[19] = word(rt, frame.at(0x10Cu)); G[16] = word(rt, frame.at(0x100u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_286
} // namespace gt::game

#define HOT286(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_286::name, gt::ReturnKind::kind)
HOT286(0x08B76150u, func_08B76150, Word);
HOT286(0x08B794F4u, func_08B794F4, Word);
HOT286(0x08B92850u, func_08B92850, Word);
// HOT286(0x08C2E1DCu, func_08C2E1DC, Word);  // no comparable synthetic case yet
HOT286(0x08C4935Cu, func_08C4935C, Word);
// HOT286(0x08C7240Cu, func_08C7240C, Word);  // no comparable synthetic case yet
HOT286(0x08C79CD0u, func_08C79CD0, Word);
HOT286(0x0882E134u, func_0882E134, Word);
HOT286(0x0883BD4Cu, func_0883BD4C, Word);
HOT286(0x0885DBF4u, func_0885DBF4, Word);
HOT286(0x0887D3BCu, func_0887D3BC, Word);
HOT286(0x088ACA78u, func_088ACA78, Word);
// HOT286(0x08937574u, func_08937574, Word);  // no comparable synthetic case yet
