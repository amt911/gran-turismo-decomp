// New batch 35 (main session): functions picked from the emitted set and written as structured C from `tools.disasm`.
// Sizes are 0x3EC-0x3F4 (251-253 instructions) each, so every body here is a whole function read instruction by
// instruction. No names: none of them has evidence yet, so they keep their addresses.
//
// `call_set` keeps the shape tools.arg_reads scans (literal address + the argument list) and stores the result in v0 as
// the original's `jal` does; `call_indirect` (gt/game/hot_common.hpp) is the `lw v0, off(vtable); jalr v0` form.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_35 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------
// 0x089DA994 (selector = a0, id = a1): an engine table setup chosen by the two arguments.
//   selector 1, id 0xFFFF: constructs the two statics at 0x08CF5428 / 0x08CF5400, hands the 0x1C0-byte descriptor at
//   0x08CF531C to 0x08A92114, fills 0x08CDEAB8 from eleven words of 0x08C8CC00..0x08C8CC28, passes the 0x0B-entry table
//   to 0x08B2095C, registers a callback pair with 0x0897D534 and then twelve (name, value) pairs through 0x08C0645C;
//   v0 is the last registration's result.
//   selector 0, id 0xFFFF: destroys 0x08CF5400 and tail-calls the destructor of 0x08CF5428.
//   Anything else: v0 = 0xFFFF (selectors 0 and 1 with another id) or the entry v0 (any other selector).
// The arguments are only compared, never dereferenced.

// ---------------------------------------------------------------------------------------------------------------
// 0x08ADA094 (handle = a0, spare = a1, size = a2, arg = a3): a block allocator over two doubly linked rings of
// descriptors (0x08D40614 and 0x08D40600 for the rings, 0x08D40610 the ring the reserve block is moved to). Three
// reporting switches at 0x08CDFB3C gate the 0x08AD83B8 messages (bit 0, bit 1). v0 = -1 on every failure path and the
// block's word +8 on success.

// Takes `node` out of the ring it links itself into (a node that links to itself is alone).
void unlink_node(Runtime &rt, u32 node) {
    const u32 first = word(rt, node);
    if (first == node) return;
    const u32 next = word(rt, node + 4u);
    put(rt, first + 4u, next);
    const u32 previous = word(rt, node + 4u);
    put(rt, node + 4u, node);
    put(rt, previous, first);
    put(rt, node, node);
}
// Links the (already unlinked) `node` in before the sentinel `ring`.
void insert_before(Runtime &rt, u32 node, u32 ring) {
    const u32 head = word(rt, ring);
    put(rt, node + 4u, ring);
    put(rt, head + 4u, node);
    const u32 back = word(rt, node + 4u);
    put(rt, node, head);
    put(rt, back, node);
}
void func_08ADA094(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const s32 handle = static_cast<s32>(ctx.gpr[4]);
    u32 spare = ctx.gpr[5];
    const s32 size = static_cast<s32>(ctx.gpr[6]);
    const u32 arg = ctx.gpr[7];
    u32 found = ctx.gpr[16];  // the descriptor, or the caller's s0 while the reserve ring was empty
    u32 filled = 0u;
    bool taken = false;
    u32 reserve = 0u;
    // The failure exit the asm reaches from everywhere: optional progress report, release of the fill block, and the
    // reserve descriptor goes back into its ring.
    const auto unwind = [&]() {
        if (taken && handle >= 0) call_set(rt, ctx, 0x08ADA528u, {static_cast<u32>(handle)});
        if (filled != 0u) call_set(rt, ctx, 0x08AD85C8u, {filled});
        if (reserve != 0u) {
            call_set(rt, ctx, 0x08AEB808u, {});
            unlink_node(rt, reserve);
            insert_before(rt, reserve, 0x08D40610u);
            call_set(rt, ctx, 0x08AEB82Cu, {});
        }
        ctx.gpr[2] = 0xFFFFFFFFu;
    };

    if (handle >= 0 && static_cast<s32>(call_set(rt, ctx, 0x08ADA718u, {static_cast<u32>(handle)})) < size) {
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u)
            call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C90B68u, 0xB0u, 0x08C90C18u});
        unwind();
        return;
    }
    call_set(rt, ctx, 0x08AEB808u, {});
    const u32 reserve_ring = word(rt, 0x08D40614u);
    if (static_cast<s32>(word(rt, reserve_ring + 8u)) >= 0) {
        reserve = reserve_ring;
        found = reserve;
        unlink_node(rt, reserve);
    }
    call_set(rt, ctx, 0x08AEB82Cu, {});
    if (found == 0u) {
        if ((word(rt, 0x08CDFB3Cu) & 2u) != 0u)
            call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB44u), 0x08C90B68u, 0xC2u, 0x08C90BFCu, spare,
                                            static_cast<u32>(size)});
        unwind();
        return;
    }
    if (handle >= 0) {
        if (call_set(rt, ctx, 0x08ADA4D8u, {static_cast<u32>(handle)}) == 0u) {
            unwind();
            return;
        }
        const s32 state = static_cast<s32>(call_set(rt, ctx, 0x08ADAB0Cu, {static_cast<u32>(handle)}));
        taken = true;  // s7 = 1 sits in the branch's delay slot: both continuations have it
        if (state <= 0) {
            if (static_cast<s32>(call_set(rt, ctx, 0x08ADA718u, {static_cast<u32>(handle)})) >= size) {
                goto carve;
            }
            if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u)
                call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C90B68u, 0xDFu, 0x08C90BC0u,
                                                static_cast<u32>(size)});
            unwind();
            return;
        }
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u)
            call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C90B68u, 0xD7u, 0x08C90BA0u});
        unwind();
        return;
    }
    if (spare == 0u) {
        filled = call_set(rt, ctx, 0x08AD84B4u, {static_cast<u32>(size), 0u});
        spare = filled;
        if (filled == 0u) {
            if ((word(rt, 0x08CDFB3Cu) & 2u) != 0u)
                call_set(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB44u), 0x08C90B68u, 0xF9u, 0x08C90BE0u,
                                                static_cast<u32>(size)});
            unwind();
            return;
        }
    }
    goto describe;

