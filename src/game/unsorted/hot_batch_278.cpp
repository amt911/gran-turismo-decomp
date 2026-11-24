// Hot batch 278: sixty-three to sixty-six-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (a bit-count tail call, list teardown, stream flush, table regeneration and
// one-shot dispatchers).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_278 {
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

void func_08894B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0x4u), G[17]); G[19] = 0u; G[18] = G[5]; G[16] = G[4]; if (G[5] == 0u) G[17] = 0u; else { G[2] = G[5] - 1u; G[19] = 0u; G[18] = G[2]; G[2] |= G[19]; if (G[2] == 0u) { G[6] = 1u; G[17] = 0u; } else { G[6] = 1u; G[7] = 0u; G[17] = 0u; G[5] = 1u; while (true) { G[2] = G[18] - G[6]; G[4] = G[18] < G[2] ? 1u : 0u; G[3] = G[19] - G[7]; G[17] += 1u; G[3] -= G[4]; G[6] = G[5] << (G[17] & 31u); G[4] = G[17] & 0x20u; G[7] = 0u; if (G[4] != 0u) G[7] = G[6]; G[18] = G[2]; if (G[4] != 0u) G[6] = 0u; G[2] = G[3] < G[7] ? 1u : 0u; G[19] = G[3]; G[3] = G[18] < G[6] ? 1u : 0u; if (G[2] != 0u) break; if (G[19] == G[7] && G[3] != 0u) break; } } } G[16] += 0x28u; G[8] = G[17]; G[4] = G[16]; G[6] = 0xFFFFFFFFu; G[7] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B1CA60u); G[4] = G[16]; G[6] = 0u; G[7] = 0u; G[8] = 1u; call_here(rt, ctx, 0x08B1CA60u); G[4] = G[16]; G[6] = G[18]; G[7] = G[19]; G[8] = G[17]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } jump(ctx, 0x08B1CA60u); } }
void func_08B3EBFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[5] = word(rt, G[4] + 0x24u); G[18] = G[4]; if (G[5] != 0u) { G[2] = load8(G[4] + 0x20u); G[6] = 0u; G[3] = 0u; bool hit = G[2] != 0u; if (!hit) { while (true) { G[3] += 1u; if (G[3] == G[5]) break; G[2] = load8(G[4] + 0x21u); G[4] += 1u; if (G[2] != 0u) { G[6] = G[3]; hit = static_cast<std::int32_t>(G[3]) >= 0; break; } } } if (hit) { G[2] = G[6] << 2; G[2] += G[18]; G[17] = word(rt, G[2]); if (G[17] != 0u) { do { G[2] = word(rt, G[17]); G[5] = G[2]; if (G[2] != 0u) { G[4] = word(rt, G[18] + 0x2Cu); call_here(rt, ctx, 0x08B2CE7Cu); } G[16] = word(rt, G[17] + 8u); G[4] = word(rt, G[18] + 0x2Cu); G[5] = G[17]; G[17] = G[16]; call_here(rt, ctx, 0x08B2CE7Cu); } while (G[16] != 0u); } } } G[2] = G[18] + 0x10u; G[3] = G[18] + 0x20u; put(rt, G[18] + 0x10u, 0u); store8(G[18] + 0x20u, 0u); put(rt, G[18] + 0x28u, 0u); put(rt, G[2] + 0xCu, 0u); store8(G[3] + 3u, 0u); put(rt, G[18], 0u); put(rt, G[18] + 4u, 0u); put(rt, G[18] + 8u, 0u); put(rt, G[18] + 0xCu, 0u); put(rt, G[2] + 4u, 0u); put(rt, G[2] + 8u, 0u); store8(G[3] + 1u, 0u); store8(G[3] + 2u, 0u); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B43A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { int tail = 0; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); G[17] = G[4] + 0x8DCu; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B3EBF4u); G[3] = word(rt, G[16]); put(rt, G[16] + 0x940u, G[2]); G[2] = word(rt, G[3] + 0xA4u); G[4] = G[16]; call_here(rt, ctx, G[2]); bool check = true; if (G[2] != 0u) { G[4] = word(rt, G[16] + 0xB0u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x3Cu); call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = G[17]; G[7] = G[3]; call_here(rt, ctx, 0x08B3F098u); check = G[2] == 0u; } if (check) { G[2] = load8(G[16] + 0xB6u); if (G[2] == 0u) { G[2] = word(rt, G[16] + 0xC0u); if (static_cast<std::int32_t>(G[2]) >= 0) { G[4] = G[16]; call_here(rt, ctx, 0x08B408B0u); } } } G[4] = G[16]; call_here(rt, ctx, 0x08B40DD8u); G[3] = word(rt, G[16]); G[17] = G[2]; G[2] = word(rt, G[3] + 0xA4u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u && G[17] != 0u) { G[2] = load8(G[16] + 0x936u); if (G[2] != 0u) { G[4] = G[16]; tail = 1; } else { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0x40u); tail = 2; } } G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail == 1) jump(ctx, 0x08B439A4u); else if (tail == 2) jump(ctx, G[25]); } }
void func_08B7807C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[17] = G[4]; if (G[4] == 0u) { G[4] = word(rt, 0x08CE0E60u); G[5] = 0x08B7807Cu; tail = true; } else { call_here(rt, ctx, 0x08B796C4u); G[3] = word(rt, G[2] + 0x38u); if (G[3] == 0u) { call_here(rt, ctx, 0x08B796C4u); G[4] = G[2]; call_here(rt, ctx, 0x08B78430u); } G[3] = sext(load16(G[17] + 0xCu), 16); G[2] = G[3] & 8u; if (G[2] == 0u) G[3] = 0u; else { G[18] = word(rt, G[17] + 0x10u); G[3] &= 3u; if (G[18] == 0u) G[3] = 0u; else { G[2] = word(rt, G[17]); put(rt, G[17], G[18]); G[16] = G[2] - G[18]; G[2] = 0u; if (G[3] == 0u) G[2] = word(rt, G[17] + 0x14u); put(rt, G[17] + 8u, G[2]); bool failed = false; while (true) { G[5] = G[18]; if (static_cast<std::int32_t>(G[16]) <= 0) break; G[2] = word(rt, G[17] + 0x24u); G[4] = word(rt, G[17] + 0x1Cu); G[6] = G[16]; call_here(rt, ctx, G[2]); G[16] -= G[2]; G[18] += G[2]; if (static_cast<std::int32_t>(G[2]) <= 0) { failed = true; break; } } if (failed) { G[2] = load16(G[17] + 0xCu); G[3] = 0xFFFFFFFFu; G[2] |= 0x40u; store16(G[17] + 0xCu, G[2]); } else G[3] = 0u; } } G[2] = G[3]; } G[17] = word(rt, frame.at(0x4u)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, 0x08B794F4u); } }
void func_08806628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[20] = G[4]; G[17] = G[5]; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08C97DA8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[19] = frame.at(4u); call_here(rt, ctx, G[2]); G[5] = G[17]; G[4] = G[19]; G[18] = G[2]; call_here(rt, ctx, 0x08A57854u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C97DD8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16] + 0x1Cu); G[17] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[16] + 0x18u); if (G[16] == 0u) { G[4] = 0x08C97DBCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[16] + 0xCu); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = word(rt, G[17] + 8u); G[5] = G[18] + G[5]; G[2] = static_cast<std::int32_t>(G[5]) < 0x2711 ? 1u : 0u; put(rt, G[17] + 8u, G[5]); if (G[2] == 0u) { G[5] = 0x2710u; put(rt, G[17] + 8u, G[5]); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[20] != frame.at(0u)) { G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0884FE7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[17] = frame.at(4u); G[18] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A5D0Cu); G[4] = 0x38u; G[16] = G[2]; call_here(rt, ctx, 0x08B2B5E0u); G[6] = G[2]; G[7] = G[2]; G[8] = G[16] + 0x30u; do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[7], G[2]); G[16] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[16] != G[8]); G[2] = word(rt, G[16] + 4u); G[3] = word(rt, G[16]); put(rt, G[7] + 4u, G[2]); G[8] = 0x0884F734u; put(rt, G[7], G[3]); G[4] = frame.at(0u); G[7] = 0xFFFFFFFFu; G[5] = word(rt, 0x08CE2D08u); G[9] = 0u; call_here(rt, ctx, 0x08A52AB8u); if (G[18] != frame.at(0u)) { G[4] = G[18]; G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } else G[5] = word(rt, frame.at(0u)); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, 0x08CE2CECu); G[2] += 1u; put(rt, 0x08CE2CECu, G[2]); G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0885046C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[17] = frame.at(4u); G[18] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A5D0Cu); G[16] = G[2]; G[2] = load8(G[2] + 0x300u); G[4] = 0x18u; G[17] = G[16] + 0x300u; if (G[2] != 0u) { call_here(rt, ctx, 0x08B2B5E0u); G[4] = word(rt, G[16] + 0x300u); G[5] = word(rt, G[17] + 8u); G[9] = word(rt, G[17] + 0xCu); G[11] = word(rt, G[17] + 0x14u); G[3] = word(rt, G[17] + 4u); G[10] = word(rt, G[17] + 0x10u); G[6] = G[2]; put(rt, G[2], G[4]); G[8] = 0x0884F734u; G[4] = frame.at(0u); put(rt, G[2] + 0xCu, G[9]); G[7] = 0xFFFFFFFFu; put(rt, G[2] + 4u, G[3]); G[9] = 0u; put(rt, G[2] + 8u, G[5]); put(rt, G[2] + 0x10u, G[10]); put(rt, G[2] + 0x14u, G[11]); G[5] = word(rt, 0x08CE2CF0u); call_here(rt, ctx, 0x08A52AB8u); if (G[18] != frame.at(0u)) { G[4] = G[18]; G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } else G[5] = word(rt, frame.at(0u)); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, 0x08CE2CECu); G[2] += 1u; put(rt, 0x08CE2CECu, G[2]); } G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0887C3AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = sext(load16(G[4] + 0x364u), 16); G[18] = G[4]; if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = 0u; G[16] = G[4] + 0xCu; do { G[4] = word(rt, G[18]); G[5] = G[16]; G[17] += 1u; G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x20u); G[16] += 0xCu; call_here(rt, ctx, G[3]); G[2] = sext(load16(G[18] + 0x364u), 16); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; } while (G[2] != 0u); } G[2] = sext(load16(G[18] + 0x366u), 16); store16(G[18] + 0x364u, 0u); if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = G[18]; G[20] = 0u; G[19] = G[18] + 0x12Cu; do { G[4] = word(rt, G[17] + 0x128u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x10u); G[20] += 1u; call_here(rt, ctx, G[2]); G[4] = word(rt, G[17] + 0x128u); G[16] = G[2]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x14u); G[5] = word(rt, G[17] + 0x124u); call_here(rt, ctx, G[2]); G[4] = word(rt, G[18]); G[5] = word(rt, G[17] + 0x128u); G[6] = G[19]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x24u); G[19] += 0xCu; call_here(rt, ctx, G[2]); G[4] = word(rt, G[17] + 0x128u); G[5] = G[16]; G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x14u); G[17] += 0xCu; call_here(rt, ctx, G[3]); G[2] = sext(load16(G[18] + 0x366u), 16); G[2] = static_cast<std::int32_t>(G[20]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; } while (G[2] != 0u); } store16(G[18] + 0x366u, 0u); G[18] = word(rt, frame.at(0x8u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
// 0x08881E10 (a0 = the object, a1 = a 0x30-byte source, a2 = a flag byte): with the object's word +0x4C null nothing happens; otherwise it is asked its
// virtual +0x20 and, when the answer is non-zero, 0x0889A968(that word + 0x28, source, object + 0x54, bit 1 of the object's +0x16EC, 1, 0, 1) runs. With the
// flag set the same 0x30-byte block is copied from the source into a frame copy (its words at +0, +0x10, +8, +0xC, +0x18, +0x24 and the byte at +0x28, the
// words at +0x10, +0x14, +0x1C, +0x20 and +0x2C zero) and 0x0889A968 runs again with the copy.
void func_08881E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], source = ctx.gpr[5], flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x40u}, {18u, 0x38u}, {17u, 0x34u}, {31u, 0x44u}, {19u, 0x3Cu}, {16u, 0x30u}});
    ctx.gpr[20] = flag;
    ctx.gpr[18] = self;
    ctx.gpr[17] = source;
    const u32 holder = word(rt, self + 0x4Cu);
    ctx.gpr[4] = holder;
    if (holder == 0u) return;
    const u32 method = word(rt, word(rt, holder) + 0x20u);
    ctx.gpr[2] = method;
    const u32 accepted = call_set(rt, ctx, method, {holder});
    const u32 target = self + 0x54u;
    ctx.gpr[19] = target;
    ctx.gpr[5] = source;
    ctx.gpr[6] = target;
    ctx.gpr[8] = 1u;
    ctx.gpr[9] = 0u;
    ctx.gpr[10] = 1u;
    if (accepted == 0u) return;
    const u32 block = word(rt, self + 0x4Cu) + 0x28u;
    ctx.gpr[16] = block;
    const u32 flags = word(rt, self + 0x16ECu);
    ctx.gpr[4] = block;
    ctx.gpr[7] = (flags >> 1u) & 1u;
    call_set(rt, ctx, 0x0889A968u, {block, source, target, (flags >> 1u) & 1u, 1u, 0u, 1u});
    ctx.gpr[5] = frame.at(0x0u);
    ctx.gpr[6] = target;
    ctx.gpr[8] = 1u;
    ctx.gpr[9] = 0u;
    ctx.gpr[10] = 1u;
    if (flag == 0u) return;
    const u32 second = word(rt, self + 0x16ECu);
    ctx.gpr[7] = (second >> 1u) & 1u;
    const u32 word_00 = word(rt, source), word_10 = word(rt, source + 0x10u);
    const u32 word_24 = word(rt, source + 0x24u), word_18 = word(rt, source + 0x18u);
    const u32 word_08 = word(rt, source + 0x8u), word_0C = word(rt, source + 0xCu);
    const u32 byte_28 = load8(source + 0x28u);
    put(rt, frame.at(0x0u), word_00);
    put(rt, frame.at(0x4u), word_10);
    put(rt, frame.at(0x8u), word_08);
    put(rt, frame.at(0xCu), word_0C);
    put(rt, frame.at(0x18u), word_18);
    put(rt, frame.at(0x24u), word_24);
    store8(frame.at(0x28u), byte_28);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    call_set(rt, ctx, 0x0889A968u, {block, frame.at(0x0u), target, (second >> 1u) & 1u, 1u, 0u, 1u});
}
// 0x0889A19C (a0 = the object, a1 = a flag (its low byte), f12 = a float; v0 = the byte at +0xB09, or 0): with the object's byte +0x8EC set nothing happens.
// With the flag set and the object's +0xB08 byte set the answer is its +0xB09 byte. Otherwise the object's word +0x20 leads (through its word +8) to a holder
// whose word +0x30 (s1) must be non-null; the leading word twice more gives a node whose halfword +0x10 must be at least 6, and the node's word +0x30 plus
// 0xF0 must be non-zero. Then 0x08B54288(s1 + 4)'s float times s1's own float, 0x08CE2FE0 and f12 is compared with 1.0: below it +0xB09 becomes 1, else 0, and
// the flag also sets +0xB08 to 1. v0 answers +0xB09 read back.
void func_0889A19C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {16u, 0x0u}, {31u, 0xCu}, {17u, 0x4u}, {52u, 0x10u}});
    ctx.gpr[18] = flag;
    ctx.gpr[16] = self;
    ctx.fpr[20] = ctx.fpr[12];
    if (load8(self + 0x8ECu) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (flag != 0u && load8(self + 0xB08u) != 0u) {
        ctx.gpr[2] = load8(self + 0xB09u);
        return;
    }
    const u32 leading = word(rt, self + 0x20u);
    const u32 holder = leading != 0u ? word(rt, leading + 0x8u) : 0u;
    const u32 block = word(rt, holder + 0x30u);
    ctx.gpr[17] = block;
    if (block == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 next = word(rt, self + 0x20u);
    const u32 node = next != 0u ? word(rt, next + 0x8u) : 0u;
    if (static_cast<s32>(load16(node + 0x10u)) < 6) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 deep = next != 0u ? word(rt, next + 0x8u) : 0u;
    if (word(rt, deep + 0x30u) + 0xF0u == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08B54288u, {block + 0x4u});
    const float scaled = psp_mul(psp_mul(psp_mul(ctx.fpr[0], float_at(rt, block)), float_at(rt, 0x08CE2FE0u)), ctx.fpr[20]);
    ctx.gpr[2] = scaled < 1.0f ? 1u : 0u;
    store8(self + 0xB09u, ctx.gpr[2]);
    if (flag != 0u) store8(self + 0xB08u, 1u);
    ctx.gpr[2] = load8(self + 0xB09u);
}
void func_088F6DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[19]); G[1] = 0x3A83126Fu; F[14] = std::bit_cast<float>(G[1]); G[18] = G[6]; G[17] = G[4]; G[4] = frame.at(0u); G[16] = G[5]; put_float(rt, frame.at(0x44u), F[21]); F[21] = F[12]; G[1] = 0x45F61000u; F[12] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x40u), F[20]); F[20] = F[13]; G[1] = 0x40266666u; F[13] = std::bit_cast<float>(G[1]); G[19] = G[7]; call_here(rt, ctx, 0x088F63C8u); G[4] = G[17]; G[5] = G[16]; G[6] = G[18]; G[7] = frame.at(0u); call_here(rt, ctx, 0x088F7364u); F[1] = float_at(rt, frame.at(0xCu)); G[1] = 0x3F000000u; F[0] = 0.5f; F[2] = float_at(rt, frame.at(8u)); F[1] = F[1] * F[0]; F[0] = float_at(rt, frame.at(4u)); G[16] = G[17] + 0x34u; put_float(rt, G[17] + 0x34u, F[20]); F[1] = F[1] * F[0]; G[1] = 0x41D80000u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 4u, F[0]); G[5] = G[19]; F[1] = F[1] / F[2]; G[4] = G[17] + 0x50u; put_float(rt, G[16] + 0x18u, F[1]); call_here(rt, ctx, 0x088F6C54u); G[1] = 0xBE60389Bu; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 0x3Cu, F[0]); put_float(rt, G[16] + 0x38u, F[21]); G[2] = word(rt, G[17] + 0x100u); G[2] &= ~(1u << 11); G[2] |= 0x400u; put(rt, G[17] + 0x100u, G[2]); store8(G[17] + 0x100u, 0u); F[21] = float_at(rt, frame.at(0x44u)); F[20] = float_at(rt, frame.at(0x40u)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x2Cu)); }  } } }
void func_08902554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[31]); G[3] = G[4]; G[7] = G[6]; F[0] = float_at(rt, G[6] + 4u); F[2] = float_at(rt, G[4] + 0x24u); G[8] = G[5]; G[2] = 0u; if (!(F[0] < F[2])) { G[2] = word(rt, G[4] + 0x8Cu); G[6] = frame.at(0x18u); G[4] = G[5]; G[5] = G[3]; bool second = false; if (G[2] != 0u) { G[2] = word(rt, G[3] + 0x90u); F[1] = F[0]; second = G[2] != 0u; } if (second) { F[0] = float_at(rt, G[3] + 0x30u); F[3] = float_at(rt, G[7]); put_float(rt, frame.at(0u), F[3]); if (!(F[1] < F[0])) F[1] = F[0]; G[10] = word(rt, G[3] + 0x94u); F[0] = float_at(rt, G[7] + 8u); G[5] = G[8]; G[4] = G[3]; G[7] = frame.at(0u); G[8] = frame.at(0xCu); G[6] = G[3] + 0x20u; G[9] = 0u; G[11] = 0u; G[10] -= 1u; put_float(rt, frame.at(4u), F[1]); put_float(rt, frame.at(0xCu), F[3]); put_float(rt, frame.at(0x10u), F[2]); put_float(rt, frame.at(0x14u), F[0]); put_float(rt, frame.at(8u), F[0]); call_here(rt, ctx, 0x08901DB8u); } else { F[1] = float_at(rt, G[3] + 0x30u); const bool less = F[0] < F[1]; F[2] = float_at(rt, G[7]); if (!less) F[0] = F[1]; F[1] = float_at(rt, G[7] + 8u); put_float(rt, frame.at(0x1Cu), F[0]); G[1] = 0xBF800000u; F[0] = -1.0f; put_float(rt, frame.at(0x18u), F[2]); put_float(rt, frame.at(0x28u), F[0]); put(rt, frame.at(0x24u), 0u); put(rt, frame.at(0x2Cu), 0u); put_float(rt, frame.at(0x20u), F[1]); call_here(rt, ctx, 0x08908A14u); } }  }  } } }
void func_0892B7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); G[19] = G[5]; G[5] = 0xFFFFFFFFu; G[17] = G[6]; G[16] = G[4]; G[2] = word(rt, G[4]); G[7] = word(rt, G[6] + 0x24u); G[2] = ~G[2]; G[18] = G[7] & G[2]; G[6] = (G[18] >> 4) & 1u; G[2] = (G[18] >> 5) & 1u; G[3] = (G[18] >> 6) & 1u; store8(G[4] + 0x15u, G[2]); store8(G[4] + 0x16u, G[3]); put(rt, G[4], G[7]); store8(G[4] + 0x14u, G[6]); if (G[6] != 0u) call_here(rt, ctx, 0x0892B778u); G[2] = load8(G[16] + 0x15u); G[5] = 1u; G[4] = G[16]; if (G[2] != 0u) call_here(rt, ctx, 0x0892B778u); G[2] = load8(G[16] + 0x16u); G[4] = G[16]; if (G[2] != 0u) call_here(rt, ctx, 0x0892B7D0u); G[17] = word(rt, G[17] + 0x34u); G[2] = (G[18] >> 7) & 1u; G[3] = (G[18] >> 8) & 1u; G[17] += G[2]; G[17] -= G[3]; G[18] = G[17] != 0u ? 1u : 0u; G[4] = G[16]; store8(G[16] + 0x17u, G[18]); call_here(rt, ctx, 0x0892B768u); if (G[2] != 0u) { G[4] = G[16]; if (G[18] != 0u) { call_here(rt, ctx, 0x0892B7E0u); G[5] = G[2]; G[4] = G[19]; G[6] = G[17]; tail = true; } } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail) jump(ctx, 0x0892B558u); } }
// 0x089DB454 (a0 = the object, a1 = a flag byte; v0 = the last call's): under the lock at 0x08CF5428 (0x08B344E0), when the flag is set or the object's word
// +0x13C is still null, the text at +0x134 is rebuilt: with a count in its header (0xC bytes back) 0x089BD4DC(object, 0x08CF6ED8) makes a new text that
// 0x089A50A0(sp + 4, new, object + 0x134, bits 2, 1 and 0 of the object's word +0x140) takes into the object's +0x13C (0x08A5BAD8 first when it is not the slot
// where the text stands, then 0x08A5B9EC releases the slot); without one 0x089E4C14(sp) makes the empty text, taken the same way from sp. Either way the
// object's word +0x1B8 becomes 1 and 0x08B3457C releases the lock.
void func_089DB454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x28u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}});
    const u32 lock = 0x08CF5428u;
    ctx.gpr[21] = lock;
    ctx.gpr[17] = self;
    ctx.gpr[16] = flag;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    if (flag != 0u || word(rt, self + 0x13Cu) == 0u) {
        const u32 slot = self + 0x13Cu;
        const u32 text = word(rt, self + 0x134u);
        ctx.gpr[19] = slot;
        ctx.gpr[18] = self + 0x134u;
        ctx.gpr[20] = frame.at(0x4u);
        if (word(rt, text - 0xCu) != 0u) {
            const u32 made = call_set(rt, ctx, 0x089BD4DCu, {self, 0x08CF6ED8u});
            const u32 bits = word(rt, self + 0x140u);
            call_set(rt, ctx, 0x089A50A0u, {frame.at(0x4u), made, self + 0x134u, (bits >> 2u) & 1u, (bits >> 1u) & 1u, bits & 1u});
            if (slot != frame.at(0x4u)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x4u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
        } else {
            call_set(rt, ctx, 0x089E4C14u, {frame.at(0x0u), 0x08CF6ED8u});
            if (slot != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x0u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
        }
    }
    put(rt, self + 0x1B8u, 1u);
    call_set(rt, ctx, 0x08B3457Cu, {lock});
}
// 0x08A02110 (a0 = the object, a1 = a flag byte; v0 = the last call's): under the lock at 0x08CF5D34 (0x08B344E0), when the flag is set or the object's word
// +0x108 is still null, the text at +0x104 is rebuilt: with a count in its header (0xC bytes back) 0x089BD4DC(object, 0x08CF6ED8) makes a new text that
// 0x089A50A0(sp + 4, new, object + 0x104, bits 3, 2 and 1 of the object's word +0x110) takes into the object's +0x108 (0x08A5BAD8 first when it is not the
// slot where the text stands, then 0x08A5B9EC releases the slot); without one 0x089E4C14(sp) makes the empty text, taken the same way from sp. Either way the
// object's word +0x114 becomes 1 and 0x08B3457C releases the lock.
// 0x08A02110 (a0 = the object, a1 = a flag byte; v0 = the last call's): under the lock at 0x08CF5D34 (0x08B344E0), when the flag is set or the object's word
// +0x108 is still null, the text at +0x104 is rebuilt: with a count in its header (0xC bytes back) 0x089BD4DC(object, 0x08CF6ED8) makes a new text that
// 0x089A50A0(sp + 4, new, object + 0x104, bits 3, 2 and 1 of the object's word +0x110) takes into the object's +0x108 (0x08A5BAD8 first when it is not the
// slot where the text stands, then 0x08A5B9EC releases the slot); without one 0x089E4C14(sp) makes the empty text, taken the same way from sp. Either way the
// object's word +0x114 becomes 1 and 0x08B3457C releases the lock.
void func_08A02110(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x28u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}});
    const u32 lock = 0x08CF5D34u;
    ctx.gpr[21] = lock;
    ctx.gpr[17] = self;
    ctx.gpr[16] = flag;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    if (flag != 0u || word(rt, self + 0x108u) == 0u) {
        const u32 slot = self + 0x108u;
        const u32 text = word(rt, self + 0x104u);
        ctx.gpr[19] = slot;
        ctx.gpr[18] = self + 0x104u;
        ctx.gpr[20] = frame.at(0x4u);
        if (word(rt, text - 0xCu) != 0u) {
            const u32 made = call_set(rt, ctx, 0x089BD4DCu, {self, 0x08CF6ED8u});
            const u32 bits = word(rt, self + 0x110u);
            call_set(rt, ctx, 0x089A50A0u, {frame.at(0x4u), made, self + 0x104u, (bits >> 3u) & 1u, (bits >> 2u) & 1u, (bits >> 1u) & 1u});
            if (slot != frame.at(0x4u)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x4u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
        } else {
            call_set(rt, ctx, 0x089E4C14u, {frame.at(0x0u), 0x08CF6ED8u});
            if (slot != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x0u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
        }
    }
    put(rt, self + 0x114u, 1u);
    call_set(rt, ctx, 0x08B3457Cu, {lock});
}
void func_08A4094C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x10u), G[16]); G[19] = frame.at(4u); G[18] = G[5]; G[17] = G[4]; G[4] = 0x08CADB50u; G[16] = word(rt, G[5]); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A6BFFCu); G[4] = G[2]; call_here(rt, ctx, 0x08B21938u); G[5] = 0x08CF6848u; G[4] = G[17] + 0x108u; put(rt, G[17] + 0x14u, G[2]); if (G[2] != 0u) { G[16] = word(rt, G[18]); G[4] = 0x08CADB50u; G[18] = G[17] + 0x108u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = G[19]; call_here(rt, ctx, G[2]); G[4] = G[18]; G[5] = G[19]; call_here(rt, ctx, 0x08B855E0u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } else call_here(rt, ctx, 0x08B855E0u); G[4] = G[17]; call_here(rt, ctx, 0x08A3EC10u); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08B1EB50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x38u), G[31]); G[17] = G[4]; G[4] = word(rt, G[4]); G[16] = G[5]; if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = word(rt, G[17] + 8u); if (G[4] != 0u) call_here(rt, ctx, 0x08B61FC0u); G[2] = 1u; put(rt, G[17] + 4u, G[2]); G[4] = frame.at(0x18u); G[5] = 0x64u; put(rt, G[17] + 8u, 0u); put(rt, frame.at(4u), G[17]); put(rt, frame.at(8u), 0u); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); put(rt, frame.at(0x14u), 0u); put(rt, frame.at(0x18u), 0u); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x20u), 0u); call_here(rt, ctx, 0x08C4F32Cu); G[4] = frame.at(4u); G[5] = G[16]; call_here(rt, ctx, 0x08B38568u); put(rt, G[17], G[2]); if (G[2] != 0u) { G[16] = word(rt, G[17] + 4u); G[4] = G[16] << 3; call_here(rt, ctx, 0x08B6461Cu); G[3] = G[2]; if (G[16] != 0u) { G[4] = 0u; G[5] = 0xFFFFFFFFu; do { G[4] += 1u; put(rt, G[3], G[5]); put(rt, G[3] + 4u, G[5]); G[3] += 8u; } while (G[16] != G[4]); } put(rt, G[17] + 8u, G[2]); } G[2] = word(rt, G[17]); G[4] = word(rt, frame.at(0x18u)); G[5] = word(rt, frame.at(0x1Cu)); G[6] = frame.at(0u); G[16] = G[2] != 0u ? 1u : 0u; call_here(rt, ctx, 0x08C4F218u); G[4] = word(rt, frame.at(0x18u)); if (G[4] != 0u) call_here(rt, ctx, 0x08B35254u); G[2] = G[16]; G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
void func_08B1EDE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); G[2] = 0x08C93344u; G[6] = word(rt, 0x08C93344u); G[5] = word(rt, G[2] + 4u); put(rt, frame.at(0u), G[6]); G[9] = G[4]; G[7] = G[4]; put(rt, frame.at(4u), G[5]); G[8] = 0u; G[6] = 0xE3u; do { G[3] = word(rt, G[7]); G[4] = word(rt, G[7] + 4u); G[5] = word(rt, G[7] + 0x634u); G[3] &= 0x80000000u; G[4] &= 0x7FFFFFFFu; G[3] |= G[4]; G[2] = G[3] & 1u; G[2] <<= 2; G[2] += frame.at(0u); G[4] = word(rt, G[2]); G[3] >>= 1; G[5] ^= G[3]; G[5] ^= G[4]; G[8] += 1u; put(rt, G[7], G[5]); G[7] += 4u; } while (G[8] != G[6]); G[6] = G[9] + 0x38Cu; G[7] = 0x18Cu; do { G[3] = word(rt, G[6]); G[4] = word(rt, G[6] + 4u); G[5] = word(rt, G[6] - 0x38Cu); G[3] &= 0x80000000u; G[4] &= 0x7FFFFFFFu; G[3] |= G[4]; G[2] = G[3] & 1u; G[2] <<= 2; G[2] += frame.at(0u); G[4] = word(rt, G[2]); G[3] >>= 1; G[5] ^= G[3]; G[5] ^= G[4]; G[7] -= 1u; put(rt, G[6], G[5]); G[6] += 4u; } while (G[7] != 0u); G[3] = word(rt, G[9] + 0x9BCu); G[4] = word(rt, G[9]); G[5] = word(rt, G[9] + 0x630u); G[3] &= 0x80000000u; G[4] &= 0x7FFFFFFFu; G[3] |= G[4]; G[2] = G[3] & 1u; G[2] <<= 2; G[2] += frame.at(0u); G[4] = word(rt, G[2]); G[3] >>= 1; G[5] ^= G[3]; G[5] ^= G[4]; put(rt, G[9] + 0x9BCu, G[5]); put(rt, G[9] + 0x9C0u, 0u); } }
void func_08BD344C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[2] = word(rt, G[7]); G[30] = word(rt, G[7] + 0x10u); G[19] = word(rt, G[7] + 0xCu); G[22] = word(rt, G[7] + 4u); G[21] = word(rt, G[7] + 8u); G[23] = G[6]; G[20] = G[4]; G[18] = G[5]; put(rt, frame.at(0x10u), G[2]); while (true) { G[4] = G[19]; if (G[18] == G[23]) break; G[17] = load16(G[18]); G[5] = G[21]; G[6] = G[17]; call_here(rt, ctx, 0x089063A8u); G[18] += 2u; G[4] = G[22]; G[5] = frame.at(0u); if (G[2] == 0u) continue; store16(frame.at(0u), G[17]); call_here(rt, ctx, 0x08BD32A8u); G[16] = G[2]; G[5] = word(rt, G[19] + 0x30u); G[3] = G[17] << 4; G[2] = G[17] << 6; G[2] -= G[3]; G[5] += G[2]; G[4] = G[30]; G[5] += 4u; call_here(rt, ctx, 0x08906304u); F[12] = F[0]; G[4] = word(rt, frame.at(0x10u)); G[5] = G[16]; call_here(rt, ctx, 0x08AF10C4u); } put(rt, G[20] + 0x10u, G[30]); G[2] = G[20]; put(rt, G[20] + 4u, G[22]); G[3] = word(rt, frame.at(0x10u)); put(rt, G[20] + 8u, G[21]); put(rt, G[20], G[3]); put(rt, G[20] + 0xCu, G[19]); G[30] = word(rt, frame.at(0x40u)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[19] = word(rt, frame.at(0x2Cu)); G[23] = word(rt, frame.at(0x3Cu)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_278
} // namespace gt::game

#define HOT278(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_278::name, gt::ReturnKind::kind)
HOT278(0x08894B70u, func_08894B70, Word);
HOT278(0x08B3EBFCu, func_08B3EBFC, Word);
HOT278(0x08B43A38u, func_08B43A38, Word);
HOT278(0x08B7807Cu, func_08B7807C, Word);
HOT278(0x08806628u, func_08806628, Word);
// HOT278(0x0884FE7Cu, func_0884FE7C, Word);  // no comparable synthetic case yet
// HOT278(0x0885046Cu, func_0885046C, Word);  // no comparable synthetic case yet
HOT278(0x0887C3ACu, func_0887C3AC, Word);
HOT278(0x08881E10u, func_08881E10, Word);
HOT278(0x0889A19Cu, func_0889A19C, Word);
HOT278(0x088F6DFCu, func_088F6DFC, Word);
HOT278(0x08902554u, func_08902554, Word);
HOT278(0x0892B7E8u, func_0892B7E8, Word);
HOT278(0x089DB454u, func_089DB454, Word);
HOT278(0x08A02110u, func_08A02110, Word);
HOT278(0x08A4094Cu, func_08A4094C, Word);
HOT278(0x08B1EB50u, func_08B1EB50, Word);
HOT278(0x08B1EDE8u, func_08B1EDE8, Word);
HOT278(0x08BD344Cu, func_08BD344C, Word);
