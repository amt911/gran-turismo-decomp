// New batch, main session 17: functions nothing had taken (the recompiler's output, none decompiled before), written as structured C
// by hand from `tools.disasm`; verified by the differential gate afterwards. No names: none has evidence yet.
// Grouped by shape where the asm repeats: one wrapper, one row per site.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_17 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same, keeping v1 (a caller that passes the callee's v1 on reads it after the jal) and f0.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A guest call with float arguments ($f12-$f15 in order) as well.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                                   std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A `lw v0, slot(vtable); jalr v0` call: keeps v1 and f0 as well (a caller reads the callee's f0).
GuestResult call_indirect_result(Runtime &rt, AllegrexContext &ctx, u32 target,
                                            std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// `sra` (arithmetic) as the asm spells it; >> on an unsigned local would be a logical shift.
u32 sra(u32 value, unsigned bits) { return static_cast<u32>(static_cast<s32>(value) >> bits); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// --- group 1 ---

// 0x08C3A1A4 (a0 = object, a1 = output object, a2 = pointer to a base, a3 unused): two indexed lookups through the
// size/pointer pairs at a0+0x14/+0x18 and a0+0x1C/+0x20 (an odd size selects a word from a table of function-ish
// entries), then a third call that builds a temporary at sp; the temporary is assigned to a1 when a1 is not sp and is
// destroyed. v0 = the destroy call's result.

// 0x08C4B43C (a0 = container with an end pointer at +4, a1 = destination, a2 = source): copies the whole range
// [a2, word(a0+4)) in 0x18-byte steps, appends it through 0x08C4B42C and shrinks the container's size by the copied
// byte count with the low three bits cleared. v0 = a1.

// 0x0880E024 (a0 = range object whose first word is an element array, a1, a2 = call arguments, a3 = index,
// f12/f13 = floats): picks element a3 - (int)trunc(float at head+8) + 1 of the 16-byte array, then forwards two
// scaled float pairs of it. v0 = the word at a0.

// The shared body of 0x08828024 / 0x088280F8 / 0x088281CC / 0x088282A0: a temporary at sp+4 gets a value from
// 0x088248E4 (its +0x14 through the site's own accessor), it is destroyed, a second temporary at sp+8 comes from
// 0x08824774, then 0x08A52AB8 builds the result at sp, which is assigned to a0 when a0 is not sp; both temporaries are
// destroyed. v0 = the last destroy's result.
void build_value_from_temporaries(Runtime &rt, AllegrexContext &ctx, u32 accessor) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x088248E4u, {frame.at(4u)});
    u32 first = word(rt, frame.at(4u));
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    first = word(rt, first + 0x14u);
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const u32 value = call(rt, ctx, accessor, {first});
    call(rt, ctx, 0x08824774u, {frame.at(8u)});
    const u32 second = word(rt, frame.at(8u));
    if (second == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A1A8u});
    call(rt, ctx, 0x08A52AB8u, {frame.at(0u), second, value, 0xFFFFFFFFu, 0u, 0u});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
}


// 0x0883B600 (a0, a1 = a count, a2 = pointer to an object): with a positive count and an object, the object's virtual
// +0xC runs into a temporary at sp+4, which 0x08B855E0 turns into the value at sp+8; a static head at 0x08D74FD0 means
// two shared temporaries need their reference counts dropped (destructor-shaped). v0 = the last call's.

// --- group 2 ---

// 0x0886A464 (a0 = owner whose +4 is a ring buffer base, a1 = data, a2 = a kind byte, a3 = a value word): kind 0 hands
// the object at base+8 to 0x088750AC, otherwise it appends an entry to the ring (index at table+0x4200, next at
// +0x4202, 132-byte slots at base+0xC), which 0x08B7C830 fills, and releases the lock 0x08B344E0 took.

// 0x0886BB40 (a0 = owner, a1 = source of two words): the counted pointer at a0+0xF4 takes the source's pointer at +4
// (its count incremented) while the word at a1 is stored at a0+0xF0; the old pointer's count is dropped and its
// virtual destructors at vtable+8 / vtable+0xC run when their counts reach zero. v0 follows the asm: the owner on the
// nothing-to-do path, 1 after the first count, and the last virtual call's result otherwise.

