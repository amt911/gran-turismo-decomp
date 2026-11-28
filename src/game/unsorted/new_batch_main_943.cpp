// New batch, main session 943: the main-session-11 drafts that mismatched (destructor/list shapes, value-object
// glue and table dispatches), re-derived by hand from `tools.disasm` as structured C (draft, unverified: the
// differential gate decides). No names — none has evidence — every comment says what the instructions do.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_943 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

s32 signed_of(u32 value) { return static_cast<s32>(value); }

// A guest call: the argument registers are set as the original sets them before the jal (a0-a3, t0-t3 in order) and
// afterwards v0 holds the result.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same, keeping v1 / f0 (a 64-bit or float result) and the callee's a1, which the caller's next call can read.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : floats) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
GuestResult call_virtual_keep(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments = {},
                              std::initializer_list<float> floats = {}) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_keep(rt, ctx, ctx.gpr[2], arguments, floats);
}
u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments = {}) {
    return call_virtual_keep(rt, ctx, object, slot, arguments).v0;
}

// Allegrex trunc.w.s: toward zero; NaN and out of range give 0x7FFFFFFF. The integer travels in the float's bits
// (an mfc1 right after it moves it out).
s32 trunc_w_s(float value) {
    if (!(value > -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline s32 half_s(Runtime &rt, u32 address) { return static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The dispatch pair of the value-object library: func_08A5BAD8(dst, object) assigns, func_08A5B9EC(object) destroys.
constexpr u32 kAssign = 0x08A5BAD8u;
constexpr u32 kDestroy = 0x08A5B9ECu;
constexpr u32 kReportNull = 0x08A5BB7Cu;  // (message): what the originals do with a null pointer
constexpr u32 kEmptyStringRep = 0x08D74FD0u;
constexpr u32 kStringRepDestroy = 0x08B8542Cu;

// The end of a function that built a value object at `temporary`: assigned to `target` (skipped when the target is
// that temporary itself), then the temporary is destroyed.
void keep_temporary(Runtime &rt, AllegrexContext &ctx, u32 target, u32 temporary) {
    if (target != temporary) call(rt, ctx, kAssign, {target, word(rt, temporary)});
    call(rt, ctx, kDestroy, {temporary});
}

// The release of a libstdc++ copy-on-write string cell: nothing when the rep `data - 0xC` is the shared empty cell
// `empty`, otherwise func_08B60804(data - 4, -1) steps its count and `dispose`(rep, frame) frees it when the old
// count was not positive.
void release_cell(Runtime &rt, AllegrexContext &ctx, u32 data, u32 flag, u32 empty, u32 dispose) {
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;  // the delay slot of the compare
    if (data - 0xCu == empty) return;
    const u32 references = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = data - 0xCu;
    ctx.gpr[5] = flag;  // the delay slot of the branch
    if (signed_of(references) <= 0) call(rt, ctx, dispose, {data - 0xCu, flag});
}

// `produce` answers one word that is stored in the frame before `make` builds the value object from it.
void value_from_word(Runtime &rt, AllegrexContext &ctx, u32 build, u32 message, u32 unwrap, u32 make) {
    const u32 target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 outer = frame.at(8u);
    const u32 inner = frame.at(4u);
    call(rt, ctx, build, {outer});
    u32 value = word(rt, outer);
    ctx.gpr[4] = message;
    if (value == 0u) call(rt, ctx, kReportNull, {message});
    value = call(rt, ctx, unwrap, {value});
    put(rt, frame.at(0u), value);
    call(rt, ctx, make, {inner, frame.at(0u)});
    keep_temporary(rt, ctx, target, inner);
    call(rt, ctx, kDestroy, {outer});
}

// 0x089FD794(self): destructor of a list holder at self + 4. Stores the vtable 0x08CCFD88 at +0; from the first node
// self->+0x10 it runs each node's virtual slot 4 (a node at +0x14 of the previous one) and steps with func_08B5F100
// until self + 8; then func_08BF913C(self + 4, word(self + 0xC)) and a tail jump to func_08B64654(self).

// 0x08A025F4(target): a value object at frame +8 (message 0x08CA8B14) unwrapped by func_08A0095C, its word into the
// argument word at frame +0, func_08A76AAC builds the value object at frame +4 for the target; both are destroyed.

// 0x08A4064C(target, formatter, argument): the formatter's function at +0x14 runs with (its halfword +0x1A, argument,
// 4), func_08B85EB8 makes a string at frame +4 from its answer, func_08A6D2D4 gives it to target, and the string is
// released. v0 = target.

// 0x08A5767C(first, object): when object's virtual slot 0x28 answers 0, func_08A78490(first) runs and v0 = first.
// Otherwise func_08A56884 builds a value at frame +0 (null word reported as 0x08CAF778), the object's virtual slot 0xC
// runs with (first, word), the temporary is destroyed and v0 = first.

// 0x08A8DAB4(first, second, third): the vector at first + 0x14 gets one more element: when its end (+4) differs from its
// limit (+8) the slot is built by func_08AAE3A8(end, first + 0x20) and the end steps by 4, otherwise func_08C2B298
// grows it; then a tail jump to func_08A8D574(first, second, third).

// 0x08A927B8(first, hole, hole, other) / 0x08A92850: func_08A92548 builds a value object at frame +4 (with the a1
// argument) and another at frame +8 (with other); their words compared by xor ("equal" when the xor is below one, or
// for 0x08A92850 "equal" when it is zero) go to func_08A85B1C at frame +0, which the target receives; all three are
// destroyed.
void value_compare(Runtime &rt, AllegrexContext &ctx, bool equal_when_zero) {
    const u32 target = ctx.gpr[4];
    const u32 other = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 first = frame.at(4u);
    const u32 second = frame.at(8u);
    call(rt, ctx, 0x08A92548u, {first, ctx.gpr[5]});
    call(rt, ctx, 0x08A92548u, {second, other});
    const u32 differ = word(rt, first) ^ word(rt, second);
    const u32 answer = equal_when_zero ? (differ != 0u ? 1u : 0u) : (differ < 1u ? 1u : 0u);
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), answer});
    keep_temporary(rt, ctx, target, frame.at(0u));
    call(rt, ctx, kDestroy, {second});
    call(rt, ctx, kDestroy, {first});
}

// 0x08AA6908(target, source): func_08AA65E4(source, source) gives a string word; the target becomes the shared empty
// rep + 0xC (0x08D74FDC) and, when that word is not null, func_08B86674(target, word, 0) installs it, a negative count
// at word(target) - 4 first runs func_08B88FF8(target), and func_08AA61D8(source, word(target), word) finishes. v0 = target.
void func_08AA6908(Runtime &rt, AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 built = call(rt, ctx, 0x08AA65E4u, {source, source});
    put(rt, target, 0x08D74FDCu);
    if (built != 0u) {
        call(rt, ctx, 0x08B86674u, {target, built, 0u});
        if (signed_of(word(rt, word(rt, target) - 4u)) >= 0) call(rt, ctx, 0x08B88FF8u, {target});
        call(rt, ctx, 0x08AA61D8u, {source, word(rt, target), built});
    }
    ctx.gpr[2] = target;
}

// 0x08ABBE60(out, first, second, third): walks the list at 0x08D7718C (next at +0, object at +4) and asks each object's
// virtual slot 0x38 with (object, out, first, second, third); the first non-zero answer ends the walk and is 1. A walk
// that finds nothing stores a 0 byte at out and answers 0.

// 0x08B10FE8: when the flag at 0x08D47494 is clear, a tail jump to func_08B2E064 answers. Otherwise it walks the list at
// 0x08D47460 (next at +8, word at +0) running func_08B105D0, then the list at 0x08D77350 (next at +0, word at +4)
// running func_08B10F7C, then func_08B174D4, and clears the flag.

// 0x08B2D584: while the flag at 0x08D68D4C is set, func_08C7A988(0x64) runs; then func_08C7A820(0x08CB542C,
// 0x03415002, 0x08D68D48, 4, 0, 0), func_08B2D45C() and, when 0x08D68D44 is set, func_08B2EE2C and func_08B64654 on it;
// finally a tail jump to func_08C7AA00(word(0x08D68D48)).

// 0x08B61484(stream, position, remaining): feeds [position, position + remaining) to func_08B7E89C(stream, ...) until it
// consumed everything. A -1 answer asks func_08B77D40 whose word at +0 must be 4 to retry the same call, otherwise the
// loop ends. v0 = remaining bytes not consumed.

// 0x08C064C8(self, argument, second): a double dispatch through two tables. The entry at self + 0x18 picks the first
// function: with its low bit clear the function is self + 0x14, otherwise word(self + 0x14 + word(argument + (entry >> 1)));
// it runs with argument + (entry >> 1). The same with self + 0x28 / self + 0x24 gives the tail-called function; it runs
// with (first answer + entry >> 1) and the second argument in both cases, while its target is self + 0x24 when the low
// bit is clear, else word(self + 0x24 + word(that argument)).

// 0x08C792EC(self, source): like 0x08C799C4 with the member word at self + 4 cleared first and the initialization done
// on self + 8 (func_08C771D4 / func_08C54A7C).

// 0x088315AC(target): the value object at frame +4 (message 0x08C9A2F8) unwrapped and destroyed; func_0885EDEC of
// that value, then func_088D5D98(that + 4) gives the word of the value object at frame +0 that the target receives.

// 0x088334D8(target): the value object at frame +4 (message 0x08C9A2F8) unwrapped and destroyed, then func_0885EDF8 of
// it and func_088DA880 of that give the word of the value object at frame +0 that the target receives.

// 0x08833E1C, 0x08833FCC, 0x08834068 (target): the value object at frame +4 (message 0x08C9A2F8) unwrapped by
// func_088294D0; then func_0885EDF8 of it and the temporary's destroy happen in the order the original uses
// (`destroy_before`), then `make` of that word and func_08A97A6C build the value object at frame +0 that the target
// receives.
void value_through_list(Runtime &rt, AllegrexContext &ctx, u32 make, bool destroy_before) {
    const u32 target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, 0x088297A8u, {temporary});
    u32 value = word(rt, temporary);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (value == 0u) call(rt, ctx, kReportNull, {0x08C9A2F8u});
    value = call(rt, ctx, 0x088294D0u, {value});
    u32 object;
    if (destroy_before) {
        call(rt, ctx, kDestroy, {temporary});
        object = call(rt, ctx, 0x0885EDF8u, {value});
    } else {
        object = call(rt, ctx, 0x0885EDF8u, {value});
        call(rt, ctx, kDestroy, {temporary});
    }
    const u32 made = call(rt, ctx, make, {object});
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0u), made});
    keep_temporary(rt, ctx, target, frame.at(0u));
}

