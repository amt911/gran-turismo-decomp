// New batch main 7 (80 functions the extra seeds made emittable, none decompiled before), written as structured C from
// `tools.disasm`. Two families dominate and share one helper each:
//  - the *reporters*: a constructor fills a stack temporary, its object is read, one build call reports a value from it
//    (a word/half/byte field, the object itself, an address into it, or a float in $f12 0x08A87EEC takes), the result
//    is moved into the caller's holder when it is not the temporary itself, and both temporaries are released with
//    0x08A5B9EC. A null object goes through 0x08A5BB7C with the function's message first.
//  - the *boxed binary functors*: a field at +0x14 of a0, the virtual at slot 0x34..0x4C of the object word(a3)
//    points at, an operator over both (add, sub, and/or/xor, shifts, signed/unsigned compares) and the same temporary
//    reporting. Slots, operators and messages are the only differences.
// No names: none of these has evidence. Before registering: the four audits (frame_audit, literal_check, sp_loads,
// arg_reads) and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_7 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// Released by the reporters below (0x08A5B9EC(sp)), moved into a caller's holder (0x08A5BAD8(holder, word(sp))) and the
// error path every one of them takes when its constructor answered null (0x08A5BB7C(message)).
constexpr u32 kRelease = 0x08A5B9ECu;
constexpr u32 kMove = 0x08A5BAD8u;
constexpr u32 kNullObject = 0x08A5BB7Cu;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee that takes its value in $f12.
u32 call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, float value) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments, {value}).v0;
    return ctx.gpr[2];
}

// --- the reporters ---

enum class Reported { Word, UnsignedHalf, UnsignedByte, Object, Address, Float };

// The reporting shape every function of this family has: a 0x20-byte frame, a constructor on a temporary (`object_below`
// false: at sp+4, the report later building at sp — true: at sp, the report building at sp+4), the value read from the
// object it leaves there, one build call with the value, then 0x08A5BAD8 for the caller's holder (unless it *is* the
// temporary) and 0x08A5B9EC on both temporaries, the one that was built first released last.
void report_value(Runtime &rt, AllegrexContext &ctx, u32 constructor, u32 error_text, u32 report, Reported reported,
                  u32 offset, bool object_below) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 object_slot = frame.at(object_below ? 0u : 4u);
    const u32 value_slot = frame.at(object_below ? 4u : 0u);
    call(rt, ctx, constructor, {object_slot});
    const u32 object = word(rt, object_slot);
    if (object == 0u) call(rt, ctx, kNullObject, {error_text});
    switch (reported) {
    case Reported::Word: call(rt, ctx, report, {value_slot, word(rt, object + offset)}); break;
    case Reported::UnsignedHalf: call(rt, ctx, report, {value_slot, rt.memory().load16(object + offset)}); break;
    case Reported::UnsignedByte: call(rt, ctx, report, {value_slot, rt.memory().load8(object + offset)}); break;
    case Reported::Object: call(rt, ctx, report, {value_slot, object}); break;
    case Reported::Address: call(rt, ctx, report, {value_slot, object + offset}); break;
    case Reported::Float: call_float(rt, ctx, report, {value_slot}, float_at(rt, object + offset)); break;
    }
    if (holder != value_slot) call(rt, ctx, kMove, {holder, word(rt, value_slot)});
    call(rt, ctx, kRelease, {value_slot});
    call(rt, ctx, kRelease, {object_slot});
}


// --- the boxed binary functors ---

enum class Operand { Word, SignedHalf, SignedByte, UnsignedHalf, UnsignedByte };
enum class Binary {
    Add,
    Subtract,
    Xor,
    And,
    Or,
    ShiftLeft,
    ShiftRight,
    ShiftRightArithmetic,
    ValueBelowField,
    FieldBelowValue,
    ValueSignedBelowField,
    FieldSignedBelowValue,
};

u32 operand_at(Runtime &rt, u32 address, Operand operand) {
    switch (operand) {
    case Operand::Word: return word(rt, address);
    case Operand::SignedHalf:
        return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
    case Operand::SignedByte:
        return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
    case Operand::UnsignedHalf: return rt.memory().load16(address);
    case Operand::UnsignedByte: return rt.memory().load8(address);
    }
    return 0u;
}

