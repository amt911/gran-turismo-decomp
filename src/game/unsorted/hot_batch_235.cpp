// Hot batch 235: fifty-five-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (handle-guarded calls through temporaries, destructors, scaled-ratio tables, threshold setters and
// state polls).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_235 {
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
    if (!(value >= -2357483648.0f && value < 2357483648.0f)) return 0x7FFFFFFF;
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

void func_088A5B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[3] = word(rt, G[4]); G[1] = 0x3F800000u; ctx.fpr[4] = std::bit_cast<float>(G[1]); G[2] = word(rt, G[3] + 0x4C8u); G[6] = word(rt, G[2] + 0x1C30u); G[7] = G[2] + 0x1C30u; if (static_cast<std::int32_t>(G[6]) > 0) { G[2] = G[7]; G[4] = 1u; G[3] = load16(G[2] + 0x1Au); for (;;) { G[4] += 1u; G[2] += 2u; ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[3])); const bool larger = ctx.fpr[4] < ctx.fpr[0]; G[3] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; if (larger) ctx.fpr[4] = ctx.fpr[0]; if (G[3] != 0u) break; G[3] = load16(G[2] + 0x1Au); } } G[2] = G[6] << 1; put(rt, G[5], G[2]); G[3] = word(rt, G[7]); G[4] = G[7]; if (static_cast<std::int32_t>(G[3]) > 0) { G[3] = G[7]; G[6] = 0u; G[1] = 0x42C80000u; ctx.fpr[3] = std::bit_cast<float>(G[1]); do { ctx.fpr[0] = float_at(rt, G[4] + 0x30u); G[6] += 1u; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[3]; put_float(rt, G[5] + 4u, ctx.fpr[0]); G[2] = load16(G[3] + 6u); ctx.fpr[1] = std::bit_cast<float>(G[2]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[2])); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[4]; put_float(rt, G[5] + 8u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[4] + 0x58u); G[4] += 4u; ctx.fpr[1] = ctx.fpr[1] / ctx.fpr[3]; put_float(rt, G[5] + 0xCu, ctx.fpr[1]); G[2] = load16(G[3] + 0x1Au); G[3] += 2u; ctx.fpr[2] = std::bit_cast<float>(G[2]); ctx.fpr[0] = static_cast<float>(static_cast<std::int32_t>(G[2])); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[4]; put_float(rt, G[5] + 0x10u, ctx.fpr[0]); G[2] = word(rt, G[7]); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] += 0x10u; } while (G[2] != 0u); } } }
void func_088CF5F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[1] = 0x42C80000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[12] = psp_mul(ctx.fpr[12], ctx.fpr[0]); { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; ctx.fpr[12] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[12]))); put_float(rt, frame.at(0x14u), ctx.fpr[21]); ctx.fpr[21] = ctx.fpr[14]; put(rt, frame.at(4u), G[31]); G[3] = bits_of(ctx.fpr[12]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); G[4] = word(rt, G[4] + 0xC0u); G[2] = static_cast<std::int32_t>(G[4]) < 0x7530 ? 1u : 0u; put_float(rt, G[16] + 0xBCu, ctx.fpr[12]); if (G[2] != 0u) { G[2] = load8(G[16] + 0x48u); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) { G[2] = load8(G[16] + 0x49u); G[2] = G[4] + G[2]; put(rt, G[16] + 0xC0u, G[2]); } else { G[2] = load8(G[16] + 0x46u); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) { G[2] = load8(G[16] + 0x47u); G[2] = G[4] + G[2]; put(rt, G[16] + 0xC0u, G[2]); } else { G[2] = load8(G[16] + 0x44u); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] = G[4] + 0xAu; if (G[2] != 0u) { put(rt, G[16] + 0xC0u, G[5]); } else { G[2] = load8(G[16] + 0x45u); G[2] = G[4] + G[2]; put(rt, G[16] + 0xC0u, G[2]); } } } } ctx.fpr[12] = ctx.fpr[13]; ctx.fpr[20] = float_at(rt, G[16] + 0xC4u); call_here(rt, ctx, 0x08B499B8u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[21]); G[1] = 0x3C88AB7Cu; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[20] = ctx.fpr[20] + ctx.fpr[0]; put_float(rt, G[16] + 0xC4u, ctx.fpr[20]); G[16] = word(rt, frame.at(0u)); ctx.fpr[21] = float_at(rt, frame.at(0x14u)); ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } } }
void func_08936ED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CC67B8u; put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put(rt, frame.at(8u), G[18]); G[18] = 0x2Cu; put(rt, frame.at(4u), G[17]); G[17] = 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x10u), G[31]); put(rt, G[4], G[2]); G[2] = word(rt, G[16] + 0x8Cu); for (;;) { G[17] += 1u; G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } G[2] = word(rt, G[16] + 0x13Cu); G[16] += 4u; G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } if (G[17] == G[18]) break; G[2] = word(rt, G[16] + 0x8Cu); } G[4] = word(rt, G[19] + 0x1ECu); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = word(rt, G[19] + 0x1F0u); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = G[19] + 0x2D0u; call_here(rt, ctx, 0x08938FCCu); G[4] = G[19] + 0x1F4u; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08938FCCu); } }
// 0x08A082A8 (a2 = a count, a3 = the object): when the count is positive, 0x08A08228(frame) prepares a temporary. With
// the count at least 2 the object's word at +4 (0x08A5BB7C(0x08CA90B8) when null) is called through its slot 0x3C and the
// result kept as the second argument; otherwise -1 is. The temporary's word (0x08A5BB7C(0x08CA90C0) when null) is the
// first argument and the object's word at +0 (0x08A5BB7C(0x08CA90B8) when null) is called through its slot 0x54; its
// float result goes to f12 and the temporary's vtable slot 0x1B8 is called with the temporary, the second argument and
// f12. The temporary is released by 0x08A5B9EC; its result is returned.
void func_08A082A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 count = ctx.gpr[6];
    const u32 object = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {16u, 0x10u}, {31u, 0x20u}, {19u, 0x1Cu}, {17u, 0x14u}});
    ctx.gpr[18] = object;
    ctx.gpr[16] = count;
    if (static_cast<s32>(count) <= 0) return;
    call_set(rt, ctx, 0x08A08228u, {frame.at(0u), ctx.gpr[5]});
    u32 argument = 0xFFFFFFFFu;
    ctx.gpr[19] = argument;
    if (static_cast<s32>(count) >= 2) {
        const u32 inner = word(rt, object + 4u);
        ctx.gpr[16] = inner;
        if (inner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA90B8u});
        argument = call_set(rt, ctx, word(rt, word(rt, inner) + 0x3Cu), {inner});
        ctx.gpr[19] = argument;
    }
    const u32 held = word(rt, frame.at(0u));
    ctx.gpr[17] = held;
    if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA90C0u});
    const u32 target_object = word(rt, object);
    ctx.gpr[16] = target_object;
    const u32 target = word(rt, word(rt, held) + 0x1B8u);
    ctx.gpr[18] = target;
    if (target_object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA90B8u});
    call_set(rt, ctx, word(rt, word(rt, target_object) + 0x54u), {target_object});
    ctx.fpr[12] = ctx.fpr[0];
    call_set(rt, ctx, target, {held, argument});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_08A084D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; G[4] = G[20]; put(rt, frame.at(0x10u), G[16]); G[16] = G[6]; put(rt, frame.at(0x18u), G[18]); G[18] = 0xFFFFFFFFu; put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[17] = G[7]; call_here(rt, ctx, 0x08A08228u); { const bool none = static_cast<std::int32_t>(G[16]) <= 0; G[16] = word(rt, frame.at(4u)); if (!none) { G[16] = word(rt, G[17]); G[4] = 0x08CA90B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; G[16] = word(rt, frame.at(4u)); } } G[4] = 0x08CA90C0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x08A06E94u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A042F4u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A21928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = G[19]; put(rt, frame.at(0x10u), G[16]); G[16] = G[6]; put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); G[17] = G[7]; call_here(rt, ctx, 0x08A21818u); { const bool none = static_cast<std::int32_t>(G[16]) <= 0; G[16] = word(rt, frame.at(4u)); if (!none) { G[18] = word(rt, frame.at(4u)); if (G[18] == 0u) { G[4] = 0x08CAA91Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08CAA8B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[18] + 0x1Cu, ctx.fpr[0]); goto release; } } if (G[16] == 0u) { G[4] = 0x08CAA91Cu; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(0u)); if (G[18] != frame.at(0u)) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); release: G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A41F4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = G[20]; put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(8u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); G[17] = frame.at(0xCu); call_here(rt, ctx, 0x08A40E90u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CADBFCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16] + 0x44u; G[4] = G[19]; call_here(rt, ctx, 0x08B88F44u); G[4] = G[17]; G[5] = G[19]; call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A4F3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); G[18] = G[6] & 0xFFu; G[6] = G[18]; put(rt, frame.at(0x24u), G[17]); G[17] = G[4]; put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x2Cu), G[31]); G[16] = G[5]; call_here(rt, ctx, 0x08A4EE1Cu); if (G[2] != 0u) { G[2] = word(rt, G[17] + 0x58u) & 0x4000u; G[4] = 0x2Eu; if (G[2] != 0u) { call_here(rt, ctx, 0x08A4F5C0u); G[7] = word(rt, frame.at(0x10u)); store16(frame.at(0x14u), G[3]); G[3] = G[2] & 0xFFFFu; G[7] = (G[7] & 0xFFFF0000u) | G[3]; G[2] >>= 16; G[7] = (G[7] & 0xFFFFu) | (G[2] << 16); G[2] = load16(frame.at(0x14u)); G[4] = G[17]; G[5] = frame.at(0u); G[6] = G[18]; put(rt, frame.at(0u), G[7]); put(rt, frame.at(0x10u), G[7]); store16(frame.at(4u), G[2]); call_here(rt, ctx, 0x08A4EE1Cu); G[4] = G[17]; G[5] = frame.at(0u); G[6] = G[18]; call_here(rt, ctx, 0x08A4EE1Cu); G[4] = G[17]; G[5] = frame.at(0u); G[6] = G[18]; call_here(rt, ctx, 0x08A4EE1Cu); G[4] = 1u; goto done; } } G[4] = 0u; G[2] = 0u; for (std::uint32_t i = 0u; i < 4u; ++i) G[2] |= static_cast<std::uint32_t>(load8(G[16] + i)) << (i * 8u); put(rt, G[17] + 0x90u, G[2]); G[3] = load16(G[16] + 4u); store16(G[17] + 0x94u, G[3]); done: G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); G[2] = G[4]; } } }
