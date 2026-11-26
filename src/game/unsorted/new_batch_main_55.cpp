// New batch, main session 55: short functions the recompiler emitted, none decompiled before, written by hand as structured C
// from `tools.disasm`. Repeated shapes live in the helpers below: call-then-tail-call wrappers, "call and return this",
// virtual-call forwarders, the scope-and-convert family (a lookup object on the frame, an assert when it is null, a
// converted value assigned to the output) and the vector-destructor family. v0 is left as the original leaves it on every
// path. A tail call pops the frame first (callee-saved registers are never modified here, so they come back as they were).
// No names: none has evidence. Draft: not yet through the gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_55 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
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
    return call_full(rt, ctx, target, arguments);
}
// `lw v0, (object); lw v1, slot(v0); jalr v1`: at the jalr v0 holds the vtable and v1 the target.
GuestResult call_virtual_v1(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    return call_full(rt, ctx, target, arguments);
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 signed_half_at(Runtime &rt, u32 address) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address)))); }

// --- one call on a0, a tail call with the same a0 (`s0 = a0; jal first; a0 = s0; j second`).
void call_then_tail(Runtime &rt, AllegrexContext &ctx, u32 first, u32 second) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, first, {self});
    }
    tail_call(ctx, second, {self});
}
// --- one call on a0, the answer is a0 (`s0 = a0; jal f; v0 = s0`).
void call_return_self(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, target, {self});
    ctx.gpr[2] = self;
}

// --- 11-instruction table accessors: element index * 392 (`a1 * 56 * 7`) in the table at **a0.
u32 element_of(Runtime &rt, u32 holder, u32 index) {
    const u32 table = word(rt, word(rt, holder));
    return table + index * 56u * 7u;
}

// --- the class initializers of 0x08A64E60..: a byte 3/5/21 at +4 (the word's upper 24 bits cleared), the vtable at +0, a tail call on this + 8.
void tag_initializer(Runtime &rt, AllegrexContext &ctx, u32 tag, u32 vtable, u32 tail) {
    const u32 self = ctx.gpr[4];
    put_byte(rt, self + 4u, tag);
    const u32 old = word(rt, self + 4u);
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = self;
    ctx.gpr[6] = old & 0xFFu;
    put(rt, self + 4u, old & 0xFFu);
    tail_call(ctx, tail, {self + 8u});
}

// --- the scope-and-convert family. `open` fills the slot `open_slot` (frame offset 0 or 4) with an object, null-checked with
// `message`; `convert` writes a value built from `argument` into the slot `result_slot` (the other one); the output (a0) is
// assigned from it unless it is that slot itself; both slots are destroyed (result slot first).
void scope_convert(Runtime &rt, AllegrexContext &ctx, u32 open, u32 open_slot, u32 message, u32 convert, u32 field_offset,
                   int kind) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 result_slot_offset = open_slot == 0u ? 4u : 0u;
    const u32 slot_open = frame.at(open_slot);
    const u32 slot_result = frame.at(result_slot_offset);
    call(rt, ctx, open, {slot_open});
    const u32 object = word(rt, slot_open);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    u32 argument;
    switch (kind) {
    case 0: argument = (word(rt, object + field_offset) >> 9) & 1u; break;                    // ext 9, 1
    case 1: argument = byte_at(rt, object + field_offset) != 0u ? 1u : 0u; break;              // lbu; sltu 0 < x
    case 2: argument = word(rt, object + field_offset) & 0xFu; break;                          // andi 0xF
    case 3: argument = word(rt, object + field_offset) != 0u ? 1u : 0u; break;                 // lw; sltu 0 < x
    default: argument = word(rt, object + field_offset) == 0u ? 1u : 0u; break;               // lw; sltiu x < 1
    }
    call(rt, ctx, convert, {slot_result, argument});
    if (out != slot_result) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, slot_result)});
    call(rt, ctx, 0x08A5B9ECu, {slot_result});
    call(rt, ctx, 0x08A5B9ECu, {slot_open});
}

