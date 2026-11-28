// New batch main-1620 (main-session batch 1620, unverified draft). Structured C from tools.disasm; every function
// here was read against its asm. Redrafts of the gate's mismatches had no gate detail in drafts/fix_info.txt, so each
// one was re-derived from the asm (v0 on exit, delay slots before the call, argument registers the callee reads).
// Before registering: run the audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1620 {
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
void put8(Runtime &rt, u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); }
void put16(Runtime &rt, u32 a, u32 v) { rt.memory().store16(a, static_cast<std::uint16_t>(v)); }

// Restores s0-s7 on exit, as the asm's epilogue does for the registers it saved.
struct SavedS {
    AllegrexContext &ctx;
    u32 s[8];
    explicit SavedS(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 8; ++i) s[i] = c.gpr[16 + i];
    }
    ~SavedS() {
        for (unsigned i = 0; i < 8; ++i) ctx.gpr[16 + i] = s[i];
    }
};

// 0x0891E198 (a0 = object): the vtable word, the zeroed fields, a float 46.0 at +0x210 and -1 at +0x224; v0 = -1.
void func_0891E198(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08926D60u, {self});
    put(rt, self, 0x08CC5D70u);
    put(rt, self + 0x1F8u, 0u);
    put(rt, self + 0x200u, 0u);
    put(rt, self + 0x1FCu, 0u);
    put(rt, self + 0x204u, 0u);
    put(rt, self + 0x20Cu, 0u);
    put(rt, self + 0x208u, 0u);
    put_float(rt, self + 0x210u, 46.0f);
    put(rt, self + 0x224u, 0xFFFFFFFFu);
    put(rt, self + 0x230u, 0u);
    put(rt, self + 0x214u, 0u);
    put(rt, self + 0x218u, 0u);
    put(rt, self + 0x21Cu, 0u);
    put(rt, self + 0x220u, 0u);
    put(rt, self + 0x228u, 0u);
    put8(rt, self + 0x22Cu, 0u);
    put(rt, self + 0x234u, 0u);
    put(rt, self + 0x238u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08932B74 (a0 = object with +0x40 / +0x44; a1..t0 = output words): calls, then the four bytes the second callee
// wrote to the frame are stored out; v0 = the signed byte at sp+0x2A.
void func_08932B74(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out1 = ctx.gpr[5];
    const u32 out2 = ctx.gpr[6];
    const u32 out3 = ctx.gpr[7];
    const u32 out4 = ctx.gpr[8];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = self;
    ctx.gpr[17] = out1;
    ctx.gpr[18] = out2;
    ctx.gpr[19] = out3;
    ctx.gpr[20] = out4;
    const u32 first = call(rt, ctx, 0x089363FCu, {self});
    call(rt, ctx, 0x08936584u, {first, ctx.gpr[5], word(rt, self + 0x40u), word(rt, self + 0x44u), sp});
    const u32 b_unsigned_2c = rt.memory().load8(sp + 0x2Cu);
    const u32 b_signed_2a = static_cast<u32>(signed_byte(rt, sp + 0x2Au));
    const u32 b_signed_2b = static_cast<u32>(signed_byte(rt, sp + 0x2Bu));
    const u32 b_unsigned_24 = rt.memory().load8(sp + 0x24u);
    put(rt, out1, b_unsigned_2c);
    put(rt, out2, b_signed_2a);
    put(rt, out3, b_signed_2b);
    put(rt, out4, b_unsigned_24);
    ctx.gpr[2] = b_signed_2a;
}

// 0x0897D930 (a0 = object, a1 = source of 9 words): the vtable, a call, then the source words copied to +0x14..+0x34.
void func_0897D930(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = src;
    ctx.gpr[17] = self;
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CB8E48u);
    const u32 dst = self + 0x14u;
    put(rt, dst + 0x00u, word(rt, src + 0x00u));
    put(rt, dst + 0x0Cu, word(rt, src + 0x0Cu));
    put(rt, dst + 0x18u, word(rt, src + 0x18u));
    put(rt, dst + 0x04u, word(rt, src + 0x04u));
    put(rt, dst + 0x10u, word(rt, src + 0x10u));
    put(rt, dst + 0x1Cu, word(rt, src + 0x1Cu));
    put(rt, dst + 0x08u, word(rt, src + 0x08u));
    put(rt, dst + 0x14u, word(rt, src + 0x14u));
    put(rt, dst + 0x20u, word(rt, src + 0x20u));
    ctx.gpr[2] = 0x08CB8E48u;
}

