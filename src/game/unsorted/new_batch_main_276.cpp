// ---- from main-session batch 187 (draft dc04ab90e547c643078b40e55c472ed412f578fb) ----
// New batch main 187 (main session): 29 functions written as structured C from `tools.disasm`, started from the old
// drafts (new_batch_main_14 @ 71efdd7 and new_batch_main_15 @ e242d11) and re-read against the asm line by line.
// DRAFT: none has been through the differential gate yet (the main session writes, the verifier gates).
//
// Groups:
//   * `invoke_member` - 18 instances of one 47-instruction shape: calling through a pointer-to-member function. The
//     invoker (a0) holds two of them: the getter's {function/call-offset +0x14, tagged delta +0x18} and the setter's
//     {+0x24, +0x28}. The getter is called on *a1; the setter's target is resolved on the getter's result; the value
//     (a2) goes through a small conversion function; the setter is the tail call. Nine members convert to an int
//     (0x08B8C590, or 0x08BE9068 for two of them), nine to a float (0x08B8C5D4, moved to $f12).
//   * `counted_range` (0x0881888C, whose twin 0x08818948 sums the ends), `destroy_member`-shaped one-offs
//     (0x089B8DA0, 0x089C946C), `divide_through`'s remainder instance (0x08AB1E50) and the rest, each noted above
//     its function.
//
// Rules followed here: a `jal` / `jalr` delay slot runs BEFORE the call (arguments are computed and stores happen
// before it); a `jalr $v0` hands the callee its own address in v0 (call_indirect), a `jalr $t6` does not (call);
// every argument register the original sets before a call is passed (a0-a3, t0-t3); a `bnel` delay slot runs only
// when the branch is taken, an ordinary branch's on both paths. Callee-saved registers are never written here: the
// original restores s0-s2 (and fp where it saves it) before returning or tail calling, so leaving them alone is the
// same end state. v0 is set where the asm leaves it, on every exit path. The input shapes these functions need are
// in /tmp/claude-1000/shapes_main187.txt (the verifier's synthetic cases read them).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_187 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using GuestResult = gt::GuestResult;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; v0 afterwards is the callee's, as after the original's `jal`. The callee starts from ctx, so a
// register this call does not pass keeps whatever the decompiled caller last put there, like the original's.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with float arguments and the callee's v0 / v1 / f0 left in the context (a 64-bit result pair, a float).
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                  std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v1, 0(object); lw v0, offset(v1); jalr v0`: v1 holds the vtable and v0 the slot's address when the callee starts.
u32 call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + offset);
    return callf(rt, ctx, ctx.gpr[2], arguments, {}).v0;
}
// `sra rd, rs, 1` on the tagged deltas of the pointer-to-member words.
u32 half(u32 value) { return static_cast<u32>(static_cast<s32>(value) >> 1); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 load8(Runtime &rt, u32 address) { return rt.memory().load8(address); }

// `divu` / `div` as the Allegrex leaves them: a zero divisor answers quotient 0xFFFFFFFF and the dividend as the
// remainder (the divider's own answer, hot_batch_121), 0x80000000 / -1 has no trap.
u32 unsigned_remainder(u32 dividend, u32 divisor) {
    if (divisor == 0u) return dividend;
    return dividend % divisor;
}

// -------------------------------------------------------------------------------------------------------------------------
// Pointer-to-member invocation (18 members below).
// The dispatch of one pointer-to-member word {base, tag}: the adjusted object is `pointer + (tag >> 1)`; with bit 0
// of the tag set the target is the vtable slot `base` of that object's class, otherwise `base` is the function
// address itself. The asm keeps v1 = tag & 1 and v0 = the target (the callee of a `jalr $v0` starts with v0 = its own
// address); both are set here the same way.
struct MemberTarget {
    u32 function;
    u32 object;
};
MemberTarget resolve_member(Runtime &rt, u32 pointer, u32 base, u32 tag) {
    const u32 object = pointer + half(tag);
    if ((tag & 1u) == 0u) return {base, object};
    return {word(rt, word(rt, object) + base), object};
}
void invoke_member(Runtime &rt, AllegrexContext &ctx, u32 convert, bool float_value) {
    const u32 invoker = ctx.gpr[4];
    const u32 index = word(rt, ctx.gpr[5]);  // a1 points at the index the getter takes
    const u32 value_argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 getter_tag = word(rt, invoker + 0x18u);
    const u32 getter_base = word(rt, invoker + 0x14u);
    const MemberTarget getter = resolve_member(rt, index, getter_base, getter_tag);
    ctx.gpr[3] = getter_tag & 1u;  // v1 as the `andi` leaves it
    ctx.gpr[2] = getter.function;
    const u32 result = call_indirect(rt, ctx, getter.function, {getter.object, index, getter_base, getter_tag});
    // The setter is resolved on the getter's result, before the conversion call (it reads memory the call may change).
    // The v0 / v1 the asm leaves here are not modelled: the conversion `jal` below overwrites both.
    const u32 setter_tag = word(rt, invoker + 0x28u);
    const u32 setter_base = word(rt, invoker + 0x24u);
    const MemberTarget setter = resolve_member(rt, result, setter_base, setter_tag);
    if (float_value) {
        const GuestResult converted = callf(rt, ctx, convert, {value_argument}, {});
        ctx.gpr[25] = setter.function;  // $t9, as the `jr $t9` correctness of the tail call expects
        ctx.fpr[12] = converted.f0;     // mov.s $f12, $f0
        tail_call(ctx, setter.function, {setter.object});
    } else {
        const u32 converted = call(rt, ctx, convert, {value_argument});
        ctx.gpr[25] = setter.function;
        tail_call(ctx, setter.function, {setter.object, converted});
    }
}


// -------------------------------------------------------------------------------------------------------------------------
// 0x08C56A90 (a0 = object, a1 = a key, a2 = the value): clears the word at +4, then 0x08C55FDC(&byte, object, 1)
// decides. When the byte is set, the object at [object + vtable[-0xC] + 0x78] has its virtual +0x20 called with
// (it, key, value) and the answer stored at +4; when the answer is not the value, the same adjusted object's +0x14
// word (with bit 1 and bit 2 set) goes to 0x08C54CFC. v0 = the object on every path.
void func_08C56A90(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self + 4u, 0u);
    call(rt, ctx, 0x08C55FDCu, {frame.at(0u), self, 1u});
    if (rt.memory().load8(frame.at(0u)) != 0u) {
        const u32 adjusted = self + word(rt, word(rt, self) - 0xCu);
        const u32 object = word(rt, adjusted + 0x78u);
        const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x20u), {object, key, value});
        put(rt, self + 4u, answer);  // stored in the `beq` delay slot, so on both paths below
        if (value != answer) {
            const u32 again = self + word(rt, word(rt, self) - 0xCu);
            call(rt, ctx, 0x08C54CFCu, {again, word(rt, again + 0x14u) | 6u});
        }
    }
    ctx.gpr[2] = self;
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08C72CF8 (a0 = destination, a1 = a pointer to the object pointer, a2 = the 8-word source): the source's eight
// words are copied to sp, the object's virtual +0x14 is called with (sp + 0x20, a1, sp, a3, t0-t3 = the copied
// words +4..+0x10), and the eight words it leaves at sp + 0x20 go to the destination. v0 = the destination.

// -------------------------------------------------------------------------------------------------------------------------
// A counted-range description (two instances: 0x0881888C subtracts the range's ends, 0x08818948 adds them; a0 =
// destination, a1 = a count, a2 = a pointer to the holder pointer): with a positive count the holder (reported through
// 0x08A5BB7C(0x08C98D34) when null) answers the range's end through its virtual +0x3C and the start through
// 0x08817924 (a constant-argument stub, so its a0 is dead), and 0x08A85B1C(cell, end-start) describes it; otherwise
// 0x08A76AAC(cell, 0x08CF6ED8) builds an empty description. 0x08A5BAD8 assigns it to a0 unless it is its own frame
// cell and 0x08A5B9EC releases it. v0 = the last call's, as the asm leaves it.
void counted_range(Runtime &rt, AllegrexContext &ctx, bool sum) {
    const u32 out = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 holder_ref = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const u32 cell = count > 0 ? frame.at(4u) : frame.at(0u);
    if (count > 0) {
        const u32 object = word(rt, holder_ref);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
        const u32 end = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
        const u32 begin = call(rt, ctx, 0x08817924u);
        call(rt, ctx, 0x08A85B1Cu, {cell, sum ? end + begin : end - begin});
    } else {
        call(rt, ctx, 0x08A76AACu, {cell, 0x08CF6ED8u});
    }
    if (out != cell) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, cell)});
    call(rt, ctx, 0x08A5B9ECu, {cell});
}
void func_0881888C(Runtime &rt, AllegrexContext &ctx) { counted_range(rt, ctx, false); }

// -------------------------------------------------------------------------------------------------------------------------
// 0x0888CFCC (a0 = object with a table at +4 and a reporter at +0x18, a1 = a key, a2 = a flag, a3 = a source, t0 = an
// optional text holder): a descriptor at sp (kind 2, the key, the flag as a byte, the 64-bit result at +0x10 / +0x14)
// goes through 0x088A5EE0(a3 + 8, key, flag, a3); a non-null t0 has 0x08896FB8(t0 + 0x20) copied into the descriptor's
// 0x80-byte text (else the byte at sp + 0x18 is cleared); 0x088936A4(object[+0x18], &descriptor) reports it, and the
// object 0x088DA588(object[+4], key) answers (when not null) has its virtual +0x10 called with (it, 0).
void func_0888CFCC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] != 0u ? 1u : 0u;  // `sltu $a2, $zero, $a2`
    const u32 third = ctx.gpr[7];
    const u32 text_holder = ctx.gpr[8];
    GuestFrame frame(ctx, 0xB0u);
    put(rt, frame.at(0u), 2u);
    put(rt, frame.at(8u), key);
    put8(rt, frame.at(0xCu), flag);
    const GuestResult info = callf(rt, ctx, 0x088A5EE0u, {third + 8u, key, flag, third}, {});
    put(rt, frame.at(0x10u), info.v0);
    put(rt, frame.at(0x14u), info.v1);
    if (text_holder != 0u) {
        const u32 text = call(rt, ctx, 0x08896FB8u, {text_holder + 0x20u});
        call(rt, ctx, 0x08B7C830u, {frame.at(0x18u), text, 0x80u});
    } else {
        put8(rt, frame.at(0x18u), 0u);
    }
    call(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0u)});
    const u32 object = call(rt, ctx, 0x088DA588u, {word(rt, self + 4u), key});
    if (object != 0u) {
        // `lw v0, 0(object); lw v1, 0x10(v0); jalr v1`: the callee starts with v0 = the vtable, not with the slot.
        const u32 table = word(rt, object);
        ctx.gpr[2] = table;
        ctx.gpr[3] = word(rt, table + 0x10u);
        call(rt, ctx, ctx.gpr[3], {object, 0u});
    }
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x0892A2C0 (a0 = a byte vector {begin, end}, a1 = a {count, source} pair): makes room for `count` two-byte records
// through 0x08BDDBA0 (append at begin + 2*count) when the vector has that many, else through 0x08BDE864 (grow by
// count - room, with a zero pair on the stack), then copies `count` two-byte records from the source to the vector's
// new begin. v0 = 0.
void func_0892A2C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 pair = ctx.gpr[5];
    const u32 begin = word(rt, self);
    const u32 end = word(rt, self + 4u);
    const u32 count = word(rt, pair);
    const u32 room = half(end - begin);  // the record count the vector holds: (end - begin) >> 1
    GuestFrame frame(ctx, 0x20u);
    put16(rt, frame.at(0u), 0u);  // the filler record pair, stored in the `beqz` delay slot on both paths
    if (count < room) {
        call(rt, ctx, 0x08BDDBA0u, {self, begin + count * 2u, end, end});
    } else {
        call(rt, ctx, 0x08BDE864u, {self, end, count - room, frame.at(0u)});
    }
    if (word(rt, pair) != 0u) {
        u32 position = 0u;
        u32 written = 0u;
        do {
            written += 1u;
            const u32 from = word(rt, pair + 4u) + position;
            const u32 to = word(rt, self) + position;
            put8(rt, to, load8(rt, from));
            put8(rt, to + 1u, load8(rt, from + 1u));
            position += 2u;
        } while (written < word(rt, pair));
    }
    ctx.gpr[2] = 0u;
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x089B8DA0 (a0 = self, a1 = a flag, a2 = an object, a3 = an extra value): 0x08A5BA28(object) takes a reference;
// the object's +0x48 member (read after that call) is released through 0x089B8BB4(member, flag, object); when the flag
// is set, 0x089BA6CC(object, self), 0x089BAA60(object, flag) and 0x089BA6CC(object, 0); 0x089B8C6C(self, object, extra);
// the virtual +0x190 of self's +4 owner with (owner, object); then the tail call 0x08A5B970(object).

// -------------------------------------------------------------------------------------------------------------------------
// 0x089C946C (a0 = object): under the global lock 0x08CF4E98, walks the ring at +0x48 from +0x54 up to its sentinel
// +0x4C: each node's +0x14 (reported through 0x08A5BB7C(0x08CA638C) when null) is released through 0x089F0AFC and the
// next node comes back from 0x08B5F100; then 0x08C01C34(ring, ring[+8]) empties it, the four ring words are reset and
// the lock is released (tail call 0x08B3457C).

// -------------------------------------------------------------------------------------------------------------------------
// 0x08A13844 (a0 = the object): constructor-shaped - store the vtable 0x08CD0418, 0x08ACF184(word(0x08CFFAAC), 0), set
// the byte at +0x41, then destruct the members at +0x10, +0x3C, +0x240, +0x200, +0x178, +0x140, +0xA0, +0x5C and +0x10
// again (0x08A12AE0 and 0x089C16C8 take the object itself), and tail-call 0x08A5BC54(object, 0x7C0, 4).

// -------------------------------------------------------------------------------------------------------------------------
// Divide a field of a0 by a virtual getter's answer (a3 = the holder pointer, read twice around the calls as the asm
// does): the object at *a3 (reported through 0x08A5BB7C(text) when null) answers the divisor through its virtual
// `vtable_offset`; the dividend is the byte at +0x14 of a0, loaded between the two calls. The remainder (mfhi of
// `divu`) is built on the stack by `builder` (0x08A85B1C), assigned to a1 unless a1 is that very cell, and released.
void divide_through(Runtime &rt, AllegrexContext &ctx, u32 vtable_offset, u32 guard_text, u32 builder) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    u32 holder = word(rt, ctx.gpr[7]);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    call_slot(rt, ctx, holder, vtable_offset, {holder});
    holder = word(rt, ctx.gpr[7]);
    const u32 dividend = load8(rt, self + 0x14u);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    const u32 divisor = call_slot(rt, ctx, holder, vtable_offset, {holder});
    call(rt, ctx, builder, {frame.at(0u), unsigned_remainder(dividend, divisor)});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_08AB1E50(Runtime &rt, AllegrexContext &ctx) { divide_through(rt, ctx, 0x44u, 0x08CB3AF0u, 0x08A85B1Cu); }

// -------------------------------------------------------------------------------------------------------------------------
// 0x08B10A7C (a0 = a tree owner with its list at +0x58, a1 = the keyed object): walks the list from owner[+0x5C] while
// the key pair (keyed +0xF0, keyed +0xF4, compared signed as the `slt`s do) sorts after a node's range (+0xF0, +0xF4 of
// the object the node's first word points at); then 0x08B34888(keyed + 4), keyed[+0xA4] = 2,
// 0x08B1FC10(owner + 0x58, node, keyed + 0x3C), 0x08B34B24(keyed + 4) and the tail call 0x08B28C74(owner + 0x64).

// -------------------------------------------------------------------------------------------------------------------------
// 0x08B1A338 (a0 = a source, a1 = a target, a2 = a pair): 0x08B1A2AC(source, target), 0x08B1A0B4(source), then a 0x20-byte
// block (its first two words zeroed, 0x08B34DA8 on the tail) receives source's {+0, +4} at +0x10/+0x14 - the word at +4
// is a shared control block whose count at +4 is bumped atomically when set (the asm's ll/sc; a plain read-modify-write
// is the same on the harness's single thread) - and the pair's two words at +0x18/+0x1C; 0x08B1F1C4(target + 0x2C, block),
// 0x08B28C74(target); v0 = source.

} // namespace
} // namespace new_batch_main_187
} // namespace gt::game

#define MAIN187(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_187::name, gt::ReturnKind::kind)
MAIN187(0x0881888Cu, func_0881888C, Word);
MAIN187(0x0888CFCCu, func_0888CFCC, Word);
MAIN187(0x0892A2C0u, func_0892A2C0, Word);
MAIN187(0x08AB1E50u, func_08AB1E50, Word);
MAIN187(0x08C56A90u, func_08C56A90, Word);

// ---- from main-session batch 188 (draft e31143fbd747001f7a900f4ea1829db731da5584) ----
// New batch main 188: 30 functions written as structured C from `tools.disasm`, picking up the unverified drafts of
// src/game/unsorted/new_batch_main_15.cpp (commit e242d117) and re-reading each body against its own asm. Four function
// families repeat and are written once as a template with the member's own constants (each member still registers and
// runs on its own):
//   * the delegate (0x08C10644 / 0x08C1344C / 0x08C1E858 / 0x08BEBBF8 / 0x08C08D5C / 0x08C13170): a0 is an invoker
//     with a tag at +0x18 and a base at +0x14 for the first dispatch, +0x28 / +0x24 for the second; bit 0 of a tag picks
//     "the word at base + the word the adjusted pointer addresses" over "base itself". The first dispatch is a real call,
//     the second a tail call after the value went through 0x08B8C590 (integer) or 0x08B8C5D4 (float, its f0 moved to
//     f12); the prepare variant builds a stack temporary first and passes it with the pointer.
//   * the divide-through-a-virtual-getter pair (0x08AB060C / 0x08AB1D90): the field at +0x14 of a0 divided by a slot's
//     answer, built on the stack by 0x08A85B1C, handed to a1 when a1 is not that slot, released.
//   * the key lookup (0x08877724 / 0x088777E4 / 0x088778A4): a temporary key built by 0x08AB85F0 / 0x08AB856C, a second
//     lookup 0x088757B0 and the member's own third call; the temporary released through the standard-library cell code.
//   * the float through a temporary (0x0899DDA0 / 0x0899DE64): a getter's float replaced by an object's when the count is
//     positive and set back through the member's own setter.
// Two functions had no draft: 0x08AFB9A4 (a constructor-shaped body whose tail call gets the six arguments a0 restored
// from the callee-saved registers) and 0x08AFC7C8 (a loop over a table of 0xC-byte entries, byte 0x47 of a name lookup).
//
// What the drafts got wrong, fixed here against the asm (each one is a mismatch the gate would have found):
//   * 0x088777E4 and 0x088778A4 do not call 0x08873260 - that is 0x088730B0 and 0x08873194 (the third call differs per
//     member); 0x0899DE64's setter is 0x0899D174, not 0x0899D214.
//   * The cell release in all of these is 0x08B60804(data - 4, -1) and dispose(data - 0xC, sp): the draft passed data -
//     0xC to the counter, which reads four bytes into the string data.
//   * 0x08C5D020/0x08C5D104 pass the normalised flag on in a2 to 0x08B60DB8; 0x088EBBF8 hands the last call the value of
//     v0 the asm leaves there (0x088D4808's), not 0x08B08AC0's.
//   * The first dispatch passes the four argument registers the original sets (a0 = adjusted pointer, a1 = the index word,
//     a2 = the base, a3 = the tag) instead of a0 alone.
//
// Rules followed: a `jal` / `jalr` delay slot runs BEFORE the call; v0 (and v1 / f0 / a1 where the next call can read
// them) is what the callee leaves, so `call` writes them back into the context like the original's registers; v0 is set
// where the asm leaves it on every exit path, the constant an argument test compares against included.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_188 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using GuestResult = gt::GuestResult;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers written to ctx as the original sets them before the `jal` / `jalr` (a0-a3,
// t0-t3 in order, then f12-f15), and v0 / v1 / f0 / a1 afterwards as the callee left them: a callee that writes none of
// them hands the caller's previous value on, and the caller's next call reads the callee's leftover a1, as the original's
// does.
GuestResult call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                 std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float value : floats) {
        if (freg <= 15u) ctx.fpr[freg++] = value;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v1, 0(object); lw v0, offset(v1); jalr v0`: v1 holds the vtable and v0 the slot's function when the callee starts.
