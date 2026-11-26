// New batch, main session 19: functions nothing had taken (the recompiler's output, none decompiled before), written as structured C
// by hand from `tools.disasm`; verified by the differential gate afterwards. No names: none has evidence yet.
// Grouped by shape where the asm repeats: one wrapper, one row per site.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_19 {
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

// A `lw v0, slot(vtable); jalr v0` call: keeps v1 and f0 as well (a caller reads the callee's v1 or f0).
GuestResult call_indirect_result(Runtime &rt, AllegrexContext &ctx, u32 target,
                                 std::initializer_list<u32> arguments = {}) {
    return call_indirect(rt, ctx, target, arguments, {});
}

// `sra` (arithmetic) as the asm spells it; >> on an unsigned local would be a logical shift.
u32 sra(u32 value, unsigned bits) { return static_cast<u32>(static_cast<s32>(value) >> bits); }
// The asm's `ext value, position, width`: the bits [position, position+width).
u32 extract(u32 value, unsigned position, unsigned width) { return (value >> position) & ((1u << width) - 1u); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// The callee-saved registers s0-s7: the original saves and restores the ones it uses, so the body keeps them here and a nested
// callee that reads one it never wrote sees the caller's value.
struct SavedCalleeSaved {
    AllegrexContext &ctx;
    u32 saved[8];
    explicit SavedCalleeSaved(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedCalleeSaved() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
    }
};

// The two indexed virtual lookups several functions here share (0x08BF4728 and its neighbours, 0x08C07510, 0x08C26328):
// a size/table pair at self+0x18/+0x14 and a second at self+0x20/+0x1C. An odd size means one more indirection: the table
// word at the halved index is an object whose word at the table offset is the target. Even: the table value is the target.
struct IndexedLookup {
    u32 at;      // the object the call is made on
    u32 size;    // its size field (its low bit selects the indirection)
    u32 table;   // the table/base word
    u32 target;  // the function to call
};
IndexedLookup lookup_at(Runtime &rt, u32 self, u32 base, u32 size_offset, u32 table_offset) {
    const u32 size = word(rt, self + size_offset);
    const u32 table = word(rt, self + table_offset);
    const u32 at = base + sra(size, 1);
    const u32 target = (size & 1u) != 0u ? word(rt, word(rt, at) + table) : table;
    return {at, size, table, target};
}
IndexedLookup first_lookup(Runtime &rt, u32 self, u32 base) { return lookup_at(rt, self, base, 0x18u, 0x14u); }
IndexedLookup second_lookup(Runtime &rt, u32 self, u32 base) { return lookup_at(rt, self, base, 0x20u, 0x1Cu); }

// --- conversion through a temporary string, 10 sites ---

// 0x0884DDA8 family (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): a lookup into a stack
// temporary (asserted non-null with a message), a conversion of it, then either the count is positive and the source object's
// virtual +0x3C is forwarded to the conversion's finish call, or the temporary is built from the fallback and assigned to the
// output. The temporary is always released; v0 is the last call's result.
void convert_from_source(Runtime &rt, AllegrexContext &ctx, u32 lookup, u32 check_message, u32 convert, u32 source_message,
                         u32 finish, u32 fallback, bool flag_argument) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 local = frame.at(4u);
    ctx.gpr[20] = local;
    ctx.gpr[21] = out;
    ctx.gpr[19] = source;
    ctx.gpr[18] = count;
    call(rt, ctx, lookup, {local});
    const u32 holder = word(rt, local);
    ctx.gpr[16] = holder;
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {check_message});
    const u32 converted = call(rt, ctx, convert, {holder});
    ctx.gpr[17] = converted;
    call(rt, ctx, 0x08A5B9ECu, {local});
    if (static_cast<s32>(count) > 0) {
        const u32 object = word(rt, source);
        ctx.gpr[16] = object;
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {source_message});
        const u32 value = call_indirect_result(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object}).v0;
        ctx.gpr[2] = call(rt, ctx, finish, {converted, flag_argument ? (value != 0u ? 1u : 0u) : value});
        return;
    }
    ctx.gpr[4] = converted;
    const u32 text = call(rt, ctx, fallback, {converted});
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), text});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- a float field of a looked-up object, 6 sites ---

// 0x08981620 family (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): with a count of at
// most 0 a float of the looked-up object is formatted and assigned to the output; otherwise the source object's virtual +0x54
// supplies the float, which is written to the looked-up object at +0xF0+write_offset (and the flag byte at +0xF0+0xC4 set).
// v0 is the release call's result.
void update_float_field(Runtime &rt, AllegrexContext &ctx, u32 lookup, u32 read_offset, u32 write_offset) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = out;
    ctx.gpr[16] = source;
    if (static_cast<s32>(count) <= 0) {
        const u32 local = frame.at(4u);
        ctx.gpr[18] = local;
        call(rt, ctx, lookup, {local});
        const u32 object = word(rt, local);
        ctx.gpr[16] = object;
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA3400u});
        const float value = float_at(rt, object + read_offset);
        const u32 text = frame.at(8u);
        ctx.gpr[16] = text;
        call_floats(rt, ctx, 0x08A87EECu, {text}, {value});
        const u32 built = word(rt, text);
        if (out != text) call(rt, ctx, 0x08A5BAD8u, {out, built});
        call(rt, ctx, 0x08A5B9ECu, {text});
        ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {local});
        return;
    }
    call(rt, ctx, lookup, {frame.at(0u)});
    const u32 holder = word(rt, frame.at(0u));
    ctx.gpr[17] = holder;
    const u32 object = word(rt, source);
    ctx.gpr[16] = object;
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA3400u});
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA3390u});
    const float value = call_indirect_result(rt, ctx, word(rt, word(rt, object) + 0x54u), {object}).f0;
    put8(rt, holder + 0xF0u + 0xC4u, 1u);
    put_float(rt, holder + 0xF0u + write_offset, value);
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- two indexed lookups forward to a virtual ---

