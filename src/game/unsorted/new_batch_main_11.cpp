// New batch, main session 11: functions of 37-39 instructions the recompiler emitted, written by hand from
// `tools.disasm` as structured C (draft, unverified: the differential gate decides). No names — none has evidence —
// every comment says what the instructions do. Arguments: a0-a3, t0-t3 (EABI32); float arguments in f12-f15.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_11 {
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

// Allegrex wsbw: the four bytes of a word reversed.
u32 wsbw(u32 value) { return __builtin_bswap32(value); }

inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline s32 half_s(Runtime &rt, u32 address) { return static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// The dispatch pair of the value-object library: func_08A5BAD8(dst, object) assigns, func_08A5B9EC(object) destroys.
constexpr u32 kAssign = 0x08A5BAD8u;
constexpr u32 kDestroy = 0x08A5B9ECu;
constexpr u32 kReportNull = 0x08A5BB7Cu;  // (message): what the originals do with a null pointer
constexpr u32 kEmptyStringRep = 0x08D74FD0u;
constexpr u32 kStringRepDestroy = 0x08B8542Cu;
constexpr u32 kEmptyWideRep = 0x08D75298u;
constexpr u32 kWideRepDestroy = 0x08B93554u;

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

// A value object built in the frame at `temporary` from a static builder, its word thrown as `message` when null,
// unwrapped, destroyed, then `describe` turns it into the text of the message object the target receives.
void message_through_value(Runtime &rt, AllegrexContext &ctx, u32 build, u32 message, u32 unwrap, u32 describe) {
    const u32 target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, build, {temporary});
    u32 value = word(rt, temporary);
    ctx.gpr[4] = message;
    if (value == 0u) call(rt, ctx, kReportNull, {message});
    value = call(rt, ctx, unwrap, {value});
    call(rt, ctx, kDestroy, {temporary});
    const u32 text = call(rt, ctx, describe, {value});
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), text});
    keep_temporary(rt, ctx, target, frame.at(0u));
}

// The same, but `produce` answers a 64-bit value (v0:v1) that becomes the temporary's: func_08A5DEE8(temp, -, a2:a3).
void value_from_pair(Runtime &rt, AllegrexContext &ctx, u32 build, u32 message, u32 unwrap, u32 produce) {
    const u32 target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, build, {temporary});
    u32 value = word(rt, temporary);
    ctx.gpr[4] = message;
    if (value == 0u) call(rt, ctx, kReportNull, {message});
    value = call(rt, ctx, unwrap, {value});
    call(rt, ctx, kDestroy, {temporary});
    const GuestResult produced = call_keep(rt, ctx, produce, {value});
    call(rt, ctx, 0x08A5DEE8u, {frame.at(0u), produced.a1, produced.v0, produced.v1});
    keep_temporary(rt, ctx, target, frame.at(0u));
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

// ---------------------------------------------------------------------------------------------------------------
// 0x08A1B970 / 0x08A1BA04 / 0x08A1BC30 / 0x08A1C408: a value object built at frame +4, its word reported through
// 0x08A5BB7C(message) when null, unwrapped, destroyed, then two calls whose answer is stored in the message object
// at frame +0, which is assigned to the target and destroyed.

// 0x08A997D0(range, compare): sorts the words of range's [+0x14, +0x18) with compare — func_08C2EF40(begin, end,
// merge_count, compare) then a tail jump to func_08C2E8F4(begin, end, compare). An empty range returns at once and
// leaves v0 alone. merge_count is 0 for one word, otherwise twice the number of halvings that reach one.

// 0x08AAE994(self): destructor-shaped. Stores the vtable 0x08CD5618 at +0, runs func_08C2ADCC from the three words at
// +0x3C, +0x40, +0x44 into the frame, frees the array those delimit (count = (word(+0x44) - word(+0x3C)) / 4 words,
// when the base is not null), releases the member pair around +0x24, then the plain members, then frees self (0x48
// bytes, alignment 4).

