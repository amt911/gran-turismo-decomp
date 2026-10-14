// Hot batch 64: more small functions with branches or loops that the five pad routes reach (call probe as in batches 57-63, now with
// the options route). As in batches 60-63, each function's comment lists the branches the compared cases of the fast gate do not take
// (found with `tools.decomp_gate coverage`); those were compared against the asm by hand, reading the branch TARGET (a value set in a
// delay slot is the result only if the target does not overwrite it). Their classes aren't known yet, so they keep their addresses as
// names. Written from the asm; the generated code was only a reading aid (it gives the relocated constants).
//
// Families: the "add an offset to a non-null pointer field" setters (plain and with a tail call), the "forward to a virtual method of
// a child object" thunks, the checked holder accessors (a null holder calls the assert function func_08A5BB7C), range fill loops.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_64 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kAssertNull = 0x08A5BB7Cu; // (message): reports a null holder (called with the holder's message, then the code goes on)
constexpr std::uint32_t kDelete = 0x08B64654u;     // (object): operator delete

// *dest = *dest + delta when the pointer field *dest is not null; v0 = the old value.
void add_to_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t old = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = old;
    ctx.gpr[3] = old + ctx.gpr[5];
    if (old != 0u) put(rt, ctx.gpr[4], old + ctx.gpr[5]);
}

// The same, then a tail call of `target` with a0 = the new pointer (a1 as it was; a2 = the field's address; a1 = 0 when `zero_a1`).
// v0 = the old value (0 means: nothing stored, no call).
void add_to_field_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, bool zero_a1) {
    const std::uint32_t field = ctx.gpr[4], delta = ctx.gpr[5];
    const std::uint32_t old = word(rt, field);
    const std::uint32_t sum = old + delta;
    ctx.gpr[3] = sum;
    // func_08BD2C68 sets v0 = delta + old before testing old; the others keep v0 = old.
    ctx.gpr[2] = zero_a1 ? sum : old;
    if (old == 0u) return;
    put(rt, field, sum);
    tail_call(ctx, target, {sum, zero_a1 ? 0u : delta, field});
}

// this[3] is a child object: tail-call its vtable method at byte `slot` (a0 = the child); v0 = the child, then its vtable. Null child: v0 = 0.
void forward_member_12(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 12u);
    ctx.gpr[2] = child;
    if (child == 0u) return;
    const std::uint32_t table = word(rt, child);
    const std::uint32_t method = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {child});
}

// Fills [first, last) with the word *value (read once); stops when the cursor equals last.
void fill_words(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t p = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    if (p == last) return;
    const std::uint32_t value = word(rt, ctx.gpr[6]);
    do {
        put(rt, p, value);
        p += 4u;
    } while (p != last);
}

// The holder (an object pointer in a field) must be set: a null one calls func_08A5BB7C(message) and the code goes on regardless.
std::uint32_t checked_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder, std::uint32_t message) {
    if (holder == 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kAssertNull, {message});
    }
    return holder;
}

} // namespace

// func_08B49858(this, value) -> 1 and this[0] = value when value < 14, else 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): value >= 14 (no store; v1 = 0 from the delay slot, v0 = v1 at the jr).
void func_08B49858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const bool ok = ctx.gpr[5] < 14u;
    if (ok) put(rt, ctx.gpr[4], ctx.gpr[5]);
    ctx.gpr[3] = ok ? 1u : 0u;
    ctx.gpr[2] = ctx.gpr[3];
}

// func_08BB0C58 / func_08BB0C78(first, last, value): every word of [first, last) = *value.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): an empty range (returns at once; the load of *value in the delay slot has no effect).
void func_08BB0C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_words(rt, ctx); }
void func_08BB0C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_words(rt, ctx); }

// func_08B8E4E0(first, last, value): every byte of [first, last) = *value; v0 = that byte (read even for an empty range).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): an empty range (v0 is still the byte: its lbu sits in the delay slot of the first beq).
void func_08B8E4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t p = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    const std::uint32_t value = rt.memory().load8(ctx.gpr[6]);
    ctx.gpr[2] = value;
    if (p == last) return;
    do {
        rt.memory().store8(p, static_cast<std::uint8_t>(value));
        ++p;
    } while (p != last);
}

