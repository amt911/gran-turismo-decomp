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
void func_08BAF3C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC2FE0u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC2FE0u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08BAF3C4 (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC2FE0 at +0 and releases the child at
// +0x2C (the count at +4 drops and at zero the virtual +8 runs, then the count at +8 drops and at zero the virtual +0xC runs), then
// releases the object at +0x24 through its count at +8 only (at zero the virtual +0xC runs), stores the vtable 0x08CC3000 at +0 and
// jumps to 0x08B64654 with the object. v0 = 0x08CC3000.
void func_089C2AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x089C2238u); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA6138u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x90u, ctx.fpr[0]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x089C2238u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x90u); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
// 0x089D2EC4 (a0 = the destination string, a2 = a mode, a3 = a text): mode 0 builds the slot at frame +4 (0x089CD62C) whose word (0x08A5BB7C(
// 0x08CA69B8) when null) 0x089CC8DC turns into the string at frame +0, which the destination takes (0x08A5BAD8 unless it is that slot); mode 1 takes
// the same slot, the text a3 goes into the slot at frame +8 (0x08A76ABC(text)) and 0x089D2EA8(word, frame +8) runs. Both paths release frame +0 and
// frame +4 (0x08A5B9EC); the string at frame +8 is left alone. Any other mode does nothing. v0 = 1 when neither mode matched.
void func_089D2EC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], mode = ctx.gpr[6], text = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = destination;
    ctx.gpr[16] = text;
    const u32 slot = frame.at(0x4u), other = frame.at(0x8u), built = frame.at(0x0u);
    if (mode == 0u) {
        ctx.gpr[18] = slot;
        call_set(rt, ctx, 0x089CD62Cu, {slot});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        call_set(rt, ctx, 0x089CC8DCu, {built, held});
        if (destination != built) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, built)});
        call_set(rt, ctx, 0x08A5B9ECu, {built});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else if (mode == 1u) {
        ctx.gpr[18] = slot;
        ctx.gpr[17] = other;
        call_set(rt, ctx, 0x089CD62Cu, {slot});
        call_set(rt, ctx, 0x08A76ABCu, {other, text});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        call_set(rt, ctx, 0x089D2EA8u, {held, other});
        call_set(rt, ctx, 0x08A5B9ECu, {other});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else {
        ctx.gpr[2] = 1u;
    }
}
// 0x08970A94 (a0 = the object, a1 = the argument): clears the +0xE0 nibble when bit 0x20 of +0xE4 is clear, calls the virtual
// slot +8 of word(a1) with a 16-byte record on the frame (0x08CCD978, base+0xC, -1, frame+4), releases the counted block behind
// the record's +0xC when it is not base (0x08D74FD0), then calls 0x08965DF8(object, 1, frame+4). v0 = the new +0xE4 word.
void func_08970A94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x28u}, {17u, 0x24u}, {31u, 0x2Cu}, {16u, 0x20u}});
    const u32 base = 0x08D74FD0u;
    ctx.gpr[18] = base;
    ctx.gpr[17] = object;
    const u32 slot = base + 0xCu;
    if ((word(rt, object + 0xE4u) & 0x20u) == 0u) {
        put(rt, object + 0xE0u, (word(rt, object + 0xE0u) & ~(0xFu << 4)) | (0xFu << 4));
    }
    const u32 table = word(rt, argument);
    put(rt, frame.at(0x10u), 0xFFFFFFFFu);
    put(rt, frame.at(0x14u), frame.at(0x4u));
    put(rt, frame.at(0xCu), slot);
    put(rt, frame.at(0x8u), 0x08CCD978u);
    call_set(rt, ctx, word(rt, table + 0x8u), {argument, frame.at(0x8u), table, slot});
    const u32 released = word(rt, frame.at(0xCu));
    ctx.gpr[16] = released - 0xCu;
    put(rt, frame.at(0x8u), 0x08CCD990u);
    if (ctx.gpr[16] != base) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {released - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {ctx.gpr[16], frame.at(0u)});
    }
    call_set(rt, ctx, 0x08965DF8u, {object, 0x1u, word(rt, frame.at(0x4u))});
    ctx.gpr[2] = word(rt, object + 0xE4u) | 0x20u;
    put(rt, object + 0xE4u, ctx.gpr[2]);
}
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

