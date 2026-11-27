// Main-session batch 1618 (draft, UNVERIFIED): functions written by hand from `tools.disasm` as structured C, several
// started from earlier drafts (0x08B7B2E4 and 0x08C7A688 from batch 14, commit db6a712) and repaired against the asm.
// Delay slots are read as "before the branch / call" (a value set there is set before the callee runs), `v0` ends as the
// asm leaves it on every exit path, and every callee-saved register the original keeps a value in stays untouched: the
// work lives in C++ locals, never in ctx.gpr[16..23], so nothing has to be saved or restored. No names: none has
// evidence beyond what its comment says.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1618 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;

// A `jal`: afterwards v0 holds the callee's result, as the instruction after the call sees it.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same where the code after the call reads v1 or f0: those are the callee's.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
u8 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }

// 0x088DCEF4 (a0 = state, a1 = index): the element a1 of a 760-byte record array (base word at +0xC0) is handed to
// 0x08E9054 with the word at +0x618, the low half of `word(state + 0x14)`'s +0x20 field read through 0x08FE518 (its
// `v0 = v1 != 0 ? v1 : 0`) and f12 = 0; a3 = 0.

// 0x0894E614 (a0 = this): the constructor: vtable 0x08CC6818, words +4 / +0x10 / +0x14 / +0x18 / +0x1C zero, the member at
// +0x64 built (0x0895E63C) and the one at +0xE0 (0x08946698), then byte +0xC = 1 and words +0xDC / +8 / +0x20 zero.
// v0 = 1.
void func_0894E614(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CC6818u);
    put(rt, self + 4u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x1Cu, 0u);
    call(rt, ctx, 0x0895E63Cu, {self + 0x64u});
    call(rt, ctx, 0x08946698u, {self + 0xE0u});
    put_byte(rt, self + 0xCu, 1u);
    put(rt, self + 0xDCu, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 0x20u, 0u);
    ctx.gpr[2] = 1u;
}

// 0x08A4F51C (a0 = a small id): a 6-byte record is built on the stack: half at +0 = a0, half at +2 = a0 + 1, half at +4 =
// the half at 0x08CDF640 (the word at 0x08CDF63C is stored over the first four bytes before the halves), and it goes to
// 0x08B77C14(sp, 0x08CDF570, word(0x08CDF56C), 6, 0x08A4F4A8). v0 = 1 when that answered non-zero, else 0.
void func_08A4F51C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 id = ctx.gpr[4];
    const u32 table = word(rt, 0x08CDF63Cu);
    const u32 extra = half_at(rt, 0x08CDF63Cu + 4u);
    const u32 other = word(rt, 0x08CDF56Cu);
    put(rt, frame.at(0u), table);
    put_half(rt, frame.at(4u), extra);
    put_half(rt, frame.at(2u), id + 1u);
    put_half(rt, frame.at(0u), id);
    const u32 result = call(rt, ctx, 0x08B77C14u, {frame.at(0u), 0x08CDF570u, other, 6u, 0x08A4F4A8u});
    ctx.gpr[2] = result != 0u ? 1u : 0u;
}

// 0x0885F22C (a0 = object holder, f12 = a value): the value is stored as a float at word(a0) + 0x4634 and clamped below at
// 2 (f20 = 0 when it is negative, 2 when it is at most 1, 2 * value otherwise: `bc1fl` runs its delay slot only when the
// branch is not taken), then 0x0885F0B8's object gets f20 + 1 at +0x18. v0 = that object.

// 0x08956470 (a0 = this, f12 = an amount): f20 = 0x09559C4's value + the amount, then 0x09551B0(this) answers the object
// whose float at +0x10C scales f20; the word at this + 8 has a float at +0x758 scaled by 1 - f20. f0 = the sum.
void func_08956470(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const float amount = ctx.fpr[12];
    const GuestResult first = call_keep(rt, ctx, 0x089559C4u, {self});
    const float fraction = first.f0 + amount;
    const GuestResult second = call_keep(rt, ctx, 0x089551B0u, {self});
    const u32 inner = word(rt, self + 8u);
    const float rest = 1.0f - fraction;
    const float scale = float_at(rt, second.v0 + 0x10Cu);
    const float base = float_at(rt, inner + 0x758u);
    ctx.fpr[0] = psp_mul(fraction, scale) + psp_mul(base, rest);
    ctx.gpr[2] = second.v0;
}