// func_08BEF0EC(dest, count, source): count times: if (dest) *dest = *source (a word, re-read each time: dest may cover source); dest += 4.
// v0 = count.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): count == 0 (v0 = 0 from the delay slot), and a null dest (the copy is skipped, the count and dest still advance in the delay slots).
void func_08BEF0EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    ctx.gpr[2] = 0u;
    if (count == 0u) return;
    std::uint32_t done = 0u;
    do {
        ++done;
        if (dest != 0u) put(rt, dest, word(rt, source));
        dest += 4u;
    } while (count != done);
    ctx.gpr[2] = done;
}

// func_08B8A654(first, last, dest) -> dest after: copies each halfword of [first, last) to dest (a null dest only advancing).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null dest (the copy is skipped, dest still advances in the loop branch's delay slot).
void func_08B8A654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], destination = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    if (source != last) {
        do {
            if (destination != 0u) rt.memory().store16(destination, static_cast<std::uint16_t>(rt.memory().load16(source)));
            source += 2u;
            destination += 2u;
        } while (source != last);
    }
    ctx.gpr[2] = destination;
}

// func_08C3CEDC(this): tail-calls the vtable method at offset 8 of the object this[5] (a0 = that object); 0 when null.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null child (v0 = 0, a0 = 0 from the delay slot, returns).
void func_08C3CEDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 20u);
    ctx.gpr[2] = 0u;
    if (child == 0u) return;
    const std::uint32_t table = word(rt, child);
    const std::uint32_t method = word(rt, table + 8u);
    ctx.gpr[2] = table;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {child});
}

// func_08B8EB80 / func_08BD3608 / func_08BDB214 / func_08BDB238(this): the vtable method at offset 4 of the child this[3].
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null child (v0 = 0, returns).
void func_08B8EB80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx, 4u); }
void func_08BD3608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx, 4u); }
void func_08BDB214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx, 4u); }
void func_08BDB238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_member_12(rt, ctx, 4u); }

// func_08BD2BD8 / BF0 / C08 / C20 / C38(field, delta): *field += delta unless *field is null; v0 = the old *field.
void func_08BD2BD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field(rt, ctx); }
void func_08BD2BF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field(rt, ctx); }
void func_08BD2C08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field(rt, ctx); }
void func_08BD2C20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field(rt, ctx); }
void func_08BD2C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field(rt, ctx); }

// func_08BD2C94 / func_08BD2CDC / func_08BD2D00 / func_08BD2C68(field, delta): as above, then for a non-null *field a tail call (a0 = the new
// pointer, a1 = delta, a2 = field; func_08BD2C68 passes a1 = 0 and has v0 = the new pointer even when *field is null).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08BD2C68 with a non-null *field (the store and the tail call with a1 = 0); the null *field of the others is taken (v0 = 0).
void func_08BD2C94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field_then(rt, ctx, 0x0890815Cu, false); }
void func_08BD2CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field_then(rt, ctx, 0x08909CB4u, false); }
void func_08BD2D00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field_then(rt, ctx, 0x08903958u, false); }
void func_08BD2C68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { add_to_field_then(rt, ctx, 0x08AF98F0u, true); }

// func_08BD31D4 / func_08BD3400(a0, object): runs func_08BD31C0 / func_08BD31AC(a0, object), then sets bit 1 of the word at +0x28 of the
// record object[0x68 / 4] when object is not null. v0 = the new word, else the callee's v0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null object (returns with the callee's v0).
void func_08BD31D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08BD31C0u, {ctx.gpr[4], object});
    }
    if (object == 0u) return;
    const std::uint32_t record = word(rt, object + 0x68u);
    const std::uint32_t flags = word(rt, record + 0x28u) | 2u;
    ctx.gpr[2] = flags;
    ctx.gpr[3] = record;
    put(rt, record + 0x28u, flags);
}
void func_08BD3400(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08BD31ACu, {ctx.gpr[4], object});
    }
    if (object == 0u) return;
    const std::uint32_t record = word(rt, object + 0x68u);
    const std::uint32_t flags = word(rt, record + 0x28u) | 2u;
    ctx.gpr[2] = flags;
    ctx.gpr[3] = record;
    put(rt, record + 0x28u, flags);
}

