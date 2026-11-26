// New batch (main session 27): functions the recompiler emitted that nothing has decompiled yet, written as structured C from
// `tools.disasm` (no recompiler output was read). Each body keeps the original's order of calls and stores (a store in a `jal`'s
// delay slot happens before the call, and the callee may read it), leaves v0 as the original leaves it on every exit path, and
// reproduces the frames a callee reads or writes through (GuestFrame). No names: none of these has evidence.
//
// Bodies shared by two addresses each (the instruction words are byte for byte the same; only the internal branch targets
// differ): 0x089006DC / 0x08900A38 (one constructor), 0x08BE207C / 0x08BE241C and 0x08BD9C44 / 0x08BDA004 (two destructor chains,
// the second one with an extra trailing call on 0x08BDA004).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_27 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before its `jal` (a0-a3, t0-t3 in order); v0 holds
// the result afterwards, as after the `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, address, arguments);
    return ctx.gpr[2];
}

inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
// Four bytes in the order the asm assembles them: the lowest byte first.
inline u32 bytes4(u32 b0, u32 b1, u32 b2, u32 b3) {
    return (b0 & 0xFFu) | ((b1 & 0xFFu) << 8) | ((b2 & 0xFFu) << 16) | ((b3 & 0xFFu) << 24);
}

// A guest call with integer and float argument registers set like the original's `jal`; v0 / v1 / f0 stay in the context.
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The `ins` instruction: `size` bits of `value` replace the same bits of `destination` from `position` up.
inline u32 insert_bits(u32 destination, u32 value, unsigned position, unsigned size) {
    const u32 mask = ((1u << size) - 1u) << position;
    return (destination & ~mask) | ((value << position) & mask);
}
inline s32 signed_byte_u(u32 value) { return static_cast<std::int8_t>(static_cast<std::uint8_t>(value)); }

// ---- functions ----

// 0x0895FA54 (a0 = a parameter block, a1 = a selector): fills the block from the tables at 0x08CDE508 — a byte count at +0x23
// selects the rows of the speed tables at 0x08CDE52C / 0x08CDE534, then two 2-row tables at 0x08CDE514 / +0x51A / +0x520 and the
// byte fields at +0x11 .. +0x22 give scale factors, and a 24-row pass over the byte tables at 0x08CDE540 / 0x08CDE558 fills
// 0x74, 0xD4, 0x148, 0x1A8, 0x208 (stride 4) and 0x268, 0x280 (stride 1). A selector of 3 maps row 5 of that pass to row 15, and
// a selector of -1 becomes 0. v0 = the byte at row + 0xC8 of the pass's last row (23).

// 0x089006DC / 0x08900A38 (a0 = the object): constructor. Three vtable words go to +0, +4, +8, then the member constructors run
// one after another on this + offset (each field was defaulted just before its own constructor where the asm stores first), and
// the body ends in a tail call to 0x089000CC with a0 = the object and v0 = the last call's result.

