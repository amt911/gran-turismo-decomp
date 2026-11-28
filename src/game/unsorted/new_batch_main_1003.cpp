// New batch (main session 1003): functions the extra seeds made emittable, written as structured C from
// `tools.disasm`. Draft only: the differential gate decides which of them land. Every body keeps the
// original's exits (`v0` on each path), the call order, and the memory it writes; arguments go in a0-a3 /
// t0-t3, float arguments in f12-f15. 0x08B69EBC (a soft-float 64-bit conversion helper) was skipped.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1003 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with float arguments ($f12, $f13, ...), keeping what the callee left in v1 / f0.
u32 call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
           std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// `lw v0, slot(v1); jalr v0`: the callee starts with v0 = its own address (an empty virtual returns it).
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// `lw v1, slot(v0); jalr v1`: v0 still holds the vtable word when the callee starts.
u32 call_keeping_v0(Runtime &rt, AllegrexContext &ctx, u32 v0, u32 target,
                    std::initializer_list<u32> arguments) {
    ctx.gpr[2] = v0;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Callee-saved registers a body keeps its arguments in and puts back.
struct SavedS0 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS0(AllegrexContext &c) : ctx(c), value(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = value; }
};
struct SavedS1 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS1(AllegrexContext &c) : ctx(c), value(c.gpr[17]) {}
    ~SavedS1() { ctx.gpr[17] = value; }
};
struct SavedS2 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS2(AllegrexContext &c) : ctx(c), value(c.gpr[18]) {}
    ~SavedS2() { ctx.gpr[18] = value; }
};
struct SavedS3 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS3(AllegrexContext &c) : ctx(c), value(c.gpr[19]) {}
    ~SavedS3() { ctx.gpr[19] = value; }
};
struct SavedS4 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS4(AllegrexContext &c) : ctx(c), value(c.gpr[20]) {}
    ~SavedS4() { ctx.gpr[20] = value; }
};
struct SavedS5 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS5(AllegrexContext &c) : ctx(c), value(c.gpr[21]) {}
    ~SavedS5() { ctx.gpr[21] = value; }
};
struct SavedS6 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS6(AllegrexContext &c) : ctx(c), value(c.gpr[22]) {}
    ~SavedS6() { ctx.gpr[22] = value; }
};
struct SavedS7 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS7(AllegrexContext &c) : ctx(c), value(c.gpr[23]) {}
    ~SavedS7() { ctx.gpr[23] = value; }
};
struct SavedFp {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedFp(AllegrexContext &c) : ctx(c), value(c.gpr[30]) {}
    ~SavedFp() { ctx.gpr[30] = value; }
};
struct SavedF20 {
    AllegrexContext &ctx;
    u32 bits;
    explicit SavedF20(AllegrexContext &c) : ctx(c), bits(c.fpr_bits(20)) {}
    ~SavedF20() { ctx.set_fpr_bits(20, bits); }
};

