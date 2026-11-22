// New batch, main session 543: 24 functions that no gate ever compared, written as structured C by hand from `tools.disasm`.
// Most of them are the same few shapes: a string built through a stack temporary (lookup, convert, format, assign, release),
// the two-indexed-virtual lookup family, and the "count decides between store and format" pair. The class/argument shapes that
// make them runnable live in /tmp/claude-1000/shapes_main543.txt (config/synth_shapes.txt format).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_543 {
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

// A `jalr` call, keeping v1 and f0 as well (a caller that reads the callee's v1 or f0 after it).
GuestResult call_indirect_result(Runtime &rt, AllegrexContext &ctx, u32 target,
                                 std::initializer_list<u32> arguments = {}) {
    return call_indirect(rt, ctx, target, arguments, {});
}

// The same with float arguments ($f12-$f15 in order).
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// `sra` (arithmetic) as the asm spells it; >> on an unsigned local would be a logical shift.
u32 sra(u32 value, unsigned bits) { return static_cast<u32>(static_cast<s32>(value) >> bits); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

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

// The two indexed virtual lookups these functions share (0x08BFE018 and its neighbours, 0x08C07510, 0x08C26328, 0x08BF0784):
// a size/table pair at self+0x18/+0x14 and a second at self+0x20/+0x1C. An odd size means one more indirection: the table word
// at the halved index is an object whose word at the table offset is the target. Even: the table value is the target.
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

// The reference block at `address`: the shared empty one (0x08D74FD0 + 0xC words) is left alone, otherwise the count at -4 is
// decremented (0x08B60804) and, once it is not positive, the block released. v0 ends as the asm's does on every exit.
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

// --- the two indexed lookups forward to a virtual ---

// 0x08BFE018 (a0 = the lookup object, a1 = destination, a2 = pointer to the source object, t0 = the conversion argument):
// converts t0 (0x08AA2A30), walks the two indexed lookups of a0 on the source object's first word, calls the second target with
// the conversion's result and hands that to 0x08B8C564 with a stack temporary, which is assigned to the destination.

// 0x08C07510 (a0 = the lookup object, a1 unused, a2 = pointer to the source object, t0 = a three-word argument): converts the
// three words at t0 (0x08AA2AFC), then tail-calls the second indexed target with them in a1-a3.

// 0x08C26328 (a0 = the lookup object, a1 unused, a2 = pointer to the source object, t0 = a two-word argument): fills two stack
// temporaries from t0's two words (0x08A76ABC), calls the second indexed target with them, then releases both temporaries.

// 0x08BF0784 (a0 = the lookup object, a1 = destination, a2 = pointer to the source object): the two indexed lookups over the
// source object's first word; the second target's result is formatted through 0x0896418C/0x08BEFE64 (with its word at +0x10)
// and assigned to the destination.

// --- a string from one or two converted sources, through a stack temporary ---

// 0x088314BC (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): a lookup into a stack
// temporary (0x088297A8), its conversion (0x088294D0 then 0x0885EDEC), then -1 when the count is not positive, else the source
// object's virtual +0x3C; 0x088DF324 combines both into a string that is assigned to the output.

// 0x0884DEA0 (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): a lookup (0x0884D660) and
// its conversion (0x0884D3D8), then the source object's virtual +0x3C is stored at +0x88 of the conversion and the record is
// formatted; a non-positive count formats the value already stored there instead.

// 0x08850B84 (a0 = the output string, a1 = the source object): converts the source (0x088547C4/0x08854678), reads a count and
// that many pairs of floats from 0x088A5B3C into a 0x804-byte buffer (the count first), formats the buffer with 0x08A44D9C and
// assigns it.

// 0x08851670 (a0 = the output string, a1 = the first source object, a2 unused, a3 = the second source object): converts both
// sources (0x088547C4/0x08854678), combines them with 0x088A78B4 and assigns the formatted result.

// 0x0884308C / 0x08855B74 (a0 = the output string): a lookup (0x088418B4 / 0x088547C4, asserted non-null with `message`) and
// its conversion (`convert`), the converted object transformed into a record at sp+4 through 0x08B85EB8, which 0x08A6D2D4
// formats into the output; the record block is released last.
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

// 0x08A2D840 (a0 = the output string): a lookup (0x08A2D338) and two conversions (0x08A2D1B8/0x08B09658) build a record at sp+8
// (0x08B85EB8) and a string at sp+0xC (0x08A6D2D4), which is assigned to the output; the record block is released.

// 0x08AB6564 (a0 = the output string): a lookup (0x08AB61DC), its byte (0x08AB5EFC), a record built by 0x08C00AD4 from that
// byte and a one-byte range at sp+1, then 0x08A6D2D4 formats the record into the output; the record block is released.

// --- a count decides between storing a flag and formatting it ---

// 0x089C0270 (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): for a positive count the
// source object's virtual +0x3C (as a flag) is stored at +0x243 of the looked-up object; otherwise that flag is formatted into
// the output.

// 0x089C0538 (a0 = the output string, a1 = source object, a2 = a count, a3 = pointer to the object): with a positive count the
// pointed object's virtual +0x3C (as a flag) is stored at +0x260 of the looked-up object; otherwise that flag is formatted into
// the output.

// 0x08A395B4 (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): for a positive count the
// source object's virtual +0x54 (a float) is handed to 0x08A38D28 with the looked-up object; otherwise the float at +0x20 of the
// looked-up object is formatted into the output.

// --- object work ---

// 0x08891984 (a0 = self): fills +0x24/+0x28 from two calls over the object at +0xC, submits a whole record (the indexed entry at
// +0x44, the words at +4..+0x18, t2 = 1) to 0x0888FBD8, runs two more helpers on the same entry and 0x0889605C/0x088963C4 on
// +0x24, then writes the command word into the indexed slot at +0x50 and sets the byte at +0x6B.

// 0x088BEB4C (a0 = self, f12 = scale): v0 of 0x088BE9C4 (with the frame at a1) chooses; zero interpolates two angles of the
// object at +0x14 by the scale and adds the scale in 0x088BB5C4, otherwise the float at the frame is clamped to [-1, 1], scaled
// by the float at +0x28C of that object, offset by the scale and floored at 0.1. f0 = the angle.

// 0x08A63A6C (a0 = self): a destructor - stores the vtable word, then for as long as the vector at +0x24 is not empty takes its
// last element (a1 = self for both virtuals), runs the element's virtuals +0xC and +4 and shrinks the vector; then releases
// +0x3C, +0x54 and +0x4C, frees the vector storage (0x08A5BC54 when the base is not null) and finishes with 0x08A780B0 on self.
void func_08A63A6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 vector = self + 0x24u;
    ctx.gpr[18] = self;
    ctx.gpr[17] = vector;
    put(rt, self, 0x08CCB980u);
    for (;;) {
        const u32 end = word(rt, vector + 4u);
        if (sra(end - word(rt, vector), 2) == 0u) break;
        const u32 element = word(rt, end - 4u);
        ctx.gpr[16] = element;
        call_indirect_result(rt, ctx, word(rt, word(rt, element) + 0xCu), {element, self});
        call_indirect_result(rt, ctx, word(rt, word(rt, element) + 4u), {element, self});
        put(rt, vector + 4u, word(rt, vector + 4u) - 4u);
    }
    call(rt, ctx, 0x08A72B94u, {word(rt, self + 0x3Cu), self});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x54u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x4Cu});
    const u32 begin = word(rt, self + 0x24u);
    call(rt, ctx, 0x08C1FBC4u, {begin, word(rt, vector + 4u), frame.at(0u)});
    const u32 base = word(rt, self + 0x24u);
    if (base != 0u) call(rt, ctx, 0x08A5BC54u, {base, (word(rt, vector + 8u) - base) >> 2 << 2, 4u});
    if (word(rt, self + 0x1Cu) != 0u) call(rt, ctx, 0x08A5B930u, {word(rt, self + 0x1Cu)});
    ctx.gpr[2] = call(rt, ctx, 0x08A780B0u, {self});
}

