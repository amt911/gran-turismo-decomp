// ---- from main-session batch 260 (draft a7c7443e163ac6d3fedcee6e49a5b4443af8b666) ----
// New batch 260 (main session, draft): functions no src/ file defines or registers, written by hand from
// `tools/disasm` as structured C. Not verified by the differential gate yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_260 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call whose float argument(s) and float result ($f0) matter; v0 keeps the callee's as well.
u32 call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
           std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
float float_call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                 std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// 0x08A4EE1C (a0 = object, a1 = a two-word kind descriptor, a2 = a flag byte): the per-frame update of the state row at
// +0x90. The kind at info+0 picks the path; each path moves the row by a step taken from the object's own fields, calls the
// row helpers (0x08A4CEDC / 0x08A4CF24 move it and answer a status, 0x08A4E6F4 resets, 0x08A4EE08 sets its flag) and, on
// the common path, appends one 16-byte entry to the ring at +0x98 (count at +0x9C, step at +0x20/+0x24, interpolation
// factors f21/f22) before asking 0x08A4CEDC once more. v0 = the final mode test on the word at +0x58, 0 on every earlier
// return (the asm ends every one of them with `a0 = 0`).

// The compiler's virtual call: v0 = the vtable (object[0]), the callee = vtable[offset], a0 = object.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    const GuestResult result = call_guest(rt, ctx, function, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The 4x4 matrix 0x08B4E648 writes: identity with (x, y, z, 1) in the last row.
void put_matrix(Runtime &rt, u32 at, float x, float y, float z) {
    put_float(rt, at + 0x00u, 1.0f);
    put_float(rt, at + 0x04u, 0.0f);
    put_float(rt, at + 0x08u, 0.0f);
    put_float(rt, at + 0x0Cu, 0.0f);
    put_float(rt, at + 0x10u, 0.0f);
    put_float(rt, at + 0x14u, 1.0f);
    put_float(rt, at + 0x18u, 0.0f);
    put_float(rt, at + 0x1Cu, 0.0f);
    put_float(rt, at + 0x20u, 0.0f);
    put_float(rt, at + 0x24u, 0.0f);
    put_float(rt, at + 0x28u, 1.0f);
    put_float(rt, at + 0x2Cu, 0.0f);
    put_float(rt, at + 0x30u, x);
    put_float(rt, at + 0x34u, y);
    put_float(rt, at + 0x38u, z);
    put_float(rt, at + 0x3Cu, 1.0f);
}

// 0x0891F110 (a0 = entity, a1 = the asset object the index is resolved on, a2 = a step record: a float at +0 and a signed
// word at +0xC, a3 = an index, signed): advances the entity's phase counter (+0x23C) through 0..3 with the height at
// +0x238 as the timer, blends the two table rows the phase picks out of 0x08CF27B4 (7 floats each) by a 0..1 factor, writes
// them to +0x21C..+0x234 and rebuilds the entity's transform at +0x10 from the referenced node's own two vectors. When the
// index does not resolve, the transform row at +0x1E4 is cleared instead. v0 is left as the last call left it.

// The string helpers' release (the block at 0x08A9D3CC and its ten repeats): a temporary is one word, its buffer; the
// buffer's header sits 0xC bytes below it and the shared empty buffer (0x08D74FDC) is never released. 0x08B60804 drops a
// reference and answers the count; only a non-positive count needs 0x08B8542C to free the buffer (a1 is scratch space).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 scratch) {
    const u32 buffer = word(rt, slot);
    const u32 header = buffer - 0xCu;
    if (header == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {header, scratch});
}

// 0x08A9D2D8 (a0 = object, a1 = the name string object, a2 = a pointer to another string object): splits the name along its
// last '/' (0x08BFD224) and '.', keeps the directory in the second temporary and the base name in a third, and resolves it
// through 08A9C7B8 / 08A5BAD8 into the object at sp+0x40; when that has entries, 08A9C1C0 consumes them. v0 = 1 when the
// name was already accepted (0x08A9C778) or the entry list was consumed or the last step produced nothing, else 0.

// 0x08AF3870 (a0 = output object, a1 = the node/camera object, a2 = a vertex count, a3 = the vertex array, t0 = a
// transform matrix): fits the counted vertices into four passes. Each pass builds a view matrix (0x08B509EC) from the
// floating box the previous pass shrank, projects every vertex through it (0x08B4ACE0) keeping the two extreme (x/-z,
// y/-z) vertices, turns those extrema into two projection scales (0x08AF3270) and shrinks the box toward the centre at
// +0x58. The object's own two vtable calls fill the matrices at 0xB0 / 0xF0; the final pass writes the view at +0x60 and
// copies the matrix at 0xF0 into +0x20.

} // namespace
} // namespace new_batch_main_260
} // namespace gt::game

#define NEW260(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_260::name, gt::ReturnKind::kind)

// ---- from main-session batch 261 (draft 1234b481a4193e957c218b391b24a96901709894) ----
// Main-session batch 261: the two vector-insert bodies 0x08BB35FC / 0x08BB4C74 (same instruction sequence, two
// template instantiations: only the word-copy helper, the walk helper and the .rodata slot holding the pool's
// once flag differ), written as structured C from `tools.disasm`.
//
// What they are, from the asm: `std::vector<4-byte item, GT's block allocator>::insert` at a position. The
// object is {begin +0, end +4, end-of-storage +8}. Room left: the last element is copied onto `end`, `end` is
// advanced, the tail [at, end-1) is moved up with 0x08B79BAC and *value lands at `at`. Full: the new storage is
// 1 element (empty) or twice the size (0xFFFFFFFC bytes when the doubling overflows, and a size of 0x3FFFFFFF
// reports through 0x08B5ADDC(0x08CA0A48) first), taken from the 8-byte block pool at 0x08D75F5C; the head
// [begin, at) is copied in, *value follows it, the tail [at, end) after that, the old range is walked
// (0x08BB1C1C, a no-op loop) and the old block is given back to the pool; then begin = new, end = the second
// copy's answer and end-of-storage = new + capacity bytes, which is also v0.
//
// The pool (its layout is read off these two functions and their siblings 0x08BB13D4 / 0x088B3460 / 0x088B2770):
// +0/+4/+8 is a plain vector of chunk pointers, +0x0C is the free-list sentinel (its first word is itself, its
// second the block count 0), +0x14 the list hint, +0x18 the block limit (0x1001, grown by half on each refill),
// +0x1C blocks handed out, +0x20 blocks in use, +0x24 their peak. A block record is {next +0, 8-byte blocks +4,
// payload +8}; the list is circular and ordered by address, and a refill coalesces the new chunk with its
// neighbours. The lazy initialiser runs on the once object 0x08D75F50 (guarded by 0x08B6441C / 0x08B64494) and
// registers 0x088B8234 through 0x08B77B1C.
//
// Not written: 0x08C66844 and 0x08C67F3C (306 instructions each, one body with a single callee swapped:
// 0x08C6F354 / 0x08C6F250). They are libstdc++'s formatted-output path, and three things in them cannot be
// reproduced faithfully from the asm alone by reading:
//  - four data-dependent stack buffers (`subu sp, sp, v0`), sized from the call's precision argument ([a2+4],
//    clamped to 6 only when negative), from a length 0x08B64C80 answers, twice that, and from the field width
//    ([a2+8]) again inside the padding loop that jumps back to 0x08C66A58 - each iteration moves sp, so the
//    decompiled body has to keep ctx.gpr[29] in step by hand;
//  - a call graph that only runs on live objects: [a1] is an object reached through its vtable (+0x18, +0x1C,
//    +0x30), [a2+0x6C] is a locale whose facet table (+0xC + 4 * index from 0x08B5C2B0) is lazily built with
//    0x08B62344 / 0x08C661C4 / 0x08B5C2F8, and the state byte at +0x1C (0, 1, 2) drives three of its branches.
//    In the synthetic arena those vtable words are garbage, so the original faults and every case is "not
//    comparable" - it needs a real stream object as captured input, not a shape;
//  - it answers a {word, byte} pair in v0/v1 (ReturnKind::DoubleWord): 0x08C66D44, its caller, reads both.
// A guessed draft of that would cost the gate a round and prove nothing, so the two are left recompiled.
//
// The callee-saved registers are set where the asm sets them (a callee may read them) and put back at the end.
// Every `beql` / `bnel` delay slot here only runs when the branch is taken (0x3644, 0x37FC, 0x385C, 0x386C,
// 0x38B0, 0x38D8, 0x3A00, 0x3A2C, 0x3A58, 0x3A80); the 0x3644 one is what makes the in-place path reload `end`
// from the object instead of the old `end` register.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_261 {
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

