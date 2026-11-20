// Hot batch 265: mid-sized functions no route reaches, written by hand from `tools.disasm` and verified by synthetic
// cases (a float unpacked into class, sign, exponent and fraction, an expiry sweep under a lock, recursive tree
// teardowns, a bit-stream writer, heap and merge steps, insertion sorts and vector erases).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_265 {
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

void func_08B71F2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = word(rt, G[4]); G[3] = G[2] >> 31; put(rt, G[5] + 4u, G[3]); G[4] = (G[2] >> 23) & 0xFFu; G[3] = G[2] & 0x007FFFFFu; if (G[4] == 0u) { G[2] = 2u; if (G[3] == 0u) { put(rt, G[5], G[2]); return; } G[2] = 0x3FFF0000u; G[6] = G[3] << 7; G[7] = 0x3FFFFFFFu; G[3] = 3u; G[2] = 0xFFFFFF82u; G[4] = G[7] < G[6] ? 1u : 0u; put(rt, G[5] + 8u, G[2]); put(rt, G[5], G[3]); if (G[4] == 0u) { G[4] = G[7]; G[3] = 0xFFFFFF82u; do { G[6] = G[6] << 1; G[2] = G[4] < G[6] ? 1u : 0u; G[3] = G[3] - 1u; } while (G[2] == 0u); put(rt, G[5] + 8u, G[3]); } put(rt, G[5] + 0xCu, G[6]); return; } G[2] = G[3] << 7; if (G[4] != 0xFFu) { G[3] = 0x40000000u; G[2] = G[2] | G[3]; G[4] = G[4] - 0x7Fu; put(rt, G[5] + 0xCu, G[2]); G[2] = 3u; put(rt, G[5] + 8u, G[4]); put(rt, G[5], G[2]); return; } G[2] = 0x00100000u; if (G[3] == 0u) { G[2] = 4u; put(rt, G[5], G[2]); return; } G[2] = G[3] & G[2]; if (G[2] == 0u) { G[2] = 1u; put(rt, G[5], G[2]); } else put(rt, G[5], 0u); put(rt, G[5] + 0xCu, G[3]); } }
// 0x08949F44 (a0 = the object): 0x0895E3F4(object) gives a value and the object's word +8 the state block; -1 ends it at once. Otherwise,
// when the state byte +0x5F9 is set, a zero value or a clear byte +0x5F8 sets that byte to 2; else 0x0895BBB4(object's +0xC word + 0x270)
// deciding either sets it to 3, and the byte +0x74F becomes 1 when +0x5F8 is below the value and -1 when the value is below it. Then
// 0x0895DCAC(object, value) is tail-called.
// 0x08949F44 (a0 = the object): 0x0895E3F4(object) gives a value and the object's word +8 the state block; -1 ends it at once. Otherwise,
// when the state byte +0x5F9 is set, a zero value or a clear byte +0x5F8 sets that byte to 2; else 0x0895BBB4(object's +0xC word + 0x270)
// deciding either sets it to 3, and the byte +0x74F becomes 1 when +0x5F8 is below the value and -1 when the value is below it. Then
// 0x0895DCAC(object, value) is tail-called.
void func_08949F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {31u, 0x10u}, {16u, 0x0u}});
    ctx.gpr[19] = 0xFFFFFFFFu;
    ctx.gpr[18] = self;
    const u32 state = word(rt, self + 0x8u);
    ctx.gpr[16] = state;
    const u32 value = call_set(rt, ctx, 0x0895E3F4u, {self});
    ctx.gpr[17] = value;
    if (value == 0xFFFFFFFFu) return;
    if (load8(state + 0x5F9u) != 0u) {
        if (value == 0u || load8(state + 0x5F8u) == 0u) {
            store8(state + 0x5F9u, 2u);
        } else {
            if (call_set(rt, ctx, 0x0895BBB4u, {word(rt, self + 0xCu) + 0x270u}) == 0u) {
                store8(state + 0x5F9u, 3u);
            }
            const u32 previous = load8(state + 0x5F8u);
            if (static_cast<s32>(previous) < static_cast<s32>(value)) {
                store8(state + 0x74Fu, 1u);
            } else if (static_cast<s32>(value) < static_cast<s32>(previous)) {
                store8(state + 0x74Fu, static_cast<u32>(-1));   // the -1 the asm keeps in s3
            }
        }
    }
    tail_call(ctx, 0x0895DCACu, {self, value});
}
// 0x08B1C68C (a0 = the list, a2 = a value, a3 = a priority): under the lock at list +4 (0x08B34D40), walks the chain at list +0x18 whose
// nodes keep their priority at +0x34, a second value at +0x30 and the successor at +0x2C, and unlinks every node that is not ordered below
// the given pair (a node is kept when its priority is above a3, or equal with its +0x30 value above a2), rewriting the head or the previous
// node's successor. 0x08B34D80 releases the lock; the removed nodes, chained through their own +0x2C, are freed one by one with
// 0x08B28D04 (the successor read before the free).
void func_08B1C68C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], value = ctx.gpr[6], priority = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x14u}});
    const u32 lock = self + 0x4u;
    ctx.gpr[20] = lock;
    ctx.gpr[19] = self;
    ctx.gpr[18] = 0u;
    ctx.gpr[17] = priority;
    ctx.gpr[16] = value;
    call_set(rt, ctx, 0x08B34D40u, {lock});
    u32 removed = 0u;
    u32 previous = 0u;
    u32 node = word(rt, self + 0x18u);
    while (node != 0u) {
        const u32 node_priority = word(rt, node + 0x34u);
        const u32 next = word(rt, node + 0x2Cu);
        if (static_cast<s32>(priority) < static_cast<s32>(node_priority)
            || (node_priority == priority && value < word(rt, node + 0x30u))) {
            previous = node;
        } else {
            if (previous == 0u) put(rt, self + 0x18u, next);
            else put(rt, previous + 0x2Cu, next);
            put(rt, node + 0x2Cu, removed);         // the chain of removed nodes, newest first
            removed = node;
            ctx.gpr[18] = removed;
        }
        node = next;
    }
    call_set(rt, ctx, 0x08B34D80u, {lock});
    while (removed != 0u) {
        const u32 next_removed = word(rt, removed + 0x2Cu);
        call_set(rt, ctx, 0x08B28D04u, {removed});
        removed = next_removed;
        ctx.gpr[18] = removed;
    }
}
void func_08965DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto ext = [](std::uint32_t v, unsigned pos) { return (v >> pos) & 0xFu; }; auto put4 = [](std::uint32_t v, std::uint32_t src, unsigned pos) { return (v & ~(0xFu << pos)) | ((src & 0xFu) << pos); }; G[2] = 1u; G[8] = G[4]; G[7] = G[5]; const std::int32_t mode = static_cast<std::int32_t>(G[6]); if (mode == 1) { G[2] = word(rt, G[4] + 0xE0u); G[5] = ~G[5]; G[3] = ext(G[2], 4u) | G[7]; G[2] = put4(G[2], G[3], 4u); G[4] = ext(G[2], 8u) & G[5]; G[2] = put4(G[2], G[4], 8u); put(rt, G[8] + 0xE0u, G[2]); return; } G[2] = mode < 2 ? 1u : 0u; if (G[2] != 0u) { if (mode != 0) return; G[2] = word(rt, G[4] + 0xE0u); G[5] = ~G[5]; G[3] = ext(G[2], 4u) & G[5]; G[2] = put4(G[2], G[3], 4u); G[4] = ext(G[2], 8u); G[5] = G[5] & G[4]; G[2] = put4(G[2], G[5], 8u); put(rt, G[8] + 0xE0u, G[2]); return; } G[2] = 2u; if (mode == 2) { G[2] = word(rt, G[4] + 0xE0u); G[4] = ~G[5]; G[3] = ext(G[2], 4u) & G[4]; G[2] = put4(G[2], G[3], 4u); G[4] = ext(G[2], 8u) | G[5]; G[2] = put4(G[2], G[4], 8u); put(rt, G[8] + 0xE0u, G[2]); return; } G[2] = 3u; if (mode != 3) return; G[2] = word(rt, G[4] + 0xE0u); G[3] = ext(G[2], 4u) | G[5]; G[2] = put4(G[2], G[3], 4u); G[4] = G[5] | ext(G[2], 8u); G[2] = put4(G[2], G[4], 8u); put(rt, G[8] + 0xE0u, G[2]); } }
void func_088858B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[17] = G[4]; put(rt, frame.at(8u), G[31]); put(rt, frame.at(0u), G[16]); G[4] = word(rt, G[4] + 0x40u); auto release = [&](std::uint32_t off) { G[3] = G[16] + off; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; return G[4] == G[2]; }; [&] { if (G[4] == 0u) return; G[5] = word(rt, G[17] + 0x44u); if (G[5] != 0u) { G[6] = 0x08884EF0u; call_here(rt, ctx, 0x08BA5FA0u); put(rt, G[17] + 0x44u, 0u); } G[3] = word(rt, G[17] + 0x68u); G[2] = load8(G[3] + 0x25u); G[2] = (G[2] << 3) + 0x10u; G[3] = G[3] + G[2]; G[16] = word(rt, G[3] + 8u); put(rt, G[3] + 4u, 0u); put(rt, G[3] + 8u, 0u); if (G[16] != 0u && release(4u)) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (release(8u)) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } put(rt, G[17] + 0x40u, 0u); }(); put(rt, G[17] + 0x18Cu, 0u); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_089590A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x40u), G[16]); G[19] = word(rt, G[4] + 8u); G[17] = G[4]; if (static_cast<std::int32_t>(G[19]) > 0) { G[18] = 0u; G[20] = 4u; G[5] = G[18]; do { G[4] = G[17]; call_here(rt, ctx, 0x0894CF40u); G[3] = word(rt, G[2] + 8u); G[5] = G[18]; G[4] = G[17]; if (G[3] != 0u) { call_here(rt, ctx, 0x0894CFF8u); G[3] = word(rt, G[2] + 8u); G[16] = G[2]; G[4] = G[2]; G[2] = sext(load8(G[3] + 3u), 8u); if (G[2] != G[20]) { call_here(rt, ctx, 0x08959B58u); G[4] = G[17]; call_here(rt, ctx, 0x08958D40u); G[6] = G[2]; G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x0895A0D8u); G[2] = word(rt, G[17] + 0x1Cu); G[5] = G[18]; G[6] = frame.at(0u); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 8u); call_here(rt, ctx, G[3]); } } } G[18] = G[18] + 1u; G[5] = G[18]; } while (G[19] != G[18]); } G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } }
void func_0896A09C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[17]); G[17] = G[5] & 0xFFu; put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x28u), G[31]); G[16] = G[4]; call_here(rt, ctx, 0x08965BD4u); G[3] = 1u; G[6] = frame.at(4u); if (G[2] != G[3]) { G[7] = frame.at(8u); G[8] = frame.at(0xCu); G[5] = frame.at(0u); G[4] = G[16]; call_here(rt, ctx, 0x08969B98u); G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x08966040u); F[0] = float_at(rt, frame.at(0u)); F[1] = float_at(rt, frame.at(4u)); G[4] = G[16]; put_float(rt, frame.at(0x1Cu), F[0]); G[5] = frame.at(0x1Cu); G[6] = frame.at(0x18u); put_float(rt, frame.at(0x18u), F[1]); G[7] = frame.at(0x14u); G[8] = frame.at(0x10u); F[0] = float_at(rt, frame.at(8u)); F[1] = float_at(rt, frame.at(0xCu)); put_float(rt, frame.at(0x14u), F[0]); put_float(rt, frame.at(0x10u), F[1]); call_here(rt, ctx, 0x08969D2Cu); } if (G[17] != 0u) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x19Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[16] = word(rt, G[16] + 0xF4u); while (G[16] != 0u) { G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x0896A09Cu); G[4] = G[16]; call_here(rt, ctx, 0x08968A38u); G[16] = G[2]; } } } G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } }
void func_08860664(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto slot = [&](std::uint32_t i) { return G[7] + (i << 4) + (i << 6) + 0x4120u; }; G[3] = G[5] << 6; G[2] = (G[5] << 4) + G[3] + 0x4120u; G[9] = G[4] + G[2]; G[3] = word(rt, G[9] + 0x24u); G[7] = G[4]; G[8] = G[5]; G[3] = G[9] + G[3]; G[2] = sext(load8(G[3] + 4u), 8u); G[11] = G[6]; G[4] = ~G[2]; if (G[4] == 0u) G[2] = G[5]; G[3] = G[2] << 6; G[2] = slot(G[2]); G[3] = load8(G[2] + 0x18u); G[10] = G[9] + G[6]; if (G[3] == 0u) { G[2] = sext(load16(G[2] + 0x1Eu), 16u); G[6] = word(rt, G[7] + 0x3F50u); G[3] = (G[2] << 6) - (G[2] << 3); G[5] = word(rt, G[6]); G[4] = (G[3] << 3) - G[3] + G[5]; G[2] = 0xFFFFFFFFu; put(rt, G[4] + 0x30u, G[2]); } G[2] = sext(load8(G[10] + 4u), 8u); G[3] = ~G[2]; if (G[3] == 0u) G[2] = G[8]; G[4] = G[2] << 6; G[2] = slot(G[2]); G[3] = load8(G[2] + 0x18u); if (G[3] == 0u) { G[3] = sext(load16(G[2] + 0x1Eu), 16u); G[5] = word(rt, G[7] + 0x3F50u); G[4] = (G[3] << 6) - (G[3] << 3); G[3] = word(rt, G[5]); G[2] = (G[4] << 3) - G[4] + G[3]; put(rt, G[2] + 0x30u, G[8]); } put(rt, G[9] + 0x24u, G[11]); } }
void func_0881ACC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[5] = G[5] - G[6]; put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); bool run = true; if (static_cast<std::int32_t>(G[5]) < 0) G[5] = 0u; else { G[2] = static_cast<std::int32_t>(G[5]) < 0x1E ? 1u : 0u; run = G[2] != 0u; } if (run) { G[19] = G[5] + G[6]; G[3] = (G[5] << 3) + G[4]; G[2] = (G[19] << 3) + G[4]; G[17] = G[3] + 0x354u; G[20] = G[5]; G[18] = G[2] + 0x354u; G[21] = 0x1Eu; G[16] = G[17]; do { G[2] = static_cast<std::int32_t>(G[19]) < 0x1E ? 1u : 0u; G[4] = frame.at(0u); G[5] = 0u; if (G[2] != 0u) { G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x088A6C18u); G[20] = G[20] + 1u; } else { G[20] = G[20] + 1u; call_here(rt, ctx, 0x088A79ACu); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x088A6C18u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088A7548u); } G[16] = G[16] + 8u; G[17] = G[17] + 8u; G[19] = G[19] + 1u; G[18] = G[18] + 8u; } while (G[20] != G[21]); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
// 0x0896A178 (a0 = the object, a1 = a recursion flag): the state-2 twin of 0x0896A09C: unless 0x08965BD4(object) reports 2,
// 0x08969B98(object, frame, frame +4, frame +8, frame +0xC) collects four floats, 0x08966040(object, 2) runs and 0x08969D2C takes the copies
// at frame +0x1C, +0x18, +0x14 and +0x10. With the flag set the virtual slot 0x19C decides and the +0xF4 chain is walked with
// 0x0896A178(child, 1) and 0x08968A38(child) as the next child.
void func_0896A178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x24u}, {16u, 0x20u}, {31u, 0x28u}});
    ctx.gpr[17] = flag;
    ctx.gpr[16] = self;
    if (call_set(rt, ctx, 0x08965BD4u, {self}) != 2u) {
        call_set(rt, ctx, 0x08969B98u, {self, frame.at(0x0u), frame.at(0x4u), frame.at(0x8u), frame.at(0xCu)});
        call_set(rt, ctx, 0x08966040u, {self, 2u});
        put_float(rt, frame.at(0x1Cu), float_at(rt, frame.at(0x0u)));
        put_float(rt, frame.at(0x18u), float_at(rt, frame.at(0x4u)));
        put_float(rt, frame.at(0x14u), float_at(rt, frame.at(0x8u)));
        put_float(rt, frame.at(0x10u), float_at(rt, frame.at(0xCu)));
        call_set(rt, ctx, 0x08969D2Cu, {self, frame.at(0x1Cu), frame.at(0x18u), frame.at(0x14u), frame.at(0x10u)});
    }
    if (flag == 0u) return;
    const u32 method = word(rt, word(rt, self) + 0x19Cu);
    if (call_set(rt, ctx, method, {self}) == 0u) return;
    u32 child = word(rt, self + 0xF4u);
    ctx.gpr[16] = child;
    while (child != 0u) {
        call_set(rt, ctx, 0x0896A178u, {child, 1u});
        const u32 next = call_set(rt, ctx, 0x08968A38u, {child});
        if (next == 0u) break;
        child = next;
        ctx.gpr[16] = child;
    }
}
void func_08A34DBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); auto &F = ctx.fpr; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; G[5] = 0u; put_float(rt, frame.at(0x28u), F[22]); F[22] = F[14]; put_float(rt, frame.at(0x24u), F[21]); F[21] = F[13]; put_float(rt, frame.at(0x20u), F[20]); F[20] = F[12]; put(rt, frame.at(0x18u), G[31]); G[2] = word(rt, G[4] + 0x20u); G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08ABBEF8u); put(rt, G[17] + 0x20u, 0u); } G[4] = G[16]; G[6] = frame.at(4u); G[5] = frame.at(0u); if (G[16] != 0u) { call_here(rt, ctx, 0x08ABC430u); if (G[2] != 0u) { G[4] = word(rt, frame.at(0u)); G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08ABBE00u); G[16] = G[2]; F[12] = F[20]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08ABBFACu); F[12] = F[21]; G[1] = 0x3F800000u; F[13] = std::bit_cast<float>(G[1]); G[4] = G[16]; call_here(rt, ctx, 0x08ABBF54u); F[12] = F[22]; G[4] = G[16]; call_here(rt, ctx, 0x08ABBF80u); G[4] = G[16]; call_here(rt, ctx, 0x08ABC1A8u); put(rt, G[17] + 0x20u, G[16]); } } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); F[22] = float_at(rt, frame.at(0x28u)); F[21] = float_at(rt, frame.at(0x24u)); F[20] = float_at(rt, frame.at(0x20u)); } } }
// 0x08A7761C (a0 = the object, a1 = its argument, a2 = a value): 0x08A5D438(frame +4, argument) makes a slot that the object's virtual
// slot 0x18 (the word at the vtable's +0x18) fills with (object, frame +4, argument). When the slot is not empty, 0x08A76AAC(frame +8, frame)
// takes it (frame +0 holding the object; a null holder complains with 0x08A5BB7C(0x08CB0E6C)), the holder's virtual slot 0x20 (holder,
// frame +8, value) runs, and frame +8 is released (0x08A5B9EC). frame +4 is released either way; returns 1 when the holder ran, else 0.
void func_08A7761C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], argument = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {17u, 0x14u}, {31u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {16u, 0x10u}});
    const u32 source = frame.at(0x4u), slot = frame.at(0x8u);
    ctx.gpr[19] = source;
    ctx.gpr[17] = self;
    ctx.gpr[21] = value;
    ctx.gpr[20] = 0u;
    ctx.gpr[18] = slot;
    ctx.gpr[16] = argument;
    call_set(rt, ctx, 0x08A5D438u, {source, argument});
    call_set(rt, ctx, word(rt, word(rt, self) + 0x18u), {self, source, argument});
    u32 filled = 0u;
    if (word(rt, source) != 0u) {
        put(rt, frame.at(0x0u), self);              // the store of the call's delay slot
        call_set(rt, ctx, 0x08A76AACu, {slot, frame.at(0x0u)});
        filled = 1u;
        ctx.gpr[20] = filled;
        const u32 holder = word(rt, source);
        ctx.gpr[16] = holder;
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E6Cu});
        call_set(rt, ctx, word(rt, word(rt, holder) + 0x20u), {holder, slot, value});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {source});
    ctx.gpr[2] = filled;
}
void func_08AB3420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(8u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = G[20]; put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(0xCu); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); G[17] = frame.at(4u); call_here(rt, ctx, 0x08AB2D80u); G[4] = 0x08CB3BA0u; G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08AB3330u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; G[5] = G[19]; call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(0xCu)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08ADACC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = G[5]; G[5] = 0x08C90CE4u; put(rt, frame.at(0xCu), G[19]); G[19] = G[6]; put(rt, frame.at(4u), G[17]); G[17] = G[4]; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08AD9E74u); G[3] = 0u; if (G[2] != 0u) { G[18] = 0u; if (static_cast<std::int32_t>(G[19]) > 0) { do { G[4] = G[17]; call_here(rt, ctx, 0x08ADAB3Cu); G[3] = G[19] - G[18]; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[20] = G[21] + G[18]; G[4] = G[17]; G[16] = G[3]; if (G[2] == 0u) { call_here(rt, ctx, 0x08ADAB3Cu); G[16] = G[2]; } G[4] = G[17]; if (static_cast<std::int32_t>(G[16]) <= 0) break; G[18] += G[16]; call_here(rt, ctx, 0x08ADA9A0u); G[5] = G[2]; G[4] = G[20]; G[6] = G[16]; call_here(rt, ctx, 0x08B79A88u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08ADA86Cu); G[3] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[19]) ? 1u : 0u; } while (G[3] != 0u); } G[3] = G[18]; } G[2] = G[3]; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B278A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[8] = sext(G[6], 8); G[11] = G[4]; G[2] = 1u; if (G[8] == 0u) return; G[7] = word(rt, G[4] + 0x80u); G[6] = sext(load8(G[4] + 0x84u), 8); G[3] = G[2] << (G[8] & 31u); G[4] = load8(G[7]); G[9] = word(rt, G[11] + 0x78u); G[3] -= 1u; G[2] = G[2] << (G[6] & 31u); G[3] &= G[5]; G[2] -= 1u; G[10] = G[2] & G[4]; G[5] = G[3] << (G[6] & 31u); if (G[7] == G[9]) { G[2] = 0u; return; } G[2] = 8u - G[6]; G[4] = sext(G[2], 8); G[3] = G[10] | G[5]; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[8]) ? 1u : 0u; store8(G[7], G[3]); if (G[2] == 0u) { G[2] = sext(G[8] + G[6], 8); G[3] = sra(G[2], 3); G[4] = G[7] + G[3]; G[6] = G[2] & 7u; } else { G[2] = sext(G[8] - G[4], 8); G[6] = G[2] & 7u; while (true) { G[4] = G[7] + 1u; G[2] = sext(G[2] - 8u, 8); G[3] = G[9] < G[4] ? 1u : 0u; G[5] = G[5] >> 8; if (G[3] != 0u) { G[2] = 0u; return; } store8(G[7] + 1u, G[5]); if (static_cast<std::int32_t>(G[2]) <= 0) break; G[7] = G[4]; } if (G[2] == 0u) { G[4] = G[7] + 2u; G[2] = G[9] < G[4] ? 1u : 0u; if (G[2] != 0u) { G[2] = 0u; return; } } } G[2] = 1u; store8(G[11] + 0x84u, G[6]); put(rt, G[11] + 0x80u, G[4]); } }
void func_08C0EBC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[23]); G[2] = 0xAAAAAAABu; put(rt, frame.at(0x28u), G[22]); G[23] = G[4] + 4u; G[22] = G[6]; put(rt, frame.at(0x24u), G[21]); G[21] = G[4]; put(rt, frame.at(0x20u), G[20]); G[20] = G[5]; put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[3] = word(rt, G[4] + 4u); G[3] -= G[6]; G[3] = sra(G[3], 2); const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); G[19] = ctx.lo; G[17] = G[5]; if (static_cast<std::int32_t>(G[19]) > 0) { G[16] = G[6]; G[18] = 0u; do { G[18] += 1u; G[4] = G[17]; G[5] = G[16]; if (G[16] != G[17]) call_here(rt, ctx, 0x08C0E654u); G[16] += 0xCu; G[17] += 0xCu; } while (G[19] != G[18]); } G[5] = word(rt, G[23]); G[4] = G[17]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08C0DCF0u); G[3] = word(rt, G[21] + 4u); G[2] = (G[22] - G[20]) & ~3u; G[3] -= G[2]; put(rt, G[21] + 4u, G[3]); G[2] = G[20]; G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08C4E874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); if (G[5] != 0u) { G[21] = 0x08D77498u; do { G[5] = word(rt, G[16] + 0xCu); G[4] = G[20]; G[18] = G[16] + 0x10u; call_here(rt, ctx, 0x08C4E874u); G[2] = word(rt, G[16] + 0x14u); G[5] = 0xFFFFFFFFu; G[19] = word(rt, G[16] + 8u); G[17] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[17] != G[21]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08C74884u); } G[2] = word(rt, G[18]); G[5] = 0xFFFFFFFFu; G[17] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[17] != G[21]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08C74884u); } G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); G[16] = G[19]; } while (G[19] != 0u); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08BDA574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); G[20] = G[5]; put(rt, frame.at(0x28u), G[18]); G[18] = G[4]; put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); store8(frame.at(0x10u), G[6]); if (G[4] != G[5]) { G[16] = G[4] + 8u; if (G[5] != G[16]) { do { G[17] = word(rt, G[16]); G[5] = word(rt, G[18]); G[19] = word(rt, G[16] + 4u); G[4] = G[17]; call_here(rt, ctx, 0x08B35FD8u); G[6] = G[16]; if (static_cast<std::int32_t>(G[2]) >= 0) { G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(4u), G[19]); put(rt, frame.at(0u), G[17]); call_here(rt, ctx, 0x08BDA4F0u); G[6] = G[16] + 8u; } else { G[2] = G[16] - G[18]; G[7] = sra(G[2], 3); G[6] = G[16] + 8u; if (static_cast<std::int32_t>(G[7]) > 0) { G[4] = G[6]; G[5] = 0u; do { G[16] -= 8u; G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] -= 8u; G[5] += 1u; put(rt, G[4], G[2]); put(rt, G[4] + 4u, G[3]); } while (G[7] != G[5]); } put(rt, G[18] + 4u, G[19]); put(rt, G[18], G[17]); } G[16] = G[6]; } while (G[20] != G[6]); } } G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_08BDB668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[2] = G[5] << 1; G[7] = G[2] + 2u; G[2] = static_cast<std::int32_t>(G[7]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[12] = G[4]; G[14] = G[5]; G[3] = G[5]; G[4] = G[7]; if (G[2] != 0u) { G[11] = G[5]; while (true) { G[2] = G[7] << 3; G[4] = G[12] + G[2]; G[10] = word(rt, G[4] - 4u); G[5] = word(rt, G[4] + 4u); G[2] = G[11] << 3; G[3] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[10]) ? 1u : 0u; G[13] = G[2] + G[12]; bool step = G[3] != 0u; if (!step) { if (G[10] != G[5]) G[2] = word(rt, G[4]); else { G[2] = word(rt, G[4] - 8u); G[3] = word(rt, G[4]); G[2] = G[3] < G[2] ? 1u : 0u; if (G[2] == 0u) G[2] = word(rt, G[4]); else step = true; } } if (step) { G[7] -= 1u; G[2] = G[7] << 3; G[4] = G[12] + G[2]; G[2] = word(rt, G[4]); } G[3] = word(rt, G[4] + 4u); G[4] = G[7] << 1; G[4] += 2u; G[5] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[11] = G[7]; put(rt, G[13], G[2]); put(rt, G[13] + 4u, G[3]); if (G[5] == 0u) break; G[7] = G[4]; } G[3] = G[7]; } if (G[6] == G[4]) { G[2] = (G[6] << 3) + G[12]; G[4] = word(rt, G[2] - 8u); G[5] = word(rt, G[2] - 4u); G[3] = (G[3] << 3) + G[12]; put(rt, G[3], G[4]); put(rt, G[3] + 4u, G[5]); G[3] = G[6] - 1u; } G[4] = G[12]; G[5] = G[3]; G[6] = G[14]; jump(ctx, 0x08BDB598u); } }
void func_08BF94D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); G[2] = G[5] - G[4]; put(rt, frame.at(0x2Cu), G[19]); G[9] = G[4]; G[19] = G[7] << 1; G[4] = sra(G[2], 3); G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[19]) ? 1u : 0u; put(rt, frame.at(0x24u), G[17]); G[17] = G[5]; put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x10u), G[7]); if (G[2] == 0u) { G[18] = G[7] << 4; G[16] = G[9]; do { G[5] = word(rt, frame.at(0x10u)); G[4] = G[16]; G[7] = G[18] + G[9]; G[5] = G[5] << 3; G[5] = G[9] + G[5]; G[8] = G[6]; G[9] = G[16]; G[6] = G[5]; call_here(rt, ctx, 0x08BF93C8u); G[9] = G[18] + G[16]; G[6] = G[2]; G[2] = G[17] - G[9]; G[4] = sra(G[2], 3); G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[19]) ? 1u : 0u; G[16] = G[9]; } while (G[2] == 0u); } G[3] = word(rt, frame.at(0x10u)); G[2] = frame.at(0x10u); put(rt, frame.at(0u), G[4]); G[3] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[4]) ? 1u : 0u; if (G[3] == 0u) G[2] = frame.at(0u); G[10] = word(rt, G[2]); G[4] = G[9]; G[7] = G[17]; G[5] = G[10] << 3; G[5] += G[9]; G[8] = G[6]; G[9] = G[16]; G[6] = G[5]; put(rt, frame.at(0x10u), G[10]); call_here(rt, ctx, 0x08BF93C8u); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_08C4B284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); G[2] = 0xB6DB6DB7u; put(rt, frame.at(0x14u), G[17]); G[18] = G[6]; G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[12] = G[4] + 4u; G[16] = G[5]; put(rt, frame.at(0x1Cu), G[31]); G[3] = word(rt, G[4] + 4u); G[3] -= G[6]; G[3] = sra(G[3], 4); const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); G[11] = ctx.lo; G[9] = G[5]; if (static_cast<std::int32_t>(G[11]) > 0) { G[2] = G[6]; G[10] = 0u; do { G[6] = G[2]; G[7] = G[9]; G[8] = G[2] + 0x70u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[10] += 1u; G[2] = G[8]; G[9] += 0x70u; } while (G[11] != G[10]); } G[5] = word(rt, G[12]); G[6] = frame.at(0u); G[4] = G[9]; call_here(rt, ctx, 0x08C4B274u); G[3] = word(rt, G[17] + 4u); G[2] = (G[18] - G[16]) & ~0xFu; G[3] -= G[2]; put(rt, G[17] + 4u, G[3]); G[2] = G[16]; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_265
} // namespace gt::game