// 0x088725E4 (a0 = object with a packet pointer at +0x5C, a1 = a flag byte, f12/f13/f14 floats): fills the 0x24-byte
// packet at *(+0x5C)+0x24 with the float range f12..f14 and its flag word, then passes the packet to 0x08872548.
void func_088725E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    const float low = ctx.fpr[12];
    const float high = ctx.fpr[13];
    const float value = ctx.fpr[14];
    const float one = 1.0f;
    if (flag != 0u) {
        const bool ordered = low <= high;
        const u32 packet = word(rt, self + 0x5Cu) + 0x24u;
        const u32 flags = word(rt, packet + 0x20u) & ~3u;
        put(rt, packet + 8u, 0u);
        put(rt, packet + 0xCu, 0u);
        const u32 x = word(rt, packet + 8u);
        const u32 y = word(rt, packet + 0xCu);
        put(rt, packet + 0x20u, flags);
        put(rt, packet + 0x10u, x);
        put(rt, packet + 0x14u, y);
        put(rt, packet + 0x18u, x);
        put(rt, packet + 0x1Cu, y);
        put_float(rt, word(rt, self + 0x5Cu) + 0x28u, value);
        const u32 span = word(rt, self + 0x5Cu) + 0x24u;
        const float previous = float_at(rt, span + 0x18u);
        put(rt, span + 0xCu, 0u);
        put_float(rt, span + 8u, previous);
        put_float(rt, span + 0x1Cu, high);
        put_float(rt, span + 0x10u, one);
        put_float(rt, span + 0x14u, low);
        if (ordered) put_float(rt, span + 0x18u, one);
        put(rt, span + 0x20u, word(rt, span + 0x20u) | 3u);
    } else {
        const u32 packet = word(rt, self + 0x5Cu) + 0x24u;
        const u32 flags = word(rt, packet + 0x20u) & ~3u;
        put_float(rt, packet + 8u, one);
        put(rt, packet + 0xCu, 0u);
        const u32 x = word(rt, packet + 8u);
        const u32 y = word(rt, packet + 0xCu);
        put(rt, packet + 0x20u, flags);
        put(rt, packet + 0x10u, x);
        put(rt, packet + 0x14u, y);
        put(rt, packet + 0x18u, x);
        put(rt, packet + 0x1Cu, y);
    }
    tail_call(ctx, 0x08872548u, {word(rt, self + 0x5Cu), self + 0xCu});
}

// 0x0888B88C (a0 = object, a1 = index): when the object's +0xC has flag bit 1 and 0x088DB2C8 accepts the object's +4,
// the object's virtual +0x70 runs (the index goes to a halfword at sp, the byte at +0x618 is cleared); otherwise the
// 432-byte record at (*+0x34) + index*432 + 0x1560 gets its count bumped and 0x088936A4 is asked to build the result.

// 0x088BF978 (a0 = an index byte, a1 = a flag byte, a2 = state): the object at a2+0x1C runs its virtual +0x24; a -1
// answer is a failure (v0 = 0). With no flag, the state's word at +0x24 gets the tool byte at +0x10 and a one/zero
// blink at +0x20, and 0x088BF82C runs; with a flag only 0x088BF94C runs. v0 = 1 when the virtual call answered.

// 0x089F9234 / 0x089F9308 (a0 = object, a1 = source): a temporary at sp+4 gets 0x089FA09C, a stack record at sp+0x14
// (vtable 0x08CCFCB0, a static head, -1, the temporary) goes to the source's virtual +8, the record's head is released
// through 0x08B60804 / 0x08B8542C, then the object takes the temporary (site-specific call) and it is destroyed.


// 0x08A22090 (a0, a1, a2 = a count, a3 = pointer to two objects): a temporary at sp comes from 0x08A22050 and is
// destroyed; with a count of 2 or more, the two objects' virtuals +0x54 (a float) and +0x30 run and 0x08AB87C0 gets the
// temporary's +0x14 with them.

// --- group 3 ---

// The shared body of 0x08A5E338 / 0x08A5E40C / 0x08A835B0 / 0x08A83684 / 0x08A8B410 (a0 = object, a1 = output, a3 =
// pointer to the list object): the list object's virtual at `virtual_offset` runs twice (a2 = the second call's v0 and a3
// its v1 go on), the middle call combines the object's +0x18/+0x1C with it, `last` builds the result at sp from the
// middle call's v0/v1, the result is assigned to a1 when a1 is not sp and destroyed.
void build_from_two_virtuals(Runtime &rt, AllegrexContext &ctx, u32 assert_string, u32 virtual_offset, u32 middle,
                             u32 last) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    u32 object = word(rt, source);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_string});
    call_indirect_result(rt, ctx, word(rt, word(rt, object) + virtual_offset), {object});
    object = word(rt, source);
    const u32 first = word(rt, self + 0x18u);
    const u32 second = word(rt, self + 0x1Cu);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_string});
    const GuestResult inner = call_indirect_result(rt, ctx, word(rt, word(rt, object) + virtual_offset), {object});
    const GuestResult middle_result = call_result(rt, ctx, middle, {first, second, inner.v0, inner.v1});
    call_result(rt, ctx, last, {frame.at(0u), second, middle_result.v0, middle_result.v1});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}


// 0x08A641F4 (a0 = object, a1 = pointer to the source object, a2, a3): 0x08A615D4 / 0x08A61BC0 set the object up from
// sp, the source's virtual +0xD0 runs with the temporary, and 0x08A63F1C runs; when the object's +0x50 is 1 the two
// teardown calls follow. v0 = the temporary's destroy.