// The callee-saved registers s0-s7: the original saves and restores them, so the body may set them (a callee reads
// them through the context) and they come back as they were.
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

// The block pool of the allocator these vectors use, and its once object.
constexpr u32 kPool = 0x08D75F5Cu;
constexpr u32 kSentinel = 0x08D75F68u;  // kPool + 0x0C: {next = itself, count = 0}
constexpr u32 kOnce = 0x08D75F50u;
constexpr u32 kOnceSlot = 0x08CB6058u;
constexpr u32 kBlockLimit = 0x1001u;
constexpr u32 kAtexit = 0x088B8234u;
constexpr u32 kLengthError = 0x08CA0A48u;
constexpr u32 kMaxItems = 0x3FFFFFFFu;
constexpr u32 kThrowLength = 0x08B5ADDCu;
constexpr u32 kAlignedAllocate = 0x08B2B4D8u;
constexpr u32 kMoveBytes = 0x08B79BACu;
constexpr u32 kPlainInsert = 0x08BB13D4u;
constexpr u32 kOnceTest = 0x08B6441Cu;
constexpr u32 kOnceRelease = 0x08B64494u;
constexpr u32 kAtexitRegister = 0x08B77B1Cu;

// The lazy pool initialiser both functions run the first time they touch the pool: when the once object 0x08D75F50
// is clear and 0x08B6441C(`once`) says this call owns the initialisation, the fields are set, the once object is
// released with the sentinel as the argument, the shared vector is emptied and 0x088B8234 is registered.
// `once` is the value a0 holds before the 0x08B6441C call: the word of the .rodata slot in the growth path, the
// object's own address (0x08D75F50) in the free path.
void pool_init(Runtime &rt, AllegrexContext &ctx, u32 once) {
    if (rt.memory().load8(kOnce) != 0u) return;
    if (call(rt, ctx, kOnceTest, {once}) == 0u) return;
    put(rt, kPool + 0x18u, kBlockLimit);
    put(rt, kPool + 0x0Cu, kSentinel);
    put(rt, kPool + 0x10u, 0u);
    put(rt, kPool + 0x04u, 0u);
    put(rt, kPool + 0x08u, 0u);
    put(rt, kPool + 0x14u, kSentinel);
    put(rt, kPool + 0x1Cu, 0u);
    put(rt, kPool + 0x20u, 0u);
    put(rt, kPool + 0x24u, 0u);
    (void)call(rt, ctx, kOnceRelease, {once, kSentinel});
    put(rt, kPool, 0u);
    (void)call(rt, ctx, kAtexitRegister, {kAtexit});
}

// The free-list walk that finds the record whose successor is the first free block above `block` (the list is
// circular, so it stops when it reaches the hint again). Answers that record; the hint in `kPool + 0x14` is the
// starting point.
u32 find_record_below(Runtime &rt, u32 block) {
    const u32 hint = word(rt, kPool + 0x14u);
    u32 node = hint;
    for (;;) {
        const u32 next = word(rt, node);
        if (node >= block) {
            if (node < next) { node = next; continue; }
            if (block < next) return node;
            node = next;
            continue;
        }
        if (block < next) return node;
        if (node < next) { node = next; continue; }
        return node;
    }
}

// Gives the 8-byte block record at `start - 8` back to the pool: the block count leaves the in-use count, the
// record is spliced into the address-ordered free list (coalescing with the block after it and the one before it)
// and the hint becomes its predecessor. The asm inlines the same initialiser as the growth path (with the once
// object's own address as the argument).
void free_block(Runtime &rt, AllegrexContext &ctx, u32 start) {
    pool_init(rt, ctx, kOnce);
    const u32 block = start - 8u;
    const u32 node = find_record_below(rt, block);
    put(rt, kPool + 0x20u, word(rt, kPool + 0x20u) - word(rt, block + 4u));
    const u32 count = word(rt, block + 4u);
    const u32 successor = word(rt, node);
    if (block + (count << 3) != successor) {
        put(rt, block, successor);
    } else {
        put(rt, block + 4u, count + word(rt, successor + 4u));
        put(rt, block, word(rt, successor));
    }
    const u32 node_count = word(rt, node + 4u);
    if (block != node + (node_count << 3)) {
        put(rt, node, block);
    } else {
        put(rt, node + 4u, node_count + word(rt, block + 4u));
        put(rt, node, word(rt, block));
    }
    put(rt, kPool + 0x14u, node);
}

// Takes `want` blocks from the pool, refilling (and growing the chunk limit by half) while no free record is big
// enough. Answers the first byte of the block, or 0 when the refill fails. `spare` is the stack word the plain
// insert gets its value from.
u32 allocate_blocks(Runtime &rt, AllegrexContext &ctx, u32 want, u32 spare) {
    for (;;) {
        const u32 circle = word(rt, kPool + 0x14u);
        u32 node = circle;
        u32 block = word(rt, node);
        if (word(rt, block + 4u) >= want) {
            // A record with room: take the blocks from its tail (or unlink it whole).
            const u32 count = word(rt, block + 4u);
            if (count == want) {
                put(rt, node, word(rt, block));
            } else {
                put(rt, block + 4u, count - want);
                block = block + ((count - want) << 3);
                put(rt, block + 4u, want);
            }
            put(rt, kPool + 0x14u, node);
            const u32 used = want + word(rt, kPool + 0x20u);
            put(rt, kPool + 0x20u, used);
            if (word(rt, kPool + 0x24u) < used) put(rt, kPool + 0x24u, used);
            return block + 8u;
        }
        // Walk the circle: every record reached is already known to be too small.
        u32 candidate = block;
        u32 predecessor = circle;
        for (;;) {
            if (candidate == circle) break;
            predecessor = candidate;
            candidate = word(rt, candidate);
            if (word(rt, candidate + 4u) >= want) {
                const u32 count = word(rt, candidate + 4u);
                if (count == want) {
                    put(rt, predecessor, word(rt, candidate));
                } else {
                    put(rt, candidate + 4u, count - want);
                    candidate = candidate + ((count - want) << 3);
                    put(rt, candidate + 4u, want);
                }
                put(rt, kPool + 0x14u, predecessor);
                const u32 used = want + word(rt, kPool + 0x20u);
                put(rt, kPool + 0x20u, used);
                if (word(rt, kPool + 0x24u) < used) put(rt, kPool + 0x24u, used);
                return candidate + 8u;
            }
        }
        // Nothing big enough: a new chunk, at most `limit` blocks, and the limit grows by half.
        const u32 limit = word(rt, kPool + 0x18u);
        const u32 blocks = want < limit ? want : limit;
        const u32 chunk = call(rt, ctx, kAlignedAllocate, {0x10u, blocks << 3});
        if (chunk == 0u) return 0u;
        put(rt, spare, chunk);
        put(rt, kPool + 0x18u, limit + (limit >> 1));
        const u32 end_slot = word(rt, kPool + 0x04u);
        const u32 storage_end = word(rt, kPool + 0x08u);
        if (end_slot == storage_end) {
            (void)call(rt, ctx, kPlainInsert, {kPool, end_slot, spare});
        } else if (end_slot != 0u) {
            put(rt, end_slot, chunk);
            put(rt, kPool + 0x04u, word(rt, kPool + 0x04u) + 4u);
        } else {
            put(rt, kPool + 0x04u, 4u);
        }
        put(rt, kPool + 0x1Cu, blocks + word(rt, kPool + 0x1Cu));
        put(rt, chunk + 4u, blocks);
        const u32 payload = chunk + 8u;
        if (payload == 0u) continue;
        put(rt, kPool + 0x20u, blocks + word(rt, kPool + 0x20u));
        // Splice the chunk into the address-ordered list (a new chunk is above every earlier one).
        u32 after = word(rt, kPool + 0x14u);
        for (;;) {
            const u32 next = word(rt, after);
            if (after < chunk) {
                if (chunk < next) break;
                if (after < next) { after = next; continue; }
                break;
            }
            if (after < next) { after = next; continue; }
            if (chunk < next) break;
            after = next;
        }
        put(rt, kPool + 0x20u, (blocks + word(rt, kPool + 0x20u)) - word(rt, chunk + 4u));
        const u32 chunk_count = word(rt, chunk + 4u);
        const u32 successor = word(rt, after);
        if (chunk + (chunk_count << 3) != successor) {
            put(rt, chunk, successor);
        } else {
            put(rt, chunk + 4u, chunk_count + word(rt, successor + 4u));
            put(rt, chunk, word(rt, successor));
        }
        const u32 node_count = word(rt, after + 4u);
        if (chunk != after + (node_count << 3)) {
            put(rt, after, chunk);
        } else {
            put(rt, after + 4u, node_count + word(rt, chunk + 4u));
            put(rt, after, word(rt, chunk));
        }
        put(rt, kPool + 0x14u, after);
    }
}

