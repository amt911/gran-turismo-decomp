// Main-session batch 64 (draft, unverified): large (720-800 byte) functions, structured C from `tools.disasm`.
// Each one keeps the callee-saved registers untouched (the original saves and restores them), calls guest code through call()
// / call_indirect(), and ends with the v0 the asm ends with. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_64 {
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

// `lw v0, slot(vtable); jalr v0` on `object`.
u32 virtual_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot) {
    const u32 function = word(rt, word(rt, object) + slot);
    return call_indirect(rt, ctx, function, {object});
}

// The ll/sc pair that decrements a counter word; returns the value it had.
u32 decrement(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}

// A shared-pointer style release of a non-null control block: the word at +4 counts users (the last one calls virtual +8), the word at
// +8 counts weak users (the last one calls virtual +0xC).
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 block) {
    if (decrement(rt, block + 4u) != 1u) return;
    virtual_call(rt, ctx, block, 8u);
    if (decrement(rt, block + 8u) != 1u) return;
    virtual_call(rt, ctx, block, 0xCu);
}

// 0x08B62A04 (a0 = list owner, a1 = key, a2 = optional base object, a3 = result block {+0 word, +4 status, +8 flags, +0xC word}):
// asks 0x08B62884 first (non-zero: done, v0 = 1); otherwise walks the owner's entries (count at +0xC, entry i = {pointer +0x10+8i,
// info +0x14+8i}) from the last one down, asking each entry's object (virtual +0x18) to fill a stack record, and merges the record into a3.
// v0 = 1 on every early exit, otherwise (status word at a3 + 4) != 0.
void func_08B62A04(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 base = ctx.gpr[6];
    const u32 out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    if (call(rt, ctx, 0x08B62884u, {owner, key, base, out}) != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    u32 flags = word(rt, out + 8u);
    if ((flags & 0x10u) != 0u) flags = word(rt, owner + 8u);
    const u32 count = word(rt, owner + 0xCu);
    for (u32 done = 0u; done != count; ++done) {
        const u32 entry = owner + 0x10u + 8u * (count - 1u - done);
        const u32 info = word(rt, entry + 4u);
        const u32 delta = static_cast<u32>(static_cast<s32>(info) >> 8);
        const u32 bit1 = (info >> 1) & 1u;
        const u32 bit0 = info & 1u;
        put(rt, frame.at(0), 0u);
        put(rt, frame.at(4), 0u);
        put(rt, frame.at(8), flags);
        put(rt, frame.at(0xC), 0u);
        if (bit1 == 0u && (flags & 1u) == 0u) continue;
        u32 adjusted = 0u;
        if (base != 0u) {
            adjusted = base + delta;
            if (bit0 != 0u) adjusted = base + word(rt, word(rt, base) + delta);
        }
        const u32 object = word(rt, entry);
        const u32 function = word(rt, word(rt, object) + 0x18u);
        if (call_indirect(rt, ctx, function, {object, key, adjusted, frame.at(0)}) == 0u) continue;
        if (word(rt, frame.at(0xC)) == 8u && bit0 != 0u) put(rt, frame.at(0xC), word(rt, entry));
        u32 record_flags = word(rt, frame.at(4));
        if (static_cast<s32>(record_flags) >= 4 && bit1 == 0u) {
            record_flags &= ~2u;
            put(rt, frame.at(4), record_flags);
        }
        const u32 previous = word(rt, out + 0xCu);
        if (previous == 0u) {
            const u32 kind = word(rt, frame.at(0));
            const u32 merged_flags = word(rt, frame.at(4));
            const u32 merged_object = word(rt, frame.at(0xC));
            put(rt, out + 8u, word(rt, frame.at(8)));
            put(rt, out + 0u, kind);
            put(rt, out + 0xCu, merged_object);
            put(rt, out + 4u, merged_flags);
            if (static_cast<s32>(merged_flags) < 4) {
                ctx.gpr[2] = 1u;
                return;
            }
            if ((merged_flags & 2u) != 0u) {
                if ((word(rt, owner + 8u) & 1u) == 0u) {
                    ctx.gpr[2] = 1u;
                    return;
                }
            } else {
                if ((merged_flags & 1u) == 0u || (word(rt, owner + 8u) & 2u) == 0u) {
                    ctx.gpr[2] = 1u;
                    return;
                }
            }
            continue;
        }
        const u32 current_kind = word(rt, out + 0u);
        const u32 record_kind = word(rt, frame.at(0));
        if (current_kind != record_kind) {
            put(rt, out + 4u, 2u);
            put(rt, out + 0u, 0u);
            ctx.gpr[2] = 1u;
            return;
        }
        if (current_kind == 0u) {
            const u32 record_object = word(rt, frame.at(0xC));
            if (record_object == 8u || previous == 8u || word(rt, record_object + 4u) != word(rt, previous + 4u)) {
                put(rt, out + 4u, 2u);
                ctx.gpr[2] = 1u;
                return;
            }
        }
        put(rt, out + 4u, word(rt, out + 4u) | word(rt, frame.at(4)));
    }
    ctx.gpr[2] = word(rt, out + 4u) != 0u ? 1u : 0u;
}

// 0x0886D5A0 and 0x0886D88C (identical code; a0 = this): a destructor. Stores the vtable, runs 0x0886BF44(this, 0) and four times
// 0x0886B600, and (when the object at +4 exists) tears the sub-object down: three virtual +0x2C releases of the slots at +0x1C..+0x24,
// three list clears, releases of the shared blocks at +0x13C, +0x108, +0x100 and +0xF4, then member destructors and a tail call to 0x08B64654.
u32 destroy_body(Runtime &rt, AllegrexContext &ctx, u32 self, bool delete_inner) {
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CBF748u);
    call(rt, ctx, 0x0886BF44u, {self, 0u});
    for (u32 round = 0u; round != 4u; ++round) call(rt, ctx, 0x0886B600u);
    const u32 inner = word(rt, self + 4u);
    if (inner == 0u) return 0u;
    for (u32 slot = 0u; slot != 3u; ++slot) {
        const u32 member = word(rt, inner + 0x1Cu + 4u * slot);
        if (member == 0u) continue;
        virtual_call(rt, ctx, member, 0x2Cu);
        put(rt, inner + 0x1Cu + 4u * slot, 0u);
    }
    const u32 part = inner + 0xF8u;
    call(rt, ctx, 0x08B95120u, {inner + 0x1F4u, word(rt, inner + 0x1F4u + 8u)});
    call(rt, ctx, 0x08B93D3Cu, {inner + 0x1DCu, word(rt, inner + 0x1DCu + 8u)});
    call(rt, ctx, 0x08B95080u, {inner + 0x1C4u, word(rt, inner + 0x1C4u + 8u)});
    const u32 first_block = word(rt, inner + 0x13Cu);
    put(rt, inner + 0xF8u, 0x08CC6850u);
    if (first_block != 0u) release_shared(rt, ctx, first_block);
    put(rt, part, 0x08CC6838u);
    call(rt, ctx, 0x08B28DC4u, {part + 0x18u});
    const u32 second_block = word(rt, part + 0x10u);
    if (second_block != 0u) release_shared(rt, ctx, second_block);
    const u32 third_block = word(rt, part + 8u);
    if (third_block != 0u) release_shared(rt, ctx, third_block);
    const u32 fourth_block = word(rt, inner + 0xF4u);
    if (fourth_block != 0u) release_shared(rt, ctx, fourth_block);
    call(rt, ctx, 0x0887D4ECu, {inner + 0x88u});
    call(rt, ctx, 0x0890AFC0u, {inner + 0x60u});
    put(rt, inner + 0x38u, 0x08CBFF08u);
    call(rt, ctx, 0x0887C8E8u, {inner + 0x3Cu});
    put(rt, inner + 0x38u, 0x08CD7118u);
    put(rt, inner, 0x08CBF758u);
    call(rt, ctx, 0x08B1FD74u, {inner});
    if (delete_inner) call(rt, ctx, 0x08B64654u, {inner});
    return inner;
}
void destroy_with_subobject(Runtime &rt, AllegrexContext &ctx) {
    const u32 inner = destroy_body(rt, ctx, ctx.gpr[4], false);
    if (inner != 0u) tail_call(ctx, 0x08B64654u, {inner});
}
// 0x0886D0CC (a0 = this): the same teardown, but the sub-object is deleted with a call to 0x08B64654 and the object itself with a tail call.
void destroy_deleting_subobject(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    destroy_body(rt, ctx, self, true);
    tail_call(ctx, 0x08B64654u, {self});
}