// 0x08A78A4C (a0 = object, a1 = pointer to a source): the source's virtual +0x10 goes through 0x08B85EB8 at sp+4,
// 0x08A6D2D4 builds the result at sp+8 from it, the result is assigned to a0 when a0 is not sp+8, and the temporary at
// sp+4 drops its count.

// 0x08AB6E80 (a0, a1, a2 = pointer to a source): the source's virtual +0xC fills sp+4, 0x08AB6A50 turns it into the
// result at sp+8, which is assigned to a0 when a0 is not sp+8; both temporaries drop their counts. v0 = the last
// destroy's.

// 0x08B0A458 (a0 = object, a1 = a kind, 0 = take the default kind from 0x08CE0A24): under the 0x08D473E8 lock, kind 5
// reuses the object at +0x110 when its virtual +0x18 answers 5, otherwise (and only for kind 5) a 0x02000CE0-byte object
// is made by 0x08B62344 / 0x08B451A0 and stored at +0x110. v0 = that object, or 0.
void func_08B0A458(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 kind = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B344E0u, {0x08D473E8u});
    if (kind == 0u) kind = word(rt, 0x08CE0A24u);
    u32 result = 0u;
    if (kind == 5u) {
        const u32 object = word(rt, word(rt, self) + 0x110u);
        if (object != 0u) {
            if (call_indirect_result(rt, ctx, word(rt, word(rt, object) + 0x18u), {object}).v0 == kind) result = object;
        } else {
            const u32 owner = word(rt, self);
            // The asm builds the size in two steps: `lui $a0, 0x2` and `ori $a0, $a0, 0xCE0`.
            const u32 created = call(rt, ctx, 0x08B62344u, {(0x2u << 16) | 0xCE0u});
            call(rt, ctx, 0x08B451A0u, {created});
            put(rt, owner + 0x110u, created);
            result = word(rt, word(rt, self) + 0x110u);
        }
    }
    call(rt, ctx, 0x08B3457Cu, {0x08D473E8u});
    ctx.gpr[2] = result;
}

// 0x08B0E6B0 (a0 = object): under the 0x08B34888 lock, +0xEC/+0x104/+0x108 are cleared and the list at +0xFC is
// walked (each node's object runs its virtual +8, +0x104 counts the nodes); an empty list runs the object's own virtual
// +0x24 with the byte at +0x68 cleared instead. When that byte was set the work goes on in 0x08B0DEB0.

// 0x08B327AC (a0 = object, a1 = a kind): a repeat of the kind at 0x08D69168 answers 1 at once; otherwise 0x08B324CC
// runs and kind 0 takes 0x08B2DBF0 with the word at 0x08CE0C80, kind 1 / 2 ask 0x08B322CC / 0x08B3265C about the object
// (0 answers 0), anything else (and a positive answer) runs 0x08B2DC70, raises 0x08CE0C80 to its maximum with 0xDE as
// 0x08B2DBF0's argument and records the kind. v0 = 1 or 0.

// The shared body of 0x08B895E0 / 0x08C0DBBC (a0 = container with an end pointer at +4, a1 = destination, a2 = source):
// copies whole 0x20-byte elements of [source, word(a0+4)) (each copied through `element_copy` with its +0x10 word),
// finishes the range through `finish` and shrinks the container's size. v0 = a1.
void copy_elements_and_shrink(Runtime &rt, AllegrexContext &ctx, u32 element_copy, u32 finish) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const s32 count = static_cast<s32>(word(rt, self + 4u) - source) >> 5;
    u32 out = destination;
    if (count > 0) {
        u32 from = source;
        for (s32 i = 0; i < count; ++i) {
            if (from != out) {
                call(rt, ctx, element_copy, {out, from});
                put(rt, out + 0x10u, word(rt, from + 0x10u));
            }
            from += 0x20u;
            out += 0x20u;
        }
    }
    call(rt, ctx, finish, {out, word(rt, self + 4u), frame.at(0u)});
    const u32 size = word(rt, self + 4u);
    put(rt, self + 4u, size - ((source - destination) & ~31u));
    ctx.gpr[2] = destination;
}


// 0x08BA6470 (a0 = object, a1, a2): 0x08B569A0 takes a lock, 0x08B566BC prepares the list at a0+8, and while the index
// is below the container's count (the virtual +0x48 of the object at +0x48, or the halfword at +0x10 of the table at
// +0x40) the element from 0x08884D6C is checked and 0x08AFA208 gets the object's +0xC fields; 0x08B56940 releases.
void func_08BA6470(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08B569A0u, {self, arg1, arg2});
    call(rt, ctx, 0x08B566BCu, {word(rt, self + 8u)});
    u32 node = word(rt, self + 8u);
    u32 index = 0u;
    for (;;) {
        const u32 counter = word(rt, node + 0x48u);
        u32 limit;
        if (counter != 0u) {
            limit = call_indirect_result(rt, ctx, word(rt, word(rt, counter) + 0x48u), {counter}).v0;
        } else {
            const u32 table = word(rt, node + 0x40u);
            if (table == 0u) break;
            limit = rt.memory().load16(table + 0x10u);
        }
        if (index >= limit) break;
        node = word(rt, self + 8u);
        if (call(rt, ctx, 0x08884D6Cu, {node, arg2, 0u, index}) != 0u)
            call(rt, ctx, 0x08AFA208u, {word(rt, self + 0x10u), arg2, 0u, index, word(rt, self + 0x14u)});
        ++index;
    }
    tail_call(ctx, 0x08B56940u, {self});
}

