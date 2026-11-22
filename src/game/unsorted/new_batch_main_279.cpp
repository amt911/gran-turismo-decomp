// ---- from main-session batch 205 (draft 2adb2f975af401020d34fd47bbbb1479549efb1c) ----
// New batch, main session 205: functions the recompiler emitted that no file had decompiled (08C54E74, 08C5F1C8,
// 08B4235C) or that only had a register-level body somewhere else (08C52070 in hot_batch_336, 0899FAB0 / 08B144F0
// as the drafts of main session 22), written by hand as structured C from `tools.disasm`. Draft only: the
// differential gate (fast, sabotage, synthetic) is run by the verifier, not here.
//
// Conventions, as in new_batch_main_22: every call writes the argument registers the original sets before the `jal`
// (a0-a3, t0-t3, f12-f15); the ones it leaves alone keep the caller's value, so a value the asm leaves in a register
// (or a callee then leaves there) is read from `ctx.gpr[]`. The delay slot of a `jal` runs BEFORE the call; a
// `bnel` / `beql` / `bc1fl` delay slot only runs when the branch is taken; a plain branch's delay slot runs on both
// paths. `v0` (and `v1` where a caller reads it) is left as the original leaves it on every exit path. The dead stack
// is not reproduced; the frames callees read or write are (`GuestFrame`, size = the original's `addiu sp, sp, -N`).
//
// Not drafted, because a draft would have to guess at their internals rather than read them off the asm:
// - 0x08AA553C: path joining over two 0x400-byte stack buffers rewritten in place by nine byte loops whose
//   `movn`/`movz` restore paths decide what stays in the output; one transposed store would only show up in the gate,
//   and the '/'/'.' handling needs the callers' string semantics first.
// - 0x08B6E52C: 64-bit sign/exponent surgery (`ext`, `ins`, `negu`, the table of ±176 at 0x08C966A0) feeding two
//   bit-twiddling helpers; the return is the (v0, v1) pair of a double, so DoubleWord, and the helper semantics
//   (0x08B71328 / 0x08B712B0) would have to be established first.
// - 0x08B8B9E8: merge-sort internals over 0x18-byte temporary objects whose fields are only ever seen through
//   0x08B8A654 / 0x08B8AD88 / 0x08B8B7B4, with the counts derived by `mult` with 0xAAAAAAAB (a division by 0x18).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_205 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The argument registers set as the original sets them: a0-a3, t0-t3 in order.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
inline u32 call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                     std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments).v0;
}
// `lw v1, 0(object); lw v0, off(v1); jalr v0`: the callee address goes in v0 and the vtable stays in v1 (the shape a
// virtual that never writes v0 answers its own address out of).
inline GuestResult call_virtual_full(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                                     std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                        std::initializer_list<u32> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}
// The same two loads with the vtable in v1 handed in (a cold path that continues with a register a helper left).
inline u32 call_slot(Runtime &rt, AllegrexContext &ctx, u32 table, u32 function,
                     std::initializer_list<u32> arguments) {
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {}).v0;
}

inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
inline void put16(Runtime &rt, u32 address, u32 value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline void put8(Runtime &rt, u32 address, u32 value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
// `trunc.w.s`, exactly as the recompiled original converts (saturating, NaN -> INT_MAX).
inline u32 trunc_word(AllegrexContext &ctx, float value) { return ctx.fpu_float_to_word_ct<1u>(value); }

// ---- functions ----

// 0x0899FAB0 (a0 = the owner, a1 = an argument): rebuilds the owner's list through 0x08A36C3C when 0x0899D044 and
// 0x0899D3CC both accept it (two orders of the three 0x0896A9C4 / 0x0896A35C list steps, chosen by 0x0899D3CC), then
// walks the list: a missing head logs through 0x08A5BB7C, 0x08A34DA8 and 0x08A5B9EC close it, the flags at + 0x13C
// get bit 3 set and bit 4 cleared, and when the object at + 0x160 exists the argument goes through 0x08A76AAC and that
// object's virtual at + 0xCC receives the two temporary holders. Then 0x089AA910's result sets flag bit 2, and
// 0x089AF5E8 / 0x089AB114 finish with the word at + 0x154; the float at + 0x120 is copied to + 0x150, which is also
// what f0 holds at the exit.
void func_0899FAB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0u);
    const u32 cursor = sp + 8u;
    if (call_set(rt, ctx, 0x0899D044u, {owner}) != 0u) {
        if (call_set(rt, ctx, 0x0899D3CCu, {owner}) != 0u) {
            put(rt, sp + 0xCu, 0u);
            call_set(rt, ctx, 0x0896A9C4u, {word(rt, owner + 0x158u), sp + 0xCu, 0u, 0u, 0u});
            const u32 holder = word(rt, owner + 0x158u);
            call_set(rt, ctx, 0x0896A35Cu, {owner, 0u, 0u, sp + 0xCu, 0u});
            call_set(rt, ctx, 0x0896A9C4u, {holder, 0u, 0u, sp + 0xCu, 0u});
        } else {
            put(rt, sp + 0xCu, 0u);
            call_set(rt, ctx, 0x0896A9C4u, {word(rt, owner + 0x158u), 0u, sp + 0xCu, 0u, 0u});
            const u32 holder = word(rt, owner + 0x158u);
            call_set(rt, ctx, 0x0896A35Cu, {owner, 0u, 0u, 0u, sp + 0xCu});
            call_set(rt, ctx, 0x0896A9C4u, {holder, 0u, 0u, 0u, sp + 0xCu});
        }
    }
    call_set(rt, ctx, 0x08A36C3Cu, {cursor});
    const u32 head = word(rt, sp + 8u);
    if (head == 0u) {
        ctx.gpr[5] = 0x4A0000u;  // the high half the `lui a1, 0x4A` in the branch delay left in a1
        call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4A0Cu});
    }
    call_set(rt, ctx, 0x08A34DA8u, {head, 0x08CA4A14u});
    call_set(rt, ctx, 0x08A5B9ECu, {cursor});
    put(rt, owner + 0x13Cu, (word(rt, owner + 0x13Cu) | 8u) & ~0x10u);
    if (word(rt, owner + 0x160u) != 0u) {
        const u32 first = sp + 0x10u;
        const u32 second = sp + 0x14u;
        const u32 temporary = sp + 4u;
        call_set(rt, ctx, 0x08A76AA0u, {first});
        call_set(rt, ctx, 0x08A76AA0u, {second});
        put(rt, sp, argument);
        call_set(rt, ctx, 0x08A76AACu, {temporary, sp});
        if (second != temporary) call_set(rt, ctx, 0x08A5BAD8u, {second, word(rt, sp + 4u)});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        const u32 target = word(rt, owner + 0x160u);
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA485Cu});
        call_virtual(rt, ctx, target, 0xCCu, {target, first, 1u, second});
        call_set(rt, ctx, 0x08A5B9ECu, {second});
        call_set(rt, ctx, 0x08A5B9ECu, {first});
    }
    const u32 state = call_set(rt, ctx, 0x089AA910u, {argument});
    put(rt, owner + 0x13Cu, (word(rt, owner + 0x13Cu) & ~4u) | ((state & 1u) << 2));
    call_set(rt, ctx, 0x089AF5E8u, {argument, 0u});
    call_set(rt, ctx, 0x089AB114u, {argument, word(rt, owner + 0x154u)});
    const float moved = float_at(rt, owner + 0x120u);
    put_float(rt, owner + 0x150u, moved);
    ctx.fpr[0] = moved;
}

// 0x08B144F0 (a0 = the object, a1 = an argument handed on to 0x08B136A8): checks the 0x40-byte block its virtual at
// vtable + 0x24 identifies. That virtual's two words seed 0x08B136A8 (which fills sp + 8); the name from 0x08B1262C is
// looked up by 0x08B23A70 and 0x08C7A848 returns a status and a length; 0x08B33F44 then reads the block into a 64-byte
// aligned frame buffer. The block's 0x40 bytes are folded with 0xCC, 0x08B13DC8 runs, and a CRC-32 with the table at
// 0x08CD7EC8 must come out zero; the first 16 bytes must equal 0x08B136A8's output, the next 16 its second half, the
// two words at + 0x20 / + 0x24 must match sp + 0x28 / sp + 0x2C, and the virtual's two words must equal the block's
// + 0x28 / + 0x2C. Returns 1 when every check holds, 0 otherwise (v0 = a0 = the answer).

// 0x08C52070 (a0 = this list's sentinel node, a1 = the other list's sentinel): list copy-assignment. While both lists
// still have nodes it copies each source value over the matching destination one (a value is the byte at +8, the words
// at +0xC / +0x10 and a string: its buffer at +0x14 and its length at +0x18; the assignment frees the old buffer with
// 0x08B61FC0, then 0x08B6461C allocates the new one when the length is positive). When the source ends first the
// remaining destination nodes are erased one at a time with 0x08C5201C; when the destination ends first every
// remaining source value goes into a fresh 0x1C-byte node from 0x08B35DA0, linked by 0x08B5C078 at the end. The
// assignment is skipped where destination and source hold the same value address (0xC520B4 vs 0xC5215C).
// v0 = a0 (return *this).

// 0x08C54E74 (a0 = this, a1 = the source): copy-assigns a container that keeps up to 8 8-byte entries inline at
// + 0x24 (more go into a buffer 0x08B6461C allocates for count<<3 bytes and zero-fills) through the data pointer at
// + 0x68 and the count at + 0x64, with the reference-counted holder at + 0x18 (0x08B6085C) and the two-step
// 0x08B6534C / 0x08B653DC reset around the old buffer (released with 0x08B61FC0 unless it is the inline one). The
// entries are copied, the words at + 4 / + 8 / + 0xC / + 0x70 follow, the lazy flag at + 0x75 on both sides is set
// through 0x08C5477C with 0x20, the byte at + 0x74 of the source is copied over, and the 0x6C sub-object is assigned
// through a frame temporary (0x08B5C450 / 0x08B5D1E0 / 0x08B5D34C) before 0x08C548C4, 0x08B6534C with 2, the word at
// + 0x10 and 0x08C54CFC with the word at + 0x14 finish. v0 = this.

// 0x0895A0D8 (a0 = the object, a1 = the record to fill, a2 = a word stored at its + 0): reads the object's state at
// + 8 and the animation at 0x08959180, and fills the record: the word from a2, a flag byte at + 4 (bits from the
// state's + 0x5F8 / + 0x5F9, the low nibble signed, bit 4 from 0x08955990), three angle bytes at + 5 / + 6 (the
// floats at + 0x534 / + 0x538 are truncated as half-degrees with 0x3C0EFA35, + 0x520 divided by it), the halfword at
// + 0x30, two float pairs at + 8 / + 0x14 (from + 0x10 and + 0x6CC), the three floats at + 0x10 / + 0x1C from the
// state and the three at + 0x20 from the animation object, then two halves at + 0x2C from the summed pairs of three
// floats 0x08955300 returns, each scaled by 0x3DD0FAC6 and clamped to ±16000. v0 = the last halfword written.
void func_0895A0D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 initial = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0u);
    const u32 state = word(rt, object + 8u);
    put(rt, out, initial);
    const u32 animation = call_set(rt, ctx, 0x08959180u, {word(rt, object), byte_u(rt, state)});
    put8(rt, out + 4u, 0u);
    put8(rt, out + 7u, byte_u(rt, animation + 0x76u));
    const u32 raw = byte_u(rt, state + 0x5F8u);
    put8(rt, out + 4u, static_cast<s32>(raw) < 0x10 ? sext8(raw) : 0xFu);
    const u32 extra = call_set(rt, ctx, 0x08955990u, {object});
    u32 flags = byte_u(rt, out + 4u);
    if (extra != 0u) {
        flags |= 0x10u;
        put8(rt, out + 4u, flags);
    }
    flags = (byte_u(rt, state + 0x5F9u) << 5) | flags;
    put8(rt, out + 4u, flags);
    put16(rt, out + 0x30u, half_u(rt, state + 0x58Eu));
    const float fifteen = 15.0f;
    const float half_degrees = std::bit_cast<float>(0x3C0EFA35u);  // one half-degree in radians
    const u32 first_angle = trunc_word(ctx, psp_mul(float_at(rt, state + 0x534u), fifteen));
    const u32 second_angle = trunc_word(ctx, psp_mul(float_at(rt, state + 0x538u), fifteen));
    put8(rt, out + 5u, (first_angle << 4) | second_angle);
    const float divided = float_at(rt, state + 0x520u) / half_degrees;
    put8(rt, out + 6u, trunc_word(ctx, divided));
    for (u32 i = 0; i != 2u; ++i) {
        put_float(rt, out + 8u + i * 4u, float_at(rt, state + 0x10u + i * 4u));
        put_float(rt, out + 0x14u + i * 4u, float_at(rt, state + 0x6CCu + i * 4u));
    }
    put_float(rt, out + 0x10u, float_at(rt, state + 0x704u));
    put_float(rt, out + 0x1Cu, float_at(rt, state + 0x6F8u));
    put_float(rt, out + 0x20u, float_at(rt, animation + 0x7Cu));
    put_float(rt, out + 0x24u, float_at(rt, animation + 0x80u));
    put_float(rt, out + 0x28u, float_at(rt, animation + 0x84u));
    put(rt, sp, 0u);
    put(rt, sp + 4u, 0u);
    for (u32 i = 0; i != 4u; ++i) {
        const u32 values = call_set(rt, ctx, 0x08955300u, {object, i});
        const float sum = float_at(rt, values + 0x88u) + float_at(rt, values + 0x8Cu);
        const u32 slot = sp + (static_cast<u32>(static_cast<s32>(i) >> 1) * 4u);
        put_float(rt, slot, float_at(rt, slot) + sum);
    }
    const float scale = std::bit_cast<float>(0x3DD0FAC6u);
    for (u32 i = 0; i != 2u; ++i) {
        const float scaled = psp_mul(float_at(rt, sp + i * 4u), scale);
        u32 value = 0u;
        if (16000.0f < scaled) {
            value = 0x3E80u;
        } else if (scaled < -16000.0f) {
            value = static_cast<u32>(-0x3E80);
        } else {
            value = trunc_word(ctx, scaled);
        }
        put16(rt, out + 0x2Cu + i * 2u, value);
        ctx.gpr[2] = value;
    }
}

// 0x08B4235C (a0 = the object, a1 = a 0x18-byte setup block): starts the object's job. Already started (byte + 0xAD
// set) answers 0 at once; otherwise it locks + 0x12C, runs 0x08B084A8 / 0x08B3FF98, stores the block's halfword + 2
// (0x20 when zero) at + 0x934, releases the old + 0xB0 through its first virtual and 0x08B2CE7C, stores
// 0x08B404C8's answer at + 0xC8, sets the buffer at + 0xCC from the block at + 0xCE0 (0x20000) with 0x08B2C8A0, takes
// the two 0x10-byte records 0x08B2CB18 finds for 0x648 / 0x4950 (0x08B3E900 / 0x08B47588 initialise them) and asks
// the second one through its virtuals + 8 (with + 0xCC), + 0x28 / + 0x30 (the block's bytes + 0x10 / + 0x11) and + 0x5C
// (0x4E9D), then 0x08B40080, the halfword + 0xB4 (the block's, or 0x4E20), 0x08B3FA98 and the + 0x40 virtual (the block
// and the frame). A positive answer sets byte + 0xAD, runs 0x08B36018 on + 0x990 and 0x08B3F7CC on + 0xBB8 and
// answers 1; any refusal answers 0 after the object's + 0x14 virtual. Both exits unlock + 0x12C. v0 = 1 or 0.
void func_08B4235C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 setup = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0u);
    u32 result = 0u;
    if (byte_u(rt, object + 0xADu) == 0u) {
        const u32 lock = object + 0x12Cu;
        call_set(rt, ctx, 0x08B344E0u, {lock});
        call_set(rt, ctx, 0x08B084A8u, {object});
        call_set(rt, ctx, 0x08B3FF98u, {object});
        const u32 limit = half_u(rt, setup + 2u);
        put16(rt, object + 0x934u, limit != 0u ? limit : 0x20u);
        if (word(rt, object + 0xB0u) != 0u) {
            call_virtual(rt, ctx, word(rt, object + 0xB0u), 0u, {word(rt, object + 0xB0u)});
            call_set(rt, ctx, 0x08B2CE7Cu, {object + 0xCCu, word(rt, object + 0xB0u)});
            put(rt, object + 0xB0u, 0u);
        }
        put(rt, object + 0xC8u, call_set(rt, ctx, 0x08B404C8u, {object}));
        call_set(rt, ctx, 0x08B2C8A0u, {object + 0xCCu, object + 0xCE0u, 0x20000u});
        const u32 first = call_set(rt, ctx, 0x08B2CB18u, {object + 0xCCu, 0x648u, 0x10u});
        if (first != 0u) call_set(rt, ctx, 0x08B3E900u, {first});
        put(rt, object + 0x930u, first);
        const u32 second = call_set(rt, ctx, 0x08B2CB18u, {object + 0xCCu, 0x4950u, 0x10u});
        if (second != 0u) call_set(rt, ctx, 0x08B47588u, {second});
        put(rt, object + 0xB0u, second);
        bool ready = call_virtual(rt, ctx, second, 8u, {second, object + 0xCCu}) != 0u;
        if (ready) {
            call_virtual(rt, ctx, word(rt, object + 0xB0u), 0x28u, {word(rt, object + 0xB0u), byte_u(rt, setup + 0x10u)});
            call_virtual(rt, ctx, word(rt, object + 0xB0u), 0x30u, {word(rt, object + 0xB0u), byte_u(rt, setup + 0x11u)});
            put(rt, object + 0x98Cu, word(rt, setup + 8u));
            put(rt, object + 0x988u, word(rt, setup + 0x14u));
            call_set(rt, ctx, 0x08B40080u, {object});
            ready = call_virtual(rt, ctx, word(rt, object + 0xB0u), 0x5Cu, {word(rt, object + 0xB0u), 0x4E9Du}) != 0u &&
                    half_u(rt, object + 0xB4u) == 0u;
        }
        if (ready) {
            const u32 timeout = half_u(rt, setup);
            put16(rt, object + 0xB4u, timeout != 0u ? timeout : 0x4E20u);
            call_set(rt, ctx, 0x08B3FA98u, {sp});
            const bool started = call_virtual(
                rt, ctx, word(rt, object + 0xB0u), 0x40u,
                {word(rt, object + 0xB0u), half_u(rt, object + 0xB4u), sp}) != 0u;
            if (started) {
                call_set(rt, ctx, 0x08B36018u, {object + 0x990u, 0u, 0x348u});
                call_set(rt, ctx, 0x08B3F7CCu, {object + 0xBB8u, sp});
                put8(rt, object + 0xADu, 1u);
                result = 1u;
            }
        }
        if (result == 0u) call_virtual(rt, ctx, object, 0x14u, {object});
        call_set(rt, ctx, 0x08B3457Cu, {lock});
    }
    ctx.gpr[2] = result;
}

// 0x08AE8FAC (a0 = the node, f12 = a level): walks the sibling chain at node + 0x1C (only while the next node's + 8
// word is not negative) and, for every node 0x08AE8EAC reports with (flags & 0x11) == 1, ORs (flags & 0x160) into the
// caller's word at + 0xC and pushes two transforms with 0x08AE86F8 / 0x08AE87F8 — the level as f12, or the level
// times the node's halfword at + 0x70 quantised by 1/4096 when its halfword + 0x5E has bit 10 set. It then builds the
// key in the frame through 0x08AD8E24 / 0x08AD8E84 / 0x08AE8874 and runs 0x08AE8A1C when the key or one of the two
// masks at + 0x98 / + 0x9C (shifted by the node's signed byte + 0xB) asks for it, plus 0x08AE8D50 for the key's second
// bit set or the second mask, then clears the node's + 0x5C halfword. A node whose key needs neither goes through
// 0x08AE87C4 first and only then builds it. Every other node is handed to 0x08AE740C.