// 0x08BF4728 (a0 = lookup object, a1 = destination, a2 = pointer to the source object, t0 = conversion argument): converts t0
// (0x08AA2A30), walks the two indexed lookups of a0 on the source object and calls the second target with the conversion's
// result, formats the returned float into a temporary and assigns it to the destination.

// 0x08BFE018: the same body, but the second target's integer result is handed to 0x08B8C564 with the temporary.

// 0x08C07510 (a0 = lookup object, a1 unused, a2 = pointer to the source object, t0 = three-word argument): converts the three
// words at t0, then tail-calls the second indexed target with them in a1-a3.

// 0x08C26328 (a0 = lookup object, a1 unused, a2 = pointer to the source object, t0 = two-word argument): fills two stack
// temporaries from t0's two words, calls the second indexed target with them, then releases both temporaries.

// --- one-offs ---

// 0x08ABAFCC (a0 = object, a1/a2 forwarded to the first call, a3 = index, t0 = flags, f12/f13 = floats): the first call gives a
// table with a count at +0x10; a negative index is taken modulo that count (unsigned), and the word at +0x14 of the element is
// converted (0x08AD3B5C) and handed to a tail call with the object, the conversion and the flags.
void func_08ABAFCC(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[7];
    const u32 flags = ctx.gpr[8] & 0xFFu;
    const float first_float = ctx.fpr[12];
    const float second_float = ctx.fpr[13];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = object;
    ctx.gpr[18] = index;
    ctx.gpr[20] = flags;
    ctx.fpr[20] = first_float;
    ctx.fpr[21] = second_float;
    const u32 found = call_floats(rt, ctx, 0x08AD3C5Cu, {ctx.gpr[6], ctx.gpr[5]}, {first_float, second_float}).v0;
    ctx.gpr[16] = found;
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 count = word(rt, found + 0x10u);
    ctx.gpr[17] = count;
    if (count == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 position;
    if (static_cast<s32>(index) >= 0) {
        position = index;
    } else {
        position = call(rt, ctx, 0x08B1ECC8u, {ctx.gpr[6]}) % count;
    }
    if (position >= count) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 inner = word(rt, (position << 2) + found + 0x14u);
    const u32 converted = call_floats(rt, ctx, 0x08AD3B5Cu, {inner}, {first_float, second_float}).v0;
    tail_call(ctx, 0x08ABADE0u, {object, converted, flags, index});
}

// 0x08AF3084 (a0 = object, a1 = record): the record's +4 (or, when that is null, +8) is an object whose virtual +8 fills a
// stack vector; with a non-zero +0x14 the vector is scaled by sqrt(+0x14 * pi/90) * +0x10, then a rotated vector is built and
// handed to 0x08B4E734. The no-object path calls 0x08AF4294 and returns a0.

// 0x08B4485C (a0 = self, a1 = flags): reads a 64-bit stamp through the member's virtual +0x1C, compares it with the one kept at
// self+0x910/+0x914 and, depending on how old it is and on the state at self+0x930, stores the new stamp and calls 0x08B429D0
// with the frame as its out-parameter; v0 is the state pointer on that early exit, else the comparison flag or the call.

// 0x08B44B24 (a0 = self): with the byte at +0xAD set and a member at +0xB0, resets the sub-object at +0x12C, reads two words
// through the member's virtuals, forwards them, and tail-calls 0x08B3457C with the sub-object after bumping the counter at +0xC4.
void func_08B44B24(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 sub = self + 0x12Cu;
    ctx.gpr[16] = self;
    ctx.gpr[17] = sub;
    if (rt.memory().load8(self + 0xADu) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (word(rt, self + 0xB0u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B344E0u, {sub});
    if (word(rt, self + 0xB0u) != 0u) {
        const GuestResult first = call_indirect_result(rt, ctx, word(rt, word(rt, self) + 0x8Cu), {self, 0u});
        put(rt, self + 0x928u, first.v0);
        put(rt, self + 0x92Cu, first.v1);
        call(rt, ctx, 0x08B44948u, {self, 0u, first.v0, first.v1});
        call(rt, ctx, 0x08B43A38u, {self});
        if (call_indirect_result(rt, ctx, word(rt, word(rt, self) + 0xBCu), {self, 0u}).v0 != 0u)
            call(rt, ctx, 0x08B43B3Cu, {self, 0u, word(rt, self + 0x928u), word(rt, self + 0x92Cu)});
        const u32 member = word(rt, self + 0xB0u);
        call_indirect_result(rt, ctx, word(rt, word(rt, member) + 0x10u), {member, 0u});
    }
    call(rt, ctx, 0x08B084F0u, {self});
    put(rt, self + 0xC4u, word(rt, self + 0xC4u) + 1u);
    tail_call(ctx, 0x08B3457Cu, {sub});
}

// --- a conversion of a looked-up object, then a source object's virtual ---

// 0x088314BC (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): a lookup into a stack
// temporary, its conversion (0x088294D0 then 0x0885EDEC), then -1 when the count is not positive, else the source object's
// virtual +0x3C; 0x088DF324 combines both into a string that is assigned to the output. v0 = the temporary's release.

// 0x0884DEA0 (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): a lookup (0x0884D660) and
// its conversion (0x0884D3D8), then the source object's virtual +0x3C is stored at +0x88 of the conversion and the structure
// is formatted; a non-positive count formats the already stored value instead. v0 = the stored value or the release.

// 0x08850B84 (a0 = the output string, a1 = the source object): converts the source (0x088547C4/0x08854678), reads a count and
// that many pairs of floats from 0x088A5B3C into a 0x804-byte buffer (the count first), formats the buffer with 0x08A44D9C and
// assigns it. v0 = the temporary's release.

// 0x08851670 (a0 = the output string, a1 = the first source object, a2 unused, a3 = the second source object): converts both
// sources (0x088547C4/0x08854678), combines them with 0x088A78B4 and assigns the formatted result. v0 = the release.

// 0x08891984 (a0 = self): fills +0x24/+0x28 from two calls over the object at +0xC, submits a whole record (the indexed entry
// at +0x44, the words at +4..+0x18, t2 = 1) to 0x0888FBD8, runs two more helpers on the same entry and 0x0889605C/0x088963C4
// on +0x24, then writes the command word into the indexed slot at +0x50 and sets the byte at +0x6B. v0 = that command word.

// 0x088A2274 (a0 = self): resets the sub-object at +4+4, clears the words at +0x10 to +0x78, copies the 13 words at +0x10 into
// the buffer at +8 when there is one, and tail-calls 0x08B3457C with the sub-object.
void func_088A2274(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08B344E0u, {word(rt, self + 4u) + 4u});
    const u32 destination = word(rt, self + 8u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x44u, 0u);
    put8(rt, self + 0x78u, 0u);
    for (u32 offset = 4u; offset <= 0x30u; offset += 4u) {
        put(rt, self + 0x10u + offset, 0u);
        put(rt, self + 0x44u + offset, 0u);
    }
    if (destination != 0u) {
        u32 from = self + 0x10u;
        u32 to = destination;
        while (from != self + 0x40u) {
            put(rt, to, word(rt, from));
            from += 4u;
            to += 4u;
        }
        put(rt, to, word(rt, from));
    }
    tail_call(ctx, 0x08B3457Cu, {word(rt, self + 4u) + 4u});
}

// 0x08955AD0 (a0 = self, a1 = output vector, a2 = index, f12 = blend): interpolates the three floats of the record index*12 of
// 0x089551B0 and the vector at 0x0893C780/0x08955300 by the blend value (1-blend, blend). v0 = the last call's result.
void func_08955AD0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 index = ctx.gpr[6];
    const float blend = ctx.fpr[12];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[18] = out;
    ctx.gpr[17] = self;
    ctx.gpr[16] = index;
    ctx.fpr[21] = blend;
    const float total = call_floats(rt, ctx, 0x089559C4u, {self}, {blend}).f0 + blend;
    ctx.fpr[20] = total;
    const u32 records = call(rt, ctx, 0x089551B0u, {self});
    ctx.gpr[19] = records;
    const u32 vector = call(rt, ctx, 0x0893C780u, {self, index});
    ctx.gpr[20] = vector;
    const u32 other = call(rt, ctx, 0x08955300u, {self, index});
    const u32 record = (index * 12u) + records;
    const float inverse = 1.0f - total;
    put_float(rt, out, (total * float_at(rt, record + 0xC8u)) + (inverse * float_at(rt, vector)));
    put_float(rt, out + 4u, (total * float_at(rt, record + 0xCCu)) + (inverse * float_at(rt, other + 0xBCu)));
    put_float(rt, out + 8u, (total * float_at(rt, record + 0xD0u)) + (inverse * float_at(rt, vector + 8u)));
}

// 0x088BEB4C (a0 = self, f12 = scale): v0 of 0x088BE9C4 chooses; zero interpolates two angles of the object at +0x14 by the
// scale and adds the scale in 0x088BB5C4, otherwise the value at the frame is clamped to [-1, 1], scaled by the float at
// +0x28C of that object, offset by the scale and floored at 0.1. f0 = the angle.

// 0x08C282B0 (a0 = lookup object, a1 = destination, a2 = pointer to the source object, t0 = argument): fills a stack temporary
// through 0x08A80BAC(t0), walks the two indexed lookups of a0 on the source object with that temporary as a1 and hands the
// second target's result to 0x08B8C564 with a fresh temporary, which is assigned to the destination; both are released.

// 0x089C0270 (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): for a positive count the
// source object's virtual +0x3C (as a flag) is stored at +0x243 of the looked-up object; otherwise that flag is formatted into
// the output. v0 = the last release.

// 0x08A2D840 (a0 = the output string): a lookup and two conversions build a temporary string and a record; the string is
// assigned to the output and the record is released (0x08B60804/0x08B8542C when it is not the shared empty one). v0 = the
// first temporary's release.

// 0x08A395B4 (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): for a positive count the
// source object's virtual +0x54 (a float) is handed to 0x08A38D28 with the looked-up object; otherwise the float at +0x20 of the
// looked-up object is formatted into the output. v0 = the last release.

// 0x08A63A6C (a0 = self): a destructor - stores the vtable word, then for as long as the vector at +0x24 is not empty takes its
// last element, runs its virtuals +0xC and +4 and shrinks the vector; then releases +0x3C, +0x54 and +0x4C, frees the vector
// storage (0x08A5BC54 when the base is not null) and finishes with 0x08A780B0 on self.

// 0x08A6FD00 (a0 = self, a1 = offset, a2 = pointer to the source object): reads a character through the source object's
// virtual +0xC into a temporary, and when the temporary's header says so selects the table at self+0x14 (0x08B88FF8 settles a
// lazy index) and stores the character at table+offset; the temporary is released. v0 = the release.

// 0x08A8B4E4 (a0 = self, a1 = string, a2 unused, a3 = pointer to the object): takes the object's virtual +0x58 twice (its
// patch words through 0x08B71EC8), combines the two floats (0x08B6728C, 0x08B70F28) and builds the string in a temporary that
// is assigned to a1. v0 = the temporary's release.
// 0x08AA3AF8 (a0 = the output string, a2 = pointer to the holder): three objects at holder+0/+4/+8 each answer float +0x54
// (a, b, c); the result a + c*(b - a) is formatted into the output. v0 = the release.

// 0x08AB6564 (a0 = the output string): a lookup (0x08AB61DC), its byte (0x08AB5EFC), a record built by 0x08C00AD4 from that
// byte and a one-byte range at sp+1, then 0x08A6D2D4 formats the record into the output; the record is released.

// 0x08AB9B04 (no arguments): walks the global list at 0x08D7717C and inserts each node's record into the global table at
// 0x08CFFA10 (the first entry whose +8 value is not below the node's), then runs +0x10 on every remaining entry and tail-calls
// 0x08ABA264.
// 0x08AC35F0 (a0 = source, a1 = output record, a2 = a value): clears the record, and when the source's first word is negative
// or the value is below its +8 field prepares the source (0x08AC2D10, 0x08AC3570); then 0x08C7A858 writes into the record and
// a positive answer bumps the counter at +8. v0 = 1 when it was written, else 0.

// 0x08B1F2B8 (a0 = self): walks a list of string segments: each entry is looked up (0x08B1F67C), its length taken
// (0x08B1F4DC) and a comparison run (0x08B1F4F4, 0x08B1F5DC); on a match the last entry advances (0x08B1F1C4) until the entry
// is the one at +0x20. v0 = the matching entry, or 0.
// 0x08B34B2C (a0 = self): snapshots the state words +0x18..+0x24 into the frame, resets them (+0x20 = 1, +0x1C/+0x24 = -1,
// +0xC = the frame), runs 0x08B349C4/0x08B34748 under the two 0x08C7A8xx calls and restores the snapshot. v0 = the last call's.

// 0x08B79E68 (no arguments; $k0 is the exception context): finds the record at +0x37C of the context object at $k0+4 in the
// 32-bucket table at 0x08D72D1C (each bucket chain of 0x84-byte records keyed by that value) and returns its +4 word; not found
// reports 0x08C96E90 through 0x08C7A960 and returns the global 0x08CE0E60. v0 = the record word.
void func_08B79E68(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kTable = 0x08D72D1Cu;
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 context = ctx.gpr[26];
    if (context == 0u) {
        call(rt, ctx, 0x08C7A960u, {0x08CB5E9Cu, 0x08C96E90u});
        ctx.gpr[2] = word(rt, 0x08CE0E60u);
        return;
    }
    const u32 object = word(rt, context + 4u);
    ctx.gpr[16] = object;
    if (object == 0u) {
        call(rt, ctx, 0x08C7A960u, {0x08CB5E9Cu, 0x08C96E90u});
        ctx.gpr[2] = word(rt, 0x08CE0E60u);
        return;
    }
    call(rt, ctx, 0x08C7A888u, {});
    const u32 key = word(rt, object + 0x37Cu);
    const u32 adjust = static_cast<s32>(key) < 0 ? 31u : 0u;  // the asm's signed `key % 32`
    const u32 offset = (((key + adjust) & 0x1Fu) - adjust) << 2;
    u32 chain = kTable;
    for (;;) {
        const u32 at = offset + chain;
        if (key == word(rt, at + 0x84u)) {
            call(rt, ctx, 0x08C7A8A8u, {});
            ctx.gpr[2] = word(rt, at + 4u);
            return;
        }
        chain = word(rt, chain);
        if (chain == 0u) break;
    }
    call(rt, ctx, 0x08C7A8A8u, {});
    call(rt, ctx, 0x08C7A960u, {0x08CB5EBCu, 0x08C96E90u});
    ctx.gpr[2] = word(rt, 0x08CE0E60u);
}

// 0x08BF0784 (a0 = lookup object, a1 = destination, a2 = pointer to the source object): the two indexed lookups over the source
// object's first word; the second target's result is formatted through 0x0896418C/0x08BEFE64 (with the word at +0x10) and
// assigned to the destination. v0 = the release.
// 0x08C0CA90 (a0 = container, a1 = record buffer): copies (end - (a1+0x40)) / 0x40 records of 0x40 bytes from a1+0x40 down to
// a1 (five floats, a word at +8 and two objects via 0x089FB570), then shrinks the container's end by one record and destroys
// that record through its vtable's first virtual. v0 = a1, the buffer.

// 0x08C581D0 (a0 = self, a1 = output word): fills a stack scratch through 0x08C55FDC and, when its first byte is set, asks the
// base object's handler at +0x84 (virtual +0xC with out-parameters in the frame) for a value, stores it at *a1 and, when the
// handler's flags are set, ors them into the object's +0x14. v0 = self; a missing handler throws through 0x08B5A9FC.
void func_08C581D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = out;
    call(rt, ctx, 0x08C55FDCu, {frame.at(0u), self, 0u});
    if (rt.memory().load8(frame.at(0u)) == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    put(rt, frame.at(4u), 0u);
    const u32 base = self + word(rt, word(rt, self) - 0xCu);
    const u32 handler = word(rt, base + 0x84u);
    if (handler == 0u) {
        call(rt, ctx, 0x08B5A9FCu, {});
        ctx.gpr[2] = self;
        return;
    }
    const u32 target = word(rt, word(rt, handler));
    put(rt, frame.at(0x1Cu), word(rt, base + 0x78u));
    put(rt, frame.at(0x18u), 0xFFFFFFFFu);
    put(rt, frame.at(0x20u), 0xFFFFFFFFu);
    put(rt, frame.at(0x14u), 0u);
    const GuestResult result = call_indirect_result(rt, ctx, word(rt, target + 0xCu),
                                                    {handler, frame.at(0x1Cu), frame.at(0x14u), base, frame.at(4u),
                                                     frame.at(8u)});
    put(rt, frame.at(0xCu), result.v0);
    put(rt, frame.at(0x10u), result.v1);
    put(rt, frame.at(0x30u), result.v0);
    put(rt, frame.at(0x34u), result.v1);
    u32 flags = word(rt, frame.at(4u));
    if ((flags & 4u) != 0u) {
        flags |= 4u;
        put(rt, frame.at(4u), flags);
    }
    put(rt, out, word(rt, frame.at(8u)));
    if (flags == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    const u32 object = self + word(rt, word(rt, self) - 0xCu);
    call(rt, ctx, 0x08C54CFCu, {object, word(rt, object + 0x14u) | flags});
    ctx.gpr[2] = self;
}
// The smart-pointer temporary the asm releases: a header word at `address`, whose -0xC word is the shared empty block
// (0x08D74FD0, not released); otherwise 0x08B60804 settles the reference count and 0x08B8542C frees it when it is not positive.
u32 release_block(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 block = word(rt, address);
    const u32 inner = block - 0xCu;
    if (inner == 0x08D74FD0u) {
        ctx.gpr[2] = inner;
        return inner;
    }
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu})) > 0) return ctx.gpr[2];
    ctx.gpr[2] = call(rt, ctx, 0x08B8542Cu, {inner, ctx.gpr[29]});
    return ctx.gpr[2];
}