// Dispatches the stream-position value of mode 1: fetches the holder from 0x08A57854 into the first slot,
// asserts it and the argument slot's object, runs that object's virtual method at vtable + 0x5C with the frame
// and the object, copies the two values through 0x08A56630 and drops the temporaries. Any other mode returns 1.
// Dispatches the stream-position value of mode 1: fetches the holder from 0x08A57854 into the first slot,
// asserts it and the argument slot's object, runs that object's virtual method at vtable + 0x5C with the frame
// and the object, copies the two values through 0x08A56630 and drops the temporaries. Any other mode returns 1.
void func_08A57BD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], mode = ctx.gpr[6], argument_slot = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x20u}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x24u}, {19u, 0x1Cu}});
    const u32 first = frame.at(0x4u), second = frame.at(0x8u), third = frame.at(0u);
    ctx.gpr[2] = 1u;
    ctx.gpr[20] = first;
    ctx.gpr[18] = second;
    ctx.gpr[17] = object;
    ctx.gpr[16] = argument_slot;
    if (mode != 1u) return;
    call_set(rt, ctx, 0x08A57854u, {first, ctx.gpr[5]});
    const u32 holder = word(rt, first);
    ctx.gpr[19] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF7ECu});
    const u32 node = word(rt, ctx.gpr[16]);
    ctx.gpr[16] = node;
    if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF778u});
    call_set(rt, ctx, word(rt, word(rt, node) + 0x5Cu), {third, node});
    call_set(rt, ctx, 0x08A56630u, {second, holder, third});
    if (object != second) call_set(rt, ctx, 0x08A5BAD8u, {object, word(rt, second)});
    call_set(rt, ctx, 0x08A5B9ECu, {second});
    call_set(rt, ctx, 0x08A69D68u, {third});
    call_set(rt, ctx, 0x08A5B9ECu, {first});
}
// Builds a 0xC-byte node holding the argument, stamps two vtable words on it (0x08CD1F30 first, then 0x08CD4168
// after the +4 member is initialised), creates its +8 member through 0x08A62AB8 (asserting it is non-null), fills
// it with the two remaining arguments through 0x08A641F4 and registers the node with the object through
// 0x08A851CC. The holder at frame + 4 is released; returns the object.
void func_08A80840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], first = ctx.gpr[5], second = ctx.gpr[6], third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {21u, 0x24u}, {31u, 0x28u}, {20u, 0x20u}, {19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}});
    const u32 holder = frame.at(0x4u), argument_slot = frame.at(0u);
    ctx.gpr[18] = holder;
    ctx.gpr[21] = object;
    put(rt, argument_slot, first);
    call_set(rt, ctx, 0x08A76AACu, {holder, argument_slot});
    const u32 node = call_set(rt, ctx, 0x08B62344u, {0xCu});
    ctx.gpr[16] = node;
    put(rt, node, 0x08CD1F30u);
    call_set(rt, ctx, 0x08A76AA0u, {node + 0x4u});
    put(rt, node, 0x08CD4168u);
    call_set(rt, ctx, 0x08A62AB8u, {node + 0x8u});
    const u32 member = word(rt, node + 0x8u);
    ctx.gpr[17] = member;
    if (member == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB14E0u});
    call_set(rt, ctx, 0x08A641F4u, {member, holder, second, third});
    call_set(rt, ctx, 0x08A851CCu, {object, node});
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[2] = object;
}
// Destructor of a container: stamps the vtable at 0x08CB6C98, releases its +0x30 handle, then for the two
// word buffers at +0x18 and +0x24 notifies their ranges through 0x08C2AE8C and frees each one (0x08A5BC54
// with the byte size rounded down to a multiple of four); finally drops the +0x14 handle and runs the base
// destructor at 0x08A780B0.
void func_08A90A3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    const u32 second_block = object + 0x18u, third_block = object + 0x24u;
    ctx.gpr[17] = object;
    ctx.gpr[18] = second_block;
    ctx.gpr[16] = third_block;
    put(rt, object, 0x08CB6C98u);
    call_set(rt, ctx, 0x08A90888u, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {object + 0x30u});
    // a word buffer at `block`: tell the runtime its range, then free it (the size a multiple of four bytes)
    auto drop_buffer = [&](u32 block, u32 byte_slot) {
        call_set(rt, ctx, 0x08C2AE8Cu, {word(rt, block), word(rt, block + 0x4u), byte_slot});
        const u32 begin = word(rt, block);
        if (begin != 0u) {
            const u32 end = word(rt, block + 0x8u);
            call_set(rt, ctx, 0x08A5BC54u, {begin, sra(end - begin, 2) << 2, 0x4u});
        }
    };
    drop_buffer(third_block, frame.at(0x1u));
    drop_buffer(second_block, frame.at(0u));
    const u32 handle = word(rt, object + 0x14u);
    if (handle != 0u) call_set(rt, ctx, 0x08A5B930u, {handle});
    call_set(rt, ctx, 0x08A780B0u, {object});
}
void func_08ACBACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[18]); G[18] = G[4] + 0xA8u; put(rt, frame.at(4u), G[17]); G[17] = G[4]; G[4] = G[18]; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B34888u); call_here(rt, ctx, 0x08C7B198u); G[16] = G[2]; G[2] = load8(G[17] + 0xA5u); if (G[2] != 0u) { G[2] = G[16] - 1u; const bool known = G[2] < 4u; G[2] = 2u; if (!known) { G[4] = G[17] + 0x60u; call_here(rt, ctx, 0x08C7B188u); const bool failed = static_cast<std::int32_t>(G[2]) < 0; G[2] = 1u; if (!failed) store8(G[17] + 0xA4u, 1u); store8(G[17] + 0xA5u, 0u); G[4] = G[18]; goto done; } } else { G[2] = 2u; } G[3] = 0u; if (G[16] == G[2]) { G[4] = 1u; call_here(rt, ctx, 0x08C7B1B0u); G[3] = G[2]; } G[2] = 3u; if (G[16] == G[2] || static_cast<std::int32_t>(G[3]) < 0) { call_here(rt, ctx, 0x08C7B1E8u); G[4] = G[18]; if (static_cast<std::int32_t>(G[2]) < 0) goto done; } G[4] = G[18]; if (G[16] != 0u) goto done; G[2] = load8(G[17] + 0xA4u); if (G[2] == 0u) goto done; store8(G[17] + 0xA4u, 0u); G[4] = G[18]; call_here(rt, ctx, 0x08B34CA0u); G[4] = G[18]; done: G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08B34B24u); } }
// 0x08AFAE68 (a0 = the object, a1/a2 = two words kept at +4 / +8, a3 = a word for +0xC, t0 = a count, t1 = a word stored on the frame): the object's
// virtual +0x14 runs first with the registers as they stand; +0x28 takes t0 — or 0 when t0 is below 2 (movn) — and +0xC takes a3. Then
// 0x08B52B70(object +0x2C, a1, a2, 0, a3, t0, 1, 0, stack = t1) starts a query whose flag, when set, makes the object keep a1 at +4 and a2 at +8 and
// run its virtual +0x34(object, 0, 0, a1, a2). v0 = the flag (0 / 1).
void func_08AFAE68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], arg1 = ctx.gpr[5], arg2 = ctx.gpr[6], arg3 = ctx.gpr[7];
    const u32 count = ctx.gpr[8], arg5 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[21] = arg1;
    ctx.gpr[20] = arg2;
    ctx.gpr[19] = self;
    ctx.gpr[18] = arg5;
    ctx.gpr[17] = arg3;
    ctx.gpr[16] = count;
    call_set(rt, ctx, word(rt, word(rt, self) + 0x14u), {self, arg1, arg2, arg3, count, arg5});
    put(rt, self + 0x28u, static_cast<s32>(count) < 2 ? 0u : count);
    put(rt, self + 0xCu, arg3);
    put(rt, frame.at(0x0u), arg5);
    const u32 started = call_set(rt, ctx, 0x08B52B70u,
        {self + 0x2Cu, arg1, arg2, 0u, arg3, count, 1u, 0u}) != 0u ? 1u : 0u;
    ctx.gpr[16] = started;
    if (started != 0u) {
        const u32 vtable = word(rt, self);
        put(rt, self + 0x4u, arg1);
        put(rt, self + 0x8u, arg2);
        call_set(rt, ctx, word(rt, vtable + 0x34u), {self, 0u, 0u, arg1, arg2});
    }
    ctx.gpr[2] = started;
}
void func_08B44E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], first = ctx.gpr[5], second = ctx.gpr[6], third = ctx.gpr[7], kind = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x18u}, {21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x1Cu}});
    const u32 block = object + 0x8DCu;
    ctx.gpr[22] = block;
    ctx.gpr[21] = object;
    ctx.gpr[20] = second;
    ctx.gpr[19] = first;
    ctx.gpr[18] = third;
    ctx.gpr[17] = ctx.gpr[11];
    ctx.gpr[16] = ctx.gpr[10];
    ctx.gpr[2] = 0x58u;
    if (kind != 0x58u) return;
    u32 entry = call_set(rt, ctx, 0x08B3F098u, {block, first, word(rt, third + 0x40u), word(rt, third + 0x44u)});
    ctx.gpr[3] = entry;
    if (entry == 0u) {
        call_set(rt, ctx, 0x08B44C10u, {object, first, second, third, 0x58u});
        entry = call_set(rt, ctx, 0x08B3F098u, {block, first, word(rt, third + 0x40u), word(rt, third + 0x44u)});
        ctx.gpr[3] = entry;
        if (entry == 0u) return;
    }
    if (word(rt, entry + 0x28u) < 0x2u) {
        ctx.gpr[2] = 0x2u;
        put(rt, entry + 0x28u, 0x2u);
    } else {
        ctx.gpr[2] = 0u;
    }
}
// Marks a 0x58-kind entry as active: looks the entry up with 0x08B3F098 (the block at +0x8DC and the two words
// at the third argument's +0x40 / +0x44); when that finds nothing it builds it through 0x08B44C10 with the same
// kind and looks again. A found entry whose +0x28 word is below 2 gets it set to 2. Another kind returns 0x58.
void func_08AE850C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[19] = word(rt, G[5] + 4u); G[2] = word(rt, G[4] + 0xCu); G[5] = G[19] == 0u ? (G[2] & ~4u) : (G[2] | 4u); G[18] = 0u; if (G[2] != G[5]) { G[3] = word(rt, G[4] + 0x1Cu); put(rt, G[4] + 0xCu, G[5]); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[18] = G[3]; if (G[18] != 0u) { do { G[3] = load8(G[18] + 0x36u); G[2] = sext(G[3], 8); G[17] = 0u; if (static_cast<std::int32_t>(G[2]) > 0) { G[16] = G[18] + 0x38u; G[2] = sext(load16(G[16]), 16); for (;;) { G[17] += 1u; G[16] += 2u; G[4] = G[2]; G[5] = 6u; G[6] = G[19]; G[7] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08AEF874u); G[3] = load8(G[18] + 0x36u); } G[2] = sext(G[3], 8); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) break; G[2] = sext(load16(G[16]), 16); } } G[3] = word(rt, G[18] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[18] = G[3]; } while (G[3] != 0u); } } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08AE4230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = 0x08D40000u; G[3] = word(rt, 0x08D40938u); G[7] = word(rt, G[4] + 0x24u); G[8] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[8] = G[3]; G[10] = sext(load16(G[7]), 16); if (G[8] == 0u) return; outer: G[3] = word(rt, G[8] + 0x10u); G[7] = 0u; if (G[3] != 0u) { G[2] = sext(load16(G[3] + 0x12u), 16); if (G[2] == G[10]) { G[2] = word(rt, G[3] + 0x14u); if (G[2] != G[5]) { G[3] = word(rt, G[8] + 0x18u); goto scan; } G[2] = word(rt, G[3] + 0x18u); G[2] ^= G[6]; G[7] = G[2] == 0u ? 1u : 0u; } } G[3] = word(rt, G[8] + 0x18u); scan: G[4] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[4] = G[3]; if (G[4] == 0u) { G[3] = word(rt, G[8] + 4u); goto advance; } G[9] = 1u; item: if (G[7] != 0u) { store8(G[4] + 0x5Au, G[9]); goto next_item; } G[2] = sext(load16(G[4] + 0x18u), 16); if (G[2] == G[10]) { G[2] = sext(load16(G[4] + 0x1Eu), 16); if (G[2] != G[5]) { G[3] = word(rt, G[4] + 4u); goto item_link; } G[2] = sext(load16(G[4] + 0x20u), 16); if (G[2] != G[6]) { G[3] = word(rt, G[4] + 4u); goto item_link; } store8(G[4] + 0x5Au, G[9]); } next_item: G[3] = word(rt, G[4] + 4u); item_link: G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[4] = G[3]; if (G[3] != 0u) goto item; G[3] = word(rt, G[8] + 4u); advance: G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[8] = G[3]; if (G[3] != 0u) goto outer; } }
void func_08BBE774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[23]); G[23] = G[4] + 4u; put(rt, frame.at(0x28u), G[22]); G[22] = G[6]; put(rt, frame.at(0x24u), G[21]); G[21] = G[5]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[2] = word(rt, G[4] + 4u); G[2] -= G[6]; G[19] = sra(G[2], 4); G[16] = G[5]; if (static_cast<std::int32_t>(G[19]) > 0) { G[17] = G[6]; G[18] = 0u; do { G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B957D8u); G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[17] + 8u); G[4] = word(rt, G[17] + 0xCu); G[18] += 1u; put(rt, G[16] + 4u, G[3]); G[17] += 0x10u; put(rt, G[16] + 8u, G[2]); put(rt, G[16] + 0xCu, G[4]); G[16] += 0x10u; } while (G[19] != G[18]); } G[5] = word(rt, G[23]); G[4] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08BBE6F0u); G[3] = word(rt, G[20] + 4u); G[2] = G[22] - G[21]; G[2] &= ~0xFu; G[3] -= G[2]; put(rt, G[20] + 4u, G[3]); G[2] = G[21]; G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08C2ECE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[3] = G[5] - G[4]; G[2] = sra(G[3], 2); { GuestFrame frame(ctx, 0x30u); G[2] = static_cast<std::int32_t>(G[2]) < 2 ? 1u : 0u; put(rt, frame.at(0x24u), G[21]); G[21] = G[6]; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); if (G[2] == 0u) { G[17] = G[3] - 4u; G[22] = frame.at(0u); G[20] = frame.at(4u); G[16] -= 4u; do { G[4] = G[20]; G[5] = G[16]; G[19] = sra(G[17], 2); call_here(rt, ctx, 0x08A76ABCu); G[17] -= 4u; G[4] = G[16]; if (G[16] != G[18]) { G[5] = word(rt, G[18]); call_here(rt, ctx, 0x08A5BAD8u); } G[5] = G[20]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[5] = 0u; G[6] = G[19]; G[7] = frame.at(0u); G[8] = G[21]; G[4] = G[18]; call_here(rt, ctx, 0x08C2EB94u); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = static_cast<std::int32_t>(G[19]) < 2 ? 1u : 0u; G[16] -= 4u; } while (G[2] == 0u); } G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08BF48E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(8u), G[18]); G[18] = G[4]; G[4] = G[8]; put(rt, frame.at(0u), G[16]); G[16] = G[8]; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); G[17] = G[6]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); call_here(rt, ctx, 0x08AA2A30u); G[4] = G[16] + 4u; G[19] = G[2]; call_here(rt, ctx, 0x08AA2898u); G[5] = word(rt, G[18] + 0x18u); G[17] = word(rt, G[17]); G[6] = word(rt, G[18] + 0x14u); G[4] = sra(G[5], 1); G[3] = G[5] & 1u; ctx.fpr[20] = ctx.fpr[0]; G[7] = G[6]; G[4] = G[17] + G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4]); G[2] += G[6]; G[7] = word(rt, G[2]); } else { G[4] = sra(G[5], 1); G[4] = G[17] + G[4]; } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[18] + 0x20u); G[3] = sra(G[2], 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[18] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[25] = word(rt, G[2]); } else { G[25] = word(rt, G[18] + 0x1Cu); G[4] = G[3] + G[5]; } ctx.fpr[12] = ctx.fpr[20]; G[5] = G[19]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } jump(ctx, G[25]); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_235
} // namespace gt::game