// 0x088345D4(target): the value object at frame +4 (message 0x08C9A2F8) unwrapped by func_088294D0, the temporary
// destroyed, then func_0885EDF8 of it, func_088DA460's float (f12) becomes the value object at frame +0 through
// func_08A87EEC, which the target receives.

// 0x08841BD4(target): the value object at frame +4 (message 0x08C9C584) unwrapped and destroyed, func_0888EB9C of it,
// func_0888FC14 of that word gives the value object at frame +0 through func_08A85B1C.

// 0x08856E5C(target): the value object at frame +4 (message 0x08C9E188) unwrapped, func_08854678 of it, func_088A5AA0's
// float kept in f20 while the temporary is destroyed, then func_08A87EEC makes the value object at frame +0 from it,
// which the target receives.

// 0x088578B0(target): the same start; func_088A6608's word, then func_08932598 of it, gives the value object at frame +0
// through func_08A97A6C.

// 0x088A5DB8(holder, argument): when func_088A579C(word(holder)) answers 0 the object is created (flag kept) and
// func_088A5D50 runs; then func_0892F95C(word(holder)->+0x4C0, 0x1A, argument) runs, the creation path releases with
// func_088A5748, and v0 is 1 unless both answer words are -1.

// 0x089C95D4(self): constructor/destructor pair. Stores the vtable 0x08CBB888 at +0, runs func_08C012FC(self + 0x14,
// word(self + 0x1C)), makes self + 0x18 the list sentinel of the member at self + 0x14 (+0x10 and +0xC point at it,
// +8 and +0x14 are 0), runs func_089C946C(self), func_08C01C34(self + 0x48, word(self + 0x50)), destroys self + 0x40,
// +0x3C, +0x38, func_08C00980(self + 0x30), func_08C012FC(self + 0x14, its +8), then a tail jump to func_08A780B0(self).

