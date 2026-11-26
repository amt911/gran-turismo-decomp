// Main-session batch 21: emitted functions of 124-133 instructions, written by hand as structured C from
// `tools.disasm` (drafts — Claude runs the gate). No name has evidence, so they keep the recompiler's
// func_XXXXXXXX names. Every exit path leaves v0 as the asm does: a call's result where the asm ends with a
// call, the loaded word where it loads one before returning, and 0 on an early return through a beqz of a 0.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_21 {
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
u32 callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
          std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (or whatever the caller
// left in v0), and a callee that never writes v0 returns that.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 v0, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = v0;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 unsigned_half(Runtime &rt, u32 a) { return rt.memory().load16(a); }
void put8(Runtime &rt, u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); }
void put16(Runtime &rt, u32 a, u32 v) { rt.memory().store16(a, static_cast<std::uint16_t>(v)); }
// The compiler's word-at-a-time struct copy loops (4 bytes each, `bytes` a multiple of 4).
void copy_words(Runtime &rt, u32 dst, u32 src, u32 bytes) {
    for (u32 i = 0; i < bytes; i += 4u) put(rt, dst + i, word(rt, src + i));
}
// Arithmetic shift right, as `sra`.
s32 sar(s32 value, unsigned bits) { return static_cast<s32>(value >> bits); }
// `mult` + `mflo`: the low 32 bits of the signed product.
u32 mult_low(s32 a, s32 b) { return static_cast<u32>(static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b)); }

// The callee-saved registers (s0-s7, fp) the original saves in its prologue and restores before returning.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 saved[9];
    explicit SavedRegs(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 8u; ++i) saved[i] = c.gpr[16u + i];
        saved[8] = c.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
        ctx.gpr[30] = saved[8];
    }
};

// --- 0x089A71C8 ---
// a0 = object, a1 = a context. Takes the lock 0x08CF4994, then for six fields of a0 (0x20, 0x24, 0x28, 0x2C, 0x30,
// 0x34) builds a temporary string from a name constant, formats it over the field and destroys it; the two
// pointer fields 0x20/0x24 are released through their vtable (+0x1B8). 0x089A717C may answer null: then nothing
// happens at all (v0 = 0). v0 = 0x08B3457C's result (the unlock) on the main path.
void log_field(Runtime &rt, AllegrexContext &ctx, u32 writer, u32 object, u32 offset, u32 name, u32 temp) {
    call(rt, ctx, 0x08A69B3Cu, {temp, name});
    call(rt, ctx, 0x08A7BFD0u, {writer, object + offset, temp});
    call(rt, ctx, 0x08A69D68u, {temp});
}
void release_member(Runtime &rt, AllegrexContext &ctx, u32 member) {
    const u32 vtable = word(rt, member);
    call_virtual(rt, ctx, vtable, word(rt, vtable + 0x1B8u), {member});
}
void func_089A71C8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const u32 object = ctx.gpr[4];
    const u32 writer = call(rt, ctx, 0x089A717Cu, {object, ctx.gpr[5]});
    if (writer == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 vtable = word(rt, writer);
    call_virtual(rt, ctx, vtable, word(rt, vtable + 0x160u), {writer});
    call(rt, ctx, 0x08B344E0u, {0x08CF4994u});

    log_field(rt, ctx, writer, object, 0x20u, 0x08CA4F9Cu, frame.at(0x14u));
    const u32 first = word(rt, object + 0x20u);
    if (first != 0u) release_member(rt, ctx, first);

    log_field(rt, ctx, writer, object, 0x24u, 0x08CA4FACu, frame.at(0x10u));
    if (word(rt, object + 0x24u) != 0u) {
        const u32 second = word(rt, object + 0x20u);
        if (second == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA4FA4u});
        release_member(rt, ctx, second);
    }

    log_field(rt, ctx, writer, object, 0x28u, 0x08CA4FB4u, frame.at(0x0Cu));
    log_field(rt, ctx, writer, object, 0x2Cu, 0x08CA4FC4u, frame.at(0x08u));
    log_field(rt, ctx, writer, object, 0x30u, 0x08CA4FD4u, frame.at(0x04u));
    log_field(rt, ctx, writer, object, 0x34u, 0x08CA4FE4u, frame.at(0x00u));
    ctx.gpr[2] = call(rt, ctx, 0x08B3457Cu, {0x08CF4994u});
}

// --- 0x08B01B2C ---
// a0 = container (words +0x60 begin, +0x64 end, +0x68 capacity; a table pointer at +0x74, a selector half at
// +0x6C). Builds the 0x54-byte element template at sp+0x50 out of a1..a3, t0, t1 and appends it (growing through
// 0x08C47924 when end == capacity) — or, with a selector >= 0, copies it over element `selector` of the table.
// v0 = the element's address.
void func_08B01B2C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xD0u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 begin = self + 0x60u;
    const u32 field = self + 0x6Cu;
    const u32 element = frame.at(0x50u);
    const s32 selector = signed_half(rt, self + 0x6Cu);

    put16(rt, frame.at(0xA2u), 0xFFFFFFFFu);
    put8(rt, frame.at(0x9Cu), ctx.gpr[6]);
    put8(rt, frame.at(0x9Du), ctx.gpr[7]);
    put8(rt, frame.at(0x9Eu), ctx.gpr[8]);
    put8(rt, frame.at(0x9Fu), ctx.gpr[9]);
    put8(rt, frame.at(0xA0u), ctx.gpr[5]);
    put(rt, frame.at(0x98u), 0u);
    put(rt, frame.at(0x94u), 0u);
    put(rt, frame.at(0x90u), 0u);
    put(rt, frame.at(0x8Cu), 0u);
    put(rt, frame.at(0x88u), 0u);
    put(rt, frame.at(0x84u), 0u);

    u32 result;
    if (selector == -1) {
        u32 end = word(rt, begin + 0x4u);
        if (end == word(rt, begin + 0x8u)) {
            call(rt, ctx, 0x08C47924u, {begin, end, element});
            result = word(rt, begin + 0x4u) - 0x54u;
        } else {
            if (end != 0u) copy_words(rt, end, element, 0x54u);
            end = word(rt, begin + 0x4u) + 0x54u;
            put(rt, begin + 0x4u, end);
            result = end - 0x54u;
        }
    } else {
        u32 slot = 0u;
        if (selector >= 0) slot = word(rt, word(rt, self + 0x74u)) + 84u * static_cast<u32>(selector);
        const u32 table = word(rt, field + 0x8u);
        put(rt, frame.at(0x4Cu), 0u);
        put(rt, frame.at(0x00u), word(rt, frame.at(0x40u)));
        put(rt, frame.at(0x48u), slot);
        put(rt, frame.at(0x44u), table);
        put(rt, frame.at(0x04u), table);
        put(rt, frame.at(0x08u), slot);
        u32 entry = 0u;
        if (selector >= 0) entry = word(rt, table) + 84u * static_cast<u32>(selector);
        put(rt, frame.at(0x0Cu), 0u);
        put(rt, frame.at(0x10u), word(rt, frame.at(0x30u)));
        put(rt, frame.at(0x14u), table);
        put(rt, frame.at(0x18u), entry);
        put(rt, frame.at(0x38u), entry);
        put(rt, frame.at(0x34u), table);
        put(rt, frame.at(0x3Cu), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        call(rt, ctx, 0x08C46CB0u, {frame.at(0xB0u), field, frame.at(0x10u)});
        put(rt, frame.at(0x20u), word(rt, frame.at(0xB0u)));
        put(rt, frame.at(0x24u), word(rt, frame.at(0xB4u)));
        put(rt, frame.at(0x28u), word(rt, frame.at(0xB8u)));
        put(rt, frame.at(0x2Cu), word(rt, frame.at(0xBCu)));
        copy_words(rt, slot, element, 0x54u);
        result = slot;
    }
    ctx.gpr[2] = result;
}

// --- 0x08B01D1C ---
// a0 = a pointer to the container, a1 = where the result goes, a2..a3, t0..t3 and four stack words (sp+0x40,
// 0x44, 0x48, 0x4C) complete the element description. Looks the element up or inserts it, then stamps the new
// element (its index in the half at +4 of the container's base, the three structs' first three words at
// +0x00/+0x0C and +0x18/+0x20). v0 = the element, or 0 when it could not be made.

// --- 0x08B987B8, 0x08B99ED0, 0x08BAFF8C ---
// A destructor shape (a0 = object): stores the vtable at *object and releases four reference-counted members
// (0x28, 0x20, 0x14, 0x0C, in that order). A member's count at +4 drops; when it was the last one the member's
// vtable +8 runs, its count at +8 drops and at zero the vtable +0xC runs. For the fourth member that +0xC call is
// the function's tail call instead (the frame is popped first). v0 = the vtable word when nothing was released,
// 1 when a count was not the last one, otherwise the last call's result.
u32 decrement_refcount(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}
void destroy_reference_members(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 object = ctx.gpr[4];
    u32 v0 = vtable;
    bool tail = false;
    u32 tail_target = 0u;
    u32 tail_v0 = 0u;
    u32 tail_argument = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        put(rt, object, vtable);
        const u32 offsets[4] = {0x28u, 0x20u, 0x14u, 0x0Cu};
        for (unsigned i = 0; i < 4u; ++i) {
            const u32 member = word(rt, object + offsets[i]);
            if (member == 0u) continue;
            if (decrement_refcount(rt, member + 4u) != 1u) {
                v0 = 1u;
                continue;
            }
            v0 = call_virtual(rt, ctx, word(rt, member), word(rt, word(rt, member) + 8u), {member});
            if (decrement_refcount(rt, member + 8u) != 1u) {
                v0 = 1u;
                continue;
            }
            tail_v0 = word(rt, member);
            const u32 target = word(rt, tail_v0 + 0xCu);
            if (i != 3u) {
                v0 = call_virtual(rt, ctx, tail_v0, target, {member});
                continue;
            }
            tail = true;
            tail_target = target;
            tail_argument = member;
        }
    }
    if (tail) {
        ctx.gpr[2] = tail_v0;
        tail_call(ctx, tail_target, {tail_argument});
        return;
    }
    ctx.gpr[2] = v0;
}
void func_08B987B8(Runtime &rt, AllegrexContext &ctx) { destroy_reference_members(rt, ctx, 0x08CC2F78u); }
void func_08B99ED0(Runtime &rt, AllegrexContext &ctx) { destroy_reference_members(rt, ctx, 0x08CC2F78u); }
void func_08BAFF8C(Runtime &rt, AllegrexContext &ctx) { destroy_reference_members(rt, ctx, 0x08CC2F78u); }

