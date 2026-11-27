// New batch main 2037 (draft only): functions drafted before that mismatched the original in the gate, rewritten by hand
// from `tools.disasm` as structured C (named locals, G[n] only at the call boundary). Each body follows the original's
// delay slots (they run before the jal), leaves v0 as the asm leaves it on every exit path and passes the argument
// registers the callee reads. Input shapes live in shapes_main2037.txt (config/synth_shapes format).
//
// What each gate report pointed at:
//   0x08950438 v0 was 0, the asm sets 1 on every path that reaches the return at 0x08950618/0x0895061C.
//   0x0887C0A8 v0 was -delta; the multiplier (movz with t0==0) makes the first delta `low - high`, not `high - low`.
//   0x088865C8 a byte at block+0x1C stayed unzeroed: the 2-iteration pair loop zeroes +0x14..+0x20, not just +0x14/+0x18.
//   0x0897B364 `lui v0,0xFF; ori v0,0xFFFF` is 0x00FFFFFF (the gate saw 00 at object+0x17F), not 0xFFFFFFFF.
//   0x08917300 the word at container+0 is the array the loop walks (`lw s1,0(a0)` then `lw v1,0(s1)`), so it takes a
//              pointer shape (the old line left it as arena data and the run faulted at 0x1). Its two `lw $s0,0xC($sp)` /
//              `lw $s0,4($sp)` read values a callee stored on the frame (the smart pointer's control word and the replaced
//              node), which the body reads back through frame.at(), not unmodelled register reloads (sp_loads flags them).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2037 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 / v1 / f0 of the callee are what the next instruction sees, as after the original's `jal`.
inline GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A call with a0-a3 set like the original sets them before the jal (a callee can read a register its prototype does not
// name). Returns v0.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_full(rt, ctx, target, arguments).v0;
}
inline float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_full(rt, ctx, target, arguments).f0;
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}
inline void copy_words(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    for (u32 offset = 0u; offset < bytes; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
}
// The atomic refcount decrement the original does with `ll`/`sc` (single-threaded here: the `sc` always lands): returns
// the value before the decrement. The asm leaves v0 = 1 right after it (the `addiu v0,zero,1` in its branch delay).
inline u32 release_count(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}

// 0x08950438 (a0 = self, a1/a2/a3 = three byte flags, t0 = a state word): the mode byte comes from (*(a0+0xC))+0x270+0x10,
// the four callees answer time / count / two floats, `window` starts at 0x1869F and only a count whose lookup succeeds
// replaces it, and the time windows (1000 / 300 / 500 / 1000) plus 0.2f / 0.7f pick which flag bytes are set. The
// original has v0 = 1 on every path that reaches the shared return (0x08950618 or the mid-epilogue 0x0895061C).
void func_08950438(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag_a = ctx.gpr[5];
    const u32 flag_b = ctx.gpr[6];
    const u32 flag_c = ctx.gpr[7];
    const u32 state = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);

    const u32 block = word(rt, self + 0x0Cu);
    const u32 time = call_set(rt, ctx, 0x08949E8Cu, {self});
    const u32 base = block + 0x270u;
    const u32 mode = byte_u(rt, base + 0x10u);
    const u32 count = call_set(rt, ctx, 0x0895534Cu, {self});
    const float f20 = call_float(rt, ctx, 0x08955960u, {self});
    const float f21 = call_float(rt, ctx, 0x0895596Cu, {self});

    if (state != 0u) put(rt, state, 1u);

    u32 window = 0x0001869Fu;
    if (count != 0u && mode != count) {
        if (call_set(rt, ctx, 0x08955760u, {self}) != 0u) window = call_set(rt, ctx, 0x0895DD1Cu, {self, count});
    }

    const bool small = static_cast<s32>(count) < 2;
    u32 threshold;
    if (!small) {
        threshold = call_set(rt, ctx, 0x0895DCB8u, {self, count});
    } else if (count != 1u) {
        threshold = 0xFFFE7961u;
    } else {
        // 0x3E4CCCCD is 0.2f.
        if (static_cast<s32>(window - 0x3E8u) < static_cast<s32>(time) && std::bit_cast<float>(0x3E4CCCCDu) < f20)
            put8(rt, flag_a, count);
        if (static_cast<s32>(time) < static_cast<s32>(0xFFFE7579u)) put8(rt, flag_b, 1u);
        ctx.gpr[2] = 1u;
        return;
    }

    if (static_cast<s32>(window - 0x12Cu) < static_cast<s32>(time) && 0.7f < f20) put8(rt, flag_a, 1u);

    if (0.0f < f20) {
        if (static_cast<s32>(time) < static_cast<s32>(threshold)) put8(rt, flag_b, 1u);
        ctx.gpr[2] = 1u;
        return;
    }
    if (f21 == 0.0f) {
        if (static_cast<s32>(time) < static_cast<s32>(threshold + 0x1F4u)) put8(rt, flag_b, 1u);
        ctx.gpr[2] = 1u;
        return;
    }
    if (static_cast<s32>(time) < static_cast<s32>(threshold + 0x3E8u)) put8(rt, flag_b, 1u);
    if (!small) put8(rt, flag_c, 1u);
    ctx.gpr[2] = 1u;
}