// 0x08B29930 (a0 = a 16-word output block, a1 = a name, a2 = its length): memsets a 0x20-byte scratch area, copies at most 32
// bytes of the name there and lays the digest out in the 16 words: the words of the name (little-endian, 4 bytes each) go twice
// (a short name fills 4 words at +4..+0x10 and +0x2C..+0x38, 17 bytes or more fills 5 at +4..+0x14 and +0x2C..+0x30 plus
// +0x34/+0x38), and the global words at 0x08C95408 / +0x0C / +0x10 / +0x14 (short) or +0x18 / +0x1C / +0x20 / +0x24 (long) are
// XORed with the global salt at 0x08C95428 into +0, +0x14, +0x28, +0x3C. Whatever the path leaves uncovered at +0x18..+0x24 is
// zeroed. v0 = the salt (short name) or the last packed word (long name).
void func_08B29930(Runtime &rt, AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 length = ctx.gpr[6] < 0x21u ? ctx.gpr[6] : 0x20u;
    GuestFrame frame(ctx, 0x70u);
    const u32 scratch = frame.at(0u);
    call_set(rt, ctx, 0x08B36018u, {scratch, 0u, 0x20u});
    call_set(rt, ctx, 0x08B79A88u, {scratch, name, length});
    const u32 salt = word(rt, 0x08C95428u);
    const u32 finger = 0x08C95418u;

    if (length < 0x11u) {
        const u32 packed0 = bytes4(byte_u(rt, scratch), byte_u(rt, scratch + 1u), byte_u(rt, scratch + 2u), byte_u(rt, scratch + 3u));
        const u32 packed1 = bytes4(byte_u(rt, scratch + 4u), byte_u(rt, scratch + 5u), byte_u(rt, scratch + 6u), byte_u(rt, scratch + 7u));
        const u32 packed2 = bytes4(byte_u(rt, scratch + 8u), byte_u(rt, scratch + 9u), byte_u(rt, scratch + 0xAu), byte_u(rt, scratch + 0xBu));
        const u32 packed3 = bytes4(byte_u(rt, scratch + 0xCu), byte_u(rt, scratch + 0xDu), byte_u(rt, scratch + 0xEu), byte_u(rt, scratch + 0xFu));
        const u32 constants = 0x08C95408u;
        put(rt, output, salt ^ word(rt, constants));
        put(rt, output + 0x14u, salt ^ word(rt, constants + 4u));
        put(rt, output + 0x28u, salt ^ word(rt, constants + 8u));
        put(rt, output + 0x2Cu, packed0);
        put(rt, output + 0x30u, packed1);
        put(rt, output + 0x34u, packed2);
        put(rt, output + 0x38u, packed3);
        put(rt, output + 0x3Cu, salt ^ word(rt, constants + 0xCu));
        put(rt, output + 4u, packed0);
        put(rt, output + 8u, packed1);
        put(rt, output + 0xCu, packed2);
        put(rt, output + 0x10u, packed3);
        ctx.gpr[2] = salt;
    } else {
        const u32 packed0 = bytes4(byte_u(rt, scratch), byte_u(rt, scratch + 1u), byte_u(rt, scratch + 2u), byte_u(rt, scratch + 3u));
        const u32 packed1 = bytes4(byte_u(rt, scratch + 4u), byte_u(rt, scratch + 5u), byte_u(rt, scratch + 6u), byte_u(rt, scratch + 7u));
        const u32 packed2 = bytes4(byte_u(rt, scratch + 8u), byte_u(rt, scratch + 9u), byte_u(rt, scratch + 0xAu), byte_u(rt, scratch + 0xBu));
        const u32 packed3 = bytes4(byte_u(rt, scratch + 0xCu), byte_u(rt, scratch + 0xDu), byte_u(rt, scratch + 0xEu), byte_u(rt, scratch + 0xFu));
        const u32 packed4 = bytes4(byte_u(rt, scratch + 0x10u), byte_u(rt, scratch + 0x11u), byte_u(rt, scratch + 0x12u), byte_u(rt, scratch + 0x13u));
        const u32 packed5 = bytes4(byte_u(rt, scratch + 0x14u), byte_u(rt, scratch + 0x15u), byte_u(rt, scratch + 0x16u), byte_u(rt, scratch + 0x17u));
        const u32 packed6 = bytes4(byte_u(rt, scratch + 0x18u), byte_u(rt, scratch + 0x19u), byte_u(rt, scratch + 0x1Au), byte_u(rt, scratch + 0x1Bu));
        const u32 packed7 = bytes4(byte_u(rt, scratch + 0x1Cu), byte_u(rt, scratch + 0x1Du), byte_u(rt, scratch + 0x1Eu), byte_u(rt, scratch + 0x1Fu));
        put(rt, output, salt ^ word(rt, finger));
        put(rt, output + 4u, packed0);
        put(rt, output + 8u, packed1);
        put(rt, output + 0xCu, packed2);
        put(rt, output + 0x10u, packed3);
        put(rt, output + 0x14u, salt ^ word(rt, finger + 4u));
        put(rt, output + 0x28u, salt ^ word(rt, finger + 8u));
        put(rt, output + 0x2Cu, packed4);
        put(rt, output + 0x30u, packed5);
        put(rt, output + 0x34u, packed6);
        put(rt, output + 0x38u, packed7);
        put(rt, output + 0x3Cu, salt ^ word(rt, finger + 0xCu));
        ctx.gpr[2] = packed7;
    }
    put(rt, output + 0x24u, 0u);
    put(rt, output + 0x18u, 0u);
    put(rt, output + 0x1Cu, 0u);
    put(rt, output + 0x20u, 0u);
}

