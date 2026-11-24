// Hot batch 276: sixty-four- and sixty-five-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (tree-node inserts, a bucket table teardown, threshold classification, round-robin searches,
// pointer rebasing after a move, handle-guarded byte fields and a float query through virtual calls).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_276 {
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

void func_08B947FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = G[6]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4] + 4u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[22] = 1u; if (G[5] == 0u) { G[22] = 0u; if (G[6] == G[20]) G[22] = 1u; else { G[4] = word(rt, G[7]); G[2] = word(rt, G[6] + 0x10u); G[16] = word(rt, G[4] - 0xCu); G[5] = G[2]; put(rt, frame.at(0u), G[16]); G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(4u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[2] = frame.at(0u); put(rt, frame.at(4u), G[17]); G[16] -= G[17]; G[6] = word(rt, G[2]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; if (static_cast<std::int32_t>(G[16]) < 0) G[22] = 1u; } } G[4] = 0x18u; call_here(rt, ctx, 0x08B35DA0u); G[16] = G[2]; G[2] += 0x10u; G[5] = G[19]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B9432Cu); G[5] = G[19] + 4u; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08B9432Cu); } G[4] = G[22]; G[6] = G[21]; G[7] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08B5F2B8u); G[3] = word(rt, G[18] + 0x14u); G[2] = G[16]; G[3] += 1u; put(rt, G[18] + 0x14u, G[3]); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08C4EA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = G[6]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4] + 4u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[22] = 1u; if (G[5] == 0u) { G[22] = 0u; if (G[6] == G[20]) G[22] = 1u; else { G[4] = word(rt, G[7]); G[2] = word(rt, G[6] + 0x10u); G[16] = word(rt, G[4] - 0xCu); G[5] = G[2]; put(rt, frame.at(0u), G[16]); G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(4u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[2] = frame.at(0u); put(rt, frame.at(4u), G[17]); G[16] -= G[17]; G[6] = word(rt, G[2]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; if (static_cast<std::int32_t>(G[16]) < 0) G[22] = 1u; } } G[4] = 0x18u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[2] += 0x10u; G[5] = G[19]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08C7464Cu); G[5] = G[19] + 4u; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08C7464Cu); } G[4] = G[22]; G[6] = G[21]; G[7] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08B5F2B8u); G[3] = word(rt, G[18] + 0x14u); G[2] = G[16]; G[3] += 1u; put(rt, G[18] + 0x14u, G[3]); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08B379E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[3] = word(rt, G[4] + 8u); G[19] = G[4]; G[4] = word(rt, G[4] + 4u); if (static_cast<std::int32_t>(G[3]) > 0) { G[2] = 0x08CE0000u; G[21] = 0x08CDA8F8u; G[20] = 0u; G[2] = G[20] << 3; while (true) { G[2] += G[4]; G[16] = word(rt, G[2]); if (G[16] != 0u) { G[18] = G[21]; G[3] = word(rt, G[16] + 4u); while (true) { put(rt, G[16], G[18]); G[17] = word(rt, G[16] + 0xCu); if (G[3] != 0u) { G[2] = word(rt, G[3] + 4u); G[4] = G[3]; G[2] -= 1u; put(rt, G[3] + 4u, G[2]); if (G[2] == 0u) { G[2] = word(rt, G[3]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } } G[4] = G[16]; G[16] = G[17]; call_here(rt, ctx, 0x08B64654u); if (G[17] == 0u) break; G[3] = word(rt, G[16] + 4u); } G[3] = word(rt, G[19] + 8u); G[4] = word(rt, G[19] + 4u); } G[20] += 1u; G[2] = static_cast<std::int32_t>(G[20]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; const bool more = G[2] != 0u; G[2] = G[20] << 3; if (!more) break; } } if (G[4] != 0u) call_here(rt, ctx, 0x08B61FC0u); put(rt, G[19] + 0xCu, 0u); put(rt, G[19] + 4u, 0u); put(rt, G[19] + 8u, 0u); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08C2BB8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(8u), G[18]); G[18] = G[4]; G[4] = 0x24u; put(rt, frame.at(0x18u), G[22]); G[22] = G[7] + 4u; put(rt, frame.at(0x14u), G[21]); G[21] = G[6]; put(rt, frame.at(0x10u), G[20]); G[20] = G[18] + 4u; put(rt, frame.at(4u), G[17]); G[17] = G[7]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0u), G[16]); G[23] = 1u; if (G[5] == 0u && G[6] != G[20]) { G[2] = word(rt, G[7]); G[3] = word(rt, G[6] + 0x10u); G[2] = G[2] < G[3] ? 1u : 0u; G[23] = G[2] != 0u ? 1u : 0u; } G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[19] = G[2] + 0x14u; G[2] += 0x10u; G[5] = G[17]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08A6972Cu); G[2] = word(rt, G[17] + 4u); G[3] = word(rt, G[22] + 4u); G[5] = G[17] + 0xCu; put(rt, G[16] + 0x14u, G[2]); G[4] = G[16] + 0x1Cu; put(rt, G[19] + 4u, G[3]); call_here(rt, ctx, 0x08B88F44u); G[2] = word(rt, G[22] + 0xCu); put(rt, G[19] + 0xCu, G[2]); } G[4] = G[23]; G[6] = G[21]; G[7] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08B5F2B8u); G[3] = word(rt, G[18] + 0x14u); G[2] = G[16]; G[3] += 1u; put(rt, G[18] + 0x14u, G[3]); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08852598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[17] = frame.at(8u); G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A6608u); G[16] = word(rt, G[18]); G[17] = G[2]; if (G[16] == 0u) { G[4] = 0x08C9D394u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); store8(G[17] + 0x138u, G[2]); } else { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A65E8u); G[5] = sext(load8(G[2] + 0x138u), 8); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0885269C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[17] = frame.at(8u); G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A6608u); G[16] = word(rt, G[18]); G[17] = G[2]; if (G[16] == 0u) { G[4] = 0x08C9D394u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); store8(G[17] + 0x139u, G[2]); } else { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A65E8u); G[5] = sext(load8(G[2] + 0x139u), 8); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0886BFC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[31]); put(rt, frame.at(4u), G[17]); G[17] = 0x08CA0000u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = word(rt, G[4] + 4u); G[4] += 0x88u; call_here(rt, ctx, 0x0887D4E4u); G[5] = 0x08C9F6C0u; G[4] = G[16]; G[6] = 0x08C9F6B4u; call_here(rt, ctx, 0x0886B9A4u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x0886E088u); G[4] = G[16]; G[6] = 0x08C9F6B4u; G[5] = 0x08C9F6BCu; call_here(rt, ctx, 0x0886B9A4u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x0886EBD0u); for (const std::uint32_t s : {0x08C9F6C4u, 0x08C9F6C8u, 0x08C9F6CCu, 0x08C9F6D0u, 0x08C9F6D4u, 0x08C9F6D8u}) { G[4] = G[16]; G[5] = s; call_here(rt, ctx, 0x0886BF44u); } G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[3] + 0x158u); G[4] = G[16]; if (G[2] == 0u) { call_here(rt, ctx, 0x0886BC14u); G[3] = word(rt, G[16] + 4u); } G[2] = 5u; put(rt, G[3] + 0x158u, G[2]); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
// 0x088BD574 (a0 = the object; f0 = a float): with the byte at +0x13 of the block at +0x24 clear the answer is f1, i.e. 0.0. Otherwise the float at +0x1C of that
// block (kept in f20) is compared with the float at +0x6C4 of the object at +8: when it is not above it, the answer is 0.0; when it is, 0x088C50B8(sp,
// object +0x1C, byte +0x10 of the block) and 0x088BBDB8(object) run (its result kept in s2, object +0x18 read into s3 first), then 0x088B1A0C(object's word +0)
// with the object's word at +0x14 already read into s0 and the float at +0x6C4 of the object at +8 into f21. Its result is a mode (2 gives 1, 3 gives 2,
// anything else 0) passed with (s2, object + 4, s3, word +0x14) and f12 = f21, f13 = f20 to 0x088BD288, whose float answer 0x088BD23C(s2) turns into the result.
void func_088BD574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {53u, 0x34u}, {52u, 0x30u}});
    const u32 link = self + 0x4u;
    ctx.gpr[17] = link;
    ctx.gpr[16] = self;
    ctx.fpr[1] = 0.0f;
    const u32 block = word(rt, self + 0x24u);
    const u32 enabled = load8(block + 0x13u);
    ctx.fpr[20] = float_at(rt, block + 0x1Cu);
    if (enabled == 0u) {
        ctx.fpr[0] = ctx.fpr[1];
        return;
    }
    if (float_at(rt, word(rt, link + 0x4u) + 0x6C4u) <= ctx.fpr[20]) {
        ctx.fpr[0] = ctx.fpr[1];
        return;
    }
    const u32 trigger = load8(block + 0x10u);
    const u32 related = word(rt, self + 0x18u);
    ctx.gpr[19] = related;
    call_set(rt, ctx, 0x088C50B8u, {frame.at(0x0u), word(rt, self + 0x1Cu), trigger});
    const u32 state = call_set(rt, ctx, 0x088BBDB8u, {self});
    ctx.gpr[18] = state;
    const u32 inner = word(rt, self);
    const u32 next = word(rt, self + 0x14u);
    ctx.gpr[16] = next;
    ctx.fpr[21] = float_at(rt, word(rt, link + 0x4u) + 0x6C4u);
    const u32 answer = call_set(rt, ctx, 0x088B1A0Cu, {inner});
    u32 mode = 0u;
    if (answer == 2u) mode = 1u;
    else if (answer == 3u) mode = 2u;
    ctx.fpr[13] = ctx.fpr[20];
    ctx.fpr[12] = ctx.fpr[21];
    call_set(rt, ctx, 0x088BD288u, {state, link, related, next, mode});
    ctx.fpr[12] = ctx.fpr[0];
    call_set(rt, ctx, 0x088BD23Cu, {state});
}
void func_088CF6D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(4u), G[17]); G[17] = G[4]; put(rt, frame.at(8u), G[31]); put(rt, frame.at(0u), G[16]); put_float(rt, frame.at(0x10u), F[20]); F[20] = float_at(rt, G[4] + 0x13Cu); call_here(rt, ctx, 0x088CEF88u); F[1] = F[0] / F[20]; G[1] = 0x3D4CCCCDu; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[17] + 0xE8u, F[20]); if (F[1] < F[0]) put(rt, G[17] + 0xE0u, 0u); else { G[1] = 0x3DCCCCCDu; F[0] = std::bit_cast<float>(G[1]); G[2] = 1u; if (F[1] < F[0]) put(rt, G[17] + 0xE0u, G[2]); else { G[1] = 0x3E19999Au; F[0] = std::bit_cast<float>(G[1]); G[2] = 2u; if (F[1] < F[0]) put(rt, G[17] + 0xE0u, G[2]); else { G[1] = 0x3E4CCCCDu; F[0] = std::bit_cast<float>(G[1]); G[2] = 4u; if (F[1] < F[0]) G[2] = 3u; put(rt, G[17] + 0xE0u, G[2]); } } } G[4] = G[17]; call_here(rt, ctx, 0x088CF6D0u); G[4] = G[17]; G[16] = G[2] << 2; call_here(rt, ctx, 0x088CEF98u); F[1] = float_at(rt, G[17] + 0xD0u); F[2] = float_at(rt, G[17] + 0xD4u); G[16] += G[17]; put_float(rt, G[16] + 0xECu, F[0]); F[1] = F[1] + F[2]; put(rt, G[17] + 0xD4u, 0u); put_float(rt, G[17] + 0xD0u, F[1]); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); F[20] = float_at(rt, frame.at(0x10u)); } } }
// 0x088DE5F4 (a0 = the object; v0 = the slot taken, or -1): 0x088DE24C(object, 1, 0) runs first and 0x088D9F80(object)'s count drives the walk: from the
// object's word +0xC4, each step adds one to it and takes the remainder modulo the count as a slot at object's word +0xC0, (slot * 760) bytes on.
// 0x088E7F6C(entry) accepting it, or with the object's word +0x640 set 0x088E4B48(that) accepting it, skips to the next turn. Otherwise 0x088E8A84(entry, 0)
// then 0x088E7F1C(entry) take it, the object's +0xC4 word becomes (slot + 1) modulo the count and the answer is the slot; after the count turns (or with no
// count at all) it is -1. 0x088D9BBC(object, 0, 0) closes.
void func_088DE5F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {31u, 0x14u}, {18u, 0x8u}, {20u, 0x10u}, {17u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[19] = self;
    call_set(rt, ctx, 0x088DE24Cu, {self, 1u, 0u});
    const u32 count = call_set(rt, ctx, 0x088D9F80u, {self});
    ctx.gpr[18] = count;
    u32 found = 0xFFFFFFFFu;
    if (static_cast<s32>(count) > 0) {
        u32 cursor = word(rt, self + 0xC4u);
        ctx.gpr[20] = 0u;
        for (;;) {
            const u32 base = word(rt, self + 0xC0u);
            const u32 slot = static_cast<u32>(static_cast<s32>(ctx.gpr[20] + cursor) % static_cast<s32>(count));
            ctx.gpr[20] += 1u;
            ctx.gpr[16] = slot;
            const u32 entry = base + (((slot << 7u) - (slot << 5u)) - slot) * 8u;
            ctx.gpr[17] = entry;
            const u32 taken = call_set(rt, ctx, 0x088E7F6Cu, {entry});
            ctx.gpr[5] = slot;
            bool skip = taken != 0u;
            if (!skip) {
                const u32 other = word(rt, self + 0x640u);
                if (other != 0u) skip = call_set(rt, ctx, 0x088E4B48u, {other}) != 0u;
            }
            if (!skip) {
                call_set(rt, ctx, 0x088E8A84u, {entry, 0u});
                call_set(rt, ctx, 0x088E7F1Cu, {entry});
                put(rt, self + 0xC4u, static_cast<u32>(static_cast<s32>(slot + 1u) % static_cast<s32>(count)));
                found = slot;
                break;
            }
            if (count == ctx.gpr[20]) break;
            cursor = word(rt, self + 0xC4u);
        }
    }
    call_set(rt, ctx, 0x088D9BBCu, {self, 0u, 0u});
    ctx.gpr[2] = found;
}
// 0x088DF220 (a0 = the object; v0 = 1 when one entry was handled, else 0): the list at the object's +0x65C holds entries of 0x2F8 bytes between the words at
// +0x65C and +0x660 (their distance / 16 is a count). When that count is above 0x088DAA8C(object)'s answer nothing is walked. Otherwise every entry below
// 0x088D9F80(object) is looked at: one with its byte +0x11 set just goes through 0x088E8108(entry); one with +0x10 clear asks 0x088E7F14(entry) for a key,
// which (unless it is -1) indexes an entry at [object + 0x65C] + (key << 4) that 0x088E884C(entry, indexed, key, object + 0x66C) takes — a non-zero answer
// makes the result 1.
void func_088DF220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {17u, 0x4u}, {31u, 0x1Cu}, {22u, 0x18u}, {21u, 0x14u}, {19u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}});
    const u32 list = self + 0x65Cu;
    ctx.gpr[20] = list;
    ctx.gpr[17] = self;
    const u32 span = sra(word(rt, list + 0x4u) - word(rt, self + 0x65Cu), 4);
    ctx.gpr[16] = span;
    const u32 count = call_set(rt, ctx, 0x088DAA8Cu, {self});
    if (static_cast<s32>(span) < static_cast<s32>(count)) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 found = 0u;
    ctx.gpr[21] = found;
    u32 index = 0u;
    ctx.gpr[18] = index;
    u32 offset = 0u;
    ctx.gpr[19] = offset;
    ctx.gpr[22] = 0xFFFFFFFFu;
    while (static_cast<s32>(index) < static_cast<s32>(call_set(rt, ctx, 0x088D9F80u, {self}))) {
        const u32 element = word(rt, self + 0xC0u) + offset;
        ctx.gpr[16] = element;
        if (load8(element + 0x11u) != 0u) {
            ++index;
            ctx.gpr[18] = index;
            call_set(rt, ctx, 0x088E8108u, {element});
            offset += 0x2F8u;
            ctx.gpr[19] = offset;
            continue;
        }
        if (load8(element + 0x10u) != 0u) {
            ++index;
            ctx.gpr[18] = index;
            offset += 0x2F8u;
            ctx.gpr[19] = offset;
            continue;
        }
        const u32 key = call_set(rt, ctx, 0x088E7F14u, {element});
        ctx.gpr[6] = key;
        if (key != 0xFFFFFFFFu) {
            const u32 indexed = word(rt, list) + (key << 4u);
            if (call_set(rt, ctx, 0x088E884Cu, {element, indexed, key, list + 0xCu}) != 0u) {
                found = 1u;
                ctx.gpr[21] = found;
            }
        }
        ++index;
        ctx.gpr[18] = index;
        offset += 0x2F8u;
        ctx.gpr[19] = offset;
    }
    ctx.gpr[2] = found;
}
void func_0890815C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(0u), G[16]); G[2] = 0x34504552u; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); G[3] = word(rt, G[4]); G[16] = G[4]; if (G[3] == G[2]) { G[2] = word(rt, G[4] + 4u); G[6] = word(rt, G[4] + 0x24u); G[7] = word(rt, G[4] + 0x20u); G[18] = G[4] - G[2]; G[6] += G[18]; put(rt, G[16] + 4u, G[4]); G[19] = 0u; G[17] = 0u; put(rt, G[4] + 0x24u, G[6]); if (G[7] != 0u) { G[4] = G[17] + G[6]; while (true) { G[5] = G[18]; call_here(rt, ctx, 0x08908098u); G[7] = word(rt, G[16] + 0x20u); G[19] += 1u; G[2] = G[19] < G[7] ? 1u : 0u; G[17] += 0x7Cu; if (G[2] == 0u) break; G[6] = word(rt, G[16] + 0x24u); G[4] = G[17] + G[6]; } } G[2] = word(rt, G[16] + 0x30u); G[3] = word(rt, G[16] + 0x38u); G[4] = word(rt, G[16] + 0x40u); G[2] = G[18] + G[2]; G[3] = G[18] + G[3]; G[4] = G[18] + G[4]; put(rt, G[16] + 0x30u, G[2]); put(rt, G[16] + 0x38u, G[3]); put(rt, G[16] + 0x40u, G[4]); put(rt, G[16] + 0x18u, 0u); if (G[7] != 0u) { G[6] = word(rt, G[16] + 0x24u); G[4] = 0u; G[3] = 0u; G[2] = G[3] + G[6]; while (true) { F[1] = float_at(rt, G[2] + 8u); F[0] = float_at(rt, G[16] + 0x18u); G[4] += 1u; G[2] = G[4] < G[7] ? 1u : 0u; G[3] += 0x7Cu; if (F[0] < F[1]) put_float(rt, G[16] + 0x18u, F[1]); const bool more = G[2] != 0u; G[2] = G[3] + G[6]; if (!more) break; } } } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_276
} // namespace gt::game

#define HOT276(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_276::name, gt::ReturnKind::kind)
// HOT276(0x08B947FCu, func_08B947FC, Word);  // no comparable synthetic case yet
// HOT276(0x08C4EA54u, func_08C4EA54, Word);  // no comparable synthetic case yet
HOT276(0x08B379E0u, func_08B379E0, Word);
HOT276(0x08C2BB8Cu, func_08C2BB8C, Word);
// HOT276(0x08852598u, func_08852598, Word);  // no comparable synthetic case yet
// HOT276(0x0885269Cu, func_0885269C, Word);  // no comparable synthetic case yet
// HOT276(0x0886BFC0u, func_0886BFC0, Word);  // no comparable synthetic case yet
HOT276(0x088BD574u, func_088BD574, Float);
HOT276(0x088CF6D8u, func_088CF6D8, Word);
HOT276(0x088DE5F4u, func_088DE5F4, Word);
HOT276(0x088DF220u, func_088DF220, Word);
HOT276(0x0890815Cu, func_0890815C, Word);