// 0x08C206DC (a0 = object, a1 = output): 0x08A98C1C fills sp from the object's +8, 0x08A6159C builds a list of that
// many entries, and the object at sp runs its virtual +0x88 once per entry; 0x08A61584 hands the list back and the
// temporary at sp is destroyed.

// 0x08C2E994 (a0 = a start, a1 = an end, a2 = owner, a3 = predicate): erases the elements of [a0, a1) for which the
// predicate's virtual +8 answers non-zero, scanning from the front and then the back, keeping the elements the
// predicate rejects (0x08A76ABC / 0x08A5BAD8). v0 = the new end.
void func_08C2E994(Runtime &rt, AllegrexContext &ctx) {
    u32 position = ctx.gpr[4];
    u32 end = ctx.gpr[5];
    const u32 owner = ctx.gpr[6];
    const u32 predicate = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    for (;;) {
        u32 answer;
        for (;;) {
            answer = call_indirect_result(rt, ctx, word(rt, word(rt, predicate) + 8u), {predicate, position, owner}).v0;
            if (answer == 0u) break;
            position += 4u;
        }
        do {
            end -= 4u;
            answer = call_indirect_result(rt, ctx, word(rt, word(rt, predicate) + 8u), {predicate, owner, end}).v0;
        } while (answer != 0u);
        if (position >= end) {
            ctx.gpr[2] = position;
            return;
        }
        call(rt, ctx, 0x08A76ABCu, {frame.at(0u), position});
        if (end != position) call(rt, ctx, 0x08A5BAD8u, {position, word(rt, end)});
        if (end != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {end, word(rt, frame.at(0u))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        position += 4u;
    }
}

// 0x08C40088 (a0 = output, a1 = begin, a2 = end, a3 = a {table, size, kind} range): for every 0x20-byte element of
// [begin, end) the entry at table + size/2 runs (a word from a table of tables when size is odd) with the kind byte;
// the range header is copied to the output. v0 = a0.

// --- group 4 ---

// 0x089C121C / 0x089C1368 (a3 = pointer to a source): a temporary at sp+8 comes from 0x089BEE4C, the source's virtual
// +0xC fills sp+4, the site's own call turns it into the target, the head at sp+4 drops its count and the temporary at
// sp+8 is destroyed. v0 = the last destroy's.
void build_from_source(Runtime &rt, AllegrexContext &ctx, u32 second_call, u32 second_offset) {
    GuestFrame frame(ctx, 0x30u);
    const u32 source = ctx.gpr[7];
    call(rt, ctx, 0x089BEE4Cu, {frame.at(8u)});
    const u32 object = word(rt, source);
    const u32 first = word(rt, frame.at(8u));
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA5EB4u});
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA5E6Cu});
    call_indirect_result(rt, ctx, word(rt, word(rt, object) + 0xCu), {frame.at(4u), object});
    call(rt, ctx, second_call, {first + second_offset, frame.at(4u)});
    const u32 temporary = word(rt, frame.at(4u));
    const u32 head = temporary - 0xCu;
    if (head != 0x08D74FD0u) {
        const u32 remaining = call(rt, ctx, 0x08B60804u, {temporary - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(remaining) <= 0) call(rt, ctx, 0x08B8542Cu, {head, frame.at(0u)});
    }
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
}


// 0x089C151C (a2 = a count, a3 = pointer to two sources): with a count of 2 or more, three temporaries at sp / sp+4 /
// sp+8 come from 0x089BEE4C / 0x089AB370 / 0x08A6C410, 0x08A6BFFC reduces the third and 0x089C1440 combines them; all
// three are destroyed. v0 = the last destroy's.
void func_089C151C(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    if (static_cast<s32>(ctx.gpr[6]) < 2) return;
    call(rt, ctx, 0x089BEE4Cu, {frame.at(0u)});
    call(rt, ctx, 0x089AB370u, {frame.at(4u), source});
    call(rt, ctx, 0x08A6C410u, {frame.at(8u), source + 4u});
    const u32 first = word(rt, frame.at(0u));
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA5EB4u});
    const u32 second = word(rt, frame.at(8u));
    const u32 third = word(rt, frame.at(4u));
    if (second == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA60DCu});
    const u32 value = call(rt, ctx, 0x08A6BFFCu, {second});
    call(rt, ctx, 0x089C1440u, {first, third, value});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08A52EC4 (a0 = output, a1, a2 = a count, a3 = pointer to a source): with a positive count the source's virtual +0x3C
