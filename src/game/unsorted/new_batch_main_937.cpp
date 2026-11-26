// New batch (main session, 937): functions drafted before (batches 8 and 9) that never came out equal, or that the
// drafters skipped twice, rewritten as structured C from `tools.disasm`. Nothing here is verified yet: the synthetic
// cases and the differential gate come later.
//
// Families in this file:
// - twelve Adhoc comparison helpers (a signed/unsigned byte or half at this+0x14 against a virtual call's answer, in a
//   stack value object, then assigned to the caller's holder);
// - three "format a value into a std::string" wrappers and five small object helpers of the same library;
// - two hash walkers over a list of nodes (virtual slots 0x68 / 0x6C), a table-entry setup, a subsystem teardown, a
//   stream list lock/unlock pair and a byte-append helper that reads its count in t0.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_937 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's jal.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call whose v0 / v1 / a1 / f0 stay in the registers like the original's. A caller that reads v1 after a
// 64-bit answer, or a1 for the next call, depends on this.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v2, slot(v1); jalr v2`.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + slot);
    return call(rt, ctx, ctx.gpr[2], arguments);
}
// The variant whose argument registers are set like the original sets them before the jal (a0-a3 in order).
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call(rt, ctx, target, arguments);
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }

// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys,
// func_08A5BB7C(message) is its null-object assert.
constexpr u32 kAssign = 0x08A5BAD8u;
constexpr u32 kDestroy = 0x08A5B9ECu;
constexpr u32 kAssert = 0x08A5BB7Cu;
// The lock pair func_08B344E0(lock) / func_08B3457C(lock).
constexpr u32 kLock = 0x08B344E0u;
constexpr u32 kUnlock = 0x08B3457Cu;

// ---- the Adhoc comparison family ------------------------------------------------------------------------------------

enum class FieldKind { Byte, UnsignedByte, Half, UnsignedHalf };
enum class CompareOp { Equal, NotEqual, FieldAtLeast, ResultAtLeast, Product };

u32 load_field(Runtime &rt, u32 address, FieldKind kind) {
    switch (kind) {
    case FieldKind::Byte: return static_cast<u32>(static_cast<std::int8_t>(rt.memory().load8(address)));
    case FieldKind::UnsignedByte: return rt.memory().load8(address);
    case FieldKind::Half: return static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(address)));
    case FieldKind::UnsignedHalf: return rt.memory().load16(address);
    }
    return 0u;
}
bool field_is_signed(FieldKind kind) { return kind == FieldKind::Byte || kind == FieldKind::Half; }

// `this + 0x14` holds the left operand; a3 is a holder whose word 0 is the right operand, asked through its vtable slot
// `slot` with (object). The two are compared (xor ==/!= 0, `>=` in the field's own signedness, or multiplied), the
// result is built into a stack value object by `emit` and assigned to the caller's holder (a1).
void compare_field(Runtime &rt, AllegrexContext &ctx, u32 slot, FieldKind kind, CompareOp op, u32 message, u32 emit) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const u32 field = load_field(rt, self + 0x14u, kind);
    if (object == 0u) call(rt, ctx, kAssert, {message});
    const u32 table = word(rt, object);
    const u32 result = call_indirect(rt, ctx, word(rt, table + slot), {object});
    u32 argument = 0u;
    switch (op) {
    case CompareOp::Equal: argument = (field ^ result) == 0u ? 1u : 0u; break;
    case CompareOp::NotEqual: argument = (field ^ result) != 0u ? 1u : 0u; break;
    case CompareOp::FieldAtLeast:
        argument = field_is_signed(kind) ? (static_cast<s32>(field) >= static_cast<s32>(result) ? 1u : 0u)
                                         : (field >= result ? 1u : 0u);
        break;
    case CompareOp::ResultAtLeast:
        argument = field_is_signed(kind) ? (static_cast<s32>(result) >= static_cast<s32>(field) ? 1u : 0u)
                                         : (result >= field ? 1u : 0u);
        break;
    case CompareOp::Product:
        argument = static_cast<u32>(static_cast<s32>(field)) * static_cast<u32>(static_cast<s32>(result));
        break;
    }
    call(rt, ctx, emit, {frame.at(0u), argument});
    if (out != frame.at(0u)) call(rt, ctx, kAssign, {out, word(rt, frame.at(0u))});
    call(rt, ctx, kDestroy, {frame.at(0u)});
}

void func_08A9673C(Runtime &rt, AllegrexContext &ctx) { compare_field(rt, ctx, 0x34u, FieldKind::Byte, CompareOp::NotEqual, 0x08CB21E8u, 0x08A97A6Cu); }
void func_08A96944(Runtime &rt, AllegrexContext &ctx) { compare_field(rt, ctx, 0x34u, FieldKind::Byte, CompareOp::ResultAtLeast, 0x08CB21E8u, 0x08A97A6Cu); }

// ---- the std::string wrappers ---------------------------------------------------------------------------------------

// `producer(object)` fills a value object at `object_address`; a null one trips the library assert at `message`.
u32 produce(Runtime &rt, AllegrexContext &ctx, u32 producer, u32 object_address, u32 message) {
    call_set(rt, ctx, producer, {object_address});
    const u32 object = word(rt, object_address);
    if (object == 0u) call_set(rt, ctx, kAssert, {message});
    return object;
}
// The temporary string is assigned to `target` (skipped when the target is the temporary itself), then destroyed.
void keep_temporary(Runtime &rt, AllegrexContext &ctx, u32 target, u32 temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}
void keep_string(Runtime &rt, AllegrexContext &ctx, u32 target, u32 temporary, u32 object_address) {
    keep_temporary(rt, ctx, target, temporary);
    call_set(rt, ctx, kDestroy, {object_address});
}

