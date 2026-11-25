// New batch, main session 1023: the five large functions (294-297 instructions) that were drafted before and either failed a
// gate route or were dropped twice as too hard. Written as structured C from `tools/disasm`; a draft batch, not verified here.
//
// Four of them are one family — the reference-counted "take a counted object and store it in a pair" shape (the strong count
// at +4, the weak count at +8, the vtable words at +8 / +0xC running when a count reaches zero, the ll / sc pairs read as a
// plain load and store: one host thread, and the `sc` success flag is 1 in v0 either way). Their earlier drafts lived in
// hot_batch_117 (0x0890F1C0, 0x08BE476C, 0x0894106C) and hot_batch_118 (0x08BC728C), registered nowhere ("compared 0 in the
// full gate": no case reached them with valid arguments, which is what the input shapes below are for). Each line was re-read
// against the asm here; where the earlier draft passed a register it had itself computed, the asm's live value is passed.
// 0x08B50EFC comes from the draft in 82aa861 (new_batch_main_47.cpp) that a5061323 dropped after a fast-gate route answered
// f0 = 0.187 where the original answered 1.0. Its determinant chain (asm up to the store at 08B51080) and the exit are
// re-checked against the asm; the answer is the word the function stores at sp+0 (08B51364 reads it back), not a local, and
// that store was missing from the old draft.
// No names: none of the five has evidence for one.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1023 {
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
// A guest call with float arguments ($f12, ...) whose $f0 is read afterwards.
GuestResult call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                   std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The `ll / sc` pair the originals use for every count step: read, add, write, retry while the store-condition failed.
void count_up(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
// The same, decrementing; true when the old value was 1, i.e. the step that takes the count to zero.
bool count_down(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old == 1u;
}

// A counted block's release: the strong count at +4 steps down and, when that was the last, virtual +8 runs; then the weak
// count at +8 steps down and its virtual +0xC runs when that was the last.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (count_down(rt, object + 4u)) ctx.gpr[2] = call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (count_down(rt, object + 8u)) ctx.gpr[2] = call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// The saved registers the long bodies touch: s0-s7, fp and f20-f31 (f17 of 0x08B50EFC is a scratch spill, not a save).
struct SavedAll {
    AllegrexContext &ctx;
    u32 gpr[9];
    float fpr[12];
    explicit SavedAll(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) gpr[i] = c.gpr[16u + i];
        gpr[8] = c.gpr[30];
        for (unsigned i = 0u; i < 12u; ++i) fpr[i] = c.fpr[20u + i];
    }
    ~SavedAll() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = gpr[i];
        ctx.gpr[30] = gpr[8];
        for (unsigned i = 0u; i < 12u; ++i) ctx.fpr[20u + i] = fpr[i];
    }
};

// 0x08BC728C (result = a0, holder = a1, key = a2, item = a3, tag = t0) -> result: with the owner O = *holder (and {0, 0}
// returned when it is null) locked through O + 0x6C, the list whose sentinel is O + 0x98 (nodes: next +0, a string object +8
// whose length sits at -0xC) is walked comparing each node's text with the key's (*key, length at -0xC) over the shorter
// length (0x08B36170) and then the lengths. A node found with item == 0 is erased (0x08BBFE68) and {0, 0} returned; with an
// item, 0x08B2CB18(O + 8, item, tag) is the new handle h, a 0x18-byte record {vtable 0x08CC52B8, 1, 1, h, *holder, shared} is
// allocated, 0x08B8E380 links it, the node's +0xC becomes h and its +0x10 shared slot is reassigned (weak counts stepped);
// result = {h, record}. Ends with 0x08BBCFA0(holder); v0 = result.