u32 load8(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put16(Runtime &rt, u32 address, u32 value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}

// The intrusive-reference release every one of these bodies ends a temporary with: word at value - 4 minus
// one (0x08B60804), and only when that hits zero the deallocator 0x08B8542C on the block value - 0xC (the
// empty-refcount sentinel 0x08D74FD0 is skipped).
void release_reference(Runtime &rt, AllegrexContext &ctx, u32 value, u32 scratch) {
    const u32 block = value - 0xCu;
    if (block == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {value - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {block, scratch});
}

// ---------------------------------------------------------------------------------------------------------
// 0x08A368A4 (a0 = unused, a1 = owner of the three sub-objects, a2 = mode, a3 = source record):
// picks the sub-objects of the source block by mode -- 2 only +4, 3 the +8 one, 4 the +0xC one, 5 the
// +0x10 one whose virtual at 0x54 gives the scale and then the +0xC one -- and reads, in the original's
// fall-through order, the virtual 0x30 of the +0xC object (a byte), 0x3C of the +8 object and 0xC of the
// +4 object (an output smart pointer in the frame), then scales both smart-pointer results through
// 0x08B855E0 into frame slots and hands them to 0x08A354EC / 0x08A3523C with the two sub-object values and
// the scale in f12; the temporaries are released and v0 is the last call's (or the mode word on the early
// exit: 2 below 4, else 5). a0 is ignored.
void func_08A368A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    SavedS4 saved4(ctx);
    SavedS5 saved5(ctx);
    SavedS6 saved6(ctx);
    SavedF20 saved_f20(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = frame.at(0u);
    constexpr u32 kEmpty = 0x08D74FD0u;
    put(rt, sp + 0x10u, kEmpty + 0xCu);
    put(rt, sp + 0x14u, kEmpty + 0xCu);

    u32 s0 = 0u;
    u32 s1 = 0u;
    u32 s5 = 0xFFFFFFFFu;
    u32 s6 = 0u;
    float f20 = 1.0f;
    u32 step = 0u;  // 3 = only the +0xC object, 2 = the +8 object, 1 = the +4 object, 0 = none
    if (mode == 2u) {
        step = 1u;
    } else if (mode == 3u) {
        s1 = 0u;
        step = 2u;
    } else if (mode == 4u) {
        s0 = word(rt, source + 0xCu);
        step = 3u;
    } else if (mode == 5u) {
        s0 = word(rt, source + 0x10u);
        if (s0 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEB8u});
        const u32 vtable = word(rt, s0);
        call_virtual(rt, ctx, word(rt, vtable + 0x54u), {s0});
        f20 = ctx.fpr[0];
        s0 = word(rt, source + 0xCu);
        step = 3u;
    } else {
        ctx.gpr[2] = static_cast<s32>(mode) < 4 ? 2u : 5u;  // the asm's `slti` is signed
    }

    if (step == 3u) {
        if (s0 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEB8u});
        const u32 vtable = word(rt, s0);
        s1 = call_virtual(rt, ctx, word(rt, vtable + 0x30u), {s0}) & 0xFFu;
        step = 2u;
    }
    if (step == 2u) {
        s0 = word(rt, source + 8u);
        if (s0 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEB8u});
        const u32 vtable = word(rt, s0);
        s6 = s1;
        s5 = call_virtual(rt, ctx, word(rt, vtable + 0x3Cu), {s0});
        step = 1u;
    }
    if (step == 1u) {
        s1 = word(rt, source + 4u);
        if (s1 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEB8u});
        const u32 vtable = word(rt, s1);
        call_keeping_v0(rt, ctx, vtable, word(rt, vtable + 0xCu), {sp + 0xCu, s1});
        call(rt, ctx, 0x08B855E0u, {sp + 0x14u, sp + 0xCu});
        release_reference(rt, ctx, word(rt, sp + 0xCu), sp + 3u);

        s1 = word(rt, source);
        if (s1 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEB8u});
        const u32 second_vtable = word(rt, s1);
        call_keeping_v0(rt, ctx, second_vtable, word(rt, second_vtable + 0xCu), {sp + 8u, s1});
        call(rt, ctx, 0x08B855E0u, {sp + 0x10u, sp + 8u});
        release_reference(rt, ctx, word(rt, sp + 8u), sp + 2u);

        call(rt, ctx, 0x08A354ECu, {sp + 4u, owner});
        const u32 first = word(rt, sp + 4u);
        if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACF04u});
        call_f(rt, ctx, 0x08A3523Cu, {first, word(rt, sp + 0x10u), word(rt, sp + 0x14u), s5, s6},
               {f20});
        call(rt, ctx, 0x08A5B9ECu, {sp + 4u});
        release_reference(rt, ctx, word(rt, sp + 0x14u), sp + 1u);
    }
    release_reference(rt, ctx, word(rt, sp + 0x10u), sp);
}

// One CRC-like pass over 0x800 bytes: state = table[byte ^ state >> 24] ^ state << 8 (0x08B12E88 / 0x08B12F44).
u32 crc_chain(Runtime &rt, u32 source, u32 state, u32 table) {
    for (u32 index = 0u; index != 0x800u; ++index) {
        state = word(rt, table + ((load8(rt, source + index) ^ (state >> 24)) << 2)) ^ (state << 8);
    }
    return state;
}

