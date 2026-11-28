// Main-session batch 2091: structured C from tools.disasm (draft, unverified).
// Functions skipped as too hard for this batch: 0892FDA0 (jump table in .rodata), 08A09DD0 and 08A09648 (vector insert loops),
// 08873634 (float arguments in f12), 08B8F4B0 (guard-failure path leaves v1 from the callee).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2091 {
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

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// swl/swr pair: four bytes of `value`, little-endian, at an unaligned address.
void put_unaligned(Runtime &rt, u32 address, u32 value) {
    for (u32 i = 0; i < 4u; ++i) {
        put8(rt, address + i, (value >> (8u * i)) & 0xFFu);
    }
}

// Byte copy of n bytes (the swl/swr copy loops and the memcpy of the same data, in effect).
void copy_bytes(Runtime &rt, u32 source, u32 destination, u32 count) {
    for (u32 i = 0; i < count; ++i) {
        put8(rt, destination + i, rt.memory().load8(source + i));
    }
}

// ll/sc decrement: returns the old value.
u32 atomic_decrement(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}

} // namespace

// v0 = result of the list walk; a node with a vtable at +0x2C is cleared through its virtual, then the list at +0x18 and +0x10 is released.

// Two unaligned copies of a 128-byte table and an 8-word pointer table, then a constructor and a tail call.
void func_08A151FC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    const u32 arg0 = ctx.gpr[4];

    const u32 obj = call(rt, ctx, 0x08A5BC20u, {0x1B0u, 4u});
    call(rt, ctx, 0x08A2A154u, {obj});
    put(rt, obj, 0x08CD04B0u);
    const u32 s2 = obj + 0x18u;
    call(rt, ctx, 0x08A1A914u, {s2});
    call(rt, ctx, 0x08B28D34u, {obj + 0xACu});
    call(rt, ctx, 0x08B28D34u, {obj + 0xD8u});

    const u32 s1 = obj + 0x10Du;
    put(rt, obj + 0x104u, 0u);
    put(rt, obj + 0x108u, 0u);
    put8(rt, obj + 0x10Cu, 0u);
    copy_bytes(rt, 0x08C8E2B0u, s1, 0x80u);

    const u32 table = 0x08C8E290u;
    put_unaligned(rt, obj + 0x18Du, word(rt, table));
    put_unaligned(rt, obj + 0x191u, word(rt, table + 0x4u));
    put_unaligned(rt, obj + 0x195u, word(rt, table + 0x8u));
    put_unaligned(rt, obj + 0x199u, word(rt, table + 0xCu));
    put_unaligned(rt, obj + 0x19Du, word(rt, table + 0x10u));
    put_unaligned(rt, obj + 0x1A1u, word(rt, table + 0x14u));
    put_unaligned(rt, obj + 0x1A5u, word(rt, table + 0x18u));
    put_unaligned(rt, obj + 0x1A9u, word(rt, table + 0x1Cu));
    call(rt, ctx, 0x08B36018u, {s1, 0u, 0x80u});
    call(rt, ctx, 0x08B36018u, {obj + 0x18Du, 0u, 0x20u});
    call(rt, ctx, 0x08B7C830u, {s1, 0x08CA9A84u, 0x80u});
    put(rt, obj + 0x14u, s2);

    put(rt, sp, obj);  // delay slot of the call below
    call(rt, ctx, 0x08A2A218u, {arg0, sp});
}

// Writes a 0x8C-byte object, guards two static inits, copies a 0xB5-byte record and tail-calls 08896E38.
void func_088979C8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];
    const u32 src = ctx.gpr[5];

    const u32 obj = call(rt, ctx, 0x08B62344u, {0xB8u});
    const u32 s1 = obj + 0x8Cu;
    call(rt, ctx, 0x0894BF50u, {s1});
    call(rt, ctx, 0x08B36018u, {obj, 0u, 0x8Cu});
    call(rt, ctx, 0x0894BEFCu, {s1});
    put(rt, obj + 0x9Cu, 0xFFFFFFFFu);
    put(rt, obj + 0xA0u, 0u);
    put(rt, obj + 0xA4u, 0u);
    put(rt, obj + 0xA8u, 0u);
    put(rt, obj + 0xACu, 0u);
    for (u32 i = 0; i <= 7u; ++i) {
        put8(rt, obj + 0xB0u + i, 0u);
    }
    put(rt, out, obj);

    // Static guard for 0x08D75EE8; the object's slot 0x08D75EF0 is set in the acquire path.
    if (rt.memory().load8(0x08D75EE8u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08D75EE8u}) != 0u) {
        put(rt, 0x08D75EF0u, 0x08CC1FF0u);
        call(rt, ctx, 0x08B64494u, {0x08D75EE8u});
    }
    const u32 slot = 0x08D75EF0u;

    const u32 s1b = out + 0x8u;
    put(rt, out + 0x4u, slot);
    call(rt, ctx, 0x08896E30u, {s1b});

    const u32 copy_src = word(rt, src);
    const u32 copy_dst = word(rt, out);
    for (u32 off = 0; off <= 0xB4u; off += 4u) {
        put(rt, copy_dst + off, word(rt, copy_src + off));
    }
    tail_call(ctx, 0x08896E38u, {s1b, src + 0x8u});
}