// gives a value (else -1), 0x08A52A18 fills sp, 0x08A52D3C builds the result at sp+4 from it, which is assigned to a0 when
// a0 is not sp+4; both temporaries are destroyed. v0 = the last destroy's.

// 0x08A58EF0 (a0 = object, a1 = a container with a node at +0x18/+0x1C, a2 = pointer to a key): walks to the container's
// node, takes the object's +0x38 as a lookup object, has its virtual +0x10 make a temporary at sp and its virtual +0xF4
// take the key; the temporary is destroyed. v0 = that destroy, or 0 when the key is null.
void func_08A58EF0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 container = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    u32 node = word(rt, container + 0x1Cu);
    if (node == 0u) {
        const u32 second = word(rt, container + 0x18u);
        if (second != 0u) {
            node = word(rt, second + 0xCu);
        } else {
            call(rt, ctx, 0x08A5BB7Cu, {0x08CAF864u});
            node = word(rt, second + 0xCu);
        }
    }
    if (word(rt, key) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 value = word(rt, node);
    if (value == 0u) {
        const u32 creator = word(rt, self + 0x38u);
        value = call_indirect_result(rt, ctx, word(rt, word(rt, creator) + 8u), {creator}).v0;
        put(rt, node, value);
    }
    const u32 lookup = word(rt, self + 0x38u);
    call_indirect_result(rt, ctx, word(rt, word(rt, lookup) + 0x10u), {frame.at(0u), lookup, value});
    const u32 result = word(rt, frame.at(0u));
    if (result == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF878u});
    call_indirect_result(rt, ctx, word(rt, word(rt, result) + 0xF4u), {result, key});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08A5EEA4 (a0 = object with a 64-bit value at +0x18/+0x1C, a1 = output, a3 = pointer to a source): the source's
// virtual +0x50 is the shift amount, the value is shifted left into (low, high), 0x08B6EE4C runs, and 0x08A5DEE8 builds
// the result at sp, which is assigned to a1 when a1 is not sp and destroyed. v0 = the destroy's.

// 0x08ACC504 (a0 = object): under the 0x08B34888 lock, 0x08C7B1C8 gives a state; a set byte at +0x8C with a state in
// 1..4 makes 0x08C7B170 create the object at +0x98 (recorded at +0x90, +0x8D set when it failed), otherwise state 2/3
// run 0x08C7B1B8(1) / 0x08C7B1A0 and state 0 with +0x8D set hands the value at +0x2D8 to +0x94 and 0x08B34CA0. The
// object's +0x60 goes on to 0x08B34B24.

// 0x08ACE4F4 (a0 = object): stores the vtable 0x08CD6B28 at +0 and runs the teardown of the three sub-object ranges
// (+0x4A0/+0x474/+0x448/+0x420, +0x3B8/+0x38C/+0x360/+0x334) unless they are empty, then +0x164/+0x40; the rest goes on
// in 0x08B64654.

// 0x08AEE608: waits until 0x08C7B118's bit 0x20 is set, answering 1 then (naming the assert sites at 0x08C924C4 /
// 0x08C924D4 with the flag at 0x08CDFB3C), and answers 0 when 0x08AD9E64 says so in between; 0x08C7A988 waits 0xC350.

// 0x08B0E784 (a0 = object, a1 = a flag byte): under the 0x08B34888 lock, every node of the list at +0xFC has its
// object's virtual +0xC called with the flag; with an empty list and a set flag the object's own virtual +0x24 runs with
// the byte at +0x68 cleared. When that byte was set the work goes on in 0x08B0DEB0.

// 0x08B1916C (a0 = object, a1 = a begin, a2 = an end or 0): 0x08B19154 seeds a walk at sp; every node's string at +8
// is compared byte for byte with [a1, a2) (or, with a2 == 0, checked by 0x08B35FD8), the node answers the address when
// it matches and 0x08B18CEC moves to the next node. v0 = the node or 0.

// 0x08B94DF8 (a0 = tree with the root at +8 and its end node at +0xC, a1 = pointer to a key): a binary search on the
// key words at +0x10; an existing key answers its node, a new one is created by 0x08B94D30 through 0x08B5F174 when the
// walk lands on the tree's end node. v0 = the node, new or found.

// --- group 5 ---

// 0x08833D44 (a0 = output, a1): a temporary at sp+4 comes from 0x08A2DA98, 0x088297A8 turns a1 into sp, 0x088294D0 /
// 0x0885EDF8 reduce it, then the first temporary's +0x14 through 0x08A2D1B8 is combined by 0x088D9F2C; the value is
// assigned to a0 when it is not sp+4. Both temporaries are destroyed; v0 = the last destroy's.

