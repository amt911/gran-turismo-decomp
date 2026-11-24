// New batch, main session 61 (DRAFT, unverified): 80 functions of 126-144 instructions the recompiler emitted, none decompiled
// before, written by hand as structured C from `tools.disasm`. Most are container/scope code: objects live in the frame
// (their address is passed to the callee), the callee-saved registers are plain C++ locals, a `jalr` is a call through a vtable
// slot. v0 is left as the original leaves it on every path (call helpers keep v0/v1/f0 as the callee left them). No names: none
// has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_61 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 signed_byte(Runtime &rt, u32 address) { return static_cast<u32>(static_cast<std::int8_t>(rt.memory().load8(address))); }
u32 signed_half(Runtime &rt, u32 address) { return static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(address))); }

constexpr u32 kAssertFail = 0x08A5BB7Cu;   // called with a message when a pointer is null
constexpr u32 kScopeClose = 0x08A5B9ECu;   // closes a scope object on the frame
constexpr u32 kScopeAssign = 0x08A5BAD8u;  // (dst, src word)

// Drops one reference of a counted string whose characters start at `data` (the header sits 0xC before): unless it is the shared
// empty representation, 0x08B60804(data - 4, -1) is the decrement and, when it is not above zero, 0x08B8542C frees it (`slot` is
// the scratch address the original passes on the frame).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 empty, u32 slot) {
    const u32 header = data - 0xCu;
    if (header == empty) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {header, slot});
}

// 0x0880D278 (a0 = out block, a2 = count, a3 = pointer to an object pointer): with count > 0 the object's vtable slot 0xC builds a
// string at frame+0x14 from the object; its last character picks the empty representation (a '/') or a new copy (frame+0xC, built
// from the text 0x08C985CC); both counted strings are released and the object's member at +0xE8 is given the string. v0 = 1 then
// (0 for count <= 0), passed to 0x08A85B1C, which fills frame+4; that block goes to a0 and is closed.
void func_0880D278(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 pointer = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    constexpr u32 kEmpty = 0x08D74FD0u;
    const u32 scope = frame.at(0x10);
    const u32 text = frame.at(0x14);
    ctx.gpr[2] = 0u;
    if (count > 0) {
        call(rt, ctx, 0x0880B8F8u, {scope});
        const u32 object = word(rt, pointer);
        if (object == 0u) call(rt, ctx, kAssertFail, {0x08C98418u});
        call_virtual(rt, ctx, object, 0xCu, {text, object});
        u32 data = word(rt, text);
        const u32 length = word(rt, data - 0xCu);
        const u32 last = length - 1u;
        if (!(last < length)) call(rt, ctx, 0x08B5AEDCu, {0x08C985B8u});
        u32 character_at = data + last;
        if (static_cast<s32>(word(rt, data - 0xCu + 8u)) >= 0) {
            call(rt, ctx, 0x08B88FF8u, {text});
            data = word(rt, text);
            character_at = data + last;
        }
        const u32 character = signed_byte(rt, character_at);
        u32 copy_slot = frame.at(0xC);
        if (character == 0x2Fu) {
            // keeps the shared empty representation in place of a copy
        } else {
            call(rt, ctx, 0x08B85EB8u, {frame.at(0x8), 0x08C985CCu, frame.at(0x3)});
            call(rt, ctx, 0x08B89124u, {copy_slot, text, frame.at(0x8)});
            call(rt, ctx, 0x08B855E0u, {text, copy_slot});
            release_string(rt, ctx, word(rt, frame.at(0xC)), kEmpty, frame.at(0x2));
            release_string(rt, ctx, word(rt, frame.at(0x8)), kEmpty, frame.at(0x1));
        }
        u32 holder = word(rt, frame.at(0x10));
        if (holder == 0u) call(rt, ctx, kAssertFail, {0x08C9848Cu});
        call(rt, ctx, 0x0880B774u, {word(rt, holder + 0xE8u), text});
        release_string(rt, ctx, word(rt, text), kEmpty, frame.at(0x0));
        call(rt, ctx, kScopeClose, {scope});
        ctx.gpr[2] = 1u;
    }
    const u32 result_block = frame.at(0x4);
    call(rt, ctx, 0x08A85B1Cu, {result_block, ctx.gpr[2]});
    if (out != result_block) call(rt, ctx, kScopeAssign, {out, word(rt, frame.at(0x4))});
    call(rt, ctx, kScopeClose, {result_block});
}

// 0x08830554 (a0 = out block, a2 = count, a3 = pointer to an object pointer): a scope at frame+8 yields a collection; when
// count > 0 the object *a3 answers its vtable slot 0x30 (the key). A second scope (frame+0xC, filled by 0x08A98A70) is
// then given, for each index below the collection's count, the new element's block at +0x14 + 4, and the result goes to a0.

