// Main-session batch 1594 (drafted, unverified): structured C from tools.disasm. Functions that the gate mismatched or
// skipped before. Each body follows its asm: named locals, G[n] only at the call boundary, delay slots before the call,
// v0 as the asm leaves it on every exit path. Skipped (too unclear to draft): 0889A2A4, 08ADCA78, 089128E8, 08AA6A4C.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1594 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// Saves and restores s0 (guest $16) around a body, as in the rest of the unsorted batches.
struct Saved {
    AllegrexContext &ctx;
    u32 s0;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]) {}
    ~Saved() { ctx.gpr[16] = s0; }
};

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 sbyte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 shalf(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// Word-by-word copy in the order the asm's copy loops run (the ranges never overlap in these functions).
void copy_words(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    for (u32 offset = 0; offset < bytes; offset += 4u) {
        put(rt, destination + offset, word(rt, source + offset));
    }
}

// The five "build a record, walk a constant table, drop the list" functions (0x40 frame). `message` goes to the first
// call, the table holds 8-byte records (a key word and a value word), `assertion` is the message of the null-head guard.
void table_drop(Runtime &rt, AllegrexContext &ctx, u32 self, u32 message, u32 table_begin, u32 table_end, u32 assertion) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = ctx.gpr[29];
    const u32 record = sp + 0x8u;
    const u32 cell = sp + 0xCu;
    call(rt, ctx, 0x08A69B3Cu, {sp, message});
    const u32 storage = call(rt, ctx, 0x08A5BC20u, {0x38u, 0x4u});
    const u32 node = call(rt, ctx, 0x08A7E914u, {storage, sp});
    put(rt, sp + 0x4u, node);
    call(rt, ctx, 0x08A76AB4u, {cell, sp + 0x4u});
    call(rt, ctx, 0x08A69D68u, {sp});
    u32 entry = table_begin;
    do {
        const u32 head = word(rt, sp + 0xCu);
        if (head == 0u) {
            call(rt, ctx, 0x08A5BB7Cu, {assertion});
        }
        const u32 key = word(rt, entry);
        const u32 value = word(rt, entry + 4u);
        call(rt, ctx, 0x08A85B1Cu, {record, value});
        entry += 8u;
        call(rt, ctx, 0x08B86E98u, {head, key, record});
        call(rt, ctx, 0x08A5B9ECu, {record});
    } while (entry != table_end);
    const u32 owner = word(rt, self);
    if (owner == 0u) {
        call(rt, ctx, 0x08A5BB7Cu, {assertion});
    }
    call(rt, ctx, 0x08A7CE50u, {owner, cell});
    call(rt, ctx, 0x08A5B9ECu, {cell});
}

// 089395C0: copies the 0x3C-byte block at a1-side staging into the frame and back, then fills a0's +0x1C/+0x20 when the
// first word is zero. v0 = 1 when the fill ran, 0 otherwise.

// 089A5994: constructor-shaped: sets two vtables, calls the member constructors, releases the list fields at
// 0x12C/0x128/0x130 (each node is the list head minus 0xC; the sentinel 0x08D74FD0 is the empty list), then calls 089A29E4.

// 08AD71F8: initialiser: stores the header halves and 1.0f at a0, allocates count * 8 bytes into +0x34/+0x38/+0x3C,
// then fills six sub-records at +0x40..+0x90 through 08B2AE10.
void func_08AD71F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 kind = ctx.gpr[6];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x30u);
        const u32 sp = ctx.gpr[29];
        put16(rt, self + 0x6u, kind);
        put(rt, self, 0x3F800000u);
        put16(rt, self + 0x4u, count);
        call(rt, ctx, 0x08AD6854u, {self + 0x8u, 0x38u, count});
        put(rt, self + 0x34u, 0u);
        put(rt, self + 0x38u, 0u);
        put(rt, self + 0x3Cu, 0u);
        if (0x1FFFFFFFu < count) {
            call(rt, ctx, 0x08B5A9C8u, {ctx.gpr[4]});
        }
        const u32 bytes = count << 3;
        const u32 block = call(rt, ctx, 0x08B62344u, {bytes});
        put(rt, self + 0x3Cu, block + bytes);
        put(rt, self + 0x34u, block);
        put(rt, self + 0x38u, block);
        put(rt, sp + 0xCu, 0u);
        put(rt, sp + 0x8u, 0u);
        call(rt, ctx, 0x08C3E1CCu, {block, count, sp + 0x8u, sp});
        put(rt, self + 0x38u, bytes + block);
        call(rt, ctx, 0x08B2AE10u, {self + 0x40u, count});
        call(rt, ctx, 0x08B2AE10u, {self + 0x50u, count});
        call(rt, ctx, 0x08B2AE10u, {self + 0x60u, count});
        call(rt, ctx, 0x08B2AE10u, {self + 0x70u, count});
        call(rt, ctx, 0x08B2AE10u, {self + 0x80u, count});
        call(rt, ctx, 0x08B2AE10u, {self + 0x90u, count});
    }
}

