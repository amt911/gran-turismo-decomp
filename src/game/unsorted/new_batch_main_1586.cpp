// New batch main 1586: DRAFT (unverified) structured C from tools.disasm. The gate has not seen these yet.
// Groups: the guarded static member built in place (13 instances of one shape, one helper), 0x088F2678 (the same
// shape with a block member), and seven functions carried from the earlier main-14 draft (the assign-or-insert
// pair, the record range moves, the guarded function-local static 0x08BC48B4, the three method registrations 0x088062D0).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1586 {
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
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }

// --- the guarded static member, built in place -----------------------------------------------------------------
// a0 = the outer object. `base` runs on it first; its vtable word is stored at +0; a `size`-byte block comes from
// func_35E344 and `sub_init` receives it (+`bias`); the block is stored at +`offset` and the member at +`offset`+4.
// The member is the object at `object`, built once: while the guard byte is clear, func_36041C(guard) answers
// non-zero -> the object's vtable is stored and func_36044 (guard) ends the build. v0 is 0x000A0000 (the `lui`
// left in it) unless the build ran, then whatever func_36044 answers.
void guarded_member(Runtime &rt, AllegrexContext &ctx, u32 base, u32 outer_vtable, u32 size, u32 offset, u32 sub_init,
                    u32 bias, u32 guard, u32 object, u32 object_vtable) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, base, {self});
    put(rt, self, outer_vtable);
    const u32 block = call(rt, ctx, 0x08B62344u, {size});
    call(rt, ctx, sub_init, {block + bias});
    put(rt, self + offset, block);
    ctx.gpr[2] = 0x000A0000u;
    if (signed_byte(rt, guard) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            put(rt, object, object_vtable);
            call(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, self + offset + 4u, object);
}

// --- "assign the holder's value, or insert it" (4 instances) ----------------------------------------------
// (a0 = the destination, a1 = a pointer to the holder pointer): a null holder assigns 0; else the object is
// compared against the kind `kind_getter` answers (func_08A76F20) — of that kind, the holder's value is
// assigned; otherwise `insert` fills a frame value object (the holder's pointer), which is assigned when it is
// non-null. v0 = 1 except when nothing was inserted.
void assign_or_insert(Runtime &rt, AllegrexContext &ctx, u32 kind_getter, u32 insert) {
    const u32 out = ctx.gpr[4];
    const u32 holder_ref = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 object = word(rt, holder_ref);
    u32 result = 1u;
    if (object == 0u) {
        call(rt, ctx, 0x08A5BAD8u, {out, 0u});
    } else {
        const u32 kind = call(rt, ctx, kind_getter, {out, 0u});
        if (call(rt, ctx, 0x08A76F20u, {object, kind}) != 0u) {
            call(rt, ctx, 0x08A5BAD8u, {out, word(rt, holder_ref)});
        } else {
            call(rt, ctx, insert, {frame.at(0u), holder_ref, ctx.gpr[6]});
            const u32 value = word(rt, frame.at(0u));
            if (value != 0u) {
                call(rt, ctx, 0x08A5BAD8u, {out, value});
            } else {
                result = 0u;
            }
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
    }
    ctx.gpr[2] = result;
}

// A 16-byte-record range move shared by 0x08B89338 and (8-byte records) 0x08C49B00: the record count is
// (end - source) >> shift, the records are copied to `destination` and `at` advances; the tail from `at` to the
// end pointer is handed to `builder` with a frame scratch value; the end pointer shrinks by (source -
// destination) rounded down to the record size, and v0 = the original destination. A zero or negative count
// skips the copy but runs the tail.
void move_record_range(Runtime &rt, AllegrexContext &ctx, u32 builder, u32 shift, u32 record_bytes,
                       u32 round_mask) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const s32 count = static_cast<s32>(word(rt, self + 4u) - source) >> shift;
    u32 at = destination;
    u32 from = source;
    if (count > 0) {
        for (s32 i = 0; i < count; ++i) {
            for (u32 w = 0; w < record_bytes; w += 4u) put(rt, at + w, word(rt, from + w));
            at += record_bytes;
            from += record_bytes;
        }
    }
    call(rt, ctx, builder, {at, word(rt, self + 4u), frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((source - destination) & round_mask));
    ctx.gpr[2] = destination;
}

void func_08A730D8(Runtime &rt, AllegrexContext &ctx) { assign_or_insert(rt, ctx, 0x08A72CC0u, 0x08AA257Cu); }

void func_08A87C28(Runtime &rt, AllegrexContext &ctx) { assign_or_insert(rt, ctx, 0x08A87690u, 0x08AA22BCu); }

void func_08AB173C(Runtime &rt, AllegrexContext &ctx) { assign_or_insert(rt, ctx, 0x08AB1304u, 0x08AA25F0u); }

void func_08B89338(Runtime &rt, AllegrexContext &ctx) { move_record_range(rt, ctx, 0x08B89328u, 4u, 16u, 0xFFFFFFF0u); }

void func_08C49B00(Runtime &rt, AllegrexContext &ctx) { move_record_range(rt, ctx, 0x08C470F4u, 3u, 8u, 0xFFFFFFF8u); }

// 0x08BC48B4: the function-local static at 0x08D765D4 with its guard byte at 0x08D76190: when the byte is set
// the address answers at once; otherwise func_08B6441C(0x08D76190) must answer non-zero, the block's vtable
// (0x08CC4FB0), the two vtables (0x08C8A47C at +8, 0) and its counted words are stored, func_08B64494 ends the
// construction and v0 = 0x08D765D4 on every path.
void func_08BC48B4(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kStatic = 0x08D765D4u;
    GuestFrame frame(ctx, 0x10u);
    if (signed_byte(rt, 0x08D76190u) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D76190u}) != 0u) {
            put(rt, kStatic + 8u, 0x08C8A47Cu);
            put(rt, kStatic + 4u, 0u);
            put(rt, kStatic + 0xCu, 0u);
            put(rt, kStatic + 0x14u, 0u);
            put(rt, kStatic + 0x24u, 0u);
            put(rt, kStatic + 0x10u, 0u);
            put(rt, kStatic + 0x20u, 0u);
            put(rt, kStatic + 0x1Cu, 0u);
            put(rt, kStatic + 0x2Cu, 0u);
            put(rt, kStatic, 0x08CC4FB0u);
            put(rt, kStatic + 0x30u, 0u);
            call(rt, ctx, 0x08B64494u, {0x08D76190u});
        }
    }
    ctx.gpr[2] = kStatic;
}

