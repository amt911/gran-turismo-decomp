// Hot batch 288: seventy-four-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (a wrapped index search in both axes, argument adapters and an event list walk).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_288 {
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

void func_0893769C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x120u); put(rt, frame.at(0x108u), G[18]); put(rt, frame.at(0x104u), G[17]); put(rt, frame.at(0x114u), G[31]); put(rt, frame.at(0x110u), G[20]); put(rt, frame.at(0x10Cu), G[19]); put(rt, frame.at(0x100u), G[16]); G[18] = G[4]; G[4] = frame.at(0u); G[17] = G[5]; G[5] = G[18] + 0xCu; G[20] = 0x08CA0000u; call_here(rt, ctx, 0x08B7BB6Cu); G[3] = G[17] << 2; G[2] = 0x08CDDE40u; G[2] = G[3] + G[2]; G[5] = word(rt, G[2]); G[4] = frame.at(0u); G[19] = G[3] + G[18]; call_here(rt, ctx, 0x08B7B9ACu); G[4] = 0x64u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08938554u); G[5] = G[17]; G[4] = G[18]; put(rt, G[19] + 0x13Cu, G[16]); call_here(rt, ctx, 0x08936498u); G[6] = 0x08CA21C8u; G[4] = G[18]; G[5] = frame.at(0u); if (G[2] != 0u) { call_here(rt, ctx, 0x089372C8u); G[5] = G[2]; G[4] = 0x10u; G[16] = G[2]; call_here(rt, ctx, 0x08937AFCu); G[3] = word(rt, G[18] + 8u); G[17] = G[2]; G[2] = 0x08CDDE0Cu; G[3] <<= 2; G[3] += G[2]; G[5] = word(rt, G[3]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B7B9ACu); G[4] = frame.at(0u); G[5] = 0x08CA21C8u; call_here(rt, ctx, 0x08B7B9ACu); G[4] = word(rt, G[19] + 0x13Cu); G[6] = G[17]; G[7] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08938B0Cu); } else { G[5] = 0x08CA21C8u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B7B9ACu); G[4] = word(rt, G[19] + 0x13Cu); G[5] = frame.at(0u); call_here(rt, ctx, 0x08938BECu); } G[18] = word(rt, frame.at(0x108u)); G[17] = word(rt, frame.at(0x104u)); G[20] = word(rt, frame.at(0x110u)); G[19] = word(rt, frame.at(0x10Cu)); G[16] = word(rt, frame.at(0x100u)); }  } }
// 0x089E7E4C (a0 = the object, a1 = a count (s3); v0 = 1 when an entry was taken over, else 0): s4 = 0x089E4EA0(object) and s0 = 0x089E4EC8(object) first.
// The loop walks s0 downwards: while it is not above -1 (signed) and the count is positive, s0 loses one; a negative s0 either restarts at -1 or, with bit 1 of
// the object's +0x150 set, at the object's word +0x158 minus one (a negative one restarts the loop). Otherwise 0x089E4DE0(object, s4, s0) gives an entry which
// 0x089E6C80(object, entry) filters (nonzero continues) and 0x089E7520(object, entry) accepts (0 continues); an accepted one drops the count by one, and when
// 0x089E4EC8(object) answers that same s0 the walk restarts at -1. With the count spent the last s0, when valid, goes through 0x089E4DE0 and 0x089E7C4C(object, entry).
void func_089E7E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}, {18u, 0x8u}});
    u32 remaining = ctx.gpr[5];
    ctx.gpr[19] = remaining;
    ctx.gpr[17] = self;
    const u32 span = call_set(rt, ctx, 0x089E4EA0u, {self});
    ctx.gpr[20] = span;
    u32 index = call_set(rt, ctx, 0x089E4EC8u, {self});
    ctx.gpr[16] = index;
    u32 result = 0u;
    for (;;) {
        const bool valid = static_cast<s32>(index) >= 0;
        if (static_cast<s32>(remaining) <= 0) {
            if (valid) {
                const u32 entry = call_set(rt, ctx, 0x089E4DE0u, {self, span, index});
                call_set(rt, ctx, 0x089E7C4Cu, {self, entry});
                result = 1u;
            }
            break;
        }
        index -= 1u;
        ctx.gpr[16] = index;
        if (!valid) break;
        if (static_cast<s32>(index) < 0) {
            if ((word(rt, self + 0x150u) & 2u) == 0u) {
                index = 0xFFFFFFFFu;
                ctx.gpr[16] = index;
                continue;
            }
            index = word(rt, self + 0x158u) - 1u;
            ctx.gpr[16] = index;
            if (static_cast<s32>(index) < 0) continue;
        }
        const u32 entry = call_set(rt, ctx, 0x089E4DE0u, {self, span, index});
        ctx.gpr[18] = entry;
        if (call_set(rt, ctx, 0x089E6C80u, {self, entry}) != 0u) continue;
        if (call_set(rt, ctx, 0x089E7520u, {self, entry}) == 0u) continue;
        remaining -= 1u;
        ctx.gpr[19] = remaining;
        const u32 now = call_set(rt, ctx, 0x089E4EC8u, {self});
        if (now == index) {
            index = 0xFFFFFFFFu;
            ctx.gpr[16] = index;
        }
    }
    ctx.gpr[2] = result;
}
// 0x089E80A4 (a0 = the object, a1 = a count (s3); v0 = 1 when an entry was taken over, else 0): s0 = 0x089E4EA0(object) and s4 = 0x089E4EC8(object) first.
// The loop walks s0 downwards: while it is not above -1 (signed) and the count is positive, s0 loses one; a negative s0 either restarts at -1 or, with bit 1 of
// the object's +0x150 set, at the object's word +0x154 minus one (a negative one restarts the loop). Otherwise 0x089E4DE0(object, s0, s4) gives an entry which
// 0x089E6C80(object, entry) filters (nonzero continues) and 0x089E7520(object, entry) accepts (0 continues); an accepted one drops the count by one, and when
// 0x089E4EA0(object) answers that same s0 the walk restarts at -1. With the count spent the last s0, when valid, goes through 0x089E4DE0 and 0x089E7C4C(object, entry).
void func_089E80A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}, {18u, 0x8u}});
    u32 remaining = ctx.gpr[5];
    ctx.gpr[19] = remaining;
    ctx.gpr[17] = self;
    u32 index = call_set(rt, ctx, 0x089E4EA0u, {self});
    ctx.gpr[16] = index;
    const u32 span = call_set(rt, ctx, 0x089E4EC8u, {self});
    ctx.gpr[20] = span;
    u32 result = 0u;
    for (;;) {
        const bool valid = static_cast<s32>(index) >= 0;
        if (static_cast<s32>(remaining) <= 0) {
            if (valid) {
                const u32 entry = call_set(rt, ctx, 0x089E4DE0u, {self, index, span});
                call_set(rt, ctx, 0x089E7C4Cu, {self, entry});
                result = 1u;
            }
            break;
        }
        index -= 1u;
        ctx.gpr[16] = index;
        if (!valid) break;
        if (static_cast<s32>(index) < 0) {
            if ((word(rt, self + 0x150u) & 2u) == 0u) {
                index = 0xFFFFFFFFu;
                ctx.gpr[16] = index;
                continue;
            }
            index = word(rt, self + 0x154u) - 1u;
            ctx.gpr[16] = index;
            if (static_cast<s32>(index) < 0) continue;
        }
        const u32 entry = call_set(rt, ctx, 0x089E4DE0u, {self, index, span});
        ctx.gpr[18] = entry;
        if (call_set(rt, ctx, 0x089E6C80u, {self, entry}) != 0u) continue;
        if (call_set(rt, ctx, 0x089E7520u, {self, entry}) == 0u) continue;
        remaining -= 1u;
        ctx.gpr[19] = remaining;
        const u32 now = call_set(rt, ctx, 0x089E4EA0u, {self});
        if (now == index) {
            index = 0xFFFFFFFFu;
            ctx.gpr[16] = index;
        }
    }
    ctx.gpr[2] = result;
}
void func_08A2ADB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[31]); G[19] = frame.at(0x10u); G[18] = G[4]; G[4] = G[19]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08ACC220u); G[2] = 1u; G[4] = G[19]; bool have = false; if (G[16] == G[2]) { G[5] = word(rt, G[17]); have = G[5] != 0u; } if (have) { G[3] = word(rt, G[5]); G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(0xCu); call_here(rt, ctx, G[2]); G[5] = word(rt, frame.at(0xCu)); G[4] = G[19]; call_here(rt, ctx, 0x08ACBEB0u); G[4] = word(rt, frame.at(0xCu)); G[16] = G[2]; G[2] = 0x08D74FD0u; G[17] = G[4] - 0xCu; G[4] -= 4u; if (G[17] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } else { G[5] = 0u; call_here(rt, ctx, 0x08ACBEB0u); G[16] = G[2]; } G[4] = G[19]; if (G[16] != 0u) { G[16] = frame.at(4u); call_here(rt, ctx, 0x08A2AA84u); G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x08A97A6Cu); G[5] = word(rt, frame.at(4u)); } else { G[16] = frame.at(8u); G[4] = G[16]; G[5] = 0u; call_here(rt, ctx, 0x08A97A6Cu); G[5] = word(rt, frame.at(8u)); } if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08ACC0F4u); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08A2C754(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x30u), G[16]); G[5] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; G[18] = G[4]; G[17] = G[6]; put_float(rt, frame.at(0x40u), F[20]); if (G[5] == 0u) { G[16] = word(rt, G[6]); if (G[16] == 0u) { G[4] = 0x08CAC140u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); if (G[16] == 0u) { G[4] = 0x08CAC140u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[4] = G[16]; G[16] = frame.at(0xCu); G[3] = word(rt, G[2] + 0x54u); G[17] = frame.at(4u); call_here(rt, ctx, G[3]); F[20] = F[0]; G[5] = word(rt, frame.at(8u)); G[4] = G[16]; call_here(rt, ctx, 0x08C1A1E4u); F[12] = F[20]; G[4] = G[16]; call_here(rt, ctx, 0x08C1A168u); G[4] = G[16]; call_here(rt, ctx, 0x08C1A13Cu); F[12] = F[0]; G[4] = G[17]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(4u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x08C1A2C4u); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } F[20] = float_at(rt, frame.at(0x40u)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } } }
void func_08A2C87C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x30u), G[16]); G[5] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; G[19] = G[4]; G[17] = G[6]; if (G[5] == 0u) { G[16] = word(rt, G[6]); if (G[16] == 0u) { G[4] = 0x08CAC140u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); if (G[16] == 0u) { G[4] = 0x08CAC140u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[17] = frame.at(0xCu); G[2] = word(rt, G[3] + 0x3Cu); G[18] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = word(rt, frame.at(8u)); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08C19C20u); G[5] = G[16] != 0u ? 1u : 0u; G[4] = G[17]; call_here(rt, ctx, 0x08C1A114u); G[4] = G[17]; call_here(rt, ctx, 0x08C19BECu); G[5] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(4u)); if (G[19] != G[18]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08C19C60u); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[19] = word(rt, frame.at(0x3Cu)); G[17] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x30u)); }  } }
void func_08A38418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x20u), G[16]); G[18] = G[5]; G[17] = G[4]; if (static_cast<std::int32_t>(G[6]) > 0) { G[19] = frame.at(0xCu); G[5] = G[7]; G[4] = G[19]; call_here(rt, ctx, 0x08A85940u); G[16] = word(rt, frame.at(0xCu)); if (G[16] == 0u) { G[4] = 0x08CAD294u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[20] = frame.at(8u); call_here(rt, ctx, G[2]); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[18]; G[4] = G[20]; call_here(rt, ctx, 0x08A38038u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08CAD29Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16] + 0x14u; G[5] = G[17]; call_here(rt, ctx, 0x08B1ED7Cu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x08A38038u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CAD29Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16] + 0x9D8u); G[4] = G[18]; put(rt, frame.at(0x10u), G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); G[5] = word(rt, frame.at(0x10u)); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08A8D1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x2Cu), G[31]); G[17] = G[4]; G[4] = frame.at(0u); G[22] = frame.at(8u); G[21] = frame.at(0xCu); G[20] = G[5]; G[19] = G[6]; G[18] = frame.at(4u); call_here(rt, ctx, 0x08A9DD14u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CB1D6Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A9BC30u); G[5] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A7C0F0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; G[5] = 0x08CF6F04u; G[6] = G[18]; call_here(rt, ctx, 0x08A7B544u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CB1D74u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x28u); G[5] = G[17]; call_here(rt, ctx, G[2]); G[5] = G[17]; G[4] = G[21]; G[6] = G[19]; call_here(rt, ctx, 0x08A8CE2Cu); G[2] = word(rt, frame.at(0xCu)); G[5] = G[20]; G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x1Cu); call_here(rt, ctx, G[3]); } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, frame.at(0x14u)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08AE8204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[16] = G[4]; G[2] = word(rt, G[4] + 0x10u); G[17] = word(rt, G[5] + 8u); G[21] = word(rt, G[5] + 4u); G[22] = sext(load16(G[2] + 0x12u), 16); G[4] = G[22]; call_here(rt, ctx, 0x08ADC1CCu); if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x10u); G[5] = G[17]; G[17] = 0u; G[4] = sext(load16(G[2] + 0x12u), 16); call_here(rt, ctx, 0x08ADC094u); G[3] = word(rt, G[16] + 0x1Cu); G[20] = G[2]; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[17] = G[3]; while (G[17] != 0u) { G[2] = word(rt, G[17] + 8u); if (G[2] == G[21]) { G[3] = load8(G[17] + 0x36u); G[2] = sext(G[3], 8); G[18] = 0u; if (static_cast<std::int32_t>(G[2]) > 0) { G[19] = G[20] >> 16; G[16] = G[17] + 0x38u; do { G[2] = sext(load16(G[16]), 16); G[18] += 1u; G[16] += 2u; G[4] = G[2]; G[5] = 7u; G[6] = G[19] & 0xFFFFu; G[7] = G[20] & 0xFFFFu; if (static_cast<std::int32_t>(G[2]) >= 0) call_here(rt, ctx, 0x08AEF874u); G[3] = load8(G[17] + 0x36u); G[2] = sext(G[3], 8); G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; } while (G[2] != 0u); } } G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[17] = G[3]; } G[4] = G[22]; call_here(rt, ctx, 0x08ADC230u); } G[16] = word(rt, frame.at(0x0u)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_288
} // namespace gt::game

#define HOT288(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_288::name, gt::ReturnKind::kind)
// HOT288(0x0893769Cu, func_0893769C, Word);  // no comparable synthetic case yet
HOT288(0x089E7E4Cu, func_089E7E4C, Word);
HOT288(0x089E80A4u, func_089E80A4, Word);
// HOT288(0x08A2ADB4u, func_08A2ADB4, Word);  // no comparable synthetic case yet
HOT288(0x08A2C754u, func_08A2C754, Word);
HOT288(0x08A2C87Cu, func_08A2C87C, Word);
// HOT288(0x08A38418u, func_08A38418, Word);  // no comparable synthetic case yet
// HOT288(0x08A8D1E0u, func_08A8D1E0, Word);  // no comparable synthetic case yet
HOT288(0x08AE8204u, func_08AE8204, Word);