GuestResult call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call(rt, ctx, function, arguments);
}
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// The release of a reference-counted cell whose data pointer is `data` (0x08B60804 is __exchange_and_add): nothing when
// data - 0xC is the shared empty cell of this string type - and the original leaves that constant in v0 there -, else the
// count at data - 4 is decremented and, when it was not positive, the cell is disposed of with `dispose`(data - 0xC,
// `flag`), which leaves its own v0.
void release_cell(Runtime &rt, AllegrexContext &ctx, u32 data, u32 flag, u32 empty_cell, u32 dispose) {
    const u32 block = data - 0xCu;
    ctx.gpr[2] = empty_cell;
    if (block == empty_cell) return;
    const u32 references = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}).v0;
    if (static_cast<s32>(references) <= 0) call(rt, ctx, dispose, {block, flag});
}

// -------------------------------------------------------------------------------------------------------------------------
// The delegate. Two dispatches over the invoker's tag / base pairs; the second one is a tail call, so the frame the
// original popped before it stays closed here too.
struct Dispatch {
    u32 function;   // the address the tail call goes to
    u32 pointer;    // the adjusted pointer (the tail call's first argument)
    u32 v0;         // what the original holds in v0 when the second step starts
    u32 v1;         // and in v1
};

// a0 = index + tag>>1 (arithmetic), a1 = the index word, a2 = the base, a3 = the tag; the target is the base itself
// when bit 0 of the tag is clear, else the word at base + the word the adjusted pointer addresses. v1 = tag & 1 either
// way, and the call's v0 is what the next step works from.
u32 first_dispatch(Runtime &rt, AllegrexContext &ctx, u32 invoker, u32 index) {
    const u32 tag = word(rt, invoker + 0x18u);
    const u32 base = word(rt, invoker + 0x14u);
    const u32 pointer = index + static_cast<u32>(static_cast<s32>(tag) >> 1);
    if ((tag & 1u) == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = base;
        return call(rt, ctx, base, {pointer, index, base, tag}).v0;
    }
    const u32 target = word(rt, word(rt, pointer) + base);
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = target;
    return call(rt, ctx, target, {pointer, index, base, tag}).v0;
}
Dispatch second_dispatch(Runtime &rt, u32 invoker, u32 pointer) {
    const u32 tag = word(rt, invoker + 0x28u);
    const u32 half = static_cast<u32>(static_cast<s32>(tag) >> 1);
    const u32 at = pointer + half;
    if ((tag & 1u) == 0u) return {word(rt, invoker + 0x24u), at, 0u, half};
    const u32 base = word(rt, invoker + 0x24u);
    const u32 sum = word(rt, at) + base;
    return {word(rt, sum), at, sum, base};
}
// The integer twin: the value goes through 0x08B8C590 and its v0 becomes the tail call's a1.
void tagged_int(Runtime &rt, AllegrexContext &ctx, u32 convert) {
    GuestFrame frame(ctx, 0x10u);
    const u32 invoker = ctx.gpr[4];
    const u32 value_argument = ctx.gpr[6];
    const u32 result = first_dispatch(rt, ctx, invoker, word(rt, ctx.gpr[5]));
    const Dispatch target = second_dispatch(rt, invoker, result);
    ctx.gpr[2] = target.v0;
    ctx.gpr[3] = target.v1;
    const u32 value = call(rt, ctx, convert, {value_argument}).v0;
    tail_call(ctx, target.function, {target.pointer, value});
}
// The float twin: 0x08B8C5D4's f0 is moved to f12 for the tail call, and a1 stays the callee's leftover.
void tagged_float(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 invoker = ctx.gpr[4];
    const u32 value_argument = ctx.gpr[6];
    const u32 result = first_dispatch(rt, ctx, invoker, word(rt, ctx.gpr[5]));
    const Dispatch target = second_dispatch(rt, invoker, result);
    ctx.gpr[2] = target.v0;
    ctx.gpr[3] = target.v1;
    const GuestResult value = call(rt, ctx, 0x08B8C5D4u, {value_argument});
    ctx.fpr[12] = value.f0;
    tail_call(ctx, target.function, {target.pointer});
}
// The prepare variant: a builder fills the stack temporary at sp+0 with the value, then the target is called with
// (pointer, &temporary) from a real frame, so no tail call.
void tagged_prepare(Runtime &rt, AllegrexContext &ctx, u32 builder, u32 frame_size) {
    GuestFrame frame(ctx, frame_size);
    const u32 invoker = ctx.gpr[4];
    const u32 value_argument = ctx.gpr[6];
    const u32 result = first_dispatch(rt, ctx, invoker, word(rt, ctx.gpr[5]));
    const Dispatch target = second_dispatch(rt, invoker, result);
    ctx.gpr[2] = target.v0;
    ctx.gpr[3] = target.v1;
    call(rt, ctx, builder, {frame.at(0u), value_argument});
    call(rt, ctx, target.function, {target.pointer, frame.at(0u)});
}


