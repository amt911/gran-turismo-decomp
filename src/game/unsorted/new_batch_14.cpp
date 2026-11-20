// New batch 14: eighty small functions (100-108 bytes) written by hand from `tools.disasm` as structured C. Delay slots are read
// as "before the branch / call" (a `jal`'s delay slot runs before the callee, a branch-likely's only when taken). The callee-saved
// registers the original keeps values in are set where the asm sets them (a callee may read them) and put back at the end; `v0`
// (or `f0` for the float ones) ends as the asm leaves it on every exit path, including the early returns. Guest memory goes
// through word()/put()/rt.memory(), calls through call() below. The frame sizes are the original prologues'; where a function has
// its own body its `GuestFrame` sits on the signature line so tools.frame_audit reads it. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_14 {
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

// wsbw: the bytes of a word reversed (these records are big-endian).
u32 swapped(u32 value) { return __builtin_bswap32(value); }

// A byte passed as an argument sign-extended into its 32-bit register (`seb`).
u32 sign_extended_byte(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }

// s0-s3 are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 4u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 4u; ++i) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[4];
};

// --- copying 36-byte records or arrays of objects ----------------------------------------------------------------
// 0x08BF0E8C and 0x08BF100C (a0 = first, a1 = end, a2 = destination): each 36-byte record of [a0, a1) is copied (nine floats:
// eight in pairs from +0, then the one at +0x20) to the destination when it is not null, source and destination advancing
// together. v0 ends as the advanced destination (or a2 itself when the range is empty).
void copy_records(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (begin == end) {
        ctx.gpr[2] = destination;
        return;
    }
    u32 source = begin;
    do {
        if (destination != 0u) {
            for (unsigned i = 0u; i < 4u; ++i) {
                put_float(rt, destination + i * 8u, float_at(rt, source + i * 8u));
                put_float(rt, destination + i * 8u + 4u, float_at(rt, source + i * 8u + 4u));
            }
            put_float(rt, destination + 0x20u, float_at(rt, source + 0x20u));
        }
        source += 0x24u;
        destination += 0x24u;
    } while (source != end);
    ctx.gpr[2] = destination;
}

// 0x08C026FC, 0x08C121C8, 0x08C25298, 0x08C2DC4C and 0x08C30A6C (a0 = first, a1 = end, a2 = destination): every element of
// [a0, a1) — `stride` bytes each — is handed to `release` as (destination, element) when the destination is not null, both
// pointers advancing together. v0 ends as the advanced destination (a2 when the range is empty).
void copy_elements(Runtime &rt, AllegrexContext &ctx, u32 stride, u32 release) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (begin != end) {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[18] = end;
        u32 source = begin;
        do {
            ctx.gpr[16] = source + stride;
            ctx.gpr[17] = destination;
            if (destination != 0u) call(rt, ctx, release, {destination, source});
            source += stride;
            destination += stride;
        } while (source != end);
    }
    ctx.gpr[2] = destination;
}

// 0x08B89238 (a0 = first, a1 = end, a2 = destination): the same walk over 32-byte elements, where each copied element also
// takes the word at +0x10 verbatim. v0 ends as the advanced destination.