// --- the vector destructors: two vtable words, a call that fills a block, then the buffer at +0xA8 is freed
// (size = (end - begin) rounded down to a multiple of 1 << shift, alignment 4) and the base destructor runs.
void vector_destructor(Runtime &rt, AllegrexContext &ctx, u32 vtable_a, u32 vtable_b, u32 fill, u32 shift) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, vtable_a);
    put(rt, self + 0x38u, vtable_b);
    call(rt, ctx, fill, {word(rt, self + 0xA8u), word(rt, self + 0xA8u + 4u), frame.at(0)});
    const u32 begin = word(rt, self + 0xA8u);
    const u32 end = word(rt, self + 0xA8u + 8u);
    const s32 count = static_cast<s32>(end - begin) >> shift;
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, static_cast<u32>(count) << shift, 4u});
    call(rt, ctx, 0x089D8D30u, {self});
}

// 0x088E4A80: a virtual call (slot 0x5C) on the object a0 points at.
void func_088E4A80(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4]);
    call_virtual(rt, ctx, object, 0x5Cu, {object});
}
// 0x088EABA0: a virtual call (slot 0xA4) on the object at a0 + 0x538.
void func_088EABA0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4] + 0x538u);
    call_virtual(rt, ctx, object, 0xA4u, {object});
}
// 0x088FDD68: f0 = the byte-swapped word at +8 of func_088FD9AC's answer (wsbw).
void func_089B45EC(Runtime &rt, AllegrexContext &ctx) { call_then_tail(rt, ctx, 0x089BA59Cu, 0x089B44E4u); }
// 0x08B114A4: the first call takes a0, the tail call a1.
// 0x08B120E0: call on this, tail call on this + 0x4D8.
// 0x08AC2F90: when a0 + 4 holds an object, jump to its virtual (slot 0x34) with (a0 + 0, that object); v0 = 0 when it is null.
// 0x08ACCD34: stores a2 at 0x08D00408, then a tail call with (a1->+0xB4, a1->+0xBC, a1->+0xD0, 0x08D00408, the old a3).
// 0x08AD08D8: two calls, v0 = 0.
void func_08C317CC(Runtime &rt, AllegrexContext &ctx) { call_return_self(rt, ctx, 0x08AA8F88u); }
void func_08C318D0(Runtime &rt, AllegrexContext &ctx) { call_return_self(rt, ctx, 0x08AA90B8u); }
void func_08C31954(Runtime &rt, AllegrexContext &ctx) { call_return_self(rt, ctx, 0x08AA9158u); }
void func_08C31A80(Runtime &rt, AllegrexContext &ctx) { call_return_self(rt, ctx, 0x08AA92D0u); }
// 0x08C55D4C: vtable words at +0 and +8, a zero at +4, a tail call on this + 8 (v0 = this, v1 = the second vtable).
void func_08C55D4C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CDB174u);
    put(rt, self + 8u, 0x08CDB088u);
    put(rt, self + 4u, 0u);
    ctx.gpr[2] = self;
    ctx.gpr[3] = 0x08CDB088u;
    tail_call(ctx, 0x08B6543Cu, {self + 8u});
}

