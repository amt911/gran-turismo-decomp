// Hot batch 236: fifty-six-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (handle-guarded calls through temporaries, flag scans, block copies, destructors and float copies).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_236 {
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
    if (!(value >= -2367483648.0f && value < 2367483648.0f)) return 0x7FFFFFFF;
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

void func_0881E8E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(4u); put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; G[4] = G[19]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C98F48u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); G[4] = 0x08C98EFCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x0881AF04u); G[5] = G[2]; G[4] = frame.at(0u); G[6] = 0u; call_here(rt, ctx, 0x08857EE4u); G[4] = G[20]; if (G[20] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0882DE9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; G[4] = G[17]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDF8u); G[16] = word(rt, G[18]); G[4] = 0x08C9A2B4u; G[17] = G[2]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088D76ECu); G[4] = frame.at(0u); G[5] = 0u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0882FA68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); G[4] = 0x08C9A2B4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[20]; G[5] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; call_here(rt, ctx, 0x0885EDF8u); G[16] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); store8(G[16] + 0x5BAu, G[18]); G[5] = 0u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_088313DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; G[4] = G[17]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EDECu); G[16] = word(rt, G[18]); G[4] = 0x08C9A2B4u; G[17] = G[2]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088DF13Cu); G[5] = word(rt, G[2] + 0x7Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0886B6F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); G[2] = 1u; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); if (G[4] == 1u) { G[2] = 0xFFFFu; if (G[5] == G[2]) { G[18] = 0x08CE0000u; G[2] = 0x08CE0000u; G[17] = 0x08CE2E88u; G[16] = 0x08CE2E58u; G[4] = G[16]; do { G[16] += 0xCu; call_here(rt, ctx, 0x08B93A70u); G[4] = G[16]; } while (G[16] != G[17]); G[2] = 0x08CE2E58u; put(rt, G[2] + 0x30u, 0u); G[4] = 0x08CE2E30u; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); goto tail; } } else { G[2] = 0xFFFFu; if (G[4] == 0u && G[5] == G[2]) { G[4] = 0x08CE2E30u; call_here(rt, ctx, 0x08B28DC4u); G[2] = 0x08CE2E88u; G[18] = 0x08CE2E58u; G[3] = G[2]; G[17] = G[2] - 0xCu; do { G[2] = word(rt, G[17]); G[16] = G[3] - 0xCu; G[4] = G[16]; G[3] = word(rt, G[2]); G[17] -= 0xCu; call_here(rt, ctx, G[3]); G[3] = G[16]; } while (G[16] != G[18]); } } G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } return; tail: jump(ctx, 0x08B28DCCu); } }
void func_08879CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x24u), G[17]); G[17] = G[4]; put(rt, frame.at(0x28u), G[31]); G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B77BFCu); G[16] = G[2]; G[2] = word(rt, frame.at(0xCu)); G[5] = 2u; if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = word(rt, G[17]); call_here(rt, ctx, 0x0886B4E8u); G[16] = G[2]; } G[4] = word(rt, G[17]); call_here(rt, ctx, 0x0886B080u); G[4] = G[2]; G[5] = G[16]; G[6] = 0u; G[7] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 8u); call_here(rt, ctx, G[3]); } G[4] = G[17]; call_here(rt, ctx, 0x08875524u); G[4] = G[2]; G[5] = 0u; call_here(rt, ctx, 0x0887BEF4u); G[4] = word(rt, G[17]); call_here(rt, ctx, 0x0886BFC0u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D75298u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B93554u); } G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_088B17A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x100u); put(rt, frame.at(0xD4u), G[17]); G[17] = G[6]; put(rt, frame.at(0xD0u), G[16]); G[16] = G[4]; put_float(rt, frame.at(0xF0u), ctx.fpr[20]); ctx.fpr[20] = ctx.fpr[12]; put(rt, frame.at(0xE0u), G[31]); put(rt, frame.at(0xDCu), G[19]); put(rt, frame.at(0xD8u), G[18]); G[2] = word(rt, G[4] + 8u); G[4] = G[5]; if (G[2] == 0u) goto zero; G[2] = load8(G[16] + 0x328u); { const bool off = G[2] == 0u; G[2] = 0u; if (off) goto done; } G[18] = G[16] + 0x10u; call_here(rt, ctx, 0x088B16A4u); G[8] = G[2]; G[2] <<= 2; G[2] += G[16]; G[3] = word(rt, G[2] + 0x2CCu); G[19] = frame.at(0x10u); G[4] = G[19]; G[6] = frame.at(0u); G[7] = G[18]; G[5] = G[3]; if (G[3] == 0u) goto zero; G[2] = 0xFFFFFFFFu; put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0u), G[17]); put(rt, frame.at(4u), 0u); put(rt, frame.at(8u), 0u); call_here(rt, ctx, 0x088BB44Cu); ctx.fpr[12] = ctx.fpr[20]; G[4] = word(rt, G[16] + 8u); G[5] = G[19]; G[6] = G[18]; G[7] = frame.at(0u); call_here(rt, ctx, 0x088BDA50u); G[1] = 0x40666666u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[0] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0]))); G[2] = bits_of(ctx.fpr[0]); goto done; zero: G[2] = 0u; done: G[19] = word(rt, frame.at(0xDCu)); G[18] = word(rt, frame.at(0xD8u)); G[17] = word(rt, frame.at(0xD4u)); G[16] = word(rt, frame.at(0xD0u)); ctx.fpr[20] = float_at(rt, frame.at(0xF0u)); } } }
// 0x088F3484 (a0 = the owner, a1 / a2 = two corner indices, a3 = the 12-byte row output, t0 = the 8-byte pair output, t1 = a 5-float block, t2 = another
// 5-float block, t3 = 12-byte corner rows; v0 = the number of rows written): the two 12-byte rows at a3 take the first three floats of t2 and of t1 (a3 and
// a3 + 0xC), the pairs at t0 take t2's fourth and fifth floats (t0) and t1's (t0 + 8). Then (a1 + 1) & 3 up to (a2 + 1) & 3, exclusive, each index copies
// its 12-byte row from t3 to the rows after them and its 8-byte pair from the owner's word +0x1A4 to the pairs after them; the count starts at 2 and grows.
void func_088F3484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    u32 first = (ctx.gpr[5] + 1u) & 3u;
    const u32 second = (ctx.gpr[6] + 1u) & 3u;
    const u32 rows = ctx.gpr[7];
    const u32 pairs = ctx.gpr[8];
    const u32 block = ctx.gpr[9];
    const u32 first_block = ctx.gpr[10];
    const u32 corner_rows = ctx.gpr[11];
    put_float(rt, rows, float_at(rt, first_block));
    put_float(rt, rows + 0x4u, float_at(rt, first_block + 0x4u));
    put_float(rt, rows + 0x8u, float_at(rt, first_block + 0x8u));
    put_float(rt, pairs, float_at(rt, first_block + 0xCu));
    put_float(rt, pairs + 0x4u, float_at(rt, first_block + 0x10u));
    put_float(rt, rows + 0xCu, float_at(rt, block));
    put_float(rt, rows + 0x10u, float_at(rt, block + 0x4u));
    put_float(rt, rows + 0x14u, float_at(rt, block + 0x8u));
    put_float(rt, pairs + 0x8u, float_at(rt, block + 0xCu));
    put_float(rt, pairs + 0xCu, float_at(rt, block + 0x10u));
    u32 row_out = rows + 0x18u;
    u32 pair_out = pairs + 0x10u;
    u32 count = 2u;
    while (first != second) {
        const u32 source = corner_rows + first * 12u;
        put_float(rt, row_out, float_at(rt, source));
        put_float(rt, row_out + 0x4u, float_at(rt, source + 0x4u));
        put_float(rt, row_out + 0x8u, float_at(rt, source + 0x8u));
        row_out += 0xCu;
        const u32 table = word(rt, owner + 0x1A4u) + first * 8u;
        put_float(rt, pair_out, float_at(rt, table));
        put_float(rt, pair_out + 0x4u, float_at(rt, table + 0x4u));
        pair_out += 0x8u;
        ++count;
        first = (first + 1u) & 3u;
    }
    ctx.gpr[2] = count;
}
void func_089192D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); G[6] = G[7]; put(rt, frame.at(0u), G[16]); put(rt, frame.at(4u), G[31]); G[16] = G[4]; call_here(rt, ctx, 0x089261BCu); G[4] = word(rt, G[16] + 0x1E0u); G[5] = G[2] != 0u ? 3u : 0xCu; call_here(rt, ctx, 0x089277A4u); G[8] = G[2] + 0x1E0u; G[6] = G[2] + 0x10u; G[7] = G[16] + 0x10u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); ctx.fpr[1] = float_at(rt, G[16] + 0x50u); G[2] = G[16] + 0x50u; G[3] = G[16] + 0x1E4u; put_float(rt, G[16] + 0x1E4u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[2] + 4u); put_float(rt, G[3] + 4u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[2] + 8u); put_float(rt, G[3] + 8u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[2] + 0xCu); put_float(rt, G[3] + 0xCu, ctx.fpr[0]); G[16] = word(rt, frame.at(0u)); } } }
void func_0892B0A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(8u), G[18]); G[18] = G[5]; G[5] = 0x35u; put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; G[4] = G[18]; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); call_here(rt, ctx, 0x08B21410u); G[4] = G[18]; G[5] = 0x38u; G[17] = G[2]; call_here(rt, ctx, 0x08B21410u); G[16] = word(rt, G[19]); G[6] = word(rt, G[19] + 4u); G[3] = G[17] | 8u; if (G[2] != 0u) G[17] = G[3]; if (G[6] != G[16]) { G[20] = 6u; G[21] = 0xBu; G[2] = load8(G[16]); for (;;) { G[4] = G[18]; G[5] = 0x36u; if (G[2] == G[20]) { call_here(rt, ctx, 0x08B21410u); G[6] = word(rt, G[19] + 4u); if (G[2] != 0u) G[17] |= 2u; G[16] += 2u; } else if (G[2] == G[21]) { G[5] = 0x39u; call_here(rt, ctx, 0x08B21410u); G[6] = word(rt, G[19] + 4u); if (G[2] != 0u) G[17] |= 4u; G[16] += 2u; } else { G[16] += 2u; } if (G[6] == G[16]) break; G[2] = load8(G[16]); } } G[2] = G[17]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08938B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x80u), G[16]); G[16] = frame.at(0x40u); G[8] = 1u; put(rt, frame.at(0x88u), G[18]); G[18] = G[4]; G[4] = G[16]; put(rt, frame.at(0x8Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x84u), G[17]); put(rt, frame.at(0x90u), G[31]); G[17] = G[6]; call_here(rt, ctx, 0x08B10170u); G[6] = frame.at(0u); G[7] = frame.at(0x70u); do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[6], G[2]); G[16] += 0x10u; G[6] += 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 8u, G[4]); put(rt, G[6] - 4u, G[5]); } while (G[16] != G[7]); put(rt, G[18] + 8u, G[19]); G[7] = 0xFFFFFFFFu; put(rt, G[18] + 4u, G[17]); G[2] = word(rt, G[16] + 8u); G[3] = word(rt, G[16]); G[4] = word(rt, G[16] + 4u); G[5] = word(rt, G[17]); put(rt, G[6] + 8u, G[2]); G[2] = 0x42445447u; put(rt, G[6], G[3]); put(rt, G[6] + 4u, G[4]); if (G[5] == G[2]) { G[5] = G[17]; G[4] = G[18]; call_here(rt, ctx, 0x08938AA4u); G[2] = word(rt, G[18] + 4u); G[4] = G[18] + 0x28u; G[6] = 3u; G[5] = word(rt, G[2] + 0xCu); call_here(rt, ctx, 0x0893A31Cu); G[7] = word(rt, G[18] + 8u); } G[19] = word(rt, frame.at(0x8Cu)); G[18] = word(rt, frame.at(0x88u)); G[17] = word(rt, frame.at(0x84u)); G[16] = word(rt, frame.at(0x80u)); G[2] = G[7]; } } }
void func_08997A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = 0x08CCBE20u; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, G[4], G[2]); G[2] = 0x08CCC078u; G[4] += 0x184u; put(rt, G[17] + 0x38u, G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x180u; call_here(rt, ctx, 0x08A5B9ECu); G[3] = G[17] + 0x158u; G[2] = G[17] + 0x170u; if (G[3] == 0u) goto members; G[16] = G[17] + 0x164u; G[18] = 2u; if (G[3] == G[2]) { G[4] = word(rt, G[17] + 0x154u); goto release; } element: G[4] = word(rt, G[16]); G[5] = word(rt, G[16] + 4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF0F4Cu); G[2] = word(rt, G[16]); G[5] = word(rt, G[16] + 8u); G[6] = 4u; G[4] = G[2]; G[5] -= G[2]; G[5] = sra(G[5], 2); G[5] <<= 2; if (G[2] != 0u) { G[18] -= 1u; call_here(rt, ctx, 0x08A5BC54u); } else { G[18] -= 1u; } { const bool last = G[18] == 0u; G[16] -= 0xCu; if (!last) goto element; } members: G[4] = word(rt, G[17] + 0x154u); release: if (G[4] != 0u) call_here(rt, ctx, 0x08A5B930u); G[4] = G[17]; call_here(rt, ctx, 0x08994D78u); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A4FB80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x120u); put(rt, frame.at(0x114u), G[17]); put(rt, frame.at(0x110u), G[16]); put(rt, frame.at(0x118u), G[31]); G[17] = G[4]; call_here(rt, ctx, 0x08A4F8B4u); G[16] = G[2]; G[4] = 0u; if (G[2] != 0u) { G[4] = G[2]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C7AC78u); G[4] = 0u; if (G[2] == 0u) { G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A4F76Cu); ctx.fpr[0] = float_at(rt, frame.at(0x30u)); ctx.fpr[1] = float_at(rt, frame.at(0x34u)); G[6] = load16(frame.at(0x50u)); ctx.fpr[2] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0]))); ctx.fpr[1] = -ctx.fpr[1]; G[7] = G[6] & 0xFFu; G[5] = G[7] & 3u; G[2] = bits_of(ctx.fpr[2]); ctx.fpr[0] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[1]))); G[4] = G[2] + 1u; G[2] = load16(frame.at(0x52u)); store8(G[17] + 6u, G[4]); store8(G[17] + 5u, G[2]); G[2] = bits_of(ctx.fpr[0]); store8(G[17] + 4u, G[6]); G[3] = G[2] + 1u; store8(G[17] + 7u, G[3]); if (G[5] != 0u) { G[2] = G[7] + 1u; do { G[7] = G[2] & 0xFFu; G[3] = G[7] & 3u; G[2] = G[7] + 1u; } while (G[3] != 0u); store8(G[17] + 4u, G[7]); } G[3] = load8(G[17] + 5u); G[2] = G[7] >> 1; G[4] = 1u; { const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[2] = ctx.lo; G[2] += 0xFu; G[2] &= ~0xFu; store16(G[17] + 0x12u, G[2]); } } G[17] = word(rt, frame.at(0x114u)); G[16] = word(rt, frame.at(0x110u)); G[2] = G[4]; } } }
void func_08AB4168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x18u), G[18]); G[4] = 0x08CB3D0Cu; G[18] = frame.at(8u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x10u), G[16]); G[16] = word(rt, G[6]); G[17] = G[5]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[5] = G[17]; G[4] = G[20]; call_here(rt, ctx, 0x08AB4090u); G[5] = G[20]; G[4] = G[18]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = G[18]; G[4] = G[19]; call_here(rt, ctx, 0x08A76ABCu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[19]; G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08ACA674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x5Cu), G[19]); G[4] = G[5]; G[19] = G[7]; put(rt, frame.at(0x58u), G[18]); G[18] = G[6]; put(rt, frame.at(0x50u), G[16]); G[16] = G[5]; put(rt, frame.at(0x64u), G[31]); put(rt, frame.at(0x60u), G[20]); put(rt, frame.at(0x54u), G[17]); if (G[5] == 0u) { G[2] = 0x80000000u; G[17] = G[2] | 0x1000u; goto result; } call_here(rt, ctx, 0x08ACA4BCu); { const bool absent = G[2] == 0u; G[2] = 0x80000000u; G[17] = G[2] | 0x1000u; if (absent) goto result; } if (G[18] == 0u || G[19] == 0u) goto result; G[20] = frame.at(4u); G[4] = G[16] + 8u; G[5] = G[20]; G[6] = 0x40u; put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x08ACA30Cu); G[17] = G[2]; G[4] = G[16] + 0x22Cu; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) < 0) goto result; call_here(rt, ctx, 0x08ACA438u); G[17] = G[2]; G[4] = G[18]; G[5] = G[19]; G[6] = 0x08CB49A4u; G[7] = G[20]; if (static_cast<std::int32_t>(G[2]) < 0) goto result; G[8] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08B3767Cu); G[2] = G[17]; goto done; result: G[2] = G[17]; done: G[20] = word(rt, frame.at(0x60u)); G[19] = word(rt, frame.at(0x5Cu)); G[18] = word(rt, frame.at(0x58u)); G[17] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x50u)); } } }
void func_08AE2F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[31]); G[2] = 0x08D40000u; G[7] = 0x08D40944u; G[3] = word(rt, G[7] + 4u); G[5] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[5] = G[3]; G[3] = G[5] + 0x14u; if (G[5] != 0u) { G[6] = word(rt, G[5]); G[2] = 0xFFFFFFFFu; put(rt, G[3] + 8u, G[2]); G[2] = 1u; G[7] -= 0x10u; put(rt, G[3] + 0xCu, 0u); put(rt, G[3] + 4u, G[3]); put(rt, G[5] + 0x14u, G[3]); put(rt, G[5] + 0x10u, G[4]); put(rt, G[5] + 0xCu, G[2]); if (G[6] != G[5]) { G[2] = word(rt, G[5] + 4u); put(rt, G[6] + 4u, G[2]); G[3] = word(rt, G[5] + 4u); put(rt, G[5] + 4u, G[5]); put(rt, G[3], G[6]); put(rt, G[5], G[5]); } G[2] = 0x08D40000u; G[3] = word(rt, 0x08D40934u); put(rt, G[5] + 4u, G[7]); put(rt, G[3] + 4u, G[5]); G[2] = word(rt, G[5] + 4u); put(rt, G[5], G[3]); G[4] = word(rt, G[5] + 8u); put(rt, G[2], G[5]); } else { G[2] = 0x08CE0000u; G[3] = word(rt, 0x08CDFB3Cu); G[5] = 0x08C9145Cu; G[7] = 0x08C914A4u; G[3] &= 2u; G[6] = 0x10Cu; if (G[3] != 0u) { G[2] = 0x08CE0000u; G[4] = word(rt, 0x08CDFB44u); call_here(rt, ctx, 0x08AD83B8u); } G[4] = 0xFFFFFFFFu; } G[2] = G[4]; } } }
void func_08B16CC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x90u); put(rt, frame.at(0x84u), G[17]); G[17] = G[5]; G[6] = G[17]; put(rt, frame.at(0x80u), G[16]); G[16] = frame.at(0x40u); G[5] = G[4]; put(rt, frame.at(0x88u), G[31]); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0xACu); G[4] = G[16]; call_here(rt, ctx, G[3]); G[8] = frame.at(0u); G[6] = frame.at(0x70u); do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[8], G[2]); G[16] += 0x10u; G[8] += 0x10u; put(rt, G[8] - 0xCu, G[3]); put(rt, G[8] - 8u, G[4]); put(rt, G[8] - 4u, G[5]); } while (G[16] != G[6]); G[2] = word(rt, G[16] + 8u); G[3] = word(rt, G[16]); G[4] = word(rt, G[16] + 4u); G[6] = G[17] + 0xA8u; put(rt, G[8] + 8u, G[2]); G[7] = frame.at(0u); G[9] = frame.at(0x30u); put(rt, G[8], G[3]); put(rt, G[8] + 4u, G[4]); do { G[2] = word(rt, G[7]); G[3] = word(rt, G[7] + 4u); G[4] = word(rt, G[7] + 8u); G[5] = word(rt, G[7] + 0xCu); put(rt, G[6], G[2]); G[7] += 0x10u; G[6] += 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 8u, G[4]); put(rt, G[6] - 4u, G[5]); } while (G[7] != G[9]); G[2] = word(rt, G[7] + 8u); G[3] = word(rt, G[7]); G[4] = word(rt, G[7] + 4u); put(rt, G[6] + 8u, G[2]); put(rt, G[6], G[3]); put(rt, G[6] + 4u, G[4]); G[17] = word(rt, frame.at(0x84u)); G[16] = word(rt, frame.at(0x80u)); } } }
void func_08B2F468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); G[2] = 1u; put(rt, frame.at(4u), G[31]); G[3] = 0xFFFFFFFFu; G[5] = 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[6] = 0x80u; store8(G[4] + 6u, 1u); G[4] = 0u; put(rt, G[16] + 0x12A8u, G[3]); put(rt, G[16] + 0xCu, G[3]); put(rt, G[16] + 0xA0u, 0u); put(rt, G[16] + 0xA4u, 0u); put(rt, G[16] + 0xA8u, 0u); put(rt, G[16] + 0xACu, 0u); put(rt, G[16] + 0x12B0u, 0u); G[4] = G[16] + 0x1Cu; put(rt, G[16] + 0x12B4u, 0u); store8(G[16] + 5u, 0u); store8(G[16] + 7u, 0u); store8(G[16] + 8u, 0u); put(rt, G[16] + 0x10u, 0u); put(rt, G[16] + 0x8F8u, 0u); call_here(rt, ctx, 0x08B36018u); G[4] = G[16] + 0xB0u; G[5] = 0u; G[6] = 0x848u; call_here(rt, ctx, 0x08B36018u); G[4] = G[16] + 0x928u; G[5] = 0u; G[6] = 0x980u; call_here(rt, ctx, 0x08B36018u); G[4] = G[16] + 0x12B8u; G[5] = 0u; G[6] = 0x8300u; call_here(rt, ctx, 0x08B36018u); G[4] = 0x95B8u; G[4] = G[16] + G[4]; G[5] = 0u; G[6] = 0x80u; call_here(rt, ctx, 0x08B36018u); G[4] = G[16]; call_here(rt, ctx, 0x08B2F44Cu); G[2] = 0x10000u; G[16] += G[2]; G[3] = 0u; G[2] = 0u; store8(G[16] - 0x69B8u, 0u); put(rt, G[16] - 0x69C0u, 0u); put(rt, G[16] - 0x69BCu, 0u); G[16] = word(rt, frame.at(0u)); } } }
void func_08B37B44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = 1u; put(rt, frame.at(0x10u), G[20]); G[20] = 4u; put(rt, frame.at(0xCu), G[19]); G[19] = 3u; put(rt, frame.at(8u), G[18]); G[18] = 2u; put(rt, frame.at(4u), G[17]); G[17] = 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0x18u), G[31]); for (;;) { G[2] = word(rt, G[16] + 4u); G[4] = G[2] + 0x14u; if (G[2] == 0u) break; G[3] = load16(G[2] + 8u); if (G[3] == G[18]) { call_here(rt, ctx, 0x08B37B44u); G[3] = word(rt, G[16] + 4u); G[17] += G[2]; G[16] = G[3] + 0x1Cu; continue; } G[4] = G[2] + 0x14u; if (G[3] == G[19] || G[3] == G[20]) { call_here(rt, ctx, 0x08B37B44u); G[3] = word(rt, G[16] + 4u); G[2] += 2u; G[17] += G[2]; G[16] = G[3] + 0x1Cu; continue; } G[4] = G[2] + 0x14u; if (G[3] != G[21]) break; call_here(rt, ctx, 0x08B37B44u); G[3] = word(rt, G[16] + 4u); G[2] += 1u; G[17] += G[2]; G[16] = G[3] + 0x1Cu; } G[2] = G[17]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B496C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { (void)rt; G[2] = G[4] < 0xEu ? 1u : 0u; { const bool small = G[2] != 0u; G[2] = 0x08C90000u; if (!small) { G[2] = 0x08C955B4u; return; } } G[2] = G[4] - 1u; G[2] = G[2] < 2u ? 1u : 0u; if (G[2] == 0u) { G[2] = 0x08C955B4u; return; } G[2] = 0x51EB851Fu; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[6] = sra(G[5], 31); G[2] = ctx.hi; G[2] = sra(G[2], 5); G[2] -= G[6]; G[4] = G[2] << 4; G[2] <<= 2; G[2] += G[4]; G[3] = G[2] << 2; G[2] += G[3]; G[2] = G[5] - G[2]; G[2] -= 0xAu; G[2] = G[2] < 0xAu ? 1u : 0u; if (G[2] != 0u) { G[2] = 0x08CB5758u; return; } G[2] = 0x66666667u; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[2] = ctx.hi; G[2] = sra(G[2], 2); G[4] = G[2] - G[6]; G[3] = G[4] << 3; G[2] = G[4] << 1; G[2] += G[3]; G[4] = G[5] - G[2]; G[3] = 1u; if (G[4] == G[3]) { G[2] = 0x08CB575Cu; return; } G[2] = 2u; if (G[4] == G[2]) { G[2] = 0x08CB5760u; return; } G[2] = 3u; if (G[4] == G[2]) { G[2] = 0x08CB5764u; return; } G[2] = 0x08CB5758u; } }
void func_08BA23EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(4u), G[17]); G[17] = G[4]; G[4] = 0x20u; put(rt, frame.at(8u), G[31]); put(rt, frame.at(0u), G[16]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[1] = 0x3F800000u; ctx.fpr[20] = std::bit_cast<float>(G[1]); G[4] = G[2]; call_here(rt, ctx, 0x08BA2360u); put_float(rt, G[16], ctx.fpr[20]); G[4] = G[16] + 4u; call_here(rt, ctx, 0x08BA22D4u); G[1] = 0x3F666666u; ctx.fpr[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 4u, ctx.fpr[0]); G[4] = G[16] + 8u; call_here(rt, ctx, 0x08BA21E4u); G[1] = 0x447A0000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 8u, ctx.fpr[0]); G[4] = G[16] + 0xCu; call_here(rt, ctx, 0x08BA20F4u); put_float(rt, G[16] + 0xCu, ctx.fpr[20]); G[4] = G[16] + 0x10u; call_here(rt, ctx, 0x08BA2004u); G[4] = G[16] + 0x14u; put(rt, G[16] + 0x10u, 0u); call_here(rt, ctx, 0x08BA1F14u); G[1] = 0x43480000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 0x14u, ctx.fpr[0]); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08BA1E24u); put(rt, G[16] + 0x18u, 0u); G[5] = G[16]; G[4] = G[17]; put(rt, G[16] + 0x1Cu, 0u); G[6] = 0u; put(rt, G[17], 0u); put(rt, G[17] + 4u, 0u); store8(G[17] + 8u, 0u); call_here(rt, ctx, 0x08B9DC50u); G[2] = 1u; store8(G[17] + 8u, 1u); G[2] = G[17]; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } } }
void func_08BECA44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[2] = G[5] + 0x30u; G[8] = G[4]; put(rt, frame.at(4u), G[31]); G[6] = word(rt, G[4] + 4u); G[16] = G[5]; if (G[2] != G[6]) { G[3] = G[6] - G[2]; G[2] = 0xAAAA0000u; G[3] = sra(G[3], 4); G[2] = 0xAAAAAAABu; { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[7] = ctx.lo; G[5] += 0x2Cu; if (static_cast<std::int32_t>(G[7]) > 0) { G[6] = 0u; G[4] = G[16] + 0x5Cu; do { ctx.fpr[0] = float_at(rt, G[4] - 0x20u); ctx.fpr[1] = float_at(rt, G[4] - 0x1Cu); G[2] = word(rt, G[4] - 0x28u); G[3] = word(rt, G[4] - 0x24u); ctx.fpr[2] = float_at(rt, G[4] - 0x18u); ctx.fpr[3] = float_at(rt, G[4] - 0x14u); ctx.fpr[4] = float_at(rt, G[4] - 0x10u); put(rt, G[5] - 0x28u, G[2]); G[6] += 1u; put(rt, G[5] - 0x24u, G[3]); put_float(rt, G[5] - 0x20u, ctx.fpr[0]); put_float(rt, G[5] - 0x1Cu, ctx.fpr[1]); put_float(rt, G[5] - 0x18u, ctx.fpr[2]); put_float(rt, G[5] - 0x14u, ctx.fpr[3]); put_float(rt, G[5] - 0x10u, ctx.fpr[4]); ctx.fpr[0] = float_at(rt, G[4] - 0xCu); put_float(rt, G[5] - 0xCu, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[4] - 8u); put_float(rt, G[5] - 8u, ctx.fpr[1]); ctx.fpr[0] = float_at(rt, G[4] - 4u); put_float(rt, G[5] - 4u, ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[4]); G[4] += 0x30u; put_float(rt, G[5], ctx.fpr[1]); G[5] += 0x30u; } while (G[6] != G[7]); G[6] = word(rt, G[8] + 4u); } } G[4] = G[6] - 0x30u; put(rt, G[8] + 4u, G[4]); G[2] = word(rt, G[6] - 0x30u); G[3] = word(rt, G[2]); call_here(rt, ctx, G[3]); G[2] = G[16]; G[16] = word(rt, frame.at(0u)); } } }
void func_08BEDF34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); G[22] = G[6]; put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[4] = word(rt, G[4]); G[20] = G[5]; if (G[4] != G[19]) { G[18] = G[4]; G[21] = 0x08CA0000u; do { G[16] = word(rt, G[4] + 8u); G[17] = word(rt, G[18]); G[4] = 0x08CA2F7Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x08A76F20u); G[4] = 0xCu; G[5] = 4u; if (G[2] != 0u) { call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[2] += 8u; G[4] = G[2]; G[5] = G[18] + 8u; if (G[2] != 0u) call_here(rt, ctx, 0x089FDAC8u); G[4] = G[16]; G[5] = G[22]; call_here(rt, ctx, 0x08B5C078u); G[4] = G[19]; G[5] = G[18]; call_here(rt, ctx, 0x08BEDE24u); } G[18] = G[17]; G[4] = G[17]; } while (G[19] != G[17]); } G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08C0D458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); G[18] = G[6]; put(rt, frame.at(4u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0u), G[16]); G[8] = word(rt, G[4]); G[16] = word(rt, G[4] + 4u); G[3] = word(rt, G[4] + 8u); G[2] = G[5] - G[8]; G[19] = sra(G[2], 6); if (G[16] != G[3] && G[16] == G[5]) { G[4] = G[16] + 0x20u; G[5] = G[6] + 0x20u; if (G[16] != 0u) { G[2] = word(rt, G[6] + 4u); G[3] = word(rt, G[6] + 8u); ctx.fpr[0] = float_at(rt, G[6] + 0xCu); ctx.fpr[1] = float_at(rt, G[6] + 0x10u); ctx.fpr[2] = float_at(rt, G[6] + 0x14u); ctx.fpr[3] = float_at(rt, G[6] + 0x18u); ctx.fpr[4] = float_at(rt, G[6] + 0x1Cu); put(rt, G[16] + 4u, G[2]); G[2] = 0x08CCFC90u; put(rt, G[16] + 8u, G[3]); put_float(rt, G[16] + 0xCu, ctx.fpr[0]); put_float(rt, G[16] + 0x10u, ctx.fpr[1]); put_float(rt, G[16] + 0x14u, ctx.fpr[2]); put_float(rt, G[16] + 0x18u, ctx.fpr[3]); put_float(rt, G[16] + 0x1Cu, ctx.fpr[4]); put(rt, G[16], G[2]); call_here(rt, ctx, 0x089FA280u); G[4] = G[16] + 0x30u; G[5] = G[18] + 0x30u; call_here(rt, ctx, 0x089FA280u); G[16] = word(rt, G[17] + 4u); G[8] = word(rt, G[17]); } G[2] = G[16] + 0x40u; put(rt, G[17] + 4u, G[2]); } else { call_here(rt, ctx, 0x08C0D100u); G[8] = word(rt, G[17]); } G[2] = G[19] << 6; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); G[2] = G[8] + G[2]; } } }
void func_08C20910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; G[4] = frame.at(0u); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); G[21] = G[5]; put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); call_here(rt, ctx, 0x08A813F4u); G[4] = G[21]; G[5] = word(rt, G[19] + 8u); G[5] <<= 1; call_here(rt, ctx, 0x08A6159Cu); G[5] = G[2]; G[2] = word(rt, G[19] + 8u); G[2] <<= 1; G[4] = G[21]; if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = G[5]; G[18] = 0u; G[20] = 0x08CB0000u; G[16] = word(rt, frame.at(0u)); do { G[18] += 2u; G[4] = 0x08CB029Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = G[17]; G[6] = G[17] + 4u; G[3] = word(rt, G[2] + 0xA0u); G[4] = G[16]; G[17] += 8u; call_here(rt, ctx, G[3]); G[2] = word(rt, G[19] + 8u); G[2] <<= 1; G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[16] = word(rt, frame.at(0u)); } while (G[2] != 0u); G[4] = G[21]; } G[5] = frame.at(0u); call_here(rt, ctx, 0x08A61584u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_236
} // namespace gt::game

