// Hot batch 104: reached functions proven by compared cases that change memory (see FINDINGS "Batch 104"); role words describe access shapes, none is a game name.
//
// Written 22 new functions (19 registered, func_08B58B10, func_08AC800C and func_0896275C not: compared 0, kernel isolation, no case changed memory; each has a commented HOT104 line with its reason) and re-registers 18 functions parked in earlier batches whose reason no longer holds (listed in the PR with the old reason and the evidence; their code is unchanged): the sceSasCore helpers func_08B83C2C / func_08B83DE8 / func_08B83E24 / func_08B83F40 / func_08B83FA4 / func_08B83FF4 / func_08B8421C (every case used to stop at a sceSasCore import), func_088A964C, func_08927440, func_08AEFC14, func_08AEFC74, func_08BDADAC, func_08AD95A0 (compared 0 or kernel imports then), func_08B34DC0 (sceHprm), func_08A4D2F8 (a sceFont constant), func_08AF4F90, func_08B52D58 and func_08B58E28 (sceDisplaySetFrameBuf). The imports: host/import_lists.hpp (sceDisplaySetFrameBuf recordable with mask 0x0F, sceHprmIsHeadphoneExist and one sceFont constant replay-safe).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value. The callee-saved registers are never written here. The delay slot of a `jal` / `jalr` runs BEFORE the call, a likely branch's delay slot only when taken, and the delay slot of a non-likely branch on both paths; v0 is set as the original leaves it on every path; the dead stack is not reproduced, the frames that callees read or write are (`GuestFrame`, offsets as in the original).
//
// Branches the compared cases of the fast gate do not take (tools.decomp_gate coverage, then each target read in the asm by hand, delay slots included):
//  - func_08BE3F0C: the allocation failing (null node: v0 = 0) and a list with no head node (the old head's back link is skipped).
//  - func_08BE3C80: every arm after the first node fits: nodes too small, nodes that fit only without the padding, the padded split (func_08BE3BD0 with a pad, and its failure), the node with a previous link, and the empty list.
//  - func_08BE3DA8: the merge arms: a next neighbour that is free (its [0] equals list + 4) and the previous neighbour that is free, with their unlink and delete calls; the compared cases only unlink a node with no free neighbours and push it on the chain.
//  - func_08A7E6E0: the hit whose entry key equals key[0] (the payload refresh, with and without value being the payload itself); the compared cases all insert.
//  - func_089593DC: an index at or above the count or below -1 (treated as -1), an empty pool (count <= 0: early return) and the single-record form.
//  - func_08B79F58: a slot whose key already equals the stored one (answer -1) and the chain walk to a second cell (the new 0x104-byte cell); the compared cases find the first slot empty.
//  - func_08BBD3B4: the answer equal to the word at this + 0x68 (the func_08B34CA0 wake-up).
//  - func_0887B2EC / func_0887AF08: a null record block (no free), and the granted guard (the static is already built in every compared case).
//  - func_08A30A18: the first-call construction of the singleton (D_08D77154 is set in every compared case) and the null counted pointer report.
//  - func_08AF188C: every reference-count arm that reaches zero (virtual slots 8 and 0xC never ran), a null shared pointer, a record slot that already held a pointer; the object's virtual slot 0xC answers 2 in every compared case, so the [0x2A0] and [0x2A4] ORs never ran and the [0x2A8] one always did.
//  - func_08B62344: the handler loop and the throw (malloc succeeds in every compared case), and a size of 0.
//  - func_08B1A3F4: every arm with a node (the drain loop saw an empty queue in every compared case), so the pointer release and the node frees never ran.
//  - func_08A52AB8: a negative count (the whole counted-object arm), the pointers differing, and a null first pointer.
//  - func_08AFFCF4: an empty table (early return), a block with no entries and the loop over several blocks.
//  - func_08B83CE8: a second start (init word already set), a failing init and the whole voice loop body (the init word is zero all through the loop).
//  - func_08B564A8 / func_08B568DC / func_08BF3EA4: no untaken branch (straight-line).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_104 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// The value `lui` leaves for an address built by a relocated lui/addiu (or lui/load/store) pair: the high half, rounded up when the low half is negative.
inline constexpr std::uint32_t lui_of(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call the way the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0. A callee that returns without setting v0 hands the function's address on to the caller's code, so both registers are set like the original sets them.
inline GuestResult call_virtual_full(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                     std::initializer_list<std::uint32_t> arguments) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline std::uint32_t call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                  std::initializer_list<std::uint32_t> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}




// The reference-count steps of the shared-object holder: `ll/sc` on a word of guest memory (single-threaded here, the host never preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful `sc`); true when the old value was 1 (the count reached zero).
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// One shared object of func_08B8FE44's elements: its use count (+4) goes down; when that was the last use the virtual slot 8 runs, then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (count_down(rt, ctx, object + 4u)) {
        (void)call_virtual_full(rt, ctx, object, 8u, {object});
        if (count_down(rt, ctx, object + 8u)) (void)call_virtual_full(rt, ctx, object, 0xCu, {object});
    }
}

// The end of a destructor that releases its last shared object with a tail jump: `jr t9` to the object's virtual slot 0xC with a0 = the object (v0 = its vtable, t9 = the slot's function), after the frame is gone.
inline void tail_virtual_slot_c(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object});
}

// ---- functions ----


// func_08BE3F0C(list = a0, x = a1, y = a2): a 0x1C-byte node from func_08B62344; null answers 0. Otherwise node[5] = x, node[6] = y, node[3] = node[4] = 0 and, under the lock list + 0xC (func_08B344E0 / func_08B3457C): node[0] = list + 4, node[1] = 0, node[2] = list[1] (the old head), the old head's [1] = node (when not null) and list[1] = node. Answers 1.
// Branches the routes do not take: see the list at the top of the file.
void func_08BE3F0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5], y = ctx.gpr[6];
    const std::uint32_t lock = self + 0xCu;
    const std::uint32_t node = call_set(rt, ctx, 0x08B62344u, {0x1Cu});
    ctx.gpr[4] = lock;
    ctx.gpr[2] = 0u;
    if (node == 0u) return;
    put(rt, node + 0x14u, x);
    put(rt, node + 0x18u, y);
    put(rt, node + 0xCu, 0u);
    put(rt, node + 0x10u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    put(rt, node, self + 4u);
    put(rt, node + 4u, 0u);
    put(rt, node + 8u, word(rt, self + 4u));
    const std::uint32_t head = word(rt, self + 4u);
    ctx.gpr[2] = head;
    ctx.gpr[4] = lock;
    if (head != 0u) put(rt, head + 4u, node);
    put(rt, self + 4u, node);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[2] = 1u;
}