// --- 0x08C2EF40 ---
// a0 = begin, a1 = end, a2 = depth limit, a3 = comparator object. A quicksort over 4-byte elements: fewer than 17
// elements returns the count, the depth limit hands the range to 0x08C2EDC4, otherwise a pivot is chosen through
// 0x08C2E994 and the right part recurses while the left one stays in the loop. The element before the pivot is
// picked by 08B35FD8-free comparisons the comparator object's vtable +8 answers (0 when equal).
u32 compare_elements(Runtime &rt, AllegrexContext &ctx, u32 object, u32 left, u32 right) {
    const u32 vtable = word(rt, object);
    const u32 target = word(rt, vtable + 8u);
    return call_virtual(rt, ctx, target, target, {object, left, right});
}
void func_08C2EF40(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const u32 begin = ctx.gpr[4];
    u32 end = ctx.gpr[5];
    const u32 depth = ctx.gpr[6];
    const u32 comparator = ctx.gpr[7];
    s32 count = sar(static_cast<s32>(end - begin), 2);
    put(rt, frame.at(0x10u), begin);
    u32 v0 = static_cast<u32>(count);
    u32 passed = 0u;               // s7
    u32 remaining = depth;         // s6
    if (count < 17) {
        ctx.gpr[2] = v0;
        return;
    }
    if (depth == 0u) {
        ctx.gpr[2] = call(rt, ctx, 0x08C2EDC4u, {begin, end, end, comparator});
        return;
    }
    for (;;) {
        if (passed == depth) {
            ctx.gpr[2] = call(rt, ctx, 0x08C2EDC4u, {begin, end, end, comparator});
            return;
        }
        const u32 middle = begin + 4u * static_cast<u32>(static_cast<s32>(v0) / 2);
        u32 position = begin;
        const u32 last = end - 4u;
        remaining -= 1u;
        passed += 1u;
        if (compare_elements(rt, ctx, comparator, begin, middle) != 0u) {
            if (compare_elements(rt, ctx, comparator, middle, last) != 0u) {
                position = middle;
            } else if (compare_elements(rt, ctx, comparator, begin, last) != 0u) {
                position = last;
            }
        } else {
            if (compare_elements(rt, ctx, comparator, begin, last) == 0u) {
                if (compare_elements(rt, ctx, comparator, middle, last) != 0u) position = last;
            }
        }
        call(rt, ctx, 0x08A76ABCu, {frame.at(0x00u), position});
        const u32 pivot = call(rt, ctx, 0x08C2E994u, {begin, end, frame.at(0x00u), comparator});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x00u)});
        call(rt, ctx, 0x08C2EF40u, {pivot, end, remaining, comparator});
        s32 left = sar(static_cast<s32>(pivot - word(rt, frame.at(0x10u))), 2);
        end = pivot;
        v0 = static_cast<u32>(left);
        if (left < 17) {
            ctx.gpr[2] = v0;
            return;
        }
    }
}