#define HOT235(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_235::name, gt::ReturnKind::kind)
HOT235(0x088A5B3Cu, func_088A5B3C, Word);
HOT235(0x088CF5F4u, func_088CF5F4, Word);
// HOT235(0x08936ED0u, func_08936ED0, Word);  // no comparable synthetic case yet
HOT235(0x08A082A8u, func_08A082A8, Word);
// HOT235(0x08A084D4u, func_08A084D4, Word);  // no comparable synthetic case yet
// HOT235(0x08A21928u, func_08A21928, Word);  // no comparable synthetic case yet
// HOT235(0x08A41F4Cu, func_08A41F4C, Word);  // no comparable synthetic case yet
HOT235(0x08A4F3CCu, func_08A4F3CC, Word);
HOT235(0x08A57BD4u, func_08A57BD4, Word);
HOT235(0x08A80840u, func_08A80840, Word);
HOT235(0x08A90A3Cu, func_08A90A3C, Word);
// HOT235(0x08ACBACCu, func_08ACBACC, Word);  // no comparable synthetic case yet
HOT235(0x08AFAE68u, func_08AFAE68, Word);
HOT235(0x08B44E64u, func_08B44E64, Word);
HOT235(0x08AE850Cu, func_08AE850C, Word);
HOT235(0x08AE4230u, func_08AE4230, Word);
// HOT235(0x08BBE774u, func_08BBE774, Word);  // no comparable synthetic case yet
HOT235(0x08C2ECE8u, func_08C2ECE8, Word);
// HOT235(0x08BF48E4u, func_08BF48E4, Word);  // no comparable synthetic case yet