// 0x0880D054 (a0 = self, a1 = text, a2 = output object, a3 = a limit): parses the text (a leading '/' skipped) into a record at
// sp+4 and a count at sp+8 (a non-negative a3 below it lowers it), hands both to 0x08B79A88 and, when the flag byte at sp+2 is
// set, releases the record through 0x08B35254. v0 = the count.
void func_0880D054(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    const u32 limit_argument = ctx.gpr[7];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = out;
    ctx.gpr[18] = limit_argument;
    ctx.gpr[16] = frame.at(4u);
    const u32 base = word(rt, self + 4u);
    ctx.gpr[17] = base;
    const u32 flag = (static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(text)))) ^ 0x2Fu) < 1u
                         ? 1u
                         : 0u;
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), text + flag, frame.at(1u)});
    const u32 value = call(rt, ctx, 0x089A3348u, {base, frame.at(4u), frame.at(2u), frame.at(8u)});
    ctx.gpr[17] = value;
    release_block(rt, ctx, frame.at(4u));
    u32 limit = word(rt, frame.at(8u));
    if (static_cast<s32>(limit_argument) >= 0 && limit_argument < limit) {
        limit = limit_argument;
        put(rt, frame.at(8u), limit);
    }
    call(rt, ctx, 0x08B79A88u, {out, value, limit});
    if (rt.memory().load8(frame.at(2u)) != 0u) call(rt, ctx, 0x08B35254u, {value});
    ctx.gpr[2] = word(rt, frame.at(8u));
}

