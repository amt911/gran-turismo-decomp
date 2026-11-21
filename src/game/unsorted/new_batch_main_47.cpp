// Main-session batch 47, draft: three of the ten large functions (297-303 instructions) the extra seeds made emittable,
// written by hand as structured C from `tools.disasm`. They are not a family: a reference-counted reset, a field-by-field
// sync driven by a difference mask, and a 4x4 matrix inverse.
// Not drafted: 0x0894106C (already decompiled in hot_batch_117.cpp, unregistered there because the full gate compared 0),
// 0x08BC9A34 (refcount/lock dance with four jalr destructor paths), 0x08A24584 (four repeated key/value insert blocks),
// 0x08C366E8 (tree/vector insertion with asserts), 0x08B0BCBC and 0x08A076B4 (0x520/0x7E0 frames with lazy global init)
// and 0x0894AE54 (a four-element force loop) — each needs more care than a draft without a gate can carry.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_47 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The callee-saved registers the big bodies use: s0-s7, fp and f20-f31.
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

// A shared_ptr-style release: the strong count at +4 goes down by one and, when it was the last, the object's virtual at
// +8 runs; then the weak count at +8 goes down and its virtual at +0xC runs when that was the last.
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    const u32 strong = word(rt, object + 4u) - 1u;
    put(rt, object + 4u, strong);
    if (strong != 0u) return;
    ctx.gpr[2] = call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    const u32 weak = word(rt, object + 8u) - 1u;
    put(rt, object + 8u, weak);
    if (weak != 0u) return;
    ctx.gpr[2] = call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// The matching assignment: a new object gets a reference, the old one is released, then the slot takes the new pointer.
void assign_object(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 object) {
    if (object == word(rt, slot)) return;
    if (object != 0u) put(rt, object + 4u, word(rt, object + 4u) + 1u);
    release_object(rt, ctx, word(rt, slot));
    put(rt, slot, object);
}

// 0x088638B0 (a0 = self): resets a set of members — the shared_ptr at +0x44E0 is released, the one at +0x44DC is cleared,
// six more at +0x44E4..+0x44F8 are assigned the null temporaries the compiler zeroes on the stack, the vector at +0x44FC
// is cleared through 0x08B90024 with an empty {0, 0} range, and 0x0896275C finishes on self + 0x44CC.
void func_088638B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 base = self + 0x44DCu;
    SavedAll saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    // The stack temporaries the original zeroes in its prologue; the values the body reads are those zeroes.
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    put(rt, frame.at(0x24u), 0u);
    put(rt, frame.at(0x28u), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, base, 0u);
    release_object(rt, ctx, word(rt, self + 0x44E0u));
    put(rt, self + 0x44E0u, 0u);
    put(rt, base + 8u, word(rt, frame.at(0xCu)));
    assign_object(rt, ctx, base + 0xCu, word(rt, frame.at(0x10u)));
    put(rt, base + 0x10u, word(rt, frame.at(0x14u)));
    assign_object(rt, ctx, base + 0x14u, word(rt, frame.at(0x18u)));
    put(rt, base + 0x18u, word(rt, frame.at(0x1Cu)));
    assign_object(rt, ctx, base + 0x1Cu, word(rt, frame.at(0x20u)));
    call(rt, ctx, 0x08B90024u, {base + 0x20u, frame.at(0x24u)});
    call(rt, ctx, 0x08B8FE44u, {word(rt, frame.at(0x24u)), word(rt, frame.at(0x28u)), frame.at(0u)});
    const u32 buffer = word(rt, frame.at(0x24u));
    if (buffer != 0u) call(rt, ctx, 0x08B35254u, {buffer});
    release_object(rt, ctx, word(rt, frame.at(0x20u)));
    release_object(rt, ctx, word(rt, frame.at(0x18u)));
    release_object(rt, ctx, word(rt, frame.at(0x10u)));
    release_object(rt, ctx, word(rt, frame.at(8u)));
    ctx.gpr[2] = call(rt, ctx, 0x0896275Cu, {self + 0x44CCu});
}

