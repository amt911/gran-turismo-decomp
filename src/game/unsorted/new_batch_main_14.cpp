// New batch main 14: 80 functions the extra seeds made emittable, written as structured C from `tools.disasm`.
// DRAFT: none has been through the differential gate yet (the main session writes, the verifier gates).
// No names: none has evidence. Groups, each with its helper's comment above it:
//   * the counted-value emitters (a holder's 64-bit field compared with a pair of words from the object, the
//     answer built as a bool value object on the frame and assigned to the destination);
//   * "assign the holder's value when it is of the expected kind, else insert it" (four instances of one shape);
//   * the vector inserters that move a run of N-byte records down before handing the tail to a range builder;
//   * a long tail of one-off shapes, each noted above its function.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>
#include <limits>

namespace gt::game {
namespace new_batch_main_14 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with float arguments (EABI: they go in $f12-$f15).
u32 callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
          std::initializer_list<float> floats) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments, floats).v0;
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
// Allegrex trunc.w.s: toward zero, NaN and out-of-range values give INT_MAX (portable: no C++ UB).
s32 trunc_w_s(float value) {
    if (!(value > -2147483648.0f && value < 2147483648.0f)) return std::numeric_limits<s32>::max();
    return static_cast<s32>(value);
}
// The Allegrex `wsbw`: byte order reversed within the word (a big-endian 64-bit load is two of these).
u32 swap_bytes(u32 value) {
    return (value >> 24) | ((value >> 8) & 0xFF00u) | ((value << 8) & 0xFF0000u) | (value << 24);
}

// --- the counted-value emitters (6 instances) -------------------------------------------------------------
// (a0 = the counted value: low word +0x18, high word +0x1C; a1 = the destination; a3 = a pointer to the holder
// pointer). The holder's object is reported through func_08A5BB7C when null and then read through anyway; its
// virtual slot loads the low word into v0 and the high word into v1, which are compared with (a0's pair) as a
// 64-bit number in `order`; func_08A97A6C(sp, bool) builds a bool value object that func_08A5BAD8 copies into
// a1 unless it already is the frame address, and func_08A5B9EC releases it.
enum class PairOrder { UnsignedLess, UnsignedGreater, SignedLess, SignedGreater };

void pair_compare_emit(Runtime &rt, AllegrexContext &ctx, u32 assert_text, u32 slot, PairOrder order) {
    const u32 pair = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 holder_ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 low = word(rt, pair + 0x18u);
    const u32 high = word(rt, pair + 0x1Cu);
    const u32 object = word(rt, holder_ref);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_text});
    const GuestResult loaded = call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object}, {});
    bool less = false;
    switch (order) {
    case PairOrder::UnsignedLess: less = loaded.v1 < high || (loaded.v1 == high && loaded.v0 < low); break;
    case PairOrder::UnsignedGreater: less = high < loaded.v1 || (loaded.v1 == high && low < loaded.v0); break;
    case PairOrder::SignedLess:
        less = static_cast<s32>(loaded.v1) < static_cast<s32>(high) || (loaded.v1 == high && loaded.v0 < low);
        break;
    case PairOrder::SignedGreater:
        less = static_cast<s32>(high) < static_cast<s32>(loaded.v1) || (loaded.v1 == high && low < loaded.v0);
        break;
    }
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0u), less ? 1u : 0u, loaded.v0});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- "assign the holder's value, or insert it" (4 instances) ----------------------------------------------
// (a0 = the destination, a1 = a pointer to the holder pointer): a null holder assigns 0; else the object is
// compared against the kind `kind_getter` answers (func_08A76F20) — of that kind, the holder's value is
// assigned; otherwise `insert` fills a frame value object (the holder's pointer), which is assigned when it is
// non-null. v0 = 1 except when nothing was inserted.
void assign_or_insert(Runtime &rt, AllegrexContext &ctx, u32 kind_getter, u32 insert) {
    const u32 out = ctx.gpr[4];
    const u32 holder_ref = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 object = word(rt, holder_ref);
    u32 result = 1u;
    if (object == 0u) {
        call(rt, ctx, 0x08A5BAD8u, {out, 0u});
    } else {
        const u32 kind = call(rt, ctx, kind_getter, {out, 0u});
        if (call(rt, ctx, 0x08A76F20u, {object, kind}) != 0u) {
            call(rt, ctx, 0x08A5BAD8u, {out, word(rt, holder_ref)});
        } else {
            call(rt, ctx, insert, {frame.at(0u), holder_ref, ctx.gpr[6]});
            const u32 value = word(rt, frame.at(0u));
            if (value != 0u) {
                call(rt, ctx, 0x08A5BAD8u, {out, value});
            } else {
                result = 0u;
            }
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
    }
    ctx.gpr[2] = result;
}

// --- 176-byte group ---------------------------------------------------------------------------------------
// 0x08A3FB80: two frame cells are built (func_08A3F684 / func_08A3F8E8), both holders reported when null
// (0x08CADA84 / 0x08CADA70) and read anyway; the first's record name (func_08B21C64 at +0x18) is handed to
// func_08B21858 on the second (+0x14), and the second is assigned to a0 (func_08A5BAD8) unless a0 is that cell;
// both cells are released (func_08A5B9EC).
void frame_pair_assign(Runtime &rt, AllegrexContext &ctx, u32 first_maker, u32 first_text) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 first_cell = frame.at(0u);
    const u32 second_cell = frame.at(4u);
    call(rt, ctx, first_maker, {first_cell, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    call(rt, ctx, 0x08A3F8E8u, {second_cell, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    const u32 first = word(rt, first_cell);
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {first_text});
    const u32 second = word(rt, second_cell);
    if (second == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CADA70u});
    const u32 name = call(rt, ctx, 0x08B21C64u, {first + 0x18u});
    call(rt, ctx, 0x08B21858u, {second + 0x14u, name});
    if (out != second_cell) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, second_cell)});
    call(rt, ctx, 0x08A5B9ECu, {second_cell});
    call(rt, ctx, 0x08A5B9ECu, {first_cell});
}


