// New batch 15: small functions written by hand from `tools.disasm` (relocations applied, branches resolved). Delay slots are
// read as "before the branch / call"; the callee-saved registers the original keeps values in are set where the asm sets them
// (a callee may read them) and put back at the end; `v0` ends as the asm leaves it on every exit path. Guest memory goes
// through word()/put()/rt.memory(), calls through call()/call_guest with every argument register the callee gets, and a
// function that ends with `j target` pops its frame and finishes with tail_call(). No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_15 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call whose result the caller reads in both `v0` and `v1`.
GuestResult call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result;
}
// A guest call with float arguments ($f12, $f13, ... in order).
u32 call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
               std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// s0-s7 are callee-saved: the body sets them like the original and the caller's values come back.
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

// --- 0x08ACB548: hands a value to a member, resets a counter and flags a byte, then finishes on the member ------------------

// --- 0x08AD2108: rotates two bytes of every entry of a 124-byte-strided chain of a table --------------------------------------

// --- 0x08ADC27C: looks an object up by a name, then runs a member while the object still has entries ------------------------
void func_08ADC27C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const bool nonzero = ctx.gpr[5] != 0u;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = nonzero ? 1u : 0u;
    const u32 found = call_set(rt, ctx, 0x08ADB278u, {object, 0x08C90EA4u});
    ctx.gpr[17] = found;
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.fpr[12] = 0.0f;  // `mtc1 zero, f12`
    for (;;) {
        const s32 remaining = static_cast<s32>(word(rt, found + 0x198u));
        ctx.gpr[2] = remaining > 0 ? 1u : 0u;
        if (!nonzero || remaining <= 0) return;
        call_set(rt, ctx, 0x08AD96D8u, {object});
    }
}

// --- 0x08AEC1A4: tears down the sound state block when it is up ---------------------------------------------------------------

// --- 0x08B2E98C: creates a singleton once (`0x08D690F8` keeps the handle) -----------------------------------------------------
void func_08B2E98C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 existing = word(rt, 0x08D690F8u);
    ctx.gpr[2] = existing;
    if (existing != 0u) return;
    rt.memory().store8(0x08D690F4u, 1u);
    put(rt, frame.at(0u), 0x08B2EAC8u);
    put(rt, frame.at(4u), 0u);
    const u32 first = call_set(rt, ctx, 0x08B2E314u, {3u, 0u, 1u});
    const u32 second = call_set(rt, ctx, 0x08B2E134u, {first});
    const u32 created = call_set(rt, ctx, 0x08B1B96Cu, {frame.at(0u), second, 0u});
    put(rt, 0x08D690F8u, created);
}

// --- 0x08B32460: resets a handle field, drains a queued value -----------------------------------------------------------------

// --- 0x08B3252C: shuts down three subsystems when the flag byte is set --------------------------------------------------------

// --- 0x08B48CFC: a name string chosen by a state word -------------------------------------------------------------------------
void func_08B48CFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 state = word(rt, ctx.gpr[4] + 0xCu);
    if (state == 1u) {
        ctx.gpr[2] = 0x08CB56B4u;
    } else if (state == 0u) {
        ctx.gpr[2] = 0x08CB56B0u;
    } else if (state == 2u) {
        ctx.gpr[2] = 0x08CB56BCu;
    } else if (state == 3u) {
        ctx.gpr[2] = 0x08CB56A8u;
    } else {
        ctx.gpr[2] = 0x08CB5694u;
    }
}

// --- 0x08BAC354 and 0x08C4B0D0: copies 0x70-byte blocks, the second in 16-byte chunks (same shape, different code) ------------
u32 copy_blocks_of_0x70_in_16_byte_chunks(Runtime &rt, u32 source, u32 limit, u32 destination) {
    if (source == limit) return destination;
    u32 source_cursor = source;
    u32 destination_cursor = destination;
    for (;;) {
        if (destination_cursor != 0u) {
            for (u32 offset = 0u; offset < 0x70u; offset += 0x10u) {
                u32 chunk[4];
                for (u32 word_index = 0u; word_index < 4u; ++word_index)
                    chunk[word_index] = word(rt, source_cursor + offset + 4u * word_index);
                for (u32 word_index = 0u; word_index < 4u; ++word_index)
                    put(rt, destination_cursor + offset + 4u * word_index, chunk[word_index]);
            }
        }
        source_cursor += 0x70u;
        const bool done = source_cursor == limit;
        destination_cursor += 0x70u;
        if (done) return destination_cursor;
    }
}

