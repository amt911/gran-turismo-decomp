// New batch 24: small functions (440-500 bytes) written by hand from `tools.disasm` as structured C. Delay slots are read as
// "before the branch / call"; the callee-saved registers the original keeps values in are set where the asm sets them (a callee
// may read them) and put back at the end; `v0` (or `f0` for the float ones) ends as the asm leaves it. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_24 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original.
inline u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
inline u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
inline s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// s0-s7 and $fp are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}
// The `ll/sc` decrement: leaves a0 = the old value, v1 = the address and v0 = 1 (the successful `sc`); true when the old
// value was 1 (the count reached zero).
inline bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}
// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}

// --- two sorted-list inserts of one 0x14-byte node (0x08BD0978, 0x08BE191C) -----------------------------------------
// The list hangs at owner + 0x98 (a sentinel node whose word +0 is the first one); every node's word +8 points 0xC bytes
// before its key. The arguments reach the compare helper func_08B36170 as (node's payload, the word the caller's a1 points
// at, the larger of the two keys). A node whose byte +0x14 is set gets a new 0x14-byte node from func_08B62344 with the
// vtable below, holding a3, a2's second word and a2's first word, appended at node + 0x18; otherwise the function pointer
// in a2's first word (offset by a2's second word through the table at a3, when that word is odd) is called with
// a3 + (a2's second word >> 1). func_08BBCFA0(owner) runs last and v0 = 1 when something was inserted or called.
void insert_into_sorted_list(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 key_source = ctx.gpr[5];
    const u32 description = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 first_word = word(rt, description);
    const u32 second_word = word(rt, description + 4u);  // the beqz's delay slot: read before the branch
    u32 inserted = 0u;
    ctx.gpr[22] = self;
    ctx.gpr[17] = key_source;
    ctx.gpr[30] = argument;
    ctx.gpr[16] = 0u;
    if (word(rt, self) == 0u) {
        ctx.gpr[2] = inserted;
        return;
    }
    call_set(rt, ctx, 0x08B34888u, {word(rt, self) + 0x6Cu});
    const u32 owner = word(rt, self);
    const u32 sentinel = owner + 0x98u;
    u32 node = word(rt, sentinel);
    u32 current = sentinel;
    u32 key = 0u;
    if (node != sentinel) {
        const u32 key_owner = word(rt, key_source);          // s4, read once before the loop
        const u32 wanted = word(rt, key_owner - 0xCu);       // s0
        for (;;) {
            current = node;
            const u32 payload = word(rt, node + 8u);
            key = word(rt, payload - 0xCu);
            const u32 smaller = (wanted < key) ? wanted : key;  // `movz v0, sp, v1`: the smaller key
            // a3..t3 are the caller's registers, passed on as they are (the callee copies t3 only to compare it with itself)
            const u32 answer = call_set(rt, ctx, 0x08B36170u, {payload, key_owner, smaller, ctx.gpr[7], ctx.gpr[8],
                                                               ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
            ctx.gpr[16] = wanted;
            if (answer != 0u || key != wanted) {
                const u32 next = word(rt, current);
                current = next;
                if (next == sentinel) break;
                node = next;
                continue;
            }
            break;
        }
    }
    if (current != sentinel) {
        if (byte_u(rt, current + 0x14u) != 0u) {
            const u32 item = call_set(rt, ctx, 0x08B62344u, {0x14u});
            put(rt, item, vtable);
            put(rt, item + 8u, argument);
            put(rt, item + 0x10u, second_word);
            put(rt, item + 4u, 0u);
            put(rt, item + 0xCu, first_word);
            const u32 list = current + 0x18u;
            const u32 last = word(rt, list + 4u);
            if (last != 0u) {
                put(rt, list + 4u, item);
                put(rt, last + 4u, item);
            } else {
                put(rt, list, item);
            }
            put(rt, item + 4u, 0u);
            inserted = 1u;
            const u32 count = word(rt, list + 8u) + 1u;
            ctx.gpr[2] = count;
            put(rt, list + 8u, count);
        } else {
            const u32 position = argument + (second_word >> 1);
            u32 target = first_word;
            if ((second_word & 1u) != 0u) target = word(rt, word(rt, position) + first_word);
            inserted = 1u;
            call_indirect(rt, ctx, target, {position});
        }
    }
    call_set(rt, ctx, 0x08BBCFA0u, {self});
    ctx.gpr[2] = inserted;
}

// func_0887B980(self = a0, source = a1, second = a2): a pair of shared-pointer pairs, built through func_08963418 into a
// temporary, is assigned to self +0/+4 and +8/+0xC (use count up, the old block released: use count, virtual slot 8, weak
// count and slot 0xC on the last), then the temporary is released (func_08B8ED98) and func_0887B40C(self, 0, 0) finishes.
// v0 = that last call's result.

// func_088EC61C(self = a0, argument = a1, state = a2): takes the guard word at state + 0x4E8 (func_08B2E2B8(0x1F4) while it
// holds neither 0 nor 4), asks the object at state + 0x538 (`func_088EC61C`'s virtual slot 0x74) with the argument, and
// clears that answer's entry (word +4, one of four 0x28-byte entries at state + 0x4EC .. + 0x4F0): when the answer's byte 0
// is 0..3, the callback at state + 0xB8 runs, then under state + 0xC4 (func_08B344E0/08B3457C) the halfword at
// state->+0x11C[byte * 2] becomes -1 and the byte at +0x1A of that table 0; func_088EEE98(state->+0x118, byte) answers a
// pointer whose word is cleared. The guard word is put back to 0 when it was 2, and v0 = 1.

// func_08ACC704(self = a0, mode = a1, third = a2, fourth = a3, fifth = t0): under the lock self + 0x60 (func_08B34888,
// released by the tail jump to func_08B34B24) a block at self + 0x98 is set up: func_08ACC640(self), the word self + 0x94
// becomes -1, self + 0x90 zero, and the word self + 0x9C takes 0 for a word 0x08D69524 of 13 or more, else the table at
// 0x08C9070C [0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB]. Then func_08B2ED74(9, block + 8), block[0] = 0x2C4,
// func_08B2E314(5, 1), (1, 2), (1, 1), (6, 0) into block + 0xC, +0x10, +0x14, +0x18, block + 0x23C = third, block + 0x38 =
// fourth, block + 0x34 = mode; for mode 1, func_08B7C830(block + 0x3C, fifth, 0x200) and block's byte +0x23B cleared.
// Bytes +0x284 and +0x244, words +0x240 cleared, then the self virtual slot 0x1C runs with self and byte self + 0x8C
// becomes 1. v0 = that virtual call's, and the tail jump keeps it.

// func_08BAFD74(self = a0, position = a1, source = a2, count = a3): a string replace over the counted buffer at *self (its
// length is the word 0xC before the data pointer): func_08B5AEDC(0x08CA09E0) when the length is smaller than the position,
// func_08B5ADDC(0x08CA09E0) when the count does not fit 0x3FFFFFFC - length. When the source points into the buffer (or
// lies before it, or the word -4 of the data is positive) the work is a tail jump to func_08B94540(self, position, 0,
// source, count). Otherwise the buffer is reallocated (func_08B943A0(self, position, 0, count)), the pointers recomputed,
// and count bytes are copied with func_08B79A88 (a single byte inline) from the source, or - when the ranges overlap - in
// two moves. v0 = self.
void func_08BAFD74(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 count = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 data = word(rt, self);
    const u32 length = word(rt, data - 0xCu);
    ctx.gpr[19] = self;
    ctx.gpr[18] = count;
    if (length < position) call_set(rt, ctx, 0x08B5AEDCu, {0x08CA09E0u});
    if (0x3FFFFFFCu - length < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA09E0u});
    bool tail = false;
    if (source < data) tail = true;
    else if (data + length >= source) tail = true;
    else if (signed_of(word(rt, data - 4u)) > 0) tail = true;
    if (tail) {
        tail_call(ctx, 0x08B94540u, {self, position, 0u, source, count});
        return;
    }
    const u32 distance = source - data;
    ctx.gpr[16] = distance;
    call_set(rt, ctx, 0x08B943A0u, {self, position, 0u, count});
    const u32 new_data = word(rt, self);
    const u32 new_source = new_data + distance;
    const u32 end = new_source + count;
    const u32 destination = new_data + position;
    const auto move = [&](u32 to, u32 from, u32 bytes) {
        if (bytes == 1u) put8(rt, to, byte_u(rt, from));
        else call_set(rt, ctx, 0x08B79A88u, {to, from, bytes});
    };
    if (destination < end) {
        const u32 offset = destination - new_source;  // the bnez's delay slot: always computed
        if (new_source < destination) {
            move(destination, new_source, offset);
            move(destination + offset, destination + count, count - offset);
        } else {
            move(destination, end, count);
        }
    } else {
        move(destination, new_source, count);
    }
    ctx.gpr[2] = self;
}

// func_088BF1FC(self = a0, mode = a1, untraced = a2, reference = a3, output = t0, f12 = a scale): at most ten probes (one
// when the scale is zero or below 5, ten from 5 up, the scale clamped to 5 below that) of the object at self + 0x1C: its
// virtual slot 0x1C answers an object whose virtual slot 8 fills the frame's position at sp + 0x28 from the frame (count 1
// at sp, the mode, self's word +0x18's byte +0x94 (1 for exactly 1) or 2 for mode 4, and the float
// func_088DF13C(self->+0xC, self->+0x10) points at). The horizontal distance from (reference[0], reference[1]) ends the
// loop at 25 (5 squared); the scale grows by 1 per probe that continues. output[0] = dx, output[1] = dy, f0 = the scale.

// func_08990D6C(self = a0, other = a1): func_08990A1C(self), func_089AD4B8(other) (its answer in s4), func_08983F74(self +
// 0x130, other); then the signed bytes at self + 0x428 + the cursor from self + 0x220 to self + 0x224 are summed, the
// cursor advanced by func_0898D9E8(self, &cursor, 0x80, 1), and the sum stored at self + 0x4A8. The frame float (filled by
// func_0896A35C(self, 0, 0, 0, &frame)) divided by (word 0x08CF68E8's float +0x68 plus 14) times its float +0x24, truncated
// as a word, goes to self + 0x4AC; self + 0x4B0 takes the frame float minus that word times the same factor. Word self +
// 0x4B4 0 calls func_0898DDB8(self); 1 adds (float)s4 to self + 0x4B8 (and s4 to self + 0x4BC unless the byte self + 0x4C0
// is zero), refills the frame (func_0896A35C) and passes its float to func_0898DC1C(self). func_08983440(self + 0x130) and
// func_089A8EB0(other) finish.

// func_08A819B4(self = a0, list = a1): iterates the list at *a1 with four value objects in its frame (sp+8, sp+0xC, sp+4,
// sp+0x10): func_08A5BB7C(0x08CB15A0) when a node is null. The node's holder at +0x14 is asked through its virtual slot 0xA8
// into sp+8; while the holder's slot 8 answers nonzero, slot 0xC runs, func_08A76ABC builds sp+0xC from the holder + 4, that
// object's slot 0x84 fills sp+4 (mode 0) and func_08A818A0(self, sp+4) fills sp+0x10; when sp+0x10 is not null the object's
// slot 0x84 runs again (mode 1) and the self virtual slot 0xA0 does with (sp+0x10, sp). Each temporary is released with
// func_08A5B9EC. v0 = the release of sp+8.

// func_08B1B420(value = a0): a packed date from a day count: value - 0x1A4452 split by 146097, 36524 and 1461 (signed
// divisions through the `mult`/`mfhi`/`msub`/`mflo` pattern) into a cycle, a year index (3 when the 36524 quotient is 4), a
// day in that cycle and a table byte; func_08B1B268 reads the year tables at 0x08C932A0 (halfword at +2*index, 5 entries)
// and 0x08C9326C (halfword at 2*(13*choice + result), 0xD entries). v0 packs the low halfword (a sign-extended halfword:
// ((index*25 + extra) * 4 + first answer + 1)) and the high halfword (byte first+1, byte day) with `ins`.

// func_08C1667C(base = a0, left = a1, reference = a2, key = a3): the partition step of a quick sort over 0x2C-byte records:
// func_08A15D54(reference, left, key) advances `left` while it answers nonzero; `right` - 0x2C is walked down the same way
// with func_08A15D54(reference, key, right); while left < right the two records are swapped (words +0/+4, floats +8/+0xC/
// +0x10/+0x14 and +0x18/+0x1C/+0x20/+0x24, word +0x28, through the frame at sp+8..sp+0x28) and left advances once more.
// The original's pointer-equality guards around the float blocks cannot fire for two distinct records. v0 = left.
// --- the insert-one-element shape of 0x08C2B298 and 0x08C3548C ---------------------------------------------------------
// (a0 = a vector {begin, finish, end} of 4-byte slots, a1 = the position, a2 = the source object): with room at the end
// (finish != end) the slot after finish is move-constructed from finish - 4 so finish can advance, the new element is
// move-constructed from the source into a frame slot, the slots from the position up are shifted by one (assign at
// destination/src pairs four bytes apart; the original skips the assign when the two coincide) and the frame slot is
// assigned to the position and destroyed. Without room the vector is doubled (4 bytes when empty, 0xFFFFFFFC on overflow),
// allocated with func_08A5BC20, the head copied into it, the element move-constructed, the tail copied after it, the old
// range destroyed (copy_tail() on it) and its block freed (func_08A5BC54); begin/finish/end are updated.
void insert_one_element(Runtime &rt, AllegrexContext &ctx, u32 move_element, u32 copy_range, u32 copy_tail, u32 message) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[21] = position;
    ctx.gpr[18] = source;
    ctx.gpr[16] = self;
    const u32 finish = word(rt, self + 4u);
    if (finish == word(rt, self + 8u)) {
        const u32 begin = word(rt, self);
        const u32 size = static_cast<u32>(signed_of(finish - begin) >> 2);
        if (size == 0x3FFFFFFFu) call_set(rt, ctx, 0x08B5ADDCu, {message});
        u32 bytes;
        if (size == 0u) {
            bytes = 4u;
        } else {
            const u32 doubled = size << 1;
            bytes = doubled < size ? 0xFFFFFFFCu : doubled << 2;
        }
        const u32 buffer = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        ctx.gpr[19] = buffer;
        const u32 head = call_set(rt, ctx, copy_range, {begin, position, buffer, frame.at(2u)});
        ctx.gpr[17] = head;
        if (head != 0u) call_set(rt, ctx, move_element, {head, source});
        const u32 tail = call_set(rt, ctx, copy_tail, {position, word(rt, self + 4u), head + 4u, frame.at(1u)});
        ctx.gpr[17] = tail;
        call_set(rt, ctx, copy_tail, {word(rt, self), word(rt, self + 4u), frame.at(0u)});
        const u32 old_begin = word(rt, self);
        const u32 old_bytes = static_cast<u32>(signed_of(word(rt, self + 8u) - old_begin) >> 2) << 2;
        if (old_begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {old_begin, old_bytes, 4u});
        put(rt, self + 8u, buffer + bytes);
        put(rt, self + 4u, tail);
        put(rt, self, buffer);
        return;
    }
    if (finish != 0u) call_set(rt, ctx, move_element, {finish, finish - 4u});
    const u32 grown = word(rt, self + 4u);
    put(rt, self + 4u, grown + 4u);
    call_set(rt, ctx, move_element, {frame.at(4u), source});
    const u32 new_finish = word(rt, self + 4u);
    u32 destination = new_finish - 0xCu;  // the loop head's first decrement
    u32 from = new_finish - 0xCu;
    const s32 count = signed_of((new_finish - 8u) - position) >> 2;
    if (count > 0) {
        ctx.gpr[20] = destination + 4u;
        for (s32 i = 0;;) {
            if (destination != from) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, from)});
            ++i;
            if (count == i) break;
            destination -= 4u;
            from -= 4u;
            ctx.gpr[20] = destination;
        }
    }
    if (position != frame.at(4u)) call_set(rt, ctx, 0x08A5BAD8u, {position, word(rt, frame.at(4u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}
void func_08C2B298(Runtime &rt, AllegrexContext &ctx) {
    insert_one_element(rt, ctx, 0x08AAE3A8u, 0x08C2B234u, 0x08C2AD88u, 0x08CB1CA0u);
}
void func_08C3548C(Runtime &rt, AllegrexContext &ctx) {
    insert_one_element(rt, ctx, 0x08AAE214u, 0x08C35428u, 0x08C35154u, 0x08CB398Cu);
}


// Releases a libstdc++ copy-on-write string rep at `text` (its rep is 0xC before the data, the count at -4): nothing for
// the empty rep D_08D75298, else func_08B60804(&count, -1) and, when the old count was not positive, func_08B93554(rep,
// frame). v0 = the empty rep / the count step / the free call's result.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 text, u32 frame_base) {
    const u32 rep = text - 0xCu;
    ctx.gpr[2] = 0x08D75298u;
    if (rep == 0x08D75298u) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B93554u, {rep, frame_base});
}

// func_08879190(self = a0): decodes the bit string func_08AB856C builds from the two words at 0x08C8981C (with self and 2
// through func_08875658) into self's target: func_0886B2F4(self[0]) -> func_0886A950(name 0x08CA008C) ->
// func_0886A9A0 gives the object; the byte count at frame +0x14 must be 2 or more, then for every bit up to frame +8 a
// character of the string (word frame +4, index >> 2) is decoded from hex/letters (digit, A-F, a-f) and its bit (index & 3)
// is passed to func_088852D0(object, string + index, bit); a zero character ends it, a negative word frame +4 calls
// func_088852C4(object, frame +8 != 0) instead. The temporary string at frame +0xC is released at the end.
void func_08879190(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = frame.at(0xCu);
    ctx.gpr[17] = self;
    call_set(rt, ctx, 0x08AB85F0u, {frame.at(0xCu)});
    put(rt, frame.at(4u), word(rt, 0x08C8981Cu));
    put(rt, frame.at(8u), word(rt, 0x08C89820u));
    call_set(rt, ctx, 0x08875658u, {self, frame.at(4u), frame.at(0xCu), 2u});
    const u32 bits = call_set(rt, ctx, 0x08AB856Cu, {frame.at(0xCu)});
    ctx.gpr[18] = bits;
    if (signed_of(word(rt, frame.at(0x14u))) >= 2) {
        u32 object = call_set(rt, ctx, 0x0886B2F4u, {word(rt, self)});
        const GuestResult looked_up = call_keep(rt, ctx, 0x0886A950u, {object, 0x08CA008Cu});
        if (object != 0u && looked_up.v0 != 0u) {
            // func_0886A9A0 indexes with a1 as the callee left it (the original passes that register on)
            object = call_set(rt, ctx, 0x0886A9A0u, {looked_up.v0, looked_up.a1});
            ctx.gpr[17] = object;
            if (signed_of(word(rt, frame.at(4u))) < 0) {
                call_set(rt, ctx, 0x088852C4u, {object, word(rt, frame.at(8u)) != 0u ? 1u : 0u});
            } else if (signed_of(word(rt, frame.at(8u))) > 0) {
                u32 character = sext8(byte_u(rt, bits));
                u32 index = 0u;
                while (character != 0u) {
                    const u32 position = word(rt, frame.at(4u)) + index;
                    const u32 digit = character - 0x30u;
                    const bool is_digit = digit < 0xAu;
                    const u32 lower = character - 0x61u;
                    const bool is_hex = is_digit || (character - 0x41u) < 6u || lower < 6u;
                    if (is_hex) {
                        const u32 shift = lower < 0x1Au ? 0u : 0x20u;  // `movz v0, zero, v1`
                        const u32 value = is_digit ? digit : character - shift - 0x37u;
                        call_set(rt, ctx, 0x088852D0u, {object, position, (value >> (index & 3u)) & 1u});
                    }
                    ++index;
                    const u32 next = static_cast<u32>(signed_of(index) >> 2);
                    if (!(signed_of(index) < signed_of(word(rt, frame.at(8u))))) break;
                    character = sext8(byte_u(rt, bits + next));
                }
            }
        }
    }
    release_string(rt, ctx, word(rt, frame.at(0xCu)), frame.at(0u));
}

// func_08880BAC(self = a0): releases the two objects at self + 4 and self + 8 through their virtual slot 0x2C (each field
// cleared), func_08880874(self), the shared object at self + 0x34, stores the vtable word 0x08CBF368 at self + 0x20 and
// releases the shared object at self + 0x18; the shared object at self + 0x10 then loses one use, and when that was the
// last one its slot 8 runs, its weak count (+8) goes down and on the last one the function ends in a tail jump to its
// slot 0xC (a0 = the object).


// func_08A386E8(out = a0, source = a1, mode = a2, argument = a3): builds a value object from the source through
// func_08A38038(slot, source), whose object's virtual slot 0x54 answers the floats; for mode 2 and up two of them
// (argument and argument + 4, each built by func_08A87CD8) are interpolated with func_08B1EFBC(object + 0x14, first,
// second), otherwise the pair (0.0, 1.0) is used. func_08A87EEC(&result, f12) builds the result value, which is assigned
// to `out` (func_08A5BAD8, skipped when it is the temporary itself) and the temporaries are destroyed (func_08A5B9EC).


// func_08C6F004(output = a0, source = a1, begin = a2, end = a3): formats the pieces of `source` (separated by NULs, from
// func_08C74968(slot, begin, end, slot2)) into `output`, which starts as the empty string 0x08D77498 + 0xC. Each pass
// formats one piece into a stack buffer of 2*(end-begin)+0x1E bytes (func_08B60890(source, buffer, piece, span)) - grown
// to the written count + 1 when that is short - and appends it (func_08C75AF8); a pass whose piece ends exactly at the
// pattern's end stops, otherwise the fill byte at 0x08CDBE28 is appended (growing through func_08C75840 when the capacity
// is short or the word -4 of the data is positive) at data + (length + 1) and the piece moves past its NUL
// (func_08B35F60). The temporary format at frame +4 is released at the end. v0 = output.

// func_08C72614(self = a0, value = a1): func_08C70314(frame, self) fills the frame; when its byte 0 is set, self's record
// at self + *(self - 0xC) holds a count (word +8) of 2 or more, and its byte +0x75 is clear, func_08C5477C(record, 0x20)
// gives the byte +0x74 (then marked at +0x75); the record is formatted by func_08C5F3E4(record, byte +0x74, buffer,
// frame +0x10, count, 1, 0) into a stack buffer of count + 0x1E bytes and the object at record + 0x78 is asked through
// its virtual slot 0x30 with (buffer, count); a different answer calls func_08C54CFC(record, word +0x14 | 1), and the
// record's count is cleared. The word frame +4's record (word +0xC of it) with bit 0x2000 set, when func_08B62318()
// answers zero, runs the object at +0x78's virtual slot 0x18 and on -1 clears word +0x14 | 1 too. v0 = self.
void func_08C72614(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[17] = self;
    put8(rt, frame.at(0x10u), ctx.gpr[5]);
    call_set(rt, ctx, 0x08C70314u, {frame.at(0u), self});
    if (byte_u(rt, frame.at(0u)) != 0u) {
        u32 record = self + word(rt, word(rt, self) - 0xCu);
        u32 count = word(rt, record + 8u);
        u32 piece = frame.at(0x10u);
        u32 kept = 1u;  // s3
        if (signed_of(count) >= 2) {
            const u32 size = (count + 0x1Eu) & ~0xFu;
            if (byte_u(rt, record + 0x75u) == 0u) {
                put8(rt, record + 0x74u, call_set(rt, ctx, 0x08C5477Cu, {record, 0x20u}));
                put8(rt, record + 0x75u, 1u);
            }
            {
                GuestFrame buffer_frame(ctx, size);
                piece = buffer_frame.at(0u);
                call_set(rt, ctx, 0x08C5F3E4u,
                         {record, sext8(byte_u(rt, record + 0x74u)), piece, frame.at(0x10u), count, 1u, 0u});
            }
            kept = count;
        }
        const u32 target = self + word(rt, word(rt, self) - 0xCu);
        const u32 object = word(rt, target + 0x78u);
        if (call_virtual_at(rt, ctx, object, 0x30u, {object, piece, kept}) != kept) {
            call_set(rt, ctx, 0x08C54CFCu, {target, word(rt, target + 0x14u) | 1u});
        }
        const u32 data = word(rt, self);
        put(rt, self + word(rt, data - 0xCu) + 8u, 0u);
    }
    const u32 holder = word(rt, frame.at(4u));
    const u32 record = holder + word(rt, word(rt, holder) - 0xCu);
    if ((word(rt, record + 0xCu) & 0x2000u) != 0u) {
        if (call_set(rt, ctx, 0x08B62318u, {}) == 0u) {
            const u32 object = word(rt, frame.at(4u));
            const u32 inner = object + word(rt, word(rt, object) - 0xCu);
            const u32 item = word(rt, inner + 0x78u);
            if (item != 0u) {
                if (call_virtual_at(rt, ctx, item, 0x18u, {item}) == 0xFFFFFFFFu) {
                    const u32 again = word(rt, frame.at(4u));
                    const u32 target = again + word(rt, word(rt, again) - 0xCu);
                    call_set(rt, ctx, 0x08C54CFCu, {target, word(rt, target + 0x14u) | 1u});
                }
            }
        }
    }
    ctx.gpr[2] = self;
}


// func_08C75FF8(self = a0, position = a1, count = a2, source = a3, source_length = t0): the in-place replace of
// `count` bytes at `position` with the `source_length` bytes at `source`: func_08B5AEDC(0x08CB58D8) when the position is
// past the length, func_08B5ADDC(0x08CB58D8) when the source length does not fit 0x3FFFFFFC - (length - n) (n = the
// smaller of count and length - position). A source outside the buffer, or a buffer whose word -4 is positive, hands the
// work to func_08C74D38(self, position, n, source, source_length). Otherwise an overlapping source is copied into a
// temporary first (func_08C749D0, released through the empty rep 0x08D77498) and the same call runs on it; a disjoint one
// goes straight on: func_08C74B68(self, position, n, source_length) then the bytes move (one inline, else func_08B79A88)
// from data + source - data (+ source_length - n when the ranges overlap) to data + position. v0 = self or that call's.
void func_08C75FF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    const u32 source_length = ctx.gpr[8];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 data = word(rt, self);
    const u32 length = word(rt, data - 0xCu);
    u32 result = self;
    if (length < position) {
        call_set(rt, ctx, 0x08B5AEDCu, {0x08CB58D8u});
    } else {
        const u32 available = length - position;
        const u32 n = count >= available ? available : count;
        if (0x3FFFFFFCu - (length - n) < source_length) {
            call_set(rt, ctx, 0x08B5ADDCu, {0x08CB58D8u});
        } else if (source < data || data + length < source || signed_of(word(rt, data - 4u)) > 0) {
            result = call_set(rt, ctx, 0x08C74D38u, {self, position, n, source, source_length});
        } else {
            const u32 position_pointer = data + position;
            u32 difference = source - data;
            bool copy_temporary = false;
            if (position_pointer < source + source_length) {
                if (source < position_pointer + n) {
                    copy_temporary = true;
                } else {
                    difference += source_length - n;
                }
            }
            if (copy_temporary) {
                call_set(rt, ctx, 0x08C749D0u, {frame.at(4u), source, source_length, frame.at(1u)});
                result = call_set(rt, ctx, 0x08C74D38u, {self, position, n, word(rt, frame.at(4u)), source_length});
                const u32 text = word(rt, frame.at(4u));
                if (text - 0xCu != 0x08D77498u) {
                    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0) {
                        call_set(rt, ctx, 0x08C74884u, {text - 0xCu, frame.at(0u)});
                    }
                }
            } else {
                call_set(rt, ctx, 0x08C74B68u, {self, position, n, source_length});
                const u32 text = word(rt, self);
                if (source_length == 1u) put8(rt, text + position, byte_u(rt, text + difference));
                else call_set(rt, ctx, 0x08B79A88u, {text + position, text + difference, source_length});
            }
        }
    }
    ctx.gpr[2] = result;
}