// func_08BE3C80(list = a0, x = a1, align = a2): the first node of the list list[1] (next at +8) whose limit node[6] is at least size = (x + align - 1) & ~(align - 1) and which, after its start node[5] is padded up to the alignment (pad = ((align - 1 + node[5]) & ~(align - 1)) - node[5]), still holds size + pad. With a pad, func_08BE3BD0(list, node, pad) splits it (null answers 0); when the limit is not exactly size, func_08BE3BD0(list, node, size) cuts it down (its answer becomes the node, null answers 0). The node is then unlinked from its neighbours (node[1] = previous, node[0] = the list head slot), reset (node[0] = list + 8, node[1] = 0, node[2] = list[2]) and pushed on the list[2] chain. Answers the node, or 0.
void func_08BE3C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t mask_low = ctx.gpr[6] - 1u;  // a3
    const std::uint32_t mask = ~mask_low;            // t0
    ctx.gpr[7] = mask_low;
    ctx.gpr[8] = mask;
    ctx.gpr[5] = mask_low + ctx.gpr[5];
    const std::uint32_t size = ctx.gpr[5] & mask;  // s1
    const std::uint32_t chain = self + 8u;         // s3
    std::uint32_t node = word(rt, self + 4u);
    while (node != 0u) {
        const std::uint32_t limit = word(rt, node + 0x18u);
        if (limit < size) {
            node = word(rt, node + 8u);
            continue;
        }
        const std::uint32_t start = word(rt, node + 0x14u);
        const std::uint32_t pad = ((mask_low + start) & mask) - start;
        if (limit < size + pad) {
            node = word(rt, node + 8u);
            continue;
        }
        if (pad != 0u) {
            ctx.gpr[7] = mask_low;
            if (call_set(rt, ctx, 0x08BE3BD0u, {self, node, pad}) == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
        const std::uint32_t current_limit = word(rt, node + 0x18u);
        if (size != current_limit) {
            node = call_set(rt, ctx, 0x08BE3BD0u, {self, node, size});
            if (node == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
        const std::uint32_t previous = word(rt, node + 4u);
        if (previous != 0u) {
            put(rt, previous + 8u, word(rt, node + 8u));
        } else {
            put(rt, word(rt, node), word(rt, node + 8u));
        }
        const std::uint32_t next = word(rt, node + 8u);
        if (next != 0u) put(rt, next + 4u, word(rt, node + 4u));
        put(rt, node, chain);
        put(rt, node + 4u, 0u);
        put(rt, node + 8u, word(rt, self + 8u));
        const std::uint32_t chain_head = word(rt, self + 8u);
        if (chain_head != 0u) put(rt, chain_head + 4u, node);
        put(rt, self + 8u, node);
        ctx.gpr[2] = node;
        return;
    }
    ctx.gpr[2] = 0u;
}

// func_08BE3DA8(list = a0, node = a1): unlinks node from its neighbours (node[1] = previous, node[2] = next, the head slot node[0] when it has no previous), clears node[0] and merges it into the free neighbours: the following one node[3] (when its [0] equals list + 4) takes node[4] and the summed limit +0x18 and node is released with func_08B64654, then the preceding one node[4] (when its [0] equals list + 4) takes the summed limit and the start +0x14, node[3], and node is unlinked from the chain at list[1] and released again. A node whose [0] is not list + 4 at the end is pushed on the list[1] chain (node[0] = list + 4, node[1] = 0, node[2] = old head). v0 is the last value the code loaded: list + 4 when it returned early, otherwise the old head.
void func_08BE3DA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    const std::uint32_t chain = self + 4u;  // s2
    {
        const std::uint32_t previous = word(rt, node + 4u);
        const std::uint32_t next = word(rt, node + 8u);
        if (previous != 0u) {
            put(rt, previous + 8u, next);
        } else {
            put(rt, word(rt, node), next);
        }
    }
    {
        const std::uint32_t next = word(rt, node + 8u);
        if (next != 0u) put(rt, next + 4u, word(rt, node + 4u));
    }
    std::uint32_t after = word(rt, node + 0xCu);   // s0
    put(rt, node, 0u);
    std::uint32_t before = word(rt, node + 0x10u);  // s1
    if (after != 0u && word(rt, after) == chain) {
        const std::uint32_t sum = word(rt, after + 0x18u) + word(rt, node + 0x18u);
        put(rt, after + 0x10u, before);
        put(rt, after + 0x18u, sum);
        if (before != 0u) put(rt, before + 0xCu, after);
        ctx.gpr[4] = node;
        (void)call_set(rt, ctx, 0x08B64654u, {node});
        node = after;
        ctx.gpr[5] = node;
        after = word(rt, after + 0xCu);
    }
    std::uint32_t loaded;
    if (before != 0u && word(rt, before) == chain) {
        const std::uint32_t sum = word(rt, before + 0x18u) + word(rt, node + 0x18u);
        const std::uint32_t start = word(rt, node + 0x14u);
        put(rt, before + 0xCu, after);
        put(rt, before + 0x14u, start);
        put(rt, before + 0x18u, sum);
        if (after != 0u) put(rt, after + 0x10u, before);
        if (word(rt, node) == chain) {
            const std::uint32_t previous = word(rt, node + 4u);
            const std::uint32_t next = word(rt, node + 8u);
            if (previous != 0u) {
                put(rt, previous + 8u, next);
            } else {
                put(rt, self + 4u, next);
            }
            const std::uint32_t following = word(rt, node + 8u);
            if (following != 0u) put(rt, following + 4u, word(rt, node + 4u));
            put(rt, node, 0u);
        }
        (void)call_set(rt, ctx, 0x08B64654u, {node});
        node = before;
        ctx.gpr[5] = node;
    }
    loaded = word(rt, node);
    ctx.gpr[2] = loaded;
    if (loaded == chain) return;
    put(rt, node, chain);
    put(rt, node + 4u, 0u);
    put(rt, node + 8u, word(rt, self + 4u));
    const std::uint32_t head = word(rt, self + 4u);
    ctx.gpr[2] = head;
    if (head != 0u) put(rt, head + 4u, node);
    put(rt, self + 4u, node);
}

// func_0896275C(this = a0): two counted-pointer slots are emptied under their own locks. Slot A (this[0] = owner, this[1] = counted block): when the owner is not null, func_08B34888 / func_08B34B24 on owner + 0x6C around the two stores this[0] = this[1] = 0, then the block (when not null) is counted down at +4 (at zero its virtual slot 8, then at +8 and at zero slot 0xC). Slot B (this[2] / this[3], lock owner + 0x40) the same, except that its last release is the tail jump to the block's virtual slot 0xC with a0 = the block.
void func_0896275C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t tail_object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t self = ctx.gpr[4];
        std::uint32_t owner = word(rt, self);
        if (owner != 0u) {
            (void)call_set(rt, ctx, 0x08B34888u, {owner + 0x6Cu});
            owner = word(rt, self);
            const std::uint32_t block = word(rt, self + 4u);
            put(rt, self, 0u);
            put(rt, self + 4u, 0u);  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B34B24u, {owner + 0x6Cu});
            if (block != 0u && count_down(rt, ctx, block + 4u)) {
                (void)call_virtual_full(rt, ctx, block, 8u, {block});
                if (count_down(rt, ctx, block + 8u)) (void)call_virtual_full(rt, ctx, block, 0xCu, {block});
            }
        }
        owner = word(rt, self + 8u);
        ctx.gpr[4] = owner;
        if (owner == 0u) return;
        (void)call_set(rt, ctx, 0x08B34888u, {owner + 0x40u});
        owner = word(rt, self + 8u);
        const std::uint32_t block = word(rt, self + 0xCu);
        put(rt, self + 8u, 0u);
        put(rt, self + 0xCu, 0u);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08B34B24u, {owner + 0x40u});
        if (block == 0u) return;
        if (!count_down(rt, ctx, block + 4u)) return;
        (void)call_virtual_full(rt, ctx, block, 8u, {block});
        if (!count_down(rt, ctx, block + 8u)) return;
        tail_object = block;
    }
    tail_virtual_slot_c(rt, ctx, tail_object);
}

