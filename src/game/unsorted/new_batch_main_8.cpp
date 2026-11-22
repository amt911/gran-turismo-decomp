// New batch (main session, 8): a draft of the functions in this batch's disassembly, written as structured C from
// `tools.disasm` and the style of new_batch_4.cpp. Nothing here is verified yet: the synthetic cases and the differential
// gate come later. Functions whose asm was not fully understood are left out and listed in the batch report.
//
// The batch has several repeated families:
//  - five "insert-bits range" helpers: they mask the low one or two bits of (end - start), call one segment move with
//    (start, end, this->4 - end), call a second move with ((this->4 - end) + start, this->4, sp), then subtract the masked
//    size from this->4 and return start;
//  - a 0x28-byte element copy loop that stores a vtable word and moves two words and six floats per element;
//  - four Adhoc value comparison helpers: they read the holder at a2[0], call a virtual slot on it, convert the result and
//    build an Adhoc long/day object into a stack holder, then reset/destroy the holder.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_8 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's jal.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A guest call whose v0/v1/f0 stay in the registers like the original's. A caller that reads v1 after a callee that returns a
// 64-bit value depends on this (the comparison helpers pass the high word to new_long_object).
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// The insert-bits range helper family. `mask_bits` is the `ins $s0, $zero, 0, N` width: the low N bits of end - start are
// cleared. `move` is the second segment move (func_08B8A7C0 / func_08BB1314 / func_08BBB8F8 / func_08C46F94 / func_08C46FD4).
u32 insert_range_move(Runtime &rt, AllegrexContext &ctx, u32 move, unsigned mask_bits) {
    const u32 self = ctx.gpr[4];
    const u32 start = ctx.gpr[5];
    const u32 end = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 masked = (end - start) & ~((1u << mask_bits) - 1u);
    const u32 tail = word(rt, self + 4u) - end;
    call(rt, ctx, 0x08B79BACu, {start, end, tail});
    call(rt, ctx, move, {tail + start, word(rt, self + 4u), ctx.gpr[29]});
    put(rt, self + 4u, word(rt, self + 4u) - masked);
    ctx.gpr[2] = start;
    return start;
}

// A linear copy of 0x28-byte elements from a0 (up to a1) into a2, with the vtable word 0x08CCE6E0 stored at the destination.
void copy_elements(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 out = ctx.gpr[6];
    if (begin == end) {
        ctx.gpr[2] = out;
        return;
    }
    u32 at = begin;
    for (;;) {
        if (out != 0u) {
            put(rt, out + 0x4u, word(rt, at + 0x4u));
            put(rt, out + 0x8u, word(rt, at + 0x8u));
            put_float(rt, out + 0xCu, float_at(rt, at + 0xCu));
            put_float(rt, out + 0x10u, float_at(rt, at + 0x10u));
            put_float(rt, out + 0x14u, float_at(rt, at + 0x14u));
            put_float(rt, out + 0x18u, float_at(rt, at + 0x18u));
            put_float(rt, out + 0x1Cu, float_at(rt, at + 0x1Cu));
            put(rt, out, 0x08CCE6E0u);
            put_float(rt, out + 0x20u, float_at(rt, at + 0x20u));
            put_float(rt, out + 0x24u, float_at(rt, at + 0x24u));
        }
        const u32 next = at + 0x28u;
        if (next == end) {
            ctx.gpr[2] = out;
            return;
        }
        at = next;
        out += 0x28u;
    }
}