// --- 44-byte functions
// 0x08805774: a call, then a tail call with (0x08D6950C, the word at 0x08D69524); v0 = 0x00090000 (a lui leftover).
// 0x0885C138: v0 / v1 = the two words at +0x38 / +0x3C of element a1 (392 bytes each) of the table at **a0.
void func_0885C138(Runtime &rt, AllegrexContext &ctx) {
    const u32 element = element_of(rt, ctx.gpr[4], ctx.gpr[5]);
    ctx.gpr[2] = word(rt, element + 0x38u);
    ctx.gpr[3] = word(rt, element + 0x3Cu);
}
// 0x0885C1D0: a tail call with (element a1, a2, a3).
void func_0885C1D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = word(rt, ctx.gpr[4]);
    const u32 table = word(rt, holder);
    const u32 stride = ctx.gpr[5] * 56u;
    ctx.gpr[2] = stride;
    ctx.gpr[3] = table;
    tail_call(ctx, 0x0885D868u, {table + stride * 7u, ctx.gpr[6], ctx.gpr[7]});
}
// 0x0885C1FC: v0 = the byte at +0x40 of the element (a2 + element address).
// 0x0885D29C: stores a2 / a3 at +0x38 / +0x3C of the element; v0 = the element.
void func_0885D29C(Runtime &rt, AllegrexContext &ctx) {
    const u32 element = element_of(rt, ctx.gpr[4], ctx.gpr[5]);
    put(rt, element + 0x38u, ctx.gpr[6]);
    put(rt, element + 0x3Cu, ctx.gpr[7]);
    ctx.gpr[2] = element;
}
// 0x0888B670: a call with a0 = the member at +0x18 and a1 = a 0xB0-byte frame whose first words are 0x10 and the old a1.
void func_0888B670(Runtime &rt, AllegrexContext &ctx) {
    const u32 member = word(rt, ctx.gpr[4] + 0x18u);
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0xB0u);
    put(rt, frame.at(8), second);
    put(rt, frame.at(0), 0x10u);
    call(rt, ctx, 0x088936A4u, {member, frame.at(0)});
}
// 0x08897130: a1 < 7 (signed) goes to +0xB0 of the object, otherwise to +0xB1; the other byte is cleared.
void func_08897130(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 value = static_cast<s32>(ctx.gpr[5]);
    const u32 object = word(rt, self);
    ctx.gpr[2] = object;
    if (value >= 7) {
        put_byte(rt, object + 0xB1u, ctx.gpr[5]);
        const u32 again = word(rt, self);
        ctx.gpr[3] = again;
        put_byte(rt, again + 0xB0u, 0u);
    } else {
        put_byte(rt, object + 0xB1u, 0u);
        const u32 again = word(rt, self);
        ctx.gpr[3] = again;
        put_byte(rt, again + 0xB0u, ctx.gpr[5]);
    }
}
// 0x088BF94C: resets a record at a1->+0x24 (a byte from a0 at +0x15 and +0x10, zeros at +0x20/0x54/0x7C/0x80/0x84), tail call on a1.
// 0x088D09E8: v0 = 1 when the words at +0x78 and +0x7C are equal and are not 0x481907FF.
void func_088D09E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 second = word(rt, ctx.gpr[4] + 0x7Cu);
    const u32 first = word(rt, ctx.gpr[4] + 0x78u);
    u32 result = 1u;
    if (first != second) result = 0u;
    else if (first == 0x481907FFu) result = 0u;
    ctx.gpr[2] = result;
    ctx.gpr[3] = first;
    ctx.gpr[4] = result;
}
// 0x0891FEF0: a call on this, then -1 at +0x248.
void func_0891FEF0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08926C2Cu, {self});
    ctx.gpr[2] = 0xFFFFFFFFu;
    put(rt, self + 0x248u, 0xFFFFFFFFu);
}
// 0x08A17E90: a tail call (a1, a2, a3) or (a1, a3, a2) depending on whether the word under *a0 is non-zero.
// 0x08A61714: stores func_08AA64DC(a1) at +8.
void func_08A61714(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 value = call(rt, ctx, 0x08AA64DCu, {ctx.gpr[5]});
    put(rt, self + 8u, value);
}
void func_08A65128(Runtime &rt, AllegrexContext &ctx) { tag_initializer(rt, ctx, 5u, 0x08CD2958u, 0x08A6972Cu); }
// 0x08930DD8: v0 = (the word at +8 of the callee's 0x30-byte frame) / 4 rounded toward zero.
void func_08930DD8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x08930D20u, {ctx.gpr[4], ctx.gpr[5], frame.at(0)});
    const s32 value = static_cast<s32>(word(rt, frame.at(8)));
    const u32 biased = static_cast<u32>(value) + 3u;
    const u32 negative = value < 0 ? 1u : 0u;
    ctx.gpr[4] = biased;
    ctx.gpr[3] = negative;
    ctx.gpr[2] = static_cast<u32>((negative != 0u ? static_cast<s32>(biased) : value) >> 2);
}