// 0x0889E6E8 and 0x0889E9F4 (identical code; a0 = this): a destructor. Stores the vtable, calls the virtual +0 of the object at +0x34
// (a1 = the word at +0x30) and clears both, destroys the member at +0x28 and the object at +0x24 (its count at +0x2C decides whether it is
// freed here or just decremented, with the handler pair at +0x34/+0x38 called afterwards), releases the shared blocks at +0x20, +0x18,
// +0x10 and +8 and stores the final vtable; v0 = that vtable.
void destroy_with_shared_members(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC2420u);
    const u32 handler = word(rt, self + 0x34u);
    if (handler != 0u) {
        call_indirect(rt, ctx, word(rt, word(rt, handler)), {handler, word(rt, self + 0x30u)});
        put(rt, self + 0x34u, 0u);
        put(rt, self + 0x30u, 0u);
    }
    call(rt, ctx, 0x0889A4F0u, {self + 0x28u});
    const u32 object = word(rt, self + 0x24u);
    call(rt, ctx, 0x08B34888u, {object});
    if (word(rt, object + 0x2Cu) == 0u) {
        call(rt, ctx, 0x08B34B24u, {object});
        const u32 callback = word(rt, object + 0x38u);
        if (callback != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, callback)), {callback, word(rt, object + 0x34u)});
            put(rt, object + 0x38u, 0u);
            put(rt, object + 0x34u, 0u);
        }
        call(rt, ctx, 0x08B28BE0u, {object});
        call(rt, ctx, 0x08B64654u, {object});
    } else {
        put(rt, object + 0x2Cu, word(rt, object + 0x2Cu) - 1u);
        u32 first = 0u;
        u32 second = 0u;
        if (word(rt, object + 0x2Cu) == 0u) {
            const u32 pending = word(rt, object + 0x34u);
            if (pending != 0u) {
                second = word(rt, object + 0x38u);
                first = pending;
                put(rt, object + 0x34u, 0u);
                put(rt, object + 0x38u, 0u);
            }
            if (rt.memory().load8(object + 0x30u) != 0u) call(rt, ctx, 0x08B34C1Cu, {object});
        }
        call(rt, ctx, 0x08B34B24u, {object});
        if (first != 0u) virtual_call(rt, ctx, first, 8u);
        if (second != 0u) call_indirect(rt, ctx, word(rt, word(rt, second)), {second, first});
    }
    const u32 offsets[4] = {0x20u, 0x18u, 0x10u, 8u};
    for (const u32 offset : offsets) {
        const u32 block = word(rt, self + offset);
        if (block != 0u) release_shared(rt, ctx, block);
    }
    put(rt, self, 0x08CC24F0u);
    ctx.gpr[2] = 0x08CC24F0u;
}

// A release of a non-null block that only holds the weak count at +8: the last one calls virtual +0xC.
void release_weak(Runtime &rt, AllegrexContext &ctx, u32 block) {
    if (decrement(rt, block + 8u) != 1u) return;
    virtual_call(rt, ctx, block, 0xCu);
}

// 0x08BE1AD4 (a0 = this): a destructor. Stores the vtable 0x08CC68D0, releases the shared block at +0x68, drops one use of the counted string
// at +0x50 (0x08B60804 on the word before it, the representation freed through 0x08B93554 when the count is not positive, unless it is
// the shared empty one 0x08D75298), releases the blocks at +0x4C, +0x44, +0x3C and +0x2C, stores 0x08CC68F0, drops the weak count of
// the block at +0x24 and stores the final vtable; v0 = that vtable 0x08CC3000.
void destroy_members_08BE(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self) {
    put(rt, self, 0x08CC68D0u);
    const u32 first = word(rt, self + 0x68u);
    if (first != 0u) release_shared(rt, ctx, first);
    const u32 text = word(rt, self + 0x50u);
    if (text - 0xCu != 0x08D75298u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, 0x08B93554u, {text - 0xCu, frame.at(0)});
    }
    const u32 offsets[4] = {0x4Cu, 0x44u, 0x3Cu, 0x2Cu};
    for (const u32 offset : offsets) {
        if (offset == 0x2Cu) put(rt, self, 0x08CC68F0u);
        const u32 block = word(rt, self + offset);
        if (block != 0u) release_shared(rt, ctx, block);
    }
    const u32 weak = word(rt, self + 0x24u);
    if (weak != 0u) release_weak(rt, ctx, weak);
    put(rt, self, 0x08CC3000u);
    ctx.gpr[2] = 0x08CC3000u;
}
void func_08BE1AD4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    destroy_members_08BE(rt, ctx, frame, ctx.gpr[4]);
}
// 0x08BE1DA4: the same destructor, then a call to 0x08B64654(this) whose result is v0.
void func_08BE1DA4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    destroy_members_08BE(rt, ctx, frame, self);
    call(rt, ctx, 0x08B64654u, {self});
}

// 0x088D8CA8 (a0 = destination, a1 = source): copies four (word, shared block) pairs from a1 + 0, +8, +0x10, +0x18 into a0 + 0x678, +0x680,
// +0x688, +0x690 (the word first; the block counts a use for the new block and releases the old one when they differ), then tail-calls
// 0x08B90024(a0 + 0x698, a1 + 0x20).
void assign_shared_pair(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 source) {
    put(rt, destination, word(rt, source));
    const u32 old_block = word(rt, destination + 4u);
    const u32 new_block = word(rt, source + 4u);
    if (new_block == old_block) return;
    if (new_block != 0u) put(rt, new_block + 4u, word(rt, new_block + 4u) + 1u);
    const u32 current = word(rt, destination + 4u);
    if (current != 0u) release_shared(rt, ctx, current);
    put(rt, destination + 4u, new_block);
}

