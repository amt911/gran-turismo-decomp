// Hot batch 222: fifty-six- to fifty-nine-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_222 {
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
    if (!(value >= -2227483648.0f && value < 2227483648.0f)) return 0x7FFFFFFF;
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


// 0x08BAF4A8 and eight copies: the destructor of an object whose vtable word is set to `first_vtable` and later to 0x08CC3000. The object held at
// +0x2C is dropped — a lock-free decrement of its count at +4; at zero its virtual +8, and when its count at +8 reaches zero too, its virtual +0xC —
// the one at +0x24 (count at +8) runs its virtual +0xC. v0 ends at 0x08CC3000, as in the asm.
void release_held_members(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 first_vtable) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[17] = object;
    // a lock-free decrement of the count at `at` (the asm's ll/sc loop): true when it reached zero
    auto release = [&](u32 at) {
        const u32 count = word(rt, at);
        put(rt, at, count - 0x1u);
        return count == 0x1u;
    };
    const u32 first_held = word(rt, object + 0x2Cu);
    put(rt, object, first_vtable);
    // the second held object is read where the asm's branch delays read it: after the first one's virtual +8 call
    u32 second_held;
    if (first_held == 0u || !release(first_held + 0x4u)) {
        second_held = word(rt, object + 0x24u);
    } else {
        call_set(rt, ctx, word(rt, word(rt, first_held) + 0x8u), {first_held});
        second_held = word(rt, object + 0x24u);
        if (release(first_held + 0x8u)) call_set(rt, ctx, word(rt, word(rt, first_held) + 0xCu), {first_held});
    }
    if (second_held != 0u && release(second_held + 0x8u)) call_set(rt, ctx, word(rt, word(rt, second_held) + 0xCu), {second_held});
    put(rt, object, 0x08CC3000u);
    ctx.gpr[2] = 0x08CC3000u;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