// 0x0887C0A8 (a0 = state, a1 = owner): two 0..1 lookups from owner+0x60's +0xF0 table become the floats at +8 / +0xC;
// then owner+0x60's +0xF4 word is diffed against the state's old +0x18 (changed bits at +0x20, shared at +0x1C), the
// per-bit counters at +0x34..+0x40 are bumped, and two deltas are squared, clamped to 0.25 and signed into +0x10 / +0x14.
// The `movz v0, -1, t0` makes the first delta `low - high` and the second `high - low`; v0 ends as the second delta.
void func_0887C0A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);

    const u32 holder = word(rt, owner + 0x60u);
    const u32 table = word(rt, holder + 0xF0u);
    const u32 first = call_set(rt, ctx, 0x08B21590u, {table, 0u});
    put_float(rt, self + 0x08u, call_float(rt, ctx, 0x0887BF80u, {first, 0x7Fu, 0x7Fu, 0x28u}));
    const u32 second = call_set(rt, ctx, 0x08B21590u, {table, 1u});
    put_float(rt, self + 0x0Cu, call_float(rt, ctx, 0x0887BF80u, {second, 0x7Fu, 0x7Fu, 0x28u}));

    const u32 previous = word(rt, self + 0x18u);
    const u32 flags = call_set(rt, ctx, 0x08B214D8u, {word(rt, word(rt, owner + 0x60u) + 0xF4u), 0u});
    const u32 changed = previous ^ flags;
    const u32 shared = flags & changed;
    put(rt, self + 0x20u, changed & ~flags);
    put(rt, self + 0x18u, flags);
    put(rt, self + 0x1Cu, shared);
    put(rt, self + 0x24u, 0u);

    // Three flag words: a shared bit forces the slot to 0x1C and marks +0x24; a slot only this side owns counts down to
    // zero and is then set to 9.
    for (u32 i = 0u; i < 3u; ++i) {
        const u32 mask = word(rt, 0x08C8983Cu + 4u * i);
        const u32 own = mask & flags;
        if ((shared & mask) != 0u) {
            put(rt, self + 0x24u, word(rt, self + 0x24u) | own);
            put(rt, self + 0x28u + 4u * i, 0x1Cu);
        } else if (own != 0u) {
            const u32 left = word(rt, self + 0x28u + 4u * i) - 1u;
            put(rt, self + 0x28u + 4u * i, left);
            if (static_cast<s32>(left) <= 0) {
                put(rt, self + 0x24u, word(rt, self + 0x24u) | own);
                put(rt, self + 0x28u + 4u * i, 9u);
            }
        }
    }

    // Bits 4..7 of the flags bump the signed counters at +0x34..+0x40, capped at 60 (`min` is the signed one).
    for (u32 bit = 4u; bit < 8u; ++bit) {
        const u32 address = self + 0x34u + 4u * (bit - 4u);
        u32 value = 0u;
        if ((flags & (1u << bit)) != 0u) {
            const s32 bumped = static_cast<s32>(word(rt, address) + 1u);
            value = static_cast<u32>(bumped < 0x3C ? bumped : 0x3C);
        }
        put(rt, address, value);
    }

    // Two signed deltas of the counter pairs, squared, clamped to 0.25, sign restored.
    u32 last = 0u;
    for (u32 i = 0u; i < 2u; ++i) {
        const u32 high = word(rt, self + 0x40u - 4u * i);
        const u32 low = word(rt, self + 0x38u - 4u * i);
        const s32 delta = (i == 0u) ? static_cast<s32>(low - high) : static_cast<s32>(high - low);
        last = static_cast<u32>(delta);
        put(rt, self + 0x10u + 4u * i, 0u);
        if (delta != 0) {
            float value = static_cast<float>(delta) / 60.0f;
            value = value * value;
            if (value < 0.25f) value = 0.25f;
            value = delta < 0 ? value * -1.0f : value * 1.0f;
            put_float(rt, self + 0x10u + 4u * i, value);
        }
    }
    ctx.gpr[2] = last;
}