// 0x08C5F1C8 (a1 = a {word, byte} pair, a2 = the holder, t0 / t1 / t2 = three more arguments; a0 and a3 are not
// read): looks the type 0x08D77478 up in the holder's container at + 0x6C (0x08B5C2B0 gives the index) and, when it
// is there, builds a four-byte key in the frame for that element with 0x08B61010 (element, 0x90-byte scratch block,
// 0x80, key, t0) and 0x08B35F60, then either answers the pair's byte (+4) or asks the pair's own virtual + 0x30
// whether the answer equals the length. The key's first byte is the state's + 0x42 (or the + 0x18 virtual with 0x25
// when + 0x1C is clear, after the + 0x1C virtual filled it with a 0..0xFF pattern verified by 0x08B36170); the next two
// are t1 / t2, or t2 / t1 with a zero in between, and the last is zero. Returns the pair's word in v0 and the byte the
// frame holds at + 8 in v1 (the pair's byte when it is not zero, else whether the lengths differed).
void func_08C5F1C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 pair = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    const u32 argument0 = ctx.gpr[8];
    const u32 argument1 = sext8(ctx.gpr[9]);   // `seb t1`
    const u32 argument2 = sext8(ctx.gpr[10]);  // `seb t2`
    GuestFrame frame(ctx, 0x160u);
    const u32 sp = frame.at(0u);
    const u32 context = word(rt, pair);
    const u32 flag = byte_u(rt, pair + 4u);
    put(rt, sp + 0x110u, flag);
    put(rt, sp + 0x114u, argument0);
    put(rt, sp + 0x118u, argument1);
    put(rt, sp + 0x11Cu, argument2);
    const u32 state = call_set(rt, ctx, 0x08B64C80u, {holder + 0x6Cu});
    u32 element = ctx.gpr[23];  // s7: only written when the index is in range
    u32 block = ctx.gpr[19];    // s3: the 0x90-byte scratch block, only set on the path below
    const u32 index = call_set(rt, ctx, 0x08B5C2B0u, {0x08D77478u});
    const u32 container = word(rt, holder + 0x6Cu);
    const u32 count = word(rt, container + 8u);
    const bool in_range = index < count;
    if (in_range) element = word(rt, word(rt, container + 4u) + index * 4u);
    // Reads the state's + 0x18 virtual with 0x25 and stores its answer as the key's first byte.
    const auto read_marker = [&](u32 table) {
        put8(rt, sp, call_slot(rt, ctx, table, word(rt, table + 0x18u), {state, 0x25u}));
    };
    if (!in_range || element == 0u) {
        // 0x08C5F3BC: the out-of-range path builds an object with 0x08B5A9FC; the code the compiler placed after it
        // keeps whatever v1 that left (the `lw v1, 0(s2)` never ran), exactly as the original does.
        ctx.gpr[4] = index;
        call_set(rt, ctx, 0x08B5A9FCu, {index});
        const u32 table = ctx.gpr[3];
        put8(rt, state + 0x1Cu, 2u);
        read_marker(table);
    } else {
        ctx.gpr[29] = sp - 0x90u;
        block = ctx.gpr[29];
        if (signed_byte(rt, state + 0x1Cu) != 0) {
            put8(rt, sp, byte_u(rt, state + 0x42u));
        } else {
            // 0x08C5F324: number the scratch buffer 0..0xFF, let the state's + 0x1C virtual fill the key against it
            // and verify it with 0x08B36170, then read the + 0x18 virtual.
            for (u32 i = 0; i != 0x100u; ++i) put8(rt, sp + 0xCu + i, i);
            const u32 pattern = sp + 0xCu;
            const u32 table = word(rt, state);
            const u32 target = word(rt, table + 0x1Cu);
            call_slot(rt, ctx, table, target, {state, pattern, sp + 0x10Cu, state + 0x1Du});
            put8(rt, state + 0x1Cu, 1u);
            if (call_set(rt, ctx, 0x08B36170u, {pattern, state + 0x1Du, 0x100u}) != 0u) {
                put8(rt, state + 0x1Cu, 2u);
            }
            read_marker(word(rt, state));
        }
    }
    if (argument2 != 0u) {
        put8(rt, sp + 3u, 0u);
        put8(rt, sp + 1u, argument2);
        put8(rt, sp + 2u, argument1);
    } else {
        put8(rt, sp + 2u, 0u);
        put8(rt, sp + 1u, argument1);
    }
    ctx.gpr[8] = word(rt, sp + 0x114u);
    // The fifth argument (t0) is the value the frame slot holds, reloaded as the original does at 0x08C5F290.
    call_set(rt, ctx, 0x08B61010u, {element, block, 0x80u, sp, word(rt, sp + 0x114u)});
    const u32 length = call_set(rt, ctx, 0x08B35F60u, {block});
    u32 result = flag;
    if (flag == 0u) {
        const u32 table = word(rt, context);
        const u32 target = word(rt, table + 0x30u);
        result = call_slot(rt, ctx, table, target, {context, block, length}) != length ? 1u : 0u;
    }
    put8(rt, sp + 8u, result);
    ctx.gpr[2] = context;
    ctx.gpr[3] = word(rt, sp + 8u);
}

} // namespace

// v0 is compared for the Word ones, both halves for the pair 0x08C5F1C8 returns.
#define NEW205(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_205::name, gt::ReturnKind::kind)
NEW205(0x0895A0D8u, func_0895A0D8, Word);
NEW205(0x0899FAB0u, func_0899FAB0, Word);
NEW205(0x08B4235Cu, func_08B4235C, Word);
NEW205(0x08C5F1C8u, func_08C5F1C8, DoubleWord);

} // namespace new_batch_main_205
} // namespace gt::game

// ---- from main-session batch 206 (draft ab844539790f85f7a56921a7346f379f762478f9) ----
// New batch 206 (main session): functions of 136-139 instructions the gate could not run with useful inputs before.
// Structured C written by hand from `tools.disasm` (Ghidra pseudocode read as an aid only); role words describe
// access shapes, none is a game name. Each body declares its GuestFrame on the signature line so that
// `tools.frame_audit` sees it (it reads the frame from the first line of a body).
// Input shapes for the synthetic generator were written for these addresses in the session's batch report (the format
// and examples are in config/synth_shapes.txt and host/synth_case.cpp).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_206 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The callee's v0 / v1 / f0 become the caller's, as after a `jal` (the next instruction reads them).
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
// A `jal` whose arguments are a0-a3, t0-t3 in order: what the callee does not take stays as the caller had it.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
inline u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 bswap32(u32 value) { return __builtin_bswap32(value); }
// The `sra n; mult magic` count of 12-byte elements from a byte distance.
inline u32 elements12(u32 distance) { return static_cast<u32>(signed_of(distance) >> 2) * 0xAAAAAAABu; }

// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr u32 kAssign = 0x08A5BAD8u;
constexpr u32 kDestroy = 0x08A5B9ECu;
// Its refcounted-string empty holder and the release helper next to it.
constexpr u32 kEmptyHolder = 0x08D74FD0u;
constexpr u32 kFreeString = 0x08B8542Cu;
// The library's failed-assert helper; 0x08CB30B8 is its "HObject" text, 0x08CB39CC its "MDeclValue" one.
constexpr u32 kAssert = 0x08A5BB7Cu;
constexpr u32 kObjectText = 0x08CB30B8u;
constexpr u32 kValueText = 0x08CB39CCu;

// --- func_08C2B54C ---
// 0x08C2B54C (a0 = a vector, a1 = another vector): the libstdc++ vector<12-byte element>::operator=(const vector&) —
// returns a0. An element is {refcounted word +0, word +4, refcounted word +8}; the copy assignment of an element is
// func_08A6A23C for the first member and func_08A5BAD8 for the second (skipped when it is the same member). With room
// the first min(n, size) elements are assigned and the surplus destroyed (func_08C2B500), or the size elements are
// assigned and the rest copy-constructed from the source's tail (func_08C2AED8); without room a block of the source's
// size is made (func_08A5BC20(bytes, 4)), the source copy-constructed into it, the old range destroyed
// (func_08C2AE8C) and freed (func_08A5BC54), and begin / capacity become the block and its end. self->+4 = self->+0 +
// n * 12.
void func_08C2B54C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    const auto assign_element = [&](u32 target, u32 source) {
        call_set(rt, ctx, 0x08A6A23Cu, {target, source});
        put(rt, target + 4u, word(rt, source + 4u));
        if (target + 8u != source + 8u) call_set(rt, ctx, kAssign, {target + 8u, word(rt, source + 8u)});
    };
    const u32 begin = word(rt, self);
    const u32 end = word(rt, self + 4u);
    const u32 capacity_end = word(rt, self + 8u);
    const u32 source_begin = word(rt, other);
    const u32 source_end = word(rt, other + 4u);
    const u32 count = elements12(source_end - source_begin);
    const u32 capacity = elements12(capacity_end - begin);
    if (capacity < count) {
        const u32 bytes = (source_end - source_begin) & ~3u;
        const u32 block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        call_set(rt, ctx, 0x08C2AED8u, {source_begin, source_end, block, frame.at(0u)});
        call_set(rt, ctx, 0x08C2AE8Cu, {begin, end, frame.at(3u)});
        if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, (capacity_end - begin) & ~3u, 4u});
        put(rt, self + 8u, block + bytes);
        put(rt, self, block);
        put(rt, self + 4u, block + bytes);
        ctx.gpr[2] = self;
        return;
    } else {
        const u32 size = elements12(end - begin);
        u32 target = begin;
        u32 source = source_begin;
        if (size < count) {
            for (u32 done = 0u; signed_of(size) > 0 && done != size; ++done) {
                assign_element(target, source);
                target += 12u;
                source += 12u;
            }
            call_set(rt, ctx, 0x08C2AED8u, {source_begin + ((end - begin) & ~3u), source_end, end, frame.at(1u)});
        } else {
            for (u32 done = 0u; signed_of(count) > 0 && done != count; ++done) {
                assign_element(target, source);
                target += 12u;
                source += 12u;
            }
            call_set(rt, ctx, 0x08C2B500u, {target, end, frame.at(2u)});
        }
    }
    put(rt, self + 4u, word(rt, self) + count * 12u);
    ctx.gpr[2] = self;
}

// --- func_08C32AFC ---
// 0x08C32AFC (a0 = a list owner, a1 = a descriptor, a2 = a handle): the descriptor's count (func_08C319D8 into a
// frame value) drives one of three shapes. With a null handle the frame value is replaced by func_08A98C1C's answer and
// the whole list is stored from it, then for each index a frame temporary (func_08A76AA0) goes through
// func_08C32AD4 and into the handle's virtual slot 0x88 (asserting "HObject" when the handle is null). With a handle,
// its virtual slot 0x78 decides: non-zero walks the indices through slot 0x8C (an element into the temporary), assigns
// it (func_08C32AD4), and when the temporary's first word is clear also inserts it through slot 0x88; zero only assigns
// each index (func_08C32AD4) without touching the handle. Every temporary is destroyed (func_08A5B9EC).

// --- func_08C35A60 ---
// 0x08C35A60 (a0 = a container, a1 = the end of its element range): the libstdc++ insert that walks the 12-byte
// elements from a0 + 0xC to a1 and puts each one in place. Per element the key word (frame +0xC), a copy of the
// element's second member (func_08AAE214) and its byte gate (frame +0x14) are made; against the container's object at
// a0 + 4 (through the rank at +0x14 of both, and then func_08B35FD8(key, a0->+0)) one of two things happens: the
// elements before the position move up by one (word, member through func_08A5BAD8 unless it is the same member, byte)
// and the copy becomes the container's first element, or a second copy (frame +0) is built and handed to
// func_08C35920(element, copy, container). Each turn destroys the temporary. v0 is the last callee's.
// Its caller 0x08C35C80 divides (a1 - a0) by 12 before calling it, which is where the 12-byte stride and the
// [a0 + 0xC, a1) range come from.

// --- func_089E0B3C ---
// 0x089E0B3C (a0 = an object, a1 = a value object, a2 = a mode): with a zero mode nothing happens. The object's state
// (func_08A06EC8) picks the shape. State 1: the value's slot 0x108 is called with func_08A06E94(a0, 0)'s answer, then
// with flag 0x20 set, flag 0x40 clear, or |a0->+0x11C| at least 1.1920929e-07 the detail call func_089E0768 runs with
// no matrix (a3 = 0) and mode = (a0->+0x1A0 >> 6) & 3; otherwise func_089A8B20(a1, a1) gives a vector whose |+4| and
// |+0xC| are compared the same way, the last case being func_089E02C4(a0, a1, a2, mode). State 4: a frame 4x4 identity
// (func_089641C4 from func_08A06E94(a0, 0..3)) is passed to func_089E0768 as a3, with mode as t0. (On the one path
// where the asm jumps into the detail call past `a0 = a0`, a0 is the virtual callee's leftover — modelled as ctx.gpr[4].)
void func_089E0B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    if (mode == 0u) return;
    constexpr float kEpsilon = std::bit_cast<float>(0x34000000u);
    const auto detail = [&](u32 matrix, u32 first) {
        call_setf(rt, ctx, 0x089E0768u, {first, value, mode, matrix, (word(rt, self + 0x1A0u) >> 6) & 3u}, {0.0f, 0.0f});
    };
    if (call_set(rt, ctx, 0x08A06EC8u, {self}) != 1u) {
        if (call_set(rt, ctx, 0x08A06EC8u, {self}) != 4u) return;
        for (u32 at = frame.at(0u); at != frame.at(0x40u); at += 0x10u) {
            put_float(rt, at, 1.0f);
            put_float(rt, at + 4u, 1.0f);
            put_float(rt, at + 8u, 1.0f);
            put_float(rt, at + 0xCu, 1.0f);
        }
        for (u32 index = 0u; index < 4u; ++index) {
            const u32 answer = call_set(rt, ctx, 0x08A06E94u, {self, index});
            call_set(rt, ctx, 0x089641C4u, {frame.at(index * 0x10u), answer});
        }
        detail(frame.at(0u), self);
        return;
    }
    const u32 entry = word(rt, word(rt, value) + 0x108u);
    const u32 argument = call_set(rt, ctx, 0x08A06E94u, {self, 0u});
    call_indirect(rt, ctx, entry, {value, argument});
    const u32 flags = word(rt, self + 0x1A0u);
    if ((flags & 0x20u) != 0u) {
        detail(0u, self);
        return;
    }
    if ((word(rt, self + 0x1B0u) & 0x40u) == 0u) {
        detail(0u, self);
        return;
    }
    if (kEpsilon <= std::fabs(float_at(rt, self + 0x11Cu))) {
        detail(0u, ctx.gpr[4]);  // the asm reuses a0 as the virtual callee left it
        return;
    }
    const u32 result = call_set(rt, ctx, 0x089A8B20u, {value, value});
    if (kEpsilon <= std::fabs(float_at(rt, result + 4u))) {
        detail(0u, self);
        return;
    }
    if (kEpsilon <= std::fabs(float_at(rt, result + 0xCu))) {
        detail(0u, self);
        return;
    }
    call_set(rt, ctx, 0x089E02C4u, {self, value, mode, (flags >> 6) & 3u});
}

// --- func_08A3E02C ---
// 0x08A3E02C (a0 = the out value): GT names this adhoc native MGpb::getKeyList (config/symbol_addrs.txt, registrar
// 0x383108). A value object from func_08A3D8B0 is unwrapped (its word +0x14, asserting when
// null), destroyed, and its record read: the count is the word at +0xC of the first record word, or, when that word is
// clear, of the block at +4 with the bytes reversed. The out value becomes a container of that many slots
// (func_08A98C1C) and for each slot a two-slot container (func_08A98C1C(_, 2)) is filled: slot 0 a string from the
// record's key (word +0x10, or the reversed word +0x20 of the block), slot 1 the int64 from the record's value (word
// +0x18, or the reversed word +0x28), both through func_08B85EB8 / func_08A6D2D4 / func_08A83160 with the value-object
// assign and destroy of each temporary and the string release (empty holder 0x08D74FD0, free func_08B8542C); then the
// container is assigned into the out value's slot and destroyed. Finally the out value gets the container and it is
// destroyed.

// --- func_08C60DE8 / func_08C61010 ---
// 0x08C60DE8 and 0x08C61010 (a0-a3, t0, t1): the libstdc++ locale pair step — byte-identical bodies except the
// `lw t2` offset (0x8 / 0x10 that tell which of the facet's two lookups answers). The facet found by func_08B5C2B0 in
// the table at a3 + 0x6C (func_08B5A9FC when out of range or empty) gives t2; func_08C5FBAC(a0, {a1 pair}, {a2 pair},
// a3, t0, t1, t2) answers a pair (v0, v1). When v0 is set and v1 is -1 the left side is asked for a character: the
// byte at its current position (+8 below +0xC) or the object's virtual slot 0x24; -1 turns it into the end state
// (v0 = 0, v1 = -1). Then, with the right side's first word clear or its state -1, the same read for the right side
// (slot 0x24 through +0). Finally, when both sides are at their end or both are not, the flag word t0 points to gets
// bit 2, and (v0, v1) is returned.
void read_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, u32 facet_offset) {
    const u32 a = ctx.gpr[4];
    const u32 c = ctx.gpr[7];
    const u32 d = ctx.gpr[8];
    const u32 e = ctx.gpr[9];
    put(rt, frame.at(0x30u), d);
    const u32 left_first = word(rt, ctx.gpr[5]);
    const u32 left_second = word(rt, ctx.gpr[5] + 4u);
    const u32 right_first = word(rt, ctx.gpr[6]);
    const u32 right_second = word(rt, ctx.gpr[6] + 4u);
    const u32 index = call_set(rt, ctx, 0x08B5C2B0u, {0x08D77478u});
    const u32 table = word(rt, c + 0x6Cu);
    u32 extra = 0u;
    if (!(index < word(rt, table + 8u))) {
        call_set(rt, ctx, 0x08B5A9FCu, {});
    } else {
        const u32 entry = word(rt, word(rt, table + 4u) + index * 4u);
        if (entry == 0u) {
            call_set(rt, ctx, 0x08B5A9FCu, {});
        } else {
            extra = word(rt, word(rt, entry + 8u) + facet_offset);
        }
    }
    put(rt, frame.at(0x14u), left_second);
    put(rt, frame.at(0x10u), left_first);
    put(rt, frame.at(0xCu), right_second);
    put(rt, frame.at(8u), right_first);
    const GuestResult result = call_keep(rt, ctx, 0x08C5FBACu, {a, frame.at(0x10u), frame.at(8u), c, d, e, extra});
    u32 ret_first = result.v0;
    u32 ret_second = result.v1;
    put(rt, frame.at(0x20u), result.v0);
    put(rt, frame.at(0x24u), result.v1);
    put(rt, frame.at(0x10u), result.v0);
    put(rt, frame.at(0x14u), result.v1);
    const auto read = [&](u32 state) -> u32 {
        // `sgetc` of the streambuf at `state`: the current byte, else its slot 0x24.
        if (word(rt, state + 8u) < word(rt, state + 0xCu)) return byte_u(rt, word(rt, state + 8u));
        return call_indirect(rt, ctx, word(rt, word(rt, state) + 0x24u), {state});
    };
    u32 left_char = 0xFFFFFFFFu;
    if (result.v0 != 0u) {
        left_char = result.v1;
        if (result.v1 == 0xFFFFFFFFu) {
            left_char = read(result.v0);
            ret_second = left_char;
            if (left_char == 0xFFFFFFFFu) {
                ret_first = 0u;
                ret_second = 0xFFFFFFFFu;
            }
        }
    }
    u32 right_state = right_second;
    if (right_first == 0u) {
        right_state = 0xFFFFFFFFu;
    } else if (right_state == 0xFFFFFFFFu) {
        right_state = read(right_first);
    }
    const bool right_at_end = right_state == 0xFFFFFFFFu;
    const bool left_at_end = left_char == 0xFFFFFFFFu;
    if (left_at_end == right_at_end) put(rt, word(rt, frame.at(0x30u)), word(rt, word(rt, frame.at(0x30u))) | 2u);
    ctx.gpr[2] = ret_first;
    ctx.gpr[3] = ret_second;
}
void func_08C60DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x70u);
    read_pair(rt, ctx, frame, 8u);
}

