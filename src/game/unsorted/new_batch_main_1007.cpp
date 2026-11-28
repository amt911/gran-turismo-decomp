// New batch, main-session 1007. Functions drafted from `tools.disasm` with the gate's old mismatches in hand
// (/home/andres/repos/gt-ops/drafts/fix_info.txt): structured C, named locals, G[n] only where a call needs the register.
// Every call passes the argument registers the original sets before the jal; branch-likely delays are annulled when the
// branch is not taken; v0 is set as the original leaves it on every exit path.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1007 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

struct CallResult {
    u32 v0;
    u32 v1;
    float f0;
};
// A guest call with the argument registers set as the original sets them before the jal (a0-a3, t0-t3 and f12-f15 in
// order): v0 / v1 / f0 of the callee stay in the context, as the instruction after the jal sees them.
CallResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                     std::initializer_list<float> float_arguments = {}) {
    u32 reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    u32 freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return {result.v0, result.v1, result.f0};
}
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_setf(rt, ctx, target, arguments).v0;
}

// The s registers (16-23) and the callee-saved f registers (20-31) a function restores in its epilogue.
struct SavedS {
    AllegrexContext &ctx;
    u32 saved[8];
    explicit SavedS(AllegrexContext &c) : ctx(c) {
        for (u32 index = 0u; index < 8u; ++index) saved[index] = c.gpr[16u + index];
    }
    ~SavedS() {
        for (u32 index = 0u; index < 8u; ++index) ctx.gpr[16u + index] = saved[index];
    }
};
struct SavedF {
    AllegrexContext &ctx;
    float saved[12];
    explicit SavedF(AllegrexContext &c) : ctx(c) {
        for (u32 index = 0u; index < 12u; ++index) saved[index] = c.fpr[20u + index];
    }
    ~SavedF() {
        for (u32 index = 0u; index < 12u; ++index) ctx.fpr[20u + index] = saved[index];
    }
};

// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// -------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08B19A8C (a0 = 1, a1 = 0xFFFF: the pair the original runs on; any other pair answers 1 / 0xFFFF at once): the static
// registrations. Two global lists at 0x08D688EC and 0x08D688FC are cleared (words +0, +4, +8), then for each of the
// thirteen factories the object it answers gets its vtable word at +8 and zeros at +0x30 and +0x2C, the factory runs a
// second time, and the second object goes to 0x08B18DFC (the list's add). The three objects after them are built from a
// creator pair (0x08C4CC3C / 0x08C4CCB4 and a child from 0x08B9BF48), given their vtable / member, registered the same
// way, and the last one is registered by the tail call. v0 = that tail call's answer.
void func_08B19A8C(Runtime &rt, AllegrexContext &ctx) {
    if (ctx.gpr[4] != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (ctx.gpr[5] != 0xFFFFu) {
        ctx.gpr[2] = 0xFFFFu;
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    const u32 table_a = 0x08D688ECu;
    const u32 table_b = 0x08D688FCu;
    for (const u32 address : {table_a, table_a + 4u, table_a + 8u, table_b, table_b + 4u, table_b + 8u}) {
        put(rt, address, 0u);
    }
    u32 a0 = table_a;
    // One registration pair: the factory's object gets `vtable` at +8 and +0x30 / +0x2C cleared (the +0x2C clear sits in
    // the second call's delay slot), then the second object goes through 0x08B18DFC.
    const auto register_pair = [&](u32 factory, u32 vtable) {
        const u32 object = call_set(rt, ctx, factory, {a0});
        put(rt, object + 8u, vtable);
        put(rt, object + 0x30u, 0u);
        put(rt, object + 0x2Cu, 0u);
        a0 = call_set(rt, ctx, factory, {a0});
        call_set(rt, ctx, 0x08B18DFCu, {a0});
    };
    register_pair(0x08BC48B4u, 0x08CB5294u);
    register_pair(0x08B9CEBCu, 0x08CB52A4u);
    register_pair(0x08B9DDB0u, 0x08CB52ACu);
    register_pair(0x08C4D18Cu, 0x08CB52B4u);
    register_pair(0x08BC4A2Cu, 0x08CB52BCu);
    register_pair(0x08BC473Cu, 0x08CB52C4u);
    register_pair(0x08BC45C4u, 0x08CB52CCu);
    register_pair(0x08BC42D4u, 0x08CB52D4u);
    register_pair(0x08BC415Cu, 0x08CB52DCu);
    register_pair(0x08BC48B4u, 0x08CB52E4u);
    register_pair(0x08BC444Cu, 0x08CB52ECu);
    register_pair(0x08B9E8D4u, 0x08CB52F8u);
    register_pair(0x08BC3FE4u, 0x08CB5304u);
    // The first created object: child from 0x08B9BF48 at +8, vtable 0x08CB5310 at +4.
    {
        const u32 object = call_set(rt, ctx, 0x08C4CC3Cu, {a0});
        const u32 child = call_set(rt, ctx, 0x08B9BF48u, {a0});
        put(rt, object + 8u, child);
        put(rt, object + 4u, 0x08CB5310u);
        a0 = call_set(rt, ctx, 0x08BC4BA4u, {a0});
    }
    // Its member: creator again, vtable 0x08CB5314 at +8, the creator's answer at +0x2C, +0x30 cleared.
    {
        const u32 creator = call_set(rt, ctx, 0x08C4CC3Cu, {a0});
        put(rt, a0 + 8u, 0x08CB5314u);
        put(rt, a0 + 0x2Cu, creator);
        put(rt, a0 + 0x30u, 0u);
        a0 = call_set(rt, ctx, 0x08BC4BA4u, {a0});
        call_set(rt, ctx, 0x08B18DFCu, {a0});
    }
    {
        const u32 object = call_set(rt, ctx, 0x08C4CCB4u, {a0});
        const u32 child = call_set(rt, ctx, 0x08B9BF48u, {a0});
        put(rt, object + 8u, child);
        put(rt, object + 4u, 0x08CB531Cu);
        a0 = call_set(rt, ctx, 0x08BC4D2Cu, {a0});
    }
    {
        const u32 creator = call_set(rt, ctx, 0x08C4CCB4u, {a0});
        put(rt, a0 + 8u, 0x08CB5324u);
        put(rt, a0 + 0x2Cu, creator);
        put(rt, a0 + 0x30u, 0u);
        a0 = call_set(rt, ctx, 0x08BC4D2Cu, {a0});
        call_set(rt, ctx, 0x08B18DFCu, {a0});
    }
    {
        const u32 object = call_set(rt, ctx, 0x08BC4EB4u, {a0});
        const u32 creator = call_set(rt, ctx, 0x08C4CCB4u, {a0});
        put(rt, object + 8u, 0x08CB5340u);
        put(rt, object + 0x2Cu, creator);
        put(rt, object + 0x30u, 0u);
        a0 = call_set(rt, ctx, 0x08BC4EB4u, {a0});
    }
    tail_call(ctx, 0x08B18DFCu, {a0});
}

// -------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0893CBCC (a0 = the object): when the byte at +0x69C of the object's word +8 is set and 0x089557BC answers 0, the
// update runs: 0x089547A0, 0x0893CA3C with the float at +0x6A8, 0x0893C870 with the byte shifted left by 4 whose low
// nibble decides whether the four per-slot differences run (each slot reads +0x364 of its 116-byte record minus +0xC8 of
// 0x08955300's answer, clamped at 0); the two clamped pairs feed a per-axis low/high accumulator at +0x6FC / +0x700 and
// the total at +0x18; 0x0893C3E4 then runs on the six records, three weighted terms at +0x4C / +0x58 against +0x6CC fold
// into the maximum at +0x6D4. v0 = the object's word +8 + 0xC.

// -------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088679D4 (a0 = the object, a1 = an input vector, a2 = a byte string, f12 / f13 / f14 = three floats): f13 <= 0 is a
// no-op. The vector (its y raised by 1.5) goes through two 0x08B4A774 transforms (the object's +0x4390 and +0x4290), a
// negative result is a no-op, and the rest arms the graphics library (0x08AFBEF4, 0x08B54FB8, 0x08B58874, 0x08B584B4,
// 0x08B58AD0) and fills the global object 0x08CF68E8 (+0x0, +0x4, +0x8, +0xC, +0x20, +0x24, +0x48..+0x60, +0x58 flag,
// 0x08A4CE38 / 0x08A4CE60 / 0x08A4CEB4 with the five float arguments). The byte string is then copied into a stack buffer
// while 0.7 per 0x40 byte and 1.0 per 0xC0 byte accumulate; past 10.7 the four bytes -30, -128, -90, 0 are written and
// 0x08A4DB50 runs on the global object and the buffer.

// -------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08BF5120 (a0 = the container (begin, end, capacity at +0, +4, +8), a1 = the position, a2 = the element source): a
// push_back of a 0x28-byte element. With room, the last element is copied to `end` (vtable 0x08CCE6E0 and the ten words),
// the elements from the position through the old end slide one slot forward (vtable kept), the source element is written at
// the position and 0x089D6684(that element) runs; v0 = its answer. Without room the container grows like a vector
// (0x08A5BC20(size, 4), 0x08BE6BD4 constructing at the position, 0x08BE6BD4 again at the end, 0x08BE593C, and
// 0x08A5BC54 releasing the old buffer), v0 = the new capacity address.
void func_08BF5120(Runtime &rt, AllegrexContext &ctx) {
    SavedS saved_s(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 container = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 base = frame.at(0u);
    constexpr u32 kElement = 0x28u;
    constexpr u32 kVtable = 0x08CCE6E0u;
    const auto copy_field = [&rt](u32 to, u32 from) { put(rt, to, word(rt, from)); };
    u32 end = word(rt, container + 4u);
    if (end == word(rt, container + 8u)) {
        const u32 begin = word(rt, container);
        const u32 scaled = static_cast<u32>(static_cast<s32>(end - begin) >> 3) * 0xCCCCCCCDu;
        constexpr u32 kMaximum = (0x666u << 16) + 0x6666u;
        if (scaled == kMaximum) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA4B44u});
        u32 elements = 1u;
        u32 capacity = 0xFFFFFFF0u;
        if (scaled == 0u) {
            capacity = (1u << 3) + (1u << 5);
        } else {
            elements = scaled << 1;
            if (elements >= scaled) capacity = (elements << 3) + (elements << 5);
        }
        const u32 buffer = call_set(rt, ctx, 0x08A5BC20u, {capacity, 4u});
        const u32 created = call_set(rt, ctx, 0x08BE6BD4u, {begin, position, buffer, base + 2u});
        if (created != 0u) {
            put(rt, created + 0u, kVtable);
            copy_field(created + 4u, source + 4u);
            copy_field(created + 8u, source + 8u);
            for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) copy_field(created + offset, source + offset);
            copy_field(created + 0x20u, source + 0x20u);
            copy_field(created + 0x24u, source + 0x24u);
        }
        const u32 tail = call_set(rt, ctx, 0x08BE6BD4u, {position, word(rt, container + 4u), created + kElement, base + 1u});
        call_set(rt, ctx, 0x08BE593Cu, {word(rt, container), word(rt, container + 4u), base});
        const u32 old_begin = word(rt, container);
        const u32 bytes = static_cast<u32>(static_cast<s32>(word(rt, container + 8u) - old_begin) >> 3) << 3;
        if (old_begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {old_begin, bytes, 4u});
        put(rt, container + 8u, buffer + capacity);
        put(rt, container + 4u, tail);
        put(rt, container + 0u, buffer);
        ctx.gpr[2] = buffer + capacity;
        return;
    }
    u32 last = end - kElement;
    if (end != 0u) {
        put(rt, end + 0u, kVtable);
        copy_field(end + 4u, last + 4u);
        copy_field(end + 8u, last + 8u);
        for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) copy_field(end + offset, last + offset);
        copy_field(end + 0x24u, last + 0x24u);
        copy_field(end + 0x20u, last + 0x20u);
        end = word(rt, container + 4u);
        last = end - kElement;
    }
    const u32 count = static_cast<u32>(static_cast<s32>(last - position) >> 3) * 0xCCCCCCCDu;
    const u32 old_end = end;
    put(rt, container + 4u, end + kElement);
    // The source element's ten words on the frame at +4 (the vtable at +4, the rest shifted by 4).
    put(rt, base + 4u, kVtable);
    copy_field(base + 8u, source + 4u);
    copy_field(base + 0xCu, source + 8u);
    for (u32 index = 0u; index < 5u; ++index) copy_field(base + 0x10u + 4u * index, source + 0xCu + 4u * index);
    copy_field(base + 0x24u, source + 0x20u);
    copy_field(base + 0x28u, source + 0x24u);
    if (static_cast<s32>(count) > 0) {
        u32 destination = old_end;
        u32 carried = old_end - 0x30u;
        u32 trailing = old_end - 8u;
        u32 shifting = last;
        u32 index = 0u;
        for (;;) {
            shifting -= kElement;
            const u32 word_4 = word(rt, shifting + 4u);
            const u32 word_8 = word(rt, shifting + 8u);
            u32 fields[5];
            for (u32 field = 0u; field < 5u; ++field) fields[field] = word(rt, shifting + 0xCu + 4u * field);
            destination -= kElement;
            put(rt, destination + 4u, word_4);
            put(rt, destination + 8u, word_8);
            for (u32 field = 0u; field < 5u; ++field) put(rt, destination + 0xCu + 4u * field, fields[field]);
            put(rt, trailing, word(rt, carried));
            put(rt, trailing + 4u, word(rt, carried + 4u));
            carried -= kElement;
            index += 1u;
            trailing -= kElement;
            if (index == count) break;
        }
    }
    // The source element written at the position (its vtable stays) and 0x089D6684 on the frame copy.
    copy_field(position + 4u, base + 8u);
    copy_field(position + 0xCu, base + 0x10u);
    copy_field(position + 0x10u, base + 0x14u);
    copy_field(position + 0x14u, base + 0x18u);
    copy_field(position + 0x18u, base + 0x1Cu);
    copy_field(position + 8u, base + 0xCu);
    copy_field(position + 0x1Cu, base + 0x20u);
    copy_field(position + 0x24u, base + 0x28u);
    copy_field(position + 0x20u, base + 0x24u);
    call_set(rt, ctx, 0x089D6684u, {base + 4u});
}