// 0x0885D3A0 (a0 = object): destroys the two-element arrays at +0x128..+0x188 (0x088978B4), runs 0x088A7548 on +0x120,
// the two-element arrays at +0xB8..+0x118 and +0x28..+0x58 (when present and not empty), and goes on in 0x088A7548 with
// +0x50.

// 0x08898570 (a0 = object, a1, a2, a3): 0x088F8D58 makes the base whose +0x20 0x08897470 fills from a3; with an object
// at +0x40 and a non-null a3, a kind of 1 runs 0x0889854C and 0x088970E0 / 0x088984E4, anything else the object's
// virtual +0x14 with a zeroed three-word record at sp.

// 0x088A61A4 (a0 = object whose +0 is the owner): unless the owner's byte at +0x4CC is set, a 0x1CB0-byte object is
// made by 0x08B62344 / 0x08B36018 and stored at the owner's +0x4C8; it is set up by 0x0892E51C (at the owner's
// +0x38 + count*384 with the count byte at +8), 0x08944100, 0x08951304 and 0x08948D04, whose float lands at +0x1CAC.
void func_088A61A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = word(rt, self);
    const u32 flag = rt.memory().load8(owner + 0x4CCu);
    ctx.gpr[2] = flag;
    if (flag != 0u) return;
    const u32 object = call(rt, ctx, 0x08B62344u, {0x1CB0u});
    call(rt, ctx, 0x08B36018u, {object, 0u, 0x1CB0u});
    put(rt, owner + 0x4C8u, object);
    const u32 base = word(rt, self);
    const s32 count = static_cast<std::int8_t>(rt.memory().load8(base + 8u));
    call(rt, ctx, 0x0892E51Cu, {base + static_cast<u32>(count) * 384u + 0x38u, object});
    const u32 middle = object + 0x690u;
    call(rt, ctx, 0x08944100u, {middle, object, word(rt, self) + 0xCu, 0xFFFFFFFFu});
    const u32 last = object + 0x1B30u;
    call(rt, ctx, 0x08951304u, {object, object + 0x1C30u, last, word(rt, self) + 0xCu});
    put_float(rt, object + 0x1CACu, call_floats(rt, ctx, 0x08948D04u, {middle, last}, {}).f0);
}

// 0x08938188 (a0 = object, a1 = key): a binary search over the table at word(a0+4) (elements of 8 bytes, keys at +0x14)
// with 0x08938088 as the comparison; the found element's key answers, else -1. v0 = the key word or -1.

// 0x0897F474 (a0, a1, a2, a3 = pointer to a source): a temporary at sp+8 comes from 0x0897E844, the source's virtual
// +0xC fills sp+4, 0x0897F34C combines them, the head at sp+4 drops its count and the temporary is destroyed.

// --- group 6 ---

// 0x08BDB668 (a0 = element array, a1 = index, a2 = count; no frame of its own): the sift-down step of a heap over
// 8-byte elements whose key word is at +4: while the larger child (the right one, or the left on a signed key tie
// broken by an unsigned value compare) keeps being smaller than the count, it is copied down into the hole; the last
// element then fills the hole when the walk ended on the last index. The work goes on in 0x08BDB598.

// The shared body of 0x08BECB24 / 0x08C10B60 (a0 = object, a2 = pointer to a base, t0 = a value): 0x08AA2A30 and the
// site's own call make two temporaries, then the size/pointer pairs at +0x14/+0x18 and +0x1C/+0x20 select two calls of
// the base's element (an odd size selects a word from a table), the second with the temporary's pointer.
void two_temporary_lookup(Runtime &rt, AllegrexContext &ctx, u32 frame_bytes, u32 make) {
    const u32 self = ctx.gpr[4];
    const u32 base = word(rt, ctx.gpr[6]);
    const u32 argument = ctx.gpr[8];
    GuestFrame frame(ctx, frame_bytes);
    call(rt, ctx, 0x08AA2A30u, {argument});
    const u32 made = call(rt, ctx, make, {frame.at(0u), argument + 4u});
    const u32 size1 = word(rt, self + 0x18u);
    const u32 inner1 = word(rt, self + 0x14u);
    const u32 at1 = base + sra(size1, 1);
    const u32 target1 = (size1 & 1u) != 0u ? word(rt, word(rt, at1) + inner1) : inner1;
    const u32 first = call(rt, ctx, target1, {at1, size1});
    const u32 size2 = word(rt, self + 0x20u);
    const u32 inner2 = word(rt, self + 0x1Cu);
    const u32 at2 = first + sra(size2, 1);
    const u32 target2 = (size2 & 1u) != 0u ? word(rt, word(rt, at2) + inner2) : inner2;
    call_indirect_result(rt, ctx, target2, {at2, made, frame.at(0u)});
}


