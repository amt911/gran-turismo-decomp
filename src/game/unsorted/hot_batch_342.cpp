// Hot batch 342: functions still recompiled (picked with `tools.next_leaves --ready`), written as structured C from
// `tools.disasm` and verified by synthetic cases. Four shapes make most of it: indexed linked-list walks over arrays of
// fixed-size nodes (the next link is a signed 16-bit index), an unrolled search over an array of words (two copies of
// one function), a tree insert that takes a position hint and compares string keys (the same function exists three more
// times with other callees; those copies were never reached by a synthetic case and are left recompiled) and
// static-initialiser functions that register a table of entries (four copies, constants differ); the rest are short
// wrappers around calls.
// The callee-saved registers are set where the asm sets them, because a callee may read them, and put back at the end
// (the original saves and restores them). `v0` ends as the asm leaves it: the last call's result or a loaded value.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_342 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal` (a callee that never writes v0 returns what
// v0 held when it was called, so the caller's v0 has to stay current).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void set_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// The callee-saved registers s0-s7: the original saves and restores them, so the body may set them (a callee reads them
// through the context) and they come back as they were.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
};

// `div` followed by `mfhi`: the remainder, toward zero; INT_MIN / -1 leaves 0. The caller handles a zero divisor (the
// original's `break 7`).
u32 signed_remainder(u32 dividend, u32 divisor) {
    if (divisor == 0xFFFFFFFFu) return 0u;
    return static_cast<u32>(static_cast<s32>(dividend) % static_cast<s32>(divisor));
}
// The original's trap on a zero divisor (`break 7`); the recompiler stops the runtime there.
void divide_by_zero(Runtime &rt, u32 pc) { rt.unsupported(pc, 0x000001CDu, "special? not lowered yet"); }

// 0x08B33AF4: polls 0x08B2DE00 (a0 as passed the first time, 100000 afterwards) and calls 0x08B2E2B8 while it answers non-zero.
void func_08B33AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    ctx.gpr[16] = 0x10000u;
    u32 busy = call(rt, ctx, 0x08B2DE00u);
    ctx.gpr[4] = 100000u;
    while (busy != 0u) {
        call(rt, ctx, 0x08B2E2B8u);
        busy = call(rt, ctx, 0x08B2DE00u);
    }
}

// 0x08ADAA34: with the object 0x08AD9E74 finds for (a0, 0x08C90CA0): its field +0x28 plus (field +0x3C + a1) modulo its
// size (+0x2C); 0 when there is none.
void func_08ADAA34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 offset = ctx.gpr[5];
    ctx.gpr[16] = offset;
    const u32 object = call(rt, ctx, 0x08AD9E74u, {ctx.gpr[4], 0x08C90CA0u});
    ctx.gpr[6] = object;
    ctx.gpr[2] = 0u;
    if (object == 0u) return;
    const u32 position = word(rt, object + 0x3Cu) + offset;
    const u32 size = word(rt, object + 0x2Cu);
    ctx.gpr[2] = position;
    if (size == 0u) {
        divide_by_zero(rt, 0x08ADAA70u);
        return;
    }
    ctx.gpr[2] = word(rt, object + 0x28u) + signed_remainder(position, size);
}


// 0x08ADAB7C: the same lookup (0x08C90CCC), answering the smaller of (size - position modulo size) and (field +0x40 - a1).
void func_08ADAB7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 offset = ctx.gpr[5];
    ctx.gpr[16] = offset;
    const u32 object = call(rt, ctx, 0x08AD9E74u, {ctx.gpr[4], 0x08C90CCCu});
    ctx.gpr[6] = object;
    ctx.gpr[2] = 0u;
    if (object == 0u) return;
    const u32 position = word(rt, object + 0x3Cu) + offset;
    const u32 size = word(rt, object + 0x2Cu);
    ctx.gpr[2] = position;
    if (size == 0u) {
        divide_by_zero(rt, 0x08ADABB4u);
        return;
    }
    const s32 until = static_cast<s32>(word(rt, object + 0x40u) - offset);
    const s32 room = static_cast<s32>(size - signed_remainder(position, size));
    ctx.gpr[2] = static_cast<u32>(std::min(room, until));
}