// -------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0887B40C (a0 = the container, a1 / a2 = two numbers): a counted range insert of 0x10-byte records into a tree-like
// list. 0x08B9AB4C / 0x08B9A958 / 0x08B8ED98 / 0x08B8FE44 set up the record area, 0x089626C8 answers the pair the record
// holds, 0x08B904D8 builds it, 0x08962AC0 inserts it, and the same sequence runs a second time for the trailing pair.
// Each inserted pair is a counted pointer (the count at +4, taken with ll/sc) and the pairs shadowed at +0x4C / +0x54
// release their old pointer, its two virtual destructors running when the count reaches zero. v0 = the last call's answer.

// -------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08839A44 (a0 = the object): the object's seven released members. 0x08838FBC fills a holder at sp + 0x20 (its word is
// asserted non-zero by 0x08A5BB7C) whose 0x08838DC4 answer is the shared record; 0x08A5B9EC releases the holder and
// 0x08A98C1C builds the two iterators at sp + 0x24 and sp + 0x28. Each of the seven members (at +0x14, its +4, +8 and
// +0xC of the word at sp + 0x28 or +0x24) is then released through 0x08A5BAD8 when it is not the iterator itself, and the
// iterator is closed by 0x08A5B9EC (the ones at +0x28 rebuilt by 0x08A98C1C with 2). The object itself goes last.

