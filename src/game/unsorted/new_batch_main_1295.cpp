// New batch main-1295 (draft, unverified): thirteen functions re-drafted from their earlier mismatched drafts, structured C
// from tools.disasm. Each one fixes the register or memory word the gate reported: 0x08A7593C / 0x08A84890 / 0x08A95050
// stored the fill's answer on the stack where the asm stores the block, 0x08B1A4F4 conflated the allocated block with the
// object built in it, 0x08AD2F20 left v0 at the last call instead of the address it computes, 0x08ABCEF0 answered the
// store's address instead of the `lui` base it holds at exit, 0x08ADDB6C / 0x08AE2580 answered the leave hook's value
// instead of the state call's, 0x08B4E878 read the matrix/vector/output from the wrong arguments, and the four
// 0x08AE1xxx writers pass the collector's key (and the leave call's a1) as the asm does.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1295 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A call with float arguments ($f12..) and a float answer; v0 / v1 / f0 come back as the callee left them.
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// The `lui` half of an address whose low half is added as a signed 16-bit immediate: the value the register holds at exit.
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// 0x08A7593C / 0x08A84890 / 0x08A95050 (a0 = self, a1..a3 to the fill): a block of `bytes` is allocated, `fill` gets
// (block, a1, a2, a3), the block (not the fill's answer) is stored on the frame and `last` gets (self, &block); v0 is the
// last call's answer.
void alloc_fill_last(Runtime &rt, AllegrexContext &ctx, u32 bytes, u32 fill, u32 last) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    (void)call(rt, ctx, fill, {block, a1, a2, a3});
    put(rt, ctx.gpr[29], block);
    (void)call(rt, ctx, last, {self, ctx.gpr[29]});
}

// 0x08AE11A0 / 0x08AE1338 / 0x08AE160C / 0x08AE1794 (a0 = object, f12 = value, f13 = second float): enter
// (0x08AEB808 with a0 = the object), a stack scratch of 4 x word(0x08D4069C) bytes that `collect` fills with
// (scratch, key, 0), leave (0x08AEB82C with a1 = `leave_a1`), then the value clamped to [low, high], scaled and truncated
// to the integer 0x08AE0F64(scratch, count, integer, f12 = the second float) takes; v0 is that last call's answer.
void scaled_parameter(Runtime &rt, AllegrexContext &ctx, u32 collect, bool object_is_key, bool leave_a1_is_object, u32 high_at,
                      u32 low_at, u32 scale_at) {
    const u32 self = ctx.gpr[4];
    const float value_in = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x30u);
    (void)call_float(rt, ctx, 0x08AEB808u, {self}, {value_in});
    const u32 count = word(rt, 0x08D4069Cu);
    GuestFrame scratch(ctx, (count * 4u + 0xFu) & ~0xFu);
    const u32 collected = call(rt, ctx, collect, {scratch.at(0u), object_is_key ? self : 0u, 0u});
    (void)call(rt, ctx, 0x08AEB82Cu, {scratch.at(0u), leave_a1_is_object ? self : 0u});
    const float high = float_at(rt, high_at);
    float value = high;
    if (value_in < high) {
        const float low = float_at(rt, low_at);
        value = value_in < low ? low : value_in;
    }
    const u32 integer = ctx.fpu_float_to_word(psp_mul(value, float_at(rt, scale_at)), 1u);
    (void)call_float(rt, ctx, 0x08AE0F64u, {scratch.at(0u), collected, integer}, {second});
}

// 0x08ADDB6C / 0x08AE2580: enter (0x08AEB808), the state reset 0x08AD8C5C(state, 0), leave (0x08AEB82C); v0 is the reset's
// answer, not the leave's.
void reset_then_leave(Runtime &rt, AllegrexContext &ctx, u32 state) {
    GuestFrame frame(ctx, 0x10u);
    (void)call(rt, ctx, 0x08AEB808u);
    const u32 result = call(rt, ctx, 0x08AD8C5Cu, {state, 0u});
    (void)call(rt, ctx, 0x08AEB82Cu, {state, 0u});
    ctx.gpr[2] = result;
}

void func_08A7593C(Runtime &rt, AllegrexContext &ctx) { alloc_fill_last(rt, ctx, 0x1Cu, 0x08A75738u, 0x08A982BCu); }
void func_08A84890(Runtime &rt, AllegrexContext &ctx) { alloc_fill_last(rt, ctx, 0x20u, 0x08A845C8u, 0x08A5D190u); }
void func_08A95050(Runtime &rt, AllegrexContext &ctx) { alloc_fill_last(rt, ctx, 0x24u, 0x08A94564u, 0x08A76AB4u); }
void func_08ADDB6C(Runtime &rt, AllegrexContext &ctx) { reset_then_leave(rt, ctx, 0x08D40644u); }
void func_08AE2580(Runtime &rt, AllegrexContext &ctx) { reset_then_leave(rt, ctx, 0x08D40688u); }


// 0x08B1A4F4 (a0 = self): a 0x30-byte block is allocated and 0x08B28D34 builds in it (its answer is dropped), the block's
// bytes +0x2C / +0x2D are set and *self takes the block; a 0x10-byte block is then filled (vtable 0x08CD9310, two 1s and the
// first block) into *(self+4); the tail 0x08B8E380 gets (self+4, block, block).
void func_08B1A4F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 block;
    {
        GuestFrame frame(ctx, 0x10u);
        block = call(rt, ctx, 0x08B62344u, {0x30u});
        (void)call(rt, ctx, 0x08B28D34u, {block});
        put_byte(rt, block + 0x2Cu, 1u);
        put_byte(rt, block + 0x2Du, 0u);
        put(rt, self, block);
        const u32 second = call(rt, ctx, 0x08B62344u, {0x10u});
        put(rt, second, 0x08CD9310u);
        put(rt, second + 4u, 1u);
        put(rt, second + 8u, 1u);
        put(rt, second + 0xCu, block);
        put(rt, self + 4u, second);
    }
    tail_call(ctx, 0x08B8E380u, {self + 4u, block, block});
}