// --- 0x08C35D44 ---
// a0 = position, a1 = limit, a2 = pivot element: walks `position` forward while its key (the word at +0x14 of the
// node the word at +4 points to) is below the pivot's, then walks `limit` backward, and swaps 0x0C-byte elements
// through the temporary at sp+4 (0x08AAE214 copies in, 0x08A5BAD8 copies the node pointer, 0x08A5B9EC destroys)
// until the two positions cross. Every node pointer is asserted non-null (0x08A5BB7C, message 0x08CB39CC).
// v0 = the position where they crossed.
void func_08C35D44(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    u32 forward = ctx.gpr[4];
    u32 backward = ctx.gpr[5];
    const u32 pivot = ctx.gpr[6];
    const u32 temp = frame.at(0x04u);
    for (;;) {
        // Forward: while this element's key is below the pivot node's.
        for (;;) {
            const u32 node = word(rt, forward + 4u);
            u32 other = word(rt, pivot + 4u);
            if (node == 0u) {
                call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
                other = word(rt, pivot + 4u);
            }
            if (other == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
            if (!(static_cast<s32>(word(rt, node + 0x14u)) < static_cast<s32>(word(rt, other + 0x14u)))) break;
            forward += 0xCu;
        }
        // One more look: the pivot's key below this element's skips straight to the backward walk, and otherwise
        // 0x08B35FD8 decides whether this element stays.
        const u32 node = word(rt, forward + 4u);
        u32 other = word(rt, pivot + 4u);
        if (node == 0u) {
            call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
            other = word(rt, pivot + 4u);
        }
        if (other == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
        const u32 key = word(rt, node + 0x14u);
        if (!(static_cast<s32>(word(rt, other + 0x14u)) < static_cast<s32>(key)) &&
            static_cast<s32>(call(rt, ctx, 0x08B35FD8u, {word(rt, forward), word(rt, pivot)})) < 0) {
            forward += 0xCu;
            continue;
        }
        // Backward: while the pivot node's key is below this element's; then one look with 0x08B35FD8 deciding
        // whether to step back again.
        for (;;) {
            u32 previous = word(rt, pivot + 4u);
            if (previous == 0u) {
                call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
                previous = word(rt, pivot + 4u);
            }
            backward -= 0xCu;
            u32 entry = word(rt, backward + 4u);
            if (entry == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
            if (static_cast<s32>(word(rt, previous + 0x14u)) < static_cast<s32>(word(rt, entry + 0x14u))) continue;
            previous = word(rt, pivot + 4u);
            if (previous == 0u) {
                call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
                previous = word(rt, pivot + 4u);
            }
            entry = word(rt, backward + 4u);
            if (entry == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
            const u32 pivot_key = word(rt, previous + 0x14u);
            if (static_cast<s32>(word(rt, entry + 0x14u)) < static_cast<s32>(pivot_key)) break;
            if (static_cast<s32>(call(rt, ctx, 0x08B35FD8u, {word(rt, pivot), word(rt, backward)})) >= 0) break;
        }
        if (forward >= backward) {
            ctx.gpr[2] = forward;
            return;
        }
        // Swap the two 0x0C-byte elements through sp+4.
        const u32 saved_word = word(rt, forward);
        call(rt, ctx, 0x08AAE214u, {temp, forward + 4u});
        put(rt, frame.at(0x00u), saved_word);
        const u32 next = backward + 4u;
        put8(rt, frame.at(0x08u), rt.memory().load8(forward + 8u));
        put(rt, forward, word(rt, backward));
        if (forward + 4u != next) call(rt, ctx, 0x08A5BAD8u, {forward + 4u, word(rt, backward + 4u)});
        put8(rt, forward + 8u, rt.memory().load8(backward + 8u));
        put(rt, backward, word(rt, frame.at(0x00u)));
        if (next != temp) call(rt, ctx, 0x08A5BAD8u, {next, word(rt, frame.at(0x04u))});
        put8(rt, backward + 8u, rt.memory().load8(frame.at(0x08u)));
        call(rt, ctx, 0x08A5B9ECu, {temp});
        forward += 0xCu;
    }
}

// --- 0x08C70A50 ---
// a0 = object, a1 = argument: builds a temporary at sp+0x20 (0x08C70314) and, through the virtual slot 3 (or slot 4
// for the flag values 0x40 and 8) of the holder at +0x80 of the sub-object, asks it to accept the argument; a
// non-zero answer sets bit 0 of the sub-object's +0x14 through 0x08C54CFC. The tail clears the +0x2000 flag path
// through 0x08B62318. v0 = a0.
void func_08C70A50(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    call(rt, ctx, 0x08C70314u, {frame.at(0x20u), self});
    u32 v1 = word(rt, frame.at(0x24u));
    if (rt.memory().load8(frame.at(0x20u)) != 0u) {
        u32 object = self + word(rt, word(rt, self) - 0xCu);
        if (rt.memory().load8(object + 0x75u) == 0u) {
            put8(rt, object + 0x74u, call(rt, ctx, 0x08C5477Cu, {object, 0x20u}));
            put8(rt, object + 0x75u, 1u);
            object = self + word(rt, word(rt, self) - 0xCu);
        }
        const u32 selector = static_cast<u32>(signed_byte(rt, object + 0x74u));
        const u32 flags = word(rt, object + 0x0Cu) & 0x4Au;
        const u32 holder = word(rt, object + 0x80u);
        if (holder == 0u) {
            ctx.gpr[2] = call(rt, ctx, 0x08B5A9FCu);
            return;
        }
        const u32 table = word(rt, holder);
        u32 answer;
        if (flags == 0x40u || flags == 0x08u) {
            put8(rt, frame.at(0x1Cu), word(rt, object + 0x78u) < 1u ? 1u : 0u);
            put(rt, frame.at(0x18u), word(rt, object + 0x78u));
            const u32 target = word(rt, table + 0x10u);
            answer = call_virtual(rt, ctx, target, target, {holder, frame.at(0x18u), object, selector, argument});
        } else {
            put8(rt, frame.at(0x0Cu), word(rt, object + 0x78u) < 1u ? 1u : 0u);
            put(rt, frame.at(0x08u), word(rt, object + 0x78u));
            const u32 target = word(rt, table + 0x0Cu);
            answer = call_virtual(rt, ctx, target, target, {holder, frame.at(0x08u), object, selector, argument});
        }
        put(rt, frame.at(0x34u), ctx.gpr[3]);
        put(rt, frame.at(0x30u), answer);
        if ((ctx.gpr[3] & 0xFFu) != 0u) {
            const u32 sub = self + word(rt, word(rt, self) - 0xCu);
            call(rt, ctx, 0x08C54CFCu, {sub, word(rt, sub + 0x14u) | 1u});
        }
        v1 = word(rt, frame.at(0x24u));
    }
    const u32 head = word(rt, word(rt, v1) - 0xCu) + v1;
    if ((word(rt, head + 0x0Cu) & 0x2000u) == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    if (call(rt, ctx, 0x08B62318u) != 0u) {
        ctx.gpr[2] = self;
        return;
    }
    const u32 base = word(rt, frame.at(0x24u));
    const u32 base2 = base + word(rt, word(rt, base) - 0xCu);
    const u32 slot = word(rt, base2 + 0x78u);
    if (slot != 0u) {
        const u32 table = word(rt, slot);
        const u32 target = word(rt, table + 0x18u);
        if (call_virtual(rt, ctx, target, target, {slot}) == 0xFFFFFFFFu) {
            const u32 again = word(rt, frame.at(0x24u));
            const u32 sub = again + word(rt, word(rt, again) - 0xCu);
            call(rt, ctx, 0x08C54CFCu, {sub, word(rt, sub + 0x14u) | 1u});
        }
    }
    ctx.gpr[2] = self;
}

// --- 0x089C9F24 ---
// a0 = object, a1 = a holder pointer, a2 = a second object, a3 = a flag byte: builds two temporaries (0x08B85EB8 at
// sp+4 and 0x089C8818 at sp+8), takes the lock 0x08B2E05C, and when 0x089C9CE4 accepts the pair calls the object's
// slots +0x12C, +0x130 and (with the flag) +0x150 on the holder's target; releases the temporaries and the lock.
// v0 = 1 when the pair was accepted, 0 otherwise.
u32 require(Runtime &rt, AllegrexContext &ctx, u32 value, u32 message) {
    if (value == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    return value;
}

// --- 0x089DCADC ---
// a0 = object, a1 = source: for every 4-word row selected by bit 9 of the source's +0x140, builds a four-float
// vector (0x08A98C1C into sp+0x14, each component through 0x08A87EEC into one of the temporaries at sp+0xC..sp+0)
// and appends it to the container 0x08A98C1C made at sp+0x10. v0 = 0x08A76ABC's caller-visible object (a0).
void func_089DCADC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    put(rt, frame.at(0x20u), self);
    const u32 index = (word(rt, source + 0x140u) >> 9) & 1u;
    const u32 base = source + 12u * index;
    const s32 count = sar(static_cast<s32>(word(rt, base + 0x120u) - word(rt, base + 0x11Cu)), 4);
    call(rt, ctx, 0x08A98C1Cu, {frame.at(0x10u), static_cast<u32>(count)});
    if (count != 0) {
        for (u32 row_index = 0; row_index < static_cast<u32>(count); ++row_index) {
            const u32 selector = (word(rt, source + 0x140u) >> 9) & 1u;
            const u32 table = word(rt, source + 12u * selector + 0x11Cu);
            const u32 row = table + (row_index << 4);
            call(rt, ctx, 0x08A98C1Cu, {frame.at(0x14u), 3u});
            for (unsigned component = 0; component < 4u; ++component) {
                const u32 temporary = frame.at(0x0Cu - 4u * component);
                const float value = float_at(rt, row + 4u * component);
                const u32 target = word(rt, word(rt, frame.at(0x14u)) + 0x14u) + 4u * component;
                callf(rt, ctx, 0x08A87EECu, {temporary}, {value});
                if (target != temporary) call(rt, ctx, 0x08A5BAD8u, {target, word(rt, temporary)});
                call(rt, ctx, 0x08A5B9ECu, {temporary});
            }
            const u32 entry = word(rt, word(rt, frame.at(0x10u)) + 0x14u) + (row_index << 2);
            if (entry != frame.at(0x14u)) call(rt, ctx, 0x08A5BAD8u, {entry, word(rt, frame.at(0x14u))});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
        }
    }
    call(rt, ctx, 0x08A76ABCu, {word(rt, frame.at(0x20u)), frame.at(0x10u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
    ctx.gpr[2] = word(rt, frame.at(0x20u));
}

// --- 0x08A17818 ---
// Takes the lock 0x08CF6250 and, when the singleton 0x08CF6240 is still null and the list 0x08A17698 returns is not
// empty, drops the old holder at 0x08CF6244, allocates the 0x130-byte object through 0x08B62344 (vtable
// 0x08CD05C0), initialises it through 0x08B0EA48 and stores it in 0x08CF6240 with its callbacks at +0x74/+0x80-ish;
// the object's vtable +8 then runs. v0 = 0x08B3457C's result (the unlock).
void func_08A17818(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    call(rt, ctx, 0x08B344E0u, {0x08CF6250u});
    u32 list = call(rt, ctx, 0x08A17698u);
    if (word(rt, list) != list && word(rt, 0x08CF6240u) == 0u) {
        list = call(rt, ctx, 0x08A17698u);
        const u32 node = word(rt, list);
        if (node + 8u != 0x08CF6244u) {
            call(rt, ctx, 0x08B855E0u, {0x08CF6244u});
            if (node + 0xCu != 0x08CF6248u) {
                const u32 first = word(rt, node + 0xCu);
                if (first != 0u) call(rt, ctx, 0x08A5BA08u, {first});
                const u32 second = word(rt, 0x08CF6248u);
                if (second != 0u) call(rt, ctx, 0x08A5B930u, {second});
                put(rt, 0x08CF6248u, word(rt, node + 0xCu));
            }
        }
        list = call(rt, ctx, 0x08A17698u);
        const u32 holder = word(rt, list);
        call(rt, ctx, 0x08B5C094u, {holder});
        const u32 stored = word(rt, holder + 0xCu);
        if (stored != 0u) call(rt, ctx, 0x08A5B930u, {stored});
        const u32 value = word(rt, holder + 0x8u);
        if (value - 0xCu != 0x08D74FD0u &&
            static_cast<s32>(call(rt, ctx, 0x08B60804u, {value - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call(rt, ctx, 0x08B8542Cu, {value - 0xCu, frame.at(0x00u)});
        }
        call(rt, ctx, 0x08A5BC54u, {holder, 0x10u, 4u});
        const u32 allocated = call(rt, ctx, 0x08B62344u, {0x130u});
        const u32 previous = word(rt, 0x08CF6244u);
        call(rt, ctx, 0x08B0DBF8u, {allocated});
        put(rt, allocated, 0x08CD05C0u);
        if (call(rt, ctx, 0x08B0EA48u, {allocated, previous}) != 0u) {
            put(rt, allocated + 0x30u, 0x08A17A0Cu);
            put(rt, allocated + 0x34u, 0u);
            put8(rt, allocated + 0x68u, 1u);
            put(rt, 0x08CF6240u, allocated);
            put8(rt, allocated + 0xF8u + 0x14u, 1u);
            put8(rt, allocated + 0xF8u + 0x30u, 0u);
            put(rt, allocated + 0xF8u + 0x2Cu, 0u);
            put(rt, allocated + 0xF8u + 0x28u, 0u);
            put(rt, word(rt, 0x08CF6240u) + 0x7Cu, 0u);
            put(rt, word(rt, 0x08CF6240u) + 0x74u, 0x08A17428u);
            put(rt, word(rt, 0x08CF6240u) + 0x78u, 0u);
            const u32 object = word(rt, 0x08CF6240u);
            const u32 vtable = word(rt, object);
            const u32 target = word(rt, vtable + 8u);
            call_virtual(rt, ctx, target, target, {object});
        } else {
            const u32 vtable = word(rt, allocated);
            const u32 target = word(rt, vtable + 4u);
            call_virtual(rt, ctx, target, target, {allocated});
        }
    }
    ctx.gpr[2] = call(rt, ctx, 0x08B3457Cu, {0x08CF6250u});
}

// --- 0x08AF5D60 ---
// a0 = owner (word at +0x9C is the table), a1 = index, a2 = first 4-float destination, a3 = second 3-float
// destination, t0/t1 = two tables. Reads the 0x20-byte entry at table + index*0x80 (+0x1C is a flag): the flag
// clear copies the entry's two vectors and runs 0x08B4BAA8 / 0x08B4AA34 / 0x08B4B858, otherwise copies them
// directly; then 0x08C3FBBC and 0x08C3FB94 publish the two pointers, and the halves at +8 (with the same two
// vectors) and +6 (the results) recurse into this function itself.

// --- 0x08BF95F0 ---
// a0 = first, a1 = middle, a2 = last: an 8-byte-element rotation. Equal first/middle (or the same left/right
// sizes) ends in a tail call to 0x08BF95B0; otherwise 0x08BF8BD4 answers the number of chunks and each pass copies
// the elements of one side over the other (forward when the left side is smaller, backward otherwise) while the
// saved element goes in at the seam. v0 is not part of the answer (the original leaves whatever the calls left).
void func_08BF95F0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const u32 first = ctx.gpr[4];
    const u32 middle = ctx.gpr[5];
    const u32 last = ctx.gpr[6];
    if (middle == first) return;
    u32 tail_target = 0u;
    {
        const s32 total = sar(static_cast<s32>(last - first), 3);
        const s32 left = sar(static_cast<s32>(middle - first), 3);
        const s32 rest = total - left;
        if (left == rest) {
            tail_target = 0x08BF95B0u;
        } else {
            const s32 chunks = static_cast<s32>(call(rt, ctx, 0x08BF8BD4u, {static_cast<u32>(total), static_cast<u32>(left)}));
            if (chunks > 0) {
                const u32 left_bytes = static_cast<u32>(left) << 3;
                const u32 right_bytes = static_cast<u32>(rest) << 3;
                const u32 guard2 = last - left_bytes;
                const bool forward = left < rest;
                u32 source = first;
                u32 guard1 = right_bytes + first;
                u32 passed = 0;
                for (;;) {
                    const u32 saved_low = word(rt, source);
                    const u32 saved_high = word(rt, source + 4u);
                    u32 seam = source;
                    const s32 count = (forward ? rest : left) / chunks - 1;
                    if (count > 0) {
                        u32 at = source;
                        for (s32 step = 0;;) {
                            const bool skip = forward ? (guard1 < at) : (at < guard2);
                            const u32 other = forward ? (at - right_bytes) : (left_bytes + at);
                            ++step;
                            if (skip) {
                                put(rt, at, word(rt, other));
                                put(rt, at + 4u, word(rt, other + 4u));
                                at = other;
                            }
                            const u32 from = forward ? (left_bytes + at) : (at - right_bytes);
                            seam = from;
                            put(rt, at, word(rt, from));
                            put(rt, at + 4u, word(rt, from + 4u));
                            if (step == count) break;
                            at = from;
                        }
                    }
                    put(rt, seam, saved_low);
                    source += 8u;
                    guard1 += 8u;
                    put(rt, seam + 4u, saved_high);
                    ++passed;
                    if (static_cast<u32>(chunks) == passed) break;
                }
            }
        }
    }
    if (tail_target != 0u) tail_call(ctx, tail_target, {first, middle, middle});
}

// --- 0x08C47C08 ---
// a0 = container (words +0 begin, +4 end, +8 capacity), a1 = position, a2 = an 8-byte value: inserts the value,
// shifting the elements from the end down to the position; when the buffer is full it grows through 0x08B35DA0 and
// rebuilds it through 0x08C47104 / 0x08C470F4 (0x08B5ADDC raises a length error past 0x1FFFFFFF elements).
// v0 = the value's first word, or the new capacity after a grow.
void func_08C47C08(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const u32 container = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    const u32 end = word(rt, container + 0x4u);
    if (end != word(rt, container + 0x8u)) {
        if (end != 0u) {
            put(rt, end, word(rt, end - 0x8u));
            put(rt, end + 4u, word(rt, end - 0x4u));
        }
        const u32 slot = word(rt, container + 0x4u);
        put(rt, container + 0x4u, slot + 0x8u);
        const u32 gap = slot - 0x8u - position;
        put(rt, frame.at(0x04u), word(rt, value));
        put(rt, frame.at(0x08u), word(rt, value + 0x4u));
        const s32 count = sar(static_cast<s32>(gap), 3);
        u32 destination = slot - 0x8u;
        u32 at = destination;
        if (count > 0) {
            for (s32 step = 0;;) {
                at -= 0x8u;
                put(rt, destination, word(rt, at));
                put(rt, destination + 4u, word(rt, at + 4u));
                ++step;
                if (step == count) break;
                destination -= 0x8u;
            }
        }
        put(rt, position, word(rt, frame.at(0x04u)));
        put(rt, position + 4u, word(rt, frame.at(0x08u)));
        ctx.gpr[2] = word(rt, frame.at(0x04u));
        return;
    }
    const u32 begin = word(rt, container);
    const u32 used = static_cast<u32>(sar(static_cast<s32>(end - begin), 3));
    if (static_cast<u32>(used) == 0x1FFFFFFFu) call(rt, ctx, 0x08B5ADDCu, {0x08CB4AD4u});
    u32 allocated = 0u;
    u32 bytes = 0u;
    if (used == 0u) {
        bytes = 8u;
        allocated = call(rt, ctx, 0x08B35DA0u, {bytes});
    } else {
        const u32 doubled = used << 1;
        if (doubled < used) {
            bytes = 0xFFFFFFF8u;
            allocated = call(rt, ctx, 0x08B35DA0u, {bytes});
        } else if (doubled != 0u) {
            bytes = doubled << 3;
            allocated = call(rt, ctx, 0x08B35DA0u, {bytes});
        }
    }
    const u32 first_slot = call(rt, ctx, 0x08C47104u, {word(rt, container), position, allocated, frame.at(0x02u)});
    if (first_slot != 0u) {
        put(rt, first_slot, word(rt, value));
        put(rt, first_slot + 4u, word(rt, value + 0x4u));
    }
    const u32 new_end = call(rt, ctx, 0x08C47104u, {position, word(rt, container + 0x4u), first_slot + 0x8u, frame.at(0x01u)});
    call(rt, ctx, 0x08C470F4u, {word(rt, container), word(rt, container + 0x4u), frame.at(0x00u)});
    const u32 old_begin = word(rt, container);
    const u32 new_capacity = allocated + bytes;
    if (old_begin != 0u) call(rt, ctx, 0x08B35254u, {old_begin});
    put(rt, container + 0x8u, new_capacity);
    put(rt, container + 0x4u, new_end);
    put(rt, container, allocated);
    ctx.gpr[2] = new_capacity;
}

// --- 0x08B98B8C, 0x08B98D98, 0x08BA6C60, 0x08BA6E6C ---
// a0 = object: stores `first_vtable` at *object, releases the handler at +8 through `release` (a1 = 0 for two of
// them), stores 0x08CC2F78 and releases the four reference-counted members (0x28, 0x20, 0x14, 0x0C). With
// `member_tail` the fourth member's vtable +0xC call is the function's tail call; otherwise the four are plain calls
// and the function ends with a tail call to 0x08B64654 with the object.
void destroy_object(Runtime &rt, AllegrexContext &ctx, u32 first_vtable, u32 release, bool clear_argument, bool member_tail) {
    const u32 self = ctx.gpr[4];
    u32 tail_target = 0u;
    u32 tail_v0 = 0u;
    u32 tail_argument = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        put(rt, self, first_vtable);
        const u32 handler = word(rt, self + 0x8u);
        if (handler != 0u) {
            if (clear_argument) call(rt, ctx, release, {handler, 0u});
            else call(rt, ctx, release, {handler});
        }
        put(rt, self, 0x08CC2F78u);
        const u32 offsets[4] = {0x28u, 0x20u, 0x14u, 0x0Cu};
        for (unsigned i = 0; i < 4u; ++i) {
            const u32 member = word(rt, self + offsets[i]);
            if (member == 0u) continue;
            if (decrement_refcount(rt, member + 4u) != 1u) continue;
            call_virtual(rt, ctx, word(rt, member), word(rt, word(rt, member) + 8u), {member});
            if (decrement_refcount(rt, member + 8u) != 1u) continue;
            const u32 table = word(rt, member);
            if (member_tail && i == 3u) {
                tail_target = word(rt, table + 0xCu);
                tail_v0 = table;
                tail_argument = member;
                break;
            }
            call_virtual(rt, ctx, table, word(rt, table + 0xCu), {member});
        }
    }
    if (tail_target != 0u) {
        ctx.gpr[2] = tail_v0;
        tail_call(ctx, tail_target, {tail_argument});
    } else if (!member_tail) {
        tail_call(ctx, 0x08B64654u, {self});
    }
}
void func_08B98B8C(Runtime &rt, AllegrexContext &ctx) { destroy_object(rt, ctx, 0x08CBFBC8u, 0x08AF75A0u, false, true); }
void func_08BA6C60(Runtime &rt, AllegrexContext &ctx) { destroy_object(rt, ctx, 0x08CC1768u, 0x08AF8AB0u, true, true); }
void func_08B98D98(Runtime &rt, AllegrexContext &ctx) { destroy_object(rt, ctx, 0x08CBFBC8u, 0x08AF75A0u, false, false); }
void func_08BA6E6C(Runtime &rt, AllegrexContext &ctx) { destroy_object(rt, ctx, 0x08CC1768u, 0x08AF8AB0u, true, false); }

// --- 0x089F3D20, 0x089F3F30 ---
// a0 = object: stores the two vtables (at +0 and +0x38), destroys the eight temporaries at +0x24C..+0x268 through
// 0x08A5B9EC, then releases the eight holders at +0x22C..+0x248 (each skipped when it is the empty holder
// 0x08D74FD0, and freed through 0x08B8542C only when 0x08B60804 does not take it), and finally runs 0x089E2430.
// v0 = 0x089E2430's result.
void destroy_object_with_members(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CC8288u);
    put(rt, self + 0x38u, 0x08CC84C4u);
    for (u32 offset = 0x268u; offset >= 0x24Cu; offset -= 4u) call(rt, ctx, 0x08A5B9ECu, {self + offset});
    const u32 offsets[8] = {0x248u, 0x244u, 0x240u, 0x23Cu, 0x238u, 0x234u, 0x230u, 0x22Cu};
    for (unsigned i = 0; i < 8u; ++i) {
        const u32 value = word(rt, self + offsets[i]);
        const u32 object = value - 0xCu;
        if (object == 0x08D74FD0u) continue;
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {value - 0x4u, 0xFFFFFFFFu})) > 0) continue;
        call(rt, ctx, 0x08B8542Cu, {object, frame.at(7u - i)});
    }
    ctx.gpr[2] = call(rt, ctx, 0x089E2430u, {self});
}
void func_089F3F30(Runtime &rt, AllegrexContext &ctx) { destroy_object_with_members(rt, ctx); }

// --- 0x08912C0C ---
// a0 = object: constructor. Initialises the base through 0x08B28DCC, clears 0x28..0x40 and 0xD4..0x110, four
// sub-objects through 0x08AB9284 (+0x44, +0x68, +0x8C, +0xB0), five through 0x0891156C (+0x120, +0x138, +0x150,
// +0x168, +0x180) with f12 = 0 and 1.0f at +0x114/+0x118/+0x1A8, then clears the table at +0x7B0 and +0x800 (four
// passes of 0x40 bytes). v0 = 0.
void func_08912C0C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B28DCCu, {self});
    put8(rt, self + 0x2Eu, 0xFFFFFFFFu);
    put8(rt, self + 0x28u, 0u);
    put8(rt, self + 0x2Bu, 0u);
    put8(rt, self + 0x29u, 0u);
    put8(rt, self + 0x2Cu, 0u);
    put8(rt, self + 0x2Du, 0u);
    put8(rt, self + 0x2Fu, 0u);
    put8(rt, self + 0x30u, 0u);
    put8(rt, self + 0x31u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);
    call(rt, ctx, 0x08AB9284u, {self + 0x44u});
    put(rt, self + 0x40u, 0u);
    call(rt, ctx, 0x08AB9284u, {self + 0x68u});
    call(rt, ctx, 0x08AB9284u, {self + 0x8Cu});
    call(rt, ctx, 0x08AB9284u, {self + 0xB0u});
    for (u32 offset = 0xD4u; offset <= 0x110u; offset += 4u) put(rt, self + offset, 0u);
    put_float(rt, self + 0x114u, 1.0f);
    callf(rt, ctx, 0x0891156Cu, {self + 0x120u}, {0.0f});
    put_float(rt, self + 0x118u, 1.0f);
    callf(rt, ctx, 0x0891156Cu, {self + 0x138u}, {0.0f});
    callf(rt, ctx, 0x0891156Cu, {self + 0x150u}, {0.0f});
    callf(rt, ctx, 0x0891156Cu, {self + 0x168u}, {0.0f});
    callf(rt, ctx, 0x0891156Cu, {self + 0x180u}, {0.0f});
    put_float(rt, self + 0x1A8u, 1.0f);
    put(rt, self + 0x198u, 0u);
    put(rt, self + 0x19Cu, 0u);
    put(rt, self + 0x1A0u, 0u);
    put(rt, self + 0x1A4u, 0u);
    put8(rt, self + 0x1ACu, 0u);
    u32 counter = 0x7Fu;
    for (;;) {
        const bool again = counter != 0u;
        counter -= 1u;
        if (!again) break;
    }
    put_float(rt, self + 0x7B0u, 1.0f);
    put(rt, self + 0x7B4u, 0u);
    put(rt, self + 0x7BCu, 0u);
    put(rt, self + 0x7B8u, 0u);
    put(rt, self + 0x7C0u, 0u);
    put(rt, self + 0x7C8u, 0u);
    put(rt, self + 0x7C4u, 0u);
    put(rt, self + 0x7CCu, 0u);
    put(rt, self + 0x7D4u, 0u);
    put(rt, self + 0x7D0u, 0u);
    put(rt, self + 0x7D8u, 0u);
    put(rt, self + 0x7E0u, 0u);
    put(rt, self + 0x7DCu, 0u);
    put(rt, self + 0x7E4u, 0u);
    put(rt, self + 0x7ECu, 0u);
    put(rt, self + 0x7E8u, 0u);
    put(rt, self + 0x7F0u, 0u);
    put(rt, self + 0x7F8u, 0u);
    put(rt, self + 0x7F4u, 0u);
    u32 table = self + 0x800u;
    for (u32 passes = 4u; passes != 0u; passes -= 1u, table += 0x40u) {
        put(rt, table, 0u);
        put(rt, table + 0x04u, 0u);
        put(rt, table + 0x08u, 0u);
        put(rt, table + 0x10u, 0u);
        put(rt, table + 0x14u, 0u);
        put(rt, table + 0x20u, 0u);
        put(rt, table + 0x24u, 0u);
        put(rt, table + 0x0Cu, 0u);
        put(rt, table + 0x18u, 0u);
        put(rt, table + 0x1Cu, 0u);
        put(rt, table + 0x28u, 0u);
        put(rt, table + 0x2Cu, 0u);
        put(rt, table + 0x30u, 0u);
        put(rt, table + 0x34u, 0u);
    }
    put(rt, self + 0x11Cu, 0u);
    ctx.gpr[2] = 0u;
}

// --- 0x088CFEB0 ---
// a0 = object, a1 = a source object: reads the vectors at sp+0x28/+0 through 0x088FAB84 / 0x088FAC80, takes the
// heading from 0x088D390C into +0x148/+0x14C, asks the object's +0x14C holder for the +0x150 value (its virtual
// +0x1C), then stores the path distance through 0x088D2DE0 at +0x140 (as a magnitude) and +0x144, the height
// difference through 0x088FB100 at +0x154 (floored at 0) and the source's +0xC at +0x158.
// v0 = 0x088FB100's result.

// --- 0x088E4168 ---
// a0 = object, a1 = a 0x200-byte area: asks the object's virtual +0x44 to fill sp+0 (five bytes become parameters
// 0x34..0x38 through 0x088EF438), clears the area (0x08B36018) and, when virtual +0x48 answers a positive count,
// formats each 0x10-byte record's three words at +0, +0x100 and +0x200 (0x08B35F60 gives each length, 0x08B37640
// appends through 0x08CA0AB8), terminates the three strings and hands them over as 0x2B/0x2C/0x2D through
// 0x088EF430. v0 = the last call's result.

// --- 0x0887F184 ---
// a0 = object, a1 = a string: parses a four-character signature (0x"RESF" through 0x08AC2690). With the first tag it
// reads one word (0x08AC2730 at sp+0x10), destroys the temporary, asks 0x08B7B8C4 for the next tag and either
// 0x0887EC04's path with the object's +0x74 or 0x0887EE40 with a 3-token read; with the second it reads a word into
// sp+0xC and 0x0887EE68 through 0x0886C798. The temporary at sp+0x14 is destroyed on the way out.
// v0 = the last 0x08AC26DC call's result.
void func_0887F184(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    call(rt, ctx, 0x08AC26D4u, {frame.at(0x14u), 0u});
    if (call(rt, ctx, 0x08AC2690u, {text}) == 0x52455346u) {
        call(rt, ctx, 0x08AC2730u, {frame.at(0x10u), text, 0u});
        const u32 signature = call(rt, ctx, 0x08AC2900u, {frame.at(0x10u)});
        call(rt, ctx, 0x08AC26DCu, {frame.at(0x10u)});
        call(rt, ctx, 0x08AC2730u, {frame.at(0x0Cu), text, 2u});
        put(rt, frame.at(0x14u), word(rt, frame.at(0x0Cu)));
        call(rt, ctx, 0x08AC26DCu, {frame.at(0x0Cu)});
        if (call(rt, ctx, 0x08B7B8C4u, {signature, 0x08CA03A8u}) == 0u) {
            call(rt, ctx, 0x08AC2730u, {frame.at(0x08u), text, 3u});
            const u32 value = call(rt, ctx, 0x08AC2900u, {frame.at(0x08u)});
            call(rt, ctx, 0x0887EC04u, {word(rt, self + 0x74u), value});
            call(rt, ctx, 0x08AC26DCu, {frame.at(0x08u)});
            const u32 holder = word(rt, self + 0x74u);
            call(rt, ctx, 0x0887EE40u,
                 {self, call(rt, ctx, 0x08AC2900u, {frame.at(0x14u)}), rt.memory().load8(holder + 0x50u),
                  rt.memory().load8(holder + 0x51u)});
        } else if (call(rt, ctx, 0x08B7B8C4u, {signature, 0x08CA03ACu}) == 0u) {
            const u32 field = word(rt, self + 0x1Cu);
            call(rt, ctx, 0x0886C798u, {frame.at(0x18u), field, call(rt, ctx, 0x08AC2900u, {frame.at(0x14u)})});
            const u32 holder = word(rt, frame.at(0x18u));
            call(rt, ctx, 0x08AC2730u, {frame.at(0x04u), text, 1u});
            call(rt, ctx, 0x0887EE68u, {self, holder, call(rt, ctx, 0x08AC28B8u, {frame.at(0x04u)})});
            call(rt, ctx, 0x08AC26DCu, {frame.at(0x04u)});
            const u32 value = word(rt, frame.at(0x18u));
            const u32 object = value - 0xCu;
            if (object != 0x08D75298u &&
                static_cast<s32>(call(rt, ctx, 0x08B60804u, {value - 0x4u, 0xFFFFFFFFu})) <= 0) {
                call(rt, ctx, 0x08B93554u, {object, frame.at(0x00u)});
            }
        } else {
            call(rt, ctx, 0x08B7B8C4u, {signature, 0x08CA03B0u});
        }
    }
    call(rt, ctx, 0x08AC26DCu, {frame.at(0x14u)});
}

} // namespace
} // namespace new_batch_main_21
} // namespace gt::game

