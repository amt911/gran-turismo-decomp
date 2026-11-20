// Main-session batch 5: 80 functions the extra seeds made emittable, all short (19-21 instructions) and written as
// structured C from `tools.disasm`. The batch does not name anything: every symbol keeps its address name (the few
// addresses that already have an address-to-role note elsewhere are pointed at below).
//
// Shapes (small helpers + one registration per function, as in new_batch_3 / new_batch_4):
//  - the guard-and-member getters (0x08824AD0..0x08824E38, 0x08A3D8F0): a temporary on the stack, a word of it must be
//    non-null (an assert-shaped call names it otherwise), the member at +0x14 goes to the real call;
//  - the "build a result on the stack, copy it into the caller's slot" family (0x08840278, 0x08840780, 0x08A205AC,
//    0x08A2B650, 0x08A2B6A4, 0x08A2B8D8, 0x08A30E48);
//  - the release loops over a circular list (0x08C2F9A4), over a vector of 8-byte pairs (0x08BA7EE4, 0x08BA8114,
//    0x08BA8164, 0x08BC099C) and over a list (0x08C50744, 0x08ABCD8C);
//  - the two-step destructors (vtable word, member calls, then a tail call into the base destructor);
//  - the "store the virtual's answer at this + 0x14" accessors (0x08AB0178, 0x08AB18FC, 0x08B86B68, 0x08B889D4,
//    0x08B89EFC).
//
// Every call passes the argument registers the original sets before the `jal`; a delay slot runs before its call, and
// v0 (v1 / f0 where the original leaves one) is set as the asm leaves it on every exit path. Callee-saved registers are
// never written (the original restores them). The `ll`/`sc` counter bump of the copy loops is a plain read-modify-write
// (single-threaded here), as in the earlier batches.
//
// Before registering: frame_audit, literal_check, sp_loads, arg_reads, then the synthetic gate and the sabotage.
//
// These bodies replace nothing: the same addresses were drafted in the register-level style of the earlier batches
// (hot_batch_152/184/185/186, 76, 84, 100, 109, 261) with their registration lines commented out ("no comparable
// synthetic case yet" / "compared 0"), so this file is the first live registration of each. Three of them already have
// an address-to-role note elsewhere worth keeping when they get names: 0x08840278 MSpecDB_getMakerSize and 0x08840780
// MSpecDB_getTunerSize (src/game/specdb/spec_db.cpp, the pad routes never call them), 0x08854A78
// MCarParameter_beginPartsChange (src/game/adhoc/game_natives.cpp, reads files inside a case), and 0x08B35DDC is libc's
// calloc (src/libc/malloc.cpp, never called on the routes; that file's own registration stays commented out).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_5 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using s32 = std::int32_t;
using u32 = std::uint32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee with float arguments ($f12-); v0, v1 and f0 are the callee's afterwards.
u32 call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
u32 byte_reversed(u32 value) { return __builtin_bswap32(value); }

// --- 0x08824AD0..0x08824E38 and 0x08A3D8F0: the member getter through a checked temporary ---------------------------

// a0 = nothing of the caller's: `prepare` fills a temporary at sp (its word at +0 must be non-null, `name` is the
// assert-shaped call that reports it otherwise), the member at +0x14 of that object is taken, the temporary is
// destroyed and the member goes to the real call. v0 = that call's result.
void member_through_temporary(Runtime &rt, AllegrexContext &ctx, u32 prepare, u32 name, u32 target) { GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, prepare, {sp});
    const u32 guard = word(rt, sp);
    if (guard == 0u) call(rt, ctx, 0x08A5BB7Cu, {name});
    const u32 member = word(rt, guard + 0x14u);
    call(rt, ctx, 0x08A5B9ECu, {sp});
    ctx.gpr[2] = call_guest(rt, ctx, target, {member});
}


// --- the "build on the stack, copy into the caller's slot" family ---------------------------------------------------

// a0 = the caller's hidden return slot. `prepare` fills sp + 4 from a null start, 0x08A85B1C(sp, that) builds the
// result at sp, the slot takes a copy of it (0x08A5BAD8) unless it already is this frame, and the temporary is
// destroyed. v0 = the destroy's result.
void temporary_into_slot(Runtime &rt, AllegrexContext &ctx, u32 prepare) { GuestFrame frame(ctx, 0x20u);
    const u32 slot = ctx.gpr[4];
    const u32 sp = ctx.gpr[29];
    put(rt, sp + 4u, 0u);
    call(rt, ctx, prepare, {sp + 4u});
    call(rt, ctx, 0x08A85B1Cu, {sp, word(rt, sp + 4u)});
    if (slot != sp) call(rt, ctx, 0x08A5BAD8u, {slot, word(rt, sp)});
    call(rt, ctx, 0x08A5B9ECu, {sp});
}

