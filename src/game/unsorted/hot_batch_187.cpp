// Hot batch 187: twenty-two-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_187 {
namespace {

using namespace gt::game::hot;

using u32 = std::uint32_t;
using s32 = std::int32_t;
u32 call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(psprecomp::Runtime &rt, u32 address) { return rt.memory().load16(address); }
void set_byte(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void set_half(psprecomp::Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

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

// Decode one little-endian base-128 value (signed, 7 bits per byte) from the bytes at a0 into the word at a1; v0 is the end.
void read_varint(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 source = ctx.gpr[4];
    u32 value = 0u;
    u32 shift = 0u;
    u32 byte;
    do {
        byte = byte_at(rt, source);
        ++source;
        value |= (byte & 0x7Fu) << (shift & 31u);
        shift += 7u;
    } while (static_cast<s32>(static_cast<std::int8_t>(byte)) < 0);
    if (shift < 0x20u && (byte & 0x40u) != 0u) value |= 0xFFFFFFFFu << (shift & 31u);
    ctx.gpr[2] = source;
    put(rt, ctx.gpr[5], value);
}
// Walk the global list at 0x08D7734C calling the virtual at `slot`; 1 unless one of them returns 0 (which stops the walk).
void all_accept(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 slot) {
    u32 result = 1u;
    for (u32 node = word(rt, 0x08D7734Cu); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        if (call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object}) == 0u) {
            result = 0u;
            break;
        }
    }
    ctx.gpr[2] = result;
}
// 0x08BB....: `locate(a0, a1)` returns a position; when it is not a1 the caller tail-calls `target(position + step, a1, position, a2)`.
u32 locate_position(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 locate) {
    return call(rt, ctx, locate, {ctx.gpr[4], ctx.gpr[5]});
}
// 0x08AF2724(word-or-address of a1) then, when a2 is set, 0x08B5A5E0(7, a0, result, 1, 0); returns a0 in that case.
void dispatch_event(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 key) {
    const u32 self = ctx.gpr[4];
    const bool enabled = (ctx.gpr[6] & 0xFFu) != 0u;
    const u32 result = call(rt, ctx, 0x08AF2724u, {key});
    if (enabled) {
        call(rt, ctx, 0x08B5A5E0u, {7u, self, result, 1u, 0u});
        ctx.gpr[2] = self;
    }
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void finish_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 out, u32 slot) {
    if (out != slot) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, slot)});
    call(rt, ctx, 0x08A5B9ECu, {slot});
}
void func_08830BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 first = call(rt, ctx, 0x08B0979Cu);
    const u32 second = call(rt, ctx, 0x08B0980Cu, {first});
    call(rt, ctx, 0x08A97A6Cu, {slot, call(rt, ctx, 0x08B089ECu, {second})});
    finish_holder(rt, ctx, out, slot);
}
void func_0887552C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 text = ctx.gpr[5];
    u32 result = 1u;
    if (call(rt, ctx, 0x08B7B8C4u, {text, 0x08C9FF84u}) != 0u) result = call(rt, ctx, 0x08B35FD8u, {text, 0x08C9FF8Cu}) == 0u ? 1u : 0u;
    ctx.gpr[2] = result;
}
void func_08884FF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 key = call(rt, ctx, 0x0886A9C0u, {ctx.gpr[5], 5u});
    if (key != 0u) {
        const u32 object = call(rt, ctx, 0x0886F8D4u, {key});
        if (object != 0u)
            while (call_indirect(rt, ctx, word(rt, word(rt, object) + 0x8Cu), {object}) == 0u) {
            }
    }
}
void func_088A579C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 target = word(rt, self + 0x4C0u);
    ctx.gpr[2] = 0u;
    if (target != 0u) {
        const u32 element = self + 0x38u + static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, self + 8u))) * 384u;
        const u32 value = call(rt, ctx, 0x08932558u, {element});
        call(rt, ctx, 0x0892F848u, {target, ctx.gpr[5], value, self});
    }
}
void func_088B2714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 member = word(rt, self + 4u);
    put(rt, self, 0x08CC30D0u);
    if (member != 0u) call_indirect(rt, ctx, word(rt, word(rt, member) + 4u), {member});
    put(rt, self + 4u, 0u);
    put(rt, self, 0x08CC3138u);
    ctx.gpr[2] = 0x08CC3138u;
}
void func_088C56A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CC3348u);
        const u32 member = word(rt, self + 4u);
        ctx.gpr[2] = 0x08CC3348u;
        if (member != 0u) {
            put(rt, member, 0x08CC33A8u);
            call(rt, ctx, 0x08B64654u, {member});
        }
        put(rt, self, 0x08CC33C0u);
        ctx.gpr[2] = 0x08CC33C0u;
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_088C63FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CC3348u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x18u});
    put(rt, block, 0x08CC3420u);
    for (const u32 offset : {4u, 8u, 0xCu, 0x10u}) put(rt, block + offset, 0u);
    set_byte(rt, block + 0x14u, 0u);
    put(rt, self + 4u, block);
}
void func_088C7844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    put(rt, self, 0u);
    ctx.gpr[2] = source;
    if (source != 0u) {
        put(rt, self, source);
        put(rt, self + 0xCu, call(rt, ctx, 0x08AF6EA8u, {source, 0x08CA0A60u}));
        put(rt, self + 0x10u, call(rt, ctx, 0x08AF6EA8u, {word(rt, self), 0x08CA0A70u}));
    }
}
void func_088D76EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 tail_argument = 0u;
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 listener = word(rt, self + 0x640u);
        ctx.gpr[2] = listener;
        if (listener != 0u) {
            const u32 record = call(rt, ctx, 0x088D9B9Cu, {self});
            ctx.gpr[2] = 1u;
            if (word(rt, record + 0x1Cu) == 1u) {
                tail = true;
                tail_argument = word(rt, record + 0x2E4u);
            }
        }
    }
    if (tail) tail_call(ctx, 0x088E4704u, {word(rt, self + 0x640u), tail_argument});
}
void destroy_member_then_free(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 member = word(rt, self + 0x1F8u);
        put(rt, self, vtable);
        ctx.gpr[2] = vtable;
        if (member != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, member) + 4u), {member});
            put(rt, self + 0x1F8u, 0u);
        }
        call(rt, ctx, 0x08925FF8u, {self});
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_0891A608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_member_then_free(rt, ctx, 0x08CC5BF0u); }
void func_0891A774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_member_then_free(rt, ctx, 0x08CC5C48u); }
void func_0892795C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = call(rt, ctx, 0x089277B4u, {self});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x28u), {object});
    ctx.gpr[2] = value & ~word(rt, 0x08CDDD6Cu + word(rt, self + 0x1FC0u) * 4u);
}
void func_0892B778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 changed = 0u;
    if (byte_at(rt, self + 4u) != 0u) {
        const u32 end = word(rt, self + 0xCu);
        const u32 begin = word(rt, self + 8u);
        const u32 current = word(rt, self + 0x10u);
        divide(ctx, ctx.gpr[5], end - begin);
        const u32 sum = ctx.hi + current;
        u32 next = begin;
        if (!(static_cast<s32>(sum) < static_cast<s32>(begin))) next = static_cast<s32>(sum) < static_cast<s32>(end) ? sum : end - 1u;
        put(rt, self + 0x10u, next);
        changed = (next ^ current) != 0u ? 1u : 0u;
    }
    ctx.gpr[2] = changed;
}
void func_08938684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = word(rt, ctx.gpr[4] + 4u);
    const u32 key = ctx.gpr[5];
    s32 high = static_cast<s32>(word(rt, table + 8u));
    s32 low = -1;
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (high <= 0) return;
    for (;;) {
        const s32 middle = (low + high) >> 1;
        const u32 value = word(rt, static_cast<u32>(middle) * 8u + table + 0x10u);
        if (value == key) {
            ctx.gpr[2] = static_cast<u32>(middle);
            return;
        }
        if (value < key) low = middle;
        else high = middle;
        if (!(low + 1 < high)) break;
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}
void func_08992FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A0D658u, {self});
    put(rt, self, 0x08CCC098u);
    put(rt, self + 0x38u, 0x08CCC2F0u);
    for (const u32 offset : {0x138u, 0x120u, 0x124u, 0x128u, 0x12Cu, 0x130u, 0x134u}) put(rt, self + offset, 0u);
    ctx.gpr[2] = 0x08CCC098u;
}
void flag_from_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 assertion, u32 flag_address) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = 1u;
    if (ctx.gpr[5] == 1u) {
        const u32 object = word(rt, ctx.gpr[6]);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assertion});
        const u32 flag = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object}) != 0u ? 1u : 0u;
        ctx.gpr[2] = flag;
        set_byte(rt, flag_address, flag);
    }
}
void func_089A324C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_from_virtual(rt, ctx, 0x08CA4BF4u, 0x08CF38F0u); }
void func_089BB504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_from_virtual(rt, ctx, 0x08CA5CA8u, 0x08CF4D4Cu); }
void locked_call_returning_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 lock, u32 target, u32 offset) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B344E0u, {lock});
    call(rt, ctx, target, {self, ctx.gpr[5] + offset});
    call(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[2] = self;
}
void func_089A6FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF4994u, 0x08A0FD38u, 0x20u); }
void func_089A7024(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF4994u, 0x08A0FD38u, 0x24u); }
void func_089CD49C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF5134u, 0x08969178u, 0x154u); }
void func_089E5D98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF575Cu, 0x08969178u, 0x15Cu); }
void func_08A0FAA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF61B0u, 0x08A76ABCu, 0x64u); }
void func_08A0FB00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF61B0u, 0x08A76ABCu, 0x68u); }
void func_08A0FB58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF61B0u, 0x08A76ABCu, 0x6Cu); }
void func_08A0FBB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { locked_call_returning_self(rt, ctx, 0x08CF61B0u, 0x08A76ABCu, 0x70u); }
void func_089BFE58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 lock = source + 0x2B0u;
    call(rt, ctx, 0x08B344E0u, {lock});
    call(rt, ctx, 0x08B88F44u, {self, source + 0x230u});
    call(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[2] = self;
}
void assign_under_lock(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 lock, u32 target) {
    const u32 source = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B344E0u, {lock});
        if (target != source) call(rt, ctx, 0x08A5BAD8u, {target, word(rt, source)});
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}
void func_089A707C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_under_lock(rt, ctx, 0x08CF4994u, ctx.gpr[4] + 0x24u); }
void func_089A758C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_under_lock(rt, ctx, 0x08CF4994u, ctx.gpr[4] + 0x20u); }
void func_08A106C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_under_lock(rt, ctx, 0x08CF61B0u, ctx.gpr[4] + 0x64u); }
void func_08A10770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_under_lock(rt, ctx, 0x08CF61B0u, ctx.gpr[4] + 0x6Cu); }
void func_08A107C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_under_lock(rt, ctx, 0x08CF61B0u, ctx.gpr[4] + 0x70u); }
void func_089BFC50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_under_lock(rt, ctx, ctx.gpr[4] + 0x2B0u, ctx.gpr[4] + 0x264u); }
void func_089AF91C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kLock = 0x08CF4AA4u;
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B344E0u, {kLock});
    const u32 count = static_cast<u32>(static_cast<s32>(word(rt, self + 0x34u) - word(rt, self + 0x30u)) >> 2);
    call(rt, ctx, 0x08B3457Cu, {kLock});
    ctx.gpr[2] = count;
}
void func_089B5E18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 found = call(rt, ctx, 0x089B5E10u, {ctx.gpr[5]});
    u32 argument = 0x08CF6ED8u;
    if (found != 0u) {
        put(rt, slot, found);
        argument = slot;
    }
    call(rt, ctx, 0x08969188u, {out, argument});
    ctx.gpr[2] = out;
}
u32 text_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 text) {
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(0x0u);
    call(rt, ctx, 0x08B20920u, {slot, text});
    return call(rt, ctx, 0x08B20A54u, {slot, word(rt, ctx.gpr[5]), 0u});
}
void set_field_from_text(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 text, u32 offset, unsigned shift, u32 mask) {
    const u32 self = ctx.gpr[4];
    const u32 value = text_value(rt, ctx, text);
    put(rt, self + offset, (word(rt, self + offset) & ~(mask << shift)) | ((value & mask) << shift));
}
void func_089CD344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_field_from_text(rt, ctx, 0x08C8CCC4u, 0x14Cu, 6u, 1u); }
void func_089E04F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_field_from_text(rt, ctx, 0x08C8D42Cu, 0x1B0u, 8u, 1u); }
void func_089E05C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_field_from_text(rt, ctx, 0x08C8D42Cu, 0x1B0u, 7u, 1u); }
void func_08A07468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_field_from_text(rt, ctx, 0x08C8DDACu, 0x1A0u, 6u, 3u); }
void func_089D7580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = text_value(rt, ctx, 0x08C8CFE8u);
    call(rt, ctx, 0x089D72D0u, {self, value});
}
void func_08A2AAE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    for (;;) {
        ctx.gpr[2] = word(rt, 0x08D69174u);
        if (ctx.gpr[2] != 1u) break;
        call(rt, ctx, 0x08B2E2B8u, {0x3E8u});
        if (call(rt, ctx, 0x08B1A844u) != 0u) break;
    }
}
void func_08A2F800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 first = call(rt, ctx, 0x08B0980Cu, {word(rt, 0x08CF6648u)});
    call(rt, ctx, 0x08A97A6Cu, {slot, call(rt, ctx, 0x08B089F8u, {first})});
    finish_holder(rt, ctx, out, slot);
}
void func_08A308C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 record = call(rt, ctx, 0x08B09A04u, {word(rt, 0x08CF6648u)});
    call(rt, ctx, 0x08A97A6Cu, {slot, ~word(rt, record + 0xCu) >> 31});
    finish_holder(rt, ctx, out, slot);
}
void func_08A401EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A76ACCu, {self});
        put(rt, self, 0x08CD12A0u);
        put(rt, self + 0x14u, 0u);
        call(rt, ctx, 0x08B88F44u, {self + 0x108u, 0x08CF6848u});
    }
    tail_call(ctx, 0x08B215B8u, {self + 0x18u, 0u, 0u, 0u, 0u});
}
void func_08A550E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CB75B8u);
        call(rt, ctx, 0x08C1D728u, {self + 0x38u, word(rt, self + 0x40u)});
        put(rt, self, 0x08CB7440u);
        call(rt, ctx, 0x08A7D4E4u, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x5Cu, 4u});
}
void make_node_with_size(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 initialise) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 slot = frame.at(0x0u);
    const u32 node = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    call(rt, ctx, initialise, {node, 4u, 0u, 0u});
    put(rt, slot, node);
    call(rt, ctx, 0x08A76AB4u, {out, slot});
}
void func_08A5E070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node_with_size(rt, ctx, 0x08A5D7A0u); }
void func_08A832E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node_with_size(rt, ctx, 0x08A82AF0u); }
void negated_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 slot = frame.at(0x0u);
    const u32 negated_low = 0u - word(rt, self + 0x18u);
    const u32 borrow = negated_low != 0u ? 1u : 0u;
    const u32 negated_high = 0u - word(rt, self + 0x1Cu) - borrow;
    call(rt, ctx, target, {slot, borrow, negated_low, negated_high});
    finish_holder(rt, ctx, out, slot);
}
void func_08A5EACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { negated_pair(rt, ctx, 0x08A5DEE8u); }
void func_08A83C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { negated_pair(rt, ctx, 0x08A83160u); }
void func_08A70B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCD860u);
    call(rt, ctx, 0x08A71E28u, {self + 0x14u, ctx.gpr[5]});
    put(rt, self + 0x18u, 0x08D74FDCu);
    ctx.gpr[2] = 0x08D74FDCu;
}
void func_08A717EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCD748u);
    call(rt, ctx, 0x08A71E28u, {self + 0x14u, ctx.gpr[5]});
    const u32 tail_block = self + 0x18u;
    put(rt, self + 0x18u, 0u);
    put(rt, tail_block + 8u, 0u);
    put(rt, tail_block + 4u, 0u);
    ctx.gpr[2] = tail_block;
}
void func_08A726B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 sum_1 = first + ctx.gpr[6];
    const u32 sum_2 = sum_1 + ctx.gpr[7];
    const u32 sum_3 = sum_2 + ctx.gpr[8];
    put(rt, self + 8u, first);
    put(rt, self + 0xCu, sum_1);
    put(rt, self + 0x1Cu, sum_2);
    put(rt, self, 0u);
    put(rt, self + 4u, sum_3);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, sum_2);
    put(rt, self + 0x18u, 0u);
    ctx.gpr[2] = first;
    if (static_cast<s32>(sum_3) <= 0) return;
    put(rt, self, self + 0x20u);
    tail_call(ctx, 0x08B36018u, {self + 0x20u, 0u, sum_3 << 2});
}
void counter_step_wrapped(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, unsigned width, u32 delta, bool is_signed) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 slot = frame.at(0x0u);
    const u32 mask = width == 2u ? 0xFFFFu : 0xFFu;
    u32 value = ((width == 2u ? half_at(rt, self + 0x14u) : byte_at(rt, self + 0x14u)) + delta) & mask;
    if (is_signed) value = width == 2u ? static_cast<u32>(static_cast<std::int16_t>(value)) : static_cast<u32>(static_cast<std::int8_t>(value));
    if (width == 2u) set_half(rt, self + 0x14u, value);
    else set_byte(rt, self + 0x14u, value);
    call(rt, ctx, 0x08A85B1Cu, {slot, value});
    finish_holder(rt, ctx, out, slot);
}
void func_08A73D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 2u, 1u, true); }
void func_08A73D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 2u, 0xFFFFFFFFu, true); }
void func_08A96AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 1u, 1u, true); }
void func_08A96AF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 1u, 0xFFFFFFFFu, true); }
void func_08AB0C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 2u, 1u, false); }
void func_08AB0C64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 2u, 0xFFFFFFFFu, false); }
void func_08AB23B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 1u, 1u, false); }
void func_08AB2410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { counter_step_wrapped(rt, ctx, 1u, 0xFFFFFFFFu, false); }
void func_08A7D53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CB6DA8u);
        call(rt, ctx, 0x08C00860u, {self + 0x20u, word(rt, self + 0x28u)});
        const u32 member = word(rt, self + 0x1Cu);
        ctx.gpr[2] = member;
        if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
        call(rt, ctx, 0x08A69D68u, {self + 0x18u});
    }
    tail_call(ctx, 0x08A780B0u, {self});
}
void func_08A8C5D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[16] = word(rt, G[4] + 0x30u); G[4] = 0x08CB1CC0u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[17]; G[6] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08A90644u); }
void func_08AA8FD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 target = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        target = word(rt, self + 4u);
        if (target == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    }
    tail_call(ctx, 0x08C313A4u, {target, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
}
void func_08AAF838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CD3150u);
    call(rt, ctx, 0x08B88F44u, {self + 0x14u, ctx.gpr[5]});
    put(rt, self + 0x18u, ctx.gpr[6]);
}
void func_08ABAB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x08B34888u, {self + 0x14u, target, ctx.gpr[6], ctx.gpr[7]});
        put(rt, target + 0x14u, ctx.gpr[6]);
        put(rt, target + 0x18u, ctx.gpr[7]);
    }
    tail_call(ctx, 0x08B34B24u, {self + 0x14u});
}
void func_08ABBBB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AB9C38u, {0x08CFFA94u});
    for (u32 node = word(rt, 0x08D77188u); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0x10u), {object});
    }
}
void func_08AC2A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    put(rt, out, ctx.gpr[6]);
    u32 done = 0u;
    if (call(rt, ctx, 0x08AC287Cu) == 4u) {
        put(rt, out, call(rt, ctx, 0x08AC2900u, {self, 0u}));
        done = 1u;
    }
    ctx.gpr[2] = done;
}
void func_08ACC2BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08ABFD0Cu, {self, 0x08D77204u, 0x08D771D0u});
    put(rt, self, 0x08CD6AA0u);
    call(rt, ctx, 0x08B28D34u, {self + 0x60u});
    put(rt, self + 0x94u, 0u);
    set_byte(rt, self + 0x8Cu, 0u);
    set_byte(rt, self + 0x8Du, 0u);
    put(rt, self + 0x90u, 0u);
}
void func_08AE3CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 table = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    u32 result = 0u;
    if (index < 0x28u) {
        result = index;
        if (static_cast<s32>(index) < 0x20) result = call(rt, ctx, 0x08ADEACCu, {index, index << 2});
        else result = word(rt, word(rt, word(rt, table + 0x10u) + 0x10u) + (index << 2) + 0x30u);
    }
    ctx.gpr[2] = result;
}
void func_08AE75C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 argument = ctx.gpr[5];
    if (argument != 0u) {
        const u32 group = word(rt, argument + 4u);
        const bool negative = static_cast<s32>(word(rt, group + 8u)) < 0;
        ctx.gpr[2] = negative ? 1u : 0u;
        u32 node = negative ? 0u : group;
        while (node != 0u) {
            u32 next = word(rt, node + 4u);
            const bool next_negative = static_cast<s32>(word(rt, next + 8u)) < 0;
            ctx.gpr[2] = next_negative ? 1u : 0u;
            if (next_negative) next = 0u;
            call(rt, ctx, 0x08AE740Cu, {node});
            node = next;
        }
    }
}
void func_08AEBCB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[2] = word(rt, self);
    if (ctx.gpr[2] != 0u) {
        call(rt, ctx, 0x08C7A9D8u, {word(rt, self + 0xCu)});
        call(rt, ctx, 0x08C7AAA0u, {word(rt, self + 0xCu), 0u});
        put(rt, self + 0xCu, 0xFFFFFFFFu);
        call(rt, ctx, 0x08AD85C8u, {word(rt, self + 0x20u), 0u});
        put(rt, self + 0x20u, 0u);
        put(rt, self + 0x24u, 0u);
    }
}
void func_08AEF8E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kTable = 0x08D41AD0u;
    const u32 index = ctx.gpr[4];
    u32 found = 0u;
    if (static_cast<s32>(index) >= 0 && !(static_cast<s32>(word(rt, kTable + 0xA0u) - 1u) < static_cast<s32>(index))) {
        const u32 record = word(rt, kTable + 0x9Cu) + index * 56u;
        found = 1u;
        put(rt, ctx.gpr[5], word(rt, record + 0x18u));
        put(rt, ctx.gpr[6], word(rt, record + 0x1Cu));
    }
    ctx.gpr[2] = found;
}
void func_08AEFCF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 first = 0u;
    call(rt, ctx, 0x08AEB808u, {self});
    if (word(rt, self + 0x38u) == 0u) {
        put(rt, self + 0x3Cu, word(rt, self + 0x3Cu) + 1u);
        first = 1u;
    }
    call(rt, ctx, 0x08AEB82Cu, {self});
    ctx.gpr[2] = first;
}
void func_08AF8C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 base = word(rt, self + 0x54u);
    ctx.gpr[2] = base;
    if (base != 0u) {
        const u32 end = base + (half_at(rt, self + 0x2Au) << 2);
        const u32 found = call(rt, ctx, 0x08C40938u, {base, end, ctx.gpr[5]});
        ctx.gpr[2] = end == found ? 0u : word(rt, found);
    }
}
void func_08B0157C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 argument = frame.at(0x0u);
    put(rt, argument, ctx.gpr[5]);
    const u32 found = call(rt, ctx, 0x08C47208u, {word(rt, self + 0xA8u), word(rt, self + 0xACu), argument});
    if (word(rt, self + 0xACu) != found) call(rt, ctx, 0x08C47150u, {self + 0xA8u, found});
}
void func_08B097AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kCurrent = 0x08D473BCu;
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 current = word(rt, kCurrent);
    if ((current ^ self) == 0u) current = 0u;
    put(rt, kCurrent, current);
    ctx.gpr[2] = 0x08D40000u;  // the `lui` half of kCurrent, left in v0 when there is no member to release
    const u32 member = word(rt, self + 4u);
    if (member != 0u) {
        call_indirect(rt, ctx, word(rt, word(rt, member)), {member, word(rt, self)});
        put(rt, self, 0u);
        put(rt, self + 4u, 0u);
    }
}
void func_08B0D4B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = 0u;
    for (u32 node = word(rt, 0x08D7734Cu); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        if (call_indirect(rt, ctx, word(rt, word(rt, object) + 0x30u), {object}) != 0u) break;
    }
}
void func_08B0D958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = 1u;
    for (u32 node = word(rt, 0x08D7734Cu); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        if (call_indirect(rt, ctx, word(rt, word(rt, object) + 0x50u), {object}) == 0u) break;
        ctx.gpr[2] = 1u;
    }
}
void func_08A9E880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 cursor_slot = ctx.gpr[4];
    u32 cursor = word(rt, cursor_slot);
    u32 digit = (static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, cursor))) - 0x30u) & 0xFFu;
    u32 value = 0u;
    if (digit < 10u) {
        ++cursor;
        for (;;) {
            const u32 next = (static_cast<u32>(static_cast<std::int8_t>(byte_at(rt, cursor))) - 0x30u) & 0xFFu;
            value = digit + value * 10u;
            digit = next;
            if (!(digit < 10u)) break;
            ++cursor;
        }
    }
    ctx.gpr[2] = value;
    put(rt, cursor_slot, cursor);
}
void func_0886E088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[5] = 5u; G[16] = G[4]; call_here(rt, ctx, 0x0886A9A0u); ctx.fpr[12] = 0.0f; if (G[2] != 0u) { G[4] = word(rt, G[16] + 0x70u) + 0x14u; call_here(rt, ctx, 0x0886F884u); G[4] = G[16]; G[5] = 5u; call_here(rt, ctx, 0x0886AA0Cu); } ctx.fpr[12] = 0.0f; G[4] = word(rt, G[16] + 0x70u); ctx.fpr[13] = ctx.fpr[12]; ctx.fpr[14] = ctx.fpr[12]; } G[16] = s0; jump(ctx, 0x08871568u); }
void func_08899D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool tail = false; { GuestFrame frame(ctx, 0x10u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x44u); call_here(rt, ctx, G[2]); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 8u); call_here(rt, ctx, G[3]); G[1] = 0x42652EE0u; ctx.fpr[12] = std::bit_cast<float>(G[1]); ctx.fpr[12] = psp_mul(ctx.fpr[0], ctx.fpr[12]); tail = true; } } if (tail) jump(ctx, 0x08B56128u); }
void func_088DA394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x130u); G[16] = word(rt, G[2] + 0x30u); G[17] = word(rt, G[2] + 0x34u); call_here(rt, ctx, 0x088D9C70u); G[4] = G[16] - G[2]; G[5] = G[17] - G[3]; G[6] = G[16] < G[4] ? 1u : 0u; G[5] -= G[6]; G[2] = G[4]; G[3] = G[5]; if (static_cast<std::int32_t>(G[5]) < 0) { G[2] = 0xFFFFFFFFu; G[3] = 0xFFFFFFFFu; } } G[16] = s0; G[17] = s1; }
void func_088E4464(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; for (;;) { G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x4Cu); call_here(rt, ctx, G[2]); if (G[2] == 0u) break; call_here(rt, ctx, 0x08B1A844u); G[4] = 0x7D0u; if (G[2] != 0u) break; call_here(rt, ctx, 0x08B2E2B8u); } } G[16] = s0; }
void func_088ED0D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[2] = G[6]; G[8] = frame.at(0u); G[10] = (load8(G[6]) | load8(G[6] + 1u) << 8 | load8(G[6] + 2u) << 16 | static_cast<std::uint32_t>(load8(G[6] + 3u)) << 24); G[3] = (load8(G[6] + 4u) | load8(G[6] + 4u + 1u) << 8 | load8(G[6] + 4u + 2u) << 16 | static_cast<std::uint32_t>(load8(G[6] + 4u + 3u)) << 24); store8(frame.at(8u), G[5]); G[6] = 0xFFFFFFFFu; G[5] = 0u; G[7] = 0xBu; put(rt, frame.at(4u), G[3]); put(rt, frame.at(0u), G[10]); G[4] = word(rt, G[4] + 0x538u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xF4u); G[9] = 0xCu; call_here(rt, ctx, G[2]); } }
void func_08927904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool five = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x089277BCu); G[3] = 5u; G[4] = G[16]; five = G[2] == G[3]; if (!five) { call_here(rt, ctx, 0x089277B4u); G[3] = word(rt, G[2]); G[4] = G[2]; G[2] = word(rt, G[3] + 0x30u); G[25] = G[2]; } } G[16] = s0; if (five) jump(ctx, 0x08920018u); else jump(ctx, G[25]); }
void func_0897851C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x184u; G[18] = G[4] + 0x18Cu; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08BEDDC4u); G[4] = G[18]; put(rt, G[16] + 0x184u, G[17]); put(rt, G[17] + 4u, G[17]); call_here(rt, ctx, 0x08BEDDC4u); put(rt, G[16] + 0x18Cu, G[18]); put(rt, G[18] + 4u, G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0897E884(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x0897E844u); G[4] = 0x08CA3400u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = G[16] + 0xF0u; ctx.fpr[0] = float_at(rt, G[2] + 0x40u); ctx.fpr[1] = float_at(rt, G[2] + 0x44u); G[4] = frame.at(0u); put_float(rt, G[2] + 0xBCu, ctx.fpr[0]); put_float(rt, G[2] + 0xC0u, ctx.fpr[1]); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_0898970C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089896CCu); G[4] = 0x08CA39F0u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = G[16] + 0xF0u; ctx.fpr[0] = float_at(rt, G[2] + 0x40u); ctx.fpr[1] = float_at(rt, G[2] + 0x44u); G[4] = frame.at(0u); put_float(rt, G[2] + 0xBCu, ctx.fpr[0]); put_float(rt, G[2] + 0xC0u, ctx.fpr[1]); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_089AB444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089AB370u); G[4] = 0x08CA50C4u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x089D5B30u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08978D3Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_089AB49C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089AB370u); G[4] = 0x08CA50C4u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08965418u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x0897BB18u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_089AB4F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x089AB370u); G[4] = 0x08CA50C4u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08965418u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08978D3Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A0F800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[16] = G[5]; G[5] = 0x08C8E004u; G[17] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B20920u); G[4] = frame.at(0u); G[5] = word(rt, G[16]); G[6] = 0u; call_here(rt, ctx, 0x08B20A54u); G[3] = word(rt, G[17] + 0x60u); G[3] = (G[3] & ~(0xFu << 24)) | ((G[2] & 0xFu) << 24); put(rt, G[17] + 0x60u, G[3]); } G[16] = s0; G[17] = s1; }
void func_08A26ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; call_here(rt, ctx, 0x08B344E0u); G[4] = G[16]; G[17] = word(rt, G[16] + 0x28u); put(rt, G[16] + 0x28u, 0u); call_here(rt, ctx, 0x08B3457Cu); G[4] = G[17]; tail = G[17] != 0u; } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08B1B99Cu); }
void func_08A888F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[2] = G[4]; G[16] = G[5]; ctx.fpr[12] = float_at(rt, G[4] + 0x14u); G[4] = frame.at(0u); ctx.fpr[0] = ctx.fpr[12] + ctx.fpr[0]; put_float(rt, G[2] + 0x14u, ctx.fpr[0]); call_here(rt, ctx, 0x08A87EECu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A88948(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[2] = G[4]; G[16] = G[5]; ctx.fpr[12] = float_at(rt, G[4] + 0x14u); G[4] = frame.at(0u); ctx.fpr[0] = ctx.fpr[12] - ctx.fpr[0]; put_float(rt, G[2] + 0x14u, ctx.fpr[0]); call_here(rt, ctx, 0x08A87EECu); G[4] = G[16]; if (G[16] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; }
void func_08A94B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x14u); G[16] = word(rt, G[6]); G[4] = 0x08CB211Cu; G[17] = G[2] + G[5]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); store8(G[17], G[2]); } G[16] = s0; G[17] = s1; }
void func_08AA91A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[7]; G[16] = G[6]; G[18] = word(rt, G[4] + 4u); G[4] = 0x08CB30B0u; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[18]; G[6] = G[16]; G[7] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, 0x08C31374u); }
void func_08ABFA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); G[16] = G[4]; put(rt, frame.at(0x10u), bits_of(f20)); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x48u); ctx.fpr[20] = ctx.fpr[12]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x44u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[16]; G[25] = word(rt, G[3] + 0x28u); } G[16] = s0; ctx.fpr[20] = f20; jump(ctx, G[25]); }
void func_08AD1040(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[3] = G[4] + 0x40u; G[5] = G[4] + 0x58u; ctx.fpr[14] = float_at(rt, G[3] + 8u); ctx.fpr[13] = float_at(rt, G[3] + 4u); G[3] = G[4] + 0x4Cu; ctx.fpr[12] = float_at(rt, G[4] + 0x40u); ctx.fpr[17] = float_at(rt, G[3] + 8u); ctx.fpr[15] = float_at(rt, G[4] + 0x4Cu); ctx.fpr[16] = float_at(rt, G[3] + 4u); ctx.fpr[17] = ctx.fpr[14] + ctx.fpr[17]; ctx.fpr[15] = ctx.fpr[12] + ctx.fpr[15]; ctx.fpr[16] = ctx.fpr[13] + ctx.fpr[16]; ctx.fpr[18] = float_at(rt, G[4] + 0x58u); ctx.fpr[19] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 8u); put_float(rt, frame.at(0u), ctx.fpr[0]); call_here(rt, ctx, 0x08B509ECu); } }
void func_08AE493C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[2] = G[6]; G[6] = G[7]; G[7] = G[8]; G[8] = G[9]; G[9] = G[10]; G[10] = G[11]; G[11] = word(rt, frame.at(0x20u)); if (G[2] == 0u) { G[2] = word(rt, frame.at(0x24u)); G[3] = word(rt, frame.at(0x28u)); put(rt, frame.at(0u), G[2]); G[2] = word(rt, frame.at(0x2Cu)); put(rt, frame.at(4u), G[3]); put(rt, frame.at(8u), G[2]); call_here(rt, ctx, 0x08AE4814u); } } }
void func_08AE5B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x50u); G[17] = G[5]; G[2] = word(rt, G[4] + 0x34u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[4] = sext(load16(G[16] + 0x1Au), 16u); G[3] = 0xDu; put(rt, frame.at(0u), G[3]); G[5] = frame.at(0u); G[3] = 1u; put(rt, frame.at(4u), G[17]); put(rt, frame.at(8u), G[3]); put(rt, frame.at(0xCu), G[2]); call_here(rt, ctx, 0x08AE85E8u); } G[16] = s0; G[17] = s1; }
void func_08AEF958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); G[5] = frame.at(0u); G[6] = frame.at(4u); call_here(rt, ctx, 0x08AEF8E4u); ctx.fpr[0] = 0.0f; G[3] = word(rt, frame.at(0u)); if (G[2] != 0u) { G[2] = 1u; if (G[3] == G[2]) { G[4] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08B842ACu); G[2] = sra(G[2], 16u); ctx.fpr[0] = std::bit_cast<float>(G[2]); G[2] = high_half(0x08CB6158u); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(ctx.fpr[0]))); ctx.fpr[0] = float_at(rt, 0x08CB6158u); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); } } } }
void func_08AF826C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = high_half(0x08CE0A14u); ctx.fpr[1] = float_at(rt, 0x08CE0A14u); G[1] = 0x3F000000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[2] = 0u; bool hit = ctx.fpr[1] < ctx.fpr[12]; ctx.fpr[2] = psp_mul(ctx.fpr[1], ctx.fpr[0]); if (hit) return; hit = ctx.fpr[2] < ctx.fpr[12]; G[1] = 0x3E800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[1], ctx.fpr[0]); G[2] = 1u; if (hit) return; hit = ctx.fpr[0] < ctx.fpr[12]; G[2] = 2u; if (hit) return; G[2] = 3u; }
void func_08B08450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0xD4u); G[16] = G[4]; call_here(rt, ctx, G[3]); G[3] = 0u; G[4] = 0x10u; G[2] = G[16] + 0x20u; do { G[3] += 1u; put(rt, G[2] + 0xCu, 0u); put(rt, G[2] + 0x10u, 0u); G[2] += 8u; } while (G[3] != G[4]); G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xD8u); } G[16] = s0; jump(ctx, G[25]); }
void func_08B08544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xD4u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); put(rt, G[16] + 0x4u, G[17]); G[4] = G[16]; put(rt, G[16] + 0x8u, G[18]); G[25] = word(rt, G[3] + 0xD8u); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, G[25]); }
void func_08B0859C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xD4u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); put(rt, G[16] + 0xCu, G[17]); G[4] = G[16]; put(rt, G[16] + 0x10u, G[18]); G[25] = word(rt, G[3] + 0xD8u); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, G[25]); }
void func_08B085F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xD4u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); put(rt, G[16] + 0x14u, G[17]); G[4] = G[16]; put(rt, G[16] + 0x18u, G[18]); G[25] = word(rt, G[3] + 0xD8u); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, G[25]); }
void func_08B086A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xD4u); G[16] = G[4]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); put(rt, G[16] + 0x24u, G[17]); G[4] = G[16]; put(rt, G[16] + 0x28u, G[18]); G[25] = word(rt, G[3] + 0xD8u); } G[16] = s0; G[17] = s1; G[18] = s2; jump(ctx, G[25]); }
void func_08B0A744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); call_here(rt, ctx, 0x08B0A5F0u); G[4] = G[2]; G[5] = 0u; if (G[2] != 0u) { G[3] = word(rt, G[2]); G[2] = word(rt, G[3] + 0x8Cu); call_here(rt, ctx, G[2]); G[4] = G[2]; G[2] |= G[3]; G[5] = G[3]; } if (G[4] == 0u && G[5] == 0u) { G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; } G[2] = G[4]; G[3] = G[5]; } }
void func_08B0D9B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    all_accept(rt, ctx, 0x14u);
}
void func_08B0DA08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    all_accept(rt, ctx, 0x18u);
}
void func_08B0DA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D7734Cu); G[16] = word(rt, 0x08D7734Cu); G[2] = 1u; while (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x3Cu); call_here(rt, ctx, G[2]); if (G[2] == 0u) break; G[16] = word(rt, G[16]); G[2] = 1u; } } G[16] = s0; }
void func_08B153B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = G[6]; G[17] = G[5]; G[4] = word(rt, G[5] + 0x10u); G[6] = word(rt, G[6] + 4u); G[5] = word(rt, G[2]); call_here(rt, ctx, 0x08B33E50u); G[16] = G[2]; G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) < 0) { call_here(rt, ctx, 0x08B1830Cu); put(rt, G[17] + 4u, G[2]); } G[2] = G[16]; } G[16] = s0; G[17] = s1; }
void func_08B173EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4] + 0x18D4u; G[16] = G[4]; for (;;) { G[4] = G[17]; call_here(rt, ctx, 0x08B28C44u); G[2] = load8(G[16] + 0x19DCu); G[4] = G[16]; if (G[2] == 0u) break; call_here(rt, ctx, 0x08B17240u); G[4] = G[16] + 0x1900u; call_here(rt, ctx, 0x08B28C74u); } } G[16] = s0; G[17] = s1; }
void func_08B194E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[7]; G[5] = G[7]; G[16] = G[4]; G[7] = G[8]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x14u); call_here(rt, ctx, G[3]); } else { put(rt, G[4], 0u); put(rt, G[4] + 4u, 0u); store8(G[4] + 8u, 0u); } G[2] = G[16]; } G[16] = s0; }
void func_08B19538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5]; G[8] = G[7]; G[5] = word(rt, G[5]); G[16] = G[4]; bool zero = true; if (G[5] != 0u) { G[7] = G[6]; if (G[6] != 0u) { G[6] = word(rt, G[2] + 4u); call_here(rt, ctx, 0x08B194E0u); zero = false; } } if (zero) { put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); store8(G[16] + 8u, 0u); } G[2] = G[16]; } G[16] = s0; }
void func_08B1C08C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B1C03Cu, {ctx.gpr[4], 0xFFFFFFFFu});
}
void func_08B1C2B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08B1C264u, {ctx.gpr[4], 0xFFFFFFFFu});
}
void func_08B1E220(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = high_half(0x08D773E8u); G[16] = word(rt, 0x08D773E8u); G[17] = G[4] & 0xFFu; if (G[16] != 0u) { G[4] = word(rt, G[16] + 4u); for (;;) { G[2] = load8(G[4] + 0x10u); if (G[2] == G[17]) call_here(rt, ctx, 0x08B1E14Cu); G[16] = word(rt, G[16]); if (G[16] == 0u) break; G[4] = word(rt, G[16] + 4u); } } } G[16] = s0; G[17] = s1; }
void func_08B1E9F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[2] = word(rt, G[4] + 8u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08B61FC0u); G[2] = word(rt, G[16]); G[4] = G[2]; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[25] = word(rt, G[2] + 4u); tail = true; } } G[16] = s0; if (tail) jump(ctx, G[25]); }
void func_08B1F6D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4] + 8u; G[19] = G[6]; G[18] = G[5]; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34D40u); G[4] = G[17]; put(rt, G[16], G[18]); put(rt, G[16] + 4u, G[19]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B34D80u); }
void func_08B22414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD9538u; G[16] = G[4]; G[5] = G[16]; G[3] = word(rt, G[4] + 0x30u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) call_here(rt, ctx, 0x08B2235Cu); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08C4FB20u); G[4] = G[16] + 4u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B27E80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0xFFFF0000u; G[7] = G[4] >> 16; G[10] = 0xFFFF000Fu; G[4] &= 0xFFFFu; G[8] = 0u; for (;;) { const bool more = G[6] != G[8]; G[9] = 0xFFF0u; if (!more) break; G[2] = load8(G[5]); G[8] += 1u; G[5] += 1u; G[4] += G[2]; G[3] = G[9] < G[4] ? 1u : 0u; G[2] = G[4] + G[10]; if (G[3] != 0u) G[4] = G[2]; G[7] += G[4]; G[3] = G[9] < G[7] ? 1u : 0u; G[2] = G[7] + G[10]; if (G[3] != 0u) G[7] = G[2]; } G[2] = (G[7] << 16) | G[4]; }
void func_08B48748(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDAB78u; G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CDAC38u; put(rt, G[16] + 0x2Cu, G[2]); G[4] += 0x2Cu; call_here(rt, ctx, 0x08B1FD74u); G[2] = 0x08CDAC48u; G[4] = G[16] + 0xCu; put(rt, G[16] + 0xCu, G[2]); call_here(rt, ctx, 0x08B1FD74u); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08B49150(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[4] = word(rt, G[4]); G[2] = 1u; G[1] = (G[4] == 1u) ? 0x3A22E4C7u : 0x3A83126Fu; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[0] = psp_mul(ctx.fpr[12], ctx.fpr[0]); }
void func_08B5D2BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[5] = 0xFFFFFFFFu; G[16] = word(rt, G[4]); G[4] = G[16]; call_here(rt, ctx, 0x08B60804u); G[3] = 1u; if (G[2] == G[3] && G[16] != 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08B5CC1Cu); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08B64654u); }
void func_08B60AF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[2] = 0x08CDB800u; G[4] = word(rt, G[4] + 8u); put(rt, G[17], G[2]); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = G[17]; call_here(rt, ctx, 0x08B5C280u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08B60C2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[2] = 0x08CDB838u; G[4] = word(rt, G[4] + 8u); put(rt, G[17], G[2]); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = G[17]; call_here(rt, ctx, 0x08B5C280u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08B60FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[2] = 0x08CDB798u; G[4] = word(rt, G[4] + 8u); put(rt, G[17], G[2]); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 4u); call_here(rt, ctx, G[2]); } G[4] = G[17]; call_here(rt, ctx, 0x08B5C280u); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08B63648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    read_varint(rt, ctx);
}
void func_08B727D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    read_varint(rt, ctx);
}
void func_08B75D48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    read_varint(rt, ctx);
}
void func_08B65608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[4] = 0x10u; G[18] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B62344u); G[3] = word(rt, G[17] + 0x18u); put(rt, G[2] + 4u, G[16]); put(rt, G[2], G[3]); put(rt, G[2] + 8u, G[18]); put(rt, G[17] + 0x18u, G[2]); put(rt, G[2] + 0xCu, 0u); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B78360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = 0x08B7B898u; put(rt, G[4] + 0x2Cu, G[2]); G[2] = 0x08B7B6F0u; put(rt, G[4] + 0x20u, G[2]); G[3] = 0x08B7B780u; G[2] = 0x08B7B804u; store16(G[4] + 0xCu, G[5]); store16(G[4] + 0xEu, G[6]); put(rt, G[4] + 0x24u, G[3]); put(rt, G[4] + 0x28u, G[2]); put(rt, G[4] + 0x0u, 0u); put(rt, G[4] + 0x4u, 0u); put(rt, G[4] + 0x8u, 0u); put(rt, G[4] + 0x10u, 0u); put(rt, G[4] + 0x14u, 0u); put(rt, G[4] + 0x18u, 0u); put(rt, G[4] + 0x1Cu, G[4]); }
void func_08B83E60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = high_half(0x08D740F8u); G[9] = word(rt, 0x08D740F8u); { GuestFrame frame(ctx, 0x10u); G[3] = 0x80420000u; G[7] = G[4]; G[8] = G[4] < 0x20u ? 1u : 0u; G[6] = G[5]; G[2] = 0x80420100u; if (G[9] != 0u) { G[5] = high_half(0x08D74100u); G[2] = 1u; G[4] = 0x08D74100u; G[5] = G[2] << (G[7] & 31u); G[7] = 0x80420000u; G[2] = 0x80420010u; if (G[8] != 0u) call_here(rt, ctx, 0x08C7B080u); } } }
void func_08B877D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08C97FCCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x18u, G[2]); put(rt, G[17] + 0x1Cu, G[3]); } G[16] = s0; G[17] = s1; }
void func_08B8897C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08C9832Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x40u); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x18u, G[2]); put(rt, G[17] + 0x1Cu, G[3]); } G[16] = s0; G[17] = s1; }
void func_08B88AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08C98418u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x30u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[2] = G[2] != 0u ? 1u : 0u; store8(G[17] + 0x14u, G[2]); } G[16] = s0; G[17] = s1; }
void func_08B8E080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBEDF8u; G[16] = G[4]; G[3] = word(rt, G[4] + 0x38u); put(rt, G[16], G[2]); G[4] = G[3]; if (G[3] != 0u) { G[2] = word(rt, G[3]); G[3] = word(rt, G[2] + 4u); call_here(rt, ctx, G[3]); } G[2] = 0x08CB7440u; put(rt, G[16], G[2]); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08A7D4E4u); }
void func_08B975B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = G[4]; G[16] = word(rt, G[4] + 8u); G[4] = G[16]; if (G[16] != 0u) { G[5] = word(rt, G[2] + 0x10u); call_here(rt, ctx, 0x08AF771Cu); G[4] = G[16]; call_here(rt, ctx, 0x08AF6D14u); G[4] = G[16]; G[5] = 0xFFFFFFFFu; tail = G[2] == 0u; } } G[16] = s0; if (tail) jump(ctx, 0x08AF7B0Cu); }
void func_08BB12AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    u32 position;
    {
        GuestFrame frame(ctx, 0x10u);
        position = locate_position(rt, ctx, 0x08BB1180u);
    }
    if (second != position) tail_call(ctx, 0x08BB0DF4u, {position + 4u, second, position, third});
}
void func_08BB8A64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    u32 position;
    {
        GuestFrame frame(ctx, 0x10u);
        position = locate_position(rt, ctx, 0x08BB890Cu);
    }
    if (second != position) tail_call(ctx, 0x08BB8A38u, {position + 2u, second, position, third});
}
void func_08BBD1B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = G[4]; G[4] = word(rt, G[5]); G[3] = word(rt, G[6] + 8u); G[5] = word(rt, G[5] + 4u); G[2] = G[4] + 4u; if (G[4] == 0u) G[2] = 0u; G[3] -= 1u; G[4] = word(rt, G[2]); put(rt, G[6] + 8u, G[3]); if (G[5] != 0u) { put(rt, G[5] + 4u, G[4]); if (G[4] == 0u) put(rt, G[6] + 4u, G[5]); } else { put(rt, G[6], G[4]); if (G[4] == 0u) put(rt, G[6] + 4u, 0u); } G[2] = G[4]; G[3] = G[5]; }
void func_08BC4FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = 8u; call_here(rt, ctx, 0x08B62344u); G[5] = G[2]; put(rt, G[2], 0u); G[4] = G[16]; G[6] = 0u; put(rt, G[2] + 4u, 0u); put(rt, G[16], 0u); put(rt, G[16] + 4u, 0u); store8(G[16] + 8u, 0u); call_here(rt, ctx, 0x08BC4F64u); G[2] = 1u; store8(G[16] + 8u, G[2]); G[2] = G[16]; } G[16] = s0; }
void func_08BD2E04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    dispatch_event(rt, ctx, word(rt, ctx.gpr[5]));
}
void func_08BD2E5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    dispatch_event(rt, ctx, ctx.gpr[5]);
}
void func_08BF0E34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = G[4]; if (G[4] != G[5]) { G[9] = 0xFFFFFFFFu; for (;;) { G[3] = G[4]; G[2] = G[6]; G[7] = 3u; do { ctx.fpr[0] = float_at(rt, G[2]); G[7] -= 1u; put_float(rt, G[3], ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[2] + 4u); G[2] += 8u; put_float(rt, G[3] + 4u, ctx.fpr[1]); G[3] += 8u; } while (G[7] != G[9]); ctx.fpr[0] = float_at(rt, G[6] + 0x20u); G[8] += 0x24u; put_float(rt, G[4] + 0x20u, ctx.fpr[0]); if (G[8] == G[5]) break; G[4] = G[8]; } } }
void func_08BF0F5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = G[6]; if (G[4] != G[5]) { do { if (G[2] == 0u) G[4] += 0x24u; else { G[6] = G[2]; G[3] = G[4]; G[7] = 4u; do { ctx.fpr[0] = float_at(rt, G[3]); G[7] -= 1u; put_float(rt, G[6], ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[3] + 4u); G[3] += 8u; put_float(rt, G[6] + 4u, ctx.fpr[1]); G[6] += 8u; } while (G[7] != 0u); ctx.fpr[0] = float_at(rt, G[4] + 0x20u); put_float(rt, G[2] + 0x20u, ctx.fpr[0]); G[4] += 0x24u; } G[2] += 0x24u; } while (G[5] != G[4]); } }
void func_08BF0FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = 0u; if (G[5] != 0u) { do { if (G[4] == 0u) G[8] += 1u; else { G[3] = G[4]; G[2] = G[6]; G[7] = 4u; do { ctx.fpr[0] = float_at(rt, G[2]); G[7] -= 1u; put_float(rt, G[3], ctx.fpr[0]); ctx.fpr[1] = float_at(rt, G[2] + 4u); G[2] += 8u; put_float(rt, G[3] + 4u, ctx.fpr[1]); G[3] += 8u; } while (G[7] != 0u); ctx.fpr[0] = float_at(rt, G[6] + 0x20u); put_float(rt, G[4] + 0x20u, ctx.fpr[0]); G[8] += 1u; } G[4] += 0x24u; } while (G[5] != G[8]); } }
void func_08C1FF18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[16] = word(rt, G[5]); G[17] = G[4]; G[4] = 0x08CB0284u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[5] = word(rt, G[2] + 0x58u); G[4] = G[16]; call_here(rt, ctx, G[5]); put(rt, G[17] + 0x18u, G[2]); put(rt, G[17] + 0x1Cu, G[3]); } G[16] = s0; G[17] = s1; }
void func_08C281AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD4108u; G[16] = G[4]; G[4] += 0xCu; put(rt, G[16], G[2]); G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C00860u); G[4] = G[16] + 8u; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CD1F30u; G[4] = G[16] + 4u; put(rt, G[16], G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; } G[16] = s0; jump(ctx, 0x08B64654u); }
void func_08C37B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = word(rt, G[4] + 0x1Cu); { GuestFrame frame(ctx, 0x20u); G[6] = 0u; G[9] = 3u; G[8] = 4u; do { G[2] = G[9] - G[6]; if (G[7] == 0u) G[2] = G[6]; G[2] <<= 3; G[3] = G[6] + frame.at(0u); G[2] = sra(G[5], G[2] & 31u); G[6] += 1u; store8(G[3], G[2]); } while (G[6] != G[8]); G[3] = word(rt, G[4]); G[5] = frame.at(0u); G[2] = word(rt, G[3] + 0x108u); G[6] = 4u; call_here(rt, ctx, G[2]); } }
void func_08C37CC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[7] = word(rt, G[4] + 0x1Cu); { GuestFrame frame(ctx, 0x20u); G[6] = 0u; G[9] = 3u; G[8] = 4u; do { G[2] = G[9] - G[6]; if (G[7] == 0u) G[2] = G[6]; G[2] <<= 3; G[3] = G[6] + frame.at(0u); G[2] = G[5] >> (G[2] & 31u); G[6] += 1u; store8(G[3], G[2]); } while (G[6] != G[8]); G[3] = word(rt, G[4]); G[5] = frame.at(0u); G[2] = word(rt, G[3] + 0x108u); G[6] = 4u; call_here(rt, ctx, G[2]); } }
void func_08C4AF5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[11] = G[5]; G[9] = G[6]; if (G[5] != G[4]) { G[8] = G[4]; G[10] = G[4]; do { G[3] = word(rt, G[9]); G[4] = word(rt, G[9] + 4u); G[5] = word(rt, G[9] + 8u); G[6] = word(rt, G[9] + 0xCu); G[7] = word(rt, G[9] + 0x10u); G[2] = word(rt, G[9] + 0x14u); put(rt, G[8], G[3]); put(rt, G[8] + 0x14u, G[2]); put(rt, G[8] + 4u, G[4]); put(rt, G[8] + 8u, G[5]); put(rt, G[8] + 0xCu, G[6]); put(rt, G[8] + 0x10u, G[7]); G[8] = G[10] + 0x18u; G[10] = G[8]; } while (G[11] != G[8]); } }
void func_08C4B078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[11] = G[5]; if (G[5] != G[4]) { G[10] = G[4]; G[9] = G[6] + 0x70u; do { G[8] = G[4]; G[7] = G[6]; do { G[2] = word(rt, G[7]); G[3] = word(rt, G[7] + 4u); G[4] = word(rt, G[7] + 8u); G[5] = word(rt, G[7] + 0xCu); put(rt, G[8], G[2]); G[7] += 0x10u; G[8] += 0x10u; put(rt, G[8] - 0xCu, G[3]); put(rt, G[8] - 8u, G[4]); put(rt, G[8] - 4u, G[5]); } while (G[7] != G[9]); G[4] = G[10] + 0x70u; G[10] = G[4]; } while (G[11] != G[4]); } }
void func_08C53F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDC298u; G[17] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08C78130u); G[4] = G[17] + 0x24u; call_here(rt, ctx, 0x08B61708u); G[2] = 0x08CDBDF0u; G[4] = G[17] + 0x1Cu; put(rt, G[17], G[2]); call_here(rt, ctx, 0x08B5D34Cu); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08C7863C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CDC298u; G[17] = G[4]; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08C78130u); G[4] = G[17] + 0x24u; call_here(rt, ctx, 0x08B61708u); G[2] = 0x08CDBDF0u; G[4] = G[17] + 0x1Cu; put(rt, G[17], G[2]); call_here(rt, ctx, 0x08B5D34Cu); G[4] = G[17]; } G[16] = s0; G[17] = s1; jump(ctx, 0x08B64654u); }
void func_08C59328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[6] &= 0xFFu; G[8] = sext(G[8] & 0xFFu, 8u); G[9] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[12] = word(rt, G[5]); G[2] = word(rt, G[9] + 0x8u); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[12]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[6] &= 0xFFu; G[8] = sext(G[8] & 0xFFu, 8u); G[10] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[11] = word(rt, G[5]); G[2] = word(rt, G[10] + 0xCu); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[11]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C59B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); G[7] = sext(G[7] & 0xFFu, 8u); G[8] &= 0xFFu; G[9] = word(rt, G[4]); G[3] = load8(G[5] + 4u); G[10] = word(rt, G[5]); G[2] = word(rt, G[9] + 0x8u); G[5] = frame.at(8u); store8(frame.at(0xCu), G[3]); put(rt, frame.at(8u), G[10]); call_here(rt, ctx, G[2]); G[4] = G[2]; put(rt, frame.at(0x14u), G[3]); G[2] = load8(frame.at(0x14u)); store8(frame.at(4u), G[2]); G[2] = G[4]; G[5] = word(rt, frame.at(4u)); } G[3] = G[5]; }
void func_08C750C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[2] = word(rt, G[4]); const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[6] = sext(G[6] & 0xFFu, 8u); G[16] = G[5] - G[2]; G[8] = G[6]; G[17] = G[4]; G[5] = G[16]; G[6] = 0u; G[7] = 1u; call_here(rt, ctx, 0x08C74FACu); G[3] = 0xFFFFFFFFu; G[2] = word(rt, G[17]); put(rt, G[2] - 4u, G[3]); G[2] += G[16]; } G[16] = s0; G[17] = s1; }
void func_08C755A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[5] = word(rt, G[5]); G[2] = word(rt, G[5] - 4u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08C75524u); G[5] = word(rt, G[16]); } G[3] = word(rt, G[5] - 0xCu); G[2] = G[17]; G[3] = G[5] + G[3]; put(rt, G[17], G[3]); } G[16] = s0; G[17] = s1; }
void func_08C76AA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = load8(G[4] + 0x80u); G[16] = G[4]; if (G[2] == 0u) { G[2] = word(rt, G[4] + 0x78u); if (G[2] == 0u) { G[4] = word(rt, G[4] + 0x7Cu); call_here(rt, ctx, 0x08B6461Cu); G[3] = 1u; store8(G[16] + 0x80u, G[3]); put(rt, G[16] + 0x78u, G[2]); } } } G[16] = s0; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_187
} // namespace gt::game