// 0x08ABFDD0(self, key): takes the lock at self + 0xC, walks the binary tree at self + 4 down its +0x10 keys (signed)
// looking for the first node whose key is not below key->+0x10, then func_08B1FA04(self, node, key) and a tail call of
// the unlock func_08B3457C(self + 0xC). A missing tree calls with a null node.
void func_08ABFDD0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B344E0u, {self + 0xCu});
        u32 node = word(rt, self + 4u);
        if (node != 0u) {
            const s32 wanted = signed_of(word(rt, key + 0x10u));
            for (;;) {
                if (wanted < signed_of(word(rt, node + 0x10u))) break;
                node = word(rt, node + 8u);
                if (node == 0u) break;
            }
        }
        call(rt, ctx, 0x08B1FA04u, {self, node, key});
    }
    tail_call(ctx, 0x08B3457Cu, {self + 0xCu});
}

// 0x08B10170(target, attribute, first, second, byte): builds a record at frame +0x10 with
// func_08B100D0(frame + 0x10, attribute, &{first, second}, byte & 0xFF) and copies its 0x3C bytes to target; v0 = target.
void func_08B10170(Runtime &rt, AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 attribute = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0u), ctx.gpr[6]);
    put(rt, frame.at(4u), ctx.gpr[7]);
    call(rt, ctx, 0x08B100D0u, {frame.at(0x10u), attribute, frame.at(0u), ctx.gpr[8] & 0xFFu});
    for (u32 offset = 0u; offset != 0x3Cu; offset += 4u) put(rt, target + offset, word(rt, frame.at(0x10u + offset)));
    ctx.gpr[2] = target;
}

// 0x08B10DC4(self, other): runs the lock at self + 0x14, refills the two lists at self + 0x40 and self + 0x4C from
// other's list at +0x3C, flags other->+0xA4 = 1, unlocks other + 4, releases self + 0x64, then a tail jump to the
// unlock of self + 0x14.

// 0x08B3433C(out): zeroes out[0..0xC], asks func_08C7A820(0x08CB5504, 0x02425818, &{out+4, 0, 0, 0, 0}, 4, 0, 0)
// (with word(frame+0) pointing at frame+4) and, when it answers 0, copies frame +4, +8, +0x10, +0x14 into
// out +0, +4, +8, +0xC. v0 = the call's answer.

// The nine report-shaped functions 0x08B5AADC..0x08B5B2DC: func_08C74A68 builds a value object at frame +4 from the
// argument, func_08B61E98(8) makes the 8-byte object, `build` fills it from the temporary, and then either the shared
// generic message or, when the temporary's rep is the wide empty cell 0x08D77498, the specific one is reported through
// func_08B62498(object, message, callback); the cell is released (func_08B60804 / func_08C74884) in between, and the
// generic message is reported last.
void report_holder(Runtime &rt, AllegrexContext &ctx, u32 build, u32 specific_message, u32 generic_message, u32 callback) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, 0x08C74A68u, {temporary, self, frame.at(1u)});
    const u32 object = call(rt, ctx, 0x08B61E98u, {8u});
    call(rt, ctx, build, {object, temporary});
    const u32 data = word(rt, temporary);
    const u32 rep = data - 0xCu;
    ctx.gpr[4] = data - 4u;
    if (rep == 0x08D77498u) call(rt, ctx, 0x08B62498u, {object, specific_message, callback});
    const u32 references = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (signed_of(references) <= 0) call(rt, ctx, 0x08C74884u, {rep, frame.at(0u)});
    call(rt, ctx, 0x08B62498u, {object, generic_message, callback});
}

// 0x08BC1FE8(self): locks self->+0x10's +0x6C, refills its list at +8 from self->+0xC, and when func_08B2D1A8
// returns the same as that object's +0x68 runs func_08B34CA0; unlocks, then a tail jump to
// func_08BBEA18(self->+0x10 + 0x98, that object).

// 0x08BD3088(out, begin, end, record): out = {+0: record->+0, +4: record->+4, +8: record->+8}, one func_08AFA208 call
// per halfword in [begin, end) with (word(record->+4), record->+0, record->+8, halfword, record->+4); v0 = out.

// 0x08BE6674(from, count, source): for each of the count records, when `from` (which steps by 0x30) is not null, copies
// the 0x30-byte record of `source` into it: words +4, +8, floats +0xC..+0x1C, the table word 0x08CCDE00 at +0, floats
// +0x20..+0x2C.

// 0x08C799C4(self, source): makes self a copy of source: self->+0 = source->+0, and the two member object slots are
// copied at self + word(source->+X - 0xC) (X = 4 and 0); then func_08C771D4(self + 4) and
// func_08C54A7C(self + word(self->+0 - 0xC), self + 4).

