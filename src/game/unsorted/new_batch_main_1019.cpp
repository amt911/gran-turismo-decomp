// New batch main 1019: long functions the main-session seeds kept dropping, hand-written as structured C from `tools.disasm`
// (two of them were drafted before; what differed from the asm is fixed here). Draft only: the differential gate (fast,
// sabotage, synthetic) is run by the verifier, not here.
// Conventions, as in new_batch_3: a `jal` passes the argument registers the original sets before it (the rest keep the
// caller's value); the delay slot of a `jal` runs BEFORE the call; `v0` is set as the original leaves it on every exit path.
// The frames callees read or write are `GuestFrame`s, size = the original's `addiu sp, sp, -N`. Callee-saved registers the
// bodies use are saved and restored, because the harness compares them. `0x08A76330` was skipped: its body is an inlined
// std::string / vector<std::string> parser whose frame-relative capacity and length words cannot be reproduced faithfully.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1019 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// A `jalr $v0` through a vtable slot: the callee starts with v0 = its own address, as after the original's `lw v0, slot(vtable)`.
u32 call_at(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_set(rt, ctx, target, arguments);
}

// The same with float arguments in $f12-$f15; v0 afterwards is the callee's, f0 its float result.
u32 call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
              std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Where the prologue of a function stores a callee-saved register: `reg` 0-31 is a general register, 32-63 a floating one
// (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};