void func_08840278(Runtime &rt, AllegrexContext &ctx) { temporary_into_slot(rt, ctx, 0x0892C890u); }
void func_08840780(Runtime &rt, AllegrexContext &ctx) { temporary_into_slot(rt, ctx, 0x0892C8C8u); }

// a0 = the caller's hidden return slot: 0x08AC32A4 takes a0 itself (the slot), 0x08AC3324 turns its answer into the
// value that 0x08A97A6C puts in the temporary on the stack; the slot copies it unless it is this frame, and the
// temporary is destroyed.

// a0 = the caller's hidden return slot: `getter` answers for the object at 0x08D69170, the answer goes through
// 0x08A97A6C into a temporary on the stack, the slot copies it unless it is this frame, and the temporary is
// destroyed.
void getter_into_slot(Runtime &rt, AllegrexContext &ctx, u32 getter) { GuestFrame frame(ctx, 0x20u);
    const u32 slot = ctx.gpr[4];
    const u32 sp = ctx.gpr[29];
    const u32 value = call(rt, ctx, getter, {0x08D69170u});
    call(rt, ctx, 0x08A97A6Cu, {sp, value});
    if (slot != sp) call(rt, ctx, 0x08A5BAD8u, {slot, word(rt, sp)});
    call(rt, ctx, 0x08A5B9ECu, {sp});
}


// 0x08A2B650 (a0 = the caller's hidden return slot): the object state test inverted (`xori 1`) and reduced to
// "not zero" (`sltiu 1`), which 0x08A97A6C puts in the temporary; the slot copies it unless it is this frame, and the
// temporary is destroyed.

// a0 = a temporary to fill, its word at +0 must be non-null (`name` reports it): 0x08843404 (0x08854678) unwraps it
// into an inner object, the real call takes that, and the temporary is destroyed at the end.
void unwrapped_call(Runtime &rt, AllegrexContext &ctx, u32 prepare, u32 name, u32 unwrap, u32 target) { GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, prepare, {sp});
    const u32 object = word(rt, sp);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {name});
    const u32 inner = call(rt, ctx, unwrap, {object});
    call(rt, ctx, target, {inner});
    call(rt, ctx, 0x08A5B9ECu, {sp});
}


// 0x089897D0: a temporary on the stack (0x089896CC fills it), its word must be non-null (0x08CA39F0 names it), the
// region at +0xF0 of it is reset (0x089833D0(object, 0, 0)) and the temporary is destroyed.
void func_089897D0(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, 0x089896CCu, {sp});
    const u32 object = word(rt, sp);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
    call(rt, ctx, 0x089833D0u, {object + 0xF0u, 0u, 0u});
    call(rt, ctx, 0x08A5B9ECu, {sp});
}

// --- the guarded object accessors ----------------------------------------------------------------------------------

// a1 = a holder whose word at +0xC (at +0 for the 0x08B86B68 / 0x08B88xxx ones) must be non-null (`name` reports it):
// the virtual at `slot` of it answers, and the answer is stored at +0x14 of a0 as `kind` says. v0 = the call's v0.
enum class Store { Word, Half, Byte, Float };
void store_virtual_at_14(Runtime &rt, AllegrexContext &ctx, u32 name, u32 offset, u32 slot, Store kind) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 object = word(rt, ctx.gpr[5] + offset);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {name});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object});
    switch (kind) {
    case Store::Word: put(rt, self + 0x14u, value); break;
    case Store::Half: rt.memory().store16(self + 0x14u, static_cast<std::uint16_t>(value)); break;
    case Store::Byte: rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(value)); break;
    case Store::Float: put_float(rt, self + 0x14u, ctx.fpr[0]); break;
    }
}