#define MAIN21(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_21::name, gt::ReturnKind::kind)
MAIN21(0x089A71C8u, func_089A71C8, Word);
MAIN21(0x08B01B2Cu, func_08B01B2C, Word);
MAIN21(0x08B987B8u, func_08B987B8, Word);
MAIN21(0x08B99ED0u, func_08B99ED0, Word);
MAIN21(0x08BAFF8Cu, func_08BAFF8C, Word);
MAIN21(0x08C2EF40u, func_08C2EF40, Word);
MAIN21(0x08C35D44u, func_08C35D44, Word);
MAIN21(0x08C70A50u, func_08C70A50, Word);
MAIN21(0x089DCADCu, func_089DCADC, Word);
MAIN21(0x08A17818u, func_08A17818, Word);
MAIN21(0x08BF95F0u, func_08BF95F0, Void);
MAIN21(0x08C47C08u, func_08C47C08, Word);
MAIN21(0x08B98B8Cu, func_08B98B8C, Void);
MAIN21(0x08BA6C60u, func_08BA6C60, Void);
MAIN21(0x08B98D98u, func_08B98D98, Void);
MAIN21(0x08BA6E6Cu, func_08BA6E6C, Void);
MAIN21(0x089F3F30u, func_089F3F30, Word);
MAIN21(0x08912C0Cu, func_08912C0C, Word);
MAIN21(0x0887F184u, func_0887F184, Word);

