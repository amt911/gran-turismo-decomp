// Hot batch 334: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
// Thirteen of them (0x089E6218, 0x08A9CEFC, 0x08A797A0, 0x088932A0, 0x08855990, 0x08A99E1C, 0x08B44948, 0x08B46AA0, 0x08B96974,
// 0x08BBECF0, 0x08B989A8, 0x08B99618, 0x08BB017C) are rewritten as structured C from the disassembly (structured rewrites,
// batches 84 and 229); the others keep their emitter bodies.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_334 {
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
// `trunc.w.s` as the recompiled baseline does it: NaN and +inf give 0x7FFFFFFF, -inf 0x80000000, finite values
// truncate toward zero and clamp to the int32 range.
std::uint32_t trunc_to_word(float value) {
    if (std::isnan(value)) return 0x7FFFFFFFu;
    const double t = std::trunc(static_cast<double>(value));
    if (t >= 2147483647.0) return 0x7FFFFFFFu;
    if (t <= -2147483648.0) return 0x80000000u;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(t));
}
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

// One reference given back on a refcounted handle: the counter at +4 is decremented (`ll`/`sc`, never retried here) and, when
// it reaches zero, the object is released through its vtable (the virtual at +8, then the second counter at +8 with the virtual
// at +0xC, the object as a0). A zero handle holds nothing.
void release_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 handle) {
    if (handle == 0u) return;
    u32 counter = word(rt, handle + 0x4u);
    put(rt, handle + 0x4u, counter - 1u);
    if (counter != 1u) return;
    call_set(rt, ctx, word(rt, word(rt, handle) + 0x8u), {handle});
    counter = word(rt, handle + 0x8u);
    put(rt, handle + 0x8u, counter - 1u);
    if (counter != 1u) return;
    call_set(rt, ctx, word(rt, word(rt, handle) + 0xCu), {handle});
}