// 0x088FE6BC (a0 = this): takes the lock at this + 0x10 (0x08B344E0), clears three flag bytes (+9, +8, +0xC), then, in the record the word at +0
// points to, empties five (word, shared block) pairs ({+0x28, +0x2C}, {+0x20, +0x24}, {+8, +0xC}, {+0x34, +0x38}, then {+0x3C, +0x40} unconditionally):
// a pair whose first word is non-zero has both cleared and its block released (the record is re-read after each release); ends by
// tail-calling the unlock 0x08B3457C(this + 0x10).
void clear_record(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 lock = self + 0x10u;
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B344E0u, {lock});
        rt.memory().store8(self + 9u, 0u);
        rt.memory().store8(self + 8u, 0u);
        rt.memory().store8(self + 0xCu, 0u);
        const u32 pairs[4][2] = {{0x28u, 0x2Cu}, {0x20u, 0x24u}, {8u, 0xCu}, {0x34u, 0x38u}};
        for (const auto &pair : pairs) {
            const u32 record = word(rt, self);
            if (word(rt, record + pair[0]) == 0u) continue;
            const u32 block = word(rt, record + pair[1]);
            put(rt, record + pair[0], 0u);
            put(rt, record + pair[1], 0u);
            if (block != 0u) release_shared(rt, ctx, block);
        }
        const u32 record = word(rt, self);
        const u32 last = word(rt, record + 0x40u);
        put(rt, record + 0x3Cu, 0u);
        put(rt, record + 0x40u, 0u);
        if (last != 0u) release_shared(rt, ctx, last);
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// A double-precision value as the soft-float library passes it: low word in the even register, high word in the odd one.
struct Pair {
    u32 lo;
    u32 hi;
};
Pair constant_at(Runtime &rt, u32 address) { return {word(rt, address), word(rt, address + 4u)}; }
// A call into the soft-float library with two pair arguments; the result pair is v0:v1.
Pair call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, Pair first, Pair second) {
    const GuestResult result = call_guest(rt, ctx, target, {first.lo, first.hi, second.lo, second.hi}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return {result.v0, result.v1};
}

// 0x08B6D284 (a0:a1 = a double x, a2:a3 = a double y, t0 = flag): a polynomial in x^2 built from the soft-float helpers 0x08B71394 (product),
// 0x08B712B0 and 0x08B71328 (sum / difference), with the constants at 0x08CB68A0.. . When |x| is at most 0x3E3FFFFF in the high word and
// 0x08B71DC8(x, y) answers 0, the result is x itself. Returns the pair in v0:v1.
void func_08B6D284(Runtime &rt, AllegrexContext &ctx) {
    const Pair x = {ctx.gpr[4], ctx.gpr[5]};
    const Pair y = {ctx.gpr[6], ctx.gpr[7]};
    const u32 use_y = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);
    const auto mul = [&](Pair a, Pair b) { return call_pair(rt, ctx, 0x08B71394u, a, b); };
    const auto add = [&](Pair a, Pair b) { return call_pair(rt, ctx, 0x08B712B0u, a, b); };
    const auto sub = [&](Pair a, Pair b) { return call_pair(rt, ctx, 0x08B71328u, a, b); };
    if (!(static_cast<s32>(0x3E3FFFFF) < static_cast<s32>((x.hi << 1) >> 1))) {
        if (call(rt, ctx, 0x08B71DC8u, {x.lo, x.hi, y.lo, y.hi}) == 0u) {
            ctx.gpr[2] = x.lo;
            ctx.gpr[3] = x.hi;
            return;
        }
    }
    const Pair square = mul(x, x);
    const Pair cube = mul(x, square);
    Pair poly = mul(square, constant_at(rt, 0x08CB68A0u));
    poly = add(poly, constant_at(rt, 0x08CB68A8u));
    poly = mul(square, poly);
    poly = sub(poly, constant_at(rt, 0x08CB68B0u));
    poly = mul(square, poly);
    poly = add(poly, constant_at(rt, 0x08CB68B8u));
    poly = mul(square, poly);
    poly = sub(poly, constant_at(rt, 0x08CB68C0u));
    Pair result;
    if (use_y != 0u) {
        const Pair half = mul(y, constant_at(rt, 0x08CB68D0u));
        const Pair term = mul(cube, poly);
        Pair mixed = add(half, term);
        mixed = mul(square, mixed);
        mixed = add(mixed, y);
        const Pair scaled = mul(cube, constant_at(rt, 0x08CB68C8u));
        const Pair correction = sub(mixed, scaled);
        result = add(x, correction);
    } else {
        Pair correction = mul(square, poly);
        correction = add(correction, constant_at(rt, 0x08CB68C8u));
        correction = mul(cube, correction);
        result = sub(x, correction);
    }
    ctx.gpr[2] = result.lo;
    ctx.gpr[3] = result.hi;
}

// 0x08A3B5CC (a0 = object): a registration function. Asks the object's virtual +0x108 for a string through a stack slot (0x08B85EB8 builds
// the name 0x08CAD544), drops the string, swaps the counted object at +0x38 for the one 0x08A76A94 answers (0x08A5BA08 counts a use of the
// new one, 0x08A5B930 drops the old), then registers handlers on the object: one 0x08B8850C call, 18 name/handler pairs through 0x08B87108
// and four name/handler/handler triples through 0x08B878E8. v0 = the last call's result.
void func_08A3B5CC(Runtime &rt, AllegrexContext &ctx) {
    static const u32 pairs[][2] = {
        {0x08CAD590u, 0x08A3BD88u},
        {0x08CAD598u, 0x08A3ABF4u},
        {0x08CAD5A0u, 0x08A3BCB4u},
        {0x08CAD5A8u, 0x08A3C6B8u},
        {0x08CAD5B0u, 0x08A3C930u},
        {0x08CAD5BCu, 0x08A3BBE0u},
        {0x08CAD5C4u, 0x08A3BB0Cu},
        {0x08CAD5D0u, 0x08A3AC40u},
        {0x08CAD5D8u, 0x08A3ACDCu},
        {0x08CAD5E0u, 0x08A3B9E8u},
        {0x08CAD5E8u, 0x08A3B8C4u},
        {0x08CAD5F8u, 0x08A3AD70u},
        {0x08CAD600u, 0x08A3B220u},
        {0x08CAD60Cu, 0x08A3B2A8u},
        {0x08CAD618u, 0x08A3C544u},
        {0x08CAD624u, 0x08A3BE8Cu},
        {0x08CAD630u, 0x08A3B4B4u},
    };
    static const u32 triples[][3] = {
        {0x08CAD644u, 0x08A3B0D8u, 0x08A3AF3Cu},
        {0x08CAD64Cu, 0x08A3B050u, 0x08A3B540u},
        {0x08CAD654u, 0x08A3AFC8u, 0x08A3B428u},
        {0x08CAD65Cu, 0x08A3B160u, 0x08A3AB70u},
    };
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 handler = word(rt, word(rt, self) + 0x108u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4), 0x08CAD544u, frame.at(1)});
    call_indirect(rt, ctx, handler, {self, frame.at(4)});
    const u32 text = word(rt, frame.at(4));
    if (text - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, 0x08B8542Cu, {text - 0xCu, frame.at(0)});
    }
    const u32 replacement = call(rt, ctx, 0x08A76A94u);
    if (replacement != 0u) call(rt, ctx, 0x08A5BA08u, {replacement});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, replacement);
    call(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A3B1E0u});
    for (const auto &entry : pairs) call(rt, ctx, 0x08B87108u, {self, entry[0], entry[1]});
    for (const auto &entry : triples) call(rt, ctx, 0x08B878E8u, {self, entry[0], entry[1], entry[2]});
}

