// New batch main 1928 (draft only): functions drafted before that mismatched the original (or were skipped twice), written
// by hand from `tools.disasm` as structured C (named locals, `G[n]` only at the call boundary). Every call passes the
// argument registers the original sets before the jal, the delay slot runs before the call, v0 is what the original leaves
// on every exit path, and callee-saved registers come back as they went in. Input shapes live in
// shapes_main1928.txt (config/synth_shapes format): one line per function that dereferences an argument. In 0x08917300
// the two `lw $s0, 0xC($sp)` / `lw $s0, 4($sp)` are values a callee stored on the frame (the holder and the replaced
// node), which the body reads back through frame.at(), not unmodelled register reloads.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1928 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 / v1 / f0 of the callee are what the next instruction sees, as after the original's `jal`.
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with a0-a3 / t0-t3 set like the original sets them before the jal (a callee can read a register its
// prototype does not name).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
inline float call_float(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                        std::initializer_list<float> float_arguments = {}) {
    return call_keep(rt, ctx, address, arguments, float_arguments).f0;
}

// The callee-saved registers a body keeps values in; the caller's values come back on scope exit (the harness compares them).
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx) {
        for (const unsigned reg : registers) {
            indexes_[count_] = reg;
            values_[count_] = ctx.gpr[reg];
            ++count_;
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < count_; ++i) ctx_.gpr[indexes_[i]] = values_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned indexes_[8]{};
    u32 values_[8]{};
    unsigned count_{};
};

// The original copies a stack record with `lw`/`sw` in 4-word blocks and a 3-word tail: the bytes are the same as a
// straight word copy (its blocks and its tail never overlap the source).
inline void copy_words(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    for (u32 at = 0u; at < bytes; at += 4u) put(rt, destination + at, word(rt, source + at));
}

// 0x08B4D994 (a0 = out 4x4, a1 = A, a2 = B): out = A(transposed) * B over the top 3x3, row 3 copied from A. Every product
// is a `mul.s` (psp_mul keeps the PSP's inf*0 NaN and no FMA contraction reorders it) and every sum is the original's
// left-to-right `add.s`. v0 ends as the raw word of A[12]: the copy loop's last `lw v0, 0x30($sp)` loads it from the
// stack buffer, which holds exactly that loaded float.
void func_08B4D994(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 out = ctx.gpr[4];
    const u32 a = ctx.gpr[5];
    const u32 b = ctx.gpr[6];
    const float a0 = float_at(rt, a + 0x00u);
    const float a1 = float_at(rt, a + 0x04u);
    const float a2 = float_at(rt, a + 0x08u);
    const float a3 = float_at(rt, a + 0x0Cu);
    const float a4 = float_at(rt, a + 0x10u);
    const float a5 = float_at(rt, a + 0x14u);
    const float a6 = float_at(rt, a + 0x18u);
    const float a7 = float_at(rt, a + 0x1Cu);
    const float a8 = float_at(rt, a + 0x20u);
    const float a9 = float_at(rt, a + 0x24u);
    const float a10 = float_at(rt, a + 0x28u);
    const float a11 = float_at(rt, a + 0x2Cu);
    const float a12 = float_at(rt, a + 0x30u);
    const float a13 = float_at(rt, a + 0x34u);
    const float a14 = float_at(rt, a + 0x38u);
    const float a15 = float_at(rt, a + 0x3Cu);
    const float b0 = float_at(rt, b + 0x00u);
    const float b1 = float_at(rt, b + 0x04u);
    const float b2 = float_at(rt, b + 0x08u);
    const float b4 = float_at(rt, b + 0x10u);
    const float b5 = float_at(rt, b + 0x14u);
    const float b6 = float_at(rt, b + 0x18u);
    const float b8 = float_at(rt, b + 0x20u);
    const float b9 = float_at(rt, b + 0x24u);
    const float b10 = float_at(rt, b + 0x28u);
    // Column 0: (A0*B0 + A4*B1) + A8*B2, and its three row siblings.
    const float o0 = psp_mul(a0, b0) + psp_mul(a4, b1) + psp_mul(a8, b2);
    const float o1 = psp_mul(a1, b0) + psp_mul(a5, b1) + psp_mul(a9, b2);
    const float o2 = psp_mul(a2, b0) + psp_mul(a6, b1) + psp_mul(a10, b2);
    const float o3 = psp_mul(a3, b0) + psp_mul(a7, b1) + psp_mul(a11, b2);
    // Column 1.
    const float o4 = psp_mul(a0, b4) + psp_mul(a4, b5) + psp_mul(a8, b6);
    const float o5 = psp_mul(a1, b4) + psp_mul(a5, b5) + psp_mul(a9, b6);
    const float o6 = psp_mul(a2, b4) + psp_mul(a6, b5) + psp_mul(a10, b6);
    const float o7 = psp_mul(a3, b4) + psp_mul(a7, b5) + psp_mul(a11, b6);
    // Column 2.
    const float o8 = psp_mul(a0, b8) + psp_mul(a4, b9) + psp_mul(a8, b10);
    const float o9 = psp_mul(a1, b8) + psp_mul(a5, b9) + psp_mul(a9, b10);
    const float o10 = psp_mul(a2, b8) + psp_mul(a6, b9) + psp_mul(a10, b10);
    const float o11 = psp_mul(a3, b8) + psp_mul(a7, b9) + psp_mul(a11, b10);
    put_float(rt, out + 0x00u, o0);
    put_float(rt, out + 0x04u, o1);
    put_float(rt, out + 0x08u, o2);
    put_float(rt, out + 0x0Cu, o3);
    put_float(rt, out + 0x10u, o4);
    put_float(rt, out + 0x14u, o5);
    put_float(rt, out + 0x18u, o6);
    put_float(rt, out + 0x1Cu, o7);
    put_float(rt, out + 0x20u, o8);
    put_float(rt, out + 0x24u, o9);
    put_float(rt, out + 0x28u, o10);
    put_float(rt, out + 0x2Cu, o11);
    put_float(rt, out + 0x30u, a12);
    put_float(rt, out + 0x34u, a13);
    put_float(rt, out + 0x38u, a14);
    put_float(rt, out + 0x3Cu, a15);
    ctx.gpr[2] = std::bit_cast<u32>(a12);
}

