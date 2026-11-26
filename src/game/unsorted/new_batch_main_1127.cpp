// Main-session batch 1127 (draft only, unverified: Claude runs the gate). Structured C from tools.disasm.
// Families: the 16-insn "select 0x08CF6ED8 then tail" wrappers, four float wrappers that collect into a stack buffer,
// clamp a value and hand it to 0x08AE0F64, two 3x4 matrix appliers, a packer, and the big constructors.
// Input shapes: /home/andres/repos/gt-ops/shapes/shapes_main1127.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1127 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

enum class Load { Word, Half, Byte, UnsignedByte };

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// A guest call whose float result is what the next instruction reads ($f0 of the callee).
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                  std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    return result;
}
// cvt.s.w: a guest word as the signed integer the FPU converts.
float signed_to_float(u32 value) { return static_cast<float>(static_cast<s32>(value)); }

// 0x08AC7288 (constructor): a0+8 and a0+0x438 are constructed, the 0x550..0x5BC block is cleared and set up (1.0f at
// +0x554, -1 at +0x598 / +0x59C), a0+0x578 is initialised with (a0+0x22680, 2), a0+0x5BC is set up and the 0x400-byte
// block at a0+0x34 is zeroed; v0 = the last callee's, and the two words at +0 and +4 are cleared last.
void func_08AC7288(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08B28D34u, {self + 8u});
    (void)call(rt, ctx, 0x08B0CC04u, {self + 0x438u});
    put8(rt, self + 0x565u, 0u);
    put_float(rt, self + 0x554u, 1.0f);
    put8(rt, self + 0x566u, 0u);
    put(rt, self + 0x550u, 0u);
    put8(rt, self + 0x567u, 0u);
    put(rt, self + 0x558u, 0u);
    put(rt, self + 0x55Cu, 0u);
    put(rt, self + 0x560u, 0u);
    put8(rt, self + 0x564u, 0u);
    put8(rt, self + 0x568u, 0u);
    put8(rt, self + 0x569u, 0u);
    put(rt, self + 0x56Cu, 0u);
    put(rt, self + 0x570u, 0u);
    put(rt, self + 0x574u, 0u);  // the delay slot of the jal
    (void)call(rt, ctx, 0x08C3CCE0u, {self + 0x578u, self + 0x22680u, 2u});
    put(rt, self + 0x59Cu, 0xFFFFFFFFu);
    put(rt, self + 0x598u, 0xFFFFFFFFu);
    put8(rt, self + 0x594u, 0u);
    put(rt, self + 0x5A0u, 0u);
    put(rt, self + 0x5A4u, 0u);
    put(rt, self + 0x5A8u, 0u);
    put(rt, self + 0x5ACu, 0u);
    put(rt, self + 0x5B0u, 0u);
    put(rt, self + 0x5B4u, 0u);
    put(rt, self + 0x5B8u, 0u);  // the delay slot of the jal
    (void)call(rt, ctx, 0x08AC6974u, {self + 0x5BCu});
    (void)call(rt, ctx, 0x08B36018u, {self + 0x34u, 0u, 0x400u});
    put(rt, self + 4u, 0u);
    put(rt, self, 0u);
}

// The 16-insn wrapper idiom: 0x08A52AB0(a0, 0x08CF6ED8) selects the member, then the tail gets the original (a0, a1).
void func_08A59CEC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4], other = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A52AB0u, {self, 0x08CF6ED8u});
    tail_call(ctx, 0x08A59C00u, {self, other});
}
void func_08A5A1B4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4], other = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A52AB0u, {self, 0x08CF6ED8u});
    tail_call(ctx, 0x08A5A0C8u, {self, other});
}
void func_08A59358(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4], other = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A52AB0u, {self, 0x08CF6ED8u});
    tail_call(ctx, 0x08A5926Cu, {self, other});
}
void func_08A7B1A8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4], other = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A5D190u, {self, 0x08CF6ED8u});
    tail_call(ctx, 0x08A7AF84u, {self, other});
}
void func_08AB4DC0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4], other = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A76AB4u, {self, 0x08CF6ED8u});
    tail_call(ctx, 0x08AB4C40u, {self, other});
}

// 0x08AE1794 (f12 = value, f13 = second float): 0x08AEB808(f12), then a stack buffer of 4 x word(0x08D4069C) bytes is
// collected by 0x08ADE2B4(buffer, 0) and sized by 0x08AEB82C(buffer, 0). The value is clamped to [0x08CB60F8, 0x08CB60F4],
// scaled by 0x08CB60FC and truncated, and 0x08AE0F64(buffer, collected, integer, f12 = f13) is the answer.