// -------------------------------------------------------------------------------------------------------------------------
// 0x08C5D020 / 0x08C5D104 (a0 = the object, a1 = a name, a2 = a flag): a standard-exception-shaped constructor. The flag
// becomes a bool at +4 (sltu, in the first call's delay slot), vtable 0x08CDB798 and a zero at +8 are stored, 0x08B60DB8
// runs with (this, 0, flag), the vtable becomes 0x08CDB930, and the name is matched twice (0x08B35FD8 with 0x08CB57E4 and
// 0x08CB57E8). Both hits build the message with 0x08B61B90(&temporary, name, 0), pass it to 0x08B60DB8 and release it
// with 0x08B61BC8; either miss returns with the zero the lookup answered.
void exception_pair(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] != 0u ? 1u : 0u;
    put(rt, self + 4u, flag);
    put(rt, self, 0x08CDB798u);
    put(rt, self + 8u, 0u);
    call(rt, ctx, 0x08B60DB8u, {self, 0u, flag});
    put(rt, self, 0x08CDB930u);
    if (call(rt, ctx, 0x08B35FD8u, {name, 0x08CB57E4u}).v0 == 0u) return;
    if (call(rt, ctx, 0x08B35FD8u, {name, 0x08CB57E8u}).v0 == 0u) return;
    call(rt, ctx, 0x08B61B90u, {frame.at(0u), name, 0u});
    call(rt, ctx, 0x08B60DB8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08B61BC8u, {frame.at(0u)});
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08877724 / 0x088777E4 / 0x088778A4 (a0 = a factory, a1 = the name in an argument object): a temporary key built by
// 0x08AB85F0 + 0x08AB856C, 0x088755B4(factory, key) against the factory's table, the key again, a second lookup
// 0x088757B0(factory, 0x08CA0074) and - when that found something - the member's own third call with (found, key,
// second key): 0x08873260, 0x088730B0 and 0x08873194 respectively. Then the temporary's cell is released; v0 is 0x08D75298
// (the wide string's empty cell) when no counter was touched.
void key_lookup(Runtime &rt, AllegrexContext &ctx, u32 report) {
    GuestFrame frame(ctx, 0x30u);
    const u32 factory = ctx.gpr[4];
    const u32 slot = frame.at(4u);
    call(rt, ctx, 0x08AB85F0u, {slot});
    const u32 first_key = call(rt, ctx, 0x08AB856Cu, {slot}).v0;
    call(rt, ctx, 0x088755B4u, {factory, first_key});
    const u32 second_key = call(rt, ctx, 0x08AB856Cu, {slot}).v0;
    const u32 found = call(rt, ctx, 0x088757B0u, {factory, 0x08CA0074u}).v0;
    if (found != 0u) call(rt, ctx, report, {found, first_key, second_key});
    release_cell(rt, ctx, word(rt, slot), frame.at(0u), 0x08D75298u, 0x08B93554u);
}
void func_088777E4(Runtime &rt, AllegrexContext &ctx) { key_lookup(rt, ctx, 0x088730B0u); }