carve:
    {
        const u32 carved = call_set(rt, ctx, 0x08AD9E74u, {static_cast<u32>(handle), 0u});
        put(rt, carved + 0x20u, word(rt, carved + 0x20u) - static_cast<u32>(size));
        spare = word(rt, carved + 0x28u) + (word(rt, carved + 0x2Cu) - static_cast<u32>(size));
        put(rt, carved + 0x2Cu, word(rt, carved + 0x2Cu) - static_cast<u32>(size));
        if (word(rt, carved + 0x30u) != 0u || word(rt, carved + 0x34u) != 0u)
            call_set(rt, ctx, 0x08ADA644u, {static_cast<u32>(handle), word(rt, found + 0x30u),
                                            word(rt, found + 0x34u)});
    }
describe:
    // Descriptor setup: the fields every path shares, then the link into the used ring.
    put_half(rt, found + 0x10u, 1u);
    put_half(rt, found + 0x12u, static_cast<u32>(handle));
    put(rt, found + 0x18u, filled);
    put(rt, found + 0x28u, spare);
    put(rt, found + 0x2Cu, static_cast<u32>(size));
    put(rt, found + 0x44u, arg);
    put(rt, found + 0x54u, 0xFFFFFFFFu);
    put(rt, found + 0x0Cu, 1u);
    put_byte(rt, found + 0x14u, 0u);
    put_byte(rt, found + 0x15u, 0u);
    put_byte(rt, found + 0x16u, 0u);
    put_byte(rt, found + 0x17u, 0u);
    put(rt, found + 0x1Cu, spare);
    put(rt, found + 0x20u, static_cast<u32>(size));
    put(rt, found + 0x24u, static_cast<u32>(size));
    put(rt, found + 0x30u, 0u);
    put(rt, found + 0x34u, 0u);
    put(rt, found + 0x38u, 0u);
    put(rt, found + 0x3Cu, 0u);
    put(rt, found + 0x40u, 0u);
    put(rt, found + 0x48u, 0u);
    put(rt, found + 0x4Cu, 0u);
    put(rt, found + 0x50u, 0xFFFFFFFFu);  // written in the lock call's delay slot, so before it
    call_set(rt, ctx, 0x08AEB808u, {});
    unlink_node(rt, found);
    insert_before(rt, found, 0x08D40600u);
    call_set(rt, ctx, 0x08AEB82Cu, {});
    ctx.gpr[2] = word(rt, found + 8u);
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08931970 leaves its v1 = word(a0 + 0x14) (a three-instruction getter: v0 = +0x10, v1 = +0x14); the setter after it
// takes that word as its third argument.
u32 getter_second_word(Runtime &rt, u32 object) { return word(rt, object + 0x14u); }