// One field read of 0x08884788: a small stack object is built from the stream (0x08AC2730 with the field index), read (0x08AC2894, its result
// is the value) and destroyed (0x08AC26DC, result unused).
u32 read_field(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 stream, u32 index) {
    call(rt, ctx, 0x08AC2730u, {slot, stream, index});
    const u32 value = call(rt, ctx, 0x08AC2894u, {slot});
    call(rt, ctx, 0x08AC26DCu, {slot});
    return value;
}

// 0x08884788 (a0 = this, a2 = stream): reads a four-character tag (0x08AC2690) and, for 'LPRB' (0x4C505242), 'REND' (0x52454E44) or 'DISP'
// (0x44495350), the tag's fields: 'LPRB' passes one value (0x08AC2900) to 0x08884758(this, value); 'DISP' puts one field in bit 3 of this[4]'s
// word +0x10; 'REND' reads nine fields into bits 0..2 of the word +0x10 and builds the flag word at +0xC from the others. v0 = 0 always.

} // namespace

// ---- helpers and drafts taken from src/game/unsorted/hot_batch_113.cpp (registration was commented out there: no comparable case)
namespace legacy_113 {
namespace {
using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

} // namespace

// The reference-count steps of a shared object: `ll/sc` on a word of guest memory (single-threaded here, the host never
// preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful
// `sc`); true when the old value was 1.
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// A null check the way these functions spell it: func_08A5BB7C(message) runs when `object` is zero (it is used anyway).
inline std::uint32_t checked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    return object;
}
// func_08A1AC08(self): on first use (guard byte 0x08CF63E0) the eight words at 0x08CF63E8 are filled with the results of
// func_08A2A110, func_08A2379C, func_08A21E28, func_08A214C8, func_08A20814, func_08A1CAD0, func_08A1C0B0 and
// func_08A1B2DC. While the table words are non-zero, func_08A7CC30(self->+0, word) runs for each. Then eight times: a
// value object is built in the frame (func_08A2982C, func_08A29588, func_08A1AEDC, func_08A278B0, func_08A270AC,
// func_08A25498, func_08A26B98, func_08A20010 at frame +0x1C .. +0), handed to func_08A7CE50(self->+0, object) (the
// target checked against 0x08CA9E50) and destroyed. Last func_08A2A724, func_08A2A824, func_08A2A5F8, func_08A2A950 and
// func_08A1ED68 on self.
void func_08A1AC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    constexpr std::uint32_t guard = 0x08CF63E0u;
    constexpr std::uint32_t table = 0x08CF63E8u;
    constexpr std::uint32_t message = 0x08CA9E50u;
    if (sext8(byte_u(rt, guard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        static constexpr std::uint32_t kBuilders[8] = {0x08A2A110u, 0x08A2379Cu, 0x08A21E28u, 0x08A214C8u,
                                                       0x08A20814u, 0x08A1CAD0u, 0x08A1C0B0u, 0x08A1B2DCu};
        for (std::uint32_t index = 0u; index < 8u; ++index) put(rt, table + index * 4u, call_set(rt, ctx, kBuilders[index], {}));
        call_set(rt, ctx, 0x08B64494u, {guard});
    }
    ctx.gpr[2] = word(rt, table);
    if (ctx.gpr[2] != 0u) {
        std::uint32_t entry = table;
        do {
            const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
            call_set(rt, ctx, 0x08A7CC30u, {target, word(rt, entry)});
            entry += 4u;
        } while (word(rt, entry) != 0u);
    }
    static constexpr std::uint32_t kMakers[8] = {0x08A2982Cu, 0x08A29588u, 0x08A1AEDCu, 0x08A278B0u,
                                                 0x08A270ACu, 0x08A25498u, 0x08A26B98u, 0x08A20010u};
    for (std::uint32_t index = 0u; index < 8u; ++index) {
        const std::uint32_t temporary = frame.at(0x1Cu - index * 4u);
        const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
        call_set(rt, ctx, kMakers[index], {temporary});
        call_set(rt, ctx, 0x08A7CE50u, {target, temporary});
        call_set(rt, ctx, kDestroy, {temporary});
    }
    call_set(rt, ctx, 0x08A2A724u, {self});
    call_set(rt, ctx, 0x08A2A824u, {self});
    call_set(rt, ctx, 0x08A2A5F8u, {self});
    call_set(rt, ctx, 0x08A2A950u, {self});
    call_set(rt, ctx, 0x08A1ED68u, {self});
}


// Copies one 32-byte element: the seven words at +0..+0x18 and the byte at +0x1C.
inline void copy_element(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source) {
    for (std::uint32_t offset = 0u; offset < 0x1Cu; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
    put8(rt, destination + 0x1Cu, byte_u(rt, source + 0x1Cu));
}

// func_08BAC714(vector, position, count, value): vector::insert(position, count, value) over 32-byte elements
// {begin +0, end +4, capacity end +8}. Nothing for count 0. With room (capacity - end >= count) the value is copied to
// a frame temporary (+0xC); when more than `count` elements follow the position the last `count` move up (func_08BAC354),
// the middle moves up backwards and func_08BAC3C0(position, position + count, temp) fills; otherwise func_08BAC688 fills
// the overflow, func_08BAC618 moves the tail and func_08BAC3C0 fills the rest. Without room a block of size + max(size,
// count) elements (func_08B5ADDC(0x08CA08A4) when over the maximum 0x7FFFFFF; func_08B35DA0 for the bytes) takes the
// prefix (func_08BAC53C), the new elements (func_08BAC5AC) and the suffix (func_08BAC53C); the old range is destroyed
// (func_08BAC238), freed (func_08B35254) and the three pointers replaced.
void func_08BAC714(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x30u), count);
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t temporary = frame.at(0xCu);
    if (!((static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 5)) < count)) {
        copy_element(rt, temporary, value);
        const std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 5);
        if (count < after) {
            call_set(rt, ctx, 0x08BAC354u, {end - (count << 5), end, end, frame.at(6u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + (count << 5));
            const std::uint32_t middle_end = end - (count << 5);
            const std::int32_t middle = signed_of(middle_end - position) >> 5;
            std::uint32_t source = middle_end;
            std::uint32_t destination = end;
            for (std::int32_t step = 0; step < middle; ++step) {
                source -= 0x20u;
                destination -= 0x20u;
                copy_element(rt, destination, source);
            }
            call_set(rt, ctx, 0x08BAC3C0u, {position, position + (count << 5), temporary});
            return;
        }
        call_set(rt, ctx, 0x08BAC688u, {end, count - after, temporary, frame.at(5u)});
        const std::uint32_t new_end = word(rt, vector + 4u) + ((count - after) << 5);
        put(rt, vector + 4u, new_end);
        call_set(rt, ctx, 0x08BAC618u, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (after << 5));
        call_set(rt, ctx, 0x08BAC3C0u, {position, end, temporary});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 5);
    put(rt, frame.at(8u), size);
    if (0x07FFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA08A4u});
    const std::uint32_t larger = size < count ? count : size;
    const std::uint32_t capacity = size + larger;
    std::uint32_t bytes;
    std::uint32_t block = 0u;
    if (capacity < size) bytes = 0xFFFFFFE0u;
    else bytes = capacity == 0u ? 0u : capacity << 5;
    if (capacity >= size && capacity == 0u) {
        block = 0u;
    } else {
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    }
    const std::uint32_t old_begin = word(rt, vector);
    std::uint32_t copied = call_set(rt, ctx, 0x08BAC53Cu, {old_begin, position, block, frame.at(3u)});
    const std::uint32_t filled = call_set(rt, ctx, 0x08BAC5ACu, {copied, word(rt, frame.at(0x30u)), value, frame.at(2u)});
    (void)filled;
    const std::uint32_t tail_target = (word(rt, frame.at(0x30u)) << 5) + copied;
    const std::uint32_t new_end = call_set(rt, ctx, 0x08BAC53Cu, {position, word(rt, vector + 4u), tail_target, frame.at(1u)});
    call_set(rt, ctx, 0x08BAC238u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    if (word(rt, vector) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, vector)});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, new_end);
    put(rt, vector, block);
}