// 0x08BB35FC / 0x08BB4C74: the insert. `copy_words` is 0x08BB1CAC / 0x08BB1B5C (the word copy that answers the
// advanced destination) and `walk` 0x08BB1C1C / 0x08BB1B4C (the no-op `for (p = first; p != last; p += 4)` the
// original runs over the old range); `once_slot` is the .rodata word holding the pool's once object.
void insert_at(Runtime &rt, AllegrexContext &ctx, u32 copy_words, u32 walk, u32 once_slot) {
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 at = ctx.gpr[5];
    const u32 value_at = ctx.gpr[6];
    ctx.gpr[18] = self;     // s2
    ctx.gpr[20] = at;       // s4
    ctx.gpr[21] = value_at; // s5
    const u32 end = word(rt, self + 4u);
    const u32 storage_end = word(rt, self + 8u);
    if (end != storage_end) {
        // Room left: move the last element onto `end`, then the tail up by one.
        u32 top = end;
        u32 last = end;
        if (end != 0u) {
            put(rt, end, word(rt, end - 4u));
            top = word(rt, self + 4u);
        } else {
            top = 0u;
        }
        const u32 bytes = ((top - 4u) - at) & ~3u;
        put(rt, self + 4u, last + 4u);
        const u32 value = word(rt, value_at);
        ctx.gpr[2] = call(rt, ctx, kMoveBytes, {top - bytes, at, bytes});
        put(rt, at, value);
        return;
    }
    // Full: the new size is one element, or twice the old one.
    const u32 old_begin = word(rt, self);
    const u32 size = (end - old_begin) >> 2;
    if (size == kMaxItems) (void)call(rt, ctx, kThrowLength, {kLengthError});
    u32 capacity_bytes;
    if (size == 0u) {
        capacity_bytes = 4u;
    } else {
        const u32 doubled = size << 1;
        capacity_bytes = doubled < size ? 0xFFFFFFFCu : doubled << 2;
    }
    pool_init(rt, ctx, word(rt, once_slot));
    const u32 blocks = ((capacity_bytes + 7u) >> 3) + 1u;
    ctx.gpr[19] = blocks;   // s3
    const u32 fresh = allocate_blocks(rt, ctx, blocks, frame.at(4u));
    ctx.gpr[17] = fresh;    // s1
    // The head [begin, at) goes first and *value right behind it; the tail [at, end) follows.
    ctx.gpr[2] = call(rt, ctx, copy_words, {old_begin, at, fresh, frame.at(2u)});
    const u32 insert_at_pointer = ctx.gpr[2];
    if (insert_at_pointer != 0u) put(rt, insert_at_pointer, word(rt, value_at));
    const u32 new_end = call(rt, ctx, copy_words, {at, word(rt, self + 4u), insert_at_pointer + 4u, frame.at(1u)});
    ctx.gpr[19] = new_end;  // s3
    (void)call(rt, ctx, walk, {word(rt, self), word(rt, self + 4u), frame.at(0u), frame.at(1u)});
    // The old storage goes back to the pool when there was one.
    const u32 old_storage = word(rt, self);
    ctx.gpr[16] = old_storage;  // s0
    if (old_storage != 0u) free_block(rt, ctx, old_storage);
    ctx.gpr[2] = fresh + capacity_bytes;
    put(rt, self + 8u, ctx.gpr[2]);
    put(rt, self + 4u, new_end);
    put(rt, self, fresh);
}

// 0x08BB35FC (a0 = vector, a1 = position, a2 = value): the pool-allocator insert.
void func_08BB35FC(Runtime &rt, AllegrexContext &ctx) { insert_at(rt, ctx, 0x08BB1CACu, 0x08BB1C1Cu, kOnceSlot); }

// 0x08BB4C74: the same body for the other instantiation (its helpers are 0x08BB1B5C / 0x08BB1B4C, its once slot
// 0x08CB6064).

} // namespace
} // namespace new_batch_main_261
} // namespace gt::game

#define MAIN261(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_261::name, gt::ReturnKind::kind)
MAIN261(0x08BB35FCu, func_08BB35FC, Word);

// ---- from main-session batch 262 (draft 6d82a59250c9ad643ac3b2a7d1339a389aa09635) ----
// New batch (main session 262): four functions no route gives useful inputs for, written by hand from `tools.disasm` as
// structured C. Draft batch: not yet verified by the differential gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_262 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The batch files' convention: a guest call whose result becomes v0.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// 0x08B5E2CC (this = a0, a1 = a value stored at this + 0): fills a registry descriptor and installs the fourteen table
// entries it points at. this + 0 is the value, +4 / +0xC are the two 14-word tables, +8 is the count 14 and +0x10 the second
// table object (0x08D726C0) whose 1 fixed word and 5 zero words are filled here. Each table object below is a
// {vtable, flag = 1, one-word payload} triple whose payload gets its own initialiser (0x08B64CD0, 0x08B647C0, 0x08B60DB8,
// 0x08B60910, 0x08B60C98, 0x08C5E298, 0x08C5DC90) and is then registered by 0x08B5D3DC(this, key, object). v0 = the address of
// the last table slot written (0x08D72644 + 4 * the last key's index), v1 = 0x08D721DC.

// The 0x08961928 family's control block: a mutex-protected count at +0x2C (0x08B34888 locks, 0x08B34B24 unlocks), a flag
// byte at +0x30 and a pair at +0x34 / +0x38 released when the count reaches zero.
void control_lock(Runtime &rt, AllegrexContext &ctx, u32 block) { call(rt, ctx, 0x08B34888u, {block}); }
void control_unlock(Runtime &rt, AllegrexContext &ctx, u32 block) { call(rt, ctx, 0x08B34B24u, {block}); }
void control_ref_up(Runtime &rt, AllegrexContext &ctx, u32 block) {
    control_lock(rt, ctx, block);
    put(rt, block + 0x2Cu, word(rt, block + 0x2Cu) + 1u);
    control_unlock(rt, ctx, block);
}
// A counted body's two counters: the strong count at +4 (at zero the virtual slot 8 runs) and the weak count at +8 (at zero slot 0xC).
void release_body(Runtime &rt, AllegrexContext &ctx, u32 body) {
    if (body == 0u) return;
    const u32 strong = word(rt, body + 4u);
    put(rt, body + 4u, strong - 1u);
    if (strong != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, body) + 8u), {body});
    const u32 weak = word(rt, body + 8u);
    put(rt, body + 8u, weak - 1u);
    if (weak != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, body) + 0xCu), {body});
}
// The control block's teardown once its count is zero: the pair at +0x34 / +0x38 is taken (virtual slot 0 on the +0x38
// object with a1 = the +0x34 one), then the block is destroyed (0x08B28BE0) and freed (0x08B64654).
void destroy_control(Runtime &rt, AllegrexContext &ctx, u32 block) {
    control_unlock(rt, ctx, block);
    const u32 first = word(rt, block + 0x34u);
    const u32 second = word(rt, block + 0x38u);
    if (second != 0u) {
        call_indirect(rt, ctx, word(rt, word(rt, second)), {second, first});
        put(rt, block + 0x38u, 0u);
        put(rt, block + 0x34u, 0u);
    }
    call(rt, ctx, 0x08B28BE0u, {block});
    call(rt, ctx, 0x08B64654u, {block});
}
// Decrement the count under the lock; at zero take the pair and the flag's handler, unlock, then release the pair's two
// objects through their virtual slots (8 on the first, 0 on the second with a1 = the first).
void release_under_lock(Runtime &rt, AllegrexContext &ctx, u32 block) {
    u32 first = 0u, second = 0u;
    put(rt, block + 0x2Cu, word(rt, block + 0x2Cu) - 1u);
    if (word(rt, block + 0x2Cu) == 0u) {
        const u32 pair = word(rt, block + 0x34u);
        if (pair != 0u) {
            second = word(rt, block + 0x38u);
            first = pair;
            put(rt, block + 0x34u, 0u);
            put(rt, block + 0x38u, 0u);
        }
        if (rt.memory().load8(block + 0x30u) != 0u) call(rt, ctx, 0x08B34C1Cu, {block});
    }
    control_unlock(rt, ctx, block);
    if (first != 0u) call_indirect(rt, ctx, word(rt, word(rt, first) + 8u), {first});
    if (second != 0u) call_indirect(rt, ctx, word(rt, word(rt, second)), {second, first});
}