#define HOT236(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_236::name, gt::ReturnKind::kind)
HOT236(0x0881E8E4u, func_0881E8E4, Word);
HOT236(0x0882DE9Cu, func_0882DE9C, Word);
HOT236(0x0882FA68u, func_0882FA68, Word);
HOT236(0x088313DCu, func_088313DC, Word);
HOT236(0x0886B6F0u, func_0886B6F0, Word);
// HOT236(0x08879CB8u, func_08879CB8, Word);  // no comparable synthetic case yet
HOT236(0x088B17A8u, func_088B17A8, Word);
HOT236(0x088F3484u, func_088F3484, Word);
HOT236(0x089192D8u, func_089192D8, Word);
HOT236(0x0892B0A0u, func_0892B0A0, Word);
HOT236(0x08938B0Cu, func_08938B0C, Word);
// HOT236(0x08997A4Cu, func_08997A4C, Word);  // no comparable synthetic case yet
// HOT236(0x08A4FB80u, func_08A4FB80, Word);  // no comparable synthetic case yet
// HOT236(0x08AB4168u, func_08AB4168, Word);  // no comparable synthetic case yet
HOT236(0x08ACA674u, func_08ACA674, Word);
HOT236(0x08AE2F08u, func_08AE2F08, Word);
HOT236(0x08B16CC4u, func_08B16CC4, Word);
HOT236(0x08B2F468u, func_08B2F468, Word);
HOT236(0x08B37B44u, func_08B37B44, Word);
HOT236(0x08B496C0u, func_08B496C0, Word);
HOT236(0x08BA23ECu, func_08BA23EC, Word);
// HOT236(0x08BECA44u, func_08BECA44, Word);  // no comparable synthetic case yet
// HOT236(0x08BEDF34u, func_08BEDF34, Word);  // no comparable synthetic case yet
// HOT236(0x08C0D458u, func_08C0D458, Word);  // no comparable synthetic case yet
// HOT236(0x08C20910u, func_08C20910, Word);  // no comparable synthetic case yet