// --- the "advance a pointer inside a range pair" family ----------------------------------------------------------
// 0x08C4130C, 0x08C41374, 0x08C415A4, 0x08C4160C, 0x08C41674, 0x08C41934 and 0x08C41A04 (a0 = four-word range table,
// a1 = pointer to the advance): the word pointed at is moved into the range it falls in — the table's word 0 is the base of
// the first range, word 1 of the second, word 2 and 3 delimit the two ranges — and the moved pointer is stored back; v0 is it
// too. A zero advance stays zero (the store happens on that path as well).
void advance_in_range(Runtime &rt, AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 advance = word(rt, holder);
    u32 result = 0u;
    if (advance != 0u) {
        const u32 first = word(rt, table);
        const u32 second = word(rt, table + 4u);
        const u32 third = word(rt, table + 8u);
        const u32 fourth = word(rt, table + 0xCu);
        u32 base = first;
        if (fourth != 0u || second != 0u) {
            if (third < fourth) {
                base = advance < fourth ? first : second;
            } else {
                base = advance < third ? second : first;
            }
        }
        result = base + advance;
    }
    put(rt, holder, result);
    ctx.gpr[2] = result;
}
void func_08C4130C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { advance_in_range(rt, ctx); }
void func_08C41374(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { advance_in_range(rt, ctx); }
void func_08C415A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { advance_in_range(rt, ctx); }
void func_08C4160C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { advance_in_range(rt, ctx); }
void func_08C41674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { advance_in_range(rt, ctx); }
void func_08C41934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { advance_in_range(rt, ctx); }
void func_08C41A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { advance_in_range(rt, ctx); }

// --- objects whose inline member is released through its vtable ---------------------------------------------------
// 0x0892B974, 0x0880A048, 0x0880AEC4 and 0x0880BDC8 (a0 = this, the member at this + member): the vtable word(s) are stored
// (one, or two with the second at +0x38), the member — {pointer, owned} at this + member — is released through the first
// virtual of its vtable when present, both words cleared, and the object finishes through `finish`.
void store_vtables_and_release(Runtime &rt, AllegrexContext &ctx, u32 member, u32 first_vtable, u32 second_vtable, u32 finish) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + member;
    put(rt, self, first_vtable);
    if (second_vtable != 0u) put(rt, self + 0x38u, second_vtable);
    const u32 held = word(rt, self + member + 4u);
    if (held != 0u) {
        const u32 inner = word(rt, held);
        call_indirect(rt, ctx, word(rt, inner), {held, word(rt, self + member)});
        put(rt, self + member + 4u, 0u);
        put(rt, self + member, 0u);
    }
    tail_call(ctx, finish, {self});
}
// 0x08A3DB74 (a0 = this): like the four above with the member at +0x14, but the vtable 0x08CD13B0 is stored and the member's
// pointer first goes through 0x08A3D718(this + 0x14's object) before the release.
// 0x0880ABAC (a0 = this, a1 = a pointer to a value): when the word pointed at exists, the name from 0x0880AA4C(this) is
// looked up on it (0x08A76F20); found, 0x08A5BAD8(this, the word pointed at) runs and v0 = 1, otherwise v0 = 0.

void func_0880A048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_vtables_and_release(rt, ctx, 0x118u, 0x08CB9C78u, 0x08CB9ED0u, 0x08A0298Cu);
}
void func_0880BDC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    store_vtables_and_release(rt, ctx, 0xE8u, 0x08CBA148u, 0x08CBA380u, 0x0896BC0Cu);
}

// --- constructors that move two vtables of a source object into a destination ------------------------------------
// 0x08C79E44 and 0x08C7978C (a0 = destination, a1 = source object): the primary vtable of the source is stored at the
// destination with its secondary pointer at the displacement the vtable names (from source +0xC), the shared part is
// initialised (0x08C786B4) with the destination's sub-object and the same for the source's second vtable (its pointer from
// source +8 at its own displacement; 0x08C7978C also clears +4 and hands the sub-object at +8). v0 ends as the source's
// second vtable pointer.
void copy_object_vtables(Runtime &rt, AllegrexContext &ctx, u32 sub_object, bool clear_word) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = source;
    ctx.gpr[16] = self;
    const u32 primary = word(rt, source);
    put(rt, self, primary);
    put(rt, self + word(rt, primary - 0xCu), word(rt, source + 0xCu));
    call(rt, ctx, 0x08C786B4u, {self + sub_object});
    const u32 secondary = word(rt, source + 4u);
    const u32 displacement = word(rt, secondary - 0xCu);
    put(rt, self, secondary);
    if (clear_word) put(rt, self + 4u, 0u);
    put(rt, self + displacement, word(rt, source + 8u));
    ctx.gpr[2] = secondary;
}

// 0x08C7A688 (a0 = destination, a1 = source): the primary vtable with the word at source +0x20 at its displacement, the word
// at source +0x24 at destination +8, the sub-object at destination +0xC initialised (0x08C786B4) and 0x08C553B0(destination,
// source + 4) finishing. v0 = that last call's result.

// --- small forwarding shapes ---------------------------------------------------------------------------------------
// 0x08ACCE9C (a0 = object, a2 = a value): the member at this + 0x2C8 is offered the first import with (a0, a1) as they came;
// when that answers 0 the second import gets (member, the value, and a 16-byte scratch above the frame). v0 = 1 only when
// both answered 0.

// 0x08B09448 (a0 stored in the record at sp + 0xC): a four-word record {0, 0x2000, 0x08B098A8, a0} is built on the stack,
// 0x08B2E314(3, -6) and 0x08B2E134 feed 0x08B2E7DC(record + 8, record, size), and its result is stored to the global
// 0x08D4744C and handed to 0x08B2E214 (the store happens before that call). v0 = the last call's result.