// The ll/sc decrement of a shared holder's use count: stores old - 1, leaves v1 = the address, a0 = the old value and v0 = 1 (the
// successful sc), and answers whether the count reached zero.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old == 1u;
}
// `lw v1, 0(holder); lw v0, offset(v1); jalr v0` with a0 = the holder; v0 / v1 / f0 afterwards are the callee's.
u32 call_vtable(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 offset) {
    const u32 table = word(rt, holder);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    const GuestResult result = call_guest(rt, ctx, function, {holder}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// One member of a chain of shared holders: release the count at +4 and, when that was the last reference, run the object's
// vtable +8 virtual and then the second count at +8 and the +0xC virtual.
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 holder) {
    if (holder == 0u) return;
    if (!count_down(rt, ctx, holder + 4u)) return;
    call_vtable(rt, ctx, holder, 8u);
    if (!count_down(rt, ctx, holder + 8u)) return;
    call_vtable(rt, ctx, holder, 0xCu);
}
// A string-ish block: its header sits 0xC bytes before the data (the shared empty header 0x08D75298 is never freed), and the
// reference count 4 bytes before the data decides whether 0x08B93554 frees it.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 scratch) {
    const u32 header = data - 0xCu;
    if (header == 0x08D75298u) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B93554u, {header, scratch});
}

// 0x08BE207C / 0x08BE241C (a0 = the object): destructor of a class holding a chain of shared holders: 0x84, 0x7C, 0x74, the
// string block at 0x50, 0x4C, 0x44, 0x3C and 0x2C are released, the vtable moves from 0x08CC6868 to 0x08CC6888 halfway and to
// 0x08CC3000 at the end, and the last holder at 0x24 only releases its +8 count through the +0xC virtual. v0 = 0x08CC3000.
void destroy_chain_08BE(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC6868u);
    release_holder(rt, ctx, word(rt, self + 0x84u));
    release_holder(rt, ctx, word(rt, self + 0x7Cu));
    release_holder(rt, ctx, word(rt, self + 0x74u));
    release_string(rt, ctx, word(rt, self + 0x50u), frame.at(0u));
    release_holder(rt, ctx, word(rt, self + 0x4Cu));
    release_holder(rt, ctx, word(rt, self + 0x44u));
    release_holder(rt, ctx, word(rt, self + 0x3Cu));
    put(rt, self, 0x08CC6888u);
    release_holder(rt, ctx, word(rt, self + 0x2Cu));
    const u32 last = word(rt, self + 0x24u);
    if (last != 0u && count_down(rt, ctx, last + 8u)) call_vtable(rt, ctx, last, 0xCu);
    put(rt, self, 0x08CC3000u);
    ctx.gpr[2] = 0x08CC3000u;
}
void func_08BE207C(Runtime &rt, AllegrexContext &ctx) { destroy_chain_08BE(rt, ctx); }

// 0x08BD9C44 / 0x08BDA004 (a0 = the object): the same chain shape with 0x168, 0x160, 0x154, 0x14C, 0x144, the string block at
// 0x54, 0x50, 0x48 and 0x3C, then a call of 0x08BD6D84(this) and — for 0x08BDA004 only — 0x08B64654(this), whose result is v0.
void destroy_chain_08BD(Runtime &rt, AllegrexContext &ctx, bool trailing) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    release_holder(rt, ctx, word(rt, self + 0x168u));
    release_holder(rt, ctx, word(rt, self + 0x160u));
    release_holder(rt, ctx, word(rt, self + 0x154u));
    release_holder(rt, ctx, word(rt, self + 0x14Cu));
    release_holder(rt, ctx, word(rt, self + 0x144u));
    release_string(rt, ctx, word(rt, self + 0x54u), frame.at(0u));
    release_holder(rt, ctx, word(rt, self + 0x50u));
    release_holder(rt, ctx, word(rt, self + 0x48u));
    release_holder(rt, ctx, word(rt, self + 0x3Cu));
    call_set(rt, ctx, 0x08BD6D84u, {self});
    if (trailing) call_set(rt, ctx, 0x08B64654u, {self});
}
void func_08BD9C44(Runtime &rt, AllegrexContext &ctx) { destroy_chain_08BD(rt, ctx, false); }
void func_08BDA004(Runtime &rt, AllegrexContext &ctx) { destroy_chain_08BD(rt, ctx, true); }