// 0x08BE476C (out = a0, pair = a1, argument = a2, extra = a3) -> out: pair is {object +0, counted block +4}. An empty pair
// zeroes the two frame slots; otherwise, under the block's own lock at object + 0x6C (0x08B34888 / 0x08B34B24),
// 0x08B2CB18(object + 8, argument, extra) is the handle h and a 0x18-byte record {vtable 0x08CC2F90, 1, 1, h, object, block}
// is allocated (0x08B62344) with the block's use count stepped up four times and down three (the releases), 0x08B8E380 links
// it and frame +8 = h, frame +0xC = the record. Then 0x08BE43B0(frame +0, frame +8) fills a second pair, a 0xA0-byte object is
// allocated and initialised by 0x08BE45C4(object, frame +0, argument, 0), stored at out +0, and a 0x10-byte record {vtable
// 0x08CC6928, 1, 1, object} at out +4; 0x08B8E380(out + 4, object, object) links it, with the object destroyed through
// 0x08BE427C / 0x08B28BE0 / 0x08B2C3BC / 0x08B64654 when that record is null. Both frame blocks (+4, +0xC) are released.

// 0x0890F1C0 (out = a0, owner = a1, key = a2) -> 0 or 1: 0x08B0EC60(frame + 0x60) fills a 0x3C-byte record that is copied to
// frame +0x18; a non-zero first word returns 0. 0x089624EC(key) answers a pair {object +0, counted block +4} (empty leaves
// frame +0x10 / +0x14 zero). Otherwise, under the lock at object + 0x6C, 0x08B2CB18(object + 8, record word +0x28, 0x80) is
// the handle h; a 0x18-byte record {vtable 0x08CC2F90, 1, 1, h, object, block} is allocated (0x08B62344), the block's count is
// stepped three times (one up before the record, two after) and the record linked by 0x08B8E380(frame +0x14, h, h); frame
// +0x10 = h. out is assigned the pair {frame +0x10, frame +0x14} the way a shared pointer is (old block released). When
// out's word is non-zero a closure {0x0890EF6C, out, that word, record word +0x28} is registered with
// 0x08AC0C38(owner, closure pointer, closure, 1) and 0x08AC0B50() runs, answering 1; otherwise 0.

// 0x0894106C (self = a0, a1, first_word = a2, second_word = a3, mode = t0, f12-f15 = x, y, z, angle) -> 0: the object's frame
// setup. s = *self (its +0x14 is the listener whose virtual +8 takes the vector) and b = self->+8, with self->+0xC the source
// of the offsets; 0x089545BC(sp + 8) fills the four floats the pass starts from. For mode 2 the offset is b->+0, for mode 1 the
// negated b->+0x70; it rotates x / y through 0x08B498AC(angle). The vector {x, z, -y} plus first_word / second_word goes to the
// listener; s +0x10..+0x18 and +0x690 / +0x694 are written and 0x0893C3E4 / 0x08954870 run. For the four records
// 0x08955300(self, i) with the transform 0x0893C780: the position, the words +0x690 / +0x694, the listener, the flag byte
// +0xE9..+0xED / floats +0xBC / +0xD0 (0x08954618 / 0x08954670 / 0x08954688) and the two counters s +0x69D / +0x69E; bit 4 of
// s +0xC is cleared. A second pass fills each record's floats +0x54 / +0x58 / +0xCC / +0x10 / +0x14 / +0x18 / +0x1C / +0xC
// from b through 0x0894E9B4 / 0x0894E714 / 0x0893D0F4 / 0x0893D064. Then 0x08945F6C(self, 1), the angle 0x08B4A430 adds to
// s +0x6FC, 0x0893C3E4 again, s +0x18, 0x08954870 and the copies into s +0x764..+0x774; unless byte s +3 is 4, 0x0895A004
// gets *self and byte s +0.