// 0x089D60F4(target): a value object at frame +0 (message 0x08CA6E2C), and func_08A76AAC builds the value object at
// frame +8 from frame +4, which holds that object's node at +0x24 when that node's +4 is set, else 0; the target
// receives it and both temporaries are destroyed.

// 0x08A14650(hole, argument, count, holder): with count > 0 the holder's object (null reported as 0x08CA9A08) answers
// its virtual slot 0x3C and that is the flag (1 when non-zero), else 1. func_08A14610 builds a value object at frame +0
// (message 0x08CA9A6C) that func_08A14374 gets with the flag, then it is destroyed.

// 0x08A33BD4(target): a value object at frame +4 (message 0x08CACCEC), func_08B1B668(self + 0x14) answers a 64-bit word
// that func_08A83160 turns into the value object at frame +0 (with its a1 and v1) for the target; the first temporary
// is destroyed in between.

// 0x08A3AC40(hole, argument, count, holder): with count > 0 the holder's object (null reported as 0x08CAD518) answers
// its virtual slot 0x54 in f0, else the value is 0.0. func_08A3A9D0 builds a value object at frame +0 (message
// 0x08CAD544) that func_08A3A4CC gets with f12 = the value; then it is destroyed.

// 0x08A738A8(holder, target, hole, other): func_08B67720 gets holder->+0x14 (a signed halfword) as a number in f12 and
// other->+0's virtual slot 0x38 (as an integer) in f13; its answer truncates toward zero and func_08A85B1C makes the
// value object at frame +0 from those bits, which the target receives. A null other is reported as 0x08CB0BA8.

} // namespace
} // namespace new_batch_main_943
} // namespace gt::game

#define NEW943(address, name) GT_DECOMP(address, gt::game::new_batch_main_943::name, gt::ReturnKind::Word)
NEW943(0x08AA6908u, func_08AA6908);