// 0x089628B0 (a0 = this, a1 = an argument, a2 = a value): 0x08BE43B0(self, a1), the member at +8 takes (word(self), a2, sp)
// through 0x08BE3FB4, 0x08BE3B38(member, 0x80) answers what is stored at +0x3C, and +0x40 is built by 0x08B28D34.
// v0 = that last call's result.
void func_089628B0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    call(rt, ctx, 0x08BE43B0u, {self, argument});
    const u32 member = self + 8u;
    call(rt, ctx, 0x08BE3FB4u, {member, word(rt, self), value, frame.at(0u)});
    put(rt, self + 0x3Cu, call(rt, ctx, 0x08BE3B38u, {member, 0x80u}));
    call(rt, ctx, 0x08B28D34u, {self + 0x40u});
}

// 0x08A3552C (a0 = this): 0x08A76ACC(this), vtable 0x08CCE588, the members at +0x14 and +0x18 built by 0x08A3AB20, the one
// at +0x1C by 0x08A3A91C(self + 0x1C, self + 0x14), then float +0x24 = 1 and word +0x20 = 0.
void func_08A3552C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CCE588u);
    const u32 first = self + 0x14u;
    call(rt, ctx, 0x08A3AB20u, {first});
    call(rt, ctx, 0x08A3AB20u, {self + 0x18u});
    call(rt, ctx, 0x08A3A91Cu, {self + 0x1Cu, first});
    put_float(rt, self + 0x24u, 1.0f);
    put(rt, self + 0x20u, 0u);
}

// 0x08A3AA50 (a0 = this): 0x08A76ACC(this), vtable 0x08CBB9A0, word +0x1C = -1, float +0x20 = 1, byte +0x24 = 1, words +0x14
// / +0x18 / +0x28 = 0, byte +0x2C = 0; the members at +0x30 and +0x34 are built by 0x08A76AA0 (the second in a tail call).
void func_08A3AA50(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBB9A0u);
    put(rt, self + 0x1Cu, 0xFFFFFFFFu);
    put_float(rt, self + 0x20u, 1.0f);
    put_byte(rt, self + 0x24u, 1u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x28u, 0u);
    put_byte(rt, self + 0x2Cu, 0u);
    call(rt, ctx, 0x08A76AA0u, {self + 0x30u});
    tail_call(ctx, 0x08A76AA0u, {self + 0x34u});
}