// ---- second slice: fifteen more functions of the batch list (drafted in a parallel pass) ----
// // Main-session batch 21b: fifteen emitted functions written by hand as structured C from `tools.disasm` (drafts —
// // the main session runs the gate). No name has evidence, so they keep the recompiler's func_XXXXXXXX names. Every
// // exit path leaves v0 as the asm does: a call's result where the asm ends with a call, the word it loads before
// // returning, the constant it leaves in v0, and the leftover of the last call where the asm never writes v0 (those
// // register themselves as Void). ReturnKind::Float is used where the result leaves in $f0.
// #include "decomp_registry.hpp"
// #include "gt/game/hot_common.hpp"
// #include "guest_call.hpp"
// 
// #include <cstdint>
// #include <initializer_list>
namespace gt::game {
namespace new_batch_main_21b {
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
u32 callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
          std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// The $f0 a `callf` leaves, for the callees whose answer is a float.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats = {}) {
    callf(rt, ctx, target, arguments, floats);
    return ctx.fpr[0];
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (or whatever the caller
// left in v0), and a callee that never writes v0 returns that.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 v0, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = v0;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 unsigned_half(Runtime &rt, u32 a) { return rt.memory().load16(a); }
void put8(Runtime &rt, u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); }
void put16(Runtime &rt, u32 a, u32 v) { rt.memory().store16(a, static_cast<std::uint16_t>(v)); }
// The compiler's word-at-a-time struct copy loops (4 bytes each, `bytes` a multiple of 4).
void copy_words(Runtime &rt, u32 dst, u32 src, u32 bytes) {
    for (u32 i = 0; i < bytes; i += 4u) put(rt, dst + i, word(rt, src + i));
}
// Arithmetic shift right, as `sra`.
s32 sar(s32 value, unsigned bits) { return static_cast<s32>(value >> bits); }
// `mult` + `mflo`: the low 32 bits of the signed product.
u32 mult_low(s32 a, s32 b) { return static_cast<u32>(static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b)); }
// `%hi` of an address as a `lui`/`addiu` pair builds it: what the pair's `lui` leaves in the register when the
// `addiu` carries the signed `%lo`. The compiler reuses one such register for several addresses of the same 64 KiB
// page, and a branch that jumps over an `addiu` leaves the `lui` half in it.
constexpr u32 lui_of(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
// `size` bits of `value` starting at `offset`, as the `ext` instruction takes them.
constexpr u32 extract_bits(u32 value, unsigned offset, unsigned size) {
    return (value >> offset) & ((1u << size) - 1u);
}
// The sign-extension of a value's low byte, as `seb` (no load, unlike `signed_byte`).
s32 signed_of_byte(u32 value) { return static_cast<std::int8_t>(value & 0xFFu); }

// An object's float through its vtable slot 0x54, after asserting the pointer (0x08CA9594's message).
float component(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA9594u});
    const u32 target = word(rt, word(rt, object) + 0x54u);
    call_virtual(rt, ctx, target, target, {object});
    return ctx.fpr[0];
}
// The same through slot 0x30 as a word, and slot 0x54 as a float, with 0x08CAD518's message.
u32 vtable_code(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAD518u});
    const u32 target = word(rt, word(rt, object) + 0x30u);
    return call_virtual(rt, ctx, target, target, {object});
}
float vtable_float(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAD518u});
    const u32 target = word(rt, word(rt, object) + 0x54u);
    call_virtual(rt, ctx, target, target, {object});
    return ctx.fpr[0];
}
// A reference-counted temporary these functions hand around: release the block behind `raw` unless it is the empty
// sentinel 0x08D74FD0 or the count at raw-4 is still positive (the pair of calls the asm repeats).
void release_temporary(Runtime &rt, AllegrexContext &ctx, u32 raw, u32 flag_address) {
    const u32 control = raw - 0xCu;
    if (control != 0x08D74FD0u &&
        static_cast<s32>(call(rt, ctx, 0x08B60804u, {raw - 4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {control, flag_address});
    }
}

// The callee-saved registers (s0-s7, fp) the original saves in its prologue and restores before returning.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 saved[9];
    explicit SavedRegs(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 8u; ++i) saved[i] = c.gpr[16u + i];
        saved[8] = c.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
        ctx.gpr[30] = saved[8];
    }
};

// --- 0x08BDF848 ---
// a0 = destination vector (begin/end/capacity at +0/+4/+8, 0x1C-byte elements), a1 = source vector: copy-assigns
// the source onto the destination, allocating a fresh buffer when the destination's is shorter than the source and
// copy-assigning in place otherwise. A count is the compiler's division by 28: `sra 2`, then the 7-reciprocal
// multiply's low word. v0 = a0.
void copy_element(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 source) {
    // The first word is stored before the calls (the asm's `jal` delay slot), then the two sub-objects are copied.
    const u32 first = word(rt, source);
    put(rt, destination, first);
    call(rt, ctx, 0x08BDDFDCu, {destination + 4u, source + 4u});
    call(rt, ctx, 0x08BDEC3Cu, {destination + 0x10u, source + 0x10u});
}

// --- 0x08C3C898 ---
// a0 = vector of 0x0C-byte elements (begin/end/capacity at +0/+4/+8), a1 = where to insert, a2 = the element to copy
// in: inserts one element, moving the tail right when there is room and reallocating with twice the element count
// when there is not. v0 = the new buffer's end on the grow path, otherwise the inserted element's first word (the
// word the asm reloads from its stack copy).
void func_08C3C898(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    const s32 magic = static_cast<s32>(0xAAAAAAABu);
    u32 end = word(rt, self + 4u);
    const u32 value_first = word(rt, value);

    if (end != word(rt, self + 8u)) {
        // Room left: one more element is constructed at the end out of the last one, then the tail moves right.
        if (end != 0u) {
            const u32 last = end - 0xCu;
            put(rt, end + 8u, word(rt, last + 8u));
            put(rt, end, word(rt, last));
            put(rt, end + 4u, word(rt, last + 4u));
            end = word(rt, self + 4u);
        }
        u32 destination = end - 0xCu;
        const s32 shift = static_cast<s32>(mult_low(sar(static_cast<s32>(destination - position), 2), magic));
        put(rt, self + 4u, end + 0xCu);
        put(rt, frame.at(0x0Cu), word(rt, value + 8u));
        put(rt, frame.at(0x04u), value_first);
        put(rt, frame.at(0x08u), word(rt, value + 4u));
        for (s32 moved = 0; moved < shift; ++moved) {
            destination -= 0xCu;  // the element that moves, the slot above it is its destination
            copy_words(rt, destination + 0xCu, destination, 0xCu);
        }
        put(rt, position + 4u, word(rt, frame.at(0x08u)));
        put(rt, position, word(rt, frame.at(0x04u)));
        put(rt, position + 8u, word(rt, frame.at(0x0Cu)));
        ctx.gpr[2] = value_first;
        return;
    }

    // Full: a buffer of twice the element count (12 bytes each) is allocated and both halves copied into it.
    const u32 begin = word(rt, self);
    const u32 count = mult_low(sar(static_cast<s32>(end - begin), 2), magic);
    if (count == 0x15555555u) call(rt, ctx, 0x08B5ADDCu, {0x08CB46E0u});
    u32 new_capacity = 1u;
    u32 bytes = (new_capacity << 4) - (new_capacity << 2);  // 12 bytes; a zero count asks for one
    bool allocate = true;
    if (count != 0u) {
        new_capacity = count << 1;
        if (new_capacity < count) {
            bytes = 0xFFFFFFFCu;  // the overflow path still asks for an (impossible) allocation
        } else if (new_capacity == 0u) {
            allocate = false;
        } else {
            bytes = (new_capacity << 4) - (new_capacity << 2);
        }
    }
    u32 buffer = 0u;
    if (allocate) buffer = call(rt, ctx, 0x08B35DA0u, {bytes});
    u32 slot = call(rt, ctx, 0x08C3C800u, {begin, position, buffer, frame.at(0x02u)});
    if (slot != 0u) {
        put(rt, slot + 8u, word(rt, value + 8u));
        put(rt, slot, word(rt, value));
        put(rt, slot + 4u, word(rt, value + 4u));
    }
    // The destructor call's delay slot saves this copy's result, which becomes the new end.
    const u32 new_end = call(rt, ctx, 0x08C3C800u, {position, word(rt, self + 4u), slot + 0xCu, frame.at(0x01u)});
    call(rt, ctx, 0x08C3C7F0u, {word(rt, self), word(rt, self + 4u), frame.at(0x00u)});
    const u32 old = word(rt, self);
    if (old != 0u) call(rt, ctx, 0x08B35254u, {old});
    put(rt, self + 8u, buffer + bytes);
    put(rt, self + 4u, new_end);
    put(rt, self, buffer);
    ctx.gpr[2] = buffer + bytes;
}

