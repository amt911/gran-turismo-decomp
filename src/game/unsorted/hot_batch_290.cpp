// Hot batch 290: seventy-five-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (a convex polygon clip test, a direction basis, speed clamps and constructors).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_290 {
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

// 0x088B67DC (a0 = the start point pair {x at +0, y at +4}, a1 = the end point pair, a2 = an entry table: the word at +0x40 is the entry
// count and each entry is a float pair {x, y} at +0 and +4): walks the count entries as consecutive edges (entry i and the entry after it,
// wrapping to entry 0 after the last), and for each edge forms the two cross products of the segment a0-a1 with it (delta_x = a1.x - a0.x,
// delta_y = a1.y - a0.y; side = next_y - y, span_x = next_x - x, arm_x = x - a0.x, arm_y = y - a0.y; crossing = delta_x * -side +
// delta_y * span_x, distance = -side * arm_x + span_x * arm_y). A zero crossing with a distance at or below 0 returns 0; otherwise the
// ratio distance / crossing narrows the interval kept in f12 (crossing < 0, the highest) and f9 (crossing >= 0, the lowest, starts at 1.0)
// and a span of at most -0.01 (-0xBC23D70A) returns 0 too. 1 when every edge passes.
void func_088B67DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 start = ctx.gpr[4], end = ctx.gpr[5], table = ctx.gpr[6];
    const float end_y = float_at(rt, end + 0x4u), start_y = float_at(rt, start + 0x4u);
    const float end_x = float_at(rt, end), start_x = float_at(rt, start);
    const u32 count = word(rt, table + 0x40u);
    const float delta_y = end_y - start_y, delta_x = end_x - start_x;
    if (count == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const float penetration = std::bit_cast<float>(0xBC23D70Au);
    float highest = 0.0f, lowest = 1.0f;
    for (u32 index = 0u;; ++index) {
        const u32 following = table + 8u * (index == count - 1u ? 0u : index + 1u);
        const u32 entry = table + 8u * index;
        const float entry_y = float_at(rt, entry + 0x4u), following_y = float_at(rt, following + 0x4u);
        const float entry_x = float_at(rt, entry), following_x = float_at(rt, following);
        const float side = following_y - entry_y;
        const float arm_x = entry_x - start_x;
        const float span_x = following_x - entry_x;
        const float arm_y = entry_y - start_y;
        const float negated = -side;
        const float crossing = psp_mul(delta_x, negated) + psp_mul(delta_y, span_x);
        const float distance = psp_mul(negated, arm_x) + psp_mul(span_x, arm_y);
        if (crossing == 0.0f) {
            if (distance <= 0.0f) {
                ctx.gpr[2] = 0u;
                return;
            }
            continue;
        }
        const float ratio = distance / crossing;
        if (crossing < 0.0f) {
            if (highest < ratio) highest = ratio;
        } else if (ratio < lowest) {
            lowest = ratio;
        }
        if (penetration <= highest - lowest) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (index == count - 1u) break;
    }
    ctx.gpr[2] = 1u;
}
void func_088DF3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; call_here(rt, ctx, 0x088D9E58u); do { if (G[2] == 0u) { G[2] = 0u; break; } G[4] = G[16]; call_here(rt, ctx, 0x088DA160u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[2] = 0u; break; } G[17] = word(rt, G[16] + 0x130u); G[2] = load16(G[17] + 0xAu); G[3] = load16(G[17] + 4u); G[2] = G[3] < G[2] ? 1u : 0u; if (G[2] != 0u) { G[2] = 0u; break; } G[4] = word(rt, G[16] + 0x640u); if (G[4] != 0u) { call_here(rt, ctx, 0x088E4AA8u); if (G[2] == 0u) { G[2] = 0u; break; } G[4] = word(rt, G[16] + 0x640u); call_here(rt, ctx, 0x088E50F4u); if (G[2] == 0u) { G[2] = word(rt, G[16] + 0x130u); G[3] = 3u; G[4] = load8(G[2] + 1u); if (G[4] == G[3]) { G[2] = 0u; break; } } G[2] = load8(G[16] + 0x5BDu); G[4] = word(rt, G[16] + 0x640u); if (G[2] != 0u) { call_here(rt, ctx, 0x088E4A80u); if (G[2] == 0u) { G[2] = 0u; break; } } else { call_here(rt, ctx, 0x088E4A58u); if (G[2] == 0u) { G[2] = 0u; break; } } G[17] = word(rt, G[16] + 0x130u); } G[4] = G[16]; call_here(rt, ctx, 0x088D9F80u); G[6] = G[2]; G[8] = 0u; G[4] = 0u; G[5] = 0u; G[7] = 1u; while (true) { G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[4] += 1u; if (G[2] == 0u) break; G[2] = word(rt, G[16] + 0xC0u); G[2] += G[5]; G[3] = word(rt, G[2] + 4u); G[5] += 0x2F8u; if (G[3] == G[7]) { G[2] = load8(G[2] + 0x10u); G[2] = G[2] != 0u ? 1u : 0u; G[8] += G[2]; } } G[2] = load16(G[17] + 0xAu); G[2] = static_cast<std::int32_t>(G[8]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[2] ^= 1u; } while (false); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); }  } }
void func_088F6AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x4u), G[31]); put_float(rt, frame.at(0x24u), F[25]); put_float(rt, frame.at(0x20u), F[24]); put_float(rt, frame.at(0x1Cu), F[23]); put_float(rt, frame.at(0x18u), F[22]); put_float(rt, frame.at(0x14u), F[21]); put_float(rt, frame.at(0x10u), F[20]); G[1] = 0x3F800000u; F[3] = 1.0f; G[16] = G[4]; F[23] = 0.0f; F[4] = F[23]; F[5] = F[23]; F[12] = float_at(rt, G[4]); F[13] = float_at(rt, G[4] + 8u); F[22] = float_at(rt, G[4] + 4u); F[0] = F[12] * F[12]; F[1] = F[13] * F[13]; F[2] = F[22] * F[22]; F[0] = F[0] + F[1]; F[21] = std::sqrt(F[0]); F[0] = F[21] * F[21]; const bool flat = F[21] == F[23]; F[0] = F[0] + F[2]; F[24] = std::sqrt(F[0]); if (!flat) { call_here(rt, ctx, 0x088F7A88u); F[20] = F[0]; F[12] = F[0]; call_here(rt, ctx, 0x08B662C4u); F[12] = F[20]; F[25] = F[0]; call_here(rt, ctx, 0x08B66524u); F[6] = F[0]; if (F[21] < F[22]) { F[3] = F[22] / F[24]; G[1] = 0x3F800000u; F[0] = 1.0f; F[1] = F[3] * F[3]; F[0] = F[0] - F[1]; F[0] = std::sqrt(F[0]); F[4] = F[6] * F[0]; F[5] = F[25] * F[0]; } else { F[2] = F[21] / F[24]; const bool below = F[22] < F[23]; F[1] = F[2] * F[2]; F[4] = F[0] * F[2]; G[1] = 0x3F800000u; F[0] = 1.0f; F[5] = F[25] * F[2]; F[0] = F[0] - F[1]; F[3] = std::sqrt(F[0]); if (below) { F[4] = F[6] * F[2]; F[3] = -F[3]; F[5] = F[25] * F[2]; } } } put_float(rt, G[16] + 0x18u, F[4]); put_float(rt, G[16] + 0x10u, F[3]); put_float(rt, G[16] + 0x14u, F[5]); F[25] = float_at(rt, frame.at(0x24u)); F[24] = float_at(rt, frame.at(0x20u)); F[23] = float_at(rt, frame.at(0x1Cu)); F[22] = float_at(rt, frame.at(0x18u)); F[21] = float_at(rt, frame.at(0x14u)); F[20] = float_at(rt, frame.at(0x10u)); G[16] = word(rt, frame.at(0x0u)); }  } } }
void func_08958974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[17] = G[4]; put_float(rt, frame.at(0x18u), F[22]); put_float(rt, frame.at(0x14u), F[21]); put_float(rt, frame.at(0x10u), F[20]); call_here(rt, ctx, 0x08958904u); if (G[2] != 0u) { G[3] = word(rt, G[17] + 8u); G[16] = G[3] + 0x778u; G[2] = load8(G[16] + 8u); if (G[2] != 0u) { G[2] = 0x08CF34D0u; F[1] = float_at(rt, G[3] + 0x6C4u); F[0] = float_at(rt, G[2] + 0x54u); const bool fast = F[0] < F[1]; F[22] = float_at(rt, G[2] + 0x60u); if (fast) { F[1] = float_at(rt, G[3] + 0x520u); F[0] = float_at(rt, G[2] + 0x5Cu); F[22] = float_at(rt, G[2] + 0x64u); F[20] = float_at(rt, G[2] + 0x6Cu); F[21] = F[1] * F[0]; } else { F[0] = float_at(rt, G[2] + 0x50u); F[21] = 0.0f; const bool mid = F[0] < F[1]; F[20] = F[21]; if (mid) { F[1] = float_at(rt, G[3] + 0x520u); F[0] = float_at(rt, G[2] + 0x58u); F[20] = float_at(rt, G[2] + 0x68u); F[21] = F[1] * F[0]; } } G[4] = G[17]; call_here(rt, ctx, 0x0895549Cu); F[2] = F[22] * F[0]; F[1] = F[20]; if (!(F[20] < F[21])) { F[1] = -F[20]; if (!(F[21] < F[1])) F[1] = F[21]; } F[0] = float_at(rt, G[16] + 4u); if (F[0] < F[1]) { F[0] = F[2] + F[0]; put_float(rt, G[16] + 4u, F[0]); if (F[1] < F[0]) put_float(rt, G[16] + 4u, F[1]); } else { F[0] = F[0] - F[2]; put_float(rt, G[16] + 4u, F[0]); if (F[0] < F[1]) put_float(rt, G[16] + 4u, F[1]); } } else G[2] = 0x08CF0000u; } F[22] = float_at(rt, frame.at(0x18u)); F[21] = float_at(rt, frame.at(0x14u)); F[20] = float_at(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } } }
void func_089C1B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x28u), G[31]); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08A0F560u); G[1] = 0xBF800000u; F[4] = -1.0f; G[3] = G[16] + 0x80u; G[2] = G[17] + 0x80u; G[5] = G[16] + 0x90u; G[4] = G[17] + 0x90u; G[6] = G[16] + 0xB0u; G[7] = G[16] + 0xD0u; if (G[17] != 0u) { put(rt, G[16] + 0x78u, 0u); if (G[3] != G[2]) { F[2] = float_at(rt, G[2] + 0xCu); F[3] = float_at(rt, G[17] + 0x80u); F[0] = float_at(rt, G[2] + 4u); F[1] = float_at(rt, G[2] + 8u); put_float(rt, G[3] + 0xCu, F[2]); put_float(rt, G[3] + 4u, F[0]); put_float(rt, G[3] + 8u, F[1]); put_float(rt, G[16] + 0x80u, F[3]); } if (G[5] != G[4]) { F[2] = float_at(rt, G[4] + 0xCu); F[3] = float_at(rt, G[17] + 0x90u); F[0] = float_at(rt, G[4] + 4u); F[1] = float_at(rt, G[4] + 8u); put_float(rt, G[5] + 0xCu, F[2]); put_float(rt, G[5] + 4u, F[0]); put_float(rt, G[5] + 8u, F[1]); put_float(rt, G[16] + 0x90u, F[3]); } F[0] = float_at(rt, G[17] + 0xA0u); G[2] = frame.at(0x10u); put(rt, frame.at(0x10u), 0u); put_float(rt, G[16] + 0xA0u, F[0]); put(rt, frame.at(0x14u), 0u); put(rt, frame.at(0x18u), 0u); put(rt, frame.at(0x1Cu), 0u); if (G[6] != G[2]) { put(rt, G[6] + 0xCu, 0u); put(rt, G[6] + 4u, 0u); put(rt, G[6] + 8u, 0u); put(rt, G[16] + 0xB0u, 0u); } G[5] = load8(G[17] + 0xCBu); F[1] = float_at(rt, G[17] + 0xC4u); G[2] = load8(G[17] + 0xC8u); G[3] = load8(G[17] + 0xC9u); G[4] = load8(G[17] + 0xCAu); G[1] = 0x3F800000u; F[0] = 1.0f; put_float(rt, G[16] + 0xC0u, F[0]); put_float(rt, G[16] + 0xC4u, F[1]); store8(G[16] + 0xC8u, G[2]); store8(G[16] + 0xC9u, G[3]); store8(G[16] + 0xCAu, G[4]); store8(G[16] + 0xCBu, G[5]); put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); put_float(rt, frame.at(8u), F[4]); put_float(rt, frame.at(0xCu), F[4]); if (G[7] != frame.at(0u)) { put_float(rt, G[7] + 0xCu, F[4]); put(rt, G[16] + 0xD0u, 0u); put(rt, G[7] + 4u, 0u); put_float(rt, G[7] + 8u, F[4]); } } G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } } }
void func_088B32FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); G[18] = G[4]; G[2] = 0x08CC30A0u; G[17] = G[18] + 0x310u; G[16] = G[18] + 0x318u; put(rt, G[18], G[2]); G[2] = 0x08CC30B8u; put(rt, G[18] + 4u, G[2]); G[4] = word(rt, G[18] + 0x318u); if (G[4] != 0u) call_here(rt, ctx, 0x08B61FC0u); put(rt, G[16] + 4u, 0u); put(rt, G[18] + 0x318u, 0u); G[4] = word(rt, G[18] + 0x310u); if (G[4] != 0u) call_here(rt, ctx, 0x08B61FC0u); put(rt, G[17] + 4u, 0u); G[16] = G[18] + 0x2F0u; put(rt, G[18] + 0x310u, 0u); G[4] = word(rt, G[18] + 0x2F0u); if (G[4] != 0u) call_here(rt, ctx, 0x08B61FC0u); put(rt, G[16] + 4u, 0u); G[16] = G[18] + 0x2E4u; put(rt, G[18] + 0x2F0u, 0u); G[4] = word(rt, G[18] + 0x2E4u); if (G[4] != 0u) call_here(rt, ctx, 0x08B61FC0u); put(rt, G[16] + 4u, 0u); G[19] = G[18] + 0x2DCu; G[5] = word(rt, G[18] + 0x2DCu); put(rt, G[18] + 0x2E4u, 0u); if (G[5] != 0u) { G[2] = word(rt, G[5] - 4u); G[2] <<= 10; G[16] = G[5] + G[2]; G[3] = G[16] - 0x400u; G[17] = G[3]; if (G[5] != G[16]) { do { G[2] = word(rt, G[17]); G[16] = G[3]; G[4] = G[3]; G[3] = word(rt, G[2]); G[17] -= 0x400u; call_here(rt, ctx, G[3]); G[2] = word(rt, G[19]); G[3] = G[16] - 0x400u; } while (G[2] != G[16]); } G[4] = G[16] - 4u; call_here(rt, ctx, 0x08B61FC0u); } put(rt, G[19] + 4u, 0u); G[4] = G[18] + 4u; put(rt, G[19], 0u); call_here(rt, ctx, 0x08AF20E8u); G[2] = 0x08CC31A0u; put(rt, G[18], G[2]); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); }  } }
void func_0885FF1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xC0u); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); put(rt, frame.at(0xA0u), G[16]); put(rt, frame.at(0xB4u), G[31]); put(rt, frame.at(0xB0u), G[20]); put(rt, frame.at(0xACu), G[19]); G[18] = G[4]; G[17] = G[5]; G[16] = G[6]; G[2] = word(rt, G[4]); G[4] = word(rt, G[2] + 0x5ACu); call_here(rt, ctx, 0x088DCD28u); if (G[2] != 0u) { G[4] = G[16]; if (G[16] != 0u) { G[5] = G[2] + 8u; call_here(rt, ctx, 0x088A6C18u); } G[2] = word(rt, G[18]); G[5] = G[17]; G[19] = frame.at(0x40u); G[4] = word(rt, G[2] + 0x5ACu); G[16] = G[19]; G[20] = frame.at(0xA0u); call_here(rt, ctx, 0x088D6ECCu); G[4] = word(rt, G[18]); G[3] = G[17] << 6; G[2] = G[17] << 4; G[2] += G[3]; G[2] += 0x4120u; G[17] = G[4] + G[2]; put(rt, G[17] + 0x24u, 0u); G[4] = frame.at(0u); G[5] = 0u; call_here(rt, ctx, 0x088A79ACu); do { G[4] = G[16]; G[16] += 0x30u; call_here(rt, ctx, 0x08897B8Cu); } while (G[20] != G[16]); G[4] = word(rt, G[17] + 0x44u); G[5] = word(rt, G[17] + 0x48u); G[16] = frame.at(8u); G[6] = G[16]; G[7] = frame.at(0u); G[8] = G[19]; call_here(rt, ctx, 0x08866510u); G[2] = 1u; put(rt, frame.at(0xCu), G[2]); G[6] = G[16]; G[7] = frame.at(0u); store8(frame.at(0x12u), 0u); G[8] = 0u; G[9] = G[19]; store8(frame.at(0x13u), 0u); G[16] = G[20]; G[5] = word(rt, G[17] + 0x48u); G[4] = word(rt, G[17] + 0x44u); call_here(rt, ctx, 0x088661B8u); G[16] -= 0x30u; while (true) { G[4] = G[16]; call_here(rt, ctx, 0x088978B4u); const bool more = G[19] != G[16]; G[16] -= 0x30u; if (!more) break; } G[4] = frame.at(0u); call_here(rt, ctx, 0x088A7548u); } G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); G[20] = word(rt, frame.at(0xB0u)); G[19] = word(rt, frame.at(0xACu)); }  } }
void func_088424D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x1Cu), G[19]); G[17] = frame.at(4u); G[21] = G[4]; G[4] = G[17]; G[20] = G[7]; G[18] = G[6]; call_here(rt, ctx, 0x088418B4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9C584u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0884177Cu); G[19] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x0888EB74u); G[17] = G[2]; if (static_cast<std::int32_t>(G[18]) > 0) { G[16] = word(rt, G[20]); if (G[16] == 0u) { G[4] = 0x08C9C520u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[17] = G[2]; } G[2] = static_cast<std::int32_t>(G[18]) < 2 ? 1u : 0u; G[6] = 0u; if (G[2] == 0u) { G[16] = word(rt, G[20] + 4u); if (G[16] == 0u) { G[4] = 0x08C9C520u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; } G[4] = G[19]; G[5] = G[17]; call_here(rt, ctx, 0x0888EBF4u); G[5] = G[2]; if (G[2] != 0u) { G[4] = frame.at(0u); call_here(rt, ctx, 0x0884F674u); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); }  } }
void func_08830A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[16] = frame.at(4u); G[19] = G[4]; G[4] = G[16]; G[20] = frame.at(8u); call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[2] = word(rt, frame.at(4u)); if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B0980Cu); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08B089D4u); G[4] = G[2]; G[18] = G[2]; call_here(rt, ctx, 0x08B2B5E0u); G[4] = G[2]; G[6] = G[18]; G[5] = 0u; G[17] = G[2]; call_here(rt, ctx, 0x08B36018u); G[3] = word(rt, G[16]); G[4] = G[17]; G[6] = G[18]; G[5] = G[3]; if (G[3] != 0u) { call_here(rt, ctx, 0x08B79A88u); G[4] = G[20]; G[5] = G[17]; G[6] = G[18]; G[7] = 1u; call_here(rt, ctx, 0x08A44D9Cu); G[4] = G[19]; if (G[19] != G[20]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } else { call_here(rt, ctx, 0x08B35254u); G[4] = frame.at(0u); G[5] = 0x08CF6ED8u; call_here(rt, ctx, 0x08A76AACu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_0880FCD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[17] = G[4]; G[18] = G[17] + 0xE8u; call_here(rt, ctx, 0x0896BDF8u); G[2] = 0x08CB97A8u; put(rt, G[17], G[2]); G[2] = 0x08CB99E0u; G[4] = 0x2E0u; put(rt, G[17] + 0x38u, G[2]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[2] += 0x230u; put(rt, G[16], 0u); G[5] = G[16] + 0x23Cu; G[3] = 0xFFFFFFFFu; put(rt, G[16] + 4u, 0u); G[1] = 0x3F800000u; F[0] = 1.0f; G[4] = G[16] + 0x2D8u; store8(G[16] + 8u, 0u); store8(G[16] + 9u, 0u); put(rt, G[16] + 0x10u, 0u); put(rt, G[2] + 8u, 0u); put(rt, G[2] + 4u, 0u); G[2] = 0x005B8D7Fu; put(rt, G[16] + 0x230u, 0u); put(rt, G[5] + 8u, 0u); put(rt, G[5] + 4u, 0u); put(rt, G[16] + 0x26Cu, G[3]); G[3] = 4u; put(rt, G[16] + 0x290u, G[2]); store16(G[16] + 0x2CAu, G[3]); put_float(rt, G[16] + 0x2D4u, F[0]); put(rt, G[16] + 0x23Cu, 0u); put(rt, G[16] + 0x270u, 0u); put(rt, G[16] + 0x274u, 0u); put(rt, G[16] + 0x278u, 0u); put(rt, G[16] + 0x294u, 0u); store16(G[16] + 0x2C8u, 0u); call_here(rt, ctx, 0x08A76AA0u); put(rt, G[17] + 0xE8u, G[16]); G[2] = 0x08D70000u; G[16] = 0x08D75108u; G[3] = sext(load8(0x08D75108u), 8); G[17] = 0x08D75110u; G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[5] = 0x08D70000u; G[17] = 0x08D75110u; G[4] = G[16]; if (G[2] != 0u) { G[2] = 0x08CBCFE8u; put(rt, 0x08D75110u, G[2]); call_here(rt, ctx, 0x08B64494u); } } put(rt, G[18] + 4u, G[17]); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_290
} // namespace gt::game

#define HOT290(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_290::name, gt::ReturnKind::kind)
HOT290(0x088B67DCu, func_088B67DC, Word);
HOT290(0x088DF3CCu, func_088DF3CC, Word);
HOT290(0x088F6AE4u, func_088F6AE4, Word);
HOT290(0x08958974u, func_08958974, Word);
HOT290(0x089C1B68u, func_089C1B68, Word);
HOT290(0x088B32FCu, func_088B32FC, Word);
HOT290(0x0885FF1Cu, func_0885FF1C, Word);
// HOT290(0x088424D8u, func_088424D8, Word);  // no comparable synthetic case yet
// HOT290(0x08830A78u, func_08830A78, Word);  // no comparable synthetic case yet
HOT290(0x0880FCD8u, func_0880FCD8, Word);
