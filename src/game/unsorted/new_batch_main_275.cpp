// ---- from main-session batch 181 (draft 681fc7c487a8b13d4a66402dfb5ebf67c2483598) ----
// Main-session batch 181: 29 functions the earlier main-session batches could not get through the gate with useful
// inputs (many were drafted in batch 10 and never verified), re-read from `tools.disasm` and written as structured C.
// Two families make up most of it:
//   * the container insert GT's parameter tables use everywhere: build a temporary key object on the frame, look the
//     object up, convert it, and assign the value into *a0 through an insert helper (three orderings, plus the float
//     and the custom 0x0881F048);
//   * destructor-shaped bodies: two vtable stores, a container block released through 0x08B60804 / 0x08B8542C, and a
//     chain of virtual calls, sometimes ending in a tail call.
// The rest are walks, an element-wise copy, two 64-bit logical operations, a container push and a vector clear.
// No names: none has evidence. Unverified drafts: the synthetic gate decides which of them register.
// Input shapes for the synthetic generator: /tmp/claude-1000/shapes_main181.txt (one line per address).
// Left out: 0x08B5AA30 — its failing path jumps back into 0x08B62498 (an assert that does not return) and then into
// the same block again, so the CFG has no exit; batch 10 left it out for the same reason.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_181 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the `jal` (a0-a3, t0-t3 in order;
// what is not passed stays as the caller had it). Afterwards v0 is the callee's, as after the `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same with float arguments ($f12, $f13, ...). Returns $f0; v0 / v1 afterwards are the callee's.
float call_with_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// The callee-saved registers a body keeps values in (the harness compares s0-s7 and $f20-$f30, so the caller's values
// have to come back).
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx), count_(0) {
        for (const unsigned reg : registers) {
            if (count_ < 8u) {
                registers_[count_] = reg;
                values_[count_] = ctx_.gpr[reg];
                ++count_;
            }
        }
    }
    ~SavedRegisters() {
        while (count_ != 0u) {
            --count_;
            ctx_.gpr[registers_[count_]] = values_[count_];
        }
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned registers_[8];
    u32 values_[8];
    unsigned count_;
};

s32 signed_byte(Runtime &rt, u32 address) {
    return static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address)));
}
void put_byte(Runtime &rt, u32 address, u32 value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}

// ---------------------------------------------------------------------------------------------------------------
// The container-insert family. Every member builds a temporary key object on the frame with `create`, looks the
// object up with `lookup`, converts it and inserts the value next to it, then destroys what it built. Three orders
// exist; the insert always ends by assigning the slot into *a0 (skipped when the destination already is the slot).
// ---------------------------------------------------------------------------------------------------------------

// create(&temporary at sp + 4, a1) → lookup(object) → destroy(temporary) → convert(object) → insert(&slot at sp, value).
struct LookupInsert {
    u32 create;
    u32 lookup;
    u32 convert;
    u32 insert;
    u32 null_text;  // the assert's text when the lookup found nothing
};

void lookup_insert(Runtime &rt, AllegrexContext &ctx, const LookupInsert &plan) {
    const u32 destination = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call_set(rt, ctx, plan.create, {temporary, ctx.gpr[5]});
    const u32 object = word(rt, temporary);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {plan.null_text});
    const u32 found = call_set(rt, ctx, plan.lookup, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    const u32 value = call_set(rt, ctx, plan.convert, {found});
    call_set(rt, ctx, plan.insert, {frame.at(0u), value});
    if (destination != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// create(&temporary at sp, a1) → lookup(object) → convert(object) → make(&value at sp + 4, value) → assign → destroy.
struct PairInsert {
    u32 create;
    u32 lookup;
    u32 convert;
    u32 make;
    u32 null_text;
};

void pair_insert(Runtime &rt, AllegrexContext &ctx, const PairInsert &plan) {
    const u32 destination = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(0u);
    const u32 slot = frame.at(4u);
    call_set(rt, ctx, plan.create, {temporary, ctx.gpr[5]});
    const u32 object = word(rt, temporary);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {plan.null_text});
    const u32 found = call_set(rt, ctx, plan.lookup, {object});
    const u32 value = call_set(rt, ctx, plan.convert, {found});
    call_set(rt, ctx, plan.make, {slot, value});
    if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// The same second order with a float value: `convert` returns it in $f0, and it goes to `make` in $f12.
void pair_insert_float(Runtime &rt, AllegrexContext &ctx, u32 create, u32 lookup, u32 convert, u32 make,
                       u32 null_text) {
    const u32 destination = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(0u);
    const u32 slot = frame.at(4u);
    call_set(rt, ctx, create, {temporary, ctx.gpr[5]});
    const u32 object = word(rt, temporary);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {null_text});
    const u32 found = call_set(rt, ctx, lookup, {object});
    const float value = call_with_floats(rt, ctx, convert, {found}, {});
    call_with_floats(rt, ctx, make, {slot}, {value});
    if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// 0x0882F370 (a0 = the destination): 0x088297A8(sp + 4, a1), 0x088294D0(object), destroyed, 0x0885FCA4(object) and
// 0x08A97A6C(sp, value) assigning into *a0.

// 0x0882F52C: the same with the conversion 0x0885E9B8.

// 0x08843D64 (a0 = the destination): the temporary at sp, the value at sp + 4: 0x088435A8(sp, a1), 0x08843404,
// 0x088D46B8 and 0x08A85B1C(sp + 4, value).

// 0x08851390: the same with 0x088547C4 / 0x08854678 / 0x088A5A90.

// 0x08856CB0: the same call chain again; only the assert's text is this table's.

// 0x088579E8: the float variant of that chain (0x088A5AE4 returns $f0, 0x08A87EEC takes it in $f12).

// 0x0881F048 (a0 = the destination): a temporary at sp + 4 (0x0881BD98), the object it holds converted by
// 0x0881B068, 0x0881C89C(sp, converted, 0) assigning into *a0 (when it is not sp), then the temporaries destroyed.

// ---------------------------------------------------------------------------------------------------------------
// Destructor-shaped bodies: two vtable stores, the container block at `block_offset` released, then the body's own
// calls (or a tail call). 0x08B60804 answers a reference count; only a non-positive answer frees the block.
// ---------------------------------------------------------------------------------------------------------------
void release_block(Runtime &rt, AllegrexContext &ctx, u32 self, u32 block_offset) {
    const u32 block = word(rt, self + block_offset);
    const u32 inner = block - 0xCu;
    if (inner != 0x08D74FD0u) {
        const u32 reference = call_set(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(reference) <= 0) call_set(rt, ctx, 0x08B8542Cu, {inner, ctx.gpr[29]});
    }
}

// 0x089B4008 (a0 = the object): the vtables 0x08CBB320 at *this and 0x08CBB578 at +0x38, 0x08B48804(this + 0x120),
// 0x08A5B9EC(this + 0x11C), the block at +0x118 released, then 0x08A0298C(this).

// 0x089D8C20 (a0 = the object): the vtables 0x08CBCDE0 at *this and 0x08CBCFCC at +0x38, the block at +0x78
// released, then 0x08A101F8(this) and 0x08A5BC54(this, 0xA8, 4).

// 0x08869148 (a0 = the object): the members at +0x1BC and +0x1C0 each get their virtual at +0x30 (a1 = 0) and at +0x10;
// the first also its virtual at +0; the last call is the +0x1C0 member's virtual at +0, as a tail call.

// 0x08AA9744 (a0 = the object): the vtable 0x08CD5358 at *this, 0x08C2FBF8 over the three words at +0x38 / +0x3C /
// +0x40 (sp is its scratch), then the vector at +0x38 freed with 0x08A5BC54 (its size from the word distance, rounded
// down to a multiple of four), the containers at +0x20 and +8 destroyed with their words at +8, and +4 with 0x08A5B9EC.

// 0x08B5156C (a0 = the object, a1 / a2 = two arguments): the vtable 0x08CDAC58 at *this, 0x08B51508(this) stored at
// +0x14, a 0x28-byte node from 0x08B62344 filled by 0x08B52238(node, a1, a2, table) and stored at +0xC20, then the
// words at +0xC, +4 and +8 cleared.
void func_08B5156C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx, {16, 17, 18, 19, 20});
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CDAC58u);
    const u32 table = call_set(rt, ctx, 0x08B51508u, {self, first});
    put(rt, self + 0x14u, table);
    const u32 node = call_set(rt, ctx, 0x08B62344u, {0x28u});
    call_set(rt, ctx, 0x08B52238u, {node, first, second, table});
    put(rt, self + 0xC20u, node);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x4u, 0u);
    put(rt, self + 0x8u, 0u);
}

// ---------------------------------------------------------------------------------------------------------------
// The one-off bodies.
// ---------------------------------------------------------------------------------------------------------------

// 0x08C2C4E0 (a0 = first, a1 = last, a2 = source): every 0xC-byte element from first on (stopping at last) gets
// 0x08A6A23C(element, source), the source's word at +4 at +4 and — unless the element's +8 already is the source's —
// the source's word at +8 through 0x08A5BAD8(element + 8, that word). v0 is the last call's answer, or element + 8 when
// the assign was skipped (the `addiu v0` before the branch).

// 0x08875CD0 (a0 = the owner): 0x08B796C4(owner)'s word at +4 is formatted into a 0x400-byte buffer with 0x08B78180,
// its length comes from 0x08B35F60 and, while the cursor stays inside the buffer, the trailing bytes below 0x21 are
// zeroed walking back from the end; 0x08875AD8(owner, buffer) then gets it (v0 = its answer).

// 0x088D3D10 (a0 = the object, a1 = the record): 0x088D2F9C(a0, a1), then the record's word at +0 through 0x088DA4B0;
// a null answer returns the object. Otherwise 0x088FDD68(answer) gives the float stored at +0x18 and handed to
// 0x088D390C(answer's object at +0x61C, object, object + 0xC, 6, f12 = it, f13 = 0); 0x088FD9D0(answer) stores its
// answer at +0x1C and the byte 1 at +0x20. v0 = the object.

// 0x089ABA7C (a1 = the key, a2 = the count, a3 = the source): with a positive count 0x089A354C(sp + 4, source, count)
// makes a value and its word is read in the destroy call's delay slot; 0x089AB370(sp, key) then makes the record at
// sp (asserted at 0x08CA50C4) and 0x089AAB80(record, value) gets it, then sp is destroyed.

// 0x08A13900 (a0 = the object, a2 / a3 = arguments): under 0x08B34888(object + 0x10), 0x08ACCE9C(object + 0x240, 0,
// a2, a3); a non-null answer sets the bytes at +0x1F7 / +0x1F8 and clears the four bytes at +0x1FA; v0 = the answer,
// read after 0x08B34B24 releases the guard.

// 0x08A5ECF4 (a0 = the object, a1 = the destination, a3 = the source): the 64-bit word at +0x18 of the object ANDed
// with what the object at *a3 returns from its virtual at +0x50 (v0 / v1); 0x08A5DEE8(sp, a1, low, high) builds the
// value, then it is assigned into *a1 and destroyed.

// 0x08A5EE14: the same with an exclusive OR.

// 0x08A83FB8: the exclusive OR again, with 0x08A83160 making the value and this module's assert text.

// 0x08A886C8 (a0 = the object, a1 = the destination, a3 = the source): the flag is "the object's float at +0x14 is
// <= what the object at *a3 returns from its virtual at +0x54" ($f20 is saved and restored); 0x08A97A6C(sp, flag)
// makes the value, which is then assigned into *a1 and destroyed.

// 0x08A9D8AC (a0 = the key, a1 = the object, a2 = the value): under the object's +0x4C lock, 0x08A76AB4(key,
// 0x08CF6ED8) and 0x08A7BFD0(object's word at +0x38, key, value) (asserted at 0x08CB2570); v0 = the key after the lock
// releases.

// 0x08AD156C (a0 = the object): the object holds at most four records of 80 bytes from +0x80. When the count at
// +0x1C0 is below four it is incremented (the store sits in the call's delay slot), 0x08B4E734(new, previous) runs on
// the two records and the three floats at +0x40 / +0x44 / +0x48 are copied to the new one; v0 = the new record + 0x40.
// A full object changes nothing and answers the new record's offset instead (the `addiu` the test leaves in v0).
void func_08AD156C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 count = static_cast<s32>(word(rt, self + 0x1C0u));
    const u32 next_offset = static_cast<u32>(count + 1) * 80u + 0x80u;
    ctx.gpr[2] = next_offset;
    const u32 next = self + next_offset;
    const u32 previous = self + static_cast<u32>(count) * 80u + 0x80u;
    if (count >= 4) return;
    put(rt, self + 0x1C0u, static_cast<u32>(count + 1));
    call_set(rt, ctx, 0x08B4E734u, {next, previous});
    const u32 source_fields = previous + 0x40u;
    const u32 target_fields = next + 0x40u;
    put_float(rt, target_fields, float_at(rt, source_fields));
    put_float(rt, target_fields + 0x4u, float_at(rt, source_fields + 0x4u));
    put_float(rt, target_fields + 0x8u, float_at(rt, source_fields + 0x8u));
    ctx.gpr[2] = target_fields;
}