// 0x0884865C (object = a3, count = a2, track = a1): builds the per-track block of a scene object. count <= 0 returns
// the entry v0; the rest picks the object at +0 of `object` through 0x08A76F20 and fills the block with the values the
// 0x0893xxxx getters/0x088D5xxx setters move into it. Its own a0 is written before it is read.
void func_0884865C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 track = ctx.gpr[5];
    const u32 object = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) <= 0) return;
    const u32 holder = frame.at(0xCu);
    call_set(rt, ctx, 0x0883A2F4u, {holder});

    // The tail every path shares: the object at frame + 4, its member through 0x08846E50, then the value setters.
    const auto fill_block = [&](u32 block) {
        const u32 track_holder = frame.at(4u);
        call_set(rt, ctx, 0x08847008u, {track_holder, track});
        const u32 inner = word(rt, track_holder);
        if (inner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
        const u32 target = call_set(rt, ctx, 0x08846E50u, {inner});
        call_set(rt, ctx, 0x08A5B9ECu, {track_holder});
        call_set(rt, ctx, 0x088D5A14u, {target, call_set(rt, ctx, 0x08931718u, {block})});
        call_set(rt, ctx, 0x088D5A04u, {target, call_set(rt, ctx, 0x089316F4u, {block})});
        const u32 flag = call_set(rt, ctx, 0x088D63D4u, {target});
        call_set(rt, ctx, 0x088D5A24u, {target, flag == 1u ? 1u : 0u});
        call_set(rt, ctx, 0x088D5AB8u, {target, call_set(rt, ctx, 0x08931790u, {block})});
        call_set(rt, ctx, 0x088D5AA8u, {target, call_set(rt, ctx, 0x089316D0u, {block})});
        call_set(rt, ctx, 0x088D5ED0u,
                 {target, 0u, call_set(rt, ctx, 0x08931970u, {block}), getter_second_word(rt, block), 0u});
        call_set(rt, ctx, 0x088D5A8Cu, {target, call_set(rt, ctx, 0x08931784u, {block})});
        call_set(rt, ctx, 0x088D5A70u, {target, call_set(rt, ctx, 0x08931784u, {block})});
        call_set(rt, ctx, 0x088D4CC4u, {target, call_set(rt, ctx, 0x0893170Cu, {block})});
        for (u32 index = 0u; index != 4u; ++index) {
            const u32 first = call_set(rt, ctx, 0x08931ADCu, {block, index});
            call_set(rt, ctx, 0x088D4E3Cu, {target, index, first});
            const u32 second = call_set(rt, ctx, 0x08931AFCu, {block, index});
            call_set(rt, ctx, 0x088D59A0u, {target, index, second});
        }
        call_set(rt, ctx, 0x088D4E68u, {target, 0u, call_set(rt, ctx, 0x08931820u, {block})});
        call_set(rt, ctx, 0x088D4EE4u, {target, 0u, call_set(rt, ctx, 0x0893182Cu, {block})});
        call_set(rt, ctx, 0x088D4F48u, {target, 0u, call_set(rt, ctx, 0x08931838u, {block})});
        call_set(rt, ctx, 0x088D4E68u, {target, 1u, call_set(rt, ctx, 0x0893185Cu, {block})});
        call_set(rt, ctx, 0x088D4EE4u, {target, 1u, call_set(rt, ctx, 0x08931844u, {block})});
        call_set(rt, ctx, 0x088D4F48u, {target, 1u, call_set(rt, ctx, 0x08931850u, {block})});
        call_set(rt, ctx, 0x08A5B9ECu, {holder});
    };

    u32 node = word(rt, object);
    if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
    if (call_set(rt, ctx, 0x08A76F20u, {node, call_set(rt, ctx, 0x08838D64u, {})}) != 0u) {
        const u32 local = frame.at(8u);
        call_set(rt, ctx, 0x08838FBCu, {local, object});
        node = word(rt, local);
        if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CFDCu});
        const u32 block = call_set(rt, ctx, 0x08838DC4u, {node});
        call_set(rt, ctx, 0x08A5B9ECu, {local});
        fill_block(block);
        return;
    }
    node = word(rt, object);
    if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
    if (call_set(rt, ctx, 0x08A76F20u, {node, call_set(rt, ctx, 0x08A6BF34u, {})}) != 0u) {
        // The virtual at vtable + 0x0C writes into frame + 0x10; the value at +0x0C of `object` is refcounted down.
        const u32 slot = frame.at(0x10u);
        node = word(rt, object);
        if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
        const u32 vtable = word(rt, node);
        (void)call_indirect(rt, ctx, word(rt, vtable + 0xCu), {slot, node});
        u32 block_source = word(rt, holder);
        if (block_source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CFDCu});
        call_set(rt, ctx, 0x08838E28u, {block_source, word(rt, slot)});
        const u32 pointer = word(rt, slot);
        if (pointer - 0xCu != 0x08D74FD0u &&
            static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) <= 0)
            call_set(rt, ctx, 0x08B8542Cu, {pointer - 0xCu, frame.at(0u)});
        block_source = word(rt, holder);
        if (block_source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CFDCu});
        fill_block(call_set(rt, ctx, 0x08838DC4u, {block_source}));
        return;
    }
    // The third variant: the block is the object at frame + 0x0C itself.
    const u32 block = word(rt, holder);
    if (block == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CFDCu});
    node = word(rt, object);
    if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
    const u32 vtable = word(rt, node);
    const u32 result = call_indirect(rt, ctx, word(rt, vtable + 0x50u), {node});
    call_set(rt, ctx, 0x08838E68u, {block, 0u, result, vtable});
    const u32 after = word(rt, holder);
    if (after == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CFDCu});
    fill_block(call_set(rt, ctx, 0x08838DC4u, {after}));
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08BAB0A0 (self = a0, position = a1, count = a2, value = a3): the vector's insert of `count` 0xAC-byte elements at
// `position` (start/finish/capacity at +0/+4/+8). Room after `finish`: the value is copied into a frame temporary, its
// 0xA4 word replaced by 0x08CC3528, then the elements move; no room: the buffer is rebuilt from the three ranges.
// v0 = 0x08CC1AA8 (the temporary's last static) after either insert, or the entry v0 when count is 0.