// 0x08A5102C (a0 = object): clears the header and the flag bytes, stores 0x400 at +0x1C, then calls the member at +0x30;
// v0 = that call's result.
void func_08A5102C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 v1 = self + 0x14u;
    put(rt, self + 0x00u, 0u);
    put(rt, self + 0x04u, 0u);
    put8(rt, self + 0x08u, 0u);
    put8(rt, self + 0x09u, 0u);
    put8(rt, self + 0x0Au, 0u);
    put8(rt, self + 0x0Bu, 0u);
    put(rt, self + 0x0Cu, 0u);
    put(rt, self + 0x10u, 0u);
    put8(rt, self + 0x14u, 0u);
    put16(rt, v1 + 0x06u, 0u);
    put8(rt, v1 + 0x01u, 0u);
    put8(rt, v1 + 0x02u, 0u);
    put8(rt, v1 + 0x03u, 0u);
    put16(rt, v1 + 0x04u, 0u);
    put16(rt, self + 0x1Cu, 0x400u);
    put16(rt, self + 0x1Eu, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    call(rt, ctx, 0x08B28D34u, {self + 0x30u});
    put8(rt, self + 0x5Du, 0u);
    put8(rt, self + 0x5Cu, 0u);
}

// 0x08808CC8 (a0 = arg0, a1, a2 = passed on): allocates 0x1C bytes, sets the vtable and three members, then calls
// the last member's callee with arg0 and the new object's pointer in a 4-byte frame slot; v0 = that result.

// 0x089FA1F4 (a0 = object, a1 = pair of words {begin, end}): allocates end-begin rounded down to 32 bytes, stores the
// begin/end/size pointers, then calls with the pair's first two words and the block; v0 = that result.

// 0x089FA30C (a0 = object, a1 = pair): as 0x089FA1F4, but the last word is the pair's word at +0xC and v0 = that word.

// 0x08AC2168 (a0 = out slot, a1 = argument): allocates a 0x44-byte object, stores it in *a0, builds a second object
// (vtable 0x08CD65D8) and stores it at +4 of a0 before the final call; v0 = a0.
void func_08AC2168(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = out;
    ctx.gpr[18] = arg;
    const u32 block = call(rt, ctx, 0x08B62344u, {0x44u});
    ctx.gpr[16] = block;
    call(rt, ctx, 0x08AC1EACu, {block, arg});
    put(rt, out, block);
    const u32 block2 = call(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, block2 + 0x4u, 1u);
    put(rt, block2 + 0xCu, block);
    put(rt, block2 + 0x0u, 0x08CD65D8u);
    put(rt, block2 + 0x8u, 1u);
    put(rt, out + 0x4u, block2);
    call(rt, ctx, 0x08B8E380u, {out + 0x4u, block, block});
    ctx.gpr[2] = out;
}

// 0x0884CC98 (a0 = arg0, a1..t1 = args): allocates 0x28 bytes, calls the builder with the block and the args, stores
// its result in a frame slot and passes it to the final callee with arg0; v0 = that callee's result.