void func_08C4B3C0(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = copy_blocks_of_0x70_in_16_byte_chunks(rt, ctx.gpr[4], ctx.gpr[5], ctx.gpr[6]);
}

// --- 0x08BAC354: the same copy, 0x20 bytes per step ---------------------------------------------------------------------------
void func_08BAC354(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[4];
    const u32 limit = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (source != limit) {
        u32 source_cursor = source;
        for (;;) {
            u32 fields[7];
            if (destination != 0u) {
                for (u32 offset = 0u; offset < 0x1Cu; offset += 4u) fields[offset / 4u] = word(rt, source_cursor + offset);
                for (u32 offset = 0u; offset < 0x1Cu; offset += 4u) put(rt, destination + offset, fields[offset / 4u]);
                rt.memory().store8(destination + 0x1Cu, rt.memory().load8(source_cursor + 0x1Cu));
            }
            source_cursor += 0x20u;
            destination += 0x20u;
            if (source_cursor == limit) break;
        }
    }
    ctx.gpr[2] = destination;
}

// --- 0x08BC1170 and 0x08BC1240: moves a reference from one slot to another through a virtual ---------------------------------
// (a0 = unused object, a1 = destination slot, a2 = holder of the source slot). The source is opened with the virtual at +0xC,
// the value moved into the destination, and the held object the destination had is released through its virtual at +4.
void move_reference(Runtime &rt, AllegrexContext &ctx) {
    u32 previous;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = ctx.gpr[5];
        const u32 source = word(rt, ctx.gpr[6]);
        ctx.gpr[4] = source;
        ctx.gpr[5] = 0u;  // the `beqz` delay slot
        u32 value = 0u;
        if (source != 0u) {
            value = call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {source});
            ctx.gpr[5] = value;
        }
        previous = word(rt, ctx.gpr[16]);
        put(rt, ctx.gpr[16], value);
        ctx.gpr[2] = previous;
        ctx.gpr[4] = previous;
        if (previous == 0u) return;
        ctx.gpr[3] = previous;
    }
    tail_call(ctx, word(rt, word(rt, previous) + 4u), {previous});
}


// --- 0x0881AF30: the index of the first member whose two words are both -1, or 0x1E -----------------------------
// a0 = a base object, a1 ignored: member i sits at base + 0x354 + 8*i and is handed to 0x088A5EE0, which answers in v0/v1.
void func_0881AF30(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    u32 cursor = ctx.gpr[4] + 0x354u;
    ctx.gpr[16] = cursor;
    ctx.gpr[18] = 0xFFFFFFFFu;
    ctx.gpr[19] = 0x1Eu;
    u32 index = 0u;
    ctx.gpr[17] = index;
    for (;;) {
        const u32 member = cursor;
        cursor += 8u;  // the `jal` delay slot advances before the call
        ctx.gpr[16] = cursor;
        const GuestResult found = call_pair(rt, ctx, 0x088A5EE0u, {member});
        if (found.v0 == 0xFFFFFFFFu && found.v1 == 0xFFFFFFFFu) break;
        ++index;
        ctx.gpr[17] = index;
        if (index == 0x1Eu) break;
    }
    ctx.gpr[2] = index;
}

// --- 0x088390FC and 0x08A1D6B8: releases the reference at +0x14 through its virtual, resets the base, then frees ------------
void destroy_with_vtable(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x14u;
        put(rt, self, vtable);
        ctx.gpr[2] = vtable;
        const u32 held = word(rt, self + 0x18u);
        ctx.gpr[2] = held;
        if (held != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, held)), {held, word(rt, self + 0x14u)});
            put(rt, self + 0x18u, 0u);
            put(rt, self + 0x14u, 0u);
        }
        call_set(rt, ctx, 0x08A780B0u, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x1Cu, 4u});
}
void func_088390FC(Runtime &rt, AllegrexContext &ctx) { destroy_with_vtable(rt, ctx, 0x08CBDAB8u); }

// --- 0x08879FC8: looks a component up by name and frees it, then finishes with the object and the argument ------------------

