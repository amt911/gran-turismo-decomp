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
} // namespace
} // namespace new_batch_main_47
} // namespace gt::game

#define MAIN47(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_47::name, gt::ReturnKind::kind)
MAIN47(0x088638B0u, func_088638B0, Word);
MAIN47(0x088955D4u, func_088955D4, Word);