// 0x0884308C family (a0 = the output string): a lookup (asserted non-null with a message) and its conversion, the converted
// object transformed into a record at sp+4 through 0x08B85EB8, 0x08A6D2D4 formats it into the output. v0 = the record release.
void format_converted(Runtime &rt, AllegrexContext &ctx, u32 lookup, u32 message, u32 convert, u32 transform) {
    const u32 out = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 local = frame.at(0xCu);
    const u32 record = frame.at(4u);
    const u32 text_local = frame.at(8u);
    ctx.gpr[19] = local;
    ctx.gpr[18] = out;
    ctx.gpr[20] = record;
    ctx.gpr[17] = text_local;
    call(rt, ctx, lookup, {local});
    const u32 holder = word(rt, local);
    ctx.gpr[16] = holder;
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 converted = call(rt, ctx, convert, {holder});
    ctx.gpr[16] = converted;
    call(rt, ctx, 0x08A5B9ECu, {local});
    const u32 value = call(rt, ctx, transform, {converted});
    call(rt, ctx, 0x08B85EB8u, {record, value + 0x20u, frame.at(1u)});
    call(rt, ctx, 0x08A6D2D4u, {text_local, record});
    if (out != text_local) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, text_local)});
    call(rt, ctx, 0x08A5B9ECu, {text_local});
    ctx.gpr[2] = release_block(rt, ctx, record);
}

