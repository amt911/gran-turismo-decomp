// New batch (main session 947): the 43/44-instruction functions of the re-draft list, written as structured C from
// `tools.disasm`. Every body keeps the original's exits (`v0` on each path), the call order and the call's delay slot,
// arguments go in a0-a3 / t0-t3, and a callee-saved register the original keeps an argument in is a plain local here.
// Input shapes for the synthetic generator: /home/andres/repos/gt-ops/shapes/shapes_main947.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_947 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0`, `v1`, `f0` and `a1` hold what the callee left, as after the original's `jal`.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_result(rt, ctx, target, arguments).v0;
}
// `lw v0, slot(vtable); jalr v0`: the callee starts with v0 = its own address.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_result(rt, ctx, target, arguments);
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }

// --- the two map inserts ------------------------------------------------------------------------------------
// a0 = the map, a1 = a holder whose first word is the tree. The key is `key_function(map, 0)`. With no tree, zero is
// released on the map (func_08A5BAD8) and the answer is 1. With a tree, the key is looked up (func_08A76F20): found ->
// the holder's tree is released and the answer is 1; not found -> func_08A76AA0 puts a new node in the frame's first
// word, released when non-zero, the temporary is destroyed (func_08A5B9EC), and the answer is 1 when a node came
// back, else 0.
void map_insert(Runtime &rt, AllegrexContext &ctx, u32 key_function) {
    const u32 map = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    u32 inserted = 1u;
    const u32 tree = word(rt, holder);
    if (tree == 0u) {
        call(rt, ctx, 0x08A5BAD8u, {map, 0u});
        ctx.gpr[2] = inserted;
        return;
    }
    const u32 key = call(rt, ctx, key_function, {map, 0u});
    if (call(rt, ctx, 0x08A76F20u, {tree, key}) != 0u) {
        call(rt, ctx, 0x08A5BAD8u, {map, word(rt, holder)});
        ctx.gpr[2] = inserted;
        return;
    }
    inserted = 0u;  // the delay slot of the call below
    call(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    const u32 node = word(rt, frame.at(0u));
    if (node != 0u) {
        inserted = 1u;  // the delay slot of the call below
        call(rt, ctx, 0x08A5BAD8u, {map, node});
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = inserted;
}

// --- the member-pair dispatch (5 addresses) -----------------------------------------------------------------
// a0 = the result holder, a1 = two (offset, function) pairs, a2 = the base object. The packed word at +0x18 resp.
// +0x20 has bit 0 set when the call is indirect, and its upper bits are an offset (arithmetic, over 2) added to a2
// (first) resp. to the first result (second). The target is the word at +0x14 resp. +0x1C, or the virtual at that
// word of the adjusted object. `last_call` runs with a0 = the holder and a1 = the second result, and v0 = the holder.
void dispatch_pair(Runtime &rt, AllegrexContext &ctx, u32 last_call) {
    const u32 holder = ctx.gpr[4];
    const u32 pairs = ctx.gpr[5];
    const u32 base = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 first_offset = word(rt, pairs + 0x18u);
    const u32 first_object = base + static_cast<u32>(signed_of(first_offset) >> 1);
    const u32 first_function = word(rt, pairs + 0x14u);
    const u32 first_target =
        (first_offset & 1u) != 0u ? word(rt, word(rt, first_object) + first_function) : first_function;
    const u32 first_result = call_virtual(rt, ctx, first_target, {first_object}).v0;
    const u32 second_offset = word(rt, pairs + 0x20u);
    const u32 second_object = first_result + static_cast<u32>(signed_of(second_offset) >> 1);
    const u32 second_function = word(rt, pairs + 0x1Cu);
    const u32 second_target =
        (second_offset & 1u) != 0u ? word(rt, word(rt, second_object) + second_function) : second_function;
    const u32 second_result = call_virtual(rt, ctx, second_target, {second_object}).v0;
    call(rt, ctx, last_call, {holder, second_result});
    ctx.gpr[2] = holder;
}

// --- the boxed-pair comparisons (6 addresses) ---------------------------------------------------------------
// a0 = the counted value (low word +0x18, high word +0x1C), a1 = destination, a3 = a pointer to the holder word.
// The holder's object is reported through func_08A5BB7C when null and then read anyway; its virtual at `slot` answers
// a 64-bit value (high in v1, low in v0), which is compared with a0's pair. `test` is the strict/non-strict
// comparison the asm performs, `sign_high` whether the high words compare signed (`slt`) or unsigned (`sltu`); the
// low words always compare unsigned. func_08A97A6C(frame, bool) builds the bool value object that func_08A5BAD8
// assigns to a1 unless it already is the frame address, and func_08A5B9EC destroys it.
enum class PairTest {
    Less,      // b < a, strict
    NotLessA,  // !(a < b), i.e. a >= b
    NotLessB,  // !(b < a), i.e. b >= a
};

void pair_compare_emit(Runtime &rt, AllegrexContext &ctx, u32 assert_text, u32 slot, PairTest test, bool sign_high) {
    const u32 pair = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 holder_ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 low = word(rt, pair + 0x18u);
    const u32 high = word(rt, pair + 0x1Cu);
    const u32 holder = word(rt, holder_ref);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_text});
    const GuestResult loaded = call_virtual(rt, ctx, word(rt, word(rt, holder) + slot), {holder});
    const auto below = [sign_high](u32 left, u32 right, u32 left_low, u32 right_low) {
        const bool high_below = sign_high ? signed_of(left) < signed_of(right) : left < right;
        return high_below || (left == right && left_low < right_low);
    };
    bool value = false;
    switch (test) {
    case PairTest::Less: value = below(loaded.v1, high, loaded.v0, low); break;
    case PairTest::NotLessA: value = !below(high, loaded.v1, low, loaded.v0); break;
    case PairTest::NotLessB: value = !below(loaded.v1, high, loaded.v0, low); break;
    }
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0u), value ? 1u : 0u, loaded.v0});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- the two frame-cell builders ----------------------------------------------------------------------------
// a0 = the destination, a1 = the argument of the first builder. Two frame cells are built (first_maker and
// func_08A3F8E8); each holder is reported when null (first_text / 0x08CADA70) and read anyway. The first's record
// name (func_08B21C64 at +0x18) goes to func_08B21858 on the second (+0x14), the second cell is assigned to a0
// (func_08A5BAD8) unless a0 already is the cell, and both cells are released (func_08A5B9EC).
void frame_pair_assign(Runtime &rt, AllegrexContext &ctx, u32 first_maker, u32 first_text) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 first_cell = frame.at(0u);
    const u32 second_cell = frame.at(4u);
    call(rt, ctx, first_maker, {first_cell, ctx.gpr[5]});
    call(rt, ctx, 0x08A3F8E8u, {second_cell});
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

