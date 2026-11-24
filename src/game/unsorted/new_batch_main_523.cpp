// New batch, main session 523: 31 short functions whose old drafts were never verified because the gate could not
// build useful inputs for them. Written by hand as structured C from `tools.disasm`; the helpers hold the shapes they
// repeat: a global-list walk, a circular-list teardown, a scalar/pair written from a virtual getter, a scope opened on
// the frame and handed to an action, and a 0x70-byte block copier. v0 is the original's on every exit path (the `lui`
// half where an empty list leaves it, a callee's value otherwise); nothing keeps a value in an s register, so the
// callee-saved set is the same as the original's. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_523 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with v0 / v1 / f0 kept as the callee left them (a caller may read v1 as the high word of a pair).
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v0, (object); lw a1, slot(v0); jalr a1`: at the jalr v0 holds the vtable and a1 the target.
GuestResult call_virtual_in_a1(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot,
                               std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[5] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 signed_byte(u32 value) { return static_cast<u32>(static_cast<std::int8_t>(value)); }
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned position, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << position;
    return (target & ~mask) | ((value << position) & mask);
}

// A global list walk: v0 starts as the `lui` half of `head` (what an empty list leaves behind), then for each node the
// virtual at `slot` of the object the node's +4 word points to runs with that object in a0; the next node is word(node).
void walk_list(Runtime &rt, AllegrexContext &ctx, u32 head, u32 slot) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = high_half(head);
    for (u32 node = word(rt, head); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object});
    }
}

// A "remove every node of a circular list" loop: the head is word(this) and the sentinel is this itself; each node is
// released with 0x08A5BC54(node, 0xC, 4).
void clear_circular_list(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    u32 node = word(rt, self);
    if (node == self) return;
    for (;;) {
        const u32 next = word(rt, node);
        call(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
        node = next;
        if (self == next) return;
    }
}

// --- script attribute, scalar: a1 points at the object; it is null-checked (assert with `message`), its virtual getter
// at `slot` runs with the object in a0, and the answer is stored as a half or a byte at a0's + offset.
enum class Scalar { Half, Byte };
void store_scalar_from_virtual(Runtime &rt, AllegrexContext &ctx, u32 message, u32 slot, u32 offset, Scalar kind) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult result = call_virtual(rt, ctx, object, slot, {object});
    if (kind == Scalar::Half) put_half(rt, self + offset, result.v0);
    else put_byte(rt, self + offset, result.v0);
}
// The same, with the slot taken through a1 (`lw v0, (object); lw a1, slot(v0); jalr a1`) and both result words stored
// at +0x18 / +0x1C.
void store_pair_from_virtual_a1(Runtime &rt, AllegrexContext &ctx, u32 message, u32 slot) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult result = call_virtual_in_a1(rt, ctx, object, slot, {object});
    put(rt, self + 0x18u, result.v0);
    put(rt, self + 0x1Cu, result.v1);
}

// --- a lookup in a scope on the frame: `open` fills the frame slot, the object it produced is null-checked (assert with
// `message`), then either the object's two floats at +0xF0+0x40 / +0x44 are copied to +0xF0+0xBC / +0xC0, or its member
// at +0x14 and a no-argument getter's value go to a two-argument action; the scope is closed at the end.
void scope_copy_float_pair(Runtime &rt, AllegrexContext &ctx, u32 open, u32 message) {
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, open, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 base = object + 0xF0u;
    const float first = float_at(rt, base + 0x40u);
    const float second = float_at(rt, base + 0x44u);
    put_float(rt, base + 0xBCu, first);
    put_float(rt, base + 0xC0u, second);
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void scope_member_action(Runtime &rt, AllegrexContext &ctx, u32 open, u32 message, u32 getter, u32 action) {
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, open, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 member = word(rt, object + 0x14u);
    const u32 value = call(rt, ctx, getter, {message});
    call(rt, ctx, action, {member, value});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08B0D8C0: every node of the global list 0x08D7734C, its object at +4 asked for vtable slot 0xC.

// 0x08B0D90C: the same with slot 0x8.

// 0x08B17240 (a0 = object): 0x08B34888(0x08D61100), stores 0x08D51100 at +0x10, 0x08B181E0(object), then a tail call
// 0x08B34B24(0x08D61100).

// 0x08B1AB08: 0x08B344E0(0x08D689E4), sets the byte at 0x08D68A2D and, only when 0x08D68A2E was clear, calls
// 0x08B1A9E4; then a tail call 0x08B3457C(0x08D689E4).

// 0x08B2D78C (a0 = a handle): 0x08C7A828(a0, 0x03416102, {1}, 4, 0, 0); a negative answer stores 7 in 0x08D68CF8.

// 0x08B2D7D8: the same with the block holding 2.

// 0x08B5A4FC: resets the graphics state block 0x08D6CE00 (0x08C7AA38(state), 0x08C7AC28(state->+0x30),
// 0x08B84B5C(state->+0x24, 0, 0)) and clears the byte at 0x08D6E0F0.

// 0x08C00980 (a0 = a circular list's sentinel): releases every node until the sentinel.

// 0x08C2AB00: the same loop.

// 0x08C2AB4C: the same loop.

// 0x08C2AB98: the same loop.

// 0x08C24790: a half at +0x14 from the object's vtable slot 0x48 (argument 0x08CB06B8 for the null check).

// 0x08C247E4: a byte at +0x14 from the object's vtable slot 0x44.

// 0x0897851C (a0 = object): initialises the two intrusive list heads at +0x184 and +0x18C with 0x08BEDDC4 and points
// each one at itself.

// 0x0897E884: scope 0x0897E844 (message 0x08CA3400), the object's floats +0x130 / +0x134 to +0x1AC / +0x1B0.

// 0x0898970C: the same with scope 0x089896CC and message 0x08CA39F0.

// 0x089AB444: scope 0x089AB370, the object's member at +0x14, 0x089D5B30() as the value, 0x08978D3C(member, value).

// 0x089AB49C: the same with 0x08965418() and 0x0897BB18(member, value).

// 0x089AB4F4: 0x08965418() and 0x08978D3C(member, value).

// 0x08A0F800 (a0 = object, a1 = pointer to a word): 0x08B20920(frame, 0x08C8E004), then 0x08B20A54(frame, *a1, 0);
// the answer's low four bits replace bits 24..27 of the object's +0x60.

// 0x08A26ED8 (a0 = object): 0x08B344E0(object), take and clear the member at +0x28, 0x08B3457C(object), and a non-null
// member is tail-called through 0x08B1B99C.

// 0x08A94B90 (a0 = container, a1 = byte offset, a2 = pointer to an object): the object's virtual getter at slot 0x3C
// answers a byte that goes to *(word(container + 0x14) + a1); a null object asserts with 0x08CB211C.

// 0x08B0DA60: walks the global list 0x08D7734C, asking each node's object at +4 for vtable slot 0x3C; a zero answer ends
// the walk with v0 = 0, a null node ends it with v0 = 1 (and so does an empty list).

// 0x08B173EC (a0 = object): 0x08B28C44(object + 0x18D4) and, while the byte at +0x19DC stays non-zero,
// 0x08B17240(object) and 0x08B28C74(object + 0x1900); v0 is the byte that ended the loop (0).

// 0x08C1FF18: the object taken through a1's vtable slot 0x58 gives two words for +0x18 / +0x1C.

// 0x08C4B078 (a0 = destination, a1 = end, a2 = a block): copies 0x70-byte blocks from a2 to a0, advancing the
// destination by 0x70, until it reaches a1 (equal pointers copy nothing); v0 is the last block's first word.

// 0x08C750C0 (a0 = pointer to a buffer pointer, a1 = end of the used range, a2 = a signed byte): 0x08C74FAC(a0,
// end - base, 0, 1, byte), then -1 goes to *(base - 4) and v0 = base + (end - base).

// 0x088183D4 (a0 = object, a1..t3 = a call description, the caller's seventh argument on the stack a pointer to a
// source): builds a block on the frame, 0x08B7C830(block + 0x14, *source, 0x80), then 0x08818168(object, block).

// 0x0885EC98 (a0 = a holder, a1 = an argument): 0x088DA0BC(object at word(a0)->+0x5AC), 0x088DE700(that, a1) and,
// when it answers non-zero, 0x088A40B0(answer); the function ends in a tail call 0x088DA57C(object, a1).

// 0x088FE308 (a0 = an object): 0x08B344E0(object + 0x10), byte +0xB = 0, 0x0896275C(object->+0x10),
// 0x08B36018(object->+0x44, 0, 0x20), then a tail call 0x08B3457C(object + 0x10).

} // namespace
} // namespace new_batch_main_523
} // namespace gt::game

#define NEW523(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_523::name, gt::ReturnKind::kind)