// --- 0x08C4E244 ---
// a0 = a typed value (type tag at +0, data pointer at +4), a1 = where the value goes: compares the tag against the
// twelve registered type descriptors (each query answers its own id) and the first match stores the pointed-to value
// converted to that type as a word. v0 = 1 when something was stored, 0 when no type matched.
void func_08C4E244(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    u32 tag = word(rt, self);  // read in each `jal`'s delay slot, i.e. before the query
    if (tag == call(rt, ctx, 0x08B9DDB0u)) {
        // float: trunc.w.s, stored back as a float
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            const float value = float_at(rt, data);
            put_float(rt, out, static_cast<float>(static_cast<s32>(ctx.fpu_float_to_word(value, 1u))));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08C4D18Cu)) {
        // 64-bit: the word pair at +0/+4 goes to the helper, its result to *a1
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, call(rt, ctx, 0x08B71DC8u, {word(rt, data), word(rt, data + 4u)}));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC473Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, static_cast<u32>(signed_byte(rt, data)));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC48B4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, rt.memory().load8(data));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC45C4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, static_cast<u32>(signed_half(rt, data)));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC444Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, unsigned_half(rt, data));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC42D4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, word(rt, data));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08B9E8D4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, word(rt, data));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC415Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, word(rt, data));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC3FE4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, word(rt, data));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08B9CEBCu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, rt.memory().load8(data));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC4A2Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, out, static_cast<u32>(signed_byte(rt, data)));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    ctx.gpr[2] = 0u;
}

// --- 0x08C56DDC ---
// a0 = a reader host (count at +4, the sub-object offset at *a0, its reader pointer at sub+0x78 and flag word at
// sub+0x14), a1 = a byte sink (position at +0x14, end at +0x18, vtable), a2 = the delimiter byte: pulls characters
// out of the reader until the delimiter, the reader's -1 or a failed push, pushing every other one into the sink and
// counting it, then sets bit 2 (plus 2 when the reader ended) of the flag word when nothing was counted or something
// failed. 08C55FDC first asks whether there is anything to read; its answer at sp+0 decides. The sink's vtable slot
// 0x34 pushes a byte and answers it, the reader's 0x24 underflows and 0x28 reads. v0 = a0.
void func_08C56DDC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 sink = ctx.gpr[5];
    const u32 delimiter = ctx.gpr[6] & 0xFFu;
    u32 flags = 0u;

    put(rt, self + 4u, 0u);
    call(rt, ctx, 0x08C55FDCu, {frame.at(0x00u), self, 1u});
    if (rt.memory().load8(frame.at(0x00u)) == 0u) {
        if (word(rt, self + 4u) == 0u) {
            const u32 sub = self + word(rt, word(rt, self) - 0xCu);
            call(rt, ctx, 0x08C54CFCu, {sub, word(rt, sub + 0x14u) | 4u});
        }
        ctx.gpr[2] = self;
        return;
    }

    const u32 reader = word(rt, self + word(rt, word(rt, self) - 0xCu) + 0x78u);
    u32 character;
    if (word(rt, reader + 8u) < word(rt, reader + 0xCu)) {
        character = rt.memory().load8(word(rt, reader + 8u));
    } else {
        const u32 target = word(rt, word(rt, reader) + 0x24u);
        character = call_virtual(rt, ctx, target, target, {reader});
    }
    // $a1 keeps the sign-extended character the sink's store uses; the reader's second slot leaves it at -1.
    s32 byte_argument = signed_of_byte(character);
    for (;;) {
        if (character == 0xFFFFFFFFu) {
            flags = 2u;
            break;
        }
        if (delimiter == character) {
            flags = 0u;
            break;
        }
        const u32 byte = static_cast<u32>(byte_argument) & 0xFFu;
        const u32 current = word(rt, sink + 0x14u);
        u32 pushed;
        if (current < word(rt, sink + 0x18u)) {
            put8(rt, current, byte);
            put(rt, sink + 0x14u, current + 1u);
            pushed = byte;
        } else {
            const u32 target = word(rt, word(rt, sink) + 0x34u);
            pushed = call_virtual(rt, ctx, target, target, {sink, byte});
        }
        if (pushed == 0xFFFFFFFFu) {
            // The asm answers 2 only when the character itself was the -1 the check above ruled out.
            flags = character == pushed ? 2u : 0u;
            break;
        }
        put(rt, self + 4u, word(rt, self + 4u) + 1u);
        const u32 at = word(rt, reader + 8u);
        if (at < word(rt, reader + 0xCu)) {
            character = rt.memory().load8(at);
            put(rt, reader + 8u, at + 1u);
            byte_argument = signed_of_byte(character);
        } else {
            const u32 target = word(rt, word(rt, reader) + 0x28u);
            character = call_virtual(rt, ctx, target, target, {reader});
            byte_argument = -1;
        }
        if (character == 0xFFFFFFFFu) {
            flags = 2u;
            break;
        }
    }

    if (word(rt, self + 4u) == 0u) {
        const u32 sub = self + word(rt, word(rt, self) - 0xCu);
        call(rt, ctx, 0x08C54CFCu, {sub, word(rt, sub + 0x14u) | 4u});
    } else if (flags != 0u) {
        const u32 sub = self + word(rt, word(rt, self) - 0xCu);
        call(rt, ctx, 0x08C54CFCu, {sub, word(rt, sub + 0x14u) | flags});
    }
    ctx.gpr[2] = self;
}

// --- 0x08C7125C ---
// a0 = object, a1 unused by the body, a2 and a3 = two more arguments for the virtual call: builds a temporary at
// sp+0x20 (08C70314) and, through vtable slot 0x14 (slot 0x18 when the sub-object's flag word masked with 0x4A is
// 0x40 or 8) of the holder at sub+0x80, hands the holder the temporary, the sub-object, its selector byte and a2/a3.
// A non-zero low byte of the answer's v1 sets bit 0 of the sub-object's +0x14 (08C54CFC). The tail clears the
// +0x2000 flag path through 08B62318. v0 = a0.

