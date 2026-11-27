// New batch main 1017 (main session): hand-written drafts from `tools/disasm`, not gated yet.
//   0x08BA9DB8, 0x08BDC338: libstdc++ vector<8-byte element {word +0, shared-pointer control block +4}>::insert(position,
//               count, value), the same instruction sequence twice (only the eight library callees and the length_error
//               message differ). Nothing when count == 0. With room: the value goes to a frame element (its control block's
//               use count incremented); the elements after the position move up (move_tail for the tail, then word and
//               shared-pointer assignments backwards, each new block counted up and the old one released) or, when fewer
//               follow, fill_short / move_short; fill copy-constructs at the position; the frame element is released.
//               Full: size + max(size, count) elements (more than 0x1FFFFFFF throws), the new range is built (build_front /
//               build_fill / build_front), the old one destroyed and freed, and begin / end / capacity are updated.
//               v0: 0x08BA9DB8 leaves the frame release's answer (or fill's result when the frame block is null); 0x08BDC338
//               leaves the same in the room path and block + bytes in the full path; both leave v0 untouched when count == 0.
//   0x08AE991C (a0 = {begin, end} indices, a1 = key, a2 = group, a3 = kind, t0 = wanted): searches the 0x38-byte entry
//               table at 0x08D40A3C (byte +4 bit 0 = used, halfword +6 = key, word +8 = group, word +0xC = order). kind 0
//               takes the table's max by (key, +0xC) among entries with group == a2 and key >= a1; kind 1 the same with key
//               > a1; kind 2 takes the entry at (random() % matched) of the group/key>= matches. Fewer than max(t0, 1)
//               group matches fall back to 0x08AE9798(a0, a1); any other kind (and a search that finds nothing on kind 2)
//               answers -1, printing through 0x08AD83B8 when the global 0x08CDFB3C bit 0 asks for it.
//   0x08874820 (a0 = object, a1 = stream/table object, a2 = string): splits the string at its last ':' and ')' - see the
//               body - into a ':'-prefix buffer, a '%'-suffix buffer and the middle, builds one or two temp strings through
//               0x08B37640 and hands the middle to the object's sub-object at +0xC; when the object's +0xC node's +0x26
//               bit 0 is set a fresh node (vtable 0x08CBFC10, three 0x08D752A4 slots, three zero pairs, flags word) and a
//               reference (vtable 0x08CBFC90) are built and swapped into +0xC first. Always answers 1.
// Skipped in this batch: 0x0883ED54 (its loop calls 0x0884CC18 with an a1 the asm never sets - the value is whatever the
// preceding call left, so a faithful transcription needs a callee the harness records, not a guess) and 0x088F82D0 (about
// ninety VFPU instructions whose results must match the VFPU register file exactly; not drafted without checking each
// prefix and the harness's VFPU comparison).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1879 {
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
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 load8(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) {
    rt.memory().store8(address, static_cast<std::uint8_t>(value));
}
inline s32 signed_half_at(Runtime &rt, u32 address) {
    return static_cast<std::int16_t>(rt.memory().load16(address));
}

// `lw v0, slot(vtable); jalr v0` (or the call through a word already loaded): v0 = the target when it starts, v0 / v1 /
// f0 as the callee leaves them.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    return call_indirect(rt, ctx, word(rt, ctx.gpr[3] + slot), arguments);
}

// The `ll`/`sc` pair on a shared object's use count, single-threaded here: the word is read, incremented or decremented
// and written back, v1 holds the address and v0 ends as the successful `sc` leaves it (1).
void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}
// v0 = 1 after the pair (the `addiu v0, zero, 1` in its branch-delay slot); the old count is the answer.
u32 count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}
// One use goes away: at zero the control block runs slot 8 (dispose), then its weak count (+8) goes down and at zero slot
// 0xC (the control block itself), both with a0 = the control block. Answers what v0 holds at the end.
u32 release_shared(Runtime &rt, AllegrexContext &ctx, u32 block) {
    if (count_down(rt, ctx, block + 4u) != 1u) return 1u;
    call_virtual(rt, ctx, block, 8u, {block});
    if (count_down(rt, ctx, block + 8u) != 1u) return 1u;
    return call_virtual(rt, ctx, block, 0xCu, {block});
}

// The eight library callees a vector<8-byte element> insertion needs. The two copies of the body below differ in nothing
// else but these addresses and the throw message.
struct InsertCallees {
    u32 move_tail;      // moves the (count) elements at the end out of the way
    u32 fill;           // copy-constructs `count` elements at a position from the frame element
    u32 fill_short;     // constructs the (count - after) new tail elements
    u32 move_short;     // shifts the `after` following elements up
    u32 message;        // the length_error message handed to the throw helper
    u32 build_front;    // copy-constructs the range before the position into the new block
    u32 build_fill;     // fills the new range
    u32 destroy;        // destroys the old range
};
constexpr u32 kThrow = 0x08B5ADDCu;
constexpr u32 kAllocate = 0x08B35DA0u;
constexpr u32 kFree = 0x08B35254u;
constexpr InsertCallees kInsertMirror = {0x08BA91A8u, 0x08BA9600u, 0x08BA9068u, 0x08BA90B8u,
                                         0x08CA0740u, 0x08BA9158u, 0x08BA9108u, 0x08BA8D90u};