// The callee-saved registers (s0-s7, fp, f20-f31) a function keeps values in, and the stores of its prologue: they are stored
// where the asm stores them (a callee can read the stack below its caller's frame), set as the asm sets them because callees
// can read them too, and given back when the function returns.
class SavedRegisters {
public:
    SavedRegisters(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        fp_ = ctx.gpr[30];
        for (unsigned i = 0; i < 12; ++i) fpr_[i] = ctx.fpr[20 + i];
        for (const SavedSlot &slot : slots) {
            if (slot.reg < 32u) put(rt, frame.at(slot.offset), ctx.gpr[slot.reg]);
            else put_float(rt, frame.at(slot.offset), ctx.fpr[slot.reg - 32u]);
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
        for (unsigned i = 0; i < 12; ++i) ctx_.fpr[20 + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

// PSP libstdc++'s counted objects (shared_ptr control blocks): a strong count at +4, a weak count at +8 and a vtable at +0
// whose +8 / +0xC slots dispose of the object and destroy the block. The original bumps a count with an `ll` / `sc` retry
// loop; the harness' runs are single-threaded, so a plain read / write leaves the same word.
void count_up(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
void retain_counted(Runtime &rt, u32 block) {
    if (block == 0u) return;
    count_up(rt, block + 4u);
}
// Both drop a count and answer the value it had: 1 means it is now zero.
u32 drop_strong_count(Runtime &rt, u32 block) {
    const u32 before = word(rt, block + 4u);
    put(rt, block + 4u, before - 1u);
    return before;
}
u32 drop_weak_count(Runtime &rt, u32 block) {
    const u32 before = word(rt, block + 8u);
    put(rt, block + 8u, before - 1u);
    return before;
}
// `if (--*strong == 0) { vtable[8](this); if (--*weak == 0) vtable[0xC](this); }`, the vtable reloaded for the second call as
// the asm does.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 block) {
    if (block == 0u) return;
    if (drop_strong_count(rt, block) != 1u) return;
    call_at(rt, ctx, word(rt, word(rt, block) + 8u), {block});
    if (drop_weak_count(rt, block) != 1u) return;
    call_at(rt, ctx, word(rt, word(rt, block) + 0xCu), {block});
}

// 0x08963418 (a0 = out, a1 = a {pointer, counted block} pair): out's four words are cleared. With a pointer the first counted
// object is built: 0x08B2B4D8(0x80, pointer) feeds 0x08BE43E4, whose block lands at frame +0; 0x08B62344(0xA0) feeds
// 0x08BE45C4(block, frame, pointer, 0) (the object, kept at frame +0x18); and 0x08B62344(0x10) builds the {0x08CC6928, 1, 1,
// object} block that 0x08B8E380 links to frame +0x1C. The temporary block at frame +4 is released, out[0] = object and out[1]
// takes the new block (its strong count up, the previous block released unless it is the same one); the block at frame +0x1C
// goes too. The second half does the same for 0x08B59AC8(second): 0x08BE43E4's block at frame +8 and 0x08BE449C's
// {pointer, block} at frame +0x10, so out[2] = its pointer and out[3] = its block with the previous one released; both
// temporaries are released. v0 = the object on the middle paths, the last release call's answer at the end.

// 0x08BD837C (a0 = out, a1 = a {object, counted block} pair, a2 = an argument, t0 = another): out's two words are cleared, and
// so they stay when the pair's object is null or 0x08BD5D54(object, a2) answers object + 0x98. Otherwise, under object +0x6C's
// lock (0x08B34888 / 0x08B34B24), 0x08B2CB18(object + 8, 0x10, t0) gives a handle h: a null answer releases h through
// 0x08BBFF7C. With h, its four words are cleared and the pair's block's use count rises three times around a 0x18-byte control
// block {vtable 0x08CC55E0, counts 1 / 1, h, object, block} (0x08B62344); when that allocation failed, 0x08909D98(h),
// 0x08BBD3B4(object, h) and 0x08BBEA18(object + 0x98, block) run. 0x08B8E380(frame + 4, h, h) links the block, the handle's
// +0xC takes h, the block's weak count rises and the handle's +0x10 slot drops its old block's weak count (its vtable +0xC
// when it reaches zero) before taking the new one. out then takes the handle and the block (use count up), the temporary
// block is released and 0x08BBCFA0(pair) ends it. v0 = out.

// 0x088D390C (a0 = p, a1 = self, a2 = out, a3 = mode, f12 = a, f13 = b): self's and out's three words are cleared; when
// 0x088DB5C4(p->+0) fails the function ends there (the two dead counter loops the asm carries before the clear are dropped).
// Two 0x088D2BC8(p) reads at (a + 2) + b and a - 2 give the scalars two 0x088D32E0(p, mode) vectors at frame +0x54 and +0x48
// are turned by: the first is kept at +0x90, the second copied to +0x3C / +0x9C. 0x08B4AA68's cross product of the two goes to
// +0x84, and its length (0x08B4A720, sqrt, 0x08B4A5E8) normalises it: 0x08B66E88 of -y and 0x08B66FBC of (x, z) give out +0 and
// out +4. The (0, 1, 0) vector at +0x6C turned by 0x08B4A9D0(+0x60, +0x6C, +0x84) is copied to +0x24 / +0x6C and normalised
// into +0x24; 0x08B4AA68(+0x30 = +0x90, +0x24), with 5.0 (f23) added on y, is +0xA8, and 0x08B4A5E8(+0x0C) with 0x08B4AA34(+0x18
// = +0x90, +0x0C), with 5.0 on y, is +0xB4. 0x088FAB84(0x088DB5C4(p->+0), +0xC0, +0xA8) then (+0xB4) fill +0xC0, copied to
// +0x00 / +0xB4; 0x08B4AA68(+0x00, +0xA8) normalised into +0x6C and 0x08B4A9D0(+0x60, +0x84, +0x6C); 0x08B66FBC of
// (sqrt(x^2 + z^2), y) of +0x60 gives out +8, and self takes +0x90's three floats. The frame pair at +0xF0 / +0xF4 (0, -1) is
// written before the first 0x088DB5C4 of that pair, as the original does.
void func_088D390C(Runtime &rt, AllegrexContext &ctx) {
    const u32 p = ctx.gpr[4];
    const u32 self = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    const u32 mode = ctx.gpr[7];
    const float a = ctx.fpr[12];
    const float b = ctx.fpr[13];
    GuestFrame frame(ctx, 0x140u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x120u}, {23u, 0x11Cu}, {22u, 0x118u}, {20u, 0x110u}, {31u, 0x124u},
                                           {21u, 0x114u}, {19u, 0x10Cu}, {18u, 0x108u}, {17u, 0x104u}, {16u, 0x100u},
                                           {32u + 23u, 0x13Cu}, {32u + 22u, 0x138u}, {32u + 21u, 0x134u}, {32u + 20u, 0x130u}});
    const u32 v00 = frame.at(0u), v0c = frame.at(0x0Cu), v18 = frame.at(0x18u), v24 = frame.at(0x24u);
    const u32 v30 = frame.at(0x30u), v3c = frame.at(0x3Cu), v48 = frame.at(0x48u), v54 = frame.at(0x54u);
    const u32 v60 = frame.at(0x60u), v6c = frame.at(0x6Cu), v84 = frame.at(0x84u), v90 = frame.at(0x90u);
    const u32 v9c = frame.at(0x9Cu), va8 = frame.at(0xA8u), vb4 = frame.at(0xB4u), vc0 = frame.at(0xC0u);
    ctx.gpr[30] = out;
    ctx.gpr[23] = p;
    ctx.gpr[22] = self;
    ctx.gpr[20] = mode;
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    if (call_set(rt, ctx, 0x088DB5C4u, {word(rt, p)}) == 0u) return;
    call_setf(rt, ctx, 0x088D2BC8u, {p}, {(a + 2.0f) + b});
    const float first = ctx.fpr[0];
    call_setf(rt, ctx, 0x088D2BC8u, {p}, {a - 2.0f});
    const float second = ctx.fpr[0];
    call_setf(rt, ctx, 0x088D32E0u, {v54, p, mode}, {first});
    put_float(rt, v90, float_at(rt, v54));
    put_float(rt, v90 + 4u, float_at(rt, v54 + 4u));
    put_float(rt, v90 + 8u, float_at(rt, v54 + 8u));
    call_setf(rt, ctx, 0x088D32E0u, {v48, p, mode}, {second});
    ctx.gpr[20] = va8;
    const float x2 = float_at(rt, v48), y2 = float_at(rt, v48 + 4u), z2 = float_at(rt, v48 + 8u);
    put_float(rt, v3c, x2);
    put_float(rt, v3c + 4u, y2);
    put_float(rt, v3c + 8u, z2);
    put_float(rt, v9c, x2);
    put_float(rt, v9c + 4u, y2);
    put_float(rt, v9c + 8u, z2);
    call_set(rt, ctx, 0x08B4AA68u, {v3c, v3c, v90});
    put_float(rt, v84, float_at(rt, v3c));
    put_float(rt, v84 + 4u, float_at(rt, v3c + 4u));
    put_float(rt, v84 + 8u, float_at(rt, v3c + 8u));
    call_set(rt, ctx, 0x08B4A720u, {v84});
    const float cross_length = std::sqrt(ctx.fpr[0]);
    call_setf(rt, ctx, 0x08B4A5E8u, {v84}, {1.0f / cross_length});
    call_setf(rt, ctx, 0x08B66E88u, {v84}, {-float_at(rt, v84 + 4u)});
    put_float(rt, out, ctx.fpr[0]);
    call_setf(rt, ctx, 0x08B66FBCu, {v84}, {float_at(rt, v84), float_at(rt, v84 + 8u)});
    put_float(rt, out + 4u, ctx.fpr[0]);
    put(rt, v6c, 0u);
    put_float(rt, v6c + 4u, 1.0f);
    put(rt, v6c + 8u, 0u);
    call_set(rt, ctx, 0x08B4A9D0u, {v60, v6c, v84});
    const float t0 = float_at(rt, v60), t1 = float_at(rt, v60 + 4u), t2 = float_at(rt, v60 + 8u);
    put_float(rt, v24, t0);
    put_float(rt, v24 + 4u, t1);
    put_float(rt, v24 + 8u, t2);
    put_float(rt, v6c, t0);
    put_float(rt, v6c + 4u, t1);
    put_float(rt, v6c + 8u, t2);
    call_setf(rt, ctx, 0x08B4A5E8u, {v24}, {1.0f});
    put_float(rt, v30, float_at(rt, v90));
    put_float(rt, v30 + 4u, float_at(rt, v90 + 4u));
    put_float(rt, v30 + 8u, float_at(rt, v90 + 8u));
    call_set(rt, ctx, 0x08B4AA68u, {v30, v30, v24});
    const float c0 = float_at(rt, v30), c1 = float_at(rt, v30 + 4u), c2 = float_at(rt, v30 + 8u);
    put_float(rt, va8, c0);
    put_float(rt, va8 + 4u, c1 + 5.0f);
    put_float(rt, va8 + 8u, c2);
    put_float(rt, v0c, float_at(rt, v6c));
    put_float(rt, v0c + 4u, float_at(rt, v6c + 4u));
    put_float(rt, v0c + 8u, float_at(rt, v6c + 8u));
    call_setf(rt, ctx, 0x08B4A5E8u, {v0c}, {1.0f});
    put_float(rt, v18, float_at(rt, v90));
    put_float(rt, v18 + 4u, float_at(rt, v90 + 4u));
    put_float(rt, v18 + 8u, float_at(rt, v90 + 8u));
    call_set(rt, ctx, 0x08B4AA34u, {v18, v18, v0c});
    const float d0 = float_at(rt, v18), d1 = float_at(rt, v18 + 4u), d2 = float_at(rt, v18 + 8u);
    put_float(rt, vb4, d0);
    put_float(rt, vb4 + 4u, d1 + 5.0f);
    put_float(rt, vb4 + 8u, d2);
    put(rt, frame.at(0xF0u), 0u);
    put(rt, frame.at(0xF4u), 0xFFFFFFFFu);
    const u32 first_handle = call_set(rt, ctx, 0x088DB5C4u, {word(rt, p)});
    call_set(rt, ctx, 0x088FAB84u, {first_handle, vc0, va8});
    put_float(rt, va8, float_at(rt, vc0));
    put_float(rt, va8 + 4u, float_at(rt, vc0 + 4u));
    put_float(rt, va8 + 8u, float_at(rt, vc0 + 8u));
    const u32 second_handle = call_set(rt, ctx, 0x088DB5C4u, {word(rt, p)});
    call_set(rt, ctx, 0x088FAB84u, {second_handle, vc0, vb4});
    const float e0 = float_at(rt, vc0), e1 = float_at(rt, vc0 + 4u), e2 = float_at(rt, vc0 + 8u);
    put_float(rt, v00 + 8u, e2);
    put_float(rt, vb4 + 8u, e2);
    put_float(rt, v00, e0);
    put_float(rt, v00 + 4u, e1);
    put_float(rt, vb4, e0);
    put_float(rt, vb4 + 4u, e1);
    call_set(rt, ctx, 0x08B4AA68u, {v00, v00, va8});
    put_float(rt, v6c, float_at(rt, v00));
    put_float(rt, v6c + 4u, float_at(rt, v00 + 4u));
    put_float(rt, v6c + 8u, float_at(rt, v00 + 8u));
    call_set(rt, ctx, 0x08B4A720u, {v6c});
    const float length = std::sqrt(ctx.fpr[0]);
    call_setf(rt, ctx, 0x08B4A5E8u, {v6c}, {1.0f / length});
    call_set(rt, ctx, 0x08B4A9D0u, {v60, v84, v6c});
    const float gx = float_at(rt, v60), gy = float_at(rt, v60 + 4u), gz = float_at(rt, v60 + 8u);
    put_float(rt, frame.at(0x7Cu), gy);
    put_float(rt, frame.at(0x80u), gz);
    call_setf(rt, ctx, 0x08B66FBCu, {v60}, {std::sqrt(psp_mul(gx, gx) + psp_mul(gz, gz)), gy});
    put_float(rt, out + 8u, ctx.fpr[0]);
    put_float(rt, self + 8u, float_at(rt, v90 + 8u));
    put_float(rt, self, float_at(rt, v90));
    put_float(rt, self + 4u, float_at(rt, v90 + 4u));
}

// 0x089B7574 (a0 = the object, a1 = a text): 0x08A36C3C builds a text token at frame +8 (an assert of 0x08CA5910 when it is
// empty), 0x08A34DA8 stores it under 0x08CA5918 and the token is released. With bit 2 of the object's +0x60 and a holder at
// +0x12C: two 0x08A76AA0 copies at frame +0xC / +0x10 and a 0x08A76AAC one at +4 built from (frame +0, text), the +0x10 copy
// assigned from the +4 one and both released, then the holder's vtable +0xCC virtual with (holder, frame +0xC, 1, frame +0x10)
// and both released. 0x0896A35C fills frame +0x18, 0x089B7378 the object's +0x154, 0x0896A9C4 (+0x124, then +0x120) the frame
// pair at +0x14 / +0x18 with 0x0896A4E0 between them. 0x089B5E10's answer, when non-null, is turned by 0x089BD4DC / 0x08969B98
// into the frame quad +0x24..+0x30: the object's +0x164 / +0x160 take (y + w) - w/20 and w/20 + y, 0x0896A35C / 0x0896A9C4 (of
// the negated z at +0x38) then 0x08969B98 (of +0x120). The advance (w - w/20 * 2) compared with +0x154 either latches the state
// at +0x168..+0x174 and byte +0x159 = 1 (v0 = 1) or clamps the frame +0x18 and runs 0x08969D2C. With +0x128 set, 0x0896A35C and
// 0x08B70F28 / 0x08B71328 (2.5) / 0x08B712B0 (2.9375) / 0x08B71EC8 give a value that 0x0896A9C4 takes with (frame +0x20) + 8.0
// at +0x34..+0x40. Finally byte +0x158 = 1, +0x15C = +0x138, 0x08967538 (+0x120, 0), 0x089AB114, 0x089C4558(1) and
// 0x089B601C(object, a1).

} // namespace

#define NEW1019(address, name) GT_DECOMP(address, gt::game::new_batch_main_1019::name, gt::ReturnKind::Word)
NEW1019(0x088D390Cu, func_088D390C);

#undef NEW1019

} // namespace new_batch_main_1019
} // namespace gt::game