// --- 0x0887E908: two base-class initialisers, a released reference at +0x70, then the object's teardown ---------------------

// --- 0x088DD870: how many of the first N entries have a type word of 1 and a field at +0x1FC that is not below -1 -----------
void func_088DD870(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 count = call_set(rt, ctx, 0x088D9F80u, {self});
    u32 matched = 0u;
    u32 offset = 0u;
    for (s32 index = 0; index < static_cast<s32>(count); ++index) {
        const u32 entry = word(rt, self + 0xC0u) + offset;
        if (word(rt, entry + 4u) == 1u && static_cast<s32>(word(rt, entry + 0x1FCu)) >= -1) ++matched;
        offset += 0x2F8u;
    }
    ctx.gpr[2] = matched;
}

// --- 0x088E6A9C: stores two words in the object a member lookup returns, then finishes on the member ----------------------

// --- 0x088E6F94: the same lookup, returning the signed halfword at +0x1C of what it found (0 when nothing) ------------------

// --- 0x088F1CC0: two vtable words around a released reference at +8 -----------------------------------------------------------
void func_088F1CC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 8u;
        put(rt, self, 0x08CC3B48u);
        call_set(rt, ctx, 0x088F1C98u, {self});
        const u32 held = word(rt, self + 0xCu);
        ctx.gpr[2] = held;
        if (held != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, held)), {held, word(rt, self + 8u)});
            put(rt, self + 0xCu, 0u);
            put(rt, self + 8u, 0u);
        }
        put(rt, self, 0x08CBF518u);
        ctx.gpr[2] = 0x08CBF518u;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// --- 0x08909928: calls a sink for each of up to three members, the last one as a tail call ---------------------------------
void func_08909928(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    u32 last;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        call_set(rt, ctx, 0x08AF12A4u, {word(rt, self + 4u), argument});
        const u32 second = word(rt, self + 8u);
        ctx.gpr[2] = second;
        if (second != 0u) call_set(rt, ctx, 0x08AF12A4u, {second, argument});
        last = word(rt, self + 0xCu);
        ctx.gpr[2] = last;
        if (last == 0u) return;
    }
    tail_call(ctx, 0x08AF12A4u, {last, argument});
}

// --- 0x0886F268: two base-class initialisers, a released reference at +0x70, then the teardown --------------------------------

// --- 0x088741E4: releases the reference at +0xC, then the second vtable word and the teardown --------------------------------

// --- 0x088757EC: resolves two names in turn and releases the result ----------------------------------------------------------
void func_088757EC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    u32 found;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        found = call_set(rt, ctx, 0x088756F0u, {self, 0u, name});
        ctx.gpr[16] = found;
        if (found == 0u) return;
        if (call_set(rt, ctx, 0x0886A950u, {found, 0x08C9FF94u}) != 0u) return;
        if (call_set(rt, ctx, 0x0886A950u, {found, 0x08C9FF90u}) != 0u) return;
    }
    tail_call(ctx, 0x0886AB7Cu, {found});
}

// --- 0x0887F9E0: passes a float on to four members, the last one as a tail call ----------------------------------------------

// --- 0x088A24D0: swaps the placeholders of two slots through a table at +0xC -------------------------------------------------

// --- 0x088ECF3C: flags an offset's byte as consumed and clears it -------------------------------------------------------------

// --- 0x088EF034: releases two members, then the tail at +8 ---------------------------------------------------------------------
void func_088EF034(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 8u;
        call_set(rt, ctx, 0x08B344E0u, {self + 8u});
        const u32 second = word(rt, self + 4u);
        ctx.gpr[2] = second;
        if (second != 0u) call_set(rt, ctx, 0x08B61FC0u, {second});
        const u32 first = word(rt, self);
        ctx.gpr[2] = first;
        put(rt, self + 4u, 0u);
        if (first != 0u) call_indirect(rt, ctx, word(rt, word(rt, first) + 4u), {first});
        call_set(rt, ctx, 0x08B3457Cu, {self + 8u});
    }
    tail_call(ctx, 0x08B28DC4u, {self + 8u});
}