// 0x08BAF4A8 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC2FE0, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BAF4A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC2FE0u); }
void func_08BAF3C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC2FE0u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_089C2AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x089C2238u); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA6138u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x90u, ctx.fpr[0]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x089C2238u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x90u); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_089D2EC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089CD62Cu); G[16] = word(rt, frame.at(4u)); G[5] = G[16]; if (G[16] == 0u) { G[4] = 0x08CA69B8u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; } G[4] = frame.at(0u); call_here(rt, ctx, 0x089CC8DCu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[18] = frame.at(4u); G[4] = G[18]; G[17] = frame.at(8u); call_here(rt, ctx, 0x089CD62Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA69B8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089D2EA8u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08970A94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x20u), G[16]); G[18] = 0x08D74FD0u; G[6] = G[5]; G[17] = G[4]; G[7] = G[18] + 0xCu; G[2] = word(rt, G[4] + 0xE4u); G[4] = G[5]; G[2] &= 0x20u; G[5] = frame.at(8u); if (G[2] == 0u) { G[3] = word(rt, G[17] + 0xE0u); G[2] = 0xFu; G[3] = (G[3] & ~0xF0u) | (G[2] << 4); put(rt, G[17] + 0xE0u, G[3]); } G[6] = word(rt, G[6]); G[3] = 0xFFFFFFFFu; G[2] = 0x08CCD978u; put(rt, frame.at(0x10u), G[3]); G[3] = frame.at(4u); put(rt, frame.at(0x14u), G[3]); put(rt, frame.at(0xCu), G[7]); put(rt, frame.at(8u), G[2]); G[2] = word(rt, G[6] + 8u); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0xCu)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(8u), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = word(rt, frame.at(4u)); G[4] = G[17]; G[5] = 1u; call_here(rt, ctx, 0x08965DF8u); G[2] = word(rt, G[17] + 0xE4u) | 0x20u; put(rt, G[17] + 0xE4u, G[2]); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } }
// Adds one entry to the target behind the +4 slot when the count is positive: resolves the slot with 0x088297A8 and
// 0x088294D0 (asserting the inner pointer) and releases it, turns it into the target with 0x0885EE58, asks the node at
// the source pointer (asserted) for its +0x3C virtual value and hands both to 0x08893684; the result is published
// from the +0 temporary with 0x08A85B1C, moved into the object and the temporary released.
void func_08830998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], count = ctx.gpr[6], source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {19u, 0x1Cu}, {18u, 0x18u}, {31u, 0x20u}, {16u, 0x10u}});
    const u32 slot = frame.at(0x4u), temporary = frame.at(0u);
    ctx.gpr[17] = slot;
    ctx.gpr[19] = object;
    ctx.gpr[18] = source;
    if (static_cast<s32>(count) <= 0) return;
    call_set(rt, ctx, 0x088297A8u, {slot, ctx.gpr[5]});
    const u32 holder = word(rt, slot);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 inner = call_set(rt, ctx, 0x088294D0u, {holder});
    ctx.gpr[16] = inner;
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    const u32 target = call_set(rt, ctx, 0x0885EE58u, {inner});
    ctx.gpr[17] = target;
    const u32 node = word(rt, source);
    ctx.gpr[16] = node;
    if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
    const u32 chosen = call_set(rt, ctx, word(rt, word(rt, node) + 0x3Cu), {node});
    const u32 made = call_set(rt, ctx, 0x08893684u, {target, chosen});
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, made});
    if (object != temporary) call_set(rt, ctx, 0x08A5BAD8u, {object, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}
void func_089FD0D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[17]); G[2] = 1u; G[19] = frame.at(8u); G[4] = G[19]; G[18] = frame.at(4u); G[16] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x089FCC58u); G[17] = word(rt, frame.at(8u)); G[4] = 0x08CA878Cu; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16]); G[4] = 0x08CA8740u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x089FCA70u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
// 0x08BC03C8 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC5220, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BC03C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC5220u); }
// 0x08BD0230 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC53C0, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BD0230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC53C0u); }
// 0x08BD3B5C (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC5550, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BD3B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC5550u); }
// 0x08BD6CA4 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC55A8, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BD6CA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC55A8u); }
// 0x08BD6D84 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC55A8, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BD6D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC55A8u); }
// 0x08BD72A8 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC5628, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BD72A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC5628u); }
void func_08BD7388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC5628u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } }
// 0x08BE0CF8 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC6888, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BE0CF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC6888u); }
// 0x08BE12FC (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC68F0, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
void func_08BE12FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC68F0u); }
void func_088344F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], count = ctx.gpr[6], source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {19u, 0x1Cu}, {18u, 0x18u}, {31u, 0x20u}, {16u, 0x10u}});
    const u32 slot = frame.at(0x4u), temporary = frame.at(0u);
    ctx.gpr[17] = slot;
    ctx.gpr[19] = object;
    ctx.gpr[18] = source;
    if (static_cast<s32>(count) <= 0) return;
    call_set(rt, ctx, 0x088297A8u, {slot, ctx.gpr[5]});
    const u32 holder = word(rt, slot);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 inner = call_set(rt, ctx, 0x088294D0u, {holder});
    ctx.gpr[16] = inner;
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    const u32 target = call_set(rt, ctx, 0x0885EDECu, {inner});
    ctx.gpr[17] = target;
    const u32 node = word(rt, source);
    ctx.gpr[16] = node;
    if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
    const u32 chosen = call_set(rt, ctx, word(rt, word(rt, node) + 0x3Cu), {node});
    const u32 made = call_set(rt, ctx, 0x088D6958u, {target, chosen});
    call_set(rt, ctx, 0x08A97A6Cu, {temporary, made});
    if (object != temporary) call_set(rt, ctx, 0x08A5BAD8u, {object, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}
// Adds one entry to the target behind the +4 slot when the count is positive: resolves the slot with 0x088297A8 and
// 0x088294D0 (asserting the inner pointer) and releases it, turns it into the target with 0x0885EDEC, asks the node at
// the source pointer (asserted) for its +0x3C virtual value and hands both to 0x088D6958; the result is published
// from the +0 temporary with 0x08A97A6C, moved into the object and the temporary released.
void func_08BC02E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC5220u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08BD014C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC53C0u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08BD3A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC5550u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08BD6BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC55A8u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08BD71C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC5628u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08BE0C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC6888u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08BE1218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC68F0u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[4] = G[17]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08A6DE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[17]); G[2] = 1u; G[19] = frame.at(8u); G[4] = G[19]; G[18] = frame.at(4u); G[16] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A6C410u); G[17] = word(rt, frame.at(8u)); G[4] = 0x08CB0754u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16]); G[4] = 0x08CB06B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x08A6C5C8u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_08AB45B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[17]); G[2] = 1u; G[19] = frame.at(8u); G[4] = G[19]; G[18] = frame.at(4u); G[16] = G[7]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08AB3FC0u); G[17] = word(rt, frame.at(8u)); G[4] = 0x08CB3D40u; if (G[17] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16]); G[4] = 0x08CB3D0Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = G[18]; call_here(rt, ctx, G[2]); G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x08AB3DF8u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_089C2BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x089C2238u); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA6138u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x94u, ctx.fpr[0]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x089C2238u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x94u); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08A394CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x08A391F0u); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08CAD378u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1Cu, ctx.fpr[0]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x08A391F0u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CAD378u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1Cu); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_089D2FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089CD62Cu); G[16] = word(rt, frame.at(4u)); G[5] = G[16]; if (G[16] == 0u) { G[4] = 0x08CA69B8u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; } G[4] = frame.at(0u); call_here(rt, ctx, 0x089CC908u); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[18] = frame.at(4u); G[4] = G[18]; G[17] = frame.at(8u); call_here(rt, ctx, 0x089CD62Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA69B8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089D2FB0u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_089EBB1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089E5E50u); G[16] = word(rt, frame.at(4u)); G[5] = G[16]; if (G[16] == 0u) { G[4] = 0x08CA7A10u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; } G[4] = frame.at(0u); call_here(rt, ctx, 0x089E4D5Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[18] = frame.at(4u); G[4] = G[18]; G[17] = frame.at(8u); call_here(rt, ctx, 0x089E5E50u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA7A10u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089EBB00u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_089EBC24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089E5E50u); G[16] = word(rt, frame.at(4u)); G[5] = G[16]; if (G[16] == 0u) { G[4] = 0x08CA7A10u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; } G[4] = frame.at(0u); call_here(rt, ctx, 0x089E4D88u); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[18] = frame.at(4u); G[4] = G[18]; G[17] = frame.at(8u); call_here(rt, ctx, 0x089E5E50u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA7A10u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089EBC08u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_089EBD2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089E5E50u); G[16] = word(rt, frame.at(4u)); G[5] = G[16]; if (G[16] == 0u) { G[4] = 0x08CA7A10u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; } G[4] = frame.at(0u); call_here(rt, ctx, 0x089E4DB4u); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[18] = frame.at(4u); G[4] = G[18]; G[17] = frame.at(8u); call_here(rt, ctx, 0x089E5E50u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA7A10u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089EBD10u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08970B80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x20u), G[16]); G[18] = 0x08D74FD0u; G[6] = G[5]; G[17] = G[4]; G[7] = G[18] + 0xCu; G[2] = word(rt, G[4] + 0xE4u); G[4] = G[5]; G[2] &= 0x20u; G[5] = frame.at(8u); if (G[2] == 0u) { G[3] = word(rt, G[17] + 0xE0u); G[2] = 0xFu; G[3] = (G[3] & ~0xF0u) | (G[2] << 4); put(rt, G[17] + 0xE0u, G[3]); } G[6] = word(rt, G[6]); G[3] = 0xFFFFFFFFu; G[2] = 0x08CCD978u; put(rt, frame.at(0x10u), G[3]); G[3] = frame.at(4u); put(rt, frame.at(0x14u), G[3]); put(rt, frame.at(0xCu), G[7]); put(rt, frame.at(8u), G[2]); G[2] = word(rt, G[6] + 8u); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0xCu)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(8u), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = word(rt, frame.at(4u)); G[4] = G[17]; G[5] = 2u; call_here(rt, ctx, 0x08965DF8u); G[2] = word(rt, G[17] + 0xE4u) | 0x20u; put(rt, G[17] + 0xE4u, G[2]); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } }
void func_08970C6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x20u), G[16]); G[18] = 0x08D74FD0u; G[6] = G[5]; G[17] = G[4]; G[7] = G[18] + 0xCu; G[2] = word(rt, G[4] + 0xE4u); G[4] = G[5]; G[2] &= 0x20u; G[5] = frame.at(8u); if (G[2] == 0u) { G[3] = word(rt, G[17] + 0xE0u); G[2] = 0xFu; G[3] = (G[3] & ~0xF0u) | (G[2] << 4); put(rt, G[17] + 0xE0u, G[3]); } G[6] = word(rt, G[6]); G[3] = 0xFFFFFFFFu; G[2] = 0x08CCD978u; put(rt, frame.at(0x10u), G[3]); G[3] = frame.at(4u); put(rt, frame.at(0x14u), G[3]); put(rt, frame.at(0xCu), G[7]); put(rt, frame.at(8u), G[2]); G[2] = word(rt, G[6] + 8u); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0xCu)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(8u), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = word(rt, frame.at(4u)); G[4] = G[17]; G[5] = 8u; call_here(rt, ctx, 0x08965DF8u); G[2] = word(rt, G[17] + 0xE4u) | 0x20u; put(rt, G[17] + 0xE4u, G[2]); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } }
void func_08970D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x20u), G[16]); G[18] = 0x08D74FD0u; G[6] = G[5]; G[17] = G[4]; G[7] = G[18] + 0xCu; G[2] = word(rt, G[4] + 0xE4u); G[4] = G[5]; G[2] &= 0x20u; G[5] = frame.at(8u); if (G[2] == 0u) { G[3] = word(rt, G[17] + 0xE0u); G[2] = 0xFu; G[3] = (G[3] & ~0xF0u) | (G[2] << 4); put(rt, G[17] + 0xE0u, G[3]); } G[6] = word(rt, G[6]); G[3] = 0xFFFFFFFFu; G[2] = 0x08CCD978u; put(rt, frame.at(0x10u), G[3]); G[3] = frame.at(4u); put(rt, frame.at(0x14u), G[3]); put(rt, frame.at(0xCu), G[7]); put(rt, frame.at(8u), G[2]); G[2] = word(rt, G[6] + 8u); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0xCu)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(8u), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = word(rt, frame.at(4u)); G[4] = G[17]; G[5] = 4u; call_here(rt, ctx, 0x08965DF8u); G[2] = word(rt, G[17] + 0xE4u) | 0x20u; put(rt, G[17] + 0xE4u, G[2]); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_222
} // namespace gt::game