// func_08A2C2B4(self): the same shape as func_08A1AC08 with another table: on first use (guard byte 0x08CF65A0) the 19
// words at 0x08CF65B8 are filled with the results of 19 builder functions (func_08A443E0 .. func_08A36D2C, in the order of
// the code); while the table words are non-zero func_08A7CC30(self->+0, word) runs for each; func_08AA3040(func_08A2C248(),
// self); func_08A4A3A0 / func_08A47388 / func_08A46DCC(self); four value objects (func_08A2C594, func_08A3D394,
// func_08A30AC8, func_08A2D068 at frame +0xC / +8 / +4 / +0) handed to func_08A7CE50(self->+0, object) (target checked
// against 0x08CAC0D0) and destroyed; then eighteen update functions on self (func_08A46340 .. func_08A4565C).
void func_08A2C2B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    constexpr std::uint32_t guard = 0x08CF65A0u;
    constexpr std::uint32_t table = 0x08CF65B8u;
    constexpr std::uint32_t message = 0x08CAC0D0u;
    if (sext8(byte_u(rt, guard)) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        static constexpr std::uint32_t kBuilders[19] = {0x08A443E0u, 0x08A43C90u, 0x08A40AD0u, 0x08A40ADCu, 0x08A3E9A4u,
                                                        0x08A3D6CCu, 0x08A3A2CCu, 0x08A398B0u, 0x08A38A2Cu, 0x08A37D0Cu,
                                                        0x08A34D34u, 0x08A3309Cu, 0x08A32264u, 0x08A3CB38u, 0x08A34088u,
                                                        0x08A2DDA0u, 0x08A2D108u, 0x08A31CBCu, 0x08A36D2Cu};
        for (std::uint32_t index = 0u; index < 19u; ++index) put(rt, table + index * 4u, call_set(rt, ctx, kBuilders[index], {}));
        call_set(rt, ctx, 0x08B64494u, {guard});
    }
    ctx.gpr[2] = word(rt, table);
    if (ctx.gpr[2] != 0u) {
        std::uint32_t entry = table;
        do {
            const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
            call_set(rt, ctx, 0x08A7CC30u, {target, word(rt, entry)});
            entry += 4u;
        } while (word(rt, entry) != 0u);
    }
    const std::uint32_t found = call_set(rt, ctx, 0x08A2C248u, {});
    call_set(rt, ctx, 0x08AA3040u, {found, self});
    call_set(rt, ctx, 0x08A4A3A0u, {self});
    call_set(rt, ctx, 0x08A47388u, {self});
    call_set(rt, ctx, 0x08A46DCCu, {self});
    static constexpr std::uint32_t kMakers[4] = {0x08A2C594u, 0x08A3D394u, 0x08A30AC8u, 0x08A2D068u};
    for (std::uint32_t index = 0u; index < 4u; ++index) {
        const std::uint32_t temporary = frame.at(0xCu - index * 4u);
        const std::uint32_t target = checked(rt, ctx, word(rt, self), message);
        call_set(rt, ctx, kMakers[index], {temporary});
        call_set(rt, ctx, 0x08A7CE50u, {target, temporary});
        call_set(rt, ctx, kDestroy, {temporary});
    }
    static constexpr std::uint32_t kUpdates[18] = {0x08A46340u, 0x08A46B74u, 0x08A4691Cu, 0x08A46A48u, 0x08A46CA0u, 0x08A459E0u,
                                                   0x08A45B0Cu, 0x08A45C38u, 0x08A45D64u, 0x08A45E90u, 0x08A45FBCu, 0x08A460E8u,
                                                   0x08A458B4u, 0x08A46214u, 0x08A45404u, 0x08A45788u, 0x08A45530u, 0x08A4565Cu};
    for (const std::uint32_t update : kUpdates) call_set(rt, ctx, update, {self});
}

// func_0889E258(self, source): source is a shared pointer {pointer +0, control block +4}. Its use count is raised twice
// (copies for the node and for the slot) and holder = self->+0x24 is locked (func_08B34888 / func_08B34B24). With
// holder->+0x2C zero func_0889DE18(self) runs; else a 16-byte node (func_08B62344(0x10): vtable 0x08CC2530, self, pointer,
// block; one more use count) replaces holder->+0x34 (the old object's virtual slot 0 first, then cleared) and holder->+0x38
// gets the static object at 0x08D75F10 (set up on first use behind the guard byte 0x08D75F08, vtable 0x08CC2560); the
// copy's use count is released. After the unlock the two remaining uses of the block are released, the very last
// weak release being a tail jump to the control block's virtual slot 0xC.

} // namespace legacy_113

