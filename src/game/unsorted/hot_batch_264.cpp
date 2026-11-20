// Hot batch 264: small functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (jump tables over value kinds that read a word, a 64-bit pair or a float converted to unsigned, a state
// table that sets flags, a code-to-string table and a float fill through a lookup call).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_264 {
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

// 0x08918CDC (a0 = a four-word output, a2 = an array of a3 pointers, f12 = a value kept in f20; v0 = a0): the output's four words are
// cleared first, and each non-null pointer's value becomes 0x08907A44(pointer, the byte at 0x08CF212C) - called with f12 = the
// value - at the output's same index.
void func_08918CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4], pointers = ctx.gpr[6];
    const s32 count = static_cast<s32>(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {52u, 0x20u}, {31u, 0x18u}, {21u, 0x14u}, {18u, 0x8u},
                                          {17u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[19] = output;
    ctx.fpr[20] = ctx.fpr[12];
    put(rt, output, 0u);
    put(rt, output + 0x4u, 0u);
    put(rt, output + 0x8u, 0u);
    put(rt, output + 0xCu, 0u);
    for (s32 index = 0; index < count; ++index) {
        const u32 pointer = word(rt, pointers + static_cast<u32>(index) * 4u);
        if (pointer == 0u) continue;
        ctx.fpr[12] = ctx.fpr[20];
        call_set(rt, ctx, 0x08907A44u, {pointer, load8(0x08CF212Cu)});
        put_float(rt, output + static_cast<u32>(index) * 4u, ctx.fpr[0]);
    }
    ctx.gpr[2] = output;
}
void func_08B613D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[4] = (G[4] & 0x3Du) - 8u; G[2] = G[4] < 0x35u ? 1u : 0u; G[3] = 0x08C90000u; if (G[2] == 0u) { G[2] = 0u; return; } G[2] = G[4] << 2; G[3] = 0x08C95E04u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B6140Cu: G[2] = 0x08CB5AACu; break; case 0x08B61418u: G[2] = 0x08CB5AC0u; break; case 0x08B61424u: G[2] = 0x08CB5ACCu; break; case 0x08B61430u: G[2] = 0x08CB5AB0u; break; case 0x08B6143Cu: G[2] = 0x08CB5AB4u; break; case 0x08B61448u: G[2] = 0x08CB5AC4u; break; case 0x08B61454u: G[2] = 0x08CB5AB8u; break; case 0x08B61460u: G[2] = 0x08CB5AA8u; break; case 0x08B6146Cu: G[2] = 0x08CB5AC8u; break; case 0x08B61478u: G[2] = 0x08CB5ABCu; break; default: G[2] = 0u; break; } } }
void func_08909628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[6] = word(rt, G[4] + 0xCu); G[2] = 1u; G[5] = G[4]; G[3] = G[6] < 7u ? 1u : 0u; put(rt, G[4] + 4u, G[2]); store8(G[4] + 0x15u, 0u); if (G[3] != 0u) { G[2] = G[6] << 2; G[3] = 0x08C8A578u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08909660u: G[2] = 1u; store8(G[5] + 1u, G[2]); break; case 0x0890966Cu: store8(G[5] + 1u, 0u); break; case 0x08909674u: G[2] = 2u; put(rt, G[5] + 4u, G[2]); store8(G[5] + 0x14u, 0u); break; case 0x08909684u: G[3] = 1u; G[2] = 1u; store8(G[5] + 0x14u, G[2]); store8(G[5] + 0x15u, G[3]); put(rt, G[5] + 4u, G[3]); break; case 0x0890969Cu: G[2] = 1u; G[3] = 2u; store8(G[5] + 0x14u, G[2]); put(rt, G[5] + 4u, G[3]); break; default: break; } } G[2] = G[6] - 3u; G[2] = G[2] < 2u ? 1u : 0u; if (G[2] == 0u) return; G[2] = word(rt, G[5] + 0x10u); const bool none = G[2] == 0u; G[2] = 1u; if (none) return; store8(G[5] + 0x15u, G[2]); put(rt, G[5] + 4u, G[2]); } }
void func_08B1D900(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x10u); G[2] = G[4]; G[4] = 0x08D68A94u; put(rt, frame.at(0u), G[16]); G[16] = G[5]; put(rt, frame.at(4u), G[31]); G[5] = G[2]; call_here(rt, ctx, 0x08B1DEB4u); bool done = G[2] == 0u; if (!done) { G[5] = word(rt, G[2]); done = G[5] == 0u; } if (!done) { G[3] = word(rt, G[5]); done = !(G[3] < 6u); } if (!done) { G[2] = G[3] << 2; G[3] = 0x08C932E0u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B1D96Cu: { F[0] = float_at(rt, G[5] + 8u); G[1] = 0x4F000000u; F[1] = std::bit_cast<float>(G[1]); F[2] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(F[0]))); const bool big = F[1] <= F[0]; G[16] = std::bit_cast<std::uint32_t>(F[2]); if (big) { F[0] = F[0] - F[1]; G[2] = 0x80000000u; F[0] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(F[0]))); G[16] = std::bit_cast<std::uint32_t>(F[0]); G[16] = G[16] | G[2]; } break; } case 0x08B1D9A4u: G[4] = word(rt, G[5] + 8u); G[5] = word(rt, G[5] + 0xCu); call_here(rt, ctx, 0x08B6F110u); G[16] = G[2]; break; default: G[16] = word(rt, G[5] + 8u); break; } } G[2] = G[16]; G[16] = word(rt, frame.at(0u)); } }
// 0x088C54AC (a0 = the object, a1 = a selector below 6, a2 = a float output; v0 = 1 when a value was written, 0 otherwise):
// 0x088C541C(object, 4) gives a record whose word +0 is a count of at least 6 and whose +0x40 starts 0x40-byte entries; the
// selector's table entry at 0x08C89E7C picks entry 1, 2, 0, count - 3, count - 2 or count - 1, and a non-negative entry's float
// +0x14 goes to *a2.
void func_088C54AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 selector = ctx.gpr[5], output = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}});
    ctx.gpr[17] = output;
    ctx.gpr[16] = selector;
    const u32 record = call_set(rt, ctx, 0x088C541Cu, {ctx.gpr[4], 4u});
    ctx.gpr[3] = record;
    ctx.gpr[2] = 0u;
    if (record == 0u) return;
    if (output == 0u) return;
    const s32 count = static_cast<s32>(word(rt, record));
    const u32 entries = record + 0x40u;
    if (count < 6) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (entries == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (selector >= 6u) {
        ctx.gpr[2] = 0u;
        return;
    }
    s32 entry = 0;
    switch (word(rt, 0x08C89E7Cu + selector * 4u)) {
    case 0x088C5518u: entry = 1; break;
    case 0x088C5520u: entry = 2; break;
    case 0x088C5528u: entry = 0; break;
    case 0x088C5530u: entry = count - 3; break;
    case 0x088C5538u: entry = count - 2; break;
    default: entry = count - 1; break;
    }
    if (entry < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.fpr[0] = float_at(rt, entries + static_cast<u32>(entry) * 64u + 0x14u);
    put_float(rt, output, ctx.fpr[0]);
    ctx.gpr[2] = 1u;
}
void func_08B1D9C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x10u); G[5] = G[4]; G[4] = 0x08D68A94u; put(rt, frame.at(4u), G[17]); G[17] = G[7]; put(rt, frame.at(0u), G[16]); put(rt, frame.at(8u), G[31]); G[16] = G[6]; call_here(rt, ctx, 0x08B1DEB4u); bool done = G[2] == 0u; if (!done) { G[5] = word(rt, G[2]); done = G[5] == 0u; } if (!done) { G[3] = word(rt, G[5]); done = !(G[3] < 6u); } if (!done) { G[2] = G[3] << 2; G[3] = 0x08C932F8u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B1DA30u: G[2] = word(rt, G[5] + 8u); G[16] = G[2]; G[17] = sra(G[2], 31u); break; case 0x08B1DA40u: G[16] = word(rt, G[5] + 8u); G[17] = 0u; break; case 0x08B1DA58u: F[12] = float_at(rt, G[5] + 8u); call_here(rt, ctx, 0x08B6F4D0u); G[16] = G[2]; G[17] = G[3]; break; case 0x08B1DA6Cu: G[4] = word(rt, G[5] + 8u); G[5] = word(rt, G[5] + 0xCu); call_here(rt, ctx, 0x08B6F300u); G[16] = G[2]; G[17] = G[3]; break; default: G[16] = word(rt, G[5] + 8u); G[17] = word(rt, G[5] + 0xCu); break; } } G[2] = G[16]; G[3] = G[17]; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_264
} // namespace gt::game

#define HOT264(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_264::name, gt::ReturnKind::kind)
HOT264(0x08918CDCu, func_08918CDC, Word);
HOT264(0x08B613D8u, func_08B613D8, Word);
HOT264(0x08909628u, func_08909628, Word);
HOT264(0x08B1D900u, func_08B1D900, Word);
HOT264(0x088C54ACu, func_088C54AC, Word);
HOT264(0x08B1D9C8u, func_08B1D9C8, DoubleWord);