// 0x08A411A8 (a0 = object with a word vector at +0x14/+0x18, a1 = holder pointer, a2 = a value): the holder's
// index (virtual +0x3C) picks an element of the vector (each 4 bytes); a negative index leaves v0 = the index
// and an index past the end leaves v0 = 0; otherwise func_08A40E90(sp, a2) fills a value object that
// func_08A5BAD8 assigns to the element unless it is the frame address, and func_08A5B9EC releases it.


void func_08A730D8(Runtime &rt, AllegrexContext &ctx) { assign_or_insert(rt, ctx, 0x08A72CC0u, 0x08AA257Cu); }
void func_08A87C28(Runtime &rt, AllegrexContext &ctx) { assign_or_insert(rt, ctx, 0x08A87690u, 0x08AA22BCu); }
void func_08AB173C(Runtime &rt, AllegrexContext &ctx) { assign_or_insert(rt, ctx, 0x08AB1304u, 0x08AA25F0u); }

// 0x08A840F4 (a0 = the counted value's low word at +0x18 / high word at +0x1C, a1 = destination, a3 = holder
// pointer): the holder's virtual +0x4C is the shift count; the 64-bit value is shifted right by it (the asm's
// sllv/srlv/srav with the movn pair for counts >= 32) and func_08A83160(sp, count & 0x20, low, high) builds a
// value object that func_08A5BAD8 assigns to a1 unless it is the frame address; func_08A5B9EC releases it.

// 0x08AC7548 (a0 = object with a handle at +0x2C, a1 = a buffer, a2 = a destination word): under the global
// lock (func_08B344E0 / func_08B3457C) func_08C7AB40(handle, a1, a2, sp, sp + 4) runs with two frame words
// cleared. A non-negative answer stores the byte (+0x29) = (sp[0] != 0) and answers 0; a negative one answers 1
// and clears *a2 when it is one of the two Sony error codes (answer + 0x7F9CFFDD < 2), else answers 2 and sets
// the byte at +0x28.

// 0x08B471FC (a0 = object, a1 = holder pointer, a2 = a flag byte): func_08B3FA98(sp + 8, a1) then
// func_08B3F8A0(sp + 8) names the record func_08C7AE50(holder's object, name, sp, a0 + 0x8D, sp + 4, 0, 1)
// reads (sp + 4 = 0x2400). A non-negative answer with a clear flag calls the object's virtual +0xB0 with
// (object, sp + 8, a0 + 0x8D, sp + 4); otherwise v0 stays the call's answer.

// 0x08B63574 (a0 = a format byte, a1 = a pointer): 0xFF answers 0 without doing anything; otherwise the low
// nibble of a0 & 0x70 picks a prefix reader (0x20, 0x30 and 0x40 end in a tail call, 0x60/0x70 report twice
// through func_08B77B04 first and then tail-call 0x08B726FC), and 0x00, 0x10 and 0x50 answer 0.
void func_08B63574(Runtime &rt, AllegrexContext &ctx) {
    const u32 type = ctx.gpr[4] & 0xFFu;
    if (type == 0xFFu) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 target = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        switch (type & 0x70u) {
        case 0x20u: target = 0x08B726ECu; break;
        case 0x30u: target = 0x08B726F4u; break;
        case 0x40u: target = 0x08B726FCu; break;
        case 0u:
        case 0x10u:
        case 0x50u:
            ctx.gpr[2] = 0u;
            return;
        default:  // 0x60 and 0x70: the format is reported twice before the same reader
            call(rt, ctx, 0x08B77B04u);
            call(rt, ctx, 0x08B77B04u);
            target = 0x08B726FCu;
            break;
        }
    }
    tail_call(ctx, target, {ctx.gpr[5]});
}

// A 16-byte-record range move shared by 0x08B89338 and (8-byte records) 0x08C49B00: the record count is
// (end - source) >> shift, the records are copied to `destination` and `at` advances; the tail from `at` to the
// end pointer is handed to `builder` with a frame scratch value; the end pointer shrinks by (source -
// destination) rounded down to the record size, and v0 = the original destination. A zero or negative count
// skips the copy but runs the tail.
void move_record_range(Runtime &rt, AllegrexContext &ctx, u32 builder, u32 shift, u32 record_bytes,
                       u32 round_mask) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const s32 count = static_cast<s32>(word(rt, self + 4u) - source) >> shift;
    u32 at = destination;
    u32 from = source;
    if (count > 0) {
        for (s32 i = 0; i < count; ++i) {
            for (u32 w = 0; w < record_bytes; w += 4u) put(rt, at + w, word(rt, from + w));
            at += record_bytes;
            from += record_bytes;
        }
    }
    call(rt, ctx, builder, {at, word(rt, self + 4u), frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((source - destination) & round_mask));
    ctx.gpr[2] = destination;
}

void func_08B89338(Runtime &rt, AllegrexContext &ctx) { move_record_range(rt, ctx, 0x08B89328u, 4u, 16u, 0xFFFFFFF0u); }
void func_08C49B00(Runtime &rt, AllegrexContext &ctx) { move_record_range(rt, ctx, 0x08C470F4u, 3u, 8u, 0xFFFFFFF8u); }

// 0x08BF7980 (a0 = an owner passed on, a1 = a list node): a recursive free of a singly linked list: the node's
// word +0xC is freed first (the same function), the reference at +0x14 is released (func_08A5B9EC), the block at
// +0x10 is freed through func_08B60804 (-1) and func_08B8542C when func_08B60804 does not answer > 0 and the
// block is not the shared empty 0x08D74FD0, and the node (0x18 bytes, align 4) is released with func_08A5BC54;
// the walk continues with the word at +8.

// 0x08C20F98 (a0 = object with a word at +8, a1 = a holder): func_08A6158C(a1) and func_08A6159C(a1, object + 8)
// produce the pair func_08A61BC0(sp + 4, sp) turns into a frame value (sp = a1); the first one's word is reported
// when null (0x08CB0284) and read anyway; its virtual +0xD0 is called with (it, sp + 4, a0 + 8, the second
// answer), then func_08A5B9EC releases sp + 4.