// ---- helpers and drafts taken from src/game/unsorted/hot_batch_114.cpp (registration was commented out there: no comparable case)
namespace legacy_114 {
namespace {
using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
// The virtual call as the compiler wrote it with the registers swapped: `lw v0, 0(obj); lw v1, slot(v0); jalr v1`.
inline std::uint32_t call_virtual_v1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
    return call_set(rt, ctx, ctx.gpr[3], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

// A word read at any alignment (the original's lwl/lwr pair), little-endian.
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return byte_u(rt, address) | (byte_u(rt, address + 1u) << 8) | (byte_u(rt, address + 2u) << 16) | (byte_u(rt, address + 3u) << 24);
}

// A word stored at any alignment (the original's swl/swr pair), little-endian.
inline void put_unaligned(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    for (std::uint32_t index = 0u; index < 4u; ++index) put8(rt, address + index, value >> (index * 8u));
}
// Copies `bytes` (a multiple of 16) from `source` (any alignment) to the aligned `destination`, 16 bytes at a time: four
// words loaded, then four stored.
inline void copy_blocks(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source, std::uint32_t bytes) {
    for (std::uint32_t offset = 0u; offset != bytes; offset += 16u) {
        const std::uint32_t a = word_unaligned(rt, source + offset);
        const std::uint32_t b = word_unaligned(rt, source + offset + 4u);
        const std::uint32_t c = word_unaligned(rt, source + offset + 8u);
        const std::uint32_t d = word_unaligned(rt, source + offset + 12u);
        put(rt, destination + offset, a);
        put(rt, destination + offset + 4u, b);
        put(rt, destination + offset + 8u, c);
        put(rt, destination + offset + 12u, d);
    }
}
// wsbw: the bytes of a word reversed.
inline std::uint32_t bswap32(std::uint32_t value) { return __builtin_bswap32(value); }

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

} // namespace

// The reference-count steps of a shared object: `ll/sc` on a word of guest memory (single-threaded here, the host never
// preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful
// `sc`); true when the old value was 1.
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// func_08BBF0D0(vector, position, count, value): the libstdc++ vector::_M_fill_insert for 16-byte elements {string, word,
// word, word} (vector = {begin, end, capacity}). Nothing for count 0. With room: a frame element made from *value (string
// copy func_08B9432C, the three words copied); when more elements follow the position than count, the last `count` are
// copied up (func_08BBEF50), the earlier ones moved backward (string assign func_08B957D8 + the three words) and the gap
// filled (func_08BBD954); otherwise the surplus copies are made at the end (func_08BBEFC8), the tail copied after them
// (func_08BBF048) and the old gap filled; the frame string is released (empty rep 0x08D75298, free func_08B93554). Full: length
// checked (func_08B5ADDC(0x08CA0DC4) beyond 0x0FFFFFFF elements), a block of size + max(size, count) elements allocated
// (func_08B35DA0), front copied (func_08BBE8D0), count copies made (func_08BBE850), the rest copied, the old range destroyed
// (func_08BBEBE4) and freed (func_08B35254), the vector updated; v0 = block + bytes.
void func_08BBF0D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    if (count == 0u) return;
    if (static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - word(rt, vector + 4u)) >> 4) >= count) {
        const std::uint32_t copy = frame.at(0xCu);
        call_set(rt, ctx, 0x08B9432Cu, {copy, value});
        const std::uint32_t end = word(rt, vector + 4u);
        std::uint32_t after = static_cast<std::uint32_t>(signed_of(end - position) >> 4);
        put(rt, frame.at(0x10u), word(rt, value + 4u));
        put(rt, frame.at(0x14u), word(rt, value + 8u));
        put(rt, frame.at(0x18u), word(rt, value + 0xCu));
        if (count < after) {
            call_set(rt, ctx, 0x08BBEF50u, {end - count * 16u, end, end, frame.at(7u)});
            std::uint32_t source = end - count * 16u;
            after = static_cast<std::uint32_t>(signed_of(source - position) >> 4);
            put(rt, vector + 4u, word(rt, vector + 4u) + count * 16u);
            if (signed_of(after) > 0) {
                std::uint32_t target = end;
                for (std::uint32_t moved = 0u; moved != after; ++moved) {
                    target -= 0x10u;
                    source -= 0x10u;
                    call_set(rt, ctx, 0x08B957D8u, {target, source});
                    const std::uint32_t w1 = word(rt, source + 4u);
                    const std::uint32_t w2 = word(rt, source + 8u);
                    const std::uint32_t w3 = word(rt, source + 0xCu);
                    put(rt, target + 4u, w1);
                    put(rt, target + 8u, w2);
                    put(rt, target + 0xCu, w3);
                }
            }
            call_set(rt, ctx, 0x08BBD954u, {position, position + count * 16u, copy});
        } else {
            call_set(rt, ctx, 0x08BBEFC8u, {end, count - after, copy, frame.at(6u)});
            const std::uint32_t moved_end = word(rt, vector + 4u) + (count - after) * 16u;
            put(rt, vector + 4u, moved_end);
            call_set(rt, ctx, 0x08BBF048u, {position, end, moved_end, frame.at(5u)});
            put(rt, vector + 4u, word(rt, vector + 4u) + after * 16u);
            call_set(rt, ctx, 0x08BBD954u, {position, end, copy});
        }
        release_string(rt, ctx, copy, 0x08D75298u, 0x08B93554u, frame.at(0u));
        return;
    }
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - word(rt, vector)) >> 4);
    if (0x0FFFFFFFu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0DC4u});
    const std::uint32_t bigger = size < count ? count : size;
    const std::uint32_t grown = size + bigger;
    const bool overflow = grown < size;
    const std::uint32_t bytes = overflow ? 0xFFFFFFF0u : grown << 4;
    std::uint32_t block = 0u;
    if (overflow || grown != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t middle = call_set(rt, ctx, 0x08BBE8D0u, {word(rt, vector), position, block, frame.at(4u)});
    call_set(rt, ctx, 0x08BBE850u, {middle, count, value, frame.at(3u)});
    const std::uint32_t finish = call_set(rt, ctx, 0x08BBE8D0u, {position, word(rt, vector + 4u), middle + count * 16u, frame.at(2u)});
    call_set(rt, ctx, 0x08BBEBE4u, {word(rt, vector), word(rt, vector + 4u), frame.at(1u)});
    const std::uint32_t begin = word(rt, vector);
    if (begin != 0u) call_set(rt, ctx, 0x08B35254u, {begin});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, finish);
    put(rt, vector, block);
}

} // namespace legacy_114

// ---- helpers and drafts taken from src/game/unsorted/hot_batch_115.cpp (registration was commented out there: no comparable case)
namespace legacy_115 {
namespace {
using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

} // namespace

// The reference-count steps of a shared object: `ll/sc` on a word of guest memory (single-threaded here, the host never
// preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful
// `sc`); true when the old value was 1.
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// Copies `count` words from `source` to `destination` (a struct copy the original unrolls into 16-byte groups).
inline void copy_words(psprecomp::Runtime &rt, std::uint32_t destination, std::uint32_t source, std::uint32_t count) {
    for (std::uint32_t index = 0u; index < count; ++index) put(rt, destination + index * 4u, word(rt, source + index * 4u));
}

// The virtual call as the compiler wrote it with the registers swapped: `lw v0, 0(obj); lw v1, slot(v0); jalr v1`.
inline std::uint32_t call_virtual_v1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
    return call_set(rt, ctx, ctx.gpr[3], arguments);
}
// A null check the way these functions spell it: func_08A5BB7C(message) runs when `object` is zero (it is used anyway).
inline std::uint32_t checked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    return object;
}
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return byte_u(rt, address) | (byte_u(rt, address + 1u) << 8) | (byte_u(rt, address + 2u) << 16) | (byte_u(rt, address + 3u) << 24);
}
inline void put_unaligned(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    for (std::uint32_t index = 0u; index < 4u; ++index) put8(rt, address + index, value >> (index * 8u));
}