// 0x089FD0D8 (a2 = a mode word, a3 = a holder whose word +0 is a source; v0 = 1 or the last call's): only mode 1 does anything:
// 0x089FCC58(sp + 8) reads a handle into sp + 8 (null: 0x08A5BB7C(0x08CA878C)), the source (null: 0x08A5BB7C(0x08CA8740)) has its virtual
// +0xC called with (sp + 4, source) and 0x089FCA70(handle, sp + 4) runs; the counted string at sp + 4 is released (empty header
// 0x08D74FD0: no count; else 0x08B60804(data - 4, -1) and, at zero or below, 0x08B8542C(header, sp)) and 0x08A5B9EC(sp + 8) frees the
// handle.
void func_089FD0D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {31u, 0x20u}, {17u, 0x14u}});
    ctx.gpr[2] = 1u;
    ctx.gpr[19] = frame.at(0x8u);
    ctx.gpr[18] = frame.at(0x4u);
    ctx.gpr[16] = holder;
    if (mode != 1u) return;
    call_set(rt, ctx, 0x089FCC58u, {frame.at(0x8u), ctx.gpr[5]});
    const u32 handle = word(rt, frame.at(0x8u));
    ctx.gpr[17] = handle;
    if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA878Cu});
    const u32 source = word(rt, ctx.gpr[16]);
    ctx.gpr[16] = source;
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8740u});
    call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {frame.at(0x4u), source});
    call_set(rt, ctx, 0x089FCA70u, {handle, frame.at(0x4u)});
    const u32 data = word(rt, frame.at(0x4u));
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    ctx.gpr[2] = 0x08D74FD0u;
    if (header != 0x08D74FD0u) {
        const u32 remaining = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(remaining) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
}

void func_08BC03C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC5220u); }
void func_08BD0230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC53C0u); }
void func_08BD3B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC5550u); }
void func_08BD6CA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC55A8u); }
void func_08BD6D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC55A8u); }
void func_08BD72A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC5628u); }
void func_08BD7388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CC5628u; G[17] = G[4]; G[16] = word(rt, G[4] + 0x2Cu); put(rt, G[4], G[2]); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } } G[4] = word(rt, G[17] + 0x24u); if (G[4] != 0u) { G[3] = G[4] + 8u; G[5] = word(rt, G[3]); G[2] = G[5] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[5] == G[2]) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); } } G[2] = 0x08CC3000u; put(rt, G[17], G[2]); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } }
void func_08BE0CF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC6888u); }
void func_08BE12FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_held_members(rt, ctx, 0x08CC68F0u); }
// 0x08BC03C8 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC5220, the object held at +0x2C is dropped
// (a lock-free decrement of its count at +4; at zero its virtual +8, then when its count at +8 reaches zero too, its virtual +0xC), the one at +0x24
// (count at +8) runs its virtual +0xC, and the vtable word becomes 0x08CC3000.
// 0x08BD0230 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC53C0, the object held at +0x2C is dropped
// 0x08BD3B5C (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC5550, the object held at +0x2C is dropped
// 0x08BD6CA4 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC55A8, the object held at +0x2C is dropped
// 0x08BD6D84 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC55A8, the object held at +0x2C is dropped
// 0x08BD72A8 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC5628, the object held at +0x2C is dropped
// 0x08BE0CF8 (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC6888, the object held at +0x2C is dropped
// 0x08BE12FC (a0 = the object; v0 = 0x08CC3000): the object's destructor. Its vtable word becomes 0x08CC68F0, the object held at +0x2C is dropped
// 0x089FD0D8 (a2 = a mode word, a3 = a holder whose word +0 is a source; v0 = 1 or the last call's): only mode 1 does anything:
// 0x089FCC58(sp + 8) reads a handle into sp + 8 (null: 0x08A5BB7C(0x08CA878C)), the source (null: 0x08A5BB7C(0x08CA8740)) has its virtual
// +0xC called with (sp + 4, source) and 0x089FCA70(handle, sp + 4) runs; the counted string at sp + 4 is released (empty header
// 0x08D74FD0: no count; else 0x08B60804(data - 4, -1) and, at zero or below, 0x08B8542C(header, sp)) and 0x08A5B9EC(sp + 8) frees the
// handle.
// Adds one entry to the target behind the +4 slot when the count is positive: resolves the slot with 0x088297A8 and
// 0x088294D0 (asserting the inner pointer) and releases it, turns it into the target with 0x0885EE58, asks the node at
// the source pointer (asserted) for its +0x3C virtual value and hands both to 0x08893684; the result is published
// from the +0 temporary with 0x08A85B1C, moved into the object and the temporary released.
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