// The shared body of 0x08BFE9FC / 0x08BFEAD4 / 0x08C1D458 / 0x08C2BC8C / 0x08C35350 (a0 = tree with the root at +8 and
// its end node at +0xC, a1 = pointer to a key): a binary search on the unsigned key words at +0x10; an existing key
// answers its node, a new one is created by the site's own call through 0x08B5F174 when the walk lands on the end node.
void tree_find_or_insert(Runtime &rt, AllegrexContext &ctx, u32 create) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    u32 node = word(rt, self + 8u);
    u32 found = self + 4u;
    bool goes_left = false;
    if (node != 0u) {
        const u32 key_value = word(rt, key);
        for (;;) {
            goes_left = key_value < word(rt, node + 0x10u);
            const u32 child = goes_left ? word(rt, node + 8u) : word(rt, node + 0xCu);
            found = node;
            if (child == 0u) break;
            node = child;
        }
        if (!goes_left) {
            if (word(rt, node + 0x10u) < word(rt, key))
                ctx.gpr[2] = call(rt, ctx, create, {self, 0u, found, key});
            else ctx.gpr[2] = node;
            put8(rt, frame.at(4u), 0u);
            return;
        }
    }
    if (found == word(rt, self + 0xCu)) {
        ctx.gpr[2] = call(rt, ctx, create, {self, 0u, found, key});
    } else {
        const u32 fetched = call(rt, ctx, 0x08B5F174u, {found});
        if (word(rt, fetched + 0x10u) < word(rt, key))
            ctx.gpr[2] = call(rt, ctx, create, {self, 0u, found, key});
        else ctx.gpr[2] = fetched;
    }
    put8(rt, frame.at(4u), 0u);
}

void func_08BFE9FC(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08BFE92Cu); }
void func_08BFEAD4(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08BFE808u); }
void func_08C1D458(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08C1D2DCu); }
void func_08C35350(Runtime &rt, AllegrexContext &ctx) { tree_find_or_insert(rt, ctx, 0x08C35280u); }

// 0x08C49A28 (a0 = container with an end pointer at +4, a1 = destination, a2 = source): copies the whole range
// [a2, word(a0+4)) in 240-byte elements (each in 15 chunks of four words), appends through 0x08C47014 and shrinks the
// container's size by the copied byte count with the low four bits cleared. v0 = a1.

// 0x08C4B284 (the same shape as 0x08C49A28 with 112-byte elements: 7 chunks of four words, appended by 0x08C4B274).

// 0x0882E8B0 (a0 = output, a1): a temporary at sp+8 comes from 0x088297A8, 0x088294D0 / 0x0885E710 reduce it and
// 0x08917A98 classifies it; the answer picks the result at sp+4 (0) or sp (1), which 0x08A85B1C builds and which is
// assigned to a0 when it is not that temporary. v0 = the last destroy's.

// 0x0884D6A0 (a2 = a count, a3 = pointer to two objects): with a count of 2 or more, a temporary at sp comes from
// 0x0884D660, 0x0884D3D8 makes its value and the two objects' virtuals +0x3C are combined by 0x0889722C; the temporary
// is destroyed.
void func_0884D6A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    if (static_cast<s32>(ctx.gpr[6]) < 2) return;
    call(rt, ctx, 0x0884D660u, {frame.at(0u)});
    const u32 first = word(rt, frame.at(0u));
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D198u});
    const u32 middle = call(rt, ctx, 0x0884D3D8u, {first});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    const u32 a = word(rt, source);
    if (a == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D158u});
    const u32 va = call_indirect_result(rt, ctx, word(rt, word(rt, a) + 0x3Cu), {a}).v0;
    const u32 b = word(rt, source + 4u);
    if (b == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D158u});
    const u32 vb = call_indirect_result(rt, ctx, word(rt, word(rt, b) + 0x3Cu), {b}).v0;
    ctx.gpr[2] = call(rt, ctx, 0x0889722Cu, {middle, va, vb});
}

// 0x088692E8 (a0 = object): sets a 0x1000-byte allocation up around the object at +0x74 / +0x1B0 through the 0x0886xxxx
// helpers and goes on in 0x08869D58 with +0x1B0.

// 0x08936ED0 (a0 = object): stores the vtable 0x08CC67B8 and runs the virtual +4 of the 44 pointers at +0x8C and +0x13C
// (four bytes apart), then of +0x1EC and +0x1F0; 0x08938FCC runs on +0x2D0 and goes on with +0x1F4.

// 0x089C0EA8 (a0 = output, a3 unused): a temporary at sp+0xC comes from 0x089BEE4C, 0x089BFE58 builds sp+4 from it,
// 0x08A6D2D4 builds sp+8 from that, which is assigned to a0 when a0 is not sp+8; both temporaries are destroyed.

