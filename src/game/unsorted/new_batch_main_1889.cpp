// New batch, main session 926: 30 functions an earlier draft left unverified, rewritten body by body from
// `tools.disasm`. Most are one of four shapes, held by the helpers below: a scope opened on the frame (the maker is the
// `as class` holder template, its source is a1) whose object is looked up (+0x18) and handed to an action
// (0x08818xxx/0x0882xxxx/0x08847xxx/0x08854xxx/0x08A1Cxxx), the same scope copying two floats, the same scope waiting
// for a getter's value and passing it with the object's member at +0x14, and a byte stored from a virtual getter. v0 is
// left as the original leaves it on every path: the last call's answer, which the helpers put in v0 in asm order.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1889 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The holder destructor and the assert the scope wrappers share.
constexpr u32 kHolderDestroy = 0x08A5B9ECu;  // (holder *)
constexpr u32 kAssertMessage = 0x08A5BB7Cu;  // (const char *)

// `as class` maker on the frame (its source is a1), object = *holder, null-checked, `lookup(node)` gives the value, the
// holder is destroyed and the value goes to `action`; v0 = the action's. The frame is 0x20 for every one of them.
void scope_lookup_action(Runtime &rt, AllegrexContext &ctx, u32 maker, u32 message, u32 lookup, u32 action) {
    GuestFrame frame(ctx, 0x20u);
    (void)call(rt, ctx, maker, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) (void)call(rt, ctx, kAssertMessage, {message});
    const u32 value = call(rt, ctx, lookup, {object});
    (void)call(rt, ctx, kHolderDestroy, {frame.at(0)});
    ctx.gpr[2] = call(rt, ctx, action, {value});
}

// The same scope, but the two floats at +0xF0+0x40 / +0x44 of the object are copied to +0xBC / +0xC0 (both loaded before
// either store, as the asm) and the holder is destroyed; v0 = the destructor's answer.
void scope_copy_float_pair(Runtime &rt, AllegrexContext &ctx, u32 maker, u32 message) {
    GuestFrame frame(ctx, 0x20u);
    (void)call(rt, ctx, maker, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) (void)call(rt, ctx, kAssertMessage, {message});
    const u32 base = object + 0xF0u;
    const float first = float_at(rt, base + 0x40u);
    const float second = float_at(rt, base + 0x44u);
    put_float(rt, base + 0xBCu, first);
    put_float(rt, base + 0xC0u, second);
    ctx.gpr[2] = call(rt, ctx, kHolderDestroy, {frame.at(0)});
}

// The same scope, but the object's member at +0x14 and `getter`'s answer go to `action(member, value)`; the getter is
// called with a0 still the assert message (the compiler never reloads it), then the holder is destroyed and v0 = its answer.
void scope_member_action(Runtime &rt, AllegrexContext &ctx, u32 maker, u32 message, u32 getter, u32 action) {
    GuestFrame frame(ctx, 0x20u);
    (void)call(rt, ctx, maker, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) (void)call(rt, ctx, kAssertMessage, {message});
    const u32 member = word(rt, object + 0x14u);
    const u32 value = call(rt, ctx, getter, {message});
    (void)call(rt, ctx, action, {member, value});
    ctx.gpr[2] = call(rt, ctx, kHolderDestroy, {frame.at(0)});
}

// MIPS `ins rt, rs, pos, size`: the `size` low bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned position, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << position;
    return (target & ~mask) | ((value << position) & mask);
}

// --- script attribute from a virtual getter ---------------------------------------------------------------------------

// 0x08C247E4 (a0 = the parameter block, a1 = pointer to the object): the object is null-checked against 0x08CB06B8, its
// vtable slot 0x44 answers and the answer's low byte goes to +0x14 of a0; v0 = the getter's answer (only the byte is stored).

// --- scope -> lookup -> action ---------------------------------------------------------------------------------------

// 0x0881800C: 0x08817FCC(scope), 0x08817A54(+0x14 getter) -> 0x08817A0C.

// 0x0882994C: 0x088297A8(scope), 0x088294D0(+0x18 getter) -> 0x0885EEE0.

// 0x08829E58: the same scope and getter, action 0x0885F46C.

// 0x08829EB0: action 0x0885EBB0.

// 0x08829F08: action 0x0885E414.

// 0x08829F60: action 0x0885F460.

// 0x08829FB8: action 0x0885F444.

// 0x0882A010: action 0x0885EA48.

// 0x0882A068: action 0x0885E940.

// 0x0882A118: action 0x0885E8DC.

// 0x0882A170: action 0x0885E878.

// 0x0882A220: action 0x0885E7DC.

// 0x0882A480: action 0x0885E764.

// 0x0882A4D8: action 0x0886006C.

// 0x0882A588: action 0x088622B8.

// 0x0882A5E0: action 0x08863340.

// 0x08847580: 0x08847008(scope), 0x08846E50(+0x14 getter) -> 0x088D5AFC.

// 0x08847958: action 0x088D4D24.

// 0x088479B0: action 0x088D59B8.

// 0x088548B0: 0x088547C4(scope), 0x08854678(+0x14 getter) -> 0x088A719C.

// 0x08A1C3B0: 0x08A1C370(scope), 0x08A1C0F4(+0x14 getter) -> 0x08A1C0FC.

// --- scope -> float pair copy ----------------------------------------------------------------------------------------

// 0x0897E884: 0x0897E844(scope), floats +0x130 / +0x134 to +0x1AC / +0x1B0, scope closed.

// 0x0898970C: the same with 0x089896CC(scope) and the message 0x08CA39F0.

// --- scope -> member + getter -> action ------------------------------------------------------------------------------

// 0x089AB444: 0x089AB370(scope), member +0x14, 0x089D5B30() as the value, 0x08978D3C(member, value), scope closed.

// 0x089AB49C: the same with 0x08965418() and 0x0897BB18(member, value).

// 0x089AB4F4: 0x08965418() and 0x08978D3C(member, value).

// --- one-off bodies --------------------------------------------------------------------------------------------------

// 0x0897851C (a0 = an object): initialises the two intrusive list heads at +0x184 and +0x18C with 0x08BEDDC4 and makes
// each one its own successor (the first head's successor sits in the second call's delay slot); v0 = the second call's.

// 0x08A0F800 (a0 = an object, a1 = pointer to a word): 0x08B20920(frame, 0x08C8E004) and 0x08B20A54(frame, word, 0) give
// four bits that replace bits 24..27 of the object's +0x60 (`ins`); v0 = the second call's answer.

// 0x08A26ED8 (a0 = an object): 0x08B344E0(object), the member at +0x28 is taken and cleared in 0x08B3457C's delay slot,
// the frame is popped, and a non-null member is tail-called through 0x08B1B99C; otherwise v0 = 0x08B3457C's answer.
void func_08A26ED8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 member = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call(rt, ctx, 0x08B344E0u, {self});
        member = word(rt, self + 0x28u);
        put(rt, self + 0x28u, 0u);
        ctx.gpr[2] = call(rt, ctx, 0x08B3457Cu, {self});
    }
    if (member == 0u) return;
    tail_call(ctx, 0x08B1B99Cu, {member});
}

} // namespace
} // namespace new_batch_main_1889
} // namespace gt::game

#define NEW926(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1889::name, gt::ReturnKind::kind)
NEW926(0x08A26ED8u, func_08A26ED8, Word);