// 0x0880B938(self): the interval is the word at 0x08CFFAAC's +0xF0 as a number times 1e-6, clamped to
// [1/60, 0.05]; takes the lock at self, passes it in f12 to func_08869444(self + 0x30), then a tail jump to the
// unlock func_08B3457C(self).

// 0x0880D148(self, text) / 0x0880D1E0: builds a string at frame +4 from text (past a leading '/'), passes it with
// self->+4 to `lookup`, releases the string (shared rep 0x08D74FD0, func_08B8542C) and answers the lookup's result.
void text_lookup(Runtime &rt, AllegrexContext &ctx, u32 lookup) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 target = word(rt, self + 4u);
    const u32 offset = byte_u(rt, text) == 0x2Fu ? 1u : 0u;
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), text + offset, frame.at(1u)});
    const u32 result = call(rt, ctx, lookup, {target, frame.at(4u)});
    release_cell(rt, ctx, word(rt, frame.at(4u)), frame.at(0u), kEmptyStringRep, kStringRepDestroy);
    ctx.gpr[2] = result;
}
void func_0880D148(Runtime &rt, AllegrexContext &ctx) { text_lookup(rt, ctx, 0x089A33F0u); }
void func_0880D1E0(Runtime &rt, AllegrexContext &ctx) { text_lookup(rt, ctx, 0x089A32C4u); }

// 0x0882F708(target): a value object at frame +4 (word reported as 0x08C9A2F8 when null) gives func_088294D0, is
// destroyed, func_0885EBD0's 64-bit answer (and its a1) goes into func_08A5DEE8 at frame +0, which is assigned to the
// target and destroyed.

// 0x08833CAC(target): the same start, then func_0885EDF8 -> func_088DE9CC whose answer func_08A97A6C turns into the
// value object at frame +4 that the target receives (the first temporary is destroyed last).

// 0x0883916C(target): the value object at frame +4 (message 0x08C9BBF8) unwrapped, destroyed, and func_08931970's
// 64-bit answer goes into func_08A5DEE8 at frame +0.

// 0x08841D04, 0x08841D9C, 0x08841E34, 0x08841F80, 0x088420B0, 0x08842908 (target): one shape — the value object at
// frame +4 (message 0x08C9C584) unwrapped and destroyed, func_0888EB9C's member at `offset` byte-reversed (wsbw), which
// func_08A85B1C turns into the value object at frame +0 that the target receives.
void value_from_swapped_member(Runtime &rt, AllegrexContext &ctx, u32 offset) {
    const u32 target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, 0x088418B4u, {temporary});
    u32 value = word(rt, temporary);
    ctx.gpr[4] = 0x08C9C584u;
    if (value == 0u) call(rt, ctx, kReportNull, {0x08C9C584u});
    value = call(rt, ctx, 0x0884177Cu, {value});
    call(rt, ctx, kDestroy, {temporary});
    const u32 object = call(rt, ctx, 0x0888EB9Cu, {value});
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), wsbw(word(rt, object + offset))});
    keep_temporary(rt, ctx, target, frame.at(0u));
}

// 0x08877CCC(self): a value object at frame +4, func_08AB856Cu's word makes func_08875584(self, word) the first
// answer and func_0886AFA8(self->+0) + 4 with it the second; then the wide string at frame +4 is released (empty cell
// 0x08D75298, func_08B93554). v0 stays the release's.

// 0x088E6548(holder): locks word(holder) + 0xEC, then for index 0..3 asks that object's virtual slot 0x50 with the
// index and sets byte object + 0x4AC + index to 1 when the answer is not 0; sets object + 0x52B to 1 and tail jumps to
// the unlock of object + 0xEC.

// 0x089C7090(target): a value object at frame +8 (message 0x08CA63A0) unwrapped by func_089C4FD4 whose word goes into
// the argument word at frame +0, func_08977AE4 builds the value object at frame +4 from it, which the target receives;
// then both temporaries are destroyed.

// 0x089C83A4(self): destroys the node at self + 0x2C (func_08C00FEC(node + 0x18, node->+0x20), func_08C012FC(node,
// node->+8), func_08B64654(node)), then when the list sentinel self + 0x30 is not empty takes its next node
// (self->+0x34), stores that node's +8 at self + 0x2C, runs func_08B5C094(node) and tail jumps to the free of the node
// (0xC bytes, alignment 4). The empty case stores 0 at self + 0x2C and answers v0 = self + 0x30.