// Allocates a 0x18-byte callback object (vtable 0x08CB6B88) whose member at +0x14 is linked to `source`, stores it at `handle` and
// wraps it in `wrapper` (0x08A76AB4); returns the object.
u32 make_callback(Runtime &rt, AllegrexContext &ctx, u32 source, u32 handle, u32 wrapper) {
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call(rt, ctx, 0x08A76ACCu, {object});
    put(rt, object, 0x08CB6B88u);
    call(rt, ctx, 0x08A6972Cu, {object + 0x14u, source});
    put(rt, handle, object);
    call(rt, ctx, 0x08A76AB4u, {wrapper, handle});
    return object;
}

// 0x0884C244 (a0 = a context, a1 = pointer to an object pointer): the object's vtable slot 0x9C is called twice with a callback
// built on the frame (first from the text 0x08C9D04C, then 0x08C9D054); between them the first result is compared with
// 0x088D47E4's, and the pair only runs when they are equal. The second call's result block (frame+0x1C) gives, through its object's
// vtable slot 0xDC(0), the argument of 0x088D5588(0x08846E50(context), .).

// 0x08850958 (a0 = out block, a3 = pointer to an object pointer): the object's vtable slot 0x3C gives a key; the list built by
// 0x08A98674 (frame+8) holds 11 objects whose slot 0x3C answers are stored as halfwords at frame+0x1A..; 0x08949730 maps them to two
// halfword arrays (frame+0x30 and frame+0x48); each pair is then looked up in two index lists (frame+0xC, frame+0x10) and the results
// are copied into frame+0 / frame+4. The two selected blocks go to frame+0xC+... and finally to a0.

// 0x0885CCDC (a0 = a pool object, a1 = slot index, a2 = a descriptor, a3 = a word, t0 = a word kept on the frame): under the lock at
// +8 the slot's entry number (word +0x1C of the slot) is read from the table at *a0 (stride 4); an empty one (-1) is given the next
// free 392-byte record (a count at +4, at most 4) initialised by 0x0885D480 / 0x0885DB04. A record that exists is waited for
// (up to 0x16 polls of 0x0885E4C4 == 3 with a 0x2710 sleep), filled from the descriptor (0x0888EBE0 / 0x0888EBF4 -> 0x0885DB58),
// 0x08862150 is asked and, after the second wait (up to 0x3EA polls), 0x0885E68C hands the record on. v0 = 1 on success, else 0.

// 0x088800F4 (a0 = an object, a1, a2 = a reader): when the two probes on the object's member (+0x74) answer zero, a tag is read
// from the reader (0x08AC2690) and one of five records is parsed with the 0x08AC27xx/29xx reader helpers into the frame and handed
// to a setter of the member: 'ANIM' (a bit field of 1 at bits 5..7 of the word at frame+0x1C, 0x0886F854/0x0886F894 when the
// object 0x08BA4A78(0x0886A9A0(object, 5)) exists), 'TWLM' (a byte -> 0x08885288), 'SLOD' (a byte and a word -> 0x088852DC, -1
// when the byte is zero), 'MVAR' (a word -> 0x088852B4). The reader on frame+0x14 is closed on every path.

// The child installer of 0x0888504C: the member object at self+0xC keeps a child at `offset`; the old one is released (vtable slot
// 0x2C), a non-null `replacement` is cloned (slot 0x18) into the member, told its kind (0x0887BD94(kind) through slot +8), given
// (text, colour) (slot 0xC), the owner (slot 0x14) and a label (slot 0x18). True when the original ends in a tail call (the child's
// vtable slot 0x28, argument `tail_argument`); false when it returns early with v0 = word(self + 0xC) as left.
bool install_child(Runtime &rt, AllegrexContext &ctx, u32 self, u32 offset, u32 replacement, u32 kind, u32 text, u32 color,
                   u32 label, u32 &tail_argument) {
    u32 member = word(rt, self + 0xCu);
    ctx.gpr[2] = member;
    const u32 old = word(rt, member + offset);
    if (old != 0u) {
        call_virtual(rt, ctx, old, 0x2Cu, {old});
        member = word(rt, self + 0xCu);
        ctx.gpr[2] = member;
        put(rt, member + offset, 0u);
    }
    if (replacement == 0u) return false;
    const u32 holder = word(rt, self + 0xCu);
    const u32 created = call_virtual(rt, ctx, replacement, 0x18u, {replacement}).v0;
    put(rt, holder + offset, created);
    member = word(rt, self + 0xCu);
    ctx.gpr[2] = member;
    const u32 child = word(rt, member + offset);
    if (child == 0u) return false;
    const u32 setter = word(rt, word(rt, child) + 8u);  // (the load sits in the call's delay slot)
    const u32 value = call(rt, ctx, 0x0887BD94u, {kind});
    call(rt, ctx, setter, {child, value});
    u32 object = word(rt, word(rt, self + 0xCu) + offset);
    call_virtual(rt, ctx, object, 0xCu, {object, text, color});
    object = word(rt, word(rt, self + 0xCu) + offset);
    call_virtual(rt, ctx, object, 0x14u, {object, self, word(rt, object)});
    object = word(rt, word(rt, self + 0xCu) + offset);
    call_virtual(rt, ctx, object, 0x18u, {object, label});
    tail_argument = word(rt, word(rt, self + 0xCu) + offset);
    return true;
}