// --- 132-byte functions
// 0x08804028: run the static constructors once (guard byte at 0x08CE1400): while the word the cursor at 0x08CDC504 points at
// is non-zero, advance the cursor and call it; then func_08B76A84(0x08C7C430) and the guard byte = 1.
// 0x0885BF5C: scan the entries of the object at a0 + 4: -1 at (*a0)+0x10, then the index of the first entry func_0885E4DC accepts.
// 0x088A3544: destructor shape: vtables, a base call, a virtual call (slot 0) on the member at +0x5C with +0x58 when set, the member
// fields cleared, then the final vtables (v0 / v1).
void func_088A3544(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC2800u);
    put(rt, self + 4u, 0x08CC2898u);
    call(rt, ctx, 0x088A322Cu, {self});
    const u32 member = word(rt, self + 0x58u + 4u);
    if (member != 0u) {
        call_virtual(rt, ctx, member, 0u, {member, word(rt, self + 0x58u)});
        put(rt, self + 0x58u, 0u);
        put(rt, self + 0x58u + 4u, 0u);
    }
    put(rt, self, 0x08CC28F8u);
    put(rt, self + 4u, 0x08CC28E0u);
    ctx.gpr[2] = 0x08CC28F8u;
    ctx.gpr[3] = 0x08CC28E0u;
}
// 0x088A5E5C: nine calls func_0892C4B8(a1, (*a0)->+0x4C0, i, i + 0x64, -1), then a tail call on *a0.
// 0x088DB018: a 64-bit difference: (a record's pair at +0x40/+0x44 of *(a0+0x130)) - func_088D9C70(); v0:v1 = (-1,-1) for a
// zero pair, (low, high) when the high word is not negative, otherwise (0, 0).
void func_088DB018(Runtime &rt, AllegrexContext &ctx) {
    const u32 record = word(rt, ctx.gpr[4] + 0x130u);
    const u32 low = word(rt, record + 0x40u);
    const u32 high = word(rt, record + 0x44u);
    u32 result_low = 0xFFFFFFFFu;
    u32 result_high = 0xFFFFFFFFu;
    GuestFrame frame(ctx, 0x10u);
    if ((low | high) != 0u) {
        const GuestResult now = call_full(rt, ctx, 0x088D9C70u);
        const u32 difference_low = low - now.v0;
        const u32 borrow = low < difference_low ? 1u : 0u;
        const u32 difference_high = high - now.v1 - borrow;
        if (static_cast<s32>(difference_high) >= 0) {
            result_low = difference_low;
            result_high = difference_high;
        } else {
            result_low = 0u;
            result_high = 0u;
        }
    }
    ctx.gpr[2] = result_low;
    ctx.gpr[3] = result_high;
}
// 0x088E6D90: under the lock at (*a0)+0xEC, ask the object *a0 (virtual slot 0x50, argument a1) for a record; v0:v1 = its words +0x20/+0x24 (0 when none).
void func_088E6D90(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B344E0u, {word(rt, self) + 0xECu});
    const u32 object = word(rt, self);
    const GuestResult found = call_virtual(rt, ctx, object, 0x50u, {object, argument});
    u32 low = 0u;
    u32 high = 0u;
    if (found.v0 != 0u) {
        low = word(rt, found.v0 + 0x20u);
        high = word(rt, found.v0 + 0x24u);
    }
    call(rt, ctx, 0x08B3457Cu, {word(rt, self) + 0xECu});
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}
void func_089A1214(Runtime &rt, AllegrexContext &ctx) { vector_destructor(rt, ctx, 0x08CCB3F0u, 0x08CCB5DCu, 0x08BE5C0Cu, 3u); }
void func_089F8674(Runtime &rt, AllegrexContext &ctx) { vector_destructor(rt, ctx, 0x08CC7C38u, 0x08CC7E24u, 0x08BE5B74u, 6u); }
void func_08A05884(Runtime &rt, AllegrexContext &ctx) { vector_destructor(rt, ctx, 0x08CC71A0u, 0x08CC738Cu, 0x08BE5C58u, 5u); }
// 0x089A4070: a list head at +0x13C (self-linked), a frame record {vtable 0x08CCE7C0, this} handed to the virtual at slot 0x134,
// then func_089A3868(this, 1, 0.0, 0.0) unless func_089A3130(this) answers non-zero.
// 0x089FD8B0: vtable at +0, then for each node of the list at this + 4 (head at +8): the payload at node+0x14 gets its virtual
// (slot 4); the node is stepped by func_08B5F100. Tail call func_08BF913C(this + 4, this + 0xC's last word).
// 0x08A436A0: scope: func_08A431F8 fills the frame slot, null-checked; func_08B855E0(object + 0x44, a2); func_08A42F54(a0, a1, slot); slot destroyed.
void func_08A436A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x08A431F8u, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CADBFCu});
    call(rt, ctx, 0x08B855E0u, {object + 0x44u, third});
    call(rt, ctx, 0x08A42F54u, {self, second, frame.at(0)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
}
// 0x08A78BE8: the object at *a1 (null-checked) answers its virtual at slot 0x24, func_08A924F0 converts that into the slot at sp+4,
// which is assigned to the output a0 unless it is that slot, then destroyed.
// 0x08AA1CF8: the virtual at slot 0x110 of *a0 fills eight bytes; they are assembled big-endian into the 64-bit value stored at a1.
void func_08AA1CF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call_virtual(rt, ctx, ctx.gpr[4], 0x110u, {ctx.gpr[4], frame.at(0), 8u});
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    u32 low = 0u;
    u32 high = 0u;
    for (u32 index = 0u; index < 8u; ++index) {
        const u32 next_high = (low >> 24) | (high << 8);
        const u32 next_low = (low << 8) | byte_at(rt, frame.at(index));
        low = next_low;
        high = next_high;
    }
    put(rt, out, low);
    put(rt, out + 4u, high);
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}
// 0x08ABB2E4: the list at 0x08D77184 (next at +0, object at +4): virtual slot 0x68 on every object, func_08ABA3AC, then slot 0x6C on every object.
// 0x08ACEB40: slot a1 (240 bytes each) of the table at a0: when its flag byte (+0x4F5) is clear, set it, store a2 at +0x4C8,
// call func_08B24238(a2), func_08ACEA1C(a0, slot + 0x420) and tail-call func_08ACEA8C with the same pair.
// 0x08AEFF5C: func_08AF0910(0), then release the handles at 0x08D41AD0 + 0x538 / 0x53C when they are not negative (set to -1),
// and the object at +0x540 (func_08AD85C8, +0x540 and +0x544 cleared).
// 0x08B09824: under the lock 0x08D473E8: func_08B09394(*a0), func_08B3177C(*a0 + 0x120), the virtual at slot 0x20 of the object at (*a0)+0x110
// when set, func_08B093D4(a0) when the byte at (*a0)+0x11C is set; then the unlock tail call.
// 0x08B1815C: with the lock at a0 + 0x182C: when func_08B1A844() is zero, count a use at +0x1858; the first use skips the unlock-and-recheck;
// v0 = !func_08B1A844() after it (0 otherwise).
void func_08B1815C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 lock = self + 0x182Cu;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B34888u, {lock});
    u32 result = 0u;
    if (call(rt, ctx, 0x08B1A844u) == 0u) {
        const u32 uses = word(rt, self + 0x1858u) + 1u;
        put(rt, self + 0x1858u, uses);
        if (uses != 1u) call(rt, ctx, 0x08B34B2Cu, {lock});
        result = (call(rt, ctx, 0x08B1A844u) ^ 1u) & 0xFFu;
    }
    call(rt, ctx, 0x08B34B24u, {lock});
    ctx.gpr[2] = result;
}
// 0x08B181E0: clears +0x1874, func_08B24D6C(this), the virtual at slot 0x18; func_08B7B2E4(this + 0x1878): 2 returns, 0 also calls
// func_08B25968(this); any other answer ends with the virtual at slot 0x1C(this, 1).
// 0x08B322CC: a chain of calls that each must succeed (a negative answer ends the chain); the last handle goes to 0x08CE0C88; v0 = 1 / 0.
// 0x08B7B780: when bit 8 of the half at +0xC is set, func_08B807FC(func_08B796C4(), half at +0xE, 0, 2); bit 12 cleared; tail call
// func_08B80438(func_08B796C4(), half at +0xE, a1, a2).
void func_08B7B780(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    u32 flags = rt.memory().load16(self + 0xCu);
    u32 handle;
    {
        GuestFrame frame(ctx, 0x10u);
        if ((flags & 0x100u) != 0u) {
            const u32 state = call(rt, ctx, 0x08B796C4u);
            call(rt, ctx, 0x08B807FCu, {state, signed_half_at(rt, self + 0xEu), 0u, 2u});
            flags = rt.memory().load16(self + 0xCu);
        }
        rt.memory().store16(self + 0xCu, static_cast<std::uint16_t>(flags & ~(1u << 12)));
        handle = call(rt, ctx, 0x08B796C4u);
    }
    tail_call(ctx, 0x08B80438u, {handle, signed_half_at(rt, self + 0xEu), first, second});
}
// 0x08B82FFC: when the word at 0x08D72E4C is 1, release the four handles at 0x08D740D8 / 0x08D740EC / +4 / +8 (func_08C7ABB8) and clear
// the flags 0x08D740E4, 0x08D72E4C, 0x08D740E0; v0 = 0.
// 0x08B8A43C: func_08B8634C(a0, a1, a2, t0); then, when t0 (the size) is non-zero: copy it from a3 to (*a0) + a1 (one byte inline,
// otherwise func_08B79A88(dest, a3, size)); v0 = a0.

} // namespace
} // namespace new_batch_main_55
} // namespace gt::game