// 0x08B018A4 (a0 = the table's owner, a1 = an index): the record is the word at +0 of a0 plus 240 bytes per index.
// When bit 7 of its u16 at +0xE2 is set, the halfword index is pushed into the container at a0 + 0x2C — the cursor at
// +0x30 is compared with +0x34, a full container goes to 0x08C47EA4(container, cursor, &half), otherwise the half is
// stored and the cursor reloaded and advanced by two (a null cursor becomes 2, as the branch-likely pair leaves it) —
// and the bits 0x2084 are cleared, that being v0. A clear bit changes nothing and answers 0.
void func_08B018A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    SavedRegisters saved(ctx, {16});
    GuestFrame frame(ctx, 0x20u);
    const u32 record = word(rt, self) + index * 240u;
    ctx.gpr[2] = 0u;
    const u32 flags = rt.memory().load16(record + 0xE2u);
    if ((flags & 0x80u) == 0u) return;
    const std::uint16_t half = static_cast<std::uint16_t>(index);
    const u32 container = self + 0x2Cu;
    const u32 cursor = word(rt, container + 0x4u);
    if (cursor == word(rt, container + 0x8u)) {
        rt.memory().store16(frame.at(0u), half);
        call_set(rt, ctx, 0x08C47EA4u, {container, cursor, frame.at(0u)});
    } else {
        if (cursor != 0u) {
            rt.memory().store16(cursor, half);
            put(rt, container + 0x4u, word(rt, container + 0x4u) + 2u);
        } else {
            put(rt, container + 0x4u, 0x2u);
        }
    }
    const u32 cleared = rt.memory().load16(record + 0xE2u) & 0xDF7Bu;
    rt.memory().store16(record + 0xE2u, static_cast<std::uint16_t>(cleared));
    ctx.gpr[2] = cleared;
}

// 0x08B10D34 (a0 = the object): the word at +0x5C's first word through 0x08B0DD88 decides; when it answers zero
// nothing else runs and v0 = 0. Otherwise the object's virtual at +0x6C(self, inner), 0x08B10CC4(self, inner),
// 0x08B10A40(inner), 0x08B0E31C(inner) and a tail call of inner's virtual at +0x24.

// 0x08B11EA0 (a0 = the object, a2 = the destination, a3 = the limit): the object's virtual at +0x9C(self),
// 0x08B17A98 and 0x08B35F60 turn into an amount; below the limit 0x08B7BB6C(destination, member) runs and s4 = 1, else
// 0; 0x08B17AA8 releases the first result and v0 = s4.

// 0x08B32138 (a1 = the output): 0x08C7AF10(sp) answers a status; a negative one leaves v0 = 0. Otherwise the word at
// sp goes to the output, the words at sp + 4 and sp + 8 to +4 and +8, the word at sp + 0x8C to +0xC and the two bytes
// at sp + 0x90 to +0x10 and +0x11 (the `lwl`/`lwr` and `swl`/`swr` pairs are aligned copies), and v0 = 1.

// 0x08B42618 (a0 = the object, a1 = the first argument, a2 = the output, a3 = a flag byte): the member at +0xB0's
// virtual at +0x3C(member, a1) answers v0 / v1, stored at the output's +8 and +0xC; 0x08B3FF90(sp, a1 + 0x12) and
// 0x08B3FA3C(sp, 0x4E9D) build the argument at sp, and the member's virtual at +0x64(member, output, sp, flag) ends it.

// 0x08B641D0: 0x08C7AAD0(0x08CB5B6C, 0x200, 0, 0) makes the node stored at 0x08D721B8; the list head at 0x08CE1F30
// becomes the node (its word at +4 the old head), the head being empty first built by 0x08B77B1C(0x08B64188).
// v0 = 0x08D721B8.
} // namespace
} // namespace new_batch_main_181
} // namespace gt::game

// 0x08B5AA30 was left out: its failing path jumps back into 0x08B62498 (an assert that does not return) and then
// into the same block again, so the body has no exit the C can mirror.
#define MAIN181(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_181::name, gt::ReturnKind::kind)
MAIN181(0x08AD156Cu, func_08AD156C, Word);
MAIN181(0x08B018A4u, func_08B018A4, Word);
MAIN181(0x08B5156Cu, func_08B5156C, Word);

// ---- from main-session batch 182 (draft 6a75b9c7ad571801b1122485d7d6b30e77483fc1) ----
// Main-session batch 182: thirty members of one family — the Adhoc attribute handlers of the script-instance tables
// (each is registered with its attribute name by func_08B87108 / func_08B878E8, batch 313's table). They are drafts, not
// verified: the differential gate decides which of them register.
//
// The family (37 instructions, prologue `addiu $sp, $sp, -0x20` in every member):
//   holder(sp + 4) = create(a1)                  the key source a1 is the caller's, the create only receives the holder
//   object = *(holder); if 0: report_null(text)
//   found = lookup(object)                       the object's own accessor
//   release(holder)                              the temporary key holder
//   value = convert(found)                       the member's own conversion
//   insert(sp + 0, value)                        a value holder built at sp
//   if a0 != sp: assign(a0, *(sp))               the destination is reached through the caller's a0
//   release(sp)                                  v0 = that release's result
// Only create, lookup, convert and the null text differ from member to member; insert is func_08A97A6C or func_08A85B1C
// (both build an integer value object), release func_08A5B9EC and assign func_08A5BAD8 in every member.
//
// Input shapes (config/synth_shapes.txt): the family's a1 is a pointer to the key holder (`lw $s0, 0x0($a1)` in the create's
// tail func_088296FC / func_08A1B4C4 / func_08838F10 …), so every member has a shape line with reg:a1=struct:16 and a
// pointer in its first word. a0 is the destination smart pointer (both read and written through assign); the generator
// always hands a0 a valid arena slot, so no shape line is needed for it.
//
// No names: none has evidence. The call targets keep their addresses; the constants come from each member's own words.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_182 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A guest call with the argument registers set as the original sets them before the `jal` (a0-a3, t0-t3 in order; what is
// not passed keeps the caller's value, which call_guest copies from ctx). Afterwards v0 holds the callee's result.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    u32 reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

constexpr u32 kReleaseHolder = 0x08A5B9ECu;  // (holder): releases the holder's object
constexpr u32 kReportNull = 0x08A5BB7Cu;     // (message): the original's null-object assert
constexpr u32 kAssignHolder = 0x08A5BAD8u;   // (holder, object): smart-pointer assignment

// What an attribute handler of this family does, with its own constants.
struct Plan {
    u32 create;     // (holder, key source): builds the key holder from the caller's a1
    u32 lookup;     // (object) → the record the member works on
    u32 convert;    // (record) → the value inserted
    u32 insert;     // (holder, value): builds the value holder at the frame's first word
    u32 null_text;  // the assert's text when the created holder's object is null
};

// The body. v0 ends as the last release's result, as the original's `jr $ra` does.
void insert_converted(Runtime &rt, AllegrexContext &ctx, const Plan &plan) {
    const u32 destination = ctx.gpr[4];
    const u32 key_source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    const u32 result = frame.at(0u);
    (void)call_set(rt, ctx, plan.create, {temporary, key_source});
    const u32 object = word(rt, temporary);
    if (object == 0u) (void)call_set(rt, ctx, kReportNull, {plan.null_text});
    const u32 found = call_set(rt, ctx, plan.lookup, {object});
    (void)call_set(rt, ctx, kReleaseHolder, {temporary});
    const u32 value = call_set(rt, ctx, plan.convert, {found});
    (void)call_set(rt, ctx, plan.insert, {result, value});
    if (destination != result) (void)call_set(rt, ctx, kAssignHolder, {destination, word(rt, result)});
    (void)call_set(rt, ctx, kReleaseHolder, {result});
}

// 0x0882F674: create 0x088297A8, lookup 0x088294D0 (`*(object + 0x18)`), convert 0x0885EA5C, insert 0x08A97A6C.
// 0x0882FCF4: the same plan, convert 0x0885EBDC.
// 0x0882FD88: convert 0x0885EBFC.
// 0x08833AFC: convert 0x08862A90.
// 0x08839204: create 0x08838FBC, lookup 0x08838DC4, convert 0x08931784, insert 0x08A85B1C, text 0x08C9BBF8.
// 0x08839298: convert 0x0893205C.
// 0x08839440: convert 0x08931790.
// 0x088394D4: convert 0x0893170C.
// 0x08839568: convert 0x089318B8.
// 0x088395FC: convert 0x089318C4.
// 0x088398D4: convert 0x089316DC.
// 0x08841C70: create 0x088418B4, lookup 0x0884177C, convert 0x0888EB90, insert 0x08A5FA8C, text 0x08C9C584.
// 0x088421FC: convert 0x0888EBBC, insert 0x08848DB8.
// 0x08842290: convert 0x0888EBC8, insert 0x0884642C.
// 0x08842324: convert 0x0888EBD4, insert 0x08845694.
// 0x08844C88: create 0x088447E8, lookup 0x08844634, convert 0x08893D14, insert 0x08A85B1C, text 0x08C9C948.
// 0x0884DD14: create 0x0884D660, lookup 0x0884D3D8, convert 0x0889779C, insert 0x08A85B1C, text 0x08C9D198.
// 0x0884E468: convert 0x08897094.
// 0x088556D8: create 0x088547C4, lookup 0x08854678, convert 0x088A6028, insert 0x08A85B1C, text 0x08C9E188.
// 0x088564B0: convert 0x088A6144.
// 0x08856544: convert 0x088A5984.
// 0x088574F0: convert 0x088A5804.
// 0x08857704: convert 0x088A7618.
// 0x08A1B720: create 0x08A1B570, lookup 0x08A1B3CC, convert 0x08A1B388, insert 0x08A85B1C, text 0x08CAA104.
// 0x08A1B7B4: convert 0x08A1B390.
// 0x08A1B848: convert 0x08A1B398.
// 0x08A1B8DC: convert 0x08A1B3A0.
// 0x08A1B970: convert 0x08A1B3A8.
// 0x08A1BA04: convert 0x08A1B3B0.
// 0x08A1BC30: convert 0x08A1B3C4.
} // namespace
} // namespace new_batch_main_182
} // namespace gt::game

#define NEW182(address, name) GT_DECOMP(address, gt::game::new_batch_main_182::name, gt::ReturnKind::Word)

// ---- from main-session batch 183 (draft fdbecc557d2408a47d8e4eea5b4e82cfb9cffa17) ----
// New batch 183 (main session): 30 emitted functions of 37-42 instructions that no src/ file registers, written by
// hand from `tools/disasm` as structured C. This is a DRAFT batch: nothing here has passed the differential gate yet.
// Role words in the comments describe access shapes; none of them is a game name (none has evidence).
//
// Conventions (the same as the later hot batches): every call passes the argument registers the original sets before
// the jal (a0-a3, t0-t3 in order, f12-f15 for the float ones) through `call_set` / `call_setf`; a register the call
// does not pass keeps the caller's value, as it does in the original. The callee-saved registers are never written here
// (the originals save and restore them), so there is no SavedRegs. The delay slot of a `jal` runs BEFORE the call; the
// delay slot of a branch-likely (`bc1fl`, `bc1tl`, `bnel`, `bgtzl`) runs only when the branch is taken (MIPS32
// semantics, which the PSP's Allegrex implements: the delay slot is nullified when the branch is not taken), so those
// effects sit inside the taken path here. v0 (and v1 / f0 where a caller could read them) is left as the original
// leaves it on every path; dead stack is not reproduced, the frames callees read or write are (`GuestFrame`).
//
// Branches the routes do not take are listed per function; each one was read against the asm by hand.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_183 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
// A `lb`: the byte sign-extended to 32 bits.
inline u32 signed_byte(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
inline s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next
// instruction sees, and a callee that sets no v0 hands the previous one on to the code after it.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address,
                             std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, address, arguments, {});
}
// The same with the argument registers written like the original writes them before the jal.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}