// --- 0x088D8918 ---
// a0 = an object with a 64-bit clock at +0x94/+0x90, a 64-bit deadline at +0x704/+0x700 and a started byte at
// +0x6F8, a1 = a flag byte: when the object has not started yet it is started, a deadline is armed (now + 5 through
// 08D9C24, its high word 1) unless one already is, and the routine waits for the deadline (08B1A844 ends the wait
// early, 08B2E2B8 is the frame step), then tears the wait state down (+0x6C0/+0x6C4 = -1, the +0x14 stream, the
// +0x6A4 tracker, the reference-counted holder at +0x6B8). v0 = 1 when it fired, 0 when the deadline is still in the
// future or the flag byte was clear.
void func_088D8918(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    if (rt.memory().load8(self + 0x6F8u) != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (call(rt, ctx, 0x088D9E58u, {self}) != 0u) call(rt, ctx, 0x088D6E9Cu, {self});

    const u32 stored_low = word(rt, self + 0x700u);
    const u32 stored_high = word(rt, self + 0x704u);
    bool fire;
    if ((stored_low | stored_high) != 0u) {
        // A deadline is already armed: fire once now has reached it.
        const u32 now_high = word(rt, self + 0x94u);
        if (static_cast<s32>(now_high) < static_cast<s32>(stored_high)) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (static_cast<s32>(now_high) > static_cast<s32>(stored_high)) {
            fire = true;
        } else if (word(rt, self + 0x90u) < stored_low) {
            ctx.gpr[2] = 0u;
            return;
        } else {
            fire = true;
        }
    } else {
        // Arm one: the 64-bit add helper (now + 5) returns the deadline in v0 (low) and v1 (high); both are stored
        // (sw v0, 0x700 / sw v1, 0x704). call() only hands back v0 (the callee runs on a copy of the context), so the
        // high word comes from the GuestResult. A constant 1 as the high word put the deadline ~71 minutes ahead and
        // the wait below never ended: the game hung on the GT logo when leaving a race's Starting Grid (2026-11-26).
        const GuestResult deadline = call_guest(rt, ctx, 0x088D9C24u,
                                                {self, stored_high, word(rt, self + 0x90u), word(rt, self + 0x94u)}, {});
        const u32 low = deadline.v0;
        const u32 high = deadline.v1;
        put(rt, self + 0x700u, low);
        put(rt, self + 0x704u, high);
        if (flag == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 now_high = word(rt, self + 0x94u);
        if (static_cast<s32>(now_high) < static_cast<s32>(high)) {
            fire = false;
        } else if (now_high != high) {
            fire = true;
        } else {
            fire = word(rt, self + 0x90u) >= low;
        }
    }
    if (!fire) {
        // Wait for the deadline one frame at a time; 08B1A844 answering non-zero ends the wait early.
        for (;;) {
            if (call(rt, ctx, 0x08B1A844u) != 0u) break;
            call(rt, ctx, 0x08B2E2B8u, {0x2710u});   // a0 = 10000 (addiu a0, zero, 0x2710 in the branch's delay slot)
            const u32 now_high = word(rt, self + 0x94u);
            const u32 deadline_high = word(rt, self + 0x704u);
            const u32 now_low = word(rt, self + 0x90u);
            const u32 deadline_low = word(rt, self + 0x700u);
            if (static_cast<s32>(now_high) < static_cast<s32>(deadline_high)) continue;
            if (now_high == deadline_high && now_low < deadline_low) continue;
            break;
        }
    }

    call(rt, ctx, 0x088FE6BCu, {word(rt, self + 0x14u)});
    const u32 started = rt.memory().load8(self + 0x6F8u);
    put(rt, self + 0x6C0u, 0xFFFFFFFFu);
    put(rt, self + 0x6C4u, 0xFFFFFFFFu);
    if (started == 0u) {
        put8(rt, self + 0x6F8u, 1u);
        call(rt, ctx, 0x088FE2ECu, {word(rt, self + 0x14u)});
        call(rt, ctx, 0x08B28C74u, {self + 0x6CCu});
        call(rt, ctx, 0x08B1B99Cu, {word(rt, self + 0x6C8u)});
        put(rt, self + 0x6C8u, 0u);
    }
    call(rt, ctx, 0x088FE308u, {word(rt, self + 0x14u)});
    call(rt, ctx, 0x0896275Cu, {self + 0x6A4u});
    const u32 holder = word(rt, self + 0x6B8u);
    put(rt, self + 0x6B4u, 0u);
    put(rt, self + 0x6B8u, 0u);
    if (holder != 0u) {
        // Two reference counts: the vtable's +8 runs when the first reaches zero, its +0xC after the second.
        const u32 first = word(rt, holder + 4u);
        put(rt, holder + 4u, first - 1u);
        if (first == 1u) {
            const u32 target = word(rt, word(rt, holder) + 8u);
            call_virtual(rt, ctx, target, target, {holder});
            const u32 second = word(rt, holder + 8u);
            put(rt, holder + 8u, second - 1u);
            if (second == 1u) {
                const u32 last = word(rt, word(rt, holder) + 0xCu);
                call_virtual(rt, ctx, last, last, {holder});
            }
        }
    }
    put(rt, self + 0x700u, 0u);
    put(rt, self + 0x704u, 0u);
    put(rt, self + 0x64Cu, 0u);
    put(rt, self + 0x650u, 0u);
    ctx.gpr[2] = 1u;
}

// --- 0x0894AC4C ---
// a0 = an object (a target pointer at +8 and a table pointer at +0xC): advances one frame of a four-wheel model.
// Three loops over the four wheels (0x08955300 and 0x089554A8 answer each wheel's two records) accumulate a weighted
// slip, a per-wheel weight and a drive slip, and the trackers at +0x6A4, +0x6B4 and +0x6C4 of the target are moved.
// v0 = the last call's result (the asm never writes it).
void func_0894AC4C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 target = word(rt, self + 8u);
    const u32 table = word(rt, self + 0xCu);
    const float step = call_float(rt, ctx, 0x0895549Cu, {self});

    // Weighted slip over the four wheels.
    float slip = 0.0f;
    for (u32 wheel = 0u; wheel != 4u; ++wheel) {
        const u32 first = call(rt, ctx, 0x08955300u, {self, wheel});
        const u32 second = call(rt, ctx, 0x089554A8u, {self, wheel});
        const float product = psp_mul(float_at(rt, second + 4u), float_at(rt, first + 0x88u));
        const float sum = slip + product;
        slip = sum - float_at(rt, first + 0x48u);
    }
    float tracker = float_at(rt, target + 0x6A4u) + slip;
    tracker = tracker + float_at(rt, target + 0x6A0u);
    put_float(rt, target + 0x6A4u, tracker);

    // Per-wheel weight of the four wheels.
    float weight = 0.0f;
    for (u32 wheel = 0u; wheel != 4u; ++wheel) {
        const u32 record = call(rt, ctx, 0x08955300u, {self, wheel});
        if (rt.memory().load8(record + 0x53u) == 0u) {
            weight = weight + float_at(rt, table + ((wheel >> 1) << 2) + 0x358u);
        }
    }

    const float ratio = 1.0f / (weight + float_at(rt, target + 0x5D8u));
    const float resistance = float_at(rt, target + 0x6B4u) +
                             psp_mul(ratio, float_at(rt, target + 0x6A4u) + float_at(rt, target + 0x684u));
    put_float(rt, target + 0x6B4u, resistance);
    float drive = float_at(rt, target + 0x6C4u) + psp_mul(step, resistance);
    put_float(rt, target + 0x6C4u, drive);
    if (drive == 0.0f) return;  // v0 stays the last call's result

    // Drive slip over the four wheels.
    float drive_slip = 0.0f;
    for (u32 wheel = 0u; wheel != 4u; ++wheel) {
        const u32 first = call(rt, ctx, 0x08955300u, {self, wheel});
        const u32 second = call(rt, ctx, 0x089554A8u, {self, wheel});
        const float with_grip = psp_mul(float_at(rt, second + 4u), float_at(rt, first + 0x8Cu));
        const float against_grip = psp_mul(float_at(rt, second), float_at(rt, first + 0x74u));
        const float sum = drive_slip + with_grip;
        drive_slip = sum - against_grip;
    }

    const float scaled = psp_mul(drive_slip, ratio);
    const float current = float_at(rt, target + 0x6C4u);
    float next = current + psp_mul(step, scaled);
    if (0.0f < psp_mul(current, next)) {
        put_float(rt, target + 0x6A4u, drive_slip + float_at(rt, target + 0x6A4u));
        put_float(rt, target + 0x6B4u, scaled + float_at(rt, target + 0x6B4u));
    } else {
        // The step would reverse the drive: take the loss off the resistance instead and stop the drive.
        put_float(rt, target + 0x6B4u,
                  float_at(rt, target + 0x6B4u) - psp_mul(current, float_at(rt, target + 0x4F8u)));
        next = 0.0f;
    }
    put_float(rt, target + 0x6C4u, next);
}

// --- 0x08A0EBF4 ---
// a0 and a1 unused by the body, a2 = 3 or 4 (how many components), a3 = an array of that many object pointers:
// builds a temporary (08A0EBB4, at sp+4 for three components and sp+0 for four), asserts each pointer non-null and
// reads its float through vtable slot 0x54, then hands the temporary and the components ($f12..$f15, three
// components passing 0 for the fourth) to 08A0E918 and destroys the temporary. Any other count leaves v0 = 4.
void func_08A0EBF4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const u32 count = ctx.gpr[6];
    const u32 pointers = ctx.gpr[7];
    if (count == 3u) {
        const u32 temporary = frame.at(0x04u);
        call(rt, ctx, 0x08A0EBB4u, {temporary});
        const u32 object = word(rt, frame.at(0x04u));
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA95E0u});
        const float first = component(rt, ctx, word(rt, pointers));
        const float second = component(rt, ctx, word(rt, pointers + 4u));
        const float third = component(rt, ctx, word(rt, pointers + 8u));
        callf(rt, ctx, 0x08A0E918u, {object}, {first, second, third, 0.0f});
        call(rt, ctx, 0x08A5B9ECu, {temporary});
        return;
    }
    if (count == 4u) {
        const u32 temporary = frame.at(0x00u);
        call(rt, ctx, 0x08A0EBB4u, {temporary});
        const u32 object = word(rt, frame.at(0x00u));
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA95E0u});
        const float first = component(rt, ctx, word(rt, pointers));
        const float second = component(rt, ctx, word(rt, pointers + 4u));
        const float third = component(rt, ctx, word(rt, pointers + 8u));
        const float fourth = component(rt, ctx, word(rt, pointers + 0xCu));
        callf(rt, ctx, 0x08A0E918u, {object}, {first, second, third, fourth});
        call(rt, ctx, 0x08A5B9ECu, {temporary});
        return;
    }
    ctx.gpr[2] = 4u;
}

// --- 0x08A3C930 ---
// a0 and a1 unused by the body, a2 = 1, 2 or 3 (which of a3's three objects take part), a3 = three object pointers:
// builds a temporary at sp+0xC (08A3A9D0) and takes the code from 08A3A318 (kind 1, with 08A3A320's float) or from
// the slot 0x30 code of a3+4 masked to a byte (kinds 2 and 3, kind 3 also reading the slot 0x54 float of a3+8), then
// calls 08A3A520 with the temporary's object, the word at sp+8 and that code. Two reference-counted temporaries
// (sp+4, sp+8) are released and the temporary destroyed; v0 is never written.

// --- 0x08ACFAF0, 0x08ACFCF8 ---
// a0 = object: its destructor body. Stores the vtable 0x08CD6B38, destroys the two runs of 0xD4-byte elements (from
// self+0x378 down to self+0x2A4 and from self+0xD4 down to self+0xFC) through their vtable's first slot, calls
// 08B28DC4 on self+0xB8 and the two 0x18-byte containers at self+0xAC and self+0x94 (each steps its +4 element
// pointer back, releasing the reference-counted element it names, then frees the lists behind its -8 node's +0x80
// and +0x84), and tail-calls 0x08ABCF2C with a0 = self.
void release_element(Runtime &rt, AllegrexContext &ctx, u32 element) {
    const u32 first = word(rt, element + 4u);
    put(rt, element + 4u, first - 1u);
    if (first != 1u) return;
    const u32 target = word(rt, word(rt, element) + 8u);
    call_virtual(rt, ctx, target, target, {element});
    const u32 second = word(rt, element + 8u);
    put(rt, element + 8u, second - 1u);
    if (second == 1u) {
        const u32 last = word(rt, word(rt, element) + 0xCu);
        call_virtual(rt, ctx, last, last, {element});
    }
}
void destroy_container(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 end = word(rt, address - 4u);
    for (;;) {
        if (word(rt, address + 4u) == end) break;
        const u32 current = word(rt, address + 4u);
        if (current == word(rt, address)) {
            const u32 block = word(rt, current + 0x80u);
            put(rt, address + 4u, block + 0x78u);
            put(rt, address, block);
        } else {
            put(rt, address + 4u, current - 8u);
        }
        const u32 element = word(rt, word(rt, address + 4u) + 4u);
        if (element != 0u) release_element(rt, ctx, element);
    }
    const u32 node = word(rt, address - 8u);
    put(rt, address + 8u, 0u);
    if (node == 0u) return;
    u32 link = word(rt, node + 0x80u);
    while (link != 0u) {
        const u32 next = word(rt, link + 0x80u);
        call(rt, ctx, 0x08B35254u, {link});
        link = next;
    }
    u32 entry = node;
    while (entry != 0u) {
        const u32 next = word(rt, entry + 0x84u);
        call(rt, ctx, 0x08B35254u, {entry});
        entry = next;
    }
}
void destroy_object(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        put(rt, self, 0x08CD6B38u);
        call(rt, ctx, 0x08B1BFACu, {self + 0x5F8u});
        call(rt, ctx, 0x08B1BFACu, {self + 0x528u});
        call(rt, ctx, 0x08AFB0F8u, {self + 0x44Cu});

        // The two runs of 0xD4-byte elements, each destroyed from the top down.
        const u32 stop = self + 0x2A4u;
        if (stop != 0u && stop != self + 0x44Cu) {
            u32 element = self + 0x44Cu - 0xD4u;
            for (;;) {
                const u32 target = word(rt, word(rt, element));
                call_virtual(rt, ctx, target, target, {element});
                if (element == stop) break;
                element -= 0xD4u;
            }
        }
        const u32 start = self + 0xFCu;
        if (start != 0u && start + 0x1A8u != start) {
            u32 element = start + 0x1A8u - 0xD4u;
            for (;;) {
                const u32 target = word(rt, word(rt, element));
                call_virtual(rt, ctx, target, target, {element});
                if (element == start) break;
                element -= 0xD4u;
            }
        }
        call(rt, ctx, 0x08B28DC4u, {self + 0xB8u});

        // The two 0x18-byte containers, self+0xAC and self+0x94.
        const u32 marker_end = self + 0x88u;
        if (marker_end != 0u && marker_end != self + 0xB8u) {
            u32 marker = self + 0xB8u;
            u32 container = self + 0xACu;
            for (;;) {
                marker -= 0x18u;  // the asm steps it before looking at the container
                destroy_container(rt, ctx, container);
                if (marker == marker_end) break;
                container -= 0x18u;
            }
        }
    }
    tail_call(ctx, 0x08ABCF2Cu, {self});
}