// --- the one-off bodies -------------------------------------------------------------------------------------
// 0x08A142C8 (a0 = manager, a1 = a text cell): lock func_08B34888(manager + 0xAC) and wait out the word at +0x108
// (func_08B34B2C, the ll/sc pair) until it is 0, set it to 1; func_08B7C830 copies 0x1F bytes of the cell's word into
// +0x18D; unlock (func_08B34B24), func_08B28C74 on +0xD8, and the work continues in the tail call on the lock.

// 0x08A2D434 (a0 = object, a1 = a value): func_08B0979C(a0); func_08A2D338(frame, a1) builds a handle (checked
// against 0x08CAC2A8), func_08A2D1B8 reads its object, func_08B0A034 combines it with the first answer and
// func_08A97A6C stores the result in a record at frame+4; the record is assigned to a0 when a0 is not it, and the
// record then the handle are destroyed. v0 = the last destruction's answer.

// 0x08A9AF28 (a0 = the vector's owner, a1 = a wanted count): round the capacity up from 1 by doubling while it is
// below a1 + 1, then grow the vector at +0x14: func_08B85798 moves the elements when the capacity is below the count
// (begin + capacity * 4 .. end), else func_08B85F30 grows it (capacity - count). The temporary from func_08A76AA0 is
// destroyed; v0 = that destruction's answer.