// 0x0888504C (a0 = an object, a1 = which child (0 or 1), a2 = the replacement): installs child 0 (member +0x190, kind 0xB, text
// 0x08CA0670, colour 0x80FF00FF, label 0x08BA60AC) or child 1 (+0x194, kind 0x10, 0x08CA0680, 0xFF8080FF, 0x08BA60B4); any other
// selector returns 1.

// 0x088DEA1C (a0 = a manager, a1, a2, a3 = an event's words): with a callback at +0x118, the event is first tried on the manager's
// pending slot (0x088E5E60 on the object at +0x640, when 0x088DE9CC says so) or placed in a free 760-byte slot of the table at +0xC0
// (found by 0x088DE5F4, with the previous slot -1/-2/-3 marked and the callback called with the slot number and the event block);
// otherwise the callback is called with -1 and the event block. v0 = 1 when the event was handled, else 0.
void func_088DEA1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 word1 = ctx.gpr[5];
    const u32 word2 = ctx.gpr[6];
    const u32 word3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    u32 callback = word(rt, self + 0x118u);
    auto finish = [&](u32 value) {
        ctx.gpr[3] = value;
        ctx.gpr[2] = value;
    };
    // The block the callback gets: (0, +4 = word at +0x124, +8 = word1, +0xC = word2, +0x10 = `slot`, +0x14 = 0, byte +0x18 = 0).
    auto fill_event = [&](u32 saved_word, u32 slot) {
        put(rt, frame.at(0x4), saved_word);
        put(rt, frame.at(0x0), 0u);
        put(rt, frame.at(0x8), word1);
        put(rt, frame.at(0xC), word2);
        put(rt, frame.at(0x14), 0u);
        put(rt, frame.at(0x10), slot);
        put_byte(rt, frame.at(0x18), 0u);
    };
    auto fallback = [&](u32 target) {
        const u32 saved_word = word(rt, self + 0x124u);
        put(rt, frame.at(0x8), word1);
        put(rt, frame.at(0x4), saved_word);
        put(rt, frame.at(0xC), word2);
        put(rt, frame.at(0x10), 0xFFFFFFFFu);
        put(rt, frame.at(0x0), 0u);
        put(rt, frame.at(0x14), 0u);
        put_byte(rt, frame.at(0x18), 0u);
        call(rt, ctx, target, {0xFFFFFFFFu, frame.at(0x0)});
        finish(0u);
    };
    if (callback == 0u) {
        finish(0u);
        return;
    }
    if (call(rt, ctx, 0x088DA4C8u, {self, 0u}) == 0u) return fallback(callback);
    const u32 limit = call(rt, ctx, 0x088D9F80u, {self});
    const u32 current = call(rt, ctx, 0x088DA168u, {self});
    if (!(static_cast<s32>(current) < static_cast<s32>(limit))) return fallback(callback);
    const u32 inner = self + 4u;
    if (call(rt, ctx, 0x088DE9CCu, {self}) != 0u) {
        if (call(rt, ctx, 0x088E5E60u, {word(rt, self + 0x640u), word1, word2, word3}) != 0u) {
            finish(1u);
            return;
        }
        return fallback(word(rt, self + 0x118u));
    }
    if (call(rt, ctx, 0x088D4C9Cu, {inner}) != 0u) return fallback(word(rt, self + 0x118u));
    const u32 slot = call(rt, ctx, 0x088DE5F4u, {self});
    if (slot == 0xFFFFFFFFu) return fallback(word(rt, self + 0x118u));
    u32 previous = 0xFFFFFFFFu;
    if (call(rt, ctx, 0x088D4D9Cu, {inner}) != 0u && slot == 0u) previous = call(rt, ctx, 0x088DE5F4u, {self});
    const u32 offset = slot * 760u;
    const u32 table = word(rt, self + 0xC0u);
    fill_event(word(rt, self + 0x124u), previous);
    call(rt, ctx, 0x088E7E94u, {table + offset, word1, word2, word3});
    const u32 entry = word(rt, self + 0xC0u) + offset;
    put_byte(rt, entry + 0x13u, 0u);
    put(rt, entry + 0x1FCu, previous);
    if (word3 != 0u) call(rt, ctx, 0x088E8A24u, {word(rt, self + 0xC0u) + offset, word(rt, word3 + 0x10u)});
    if (previous != 0xFFFFFFFFu) {
        const u32 other = word(rt, self + 0xC0u) + previous * 760u;
        put(rt, other + 0x1FCu, 0xFFFFFFFEu);
        put_byte(rt, other + 0x13u, 0u);
    }
    u32 target = 0u;
    if (call(rt, ctx, 0x088D4D9Cu, {inner}) == 0u) {
        target = word(rt, self + 0x118u);
    } else if (slot != 2u) {
        target = word(rt, self + 0x118u);
    } else {
        const u32 mark = word(rt, self + 0xC0u) + offset;
        put(rt, mark + 0x1FCu, 0xFFFFFFFDu);
        put_byte(rt, mark + 0x13u, 0u);
        target = word(rt, self + 0x118u);
    }
    call(rt, ctx, target, {slot, frame.at(0x0)});
    finish(1u);
}

