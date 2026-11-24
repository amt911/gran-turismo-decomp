// Main-session batch 10: 77 functions the extra seeds made emittable, hand-written as structured C from `tools.disasm`.
// Most of them are one family GT's parameter tables use everywhere: build a temporary key object on the frame, look the
// object up, destroy the temporary, convert the result and assign it into a container (an insert). The rest are
// destructor-shaped bodies (vtable stores, one release, a tail call), small list walks, an unaligned record copy and a
// handful of one-off getters/compare predicates.
// No names: none has evidence. Unverified drafts: the synthetic gate decides which of them register.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_10 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the `jal` (a0-a3, t0-t3 in order; what
// is not passed stays as the caller had it). Afterwards v0 is the callee's, as after the `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with float arguments ($f12, $f13, …). Returns $f0; v0 / v1 afterwards are the callee's.
float call_with_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// The callee-saved registers a body keeps values in (the harness compares s0-s7, so the caller's values come back).
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

u32 signed_byte(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// `wsbw`: the bytes of a word reversed.
u32 swapped(u32 value) { return __builtin_bswap32(value); }

// ---------------------------------------------------------------------------------------------------------------
// The container-insert family.
//
// Two orders exist. In the first one the temporary key object sits at sp + 4 and the value slot at sp; the second one
// builds the temporary at sp and the value at sp + 4. Both end by assigning the slot into *a0 (skipped when the
// destination already is the slot) and destroying what they built.
// ---------------------------------------------------------------------------------------------------------------

// What the value inserted is: the looked-up object itself, the object `convert` returned plus an offset, or a byte of it.
enum class Value : std::uint8_t { Object, Offset, ByteAt };

// The first order: create → lookup → destroy(temporary) → convert → insert(sp, value).
struct LookupInsert {
    u32 create;    // (temporary): builds the key object
    u32 lookup;    // (object) → the object looked up
    u32 convert;   // 0: the object itself is the value
    Value kind;
    u32 offset;    // WordAt / ByteAt: from the converted object
    u32 insert;    // (&slot, value)
    u32 null_text; // the assert's text when the lookup found nothing
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
    u32 value = found;
    if (plan.convert != 0u) value = call_set(rt, ctx, plan.convert, {found});
    if (plan.kind == Value::Offset) value = value + plan.offset;
    else if (plan.kind == Value::ByteAt) value = rt.memory().load8(value + plan.offset);
    call_set(rt, ctx, plan.insert, {frame.at(0u), value});
    if (destination != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// The same with a float value: `convert` returns it in $f0, and it goes to the insert in $f12.
void lookup_insert_float(Runtime &rt, AllegrexContext &ctx, u32 create, u32 lookup, u32 convert, u32 insert,
                         u32 null_text) {
    const u32 destination = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call_set(rt, ctx, create, {temporary, ctx.gpr[5]});
    const u32 object = word(rt, temporary);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {null_text});
    const u32 found = call_set(rt, ctx, lookup, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    const float value = call_with_floats(rt, ctx, convert, {found}, {});
    call_with_floats(rt, ctx, insert, {frame.at(0u)}, {value});
    if (destination != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// The second order: create(sp) → lookup → convert → make the value at sp + 4 → assign → destroy(sp + 4), destroy(sp).
struct PairInsert {
    u32 create;
    u32 lookup;
    u32 convert;   // 0: none
    u32 convert2;  // 0: none (applied to the first convert's result)
    u32 make;      // (&value, value)
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
    u32 value = found;
    if (plan.convert != 0u) value = call_set(rt, ctx, plan.convert, {found});
    if (plan.convert2 != 0u) value = call_set(rt, ctx, plan.convert2, {value});
    call_set(rt, ctx, plan.make, {slot, value});
    if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// The second order with a float value made at sp + 4 in $f12.
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

// The second order with a 64-bit value: `convert` returns v0 / v1, and both go to the insert ($a2 / $a3; $a1 is
// whatever the callee left, the original does not touch it).
void pair_insert_dword(Runtime &rt, AllegrexContext &ctx, u32 create, u32 lookup, u32 convert, u32 null_text) {
    const u32 destination = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(0u);
    const u32 slot = frame.at(4u);
    call_set(rt, ctx, create, {temporary, ctx.gpr[5]});
    const u32 object = word(rt, temporary);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {null_text});
    const u32 found = call_set(rt, ctx, lookup, {object});
    const GuestResult pair = call_indirect(rt, ctx, convert, {found}, {});
    call_set(rt, ctx, 0x08A5DEE8u, {slot, pair.a1, pair.v0, pair.v1});
    if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// A word of the looked-up object inserted as a value built at sp + 4, with the temporary at sp + 8.
void field_insert(Runtime &rt, AllegrexContext &ctx, u32 create, u32 null_text, u32 field) {
    const u32 destination = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 temporary = frame.at(8u);
    const u32 slot = frame.at(4u);
    const u32 argument = frame.at(0u);
    call_set(rt, ctx, create, {temporary, ctx.gpr[5]});
    const u32 object = word(rt, temporary);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {null_text});
    put(rt, argument, word(rt, object + field));
    call_set(rt, ctx, 0x08A76AACu, {slot, argument});
    if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

// 0x0882F370, 0x0882F52C, 0x0882F674, 0x0882FCF4, 0x0882FD88, 0x08833AFC: the same plan for the same lookup, only the
// conversion differs (this table's values are all built by its own accessors).
// 0x088333C0: the converted object plus 0xC is the value.
// 0x088512FC: the conversion returns a float, inserted as one.

// 0x0882FB48: the only member of the family that converts before it destroys the temporary: 0x0885EDF8 returns the
// record and the value is its byte at +0x5BA.

// The second order, whose value slot is at sp + 4.
// 0x089B9794 and 0x08A024C8: the temporary at sp + 8, a word of the looked-up object at sp, the value at sp + 4.

// 0x089BA264: the temporary at sp, the value of the object's virtual at +0x114 at sp + 4, the slot at sp + 8.

// 0x0881F048 (a0 = the destination): a temporary at sp + 4 (0x0881BD98), the object it holds converted by
// 0x0881B068, 0x0881C89C(sp, converted, 0) assigning into *a0 (when it is not sp), then the temporaries destroyed.

// 0x0885D07C (a0 = the owner): walks the parameter records of 0x0885EA04 (0x188 bytes each, the byte at +0x16 enabling
// one, the identifier at +0x1C minus one meaning "none", the value at +0x20 minus one meaning "the identifier") and
// passes each to 0x0885EA04(owner's word at +4, identifier, value). The offset advances before the identifier is
// tested, and a "none" identifier leaves the container register as it was.
void func_0885D07C(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    SavedRegisters saved(ctx, {16, 17, 18});
    GuestFrame frame(ctx, 0x10u);
    u32 container = word(rt, owner);
    const s32 count = static_cast<s32>(word(rt, container + 4u));
    ctx.gpr[2] = static_cast<u32>(count);
    if (count <= 0) return;
    u32 index = 0u;
    u32 offset = 0u;
    for (;;) {
        const u32 record = word(rt, container) + offset;
        ++index;
        const bool enabled = rt.memory().load8(record + 0x16u) != 0u;
        offset += 0x188u;
        if (enabled) {
            const u32 identifier = word(rt, record + 0x1Cu);
            if (identifier != 0xFFFFFFFFu) {
                const u32 parameter = word(rt, record + 0x20u);
                const u32 value = parameter == 0xFFFFFFFFu ? identifier : parameter;
                call_set(rt, ctx, 0x0885EA04u, {word(rt, owner + 4u), identifier, value});
                container = word(rt, owner);
            }
        }
        ctx.gpr[2] = index < word(rt, container + 4u) ? 1u : 0u;
        if (ctx.gpr[2] == 0u) break;
    }
}

// 0x0886CB54 (a0 = the object, a1 = the key, a2 = the value): an ordered-container insert. 0x08B93AE8 finds the node
// from the object's +0x1DC; the end node (+0x1E0) or a key below the node's +0x10 builds a new node with 0x08B940D8
// (its key at sp, a zero byte at sp + 4) and stores the value at +0x14; any other key stores the value in the node.
void func_0886CB54(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx, {16, 17, 18});
    const u32 tree = word(rt, self + 4u);
    const u32 owner = tree + 0x1DCu;
    const u32 end = tree + 0x1E0u;
    put(rt, frame.at(0x10u), key);
    const u32 found = call_set(rt, ctx, 0x08B93AE8u, {owner, frame.at(0x10u)});
    if (end == found || key < word(rt, found + 0x10u)) {
        put(rt, frame.at(0u), key);
        put_byte(rt, frame.at(4u), 0u);
        const u32 node = call_set(rt, ctx, 0x08B940D8u, {owner, found, frame.at(0u)});
        put_byte(rt, node + 0x14u, value);
        ctx.gpr[2] = node;
        return;
    }
    put_byte(rt, found + 0x14u, value);
    ctx.gpr[2] = 0u;  // the comparison that sent it here
}

// 0x08869BB4 (a0 = the object, a1 = the key, a2 = the value, f12 = the amount): under the object's lock (0x08B344E0)
// finds the entry at +0x1B0 with 0x0886A020, takes its member 4 with 0x0886A9A0, and 0x08B93734's object gets
// 0x0887051C(0, amount, 0); then 0x08B3457C releases the lock in a tail call.

// 0x088E59A0 (a0 = the object): the two members at +0 and +4 each get their virtual at +0x10 and have +0x530 cleared,
// the second only when it exists; then 0x08B08D54 / 0x08B08D84 on the first's word at +0xA8, its virtual at +0x38 and
// v0 = (its result != 0).

// 0x089BEE8C (a1 = the key, a2 = the count, a3 = the source): with a positive count the object at *a3's virtual at
// +0x3C decides the flag (nonzero); the record 0x089BEE4C(sp, key) then gets the flag at +0x2AC.

// 0x08A13900 (a0 = the object, a2 = argument, a3 = argument): under 0x08B34888(object + 0x10),
// 0x08ACCE9C(object + 0x240, 0, argument, argument); a non-null answer sets four bytes of the object and clears a word;
// v0 = the answer, after 0x08B34B24 releases it.

// 0x08A5ECF4 (a0 = the object, a1 = the destination, a3 = the source): v1 = the AND of the object's 64-bit word at
// +0x18 with what the object at *a3 returns from its virtual at +0x50; 0x08A5DEE8 makes the value at sp and assigns it.

// 0x08A5EE14: the same with an exclusive OR.

// 0x08A83FB8: the exclusive OR again, with 0x08A83160 making the value.

// 0x08A886C8 (a0 = the object, a1 = the destination, a3 = the source): the flag is "the object's float at +0x14 <=
// what the object at *a3 returns from its virtual at +0x54"; 0x08A97A6C(sp, flag) makes the value and assigns it.

// 0x08A9D8AC (a0 = the key, a1 = the object, a2 = the value): under the object's +0x4C lock,
// 0x08A76AB4(key, 0x08CF6ED8) and 0x08A7BFD0(object's word at +0x38, key, value); v0 = the key after the lock releases.

// 0x08ABFFA0 (a0 = the object, a1 = the callback, f12 = the argument): under the object's +0xC lock walks the nodes
// from +4 (the node at the entry, the next at entry + 8), releases the lock around each callback(node, f12) and takes
// it again; the tail call releases it.
void func_08ABFFA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 callback = ctx.gpr[5];
    const float argument = ctx.fpr[12];
    SavedRegisters saved(ctx, {16, 17, 18, 19});
    GuestFrame frame(ctx, 0x30u);
    const u32 lock = self + 0xCu;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    u32 entry = word(rt, self + 4u);
    if (entry != 0u) {
        for (;;) {
            const u32 node = word(rt, entry);
            const u32 next = word(rt, entry + 8u);
            call_set(rt, ctx, 0x08B3457Cu, {lock});
            call_with_floats(rt, ctx, callback, {node}, {argument});
            call_set(rt, ctx, 0x08B344E0u, {lock});
            entry = next;
            if (entry == 0u) break;
        }
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// 0x08ACDC00 (a1 = the object): the word at +0xE8 is detached first; when there was one, under the +0x80 lock the
// byte at +0xED is cleared, 0x08B28C74(object + 0x28) and 0x08B1B99C(detached) run; then 0x08AF07C8(0, 1) and the
// word at +0xE4 released through 0x08B35254.
void func_08ACDC00(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[5];
    SavedRegisters saved(ctx, {16, 17, 18});
    GuestFrame frame(ctx, 0x10u);
    const u32 lock = self + 0x80u;
    const u32 detached = word(rt, self + 0xE8u);
    put(rt, self + 0xE8u, 0u);
    if (detached != 0u) {
        call_set(rt, ctx, 0x08B344E0u, {lock});
        put_byte(rt, self + 0xEDu, 0u);
        call_set(rt, ctx, 0x08B3457Cu, {lock});
        call_set(rt, ctx, 0x08B28C74u, {self + 0x28u});
        call_set(rt, ctx, 0x08B1B99Cu, {detached});
    }
    call_set(rt, ctx, 0x08AF07C8u, {0u, 1u});
    const u32 object = word(rt, self + 0xE4u);
    if (object != 0u) {
        call_set(rt, ctx, 0x08B35254u, {object});
        put(rt, self + 0xE4u, 0u);
    }
}

// 0x08AF8500 (a0 = the object, a1 = the index, t0 = the mode): the table at 0x08D4725C / 0x08D47260 bounds the index
// (below the count it returns the `lui`'s leftover 0x00070000, above the last index 1); the entry is the object's
// word at +0x38 plus 48 bytes per index. Mode 1 with bit 9 of the entry set returns 1, mode 2 without it returns 2;
// every other case ends in a tail call of 0x08AF84B0(entry, 0, object's word at +0x94).
void func_08AF8500(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 mode = ctx.gpr[8];
    const u32 table = 0x08D4725Cu;
    const u32 count = word(rt, table);
    ctx.gpr[2] = 0x00070000u;
    if (static_cast<s32>(index) < static_cast<s32>(count)) return;
    if (static_cast<s32>(word(rt, table + 4u)) < static_cast<s32>(index)) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 entry = word(rt, self + 0x38u) + index * 48u;
    const u32 bit = (word(rt, entry) >> 9) & 1u;
    ctx.gpr[2] = entry;
    if (mode == 1u) {
        if (bit != 0u) {
            ctx.gpr[2] = 1u;
            return;
        }
    } else if (mode == 2u) {
        if (bit == 0u) {
            ctx.gpr[2] = 2u;
            return;
        }
    }
    ctx.gpr[2] = mode == 0u ? entry : (mode == 1u ? 1u : 2u);
    tail_call(ctx, 0x08AF84B0u, {entry, 0u, word(rt, self + 0x94u)});
}

// 0x08B10D34 (a0 = the object): the word at +0x5C's first word through 0x08B0DD88 decides; when it answers zero
// nothing else runs and v0 = 0. Otherwise the object's virtual at +0x6C, 0x08B10CC4(object, inner), 0x08B10A40(inner),
// 0x08B0E31C(inner), and a tail call of inner's virtual at +0x24.

// 0x08B11EA0 (a0 = the object, a2 = the destination, a3 = the limit): the object's virtual at +0x9C, 0x08B17A98 and
// 0x08B35F60 turn into an amount; below the limit 0x08B7BB6C(destination, member) runs and v0 = 1, else 0;
// 0x08B17AA8 releases the first result.

// 0x08B2EE2C (a0 = the object): the word at +0 holds a state. 1: 0x08C7A8D0(word +0x10, 0, 0, 0, 0) and then a tail
// call of 0x08C7A8C8(word +0x10) keeping the call's v0 — the pointer null returns 0 instead. 2: the byte at +4, zeroed
// after 0x08C7B1E0(word +0xC) ran only when it was set (v0 = that call's result, else 0). Any other state returns 2.
void func_08B2EE2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 tail = 0u;
    u32 tail_argument = 0u;
    u32 tail_second = 0u;
    {
        SavedRegisters saved(ctx, {16});
        GuestFrame frame(ctx, 0x10u);
        const u32 state = word(rt, self);
        ctx.gpr[2] = 1u;
        if (state == 1u) {
            const u32 pointer = word(rt, self + 0x10u);
            ctx.gpr[2] = pointer;
            ctx.gpr[4] = pointer;
            if (pointer != 0u) {
                const GuestResult released = call_guest(rt, ctx, 0x08C7A8D0u, {pointer, 0u, 0u, 0u, 0u}, {});
                ctx.gpr[2] = released.v0;
                ctx.gpr[3] = released.v1;
                ctx.fpr[0] = released.f0;
                tail_argument = word(rt, self + 0x10u);
                tail_second = released.a1;  // the tail call gets a1 as the callee left it
                tail = 0x08C7A8C8u;
            }
        } else if (state == 2u) {
            const u32 flag = rt.memory().load8(self + 4u);
            ctx.gpr[2] = flag;
            if (flag != 0u) call_set(rt, ctx, 0x08C7B1E0u, {word(rt, self + 0xCu)});
            put_byte(rt, self + 4u, 0u);
        } else {
            ctx.gpr[2] = 2u;
        }
    }
    if (tail != 0u) tail_call(ctx, tail, {tail_argument, tail_second});
}

// 0x08B75EDC (a0 = a selector byte, a1 = the object): the low byte 0xFF returns 0; the four high bits pick: 0x20 the
// word at +4, 0x30 the word at +8, 0x50 zero, 0x00/0x10 zero. 0x40, 0x60 and 0x70 run 0x08B77B04(selector, object)
// twice (the first's branch is always taken) and then return the word at +8.
void func_08B75EDC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 selector = ctx.gpr[4] & 0xFFu;
    ctx.gpr[4] = selector;
    if (selector == 0xFFu) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 kind = selector & 0x70u;
    ctx.gpr[4] = kind;
    const u32 object = ctx.gpr[5];
    if (kind == 0x20u) {
        ctx.gpr[2] = word(rt, object + 4u);
        return;
    }
    if (kind < 0x21u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (kind == 0x30u) {
        ctx.gpr[2] = word(rt, object + 8u);
        return;
    }
    if (kind == 0x50u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const GuestResult first = call_guest(rt, ctx, 0x08B77B04u, {kind, object}, {});
    ctx.gpr[2] = first.v0;
    ctx.gpr[3] = first.v1;
    ctx.gpr[5] = first.a1;  // the second call gets a1 as the first one left it
    call_set(rt, ctx, 0x08B77B04u, {kind});
    ctx.gpr[2] = word(rt, object + 8u);
}

// 0x08B87CAC (a0 = the object, a destructor): the vtable 0x08CB88A8 is stored at *this, the block at +0x44 (its -0xC
// not being 0x08D74FD0) released through 0x08B60804(block - 4, -1) and, unless that answered positive,
// 0x08B8542C(block - 0xC, sp); then 0x08B87B60(this + 0x2C, word +0x34), 0x08B87AB0(this + 0x14, word +0x1C) and
// 0x08A780B0(this).

// 0x08BDA64C (a0 = first, a1 = last): a range shorter than 0x88 elements is only handed to 0x08BDA574 in a tail call;
// otherwise 0x08BDA574(first, first + 0x80) runs and every 8-byte element from first + 0x80 on goes through
// 0x08BDA4F0(element, sp, element + 8) with the pair copied to sp first. v0 = the last call's result.

// 0x08BEE82C (a0 = the list head, a1 = the key): walks the list from word(a0) while 0x089FD6E0(key, node's word at +8)
// answers nonzero: 0x08B5C094(node), 0x08A5B9EC(node + 8) and 0x08A5BC54(node, 0xC, 4) free it. A head that is the
// list itself returns it; otherwise v0 = the node that closed the walk.

// 0x08C2C4E0 (a0 = first, a1 = last, a2 = source): every 0xC-byte element from first on (stopping at last) gets
// 0x08A6A23C(element, source), the source's word at +4 at +4, and — unless the element is the source — the source's
// word at +8 through 0x08A5BAD8(element + 8, that word).

// 0x08869148 (a0 = the object): the members at +0x1BC and +0x1C0 each get their virtual at +0x30 (a1 = 0) and at +0x10;
// the first also its virtual at +0; the last is a tail call of the +0x1C0 member's virtual at +0.

// 0x08875CD0 (a0 = the owner): 0x08B796C4(owner)'s word at +4 formats into a 0x400-byte buffer with 0x08B78180, its
// length comes from 0x08B35F60 and the trailing bytes below 0x21 are zeroed walking back from the end (while still
// inside the buffer); 0x08875AD8(owner, buffer) then gets it. v0 = its result.

// 0x088D3D10 (a0 = the object, a1 = the record): 0x088D2F9C(a0, a1), then the record's word at +0 through
// 0x088DA4B0; a null answer returns the object. Otherwise 0x088FDD68(answer) fills the float at +0x18 and, with
// 0x088D390C(answer's word at +0x61C, object, object + 0xC, 6, f12 = it, f13 = 0), 0x088FD9D0(answer) stores the word
// at +0x1C and the byte 1 at +0x20. v0 = the object.

// 0x088D6114 (a0 = one record, a1 = another): both are compared by their 64-bit value at +0x28 (each word byte
// reversed, `wsbw`, through the stack at sp) and by their signed byte at +0x4C; 1 when both agree, else 0.
void func_088D6114(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 left = word(rt, ctx.gpr[4]);
    const u32 right = word(rt, ctx.gpr[5]);
    put(rt, frame.at(0u), word(rt, right + 0x28u));
    put(rt, frame.at(4u), word(rt, right + 0x2Cu));
    const u32 left_high = swapped(word(rt, left + 0x2Cu));
    const u32 left_low = swapped(word(rt, left + 0x28u));
    if (swapped(word(rt, frame.at(4u))) != left_high) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (swapped(word(rt, frame.at(0u))) != left_low) {
        ctx.gpr[2] = 0u;
        return;
    }
    const s32 left_byte = static_cast<std::int8_t>(rt.memory().load8(left + 0x4Cu));
    const s32 right_byte = static_cast<std::int8_t>(rt.memory().load8(right + 0x4Cu));
    ctx.gpr[2] = left_byte == right_byte ? 1u : 0u;
}

// 0x088FB5D8 (a0 = the output, a1 = the source, a2 = two floats): the word at +4 of the source (else the word at +0)
// selects the branch: 0x08902884(out, first, sp + 8, source) with the two floats at sp + 8 / sp + 0xC, or
// 0x08904A8C(out, second, sp, source) with them at sp / sp + 4; neither leaves three zero words at the output.
// v0 = the output.
void func_088FB5D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 values = ctx.gpr[6];
    SavedRegisters saved(ctx, {16});
    GuestFrame frame(ctx, 0x20u);
    const u32 first = word(rt, source + 4u);
    if (first != 0u) {
        put_float(rt, frame.at(8u), float_at(rt, values));
        put_float(rt, frame.at(0xCu), float_at(rt, values + 4u));
        call_set(rt, ctx, 0x08902884u, {out, first, frame.at(8u), source});
        ctx.gpr[2] = out;
        return;
    }
    const u32 second = word(rt, source);
    if (second != 0u) {
        put_float(rt, frame.at(0u), float_at(rt, values));
        put_float(rt, frame.at(4u), float_at(rt, values + 4u));
        call_set(rt, ctx, 0x08904A8Cu, {out, second, frame.at(0u), source});
        ctx.gpr[2] = out;
        return;
    }
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    ctx.gpr[2] = out;
}

// 0x089ABA7C (a1 = the key, a2 = the count, a3 = the source): with a positive count, 0x089A354C(sp + 4, source, count)
// gives a value; 0x089AB370(sp, a1) then makes the record at sp, and with one, 0x089AAB80(record, value) runs.

// 0x089B3F78 (a0 = the object, a destructor): the vtables 0x08CBB320 at *this and 0x08CBB578 at +0x38, 0x08B48804(this
// + 0x120) and 0x08A5B9EC(this + 0x11C), then the block at +0x118 released like 0x08B87CAC does, and 0x08A0298C(this).
void func_089B3F78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16, 17});
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CBB320u);
    put(rt, self + 0x38u, 0x08CBB578u);
    call_set(rt, ctx, 0x08B48804u, {self + 0x120u});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x11Cu});
    const u32 block = word(rt, self + 0x118u);
    const u32 inner = block - 0xCu;
    if (inner != 0x08D74FD0u) {
        const u32 reference = call_set(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(reference) <= 0) call_set(rt, ctx, 0x08B8542Cu, {inner, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08A0298Cu, {self});
}

// 0x089D02B8 (a0 = the key, a2 = the mode, a3 = the record): only mode 1 does anything: 0x089CD62C(sp) gives a key
// (asserted at 0x08CA69B8), the record at *a3 (asserted at 0x08CA697C) answers its virtual at +0x3C, and
// 0x089D027C(key, answer) runs; then sp is destroyed. Every other mode returns without touching anything.
void func_089D02B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    SavedRegisters saved(ctx, {16, 17});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    call_set(rt, ctx, 0x089CD62Cu, {frame.at(0u), ctx.gpr[5]});
    const u32 key = word(rt, frame.at(0u));
    if (key == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
    const u32 record = word(rt, source);
    if (record == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA697Cu});
    const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, record) + 0x3Cu), {record}, {}).v0;
    call_set(rt, ctx, 0x089D027Cu, {key, answer});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x089D8C20 (a0 = the object, a destructor): the vtable 0x08CBCDE0 at *this and 0x08CBCFCC at +0x38, the block at