// 0x088798D0 (a0 = a factory): the same temporary, 0x088756F0(factory, 0, key) and, when it answers, the found object's
// virtual at +0x1C; then the same release. v0 is 0x08D75298 on the untouched path.
void func_088798D0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 factory = ctx.gpr[4];
    const u32 slot = frame.at(4u);
    call(rt, ctx, 0x08AB85F0u, {slot});
    const u32 key = call(rt, ctx, 0x08AB856Cu, {slot}).v0;
    const u32 found = call(rt, ctx, 0x088756F0u, {factory, 0u, key}).v0;
    if (found != 0u) call_slot(rt, ctx, found, 0x1Cu, {found});
    release_cell(rt, ctx, word(rt, slot), frame.at(0u), 0x08D75298u, 0x08B93554u);
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08975F1C (a0 = self, a1 = an object): a request record at sp+0xC..+0x18 (0x08CCDA08, the empty cell 0x08D74FD0 + 0xC,
// -1, and an output slot) goes to the object's virtual at +8 with a1 = the record; then the first temporary is released
// (the cell sentinel here is 0x08D74FD0, and 0x08CCD990 overwrites the record's first word, as the original does) and
// 0x08974698(self, &output) runs last, so v0 is its answer.

// -------------------------------------------------------------------------------------------------------------------------
// 0x08A514DC (a0 = the object): mark the byte at +0x5C, run 0x08B28C74 on +0x30, 0x08B2E224 and 0x08B2E28C on the +0x2C
// owner, release the +0x28, +0xC and +0x10 blocks (when each is set), take the lock 0x08CF6D34, release and clear the +0
// and +4 blocks, and hand the unlock to a tail call.

// -------------------------------------------------------------------------------------------------------------------------
// 0x08AB060C / 0x08AB1D90 (a0 = the object, a1 = the caller's holder, a3 = a pointer to the object whose slot answers):
// the field at +0x14 of a0 (an unsigned half or byte) divided by the answer of slot 0x48 / 0x44 of a3's object - both the
// dividend's owner and the divisor queried through a0 = the same object, each guarded with 0x08A5BB7C when the pointer is
// null. The quotient is built on the stack by 0x08A85B1C, moved into a1 unless a1 is that slot, and released.
enum class Dividend { UnsignedHalf, UnsignedByte };

void divide_through(Runtime &rt, AllegrexContext &ctx, u32 vtable_offset, u32 guard_text, Dividend kind) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 owner = ctx.gpr[7];
    u32 holder = word(rt, owner);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    call_slot(rt, ctx, holder, vtable_offset, {holder});
    holder = word(rt, owner);
    // The dividend is loaded in the second guard's branch delay slot, so on both of its paths.
    const u32 dividend = kind == Dividend::UnsignedHalf ? rt.memory().load16(self + 0x14u) : rt.memory().load8(self + 0x14u);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    const u32 divisor = call_slot(rt, ctx, holder, vtable_offset, {holder}).v0;
    const u32 quotient = divisor == 0u ? 0xFFFFFFFFu : dividend / divisor;  // divu: a zero divisor leaves -1
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), quotient});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08ACBDF0 / 0x08ACBEB0 (a0 = the object, a1 = the request text): take the global lock 0x08D69170 (0x08B31990, or
// 0x08B327AC with mode 2 when it is not held yet), let 0x08ACB910 build a descriptor at sp+0xC, fill a request at sp with
// the member's own mode (4 / 2) at +0x3C and a copied text, chain it through 0x08ACBC78 with sp stored at +0x40, and
// answer that call's v0 - or 0 from either refusal.
void copy_pair(Runtime &rt, AllegrexContext &ctx, u32 mode) {
    GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 descriptor = frame.at(0xCu);
    if (call(rt, ctx, 0x08B31990u, {0x08D69170u, text}).v0 == 0u) {
        if (call(rt, ctx, 0x08B327ACu, {0x08D69170u, 2u}).v0 == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    if (call(rt, ctx, 0x08ACB910u, {self, descriptor}).v0 == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, frame.at(0x3Cu), mode);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(8u), 0u);
    if (text != 0u) call(rt, ctx, 0x08B7C830u, {frame.at(0u), text, 8u});
    put(rt, frame.at(0x40u), frame.at(0u));
    ctx.gpr[2] = call(rt, ctx, 0x08ACBC78u, {self, descriptor}).v0;
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08AFB9A4 (a0 = the object, then a1, a2, a3, t0, t1 for the tail call): constructor-shaped. vtable 0x08CD7738 at +0,
// 1.0f at +0x24 and 0x08CD777C at +0x28, words +4..+0x20 cleared, then 0x08B5284C(this + 0x2C) and a tail call of
// 0x08AFACCC with this and the five original arguments, which is why they are saved before the call: the callee's own
// registers would otherwise stand there.
void func_08AFB9A4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const u32 fifth = ctx.gpr[9];
    put(rt, self, 0x08CD7738u);
    put(rt, self + 0x28u, 0x08CD777Cu);
    put_float(rt, self + 0x24u, 1.0f);
    for (u32 offset = 4u; offset <= 0x20u; offset += 4u) put(rt, self + offset, 0u);
    call(rt, ctx, 0x08B5284Cu, {self + 0x2Cu});
    tail_call(ctx, 0x08AFACCCu, {self, first, second, third, fourth, fifth});
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08AFC7C8 (a0 = the object, a1 = a byte, a2 = an optional name): walk the table at +0x1C (0x16 = its count as an
// unsigned half, entries of 0xC bytes, the count re-read every pass). With a name, an entry whose first word 0x08B35FD8
// matches is skipped; otherwise 0x08AFC8C8(self, its signed half at +0xA) and 0x08AFC8B8(that, 0x47) run, and a non-null
// answer gets the byte stored at +0xD.
void func_08AFC7C8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 byte = ctx.gpr[5];
    const u32 name = ctx.gpr[6];
    u32 offset = 0;
    for (u32 index = 0; index < rt.memory().load16(self + 0x16u); ++index, offset += 0xCu) {
        const u32 entry = word(rt, self + 0x1Cu) + offset;
        if (name != 0u && call(rt, ctx, 0x08B35FD8u, {word(rt, entry), name}).v0 != 0u) continue;
        const u32 result = call(rt, ctx, 0x08AFC8C8u, {self, static_cast<u32>(signed_half(rt, entry + 0xAu))}).v0;
        const u32 target = call(rt, ctx, 0x08AFC8B8u, {result, 0x47u}).v0;
        if (target != 0u) rt.memory().store8(target + 0xDu, static_cast<std::uint8_t>(byte));
    }
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08C5518C (a0 = the destination, a1 = the source record, a2 = an argument): copy constructor with vtable-relative
// offsets - the source's +4, +0 and +0xC objects hold their destination offsets at -0xC (called 4th, 1st and 0xCth
// here), +4 of the destination is zeroed, then the source's +0, +8, +0x10, +0x14 and +0x18 words are stored through
// them, and 0x08C54A7C(this + this[0][-0xC], argument) finishes.

// -------------------------------------------------------------------------------------------------------------------------
// 0x088212F8 (a0 and a1 = two keys): each key is built in its own stack slot (0x088547C4 into sp+4 and sp, the slot's
// word checked against null with 0x08A5BB7C 0x08C995DC and released with 0x08A5B9EC after 0x08854678 reads it), then both
// go through 0x088A7608 and the answer is the signed "the first sorts before the second" (slt).

// -------------------------------------------------------------------------------------------------------------------------
// 0x0884B9D0 (a0 = a factory): 0x08847008 on a stack slot, a null word guarded (0x08A5BB7C, 0x08C9CBBC), 0x08846E50 reads
// it and the slot is released; 0x088D47EC answers the size, 0x08B2B4D8(0x10, size) the block, 0x088D51BC(first, block)
// fills it and 0x08A44D9C builds the value at sp with (block, size, 1); the caller's holder (a0) is moved into unless it is
// that slot, which is released last.

// -------------------------------------------------------------------------------------------------------------------------
// 0x088E58DC (a0 = a pair of object slots): 0x08B0979C, then 0x08B09908 on its answer; then each of the two objects
// (a0[0], a0[1]): a null one is skipped, one marked at +0x521 or whose virtual at +8 answers zero sends the whole thing
// through 0x088E57CC with 0 (that is the answer), otherwise 0x088ED560(object), 0x088ED6F4(object, 0) and the marks
// +0x521 = 1 and +0x50C = 1. v0 = 1 after both.

// -------------------------------------------------------------------------------------------------------------------------
// 0x088EBBF8 (a0 = self, a1 = a flag byte): the virtual at +0x9C of the device at +0x538 must answer non-zero. Then
// 0x08B08AC0(word(self + 0xA8)) is the third argument, 0x088D4808() the fourth (and its v0 is what the context holds when
// the device's virtual at +0x2C starts), and the call is (device, argument, third, fourth, flag). A non-zero answer clears
// self's +0x574 and +0x560..+0x570 and gives 1; anything else 0.

// -------------------------------------------------------------------------------------------------------------------------
// 0x088ECCD4 (a0 = a source object, a1 = a byte, a2 = an optional four-halfword source): the virtual at +0x18 of a0 (a1 =
// 0) answers the record's first two words; +8 is the byte, +0xA is (a2 != 0), +0xC..+0x12 are a2's four halfwords and
// +0x14..+0x17 the four bytes at a0 + 0x4AC. The device's virtual at +0xF4 at +0x538 takes (device, 0, -1, 3, &record,
// 0x18).

// -------------------------------------------------------------------------------------------------------------------------
// 0x0898E358 (a0 = an out record, a3 = a pointer to an object): 0x0898E170 fills a stack slot (guarded with 0x08A5BB7C,
// 0x08CA3CB8), the object through a3 is guarded too (0x08CA3C7C), its virtual at +0x3C answers, 0x0898D8D4(temporary,
// answer) makes the value, the temporary is released, 0x08A85B1C builds the value at sp, a0's holder is moved into unless
// it is that slot, and the slot is released last.

// -------------------------------------------------------------------------------------------------------------------------
// 0x0899DDA0 / 0x0899DE64 (a0 = self, a2 = a count, a3 = a pointer to an object): 0x0899DD60 fills a stack slot (guarded
// with 0x08A5BB7C, 0x08CA496C, and again before the setter), 0x0899D164 answers its float; with a positive count the
// object through a3 (guarded, 0x08CA485C) answers through its virtual at +0x54 instead; the float goes back through
// 0x0899D214 / 0x0899D174 respectively and the slot is released.
void float_through(Runtime &rt, AllegrexContext &ctx, u32 setter) {
    GuestFrame frame(ctx, 0x30u);
    const u32 count = ctx.gpr[6];
    const u32 object_pointer = ctx.gpr[7];
    call(rt, ctx, 0x0899DD60u, {frame.at(0u)});
    u32 holder = word(rt, frame.at(0u));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
    float value = call(rt, ctx, 0x0899D164u, {holder}).f0;
    if (static_cast<s32>(count) > 0) {
        const u32 object = word(rt, object_pointer);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA485Cu});
        value = call_slot(rt, ctx, object, 0x54u, {object}).f0;
    }
    holder = word(rt, frame.at(0u));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
    call(rt, ctx, setter, {holder}, {value});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x08AB1F0C (a0 = an object, a1 = an out record, a3 = a pointer to an object with a scale): the unsigned byte at a0+0x14
// and the scale object's virtual at +0x44 (guarded with 0x08A5BB7C, 0x08CB3AF0) go to 0x08B67720 as f12 / f13, and its
// f0 is truncated the way `trunc.w.s` with the 2^31 adjustment does: from 2^31 up, f0 - 2^31 is truncated and the sign
// bit forced on. 0x08A85B1C builds the word at sp, a1's holder is moved into unless it is that slot, and the slot is
// released.
} // namespace
} // namespace new_batch_main_188
} // namespace gt::game

#define NEW188(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_188::name, gt::ReturnKind::kind)
NEW188(0x088777E4u, func_088777E4, Word);
NEW188(0x088798D0u, func_088798D0, Word);
NEW188(0x08AFB9A4u, func_08AFB9A4, Word);
NEW188(0x08AFC7C8u, func_08AFC7C8, Word);

// ---- from main-session batch 189 (draft 1137927b0c481b21d347d33436d14b9218dbf3fa) ----
// New batch, main session 189: functions nothing had taken, written as structured C by hand from `tools.disasm` (the
// recompiled original is the differential reference; docs/agents/decomp-workflow.md). Draft, unverified: the writer does
// not run the gate. No names: none has evidence yet.
//
// 25 of the 28 addresses of the batch are written here. Three of them are left out because they are not functions of
// their own in the original, and a decompiled body for them cannot behave like the original:
//   * 0x08B6250C - no epilogue: its 32 instructions end inside a `j`'s delay slot, and every path ends in 0x08B6459C,
//     which runs 0x08B64564 (a `jalr a0` into 0x08B77B04) and never returns. The code after the first 0x08B6459C call
//     reads `v1`/`a0` that only the (never returning) callee sets, and the original falls on into 0x08B6258C.
//   * 0x08BF9F3C - a loop-continuation label, not a function: its first instruction branches back to 0x08BF9ED4, which
//     is before it and inside the merge body the recompiler never gave a symbol of its own. `src/core/thunks.cpp` already
//     models the `j` from 0x08BF9ECC to it with GT_DECOMP_THUNK.
//   * 0x08B63AF8 - a fragment of the C++ exception machinery: two of its branches (0x08B63B74 -> 0x08B63BE4 and
//     0x08B63B94 -> 0x08B63BDC) leave its own range for the bodies of the following "functions", where one of them runs
//     on into 0x08B63BEC with this frame's `ra`. Its end state cannot be reproduced from a standalone body.
// The synthetic input shapes of the batch are in /tmp/claude-1000/shapes_main189.txt (one line per address that
// dereferences an argument or a global).
//
// Rules followed here, as the batch files do (docs/agents/decomp-workflow.md, "Structured C only"): a `jal`/`jalr`
// delay slot runs BEFORE the call, so a store or an arithmetic in a delay slot is written before the call; `v0` is set
// where the asm leaves it on every exit path, and a tail call (`j` out of the function or `jr` through a register a
// caller loaded) goes through `tail_call`; arithmetic shifts stay arithmetic; an argument register the callee may read
// is passed even when the original only inherited it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_189 {
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
// The same, keeping v1 and f0 (a caller that passes the callee's v1 on reads it after the jal).
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The reference-counted string release the deallocators share: `data - 0xC` against the shared empty representation
// `empty`; otherwise `release(data - 4, -1)` and, when that count reaches zero or below, `free(data - 0xC, scratch)`
// (0x08B60804 / 0x08B8542C, and 08C74884 in the throw helper). `scratch` is a byte of the caller's frame the free
// helper takes the address of.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 release, u32 free_storage, u32 empty, u32 data, u32 scratch) {
    const u32 count = data - 0xCu;
    if (count == empty) return;
    if (static_cast<s32>(call(rt, ctx, release, {data - 4u, 0xFFFFFFFFu})) <= 0)
        call(rt, ctx, free_storage, {count, scratch});
}

// The vector release two of the deallocators share (0x08A90B18, 0x0899796C): `prepare(begin, word(base + 4), scratch)`,
// then - when the begin pointer was not null - `0x08A5BC54(begin, ((word(base + 8) - begin) >> 2) << 2, 4)`.
void release_vector(Runtime &rt, AllegrexContext &ctx, u32 prepare, u32 base, u32 scratch) {
    call(rt, ctx, prepare, {word(rt, base), word(rt, base + 4u), scratch});
    const u32 begin = word(rt, base);
    if (begin == 0u) return;
    const u32 count = static_cast<u32>(static_cast<s32>(word(rt, base + 8u) - begin) >> 2);
    call(rt, ctx, 0x08A5BC54u, {begin, count << 2, 4u});
}

// The shared body of 0x08B895E0 / 0x08C0DBBC (a0 = container with an end pointer at +4, a1 = destination, a2 = source):
// copies whole 0x20-byte elements of [source, word(a0+4)) (each copied through `element_copy` with its +0x10 word, and
// only when the two addresses differ), finishes the range through `finish` and shrinks the container's size by the
// copied byte count with the low five bits cleared. v0 = a1.
void copy_elements_and_shrink(Runtime &rt, AllegrexContext &ctx, u32 element_copy, u32 finish) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const s32 count = static_cast<s32>(word(rt, self + 4u) - source) >> 5;
    u32 out = destination;
    if (count > 0) {
        u32 from = source;
        for (s32 index = 0; index < count; ++index) {
            if (from != out) {
                call(rt, ctx, element_copy, {out, from});
                put(rt, out + 0x10u, word(rt, from + 0x10u));
            }
            from += 0x20u;
            out += 0x20u;
        }
    }
    call(rt, ctx, finish, {out, word(rt, self + 4u), frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((source - destination) & ~31u));
    ctx.gpr[2] = destination;
}