// 0x0885BFE0 (a0 = self): picks one of the object's word(0)'s sub-structures by its flag bytes at +0x14..+0x18 (0x90, 0x100,
// 0xE4, 0xC8 or 0xAC), then either submits its words to 0x08865A04 (flag +0x17) or takes the count from 0x088D5DA8 over the
// object's +0x7C, clamps it to the structure's first word and submits it to 0x08865B98. v0 = 0.
void func_0885BFE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, self);
    u32 selected = rt.memory().load8(object + 0x18u) != 0u ? object + 0x100u : object + 0x90u;
    if (rt.memory().load8(object + 0x14u) != 0u) {
        if (rt.memory().load8(object + 0x17u) != 0u) {
            selected = object + 0xE4u;
        } else {
            selected = object + 0xC8u;
            if (rt.memory().load8(object + 0x15u) == 0u) selected = object + 0xACu;
        }
    }
    ctx.gpr[16] = selected;
    const u32 limit = rt.memory().load8(object + 0x15u) != 0u ? 0xFFFFFFFFu : word(rt, selected + 0x14u);
    ctx.gpr[18] = limit;
    if (rt.memory().load8(object + 0x17u) != 0u) {
        call(rt, ctx, 0x08865A04u,
             {word(rt, self + 4u), word(rt, selected + 4u), word(rt, selected + 8u), word(rt, selected + 0xCu),
              word(rt, selected + 0x10u), word(rt, selected + 0x18u)});
    } else {
        const u32 count = call(rt, ctx, 0x088D5DA8u, {word(rt, object + 0x7Cu)});
        const u32 structure_limit = word(rt, selected);
        const u32 t2 = word(rt, selected + 0x18u);
        const u32 smaller = static_cast<s32>(count) < static_cast<s32>(structure_limit) ? count : structure_limit;
        const u32 chosen = structure_limit != 0u ? smaller : count;
        call(rt, ctx, 0x08865B98u,
             {word(rt, self + 4u), chosen, word(rt, selected + 4u), word(rt, selected + 8u), word(rt, selected + 0xCu),
              word(rt, selected + 0x10u), t2, limit});
    }
    ctx.gpr[2] = 0u;
}