// One 16-byte element assigned over another: its first word through the handle assignment 0x08B957D8, the other three copied.
void assign_element(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 destination, u32 source) {
    call_set(rt, ctx, 0x08B957D8u, {destination, source});
    put(rt, destination + 0x4u, word(rt, source + 0x4u));
    put(rt, destination + 0x8u, word(rt, source + 0x8u));
    put(rt, destination + 0xCu, word(rt, source + 0xCu));
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

// 0x089E6218 (a0 = an owner, a1 = a handle value, a2 = the address of a handle word): under the lock at 0x08CF575C (0x08B344E0 / 0x08B3457C)
// the owner's handle at +0x190 is made to mirror a copy of the global one (0x08969188 into a temporary, then 0x08A5BAD8 assigns when it is another
// handle). If the copy holds an object, it is bound to a1 (0x089BA4D8 when a1 is not 0) and 0x089BA6CC(object, 0) is called. If the word at a2
// is set, a second temporary (0x08969178 from it) takes the owner's +0xE8 (0x089BA6CC), its flag word +0xE0 loses bit 0x4000 and a1 is passed to
// 0x089BAA60. At the end the owner's handle takes the one at a2 (0x08A5BAD8, under the lock again) and a1 (when not 0) is passed to 0x089AB114
// with the owner's handle. The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_089E6218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4], value = ctx.gpr[5], handle_slot = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x28u}, {16u, 0x10u}, {21u, 0x24u}, {19u, 0x1Cu}, {18u, 0x18u}, {20u, 0x20u}, {17u, 0x14u}, {31u, 0x30u}, {23u, 0x2Cu}});
    const u32 lock = 0x08CF575Cu, global_handle = 0x08CF6ED8u;
    const u32 copy = frame.at(0x4u), scratch = frame.at(0x0u), second = frame.at(0x8u);
    const u32 own_handle = owner + 0x190u;
    ctx.gpr[22] = copy;
    ctx.gpr[16] = 0x08CF0000u;
    ctx.gpr[21] = 0x08CF0000u;
    ctx.gpr[19] = owner;
    ctx.gpr[18] = value;
    ctx.gpr[20] = handle_slot;
    ctx.gpr[17] = own_handle;
    // The object behind pointer slot `slot_at`; a null one is reported first (0x08A5BB7C).
    auto fetch = [&](u32 slot_at) {
        const u32 object = word(rt, slot_at);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A08u});
        return object;
    };
    call_set(rt, ctx, 0x08969188u, {copy, global_handle});
    call_set(rt, ctx, 0x08B344E0u, {lock});
    if (copy != own_handle) call_set(rt, ctx, 0x08A5BAD8u, {copy, word(rt, own_handle)});
    call_set(rt, ctx, 0x08969188u, {scratch, global_handle});
    if (own_handle != scratch) call_set(rt, ctx, 0x08A5BAD8u, {own_handle, word(rt, scratch)});
    call_set(rt, ctx, 0x08A5B9ECu, {scratch});
    call_set(rt, ctx, 0x08B3457Cu, {lock});
    u32 object = word(rt, copy);
    ctx.gpr[16] = object;
    u32 stop_word;
    if (object == 0u) {
        stop_word = word(rt, handle_slot);
        ctx.gpr[2] = stop_word;
    } else {
        if (value != 0u) {
            call_set(rt, ctx, 0x089BA4D8u, {object, value});
            object = word(rt, copy);
            ctx.gpr[16] = object;
        }
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7A08u});
        call_set(rt, ctx, 0x089BA6CCu, {object, 0u});
        stop_word = word(rt, handle_slot);
        ctx.gpr[2] = stop_word;
    }
    if (stop_word != 0u) {
        ctx.gpr[23] = second;
        call_set(rt, ctx, 0x08969178u, {second, handle_slot});
        object = fetch(second);
        call_set(rt, ctx, 0x089BA6CCu, {object, owner + 0xE8u});
        object = fetch(second);
        put(rt, object + 0xE0u, word(rt, object + 0xE0u) & ~0x4000u);
        if (value != 0u) {
            object = fetch(second);
            call_set(rt, ctx, 0x089BAA60u, {object, value});
        }
        call_set(rt, ctx, 0x08A5B9ECu, {second});
    }
    call_set(rt, ctx, 0x08B344E0u, {lock});
    if (own_handle != handle_slot) call_set(rt, ctx, 0x08A5BAD8u, {own_handle, word(rt, handle_slot)});
    if (value != 0u) call_set(rt, ctx, 0x089AB114u, {value, word(rt, own_handle)});
    call_set(rt, ctx, 0x08B3457Cu, {lock});
    call_set(rt, ctx, 0x08A5B9ECu, {copy});
}
// 0x08A9CEFC (a0 = an object): after 0x08A5B8B8(object) and 0x08A7BE84, an object whose byte +0x7C is set is torn down: the byte is cleared,
// every node of the list at +0x24 gets a virtual +8 call with 1, and the handle / string members +0x78, +0x74 (0x08A983D8) and +0x40, +0x3C,
// +0x38, +0x34 (0x08A7C0E0) are reset from the global handle 0x08CF6ED8 through a temporary each (assigned with 0x08A5BAD8 when it is another
// handle, the temporary destroyed with 0x08A5B9EC). Last the intrusive list at +0x2C is re-initialised (0x08C2F9F0) and the tree at +0xC is
// emptied (0x08B5F100 walks, 0x08C301B0 erases) and reset. v0 = object + 0x24 when the byte is clear, else the last call's result.
// The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_08A9CEFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x2Cu}, {31u, 0x30u}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    const u32 global_handle = 0x08CF6ED8u;
    ctx.gpr[19] = object;
    call_set(rt, ctx, 0x08A5B8B8u, {object});
    call_set(rt, ctx, 0x08A7BE84u, {});
    ctx.gpr[2] = object + 0x24u;
    if (load8(object + 0x7Cu) == 0u) return;
    const u32 list = object + 0x24u;
    const u32 end = word(rt, list);
    ctx.gpr[17] = end;
    store8(object + 0x7Cu, 0u);
    if (list != end) {
        u32 node = list;
        ctx.gpr[16] = node;
        do {
            call_set(rt, ctx, word(rt, word(rt, node + 0x4u) + 0x8u), {1u});
            node = word(rt, node + 0x4u);
            ctx.gpr[16] = node;
        } while (end != node);
    }
    ctx.gpr[18] = 0x08CF0000u;
    // Resets the member at `field` from the global handle through the temporary `temp`; `next_s1` is the temporary the asm points s1 at next.
    auto reset = [&](u32 routine, u32 field, u32 temp, u32 next_s1) {
        ctx.gpr[17] = temp;
        ctx.gpr[16] = field;
        call_set(rt, ctx, routine, {temp, global_handle});
        if (field != temp) call_set(rt, ctx, 0x08A5BAD8u, {field, word(rt, temp)});
        if (next_s1 != 0u) ctx.gpr[17] = next_s1;
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    };
    reset(0x08A983D8u, object + 0x78u, frame.at(0x14u), frame.at(0x10u));
    reset(0x08A983D8u, object + 0x74u, frame.at(0x10u), frame.at(0xCu));
    reset(0x08A7C0E0u, object + 0x40u, frame.at(0xCu), frame.at(0x8u));
    reset(0x08A7C0E0u, object + 0x3Cu, frame.at(0x8u), frame.at(0x4u));
    reset(0x08A7C0E0u, object + 0x38u, frame.at(0x4u), 0u);
    const u32 member = object + 0x34u, intrusive = object + 0x2Cu;
    ctx.gpr[16] = member;
    call_set(rt, ctx, 0x08A7C0E0u, {frame.at(0x0u), global_handle});
    if (member != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {member, word(rt, frame.at(0x0u))});
    ctx.gpr[16] = intrusive;
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
    call_set(rt, ctx, 0x08C2F9F0u, {intrusive});
    const u32 tree = object + 0xCu, sentinel = object + 0x10u;
    ctx.gpr[17] = tree;
    put(rt, intrusive + 0x4u, intrusive);
    ctx.gpr[18] = sentinel;
    put(rt, object + 0x2Cu, intrusive);
    u32 node = word(rt, tree + 0xCu);
    while (node != sentinel) node = call_set(rt, ctx, 0x08B5F100u, {node});
    call_set(rt, ctx, 0x08C301B0u, {tree, word(rt, tree + 0x8u)});
    put(rt, tree + 0x10u, sentinel);
    put(rt, tree + 0x14u, 0u);
    put(rt, tree + 0xCu, sentinel);
    put(rt, tree + 0x8u, 0u);
}
void func_08B02D8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x90u), G[20]); put(rt, frame.at(0x8Cu), G[19]); put(rt, frame.at(0x88u), G[18]); put(rt, frame.at(0x84u), G[17]); put(rt, frame.at(0x80u), G[16]); put(rt, frame.at(0x94u), G[31]); G[20] = frame.at(0x70u); G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; G[5] = load16(G[5] + 0x2u); G[2] = load16(G[6] + 0x2u); G[3] = G[5] << 18; G[2] = G[2] << 18; G[2] = sra(G[2], 18); G[3] = sra(G[3], 18); G[3] = G[3] + G[2]; G[5] = (G[5] & ~0x3FFFu) | ((G[3] << 0) & 0x3FFFu); store16(G[17] + 0x2u, G[5]); G[3] = word(rt, G[4] + 0x7Cu); G[4] = sext(load16(G[6] + 0x0u), 16); store8(frame.at(0x50u), G[2]); G[3] = G[17] - G[3]; G[2] = 0xAAAA0000u; G[3] = sra(G[3], 1); G[2] = G[2] | 0xAAABu; { const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32u); } G[5] = word(rt, G[16] + 0x0u); G[2] = G[4] << 8; G[3] = word(rt, frame.at(0x50u)); G[4] = G[4] << 4; G[2] = G[2] - G[4]; G[5] = G[5] + G[2]; G[19] = ctx.lo; put(rt, frame.at(0x70u), G[3]); put(rt, frame.at(0x78u), G[5]); put(rt, frame.at(0x5Cu), 0u); put(rt, frame.at(0x58u), G[5]); put(rt, frame.at(0x54u), G[16]); put(rt, frame.at(0x60u), G[3]); put(rt, frame.at(0x64u), G[16]); put(rt, frame.at(0x68u), G[5]); put(rt, frame.at(0x6Cu), 0u); put(rt, frame.at(0x74u), G[16]); put(rt, frame.at(0x7Cu), 0u); goto L_08B02E84; L_08B02E48: ; store16(G[2] + 0xEAu, G[19]); G[2] = word(rt, frame.at(0x78u)); G[4] = G[2]; put(rt, frame.at(0x7Cu), G[2]); call_here(rt, ctx, 0x08C46D74u); G[2] = sext(G[2] & 0xFFFFu, 16); G[3] = G[2] << 4; G[4] = G[2] << 8; G[4] = G[4] - G[3]; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[3] = 0u; if (tk) goto L_08B02E80; } G[2] = word(rt, frame.at(0x74u)); G[3] = word(rt, G[2] + 0x0u); G[3] = G[3] + G[4]; L_08B02E80: ; put(rt, frame.at(0x78u), G[3]); L_08B02E84: ; G[2] = word(rt, frame.at(0x78u)); G[5] = G[20]; { const bool tk = G[2] != 0u; G[6] = G[16]; if (tk) goto L_08B02E48; } G[2] = sext(load16(G[17] + 0x0u), 16); G[3] = sext(load16(G[18] + 0x0u), 16); store8(frame.at(0x30u), G[16]); G[4] = G[2] << 8; G[2] = G[2] << 4; store8(frame.at(0x20u), G[16]); G[4] = G[4] - G[2]; G[8] = G[3] << 8; G[7] = word(rt, G[16] + 0x0u); G[9] = word(rt, frame.at(0x30u)); G[2] = word(rt, frame.at(0x20u)); G[3] = G[3] << 4; G[8] = G[8] - G[3]; G[8] = G[7] + G[8]; G[6] = G[20]; G[7] = G[7] + G[4]; G[5] = frame.at(0u); G[4] = frame.at(0x10u); put(rt, frame.at(0x0u), G[2]); put(rt, frame.at(0x4u), G[16]); put(rt, frame.at(0x34u), G[16]); put(rt, frame.at(0x44u), G[16]); put(rt, frame.at(0x14u), G[16]); put(rt, frame.at(0x24u), G[16]); put(rt, frame.at(0x10u), G[9]); put(rt, frame.at(0x18u), G[7]); put(rt, frame.at(0x8u), G[8]); put(rt, frame.at(0x3Cu), 0u); put(rt, frame.at(0x38u), G[7]); put(rt, frame.at(0x40u), G[9]); put(rt, frame.at(0x48u), G[7]); put(rt, frame.at(0x4Cu), 0u); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x2Cu), 0u); put(rt, frame.at(0x28u), G[8]); put(rt, frame.at(0xCu), 0u); call_here(rt, ctx, 0x08C48EC4u); G[2] = G[17]; G[4] = load16(G[18] + 0x0u); store16(G[17] + 0x0u, G[4]); G[4] = 0xFFFF8000u; G[3] = load16(G[18] + 0x2u); G[3] = G[3] | G[4]; store16(G[18] + 0x2u, G[3]); goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x90u)); G[19] = word(rt, frame.at(0x8Cu)); G[18] = word(rt, frame.at(0x88u)); G[17] = word(rt, frame.at(0x84u)); G[16] = word(rt, frame.at(0x80u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B15090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x94u), G[21]); put(rt, frame.at(0x8Cu), G[19]); put(rt, frame.at(0x88u), G[18]); put(rt, frame.at(0x98u), G[31]); put(rt, frame.at(0x90u), G[20]); put(rt, frame.at(0x84u), G[17]); put(rt, frame.at(0x80u), G[16]); G[19] = G[4]; G[18] = G[5]; G[17] = word(rt, G[5] + 0x8u); G[20] = word(rt, G[17] + 0x50u); G[16] = word(rt, G[17] + 0x7Cu); G[4] = G[20]; call_here(rt, ctx, 0x08B17A98u); { const bool tk = G[16] != 0u; G[21] = G[2]; if (tk) goto L_08B151A0; } G[2] = word(rt, G[19] + 0x0u); G[16] = frame.at(0x40u); G[6] = G[17]; G[3] = word(rt, G[2] + 0xACu); G[4] = G[16]; { const std::uint32_t tg = G[3]; G[5] = G[19]; call_here(rt, ctx, tg); } G[8] = frame.at(0u); G[6] = frame.at(0x70u); L_08B150F8: ; G[2] = word(rt, G[16] + 0x0u); G[3] = word(rt, G[16] + 0x4u); G[4] = word(rt, G[16] + 0x8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[8] + 0x0u, G[2]); G[16] = G[16] + 0x10u; G[8] = G[8] + 0x10u; put(rt, G[8] + 0xFFFFFFF4u, G[3]); put(rt, G[8] + 0xFFFFFFF8u, G[4]); { const bool tk = G[16] != G[6]; put(rt, G[8] + 0xFFFFFFFCu, G[5]); if (tk) goto L_08B150F8; } G[2] = word(rt, G[16] + 0x8u); G[3] = word(rt, G[16] + 0x0u); G[4] = word(rt, G[16] + 0x4u); G[7] = frame.at(0u); put(rt, G[8] + 0x8u, G[2]); G[6] = G[17] + 0xA8u; G[9] = frame.at(0x30u); put(rt, G[8] + 0x0u, G[3]); put(rt, G[8] + 0x4u, G[4]); L_08B15148: ; G[2] = word(rt, G[7] + 0x0u); G[3] = word(rt, G[7] + 0x4u); G[4] = word(rt, G[7] + 0x8u); G[5] = word(rt, G[7] + 0xCu); put(rt, G[6] + 0x0u, G[2]); G[7] = G[7] + 0x10u; G[6] = G[6] + 0x10u; put(rt, G[6] + 0xFFFFFFF4u, G[3]); put(rt, G[6] + 0xFFFFFFF8u, G[4]); { const bool tk = G[7] != G[9]; put(rt, G[6] + 0xFFFFFFFCu, G[5]); if (tk) goto L_08B15148; } G[2] = word(rt, G[7] + 0x8u); G[3] = word(rt, G[7] + 0x0u); G[4] = word(rt, G[7] + 0x4u); put(rt, G[6] + 0x8u, G[2]); put(rt, G[6] + 0x0u, G[3]); put(rt, G[6] + 0x4u, G[4]); G[2] = word(rt, G[17] + 0xA8u); if (G[2] != 0u) { G[16] = 0xFFFFFFFFu; goto L_08B151F8; } G[7] = 0x1u; goto L_08B15228; L_08B151A0: ; G[2] = 0x1u; { const bool tk = G[16] != G[2]; G[16] = 0xFFFFFFFFu; if (tk) goto L_08B151F8; } G[3] = word(rt, G[17] + 0x94u); G[2] = 0x2u; G[7] = 0x3u; G[4] = G[3] & 0x2u; if (G[4] == 0u) G[7] = G[2]; G[5] = G[3] & 0x1u; G[2] = G[7] | 0x200u; if (G[5] != 0u) G[7] = G[2]; G[4] = G[3] & 0x4u; G[2] = G[7] | 0x400u; if (G[4] != 0u) G[7] = G[2]; G[5] = G[3] & 0x10u; G[2] = G[7] | 0x100u; G[3] = G[3] & 0x8u; if (G[3] != 0u) G[7] = G[2]; { const bool tk = G[5] == 0u; G[6] = 0x1A4u; if (tk) goto L_08B1522C; } G[7] = G[7] | 0x800u; goto L_08B1522C; L_08B151F8: ; G[4] = G[20]; call_here(rt, ctx, 0x08B17AA8u); { const bool tk = static_cast<std::int32_t>(G[16]) >= 0; G[4] = G[19]; if (tk) goto L_08B15218; } G[4] = G[16]; call_here(rt, ctx, 0x08B1830Cu); put(rt, G[18] + 0x4u, G[2]); goto L_08B15240; L_08B15218: ; G[5] = G[18]; call_here(rt, ctx, 0x08B1065Cu); put(rt, G[18] + 0x10u, G[16]); goto L_08B15240; L_08B15228: ; G[6] = 0u; L_08B1522C: ; G[4] = G[21]; G[5] = G[7]; call_here(rt, ctx, 0x08B33C0Cu); G[16] = G[2]; goto L_08B151F8; L_08B15240: ; G[2] = G[16]; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x94u)); G[19] = word(rt, frame.at(0x8Cu)); G[18] = word(rt, frame.at(0x88u)); G[20] = word(rt, frame.at(0x90u)); G[17] = word(rt, frame.at(0x84u)); G[16] = word(rt, frame.at(0x80u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B46AA0: registers one more entry in the object a0: the handle *(a1) is opened (0x08C7AE38 over the text built at sp + 2;
// a negative answer returns it straight away). With a callback at +0x18 a temporary at sp + 0x18 (a copy of the object at +0x70
// and of the text at sp + 2, plus the halfword at sp) is offered to it: an answer of 0 sends the temporary to the virtual at
// vtable +0xAC and ends the call. Otherwise vtable +0xA0 allocates 0x2400 bytes for the entry, which takes the handle at +0,
// copies of +0x70 and of the text at +4 / +0x1A and the flags at +0x30 / +0x32, and is registered by 0x08B46904(self, entry);
// the eight slots at self + 0x489C (0x16 bytes apart) are each handed to 0x08B3F964 with the entry's text and destroyed
// (0x08B3F838) when that answers non-zero. v0 = what the last call answered.
void func_08B46AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x80u); put(rt, frame.at(0x78u), G[22]); put(rt, frame.at(0x70u), G[20]); put(rt, frame.at(0x64u), G[17]); put(rt, frame.at(0x7Cu), G[31]); put(rt, frame.at(0x68u), G[18]); put(rt, frame.at(0x60u), G[16]); put(rt, frame.at(0x74u), G[21]); put(rt, frame.at(0x6Cu), G[19]); G[22] = frame.at(0x2u); G[20] = G[4]; G[4] = G[22]; G[17] = G[5]; call_here(rt, ctx, 0x08B3FA98u); G[4] = G[22]; G[16] = word(rt, G[17] + 0x0u); call_here(rt, ctx, 0x08B3F8A0u); G[5] = G[2]; G[4] = G[16]; G[6] = frame.at(0u); G[7] = 0u; G[8] = 0x1u; call_here(rt, ctx, 0x08C7AE38u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[18] = G[2]; if (tk) goto L_08B46C50; } G[2] = word(rt, G[20] + 0x18u); { const bool tk = G[2] == 0u; G[16] = frame.at(0x1Cu); if (tk) goto L_08B46BB8; } G[17] = frame.at(0x32u); G[4] = G[16]; call_here(rt, ctx, 0x08B3FA98u); G[4] = G[17]; call_here(rt, ctx, 0x08B3FA98u); G[4] = G[16]; put(rt, frame.at(0x18u), 0u); G[21] = G[20] + 0x70u; call_here(rt, ctx, 0x08B3F838u); G[4] = G[17]; call_here(rt, ctx, 0x08B3F838u); G[4] = G[16]; G[5] = G[21]; store8(frame.at(0x48u), 0u); G[16] = frame.at(0x18u); store8(frame.at(0x49u), 0u); store8(frame.at(0x4Au), 0u); put(rt, frame.at(0x4Cu), 0u); put(rt, frame.at(0x50u), 0u); put(rt, frame.at(0x54u), 0u); put(rt, frame.at(0x58u), 0u); put(rt, frame.at(0x18u), G[18]); call_here(rt, ctx, 0x08B3F8C0u); G[4] = G[17]; G[5] = G[22]; call_here(rt, ctx, 0x08B3F7CCu); G[5] = load16(frame.at(0x0u)); G[4] = G[17]; call_here(rt, ctx, 0x08B3FA3Cu); G[2] = word(rt, G[20] + 0x18u); G[5] = word(rt, G[20] + 0x1Cu); G[4] = G[16]; { const std::uint32_t tg = G[2]; store8(frame.at(0x4Au), 0u); call_here(rt, ctx, tg); } { const bool tk = G[2] != 0u; G[3] = word(rt, G[20] + 0x0u); if (tk) goto L_08B46BC0; } G[4] = G[20]; G[2] = word(rt, G[3] + 0xACu); { const std::uint32_t tg = G[2]; G[5] = G[16]; call_here(rt, ctx, tg); } goto L_08B46C54; L_08B46BB8: ; G[21] = G[20] + 0x70u; G[3] = word(rt, G[20] + 0x0u); L_08B46BC0: ; G[4] = G[20]; G[5] = 0x2400u; G[2] = word(rt, G[3] + 0xA0u); G[17] = G[20] + 0x489Cu; { const std::uint32_t tg = G[2]; G[19] = 0u; call_here(rt, ctx, tg); } put(rt, G[2] + 0x0u, G[18]); G[4] = G[2] + 0x4u; G[18] = G[2] + 0x1Au; G[5] = G[21]; G[16] = G[2]; call_here(rt, ctx, 0x08B3F8C0u); G[5] = G[22]; G[4] = G[18]; call_here(rt, ctx, 0x08B3F7CCu); G[5] = load16(frame.at(0x0u)); G[4] = G[18]; G[21] = 0x8u; call_here(rt, ctx, 0x08B3FA3Cu); G[2] = 0x1u; store8(G[16] + 0x30u, G[2]); G[5] = G[16]; G[4] = G[20]; store8(G[16] + 0x32u, 0u); call_here(rt, ctx, 0x08B46904u); G[4] = G[17]; L_08B46C28: ; G[5] = G[18]; call_here(rt, ctx, 0x08B3F964u); G[19] = G[19] + 0x1u; G[4] = G[17]; { const bool tk = G[2] == 0u; G[17] = G[17] + 0x16u; if (tk) goto L_08B46C48; } call_here(rt, ctx, 0x08B3F838u); L_08B46C48: ; { const bool tk = G[19] != G[21]; G[4] = G[17]; if (tk) goto L_08B46C28; } L_08B46C50: ; L_08B46C54: ; goto exit_;  exit_: ; G[22] = word(rt, frame.at(0x78u)); G[20] = word(rt, frame.at(0x70u)); G[17] = word(rt, frame.at(0x64u)); G[18] = word(rt, frame.at(0x68u)); G[16] = word(rt, frame.at(0x60u)); G[21] = word(rt, frame.at(0x74u)); G[19] = word(rt, frame.at(0x6Cu)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x088932A0 (a0 = a timer object, a2:a3 = a 64-bit tick count): converts the count to a float (0x08B6EE4C compares it with 0, 0x08B6EF68
// converts; a count with the top bit set is halved with its low bit kept as a sticky bit and the result doubled) and scales it by
// 0.001 (0x3A83126F) into f20, the new time in seconds. The object's state at +0x8448..+0x8460: when the previous time (+0x8454) is 0 the start (+0x8458)
// takes the new one, else the elapsed time (+0x8450) is new - start; the new time is stored at +0x8454. With a callback (+0x30, argument +0x34)
// and an active byte (+0x8448), the whole-second counts of the time left (+0x844C minus the tick progress) and of the remaining time to the end
// (+0x845C - time) are compared and the callback gets 8 (or 7) with the count in the frame when the count changed and is in 1..3 (resp. 1..3);
// when the remaining time reaches 0 the time left is set to 0 and the active byte cleared. v0 = what the asm leaves there (0x10000 without a
// callback, 0 when not active, object + 0x10000 otherwise).
void func_088932A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], lo = ctx.gpr[6], hi = ctx.gpr[7];
    GuestFrame frame(ctx, 0x150u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x134u}, {16u, 0x130u}, {18u, 0x138u}, {31u, 0x13Cu}, {52u, 0x140u}});
    ctx.gpr[17] = hi;
    ctx.gpr[16] = lo;
    ctx.gpr[18] = object;
    float ticks;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B6EE4Cu, {lo, hi, 0u, 0u})) > 0) {
        call_set(rt, ctx, 0x08B6EF68u, {lo, hi});
        ticks = ctx.fpr[0];
    } else {
        call_set(rt, ctx, 0x08B6EF68u, {(lo & 0x1u) | ((hi << 31) | (lo >> 1)), hi >> 1});
        ticks = ctx.fpr[0] + ctx.fpr[0];
    }
    const u32 at_time = object + 0x8454u, at_start = object + 0x8458u, at_elapsed = object + 0x8450u, at_active = object + 0x8448u;
    const u32 at_end = object + 0x845Cu, at_left = object + 0x844Cu, at_count = object + 0x8460u;
    float now = psp_mul(ticks, std::bit_cast<float>(0x3A83126Fu));
    ctx.fpr[20] = now;
    if (float_at(rt, at_time) == 0.0f) put_float(rt, at_start, now);
    else put_float(rt, at_elapsed, now - float_at(rt, at_start));
    const u32 callback = word(rt, object + 0x30u);
    put_float(rt, at_time, now);
    ctx.gpr[2] = 0x10000u;
    if (callback == 0u) return;
    ctx.gpr[2] = load8(at_active);
    if (ctx.gpr[2] == 0u) return;
    const u32 count = word(rt, at_count);
    if (count != 0u) {
        const float step = psp_mul(static_cast<float>(static_cast<s32>(count)), std::bit_cast<float>(0x3A83126Fu));
        const u32 left = trunc_to_word(float_at(rt, at_left) - step);
        const u32 remaining = trunc_to_word((float_at(rt, at_end) - now) - step);
        if (left != remaining && static_cast<s32>(left) < 4 && static_cast<s32>(left) > 0) {
            put(rt, frame.at(0x0u), 8u);
            put(rt, frame.at(0x8u), left);
            call_set(rt, ctx, callback, {word(rt, object + 0x34u), frame.at(0x0u)});
        }
    }
    ctx.gpr[2] = object + 0x10000u;
    now = float_at(rt, at_end) - now;
    ctx.fpr[20] = now;
    const u32 whole_left = trunc_to_word(float_at(rt, at_left));
    const u32 whole_now = trunc_to_word(now);
    float zero = 0.0f;
    if (whole_left != whole_now && static_cast<s32>(whole_left) < 4 && static_cast<s32>(whole_left) > 0) {
        put(rt, frame.at(0x98u), 7u);
        put(rt, frame.at(0xA0u), whole_left);
        call_set(rt, ctx, word(rt, object + 0x30u), {word(rt, object + 0x34u), frame.at(0x98u)});
    }
    ctx.gpr[2] = object + 0x10000u;
    put_float(rt, at_left, now);
    if (now <= zero) {
        put_float(rt, at_left, zero);
        store8(at_active, 0u);
    }
}
void func_088B3DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[3] = 0x1u; G[17] = G[5]; G[16] = G[4]; G[4] = sext(load8(G[4] + 0x12u), 8); store8(G[16] + 0x13u, G[3]); G[3] = 0x3u; G[2] = G[4] + 0x1u; G[2] = sext(G[2] & 0xFFu, 8); { const bool tk = G[2] != G[3]; store8(G[16] + 0x12u, G[2]); if (tk) goto L_088B3E28; } store8(G[16] + 0x12u, 0u); L_088B3E28: ; { const bool tk = G[4] != 0u; if (tk) goto L_088B3FB0; } G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[2] = 0xFFFFFFFFu; G[6] = frame.at(0u); G[5] = G[4] + G[5]; G[18] = G[16] + 0x8u; put(rt, frame.at(0x0u), G[2]); call_here(rt, ctx, 0x08BB16ECu); G[5] = 0u; G[6] = word(rt, G[18] + 0x4u); G[4] = word(rt, G[16] + 0x8u); G[6] = G[6] + 0x1Fu; G[6] = G[6] >> 5; G[6] = G[6] << 2; call_here(rt, ctx, 0x08B36018u); store16(G[16] + 0x10u, 0u); G[4] = G[17]; G[2] = word(rt, G[17] + 0x0u); G[3] = word(rt, G[2] + 0x8u); { const std::uint32_t tg = G[3]; G[5] = G[16]; call_here(rt, ctx, tg); } G[8] = word(rt, G[16] + 0x4u); { const bool tk = G[8] == 0u; if (tk) goto L_088B3FB0; } G[7] = word(rt, G[16] + 0x0u); G[4] = 0u; G[5] = 0xFFFFFFFFu; G[12] = 0xFFFFFFFFu; G[10] = 0u; G[11] = 0u; G[9] = 0x1u; G[13] = 0x1u; L_088B3EB0: ; { const bool tk = static_cast<std::int32_t>(G[4]) >= 0; G[6] = word(rt, G[18] + 0x0u); if (tk) goto L_088B3EE0; } if (G[9] == G[13]) { G[2] = G[4] >> 5; goto L_088B3EE4; } G[3] = 0u - G[9]; G[3] = G[3] >> 5; G[2] = G[3] << 2; G[2] = G[6] - G[2]; G[3] = G[3] << 5; G[2] = G[2] + 0xFFFFFFFCu; G[3] = G[3] + 0x20u; goto L_088B3EF0; L_088B3EE0: ; G[2] = G[4] >> 5; L_088B3EE4: ; G[2] = G[2] << 2; G[2] = G[6] + G[2]; G[3] = G[4] & 0x1Fu; L_088B3EF0: ; G[2] = word(rt, G[2] + 0x0u); G[6] = 0x1u; G[3] = G[6] << (G[3] & 31u); G[2] = G[2] & G[3]; { const bool tk = G[2] == 0u; G[2] = G[11] < 0x2u ? 1u : 0u; if (tk) goto L_088B3F30; } G[10] = G[10] + 0x1u; G[2] = G[10] < 0x2u ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = G[7] + G[4]; if (tk) goto L_088B3F20; } G[5] = G[4]; goto L_088B3F50; L_088B3F20: ; store8(G[2] + 0x0u, G[6]); G[7] = word(rt, G[16] + 0x0u); G[8] = word(rt, G[16] + 0x4u); goto L_088B3F50; L_088B3F30: ; { const bool tk = G[2] == 0u; G[2] = G[4] + G[7]; if (tk) goto L_088B3F54; } G[3] = sext(load8(G[2] + 0x0u), 8); { const bool tk = G[3] != 0u; if (tk) goto L_088B3F58; } G[11] = G[11] + 0x1u; G[2] = G[11] ^ 0x1u; if (G[2] == 0u) G[12] = G[4]; L_088B3F50: ; G[2] = G[4] + G[7]; L_088B3F54: ; G[3] = sext(load8(G[2] + 0x0u), 8); L_088B3F58: ; if (static_cast<std::int32_t>(G[3]) >= 0) { store16(G[16] + 0x10u, G[9]); goto L_088B3F60; } L_088B3F60: ; G[4] = G[4] + 0x1u; G[2] = G[4] < G[8] ? 1u : 0u; { const bool tk = G[2] != 0u; G[9] = G[9] + 0x1u; if (tk) goto L_088B3EB0; } G[2] = 0x1u; { const bool tk = G[11] != G[2]; G[2] = 0xFFFFFFFFu; if (tk) goto L_088B3F94; } G[3] = G[7] + G[12]; store8(G[3] + 0x0u, G[2]); G[4] = G[12] + 0x1u; G[2] = load16(G[16] + 0x10u); if (G[4] == G[2]) { store16(G[16] + 0x10u, G[12]); goto L_088B3F94; } L_088B3F94: ; G[2] = G[10] < 0x2u ? 1u : 0u; { const bool tk = G[2] != 0u; G[3] = 0x1u; if (tk) goto L_088B3FAC; } G[2] = word(rt, G[16] + 0x0u); G[2] = G[2] + G[5]; store8(G[2] + 0x0u, G[3]); L_088B3FAC: ; L_088B3FB0: ; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B44948 (a0 = an object, a2:a3 = a 64-bit time stamp): when the object's virtual +0xA4 answers nonzero, and +0xA0 does too, the list from
// 0x08B3F3C8(object + 0x8DC) is walked (each link: +0 the entry, +8 the next link). Entries with a nonzero byte +4 are skipped, and so are
// those whose stamp (+0x30, 64-bit) is within +-0x32C7 of the time. For the first remaining entry: when its previous time (+0x40) is 0 it is
// registered (0x08B40A40) and its time set; else, when the absolute distance to the previous time is at least 0xBB9, the entry is dispatched
// (the +0xB0 object's virtual +0x50 with entry + 0x12, then a 0x58-byte record built in the frame with 0x08B3F478 / 0x08B36018 / 0x08B3E7FC and
// sent through 0x08B41F70 and 0x08B42A90). If +0xA0 answers 0, 0x08B4485C and 0x08B42710 are called instead. v0 = what the asm leaves
// there (the last test's value on the early exits).
void func_08B44948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], lo = ctx.gpr[6], hi = ctx.gpr[7];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x68u}, {17u, 0x64u}, {16u, 0x60u}, {31u, 0x74u}, {20u, 0x70u}, {19u, 0x6Cu}});
    ctx.gpr[18] = object;
    ctx.gpr[17] = hi;
    ctx.gpr[16] = lo;
    auto virtual_at = [&](u32 offset) { return word(rt, word(rt, object) + offset); };
    if (call_set(rt, ctx, virtual_at(0xA4u), {object}) == 0u) return;
    if (call_set(rt, ctx, virtual_at(0xA0u), {object}) == 0u) {
        call_set(rt, ctx, 0x08B4485Cu, {object, 0u});
        call_set(rt, ctx, 0x08B42710u, {object, 0u});
        return;
    }
    u32 link = call_set(rt, ctx, 0x08B3F3C8u, {object + 0x8DCu});
    if (link == 0u) return;
    ctx.gpr[20] = frame.at(0x8u);
    u32 entry = word(rt, link);
    ctx.gpr[19] = entry;
    for (;;) {
        const u32 skipped = load8(entry + 0x4u);
        ctx.gpr[2] = skipped;
        bool inside_window = false;
        if (skipped == 0u) {
            // |time - entry stamp| < 0x32C7 as a 64-bit window test: (diff + 0x32C7) < 0x658F.
            const u32 diff = lo - word(rt, entry + 0x30u);
            const u32 borrow = lo < diff ? 1u : 0u;
            const u32 diff_high = hi - word(rt, entry + 0x34u);
            const u32 sum = diff + 0x32C7u;
            const u32 high = (sum < diff ? 1u : 0u) + (diff_high - borrow);
            inside_window = high == 0u && sum < 0x658Fu;
            if (inside_window) ctx.gpr[2] = 1u;
        }
        if (skipped == 0u && !inside_window) {
            const u32 last_lo = word(rt, entry + 0x40u), last_hi = word(rt, entry + 0x44u);
            if ((last_lo | last_hi) == 0u) {
                call_set(rt, ctx, 0x08B40A40u, {object, entry, 0u});
                put(rt, entry + 0x40u, lo);
                put(rt, entry + 0x44u, hi);
                return;
            }
            const u32 delta = lo - last_lo;
            const u32 delta_borrow = lo < delta ? 1u : 0u;
            const u32 delta_high = hi - last_hi - delta_borrow;
            const u32 sign = sra(delta_high, 31);
            const u32 folded_low = sign ^ delta, folded_high = sign ^ delta_high;
            const u32 abs_low = folded_low - sign;
            const u32 abs_borrow = folded_low < abs_low ? 1u : 0u;
            const u32 abs_high = folded_high - sign - abs_borrow;
            ctx.gpr[2] = folded_low;
            if (!(static_cast<s32>(abs_high) > 0)) {
                if (abs_high != 0u) return;
                ctx.gpr[2] = abs_low < 0xBB9u ? 1u : 0u;
                if (abs_low < 0xBB9u) return;
            }
            const u32 holder = word(rt, object + 0xB0u);
            call_set(rt, ctx, word(rt, word(rt, holder) + 0x50u), {holder, entry + 0x12u});
            call_set(rt, ctx, 0x08B3F478u, {frame.at(0x8u)});
            call_set(rt, ctx, 0x08B36018u, {frame.at(0x0u), 0u, 0x58u});
            call_set(rt, ctx, 0x08B3E7FCu, {frame.at(0x8u), entry});
            call_set(rt, ctx, 0x08B41F70u, {object, load16(entry + 0x0u)});
            call_set(rt, ctx, 0x08B42A90u, {object, 0x4E25u, frame.at(0x0u), 0x58u, 0u, 1u, 0u, 0u});
            return;
        }
        link = word(rt, link + 0x8u);
        if (link == 0u) return;
        entry = word(rt, link);
        ctx.gpr[19] = entry;
    }
}
void func_0881DD34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[30] = frame.at(0x4u); G[23] = G[6] & 0xFFu; G[22] = G[4]; G[21] = 0u; put(rt, frame.at(0x10u), G[5]); call_here(rt, ctx, 0x08A98A70u); G[2] = word(rt, frame.at(0x10u)); G[18] = G[2] + 0x354u; L_0881DD80: ; G[4] = G[18]; call_here(rt, ctx, 0x088A5EE0u); G[4] = 0xFFFFFFFFu; { const bool tk = G[4] != G[2]; if (tk) goto L_0881DD9C; } { const bool tk = G[4] == G[3]; G[2] = G[22]; if (tk) goto L_0881DEE4; } L_0881DD9C: ; if (G[23] == 0u) { G[16] = word(rt, G[22] + 0x0u); goto L_0881DE68; } G[4] = G[18]; call_here(rt, ctx, 0x088A5EE0u); G[4] = G[18]; G[17] = G[3]; G[16] = G[2]; call_here(rt, ctx, 0x088A6164u); G[4] = word(rt, frame.at(0x10u)); G[6] = G[16]; G[7] = G[17]; G[8] = G[2]; call_here(rt, ctx, 0x0881A8C0u); G[19] = word(rt, G[22] + 0x0u); { const bool tk = G[19] != 0u; G[20] = G[2]; if (tk) goto L_0881DDE8; } G[2] = 0x08CA0000u; G[4] = 0x08C99264u; call_here(rt, ctx, 0x08A5BB7Cu); L_0881DDE8: ; G[4] = G[18]; call_here(rt, ctx, 0x088A5EE0u); G[4] = G[18]; G[16] = G[2]; G[17] = G[3]; call_here(rt, ctx, 0x088A6164u); G[8] = G[2]; G[6] = G[16]; G[4] = G[30]; G[16] = G[19] + 0x14u; G[7] = G[17]; G[9] = G[20]; call_here(rt, ctx, 0x0884CD28u); G[5] = word(rt, G[16] + 0x4u); G[2] = word(rt, G[16] + 0x8u); { const bool tk = G[5] == G[2]; G[4] = G[16]; if (tk) goto L_0881DE50; } if (G[5] == 0u) { G[5] = G[5] + 0x4u; goto L_0881DE48; } G[4] = G[5]; G[5] = G[30]; call_here(rt, ctx, 0x08A76ABCu); G[5] = word(rt, G[16] + 0x4u); G[5] = G[5] + 0x4u; L_0881DE48: ; put(rt, G[16] + 0x4u, G[5]); goto L_0881DE58; L_0881DE50: ; G[6] = G[30]; call_here(rt, ctx, 0x08B8A03Cu); L_0881DE58: ; G[4] = G[30]; call_here(rt, ctx, 0x08A5B9ECu); G[21] = G[21] + 0x1u; goto L_0881DED4; L_0881DE68: ; if (G[16] != 0u) { G[16] = G[16] + 0x14u; goto L_0881DE80; } G[2] = 0x08CA0000u; G[4] = 0x08C99264u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = G[16] + 0x14u; L_0881DE80: ; G[5] = G[18]; G[4] = frame.at(0u); G[6] = 0u; call_here(rt, ctx, 0x08857EE4u); G[5] = word(rt, G[16] + 0x4u); G[2] = word(rt, G[16] + 0x8u); { const bool tk = G[5] == G[2]; G[4] = G[16]; if (tk) goto L_0881DEC0; } { const bool tk = G[5] == 0u; G[4] = G[5]; if (tk) goto L_0881DEB4; } G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[5] = word(rt, G[16] + 0x4u); L_0881DEB4: ; G[5] = G[5] + 0x4u; put(rt, G[16] + 0x4u, G[5]); goto L_0881DEC8; L_0881DEC0: ; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B8A03Cu); L_0881DEC8: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[21] = G[21] + 0x1u; L_0881DED4: ; G[2] = 0x1Eu; { const bool tk = G[21] != G[2]; G[18] = G[18] + 0x8u; if (tk) goto L_0881DD80; } G[2] = G[22]; L_0881DEE4: ; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0890153C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); G[2] = word(rt, G[4] + 0x4u); G[18] = G[4] - G[2]; { const bool tk = G[18] == 0u; G[16] = G[4]; if (tk) goto L_08901700; } G[2] = word(rt, G[4] + 0x48u); { const bool tk = G[2] == 0u; put(rt, G[16] + 0x4u, G[4]); if (tk) goto L_08901578; } G[2] = G[18] + G[2]; put(rt, G[4] + 0x48u, G[2]); L_08901578: ; G[2] = word(rt, G[16] + 0x50u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901588; } put(rt, G[16] + 0x50u, G[2]); L_08901588: ; G[2] = word(rt, G[16] + 0x58u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901598; } put(rt, G[16] + 0x58u, G[2]); L_08901598: ; G[2] = word(rt, G[16] + 0x60u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_089015A8; } put(rt, G[16] + 0x60u, G[2]); L_089015A8: ; G[2] = word(rt, G[16] + 0x68u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_089015B8; } put(rt, G[16] + 0x68u, G[2]); L_089015B8: ; G[2] = word(rt, G[16] + 0x70u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_089015C8; } put(rt, G[16] + 0x70u, G[2]); L_089015C8: ; G[2] = word(rt, G[16] + 0x78u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_089015D8; } put(rt, G[16] + 0x78u, G[2]); L_089015D8: ; G[2] = word(rt, G[16] + 0x80u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_089015E8; } put(rt, G[16] + 0x80u, G[2]); L_089015E8: ; G[2] = word(rt, G[16] + 0x88u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_089015F8; } put(rt, G[16] + 0x88u, G[2]); L_089015F8: ; G[2] = word(rt, G[16] + 0x90u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901608; } put(rt, G[16] + 0x90u, G[2]); L_08901608: ; G[2] = word(rt, G[16] + 0x98u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901618; } put(rt, G[16] + 0x98u, G[2]); L_08901618: ; G[2] = word(rt, G[16] + 0xA0u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901628; } put(rt, G[16] + 0xA0u, G[2]); L_08901628: ; G[2] = word(rt, G[16] + 0xA8u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901638; } put(rt, G[16] + 0xA8u, G[2]); L_08901638: ; G[2] = word(rt, G[16] + 0xB0u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901648; } put(rt, G[16] + 0xB0u, G[2]); L_08901648: ; G[2] = word(rt, G[16] + 0xB8u); { const bool tk = G[2] == 0u; G[2] = G[18] + G[2]; if (tk) goto L_08901658; } put(rt, G[16] + 0xB8u, G[2]); L_08901658: ; G[2] = word(rt, G[16] + 0x44u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[2] = word(rt, G[16] + 0x8Cu); goto L_08901694; } G[17] = 0u; G[2] = word(rt, G[16] + 0x48u); L_0890166C: ; G[4] = G[17] << 3; G[5] = G[18]; G[4] = G[2] + G[4]; call_here(rt, ctx, 0x088FB578u); G[2] = word(rt, G[16] + 0x44u); G[17] = G[17] + 0x1u; G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x48u); goto L_0890166C; } G[2] = word(rt, G[16] + 0x8Cu); L_08901694: ; if (static_cast<std::int32_t>(G[2]) <= 0) { G[2] = word(rt, G[16] + 0xB4u); goto L_089016CC; } G[19] = 0u; G[17] = 0u; L_089016A4: ; G[4] = word(rt, G[16] + 0x90u); G[5] = G[18]; G[19] = G[19] + 0x1u; G[4] = G[4] + G[17]; call_here(rt, ctx, 0x08900F7Cu); G[2] = word(rt, G[16] + 0x8Cu); G[2] = static_cast<std::int32_t>(G[19]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = G[17] + 0xCu; if (tk) goto L_089016A4; } G[2] = word(rt, G[16] + 0xB4u); L_089016CC: ; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; if (tk) goto L_08901704; } G[19] = 0u; G[17] = 0u; L_089016DC: ; G[4] = word(rt, G[16] + 0xB8u); G[5] = G[18]; G[19] = G[19] + 0x1u; G[4] = G[17] + G[4]; call_here(rt, ctx, 0x08901498u); G[2] = word(rt, G[16] + 0xB4u); G[2] = static_cast<std::int32_t>(G[19]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = G[17] + 0x28u; if (tk) goto L_089016DC; } L_08901700: ; L_08901704: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_089E583C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); auto &F = ctx.fpr; G[18] = G[5]; put_float(rt, frame.at(0x34u), F[21]); put_float(rt, frame.at(0x30u), F[20]); G[2] = word(rt, G[4] + 0x150u); G[2] = G[2] & 0x8u; { const bool tk = G[2] == 0u; G[16] = G[4]; if (tk) goto L_089E58C4; } F[1] = float_at(rt, G[4] + 0x164u); F[0] = std::bit_cast<float>(0u); G[19] = sext(load16(G[4] + 0x120u), 16); ctx.set_fpu_condition(F[0] < F[1]); { const bool tk = !ctx.fpu_condition(); G[17] = sext(load16(G[4] + 0x122u), 16); if (tk) goto L_089E58C4; } G[19] = G[19] + G[17]; call_here(rt, ctx, 0x089E4E3Cu); F[20] = F[0]; G[4] = G[16]; call_here(rt, ctx, 0x089E4E28u); F[20] = F[20] / F[0]; G[4] = G[16]; call_here(rt, ctx, 0x089E56D4u); F[3] = float_at(rt, G[16] + 0x140u); F[1] = std::bit_cast<float>(G[19]); F[0] = psp_mul(F[0], F[3]); F[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); F[21] = F[0] + F[2]; goto L_089E58D0; L_089E58C4: ; F[21] = std::bit_cast<float>(0u); G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); L_089E58D0: ; G[4] = word(rt, G[16] + 0x190u); if (G[4] == 0u) { G[4] = G[16]; goto L_089E58EC; } G[2] = word(rt, G[16] + 0x19Cu); { const bool tk = G[18] == G[2]; G[17] = G[4]; if (tk) goto L_089E5900; } G[4] = G[16]; L_089E58EC: ; G[5] = G[18]; call_here(rt, ctx, 0x089E5814u); G[2] = word(rt, G[2] + 0xCu); { const bool tk = G[2] == 0u; G[17] = G[2]; if (tk) goto L_089E5908; } L_089E5900: ; { const bool tk = G[17] != 0u; G[4] = G[16]; if (tk) goto L_089E591C; } L_089E5908: ; G[2] = word(rt, G[16] + 0x15Cu); { const bool tk = G[2] == 0u; G[17] = 0u; if (tk) goto L_089E59F4; } G[17] = G[2]; G[4] = G[16]; L_089E591C: ; G[5] = G[18]; call_here(rt, ctx, 0x089E5814u); F[1] = float_at(rt, G[2] + 0x10u); F[2] = float_at(rt, G[2] + 0x14u); F[4] = float_at(rt, G[2] + 0x18u); F[1] = F[1] - F[21]; F[2] = psp_mul(F[20], F[2]); F[3] = float_at(rt, G[2] + 0x1Cu); F[0] = std::bit_cast<float>(0u); F[1] = psp_mul(F[20], F[1]); F[4] = psp_mul(F[20], F[4]); F[2] = F[2] + F[0]; F[3] = psp_mul(F[20], F[3]); F[1] = F[21] + F[1]; G[6] = frame.at(0x8u); G[7] = frame.at(0x4u); G[8] = frame.at(0u); G[4] = G[17]; G[5] = frame.at(0xCu); put_float(rt, frame.at(0xCu), F[1]); put_float(rt, frame.at(0x8u), F[2]); put_float(rt, frame.at(0x4u), F[4]); put_float(rt, frame.at(0x0u), F[3]); call_here(rt, ctx, 0x0896A9C4u); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089E5814u); G[3] = word(rt, G[2] + 0x20u); G[2] = 0x00010000u; G[2] = G[3] & G[2]; if (G[2] == 0u) { G[3] = 0u; goto L_089E59B0; } G[2] = 0x00020000u; G[2] = G[3] & G[2]; { const bool tk = G[2] != 0u; G[3] = 0x1u; if (tk) goto L_089E59B0; } G[3] = 0u; L_089E59B0: ; G[2] = word(rt, G[17] + 0xE0u); G[4] = G[16]; G[5] = G[18]; G[2] = (G[2] & ~0x4000u) | ((G[3] << 14) & 0x4000u); put(rt, G[17] + 0xE0u, G[2]); call_here(rt, ctx, 0x089E5814u); G[4] = G[17]; G[5] = word(rt, G[2] + 0x20u); G[5] = (G[5] >> 19) & 0x1u; call_here(rt, ctx, 0x089687D8u); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089E5814u); G[5] = word(rt, G[2] + 0x20u); G[4] = G[17]; G[5] = (G[5] >> 20) & 0x1u; call_here(rt, ctx, 0x08968850u); L_089E59F4: ; G[2] = G[17]; F[21] = float_at(rt, frame.at(0x34u)); F[20] = float_at(rt, frame.at(0x30u)); goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A797A0 (a0 = the output handle, a1 = the address of an object pointer, a2 = a mode, a3 = the address of a second object pointer): a text
// substring. Mode 0 takes the object's text from its virtual +0xDC(0) (a position), the length from +0xD8, and builds the result in a
// temporary (0x08A950D0(temp, position, length, 0x10)). Mode 1 reads a count from the second object (virtual +0x3C): when it is below the first
// object's length the result is the part after it (position +0xDC(count), length +0xD8 - count), else the whole text (0x08A94CE8). The temporary
// goes to the output handle (0x08A5BAD8 when it is another one) and is destroyed (0x08A5B9EC). Any other mode returns 1. A null object is
// reported first (0x08A5BB7C). The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_08A797A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4], slot = ctx.gpr[5], mode = ctx.gpr[6], other = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {17u, 0x14u}, {31u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {16u, 0x10u}});
    ctx.gpr[19] = out;
    ctx.gpr[17] = slot;
    // The object behind pointer slot `slot_at`; a null one is reported first (0x08A5BB7C).
    auto fetch = [&](u32 slot_at) {
        const u32 object = word(rt, slot_at);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
        return object;
    };
    auto virtual_at = [&](u32 object, u32 offset) { return word(rt, word(rt, object) + offset); };
    // Builds the result text in `temp` from (position, length), gives it to the output handle and destroys it.
    auto finish = [&](u32 temp, u32 position, u32 length) {
        call_set(rt, ctx, 0x08A950D0u, {temp, position, length, 0x10u});
        if (out != temp) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, temp)});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    };
    if (mode == 0u) {
        u32 object = fetch(slot);
        const u32 position = call_set(rt, ctx, virtual_at(object, 0xDCu), {object, 0u});
        ctx.gpr[18] = position;
        object = fetch(slot);
        ctx.gpr[16] = frame.at(0x8u);
        const u32 length = call_set(rt, ctx, virtual_at(object, 0xD8u), {object});
        finish(frame.at(0x8u), position, length);
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    u32 object = fetch(other);
    const u32 count = call_set(rt, ctx, virtual_at(object, 0x3Cu), {object});
    ctx.gpr[18] = count;
    object = fetch(slot);
    const u32 length = call_set(rt, ctx, virtual_at(object, 0xD8u), {object});
    if (count < length) {
        object = fetch(slot);
        const u32 position = call_set(rt, ctx, virtual_at(object, 0xDCu), {object, count});
        ctx.gpr[20] = position;
        object = fetch(slot);
        ctx.gpr[16] = frame.at(0x4u);
        const u32 rest = call_set(rt, ctx, virtual_at(object, 0xD8u), {object});
        finish(frame.at(0x4u), position, rest - count);
    } else {
        call_set(rt, ctx, 0x08A94CE8u, {frame.at(0x0u)});
        if (out != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x0u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
    }
}
// 0x08A99E1C (a0 = the output handle, a2 = a mode, a3 = the address of an argument block): picks an operand the way the mode says and builds a
// value with 0x08A99CB0 from a temporary object (0x08A98674 into the frame): mode 1 uses (block, 0, -1), mode 2 (block, the +0x4 object's virtual
// +0x3C, -1), mode 3 (block, the +0x4 object's virtual +0x3C, the +0x8 object's virtual +0x3C); any other mode uses -1 directly. The value
// goes through 0x08A85B1C into a temporary handle that is assigned to the output handle (0x08A5BAD8 when it is another one) and destroyed
// (0x08A5B9EC). A null object is reported first (0x08A5BB7C). The callee-saved registers hold what the asm keeps in them at every call.
void func_08A99E1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4], mode = ctx.gpr[6], block = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {17u, 0x14u}, {31u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {16u, 0x10u}});
    ctx.gpr[19] = out;
    ctx.gpr[17] = block;
    // The object behind pointer slot `slot_at`; a null one is reported first with `report`.
    auto fetch = [&](u32 slot_at, u32 report) {
        const u32 object = word(rt, slot_at);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {report});
        return object;
    };
    auto count_of = [&](u32 slot_at) {
        const u32 object = fetch(slot_at, 0x08CB2328u);
        return call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    };
    u32 value = 0xFFFFFFFFu;
    ctx.gpr[16] = value;
    if (mode == 2u) {
        const u32 temp = frame.at(0x8u);
        ctx.gpr[20] = temp;
        call_set(rt, ctx, 0x08A98674u, {temp});
        const u32 holder = word(rt, temp);
        ctx.gpr[18] = holder;
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB23CCu});
        const u32 first = count_of(block + 0x4u);
        value = call_set(rt, ctx, 0x08A99CB0u, {holder, block, first, 0xFFFFFFFFu});
        ctx.gpr[16] = value;
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    } else if (mode == 3u) {
        const u32 temp = frame.at(0x4u);
        ctx.gpr[21] = temp;
        call_set(rt, ctx, 0x08A98674u, {temp});
        const u32 holder = word(rt, temp);
        ctx.gpr[20] = holder;
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB23CCu});
        const u32 first = count_of(block + 0x4u);
        ctx.gpr[18] = first;
        const u32 second = count_of(block + 0x8u);
        value = call_set(rt, ctx, 0x08A99CB0u, {holder, block, first, second});
        ctx.gpr[16] = value;
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    } else if (mode == 1u) {
        const u32 temp = frame.at(0xCu);
        ctx.gpr[18] = temp;
        call_set(rt, ctx, 0x08A98674u, {temp});
        const u32 holder = fetch(temp, 0x08CB23CCu);
        value = call_set(rt, ctx, 0x08A99CB0u, {holder, block, 0u, 0xFFFFFFFFu});
        ctx.gpr[16] = value;
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    }
    const u32 handle = frame.at(0x0u);
    call_set(rt, ctx, 0x08A85B1Cu, {handle, value});
    if (out != handle) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, handle)});
    call_set(rt, ctx, 0x08A5B9ECu, {handle});
}
void func_08A9C7B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x1070u); put(rt, frame.at(0x105Cu), G[19]); put(rt, frame.at(0x1064u), G[31]); put(rt, frame.at(0x1058u), G[18]); put(rt, frame.at(0x1054u), G[17]); put(rt, frame.at(0x1050u), G[16]); put(rt, frame.at(0x1060u), G[20]); G[5] = 0x08CF0000u; G[5] = 0x08CF6ED8u; G[19] = frame.at(0xCu); G[18] = G[4]; G[17] = G[6]; call_here(rt, ctx, 0x08A90470u); G[4] = G[19]; G[5] = frame.at(0x44u); call_here(rt, ctx, 0x08A894C8u); G[16] = word(rt, frame.at(0xCu)); if (G[16] != 0u) { G[4] = G[16]; goto L_08A9C814; } G[4] = 0x08CB0000u; G[4] = 0x08CB265Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_08A9C814: ; G[5] = G[17]; G[6] = 0x1u; call_here(rt, ctx, 0x08A8917Cu); { const bool tk = G[2] == 0u; G[20] = frame.at(0x18u); if (tk) goto L_08A9C96C; } G[4] = G[20]; G[5] = G[19]; G[16] = frame.at(0x10u); call_here(rt, ctx, 0x08AA6740u); G[4] = G[20]; G[5] = G[16]; G[6] = 0x8u; call_here(rt, ctx, 0x08AA61D8u); G[5] = 0x08CB0000u; G[4] = G[16]; G[5] = 0x08CB2668u; G[6] = 0x4u; call_here(rt, ctx, 0x08B7C6BCu); if (G[2] != 0u) { store8(frame.at(0x14u), 0u); goto L_08A9C900; } G[4] = frame.at(0x14u); G[5] = 0u; G[6] = 0xAu; call_here(rt, ctx, 0x08B7DF7Cu); G[17] = G[2]; G[2] = static_cast<std::int32_t>(G[2]) < 9 ? 1u : 0u; { const bool tk = G[2] != 0u; put(rt, frame.at(0x40u), G[17]); if (tk) goto L_08A9C88C; } G[4] = G[20]; call_here(rt, ctx, 0x08AA6B80u); L_08A9C88C: ; G[4] = 0x34u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[17] = frame.at(0x8u); G[16] = G[2]; call_here(rt, ctx, 0x08A90FE4u); G[4] = G[17]; G[5] = frame.at(0x4u); put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08A76AB4u); { const bool tk = G[18] == G[17]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_08A9C8CC; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_08A9C8CC: ; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18] + 0x0u); if (G[16] != 0u) { G[4] = G[16]; goto L_08A9C8F0; } G[4] = 0x08CB0000u; G[4] = 0x08CB2588u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_08A9C8F0: ; G[5] = G[20]; call_here(rt, ctx, 0x08A9109Cu); G[16] = word(rt, frame.at(0xCu)); goto L_08A9C904; L_08A9C900: ; G[16] = word(rt, frame.at(0xCu)); L_08A9C904: ; if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A9C91C; } G[4] = 0x08CB0000u; G[4] = 0x08CB265Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A9C91C: ; G[2] = word(rt, G[3] + 0x11Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[4] = word(rt, frame.at(0x34u)); G[5] = word(rt, frame.at(0x38u)); G[6] = frame.at(0u); call_here(rt, ctx, 0x08C2FBF8u); G[4] = word(rt, frame.at(0x34u)); G[2] = word(rt, frame.at(0x3Cu)); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (tk) goto L_08A9C958; } G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08A9C958: ; G[5] = word(rt, frame.at(0x24u)); G[4] = frame.at(0x1Cu); call_here(rt, ctx, 0x08C0DD50u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); L_08A9C96C: ; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[18]; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x105Cu)); G[18] = word(rt, frame.at(0x1058u)); G[17] = word(rt, frame.at(0x1054u)); G[16] = word(rt, frame.at(0x1050u)); G[20] = word(rt, frame.at(0x1060u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B96974: inserts one element (a2) at position a1 of the vector at a0. With room to spare (+4 != +8) the slot one past the
// end is copy-constructed from the last element (0x08B9432C), the end grows, a stack copy of a2 is built at sp + 4, the elements
// from the end down to a1 move up one slot (0x08B957D8, assignment) and a1 is assigned from the stack copy, which is destroyed
// (0x08B93554 unless 0x08B60804 answers above zero, the empty sentinel being 0x08D75298). A full vector grows instead to twice
// its size (one slot when empty; 0x08B5ADDC when the count is 0x3FFFFFFF): 0x08B35DA0 allocates, 0x08B96910 builds the new
// element at a1 and the old tail after it, 0x08B9688C and 0x08B35254 finish the old buffer, and +0 / +4 / +8 take the new
// buffer, its end and its capacity end. v0 = the capacity end, or what the destruction answers.
void func_08B96974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[20] = G[5]; G[16] = G[4]; G[4] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[16] + 0x8u); { const bool tk = G[4] == G[2]; G[18] = G[6]; if (tk) goto L_08B96A64; } { const bool tk = G[4] == 0u; G[5] = G[4] + 0xFFFFFFFCu; if (tk) goto L_08B969C0; } call_here(rt, ctx, 0x08B9432Cu); G[4] = word(rt, G[16] + 0x4u); L_08B969C0: ; G[4] = G[4] + 0x4u; G[21] = frame.at(0x4u); put(rt, G[16] + 0x4u, G[4]); G[5] = G[18]; G[4] = G[21]; call_here(rt, ctx, 0x08B9432Cu); G[3] = word(rt, G[16] + 0x4u); G[17] = G[3] + 0xFFFFFFF8u; G[2] = G[17] - G[20]; G[19] = sra(G[2], 2); { const bool tk = static_cast<std::int32_t>(G[19]) <= 0; G[4] = G[3] + 0xFFFFFFFCu; if (tk) goto L_08B96A18; } G[16] = G[4]; G[18] = 0u; G[16] = G[16] + 0xFFFFFFFCu; L_08B969FC: ; G[17] = G[17] + 0xFFFFFFFCu; G[18] = G[18] + 0x1u; G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B957D8u); { const bool tk = G[19] != G[18]; G[16] = G[16] + 0xFFFFFFFCu; if (tk) goto L_08B969FC; } L_08B96A18: ; G[4] = G[20]; G[5] = G[21]; call_here(rt, ctx, 0x08B957D8u); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D75298u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; if (tk) goto L_08B96B34; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { goto L_08B96B34; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); goto L_08B96B34; L_08B96A64: ; G[5] = word(rt, G[16] + 0x0u); G[2] = 0x3FFF0000u; G[2] = G[2] | 0xFFFFu; G[3] = G[4] - G[5]; G[3] = sra(G[3], 2); { const bool tk = G[3] != G[2]; G[4] = 0x08CA0000u; if (tk) goto L_08B96A88; } G[4] = 0x08C9F840u; call_here(rt, ctx, 0x08B5ADDCu); L_08B96A88: ; { const bool tk = G[3] == 0u; G[6] = 0x1u; if (tk) goto L_08B96AA0; } G[6] = G[3] << 1; G[2] = G[6] < G[3] ? 1u : 0u; { const bool tk = G[2] != 0u; G[21] = 0xFFFFFFFCu; if (tk) goto L_08B96AAC; } L_08B96AA0: ; G[21] = G[6] << 2; { const bool tk = G[6] == 0u; G[19] = 0u; if (tk) goto L_08B96ABC; } L_08B96AAC: ; G[4] = G[21]; call_here(rt, ctx, 0x08B35DA0u); G[5] = word(rt, G[16] + 0x0u); G[19] = G[2]; L_08B96ABC: ; G[4] = G[5]; G[6] = G[19]; G[5] = G[20]; G[7] = frame.at(0x3u); call_here(rt, ctx, 0x08B96910u); { const bool tk = G[2] == 0u; G[17] = G[2]; if (tk) goto L_08B96AE4; } G[5] = G[18]; G[4] = G[2]; call_here(rt, ctx, 0x08B9432Cu); L_08B96AE4: ; G[5] = word(rt, G[16] + 0x4u); G[6] = G[17] + 0x4u; G[4] = G[20]; G[7] = frame.at(0x2u); call_here(rt, ctx, 0x08B96910u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); G[17] = G[2]; call_here(rt, ctx, 0x08B9688Cu); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[19] + G[21]; if (tk) goto L_08B96B24; } call_here(rt, ctx, 0x08B35254u); G[2] = G[19] + G[21]; L_08B96B24: ; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08B96B34: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08BBECF0: copy assignment of the vector of 16-byte elements at a0 from the one at a1 (itself: nothing to do). When the
// destination has fewer slots than the source (+8 - +0 against the source's +4 - +0) a buffer of exactly the source's size is
// allocated (0x08B35DA0) and the whole source range copy-constructed into it (0x08BBEC68); the old range is then destroyed
// (0x08BBEBE4) and freed (0x08B35254) and +0 / +4 / +8 become the new buffer and its two ends. Otherwise the elements the
// destination already holds are assigned one by one: when it holds fewer than the source the rest is copy-constructed at the old
// end (0x08BBEC68), otherwise the extra ones are destroyed (0x08BBE6F0). Either way end = begin + 16 * source count, and v0 = a0.
void func_08BBECF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x20u}, {18u, 0x18u}, {31u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}});
    if (source == self) {
        ctx.gpr[2] = self;
        return;
    }
    const u32 first = word(rt, source + 0x0u), last = word(rt, source + 0x4u);
    const u32 count = sra(last - first, 4);
    const u32 begin = word(rt, self + 0x0u);
    if (sra(word(rt, self + 0x8u) - begin, 4) < count) {
        u32 bytes = 0u, buffer = 0u;
        if (count != 0u) {
            bytes = count << 4;
            buffer = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        }
        call_set(rt, ctx, 0x08BBEC68u, {first, last, buffer, frame.at(0x0u)});
        call_set(rt, ctx, 0x08BBEBE4u, {word(rt, self + 0x0u), word(rt, self + 0x4u), frame.at(0x3u)});
        const u32 previous = word(rt, self + 0x0u);
        if (previous != 0u) call_set(rt, ctx, 0x08B35254u, {previous});
        put(rt, self + 0x8u, buffer + bytes);
        put(rt, self + 0x0u, buffer);
        put(rt, self + 0x4u, buffer + bytes);
    } else {
        const u32 held = sra(word(rt, self + 0x4u) - begin, 4);
        u32 destination = begin, from = first;
        if (held < count) {
            for (s32 index = 0; index < static_cast<s32>(held); ++index) {
                assign_element(rt, ctx, destination, from);
                destination += 0x10u;
                from += 0x10u;
            }
            const u32 end = word(rt, self + 0x4u);
            const u32 offset = (end - word(rt, self + 0x0u)) & 0xFFFFFFF0u;
            call_set(rt, ctx, 0x08BBEC68u, {word(rt, source + 0x0u) + offset, word(rt, source + 0x4u), end, frame.at(0x1u)});
        } else {
            for (s32 index = 0; index < static_cast<s32>(count); ++index) {
                assign_element(rt, ctx, destination, from);
                destination += 0x10u;
                from += 0x10u;
            }
            call_set(rt, ctx, 0x08BBE6F0u, {destination, word(rt, self + 0x4u), frame.at(0x2u)});
        }
        put(rt, self + 0x4u, word(rt, self + 0x0u) + (count << 4));
    }
    ctx.gpr[2] = self;
}
// 0x08855990 (a0 = the output handle, a1 = a key, a2 = a count, a3 = the address of an object pointer): with a2 > 0, a null object gives the
// default handle (0x08A97A6C(temp, 0)); otherwise the object's 0x08A6BF34 / 0x08A76F20 test selects one of two lookups (0x088547C4 with the key into a
// temporary, converted by 0x08854678 into a table), then 0x088A58A4 (with the object's virtual +0x50 result in a2:a3) or 0x088A5820 (with a
// string built by the object's virtual +0xC, released the library way: representation 0xC below the text, count at -4 changed by 0x08B60804,
// freed with 0x08B8542C unless it is the shared empty string 0x08D74FD0) produces a value that 0x08A97A6C turns into the result handle. The result
// goes to the output handle (0x08A5BAD8 when it is another one) and is destroyed (0x08A5B9EC). A null object is reported first (0x08A5BB7C).
// The callee-saved registers hold what the asm keeps in them at every call (their stores land on the stack).
void func_08855990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4], key = ctx.gpr[5], count = ctx.gpr[6], slot = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}, {31u, 0x34u}, {20u, 0x30u}, {16u, 0x20u}});
    ctx.gpr[19] = out;
    ctx.gpr[18] = key;
    ctx.gpr[17] = slot;
    if (static_cast<s32>(count) <= 0) return;
    const u32 result = frame.at(0x4u);
    // Gives the handle built in `temp` to the output handle and destroys it.
    auto deliver = [&](u32 temp) {
        if (out != temp) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, temp)});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    };
    // The object behind pointer slot `slot_at`; a null one is reported first (0x08A5BB7C).
    auto fetch = [&](u32 slot_at, u32 report) {
        const u32 object = word(rt, slot_at);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {report});
        return object;
    };
    u32 object = word(rt, slot);
    ctx.gpr[16] = object;
    if (object == 0u) {
        const u32 temp = frame.at(0x10u);
        ctx.gpr[16] = temp;
        call_set(rt, ctx, 0x08A97A6Cu, {temp, 0u});
        deliver(temp);
        return;
    }
    const u32 probe = call_set(rt, ctx, 0x08A6BF34u, {});
    const bool second_form = call_set(rt, ctx, 0x08A76F20u, {object, probe}) != 0u;
    const u32 temp = second_form ? frame.at(0x8u) : frame.at(0xCu);
    ctx.gpr[20] = temp;
    u32 value;
    if (!second_form) {
        call_set(rt, ctx, 0x088547C4u, {temp, key});
        object = fetch(temp, 0x08C9E188u);
        ctx.gpr[18] = call_set(rt, ctx, 0x08854678u, {object});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
        object = fetch(slot, 0x08C9E14Cu);
        const u32 low = call_set(rt, ctx, word(rt, word(rt, object) + 0x50u), {object});
        value = call_set(rt, ctx, 0x088A58A4u, {ctx.gpr[18], ctx.gpr[5], low, ctx.gpr[3]});
        ctx.gpr[17] = value;
    } else {
        call_set(rt, ctx, 0x088547C4u, {temp, key});
        object = fetch(temp, 0x08C9E188u);
        ctx.gpr[18] = call_set(rt, ctx, 0x08854678u, {object});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
        object = fetch(slot, 0x08C9E14Cu);
        const u32 text = frame.at(0x14u);
        call_set(rt, ctx, word(rt, word(rt, object) + 0xCu), {text, object});
        value = call_set(rt, ctx, 0x088A5820u, {ctx.gpr[18], word(rt, text)});
        ctx.gpr[17] = value;
        const u32 empty = 0x08D74FD0u;
        const u32 rep = word(rt, text) - 0xCu;
        ctx.gpr[2] = empty;
        ctx.gpr[16] = rep;
        if (rep != empty) {
            if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {word(rt, text) - 0x4u, 0xFFFFFFFFu})) <= 0) {
                call_set(rt, ctx, 0x08B8542Cu, {rep, frame.at(0x0u)});
            }
        }
    }
    ctx.gpr[16] = result;
    call_set(rt, ctx, 0x08A97A6Cu, {result, value});
    deliver(result);
}
void func_088F13A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); auto &F = ctx.fpr; G[16] = G[7]; G[19] = G[6]; G[18] = G[4]; G[17] = G[5]; put_float(rt, frame.at(0x30u), F[20]); call_here(rt, ctx, 0x088F1288u); { const bool tk = G[16] == 0u; G[2] = 0u; if (tk) goto L_088F155C; } G[7] = G[16] + 0xFFFFFFFFu; G[2] = G[7] < 0x8u ? 1u : 0u; G[16] = word(rt, G[17] + 0x10u); { const bool tk = G[2] == 0u; G[22] = 0u; if (tk) goto L_088F1408; } G[2] = G[7] << 3; G[2] = G[2] + G[16]; G[22] = load8(G[2] + 0x24u); L_088F1408: ; G[4] = word(rt, G[17] + 0x18u); call_here(rt, ctx, 0x088EF910u); if (G[2] == 0u) { G[2] = word(rt, G[18] + 0x0u); goto L_088F155C; } G[2] = load8(G[16] + 0x6Bu); { const bool tk = G[2] == 0u; G[3] = G[18] + 0x14u; if (tk) goto L_088F1534; } G[4] = word(rt, G[17] + 0x0u); G[5] = G[19]; call_here(rt, ctx, 0x088DCD90u); { const bool tk = G[2] == 0u; G[16] = G[2]; if (tk) goto L_088F1548; } G[3] = word(rt, G[2] + 0x0u); G[4] = G[2]; G[2] = word(rt, G[3] + 0x8Cu); { const std::uint32_t tg = G[2]; G[21] = 0u; call_here(rt, ctx, tg); } { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[20] = G[2]; if (tk) goto L_088F147C; } G[19] = 0u; L_088F1458: ; G[3] = word(rt, G[16] + 0x0u); G[5] = G[19]; G[4] = G[16]; G[2] = word(rt, G[3] + 0x94u); { const std::uint32_t tg = G[2]; G[19] = G[19] + 0x1u; call_here(rt, ctx, tg); } G[2] = 0u < G[2] ? 1u : 0u; { const bool tk = G[20] != G[19]; G[21] = G[21] + G[2]; if (tk) goto L_088F1458; } L_088F147C: ; { const bool tk = static_cast<std::int32_t>(G[20]) <= 0; G[3] = G[18] + 0x14u; if (tk) goto L_088F1534; } if (G[21] != G[20]) { G[2] = load8(G[3] + 0x3Bu); goto L_088F1538; } G[16] = word(rt, G[17] + 0x4u); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x14u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } F[1] = F[0]; G[1] = 0x3F000000u; F[0] = std::bit_cast<float>(G[1]); F[20] = float_at(rt, G[18] + 0x14u); F[0] = psp_mul(F[1], F[0]); ctx.set_fpu_condition(F[20] < F[0]); if (ctx.fpu_condition()) { F[20] = F[20] + F[1]; goto L_088F14C4; } L_088F14C4: ; G[3] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t tg = G[2]; G[5] = frame.at(0u); call_here(rt, ctx, tg); } if (G[2] == 0u) { G[2] = load8(G[18] + 0x4Fu); goto L_088F14F8; } F[0] = float_at(rt, frame.at(0x0u)); ctx.set_fpu_condition(F[20] < F[0]); { const bool tk = !ctx.fpu_condition(); G[3] = G[18] + 0x14u; if (tk) goto L_088F1534; } G[2] = load8(G[18] + 0x4Fu); L_088F14F8: ; G[2] = G[2] & 0x2u; { const bool tk = G[2] == 0u; G[3] = G[18] + 0x14u; if (tk) goto L_088F154C; } G[2] = load8(G[18] + 0x50u); if (G[2] != 0u) { G[2] = load8(G[3] + 0x3Bu); goto L_088F1550; } G[4] = G[22]; G[5] = 0x64u; call_here(rt, ctx, 0x088F10BCu); G[5] = G[2]; G[4] = G[18]; G[6] = 0x5u; call_here(rt, ctx, 0x088F1098u); G[3] = G[18] + 0x14u; goto L_088F154C; L_088F1534: ; G[2] = load8(G[3] + 0x3Bu); L_088F1538: ; G[2] = G[2] | 0x2u; store8(G[3] + 0x3Bu, G[2]); L_088F1540: ; G[2] = word(rt, G[18] + 0x0u); goto L_088F155C; L_088F1548: ; G[3] = G[18] + 0x14u; L_088F154C: ; G[2] = load8(G[3] + 0x3Bu); L_088F1550: ; G[2] = (G[2] & ~0x2u) | ((0u << 1) & 0x2u); store8(G[3] + 0x3Bu, G[2]); goto L_088F1540; L_088F155C: ; F[20] = float_at(rt, frame.at(0x30u)); goto exit_;  exit_: ; G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0891E93C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x58u), G[18]); put(rt, frame.at(0x54u), G[17]); put(rt, frame.at(0x50u), G[16]); put(rt, frame.at(0x5Cu), G[31]); auto &F = ctx.fpr; G[1] = 0x44250000u; F[0] = std::bit_cast<float>(G[1]); G[18] = G[4]; G[17] = G[6]; G[16] = G[5]; put_float(rt, frame.at(0x60u), F[20]); F[1] = float_at(rt, G[5] + 0x218u); G[2] = word(rt, G[5] + 0x224u); { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; F[20] = F[1] / F[0]; if (tk) goto L_0891E994; } G[4] = 0x08CF0000u; G[4] = 0x08CF28A8u; call_here(rt, ctx, 0x08B1ED60u); G[4] = G[2]; call_here(rt, ctx, 0x08B1EFF8u); F[1] = float_at(rt, G[16] + 0x218u); put(rt, G[16] + 0x224u, 0u); L_0891E994: ; G[1] = 0x41F00000u; F[0] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[1] < F[0]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_0891E9C4; } G[4] = word(rt, G[17] + 0x5C8u); call_here(rt, ctx, 0x08918148u); { const bool tk = G[2] == 0u; F[1] = float_at(rt, G[16] + 0x218u); if (tk) goto L_0891E9C4; } G[2] = 0x1u; store8(G[16] + 0x22Cu, G[2]); L_0891E9C4: ; G[1] = 0x43340000u; F[0] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[1] < F[0]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_0891E9E8; } G[2] = load8(G[16] + 0x22Cu); if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x21Cu); goto L_0891EA04; } L_0891E9E8: ; G[1] = 0x44250000u; F[0] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[0] <= F[1]); { const bool tk = !ctx.fpu_condition(); if (tk) goto L_0891EA1C; } G[2] = word(rt, G[16] + 0x21Cu); L_0891EA04: ; G[4] = word(rt, G[16] + 0x1E0u); put(rt, G[16] + 0x220u, 0u); G[2] = G[2] + 0x1u; G[5] = 0x3u; put(rt, G[16] + 0x21Cu, G[2]); call_here(rt, ctx, 0x089281E4u); L_0891EA1C: ; G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[0] < F[20]); if (ctx.fpu_condition()) { F[20] = F[0]; goto L_0891EA34; } L_0891EA34: ; G[3] = word(rt, G[16] + 0x224u); G[2] = 0x08CF0000u; G[2] = 0x08CF225Cu; G[4] = G[3] << 6; G[3] = G[3] << 3; G[3] = G[3] + G[4]; G[3] = G[3] + G[2]; G[4] = G[3] + 0x20u; G[2] = G[3] + 0xCu; G[5] = G[3] + 0x2Cu; F[14] = float_at(rt, G[2] + 0x8u); F[15] = float_at(rt, G[5] + 0x8u); F[0] = float_at(rt, G[3] + 0x0u); F[4] = float_at(rt, G[2] + 0x4u); F[8] = float_at(rt, G[4] + 0x4u); F[9] = float_at(rt, G[4] + 0x8u); F[11] = float_at(rt, G[5] + 0x4u); F[1] = float_at(rt, G[3] + 0x4u); F[2] = float_at(rt, G[3] + 0x8u); F[3] = float_at(rt, G[3] + 0xCu); F[7] = float_at(rt, G[3] + 0x20u); F[10] = float_at(rt, G[3] + 0x2Cu); F[13] = float_at(rt, G[4] + 0x18u); F[16] = float_at(rt, G[4] + 0x1Cu); F[5] = float_at(rt, G[3] + 0x18u); F[6] = float_at(rt, G[3] + 0x1Cu); F[12] = F[20]; G[2] = word(rt, G[3] + 0x40u); put_float(rt, frame.at(0x0u), F[0]); G[5] = G[16]; G[4] = G[18]; put_float(rt, frame.at(0x4u), F[1]); G[6] = frame.at(0u); put_float(rt, frame.at(0x8u), F[2]); put_float(rt, frame.at(0xCu), F[3]); put_float(rt, frame.at(0x10u), F[4]); put_float(rt, frame.at(0x14u), F[14]); put_float(rt, frame.at(0x18u), F[5]); put_float(rt, frame.at(0x1Cu), F[6]); put_float(rt, frame.at(0x20u), F[7]); put_float(rt, frame.at(0x24u), F[8]); put_float(rt, frame.at(0x28u), F[9]); put_float(rt, frame.at(0x2Cu), F[10]); put_float(rt, frame.at(0x30u), F[11]); put_float(rt, frame.at(0x34u), F[15]); put_float(rt, frame.at(0x38u), F[13]); put_float(rt, frame.at(0x3Cu), F[16]); F[0] = float_at(rt, G[3] + 0x44u); put(rt, frame.at(0x40u), G[2]); put_float(rt, frame.at(0x44u), F[0]); call_here(rt, ctx, 0x0891E2A8u); G[2] = G[18]; F[20] = float_at(rt, frame.at(0x60u)); goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x58u)); G[17] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x50u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08AA51E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x830u); put(rt, frame.at(0x818u), G[18]); put(rt, frame.at(0x814u), G[17]); put(rt, frame.at(0x810u), G[16]); put(rt, frame.at(0x828u), G[31]); put(rt, frame.at(0x824u), G[21]); put(rt, frame.at(0x820u), G[20]); put(rt, frame.at(0x81Cu), G[19]); G[18] = G[4]; G[17] = G[5]; G[3] = word(rt, G[6] + 0x0u); G[2] = word(rt, G[3] + 0xFFFFFFF4u); { const bool tk = G[2] == 0u; G[16] = G[6]; if (tk) goto L_08AA5368; } G[4] = G[6]; call_here(rt, ctx, 0x08AA43CCu); if (G[2] == 0u) { G[17] = word(rt, G[17] + 0x0u); goto L_08AA523C; } G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08B88F44u); G[2] = G[18]; goto L_08AA53A0; L_08AA523C: ; G[2] = sext(load8(G[17] + 0x0u), 8); G[3] = G[2] + 0xFFFFFF9Fu; G[2] = G[2] + 0xFFFFFFBFu; G[2] = G[2] < 0x1Au ? 1u : 0u; G[3] = G[3] < 0x1Au ? 1u : 0u; G[2] = G[2] | G[3]; { const bool tk = G[2] == 0u; G[16] = word(rt, G[16] + 0x0u); if (tk) goto L_08AA5294; } G[6] = G[17]; L_08AA5260: ; G[5] = sext(load8(G[6] + 0x1u), 8); G[4] = G[5] + 0xFFFFFFBFu; G[3] = G[5] + 0xFFFFFF9Fu; G[4] = G[4] < 0x1Au ? 1u : 0u; G[3] = G[3] < 0x1Au ? 1u : 0u; G[2] = G[5] + 0xFFFFFFD0u; G[4] = G[4] | G[3]; G[2] = G[2] < 0xAu ? 1u : 0u; G[2] = G[2] | G[4]; { const bool tk = G[2] == 0u; G[7] = G[6] + 0x1u; if (tk) goto L_08AA5378; } { const bool tk = G[5] != 0u; G[6] = G[7]; if (tk) goto L_08AA5260; } L_08AA5294: ; G[21] = G[17]; L_08AA5298: ; G[2] = sext(load8(G[16] + 0x0u), 8); G[3] = G[2] + 0xFFFFFF9Fu; G[2] = G[2] + 0xFFFFFFBFu; G[2] = G[2] < 0x1Au ? 1u : 0u; G[3] = G[3] < 0x1Au ? 1u : 0u; G[2] = G[2] | G[3]; if (G[2] == 0u) { store8(frame.at(0x1u), 0u); goto L_08AA52FC; } G[6] = G[16]; L_08AA52BC: ; G[5] = sext(load8(G[6] + 0x1u), 8); G[4] = G[5] + 0xFFFFFFBFu; G[3] = G[5] + 0xFFFFFF9Fu; G[4] = G[4] < 0x1Au ? 1u : 0u; G[3] = G[3] < 0x1Au ? 1u : 0u; G[2] = G[5] + 0xFFFFFFD0u; G[4] = G[4] | G[3]; G[2] = G[2] < 0xAu ? 1u : 0u; G[2] = G[2] | G[4]; { const bool tk = G[2] == 0u; G[7] = G[6] + 0x1u; if (tk) goto L_08AA538C; } { const bool tk = G[5] != 0u; G[6] = G[7]; if (tk) goto L_08AA52BC; } store8(frame.at(0x1u), 0u); goto L_08AA52FC; L_08AA52F8: ; store8(frame.at(0x1u), 0u); L_08AA52FC: ; G[19] = frame.at(0x1u); G[20] = 0x2Fu; G[2] = sext(load8(G[16] + 0x0u), 8); { const bool tk = G[2] == G[20]; G[4] = G[19]; if (tk) goto L_08AA5338; } G[5] = G[17]; call_here(rt, ctx, 0x08B7B9ACu); G[4] = G[17]; call_here(rt, ctx, 0x08B35F60u); G[3] = sext(load8(G[21] + 0x0u), 8); { const bool tk = G[3] == 0u; G[4] = G[19] + G[2]; if (tk) goto L_08AA5334; } store8(G[4] + 0x0u, G[20]); G[4] = G[4] + 0x1u; L_08AA5334: ; store8(G[4] + 0x0u, 0u); L_08AA5338: ; G[5] = G[16]; G[16] = frame.at(0x401u); call_here(rt, ctx, 0x08B7B9ACu); G[5] = G[19]; G[4] = G[16]; call_here(rt, ctx, 0x08AA4C64u); G[4] = G[18]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B85EB8u); G[2] = G[18]; goto L_08AA53A0; L_08AA5368: ; G[5] = G[6]; call_here(rt, ctx, 0x08B88F44u); G[2] = G[18]; goto L_08AA53A0; L_08AA5378: ; G[2] = 0x3Au; { const bool tk = G[5] == G[2]; G[21] = G[6] + 0x2u; if (tk) goto L_08AA5298; } G[21] = G[17]; goto L_08AA5298; L_08AA538C: ; G[2] = 0x3Au; if (G[5] == G[2]) { G[16] = G[6] + 0x2u; goto L_08AA52F8; } store8(frame.at(0x1u), 0u); goto L_08AA52FC; L_08AA53A0: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x818u)); G[17] = word(rt, frame.at(0x814u)); G[16] = word(rt, frame.at(0x810u)); G[21] = word(rt, frame.at(0x824u)); G[20] = word(rt, frame.at(0x820u)); G[19] = word(rt, frame.at(0x81Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B012D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xB0u); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); put(rt, frame.at(0xACu), G[31]); put(rt, frame.at(0xA0u), G[16]); G[18] = G[4]; G[2] = sext(load16(G[4] + 0x4u), 16); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[17] = G[5]; if (tk) goto L_08B014A4; } G[3] = G[2] << 4; G[2] = G[2] << 2; G[8] = word(rt, G[5] + 0x60u); G[3] = G[3] - G[2]; G[4] = word(rt, frame.at(0x50u)); G[5] = word(rt, frame.at(0x20u)); G[6] = word(rt, frame.at(0x24u)); G[2] = G[3] << 3; G[2] = G[2] - G[3]; G[2] = G[8] + G[2]; G[3] = G[17] + 0x60u; put(rt, frame.at(0x10u), G[4]); G[7] = 0xFFFFFFFFu; put(rt, frame.at(0x0u), G[5]); put(rt, frame.at(0x4u), G[6]); put(rt, frame.at(0x68u), G[3]); put(rt, frame.at(0x5Cu), 0u); put(rt, frame.at(0x58u), G[2]); put(rt, frame.at(0x54u), G[3]); put(rt, frame.at(0x30u), G[4]); put(rt, frame.at(0x34u), G[3]); put(rt, frame.at(0x38u), G[2]); put(rt, frame.at(0x3Cu), 0u); put(rt, frame.at(0x80u), G[4]); put(rt, frame.at(0x84u), G[3]); put(rt, frame.at(0x88u), G[2]); put(rt, frame.at(0x8Cu), 0u); put(rt, frame.at(0x2Cu), 0u); put(rt, frame.at(0x28u), 0u); put(rt, frame.at(0x70u), G[5]); put(rt, frame.at(0x74u), G[6]); put(rt, frame.at(0x78u), 0u); put(rt, frame.at(0x7Cu), 0u); put(rt, frame.at(0x14u), G[3]); put(rt, frame.at(0x18u), G[2]); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x8u), 0u); put(rt, frame.at(0xCu), 0u); { const bool tk = G[2] == 0u; put(rt, frame.at(0x64u), 0u); if (tk) goto L_08B013B8; } G[3] = G[2] - G[8]; G[2] = 0x3CF30000u; G[3] = sra(G[3], 2); G[2] = G[2] | 0xCF3Du; { const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32u); } G[3] = ctx.lo; G[7] = sext(G[3] & 0xFFFFu, 16); L_08B013B8: ; G[2] = 0xFFFFFFFFu; store16(frame.at(0x62u), G[2]); G[3] = 0u; store16(frame.at(0x60u), G[7]); { const bool tk = static_cast<std::int32_t>(G[7]) < 0; G[6] = word(rt, frame.at(0x68u)); if (tk) goto L_08B013EC; } G[2] = G[7] << 2; G[3] = G[7] << 4; G[3] = G[3] - G[2]; G[4] = word(rt, G[6] + 0x0u); G[2] = G[3] << 3; G[2] = G[2] - G[3]; G[3] = G[4] + G[2]; L_08B013EC: ; G[2] = word(rt, frame.at(0x40u)); put(rt, frame.at(0x84u), G[6]); put(rt, frame.at(0x80u), G[2]); put(rt, frame.at(0x88u), G[3]); put(rt, frame.at(0x4Cu), 0u); put(rt, frame.at(0x48u), G[3]); put(rt, frame.at(0x44u), G[6]); put(rt, frame.at(0x8Cu), 0u); goto L_08B01484; L_08B01410: ; G[2] = load8(G[16] + 0x51u); if (G[2] != 0u) { put(rt, frame.at(0x8Cu), G[16]); goto L_08B01458; } call_here(rt, ctx, 0x08C46CB0u); G[2] = word(rt, frame.at(0x90u)); G[3] = word(rt, frame.at(0x94u)); G[5] = G[16]; put(rt, frame.at(0x80u), G[2]); G[4] = G[17]; put(rt, frame.at(0x84u), G[3]); G[2] = word(rt, frame.at(0x98u)); G[3] = word(rt, frame.at(0x9Cu)); put(rt, frame.at(0x88u), G[2]); put(rt, frame.at(0x8Cu), G[3]); call_here(rt, ctx, 0x08B014BCu); G[16] = word(rt, frame.at(0x88u)); goto L_08B01488; L_08B01458: ; G[5] = sext(load16(G[16] + 0x52u), 16); G[2] = G[5] << 4; G[3] = G[5] << 2; G[2] = G[2] - G[3]; G[4] = G[2] << 3; { const bool tk = static_cast<std::int32_t>(G[5]) < 0; G[4] = G[4] - G[2]; if (tk) goto L_08B01480; } G[2] = word(rt, frame.at(0x84u)); G[3] = word(rt, G[2] + 0x0u); G[7] = G[3] + G[4]; L_08B01480: ; put(rt, frame.at(0x88u), G[7]); L_08B01484: ; G[16] = word(rt, frame.at(0x88u)); L_08B01488: ; G[4] = frame.at(0x90u); G[5] = frame.at(0x60u); G[6] = frame.at(0x80u); { const bool tk = G[16] != 0u; G[7] = 0u; if (tk) goto L_08B01410; } G[2] = sext(load16(frame.at(0x60u)), 16); store16(G[18] + 0x4u, G[2]); L_08B014A4: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B437C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[20] = G[4] + 0x8DCu; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x90u); { const std::uint32_t tg = G[2]; G[19] = G[4]; call_here(rt, ctx, tg); } G[5] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08B3EE8Cu); { const bool tk = G[2] != 0u; G[3] = G[2]; if (tk) goto L_08B4381C; } G[3] = word(rt, G[19] + 0x0u); G[2] = word(rt, G[3] + 0x40u); { const std::uint32_t tg = G[2]; G[4] = G[19]; call_here(rt, ctx, tg); } goto L_08B43988; L_08B4381C: ; G[2] = 0x1u; store8(G[3] + 0x4u, G[2]); G[5] = G[19] + 0x454u; G[6] = word(rt, G[5] + 0x480u); if (G[6] != 0u) { G[2] = load8(G[5] + 0x400u); goto L_08B43970; } G[3] = word(rt, G[19] + 0x0u); goto L_08B438D0; L_08B4383C: ; { const bool tk = G[2] == 0u; G[4] = G[4] + 0x1u; if (tk) goto L_08B43854; } { const bool tk = static_cast<std::int32_t>(G[3]) >= 0; G[7] = G[3]; if (tk) goto L_08B43868; } G[3] = word(rt, G[19] + 0x0u); goto L_08B438D0; L_08B43854: ; G[3] = G[3] + 0x1u; if (G[3] != G[6]) { G[2] = load8(G[4] + 0x401u); goto L_08B4383C; } G[3] = word(rt, G[19] + 0x0u); goto L_08B438D0; L_08B43868: ; G[2] = G[7] << 2; L_08B4386C: ; G[2] = G[2] + G[5]; G[17] = word(rt, G[2] + 0x0u); if (G[17] == 0u) { G[3] = word(rt, G[19] + 0x0u); goto L_08B438D0; } G[18] = word(rt, G[17] + 0x0u); L_08B43880: ; if (G[18] == 0u) { G[17] = word(rt, G[17] + 0x8u); goto L_08B438C4; } G[2] = load8(G[18] + 0x8u); { const bool tk = G[2] == 0u; G[4] = G[19]; if (tk) goto L_08B438C0; } G[3] = word(rt, G[19] + 0x0u); G[2] = word(rt, G[3] + 0x90u); { const std::uint32_t tg = G[2]; G[16] = sext(load16(G[18] + 0x16u), 16); call_here(rt, ctx, tg); } { const bool tk = G[16] == G[2]; G[4] = G[19]; if (tk) goto L_08B438C0; } G[3] = word(rt, G[19] + 0x0u); G[2] = word(rt, G[3] + 0x90u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } store16(G[18] + 0x16u, G[2]); L_08B438C0: ; G[17] = word(rt, G[17] + 0x8u); L_08B438C4: ; if (G[17] != 0u) { G[18] = word(rt, G[17] + 0x0u); goto L_08B43880; } G[3] = word(rt, G[19] + 0x0u); L_08B438D0: ; put(rt, frame.at(0x8u), 0u); put(rt, frame.at(0x0u), 0u); put(rt, frame.at(0x4u), 0u); store16(frame.at(0x8u), 0u); G[2] = word(rt, G[3] + 0x90u); { const std::uint32_t tg = G[2]; G[4] = G[19]; call_here(rt, ctx, tg); } G[4] = G[19]; G[5] = 0x4E23u; G[6] = frame.at(0u); G[7] = 0xCu; G[8] = 0x1u; G[9] = 0x1u; G[10] = 0u; G[11] = 0u; store16(frame.at(0xAu), G[2]); call_here(rt, ctx, 0x08B42A90u); { const bool tk = G[2] == 0u; if (tk) goto L_08B43988; } G[3] = word(rt, G[19] + 0x0u); G[4] = G[19]; G[2] = word(rt, G[3] + 0x8Cu); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[4] = G[20]; G[16] = G[2]; G[17] = G[3]; call_here(rt, ctx, 0x08B3F3C8u); { const bool tk = G[2] == 0u; if (tk) goto L_08B43988; } G[3] = word(rt, G[2] + 0x0u); L_08B4394C: ; if (G[3] == 0u) { G[2] = word(rt, G[2] + 0x8u); goto L_08B43960; } put(rt, G[3] + 0x30u, G[16]); put(rt, G[3] + 0x34u, G[17]); G[2] = word(rt, G[2] + 0x8u); L_08B43960: ; if (G[2] != 0u) { G[3] = word(rt, G[2] + 0x0u); goto L_08B4394C; } goto L_08B43988; L_08B43970: ; G[7] = 0u; G[4] = G[5]; { const bool tk = G[2] == 0u; G[3] = 0u; if (tk) goto L_08B43854; } G[2] = G[7] << 2; goto L_08B4386C; L_08B43988: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B989A8: destructor of an object that holds four refcounted handles: the vtable at +0 becomes 0x08CC2F78 and the handles
// at +0x28, +0x20, +0x14 and +0xC each give one reference back, then the base destructor 0x08B64654(self) runs as a tail call.
void func_08B989A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {17u, 0x4u}, {31u, 0xCu}, {16u, 0x0u}});
        ctx.gpr[17] = self;
        ctx.gpr[18] = self + 0x1Cu;
        put(rt, self + 0x0u, 0x08CC2F78u);
        release_handle(rt, ctx, word(rt, self + 0x28u));
        release_handle(rt, ctx, word(rt, self + 0x20u));
        release_handle(rt, ctx, word(rt, self + 0x14u));
        release_handle(rt, ctx, word(rt, self + 0xCu));
        ctx.gpr[4] = self;
    }
    jump(ctx, 0x08B64654u);
}
// 0x08B99618: destructor of an object that holds four refcounted handles: the vtable at +0 becomes 0x08CC2F78 and the handles
// at +0x28, +0x20, +0x14 and +0xC each give one reference back, then the base destructor 0x08B64654(self) runs as a tail call.
void func_08B99618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {17u, 0x4u}, {31u, 0xCu}, {16u, 0x0u}});
        ctx.gpr[17] = self;
        ctx.gpr[18] = self + 0x1Cu;
        put(rt, self + 0x0u, 0x08CC2F78u);
        release_handle(rt, ctx, word(rt, self + 0x28u));
        release_handle(rt, ctx, word(rt, self + 0x20u));
        release_handle(rt, ctx, word(rt, self + 0x14u));
        release_handle(rt, ctx, word(rt, self + 0xCu));
        ctx.gpr[4] = self;
    }
    jump(ctx, 0x08B64654u);
}
// 0x08BB017C: destructor of an object that holds four refcounted handles: the vtable at +0 becomes 0x08CC2F78 and the handles
// at +0x28, +0x20, +0x14 and +0xC each give one reference back, then the base destructor 0x08B64654(self) runs as a tail call.
void func_08BB017C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {17u, 0x4u}, {31u, 0xCu}, {16u, 0x0u}});
        ctx.gpr[17] = self;
        ctx.gpr[18] = self + 0x1Cu;
        put(rt, self + 0x0u, 0x08CC2F78u);
        release_handle(rt, ctx, word(rt, self + 0x28u));
        release_handle(rt, ctx, word(rt, self + 0x20u));
        release_handle(rt, ctx, word(rt, self + 0x14u));
        release_handle(rt, ctx, word(rt, self + 0xCu));
        ctx.gpr[4] = self;
    }
    jump(ctx, 0x08B64654u);
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_334
} // namespace gt::game

