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

void func_08918CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[20]); G[20] = G[7]; put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put_float(rt, frame.at(0x20u), F[20]); F[20] = F[12]; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); put(rt, G[4] + 8u, 0u); put(rt, G[4] + 0xCu, 0u); if (static_cast<std::int32_t>(G[7]) > 0) { G[17] = G[6]; G[18] = 0u; G[21] = 0x08CF0000u; G[16] = G[4]; do { G[2] = word(rt, G[17]); F[12] = F[20]; G[18] = G[18] + 1u; G[17] = G[17] + 4u; G[4] = G[2]; if (G[2] != 0u) { G[5] = load8(0x08CF212Cu); call_here(rt, ctx, 0x08907A44u); put_float(rt, G[16], F[0]); } G[16] = G[16] + 4u; } while (G[20] != G[18]); } G[2] = G[19]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); F[20] = float_at(rt, frame.at(0x20u)); } }
// 0x08B613D8 (a0 = a code; v0 = a rodata address or 0): the code masked with 0x3D and reduced by 8 indexes the 0x35-entry jump table at 0x08C95E04
// (above it the answer is 0); its ten handlers each return one rodata address, the rest of the table pointing at the handler that returns 0.
void func_08B613D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 index = (ctx.gpr[4] & 0x3Du) - 0x8u;
    if (index >= 0x35u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 results[0x35] = {
        0x08CB5AB4u, 0x0u,        0x0u,        0x0u,        0x08CB5AC8u, 0x0u,        0x0u,        0x0u,        0x08CB5AACu, 0x08CB5AB0u,
        0x0u,        0x0u,        0x08CB5AC0u, 0x08CB5AC4u, 0x0u,        0x0u,        0x08CB5AB8u, 0x0u,        0x0u,        0x0u,
        0x08CB5ACCu, 0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,
        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,        0x0u,
        0x08CB5AACu, 0x0u,        0x0u,        0x0u,        0x08CB5AC0u, 0x0u,        0x0u,        0x0u,        0x08CB5ABCu, 0x0u,
        0x0u,        0x0u,        0x08CB5AA8u,
    };
    ctx.gpr[2] = results[index];
}
// 0x08909628 (a0 = the object; v0 = 1 when the kind at +0xC is 3 or 4, else 0): the object's word +4 is set to 1 and its byte +0x15 cleared; a kind
// below 7 selects one of the jump table's handlers at 0x08C8A578, whose cases touch the byte +1, the byte +0x14, the byte +0x15 and the word +4. A
// kind of 3 or 4 with a non-zero word +0x10 then sets the byte +0x15 and the word +4 to 1.
void func_08909628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 kind = word(rt, object + 0xCu);
    put(rt, object + 0x4u, 0x1u);
    store8(object + 0x15u, 0u);
    if (kind < 0x7u) {
        switch (kind) {
        case 0x1u:
            store8(object + 0x1u, 0u);
            break;
        case 0x2u:
            store8(object + 0x1u, 0x1u);
            break;
        case 0x3u:
        case 0x6u:
            store8(object + 0x14u, 0x1u);
            put(rt, object + 0x4u, 0x2u);
            break;
        case 0x4u:
            put(rt, object + 0x4u, 0x2u);
            store8(object + 0x14u, 0u);
            break;
        case 0x5u:
            store8(object + 0x14u, 0x1u);
            store8(object + 0x15u, 0x1u);
            put(rt, object + 0x4u, 0x1u);
            break;
        default:
            break;
        }
    }
    if (kind - 0x3u >= 0x2u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (word(rt, object + 0x10u) != 0u) {
        store8(object + 0x15u, 0x1u);
        put(rt, object + 0x4u, 0x1u);
    }
    ctx.gpr[2] = 0x1u;
}
void func_08B1D900(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x10u); G[2] = G[4]; G[4] = 0x08D68A94u; put(rt, frame.at(0u), G[16]); G[16] = G[5]; put(rt, frame.at(4u), G[31]); G[5] = G[2]; call_here(rt, ctx, 0x08B1DEB4u); bool done = G[2] == 0u; if (!done) { G[5] = word(rt, G[2]); done = G[5] == 0u; } if (!done) { G[3] = word(rt, G[5]); done = !(G[3] < 6u); } if (!done) { G[2] = G[3] << 2; G[3] = 0x08C932E0u; G[2] = G[2] + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08B1D96Cu: { F[0] = float_at(rt, G[5] + 8u); G[1] = 0x4F000000u; F[1] = std::bit_cast<float>(G[1]); F[2] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(F[0]))); const bool big = F[1] <= F[0]; G[16] = std::bit_cast<std::uint32_t>(F[2]); if (big) { F[0] = F[0] - F[1]; G[2] = 0x80000000u; F[0] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(F[0]))); G[16] = std::bit_cast<std::uint32_t>(F[0]); G[16] = G[16] | G[2]; } break; } case 0x08B1D9A4u: G[4] = word(rt, G[5] + 8u); G[5] = word(rt, G[5] + 0xCu); call_here(rt, ctx, 0x08B6F110u); G[16] = G[2]; break; default: G[16] = word(rt, G[5] + 8u); break; } } G[2] = G[16]; G[16] = word(rt, frame.at(0u)); } }
void func_088C54AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[17] = G[6]; put(rt, frame.at(0u), G[16]); G[16] = G[5]; put(rt, frame.at(8u), G[31]); G[5] = 4u; call_here(rt, ctx, 0x088C541Cu); G[3] = G[2]; [&] { if (G[2] == 0u) { G[2] = 0u; return; } if (G[17] == 0u) { G[2] = 0u; return; } G[6] = word(rt, G[2]); G[2] = static_cast<std::int32_t>(G[6]) < 6 ? 1u : 0u; G[5] = G[3] + 0x40u; if (G[2] != 0u) { G[2] = 0u; return; } G[2] = G[16] < 6u ? 1u : 0u; if (G[5] == 0u) { G[2] = 0u; return; } if (G[2] == 0u) { G[2] = 0u; return; } G[3] = 0x08C89E7Cu; G[2] = (G[16] << 2) + G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x088C5518u: G[2] = 1u; break; case 0x088C5520u: G[2] = 2u; break; case 0x088C5528u: G[2] = 0u; break; case 0x088C5530u: G[2] = G[6] - 3u; break; case 0x088C5538u: G[2] = G[6] - 2u; break; default: G[2] = G[6] - 1u; break; } if (G[4] >= 0x088C5530u && static_cast<std::int32_t>(G[2]) < 0) { G[2] = 0u; return; } G[2] = (G[2] << 6) + G[5]; F[0] = float_at(rt, G[2] + 0x14u); G[2] = 1u; put_float(rt, G[17], F[0]); }(); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
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