// 0x0887C0A8 (a0 = state, a1 = owner): two 0..1 lookups from owner+0x60's +0xF0 table become the floats at +8 / +0xC;
// then owner+0x60's +0xF4 word is diffed against the state's old +0x18 (changed bits at +0x20, shared at +0x1C), the
// per-bit counters at +0x34..+0x40 are bumped, and two deltas (+0x40-+0x38 and +0x3C-+0x34) are squared, clamped to
// 0.25 and signed into the floats at +0x10 / +0x14. v0 ends as the second delta (the last `mflo`).

// 0x0897B364 (a0 = object, a1 = a pointer stored at +0x14): constructor — two vtables, a self-linked pair at +0x18, two
// 0x58-byte tables copied from .rodata into +0x94 and +0xEC, the descriptor fields at +0x14A..+0x158, four one-argument
// initialisers at +0x19C..+0x1A8 and a block of zeroed/1.0f fields. v0 ends as the 0xFFFFFFFF stored at +0x17C.

// 0x08AB8104 (a0 = table object with its count at +0x1480, a1 = key, a2 = 32-byte record, a3 = 4-byte tag): a table of
// 32 entries of 0xA4 bytes; a match for `key` (0x08B35FD8) gets the record at +0x80 and the tag at +0xA0, otherwise a new
// entry is copied in (0x08B7C830 fills its first 0x80 bytes) and the count grows. v0 = 1 when something was written, 0
// when the table is full.
void func_08AB8104(Runtime &rt, AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 record = ctx.gpr[6];
    const u32 tag = ctx.gpr[7];
    SavedRegisters saved(ctx, {16, 17, 18, 19, 20, 21, 22, 23});
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[20] = base;
    ctx.gpr[22] = key;
    ctx.gpr[23] = tag;
    ctx.gpr[16] = record;
    const s32 count = static_cast<s32>(word(rt, base + 0x1480u));
    if (count >= 0x20) {
        ctx.gpr[2] = 0u;
        return;
    }
    s32 found = -1;
    for (s32 i = 0; i < count; ++i) {
        const u32 offset = static_cast<u32>(i) * 0xA4u;
        ctx.gpr[17] = base + offset;
        ctx.gpr[19] = static_cast<u32>(i);
        ctx.gpr[21] = offset;
        if (call_set(rt, ctx, 0x08B35FD8u, {ctx.gpr[17], key}) == 0u) {
            found = i;
            break;
        }
    }
    u32 slot;
    if (found >= 0) {
        slot = base + static_cast<u32>(found) * 0xA4u;
    } else {
        call_set(rt, ctx, 0x08B7C830u, {base + static_cast<u32>(count) * 0xA4u, key, 0x80u});
        const u32 current = word(rt, base + 0x1480u);
        slot = base + current * 0xA4u;
        put(rt, base + 0x1480u, current + 1u);
    }
    copy_words(rt, slot + 0x80u, record, 0x20u);
    put(rt, slot + 0xA0u, word(rt, tag));
    ctx.gpr[2] = 1u;
}