// The tail both paths of 0x088955D4 share (0x088959B8): with `refresh`, the size bookkeeping at +0xC and +0x24 is
// rewritten first; then the counters at +8/+0x24 step, the state at *(+0x1C) is copied twice (from itself and from the
// source), and 0x08894EC4 runs when the grown size still fits. v0 = 1 (the original returns its a1 flag, set to 1 here).
void sync_tail(Runtime &rt, AllegrexContext &ctx, u32 self, u32 source, u32 holder, u32 pending, bool refresh) {
    if (refresh) {
        put(rt, self + 0x24u, 0u);
        put(rt, self + 0xCu, word(rt, self + 0x2Cu) - word(rt, self + 0x28u) + 0x21u);
    }
    const u32 counter = word(rt, self + 0x24u) + 1u;
    put(rt, self + 8u, pending + 1u);
    put(rt, self + 0x24u, counter);
    const u32 object = word(rt, holder);
    call(rt, ctx, 0x0892B4B0u, {object + 0x20u, object});
    call(rt, ctx, 0x0892B4B0u, {object, source});
    const u32 size = word(rt, self + 0x2Cu) - word(rt, self + 0x28u) + 0x41u;
    if (!(size < word(rt, self + 4u))) {
        put(rt, self + 0x10u, word(rt, self + 8u));
        call(rt, ctx, 0x08894EC4u, {self});
    }
    ctx.gpr[2] = 1u;
}