constexpr InsertCallees kInsertSecond = {0x08BDBFE0u, 0x08BDC228u, 0x08BDC030u, 0x08BDC080u,
                                         0x08CA1A44u, 0x08BDC1D8u, 0x08BDC188u, 0x08B8ECACu};

// vector = {begin +0, end +4, capacity +8}, element = {value +0, control block +4}; count == 0 does nothing.
void insert_shared_values(Runtime &rt, AllegrexContext &ctx, const InsertCallees &callees) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;  // v0 untouched
    const u32 end = word(rt, vector + 4u);
    const u32 room = static_cast<u32>(signed_of(word(rt, vector + 8u) - end) >> 3);
    if (room < count) {
        // No room: a fresh block of size + max(size, count) elements.
        const u32 begin = word(rt, vector);
        const u32 size = static_cast<u32>(signed_of(end - begin) >> 3);
        put(rt, frame.at(8u), size);
        if (0x1FFFFFFFu - size < count) call(rt, ctx, kThrow, {callees.message});
        const u32 grow = size < count ? count : size;
        const u32 total = size + grow;
        u32 bytes = 0xFFFFFFF8u;  // a wrapped total asks for -8 bytes
        u32 block = 0u;
        if (total < size) {
            block = call(rt, ctx, kAllocate, {bytes});
        } else {
            bytes = total << 3;
            if (total != 0u) block = call(rt, ctx, kAllocate, {bytes});
        }
        const u32 front = call(rt, ctx, callees.build_front, {begin, position, block, frame.at(3u)});
        call(rt, ctx, callees.build_fill, {front, count, value, frame.at(2u)});
        const u32 back = call(rt, ctx, callees.build_front,
                              {position, word(rt, vector + 4u), front + (count << 3), frame.at(1u)});
        call(rt, ctx, callees.destroy, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        const u32 old_begin = word(rt, vector);
        if (old_begin != 0u) call(rt, ctx, kFree, {old_begin});
        put(rt, vector + 8u, block + bytes);
        put(rt, vector + 4u, back);
        put(rt, vector, block);
        ctx.gpr[2] = block + bytes;
        return;
    }
    // Room: the value goes to a frame element first, holding one use of its control block.
    const u32 element = frame.at(0xCu);
    const u32 block = word(rt, value + 4u);
    put(rt, element, word(rt, value));
    put(rt, element + 4u, block);
    if (block != 0u) count_up(rt, ctx, block + 4u);
    const u32 after = static_cast<u32>(signed_of(end - position) >> 3);
    u32 filled;
    if (count < after) {
        const u32 source_end = end - (count << 3);
        call(rt, ctx, callees.move_tail, {source_end, end, end, frame.at(6u)});
        const u32 middle = static_cast<u32>(signed_of(source_end - position) >> 3);
        put(rt, vector + 4u, word(rt, vector + 4u) + (count << 3));
        if (signed_of(middle) > 0) {
            u32 source = source_end;
            u32 destination = end;
            for (u32 index = 0u; index != middle; ++index) {
                source -= 8u;
                destination -= 8u;
                const u32 moved = word(rt, source);
                const u32 old_block = word(rt, destination + 4u);
                put(rt, destination, moved);
                const u32 new_block = word(rt, source + 4u);
                if (new_block == old_block) continue;
                if (new_block != 0u) count_up(rt, ctx, new_block + 4u);
                const u32 previous = word(rt, destination + 4u);
                if (previous != 0u) release_shared(rt, ctx, previous);
                put(rt, destination + 4u, new_block);
            }
        }
        filled = call(rt, ctx, callees.fill, {position, position + (count << 3), element});
    } else {
        call(rt, ctx, callees.fill_short, {end, count - after, element, frame.at(5u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + ((count - after) << 3));
        call(rt, ctx, callees.move_short, {position, end, word(rt, vector + 4u), frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (after << 3));
        filled = call(rt, ctx, callees.fill, {position, end, element});
    }
    const u32 element_block = word(rt, element + 4u);
    // sp_loads flags this `lw s0, 0x10($sp)`: it reads the frame element's control block again (the word stored at element
    // + 4 above), not a callee-saved register reload.
    ctx.gpr[2] = element_block != 0u ? release_shared(rt, ctx, element_block) : filled;
}

// The entry table the lookup searches (0x08D40A3C); each entry is 0x38 bytes.
constexpr u32 kEntryTable = 0x08D40A3Cu;
constexpr u32 kEntryStride = 0x38u;

u32 entry_at(Runtime &rt, u32 index) { return word(rt, kEntryTable) + index * kEntryStride; }
bool entry_used(Runtime &rt, u32 entry) { return (load8(rt, entry + 4u) & 1u) != 0u; }

// kind 0 (`descending` false) / kind 1 (`descending` true): the table's maximum by (key, +0xC) among the entries in
// [begin, end) whose group matches and whose key passes the kind's test. Fewer than `wanted` group matches fall back.
u32 select_entry(Runtime &rt, AllegrexContext &ctx, u32 pair, u32 begin, u32 end, u32 key, u32 group, u32 wanted,
                 bool descending) {
    u32 best = 0xFFFFFFFFu;
    s32 best_key = -1;
    u32 best_order = 0xFFFFFFFFu;
    u32 matched = 0u;
    if (!(end < begin)) {
        u32 index = begin;
        u32 entry = entry_at(rt, begin);
        for (;;) {
            if (entry_used(rt, entry) && word(rt, entry + 8u) == group) {
                const s32 entry_key = signed_half_at(rt, entry + 6u);
                const bool passes = descending ? entry_key > static_cast<s32>(key) : entry_key >= static_cast<s32>(key);
                if (passes) {
                    const u32 order = word(rt, entry + 0xCu);
                    if (best == 0xFFFFFFFFu || entry_key > best_key ||
                        (entry_key == best_key && !(order < best_order))) {
                        best = word(rt, entry);
                        best_key = entry_key;
                        best_order = order;
                    }
                }
                ++matched;
            }
            ++index;
            if (end < index) break;
            entry += kEntryStride;
        }
    }
    if (matched < wanted) return call(rt, ctx, 0x08AE9798u, {pair, key});
    return best;
}

// kind 2: the entry at (0x08AD9E2C() % matches) of the group matches whose key is at least `key`.
u32 select_remainder(Runtime &rt, AllegrexContext &ctx, u32 pair, u32 begin, u32 end, u32 key, u32 group, u32 wanted) {
    u32 count = 0u;
    u32 matches = 0u;
    {
        u32 index = begin;
        u32 entry = entry_at(rt, begin);
        for (;;) {
            const bool used = entry_used(rt, entry);
            const u32 next = index + 1u;
            const bool done = end < next;
            if (used && word(rt, entry + 8u) == group) {
                ++count;
                if (signed_half_at(rt, entry + 6u) >= static_cast<s32>(key)) ++matches;
            }
            index = next;
            if (done) break;
            entry += kEntryStride;
        }
    }
    if (count < wanted) return call(rt, ctx, 0x08AE9798u, {pair, key});
    if (matches == 0u) return 0xFFFFFFFFu;
    const u32 remainder = call(rt, ctx, 0x08AD9E2Cu, {}) % matches;
    u32 seen = 0u;
    {
        u32 index = begin;
        u32 entry = entry_at(rt, begin);
        for (;;) {
            const bool used = entry_used(rt, entry);
            const u32 next = index + 1u;
            const bool done = end < next;
            if (used && word(rt, entry + 8u) == group &&
                signed_half_at(rt, entry + 6u) >= static_cast<s32>(key)) {
                if (remainder == seen) return word(rt, entry);
                ++seen;
            }
            index = next;
            if (done) break;
            entry += kEntryStride;
        }
    }
    if ((word(rt, 0x08CDFB3Cu) & 1u) == 0u) return 0xFFFFFFFFu;
    call(rt, ctx, 0x08AD83B8u,
         {word(rt, 0x08CDFB40u), 0x08C91AB4u, 0x1BFu, 0x08C91B00u, count, matches, remainder});
    return 0xFFFFFFFFu;
}

} // namespace

// 0x08BA9DB8: the insertion through the mirror library's eight callees.
// 0x08BDC338: the insertion through the second library's eight callees.
void func_08BDC338(Runtime &rt, AllegrexContext &ctx) { insert_shared_values(rt, ctx, kInsertSecond); }

// 0x08AE991C (a0 = {begin, end}, a1 = key, a2 = group, a3 = kind, t0 = wanted).
void func_08AE991C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 pair = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 group = ctx.gpr[6];
    const s32 kind = static_cast<s32>(ctx.gpr[7]);
    const s32 wanted_register = static_cast<s32>(ctx.gpr[8]);
    const u32 wanted = static_cast<u32>(wanted_register < 1 ? 1 : wanted_register);
    const u32 begin = word(rt, pair);
    const u32 end = word(rt, pair + 4u);
    if (kind == 1) {
        ctx.gpr[2] = select_entry(rt, ctx, pair, begin, end, key, group, wanted, true);
        return;
    }
    if (kind < 2) {
        if (kind == 0) {
            ctx.gpr[2] = select_entry(rt, ctx, pair, begin, end, key, group, wanted, false);
            return;
        }
    } else if (kind == 2) {
        ctx.gpr[2] = select_remainder(rt, ctx, pair, begin, end, key, group, wanted);
        return;
    }
    // Out of range: -1, printed through 0x08AD83B8 when the global flag asks for it.
    if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u)
        call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C91AB4u, 0x1CBu, 0x08C91AE4u});
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08874820 (a0 = object, a1 = stream/table object, a2 = string).

} // namespace new_batch_main_1879
} // namespace gt::game

#define NEW1017(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1879::name, gt::ReturnKind::kind)
NEW1017(0x08BDC338u, func_08BDC338, Word);
NEW1017(0x08AE991Cu, func_08AE991C, Word);