// 0x08961928: the counted-pointer constructor the hot batches call (a0 = a four-word source, a1 / a2 / a3 / t1 stored into the
// new object, t0 = a holder whose word is the control block, t2 = a {control block, counted body} pair). A 0x6C-byte object is
// allocated (0x08B62344) and set up (0x08AC0640), the block and body from t2 are taken (their counts raised), the source's four
// words are copied (the counted ones at +4 / +0xC raised), 0x08B93CD8 builds the 0x50 field from a1, then the block's count is
// raised four times, a 0x14-byte holder {vtable 0x08CC6910, 1, 1, the object, the block} is allocated and the temporary
// references are dropped. v0 is not a return value (Void): the asm ends on whichever release path ran.

// 0x0890A78C: the same counted-pointer constructor shape as 0x08961928 with a larger object (a0 = a value stored at +0x40,
// a1 = the four-word source, a2 = the argument the +0x54 field is built from, a3 = a flag byte that builds the +0x64 pointer,
// t0 = a holder whose word is the control block, t1 = the {control block, counted body} pair). A 0x14C-byte object is allocated
// and set up (0x08AC0640), the block and body counts are raised, the source's four words are copied to +0x44 (the two counted
// ones at +4 / +0xC raised), 0x08B93CD8 builds the +0x54 field, the +0x64 field comes from 0x0890AD68 when the flag is set, the
// block's count is raised four times, the 0x14-byte holder is built, the temporary references are dropped (0x08BD668C and
// 0x08BD7974 are this family's counted assign and dispose), and the body's count is dropped last. v0 is not a return value (Void).

// The noreturn cleanup chain at 10C4 and the code the original falls into at 08B640E4 (kept so a returning callee still
// writes what the original writes; there a0 is whatever 0x08B6459C leaves, approximated by the 0x08B64564 result).
u32 terminate_08B63BEC(Runtime &rt, AllegrexContext &ctx, u32 table) {
    call(rt, ctx, 0x08B62198u, {table + 0x30u});
    const u32 left = call(rt, ctx, 0x08B64564u, {word(rt, table + 0x0Cu)});
    call(rt, ctx, 0x08B62198u, {left});
    call(rt, ctx, 0x08B6459Cu, {});
    call(rt, ctx, 0x08B62260u, {});
    call(rt, ctx, 0x08B75710u, {left});
    put(rt, left, 0x08CDC070u);
    return 0x08CDC070u;
}
// The three calls that end a good scan (08B63DF0): the status words 0x18 / 0x24 and the +0x4D0 word, then 7.
u32 success_08B63BEC(Runtime &rt, AllegrexContext &ctx, u32 self, u32 table, u32 start, u32 frame) {
    call(rt, ctx, 0x08B729C4u, {self, 4u, table + 0x30u});
    call(rt, ctx, 0x08B729C4u, {self, 5u, start});
    call(rt, ctx, 0x08B72A1Cu, {self, word(rt, frame + 0x34u)});
    return 7u;
}

// The tail of 0x08B63BEC from 08B63DAC: flags (a1) bit 0 selects the "parsed" result (6, after writing the five words at
// table + 0x18), bit 3 and the two marker words pick the terminate paths, and mode (v1) says how the scan ended (1 = the
// input ran out, 2 = the growth path emptied the buffer, 3 = the growth path failed). Returns v0: 6, 7 or 8.
u32 finish_08B63BEC(Runtime &rt, AllegrexContext &ctx, u32 frame, u32 self, u32 table, u32 flags, u32 second, u32 third,
                    u32 start, u32 mode, u32 limit) {
    if ((flags & 1u) != 0u) {
        if (mode == 2u) return 8u;
        if (word(rt, frame + 0x3Cu) == 0u) return 6u;
        put(rt, table + 0x18u, start);
        put(rt, table + 0x20u, word(rt, frame + 0x38u));
        put(rt, table + 0x24u, word(rt, frame + 0x34u));
        put(rt, table + 0x1Cu, limit);
        put(rt, table + 0x28u, word(rt, frame));
        return 6u;
    }
    const bool markers = (flags & 8u) == 0u && second == 0x432B2B00u && third == 0x474E5543u;
    if (mode == 1u) {
        if (!markers) call(rt, ctx, 0x08B6459Cu, {});
        return terminate_08B63BEC(rt, ctx, table);
    }
    if (static_cast<s32>(start) < 0) {
        if (markers) {
            // A last decode into the frame's descriptor before the good-scan tail.
            (void)call(rt, ctx, 0x08B637DCu, {self, word(rt, frame + 0x38u), frame + 0x14u});
            put(rt, table + 0x24u, call(rt, ctx, 0x08B63574u, {rt.memory().load8(frame + 0x28u), self}));
        } else {
            call(rt, ctx, 0x08B645CCu, {});
            call(rt, ctx, 0x08B6459Cu, {});
            return terminate_08B63BEC(rt, ctx, table);
        }
    }
    return success_08B63BEC(rt, ctx, self, table, start, frame);
}