// ---------------------------------------------------------------------------------------------------------------
// The value-object end of a message: the temporary at frame +4 is assigned to `target` (skipped when the target IS
// that temporary) and destroyed; 0x08A5BAD8 assigns, 0x08A5B9EC destroys.
void keep_temporary(Runtime &rt, AllegrexContext &ctx, u32 target, u32 temporary) {
    if (target != temporary) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08A1C408 (a0 = the destination value object): build the value object at sp+4 through 0x08A1C370, report a null
// word through 0x08A5BB7C(0x08CAA224), unwrap it with 0x08A1C0F4, destroy the temporary, turn the result into text
// with 0x08A1C11C, build the message at sp through 0x08A85B1C(sp, text), assign it to a0 and destroy sp.
// v0 = the last destroy's. Branches: the null holder (the report), and a0 == sp (the assignment is skipped).

// ---------------------------------------------------------------------------------------------------------------
// 0x08AE074C (a0 = an index into the global object table, f12 = a float): look the object up with
// 0x08ADDCD4(a0, 0x08C91180); a null answer returns v0 = 0. Otherwise clamp the float to [0, float at 0x08CB60B0]
// (the upper bound first: a NaN fails `x < bound` and becomes the bound), scale it by the float at 0x08CB60B4,
// truncate toward zero and hand it to 0x08AD8CB0(object + 0x3C, 10, value). v0 = that call's result.
// Branches: the null lookup; the three clamp outcomes (below zero, inside, not below the upper bound). The
// `mov.s f1, f0` in the bc1fl delay slot runs only when the branch is taken, so it is the "x >= bound" answer;
// the compare's answer is not re-read where the branch was taken.
void func_08AE074C(Runtime &rt, AllegrexContext &ctx) {
    const float input = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call_set(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C91180u});
    if (object == 0u) return;
    const float upper = float_at(rt, 0x08CB60B0u);
    float value = 0.0f;
    if (!(input < upper)) {  // bc1fl taken: f1 = the upper bound in its delay slot
        value = upper;
    } else if (input < 0.0f) {  // f1 was 0.0f (0x44800800 mtc1)
        value = 0.0f;
    } else {
        value = input;  // the asm's remaining `x < upper ? x : upper`; x < upper holds here
    }
    const float scaled = psp_mul(value, float_at(rt, 0x08CB60B4u));
    const u32 count = static_cast<u32>(trunc_w_s(scaled));
    call_set(rt, ctx, 0x08AD8CB0u, {object + 0x3Cu, 0xAu, count});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B17570 (a0 = the object): construct the base with 0x08B18264(a0, sp) over a frame pair (0x08D59100, 0x8000),
// store the vtable 0x08CD91A0 at +0, initialise the six 0x2C-byte sub-objects at +0x18D4..+0x19B0 through
// 0x08B28D34, and clear +0x19FC (word), +0x19DC / +0x19DD (bytes) and +0x19E4..+0x19F0 (four words).
// v0 = the last 0x08B28D34 result (nothing after the calls writes v0).
void func_08B17570(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), 0x08D59100u);
    put(rt, frame.at(4u), 0x8000u);
    call_set(rt, ctx, 0x08B18264u, {self, frame.at(0u)});
    put(rt, self, 0x08CD91A0u);
    call_set(rt, ctx, 0x08B28D34u, {self + 0x18D4u});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x1900u});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x192Cu});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x1958u});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x1984u});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x19B0u});
    put(rt, self + 0x19FCu, 0u);
    put8(rt, self + 0x19DCu, 0u);
    put8(rt, self + 0x19DDu, 0u);
    put(rt, self + 0x19E4u, 0u);
    put(rt, self + 0x19E8u, 0u);
    put(rt, self + 0x19ECu, 0u);
    put(rt, self + 0x19F0u, 0u);
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B5AADC .. 0x08B5B2DC (a0 = a C string): the libstdc++-style report that builds a string cell at sp+4 from a0
// (0x08C74A68(temporary, a0, &byte)), allocates the 8-byte message object (0x08B61E98(8)), builds it from the cell
// (the per-function constructor), then throws it with 0x08B62498(object, message, callback) — which never returns
// (`tools.disasm` ends it without a `jr $ra`), the same message at both call sites. The cell is released on the way
// (0x08B60804(data - 4, -1), and 0x08C74884(cell, sp) when the count is not positive) unless it is the shared empty
// cell 0x08D77498.
// Note: after the first throw the asm would loop back into the release while the count stays positive; that path is
// unreachable because the callee does not return, and it is not reproduced.
void report_holder(Runtime &rt, AllegrexContext &ctx, u32 build, u32 message, u32 callback) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call_set(rt, ctx, 0x08C74A68u, {temporary, self, frame.at(1u)});
    const u32 object = call_set(rt, ctx, 0x08B61E98u, {8u});
    call_set(rt, ctx, build, {object, temporary});
    const u32 data = word(rt, temporary);
    const u32 cell = data - 0xCu;
    if (cell == 0x08D77498u) call_set(rt, ctx, 0x08B62498u, {object, message, callback});
    const u32 references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (signed_of(references) <= 0) call_set(rt, ctx, 0x08C74884u, {cell, frame.at(0u)});
    call_set(rt, ctx, 0x08B62498u, {object, message, callback});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08BC1FE8 (a0 = the object): s0 = object->+0x10, lock = s0 + 0x6C, table = s0 + 8. Lock (0x08B34888), refill
// the table with object->+0xC (0x08B2CE7C), and when 0x08B2D1A8(table) equals s0->+0x68 run 0x08B34CA0(lock);
// unlock (0x08B34B24). Then a tail jump to 0x08BBEA18(object->+0x10 + 0x98, s0).
// Branches: the equality (0x08B34CA0 is not run when they differ).

// ---------------------------------------------------------------------------------------------------------------
// 0x08C064C8 (a0 = self, a1 = the object, a2 = the extra argument): the two-table dispatch of a pointer-to-member
// call. The word at self+0x18 is the this-adjustment: with its low bit clear the first callee is self->+0x14,
// otherwise word(self->+0x14 + word(object + (adjustment >> 1))); it runs with a0 = object + (adjustment >> 1),
// a1 = object, a2 = self->+0x14. Its answer is the second adjustment: with the low bit of self+0x28 set the tail
// callee is word(self->+0x24 + word(answer + (adjustment >> 1))) and v0 / v1 keep the address sum / self->+0x24,
// otherwise the tail callee is self->+0x24 with v0 = 0 and v1 = adjustment >> 1. The tail runs with
// a0 = answer + (adjustment2 >> 1) and a1 = the extra argument.
// Branches: the virtual form of each pointer (both are taken only for an object whose table entry has the low bit).