// 0x144 (0x08B5AA30, a0 = text): builds a string at sp+4 through 0x08C74A68, allocates an 8-byte exception object
// (0x08B61E98), runs 0x08B5B4F4(object, &text), drops the string's storage (0x08B60804 / 0x08C74884, the shared empty
// representation 0x08D77498) and throws the object with 0x08B62498(object, 0x08CDAE10, 0x08B5B5FC). Nothing returns.

// 0x196 (0x08B0C8D8, a0 = self, a1..a3 and t0 as given): 0x08B0C778 with all five, then 0x08B0EA48(self + 0x20, a1)
// stored at self+8. A null block leaves mode 2 at +4 and answers -1; otherwise block+0x54 = self, 0x08B0DE84(block),
// block+0x7C = a3, block+0x94 = t0, 0x08B0DF0C(block), then 0x08B0C880(self) and - when negative - 0x08B0C358(self).
// v0 = that status (-1 when the block was refused).

// 0x196 (0x08C122C8, a0 = vector with its end pointer at +4, a1 = destination, a2 = source): moves
// (word(a0+4) - a2) / 0x10 records of 0x10 bytes from a2 on to a1 through 0x089FB570, then 0x08C12284(next, end, &temp)
// and shrinks the end pointer by (source - destination) with the low four bits cleared. v0 = the destination.

// 0x200 (0x089EFA48, a0 = object): stores the two vtables (0x08CBF080 at +0, 0x08CBF2BC at +0x38), destroys the two
// members at +0x244 and +0x240 (0x08A5B9EC), releases the strings at +0x23C and +0x238 (0x08B60804 / 0x08B8542C, the
// shared empty representation 0x08D74FD0) and goes on in 0x089E2430(object). v0 = its answer.

// 0x200 (0x08B2EBE4, a0 = a function, a1 = its argument): when 0x08C7A9E8() answers the word at 0x08D690F0 the function
// runs directly with the argument (a zero function answers nothing). Otherwise the frame keeps the two at sp+0x30 /
// sp+0x34 and sp itself at 0x08D690C0 between 0x08B28D34() and the 0x08D690C4 dispatches 0x08B34888, 0x08B34B24,
// 0x08B34888, 0x08B34C1C and 0x08B34B24, then 0x08B28C44(sp + 4) and 0x08B28BE0(sp + 4) run. v0 = the last result.

// 0x200 (0x08C65E3C, a0, a1 = a two-word source, a2 = a flags byte, a3, t0, t1): copies word(a1) and the byte at a1+4
// into the frame at sp+8 / sp+0xC and calls 0x08C6500C (flag byte set) or 0x08C65724 (clear) with
// (a0, &copy, a3, sign-extended byte of t0, t1, t1, word(a1), flags). v0 = the callee's v0 and v1 = the byte of its v1.
void func_08C65E3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[5];
    const u32 flags = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(8u), word(rt, source));
    put8(rt, frame.at(0xCu), rt.memory().load8(source + 4u));
    const u32 signed_t0 = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[8])));
    const GuestResult result = call_result(rt, ctx, (flags & 0xFFu) != 0u ? 0x08C6500Cu : 0x08C65724u,
                                           {ctx.gpr[4], frame.at(8u), third, signed_t0, ctx.gpr[9], ctx.gpr[9],
                                            word(rt, source), flags & 0xFFu});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1 & 0xFFu;
}

// 0x204 (0x089ECBAC, a0 = object, a1 = a source object): stores the vtable 0x08CCFA30 at +0, runs the source's virtual
// +0x108 twice - on the word at word(a1) with this+4, then with this+8 (an assert 0x08A5BB7C(0x08CA7CD0) when it is
// null) - sets +0xC to 1, 0x08A71E28(this + 0x10, a1), and clears +0x14..+0x1C, the byte at +0x24, +0x20, +0x28 and the
// byte at +0x2C. v0 = this + 0x14.
void func_089ECBAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CCFA30u);
    const u32 first = word(rt, source);
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA7CD0u});
    call_indirect(rt, ctx, word(rt, word(rt, first) + 0x108u), {self + 4u, first});
    const u32 second = word(rt, source);
    if (second == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA7CD0u});
    call_indirect(rt, ctx, word(rt, word(rt, second) + 0x108u), {self + 8u, second});
    put(rt, self + 0xCu, 1u);
    call(rt, ctx, 0x08A71E28u, {self + 0x10u, source});
    const u32 block = self + 0x14u;  // the asm keeps this+0x14 in v0 and clears +8 / +4 of it
    put(rt, block + 8u, 0u);
    put(rt, block + 4u, 0u);
    put(rt, self + 0x14u, 0u);
    put8(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x20u, 0u);
    put8(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    ctx.gpr[2] = block;
}

// 0x208 (0x08ACB3B4, a0 = object): takes the 0x08B34888 lock on this+0x60, asks 0x08C7B1A8 for a state and, when the byte
// at +0x8C is set and the state is not 1..4, asks 0x08C7B190(word at +0x94) for a value (+0x90 = it, byte +0x8D = 1 when
// it is not null) and clears +0x8C. Otherwise state 2 runs 0x08C7B1D8(1), state 3 0x08C7B1C0, and state 0 clears +0x8D
// with 0x08B34CA0 when it was set. The lock is released by 0x08B34B24, which the function jumps to with this+0x60.

// 0x208 (0x08ACEE34, a0 = object): stores the vtable 0x08CD6B28 at +0 and runs 0x08ACDD14, 0x08C7AD80 and 0x08B2EE2C
// on +0x2C, +0x18 and +4. Unless this+0x420 is the address of its second half (+0x510) it also runs 0x08B28DC4 on
// +0x4A0, 0x08B28BE0 on +0x474 and +0x448 and 0x08B28DC4 on +0x420; unless this+0x334 is that same address, 0x08B28DC4
// on +0x3B8 and 0x08B28BE0 on +0x38C, +0x360 and +0x334. 0x08B28DC4(+0x164) runs either way and the work goes on in
// 0x08B0C808(this + 0x40).

// 0x212 (0x0883B600, a0, a1 = a count, a2 = pointer to an object): with a positive count and an object, the object's
// virtual +0xC runs into a temporary at sp+4, which 0x08B855E0 turns into the value at sp+8; a head at 0x08D74FD0 means
// two shared temporaries need their reference counts dropped. v0 = the last call's.

// 0x212 (0x08A641F4, a0 = object, a1 = pointer to the source object, a2, a3): 0x08A615D4 / 0x08A61BC0 set the object up
// from sp, the source's virtual +0xD0 runs with the temporary at sp+4, and 0x08A63F1C runs; when the object's +0x50 is
// 1 the two teardown calls follow. v0 = the temporary's destroy.
void func_08A641F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    const u32 arg3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 temporary = frame.at(4u);
    call_result(rt, ctx, 0x08A615D4u, {self, 1u});
    put(rt, frame.at(0u), self);
    call(rt, ctx, 0x08A61BC0u, {temporary, frame.at(0u)});
    const u32 object = word(rt, source);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0xD0u), {object, temporary, arg2, arg3});
    call(rt, ctx, 0x08A63F1Cu, {self, 0u});
    if (word(rt, self + 0x50u) == 1u) {
        call(rt, ctx, 0x08A6158Cu, {self});   // a0 = s1 = self (the stale a0 held it too)
        call_result(rt, ctx, 0x08A615D4u, {self, 0u});
    }
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {temporary});
}

// 0x212 (0x08B0E6B0, a0 = object): under the 0x08B34888 lock on this+4, +0xEC / +0x104 / +0x108 are cleared and the list
// at +0xFC is walked (each node's object runs its virtual +8 and +0x104 counts the nodes); an empty list runs the
// object's own virtual +0x24 with the byte at +0x68 cleared instead and keeps that byte. When it was set the work goes
// on in 0x08B0DEB0(this).

// 0x212 (0x08B327AC, a0 = object, a1 = a kind): a repeat of the kind recorded at 0x08D69168 answers 1 at once; otherwise
// 0x08B324CC runs and kind 0 takes 0x08B2DBF0 with the word at 0x08CE0C80, kind 1 / 2 ask 0x08B322CC / 0x08B3265C about
// the object (0 answers 0), anything else (and a positive answer) runs 0x08B2DC70, raises 0x08CE0C80 to the signed
// maximum with 0xDE as 0x08B2DBF0's argument and records the kind. v0 = 1 or 0.

// 0x212 (0x08B895E0) and 0x212 (0x08C0DBBC): the 0x20-byte element twin of 0x08C122C8.

// 0x216 (0x089C1368, a3 = pointer to a source): a temporary at sp+8 comes from 0x089BEE4C, the source's virtual +0xC
// fills sp+4 with an object of its own, 0x089C12F4 turns that into the target, the head at sp+4 drops its count and the
// temporary at sp+8 is destroyed. v0 = the last destroy's.

// 0x216 (0x08C49A28, a0 = container with an end pointer at +4, a1 = destination, a2 = source): copies the whole range
// [a2, word(a0+4)) in 240-byte elements (each as fifteen 4-word chunks), appends through 0x08C47014 and shrinks the
// container's size by the copied byte count with the low four bits cleared. The count is the asm's `mult`/`mflo` by
// 0xEEEEEEEF (the low product), so a signed `> 0` test decides. v0 = a1.

// 0x220 (0x088692E8, a0 = object): sets a 0x1000-byte allocation up around the object's +0x74 / +0x1B0 halves through
// the 0x0886xxxx helpers and goes on in 0x08869D58 with +0x1B0.

// 0x220 (0x08936DF4, a0 = object): stores the vtable 0x08CC67B8 and runs the virtual +4 of the 44 pointers at +0x8C and
// at +0x13C (four bytes apart, the first one before the count), then of +0x1EC and +0x1F0; 0x08938FCC runs on +0x2D0
// and the work goes on in it with +0x1F4.

// 0x220 (0x08A90B18, a0 = object): stores the vtable 0x08CB6C98 at +0, runs 0x08A90888, destroys the member at +0x30
// and releases the two vectors at +0x24 and +0x18 (0x08C2AE8C / 0x08A5BC54), then the member at +0x14 when it is not
// null (0x08A5B930) and goes on in 0x08A780B0(object). v0 = its answer.

// 0x224 (0x0899796C, a0 = object): stores the two vtables (0x08CCBE20 at +0, 0x08CCC078 at +0x38), destroys the two
// members at +0x184 and +0x180 (0x08A5B9EC), releases the two 0xC-byte vectors at this+0x164 and this+0x158 in that
// order (0x08BF0F4C / 0x08A5BC54), the member at +0x154 when it is not null, and goes on in 0x08994D78(object).