// func_08C2822C(this) -> whether the record this[2] has a nonzero word at +0x50 (a null record calls func_08A5BB7C first).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a zero word at +0x50 (v0 = 0), and a null record (the assert call, then the load goes on).
void func_08C2822C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t record = checked_holder(rt, ctx, word(rt, ctx.gpr[4] + 8u), 0x08CB14E0u);
    ctx.gpr[2] = word(rt, record + 0x50u) != 0u ? 1u : 0u;
}

// func_08AA28DC(this): the holder this[0] (checked, message 0x08CB2B50), then the tail call of its vtable method at offset 0x50 (a0 = holder).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder (the assert call with the message, then the loads go on).
void func_08AA28DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = checked_holder(rt, ctx, word(rt, ctx.gpr[4]), 0x08CB2B50u);
    const std::uint32_t method = word(rt, word(rt, holder) + 0x50u);
    ctx.gpr[2] = method;
    ctx.gpr[25] = method;
    tail_call(ctx, method, {holder});
}

// func_08AA2A74 / func_08AA2AB8(this): the holder this[0] (checked), then its vtable method at offset 0x38 / 0x34 called with a0 = holder; v0 = its result.
void func_08AA2A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = checked_holder(rt, ctx, word(rt, ctx.gpr[4]), 0x08CB2B50u);
    const std::uint32_t method = word(rt, word(rt, holder) + 0x38u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, method, {holder});
}
void func_08AA2AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = checked_holder(rt, ctx, word(rt, ctx.gpr[4]), 0x08CB2B50u);
    const std::uint32_t method = word(rt, word(rt, holder) + 0x34u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, method, {holder});
}

// func_08B86CA8(this): the holder this[0x38 / 4] (checked, message 0x08C97DC4), then the tail call of its vtable method at offset 0x124 (a0 = holder).
// When that method is this very function (a wrapper around a wrapper) the tail call is a loop here: the harness reads a jump to the entry
// being compared as a return.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder (the assert call, then the loads go on).
void func_08B86CA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kSelf = 0x08B86CA8u;
    std::uint32_t self = ctx.gpr[4];
    for (;;) {
        const std::uint32_t holder = checked_holder(rt, ctx, word(rt, self + 0x38u), 0x08C97DC4u);
        const std::uint32_t method = word(rt, word(rt, holder) + 0x124u);
        ctx.gpr[2] = method;
        ctx.gpr[25] = method;
        if (method != kSelf) {
            tail_call(ctx, method, {holder});
            return;
        }
        self = holder;
        ctx.gpr[4] = holder;
    }
}

// func_08B978AC / func_08B9AB6C(this, object): nothing when object is null; else func_08873EA0 / func_0887B2EC(object), then operator delete (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null object (returns with v0 untouched).
void func_08B978AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    if (object == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08873EA0u, {object});
    }
    tail_call(ctx, kDelete, {object});
}
void func_08B9AB6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    if (object == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x0887B2ECu, {object});
    }
    tail_call(ctx, kDelete, {object});
}

// func_08BE4458(this): nothing when this[3] is null; else func_08962A20(child), then operator delete (tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null child (returns with v0 untouched).
void func_08BE4458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t child = word(rt, ctx.gpr[4] + 12u);
    if (child == 0u) return;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08962A20u, {child});
    }
    tail_call(ctx, kDelete, {child});
}

// func_08935F64(this): v0 = 0; when the signed byte this[0x10] is not -1, tail call func_08935F44(this, that byte).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the byte -1 (returns v0 = 0, no tail call).
void func_08935F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(ctx.gpr[4] + 0x10u))));
    ctx.gpr[2] = 0u;
    ctx.gpr[3] = 0xFFFFFFFFu;
    if (value == 0xFFFFFFFFu) return;
    tail_call(ctx, 0x08935F44u, {ctx.gpr[4], value});
}

// func_088841D0(this, a, b, c): v0 = 0 when this[2] is null; else tail call func_08883E74(this[3], this[2], a, b & 255, c & 255, this), v0 = a.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): this[2] null (v0 = 0, no tail call; the argument masks in the delay slots are dead then).
void func_088841D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t target = word(rt, self + 8u);
    const std::uint32_t a = ctx.gpr[5], b = ctx.gpr[6] & 255u, c = ctx.gpr[7] & 255u;
    ctx.gpr[2] = a;
    ctx.gpr[3] = b;
    if (target == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08883E74u, {word(rt, self + 12u), target, a, b, c, self});
}

