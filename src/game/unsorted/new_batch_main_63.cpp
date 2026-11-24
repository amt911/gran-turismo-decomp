// New batch, main session 63 (DRAFT, unverified): long functions (640..720 bytes) the recompiler emitted, written by hand as
// structured C from `tools.disasm`. v0 is left as the original leaves it on every path (call helpers set it the way the
// original's `jal`/`jalr` has it). No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_63 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// `lw v0, (object); lw v1, slot(v0); jalr v1`: at the jalr v0 holds the vtable and v1 the target.
GuestResult call_virtual_v1(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw a1, (object); lw v0, slot(a1); jalr v0`: a1 holds the vtable and v0 the target; a0 = object.
GuestResult call_virtual_a1(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[5] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, {object, table}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned position, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << position;
    return (target & ~mask) | ((value << position) & mask);
}

// 0x088CD9C8 (a0 = out block, a1 = source object, a2 = count): reads the object through its virtual getters into the block:
// flag bits of the word at +0 (slots 0x0C..0x14, 0x3C..0x4C), bytes/half/words at +1..+0x18 (slots 0x08, 0x18..0x30), a float at +0x9C
// (slot 0x50), flag bits of the word at +0xA0 (0x54, 0x5C, 0x60) and a byte at +0xA1 (0x58); then `count` pairs of words at +0x1C / +0x5C.

// 0x08924B30 (a0 = this, a2, a3): a constructor. Base init 0x08925FE4(this), the vtable 0x08CC5FC0 at +0, ten sub-objects initialised
// by 0x089263EC (offsets 0x10, 0x3C, 0x68, 0x94, 0xC0, 0xEC, 0x118, 0x144, 0x174, 0x1A0) each followed by its vtable word, six
// records of 0x2C at +0x1E0 and +0x264 by 0x08926398, a2 / a3 at +8 / +0xC, 0x08926488 on four of the members, 0x08926514 on
// +0x118 / +0x174 with a table chosen by a3 == 2, then a virtual call (slot 8) on the six records; ends with +0x1D0..+0x1DC zeroed, v0 = this + 0x1D0.
void func_08924B30(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg_c = ctx.gpr[6];
    const u32 arg_d = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    call(rt, ctx, 0x08925FE4u, {self});
    put(rt, self, 0x08CC5FC0u);
    constexpr u32 kPlain = 0x08CC61C0u;
    constexpr u32 kAlt = 0x08CC6150u;
    constexpr u32 kBase = 0x08CC6198u;
    call(rt, ctx, 0x089263ECu, {self + 0x10u});
    put(rt, self + 0x10u, kPlain);
    call(rt, ctx, 0x089263ECu, {self + 0x3Cu});
    put(rt, self + 0x3Cu, kPlain);
    call(rt, ctx, 0x089263ECu, {self + 0x68u});
    put(rt, self + 0x68u, kPlain);
    call(rt, ctx, 0x089263ECu, {self + 0x94u});
    put(rt, self + 0x94u, kPlain);
    call(rt, ctx, 0x089263ECu, {self + 0xC0u});
    put(rt, self + 0xC0u, kPlain);
    call(rt, ctx, 0x089263ECu, {self + 0xECu});
    put(rt, self + 0xECu, kPlain);
    call(rt, ctx, 0x089263ECu, {self + 0x118u});
    put(rt, self + 0x118u, kBase);
    call(rt, ctx, 0x089263ECu, {self + 0x144u});
    put(rt, self + 0x144u, kAlt);
    call(rt, ctx, 0x089263ECu, {self + 0x174u});
    put(rt, self + 0x174u, kBase);
    call(rt, ctx, 0x089263ECu, {self + 0x1A0u});
    put(rt, self + 0x1A0u, kAlt);
    for (u32 record = 0; record < 3u; ++record) call(rt, ctx, 0x08926398u, {self + 0x1E0u + 0x2Cu * record});
    for (u32 record = 0; record < 3u; ++record) call(rt, ctx, 0x08926398u, {self + 0x264u + 0x2Cu * record});
    put(rt, self + 8u, arg_c);
    put(rt, self + 0xCu, arg_d);
    constexpr u32 kText = 0x08CDDC60u;
    call(rt, ctx, 0x08926488u, {self + 0x10u, kText, 1u});
    call(rt, ctx, 0x08926488u, {self + 0x3Cu, kText, 1u});
    call(rt, ctx, 0x08926488u, {self + 0xC0u, kText, 1u});
    call(rt, ctx, 0x08926488u, {self + 0xECu, kText, 1u});
    const u32 text_b = arg_d == 2u ? 0x08CDDC18u : 0x08CDDC30u;
    call(rt, ctx, 0x08926514u, {self + 0x118u, text_b, 1u});
    call(rt, ctx, 0x08926514u, {self + 0x174u, text_b, 1u});
    for (u32 record = 0; record < 3u; ++record) {
        const u32 first = self + 0x1E0u + 0x2Cu * record;
        call_virtual_v1(rt, ctx, first, 8u, {first, 0x08CDDC00u, 1u});
        const u32 second = self + 0x264u + 0x2Cu * record;
        call_virtual_v1(rt, ctx, second, 8u, {second, 0x08CDDBE8u, 1u});
    }
    const u32 block = self + 0x1D0u;
    put(rt, block, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    ctx.gpr[2] = block;
}

// 0x08AEF484 (a0 = object, a1 = case 0..7, a2, a3): updates the 0x6C-byte entry (index = word at a0 + 0x1C) of the table whose
// pointer is at 0x08D41F80, setting bits in the entry's flag word at +4; the cases are a jump table at 0x08C92554.
void func_08AEF484(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 which = ctx.gpr[5];
    const u32 arg = ctx.gpr[6];
    const u32 arg_d = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    if (which >= 8u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 index = word(rt, object + 0x1Cu);
    constexpr u32 kTable = 0x08D41F80u;
    auto entry = [&]() { return word(rt, kTable) + index * 0x6Cu; };
    // The tail of cases 0 and 0x58C: byte +5 = 1, byte +4 = (byte | 2) with bit 4 cleared.
    auto finish_object = [&](bool set_two) {
        u32 value = byte_at(rt, object + 4u);
        put_byte(rt, object + 5u, 1u);
        if (set_two) {
            value |= 2u;
            value = insert_bits(value, 0u, 4u, 1u);
        } else {
            value = insert_bits(value, 0u, 1u, 1u);
        }
        put_byte(rt, object + 4u, value);
        ctx.gpr[2] = value;
    };
    ctx.gpr[2] = 0x08C92554u + which * 4u;
    switch (which) {
    case 0: {
        if (word(rt, arg + 0x18u) == 2u) {
            u32 at = entry();
            const u32 member = word(rt, arg + 0x24u);
            u32 flags = word(rt, at + 4u);
            put(rt, at + 0x14u, member);
            put(rt, at + 4u, flags | 0x10u);
            at = entry();
            flags = word(rt, at + 4u);
            put(rt, at + 8u, 1u);
            put(rt, at + 4u, flags | 2u);
            finish_object(true);
            return;
        }
        u32 at = entry();
        const u32 holder = word(rt, arg + 0x48u);
        const u32 offset = word(rt, arg + 0x2Cu);
        u32 flags = word(rt, at + 4u);
        const u32 sum = word(rt, holder + 8u) + offset;
        put(rt, at + 4u, flags | 4u);
        put(rt, at + 0xCu, sum);
        at = entry();
        flags = word(rt, at + 4u);
        put(rt, at + 0x10u, word(rt, arg + 0x44u));
        put(rt, at + 4u, flags | 8u);
        at = entry();
        flags = word(rt, at + 4u);
        put(rt, at + 0x30u, 0u);
        put(rt, at + 4u, flags | 0x800u);
        at = entry();
        flags = word(rt, at + 4u);
        put(rt, at + 0x34u, 0u);
        put(rt, at + 4u, flags | 0x1000u);
        at = entry();
        flags = word(rt, at + 4u);
        put(rt, at + 8u, 0u);
        put(rt, at + 4u, flags | 2u);
        finish_object(true);
        return;
    }
    case 1: return;
    case 2: {
        const u32 at = entry();
        const u32 flags = word(rt, at + 4u);
        put(rt, at + 8u, 2u);
        put(rt, at + 4u, flags | 2u);
        ctx.gpr[2] = 2u;
        return;
    }
    case 3: {
        const u32 at = entry();
        const u32 flags = word(rt, at + 4u);
        put(rt, at + 8u, 3u);
        put(rt, at + 4u, flags | 2u);
        finish_object(false);
        return;
    }
    case 4: call(rt, ctx, 0x08AEF1D0u, {object, arg}); return;
    case 5: {
        put_half(rt, 0x08D41AD0u + index * 2u + 0x3E4u, arg);
        const u32 at = entry();
        const u32 flags = word(rt, at + 4u);
        put(rt, at + 0x2Cu, arg);
        put(rt, at + 4u, flags | 0x400u);
        ctx.gpr[2] = flags | 0x400u;
        return;
    }
    case 6: {
        const u32 at = entry();
        const u32 flags = word(rt, at + 4u);
        put(rt, at + 0x18u, arg);
        put(rt, at + 4u, flags | 0x20u);
        put_byte(rt, object + 5u, 1u);
        ctx.gpr[2] = flags | 0x20u;
        return;
    }
    default: {
        u32 at = entry();
        u32 flags = word(rt, at + 4u);
        put(rt, at + 0x30u, arg);
        put(rt, at + 4u, flags | 0x800u);
        at = entry();
        flags = word(rt, at + 4u);
        put(rt, at + 0x34u, arg_d);
        put(rt, at + 4u, flags | 0x1000u);
        ctx.gpr[2] = flags | 0x1000u;
        return;
    }
    }
}

} // namespace
} // namespace new_batch_main_63
} // namespace gt::game

#define NEW63(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_63::name, gt::ReturnKind::kind)
NEW63(0x08924B30u, func_08924B30, Word);
NEW63(0x08AEF484u, func_08AEF484, Word);