// ---------------------------------------------------------------------------------------------------------------
// 0x08C73C00 (a0 = a value object holding a string cell, a1 = the compare's first argument, a2 = the start index,
// a3 = the compare's third argument): scan [start, length) of the string with 0x08B79998(a1, byte, a3) and return
// the index of the first byte where it answers ZERO; object->+0xC-relative length = word(word(*a0) - 0xC).
// v0 = the index, or -1 when the range is empty or no byte matched. Branches: the range test and the answer test.
void func_08C73C00(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 compare_argument = ctx.gpr[5];
    const u32 third = ctx.gpr[7];
    u32 index = ctx.gpr[6];
    u32 result = 0xFFFFFFFFu;
    u32 data = word(rt, self);
    if (index < word(rt, data - 0xCu)) {
        for (;;) {
            const u32 answer = call_set(rt, ctx, 0x08B79998u,
                                        {compare_argument, signed_byte(rt, data + index), third});
            if (answer == 0u) {
                result = index;
                break;
            }
            data = word(rt, self);
            ++index;
            if (!(index < word(rt, data - 0xCu))) {
                result = 0xFFFFFFFFu;
                break;
            }
        }
    }
    ctx.gpr[2] = result;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08C73A98 (a0 = a value object holding a string cell, a1 = the compare's first argument, a2 = the start index,
// a3 = the compare's third argument): scan [start, length) with 0x08B79998(a1, byte, a3) and return the index of the
// first byte where it answers NON-ZERO. A zero third argument short-circuits to -1 before the string is read.
// v0 = the index, or -1 when the range is empty or no byte matched. Branches: a3 == 0, the range test, the answer.

// ---------------------------------------------------------------------------------------------------------------
// 0x089C9670 (a0 = the object): destructor-shaped. Store the vtable 0x08CBB888 at +0, release the list at +0x1C
// through 0x08C012FC(a0 + 0x14, word(a0 + 0x1C)), point the pair at +0x20 / +0x24 to +0x2C, clear +0x1C / +0x24 /
// +0x28/+0x2C's tail word, run 0x089C946C(a0), release +0x50 (0x08C01C34(a0 + 0x48, word(a0 + 0x50))), destroy the
// value objects at +0x40 / +0x3C / +0x38 (0x08A5B9EC), release the block at +0x30 (0x08C00980), release the list
// again (0x08C012FC(a0 + 0x14, word(a0 + 0x1C))) and tail-jump to 0x08A780B0(a0).
// v0 = the tail call's. Branches: none.

// ---------------------------------------------------------------------------------------------------------------
// 0x08B0957C (a0 = destination, a1 = source): copy 0x24 bytes (nine words) forward from a1 to a0. The original picks
// an unaligned `lwl`/`lwr` path when either pointer is not word-aligned; both paths copy the same bytes, so only the
// result matters. v0 = the last word read (the one at source + 0x20). No branches, no calls.
void func_08B0957C(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    u32 last = 0u;
    for (u32 offset = 0u; offset <= 0x20u; offset += 4u) {
        last = word(rt, source + offset);
        put(rt, destination + offset, last);
    }
    ctx.gpr[2] = last;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x0882E98C (a0 = the destination value object): build a handle at sp+4 (0x088297A8), report a null word through
// 0x08A5BB7C(0x08C9A2F8), unwrap it (0x088294D0), destroy the temporary, chain 0x0885E710(handle) and
// 0x08917918(first, 1), build the message at sp with 0x08A85B1C(sp, second), assign it to a0 and destroy sp.
// v0 = the last destroy's. Branches: the null holder and a0 == sp (assignment skipped).

// ---------------------------------------------------------------------------------------------------------------
// 0x0897BB68 (a0 = the object): set byte +0x88 = 1, run 0x08977260(object), hand its answer to 0x089B17B8, whose
// answer goes to 0x089FD934 (which ignores a0), then 0x0897BB18(object, that answer) and 0x0897851C(object).
// Then clear the bitmap range [object->+0x74:bit at +0x78, object->+0x7C:bit at +0x80): each bit is cleared in its
// word (the word is rewritten for every bit) and the position moves to bit 0 of the next word after bit 31.
// v0 = 1 << the final bit index (sllv masks it to five bits), the value the return test computed.
// Branches: the every-0x20-bit word advance and the range-end test.

// ---------------------------------------------------------------------------------------------------------------
// 0x08B100D0 (a0 = destination, a1 / a2 handed on, a3 = a byte flag): zero a four-word header at sp, let
// 0x08B0F4E0(sp + 0x10, a1, a2, sp, flag) build a 0x3C-byte object at sp+0x10, and copy it to a0 (the asm copies
// 0x30 bytes four words at a time and then the three words at +0x30 / +0x34 / +0x38 in the order +0x38, +0x00,
// +0x04). v0 = a0. No branches.

// ---------------------------------------------------------------------------------------------------------------
// 0x08B4DFF4 (a0 = a 4x4 matrix of floats at +0x00..+0x3C): recompute it in place. The corner products are
// multiplied and subtracted as the asm does (neg.s on three inputs, mul.s five times), then +0x10 / +0x20 / +0x04 /
// +0x08 / +0x18 / +0x24 receive the words that were at +0x04 / +0x08 / +0x10 / +0x20 / +0x24 / +0x18, and +0x30 /
// +0x34 / +0x38 receive the computed values. No calls, no branches, v0 untouched.
void func_08B4DFF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 matrix = ctx.gpr[4];
    const float m00 = float_at(rt, matrix + 0x00u);
    const float m04 = float_at(rt, matrix + 0x04u);
    const float m08 = float_at(rt, matrix + 0x08u);
    const float m10 = float_at(rt, matrix + 0x10u);
    const float m14 = float_at(rt, matrix + 0x14u);
    const float m18 = float_at(rt, matrix + 0x18u);
    const float m20 = float_at(rt, matrix + 0x20u);
    const float m24 = float_at(rt, matrix + 0x24u);
    const float m28 = float_at(rt, matrix + 0x28u);
    const float m30 = float_at(rt, matrix + 0x30u);
    const float m34 = float_at(rt, matrix + 0x34u);
    const float m38 = float_at(rt, matrix + 0x38u);
    // The asm's f-register use, in order: f12 = m10, f13 = m20, f2 = -m00, f1 = -m10, f4 = -m20, f0 = m30,
    // f5 = m34, f10 = m04, f11 = m24, f6 = m14, f3 = m38, f8 = m08, f9 = m18, f1 = m28.
    const float f4 = psp_mul(m30, -m20);
    const float f2 = psp_mul(-m00, m30);
    const float f6 = psp_mul(m34, m14);
    const float f0 = psp_mul(m30, -m10);
    const float f7 = psp_mul(m34, m24);
    const float f5 = psp_mul(m04, m34);
    const float f2b = f2 - f5;
    const float f0b = f0 - f6;
    const float f1b = psp_mul(m38, m28);
    const float f5b = psp_mul(m08, m38);
    const float f4b = f4 - f7;
    const float f3b = psp_mul(m38, m18);
    put_float(rt, matrix + 0x10u, m04);
    put_float(rt, matrix + 0x20u, m08);
    const float final_30 = f2b - f5b;
    const float final_38 = f4b - f1b;
    const float final_34 = f0b - f3b;
    put_float(rt, matrix + 0x38u, final_38);
    put_float(rt, matrix + 0x30u, final_30);
    put_float(rt, matrix + 0x04u, m10);
    put_float(rt, matrix + 0x24u, m18);
    put_float(rt, matrix + 0x34u, final_34);
    put_float(rt, matrix + 0x08u, m20);
    put_float(rt, matrix + 0x18u, m24);
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B95080 (a0 = the owner, a1 = a node): walk the chain at node + 8. For each node: recurse into the subtree at
// node + 0xC (same owner), then release the holder node + 0x14: unless its cell (data - 0xC) is the shared empty
// 0x08D75298, 0x08B60804(data - 4, -1) drops the count and 0x08B93554(cell, sp) frees it when the count is not
// positive; finally 0x08B35254(node). A null node returns at once. v0 = the last 0x08B35254 result (or the caller's
// when the list is empty). Branches: the null node, the empty cell, the count test.
void func_08B95080(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    u32 node = ctx.gpr[5];
    if (node == 0u) return;
    GuestFrame frame(ctx, 0x30u);
    while (node != 0u) {
        call_set(rt, ctx, 0x08B95080u, {owner, word(rt, node + 0xCu)});
        const u32 holder = word(rt, node + 0x14u);
        const u32 next = word(rt, node + 8u);
        const u32 cell = holder - 0xCu;
        if (cell != 0x08D75298u) {
            if (signed_of(call_set(rt, ctx, 0x08B60804u, {holder - 4u, 0xFFFFFFFFu})) <= 0) {
                call_set(rt, ctx, 0x08B93554u, {cell, frame.at(0u)});
            }
        }
        call_set(rt, ctx, 0x08B35254u, {node});
        node = next;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08BDD7D8 (a0 = first record, a1 = one past the last): destroy the 0x1C-byte records of [first, last). For each
// record, with s0 = record + 4: 0x08BDD7C8(s0->+0xC, s0->+0x10, sp+1), release s0->+0xC when not null (0x08B35254),
// 0x08BDD7B8(s0->+0, s0->+4, sp) and release s0->+0 when not null. An empty range does nothing and leaves v0 alone.
// v0 = the last 0x08B35254 result (or the last 0x08BDD7B8 when the final head word is null).

// ---------------------------------------------------------------------------------------------------------------
// 0x0887F3B4 (a0 = the object): destructor-shaped. Store 0x08CC1130 at +0 and 0x08CC116C at +0x70, run
// 0x0886AA0C(a0, 5), (a0, 2), (a0, 4) and (a0, 3), then when +0x78 is not null call its first virtual with
// a1 = word(a0 + 0x74) and clear +0x78 / +0x74; store 0x08CC1180 at +0x70 and tail-jump to 0x0886A858(a0).
// v0 = the tail call's. Branches: the null +0x78 (the virtual call and both clears are skipped).

// ---------------------------------------------------------------------------------------------------------------
// 0x08ABA868 (a0 = the object): allocate one 0x80-byte block per 0x08C1A1B8(0x08CFFA70) entry
// (0x08B2B4D8(0x80, count << 7)), register it with 0x08B2B760(0x08CFFA54, block, size, 0x80) and 0x08AD3E70(),
// set byte +0x4C = 1, then build the two-word record at sp (handler 0x08ABAD68, this) and register it:
// 0x08B2E314(6, -0x40), 0x08B2E134(that) and 0x08B1B96C(sp, that, 0), stored at +0x40. v0 = that last call's.
// No branches.

// ---------------------------------------------------------------------------------------------------------------
// 0x08C73920 (a0 = a value object holding a string cell, a1 = the compare's first argument, a2 = the start index,
// a3 = the count): the backward scan of the same family as 0x08C73A98 / 0x08C73C00. With length = word(*a0 - 0xC),
// a count larger than the length returns -1 at once. Otherwise the start is min(a2, length - count) and the loop
// calls 0x08B36170(a1, byte, a3) at each position from there down to 0, returning the position where it answers
// ZERO; -1 when none did. v0 = the position or -1. Branches: the length test, the answer test, the position test.
// The current position is kept in a C local instead of the original's sp+0x10 slot (dead stack, not compared).

// ---------------------------------------------------------------------------------------------------------------
// The three 0x08C538xx helpers share a shape: f1 = 0.0f; 0x08C5250C clamps the object's input float to [-1, 1];
// its answer is stored at the FIRST output pointer in the delay slot of the next call, 0x08B499B8 turns it into an
// angle; a zero angle stores 0.0f at the SECOND output and one 0x08B4A430 product goes to the THIRD, otherwise two
// 0x08B4A430 products do (the second output first). The `neg.s` in a jal delay slot negates the float before the
// call (the delay slot runs before it). 0x08B4A430(a, b) multiplies two floats.

// 0x08C5380C (a0 = the object, a1 / a2 / a3 = three float outputs): input at +0x14; at a non-zero angle the outputs
// are 0x08B4A430(-+0x08, +0x20) and 0x08B4A430(-+0x0C, +0x10); at zero, 0x08B4A430(+0x04, +0x00). Nothing writes v0.
void func_08C5380C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first_out = ctx.gpr[5];
    const u32 second_out = ctx.gpr[6];
    const u32 third_out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const float clamped = call_setf(rt, ctx, 0x08C5250Cu, {}, {float_at(rt, self + 0x14u)}).f0;
    put_float(rt, first_out, clamped);
    const float angle = call_setf(rt, ctx, 0x08B499B8u, {}, {clamped}).f0;
    if (angle == 0.0f) {
        put_float(rt, second_out, 0.0f);
        const float product = call_setf(rt, ctx, 0x08B4A430u, {},
                                        {float_at(rt, self + 0x04u), float_at(rt, self + 0x00u)}).f0;
        put_float(rt, third_out, product);
        return;
    }
    const float a = call_setf(rt, ctx, 0x08B4A430u, {},
                              {-float_at(rt, self + 0x08u), float_at(rt, self + 0x20u)}).f0;
    put_float(rt, second_out, a);
    const float b = call_setf(rt, ctx, 0x08B4A430u, {},
                              {-float_at(rt, self + 0x0Cu), float_at(rt, self + 0x10u)}).f0;
    put_float(rt, third_out, b);
}

// 0x08C538B4 (a0 = the object, a1 = the object again as the third output, a2 = the second output, a3 = the first):
// input at -+0x0C; at a non-zero angle the outputs are 0x08B4A430(+0x18, +0x00) and 0x08B4A430(+0x14, +0x10); at
// zero, 0x08B4A430(-+0x1C, +0x20). Nothing writes v0.
void func_08C538B4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 third_out = ctx.gpr[5];
    const u32 second_out = ctx.gpr[6];
    const u32 first_out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const float clamped = call_setf(rt, ctx, 0x08C5250Cu, {}, {-float_at(rt, self + 0x0Cu)}).f0;
    put_float(rt, first_out, clamped);
    const float angle = call_setf(rt, ctx, 0x08B499B8u, {}, {clamped}).f0;
    if (angle == 0.0f) {
        put_float(rt, second_out, 0.0f);
        const float product = call_setf(rt, ctx, 0x08B4A430u, {},
                                        {-float_at(rt, self + 0x1Cu), float_at(rt, self + 0x20u)}).f0;
        put_float(rt, third_out, product);
        return;
    }
    const float a = call_setf(rt, ctx, 0x08B4A430u, {},
                              {float_at(rt, self + 0x18u), float_at(rt, self + 0x00u)}).f0;
    put_float(rt, second_out, a);
    const float b = call_setf(rt, ctx, 0x08B4A430u, {},
                              {float_at(rt, self + 0x14u), float_at(rt, self + 0x10u)}).f0;
    put_float(rt, third_out, b);
}

// 0x08C5395C (a0 = the object, a1 = the first output, a2 = the third output, a3 = the second output): input at
// -+0x1C; at a non-zero angle the outputs are 0x08B4A430(+0x04, +0x10) and 0x08B4A430(+0x18, +0x20); at zero,
// 0x08B4A430(-+0x08, +0x00). Nothing writes v0.
void func_08C5395C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first_out = ctx.gpr[5];
    const u32 third_out = ctx.gpr[6];
    const u32 second_out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const float clamped = call_setf(rt, ctx, 0x08C5250Cu, {}, {-float_at(rt, self + 0x1Cu)}).f0;
    put_float(rt, first_out, clamped);
    const float angle = call_setf(rt, ctx, 0x08B499B8u, {}, {clamped}).f0;
    if (angle == 0.0f) {
        put_float(rt, second_out, 0.0f);
        const float product = call_setf(rt, ctx, 0x08B4A430u, {},
                                        {-float_at(rt, self + 0x08u), float_at(rt, self + 0x00u)}).f0;
        put_float(rt, third_out, product);
        return;
    }
    const float a = call_setf(rt, ctx, 0x08B4A430u, {},
                              {float_at(rt, self + 0x04u), float_at(rt, self + 0x10u)}).f0;
    put_float(rt, second_out, a);
    const float b = call_setf(rt, ctx, 0x08B4A430u, {},
                              {float_at(rt, self + 0x18u), float_at(rt, self + 0x20u)}).f0;
    put_float(rt, third_out, b);
}

}  // namespace
}  // namespace new_batch_main_183
}  // namespace gt::game

#define NEW183(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_183::name, gt::ReturnKind::kind)
NEW183(0x08AE074Cu, func_08AE074C, Word);
NEW183(0x08B0957Cu, func_08B0957C, Word);
NEW183(0x08B17570u, func_08B17570, Word);
NEW183(0x08B4DFF4u, func_08B4DFF4, Word);
NEW183(0x08B95080u, func_08B95080, Word);
NEW183(0x08C5380Cu, func_08C5380C, Word);
NEW183(0x08C538B4u, func_08C538B4, Word);
NEW183(0x08C5395Cu, func_08C5395C, Word);
NEW183(0x08C73C00u, func_08C73C00, Word);

// ---- from main-session batch 184 (draft de1925ab72a27e744a1462626a54ec58f139ef30) ----
// New batch main 184 (main session): functions no src/ file defines or registers, written by hand from
// `tools.disasm` as structured C. Draft batch, not yet verified by the differential gate.
// Three shapes dominate and share a helper each:
//  - the *conditional copy assignment* (14 instances, 0x30-byte frame): a0 = the destination holder, a1 = a source
//    holder whose word is the object; an accessor builds a handle from (a0, 0), a comparison call decides whether the
//    source word moves into the destination, and a temporary built on the stack is moved in when the comparison
//    answers zero. v0 = 1 when something was moved in, else 0.
//  - the *angle pair builders* (four instances at 0x08C53A04..0x08C53BFC): a0 = a parameter block of floats, three
//    output floats; one call clamps a value to [-1, 1], the next completes the angle, and a zero angle writes one
//    output to 0 and computes one angle, a non-zero one computes two.
//  - the *constructors/destructors* at 0x0887AFB4, 0x089C9528, 0x08A13E70/0x08A13F1C and 0x08A142C8: a vtable store,
//    a run of member calls and a tail call (the original's `j target` after the frame is popped).
// Skipped here: 0x08B7B2E4, 0x08B7B34C and 0x08B17104 are setjmp/longjmp-shaped (they store or reload s0-s7, sp, fp,
// ra and f20-f31 in a record), which does not decompile into a call without modelling the guest's own return address.
// No names: none of these has evidence. Branches the routes may not take were compared with the asm by hand; the
// delay-slot notes are on each body.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_184 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same leaving v0/v1/f0 as the callee left them (the float helpers below read the callee's f0 and keep v0/v1).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// --- the conditional copy assignment (0x08A5BAD8 / 0x08A76F20 / 0x08A76AA0 / 0x08A5B9EC) ---
// a0 = the destination holder, a1 = the source holder. The source word at +0 is the object: null moves null into the
// destination and answers 1. Otherwise `accessor(a0, 0)` builds a handle, 0x08A76F20(object, handle) answers whether
// the source's object can be adopted (non-zero: 0x08A5BAD8(destination, source word) and 1), and the fallback builds
// a temporary at sp (0x08A76AA0), moves its word into the destination when it is not null (answering 1) and releases
// it (0x08A5B9EC). v0 = the flag the original keeps in s3.
void assign_from_holder(Runtime &rt, AllegrexContext &ctx, u32 accessor) {
    GuestFrame frame(ctx, 0x30u);
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 object = word(rt, source);
    if (object == 0u) {
        call(rt, ctx, 0x08A5BAD8u, {destination, 0u});
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 handle = call(rt, ctx, accessor, {destination, 0u});
    if (call(rt, ctx, 0x08A76F20u, {object, handle}) != 0u) {
        call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, source)});
        ctx.gpr[2] = 1u;
        return;
    }
    // 08A76AA0 sets its own a1 (lui a1, 0x08CF6ED8): the original leaves the previous callee's a1, which it ignores.
    call(rt, ctx, 0x08A76AA0u, {frame.at(0)});
    u32 moved = 0u;
    if (word(rt, frame.at(0)) != 0u) {
        call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0))});
        moved = 1u;
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
    ctx.gpr[2] = moved;
}

// --- the vtable constructors/destructors ---