// func_08A14778(owner = a0): builds the 0x1B0-byte object (func_08A5BC20(0x1B0, 4), constructed by func_08A2A154, vtable
// word 0x08CD04B0), the member at +0x18 (func_08A1A914) and two at +0xAC and +0xD8 (func_08B28D34); the compiler's inline
// copies put the 0x80 bytes at 0x08C8E2B0 at +0x10D and the eight words at 0x08C8E290 at +0x190, then +0x10D gets 0x80
// zero bytes and +0x18D 0x20 (func_08B36018), 0x80 bytes are copied from 0x08CA9A84 (func_08B7C830) and the member +0x14
// points at +0x18. func_08A2A218(owner, &object) finishes; v0 is its answer.


// func_08C74B68(self = a0, position = a1, count = a2, inserted = a3): the length/byte step of a string replace: the new
// length is length + inserted - count; when the capacity is short or the word -4 of the data is positive,
// func_08C74398(new length, capacity, frame) reallocates and the head [0, position) and the tail [position + count, ..)
// (the smaller of the two is copied inline, else func_08B79A88) move into the new block; otherwise, with inserted equal to
// count the tail already sits right, else it moves by (inserted - count) bytes (func_08B79BAC). The old rep is released
// (empty rep 0x08D77498) and the new length and the terminator byte at 0x08CDBE28 are stored. v0 = data + new length.