// 0x08AFC484 (a0 = object, f12..f19): 0x08AFBF3C(f12, f13), the virtual slot 0x18 of the object, then
// 0x08B5A838(f12 = f16, f13 = f17, f14 = f18, f15 = f19). The signed words at +0x18 / +0x1C are converted to floats (w, h)
// and the tail 0x08B557B0 gets f12 = w x f12, f13 = h x f13, f14 = w x f12 + w x f14, f15 = h x f13 + h x f15.
void func_08AFC484(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const float f27 = ctx.fpr[15], f26 = ctx.fpr[14], f25 = ctx.fpr[19], f24 = ctx.fpr[18];
    const float f23 = ctx.fpr[17], f22 = ctx.fpr[16], f21 = ctx.fpr[12], f20 = ctx.fpr[13];
    (void)callf(rt, ctx, 0x08AFBF3Cu, {self}, {f21, f20});
    const u32 vtable = word(rt, self);
    (void)call_indirect(rt, ctx, word(rt, vtable + 0x18u), {self});
    (void)callf(rt, ctx, 0x08B5A838u, {self}, {f22, f23, f24, f25});
    const float w = signed_to_float(word(rt, self + 0x18u));
    const float h = signed_to_float(word(rt, self + 0x1Cu));
    const float x = psp_mul(w, f21);
    const float y = psp_mul(h, f20);
    const float x_end = x + psp_mul(w, f26);
    const float y_end = y + psp_mul(h, f27);
    ctx.fpr[20] = f20;
    ctx.fpr[21] = f21;
    ctx.fpr[22] = f22;
    ctx.fpr[23] = f23;
    ctx.fpr[24] = f24;
    ctx.fpr[25] = f25;
    ctx.fpr[26] = f26;
    ctx.fpr[27] = f27;
    ctx.fpr[12] = x;
    ctx.fpr[13] = y;
    ctx.fpr[14] = x_end;
    ctx.fpr[15] = y_end;
    tail_call(ctx, 0x08B557B0u, {self});
}

// The other three float wrappers share the body above: 0x08ADE328 / 0x08ADE3A8 / 0x08ADE4AC collect into the stack
// buffer with (a0, a0 of the caller, 0) and 0x08AEB82C gets (buffer, caller's a0); the clamp constants are their own.

// 0x08B1BD50 (a0 = object): 0x08B344E0(a0), then f20 = f0 of 0x08B1B0F4(a0+0x98) and 0x08B1BBEC(a0+0x28). +0x8C gets
// f20 + word(+0x90) - word(+0x84) as a float, cleared to 0 when 1.0f > word(+0x90) and word(+0x84) < f20; +0x88 gets
// trunc(f20 x 64) / 64 clamped to [0, 1] (below 1 but not below 0 becomes 1.0f, as the asm); +0x84 = f20, +0x90 = 0,
// 0x08B1BC8C(a0+0x78) and the tail 0x08B3457C(a0).

// 0x08B4E780 (out = a0, matrix = a1, vector = a2): 0x08B4E734(out, matrix, vector) first; then out's +0x30..+0x3C gets
// the columns of the 3x4 matrix (the transpose: out += matrix^T x vector) added. v0 = 0x08B4E734's.
void func_08B4E780(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4], matrix = ctx.gpr[5], vector = ctx.gpr[6];
    (void)call(rt, ctx, 0x08B4E734u, {out, matrix, vector});
    const float m0 = float_at(rt, matrix), m1 = float_at(rt, matrix + 4u), m2 = float_at(rt, matrix + 8u),
                m3 = float_at(rt, matrix + 0xCu), m4 = float_at(rt, matrix + 0x10u), m5 = float_at(rt, matrix + 0x14u),
                m6 = float_at(rt, matrix + 0x18u), m7 = float_at(rt, matrix + 0x1Cu), m8 = float_at(rt, matrix + 0x20u),
                m9 = float_at(rt, matrix + 0x24u), m10 = float_at(rt, matrix + 0x28u), m11 = float_at(rt, matrix + 0x2Cu);
    const float c0 = float_at(rt, vector + 0x30u), c1 = float_at(rt, vector + 0x34u), c2 = float_at(rt, vector + 0x38u);
    const float o0 = float_at(rt, out + 0x30u), o1 = float_at(rt, out + 0x34u), o2 = float_at(rt, out + 0x38u),
                o3 = float_at(rt, out + 0x3Cu);
    const float x = ((psp_mul(m0, c0) + psp_mul(m4, c1)) + psp_mul(m8, c2)) + o0;
    const float y = ((psp_mul(m1, c0) + psp_mul(m5, c1)) + psp_mul(m9, c2)) + o1;
    const float z = ((psp_mul(m2, c0) + psp_mul(m6, c1)) + psp_mul(m10, c2)) + o2;
    const float w = ((psp_mul(m3, c0) + psp_mul(m7, c1)) + psp_mul(m11, c2)) + o3;
    put_float(rt, out + 0x30u, x);
    put_float(rt, out + 0x3Cu, w);
    put_float(rt, out + 0x34u, y);
    put_float(rt, out + 0x38u, z);
}