// 0x08B0C6B8 (a0 = a name, a1 = a value): the name is looked up (0x08B0C43C); with an object found, its second virtual
// (vtable +0x80) finishes the work with (object, name, value) as a tail call, otherwise v0 = -1.
void func_08B0C6B8(psprecomp::Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    u32 function = 0u;
    u32 found = 0u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = value;
        found = call(rt, ctx, 0x08B0C43Cu, {self});
        if (found == 0u) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        function = word(rt, word(rt, found) + 0x80u);
    }
    tail_call(ctx, function, {found, self, value});
}

// 0x08B5A270 (a0 = a table, a1 = a value): the table's slot number comes from 0x08B5A018(a0); unless it is 0xFF the record
// is prepared (0x08B59B38(table, table + 0x202, 2, slot)) and the word at table + slot * 16 + 8 is the answer. v0 = 0 for
// slot 0xFF.

// 0x08C3D0C8 (a0 = object, a1 = first, a2 = second): 0x08B79BAC(first, second, word(object + 4) - second) fills the tail
// and the object's length word shrinks by the 4-byte rounded span (second - first). v0 = first.
void func_08C3D0C8(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    ctx.gpr[18] = self;
    ctx.gpr[17] = first;
    ctx.gpr[16] = second;
    call(rt, ctx, 0x08B79BACu, {first, second, word(rt, self + 4u) - second});
    const u32 span = (second - first) & ~3u;
    put(rt, self + 4u, word(rt, self + 4u) - span);
    ctx.gpr[2] = first;
}

// 0x08C52288 (a0 = a circular list's head): every node's member at +0x14 is released (0x08B61FC0) when present and the node
// itself is freed (0x08B35254), walking the ring until the next node is the head again. v0 ends as the last free's result
// (or the head's first word when the ring is empty).

// 0x08C76340 (a0 = object, a1 = first, a2 = second, a3 = a helper object): the helper is asked for the base word of the
// object (0x08B35F60) and the work finishes in a tail call with (object, first - word(object), second - first, helper, the
// helper's answer).
void func_08C76340(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 helper = ctx.gpr[7];
    const u32 base = word(rt, self);
    ctx.gpr[19] = second - first;
    ctx.gpr[18] = helper;
    ctx.gpr[17] = self;
    ctx.gpr[16] = first - base;
    const u32 answer = call(rt, ctx, 0x08B35F60u, {helper, first, second, helper});
    tail_call(ctx, 0x08C75FF8u, {self, first - base, second - first, helper, answer});
}

// 0x0885E430 (a0 = an object): four getters are chained (0x08916474, 0x08916194, 0x088C7F64, 0x088C78B4), the object's word
// at +0x5AC is flipped to 1 (0x088D7214) and back to 0 (0x088DAFBC), and the member at word(word(object) + 0x3F50) + 0x7C
// finishes as a tail call.

// 0x08869D70 (a0 = an object): 0x08869CCC(object, 0), 0x08869CE8(object, 0), then the block at word(object + 4) + 0x4238 is
// initialised twice (0x0886BFC0, 0x0886BF44) and stepped four times (0x0886B600; no argument registers read).

// 0x08885E48 (a0 = this): the vtable 0x08CC16E8 is stored, 0x0888598C(this) runs, the owned object at +0xC is released
// (0x08885CEC, then 0x08B64654) and cleared, then the vtable 0x08CBF428 is stored and 0x08B64654(this) finishes.

// 0x088EEEB8 (a0 = an object holding an array of 0x240/0x90 elements from +4): the member at +8 is locked
// (0x08B344E0), each element is prepared (0x088EED18) and the member is released (0x08B3457C) as a tail call.
void func_088EEEB8(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 member = self + 8u;
    ctx.gpr[19] = member;
    ctx.gpr[17] = self;
    ctx.gpr[18] = 0x240u;
    ctx.gpr[16] = 0u;
    call(rt, ctx, 0x08B344E0u, {member});
    u32 offset = 0u;
    do {
        call(rt, ctx, 0x088EED18u, {word(rt, self + 4u) + offset});
        offset += 0x90u;
    } while (offset != 0x240u);
    tail_call(ctx, 0x08B3457Cu, {member});
}