// 0x08B63BEC (a0 = 1, a1 = a flags word, a2 / a3 = two marker words, t0 = a 0x30-byte header whose +0x18..+0x28 words carry the
// scan state, t1 = the stream object). A parse driver: 0x08B72A24 / 0x08B72A14 / 0x08B72A1C read and write the stream's
// +0x4D4 / +0x4D0 words, 0x08B637DC fills the 0x16-byte descriptor at sp + 0x14, 0x08B63574 and 0x08B636A0 decode one value
// into the four frame words at sp + 4 / 8 / 0xC / 0x10, and 0x08B63614 / 0x08B63648 do the same into a buffer. The scan
// loop walks the input until the descriptor's limit (sp + 0x24); when the output no longer fits, the growth path recomputes
// the limit from the descriptor and copies chunks. Returns 1 / 2 / 3 / 6 / 7 / 8 (v0 = v1); a0 != 1 returns 3 at once.
void func_08B63BEC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x80u);
    const u32 self = ctx.gpr[9];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 header = ctx.gpr[8];
    const u32 table = header - 0x30u;
    const u32 flags = ctx.gpr[5];
    const u32 frame_base = frame.at(0u);

    put(rt, frame.at(0u), header + 0x20u);
    put(rt, frame.at(0x40u), flags);
    if (ctx.gpr[4] != 1u) {
        ctx.gpr[2] = 3u;
        return;
    }

    u32 flag = 0u;
    if (second == 0x432B2B00u) flag = third == 0x474E5543u ? 1u : 0u;
    put(rt, frame.at(0x3Cu), flag);

    u32 mode = 1u, start = 0u, position = 0u, limit = 0u;
    if (flags == 6u && flag != 0u) {
        put(rt, frame.at(0x38u), word(rt, table + 0x20u));
        const u32 count = word(rt, table + 0x24u);
        put(rt, frame.at(0x34u), count);
        start = word(rt, table + 0x18u);
        mode = count != 0u ? 3u : 1u;
        ctx.gpr[2] = finish_08B63BEC(rt, ctx, frame_base, self, table, flags, second, third, start, mode, limit);
        return;
    }

    const u32 descriptor = call(rt, ctx, 0x08B72A24u, {self});
    if (descriptor == 0u) {
        ctx.gpr[2] = 8u;
        return;
    }
    put(rt, frame.at(0x38u), descriptor);
    position = call(rt, ctx, 0x08B637DCu, {self, descriptor, frame.at(0x14u)});
    put(rt, frame.at(0x1Cu), call(rt, ctx, 0x08B63574u, {rt.memory().load8(frame.at(0x28u)), self}));
    limit = call(rt, ctx, 0x08B72A14u, {self}) - 1u;

    // The scan loop: three decodes into the frame words, joined, until the descriptor's limit is reached.
    // (sp_loads lists the three `lw s0, 8(sp)` of the growth path: sp + 8 is the descriptor's second word, not a saved
    // register -- the C++ reads it as `word(rt, frame.at(8u))`.)
    for (;;) {
        if (!(position < word(rt, frame.at(0x24u)))) {
            put(rt, frame.at(0x34u), 0u);
            mode = 1u;
            start = 0u;
            limit = 0u;
            break;
        }
        start = rt.memory().load8(frame.at(0x29u));
        u32 value = call(rt, ctx, 0x08B63574u, {start, 0u});
        position = call(rt, ctx, 0x08B636A0u, {start, value, position, frame.at(4u)});
        start = rt.memory().load8(frame.at(0x29u));
        value = call(rt, ctx, 0x08B63574u, {start, 0u});
        position = call(rt, ctx, 0x08B636A0u, {start, value, position, frame.at(0x10u)});
        start = rt.memory().load8(frame.at(0x29u));
        value = call(rt, ctx, 0x08B63574u, {start, 0u});
        position = call(rt, ctx, 0x08B636A0u, {start, value, position, frame.at(0x0Cu)});
        position = call(rt, ctx, 0x08B63614u, {position, frame.at(8u)});
        const u32 used = word(rt, frame.at(0x14u)) + word(rt, frame.at(4u));
        if (limit >= used) {
            put(rt, frame.at(0x34u), 0u);
            mode = 1u;
            start = 0u;
            limit = 0u;
            break;
        }
        if (limit >= used + word(rt, frame.at(0x10u))) continue;

        // The growth path (08B63F28): the new limit comes from the descriptor, then chunks are copied.
        u32 extra = 0u;
        if (word(rt, frame.at(0x0Cu)) != 0u) extra = word(rt, frame.at(0x0Cu)) + word(rt, frame.at(0x18u));
        put(rt, frame.at(0x34u), extra);
        if (word(rt, frame.at(8u)) != 0u) limit = word(rt, frame.at(0x24u)) + word(rt, frame.at(8u)) - 1u;
        else limit = 0u;
        if (extra == 0u) {
            ctx.gpr[2] = 8u;
            return;
        }
        if (limit == 0u) {
            mode = 2u;
            start = 0u;
            break;
        }
        u32 scan = 0u;
        if ((flags & 8u) == 0u && second == 0x432B2B00u && third == 0x474E5543u) scan = word(rt, table);
        put(rt, frame.at(0x30u), 0u);
        for (;;) {
            start = call(rt, ctx, 0x08B63648u, {limit, frame.at(8u)});
            (void)call(rt, ctx, 0x08B63648u, {start, frame.at(0x0Cu)});
            const u32 chunk = word(rt, frame.at(8u));
            if (chunk == 0u) {
                put(rt, frame.at(0x30u), 1u);
            } else if (static_cast<s32>(chunk) < 0) {
                if (scan == 0u) {
                    (void)call(rt, ctx, 0x08B63614u, {word(rt, frame.at(0x20u)) - chunk - 1u, frame.at(0x10u)});
                    if (word(rt, frame.at(0x10u)) >= 1u) {
                        // falls to the shared room test below
                    } else {
                        start = word(rt, frame.at(8u));
                        mode = 3u;
                        break;
                    }
                } else {
                    const u32 rejected = call(rt, ctx, 0x08B63A70u, {frame.at(0x14u), scan});
                    if (((rejected ^ 1u) & 0xFFu) != 0u) {
                        start = word(rt, frame.at(8u));
                        mode = 3u;
                        break;
                    }
                }
            } else {
                const u32 made = call(rt, ctx, 0x08B63924u, {frame.at(0x14u), chunk});
                if (made == 0u) {
                    start = word(rt, frame.at(8u));
                    mode = 3u;
                    break;
                }
                if (scan != 0u) {
                    const u32 accepted = call(rt, ctx, 0x08B639D8u, {made, scan, frame_base});
                    if (accepted != 0u) {
                        start = word(rt, frame.at(8u));
                        mode = 3u;
                        break;
                    }
                }
            }
            const u32 room = word(rt, frame.at(0x0Cu));
            if (room == 0u) {
                if (word(rt, frame.at(0x30u)) != 0u) {
                    mode = 2u;
                    start = 0u;
                } else {
                    ctx.gpr[2] = 8u;
                    return;
                }
                break;
            }
            limit = start + room;
        }
        break;
    }

    ctx.gpr[2] = finish_08B63BEC(rt, ctx, frame_base, self, table, flags, second, third, start, mode, limit);
}

} // namespace
} // namespace new_batch_main_262
} // namespace gt::game

#define MAIN262(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_262::name, gt::ReturnKind::kind)
MAIN262(0x08B63BECu, func_08B63BEC, Word);
// v0 of these two is whichever release path ran last (their callers ignore it): Void, so the harness compares the memory and
// the callee-saved registers only.

// ---- from main-session batch 263 (draft 3cf031044b91722a72470c6fa8fda2437b02cf1a) ----
// New batch 263 (main session): functions no src/ file defines or registers. Written by hand from
// `tools/disasm` as structured C; this is a draft batch, not yet verified by the differential gate.
// The helpers below cover the shapes the batch repeats: the reference-counted string an item's virtual
// hands a caller (length at -0xC, count at -4), the copy-into-owned-buffer sequence the format object
// repeats three times, and the character-conversion tables it builds from a converter's state.
//
// Left undrafted on purpose (unsure, and this batch cannot run the gate to iterate on them):
//   0x08B6300C - the pattern matcher's merge state machine. Its per-entry call fills a five-word
//                record and its result code (2/6 final, others merged by OR-ing the output words,
//                with fallbacks through the subject's virtual 0x20 whose a2 argument the asm leaves
//                as whatever the previous call left there), so a confident draft needs the gate.
// The three bodies here are 0x08C661C4 (the format item), 0x08C6A6C0 (its two-stream scanner) and
// 0x089F1AEC (the spline blend).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_263 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them: a 64-bit result's high word (v1) and a
// callee that never sets v0 both matter to the code after the call.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// ---- the reference-counted strings the format-object virtuals return -------------------------------
// Such a string is a character pointer whose header sits 0xC bytes before it: the length at -0xC, the
// reference count at -4. 0x08D77498 is the shared empty string: its header must not be touched.
constexpr u32 kEmptyString = 0x08D77498u;
// 0x08B60804 is the counted release (returns the old count); a count that reaches zero frees through
// 0x08C74884. `scratch` is the frame byte the original hands the deallocator.
void release_reference_counted(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, u32 characters, u32 scratch) {
    const u32 header = characters - 0xCu;
    if (header == kEmptyString) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {characters - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08C74884u, {header, frame.at(scratch)});
}

// ---- 0x08C661C4: the format object's strings and conversion tables ----------------------------------
// The item's virtual `slot` hands a reference-counted string. The object keeps its length at
// `length_field` and an owned copy (0x08B6461C is operator new) at `pointer_field`; `chars_scratch` and
// `copy_scratch` are the frame offsets of the two locals the virtual fills (the original asks twice).
u32 copy_item_string(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, u32 self, u32 item, u32 slot,
                     u32 length_field, u32 pointer_field, u32 chars_scratch, u32 copy_scratch,
                     u32 first_free_scratch, u32 second_free_scratch) {
    const u32 chars_holder = frame.at(chars_scratch);
    u32 method = word(rt, word(rt, item) + slot);
    call_indirect(rt, ctx, method, {chars_holder, item});
    const u32 characters = word(rt, chars_holder);
    const u32 length = word(rt, characters - 0xCu);
    put(rt, self + length_field, length);
    release_reference_counted(rt, ctx, frame, characters, first_free_scratch);
    const u32 buffer = call(rt, ctx, 0x08B6461Cu, {length});
    method = word(rt, word(rt, item) + slot);
    call_indirect(rt, ctx, method, {frame.at(copy_scratch), item});
    call(rt, ctx, 0x08C742D8u, {frame.at(copy_scratch), buffer, length, 0u});
    release_reference_counted(rt, ctx, frame, word(rt, frame.at(copy_scratch)), second_free_scratch);
    put(rt, self + pointer_field, buffer);
    return length;
}

