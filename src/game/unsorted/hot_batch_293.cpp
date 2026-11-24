// Hot batch 293: seventy-five and seventy-six-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (a keyed insert, a slot sweep, a state copy and a wrapped list cursor).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_293 {
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

// 0x08C34990 (a0 = the owner, a1 = a flag, a2 = a node, a3 = a text holder; v0 = the 0x1C-byte object): the mark starts at 1 (flag set, the node being the
// owner's +4 list head, or the node's word +0x14 below the holder's word +4, comparing unsigned) and falls to 0 when the holder's word +4 is below it, or, with
// both equal, when 0x08B36170(the holder's text head, the node's +0x10 head, the shorter length) — or the difference of the lengths when it answers zero — is
// not below zero. 0x08A5BC20(0x1C, 4) makes a 0x1C-byte object whose +0x10 part takes the text (0x08B88F44(part, holder)) and the holder's words +4 / +8 go
// to the part's; the object goes to 0x08B5F2B8(mark, object, node, owner + 4) and the owner's word +0x14 counts it.
void func_08C34990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], flag = ctx.gpr[5], node = ctx.gpr[6], holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame,
                         {{22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {31u, 0x2Cu}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[22] = 1u;
    const u32 link = self + 0x4u;
    ctx.gpr[21] = link;
    ctx.gpr[20] = node;
    ctx.gpr[19] = self;
    ctx.gpr[18] = holder;
    if (flag == 0u && node != link) {
        const u32 other = word(rt, holder + 0x4u);
        const u32 own = word(rt, node + 0x14u);
        if (!(other < own)) {
            if (own < other) {
                ctx.gpr[22] = 0u;
            } else {
                const u32 first_head = word(rt, holder);
                const u32 second_head = word(rt, node + 0x10u);
                const u32 first_length = word(rt, first_head - 0xCu);
                const u32 second_length = word(rt, second_head - 0xCu);
                ctx.gpr[16] = first_length;
                put(rt, frame.at(0x0u), first_length);
                put(rt, frame.at(0x4u), second_length);
                const u32 difference = first_length - second_length;
                const u32 shorter = second_length < first_length ? second_length : first_length;
                const u32 compared = call_set(rt, ctx, 0x08B36170u, {first_head, second_head, shorter});
                const u32 verdict = compared != 0u ? compared : difference;
                ctx.gpr[16] = verdict;
                if (!(static_cast<s32>(verdict) < 0)) ctx.gpr[22] = 0u;
            }
        }
    }
    const u32 made = call_set(rt, ctx, 0x08A5BC20u, {0x1Cu, 4u});
    const u32 part = made + 0x10u;
    ctx.gpr[16] = part;
    ctx.gpr[17] = made;
    if (part != 0u) {
        call_set(rt, ctx, 0x08B88F44u, {part, holder});
        put(rt, part + 0x4u, word(rt, holder + 0x4u));
        put(rt, part + 0x8u, word(rt, holder + 0x8u));
    }
    call_set(rt, ctx, 0x08B5F2B8u, {ctx.gpr[22], made, node, link});
    put(rt, self + 0x14u, word(rt, self + 0x14u) + 1u);
    ctx.gpr[2] = made;
}
void func_0881B820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[6] &= 0xFFu; G[21] = G[4]; G[20] = G[5]; G[2] = word(rt, G[5]); put(rt, frame.at(8u), G[6]); put(rt, frame.at(0u), G[2]); put(rt, frame.at(4u), 0u); if (static_cast<std::int32_t>(G[2]) > 0) { G[30] = 0u; G[19] = 0u; G[22] = 0x08CA0000u; G[23] = 0x20u; do { G[18] = 0u; while (true) { G[2] = word(rt, G[20] + 0x30u); G[5] = G[2] + G[19]; G[3] = G[5] + G[18]; G[2] = load8(G[3] + 2u); G[2] &= 0xFu; G[4] = 0x08C98F20u; if (static_cast<std::int32_t>(G[2]) > 0) { G[16] = load16(G[5]); G[17] = 0u; G[7] = 0u; G[6] = G[16]; call_here(rt, ctx, 0x0892C810u); G[8] = G[18]; G[4] = G[21]; G[6] = G[16]; G[7] = G[17]; if (G[2] != 0u) break; call_here(rt, ctx, 0x0881A8C0u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[2] = word(rt, frame.at(4u)); G[6] = G[16]; G[7] = G[17]; G[2] += 1u; put(rt, frame.at(4u), G[2]); G[8] = G[18]; G[2] = word(rt, frame.at(8u)); G[4] = G[21]; if (G[2] == 0u) call_here(rt, ctx, 0x0881B758u); } } G[18] += 1u; if (G[18] == G[23]) break; } G[2] = word(rt, frame.at(0u)); G[30] += 1u; G[19] += 0x22u; } while (G[2] != G[30]); } G[2] = word(rt, frame.at(4u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08826FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; G[19] = G[4]; G[18] = G[7]; if (G[6] == 0u) { G[17] = frame.at(8u); G[4] = G[17]; call_here(rt, ctx, 0x088248E4u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08C99B4Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[17]; G[20] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[18] + 4u); G[17] = G[2]; if (G[16] == 0u) { G[4] = 0x08C99B24u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = G[20]; G[5] = G[17]; call_here(rt, ctx, 0x0885C228u); G[16] = frame.at(4u); if (G[2] != 0u) { G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x0884F674u); G[5] = word(rt, frame.at(4u)); if (G[19] != G[16]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x08A76AA0u); G[5] = word(rt, frame.at(0u)); if (G[19] != frame.at(0u)) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[20] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_088A5450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x18u), G[31]); G[21] = 3u; G[20] = G[5]; G[19] = G[4]; G[18] = 0u; G[17] = G[20] + 0x38u; G[16] = G[4] + 0x38u; G[2] = word(rt, G[5] + 4u); G[3] = load8(G[5] + 8u); G[6] = load8(G[20] + 0xAu); G[5] = load8(G[5] + 9u); G[7] = load8(G[20] + 0xBu); put(rt, G[4] + 4u, G[2]); store8(G[4] + 8u, G[3]); store8(G[4] + 9u, G[5]); store8(G[4] + 0xAu, G[6]); store8(G[4] + 0xBu, G[7]); do { G[4] = G[16]; G[5] = G[17]; G[18] += 1u; call_here(rt, ctx, 0x08932450u); G[16] += 0x180u; G[17] += 0x180u; } while (G[18] != G[21]); G[2] = word(rt, G[20] + 0xCu); G[3] = word(rt, G[20] + 0x10u); G[4] = word(rt, G[20] + 0x14u); G[5] = word(rt, G[20] + 0x18u); G[6] = word(rt, G[20] + 0x1Cu); G[7] = word(rt, G[20] + 0x20u); G[8] = word(rt, G[20] + 0x24u); G[9] = word(rt, G[20] + 0x28u); G[16] = load16(G[20] + 0x4BAu); G[17] = load16(G[20] + 0x4BCu); G[10] = load8(G[20] + 0x2Cu); G[11] = load8(G[20] + 0x2Du); G[12] = load8(G[20] + 0x2Eu); G[13] = load8(G[20] + 0x2Fu); G[14] = word(rt, G[20] + 0x30u); G[15] = load16(G[20] + 0x4B8u); G[18] = load16(G[20] + 0x4BEu); put(rt, G[19] + 0xCu, G[2]); store16(G[19] + 0x4BAu, G[16]); store16(G[19] + 0x4BCu, G[17]); put(rt, G[19] + 0x10u, G[3]); put(rt, G[19] + 0x14u, G[4]); put(rt, G[19] + 0x18u, G[5]); put(rt, G[19] + 0x1Cu, G[6]); put(rt, G[19] + 0x20u, G[7]); put(rt, G[19] + 0x24u, G[8]); put(rt, G[19] + 0x28u, G[9]); store8(G[19] + 0x2Cu, G[10]); store8(G[19] + 0x2Du, G[11]); store8(G[19] + 0x2Eu, G[12]); store8(G[19] + 0x2Fu, G[13]); put(rt, G[19] + 0x30u, G[14]); store16(G[19] + 0x4B8u, G[15]); store16(G[19] + 0x4BEu, G[18]); G[2] = word(rt, G[20] + 0x4D0u); put(rt, G[19] + 0x4D0u, G[2]); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_088E7ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x481907FFu; G[17] = G[4]; G[3] = G[4]; G[6] = G[2]; G[5] = 0x14u; put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, 0u); G[4] = 0u; put(rt, G[17] + 0x1Cu, 0u); put(rt, G[17] + 0x20u, G[2]); put(rt, G[17] + 0x2Cu, G[2]); put(rt, G[17] + 0x28u, G[2]); put(rt, G[17] + 0x24u, 0u); do { G[4] += 1u; put(rt, G[3] + 0x30u, G[6]); G[3] += 4u; } while (G[4] != G[5]); G[18] = 0u; while (true) { G[4] = word(rt, G[17]); call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[5] = G[18]; if (G[2] == 0u) break; G[4] = word(rt, G[17]); call_here(rt, ctx, 0x088DCD28u); G[16] = G[2]; G[2] = static_cast<std::int32_t>(G[18]) < 2 ? 1u : 0u; if (G[16] != 0u) { G[4] = G[16]; G[5] = 1u; if (G[2] != 0u) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[16]; G[5] = 0u; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x74u); G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x5Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); } else { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x74u); G[4] = G[16]; G[5] = 0u; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x60u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x088E7A04u); } } G[18] += 1u; } G[17] = word(rt, frame.at(0x4u)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); }  } }
// 0x08927BE4 (a0 = the owner, a1 = the list, a2 / a3 = two flag bytes; v0 = 1 or 0): when the entry at list's word +0 indexed by the list's word +8 (8 bytes
// each) is null, the list is walked from that index for its word +4 entries: 0x08927BC8(owner, entry's word +4) ending the walk when it answers non-zero
// (the index read again first). Each step moves the index by -1 (a3 set) or +1, a2 = 1 forces it to 0, an index at or above the count becomes 0 and a negative
// one becomes the count - 1. When the index moved from where it started the list is handed to 0x089274C8(owner, list) and the answer is 0, else 1.
void func_08927BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], list = ctx.gpr[5];
    const u32 mode = ctx.gpr[6] & 0xFFu, direction = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame,
                         {{22u, 0x18u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}, {31u, 0x1Cu}, {21u, 0x14u}, {17u, 0x4u}});
    ctx.gpr[20] = self;
    ctx.gpr[19] = mode;
    ctx.gpr[18] = direction;
    ctx.gpr[16] = list;
    u32 index = word(rt, list + 0x8u);
    const u32 first = word(rt, list);
    ctx.gpr[22] = index;
    u32 result = 1u;
    if (word(rt, first + (index << 3u)) == 0u) {
        u32 step = 0u;
        ctx.gpr[21] = 1u;
        for (;;) {
            const u32 count = word(rt, list + 0x4u);
            const bool more = static_cast<s32>(step) < static_cast<s32>(count);
            ctx.gpr[17] = step + 1u;
            if (!more) break;
            const u32 entry = word(rt, list) + (index << 3u);
            const u32 blocked = call_set(rt, ctx, 0x08927BC8u, {self, word(rt, entry + 0x4u)});
            index = word(rt, list + 0x8u);
            if (blocked != 0u) break;
            if (direction != 0u) index -= 1u;
            else index += 1u;
            put(rt, list + 0x8u, index);
            if (mode == ctx.gpr[21]) {
                put(rt, list + 0x8u, 0u);
                index = 0u;
            }
            if (!(static_cast<s32>(index) < static_cast<s32>(word(rt, list + 0x4u)))) {
                index = 0u;
                put(rt, list + 0x8u, 0u);
            } else if (static_cast<s32>(index) < 0) {
                index = word(rt, list + 0x4u) - 1u;
                put(rt, list + 0x8u, index);
            }
            step = ctx.gpr[17];
        }
    }
    if (ctx.gpr[22] != index) {
        call_set(rt, ctx, 0x089274C8u, {self, list});
        result = 0u;
    }
    ctx.gpr[2] = result;
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_293
} // namespace gt::game

#define HOT293(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_293::name, gt::ReturnKind::kind)
HOT293(0x08C34990u, func_08C34990, Word);
HOT293(0x0881B820u, func_0881B820, Word);
HOT293(0x08826FB8u, func_08826FB8, Word);
HOT293(0x088A5450u, func_088A5450, Word);
HOT293(0x088E7ABCu, func_088E7ABC, Word);
HOT293(0x08927BE4u, func_08927BE4, Word);