// func_08A7E6E0(this = a0, key = a1, value = a2): under the global lock D_08CFF450 (func_08B344E0 / func_08B3457C): the table this + 0x20 is searched for key[0] (func_08BFE7D0; this + 0x24 is the end marker). A hit whose entry word +0x10 equals key[0] has its payload at entry + 0x14 refreshed with func_08A5BAD8(payload, value[0]) unless value is that very payload; otherwise (no hit, or another key) a counted temporary is built on the stack (func_08A6972C(sp, key), func_08A5D424(sp + 4, value)), the entry inserted (func_08BFEBAC(table, hit, sp)) and the temporaries released (func_08A5B9EC(sp + 4), func_08A69D68(sp)). Both ways end with func_08AAD93C(D_08CF705C, this, key, value).
void func_08A7E6E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5], value = ctx.gpr[6];
    constexpr std::uint32_t lock = 0x08CFF450u;
    const std::uint32_t table = self + 0x20u;
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    const std::uint32_t found = call_set(rt, ctx, 0x08BFE7D0u, {table, key});
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = key;
    bool insert = true;
    if (found != self + 0x24u) {
        const std::uint32_t stored = word(rt, found + 0x10u);
        const std::uint32_t wanted = word(rt, key);
        ctx.gpr[6] = found + 0x14u;  // the delay slot of the bne
        if (stored == wanted) {
            insert = false;
            const std::uint32_t payload = found + 0x14u;
            ctx.gpr[4] = payload;
            if (value != payload) (void)call_set(rt, ctx, 0x08A5BAD8u, {payload, word(rt, value)});
        }
    }
    if (insert) {
        (void)call_set(rt, ctx, 0x08A6972Cu, {frame.at(0u), key, ctx.gpr[6]});
        (void)call_set(rt, ctx, 0x08A5D424u, {frame.at(4u), value});
        (void)call_set(rt, ctx, 0x08BFEBACu, {table, found, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    }
    (void)call_set(rt, ctx, 0x08AAD93Cu, {0x08CF705Cu, self, key, value});
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
}


// func_089593DC(this = a0, index = a1): initialises records of 0xD4 bytes of the pool this (record i is func_08959180(this, i); this[2] is the count). An index outside [-1, count) counts as -1; -1 initialises every record 0 .. count - 1 (each cleared with func_08B36018 first), any other index only that record (not cleared). Each record then gets [0x74] = 1, [0x9C] = [0] = [4] = [0x38] = [0x3C] = [0x70] = -1, the bytes [0x75..0x78] = 0 and func_089593B8(record). v0 = 1 after a loop, otherwise the compare result the early exit leaves (-1 for an index below the count, 0 for one at or above it).
void func_089593DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t self = ctx.gpr[4];
    const std::int32_t requested = signed_of(ctx.gpr[5]);
    const std::int32_t count = signed_of(word(rt, self + 8u));
    const bool below_count = requested < count;
    ctx.gpr[2] = below_count ? 0xFFFFFFFFu : 0u;
    std::int32_t only = -1;  // s3
    std::int32_t first = 0, last = 0;  // s2, s4
    if (below_count && requested >= -1) {
        only = requested;
        first = requested;
        last = requested;
    }
    if (only == -1) {
        last = count - 1;
        if (last < 0) return;
        first = 0;
    }
    for (;;) {
        const std::uint32_t record = call_set(rt, ctx, 0x08959180u, {self, static_cast<std::uint32_t>(first)});
        ++first;
        if (only == -1) (void)call_set(rt, ctx, 0x08B36018u, {record, 0u, 0xD4u});
        put8(rt, record + 0x74u, 1u);
        put(rt, record + 0x9Cu, 0xFFFFFFFFu);
        put(rt, record, 0xFFFFFFFFu);
        put8(rt, record + 0x76u, 0u);
        put(rt, record + 4u, 0xFFFFFFFFu);
        put(rt, record + 0x38u, 0xFFFFFFFFu);
        put(rt, record + 0x3Cu, 0xFFFFFFFFu);
        put(rt, record + 0x70u, 0xFFFFFFFFu);
        put8(rt, record + 0x75u, 0u);
        put8(rt, record + 0x77u, 0u);
        put8(rt, record + 0x78u, 0u);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x089593B8u, {record});
        ctx.gpr[2] = last < first ? 1u : 0u;
        if (last < first) break;
    }
}