// `field` is the loaded field, `value` what the virtual answered; the shift counts are masked to five bits as the
// hardware does, and the compares are the instruction's own (sltu/slt).
u32 apply_binary(Binary binary, u32 field, u32 value) {
    switch (binary) {
    case Binary::Add: return field + value;
    case Binary::Subtract: return field - value;
    case Binary::Xor: return field ^ value;
    case Binary::And: return field & value;
    case Binary::Or: return field | value;
    case Binary::ShiftLeft: return field << (value & 31u);
    case Binary::ShiftRight: return field >> (value & 31u);
    case Binary::ShiftRightArithmetic: return static_cast<u32>(static_cast<s32>(field) >> (value & 31u));
    case Binary::ValueBelowField: return value < field ? 1u : 0u;
    case Binary::FieldBelowValue: return field < value ? 1u : 0u;
    case Binary::ValueSignedBelowField:
        return static_cast<s32>(value) < static_cast<s32>(field) ? 1u : 0u;
    case Binary::FieldSignedBelowValue:
        return static_cast<s32>(field) < static_cast<s32>(value) ? 1u : 0u;
    }
    return 0u;
}

// The functor shape: the 0x20-byte frame, the field at +0x14 of a0 read first, null(a3) reported with the function's
// message, the virtual at `slot` of the object word(a3) points at called with that object, and the operator's result
// reported through the same temporary handling the reporters use.
void boxed_binary(Runtime &rt, AllegrexContext &ctx, Operand operand, u32 slot, Binary binary, u32 report, u32 error_text) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 box = word(rt, ctx.gpr[7]);
    const u32 field = operand_at(rt, self + 0x14u, operand);
    if (box == 0u) call(rt, ctx, kNullObject, {error_text});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, box) + slot), {box});
    call(rt, ctx, report, {frame.at(0), apply_binary(binary, field, value)});
    if (holder != frame.at(0)) call(rt, ctx, kMove, {holder, word(rt, frame.at(0))});
    call(rt, ctx, kRelease, {frame.at(0)});
}

void func_08A5FD68(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::Word, 0x4Cu, Binary::Subtract, 0x08A5FA8Cu, 0x08CB00F8u);
}
void func_08A602F0(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::Word, 0x4Cu, Binary::FieldBelowValue, 0x08A97A6Cu, 0x08CB00F8u);
}
void func_08A73E84(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedHalf, 0x4Cu, Binary::And, 0x08A85B1Cu, 0x08CB0BA8u);
}
void func_08A73F04(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedHalf, 0x4Cu, Binary::Or, 0x08A85B1Cu, 0x08CB0BA8u);
}
void func_08A73F84(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedHalf, 0x4Cu, Binary::Xor, 0x08A85B1Cu, 0x08CB0BA8u);
}
void func_08AB0A30(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::UnsignedHalf, 0x48u, Binary::FieldBelowValue, 0x08A97A6Cu, 0x08CB3A78u);
}
void func_08AB0F64(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::UnsignedHalf, 0x4Cu, Binary::ShiftRightArithmetic, 0x08A85B1Cu, 0x08CB3A78u);
}
void func_08AB2510(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::UnsignedByte, 0x4Cu, Binary::And, 0x08A85B1Cu, 0x08CB3AF0u);
}
void func_08AB2710(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::UnsignedByte, 0x4Cu, Binary::ShiftRightArithmetic, 0x08A85B1Cu, 0x08CB3AF0u);
}

// --- one-off shapes ---

// 0x08BE4008 (a0 = object): empties the two node lists at +4 and +8 — each walked through 0x08B64654 for as long as the
// node's word at +8 is non-zero — then hands +0xC to 0x08B3457C (a tail call). The `beql` at 08BE4030 runs its delay
// slot (`a0 = word(self+8)`) only when it is taken, so the word is not read on the walk path.

// 0x08C20AD0 (a0 = object, a1 = holder): with the signed count at +8 positive, passes it to 0x08A6159C(holder, count),
// re-reads it (a positive one spins v0 up to it), and reports it through a temporary at sp: 0x08A85B1C(sp, count),
// 0x08A61584(holder, sp), 0x08A5B9EC(sp).