// +0x78 released like 0x08B87CAC does, then 0x08A101F8(this) and 0x08A5BC54(this, 0xA8, 4).

// 0x08B32138 (a1 = the output): 0x08C7AF10(sp) returns a negative status into v0 = 0; otherwise word(sp) at the
// output, the two words from sp + 4 at +4 and +8, the word from sp + 0x8C at +0xC and the two bytes from sp + 0x90 at
// +0x10 and +0x11 (the `lwl`/`lwr` and `swl`/`swr` pairs are aligned copies here), v0 = 1.

// 0x08B42618 (a0 = the object, a1 = the first argument, a2 = the output, a3 = a flag byte): the member at +0xB0's
// virtual at +0x3C(member, a1) gives v0 / v1, stored at the output's +8 and +0xC; 0x08B3FF90(sp, a1 + 0x12) and
// 0x08B3FA3C(sp, 0x4E9D) build the argument at sp, and the member's virtual at +0x64(member, output, sp, flag) ends it.

// 0x08B641D0: 0x08C7AAD0(0x08CB5B6C, 0x200, 0, 0) makes the node stored at 0x08D721B8; the list head at 0x08CE1F30
// becomes the node (its word at +4 the old head), the head being empty first built by 0x08B77B1C(0x08B64188).
// v0 = 0x08D721B8.