// 0x08B50EFC: the inverse of a 4x4 matrix given as 16 floats — f12-f19 are rows 0 and 1, the eight stack arguments at the
// caller's sp+0x20..0x3C (this frame's +0xA0..+0xBC) are rows 2 and 3 — written through the 16 pointers a0-a3, t0-t3 and the
// eight stack words at this frame's +0x80..+0x9C; f0 = the determinant. The body is one straight-line cofactor expansion, so
// its locals are named after the FPU registers it moves through: the order of the float operations is the identity of the
// function and has to be read against `tools/disasm`. The registers it spills to its frame are locals.
void func_08B50EFC(Runtime &rt, AllegrexContext &ctx) {
    const float in0 = float_at(rt, ctx.gpr[29] + 0xA0u);
    const float in1 = float_at(rt, ctx.gpr[29] + 0xA4u);
    const float in2 = float_at(rt, ctx.gpr[29] + 0xA8u);
    const float in3 = float_at(rt, ctx.gpr[29] + 0xACu);
    const float in4 = float_at(rt, ctx.gpr[29] + 0xB0u);
    const float in5 = float_at(rt, ctx.gpr[29] + 0xB4u);
    const float in6 = float_at(rt, ctx.gpr[29] + 0xB8u);
    const float in7 = float_at(rt, ctx.gpr[29] + 0xBCu);
    SavedAll saved(ctx);
    GuestFrame frame(ctx, 0x80u);
    float f10 = ctx.fpr[10], f11 = ctx.fpr[11], f12 = ctx.fpr[12], f13 = ctx.fpr[13];
    float f14 = ctx.fpr[14], f15 = ctx.fpr[15], f16 = ctx.fpr[16], f17 = ctx.fpr[17];
    float f18 = ctx.fpr[18], f19 = ctx.fpr[19];
    float f20 = ctx.fpr[20], f21 = ctx.fpr[21], f22 = ctx.fpr[22], f23 = ctx.fpr[23];
    float f24 = ctx.fpr[24], f25 = ctx.fpr[25], f26 = ctx.fpr[26], f27 = ctx.fpr[27];
    float f28 = ctx.fpr[28], f29 = ctx.fpr[29], f30 = ctx.fpr[30], f31 = ctx.fpr[31];
    float f0 = ctx.fpr[0], f1 = ctx.fpr[1], f2 = ctx.fpr[2], f3 = ctx.fpr[3];
    float f4 = ctx.fpr[4], f5 = ctx.fpr[5], f6 = ctx.fpr[6], f7 = ctx.fpr[7];
    float f8 = in1;
    float f9 = in0;
    const float saved17 = ctx.fpr[17];
    f5 = psp_mul(ctx.fpr[16], ctx.fpr[13]);
    f7 = psp_mul(ctx.fpr[12], ctx.fpr[17]);
    f0 = in2;
    f17 = in6;
    f1 = in7;
    f2 = in6;
    f4 = in3;
    f23 = ctx.fpr[16];
    f3 = psp_mul(f2, f4);
    f16 = in7;
    f6 = psp_mul(f0, f1);
    f4 = psp_mul(ctx.fpr[12], f8);
    f1 = psp_mul(ctx.fpr[13], f9);
    f0 = psp_mul(f17, ctx.fpr[19]);
    f2 = psp_mul(f16, ctx.fpr[18]);
    f6 = f6 - f3;
    f4 = f4 - f1;
    f22 = in2;
    f2 = f2 - f0;
    f1 = psp_mul(f22, ctx.fpr[19]);
    f21 = in3;
    f16 = in5;
    f3 = psp_mul(f21, ctx.fpr[18]);
    f7 = f7 - f5;
    f20 = in4;
    float sp10 = f2;
    f3 = f3 - f1;
    f2 = psp_mul(ctx.fpr[12], f16);
    f0 = psp_mul(ctx.fpr[13], f20);
    f1 = psp_mul(f17, ctx.fpr[15]);
    f24 = saved17;
    float sp1C = f7;
    f20 = f2 - f0;
    f5 = psp_mul(f24, f9);
    float sp18 = f6;
    f6 = psp_mul(f23, f8);
    f7 = f15;
    float sp14 = f4;
    f6 = f6 - f5;
    f5 = psp_mul(f23, f16);
    f25 = in7;
    f2 = sp18;
    f8 = sp10;
    f4 = psp_mul(f25, ctx.fpr[14]);
    float sp0C = f3;
    f4 = f4 - f1;
    f3 = sp14;
    f27 = sp1C;
    f0 = psp_mul(f27, f2);
    f2 = psp_mul(f3, f8);
    f30 = ctx.fpr[14];
    f0 = f0 - f2;
    f2 = psp_mul(f22, f15);
    const float sp20 = ctx.fpr[12];
    const float sp24 = ctx.fpr[13];
    float sp08 = f6;
    f9 = in4;
    f15 = sp0C;
    float sp04 = f4;
    f1 = psp_mul(f24, f9);
    f4 = psp_mul(f21, ctx.fpr[14]);
    f3 = psp_mul(f20, f15);
    f22 = sp08;
    f17 = in0;
    f21 = in1;
    f24 = sp04;
    f31 = f4 - f2;
    f8 = f5 - f1;
    f0 = f0 + f3;
    f1 = psp_mul(f21, f9);
    f3 = psp_mul(f22, f24);
    f6 = psp_mul(f17, f16);
    f4 = psp_mul(ctx.fpr[19], ctx.fpr[14]);
    f2 = psp_mul(ctx.fpr[18], f7);
    f5 = psp_mul(f8, f31);
    f22 = f6 - f1;
    f0 = f0 + f3;
    f4 = f4 - f2;
    f0 = f0 - f5;
    f1 = psp_mul(f22, f4);
    f0 = f0 + f1;
    const float det = f0;
    put_float(rt, frame.at(0u), det); // 08B51080: the determinant goes to sp+0 and the epilogue reads it back
    f0 = 0.0f;
    f25 = det;
    f29 = ctx.fpr[18];
    if (f25 != f0) {
        float sp2C, sp30, sp34, sp38, sp3C, sp40, sp44, sp48, sp4C;
        f27 = psp_mul(f17, f4);
        f1 = sp08;
        f6 = in4;
        f2 = sp14;
        f28 = psp_mul(f16, f4);
        f26 = psp_mul(f21, f4);
        f17 = psp_mul(f1, f7);
        f21 = psp_mul(f29, f20);
        f2 = psp_mul(f2, f29);
        f4 = psp_mul(f6, f4);
        f0 = 1.0f;
        f0 = f0 / f25;
        f3 = saved17;
        f5 = sp18;
        sp34 = f27;
        f11 = psp_mul(f23, f24);
        f13 = psp_mul(f12, f15);
        f24 = sp0C;
        f25 = in1;
        f27 = sp04;
        sp48 = f2;
        f10 = psp_mul(f12, f5);
        f2 = psp_mul(f25, f27);
        sp2C = f4;
        f9 = psp_mul(f3, f31);
        f4 = psp_mul(f3, f5);
        f15 = sp14;
        f5 = sp10;
        sp3C = f17;
        f18 = psp_mul(f1, f14);
        f12 = psp_mul(f15, f19);
        sp40 = f21;
        f21 = psp_mul(f16, f24);
        f15 = psp_mul(f16, f31);
        f3 = sp24;
        f6 = in0;
        f17 = sp18;
        f24 = in4;
        f25 = in3;
        f27 = in3;
        f16 = psp_mul(f23, f17);
        f1 = psp_mul(f3, f5);
        f17 = psp_mul(f24, f31);
        f3 = psp_mul(f6, f5);
        f24 = psp_mul(f25, f8);
        f6 = psp_mul(f19, f22);
        f25 = psp_mul(f7, f22);
        f5 = psp_mul(f27, f20);
        f19 = psp_mul(f19, f20);
        f7 = psp_mul(f7, f8);
        f14 = psp_mul(f14, f22);
        f22 = psp_mul(f29, f22);
        f29 = in2;
        f30 = psp_mul(f30, f8);
        f4 = f4 + f21;
        f20 = psp_mul(f29, f20);
        f6 = f6 - f24;
        f5 = f5 - f25;
        f7 = f7 - f19;
        f25 = sp10;
        f19 = sp40;
        f21 = sp1C;
        f24 = sp20;
        f30 = f19 - f30;
        f21 = psp_mul(f21, f29);
        f24 = psp_mul(f24, f25);
        f8 = psp_mul(f29, f8);
        f23 = psp_mul(f23, f31);
        f2 = f2 - f15;
        f3 = f3 - f16;
        f15 = sp2C;
        f16 = sp34;
        f10 = f10 + f17;
        f14 = f14 - f20;
        f17 = sp3C;
        f20 = sp48;
        f11 = f11 - f15;
        f13 = f13 + f16;
        sp38 = f23;
        f12 = f12 - f17;
        f8 = f8 - f22;
        f23 = sp0C;
        f22 = sp24;
        sp44 = f30;
        f18 = f18 - f20;
        f1 = f1 + f28;
        sp4C = f21;
        f9 = f9 - f26;
        f26 = psp_mul(f22, f23);
        sp30 = f24;
        f29 = in7;
        f27 = sp1C;
        f15 = in3;
        f16 = sp1C;
        f17 = in6;
        f19 = in1;
        f20 = sp18;
        f21 = saved17;
        f28 = psp_mul(f27, f29);
        f30 = psp_mul(f16, f17);
        f27 = psp_mul(f27, f15);
        f16 = psp_mul(f22, f20);
        f15 = psp_mul(f19, f25);
        f22 = sp04;
        f25 = in0;
        f24 = in4;
        f17 = psp_mul(f21, f22);
        f20 = psp_mul(f25, f22);
        f22 = sp08;
        f19 = psp_mul(f24, f23);
        f4 = f4 - f15;
        f21 = psp_mul(f29, f22);
        f23 = sp14;
        f15 = sp30;
        f2 = f2 - f16;
        f16 = sp38;
        f22 = psp_mul(f29, f23);
        f1 = f1 - f17;
        f3 = f3 - f19;
        f9 = f9 - f26;
        f10 = f10 - f20;
        f11 = f11 - f15;
        f13 = f13 - f16;
        f6 = f6 + f21;
        f4 = psp_mul(f0, f4);
        f2 = psp_mul(f0, f2);
        f1 = psp_mul(f0, f1);
        f9 = psp_mul(f0, f9);
        f3 = psp_mul(f0, f3);
        f10 = psp_mul(f0, f10);
        f11 = psp_mul(f0, f11);
        f13 = psp_mul(f0, f13);
        f6 = psp_mul(f0, f6);
        f5 = f5 - f22;
        f7 = f7 + f28;
        f5 = psp_mul(f0, f5);
        f12 = f12 - f27;
        f7 = psp_mul(f0, f7);
        f12 = psp_mul(f0, f12);
        f25 = in6;
        f29 = sp08;
        f24 = psp_mul(f25, f29);
        f17 = sp44;
        f19 = sp4C;
        f25 = psp_mul(f25, f23);
        f8 = f8 - f24;
        f28 = f17 - f30;
        f14 = f14 + f25;
        f8 = psp_mul(f0, f8);
        f18 = f18 + f19;
        f14 = psp_mul(f0, f14);
        f18 = psp_mul(f0, f18);
        f0 = psp_mul(f0, f28);
        put_float(rt, ctx.gpr[4], f4);
        put_float(rt, ctx.gpr[5], f2);
        put_float(rt, ctx.gpr[6], f1);
        put_float(rt, ctx.gpr[7], f9);
        put_float(rt, ctx.gpr[8], f3);
        put_float(rt, ctx.gpr[9], f10);
        put_float(rt, ctx.gpr[10], f11);
        put_float(rt, ctx.gpr[11], f13);
        put_float(rt, word(rt, frame.at(0x80u)), f6);
        put_float(rt, word(rt, frame.at(0x84u)), f5);
        put_float(rt, word(rt, frame.at(0x88u)), f7);
        put_float(rt, word(rt, frame.at(0x8Cu)), f12);
        put_float(rt, word(rt, frame.at(0x90u)), f8);
        put_float(rt, word(rt, frame.at(0x94u)), f14);
        put_float(rt, word(rt, frame.at(0x98u)), f0);
        put_float(rt, word(rt, frame.at(0x9Cu)), f18);
    }
    ctx.fpr[0] = float_at(rt, frame.at(0u)); // 08B51364: the answer is the word at sp+0, not the register
}
} // namespace
} // namespace new_batch_main_1023
} // namespace gt::game

#define NEW1023(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1023::name, gt::ReturnKind::kind)
NEW1023(0x08B50EFCu, func_08B50EFC, Float);