// The two builders below differ only in their callees: allocate 0x2C bytes, call the builder with the block and the
// pointer/word arguments, then store its result in a frame slot and pass it to the final callee with a0.
void build_and_store(Runtime &rt, AllegrexContext &ctx, u32 builder, u32 final_callee) {
    const u32 arg0 = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = arg1;
    ctx.gpr[17] = a2;
    ctx.gpr[18] = a3;
    ctx.gpr[19] = t0;
    ctx.gpr[20] = arg0;
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    const u32 built = call(rt, ctx, builder, {block, arg1, a2, a3, t0});
    put(rt, sp, built);
    call(rt, ctx, final_callee, {arg0, sp});
}

// 0x0899C7F4 and 0x0899CA0C: the same shape, calling 0x0899C6E0 / 0x0899C900 as the builder.

// 0x089C3B74 (a0 = out, a1, a2 = args, f12 / f13 = floats): allocates 0x2C bytes, calls the builder with the block,
// a1, a2 and the two floats, stores its result for the final callee (a0 = out, a1 = frame slot); f20 / f21 kept.

// 0x0892145C (a0 = object, a2 = word, a3 = flag byte): calls the base constructor, then the vtable, the zeroed fields,
// the flag bytes and two floats (5.0 at +0x2C, 50.0 at +0x30); v0 = the vtable.
void func_0892145C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 word_arg = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = word_arg;
    ctx.gpr[18] = flag;
    call(rt, ctx, 0x08925FE4u, {self});
    put(rt, self, 0x08CC6000u);
    put(rt, self + 0x08u, word_arg);
    put(rt, self + 0x0Cu, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x24u, 0u);
    put8(rt, self + 0x35u, flag);
    put_float(rt, self + 0x2Cu, 5.0f);
    put_float(rt, self + 0x30u, 50.0f);
    put8(rt, self + 0x34u, 0u);
    ctx.gpr[2] = 0x08CC6000u;
}

// 0x089CBA20 (a0 = object): the base constructor, then the vtable, four sub-objects at +0x12C / +0x138 / +0x144 / +0x150
// with their vtable-less headers zeroed; v0 = 2.
void func_089CBA20(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08A0D658u, {self});
    put(rt, self, 0x08CC9FD8u);
    put(rt, self + 0x38u, 0x08CCA230u);
    put(rt, self + 0x120u, 2u);
    put(rt, self + 0x124u, 0u);
    put(rt, self + 0x128u, 0u);
    put(rt, self + 0x12Cu, 0u);
    put(rt, self + 0x134u, 0u);
    put(rt, self + 0x130u, 0u);
    put(rt, self + 0x140u, 0u);
    put(rt, self + 0x13Cu, 0u);
    put(rt, self + 0x138u, 0u);
    put(rt, self + 0x144u, 0u);
    put(rt, self + 0x14Cu, 0u);
    put(rt, self + 0x148u, 0u);
    put(rt, self + 0x158u, 0u);
    put(rt, self + 0x154u, 0u);
    put(rt, self + 0x150u, 0u);
    put(rt, self + 0x160u, 0u);
    put(rt, self + 0x15Cu, 0u);
    ctx.gpr[2] = 2u;
}

// 0x089FA008 (a0 = object): zeroes three words, allocates 0x20 bytes (0x10 items), stores the block, its end and
// four 1.0 floats in a frame area, calls the initialiser, then sets +4 to the end and +0xC to -1; v0 = the end.
void func_089FA008(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = self;
    put(rt, self + 0x0u, 0u);
    put(rt, self + 0x4u, 0u);
    put(rt, self + 0x8u, 0u);
    const u32 block = call(rt, ctx, 0x08B35840u, {0x10u, 0x20u});
    const u32 end = block + 0x20u;
    put(rt, self + 0x0u, block);
    put(rt, self + 0x4u, block);
    put(rt, self + 0x8u, end);
    put(rt, sp + 0x20u, 0xFFFFFFFFu);
    put_float(rt, sp + 0x10u, 1.0f);
    put_float(rt, sp + 0x1Cu, 1.0f);
    put_float(rt, sp + 0x18u, 1.0f);
    put_float(rt, sp + 0x14u, 1.0f);
    call(rt, ctx, 0x08BE6028u, {block, 1u, sp + 0x10u, sp});
    put(rt, self + 0xCu, 0xFFFFFFFFu);
    const u32 new_end = word(rt, self) + 0x20u;
    put(rt, self + 0x4u, new_end);
    ctx.gpr[2] = new_end;
}