// An Adhoc value comparison helper that builds a long value: vtable slot `slot` on a2[0], convert via a 64-bit helper, then
// new_long_object(sp, count, low, high); the caller's holder is reset from the stack holder and the stack holder destroyed.
void compare_emit_long(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 convert) {
    if (static_cast<s32>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, ctx.gpr[6]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    const u32 table = word(rt, object);
    const u32 raw = call_indirect(rt, ctx, word(rt, table + slot), {object});
    const GuestResult value = call_full(rt, ctx, convert, {raw});
    call(rt, ctx, 0x08A5DEE8u, {frame.at(0u), ctx.gpr[5], value.v0, value.v1});
    if (self != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// The same shape, but the converted value is a day (a single word) for new_day_object(sp, day).
void compare_emit_day(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 convert) {
    if (static_cast<s32>(ctx.gpr[5]) <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, ctx.gpr[6]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    const u32 table = word(rt, object);
    const u32 raw = call_indirect(rt, ctx, word(rt, table + slot), {object});
    const u32 day = call(rt, ctx, convert, {raw, table});
    call(rt, ctx, 0x08A5FA8Cu, {frame.at(0u), day});
    if (self != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// The field kinds the comparison helpers load from `this + 0x14`.
enum class FieldKind { Byte, UnsignedByte, Half, UnsignedHalf, Word };
// What the helper compares the virtual call's result with, or that it multiplies the two.
enum class CompareOp { Equal, NotEqual, FieldAtLeast, ResultAtLeast, Product };

u32 load_field(Runtime &rt, u32 address, FieldKind kind) {
    switch (kind) {
    case FieldKind::Byte: return static_cast<u32>(static_cast<std::int8_t>(rt.memory().load8(address)));
    case FieldKind::UnsignedByte: return rt.memory().load8(address);
    case FieldKind::Half: return static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(address)));
    case FieldKind::UnsignedHalf: return rt.memory().load16(address);
    case FieldKind::Word: return word(rt, address);
    }
    return 0u;
}
bool field_is_signed(FieldKind kind) { return kind == FieldKind::Byte || kind == FieldKind::Half; }

// The Adhoc comparison family: `this + 0x14` holds the left operand (signed byte/half for lb/lh, unsigned for lbu/lhu, a word
// for lw); a3 is a holder whose word 0 is the right operand; its vtable slot is called with (object). The two are compared
// (xor ==/!= 0, `>=` in the field's own signedness, or multiplied), an Adhoc bool/int/day object is built into a stack holder,
// and the caller's holder (a1) is reset from it. `emit` is one of new_bool_object / new_integer_object / new_day_object.
void compare_field(Runtime &rt, AllegrexContext &ctx, u32 slot, FieldKind kind, CompareOp op, u32 message, u32 emit) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const u32 field = load_field(rt, self + 0x14u, kind);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 table = word(rt, object);
    const u32 result = call_indirect(rt, ctx, word(rt, table + slot), {object});
    u32 argument = 0u;
    switch (op) {
    case CompareOp::Equal: argument = (field ^ result) == 0u ? 1u : 0u; break;
    case CompareOp::NotEqual: argument = (field ^ result) != 0u ? 1u : 0u; break;
    case CompareOp::FieldAtLeast:
        argument = field_is_signed(kind) ? (static_cast<s32>(field) >= static_cast<s32>(result) ? 1u : 0u)
                                         : (field >= result ? 1u : 0u);
        break;
    case CompareOp::ResultAtLeast:
        argument = field_is_signed(kind) ? (static_cast<s32>(result) >= static_cast<s32>(field) ? 1u : 0u)
                                         : (result >= field ? 1u : 0u);
        break;
    case CompareOp::Product: argument = static_cast<u32>(static_cast<s32>(field) * static_cast<s32>(result)); break;
    }
    call(rt, ctx, emit, {frame.at(0u), argument});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// A virtual-call forwarder: a0 = the result object, a1 = the argument, a2 = the holder whose word 0 is the callee object. It
// calls the object's vtable entry 0x4C with (object), then calls a1[0]'s vtable entry 0x84 with (result, argument, first result).
// v0 = the result object.



void func_0883DA48(Runtime &rt, AllegrexContext &ctx) { compare_emit_long(rt, ctx, 0x3Cu, 0x089300C0u); }
void func_0883E924(Runtime &rt, AllegrexContext &ctx) { compare_emit_long(rt, ctx, 0x3Cu, 0x089300B0u); }
void func_08840C7C(Runtime &rt, AllegrexContext &ctx) { compare_emit_day(rt, ctx, 0x50u, 0x089304C0u); }
void func_08840D00(Runtime &rt, AllegrexContext &ctx) { compare_emit_day(rt, ctx, 0x50u, 0x089304C8u); }


void func_08A96840(Runtime &rt, AllegrexContext &ctx) { compare_field(rt, ctx, 0x34u, FieldKind::Byte, CompareOp::FieldAtLeast, 0x08CB21E8u, 0x08A97A6Cu); }



void func_08AB2158(Runtime &rt, AllegrexContext &ctx) { compare_field(rt, ctx, 0x44u, FieldKind::UnsignedByte, CompareOp::FieldAtLeast, 0x08CB3AF0u, 0x08A97A6Cu); }


// Releases the four pointers at +4, +8, +0xC, +0x10 and clears the byte at +0. v0 ends as the last loaded pointer (or the
// release's result when it was non-null), as the original leaves it.
void func_0892C034(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    for (const u32 offset : {0x4u, 0x8u, 0xCu, 0x10u}) {
        const u32 pointer = word(rt, self + offset);
        ctx.gpr[2] = pointer;
        if (pointer != 0u) {
            call(rt, ctx, 0x08B61FC0u, {pointer});
            put(rt, self + offset, 0u);
        }
    }
    rt.memory().store8(self, 0u);
}

// Copies 0x34 bytes from a1 into this + 0x44, sets the byte at this + 0x78 and unlocks this->4 + 4.

// A derived constructor: two vtable stores, a base constructor at this + 4, an RTTI/offset word fix-up, then a base
// constructor at this + 0xA8 and a chain constructor.

// A derived destructor: vtable stores, a member release at this + 0xAC, then a chain destructor.

} // namespace
} // namespace new_batch_main_8
} // namespace gt::game

#define NEW8(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_8::name, gt::ReturnKind::kind)
NEW8(0x0883DA48u, func_0883DA48, Word);
NEW8(0x0883E924u, func_0883E924, Word);
NEW8(0x08840C7Cu, func_08840C7C, Word);
NEW8(0x08840D00u, func_08840D00, Word);
NEW8(0x0892C034u, func_0892C034, Word);
NEW8(0x08A96840u, func_08A96840, Word);
NEW8(0x08AB2158u, func_08AB2158, Word);