// 0x088EF470 (a0 = this, a1 = slot 0..6, a2 = a value): the byte at +0x92 is set to 1; a slot below 7 and a value of 0 or
// more (clamped to 250) make the eight records at this + 0x24 + index * 8 take the value in the slot's field (the jump table
// at 0x08C8A104 orders them 1, 2, 3, 4, 0, 5, 6). v0 = 1 for a bad slot, 250 for a negative value, and the last table
// index * 8 = 0x40 after the loop (the delay slot of the loop's exit branch).
void func_088EF470(psprecomp::Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kSlotOffsets[] = {1u, 2u, 3u, 4u, 0u, 5u, 6u};
    const u32 self = ctx.gpr[4];
    const u32 slot = ctx.gpr[5];
    s32 value = static_cast<s32>(ctx.gpr[6]);
    rt.memory().store8(self + 0x92u, 1u);
    u32 result = 1u;
    if (slot < 7u) {
        result = 0xFAu;
        if (value >= 0) {
            if (value > 0xFAu) value = 0xFAu;
            for (u32 index = 0u; index < 8u; ++index) {
                rt.memory().store8(self + index * 8u + 0x24u + kSlotOffsets[slot], static_cast<u32>(value));
            }
            result = 8u << 3;
        }
    }
    ctx.gpr[2] = result;
}

// 0x089B17B8 (a0 = this): 0x089B17AC(this) then five members are reset (0x089B0044, 0x089AB008, 0x089B05F8, 0x089B08E4,
// 0x089AFF3C), each with a zero argument, and 0x089B0284(this, 0) finishes.

// 0x089BF568 (a0 = this, f12 = a scale): the member at +0x2B0 is locked (0x08B344E0), the scale is kept at +0x244 and in
// f20, the object at +0x264 runs its virtual at +0x6C with the scale in f12, then the member is released (0x08B3457C). f20
// comes back as the caller left it.

// --- lookups through a vtable that answer a float ----------------------------------------------------------------
// 0x089DA558 and 0x089DA768 (a0 = this, a1 = an index): the member at +0x8C may first correct the index through the
// virtual at vtable +0x1C8 (answer - index - 1), then the virtual at +0x1D0(this, index) names an object whose float at
// +0x14 (0x089DA558) or +0x18 (0x089DA768) is the answer, in f0.
void lookup_float(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const u32 self = ctx.gpr[4];
    u32 index = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = index;
    const u32 vtable = word(rt, self);
    const u32 second_slot = word(rt, vtable + 0x1D0u);
    ctx.gpr[18] = second_slot;
    if (rt.memory().load8(self + 0x8Cu) != 0u) {
        const u32 corrected = call_indirect(rt, ctx, word(rt, vtable + 0x1C8u), {self, index});
        index = corrected - index - 1u;
        ctx.gpr[17] = index;
    }
    const u32 object = call_indirect(rt, ctx, second_slot, {self, index});
    ctx.fpr[0] = float_at(rt, object + offset);
}
void func_089DA768(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { lookup_float(rt, ctx, 0x18u); }

// 0x08A2703C (a0 = this): the callback at +0x30, when present, runs with a0 = this and its answer (0 or a value) is kept;
// the member at +0x30 is locked (0x08B344E0), the byte answer goes to +0x2D, a nonzero answer arms the timer 0x000F4240 at
// +0x34, the byte +0x2C becomes 1 and 0x08B3457C(this) finishes.

// 0x08A562F0 and 0x08A563E8 (a0 = an answer object, a1 = holder): the holder's object at +0x14, or 0x08CAF798 when there is
// none, gets the virtual at vtable +0x128 (0x08A562F0) or +0x14C (0x08A563E8) with (answer, object, holder). v0 = a0.
void forward_virtual(Runtime &rt, AllegrexContext &ctx, u32 slot) {
    const u32 answer = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = answer;
    const u32 object = word(rt, holder + 0x14u);
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    ctx.gpr[17] = holder;
    call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {answer, object, holder});
    ctx.gpr[2] = answer;
}
void func_08A563E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_virtual(rt, ctx, 0x14Cu); }

// 0x08A60FB0 (a0 = an object): a smart pointer is filled from 0x08A60D90(sp) (null is an error, 0x08CB01D8), the object is
// looked up (0x08A60BF4) and, unless it is `self`, released (0x08A5BAD8(self, its word)); the temporary is released
// (0x08A5B9EC(sp)). v0 = that last release's result.

// 0x08AC0290 (a0 unused, a1 = holder): 0x08B0C880(holder) fails with v0 < 0; otherwise the table at word(holder + 8) has
// its callback at +0x130 run with its context at +0x134 (or -1 without one), the holder is released (0x08B0C71C) and v0 is
// the answer.
void func_08AC0290(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 holder = ctx.gpr[5];
    const u32 table = word(rt, holder + 8u);
    ctx.gpr[17] = holder;
    ctx.gpr[18] = table;
    const u32 first = call(rt, ctx, 0x08B0C880u, {holder});
    ctx.gpr[16] = first;
    u32 result = first;
    if (static_cast<s32>(first) >= 0) {
        const u32 callback = word(rt, table + 0x130u);
        result = 0xFFFFFFFFu;
        if (callback != 0u) result = call_indirect(rt, ctx, callback, {word(rt, table + 0x134u)});
        call(rt, ctx, 0x08B0C71Cu, {holder});
    }
    ctx.gpr[2] = result;
}