// 0x089FD040(self): destructor. Stores the vtables 0x08CC7808 at +0 and 0x08CC79A4 at +0x38, destroys self + 0x68,
// releases the string at self->+0x64 (0x08D74FD0 / func_08B8542C), runs func_089BBAC8(self) and frees self (0x6C bytes).
void func_089FD040(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC7808u);
    put(rt, self + 0x38u, 0x08CC79A4u);
    call(rt, ctx, kDestroy, {self + 0x68u});
    release_cell(rt, ctx, word(rt, self + 0x64u), frame.at(0u), kEmptyStringRep, kStringRepDestroy);
    call(rt, ctx, 0x089BBAC8u, {self});
    call(rt, ctx, 0x08A5BC54u, {self, 0x6Cu, 4u});
}

// 0x089FD794(self): destructor of a list holder at self + 4. Stores the vtable 0x08CCFD88 at +0; from the first node
// self->+0x10 it runs each node's virtual slot 4 (a node at +0x14 of the previous one) and steps with func_08B5F100
// until self + 8; then func_08BF913C(self + 4, word(self + 0xC)) and a tail jump to func_08B64654(self).

// 0x08A025F4(target): a value object at frame +8 (message 0x08CA8B14) unwrapped by func_08A0095C, its word into the
// argument word at frame +0, func_08A76AAC builds the value object at frame +4 for the target; both are destroyed.

// 0x08A028F4(self): destructor. Stores the vtables 0x08CB9A00 at +0 and 0x08CB9C58 at +0x38, destroys self + 0x10C
// and self + 0x108, releases the string at self->+0x104 (0x08D74FD0 / func_08B8542C), runs func_089B8884(self + 0xE8)
// and func_0896BC0C(self).
void func_08A028F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CB9A00u);
    put(rt, self + 0x38u, 0x08CB9C58u);
    call(rt, ctx, kDestroy, {self + 0x10Cu});
    call(rt, ctx, kDestroy, {self + 0x108u});
    release_cell(rt, ctx, word(rt, self + 0x104u), frame.at(0u), kEmptyStringRep, kStringRepDestroy);
    call(rt, ctx, 0x089B8884u, {self + 0xE8u});
    call(rt, ctx, 0x0896BC0Cu, {self});
}

// 0x08A4064C(target, formatter, argument): the formatter's function at +0x14 runs with (its halfword +0x1A, argument,
// 4), func_08B85EB8 makes a string at frame +4 from its answer, func_08A6D2D4 gives it to target, and the string is
// released. v0 = target.

// 0x08A5767C(first, object): when object's virtual slot 0x28 answers 0, func_08A78490(first) runs and v0 = first.
// Otherwise func_08A56884 builds a value at frame +0 (null word reported as 0x08CAF778), the object's virtual slot 0xC
// runs with (first, word), the temporary is destroyed and v0 = first.

// 0x08A5E164 / 0x08A5E1FC(self, target, hole, holder): adds (or subtracts, 0x08A5E1FC) the 64-bit word at holder->+0's
// virtual slot 0x50 to self's 64-bit [s0 = +0x18, s1 = +0x1C], and func_08A5DEE8 makes the temporary at frame +0 from
// the two halves (its a1 passed on), which the target receives. A null holder is reported as 0x08CB0080.
void value_add_sub(Runtime &rt, AllegrexContext &ctx, bool subtract) {
    const u32 self = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 low = word(rt, self + 0x18u);
    const u32 high = word(rt, self + 0x1Cu);
    const u32 object = word(rt, holder);
    ctx.gpr[4] = 0x08CB0080u;
    if (object == 0u) call(rt, ctx, kReportNull, {0x08CB0080u});
    const GuestResult addend = call_virtual_keep(rt, ctx, object, 0x50u, {object});
    u32 sum_low;
    u32 sum_high;
    if (subtract) {
        sum_low = low - addend.v0;
        sum_high = high - addend.v1 - (sum_low > low ? 1u : 0u);
    } else {
        sum_low = low + addend.v0;
        sum_high = high + addend.v1 + (sum_low < low ? 1u : 0u);
    }
    call(rt, ctx, 0x08A5DEE8u, {frame.at(0u), addend.a1, sum_low, sum_high});
    keep_temporary(rt, ctx, target, frame.at(0u));
}
void func_08A5E164(Runtime &rt, AllegrexContext &ctx) { value_add_sub(rt, ctx, false); }
void func_08A5E1FC(Runtime &rt, AllegrexContext &ctx) { value_add_sub(rt, ctx, true); }