// 0x08A6FD00 (a0 = self, a1 = offset, a2 = pointer to the source object): reads a character through the source object's virtual
// +0xC into a temporary, and when the temporary's header says so selects the table at self+0x14 (0x08B88FF8 settles a lazy
// index) and stores the character at table+offset; the temporary block is released.

// 0x08A8B4E4 (a0 = self, a1 = the output string, a2 unused, a3 = pointer to the object): takes the object's virtual +0x58 twice
// (its two words through 0x08B71EC8), combines the two floats (0x08B6728C, 0x08B70F28) and builds the string in a temporary
// that is assigned to a1.

// 0x08AA3AF8 (a0 = the output string, a2 = pointer to the holder): three objects at holder+0/+4/+8 each answer float +0x54
// (a, b, c); the result a + c*(b - a) is formatted into the output.

// 0x08AC35F0 (a0 = source, a1 = output record, a2 = a value): clears the record, and when the source's first word is negative
// or the value is below its +8 field prepares the source (0x08AC2D10, 0x08AC3570); then 0x08C7A858 writes into the record and a
// positive answer bumps the counter at +8. v0 = 1 when it was written, else 0.

// 0x08C0CA90 (a0 = container, a1 = record buffer): copies (end - (a1+0x40)) / 0x40 records of 0x40 bytes from a1+0x40 down to
// a1 (five floats, a word at +8 and two objects via 0x089FB570), then shrinks the container's end by one record and destroys
// that record through its vtable's first virtual. v0 = a1, the buffer.

// 0x089ECE50 (a0 = self): a destructor - stores the vtable word, runs 0x089EC798, frees the vector at +0x14 through
// 0x08C0A850/0x08A5BC54, releases +0x10 and the two blocks at +8 and +4 and finishes with 0x08B64654.
} // namespace
} // namespace new_batch_main_543
} // namespace gt::game

#define NEW543(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_543::name, gt::ReturnKind::kind)
NEW543(0x08A63A6Cu, func_08A63A6C, Word);