// 0880FEE4: builds the object at a0 (vtable store, base call, 0x2E0-byte block into +0x38 and +0xE8, field fills). The
// guard byte at 0x08D75108 runs a one-time init that stores 0x08CBCFE8 at 0x08D75110 and then calls 08B64494.

// 0885A6BC and the four 0x08A4646C-family functions: the same list build and drop, with their own message, table and
// assertion (see table_drop).

// 088B31D0: destructor-shaped: resets the vtables and the four owned pointers (+0x318, +0x310, +0x2F0, +0x2E4) with
// their frees, then walks the 0x400-byte element array headed at +0x2DC (count word at head - 4) calling each element's
// vtable slot (the first word of the element's object), frees the array and clears +0x2DC/+0x2E0.

// 08AF2D98: constructor: allocates 0xA8 bytes, fills the object (a1 at +0, a2 at +4, floats 0.5/2000/1.0, flags), stores it
// into a0, and sets +4 to the 0x08D77288 table entry (guard byte 0x08D77280 runs the one-time init).

// 08880330: object setup: vtable stores, a 0x78-byte sub-object at +0x70 and +0x74, one-time init of 0x08D75E10, then
// four 0886AAA4 registrations and a tail call into 0886AAA4 for the last one.
void func_08880330(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 sub;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x0886AD04u, {self});
        put(rt, self, 0x08CC11C8u);
        put(rt, self + 0x70u, 0x08CC1204u);
        const u32 block = call(rt, ctx, 0x08B62344u, {0x78u});
        call(rt, ctx, 0x08886A60u, {block + 0x4u});
        call(rt, ctx, 0x0886FA58u, {block + 0x14u});
        call(rt, ctx, 0x08884AC8u, {block + 0x28u});
        call(rt, ctx, 0x088710DCu, {block + 0x3Cu});
        call(rt, ctx, 0x0886FC04u, {block + 0x50u});
        call(rt, ctx, 0x0886FDB0u, {block + 0x64u});
        call(rt, ctx, 0x0886F8BCu, {block + 0x50u, 1u});
        put(rt, self + 0x74u, block);
        const u32 flag = 0x08D75E08u;
        const u32 slot = 0x08D75E10u;
        if (sbyte(rt, flag) == 0) {
            if (call(rt, ctx, 0x08B6441Cu, {flag}) != 0u) {
                put(rt, slot, 0x08CC1270u);
                call(rt, ctx, 0x08B64494u, {flag});
            }
        }
        put(rt, self + 0x78u, slot);
        call(rt, ctx, 0x0886AAA4u, {self, block + 0x4u});
        call(rt, ctx, 0x0886AAA4u, {self, block + 0x14u});
        call(rt, ctx, 0x0886AAA4u, {self, block + 0x28u});
        sub = block + 0x3Cu;
    }
    tail_call(ctx, 0x0886AAA4u, {self, sub});
}

// 0898D5A0: object reset: vtables at +0 and +0x38, three list release checks (the 0x08D74FD0 sentinel is the empty
// list), buffer copies through 08BEEA84 and 08A5BC54, then 0897D548 and 0896BC0C.