// The shared body of 0x08B6518C / 0x08B6526C (a0 = object): vtable 0x08CDC170 at +0, the word at +4 from a boolean, the
// byte at +0xC from a boolean that only counts with a non-null pointer, +0x10 / +0x14 zero, the word at +0x18 from the
// pointer (0x08B64CC0() when it is null) and two 0x100-byte zeroed buffers at +0x1D and +0x11D (0x08B36018) with the
// bytes at +0x1C / +0x21D cleared. v0 = the last memset's answer.
void init_two_buffers(Runtime &rt, AllegrexContext &ctx, bool word_at_four, u32 pointer, bool flagged, u32 inherited) {
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CDC170u);
    put(rt, self + 4u, word_at_four ? 1u : 0u);
    put8(rt, self + 0xCu, (pointer != 0u && flagged) ? 1u : 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, pointer != 0u ? pointer : call(rt, ctx, 0x08B64CC0u, {self, inherited}));
    call(rt, ctx, 0x08B36018u, {self + 0x1Du, 0u, 0x100u});
    put8(rt, self + 0x1Cu, 0u);
    call(rt, ctx, 0x08B36018u, {self + 0x11Du, 0u, 0x100u});
    put8(rt, self + 0x21Du, 0u);
}
// 0x224 (0x08B6518C, a0 = object, a1, a2 = the pointer, a3 = a flag byte, t0 = a boolean): the pointer is a2 and the
// flag a3's byte.
void func_08B6518C(Runtime &rt, AllegrexContext &ctx) {
    init_two_buffers(rt, ctx, ctx.gpr[8] != 0u, ctx.gpr[6], (ctx.gpr[7] & 0xFFu) != 0u, ctx.gpr[5]);
}
// 0x224 (0x08B6526C, a0 = object, a1 = the pointer, a2 = a flag byte, a3 = a boolean): a copy of 0x08B6518C whose
// arguments are all one register lower.
void func_08B6526C(Runtime &rt, AllegrexContext &ctx) {
    init_two_buffers(rt, ctx, ctx.gpr[7] != 0u, ctx.gpr[5], (ctx.gpr[6] & 0xFFu) != 0u, ctx.gpr[5]);
}

// 0x228 (0x08B04C78, a0 = object with an element array at +0x44, a1 = an index record with a halfword at +0xEC, a2, a3):
// a negative index answers the word at +0x44 unchanged. Otherwise the entry at word(object+0x44) + 8 * halfword(a1+0xEC)
// is walked: each one with a negative halfword at +2 goes through 0x08B046C0(object, a1, entry, a2) first, a negative
// halfword at +6 ends the walk and otherwise the next entry is word(object+0x44) + 8 * it. The frame's words at +0 ...
// +0x3C are four copies of (the word already at sp+0x10, this+0x44, the entry, 0) - the original reads that slot before
// writing it, and the copies are dead but written, so they are kept. v0 = the word at +0x44, or the halfword at +6.
void func_08B04C78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index_record = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const s32 index = signed_half(rt, index_record + 0xECu);
    if (index < 0) return;
    const u32 base = self + 0x44u;
    u32 stored = word(rt, self + 0x44u);
    u32 current = stored + static_cast<u32>(index) * 8u;
    const u32 value = word(rt, frame.at(0x10u));
    put(rt, frame.at(0x30u), value);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x18u), current);
    put(rt, frame.at(0x14u), base);
    put(rt, frame.at(0x20u), value);
    put(rt, frame.at(0x24u), base);
    put(rt, frame.at(0x28u), current);
    put(rt, frame.at(0x2Cu), 0u);
    put(rt, frame.at(0u), value);
    put(rt, frame.at(4u), base);
    put(rt, frame.at(8u), current);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x34u), base);
    put(rt, frame.at(0x38u), current);
    put(rt, frame.at(0x3Cu), 0u);
    for (;;) {
        if (current == 0u) {
            ctx.gpr[2] = stored;
            return;
        }
        if (signed_half(rt, current + 2u) < 0) call(rt, ctx, 0x08B046C0u, {self, index_record, current, arg2});
        const s32 next = signed_half(rt, current + 6u);
        if (next < 0) {
            ctx.gpr[2] = static_cast<u32>(next);
            return;
        }
        stored = word(rt, self + 0x44u);
        current = stored + static_cast<u32>(next) * 8u;
    }
}

// 0x232 (0x08AFAF44, a0 = object, a1..a3 / t0..t2 as given): the object's virtual +0x14 runs first, then +0x28 is t1
// (cleared when it is below 2), +0xC is t0, t2 goes to the outgoing argument slot at sp and 0x08B52B70(this+0x2C, a1, a2,
// a3, t0, t1, 0, 0) prepares the object. A non-zero answer stores a1 at +4 and a2 at +8 and runs the virtual +0x34 with
// (this, 0, 0, a1, a2). v0 = 1 when it prepared, else 0.
void func_08AFAF44(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 t2 = ctx.gpr[10];
    GuestFrame frame(ctx, 0x30u);
    call_indirect(rt, ctx, word(rt, word(rt, self) + 0x14u),
                  {self, first, second, third, t0, t1, t2, ctx.gpr[11]});
    put(rt, self + 0x28u, static_cast<s32>(t1) < 2 ? 0u : t1);
    put(rt, self + 0xCu, t0);
    put(rt, frame.at(0u), t2);
    const u32 prepared =
        call(rt, ctx, 0x08B52B70u, {self + 0x2Cu, first, second, third, t0, t1, 0u, 0u}) != 0u ? 1u : 0u;
    if (prepared != 0u) {
        put(rt, self + 4u, first);
        put(rt, self + 8u, second);
        call_indirect(rt, ctx, word(rt, word(rt, self) + 0x34u), {self, 0u, 0u, first, second});
    }
    ctx.gpr[2] = prepared;
}

} // namespace
} // namespace new_batch_main_189
} // namespace gt::game

#define NEW189(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_189::name, gt::ReturnKind::kind)
NEW189(0x089ECBACu, func_089ECBAC, Word);
NEW189(0x08A641F4u, func_08A641F4, Word);
NEW189(0x08AFAF44u, func_08AFAF44, Word);
NEW189(0x08B04C78u, func_08B04C78, Word);
NEW189(0x08B6518Cu, func_08B6518C, Word);
NEW189(0x08B6526Cu, func_08B6526C, Word);
NEW189(0x08C65E3Cu, func_08C65E3C, DoubleWord);

// ---- from main-session batch 190 (draft 2c7a3618ad2347fca5cc630e70089b5794637133) ----
// New batch, main session 190: functions nothing had taken (the recompiler's output, none decompiled before), written as
// structured C by hand from `tools.disasm`; meant for verification afterwards. No names: none has evidence yet.
// Grouped by shape where the asm repeats: the conversion-through-a-temporary family (7 sites), the float-field family
// (6 sites) and the text-builder pairs (4 sites); the rest are single bodies.
//
// These addresses already have register-level drafts in other files, all left unregistered by earlier batches for lack of a
// comparable case: hot_batch_224.cpp (0884DDA8, 0884E370, 0884E5F4, 0884E8DC, 088561B0, 088562A8, 088573F8, 08981620,
// 08981718, 0898C440, 0898C538, 0898F454, 0898F54C), hot_batch_227.cpp (08856EF8, 08856FF8, 088570F8, 088572F8),
// hot_batch_88.cpp (0883365C, 08AB9B04 — sceUtility), hot_batch_111.cpp (088AAE08 — compared 0), hot_batch_87.cpp
// (0897BFEC — sceDisplay), hot_leaves.cpp (089D9850). The synthetic input shapes for these bodies live in
// /tmp/claude-1000/shapes_main190.txt (to be merged into config/synth_shapes.txt); the duplicate drafts must be deleted
// when this batch is verified.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace new_batch_main_190 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0/v1/f0/a1 hold what the callee left, as after the original's `jal` (a callee that never writes
// one leaves the caller's value: call_guest starts from this context).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// The same for a `lw v0, slot(vtable); jalr v0` call: v0 = the target on entry (a callee that never writes it returns the
// target), then v0/v1/f0/a1 as the callee left them.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same for a callee taking float arguments ($f12 onward).
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// The callee-saved registers a function keeps values in: stored into its frame where its prologue stores them (a callee can
// read the stack below its caller's frame), given back when it returns.
class Saved {
public:
    Saved(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, std::initializer_list<std::pair<unsigned, u32>> slots)
        : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
        for (const auto &[reg, offset] : slots) put(rt, frame.at(offset), ctx.gpr[reg]);
    }
    ~Saved() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    Saved(const Saved &) = delete;
    Saved &operator=(const Saved &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// --- conversion through a temporary string, 7 sites ---

// 0x0884DDA8 family (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): a lookup into a
// stack temporary (asserted non-null), the conversion of it, then either the count is positive and the source object's
// virtual +0x3C is forwarded to the conversion's finish call, or the temporary is built from the fallback and assigned to
// the output. The temporary is always released; v0 is the last call's result.
void convert_from_source(Runtime &rt, AllegrexContext &ctx, u32 lookup, u32 missing_handle_message, u32 convert,
                         u32 missing_object_message, u32 finish, u32 fallback) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{20u, 0x20u}, {21u, 0x24u}, {19u, 0x1Cu}, {18u, 0x18u},
                                 {16u, 0x10u}, {31u, 0x28u}, {17u, 0x14u}});
    const u32 handle = frame.at(4u);
    call(rt, ctx, lookup, {handle});
    const u32 holder = word(rt, handle);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {missing_handle_message});
    const u32 converted = call(rt, ctx, convert, {holder});
    call(rt, ctx, 0x08A5B9ECu, {handle});
    if (static_cast<s32>(count) > 0) {
        const u32 object = word(rt, source);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {missing_object_message});
        const u32 value = call_virtual(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object}).v0;
        ctx.gpr[2] = call(rt, ctx, finish, {converted, value});
        return;
    }
    const u32 text = call(rt, ctx, fallback, {converted});
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), text});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}


// --- a float field of a looked-up object, 6 sites ---

// 0x08981620 family (a0 = the output string, a1 unused, a2 = a count, a3 = pointer to the source object): with a count of at
// most 0 a float of the looked-up object is formatted and assigned to the output; otherwise the source object's virtual
// +0x54 supplies the float, which is written at +0xF0+write_offset and at +0xF0+second_write_offset of the looked-up
// object (and the flag byte at +0xF0+0xC4 set). v0 is the release call's result.
void update_float_field(Runtime &rt, AllegrexContext &ctx, u32 lookup, u32 read_offset, u32 write_offset,
                        u32 second_write_offset, u32 missing_object_message, u32 missing_source_message) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    if (static_cast<s32>(count) > 0) {
        const u32 handle = frame.at(0u);
        call(rt, ctx, lookup, {handle});
        const u32 holder = word(rt, handle);
        if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {missing_object_message});
        const u32 object = word(rt, source);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {missing_source_message});
        const float value = call_virtual(rt, ctx, word(rt, word(rt, object) + 0x54u), {object}).f0;
        const u32 fields = holder + 0xF0u;
        put8(rt, fields + 0xC4u, 1u);
        put_float(rt, fields + write_offset, value);
        put_float(rt, fields + second_write_offset, value);
        call(rt, ctx, 0x08A5B9ECu, {handle});
        return;
    }
    const u32 handle = frame.at(4u);
    call(rt, ctx, lookup, {handle});
    const u32 object = word(rt, handle);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {missing_object_message});
    const float value = float_at(rt, object + read_offset);
    const u32 text = frame.at(8u);
    call_floats(rt, ctx, 0x08A87EECu, {text}, {value});
    if (out != text) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, text)});
    call(rt, ctx, 0x08A5B9ECu, {text});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {handle});
}


// --- text-builder pairs, 4 sites ---