// 0x08B59F6C (a0 = a table, a1 = the wanted word): a hash probe. The index byte at +0x202 (0xFF ends it) picks a
// 0x10-byte slot; its word at +8 is compared with the wanted one and the next index is the slot's byte at +3. A match
// runs func_08B59BC0(a0, +0x202, index) and func_08B59D6C(a0, index) and answers 1; no match (or a null wanted
// word) answers 0.

// 0x08BD5D54 (a0 = the map's holder, a1 = a key holder): walk the list at a0 + 0x98 while the nodes' strings (word at
// +8, length at -0xC) compare below the key's string (func_08B36170 over the shorter length); a node whose length
// equals the key's ends the walk. A non-zero compare advances to the node's next word. v0 = the node reached (a0 +
// 0x98 when the walk runs off the end).

// 0x0885FCA4 (a0 = a holder): the object at the holder's word + 0x5AC is set up by func_088D6AD0, then at most 0x1C2
// rounds wait for it: func_088D6B38 answers 1 (return 1), func_08B1A844 answers non-zero (return 0), the poll
// func_088D4C9C on its block at +4 non-zero with its byte at +0x5BC non-zero returns 0, and func_08B2E2B8(0x186A0)
// ends each round. A loop that runs out answers 0.
void func_0885FCA4(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, word(rt, holder) + 0x5ACu);
    call(rt, ctx, 0x088D6AD0u, {object});
    u32 rounds = 0u;
    for (;;) {
        ++rounds;  // the delay slot of the call below
        if (call(rt, ctx, 0x088D6B38u, {object}) != 0u) {
            ctx.gpr[2] = 1u;
            return;
        }
        if (call(rt, ctx, 0x08B1A844u) != 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 owner = word(rt, word(rt, holder) + 0x5ACu);
        if (call(rt, ctx, 0x088D4C9Cu, {owner + 4u}) != 0u) {
            const u32 field = word(rt, word(rt, holder) + 0x5ACu);
            if (byte_at(rt, field + 0x5BCu) != 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
        call(rt, ctx, 0x08B2E2B8u, {0x186A0u});
        if (rounds == 0x1C2u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
}

// 0x0886E648 (a0 = object): the destructor. Store the vtable 0x08CBF770, run func_0886A8B8(self, index 0..2, 0) and
// func_0886AA0C for 2, 5 and 6, then release the member at +0x74 through its vtable slot 0 (a1 = the word at +0x70)
// when it is set; continue in the tail call func_0886A858(self).

// 0x0888CAFC (a0 = object, a1 = that callee's index, a2 = a selector, a3 = a direction): func_088DA788(object's word
// at +4) answers a record whose word at +0x618 is the table. Selector 1 reads the pair at +0x210 / +0x214 and the
// record's word at +0x70; any other selector's index at +0x30 picks a 0xC-byte entry (index * 16 - index * 4) of the
// table (words at +0x10 / +0x14) and the record's word at index * 4 + 0x100. v0 = the direction ? entry_first -
// object_word : object_word - entry_first; v1 (and a3) = the direction ? record_word - pair_second : pair_second -
// record_word.

// 0x088B3D38 (a0 = a source, a1 = a destination sink): func_08BB15E4(sink, frame) with the source's count at +4
// stored at the frame first; each of that many entries (the word at +0 plus the index) goes through
// func_08BB1610(sink, entry); then func_088BA424(sink, word at +8, ((word at +0xC + 0x1F) >> 5) << 2),
// func_08BB1610(sink, +0x12) and func_08BB163C(that answer, +0x13). v0 = the last answer.
void func_088B3D38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 sink = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), word(rt, self + 4u));  // the delay slot of the call below
    call(rt, ctx, 0x08BB15E4u, {sink, frame.at(0u)});
    const u32 count = word(rt, self + 4u);
    for (u32 index = 0u; index < count; ++index) call(rt, ctx, 0x08BB1610u, {sink, word(rt, self) + index});
    const u32 span = word(rt, self + 0xCu);
    call(rt, ctx, 0x088BA424u, {sink, word(rt, self + 8u), ((span + 0x1Fu) >> 5) << 2});
    const u32 record = call(rt, ctx, 0x08BB1610u, {sink, self + 0x12u});
    ctx.gpr[2] = call(rt, ctx, 0x08BB163Cu, {record, self + 0x13u});
}

// 0x088EC56C (a0 = argument, a1, a2 = object): the object's word at +0x538 is asked through its virtual at +0x74;
// when the object's word at +0xB4 exists, a 0x1C-byte descriptor on the frame (built from that word at +0xC0 and the
// first answer's word at +0x10) goes to func_088EDD5C, then the object's virtual at +0xA0 answers and a non-zero
// answer runs func_088EE154(object, argument). v0 = 0 on the paths that do not call it.

// 0x0893A4AC (a0 = object, a1, a2, a3 = a source): func_08939F4C(object), func_0893A210(object) gives a handle,
// func_0893A0D0(object, it) stores a1 in it, func_0893A3A4(object, it) stores a2, func_0893A104(object, it) answers
// the slot where 1 is stored through func_0893A3DC(object, it), and func_08B79A88 copies a3 into that slot's object
// with a2 = the object's word at +4; the work continues in the tail call func_08939F54(object).

// 0x0896BABC (a0 = object): the destructor. Store the vtable 0x08CB92F8 and 0x08CB9530 at +0x38, func_08965B78 on
// the word at +0x80, func_08968F98 on the word at +0xD0, then the object at +0xCC - 0xC is released when it is not the
// table 0x08D74FD0 (func_08B60804 answers > 0, else func_08B8542C); release +0xA0, func_089B8884(+0x84),
// func_089BBAC8(object), and func_08A5BC54(object, 0xF0, 4). v0 = the last answer.

// 0x08A411A8 (a0 = object with a word vector at +0x14/+0x18, a1 = a holder pointer, a2 = a value): the holder's
// index (virtual +0x3C) picks an element of the vector (4 bytes each); a negative index returns it, an index past the
// end returns the vector's begin word, and otherwise func_08A40E90(frame, a2) fills a value object that func_08A5BAD8
// assigns to the element unless it is the frame address, and func_08A5B9EC destroys it.

// 0x08A840F4 (a0 = the counted value's low word at +0x18 / high word at +0x1C, a1 = destination, a3 = a pointer to
// the holder word): the holder's virtual at +0x4C is the shift count; the 64-bit value is shifted right by it (the
// asm's sllv/srlv/srav with the movn pair for counts >= 32) and func_08A83160(frame, count & 0x20, low, high) builds
// a value object that func_08A5BAD8 assigns to a1 unless it is the frame address; func_08A5B9EC destroys it.

// --- the per-function wrappers ------------------------------------------------------------------------------
void func_08A1B4C4(Runtime &rt, AllegrexContext &ctx) { map_insert(rt, ctx, 0x08A1B318u); }
void func_08A71D64(Runtime &rt, AllegrexContext &ctx) { map_insert(rt, ctx, 0x08A719B8u); }



void func_08A83A10(Runtime &rt, AllegrexContext &ctx) { pair_compare_emit(rt, ctx, 0x08CB168Cu, 0x40u, PairTest::NotLessA, true); }

} // namespace
} // namespace new_batch_main_947
} // namespace gt::game

#define NEW947(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_947::name, gt::ReturnKind::kind)
NEW947(0x08A1B4C4u, func_08A1B4C4, Word);
NEW947(0x08A71D64u, func_08A71D64, Word);
NEW947(0x0885FCA4u, func_0885FCA4, Word);
NEW947(0x088B3D38u, func_088B3D38, Word);
NEW947(0x08A83A10u, func_08A83A10, Word);