// 0x08B54104 (VFPU): v0 = the bit pattern of |m * (a0, 1) transformed by E100 twice| and f0 = f12 * that length.
// a0 = three words (a 3-vector), a1 = a 4x4 matrix in VFPU M200, a2 = a 4x4 matrix in VFPU M100. The result is the
// distance between the two transformed points, scaled by the incoming f12 (mul.s in the `jr $ra` delay slot).
void func_08B54104(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4];
    const u32 first_matrix = ctx.gpr[5];
    const u32 second_matrix = ctx.gpr[6];
    for (unsigned column = 0u; column < 4u; ++column)
        gt::vfpu::load_column(rt, ctx, 2u, column, first_matrix + 0x10u * column);
    // lwl/lwr pairs: v0 = the word at a0, v1 = the word at a0 + 4 (they also land in those GPRs), a1 = a0 + 8.
    gt::vfpu::move_to(ctx, rt.memory().load32(vector), 0x01u);        // mtv v0, S010
    gt::vfpu::move_to(ctx, rt.memory().load32(vector + 4u), 0x21u);   // mtv v1, S011
    gt::vfpu::move_to(ctx, rt.memory().load32(vector + 8u), 0x41u);   // mtv a1, S012
    gt::vfpu::transform(ctx, 2u, 0x28u, 1u, 4u, 3u);                  // vhtfm4.q C020, E200, C010
    for (unsigned column = 0u; column < 4u; ++column)
        gt::vfpu::load_column(rt, ctx, 1u, column, second_matrix + 0x10u * column);
    gt::vfpu::transform(ctx, 1u, 0x24u, 2u, 4u, 4u);                  // vtfm4.q C010, E100, C020
    gt::vfpu::binary(ctx, gt::vfpu::Binary::Add, 2u, 2u, 0u, 4u);     // vadd.q C020, C020, C000
    gt::vfpu::unary(ctx, 16u, 3u, 0x61u, 1u);                         // vrcp.s S030, S013
    gt::vfpu::transform(ctx, 0u, 0x24u, 2u, 4u, 4u);                  // vtfm4.q C000, E100, C020
    gt::vfpu::scale(ctx, 1u, 1u, 3u, 3u);                            // vscl.t C010, C010, S030
    gt::vfpu::scale(ctx, 0u, 0u, 3u, 3u);                            // vscl.t C000, C000, S030
    gt::vfpu::binary(ctx, gt::vfpu::Binary::Sub, 1u, 1u, 0u, 3u);     // vsub.t C010, C010, C000
    gt::vfpu::dot(ctx, 1u, 1u, 1u, 3u);                               // vdot.t S010, C010, C010
    gt::vfpu::unary(ctx, 22u, 1u, 1u, 1u);                            // vsqrt.s S010, S010
    ctx.gpr[2] = ctx.vfpu_scalar_bits(1u);                            // mfv v0, S010
    ctx.fpr[1] = std::bit_cast<float>(ctx.gpr[2]);                    // mtc1 v0, f1
    ctx.fpr[0] = psp_mul(ctx.fpr[12], ctx.fpr[1]);                    // mul.s f0, f12, f1 (jr $ra delay slot)
}

// 0x0887AFB4 (a0 = an object): the vtable 0x08CBFDA8, a 0x56C-byte record allocated (0x08B62344) and built
// (0x0887AE54) into +4, and the global 0x08D753F0 stored at +8. When the byte at 0x08D753E8 is zero, 0x08B6441C
// decides: non-zero stores the shared 0x08CBFE28 at 0x08D753F0 and runs 0x08B64494 on it either way.
// v0 = the last call's result, or the address pair's `lui` half (0x08D70000) when the byte was already set.
void func_0887AFB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CBFDA8u);
    const u32 record = call(rt, ctx, 0x08B62344u, {0x56Cu});
    call(rt, ctx, 0x0887AE54u, {record});
    put(rt, self + 4u, record);
    u32 shared = 0x08D753F0u;
    if (static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(0x08D753E8u))) != 0) {
        // The `lui` half of the address pair above stays in v0 on this path (the recompiler's `lui 0x8D7`).
        ctx.gpr[2] = 0x08D70000u;
    } else {
        const u32 created = call(rt, ctx, 0x08B6441Cu, {0x08D753E8u});
        if (created != 0u) {
            put(rt, 0x08D753F0u, 0x08CBFE28u);
            ctx.gpr[2] = call(rt, ctx, 0x08B64494u, {0x08D753E8u});
            shared = 0x08D753F0u;
        } else {
            ctx.gpr[2] = created;
        }
    }
    put(rt, self + 8u, shared);
}

// 0x0881EF0C (a0 = the output holder, a1 = a source the first callee keeps): 0x0881BD98 builds a handle at sp
// (checked against 0x08C98F48), 0x0881B068 resolves it, 0x0881AA7C reads a value, 0x08A950D0 builds a 0x10-byte
// record at sp+4 from that value, and 0x0881B324 takes the resolved object with the record's word at +0x14. The
// output is released (0x08A5BAD8) when it is not sp+4, then the record (0x08A5B9EC): v0 = that release's answer.
// `lw s0, 0x0(sp)` (tools.sp_loads) is the frame slot the first callee filled, not a register reload: it is the
// `word(rt, handle)` below.

// 0x088D0CCC (a0 = the output object, a1 = an index the first callee reads, a2 = a value): 0x088DCD28(a2, a1) names
// an object; its virtual at vtable +0x8C gives a count, and for every index below it the virtual at vtable +0x90
// runs: a non-zero answer increments the word at +0x5C of the output, a zero answer lowers the flag (s4) to 0. The
// byte at +0x5B becomes the flag, and v0 = the last virtual answer or the incremented counter.

// 0x088E649C (a0 = a holder, a1 = a byte): with the lock 0x08B344E0 taken on the first object's block at +0xEC, its
// virtual at vtable +0x50 (a1 = -1) answers an object whose byte at +0xA takes the value; unlocked with 0x08B3457C.
// When the second object at +4 exists the same runs on its block, and the work continues in the tail call
// 0x08B3457C on that block. v0 = 0 on the early return (the loaded +4 word).

// 0x089C9528 (a0 = object): the destructor. Store the vtable 0x08CBB888, trim the array at +0x14 through
// 0x08C012FC, point its words at +0x10 / +0xC at +0x18 and clear +8 / +0x14, run 0x089C946C, release the arrays at
// +0x48 (0x08C01C34), +0x40, +0x3C and +0x38, release +0x30 (0x08C00980), trim +0x14 again, 0x08A780B0, and
// continue in the tail call 0x08A5BC54(a0, 0x80, 4).

// 0x08A13E70 and 0x08A13F1C (byte-identical bodies; a0 = object): store the vtable 0x08CD0418,
// 0x08ACF184(word(0x08CFFAAC), 0), the byte at +0x41 becomes 1, then 0x08A1371C, 0x08B28C74(+0x10),
// 0x08B1B99C(word(+0x3C)), 0x08A12AE0, 0x08ACEF04(+0x240), 0x08B28BE0(+0x200), 0x08A19DD4(+0x178),
// 0x08A19DD4(+0x140), 0x08B241A0(+0xA0), 0x08C15D30(+0x5C), 0x08B28BE0(+0x10), and continue in the tail call
// 0x089C16C8(a0).
void construct_with_shared_registry(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 sub = self + 0x10u;
        put(rt, self, 0x08CD0418u);
        call(rt, ctx, 0x08ACF184u, {word(rt, 0x08CFFAACu), 0u});
        rt.memory().store8(self + 0x41u, 1u);
        call(rt, ctx, 0x08A1371Cu, {self});
        call(rt, ctx, 0x08B28C74u, {sub});
        call(rt, ctx, 0x08B1B99Cu, {word(rt, self + 0x3Cu)});
        call(rt, ctx, 0x08A12AE0u, {self});
        call(rt, ctx, 0x08ACEF04u, {self + 0x240u});
        call(rt, ctx, 0x08B28BE0u, {self + 0x200u});
        call(rt, ctx, 0x08A19DD4u, {self + 0x178u});
        call(rt, ctx, 0x08A19DD4u, {self + 0x140u});
        call(rt, ctx, 0x08B241A0u, {self + 0xA0u});
        call(rt, ctx, 0x08C15D30u, {self + 0x5Cu});
        call(rt, ctx, 0x08B28BE0u, {sub});
    }
    tail_call(ctx, 0x089C16C8u, {self});
}

// 0x08A142C8 (a0 = object, a1 = a string cell): lock 0x08B34888(object + 0xAC); the word at +0x108 is compared and
// set to 1 with an ll/sc pair, and a non-zero word yields the lock back through 0x08B34B2C first. 0x08B7C830
// copies 0x1F bytes of the cell (the word at +(a1)) into +0x18D, 0x08B34B24 unlocks, 0x08B28C74 runs on +0xD8, and the
// work continues in the tail call 0x08B28C44(object + 0xAC).

// 0x08C53A04 (a0 = the parameter block, a1/a2/a3 = float outputs): a2 takes the clamped value, a3 the second angle
// on the non-zero path, and a1 the final angle on both paths. f0 = the last 0x08B4A430 result.
void func_08C53A04(Runtime &rt, AllegrexContext &ctx) {
    const u32 params = ctx.gpr[4];
    const u32 out_final = ctx.gpr[5];
    const u32 out_first = ctx.gpr[6];
    const u32 out_zeroed = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const GuestResult clamped = call_keepf(rt, ctx, 0x08C5250Cu, {}, {float_at(rt, params + 0x18u)});
    put_float(rt, out_first, clamped.f0);  // stored in the second call's delay slot, i.e. before it runs
    const GuestResult completed = call_keepf(rt, ctx, 0x08B499B8u, {}, {clamped.f0});
    if (completed.f0 == 0.0f) {
        put_float(rt, out_zeroed, 0.0f);  // the likely branch's delay slot
        (void)call_keepf(rt, ctx, 0x08B4A430u, {},
                         {float_at(rt, params + 0x14u), float_at(rt, params + 0x10u)});
        put_float(rt, out_final, ctx.fpr[0]);
        return;
    }
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, params + 0x0Cu), float_at(rt, params + 0x00u)});
    put_float(rt, out_zeroed, ctx.fpr[0]);
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, params + 0x1Cu), float_at(rt, params + 0x20u)});
    put_float(rt, out_final, ctx.fpr[0]);  // the `j` to the epilogue's delay slot
}

// 0x08C53AAC (a0 = the parameter block, a1/a2/a3 = float outputs): a1 takes the clamped value, a2 the second angle on
// the non-zero path, and a3 the final angle on both paths.
void func_08C53AAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 params = ctx.gpr[4];
    const u32 out_first = ctx.gpr[5];
    const u32 out_second = ctx.gpr[6];
    const u32 out_final = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const GuestResult clamped = call_keepf(rt, ctx, 0x08C5250Cu, {}, {float_at(rt, params + 0x18u)});
    put_float(rt, out_first, clamped.f0);
    const GuestResult completed = call_keepf(rt, ctx, 0x08B499B8u, {}, {clamped.f0});
    if (completed.f0 == 0.0f) {
        put_float(rt, out_second, 0.0f);
        (void)call_keepf(rt, ctx, 0x08B4A430u, {},
                         {float_at(rt, params + 0x04u), float_at(rt, params + 0x00u)});
        put_float(rt, out_final, ctx.fpr[0]);
        return;
    }
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, params + 0x08u), float_at(rt, params + 0x28u)});
    put_float(rt, out_second, ctx.fpr[0]);
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, params + 0x10u), float_at(rt, params + 0x14u)});
    put_float(rt, out_final, ctx.fpr[0]);
}

// 0x08C53B54 (a0 = the parameter block, a1/a2/a3 = float outputs): a3 takes the clamped value (negated before the
// first call), a2 the second angle on the non-zero path, and a1 the final angle on both paths.
void func_08C53B54(Runtime &rt, AllegrexContext &ctx) {
    const u32 params = ctx.gpr[4];
    const u32 out_final = ctx.gpr[5];
    const u32 out_second = ctx.gpr[6];
    const u32 out_first = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const GuestResult clamped = call_keepf(rt, ctx, 0x08C5250Cu, {}, {-float_at(rt, params + 0x10u)});
    put_float(rt, out_first, clamped.f0);
    const GuestResult completed = call_keepf(rt, ctx, 0x08B499B8u, {}, {clamped.f0});
    if (completed.f0 == 0.0f) {
        put_float(rt, out_second, 0.0f);
        (void)call_keepf(rt, ctx, 0x08B4A430u, {},
                         {-float_at(rt, params + 0x24u), float_at(rt, params + 0x28u)});
        put_float(rt, out_final, ctx.fpr[0]);
        return;
    }
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {float_at(rt, params + 0x20u), float_at(rt, params + 0x00u)});
    put_float(rt, out_second, ctx.fpr[0]);
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {float_at(rt, params + 0x18u), float_at(rt, params + 0x14u)});
    put_float(rt, out_final, ctx.fpr[0]);
}