// 0x08C24A70 (a0 = owner, a1 = begin, a2 = end): moves [end, word(owner+4)) down to begin with 0x08B79BAC(begin, end,
// word(owner+4) - end), then 0x08C24A18(begin + that length, word(owner+4), sp); the count at +4 drops by (end - begin);
// v0 = begin.
void func_08C24A70(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 begin = ctx.gpr[5];
    const u32 end = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 moved = end - begin;
    const u32 remaining = word(rt, owner + 4u) - end;
    call(rt, ctx, 0x08B79BACu, {begin, end, remaining});
    call(rt, ctx, 0x08C24A18u, {remaining + begin, word(rt, owner + 4u), frame.at(0)});
    put(rt, owner + 4u, word(rt, owner + 4u) - moved);
    ctx.gpr[2] = begin;
}

// 0x08C79828 (a0 = object): constructor — the two vtable words at +0 and +0xAC, 0x08C786B4 on +8, then the base
// pointer and the second word the table at 0x08CDC2F8 holds (+4 and +8) take the place of +0 and, at the offset the
// base pointer's word at -0xC names, of a second slot; the word at +4 is zeroed and 0x08B6543C runs on +0xAC (a tail
// call).

// 0x08864A74 (a0 = object): a teardown — 0x088D9B1C on the word at +0x5AC of the object's first word with a1 = 1, then
// four calls on the object, then 0x0885F428 on it (a tail call).

// 0x0886EC54 (a0 = object, a1 = target, a2 = source): with a2 null or 0x08B7B8C4(a2, 0x08C9F790) non-zero, builds a value
// from the virtual at +0xC of the header at word(a0+0x70) with 0x08B4E734(target, sp) and 0x08B507B8(target); v0 = 1 then,
// 0 when the source was rejected.
void func_0886EC54(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    const u32 header = word(rt, owner + 0x70u) + 0xCu;
    bool build = true;
    if (source != 0u && call(rt, ctx, 0x08B7B8C4u, {source, 0x08C9F790u}) == 0u) build = false;
    u32 result = 0u;
    if (build) {
        call_indirect(rt, ctx, word(rt, word(rt, header) + 0xCu), {frame.at(0), header});
        call(rt, ctx, 0x08B4E734u, {target, frame.at(0)});
        call(rt, ctx, 0x08B507B8u, {target});
        result = 1u;
    }
    ctx.gpr[2] = result;
}

// 0x08879990 (a0 = object): a release — 0x088757EC on the node 0x08AB856C(0x08AB85F0(&temp)) fills, then the counter
// word at temp-4: when the header at temp-0xC is not 0x08D75298 and 0x08B60804(header, -1) answers > 0 nothing else runs,
// otherwise 0x08B93554(header, sp).
void func_08879990(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    call(rt, ctx, 0x08AB85F0u, {frame.at(4)});
    const u32 node = call(rt, ctx, 0x08AB856Cu, {frame.at(4)});
    call(rt, ctx, 0x088757ECu, {self, node});
    const u32 value = word(rt, frame.at(4));
    const u32 header = value - 0xCu;
    if (header == 0x08D75298u) {
        ctx.gpr[2] = 0x08D75298u;
        return;
    }
    const u32 counted = call(rt, ctx, 0x08B60804u, {value - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(counted) > 0) return;
    call(rt, ctx, 0x08B93554u, {header, frame.at(0)});
}

// 0x0888B360 (a0 = object, a1..a3): fills a 0x20-byte stack record (0x1F, a1, a2, a3 & 0xFF) and passes it to
// 0x088936A4 with the word at +0x18; when the object's handler word at +0x28 is set, calls it with a1, a2, a3 & 0xFF and
// the word at +0x30 as the fourth argument.
void func_0888B360(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 flags = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0xC0u);
    put(rt, frame.at(0), 0x1Fu);
    put(rt, frame.at(8), first);
    put(rt, frame.at(0xC), second);
    put(rt, frame.at(0x10), flags);
    call(rt, ctx, 0x088936A4u, {word(rt, self + 0x18u), frame.at(0)});
    const u32 handler = word(rt, self + 0x28u);
    if (handler == 0u) return;
    call_indirect(rt, ctx, handler, {first, second, flags, word(rt, self + 0x30u)});
}

// 0x0889D6B8 (a0 = object, a1 = argument): calls every (function, argument) pair of the array at +0x28 of the object the
// word at a0 points at — the count at +0x30 is re-read after each call and the array base too — then 0x08B3457C on it (a
// tail call).

// 0x08918068 (a0 = array of 4-byte entries, a1 = index): the entry at a0 + a1*4 + 8 is looked up twice through
// 0x089277BC (a1 = 0 both times, as the original sets it). When the first lookup answers 0 and the object
// 0x08918040(array, 0) returns has a null word at +0x9C the answer is 1; otherwise the answer is 1 only when the second
// lookup answers 0 and 0x0891FF94(entry) answers non-zero.