// func_08843E7C(target): func_088435A8 builds an object in the frame, func_08843404(object) an entry whose byte at
// +0x80 goes into func_08A97A6C(string, byte); the string is assigned to target, then both temporaries are destroyed.

// func_08843F04(target): the same wrapper reading the byte at +0x81.

// func_0896DC4C(target): the value is the word at +0x94 of the produced object; func_08A76AAC builds the string from
// its address in the frame.

// ---- object plumbing ------------------------------------------------------------------------------------------------

// func_0885C538(self): under the lock at self+8, func_0885E430(self->+4), then func_0885C3C8(self); self->+0->+0x30 and
// +0x34 are cleared and eight 8-byte entries at +0x38 each store -1 in both words. v0 is the last entry's base.

// func_088A2364(self, index): unlinks the free node `index` of the table at self->+0xC (a signed byte link per node);
// when the node it pointed at is a real one, that node is marked -1 and func_088A2274 hands its 124-byte record (table
// self->+4) back. Then `index` itself is marked and its record handed back — a tail jump to func_088A2274.

// func_088BBC70(self, count, ...): func_088C50B8(hash, self->+0x1C, byte at self->+0x24 + 0x10) starts a hash over
// self->+0x24->+0x11 words, func_088C5324 feeds it `count` times and func_088C50C4 finishes it with the last answer
// plus one (a negative count skips the loop and only finishes, with byte-0x11 - 1).

// func_088DA8DC(self, index): func_088E89CC hands back the 760-byte record at self->+0xC0 + 760*index, then the object
// func_088F8D58(self->+0x14C, index) is locked by func_088F8938, its virtual slot 0x10 is called with 0, and slot 0x1C
// is the tail call (0 as well).

// func_0891FE68(self), a destructor: the 13 slots +0x250 .. +0x280 each release their object through its second virtual
// slot and are cleared; then func_08925FF8(self) and a tail jump to func_08B64654(self).

// func_08930F8C(self, second) -> 1 when func_08938CB4(func_089363FC(), func_08936578(object, a1, low, high), second)
// is non-zero: the pair comes from func_08931900(self), a pair of -1/-1 answers 0 without any of the three calls, and
// func_08936578 receives the a1 func_089363FC left behind.

// func_08ABB2E4(): walks the list head 0x08D77184 twice, calling slot 0x68 of each node's object first and slot 0x6C
// after func_08ABA3AC ran; v0 is the last call's answer (the first list's when the second is empty).

// func_08ACEB40(self, index, value): the entry at self + 240*index, whose +0x4F5 byte is already set, is left untouched
// (v0 keeps that byte); otherwise the byte is set, the value stored at +0x4C8, func_08B24238(value) called, and the
// region at +0x420 is set up by func_08ACEA1C and then handed to a tail jump into func_08ACEA8C.

// func_08AEFF5C(): func_08AF0910(0), then the two handles at 0x08D41AD0 + 0x538 / + 0x53C are released (each when
// non-negative, then set to -1), and the object at +0x540 is released by func_08AD85C8 with +0x544 and +0x540 cleared.
// v0 is that call's answer, or the +0x540 word (zero) when it is null.

// func_08B09824(self): under the lock 0x08D473E8, func_08B09394(self->0), func_08B3177C(self->0 + 0x120), the release of
// self->0->+0x110 through its virtual slot 0x20, func_08B093D4(self) when the byte at self->0 + 0x11C is set, and the
// unlock as the tail call.

// func_08B181E0(self): the word at +0x1874 is cleared, func_08B24D6C(self) and virtual slot 0x18 run, then
// func_08B7B2E4(self + 0x1878) picks: 0 runs func_08B25968 and continues, 2 returns self, anything else goes straight
// to virtual slot 0x1C with a1 = 1.

// func_08B322CC(): 1 when func_08B321C8(self) is non-zero and the four 0x08C7AFxx calls all answer non-negative (the
// last answer is stored at 0x08CE0C88); 0 otherwise.

// func_08B82FFC(): when 0x08D72E4C is not 1 it answers 0x80440002 untouched; else it frees the four words at
// 0x08D740D8 / 0x08D740EC / +4 / +8 and clears 0x08D740E4, 0x08D72E4C and 0x08D740E0, answering 0.

// func_08B8A43C(self, offset, source, count as a3, count as t0): func_08B8634C(self, offset, ?, t0) first; with t0
// non-zero, count 1 stores source's byte at self->0 + offset, any other count hands (self->0 + offset, source, t0) to
// func_08B79A88. v0 is always self.

// func_08C798FC(self), a derived destructor: vtable stores, func_08C786B4(self + 8), then the pointer pair at
// 0x08CDC2F8 + 4 / + 8 (its offset read at -0xC of the first), and func_08B6543C(self + 0xAC) before the tail chain.

} // namespace
} // namespace new_batch_main_937
} // namespace gt::game

#define NEW937(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_937::name, gt::ReturnKind::kind)
NEW937(0x08A9673Cu, func_08A9673C, Word);
NEW937(0x08A96944u, func_08A96944, Word);