#define HOT265(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_265::name, gt::ReturnKind::kind)
HOT265(0x08B71F2Cu, func_08B71F2C, Word);
HOT265(0x08949F44u, func_08949F44, Word);
HOT265(0x08B1C68Cu, func_08B1C68C, Word);
HOT265(0x08965DF8u, func_08965DF8, Word);
HOT265(0x088858B0u, func_088858B0, Word);
HOT265(0x089590A4u, func_089590A4, Word);
HOT265(0x0896A09Cu, func_0896A09C, Word);
HOT265(0x08860664u, func_08860664, Word);
HOT265(0x0881ACC0u, func_0881ACC0, Word);
HOT265(0x0896A178u, func_0896A178, Word);
HOT265(0x08A34DBCu, func_08A34DBC, Word);
HOT265(0x08A7761Cu, func_08A7761C, Word);
// HOT265(0x08AB3420u, func_08AB3420, Word);  // no comparable synthetic case yet
HOT265(0x08ADACC8u, func_08ADACC8, Word);
HOT265(0x08B278A0u, func_08B278A0, Word);
// HOT265(0x08C0EBC0u, func_08C0EBC0, Word);  // no comparable synthetic case yet
HOT265(0x08C4E874u, func_08C4E874, Word);
HOT265(0x08BDA574u, func_08BDA574, Word);
// HOT265(0x08BDB668u, func_08BDB668, Word);  // no comparable synthetic case yet
HOT265(0x08BF94D8u, func_08BF94D8, Word);
// HOT265(0x08C4B284u, func_08C4B284, Word);  // no comparable synthetic case yet