// 0x08BC48B4: the function-local static at 0x08D765D4 with its guard byte at 0x08D76190: when the byte is set
// the address answers at once; otherwise func_08B6441C(0x08D76190) must answer non-zero, the block's vtable
// (0x08CC4FB0), the two vtables (0x08C8A47C at +8, 0) and its counted words are stored, func_08B64494 ends the
// construction and v0 = 0x08D765D4 on every path.
void func_08BC48B4(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kStatic = 0x08D765D4u;
    GuestFrame frame(ctx, 0x10u);
    if (signed_byte(rt, 0x08D76190u) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D76190u}) != 0u) {
            put(rt, kStatic + 8u, 0x08C8A47Cu);
            put(rt, kStatic + 4u, 0u);
            put(rt, kStatic + 0xCu, 0u);
            put(rt, kStatic + 0x14u, 0u);
            put(rt, kStatic + 0x24u, 0u);
            put(rt, kStatic + 0x10u, 0u);
            put(rt, kStatic + 0x20u, 0u);
            put(rt, kStatic + 0x1Cu, 0u);
            put(rt, kStatic + 0x2Cu, 0u);
            put(rt, kStatic, 0x08CC4FB0u);
            put(rt, kStatic + 0x30u, 0u);
            call(rt, ctx, 0x08B64494u);
        }
    }
    ctx.gpr[2] = kStatic;
}

// --- 180-byte group ---------------------------------------------------------------------------------------
// 0x088062D0 (a0 = a pointer to the object pointer): three method registrations func_08B87108(holder, name,
// handler) with the holder re-read (and reported through func_08A5BB7C(0x08C97DCC) when null) before each; the
// third is a tail call.
void func_088062D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        object = word(rt, self);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97DCCu});
        call(rt, ctx, 0x08B87108u, {object, 0x08C97E24u, 0x08806FA0u});
        object = word(rt, self);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97DCCu});
        call(rt, ctx, 0x08B87108u, {object, 0x08C97E30u, 0x0880714Cu});
        object = word(rt, self);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97DCCu});
    }
    tail_call(ctx, 0x08B87108u, {object, 0x08C97E3Cu, 0x0880602Cu});
}

// 0x08821558 (a0 = a pointer to the object pointer, a1 = a value, a2 = a byte selector): a frame pair is filled
// with a tag (0x08CBE7B0 when a2, else 0x08CBE7E0) and a1, handed to the object's func_08A997D0, and retagged
// 0x08CBE7C8; a null object is reported through func_08A5BB7C(0x08C99660) and used. v0 = 0x08CBE7C8.

// 0x0882F5C0 (a0 = destination): func_088297A8(sp + 4) fills a holder (reported through
// func_08A5BB7C(0x08C9A2F8) when null), func_088294D0 turns it into an object that is released (func_08A5B9EC)
// right away; func_0885E9D8(object) answers a 64-bit value whose (-1, -1) leaves v0 = -1, else it is asked
// again and func_08A5DEE8(sp, -1, low, high) builds a value object that func_08A5BAD8 assigns to a0 unless it is
// the frame address; func_08A5B9EC releases it.

// A 64-bit value printed through a value object: 0x088418B4(sp + 4) fills a holder (reported through
// func_08A5BB7C(0x08C9C584) when null), func_0884177C(object) is released at once, and func_0888EB9C(object)'s
// object holds a big-endian 64-bit value at `offset` (two wsbw byte swaps); func_08A83160(sp, a1, low, high)
// builds the value object that func_08A5BAD8 assigns to a0 unless it is the frame address; func_08A5B9EC ends it.
void print_int64(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const u32 out = ctx.gpr[4];
    const u32 width = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x088418B4u, {frame.at(4u)});
    const u32 holder = word(rt, frame.at(4u));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9C584u});
    const u32 object = call(rt, ctx, 0x0884177Cu, {holder});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const u32 value = call(rt, ctx, 0x0888EB9Cu, {object});
    const u32 high = swap_bytes(word(rt, value + offset));
    const u32 low = swap_bytes(word(rt, value + offset + 4u));
    call(rt, ctx, 0x08A83160u, {frame.at(0u), width, low, high});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}


// 0x088549C4 (a0 = destination, a1 = the first key, a3 = the second key): two lookups, each func_088547C4(cell,
// key) + func_08854678(holder) (reported through func_08A5BB7C(0x08C9E188) when null) + func_08A5B9EC(cell), then
// func_088A6C48(first, second) combines them.

// 0x088E5A34 (a0 = object with the owner at +0, a child at +4): func_088E59A0(a0) and func_088E57CC(a0) clear it;
// when the owner has a child at +0xA4, func_08B2E2B8(0x4E20) runs, the child's virtual +0x2C is called and the
// child cleared; the owner's +0xA4 is cleared again; a member at +4 has its virtual +4 called; a non-null owner
// ends the function with its virtual +4 as a tail call.

// 0x089061F4 (a0 = a holder of five members at +4..+0x14, f12 = a value): the value is scaled by 1000, added to
// the word accumulator at 0x08CE9F2C and stored at 0x08CE9F28; each member is passed to func_08905984 and then
// to func_089061D8; the function ends with a tail call of func_08905EB4(a0, -1) with f12 = the incoming value.