// 0x08A86078(holder, target, hole, other): func_08B67720 gets the word at holder + 0x14 (as an integer) in f12 and
// other->+0's virtual slot 0x3C (as an integer) in f13; its answer truncates toward zero (trunc.w.s) and
// func_08A85B1C makes the value object at frame +0 from those integer bits, which the target receives. A null other is
// reported as 0x08CB181C.
void func_08A86078(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    const u32 other = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const float first = static_cast<float>(signed_of(word(rt, holder + 0x14u)));
    const u32 object = word(rt, other);
    ctx.gpr[4] = 0x08CB181Cu;
    if (object == 0u) call(rt, ctx, kReportNull, {0x08CB181Cu});
    const u32 second = call_virtual_at(rt, ctx, object, 0x3Cu, {object});
    const float answer = call_keep(rt, ctx, 0x08B67720u, {}, {first, static_cast<float>(signed_of(second))}).f0;
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), static_cast<u32>(trunc_w_s(answer))});
    keep_temporary(rt, ctx, target, frame.at(0u));
}

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

// 0x08ABBE60(out, first, second, third): walks the list at 0x08D7718C (next at +0, object at +4) and asks each object's
// virtual slot 0x38 with (object, out, first, second, third); the first non-zero answer ends the walk and is 1. A walk
// that finds nothing stores a 0 byte at out and answers 0.

// 0x08B10204(target, attribute, byte): like 0x08B10170 but func_08B100D0 gets (frame + 0x10, attribute,
// &{0, 0}, byte & 0xFF) and copies 0x3C bytes of the record to target; v0 = target.
void func_08B10204(Runtime &rt, AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 attribute = ctx.gpr[5];
    const u32 value = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    call(rt, ctx, 0x08B100D0u, {frame.at(0x10u), attribute, frame.at(0u), value});
    for (u32 offset = 0u; offset != 0x3Cu; offset += 4u) put(rt, target + offset, word(rt, frame.at(0x10u + offset)));
    ctx.gpr[2] = target;
}

// 0x08B10FE8: when the flag at 0x08D47494 is clear, a tail jump to func_08B2E064 answers. Otherwise it walks the list at
// 0x08D47460 (next at +8, word at +0) running func_08B105D0, then the list at 0x08D77350 (next at +0, word at +4)
// running func_08B10F7C, then func_08B174D4, and clears the flag.

// 0x08B2D584: while the flag at 0x08D68D4C is set, func_08C7A988(0x64) runs; then func_08C7A820(0x08CB542C,
// 0x03415002, 0x08D68D48, 4, 0, 0), func_08B2D45C() and, when 0x08D68D44 is set, func_08B2EE2C and func_08B64654 on it;
// finally a tail jump to func_08C7AA00(word(0x08D68D48)).

// 0x08B61484(stream, position, remaining): feeds [position, position + remaining) to func_08B7E89C(stream, ...) until it
// consumed everything. A -1 answer asks func_08B77D40 whose word at +0 must be 4 to go on, otherwise the loop ends.
// v0 = remaining bytes not consumed.

// 0x08BBB6F8(self, destination, source): copies (word(self + 4) - source) / 4 records of two halfwords each from source
// to destination, advances destination by 4 until it equals word(self + 4) (func_08BBB6E8), and subtracts
// (source - destination) rounded down to 4 from word(self + 4). v0 = destination.
void func_08BBB6F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const s32 count = static_cast<s32>(word(rt, self + 4u) - source) >> 2;
    u32 at = destination;
    for (s32 index = 0; index < count; ++index) {
        put16(rt, at, half_u(rt, source + 4u * static_cast<u32>(index)));
        put16(rt, at + 2u, half_u(rt, source + 4u * static_cast<u32>(index) + 2u));
        at += 4u;
    }
    call(rt, ctx, 0x08BBB6E8u, {at, word(rt, self + 4u), frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((source - destination) & ~3u));
    ctx.gpr[2] = destination;
}