// Shared by 0x0896F1BC and 0x0896F3B4 (they differ only in the text 0x08CA2B00 / 0x08CA2B10): when bit 2 of the word at +0x60 is
// set, an object (frame+0x14) is looked up for `text` by 0x08A7BFD0; its vtable slot 0xCC runs with (object, an empty list
// frame+0x18, 2, a block of two lists frame+0x1C) after the two argument words are appended to the block, and when the list at
// frame+0x18 is non-empty its first object's slot 0x3C answers; every list is closed. v0 = that answer, else 0.
void query_object(Runtime &rt, AllegrexContext &ctx, u32 text) {
    const u32 self = ctx.gpr[4];
    const u32 first_word = ctx.gpr[5];
    const u32 second_word = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    u32 result = 0u;
    if ((word(rt, self + 0x60u) & 4u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 found = frame.at(0x14);
    call(rt, ctx, 0x08A76AA0u, {found});
    call(rt, ctx, 0x08A69B3Cu, {frame.at(0x10), text});
    call(rt, ctx, 0x08A7BFD0u, {self, found, frame.at(0x10)});
    call(rt, ctx, 0x08A69D68u, {frame.at(0x10)});
    if (word(rt, found) == 0u) {
        call(rt, ctx, kScopeClose, {found});
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 empty_list = frame.at(0x18);
    call(rt, ctx, 0x08A76AA0u, {empty_list});
    const u32 block = frame.at(0x1C);
    for (u32 cell = block; cell != frame.at(0x24); cell += 4u) call(rt, ctx, 0x08A76AA0u, {cell});
    put(rt, frame.at(0x8), first_word);
    call(rt, ctx, 0x08A76AACu, {frame.at(0xC), frame.at(0x8)});
    if (block != frame.at(0xC)) call(rt, ctx, kScopeAssign, {block, word(rt, frame.at(0xC))});
    call(rt, ctx, kScopeClose, {frame.at(0xC)});
    put(rt, frame.at(0x0), second_word);
    call(rt, ctx, 0x08A76AACu, {frame.at(0x4), frame.at(0x0)});
    if (frame.at(0x20) != frame.at(0x4)) call(rt, ctx, kScopeAssign, {frame.at(0x20), word(rt, frame.at(0x4))});
    call(rt, ctx, kScopeClose, {frame.at(0x4)});
    const u32 object = word(rt, found);
    if (object == 0u) call(rt, ctx, kAssertFail, {0x08CA2878u});
    const u32 method = word(rt, word(rt, object) + 0xCCu);
    call(rt, ctx, method, {object, empty_list, 2u, block});
    const u32 head = word(rt, empty_list);
    const bool answered = head != 0u;
    if (answered) result = call_virtual(rt, ctx, head, 0x3Cu, {head}).v0;
    for (u32 cell = frame.at(0x24) - 4u;; cell -= 4u) {
        call(rt, ctx, kScopeClose, {cell});
        if (cell == block) break;
    }
    call(rt, ctx, kScopeClose, {empty_list});
    call(rt, ctx, kScopeClose, {found});
    ctx.gpr[2] = answered ? result : 0u;
}

// 0x0896F1BC: the query with the text 0x08CA2B00.
void func_0896F1BC(Runtime &rt, AllegrexContext &ctx) { query_object(rt, ctx, 0x08CA2B00u); }
// 0x0896F3B4: the query with the text 0x08CA2B10.
void func_0896F3B4(Runtime &rt, AllegrexContext &ctx) { query_object(rt, ctx, 0x08CA2B10u); }

} // namespace
} // namespace new_batch_main_61
} // namespace gt::game

#define NEW61(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_61::name, gt::ReturnKind::kind)
NEW61(0x0880D278u, func_0880D278, Word);
NEW61(0x088DEA1Cu, func_088DEA1C, Word);
NEW61(0x0896F1BCu, func_0896F1BC, Word);
NEW61(0x0896F3B4u, func_0896F3B4, Word);
