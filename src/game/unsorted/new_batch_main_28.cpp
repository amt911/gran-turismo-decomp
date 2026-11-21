// New batch 28 (main session): hand-written drafts from `tools/disasm`, not gated yet.
//   0x089283AC: sets the four state flags of an object (four (index, byte) pairs, chosen by the word at +0x1D1C) and
//               then runs the transition the word at +0x1FB8 selects: report the end object (0x08927D14 tail call),
//               a full step (0x08927BE4 tail call), or for 0x0B a virtual call and 0x089281E4.
//   0x0888D9A8: updates one request entry of an object's tables (see update_entry).
//   0x08B04000: two passes over the object's elements (see func_08B04000).
// The batch's other two candidates (0x08C174D4, 0x08B452EC) are not written here: each is two nested loops whose blocks
// are entered from several places plus float copies through struct assignments; the drafts were not confident enough to
// hand in blind. 0x0897BC08 was dropped from the batch: src/game/unsorted/hot_batch_117.cpp already defines it (and
// leaves it unregistered, compared 0 in the full gate), so a second body would be duplicate work. See the PR notes.
// `tools.arg_reads` verdicts for the calls of this file: 0x0888A878 is listed with a2/a3, which it only loads with
// lwl/lwr pairs (both halves written, so the incoming value never shows); 0x088DA1C8 is listed with a3, a loop counter
// it clears at 0x088DA238 before the only read (0x088DA20C); 0x08B00DF0 is listed with a2, which 0x08B00E14 reads
// only after 0x08B00E2C set it (the loop's back edge); 0x08AFFA70 really does take the cursor's t0 as its fifth
// argument and gets it (0x08AFFA88 copies it to s5). 0x08B00E90 is listed with t1, which nothing in 0x08B04000 writes:
// the original hands on its own caller's leftover, and all 0x08B00E90 does with it is store it into its frame's slot
// 0x88 (0x08B00FF4) — harmless unless that slot is read before 0x08B01040 writes it; kept unfixed because the value
// cannot be reproduced.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_28 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose argument registers are set like the original sets them before its `jal` (a0-a3, t0-t3 in order, so a
// callee that reads one of them sees this call's value and not a previous callee's leftover); v0 afterwards holds its result.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The callee-saved registers the original saves in its prologue and restores on every exit (s0..s<N-1>): the gate compares
// them, so a body that borrows them puts the caller's values back.
template <unsigned N>
struct SavedRegisters {
    AllegrexContext &ctx;
    u32 saved[N];
    explicit SavedRegisters(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < N; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < N; ++i) ctx.gpr[16u + i] = saved[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;
};

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_half(u32 value) { return static_cast<s32>(static_cast<std::int16_t>(value)); }
// MIPS `ins rt, rs, pos, size`: `size` bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}
s32 signed_byte(u32 value) { return static_cast<s32>(static_cast<std::int8_t>(static_cast<std::uint8_t>(value))); }
// A guest call whose v1 the next instruction reads (a 64-bit answer split over v0/v1): both stay in the context.
u32 call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result.v0;
}
// The frame pointer is callee-saved and compared as well.
struct SavedFramePointer {
    AllegrexContext &ctx;
    u32 saved;
    explicit SavedFramePointer(AllegrexContext &c) : ctx(c), saved(c.gpr[30]) {}
    ~SavedFramePointer() { ctx.gpr[30] = saved; }
    SavedFramePointer(const SavedFramePointer &) = delete;
    SavedFramePointer &operator=(const SavedFramePointer &) = delete;
};

// ---------------------------------------------------------------------------------------------------------------
// 0x08B04000 (a0 = the object): two passes. (1) From the index at +0x1C, walk the 240-byte elements of the array the
// pointer at +0x24 holds (next index at +0xE8, negative ends): each element whose halfword at +0xE2 lacks 0x80 is
// handed to 0x08B00DF0 and, unless one of two flag bits says otherwise, the element's flags at +0xE2 lose bit 14
// (when bit 4 is set, bit 0x4000 is clear, and the halfword at +2 of the 6-byte table entry at +0x7C + 6*(+0xEA)
// has bit 14 clear too). (2) For the slot at +0xC0, the halfword list [0x2C, 0x30) of element indices: each candidate
// element past the current one is compared (its float at +0x98 + 4*slot against the current element's at +0xA4 + 4*slot)
// and, through 0x08AFD540 and 0x08AFFA70, merged: the 0x08B02FD4 report, the two virtuals at +0xDC's vtable slot 8, and
// 0x08B00E90 close the round. v0 = the last cursor step, 0 on an early return, or 0x08B0151C's answer at the top.