// 0x08C064C8(self, argument, second): a double dispatch through two tables. The entry at self + 0x18 picks the first
// function: with its low bit clear the function is self + 0x14, otherwise word(self + 0x14 + word(argument + (entry >> 1)));
// it runs with argument + (entry >> 1). The same with self + 0x28 / self + 0x24 gives the tail-called function, run
// with (self->+0x24 + the first answer) when its low bit is set, else with the answer + (entry >> 1).

// 0x08C792EC(self, source): like 0x08C799C4 with the member word at self + 4 cleared first and the initialization done
// on self + 8 (func_08C771D4 / func_08C54A7C).

// 0x088315AC(target): the value object at frame +4 (message 0x08C9A2F8) unwrapped and destroyed; func_0885EDEC of
// that value, then func_088D5D98(that + 4) gives the word of the value object at frame +0 that the target receives.

// 0x088334D8(target): the value object at frame +4 (message 0x08C9A2F8) unwrapped and destroyed, then func_0885EDF8 of
// it and func_088DA880 of that give the word of the value object at frame +0 that the target receives.

// 0x08833E1C, 0x08833FCC, 0x08834068 (target): the value object at frame +4 (message 0x08C9A2F8) unwrapped, func_0885EDF8
// of it, the temporary destroyed, then `make` of that word and func_08A97A6C which builds the value object at frame +0
// that the target receives.
void value_through_list(Runtime &rt, AllegrexContext &ctx, u32 make) {
    const u32 target = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 temporary = frame.at(4u);
    call(rt, ctx, 0x088297A8u, {temporary});
    u32 value = word(rt, temporary);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (value == 0u) call(rt, ctx, kReportNull, {0x08C9A2F8u});
    value = call(rt, ctx, 0x088294D0u, {value});
    const u32 object = call(rt, ctx, 0x0885EDF8u, {value});
    call(rt, ctx, kDestroy, {temporary});
    const u32 made = call(rt, ctx, make, {object});
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0u), made});
    keep_temporary(rt, ctx, target, frame.at(0u));
}

// 0x088345D4(target): the value object at frame +4 (message 0x08C9A2F8) unwrapped, func_0885EDF8 of it, the temporary
// destroyed, then func_088DA460's float (f12) becomes the value object at frame +0 through func_08A87EEC, which the
// target receives.

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

// 0x0892D3E0(hole, object, record): each of the four values at object + 0xBA, + 0xB8 (unsigned) and + 0x174, + 0x176
// (signed) is scaled by the byte at record + 4, + 3, + 5, + 6 and divided by 100 (the low 32 bits of the multiply by
// 0x51EB851F shifted by 37, with the sign fix for the signed ones). A leaf.

// 0x08936C84(self): constructor. Stores the vtable 0x08CC67B8 at +0, runs func_08938EAC on self + 0x1F4 and self + 0x2D0,
// sets +8 = -1 and +4 = 0, zeroes the 0x2C words at +0x8C and at +0x13C, zeroes +0x1EC and +0x1F0, and copies the two
// words at 0x08CA21E4 to self + 0x10 (self + 0xC first). v0 = self + 0xC.
void func_08936C84(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC67B8u);
    call(rt, ctx, 0x08938EACu, {self + 0x1F4u});
    call(rt, ctx, 0x08938EACu, {self + 0x2D0u});
    put(rt, self + 8u, 0xFFFFFFFFu);
    put(rt, self + 4u, 0u);
    for (u32 index = 0u; index != 0x2Cu; ++index) {
        put(rt, self + 0x8Cu + index * 4u, 0u);
        put(rt, self + 0x13Cu + index * 4u, 0u);
    }
    put(rt, self + 0x1ECu, 0u);
    put(rt, self + 0x1F0u, 0u);
    put(rt, self + 0xCu, word(rt, 0x08CA21E4u));
    put(rt, self + 0x10u, word(rt, 0x08CA21E8u));
    ctx.gpr[2] = self + 0xCu;
}

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