// 0x089CF190 (a0 = object, a1 = a value): func_08969188(sp, 0x08CF6ED8) then func_089CDDF8(a0, a1, sp) and
// func_08A5B9EC(sp); when the pending index at +0x1A0 is non-negative and differs from the one at +0x198,
// func_089CF134 gates an optional func_089CE6F0(a0, pending) and func_089CCA54(a0)'s answer goes to
// func_089CE6F0 with the low bit of +0x14C, raising +0x1C0; the pending index becomes -1 and v0 = -1.
void func_089CF190(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08969188u, {frame.at(0u), 0x08CF6ED8u});
    call(rt, ctx, 0x089CDDF8u, {self, argument, frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    const s32 pending = static_cast<s32>(word(rt, self + 0x1A0u));
    if (pending >= 0 && word(rt, self + 0x198u) != static_cast<u32>(pending)) {
        if (call(rt, ctx, 0x089CF134u, {self}) != 0u) {
            call(rt, ctx, 0x089CE6F0u, {self, word(rt, self + 0x1A0u), 0u});
        }
        const u32 value = call(rt, ctx, 0x089CCA54u, {self});
        call(rt, ctx, 0x089CE6F0u, {self, value, word(rt, self + 0x14Cu) & 1u});
        put(rt, self + 0x1C0u, 1u);
    }
    put(rt, self + 0x1A0u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x089E6EEC: the same shape as 0x089CF190 with the indices at +0x1A4 / +0x19C, its own gates
// (func_089E6218, func_089E6E60, func_089E6D0C twice with the low bit of +0x150) and the flag at +0x1D8.
void func_089E6EEC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08969188u, {frame.at(0u), 0x08CF6ED8u});
    call(rt, ctx, 0x089E6218u, {self, argument, frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    const s32 pending = static_cast<s32>(word(rt, self + 0x1A4u));
    if (pending >= 0 && word(rt, self + 0x19Cu) != static_cast<u32>(pending)) {
        if (call(rt, ctx, 0x089E6E60u, {self}) != 0u) {
            call(rt, ctx, 0x089E6D0Cu, {self, word(rt, self + 0x1A4u), 0u});
        }
        const u32 index = word(rt, self + 0x19Cu);
        if (static_cast<s32>(index) >= 0) {
            call(rt, ctx, 0x089E6D0Cu, {self, index, word(rt, self + 0x150u) & 1u});
        }
        put(rt, self + 0x1D8u, 1u);
    }
    put(rt, self + 0x1A4u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08A1F520 (a0 = unused here, a1 = a selector, a2 = a pointer to the holder pointer): selector 0 calls
// func_08A4D2F0(0x08CF68E8) and returns its answer, any selector other than 1 answers 1, and 1 releases the
// holder's object: its virtual +0xC writes into sp + 4, func_08A4D2F0(0x08CF68E8, sp[1]) runs, and the block at
// sp[1] (reported through func_08A5BB7C(0x08CAA480) when the holder is null) is freed through func_08B60804(-1)
// and func_08B8542C unless it is the shared empty 0x08D74FD0 and func_08B60804 answers > 0.
void func_08A1F520(Runtime &rt, AllegrexContext &ctx) {
    const u32 which = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    if (which == 0u) {
        call(rt, ctx, 0x08A4D2F0u, {word(rt, 0x08CF68E8u)});
        return;
    }
    if (which != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 object = word(rt, ctx.gpr[6]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {frame.at(4u), object});
    call(rt, ctx, 0x08A4D2F0u, {word(rt, 0x08CF68E8u), word(rt, frame.at(4u))});
    const u32 block = word(rt, frame.at(4u));
    const u32 inner = block - 0xCu;
    if (inner != 0x08D74FD0u &&
        static_cast<s32>(call(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {inner, frame.at(0u)});
    }
}

// 0x08AC6B24: six records of 0x1C bytes from 0x08CFFD90 are torn down (func_08B28C74 on the word at +4, the byte
// at +0xC cleared, func_08B1B99C on the word at +0, then func_08B28BE0 + func_08B64654 on the words at +4 and
// +8 when non-null); the function ends with a tail call of func_08B35254(func_08B2B6F4(0x08CFFD58)).

// 0x08ACEA8C (a0 = unused, a1 = the object at +0xD8 float / +0xE4 buffer / +0xE8 handle / +0xED byte):
// func_08B35840(0x40, 0x2000) allocates the buffer (stored at +0xE4) that func_08B36018 clears and func_08AF0508
// (0, 2, buffer, 0x2000) registers; func_08AF0A74(0, f12 = f13 = the float at +0xD8) starts it, the byte at
// +0xED is set, a four-word descriptor on the frame (0, 0x2800, 0x08ACE6D4, the object) goes to
// func_08B1B92C(descriptor + 8, descriptor, func_08B2E134(func_08B2E314(3, -1)), 0) whose answer is the handle at
// +0xE8.

// 0x08B1A9E4: the shutdown walk. The byte 0x08D689E0 and (inside the lock 0x08D689E4) 0x08D68A2F are set; when
// 0x08D68A2F was clear, every node of the list at 0x08D773DC calls its entry's callback at [word(node + 4) + 8]
// with [entry + 0xC] and the finalizer at 0x08D68A28 (a tail call when non-null). v0 = 0x00090000 otherwise.

// 0x08B28470 (a0 = object with a buffer at +0x5C and a length at +0x64): func_08B273CC, func_08B27384(a0) and
// func_08B27460(a0) start it; while bytes remain, func_08B2811C(a0, cursor, remaining) consumes them (a negative
// answer answers 0); then func_08B273A0(a0, 0x100) gates func_08B2740C / func_08B27780 / func_08B27C28, whose
// non-zero answer is the result. v0 = 0 or 1.

// 0x08B30110 (a0 = object with a thread handle at +0xC and a block at +0x964C): a negative handle returns it;
// else func_08C7AE78(handle), func_08B2E0B0()'s answer kept for later, func_08B35214()'s answer is locked
// (func_08B344E0), func_08C7AEB8(handle) runs, func_08B2F17C(0x4C00, 0) allocates the block when it is null
// (then func_08B35238(0)), the lock is released (func_08B3457C) and func_08B2E0A4 undoes func_08B2E0B0; the
// handle becomes -1.

// 0x08B63924 (a0 = object with a word at +8, a length at +0xC and a kind byte at +0x14, a1 = a count): the kind
// (0xFF means "no scaling") selects the shift (kind & 7: 0 and 3 scale by 4, 2 by 2, 4 by 8, 1 and 5..7 report
// through func_08B77B04, which never returns — the original loops there) and func_08B636A0(kind, word +8,
// word +0xC - count * scale, sp) fills the frame word that answers.
void func_08B63924(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 kind = rt.memory().load8(self + 0x14u);
    u32 shift = 0u;
    if (kind != 0xFFu) {
        const u32 low = kind & 7u;
        if (low == 0u || low == 3u) {
            shift = 2u;
        } else if (low == 2u) {
            shift = 1u;
        } else if (low == 4u) {
            shift = 3u;
        } else {
            call(rt, ctx, 0x08B77B04u);
            return;  // the original loops on the report; it never returns
        }
    }
    call(rt, ctx, 0x08B636A0u,
         {kind, word(rt, self + 8u), word(rt, self + 0xCu) - (count << shift), frame.at(0u)});
    ctx.gpr[2] = word(rt, frame.at(0u));
}

// 0x08BB0D40 (a0 = first, a1 = end, a2 = descriptor: function at +0, stride at +4): for each 0x400-byte block
// from a0 to a1 the descriptor's function is called with (block + stride/2); a set low bit of the stride makes
// the target [word(argument) + function] instead. v0 = the function, v1 = the stride.

// 0x08BE864C (a0 = first, a1 = end, a2 = destination): copies 0x40-byte records from a0 to the destination
// (words at +4 and +8, the float words +0xC..+0x1C and the vtable 0x08CCFC90 at +0) while both advance, calling
// func_089FA280 on the sub-objects at +0x20 and +0x30; a null destination is skipped. v0 = the destination.

// A tagged callback pair (a0 = destination, a1 = an object with the pairs at +0x14/+0x18 and +0x1C/+0x20,
// a2 = a base pointer). Each pair is a tagged pointer: a clear low bit calls the word at +0x14 (resp. +0x1C)
// with base + (tag >> 1), a set one calls [word(base + (tag >> 1)) + that word]; the first call also gets the
// pair's own word in a1. func_0896418C(destination, second answer) runs, the word at +0x10 of the second answer
// is copied to the destination's +0x10, and v0 = the destination (three instances: 0x08BF0570, 0x08C08E1C,
// 0x08C13230).
void tagged_pair_call(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 base = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 first_tag = word(rt, holder + 0x18u);
    const u32 first_base = word(rt, holder + 0x14u);
    const u32 first_argument = base + static_cast<u32>(static_cast<s32>(first_tag) >> 1);
    u32 first_target = first_base;
    if ((first_tag & 1u) != 0u) first_target = word(rt, word(rt, first_argument) + first_base);
    const u32 first = call_indirect(rt, ctx, first_target, {first_argument, first_base});
    const u32 second_tag = word(rt, holder + 0x20u);
    const u32 second_base = word(rt, holder + 0x1Cu);
    const u32 second_argument = first + static_cast<u32>(static_cast<s32>(second_tag) >> 1);
    u32 second_target = second_base;
    if ((second_tag & 1u) != 0u) second_target = word(rt, word(rt, second_argument) + second_base);
    const u32 second = call_indirect(rt, ctx, second_target, {second_argument, first});
    call(rt, ctx, 0x0896418Cu, {self, second});
    put(rt, self + 0x10u, word(rt, second + 0x10u));
    ctx.gpr[2] = self;
}


// 0x08BEBF8C: the same tagged pair as 0x08BF0570, but the answer is the destination with the second answer's
// two words at +0 and +4 copied into it.

// 0x08C18D28 (a0 = the sentinel of a circular list): every node up to the sentinel is freed: the word at +0xC
// through func_08A5B930, the block at +8 through func_08B60804(-1) and func_08B8542C unless it is the shared
// empty 0x08D74FD0 and func_08B60804 answers > 0, and the node (0x10 bytes, align 4) with func_08A5BC54.

// 0x08C53ED0 (a0 = a rope node, a1 = an index): the byte at the index of the rope. A node with data at +0x10
// answers its byte; kind 0 (byte +0xC) answers node[0x18][index]; kind 1 descends into [node+0x18] while the
// index is below its length, else into [node+0x1C] with the length subtracted; kinds 2 and 3 call the virtual at
// [node+0x18]'s +8 with (that object, index, 1, sp) and answer the byte it wrote to sp. Kinds >= 4 loop on the
// kind test in the original (never returning); this version treats them as the 2/3 case.
s32 rope_byte(Runtime &rt, AllegrexContext &ctx, u32 node, u32 index) {
    GuestFrame frame(ctx, 0x20u);
    for (;;) {
        const u32 data = word(rt, node + 0x10u);
        if (data != 0u) return signed_byte(rt, data + index);
        const u32 kind = rt.memory().load8(node + 0xCu) & 0xFFu;
        if (kind == 0u) return signed_byte(rt, word(rt, node + 0x18u) + index);
        if (kind == 1u) {
            const u32 inner = word(rt, node + 0x18u);
            const u32 length = word(rt, inner);
            if (index < length) {
                node = inner;
            } else {
                node = word(rt, node + 0x1Cu);
                index -= length;
            }
            continue;
        }
        const u32 extractor = word(rt, node + 0x18u);
        call_indirect(rt, ctx, word(rt, word(rt, extractor) + 8u), {extractor, index, 1u, frame.at(0u)});
        return signed_byte(rt, frame.at(0u));
    }
}


// 0x08C54C20 (a0 = the answer, a1 = an object with a child at +0x78, a2 = a value): func_08B5C450(a0, a1 + 0x6C),
// func_08B6594C(sp + 4, a1, a2) with func_08B5D34C(sp + 4), func_08C548C4(a1, a2), and — when the child exists —
// func_08B5C450(sp, child + 0x1C), the child's virtual +8(a1 = a2), func_08B5D1E0(child + 0x1C, a2) and
// func_08B5D34C(sp). v0 = a0.

// --- 184-byte group ---------------------------------------------------------------------------------------
// 0x0885DF24 (a0 = object with a member at +0x5B4 and a table at +0x5BC, a1/a2/a3/t0 = the descriptor,
// t1 = a byte): func_08896CD8(a0 + 0x5BC, a3, a1, t0, t1) creates the node; on success its word +4 is a2,
// func_088F9480(a0's +0x5B4) runs, func_0889717C(a3) gates func_088F8924(node), and the byte at +0x50 is t1.
void func_0885DF24(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const u32 flag = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    const u32 node = call(rt, ctx, 0x08896CD8u, {self + 0x5BCu, third, first, fourth, flag});
    if (node == 0u) return;
    put(rt, node + 4u, second);
    call(rt, ctx, 0x088F9480u, {word(rt, self + 0x5B4u)});
    if (call(rt, ctx, 0x0889717Cu, {third}) != 0u) call(rt, ctx, 0x088F8924u, {node});
    rt.memory().store8(node + 0x50u, static_cast<std::uint8_t>(flag));
}

// 0x0887F704 (a0 = object): the destructor: vtable 0x08CC11C8 at +0, 0x08CC1204 at +0x70, func_0886AA0C five
// times with 5, 2, 4, 6 and 3, the member at +0x78 released through its virtual 0 (a1 = the word at +0x74) and
// its slots cleared, the vtable 0x08CC1180 at +0x70, func_0886A858(a0), and a tail call of func_08B64654(a0).

// 0x0888B748 (a0 = object with a table member at +4 and a reporter at +0x18, a1 = a key): func_088DA588(key),
// func_088DA788(key) (whose +0x54 is a kind), and when the first answer's byte +8 is clear, func_088DC858 is
// null, func_088DB2C8 is non-null and the kind is 5, a descriptor (0x23, the key at +8) goes to
// func_088936A4(reporter, sp). Then func_088DA588(key)'s object has its virtual +0x1C called with (it, 0).

// 0x088A1A3C (a0 = object with a s16 table at +0x10, a record base at +8 and a second table at +0x4C;
// a1 = an index, a2 = a value, a3 = a byte): the entry's s16 (max with 0) is `n`; the record at
// base + n * 0x430 + 8 gets a2 through func_088A5814 (a3) or func_088A6C48, and the second table's
// n * 0x690 element goes through func_088A6068; v0 = the record.

// 0x088E7A04 (a0 = object with the world at +0, a1 = a value): func_088D414C(sp, world's +0x620, a1, 2) fills
// the frame; a clear byte at sp + 0x20 takes func_088E798C(a0, a1, sp), else func_088E7894(a0, a1, sp, sp + 0xC)
// with two zero floats. Then the world's +0x14C object's func_088F8D58 answer has its virtual +0x1C and then +0x10
// called with (it, 1).

// 0x08937CA0 (a0 = the answer, a1/a2/a3 = the caller's other arguments, t0 = 1): func_08B10170(sp + 0x40,
// a1, t0 = 1) fills a 0x3C-byte record that is copied down to sp (0x30 bytes in 0x10-byte steps, then three
// words); a2 lands at +4 and a3 at +8 of the answer; func_08937B0C(answer, copied word at sp + 0x34) runs and a
// non-zero answer makes v0 = a3, else -1.

// 0x08978BBC (a0 = object): the destructor: vtable 0x08CBB740 at +0; the four references at +0x1A8..+0x19C are
// released with func_08A5B9EC; func_08BEDDC4 on +0x18C and +0x184; the block at +0x74 (its end at +0x84 rounded
// down to words) is released with func_08A5BC54; func_08B28BE0(+0x48), func_08B28DC4(+0x20) and
// func_08BEDD64(+0x18); a tail call of func_08A780B0(a0).

// 0x089A22EC (a0 = destination, a1 = a value object): a frame value object (vtable 0x08CCD9F0, tag 0x08D74FDC,
// -1, sp + 4) takes the object's virtual +8 (a0 = the object, a1 = sp + 8); then the cell at sp + 0xC is freed
// through func_08B60804(-1) and func_08B8542C unless it is the shared empty 0x08D74FD0, and func_089A0A40(a0,
// f12 = the float at sp + 4) runs.

// 0x089BA420 (a0 = object, a1 = an argument object): when the object's virtual +0x178 answers non-zero, the
// argument object's word +0x17C is called with (a1, the same virtual's answer); the same for virtual +0x16C and
// the word at +0x170, which is the function's tail call.
void func_089BA420(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (call_indirect(rt, ctx, word(rt, word(rt, self) + 0x178u), {self}) != 0u) {
        const u32 handler = word(rt, word(rt, other) + 0x17Cu);
        const u32 argument = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x178u), {self});
        call_indirect(rt, ctx, handler, {other, argument});
    }
    if (call_indirect(rt, ctx, word(rt, word(rt, self) + 0x16Cu), {self}) != 0u) {
        const u32 handler = word(rt, word(rt, other) + 0x170u);
        const u32 argument = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x16Cu), {self});
        tail_call(ctx, handler, {other, argument});
    }
}

// An optional-value test (three instances: 0x08A2AB38, 0x08A2ABF0, 0x08A2ACA8; a0 = destination): a container is
// built at sp + 8 (func_08ACC220), `test` answers whether it holds a value — when it does, func_08A2AA84(sp + 8)
// runs first — func_08A97A6C(cell, bool) builds the result value object at sp + 4 (empty) or sp (holds one), which
// func_08A5BAD8 assigns to a0 unless it is that cell, func_08A5B9EC releases it and func_08ACC0F4 destroys the
// container. v0 = the last call's answer.
void optional_value_test(Runtime &rt, AllegrexContext &ctx, u32 test) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08ACC220u, {frame.at(8u)});
    const bool has_value = call(rt, ctx, test, {frame.at(8u)}) != 0u;
    const u32 cell = has_value ? frame.at(0u) : frame.at(4u);
    if (has_value) call(rt, ctx, 0x08A2AA84u, {frame.at(8u)});
    call(rt, ctx, 0x08A97A6Cu, {cell, has_value ? 1u : 0u});
    if (out != cell) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, cell)});
    call(rt, ctx, 0x08A5B9ECu, {cell});
    call(rt, ctx, 0x08ACC0F4u, {frame.at(8u)});
}