// 0x0898A220 (a0 = self, a1 = other): with the other object's header word non-zero, resolves it through the handler at
// self+0x1E0 (0x089CBFDC, 0x089BD4DC, 0x089CBF68, 0x08968668) and tail-calls 0x0898A0CC with the result, or 0x0898952C with
// the other object when it is zero. v0 = 0 when the header says there is nothing to do.
void func_0898A220(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = other;
    ctx.gpr[17] = self;
    const u32 handler = word(rt, self + 0x1E0u);
    ctx.gpr[16] = handler;
    if (handler == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39B4u});
    call(rt, ctx, 0x089CBFDCu, {handler, other});
    if (word(rt, word(rt, other) - 0xCu) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 value = call(rt, ctx, 0x089BD4DCu, {self});
    ctx.gpr[19] = value;
    const u32 inner = word(rt, self + 0x1E0u);
    ctx.gpr[16] = inner;
    if (inner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39B4u});
    const u32 first = call(rt, ctx, 0x089CBF68u, {inner});
    const u32 result = call(rt, ctx, 0x08968668u, {value, first, other});
    if (result == 0u) {
        tail_call(ctx, 0x0898952Cu, {self, other});
        return;
    }
    tail_call(ctx, 0x0898A0CCu, {self, result});
}

// 0x089C0538 (a0 = the output string, a1 = source object, a2 = a count, a3 = pointer to the object): with a positive count the
// pointed object's virtual +0x3C (as a flag) is stored at +0x260 of the looked-up object; otherwise that flag is formatted into
// the output. v0 = the last release.
// 0x089D9850 (a0 = destination object, a1 = pointer to the source object): builds a stack wrapper (vtable 0x08CCD9D8, the
// shared reference block, -1 at +0x10, its own address at +0x14) on the source's virtual +8, then runs 0x089D86E8 with the
// destination and the wrapper at sp+4, releasing both reference blocks the way the smart-pointer temporaries are.

// 0x089ECE50 (a0 = self): a destructor - stores the vtable word, runs 0x089EC798, frees the vector at +0x14 through
// 0x08C0A850/0x08A5BC54, releases +0x10 and the two blocks at +8 and +4 and finishes with 0x08B64654.

// 0x089FB168 (a0 = vector of 0x20-byte elements, a1 = new count): shrinks to a1 (0x08C0DBBC) or grows by the difference
// (0x08C0E3B8, a default element at sp); with one element already there and two or more wanted, every new element is a copy of
// the first (0x089641C4 and its +0x10 word). v0 = 1 when nothing was filled and the count was not below 2, else 0.
// 0x08A08A38 (a0 = self): a destructor - two vtable words (+0 and +0x38), 0x089F9B74 on +0x10C, the two lists at +0x100 and
// +0xF4 and +0xE8 through 0x08C12060/0x08BEEA84 and 0x08B35254/0x08A5BC54, then 0x0896BC0C on self.
void func_08A08A38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = self + 0xE8u;
    ctx.gpr[17] = self + 0xF4u;
    ctx.gpr[16] = self;
    put(rt, self, 0x08CBC288u);
    put(rt, self + 0x38u, 0x08CBC4C0u);
    call(rt, ctx, 0x089F9B74u, {self + 0x10Cu});
    call(rt, ctx, 0x08C12060u, {word(rt, self + 0x100u), word(rt, self + 0x104u), frame.at(2u)});
    if (word(rt, self + 0x100u) != 0u) call(rt, ctx, 0x08B35254u, {word(rt, self + 0x100u)});
    const u32 second_vector = self + 0xF4u;
    const u32 first_vector = self + 0xE8u;
    call(rt, ctx, 0x08BEEA84u, {word(rt, second_vector), word(rt, second_vector + 4u), frame.at(1u)});
    const u32 second_base = word(rt, second_vector);
    if (second_base != 0u)
        call(rt, ctx, 0x08A5BC54u, {second_base, (word(rt, second_vector + 8u) - second_base) >> 2 << 2, 4u});
    call(rt, ctx, 0x08BEEA84u, {word(rt, first_vector), word(rt, first_vector + 4u), frame.at(0u)});
    const u32 first_base = word(rt, first_vector);
    if (first_base != 0u)
        call(rt, ctx, 0x08A5BC54u, {first_base, (word(rt, first_vector + 8u) - first_base) >> 2 << 2, 4u});
    ctx.gpr[2] = call(rt, ctx, 0x0896BC0Cu, {self});
}

// 0x08A7531C (a0 = output string, a1 = the object): builds the string at sp+4 through 0x08AAB5DC/0x08AABA8C with three literals,
// the text of 0x08A5D454 and the word at +4 of the object at +0x14, then 0x08B88F44 writes it to the output. v0 = the output.
void func_08A7531C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 local = frame.at(4u);
    ctx.gpr[16] = local;
    ctx.gpr[18] = out;
    ctx.gpr[17] = object;
    call(rt, ctx, 0x08AAB5DCu, {local});
    u32 text = call(rt, ctx, 0x08AABA8Cu, {local, 0x08CB0D5Cu});
    text = call(rt, ctx, 0x08AABA8Cu, {text, call(rt, ctx, 0x08A5D454u, {object})});
    text = call(rt, ctx, 0x08AABA8Cu, {text, 0x08CB0D70u});
    text = call(rt, ctx, 0x08AABA8Cu, {text, 0x08CB0D74u});
    text = call(rt, ctx, 0x08AABA8Cu, {text, word(rt, word(rt, object + 0x14u) + 4u)});
    text = call(rt, ctx, 0x08AABA8Cu, {text, 0x08CB0D78u});
    call(rt, ctx, 0x08B88F44u, {out, call(rt, ctx, 0x08AAB540u, {local})});
    release_block(rt, ctx, local);
    ctx.gpr[2] = out;
}