// 0x08991E44 (a0 = object): releases the chain the object's word at +0x84 heads (each node through the virtual at +0x19C
// of its own vtable, the next one at its +0x54), then asks the object's virtual at +0x1A4 and stores the word at +0x10 of
// the object's +0x78 area when it answered non-zero, the word at (+0x78)+0xC otherwise, into +0x9C.

// 0x089B22A4 (a0 = object, a1 = value): 0x089AB800 on the object, then three calls on the pair (+0x30, +0x18) — the value
// goes into the second one as a3 with a2 = 0 — then 0x089AB8A8 on the object (a tail call).

// 0x089C5680 (a0 = object, a1 = value): builds a value at sp (0x08A12E38), passes it to the virtual at +0x1C of the object
// it holds (with a1 = value), reports the pair through 0x089C160C(object, sp), releases sp; v0 = a0.

// 0x089F13E8 (a0 = object, a1..a3): allocates a 0x2C-byte record (0x08A5BC20(0x2C, 4)), fills it with
// 0x089F12C4(record, a1, a2, a3) and registers it with 0x089FDAB0 through a one-word stack record.
void func_089F13E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    call(rt, ctx, 0x089F12C4u, {record, first, second, third});
    put(rt, frame.at(0), record);
    call(rt, ctx, 0x089FDAB0u, {self, frame.at(0)});
}

// 0x08A1369C (a0 = object, a1 = state): locks the object's +0x10 area with 0x08B34888; the state is stored into +0xC —
// always, unless it is 2 and one of the bytes at +0x40 and +0x41 is set — and v0 = 1 only when it was stored; unlock with
// 0x08B34B24.

// 0x08A7CA54 (a0 = object, a1 = source, a2 = value): 0x08A924F0(sp+4, &value), 0x08A91A38(sp+8, source, sp+4), the pair
// goes to 0x08A7C5C8(object, sp+8), then both temporaries are released, the first one last.
void func_08A7CA54(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 first = frame.at(4);
    const u32 second = frame.at(8);
    put(rt, frame.at(0), value);
    call(rt, ctx, 0x08A924F0u, {first, frame.at(0)});
    call(rt, ctx, 0x08A91A38u, {second, source, first});
    call(rt, ctx, 0x08A7C5C8u, {self, second});
    call(rt, ctx, kRelease, {second});
    call(rt, ctx, kRelease, {first});
}

// 0x08AA65E4 (a0 = source): a variable-length code — 0x08AA64DC(a0) answers the first seven bits (bit 0x80 set: more
// follow). Each further byte is 0x08AA64DC(a0) again, shifted into s1 while the mask s2 (0x7F first, then ((s2 >> 1) << 8) |
// 0xFF) keeps its 0x80 bit; v0 = s2 & s1.
void func_08AA65E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    u32 mask = 0x7Fu;
    u32 value = call(rt, ctx, 0x08AA64DCu, {source});
    if ((value & 0x80u) != 0u) {
        u32 shifted = value;
        for (;;) {
            const u32 next = call(rt, ctx, 0x08AA64DCu, {source});
            shifted = shifted << 1;
            const u32 mask_shifted = (mask >> 1) << 8;
            value = (value << 8) | next;
            const u32 more = shifted & 0x80u;
            mask = mask_shifted | 0xFFu;
            if (more == 0u) break;
        }
    }
    ctx.gpr[2] = mask & value;
}

// 0x08AC4850 (a0 = object): locks the object's +0x30 area with 0x08B34888; while the word at +0x25CC is set, sets the byte
// at +0x25DC, runs 0x08B34B2C(lock) and stops once the byte at +0x25B2 is clear (v0 = 1) or the word clears (v0 = 0);
// unlocks with 0x08B34B24.
void func_08AC4850(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 lock = self + 0x30u;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B34888u, {lock});
    u32 result = 0u;
    if (word(rt, self + 0x25CCu) != 0u) {
        for (;;) {
            rt.memory().store8(self + 0x25DCu, 1u);
            call(rt, ctx, 0x08B34B2Cu, {lock});
            if (rt.memory().load8(self + 0x25B2u) == 0u) {
                result = 1u;
                break;
            }
            if (word(rt, self + 0x25CCu) == 0u) break;
        }
    }
    call(rt, ctx, 0x08B34B24u, {lock});
    ctx.gpr[2] = result;
}