// 0x08A33C70 (a0 = destination): func_08B2DEE4()'s 2-word answer goes to func_08B1B740, whose two answers fill the
// frame's four words (+4/+8 and +0x10/+0x14); func_08A337A8(sp) fills the object at sp (reported through
// func_08A5BB7C(0x08CACCEC) when null and read anyway), whose +0x14 / +0x18 take the two words; func_08A5BAD8
// assigns it to a0 unless it is the frame address, and func_08A5B9EC releases it.

// 0x08A3523C (a0 = object with the target at +0x18, a1 = a first value, a2/a3, t0 = a byte, f12 = a float):
// func_08A3A73C(target, a1, 0) runs, the target is re-read (reported through func_08A5BB7C(0x08CACEC8) when null
// both times), and the function ends with a tail call of func_08A3A5BC(target, a2, a3, t0) with the same float.

// 0x08AB5620 (a0 = object with the vtable word at +0, a1 = a pointer to the holder pointer): the holder's object
// (null answers with no work) has its virtual +0xDC called with (it, 0); the holder is re-read (reported through
// func_08A5BB7C(0x08CB3EC4) when null), its virtual +0xD8 called, and the object's word +0x108 handler is the
// tail call with (a0, the first answer, the second).

// 0x08ABD034 (a0 = object): the state word at +0x10 takes func_08B2E750(a0)'s answer; the object's virtual +0x2C
// runs, then virtal +8 while it answers non-zero: virtual +0x34, virtual +0x38, virtual +8 again and, when it is
// still non-zero, virtual +0x3C before the loop repeats. Virtual +0x30 ends the function and +0x10 becomes -1.