// 0x088955D4 (a0 = object, a1 = the struct to sync from): returns 0 when the object's byte at +0x18 is set, else 1.
// It copies the object's state (+0x1C -> a local 0x18-byte struct), clamps the halfword at +0x40 of the copied state into
// [-0x1000, 0x1000], then compares the local struct with a1 field by field and records each difference as a bit of a mask;
// every set bit drives one setter (0x08BABC90 for unsigned bytes, 0x08BABCA0 for signed bytes, 0x08BABCB0 for halfwords).
// Bit 0x4000 means the byte array at +0x16 differs: its count is re-set through 0x08894B70 and every byte re-set.
void func_088955D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedAll saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 local = frame.at(0u);
    if (rt.memory().load8(self + 0x18u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 state = word(rt, self + 0x1Cu);
    const u32 holder = self + 0x1Cu;
    call(rt, ctx, 0x0892B4B0u, {local, state});
    const s32 raw = static_cast<std::int16_t>(rt.memory().load16(state + 4u));
    put(rt, state + 0x40u, static_cast<u32>(raw));
    if (!(raw < 0x10 && raw > -0x10)) {
        const s32 doubled = raw * 2 - static_cast<std::int16_t>(rt.memory().load16(state + 0x24u));
        put(rt, state + 0x40u, static_cast<u32>(doubled < 0x1001 ? (doubled < -0x1000 ? -0x1000 : doubled) : 0x1000));
    }
    const u32 clamped = static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(state + 0x40u)));
    rt.memory().store16(local + 4u, static_cast<std::uint16_t>(clamped));
    u32 mask = 0x800u;
    if ((rt.memory().load8(local) ^ rt.memory().load8(source)) == 0u) mask = 0u;
    if ((rt.memory().load8(local + 1u) ^ rt.memory().load8(source + 1u)) != 0u) mask |= 0x1000u;
    if ((rt.memory().load8(local + 2u) ^ rt.memory().load8(source + 2u)) != 0u) mask |= 0x8u;
    if ((clamped ^ static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(source + 4u)))) != 0u) mask |= 0x2u;
    if ((rt.memory().load8(local + 6u) ^ rt.memory().load8(source + 6u)) != 0u) mask |= 0x10u;
    if ((rt.memory().load8(local + 7u) ^ rt.memory().load8(source + 7u)) != 0u) mask |= 0x20u;
    if ((rt.memory().load16(local + 8u) ^ rt.memory().load16(source + 8u)) != 0u) mask |= 0x1u;
    if ((rt.memory().load16(local + 0xAu) ^ rt.memory().load16(source + 0xAu)) != 0u) mask |= 0x4u;
    if ((rt.memory().load16(local + 0xCu) ^ rt.memory().load16(source + 0xCu)) != 0u) mask |= 0x80u;
    if ((rt.memory().load16(local + 0xEu) ^ rt.memory().load16(source + 0xEu)) != 0u) mask |= 0x40u;
    if ((rt.memory().load8(local + 0x14u) ^ rt.memory().load8(source + 0x14u)) != 0u) mask |= 0x2000u;
    if ((rt.memory().load16(local + 0x10u) ^ rt.memory().load16(source + 0x10u)) != 0u) mask |= 0x100u;
    if ((rt.memory().load8(local + 0x12u) ^ rt.memory().load8(source + 0x12u)) != 0u) mask |= 0x200u;
    if ((rt.memory().load8(local + 0x13u) ^ rt.memory().load8(source + 0x13u)) != 0u) mask |= 0x400u;
    const s32 count = static_cast<std::int8_t>(rt.memory().load8(local + 3u));
    if (count != static_cast<std::int8_t>(rt.memory().load8(source + 3u))) {
        mask |= 0x4000u;
    } else if (count > 0) {
        bool differs = rt.memory().load8(local + 0x16u) != rt.memory().load8(source + 0x16u);
        for (s32 i = 1; i < count && !differs; ++i) {
            differs = rt.memory().load8(local + 0x16u + static_cast<u32>(i)) !=
                      rt.memory().load8(source + 0x16u + static_cast<u32>(i));
        }
        if (differs) mask |= 0x4000u;
    }
    const u32 pending = word(rt, self + 8u);
    if (pending != 0u && mask == 0u) {
        sync_tail(rt, ctx, self, source, holder, pending, false);
        return;
    }
    if (static_cast<s32>(word(rt, self + 0x24u)) > 0) call(rt, ctx, 0x08894B70u, {self, word(rt, self + 0x24u)});
    call(rt, ctx, 0x08894B70u, {self, mask + 1u});
    if ((mask & 0x800u) != 0u)
        call(rt, ctx, 0x08BABC90u, {self, rt.memory().load8(local), rt.memory().load8(source)});
    if ((mask & 0x1000u) != 0u)
        call(rt, ctx, 0x08BABC90u, {self, rt.memory().load8(local + 1u), rt.memory().load8(source + 1u)});
    if ((mask & 0x8u) != 0u)
        call(rt, ctx, 0x08BABCA0u, {self, static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(local + 2u)))),
                                    static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(source + 2u))))});
    if ((mask & 0x2u) != 0u)
        call(rt, ctx, 0x08BABCB0u, {self, static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(local + 4u))),
                                    static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(source + 4u)))});
    if ((mask & 0x10u) != 0u)
        call(rt, ctx, 0x08BABCA0u, {self, static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(local + 6u)))),
                                    static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(source + 6u))))});
    if ((mask & 0x20u) != 0u)
        call(rt, ctx, 0x08BABC90u, {self, rt.memory().load8(local + 7u), rt.memory().load8(source + 7u)});
    if ((mask & 0x1u) != 0u)
        call(rt, ctx, 0x08BABCB0u, {self, static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(local + 8u))),
                                    static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(source + 8u)))});
    if ((mask & 0x4u) != 0u)
        call(rt, ctx, 0x08BABCB0u, {self, static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(local + 0xAu))),
                                    static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(source + 0xAu)))});
    if ((mask & 0x80u) != 0u)
        call(rt, ctx, 0x08BABCB0u, {self, static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(local + 0xCu))),
                                    static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(source + 0xCu)))});
    if ((mask & 0x40u) != 0u)
        call(rt, ctx, 0x08BABCB0u, {self, static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(local + 0xEu))),
                                    static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(source + 0xEu)))});
    if ((mask & 0x2000u) != 0u)
        call(rt, ctx, 0x08BABC90u, {self, rt.memory().load8(local + 0x14u), rt.memory().load8(source + 0x14u)});
    if ((mask & 0x100u) != 0u)
        call(rt, ctx, 0x08BABCB0u, {self, static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(local + 0x10u))),
                                    static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(source + 0x10u)))});
    if ((mask & 0x200u) != 0u)
        call(rt, ctx, 0x08BABCA0u, {self, static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(local + 0x12u)))),
                                    static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(source + 0x12u))))});
    if ((mask & 0x400u) != 0u)
        call(rt, ctx, 0x08BABCA0u, {self, static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(local + 0x13u)))),
                                    static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(source + 0x13u))))});
    if ((mask & 0x4000u) != 0u) {
        const s32 bytes = static_cast<std::int8_t>(rt.memory().load8(source + 3u));
        call(rt, ctx, 0x08894B70u, {self, static_cast<u32>(bytes + 1)});
        for (s32 i = 0; i < bytes; ++i) {
            call(rt, ctx, 0x08BABCA0u,
                 {self, static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(local + 0x16u + static_cast<u32>(i))))),
                  static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(source + 0x16u + static_cast<u32>(i))))) });
        }
    }
    sync_tail(rt, ctx, self, source, holder, word(rt, self + 8u), true);
}

// 0x08B50EFC: the inverse of a 4x4 matrix given as 16 floats — f12-f19 are rows 0 and 1, the eight stack arguments at
// the caller's sp+0x20..0x3C (this frame's +0xA0..+0xBC) are rows 2 and 3 — written to the 16 pointers a0-a3, t0-t3 and
// the eight stack words at this frame's +0x80..+0x9C; f0 = the determinant. The body is one straight-line cofactor
// expansion, so its locals are named after the FPU registers it moves through: the order of the float operations is the
// identity of the function and has to be read against `tools.disasm`. The registers it spills to its frame are locals.
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
    ctx.fpr[0] = det;
}
} // namespace
} // namespace new_batch_main_47
} // namespace gt::game

#define MAIN47(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_47::name, gt::ReturnKind::kind)
MAIN47(0x088638B0u, func_088638B0, Word);
MAIN47(0x088955D4u, func_088955D4, Word);
MAIN47(0x08B50EFCu, func_08B50EFC, Float);
