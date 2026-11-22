// New batch main-session 15: 80 functions the recompiler emitted, written by hand as structured C from `tools.disasm`. Draft,
// unverified: the writer does not run the gate (docs/agents/decomp-workflow.md). 79 are written and registered here; 0x08B2E620
// is left out because four of its calls read argument registers only the previous callee set and its `jalr` argument is
// whatever a 99-instruction function left behind.
//
// 77 of the 80 already had an unregistered, register-level (one statement per instruction) draft in another batch file, with its
// registration line commented out - "HOTnnn: compared 0 / no comparable synthetic case yet" - so none of them is verified yet.
// Those addresses, which are not registered anywhere else (checked), are registered here with this structured version; the old
// drafts are another session's work and were left untouched:
//   hot_batch_86  08900350 0892FCE0 08A13844
//   hot_batch_88  088941A0 088D7158
//   hot_batch_85  08AD5460
//   hot_batch_67  08B8CB18 08BEB5E0 08BF2A78 08BF41D0 08BF6664 08BFDA8C 08BFF398 08C0567C 08C05BCC 08C07970
//                 08C0A460 08C0BC20 08C1E858 (getter_then_setter_value), hot_batch_77: the float twin ptmf_call_float
//                 (08B8CDC4 08BF02D4 08BFC5C4 08C00610 08C076C8 08C08958 08C0BECC 08C10644 08C1344C)
//   hot_batch_100 08B0EC60 08BE834C (also hot_batch_82 / hot_leaves), hot_batch_110: 08877354
//   hot_batch_212 0892A2C0 089B8DA0 089C946C 08A737EC 08A96560 08AB06CC 08AB1E50 08B10A7C 08B1A338 08C5D020
//                 08C5D104 08C7A064
//   hot_batch_213 088618E8 088756F0 08877724 088777E4 088778A4 088798D0 08975F1C 08A514DC 08A5FE6C 08A7372C
//                 08A964A0 08AB060C 08AB1D90 08ACBDF0 08ACBEB0 08BEBBF8 08C08D5C 08C13170 08C5518C
//   hot_batch_214 088212F8 0882D4BC 0884B9D0 088E4DFC 088E58DC 088EBBF8 088ECCD4 0898E358 0899DDA0
//                 0899DE64 08A58394 08AB1F0C 08B0C8D8 08B48398 08C122C8
// Only 0x08AD9C18 (a jump-table dispatch) and 0x08B5C154 (a category switch with an assert path) are new in this file. The
// synthetic input shapes that go with the batch are in /tmp/claude-1000/shapes_main15.txt (one line per function, 61 of the 79).
//
// Three shapes repeat and are written once as a template with the member's own constants (each member still registers and runs
// on its own):
//   * `tagged_int` / `tagged_float` / `tagged_prepare` - a delegate: a0 is the invoker, a1 points at an index, a2 the value.
//     The invoker holds a tag at +0x18 (>>1, bit 0 = "go through the object's vtable") and a base at +0x14; the same pair at
//     +0x28/+0x24 selects the function the tail call goes to. Nine registrations share the float variant, thirteen the int one.
//   * `divide_through` - a field of a0 at +0x14 divided by a virtual getter's result, turned into a remainder or a quotient
//     (mflo / mfhi of the `div`), wrapped by a stack builder and released. Nine members.
//   * `copy_pair` (0x08ACBDF0 / 0x08ACBEB0) - lock, allocate a descriptor, fill a stack request, chain it, release.
//
// Rules followed here: a `jal` / `jalr` delay slot runs BEFORE the call (arguments are computed before the call, stores the
// delay slot performs happen before it); a `jalr v0` hands the callee its own address in v0 (call_slot), a `jalr rs` does not
// (the callee sees the caller's v0); arithmetic shifts stay arithmetic (`>> 1` on the signed value); every `div` reproduces the
// recompiled original's lo/hi, a zero divisor included, though on a zero divisor the original's `break` stops its run (the
// harness calls that case not comparable). v0 is set where the asm leaves it, on every exit path.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace new_batch_main_15 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using GuestResult = gt::GuestResult;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; v0 afterwards is the callee's, as after the original's `jal`. The callee starts from ctx, so a register this
// call does not pass keeps whatever the decompiled caller last put there, like the original's.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with float arguments and the callee's v0 / v1 / f0 left in the context (a 64-bit result pair, a float result).
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                  std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v1, 0(object); lw v0, offset(v1); jalr v0`: v1 holds the vtable and v0 the slot's address when the callee starts.
GuestResult call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + offset);
    return callf(rt, ctx, ctx.gpr[2], arguments, {});
}

u32 shr1(u32 value) { return static_cast<u32>(static_cast<s32>(value) >> 1); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
// `ins rd, zero, 0, 4`: the low four bits cleared, which is what the original's bitfield insert computes.
u32 cleared_low_nibble(u32 value) { return value & ~0xFu; }
u32 load16(Runtime &rt, u32 address) { return rt.memory().load16(address); }
u32 load8(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// `div` / `divu` as the recompiled original computes lo and hi (a zero divisor keeps the MIPS fallback; the original's
// generated `break` guard stops its own run there, so such a case is not comparable).
u32 signed_quotient(s32 dividend, s32 divisor) {
    if (divisor == 0) return dividend >= 0 ? 0xFFFFFFFFu : 1u;
    if (dividend == static_cast<s32>(0x80000000u) && divisor == -1) return 0x80000000u;
    return static_cast<u32>(dividend / divisor);
}
u32 signed_remainder(s32 dividend, s32 divisor) {
    if (divisor == 0) return static_cast<u32>(dividend);
    if (dividend == static_cast<s32>(0x80000000u) && divisor == -1) return 0u;
    return static_cast<u32>(dividend % divisor);
}
u32 unsigned_quotient(u32 dividend, u32 divisor) {
    if (divisor == 0u) return 0xFFFFFFFFu;
    return dividend / divisor;
}
u32 unsigned_remainder(u32 dividend, u32 divisor) {
    if (divisor == 0u) return dividend;
    return dividend % divisor;
}

// -------------------------------------------------------------------------------------------------------------------------
// Delegate: a1 points at an index, a2 is the value. +0x18 / +0x14 hold the first dispatch's tag and base, +0x28 / +0x24 the
// second's. Both dispatches look the same: when bit 0 of the tag is set the target is the vtable slot `base` of the object the
// adjusted pointer addresses, otherwise `base` is the plain function address. The second one is a tail call (the frame the
// original popped first), so `fn` and v0 / v1 as the original leaves them before the jump.
struct Dispatch {
    u32 function;   // the address the tail call / indirect call goes to
    u32 pointer;    // the adjusted pointer (the tail call's first argument)
    u32 v0;         // what the original holds in v0 when the second call starts
    u32 v1;         // and in v1
};
Dispatch second_dispatch(Runtime &rt, u32 invoker, u32 pointer) {
    const u32 tag = word(rt, invoker + 0x28u);
    const u32 half = shr1(tag);
    const u32 at = pointer + half;
    if ((tag & 1u) == 0u) return {word(rt, invoker + 0x24u), at, 0u, half};
    const u32 base = word(rt, invoker + 0x24u);
    const u32 sum = word(rt, at) + base;
    return {word(rt, sum), at, sum, base};
}
u32 first_dispatch(Runtime &rt, AllegrexContext &ctx, u32 invoker, u32 index) {
    const u32 tag = word(rt, invoker + 0x18u);
    const u32 base = word(rt, invoker + 0x14u);
    const u32 at = index + shr1(tag);
    if ((tag & 1u) == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = base;
        return callf(rt, ctx, base, {at}, {}).v0;
    }
    const u32 table = word(rt, at);
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = word(rt, table + base);
    return callf(rt, ctx, ctx.gpr[2], {at}, {}).v0;
}
void tagged_int(Runtime &rt, AllegrexContext &ctx, u32 second_call) {
    GuestFrame frame(ctx, 0x10u);
    const u32 invoker = ctx.gpr[4];
    const u32 value_argument = ctx.gpr[6];
    const u32 result = first_dispatch(rt, ctx, invoker, word(rt, ctx.gpr[5]));
    const Dispatch target = second_dispatch(rt, invoker, result);
    ctx.gpr[2] = target.v0;
    ctx.gpr[3] = target.v1;
    const u32 value = call(rt, ctx, second_call, {value_argument});
    ctx.gpr[25] = target.function;
    tail_call(ctx, target.function, {target.pointer, value});
}
void tagged_float(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 invoker = ctx.gpr[4];
    const u32 value_argument = ctx.gpr[6];
    const u32 result = first_dispatch(rt, ctx, invoker, word(rt, ctx.gpr[5]));
    const Dispatch target = second_dispatch(rt, invoker, result);
    ctx.gpr[2] = target.v0;
    ctx.gpr[3] = target.v1;
    const GuestResult value = callf(rt, ctx, 0x08B8C5D4u, {value_argument}, {});
    ctx.gpr[25] = target.function;
    ctx.fpr[12] = value.f0;
    tail_call(ctx, target.function, {target.pointer});
}
// The same dispatch, but the second step hands a stack temporary to a builder first and calls the target with (pointer, &temporary)
// from a real frame (so the caller's frame is back before returning, no tail call).
void tagged_prepare(Runtime &rt, AllegrexContext &ctx, u32 builder, u32 frame_size) {
    GuestFrame frame(ctx, frame_size);
    const u32 invoker = ctx.gpr[4];
    const u32 value_argument = ctx.gpr[6];
    const u32 result = first_dispatch(rt, ctx, invoker, word(rt, ctx.gpr[5]));
    const Dispatch target = second_dispatch(rt, invoker, result);
    ctx.gpr[2] = target.v0;
    ctx.gpr[3] = target.v1;
    call(rt, ctx, builder, {frame.at(0u), value_argument});
    callf(rt, ctx, target.function, {target.pointer, frame.at(0u)}, {});
}


// -------------------------------------------------------------------------------------------------------------------------
// Divide a field of a0 by a virtual getter's answer. `holder` (a3) is the object whose slot `vtable_offset` gives both the
// dividend's source (queried twice, guarded when null) and the divisor; the field at +0x14 of a0 is loaded in the second guard
// branch's delay slot. The result is built on the stack (builder writes the word at sp+0), given to a1 when a1 is not that very
// stack slot, and released.
enum class Field { Word, SignedHalf, UnsignedHalf, SignedByte, UnsignedByte };

u32 field_at(Runtime &rt, u32 address, Field kind) {
    switch (kind) {
    case Field::Word: return word(rt, address);
    case Field::SignedHalf: return sext16(load16(rt, address));
    case Field::UnsignedHalf: return load16(rt, address);
    case Field::SignedByte: return sext8(load8(rt, address));
    case Field::UnsignedByte: return load8(rt, address);
    }
    return 0u;
}

void divide_through(Runtime &rt, AllegrexContext &ctx, u32 vtable_offset, u32 guard_text, Field kind, bool is_signed,
                    bool quotient, u32 builder) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    u32 holder = word(rt, ctx.gpr[7]);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    call_slot(rt, ctx, holder, vtable_offset, {holder});
    holder = word(rt, ctx.gpr[7]);
    const u32 dividend = field_at(rt, self + 0x14u, kind);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    const u32 divisor = call_slot(rt, ctx, holder, vtable_offset, {holder}).v0;
    u32 value;
    if (quotient) {
        value = is_signed ? signed_quotient(static_cast<s32>(dividend), static_cast<s32>(divisor))
                          : unsigned_quotient(dividend, divisor);
    } else {
        value = is_signed ? signed_remainder(static_cast<s32>(dividend), static_cast<s32>(divisor))
                          : unsigned_remainder(dividend, divisor);
    }
    call(rt, ctx, builder, {frame.at(0u), value});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

void func_08A737EC(Runtime &rt, AllegrexContext &ctx) { divide_through(rt, ctx, 0x38u, 0x08CB0BA8u, Field::SignedHalf, true, false, 0x08A85B1Cu); }
void func_08A7372C(Runtime &rt, AllegrexContext &ctx) { divide_through(rt, ctx, 0x38u, 0x08CB0BA8u, Field::SignedHalf, true, true, 0x08A85B1Cu); }
void func_08A96560(Runtime &rt, AllegrexContext &ctx) { divide_through(rt, ctx, 0x34u, 0x08CB21E8u, Field::SignedByte, true, false, 0x08A85B1Cu); }
void func_08A964A0(Runtime &rt, AllegrexContext &ctx) { divide_through(rt, ctx, 0x34u, 0x08CB21E8u, Field::SignedByte, true, true, 0x08A85B1Cu); }
void func_08AB06CC(Runtime &rt, AllegrexContext &ctx) { divide_through(rt, ctx, 0x48u, 0x08CB3A78u, Field::UnsignedHalf, false, false, 0x08A85B1Cu); }
void func_08A5FE6C(Runtime &rt, AllegrexContext &ctx) { divide_through(rt, ctx, 0x4Cu, 0x08CB00F8u, Field::Word, false, true, 0x08A5FA8Cu); }

// -------------------------------------------------------------------------------------------------------------------------
// 0x08ACBDF0 / 0x08ACBEB0 (a1 = the request text, a2 = 0): take a global lock (0x08D69170), build a stack request with its mode,
// chain it through 0x08ACBC78, release. Returns 0 when the lock or the chain refuses, else 0x08ACBC78's answer.
void copy_pair(Runtime &rt, AllegrexContext &ctx, u32 mode) {
    GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 descriptor = frame.at(0xCu);
    const u32 held = call(rt, ctx, 0x08B31990u, {0x08D69170u, text});
    if (held == 0u) {
        if (call(rt, ctx, 0x08B327ACu, {0x08D69170u, 2u}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    if (call(rt, ctx, 0x08ACB910u, {self, descriptor}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, frame.at(0x3Cu), mode);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(8u), 0u);
    if (text != 0u) call(rt, ctx, 0x08B7C830u, {frame.at(0u), text, 8u});
    put(rt, frame.at(0x40u), frame.at(0u));
    ctx.gpr[2] = call(rt, ctx, 0x08ACBC78u, {self, descriptor});
}

// -------------------------------------------------------------------------------------------------------------------------
// 0x088D7158 (a0 = object, a1 = a byte): two precondition checks, a flag byte derived from a1 written at +0x13 of the word at
// +0x130, then either the 0x088D6FD4 path (which sets +0x1F) or 0x088DDB6C; v0 = v1 = 1 on both, 0 when a check fails.
void func_088D7158(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    if (call(rt, ctx, 0x088D9E58u, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call(rt, ctx, 0x088DA0BCu, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 inner = word(rt, self + 0x640u);
    if (inner != 0u && flag == 0u) call(rt, ctx, 0x088E4120u, {inner});
    const u32 holder = word(rt, self + 0x130u);
    put8(rt, holder + 0x13u, flag ^ 1u);
    if (call(rt, ctx, 0x088DAA84u, {self}) != 0u) {
        call(rt, ctx, 0x088D6FD4u, {self, 0u, 0u, 0xFFFFFFFFu});
        put8(rt, self + 0x1Fu, 1u);
    } else {
        call(rt, ctx, 0x088DDB6Cu, {self});
    }
    ctx.gpr[2] = 1u;
}

// 0x0892A2C0 (a0 = a vector of pointers, a1 = a {count, source} pair): inserts two bytes per element through 0x08BDDBA0 (or
// hands the tail to 0x08BDE864 when there is no room), then copies `count` two-byte pairs from the source into the vector. v0 = 0.

// 0x089B8DA0 (a0 = self, a1 = a flag, a2 = an object, a3 = an extra value): 0x08A5BA28(object); destroy object's +0x48 member;
// when the flag is set, 0x089BA6CC(self), 0x089BAA60(object, flag), 0x089BA6CC(self, 0); 0x089B8C6C(self, object, extra); the
// virtual at +0x190 of the object's +4 owner with (owner, object); then the tail call 0x08A5B970(object).

// 0x089C946C (a0 = object): walking the ring at +0x48 from +0x54 (its sentinel +0x4C), release each node's +0x14 through
// 0x089F0AFC, then reset the ring to empty and release the global lock.

// 0x08A13844 (a0 = the object): constructor-shaped - store the vtable 0x08CD0418, 0x08ACF184(word(0x08CFFAAC), 0), set the
// byte at +0x41, then destruct the members at +0x10, +0x3C, +0x240, +0x200, +0x178, +0x140, +0xA0, +0x5C and +0x10 again, and
// tail-call 0x08A5BC54(this, 0x7C0, 4).

// 0x08AD9C18 (a0 = a format 0..17, a1 = a width, a3 = a third value, t1 = a size): a negative size is returned as it is; else the
// size divided by the format's unit, scaled to bytes for formats 3, 5 and 6 (28, 6 and 3*512 bytes per unit). Formats 16 and 17
// use four bytes per unit, every other one two. The table at 0x08C90A90 (18 words) gives the per-format block; five of the
// entries and the out-of-range path are the same two-byte division code.
void func_08AD9C18(Runtime &rt, AllegrexContext &ctx) {
    const u32 format = ctx.gpr[4];
    const u32 width = ctx.gpr[5];
    const u32 third = ctx.gpr[7];
    const s32 size = static_cast<s32>(ctx.gpr[9]);
    if (size < 0) {
        ctx.gpr[2] = ctx.gpr[9];
        return;
    }
    u32 result;
    if (format >= 0x12u) {
        result = signed_quotient(size, static_cast<s32>(width << 1));
    } else switch (format) {
    case 3u:
        result = signed_quotient(size, static_cast<s32>(width << 4)) * 28u;
        break;
    case 5u:
        result = signed_quotient(size, static_cast<s32>(width << 2)) * 6u;
        break;
    case 6u:
        result = signed_quotient(size, static_cast<s32>(third)) * 3u * 512u;
        break;
    case 16u:
    case 17u:
        result = signed_quotient(size, static_cast<s32>(width << 2));
        break;
    default:
        result = signed_quotient(size, static_cast<s32>(width << 1));
        break;
    }
    ctx.gpr[2] = result;
}

// 0x08B10A7C (a0 = a tree owner, a1 = the key pair's object): walk the list at a0+0x58 while the key (a1+0xF0, a1+0xF4) sorts
// after a node's range (+0xF0, +0xF4 of the object the node's word points at); then 0x08B34888(&a1[4]), mark a1+0xA4 = 2,
// 0x08B1FC10(a0+0x58, node, a1+0x3C), 0x08B34B24(&a1[4]) and tail-call 0x08B28C74(a0+0x64).

// 0x08B1A338 (a0 = a source, a1 = a target, a2 = a pair): 0x08B1A2AC(a0, a1), 0x08B1A0B4(a0), allocate 0x20 bytes (its two head
// words zeroed, 0x08B34DA8 on the tail), copy a0's {+0, +4} into +0x10/+0x14 (bumping the reference count at +4 when set), copy
// the pair into +0x18/+0x1C, 0x08B1F1C4(a1+0x2C, block), 0x08B28C74(a1); v0 = a0.

// 0x08BE834C (a0 = destination array, a1 = count, a2 = source array): copies `count` records of 0x40 bytes - the two words at
// +4/+8, the five float words at +0xC..+0x1C, the vtable 0x08CCFC90 at +0, and the two 0x10-byte members at +0x20 and +0x30.
// v0 is the last 0x089FA280 result (untouched when the count is zero).

// 0x08C5D020 / 0x08C5D104 (a0 = the object, a1 = a name, a2 = a flag): a standard-exception-shaped constructor - vtable
// 0x08CDB798, the flag as a bool at +4, 0x08B60DB8(this, 0), vtable 0x08CDB930, then the name is looked up twice
// (0x08B35FD8 with 0x08CB57E4 and 0x08CB57E8) and, when both hit, 0x08B61B90(&temporary, name, 0), 0x08B60DB8(this, temporary)
// and 0x08B61BC8(&temporary).
void exception_pair(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    put(rt, self + 4u, ctx.gpr[6] != 0u ? 1u : 0u);
    put(rt, self, 0x08CDB798u);
    put(rt, self + 8u, 0u);
    call(rt, ctx, 0x08B60DB8u, {self, 0u});
    put(rt, self, 0x08CDB930u);
    if (call(rt, ctx, 0x08B35FD8u, {name, 0x08CB57E4u}) == 0u) return;
    if (call(rt, ctx, 0x08B35FD8u, {name, 0x08CB57E8u}) == 0u) return;
    call(rt, ctx, 0x08B61B90u, {frame.at(0u), name, 0u});
    call(rt, ctx, 0x08B60DB8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08B61BC8u, {frame.at(0u)});
}

// 0x08C7A064 (a0 = the destination, a1 = the source record): copy-constructor with vtable-relative field offsets - the four
// words the source's +0, +4, +8 and +0x10 objects hold at -0xC are the destinations' offsets, +4 is zeroed, then the words at
// source +0xC, +0x14, +0x18, +0x20 and +0x24 and the source's own first word are stored through those offsets;
// 0x08C771D4(&this[0xC]) and 0x08C54A7C(this + offset, &this[0xC]).

// 0x088618E8 (a0 = self, a1 = a flag byte): 0x0888B208(self[0x5B8]); 0x088DAFBC(self[0x5AC], self[0x5B8]); when the flag is 1,
// 0x08891534(self + 0x3F80); 0x0888AB4C on each of the two 0x1C38-byte members from +0x608 with a1 = 0; the four halfwords at
// +0x3E84 and the four bytes at +0x3E8C are set to 0xFFFF / 0; +0x3E78, +0x3FD4 and +0x3FDB are cleared. v0 = 0.
void func_088618E8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    call(rt, ctx, 0x0888B208u, {word(rt, self + 0x5B8u)});
    call(rt, ctx, 0x088DAFBCu, {word(rt, self + 0x5ACu), word(rt, self + 0x5B8u)});
    if (flag == 1u) call(rt, ctx, 0x08891534u, {self + 0x3F80u});
    for (u32 index = 0; index != 2u; ++index) call(rt, ctx, 0x0888AB4Cu, {self + 0x608u + index * 0x1C38u, 0u});
    for (u32 index = 0; index != 4u; ++index) {
        put16(rt, self + 0x3E84u + index * 2u, 0xFFFFFFFFu);   // the halfword of the -1 the original keeps in a2
        put8(rt, self + 0x3E8Cu + index, 0u);
    }
    put(rt, self + 0x3E78u, 0u);
    put(rt, self + 0x3FD4u, 0u);
    put8(rt, self + 0x3FDBu, 0u);
    ctx.gpr[2] = 0u;
}

// 0x088756F0 (a0 = a pointer to the owner, a1 = a name, a2 = a qualifier): a name whose first byte is zero counts as empty; an
// empty or absent qualifier makes the caller pass 0 and takes the owner's default through 0x0886B2F4(owner). Then a present name
// is matched with 0x0886A950(found, name); on a hit (or without a name) the tail call 0x0886B9A4(owner, name, qualifier) does
// the work, and when nothing matched v0 is the found pointer.

// 0x08877724 / 0x088777E4 / 0x088778A4 (a0 = a factory, a1 = a name in an argument object): build a temporary from 0x08AB85F0 +
// 0x08AB856C, 0x088755B4(factory, key), then a second temporary's key, 0x088757B0(factory, 0x08CA0074), and when that exists
// 0x08873260(found, first_key, second_key); release the second temporary, 0x08B60804 / 0x08B93554 the first.
void key_lookup(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 factory = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 first_slot = frame.at(4u);
    call(rt, ctx, 0x08AB85F0u, {first_slot});
    const u32 first_key = call(rt, ctx, 0x08AB856Cu, {first_slot});
    call(rt, ctx, 0x088755B4u, {factory, first_key});
    const u32 second_key = call(rt, ctx, 0x08AB856Cu, {first_slot});
    const u32 found = call(rt, ctx, 0x088757B0u, {factory, 0x08CA0074u});
    if (found != 0u) call(rt, ctx, 0x08873260u, {found, first_key, second_key});
    const u32 holder = word(rt, first_slot);
    const u32 block = holder - 0xCu;
    if (block != 0x08D75298u) {
        const u32 status = call(rt, ctx, 0x08B60804u, {block, 0xFFFFFFFFu});
        if (static_cast<s32>(status) <= 0) call(rt, ctx, 0x08B93554u, {block, first_slot});
    }
}

// 0x088798D0 (a0 = a factory): 0x08AB85F0 / 0x08AB856C on a temporary, 0x088756F0(factory, 0, key), and when something is
// found its virtual at +0x1C; then release the temporary (0x08B60804, or 0x08B93554 when the block is a sentinel or the count
// is not positive). v0 is that last call's result.

// 0x0892FCE0 (a0 = a map, a1 = unused, a2 = and a3 = a key pair): look the table up (0x089363FC), 0x0893656C(table, key pair)
// (a forwarder that only reads a2/a3); answer 0x26 is a miss. When the pair is already the cached one (+8/+0xC) the answer is 1;
// else 0x0892FBE0(map), the table again and 0x08937918(table, key pair, &value) - a hit stores +0x10/+0x14 and the pair and
// answers 1.
void func_0892FCE0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 map = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 key_first = ctx.gpr[6];
    const u32 key_second = ctx.gpr[7];
    const u32 table = call(rt, ctx, 0x089363FCu, {map, argument, key_first, key_second});
    if (call(rt, ctx, 0x0893656Cu, {table, argument, key_first, key_second}) == 0x26u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (word(rt, map + 8u) == key_first && word(rt, map + 0xCu) == key_second) {
        ctx.gpr[2] = 1u;
        return;
    }
    call(rt, ctx, 0x0892FBE0u, {map});
    const u32 again = call(rt, ctx, 0x089363FCu, {map, argument, key_first, key_second});
    const u32 slot = call(rt, ctx, 0x08937918u, {again, argument, key_first, key_second, frame.at(0u)});
    if (slot == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, map + 0x10u, slot);
    put(rt, map + 0x14u, word(rt, frame.at(0u)));
    put(rt, map + 8u, key_first);
    put(rt, map + 0xCu, key_second);
    ctx.gpr[2] = 1u;
}

// 0x08975F1C (a0 = self, a1 = an object): fill a temporary request at +0xC..+0x18 (0x08D74FD0+0xC, -1, 0x08CCDA08, and an
// output slot) and hand it to the object's virtual at +8; then release the first temporary and 0x08974698(self, &output).

// 0x08A514DC (a0 = the object): mark +0x5C, 0x08B28C74(a0+0x30), 0x08B2E224 and 0x08B2E28C on the +0x2C owner, release the
// +0x28, +0xC and +0x10 blocks, take the global lock, release and clear the +0 and +4 blocks, and tail-call the unlock.

// 0x08B0EC60 (a0 = a source object, a1 = a destination buffer): 0x08B0CE58(a0), 0x08B0E0CC(&temporary), 0x08B0EA48(&temporary,
// a1); when a block comes back: mode 4 at +0x7C, 0x08B0DE3C(block), copy the 0x3C bytes at block+0xA8 to a1, 0x08B0EA58(block);
// then 0x08B0DDF0(&temporary). v0 = a0.
void func_08B0EC60(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x110u);
    const u32 self = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    call(rt, ctx, 0x08B0CE58u, {self});
    call(rt, ctx, 0x08B0E0CCu, {frame.at(0u)});
    const u32 block = call(rt, ctx, 0x08B0EA48u, {frame.at(0u), target});
    if (block != 0u) {
        put(rt, block + 0x7Cu, 4u);
        call(rt, ctx, 0x08B0DE3Cu, {block});
        u32 from = block + 0xA8u;
        u32 to = target;
        while (from != block + 0xD8u) {
            for (u32 index = 0; index != 4u; ++index) put(rt, to + index * 4u, word(rt, from + index * 4u));
            from += 0x10u;
            to += 0x10u;
        }
        put(rt, to + 8u, word(rt, from + 8u));
        put(rt, to, word(rt, from));
        put(rt, to + 4u, word(rt, from + 4u));
        call(rt, ctx, 0x08B0EA58u, {block});
    }
    call(rt, ctx, 0x08B0DDF0u, {frame.at(0u)});
    ctx.gpr[2] = self;
}

// 0x08B0C8D8 (a0 = self, a1, a2, a3 and t0 as given): 0x08B0C778 with all five, 0x08B0EA48(self+0x20, a1) stored at self+8; a null
// block leaves mode 2 at +4 and answers -1; else block+0x54 = self, 0x08B0DE84(block), block+0x7C = a3, block+0x94 = t0,
// 0x08B0DF0C(block), 0x08B0C880(self) and - when negative - 0x08B0C358(self). v0 = that status.

// 0x08B48398 (a0 = an owner, a1 = an optional text): allocate 0x44 bytes through 0x08B2CB18(owner[0x28], 0x44, 0x10); when it
// comes back: 0x08B3FA98 and 0x08B3F838 on both of its two 0x1A-byte halves, its +0x30..+0x32 bytes and +0x34..+0x40 words
// cleared; with a text, a second 0x10-byte block through the same allocator stored at +0x40. v0 = the block (0 when the
// allocator refused, which still writes the text pointer at address 0x40 like the original).
void func_08B48398(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 block = call(rt, ctx, 0x08B2CB18u, {word(rt, owner + 0x28u), 0x44u, 0x10u});
    const u32 second_half = block + 0x1Au;
    if (block != 0u) {
        call(rt, ctx, 0x08B3FA98u, {block + 4u});
        call(rt, ctx, 0x08B3FA98u, {second_half});
        put(rt, block, 0u);
        call(rt, ctx, 0x08B3F838u, {block + 4u});
        call(rt, ctx, 0x08B3F838u, {second_half});
        put8(rt, block + 0x30u, 0u);
        put8(rt, block + 0x31u, 0u);
        put8(rt, block + 0x32u, 0u);
        put(rt, block + 0x34u, 0u);
        put(rt, block + 0x38u, 0u);
        put(rt, block + 0x3Cu, 0u);
        put(rt, block + 0x40u, 0u);
    }
    if (text != 0u) put(rt, block + 0x40u, call(rt, ctx, 0x08B2CB18u, {word(rt, owner + 0x28u), text, 0x10u}));
    ctx.gpr[2] = block;
}

// 0x08B5C154 (a0 = a locale category): 0 gives 0; a value whose bits outside 0x3F are clear is kept; anything else is a
// category that went through _S_normalize_category's assert 0x08B5AFDC (which never returns; the original then indexes its own
// jump table with the message address, so the fall-through below is only a faithful shape). Otherwise 0..5 answer 0x3F, 4, 1,
// 0x10, 2 and 8 from the table at 0x08C95D74.
void func_08B5C154(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 category = ctx.gpr[4];
    if (category == 0u) {
        ctx.gpr[2] = category;
        return;
    }
    if ((category & 0x3Fu) != 0u) {
        if ((category & ~0x3Fu) == 0u) {
            ctx.gpr[2] = category;
            return;
        }
        call(rt, ctx, 0x08B5AFDCu, {0x08CB5778u});
    } else if (category >= 6u) {
        call(rt, ctx, 0x08B5AFDCu, {0x08CB5778u});
    }
    u32 result = 0u;
    switch (category) {
    case 0u: result = 0x3Fu; break;
    case 1u: result = 4u; break;
    case 2u: result = 1u; break;
    case 3u: result = 0x10u; break;
    case 4u: result = 2u; break;
    case 5u: result = 8u; break;
    default: break;
    }
    ctx.gpr[2] = result;
}

// 0x08C122C8 (a0 = a vector, a1 = the destination, a2 = the source): move (vector[4] - a2) / 0x10 records of 0x10 bytes from a2
// on to a1 through 0x089FB570, then 0x08C12284(next, vector[4], &temporary) and shrink vector[4] by (source - destination)
// rounded down to 0x10. v0 = the destination.

// 0x088212F8 (a0 and a1 = two keys): build each key in its own stack slot (0x088547C4 then 0x08854678, released with
// 0x08A5B9EC), turn both into sortable values with 0x088A7608 and answer the signed "first before second".

// 0x0882D4BC (a0 = a record, a1 = a first key, a2 = a second key, a3 = a third): 0x08B85EB8(&temporary, a2, &byte),
// 0x0899C764(&value, a0, a1, &temporary, a3), 0x089A9324(a0, &value), release the value and the temporary.

// 0x0884B9D0 (a0 = a factory): 0x08847008(&temporary), 0x08846E50(temporary), release it, 0x088D47EC() as the size,
// 0x08B2B4D8(0x10, size), 0x088D51BC(first, block), 0x08A44D9C(&temporary, block, size, 1), release the temporary.

// 0x08877354 (a0 = a factory): 0x08AB85F0 / 0x08AB856C on a temporary, 0x08B77BF4(key) whose v0/v1 pair goes to
// 0x08B71EC8(v0, v1) as a float; when the temporary's +0xC is positive, 0x088757B0(factory, 0x08CA0074) and its +0xC member
// through 0x08AF4458 with the float; then release the temporary.

// 0x088941A0 (a0 = a pointer to the source, a1 = the destination): copy 0x24 bytes through the swl/swr pair when either side is
// unaligned and through plain words otherwise, then 0x08894020 - which just answers 0x30, so v0 = a1 + 0x30. The two words the
// copy leaves in a0 and a1 are what the call passes on.
void func_088941A0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 destination = ctx.gpr[5];
    const u32 source = word(rt, ctx.gpr[4]);
    const u32 end = source + 0x20u;
    u32 from = source;
    u32 to = destination;
    u32 third = 0;
    u32 fourth = 0;
    if (((source | destination) & 3u) != 0u) {
        while (from != end) {
            const u32 part0 = word(rt, from);
            const u32 part1 = word(rt, from + 4u);
            const u32 part2 = word(rt, from + 8u);
            const u32 part3 = word(rt, from + 0xCu);
            from += 0x10u;
            rt.memory().store_word_left(to + 3u, part0);
            rt.memory().store_word_right(to, part0);
            rt.memory().store_word_left(to + 7u, part1);
            rt.memory().store_word_right(to + 4u, part1);
            to += 0x10u;
            rt.memory().store_word_left(to - 5u, part2);
            rt.memory().store_word_right(to - 8u, part2);
            rt.memory().store_word_left(to - 1u, part3);
            rt.memory().store_word_right(to - 4u, part3);
            third = part2;
            fourth = part3;
        }
        const u32 tail = word(rt, from);
        rt.memory().store_word_left(to + 3u, tail);
        rt.memory().store_word_right(to, tail);
    } else {
        while (from != end) {
            const u32 part0 = word(rt, from);
            const u32 part1 = word(rt, from + 4u);
            const u32 part2 = word(rt, from + 8u);
            const u32 part3 = word(rt, from + 0xCu);
            from += 0x10u;
            put(rt, to, part0);
            put(rt, to + 4u, part1);
            put(rt, to + 8u, part2);
            put(rt, to + 0xCu, part3);
            to += 0x10u;
            third = part2;
            fourth = part3;
        }
        put(rt, to, word(rt, from));
    }
    ctx.gpr[2] = destination + call(rt, ctx, 0x08894020u, {third, fourth, to});
}

// 0x088E4DFC (a0 = a holder, a1 = out words, a2 = a two-word out record, a3 = an out half, t0 = sixteen out words): copy the
// record at holder[0]+0x178 - its +0x178 half, +0x17A half, the two words at +0x180/+0x184 and the sixteen words from +0x188 -
// under the lock at +0x1C8. v0 = (word(a1) + 1) != 0.
void func_088E4DFC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 holder = ctx.gpr[4];
    const u32 first_out = ctx.gpr[5];
    const u32 pair_out = ctx.gpr[6];
    const u32 half_out = ctx.gpr[7];
    const u32 words_out = ctx.gpr[8];
    const u32 object = word(rt, holder);
    call(rt, ctx, 0x08B344E0u, {object + 0x1C8u});
    const u32 record = object + 0x178u;
    put(rt, first_out, sext16(load16(rt, record)));
    put(rt, pair_out, word(rt, record + 8u));
    put(rt, pair_out + 4u, word(rt, record + 0xCu));
    put(rt, half_out, sext16(load16(rt, record + 2u)));
    for (u32 index = 0; index != 0x10u; ++index) put(rt, words_out + index * 4u, word(rt, record + 0x10u + index * 4u));
    call(rt, ctx, 0x08B3457Cu, {word(rt, holder) + 0x1C8u});
    ctx.gpr[2] = (word(rt, first_out) + 1u) != 0u ? 1u : 0u;
}

// 0x088E58DC (a0 = a pair of object slots): 0x08B0979C, 0x08B09908 on its answer, then for each of the two objects
// (a0[0], a0[1]): when it exists and is not marked at +0x521 and its virtual at +8 answers non-zero, 0x088ED560(object),
// 0x088ED6F4(object, 0) and mark +0x521 = 1, +0x50C = 1. A missing object skips it; a marked one or a failing virtual goes to
// 0x088E57CC(self), which answers 0. v0 = 1 after both.

// 0x088EBBF8 (a0 = self, a1 = a flag byte): the virtual at +0x9C of self[0x538] must answer non-zero; then 0x08B08AC0(self[0xA8])
// whose +0x2C slot is the function, 0x088D4808() as the fourth argument, the call with (device, owner, answer, fourth, flag);
// a non-zero answer clears self's +0x574 and +0x560..+0x570. v0 = 1 when that call answered non-zero.

// 0x088ECCD4 (a0 = a source object, a1 = a byte, a2 = a four-halfword source): the virtual at +0x18 of a0 (a1 = 0) answers the
// first two words of a stack record; +8 is the byte, +0xA is (a2 != 0), +0xC..+0x12 are a2's four halfwords (when a2 exists) and
// +0x14..+0x17 are the four bytes at a0+0x4AC. The virtual at +0xF4 of a0[0x538] takes the record with
// (device, 0, -1, 3, &record, 0x18).

// 0x08900350 (a0 = a moving object): when the timer at +0x41C is negative it restarts from +0x414/+0x418; a speed of exactly
// 1.0 becomes 1.3; and when the three timed values at +0x49C/+0x4A0/+0x4A4 are exactly 100.0, 1000.0 and 0, the +0xC, +0x30
// and +0x38 words replace them (and +0x34). No frame, no call.
void func_08900350(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    if (float_at(rt, self + 0x41Cu) < 0.0f) {
        put(rt, self + 0x41Cu, word(rt, self + 0x414u));
        put(rt, self + 0x420u, word(rt, self + 0x418u));
    }
    if (float_at(rt, self + 4u) == 1.0f) put(rt, self + 4u, 0x3FA66666u);
    if (float_at(rt, self + 0x49Cu) == 100.0f && float_at(rt, self + 0x4A0u) == 1000.0f &&
        float_at(rt, self + 0x4A4u) == 0.0f) {
        put(rt, self + 0x4A0u, word(rt, self + 0xCu));
        put(rt, self + 0x49Cu, word(rt, self + 0x30u));
        put(rt, self + 0x4A4u, word(rt, self + 0x38u));
        put(rt, self + 0x34u, word(rt, self + 0xCu));
    }
}

// 0x0898E358 (a0 = an out record, a1 = unused, a2 unused, a3 = a pointer to an object): 0x0898E170(&temporary),
// 0x0898D8D4(temporary, object's virtual at +0x3C), released, then the result through 0x08A85B1C into a stack slot given to a0.

// 0x0899DDA0 / 0x0899DE64 (a0 = self, a2 = a count, a3 = a pointer to an object): 0x0899DD60(&temporary),
// 0x0899D164(temporary) as a float; with a positive count the object's virtual at +0x54 replaces it; 0x0899D214(temporary,
// float); release the temporary.
void float_through(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 count = ctx.gpr[6];
    const u32 object_pointer = ctx.gpr[7];
    call(rt, ctx, 0x0899DD60u, {frame.at(0u)});
    u32 holder = word(rt, frame.at(0u));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
    float value = callf(rt, ctx, 0x0899D164u, {holder}, {}).f0;
    if (static_cast<s32>(count) > 0) {
        const u32 object = word(rt, object_pointer);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA485Cu});
        value = call_slot(rt, ctx, object, 0x54u, {object}).f0;
    }
    holder = word(rt, frame.at(0u));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA496Cu});
    callf(rt, ctx, 0x0899D214u, {holder}, {value});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08A58394 (a0 = self, a1 = an owner): 0x08A76ACC(self), vtable 0x08CB7730 at self, 0x08A52A98(self+0x14, owner), the
// owner's virtual at +0x120, 0x08A5BC20(0x18, 4), 0x08A56018(block, 0, answer, 0, 0), then 0x08A5BAB8(self+0x18, &block) and
// self+0x1C = 0.

// 0x08AB1F0C (a0 = an object, a1 = an out record, a3 = a pointer to a scale object): the byte at a0+0x14 and the scale object's
// virtual at +0x44 are the two floats 0x08B67720 combines; the result is truncated to a word (`trunc.w.s`, with the 2^31
// adjustment for values from 2^31 up), 0x08A85B1C builds it on the stack, it goes to a1 unless a1 is the stack slot, and the
// slot is released.

// 0x08AD5460 (a0 = an out state, a1 = a source record): 0x08AD5394(a0, a1), 0x08B10204(&temporary, a1, 1), copy the 0x3C bytes
// at &temporary to the stack, and when the first word is zero mark a0+0xC = 1 with a0+8 = stack[0x10] and a0+4 = stack[0x20] and
// answer 1; otherwise 0.
void func_08AD5460(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x90u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 temporary = frame.at(0x40u);
    call(rt, ctx, 0x08AD5394u, {self, source});
    call(rt, ctx, 0x08B10204u, {temporary, source, 1u});
    u32 from = temporary;
    u32 to = frame.at(0u);
    while (from != frame.at(0x70u)) {
        for (u32 index = 0; index != 4u; ++index) put(rt, to + index * 4u, word(rt, from + index * 4u));
        from += 0x10u;
        to += 0x10u;
    }
    put(rt, to, word(rt, from));
    put(rt, to + 4u, word(rt, from + 4u));
    put(rt, to + 8u, word(rt, from + 8u));
    if (word(rt, frame.at(0u)) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put8(rt, self + 0xCu, 1u);
    put(rt, self + 8u, word(rt, frame.at(0x10u)));
    put(rt, self + 4u, word(rt, frame.at(0x20u)));
    ctx.gpr[2] = 1u;
}
// 0x08C5518C (a0 = the destination, a1 = the source record, a2 = an argument): copy-constructor with vtable-relative offsets -
// the source's +0, +4 and +0xC objects hold their destination offsets at -0xC, +4 is zeroed, then source's +0, +8, +0x10,
// +0x14 and +0x18 words are stored through those offsets (the source's +0x10 at the destination's +8), and
// 0x08C54A7C(this + offset, argument) finishes.
} // namespace
} // namespace new_batch_main_15
} // namespace gt::game

#define NEW15(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_15::name, gt::ReturnKind::kind)
NEW15(0x088618E8u, func_088618E8, Word);
NEW15(0x088941A0u, func_088941A0, Word);
NEW15(0x088D7158u, func_088D7158, Word);
NEW15(0x088E4DFCu, func_088E4DFC, Word);
NEW15(0x08900350u, func_08900350, Void);
NEW15(0x0892FCE0u, func_0892FCE0, Word);
NEW15(0x08A5FE6Cu, func_08A5FE6C, Word);
NEW15(0x08A7372Cu, func_08A7372C, Word);
NEW15(0x08A737ECu, func_08A737EC, Word);
NEW15(0x08A964A0u, func_08A964A0, Word);
NEW15(0x08A96560u, func_08A96560, Word);
NEW15(0x08AB06CCu, func_08AB06CC, Word);
NEW15(0x08AD5460u, func_08AD5460, Word);
NEW15(0x08AD9C18u, func_08AD9C18, Word);
NEW15(0x08B0EC60u, func_08B0EC60, Word);
NEW15(0x08B48398u, func_08B48398, Word);
NEW15(0x08B5C154u, func_08B5C154, Word);