// 0x08A7DC44 (a0 = object with a virtual slot 0x118, a1, a2 = args): two out-parameters (sp, sp+4), the object's
// virtual call, then the result word is tested against zero; v0 = (word != 0).
void func_08A7DC44(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0);
    const u32 out = sp + 0x4u;
    ctx.gpr[16] = self;
    ctx.gpr[17] = out;
    ctx.gpr[18] = a1;
    ctx.gpr[19] = a2;
    call(rt, ctx, 0x08A5D438u, {out, a1});
    call(rt, ctx, 0x08A7D710u, {sp, self, out, a1});
    call(rt, ctx, 0x08A5B9ECu, {sp});
    const u32 vtable = word(rt, self);
    call_indirect(rt, ctx, word(rt, vtable + 0x118u), {self, out, a2});
    const u32 flag = word(rt, sp + 0x4u);
    call(rt, ctx, 0x08A5B9ECu, {out});
    ctx.gpr[2] = flag != 0u ? 1u : 0u;
}

// 0x08B56FD8 (a0 = a 4x4 float matrix, with the rows at +0, +4 ... +0x2C): copies twelve of its floats into a frame
// block with the constant at 0x08CB61C0 and three zeroed words, then calls 0x08B566BC with that block and a1..a3.
void func_08B56FD8(Runtime &rt, AllegrexContext &ctx) {
    const u32 mat = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = frame.at(0);
    const float m00 = float_at(rt, mat + 0x00u);
    const float m2c = float_at(rt, mat + 0x2Cu);
    const float m10 = float_at(rt, mat + 0x10u);
    const float m20 = float_at(rt, mat + 0x20u);
    const float m04 = float_at(rt, mat + 0x04u);
    const float m14 = float_at(rt, mat + 0x14u);
    const float m24 = float_at(rt, mat + 0x24u);
    const float m08 = float_at(rt, mat + 0x08u);
    const float m18 = float_at(rt, mat + 0x18u);
    const float m28 = float_at(rt, mat + 0x28u);
    const float m0c = float_at(rt, mat + 0x0Cu);
    const float m1c = float_at(rt, mat + 0x1Cu);
    const float constant = float_at(rt, 0x08CB61C0u);
    put_float(rt, sp + 0x00u, m00);
    put_float(rt, sp + 0x04u, m10);
    put_float(rt, sp + 0x08u, m20);
    put(rt, sp + 0x0Cu, 0u);
    put_float(rt, sp + 0x10u, m04);
    put_float(rt, sp + 0x14u, m14);
    put_float(rt, sp + 0x18u, m24);
    put(rt, sp + 0x1Cu, 0u);
    put_float(rt, sp + 0x20u, m08);
    put_float(rt, sp + 0x24u, m18);
    put_float(rt, sp + 0x28u, m28);
    put(rt, sp + 0x2Cu, 0u);
    put_float(rt, sp + 0x30u, m0c);
    put_float(rt, sp + 0x34u, m1c);
    put_float(rt, sp + 0x38u, m2c);
    put_float(rt, sp + 0x3Cu, constant);
    call(rt, ctx, 0x08B566BCu, {sp, a1, a2, a3});
}

