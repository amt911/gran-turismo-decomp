// Main-session batch 999: functions drafted by hand as structured C from `tools.disasm`.
// Draft only (no gate run here): each body keeps the original's v0 on every exit path, every argument register a
// callee reads, the delay slot of a `jal` before the call, and the frame size of the original's prologue.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_999 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The callee-saved registers the body keeps state in: the original saves them in its prologue and restores them from
// the frame before it returns, so the caller sees them unchanged.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 saved[8];
    explicit SavedRegs(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedRegs() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
    }
};

// 0x08C481C8 (a0 = first record, a1 = end, a2 = destination): copies the 0xF0-byte records from a0 up to (not
// including) a1 into a2, stepping both by 0xF0. A null destination still walks the records (nothing is written), and
// v0 = the destination past the last record in both cases.

// The graphics library's pool: `kPool` holds the two block lists and the lock words, `kFreeList` (+0x14) the head of
// the free list and (+0x20) its byte count, and `kGuard` the byte that says the free list was initialised.
inline constexpr u32 kPool = 0x08D75F9Cu;
inline constexpr u32 kFreeList = 0x08D75F5Cu;
inline constexpr u32 kGuard = 0x08D75F50u;
inline constexpr u32 kPoolBlock = 0x08D75FC0u;

// The free list's first use: 08B6441C answers whether the list has to be built, and the thirteen stores below make
// 0x08D75F5C a list whose head is the sentinel at 0x08D75F68. Then the guard 08B64494 and the atexit-like 08B77B1C.
void ensure_free_list(Runtime &rt, AllegrexContext &ctx) {
    if (rt.memory().load8(kGuard) != 0u) return;
    if (call(rt, ctx, 0x08B6441Cu, {kGuard}) == 0u) return;
    put(rt, kFreeList + 0x18u, 0x1001u);
    put(rt, kFreeList + 0x0Cu, 0x08D75F68u);
    put(rt, kFreeList + 0x10u, 0u);
    put(rt, kFreeList + 0x04u, 0u);
    put(rt, kFreeList + 0x08u, 0u);
    put(rt, kFreeList + 0x14u, 0x08D75F68u);
    put(rt, kFreeList + 0x1Cu, 0u);
    put(rt, kFreeList + 0x20u, 0u);
    put(rt, kFreeList + 0x24u, 0u);
    put(rt, kFreeList + 0x00u, 0u);
    call(rt, ctx, 0x08B64494u, {kGuard});
    call(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
}

// Puts the block whose payload starts at `payload` back on the free list: its 8-byte header is `payload - 8`, with the
// next block's header at +0 and this block's size in eight-byte units at +4. The list is walked forward while the next
// header is not above the new one (the unsigned comparisons of the asm, wrap-around included), the byte count at
// kFreeList+0x20 drops by the block's size, and the block is merged with the neighbour on each side when the headers
// are exactly one block apart.
void insert_free_block(Runtime &rt, AllegrexContext &ctx, u32 payload) {
    const u32 block = payload - 8u;
    u32 anchor = word(rt, kFreeList + 0x14u);
    for (;;) {
        const u32 next = word(rt, anchor);
        if (anchor < block) {
            if (block < next) break;
            if (anchor < next) {
                anchor = next;
                continue;
            }
            break;
        }
        if (anchor < next) {
            anchor = next;
            continue;
        }
        if (block < next) break;
        anchor = next;
    }
    put(rt, kFreeList + 0x20u, word(rt, kFreeList + 0x20u) - word(rt, block + 4u));
    const u32 size = word(rt, block + 4u);
    const u32 successor = word(rt, anchor);
    put(rt, block, successor);
    if (block + (size << 3) == successor) {
        const u32 successor_size = word(rt, successor + 4u);
        const u32 successor_next = word(rt, successor);
        put(rt, block + 4u, size + successor_size);
        put(rt, block, successor_next);
    }
    const u32 anchor_size = word(rt, anchor + 4u);
    put(rt, anchor, block);
    if (anchor + (anchor_size << 3) == block) {
        const u32 merged_size = word(rt, block + 4u);
        const u32 merged_next = word(rt, block);
        put(rt, anchor + 4u, anchor_size + merged_size);
        put(rt, anchor, merged_next);
    }
    put(rt, kFreeList + 0x14u, anchor);
}

// 0x088B82DC (no arguments): gives the two pool blocks at 0x08D75FC0 and at 0x08D75F9C+0 back to the free list at
// 0x08D75F5C between the two lock calls 08BB1B20 / 08BB1B30 (the second one clears 0x08D75F9C+0x14 first). v0 = the
// free list's address once a block was inserted, else the second lock call's answer.

// The handle pair 0x08B398CC builds at `out`: `kBasicTable` for the intermediate value, `kDerivedTable` for the result.
// A payload is a reference-counted object: the count at +4 and the vtable at +0, whose slot +4 releases it.
inline constexpr u32 kBasicTable = 0x08CDA8F8u;
inline constexpr u32 kDerivedTable = 0x08CDA908u;

// The copy-and-swap the original repeats three times: the temporary pair at `slot` holds the new payload at +4; the
// value held at `held_at` loses a count (its vtable's +4 slot runs when it reaches zero), the new payload gains one and
// becomes the held value, and 08C50CCC destroys the temporary at `slot`.
void swap_held(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 held_at) {
    const u32 old = word(rt, held_at);
    if (old != 0u) {
        const u32 left = word(rt, old + 4u) - 1u;
        put(rt, old + 4u, left);
        if (left == 0u) {
            const u32 table = word(rt, old);
            const u32 release = word(rt, table + 4u);
            ctx.gpr[3] = table;
            call_indirect(rt, ctx, release, {old});
        }
    }
    const u32 fresh = word(rt, slot + 4u);
    if (fresh != 0u) {
        put(rt, held_at, fresh);
        put(rt, fresh + 4u, word(rt, fresh + 4u) + 1u);
    }
    call(rt, ctx, 0x08C50CCCu, {slot});
}

// 0x08B398CC (a0 = out, a1 = source, a2 = state holder): builds the two-word handle at `out` and advances the state.
// 0: a fresh 0x24-byte object from 08B62344, initialised by 08B39484(object, source+0xC) and taken as the held payload.
// 2: 08B1EB40(*source) is kept, 08B39148(source) becomes the new state, 08B396D4 builds a temporary that is taken as
// the held payload; when the state is then 3 and the byte at +0xD of *source is set, a second object is made by
// 08B39548(object, 4, kept, temporary), held and also advances the state; any other state becomes 08B388D4(0x08CB5600)
// and an empty handle. Every path ends by publishing the held payload at `out` under kDerivedTable and destroying the
// spare pair at sp+0x18; v0 = out.
void func_08B398CC(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 state_holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[21] = source;
    ctx.gpr[19] = state_holder;

    const u32 held_at = frame.at(0x1Cu);
    const u32 spare_at = frame.at(0x18u);
    put(rt, held_at, 0u);
    put(rt, spare_at, kDerivedTable);
    const u32 state = word(rt, state_holder);
    if (state == 0u) {
        const u32 created = call(rt, ctx, 0x08B62344u, {0x24u});
        ctx.gpr[16] = created;
        call(rt, ctx, 0x08B39484u, {created, source + 0xCu});
        put(rt, frame.at(0x10u), kBasicTable);
        put(rt, frame.at(0x14u), created);
        if (created != 0u) put(rt, created + 4u, word(rt, created + 4u) + 1u);
        put(rt, frame.at(0x10u), kDerivedTable);
        swap_held(rt, ctx, frame.at(0x10u), held_at);
        put(rt, state_holder, call(rt, ctx, 0x08B39148u, {source}));
    } else if (state == 2u) {
        const u32 kept = call(rt, ctx, 0x08B1EB40u, {word(rt, source)});
        ctx.gpr[20] = kept;
        put(rt, state_holder, call(rt, ctx, 0x08B39148u, {source}));
        call(rt, ctx, 0x08B396D4u, {frame.at(8u), source, state_holder});
        swap_held(rt, ctx, frame.at(8u), held_at);
        if (word(rt, state_holder) == 3u) {
            const u32 object = word(rt, source);
            if (rt.memory().load8(object + 0xDu) != 0u) {
                const u32 created = call(rt, ctx, 0x08B62344u, {0x24u});
                ctx.gpr[16] = created;
                call(rt, ctx, 0x08B39548u, {created, 4u, kept, spare_at});
                put(rt, frame.at(0u), kBasicTable);
                put(rt, frame.at(4u), created);
                if (created != 0u) put(rt, created + 4u, word(rt, created + 4u) + 1u);
                put(rt, frame.at(0u), kDerivedTable);
                swap_held(rt, ctx, frame.at(0u), held_at);
            }
            put(rt, state_holder, call(rt, ctx, 0x08B39148u, {source}));
        } else {
            const u32 next_state = call(rt, ctx, 0x08B388D4u, {0x08CB5600u});
            put(rt, state_holder, next_state);
            put(rt, out, kDerivedTable);
            put(rt, out + 4u, 0u);
            call(rt, ctx, 0x08C50CCCu, {spare_at});
            ctx.gpr[2] = out;
            return;
        }
    }

    const u32 held = word(rt, held_at);
    put(rt, out, kBasicTable);
    put(rt, out + 4u, held);
    if (held != 0u) put(rt, held + 4u, word(rt, held + 4u) + 1u);
    put(rt, out, kDerivedTable);
    call(rt, ctx, 0x08C50CCCu, {spare_at});
    ctx.gpr[2] = out;
}

// 0x08A54104 (a0 = self, a1 = object, a2 = holder): when *holder is null, walks the nodes from self+0x44 up to the
// sentinel self+0x3C (08B5F16C gives the next one) and for each node hands 08A57B54(sp+0xC, member+0xC, object+0x18,
// sum) the sum of the node's height (word(node+0x14)+0x10, asserted non-null) and the object's (word(object+0x1C), or
// word(word(object+0x18)+0xC) when that is null), then 08A56824(temporary, *holder). When *holder is not null it asks
// *holder's vtable slot 0x90 first (a null answer ends the call), builds the iterator pair at sp+0x10 from slot 0xA8,
// and per element (the pair's +0x14, slots +8 "drained" and +0xC "next") fills sp+0x14 / sp+4 / sp+8 through slot
// 0x84 / 0x5C / 0x84 and hands them to 08A565C0(object, sp+8, sp), destroying each temporary. The body ends with
// 08A5B9EC on the pair; every path that ran a callee leaves its answer in v0.
void func_08A54104(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = object;
    ctx.gpr[20] = out;

    if (word(rt, out) == 0u) {
        u32 node = word(rt, self + 0x44u);
        const u32 end = self + 0x3Cu;
        if (node == end) {
            ctx.gpr[2] = 0u;
            return;
        }
        u32 next = 0u;
        for (;;) {
            const u32 holder = word(rt, node + 0x14u);
            if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
            const u32 height = word(rt, holder + 0x10u);
            const u32 direct = word(rt, object + 0x1Cu);
            u32 total;
            if (direct != 0u) {
                total = direct + height;
            } else {
                const u32 owner = word(rt, object + 0x18u);
                if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF540u});
                total = word(rt, owner + 0xCu) + height;
            }
            const u32 again = word(rt, node + 0x14u);
            if (again == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
            call(rt, ctx, 0x08A57B54u, {frame.at(0xCu), again + 0xCu, object + 0x18u, total});
            const u32 built = word(rt, frame.at(0xCu));
            if (built == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF5E0u});
            call(rt, ctx, 0x08A56824u, {built, out});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
            next = call(rt, ctx, 0x08B5F16Cu, {node});
            if (next == end) break;
            node = next;
        }
        ctx.gpr[2] = next;
        return;
    }

    const u32 first = word(rt, out);
    const u32 proceed = call_indirect(rt, ctx, word(rt, word(rt, first) + 0x90u), {first});
    if (proceed == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 start = word(rt, out);
    if (start == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
    call_indirect(rt, ctx, word(rt, word(rt, start) + 0xA8u), {frame.at(0x10u), start});
    for (;;) {
        const u32 current = word(rt, frame.at(0x10u));
        if (current == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF5ECu});
        const u32 element = word(rt, current + 0x14u);
        if (call_indirect(rt, ctx, word(rt, word(rt, element) + 8u), {element}) == 0u) {
            ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
            return;
        }
        call_indirect(rt, ctx, word(rt, word(rt, element) + 0xCu), {element});

        const u32 item = word(rt, frame.at(0x10u));
        if (item == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF5ECu});
        const u32 member = word(rt, item + 0x14u);
        call(rt, ctx, 0x08A76ABCu, {frame.at(0x14u), member + 4u});
        u32 made = word(rt, frame.at(0x14u));
        if (made == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        call_indirect(rt, ctx, word(rt, word(rt, made) + 0x84u), {frame.at(4u), made, 0u});
        made = word(rt, frame.at(4u));
        if (made == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        call_indirect(rt, ctx, word(rt, word(rt, made) + 0x5Cu), {frame.at(8u), made});
        made = word(rt, frame.at(0x14u));
        if (made == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        call_indirect(rt, ctx, word(rt, word(rt, made) + 0x84u), {frame.at(0u), made, 1u});
        call(rt, ctx, 0x08A565C0u, {object, frame.at(8u), frame.at(0u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        call(rt, ctx, 0x08A69D68u, {frame.at(8u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    }
}

} // namespace
} // namespace new_batch_main_999
} // namespace gt::game

#define NEW999(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_999::name, gt::ReturnKind::kind)
NEW999(0x08B398CCu, func_08B398CC, Word);
NEW999(0x08A54104u, func_08A54104, Word);