// 0x08B2A230: 0x08B2A0EC(sp + 0, sp + 4, sp + 8) fills three floats; each x 255.0f is truncated and the bytes pack as
// (c0 & 0xFF) | ((c1 & 0xFF) << 8) | ((c2 & 0xFF) << 16) | 0xFF000000.
void func_08B2A230(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    (void)call(rt, ctx, 0x08B2A0ECu, {frame.at(0u), frame.at(4u), frame.at(8u)});
    const u32 c0 = ctx.fpu_float_to_word(psp_mul(float_at(rt, frame.at(0u)), 255.0f), 1u);
    const u32 c1 = ctx.fpu_float_to_word(psp_mul(float_at(rt, frame.at(4u)), 255.0f), 1u);
    const u32 c2 = ctx.fpu_float_to_word(psp_mul(float_at(rt, frame.at(8u)), 255.0f), 1u);
    ctx.gpr[2] = (c0 & 0xFFu) | ((c1 & 0xFFu) << 8) | ((c2 & 0xFFu) << 16) | 0xFF000000u;
}

// 0x08B193CC (a0 = object): 0x08B1935C(a0) answers the second argument of the tail 0x08B18C78(a0, answer).

// 0x08B4E878 (matrix = a0, vector = a1, out = a2): 0x08B4E734(a0, a1, a2) first; then each row of the matrix dotted with
// the vector (rows at +0, +0xC, +0x18, +0x24 against the vector's +0x30..+0x38) is added to out's +0x30..+0x3C.

// 0x08A90C48 (self = a0, source = a1): 0x08A782D8(self, source) then vtable 0x08CB6C98 at self+0 and +0x14 = 0. The two
// arrays source+0x18 {begin, end} and source+0x24 {begin, end} are copied into self+0x18 / self+0x24 as
// {buffer, used, buffer + bytes} with bytes = (end - begin) & ~3: 0x08A5BC20(bytes, 4) allocates and 0x08C2AED8(begin,
// end, buffer, &flag) decompresses (the flag words are the two bytes of the frame). source+0x30 is copied to self+0x30
// by 0x08AB5004.
void func_08A90C48(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4], source = ctx.gpr[5];
    (void)call(rt, ctx, 0x08A782D8u, {self, source});
    put(rt, self, 0x08CB6C98u);
    put(rt, self + 0x14u, 0u);
    const u32 first_begin = word(rt, source + 0x18u), first_end = word(rt, source + 0x1Cu);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x20u, 0u);
    const u32 first_bytes = (first_end - first_begin) & ~3u;
    const u32 first_buffer = call(rt, ctx, 0x08A5BC20u, {first_bytes, 4u});
    put(rt, self + 0x18u, first_buffer);
    put(rt, self + 0x1Cu, first_buffer);
    put(rt, self + 0x20u, first_buffer + first_bytes);
    const u32 first_used = call(rt, ctx, 0x08C2AED8u, {first_begin, first_end, first_buffer, frame.at(1u)});
    put(rt, self + 0x1Cu, first_used);
    const u32 second_begin = word(rt, source + 0x24u), second_end = word(rt, source + 0x28u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    const u32 second_bytes = (second_end - second_begin) & ~3u;
    const u32 second_buffer = call(rt, ctx, 0x08A5BC20u, {second_bytes, 4u});
    put(rt, self + 0x24u, second_buffer);
    put(rt, self + 0x28u, second_buffer);
    put(rt, self + 0x2Cu, second_buffer + second_bytes);
    const u32 second_used = call(rt, ctx, 0x08C2AED8u, {second_begin, second_end, second_buffer, frame.at(0u)});
    put(rt, self + 0x28u, second_used);
    (void)call(rt, ctx, 0x08AB5004u, {self + 0x30u, source + 0x30u});
}