// 0x08C53BFC (a0 = the parameter block, a1/a2/a3 = float outputs): a1 takes the clamped value (negated before the
// first call), a3 the second angle on the non-zero path, and a2 the final angle on both paths.
void func_08C53BFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 params = ctx.gpr[4];
    const u32 out_first = ctx.gpr[5];
    const u32 out_final = ctx.gpr[6];
    const u32 out_second = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const GuestResult clamped = call_keepf(rt, ctx, 0x08C5250Cu, {}, {-float_at(rt, params + 0x24u)});
    put_float(rt, out_first, clamped.f0);
    const GuestResult completed = call_keepf(rt, ctx, 0x08B499B8u, {}, {clamped.f0});
    if (completed.f0 == 0.0f) {
        put_float(rt, out_second, 0.0f);
        (void)call_keepf(rt, ctx, 0x08B4A430u, {},
                         {-float_at(rt, params + 0x08u), float_at(rt, params + 0x00u)});
        put_float(rt, out_final, ctx.fpr[0]);
        return;
    }
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {float_at(rt, params + 0x04u), float_at(rt, params + 0x14u)});
    put_float(rt, out_second, ctx.fpr[0]);
    (void)call_keepf(rt, ctx, 0x08B4A430u, {}, {float_at(rt, params + 0x20u), float_at(rt, params + 0x28u)});
    put_float(rt, out_final, ctx.fpr[0]);
}

// --- the conditional copy assignments, one per accessor ---
void func_0880A698(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x0880A530u); }
void func_08835334(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x088350ACu); }
void func_089BA138(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x089B9E9Cu); }
void func_089C218C(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x089C16F4u); }
void func_089ED128(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x089ECFA8u); }
void func_089F5574(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x089F5478u); }
void func_089F7DB0(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x089F789Cu); }
void func_08A0817C(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x08A06E50u); }
void func_08A2D22C(Runtime &rt, AllegrexContext &ctx) { assign_from_holder(rt, ctx, 0x08A2D114u); }

} // namespace
} // namespace new_batch_main_184
} // namespace gt::game

#define MAIN184(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_184::name, gt::ReturnKind::kind)
MAIN184(0x0880A698u, func_0880A698, Word);
MAIN184(0x08835334u, func_08835334, Word);
MAIN184(0x0887AFB4u, func_0887AFB4, Word);
MAIN184(0x089BA138u, func_089BA138, Word);
MAIN184(0x089C218Cu, func_089C218C, Word);
MAIN184(0x089ED128u, func_089ED128, Word);
MAIN184(0x089F5574u, func_089F5574, Word);
MAIN184(0x089F7DB0u, func_089F7DB0, Word);
MAIN184(0x08A0817Cu, func_08A0817C, Word);
MAIN184(0x08A2D22Cu, func_08A2D22C, Word);
MAIN184(0x08B54104u, func_08B54104, Float);
MAIN184(0x08C53A04u, func_08C53A04, Float);
MAIN184(0x08C53AACu, func_08C53AAC, Float);
MAIN184(0x08C53B54u, func_08C53B54, Float);
MAIN184(0x08C53BFCu, func_08C53BFC, Float);

// ---- from main-session batch 185 (draft c95bf69783f579e4f5283355a02754a7750bcd97) ----
// New batch main 185: thirty functions written by hand from `tools/disasm` as structured C. DRAFT: the
// differential gate has not run them (their commands could not build useful inputs before); config/synth_shapes.txt
// (kept with the batch, not in the repo) lays out the argument struct each one reads.
//
// Two shapes are one helper with the instance's call targets in the wrapper:
//   * holder_cast — the Adhoc "cast a holder to a class" template, 14 instances (as_class.cpp holds the same
//     template under the gt::adhoc namespace for the instances whose routes reach it; these are the ones no
//     route reached).
//   * dispatch_pair — the member-pointer dispatch over two (offset, function) pairs, 5 instances.
//
// Every call passes the argument registers the original sets before the `jal` (`call` writes them into ctx, so a
// register the original leaves holding an earlier value keeps the caller's). A `jal`'s delay slot runs BEFORE the
// callee and a `beql`/`bnel`/`bgtz` delay slot runs before its branch is decided; each is written where it runs.
// v0 is set as the original leaves it on every exit path: a path ending in a call keeps that call's v0, a path that
// never writes v0 keeps the caller's. The callee-saved registers are never written here, as the originals restore
// them; the frames callees read or write are modelled with GuestFrame at the original's size.
//
// No names: none of these has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_185 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

constexpr u32 kHolderReset = 0x08A5BAD8u;    // (holder *out, object *): out = object
constexpr u32 kHolderDestroy = 0x08A5B9ECu;  // (holder *)
constexpr u32 kIsInstanceOf = 0x08A76F20u;   // (object *, class *) -> bool
constexpr u32 kNilHolder = 0x08A76AA0u;      // (holder *out): out = the global nil

// Calls the guest function with the argument registers the original sets before its `jal`, and leaves its v0 in v0
// (a callee that never writes v0 answers with what it was called with).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// --- 0x0880881C family: the holder cast template (14 instances) ------------------------------------------------
// out = a0 (holder *), source = a1 (const holder *). object = *source; an empty source resets out to nothing and
// answers 1. Otherwise the class getter runs on out (its a1 is 0) and, when the object is an instance of that class,
// out is reset to *source and the answer is 1. Otherwise the fallback makes a temporary at sp (the nil holder for
// every instance but 0x08A82EC0, whose fallback takes the source): out takes the temporary's value when it holds
// something — the answer is 1 then, else 0 — and the temporary is destroyed. v0 = the answer.
u32 holder_cast(Runtime &rt, AllegrexContext &ctx, u32 class_of, u32 fallback, bool fallback_takes_source) {
    GuestFrame frame(ctx, 0x30u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 object = word(rt, source);
    if (object == 0u) {
        call(rt, ctx, kHolderReset, {out, 0u});
        ctx.gpr[2] = 1u;
        return ctx.gpr[2];
    }
    const u32 cls = call(rt, ctx, class_of, {out, 0u});
    if (call(rt, ctx, kIsInstanceOf, {object, cls}) != 0u) {
        call(rt, ctx, kHolderReset, {out, word(rt, source)});
        ctx.gpr[2] = 1u;
        return ctx.gpr[2];
    }
    call(rt, ctx, fallback, {frame.at(0u), fallback_takes_source ? source : cls});
    u32 holds = 0u;
    if (const u32 made = word(rt, frame.at(0u)); made != 0u) {
        call(rt, ctx, kHolderReset, {out, made});
        holds = 1u;
    }
    call(rt, ctx, kHolderDestroy, {frame.at(0u)});
    ctx.gpr[2] = holds;
    return holds;
}

// --- 0x08BEB9E4 family: the two-pair member-pointer dispatch (5 instances) -------------------------------------
// pairs = a1: packed offset at +0x18 / +0x20, function word at +0x14 / +0x1C. Bit 0 of the packed word set means the
// function word is a vtable displacement, resolved on the object at base + (offset >> 1) (first; base = a2) or at
// first_result + (offset >> 1) (second); clear means the word is the function. Both calls get a0 = the object and
// a1 = its function word (the second also finds the first result in a1). The last call gets a0 = a0 (the output
// holder) and a1 = the second result, and v0 = that holder however the calls went.
u32 dispatch_pair(Runtime &rt, AllegrexContext &ctx, u32 last_call) {
    GuestFrame frame(ctx, 0x10u);
    const u32 holder = ctx.gpr[4];
    const u32 pairs = ctx.gpr[5];
    const u32 base = ctx.gpr[6];
    const u32 first_offset = word(rt, pairs + 0x18u);
    const u32 first_object = base + static_cast<u32>(static_cast<s32>(first_offset) >> 1);
    const u32 first_function = word(rt, pairs + 0x14u);
    const u32 first_target =
        (first_offset & 1u) != 0u ? word(rt, word(rt, first_object) + first_function) : first_function;
    const u32 first_result = call_indirect(rt, ctx, first_target, {first_object, first_function});
    const u32 second_offset = word(rt, pairs + 0x20u);
    const u32 second_object = first_result + static_cast<u32>(static_cast<s32>(second_offset) >> 1);
    const u32 second_function = word(rt, pairs + 0x1Cu);
    const u32 second_target =
        (second_offset & 1u) != 0u ? word(rt, word(rt, second_object) + second_function) : second_function;
    const u32 second_result = call_indirect(rt, ctx, second_target, {second_object, first_result});
    call(rt, ctx, last_call, {holder, second_result});
    ctx.gpr[2] = holder;
    return holder;
}

void func_08A2E4C8(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A2DDACu, kNilHolder, false); }
void func_08A52264(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A52164u, kNilHolder, false); }
void func_08A5973C(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A5963Cu, kNilHolder, false); }
void func_08A59C00(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A59B00u, kNilHolder, false); }
void func_08A5A0C8(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A59FC8u, kNilHolder, false); }
void func_08A8A3B8(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A8A178u, kNilHolder, false); }
void func_08A8C1EC(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A8BF90u, kNilHolder, false); }
void func_08A9B8FC(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A9B7A4u, kNilHolder, false); }
// 0x08A82EC0: the one instance whose fallback takes the source instead of the class.
void func_08A82EC0(Runtime &rt, AllegrexContext &ctx) { holder_cast(rt, ctx, 0x08A82A68u, 0x08AA23A8u, true); }


// 0x08A2D434 (a0 = object passed to the reader, a1 = a value): the reader 0x08B0979C runs first and its answer is
// 0x08A2D338's a0 when that builds a handle at sp from the value (a null handle hits the assert at 0x08A5BB7C with
// the table 0x08CAC2A8). 0x08A2D1B8 reads the handle's object, 0x08B0A034 combines it with the reader's answer and
// 0x08A97A6C stores that in a record at sp+4; the output holder takes the record unless it is the record, then the
// record and the handle are destroyed. v0 = the last destruction's answer.

// 0x08A9AF28 (a0 = an object whose vector starts at +0x14, a1 = a wanted count): round the capacity up from 1 by
// doubling until it covers a1 + 1 (the count a1 + 1 is formed before the frame), build a nil temporary at sp, and
// resize the vector at +0x14: 0x08B85798 erases [begin + capacity, end) when the capacity is below the element count,
// else 0x08B85F30 inserts capacity - count nils at the end. v0 = the temporary's destruction.

// 0x08AD83B8 (a0 = a 0x400-byte destination, a1 = a text, a2/a3/t0/t1 = the first varargs): the text after its
// first '/' (0x08B7C9E4 with 0x2F; the text itself when absent), then 0x08B37640 formats into sp with the table
// 0x08C9084C, the destination, that position and the four varargs (t2/t3 go to the va area at sp+0x820 with t0/t1).
// 0x08B373EC finishes the text into sp+0x400 with the va area and 0x08AD8464 reads it. v0 = the last answer.

// 0x08B22204 (a0 = the object, a1/a2 = two members): a constructor. The vtable 0x08CD9538 at +0, the member at +4
// built by 0x08B28DCC, the two arguments at +0x2C / +0x30, the members at +0x34 (0x08C4FB18 with the object) and +0x40
// (0x08C4FB30), the vtable words 0x08D68B7C at +0x5C / +0x60 and zeros at +0x64..+0x6C. With a non-zero member at
// +0x30 the work ends in the tail call 0x08B221B4(member, object); otherwise v0 = 0x08D68B7C.
void func_08B22204(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    u32 member;
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CD9538u);
        call(rt, ctx, 0x08B28DCCu, {self + 4u, first});
        put(rt, self + 0x2Cu, first);
        put(rt, self + 0x30u, second);
        call(rt, ctx, 0x08C4FB18u, {self + 0x34u, self});
        call(rt, ctx, 0x08C4FB30u, {self + 0x40u});
        member = word(rt, self + 0x30u);
        put(rt, self + 0x60u, 0x08D68B7Cu);
        put(rt, self + 0x5Cu, 0x08D68B7Cu);
        put(rt, self + 0x64u, 0u);
        put(rt, self + 0x68u, 0u);
        put(rt, self + 0x6Cu, 0u);
    }
    if (member != 0u) tail_call(ctx, 0x08B221B4u, {member, self});
    ctx.gpr[2] = 0x08D68B7Cu;
}

// 0x08B59F6C (a0 = a table, a1 = the wanted word): a hash probe. A zero wanted word answers 1. The index at +0x202
// (0xFF ends the walk) picks a 0x10-byte slot; a slot whose word at +8 equals the wanted one runs
// 0x08B59BC0(table, table + 0x202, index) and 0x08B59D6C(table, index) and answers 1, and otherwise the slot's byte
// at +3 is the next index (0xFF answers 0).

// 0x08BBB988 (a0 = a table holder, a1 = an index, a2 = the output record): the entry is index * 4 into the table's
// word at +4; 0x08B03BB4 looks the entry's signed halfword at +0 up under the holder's word at +0x1C. The output gets
// the entry's +2 sign-extended to 15 bits at +0, zero at +2, the object at +4, the object's +0x20 / +0x38 / +0x50 at
// +8 / +0x10 / +0x14 and — unless the object's +0xE2 has bit 0 — the object's +0xB0 at +0xC. v0 = 1 when bit 2 of the
// object's +0xE2 is clear or the entry's +2 has bit 15 set, else 0.
void func_08BBB988(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 entry = word(rt, holder + 4u) + (index << 2);
    const u32 object = call(rt, ctx, 0x08B03BB4u, {word(rt, holder + 0x1Cu),
                                                   static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(
                                                       rt.memory().load16(entry))))});
    const u32 flags = rt.memory().load16(object + 0xE2u);
    const u32 trimmed = static_cast<u32>(static_cast<s32>(static_cast<u32>(rt.memory().load16(entry + 2u)) << 17) >> 17);
    rt.memory().store16(out, static_cast<std::uint16_t>(trimmed));
    rt.memory().store16(out + 2u, 0u);
    put(rt, out + 4u, object);
    put(rt, out + 8u, object + 0x20u);
    put(rt, out + 0xCu, (flags & 1u) != 0u ? object + 0xB0u : 0u);
    put(rt, out + 0x10u, object + 0x38u);
    put(rt, out + 0x14u, object + 0x50u);
    ctx.gpr[2] = (flags & 4u) == 0u || (rt.memory().load16(entry + 2u) & 0x8000u) != 0u ? 1u : 0u;
}