// 0x088865C8 (a0 = object): constructor — a vtable at +0x50 over a 0xEC block, a second vtable at +0, a 0x10 node handed
// to 0x08B8E380, the 3.0f / 1/60 / 1.0f fields, a 0x20-word zero block at +0x198, sixteen 6-byte records at +0x2B8,
// three container initialisers and four zeroed words. v0 ends as 2 (the last clear loop's counter).
void func_088865C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);

    put(rt, self + 0x50u, 0x08CC1798u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    put(rt, self + 0x4Cu, 0u);
    const u32 inner = self + 0x50u;
    const u32 cleared = word(rt, inner + 0x14u) & ~3u;
    put(rt, inner + 0x10u, 0u);
    put(rt, inner + 0x14u, cleared);

    const u32 table = 0x08D75298u + 0xCu;
    const u32 block = call_set(rt, ctx, 0x08B62344u, {0xECu});
    put(rt, block + 0x00u, 0x08CC1800u);
    put(rt, block + 0x04u, 0u);
    put(rt, block + 0x08u, 0u);
    put(rt, block + 0x0Cu, table);
    put(rt, block + 0x10u, table);
    put(rt, block + 0x14u, 0u);
    put(rt, block + 0x18u, 0u);
    put(rt, block + 0x1Cu, 0u);
    put(rt, block + 0x20u, 0u);
    put8(rt, block + 0x24u, 0u);
    put8(rt, block + 0x25u, 0u);
    put(rt, block + 0x00u, 0x08CC17C0u);
    put(rt, block + 0x24u, word(rt, block + 0x24u) & ~(1u << 16u));
    put(rt, self + 0x68u, block);

    const u32 node = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, node + 0x00u, 0x08CC1840u);
    put(rt, node + 0x04u, 1u);
    put(rt, node + 0x08u, 1u);
    put(rt, node + 0x0Cu, block);
    put(rt, self + 0x6Cu, node);
    call_set(rt, ctx, 0x08B8E380u, {self + 0x6Cu, block, block, 1u, ctx.gpr[8], ctx.gpr[9], ctx.gpr[10],
                                    ctx.gpr[11]});

    put_float(rt, self + 0x160u, 3.0f);
    put_float(rt, self + 0x168u, std::bit_cast<float>(0x3C888889u));
    put_float(rt, self + 0x170u, 1.0f);
    u32 flags = word(rt, self + 0x78u);
    flags = (flags & ~0xFFFu) | 0x1000u;
    flags &= ~(7u << 13u);
    put(rt, self + 0x78u, flags);
    put(rt, self + 0x188u, table);
    put(rt, self + 0x70u, 0u);
    put(rt, self + 0x74u, 0u);
    rt.memory().store16(self + 0x7Au, 0u);
    rt.memory().store16(self + 0x9Au, 0u);
    put(rt, self + 0x15Cu, 0u);
    put(rt, self + 0x164u, 0u);
    put(rt, self + 0x16Cu, 0u);
    put(rt, self + 0x184u, table);
    put(rt, self + 0x18Cu, 0u);
    put(rt, self + 0x190u, 0u);
    put(rt, self + 0x194u, 0u);
    for (u32 i = 0u; i < 0x20u; ++i) put(rt, self + 0x198u + 4u * i, 0u);
    for (u32 i = 0u; i < 0x10u; ++i) {
        const u32 record = self + 0x2B8u + 6u * i;
        rt.memory().store8(record, 0u);
        rt.memory().store16(record + 2u, static_cast<std::uint16_t>(0xFFFFu));
        rt.memory().store16(record + 4u, static_cast<std::uint16_t>(0xFFFFu));
    }

    call_set(rt, ctx, 0x08B4E648u, {self});
    call_set(rt, ctx, 0x08B945C4u, {self + 0x188u, 0x08C89A48u, 0u});
    call_set(rt, ctx, 0x08884DA0u, {self, 1u});
    for (u32 i = 0u; i < 2u; ++i) {
        put(rt, self + 0x174u + 4u * i, 0u);
        put(rt, self + 0x17Cu + 4u * i, 0u);
    }
    ctx.gpr[2] = 2u;
}