// 0x08896970 (a0 = object, a1 / a2 = words): the base constructor, the vtable, and two embedded members, each
// constructed by 0x08895D88; v0 = 1.
void func_08896970(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = a1;
    ctx.gpr[18] = a2;
    call(rt, ctx, 0x08898EE8u, {self});
    put(rt, self, 0x08CC1F60u);
    put(rt, self + 0x68u, 0xFFFFFFFFu);
    put(rt, self + 0x58u, a1);
    put(rt, self + 0x5Cu, a2);
    put(rt, self + 0x60u, 0u);
    put(rt, self + 0x6Cu, 0u);
    call(rt, ctx, 0x08895D88u, {self + 0x70u});
    put8(rt, self + 0xA8u, 0u);
    put8(rt, self + 0xA9u, 0u);
    put(rt, self + 0xACu, 0u);
    put(rt, self + 0xB0u, 0u);
    call(rt, ctx, 0x08895D88u, {self + 0xB4u});
    put8(rt, self + 0xF8u, 1u);
    put(rt, self + 0xFCu, 0u);
    put(rt, self + 0xECu, 0u);
    put8(rt, self + 0xF0u, 0u);
    put(rt, self + 0xF4u, 0u);
    ctx.gpr[2] = 1u;
}

// 0x088F78CC (f12 = x, f13 = offset, f14 = scale): calls 0x088F7878 with f12, then reduces
// offset + scale * result into [0, 360) by adding or subtracting 360 until it lies there; f0 = the result.
void func_088F78CC(Runtime &rt, AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const float offset = ctx.fpr[13];
    const float scale = ctx.fpr[14];
    const float keep20 = ctx.fpr[20];
    const float keep21 = ctx.fpr[21];
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[20] = scale;
    ctx.fpr[21] = offset;
    const float period = std::bit_cast<float>(0x43B40000u);
    const float zero = 0.0f;
    const float r = call_indirect(rt, ctx, 0x088F7878u, {}, {x, offset, scale}).f0;
    float y = offset + psp_mul(r, scale);
    if (y < zero) {
        y = y + period;
        while (y < zero) y = y + period;
    }
    if (period < y) {
        y = y - period;
        while (period < y) y = y - period;
    }
    ctx.fpr[0] = y;
    ctx.fpr[20] = keep20;
    ctx.fpr[21] = keep21;
}

// 0x08AFB6B4 and 0x08AFB74C (identical bodies): the base constructor on a0, the vtable, two frame-local helpers
// (0x08AFB4B4 and 0x08AFB0F8 with the frame), then the final call with a0 and the six argument registers.
void build_pair(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x100u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = self;
    ctx.gpr[17] = a1;
    ctx.gpr[18] = a2;
    ctx.gpr[19] = a3;
    ctx.gpr[20] = t0;
    ctx.gpr[21] = t1;
    call(rt, ctx, 0x08AFB4E8u, {self});
    put(rt, self, 0x08CD7848u);
    call(rt, ctx, 0x08AFB4B4u, {sp});
    call(rt, ctx, 0x08AFB0F8u, {sp});
    call(rt, ctx, 0x08AFAE68u, {self, a1, a2, a3, t0, t1});
}
void func_08AFB6B4(Runtime &rt, AllegrexContext &ctx) { build_pair(rt, ctx); }
void func_08AFB74C(Runtime &rt, AllegrexContext &ctx) { build_pair(rt, ctx); }

// 0x08B4F548 (a0 = object, a1 = source pointer): f0 = 0x08C524CC(a1); the two 3-float results go through the frame
// and the 0.9 branch picks the callees; v0 = the last callee's result.
void lerp_pair(Runtime &rt, AllegrexContext &ctx, u32 source, u32 target, u32 below_target, u32 above_target, u32 done) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = source;
    ctx.gpr[17] = self;
    const float t = call_indirect(rt, ctx, target, {source}, {}).f0;
    const float limit = std::bit_cast<float>(0x3F666666u);
    if (t < limit) {
        call(rt, ctx, below_target, {source, sp, sp + 4u, sp + 8u});
        call_indirect(rt, ctx, done, {self}, {float_at(rt, sp), float_at(rt, sp + 4u), float_at(rt, sp + 8u)});
    } else {
        call(rt, ctx, above_target, {source, sp, sp + 4u, sp + 8u});
        call_indirect(rt, ctx, 0x08B4C804u, {self}, {float_at(rt, sp), float_at(rt, sp + 4u), float_at(rt, sp + 8u)});
    }
}
void func_08B4F548(Runtime &rt, AllegrexContext &ctx) {
    lerp_pair(rt, ctx, ctx.gpr[5], 0x08C524CCu, 0x08B50994u, 0x08B5099Cu, 0x08B4C6BCu);
}
void func_08B4F82C(Runtime &rt, AllegrexContext &ctx) {
    lerp_pair(rt, ctx, ctx.gpr[5], 0x08C524CCu, 0x08C52718u, 0x08C52670u, 0x08B4C6BCu);
}