// 0x08BD5D54 (a0 = a map's object, a1 = a key holder): walk the list whose sentinel is a0 + 0x98. Each node's string
// (word at +8) has its length at -0xC; 0x08B36170 compares node and key over the shorter length (a0 = the node's
// string, a1 = the key, a2 = that length). A node whose compare is zero and whose length equals the key's ends the
// walk; a non-zero compare or a different length moves to the node's next word at +0. v0 = the node reached (the
// sentinel when the list ends).

// 0x08C7366C (a0 = a string holder, a1 = an object, a2 = a last index bound, a3 = an argument): the holder's string
// (word at +0 with its length at -0xC) is scanned backwards from min(a2, length - 1) with at most length positions:
// 0x08B79998(object, the byte, a3) answers 0 for the character looked for, and the position found is the answer.
// A zero-length string or no match answers -1.
void func_08C7366C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 bound = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const u32 length = word(rt, word(rt, self) - 0xCu);
    s32 position = -1;
    if (length != 0u) {
        const u32 last = length - 1u;
        position = static_cast<s32>(bound < last ? bound : last);
        const u32 positions = static_cast<u32>(position) + 1u;
        for (u32 checked = 0u;; ++checked) {
            const u32 character = static_cast<u32>(signed_byte(rt, word(rt, self) + static_cast<u32>(position)));
            if (call(rt, ctx, 0x08B79998u, {object, character, argument}) == 0u) break;
            position -= 1;
            if (checked + 1u == positions) {
                position = -1;
                break;
            }
        }
    }
    ctx.gpr[2] = static_cast<u32>(position);
}

// 0x0886E6F8 (a0 = the object): a destructor. The vtable 0x08CBF770 at +0, then 0x0886A8B8(object, i, 0) for i = 0..2,
// 0x0886AA0C(object, 2), (object, 5) and (object, 6). A non-zero member at +0x74 is released through its vtable's
// first slot (a1 = the word at +0x70) and both +0x74 and +0x70 are cleared. The work ends in the tail call
// 0x0886A858(object).

// 0x0887FB30 (a0 = the object): a destructor. The vtable 0x08CC11C8 at +0 and 0x08CC1204 at +0x70, then
// 0x0886AA0C(object, n) for n = 5, 2, 4, 6, 3. A non-zero member at +0x78 is released through its vtable's first slot
// (a1 = the word at +0x74) and +0x78 / +0x74 are cleared; +0x70 then takes the vtable word 0x08CC1180. The work ends
// in the tail call 0x0886A858(object).

// 0x08BF7980 (a0 = an owner passed on, a1 = a list node): a recursive free of a singly linked list. Each node's chain
// word at +0xC is freed first (the recursive call), then the holder at +0x14 is destroyed. The block at +0x10 is
// released only when its header at -0xC is not the shared empty block 0x08D74FD0: 0x08B60804(block - 4, -1) decides
// (a positive answer skips the release) and 0x08B8542C(header, sp) frees it. 0x08A5BC54(node, 0x18, 4) releases the
// node itself and the walk goes on with the node's word at +8. v0 = that last release (the caller's when there is no
// node).
void func_08BF7980(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    u32 node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    while (node != 0u) {
        call(rt, ctx, 0x08BF7980u, {owner, word(rt, node + 0xCu)});
        const u32 next = word(rt, node + 8u);
        call(rt, ctx, kHolderDestroy, {node + 0x14u});
        const u32 block = word(rt, node + 0x10u);
        const u32 header = block - 0xCu;
        if (header != 0x08D74FD0u) {
            const u32 kept = call(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu});
            if (static_cast<s32>(kept) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
        }
        call(rt, ctx, 0x08A5BC54u, {node, 0x18u, 4u});
        node = next;
    }
}

} // namespace
} // namespace new_batch_main_185
} // namespace gt::game

#define MAIN185(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_185::name, gt::ReturnKind::kind)
MAIN185(0x08A2E4C8u, func_08A2E4C8, Word);
MAIN185(0x08A52264u, func_08A52264, Word);
MAIN185(0x08A5973Cu, func_08A5973C, Word);
MAIN185(0x08A59C00u, func_08A59C00, Word);
MAIN185(0x08A5A0C8u, func_08A5A0C8, Word);
MAIN185(0x08A82EC0u, func_08A82EC0, Word);
MAIN185(0x08A8A3B8u, func_08A8A3B8, Word);
MAIN185(0x08A8C1ECu, func_08A8C1EC, Word);
MAIN185(0x08A9B8FCu, func_08A9B8FC, Word);
MAIN185(0x08B22204u, func_08B22204, Word);
MAIN185(0x08BBB988u, func_08BBB988, Word);
MAIN185(0x08BF7980u, func_08BF7980, Word);
MAIN185(0x08C7366Cu, func_08C7366C, Word);

// ---- from main-session batch 186 (draft 742be889fd5930ee26cf31110a885ae8a3c966fb) ----
// New batch main 186: 30 functions written as structured C from `tools.disasm`.
// DRAFT: none of these has been through the differential gate yet (the main session writes, the verifier gates).
// No names: none has evidence. The repeated shapes get one helper each, noted above it:
//   * optional_value_test          (0x08A2AB38, 0x08A2ABF0, 0x08A2ACA8)
//   * release_reference_range      (0x08B8ECAC, 0x08BA8FB0)
//   * tagged_pair_call             (0x08BF0570, 0x08C08E1C, 0x08C13230)
//   * tagged_pair_tail             (0x08C25A3C, 0x08C2CDC4, 0x08C37E8C)
//   * release_reference_destructor (0x08978B04, 0x08978BBC — byte-identical bodies)
// Every body leaves v0 (or $f0 for the one float answer) as the original does on each exit path: the last call's
// result, the constant the asm loads, the value its ll/sc leaves, or the caller's own v0 where the original
// returns without writing it. Input shapes for the synthetic generator: /tmp/claude-1000/shapes_main186.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_186 {
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
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    return result.v0;
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// The shared empty / counted block the reference holders compare a released cell against (a fixed .bss address).
constexpr u32 kEmptyHolder = 0x08D74FD0u;
// The stride of one record / one element of the two tables of 0x088A1A3C. The original builds both by shifts only
// (0x430 = ((count << 2) + (count << 6) - count) << 4, 0x690 = ((count << 7) - (count << 4)) * 15), so neither
// appears as an immediate in its disassembly.
constexpr u32 kRecordStride = 0x430u;
constexpr u32 kElementStride = 0x690u;

// --- the 180-byte group ------------------------------------------------------------------------------------
// 0x08B10754: the weighted mean of a list. func_08B34888(0x08D47468) takes the lock, every node of the list at
// 0x08D47460 weighs 1.0 (added in the virtual call's delay slot, so a node counts before its own call) and adds
// its virtual +0x38 float result to the sum, func_08B34B24(0x08D47468) releases the lock, and $f0 = sum / weight
// — 1.0 when the list was empty (the division sits in a branch-likely delay slot and is skipped then).
void func_08B10754(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B34888u, {0x08D47468u});
    float weight = 0.0f;
    float sum = 0.0f;
    for (u32 node = word(rt, 0x08D47460u); node != 0u; node = word(rt, node + 8u)) {
        const u32 object = word(rt, node);
        weight = weight + 1.0f;
        const GuestResult measured = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x38u), {object}, {});
        sum = sum + measured.f0;
    }
    call(rt, ctx, 0x08B34B24u, {0x08D47468u});
    ctx.fpr[0] = weight == 0.0f ? 1.0f : sum / weight;
}