// 0x08AFC55C (a0..a3 = four words handed to the builders): builds two 0x3C-byte stack records (0x08B37640 / 0x08B0CE58 /
// 0x08B35F60 then 0x08B0F6F0, and 0x08B0EF1C with 12 * (a1 * a2)) and copies each into the block at sp+0x80; v0 = 1 when
// that block ends as zero, 0 otherwise (and 0 as soon as the first record leaves it non-zero).
void func_08AFC55C(Runtime &rt, AllegrexContext &ctx) {
    const u32 s6 = ctx.gpr[4];
    const u32 s5 = ctx.gpr[5];
    const u32 s4 = ctx.gpr[6];
    const u32 s3 = ctx.gpr[7];
    SavedRegisters saved(ctx, {16, 17, 18, 19, 20, 21, 22});
    GuestFrame frame(ctx, 0x360u);
    ctx.gpr[22] = s6;
    ctx.gpr[21] = s5;
    ctx.gpr[20] = s4;
    ctx.gpr[19] = s3;
    ctx.gpr[18] = frame.at(0x80u);
    ctx.gpr[17] = frame.at(0x2C0u);
    ctx.gpr[16] = frame.at(0xC0u);
    call_set(rt, ctx, 0x08B37640u, {frame.at(0xC0u), 0x08CB4AC4u, s5, s4, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10],
                                    ctx.gpr[11]});
    call_set(rt, ctx, 0x08B0CE58u, {frame.at(0x80u)});
    const u32 answer = call_set(rt, ctx, 0x08B35F60u, {frame.at(0xC0u)});
    call_set(rt, ctx, 0x08B0F6F0u, {frame.at(0x2C0u), s3, frame.at(0xC0u), answer});
    copy_words(rt, frame.at(0x40u), frame.at(0x2C0u), 0x30u + 0xCu);
    copy_words(rt, frame.at(0x80u), frame.at(0x40u), 0x30u + 0xCu);
    if (word(rt, frame.at(0x80u)) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 product = static_cast<u32>(s5) * static_cast<u32>(s4);
    const u32 scaled = (product << 4u) - (product << 2u);
    call_set(rt, ctx, 0x08B0EF1Cu, {frame.at(0x300u), s3, s6, scaled});
    copy_words(rt, frame.at(0x00u), frame.at(0x300u), 0x30u + 0xCu);
    copy_words(rt, frame.at(0x80u), frame.at(0x00u), 0x30u + 0xCu);
    ctx.gpr[2] = word(rt, frame.at(0x80u)) == 0u ? 1u : 0u;
}

