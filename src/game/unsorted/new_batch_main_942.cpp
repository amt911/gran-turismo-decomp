// New batch, main session 942: functions earlier batches mismatched or skipped, re-read from `tools.disasm` and written
// by hand as structured C (draft, unverified: the differential gate decides). No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_942 {
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
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// Allegrex wsbw: the four bytes of a word reversed.
u32 wsbw(u32 value) { return __builtin_bswap32(value); }

// The value-object library: func_08A5BAD8(dst, object) assigns, func_08A5B9EC(object) destroys, func_08A5BB7C(message)
// is what the originals do with a missing pointer.
constexpr u32 kAssign = 0x08A5BAD8u;
constexpr u32 kDestroy = 0x08A5B9ECu;
constexpr u32 kReportNull = 0x08A5BB7Cu;
// The shared empty representation of the wide string type, and the reporter of a message object.
constexpr u32 kEmptyWideRep = 0x08D77498u;
constexpr u32 kReport = 0x08B62498u;

// The end of a function that built a value object at `temporary`: assigned to `target` (skipped when the target is
// that temporary itself), then the temporary is destroyed.
void keep_temporary(Runtime &rt, AllegrexContext &ctx, u32 target, u32 temporary) {
    if (target != temporary) call(rt, ctx, kAssign, {target, word(rt, temporary)});
    call(rt, ctx, kDestroy, {temporary});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08A1C408(target): a value object built at frame +4 by func_08A1C370 from the target, its word thrown as
// 0x08CAA224 when null, unwrapped by func_08A1C0F4, destroyed, func_08A1C11C turns it into one word, and
// func_08A85B1C builds the value object at frame +0 that the target receives (skipped when the target is that
// temporary); the temporary is destroyed last.

// 0x08A997D0(range, compare): sorts the words of range's [+0x14, +0x18) with compare — func_08C2EF40(begin, end,
// merge_count, compare) and then a tail jump to func_08C2E8F4(begin, end, compare). An empty range returns at once
// and leaves v0 alone. merge_count is 0 for one word, otherwise twice the number of halvings that reach one.

// 0x08AAE994(self): destructor-shaped. Stores the vtable 0x08CD5618 at +0, runs func_08C2ADCC from the three words at
// +0x3C, +0x40, +0x44 into the frame, frees the array those delimit (last - first as a word count times 4, when the
// first is not null), releases the member pair around +0x24, runs func_08AAE904 and frees self (0x48 bytes,
// alignment 4).
void func_08AAE994(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CD5618u);
    call(rt, ctx, 0x08C2ADCCu, {word(rt, self + 0x3Cu), word(rt, self + 0x40u), frame.at(0u)});
    const u32 first = word(rt, self + 0x3Cu);
    const u32 count = static_cast<u32>(signed_of(word(rt, self + 0x44u) - first) >> 2);
    if (first != 0u) call(rt, ctx, 0x08A5BC54u, {first, count << 2, 4u});
    call(rt, ctx, 0x08C2AE10u, {self + 0x24u, word(rt, self + 0x2Cu)});
    call(rt, ctx, 0x08AAE904u, {self});
    call(rt, ctx, 0x08A5BC54u, {self, 0x48u, 4u});
}

// 0x08B10DC4(self, other): takes the lock at self + 0x14, refills the list at self + 0x40 from other's list at +0x3C,
// takes other's lock at +4, flags other + 0xA4 = 1, refills the list at self + 0x4C, releases other's lock, runs
// func_08B28C74(self + 0x64) and tail jumps to the release of self's lock at +0x14.

// 0x08B3433C(out): zeroes out[0..0xC] and the 0x18 bytes of frame from the word at frame +0 (which holds frame +4),
// asks func_08C7A820(0x08CB5504, 0x02425818, frame, 4, 0, 0) and, when it answers 0, copies frame +4, +8, +0x10,
// +0x14 into out +0, +4, +8, +0xC. v0 is that answer on both paths.

// The nine report-shaped functions 0x08B5AADC..0x08B5B2DC: func_08C74A68 builds a value object at frame +4 from the
// argument, func_08B61E98(8) makes the 8-byte object, `build` fills it from the temporary, and the message is reported
// through func_08B62498(object, message, callback) — at once when the temporary's representation is the shared empty
// cell 0x08D77498, else after func_08B60804 has stepped the count at data - 4 down and func_08C74884 freed the cell
// when the old count was not positive. The report is the last thing done: the originals end with a `j` to it.
void report_holder(Runtime &rt, AllegrexContext &ctx, u32 build, u32 message, u32 callback) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, 0x08C74A68u, {temporary, self, frame.at(1u)});
    const u32 object = call(rt, ctx, 0x08B61E98u, {8u});
    call(rt, ctx, build, {object, temporary});
    const u32 data = word(rt, temporary);
    const u32 representation = data - 0xCu;
    if (representation == kEmptyWideRep) call(rt, ctx, kReport, {object, message, callback});
    const u32 references = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (signed_of(references) <= 0) call(rt, ctx, 0x08C74884u, {representation, frame.at(0u)});
    call(rt, ctx, kReport, {object, message, callback});
}