// 0x08BE642C (a0 = first, a1 = last, a2 = destination): copies 0x30-byte records from first to last into the
// destination (a null destination skips the copy but still advances) and returns the advanced destination.
void func_08BE642C(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (first == last) {
        ctx.gpr[2] = destination;
        return;
    }
    u32 source = first;
    u32 field = first + 0x2Cu;
    do {
        source += 0x30u;
        if (destination != 0u) {
            put(rt, destination + 4u, word(rt, field - 0x28u));
            put(rt, destination + 8u, word(rt, field - 0x24u));
            put_float(rt, destination + 0xCu, float_at(rt, field - 0x20u));
            put_float(rt, destination + 0x10u, float_at(rt, field - 0x1Cu));
            put_float(rt, destination + 0x14u, float_at(rt, field - 0x18u));
            put_float(rt, destination + 0x18u, float_at(rt, field - 0x14u));
            put_float(rt, destination + 0x1Cu, float_at(rt, field - 0x10u));
            put(rt, destination, 0x08CCDE00u);
            put_float(rt, destination + 0x20u, float_at(rt, field - 0xCu));
            put_float(rt, destination + 0x24u, float_at(rt, field - 8u));
            put_float(rt, destination + 0x28u, float_at(rt, field - 4u));
            put_float(rt, destination + 0x2Cu, float_at(rt, field));
        }
        field += 0x30u;
        destination += 0x30u;
    } while (last != source);
    ctx.gpr[2] = destination;
}