// ---------------------------------------------------------------------------------------------------------
// 0x08B12D50 (a0 = state, a1 = a bit index): takes the state's lock at +0x1E0, and unless the flag byte at
// +0x258 is clear, sets the index's bit in the byte table at +0x268 and, when a cache slot at +0x238 has to
// be rebuilt, reloads the 0x800-byte block through 0x08B11F30. Then 0x08B11F8C gives the record for the
// index (its base word summed with index*16 & 0x7FF), and 0x20 rounds of 0x08B33D3C read 0x800 bytes each
// into the buffer at +0x270, over which four CRC-like chains run (32-bit state, table[byte ^ state >> 24]
// ^ state << 8, tables at 0x08CB616C / 0x08CB6170 / 0x08CB6174 / 0x08CB6178). When the four words at the
// record match, the index's bit is set in the second table at +0x26C. v0 = whether the bit is set there.
void func_08B12D50(Runtime &rt, AllegrexContext &ctx) {
    const u32 state = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    SavedS4 saved4(ctx);
    SavedS5 saved5(ctx);
    SavedS6 saved6(ctx);
    SavedS7 saved7(ctx);
    SavedFp saved_fp(ctx);
    GuestFrame frame(ctx, 0x40u);

    call(rt, ctx, 0x08B344E0u, {state + 0x1E0u});  // lock
    u32 result = 0u;
    bool check = load8(rt, state + 0x258u) != 0u;
    const u32 byte_index = index >> 3;
    const u32 bit = (1u << (index & 7u)) & 0xFFu;
    if (check) {
        const u32 table = word(rt, state + 0x268u) + byte_index;
        if ((load8(rt, table) & bit) == 0u) {
            put8(rt, table, bit | load8(rt, table));
            const u32 shifted = index << 4;
            const u32 slot = shifted >> 11;
            if (slot != word(rt, state + 0x238u)) {
                put(rt, state + 0x238u, slot);
                call(rt, ctx, 0x08B344E0u, {state + 0x1B8u});  // lock
                call(rt, ctx, 0x08B11F30u,
                     {state, word(rt, state + 0x234u) + (slot << 11), word(rt, state + 0x23Cu), 0x800u});
                call(rt, ctx, 0x08B3457Cu, {state + 0x1B8u});
            }
            const u32 record = word(rt, state + 0x23Cu) + (shifted & 0x7FFu);
            const u32 handle = call(rt, ctx, 0x08B11F8Cu, {state, state + 0x12Cu, index << 16});
            const u32 buffer = word(rt, state + 0x270u);
            bool matched = true;
            if (static_cast<s32>(handle) < 0) {
                matched = false;
                call(rt, ctx, 0x08B11E3Cu, {state});
            } else {
                u32 third = 0u;
                u32 second = 0u;
                u32 first = 0u;
                u32 fourth = 0u;
                for (u32 round = 0x20u; round != 0u; --round) {
                    if (call(rt, ctx, 0x08B33D3Cu, {handle, buffer, 0x800u}) != 0x800u) {
                        matched = false;
                        call(rt, ctx, 0x08B33A24u, {handle});
                        call(rt, ctx, 0x08B11E3Cu, {state});
                        break;
                    }
                    const u32 source = word(rt, state + 0x270u);
                    third = crc_chain(rt, source, third, word(rt, 0x08CB616Cu));
                    second = crc_chain(rt, source, second, word(rt, 0x08CB6170u));
                    first = crc_chain(rt, source, first, word(rt, 0x08CB6174u));
                    fourth = crc_chain(rt, source, fourth, word(rt, 0x08CB6178u));
                }
                if (matched) {
                    call(rt, ctx, 0x08B33A24u, {handle});
                    matched = word(rt, record) == third && word(rt, record + 4u) == second &&
                              word(rt, record + 8u) == first && word(rt, record + 0xCu) == fourth;
                    if (matched) {
                        const u32 second_table = word(rt, state + 0x26Cu) + byte_index;
                        put8(rt, second_table, bit | load8(rt, second_table));
                    }
                }
            }

        }
    }
    if (check) {
        const u32 second_table = word(rt, state + 0x26Cu) + byte_index;
        result = ((bit & load8(rt, second_table)) != 0u) ? 1u : 0u;
    }
    call(rt, ctx, 0x08B3457Cu, {state + 0x1E0u});  // unlock
    ctx.gpr[2] = result;
}

// ---------------------------------------------------------------------------------------------------------
// 0x08AAADC8 (a0 = container, a1 = source, a2 / a3 passed through to 0x08AAA2C0): resets the container's
// stream and starts a new source in it, clears and rebuilds the +0x38 vector from the +0x34 count (one
// element at a time through 0x08B88F44 / 0x08C312EC), walks the +0x2C list to re-point its members at the
// vector, then hands the object to the virtual at 0x114 and to 0x08C312E4 before a last call to 0x08AA97D4.

// ---------------------------------------------------------------------------------------------------------
// 0x08B04D98 (a0 = output record, a1 = a second word handed to the tail call, a2 = a count, a3 = a count
// kept in the frame): builds the 0x120-byte block 0x08B62344 allocates -- six 0x18-byte nodes whose +2
// halfwords are -1 and whose +8 pointers link them in a chain, the default lists through 0x08C4A6B0 /
// 0x08C4A4AC / 0x08C4A3B8 / 0x08C4A2C4 / 0x08C4A8F8 / 0x08C4A7CC / 0x08C4A594, then the constants at
// +0xB4 (gravity -9.8, 5 as an int, 0.5, 0.1, 0.000025, 0.001217) -- stores it at the output, fills the
// output's +4 with the singleton build from 0x08B6441C / 0x08B64494 (0x08D77330 / 0x08D77338, vtable
// 0x08CD79B0), and ends in a jump to 0x08B039F8(out, a1 ? a1 : 0x60, 0x10).