// 0x08B27780 (a0 = self): clamps three settings of the objects at +0x40, +0x48 and +0x50 (0x08B26EB8 with their ranges), stores
// the clamped halves at +0x44, +0x4C and +0x54 (the last from the byte table at 0x08CE0C76 indexed by its +0x50 object), and
// runs 0x08B276BC/0x08B276B4. v0 = 1 when the last setting was below 4.
// 0x08B2C62C (a0 = tree, a1 = node, a2 = hint, a3 = an alignment mask): inserts the node below its masked parent (or
// rebalances through 0x08B2C42C/0x08B2C504 when the gap is not below the tree's +0x54 limit), links it and rebalances again
// when the inserted node's +4 is not below the limit. v0 = the inserted node.

// 0x08BBF9B8 (a0 = first source record, a1 = one past the last, a2 = destination): copies the 0x14-byte records (0x08B9432C)
// and each one's inner 0x10-byte vector (allocated with 0x08B35DA0 and filled by 0x08BBEC68, words at +4/+8/+0xC) plus the word
// at +0x10. v0 = the destination past the last copied record.
// 0x08BED6FC (a0 = vector of 0x30-byte elements, a1 = position, a2 = the element source): appends (0x08BED390 when the vector
// is full or the position is the end) or copies the 0x30 bytes (vtable 0x08CCDE00, words +4/+8, floats +0xC..+0x2C) at the end
// and advances it. v0 = the element at the position's index.

// 0x08BE81A0 (a0 = self, a1 = index): builds the 0x50-byte default element at sp (vtable 0x08CCFF88, eight 1.0 floats, -1 at
// +0x30 and +0x50) and inserts it at the index into the vector at +0xA8 (0x08BE6244) or past the end (0x08BE7DCC), then
// destroys the temporary (0x089D6684). v0 = that destroy call's result.
// 0x08BF8ECC (a0 = begin of 8-byte elements, a1 = end): an insertion step - each element whose word's +0x134 (and then its +4)
// compares below the first element's is moved to the front by shifting [begin, element) up one slot; otherwise 0x08BF8E6C
// looks at it.

// 0x08C2C1FC (a0 = container, a1 = destination, a2 = source): moves the (end - source)/12 elements from source to destination
// (0x08A6A23C plus the word at +4 and the object at +8 when the addresses differ), lets 0x08C2B500 fix what is left and
// shrinks the container's end by (source - destination) with the low two bits cleared. v0 = the entry destination.
// 0x0883365C (a0 = the output string): converts a looked-up object (0x088297A8/0x088294D0, asserted non-null) through
// 0x0885EE70, asks 0x08891608 whether there is anything, allocates 0x08892138's size with 0x08B2B5E0, fills it with
// 0x08B79A88 from 0x088915E0's count and formats it with 0x08A44D9C. v0 = 0 when there was nothing, else the release.

// 0x0885FD54 (a0 = self, a1 unused, a2 = third, a3 = fourth): sets the byte at +0x3FDB of the object at word(a0), takes the
// handle at its +0x5AC, waits out two conditions (0x0885EBDC/0x0885E9B8, sleeping 0x3E80 between), then hands the handle and a2/a3
// to 0x088D5ED0 and finishes it with 0x088DE9CC/0x088D9FB4/0x088D8154. v0 = 1.

// 0x088AAE08 (a0 = self, a1 = second, a2 = third): fills a 0x20-byte local (0x088A6088, 0x088A0DD4), runs 0x08BAC9EC on the
// object's +0x4C, and when it produced an object decrements the counters at +4 and +8 (release at one) and runs its virtuals +8
// and +0xC. v0 = 0 without an object, 1 when a counter was not the last, else the last virtual's result.
// 0x088D3DA0 (a0 = self, a1 = pointer to the object, a2 = second, a3 = kind): 0x088D2F9C first; a kind other than 10/13/14 also
// needs 0x088D4E08 to accept the object. Then 0x088D61E0's float is stored at +0x18, scaled by 0x088D2BC8 into 0x088D390C
// (flags 2 for 13, 1 for 14, 6 otherwise, with f13 = 0) and 0x088D4E34's word stored at +0x1C with the flag byte +0x20 set.
void func_088D3DA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 kind = ctx.gpr[7];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = holder;
    ctx.gpr[20] = second;
    ctx.gpr[18] = kind;
    ctx.gpr[17] = self;
    call(rt, ctx, 0x088D2F9Cu, {self});
    const u32 object = word(rt, holder);
    const u32 inner = word(rt, object + 0x61Cu);
    ctx.gpr[19] = inner;
    ctx.gpr[16] = object + 4u;
    bool convert = true;
    if (kind != 0xAu && kind != 0xDu && kind != 0xEu) convert = call(rt, ctx, 0x088D4E08u, {object + 4u}) != 0u;
    if (!convert) {
        ctx.gpr[2] = self;
        return;
    }
    const float value = call_floats(rt, ctx, 0x088D61E0u, {object + 4u, second}, {}).f0;
    put_float(rt, self + 0x18u, value);
    const float angle = call_floats(rt, ctx, 0x088D2BC8u, {inner}, {value}).f0;
    u32 flags = 1u;
    if (kind == 0xDu) flags = 2u;
    else if (kind != 0xEu) flags = 6u;
    call_floats(rt, ctx, 0x088D390Cu, {inner, self, self + 0xCu, flags}, {angle, 0.0f});
    const u32 result = call(rt, ctx, 0x088D4E34u, {object + 4u, second});
    put8(rt, self + 0x20u, 1u);
    put(rt, self + 0x1Cu, result);
    ctx.gpr[2] = self;
}

// 0x088E7894 (a0 = self, a1..a3 = arguments, f12 = scale): the objects at word(a0)+0x148/+0x14C drive one virtual (vtable +0x30,
// eight arguments), then +0x44 with the scale times 0x3E8E38E4, then +0x1C with 1, and the function ends in a tail call of the
// first handle's +0x10 with 0.