// a0 = the text, a2 = 0 for the read shape and nonzero for the write one, a3 = a pointer to the holder: read forms the text
// with `read_fn`, write stores the holder's virtual +0x3C result with `write_fn`. Both builders start at 0x088547C4 (the
// string 0x08C9E188 reports a missing handle, 0x08C9E14C a missing object).
void text_pair(Runtime &rt, AllegrexContext &ctx, u32 read_fn, u32 write_fn) {
    const u32 text = ctx.gpr[4];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {31u, 0x1Cu}, {16u, 0x10u}});
    if (ctx.gpr[6] != 0u) {
        const u32 handle = ctx.gpr[29];
        call(rt, ctx, 0x088547C4u, {handle});
        const u32 owner = word(rt, handle);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});
        const u32 object = call(rt, ctx, 0x08854678u, {owner});
        const u32 subject = word(rt, holder);
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9E14Cu});
        const u32 value = call_virtual(rt, ctx, word(rt, word(rt, subject) + 0x3Cu), {subject}).v0;
        call(rt, ctx, write_fn, {object, value});
        call(rt, ctx, 0x08A5B9ECu, {handle});
        return;
    }
    const u32 handle = frame.at(4u);
    call(rt, ctx, 0x088547C4u, {handle});
    const u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});
    const u32 object = call(rt, ctx, 0x08854678u, {owner});
    const u32 value = call(rt, ctx, read_fn, {object});
    const u32 appended = frame.at(8u);
    call(rt, ctx, 0x08A85B1Cu, {appended, value});
    if (text != appended) call(rt, ctx, 0x08A5BAD8u, {text, word(rt, appended)});
    call(rt, ctx, 0x08A5B9ECu, {appended});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {handle});
}


// 0x08B05500 (a0 = the record): two indices (the signed halves at +0x1C and +0x1E) select 0xF0-byte elements of the table
// the word at +0x24 points to; the second one walks the chain (each element's +0xE8 half indexes the next) calling
// 0x08B05444 on every element. The function also lays out a 0x40-byte iterator on its stack (never read by anyone: the
// callee's frame lies below it) and v0 ends as the last call's, the failing element's index, the table word, or the
// stack word at +0 that the asm loads before the walk.
void func_08B05500(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    Saved saved(rt, ctx, frame, {{31u, 0x48u}, {17u, 0x44u}, {16u, 0x40u}});
    const s32 first_index = signed_half(rt, self + 0x1Cu + 0x2u);  // the asm's `lh 0x2($a2)` with $a2 = a0 + 0x1C
    u32 first = 0u;
    if (first_index >= 0) {
        const u32 table_holder = word(rt, self + 0x24u);
        const u32 index = static_cast<u32>(first_index);
        first = word(rt, table_holder) + ((index << 8) - (index << 4));
    }
    const s32 second_index = signed_half(rt, self + 0x1Cu);
    const u32 table_holder = word(rt, self + 0x24u);
    const u32 stale_high = word(rt, frame.at(0x10u));
    put(rt, frame.at(0x2Cu), first);
    put(rt, frame.at(0x20u), stale_high);
    put(rt, frame.at(0x1Cu), first);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x14u), table_holder);
    put(rt, frame.at(0x24u), table_holder);
    put(rt, frame.at(0x28u), 0u);
    u32 second = 0u;
    if (second_index >= 0) {
        const u32 index = static_cast<u32>(second_index);
        second = word(rt, table_holder) + ((index << 8) - (index << 4));
    }
    const u32 stale_low = word(rt, frame.at(0u));
    put(rt, frame.at(0x0Cu), 0u);
    put(rt, frame.at(0x30u), stale_low);
    put(rt, frame.at(0x08u), second);
    put(rt, frame.at(0x04u), table_holder);
    put(rt, frame.at(0x34u), table_holder);
    put(rt, frame.at(0x38u), second);
    put(rt, frame.at(0x3Cu), 0u);
    ctx.gpr[2] = stale_low;
    while (second != 0u) {
        ctx.gpr[4] = second;
        call(rt, ctx, 0x08B05444u, {second});
        const s32 next_index = signed_half(rt, second + 0xE8u);
        ctx.gpr[2] = static_cast<u32>(next_index);
        if (next_index < 0) break;
        const u32 base = word(rt, table_holder);
        ctx.gpr[2] = base;
        const u32 index = static_cast<u32>(next_index);
        second = base + ((index << 8) - (index << 4));
    }
}

// 0x08AB9B04 (no arguments): walks the global list at 0x08D7717C and inserts each node's record into the global table at
// 0x08CFFA10 (before the first entry whose +8 value is not below the node's), then runs +0x10 on every entry and tail-calls
// 0x08ABA264, which repeats the walk over the table at 0x08CFFA20.

// 0x0887C6AC (a0 = the new object, a1 = a value it keeps at +0): allocates a 0x368-byte container, stores a1 at its +0 and
// clears its 24 and 48 twelve-byte records (each through 0x08AC26A4 with a zero) and the two halves at +0x364/+0x366; the
// container goes to a0+4 and the lazily built global object at 0x08D75400 (guard byte 0x08D753F8, vtable 0x08CBFED8) to
// a0+8. v0 is the guard release's result, or 0x000A0000 when the object already existed.

// 0x089D9850 (a0 = destination object, a1 = pointer to the source object): builds a stack wrapper (vtable 0x08CCD9D8, the
// shared reference block, -1 at +0x10, its own address at +0x14) on the source's virtual +8, then runs 0x089D86E8 with the
// destination and the wrapper, releasing both reference blocks the way the smart-pointer temporaries are. v0 is the last
// release call's result, else the wrapper's first word.

// 0x0883365C (a0 = the output string): converts a looked-up object (0x088297A8/0x088294D0, asserted non-null) through
// 0x0885EE70, asks 0x08891608 whether there is anything, allocates 0x08892138's size with 0x08B2B5E0, fills it with
// 0x08B79A88 from 0x088915E0's count and formats it with 0x08A44D9C. v0 = 0 when there was nothing, else the release.

// 0x088AAE08 (a0 = self, a1 = second, a2 = third): fills a 0x20-byte local (0x088A6088, 0x088A0DD4), runs 0x08BAC9EC on the
// object's +0x4C, and when it produced an object decrements the counters at +4 and +8 (stopping at one the first time) and
// runs its virtuals +8 and +0xC. v0 = 0 without an object, 1 when a counter was not the last, else the last virtual's.

// 0x0897BFEC (a0 unused, a1 unused, a2 = count, a3 = argument): a lookup at 0x08978000 into sp+8; with a positive count also
// one at 0x08A5D2D4 into sp+4, whose object's virtual +0xC4 receives a temporary string (0x089788A8/0x08A85B1C); then
// 0x0897BC08 on the first lookup and its release. v0 = that release.

} // namespace
} // namespace new_batch_main_190
} // namespace gt::game

#define NEW190(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_190::name, gt::ReturnKind::kind)
NEW190(0x08B05500u, func_08B05500, Word);

// ---- from main-session batch 191 (draft 971b69cb6591afb85c667795f63c1bacf408e80a) ----
// New batch 191 (main session): two one-off functions no src/ file defines, plus the 21 members of the script-attribute
// accessor family at 0x08851968 (drafted before in new_batch_main_20 and in hot_batch_135/156, but never registered
// there because no synthetic case compared: "no comparable synthetic case yet"). Written by hand from `tools/disasm`
// as structured C; a draft batch, not yet verified by the differential gate.
//  - 0x088BC21C: the schedule keep-alive/countdown step (float distance test plus a tail call into 0x088BB5A0);
//  - 0x08AC30B4: the random-value queue append (bounded retry against the 64-entry ring at +0xD2C);
//  - the accessor family: one member per field, all byte-identical except the field offset and its width.
// Input shapes for the synthetic generator: /tmp/claude-1000/shapes_main191.txt (a3 holds the object holder, a0 the
// queue object). Audits: frame_audit 0 wrong, literal_check 0 missing, arg_reads 0; sp_loads lists only the handle
// words 0x088547C4 leaves at frame + 4 / + 8, which the body already models as word(rt, frame.at(4u / 8u)) — not a
// hidden spill. NOTE for the verifier: hot_batch_135.cpp / hot_batch_156.cpp already define these 21 addresses with
// their `GT_DECOMP` lines commented out; keep those commented (one registration per address) if this batch's are
// enabled.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_191 {
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

// --- the attribute accessors (one per field) ---
// a0 = the text a read goes into, a2 = 0 to read and > 0 to write, a3 = a pointer to the holder of the object the
// attribute belongs to, a1 unused. Write: 0x088547C4 builds a handle at frame + 8 (0x08C9D3BC reports a missing one),
// 0x08854678 hands out the container, 0x088A6608 the object, and the virtual +0x3C of the holder's object supplies the
// value stored in the field; v0 = that value. Read: the same through 0x088A65E8, the field is appended to a temporary
// text at frame + 0 by 0x08A85B1C, assigned over a0 by 0x08A5BAD8 when a0 is not that temporary, and released by
// 0x08A5B9EC; v0 = the release's result.
void attribute_accessor(Runtime &rt, AllegrexContext &ctx, u32 field, bool half) {
    GuestFrame frame(ctx, 0x20u);
    const u32 text = ctx.gpr[4];
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 handle = frame.at(8u);
        call(rt, ctx, 0x088547C4u, {handle});
        const u32 owner = word(rt, handle);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
        const u32 container = call(rt, ctx, 0x08854678u, {owner});
        call(rt, ctx, 0x08A5B9ECu, {handle});
        const u32 object = call(rt, ctx, 0x088A6608u, {container});
        const u32 subject = word(rt, holder);
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D394u});
        const u32 value = call_indirect(rt, ctx, word(rt, word(rt, subject) + 0x3Cu), {subject});
        if (half) rt.memory().store16(object + field, static_cast<std::uint16_t>(value));
        else rt.memory().store8(object + field, static_cast<std::uint8_t>(value));
        ctx.gpr[2] = value;
        return;
    }
    const u32 handle = frame.at(4u);
    call(rt, ctx, 0x088547C4u, {handle});
    const u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
    const u32 container = call(rt, ctx, 0x08854678u, {owner});
    call(rt, ctx, 0x08A5B9ECu, {handle});
    const u32 object = call(rt, ctx, 0x088A65E8u, {container});
    const u32 value = half ? rt.memory().load16(object + field) : rt.memory().load8(object + field);
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), value});
    const u32 temporary = word(rt, frame.at(0u));
    if (text != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {text, temporary});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}


// 0x088BC21C (a0 = object): a countdown kept at +0xA3 of the state the object points at in +0x24. While the object's
// +0x2C is flagged odd, the count grows by one up to 180 when the squared distance between its (+0x14, +0x1C) and the
// (+0x10, +0x14) of the object at self + 8 reaches 10000, and shrinks by one below that; the other paths borrow from
// the count, dropping it to zero below. Three early exits zero the count, and the function ends in 0x088BB5A0(state)
// (a jump, so ra is the caller's) whenever the count was already zero or just reached it. The asm also leaves
// v1 = self, a1 = the flagged object and a2 = self + 4 as the function returns; the tail callee reads only a0.
void func_088BC21C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 state = word(rt, self + 0x24u);
    const u32 object = word(rt, self + 0x2Cu);
    ctx.gpr[3] = self;
    ctx.gpr[5] = object;
    ctx.gpr[6] = self + 4u;
    if (rt.memory().load8(state + 0xFu) != 0u || rt.memory().load8(state + 0x6Du) != 0u ||
        rt.memory().load8(state + 0x6Eu) >= 0x3Cu) {
        rt.memory().store8(state + 0xA3u, 0u);
        tail_call(ctx, 0x088BB5A0u, {state});
        return;
    }
    if ((word(rt, object) & 1u) != 0u) {
        const u32 other = word(rt, self + 8u);
        const float along = float_at(rt, object + 0x1Cu) + float_at(rt, other + 0x14u);
        const float across = float_at(rt, object + 0x14u) - float_at(rt, other + 0x10u);
        const float along2 = along * along;
        const float across2 = across * across;
        const float reached = 10000.0f;
        if (reached <= across2 + along2) {
            const u32 count = rt.memory().load8(state + 0xA3u);
            if (count >= 180u) {
                ctx.gpr[2] = count;
                return;
            }
            ctx.gpr[2] = count + 1u;
            rt.memory().store8(state + 0xA3u, static_cast<std::uint8_t>(count + 1u));
            return;
        }
    }
    const u32 count = rt.memory().load8(state + 0xA3u);
    ctx.gpr[2] = count != 0u ? count - 1u : 0u;
    if (count != 0u) {
        rt.memory().store8(state + 0xA3u, static_cast<std::uint8_t>(count - 1u));
        if (count != 1u) return;
    }
    tail_call(ctx, 0x088BB5A0u, {state});
}