void func_08AB18FC(Runtime &rt, AllegrexContext &ctx) { store_virtual_at_14(rt, ctx, 0x08CB3AF0u, 0u, 0x44u, Store::Byte); }
void func_08B889D4(Runtime &rt, AllegrexContext &ctx) { store_virtual_at_14(rt, ctx, 0x08C9832Cu, 0u, 0x54u, Store::Float); }
void func_08B89EFC(Runtime &rt, AllegrexContext &ctx) { store_virtual_at_14(rt, ctx, 0x08C98D34u, 0u, 0x4Cu, Store::Word); }

// 0x08AB6F7C / 0x08AB7054 (a0 = this, a1 = a holder): the holder's word at +0xC must be non-null (0x08CB4250 names it)
// and goes to the real call as its second argument, with this as the first. v0 = this.
void member_two_argument_call(Runtime &rt, AllegrexContext &ctx, u32 name, u32 target) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 member = word(rt, ctx.gpr[5] + 0xCu);
    if (member == 0u) call(rt, ctx, 0x08A5BB7Cu, {name});
    call(rt, ctx, target, {self, member});
    ctx.gpr[2] = self;
}

void func_08AB6F7C(Runtime &rt, AllegrexContext &ctx) { member_two_argument_call(rt, ctx, 0x08CB4250u, 0x08AAECB0u); }
void func_08AB7054(Runtime &rt, AllegrexContext &ctx) { member_two_argument_call(rt, ctx, 0x08CB4250u, 0x08AAE2D4u); }

// --- the release / copy loops --------------------------------------------------------------------------------------

// 0x08C2F9A4 (a0 = the head of a circular list whose last node points back at the head): every node but the head is
// released (0x08A5BC54(node, 0xC, 4)); the next node is read before the call, so a released node is never read again.

// The word-pair copy of 0x08BA7EE4 / 0x08BA8114 / 0x08BA8164 / 0x08BC099C (a0 = first pair, a1 = one past the last,
// a2 = the target pair): each 8-byte pair is copied and the word at +4 of the source pair, when not null, has its
// count at +4 raised by one (`ll`/`sc`, single-threaded here). A null target advances without writing, and the null
// test is made again for every pair. v0 = the target after the last pair.
void copy_counted_pairs(Runtime &rt, AllegrexContext &ctx) {
    const u32 last = ctx.gpr[5];
    u32 source = ctx.gpr[4];
    u32 target = ctx.gpr[6];
    while (source != last) {
        if (target != 0u) {
            put(rt, target, word(rt, source));
            const u32 counted = word(rt, source + 4u);
            put(rt, target + 4u, counted);
            if (counted != 0u) put(rt, counted + 4u, word(rt, counted + 4u) + 1u);
        }
        source += 8u;
        target += 8u;
    }
    ctx.gpr[2] = target;
    ctx.gpr[3] = target;
}

void func_08BA7EE4(Runtime &rt, AllegrexContext &ctx) { copy_counted_pairs(rt, ctx); }

// 0x08C50744 (a0 = first slot, a1 = one past the last): every 0x10-byte slot whose first word is not null has that
// word released (0x08B61FC0). v0 is the word the loop looked at last (the null one), or the last call's result.
void func_08C50744(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 last = ctx.gpr[5];
    for (u32 slot = ctx.gpr[4]; slot != last; slot += 0x10u) {
        const u32 object = word(rt, slot);
        ctx.gpr[2] = object;
        if (object != 0u) call(rt, ctx, 0x08B61FC0u, {object});
    }
}

// 0x08ABCD8C: the list head at 0x08D77190; for every node the object at +4 (a0) takes the virtual at +0xC.

// 0x08C24F48 (a0 = first, a1 = one past the last, a2 = an argument): every 4-byte slot goes to 0x08A6A23C(slot, a2).

// 0x089B8C18 (a0 = object, a1 = an argument): the list at +0xC of the object; every node goes to 0x089B8BB4(object, a1)
// and the next node is its word at +0x54 (read before the call).

// --- the two-step destructors (vtable word, member calls, a tail call into the base) --------------------------------

// a0 = this: the vtable goes at +0 (its first store sits in the call's delay slot), `slot` of it is destroyed by
// 0x08A5B930 when its word is not null, then the work goes on in a tail call.
void release_member_then_tail(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 slot, u32 tail) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, vtable);
    const u32 member = word(rt, self + slot);
    if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
    tail_call(ctx, tail, {self});
}

void func_0899A490(Runtime &rt, AllegrexContext &ctx) { release_member_then_tail(rt, ctx, 0x08CCBBA8u, 0x138u, 0x089FC750u); }