// func_0889F010(self = a0, first = a1, source_a = a2, source_b = a3, second = t0, third = t1): a constructor: the vtable
// word 0x08CC2420, two {pointer, control block} pairs from source_a (+0/+4, +8/+0xC) at self +4/+8 and +0xC/+0x10, two
// more from source_b (+0/+4, +8/+0xC) at self +0x14/+0x18 and +0x1C/+0x20 (each block's count at +4 incremented with
// `ll/sc`), then a 0x3C-byte object (func_08B62344, func_08B28D34, words +0x2C/+0x34/+0x38 and byte +0x30 cleared) built by
// func_0889C2C0(self + 0x28, first, source_a, source_b, t0 & 0xFF, self + 0x24) and kept at self + 0x24; func_08912E08(self
// + 0x30) runs and, unless t1 & 0xFF is set, the function ends in a tail jump to func_08911D80(self + 0x30, self + 0x24,
// source_a, first).


// func_08A5159C(self = a0): under the lock 0x08CF6D34 (func_08B344E0 / 08B3457C), byte self + 0x5C cleared, walks the
// entries self + 0x24 downwards from index (word +0x24) + 1 modulo the halfword +0x1C to index inclusive: when the flag
// byte at self->+0x28[index] has bit 0 clear and the halfword entry at self->+0x10[index] is not zero, the object is that
// halfword's bits 1-2 (3: self + 4 when set, else self + 0), asked through func_08C7ACB0(object, entry, frame + 0x18), and
// func_08C7ACA8(object, the entry halfword, frame, 0, 0) runs with a frame built from self's byte +8 / +9 / +0xB, word +0xC
// and halfword +0x1E (index * that). The flag byte's bit 0 is then set and self + 0x24 steps down (wrapping to the count -
// 1). func_08B28C44(self + 0x30) runs after the unlock; while byte self + 0x5C stays clear the lock is taken again.