// func_08B79F58(key = a0, value = a1): a table of 32 slots per cell (cell[0] = next cell, value at cell + 4 + 4 * (key % 32), key at cell + 0x84 + 4 * (key % 32)), the first cell at D_08D72D1C, filled under sceKernelCpuSuspendIntr: value[0x37C] = key; the first cell whose slot is empty takes (value, key) and answers 0, a cell whose slot already holds the key answers -1 (nothing changes), and when the chain ends a new 0x104-byte cell (func_08B35DAC(D_08CE0E60, 0x104), cleared with func_08B36018) takes the pair and is linked at the last cell's [0] under a second suspend / resume.
void func_08B79F58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t key = ctx.gpr[4], value = ctx.gpr[5];
    const std::uint32_t mask = call_set(rt, ctx, 0x08C7A888u, {});
    const std::uint32_t sign = static_cast<std::uint32_t>(signed_of(key) >> 31) >> 27;
    const std::uint32_t slot = ((key + sign) & 0x1Fu) - sign;
    const std::uint32_t offset = slot << 2;  // s2
    std::uint32_t cell = 0x08D72D1Cu;
    put(rt, value + 0x37Cu, key);
    for (;;) {
        const std::uint32_t entry = cell + offset;
        const std::uint32_t stored = word(rt, entry + 0x84u);
        if (stored == 0u) {
            put(rt, entry + 4u, value);
            put(rt, entry + 0x84u, key);
            (void)call_set(rt, ctx, 0x08C7A8A8u, {mask});
            ctx.gpr[2] = 0u;
            return;
        }
        if (key == stored) {
            (void)call_set(rt, ctx, 0x08C7A8A8u, {mask});
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        const std::uint32_t next = word(rt, cell);
        if (next == 0u) break;
        cell = next;
    }
    (void)call_set(rt, ctx, 0x08C7A8A8u, {mask});
    const std::uint32_t fresh = call_set(rt, ctx, 0x08B35DACu, {word(rt, 0x08CE0E60u), 0x104u});
    (void)call_set(rt, ctx, 0x08B36018u, {fresh, 0u, 0x104u});
    put(rt, fresh + offset + 4u, value);
    put(rt, fresh + offset + 0x84u, key);
    const std::uint32_t second = call_set(rt, ctx, 0x08C7A888u, {});
    put(rt, cell, fresh);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08C7A8A8u, {second});
    ctx.gpr[2] = 0u;
}


// func_08BBD3B4(this = a0, value = a1): under the lock this + 0x6C (func_08B34888): func_08B2CE7C(this + 8, value); when func_08B2D1A8(this + 8) answers the word at this + 0x68 the lock's wake-up func_08B34CA0(lock) runs; the tail jump func_08B34B24(lock) releases the lock. v0 is the release's.
void func_08BBD3B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t lock;
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t self = ctx.gpr[4], value = ctx.gpr[5];
        lock = self + 0x6Cu;
        (void)call_set(rt, ctx, 0x08B34888u, {lock});
        (void)call_set(rt, ctx, 0x08B2CE7Cu, {self + 8u, value});
        const std::uint32_t answer = call_set(rt, ctx, 0x08B2D1A8u, {self + 8u});
        ctx.gpr[4] = lock;
        if (answer == word(rt, self + 0x68u)) (void)call_set(rt, ctx, 0x08B34CA0u, {lock});
    }
    tail_call(ctx, 0x08B34B24u, {lock});
}

// func_0887B2EC(this = a0): func_0887B0DC(this), then the 24 records of 0x38 bytes that end at this + 0x56C are destroyed from the last to the first (the array starts at this + 0x2C): for record r, func_08B8ED98(r + 0x1C), func_08B8FE44(r[4], r[5], sp), the block r[4] (when not null) freed with func_08B35254 and func_08B8ED98(r). v0 is the last call's.
void func_0887B2EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t self = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x0887B0DCu, {self});
    const std::uint32_t begin = self + 0x2Cu;
    std::uint32_t record = self + 0x56Cu;
    if (begin == record) return;
    for (std::uint32_t remaining = 0x18u; remaining != 0u; --remaining) {
        record -= 0x38u;
        (void)call_set(rt, ctx, 0x08B8ED98u, {record + 0x1Cu});
        (void)call_set(rt, ctx, 0x08B8FE44u, {word(rt, record + 0x10u), word(rt, record + 0x14u), frame.at(0u)});
        const std::uint32_t block = word(rt, record + 0x10u);
        if (block != 0u) (void)call_set(rt, ctx, 0x08B35254u, {block});
        (void)call_set(rt, ctx, 0x08B8ED98u, {record});
    }
}

// func_0887AF08(this = a0): this[0] = the vtable 0x08CBFDA8; this[1] = a new 0x56C-byte object (func_08B62344, initialised by func_0887AE54); this[2] = the function-local static D_08D753F0, which the first call builds under the guard D_08D753E8 (func_08B6441C acquires it: when granted the static word becomes 0x08CBFE28 and func_08B64494 releases the guard).
void func_0887AF08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CBFDA8u);
    const std::uint32_t object = call_set(rt, ctx, 0x08B62344u, {0x56Cu});
    (void)call_set(rt, ctx, 0x0887AE54u, {object});
    put(rt, self + 4u, object);
    constexpr std::uint32_t guard = 0x08D753E8u, instance = 0x08D753F0u;
    ctx.gpr[2] = lui_of(guard);  // the relocated lui that builds the guard's address leaves v1 = v0 = 0x08D70000
    ctx.gpr[3] = lui_of(guard);
    if (sext8(byte_u(rt, guard)) == 0u) {
        const std::uint32_t granted = call_set(rt, ctx, 0x08B6441Cu, {guard});
        if (granted != 0u) {
            ctx.gpr[2] = 0x08CBFE28u;
            put(rt, instance, 0x08CBFE28u);
            (void)call_set(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, self + 8u, instance);
}

// func_08A30A18(out = a0): the singleton D_08D77154 is built on the first call (func_08A5BC20(0x14, 4) -> func_08A2DED0 initialises it, the word is stored before func_08A5BA28 runs, func_08A9DD14(sp) fills a counted pointer that must not be null (func_08A5BB7C reports otherwise) and func_08A9BACC(it, 0x08C1A988) takes it; func_08A5B9EC(sp) releases the pointer). Every call ends with func_08A76AB4(out, sp + 4) after sp[4] = D_08D77154. v0 is that call's.
void func_08A30A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t out = ctx.gpr[4];
    constexpr std::uint32_t singleton = 0x08D77154u;
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = 0x14u;
    if (word(rt, singleton) == 0u) {
        const std::uint32_t created = call_set(rt, ctx, 0x08A5BC20u, {0x14u, 4u});
        (void)call_set(rt, ctx, 0x08A2DED0u, {created});
        put(rt, singleton, created);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A5BA28u, {created});
        (void)call_set(rt, ctx, 0x08A9DD14u, {frame.at(0u)});
        const std::uint32_t pointer = word(rt, frame.at(0u));
        ctx.gpr[4] = 0x08CAC388u;
        if (pointer == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC388u});
        (void)call_set(rt, ctx, 0x08A9BACCu, {pointer, 0x08C1A988u});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    }
    const std::uint32_t current = word(rt, singleton);
    put(rt, frame.at(4u), current);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AB4u, {out, frame.at(4u)});
}