// 0x08C53ED0 (a0 = a rope node, a1 = an index within it): the byte at that index. A node with a data pointer at
// +0x10 answers the byte there; otherwise the kind byte at +0xC picks: 0 answers the byte at [[node + 0x18] +
// index]; 1 descends into [node + 0x18] while the index is below its length word, else into [node + 0x1C] with
// the length subtracted (the data test is NOT repeated on a descended node — the original re-enters at the kind
// load); 2 and 3 call the virtual at [[node + 0x18]]'s +8 with (that object, index, 1, sp) and answer the byte it
// wrote to the frame. Kinds 4 and above re-run the kind test forever in the original, which therefore never
// returns; this version sends them down the 2/3 path (no case can reach the difference).
s32 rope_byte(Runtime &rt, AllegrexContext &ctx, u32 node, u32 index) {
    GuestFrame frame(ctx, 0x20u);
    const u32 data = word(rt, node + 0x10u);
    if (data != 0u) return signed_byte(rt, data + index);
    for (;;) {
        const u32 kind = rt.memory().load8(node + 0xCu);
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


// 0x08C73B40 (a0 = an object whose word 0 is a byte base, a1 = a value, a2 = a start index, a3 = a context): the
// count is the word at (base - 0xC) — a zero count, or a null context, answers -1. From max(index, count - 1)
// downwards each byte of the base goes to func_08B79998(value, byte, context): the first non-zero answer returns
// that index, and running off the start answers -1. The base pointer is re-read from the object every step.
void func_08C73B40(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 start = ctx.gpr[6];
    const u32 context = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const u32 count = word(rt, word(rt, self) - 0xCu);
    if (count == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    if (context == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 last = count - 1u;
    u32 index = start;
    if (start < last) index = last;  // sltu: the larger of the two, unsigned
    const u32 steps = index + 1u;
    // The original tests its counter after the call, so a start index of -1 (steps wraps to 0) walks the range
    // forever instead of not at all: a do-while keeps that.
    u32 step = 0u;
    do {
        const u32 character = static_cast<u32>(signed_byte(rt, word(rt, self) + index));
        if (call(rt, ctx, 0x08B79998u, {value, character, context}) != 0u) {
            ctx.gpr[2] = index;
            return;
        }
        ++step;
        index -= 1u;
    } while (step != steps);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// --- the 184-byte group ------------------------------------------------------------------------------------
// 0x0887F704 (a0 = an object): a destructor. Vtable 0x08CC11C8 at +0 and 0x08CC1204 at +0x70 (in the delay slot of
// the first call); func_0886AA0C releases the members 5, 2, 4, 6 and 3 in that order; the member pointer at +0x78
// goes through its virtual 0 with the word at +0x74 and both words are cleared; vtable 0x08CC1180 at +0x70,
// func_0886A858(a0), and the work ends in a tail call of func_08B64654(a0).

// 0x0888B748 (a0 = a registry with its key table at +4 and its reporter at +0x18, a1 = a key): func_088DA588(key
// table, key) names the entry and func_088DA788's +0x54 is its kind. When the entry's byte +8 is clear,
// func_088DC858(key table, key) finds nothing, func_088DB2C8(key table) is non-null and the kind is 5, a kind
// 0x23 descriptor carrying the key at +8 goes to func_088936A4 at the reporter. Then the entry is looked up again
// and its virtual +0x1C is called with (entry, 0).
void func_0888B748(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0xB0u);
    const u32 entry = call(rt, ctx, 0x088DA588u, {word(rt, self + 4u), key});
    const u32 kind = word(rt, call(rt, ctx, 0x088DA788u, {word(rt, self + 4u), key}) + 0x54u);
    if (rt.memory().load8(entry + 8u) == 0u) {
        if (call(rt, ctx, 0x088DC858u, {word(rt, self + 4u), key}) == 0u &&
            call(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), key}) != 0u && kind == 5u) {
            put(rt, frame.at(0u), 0x23u);
            put(rt, frame.at(8u), key);
            call(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
        }
    }
    const u32 found = call(rt, ctx, 0x088DA588u, {word(rt, self + 4u), key});
    call_indirect(rt, ctx, word(rt, word(rt, found) + 0x1Cu), {found, 0u});
}

// 0x088A1A3C (a0 = an object with an s16 index table at +0x10, a record base at +8 and a second table at +0x4C;
// a1 = an index into the s16 table, a2 = a value, a3 = a byte flag): the entry's s16 (at least 0) picks record
// `count` — base + count * 0x430 + 8 — which takes the value through func_088A5814 (flag set) or func_088A6C48,
// and the second table's count * 0x690 element goes to func_088A6068 with that record. v0 = the record.

// 0x088E7A04 (a0 = an object with the world at +0, a1 = a value): func_088D414C(sp, [world] + 0x620, value, 2)
// fills the frame; a clear byte at sp + 0x20 takes func_088E798C(a0, value, sp), else func_088E7894(a0, value, sp,
// sp + 0xC) with two zero floats. Then [world] + 0x14C's func_088F8D58(value) answer has its virtual +0x1C and
// then +0x10 called with (it, 1); v0 = the last of them.

// 0x08937CA0 (a0 = the answer, a1-a3 = the descriptor, t0 is forced to 1): func_08B10170(sp + 0x40, a1, a2, a3, 1)
// fills a 0x3C-byte record that is copied down to sp (0x30 bytes in 0x10-byte steps, then three words); a2 lands
// at +4 and a3 at +8 of the answer; func_08937B0C(answer, the copied word at sp + 0x34) runs and a non-zero answer
// makes v0 = a3, else -1 (the original's `movn`).

// 0x08978B04 and 0x08978BBC (a0 = an object; the two bodies are byte-identical): a destructor. Vtable 0x08CBB740
// at +0; the four reference cells at +0x1A8, +0x1A4, +0x1A0 and +0x19C are released with func_08A5B9EC in that
// (descending) order; func_08BEDDC4 runs on +0x18C and +0x184; the block at +0x74, its end at +0x84 rounded down
// to words, is released with func_08A5BC54; then func_08B28BE0(+0x48), func_08B28DC4(+0x20), func_08BEDD64(+0x18)
// and the work ends in a tail call of func_08A780B0(a0). (The original also tests `a0 + 0x19C` for null and skips
// the four releases then, which no object address can satisfy; the count is the loop's own 4.)
void release_reference_destructor(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, 0x08CBB740u);
        for (u32 step = 0u; step < 4u; ++step) {
            call(rt, ctx, 0x08A5B9ECu, {self + 0x1A8u - step * 4u});
        }
        call(rt, ctx, 0x08BEDDC4u, {self + 0x18Cu});
        call(rt, ctx, 0x08BEDDC4u, {self + 0x184u});
        const u32 block = word(rt, self + 0x74u);
        if (block != 0u) {
            call(rt, ctx, 0x08A5BC54u, {block, (word(rt, self + 0x84u) - block) & ~3u, 4u});
        }
        call(rt, ctx, 0x08B28BE0u, {self + 0x48u});
        call(rt, ctx, 0x08B28DC4u, {self + 0x20u});
        call(rt, ctx, 0x08BEDD64u, {self + 0x18u});
    }
    tail_call(ctx, 0x08A780B0u, {self});
}


// 0x089A22EC (a0 = a destination, a1 = an object): a value object is built on the frame — vtable 0x08CCD9F0 at
// sp + 8, the empty holder 0x08D74FDC at sp + 0xC, -1 at sp + 0x10 and a pointer to the float storage at sp + 0x14
// — and the object's virtual +8 is called with (object, it). The cell it leaves at sp + 0xC then goes through the
// reference release — unless it is still the empty holder: func_08B60804(cell - 4, -1) answers whether the block is
// shared, and func_08B8542C(cell - 0xC, sp) releases it. func_089A0A40(a0, $f12 = the float the value object left
// at sp + 4) ends the function.

// An optional-value test (three instances; a0 = the destination): a container is built at sp + 8
// (func_08ACC220), `test` answers whether it holds a value — when it does, func_08A2AA84(sp + 8) runs first —
// func_08A97A6C(cell, bool) builds the answer value object at sp + 4 (empty) or sp (holds one), which
// func_08A5BAD8 assigns to a0 unless a0 is that cell; func_08A5B9EC releases it and func_08ACC0F4 destroys the
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


// 0x08A33C70 (a0 = the destination): func_08B2DEE4()'s two-word answer goes to func_08B1B740, whose two words then
// fill the frame's four cells (+4/+8 and +0x10/+0x14 — the original re-composes them from halves, a no-op);
// func_08A337A8(sp) fills the holder at sp, reported through func_08A5BB7C(0x08CACCEC) when null and read anyway,
// whose +0x14 and +0x18 take the two words; func_08A5BAD8 assigns it to a0 unless a0 is the frame cell, and
// func_08A5B9EC releases it. (tools.sp_loads flags the original's `lw $s0, 0(sp)` after func_08A337A8: it is that
// call's frame word, not a restore — modelled by reading frame + 0 below.)

// 0x08AB5620 (a0 = an object, a1 = a pointer to a holder pointer): a null holder ends the function with no work.
// Otherwise the holder's virtual +0xDC is called with (holder, 0) while the object's +0x108 slot is fetched as the
// tail target, the holder is re-read (reported through func_08A5BB7C(0x08CB3EC4) when null and read anyway) and its
// virtual +0xD8 called; the object's +0x108 handler ends the function with (a0, the first answer, the second).

// 0x08ABD034 (a0 = an object): the state word at +0x10 takes func_08B2E750(a0)'s answer; the virtual +0x2C runs,
// then — while the virtual +8 answers non-zero — the virtual +0x34, the virtual +0x38, the virtual +8 again and,
// when it is still non-zero, the virtual +0x3C before the loop repeats. The virtual +0x30 ends the function, which
// leaves -1 in the state word and in v0.

// 0x08AC7EDC (a0 = an object with a member at +4, a worker at +8, a callback at +0x10 with its argument at +0x14,
// a reporter at +0x18 and the loop flags at +0xC / +0xD): until the byte at +0xC is clear: func_08B2E0B0,
// func_08B28C44([a0 + 4]), func_08B34888(worker), func_08B34B24(worker), func_08B2E0EC(reporter), the callback
// (a0 = its argument) when non-null, the byte at +0xD cleared, then func_08B34888(worker), func_08B28BE8(worker)
// (the clear sits in that call's delay slot) and func_08B34B24(worker) again. v0 = 0 on the way out (the byte).

// 0x08B301C4 (a0 = an object with a state byte at +4, a lock at +0x8FC and a handle at +0x12A8): func_08B30110(a0)
// first; with the byte at +4 set, the func_08B2E0B0 / func_08B35214 / func_08B344E0 lock sequence, func_08C7AE88,
// func_08B2F17C(0x1800, 1) stored at 0x08D69164, func_08B3457C, and func_08B2E0A4 — which takes the *older*
// answer, the delay slot's `move s1, v0` runs before func_08B35214 (the trap this body keeps). Then the lock at
// +0x8FC, func_08B2F548(a0), the handle -1, and the byte at +4 and the word at +0x14 cleared. Ends in a tail call
// of func_08B3457C(lock).

// 0x08B62260 (a0 = the argument func_08B62178 takes): the holder func_08B62178 answers is cleared and its object
// freed through func_08B733F0(object + 0x30) unless the object carries the C++ ABI's string id (0x432B2B00 at
// +0x30, 0x474E5543 at +0x34). With the id, the count at +0x14 is handled: negative counts are bumped towards -1
// (a bump that reaches 0 hands the holder the word at +0x10 and reports that word), a count of 1 hands the holder
// +0x10 and frees the string, a count above 1 is stored back (and v0 = the count), and a count of 0 reports
// through func_08B6459C first: its answer + 1 becomes the count, or a -1 answer hands the holder +0x10.

// A range of counted references (two instances; a0 = first, a1 = end, records of 8 bytes): each record's object at
// +4 is skipped when null; its counters at +4 and +8 are decremented and a counter that reaches zero calls the
// object's virtual +8 (first) or +0xC (second) with the object. The original's ll/sc pairs leave 1 in v0, which
// this models (a plain read-modify-write otherwise: one guest thread runs here).
void release_reference_range(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    for (u32 at = begin; at != end; at += 8u) {
        const u32 object = word(rt, at + 4u);
        if (object == 0u) continue;
        const u32 first = word(rt, object + 4u);
        put(rt, object + 4u, first - 1u);
        ctx.gpr[2] = 1u;
        if (first != 1u) continue;
        call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
        const u32 second = word(rt, object + 8u);
        put(rt, object + 8u, second - 1u);
        ctx.gpr[2] = 1u;
        if (second != 1u) continue;
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
    }
}


// 0x08BBC804 (a0 unused, a1 = a pointer to the holder pointer, a2 = the source): func_08B62344(0x4B0) allocates
// the object, its vtable 0x08CC43E8 is stored and 0x4AC bytes (0x4A0 in 0x10-byte steps, then three words) are
// copied from the source; the holder takes the new object and a previous one, when non-null, is released through
// its virtual +4 as the tail call. With no previous object v0 = the last word copied (the original loads it and
// never overwrites it).
void func_08BBC804(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder_address = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = call(rt, ctx, 0x08B62344u, {0x4B0u});
    put(rt, object, 0x08CC43E8u);
    u32 at = object + 4u;
    const u32 end = source + 0x4A0u;
    for (u32 from = source; from != end; from += 0x10u, at += 0x10u) {
        for (u32 offset = 0u; offset < 0x10u; offset += 4u) put(rt, at + offset, word(rt, from + offset));
    }
    put(rt, at, word(rt, end));
    put(rt, at + 4u, word(rt, end + 4u));
    put(rt, at + 8u, word(rt, end + 8u));
    const u32 previous = word(rt, holder_address);
    put(rt, holder_address, object);
    if (previous == 0u) {
        ctx.gpr[2] = word(rt, end + 8u);
        return;
    }
    tail_call(ctx, word(rt, word(rt, previous) + 4u), {previous});
}

// A tagged callback pair (a0 = the destination, a1 = an object with the pairs at +0x14/+0x18 and +0x1C/+0x20,
// a2 = a base pointer). Each pair is a tagged pointer: a clear low bit calls the pair's own word at +0x14
// (resp. +0x1C) with base + (tag >> 1), a set one calls [word(base + (tag >> 1)) + that word]; the first call also
// gets the pair's own word in a1, the second the first answer. func_0896418C(destination, second answer) runs, the
// word at +0x10 of the second answer is copied to the destination's +0x10, and v0 = the destination (three
// instances: 0x08BF0570, 0x08C08E1C, 0x08C13230).
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


// A tagged pair whose second half is the tail call (three instances: 0x08C25A3C and 0x08C2CDC4 with 0x08AA2920,
// 0x08C37E8C with 0x08AA2AFC; a0 = the pair object, a2 = a pointer to the argument object, t0 = the first free
// argument). `prepare(t0, a1)` runs first, then the two tagged calls described at tagged_pair_call — the pairs at
// +0x14/+0x18 and +0x1C/+0x20, the argument object's word as the base; the first call gets the pair's tag in a1 —
// and the second target is the tail call with (its argument, the prepared value).
void tagged_pair_tail(Runtime &rt, AllegrexContext &ctx, u32 prepare) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 prepared = call(rt, ctx, prepare, {ctx.gpr[8], ctx.gpr[5]});
    const u32 base = word(rt, ctx.gpr[6]);  // the original reads the argument object only after the prepare call
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


// 0x08C491E4 (a0 = the low end of a range of s16 indices, a1 = the high end, a2 = the pivot index as an s16,
// a3 = a descriptor whose word +0 is a fixed byte offset and whose word +4 holds the record table at its word 0):
// one partition step of a sort over 0xF0-byte records keyed by the float at +0x98. The low index advances while
// its key is below the pivot's and the high one steps back while the pivot's is below its key; when they cross the
// answer is the low end, else the two indices are swapped and the walk repeats (the pivot key and its address stay
// in registers; the table pointer is re-read after the stores). No frame: a leaf.
void func_08C491E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    u32 high = ctx.gpr[5];
    const s32 pivot_index = static_cast<std::int16_t>(static_cast<std::uint16_t>(ctx.gpr[6]));
    const u32 descriptor = ctx.gpr[7];
    const u32 table_holder = word(rt, descriptor + 4u);
    const u32 fixed = word(rt, descriptor) << 2;
    u32 table = word(rt, table_holder);
    const float pivot = float_at(rt, table + static_cast<u32>(pivot_index) * 240u + fixed + 0x98u);
    u32 low = begin;
    for (;;) {
        u32 index;
        for (;;) {
            index = static_cast<u32>(signed_half(rt, low));
            if (!(float_at(rt, table + index * 240u + fixed + 0x98u) < pivot)) break;
            low += 2u;
        }
        u32 other;
        for (;;) {
            high -= 2u;
            other = rt.memory().load16(high);
            const s32 other_index = static_cast<std::int16_t>(static_cast<std::uint16_t>(other));
            if (!(pivot < float_at(rt, table + static_cast<u32>(other_index) * 240u + fixed + 0x98u))) break;
        }
        if (low >= high) {
            ctx.gpr[2] = low;
            return;
        }
        rt.memory().store16(low, static_cast<std::uint16_t>(other));
        low += 2u;
        rt.memory().store16(high, static_cast<std::uint16_t>(index));
        table = word(rt, table_holder);
    }
}

} // namespace
} // namespace new_batch_main_186
} // namespace gt::game

// 08B10754 answers in $f0, the others in $v0.
#define NEW186(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_186::name, gt::ReturnKind::kind)
NEW186(0x0888B748u, func_0888B748, Word);
NEW186(0x08B10754u, func_08B10754, Float);
NEW186(0x08BBC804u, func_08BBC804, Word);
NEW186(0x08C491E4u, func_08C491E4, Word);
NEW186(0x08C73B40u, func_08C73B40, Word);