// 0x0897BFEC (a0 unused, a1 unused, a2 = count, a3 = argument): a lookup at 0x08978000 into sp+8; with a positive count also
// one at 0x08A5D2D4 into sp+4, whose object's virtual +0xC4 receives a temporary string (0x089788A8/0x08A85B1C); then
// 0x0897BC08 on the first lookup and its release. v0 = that release.
// 0x08B0066C (a0 = self, a1 = destination, a2 = index, a3 = second, t0 = argument): builds two stack keys (0x08C45F08 with a3
// and t0, 0x08C46184) and walks the records of the block at word(self+0xD8) + index*0x200 + 0x80: each one's key
// (0x08C45D84/0x08C45F1C) is written into the destination at 0x10-byte steps (0x08C46A5C). v0 = the record's 4-bit count.

// 0x08B0E5B8 (a0 = self, a1 = other): 0x08B34888 on self+4 first; when the other's +0x58 is self, bumps self+0x108 and runs
// 0x08B1FBAC/0x08B1FC48 on the list at self+0xF8 with the other's +0x5C (copying its +0xEC) and, if the list emptied, the
// byte at +0x68 as a flag around the virtual +0x24. Then 0x08B34B24, and the flag chooses 0x08B0DEB0 as a tail call.
void func_08B0E5B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 sub = self + 4u;
    const u32 flag_address = other + 0x5Cu;
    const u32 list = self + 0xF8u;
    ctx.gpr[20] = sub;
    ctx.gpr[17] = other;
    ctx.gpr[16] = self;
    ctx.gpr[19] = flag_address;
    ctx.gpr[18] = list;
    call(rt, ctx, 0x08B34888u, {sub});
    u32 value = 0u;
    if (self == word(rt, other + 0x58u)) {
        put(rt, self + 0x108u, word(rt, self + 0x108u) + 1u);
        if (call(rt, ctx, 0x08B1FBACu, {list, flag_address}) != 0u) {
            const u32 extra = word(rt, other + 0xECu);
            if (extra != 0u) put(rt, self + 0xECu, extra);
            call(rt, ctx, 0x08B1FC48u, {list, flag_address});
            if (word(rt, list + 4u) == 0u) {
                const u32 vtable = word(rt, self);
                value = rt.memory().load8(self + 0x68u);
                put8(rt, self + 0x68u, 0u);
                call_indirect_result(rt, ctx, word(rt, vtable + 0x24u), {self});
            }
        }
    }
    ctx.gpr[17] = value;
    call(rt, ctx, 0x08B34B24u, {sub});
    if (value != 0u) {
        tail_call(ctx, 0x08B0DEB0u, {self});
        return;
    }
}
// 0x08B15268 (a0 = output struct, a1, a2 = holder): fills the 0x3C-byte block at sp+0x40 (0x08B1837C with 0x08B17A98's word
// of the holder's +0x50), copies it to sp and from there into the output (0x3C bytes in 4-word rows plus a last 3), finishing
// the handle with 0x08B17AA8. v0 = the output.

// 0x08BD74BC and 0x08BD787C (same body, a0 = self): takes a counted object at +0x2C (bumping the count at its +4), clears the
// byte at +0xE and the word at +0x1C, calls the array entry at (table + size/2, with one indirection through +0x30 when the size
// is odd) and then releases the counted object (counts at +4 and +8, each release at one calls its virtual +8 or +0xC).
// v0 = the word at +0x1C after the call.
void release_counted_and_dispatch(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 counted = word(rt, self + 0x2Cu);
    ctx.gpr[17] = counted;
    ctx.gpr[16] = self;
    if (counted != 0u) put(rt, counted + 4u, word(rt, counted + 4u) + 1u);
    const u32 size = word(rt, self + 0x34u);
    put8(rt, self + 0xEu, 0u);
    put(rt, self + 0x1Cu, 0u);
    const u32 at = word(rt, self + 0x28u) + sra(size, 1);
    const u32 target = (size & 1u) != 0u ? word(rt, word(rt, at) + word(rt, self + 0x30u)) : word(rt, self + 0x30u);
    call_indirect_result(rt, ctx, target, {at});
    const u32 result = word(rt, self + 0x1Cu);
    ctx.gpr[16] = result;
    if (counted != 0u) {
        const bool last = word(rt, counted + 4u) == 1u;
        put(rt, counted + 4u, word(rt, counted + 4u) - 1u);
        if (last) {
            call_indirect_result(rt, ctx, word(rt, word(rt, counted) + 8u), {counted});
            const bool last_second = word(rt, counted + 8u) == 1u;
            put(rt, counted + 8u, word(rt, counted + 8u) - 1u);
            if (last_second) call_indirect_result(rt, ctx, word(rt, word(rt, counted) + 0xCu), {counted});
        }
    }
    ctx.gpr[2] = result;
}

void func_08BD74BC(Runtime &rt, AllegrexContext &ctx) { release_counted_and_dispatch(rt, ctx); }
void func_08BD787C(Runtime &rt, AllegrexContext &ctx) { release_counted_and_dispatch(rt, ctx); }
} // namespace
} // namespace new_batch_main_19
} // namespace gt::game

#define NEW19(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_19::name, gt::ReturnKind::kind)
NEW19(0x0880D054u, func_0880D054, Word);
NEW19(0x0885BFE0u, func_0885BFE0, Word);
NEW19(0x088A2274u, func_088A2274, Word);
NEW19(0x088D3DA0u, func_088D3DA0, Word);
NEW19(0x08955AD0u, func_08955AD0, Word);
NEW19(0x0898A220u, func_0898A220, Word);
NEW19(0x08A08A38u, func_08A08A38, Word);
NEW19(0x08A7531Cu, func_08A7531C, Word);
NEW19(0x08ABAFCCu, func_08ABAFCC, Word);
NEW19(0x08B0E5B8u, func_08B0E5B8, Word);
NEW19(0x08B44B24u, func_08B44B24, Word);
NEW19(0x08B79E68u, func_08B79E68, Word);
NEW19(0x08BD74BCu, func_08BD74BC, Word);
NEW19(0x08BD787Cu, func_08BD787C, Word);
NEW19(0x08C581D0u, func_08C581D0, Word);