// 0x08A5CCA8 (a0 = object, a1 = word, a2..t3 = variadic words kept in the frame): the base constructor, the vtable,
// two builders on a frame block, then a call with the block's pointer; v0 = a0.
void func_08A5CCA8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 t2 = ctx.gpr[10];
    const u32 t3 = ctx.gpr[11];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = sp + 0x4u;
    ctx.gpr[17] = self;
    ctx.gpr[18] = a1;
    put(rt, sp + 0x48u, a2);
    put(rt, sp + 0x4Cu, a3);
    put(rt, sp + 0x50u, t0);
    put(rt, sp + 0x54u, t1);
    put(rt, sp + 0x58u, t2);
    put(rt, sp + 0x5Cu, t3);
    call(rt, ctx, 0x08A5CB28u, {self});
    put(rt, self, 0x08D74FDCu);
    call(rt, ctx, 0x08A9E138u, {sp + 4u, self});
    put(rt, sp, sp + 0x48u);
    call(rt, ctx, 0x08A9E20Cu, {sp + 4u, a1, sp + 0x48u});
    put(rt, sp + 4u, 0x08CD48E0u);
    call(rt, ctx, 0x08A9E848u, {sp + 4u});
    ctx.gpr[2] = self;
}

// 0x08A75B38 (a0 = object with three words, a1 = argument): allocates 4 bytes, sets the three words, calls the
// initialisers on the block and a frame slot, then the final call with the block and a1; v0 = its result.
void func_08A75B38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = 0;
    ctx.gpr[17] = self;
    ctx.gpr[18] = sp + 4u;
    ctx.gpr[19] = a1;
    put(rt, self, 0u);
    put(rt, self + 0x4u, 0u);
    put(rt, self + 0x8u, 0u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {4u, 4u});
    ctx.gpr[16] = block;
    put(rt, self + 0x8u, block + 4u);
    put(rt, self + 0x0u, block);
    put(rt, self + 0x4u, block);
    call(rt, ctx, 0x08A696E4u, {sp + 4u});
    call(rt, ctx, 0x08C251D0u, {block, 1u, sp + 4u, sp});
    call(rt, ctx, 0x08A69D68u, {sp + 4u});
    const u32 current = word(rt, self);
    put(rt, self + 0x4u, current + 4u);
    call(rt, ctx, 0x08A6A23Cu, {current, a1});
}

// 0x08898DB4 (a0 = object): the base constructor, the vtable, the member slots at +0x30 / +0x38 built from two
// allocations with their vtables; v0 = the second allocation.
void func_08898DB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x088F946Cu, {self});
    put(rt, self, 0x08CC20A8u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x40u, 0u);
    const u32 first = call(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, first + 0x0u, 0x08CC2220u);
    put(rt, first + 0xCu, self);
    put(rt, first + 0x4u, 0u);
    put(rt, first + 0x8u, 0u);
    put(rt, self + 0x30u, first);
    const u32 second = call(rt, ctx, 0x08B62344u, {0x14u});
    put(rt, second + 0x0u, 0x08CC32D0u);
    put(rt, second + 0x4u, 0x08CC3300u);
    put(rt, second + 0x8u, 0x08CC3318u);
    put(rt, second + 0xCu, 0x08CC332Cu);
    put(rt, second + 0x10u, 0u);
    put(rt, self + 0x38u, second);
    ctx.gpr[2] = second;
}