// 0x08AC7488 (a0 = object): 0x08B344E0 on the object, then 0x08C7AB68(word(a0+0x2C), &count): a non-zero answer sets the
// byte at +0x28 and leaves v0 = 0; a count above zero and below the signed word at +0x38 answers 1; then 0x08B3457C.

// 0x08AC7E5C (a0 = object): the same shape as 0x08AC4850 on this object's own slots — lock at +8, flag word at +0x584,
// byte at +0x594 and the word at +0x566 — v0 = 1 when the byte went clear.
void func_08AC7E5C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 lock = self + 8u;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B34888u, {lock});
    u32 result = 0u;
    if (word(rt, self + 0x584u) != 0u) {
        for (;;) {
            rt.memory().store8(self + 0x594u, 1u);
            call(rt, ctx, 0x08B34B2Cu, {lock});
            if (rt.memory().load8(self + 0x566u) == 0u) {
                result = 1u;
                break;
            }
            if (word(rt, self + 0x584u) == 0u) break;
        }
    }
    call(rt, ctx, 0x08B34B24u, {lock});
    ctx.gpr[2] = result;
}

// 0x08ACBF70 (a0 = object): gated by 0x08B319A4(0x08D69170) or 0x08B327AC(0x08D69170, 1); then 0x08ACB910(object, sp) —
// when it answers non-zero the word 3 goes to sp+0x30 and 0x08ACBC78(object, sp) is the result, 0 otherwise.

// 0x08ACBFF0 (a0 = object): 0x08ACBF70 with the type word 1 instead of 3.

// 0x08B26D94 (a0 = array of 0x10-byte entries, a1 = count): for each entry of the first `count` whose first word is set,
// 0x08B26D0C(object, previous result, index) runs and its answer becomes the next previous; v0 = the last answer, -1 when
// none ran.

// 0x08B2DBF0 (a0 = a value): picks a (a0, a2) constant pair by the value's signed range — 0x14D/0xA6 above 0x14D,
// 0x10A/0x85 in [0xDF, 0x10B), 0xDE/0x6F in [0xBF, 0xDF), 0xBE/0x5F below (with a1 = 1 when it is not positive) — calls
// 0x08C7AF70 with a1 = the value and answers whether its result was 0.

// 0x08B46904 (a0 = object, a1 = argument): the object's virtual at +0x14, then a 0x10-byte record from
// 0x08B2CB18(word(a0+0x28), 0xC, 0x10) filled by 0x08C52354(record, a1) when the allocation answered, 0x08B1FBEC on
// object+0x2C with the record, and the object's virtual at +0x18 (a tail call).
} // namespace
} // namespace new_batch_main_7
} // namespace gt::game

#define NEWMAIN7(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_7::name, gt::ReturnKind::kind)
// The reporters.
// The boxed binary functors.
NEWMAIN7(0x08A5FD68u, func_08A5FD68, Word);
NEWMAIN7(0x08A602F0u, func_08A602F0, Word);
NEWMAIN7(0x08A73E84u, func_08A73E84, Word);
NEWMAIN7(0x08A73F04u, func_08A73F04, Word);
NEWMAIN7(0x08A73F84u, func_08A73F84, Word);
NEWMAIN7(0x08AB0A30u, func_08AB0A30, Word);
NEWMAIN7(0x08AB0F64u, func_08AB0F64, Word);
NEWMAIN7(0x08AB2510u, func_08AB2510, Word);
NEWMAIN7(0x08AB2710u, func_08AB2710, Word);
// One-off shapes.
NEWMAIN7(0x08C24A70u, func_08C24A70, Word);
NEWMAIN7(0x0886EC54u, func_0886EC54, Word);
NEWMAIN7(0x08879990u, func_08879990, Word);
NEWMAIN7(0x0888B360u, func_0888B360, Word);
NEWMAIN7(0x089F13E8u, func_089F13E8, Word);
NEWMAIN7(0x08A7CA54u, func_08A7CA54, Word);
NEWMAIN7(0x08AA65E4u, func_08AA65E4, Word);
NEWMAIN7(0x08AC4850u, func_08AC4850, Word);
NEWMAIN7(0x08AC7E5Cu, func_08AC7E5C, Word);