// 0x08A54ED4 (a0 = this, a2 = a pointer): 0x08A52660(this), vtable 0x08CB75B8, the block at +0x38 is initialised (both its
// ends point at self + 0x3C), +0x50 = a2, +0x54 = -1, byte +0x58 = 0. v0 = -1.
void func_08A54ED4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[6];
    call(rt, ctx, 0x08A52660u, {self});
    put(rt, self, 0x08CB75B8u);
    const u32 block = self + 0x38u;
    const u32 storage = self + 0x3Cu;
    put(rt, block + 0x10u, storage);
    put(rt, block + 0x14u, 0u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, storage);
    put(rt, self + 0x50u, source);
    put(rt, self + 0x54u, 0xFFFFFFFFu);
    put_byte(rt, self + 0x58u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08A9E700 (a0 = this): 0x08B70F28(this) answers a pair (v0, v1) handed to 0x08B3767C(sp, 0x20, 0x08CB2718, a3, v0, v1);
// the text 0x08B35F60 formats from sp goes to the member at +0x20 through 0x08BEED1C(member, sp, text). v0 = this.
void func_08A9E700(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const GuestResult pair = call_keep(rt, ctx, 0x08B70F28u, {self});
    call(rt, ctx, 0x08B3767Cu, {frame.at(0u), 0x20u, 0x08CB2718u, ctx.gpr[7], pair.v0, pair.v1});
    const u32 member = word(rt, self + 0x20u);
    const u32 text = call(rt, ctx, 0x08B35F60u, {frame.at(0u)});
    call(rt, ctx, 0x08BEED1Cu, {member, frame.at(0u), text});
    ctx.gpr[2] = self;
}

// 0x08AD0FD8 (a0 = a vector of three floats, a1 = three signed bytes): the bytes become floats at +0 / +4 / +8, the
// magnitude 0x08B4A720 answers (a1 is the third byte by then) is square-rooted, and the function ends in 0x08B4A5E8 with
// f12 = 1 / sqrt and a0 = the vector.
void func_08AD0FD8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const s32 x = signed_byte(rt, source);
    const s32 y = signed_byte(rt, source + 1u);
    const s32 z = signed_byte(rt, source + 2u);
    put_float(rt, out, static_cast<float>(x));
    put_float(rt, out + 4u, static_cast<float>(y));
    put_float(rt, out + 8u, static_cast<float>(z));
    const GuestResult result = call_keep(rt, ctx, 0x08B4A720u, {out, static_cast<u32>(z)});
    ctx.fpr[12] = 1.0f / std::sqrt(result.f0);
    tail_call(ctx, 0x08B4A5E8u, {out});
}

// 0x08B28E38 (a0 = this, f12..f15): f13 = f15 - f14, f20 = f14, and 0x08B28E04(1) is asked with f12 = f12 - *(this + 8) - f14
// and that f13; f20 = (f14 + its f0) * f13 is added to the float at this. v0 = this.
void func_08B28E38(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const float f12 = ctx.fpr[12];
    const float factor = ctx.fpr[13];
    const float low = ctx.fpr[14];
    const float high = ctx.fpr[15];
    ctx.fpr[13] = high - low;
    const float base = float_at(rt, self + 8u);
    ctx.fpr[12] = f12 - base - low;
    const GuestResult result = call_keep(rt, ctx, 0x08B28E04u, {1u}, {ctx.fpr[12], ctx.fpr[13]});
    const float grown = low + result.f0;
    const float value = float_at(rt, self);
    ctx.fpr[0] = value + psp_mul(grown, factor);
    put_float(rt, self, ctx.fpr[0]);
    ctx.gpr[2] = self;
}

// 0x08B7B2E4 (a0 = a 0x5C-byte record): the callee-saved registers and f20..f31 are stored into it (no frame, nothing
// restored). v0 = 0.

// 0x08C7A688 (a0 = this, a1 = source): the source's vtable word goes to this, its +0x20 goes to this + the offset the
// vtable keeps at -0xC, its +0x24 to this + 8; then 0x08C786B4(this + 0xC, that +0x20 value) and 0x08C553B0(this, source + 4).
// v0 = the last call's result.

// 0x088D464C (a0 = this, a1 and a2 = two parameters): one stack record is built by 0x08B09F98 and fed the two parameters
// through 0x08B09660 and 0x08B096A4, handed to 0x088D45D8(this, record) and released by 0x08B09290(record).
// v0 = the release's result.
void func_088D464C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    call(rt, ctx, 0x08B09F98u, {frame.at(0u)});
    call(rt, ctx, 0x08B09660u, {frame.at(0u), first});
    call(rt, ctx, 0x08B096A4u, {frame.at(0u), second});
    call(rt, ctx, 0x088D45D8u, {self, frame.at(0u)});
    call(rt, ctx, 0x08B09290u, {frame.at(0u)});
}

// 0x089498F4 (a0 = this, a1, a2 and a3/t0 = four arguments): the signed half at +0x150 is replaced by a2's half while
// 0x089494D4(this, a1, sp, 0) runs, then 0x08949730(byte +0x155, sp, a3, t0), and the old half is put back.
// v0 = that second call's result.
void func_089498F4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 half = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    const u32 fifth = ctx.gpr[8];
    const u16 saved = rt.memory().load16(self + 0x150u);
    put_half(rt, self + 0x150u, half);
    call(rt, ctx, 0x089494D4u, {self, second, frame.at(0u), 0u});
    call(rt, ctx, 0x08949730u, {byte_at(rt, self + 0x155u), frame.at(0u), fourth, fifth});
    put_half(rt, self + 0x150u, saved);
}