// A counted-pointer reference taken: `ll/sc` on the word at address (single-threaded here), leaving v1 = the address and v0 = 1 (the successful sc).
inline void count_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
    put(rt, address, old + 1u);
}
// The same decrement with the old value kept in v1 (the register allocation func_08AF188C uses for its temporary): v0 = 1; true when the old value was 1.
inline bool count_down_v1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// func_08AF188C(this = a0, holder = a1, mask = a2, value = a3, flag_a = t0, flag_b = t1): the next 20-byte record of the table this[1] + 0x18 (index = the counter at this[1] + 0x298, incremented) is filled: record[0] = holder[0], record[1] = the counted pointer holder[1] (its count at +4 goes up, the pointer previously in the slot is released: count at +4 down, at zero its virtual slot 8, then the weak count at +8 and at zero slot 0xC; nothing happens when the pointer is the same), record[2] = mask, record[3] = value, the bytes record[0x10] = flag_a & 0xFF and record[0x11] = flag_b & 0xFF. The holder's object (holder[0], virtual slot 0xC) is then asked three times: this[1][0x29C] |= mask always, [0x2A0] |= mask unless the answer is above 0, [0x2A4] unless it is at least 2, [0x2A8] unless it is at least 3. v0 = 0 when the third answer is 3 or more, otherwise the new [0x2A8].
// Branches the routes do not take: see the list at the top of the file.
void func_08AF188C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t self = ctx.gpr[4], holder = ctx.gpr[5], mask = ctx.gpr[6], value = ctx.gpr[7];
    const std::uint32_t flag_a = ctx.gpr[8] & 0xFFu, flag_b = ctx.gpr[9] & 0xFFu;
    ctx.gpr[8] = flag_a;
    ctx.gpr[9] = flag_b;
    const std::uint32_t pool = word(rt, self + 4u);
    const std::uint32_t index = word(rt, pool + 0x298u);
    put(rt, pool + 0x298u, index + 1u);
    const std::uint32_t record = pool + 0x18u + index * 20u;
    const std::uint32_t shared = word(rt, holder + 4u);
    const std::uint32_t object = word(rt, holder);
    if (shared != 0u) count_up(rt, ctx, shared + 4u);
    if (shared != 0u) count_up(rt, ctx, shared + 4u);
    const std::uint32_t previous = word(rt, record + 4u);
    put(rt, record, object);
    const std::uint32_t slot = record + 4u;
    if (shared != previous) {
        if (shared != 0u) count_up(rt, ctx, shared + 4u);
        const std::uint32_t old_pointer = word(rt, slot);
        if (old_pointer != 0u) {
            if (count_down(rt, ctx, old_pointer + 4u)) {
                (void)call_virtual_full(rt, ctx, old_pointer, 8u, {old_pointer});
                if (count_down(rt, ctx, old_pointer + 8u)) (void)call_virtual_full(rt, ctx, old_pointer, 0xCu, {old_pointer});
            }
        }
        put(rt, slot, shared);
    }
    put8(rt, record + 0x11u, flag_b);
    put8(rt, record + 0x10u, flag_a);
    put(rt, record + 0xCu, value);
    put(rt, record + 8u, mask);
    if (shared != 0u) {
        if (count_down_v1(rt, ctx, shared + 4u)) {
            (void)call_virtual_full(rt, ctx, shared, 8u, {shared});
            if (count_down(rt, ctx, shared + 8u)) (void)call_virtual_full(rt, ctx, shared, 0xCu, {shared});
        }
        if (count_down_v1(rt, ctx, shared + 4u)) {
            (void)call_virtual_full(rt, ctx, shared, 8u, {shared});
            if (count_down(rt, ctx, shared + 8u)) (void)call_virtual_full(rt, ctx, shared, 0xCu, {shared});
        }
    }
    std::uint32_t table = word(rt, self + 4u);
    put(rt, table + 0x29Cu, mask | word(rt, table + 0x29Cu));
    std::uint32_t target = word(rt, holder);
    std::uint32_t answer = call_virtual_full(rt, ctx, target, 0xCu, {target}).v0;
    if (signed_of(answer) <= 0) {
        table = word(rt, self + 4u);
        put(rt, table + 0x2A0u, mask | word(rt, table + 0x2A0u));
    }
    target = word(rt, holder);
    answer = call_virtual_full(rt, ctx, target, 0xCu, {target}).v0;
    ctx.gpr[2] = signed_of(answer) < 2 ? 1u : 0u;
    if (signed_of(answer) < 2) {
        table = word(rt, self + 4u);
        put(rt, table + 0x2A4u, mask | word(rt, table + 0x2A4u));
    }
    target = word(rt, holder);
    answer = call_virtual_full(rt, ctx, target, 0xCu, {target}).v0;
    ctx.gpr[2] = signed_of(answer) < 3 ? 1u : 0u;
    if (signed_of(answer) < 3) {
        table = word(rt, self + 4u);
        const std::uint32_t updated = mask | word(rt, table + 0x2A8u);
        put(rt, table + 0x2A8u, updated);
        ctx.gpr[2] = updated;
    }
}


// func_08B62344(size = a0): operator new. A request of 0 asks for 1 byte; the block is func_08B35DA0(size) when that answers non-zero. Otherwise the installed handler D_08D721B4 (when not null) is called without arguments and the allocation is tried again, until one succeeds or no handler is left; then a 4-byte exception object is built (func_08B61E98(4), vtable word 0x08CDBE58) and func_08B62498(object, 0x08CDBE34, 0x08B6240C) throws it (it does not return). v0 is the block.
// Branches the routes do not take: see the list at the top of the file.
void func_08B62344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t size = ctx.gpr[4] != 0u ? ctx.gpr[4] : 1u;
    std::uint32_t block = call_set(rt, ctx, 0x08B35DA0u, {size});
    for (;;) {
        if (block != 0u) return;
        const std::uint32_t handler = word(rt, 0x08D721B4u);
        ctx.gpr[2] = handler;
        if (handler == 0u) break;
        (void)call_set(rt, ctx, handler, {});
        block = call_set(rt, ctx, 0x08B35DA0u, {size});
    }
    const std::uint32_t exception = call_set(rt, ctx, 0x08B61E98u, {4u});
    put(rt, exception, 0x08CDBE58u);
    (void)call_set(rt, ctx, 0x08B62498u, {exception, 0x08CDBE34u, 0x08B6240Cu});
}