// The `sra 2` of a byte distance: the distance in words.
u32 element_words(u32 byte_distance) { return static_cast<u32>(static_cast<s32>(byte_distance) >> 2); }
// GCC's division by 0xAC: `mult`/`mflo` by 0x2FA0BE83 (the modular inverse of 43), the low 32 bits of the product,
// exactly as the original computes it.
u32 element_index(u32 byte_distance) { return element_words(byte_distance) * 0x2FA0BE83u; }
void func_08BAB0A0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x100u);
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    if (count == 0u) return;
    const u32 finish = word(rt, self + 4u);
    const u32 capacity = word(rt, self + 8u);
    if ((element_index(capacity - finish) < count)) {
        // No room after `finish`: rebuild the vector with room for max(existing, count) more elements.
        const u32 begin = word(rt, self);
        const u32 existing = element_index(finish - begin);
        put(rt, frame.at(8u), existing);
        if (0x017D05F4u - existing < count) call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0758u});
        const u32 wanted = existing + (existing < count ? count : existing);
        u32 new_bytes = 0u;
        u32 new_begin = 0u;
        if (wanted != 0u) {
            new_bytes = wanted < existing ? 0xFFFFFFF0u : wanted * 0xACu;
            new_begin = call_set(rt, ctx, 0x08B35DA0u, {new_bytes});
        }
        const u32 source = word(rt, self);
        const u32 middle = call_set(rt, ctx, 0x08BAAFC8u, {source, position, new_begin, frame.at(3u)});
        call_set(rt, ctx, 0x08BAAEF0u, {middle, count, value, frame.at(2u)});
        // The third range's destination is the first copy's end plus the inserted run (s0 still holds the first result).
        const u32 third = call_set(rt, ctx, 0x08BAAFC8u, {position, finish, middle + count * 0xACu, frame.at(1u)});
        call_set(rt, ctx, 0x08BAA4E0u, {word(rt, self), word(rt, self + 4u), frame.at(0u)});
        const u32 end_value = new_begin + new_bytes;
        if (word(rt, self) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, self)});
        put(rt, self + 8u, end_value);
        put(rt, self + 4u, third);
        put(rt, self, new_begin);
        ctx.gpr[2] = end_value;
        return;
    }
    // Room enough: the temporary element is a copy of `value` whose +0xA4 word is the empty static.
    const u32 temporary = frame.at(0xCu);
    for (u32 offset = 0u; offset != 0xA0u; offset += 0x10u) {
        for (u32 index = 0u; index != 4u; ++index)
            put(rt, temporary + offset + 4u * index, word(rt, value + offset + 4u * index));
    }
    put(rt, temporary + 0xA0u, word(rt, value + 0xA0u));
    const u32 tail_words = element_words(finish - position);  // s5, the tail in words
    const u32 after = element_index(finish - position);
    put(rt, frame.at(0xB0u), 0x08CC3528u);
    put(rt, temporary + 0xA8u, word(rt, value + 0xA8u));
    const u32 old_finish = finish;
    if (count >= after) {
        // The tail is shorter than the inserted run: fill past the end, then move the tail after it.
        call_set(rt, ctx, 0x08BAACA0u, {finish, count - after, temporary, frame.at(5u)});
        const u32 middle = finish + (count - after) * 0xACu;
        call_set(rt, ctx, 0x08BAABC8u, {position, old_finish, middle, frame.at(4u)});
        const u32 new_finish = word(rt, self + 4u) + tail_words * 4u;
        put(rt, self + 4u, new_finish);
        call_set(rt, ctx, 0x08BAA70Cu, {position, old_finish, temporary});
    } else {
        // The tail covers the inserted run: move it forward first, then fill the gap.
        call_set(rt, ctx, 0x08BAAAE0u, {old_finish - count * 0xACu, old_finish, old_finish, frame.at(6u)});
        const u32 split = old_finish - count * 0xACu;
        const u32 moved = element_index(split - position);
        put(rt, self + 4u, word(rt, self + 4u) + count * 0xACu);
        // Backward copy of the elements before the split, one whole element per step.
        u32 source = split - 0xACu;
        u32 destination = old_finish - 0xACu;
        u32 remaining = moved;
        while (remaining != 0u) {
            for (u32 offset = 0u; offset != 0xA0u; offset += 0x10u) {
                for (u32 index = 0u; index != 4u; ++index)
                    put(rt, destination + offset + 4u * index, word(rt, source + offset + 4u * index));
            }
            put(rt, destination + 0xA0u, word(rt, source + 0xA0u));
            put(rt, destination + 0xA8u, word(rt, source + 0xA8u));
            source -= 0xACu;
            destination -= 0xACu;
            --remaining;
        }
        call_set(rt, ctx, 0x08BAA70Cu, {position, position + count * 0xACu, temporary});
    }
    put(rt, frame.at(0xB0u), 0x08CC1AA8u);
    ctx.gpr[2] = 0x08CC1AA8u;
}

} // namespace
} // namespace new_batch_main_35
} // namespace gt::game

#define NEWMAIN35(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_35::name, gt::ReturnKind::kind)
NEWMAIN35(0x0884865Cu, func_0884865C, Word);
NEWMAIN35(0x08ADA094u, func_08ADA094, Word);
NEWMAIN35(0x08BAB0A0u, func_08BAB0A0, Word);