// 0x088CEDC4 (a0 = object): zeroes the header and the members, builds two sub-objects, calls the second base
// initialiser with a1 = 0x08CDD21C, and tail-calls 0x088CED68 with the object.
void func_088CEDC4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedS saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self + 0x130u, 0x78u);
        put(rt, self + 0x00u, 0u);
        put(rt, self + 0x04u, 0u);
        put(rt, self + 0x08u, 0u);
        put(rt, self + 0x0Cu, 0u);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x98u, 0u);
        put(rt, self + 0x9Cu, 0u);
        put8(rt, self + 0x94u, 0u);
        put8(rt, self + 0x95u, 0u);
        put(rt, self + 0x134u, 0u);
        put(rt, self + 0x124u, 0u);
        call(rt, ctx, 0x088CEA34u, {self});
        call(rt, ctx, 0x088CEA60u, {self});
        put_float(rt, self + 0x13Cu, 1.0f);
        put(rt, self + 0x140u, 0u);
        put(rt, self + 0x144u, 0u);
        put(rt, self + 0x148u, 0u);
        put(rt, self + 0x14Cu, 0u);
        put(rt, self + 0x150u, 0u);
        put(rt, self + 0x154u, 0u);
        put(rt, self + 0x158u, 0u);
        call(rt, ctx, 0x088CEA7Cu, {self, 0x08CDD21Cu});
        put(rt, self + 0xA0u, 0u);
        put8(rt, self + 0xA4u, 0u);
        put8(rt, self + 0xA5u, 0u);
    }
    tail_call(ctx, 0x088CED68u, {self});
}

// 0x0898D3DC (a0 = object): the base constructor and two embedded members, a float pair (256.0 / 24.0) in the frame,
// then the final initialiser; v0 = its result.
void func_0898D3DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = frame.at(0);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x0896BDF8u, {self});
    put(rt, self, 0x08CBC030u);
    put(rt, self + 0x38u, 0x08CBC26Cu);
    call(rt, ctx, 0x08984824u, {self + 0xF0u});
    call(rt, ctx, 0x089CC6C0u, {self + 0x1E0u});
    put(rt, self + 0x1F4u, 0x08D74FDCu);
    put(rt, self + 0x1E4u, 0u);
    put8(rt, self + 0x1E8u, 0u);
    put8(rt, self + 0x1E9u, 0u);
    put8(rt, self + 0x1EAu, 0u);
    put8(rt, self + 0x1EBu, 0u);
    put(rt, self + 0x1ECu, 0u);
    put(rt, self + 0x1F0u, 0u);
    put_float(rt, sp + 0x4u, 256.0f);
    put_float(rt, sp + 0x0u, 24.0f);
    call(rt, ctx, 0x0896A9C4u, {self, 0u, 0u, sp + 4u, sp});
}

// 0x08A24DAC (a0 = object): calls two member initialisers on the object and on object+0x10 / +0x34, sets the vtables
// and float fields, then the final call on 0 that gives the object's +0x58 member; v0 = that result.
void func_08A24DAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08AB9270u, {self});
    call(rt, ctx, 0x08AB9284u, {self + 0x10u});
    call(rt, ctx, 0x08AB9284u, {self + 0x34u});
    put(rt, self + 0x68u, 0x08D74FDCu);
    put_float(rt, self + 0x88u, 1.0f);
    put(rt, self + 0x58u, 0u);
    put(rt, self + 0x5Cu, 0u);
    put(rt, self + 0x60u, 0u);
    put(rt, self + 0x64u, 0x08D74FDCu);
    put(rt, self + 0x6Cu, 0u);
    put(rt, self + 0x70u, 0u);
    put_float(rt, self + 0x74u, 1.0f);
    put(rt, self + 0x78u, 0u);
    put_float(rt, self + 0x7Cu, 1.0f);
    put_float(rt, self + 0x80u, 1.0f);
    put(rt, self + 0x84u, 0u);
    const u32 result = call(rt, ctx, 0x08ABB9E8u, {0u});
    put(rt, self + 0x58u, result);
    put(rt, self + 0x20u, self);
    put(rt, self + 0x44u, self);
    ctx.gpr[2] = result;
}