// Releases two nodes (vtable +0x2C, then the ll/sc refcounts at +4 and +8 with their virtuals at +8 / +0xC), then tail-calls the destructor.

// Constructs an object of 0x9770+ bytes behind a static guard, fills the 0x30 table and returns the head of the registry.
void func_08B0AE94(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 out = ctx.gpr[4];

    const u32 obj = call(rt, ctx, 0x08B62344u, {0x2A450u});
    const u32 s1 = obj + 0x9770u;
    put(rt, obj, 0u);
    put(rt, obj + 0xB4u, 0u);
    put(rt, obj + 0xB8u, 0u);
    put(rt, obj + 0xBCu, 0u);
    put(rt, obj + 0xC0u, 0u);
    call(rt, ctx, 0x08B09F98u, {obj + 0xD4u});
    call(rt, ctx, 0x08B08B1Cu, {obj + 0x100u});
    put(rt, obj + 0x104u, 0u);
    put(rt, obj + 0x108u, 0u);
    put(rt, obj + 0x10Cu, 0u);
    put(rt, obj + 0x114u, 0u);
    put(rt, obj + 0x118u, 0u);
    put8(rt, obj + 0x11Cu, 0u);
    put8(rt, obj + 0x11Du, 0u);
    call(rt, ctx, 0x08B2F31Cu, {obj + 0x120u});
    call(rt, ctx, 0x08B451A0u, {s1});
    for (u32 off = 0x4u; off <= 0x17u; ++off) {
        put8(rt, obj + off, 0u);
    }
    call(rt, ctx, 0x08B36018u, {obj + 0x18u, 0u, 0x18u});
    call(rt, ctx, 0x08B36018u, {obj + 0x30u, 0u, 0x80u});
    put(rt, obj + 0xCCu, 0x1Eu);
    put8(rt, obj + 0xB0u, 0u);
    put(rt, obj + 0xC8u, 0x3Cu);
    put(rt, obj + 0xD0u, 0x5Au);
    put(rt, obj + 0x110u, s1);
    put8(rt, obj + 0xB2u, 0u);
    put8(rt, obj + 0xB1u, 0u);
    put(rt, obj + 0xC4u, 0x3Cu);
    put(rt, out, obj);

    const u32 guard = 0x08D77340u;
    const u32 slot = 0x08D77348u;
    if (rt.memory().load8(guard) == 0u && call(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        put(rt, slot, 0x08CD3B20u);
        call(rt, ctx, 0x08B64494u, {guard});
    }
    put(rt, out + 0x4u, slot);

    u32 cur = word(rt, 0x08D473BCu);
    if (cur == 0u) {
        cur = out;
    }
    put(rt, 0x08D473BCu, cur);
    ctx.gpr[2] = cur;
}

// Constructs a 0x28-byte object with two embedded tables, a 0x68-byte object and a 0x10-byte node, then tail-calls 08B8E380.
void func_0887465C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 s3 = self + 0xCu;

    put(rt, self, 0u);
    put(rt, self + 0x4u, 0u);
    put(rt, self + 0x8u, 0u);
    const u32 obj = call(rt, ctx, 0x08B62344u, {0x28u});
    put(rt, obj, 0x08CBFC50u);
    const u32 s4 = 0x08D752A4u;
    put(rt, obj + 0x4u, 0u);
    put(rt, obj + 0x8u, 0u);
    put(rt, obj + 0xCu, s4);
    put(rt, obj + 0x10u, s4);
    for (u32 off = 0x14u; off <= 0x20u; off += 8u) {
        put(rt, obj + off, 0u);
        put(rt, obj + off + 0x4u, 0u);
    }
    put8(rt, obj + 0x24u, 0u);
    put8(rt, obj + 0x25u, 0u);
    put(rt, obj, 0x08CBFC10u);
    put(rt, obj + 0x24u, word(rt, obj + 0x24u) & ~0x10000u);
    put(rt, s3, obj);  // delay slot of the allocation below
    const u32 blk = call(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, blk, 0x08CBFC90u);
    put(rt, blk + 0x4u, 1u);
    put(rt, blk + 0xCu, obj);
    put(rt, blk + 0x8u, 1u);
    put(rt, s3 + 0x4u, blk);  // delay slot of the call below
    call(rt, ctx, 0x08B8E380u, {self + 0x10u, obj, obj, 1u});

    const u32 obj2 = call(rt, ctx, 0x08B62344u, {0x68u});
    put(rt, obj2, 0x08CBFB30u);
    put(rt, obj2 + 0x4u, 0u);
    put(rt, obj2 + 0x8u, 0u);
    put(rt, obj2 + 0xCu, s4);
    put(rt, obj2 + 0x10u, s4);
    for (u32 off = 0x14u; off <= 0x20u; off += 8u) {
        put(rt, obj2 + off, 0u);
        put(rt, obj2 + off + 0x4u, 0u);
    }
    put8(rt, obj2 + 0x24u, 0u);
    put8(rt, obj2 + 0x25u, 0u);
    put(rt, obj2, 0x08CBFAF0u);
    put(rt, obj2 + 0x24u, word(rt, obj2 + 0x24u) & ~0x10000u);
    const u32 s0 = self + 0x18u;
    put(rt, self + 0x14u, obj2);  // delay slot of the allocation below
    const u32 blk2 = call(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, blk2 + 0x4u, 1u);
    put(rt, blk2, 0x08CBFBB0u);
    put(rt, blk2 + 0x8u, 1u);
    put(rt, blk2 + 0xCu, obj2);
    put(rt, self + 0x18u, blk2);
    tail_call(ctx, 0x08B8E380u, {s0, obj2, obj2, 1u});
}