// 0x08AC03E0 (a0 = this, a1 = first, a2 = second, a3 = flag in its byte): with the flag the pair goes to 0x08B1554C(object,
// first, second, flag), without it a two-word record {first, second} goes to 0x08B0C650(object, record), object being the word
// at word(this + 8) + 0x54; the answer is kept at +0x1C and v0 = 1 when it is 0 or positive, else 0.
void func_08AC03E0(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    ctx.gpr[16] = self;
    const u32 object = word(rt, word(rt, self + 8u) + 0x54u);
    if (flag != 0u) {
        put(rt, self + 0x1Cu, call(rt, ctx, 0x08B1554Cu, {object, first, second, flag}));
    } else {
        put(rt, frame.at(0u), first);
        put(rt, frame.at(4u), second);
        put(rt, self + 0x1Cu, call(rt, ctx, 0x08B0C650u, {object, frame.at(0u)}));
    }
    ctx.gpr[2] = static_cast<s32>(word(rt, self + 0x1Cu)) >= 0 ? 1u : 0u;
}

// 0x08AEEE50 (a0 = this, a1 = a count, a2 = a value): 0x08AEEEF4(this, count, value) sets the scene, then the seven-argument
// 0x08AEED10(this, 1, count, value, 0, 0, 0) and 0x08AEEFD4(this, 1) finish. v0 = the last call's result.

// 0x08AF0910 (a0 = a value): every entry of the table at 0x08D41AD0 (count at +0x544) takes the value (0x08AF07C8(index,
// value)). v0 = the table address when there is nothing to do, else 0 (the last count test).

// 0x08AF4294 (a0 = an object, a1 = holder): the holder's object (word at +4, else the word at +8) runs its virtual at
// vtable + 8 with (self, object); without one, a record is built (0x08B4E648(sp)) and applied (0x08B4E734(self, record)).
// v0 = self.

// 0x08B0C650 (a0 = a name, a1 = a value): 0x08B0C43C(name) looks the object up; found, its third virtual (vtable + 0x84)
// runs with (object, name, value, 1) as a tail call; otherwise v0 = -1.
void func_08B0C650(psprecomp::Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    u32 found = 0u;
    u32 function = 0u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = value;
        found = call(rt, ctx, 0x08B0C43Cu, {self});
        if (found == 0u) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        function = word(rt, word(rt, found) + 0x84u);
    }
    tail_call(ctx, function, {found, self, value, 1u});
}

// 0x08B174D4: the table 0x08D6112C is prepared (0x08B1728C) and the two globals 0x08D62B40 and 0x08D62B3C are released
// (0x08B1B99C) and cleared when present. v0 ends as the last value loaded (0) or as the second release's result.

// 0x08B180F4 (a0 = this): the first virtual (vtable +0x1C) runs with 0, the record at this + 0x1860 + index * 8 (index at
// +0x1874) goes to 0x08B24D84(this, first, second), the index flips between 0 and 1, and the second virtual (vtable +0x18)
// finishes as a tail call.

// 0x08B319FC: an import status (0x08C7AEF0(sp)) maps to 0 (anything but 0x80410B08 on failure, a zero word), 3 (0x80410B08)
// or from the word it filled: 1, 2, 4 for other nonzero values.

// 0x08B6594C (a0 = an answer, a1 = an object, a2 = a value): the member at object + 0x6C is set up (0x08B5C450(a0, member)),
// takes the value (0x08B5D1E0(member, value)), the object is flagged (0x08B6534C(object, 1)). v0 = a0.

// 0x08B7B2E4 (a0 = a record): the callee-saved integer registers s0-s7, sp, fp and ra and the floats f20-f31 are stored in
// the record (0xA to 0x58) and v0 = 0.
void func_08B7B2E4(psprecomp::Runtime &rt, AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    for (unsigned index = 0u; index < 8u; ++index) put(rt, record + index * 4u, ctx.gpr[16u + index]);
    put(rt, record + 0x20u, ctx.gpr[29]);
    put(rt, record + 0x24u, ctx.gpr[30]);
    put(rt, record + 0x28u, ctx.gpr[31]);
    for (unsigned index = 0u; index < 12u; ++index) put_float(rt, record + 0x2Cu + index * 4u, ctx.fpr[20u + index]);
    ctx.gpr[2] = 0u;
}