// 0x089C1440 (a0 = object, a2 = a value): under the lock at a0+0x2B0, 0x08B85EB8 / 0x08B855E0 put the value into
// a0+0x230 (the temporary at sp+8 drops its count), 0x089C5680 builds sp+4 from a0+0x22C, which is assigned to
// a0+0x264 when different, and the lock is released.

// The shared body of 0x089D0D4C / 0x089D0E28 / 0x089E9544 / 0x089E9620 / 0x089E96FC / 0x089E97D8 (a0 = output,
// a2 = a count, a3 = pointer to a source): a temporary at sp+4 comes from `make`, its value (1 when the count is not
// positive, else the source's virtual +0x3C) goes through the site's own builder and `finish`, which fills sp; the
// result is assigned to a0 when a0 is not sp and both temporaries are destroyed.
void build_through_count(Runtime &rt, AllegrexContext &ctx, u32 make, u32 assert_source, u32 assert_temp, u32 build,
                         u32 finish, u32 initial_value) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, make, {frame.at(4u)});
    u32 value = initial_value;
    if (static_cast<s32>(count) > 0) {
        const u32 object = word(rt, source);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_source});
        value = call_indirect_result(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object}).v0;
    }
    const u32 temporary = word(rt, frame.at(4u));
    if (temporary == 0u) call(rt, ctx, 0x08A5BB7Cu, {assert_temp});
    const u32 built = call(rt, ctx, build, {temporary, value});
    call(rt, ctx, finish, {frame.at(0u), built});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}


// 0x08A06BF8 (a0 = object, a1 = pointer to a source): a 0x34-byte record at sp+4 (vtable 0x08CCE190, a static head,
// -1, four 1.0 floats and a pointer to sp+0x20) goes to the source's virtual +8; the head is released and 0x08A05490
// runs on the object with the record.

// 0x08A21928 (a0 unused past the first store, a2 = a count, a3 = pointer to a source): a temporary at sp+4 comes from
// 0x08A21818; with a positive count the source's virtual +0x54 (a float) lands at +0x1C of it, otherwise its own +0x1C
// float goes through 0x08A87EEC into sp. Both temporaries are destroyed.

// 0x08A41F4C (a0 = output): a temporary at sp+4 comes from 0x08A40E90, 0x08B88F44 builds sp+8 from its +0x44 / +0x48
// and 0x08A6D2D4 builds sp+0xC, which is assigned to a0 when a0 is not sp+0xC; all temporaries are destroyed.

// 0x08A8712C (a0 = output, a1, a2, a3): a temporary at sp+4 comes from 0x08A76AAC with the argument at sp, a 12-byte
// object is allocated by 0x08B62344 (its vtable at +0 is set twice, 0x08CD4168 last), its +4 and +8 by 0x08A76AA0 /
// 0x08A62AB8, and its +8 through 0x08A641F4; 0x08A851CC takes it and the temporary is destroyed. v0 = a0.
void func_08A8712C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0u), ctx.gpr[5]);
    call(rt, ctx, 0x08A76AACu, {frame.at(4u), frame.at(0u)});
    const u32 object = call(rt, ctx, 0x08B62344u, {0xCu});
    put(rt, object, 0x08CD1F30u);
    call(rt, ctx, 0x08A76AA0u, {object + 4u});
    put(rt, object, 0x08CD4168u);
    call(rt, ctx, 0x08A62AB8u, {object + 8u});
    const u32 inner = word(rt, object + 8u);
    if (inner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB18C4u});
    call(rt, ctx, 0x08A641F4u, {inner, frame.at(4u), ctx.gpr[6], ctx.gpr[7]});
    call(rt, ctx, 0x08A851CCu, {self, object});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[2] = self;
}

// 0x08AB3420 (a0 = output): a temporary at sp+8 comes from 0x08AB2D80, 0x08AB3330 builds sp+0xC from it, which
// 0x08A6D2D4 turns into sp+4 for a0 (when a0 is not sp+4); all three temporaries are destroyed.

} // namespace
} // namespace new_batch_main_17
} // namespace gt::game

#define NEW17(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_17::name, gt::ReturnKind::kind)
NEW17(0x088725E4u, func_088725E4, Word);
NEW17(0x08B0A458u, func_08B0A458, Word);
NEW17(0x08BA6470u, func_08BA6470, Word);
NEW17(0x08C2E994u, func_08C2E994, Word);
NEW17(0x089C151Cu, func_089C151C, Word);
NEW17(0x08A58EF0u, func_08A58EF0, Word);
NEW17(0x088A61A4u, func_088A61A4, Word);
NEW17(0x08BFE9FCu, func_08BFE9FC, Word);
NEW17(0x08BFEAD4u, func_08BFEAD4, Word);
NEW17(0x08C1D458u, func_08C1D458, Word);
NEW17(0x08C35350u, func_08C35350, Word);
NEW17(0x0884D6A0u, func_0884D6A0, Word);
NEW17(0x08A8712Cu, func_08A8712C, Word);
