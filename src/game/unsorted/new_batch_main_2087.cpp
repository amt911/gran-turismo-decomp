// Main-session batch 2087: eleven bodies that earlier drafts mismatched or skipped. Hand-written structured C from
// `tools.disasm`; no names (none has evidence), no byte-for-byte claim. Unverified drafts: the gate decides.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2087 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

float f_bits(u32 bits) { return std::bit_cast<float>(bits); }

// A guest call: the listed arguments go to a0-a3/t0-t3 (unlisted registers keep their current value), floats to
// f12..., and afterwards v0/v1/f0 are the callee's, as after the `jal`.
GuestResult call_g(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                   std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A `lw v0, slot(vtable); jalr v0` call (the target is v0 at entry, its own address).
u32 call_v(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_indirect(rt, ctx, target, arguments);
}
s32 sbyte(Runtime &rt, u32 address) { return static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))); }

// ---------------------------------------------------------------------------------------------------------------------
// 0x088F574C: clears four 0x28-byte name records at 0x08CDD6E4, then initialises four 0x84-byte records at 0x08CE9C88:
// each is zeroed, two bytes of the table at 0x08CDD700 become scaled floats, and the name is attached.
void func_088F574C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    (void)frame;
    const float scale = f_bits(0x451F6000u);
    u32 name1 = 0x08CDD6E4u;
    for (u32 index = 0u; index != 4u; ++index) {
        call_g(rt, ctx, 0x0893A814u, {name1, 0u});
        name1 += 0x28u;
    }
    u32 name = 0x08CDD6E4u;
    u32 record = 0x08CE9C88u;
    u32 table = 0x08CDD700u;
    for (u32 step = 0u; step != 4u; ++step) {
        call_g(rt, ctx, 0x08B36018u, {record, 0u, 0x84u});
        float f12 = f_bits(0x3CC6F2D5u);
        float f13 = static_cast<float>(rt.memory().load8(table)) / scale;
        call_g(rt, ctx, 0x0893A854u, {record, name, 0u}, {f12, f13});
        f12 = f_bits(0x3C5BE3F9u);
        f13 = static_cast<float>(rt.memory().load8(table + 1u)) / scale;
        const float f14 = static_cast<float>(rt.memory().load8(table + 2u)) / scale;
        call_g(rt, ctx, 0x0893A8D8u, {record, name, 0u}, {f12, f13, f14});
        call_g(rt, ctx, 0x0893A9CCu, {record, name});
        record += 0x84u;
        name += 0x28u;
        table += 0x28u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08AA6A4C: runs a small sentinel-list walk (0x08AA5E90) then rebuilds the container at +0x1C when the count at
// +0x18 is non-zero: two list passes (0x08B855E0/0x08B5F100, then 0x08AA6178).

// ---------------------------------------------------------------------------------------------------------------------
// 0x08ACEBC4: constructs the object in a0 by initialising its three sub-blocks and every contained helper.

// ---------------------------------------------------------------------------------------------------------------------
// 0x08AB8FD8 (a0 = row-pointer array, a1 = rows, a2 = columns, a3 = stride in words, t0/t1/t2 = float outs): per row
// folds the min, max and sum of the floats at the row's stride, then writes |max|/|min|, maxsum/columns and
// sum/(rows*columns) through the square-root helper 0x08B49D7C.
void func_08AB8FD8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    (void)frame;
    const u32 rows = ctx.gpr[5];
    const u32 columns = ctx.gpr[6];
    const u32 stride = ctx.gpr[7] << 2;
    const u32 array = ctx.gpr[4];
    float f20 = 0.0f;
    float f3 = 0.0f;
    float f4 = 0.0f;
    float f5 = 0.0f;
    if (static_cast<s32>(rows) > 0) {
        for (u32 row = 0u; row < rows; ++row) {
            const u32 base = word(rt, array + row * 4u);
            float f2 = 0.0f;
            if (static_cast<s32>(columns) > 0) {
                for (u32 column = 0u; column < columns; ++column) {
                    const float f1 = float_at(rt, base + column * stride);
                    f2 = f2 + psp_mul(f1, f1);
                    if (!(f3 < f1)) f3 = f1;
                    if (f4 < f1) f4 = f1;
                }
            }
            f20 = f20 + f2;
            if (f5 < f2) f5 = f2;
        }
    }
    const float absmax = std::fabs(f4);
    const float absmin = std::fabs(f3);
    const float spread = (absmax < absmin) ? absmin : absmax;
    put_float(rt, ctx.gpr[8], spread);
    const float r1 = call_g(rt, ctx, 0x08B49D7Cu, {}, {f5 / static_cast<float>(static_cast<s32>(columns))}).f0;
    put_float(rt, ctx.gpr[9], r1);
    if (ctx.gpr[10] != 0u) {
        const s32 total = static_cast<s32>(rows) * static_cast<s32>(columns);
        const float r2 = call_g(rt, ctx, 0x08B49D7Cu, {}, {f20 / static_cast<float>(total)}).f0;
        put_float(rt, ctx.gpr[10], r2);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B49014 (a0 = object, f12/f13 = floats): 0x08B48FA0(object, f13) answers a float; the object's mode at +0x10
// (1/2 -> one scale, 3/4 -> another, else a third) then picks a small integer through clamped float ranges.
void func_08B49014(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    (void)frame;
    const u32 self = ctx.gpr[4];
    const float f20 = ctx.fpr[12];
    const float f1 = call_g(rt, ctx, 0x08B48FA0u, {self}, {ctx.fpr[13]}).f0;

    auto limit_c = [&](float f12) -> u32 {
        if (f12 < f_bits(0x3C23D70Au)) return 0u;
        if (f1 < f_bits(0x358637BDu)) return 0x3E7u;
        const float value = (f12 * f_bits(0x3C23D70Au)) / f1;
        const s32 truncated = static_cast<s32>(value);
        if (f_bits(0x4479C000u) < value) return 0x3E7u;
        return static_cast<u32>(truncated);
    };
    auto limit_b = [&]() -> u32 {
        if (f20 < f_bits(0x3A83126Fu)) return 0x3E7u;
        const float value = psp_mul(f1, f_bits(0x49742400u)) / f20;
        const s32 truncated = static_cast<s32>(value);
        if (f_bits(0x4479C000u) <= value) return 0x3E7u;
        return static_cast<u32>(truncated);
    };

    const s32 mode = static_cast<s32>(word(rt, self + 0x10u));
    if (mode == 2 || mode == 1) {
        ctx.gpr[2] = limit_c(psp_mul(f20, f_bits(0x3F1F136Au)));
    } else if (mode < 3) {
        ctx.gpr[2] = limit_c(f20);
    } else if (mode < 5) {
        ctx.gpr[2] = limit_b();
    } else {
        ctx.gpr[2] = limit_c(f20);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08939484 (a0 = self, a1..a3): when self+0x1C is empty, builds three stack blocks and hands them through
// 0x08939044; stores its result at +0x1C, clears +0x20 and returns 1 (0 when already initialised).
void func_08939484(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xE0u);
    const u32 self = ctx.gpr[4];
    u32 v1 = 0u;
    if (word(rt, self + 0x1Cu) == 0u) {
        call_g(rt, ctx, 0x08B0CE58u, {frame.at(0x40u)});
        call_g(rt, ctx, 0x08B0EC60u, {frame.at(0x80u), ctx.gpr[5]});
        for (u32 off = 0u; off < 0x30u; off += 4u) put(rt, frame.at(0x00u) + off, word(rt, frame.at(0x80u) + off));
        for (u32 off = 0u; off < 0xCu; off += 4u) put(rt, frame.at(0x30u) + off, word(rt, frame.at(0xB0u) + off));
        for (u32 off = 0u; off < 0x30u; off += 4u) put(rt, frame.at(0x40u) + off, word(rt, frame.at(0x00u) + off));
        for (u32 off = 0u; off < 0xCu; off += 4u)
            put(rt, frame.at(0x40u) + 0x30u + off, word(rt, frame.at(0x30u) + off));
        call_g(rt, ctx, 0x08939044u, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
        put(rt, self + 0x1Cu, ctx.gpr[2]);
        rt.memory().store8(self + 0x20u, 0);
        v1 = 1u;
    }
    ctx.gpr[2] = v1;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B19948: tests the word at a0 against ten type probes; the first that matches with a non-zero word at +4 wins
// (returns 1), the last probe also answers 0 when it matches with +4 zero.
void func_08B19948(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)frame;
    const u32 self = ctx.gpr[4];
    auto matches = [&](u32 probe) -> bool {
        if (word(rt, self) != call_g(rt, ctx, probe, {self}).v0) return false;
        return word(rt, self + 4u) != 0u;
    };
    if (matches(0x08BC473Cu) || matches(0x08BC48B4u) || matches(0x08BC45C4u) || matches(0x08BC444Cu) ||
        matches(0x08BC42D4u) || matches(0x08B9E8D4u) || matches(0x08BC415Cu) || matches(0x08BC3FE4u) ||
        matches(0x08BC4A2Cu)) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 s0 = word(rt, self);
    const u32 last = call_g(rt, ctx, 0x08B9CEBCu, {self}).v0;
    ctx.gpr[2] = (s0 == last && word(rt, self + 4u) != 0u) ? 1u : 0u;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08A768F4 (a0 = object, a1 = string): walks the string and, per character, calls a handler with a class string
// (control characters 7..13 pick one of seven names, printable ones another, the rest a third).
void func_08A768F4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)frame;
    const u32 self = ctx.gpr[4];
    u32 str = ctx.gpr[5];
    static const u32 kControlNames[7] = {0x08CB0E10u, 0x08CB0E14u, 0x08CB0E18u, 0x08CB0E1Cu,
                                         0x08CB0E20u, 0x08CB0E24u, 0x08CB0E28u};
    for (;;) {
        const s32 c = sbyte(rt, str);
        if (c == 0) break;
        const u32 control = static_cast<u32>(c - 7);
        if (control < 7u) {
            call_g(rt, ctx, 0x08A7677Cu, {self, kControlNames[control]});
        } else if (static_cast<u32>(c - 0x20) < 0x5Fu) {
            call_g(rt, ctx, 0x08A767ECu, {self, 0x08CB0E2Cu});
        } else {
            call_g(rt, ctx, 0x08A767ECu, {self, 0x08CB0E30u});
        }
        str += 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B970A4: the run-once initialiser for four guard-guarded global blocks; each block is registered
// (0x08B6441C/0x08B64494) and wired together, and v0 answers the last block's address.
void func_08B970A4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    (void)frame;
    constexpr u32 kA = 0x08D750A8u;
    constexpr u32 kF0 = 0x08D750F0u;
    constexpr u32 k008 = 0x08D75008u;
    constexpr u32 k0A0 = 0x08D750A0u;
    constexpr u32 k2A8 = 0x08D752A8u;
    constexpr u32 k3A0 = 0x08D753A0u;
    constexpr u32 k3B0 = 0x08D753B0u;
    constexpr u32 k3BC = 0x08D753BCu;

    if (sbyte(rt, kA) == 0 && call_g(rt, ctx, 0x08B6441Cu, {kA}).v0 != 0u) {
        if (sbyte(rt, kF0) == 0 && call_g(rt, ctx, 0x08B6441Cu, {kF0}).v0 != 0u) {
            put(rt, k2A8 + 8u, 0x08CBF670u);
            put(rt, k2A8 + 4u, 0x08D752B4u);
            put(rt, k2A8, 0u);
            call_g(rt, ctx, 0x08B64494u, {kF0});
        }
        put(rt, k3BC, k2A8);
        if (sbyte(rt, k008) == 0 && call_g(rt, ctx, 0x08B6441Cu, {k008}).v0 != 0u) {
            put(rt, k3A0 + 8u, 0u);
            put(rt, k3A0 + 4u, 0u);
            put(rt, k3A0, 0u);
            call_g(rt, ctx, 0x08B64494u, {k008});
        }
        put(rt, k3BC + 4u, k3A0);
        call_g(rt, ctx, 0x08B64494u, {kA});
    }
    if (sbyte(rt, k0A0) == 0 && call_g(rt, ctx, 0x08B6441Cu, {k0A0}).v0 != 0u) {
        put(rt, k3B0 + 8u, 0x08CBFA88u);
        put(rt, k3B0 + 4u, k3BC);
        put(rt, k3B0, 0u);
        call_g(rt, ctx, 0x08B64494u, {k0A0});
    }
    ctx.gpr[2] = 0x08D753B0u;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0885C704: constructs the object in a0 by allocating its top block, four 0x188-byte slots and the four handlers,
// then tail-calls 0x0885C3C8(self, word(self)).
void func_0885C704(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self + 4u, 0u);
    call_g(rt, ctx, 0x08B28DCCu, {self + 8u});
    const u32 block = call_g(rt, ctx, 0x08B62344u, {0x138u}).v0;
    put(rt, self, block);
    call_g(rt, ctx, 0x08B36018u, {block, 0u, 0x138u});
    const u32 table = call_g(rt, ctx, 0x08B6461Cu, {0x628u}).v0;
    put(rt, table + 4u, 4u);
    const u32 slots = table + 8u;
    for (u32 i = 0u; i != 4u; ++i) {
        call_g(rt, ctx, 0x0885D720u, {slots + i * 0x188u});
    }
    put(rt, block, slots);
    put(rt, word(rt, self) + 0x78u, 0u);
    const u32 s0a = word(rt, self);
    const u32 r97 = call_g(rt, ctx, 0x08B0979Cu, {}).v0;
    const u32 r98 = call_g(rt, ctx, 0x08B0980Cu, {r97}).v0;
    put(rt, s0a + 0x8Cu, r98);
    {
        const u32 holder = word(rt, self);
        const u32 obj = call_g(rt, ctx, 0x08B62344u, {8u}).v0;
        call_g(rt, ctx, 0x088D65DCu, {obj});
        put(rt, holder + 0x7Cu, obj);
    }
    {
        const u32 holder = word(rt, self);
        const u32 obj = call_g(rt, ctx, 0x08B62344u, {8u}).v0;
        call_g(rt, ctx, 0x08894444u, {obj});
        put(rt, holder + 0x80u, obj);
    }
    {
        const u32 holder = word(rt, self);
        const u32 obj = call_g(rt, ctx, 0x08B62344u, {8u}).v0;
        call_g(rt, ctx, 0x08893F7Cu, {obj});
        put(rt, holder + 0x84u, obj);
    }
    {
        const u32 holder = word(rt, self);
        const u32 obj = call_g(rt, ctx, 0x08B62344u, {0x88u}).v0;
        call_g(rt, ctx, 0x088D441Cu, {obj});
        put(rt, holder + 0x88u, obj);
    }
    const u32 inner = word(rt, self);
    put(rt, inner + 0x28u, 0xFFFFFFFFu);
    put(rt, inner + 0x2Cu, 0u);
    tail_call(ctx, 0x0885C3C8u, {self, word(rt, self)});
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08A643B8 (a0 = vector, a1 = argument, a2 = index): erases the element at +0x24 by stepping the ring at +0x28
// through its virtual slots; returns 1 when the index was past the end, else runs the erase and returns 0.
void func_08A643B8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    (void)frame;
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 index = ctx.gpr[6];
    const u32 v1slot = self + 0x24u;
    const u32 first = word(rt, word(rt, self + 0x28u) - 4u);
    const u32 r0 = call_v(rt, ctx, word(rt, word(rt, first) + 8u), {first});
    u32 result = 1u;
    bool erased = (r0 == 2u);
    while (!erased) {
        const u32 node = word(rt, word(rt, v1slot + 4u) - 4u);
        call_v(rt, ctx, word(rt, word(rt, node) + 0xCu), {node, self});
        call_v(rt, ctx, word(rt, word(rt, node) + 4u), {node});
        const u32 newEnd = word(rt, v1slot + 4u) - 4u;
        const u32 begin = word(rt, self + 0x24u);
        const u32 count = static_cast<u32>(static_cast<s32>(newEnd - begin) >> 2);
        put(rt, v1slot + 4u, newEnd);
        if (!(index < count)) {
            result = 1u;
            break;
        }
        const u32 obj = word(rt, word(rt, self + 0x28u) - 4u);
        const u32 r = call_v(rt, ctx, word(rt, word(rt, obj) + 8u), {obj});
        if (r == 2u) erased = true;
    }
    if (erased) {
        const u32 node = word(rt, word(rt, v1slot + 4u) - 4u);
        call_v(rt, ctx, word(rt, word(rt, node) + 0xCu), {node, self});
        call_v(rt, ctx, word(rt, word(rt, node) + 4u), {node});
        put(rt, v1slot + 4u, word(rt, v1slot + 4u) - 4u);
        const u32 pushed = word(rt, self + 0x38u) + 8u;
        put(rt, self + 0x38u, pushed);
        call_g(rt, ctx, 0x08A61584u, {self, arg1});
        result = 0u;
    }
    ctx.gpr[2] = result;
}

} // namespace
} // namespace new_batch_main_2087
} // namespace gt::game

#define NEWMAIN2087(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2087::name, gt::ReturnKind::kind)
NEWMAIN2087(0x088F574Cu, func_088F574C, Word);
NEWMAIN2087(0x08AB8FD8u, func_08AB8FD8, Word);
NEWMAIN2087(0x08B49014u, func_08B49014, Word);
NEWMAIN2087(0x08939484u, func_08939484, Word);
NEWMAIN2087(0x08B19948u, func_08B19948, Word);
NEWMAIN2087(0x08A768F4u, func_08A768F4, Word);
NEWMAIN2087(0x08B970A4u, func_08B970A4, Word);
NEWMAIN2087(0x0885C704u, func_0885C704, Word);
NEWMAIN2087(0x08A643B8u, func_08A643B8, Word);