// 0x08AC30B4 (a0 = the queue object): a queue of 64 u16 values at +0xD2C with its head at +0xCA0 and its length at
// +0xCA4, fed from the session value at +0xC9C. The length first drops to half the session value when that is smaller
// (the asm stores it back unconditionally), then 0x08B1EFBC(self + 0x14, 0.0f, (float)session) — the random state's
// fraction times the session value — is drawn and truncated, and redrawn until the ring's `length` entries starting at
// (head - length) & 0x3F do not already hold it. The value is appended at the head, length and head advance, and
// v0 = the value (v1 = the new length, as the asm leaves it).
void func_08AC30B4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 state = self + 0x14u;
    const s32 session = static_cast<s32>(word(rt, self + 0xC9Cu));
    const u32 half = static_cast<u32>(session >> 1);
    u32 count = word(rt, self + 0xCA4u);
    if (static_cast<s32>(half) < static_cast<s32>(count)) count = half;
    put(rt, self + 0xCA4u, count);
    const u32 start = (word(rt, self + 0xCA0u) - count) & 0x3Fu;
    s32 value;
    for (;;) {
        const GuestResult drawn = call_guest(rt, ctx, 0x08B1EFBCu, {state}, {0.0f, static_cast<float>(session)});
        value = static_cast<s32>(std::trunc(drawn.f0));
        bool held = false;
        if (static_cast<s32>(count) > 0) {
            for (u32 offset = 0u; offset != count; ++offset) {
                const u32 entry = rt.memory().load16(self + 0xD2Cu + 2u * ((start + offset) & 0x3Fu));
                if (entry == static_cast<std::uint16_t>(value)) {
                    held = true;
                    break;
                }
            }
        }
        if (!held) break;
    }
    const u32 head = word(rt, self + 0xCA0u);
    put(rt, self + 0xCA4u, count + 1u);
    rt.memory().store16(self + 0xD2Cu + 2u * (head & 0x3Fu), static_cast<std::uint16_t>(value));
    put(rt, self + 0xCA0u, (head + 1u) & 0x3Fu);
    ctx.gpr[3] = count + 1u;
    ctx.gpr[2] = static_cast<u32>(value);
}
} // namespace
} // namespace new_batch_main_191
} // namespace gt::game

#define NEW191(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_191::name, gt::ReturnKind::kind)
NEW191(0x088BC21Cu, func_088BC21C, Word);
NEW191(0x08AC30B4u, func_08AC30B4, Word);

// ---- from main-session batch 192 (draft 25e434f47d76911a1c500055a3433f47e980d5d6) ----
// New batch 192 (main session): script-attribute accessors, convert-and-assign members and two destructors that no route
// reaches, written as structured C from `tools/disasm` and meant for verification by synthetic cases. The input shapes
// the generator needs are listed per address in /tmp/claude-1000/shapes_main192.txt (format: config/synth_shapes.txt).
//  - 0x088530C4..0x08853AEC: eleven unsigned-halfword script attributes (one accessor each): a0 = the text, a2 = 0 to
//    read and > 0 to write, a3 = a pointer to the holder of the object the attribute belongs to; the same shape as
//    batch 20's attribute accessors;
//  - 0x0884A07C..0x0884B8C0: eight members of one class's convert-and-assign family: a2 > 0 converts and assigns,
//    a2 <= 0 reads the value into the caller's text;
//  - 0x089D05FC / 0x08A9DFEC: destructors that release their members, then their allocator blocks;
//  - 0x08AEED10: pushes a seven-word record into the ring buffer at 0x08D41AD0 under its semaphore.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace new_batch_main_192 {
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

// The callee-saved registers a function keeps values in: stored into its frame where its prologue stores them (a callee
// can read the stack below its caller's frame), given back when it returns.
class Saved {
public:
    Saved(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, std::initializer_list<std::pair<unsigned, u32>> slots)
        : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
        for (const auto &[reg, offset] : slots) put(rt, frame.at(offset), ctx.gpr[reg]);
    }
    ~Saved() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    Saved(const Saved &) = delete;
    Saved &operator=(const Saved &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// --- the attribute accessor family (0x088530C4 .. 0x08853AEC) --------------------------------------------------
// a0 = the text a read goes into, a2 = 0 to read and > 0 to write, a3 = a pointer to the holder of the object the
// attribute belongs to. Write: build a handle at sp+8 (0x088547C4, the string 0x08C9D3BC reports a missing one), take the
// object 0x08854678 hands out, ask 0x088A6608 for the attribute's storage, and store the holder's virtual +0x3C result
// (a halfword) in it with 0x08C9D394 reporting a missing holder. Read: the same through 0x088A65E8, the halfword goes to
// 0x08A85B1C (a0 = sp) and 0x08A5BAD8 assigns the text over a0. v0 = the virtual's result on the write path, else the
// last call's.
void attribute_accessor(Runtime &rt, AllegrexContext &ctx, u32 field) {
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {31u, 0x1Cu}});
    const u32 text = ctx.gpr[4];
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 handle = frame.at(8u);
        ctx.gpr[17] = handle;
        call(rt, ctx, 0x088547C4u, {handle});
        const u32 owner = word(rt, handle);
        ctx.gpr[16] = owner;
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
        const u32 middle = call(rt, ctx, 0x08854678u, {owner});
        ctx.gpr[16] = middle;
        call(rt, ctx, 0x08A5B9ECu, {handle});
        const u32 storage = call(rt, ctx, 0x088A6608u, {middle});
        ctx.gpr[17] = storage;
        const u32 subject = word(rt, holder);
        ctx.gpr[16] = subject;
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D394u});
        const u32 vtable = word(rt, subject);
        ctx.gpr[3] = vtable;
        const u32 value = call_indirect(rt, ctx, word(rt, vtable + 0x3Cu), {subject});
        rt.memory().store16(storage + field, static_cast<std::uint16_t>(value));
        ctx.gpr[2] = value;
        return;
    }
    const u32 handle = frame.at(4u);
    ctx.gpr[18] = handle;
    call(rt, ctx, 0x088547C4u, {handle});
    const u32 owner = word(rt, handle);
    ctx.gpr[16] = owner;
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
    const u32 middle = call(rt, ctx, 0x08854678u, {owner});
    ctx.gpr[16] = middle;
    call(rt, ctx, 0x08A5B9ECu, {handle});
    const u32 storage = call(rt, ctx, 0x088A65E8u, {middle});
    const u32 value = rt.memory().load16(storage + field);
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), value});
    if (text != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {text, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}


// --- the convert-and-assign family (0x0884A07C .. 0x0884B8C0) -------------------------------------------------
// a0 = the output text/object, a1 = a second argument the builder takes, a2 > 0 to assign and <= 0 to read, a3 = a pointer
// to the holder. Both paths build a temporary at sp+8 (assign) or sp+4 (read) with 0x08847008, convert its object with
// 0x08846E50 and release the temporary with 0x08A5B9EC.
//  Assign: the holder's object runs its virtual +0x3C and its answer goes to `set` with the converted value; `flag`
//  selects the answer itself or its truth (a1 = v0, or sltu a1, zero, v0).
//  Read: the converted value goes to `get`, the text at sp is built with 0x08A85B1C, 0x08A5BAD8 assigns it over a0 when
//  a0 is not sp, and the text is released. v0 = the last call's result on that path.
void convert_and_assign(Runtime &rt, AllegrexContext &ctx, u32 set, u32 get, bool flag) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {19u, 0x1Cu}, {31u, 0x20u}});
    const u32 output = ctx.gpr[4];
    const s32 mode = static_cast<s32>(ctx.gpr[6]);
    const u32 holder = ctx.gpr[7];
    ctx.gpr[17] = output;
    if (mode > 0) {
        const u32 temporary = frame.at(8u);
        ctx.gpr[19] = temporary;
        call(rt, ctx, 0x08847008u, {temporary, ctx.gpr[5]});
        const u32 object = word(rt, temporary);
        ctx.gpr[16] = object;
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
        const u32 converted = call(rt, ctx, 0x08846E50u, {object});
        ctx.gpr[17] = converted;  // addu s1, v0 sits in the destructor call's delay slot
        call(rt, ctx, 0x08A5B9ECu, {temporary});
        const u32 subject = word(rt, holder);
        ctx.gpr[16] = subject;
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
        const u32 vtable = word(rt, subject);
        ctx.gpr[3] = vtable;
        const u32 result = call_indirect(rt, ctx, word(rt, vtable + 0x3Cu), {subject});
        const u32 answer = flag ? (result != 0u ? 1u : 0u) : result;
        call(rt, ctx, set, {converted, answer});
        return;
    }
    const u32 temporary = frame.at(4u);
    ctx.gpr[18] = temporary;
    call(rt, ctx, 0x08847008u, {temporary, ctx.gpr[5]});
    const u32 object = word(rt, temporary);
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
    const u32 converted = call(rt, ctx, 0x08846E50u, {object});
    ctx.gpr[16] = converted;
    call(rt, ctx, 0x08A5B9ECu, {temporary});
    const u32 text = call(rt, ctx, get, {converted});
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), text});
    if (output != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}


// --- the destructors ------------------------------------------------------------------------------------------
// 0x089D05FC (a0 = this): stores the two vtables (0 and +0x38), releases the four handles at +0x188..+0x17C, then walks the
// two 0xC-byte elements at +0x16C and +0x160 downwards from end (+0x178) to begin (+0x160) releasing each one's block,
// releases +0x154, then the block at +0x150 (unless it is the empty sentinel 0x08D74FD0) and finally 0x08995714(this).

// 0x08A9DFEC (a0 = this): stores the vtable at 0, tells 0x08A9DCB0(1), releases +0x78, +0x74, +0x4C, then the allocator
// blocks at +0x48 and +0x44 (each unless it is the empty sentinel 0x08D74FD0; 0x08B8542C takes the block with sp+1 or sp as
// its second argument), the handles at +0x40, +0x3C, +0x38, +0x34, then +0x2C and +0x24, the pair at +0xC (+0x14 as the
// second argument), and finally this to 0x08A5BB5C.

// 0x08AEED10 (a0, a1, a2, a3, t0, t1, t2): under the semaphore at global +0x4F4 (locked with (sema, 1, 0)) the seven
// arguments are stored at entry `global +0x500 + (index << 5)` of a 32-byte record ring whose length is the word at +0x504,
// with a zero closing word; the index (+0x508) wraps, the counter at +0x510 is bumped, the semaphore at +0x4F8 is signalled
// and the one at +0x4F0 last, whose result is v0. A zero ring length makes the original break (div by zero).
} // namespace
} // namespace new_batch_main_192
} // namespace gt::game

#define NEW192(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_192::name, gt::ReturnKind::kind)