// func_08B58B10(mode = a0): the display setter with its arguments chosen by mode (mode & 0xFF): 0 -> sceDisplaySetFrameBuf(0, 0, 0, 1) (the stub 0x08C7AC08 as a tail jump); 1 -> (D_08D6CE80, D_08D6CE98 as a halfword, D_08D6CE84, 1) taken from the graphics library's block at 0x08D6CE00; any other mode returns at once (v0 = 1).
void func_08B58B10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mode = ctx.gpr[4] & 0xFFu;
    ctx.gpr[3] = mode;
    if (mode == 0u) {
        tail_call(ctx, 0x08C7AC08u, {0u, 0u, 0u, 1u});
        return;
    }
    constexpr std::uint32_t block = 0x08D6CE00u;
    ctx.gpr[2] = 1u;
    if (mode != 1u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[5] = block;
        ctx.gpr[6] = 0u;
        ctx.gpr[7] = 1u;
        return;
    }
    const std::uint32_t a2 = word(rt, block + 0x84u), a0 = word(rt, block + 0x80u), a1 = half_u(rt, block + 0x98u);
    tail_call(ctx, 0x08C7AC08u, {a0, a1, a2, 1u});
}

// func_08BF3EA4(out = a0, first = a2, second = a3, third = t0): builds a bound-call node. func_08A69B3C(sp + 8), then a 0x2C-byte object from func_08A5BC20(0x2C, 4) initialised by func_08A98220(object, sp + 8), with vtable 0x08CCE4F8, object[5] = first[0], object[6] = first[1], object[7] = second[0], [8] = second[1], [9] = third[0], [10] = third[1]; func_08A69D68(sp + 8) releases the temporary, the object goes into a counted pointer on the stack (func_08A5D414), func_08A7C5C8(out, pointer) takes it and func_08A5B9EC releases the pointer. (When first[0] is the function 0x08B867AC and first[1] is 0, object[6] is stored as 0: the same value.) v0 is the last release's.
void func_08BF3EA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t out = ctx.gpr[4], first = ctx.gpr[6], second = ctx.gpr[7], third = ctx.gpr[8];
    put(rt, frame.at(0x10u), out);
    const std::uint32_t s2 = word(rt, first), s3 = word(rt, first + 4u);
    const std::uint32_t s7 = word(rt, second), fp = word(rt, third), s5 = word(rt, second + 4u);
    const std::uint32_t s6 = word(rt, third + 4u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(8u)});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    (void)call_set(rt, ctx, 0x08A98220u, {object, frame.at(8u)});
    put(rt, object, 0x08CCE4F8u);
    put(rt, object + 0x18u, s3);
    put(rt, object + 0x14u, s2);
    put(rt, object + 0x20u, s5);
    put(rt, object + 0x1Cu, s7);
    put(rt, object + 0x28u, s6);
    put(rt, object + 0x24u, fp);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(8u)});
    put(rt, frame.at(0u), object);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A5D414u, {frame.at(4u), frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A7C5C8u, {word(rt, frame.at(0x10u)), frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}

// func_08B1A3F4(this = a0): func_08B19F60(this), then every node func_08B1F2B8(this + 0x2C) answers is taken apart: func_08B1A174(node), the counted pointer node[5] (when not null) loses a reference (at +4 and, at zero, virtual slot 8, then +8 and slot 0xC), func_08B34DA0(node + 8) and the node is freed with func_08B64654. At the end func_08B34DA0 runs on this + 0x54, + 0x44 and + 0x34 and the tail jump func_08B28BE0(this) finishes. v0 is the tail's.
void func_08B1A3F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t self;
    {
        GuestFrame frame(ctx, 0x20u);
        self = ctx.gpr[4];
        (void)call_set(rt, ctx, 0x08B19F60u, {self});
        for (;;) {
            const std::uint32_t node = call_set(rt, ctx, 0x08B1F2B8u, {self + 0x2Cu});
            if (node == 0u) break;
            (void)call_set(rt, ctx, 0x08B1A174u, {node});
            const std::uint32_t pointer = word(rt, node + 0x14u);
            if (pointer != 0u && count_down(rt, ctx, pointer + 4u)) {
                (void)call_virtual_full(rt, ctx, pointer, 8u, {pointer});
                if (count_down(rt, ctx, pointer + 8u)) (void)call_virtual_full(rt, ctx, pointer, 0xCu, {pointer});
            }
            (void)call_set(rt, ctx, 0x08B34DA0u, {node + 8u});
            (void)call_set(rt, ctx, 0x08B64654u, {node});
        }
        (void)call_set(rt, ctx, 0x08B34DA0u, {self + 0x54u});
        (void)call_set(rt, ctx, 0x08B34DA0u, {self + 0x44u});
        (void)call_set(rt, ctx, 0x08B34DA0u, {self + 0x34u});
    }
    tail_call(ctx, 0x08B28BE0u, {self});
}

// func_08AC800C(owner = a0, name = a1, flag = a2, f12, f13, f14): a null name answers 0. Under the lock D_08CFFD30 (func_08B344E0 / func_08B3457C): when the pool header word D_08CFFD58 + 0x18 is not null, func_08B2B7D8(D_08CFFD58, 0) hands out a slot and func_08AC71B4(slot) prepares it. A slot is filled: slot[0] = owner, func_08B7C830(slot + 0x34, name, 0x3FF) copies the name, func_08AC75F8(slot) with f12, slot[0x550] = f13 (float), slot[0x564] = flag & 0xFF with func_08AC69F4(slot) and f14, and func_08AC7F94(slot). Answers the slot, or 0 when none was handed out.
void func_08AC800C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t owner = ctx.gpr[4], name = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    const float f20 = ctx.fpr[12], f22 = ctx.fpr[13], f21 = ctx.fpr[14];
    constexpr std::uint32_t lock = 0x08CFFD30u, pool = 0x08CFFD58u;
    if (name == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    std::uint32_t slot = 0u;
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    if (word(rt, pool + 0x18u) != 0u) {
        ctx.gpr[5] = 0u;
        slot = call_set(rt, ctx, 0x08B2B7D8u, {pool, 0u});
        ctx.gpr[4] = slot;
        if (slot != 0u) (void)call_set(rt, ctx, 0x08AC71B4u, {slot});
    }
    ctx.gpr[5] = 0u;
    ctx.gpr[4] = lock;
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[5] = name;
    ctx.gpr[6] = 0x3FFu;
    if (slot == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, slot, owner);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B7C830u, {slot + 0x34u, name, 0x3FFu});
    (void)call_setf(rt, ctx, 0x08AC75F8u, {slot}, {f20});
    put_float(rt, slot + 0x550u, f22);
    put8(rt, slot + 0x564u, flag);  // the delay slot of the jal
    (void)call_setf(rt, ctx, 0x08AC69F4u, {slot}, {f21});
    (void)call_set(rt, ctx, 0x08AC7F94u, {slot});
    ctx.gpr[2] = slot;
}