// 0x08A612C0 (a0 = this): the vtable at +0, 0x08C1F510(this + 0x18), 0x08A5B9EC(this + 0x14), 0x08A780B0(this), then
// the release of the whole object in a tail call.

// 0x08A3AEE8 (a0 = this): the vtable word at +0, 0x08A3A6FC(this), the two members +0x34 and +0x30 destroyed, the
// base destructor at +0, then the release of the whole object in a tail call.

// 0x089B7178 (a0 = this): the vtable word at +0, its second vtable word at +0x38, the three members +0x134, +0x130
// and +0x12C destroyed, then the base destructor in a tail call.
void func_089B7178(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CCAC10u);
    put(rt, self + 0x38u, 0x08CCAE6Cu);
    call(rt, ctx, 0x08A5B9ECu, {self + 0x134u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x130u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x12Cu});
    tail_call(ctx, 0x089A29E4u, {self});
}

// 0x089BFDA0 (a0 = this): the vtable word at +0 and its second one at +0x38, 0x08B28DC4(this + 0x2B0),
// 0x08A5B9EC(this + 0x264), 0x08A19DD4(this + 0x22C), then the base destructor in a tail call.
void func_089BFDA0(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CCA568u);
    put(rt, self + 0x38u, 0x08CCA7A8u);
    call(rt, ctx, 0x08B28DC4u, {self + 0x2B0u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x264u});
    call(rt, ctx, 0x08A19DD4u, {self + 0x22Cu});
    tail_call(ctx, 0x089E2430u, {self});
}

// --- single-shape functions ---------------------------------------------------------------------------------------

// 0x08C553B0 (a0 = a destination block, a1 = a record of six words): the words at +0, +4 and +0xC of the record are
// addresses, and the word at -0xC of each relocates a place inside the block (the compiler's "base of the subobject"):
// the record's +0x14 goes to that place of a1[0], the block's +4 is zeroed and its +8 keeps a1[0xC], the record's
// +0x10 goes at +8 of the place of a1[0xC], a1[4] to the block's +0 and a1[8] to the place of a1[4].
// v0 = the first of those places, v1 = the word from +0x14.

// 0x08C6F4DC (a0 = object, a1 = an argument): 0x08C5477C(a0 + word(*a0 - 0xC), 0xA) then the tail call
// 0x08B5F7EC(a0, a1, that result).

// 0x08C782C4 (a0 = object): 0x08C78130(a0 + 4); a non-zero answer is the result, otherwise the object's subobject
// (base of *a0 at -0xC) sets bit 2 of its word at +0x14 and goes on in the tail call 0x08C54CFC.
void func_08C782C4(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 found = call(rt, ctx, 0x08C78130u, {self + 4u});
    if (found != 0u) return;
    const u32 whole = self + word(rt, word(rt, self) - 0xCu);
    tail_call(ctx, 0x08C54CFCu, {whole, word(rt, whole + 0x14u) | 4u});
}

// 0x08875038 (a0 = a word to store the new object in, a1 = its argument): 0x08B62344(0x858) creates it,
// 0x088765D4(it, a1) fills it, and the word at *a0 takes it. v0 = the fill's result.
void func_08875038(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 created = call(rt, ctx, 0x08B62344u, {0x858u});
    call(rt, ctx, 0x088765D4u, {created, argument});
    put(rt, out, created);
}

// 0x08875D60 (a0 = object): v0 = 0x08875AD8(a0); when the object at +4 exists its virtual at +8 (a0 = it) runs.
void func_08875D60(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 answer = call(rt, ctx, 0x08875AD8u, {self});
    const u32 object = word(rt, self + 4u);
    if (object != 0u) call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    ctx.gpr[2] = answer;
}

// 0x088A9DF8 (a0 = a lookup object, a1 = an index shifted by 5, a2 and a3 = two words for the first temporary): the
// table at +0x32C of the word at +8 of a0; 0x08BAD978(sp, entry) and 0x08BAD950(sp + 4, entry + 0x10) build on the
// stack, and v0 = the byte at +0x1C of the entry.
void func_088A9DF8(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5] << 5;
    const u32 sp = ctx.gpr[29];
    put(rt, sp, ctx.gpr[6]);
    put(rt, sp + 4u, ctx.gpr[7]);
    const u32 entry = word(rt, word(rt, self + 8u) + 0x32Cu) + index;
    call(rt, ctx, 0x08BAD978u, {sp, entry});
    call(rt, ctx, 0x08BAD950u, {sp + 4u, entry + 0x10u});
    ctx.gpr[2] = rt.memory().load8(entry + 0x1Cu);
}