#define HOT222(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_222::name, gt::ReturnKind::kind)
HOT222(0x08BAF4A8u, func_08BAF4A8, Word);
HOT222(0x08BAF3C4u, func_08BAF3C4, Word);
// HOT222(0x089C2AC8u, func_089C2AC8, Word);  // no comparable synthetic case yet
HOT222(0x089D2EC4u, func_089D2EC4, Word);
HOT222(0x08970A94u, func_08970A94, Word);
HOT222(0x08830998u, func_08830998, Word);
HOT222(0x089FD0D8u, func_089FD0D8, Word);
HOT222(0x08BC03C8u, func_08BC03C8, Word);
HOT222(0x08BD0230u, func_08BD0230, Word);
HOT222(0x08BD3B5Cu, func_08BD3B5C, Word);
HOT222(0x08BD6CA4u, func_08BD6CA4, Word);
HOT222(0x08BD6D84u, func_08BD6D84, Word);
HOT222(0x08BD72A8u, func_08BD72A8, Word);
HOT222(0x08BD7388u, func_08BD7388, Word);
HOT222(0x08BE0CF8u, func_08BE0CF8, Word);
HOT222(0x08BE12FCu, func_08BE12FC, Word);
HOT222(0x088344F4u, func_088344F4, Word);
HOT222(0x08BC02E4u, func_08BC02E4, Word);
HOT222(0x08BD014Cu, func_08BD014C, Word);
HOT222(0x08BD3A78u, func_08BD3A78, Word);
HOT222(0x08BD6BC0u, func_08BD6BC0, Word);
HOT222(0x08BD71C4u, func_08BD71C4, Word);
HOT222(0x08BE0C14u, func_08BE0C14, Word);
HOT222(0x08BE1218u, func_08BE1218, Word);
HOT222(0x08A6DE64u, func_08A6DE64, Word);
HOT222(0x08AB45B4u, func_08AB45B4, Word);
// HOT222(0x089C2BB0u, func_089C2BB0, Word);  // no comparable synthetic case yet
// HOT222(0x08A394CCu, func_08A394CC, Word);  // no comparable synthetic case yet
HOT222(0x089D2FCCu, func_089D2FCC, Word);
HOT222(0x089EBB1Cu, func_089EBB1C, Word);
HOT222(0x089EBC24u, func_089EBC24, Word);
HOT222(0x089EBD2Cu, func_089EBD2C, Word);
HOT222(0x08970B80u, func_08970B80, Word);
HOT222(0x08970C6Cu, func_08970C6C, Word);
HOT222(0x08970D58u, func_08970D58, Word);