// --- func_0890265C ---
// 0x0890265C (a0 = an animation record, a1 = a key, a2 = a cursor struct {index +0, +4}): the cursor's +4 index, when
// it is positive, selects the frame range: through the table at +0x90 and its pairs of signed halfwords at +8 / +0xA
// when a0 + 0x8C and a0 + 0x90 are set, else the packed word at a0 + index + 4 (start = >> 16, end = start + low half).
// A cursor at 0 asks func_08902554(a0, frame, a1) for it instead (returning 0.0f when that fails) and writes it back.
// Over the range, each pair of frames (40-byte records from a0 + 0x58, index halves from a0 + 0x60) is measured by
// func_08905594 twice (word 0..0xC and 0xC..0x18 of both): the answer's |frame + 8| is compared with the best so far
// (1.4e38 first), and the better one blends the frames' words +0x24 across the wrap at a0 + 0x14 by frame + 0
// (subtracting the wrap when it exceeds it). f0 = the best blend (0.0 when no pair qualifies).
void func_0890265C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x80u);
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 cursor = ctx.gpr[6];
    const s32 requested = signed_of(word(rt, cursor + 4u));
    u32 index = static_cast<u32>(requested);
    if (requested <= 0) {
        put(rt, frame.at(0x34u), 0xFFFFFFFFu);
        put(rt, frame.at(0x30u), 0u);
        if (call_set(rt, ctx, 0x08902554u, {self, frame.at(0u), key}) == 0u) {
            ctx.fpr[0] = 0.0f;
            return;
        }
        put(rt, cursor + 4u, word(rt, frame.at(0x34u)));
        put(rt, cursor, word(rt, frame.at(0x30u)));
    }
    u32 start;
    u32 stop;
    if (word(rt, self + 0x8Cu) != 0u && word(rt, self + 0x90u) != 0u) {
        const u32 record = (self + index) - (index << 2) + word(rt, self + 0x90u);
        start = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(record + 8u))));
        stop = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(record + 0xAu)))) + start;
    } else {
        const u32 packed = word(rt, self + index + 4u);
        start = packed >> 16;
        stop = start + (packed & 0xFFFFu);
    }
    float best = 0.0f;
    if (signed_of(start) < signed_of(stop)) {
        float best_magnitude = std::bit_cast<float>(0x7F7FFFFFu);
        const u32 count = stop - start;
        u32 position = start << 1;
        for (u32 done = 0u; done != count; ++done) {
            const u32 first = static_cast<u32>(
                static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(word(rt, self + 0x60u) + position))));
            const u32 records = word(rt, self + 0x58u);
            const u32 next_index = first + 1u;
            const u32 next_offset = next_index != word(rt, self + 0x54u) ? next_index * 40u : 0u;
            const u32 current = records + first * 40u;
            const u32 following = records + next_offset;
            bool found = call_set(rt, ctx, 0x08905594u,
                                  {frame.at(0u), key, current, following, current + 0xCu, following + 0xCu}) != 0u;
            if (!found) {
                found = call_set(rt, ctx, 0x08905594u,
                                 {frame.at(0u), key, current + 0xCu, following + 0xCu, current + 0x18u, following + 0x18u}) != 0u;
            }
            if (found) {
                const float magnitude = std::fabs(float_at(rt, frame.at(8u)));
                if (magnitude < best_magnitude) {
                    float low = float_at(rt, current + 0x24u);
                    float high = float_at(rt, following + 0x24u);
                    const float wrap = float_at(rt, self + 0x14u);
                    if (high < low) high += wrap;
                    best = low + (high - low) * float_at(rt, frame.at(0u));
                    best_magnitude = magnitude;
                    if (wrap < best) best -= wrap;
                }
            }
            position += 2u;
        }
    }
    ctx.fpr[0] = best;
}

// --- func_08A7F3D0 ---
// 0x08A7F3D0 (a0 = a map, a1, a2 = the caller's, a3 = a doubly-linked list head): func_08A76E94(a0..a3) runs first.
// When the list is empty (*a3 == a3) the work goes to func_08A7D1F8(a0, a2). Otherwise the head node's name (its word
// +8) is copied into the frame by func_08B88F44, a value object (func_08A5D438, frame +0xC) is filled from it
// (func_08A69654 copy, func_08A7D5FC(a0, object, copy), func_08A69D68 destroy of the copy) and, when the object is set,
// an empty temporary (func_08A76AA0, frame +0x10) goes through the object's virtual slot 0x1C(a1). If the temporary
// came back set, the head node is unlinked (func_08B5C094), its name released (empty holder 0x08D74FD0, func_08B60804 /
// 0x08B8542C) and the node freed (func_08A5BC54, 0xC, 4); the temporary's virtual slot 0x100(a1, a2, a3) is called, then
// a 12-byte node is made (func_08A5BC20(0xC, 4)) whose +8 is the copied name (func_08B88F44) and which is linked in
// front of the head's next (func_08B5C078). Both the temporary and the object are destroyed (func_08A5B9EC) and the
// copied name released. The assert texts are "HValue" 0x08CB1358 and "HObject" 0x08CB1350.
void func_08A7F3D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 list = ctx.gpr[7];
    call_set(rt, ctx, 0x08A76E94u, {self, first, second, list});
    const u32 name = frame.at(8u);
    const u32 object = frame.at(0xCu);
    const u32 copied = frame.at(4u);
    const u32 temporary = frame.at(0x10u);
    const auto release_name = [&](u32 text, u32 scratch) {
        if (text - 0xCu != kEmptyHolder &&
            !(signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0)) {
            call_set(rt, ctx, kFreeString, {text - 0xCu, scratch});
        }
    };
    if (word(rt, list) == list) {
        call_set(rt, ctx, 0x08A7D1F8u, {self, second});
        return;
    }
    call_set(rt, ctx, 0x08B88F44u, {name, word(rt, list) + 8u});
    call_set(rt, ctx, 0x08A5D438u, {object});
    call_set(rt, ctx, 0x08A69654u, {copied, name});
    call_set(rt, ctx, 0x08A7D5FCu, {self, object, copied});
    call_set(rt, ctx, 0x08A69D68u, {copied});
    if (word(rt, object) != 0u) {
        call_set(rt, ctx, 0x08A76AA0u, {temporary});
        const u32 handle = word(rt, object);
        if (handle == 0u) call_set(rt, ctx, kAssert, {0x08CB1358u});
        call_virtual_at(rt, ctx, handle, 0x1Cu, {handle, temporary, first});
        if (word(rt, temporary) != 0u) {
            const u32 node = word(rt, list);
            call_set(rt, ctx, 0x08B5C094u, {node});
            release_name(word(rt, node + 8u), frame.at(1u));
            call_set(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
            const u32 handle2 = word(rt, temporary);
            if (handle2 == 0u) call_set(rt, ctx, kAssert, {0x08CB1350u});
            call_virtual_at(rt, ctx, handle2, 0x100u, {handle2, first, second, list});
            const u32 next = word(rt, list);  // the asm reads *list in the allocator call's delay slot
            const u32 fresh = call_set(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
            if (fresh + 8u != 0u) call_set(rt, ctx, 0x08B88F44u, {fresh + 8u, name});
            call_set(rt, ctx, 0x08B5C078u, {fresh, next});
        }
        call_set(rt, ctx, kDestroy, {temporary});
    }
    call_set(rt, ctx, kDestroy, {object});
    release_name(word(rt, name), frame.at(0u));
}

} // namespace
} // namespace new_batch_206
} // namespace gt::game

#define NEW206(address, name, kind) GT_DECOMP(address, gt::game::new_batch_206::name, gt::ReturnKind::kind)
NEW206(0x08A7F3D0u, func_08A7F3D0, Word);
NEW206(0x089E0B3Cu, func_089E0B3C, Word);
NEW206(0x0890265Cu, func_0890265C, Float);
NEW206(0x08C2B54Cu, func_08C2B54C, Word);
NEW206(0x08C60DE8u, func_08C60DE8, DoubleWord);

// ---- from main-session batch 207 (draft 61783045b0b2c5d6498c9506d63aceb49c3989ec) ----
// New batch main 207: functions the earlier batches wrote only as register-level bodies (or not at all), rewritten as
// structured C from `tools/disasm`. Draft batch: nothing here is verified yet; the differential gate and the synthetic
// cases decide. The input shapes the synthetic generator needs are in config/synth_shapes.txt (or the batch's shape
// file) so a pointer argument gets a valid struct instead of an arena word.
//
// Left out of this batch, with the reason:
//  - 0x08B57F2C: a 4x4 matrix inverse written entirely in VFPU (lv.q/vmul.q/vtfm4.q/vrcp.s, `bvt 0`), including the
//    vpfx/vpft swizzles per column; it needs a VFPU model (include/gt/vfpu.hpp) before a body can be read at all.
//  - 0x08B044C0: reads words of its own frame that nothing writes (`lw v0, 0x20(sp)`, `0x10(sp)`, `0x0(sp)` before any
//    store to them) and copies them into the record it passes on; the original's behaviour comes from the caller's dead
//    stack, which the harness excludes.
//  - 0x08C773A4: allocates with `subu sp, sp, v0` (a variable-length array) and passes pointers into that area as
//    arguments (t1/t2) to a virtual method; the frame is not a fixed GuestFrame.
//  - 0x08C16480: a heap sift-down whose blocks are entered from the middle (a jump lands between the float moves), so a
//    structured body would need a state machine to keep the exact store order.
//
// Conventions as in the other batch files: a call passes the argument registers the original sets before the jal (the
// rest keep the caller's value, as the original's caller-saved registers do), the delay slot of a call is modelled
// before the call, v0 is what the asm leaves on every exit, and the frame stores the callees read are reproduced.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_207 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them: the shapes below read the callee's v1 (the high word of a
// 64-bit result) and f0 (a float result), and a callee that never sets v0 hands the previous one on.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments, {}).v0;
}
// The same with float arguments (f12-f15).
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments, float_arguments);
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (hot_common's rule).
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments,
                         std::initializer_list<float> float_arguments) {
    return call_indirect(rt, ctx, word(rt, word(rt, object) + offset), arguments, float_arguments);
}

inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// ---- 0x088BCE94 (a0 = state object, f12 = a speed): a 0..1 factor from the record and the block the object points at.
// v1 = 0x088C50B8(sp, word(a0+0x1C), byte(*(a0+0x24)+0x10)) fills sp[0]; 0x088C5324(sp, byte(+0x11) or 0 when
// sp[0] ^ byte(+0x11) is 0) keys it and 0x088C50C4(sp, key) answers the record. The distance is the length of
// (record+4, record+8) - (block+0x10, block+0x14) through 0x08B49D7C; it is divided by (1 + record[1] * 0.01)^2 when
// that is above 1. Then (factor = 0x088C4A30(a0)) * byte(*(a0+0x24)+0x50) is taken off, and when what is left is not
// positive the answer is 1.0f. Otherwise: the answer is `f4` clamped to 0..1 (a = 2 * the remainder, n = f12^2 -
// (*(a0+8)+0x6C4)^2, v = (2 * (*(a0+4)+0x13AC)) * -(n / a), clamped) when |f4 - clamped(base + (2 * v * (0x6B4 - n) +
// 0.05) * 0.4)| is above 0.35, and that clamped sum otherwise.

// ---- 0x088E9838 (a0 = state object, a1 = from, a2 = to): the ratio the object stores for the pair (from, to).
// The two records are fetched through 0x088DA788(a0, entry): sp[8] = record[0x14] (the word), sp[0xC] = float(record)
// for `from`, sp[0x10] / sp[0x14] the same for `to`; when 0x088D4C9C(a0+4) is not 0 the word at +0x18 replaces those of
// sp[8] / sp[0x10]. 0x088D0F98(sp+8, sp+0x10; f12 = 0x088FB028(0x088DB5C4(a0))) gives (v0, v1) - the count and a float in
// v1 - stored at sp[0] / sp[0x20] / sp[0x24] and sp[4]. The magnitude is |float(sp[4])| with sign = (v0 < 0 or that float
// < 0): when the count is positive, or when 0x088D4F64(a0+4, sign) <= the magnitude, the value is
// 0x088D4E84(a0+4, sign) + 1.0f; when 0x088D4F00(a0+4, sign) (second read) is not below the magnitude it is 1.0f;
// otherwise (magnitude - A) / (B - C) * D + 1.0f, A/C = 0x088D4F00, B = 0x088D4F64, D = 0x088D4E84, all of (a0+4, sign).
// The value goes to float(a0, to)+0x50 and to the virtual at slot 0x74 of word(a0+0x148) (a1 = to, f12 = the value).

// ---- 0x08AC3BA8 (a0 = state object): the state-machine step. 0x08AC2EDC(sp) builds the 0x180-byte block at sp; every
// round 0x08B2DE00() reports whether to wait (0x08B2E2B8(1000000) and round again), and a nonzero byte at a0+5 ends the
// step through 0x08AC2F54(sp). Otherwise the command word at a0+8 is cleared to -1 and dispatched (0..5:
// 0 = 0x08AC2EEC(sp) + state 0; 1 = keep state 1, else state = sp+0xC, call 0x08AC391C(a0) unless state was 2, then the
// shared block; 2 = 0x08AC2F5C(sp) + state 2; 3 = 0x08AC2EEC(sp), 0x08AC391C(a0), state = sp+0xC, shared block;
// 4 = nothing; 5 = 0x08AC2F90(sp; f12 = float(a0+0x10)), state 3). The shared block: 0x08AC3A98(a0, state, 0x180) is 0
// -> state 1, else 0x08AC338C(sp, state, 0; f12 = 0, f13 = float(a0+0xC)) and state 1. Then 0x08AC2FE0(sp): when 0,
// state 1 does 0x08AC2EEC(sp), 0x08AC391C(a0) and either 0x08B2E2B8(1000000) or the shared 0x08AC3A98 / 0x08AC338C
// pair, and state 3 does 0x08AC2EEC(sp) + state 0; when not 0 and float(a0+0xC) changed, 0x08AC2FB8(sp; f12 = the new
// value). Every round ends with 0x08B2E2B8(50000).
void func_08AC3BA8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x1B0u);
    const u32 local = frame.at(0u);
    call(rt, ctx, 0x08AC2EDCu, {local});
    float speed = float_at(rt, self + 0xCu);
    u32 state = 0u;
    for (;;) {
        if (rt.memory().load8(self + 5u) != 0u) {
            call(rt, ctx, 0x08AC2F54u, {local});
            return;
        }
        if (call(rt, ctx, 0x08B2DE00u) != 0u) {
            call(rt, ctx, 0x08B2E2B8u, {0xF4240u});
            continue;
        }
        const u32 command = word(rt, self + 8u);
        bool rebuild = false;
        if (signed_of(command) >= 0) {
            put(rt, self + 8u, 0xFFFFFFFFu);
            if (command < 6u) {
                switch (command) {
                case 0u:
                    call(rt, ctx, 0x08AC2EECu, {local});
                    state = 0u;
                    break;
                case 1u: {
                    if (state == 1u) break;
                    const bool advance = state == 2u;
                    state = local + 0xCu;
                    if (!advance && rt.memory().load8(self + 0xCA8u) != 0u) call(rt, ctx, 0x08AC391Cu, {self});
                    rebuild = true;
                    break;
                }
                case 2u:
                    call(rt, ctx, 0x08AC2F5Cu, {local});
                    state = 2u;
                    break;
                case 3u:
                    call(rt, ctx, 0x08AC2EECu, {local});
                    call(rt, ctx, 0x08AC391Cu, {self});
                    state = local + 0xCu;
                    rebuild = true;
                    break;
                case 5u:
                    call_setf(rt, ctx, 0x08AC2F90u, {local}, {float_at(rt, self + 0x10u)});
                    state = 3u;
                    break;
                default:
                    break;
                }
            }
        }
        if (rebuild) {
            if (call(rt, ctx, 0x08AC3A98u, {self, state, 0x180u}) == 0u) {
                state = 1u;
            } else {
                call_setf(rt, ctx, 0x08AC338Cu, {local, state, 0u}, {0.0f, speed});
                state = 1u;
            }
        }
        if (call(rt, ctx, 0x08AC2FE0u, {local}) == 0u) {
            if (state == 1u) {
                call(rt, ctx, 0x08AC2EECu, {local});
                if (call(rt, ctx, 0x08AC391Cu, {self}) == 0u) {
                    call(rt, ctx, 0x08B2E2B8u, {0xF4240u});
                } else if (call(rt, ctx, 0x08AC3A98u, {self, local + 0xCu, 0x180u}) != 0u) {
                    call_setf(rt, ctx, 0x08AC338Cu, {local, local + 0xCu, 0u}, {0.0f, speed});
                }
            } else if (state == 3u) {
                call(rt, ctx, 0x08AC2EECu, {local});
                state = 0u;
            }
        } else {
            const float now = float_at(rt, self + 0xCu);
            if (speed != now) {
                speed = float_at(rt, self + 0xCu);
                call_setf(rt, ctx, 0x08AC2FB8u, {local}, {speed});
            }
        }
        call(rt, ctx, 0x08B2E2B8u, {0xC350u});
    }
}

// ---- 0x08BB2B00 (a0 = holder with the slot array end at +4, a1 = the slot array to store, a2 = the source array): the
// assign-and-release pair for `count = (word(a0+4) - a2) / 4` entries. For each entry: the new node's use count (+0x10)
// goes up, the slot takes it, and when the old node's count reaches 0 it is destroyed - its word at +0xC unlinked from
// the list at +0xC (count +0x1C going down), the word at +0xC cleared, 0x08BB1FF0(node), then the two holder objects of
// the block 0x08BB1F48() (+0x34 rebuilt through 0x08BB1D0C(object, +0x28, sp+4, 0x08BB1B40) with the wanted type 4 and
// then 0x14) are unlinked and the node released. Then 0x08BB24B0(a1, word(a0+4), sp) and word(a0+4) -= (a2 - a1) & ~3;
// v0 = a1.