// 0x088B1418 (a0 = a holder): three words of a record on the stack (0x08CC3040, 6, -1) and the virtual at +0x30 of
// *(*a0) with a0 = *a0, a1 = a0[1], a2 = the record and a3 = *(*a0).

// 0x088E47B4 (a0 = a holder, a1 = an offset): under 0x08B344E0 / 0x08B3457C on the lock at *a0 + 0x1C8, v0 is the
// signed byte at a1 + *a0 + 0x4B0.

// 0x08925C2C (a0 = a lookup object: a base word at +4 and a count at +0xC): the word at +0x60 of base + count*0x268
// plus 4 goes to 0x08B21590, whose answer minus 0x80 is converted to a float and multiplied by 1.0f. f0 = that float
// (v0 = the integer it came from).

// 0x08938088 (a0 = a directory, a1 = the text, a2 = an index shifted by 3, a3 = a length - 1): the directory's word
// at +4 gives the table (its count at +4, its entries at +0x10): entry = table + 0x10 + count*8 + table[index*2].
// A length that matches a3 hands (text, entry + 2, length) to 0x08B36170 in a tail call, otherwise v0 = a3 - length.

// 0x08A003F8 (a0 = object, a1 = an argument): 0x08A5BA28(a1), then 0x089B8BAC(this + 0xE8, a1) and
// 0x089B8F30(this + 0xE8, a1), then the tail call 0x08A5B970(a1).

// 0x08A12E38 (a0 = an object): a block of 0x7C0 bytes (0x08A5BC20(0x7C0, 4)) goes through 0x08A12C54 and its address
// sits at sp for 0x089C15F4(object, sp).

// 0x08A4A350 (a1 = a signed count, a2 = a pointer to the holder): 1000, or with a count above zero the virtual at
// +0x3C of the object at *a2 (0x08CAED38 names it when that is null); the answer goes to 0x08B2E2B8 as a0.