// 0x08B11AC0 (self = a0, name = a1, arg2 = a2, arg3 = a3): 0x08B11830(self, name, arg2, arg3), then 0x08B0D800(self +
// 0xA0) and the two vtables 0x08CD7D60 / 0x08CD7E40; arg2 goes to +0x1AC, arg3 to +0x10, -1 to +0x1B4 / +0x238 / +0x25C /
// +0x260, the 0x1B0..0x27C block is cleared with 0x08B28DCC / 0x08B2BD24 on self+0x1B8, +0x1E0, +0x280, +0x2D0 and +0x4D4
// are cleared, 0x08B2EDD4(self+0x4D8, 0x08CB4B44), name is copied to +0xAC (0x80 bytes) by 0x08B7C830, +0x12C is zeroed
// (0x80 bytes) and the tail zeroes +0x2D4 (0x200 bytes).
void func_08B11AC0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4], name = ctx.gpr[5], arg2 = ctx.gpr[6], arg3 = ctx.gpr[7];
    (void)call(rt, ctx, 0x08B11830u, {self, name, arg2, arg3});
    (void)call(rt, ctx, 0x08B0D800u, {self + 0xA0u});
    put(rt, self, 0x08CD7D60u);
    put(rt, self + 0xA0u, 0x08CD7E40u);
    put(rt, self + 0x1ACu, arg2);
    put(rt, self + 0x1B4u, 0xFFFFFFFFu);
    put(rt, self + 0x1B0u, 0u);
    (void)call(rt, ctx, 0x08B28DCCu, {self + 0x1B8u});
    (void)call(rt, ctx, 0x08B28DCCu, {self + 0x1E0u});
    put(rt, self + 0x260u, 0xFFFFFFFFu);
    put(rt, self + 0x238u, 0xFFFFFFFFu);
    put(rt, self + 0x25Cu, 0xFFFFFFFFu);
    put(rt, self + 0x230u, 0u);
    put(rt, self + 0x234u, 0u);
    put(rt, self + 0x23Cu, 0u);
    put(rt, self + 0x240u, 0u);
    put(rt, self + 0x244u, 0u);
    put(rt, self + 0x248u, 0u);
    put(rt, self + 0x24Cu, 0u);
    put(rt, self + 0x250u, 0u);
    put(rt, self + 0x254u, 0u);
    put8(rt, self + 0x258u, 0u);
    put(rt, self + 0x264u, 0u);
    put(rt, self + 0x268u, 0u);
    put(rt, self + 0x26Cu, 0u);
    put(rt, self + 0x270u, 0u);
    put(rt, self + 0x274u, 0u);
    put(rt, self + 0x278u, 0u);
    put8(rt, self + 0x27Cu, 0u);  // the delay slot of the jal
    (void)call(rt, ctx, 0x08B2BD24u, {self + 0x280u});
    put(rt, self + 0x2D0u, 0u);
    put(rt, self + 0x4D4u, 0u);  // the delay slot of the jal
    (void)call(rt, ctx, 0x08B2EDD4u, {self + 0x4D8u, 0x08CB4B44u});
    put(rt, self + 0x10u, arg3);
    (void)call(rt, ctx, 0x08B7C830u, {self + 0xACu, name, 0x80u});
    (void)call(rt, ctx, 0x08B36018u, {self + 0x12Cu, 0u, 0x80u});
    tail_call(ctx, 0x08B36018u, {self + 0x2D4u, 0u, 0x200u});
}

// 0x08ACEBC4 (constructor): vtable 0x08CD6B28 at a0+0; 0x08B2EE10(a0+4, 2, 0x300), 0x08B2EE10(a0+0x18, 2, 0x303),
// 0x08B2EDD4(a0+0x2C, 0x08CB49D4), 0x08B0CC04(a0+0x40), +0x160 / +0x162 = 0, 0x08B28DCC(a0+0x164), +0x244 = a0+0x240,
// the 0x300..0x330 words cleared, the 0x334..0x4A0 objects built (0x08B28D34 / 0x08B28DCC), +0x510 = 4, +0x514 = -1,
// +0x518 = 0, 0x08B2EEB8(a0+4 / a0+0x18 / a0+0x2C), 0x08C7AD70() and +0x240 = 0; v0 = the last callee's.