// Puts `item` (when non-null) on the free chain of the size-class record that func_08BB1F48()'s table gives for `key`
// (looked up, or made with func_08BB1D0C(+0x24, +0x28, &key, 0x08BB1B40) when missing or of another class): the record's
// +0x1C count goes down and the chain head (+0xC) takes the item; v0 = the new count and v1 = the old head then.
inline void return_to_class(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t key, std::uint32_t item, std::uint32_t key_slot) {
    const std::uint32_t context = call_set(rt, ctx, 0x08BB1F48u, {});
    put(rt, key_slot, key);
    std::uint32_t record = word(rt, context + 0x34u);
    if (record == 0u || word(rt, record + 0x10u) != key) {
        const std::uint32_t found = call_set(rt, ctx, 0x08BB1D0Cu, {word(rt, context + 0x24u), word(rt, context + 0x28u), key_slot, 0x08BB1B40u});
        record = word(rt, found);
        put(rt, context + 0x34u, record);
    }
    if (item != 0u) {
        const std::uint32_t head = word(rt, record + 0xCu);
        const std::uint32_t count = word(rt, record + 0x1Cu) - 1u;
        put(rt, record + 0x1Cu, count);
        put(rt, item, head);
        put(rt, record + 0xCu, item);
        ctx.gpr[2] = count;
        ctx.gpr[3] = head;
    }
    ctx.gpr[4] = record;
}

// func_08AD0C9C(self, list): under the lock at self + 4 (after self's virtual slot 0x6C), for up to 8 entries of `list` (stop
// at a null one, skip empty strings): func_08B0EC60 fills a 60-byte record in the frame; when its first word is 0 the size
// word at +0x10 rounded up to 128 goes to a frame size table and total += size + 128. With a total, memory is claimed
// (func_08B2E0B0, func_08B35418 limit, func_08B35DA0 for half the spare) and each sized entry gets func_08B15490(size, 1).
// A second pass zeroes self->+0x74[i] and, for sized entries, builds a record with func_08B10170, keeps its +0x20 word in
// self->+0x74[i] and, when func_08AF6CE8 accepts it, runs func_08AF771C(obj, 0) and func_08AF7B0C(obj, -1). The unlock is
// a tail jump to func_08B34B24.

// func_08B13388(self): func_08B33790(self + 0xAC); a frame request {self + 0x12C, 0x80, 0} goes through func_08B1262C /
// func_08B23A70(.., 0x08CB4BD4); then func_08C7A848(self + 0xAC, request) is polled (func_08B2E064 between tries) until it
// answers 0 and func_08B33C0C(.., 1, 0) returns >= 0 (self->+0x1B0 / +0x1B4 take frame +0x48 and that result). A 64-byte-aligned
// frame buffer is read by func_08B11F30; unless its first word is 0x71D319F3 or 0x515111D3 that is all (v0 = 0x515111D3).
// Otherwise its fields are normalised (func_08B1222C), its first 40 bytes copied to self->+0x208 and the buffers made
// (func_08B15490) and read in (func_08B11F30) per the format word, ending with func_08B11E3C(self).

} // namespace legacy_115

// ---- helpers and drafts taken from src/game/unsorted/hot_batch_116.cpp (registration was commented out there: no comparable case)
namespace legacy_116 {
namespace {
using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

} // namespace

// The reference-count steps of a shared object: `ll/sc` on a word of guest memory (single-threaded here, the host never
// preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful
// `sc`); true when the old value was 1.
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// A null check the way these functions spell it: func_08A5BB7C(message) runs when `object` is zero (it is used anyway).
inline std::uint32_t checked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    return object;
}

// The virtual call as the compiler wrote it with the registers swapped: `lw v0, 0(obj); lw v1, slot(v0); jalr v1`.
inline std::uint32_t call_virtual_v1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[3] = word(rt, ctx.gpr[2] + slot);
    return call_set(rt, ctx, ctx.gpr[3], arguments);
}

// A count of 24-byte elements from a byte distance: the compiler's `sra 3` and multiply by the inverse of 3 (0xAAAAAAAB).
inline std::uint32_t elements24(std::uint32_t distance) { return static_cast<std::uint32_t>(signed_of(distance) >> 3) * 0xAAAAAAABu; }
// func_08AAADC8(self, key): func_08AAA2C0(); with owner = self->+4 (every use checked against 0x08CB30B0): func_08A71B00(owner,
// word 0x08CDF808), `mode` = owner's virtual slot 0x118, func_08C312E4(owner, 0), func_08C34248(self, func_08A76ABC(temp, key)),
// `old` = slot 0x118 again. The vector at self + 0x38 (begin +0x38, end +0x3C) is resized to the count at self->+0x34 (the
// shared-string result held in frame +4 is released); the tree nodes of self + 0x20 are walked (func_08B5F100) and each
// handed to func_08B855E0(begin + node->+0x14 * 4, node + 0x10); func_08A71B00(owner, count); for each of the count
// entries func_08B88F44(frame +8, begin + 4i) and func_08C312EC(owner, it) (the string released); then slot 0x114(owner, mode),
// func_08C312E4(owner, old) and func_08AA97D4(self).