// func_088A48C8(self = a0): the destructor that follows func_088A3750: the vtable words 0x08CC29A8 (self), 0x08CC2C30 /
// 0x08CC2C4C (self + 0x318 / +0x31C) and func_088A8420(self + 0x318), the blocks at +0x338 (func_08BAE078(+0x338,
// +0x33C, frame +1), then func_08B35254) and +0x32C (func_08BAC238(+0x32C, +0x330, frame), then func_08B35254), the vtable
// words 0x08CC2A98 / 0x08CC2AB0, the object at self + 0x274 through its first virtual (and cleared), 0x08CC2CB8 /
// 0x08CC2DDC at self + 0x20, four objects ending at self + 0x158 (steps of 0xC, first virtual, self's address) and four
// ending at self + 0x118 (steps of 0x1C), 0x08CBF428 / 0x08CC16A0 at self + 0x20, 0x08CC2AD8 at self + 0x10,
// func_088A7548(self + 8), 0x08CC2AE8 at self and func_08B64654(self). v0 = its result.


// func_0890F664(first = a0, list = a1, holder = a2): runs holder's virtual slot 8 (one answer) and func_08AB8718(list)
// (the other), takes the smaller of the two as the count and for every index below it: holder's slot 0xC(holder, index, 0)
// selects an object (0 when it is null), that object's slot 8 decides (above zero goes on, else the object is 0), its slot
// 0x24 answers the value stored at first + 0x80 + 4*index (the previous one kept), its slot 0x10 fills the frame at sp+8 and
// func_08AB8A28(list, index, sp+8) runs, its slot 0x14 fills sp+0x14 and func_08AB8930(list, index, sp+0x14) runs; when
// the value did not change, its slot 0x18 and func_08AB8A04(list, index, sp+8) run, else func_08AB8720(list, index). Each
// step ends with func_08AB897C(list, index, 0, 1.0).