// 0x08BC02E4 (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC5220 at +0 and releases the child at
// +0x2C (the count at +4 drops and at zero the virtual +8 runs, then the count at +8 drops and at zero the virtual +0xC runs), then
// releases the object at +0x24 through its count at +8 only (at zero the virtual +0xC runs), stores the vtable 0x08CC3000 at +0 and
// jumps to 0x08B64654 with the object. v0 = 0x08CC3000.
void func_08BC02E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC5220u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC5220u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08BD014C (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC53C0 at +0 and releases the child at
void func_08BD014C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC53C0u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC53C0u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08BD3A78 (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC5550 at +0 and releases the child at
void func_08BD3A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC5550u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC5550u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08BD6BC0 (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC55A8 at +0 and releases the child at
void func_08BD6BC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC55A8u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC55A8u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08BD71C4 (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC5628 at +0 and releases the child at
void func_08BD71C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC5628u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC5628u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08BE0C14 (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC6888 at +0 and releases the child at
void func_08BE0C14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC6888u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC6888u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08BE1218 (a0 = the object; ends with the tail call 0x08B64654(object)): stores the vtable 0x08CC68F0 at +0 and releases the child at
void func_08BE1218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[2] = 0x08CC68F0u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = word(rt, self + 0x2Cu);
    put(rt, self, 0x08CC68F0u);
    if (ctx.gpr[16] != 0u) {
        const u32 count = word(rt, ctx.gpr[16] + 0x4u);
        put(rt, ctx.gpr[16] + 0x4u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0x8u), {ctx.gpr[16]});
            const u32 second = word(rt, ctx.gpr[16] + 0x8u);
            put(rt, ctx.gpr[16] + 0x8u, second - 1u);
            ctx.gpr[2] = 1u;
            if (second == 1u) call_indirect(rt, ctx, word(rt, word(rt, ctx.gpr[16]) + 0xCu), {ctx.gpr[16]});
        }
    }
    const u32 owned = word(rt, self + 0x24u);
    if (owned != 0u) {
        const u32 count = word(rt, owned + 0x8u);
        put(rt, owned + 0x8u, count - 1u);
        ctx.gpr[2] = 1u;
        if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, owned) + 0xCu), {owned});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
    tail_call(ctx, 0x08B64654u, {self});
}
// 0x08A6DE64 (a2 = a mode word, a3 = a holder whose word +0 is a source; v0 = 1 or the last call's): only mode 1 does anything:
// 0x08A6C410(sp + 8) reads a handle into sp + 8 (null: 0x08A5BB7C(0x08CB0754)), the source (null: 0x08A5BB7C(0x08CB06B8)) has its virtual
// +0xC called with (sp + 4, source) and 0x08A6C5C8(handle, sp + 4) runs; the counted string at sp + 4 is released (empty header
// 0x08D74FD0: no count; else 0x08B60804(data - 4, -1) and, at zero or below, 0x08B8542C(header, sp)) and 0x08A5B9EC(sp + 8) frees the
// handle.
void func_08A6DE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {31u, 0x20u}, {17u, 0x14u}});
    ctx.gpr[2] = 1u;
    ctx.gpr[19] = frame.at(0x8u);
    ctx.gpr[18] = frame.at(0x4u);
    ctx.gpr[16] = holder;
    if (mode != 1u) return;
    call_set(rt, ctx, 0x08A6C410u, {frame.at(0x8u), ctx.gpr[5]});
    const u32 handle = word(rt, frame.at(0x8u));
    ctx.gpr[17] = handle;
    if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0754u});
    const u32 source = word(rt, ctx.gpr[16]);
    ctx.gpr[16] = source;
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB06B8u});
    call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {frame.at(0x4u), source});
    call_set(rt, ctx, 0x08A6C5C8u, {handle, frame.at(0x4u)});
    const u32 data = word(rt, frame.at(0x4u));
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    ctx.gpr[2] = 0x08D74FD0u;
    if (header != 0x08D74FD0u) {
        const u32 remaining = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(remaining) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
}
// 0x08AB45B4 (a2 = a mode word, a3 = a holder whose word +0 is a source; v0 = 1 or the last call's): only mode 1 does anything:
// 0x08AB3FC0(sp + 8) reads a handle into sp + 8 (null: 0x08A5BB7C(0x08CB3D40)), the source (null: 0x08A5BB7C(0x08CB3D0C)) has its virtual
// +0xC called with (sp + 4, source) and 0x08AB3DF8(handle, sp + 4) runs; the counted string at sp + 4 is released (empty header
void func_08AB45B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {31u, 0x20u}, {17u, 0x14u}});
    ctx.gpr[2] = 1u;
    ctx.gpr[19] = frame.at(0x8u);
    ctx.gpr[18] = frame.at(0x4u);
    ctx.gpr[16] = holder;
    if (mode != 1u) return;
    call_set(rt, ctx, 0x08AB3FC0u, {frame.at(0x8u), ctx.gpr[5]});
    const u32 handle = word(rt, frame.at(0x8u));
    ctx.gpr[17] = handle;
    if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB3D40u});
    const u32 source = word(rt, ctx.gpr[16]);
    ctx.gpr[16] = source;
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB3D0Cu});
    call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {frame.at(0x4u), source});
    call_set(rt, ctx, 0x08AB3DF8u, {handle, frame.at(0x4u)});
    const u32 data = word(rt, frame.at(0x4u));
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    ctx.gpr[2] = 0x08D74FD0u;
    if (header != 0x08D74FD0u) {
        const u32 remaining = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(remaining) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
}