// One conversion table: the converter 0x08B64C80 answers has a state byte at +0x1C.
//   1 - it already built its table: copy the static one (0x08B79A88 is memcpy).
//   0 - its table is the identity: fill converter+0x1D from 0x08B36170's comparison of 0..255, and mark
//       it 2 when the identity is not what the converter produced.
//   other - translate the static table through the converter's virtual 0x1C.
void build_conversion_table(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, u32 converter, u32 source,
                            u32 end, u32 destination) {
    const u32 state = rt.memory().load8(converter + 0x1Cu);
    if (state == 1u) {
        call(rt, ctx, 0x08B79A88u, {destination, source, end - source});
        return;
    }
    if (state == 0u) {
        const u32 identity = frame.at(0x24u);
        for (u32 i = 0; i < 0x100u; ++i) rt.memory().store8(identity + i, static_cast<u8>(i));
        call_indirect(rt, ctx, word(rt, word(rt, converter) + 0x1Cu),
                      {converter, identity, frame.at(0x124u), converter + 0x1Du});
        rt.memory().store8(converter + 0x1Cu, 1u);
        if (call(rt, ctx, 0x08B36170u, {identity, converter + 0x1Du, 0x100u}) != 0u)
            rt.memory().store8(converter + 0x1Cu, 2u);
    }
    call_indirect(rt, ctx, word(rt, word(rt, converter) + 0x1Cu), {converter, source, end, destination});
}

// 0x08C661C4 (a0 = a 0x68-byte format object, a1 = the holder whose item table the object is built
// from): the item is table[08B5C2B0(0x08D77484)] of *(a1); its three virtual strings (0x10/0x14/0x18)
// become owned copies (pointer at +8/+14/+1C, length at +C/+18/+20), the first string's leading
// character sets the byte at +10, virtuals +8/+C fill the bytes at +24/+25, and the converter's tables
// fill +26 (36 bytes) and +4A (26 bytes). +64 is set on entry.

// 0x08C6A6C0 (a0 = context, a1 = the A stream descriptor {node, value}, a2 = the B stream
// descriptor, a3 = the format holder, t0 = the flags word, t1 = the out byte): when format->flags bit 0
// is clear the pair is compared by 08C699FC with a -1 status word and its 64-bit result is returned
// (*out = status != 0, flags |= 4 when status >= 2). Otherwise the format item
// (the 0x68 object 08C661C4 builds, cached at word[format+0x6C] by the key 08B5C2B0(0x08D77484)) is
// matched against the A stream: its tables at +0x14/+0x18 and +0x1C/+0x20 are compared character by
// character (index +0), consuming A one character at a time through the stream's virtuals 0x24 (next)
// and 0x28 (advance), while the B stream only marks whether it still has characters. Returns
// {A node or 0, A character or -1}; flags gets bit 2 when a table of the item was fully matched and
// bit 1 when the scan stopped for a reason other than a plain mismatch, *out tells whether the full
// item matched.

// 0x089F1AEC (a0 = a spline object, a1 = the output object, a2 = an optional frame object, a3 = a
// rectangle of four floats). A 4x4 matrix is blended from the four 4-float rows 08A06E94(a0, 0/1/3/2)
// with the reciprocal weights of 089A8B68/089A8B80 (the words at a0+0xB0 + 0x40*[a0+0xB4], +8 and
// +0xC); 089641C4 copies 16 bytes (four VFPU lanes) per row into sp+0x80..0xBF. The output object's
// virtual 0x10C receives that matrix with the rectangle (sp+0x40) and the frame record (sp+0x60, built
// only when a2 answers through its virtual 0x28). When 08A06EC8(a0) is not 4 the matrix is the
// identity of sixteen 1.0 floats. The a2==0 / virtual-0x28==0 path calls 089A8C24(a1) first.

} // namespace
} // namespace new_batch_main_263
} // namespace gt::game

#define MAIN263(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_263::name, gt::ReturnKind::kind)

// ---- from main-session batch 264 (draft 523222f673cc13cf16d454fe4c194ae3206ca837) ----
// New batch 264 (main session): functions the extra seeds made emittable. Written by hand from `tools/disasm` as
// structured C; this is a draft batch, not yet verified by the differential gate.
//
// Left out of this batch: 0x08AED0AC (the state machine of the same 0x08D41AD0 channel block, 375 instructions:
// an outer loop whose body re-enters at two labels after the two top calls, eight report paths and ~25 guest
// calls). Its per-path register and delay-slot bookkeeping (s0 from the block's +0x2088 counter, the min in a
// branch delay, the results of 0x08C7AB18/0x08C7AB60/0x08C7AB68 folded into the next call) could not be
// transcribed with the confidence a draft needs, so it is left for a session that can run the gate on it.
//
// The two shapes here share nothing: 0x08951944 is a pure decoder (no calls, no frame: bytes in, floats out), and
// 0x08AEAD38 is the setup routine of the 0x08D41AD0 sound/sequence channel block (memset, eighteen config reads
// through 0x08AD8960, a mode switch that installs two handler words, a lock/reader pair, two pool lists and the
// two calls that wire them). Existing siblings of the second one (func_08AE6620, func_08AE2D38, func_08AEB3BC — the
// teardown of the same block) fixed the block layout and the cleanup order used below.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_264 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s16 = std::int16_t;

// A guest call that leaves v0 / v1 / f0 in the registers as the original's jal does.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order);
// what the callee does not take stays as the caller had it in ctx.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments).v0;
}

// The callee-saved registers s0-s7 and fp as they were at entry: the original restores them in its epilogue.
class SavedRegs {
public:
    explicit SavedRegs(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_fp_ = ctx.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    AllegrexContext &ctx_;
    std::uint32_t saved_[8]{};
    std::uint32_t saved_fp_{};
};

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void store8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
s32 half_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }

// 0x08951944 (a0 = output record, a1 = parameter bytes): decodes the parameter block into the record.
//   +0 / +4 / +8       flag byte and two durations: 0 / 1.0f / 1.0f when byte 3 or 4 is zero or byte 4 is 100,
//                      else 1 and byte 4 / byte 5 over 100.0f (byte 5 zero gives 0.0f).
//   +0xC..+0x14        flag byte = 1 when byte 2 and the halfword at byte 0 are non-zero; byte 0 / 1000.0f and
//                      byte 1 * 0.027777778f in that case, else 0 / 0.
//   +0x18, +0x1C..     boolean "any of bytes 6, 7, 8" and three of them * 0.01f.
//   +0x28..+0x3C       byte 9 as a word, byte 0xC as a word, bytes 0xA/0xB/0xD/0xE * 0.01f.
//   +0x40..+0x4C       byte 0x2E != 0 and bytes 0x2F/0x30/0x31 as floats.
//   +0x50..+0x60       byte 0x33 != 0 && byte 0x32 < byte 0x33; 0x32 and 0x33 * 10 * 0.2777778f; 0x34 and 0x35
//                      * 10 * 0.1f.
//   +0x64..+0x6C       byte 0x3A < byte 0x3B and both * 10 * 0.2777778f.
//   +0x70..+0x9C       two passes of two 12-byte angle blocks: the signed halfwords at 0x3C/0x40/0x44 and
//                      0x48/0x4C/0x50 of a1 + 2 * i, * pi/180, at a0 + 4 * i (stride 4, each pass fills 0x14 bytes).
//   +0xA0..+0xAC       byte 0xD of the table at 0x08CDE230, 15 * its byte 0x1CF * pi/180, and bytes 0x10/0x11
//                      (each != 0) as two booleans.
//   +0xAC..+0x10C      two passes of a fixed 10-field block (bytes 0x14..0x26 * 0.01f, stride 8 and 4); bytes
//                      0x28 and 0x2A of a1 + i: 0 writes 0 / 0 / 0 at +0xFC/+0x100/+0x108, else 1 at +0xFC and
//                      (below 100: * 0.01f, else its 1.0f) at +0x100, and at +0x108 the same for byte 0x2A, the
//                      +0x28 value when byte 0x2A is zero and 1.0f when it is 100 or more.
// v0 is the byte 0x2A of the last pass (0 when that pass's byte 0x28 was zero), the value the asm leaves at the one
// jr $ra. No frame and no callee.