// --- 0x08947A30: a float member of the display list, by the object's type byte (returns in f0) --------------------------------
void func_08947A30(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.fpr[0] = std::bit_cast<float>(0x41598A02u);
    if (rt.memory().load8(self) == 7u) {
        call_set(rt, ctx, 0x089475F0u, {self});
        ctx.gpr[2] = 0x08CF0000u;  // the relocated `lui` base of the two constants below
        const float doubled = float_at(rt, 0x08CF2EA8u) + float_at(rt, 0x08CF2EA8u);
        ctx.fpr[0] = (doubled + float_at(rt, 0x08CF2EA4u)) / std::bit_cast<float>(0x3F3C49BAu);
    } else if (rt.memory().load8(self + 0x12u) != 4u) {
        ctx.fpr[0] = 0.0f;
    }
}

// --- 0x089BF71C: a member's virtual, between two locks of the block at +0x2B0 -------------------------------------------------

// --- 0x089E63F0: three calls on a temporary, then two state words ----------------------------------------------------------

// --- 0x08A39B70: locks a block, clears three members of the object the accessor answers --------------------------------------

// --- 0x08A5A28C and 0x08AAC738: stores a virtual's byte answer at the entry a property bag holds -------------------------------
// (a1 = the bag: its word +0x1C, or +0x18 and then +0xC, is the target; a2 = holder whose object's virtual is asked).
void store_byte_from_virtual(Runtime &rt, AllegrexContext &ctx, u32 member_offset, u32 assert_message) {
    const u32 bag = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = holder;
    u32 target = word(rt, bag + 0x1Cu);
    ctx.gpr[16] = target;
    if (target == 0u) {
        target = word(rt, bag + 0x18u);
        ctx.gpr[16] = target;
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {assert_message});
        target = word(rt, target + 0xCu);
        ctx.gpr[16] = target;
    }
    const u32 object = word(rt, holder);
    ctx.gpr[2] = object;
    if (object != 0u) {
        const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + member_offset), {object});
        rt.memory().store8(target, static_cast<u8>(value));
    }
}

// --- 0x08AAD510: tears down 26 sub-objects of a block at +0x68 ----------------------------------------------------------------

// --- 0x08AC0584: fills a cached id, and answers whether the cached id is nonnegative -----------------------------------------
void func_08AC0584(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 object = word(rt, self + 0x18u);
    const u32 field = word(rt, word(rt, object + 0x14u) + 0xCu);
    if (flag != 0u) {
        put(rt, self + 0x1Cu, call_set(rt, ctx, 0x08B1554Cu, {word(rt, object), first, field}));
    } else {
        put(rt, frame.at(0u), first);
        put(rt, frame.at(4u), field);
        put(rt, self + 0x1Cu, call_set(rt, ctx, 0x08B0C650u, {word(rt, object), frame.at(0u)}));
    }
    ctx.gpr[2] = static_cast<s32>(word(rt, self + 0x1Cu)) >= 0 ? 1u : 0u;
}

// --- 0x08AC38AC: releases the block at +0x18C, after a teardown the flag byte at +0x1B4 asks for -------------------------------
void func_08AC38AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 member = self + 0x18Cu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = member;
    ctx.gpr[18] = 0xFFFFFFFFu;
    u32 result = 0xFFFFFFFFu;
    call_set(rt, ctx, 0x08B344E0u, {member});
    if (rt.memory().load8(self + 0x1B4u) != 0u) {
        call_set(rt, ctx, 0x08AC381Cu, {self});
        result = call_set(rt, ctx, 0x08AC2DECu, {self});
    }
    call_set(rt, ctx, 0x08B3457Cu, {member});
    ctx.gpr[2] = result;
}

// --- 0x08AC77B0: clears a flag when a float member is zero, then drops the held block at +0x56C --------------------------------

// --- 0x08ACE96C: builds a rendering object and keeps it at +0xE4 ---------------------------------------------------------------

// --- 0x08B0D510 and 0x08B0D790: sums (a virtual's 64-bit size >> 10) over a linked list --------------------------------------
// The virtual answers in v0:v1 (a 64-bit byte count); the page count is its value + 0x3FF shifted right by 10.
u32 sum_pages_of_list(Runtime &rt, AllegrexContext &ctx, u32 member_offset) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    u32 node = word(rt, 0x08D7734Cu);
    u32 total = 0u;
    while (node != 0u) {
        const u32 object = word(rt, node + 4u);
        const u32 size = call_indirect(rt, ctx, word(rt, word(rt, object) + member_offset), {object});
        const u32 high = ctx.gpr[3];
        const u32 rounded = size + 0x3FFu;
        const u32 carry = rounded < size ? 1u : 0u;
        node = word(rt, node);
        total += ((carry + high) << 22) | (rounded >> 10);
    }
    return total;
}