#define HOT334(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_334::name, gt::ReturnKind::kind)
HOT334(0x089E6218u, func_089E6218, Word);
HOT334(0x08A9CEFCu, func_08A9CEFC, Word);
HOT334(0x08B02D8Cu, func_08B02D8C, Word);
HOT334(0x08B15090u, func_08B15090, Word);
// HOT334(0x08B46AA0u, func_08B46AA0, Word);  // no comparable synthetic case yet
HOT334(0x088932A0u, func_088932A0, Word);
HOT334(0x088B3DE8u, func_088B3DE8, Word);
HOT334(0x08B44948u, func_08B44948, Word);
// HOT334(0x0881DD34u, func_0881DD34, Word);  // no comparable synthetic case yet
HOT334(0x0890153Cu, func_0890153C, Word);
// HOT334(0x089E583Cu, func_089E583C, Word);  // no comparable synthetic case yet
HOT334(0x08A797A0u, func_08A797A0, Word);
HOT334(0x08A99E1Cu, func_08A99E1C, Word);
HOT334(0x08A9C7B8u, func_08A9C7B8, Word);
// HOT334(0x08B96974u, func_08B96974, Word);  // no comparable synthetic case yet
HOT334(0x08BBECF0u, func_08BBECF0, Word);
HOT334(0x08855990u, func_08855990, Word);
HOT334(0x088F13A4u, func_088F13A4, Word);
HOT334(0x0891E93Cu, func_0891E93C, Word);
HOT334(0x08AA51E0u, func_08AA51E0, Word);
HOT334(0x08B012D8u, func_08B012D8, Word);
// HOT334(0x08B437C0u, func_08B437C0, Word);  // no comparable synthetic case yet
HOT334(0x08B989A8u, func_08B989A8, Word);
HOT334(0x08B99618u, func_08B99618, Word);
HOT334(0x08BB017Cu, func_08BB017C, Word);