// 0x08B45054 (constructor): 0x08B08840(a0), vtable 0x08CDA9A0 at a0+0, +0xC0 = -1, the 0xAD..0xC8 bytes/words cleared,
// 0x08B2C350(a0+0xCC), 0x08B28DCC(a0+0x12C), 0x08B3EB80(a0+0x154), a0+0x454 / a0+0x654 zeroed (0x200 bytes each),
// a0+0x854 (0x80 bytes), block+0x480 = 0x80, block+0x484 = 0, 0x08B3F75C(a0+0x8DC, a0+0xCC), the 0x910..0x98C words cleared,
// 0x08B3FA98(a0+0xBB8), the globals 0x08D69500 / 0x08D694FC zeroed and the tail 0x08B3FF98(a0).
void func_08B45054(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 block = self + 0x454u;  // s1: the 0x480 / 0x484 words below are offsets of it
    (void)call(rt, ctx, 0x08B08840u, {self});
    put(rt, self, 0x08CDA9A0u);
    put(rt, self + 0xC0u, 0xFFFFFFFFu);
    put8(rt, self + 0xADu, 0u);
    put(rt, self + 0xB0u, 0u);
    put16(rt, self + 0xB4u, 0u);
    put8(rt, self + 0xB6u, 0u);
    put8(rt, self + 0xB7u, 0u);
    put8(rt, self + 0xB8u, 0u);
    put8(rt, self + 0xB9u, 0u);
    put8(rt, self + 0xBAu, 0u);
    put(rt, self + 0xBCu, 0u);
    put(rt, self + 0xC4u, 0u);
    put(rt, self + 0xC8u, 0u);  // the delay slot of the jal
    (void)call(rt, ctx, 0x08B2C350u, {self + 0xCCu});
    (void)call(rt, ctx, 0x08B28DCCu, {self + 0x12Cu});
    (void)call(rt, ctx, 0x08B3EB80u, {self + 0x154u});
    (void)call(rt, ctx, 0x08B36018u, {self + 0x454u, 0u, 0x200u});
    (void)call(rt, ctx, 0x08B36018u, {self + 0x654u, 0u, 0x200u});
    (void)call(rt, ctx, 0x08B36018u, {self + 0x854u, 0u, 0x80u});
    put(rt, block + 0x480u, 0x80u);
    put(rt, block + 0x484u, 0u);  // the delay slot of the jal
    (void)call(rt, ctx, 0x08B3F75Cu, {self + 0x8DCu, self + 0xCCu});
    put(rt, self + 0x910u, 0u);
    put(rt, self + 0x914u, 0u);
    put(rt, self + 0x93Cu, 0xFFFFFFFFu);
    put(rt, self + 0x918u, 0u);
    put(rt, self + 0x91Cu, 0u);
    put(rt, self + 0x920u, 0u);
    put(rt, self + 0x924u, 0u);
    put(rt, self + 0x928u, 0u);
    put(rt, self + 0x92Cu, 0u);
    put(rt, self + 0x930u, 0u);
    put16(rt, self + 0x934u, 0u);
    put8(rt, self + 0x936u, 0u);
    put16(rt, self + 0x938u, 0u);
    put(rt, self + 0x940u, 0u);
    put(rt, self + 0x944u, 0u);
    put(rt, self + 0x988u, 0u);
    put(rt, self + 0x98Cu, 0u);  // the delay slot of the jal
    (void)call(rt, ctx, 0x08B3FA98u, {self + 0xBB8u});
    put(rt, 0x08D69500u, 0u);
    put(rt, 0x08D694FCu, 0u);
    tail_call(ctx, 0x08B3FF98u, {self});
}
} // namespace
} // namespace new_batch_main_1127
} // namespace gt::game

#define NEW1127(address, name) GT_DECOMP(address, gt::game::new_batch_main_1127::name, gt::ReturnKind::Word)
NEW1127(0x08AC7288u, func_08AC7288);
NEW1127(0x08A59CECu, func_08A59CEC);
NEW1127(0x08A5A1B4u, func_08A5A1B4);
NEW1127(0x08AFC484u, func_08AFC484);
NEW1127(0x08A59358u, func_08A59358);
NEW1127(0x08A7B1A8u, func_08A7B1A8);
NEW1127(0x08B4E780u, func_08B4E780);
NEW1127(0x08B2A230u, func_08B2A230);
NEW1127(0x08A90C48u, func_08A90C48);
NEW1127(0x08AB4DC0u, func_08AB4DC0);
NEW1127(0x08B11AC0u, func_08B11AC0);
NEW1127(0x08B45054u, func_08B45054);