// 0x08AA8F38 (a0 = object): the word at +4 must be non-null (0x08CB30B0 names it); 0x08A71C1C(it) is an index into
// the word table at 0x08CFF7D8. v0 = that word.
void func_08AA8F38(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 handle = word(rt, ctx.gpr[4] + 4u);
    if (handle == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    const u32 index = call(rt, ctx, 0x08A71C1Cu, {handle});
    ctx.gpr[2] = word(rt, 0x08CFF7D8u + (index << 2));
}

// 0x08B10C74 (a0 = object, a1 = another object): under 0x08B34888 / 0x08B34B24 on the lock at +0x14, the words at
// +0x40 of a0 and +0x3C of a1 go through 0x08B1FCD4.
void func_08B10C74(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    call(rt, ctx, 0x08B34888u, {self + 0x14u});
    call(rt, ctx, 0x08B1FCD4u, {self + 0x40u, other + 0x3Cu});
    tail_call(ctx, 0x08B34B24u, {self + 0x14u});
}

// 0x08B15360 (a0 = an argument of the first call, a1 = a holder): 0x08B104F8(a0, a1), then the status
// 0x08B33A24(a1[4]); a negative status is turned into a value by 0x08B1830C that goes to a1[1]. v0 = the status.

// 0x08B2235C (a0 = object, a1 = another object): under 0x08B344E0 / 0x08B3457C on the lock at +4, the words at
// +0x2C of a0 and +0x34 of a1 go through 0x08B1FCD4.

// 0x08B31A64 (a0 = object): with the state test 0x08B319FC(a0) equal to 3, the word at +0x18 is used as it is when it
// is not null, else filled by 0x08B318E4(0x2C00); the answer is `slt $a0, $zero, $v0` (signed: above zero), and 1 when
// the state is not 3. v0 = a0 = the answer.

// 0x08B729C4 (a0 = a table of slots, a1 = a signed index, a2 = a value): with the index below 0x133 and the kind byte
// of the table at 0x08D72BB4 equal to 4, the slot at a0[index] takes the value and v0 = 4; anything else goes to
// 0x08B77B04 and v0 is its result.

// 0x08C6B8AC, 0x08C6C51C, 0x08C6D258, 0x08C6E03C (a0 = the this of the call, a1 and a2 = two 8-byte pairs): both pairs
// are copied onto the stack and the call gets them as its second and third argument.
void pairs_to_call(Runtime &rt, AllegrexContext &ctx, u32 target) { GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 sp = ctx.gpr[29];
    put(rt, sp + 0x10u, word(rt, first));
    put(rt, sp + 0x14u, word(rt, first + 4u));
    put(rt, sp + 8u, word(rt, second));
    put(rt, sp + 0xCu, word(rt, second + 4u));
    ctx.gpr[2] = call_guest(rt, ctx, target, {self, sp + 0x10u, sp + 8u});
}

void func_08C6B8AC(Runtime &rt, AllegrexContext &ctx) { pairs_to_call(rt, ctx, 0x08C6AC7Cu); }
void func_08C6C51C(Runtime &rt, AllegrexContext &ctx) { pairs_to_call(rt, ctx, 0x08C6B8FCu); }
void func_08C6D258(Runtime &rt, AllegrexContext &ctx) { pairs_to_call(rt, ctx, 0x08C6C56Cu); }

// 0x08C751C8 (a0 = a buffer holder, a1 = the position, a2 = the end): the distance of the position from the buffer
// start (the word at +0) moves the tail in 0x08C74B68(holder, offset, end - position, 0), the word before the start
// becomes -1 and v0 = the position. 0x08C75218 is the same with a length of 1.
void close_off_tail(Runtime &rt, AllegrexContext &ctx, u32 length) { GuestFrame frame(ctx, 0x10u);
    const u32 holder = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 base = word(rt, holder);
    const u32 offset = position - base;
    call(rt, ctx, 0x08C74B68u, {holder, offset, length, 0u});
    const u32 refreshed = word(rt, holder);
    put(rt, refreshed - 4u, 0xFFFFFFFFu);
    ctx.gpr[2] = refreshed + offset;
}


// 0x088056A4: eight initialisers in order, then the tail call 0x088057B0. None of them reads the argument registers
// and the original leaves a0 alone between the calls.

// 0x08862BF8 (a0 = an object): when 0x08862A90(a0) is not zero, the word at +0x3F80 of *a0 goes to 0x088915D4 and
// its word at +0xE0 (its bytes reversed by `wsbw`) is the second argument of 0x088DADAC, whose first is the word at
// +0x5AC of *a0. v0 = v1 = 1 when that ran, 0 otherwise.
void func_08862BF8(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    if (call(rt, ctx, 0x08862A90u, {self}) != 0u) {
        const u32 found = call(rt, ctx, 0x088915D4u, {word(rt, self) + 0x3F80u});
        const u32 reversed = byte_reversed(word(rt, found + 0xE0u));
        call(rt, ctx, 0x088DADACu, {word(rt, word(rt, self) + 0x5ACu), reversed});
        result = 1u;
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// 0x0892B9DC (a0 = object, a1 = an argument): 0x08B2235C(a1, a0); the object at +0x2C, when there is one, takes the
// virtual at +0xC with (it, word(a0 + 0x260) + 0x40); last, the word at +0x260 of a0 becomes zero (v0 = it).

// 0x08A35160 (a0 = object, a1 = an argument): 0x08A35114(a0, a1) first; the member at +0x1C must be non-null
// (0x08CACEC8 names it) and goes to 0x08A3A73C with a1 and a third argument 0 in a tail call.
void func_08A35160(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    call(rt, ctx, 0x08A35114u, {self, argument});
    const u32 object = word(rt, self + 0x1Cu);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEC8u});
    tail_call(ctx, 0x08A3A73Cu, {object, argument, 0u});
}

// 0x08A6AD7C (a0 = object, a1 = an argument): the virtual at +0x11C of *a0 (a0 = the object) first, then
// 0x08B855E0(this + 0x1C, a1) and the two words +0x18 = 1 and +0x14 = 0. v0 = v1 = 1.

// 0x08A704C8 / 0x08A71F1C: a temporary on the stack (0x08A70338 / 0x08A71E70 fills it) whose word must be non-null
// (0x08CB0928 / 0x08CB0A94 names it) takes the virtual at +0x10C / +0x120 of its vtable, then the temporary is
// destroyed. v0 = the destroy's result.
void temporary_virtual(Runtime &rt, AllegrexContext &ctx, u32 prepare, u32 name, u32 slot) { GuestFrame frame(ctx, 0x20u);
    const u32 sp = ctx.gpr[29];
    call(rt, ctx, prepare, {sp});
    const u32 object = word(rt, sp);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {name});
    call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object});
    call(rt, ctx, 0x08A5B9ECu, {sp});
}