// 0x08BACB64 (a0 = an object holding a sub-object at +0xC): destructor of the sub-object — clears its owner's back-pointer at
// +0x48 when it still names the holder at +0x20/+8, installs the member vtables 0x08CC1730 at 0xB74, 0xB68, 0xB58, 0xB4C,
// 0xB40, 0xB30, 0xB20 (each destroyed by 0x08AF20E8), 0x08CC22B0 at 0xB14 and 0x08CC2288 at 0xB0C (0x08AF86DC), runs
// 0x08BAC238 over the pair at 0x8E0/0x8E4 and frees 0x8E0, destroys the blocks at 0x60, 0x48, 0x38, 0x28 with 0x08909D98,
// releases the holders at 0x5C, 0x24, 0x1C, 0x14, 0x0C and 0x04, and ends in 0x08B64654(sub). v0 = that last result; a null
// sub-object returns without touching v0.
void func_08BACB64(Runtime &rt, AllegrexContext &ctx) {
    const u32 sub = word(rt, ctx.gpr[4] + 0xCu);
    if (sub == 0u) return;
    GuestFrame frame(ctx, 0x20u);
    const u32 owner = word(rt, sub + 0x68u);
    if (owner != 0u) {
        const u32 holder = word(rt, sub + 0x20u);
        if (holder != 0u) {
            const u32 other = word(rt, holder + 8u);
            if (other != 0u && word(rt, owner + 0x48u) == word(rt, other + 0x48u)) put(rt, owner + 0x48u, 0u);
        }
    }
    for (const u32 offset : {0xB74u, 0xB68u, 0xB58u, 0xB4Cu, 0xB40u, 0xB30u, 0xB20u}) {
        put(rt, sub + offset, 0x08CC1730u);
        call_set(rt, ctx, 0x08AF20E8u, {sub + offset});
    }
    put(rt, sub + 0xB14u, 0x08CC22B0u);
    call_set(rt, ctx, 0x08AF86DCu, {sub + 0xB14u});
    put(rt, sub + 0xB0Cu, 0x08CC2288u);
    call_set(rt, ctx, 0x08AF86DCu, {sub + 0xB0Cu});
    call_set(rt, ctx, 0x08BAC238u, {word(rt, sub + 0x8E0u), word(rt, sub + 0x8E4u), frame.at(0u)});
    const u32 table = word(rt, sub + 0x8E0u);
    if (table != 0u) call_set(rt, ctx, 0x08B35254u, {table});
    call_set(rt, ctx, 0x08909D98u, {sub + 0x60u});
    release_holder(rt, ctx, word(rt, sub + 0x5Cu));
    for (u32 block = sub + 0x48u;; block -= 0x10u) {
        call_set(rt, ctx, 0x08909D98u, {block});
        if (block == sub + 0x38u) break;
    }
    call_set(rt, ctx, 0x08909D98u, {sub + 0x28u});
    release_holder(rt, ctx, word(rt, sub + 0x24u));
    release_holder(rt, ctx, word(rt, sub + 0x1Cu));
    release_holder(rt, ctx, word(rt, sub + 0x14u));
    release_holder(rt, ctx, word(rt, sub + 0xCu));
    release_holder(rt, ctx, word(rt, sub + 4u));
    call_set(rt, ctx, 0x08B64654u, {sub});
}

// 0x088AA774 (a0 = object, a1 = a position vector, f12-f15 = four scale values): rebuilds the object's render block at +0x5C
// from the data record at word[word[a0+0x4C]+0x14]: it clears the 0x280-byte block, writes the (at most 127) 4-word points of
// the record relative to the position (w = 1), packs the record's three signed row counts into the bit field word at +0x1FC
// (4 bits each for the first two at 0, 4, 8 and 12, 16; 5 bits for the third at 20 and 25), fills the second group of rows
// (0xE0, stride 0x10, the fourth float plus 0x08BAE7C8's answer) and the byte pairs at 0x1C0, then the two 4-float vectors at
// 0x1A0 / 0x1B0 (relative to the position, w = 1) for 0x08BAE824, and finally the object's own defaults (0x80..0x88 from the
// position, 0x8C/0xA0/0xB4/0xBC = 1/scale, 0xC0/0xC4 = 0.5, 0xC8 = 0.05, halfword 0xDA = 1, byte 0x2DC = 1). v0 = 1, or 0 when
// the object has no record.

} // namespace
} // namespace new_batch_main_27
} // namespace gt::game


#define MAIN27(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_27::name, gt::ReturnKind::kind)
MAIN27(0x08B29930u, func_08B29930, Word);
MAIN27(0x08BACB64u, func_08BACB64, Word);
MAIN27(0x08BD9C44u, func_08BD9C44, Word);
MAIN27(0x08BDA004u, func_08BDA004, Word);
MAIN27(0x08BE207Cu, func_08BE207C, Word);