// func_08A52AB8(result = a0, x = a1, name = a2, count = a3, p = t0, q = t1): func_08A52AA8(sp + 0x14, &x) builds a counted pointer on the stack; with a count >= 0 a 0x44-byte object (func_08A5BC20(0x44, 4)) is set up by func_08A5B70C(object, sp + 0x14, count), put into a second counted pointer (func_08A52AB0(sp + 0xC, &object)) and assigned over the first (func_08A5BAD8 when they differ) before the second is released (func_08A5B9EC). The first pointer must not be null (func_08A5BB7C reports 0x08CAF4A8 otherwise); its virtual slot 0x124 is called, a 0x18-byte object is built by func_08A56018(object, name, 0, p, q), kept in a pointer (func_08A5BAB8(sp + 8, &object)) and func_08A57B54(result, sp + 0x14, sp + 8, 0) fills the result; both pointers are released. v0 = result.
// Branches the routes do not take: see the list at the top of the file.
void func_08A52AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t result = ctx.gpr[4], x = ctx.gpr[5], name = ctx.gpr[6], count = ctx.gpr[7];
    const std::uint32_t p = ctx.gpr[8], q = ctx.gpr[9];
    put(rt, frame.at(0x10u), x);
    const std::uint32_t first = frame.at(0x14u), second_slot = frame.at(0xCu), third = frame.at(8u);
    (void)call_set(rt, ctx, 0x08A52AA8u, {first, frame.at(0x10u)});
    ctx.gpr[4] = 0x44u;
    ctx.gpr[5] = 4u;
    if (signed_of(count) >= 0) {
        const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x44u, 4u});
        (void)call_set(rt, ctx, 0x08A5B70Cu, {object, first, count});
        put(rt, frame.at(4u), object);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A52AB0u, {second_slot, frame.at(4u)});
        if (first != second_slot) (void)call_set(rt, ctx, 0x08A5BAD8u, {first, word(rt, second_slot)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {second_slot});
    }
    const std::uint32_t pointer = word(rt, first);
    ctx.gpr[4] = 0x08CAF4A8u;
    if (pointer == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF4A8u});
    (void)call_virtual_full(rt, ctx, pointer, 0x124u, {pointer});
    const std::uint32_t built = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call_set(rt, ctx, 0x08A56018u, {built, name, 0u, p, q});
    put(rt, frame.at(0u), built);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A5BAB8u, {third, frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A57B54u, {result, first, third, 0u});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {third});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {first});
    ctx.gpr[2] = result;
}

// func_08B568DC(matrix = a0): the graphics library's flag word D_08D6CE6C gets bit 2 and the tail jump func_08B56858(0x08D6D0E0, matrix) follows (v1 = 0x08D6CE00, v0 = the new flags).
void func_08B568DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags) | 2u;
    ctx.gpr[3] = kGuState;
    ctx.gpr[2] = flags;
    const std::uint32_t matrix = ctx.gpr[4];
    put(rt, kGuFlags, flags);
    tail_call(ctx, 0x08B56858u, {kGuCommandBlock, matrix});
}

// func_08B564A8(): the flag word D_08D6CE6C gets bit 2 and the tail jump func_08B56468(0x08D6D0E0) follows (v0 = 0x08D6CE00, v1 = the new flags).
void func_08B564A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flags = word(rt, kGuFlags) | 2u;
    ctx.gpr[2] = kGuState;
    ctx.gpr[3] = flags;
    put(rt, kGuFlags, flags);
    tail_call(ctx, 0x08B56468u, {kGuCommandBlock});
}


// func_08AFFCF4(this = a0): recomputes the bounding box this[0x26..0x2B] of a mesh. The pairs (v0, v1) the vector helpers hand back are spilled and passed on through the stack exactly as the original does (the helpers read them through pointers into the frame): func_08C45F08(this, this + 0x20) and func_08C46184(sp + 0x44, pair) build the transform at sp + 0x44. The minimum corner this + 0x98 is set to 0x7F7FFFFF (FLT_MAX) in its three floats and the maximum corner this + 0xA4 to 0xFF7FFFFF (-FLT_MAX). Then, for each block of 0x200 bytes of the table this[0x36] + 0x80 (this[0x36] + 0x7E halfwords of them; the kind count (word +0x1FC >> 8) & 0xF names how many 16-byte entries the block holds): for each entry, func_08C45D84 / func_08C45F1C build the (transform, entry) pair, func_08C46A5C(sp + 0x34, transform, entry) transforms the entry, its three floats are copied to sp + 0x28, func_08C45E6C(min, sp + 0x28) / func_08C46594(min, pair) fold it into the minimum and func_08C45E58(max, sp + 0x28) / func_08C4653C(max, pair) into the maximum. v0 = the end of the table.
// Branches the routes do not take: see the list at the top of the file.
void func_08AFFCF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xE0u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t minimum = self + 0x98u, maximum = self + 0xA4u;
    ctx.gpr[5] = self + 0x20u;
    GuestResult pair = call_setf(rt, ctx, 0x08C45F08u, {self, self + 0x20u}, {});
    put(rt, frame.at(0x90u), pair.v0);
    put(rt, frame.at(0x94u), pair.v1);
    put(rt, frame.at(0x20u), pair.v0);
    put(rt, frame.at(0x24u), pair.v1);  // the delay slot of the jal
    (void)call_setf(rt, ctx, 0x08C46184u, {frame.at(0x44u), pair.v0, pair.v1}, {});
    put(rt, self + 0x98u, 0x7F7FFFFFu);
    put(rt, minimum + 8u, 0x7F7FFFFFu);
    put(rt, minimum + 4u, 0x7F7FFFFFu);
    put(rt, self + 0xA4u, 0xFF7FFFFFu);
    put(rt, maximum + 8u, 0xFF7FFFFFu);
    put(rt, maximum + 4u, 0xFF7FFFFFu);
    const std::uint32_t pool = word(rt, self + 0xD8u);
    std::uint32_t block = pool + 0x80u;
    const std::uint32_t end = block + (half_u(rt, pool + 0x7Eu) << 9);
    put(rt, frame.at(0xA0u), end);
    ctx.gpr[2] = end;
    if (block == end) return;
    for (;;) {
        const std::uint32_t last = block + (((word(rt, block + 0x1FCu) >> 8) & 0xFu) << 4);
        put(rt, frame.at(4u), block);
        if (last != block) {
            do {
                const std::uint32_t element = call_setf(rt, ctx, 0x08C45D84u, {frame.at(4u)}, {}).v0;
                pair = call_setf(rt, ctx, 0x08C45F1Cu, {frame.at(0x44u), element}, {});
                put(rt, frame.at(0u), frame.at(0x18u));
                const std::uint32_t source = word(rt, frame.at(0u));
                put(rt, frame.at(0x18u), pair.v0);
                put(rt, frame.at(0x1Cu), pair.v1);
                put(rt, frame.at(0x90u), pair.v0);
                put(rt, frame.at(0x94u), pair.v1);
                (void)call_setf(rt, ctx, 0x08C46A5Cu, {frame.at(0x34u), word(rt, source), word(rt, source + 4u)}, {});
                put(rt, frame.at(0x28u), word(rt, frame.at(0x34u)));
                put(rt, frame.at(0x2Cu), word(rt, frame.at(0x38u)));
                put(rt, frame.at(0x30u), word(rt, frame.at(0x3Cu)));  // the delay slot of the jal
                pair = call_setf(rt, ctx, 0x08C45E6Cu, {minimum, frame.at(0x28u)}, {});
                put(rt, frame.at(0x10u), pair.v0);
                put(rt, frame.at(0x14u), pair.v1);
                put(rt, frame.at(0x90u), pair.v0);
                put(rt, frame.at(0x94u), pair.v1);  // the delay slot of the jal
                (void)call_setf(rt, ctx, 0x08C46594u, {minimum, word(rt, frame.at(0x10u)), word(rt, frame.at(0x14u))}, {});
                pair = call_setf(rt, ctx, 0x08C45E58u, {maximum, frame.at(0x28u)}, {});
                put(rt, frame.at(8u), pair.v0);
                put(rt, frame.at(0xCu), pair.v1);
                put(rt, frame.at(0x90u), pair.v0);
                put(rt, frame.at(0x94u), pair.v1);  // the delay slot of the jal
                (void)call_setf(rt, ctx, 0x08C4653Cu, {maximum, word(rt, frame.at(8u)), word(rt, frame.at(0xCu))}, {});
                put(rt, frame.at(4u), word(rt, frame.at(4u)) + 0x10u);
            } while (last != word(rt, frame.at(4u)));
        }
        ctx.gpr[2] = word(rt, frame.at(0xA0u));
        block += 0x200u;
        if (block == word(rt, frame.at(0xA0u))) break;
    }
}