// 0x08A12980 (a0 = a record of three words): (word(+8):word(+4)) * 1000000 is added to the 64-bit value
// 0x08B6F518(low, high, word(+0x10), 0) answers. v0 = the low word, v1 = the high one.
void func_08A12980(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u64 scale = 1000000ull;
    const u64 low = static_cast<u64>(word(rt, self + 8u)) * scale;
    const u64 high = static_cast<u64>(word(rt, self + 4u)) * scale;
    const GuestResult result = call_keep(rt, ctx, 0x08B6F518u,
                                         {static_cast<u32>(low), static_cast<u32>(low >> 32), word(rt, self + 0x10u), 0u});
    const u64 total = high + ((static_cast<u64>(result.v1) << 32) | result.v0);
    ctx.gpr[2] = static_cast<u32>(total);
    ctx.gpr[3] = static_cast<u32>(total >> 32);
}

// 0x08A7C8A4 (a0 = this, a2 = a parameter): two stack objects (sp and sp + 4) are built by 0x08A69654 and 0x08A74F00 (the
// second from the first and the parameter), handed to 0x08A7C5C8(this, sp + 4), and released by 0x08A5B9EC and 0x08A69D68.

// 0x08A7C910: the same shape as 0x08A7C8A4 with the second builder 0x08A74A4C.

// 0x08A7C97C: the same shape as 0x08A7C8A4 with the first builder 0x08A69B3C.

// 0x08A7C9E8: both builders differ from 0x08A7C8A4 (0x08A69B3C and 0x08A74A4C).

// 0x08B1CD20 (a0 = a buffer descriptor, a1 = a format, a2..t3 = six arguments): a six-word record of the arguments is built
// at sp + 0x28 and its address is also the fifth argument on the stack; 0x08B375C8(buffer + word(+8), word(+4) - word(+8),
// a1, &record) formats into the buffer ((+0) is its base, (+4) its end, (+8) the write offset), then 0x08B1CB80(this).

// 0x0884C5F4 (a0 = this, a2, a3, t0, t1): 0x08A76ACC(this), vtable 0x08CBD348, +0x18 = a2, +0x1C = a3, half +0x20 = t0,
// half +0x22 = t1. v0 = 0x08CBD348.
void func_0884C5F4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const u32 fifth = ctx.gpr[9];
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBD348u);
    put_half(rt, self + 0x22u, fifth);
    put(rt, self + 0x18u, second);
    put(rt, self + 0x1Cu, third);
    put_half(rt, self + 0x20u, fourth);
    ctx.gpr[2] = 0x08CBD348u;
}

// 0x08857F54 (a0 = this, a1, a2 = a byte): a 0x1C-byte record is allocated by 0x08A5BC20(0x1C, 4), built by
// 0x08857E1C(record, a1, byte), stored on the stack and handed to 0x08A76AB4(this, sp).
void func_08857F54(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6] & 0xFFu;
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x1Cu, 4u});
    call(rt, ctx, 0x08857E1Cu, {record, second, third});
    put(rt, frame.at(0u), record);
    call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x088A28C4 (a0 = this, a1, a2): a stack object is built by 0x088A79AC(sp, 0) and fed a2 through 0x088A5820, handed to