// Calls a vtable slot to produce a 0x2400 record, fills it, then the 8-entry table loop; a null callback takes the same record path.

// Registers one 0x40-byte record from the table, then the same record on the second chain.
void func_089104C8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = ctx.gpr[29];
    const u32 s3 = ctx.gpr[4];
    const u32 s2 = ctx.gpr[5];
    const u32 s6 = ctx.gpr[6];
    const u32 s4 = sp + 0xCu;
    const u32 s7 = sp + 0x10u;
    bool ok = false;

    call(rt, ctx, 0x08A9FA9Cu, {s4, 0x08CA17F8u});
    if (word(rt, sp + 0xCu) != 0u) {
        call(rt, ctx, 0x08A810F4u, {s7, s4});
        if (word(rt, sp + 0x10u) != 0u) {
            const u32 s0 = sp + 0x8u;
            call(rt, ctx, 0x08B85EB8u, {s0, s6, sp + 0x1u});
            const u32 s5 = sp + 0x14u;
            call(rt, ctx, 0x08A6D2D4u, {s5, s0});
            const u32 a0v = word(rt, sp + 0x8u);
            if (a0v - 0xCu != 0x08D74FD0u) {
                if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {a0v - 0x4u, 0xFFFFFFFFu})) <= 0) {
                    call(rt, ctx, 0x08B8542Cu, {a0v - 0xCu, sp});
                }
            }
            const u32 s1 = word(rt, sp + 0x10u);
            if (s1 == 0u) {
                call(rt, ctx, 0x08A5BB7Cu, {0x08CA1804u});
            }
            const u32 fn = word(rt, word(rt, s1) + 0x9Cu);
            call(rt, ctx, fn, {sp + 0x4u, s1, s5});
            call(rt, ctx, 0x08A5BAD8u, {s4, word(rt, sp + 0x4u)});
            call(rt, ctx, 0x08A5B9ECu, {sp + 0x4u});
            if (word(rt, sp + 0xCu) != 0u) {
                const u32 s1b = sp + 0x18u;
                call(rt, ctx, 0x08A6C410u, {s1b, s4});
                const u32 p = word(rt, sp + 0x18u);
                if (p == 0u) {
                    call(rt, ctx, 0x08A5BB7Cu, {0x08CA180Cu});
                }
                const u32 v = call(rt, ctx, 0x08A6BFFCu, {p});
                call(rt, ctx, 0x08B7C830u, {s3, v, s2});
                put8(rt, s3 + s2 - 1u, 0u);
                call(rt, ctx, 0x08A5B9ECu, {s1b});
                call(rt, ctx, 0x08A5B9ECu, {s5});
                call(rt, ctx, 0x08A5B9ECu, {s7});
                ok = true;
            } else {
                call(rt, ctx, 0x08A5B9ECu, {s5});
                call(rt, ctx, 0x08A5B9ECu, {s7});
            }
        } else {
            call(rt, ctx, 0x08A5B9ECu, {s7});
        }
    }
    if (!ok) {
        call(rt, ctx, 0x08B7C830u, {s3, s6, s2});
        put8(rt, s3 + s2 - 1u, 0u);
    }
    call(rt, ctx, 0x08A5B9ECu, {s4});
    ctx.gpr[2] = ok ? 1u : 0u;
}

} // namespace new_batch_main_2091
} // namespace gt::game

#define NEWMAIN2091(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2091::name, gt::ReturnKind::kind)
NEWMAIN2091(0x08A151FCu, func_08A151FC, Word);
NEWMAIN2091(0x088979C8u, func_088979C8, Word);
NEWMAIN2091(0x08B0AE94u, func_08B0AE94, Word);
NEWMAIN2091(0x0887465Cu, func_0887465C, Word);
NEWMAIN2091(0x089104C8u, func_089104C8, Word);