// 0x08AC7820 (a0 = this): the vtable-free init 0x08AC76E8(this, 0.0f), 0x08AC77B0(this), the words at +0 and +4
// zeroed, then 0x08AC718C(this + 0x5BC), 0x08C3CCD0(this + 0x578), 0x08B0C808(this + 0x438) and the tail call
// 0x08B28BE0(this + 8).

// 0x08B0DD88 (a0 = object): under 0x08B34888 / 0x08B34B24 on the lock at +4, the flag byte at +0xE8 is replaced by 1
// and v0 = the previous byte taken with `xori 1` (the "was it off" answer).

// 0x08B35DDC (a0, a1 = two factors): the low 32 bits of the product (the `mult`'s lo) is the size
// 0x08B35DA0(bytes) allocates; when the block is not null 0x08B36018(block, 0, size) clears it. v0 = the block.
void func_08B35DDC(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 size = ctx.gpr[4] * ctx.gpr[5];
    const u32 block = call(rt, ctx, 0x08B35DA0u, {size});
    if (block != 0u) call(rt, ctx, 0x08B36018u, {block, 0u, size});
    ctx.gpr[2] = block;
}

// 0x08BC4DDC (a0 = this, a1 = an argument, a2-a3/t0-t3 = further arguments): the further argument registers are
// stored at sp + 0x18 and up (where the callee reads them) and 0x08BC4D2C(this, a1) answers; its v0, a1 and a zero
// byte go to +0, +4 and +8 of this.
void func_08BC4DDC(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 sp = ctx.gpr[29];
    put(rt, sp + 0x18u, ctx.gpr[6]);
    put(rt, sp + 0x1Cu, ctx.gpr[7]);
    put(rt, sp + 0x20u, ctx.gpr[8]);
    put(rt, sp + 0x24u, ctx.gpr[9]);
    put(rt, sp + 0x28u, ctx.gpr[10]);
    put(rt, sp + 0x2Cu, ctx.gpr[11]);
    const u32 answer = call(rt, ctx, 0x08BC4D2Cu, {self, argument});
    put(rt, self, answer);
    put(rt, self + 4u, argument);
    rt.memory().store8(self + 8u, 0u);
}

} // namespace
} // namespace new_batch_main_5
} // namespace gt::game

#define NEW5(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_5::name, gt::ReturnKind::kind)
NEW5(0x08C782C4u, func_08C782C4, Word);
NEW5(0x08840278u, func_08840278, Word);
NEW5(0x08840780u, func_08840780, Word);
NEW5(0x08875038u, func_08875038, Word);
NEW5(0x08875D60u, func_08875D60, Word);
NEW5(0x088A9DF8u, func_088A9DF8, Word);
NEW5(0x0899A490u, func_0899A490, Word);
NEW5(0x08AA8F38u, func_08AA8F38, Word);
NEW5(0x08AB6F7Cu, func_08AB6F7C, Word);
NEW5(0x08AB7054u, func_08AB7054, Word);
NEW5(0x08B10C74u, func_08B10C74, Word);
NEW5(0x08BA7EE4u, func_08BA7EE4, Word);
NEW5(0x08C50744u, func_08C50744, Word);
NEW5(0x08C6B8ACu, func_08C6B8AC, Word);
NEW5(0x08C6C51Cu, func_08C6C51C, Word);
NEW5(0x08C6D258u, func_08C6D258, Word);
NEW5(0x08862BF8u, func_08862BF8, Word);
NEW5(0x089897D0u, func_089897D0, Word);
NEW5(0x089B7178u, func_089B7178, Word);
NEW5(0x089BFDA0u, func_089BFDA0, Word);
NEW5(0x08A35160u, func_08A35160, Word);
NEW5(0x08AB18FCu, func_08AB18FC, Word);
NEW5(0x08B35DDCu, func_08B35DDC, Word);
NEW5(0x08B889D4u, func_08B889D4, Word);
NEW5(0x08B89EFCu, func_08B89EFC, Word);
NEW5(0x08BC4DDCu, func_08BC4DDC, Word);