// ---- 0x08C496A4 (a0 = first, a1 = last, a2 = depth, a3 = comparator words): the introsort loop over a range of halfword
// indices [first, last) ordered by a float key. comp points at two words copied to sp[0] / sp[4] (the callees get sp);
// the key of the index h (a signed halfword) is the float at (*word(comp+4)) + 240 * h + 4 * word(comp) + 0x98. A range
// of fewer than 17 elements returns v0 = last - first. Otherwise, with remaining = depth and round = 0: when round
// reaches depth the heap fallback 0x08C49524(first, last, last, sp) ends it (v0 = its result); else remaining -= 1, the
// pivot is the median of the keys of first, mid = first + ((n + (n >> 31)) & ~1) and last - 2, p = 0x08C491E4(first,
// last, pivot, sp) partitions, 0x08C496A4(p, last, remaining, sp) sorts the upper part, last = p, round += 1 and the
// loop goes on while the lower part has 17 or more elements (v0 = 1 at that exit).
void func_08C496A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 depth = ctx.gpr[6];
    const u32 comp = ctx.gpr[7];
    u32 last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const u32 object = word(rt, comp + 4u);
    const u32 field = word(rt, comp);
    put(rt, frame.at(4u), object);
    put(rt, frame.at(0u), field);
    u32 count = static_cast<u32>(signed_of(last - first) >> 1);
    ctx.gpr[2] = last - first;
    if (signed_of(count) < 0x11) return;
    const u32 key_offset = field << 2u;
    u32 remaining = depth;
    for (u32 round = 0u;; ++round) {
        if (depth == round) {
            (void)call_set(rt, ctx, 0x08C49524u, {first, last, last, frame.at(0u)});
            return;
        }
        const u32 mid = first + ((count + (count >> 31u)) & ~1u);
        const u32 base = word(rt, object);
        const auto key = [&](u32 at) {
            const u32 index = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(half_u(rt, at))));
            return float_at(rt, base + index * 240u + key_offset + 0x98u);
        };
        const float middle = key(mid);
        const float head = key(first);
        --remaining;
        const u32 tail = last - 2u;
        u32 pick;
        if (head < middle) {
            const float back = key(tail);
            if (middle < back) pick = mid;
            else if (head < back) pick = tail;
            else pick = first;
        } else {
            const float back = key(tail);
            if (head < back) pick = first;
            else if (middle < back) pick = tail;
            else pick = mid;
        }
        const u32 pivot = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(half_u(rt, pick))));
        const u32 split = call_set(rt, ctx, 0x08C491E4u, {first, last, pivot, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08C496A4u, {split, last, remaining, frame.at(0u)});
        count = static_cast<u32>(signed_of(split - first) >> 1);
        last = split;
        const bool small = signed_of(count) < 0x11;
        ctx.gpr[2] = small ? 1u : 0u;
        if (small) return;
    }
}

// ---- 0x08C47728 (a0 = vector, a1 = position, a2 = source): insert the 6-byte element at `position` (3 halfwords).
// A full vector grows: n = ((end - begin) >> 1) * 0xAAAAAAAB (the low half of the signed product, the compiler's
// division by 6 of a pointer difference that is a multiple of 6), a length of 0x2AAAAAAA throws 0x08CB4AD4, the new
// byte count is 6 * max(2n, 1) (0xFFFFFFFC when 2n wraps), 0x08B35DA0 allocates it, 0x08C470B4(begin, position, new;
// sp+2) copies the head, the element is written after it (skipped when that copy answered 0, and then the tail copy
// starts at 6 as the original does), 0x08C470B4(position, end, after; sp+1) copies the tail, 0x08C470A4(begin, end, sp)
// fills the old range and 0x08B35254 frees the old block; begin / end / capacity become new / last / new + bytes.
// Otherwise the last element moves up to `end` (3 halfwords) and the elements [position, end - 6] move up by 6, then the
// element is stored at `position` (a 4-byte word and a halfword, both unaligned).

} // namespace
} // namespace new_batch_main_207
} // namespace gt::game

#define NEW207(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_207::name, gt::ReturnKind::kind)
NEW207(0x08AC3BA8u, func_08AC3BA8, Word);
NEW207(0x08C496A4u, func_08C496A4, Word);
// ((end - 6 - position) >> 1) * 0xAAAAAAAB (the low word of the product), which is the element count only when the
// pointer difference is a multiple of 6 - true in the game, never true for a synthetic pointer pair, so the loop bound
// is garbage and the case hangs instead of comparing.

// ---- from main-session batch 208 (draft 7ba094e3bcb2d388414ae6052c634317c6fdac1c) ----
// Main-session batch 208: ten functions of 141-144 instructions that no earlier batch wrote (`grep -rn "void func_X(" src/`
// found none of the addresses; the hits in src/ are callers or registration tables that name them as callbacks). Each one was
// read with `tools.disasm` (relocations applied, every branch printed as `-> ADDRESS`), in structured C: named locals typed as
// the asm shows them, guest memory through the helpers, a call set through `call_set` / `call_setf` (the argument registers the
// original sets before the jal: a0-a3, t0-t3, f12-f15) and its result kept in v0 / v1 / f0 by `call_keep`, so a callee that
// writes no v0 hands the previous one on exactly as the recompiled original does.
//
// The `jal` delay slots are read as "before the call": where the asm stores a call's result in the delay slot of that same call
// (`addu s0, v0, zero` after `jal 0x08A5BC20`), the C++ keeps the value first; where a value only survives because the delay slot
// of a branch sets it, the code below says so.
//
// What the functions are (the words describe the shape of the accesses; none is a name from the game, except the two
// 0x0897A588 / 0x089C9238, which config/symbol_addrs.txt names from the Adhoc registration chain):
//  - func_0897A588 (MUpdateContext::unloadGpb) and func_089C9238 (MManager::unloadGpb), the same body twice with four different
//    constants: they build a pair and a key holder out of the argument array at a3, make one smart pointer per object through
//    its virtual slot 0xC, merge them (0x089C9024) and release the temporaries. Mode 2 takes both entries of a3, mode 1 only the
//    first; any other mode answers v0 = 2.
//  - func_089B06B0: under the library lock (0x08CF4A04) two lookups of the object (0x089AA5A0 / 0x089AA5FC) are read, and when
//    the two "valid" ones differ, a 0x24-byte record per side is allocated (vtable 0x08CC99B8 / 0x08CC76D8), constructed with
//    (object, side), published through 0x089FF120 into a handle built on the frame and announced with 0x0897783C; both lookups
//    are then released (0x08A5B930) and the lock left.
//  - func_08A7ED20: inserts one record into the map whose header lives at dest[0x10] from the tree node at word(a3): the node's
//    payload (+8, copied with 0x08B88F44) becomes a holder (0x08A5D438), a pair (0x08A69654) and the insert at slot 0x1C
//    (0x08A7D5FC); then the node's string and 0xC-byte block go, a record is allocated, filled through the virtual slot 0x100 and
//    0x08B5C078, and a key copy (0x08B88F44) is pushed as the new node.
//  - func_08B8B7B4: the sift-down of a heap of 24-byte records ([+0] [+4] two words, [+8] a three-word vector): while the child
//    index is below count the smaller child (0x08B8A72C) is moved into the hole ([+0] [+4] copied, the vector with 0x08B8AD88),
//    a lone left child is moved up, and the value a3 points at is copied to a temporary record, pushed (0x08B8B630) and freed.
//  - func_0897C460: a distance-driven trigger. The record is s2 = a1 + a0 and starts at the object a2 (0x089AAC8C); the length is
//    sqrt((float)a3 * a3 + (float)t0 * t0), 0 carried into an angle by 0x08B66FBC, and the direction index is
//    trunc((angle / pi + 180 + k * 22.5) / (k * 45)) with k = 1 or 2 (bit 0x20 of the object's +0x138). The byte counter at
//    +0x148 advances by trunc(length / 10) + 1 (reset to 5 when the index changed); at 10 an object is built (0x08A5BC20, 0x40
//    bytes, constructor 0x089D6378 with the table entry 0x08CDE768 + 4 * index and the angle pair of 0x089AA95C) and handed to a
//    caller-supplied callback (0x0897783C).
//  - func_088BD6BC: a float weight. (f21 + 0.5) * 0.5 when the byte at word(a2[4]) + 0x578 or the byte at word(a2[0]) + 0x13E2 is
//    set; two 0x0894268C lookups over the direction byte a0[0x12] blend a * (mode0) + (1 - a) * (mode1), all times the record's
//    +0x1350 and 0.9 when the flag was set; the pair (a2[0x18], a2[0x20]) tilts it by +/-9.8 through 0x08B498AC before the object's
//    +0x1AC scales it (0x08B49D7C = the square root, 0x088BB4B8 the angle term) and the result is floored at 0.1.
//  - func_088616A8: the table update of a record of 0x1C38 bytes (see its comment).
//  - func_089C9CE4: an erase-by-key over the container at a0 with an out-parameter at a1 (see its comment).
//  - func_08A7A3D4: the copy of a map tree (see its comment).
//
// Every exit sets v0 / f0 as the asm leaves it, including the constants a test leaves in v0. Registers the original restores
// (s0-s7, fp) are never written here, so no SavedRegisters; f20-f22 are C++ locals, not context registers. The dead stack
// (spilled arguments nothing reads) is not reproduced; the frames callees read are.
//
// Static audits on this file (no differential gate ran: this is a main-session draft, so the registration lines are live but
// unproven): `tools.frame_audit` 0 wrong; `tools.literal_check` 0 literals missing; `tools.sp_loads` lists 10 loads of
// s-registers from a frame slot they were not saved to, and each one is a value the asm keeps on the frame and the C++ reads
// back with `word(rt, frame.at(off))` (the pair at sp + 0x1C / 0x10 of the two Adhoc natives, the iterator and key of
// 0x089C9CE4, the temporary and the hint holder of 0x08A7ED20, the saved a1 of 0x08B8B7B4); `tools.arg_reads` lists one call
// (0x089AAC8C reads a1) that is harmless: the original never writes a1 before that jal either, so the callee sees the function's
// own a1 (the record offset), and the C++ leaves ctx.gpr[5] untouched.
// The synthetic input shapes this batch needs are written per address; the two Adhoc natives need a3 pointing at two objects,
// the rest are named at the top of each function's comment.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_208 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;

using u32 = std::uint32_t;
using s32 = std::int32_t;

// The shared empty representation of the one-byte-character string; its characters follow at +0xC.
constexpr u32 kStringEmpty = 0x08D74FD0u;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3, f12-f15).
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_setf(rt, ctx, address, arguments, {}).v0;
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (the recompiler's rule).
inline u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = address;
    return call_set(rt, ctx, address, arguments);
}
inline u32 slot_of(Runtime &rt, u32 object, u32 offset) { return word(rt, word(rt, object) + offset); }

inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline s32 byte_s(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline s32 half_s(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
inline u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
// cvt.s.w of a word read as a signed integer.
inline float cvt_sw(u32 bits) { return static_cast<float>(signed_of(bits)); }
inline float fbits(u32 bits) { return std::bit_cast<float>(bits); }

// The release of a one-byte-character string holder at `address`: the shared empty representation is left alone, otherwise the
// reference count at -4 of the representation drops (0x08B60804) and, when it reaches zero, the block at -0xC is freed
// (0x08B8542C, whose a1 is a byte of the caller's frame: the asm passes sp + N, given here as `pad`).
// v0 / v1 stay as the last callee left them, like the original's.
inline void release_string(Runtime &rt, AllegrexContext &ctx, u32 address, u32 pad) {
    const u32 representation = word(rt, address);
    const u32 block = representation - 0xCu;
    if (block == kStringEmpty) return;
    const u32 left = call_keep(rt, ctx, 0x08B60804u, {representation - 4u, 0xFFFFFFFFu});
    if (signed_of(left) > 0) return;
    (void)call_keep(rt, ctx, 0x08B8542Cu, {block, pad});
}
// The null test the original writes as `bnez x; jal 0x08A5BB7C(string)`: the guard reports and the code goes on.
inline void check_present(Runtime &rt, AllegrexContext &ctx, u32 value, u32 message) {
    if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {message});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0897A588 / 0x089C9238: the same body with four different constants
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Both are Adhoc natives (config/symbol_addrs.txt: MUpdateContext::unloadGpb and MManager::unloadGpb, registrar 0x08B87108), and
// the two listings differ only in the addresses passed here (a diff of the two `tools.disasm` outputs with the immediates
// masked leaves eight lines, all of them the pair constructor and three constants).
//
// a2 is the mode: 1 takes the first object of the array at a3 (word(a3)), 2 takes both (word(a3) and word(a3 + 4)), anything else
// answers v0 = 2 and touches nothing. Each mode makes a pair (the constructor), a key holder (0x08B85EB8(holder, name, sp + 4)) and
// one smart pointer per object through the object's virtual slot 0xC, merges them with 0x089C9024(pair, first, second), releases
// both strings and destroys the pair (0x08A5B9EC). A null object or pair reports through 0x08A5BB7C and the code goes on.
// v0: the mode that does nothing answers 2, any other mode answers the last callee's v0 (the `addiu v0, zero, 1` at the top is
// overwritten by the pair constructor), so the return contract here is Void, as the commented registration line says.
void unload_gpb(Runtime &rt, AllegrexContext &ctx, u32 pair_ctor, u32 empty_pair_message, u32 null_object_message, u32 name) {
    const u32 mode = ctx.gpr[6];
    const u32 objects = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    u32 pair;
    if (mode == 1u) {
        pair = frame.at(0x1Cu);
        const u32 holder = frame.at(0x18u);
        const u32 result = frame.at(0x14u);
        (void)call_set(rt, ctx, pair_ctor, {pair});
        const u32 first = word(rt, pair);
        check_present(rt, ctx, first, empty_pair_message);
        (void)call_set(rt, ctx, 0x08B85EB8u, {holder, name, frame.at(4u)});
        const u32 object = word(rt, objects);
        check_present(rt, ctx, object, null_object_message);
        (void)call_virtual(rt, ctx, slot_of(rt, object, 0xCu), {result, object});
        (void)call_set(rt, ctx, 0x089C9024u, {first, holder, result});
        release_string(rt, ctx, result, frame.at(3u));
        release_string(rt, ctx, holder, frame.at(2u));
    } else if (mode == 2u) {
        pair = frame.at(0x10u);
        const u32 first_holder = frame.at(0xCu);
        const u32 second_holder = frame.at(8u);
        (void)call_set(rt, ctx, pair_ctor, {pair});
        const u32 pair_word = word(rt, pair);
        const u32 first = word(rt, objects);
        check_present(rt, ctx, pair_word, empty_pair_message);
        check_present(rt, ctx, first, null_object_message);
        (void)call_virtual(rt, ctx, slot_of(rt, first, 0xCu), {first_holder, first});
        const u32 second = word(rt, objects + 4u);
        check_present(rt, ctx, second, null_object_message);
        (void)call_virtual(rt, ctx, slot_of(rt, second, 0xCu), {second_holder, second});
        (void)call_set(rt, ctx, 0x089C9024u, {pair_word, first_holder, second_holder});
        release_string(rt, ctx, second_holder, frame.at(1u));
        release_string(rt, ctx, first_holder, frame.at(0u));
    } else {
        ctx.gpr[2] = 2u;
        return;
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pair});
}

void func_0897A588(Runtime &rt, AllegrexContext &ctx) {
    unload_gpb(rt, ctx, 0x089C9C34u, 0x08CA3268u, 0x08CA2F84u, 0x08C8BDBCu);
}
void func_089C9238(Runtime &rt, AllegrexContext &ctx) {
    unload_gpb(rt, ctx, 0x089C5850u, 0x08CA63A0u, 0x08CA6304u, 0x08C8CC2Cu);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x089B06B0: rebuild the two records of an object under the library lock
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// If the lock 0x08CF4A04 cannot be taken (0x08B3453C answers 0) nothing happens and v0 = 0. Otherwise the two lookups of the
// object (0x089AA5A0 and 0x089AA5FC) are made, each released again through 0x08A5BA08 when it answered non-zero. A lookup counts
// as valid when it is non-zero and its word at +4 is non-zero. Only when the two valid ones DIFFER does the function publish: for
// each valid side a 0x24-byte record is allocated (0x08A5BC20), constructed with (object, side) (0x08965984), given the vtable
// (0x08CC99B8 for the first lookup, 0x08CC76D8 for the second) and published through 0x089FF120 (a handle built on the frame at
// sp + 4 / sp whose first word holds the record) whose address then goes to 0x0897783C together with word(self + 0x14) - the
// announcement, re-read before the second record because the first construction may have changed it. Then both lookups are
// released (0x08A5B930) and the lock is left (0x08B3457C); v0 is that last call's.
// The asm's second copy of the "valid" test (0x089B0884: s1 = 0, then a 0x24-byte record constructed with a null side) is
// unreachable - it retests the condition that just brought control there - so it is not reproduced.
void func_089B06B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    if (call_set(rt, ctx, 0x08B3453Cu, {0x08CF4A04u}) == 0u) return;  // v0 = 0
    const u32 first = call_set(rt, ctx, 0x089AA5A0u, {self});
    if (first != 0u) (void)call_set(rt, ctx, 0x08A5BA08u, {first});
    const u32 second = call_set(rt, ctx, 0x089AA5FCu, {self});
    if (second != 0u) (void)call_set(rt, ctx, 0x08A5BA08u, {second});
    const u32 valid_first = (first != 0u && word(rt, first + 4u) != 0u) ? first : 0u;
    const u32 valid_second = (second != 0u && word(rt, second + 4u) != 0u) ? second : 0u;
    if (valid_first != valid_second) {
        if (valid_first != 0u) {
            const u32 notify = word(rt, self + 0x14u);
            const u32 created = call_set(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
            (void)call_set(rt, ctx, 0x08965984u, {created, self, valid_first});
            put(rt, created, 0x08CC99B8u);
            const u32 handle = frame.at(4u);
            put(rt, handle, created);
            (void)call_set(rt, ctx, 0x089FF120u, {frame.at(0xCu), handle});
            (void)call_set(rt, ctx, 0x0897783Cu, {notify, frame.at(0xCu)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        }
        (void)call_set(rt, ctx, 0x089B05F8u, {self, valid_second});
        if (valid_second != 0u) {
            const u32 notify = word(rt, self + 0x14u);
            const u32 created = call_set(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
            (void)call_set(rt, ctx, 0x08965984u, {created, self, valid_second});
            put(rt, created, 0x08CC76D8u);
            const u32 handle = frame.at(0u);
            put(rt, handle, created);
            (void)call_set(rt, ctx, 0x089FF120u, {frame.at(8u), handle});
            (void)call_set(rt, ctx, 0x0897783Cu, {notify, frame.at(8u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        }
    }
    if (second != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {second});
    if (first != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {first});
    (void)call_set(rt, ctx, 0x08B3457Cu, {0x08CF4A04u});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08A7ED20: insert one record into the map at dest[0x10] from the tree node at word(a3)
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// a0 = the container, a1 = the value handed to the virtual insert, a2 = an argument of the slot-0x100 call, a3 = a pointer whose
// word is the source tree node. When that word is the pointer itself (the empty map's end node) the work is 0x08A7D1F8(container,
// a2) and the function returns its v0.
// Otherwise the node's payload at +8 becomes the key holder at sp + 8 (0x08B88F44), a temporary (0x08A5D438), a pair
// (0x08A69654(pair, key)) inserted with 0x08A7D5FC(container, temporary, pair) and the pair destroyed (0x08A69D68). When
// word(temporary) is zero the rest is skipped. Otherwise a hint holder at sp + 0x10 is made (0x08A76AA0), the temporary is
// dereferenced (its vtable slot 0x1C runs with (temporary, holder, a1)) and, when that holder is non-zero, the destination
// value is assigned (0x08A5BAD8(a1, word(holder)) unless a1 IS the holder), the source node's string is released
// (0x08B60804 / 0x08B8542C) and its 0xC-byte block freed (0x08A5BC54), then the node the slot-0x100 call returns is filled: a
// 0xC-byte block (0x08A5BC20) whose +8 is a copy of the key (0x08B88F44) and whose head goes to 0x08B5C078(block, word(a3)). The
// hint holder is destroyed and the temporary too, and the key holder at sp + 8 is released at the end.
// v0 is the last callee's on every path (call_keep keeps it), which is why the registration below asks for Word.
void func_08A7ED20(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value_argument = ctx.gpr[5];
    const u32 second_argument = ctx.gpr[6];
    const u32 node_pointer = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const u32 node = word(rt, node_pointer);
    if (node == node_pointer) {  // the empty map's node is the pointer itself
        (void)call_set(rt, ctx, 0x08A7D1F8u, {self, second_argument});
        return;
    }
    const u32 key = frame.at(8u);
    const u32 temporary = frame.at(0xCu);
    const u32 pair = frame.at(4u);
    (void)call_set(rt, ctx, 0x08B88F44u, {key, node + 8u});
    (void)call_set(rt, ctx, 0x08A5D438u, {temporary});
    (void)call_set(rt, ctx, 0x08A69654u, {pair, key});
    (void)call_set(rt, ctx, 0x08A7D5FCu, {self, temporary, pair});
    (void)call_set(rt, ctx, 0x08A69D68u, {pair});
    if (word(rt, temporary) != 0u) {
        const u32 holder = frame.at(0x10u);
        (void)call_set(rt, ctx, 0x08A76AA0u, {holder});
        const u32 object = word(rt, temporary);
        check_present(rt, ctx, object, 0x08CB1358u);
        (void)call_virtual(rt, ctx, slot_of(rt, object, 0x1Cu), {object, holder, value_argument});
        if (word(rt, holder) != 0u) {
            if (value_argument != holder) (void)call_set(rt, ctx, 0x08A5BAD8u, {value_argument, word(rt, holder)});
            const u32 node = word(rt, node_pointer);
            (void)call_set(rt, ctx, 0x08B5C094u, {node});
            release_string(rt, ctx, node + 8u, frame.at(1u));
            (void)call_set(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
            const u32 inserted = word(rt, holder);
            check_present(rt, ctx, inserted, 0x08CB1350u);
            (void)call_virtual(rt, ctx, slot_of(rt, inserted, 0x100u), {inserted, value_argument, second_argument, node_pointer});
            const u32 payload = word(rt, node_pointer);
            const u32 block = call_set(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
            if (block + 8u != 0u) (void)call_set(rt, ctx, 0x08B88F44u, {block + 8u, key});
            (void)call_set(rt, ctx, 0x08B5C078u, {block, payload});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    release_string(rt, ctx, key, frame.at(0u));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08B8B7B4: the sift-down of a heap of 24-byte records
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// The records are 24 bytes: two words at +0 / +4 and a three-word vector at +8 (begin, end, capacity end); the comparator
// 0x08B8A72C reads the [begin, end) words of two of them. a1 is the hole the value must reach, a2 the count (the end index), a3
// the value (a record: its two words and its vector). The caller (0x08B8BC08) copies the value to a temporary and passes its
// address.
//
// The loop: while 2 * hole + 2 < count (signed `slt`), the two children are 2 * hole + 1 (left) and 2 * hole + 2 (right); when
// 0x08B8A72C(right, left) answers 0 the right child goes into the hole, else the left one: the chosen record's two words are
// copied and its vector move-assigned (0x08B8AD88) unless the destination tail is the source tail; the hole becomes that child.
// When the loop stops because the child index equals count, a lone left child (count - 1) is moved up the same way.
// Then the value a3 becomes a temporary record on the frame (its words copied, its vector copied with 0x08A5BC20 +
// 0x08B8A654 into sp + 0x10..0x18), pushed by 0x08B8B630(first, hole, a1, &temporary, capacity end), destroyed (0x08B8A644) and
// its storage freed (0x08A5BC54(begin, bytes, 4)).
// v0: the last call's, or - when nothing was allocated - the `sra` of (capacity end - begin) by one that the asm leaves in v0.

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0897C460: the distance-driven trigger of a record
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// a0 = the offset added to a1 to reach the record (s2 = a1 + a0), a2 = the object asked for its record (0x089AAC8C; a zero
// answer leaves v0 = 0 and nothing else happens), a3 and t0 = the two signed integers of the distance. The record's +0x138 bit
// 0x20 picks k = 1 (bit set: s0 = 1, the divisor s1 = 8) or k = 2 (s0 = 2, s1 = 4).
// The length is sqrt(x * x + y * y) of the two integers as floats; 0 gives v0 = 0 at once. The angle is
// 0x08B66FBC(a2, f12 = t0 / length, f13 = a3 / length) (f13 takes a3, f12 takes t0) and the index
//     trunc((angle * 180 / pi + 180 + k * 22.5) / (k * 45))
// is kept in s5 (times k) only when it differs from s1 (8 for k = 1, 4 for k = 2): the `mult` in the branch's delay slot runs
// either way, `mflo` is only read on the path that keeps it. When the signed byte at +0x149 differs from s5 the counter byte at
// +0x148 is reset to 5; then it advances by trunc(length / 10) + 1 and, if the sign-extended byte is still below 10, the function
// answers 0 after storing s5 at +0x149.
// Otherwise 0x089AA95C(a2, sp + 8, sp + 0xC) gives two floats, a 0x40-byte block is allocated (0x08A5BC20), constructed by
// 0x089D6378(block, a2, record, 0x08CDE768 + 4 * s5, a1, 0, the two floats), given the vtable 0x08CC9758 and a zero byte at +0x3C,
// held in a smart pointer at sp + 4 (0x089D5E8C(sp + 4, sp)) and handed to the callback 0x0897783C(a0, sp + 4) - a0 is the
// function's own a0, which the epilogue never overwrites. The smart pointer is destroyed, the counter byte reset and v0 = 1.
void func_0897C460(Runtime &rt, AllegrexContext &ctx) {
    const u32 origin = ctx.gpr[4];
    const u32 offset = ctx.gpr[5];
    const u32 asked = ctx.gpr[6];
    const u32 x_input = ctx.gpr[7];
    const u32 y_input = ctx.gpr[8];
    GuestFrame frame(ctx, 0x50u);
    const u32 record_object = call_set(rt, ctx, 0x089AAC8Cu, {asked});
    if (record_object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const float x = cvt_sw(x_input);
    const float y = cvt_sw(y_input);
    const float length = std::sqrt(psp_mul(x, x) + psp_mul(y, y));
    if (!(0.0f < length)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const float normalized_x = x / length;
    const float normalized_y = y / length;
    const u32 flags = word(rt, record_object + 0x138u);
    const u32 scale = (flags & 0x20u) != 0u ? 1u : 2u;   // s0
    const u32 span = (flags & 0x20u) != 0u ? 8u : 4u;    // s1
    const float angle = call_setf(rt, ctx, 0x08B66FBCu, {asked}, {normalized_y, normalized_x}).f0;  // f12 = t0, f13 = a3
    const float degrees = psp_mul(angle, 180.0f) / 3.14159274101257324f;  // 0x4334 / 0x40490FDB
    const float biased = degrees + 180.0f + psp_mul(static_cast<float>(scale), 22.5f);
    const float divisor = psp_mul(static_cast<float>(scale), 45.0f);
    const u32 quotient = static_cast<u32>(static_cast<s32>(biased / divisor));  // trunc.w.s (the PSP saturates, not wraps)
    u32 direction = 0u;
    const u32 scaled = quotient * scale;  // the `mult` of the branch's delay slot: v0 * s0
    if (span != quotient) direction = scaled;
    const u32 record = offset + origin;  // s2
    if (byte_s(rt, record + 0x149u) != signed_of(direction)) put8(rt, record + 0x148u, 5u);
    const u32 advance = static_cast<u32>(static_cast<s32>(length / 10.0f));  // trunc.w.s
    const u32 counter = sext8(advance + byte_u(rt, record + 0x148u) + 1u);
    put8(rt, record + 0x148u, counter);
    if (signed_of(counter) < 10) {
        put8(rt, record + 0x149u, direction);
        ctx.gpr[2] = 0u;  // a0 = 0 in the asm, `addu v0, a0, zero` at the exit
        return;
    }
    const u32 pair_low = frame.at(8u);
    const u32 pair_high = frame.at(0xCu);
    (void)call_set(rt, ctx, 0x089AA95Cu, {asked, pair_low, pair_high});
    const float first = fbits(word(rt, pair_low));
    const float second = fbits(word(rt, pair_high));
    const u32 table_entry = word(rt, 0x08CDE768u + 4u * direction);
    const u32 block = call_set(rt, ctx, 0x08A5BC20u, {0x40u, 4u});
    (void)call_setf(rt, ctx, 0x089D6378u, {block, asked, record_object, table_entry, offset, 0u}, {first, second});
    ctx.gpr[2] = 0x08CC9758u;  // the asm builds the vtable address in v0 before storing it (the block is s1)
    put(rt, block, 0x08CC9758u);
    put8(rt, block + 0x3Cu, 0u);
    const u32 holder = frame.at(4u);
    put(rt, holder, block);
    (void)call_set(rt, ctx, 0x089D5E8Cu, {holder, frame.at(0u)});
    (void)call_set(rt, ctx, 0x0897783Cu, {origin, holder});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    put8(rt, record + 0x148u, 0u);
    put8(rt, record + 0x149u, direction);
    ctx.gpr[2] = 1u;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088BD6BC: the float weight of a record
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// a0 = the object (its byte +0x12 is the direction), a1 = a record whose float at +0x2C is the blend, a2 = the holder whose word
// is the table object, a3 = the object whose float at +0x1AC scales the result, t0 = an argument of 0x088BB4B8.
// The blended factor a is the record's +0x2C, halved as (a + 0.5) * 0.5 when the byte at word(a2[4]) + 0x578 is set OR the byte
// at word(a2[0]) + 0x13E2 is set (the `beql` skips the halving only when both are clear).
// Two lookups 0x0894268C(word(a2[0]), 0 or 2, direction) give two records; the value is
//     (a * v0[0] * v0[direction].f8 + (1 - a) * v2[0] * v2[direction].f8) * f(word(a2[0]) + 0x1350)
// and times 0.9 (0x3F666666) more when the halving flag was set.
//
// The pair (a2[0x18], a2[0x20]) tilts it: with 0 <= a2[0x18] and a2[0x20] < 0 the weight becomes a * sp[0] + sp[4] * (-9.8)
// (0x08B498AC(sp + 4, sp)); with a2[0x18] < 0 and a2[0x20] > 0, a * sp[4] + sp[0] * 9.8 (0x08B498AC(sp, sp + 4)); the result is
// then scaled by a2[0x18] with the sign of that first test. Then the object's +0x1AC: when it is above 0,
// 0x08B49D7C(f12 = the weight) is the square root, 0x088BB4B8(a0 = direction, a1 = the object, a2 = t0, f12 = that root) the
// angle term, and the factor becomes (1 - +0x1AC) + +0x1AC * term, floored at 0.1 (0x3DCCCCCD). f0 = the weight.
//
// The float operations keep the asm's order and the PSP's multiply (psp_mul). 0x08B49D7C reads no a0 (it is a square root with
// a sign test); the a0 the asm leaves in that register is whatever the previous call set, so nothing is passed here.

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088616A8: the table update
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// self (a0). Nothing happens when 0x088DAA84(self[0x5AC]) answers zero. R = self + 0x608 + 7224 * self[0x3E78] (56 * 129) is a
// record of 0x1C38 bytes; u = 0x088D9C50(self[0x5AC], 0, R), and a non-zero u runs 0x08861538(self, R, u). Nothing more happens
// when the byte R[0x1C34] is zero. c = the signed halfword 0x088DA160(self[0x5AC]) is stored at R[0x1C32] and, when positive,
// R[0x1C2A + 2 * k] = 0x088DA170(self[0x5AC], k) for k below the re-read R[0x1C32].
// R[0x1C28] = 0, then for the n = 0x088D9F80(self[0x5AC]) slots j (entry j of 0xA8 bytes from R): when
// 0x088DC90C(self[0x5AC], j) is below 2 and the byte at R + 0xA8 * j + 0x102A is set, j is appended as a halfword at
// R[0x1C20 + 2 * R[0x1C28]] and R[0x1C28] += 1.
// The halfword list R[0x1C20, 0x1C20 + 2 * n) is then selection-sorted by the 64-bit key (high word at +0x24, low at +0x20) of the
// record R + 0x1018 + 168 * index: for i, with the pivot the entry i, every later entry whose key is below the pivot's is swapped
// in unless the byte R[0x1C35] equals that "below" answer (an equal key is never swapped).
// v0 = 0 on every exit (the `lbu` of the guard, and the sort's own `slt` on the way out).
void func_088616A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    if (call_set(rt, ctx, 0x088DAA84u, {word(rt, self + 0x5ACu)}) == 0u) return;  // v0 = 0
    const u32 index = word(rt, self + 0x3E78u);
    const u32 record = self + (index * 56u + (index * 56u << 7u)) + 0x608u;
    const u32 found = call_set(rt, ctx, 0x088D9C50u, {word(rt, self + 0x5ACu), 0u, record});
    if (found != 0u) (void)call_set(rt, ctx, 0x08861538u, {self, record, found});
    if (byte_u(rt, record + 0x1C34u) == 0u) {
        ctx.gpr[2] = 0u;  // the `lbu` of the test
        return;
    }
    const u32 raw_count = call_set(rt, ctx, 0x088DA160u, {word(rt, self + 0x5ACu)});
    const u32 stored = sext16(raw_count);            // `seh v0, v0` right after the call
    put16(rt, record + 0x1C32u, stored);             // the store sits in the delay slot of the test below
    ctx.gpr[2] = stored;                             // the `seh` leaves it in v0 for the path that skips the loop
    if (signed_of(stored) > 0) {
        u32 k = 0u;
        do {
            const u32 value = call_set(rt, ctx, 0x088DA170u, {word(rt, self + 0x5ACu), k});
            put16(rt, record + 0x1C2Au + 2u * k, value);
            ++k;
        } while (signed_of(k) < half_s(rt, record + 0x1C32u));
    }
    put16(rt, record + 0x1C28u, 0u);
    const s32 slots = signed_of(call_set(rt, ctx, 0x088D9F80u, {word(rt, self + 0x5ACu)}));
    if (slots > 0) {
        for (s32 j = 0; j != slots; ++j) {
            const u32 kind = call_set(rt, ctx, 0x088DC90Cu, {word(rt, self + 0x5ACu), static_cast<u32>(j)});
            if (kind < 2u && byte_u(rt, record + 0xA8u * static_cast<u32>(j) + 0x102Au) != 0u) {
                const u32 used = sext16(half_u(rt, record + 0x1C28u));
                put16(rt, record + 0x1C20u + 2u * used, static_cast<u32>(j));
                put16(rt, record + 0x1C28u, used + 1u);
            }
        }
    }
    const s32 n = half_s(rt, record + 0x1C28u);
    for (s32 i = 0; i < n - 1; ++i) {  // the loop test's `slt` leaves 0 in v0 when it ends
        u32 pivot = sext16(half_u(rt, record + 0x1C20u + 2u * static_cast<u32>(i)));
        u32 pivot_record = record + 0x1018u + 168u * pivot;
        const u32 descending = byte_u(rt, record + 0x1C35u);
        for (s32 j = i + 1; j < n; ++j) {
            const u32 slot = record + 0x1C20u + 2u * static_cast<u32>(j);
            const u32 other = sext16(half_u(rt, slot));
            const u32 other_record = record + 0x1018u + 168u * other;
            const u32 pivot_high = word(rt, pivot_record + 0x24u), pivot_low = word(rt, pivot_record + 0x20u);
            const u32 other_high = word(rt, other_record + 0x24u), other_low = word(rt, other_record + 0x20u);
            u32 below = 1u;
            if (other_high != pivot_high) below = other_high < pivot_high ? 1u : 0u;
            else if (other_low == pivot_low) continue;  // an equal key is never swapped
            else below = other_low < pivot_low ? 1u : 0u;
            if (descending == below) continue;
            put16(rt, slot, pivot);
            put16(rt, record + 0x1C20u + 2u * static_cast<u32>(i), other);
            pivot = other;
            pivot_record = other_record;
        }
    }
    ctx.gpr[2] = 0u;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x089C9CE4: erase by key over the container at a0, with the out-parameter at a1
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// a0 = the container, a1 = the out-parameter (a smart pointer). 0x08A894C8(sp + 0xC, sp + 0x58) makes the iterator at sp + 0xC
// (a null one reports 0x08CA6674) and 0x08A8917C(it, a0, 1) asks whether the key is present: zero jumps straight to the fallback.
// Otherwise 0x08985F2C(sp + 0x20, sp + 0xC) fills the search record; a four-word iterator is built at sp + 0x10
// (0x08CCED90, 0x08D74FDC, -1, a1) and the record's virtual slot 8 runs with (sp + 0x20, sp + 0x10); the string at sp + 0x14 is
// released and 0x08CCD990 stored at sp + 0x10 instead.
//   - word(a1) == 0: nothing is erased and the fallback runs (the result stays 0);
//   - else when the byte at sp + 0x4C is zero: the result is 1 and nothing is erased;
//   - else 0x08AA46C4(sp + 0x10, a0), a key is made at sp + 8 (0x089C9C34; a null one reports 0x08CA63A0) and
//     0x089C8680(key, word(a1), sp + 0x10) erases; the key and the string at sp + 0x10 are released and the result is 1.
// Then the container's virtual slot 0x11C runs (a null container reports 0x08CA6674) and 0x08985FE4(sp + 0x20).
// The fallback: a default is made at sp + 4 (0x089BAC3C(sp + 4, 0x08CF6ED8)), assigned to the out-parameter
// (0x08A5BAD8(a1, word(sp + 4)) unless a1 IS sp + 4) and destroyed, and the result becomes 0. At the end the iterator at sp + 0xC
// is destroyed (the delay slot of the jump to the epilogue sets s0 = sp + 4, unused afterwards) and v0 is the result.
void func_089C9CE4(Runtime &rt, AllegrexContext &ctx) {
    const u32 container = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x1090u);
    const u32 iterator = frame.at(0xCu);
    const u32 record = frame.at(0x20u);
    u32 result = 0u;   // s1
    u32 settled = 0u;  // s3: the fallback is skipped when this is set
    (void)call_set(rt, ctx, 0x08A894C8u, {iterator, frame.at(0x58u)});
    const u32 it = word(rt, iterator);
    check_present(rt, ctx, it, 0x08CA6674u);
    if (call_set(rt, ctx, 0x08A8917Cu, {it, container, 1u}) != 0u) {
        (void)call_set(rt, ctx, 0x08985F2Cu, {record, iterator});
        put(rt, frame.at(0x18u), 0xFFFFFFFFu);
        put(rt, frame.at(0x10u), 0x08CCED90u);
        put(rt, frame.at(0x14u), 0x08D74FD0u + 0xCu);  // `addiu v1, s7, 0xC`
        put(rt, frame.at(0x1Cu), out);
        const u32 object = word(rt, record);
        (void)call_virtual(rt, ctx, word(rt, object + 8u), {record, frame.at(0x10u)});
        put(rt, frame.at(0x10u), 0x08CCD990u);  // the delay slot of the release's first test
        release_string(rt, ctx, frame.at(0x14u), frame.at(1u));
        if (word(rt, out) != 0u) {
            if (byte_u(rt, frame.at(0x4Cu)) != 0u) {
                (void)call_set(rt, ctx, 0x08AA46C4u, {frame.at(0x10u), container});
                const u32 key = frame.at(8u);
                (void)call_set(rt, ctx, 0x089C9C34u, {key});
                const u32 made = word(rt, key);
                check_present(rt, ctx, made, 0x08CA63A0u);
                (void)call_set(rt, ctx, 0x089C8680u, {made, word(rt, out), frame.at(0x10u)});
                (void)call_set(rt, ctx, 0x08A5B9ECu, {key});
                release_string(rt, ctx, frame.at(0x10u), frame.at(0u));
            }
            result = 1u;
            settled = 1u;
        }
        const u32 lock = word(rt, iterator);
        check_present(rt, ctx, lock, 0x08CA6674u);
        (void)call_virtual(rt, ctx, word(rt, word(rt, lock) + 0x11Cu), {lock});
        (void)call_set(rt, ctx, 0x08985FE4u, {record});
    }
    if (settled == 0u) {
        const u32 fallback = frame.at(4u);
        (void)call_set(rt, ctx, 0x089BAC3Cu, {fallback, 0x08CF6ED8u});
        if (out != fallback) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, fallback)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {fallback});
        result = 0u;
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {iterator});
    ctx.gpr[2] = result;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08A7A3D4: the copy of a map tree
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// dest (a0), source (a1): copies the map `source` (its tree object at source[0x10]: a header whose end node is header + 4 and whose
// first node is header[0xC]; a node: key at +0x10, value cell at +0x14) into a new tree object 0x08A5BC20(0x240, 4) = dest[0x10]
// ({[4], [8] = 0, [0xC] = [0x10] = header + 4, [0x14] = 0}). Nothing happens (v0 as it was) when source is null or
// source[0x10] is 0.
// For each source node n in order (0x08B5F100 steps): k = n + 0x10; h = 0x08C25AF4(dest tree, k) is the insertion hint; the node is
// inserted with 0x08C25E64(tree, h, key holder) when h is the end node or n[0x10] < h[0x10] (unsigned), the key holder being
// sp + 0x18 (0x08A6972C(sp + 0x18, k), a pair 0x08A76AA0(sp + 4) / 0x08A76ABC(sp + 0x1C, sp + 4) around it) when k[4] is non-zero
// and sp + 0x10 (pair sp + 0 / sp + 0x14) when it is zero; the value cell of the node found or inserted (+0x14) is assigned
// n[0x14] (a copy 0x08A77444(sp + 0xC, n[0x14]); null reports 0x08CB0E38 first) in the first case and an empty holder
// (0x08A76AA0(sp + 8)) in the second, with 0x08A5BAD8 unless it is the source itself. v0 = source header + 4 (the loop test's) when
// it ends.
void func_08A7A3D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 dest = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    if (source == 0u) return;
    ctx.gpr[2] = word(rt, source + 0x10u);
    if (ctx.gpr[2] == 0u) return;
    const u32 tree = call_set(rt, ctx, 0x08A5BC20u, {0x240u, 4u});
    put(rt, tree + 0xCu, tree + 4u);
    put(rt, tree + 0x10u, tree + 4u);
    put(rt, tree + 0x14u, 0u);
    put(rt, tree + 4u, 0u);
    put(rt, tree + 8u, 0u);
    put(rt, dest + 0x10u, tree);
    const u32 header = word(rt, source + 0x10u);
    u32 node = word(rt, header + 0xCu);
    ctx.gpr[2] = header + 4u;
    if (node == header + 4u) return;
    for (;;) {
        const u32 key = node + 0x10u;
        const u32 destination_tree = word(rt, dest + 0x10u);
        const bool has_value = word(rt, key + 4u) != 0u;
        const u32 hint = call_set(rt, ctx, 0x08C25AF4u, {destination_tree, key});
        u32 found = hint;  // s0
        // The frame rows of the two halves: the default holder, the key copy, the pair, then the value tail.
        const u32 plain = has_value ? frame.at(4u) : frame.at(0u);
        const u32 copy = has_value ? frame.at(0x18u) : frame.at(0x10u);
        const u32 pair = has_value ? frame.at(0x1Cu) : frame.at(0x14u);
        if (hint == destination_tree + 4u || word(rt, node + 0x10u) < word(rt, hint + 0x10u)) {
            (void)call_set(rt, ctx, 0x08A76AA0u, {plain});
            (void)call_set(rt, ctx, 0x08A6972Cu, {copy, key});
            (void)call_set(rt, ctx, 0x08A76ABCu, {pair, plain});
            found = call_set(rt, ctx, 0x08C25E64u, {destination_tree, hint, copy});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {pair});
            (void)call_set(rt, ctx, 0x08A69D68u, {copy});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {plain});
        }
        found += 0x14u;
        if (has_value) {
            const u32 value = word(rt, node + 0x14u);
            if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
            const u32 temporary = frame.at(0xCu);
            (void)call_set(rt, ctx, 0x08A77444u, {temporary, value});
            if (found != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {found, word(rt, temporary)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        } else {
            const u32 empty = frame.at(8u);
            (void)call_set(rt, ctx, 0x08A76AA0u, {empty});
            if (found != empty) (void)call_set(rt, ctx, 0x08A5BAD8u, {found, word(rt, empty)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {empty});
        }
        node = call_set(rt, ctx, 0x08B5F100u, {node});
        ctx.gpr[2] = word(rt, source + 0x10u) + 4u;
        if (node == ctx.gpr[2]) break;
    }
}
} // namespace
} // namespace new_batch_main_208
} // namespace gt::game

// The return kinds: the two Adhoc natives leave the last callee's v0 (the `addiu v0, 1` at their top never survives the pair
// constructor), so they ask for Void; every other function's v0 / f0 is modelled exactly as the asm leaves it.
#define NEW208(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_208::name, gt::ReturnKind::kind)
NEW208(0x0897A588u, func_0897A588, Void);   // MUpdateContext::unloadGpb
NEW208(0x089C9238u, func_089C9238, Void);   // MManager::unloadGpb, the same body with four other constants
NEW208(0x089B06B0u, func_089B06B0, Word);   // v0 = 0 when the lock is busy, else 0x08B3457C's answer
NEW208(0x08A7ED20u, func_08A7ED20, Word);   // v0 is the last callee's on every path
NEW208(0x0897C460u, func_0897C460, Word);   // 0 or 1
NEW208(0x088616A8u, func_088616A8, Word);   // 0 on every exit
NEW208(0x089C9CE4u, func_089C9CE4, Word);   // 0 or 1
NEW208(0x08A7A3D4u, func_08A7A3D4, Word);   // v0 = source header + 4 at the end, as the loop test leaves it

// ---- from main-session batch 209 (draft 84d165935d205645f1fcc1ebe2408bf2ec6a8194) ----
// New batch, main session 209: functions the recompiler emitted that no file had decompiled yet, written by hand as
// structured C from `tools.disasm`. Draft only: the differential gate (fast, sabotage, synthetic) is run by the
// verifier, not here.
//
// Conventions, as in new_batch_3: a `jal` passes the argument registers the original sets before it (the rest keep the
// caller's value); the delay slot of a `jal` runs BEFORE the call; a `bnel` / `beql` / `bc1fl` delay slot only runs
// when the branch is taken; a plain branch's delay slot runs on both paths. `v0` is set as the original leaves it on
// every exit path. The frames callees read or write are `GuestFrame`s, size = the original's `addiu sp, sp, -N`.
// Callee-saved registers the body uses are saved and restored, because the harness compares them.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_209 {
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
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// `lw v1, off(v0); jalr v1` after `lw v0, 0(object)`: the callee starts with v0 = the vtable and v1 = its own address.
u32 call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + offset);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// The callee-saved registers the batch's bodies use (s0-s7 and fp).
struct SavedRegisters {
    AllegrexContext &ctx;
    u32 saved[9];
    explicit SavedRegisters(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
        saved[8] = c.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
        ctx.gpr[30] = saved[8];
    }
};

// 0x08B3F4D0 (a0 = a four-slot map, a1 = the key): the map is {slot values at +0..+0xF, keys at +0x10..+0x1F, active
// flags (bytes) at +0x20..+0x23, word count at +0x24, word used at +0x28, an owner pointer at +0x2C}. Nothing happens
// with a zero count. Otherwise the first active slot whose key is a1 is looked up twice (the asm repeats the same
// search). A null slot value answers the slot's address. Otherwise the slot is cleared (value 0, flag 0), the used word
// drops by one, and the removed node's links are repaired: the nearest active slot before the removed one (or, when
// there is none, the first active slot after it, whose +4 becomes 0) is linked to the first active slot after it
// (before->+8 = after and after->+4 = before; before->+8 = 0 when there is no after), then 0x08B2CE7C(owner, node[0])
// runs and the node itself goes to 0x08B2CE7C as a tail call.
void func_08B3F4D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 count = word(rt, self + 0x24u);
    if (count == 0u) return;
    // The search: the first slot with a set flag whose key matches; on the way out v0 is what the last read left.
    u32 last = 0u;
    u32 index = 0u;
    for (;;) {
        const u32 flag = rt.memory().load8(self + index + 0x20u);
        last = flag;
        if (flag != 0u) {
            last = word(rt, self + index * 4u + 0x10u);
            if (key == last) break;
        }
        ++index;
        if (index == count) {
            ctx.gpr[2] = last;
            return;
        }
    }
    const u32 slot = self + index * 4u;
    ctx.gpr[2] = slot;
    const u32 node = word(rt, slot);
    if (node == 0u) return;
    const u32 sub = word(rt, node);
    if (sub == 0u) return;
    // The same search again, as the asm repeats it; its failure path joins the free.
    u32 second = 0u;
    bool found = true;
    for (;;) {
        const u32 flag = rt.memory().load8(self + second + 0x20u);
        if (flag != 0u && key == word(rt, self + second * 4u + 0x10u)) break;
        ++second;
        if (second == count) {
            found = false;
            break;
        }
    }
    bool freed = false;
    if (found) {
        const u32 used = word(rt, self + 0x28u) - 1u;
        put(rt, self + index * 4u, 0u);
        rt.memory().store8(self + index + 0x20u, 0u);
        put(rt, self + 0x28u, used);
        if (index > 0u) {
            // The nearest active slot before the removed one, and the first active slot after it.
            u32 values_ptr = self + (index - 1u) * 4u;
            u32 flags_ptr = self + index + 0x1Fu;
            u32 counter = 0u;
            for (;;) {
                const s32 left = static_cast<s32>(index) - 1 - static_cast<s32>(counter);
                const bool in_range = left < static_cast<s32>(count);
                ++counter;
                if (in_range && rt.memory().load8(flags_ptr) != 0u) {
                    const u32 before = word(rt, values_ptr);
                    if (before != 0u) {
                        if (index + 1u < 4u) {
                            u32 after_index = index + 1u;
                            u32 after_values = self + after_index * 4u;
                            u32 after_flags = self + index + 0x21u;
                            for (;;) {
                                if (static_cast<s32>(after_index) < static_cast<s32>(count)) {
                                    ++after_index;
                                    if (static_cast<s32>(after_index - 1u) >= 0 &&
                                        rt.memory().load8(after_flags) != 0u) {
                                        const u32 after = word(rt, after_values);
                                        if (after != 0u) {
                                            put(rt, after + 4u, before);
                                            put(rt, before + 8u, after);
                                            freed = true;
                                            break;
                                        }
                                    }
                                } else {
                                    ++after_index;
                                }
                                after_values += 4u;
                                after_flags += 1u;
                                if (after_index == 4u) {
                                    put(rt, before + 8u, 0u);
                                    freed = true;
                                    break;
                                }
                            }
                        } else {
                            put(rt, before + 8u, 0u);
                            freed = true;
                        }
                        if (freed) break;
                    }
                }
                values_ptr -= 4u;
                flags_ptr -= 1u;
                if (counter == index) break;
            }
        }
        if (!freed) {
            // No active slot before the removed one: the first one after it becomes the head.
            u32 after_index = index + 1u;
            if (after_index < 4u) {
                u32 after_values = self + after_index * 4u;
                u32 after_flags = self + index + 0x21u;
                for (;;) {
                    if (static_cast<s32>(after_index) < static_cast<s32>(count)) {
                        ++after_index;
                        if (static_cast<s32>(after_index - 1u) >= 0 &&
                            rt.memory().load8(after_flags) != 0u) {
                            const u32 after = word(rt, after_values);
                            if (after != 0u) {
                                put(rt, after + 4u, 0u);
                                break;
                            }
                        }
                    } else {
                        ++after_index;
                    }
                    after_values += 4u;
                    after_flags += 1u;
                    if (after_index == 4u) break;
                }
            }
        }
    }
    const u32 result = call(rt, ctx, 0x08B2CE7Cu, {word(rt, self + 0x2Cu), sub});
    ctx.gpr[2] = result;
    tail_call(ctx, 0x08B2CE7Cu, {word(rt, self + 0x2Cu), node});
}

// 0x08A9A704 (a0 = the output object, a1 = a container: word +0x14 = begin and word +0x18 = end of an array of object
// pointers): builds the text of the container in a frame builder (sp + 8: 0x08AAB5DC constructs it, 0x08AABA8C appends
// a literal / an object and 0x08AAB8F0 an integer) — a first literal (0x08CB23D4), the answer of the container's
// virtual at +0x24 through 0x08A7BDB0, the literal again, the element count, 0x08CB23D8 and 0x08CB23DC — then each
// element (separated by 0x08CB23E0; a null element appends 0x08CB23E4, otherwise the element's virtual at +0xC fills
// the frame handle at sp + 4 and it is appended and released) until the text reaches 0x81 bytes (which appends
// 0x08CB23E8 and stops). It closes with 0x08CB23F0 and 0x08CB23F4, hands the text to 0x08B88F44(a0, text) and
// releases the builder. v0 = a0.

// A reference-counted record release, as 0x08BA42D0's tail has it three times: the count at +4 drops by one and, when
// it reached zero, the virtual at +8 runs (a0 = the record), then the count at +8 drops and the virtual at +0xC runs
// when that reached zero too. v0 ends as the original leaves it: 1 when no virtual ran, else the last one's result.
void release_record(Runtime &rt, AllegrexContext &ctx, u32 record) {
    if (record == 0u) return;
    const u32 first = word(rt, record + 4u);
    put(rt, record + 4u, first - 1u);
    ctx.gpr[2] = 1u;
    if (first != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, record) + 8u), {record});
    const u32 second = word(rt, record + 8u);
    put(rt, record + 8u, second - 1u);
    ctx.gpr[2] = 1u;
    if (second != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, record) + 0xCu), {record});
}

// 0x08BA42D0 (a0 = a pair {object word, control word}): builds the two reference-counted temporaries with the allocator
// 0x08B62344 (0x2C bytes through 0x088B1214, and 0x68 bytes through 0x088B0418 with the incremented pair at sp + 0x10
// and the 0x2C block at sp + 8), each with a 0x10-byte record {vtable, 1, 1, the block} handed to 0x08B8E380 /
// 0x08B97820, releases the 0x68 block through 0x088B0C18, then releases the three records from the frame. v0 as the
// last of those leaves it.

// 0x088CC988 (a0 = the scene/object, a1 = a mode (4 or not), a2 = a descriptor word, a3 = an instance index, t0 and t1
// = two words the function answers back in v0 / v1, t2 = an optional object, f12 and f13 = two floats): the state at
// +0x63C answers its virtual at +0x1C, and that answer's virtual at +8 receives the descriptor block the function
// builds in the frame at sp + 0x48 ({3, mode == 4 ? 2 : 0, mode, a2, f12, 0}). The block's floats at +0x28/+0x2C/+0x30
// and 0x089545BC's matrix at sp + 0x10 (with the byte at +0x2C set to 1 first, which the optional t2's virtual at
// +0x18 may clear) fill the instance at a0 + 40 * a3: the position at +0x38/+0x3C/+0x40, the vector at +0x44..+0x4C
// (normalised through 0x08B4A720 / 0x08B4A5E8 unless the byte was cleared), and the two words at +0x50/+0x54. The
// vector is scaled by 0.04 into the frame and added to itself with 0x08B4AA34. v0 / v1 = t0 / t1.

// 0x08C36114 (a0 = a heap array of 12-byte elements {word value at +0, string at +4, flag byte at +8}, a1 = the hole
// index, a2 = the element count, a3 = the value about to be pushed): the sift-down of a heap, children 2i+1 and 2i+2.
// Each level picks the right child unless its string's word at +0x14 is below the left one's; on equal lengths
// 0x08B35FD8(right value, left value) breaks the tie (a negative answer picks the left). The chosen element is copied
// into the hole (the value word, 0x08A5BAD8 for the string, the flag byte), and the hole moves down. A right child
// index equal to the count moves the last element (count - 1) into the hole once. The tail builds the value's copy in
// the frame (value word, 0x08AAE214 for the string, flag byte), hands it to 0x08C35F34(array, hole, a1, copy) and
// destroys the copy's string.
void func_08C36114(Runtime &rt, AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    const u32 index_argument = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 first_word = frame.at(0x10u);
    put(rt, first_word, index_argument);
    put(rt, frame.at(0x14u), value);
    ctx.gpr[23] = base;
    ctx.gpr[30] = count;
    const u32 index = word(rt, first_word);
    u32 hole = index;
    u32 child = hole * 2u + 2u;
    if (static_cast<s32>(child) < static_cast<s32>(count)) {
        do {
            const u32 right = base + child * 12u;
            const u32 left_index = child - 1u;
            const u32 left = base + left_index * 12u;
            u32 chosen_index = child;
            u32 chosen_value;
            u32 right_string = word(rt, right + 4u);
            if (right_string == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
            u32 left_string = word(rt, left + 4u);
            u32 right_length = word(rt, right_string + 0x14u);
            if (left_string == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
            const u32 left_length = word(rt, left_string + 0x14u);
            if (static_cast<s32>(right_length) < static_cast<s32>(left_length)) {
                chosen_index = left_index;
                chosen_value = word(rt, left);
            } else {
                right_string = word(rt, right + 4u);
                if (right_string == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
                left_string = word(rt, left + 4u);
                right_length = word(rt, right_string + 0x14u);
                if (left_string == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
                const u32 left_length_again = word(rt, left_string + 0x14u);
                if (static_cast<s32>(left_length_again) < static_cast<s32>(right_length)) {
                    chosen_index = child;
                    chosen_value = word(rt, right);
                } else {
                    const u32 right_value = word(rt, right);
                    const u32 left_value = word(rt, left);
                    if (static_cast<s32>(call(rt, ctx, 0x08B35FD8u, {right_value, left_value})) < 0) {
                        chosen_index = left_index;
                        chosen_value = left_value;
                    } else {
                        chosen_index = child;
                        chosen_value = right_value;
                    }
                }
            }
            const u32 chosen = base + chosen_index * 12u;
            const u32 hole_address = base + hole * 12u;
            put(rt, hole_address, chosen_value);
            if (hole_address + 4u != chosen + 4u) call(rt, ctx, 0x08A5BAD8u, {hole_address + 4u, word(rt, chosen + 4u)});
            rt.memory().store8(hole_address + 8u, rt.memory().load8(chosen + 8u));
            hole = chosen_index;
            child = hole * 2u + 2u;
        } while (static_cast<s32>(child) < static_cast<s32>(count));
    }
    if (count == child) {
        const u32 left_index = child - 1u;
        const u32 left = base + left_index * 12u;
        const u32 hole_address = base + hole * 12u;
        const u32 left_value = word(rt, left);
        put(rt, hole_address, left_value);
        if (hole_address + 4u != left + 4u) call(rt, ctx, 0x08A5BAD8u, {hole_address + 4u, word(rt, left + 4u)});
        rt.memory().store8(hole_address + 8u, rt.memory().load8(left + 8u));
        hole = left_index;
    }
    const u32 temp = frame.at(0u);
    const u32 temp_string = frame.at(4u);
    const u32 input = word(rt, frame.at(0x14u));
    put(rt, temp, word(rt, input));
    call(rt, ctx, 0x08AAE214u, {temp_string, input + 4u});
    rt.memory().store8(temp + 8u, rt.memory().load8(input + 8u));
    call(rt, ctx, 0x08C35F34u, {base, hole, index, temp, temp_string});
    call(rt, ctx, 0x08A5B9ECu, {temp_string});
}

// 0x08B81630 (a0 = the allocator/owner, a1 and a2 = two bignums: word +0x10 = the number of 32-bit words, each holding
// two base-2^16 digits (low half first), the digits at +0x14): the larger of the two is kept as `big`, `total` is the sum
// of the word counts, and 0x08B82260(owner, word(big + 4) + (word(big + 8) < total)) gives the result block, whose
// +0x14 is zeroed for `total` words. Each word of `small` then folds its low and its high digit over `big` with the
// two 16-bit halves of the accumulate (the `mtlo`/`madd`/`mflo` pairs: LO = value + digit * half, mod 2^32, which is
// plain u32 arithmetic), carrying between digits, and each pass stores its final carry in the following word. The tail
// trims the trailing zero words into the size at +0x10 and answers the block.

// 0x08A155B8 (a0 = object, a1 = point, a2 = point, a3 = float out, t0 = float out): returns 0 when the point is behind
// the object's slope (0.5 * (+0x14 + +0x1C) against 0.5 * (a1.x + a1.z), read from +0x10 swapped). Otherwise it picks
// the smaller of |a2.x - low + 1| and |a2.z - low + 1| (high for the swapped layout), compares the pair along +0x18
// or +0x20, and either answers 1 with the larger non-negative component at a3, or asks 0x08B66FBC for the angle of the
// distance (f12) at that component (f13): a3 = the distance, t0 = the angle / pi * 180, and v0 = (that angle < +0xC).
void func_08A155B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 out_distance = ctx.gpr[7];
    const u32 out_angle = ctx.gpr[8];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = object;
    ctx.gpr[18] = out_distance;
    ctx.gpr[17] = out_angle;
    put(rt, out_angle, 0u);
    put_float(rt, out_distance, 10000.0f);
    const float low = float_at(rt, object + 0x14u);
    const float high = float_at(rt, object + 0x1Cu);
    if (!(psp_mul(low + high, 0.5f) < psp_mul(float_at(rt, first) + float_at(rt, first + 8u), 0.5f))) {
        ctx.gpr[2] = 0u;
        return;
    }
    const float second_x = float_at(rt, second);
    const float second_z = float_at(rt, second + 8u);
    float fx;
    float fz;
    if (rt.memory().load8(object + 0x10u) == 0u) {
        fx = (second_x - low) + 1.0f;
        fz = (second_z - low) + 1.0f;
    } else {
        fx = (second_x - high) + 1.0f;
        fz = (second_z - high) + 1.0f;
    }
    // The join both branches reach: a3 = the distance between the pair, t0 = the angle in degrees, v0 its comparison
    // with the object's limit.
    const auto finish = [&](float candidate, float distance) {
        const float angle = psp_mul(call_guest(rt, ctx, 0x08B66FBCu, {}, {distance, candidate}).f0 /
                                        std::bit_cast<float>(0x40490FDBu),
                                    180.0f);
        put_float(rt, out_distance, std::sqrt(psp_mul(candidate, candidate) + psp_mul(distance, distance)));
        put_float(rt, out_angle, angle);
        ctx.gpr[2] = angle < float_at(rt, object + 0x0Cu) ? 1u : 0u;
    };
    const float pair = std::fabs(fx) < std::fabs(fz) ? fx : fz;
    const float limit = float_at(rt, object + 0x18u);
    const float along = float_at(rt, second + 0x0Cu);
    if (!(along <= limit)) {
        finish(pair, limit - along);
        return;
    }
    const float other_limit = float_at(rt, object + 0x20u);
    const float other = float_at(rt, second + 4u);
    if (!(other_limit <= other)) {
        if (0.0f <= fx) {
            ctx.gpr[2] = 1u;
            if (fx < fz) fx = fz;
            put_float(rt, out_distance, fx);
            return;
        }
        if (0.0f <= fz) {
            ctx.gpr[2] = 1u;
            put_float(rt, out_distance, fz);
            return;
        }
        ctx.gpr[2] = 0u;
        return;
    }
    finish(pair, other - other_limit);
}

// 0x08AFBFEC (a0 = a vertex array of 12-byte xyz triples, a1..t1 = five indices): builds the four edge vectors from the
// vertex at a1 (to the ones at a2, a3, t0 and t1) in the frame and folds them with 0x08AFBF8C three times (into the
// frame at sp, sp + 0xC and sp + 0x18, against sp + 0x24), which gives three values. The global block at 0x08D47308
// holds a cached tolerance, refreshed from the values' magnitude times -0.0001 through 0x08B6441C / 0x08B64494; the
// point is inside when each value lies between -tolerance and +tolerance. v0 = 1 or 0.

// 0x08A1CB1C (a0 = the object): 0x08ACB4F8(a0) first, then the word at +0x24 (a 0x8011xxxx kind) selects a small
// number through the binary decision tree the compiler built. Every leaf of the tree is an equality (the ranges the
// tree narrows are single values), so the whole tree is a switch; 0xD is its default, 0, 1 and 2 answer themselves.
// 0x307, 0x383, 0x385 and 0x3C7 are not immediates of the asm: they are the single values of the ranges the tree
// narrows around 0x306 / 0x308, 0x382 / 0x384, 0x384 / 0x386 and 0x3C6 / 0x3C8.
constexpr u32 kind_code(u32 low) { return 0x80110000u | low; }

// 0x08B114CC (a0 = the caller's object, a1 = the state object): 0x08B0DD88(a1) first; when it answers 0 nothing else
// runs. The word at +0x7C of the state object picks a virtual (0x4C / 0x50 / 0x64 / 0x54 / 0x58 / 0x5C / 0x60 / 0x70
// with (a0, a1), or 0x48 checked first for states 0 / 1), and everything but the 0 / 1 failure path also calls
// 0x08B114A4 and marks s3. A state 0xE or above stores 2 at +0xA8. Then the state object's own update sequence
// (0x08B0E31C, 0x08B34888 on +4, and one of 0x08B11434 / 0x08B10CC4 picked by the s4 flag when +0xE9 is zero and
// +0xA4 is not 3, a default 1 at +0xA8, 0x08B10A40 when s3 is clear, 0x08B34B24 on +4) and, with s3 set, a tail call
// of the virtual at +0x24 of the state object.

} // namespace
} // namespace new_batch_main_209
} // namespace gt::game

#define MAIN209(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_209::name, gt::ReturnKind::kind)
MAIN209(0x08A155B8u, func_08A155B8, Word);
MAIN209(0x08C36114u, func_08C36114, Word);
MAIN209(0x08B3F4D0u, func_08B3F4D0, Word);

// ---- from main-session batch 210 (draft 4399f5b3060ba3f2982f7cf6f5eea6eaca68a0df) ----
// New batch 210 (main session): emitted functions that no src/ file defines or registers, written by hand
// from `tools/disasm` as structured C. Draft batch: no gate was run on them here (a separate pass verifies),
// so no name has evidence and every function keeps func_XXXXXXXX. The synthetic-input shapes of the eight
// functions are in /tmp/claude-1000/shapes_main210.txt (one line per address).
//
// 0x08C65F04 is left out on purpose: it moves sp by a size computed from a variadic argument list
// (`addiu $sp, $sp, -0x150` twice, a float block built at sp and a printf-like call at 0x08C6F354), the
// shape that has to be read together with its caller.
// 0x08B649D4 is left out too: its recorded size (604 bytes in recomp/analysis/functions.tsv) ends at
// 0x08B64C30, but the tail blocks at 0x08B64C08-0x08B64C64 jump back into the middle of its byte-refill
// loop, so the code the function really runs reaches past the recorded boundary and which part the
// recompiled original covers is ambiguous.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_210 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what
// the next instruction sees, and a callee that sets no v0 hands the previous one on to the code after it.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A virtual call: `lw v1, (object); lw v0, slot(v1); jalr v0` with a0 = object. v1 and v0 hold the vtable and
// the target as the callee starts, and its v0 / v1 / f0 stay afterwards.
u32 call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot) {
    const u32 vtable = word(rt, object);
    const u32 target = word(rt, vtable + slot);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = target;
    return call_keep(rt, ctx, target, {object}).v0;
}

// The callee-saved registers s0-s7 and fp as at entry (the original saves them in its frame and restores them
// before it returns or tail-calls).
class SavedRegs {
public:
    explicit SavedRegs(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_fp_ = ctx.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 saved_fp_;
};

// A callee-saved floating-point register as at entry ($f20-$f25 in these functions).
class SavedFloat {
public:
    SavedFloat(AllegrexContext &ctx, unsigned index) : ctx_(ctx), index_(index), saved_(ctx.fpr[index]) {}
    ~SavedFloat() { ctx_.fpr[index_] = saved_; }
    SavedFloat(const SavedFloat &) = delete;
    SavedFloat &operator=(const SavedFloat &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned index_;
    float saved_;
};

u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The ll / sc decrement of a counter (one thread: a plain load and store). The old value is left in a0, v0 = 1
// (the sc's success flag, which the delay slot of its retry branch re-sets) and v1 the address.
u32 decrement_counter(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}

// The Allegrex `div` of a signed pair: a zero divisor leaves LO all ones for a non-negative dividend and 1
// for a negative one (the instruction raises no exception), where C++ division would trap.
s32 mips_div(s32 dividend, s32 divisor) {
    if (divisor == 0) return dividend < 0 ? 1 : -1;
    if (divisor == -1) return static_cast<s32>(0u - static_cast<u32>(dividend));
    return dividend / divisor;
}

// ---------------------------------------------------------------------------------------------------
// 0x08BB0608 (a0 = object): a destructor. It stores three vtables in turn (0x08CC2FC0 at +0, then
// 0x08CC2FE0 and finally 0x08CC3000) and lets the reference-counted members go: the list at +0x58 goes to
// 0x08B60804(list - 4, -1) and, when that answer is not positive, to 0x08B93554(list - 0xC, sp); the four
// objects at +0x54, +0x4C, +0x3C and +0x2C have a strong counter at +4 (slot 8 of the vtable) and a weak
// one at +8 (slot 0xC), and object +0x24 only the weak one at +8. A counter whose old value was 1 releases
// the object. v0 ends as 0x08CC3000.
void func_08BB0608(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        put(rt, self, 0x08CC2FC0u);  // the delay slot of the first test: it runs on both paths
        const u32 list = word(rt, self + 0x58u);
        if (list - 0xCu != 0x08D75298u) {
            const u32 taken = call_keep(rt, ctx, 0x08B60804u, {list - 4u, 0xFFFFFFFFu}).v0;
            if (static_cast<s32>(taken) <= 0) (void)call_keep(rt, ctx, 0x08B93554u, {list - 0xCu, frame.at(0u)});
        }
        // The strong/weak pair of +0x54, +0x4C and +0x3C, in that order.
        for (const u32 offset : {0x54u, 0x4Cu, 0x3Cu}) {
            const u32 object = word(rt, self + offset);
            ctx.gpr[16] = object;
            if (object == 0u) continue;
            if (decrement_counter(rt, ctx, object + 4u) != 1u) continue;
            (void)call_slot(rt, ctx, object, 8u);
            if (decrement_counter(rt, ctx, object + 8u) != 1u) continue;
            (void)call_slot(rt, ctx, object, 0xCu);
        }
        // The second vtable is stored in the delay slot of the +0x2C test, before that member is released.
        put(rt, self, 0x08CC2FE0u);
        const u32 strong = word(rt, self + 0x2Cu);
        ctx.gpr[16] = strong;
        if (strong != 0u && decrement_counter(rt, ctx, strong + 4u) == 1u) {
            (void)call_slot(rt, ctx, strong, 8u);
            if (decrement_counter(rt, ctx, strong + 8u) == 1u) (void)call_slot(rt, ctx, strong, 0xCu);
        }
        const u32 weak_only = word(rt, self + 0x24u);
        ctx.gpr[16] = weak_only;
        if (weak_only != 0u && decrement_counter(rt, ctx, weak_only + 8u) == 1u) (void)call_slot(rt, ctx, weak_only, 0xCu);
        put(rt, self, 0x08CC3000u);
    }
    ctx.gpr[2] = 0x08CC3000u;
}

// ---------------------------------------------------------------------------------------------------
// 0x0895C140 (a0 = self): q = self[0xC], r = self[8], P = q + 0x270 (per-side data: bytes at P + 0x14 + i,
// floats at P + 0x1C + 4 * i). When P[0] == 7, 0x08948324(self, 7) runs first. For the sides i = 0, 1
// (a = 2 * i, b = a ^ 1) whose byte P[0x14 + i] - 6 is 0 or 1 and while r[0x6C4] is not below 0x4031C71D:
// g = the float 0x0895B5B0(self, sp) wrote to sp when it answers non-zero, otherwise (while r[0x6C4] is not
// above 0x42480001) g = min(|r[0x520]| * 0x40747645, 1.0f); p = P[0x1C + 4 * i] * g (nothing when p is
// 0.0f); A = 0x08955300(self, a), B = 0x08955300(self, b), m = (A[0x34] + B[0x34]) * 0.5f raised by m / 8
// when r[0x520] > 0 and lowered by m / 8 when r[0x520] < 0; B[0x34] < m gives B[0x9C] += p * 0x3EE00000 and
// A[0x9C] -= p * 0x3F100000, m < B[0x34] gives B[0x9C] -= p * 0x3F100000 and A[0x9C] += p * 0x3EE00000.
// v0 ends as 2 (the loop test's `addiu v0, zero, 2`), the value the compared cases of batch 96 left.
void func_0895C140(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 q = word(rt, self + 0xCu), r = word(rt, self + 8u);
    GuestFrame frame(ctx, 0x40u);
    SavedFloat saved_f20(ctx, 20u), saved_f21(ctx, 21u);
    const u32 block = q + 0x270u;
    if (byte_u(rt, block) == 7u) (void)call_keep(rt, ctx, 0x08948324u, {self, 7u});
    const u32 slot = frame.at(0u);
    for (u32 i = 0u; i != 2u; ++i) {
        const u32 a = 2u * i, b = a ^ 1u;
        if (byte_u(rt, block + i + 0x14u) - 6u > 1u) continue;
        put(rt, slot, 0u);
        if (float_at(rt, r + 0x6C4u) < std::bit_cast<float>(0x4031C71Du)) continue;
        float g;
        if (call_keep(rt, ctx, 0x0895B5B0u, {self, slot}).v0 != 0u) {
            g = float_at(rt, slot);
        } else {
            if (std::bit_cast<float>(0x42480001u) < float_at(rt, r + 0x6C4u)) continue;
            g = float_at(rt, r + 0x520u);
            put_float(rt, slot, g);
            if (g < 0.0f) {
                g = -g;
                put_float(rt, slot, g);
            }
            g = psp_mul(float_at(rt, slot), std::bit_cast<float>(0x40747645u));
            put_float(rt, slot, g);
            if (1.0f < g) {
                g = 1.0f;
                put_float(rt, slot, g);
            }
        }
        const float amount = psp_mul(float_at(rt, block + 0x1Cu + 4u * i), g);
        if (amount == 0.0f) continue;
        const u32 first = call_keep(rt, ctx, 0x08955300u, {self, a}).v0;
        const u32 second = call_keep(rt, ctx, 0x08955300u, {self, b}).v0;
        const float other = float_at(rt, second + 0x34u);
        const float side = float_at(rt, r + 0x520u);
        float middle = psp_mul(float_at(rt, first + 0x34u) + other, 0.5f);
        const float eighth = psp_mul(middle, std::bit_cast<float>(0x3E000000u));
        if (0.0f < side)
            middle = middle + eighth;
        else if (side < 0.0f)
            middle = middle - eighth;
        const float big = psp_mul(amount, std::bit_cast<float>(0x3F100000u));
        const float small = psp_mul(amount, std::bit_cast<float>(0x3EE00000u));
        if (other < middle) {
            put_float(rt, second + 0x9Cu, small + float_at(rt, second + 0x9Cu));
            put_float(rt, first + 0x9Cu, float_at(rt, first + 0x9Cu) - big);
        } else if (middle < other) {
            put_float(rt, second + 0x9Cu, float_at(rt, second + 0x9Cu) - big);
            put_float(rt, first + 0x9Cu, small + float_at(rt, first + 0x9Cu));
        }
    }
    ctx.gpr[2] = 2u;
}

// ---------------------------------------------------------------------------------------------------
// 0x089CF330 (a0 = self, a1 = key, a2 = value, a3 = holder): when 0x089CE09C(self, key) answers 0 nothing
// happens (v0 = 0). Otherwise 0x08B344E0 locks 0x08CF5134 and 0x089CE1F4(self, key)'s word at +0xC is
// compared with word(a3): equal unlocks and returns, and a zero word(a3) skips to the second half. With a
// non-zero word(a3) a temporary is built at sp + 4 from (a3) through 0x08969178 and a node is filled:
// 0x089BA6CC(node, self + 0xE8), 0x089687D8(node, 0x089CF134(self, key)), 0x08968850(node,
// 0x089CF084(self, key)), node[0x60] |= 8 and, when a2 is not 0, 0x089BAA60(node, a2); the temporary is
// destroyed. The second half builds another temporary at sp from 0x08CF6ED8 through 0x08969188, unlocks,
// copies the element 0x089CE1F4(self, key) + 0xC into it (over the slot when they differ) and, when key
// differs from self[0x198], sets self[0x1C0] = 1; then unlocks, and when the temporary's word is not 0
// (after the assertion 0x08CA6984) releases it with 0x089BA4D8(holder) and 0x089BA6CC(0) and destroys it.
// Every assertion is 0x08A5BB7C(0x08CA6984) followed by the same work with the value a null read gave.
// The seven `lw $s0, 4($sp)` / `lw $s0, 0($sp)` of `tools.sp_loads` are the temporary's word being re-read
// (word(rt, frame.at(4u)) / word(rt, frame.at(0u))) before each use, as the original does.
void func_089CF330(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], key = ctx.gpr[5], value = ctx.gpr[6], holder = ctx.gpr[7];
    const u32 assertion = 0x08CA6984u, lock = 0x08CF5134u;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        ctx.gpr[19] = key;
        ctx.gpr[20] = value;
        ctx.gpr[18] = holder;
        if (call_keep(rt, ctx, 0x089CE09Cu, {self, key}).v0 == 0u) return;
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        const u32 found = call_keep(rt, ctx, 0x089CE1F4u, {self, key}).v0;
        if (word(rt, holder) == word(rt, found + 0xCu)) {
            (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
            return;
        }
        const u32 temporary = frame.at(4u);
        if (word(rt, holder) != 0u) {
            // Each use re-reads the temporary's word and, when it is 0, runs the assertion first.
            const auto checked = [&]() {
                const u32 node = word(rt, temporary);
                if (node == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {assertion});
                return node;
            };
            (void)call_keep(rt, ctx, 0x08969178u, {temporary, holder});
            (void)call_keep(rt, ctx, 0x089BA6CCu, {checked(), self + 0xE8u});
            (void)call_keep(rt, ctx, 0x089687D8u, {checked(), call_keep(rt, ctx, 0x089CF134u, {self, key}).v0});
            (void)call_keep(rt, ctx, 0x08968850u, {checked(), call_keep(rt, ctx, 0x089CF084u, {self, key}).v0});
            const u32 node = checked();
            put(rt, node + 0x60u, word(rt, node + 0x60u) | 8u);  // the store is the delay slot of the value's test
            if (value != 0u) (void)call_keep(rt, ctx, 0x089BAA60u, {checked(), value});
            (void)call_keep(rt, ctx, 0x08A5B9ECu, {temporary});
        }
        (void)call_keep(rt, ctx, 0x08969188u, {frame.at(0u), 0x08CF6ED8u});
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        const u32 element = call_keep(rt, ctx, 0x089CE1F4u, {self, key}).v0 + 0xCu;
        if (frame.at(0u) != element) (void)call_keep(rt, ctx, 0x08A5BAD8u, {frame.at(0u), word(rt, element)});
        if (holder != element) (void)call_keep(rt, ctx, 0x08A5BAD8u, {element, word(rt, holder)});
        if (key != word(rt, self + 0x198u)) put(rt, self + 0x1C0u, 1u);
        (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
        u32 released = word(rt, frame.at(0u));
        if (released != 0u) {
            if (value != 0u) {
                (void)call_keep(rt, ctx, 0x089BA4D8u, {released, value});
                released = word(rt, frame.at(0u));
            }
            if (released == 0u) {
                (void)call_keep(rt, ctx, 0x08A5BB7Cu, {assertion});
                released = 0u;
            }
            (void)call_keep(rt, ctx, 0x089BA6CCu, {released, 0u});
        }
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    }
}

// ---------------------------------------------------------------------------------------------------
// 0x08C2E514 (a0 = first, a1 = middle, a2 = last, pointers to 4-byte reference-counted elements): the
// gcd-cycle rotate. first == middle returns with v0 untouched; last == middle returns v0 = last - first.
// Otherwise k = (middle - first) / 4 and l = (last - first) / 4 - k; k == l is swap_ranges(first, middle,
// middle) through 0x08C2E14C (which returns the moved-to end) and returns its v0. Else d = gcd(n, k)
// (0x08BF8BD4) and the classic cycle loop runs d times: a temporary built from *first by 0x08A76ABC, then,
// for k < l, l / d steps of `if (p > first + l) { *p = *(p - l); p -= l; } *p = *(p + k); p += k;` and
// otherwise k / d - 1 steps of `if (p < last - k) { *p = *(p + k); p += k; } *p = *(p - l); p -= l;`,
// ending with *p = temporary; first and first + l advance one element per cycle. Every element store is the
// 0x08A5BAD8(destination, *source) assignment and the temporary is destroyed by 0x08A5B9EC; v0 ends as the
// last 0x08A5B9EC answer.
void func_08C2E514(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], middle = ctx.gpr[5], last = ctx.gpr[6];
    if (first == middle) return;
    if (last == middle) {
        ctx.gpr[2] = last - first;
        return;
    }
    {
        GuestFrame frame(ctx, 0x60u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = first;
        ctx.gpr[16] = last;
        const s32 total = static_cast<s32>(last - first) >> 2;
        const s32 k = static_cast<s32>(middle - first) >> 2;
        const s32 l = total - k;
        put(rt, frame.at(0x1Cu), static_cast<u32>(k));
        put(rt, frame.at(0x18u), static_cast<u32>(l));
        if (k == l) {
            (void)call_keep(rt, ctx, 0x08C2E14Cu, {first, middle, middle});
            return;
        }
        const s32 divisor = static_cast<s32>(call_keep(rt, ctx, 0x08BF8BD4u, {static_cast<u32>(total), static_cast<u32>(k)}).v0);
        if (divisor <= 0) return;
        const bool forwards = k < l;
        const u32 step = static_cast<u32>(k) << 2, other_step = static_cast<u32>(l) << 2;
        put(rt, frame.at(0x14u), 0u);                        // the cycle counter
        put(rt, frame.at(0x10u), last - step);               // last - k
        put(rt, frame.at(0x20u), forwards ? 1u : 0u);
        u32 cursor = first;                                  // the element each cycle starts from
        u32 end_of_forward_range = first + other_step;       // first + l
        do {
            (void)call_keep(rt, ctx, 0x08A76ABCu, {frame.at(0u), cursor});
            u32 resting = cursor;
            u32 p = cursor;
            if (forwards) {
                // k < l: l / d steps from p, walking forward by k and back by l.
                const s32 cycles = l / divisor;
                if (cycles > 0) {
                    for (s32 j = 0; j != cycles; ++j) {
                        if (end_of_forward_range < p) {
                            const u32 source = p - other_step;
                            if (source != p) (void)call_keep(rt, ctx, 0x08A5BAD8u, {p, word(rt, source)});
                            p = source;
                        }
                        const u32 next = p + step;
                        if (next != p) (void)call_keep(rt, ctx, 0x08A5BAD8u, {p, word(rt, next)});
                        p = next;
                    }
                    resting = p;
                }
            } else {
                // k >= l: k / d - 1 steps from p, walking forward by k and back by l.
                const s32 cycles = k / divisor - 1;
                if (cycles > 0) {
                    for (s32 j = 0; j != cycles; ++j) {
                        const u32 box = word(rt, frame.at(0x10u));
                        const u32 next = p + step;
                        if (p < box) {
                            if (p != next) (void)call_keep(rt, ctx, 0x08A5BAD8u, {p, word(rt, next)});
                            p = next;
                        }
                        const u32 back = p - other_step;
                        if (p != back) (void)call_keep(rt, ctx, 0x08A5BAD8u, {p, word(rt, back)});
                        p = back;
                    }
                    resting = p;
                }
            }
            if (resting != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {resting, word(rt, frame.at(0u))});
            const u32 cycle = word(rt, frame.at(0x14u)) + 1u;
            ctx.gpr[19] = cursor + 4u;
            put(rt, frame.at(0x14u), cycle);
            (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
            cursor += 4u;
            end_of_forward_range += 4u;
        } while (static_cast<u32>(divisor) != word(rt, frame.at(0x14u)));
    }
}

// ---------------------------------------------------------------------------------------------------
// 0x08A01C8C (a0 = owner, a1 = a byte flag, a2 = a step, f12 / f13 = two floats): 0x08A01014(owner, sp,
// sp + 4) answers n; with the flag set s3 = step and s2 = (n - 1) / step, without it s2 = step and
// s3 = (n - 1) / step. sp + 8 = s3 * sp[0] + 2 * f12 + (s3 - 1) * f13 and sp + 0xC = 2 * f12 + s2 * sp[4] +
// (s2 - 1) * f13 go to 0x0896A9C4(owner, 0, 0, sp + 8, sp + 0xC). Then for each node of the list at
// owner[0xF4]: a node whose word +0xE0 does not have bit 0x1000 asks 0x0896A35C(node, 0, 0, sp + 8, sp + 0xC)
// and hands 0x0896A9C4(node, sp + 0x14, sp + 0x10, 0, 0) the two floats (f20 + *sp/2, f21 + *(sp+4)/2) of
// the values it read; with the flag set the walk counts along x (f21 advances by f13 + sp[0] each node,
// re-seeding f21 = f12 + sp[0] / 2 and advancing f20 by f13 + sp[4] every s3 nodes), without it along y (the
// same with f20 / sp[4] / f12 / f13 / s2 swapped). 0x08968A38(node) gives the next node. v0 ends as the
// last 0x08968A38 answer, or as s3 - 1 when the list is empty.

// ---------------------------------------------------------------------------------------------------
// 0x0896108C (a0 = holder, a1 = the 0x30-byte element to place): the holder's word +0 is the array of
// element pointers, +4 its capacity and +8 its count (also stored at +0xC). With count 0 the element is
// copied onto word(array) and both counts become 1. Otherwise the array is walked from the start for the
// first pointer whose float at +0xC is above the element's (elements stay sorted by that float):
//   - found with count < capacity: the array's [position, end) shifts up one slot (the word at end, the
//     free slot's pointer, is saved), position gets it and the element is copied onto it; both counts + 1;
//   - found with count >= capacity: [start, position - 4) shifts down one slot and position - 4 gets the
//     start's old pointer, onto which the element is copied; the counts stay;
//   - not found (position == end): with capacity left the element is copied onto word(end); otherwise
//     [start, end - 4) shifts down, end - 4 gets the start's old pointer and the element is copied onto it.
// v0 = 1 (empty), the new count (copied) or 0 (a full array, nothing copied).

// ---------------------------------------------------------------------------------------------------
// 0x08C51360 (a0 = owner, a1 = a flag, a2 = an optional record with its flag at +0x10 and its string at
// +0x14 / +0x18, a3 = a record with its flag at +0 and its string at +4 / +8): computes the "before" flag
// fp for the tree insert and builds a 0x20-byte node from a3: node[0x10] = the byte at a3 + 0,
// node[0x14] = 0x08B6461C(node[0x18]) with the bytes of a3's string copied into it, node[0x18] = a3[8] and
// node[0x1C] = a3[0xC]. Then 0x08B5F2B8(fp, node, a2, owner + 4) runs and owner[0x14] += 1; v0 = the node.
// fp = 1 when a1 is not 0, when a2 is owner + 4, when a2's flag is set and a3's is clear, when a3's string
// is shorter than a2's, or when a3's string compares below a2's at the first difference; fp = 0 otherwise.
// A second loop re-reads both strings from the start (the order the original's inlined compare leaves) and
// always answers 0; both strings are read byte by byte up to the length they share.
void func_08C51360(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4], flag = ctx.gpr[5], optional = ctx.gpr[6], record = ctx.gpr[7];
    u32 before = 0u;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        ctx.gpr[21] = owner;
        ctx.gpr[22] = optional;
        ctx.gpr[20] = record;
        bool decided = true;
        if (flag != 0u || optional == owner + 4u) {
            before = 1u;
        } else if (byte_u(rt, record) != 0u) {
            if (byte_u(rt, optional + 0x10u) == 0u) before = 0u; else decided = false;
        } else {
            if (byte_u(rt, optional + 0x10u) != 0u) before = 1u; else decided = false;
        }
        if (!decided) {
            const u32 record_length = word(rt, record + 8u);
            const u32 optional_length = word(rt, optional + 0x18u);
            // The second loop: both strings again, from their first byte, answering 0 on any difference.
            const auto recompare = [&]() -> u32 {
                if (static_cast<s32>(optional_length) <= 0) return 0u;
                const u32 optional_bytes = word(rt, optional + 0x14u);
                const u32 record_bytes = word(rt, record + 4u);
                if (byte_u(rt, optional_bytes) < byte_u(rt, record_bytes)) return 0u;
                if (!(byte_u(rt, record_bytes) < byte_u(rt, optional_bytes))) {
                    for (u32 index = 0u;;) {
                        index += 1u;
                        if (index == optional_length) return 0u;
                        if (byte_u(rt, optional_bytes + index) < byte_u(rt, record_bytes + index)) return 0u;
                        if (byte_u(rt, record_bytes + index) < byte_u(rt, optional_bytes + index)) return 0u;
                    }
                }
                return 0u;
            };
            if (static_cast<s32>(record_length) < static_cast<s32>(optional_length)) {
                before = 1u;
            } else if (static_cast<s32>(optional_length) < static_cast<s32>(record_length)) {
                before = 0u;
            } else if (static_cast<s32>(record_length) <= 0) {
                before = recompare();
            } else {
                const u32 record_bytes = word(rt, record + 4u);
                const u32 optional_bytes = word(rt, optional + 0x14u);
                if (byte_u(rt, record_bytes) < byte_u(rt, optional_bytes)) {
                    before = 1u;
                } else if (byte_u(rt, optional_bytes) < byte_u(rt, record_bytes)) {
                    before = recompare();
                } else {
                    for (u32 index = 0u;;) {
                        index += 1u;
                        if (index == record_length) {
                            before = recompare();
                            break;
                        }
                        if (byte_u(rt, record_bytes + index) < byte_u(rt, optional_bytes + index)) {
                            before = 1u;
                            break;
                        }
                        if (byte_u(rt, optional_bytes + index) < byte_u(rt, record_bytes + index)) {
                            before = recompare();
                            break;
                        }
                    }
                }
            }
        }
        const u32 allocated = call(rt, ctx, 0x08B35DA0u, {0x20u});
        ctx.gpr[19] = allocated + 0x10u;
        u32 node = ctx.gpr[17];  // s1 only takes the allocation below the (never taken) guard
        if (allocated + 0x10u != 0u) {
            node = allocated;
            ctx.gpr[16] = allocated + 0x14u;  // s0
            ctx.gpr[18] = record + 4u;        // s2
            const u32 length = word(rt, record + 8u);
            put_byte(rt, allocated + 0x10u, byte_u(rt, record));
            put(rt, allocated + 0x14u, 0u);
            if (static_cast<s32>(length) > 0) {
                put(rt, allocated + 0x18u, length);
                const u32 buffer = call(rt, ctx, 0x08B6461Cu, {length});
                if (static_cast<s32>(word(rt, allocated + 0x18u)) > 0) {
                    put(rt, allocated + 0x14u, buffer);
                    for (u32 index = 0u;;) {
                        const u32 source = word(rt, record + 4u) + index;
                        const std::uint8_t value = static_cast<std::uint8_t>(byte_u(rt, source));
                        const u32 destination = word(rt, allocated + 0x14u) + index;
                        index += 1u;
                        rt.memory().store8(destination, value);
                        if (!(static_cast<s32>(index) < static_cast<s32>(word(rt, allocated + 0x18u)))) break;
                    }
                }
            }
            put(rt, allocated + 0x1Cu, word(rt, record + 0xCu));
        }
        (void)call_keep(rt, ctx, 0x08B5F2B8u, {before, node, optional, owner + 4u});
        put(rt, owner + 0x14u, word(rt, owner + 0x14u) + 1u);
        ctx.gpr[2] = node;
    }
}

// ---------------------------------------------------------------------------------------------------
// 0x08C356C8 (a0 = vector, a1 = the position to insert at, a2 = the 0xC-byte element to insert): inserts
// the element into the vector of {word, managed pointer at +4, byte at +8} elements (0 + begin, +4 + end,
// +8 + capacity end).
//   - end != capacity end: when end is not 0 the last element (end - 0xC) is constructed at end through
//     0x08AAE214(end + 4, end - 8) and end advances by one; then the temporary element is built at sp + 4
//     from (a2) (its +0 word stored, 0x08AAE214(sp + 8, a2 + 4), its byte at sp + 0xC), the elements
//     [position, end - 0x18] move up one through 0x08A5BAD8 (the +0 word stored, the pointer assigned, the
//     byte stored), the temporary goes into position and 0x08A5B9EC(sp + 8) destroys it; v0 = its answer;
//   - end == capacity end: the length (end - begin) / 12 is checked against 0x15555555 (0x08B5ADDC of
//     0x08CB398C), the new capacity is twice the length (1 when empty, -4 on the overflow path) and the
//     buffer 0x08A5BC20(capacity * 12, 4) is filled: 0x08C3564C(begin, position, buffer, sp + 2) moves the
//     elements before position, the new element is constructed, 0x08C3564C(position, end, place + 0xC,
//     sp + 1) moves the rest, 0x08C35198(begin, end, sp) destroys the old range and 0x08A5BC54(begin,
//     ((capacity end - begin) >> 2) << 2, 4) frees the old buffer. The vector's three words become the
//     buffer, the new end and buffer + capacity * 12, which is also v0.
void func_08C356C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x40u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = vector;
        ctx.gpr[22] = position;
        ctx.gpr[19] = value;
        const u32 temporary = frame.at(4u);       // the element built from `value`
        const u32 temporary_part = frame.at(8u);  // its managed pointer
        u32 end = word(rt, vector + 4u);
        ctx.gpr[16] = end;
        if (end == word(rt, vector + 8u)) {
            // Grow: the classic _M_check_len + _M_realloc_insert.
            const u32 begin = word(rt, vector);
            u32 elements = static_cast<u32>(static_cast<s32>(end - begin) >> 2) * 0xAAAAAAABu;
            if (elements == 0x15555555u) elements = call(rt, ctx, 0x08B5ADDCu, {0x08CB398Cu});
            u32 bytes;
            if (elements == 0u) {
                bytes = 1u << 4;
                bytes -= 1u << 2;  // 1 * 12 bytes
            } else {
                const u32 doubled = elements << 1;
                bytes = doubled < elements ? 0xFFFFFFFCu : (doubled << 4) - (doubled << 2);
            }
            const u32 buffer = call(rt, ctx, 0x08A5BC20u, {bytes, 4u});
            ctx.gpr[18] = buffer;
            const u32 placed = call_keep(rt, ctx, 0x08C3564Cu, {begin, position, buffer, frame.at(2u)}).v0;
            ctx.gpr[16] = placed;
            if (placed != 0u) {
                put(rt, placed, word(rt, value));
                (void)call_keep(rt, ctx, 0x08AAE214u, {placed + 4u, value + 4u});
                put_byte(rt, placed + 8u, byte_u(rt, value + 8u));
            }
            const u32 new_end =
                call_keep(rt, ctx, 0x08C3564Cu, {position, word(rt, vector + 4u), placed + 0xCu, frame.at(1u)}).v0;
            ctx.gpr[16] = new_end;
            (void)call_keep(rt, ctx, 0x08C35198u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
            const u32 old_begin = word(rt, vector);
            const u32 old_capacity_end = word(rt, vector + 8u);
            if (old_begin != 0u) {
                const u32 old_bytes = static_cast<u32>(static_cast<s32>(old_capacity_end - old_begin) >> 2) << 2;
                (void)call_keep(rt, ctx, 0x08A5BC54u, {old_begin, old_bytes, 4u});
            }
            put(rt, vector + 8u, buffer + bytes);
            put(rt, vector + 4u, new_end);
            put(rt, vector, buffer);
            ctx.gpr[2] = buffer + bytes;
            return;
        }
        if (end != 0u) {
            const u32 last = end - 0xCu;
            put(rt, end, word(rt, last));  // the delay slot of the constructor call
            (void)call_keep(rt, ctx, 0x08AAE214u, {end + 4u, last + 4u});
            put_byte(rt, end + 8u, byte_u(rt, last + 8u));
            end = word(rt, vector + 4u);
        }
        end += 0xCu;
        put(rt, vector + 4u, end);
        put(rt, temporary, word(rt, value));  // the temporary's +0 word at sp + 4
        (void)call_keep(rt, ctx, 0x08AAE214u, {temporary_part, value + 4u});
        put_byte(rt, temporary_part + 4u, byte_u(rt, value + 8u));
        const s32 count = static_cast<s32>(static_cast<u32>(static_cast<s32>(end - 0x18u - position) >> 2) * 0xAAAAAAABu);
        if (count > 0) {
            u32 source_word = end - 0x24u, source_part = end - 0x18u;
            u32 destination_word = end - 0x18u, destination_part = end - 0xCu;
            for (s32 i = 0; i != count; ++i) {
                const u32 moved = word(rt, source_word);
                destination_part -= 0xCu;
                source_part -= 0xCu;
                put(rt, destination_word, moved);
                if (destination_part != source_part)
                    (void)call_keep(rt, ctx, 0x08A5BAD8u, {destination_part + 4u, word(rt, source_part + 4u)});
                put_byte(rt, destination_word + 8u, byte_u(rt, source_word + 8u));
                source_word -= 0xCu;
                destination_word -= 0xCu;
            }
        }
        put(rt, position, word(rt, temporary));
        if (position + 4u != temporary_part) (void)call_keep(rt, ctx, 0x08A5BAD8u, {position + 4u, word(rt, temporary_part)});
        put_byte(rt, position + 8u, byte_u(rt, temporary_part + 4u));
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {temporary_part});
    }
}

} // namespace
} // namespace new_batch_main_210
} // namespace gt::game

#define NEW210(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_210::name, gt::ReturnKind::kind)
NEW210(0x08BB0608u, func_08BB0608, Word);
NEW210(0x0895C140u, func_0895C140, Word);
NEW210(0x089CF330u, func_089CF330, Word);
NEW210(0x08C2E514u, func_08C2E514, Word);
NEW210(0x08C51360u, func_08C51360, Word);
NEW210(0x08C356C8u, func_08C356C8, Word);