// ---------------------------------------------------------------------------------------------------------------
// 0x089283AC sets four state bytes: 0x08916474 gives the table, 0x089161A0 writes one byte at 0x08CF205C + index.
void set_state_pairs(Runtime &rt, AllegrexContext &ctx, const u32 (&pairs)[4][2]) {
    for (const auto &pair : pairs) {
        const u32 table = call_set(rt, ctx, 0x08916474u);
        call_set(rt, ctx, 0x089161A0u, {table, 0u, pair[0], pair[1]});
    }
}

// 0x089283AC (a0 = the object, a1 = the argument handed on to 0x089275B0 / 0x08927EAC / 0x089200CC).
void func_089283AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];  // s4
    SavedRegisters<5> saved(ctx);  // s0-s4
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = 0xAu;  // v0 before the first compare
    // 0x089283D4: the flags, by the word at +0x1D1C (all four pairs are (state, byte) writes).
    if (word(rt, self + 0x1FB8u) == 0xAu) {
        const u32 state = word(rt, self + 0x1D1Cu);
        if (state == 1u || state == 2u) {
            set_state_pairs(rt, ctx, {{1u, 1u}, {0u, 0u}, {2u, 0u}, {3u, 0u}});
        } else if (state == 3u) {
            set_state_pairs(rt, ctx, {{1u, 0u}, {0u, 0u}, {2u, 1u}, {3u, 0u}});
        } else if (state == 4u) {
            set_state_pairs(rt, ctx, {{1u, 0u}, {0u, 0u}, {2u, 0u}, {3u, 1u}});
        } else {
            set_state_pairs(rt, ctx, {{1u, 0u}, {0u, 1u}, {2u, 0u}, {3u, 0u}});
        }
    } else {
        set_state_pairs(rt, ctx, {{0u, 0u}, {1u, 0u}, {2u, 0u}, {3u, 0u}});
    }
    // 0x089285B0: the word at +0x1FB8 now selects the transition (s0; v0 = 3, then 0x0B).
    const u32 phase = word(rt, self + 0x1FB8u);
    if (phase == 0xBu) {
        // 0x0892870C: idle: one virtual call; a yes answer hands over to 0x089281E4(self, 3).
        const u32 busy = call_set(rt, ctx, 0x089273B4u, {self});
        if (busy != 0u) return;  // v0 = busy
        const u32 object = call_set(rt, ctx, 0x089273D8u, {self});
        const u32 slot = word(rt, word(rt, object) + 0xCu);
        const u32 answer = call_indirect(rt, ctx, slot, {object, 0x20u});
        if (answer == 0u) return;  // v0 = 0
        tail_call(ctx, 0x089281E4u, {self, 3u});
        return;
    }
    if (phase != 0u && phase != 3u) {  // 0x089285C4: the other states only report back
        ctx.gpr[2] = 0xBu;
        return;
    }
    // 0x089285CC: 0x089273B4 still busy sends everything to the report path.
    if (call_set(rt, ctx, 0x089273B4u, {self}) != 0u) {
        if (phase != 0u) {
            if (word(rt, self + 0x1FCu) == 0x22u) {
                ctx.gpr[2] = 0x22u;
                return;
            }
        }
        tail_call(ctx, 0x08927D14u, {self, self + 0x2184u});
        return;
    }
    // 0x08928618: one step; 0x08927EAC answers the state, 0x08927E4C the mode when the state is not 1.
    call_set(rt, ctx, 0x089275B0u, {self, other});
    const u32 previous = ctx.gpr[2];  // s3: this call's answer, read in the next call's delay slot
    const u32 step = call_set(rt, ctx, 0x08927EACu, {self, other});  // s0
    u32 mode = 0u;   // s2
    u32 answer = 0u; // v1
    if (step != 1u) {
        answer = call_set(rt, ctx, 0x08927E4Cu, {self});
        mode = word(rt, self + 0x2050u);
    } else {
        mode = 1u;
    }
    const bool same = previous == 1u;  // s3, 0x08928654
    // 0x08928678: the flags differ -> tell the state machine (3, or the object at +0x2194), v0 = 1 either way.
    if (same || answer == 1u || byte_at(rt, self + 0x2054u) == step) {
        ctx.gpr[2] = 1u;
    } else if (mode == 0u) {
        call_set(rt, ctx, 0x089281E4u, {self, 3u});
        ctx.gpr[2] = 1u;
    } else {
        call_set(rt, ctx, 0x089274C8u, {self, self + 0x2194u});
        ctx.gpr[2] = 1u;
    }
    // 0x0892869C: the byte at +0x2054 remembers the state; only mode 1 goes on.
    rt.memory().store8(self + 0x2054u, static_cast<std::uint8_t>(step));
    if (mode != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 slot = self + 0x2194u;  // s0
    call_set(rt, ctx, 0x08927D14u, {self, slot});
    if (same) {
        // 0x089286BC: the slot's word at +0xC moves to +8, then the two reports.
        put(rt, slot + 8u, word(rt, slot + 0xCu));
        call_set(rt, ctx, 0x089274C8u, {self, slot});
        call_set(rt, ctx, 0x089200CCu, {self, other, word(rt, self + 0x1FB0u)});
    }
    tail_call(ctx, 0x08927BE4u, {self, slot, 0u, 0u});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x0888D9A8 (a0 = the object, a1 = handed to 0x088D62FC, a2 = the request). One request entry of the object's table at
// +0x34 (168 bytes each, entries at +0xD78; the ids at +0x1018; counters 432 bytes each at +0x1560):
// the state is copied out to a stack block at sp+0x10, patched from the request, run through the id lookups, and copied
// back. sp+0..sp+8 is a three-word argument block. `fresh` is the request's byte at +5.
void update_entry(Runtime &rt, AllegrexContext &ctx, u32 self, u32 argument, u32 request, u32 base) {
    const u32 buffer = base + 0x10u;              // s7
    const s32 index = signed_half(half_at(rt, request));  // s2
    // 0x0888D9F4: the state comes out of the table entry into the stack block.
    call_set(rt, ctx, 0x0888A878u, {buffer, word(rt, self + 0x34u) + static_cast<u32>(index * 168) + 0xD78u});
    // 0x0888DA20: patch the block: a flag byte, the request's halfword, a count of one, and the counter byte.
    const s32 byte4 = signed_byte(byte_at(rt, request + 4u));
    const s32 byte5 = signed_byte(byte_at(rt, request + 5u));
    const u32 half2 = half_at(rt, request + 2u);
    const u32 old_counter = byte_at(rt, buffer + 0x13u);
    const u32 fresh = byte5 != 0 ? 1u : 0u;  // s4: `sltu s4, zero, v0`
    u32 counter_byte = (static_cast<u32>(byte4) + 1u) & 0xFFu;
    if (fresh != 0u) counter_byte = 0u;  // movn
    put8(rt, buffer + 0x11u, 1u);
    const u32 count = old_counter + 1u;  // a0
    put8(rt, buffer + 0x13u, counter_byte);
    put_half(rt, buffer + 0x14u, half2);
    put8(rt, buffer + 0x12u, fresh);
    // 0x0888DA54: a new request carries its own 64-bit pair, an old one keeps the block's.
    u32 pair_low;
    u32 pair_high;
    if (fresh != 0u) {
        pair_low = word(rt, request + 8u);
        pair_high = static_cast<u32>(static_cast<s32>(pair_low) >> 31);
    } else {
        pair_low = word(rt, buffer + 0x20u);
        pair_high = word(rt, buffer + 0x24u);
    }
    put(rt, buffer + 0x20u, pair_low);
    put(rt, buffer + 0x24u, pair_high);
    // 0x0888DA90: `count` words from the request's +0x10 and `count` halfwords from its +0x50 into the block.
    if (static_cast<s32>(count) > 0) {
        for (u32 i = 0u; i < count; ++i) {
            put(rt, buffer + 0x28u + i * 4u, word(rt, request + 0x10u + i * 4u));
            put(rt, buffer + 0x68u + i * 4u, static_cast<u32>(signed_half(half_at(rt, request + 0x50u + i * 2u))));
        }
    }
    // 0x0888DAB4: the entry's id, then either the state it already had (a new request) or its stored one.
    const u32 moved = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), static_cast<u32>(index)});  // s3
    const u32 holder = word(rt, self + 4u);
    const u32 request_word = word(rt, request + 0x70u);
    if (fresh != 0u) {
        const u32 entry = word(rt, self + 0x34u) + static_cast<u32>(index * 168) + 0x1018u;
        // 0x0888DAE8: the stored pair decides whether the request replaces it (the byte at +0x1C35 flips the sense).
        if (byte_at(rt, entry + 0x11u) != 0u) {
            const u32 stored_low = word(rt, entry + 0x20u);
            const u32 stored_high = word(rt, entry + 0x24u);
            bool replace;
            if (stored_low == pair_low && stored_high == pair_high) {
                replace = false;  // the same pair: nothing to write back
            } else {
                bool flag;  // a1
                if (pair_high < stored_high) flag = true;
                else if (pair_high != stored_high) flag = false;
                else flag = pair_low < stored_low;
                replace = byte_at(rt, word(rt, self + 0x34u) + 0x1C35u) != (flag ? 1u : 0u);
            }
            if (replace) {
                call_set(rt, ctx, 0x0888A878u, {entry, buffer});
                call_set(rt, ctx, 0x0888AF70u, {word(rt, self + 0x34u), buffer});
            }
        }
        // 0x0888DB48: the argument block, then the update of the moved entry (s3).
        put(rt, base, static_cast<u32>(signed_half(half_at(rt, request))));
        put(rt, base + 4u, request_word);
        put(rt, base + 8u, static_cast<u32>(signed_half(half_at(rt, request + 2u))) + 1u);
        put(rt, moved + 0x38u, call_set(rt, ctx, 0x088D2774u, {word(rt, holder + 0x618u), base, holder}));
        call_set(rt, ctx, 0x088D0934u, {moved, static_cast<u32>(byte4), request_word});
        call_set(rt, ctx, 0x088D097Cu, {moved, word(rt, request + 8u)});
        put(rt, moved + 0x194u, request_word);
        put(rt, moved + 0x70u, request_word);
        // 0x0888DBB4: the counter by index, wrapped to a halfword; its remainder modulo 100 picks the slot at +0x1C.
        const u32 table = word(rt, self + 0x34u) + static_cast<u32>(index) * 432u + 0x1560u;
        const u32 wrapped = static_cast<u32>(signed_half(half_at(rt, table + 2u) + half_at(rt, request + 2u)));
        put_half(rt, table + 4u, wrapped);
        const s32 slot = static_cast<s32>(wrapped) - 1;
        put(rt, table + 0x1Cu + static_cast<u32>(slot % 100) * 4u, pair_low);
    } else {
        // 0x0888DC1C: an old request only fills the argument block and refreshes the moved entry once.
        put(rt, base, static_cast<u32>(index));
        put(rt, base + 4u, request_word);
        put(rt, base + 8u, static_cast<u32>(signed_half(half_at(rt, request + 2u))));
        put(rt, moved + 0x38u, call_set(rt, ctx, 0x088D26B4u, {word(rt, holder + 0x618u), base, static_cast<u32>(byte4)}));
        call_set(rt, ctx, 0x088D0934u, {moved, static_cast<u32>(byte4), request_word});
    }
    // 0x0888DC58: the block goes back, then the two id lookups must agree before the last call.
    call_set(rt, ctx, 0x0888A878u, {word(rt, self + 0x34u) + static_cast<u32>(index * 168) + 0xD78u, buffer});
    call_set(rt, ctx, 0x0888AD94u, {word(rt, self + 0x34u), static_cast<u32>(index), buffer});
    if (call_set(rt, ctx, 0x088D62FCu, {argument}) != 0u) return;
    if (call_set(rt, ctx, 0x088DA1C8u, {word(rt, self + 4u), static_cast<u32>(index)}) != 1u) return;
    const u32 first = call_pair(rt, ctx, 0x0888CAFCu, {self, static_cast<u32>(index), fresh, 1u});
    const u32 second = ctx.gpr[3];  // v1 of that call, compared at 0x0888DD20
    put(rt, base, first);
    put(rt, base + 4u, second);
    put(rt, buffer + 0xA0u, first);
    put(rt, buffer + 0xA4u, second);
    u32 selected;
    if (fresh != 0u) {
        selected = word(rt, word(rt, holder + 0x618u) + 0x20Cu);
    } else {
        const u32 slot_index = word(rt, moved + 0x2Cu);
        selected = word(rt, word(rt, holder + 0x618u) + slot_index * 12u);
    }
    if (call_set(rt, ctx, 0x08918438u, {word(rt, self + 8u), selected, 0u}) == 0u) return;
    if (second != 0u) return;
    call_set(rt, ctx, 0x0888B24Cu, {self, 0u, 0u, base});
}

// 0x0888DD40: v0 = the request's byte at +5 (sign-extended); when it is set, 0x089936DC(self->+0x18) answers instead.
void finish_request(Runtime &rt, AllegrexContext &ctx, u32 self, u32 request) {
    const u32 flag = static_cast<u32>(signed_byte(byte_at(rt, request + 5u)));
    ctx.gpr[2] = flag;
    if (flag != 0u) ctx.gpr[2] = call_set(rt, ctx, 0x089936DCu, {word(rt, self + 0x18u)});
}


} // namespace
} // namespace new_batch_28
} // namespace gt::game


#define NEW28(address, name, kind) GT_DECOMP(address, gt::game::new_batch_28::name, gt::ReturnKind::kind)
NEW28(0x089283ACu, func_089283AC, Word);