// ---------------------------------------------------------------------------------------------------------
// The packed alpha both sprite submissions derive from the frame's 0x18 float: 255 when the raw value is at
// least `limit`, 0 when the scaled value (raw * 0.125, or raw / limit) is negative, that scaled value times
// 255 when it is at most 1.0, else 255 -- then the saturating trunc.w.s to a word, packed into the top byte
// of 0x00FFFFFF, or 0xFFFFFFFF when the word does not fit a byte.
u32 packed_alpha(AllegrexContext &ctx, float raw, float limit, bool eighth) {
    float value = std::bit_cast<float>(0x437F0000u);  // 255.0
    if (raw < limit) {
        const float scaled = eighth ? psp_mul(raw, 0.125f) : raw / limit;
        if (scaled < 0.0f) value = 0.0f;
        else if (!(1.0f < scaled)) value = psp_mul(scaled, std::bit_cast<float>(0x437F0000u));
    }
    u32 truncated = ctx.fpu_float_to_word(value, 1u);
    if (std::bit_cast<float>(0x4F000000u) <= value) {
        truncated = ctx.fpu_float_to_word(value - std::bit_cast<float>(0x4F000000u), 1u) | 0x80000000u;
    }
    return truncated < 0x100u ? ((truncated << 24) | 0x00FFFFFFu) : 0xFFFFFFFFu;
}

// The four 0x18-byte vertices at `out`: the w component alternates 1.0 / 0.0 (the other way for `second`), the
// packed colour sits at +8 of each, and the three floats come from the frame blocks at 0x20 / 0x30 / 0x50 / 0x40.
void sprite_vertices(Runtime &rt, u32 out, bool second, u32 colour, u32 block) {
    const float one = std::bit_cast<float>(0x3F800000u);
    put(rt, out + 0x08u, colour);
    if (!second) {
        put_float(rt, out, one);
        put_float(rt, out + 0x04u, one);
        put(rt, out + 0x18u, 0u);
        put_float(rt, out + 0x1Cu, one);
        put_float(rt, out + 0x30u, one);
        put(rt, out + 0x34u, 0u);
        put(rt, out + 0x48u, 0u);
    } else {
        put(rt, out, 0u);
        put_float(rt, out + 0x04u, one);
        put_float(rt, out + 0x18u, one);
        put_float(rt, out + 0x1Cu, one);
        put(rt, out + 0x30u, 0u);
        put(rt, out + 0x34u, 0u);
        put_float(rt, out + 0x48u, one);
    }
    put(rt, out + 0x0Cu, word(rt, block + 0x20u));
    put(rt, out + 0x10u, word(rt, block + 0x24u));
    put(rt, out + 0x14u, word(rt, block + 0x28u));
    put(rt, out + 0x24u, word(rt, block + 0x30u));
    put(rt, out + 0x28u, word(rt, block + 0x34u));
    put(rt, out + 0x2Cu, word(rt, block + 0x38u));
    put(rt, out + 0x3Cu, word(rt, block + 0x50u));
    put(rt, out + 0x40u, word(rt, block + 0x54u));
    put(rt, out + 0x44u, word(rt, block + 0x58u));
    put(rt, out + 0x4Cu, 0u);
    put(rt, out + 0x54u, word(rt, block + 0x40u));
    put(rt, out + 0x58u, word(rt, block + 0x44u));
    put(rt, out + 0x5Cu, word(rt, block + 0x48u));
}

// 0x088C7948 (a0 ignored, a1 = four 0x10-byte vertices, a2 = flag): asks 0x08B5A668 for the two projection
// blocks in the frame, packs the alpha of the frame's 0x18 value (raw * 0.125, 255 when it does not fit), takes
// 0x60 bytes from 0x08B550E0, spins, sets each vertex's w and calls 0x08B5A7F0 per vertex, fills the four
// 0x18-byte vertices (the 0/1 variant when the low byte of a2 is set) and submits them through 0x08B569A0 /
// 0x08B56510 / 0x08B555F8 / 0x08B556A0 / 0x08B55588 / 0x08B56940; v0 = the last call's.

// 0x088C9918 (a0 ignored, a1 = four 0x10-byte vertices, a2 = flag): the same sprite submission as 0x088C7948,
// with the alpha of the frame's 0x18 value divided by 48 and the second vertex variant chosen when a2 is zero.

} // namespace
} // namespace new_batch_main_1003
} // namespace gt::game

#define NEW1003(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1003::name, gt::ReturnKind::kind)
NEW1003(0x08A368A4u, func_08A368A4, Word);
NEW1003(0x08B12D50u, func_08B12D50, Word);