// 0x08B85860 (a0 = this, a1 = holder): the holder's object (its word) must exist (0x08C97BA4); its virtual at vtable +0x5C
// fills a record on the stack with (sp, object), the member at this + 0x14 takes it (0x08A6A23C) and the record is released
// (0x08A69D68). v0 = that last release's result.
void func_08B85860(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 member = ctx.gpr[4] + 0x14u;
    ctx.gpr[17] = member;
    const u32 object = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97BA4u});
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x5Cu), {frame.at(0u), object});
    call(rt, ctx, 0x08A6A23Cu, {member, frame.at(0u)});
    call(rt, ctx, 0x08A69D68u, {frame.at(0u)});
}

// --- functions that pass a five-byte record to one of two variants of a call ------------------------------------
// 0x08C66D44 and 0x08C6843C (a0 = this, a1 = a five-byte record, a2 = ..., a3 = a signed byte, t0 = the fifth argument
// moved to t2, t1 = moved to t3): the record {word at a1, byte at a1 + 4} is copied to the stack and handed to the callee
// as (this, copy, a2, sext(a3), fifth, t1, t0, t1) — fifth is 0x4C for 0x08C66D44 and 0 for 0x08C6843C. Afterwards the
// callee's v1 goes to sp + 0x14, its low byte to sp + 4, and v0/v1 come back as the result and the word at sp + 4 (which
// reloads the low byte just written over the three bytes already there).
void struct_copy_call(Runtime &rt, AllegrexContext &ctx, u32 callee, u32 fifth) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 record = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = sign_extended_byte(ctx.gpr[7]);
    const u32 sixth = ctx.gpr[9];
    ctx.gpr[16] = self;
    put(rt, frame.at(8u), word(rt, record));
    rt.memory().store8(frame.at(0xCu), rt.memory().load8(record + 4u));
    const GuestResult result =
        call_guest(rt, ctx, callee, {self, frame.at(8u), third, fourth, fifth, sixth, ctx.gpr[8], ctx.gpr[9]}, {});
    ctx.gpr[2] = result.v0;
    put(rt, frame.at(0x14u), result.v1);
    rt.memory().store8(frame.at(4u), rt.memory().load8(frame.at(0x14u)));
    const u32 low_word = word(rt, frame.at(4u));
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = low_word;
}

// --- a pointer with a new object behind it, or the answer of a construction call ---------------------------------
// 0x088152F8, 0x0897E8DC, 0x08989764 and 0x08989824 (a0 unused, a1 = a holder, a2 = first, a3 = second): the construction
// `construct(holder)` writes a new object to the stack (null is an error: the string `text`); then either the word at
// object + `member` (088152F8: its owned member at +0xE8) or the object's sub-object at `member` takes (first, second)
// through `use`, and the record is released (0x08A5B9EC(sp)). v0 = that last release's result.
void construct_through_holder(Runtime &rt, AllegrexContext &ctx, u32 construct, u32 text, u32 use, u32 member,
                              bool dereference) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    ctx.gpr[18] = second;
    ctx.gpr[17] = first;
    call(rt, ctx, construct, {frame.at(0u), ctx.gpr[5]});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {text});
    if (dereference) {
        call(rt, ctx, use, {word(rt, object + member), first, second});
    } else {
        call(rt, ctx, use, {object + member, first, second});
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- small actions on a member that is locked, used and released ------------------------------------------------
// 0x0885BDA8 (a0 = this): the member at +8 is locked (0x08B344E0); with the byte at +0x19 set the fifth argument is the word
// at +0x88, otherwise 0; 0x088619A8(word(this + 4), word(vtable + 0x7C), word(vtable + 0x80), word(vtable + 0x84), fifth)
// runs, the member is released (0x08B3457C) and its result is v0.

// 0x0885D010 (a0 = this): the member at +8 is locked (0x08B344E0); the byte at vtable +0x17 picks 0x0885BF5C(this) or
// 0x0885CF00(this), the member is released (0x08B3457C) and the pick's result is v0.

// 0x08881C38 (a0 = this, a1 = a value): the word at word(this + 0xC) + 0x16F0 must be 0 or more; then 0x0886B080(word(this
// + 8)) names an object whose virtual at word(object) + 0x30 finishes with (object, that word, a1) as a tail call.
void func_08881C38(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    ctx.gpr[17] = value;
    ctx.gpr[16] = self;
    const u32 status = word(rt, word(rt, self + 0xCu) + 0x16F0u);
    if (static_cast<s32>(status) < 0) {
        ctx.gpr[2] = status;
        return;
    }
    const u32 object = call(rt, ctx, 0x0886B080u, {word(rt, self + 8u)});
    const u32 slot = word(rt, word(rt, self + 0xCu) + 0x16F0u);
    tail_call(ctx, word(rt, word(rt, object) + 0x30u), {object, slot, value});
}

// 0x0888B5CC (a0 = this, a1 = a byte, a2 = a value): when the word at +0x1C is 1 the member at +0x14 takes the byte
// (0x08891EF8); then a three-word record {0xB, byte at +8, a2 at +0xC} goes to 0x088936A4(word(this + 0x18), record) and its
// result is v0.
void func_0888B5CC(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 byte = ctx.gpr[5] & 0xFFu;
    const u32 value = ctx.gpr[6];
    ctx.gpr[18] = value;
    ctx.gpr[17] = byte;
    ctx.gpr[16] = self;
    if (word(rt, self + 0x1Cu) == 1u) call(rt, ctx, 0x08891EF8u, {word(rt, self + 0x14u), byte});
    put(rt, frame.at(0u), 0xBu);
    rt.memory().store8(frame.at(8u), byte);
    put(rt, frame.at(0xCu), value);
    call(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u), value});
}