// 0x08AC7EDC (a0 = object with a member at +4, a state byte at +0xC, a worker at +8, a callback at +0x10, its
// argument at +0x14 and a reporter at +0x18): until the byte at +0xC is clear: func_08B2E0B0(),
// func_08B28C44(a0's +4), func_08B34888(worker), func_08B34B24(worker), func_08B2E0EC(reporter), the callback
// (a0 = its argument) when non-null, the byte at +0xD cleared, then func_08B34888 / func_08B28BE8 / func_08B34B24
// on the worker again.

// 0x08B301C4 (a0 = object with a state byte at +4, a lock at +0x8FC and a handle at +0x12A8): func_08B30110(a0)
// then, when the byte at +4 is set, the func_08B2E0B0 / func_08B35214 / func_08B344E0 lock, func_08C7AE88,
// func_08B2F17C(0x1800, 1) stored at 0x08D69164, func_08B3457C and func_08B2E0A4; then the lock at +0x8FC,
// func_08B2F548(a0), the handle -1, the byte at +4 and the word at +0x14 cleared, and a tail call of
// func_08B3457C(lock).

// 0x08B62260 (a0 = an argument passed on): func_08B62178's holder (null ends the function) holds an object whose
// words +0x30/+0x34 must be the C++ ABI id (0x432B2B00 / 0x474E5543): otherwise the holder is cleared and the
// object's +0x30 goes to func_08B733F0 as a tail call. With the id, the refcount at +0x14 is bumped (a -1 wraps
// to 0, and then the holder takes the word at +0x10) or decremented: a zero count hands the string at +0x30 to
// func_08B733F0 and a count of 1 takes the reported path (func_08B6459C).