#define HOT187(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_187::name, gt::ReturnKind::kind)
HOT187(0x08830BA4u, func_08830BA4, Word);
HOT187(0x0886E088u, func_0886E088, Word);
HOT187(0x0887552Cu, func_0887552C, Word);
HOT187(0x08884FF4u, func_08884FF4, Word);
HOT187(0x08899D84u, func_08899D84, Word);
HOT187(0x088A579Cu, func_088A579C, Word);
HOT187(0x088B2714u, func_088B2714, Word);
HOT187(0x088C56A8u, func_088C56A8, Word);
HOT187(0x088C63FCu, func_088C63FC, Word);
HOT187(0x088C7844u, func_088C7844, Word);
HOT187(0x088D76ECu, func_088D76EC, Word);
HOT187(0x088DA394u, func_088DA394, DoubleWord);
// HOT187(0x088E4464u, func_088E4464, Word);  // no comparable synthetic case yet
// HOT187(0x088ED0D4u, func_088ED0D4, Word);  // no comparable synthetic case yet
HOT187(0x0891A608u, func_0891A608, Word);
HOT187(0x0891A774u, func_0891A774, Word);
HOT187(0x08927904u, func_08927904, Word);
HOT187(0x0892795Cu, func_0892795C, Word);
HOT187(0x0892B778u, func_0892B778, Word);
HOT187(0x08938684u, func_08938684, Word);
// HOT187(0x0897851Cu, func_0897851C, Word);  // no comparable synthetic case yet
// HOT187(0x0897E884u, func_0897E884, Word);  // no comparable synthetic case yet
// HOT187(0x0898970Cu, func_0898970C, Word);  // no comparable synthetic case yet
HOT187(0x08992FC0u, func_08992FC0, Word);
HOT187(0x089A324Cu, func_089A324C, Word);
HOT187(0x089A6FCCu, func_089A6FCC, Word);
HOT187(0x089A7024u, func_089A7024, Word);
HOT187(0x089A707Cu, func_089A707C, Word);
HOT187(0x089A758Cu, func_089A758C, Word);
// HOT187(0x089AB444u, func_089AB444, Word);  // no comparable synthetic case yet
// HOT187(0x089AB49Cu, func_089AB49C, Word);  // no comparable synthetic case yet
// HOT187(0x089AB4F4u, func_089AB4F4, Word);  // no comparable synthetic case yet
HOT187(0x089AF91Cu, func_089AF91C, Word);
HOT187(0x089B5E18u, func_089B5E18, Word);
HOT187(0x089BB504u, func_089BB504, Word);
HOT187(0x089BFC50u, func_089BFC50, Word);
HOT187(0x089BFE58u, func_089BFE58, Word);
HOT187(0x089CD344u, func_089CD344, Word);
HOT187(0x089CD49Cu, func_089CD49C, Word);
HOT187(0x089D7580u, func_089D7580, Word);
HOT187(0x089E04F8u, func_089E04F8, Word);
HOT187(0x089E05C8u, func_089E05C8, Word);
HOT187(0x08A07468u, func_08A07468, Word);
// HOT187(0x08A0F800u, func_08A0F800, Word);  // no comparable synthetic case yet
HOT187(0x089E5D98u, func_089E5D98, Word);
HOT187(0x08A0FAA8u, func_08A0FAA8, Word);
HOT187(0x08A0FB00u, func_08A0FB00, Word);
HOT187(0x08A0FB58u, func_08A0FB58, Word);
HOT187(0x08A0FBB0u, func_08A0FBB0, Word);
HOT187(0x08A106C0u, func_08A106C0, Word);
HOT187(0x08A10770u, func_08A10770, Word);
HOT187(0x08A107C8u, func_08A107C8, Word);
// HOT187(0x08A26ED8u, func_08A26ED8, Word);  // no comparable synthetic case yet
HOT187(0x08A2AAE0u, func_08A2AAE0, Word);
HOT187(0x08A2F800u, func_08A2F800, Word);
HOT187(0x08A308C0u, func_08A308C0, Word);
HOT187(0x08A401ECu, func_08A401EC, Word);
HOT187(0x08A550E8u, func_08A550E8, Word);
HOT187(0x08A5E070u, func_08A5E070, Word);
HOT187(0x08A832E8u, func_08A832E8, Word);
HOT187(0x08A5EACCu, func_08A5EACC, Word);
HOT187(0x08A83C70u, func_08A83C70, Word);
HOT187(0x08A70B1Cu, func_08A70B1C, Word);
HOT187(0x08A717ECu, func_08A717EC, Word);
HOT187(0x08A726B0u, func_08A726B0, Word);
HOT187(0x08A73D2Cu, func_08A73D2C, Word);
HOT187(0x08A73D84u, func_08A73D84, Word);
HOT187(0x08A7D53Cu, func_08A7D53C, Word);
HOT187(0x08A888F0u, func_08A888F0, Word);
HOT187(0x08A88948u, func_08A88948, Word);
HOT187(0x08A8C5D0u, func_08A8C5D0, Word);
// HOT187(0x08A94B90u, func_08A94B90, Word);  // no comparable synthetic case yet
HOT187(0x08A96AA0u, func_08A96AA0, Word);
HOT187(0x08A96AF8u, func_08A96AF8, Word);
HOT187(0x08AB0C0Cu, func_08AB0C0C, Word);
HOT187(0x08AB0C64u, func_08AB0C64, Word);
HOT187(0x08AB23B8u, func_08AB23B8, Word);
HOT187(0x08AB2410u, func_08AB2410, Word);
HOT187(0x08A9E880u, func_08A9E880, Word);
HOT187(0x08AA8FD0u, func_08AA8FD0, Word);
// HOT187(0x08AA91A0u, func_08AA91A0, Word);  // no comparable synthetic case yet
HOT187(0x08AAF838u, func_08AAF838, Word);
HOT187(0x08ABAB44u, func_08ABAB44, Word);
HOT187(0x08ABBBB8u, func_08ABBBB8, Word);
HOT187(0x08ABFA60u, func_08ABFA60, Word);
HOT187(0x08AC2A8Cu, func_08AC2A8C, Word);
HOT187(0x08ACC2BCu, func_08ACC2BC, Word);
HOT187(0x08AD1040u, func_08AD1040, Word);
HOT187(0x08AE3CD8u, func_08AE3CD8, Word);
HOT187(0x08AE493Cu, func_08AE493C, Word);
HOT187(0x08AE5B50u, func_08AE5B50, Word);
HOT187(0x08AE75C0u, func_08AE75C0, Word);
HOT187(0x08AEBCB8u, func_08AEBCB8, Word);
HOT187(0x08AEF8E4u, func_08AEF8E4, Word);
HOT187(0x08AEF958u, func_08AEF958, Float);
HOT187(0x08AEFCF0u, func_08AEFCF0, Word);
HOT187(0x08AF826Cu, func_08AF826C, Word);
HOT187(0x08AF8C54u, func_08AF8C54, Word);
HOT187(0x08B0157Cu, func_08B0157C, Word);
HOT187(0x08B08450u, func_08B08450, Word);
HOT187(0x08B08544u, func_08B08544, Word);
HOT187(0x08B0859Cu, func_08B0859C, Word);
HOT187(0x08B085F4u, func_08B085F4, Word);
HOT187(0x08B086A4u, func_08B086A4, Word);
HOT187(0x08B097ACu, func_08B097AC, Word);
HOT187(0x08B0A744u, func_08B0A744, DoubleWord);
HOT187(0x08B0D4B8u, func_08B0D4B8, Word);
HOT187(0x08B0D958u, func_08B0D958, Word);
HOT187(0x08B0D9B0u, func_08B0D9B0, Word);
HOT187(0x08B0DA08u, func_08B0DA08, Word);
// HOT187(0x08B0DA60u, func_08B0DA60, Word);  // no comparable synthetic case yet
HOT187(0x08B153B0u, func_08B153B0, Word);
// HOT187(0x08B173ECu, func_08B173EC, Word);  // no comparable synthetic case yet
HOT187(0x08B194E0u, func_08B194E0, Word);
HOT187(0x08B19538u, func_08B19538, Word);
HOT187(0x08B1C08Cu, func_08B1C08C, Word);
HOT187(0x08B1C2B4u, func_08B1C2B4, Word);
HOT187(0x08B1E220u, func_08B1E220, Word);
HOT187(0x08B1E9F0u, func_08B1E9F0, Word);
HOT187(0x08B1F6D8u, func_08B1F6D8, Word);
HOT187(0x08B22414u, func_08B22414, Word);
HOT187(0x08B27E80u, func_08B27E80, Word);
HOT187(0x08B48748u, func_08B48748, Word);
HOT187(0x08B49150u, func_08B49150, Float);
HOT187(0x08B5D2BCu, func_08B5D2BC, Word);
HOT187(0x08B60AF8u, func_08B60AF8, Word);
HOT187(0x08B60C2Cu, func_08B60C2C, Word);
HOT187(0x08B60FA4u, func_08B60FA4, Word);
HOT187(0x08B63648u, func_08B63648, Word);
HOT187(0x08B727D8u, func_08B727D8, Word);
HOT187(0x08B75D48u, func_08B75D48, Word);
HOT187(0x08B65608u, func_08B65608, Word);
HOT187(0x08B78360u, func_08B78360, Word);
HOT187(0x08B83E60u, func_08B83E60, Word);
// HOT187(0x08B877D4u, func_08B877D4, Word);  // no comparable synthetic case yet
// HOT187(0x08B8897Cu, func_08B8897C, Word);  // no comparable synthetic case yet
// HOT187(0x08B88AB0u, func_08B88AB0, Word);  // no comparable synthetic case yet
HOT187(0x08B8E080u, func_08B8E080, Word);
HOT187(0x08B975B8u, func_08B975B8, Word);
HOT187(0x08BB12ACu, func_08BB12AC, Word);
HOT187(0x08BB8A64u, func_08BB8A64, Word);
HOT187(0x08BBD1B8u, func_08BBD1B8, DoubleWord);
HOT187(0x08BC4FB8u, func_08BC4FB8, Word);
HOT187(0x08BD2E04u, func_08BD2E04, Word);
HOT187(0x08BD2E5Cu, func_08BD2E5C, Word);
HOT187(0x08BF0E34u, func_08BF0E34, Word);
HOT187(0x08BF0F5Cu, func_08BF0F5C, Word);
HOT187(0x08BF0FB4u, func_08BF0FB4, Word);
// HOT187(0x08C1FF18u, func_08C1FF18, Word);  // no comparable synthetic case yet
HOT187(0x08C281ACu, func_08C281AC, Word);
HOT187(0x08C37B5Cu, func_08C37B5C, Word);
// HOT187(0x08C37CC0u, func_08C37CC0, Word);  // no comparable synthetic case yet
HOT187(0x08C4AF5Cu, func_08C4AF5C, Word);
// HOT187(0x08C4B078u, func_08C4B078, Word);  // no comparable synthetic case yet
HOT187(0x08C53F94u, func_08C53F94, Word);
HOT187(0x08C7863Cu, func_08C7863C, Word);
HOT187(0x08C59328u, func_08C59328, DoubleWord);
HOT187(0x08C59380u, func_08C59380, DoubleWord);
HOT187(0x08C59B90u, func_08C59B90, DoubleWord);
// HOT187(0x08C750C0u, func_08C750C0, Word);  // no comparable synthetic case yet
HOT187(0x08C755A0u, func_08C755A0, Word);
HOT187(0x08C76AA4u, func_08C76AA4, Word);