// 0x08AEAD38 (a0 = config reader handle, s2): sets up the sound/sequence channel block at 0x08D41AD0.
//   memset(block, 0, 0x568); block[0x4BC] = 1; block[0xC] = -1; then eighteen config reads through
//   0x08AD8960(handle, name, &slot, default): block +4 (1), block +0x498 (1), the six frame slots
//   +0 (0x20), +4 (0x100), +8 (2), +0xC (2), +0x10 (word(+8) + word(+0xC)), +0x14 (0x10), block +0x46C (0x10),
//   +0x468 (1), +0x470 (0xC000), +0x4D8 (0), the frame slots +0x18 (0), +0x1C (1), +0x20 (0), +0x24 (0x3E8),
//   block +0x94 (0x4000) and block +0x30 (1).
//   The count in frame +0 clamped to [0, 0x20] (negative: 0) goes back into frame +0; block +0x30 picks the two
//   handler words installed at block +0x38 / +0x34: 0 -> 0x08AEB734 / 0x08AEB6EC, 1 -> 0x08AEB798 / 0x08AEB770,
//   2 -> 0x08CE3CE4 / 0x08AEB7BC, any other value leaves them and only logs (0x08C91F34).
//   Then 0x08C7A998(0x08C91F34, 0, 1, 1, 0) -> block +0xC (negative: log 0x20F and tear down),
//   0x08C7AAB0(block +0x10, 0x08C91F40, 0, 0, 0) (negative: log 0x21C and tear down), 0x08B82420(block +0x10)
//   (non-zero: log 0x224 and tear down), 0x08AEE144(handle, word(frame +0x10)) and 0x08AEFE28(handle) (0: tear
//   down); when block +0x498 is set 0x08AEC030(word(+0), word(+4)) must answer non-zero, when block +0x468 is set
//   0x08AEC8C0(word(+0x10), word(+8), word(+0xC)) must too (0: tear down).
//   A non-zero 0x08AE942C(word(+0) + word(+0x10)) starts the two lists: 0x08AE9540(0, 0, 0),
//   0x08AE9540(1, first, word(+0)), 0x08AE9540(2, first + second, word(+0x10)); 0x08AE9528() goes to block +0x9C
//   and 0x08AE9534() to block +0xA0; every record of the block +0x9C list (step 0x38) gets its index at +0x1C.
//   block +0x90 = 1, then 0x08AEBB0C(block +0x3C, 1, word(+0x14), word(+0x18), word(+4), 0, 0) and, when
//   frame +0x20 is set, 0x08AEBB0C(block +0x64, 2, word(+0x14), word(+0x1C), 0x400, 1, word(+0x24)); the former
//   installs one list (block +0x8C = 1), the latter two (block +0x8C = 2). Either way block[0] = 1 and v0 = 1.
//   Every failure tears the block down (the order func_08AEB3BC uses) and answers 0.
// Branches the gate must still prove: the two handler pairs other than the one the config selects, the three
// report sites and the whole teardown path — none of them run for a fresh block with the default config.

} // namespace
} // namespace new_batch_main_264
} // namespace gt::game

#define NEW264(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_264::name, gt::ReturnKind::kind)

// ---- from main-session batch 265 (draft 3b8e35d7645e1b1c5e71de51f1189eb5e2ed2ce8) ----
// Main-session batch 265: three large functions nothing verified yet, written by hand from `tools.disasm`.
// The input shapes that make them comparable live in config/synth_shapes.txt (see the comment of each function).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_265 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

inline std::int32_t signed_of(u32 value) { return static_cast<std::int32_t>(value); }

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose v0 / v1 / f0 stay in the registers like the original's (the next instruction reads them).
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal; v0 = its result.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> floats) {
    return call_keep(rt, ctx, address, arguments, floats);
}
// `lw v1, 0(object); lw v0, slot(v1); jalr v0`: a virtual call with a0 = the first argument.
inline u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// A MIPS `ins` (bitfield insert): `width` bits of `source` go to bit `at` of `value`.
inline u32 insert_bits(u32 value, u32 source, unsigned at, unsigned width) {
    const u32 mask = ((1u << width) - 1u) << at;
    return (value & ~mask) | ((source << at) & mask);
}

// 0x08B15FC0 (a1 = loader object; a0 unused) -> bytes used, or a negative result: reads a PAC archive through the open
// file of `loader` (+8 the file, its +0xB8 the size; +0x20 set = busy (error 0xF); +0x28 the buffer, owned when +0x30
// is set; +0x2C the size limit; +0x34 / +0x38 the per-type entry handlers; +4 the error code). func_08B0C880 starts the
// read; the 0x40-byte header (func_08B0C650 with a {buffer, size} pair) holds the magic: "LCAP" / "BCAP" (0x4C434150 /
// 0x50414342) are taken as they are, 0x42434150 / 0x5041434C are byte-swapped (the words +4..+0x3C, then each 16-byte
// entry's two halfwords and three words). The table of entries (count at +0x3C) follows; an owned buffer is grown with
// func_08B35E3C to the size the handlers (or each entry's alignment + size) ask for. Each entry is then reached by
// func_08B158BC (skipping the gap to its file offset +8) and either given to its handler (`fn(context, &state, 0)`) or
// read at the next position aligned to its +4 (that position written to its +8). func_08B0C71C closes; errors are 3
// (memory), 5 (reading), 2 (magic), 0x15 (a handler). Drafted first (unregistered, compared 0) inside hot_batch_134.cpp
// and re-read against the asm here.
// Shape: a1 -> {+8 file, +0x20 busy, +0x28 buffer, +0x2C limit, +0x30 owned, +0x34 handlers, +0x38 typed, +4 error}.
void func_08B15FC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 loader = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const auto at = [&](u32 offset) { return frame.at(offset); };
    const auto close = [&] { (void)call_set(rt, ctx, 0x08B0C71Cu, {loader}); };
    const auto fail = [&](u32 code) {
        close();
        put(rt, loader + 4u, code);
        ctx.gpr[2] = 0xFFFFFFFFu;
    };
    const auto read = [&](u32 pair) { return call_set(rt, ctx, 0x08B0C650u, {loader, pair}); };
    // A byte-reversed word at `address` (the asm's lwl / lwr / wsbw / sb sequence, any alignment).
    const auto reverse = [&](u32 address) {
        const u32 b0 = byte_u(rt, address);
        const u32 b1 = byte_u(rt, address + 1u);
        const u32 b2 = byte_u(rt, address + 2u);
        const u32 b3 = byte_u(rt, address + 3u);
        put8(rt, address, b3);
        put8(rt, address + 1u, b2);
        put8(rt, address + 2u, b1);
        put8(rt, address + 3u, b0);
    };
    // The handler record an entry picks: the first halfword indexes a table of 8-byte records when +0x38 is signed
    // non-negative, else every entry shares the record at +0x34.
    const auto handler_of = [&](u32 entry) -> u32 {
        if (signed_of(word(rt, loader + 0x38u)) >= 0) return word(rt, loader + 0x34u) + half_u(rt, entry) * 8u;
        return word(rt, loader + 0x34u);
    };

    const u32 file_size = word(rt, word(rt, loader + 8u) + 0xB8u);
    u32 limit = word(rt, loader + 0x2Cu);
    u32 buffer = word(rt, loader + 0x28u);
    if (word(rt, loader + 0x20u) != 0u) {
        put(rt, loader + 4u, 0xFu);
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    if (file_size < limit) limit = file_size;
    u32 result = call_set(rt, ctx, 0x08B0C880u, {loader, file_size});
    if (signed_of(result) < 0) {
        ctx.gpr[2] = result;
        return;
    }
    u32 size = limit;
    if (buffer == 0u) {
        size = limit != 0u ? limit : 0x800u;
        buffer = call_set(rt, ctx, 0x08B15490u, {size, 1u});
        if (buffer == 0u) {
            fail(3u);
            return;
        }
        put8(rt, loader + 0x30u, 1u);
    }
    put(rt, loader + 0x28u, buffer);
    if (size < 0x40u) {
        fail(3u);
        return;
    }
    put(rt, at(0x14u), 0x40u);
    put(rt, at(0x10u), buffer);
    result = read(at(0x10u));
    if (signed_of(result) < 0) {
        close();
        put(rt, loader + 4u, 5u);
        ctx.gpr[2] = result;
        return;
    }
    u32 native = 0u;
    // The asm builds the two byte-swapped magics with `lui $a0, 0x5041` and `ori $v0, $a0, 0x4342 / 0x434C`.
    const u32 swapped_low = 0x50410000u | 0x4342u;
    const u32 swapped_high = 0x50410000u | 0x434Cu;
    const u32 magic = word(rt, buffer);
    if (magic == 0x4C434150u || magic == swapped_low) {
        native = 1u;
    } else if (magic == 0x42434150u || magic == swapped_high) {
        for (u32 offset = 4u; offset < 0x40u; offset += 4u) reverse(buffer + offset);
    } else {
        fail(2u);
        return;
    }
    // s5 in the asm: where the entry count is read (the buffer, or its grown copy).
    u32 header = buffer;
    const u32 table = word(rt, buffer + 0x3Cu) * 16u;
    const u32 start = table + 0x40u;
    if (size < start) {
        if (byte_u(rt, loader + 0x30u) == 0u) {
            fail(3u);
            return;
        }
        buffer = call_set(rt, ctx, 0x08B35E3Cu, {buffer, start});
        header = buffer;
        if (buffer == 0u) {
            fail(3u);
            return;
        }
        size = start;
    }
    put(rt, at(8u), buffer + 0x40u);
    put(rt, at(0xCu), table);
    result = read(at(8u));
    if (signed_of(result) < 0) {
        close();
        put(rt, loader + 4u, 5u);
        ctx.gpr[2] = result;
        return;
    }
    const auto count = [&] { return word(rt, header + 0x3Cu); };
    // A foreign-endian archive: each entry's two halfwords and three words are byte-reversed.
    if (native == 0u) {
        for (u32 index = 0u; index < count(); ++index) {
            const u32 entry = buffer + 0x40u + index * 16u;
            const u32 b0 = byte_u(rt, entry);
            const u32 b1 = byte_u(rt, entry + 1u);
            const u32 b2 = byte_u(rt, entry + 2u);
            const u32 b3 = byte_u(rt, entry + 3u);
            put8(rt, entry, b1);
            put8(rt, entry + 1u, b0);
            put8(rt, entry + 2u, b3);
            put8(rt, entry + 3u, b2);
            reverse(entry + 4u);
            reverse(entry + 8u);
            reverse(entry + 0xCu);
        }
    }
    // The handlers' state, at frame +0x18: loader, size, buffer, native, index, entry, cursor, room, file position.
    const u32 state = at(0x18u);
    if (byte_u(rt, loader + 0x30u) != 0u) {
        put(rt, at(0x30u), 0u);
        put(rt, at(0x34u), 0u);
        put(rt, at(0x1Cu), size);
        put(rt, at(0x18u), loader);
        put(rt, at(0x20u), buffer);
        put8(rt, at(0x24u), native);
        put(rt, at(0x38u), size);
        for (u32 index = 0u; index < count(); ++index) {
            const u32 entry = buffer + 0x40u + index * 16u;
            put(rt, at(0x28u), index);
            put(rt, at(0x2Cu), entry);
            const u32 handler = handler_of(entry);
            if (handler != 0u && word(rt, handler) != 0u) {
                if (signed_of(call_set(rt, ctx, word(rt, handler), {word(rt, handler + 4u), state, 1u})) < 0) {
                    fail(0x15u);
                    return;
                }
                continue;
            }
            put(rt, at(0x1Cu), word(rt, at(0x1Cu)) + word(rt, entry + 4u) + word(rt, entry + 0xCu));
        }
        const u32 needed = word(rt, at(0x1Cu));
        if (size != needed) {
            buffer = call_set(rt, ctx, 0x08B35E3Cu, {buffer, needed});
            header = buffer;
            size = needed;
            if (buffer == 0u) {
                fail(3u);
                return;
            }
        }
    }
    put8(rt, at(0x24u), native);
    put(rt, at(0x34u), size - start);
    put(rt, at(0x30u), buffer + start);
    put(rt, at(0x1Cu), size);
    put(rt, at(0x18u), loader);
    put(rt, at(0x20u), buffer);
    put(rt, at(0x38u), start);
    for (u32 index = 0u; index < count(); ++index) {
        const u32 entry = buffer + 0x40u + index * 16u;
        const u32 gap = word(rt, entry + 8u) - word(rt, at(0x38u));
        const u32 bytes = word(rt, entry + 0xCu);
        if (signed_of(call_set(rt, ctx, 0x08B158BCu, {loader, gap})) < 0) {
            fail(5u);
            return;
        }
        put(rt, at(0x28u), index);
        const u32 position = gap + word(rt, at(0x38u));
        put(rt, at(0x38u), position);
        put(rt, at(0x2Cu), entry);
        const u32 handler = handler_of(entry);
        if (handler != 0u && word(rt, handler) != 0u) {
            if (signed_of(call_set(rt, ctx, word(rt, handler), {word(rt, handler + 4u), state, 0u})) < 0) {
                fail(0x15u);
                return;
            }
        } else {
            const u32 mask = word(rt, entry + 4u) - 1u;
            const u32 cursor = word(rt, at(0x30u));
            const u32 placed = (mask + cursor) & ~mask;
            put(rt, entry + 8u, placed);
            const u32 room = word(rt, at(0x34u)) - (placed - cursor);
            put(rt, at(0x30u), placed);
            put(rt, at(0x34u), room);
            if (room < bytes) {
                fail(3u);
                return;
            }
            put(rt, at(0u), placed);
            put(rt, at(4u), bytes);
            if (signed_of(read(at(0u))) < 0) {
                fail(0x15u);
                return;
            }
        }
        put(rt, at(0x38u), bytes + word(rt, at(0x38u)));
    }
    const u32 used = word(rt, at(0x30u)) - buffer;
    const u32 closed = call_set(rt, ctx, 0x08B0C71Cu, {loader});
    if (signed_of(closed) >= 0) {
        ctx.gpr[2] = used;
        return;
    }
    ctx.gpr[2] = signed_of(used) >= 0 ? closed : used;
}