// --- 0x08AF8CAC ---
// a0 = an object (a byte count at +0x12, a float at +0 and a pointer at +4): quantises the count into a band. The
// count is clamped down to the global limit at 0x08CE0A04 when that is not negative, the object's float is scaled by
// 08B5420C's answer, the global float at 0x08CE0A1C and the ratio at 0x08D4726C (a ratio above 0.99 short-circuits
// the band to 1), the band is clamped to [0, 1], and the answer is floor(band * (count - 1)) plus a half-step carried
// through the global at 0x08CE0A18 and clamped to [0, 1]. Answers in $f0; v0 is never written.
void func_08AF8CAC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    s32 count = static_cast<s32>(rt.memory().load8(self + 0x12u));
    const s32 limit = static_cast<s32>(word(rt, 0x08CE0A04u));
    if (limit >= 0) {
        // `movz` picks the local byte when the limit is below it and the global's word otherwise.
        put(rt, frame.at(0x1Cu), static_cast<u32>(count));
        count = static_cast<s32>(word(rt, limit < count ? frame.at(0x1Cu) : 0x08CE0A04u));
    }
    if (count <= 0) {
        ctx.fpr[0] = 0.0f;
        return;
    }
    const float scaled = call_float(rt, ctx, 0x08B5420Cu, {self + 4u, word(rt, 0x08CE0A08u)});
    const float value = psp_mul(psp_mul(scaled, float_at(rt, self)), float_at(rt, 0x08CE0A1Cu));
    float band = 0.0f < value ? value : 0.0f;
    if (!(band < 1.0f)) band = 1.0f;
    const float ratio = float_at(rt, 0x08D4726Cu);
    float fraction;
    if (0.99f < ratio) {
        fraction = 1.0f;
    } else {
        // pow(2, global) then pow(band, that), mapped through the ratio
        const float exponent = call_float(rt, ctx, 0x08B67720u, {}, {2.0f, float_at(rt, 0x08D47270u)});
        const float powered = call_float(rt, ctx, 0x08B67720u, {}, {band, exponent});
        fraction = (powered - ratio) / (1.0f - ratio);
    }
    float clamped = 0.0f < fraction ? fraction : 0.0f;
    if (!(clamped < 1.0f)) clamped = 1.0f;
    const float position = psp_mul(1.0f - clamped, static_cast<float>(count - 1));
    const float floored = call_float(rt, ctx, 0x08B663B4u, {}, {position});
    float step = position - floored;
    step = step - 0.5f;
    step = step / float_at(rt, 0x08CE0A18u);
    step = step + 0.5f;
    float tail = 0.0f < step ? step : 0.0f;
    if (!(tail < 1.0f)) tail = 1.0f;
    ctx.fpr[0] = floored + tail;
}

// --- 0x08B05B1C ---
// a0 = an object (elements at +0, count at +0x3C, a table of 0x18-byte entries at +0xC): for each 0x70-byte element
// it reads the element's two coverage spans through 08BBBEC4 (an entry index packed in bits 12..21 of the word at
// +0x6C and a second one in the top ten bits), clamps what is left of the span to the element's limit at +0x68 (or
// to the symmetric limit of the neighbour the low twelve bits name), stores it at +0x60 and hands the remaining span
// to 08B057F0 for the element and, unless the top index is 0x3FF, its +0x18 sub-object. v0 is never written.
void func_08B05B1C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 base = word(rt, self);
    const u32 count = word(rt, self + 0x3Cu);
    const u32 end = base + ((count << 7) - (count << 4));  // 0x70 bytes each
    for (u32 element = base; element != end; element += 0x70u) {
        const u32 flags = word(rt, element + 0x6Cu);
        const u32 table = word(rt, self + 0x0Cu);
        const u32 index = extract_bits(flags, 12, 10);  // `ext`: bits 12..21
        const u32 entry = table + ((index << 5) - (index << 3));
        const u32 flags_address = element + 0x6Cu;  // the asm reads the element's fields off this word
        const float first = float_at(rt, flags_address - 0xCu);
        float span = first - float_at(rt, flags_address - 8u);
        const float low = call_float(rt, ctx, 0x08BBBEC4u, {element + 0x30u, entry});
        const float high = call_float(rt, ctx, 0x08BBBEC4u, {element + 0x3Cu, entry + 0x0Cu});
        span = span - (low + high);
        const bool has_second = (flags & 0xFFC00000u) != 0xFFC00000u;
        if (has_second) {
            const u32 second = flags >> 22;
            const u32 other = table + ((second << 5) - (second << 3));
            const float other_low = call_float(rt, ctx, 0x08BBBEC4u, {element + 0x48u, other});
            const float other_high = call_float(rt, ctx, 0x08BBBEC4u, {element + 0x54u, other + 0x0Cu});
            span = span - (other_low + other_high);
        }
        float value;
        if ((flags & 0xFFFu) == 0xFFFu) {
            const float limit = float_at(rt, flags_address - 4u);
            value = span < limit ? limit : span;
        } else {
            const u32 neighbour = base + 0x70u * (flags & 0xFFFu);
            const float limit = psp_mul(float_at(rt, neighbour + 0x60u), float_at(rt, flags_address - 4u));
            value = span < -limit ? -limit : span;
            if (!(value < limit)) value = limit;
        }
        put_float(rt, element + 0x60u, value);
        const float remaining = value - first;
        callf(rt, ctx, 0x08B057F0u, {entry, element}, {remaining});
        if (has_second) {
            const u32 second = flags >> 22;
            callf(rt, ctx, 0x08B057F0u, {table + ((second << 5) - (second << 3)), element + 0x18u}, {remaining});
        }
    }
}

// --- 0x08C454D0 ---
// No arguments: the guard chain of one group of static objects. Each guard byte (0x08D74F78, 0x08D74F98,
// 0x08D74FA8, 0x08D74FC8, 0x08D74FA0, 0x08D74F90, 0x08D74F70) is checked, 08B6441C acquires it, the object at
// 0x08D7729C / 0x08D772AC / 0x08D772D8 / 0x08D77300 gets its +0/+4/+8 words and 08B64494 releases the guard, and the
// objects are linked into their parents' +4 words. v0 = 0x08D77300.
void guard_fa0(Runtime &rt, AllegrexContext &ctx, u32 guard_argument) {
    if (rt.memory().load8(0x08D74FA0u) == 0u && call(rt, ctx, 0x08B6441Cu, {guard_argument}) != 0u) {
        put(rt, 0x08D772B4u, 0x08CD7988u);
        put(rt, 0x08D772B0u, 0x08D772B8u);
        put(rt, 0x08D772ACu, 0u);
        call(rt, ctx, 0x08B64494u, {0x08D74FA0u});
    }
    put(rt, 0x08D772E4u, 0x08D772ACu);
    call(rt, ctx, 0x08B64494u, {0x08D74F98u});
}
void guard_f90(Runtime &rt, AllegrexContext &ctx, u32 guard_argument) {
    if (rt.memory().load8(0x08D74F90u) == 0u && call(rt, ctx, 0x08B6441Cu, {guard_argument}) != 0u) {
        put(rt, 0x08D772E0u, 0x08CD7994u);
        put(rt, 0x08D772DCu, 0x08D772E4u);
        put(rt, 0x08D772D8u, 0u);
        call(rt, ctx, 0x08B64494u, {0x08D74F90u});
    }
}
void guard_f70(Runtime &rt, AllegrexContext &ctx, u32 guard_argument) {
    if (rt.memory().load8(0x08D74F70u) == 0u && call(rt, ctx, 0x08B6441Cu, {guard_argument}) != 0u) {
        put(rt, 0x08D77308u, 0x08CD799Cu);
        put(rt, 0x08D77304u, 0x08D7730Cu);
        put(rt, 0x08D77300u, 0u);
        call(rt, ctx, 0x08B64494u, {0x08D74F70u});
    }
    ctx.gpr[2] = 0x08D77300u;
}
void func_08C454D0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    if (rt.memory().load8(0x08D74F78u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08D74F78u}) != 0u) {
        if (rt.memory().load8(0x08D74F98u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08D74F98u}) != 0u) {
            if (rt.memory().load8(0x08D74FA8u) == 0u) {
                if (call(rt, ctx, 0x08B6441Cu, {0x08D74FA8u}) != 0u) {
                    if (rt.memory().load8(0x08D74FC8u) == 0u &&
                        call(rt, ctx, 0x08B6441Cu, {0x08D74FC8u}) != 0u) {
                        put(rt, 0x08D7729Cu + 8u, 0x08CD7988u);
                        put(rt, 0x08D7729Cu + 4u, 0x08D772A8u);
                        put(rt, 0x08D7729Cu, 0u);
                        call(rt, ctx, 0x08B64494u, {0x08D74FC8u});
                    }
                    put(rt, 0x08D772B8u, 0x08D7729Cu);
                    call(rt, ctx, 0x08B64494u, {0x08D74FA8u});
                    guard_fa0(rt, ctx, 0x08D74FA0u);
                } else {
                    // The asm's skip branch lands on the next guard's load, so a0 still holds the pair's %hi half.
                    guard_fa0(rt, ctx, lui_of(0x08D74FA0u));
                }
            } else {
                guard_fa0(rt, ctx, 0x08D74FA0u);
            }
            guard_f90(rt, ctx, 0x08D74F90u);
        } else {
            guard_f90(rt, ctx, lui_of(0x08D74F90u));
        }
        put(rt, 0x08D7730Cu, 0x08D772D8u);
        call(rt, ctx, 0x08B64494u, {0x08D74F78u});
        guard_f70(rt, ctx, 0x08D74F70u);
        return;
    }
    // The guard was already set (or refused): straight to the last object, with the %hi half left in a0.
    guard_f70(rt, ctx, lui_of(0x08D74F78u));
}

// --- 0x08C4D2E4 ---
// a0 = a typed value (type tag at +0, data pointer at +4), a1 = the value byte: the setter sibling of 0x08C4E244.
// The tag selects which descriptor's store runs — a float made from the byte, a 64-bit value through 08B71CE0
// (08B71328 when the byte is negative), a byte, a halfword, a word, a word pair, or a byte again. v0 = 1 when
// something was stored, 0 when no type matched.
void func_08C4D2E4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5] & 0xFFu;
    const s32 signed_value = signed_of_byte(value);
    u32 tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08B9DDB0u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put_float(rt, data, static_cast<float>(value));
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08C4D18Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            callf(rt, ctx, 0x08B71CE0u, {static_cast<u32>(signed_value)}, {});
            u32 low = ctx.gpr[2];
            u32 high = ctx.gpr[3];
            if (signed_value < 0) {
                callf(rt, ctx, 0x08B71328u, {low, high, 0u, 0x40700000u}, {});
                low = ctx.gpr[2];
                high = ctx.gpr[3];
            }
            put(rt, data, low);
            put(rt, data + 4u, high);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC473Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put8(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC48B4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put8(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC45C4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put16(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC444Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put16(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC42D4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08B9E8D4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC415Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, data, value);
            put(rt, data + 4u, 0u);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC3FE4u)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put(rt, data, value);
            put(rt, data + 4u, 0u);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08B9CEBCu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put8(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    tag = word(rt, self);
    if (tag == call(rt, ctx, 0x08BC4A2Cu)) {
        const u32 data = word(rt, self + 4u);
        if (data != 0u) {
            put8(rt, data, value);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    ctx.gpr[2] = 0u;
}

} // namespace
} // namespace new_batch_main_21b
} // namespace gt::game

#define MAIN21B(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_21b::name, gt::ReturnKind::kind)
MAIN21B(0x08C3C898u, func_08C3C898, Word);
MAIN21B(0x08C4E244u, func_08C4E244, Word);
MAIN21B(0x08C56DDCu, func_08C56DDC, Word);
MAIN21B(0x088D8918u, func_088D8918, Word);
MAIN21B(0x0894AC4Cu, func_0894AC4C, Word);
MAIN21B(0x08A0EBF4u, func_08A0EBF4, Void);
MAIN21B(0x08AF8CACu, func_08AF8CAC, Float);
MAIN21B(0x08B05B1Cu, func_08B05B1C, Void);
MAIN21B(0x08C454D0u, func_08C454D0, Word);
MAIN21B(0x08C4D2E4u, func_08C4D2E4, Word);