// --- 0x08B10CC4 and 0x08B11434: locks two blocks, copies one member pair across, then the same teardown (0x58 vs 0x4C) --------
void relink_members(Runtime &rt, AllegrexContext &ctx, u32 member_offset) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[16] = other;
        ctx.gpr[17] = self;
        ctx.gpr[18] = other + 4u;
        ctx.gpr[19] = self + 0x14u;
        call_set(rt, ctx, 0x08B34888u, {self + 0x14u});
        call_set(rt, ctx, 0x08B34888u, {other + 4u});
        call_set(rt, ctx, 0x08B1FCD4u, {self + member_offset, other + 0x3Cu});
        put(rt, other + 0xA4u, 0u);
        call_set(rt, ctx, 0x08B34B24u, {other + 4u});
    }
    tail_call(ctx, 0x08B34B24u, {self + 0x14u});
}

// --- 0x08B33688: creates a subsystem once (the handle sits at 0x08D69208) and starts it ---------------------------------------
void func_08B33688(Runtime &rt, AllegrexContext &ctx) {
    u32 created;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        const s32 existing = static_cast<s32>(word(rt, 0x08D69208u));
        ctx.gpr[2] = static_cast<u32>(existing);
        if (existing > 0) return;
        const u32 handle = call_set(rt, ctx, 0x08C7B118u);
        put(rt, 0x08D69204u, handle);
        created = call_set(rt, ctx, 0x08B2ECACu, {0x08CB54DCu, 0x08B33624u, 0u});
        put(rt, 0x08D69208u, created);
    }
    tail_call(ctx, 0x08C7B130u, {created});
}

// --- 0x08B34060: the same for the state at 0x08D69274, plus a registration call -----------------------------------------------
void func_08B34060(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const s32 existing = static_cast<s32>(word(rt, 0x08D69274u));
    ctx.gpr[2] = static_cast<u32>(existing);
    if (existing > 0) return;
    const u32 created = call_set(rt, ctx, 0x08B2ECACu, {0x08CB54F0u, 0x08B34270u, 0u});
    put(rt, frame.at(0u), created);
    put(rt, 0x08D69274u, created);
    call_set(rt, ctx, 0x08C7A820u, {0x08CB54FCu, 0x02415821u, frame.at(0u), 4u, 0u, 0u});
}

// --- 0x08B40080: three handler registrations on the object held at +0xB0, the last one as a tail call -------------------------

// --- 0x08B56270: scales the three columns of a 3x4 matrix by three floats and sets its dirty words -----------------------------

// --- 0x08C70078: when a virtual's answer differs from the expected one, retries through a flag -------------------------------
void func_08C70078(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 expected = ctx.gpr[6];
    u32 object;
    u32 argument;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = expected;
        ctx.gpr[17] = self;
        object = self + word(rt, word(rt, self) - 0xCu);
        const u32 member = word(rt, object + 0x78u);
        ctx.gpr[4] = member;
        if (call_indirect(rt, ctx, word(rt, word(rt, member) + 0x30u), {member}) == expected) return;
        object = self + word(rt, word(rt, self) - 0xCu);
        argument = word(rt, object + 0x14u) | 1u;
        ctx.gpr[4] = object;
    }
    tail_call(ctx, 0x08C54CFCu, {object, argument});
}

// --- 0x08C73368: a setup call, a virtual and a member call, answering a0 ------------------------------------------------------
void func_08C73368(Runtime &rt, AllegrexContext &ctx) {
    const u32 answer = ctx.gpr[4];
    const u32 self = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = argument;
    ctx.gpr[18] = self + 0x1Cu;
    ctx.gpr[19] = answer;
    call_set(rt, ctx, 0x08B5C450u, {answer, self + 0x1Cu});
    call_indirect(rt, ctx, word(rt, word(rt, self) + 8u), {self, argument});
    call_set(rt, ctx, 0x08B5D1E0u, {self + 0x1Cu, argument});
    ctx.gpr[2] = answer;
}

