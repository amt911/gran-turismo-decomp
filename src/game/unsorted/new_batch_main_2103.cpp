// New batch main-2103 (draft, unverified). Functions decompiled from tools.disasm, structured C.
// 0x08873ADC: releases the two reference slots of this: owner (+0x0C) first, then node (+0x14). A slot object keeps two table
// entries at +0x14 + 8k (object P) and +0x18 + 8k (block Q); k = 1 - state byte (+0x25) for the check and the block of the
// flags-bit1 path, k = state for the other. A block's counts at +4 and +8 each run a vtable slot when they drop to 1.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2103 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

u32 halfword_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// ll/sc decrement: stores old - 1 and returns the old count (the asm tests it against 1).
u32 dec_count(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}

// Block q: the count at +4 drops first (vtable slot 8 runs when it was 1), then the count at +8 (slot 0xC). A count that was
// not 1 ends the release at once.
void release_block(Runtime &rt, AllegrexContext &ctx, u32 q) {
    if (dec_count(rt, q + 4u) != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, q) + 8u), {q});
    if (dec_count(rt, q + 8u) != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, q) + 0xCu), {q});
}

void flip_state(Runtime &rt, u32 object) { put_byte(rt, object + 0x25u, 1u - byte_at(rt, object + 0x25u)); }

void clear_bit16(Runtime &rt, u32 object) { put(rt, object + 0x24u, word(rt, object + 0x24u) & ~0x10000u); }

// The owner slot (this+0xC). When the owner's flag bit 0 is set and the entry for k = 1 - state holds an object with bit 0
// set, the entry is released and F(a1) is called. Returns whether F was called.
bool owner_part(Runtime &rt, AllegrexContext &ctx, u32 self, u32 s2) {
    const u32 owner = word(rt, self + 0xCu);
    if ((halfword_at(rt, owner + 0x26u) & 1u) == 0u) return false;
    const u32 state = byte_at(rt, owner + 0x25u);
    const u32 other = 1u - state;
    const u32 entry = word(rt, owner + 0x14u + other * 8u);
    if (entry == 0u || (word(rt, entry + 0x18u) & 1u) == 0u) return false;
    if ((word(rt, entry + 0x18u) & 2u) != 0u) {
        const u32 block = word(rt, owner + 0x18u + other * 8u);
        put(rt, owner + 0x14u + other * 8u, 0u);
        put(rt, owner + 0x18u + other * 8u, 0u);
        if (block != 0u) release_block(rt, ctx, block);
    } else {
        const u32 block = word(rt, owner + 0x18u + state * 8u);
        put(rt, owner + 0x14u + state * 8u, 0u);
        put(rt, owner + 0x18u + state * 8u, 0u);
        if (block != 0u) release_block(rt, ctx, block);
        flip_state(rt, word(rt, self + 0xCu));
    }
    const u32 reloaded = word(rt, self + 0xCu);
    clear_bit16(rt, reloaded);
    call(rt, ctx, 0x0886BAF8u, {s2});
    return true;
}

// The node slot (this+0x14), same checks as the owner. Returns true when it ends in the tail call of F(a1); false means the
// function returns with v0 = 0.
bool node_part(Runtime &rt, AllegrexContext &ctx, u32 self) {
    const u32 node = word(rt, self + 0x14u);
    if ((halfword_at(rt, node + 0x26u) & 1u) == 0u) return false;
    const u32 state = byte_at(rt, node + 0x25u);
    const u32 other = 1u - state;
    const u32 entry = word(rt, node + 0x14u + other * 8u);
    if (entry == 0u || (word(rt, entry + 0x18u) & 1u) == 0u) return false;
    if ((word(rt, entry + 0x18u) & 2u) != 0u) {
        const u32 block = word(rt, node + 0x18u + other * 8u);
        put(rt, node + 0x14u + other * 8u, 0u);
        put(rt, node + 0x18u + other * 8u, 0u);
        if (block != 0u) release_block(rt, ctx, block);
        return true;
    }
    const u32 block = word(rt, node + 0x18u + state * 8u);
    put(rt, node + 0x14u + state * 8u, 0u);
    put(rt, node + 0x18u + state * 8u, 0u);
    if (block != 0u) release_block(rt, ctx, block);
    flip_state(rt, word(rt, self + 0x14u));
    return true;
}

void func_08873ADC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 s2 = ctx.gpr[5];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        owner_part(rt, ctx, self, s2);
        tail = node_part(rt, ctx, self);
        if (tail) {
            const u32 node = word(rt, self + 0x14u);
            clear_bit16(rt, node);
            ctx.gpr[2] = word(rt, node + 0x24u);
        } else {
            ctx.gpr[2] = 0u;
        }
    }
    if (tail) tail_call(ctx, 0x0886BAF8u, {s2});
}

}  // namespace
}  // namespace new_batch_main_2103
}  // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2103::name, gt::ReturnKind::kind)
NEWM(0x08873ADCu, func_08873ADC, Word);