// --- 180-byte group ---------------------------------------------------------------------------------------
// 0x088062D0 (a0 = a pointer to the object pointer): three method registrations func_08B87108(holder, name,
// handler) with the holder re-read (and reported through func_08A5BB7C(0x08C97DCC) when null) before each; the
// third is a tail call.
void func_088062D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        object = word(rt, self);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97DCCu});
        call(rt, ctx, 0x08B87108u, {object, 0x08C97E24u, 0x08806FA0u});
        object = word(rt, self);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97DCCu});
        call(rt, ctx, 0x08B87108u, {object, 0x08C97E30u, 0x0880714Cu});
        object = word(rt, self);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C97DCCu});
    }
    tail_call(ctx, 0x08B87108u, {object, 0x08C97E3Cu, 0x0880602Cu});
}



// 0x088F2678 (a0 = the outer object): vtable 0x08CC3B48 at +0, +4 cleared, a 0x9D0-byte block from func_35E344 built
// by func_088F1BB8 and stored at +8; the member at +0xC is the guarded object 0x08D76010 (guard 0x08D76008, its
// vtable 0x08CC3B78). v0 = 0x000A0000 unless the build ran.

} // namespace
} // namespace new_batch_main_1586
} // namespace gt::game

#define MAIN1586(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1586::name, gt::ReturnKind::kind)
MAIN1586(0x08A730D8u, func_08A730D8, Word);
MAIN1586(0x08A87C28u, func_08A87C28, Word);
MAIN1586(0x08AB173Cu, func_08AB173C, Word);
MAIN1586(0x08B89338u, func_08B89338, Word);
MAIN1586(0x08BC48B4u, func_08BC48B4, Word);
MAIN1586(0x08C49B00u, func_08C49B00, Word);
MAIN1586(0x088062D0u, func_088062D0, Word);