// A range of counted references (two instances: 0x08B8ECAC and 0x08BA8FB0; a0 = first, a1 = end, records of
// 8 bytes): each record's object at +4 is skipped when null; its counters at +4 and +8 are decremented (the
// original's ll/sc pairs, a plain read-modify-write here) and a counter that reaches zero calls the object's
// virtual +8 (first) or +0xC (second) with the object.
void release_reference_range(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    for (u32 at = begin; at != end; at += 8u) {
        const u32 object = word(rt, at + 4u);
        if (object == 0u) continue;
        const u32 first = word(rt, object + 4u);
        put(rt, object + 4u, first - 1u);
        if (first != 1u) continue;
        call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
        const u32 second = word(rt, object + 8u);
        put(rt, object + 8u, second - 1u);
        if (second != 1u) continue;
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
    }
}


// 0x08BBC804 (a0 = unused, a1 = a pointer to the holder, a2 = the source): func_08B62344(0x4B0) allocates the
// object, its vtable 0x08CC43E8 is stored and 0x4AC bytes (0x4A0 in 0x10-byte steps plus three words) are copied
// from the source; the holder's previous object, when non-null, is released through its virtual +4 as a tail call.

// A tagged pair whose second half is the tail call (three instances: 0x08C25A3C, 0x08C2CDC4 and 0x08C37E8C;
// a0 = the pair object, a2 = a pointer to the argument object, t0 = the first free argument): `prepare(t0, a1)`
// runs first, then the two tagged calls described at tagged_pair_call (the pairs at +0x14/+0x18 and +0x1C/+0x20,
// the argument object's word as the base) and the second target is the tail call with (its argument, the first
// answer).
void tagged_pair_tail(Runtime &rt, AllegrexContext &ctx, u32 prepare) {
    const u32 self = ctx.gpr[4];
    const u32 base = word(rt, ctx.gpr[6]);
    GuestFrame frame(ctx, 0x10u);
    const u32 prepared = call(rt, ctx, prepare, {ctx.gpr[8], ctx.gpr[5]});
    const u32 first_tag = word(rt, self + 0x18u);
    const u32 first_base = word(rt, self + 0x14u);
    const u32 first_argument = base + static_cast<u32>(static_cast<s32>(first_tag) >> 1);
    u32 first_target = first_base;
    if ((first_tag & 1u) != 0u) first_target = word(rt, word(rt, first_argument) + first_base);
    const u32 first = call_indirect(rt, ctx, first_target, {first_argument, first_tag});
    const u32 second_tag = word(rt, self + 0x20u);
    const u32 second_base = word(rt, self + 0x1Cu);
    const u32 second_argument = first + static_cast<u32>(static_cast<s32>(second_tag) >> 1);
    u32 second_target = second_base;
    if ((second_tag & 1u) != 0u) second_target = word(rt, word(rt, second_argument) + second_base);
    tail_call(ctx, second_target, {second_argument, prepared});
}