// 0x08890C5C (a0 = this): 0x08890AC0(this) and 0x0888F548(this) run, the word at answer + 4 (byte-reversed) selects: when it
// is zero the word at answer + 8 (reversed) plus word(this) is the value, otherwise the word at answer + 8; the result comes
// back byte-reversed again, in v0.

// 0x088DC718 (a0 = this): 0x088E4AA8(word(this + 0x640)) decides: the halfword at word(this + 0x130) + 0x10 goes to its
// +0xA, or its +0xA is cleared; then word(this + 0x640), when present, takes (0xB, 1, 0) as a tail call (0x088E4360).
void func_088DC718(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    if (call(rt, ctx, 0x088E4AA8u, {word(rt, self + 0x640u)}) != 0u) {
        const u32 object = word(rt, self + 0x130u);
        rt.memory().store16(object + 0xAu, rt.memory().load16(object + 0x10u));
    } else {
        rt.memory().store16(word(rt, self + 0x130u) + 0xAu, 0u);
    }
    const u32 object = word(rt, self + 0x640u);
    if (object == 0u) {
        ctx.gpr[2] = object;
        return;
    }
    tail_call(ctx, 0x088E4360u, {object, 0xBu, 1u, 0u});
}

// 0x088E0B78 and 0x088E0BE4 (a0 = this, a1 = a value, a2 passed on): when the words at +0x5D0 and +0x5D4 are not both set,
// the member at +0x5E8 is locked and released (0x08B344E0, 0x08B3457C) and the value re-read; then the word at +0x5D4 takes
// the value through 0x088F9E38 (0x088E0B78) or 0x088F9C80 (0x088E0BE4) as a tail call.
void func_088E0BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 member = self + 0x5E8u;
    ctx.gpr[18] = value;
    ctx.gpr[17] = member;
    ctx.gpr[16] = self;
    u32 target = word(rt, self + 0x5D4u);
    if (word(rt, self + 0x5D0u) == 0u || target == 0u) {
        call(rt, ctx, 0x08B344E0u, {member});
        call(rt, ctx, 0x08B3457Cu, {member});
        target = word(rt, self + 0x5D4u);
    }
    tail_call(ctx, 0x088F9C80u, {target, value, ctx.gpr[6]});
}

// 0x088E66C0 (a0 = this): the object word(this) has its member at +0xEC locked (0x08B344E0), its virtual at vtable +0x50 runs
// with -1 and a found entry gets the byte +7 set to 1, the member is released (0x08B3457C) and 0x088E511C(this) answers.
// v0 = that answer with its low bit flipped (`xori 1`).

// 0x088EABC8 (a0 = this, a1 = a byte): the member at +0x538 answers its virtual at vtable +0xA0; when that is nonzero, or its
// virtual at +0x38 with word(this + 0xA8) is, the word at +0x560 is cleared and v0 = 2, otherwise v0 = 0.

// 0x088EC13C (a0 = this): the first virtual (vtable +0xA4) runs; the member at +0x538 then answers its virtual at +0x44 and,
// when that is nonzero, the word at +0x56C is cleared and v0 = 5, otherwise the first virtual (vtable +0x10) runs again and
// v0 = 0.

// 0x089176EC (a0 = a three-float destination, a1 = holder): the holder's object at +4 fills three floats on the stack through
// its virtual at vtable +0x58 (with (object, sp)); copied to the destination (or the destination is zeroed). v0 = the
// destination.