// 0x08AD2F20 (a0 = self): sixteen-bit fields -1 at +4, +6, +8, +0xC, +0xE and 0 at +0xA, the vtable at +0, two
// 0x08AD5EBC sub-constructors at +0x14 and +0x44, 1.0f at +0x50 and 0x4C088344 at +0x24, zeros to +0x78; v0 = self+0x2C.
void func_08AD2F20(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put_half(rt, self + 0xEu, 0xFFFFu);
    put_half(rt, self + 0x6u, 0xFFFFu);
    put_half(rt, self + 0x8u, 0xFFFFu);
    put_half(rt, self + 0xAu, 0u);
    put_half(rt, self + 0xCu, 0xFFFFu);
    put_half(rt, self + 0x4u, 0xFFFFu);
    put(rt, self, 0x08CD6C70u);
    (void)call(rt, ctx, 0x08AD5EBCu, {self + 0x14u});
    put(rt, self + 0x1Cu, 0u);
    (void)call(rt, ctx, 0x08AD5EBCu, {self + 0x44u});
    put_float(rt, self + 0x50u, 1.0f);
    put(rt, self + 0x4Cu, 0u);
    put(rt, self + 0x54u, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0x4C088344u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x78u, 0u);
    ctx.gpr[2] = self + 0x2Cu;
}

// 0x08ABCEF0 (a0 = self): base 0x08ABD2E4, vtable 0x08CD62B8 at +0, a zero byte at +0x84 and the global at 0x08CFFAAC takes
// the object; v0 is the `lui` base of that global store, which is what the register holds at exit.
void func_08ABCEF0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call(rt, ctx, 0x08ABD2E4u, {self});
    put(rt, self, 0x08CD62B8u);
    put_byte(rt, self + 0x84u, 0u);
    put(rt, 0x08CFFAACu, self);
    ctx.gpr[2] = high_half(0x08CFFAACu);
}

// 0x08B4E878 (a0 = out, a1 = matrix, a2 = vector): 0x08B4E734(out, matrix, vector), then each of out's +0x30..+0x3C is
// increased by the matching matrix row (+0 / +0x10 / +0x20, +4 / +0x14 / +0x24, ...) dotted with the vector's +0x30..+0x38.
void func_08B4E878(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 out = ctx.gpr[4], matrix = ctx.gpr[5], vector = ctx.gpr[6];
    (void)call(rt, ctx, 0x08B4E734u, {out, matrix, vector});
    const float o0 = float_at(rt, out + 0x30u), o1 = float_at(rt, out + 0x34u);
    const float o2 = float_at(rt, out + 0x38u), o3 = float_at(rt, out + 0x3Cu);
    const float first0 = float_at(rt, vector + 0x30u), first1 = float_at(rt, vector + 0x34u);
    const float first2 = float_at(rt, vector + 0x38u);
    put_float(rt, out + 0x30u, o0 + ((psp_mul(float_at(rt, matrix), first0) + psp_mul(float_at(rt, matrix + 0x10u), first1)) +
                                    psp_mul(float_at(rt, matrix + 0x20u), first2)));
    const float second0 = float_at(rt, vector + 0x30u), second1 = float_at(rt, vector + 0x34u);
    const float second2 = float_at(rt, vector + 0x38u);
    put_float(rt, out + 0x34u, o1 + ((psp_mul(float_at(rt, matrix + 4u), second0) + psp_mul(float_at(rt, matrix + 0x14u), second1)) +
                                     psp_mul(float_at(rt, matrix + 0x24u), second2)));
    const float third0 = float_at(rt, vector + 0x30u), third1 = float_at(rt, vector + 0x34u);
    const float third2 = float_at(rt, vector + 0x38u);
    put_float(rt, out + 0x38u, o2 + ((psp_mul(float_at(rt, matrix + 8u), third0) + psp_mul(float_at(rt, matrix + 0x18u), third1)) +
                                     psp_mul(float_at(rt, matrix + 0x28u), third2)));
    const float fourth0 = float_at(rt, vector + 0x30u), fourth1 = float_at(rt, vector + 0x34u);
    const float fourth2 = float_at(rt, vector + 0x38u);
    put_float(rt, out + 0x3Cu, o3 + ((psp_mul(float_at(rt, matrix + 0xCu), fourth0) + psp_mul(float_at(rt, matrix + 0x1Cu), fourth1)) +
                                     psp_mul(float_at(rt, matrix + 0x2Cu), fourth2)));
}

} // namespace
} // namespace new_batch_main_1295
} // namespace gt::game

#define NEW1295(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1295::name, gt::ReturnKind::kind)
NEW1295(0x08A7593Cu, func_08A7593C, Word);
NEW1295(0x08A84890u, func_08A84890, Word);
NEW1295(0x08A95050u, func_08A95050, Word);
NEW1295(0x08ABCEF0u, func_08ABCEF0, Word);
NEW1295(0x08AD2F20u, func_08AD2F20, Word);
NEW1295(0x08ADDB6Cu, func_08ADDB6C, Word);
NEW1295(0x08AE2580u, func_08AE2580, Word);
NEW1295(0x08B1A4F4u, func_08B1A4F4, Word);
NEW1295(0x08B4E878u, func_08B4E878, Word);