// 0x08A238A8(self): for each of the two objects at self + 0x5C and self + 0x60, when present: func_08ABB72C(f12 =
// self->+0x74 for the first, +0x80 for the second), func_08ABB6D4(f12, f13 = +0x7C / +0x84, +0x88) and
// func_08ABB6A4(self->+0x70 truncated to its float bits); the second one's last call is a tail jump.
void func_08A238A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        u32 object = word(rt, self + 0x5Cu);
        if (object != 0u) {
            call_keep(rt, ctx, 0x08ABB72Cu, {object}, {float_at(rt, self + 0x74u)});
            object = word(rt, self + 0x5Cu);
            call_keep(rt, ctx, 0x08ABB6D4u, {object}, {float_at(rt, self + 0x78u), float_at(rt, self + 0x7Cu)});
            object = word(rt, self + 0x5Cu);
            call(rt, ctx, 0x08ABB6A4u, {object, static_cast<u32>(trunc_w_s(float_at(rt, self + 0x70u)))});
        }
        object = word(rt, self + 0x60u);
        if (object == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        call_keep(rt, ctx, 0x08ABB72Cu, {object}, {float_at(rt, self + 0x80u)});
        object = word(rt, self + 0x60u);
        call_keep(rt, ctx, 0x08ABB6D4u, {object}, {float_at(rt, self + 0x84u), float_at(rt, self + 0x88u)});
    }
    tail_call(ctx, 0x08ABB6A4u, {word(rt, self + 0x60u), static_cast<u32>(trunc_w_s(float_at(rt, self + 0x70u)))});
}

// 0x08A33BD4(target): a value object at frame +4 (message 0x08CACCEC), func_08B1B668(self + 0x14) answers a 64-bit word
// that func_08A83160 turns into the value object at frame +0 (with its a1) for the target; the first temporary is
// destroyed in between.

// 0x08A3AC40(hole, argument, count, holder): with count > 0 the holder's object (null reported as 0x08CAD518) answers
// its virtual slot 0x54 in f0, else the value is 0.0. func_08A3A9D0 builds a value object at frame +0 (message
// 0x08CAD544) that func_08A3A4CC gets with f12 = the value; then it is destroyed.

// 0x08A738A8(holder, target, hole, other): func_08B67720 gets holder->+0x14 (a signed halfword) as a number in f12 and
// other->+0's virtual slot 0x38 (as an integer) in f13; its answer truncates toward zero and func_08A85B1C makes the
// value object at frame +0 from those bits, which the target receives. A null other is reported as 0x08CB0BA8.

// 0x08A7C194(first, second): a value object at frame +4 and func_08A7C0E0(frame + 8, &frame + 0 = first) builds the
// value at frame +8; its word (null reported as 0x08CB1348) gets func_08A9D890(word, second, frame + 8), which is the
// answer; both temporaries are destroyed.
void func_08A7C194(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 outer = frame.at(4u);
    const u32 inner = frame.at(8u);
    call(rt, ctx, 0x08A9DD14u, {outer});
    put(rt, frame.at(0u), first);
    call(rt, ctx, 0x08A7C0E0u, {inner, frame.at(0u)});
    const u32 value = word(rt, outer);
    ctx.gpr[4] = 0x08CB1348u;
    if (value == 0u) call(rt, ctx, kReportNull, {0x08CB1348u});
    const u32 result = call(rt, ctx, 0x08A9D890u, {value, second, inner});
    call(rt, ctx, kDestroy, {inner});
    call(rt, ctx, kDestroy, {outer});
    ctx.gpr[2] = result;
}

} // namespace
} // namespace new_batch_main_11
} // namespace gt::game

#define MAIN11(address, name) GT_DECOMP(address, gt::game::new_batch_main_11::name, gt::ReturnKind::Word)
MAIN11(0x08ABFDD0u, func_08ABFDD0);
MAIN11(0x08B10170u, func_08B10170);
MAIN11(0x0880D148u, func_0880D148);
MAIN11(0x0880D1E0u, func_0880D1E0);
MAIN11(0x089FD040u, func_089FD040);
MAIN11(0x08A028F4u, func_08A028F4);
MAIN11(0x08A5E164u, func_08A5E164);
MAIN11(0x08A5E1FCu, func_08A5E1FC);
MAIN11(0x08A86078u, func_08A86078);
MAIN11(0x08B10204u, func_08B10204);
MAIN11(0x08BBB6F8u, func_08BBB6F8);
MAIN11(0x08936C84u, func_08936C84);
MAIN11(0x08A238A8u, func_08A238A8);
MAIN11(0x08A7C194u, func_08A7C194);