// 0x088A2898(this, a1, sp) whose result is kept, and released by 0x088A7548. v0 = the kept result.
void func_088A28C4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    call(rt, ctx, 0x088A79ACu, {frame.at(0u), 0u});
    call(rt, ctx, 0x088A5820u, {frame.at(0u), third});
    const u32 result = call(rt, ctx, 0x088A2898u, {self, second, frame.at(0u)});
    call(rt, ctx, 0x088A7548u, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// 0x08993E5C (a0 = this, a1 = a parameter): 0x08A0D658(this, a1), vtable 0x08CC89D0, the table at +0x38 = 0x08CC8C28, the
// member at +0x120 built by 0x08A17B10(self + 0x120, a1), words +0x134..+0x14C zero. v0 = that call's result.

// 0x089B41E8 (a0 = this): 0x08A02B98(this), vtable 0x08CBB320, the second table at +0x38 = 0x08CBB578, +0x118 =
// 0x08D74FDC, the member at +0x11C built by 0x08A03794(self + 0x11C, 0x08CF6ED8), +0x120 by 0x08B487AC, then bit 3 of the
// word at +0x60 is set. v0 = that word.
void func_089B41E8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08A02B98u, {self});
    put(rt, self, 0x08CBB320u);
    put(rt, self + 0x38u, 0x08CBB578u);
    put(rt, self + 0x118u, 0x08D74FD0u + 0xCu);
    call(rt, ctx, 0x08A03794u, {self + 0x11Cu, 0x08CF6ED8u});
    call(rt, ctx, 0x08B487ACu, {self + 0x120u});
    const u32 flags = word(rt, self + 0x60u) | 8u;
    put(rt, self + 0x60u, flags);
    ctx.gpr[2] = flags;
}

// 0x089F5A00 (a0 = this, a1, a2): a 0x24-byte record is allocated by 0x08A5BC20(0x24, 4), built by 0x089F5918(record, a1,
// a2), stored on the stack and handed to 0x089FF120(this, sp).
void func_089F5A00(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    call(rt, ctx, 0x089F5918u, {record, second, third});
    put(rt, frame.at(0u), record);
    call(rt, ctx, 0x089FF120u, {self, frame.at(0u)});
}

// 0x089F6054: the same shape as 0x089F5A00 with the builder 0x089F5F6C.
void func_089F6054(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    call(rt, ctx, 0x089F5F6Cu, {record, second, third});
    put(rt, frame.at(0u), record);
    call(rt, ctx, 0x089FF120u, {self, frame.at(0u)});
}

// 0x08A0BDD8: the same shape as 0x089F5A00 with the builder 0x08A0BA2C and the receiver 0x08965854.
void func_08A0BDD8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 record = call(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    call(rt, ctx, 0x08A0BA2Cu, {record, second, third});
    put(rt, frame.at(0u), record);
    call(rt, ctx, 0x08965854u, {self, frame.at(0u)});
}

} // namespace
} // namespace new_batch_main_1618
} // namespace gt::game

#define NEW1618(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1618::name, gt::ReturnKind::kind)
NEW1618(0x0894E614u, func_0894E614, Word);
NEW1618(0x08A4F51Cu, func_08A4F51C, Word);
NEW1618(0x08956470u, func_08956470, Float);
NEW1618(0x089628B0u, func_089628B0, Word);
NEW1618(0x08A3552Cu, func_08A3552C, Word);
NEW1618(0x08A3AA50u, func_08A3AA50, Word);
NEW1618(0x08A54ED4u, func_08A54ED4, Word);
NEW1618(0x08A9E700u, func_08A9E700, Word);
NEW1618(0x08AD0FD8u, func_08AD0FD8, Void);
NEW1618(0x08B28E38u, func_08B28E38, Word);
NEW1618(0x088D464Cu, func_088D464C, Word);
NEW1618(0x089498F4u, func_089498F4, Word);
NEW1618(0x08A12980u, func_08A12980, DoubleWord);
NEW1618(0x0884C5F4u, func_0884C5F4, Word);
NEW1618(0x08857F54u, func_08857F54, Word);
NEW1618(0x088A28C4u, func_088A28C4, Word);
NEW1618(0x089B41E8u, func_089B41E8, Word);
NEW1618(0x089F5A00u, func_089F5A00, Word);
NEW1618(0x089F6054u, func_089F6054, Word);
NEW1618(0x08A0BDD8u, func_08A0BDD8, Word);