void func_089C2BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x089C2238u); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA6138u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x94u, ctx.fpr[0]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x089C2238u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA6164u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x94u); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
void func_08A394CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x08A391F0u); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08CAD378u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CAD34Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); put_float(rt, G[17] + 0x1Cu, ctx.fpr[0]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x08A391F0u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CAD378u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[12] = float_at(rt, G[16] + 0x1Cu); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } }
// 0x089D2FCC (a0 = the destination string, a2 = a mode, a3 = a text): the same two modes as func_089D2EC4 with the other formatter: mode 0 builds the
// slot at frame +4 (0x089CD62C) whose word (0x08A5BB7C(0x08CA69B8) when null) 0x089CC908 turns into the string at frame +0, which the destination
// takes (0x08A5BAD8 unless it is that slot); mode 1 takes the same slot, the text a3 goes into the slot at frame +8 (0x08A76ABC(text)) and
// 0x089D2FB0(word, frame +8) runs. Both paths release frame +0 and frame +4 (0x08A5B9EC). Any other mode does nothing. v0 = 1 when neither mode matched.
void func_089D2FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], mode = ctx.gpr[6], text = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = destination;
    ctx.gpr[16] = text;
    const u32 slot = frame.at(0x4u), other = frame.at(0x8u), built = frame.at(0x0u);
    if (mode == 0u) {
        ctx.gpr[18] = slot;
        call_set(rt, ctx, 0x089CD62Cu, {slot});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        call_set(rt, ctx, 0x089CC908u, {built, held});
        if (destination != built) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, built)});
        call_set(rt, ctx, 0x08A5B9ECu, {built});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else if (mode == 1u) {
        ctx.gpr[18] = slot;
        ctx.gpr[17] = other;
        call_set(rt, ctx, 0x089CD62Cu, {slot});
        call_set(rt, ctx, 0x08A76ABCu, {other, text});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        call_set(rt, ctx, 0x089D2FB0u, {held, other});
        call_set(rt, ctx, 0x08A5B9ECu, {other});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else {
        ctx.gpr[2] = 1u;
    }
}
// 0x089EBB1C (a0 = the destination string, a2 = a mode, a3 = a text): mode 0 builds the slot at frame +4 (0x089E5E50) whose word
// (0x08A5BB7C(0x08CA7A10) when null) 0x089E4D5C turns into the string at frame +0, which the destination takes (0x08A5BAD8 unless it is that
// slot); mode 1 takes the same slot, the text a3 goes into the slot at frame +8 (0x08A76ABC(text)) and 0x089EBB00(word, frame +8) runs. Both paths
// release frame +0 and frame +4 (0x08A5B9EC). Any other mode does nothing. v0 = 1 when neither mode matched.
void func_089EBB1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], mode = ctx.gpr[6], text = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = destination;
    ctx.gpr[16] = text;
    const u32 slot = frame.at(0x4u), other = frame.at(0x8u), built = frame.at(0x0u);
    if (mode == 0u) {
        ctx.gpr[18] = slot;
        call_set(rt, ctx, 0x089E5E50u, {slot});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A10u});
        call_set(rt, ctx, 0x089E4D5Cu, {built, held});
        if (destination != built) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, built)});
        call_set(rt, ctx, 0x08A5B9ECu, {built});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else if (mode == 1u) {
        ctx.gpr[18] = slot;
        ctx.gpr[17] = other;
        call_set(rt, ctx, 0x089E5E50u, {slot});
        call_set(rt, ctx, 0x08A76ABCu, {other, text});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A10u});
        call_set(rt, ctx, 0x089EBB00u, {held, other});
        call_set(rt, ctx, 0x08A5B9ECu, {other});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else {
        ctx.gpr[2] = 1u;
    }
}
// 0x089EBC24 (a0 = the destination string, a2 = a mode, a3 = a text): mode 0 builds the slot at frame +4 (0x089E5E50) whose word
// (0x08A5BB7C(0x08CA7A10) when null) 0x089E4D88 turns into the string at frame +0, which the destination takes (0x08A5BAD8 unless it is that
// slot); mode 1 takes the same slot, the text a3 goes into the slot at frame +8 (0x08A76ABC(text)) and 0x089EBC08(word, frame +8) runs. Both paths
// release frame +0 and frame +4 (0x08A5B9EC). Any other mode does nothing. v0 = 1 when neither mode matched.
void func_089EBC24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], mode = ctx.gpr[6], text = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = destination;
    ctx.gpr[16] = text;
    const u32 slot = frame.at(0x4u), other = frame.at(0x8u), built = frame.at(0x0u);
    if (mode == 0u) {
        ctx.gpr[18] = slot;
        call_set(rt, ctx, 0x089E5E50u, {slot});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A10u});
        call_set(rt, ctx, 0x089E4D88u, {built, held});
        if (destination != built) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, built)});
        call_set(rt, ctx, 0x08A5B9ECu, {built});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else if (mode == 1u) {
        ctx.gpr[18] = slot;
        ctx.gpr[17] = other;
        call_set(rt, ctx, 0x089E5E50u, {slot});
        call_set(rt, ctx, 0x08A76ABCu, {other, text});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A10u});
        call_set(rt, ctx, 0x089EBC08u, {held, other});
        call_set(rt, ctx, 0x08A5B9ECu, {other});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else {
        ctx.gpr[2] = 1u;
    }
}
// 0x089EBD2C (a0 = the destination string, a2 = a mode, a3 = a text): mode 0 builds the slot at frame +4 (0x089E5E50) whose word
// (0x08A5BB7C(0x08CA7A10) when null) 0x089E4DB4 turns into the string at frame +0, which the destination takes (0x08A5BAD8 unless it is that
// slot); mode 1 takes the same slot, the text a3 goes into the slot at frame +8 (0x08A76ABC(text)) and 0x089EBD10(word, frame +8) runs. Both paths
// release frame +0 and frame +4 (0x08A5B9EC). Any other mode does nothing. v0 = 1 when neither mode matched.
void func_089EBD2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], mode = ctx.gpr[6], text = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = destination;
    ctx.gpr[16] = text;
    const u32 slot = frame.at(0x4u), other = frame.at(0x8u), built = frame.at(0x0u);
    if (mode == 0u) {
        ctx.gpr[18] = slot;
        call_set(rt, ctx, 0x089E5E50u, {slot});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A10u});
        call_set(rt, ctx, 0x089E4DB4u, {built, held});
        if (destination != built) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, built)});
        call_set(rt, ctx, 0x08A5B9ECu, {built});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else if (mode == 1u) {
        ctx.gpr[18] = slot;
        ctx.gpr[17] = other;
        call_set(rt, ctx, 0x089E5E50u, {slot});
        call_set(rt, ctx, 0x08A76ABCu, {other, text});
        const u32 held = word(rt, slot);
        ctx.gpr[16] = held;
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A10u});
        call_set(rt, ctx, 0x089EBD10u, {held, other});
        call_set(rt, ctx, 0x08A5B9ECu, {other});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    } else {
        ctx.gpr[2] = 1u;
    }
}
// 0x08970B80 (a0 = the object, a1 = the argument): the same as func_08970A94 with literal 2 in the final 0x08965DF8 call.
void func_08970B80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x20u), G[16]); G[18] = 0x08D74FD0u; G[6] = G[5]; G[17] = G[4]; G[7] = G[18] + 0xCu; G[2] = word(rt, G[4] + 0xE4u); G[4] = G[5]; G[2] &= 0x20u; G[5] = frame.at(8u); if (G[2] == 0u) { G[3] = word(rt, G[17] + 0xE0u); G[2] = 0xFu; G[3] = (G[3] & ~0xF0u) | (G[2] << 4); put(rt, G[17] + 0xE0u, G[3]); } G[6] = word(rt, G[6]); G[3] = 0xFFFFFFFFu; G[2] = 0x08CCD978u; put(rt, frame.at(0x10u), G[3]); G[3] = frame.at(4u); put(rt, frame.at(0x14u), G[3]); put(rt, frame.at(0xCu), G[7]); put(rt, frame.at(8u), G[2]); G[2] = word(rt, G[6] + 8u); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(0xCu)); G[2] = 0x08CCD990u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(8u), G[2]); G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[6] = word(rt, frame.at(4u)); G[4] = G[17]; G[5] = 2u; call_here(rt, ctx, 0x08965DF8u); G[2] = word(rt, G[17] + 0xE4u) | 0x20u; put(rt, G[17] + 0xE4u, G[2]); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } }
// 0x08970C6C (a0 = the object, a1 = the argument): the same as func_08970A94 with literal 8 in the final 0x08965DF8 call.
void func_08970C6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x28u}, {17u, 0x24u}, {31u, 0x2Cu}, {16u, 0x20u}});
    const u32 base = 0x08D74FD0u;
    ctx.gpr[18] = base;
    ctx.gpr[17] = object;
    const u32 slot = base + 0xCu;
    if ((word(rt, object + 0xE4u) & 0x20u) == 0u) {
        put(rt, object + 0xE0u, (word(rt, object + 0xE0u) & ~(0xFu << 4)) | (0xFu << 4));
    }
    const u32 table = word(rt, argument);
    put(rt, frame.at(0x10u), 0xFFFFFFFFu);
    put(rt, frame.at(0x14u), frame.at(0x4u));
    put(rt, frame.at(0xCu), slot);
    put(rt, frame.at(0x8u), 0x08CCD978u);
    call_set(rt, ctx, word(rt, table + 0x8u), {argument, frame.at(0x8u), table, slot});
    const u32 released = word(rt, frame.at(0xCu));
    ctx.gpr[16] = released - 0xCu;
    put(rt, frame.at(0x8u), 0x08CCD990u);
    if (ctx.gpr[16] != base) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {released - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {ctx.gpr[16], frame.at(0u)});
    }
    call_set(rt, ctx, 0x08965DF8u, {object, 0x8u, word(rt, frame.at(0x4u))});
    ctx.gpr[2] = word(rt, object + 0xE4u) | 0x20u;
    put(rt, object + 0xE4u, ctx.gpr[2]);
}
// 0x08970D58 (a0 = the object, a1 = the argument): the same as func_08970A94 with literal 4 in the final 0x08965DF8 call.
void func_08970D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x28u}, {17u, 0x24u}, {31u, 0x2Cu}, {16u, 0x20u}});
    const u32 base = 0x08D74FD0u;
    ctx.gpr[18] = base;
    ctx.gpr[17] = object;
    const u32 slot = base + 0xCu;
    if ((word(rt, object + 0xE4u) & 0x20u) == 0u) {
        put(rt, object + 0xE0u, (word(rt, object + 0xE0u) & ~0xF0u) | (0xFu << 4));
    }
    const u32 table = word(rt, argument);
    put(rt, frame.at(0x10u), 0xFFFFFFFFu);
    put(rt, frame.at(0x14u), frame.at(0x4u));
    put(rt, frame.at(0xCu), slot);
    put(rt, frame.at(0x8u), 0x08CCD978u);
    call_set(rt, ctx, word(rt, table + 0x8u), {argument, frame.at(0x8u), table, slot});
    const u32 released = word(rt, frame.at(0xCu));
    ctx.gpr[16] = released - 0xCu;
    put(rt, frame.at(0x8u), 0x08CCD990u);
    if (ctx.gpr[16] != base) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {released - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {ctx.gpr[16], frame.at(0u)});
    }
    call_set(rt, ctx, 0x08965DF8u, {object, 0x4u, word(rt, frame.at(0x4u))});
    ctx.gpr[2] = word(rt, object + 0xE4u) | 0x20u;
    put(rt, object + 0xE4u, ctx.gpr[2]);
}

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