// 0x08C2045C (a0 = the destination, a1 = the key, a2 = the value): 0x08A76ABC(sp, value), 0x08BF7C60(sp + 4, sp),
// 0x08A744F0(sp + 8, key, sp + 4) and 0x08A7C5C8(destination, sp + 8), with sp + 8, sp + 4 and sp destroyed after.
void func_08C2045C(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    SavedRegisters saved(ctx, {16, 17, 18, 19});
    GuestFrame frame(ctx, 0x30u);
    const u32 first = frame.at(0u);
    const u32 second = frame.at(4u);
    const u32 third = frame.at(8u);
    call_set(rt, ctx, 0x08A76ABCu, {first, value});
    call_set(rt, ctx, 0x08BF7C60u, {second, first});
    call_set(rt, ctx, 0x08A744F0u, {third, key, second});
    call_set(rt, ctx, 0x08A7C5C8u, {destination, third});
    call_set(rt, ctx, 0x08A5B9ECu, {third});
    call_set(rt, ctx, 0x08A5B9ECu, {second});
    call_set(rt, ctx, 0x08A5B9ECu, {first});
}

// 0x08C54DE4 (a0 = the object, a1 = a selector, a2 = a fallback): the table at +0x7C (its absence ends in the
// no-return 0x08B5A9FC) holds the signed byte at +0x11D of the selector, which is the answer when it is not zero;
// otherwise the table's virtual at +0x20(table, selector, fallback) answers, is stored there unless it equals the
// fallback, and comes back sign extended.
void func_08C54DE4(Runtime &rt, AllegrexContext &ctx) {
    const s32 selector = static_cast<std::int8_t>(ctx.gpr[5]);
    const s32 fallback = static_cast<std::int8_t>(ctx.gpr[6]);
    SavedRegisters saved(ctx, {16, 17});
    GuestFrame frame(ctx, 0x10u);
    const u32 table = word(rt, ctx.gpr[4] + 0x7Cu);
    if (table == 0u) {
        call_set(rt, ctx, 0x08B5A9FCu, {0u});
        return;
    }
    const u32 slot = table + (static_cast<u32>(selector) & 0xFFu);
    const s32 stored = static_cast<std::int8_t>(rt.memory().load8(slot + 0x11Du));
    if (stored != 0) {
        ctx.gpr[2] = static_cast<u32>(stored);
        return;
    }
    ctx.gpr[6] = static_cast<u32>(fallback);
    const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, table) + 0x20u), {table, static_cast<u32>(selector)}, {}).v0;
    if (static_cast<u32>(fallback) != answer) put_byte(rt, slot + 0x11Du, answer);
    ctx.gpr[2] = static_cast<u32>(static_cast<std::int8_t>(answer));
}

} // namespace
} // namespace new_batch_main_10
} // namespace gt::game

#define MAIN10(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_10::name, gt::ReturnKind::kind)
MAIN10(0x0885D07Cu, func_0885D07C, Word);
MAIN10(0x0886CB54u, func_0886CB54, Word);
MAIN10(0x088D6114u, func_088D6114, Word);
MAIN10(0x088FB5D8u, func_088FB5D8, Word);
MAIN10(0x089B3F78u, func_089B3F78, Word);
MAIN10(0x089D02B8u, func_089D02B8, Word);
MAIN10(0x08ABFFA0u, func_08ABFFA0, Word);
MAIN10(0x08ACDC00u, func_08ACDC00, Word);
MAIN10(0x08AF8500u, func_08AF8500, Word);
MAIN10(0x08B2EE2Cu, func_08B2EE2C, Word);
MAIN10(0x08B75EDCu, func_08B75EDC, Word);
MAIN10(0x08BE642Cu, func_08BE642C, Word);
MAIN10(0x08C2045Cu, func_08C2045C, Word);
MAIN10(0x08C54DE4u, func_08C54DE4, Word);
