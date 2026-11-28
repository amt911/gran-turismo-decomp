// Main batch 1008: re-drafts of functions earlier batches registered (and that mismatched the original in the gate) or
// skipped twice as too hard, plus one that was left for a route that reaches it. Written from tools.disasm; each comment
// says what the gate saw (or why it was skipped) and what the asm says instead.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1008 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them (several of these read the callee's v0 next).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order):
// what the callee does not take keeps the value the caller had.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments).v0;
}
// The same with float arguments in f12-f15.
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 19u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (call_indirect's rule).
u32 call_slot(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_indirect(rt, ctx, target, arguments);
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline float fbits(u32 bits) { return std::bit_cast<float>(bits); }
void copy_words(Runtime &rt, u32 to, u32 from, u32 words) {
    for (u32 index = 0u; index != words; ++index) put(rt, to + 4u * index, word(rt, from + 4u * index));
}
// An intrusive counted reference: the atomic `ll / sc` counter at +offset drops by one, and the caller destroys the
// object when the old value was 1.
u32 drop_count(Runtime &rt, u32 object, u32 offset) {
    const u32 address = object + offset;
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (drop_count(rt, object, 4u) != 1u) return;
    (void)call_slot(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (drop_count(rt, object, 8u) != 1u) return;
    (void)call_slot(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}
// The PSP float-to-integer sequence (trunc.w.s with the 2^31 fix-up).
u32 float_to_u32(float value) {
    if (value >= 2147483648.0f) return static_cast<u32>(static_cast<s32>(value - 2147483648.0f)) | 0x80000000u;
    return static_cast<u32>(static_cast<s32>(value));
}
// A vertex colour from a byte: 0x<byte>FFFFFF, anything from 0x100 up opaque white.
u32 pack_alpha(float value) {
    const u32 index = float_to_u32(value);
    return index < 0x100u ? ((index << 24u) | 0x00FFFFFFu) : 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// 0x08BF1298 (a0 = vector, a1 = position, a2 = count, a3 = value): the libstdc++ vector<36-byte element>::_M_fill_insert
// (nothing for count 0). vector is {begin, end, capacity}. With room (spare elements from end to capacity >= count) the
// value is copied to a 36-byte frame temporary (sp + 0xC); when count is below the elements after `position` the last
// `count` elements are constructed past the end (func_08BF0F5C), the middle is moved backwards one element at a time
// (32 bytes of float pairs and the word at +0x20, i.e. the whole element) and func_08BF0E34 fills the gap; else the
// surplus copies of the value are constructed at the end (func_08BF0FB4), the tail moved there (func_08BF100C) and the
// old range filled. Full: a block of size + max(size, count) elements (func_08A5BC20, the length check
// func_08B5ADDC(0x08CA43C8) beyond 0x071C71C7) receives the prefix (func_08BF0E8C), count copies of the value
// (func_08BF0EF0) and the tail, the old range is destroyed (func_08BF0F4C) and freed (func_08A5BC54) and begin / end /
// capacity updated. v0 = the last call's answer, or block + bytes on the full path.
// The earlier draft's byte count was 32 - 4 per element (0x1C) where the original builds 32 + 4 (0x24), so the full
// path allocated a block 8 bytes short per element.

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// 0x08C4C0A0 (a0 = vector, a1 = position, a2 = count, a3 = value): the same fill-insert for 112-byte elements
// (0xB6DB6DB7 after an arithmetic >> 4, so element = 0x70). Same shape as func_08BF1298: room path (the value to the
// frame temporary at +0xC, func_08C4AD04 constructing the last `count` elements past the end, the backwards element
// copy, func_08C4B078 filling) or the tail path (func_08C4AE48, func_08C4B0D0, func_08C4B078), and the full path
// (func_08B35DA0 allocation with func_08B5ADDC(0x08CB4AFC) beyond 0x2492492, func_08C4B3C0 prefix and tail,
// func_08C4B35C copies, func_08C498B4 destruction, func_08B35254 free). v0 = block + bytes on the full path, the last
// call's answer otherwise, and the caller's own v0 when count is 0.
void func_08C4C0A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    GuestFrame frame(ctx, 0xC0u);
    put(rt, frame.at(0x80u), count);
    if (count == 0u) return;
    constexpr u32 kElement = 0x70u;              // 112 bytes
    constexpr u32 kInverseSeven = 0xB6DB6DB7u;   // / 7 after an arithmetic >> 4, i.e. / 112 bytes
    const u32 end = word(rt, vector + 4u);
    const u32 spare = static_cast<u32>(signed_of(word(rt, vector + 8u) - end) >> 4) * kInverseSeven;
    if (spare < count) {
        const u32 begin = word(rt, vector);
        const u32 size = static_cast<u32>(signed_of(end - begin) >> 4) * kInverseSeven;
        put(rt, frame.at(8u), size);
        if (0x2492492u - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4AFCu});
        const u32 grow = count < size ? size : count;  // _M_check_len's max(size, count)
        const u32 length = size + grow;
        u32 bytes = 0u;
        u32 block = 0u;
        u32 front = begin;
        if (length < size) {
            bytes = 0xFFFFFFE0u;
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
            front = word(rt, vector);
        } else if (length != 0u) {
            bytes = (length << 7) - (length << 4);  // 112 * length
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
            front = word(rt, vector);
        }
        const u32 prefix_end = call_set(rt, ctx, 0x08C4B3C0u, {front, position, block, frame.at(3u)});
        call_set(rt, ctx, 0x08C4B35Cu, {prefix_end, count, value, frame.at(2u)});
        const u32 new_end =
            call_set(rt, ctx, 0x08C4B3C0u, {position, word(rt, vector + 4u), prefix_end + count * kElement, frame.at(1u)});
        call_set(rt, ctx, 0x08C498B4u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        const u32 old = word(rt, vector);
        if (old != 0u) call_set(rt, ctx, 0x08B35254u, {old});
        ctx.gpr[2] = block + bytes;
        put(rt, vector + 8u, block + bytes);
        put(rt, vector + 4u, new_end);
        put(rt, vector, block);
        return;
    }
    const u32 temporary = frame.at(0xCu);
    copy_words(rt, temporary, value, 28u);
    const u32 after_units = static_cast<u32>(signed_of(end - position) >> 4);
    const u32 after = after_units * kInverseSeven;
    if (count < after) {
        call_set(rt, ctx, 0x08C4AD04u, {end - count * kElement, end, end, frame.at(6u)});
        const u32 source_end = end - count * kElement;
        const u32 moved = static_cast<u32>(signed_of(source_end - position) >> 4) * kInverseSeven;
        put(rt, vector + 4u, word(rt, vector + 4u) + count * kElement);
        if (signed_of(moved) > 0) {
            u32 from = source_end;
            u32 to = end;
            for (u32 index = 0u; index != moved; ++index) {
                from -= kElement;
                to -= kElement;
                copy_words(rt, to, from, 28u);
            }
        }
        call_set(rt, ctx, 0x08C4B078u, {position, position + count * kElement, temporary});
        return;
    }
    call_set(rt, ctx, 0x08C4AE48u, {end, count - after, temporary, frame.at(5u)});
    put(rt, vector + 4u, word(rt, vector + 4u) + (count - after) * kElement);
    call_set(rt, ctx, 0x08C4B0D0u, {position, end, word(rt, vector + 4u), frame.at(4u)});
    put(rt, vector + 4u, word(rt, vector + 4u) + (after_units << 4));
    call_set(rt, ctx, 0x08C4B078u, {position, end, temporary});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// 0x08B70F70 (a0 = lhs, a1 = rhs, a2 = out): the soft-float double add over the library's records of 0x18 bytes
// {class, sign, exponent, -, mantissa low at +0x10, mantissa high at +0x14}, where class below 2 is a NaN, 2 zero,
// 3 normal and 4 infinity. v0 is a pointer: the NaN operand itself for a class below 2, the infinity of either operand,
// the NaN record 0x08C966B0 when two infinities of different signs meet, the zero operand unless both are zero (then
// `out` with class 2 and the sign the two share), and `out` for two normals. There the exponents are compared: a
// distance of 64 or more drops the smaller mantissa (the answer takes the larger exponent); otherwise the smaller
// mantissa is shifted right by the distance, keeping the bits it drops in bit 0 of the low word. Equal signs add the two
// 64-bit mantissas, different signs subtract the smaller from the larger and negate a negative difference; the result is
// stored with its sign and the larger exponent, then normalized left while the high word minus one plus the carry of a
// non-zero low word stays under 0x0FFFFFFF (when it lands exactly on the limit the loop shifts once more only while the
// shifted low word is non-zero), and one right shift with the dropped bit kept runs when the high word is above
// 0x1FFFFFFF. The earlier draft tested the carry as `low == 0` and reused a stale value as the shift carry.

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// 0x08B28768 (a0 = self): one read step of the platform's binary stream reader. self[0x98] is cleared and, with the
// pending flag self[0x56] set, the virtual self[0][8] runs and func_08B272A0(self). Then self[0x30] = the mode table
// 0x08C93798[self[4]], the buffer is self[0x88] and its end self[0x88] + self[0x8C], consumed = self[0x68], left =
// self[0x6C]. With the pending flag still set the state self[8] selects a record read into the buffer: 1 needs 0xA bytes
// (func_08B28704 with self[0xC]), 2 needs 2 (func_08B27E00), 3 needs 8 (func_08B286BC), each advancing the buffer; a
// short buffer runs the virtual self[0][0xC] and answers 0. The credit loop (self[0x60], clamped to 0xFFFF, against
// `left`) stores the progress in self[0x5C..0x7C], asks func_08B28524(self) and walks the buffer by each credit until
// `left` is used up, summing into self[0x70]; self[0x58] says whether a record was completed (self[0x57] and the credit
// test decide). A completed record then runs func_08B27874(self, 0) and func_08B2797C(self), and the state 1
// (func_08B29E88) / 2 (func_08B27E80) answer is stored at self[0xA0] / self[0x9C] and written into the buffer when
// self[0x58] is set (8 bytes, then 4 more of self[0x70] / 4 bytes), little-endian, the low byte first. The virtual
// self[0][0xC] runs once more when a record was written, then self[0x98] = 1, self[0x94] = the bytes written since
// self[0x88] and v0 = 1; the failure paths answer 0. The earlier draft wrote both record halves big-endian (the original
// stores the low byte at +0 and the top byte at +3).

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// 0x088871DC (a0 = collection, a1 = argument, a2 = source, a3 = key): walks the collection's items 0 .. count-1.
// count = collection->vtable[0x24](collection); each item is asked as (out = {object @ +0x10, counted @ +0x14}) through
// collection->vtable[0x28](&out, collection, index). With an object, source->vtable[0x18](source, index) gives an entry
// whose virtual 0xC and 0x10 must both answer non-zero; then source->vtable[0x20](source, index) must answer non-zero
// (a zero skips the item, with source->vtable[0x24] still stored at frame +0x28), the key's own virtual 0x64 yields a
// value compared with the index (frame +0x24), an optional holder at collection + 0x24 fills frame +0x20, and the
// object's virtual 0x10 runs with seven arguments (a1 = the call's a1, a2 = the 0x20 answer, a3 = the 0x24 answer,
// t0 = the key, t1 = +0x20, t2 = +0x24). A global object (0x08CFFAAC) then gets the item's pair by address through its
// virtual 0x1C, and both counted references (the copy and the item's) are released. v0 ends as the collection's vtable
// word (the loop's last load) or the count when it is not positive. The earlier draft ran the body even when the 0x20
// answer was zero.
void func_088871DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 collection = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 key = ctx.gpr[7];
    if (source == 0u || key == 0u) return;  // v0 untouched, as the original's early return
    GuestFrame frame(ctx, 0x60u);
    const u32 count =
        call_slot(rt, ctx, word(rt, word(rt, collection) + 0x24u), {collection, argument, source, key});
    if (signed_of(count) <= 0) return;  // v0 = the count
    u32 index = 0u;
    while (index != count) {
        (void)call_slot(rt, ctx, word(rt, word(rt, collection) + 0x28u), {frame.at(0x10u), collection, index, key});
        const u32 item = word(rt, frame.at(0x10u));
        bool body = false;
        if (item != 0u) {
            const u32 entry = call_slot(rt, ctx, word(rt, word(rt, source) + 0x18u), {source, index, index, key});
            if (call_slot(rt, ctx, word(rt, word(rt, entry) + 0xCu), {entry, index, index, key}) != 0u &&
                call_slot(rt, ctx, word(rt, word(rt, entry) + 0x10u), {entry, index, index, key}) != 0u)
                body = true;
        }
        if (body) {
            const u32 selected = call_slot(rt, ctx, word(rt, word(rt, source) + 0x20u), {source, index, index, key});
            const u32 selected_other =
                call_slot(rt, ctx, word(rt, word(rt, source) + 0x24u), {source, index, index, key});
            put(rt, frame.at(0x28u), selected_other);
            if (selected != 0u) {
                put(rt, frame.at(0x20u), 0u);
                const u32 kind = call_slot(rt, ctx, word(rt, word(rt, key) + 0x64u), {key});
                put(rt, frame.at(0x24u), (index ^ kind) < 1u ? 1u : 0u);
                const u32 holder = word(rt, collection + 0x24u);
                if (holder != 0u) {
                    (void)call_slot(rt, ctx, word(rt, word(rt, holder) + 0xCu), {frame.at(8u), holder, index});
                    put(rt, frame.at(0x20u), word(rt, frame.at(8u)));
                    const u32 temporary = word(rt, frame.at(0xCu));
                    if (temporary != 0u) release_object(rt, ctx, temporary);
                }
                const u32 first = word(rt, frame.at(0x10u));
                (void)call_slot(rt, ctx, word(rt, word(rt, first) + 0x10u),
                                {first, argument, selected, selected_other, key, word(rt, frame.at(0x20u)),
                                 word(rt, frame.at(0x24u))});
                const u32 manager = word(rt, 0x08CFFAACu);
                put(rt, frame.at(0u), word(rt, frame.at(0x10u)));
                put(rt, frame.at(4u), word(rt, frame.at(0x14u)));
                const u32 counted = word(rt, frame.at(4u));
                if (counted != 0u) put(rt, counted + 4u, word(rt, counted + 4u) + 1u);
                (void)call_slot(rt, ctx, word(rt, word(rt, manager) + 0x1Cu), {manager, frame.at(0u)});
                const u32 temporary = word(rt, frame.at(4u));
                if (temporary != 0u) release_object(rt, ctx, temporary);
            }
        }
        const u32 item_counted = word(rt, frame.at(0x14u));
        if (item_counted != 0u) release_object(rt, ctx, item_counted);
        ++index;
    }
    ctx.gpr[2] = word(rt, collection);  // the last load of the loop's delay slot (`lw v0, 0(s3)`)
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// 0x088C9F48 (this, a1): a graphics setup pass. Without its words +0xC, +0x10, +0x14, +0x18 or with a negative
// +0x1C / +0x20 it does nothing (each early exit answering what its own load left in v0: 0, 0, 0, the word at +0x14,
// then +0x1C / +0x20 themselves); otherwise 0x08AF6D14(this[0x18], a1) must answer non-zero and a fixed sequence of the
// graphics library runs (0x08B5711C(0x481), 0x08B58874(0x6F), 0x08B5877C(6, f12 = 0.05f), 0x08B54FB8(0x6A / 0x64 /
// 0x78), 0x08B58874(0x8002 / 0x70), 0x08B54FB8(0x6C), 0x08B58A74(0), 0x08B58AD0(1,1,1,1), 0x08B55AAC pairs,
// 0x08B55B90, 0x08B584D0(0x400, 0x302, 0x303), 0x08B58874(2)). Each of the +0x28 stages at this + index*0x108 then runs
// 0x08AF6CE0(array[word(this + 0x1C)] + index*0x98, 0) and two 0x088C9918(this, stage + 0x60 / +0xA0, 0 / 1) with the
// float at stage + 0x160, then the same pair with +0xE0 / +0x120 and the float at +0x164; the stage count is re-read
// every pass. With a non-negative +0x24: 0x08AF6CE0(array[word(this + 0x24)] + ...), 0x08B584D0(0x400, 0x302, 1),
// 0x088C9C08(this, a1), 0x08B54FB8(0x8002), 0x08B58A74(1), 0x08B58874(0x6D) and a tail call of 0x08B58874(0x6A). A
// negative +0x24 tail-calls 0x08AFBEF4 instead, with a1 = word(this + 0x18), a2 = word(this + 0x24) and a0 / a3 as the
// last callee left them.
void func_088C9F48(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    u32 tail_target = 0u;
    u32 tail_argument = 0u;
    u32 tail_second = 0u;
    u32 tail_third = 0u;
    {
        GuestFrame frame(ctx, 0x30u);
        u32 check = word(rt, self + 0xCu);
        if (check == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        check = word(rt, self + 0x10u);
        if (check == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        check = word(rt, self + 0x14u);
        if (check == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 records = word(rt, self + 0x18u);
        if (records == 0u) {
            ctx.gpr[2] = word(rt, self + 0x14u);  // v0 still holds the word the previous test loaded
            return;
        }
        const u32 first_index = word(rt, self + 0x1Cu);
        if (signed_of(first_index) < 0) {
            ctx.gpr[2] = first_index;
            return;
        }
        const u32 second_index = word(rt, self + 0x20u);
        if (signed_of(second_index) < 0) {
            ctx.gpr[2] = second_index;
            return;
        }
        if (call_keep(rt, ctx, 0x08AF6D14u, {records, argument}).v0 == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        (void)call_set(rt, ctx, 0x08B5711Cu, {0x481u});
        (void)call_set(rt, ctx, 0x08B58874u, {0x6Fu});
        (void)call_setf(rt, ctx, 0x08B5877Cu, {6u}, {fbits(0x3D4CCCCDu)});
        (void)call_set(rt, ctx, 0x08B54FB8u, {0x6Au});
        (void)call_set(rt, ctx, 0x08B54FB8u, {0x64u});
        (void)call_set(rt, ctx, 0x08B54FB8u, {0x78u});
        (void)call_set(rt, ctx, 0x08B58874u, {0x8002u});
        (void)call_set(rt, ctx, 0x08B58874u, {0x70u});
        (void)call_set(rt, ctx, 0x08B54FB8u, {0x6Cu});
        (void)call_set(rt, ctx, 0x08B58A74u, {0u});
        (void)call_set(rt, ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
        (void)call_set(rt, ctx, 0x08B55AACu, {0u, 1u});
        (void)call_set(rt, ctx, 0x08B55AACu, {2u, 0x8000u});
        (void)call_set(rt, ctx, 0x08B55AACu, {1u, 0u});
        (void)call_set(rt, ctx, 0x08B55AACu, {5u, 1u});
        (void)call_set(rt, ctx, 0x08B55AACu, {6u, 1u});
        (void)call_set(rt, ctx, 0x08B55AACu, {4u, 1u});
        (void)call_set(rt, ctx, 0x08B55AACu, {3u, 1u});
        (void)call_set(rt, ctx, 0x08B55B90u, {});
        (void)call_set(rt, ctx, 0x08B584D0u, {0x400u, 0x302u, 0x303u});
        (void)call_set(rt, ctx, 0x08B58874u, {2u});
        for (s32 index = 0; index < signed_of(word(rt, self + 0x28u)); ++index) {
            const u32 stage = self + static_cast<u32>(index) * 0x108u;
            (void)call_set(rt, ctx, 0x08AF6CE0u, {word(rt, word(rt, self + 0x18u) + 0x18u) + word(rt, self + 0x1Cu) * 152u, 0u});
            (void)call_setf(rt, ctx, 0x088C9918u, {self, stage + 0x60u, 0u}, {float_at(rt, stage + 0x160u)});
            (void)call_setf(rt, ctx, 0x088C9918u, {self, stage + 0xA0u, 1u}, {float_at(rt, stage + 0x160u)});
            (void)call_set(rt, ctx, 0x08AF6CE0u, {word(rt, word(rt, self + 0x18u) + 0x18u) + word(rt, self + 0x20u) * 152u, 0u});
            (void)call_setf(rt, ctx, 0x088C9918u, {self, stage + 0xE0u, 0u}, {float_at(rt, stage + 0x164u)});
            (void)call_setf(rt, ctx, 0x088C9918u, {self, stage + 0x120u, 1u}, {float_at(rt, stage + 0x164u)});
        }
        const u32 after = word(rt, self + 0x24u);
        if (signed_of(after) >= 0) {
            (void)call_set(rt, ctx, 0x08AF6CE0u, {word(rt, word(rt, self + 0x18u) + 0x18u) + after * 152u, 0u});
            (void)call_set(rt, ctx, 0x08B584D0u, {0x400u, 0x302u, 1u});
            (void)call_set(rt, ctx, 0x088C9C08u, {self, argument});
            (void)call_set(rt, ctx, 0x08B54FB8u, {0x8002u});
            (void)call_set(rt, ctx, 0x08B58A74u, {1u});
            (void)call_set(rt, ctx, 0x08B58874u, {0x6Du});
            tail_target = 0x08B58874u;
            tail_argument = 0x6Au;
            tail_second = ctx.gpr[5];
            tail_third = ctx.gpr[6];
        } else {
            tail_target = 0x08AFBEF4u;
            tail_argument = ctx.gpr[4];
            tail_second = word(rt, self + 0x18u);
            tail_third = after;
        }
    }
    tail_call(ctx, tail_target, {tail_argument, tail_second, tail_third, ctx.gpr[7]});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// 0x088C9C08 (a0 = this, a1 = the argument handed on): the sprite-batch builder its sibling 0x088C9F48 calls. With
// word(this + 0x28) rows it runs, 0x3F0 bytes per row and two passes of 20 records of 0x18 bytes per row (the blocks at
// this + 0xAB0 + 24*index + pass*0x1F8 + row*0x3F0): each record's four 0x18-byte corner vectors (offset +0xC of the
// record and of the next one, plus the two records themselves) are read into the frame by 0x088C98F0, run through
// 0x08B5A7F0, and the four vertices (0x18 bytes each: 0, 0.0f, the colour, the three floats) are written into a
// 0x60-byte object from 0x08B550E0(0x60). The colour is 0xFFFFFFFF and the two "alpha" fields are 204.0f, except the
// first record's first alpha (0.0f) and the last record's others (0.0f, which keeps the sibling colour); then
// 0x08B569A0(the third vertex), 0x08B56510, 0x08B555F8(object), 0x08B556A0(0, 0, 0x103, 0, 3, 0x103),
// 0x08B55588(4, 4) and 0x08B56940 run per record. v0 = word(this + 0x28) when it is not positive (0 at the end).
// The four-byte colour field of a vertex is (alpha << 24) | 0xFFFFFF for an alpha under 0x100, else 0xFFFFFFFF; the
// second pair ORs (alpha << 24) or 0x00FF0000 into the colour already built.
void func_088C9C08(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x90u);
    (void)call_keep(rt, ctx, 0x08AF26F0u, {argument, argument});
    (void)call_keep(rt, ctx, 0x08B56A00u, {0u});
    const s32 rows = signed_of(word(rt, self + 0x28u));
    if (rows <= 0) {
        ctx.gpr[2] = static_cast<u32>(rows);
        return;
    }
    const float one = 1.0f;
    u32 row_offset = 0u;  // sp + 0x48
    for (s32 row = 0; row < rows; ++row) {
        u32 pass = 0u;        // sp + 0x40
        u32 block = 0u;       // fp
        do {
            for (u32 index = 0u; index != 20u; ++index) {
                const u32 record = self + 0xAB0u + 24u * index + block + row_offset;
                const u32 next_record = self + 0xAB0u + 24u * (index + 1u) + block + row_offset;
                const float first_alpha = index == 0u ? 0.0f : 204.0f;
                const u32 object = call(rt, ctx, 0x08B550E0u, {0x60u});
                const u32 colour = pack_alpha(first_alpha);
                (void)call_keep(rt, ctx, 0x088C98F0u, {record + 0xCu, frame.at(0x00u)});
                (void)call_keep(rt, ctx, 0x088C98F0u, {record, frame.at(0x10u)});
                (void)call_keep(rt, ctx, 0x088C98F0u, {next_record, frame.at(0x20u)});
                (void)call_keep(rt, ctx, 0x088C98F0u, {next_record + 0xCu, frame.at(0x30u)});
                for (u32 corner = 0u; corner != 4u; ++corner)
                    (void)call_keep(rt, ctx, 0x08B5A7F0u, {frame.at(0x10u * corner), frame.at(0x10u * corner)});
                put(rt, object + 0x00u, 0u);
                put_float(rt, object + 0x04u, 0.0f);
                put(rt, object + 0x08u, colour);
                put(rt, object + 0x0Cu, word(rt, frame.at(0x00u)));
                put(rt, object + 0x10u, word(rt, frame.at(0x04u)));
                put(rt, object + 0x14u, word(rt, frame.at(0x08u)));
                put_float(rt, object + 0x18u, one);
                put_float(rt, object + 0x1Cu, 0.0f);
                put(rt, object + 0x20u, colour);
                put(rt, object + 0x24u, word(rt, frame.at(0x10u)));
                put(rt, object + 0x28u, word(rt, frame.at(0x14u)));
                put(rt, object + 0x2Cu, word(rt, frame.at(0x18u)));
                const float second_alpha = index == 19u ? 0.0f : 204.0f;
                const u32 second = float_to_u32(second_alpha);
                const u32 second_colour = (second < 0x100u ? (second << 24u) : 0x00FF0000u) | colour;
                put(rt, object + 0x30u, 0u);
                put_float(rt, object + 0x34u, one);
                put(rt, object + 0x38u, second_colour);
                put(rt, object + 0x3Cu, word(rt, frame.at(0x30u)));
                put(rt, object + 0x40u, word(rt, frame.at(0x34u)));
                put(rt, object + 0x44u, word(rt, frame.at(0x38u)));
                put_float(rt, object + 0x48u, one);
                put_float(rt, object + 0x4Cu, one);
                put(rt, object + 0x50u, second_colour);
                put(rt, object + 0x54u, word(rt, frame.at(0x20u)));
                put(rt, object + 0x58u, word(rt, frame.at(0x24u)));
                put(rt, object + 0x5Cu, word(rt, frame.at(0x28u)));
                (void)call_keep(rt, ctx, 0x08B569A0u, {object + 0x48u});
                (void)call_keep(rt, ctx, 0x08B56510u, {});
                (void)call_keep(rt, ctx, 0x08B555F8u, {object});
                (void)call_keep(rt, ctx, 0x08B556A0u, {0u, 0u, 0x103u, 0u, 3u, 0x103u});
                (void)call_keep(rt, ctx, 0x08B55588u, {4u, 4u});
                (void)call_keep(rt, ctx, 0x08B56940u, {});
            }
            ++pass;
            block += 0x1F8u;
        } while (pass != 2u);
        row_offset += 0x3F0u;
    }
    ctx.gpr[2] = 0u;
}

} // namespace
} // namespace new_batch_main_1008
} // namespace gt::game

#define NEWMAIN1008(address, name, kind)                                                                               \
    GT_DECOMP(address, gt::game::new_batch_main_1008::name, gt::ReturnKind::kind)
NEWMAIN1008(0x088C9C08u, func_088C9C08, Word);
NEWMAIN1008(0x08C4C0A0u, func_08C4C0A0, Word);
NEWMAIN1008(0x088871DCu, func_088871DC, Word);
NEWMAIN1008(0x088C9F48u, func_088C9F48, Word);