// func_08C4BD80(vector, position, count, value): the libstdc++ vector<24-byte element>::insert(position, count, value). Nothing
// when count == 0. The value (6 words) is copied to frame +0xC first. With room (capacity +8 - end +4, in elements): the elements
// after the position are moved up by count (func_08C4AD94 for the tail copy, a backward word-by-word copy for the middle,
// func_08C4AF5C fills) or, when fewer than count follow, func_08C4AEAC / func_08C4AFB4 / func_08C4AF5C. Full: a block of size
// + max(size, count) elements (func_08B5ADDC(0x08CB4AFC) beyond the maximum 0x0AAAAAAA; func_08B35DA0 allocates) receives
// the front (func_08C4B568), the new elements (func_08C4B50C) and the back (func_08C4B568); the old range is destroyed
// (func_08C498C4), the old block freed (func_08B35254) and begin / end / capacity updated.
void func_08C4BD80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4];
    const std::uint32_t position = ctx.gpr[5];
    const std::uint32_t count = ctx.gpr[6];
    const std::uint32_t value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    put(rt, frame.at(0x30u), count);
    if (count == 0u) return;
    std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = elements24(word(rt, vector + 8u) - end);
    if (!(room < count)) {
        const std::uint32_t before = static_cast<std::uint32_t>(signed_of(end - position) >> 3);
        const std::uint32_t after = before * 0xAAAAAAABu;
        const std::uint32_t copy = frame.at(0xCu);
        for (std::uint32_t index = 0u; index < 6u; ++index) put(rt, copy + index * 4u, word(rt, value + index * 4u));
        if (count < after) {
            const std::uint32_t bytes = count * 24u;
            call_set(rt, ctx, 0x08C4AD94u, {end - bytes, end, end, frame.at(6u)});
            const std::uint32_t source_end = end - bytes;
            const std::uint32_t middle = elements24(source_end - position);
            put(rt, vector + 4u, word(rt, vector + 4u) + bytes);
            std::uint32_t source = source_end;
            std::uint32_t destination = end;
            for (std::uint32_t index = 0u; signed_of(middle) > 0 && index != middle; ++index) {
                source -= 24u;
                destination -= 24u;
                std::uint32_t words[6];
                for (std::uint32_t k = 0u; k < 6u; ++k) words[k] = word(rt, source + k * 4u);
                for (std::uint32_t k = 0u; k < 6u; ++k) put(rt, destination + k * 4u, words[k]);
            }
            call_set(rt, ctx, 0x08C4AF5Cu, {position, position + count * 24u, copy});
            return;
        }
        call_set(rt, ctx, 0x08C4AEACu, {end, count - after, copy, frame.at(5u)});
        const std::uint32_t new_end = word(rt, vector + 4u) + (count - after) * 24u;
        put(rt, vector + 4u, new_end);
        call_set(rt, ctx, 0x08C4AFB4u, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (before << 3));
        call_set(rt, ctx, 0x08C4AF5Cu, {position, end, copy});
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = elements24(end - begin);
    put(rt, frame.at(8u), size);
    if (0x0AAAAAAAu - size < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4AFCu});
    const std::uint32_t grow = size < count ? count : size;
    const std::uint32_t total = size + grow;
    std::uint32_t block = 0u;
    std::uint32_t bytes = 0xFFFFFFF0u;
    if (!(total < size)) {
        bytes = 0u;
        if (total != 0u) bytes = total * 24u;
    }
    if (total < size || total != 0u) block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const std::uint32_t front = call_set(rt, ctx, 0x08C4B568u, {word(rt, vector), position, block, frame.at(3u)});
    call_set(rt, ctx, 0x08C4B50Cu, {front, count, value, frame.at(2u)});
    const std::uint32_t back = call_set(rt, ctx, 0x08C4B568u, {position, word(rt, vector + 4u), front + count * 24u, frame.at(1u)});
    call_set(rt, ctx, 0x08C498C4u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_block = word(rt, vector);
    ctx.gpr[2] = block + bytes;
    if (old_block != 0u) call_set(rt, ctx, 0x08B35254u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, back);
    put(rt, vector, block);
}

} // namespace legacy_116

// ---- helpers and drafts taken from src/game/unsorted/hot_batch_97.cpp (registration was commented out there: no comparable case)
namespace legacy_97 {
namespace {
using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (call_indirect's rule).
inline std::uint32_t call_set_v0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = address;
    return call_set(rt, ctx, address, arguments);
}
// A virtual call: the slot at `offset` of the object's vtable (the word at object[0]).
inline std::uint32_t slot_of(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset) { return word(rt, word(rt, object) + offset); }
// The HI of the MIPS signed `div` (what `mfhi` reads): the remainder; a zero divisor leaves the dividend (the recompiler's rule), INT_MIN / -1 leaves 0.
inline std::uint32_t mips_hi(std::uint32_t dividend, std::uint32_t divisor) {
    if (divisor == 0u) return dividend;
    if (dividend == 0x80000000u && divisor == 0xFFFFFFFFu) return 0u;
    return static_cast<std::uint32_t>(signed_of(dividend) % signed_of(divisor));
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}

// The four floats of one row of a table of 16-byte rows (a glyph or digit table): rows = trunc(table[8]), the row is table + ((index - (rows - 1)) << 4) and the floats sit at row + base + 0 / 4 / 8 / 0xC (f12 / f13 / f14 / f15 in that order).
inline std::array<float, 4> table_row(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t table, std::uint32_t index, std::uint32_t base) {
    const std::uint32_t rows = ctx.fpu_float_to_word(float_at(rt, table + 8u), 1u);
    const std::uint32_t row = table + ((index - (rows - 1u)) << 4u) + base;
    return {float_at(rt, row), float_at(rt, row + 4u), float_at(rt, row + 8u), float_at(rt, row + 0xCu)};
}
// The static all-zero-or-default record a digit window uses for a leading zero (0x08CE29F0): its floats sit at +0 / 4 / 8 / 0xC.
inline std::array<float, 4> record_floats(psprecomp::Runtime &rt, std::uint32_t row) {
    return {float_at(rt, row), float_at(rt, row + 4u), float_at(rt, row + 8u), float_at(rt, row + 0xCu)};
}

// func_08B3177C(self): a state check on a record (self[0xC] < 0 returns that value; the rest tail-calls). self[8] != 0: self[8] = 0 and func_08B3027C(self). Otherwise func_08B344E0(self + 0x8FC); when self[5] == 0 and self[0x12A8] < 0 (signed) and func_08B2E05C() gives a 64-bit time (lo = v0, hi = v1) after the stamp (self[0x12B4]:self[0x12B0]) (compared signed on the high word, unsigned on the low), func_08B3457C(self + 0x8FC), func_08B2F70C(self, 8) and the tail call func_08B3027C(self). Else func_08B3457C(self + 0x8FC) and, when the byte self[0x9648] is 0, the tail call func_08B31600(self); otherwise the time func_08B2E05C() minus the stamp (self[0x9644]:self[0x9640]) as an absolute 64-bit value: above 10,000,000 gives func_08B2F70C(self, 8) and the tail call func_08B3027C(self), else the tail call func_08B31600(self).
// Not registered (see its HOT97 line): every path outside the compared cases was read against the asm by hand and by an independent read-only review, none was run.

} // namespace
} // namespace legacy_97

} // namespace new_batch_main_64
} // namespace gt::game

#define NEW64(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_64::name, gt::ReturnKind::kind)
NEW64(0x08B62A04u, func_08B62A04, Word);
NEW64(0x0886D5A0u, destroy_with_subobject, Word);
NEW64(0x0886D88Cu, destroy_with_subobject, Word);
NEW64(0x0889E6E8u, destroy_with_shared_members, Word);
NEW64(0x0889E9F4u, destroy_with_shared_members, Word);
NEW64(0x08BE1AD4u, func_08BE1AD4, Word);
NEW64(0x08BE1DA4u, func_08BE1DA4, Word);
NEW64(0x088FE6BCu, clear_record, Word);
NEW64(0x0886D0CCu, destroy_deleting_subobject, Word);
NEW64(0x08B6D284u, func_08B6D284, DoubleWord);
NEW64(0x08A3B5CCu, func_08A3B5CC, Word);
NEW64(0x08A1AC08u, legacy_113::func_08A1AC08, Word);
NEW64(0x08BAC714u, legacy_113::func_08BAC714, Word);
NEW64(0x08A2C2B4u, legacy_113::func_08A2C2B4, Word);
NEW64(0x08BBF0D0u, legacy_114::func_08BBF0D0, Word);
NEW64(0x08C4BD80u, legacy_116::func_08C4BD80, Word);