// --- 0x08927278: calls the virtual at +0x10 of 13 objects in a row, then parks the counter at -1 -------------------------------

// --- 0x089BFB70: tears down a member it may still hold, between two locks of the block at +0x2B0 ------------------------------

// --- 0x089C12F4: hands an argument to the member at +0x230, then lets the held object see +0x22C -------------------------------

// --- 0x089D8474: stores a float at +0x1C of the entry a virtual answers ------------------------------------------------------

// --- 0x089DA370: whether the entry a virtual answers has a nonzero word at +8 (or, reversed, is empty) --------------------------

// --- 0x08A2B5DC: drains a subsystem into a temporary and clears the flag byte the accessor reports ------------------------------
void func_08A2B5DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    ctx.gpr[16] = self;
    const u32 first = call_set(rt, ctx, 0x08B0979Cu, {self});
    const u32 second = call_set(rt, ctx, 0x08B09A04u, {first});
    if (rt.memory().load8(second + 4u) != 0u) call_set(rt, ctx, 0x08B3027Cu, {second});
    const u32 value = call_set(rt, ctx, 0x08B327ACu, {0x08D69170u, 0u});
    call_set(rt, ctx, 0x08A97A6Cu, {frame.at(0u), value});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- 0x08A4D508: creates the object kept at 0x08CF68E8 once, then finishes on it with a name -----------------------------------
void func_08A4D508(Runtime &rt, AllegrexContext &ctx) {
    u32 object;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        const u32 existing = word(rt, 0x08CF68E8u);
        ctx.gpr[2] = existing;
        if (existing != 0u) return;
        object = call_set(rt, ctx, 0x08B62344u, {0xA8u});
        ctx.gpr[16] = object;
        call_set(rt, ctx, 0x08A4D420u, {object});
        put(rt, 0x08CF68E8u, object);
        call_set(rt, ctx, 0x08A4CD1Cu, {object});
    }
    tail_call(ctx, 0x08A4DDB8u, {object, 0x08CAF14Cu});
}

// --- 0x08A60E5C: resets a list head at +0x18 and links it to itself -----------------------------------------------------------

// --- 0x08A6576C: builds an entry with a type byte of 0xB and two member blocks, the second one as a tail call --------------------
void func_08A6576C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second_argument = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = first;
        ctx.gpr[18] = second;
        rt.memory().store8(self + 4u, 0xBu);
        put(rt, self, 0x08CD2A48u);
        put(rt, self + 4u, word(rt, self + 4u) & 0xFFu);
        call_set(rt, ctx, 0x08A75D04u, {self + 8u, second_argument});
        call_set(rt, ctx, 0x08A6972Cu, {self + 0x14u, first});
    }
    tail_call(ctx, 0x08A6972Cu, {self + 0x18u, second});
}

// --- a family of wrappers around the property bag -----------------------------------------------------------------------------
// a0 = destination, the holder is a1 (or a2 in the 08A733AC/08A96120/08AB* ones): the holder's object is asked through its
// virtual at `member_offset`, the answer is wrapped into a temporary at sp, applied with 0x08A76ABC, and the temporary
// released. 0x08A76ABC leaves a0 as the answer of the "apply" variant; the "assign" variant starts with `beq a0, sp`, so
// it copies into the caller's reference at a0 when that is not the temporary, and answers what the release returns.
void apply_member_from_holder(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 member_offset, u32 constructor,
                              u32 assert_message) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {assert_message});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + member_offset), {object});
    call_set(rt, ctx, constructor, {frame.at(0u), value});
    call_set(rt, ctx, 0x08A76ABCu, {self, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = self;
}
void assign_member_from_holder(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 member_offset, u32 constructor,
                               u32 assert_message) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {assert_message});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + member_offset), {object});
    call_set(rt, ctx, constructor, {frame.at(0u), value});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