// func_08B27C28(self = a0): a validation pass: func_08B27874(self, word(self + 0x64) + 6) then func_08B278A0 checks --
// (byte +0x58, 1), (2, 2), (half +0x44 - 0x101, 5), (half +0x4C - 1, 5), (half +0x54 - 4, 4) -- each ending the function
// with 0 when it answers 0. With half +0x54 positive, every entry of the byte table at 0x08CE0C64 is an index into the
// 0x10-byte records at word self + 0x50, whose halfword +0xE is checked with 3; then the byte stream [self + 0x34,
// self + 0x38) is walked: each byte picks a record whose halfword +0xC and signed byte +0xE are checked with 3, and an
// opcode 0x10, 0x11 or 0x12 also checks the next byte (with 2, 3, 7). func_08B27AE8(self) finishes. v0 = its answer, or 0.
void func_08B27C28(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x08B27874u, {self, word(rt, self + 0x64u) + 6u});
    if (call_set(rt, ctx, 0x08B278A0u, {self, byte_u(rt, self + 0x58u), 1u}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call_set(rt, ctx, 0x08B278A0u, {self, 2u, 2u}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call_set(rt, ctx, 0x08B278A0u, {self, half_u(rt, self + 0x44u) - 0x101u, 5u}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call_set(rt, ctx, 0x08B278A0u, {self, half_u(rt, self + 0x4Cu) - 1u, 5u}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call_set(rt, ctx, 0x08B278A0u, {self, half_u(rt, self + 0x54u) - 4u, 4u}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 table = 0x08CE0C64u;
    const s32 entries = signed_of(half_u(rt, self + 0x54u));
    if (entries > 0) {
        for (s32 i = 0; i < entries; ++i) {
            const u32 record = word(rt, self + 0x50u) + (byte_u(rt, table + static_cast<u32>(i)) << 4);
            if (call_set(rt, ctx, 0x08B278A0u, {self, half_u(rt, record + 0xEu), 3u}) == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
    }
    u32 cursor = word(rt, self + 0x34u);
    const u32 end = word(rt, self + 0x38u);
    if (cursor != end) {
        for (;;) {
            const u32 opcode = byte_u(rt, cursor);
            const u32 record = word(rt, self + 0x50u) + (opcode << 4);
            u32 next = cursor + 1u;
            if (call_set(rt, ctx, 0x08B278A0u, {self, half_u(rt, record + 0xCu), sext8(byte_u(rt, record + 0xEu))}) == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
            u32 width = 0u;
            if (opcode == 0x11u) width = 3u;
            else if (opcode == 0x12u) width = 7u;
            else if (opcode == 0x10u) width = 2u;
            if (width != 0u) {
                next = cursor + 2u;
                if (call_set(rt, ctx, 0x08B278A0u, {self, byte_u(rt, cursor + 1u), width}) == 0u) {
                    ctx.gpr[2] = 0u;
                    return;
                }
            }
            if (end == next) break;
            cursor = next;
        }
    }
    call_set(rt, ctx, 0x08B27AE8u, {self});
}


// func_08B3141C(self = a0, selector = a1, first = a2, second = a3, third = t0): dispatches on selector through the table at
// 0x08C93B08 (0 and 1, 4, 5, 6 and anything from 11 up: func_08B30A6C(self, selector, first, second, third)). 2: when the
// byte self + 6 is set the shared step runs; otherwise with word (self + 0x10000) + 0x963C not negative, second == 8 and
// third's two words equal to +0x9638/+0x963C it runs too, else func_08B2FBEC(self, first, 0, 0). The shared step (the byte
// self + 7): when it is set it is the answer; when clear func_08B2FE44(self, first, 0, 0) runs and the byte is read again,
// zero ends in the same tail jump. 3, 8, 10: func_08B2F5D0 and func_08B31150 with (self, first), then a tail jump to
// func_08B3120C(self). 7: func_08B2FA04(self, first), func_08B3120C(self), func_08B31328(self, first), then the tail jump.
// 9: a tail jump to func_08B2F70C(self, 8).
void func_08B3141C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 selector = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    const u32 third = ctx.gpr[8];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[20] = second;
    ctx.gpr[19] = third;
    ctx.gpr[18] = first;
    ctx.gpr[17] = selector;
    ctx.gpr[16] = self;
    // The step at 0x08B314E4: `flag` is self's byte +7.
    const auto common_step = [&](u32 flag) {
        if (flag == 0u) {
            call_set(rt, ctx, 0x08B2FE44u, {self, first, 0u, 0u});
            flag = byte_u(rt, self + 7u);
        }
        if (flag == 0u) {
            tail_call(ctx, 0x08B30A6Cu, {self, selector, first, second, third});
            return;
        }
        ctx.gpr[2] = flag;
    };
    switch (selector) {
    case 2u: {
        if (byte_u(rt, self + 6u) == 0u) {
            const u32 base = self + 0x10000u;
            const u32 stored = word(rt, base + 0x963Cu);
            if (signed_of(stored) < 0 || second != 8u || word(rt, third) != word(rt, base + 0x9638u) ||
                word(rt, third + 4u) != stored) {
                tail_call(ctx, 0x08B2FBECu, {self, first, 0u, 0u});
                return;
            }
        }
        common_step(byte_u(rt, self + 7u));
        return;
    }
    case 3u:
    case 8u:
    case 10u:
        call_set(rt, ctx, 0x08B2F5D0u, {self, first});
        call_set(rt, ctx, 0x08B31150u, {self, first});
        tail_call(ctx, 0x08B3120Cu, {self});
        return;
    case 7u:
        call_set(rt, ctx, 0x08B2FA04u, {self, first});
        call_set(rt, ctx, 0x08B3120Cu, {self});
        call_set(rt, ctx, 0x08B31328u, {self, first});
        tail_call(ctx, 0x08B30A6Cu, {self, selector, first, second, third});
        return;
    case 9u:
        tail_call(ctx, 0x08B2F70Cu, {self, 8u});
        return;
    default:
        tail_call(ctx, 0x08B30A6Cu, {self, selector, first, second, third});
        return;
    }
}

// func_08C6F818(self = a0, other = a1): the same record update as func_08C72614 with `other`'s string as the piece: when
// the byte frame +0 is set, other's length (word -0xC of its data) is compared with self's record count (word +8 at self +
// self's length): when it is smaller the piece is formatted into a stack buffer of count + 0x1E bytes (func_08C5F3E4, the
// record's byte +0x74 computed by func_08C5477C and marked at +0x75 first) and the record's object at +0x78 is asked
// through its virtual slot 0x30 with (buffer, count), else with (other's data, other's length); a different answer calls
// func_08C54CFC(record, word +0x14 | 1) and the count is cleared. The tail is func_08C72614's: word frame +4's record with
// bit 0x2000 set runs func_08B62318() and the object at +0x78's virtual slot 0x18, -1 clearing word +0x14 | 1 too.
// v0 = self.
void func_08C6F818(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = other;
    call_set(rt, ctx, 0x08C70314u, {frame.at(0u), self});
    if (byte_u(rt, frame.at(0u)) != 0u) {
        u32 source = word(rt, other);
        u32 length = word(rt, source - 0xCu);
        const u32 record = self + word(rt, word(rt, self) - 0xCu);
        const u32 count = word(rt, record + 8u);
        if (signed_of(length) < signed_of(count)) {
            const u32 size = (count + 0x1Eu) & ~0xFu;
            if (byte_u(rt, record + 0x75u) == 0u) {
                put8(rt, record + 0x74u, call_set(rt, ctx, 0x08C5477Cu, {record, 0x20u}));
                put8(rt, record + 0x75u, 1u);
            }
            {
                GuestFrame buffer_frame(ctx, size);
                call_set(rt, ctx, 0x08C5F3E4u,
                         {record, sext8(byte_u(rt, record + 0x74u)), buffer_frame.at(0u), source, count, length, 0u});
                source = buffer_frame.at(0u);
                length = count;
            }
        }
        const u32 target = self + word(rt, word(rt, self) - 0xCu);
        const u32 object = word(rt, target + 0x78u);
        if (call_virtual_at(rt, ctx, object, 0x30u, {object, source, length}) != length) {
            call_set(rt, ctx, 0x08C54CFCu, {target, word(rt, target + 0x14u) | 1u});
        }
        const u32 data = word(rt, self);
        put(rt, self + word(rt, data - 0xCu) + 8u, 0u);
    }
    const u32 holder = word(rt, frame.at(4u));
    const u32 record = holder + word(rt, word(rt, holder) - 0xCu);
    if ((word(rt, record + 0xCu) & 0x2000u) != 0u) {
        if (call_set(rt, ctx, 0x08B62318u, {}) == 0u) {
            const u32 object = word(rt, frame.at(4u));
            const u32 inner = object + word(rt, word(rt, object) - 0xCu);
            const u32 item = word(rt, inner + 0x78u);
            if (item != 0u) {
                if (call_virtual_at(rt, ctx, item, 0x18u, {item}) == 0xFFFFFFFFu) {
                    const u32 again = word(rt, frame.at(4u));
                    const u32 target = again + word(rt, word(rt, again) - 0xCu);
                    call_set(rt, ctx, 0x08C54CFCu, {target, word(rt, target + 0x14u) | 1u});
                }
            }
        }
    }
    ctx.gpr[2] = self;
}


// func_08B5853C(selector = a0 - 0x42, value = a1): a dispatch table at 0x08C957AC of 0x68 entries (0x08B58590 when the
// selector is 0x68 or more), each choosing the three codes handed to func_08B584D0 and the two colours handed to
// func_08B584F4(a, a, a) and the tail jump to func_08B583FC(b, b, b); some entries take their colours from the low byte of
// value (one of them its complement).
void func_08B5853C(Runtime &rt, AllegrexContext &ctx) {
    const u32 selector = ctx.gpr[4] - 0x42u;
    const u32 value = ctx.gpr[5] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    // 0: 0x08B58590, 1: 0x08B58578, 2: 0x08B585A8, 3: 0x08B585C0, 4: 0x08B585DC, 5: 0x08B585F4, 6: 0x08B5860C,
    // 7: 0x08B58624, 8: 0x08B5863C, 9: 0x08B58654, 10: 0x08B5866C, 11: 0x08B58684, 12: 0x08B5869C, 13: 0x08B586B4,
    // 14: 0x08B586CC.
    static constexpr std::uint8_t kHandler[0x68] = {
        7,  0,  0,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  2,  0,  0,  0,  5,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  9,  0,  3,  0,  0,  0,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  12, 0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  13, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  14, 0,  0,  0,  10, 11,
    };
    const unsigned handler = selector < 0x68u ? kHandler[selector] : 0u;
    u32 first = 0x400u;
    u32 second = 0x302u;
    u32 third = 0x303u;
    u32 left = 0xFFu;
    u32 right = 0xFFu;
    switch (handler) {
    case 1u:
        first = 0x401u;
        second = 0x302u;
        third = 0x302u;
        break;
    case 2u:
        second = 0x304u;
        third = 0x305u;
        break;
    case 3u:
        left = value;
        right = (~value) & 0xFFu;
        second = 0x30Au;
        third = 0x30Au;
        break;
    case 4u:
        second = 0x302u;
        third = 0x30Au;
        break;
    case 5u:
        second = 0x304u;
        third = 0x30Au;
        break;
    case 6u:
        left = value;
        second = 0x30Au;
        third = 0x30Au;
        break;
    case 7u:
        first = 0x402u;
        second = 0x30Au;
        third = 0x302u;
        break;
    case 8u:
        first = 0x402u;
        second = 0x30Au;
        third = 0x304u;
        break;
    case 9u:
        first = 0x402u;
        second = 0x30Au;
        third = 0x30Au;
        right = value;
        break;
    case 10u:
        left = value;
        second = 0x30Au;
        third = 0x30Au;
        right = 0u;
        break;
    case 11u:
        second = 0x30Au;
        third = 0x30Au;
        left = 0u;
        right = value;
        break;
    case 12u:
        second = 0x30Au;
        third = 0x302u;
        left = 0u;
        break;
    case 13u:
        second = 0x30Au;
        third = 0x304u;
        left = 0u;
        break;
    case 14u:
        first = 0x401u;
        second = 0x30Au;
        third = 0x30Au;
        left = value;
        right = value;
        break;
    default:
        break;
    }
    call_set(rt, ctx, 0x08B584D0u, {first, second, third});
    call_set(rt, ctx, 0x08B584F4u, {left, left, left});
    tail_call(ctx, 0x08B583FCu, {right, right, right});
}

// func_089A9E58(self = a0, pairs = a1, extra = a2, f12 = a scale): builds a vertex list and hands it to the object's
// virtual slot 0x10C. With a positive scale: the direction from pairs[0]/pairs[1] to pairs[2]/pairs[3] scaled by
// scale / 2 / its length, and four steps along `pairs` (0x10-byte records) writing two points per step at sp and, when
// `extra` is set, the record's own at sp + 0x20; the call gets (self, word 0x08C8CC14, 4, sp, extra ? sp + 0x20 : 0).
// Otherwise four fixed points (extra's first and third records around pairs) and the call gets (self, word 0x08C8CC24, 2,
// sp + 0x20, extra ? sp : 0). v0 = that call's answer.


// func_08C35F34(base = a0, index = a1, limit = a2, value = a3): the heap step of a 12-byte-record queue: when limit is
// below the index the record is inserted by walking up from parent (index - 1) / 2 -- the record's word +4 holder's word
// +0x14 against the value's (func_08B35FD8(record word, value word) breaks the tie) -- each parent copied down into the
// hole (word +0, the word +4 assigned by func_08A5BAD8 unless both are the same, byte +8) and the hole moved up while the
// limit stays below the parent; the value is then stored in the hole (func_08A5BAD8 for the word +4). Otherwise the value
// goes straight to the index. func_08A5BB7C(0x08CB39CC) reports a null holder first.

// func_088CBC70(mode = a0, version = a1): the static constructor (mode 1) and destructor (mode 0) of the four 0xB60-byte
// objects at 0x08CE6E10 (construction: func_08B28DCC each, the fields +0x5C8..+0x614 set as the asm does - 2, 0.4, 4.0,
// 1, zeros and the two bytes +0x60C/+0x60D - 0x24 records of 0x28 bytes with words +8..+0x1C and +0x50..+0x58 cleared and
// 0x24 records of 0x24 bytes with words -0xC..+8 cleared, 1 at +0x65C and zeros at +0x658/+0x660); destruction: from
// 0x08CE9030 down to 0x08CE6E10, func_088CBBC8 and func_08B28DC4 each. Anything else does nothing.


// func_08B0029C(self = a0, a1 = a record, a2 = three floats, a3 = four floats, t0 = three floats or 0, t1 = three floats or
// 0, t2/t3 = flag bytes, and three stack words at caller sp +0/+4/+8): stores a3's four floats at self, a2's three at
// self + 0x20, t0's three at self + 0x38 (zeroed when t0 is 0) and clears self + 0x80 and +0x8C; the flags halfword at
// +0xE2 is t2 | (t3 ? 0x10 : 0) | (stack byte +0 ? 0x20 : 0) | (stack byte +8 ? 0 : 0x80), +0xE0 the stack word +4's low
// half, +0xEA/+0xEC and +0xD0 become -1, +0xD8 the record, +0xD4/+0xDC zero. func_08BAE278(self, frame +0x14 from the
// record +0x24 and its +4/+8) and func_08BAC284(frame +0x20, that, the record + 0x24) run, func_08C46508(self + 0x20,
// frame +0x20), +0xE6 becomes 0x7FFF, func_08AFEA88(self), func_08AFEB98(self, frame +8: t1's three floats or zeros),
// func_08B055EC(self + 0xB0) and func_08AFFCF4(self). v0 = its answer.

// func_089C8818(out = a0, first = a1, second = a2): compares *first and *second: func_08B88F44 builds the temporary at
// frame +0xC; when *first is shorter the empty rep 0x08D74FD0 goes on (and the call below depends on the caller's s5,
// which this path never sets); otherwise func_08B8D96C(frame +8, first, len(*first) - len(*second), len(*second)) slices
// *first, func_08B36170(*second, that, the smaller length) answers, and the slice is released (empty rep 0x08D74FD0,
// func_08B60804 then func_08B8542C). func_08B89054(frame +0xC, second) runs when that test answered nonzero or the
// lengths were equal. Unless func_08AA43CC(frame +0xC) answers nonzero: func_08A5CC40(out), func_08B89124(frame +4, it,
// frame +0xC), func_08B855E0(frame +0xC, frame +4) and the release of frame +4. func_08AA58D4(out, frame +0xC) and the
// release of frame +0xC finish. v0 = out.


// The locale-step pair 0x08C55DF0 / 0x08C55FDC (identical code): (out = a0, text = a1, flag = a2): byte out cleared, the
// record at text + *(text - 0xC); when its word +0x14 is zero the object at +0x70 (when set) is asked through
// func_08C70238, and with `flag` set and the record's word +0xC holding 0x1000 the digit run is walked: the first character
// at the record's +0x78 object (its byte cursor +8 while below +0xC, else its virtual slot 0x24), then the table at the
// record's +0x7C (word +0x18's bytes, sign-extended) tests bit 8 of that character and of each following one (each cursor
// read, an exhausted cursor asking the slot, -1 ending the run): the loop consumes characters while their bit 8 stays set,
// and 0x08C55F84's exit (a 0xFF character) keeps two flag bits. The record's word +0x14 goes into the flags (4 when it is
// set or the flags are not zero) and func_08C54CFC(record, flags | word +0x14) ends the function; otherwise byte out
// becomes 1 and the function returns v0 = 1. func_08B5A9FC reports a missing table.
void locale_digit_step(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = out;
    ctx.gpr[17] = flag;
    ctx.gpr[16] = text;
    u32 data = word(rt, text);
    put8(rt, out, 0u);
    const u32 record = text + word(rt, data - 0xCu);
    u32 flags = 0u;
    bool treat = false;  // every path of the original ends in the flags step
    if (word(rt, record + 0x14u) != 0u) {
        treat = true;
    } else {
        const u32 inner = word(rt, record + 0x70u);
        if (inner != 0u) {
            call_set(rt, ctx, 0x08C70238u, {inner});
            data = word(rt, text);
        }
        if (flag != 0u) {
            treat = true;  // straight to the flags step with flags 0
        } else {
            const u32 now = text + word(rt, data - 0xCu);
            if ((word(rt, now + 0xCu) & 0x1000u) == 0u) {
                treat = true;
            } else {
                const u32 state = word(rt, now + 0x78u);
                const auto next_character = [&](bool advance) {
                    const u32 cursor = word(rt, state + 8u);
                    if (cursor < word(rt, state + 0xCu)) {
                        const u32 value = byte_u(rt, cursor);
                        if (advance) put(rt, state + 8u, cursor + 1u);
                        return value;
                    }
                    return call_virtual_at(rt, ctx, state, 0x24u, {state});
                };
                u32 character = next_character(false);
                u32 table = word(rt, text + word(rt, word(rt, text) - 0xCu) + 0x7Cu);
                if (table == 0u) call_set(rt, ctx, 0x08B5A9FCu, {});
                if (character + 1u == 0u) {
                    flags = 2u;
                    treat = true;
                } else if ((sext8(byte_u(rt, word(rt, table + 0x18u) + (character & 0xFFu))) & 8u) == 0u) {
                    treat = true;
                } else {
                    for (;;) {
                        const u32 value = next_character(true);
                        if (value + 1u == 0u) {
                            flags = 2u;
                            treat = true;
                            break;
                        }
                        const u32 peeked = next_character(false);
                        if (peeked == 0xFFFFFFFFu) {
                            flags = 2u;
                            treat = true;
                            break;
                        }
                        const u32 index = (peeked & 0xFFu) + word(rt, table + 0x18u);
                        if ((sext8(byte_u(rt, index)) & 8u) == 0u) {
                            treat = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    const u32 record_now = text + word(rt, word(rt, text) - 0xCu);
    const u32 held = word(rt, record_now + 0x14u);
    if (held != 0u || flags != 0u) {
        tail_call(ctx, 0x08C54CFCu, {record_now, held | (flags | 4u)});
        return;
    }
    put8(rt, out, 1u);
    ctx.gpr[2] = 1u;
}
void func_08C55FDC(Runtime &rt, AllegrexContext &ctx) { locale_digit_step(rt, ctx); }


// func_08A4C580(out = a0, count = a1, holder = a2): with count above zero and holder's object (func_08A5BB7C(0x08CAED38)
// when null): func_08A76F20(object, func_08A85448()) chooses; nonzero asks the object's virtual slot 0x3C, serialises the
// answer (func_08B29E88(0, frame +0xC, 4)), builds the value with func_08A85B1C(frame +8, that), assigns it to `out`
// (func_08A5BAD8 unless it is the frame slot) and destroys the slot. Zero tries func_08A76F20(object,
// func_08A82A5C()) instead: nonzero asks slot 0x40 and serialises 8 bytes (frame +0x10) the same way through frame +4; a
// zero there asks slot 0xDC, then slot 0xD8 of the object (reloaded from holder), serialises that with func_08B29E88(0,
// slot 0xDC's answer, slot 0xD8's answer), builds the value at the frame and assigns it to `out` the same way.
void func_08A4C580(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[18] = out;
    ctx.gpr[17] = holder;
    if (signed_of(count) <= 0) return;
    u32 object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    if (call_set(rt, ctx, 0x08A76F20u, {object, call_set(rt, ctx, 0x08A85448u, {})}) != 0u) {
        object = word(rt, holder);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
        const u32 value = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
        put(rt, frame.at(0xCu), value);
        const u32 serialised = call_set(rt, ctx, 0x08B29E88u, {0u, frame.at(0xCu), 4u});
        call_set(rt, ctx, 0x08A85B1Cu, {frame.at(8u), serialised});
        if (out != frame.at(8u)) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(8u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        return;
    }
    object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    if (call_set(rt, ctx, 0x08A76F20u, {object, call_set(rt, ctx, 0x08A82A5Cu, {})}) != 0u) {
        object = word(rt, holder);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
        const u32 vtable = word(rt, object);
        const u32 value = word(rt, vtable + 0x40u);
        ctx.gpr[2] = value;
        const GuestResult answer = call_keep(rt, ctx, value, {object});
        put(rt, frame.at(0x10u), answer.v0);
        put(rt, frame.at(0x14u), answer.v1);
        const u32 serialised = call_set(rt, ctx, 0x08B29E88u, {0u, frame.at(0x10u), 8u});
        call_set(rt, ctx, 0x08A85B1Cu, {frame.at(4u), serialised});
        if (out != frame.at(4u)) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(4u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        return;
    }
    object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    const u32 first = call_virtual_at(rt, ctx, object, 0xDCu, {object, 0u});
    ctx.gpr[19] = first;
    object = word(rt, holder);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    const u32 second = call_virtual_at(rt, ctx, object, 0xD8u, {object});
    const u32 serialised = call_set(rt, ctx, 0x08B29E88u, {0u, first, second});
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), serialised});
    if (out != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

} // namespace
} // namespace new_batch_24
} // namespace gt::game

#define NEW24(address, name, kind) GT_DECOMP(address, gt::game::new_batch_24::name, gt::ReturnKind::kind)
NEW24(0x08BAFD74u, func_08BAFD74, Word);
NEW24(0x08C2B298u, func_08C2B298, Word);
NEW24(0x08C3548Cu, func_08C3548C, Word);
NEW24(0x08879190u, func_08879190, Word);
NEW24(0x08C72614u, func_08C72614, Word);
NEW24(0x08C75FF8u, func_08C75FF8, Word);
NEW24(0x08B27C28u, func_08B27C28, Word);
NEW24(0x08B3141Cu, func_08B3141C, Word);
NEW24(0x08C6F818u, func_08C6F818, Word);
NEW24(0x08B5853Cu, func_08B5853C, Word);
NEW24(0x08C55FDCu, func_08C55FDC, Word);
NEW24(0x08A4C580u, func_08A4C580, Word);