// func_08B83CE8(grain = a0): the sound library's one-time start (the init word D_08D740F8 is 0 before it): the Sas import stub 0x08C7B048(core 0x08D74100, grain, 32, 0, 44100) answers its error code at once when it fails; otherwise for each of the 32 voices, only while the init word is set (it is not until the loop ends, so the three stub calls in the body never run on a first start): 0x08C7B0A8(core, voice, 0x1000), 0x08C7B0C0(core, voice, 0xF, 0x5FC0) behind a second look at the word and 0x08C7B050(core, voice, 0, 0, 0, 0) behind a third; then the word is set to 1 and v0 = 0. A call with the word already set answers 0x80420101.
// Branches the routes do not take: see the list at the top of the file.
void func_08B83CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t grain = ctx.gpr[4];
    constexpr std::uint32_t flag = 0x08D740F8u, core = 0x08D74100u;
    ctx.gpr[5] = grain;
    ctx.gpr[6] = word(rt, flag);
    ctx.gpr[3] = 0x80420101u;
    if (word(rt, flag) != 0u) {
        ctx.gpr[2] = 0x80420101u;
        return;
    }
    const std::uint32_t status = call_set(rt, ctx, 0x08C7B048u, {core, grain, 0x20u, 0u, 0xAC44u});
    ctx.gpr[3] = status;
    if (status != 0u) return;
    for (std::uint32_t voice = 0u; voice < 0x20u;) {
        if (word(rt, flag) != 0u) {
            ctx.gpr[6] = 0x1000u;
            (void)call_set(rt, ctx, 0x08C7B0A8u, {core, voice, 0x1000u});
            if (word(rt, flag) != 0u) {
                (void)call_set(rt, ctx, 0x08C7B0C0u, {core, voice, 0xFu, 0x5FC0u});
                if (word(rt, flag) != 0u) {
                    ++voice;  // the delay slot of the jal
                    (void)call_set(rt, ctx, 0x08C7B050u, {core, voice - 1u, 0u, 0u, 0u, 0u});
                    continue;
                }
            }
        }
        ++voice;
    }
    ctx.gpr[5] = 1u;
    put(rt, flag, 1u);
    ctx.gpr[3] = 0u;
    ctx.gpr[2] = 0u;
}

// ---- end of functions ----
}  // namespace
}  // namespace hot_batch_104
}  // namespace gt::game

#define HOT104(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_104::name, gt::ReturnKind::kind)
#define HOT104N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_104::name, gt::ReturnKind::kind)

// HOT104REGS
HOT104(0x08BE3F0Cu, func_08BE3F0C, Word);
HOT104(0x08BE3C80u, func_08BE3C80, Word);
HOT104(0x08BE3DA8u, func_08BE3DA8, Word);
// HOT104(0x0896275Cu, func_0896275C, Word);  // HOT104: writes memory but no compared case changed it (16 cases, every owner pointer was null)
HOT104(0x08A7E6E0u, func_08A7E6E0, Word);
HOT104(0x089593DCu, func_089593DC, Word);
HOT104(0x08B79F58u, func_08B79F58, Word);
HOT104(0x08BBD3B4u, func_08BBD3B4, Word);
HOT104(0x0887B2ECu, func_0887B2EC, Word);
HOT104(0x0887AF08u, func_0887AF08, Word);
HOT104(0x08A30A18u, func_08A30A18, Word);
HOT104(0x08AF188Cu, func_08AF188C, Word);
HOT104(0x08B62344u, func_08B62344, Word);
// HOT104(0x08B58B10u, func_08B58B10, Word);  // HOT104: compared 0 in the fast gate: no call reached the registration (5 boot-time calls per route in the probe)
HOT104(0x08BF3EA4u, func_08BF3EA4, Word);
HOT104(0x08B1A3F4u, func_08B1A3F4, Word);
// HOT104(0x08AC800Cu, func_08AC800C, Word);  // HOT104: compared 0: all 5 cases not comparable (kernel isolation, LwMutex contention in the pool lock)
HOT104(0x08A52AB8u, func_08A52AB8, Word);
HOT104(0x08B568DCu, func_08B568DC, Word);
HOT104(0x08B564A8u, func_08B564A8, Word);
HOT104(0x08AFFCF4u, func_08AFFCF4, Word);
HOT104(0x08B83CE8u, func_08B83CE8, Word);