// 0x08C491E4 (a0/a1 = the range of s16 indices, a2 = the pivot index as a s16, a3 = a descriptor: a fixed byte
// offset at +0 and the table pointer at +4): the partition step of a sort over records of 0xF0 bytes whose float
// at +0x98 is the key: the low index advances while its key is below the pivot's, the high one steps back while
// the pivot is below its key; crossing ends the function with the low pointer, else the two indices are swapped
// and the walk repeats (the range builder in a3's base is re-read every time).

// 0x08C49D08 (a0 = a vector of 6-byte records with its end word at +4, a1 = destination, a2 = source): the record
// count is ((end - source) >> 1) * 0xAAAAAAAB (the asm's division by 6); each record's three halfwords are copied
// down; func_08C470A4(tail, end, sp) builds the range and the end word shrinks by (source - destination) rounded
// down to 2; v0 = the destination.
void func_08C49D08(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 halved = static_cast<u32>(static_cast<s32>(word(rt, self + 4u) - source) >> 1);
    const s32 count = static_cast<s32>(halved * 0xAAAAAAABu);
    u32 at = destination;
    u32 from = source;
    if (count > 0) {
        for (s32 i = 0; i < count; ++i) {
            for (u32 w = 0; w < 6u; w += 2u) {
                rt.memory().store16(at + w, rt.memory().load16(from + w));
            }
            at += 6u;
            from += 6u;
        }
    }
    call(rt, ctx, 0x08C470A4u, {at, word(rt, self + 4u), frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((source - destination) & 0xFFFFFFFEu));
    ctx.gpr[2] = destination;
}

// 0x08C56A90 (a0 = object, a1 = the lookup key, a2 = the value): the word at +4 is cleared and
// func_08C55FDC(sp, a0, 1) decides; when the byte at sp is set, the object at [a0's vtable offset + 0x78] has its
// virtual +0x20 called with (it, key, value) and the answer is stored at +4 — when the value differs, the same
// adjusted object's +0x14 word (with bit 2 set) goes to func_08C54CFC. v0 = a0.

// 0x08C72CF8 (a0 = destination, a1 = a pointer to the object pointer, a2 = the 8-word source): the object's
// virtual +0x14 is called with (sp + 0x20, a1, sp) after the source's eight words are copied to sp, and the eight
// words it leaves at sp + 0x20 go to the destination. v0 = destination.

// --- 188-byte group ---------------------------------------------------------------------------------------
// A counted-range description (two instances: 0x0881888C subtracts the range's ends, 0x08818948 adds them; a0 =
// destination, a1 = a count, a2 = a pointer to the holder pointer): with a positive count the holder (reported
// through func_08A5BB7C(0x08C98D34) when null) answers the range's end through its virtual +0x3C and the start
// through func_08817924, and func_08A85B1C(sp + 4, end - start) describes it; otherwise func_08A76AAC(sp,
// 0x08CF6ED8) builds an empty description. func_08A5BAD8 assigns it to a0 unless it is its own frame cell and
// func_08A5B9EC releases it.
void counted_range(Runtime &rt, AllegrexContext &ctx, bool sum) {
    const u32 out = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 holder_ref = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    u32 cell = frame.at(0u);
    if (count > 0) {
        cell = frame.at(4u);
        const u32 object = word(rt, holder_ref);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
        const u32 end = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
        const u32 begin = call(rt, ctx, 0x08817924u, {object});
        call(rt, ctx, 0x08A85B1Cu, {cell, sum ? end + begin : end - begin});
    } else {
        call(rt, ctx, 0x08A76AACu, {cell, 0x08CF6ED8u});
    }
    if (out != cell) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, cell)});
    call(rt, ctx, 0x08A5B9ECu, {cell});
}

void func_08818948(Runtime &rt, AllegrexContext &ctx) { counted_range(rt, ctx, true); }

// 0x0888CFCC (a0 = object with a table at +4 and a reporter at +0x18, a1 = a key, a2 = a flag, a3 = a source,
// t0 = an optional text holder): a descriptor at sp (kind 2, the key, the flag as a byte, the source's words at
// +8 / +0x10 / +0x14) goes through func_088A5EE0(a3 + 8, a1, flag, a3); a non-null t0 has func_08896FB8(t0 + 0x20)
// copied into the descriptor's 0x80-byte text (else the byte at sp + 0x18 is cleared); func_088936A4(reporter, sp)
// reports it, and func_088DA588(table, key)'s object has its virtual +0x10 called with (it, 0).

} // namespace
} // namespace new_batch_main_14
} // namespace gt::game

#define MAIN14(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_14::name, gt::ReturnKind::kind)
MAIN14(0x08A730D8u, func_08A730D8, Word);
MAIN14(0x08A87C28u, func_08A87C28, Word);
MAIN14(0x08AB173Cu, func_08AB173C, Word);
MAIN14(0x08B63574u, func_08B63574, Word);
MAIN14(0x08B89338u, func_08B89338, Word);
MAIN14(0x08BC48B4u, func_08BC48B4, Word);
MAIN14(0x08C49B00u, func_08C49B00, Word);
MAIN14(0x088062D0u, func_088062D0, Word);
MAIN14(0x089CF190u, func_089CF190, Word);
MAIN14(0x089E6EECu, func_089E6EEC, Word);
MAIN14(0x08A1F520u, func_08A1F520, Word);
MAIN14(0x08B63924u, func_08B63924, Word);
MAIN14(0x0885DF24u, func_0885DF24, Word);
MAIN14(0x089BA420u, func_089BA420, Word);
MAIN14(0x08C49D08u, func_08C49D08, Word);
MAIN14(0x08818948u, func_08818948, Word);