// 0x08B4AE1C (f12 = t, a0 = out, a1 / a2 = two points): the lerp of the two points by t and 1 - t through
// 0x08B4A560 and 0x08B4AC0C, written to out; void.
void func_08B4AE1C(Runtime &rt, AllegrexContext &ctx) {
    const float t = ctx.fpr[12];
    const u32 out = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const float keep20 = ctx.fpr[20];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0);
    ctx.fpr[20] = t;
    ctx.gpr[16] = a2;
    ctx.gpr[17] = out;
    const float a1x = float_at(rt, a1);
    const float a1y = float_at(rt, a1 + 0x4u);
    put_float(rt, sp + 0xCu, a1y);
    put_float(rt, sp + 0x8u, a1x);
    call_indirect(rt, ctx, 0x08B4A560u, {sp + 8u}, {1.0f - t});
    const float a2x = float_at(rt, a2);
    const float a2y = float_at(rt, a2 + 0x4u);
    put_float(rt, sp + 0x4u, a2y);
    put_float(rt, sp + 0x0u, a2x);
    call_indirect(rt, ctx, 0x08B4A560u, {sp}, {t});
    const float first = float_at(rt, sp + 0x8u);
    const float second = float_at(rt, sp + 0xCu);
    put_float(rt, sp + 0x10u, first);
    put_float(rt, sp + 0x14u, second);
    call(rt, ctx, 0x08B4AC0Cu, {sp + 0x10u, sp + 0x10u, sp});
    put_float(rt, out + 0x4u, float_at(rt, sp + 0x14u));
    put_float(rt, out + 0x0u, float_at(rt, sp + 0x10u));
    ctx.fpr[20] = keep20;
}

} // namespace
} // namespace new_batch_main_1620
} // namespace gt::game

#define NEW1620(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1620::name, gt::ReturnKind::kind)
NEW1620(0x0891E198u, func_0891E198, Word);
NEW1620(0x08932B74u, func_08932B74, Word);
NEW1620(0x0897D930u, func_0897D930, Word);
NEW1620(0x08A5102Cu, func_08A5102C, Word);
NEW1620(0x08AC2168u, func_08AC2168, Word);
NEW1620(0x0892145Cu, func_0892145C, Word);
NEW1620(0x089CBA20u, func_089CBA20, Word);
NEW1620(0x089FA008u, func_089FA008, Word);
NEW1620(0x08A7DC44u, func_08A7DC44, Word);
NEW1620(0x08B56FD8u, func_08B56FD8, Word);
NEW1620(0x08896970u, func_08896970, Word);
NEW1620(0x088F78CCu, func_088F78CC, Float);
NEW1620(0x08AFB6B4u, func_08AFB6B4, Word);
NEW1620(0x08AFB74Cu, func_08AFB74C, Word);
NEW1620(0x08B4F548u, func_08B4F548, Word);
NEW1620(0x08B4F82Cu, func_08B4F82C, Word);
NEW1620(0x08A5CCA8u, func_08A5CCA8, Word);
NEW1620(0x08A75B38u, func_08A75B38, Word);
NEW1620(0x08898DB4u, func_08898DB4, Word);
NEW1620(0x088CEDC4u, func_088CEDC4, Word);
NEW1620(0x0898D3DCu, func_0898D3DC, Word);
NEW1620(0x08A24DACu, func_08A24DAC, Word);
NEW1620(0x08B4AE1Cu, func_08B4AE1C, Void);