// 0x08C45270: the static-initialiser chain — five guarded one-time registrations (0x08B6441C asks, 0x08B64494 registers)
// fill the .data records at 0x08D7728C..0x08D77324 with their vtable, back pointer and zero, in the original's nesting.
// Every path returns 0x08D77314.
void func_08C45270(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const auto guard = [&](u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); };
    if (guard(0x08D74F58u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74F58u}) != 0u) {
        if (guard(0x08D74F98u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74F98u}) != 0u) {
            if (guard(0x08D74FA8u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74FA8u}) != 0u) {
                if (guard(0x08D74FC8u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74FC8u}) != 0u) {
                    put(rt, 0x08D7729Cu + 8u, 0x08CD7988u);
                    put(rt, 0x08D7729Cu + 4u, 0x08D772A8u);
                    put(rt, 0x08D7729Cu, 0u);
                    call_set(rt, ctx, 0x08B64494u, {0x08D74FC8u});
                }
                put(rt, 0x08D772B8u, 0x08D7729Cu);
                call_set(rt, ctx, 0x08B64494u, {0x08D74FA8u});
            }
            if (guard(0x08D74FA0u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74FA0u}) != 0u) {
                put(rt, 0x08D772ACu + 8u, 0x08CD7988u);
                put(rt, 0x08D772ACu + 4u, 0x08D772B8u);
                put(rt, 0x08D772ACu, 0u);
                call_set(rt, ctx, 0x08B64494u, {0x08D74FA0u});
            }
            put(rt, 0x08D772E4u, 0x08D772ACu);
            call_set(rt, ctx, 0x08B64494u, {0x08D74F98u});
        }
        if (guard(0x08D74F90u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74F90u}) != 0u) {
            put(rt, 0x08D772D8u + 8u, 0x08CD7994u);
            put(rt, 0x08D772D8u + 4u, 0x08D772E4u);
            put(rt, 0x08D772D8u, 0u);
            call_set(rt, ctx, 0x08B64494u, {0x08D74F90u});
        }
        put(rt, 0x08D77320u, 0x08D772D8u);
        if (guard(0x08D74FC0u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74FC0u}) != 0u) {
            put(rt, 0x08D7728Cu + 8u, 0x08CD7988u);
            put(rt, 0x08D7728Cu + 4u, 0x08D77298u);
            put(rt, 0x08D7728Cu, 0u);
            call_set(rt, ctx, 0x08B64494u, {0x08D74FC0u});
        }
        put(rt, 0x08D77320u + 4u, 0x08D7728Cu);
        call_set(rt, ctx, 0x08B64494u, {0x08D74F58u});
    }
    if (guard(0x08D74F50u) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08D74F50u}) != 0u) {
        put(rt, 0x08D77314u + 8u, 0x08CD79A0u);
        put(rt, 0x08D77314u + 4u, 0x08D77320u);
        put(rt, 0x08D77314u, 0u);
        call_set(rt, ctx, 0x08B64494u, {0x08D74F50u});
    }
    ctx.gpr[2] = 0x08D77314u;
}

// 0x088865C8 (a0 = object): constructor — a vtable at +0x50 over a 0xEC block, a second vtable at +0, a 0x10 node handed to
// 0x08B8E380, the 3.0f / 1.0f / 1/60 fields, a 0x20-word zero block at +0x198, sixteen 6-byte records at +0x2B8, three
// container initialisers and four zeroed words. v0 ends as 2 (the last clear loop's counter).

// The atomic refcount decrement the original does with `ll`/`sc` (single-threaded here: the `sc` always lands): returns
// the value before the decrement.
inline u32 release_count(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}

// 0x08917300 (a0 = container, a1 = count): fills the container's array (word at +0) with `count` pairs — an object from
// 08916A50(alloc(0x18)) and a 0x10 node (vtable 0x08CC5890, counts 1/1, +0xC = the object) assigned through 08B8E380 —
// chains each new object to the previous at +0, and releases the node it replaced (count at +4, virtuals at +8/+0xC when
// it reaches 1). v0 ends as &array[count-1], or the last call's answer when count <= 0.

} // namespace
} // namespace new_batch_main_1928
} // namespace gt::game

#define NEW1928(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1928::name, gt::ReturnKind::kind)
NEW1928(0x08B4D994u, func_08B4D994, Word);
NEW1928(0x08AB8104u, func_08AB8104, Word);
NEW1928(0x08AFC55Cu, func_08AFC55C, Word);
NEW1928(0x08C45270u, func_08C45270, Word);