// 0x0897B364 (a0 = object, a1 = a pointer stored at +0x14): constructor — two vtables, a self-linked pair at +0x18, two
// 0x58-byte tables copied from .rodata into +0x94 and +0xEC, the descriptor fields at +0x14A..+0x158, four one-argument
// initialisers at +0x19C..+0x1A8 and a block of zeroed/1.0f fields. The word at +0x17C is 0x00FFFFFF (`lui 0xFF` + `ori`),
// not 0xFFFFFFFF; v0 ends as the 0xFFFFFFFF stored at +0x1F0.
void func_0897B364(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);

    call_set(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self + 0x00u, 0x08CBB740u);
    put(rt, self + 0x18u, self + 0x18u);
    put(rt, self + 0x14u, argument);
    put(rt, self + 0x1Cu, self + 0x18u);
    call_set(rt, ctx, 0x08B28DCCu, {self + 0x20u});
    call_set(rt, ctx, 0x08B28D34u, {self + 0x48u});
    put(rt, self + 0x78u, 0u);
    put(rt, self + 0x74u, 0u);
    put(rt, self + 0x7Cu, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, self + 0x84u, 0u);
    put8(rt, self + 0x88u, 0u);
    put(rt, self + 0x8Cu, 0u);
    put(rt, self + 0x90u, 0u);
    copy_words(rt, self + 0x94u, 0x08C8BEACu, 0x50u + 8u);
    copy_words(rt, self + 0xECu, 0x08C8BE54u, 0x50u + 8u);

    rt.memory().store16(self + 0x14Au, rt.memory().load16(0x08C8BE50u));
    put(rt, self + 0x14Cu, word(rt, 0x08C8BE48u));
    rt.memory().store16(self + 0x154u, rt.memory().load16(0x08C8BE44u));
    put(rt, self + 0x150u, word(rt, 0x08C8BE48u + 4u));
    put(rt, self + 0x158u, word(rt, 0x08C8BE40u));
    put(rt, self + 0x168u, 0x0Au);
    rt.memory().store16(self + 0x174u, 0x14u);
    put(rt, self + 0x16Cu, 0x12Cu);
    rt.memory().store16(self + 0x176u, 6u);
    put_float(rt, self + 0x170u, 1.0f);
    put(rt, self + 0x17Cu, 0x00FFFFFFu);
    put(rt, self + 0x144u, 0u);
    put8(rt, self + 0x148u, 0u);
    put8(rt, self + 0x149u, 0u);
    rt.memory().store16(self + 0x156u, 0u);
    put(rt, self + 0x15Cu, 0u);
    put8(rt, self + 0x160u, 0u);
    put(rt, self + 0x164u, 0u);
    put(rt, self + 0x178u, 0u);
    put(rt, self + 0x180u, 0u);
    put(rt, self + 0x184u, self + 0x184u);
    put(rt, self + 0x188u, self + 0x184u);
    put(rt, self + 0x18Cu, self + 0x18Cu);
    put(rt, self + 0x190u, self + 0x18Cu);
    put(rt, self + 0x194u, 0u);
    put8(rt, self + 0x198u, 0u);
    for (u32 i = 0u; i < 4u; ++i) call_set(rt, ctx, 0x08A76AA0u, {self + 0x19Cu + 4u * i});

    put_float(rt, self + 0x1D4u, 1.0f);
    put(rt, self + 0x1ACu, 0u);
    put(rt, self + 0x1B0u, 0u);
    put8(rt, self + 0x1B4u, 0u);
    put8(rt, self + 0x1B5u, 0u);
    put(rt, self + 0x1B8u, 0u);
    put_float(rt, self + 0x1BCu, 1.0f);
    put(rt, self + 0x1C0u, 0u);
    put_float(rt, self + 0x1C4u, 1.0f);
    put(rt, self + 0x1C8u, 0u);
    put(rt, self + 0x1CCu, 0u);
    put_float(rt, self + 0x1D0u, 1.0f);
    put(rt, self + 0x1E0u, 0u);
    put(rt, self + 0x1F0u, 0xFFFFFFFFu);
    put(rt, self + 0x1E4u, 0u);
    put(rt, self + 0x1E8u, 0u);
    put(rt, self + 0x1ECu, 0u);
    put8(rt, self + 0x200u, 0u);
    put8(rt, self + 0x201u, 0u);
    put8(rt, self + 0x202u, 0u);
    put8(rt, self + 0x203u, 0u);
    put8(rt, self + 0x204u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08917300 (a0 = container, a1 = count): fills the container's array (word at +0) with `count` (object, node) pairs —
// the object from 08916A50(alloc(0x18)) and a 0x10 node (vtable 0x08CC5890, counts 1/1, +0xC = the object) — chains each
// new object to the previous at +0 and releases the pair it replaced (count at +4, virtuals at +8/+0xC when it reaches
// 1). v0 ends as array + 8*(count-1) when count > 0, else as the last call's answer.

} // namespace
} // namespace new_batch_main_2037
} // namespace gt::game

#define NEW2037(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2037::name, gt::ReturnKind::kind)
NEW2037(0x08950438u, func_08950438, Word);
NEW2037(0x0887C0A8u, func_0887C0A8, Word);
NEW2037(0x088865C8u, func_088865C8, Word);
NEW2037(0x0897B364u, func_0897B364, Word);
