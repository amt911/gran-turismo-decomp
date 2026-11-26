// Main-session batch 1020 (draft): functions the earlier drafts skipped or mismatched, read from tools.disasm.
// Written mostly as relief work on the reference-counted member swaps: a shared object keeps its count at +4, its
// weak count at +8 and a vtable whose slots 8 and 0xC are the destructors. The count steps are `ll/sc` sequences
// (single-threaded here, the host never preempts inside one), entered with v1 = the word's address, a0 = the old
// value and v0 = 1.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1020 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the `jal` (a0-a3, t0-t3 in order):
// what the callee does not take keeps the caller's value, and v0 / v1 / a1 / f0 afterwards are the callee's, as after
// the original's jump and link.
GuestResult call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}

// `lw v1,0(object); lw v0,slot(v1); jalr v0` with a0 = object: v1 is the table at the call.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + slot);
    ctx.gpr[3] = table;
    return call_indirect(rt, ctx, function, arguments);
}

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// The `ll/sc` increment of a shared block's count: v1 = the word's address, v0 = 1 (the successful `sc`).
void incref(Runtime &rt, AllegrexContext &ctx, u32 block) {
    const u32 count = word(rt, block + 4u);
    ctx.gpr[3] = block + 4u;
    ctx.gpr[2] = 1u;
    put(rt, block + 4u, count + 1u);
}

// The `ll/sc` decrement: a0 = the old value, v1 = the word's address, v0 = 1; true when the count reached zero.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// The whole release of a shared block: count +4 down (not 1: done), virtual slot 8; count +8 down (not 1: done),
// virtual slot 0xC. A null block does nothing.
void release_block(Runtime &rt, AllegrexContext &ctx, u32 block) {
    if (block == 0u) return;
    if (!count_down(rt, ctx, block + 4u)) return;
    (void)call_virtual(rt, ctx, block, 8u, {block});
    if (!count_down(rt, ctx, block + 8u)) return;
    (void)call_virtual(rt, ctx, block, 0xCu, {block});
}

// An intrusive pointer assignment on the holder word at `slot`: the new block counted up first, the old one released,
// then the store (which the original puts in the delay slot of the branch that skips the destructor).
void assign_block(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 value) {
    if (word(rt, slot) == value) return;
    if (value != 0u) incref(rt, ctx, value);
    const u32 previous = word(rt, slot);
    if (previous != 0u) release_block(rt, ctx, previous);
    put(rt, slot, value);
}