#define NEW_MAIN55(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_55::name, gt::ReturnKind::kind)
NEW_MAIN55(0x088E4A80u, func_088E4A80, Word);
NEW_MAIN55(0x088EABA0u, func_088EABA0, Word);
NEW_MAIN55(0x089B45ECu, func_089B45EC, Word);
NEW_MAIN55(0x08C317CCu, func_08C317CC, Word);
NEW_MAIN55(0x08C318D0u, func_08C318D0, Word);
NEW_MAIN55(0x08C31954u, func_08C31954, Word);
NEW_MAIN55(0x08C31A80u, func_08C31A80, Word);
NEW_MAIN55(0x08C55D4Cu, func_08C55D4C, Word);
NEW_MAIN55(0x0885C1D0u, func_0885C1D0, Word);
NEW_MAIN55(0x0885D29Cu, func_0885D29C, Word);
NEW_MAIN55(0x0888B670u, func_0888B670, Word);
NEW_MAIN55(0x08897130u, func_08897130, Word);
NEW_MAIN55(0x088D09E8u, func_088D09E8, Word);
NEW_MAIN55(0x0891FEF0u, func_0891FEF0, Word);
NEW_MAIN55(0x08A61714u, func_08A61714, Word);
NEW_MAIN55(0x08A65128u, func_08A65128, Word);
NEW_MAIN55(0x08930DD8u, func_08930DD8, Word);
NEW_MAIN55(0x088A3544u, func_088A3544, Word);
NEW_MAIN55(0x088E6D90u, func_088E6D90, DoubleWord);
NEW_MAIN55(0x089A1214u, func_089A1214, Word);
NEW_MAIN55(0x089F8674u, func_089F8674, Word);
NEW_MAIN55(0x08A05884u, func_08A05884, Word);
NEW_MAIN55(0x08A436A0u, func_08A436A0, Word);
NEW_MAIN55(0x08B1815Cu, func_08B1815C, Word);
NEW_MAIN55(0x08B7B780u, func_08B7B780, Word);
NEW_MAIN55(0x0885C138u, func_0885C138, DoubleWord);
NEW_MAIN55(0x088DB018u, func_088DB018, DoubleWord);
NEW_MAIN55(0x08AA1CF8u, func_08AA1CF8, DoubleWord);