// The same "apply" shape, for a virtual that answers in f0 (the temporary's constructor takes $f12).
void apply_float_member_from_holder(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 member_offset, u32 constructor,
                                    u32 assert_message) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {assert_message});
    call_indirect(rt, ctx, word(rt, word(rt, object) + member_offset), {object});
    const float value = ctx.fpr[0];
    call_float(rt, ctx, constructor, {frame.at(0u)}, {value});
    call_set(rt, ctx, 0x08A76ABCu, {self, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = self;
}
void func_08AA25F0(Runtime &rt, AllegrexContext &ctx) {
    apply_member_from_holder(rt, ctx, ctx.gpr[5], 0x44u, 0x08AB19B0u, 0x08CB2B50u);
}
void func_08AA2664(Runtime &rt, AllegrexContext &ctx) {
    apply_member_from_holder(rt, ctx, ctx.gpr[5], 0x34u, 0x08A960C0u, 0x08CB2B50u);
}
void func_08AA26D8(Runtime &rt, AllegrexContext &ctx) {
    apply_member_from_holder(rt, ctx, ctx.gpr[5], 0x30u, 0x08A97A6Cu, 0x08CB2B50u);
}
void func_08A78FCC(Runtime &rt, AllegrexContext &ctx) {
    assign_member_from_holder(rt, ctx, ctx.gpr[5], 0x44u, 0x08AB19B0u, 0x08CB0E38u);
}
void func_08A790B4(Runtime &rt, AllegrexContext &ctx) {
    assign_member_from_holder(rt, ctx, ctx.gpr[5], 0x4Cu, 0x08A5FA8Cu, 0x08CB0E38u);
}

// --- 0x08ABAB9C: links a node into the list at +0x44/+0x48 once, then finishes on the lock at +0x14 ---------------------------

// --- 0x08ACB484 and 0x08ACC640: waits out a flag byte, runs the object's virtual at +0x20, answers its word at +0x90 ---------
void run_while_flag_and_take(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0x60u;
    call_set(rt, ctx, 0x08B34888u, {self + 0x60u});
    while (rt.memory().load8(self + 0x8Du) != 0u) call_set(rt, ctx, 0x08B34B2Cu, {self + 0x60u});
    const u32 vtable = word(rt, self);
    ctx.gpr[2] = vtable;
    call_indirect(rt, ctx, word(rt, vtable + 0x20u), {self});
    const u32 result = word(rt, self + 0x90u);
    ctx.gpr[16] = result;
    call_set(rt, ctx, 0x08B34B24u, {self + 0x60u});
    ctx.gpr[2] = result;
}

// @@APPEND@@

} // namespace
} // namespace new_batch_15
} // namespace gt::game

#define NEW15(address, name, kind) GT_DECOMP(address, gt::game::new_batch_15::name, gt::ReturnKind::kind)
NEW15(0x08ADC27Cu, func_08ADC27C, Word);
NEW15(0x08B2E98Cu, func_08B2E98C, Word);
NEW15(0x08B48CFCu, func_08B48CFC, Word);
NEW15(0x08BAC354u, func_08BAC354, Word);
NEW15(0x08C4B3C0u, func_08C4B3C0, Word);
NEW15(0x0881AF30u, func_0881AF30, Word);
NEW15(0x088390FCu, func_088390FC, Word);
NEW15(0x088757ECu, func_088757EC, Word);
NEW15(0x088DD870u, func_088DD870, Word);
NEW15(0x088EF034u, func_088EF034, Word);
NEW15(0x088F1CC0u, func_088F1CC0, Word);
NEW15(0x08909928u, func_08909928, Word);
NEW15(0x08947A30u, func_08947A30, Float);
NEW15(0x08AC0584u, func_08AC0584, Word);
NEW15(0x08AC38ACu, func_08AC38AC, Word);
NEW15(0x08B33688u, func_08B33688, Word);
NEW15(0x08B34060u, func_08B34060, Word);
NEW15(0x08C70078u, func_08C70078, Word);
NEW15(0x08C73368u, func_08C73368, Word);
NEW15(0x08A2B5DCu, func_08A2B5DC, Word);
NEW15(0x08A4D508u, func_08A4D508, Word);
NEW15(0x08A6576Cu, func_08A6576C, Word);
NEW15(0x08A78FCCu, func_08A78FCC, Word);
NEW15(0x08A790B4u, func_08A790B4, Word);
NEW15(0x08AA25F0u, func_08AA25F0, Word);
NEW15(0x08AA2664u, func_08AA2664, Word);
NEW15(0x08AA26D8u, func_08AA26D8, Word);