// 0x089AB5D8: when a2 is zero: 0x089AB370 fills a handle in the frame, a null handle reports through 0x08A5BB7C, 0x0897BB68
// takes its field +0x14 and 0x08A5B9EC releases the frame's handle. v0 is untouched when a2 is non-zero.
void func_089AB5D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 mode = ctx.gpr[6];
    ctx.gpr[4] = frame.at(0u);
    if (mode != 0u) return;
    call(rt, ctx, 0x089AB370u, {frame.at(0u), ctx.gpr[5], mode});
    const u32 handle = word(rt, frame.at(0u));
    ctx.gpr[16] = handle;
    ctx.gpr[4] = 0x08CA50C4u;
    if (handle == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    call(rt, ctx, 0x0897BB68u, {word(rt, handle + 0x14u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x089AFE00: when a2 is 1: 0x089A354C fills a handle at frame + 4 from a3, 0x089AB370 one at frame + 0 from a1; a null
// handle reports through 0x08A5BB7C, 0x089AFC78 takes both and both are released with 0x08A5B9EC. v0 is 1 otherwise.
void func_089AFE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 source = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    const u32 second = frame.at(4u);
    ctx.gpr[2] = 1u;
    ctx.gpr[17] = second;
    ctx.gpr[16] = source;
    ctx.gpr[4] = second;
    ctx.gpr[5] = argument;
    if (mode != 1u) return;
    call(rt, ctx, 0x089A354Cu, {second, argument});
    call(rt, ctx, 0x089AB370u, {frame.at(0u), source});
    const u32 handle = word(rt, frame.at(0u));
    ctx.gpr[16] = handle;
    ctx.gpr[4] = 0x08CA50C4u;
    if (handle == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    call(rt, ctx, 0x089AFC78u, {handle, word(rt, frame.at(4u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {second});
}

// Nodes of 84 bytes in an array whose base is the container's word +0x60; a node's next link is the signed halfword at +0x52
// (an index, negative at the end of the list) and the list starts at the halfword at +4 of the holder.
constexpr u32 kNodeTable = 0x60u;
constexpr u32 kNodeSize = 84u;
constexpr u32 kNodeNext = 0x52u;
u32 node_at(Runtime &rt, u32 container, s32 index) { return word(rt, container + kNodeTable) + kNodeSize * static_cast<u32>(index); }

// 0x08B011F0: the first node of the list whose bytes +0x50 / +0x4C / +0x4D / +0x4E / +0x4F equal a2 (unsigned) / a3 (signed) /
// t0 (unsigned) / t1 (signed) / t2 (unsigned); 0 when none.
void func_08B011F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 holder = ctx.gpr[4];
    const u32 container = ctx.gpr[5];
    const u32 key0 = ctx.gpr[6];
    const u32 key1 = ctx.gpr[7];
    const u32 key2 = ctx.gpr[8];
    const u32 key3 = ctx.gpr[9];
    const u32 key4 = ctx.gpr[10];
    const s32 first = signed_half_at(rt, holder + 4u);
    ctx.gpr[2] = 0u;
    if (first < 0) return;
    u32 node = node_at(rt, container, first);
    while (node != 0u) {
        if (byte_at(rt, node + 0x50u) == key0 && static_cast<u32>(signed_byte_at(rt, node + 0x4Cu)) == key1 &&
            byte_at(rt, node + 0x4Du) == key2 && static_cast<u32>(signed_byte_at(rt, node + 0x4Eu)) == key3 &&
            byte_at(rt, node + 0x4Fu) == key4) {
            ctx.gpr[2] = node;
            return;
        }
        const s32 next_index = signed_half_at(rt, node + kNodeNext);
        if (next_index < 0) break;
        node = node_at(rt, container, next_index);
    }
    ctx.gpr[2] = 0u;
}

// Nodes of 8 bytes in an array whose base is the word +0x44 of the first argument; the list starts at the signed halfword
// +0xEC of the second (a1 in 0x08B03E1C: a2), a node's next link is the signed halfword at +6 and its other link at +2.
u32 small_node_at(Runtime &rt, u32 table_owner, s32 index) { return word(rt, table_owner + 0x44u) + (static_cast<u32>(index) << 3); }

// 0x08B03E1C: for every node of the list a2 starts whose halfword +2 equals a1, calls 0x08B03CE4(a0, a1, a2, node, a3).
void func_08B03E1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(ctx);
    const u32 owner = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 object = ctx.gpr[6];
    const u32 extra = ctx.gpr[7];
    ctx.gpr[21] = extra;
    ctx.gpr[20] = object;
    ctx.gpr[19] = key;
    ctx.gpr[18] = owner;
    const s32 first = signed_half_at(rt, object + 0xECu);
    if (first < 0) return;
    ctx.gpr[17] = owner + 0x44u;
    u32 node = small_node_at(rt, owner, first);
    ctx.gpr[16] = node;
    ctx.gpr[2] = node;
    while (node != 0u) {
        ctx.gpr[4] = owner;
        ctx.gpr[5] = key;
        ctx.gpr[6] = object;
        ctx.gpr[7] = node;
        ctx.gpr[8] = extra;
        ctx.gpr[2] = static_cast<u32>(signed_half_at(rt, node + 2u));
        if (ctx.gpr[2] == key) call(rt, ctx, 0x08B03CE4u, {owner, key, object, node, extra});
        const s32 next_index = signed_half_at(rt, node + 6u);
        ctx.gpr[2] = static_cast<u32>(next_index);
        if (next_index < 0) break;
        node = small_node_at(rt, owner, next_index);
        ctx.gpr[16] = node;
    }
}

// 0x08B04A40: for every node of the list a1 starts: a node whose halfword +2 is non-negative goes to
// 0x08B047D0(a0, a1, that halfword, node), any other to 0x08B046C0(a0, a1, node, a2).
void func_08B04A40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(ctx);
    const u32 owner = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 extra = ctx.gpr[6];
    ctx.gpr[20] = extra;
    ctx.gpr[19] = object;
    ctx.gpr[18] = owner;
    const s32 first = signed_half_at(rt, object + 0xECu);
    if (first < 0) return;
    ctx.gpr[17] = owner + 0x44u;
    u32 node = small_node_at(rt, owner, first);
    ctx.gpr[16] = node;
    ctx.gpr[2] = node;
    while (node != 0u) {
        ctx.gpr[4] = owner;
        ctx.gpr[5] = object;
        ctx.gpr[6] = node;
        ctx.gpr[7] = extra;
        ctx.gpr[8] = extra;
        const s32 link = signed_half_at(rt, node + 2u);
        if (link >= 0) {
            ctx.gpr[6] = static_cast<u32>(link);
            ctx.gpr[7] = node;
            call(rt, ctx, 0x08B047D0u, {owner, object, static_cast<u32>(link), node, extra});
        } else {
            call(rt, ctx, 0x08B046C0u, {owner, object, node, extra, extra});
        }
        const s32 next_index = signed_half_at(rt, node + 6u);
        ctx.gpr[2] = static_cast<u32>(next_index);
        if (next_index < 0) break;
        node = small_node_at(rt, owner, next_index);
        ctx.gpr[16] = node;
    }
}

// 0x08C47208 / 0x08C47338: the search over an array of words that unrolls by four: the first element in [a0, a1) equal to the
// word a2 points at (its address), else a1. A range of less than four elements is looked at by its element count, so a count
// of 0 or less answers a1 without reading a2.
void find_unrolled(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    const u32 value_address = ctx.gpr[6];
    const s32 blocks = static_cast<s32>(last - first) >> 4;
    u32 cursor = first;
    ctx.gpr[2] = last;
    if (blocks > 0) {
        const u32 value = word(rt, value_address);
        for (s32 block = 0; block < blocks; ++block) {
            for (u32 lane = 0u; lane < 4u; ++lane) {
                if (word(rt, cursor) == value) {
                    ctx.gpr[2] = cursor;
                    return;
                }
                cursor += 4u;
            }
        }
    }
    const s32 remaining = static_cast<s32>(last - cursor) >> 2;
    if (remaining >= 1 && remaining <= 3) {
        const u32 value = word(rt, value_address);
        for (s32 index = 0; index < remaining; ++index) {
            if (word(rt, cursor) == value) {
                ctx.gpr[2] = cursor;
                return;
            }
            cursor += 4u;
        }
    }
    ctx.gpr[2] = last;
}
void func_08C47208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_unrolled(rt, ctx); }
void func_08C47338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_unrolled(rt, ctx); }

// The tree insert with a position hint over keys that are strings (a string's characters at word +0x10 of a node, its length at
// characters - 0xC). Its callees are insert a key without a hint (0x08B948FC), insert next to a node (0x08B947FC), memcmp
// and the neighbour of a node (0x08B5F174 before, 0x08B5F100 after). Words of the tree: +4 is the header node, +0xC / +0x10 the first / last
// node, +0x14 the size; a node's words +0xC and +0x10 are the child and the key.
struct HintInsert {
    u32 insert_unique;   // (tree, key)
    u32 insert_at;       // (tree, x, y, key)
};
// The order of two strings: memcmp over the shorter length, the length difference when the prefix is equal.
s32 string_order(Runtime &rt, AllegrexContext &ctx, u32 left, u32 right) {
    const u32 left_length = word(rt, left - 0xCu);
    const u32 right_length = word(rt, right - 0xCu);
    const u32 shorter = right_length < left_length ? right_length : left_length;
    const u32 result = call(rt, ctx, 0x08B36170u, {left, right, shorter});
    return static_cast<s32>(result != 0u ? result : left_length - right_length);
}
u32 insert_with_hint(Runtime &rt, AllegrexContext &ctx, const HintInsert &callees) {
    const u32 tree = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    const auto insert_unique = [&]() { return call(rt, ctx, callees.insert_unique, {tree, key}); };
    const auto insert_at = [&](u32 x, u32 y) { return call(rt, ctx, callees.insert_at, {tree, x, y, key}); };
    if (position == tree + 4u) {
        if (word(rt, tree + 0x14u) != 0u) {
            const u32 last = word(rt, tree + 0x10u);
            if (string_order(rt, ctx, word(rt, last + 0x10u), word(rt, key)) < 0) return insert_at(0u, last);
        }
        return insert_unique();
    }
    if (string_order(rt, ctx, word(rt, key), word(rt, position + 0x10u)) < 0) {
        if (position == word(rt, tree + 0xCu)) return insert_at(position, position);
        const u32 before = call(rt, ctx, 0x08B5F174u, {position, position});
        if (string_order(rt, ctx, word(rt, before + 0x10u), word(rt, key)) >= 0) return insert_unique();
        if (word(rt, before + 0xCu) != 0u) return insert_at(position, position);
        return insert_at(0u, before);
    }
    if (string_order(rt, ctx, word(rt, position + 0x10u), word(rt, key)) >= 0) return position;
    if (word(rt, tree + 0x10u) == position) return insert_at(0u, position);
    const u32 after = call(rt, ctx, 0x08B5F100u, {position, word(rt, key)});
    if (string_order(rt, ctx, word(rt, key), word(rt, after + 0x10u)) >= 0) return insert_unique();
    if (word(rt, position + 0xCu) != 0u) return insert_at(after, after);
    return insert_at(0u, position);
}
void hint_insert(Runtime &rt, AllegrexContext &ctx, const HintInsert &callees) {
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(ctx);
    ctx.gpr[2] = insert_with_hint(rt, ctx, callees);
}
void func_08B94A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hint_insert(rt, ctx, {0x08B948FCu, 0x08B947FCu}); }

// Static initialisers: when a0 is 1 and a1 is 0xFFFF, 0x08A92114 registers the object O (a0 = the table's first word, size a2)
// and 0x0897D534 on it, then each row copies two words from a table into the frame (a pair) and calls the row callee with
// (entry, name, &pair, O); rows run from the last frame slot down to offset 0. v0 is 1 (a0 differs), 0xFFFF (a1 differs) or the
// last call's result.
struct StaticRow {
    u32 entry;
    u32 name;
    u32 pair_source;
};
struct StaticInit {
    u32 frame_size;
    u32 s0;
    u32 s1;
    bool s0_before_first_call;
    u32 object;
    u32 name;
    u32 size;
    u32 first_a3;
    u32 first_t0;
    u32 first_t1;
    u32 second_a2;
    u32 second_a3;
    u32 row_callee;
    const StaticRow *rows;
    u32 row_count;
};
void static_init(Runtime &rt, AllegrexContext &ctx, const StaticInit &init) {
    GuestFrame frame(ctx, init.frame_size);
    SavedRegisters saved(ctx);
    const u32 argument0 = ctx.gpr[4];
    const u32 argument1 = ctx.gpr[5];
    ctx.gpr[2] = 1u;
    if (argument0 != 1u) return;
    ctx.gpr[2] = 0xFFFFu;
    if (init.s0_before_first_call) ctx.gpr[16] = init.s0;
    ctx.gpr[17] = init.s1;
    if (argument1 != 0xFFFFu) return;
    ctx.gpr[8] = init.first_t0;
    ctx.gpr[9] = init.first_t1;
    call(rt, ctx, 0x08A92114u, {init.object + 0x10u, init.name, init.size, init.first_a3, init.first_t0, init.first_t1});
    ctx.gpr[16] = init.s0;
    call(rt, ctx, 0x0897D534u, {init.object, init.name, init.second_a2, init.second_a3});
    for (u32 row = 0u; row < init.row_count; ++row) {
        const StaticRow &entry = init.rows[row];
        const u32 pair = frame.at((init.row_count - 1u - row) * 8u);
        const u32 low = word(rt, entry.pair_source);
        const u32 high = word(rt, entry.pair_source + 4u);
        ctx.gpr[8] = low;
        ctx.gpr[9] = high;
        put(rt, pair, low);
        put(rt, pair + 4u, high);
        call(rt, ctx, init.row_callee, {entry.entry, entry.name, pair, init.object});
    }
}

const StaticRow kRows089890C8[] = {{0x08CF433Cu, 0x08CA397Cu, 0x08C8C0F0u}, {0x08CF432Cu, 0x08CA3990u, 0x08C8C0E8u},
                                   {0x08CF431Cu, 0x08CA3994u, 0x08C8C0E0u}, {0x08CF430Cu, 0x08CA399Cu, 0x08C8C0D8u},
                                   {0x08CF42FCu, 0x08CA39A4u, 0x08C8C0D0u}};
const StaticRow kRows089CA7B0[] = {{0x08CF4F2Cu, 0x08CA66A8u, 0x08C8CCB8u}, {0x08CF4F1Cu, 0x08CA66B0u, 0x08C8CCB0u},
                                   {0x08CF4F0Cu, 0x08CA66BCu, 0x08C8CCA8u}, {0x08CF4EFCu, 0x08CA66CCu, 0x08C8CCA0u},
                                   {0x08CF4EECu, 0x08CA66D8u, 0x08C8CC98u}};
const StaticRow kRows0899D690[] = {{0x08CF4784u, 0x08CA48ACu, 0x08C8C4A4u}, {0x08CF4774u, 0x08CA48B8u, 0x08C8C49Cu},
                                   {0x08CF4764u, 0x08CA48CCu, 0x08C8C494u}, {0x08CF4754u, 0x08CA48DCu, 0x08C8C48Cu},
                                   {0x08CF4744u, 0x08CA48E4u, 0x08C8C484u}, {0x08CF4734u, 0x08CA48E8u, 0x08C8C47Cu},
                                   {0x08CF4724u, 0x08CA48ECu, 0x08C8C474u}, {0x08CF4714u, 0x08CA48F4u, 0x08C8C46Cu}};
const StaticRow kRows089F63BC[] = {{0x08CF5A6Cu, 0x08CA8378u, 0x08C8D9D4u}, {0x08CF5A5Cu, 0x08CA8380u, 0x08C8D9CCu},
                                   {0x08CF5A4Cu, 0x08CA838Cu, 0x08C8D9C4u}, {0x08CF5A3Cu, 0x08CA8398u, 0x08C8D9BCu},
                                   {0x08CF5A2Cu, 0x08CA83A0u, 0x08C8D9B4u}, {0x08CF5A1Cu, 0x08CA83A8u, 0x08C8D9ACu},
                                   {0x08CF5A0Cu, 0x08CA83B4u, 0x08C8D9A4u}, {0x08CF59FCu, 0x08CA83B8u, 0x08C8D99Cu}};

void func_089890C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static const StaticInit init{0x40u, 0x004A0000u, 0x00010000u, true, 0x08CF42D0u, 0x08CA3970u, 0x200u, 0x08988EECu,
                                 0x0898A0C0u, 0x08988F10u, 0x0898AB10u, 0x0898AACCu, 0x08BEFD08u, kRows089890C8, 5u};
    static_init(rt, ctx, init);
}
void func_089CA7B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static const StaticInit init{0x40u, 0x004A0000u, 0x00020000u, true, 0x08CF4EC0u, 0x08CA66A0u, 0x170u, 0x089CA6D4u,
                                 0x089CAF80u, 0x089CA6F8u, 0x089CB144u, 0x089CB100u, 0x08C01DC0u, kRows089CA7B0, 5u};
    static_init(rt, ctx, init);
}
void func_0899D690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static const StaticInit init{0x50u, 0x00020000u, 0x004A0000u, false, 0x08CF46E8u, 0x08CA48A0u, 0x170u, 0x0899D014u,
                                 0x0899E65Cu, 0x0899D038u, 0x0899E22Cu, 0x0899E1E8u, 0x08BF40C0u, kRows0899D690, 8u};
    static_init(rt, ctx, init);
}
void func_089F63BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static const StaticInit init{0x50u, 0x00020000u, 0x004A0000u, false, 0x08CF59D0u, 0x08CA836Cu, 0xA0u, 0x089F60F8u,
                                 0x089F7534u, 0x089F611Cu, 0x089F7098u, 0x089F6F30u, 0x08C0B584u, kRows089F63BC, 8u};
    static_init(rt, ctx, init);
}

} // namespace
} // namespace hot_batch_342
} // namespace gt::game

#define HOT342(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_342::name, gt::ReturnKind::kind)
HOT342(0x08B33AF4u, func_08B33AF4, Word);
HOT342(0x08ADAA34u, func_08ADAA34, Word);
HOT342(0x08ADAB7Cu, func_08ADAB7C, Word);
HOT342(0x089AB5D8u, func_089AB5D8, Word);
HOT342(0x089AFE00u, func_089AFE00, Word);
HOT342(0x08B011F0u, func_08B011F0, Word);
HOT342(0x08B03E1Cu, func_08B03E1C, Word);
HOT342(0x08B04A40u, func_08B04A40, Word);
HOT342(0x08C47208u, func_08C47208, Word);
HOT342(0x08C47338u, func_08C47338, Word);
HOT342(0x08B94A58u, func_08B94A58, Word);
HOT342(0x089890C8u, func_089890C8, Word);
HOT342(0x089CA7B0u, func_089CA7B0, Word);
HOT342(0x0899D690u, func_0899D690, Word);
HOT342(0x089F63BCu, func_089F63BC, Word);