// 0x08961EF0 (a0 = source object, a1, a2, t0-t3 = arguments kept, a3 = pointer to the handle to hold, and a ninth
// argument on the caller's stack: a pointer to a {word, block} pair) -> nothing compared but v0. Builds a 0x88-byte
// object at fresh memory: func_08B62344 allocates it and func_08AC0640 constructs it, the vtable 0x08CC6868 and the
// word 0x08BE2F7C go to +0 and +0x30, the pair's words to +0x38 / +0x3C, the four words of a0 to +0x40..+0x4C,
// func_08B93CD8 fills +0x50 from a1 and a frame word, and a2 / t0-t3 are kept at +0x54..+0x64. Every object word that
// carries a reference (the pair's block, a0's +4 and +0xC values) has its counter at +4 bumped. The handle
// (word(*a3)) is locked, counted and unlocked four times (func_08B34888 / func_08B34B24 around the counter at +0x2C),
// with a 0x14-byte reference object (vtable 0x08CC68A8, counter 1, +0xC the new object, +0x10 the handle) built in
// between; then three releases run the same lock / count test: a count of zero destroys the held object through its slot
// 0 (func_08B28BE0, func_08B64654), otherwise the counter drops, the pending pair (+0x34 / +0x38) is taken and released
// through slots 8 and 0, the flag at +0x30 calls func_08B34C1C, and the handle is unlocked. func_08BE079C and
// func_08BE2F7C then finish the reference object, whose counters at +4 / +8 release through slots 8 / 0xC; the pair's
// block is released the same way last. The ninth argument is the caller's sp[0] (plus 0x50 here), so a synthetic case
// cannot make it valid: this body is faithful but only the routes that pass a real pair can compare it.
// Shape: a0 -> {+0 source word, +4 / +0xC referenced pointers, +8 word}; a3 points at the handle word (not expressible
// as a second register on the one shape line).

// 0x08B07048 (a0 = record table, a1 / a2 / a3 / t0 / t1 / t2 = arguments, f12 / f13 floats) -> the record count
// plus the three records written (count + 3, or count + 2 on the zero reach). func_08B06C10 answers how many 0x70-byte
// records the table already holds; the new ones are appended at base + count * 0x70. Each record gets three words at
// +0 / +4 / +8 (a vector func_08B0562C fills into the frame), 8-byte objects from func_08BAE2B0 at +0xC, +0x18, +0x24
// (built from func_08BAE264 / func_08BAE364 / func_08C4ACC8 over func_08BDCCB8 pairs, func_08C46D64 and the pointer
// func_08BAE2FC / func_08BAC428 wrap), the floats at +0x60..+0x68 (the caller's reach, func_08B061D8's answer, and
// -FLT_MAX or zero) and a packed word at +0x6C (0xFFF, then t1 and t2 in 10-bit fields). The third record's reach is
// the float at a2 + 4 * count + 0x30, negated when negative; when it is zero only a zero float at a2 + 4 * count + 0x3C
// is written and the count + 2 returned. func_08B06978 closes every record. Drafted from the asm with the recompiled
// unit read as a register-flow aid.
// Shape: a2 -> {+0xC / +0x18 / +0x20 / +0x24 handed to the helpers, +0x30 reach, +0x34..+0x3C per-record floats,
// +4 * count table}; a0 is the record table the appended records write (0x70 bytes each).

} // namespace
} // namespace new_batch_main_265
} // namespace gt::game

#define MAIN265(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_265::name, gt::ReturnKind::kind)
MAIN265(0x08B15FC0u, func_08B15FC0, Word);