// -------------------------------------------------------------------------------------------------------------------------------------------------
// 0x089EB244 (a0 = the object, a1 = a word): the list update under the global lock 0x08CF575C. The words at +0x19C and
// +0x1A0 are reconciled (+0x1A0 takes +0x19C when they differ, otherwise the second becomes -1); a negative +0x19C ends
// it. Then the key named 0x08CF6ED8 is built at sp + 0x20 (0x08A983D8), and when 0x089E7700 / 0x089E7520 /
// 0x089E7340 accept the object, the two temporary iterators at sp + 0x24 / +0x28 are filled (0x08A76AA0, 0x08A76AAC)
// and the object's +0x188 virtual +0xCC runs with (2, the iterators); the second block (when +0x18C is set and the two
// words differ) repeats it with three iterators and code 3. Both blocks release their temporaries in reverse order; the
// last 0x08A5B9EC on sp + 0x20 answers, or 0x08B3457C's answer on the negative path.
void func_089EB244(Runtime &rt, AllegrexContext &ctx) {
    SavedS saved_s(ctx);
    GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 base = frame.at(0u);
    const u32 key = base + 0x20u;
    const u32 temporary_a = base + 0x24u;
    const u32 temporary_b = base + 0x28u;
    call_set(rt, ctx, 0x08B344E0u, {0x08CF575Cu});
    const u32 first = word(rt, self + 0x19Cu);
    u32 second = word(rt, self + 0x1A0u);
    if (first != second) {
        put(rt, self + 0x1A0u, first);
    } else {
        second = 0xFFFFFFFFu;
    }
    call_set(rt, ctx, 0x08B3457Cu, {0x08CF575Cu});
    if (static_cast<s32>(first) < 0) return;
    call_set(rt, ctx, 0x08A983D8u, {key, 0x08CF6ED8u});
    if (call_set(rt, ctx, 0x089E7700u, {self, first}) == 0u) {
        call_set(rt, ctx, 0x089E75ACu, {self, first, 1u});
        call_set(rt, ctx, 0x089E7020u, {self, first, 0u});
        if (call_set(rt, ctx, 0x089E7520u, {self, first}) != 0u &&
            call_set(rt, ctx, 0x089E7340u, {self, first}) != 0u) {
            call_set(rt, ctx, 0x08A98C1Cu, {base + 0x1Cu, 1u});
            call_set(rt, ctx, 0x08A5BAD8u, {key, word(rt, base + 0x1Cu)});
            call_set(rt, ctx, 0x08A5B9ECu, {base + 0x1Cu});
            const u32 member = word(rt, word(rt, key) + 0x14u);
            call_set(rt, ctx, 0x08A85B1Cu, {base + 0x18u, first});
            if (member != base + 0x18u) call_set(rt, ctx, 0x08A5BAD8u, {member, word(rt, base + 0x18u)});
            call_set(rt, ctx, 0x08A5B9ECu, {base + 0x18u});
        }
    }
    if (word(rt, self + 0x188u) != 0u && word(rt, key) != 0u) {
        for (u32 slot = base + 0x28u; slot < base + 0x30u; slot += 4u) call_set(rt, ctx, 0x08A76AA0u, {slot});
        put(rt, base + 0x10u, argument);
        call_set(rt, ctx, 0x08A76AACu, {base + 0x14u, base + 0x10u});
        if (temporary_b != base + 0x14u) call_set(rt, ctx, 0x08A5BAD8u, {temporary_b, word(rt, base + 0x14u)});
        call_set(rt, ctx, 0x08A5B9ECu, {base + 0x14u});
        if (base + 0x2Cu != key) call_set(rt, ctx, 0x08A5BAD8u, {base + 0x2Cu, word(rt, key)});
        call_set(rt, ctx, 0x08A76AA0u, {temporary_a});
        const u32 object = word(rt, self + 0x188u);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA78D8u});
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCCu), {object, temporary_a, 2u, temporary_b});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary_a});
        for (u32 slot = base + 0x2Cu; slot >= base + 0x28u; slot -= 4u) call_set(rt, ctx, 0x08A5B9ECu, {slot});
    }
    if (word(rt, self + 0x18Cu) != 0u && second != first) {
        for (u32 slot = base + 0x28u; slot < base + 0x34u; slot += 4u) call_set(rt, ctx, 0x08A76AA0u, {slot});
        put(rt, base + 8u, argument);
        call_set(rt, ctx, 0x08A76AACu, {base + 0xCu, base + 8u});
        if (temporary_b != base + 0xCu) call_set(rt, ctx, 0x08A5BAD8u, {temporary_b, word(rt, base + 0xCu)});
        call_set(rt, ctx, 0x08A5B9ECu, {base + 0xCu});
        call_set(rt, ctx, 0x08A85B1Cu, {base + 4u, first});
        if (base + 0x2Cu != base + 4u) call_set(rt, ctx, 0x08A5BAD8u, {base + 0x2Cu, word(rt, base + 4u)});
        call_set(rt, ctx, 0x08A5B9ECu, {base + 4u});
        call_set(rt, ctx, 0x08A85B1Cu, {base, second});
        if (base + 0x30u != base) call_set(rt, ctx, 0x08A5BAD8u, {base + 0x30u, word(rt, base)});
        call_set(rt, ctx, 0x08A5B9ECu, {base});
        call_set(rt, ctx, 0x08A76AA0u, {base + 0x24u});
        const u32 object = word(rt, self + 0x18Cu);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA78D8u});
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCCu), {object, base + 0x24u, 3u, temporary_b});
        call_set(rt, ctx, 0x08A5B9ECu, {base + 0x24u});
        for (u32 slot = base + 0x30u; slot >= base + 0x28u; slot -= 4u) call_set(rt, ctx, 0x08A5B9ECu, {slot});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {key});
}

} // namespace
} // namespace new_batch_main_1007
} // namespace gt::game

#define NEW1007(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1007::name, gt::ReturnKind::kind)
NEW1007(0x08B19A8Cu, func_08B19A8C, Word);
NEW1007(0x08BF5120u, func_08BF5120, Word);
NEW1007(0x089EB244u, func_089EB244, Word);