// 0x0892EE54: 0x08936DBC and 0x089363FC set the table up and 0x089378D4(answer, word(0x08D69524), this) runs; then the two
// register arrays at 0x08CF296C and 0x08CF297C (2 words each) are cleared. v0 ends as the loop's upper bound 0x08CF2984.

// 0x089C66F4 (a0 = this, a1 = first, a2 = second): 0x089C6610(first, second, sp, sp + 4) fills two stack words; when it
// answers 0, 0x08A44960(this, 0x08CF6ED8) runs, otherwise 0x08A44D9C(this, answer, word(sp + 4), byte(sp)). v0 = this.

// 0x089C6AB4 (a0 = this): the vtable 0x08CCD338 is stored and the member at +0x240 released through its first virtual (the
// guard skips the empty range); 0x08B22AF0(this + 0x208) and 0x08978B04(this) run, then 0x08A5BC54(this, 0x4D0, 4) finishes.

// 0x08A9C70C (a0 = this, a1 = a key): the member at +0x4C is locked (0x08B344E0), the lookup 0x08C2FA50(this + 0xC, key)
// answers; unless it is this + 0x10, its word at +0x14 is the answer (0 otherwise), and the member is released
// (0x08B3457C). v0 = that answer.

// 0x08AA5E24 (a0 = an object, a1 unused, a2 = a low word, a3 = a high word): eight bytes are built on the stack — byte i is
// ((a3 << 1) << ((~8i) & 31)) | (a2 >> 8i), or a3 >> 8i when bit 0x20 of 8i is set (never for i < 8) — and handed to
// 0x08AA5D2C(object, bytes, 8). v0 = that call's result.
void func_08AA5E24(psprecomp::Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 object = ctx.gpr[4];
    const u32 low = ctx.gpr[6];
    const u32 high = ctx.gpr[7];
    const u32 doubled = high << 1;
    for (u32 index = 0u; index < 8u; ++index) {
        const u32 shift = index * 8u;
        const u32 left = doubled << ((~shift) & 31u);
        u32 value = left | (low >> (shift & 31u));
        if ((shift & 0x20u) != 0u) value = high >> (shift & 31u);
        rt.memory().store8(frame.at(0u) + index, value);
    }
    call(rt, ctx, 0x08AA5D2Cu, {object, frame.at(0u), 8u});
}

// 0x08AC45F4: 0x08C1A1B8(0x08CFFD10) gives a count, count * 0x9A00 bytes are obtained (0x08B2B4D8(0x80, size)) and filled
// (0x08B2B760(0x08CFFCF4, buffer, size, 0x9A00)); 0x08B2EEB8(0x08CFFCB8) then 0x08C7ACD0(0x08CFFCB8) finish.

} // namespace
} // namespace new_batch_14
} // namespace gt::game

#define NEW14(address, name, kind) GT_DECOMP(address, gt::game::new_batch_14::name, gt::ReturnKind::kind)
NEW14(0x08B0C6B8u, func_08B0C6B8, Word);
NEW14(0x08C3D0C8u, func_08C3D0C8, Word);
NEW14(0x08C76340u, func_08C76340, Word);
NEW14(0x088EEEB8u, func_088EEEB8, Word);
NEW14(0x088EF470u, func_088EF470, Word);
NEW14(0x089DA768u, func_089DA768, Float);
NEW14(0x08A563E8u, func_08A563E8, Word);
NEW14(0x08AC0290u, func_08AC0290, Word);
NEW14(0x08AC03E0u, func_08AC03E0, Word);
NEW14(0x08B0C650u, func_08B0C650, Word);
NEW14(0x08B7B2E4u, func_08B7B2E4, Word);
NEW14(0x08B85860u, func_08B85860, Word);
NEW14(0x08C4130Cu, func_08C4130C, Word);
NEW14(0x08C41374u, func_08C41374, Word);
NEW14(0x08C415A4u, func_08C415A4, Word);
NEW14(0x08C4160Cu, func_08C4160C, Word);
NEW14(0x08C41674u, func_08C41674, Word);
NEW14(0x08C41934u, func_08C41934, Word);
NEW14(0x08C41A04u, func_08C41A04, Word);
NEW14(0x0880A048u, func_0880A048, Word);
NEW14(0x0880BDC8u, func_0880BDC8, Word);
NEW14(0x08881C38u, func_08881C38, Word);
NEW14(0x0888B5CCu, func_0888B5CC, Word);
NEW14(0x088DC718u, func_088DC718, Word);
NEW14(0x088E0BE4u, func_088E0BE4, Word);
NEW14(0x08AA5E24u, func_08AA5E24, Word);