// 08AD30B0: init of the object at a0: sub-initialisers, the float/flag fields, a 0xA0 buffer into +0x29C via 08AD6AB8,
// one-time init of 0x08D77260/0x08D77268, then 08C3DECC and 08B28DCC.
void func_08AD30B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        call(rt, ctx, 0x08AD16C0u, {self});
        call(rt, ctx, 0x08AD1098u, {self + 0x1D0u});
        put(rt, self + 0x260u, 0x42480000u);
        put(rt, self + 0x268u, 0x3F800000u);
        put(rt, self + 0x250u, 0x3F800000u);
        put(rt, self + 0x254u, 0x3F800000u);
        put(rt, self + 0x258u, 0x3F800000u);
        put(rt, self + 0x25Cu, 0x3F800000u);
        put(rt, self + 0x264u, 0x3F800000u);
        put8(rt, self + 0x27Au, 1u);
        put(rt, self + 0x26Cu, 0u);
        put16(rt, self + 0x270u, 0u);
        put16(rt, self + 0x272u, 0u);
        put16(rt, self + 0x274u, 0u);
        put16(rt, self + 0x276u, 0u);
        put8(rt, self + 0x278u, 0u);
        put8(rt, self + 0x279u, 0u);
        call(rt, ctx, 0x08AD0FACu, {self + 0x27Cu});
        const u32 buffer = call(rt, ctx, 0x08B62344u, {0xA0u});
        call(rt, ctx, 0x08AD6AB8u, {buffer, 0x80u, 0x18u});
        put(rt, self + 0x29Cu, buffer);
        if (sbyte(rt, 0x08D77260u) == 0) {
            if (call(rt, ctx, 0x08B6441Cu, {0x08D77260u}) != 0u) {
                put(rt, 0x08D77268u, 0x08CD6CF0u);
                call(rt, ctx, 0x08B64494u, {0x08D77260u});
            }
        }
        put(rt, self + 0x2A0u, 0x08D77268u);
        call(rt, ctx, 0x08C3DECCu, {self + 0x2A4u});
        call(rt, ctx, 0x08B28DCCu, {self + 0x40B0u});
        put(rt, self + 0x240u, 0x3F800000u);
    }
}

// 08AD272C: teardown of the object at a0: 3304E0 on the member at +0x40B0, releases the owner at +0x29C/+0x2A0, walks
// the 0x7C-byte node list headed by the halfword at +0x2A4 (node index * 124 + 0xC), calls the 128 slots of the
// 0x3D90..0x3E0C pointer table backwards, then the owner's vtable slot 0.

// 08A61C58: list insert/drain: copies the owner's fields into a 0xC-byte record at the pair's cursor (+4, bumps it by 0xC),
// then walks the owner's pointer array from end-4 down to begin, calling each element's vtable slot +0x14 and appending
// the answer as a record. v0 follows the last call (or the pair's +8 word when nothing ran).
void func_08A61C58(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 pair = ctx.gpr[5];
    u32 v0;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x40u);
        const u32 sp = ctx.gpr[29];
        const u32 field34 = word(rt, self + 0x34u);
        const u32 field3c = word(rt, self + 0x3Cu);
        const u32 field18 = word(rt, self + 0x18u);
        v0 = word(rt, pair + 0x8u);
        put(rt, sp + 0x4u, field34);
        put(rt, sp, field18);
        put(rt, sp + 0x8u, field3c);
        u32 cursor = word(rt, pair + 0x4u);
        if (cursor == v0) {
            v0 = call(rt, ctx, 0x08C1FC90u, {pair, cursor, sp});
        } else {
            if (cursor != 0u) {
                put(rt, cursor + 0x8u, field3c);
                put(rt, cursor, field18);
                put(rt, cursor + 0x4u, field34);
            }
            put(rt, pair + 0x4u, word(rt, pair + 0x4u) + 0xCu);
        }
        const u32 begin = word(rt, self + 0x24u);
        const u32 end = word(rt, self + 0x28u);
        if (end != begin) {
            u32 link = end;
            u32 slot;
            do {
                slot = link - 4u;
                put(rt, sp + 0xCu, 0u);
                put(rt, sp + 0x10u, 0u);
                put(rt, sp + 0x14u, 0u);
                const u32 element = word(rt, slot);
                const u32 target = word(rt, word(rt, element) + 0x14u);
                v0 = call_indirect(rt, ctx, target, {element, sp + 0xCu});
                if (v0 != 0u) {
                    v0 = word(rt, pair + 0x8u);
                    cursor = word(rt, pair + 0x4u);
                    if (cursor == v0) {
                        v0 = call(rt, ctx, 0x08C1FC90u, {pair, cursor, sp + 0xCu});
                    } else {
                        if (cursor != 0u) {
                            put(rt, cursor + 0x4u, word(rt, sp + 0x10u));
                            v0 = word(rt, sp + 0xCu);
                            put(rt, cursor, v0);
                            put(rt, cursor + 0x8u, word(rt, sp + 0x14u));
                        }
                        put(rt, pair + 0x4u, word(rt, pair + 0x4u) + 0xCu);
                    }
                }
                link = slot;
            } while (slot != begin);
        }
    }
    ctx.gpr[2] = v0;
}

} // namespace
} // namespace new_batch_main_1594
} // namespace gt::game

#define NEWM(address, name) GT_DECOMP(address, gt::game::new_batch_main_1594::name, gt::ReturnKind::Word)
NEWM(0x08AD71F8u, func_08AD71F8);
NEWM(0x08880330u, func_08880330);
NEWM(0x08AD30B0u, func_08AD30B0);
NEWM(0x08A61C58u, func_08A61C58);