// func_08B93AE8(tree, key) -> the first node whose key (a signed word at +0x10) is not below *key (std::map::lower_bound: nodes {.., left +8, right
// +12}, root tree[2]); tree + 4 (the end node) when there is none.
void func_08B93AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t end = ctx.gpr[4] + 4u;
    std::uint32_t result = end;
    std::uint32_t node = word(rt, ctx.gpr[4] + 8u);
    if (node != 0u) {
        const std::int32_t key = static_cast<std::int32_t>(word(rt, ctx.gpr[5]));
        do {
            if (static_cast<std::int32_t>(word(rt, node + 0x10u)) < key) {
                node = word(rt, node + 12u);
            } else {
                result = node;
                node = word(rt, node + 8u);
            }
        } while (node != 0u);
    }
    ctx.gpr[2] = result;
}

// func_0886BAF8 / func_0886BC14(this): the word at this[1] + 0x150 decremented / incremented (an ll/sc loop that succeeds: v0 = 1).
void func_0886BAF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t at = word(rt, ctx.gpr[4] + 4u) + 0x150u;
    put(rt, at, word(rt, at) - 1u);
    ctx.gpr[3] = at;
    ctx.gpr[2] = 1u;
}
void func_0886BC14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t at = word(rt, ctx.gpr[4] + 4u) + 0x150u;
    put(rt, at, word(rt, at) + 1u);
    ctx.gpr[3] = at;
    ctx.gpr[2] = 1u;
}

} // namespace hot_batch_64
} // namespace gt::game

#define HOT64(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_64::name, gt::ReturnKind::kind)
HOT64(0x08B49858u, func_08B49858, Word);
HOT64(0x08BB0C58u, func_08BB0C58, Word);
HOT64(0x08BB0C78u, func_08BB0C78, Word);
HOT64(0x08B8E4E0u, func_08B8E4E0, Word);
HOT64(0x08BEF0ECu, func_08BEF0EC, Word);
HOT64(0x08B8A654u, func_08B8A654, Word);
HOT64(0x08C3CEDCu, func_08C3CEDC, Word);
HOT64(0x08B8EB80u, func_08B8EB80, Word);
HOT64(0x08BD3608u, func_08BD3608, Word);
HOT64(0x08BDB214u, func_08BDB214, Word);
HOT64(0x08BDB238u, func_08BDB238, Word);
HOT64(0x08BD2BD8u, func_08BD2BD8, Word);
HOT64(0x08BD2BF0u, func_08BD2BF0, Word);
HOT64(0x08BD2C08u, func_08BD2C08, Word);
HOT64(0x08BD2C20u, func_08BD2C20, Word);
HOT64(0x08BD2C38u, func_08BD2C38, Word);
HOT64(0x08BD2C94u, func_08BD2C94, Word);
HOT64(0x08BD2CDCu, func_08BD2CDC, Word);
HOT64(0x08BD2D00u, func_08BD2D00, Word);
HOT64(0x08BD2C68u, func_08BD2C68, Word);
HOT64(0x08BD31D4u, func_08BD31D4, Word);
HOT64(0x08BD3400u, func_08BD3400, Word);
HOT64(0x08C2822Cu, func_08C2822C, Word);
HOT64(0x08AA28DCu, func_08AA28DC, DoubleWord);
HOT64(0x08AA2A74u, func_08AA2A74, Word);
HOT64(0x08AA2AB8u, func_08AA2AB8, Word);
HOT64(0x08B86CA8u, func_08B86CA8, Word);
HOT64(0x08B978ACu, func_08B978AC, Word);
HOT64(0x08B9AB6Cu, func_08B9AB6C, Word);
HOT64(0x08BE4458u, func_08BE4458, Word);
HOT64(0x08935F64u, func_08935F64, Word);
HOT64(0x088841D0u, func_088841D0, Word);
// Written but not registered: no route calls it (compared 0 in all five routes of the fast gate).
// HOT64(0x08B93AE8u, func_08B93AE8, Word);
HOT64(0x0886BAF8u, func_0886BAF8, Word);
HOT64(0x0886BC14u, func_0886BC14, Word);