// 0x08BC1FE8(self): takes the lock at self->+0x10's +0x6C, refills that object's list at +8 from self->+0xC, and when
// func_08B2D1A8 answers the same as its +0x68 runs func_08B34CA0; unlocks, then tail jumps to
// func_08BBEA18(self->+0x10 + 0x98, self->+0x10).

// 0x08BD3088(out, begin, end, record): out = {+0: record->+0, +4: record->+4, +8: the byte record->+8}, one
// func_08AFA208 call per halfword of [begin, end) with (word(record->+4), record->+0, record->+8, the halfword,
// record->+4); v0 = out.

// 0x08C799C4(self, source): makes self a copy of the object at source: self->+0 = source->+0, and source->+8 and
// source->+0xC are stored at self plus the member offsets that source->+4 and source->+0 carry at -0xC; then
// func_08C771D4(self + 4) and func_08C54A7C(self plus the offset of its own +0, self + 4).

// 0x0880B938(self): the interval is the word at the object named by 0x08CFFAAC, at its +0xF0, as a number times 1e-6,
// clamped to [1/60, 0.05]; takes the lock at self, passes it in f12 to func_08869444(self + 0x30), then tail jumps to
// the unlock func_08B3457C(self).

// 0x0882F708(target): a value object at frame +4 built by func_088297A8, its word thrown as 0x08C9A2F8 when null,
// func_088294D0's answer unwrapped and destroyed, func_0885EBD0's 64-bit answer goes into func_08A5DEE8 at frame +0,
// which the target receives and which is destroyed last.

// 0x08833CAC(target): the value object at frame +0 (message 0x08C9A2F8 when null) unwrapped by func_088294D0, then
// func_0885EDF8 -> func_088DE9CC whose answer func_08A97A6C turns into the value object at frame +4 that the target
// receives; the target's temporary is destroyed, then the first one.

// 0x0883916C(target): the value object at frame +4 built by func_08838FBC, its word thrown as 0x08C9BBF8 when null,
// func_08838DC4's answer unwrapped and destroyed, func_08931970's 64-bit answer goes into func_08A5DEE8 at frame +0,
// which the target receives and which is destroyed last.

// 0x08841D04 / 0x08841D9C / 0x08841E34 / 0x08841F80 / 0x088420B0 / 0x08842908 (target): one shape — the value object
// at frame +4 built by func_088418B4 (message 0x08C9C584 when null), func_0884177C's answer unwrapped and destroyed,
// func_0888EB9C's member at `offset` byte-reversed (wsbw), which func_08A85B1C turns into the value object at frame +0
// that the target receives and destroys.
void swapped_member(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const u32 target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, 0x088418B4u, {temporary});
    u32 value = word(rt, temporary);
    if (value == 0u) call(rt, ctx, kReportNull, {0x08C9C584u});
    value = call(rt, ctx, 0x0884177Cu, {value});
    call(rt, ctx, kDestroy, {temporary});
    const u32 object = call(rt, ctx, 0x0888EB9Cu, {value});
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), wsbw(word(rt, object + offset))});
    keep_temporary(rt, ctx, target, frame.at(0u));
}

// 0x088E6548(holder): takes the lock at word(holder) + 0xEC, then for index 0..3 asks that object's virtual slot 0x50
// with the index; a non-zero answer sets the byte at word(holder) + 0x4AC + index. Sets the byte at +0x52B and tail
// jumps to the unlock of word(holder) + 0xEC.

// 0x089C7090(target): a value object at frame +8 built by func_089C5850 (message 0x08CA63A0 when null) unwrapped by
// func_089C4FD4, whose word goes into the argument word at frame +0; func_08977AE4 builds the value object at frame +4
// from it, which the target receives; then that temporary is destroyed, then the first one.

// 0x089C83A4(self): destroys the node at self + 0x2C (func_08C00FEC(node + 0x18, node->+0x20), func_08C012FC(node,
// node->+8), func_08B64654(node)); when the list sentinel self + 0x30 is empty it stores 0 at self + 0x2C and answers
// v0 = the sentinel, else it takes the next node (self->+0x34), stores that node's +8 at self + 0x2C (the call's delay
// slot), runs func_08B5C094(node) and tail jumps to the free of the node (0xC bytes, alignment 4).

} // namespace
} // namespace new_batch_main_942
} // namespace gt::game

#define NEW942(address, name) GT_DECOMP(address, gt::game::new_batch_main_942::name, gt::ReturnKind::Word)
NEW942(0x08AAE994u, func_08AAE994);
#undef NEW942