// func_088D9178(object = a0, flag = a1): when the object's deadline word pair (+0x708 low, +0x70C high) is zero, the
// byte at +0x10 of the four 0x2F8-byte cells hanging off +0xC0 is cleared, the deadline becomes func_088D9C24(+0x90,
// +0x94) — that helper answers a2 + 5 with the carry added to a3 — and a zero flag returns 0 right away. The current
// time (+0x90 / +0x94) is then compared with the deadline (signed high word first): behind it the function either
// waits (fresh deadline) or answers 0 (deadline that was already there). The wait polls func_08B1A844 (its result
// never read as an argument) and yields through func_08B2E2B8(0x2710) until the deadline is reached.
// The collection: the three words at +0x65C, +0x660, +0x664 are cleared and given to func_08B8FE44, the emptied list
// head func_08B90024(object + 0x65C, stack + 0x10) is drained into the seven-word temporary at stack + 0x10, whose
// words +0x1C / +0x20 / +0x24 / +0x28 are then swapped into the object: +0x668 (plain store), the shared pointer at
// +0x66C, +0x670 (plain store) and the shared pointer at +0x674. The temporary's two blocks are released, the list at
// stack + 0x10 is given back to func_08B8FE44 / func_08B35254, the deadline pair is cleared and v0 = 1.
// The path where the list head is the stack temporary itself (object + 0x65C == stack + 0x10) skips the collection.
void func_088D9178(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    const u32 argument1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    u32 deadline_lo = word(rt, object + 0x708u);
    u32 deadline_hi = word(rt, object + 0x70Cu);
    bool wait = false;
    if ((deadline_lo | deadline_hi) == 0u) {
        for (u32 offset = 0u;;) {
            const u32 cell = word(rt, object + 0xC0u) + offset;
            offset += 0x2F8u;
            put8(rt, cell + 0x10u, 0u);
            if (offset == 0xBE0u) break;
        }
        const GuestResult deadline = call(rt, ctx, 0x088D9C24u, {object, argument1, word(rt, object + 0x90u), word(rt, object + 0x94u)});
        deadline_lo = deadline.v0;
        deadline_hi = deadline.v1;
        put(rt, object + 0x708u, deadline_lo);
        put(rt, object + 0x70Cu, deadline_hi);
        if (flag == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 current_hi = word(rt, object + 0x94u);
        if (static_cast<s32>(current_hi) < static_cast<s32>(deadline_hi)) {
            wait = true;
        } else if (current_hi == deadline_hi && word(rt, object + 0x90u) < deadline_lo) {
            wait = true;
        }
    } else {
        const u32 current_hi = word(rt, object + 0x94u);
        if (static_cast<s32>(current_hi) < static_cast<s32>(deadline_hi)) {
            ctx.gpr[3] = deadline_hi;
            ctx.gpr[2] = 0u;
            return;
        }
        if (current_hi == deadline_hi && word(rt, object + 0x90u) < deadline_lo) {
            ctx.gpr[3] = deadline_hi;
            ctx.gpr[2] = 0u;
            return;
        }
    }
    if (wait) {
        for (;;) {
            const u32 ready = call(rt, ctx, 0x08B1A844u, {word(rt, object + 0x90u), word(rt, object + 0x94u), word(rt, object + 0x708u),
                                                          word(rt, object + 0x70Cu)})
                                  .v0;
            if (ready != 0u) break;
            (void)call(rt, ctx, 0x08B2E2B8u, {0x2710u});
            deadline_hi = word(rt, object + 0x70Cu);
            const u32 current_hi = word(rt, object + 0x94u);
            deadline_lo = word(rt, object + 0x708u);
            const u32 current_lo = word(rt, object + 0x90u);
            if (static_cast<s32>(current_hi) < static_cast<s32>(deadline_hi)) continue;
            if (current_hi != deadline_hi) break;
            if (current_lo < deadline_lo) continue;
            break;
        }
    }
    const u32 head = object + 0x65Cu;
    for (u32 offset = 0x10u; offset <= 0x28u; offset += 4u) put(rt, frame.at(offset), 0u);
    u32 temporary_block = 0u;
    if (head != frame.at(0x10u)) {
        const u32 first = word(rt, object + 0x65Cu);
        const u32 second = word(rt, object + 0x660u);
        const u32 third = word(rt, object + 0x664u);
        put(rt, object + 0x65Cu, 0u);
        put(rt, object + 0x660u, 0u);
        put(rt, object + 0x664u, 0u);
        put(rt, frame.at(0xCu), third);
        put(rt, frame.at(4u), first);
        put(rt, frame.at(8u), second);
        (void)call(rt, ctx, 0x08B8FE44u, {first, second, frame.at(1u), third});
        const u32 previous = word(rt, frame.at(4u));
        if (previous != 0u) (void)call(rt, ctx, 0x08B35254u, {previous});
        (void)call(rt, ctx, 0x08B90024u, {head, frame.at(0x10u)});
        put(rt, object + 0x668u, word(rt, frame.at(0x1Cu)));
        assign_block(rt, ctx, object + 0x66Cu, word(rt, frame.at(0x20u)));
        put(rt, object + 0x668u + 8u, word(rt, frame.at(0x24u)));
        assign_block(rt, ctx, object + 0x66Cu + 8u, word(rt, frame.at(0x28u)));
        temporary_block = word(rt, frame.at(0x28u));
    }
    release_block(rt, ctx, temporary_block);
    release_block(rt, ctx, word(rt, frame.at(0x20u)));
    (void)call(rt, ctx, 0x08B8FE44u, {word(rt, frame.at(0x10u)), word(rt, frame.at(0x14u)), frame.at(0u)});
    const u32 temporary = word(rt, frame.at(0x10u));
    if (temporary != 0u) (void)call(rt, ctx, 0x08B35254u, {temporary});
    put(rt, object + 0x708u, 0u);
    put(rt, object + 0x70Cu, 0u);
    ctx.gpr[2] = 1u;
}

// func_088FCCCC(object = a0, first = a1, second = a2): builds a 0x548-byte object with func_088FCA30(new, a1, a2, 1)
// (vtable 0x08CC4300), gives it a 0x10-byte counter block (vtable 0x08CC4370, counts 1 / 1, owner at +0xC) and installs
// both at object + 0x28 (object word / +4 counter), releasing the counter it replaces. The member-function adaptor
// func_08BBC08C fills the stack pair at +8 whose result (function +0xC / adjustment +0x10) is then called on the new
// object: a plain pointer when the adjustment's bit 0 is clear, else vtable slot `function` of object + (adjustment >> 1).
// The counter is counted up twice for that call and released twice afterwards.
// When the object word at +0x2C is set, the call is queued instead: a 0x14-byte node (vtable 0x08CC4388, object +4,
// counter +8, function +0xC, adjustment +0x10) is pushed onto the queue at object word +0x34 (the previous head called
// through virtual slot 0 when one is there), the queue's second word becomes the lazily built static at 0x08D76448
// (guard byte 0x08D761B0, vtable 0x08CC43A0) and the extra count taken for the node is released. The object's lock word
// (new object +4) is held around the call with func_08B34888 / func_08B34B24.

// The one-time state of the ordered container at 0x08D75F5C: 0x08D75F50 is its guard byte, func_08B6441C the guard
// acquisition (zero: another thread has it, do nothing) and func_08B64494 the release; 0x088B276C is registered as the
// teardown. The seven words +0xC .. +0x24 become the empty node 0x08D75F68 and the counts 0.
void init_container(Runtime &rt, AllegrexContext &ctx) {
    if (byte_at(rt, 0x08D75F50u) != 0u) return;
    if (call(rt, ctx, 0x08B6441Cu, {0x08D75F50u}).v0 == 0u) return;
    put(rt, 0x08D75F5Cu + 0x18u, 0x1001u);
    put(rt, 0x08D75F5Cu + 0xCu, 0x08D75F68u);
    put(rt, 0x08D75F5Cu + 0x10u, 0u);
    put(rt, 0x08D75F5Cu + 4u, 0u);
    put(rt, 0x08D75F5Cu + 8u, 0u);
    put(rt, 0x08D75F5Cu + 0x14u, 0x08D75F68u);
    put(rt, 0x08D75F5Cu + 0x1Cu, 0u);
    put(rt, 0x08D75F5Cu + 0x20u, 0u);
    put(rt, 0x08D75F5Cu + 0x24u, 0u);
    put(rt, 0x08D75F5Cu, 0u);
    (void)call(rt, ctx, 0x08B64494u, {0x08D75F50u});
    (void)call(rt, ctx, 0x08B77B1Cu, {0x088B276Cu});
}

// func_088B3428(object = a0, argument1 = a1, argument2 = a2): the ordered container's insertion, copied instruction for
// instruction (the tree descent is pointer arithmetic on the 8-byte nodes, so it keeps the original's adds and compares
// rather than a hand-made model of the structure). object[0] = 0x08CC30D0 and object[4] = 0 are written first, then the
// container state is built once. The descent starts at node 0x08D75F5C + 0x14 and walks word(node) while the node's
// +4 word is 2 or more (2: the node is a leaf entry and is linked out), entering a fresh leaf at +4 = 2, counting the
// moved entries at container +0x1C / +0x20 / +0x24. At a node whose +4 word is below 2 (the tree slot), the slot's
// capacity word +0x18 doubles by half, func_08B2B4D8(0x10, capacity * 8 or 0x10) allocates the new entry table, the
// table is pushed in front of container +4 (or grown by func_08BB13D4 when full), the allocated entry is linked into
// both directions and the container head +0x14 is updated; the second walk then finds the entry to link the fresh
// leaf after and fixes the two neighbourhood links around it. func_08B62344(0x338) allocates the result, stored at
// object[4], and func_088B301C(result, a1, a2) fills it (its v0 is the function's v0).

// The six characters the parser skips before a token: space, form feed, line feed, carriage return, tab, vertical tab.
bool is_space(s32 c) { return c == 0x20 || c == 0xC || c == 0xA || c == 0xD || c == 9 || c == 0xB; }

// One character appended to the COW string whose data pointer sits at `slot` (its header is data - 0xC: length at +0,
// capacity at +4, a word at +8 that also asks for a grow when positive): func_08B864EC(slot, length + 1) grows it, then
// the character, the cleared word at data - 4, the new length and the terminator are written. Answers the new length.
u32 append_char(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 character) {
    u32 data = word(rt, slot);
    u32 header = data - 0xCu;
    u32 length = word(rt, header);
    const u32 new_length = length + 1u;
    if (word(rt, header + 4u) < new_length || static_cast<s32>(word(rt, header + 8u)) > 0) {
        (void)call(rt, ctx, 0x08B864ECu, {slot, new_length});
        data = word(rt, slot);
        length = word(rt, data - 0xCu);
    }
    put8(rt, data + length, character);
    const u32 current = word(rt, slot);
    put(rt, current - 4u, 0u);
    put(rt, current - 0xCu, new_length);
    put8(rt, current + new_length, 0u);
    return new_length;
}

// The release of the COW string at `slot`: nothing when its header is the shared empty one (0x08D74FD0), otherwise
// func_08B60804(data - 4, -1) and, when its answer is not positive, func_08B8542C(header, argument).
void release_temp(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 argument) {
    const u32 data = word(rt, slot);
    const u32 header = data - 0xCu;
    if (header == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}).v0) > 0) return;
    (void)call(rt, ctx, 0x08B8542Cu, {header, argument});
}

// func_089861BC(object = a0, text = a1): the `#<number> "<text>"` directive at word(a1). The whitespace test is the six
// characters above, repeated three times (before the '#', between the '#' and the number, between the number and the
// quote). The number's characters are appended to a stack COW string (empty header 0x08D74FD0, data 0x08D74FDC), then
// func_08B77BFC(that string) turns it into a number stored at object + 0xC and the string is released. A quoted text
// after it is compared with the constant at 0x08CA3958 through func_08B85EB8 / func_08B36170: when the comparison is
// zero, the string is also stored at object + 4; either way it is stored at object + 8 with func_08B855E0. v0 = 1 when
// the quoted text was read, 0 on every path that has to release the temporary and give up.

} // namespace

#define NEW1020(address, name) GT_DECOMP(address, gt::game::new_batch_main_1020::name, gt::ReturnKind::Word)
NEW1020(0x088D9178u, func_088D9178);

} // namespace new_batch_main_1020
} // namespace gt::game
