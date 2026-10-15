// Hot batch 83: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five routes with
// --decomp off; batch 82 = PR #203 excluded with --exclude-ref origin/decomp/hot-batch-82, and no address that #203 writes, registers or leaves unregistered is taken here). Before reading any function
// its address was searched for in src/ and in the branch of batch 82. The candidates are the reached singles ordered by calls on the five routes, without VFPU, without imports that the harness cannot
// replay (semaphores, the vblank wait, sasCore, threads, setjmp / longjmp), and without functions that are really several entry points; each was read by hand word by word with `tools/disasm.py` (branch
// targets and relocations resolved by the tool, every delay slot part of its branch's path).
//
//  - strings and containers of the engine: append (func_08B89054), range constructor (func_08B85E00), reverse find (func_08BFD224, func_08B7C9E4), substring (func_08B8D96C), representation allocator
//    (func_08B93B20), list / tree walkers and destructors (func_08BEE778, func_08BEEE28, func_08C1ED14, func_08BF913C, func_08A782D8), erase (func_08C01DFC), a sort of 16-bit values (func_08C49140);
//  - the heap's block list (func_08BE3B38, func_08BE40AC, func_08962918, func_089626C8), the thread record stack (func_08B77B1C), guarded statics and static-init holders (func_0890B2F4, func_08B5E0EC, func_08B6441C);
//  - maths: a lerp (func_08BB9130), a cosine by range (func_08B66524), quaternion rotation (func_08BAC284, func_08AFEAB8), a component-wise select (func_08C465EC), soft-double helpers (func_08B71A90, func_08B71328);
//  - counters, pools and small initialisers (func_08C3DB68, func_08C3E24C, func_08B1C820, func_08B52730, func_08B344B0, func_08C3C6D4, func_08B515FC, func_08B658FC, func_088A8B14, ...).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest
// copies from ctx. Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's; a callee that sets no v0 hands the previous one on).
//
// Compared only on an early exit or a side path (the comment of each function lists what was read from the asm only): func_08C30E0C (count 0 only), func_08A782D8 (empty source only),
// func_08B0B36C (the `*ok = 0` leave only; its success body was first read wrong in the v0 it returns), func_08C49140 (ranges of at most 16 values only), func_08B18A58 (flag clear only),
// func_08BEEE28 (the shared empty representation only), func_08B66524 (quadrant 1 only), func_089445D8 (answers 1 only), func_08ADBFE0 (finds an entry only).

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_83 {
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

inline float fbits(std::uint32_t bits) { return std::bit_cast<float>(bits); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline std::int16_t half_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
// lwl / lwr pair at `address` + 3 / + 0: the little-endian word that starts at an unaligned address.
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return static_cast<std::uint32_t>(rt.memory().load8(address)) | static_cast<std::uint32_t>(rt.memory().load8(address + 1u)) << 8 |
           static_cast<std::uint32_t>(rt.memory().load8(address + 2u)) << 16 | static_cast<std::uint32_t>(rt.memory().load8(address + 3u)) << 24;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B1C820(this, a1, lo, hi): after func_08B34D40(this + 4) a signed 64-bit amount (lo = a2, hi = a3) is stored at this[0x10] / this[0x14]: a positive amount as it is, a zero or negative
// one after being taken from the stored value (stored - amount, the high word minus the borrow `stored < difference`); an amount of lo != 0 with hi == 0 is stored as it is. func_08B34D80(this + 4) runs
// with both words already stored, and func_08B1C68C(this, a1, lo, hi) is the tail call.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): hi > 0 (stored as it is: `bgtz` to 08B1C888), hi == 0 with lo != 0 (`bnel s2`: the delay slot stores lo, then 08B1C88C stores hi), hi == 0 with lo == 0 (the subtraction of zero) and a borrow (stored < difference); every compared amount has hi < 0 and no borrow.
void func_08B1C820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t lo = ctx.gpr[6], hi = ctx.gpr[7];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_keep(rt, ctx, 0x08B34D40u, {self + 4u});
        ctx.gpr[4] = self + 4u;
        bool subtract;
        if (signed_of(hi) > 0) subtract = false;
        else if (hi != 0u) subtract = true;
        else subtract = (lo == 0u);
        if (subtract) {
            const std::uint32_t low = word(rt, self + 0x10u), high = word(rt, self + 0x14u);
            const std::uint32_t difference = low - lo;
            ctx.gpr[2] = low;
            ctx.gpr[3] = high;
            ctx.gpr[5] = low < difference ? 1u : 0u;
            lo = difference;
            hi = (high - hi) - ctx.gpr[5];
            ctx.gpr[6] = lo;
            ctx.gpr[7] = hi;
        }
        put(rt, self + 0x10u, lo);
        put(rt, self + 0x14u, hi);
        (void)call_keep(rt, ctx, 0x08B34D80u, {self + 4u});
    }
    ctx.gpr[4] = self;
    ctx.gpr[6] = lo;
    ctx.gpr[7] = hi;
    tail_call(ctx, 0x08B1C68Cu, {self, ctx.gpr[5], lo, hi});
}

// func_08BB9130(a, b) (floats f12 / f13): a linear interpolation a + func_08B1EFF8(func_08B1ED60()) * (b - a); the factor comes from the first function's v0 passed on as a0.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08BB9130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float start = ctx.fpr[12], end = ctx.fpr[13];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08B1ED60u, {}, {start, end});
    const float span = end - start;
    ctx.gpr[4] = ctx.gpr[2];
    (void)call_keep(rt, ctx, 0x08B1EFF8u, {ctx.gpr[2]});
    ctx.fpr[0] = start + psp_mul(ctx.fpr[0], span);
}

// func_08A32618(this, a1, a2): a temporary holder (on the stack) is filled by func_08A40C24(holder, this[0x40C] - 4), func_08A322A8(this, a1, a2) runs and func_08A5B9EC(holder) destroys the holder;
// v0 is that last call's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08A32618(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08A40C24u, {frame.at(0u), word(rt, self + 0x40Cu) - 4u});
    (void)call_keep(rt, ctx, 0x08A322A8u, {self, a1, a2});
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08B488BC(this, -, a2): none when this[0] is 0 (v0 = 0). Otherwise func_08B48814(this) answers an index and func_08B4889C(this, index, a2) is the tail call.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0] == 0 (the `beqz` to 08B48908: v0 = 0, nothing else).
void func_08B488BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a2 = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = word(rt, self);
        if (ctx.gpr[3] == 0u) return;
        const std::uint32_t index = call_keep(rt, ctx, 0x08B48814u, {self});
        ctx.gpr[5] = index;
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B4889Cu, {self, ctx.gpr[5], a2});
}

// func_08ADBFE0(a0, row, column): the object func_08ADB278(a0, D_08C90E78) is a two-level table: its table [obj + 0x34] (count [obj + 0x38]) holds at row * 4 + 8 an offset; obj[0x14] + offset is
// the second table whose count is at +4 and whose entry column * 4 + 8 is another offset; v0 = obj[0x14] + that offset. Anything missing (no object, no table, an index outside 0..count - 1, a zero offset) gives 0.
// v1 as the instructions leave it: the object (obj[0x14] once the first offset was found).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): every early leave: no object, no table (this[0x34] == 0), a negative row or one past the count, a zero first offset, a negative column or one past the second count, a zero second offset (each gives v0 = 0, v1 as set so far); the compared cases only find an entry.
void func_08ADBFE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t row = ctx.gpr[5], column = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = 0x08C90E78u;
    const std::uint32_t object = call_keep(rt, ctx, 0x08ADB278u, {ctx.gpr[4], 0x08C90E78u});
    ctx.gpr[3] = object;
    ctx.gpr[2] = 0u;
    if (object == 0u) return;
    const std::uint32_t table = word(rt, object + 0x34u);
    ctx.gpr[5] = table;
    if (table == 0u) return;
    if (signed_of(row) < 0) return;
    if (signed_of(word(rt, object + 0x38u) - 1u) < signed_of(row)) return;
    const std::uint32_t offset = word(rt, row * 4u + table + 8u);
    ctx.gpr[5] = offset;
    if (offset == 0u) return;
    const std::uint32_t base = word(rt, object + 0x14u);
    ctx.gpr[3] = base;
    ctx.gpr[5] = base + offset;
    if (signed_of(column) < 0) return;
    const std::uint32_t second = base + offset;
    if (signed_of(word(rt, second + 4u) - 1u) < signed_of(column)) return;
    const std::uint32_t inner = word(rt, column * 4u + second + 8u);
    ctx.gpr[5] = inner;
    if (inner == 0u) return;
    ctx.gpr[2] = base + inner;
}

// func_08B6441C(this): when the byte this[0] is 0: func_08B64260(), func_08B643A8(this) and, when that answers 0, func_08B64344(); v0 = v1 = 1 when func_08B643A8 answered non-zero, else 0. A non-zero byte
// leaves with 0 at once.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-zero byte (the first `beqz` falls to the return: v0 = v1 = 0) and func_08B643A8 answering 0 (func_08B64344 runs, then v0 = v1 = 0).
void func_08B6441C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[3] = 0u;
    if (byte_at(rt, self) != 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    (void)call_keep(rt, ctx, 0x08B64260u, {});
    ctx.gpr[4] = self;
    if (call_keep(rt, ctx, 0x08B643A8u, {self}) != 0u) {
        ctx.gpr[3] = 1u;
        ctx.gpr[2] = 1u;
        return;
    }
    (void)call_keep(rt, ctx, 0x08B64344u, {});
    ctx.gpr[3] = 0u;
    ctx.gpr[2] = 0u;
}

// func_08A9DD14(this): func_08A9DCB0(0) answers an object that is stored on the stack and func_08A5BAB8(this, &object) runs with it; when func_08A9DCB0(0) then answers an object whose byte +0x7C is 0,
// func_08A9C340(func_08A9DCB0(0)) runs. v0 is the last call's, or that object when its byte is set.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08A9DD14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[4] = 0u;
    const std::uint32_t first = call_keep(rt, ctx, 0x08A9DCB0u, {0u});
    ctx.gpr[4] = self;
    ctx.gpr[5] = frame.at(0u);
    put(rt, frame.at(0u), first);
    (void)call_keep(rt, ctx, 0x08A5BAB8u, {self, frame.at(0u)});
    ctx.gpr[4] = 0u;
    const std::uint32_t second = call_keep(rt, ctx, 0x08A9DCB0u, {0u});
    ctx.gpr[3] = rt.memory().load8(second + 0x7Cu);
    ctx.gpr[4] = 0u;
    if (ctx.gpr[3] != 0u) return;
    const std::uint32_t third = call_keep(rt, ctx, 0x08A9DCB0u, {0u});
    ctx.gpr[4] = third;
    (void)call_keep(rt, ctx, 0x08A9C340u, {third});
}

// func_08AE6B34(this): the node func_08AE655C(this, 0) (none: v0 = 0, nothing else) gets func_08AE85E8(this, D_08CDFDB8) and func_08AE6E4C(node + 0x28), node[0xC] = 0, and is moved in its circular list:
// when node[0] is not the node itself, it is unlinked (node[0]->[4] = node[4]; node[4]->[0] = node[0]; both links back to the node), then inserted before the sentinel D_08D4098C (node[4] = sentinel, the old
// last node (sentinel[0] before) becomes node[0], its [4] = node, sentinel[0] = node). v0 = sentinel when the node was moved, or func_08AE6E4C's answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): no node (v0 = 0 and nothing else) and a node whose [0] is the node itself (the `beq a0, s1` skips the unlink).
void func_08AE6B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = 0u;
    const std::uint32_t node = call_keep(rt, ctx, 0x08AE655Cu, {self, 0u});
    ctx.gpr[5] = 0x08CDFDB8u;
    ctx.gpr[4] = self;
    if (node == 0u) return;
    (void)call_keep(rt, ctx, 0x08AE85E8u, {self, 0x08CDFDB8u});
    ctx.gpr[4] = node + 0x28u;
    (void)call_keep(rt, ctx, 0x08AE6E4Cu, {node + 0x28u});
    const std::uint32_t next = word(rt, node);
    ctx.gpr[4] = next;
    put(rt, node + 0xCu, 0u);
    constexpr std::uint32_t kSentinel = 0x08D4098Cu;
    ctx.gpr[6] = kSentinel;
    if (next != node) {
        const std::uint32_t previous = word(rt, node + 4u);
        ctx.gpr[2] = previous;
        put(rt, next + 4u, previous);
        const std::uint32_t reloaded = word(rt, node + 4u);
        ctx.gpr[3] = reloaded;
        put(rt, node + 4u, node);
        put(rt, reloaded, next);
        put(rt, node, node);
    }
    const std::uint32_t last = word(rt, kSentinel);
    ctx.gpr[3] = last;
    put(rt, node + 4u, kSentinel);
    put(rt, last + 4u, node);
    ctx.gpr[2] = word(rt, node + 4u);
    put(rt, node, last);
    put(rt, ctx.gpr[2], node);
}

// func_08AE85E8(this, message): the node func_08AE655C(this, D_08C91A1C) gets the handler table entry for the kind message[0] (D_08CDFDBC + kind * 4) called with (node, message) when kind < 0x16.
// v0 = 0 without a node; a kind of 0x16 or more leaves with v0 = the address of the table entry (the delay slot computed it), v1 = (kind < 0x16).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): no node (v0 = 0, v1 = (kind < 0x16)) and kinds of 0x16 or more (v0 = the table entry's address, no call); every compared message has a node and a kind below 0x16.
void func_08AE85E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], message = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = 0x08C91A1Cu;
    const std::uint32_t kind = word(rt, message);
    const std::uint32_t node = call_keep(rt, ctx, 0x08AE655Cu, {self, 0x08C91A1Cu});
    ctx.gpr[3] = kind < 0x16u ? 1u : 0u;
    if (node == 0u) return;
    ctx.gpr[4] = node;
    ctx.gpr[6] = kind << 2;
    ctx.gpr[2] = 0x08CDFDBCu + (kind << 2);
    ctx.gpr[5] = message;
    if (kind >= 0x16u) return;
    (void)call_keep(rt, ctx, word(rt, ctx.gpr[2]), {node, message});
}

// func_08B344B0(this): this[0x24] = this[0x20] = 0 and the import sceKernelCreateLwMutex(this, D_08C93B48, 0x200, 0, 0) as the tail call (its results are the stub's).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B344B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x20u, 0u);
    tail_call(ctx, 0x08C7AAB0u, {self, 0x08C93B48u, 0x200u, 0u, 0u});
}

// func_08B0B36C(a0, a1, ok = a2, size = a3): the record func_08B0B34C(a0, a1, size) (none: v0 = 0, *ok untouched) is checked by func_08B0B104(record): a zero answer sets the byte *ok = 0 and gives the record;
// otherwise the negated unaligned word at record + 4 is len, func_08B35840(0x10, len) allocates, func_08B26B74(&local, alloc, record, 1) builds the result in the local frame object (func_08B24DB4(&local)
// destroys it), *ok = 1, *size = len when size is non-null, and v0 = the allocated block (the delay slot of the func_08B26B74 call copies func_08B35840's v0 into s0 before the call).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): no record from func_08B0B34C (v0 = 0, *ok untouched) and the whole success body (func_08B0B104 answering non-zero: the allocation, func_08B26B74 / func_08B24DB4, *ok = 1, *size = length, v0 = the block): every compared call took the `*ok = 0` leave. The success body was read from the asm only (the delay slot of the func_08B26B74 call copies func_08B35840's v0 into s0, which becomes v0).
void func_08B0B36C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t okay = ctx.gpr[6], size = ctx.gpr[7];
    GuestFrame frame(ctx, 0x1850u);
    ctx.gpr[6] = size;
    const std::uint32_t record = call_keep(rt, ctx, 0x08B0B34Cu, {ctx.gpr[4], ctx.gpr[5], size});
    std::uint32_t result = record;
    if (record != 0u) {
        ctx.gpr[4] = record;
        if (call_keep(rt, ctx, 0x08B0B104u, {record}) == 0u) {
            ctx.gpr[4] = 0x10u;
            rt.memory().store8(okay, 0u);
        } else {
            ctx.gpr[4] = 0x10u;
            const std::uint32_t length = 0u - word_unaligned(rt, record + 4u);
            const std::uint32_t block = call_keep(rt, ctx, 0x08B35840u, {0x10u, length});
            ctx.gpr[5] = block;
            ctx.gpr[6] = record;
            ctx.gpr[7] = 1u;
            (void)call_keep(rt, ctx, 0x08B26B74u, {frame.at(0u), block, record, 1u});
            (void)call_keep(rt, ctx, 0x08B24DB4u, {frame.at(0u)});
            ctx.gpr[2] = 1u;
            result = block;
            rt.memory().store8(okay, 1u);
            if (size != 0u) put(rt, size, length);
        }
    }
    ctx.gpr[2] = result;
}

// func_08B85E00(begin, end): a new 1-byte-character string from the range [begin, end): the empty range gives the shared empty representation's characters (D_08D74FD0 + 0xC); a null begin with a non-null
// end calls func_08B5AADC(D_08C97BAC) first. func_08B8543C(length, 0) allocates the representation (length at +0, reference count +8 = 0), the characters follow at +0xC (one byte copied in place, more by
// func_08B79A88(chars, begin, length)) and the terminating byte is the byte D_08CB7430. v0 = the characters.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null begin with a non-null end (the func_08B5AADC call with D_08C97BAC).
void func_08B85E00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t begin = ctx.gpr[4], end = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t chars;
    if (begin == end) {
        ctx.gpr[2] = 0x08D74FD0u;
        chars = 0x08D74FD0u + 0xCu;
    } else {
        const std::uint32_t length = end - begin;
        if (begin == 0u && end != 0u) {
            ctx.gpr[4] = 0x08C97BACu;
            (void)call_keep(rt, ctx, 0x08B5AADCu, {0x08C97BACu});
        }
        ctx.gpr[4] = length;
        ctx.gpr[5] = 0u;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B8543Cu, {length, 0u});
        chars = block + 0xCu;
        if (length == 1u) {
            ctx.gpr[2] = rt.memory().load8(begin);
            rt.memory().store8(block + 0xCu, static_cast<std::uint8_t>(ctx.gpr[2]));
        } else {
            ctx.gpr[5] = begin;
            ctx.gpr[4] = chars;
            ctx.gpr[6] = length;
            (void)call_keep(rt, ctx, 0x08B79A88u, {chars, begin, length});
        }
        ctx.gpr[3] = 0x08CB4000u;
        ctx.gpr[4] = rt.memory().load8(0x08CB7430u);
        ctx.gpr[2] = chars + length;
        put(rt, block, length);
        put(rt, block + 8u, 0u);
        rt.memory().store8(chars + length, static_cast<std::uint8_t>(ctx.gpr[4]));
    }
    ctx.gpr[2] = chars;
}

// func_08BFD224(this, character, from): the last position <= from of the byte `character` (compared as a signed byte) in the string this[0] (length at data - 0xC); -1 when there is none or the string is empty.
// v0 = v1 = the position.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty string (v0 = v1 = -1 at once) and a `from` below length - 1 (the `movn` keeps `from`).
void func_08BFD224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t data = word(rt, ctx.gpr[4]);
    const std::uint32_t length = word(rt, data - 0xCu);
    const std::int32_t wanted = static_cast<std::int8_t>(ctx.gpr[5] & 0xFFu);
    const std::uint32_t from = ctx.gpr[6];
    std::uint32_t found = 0xFFFFFFFFu;
    if (length != 0u) {
        std::uint32_t last = length - 1u;
        if (from < last) last = from;
        const std::uint32_t count = last + 1u;
        for (std::uint32_t taken = 0u; taken != count; ++taken) {
            const std::uint32_t position = count - 1u - taken;
            if (byte_at(rt, data + position) == wanted) {
                found = position;
                break;
            }
        }
    }
    ctx.gpr[3] = found;
    ctx.gpr[2] = found;
}

// func_08C3E24C(pool, address): the slot number of an address in a pool of equal slots: pool[1] is the first slot (rounded up to 4), pool[2] the size of the area, pool[3] the slot size (rounded up to 4);
// the number is (address - first) / size when the address lies in the area and (address - first) % size is 0, else -1. v0 = the number, v1 as the instructions leave it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an address below the first slot, one at or past the end of the area, a zero slot size (lo = -1, hi = the offset) and a non-zero remainder (v0 = -1).
void func_08C3E24C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pool = ctx.gpr[4], address = ctx.gpr[5];
    const std::uint32_t start = word(rt, pool + 4u);
    ctx.gpr[6] = start;
    std::uint32_t first = (start + 3u) & ~3u;
    ctx.gpr[3] = first;
    ctx.gpr[7] = 0xFFFFFFFFu;
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (address < first) return;
    const std::uint32_t limit = start + word(rt, pool + 8u);
    ctx.gpr[2] = limit;
    first = address - first;
    ctx.gpr[3] = first;
    if (!(address < limit)) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const std::uint32_t size = (word(rt, pool + 0xCu) + 3u) & ~3u;
    std::uint32_t quotient = 0xFFFFFFFFu, remainder = first;
    if (size != 0u) {
        quotient = first / size;
        remainder = first % size;
    }
    ctx.gpr[3] = remainder;
    ctx.gpr[2] = remainder == 0u ? quotient : 0xFFFFFFFFu;
    ctx.gpr[7] = ctx.gpr[2];
}

// func_08C3DB68(pool): takes the slot at pool[2] (the first free one, as a signed halfword) of a pool of 124-byte slots starting at pool + 0xC: with index = -1 nothing (v0 = 0). The slot's [8] halfword becomes
// the next free index (pool[2]), the previous head pool[1] is stored in its [4] halfword, its [6] and [8] halfwords are -1; pool[0] takes the index when it was negative, the slot named by pool[1] (if any)
// links back with its [0x12], pool[1] = index, pool[4] += 1. v0 = the slot (pool + 0xC + index * 124).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): index < 0 (the `bltz` to the return: v0 = 0).
void func_08C3DB68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pool = ctx.gpr[4];
    const std::int32_t index = half_at(rt, pool + 4u);
    ctx.gpr[5] = pool;
    ctx.gpr[8] = 0xFFFFFFFFu;
    ctx.gpr[7] = 0u;
    ctx.gpr[4] = static_cast<std::uint32_t>(index);
    ctx.gpr[3] = static_cast<std::uint32_t>(index) << 2;
    ctx.gpr[2] = static_cast<std::uint32_t>(index) * 124u;
    if (index < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t slot = pool + static_cast<std::uint32_t>(index) * 124u + 0xCu;
    ctx.gpr[2] = pool + static_cast<std::uint32_t>(index) * 124u;
    ctx.gpr[7] = slot;
    const std::uint32_t next = rt.memory().load16(slot + 8u);
    const std::uint32_t head = rt.memory().load16(pool + 2u);
    ctx.gpr[3] = next;
    ctx.gpr[2] = head;
    rt.memory().store16(pool + 4u, static_cast<std::uint16_t>(next));
    rt.memory().store16(slot + 4u, static_cast<std::uint16_t>(head));
    rt.memory().store16(slot + 8u, 0xFFFFu);
    rt.memory().store16(slot + 6u, 0xFFFFu);
    const std::int32_t first = half_at(rt, pool);
    ctx.gpr[2] = static_cast<std::uint32_t>(first);
    const std::uint32_t narrow = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(index)));
    ctx.gpr[6] = narrow;
    if (first < 0) rt.memory().store16(pool, static_cast<std::uint16_t>(narrow));
    const std::int32_t head_index = half_at(rt, pool + 2u);
    ctx.gpr[2] = static_cast<std::uint32_t>(head_index);
    const std::uint32_t head_slot = static_cast<std::uint32_t>(head_index) * 124u + pool;
    ctx.gpr[3] = head_slot;
    ctx.gpr[4] = static_cast<std::uint32_t>(head_index) << 2;
    if (head_index >= 0) rt.memory().store16(head_slot + 0x12u, static_cast<std::uint16_t>(narrow));
    std::uint32_t count = rt.memory().load16(pool + 8u);
    rt.memory().store16(pool + 2u, static_cast<std::uint16_t>(narrow));
    count += 1u;
    ctx.gpr[2] = count;
    rt.memory().store16(pool + 8u, static_cast<std::uint16_t>(count));
    ctx.gpr[2] = slot;
}

// func_08BAC284(out, quaternion, vector): the vector (a2[0..2]) rotated by the quaternion {x, y, z, w} (a1[0..3]) as 2 * (R * v) with R built from half-sized products, written to out[0], out[2], out[1] in that
// order (note the swap: the first result goes to +0, the second to +8, the third to +4). The temporaries keep the asm's register names; every multiply is the PSP's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08BAC284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], q = ctx.gpr[5], v = ctx.gpr[6];
    float f0 = float_at(rt, q + 8u), f7 = float_at(rt, q + 4u), f4 = float_at(rt, q);
    const float f12 = psp_mul(f0, f0);
    const float f14 = psp_mul(f7, f7);
    float f1 = float_at(rt, q + 0xCu);
    float f6 = fbits(0x3F000000u);
    float f5 = psp_mul(f4, f4);
    const float f11 = psp_mul(f0, f1);
    const float f15 = psp_mul(f4, f1);
    float f8 = psp_mul(f4, f7);
    f1 = psp_mul(f7, f1);
    f4 = psp_mul(f4, f0);
    f7 = psp_mul(f7, f0);
    float f10 = f6 - f12;
    float f2 = f6 - f14;
    float f3 = f4 - f1;
    const float f13 = f8 + f11;
    float f9 = f15 + f7;
    f6 = f6 - f5;
    f10 = f10 - f5;
    f2 = f2 - f12;
    f8 = f8 - f11;
    f0 = float_at(rt, v);
    f5 = float_at(rt, v + 4u);
    f4 = f4 + f1;
    f3 = psp_mul(f0, f3);
    f9 = psp_mul(f5, f9);
    f2 = psp_mul(f0, f2);
    f8 = psp_mul(f5, f8);
    f6 = f6 - f14;
    f0 = psp_mul(f0, f13);
    f5 = psp_mul(f5, f10);
    f7 = f7 - f15;
    f1 = float_at(rt, v + 8u);
    f2 = f2 + f8;
    f0 = f0 + f5;
    f6 = psp_mul(f1, f6);
    f4 = psp_mul(f1, f4);
    f3 = f3 + f9;
    f1 = psp_mul(f1, f7);
    f2 = f2 + f4;
    f3 = f3 + f6;
    f0 = f0 + f1;
    f2 = f2 + f2;
    f3 = f3 + f3;
    f0 = f0 + f0;
    put_float(rt, out, f2);
    put_float(rt, out + 8u, f3);
    put_float(rt, out + 4u, f0);
    ctx.fpr[0] = f0;
}

// func_08C465EC(out, a, b): out[i] = 1.0f when a[i] < b[i] (a NaN compares false) else 0.0f, for the three components; v0 = out + 8 and f0 = b[2] as the instructions leave them.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a[0] < b[0] and a[1] < b[1] true (the 1.0f select of the first two components; only the 0.0f select ran for them).
void func_08C465EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6];
    ctx.gpr[2] = out + 8u;
    put_float(rt, out, float_at(rt, a) < float_at(rt, b) ? 1.0f : 0.0f);
    put_float(rt, out + 4u, float_at(rt, a + 4u) < float_at(rt, b + 4u) ? 1.0f : 0.0f);
    const float f0 = float_at(rt, b + 8u);
    ctx.fpr[0] = f0;
    put_float(rt, out + 8u, float_at(rt, a + 8u) < f0 ? 1.0f : 0.0f);
    ctx.gpr[4] = out + 4u;
}

// func_08B52730(this): six words this[0], [4], [8], [0xC], [0x10], [0x14] and [0x18] get their top byte replaced by a tag (0x54, 0x55, 0x58, 0x56, 0x57, 0x5B and 0x0B for the last one) and are written back in
// the order [0x18], [0], [4], [8], [0xC], [0x10], [0x14]. v0 = the new [0x18], v1 = the new [0], a1 / a2 / a3 / t0 / t1 the other new words.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B52730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t w18 = word(rt, self + 0x18u), w0 = word(rt, self), w4 = word(rt, self + 4u), w8 = word(rt, self + 8u);
    const std::uint32_t wc = word(rt, self + 0xCu), w10 = word(rt, self + 0x10u), w14 = word(rt, self + 0x14u);
    constexpr std::uint32_t kLow = 0x00FFFFFFu;
    ctx.gpr[10] = w0;
    ctx.gpr[11] = w4;
    ctx.gpr[12] = w8;
    ctx.gpr[13] = wc;
    ctx.gpr[14] = w10;
    ctx.gpr[15] = w14;
    ctx.gpr[2] = 0x0B000000u | (w18 & kLow);
    ctx.gpr[3] = 0x54000000u | (w0 & kLow);
    ctx.gpr[5] = 0x55000000u | (w4 & kLow);
    ctx.gpr[6] = 0x58000000u | (w8 & kLow);
    ctx.gpr[7] = 0x56000000u | (wc & kLow);
    ctx.gpr[8] = 0x57000000u | (w10 & kLow);
    ctx.gpr[9] = 0x5B000000u | (w14 & kLow);
    put(rt, self + 0x18u, ctx.gpr[2]);
    put(rt, self, ctx.gpr[3]);
    put(rt, self + 4u, ctx.gpr[5]);
    put(rt, self + 8u, ctx.gpr[6]);
    put(rt, self + 0xCu, ctx.gpr[7]);
    put(rt, self + 0x10u, ctx.gpr[8]);
    put(rt, self + 0x14u, ctx.gpr[9]);
}

// func_08B77B1C(value): interrupts are suspended (sceKernelCpuSuspendIntr) and func_08B79E68() answers the thread's record; its block list head is record[0x148] (the embedded block at record + 0x14C when
// there is none yet). A block holds a count at +4 and its entries from +8; a block with 0x20 entries or more is replaced by a new one (func_08B35DAC(record, 0x10C): none gives v0 = -1 after the interrupts
// were resumed once) linked in front (next = the old head, count 0, [0x108] = 0). The value is appended and the count raised; the interrupts are resumed (CpuResumeIntr is a void import, so v0 stays) and v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B35DAC answering 0 (v0 = -1 after the first resume of the interrupts).
void func_08B77B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t state = call_keep(rt, ctx, 0x08C7A888u, {});
    const std::uint32_t record = call_keep(rt, ctx, 0x08B79E68u, {});
    std::uint32_t block = word(rt, record + 0x148u);
    ctx.gpr[4] = state;
    if (block == 0u) {
        block = record + 0x14Cu;
        put(rt, record + 0x148u, block);
    }
    std::uint32_t count = word(rt, block + 4u);
    if (!(signed_of(count) < 0x20)) {
        (void)call_keep(rt, ctx, 0x08C7A8A8u, {state});
        ctx.gpr[4] = record;
        ctx.gpr[5] = 0x10Cu;
        block = call_keep(rt, ctx, 0x08B35DACu, {record, 0x10Cu});
        ctx.gpr[2] = 0xFFFFFFFFu;
        if (block == 0u) return;
        state = call_keep(rt, ctx, 0x08C7A888u, {});
        const std::uint32_t head = word(rt, record + 0x148u);
        ctx.gpr[3] = 0u;
        put(rt, block + 4u, 0u);
        put(rt, block, head);
        put(rt, block + 0x108u, 0u);
        put(rt, record + 0x148u, block);
        count = 0u;
    }
    ctx.gpr[2] = (count << 2) + block;
    put(rt, (count << 2) + block + 8u, value);
    ctx.gpr[3] = count + 1u;
    put(rt, block + 4u, count + 1u);
    ctx.gpr[4] = state;
    (void)call_keep(rt, ctx, 0x08C7A8A8u, {state});
    ctx.gpr[2] = 0u;
}

// func_08B5E0EC(this): this[0] = 0, func_08B5DE48(this), the interrupts are suspended (the state is kept in D_08D6E134), func_08B6085C(D_08D6E120, 1) runs and this[0] = D_08D6E120 before they are resumed
// (a void import: v0 stays D_08D6E120).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B5E0EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0u);
    (void)call_keep(rt, ctx, 0x08B5DE48u, {self});
    const std::uint32_t state = call_keep(rt, ctx, 0x08C7A888u, {});
    put(rt, 0x08D6E134u, state);
    ctx.gpr[5] = 1u;
    ctx.gpr[4] = word(rt, 0x08D6E120u);
    (void)call_keep(rt, ctx, 0x08B6085Cu, {ctx.gpr[4], 1u});
    const std::uint32_t current = word(rt, 0x08D6E120u);
    ctx.gpr[2] = current;
    ctx.gpr[4] = word(rt, 0x08D6E134u);
    put(rt, self, current);
    (void)call_keep(rt, ctx, 0x08C7A8A8u, {ctx.gpr[4]});
}

// func_08B8D96C(out, string, position, count): out[0] = func_08B8D8B4(data + position, data + position + n, &local) with n = min(count, length - position) (unsigned) and data = string[0] (length at data - 0xC).
// A position past the length calls func_08B5AEDC(D_08C9C3CC) first (the error path; it does not come back in the original's use).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): position > length (the func_08B5AEDC(D_08C9C3CC) error call).
void func_08B8D96C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], string = ctx.gpr[5], position = ctx.gpr[6];
    std::uint32_t count = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t data = word(rt, string);
    const std::uint32_t length = word(rt, data - 0xCu);
    ctx.gpr[8] = data;
    ctx.gpr[3] = length;
    if (length < position) {
        ctx.gpr[4] = 0x08C9C3CCu;
        (void)call_keep(rt, ctx, 0x08B5AEDCu, {0x08C9C3CCu});
    }
    const std::uint32_t rest = length - position;
    if (!(count < rest)) count = rest;
    ctx.gpr[7] = count;
    ctx.gpr[6] = frame.at(0u);
    const std::uint32_t result = call_keep(rt, ctx, 0x08B8D8B4u, {data + position, position + (data + count), frame.at(0u), count});
    put(rt, out, result);
}

// func_08BEE778(list, key): over the circular list whose head is `list` (links at node + 0): node[8] is the owner (0 calls func_08A5BB7C(D_08CA2F7C), the assertion); when func_08A76F20(owner, key) answers
// non-zero the node is destroyed: func_08B5C094(node), func_08A5B9EC(node + 8) and func_08A5BC54(node, 0xC, 4) (a free of 12 bytes, alignment 4). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null owner (the func_08A5BB7C(D_08CA2F7C) assertion call).
void func_08BEE778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t current = word(rt, head);
    ctx.gpr[4] = current;
    if (current == head) return;
    for (;;) {
        const std::uint32_t node = current;
        const std::uint32_t owner = word(rt, node + 8u);
        const std::uint32_t next = word(rt, node);
        ctx.gpr[4] = 0x08CA2F7Cu;
        if (owner == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA2F7Cu});
        ctx.gpr[4] = owner;
        ctx.gpr[5] = key;
        if (call_keep(rt, ctx, 0x08A76F20u, {owner, key}) != 0u) {
            ctx.gpr[4] = node;
            (void)call_keep(rt, ctx, 0x08B5C094u, {node});
            ctx.gpr[4] = node + 8u;
            (void)call_keep(rt, ctx, 0x08A5B9ECu, {node + 8u});
            ctx.gpr[4] = node;
            ctx.gpr[5] = 0xCu;
            ctx.gpr[6] = 4u;
            (void)call_keep(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
        }
        current = next;
        ctx.gpr[4] = next;
        if (head == next) break;
    }
}

// func_08B89054(this, other): the 1-byte-character string append: nothing for an empty other (v0 = this); the representation is made unique and big enough (func_08B864EC(this, newlength) when its capacity [-8]
// is smaller than the new length or its reference count [-4] is positive); the bytes of other go to the end (a single byte in place, more by func_08B79A88(end, other data, length)); the length [-0xC] is
// the new one, the reference count [-4] = 0 and the terminating byte (D_08CB7430) follows. v0 = this.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a positive reference count with enough capacity (the reserve call comes from the `blezl` falling through) and a one-byte other (the in-place copy).
void func_08B89054(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t source = word(rt, other);
    const std::uint32_t length = word(rt, source - 0xCu);
    ctx.gpr[8] = source;
    if (length != 0u) {
        std::uint32_t data = word(rt, self);
        const std::uint32_t old_length = word(rt, data - 0xCu);
        const std::uint32_t capacity = word(rt, data - 8u);
        const std::uint32_t total = length + old_length;
        ctx.gpr[5] = total;
        bool reserve = capacity < total;
        std::uint32_t current = 0u;
        if (!reserve) {
            if (signed_of(word(rt, data - 4u)) > 0) reserve = true;
            else current = word(rt, data - 0xCu);
        }
        if (reserve) {
            (void)call_keep(rt, ctx, 0x08B864ECu, {self, total});
            source = word(rt, other);
            data = word(rt, self);
            current = word(rt, data - 0xCu);
        }
        const std::uint32_t end = data + current;
        ctx.gpr[5] = source;
        ctx.gpr[6] = length;
        ctx.gpr[4] = end;
        if (length == 1u) {
            rt.memory().store8(end, rt.memory().load8(source));
        } else {
            (void)call_keep(rt, ctx, 0x08B79A88u, {end, source, length});
        }
        const std::uint32_t representation = word(rt, self);
        put(rt, representation - 4u, 0u);
        put(rt, representation - 0xCu, total);
        const std::uint32_t terminator = rt.memory().load8(0x08CB7430u);
        ctx.gpr[3] = 0x08CB4000u;
        rt.memory().store8(representation + total, static_cast<std::uint8_t>(terminator));
    }
    ctx.gpr[2] = self;
}

// func_08AFEAB8(this, a1, a2): the position vector this[0xD8] + 0x24 (three floats) is copied to a local and func_08BAE278(a2, &local) answers a pair (v0, v1) kept in the locals; func_08C45F6C(a1, &pair)
// answers another pair whose v1 points at two words (the rotation and vector for func_08BAC284(&result, v1[0], v1[1])); func_08BAE2FC(&result2, second v0, &result) fills three words that
// become this[0x20 .. 0x28], and func_08AFEA94(this, a2) is the last call.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08AFEAB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t position = word(rt, self + 0xD8u) + 0x24u;
    put(rt, frame.at(0x10u), word(rt, position));
    put(rt, frame.at(0x14u), word(rt, position + 4u));
    put(rt, frame.at(0x18u), word(rt, position + 8u));
    ctx.gpr[4] = a2;
    ctx.gpr[5] = frame.at(0x10u);
    const GuestResult first = call_keep(rt, ctx, 0x08BAE278u, {a2, frame.at(0x10u)}, {});
    ctx.gpr[4] = a1;
    ctx.gpr[5] = frame.at(0u);
    put(rt, frame.at(0u), first.v0);
    put(rt, frame.at(4u), first.v1);
    put(rt, frame.at(0x40u), first.v0);
    put(rt, frame.at(0x44u), first.v1);
    const GuestResult second = call_keep(rt, ctx, 0x08C45F6Cu, {a1, frame.at(0u)}, {});
    ctx.gpr[4] = frame.at(0x28u);
    put(rt, frame.at(8u), second.v0);
    put(rt, frame.at(0x40u), second.v0);
    put(rt, frame.at(0x44u), second.v1);
    put(rt, frame.at(0xCu), second.v1);
    ctx.gpr[6] = word(rt, second.v1 + 4u);
    ctx.gpr[5] = word(rt, second.v1);
    (void)call_keep(rt, ctx, 0x08BAC284u, {frame.at(0x28u), ctx.gpr[5], ctx.gpr[6]});
    ctx.gpr[5] = word(rt, frame.at(8u));
    ctx.gpr[6] = frame.at(0x28u);
    ctx.gpr[4] = frame.at(0x1Cu);
    (void)call_keep(rt, ctx, 0x08BAE2FCu, {frame.at(0x1Cu), ctx.gpr[5], frame.at(0x28u)});
    ctx.gpr[3] = word(rt, frame.at(0x20u));
    ctx.gpr[6] = word(rt, frame.at(0x24u));
    ctx.gpr[2] = word(rt, frame.at(0x1Cu));
    ctx.gpr[4] = self;
    ctx.gpr[5] = a2;
    put(rt, self + 0x20u, ctx.gpr[2]);
    put(rt, self + 0x24u, ctx.gpr[3]);
    put(rt, self + 0x28u, ctx.gpr[6]);
    (void)call_keep(rt, ctx, 0x08AFEA94u, {self, a2});
}

// func_08C49140(first, last, comparator): a sort of the 16-bit values [first, last) with the comparator pair (comparator[0], comparator[1]) copied to the stack: func_08C49038(first, last, &copy) alone when the
// range holds at most 16 values; otherwise the first 16 are sorted by func_08C49038(first, first + 0x20, &copy) and every following value is inserted with func_08C48FC4(&value, value, &copy) (the copy
// rebuilt on the stack as {comparator[0], the second word reloaded from the first copy}).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a range over 16 values (the first-16 sort, the insertion loop and the `beq s0, s1` leave): every compared range was small, the long path was read from the asm only (the copy of the comparator rebuilt as {comparator[0], the second word reloaded after the first call}, the loop's delay slots: a0 = the old cursor, the cursor +2 before the call).
void func_08C49140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5], comparator = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const bool small = signed_of(last - first) >> 1 < 0x11;
    const std::uint32_t second_word = word(rt, comparator + 4u);
    const std::uint32_t first_word = word(rt, comparator);
    put(rt, frame.at(0xCu), second_word);
    put(rt, frame.at(8u), first_word);
    ctx.gpr[3] = second_word;
    if (small) {
        ctx.gpr[5] = last;
        ctx.gpr[6] = frame.at(8u);
        (void)call_keep(rt, ctx, 0x08C49038u, {first, last, frame.at(8u)});
        return;
    }
    std::uint32_t cursor = first + 0x20u;
    ctx.gpr[5] = cursor;
    ctx.gpr[6] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08C49038u, {first, cursor, frame.at(8u)});
    const std::uint32_t reloaded = word(rt, frame.at(0xCu));
    put(rt, frame.at(0u), first_word);
    put(rt, frame.at(4u), reloaded);
    if (cursor == last) return;
    do {
        const std::uint32_t at = cursor;
        const std::uint32_t value = static_cast<std::uint32_t>(static_cast<std::int32_t>(half_at(rt, at)));
        ctx.gpr[5] = value;
        ctx.gpr[4] = at;
        ctx.gpr[6] = frame.at(0u);
        cursor += 2u;
        (void)call_keep(rt, ctx, 0x08C48FC4u, {at, value, frame.at(0u)});
        ctx.gpr[5] = cursor;
    } while (last != cursor);
}

// func_08A63F1C(this, limit): the ring owner this[0x20] (a pointer to a slot) is taken over: the old content s3 of the slot is released by func_08A5BA08, this[0x1C] (if set) by func_08A5B930, this[0x1C] = the
// old content, slot = this. Then, while limit < (this[0x28] - this[0x24]) >> 2 (unsigned compare), func_08A63C5C(this, limit) answers into this[0x50]; the answer 2 ends the loop with v0 = 2. Otherwise v0 =
// this[0x50]. The slot gets its old content back, this[0x1C] (if set) is released by func_08A5B930 and cleared.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0x1C] non-zero at entry (the first func_08A5B930 release) and the answer 2 of func_08A63C5C (the loop ends with v0 = 2).
void func_08A63F1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], limit = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t slot = word(rt, self + 0x20u);
    const std::uint32_t previous = word(rt, slot);
    std::uint32_t result = self;
    if (previous != 0u) (void)call_keep(rt, ctx, 0x08A5BA08u, {previous});
    ctx.gpr[4] = word(rt, self + 0x1Cu);
    if (ctx.gpr[4] != 0u) (void)call_keep(rt, ctx, 0x08A5B930u, {ctx.gpr[4]});
    put(rt, self + 0x1Cu, previous);
    put(rt, slot, result);
    for (;;) {
        const std::uint32_t difference = word(rt, self + 0x28u) - word(rt, result + 0x24u);
        ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(difference) >> 2);
        ctx.gpr[4] = result;
        ctx.gpr[5] = limit;
        if (!(limit < static_cast<std::uint32_t>(signed_of(difference) >> 2))) {
            result = word(rt, result + 0x50u);
            break;
        }
        const std::uint32_t answer = call_keep(rt, ctx, 0x08A63C5Cu, {result, limit});
        put(rt, result + 0x50u, answer);
        if (answer == 2u) {
            result = 2u;
            break;
        }
    }
    put(rt, slot, previous);
    ctx.gpr[4] = word(rt, self + 0x1Cu);
    if (ctx.gpr[4] != 0u) (void)call_keep(rt, ctx, 0x08A5B930u, {ctx.gpr[4]});
    put(rt, self + 0x1Cu, 0u);
    ctx.gpr[2] = result;
}

// func_08C31920(a0, this, source): func_08C31444(source) answers a pair (v0, v1) and func_08C318F8(this, a1, v0, v1) is the tail call.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08C31920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5], source = ctx.gpr[6];
    GuestResult pair;
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = source;
        pair = call_keep(rt, ctx, 0x08C31444u, {source}, {});
        ctx.gpr[6] = pair.v0;
        ctx.gpr[4] = self;
        ctx.gpr[7] = pair.v1;
    }
    tail_call(ctx, 0x08C318F8u, {self, ctx.gpr[5], pair.v0, pair.v1});
}

// func_08B7C9E4(string, character): the last occurrence of `character` in the string, by repeated func_08B7BA50(position, character) (the first-occurrence search): v0 = the last non-zero answer, or 0
// when there is none. A character of 0 is the tail call func_08B7BA50(string, 0) (the terminator's position).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a zero character (the tail call func_08B7BA50(string, 0)).
void func_08B7C9E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t string = ctx.gpr[4], character = ctx.gpr[5];
    if (character == 0u) {
        ctx.gpr[2] = string;
        tail_call(ctx, 0x08B7BA50u, {string, character});
        return;
    }
    GuestFrame frame(ctx, 0x10u);
    std::uint32_t position = string, last = 0u;
    ctx.gpr[2] = string;
    for (;;) {
        ctx.gpr[4] = position;
        const std::uint32_t found = call_keep(rt, ctx, 0x08B7BA50u, {position, character});
        if (found == 0u) break;
        last = found;
        position = found + 1u;
        ctx.gpr[2] = position;
    }
    ctx.gpr[2] = last;
}

// func_08AB85F0(this): func_08B93CD8(this, a1, &local) runs, this[8] = 0, and when the byte count [this[0] - 4] is not negative func_08BAFF30(this) runs (this[0] is read again); this[4] = this[0] and the
// tail-like call func_08AB836C(this) ends it (v0 = its answer).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a negative count at [this[0] - 4] (the `bltz` skips func_08BAFF30 and the reload of this[0]).
void func_08AB85F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[6] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B93CD8u, {self, ctx.gpr[5], frame.at(0u)});
    std::uint32_t head = word(rt, self);
    put(rt, self + 8u, 0u);
    const std::uint32_t count = word(rt, head - 4u);
    ctx.gpr[3] = head;
    ctx.gpr[2] = count;
    ctx.gpr[4] = self;
    if (signed_of(count) >= 0) {
        (void)call_keep(rt, ctx, 0x08BAFF30u, {self});
        head = word(rt, self);
        ctx.gpr[3] = head;
    }
    put(rt, self + 4u, head);
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08AB836Cu, {self});
}

// func_08BF913C(tree, node): destroys a chain of 0x18-byte nodes: for each node, the subtree in node[0xC] first (recursively, func_08BF913C(tree, node[0xC])), then the node itself is freed by
// func_08A5BC54(node, 0x18, 4) and the walk continues with node[8] (read before the free) until it is 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a chain of more than one node (`bnez s0`: the walk continues); every compared chain had one node.
void func_08BF913C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (node == 0u) return;
    for (;;) {
        ctx.gpr[5] = word(rt, node + 0xCu);
        ctx.gpr[4] = tree;
        (void)call_keep(rt, ctx, 0x08BF913Cu, {tree, ctx.gpr[5]});
        const std::uint32_t next = word(rt, node + 8u);
        ctx.gpr[4] = node;
        ctx.gpr[5] = 0x18u;
        ctx.gpr[6] = 4u;
        (void)call_keep(rt, ctx, 0x08A5BC54u, {node, 0x18u, 4u});
        const std::uint32_t follow = next;
        node = next;
        if (follow == 0u) break;
    }
}

// func_08C30E0C(address, count, argument): for count entries (4 bytes apart) starting at `address`, func_08B88F44(entry address, argument) runs for each non-null address. A zero count leaves at once.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-zero count: the whole loop (and the null-entry skip) was read from the asm only, every compared call had count 0 and left at once.
void func_08C30E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t entry = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5], argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    if (count == 0u) return;
    std::uint32_t index = 0u;
    do {
        ++index;
        ctx.gpr[4] = entry;
        ctx.gpr[5] = argument;
        if (entry != 0u) (void)call_keep(rt, ctx, 0x08B88F44u, {entry, argument});
        entry += 4u;
    } while (count != index);
}

// func_08B93B20(length, capacity): allocates a string representation for `length` characters when the old capacity is `capacity`: a length over 0x3FFFFFFC calls func_08B5ADDC(D_08C9F670) (the error);
// a length above the old capacity is raised to twice the capacity when that is bigger; sizes (length + 0x1D) over 0x1000 that grew are rounded up to a whole 0x1000 page (and clamped to 0x3FFFFFFC,
// size 0x40000009); func_08B35DA0(length + 0xD) allocates and the capacity is stored at +4 of the block. v0 = the block.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a length over 0x3FFFFFFC (the error call), a length below twice the old capacity (the doubling), sizes over 0x1000 (the page rounding and the clamp to 0x3FFFFFFC / request 0x40000009) and a zero request; the compared cases take the plain allocation.
void func_08B93B20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t length = ctx.gpr[4];
    const std::uint32_t old = ctx.gpr[5];
    constexpr std::uint32_t kMax = 0x3FFFFFFCu;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[6] = kMax;
    const std::uint32_t original = length;
    if (kMax < length) {
        ctx.gpr[4] = 0x08C9F670u;
        (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08C9F670u});
    }
    std::uint32_t request;
    std::uint32_t size = original + 0x1Du;
    if (old < original) {
        const std::uint32_t twice = old << 1;
        if (original < twice) length = twice;
        size = length + 0x1Du;
    }
    request = length + 0xDu;
    ctx.gpr[3] = size;
    if (!(size < 0x1001u) && old < length) {
        size = 0x1000u - (size & 0xFFFu);
        ctx.gpr[3] = size;
        length += size;
        request = length + 0xDu;
        if (kMax < length) {
            length = kMax;
            request = 0x40000009u;
        }
    }
    ctx.gpr[2] = 0u;
    if (request != 0u) {
        ctx.gpr[4] = request;
        ctx.gpr[2] = call_keep(rt, ctx, 0x08B35DA0u, {request});
    }
    put(rt, ctx.gpr[2] + 4u, length);
}

// func_08BEEE28(list): destroys a circular list of 12-byte nodes whose [8] is the characters of a string representation: for every node the representation (characters - 0xC) that is not the shared empty
// one (D_08D74FD0) gets func_08B60804(characters - 4, -1) (the reference count decrement); a result of 0 or less runs func_08B8542C(representation, &local); the node is freed by func_08A5BC54(node, 0xC, 4).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty list, a representation other than the shared empty one (the func_08B60804 decrement and, for an answer of 0 or less, func_08B8542C) and a list of more than one node; every compared node held the shared empty representation.
void func_08BEEE28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t head = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t node = word(rt, head);
    if (node == head) return;
    for (;;) {
        const std::uint32_t characters = word(rt, node + 8u);
        ctx.gpr[5] = 0xFFFFFFFFu;
        const std::uint32_t next = word(rt, node);
        const std::uint32_t representation = characters - 0xCu;
        ctx.gpr[4] = characters - 4u;
        if (representation != 0x08D74FD0u) {
            const std::uint32_t answer = call_keep(rt, ctx, 0x08B60804u, {characters - 4u, 0xFFFFFFFFu});
            ctx.gpr[4] = representation;
            ctx.gpr[5] = frame.at(0u);
            if (!(signed_of(answer) > 0)) (void)call_keep(rt, ctx, 0x08B8542Cu, {representation, frame.at(0u)});
        }
        ctx.gpr[4] = node;
        ctx.gpr[5] = 0xCu;
        ctx.gpr[6] = 4u;
        (void)call_keep(rt, ctx, 0x08A5BC54u, {node, 0xCu, 4u});
        node = next;
        if (next == head) break;
    }
}

// func_08BE3B38(this, alignment): with the list this + 0xC locked (func_08B344E0 .. func_08B3457C), the biggest usable space of the blocks chained from this[4] (next at +8, start +0x14, size +0x18): for each
// block the start is rounded up to the alignment (a power of two: (start + alignment - 1) & -alignment) and the size reduced by that gap and rounded down to the alignment; v0 = the biggest (unsigned), 0 for no block.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a list with more than one block (the `movn` keeping the biggest); every compared list had one block.
void func_08BE3B38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], mask_base = ctx.gpr[5] - 1u;
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08B344E0u, {self + 0xCu});
    std::uint32_t node = word(rt, self + 4u);
    std::uint32_t best = 0u;
    ctx.gpr[5] = node;
    if (node != 0u) {
        const std::uint32_t mask = ~mask_base;
        ctx.gpr[6] = mask;
        do {
            const std::uint32_t start = word(rt, node + 0x14u);
            const std::uint32_t size = word(rt, node + 0x18u);
            node = word(rt, node + 8u);
            const std::uint32_t gap = ((mask_base + start) & mask) - start;
            const std::uint32_t usable = (size - gap) & mask;
            ctx.gpr[4] = start;
            ctx.gpr[3] = usable;
            ctx.gpr[2] = best < usable ? 1u : 0u;
            if (best < usable) best = usable;
        } while (node != 0u);
        ctx.gpr[5] = node;
    }
    (void)call_keep(rt, ctx, 0x08B3457Cu, {self + 0xCu});
    ctx.gpr[2] = best;
}

// func_08C27DC8(owner, first, last): for every node from `first` up to (not including) `last`, func_08BFEBAC(owner, owner + 4, node + 0x10) runs, and the next node is func_08B5F16C(node). v0 = the last
// next-node answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): first == last (nothing runs).
void func_08C27DC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4], last = ctx.gpr[6];
    std::uint32_t node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    if (last == node) return;
    do {
        ctx.gpr[6] = node + 0x10u;
        ctx.gpr[5] = owner + 4u;
        ctx.gpr[4] = owner;
        (void)call_keep(rt, ctx, 0x08BFEBACu, {owner, owner + 4u, node + 0x10u});
        ctx.gpr[4] = node;
        const std::uint32_t next = call_keep(rt, ctx, 0x08B5F16Cu, {node});
        node = next;
    } while (last != node);
}

// func_08A0BA5C(a0, a1, object): nothing (v0 = 0) for a null object; func_089660E8(object) answering non-zero also gives 0; otherwise the virtual slot 0x7E (offset 0x1F8) of the object runs as the tail call
// with (object, a1, a0).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null object and a non-zero func_089660E8 answer (both leave with v0 = 0).
void func_08A0BA5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], object = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[4] = object;
        if (object == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const std::uint32_t answer = call_keep(rt, ctx, 0x089660E8u, {object});
        ctx.gpr[4] = object;
        ctx.gpr[5] = a1;
        ctx.gpr[6] = a0;
        if (answer != 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 0x1F8u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[25] = function;
    }
    tail_call(ctx, ctx.gpr[25], {object, a1, a0});
}

// func_08B71A90(a, b, c, d): two 64-bit values {a, b} and {c, d} (as stored to the stack) are unpacked by func_08B72314(&value, &unpacked) and when both unpacked class words ([0] unpacked < 2) are 2 or more
// func_08B72440(&unpacked1, &unpacked2) answers; otherwise v0 = v1 = -1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an unpacked class word below 2 in either value (v0 = v1 = -1).
void func_08B71A90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6], d = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0u), a);
    put(rt, frame.at(4u), b);
    ctx.gpr[5] = frame.at(0x10u);
    ctx.gpr[4] = frame.at(0u);
    put(rt, frame.at(0xCu), d);
    put(rt, frame.at(8u), c);
    (void)call_keep(rt, ctx, 0x08B72314u, {frame.at(0u), frame.at(0x10u)});
    ctx.gpr[4] = frame.at(8u);
    ctx.gpr[5] = frame.at(0x28u);
    (void)call_keep(rt, ctx, 0x08B72314u, {frame.at(8u), frame.at(0x28u)});
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[2] = word(rt, frame.at(0x10u)) < 2u ? 1u : 0u;
    if (ctx.gpr[2] != 0u || word(rt, frame.at(0x28u)) < 2u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const std::uint32_t answer = call_keep(rt, ctx, 0x08B72440u, {frame.at(0x10u), frame.at(0x28u)});
    ctx.gpr[3] = answer;
    ctx.gpr[2] = answer;
}

// func_08A34DA8(this): f12 = this's float at +0x24, f13 = 0.0, f14 = 1.0 and the tail call func_08A34D78(this).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08A34DA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.fpr[12] = float_at(rt, self + 0x24u);
    ctx.fpr[14] = 1.0f;
    ctx.fpr[13] = 0.0f;
    tail_call(ctx, 0x08A34D78u, {self});
}

// func_08B515FC(this, a1, a2): this[0] = D_08CDAC58, this[0x14] = func_08B51508(this), the block func_08B62344(0x28) is filled by func_08B52238(block, a1, a2, this[0x14]) and kept in this[0xC20];
// this[0xC] = this[4] = this[8] = 0. v0 is func_08B52238's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B515FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CDAC58u);
    ctx.gpr[2] = 0x08CDAC58u;
    const std::uint32_t first = call_keep(rt, ctx, 0x08B51508u, {self});
    ctx.gpr[4] = 0x28u;
    put(rt, self + 0x14u, first);
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x28u});
    ctx.gpr[5] = a1;
    ctx.gpr[6] = a2;
    ctx.gpr[7] = first;
    ctx.gpr[4] = block;
    (void)call_keep(rt, ctx, 0x08B52238u, {block, a1, a2, first});
    put(rt, self + 0xC20u, block);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
}

// func_08C3C6D4(this): func_08B0DF38(this), func_08B0C304(this + 0xF8); the sub-object this + 0xF8 gets [0x18] = D_08AC0290 and [0x1C] = 0; this[0x54] = this + 0xF8, this[0x6C] = D_08B10588,
// this[0x74] = D_08B10590, this[0x78] = 0, this[0x70] = 0. v0 = D_08B10588, v1 = D_08B10590.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08C3C6D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t inner = self + 0xF8u;
    (void)call_keep(rt, ctx, 0x08B0DF38u, {self});
    ctx.gpr[4] = inner;
    (void)call_keep(rt, ctx, 0x08B0C304u, {inner});
    put(rt, inner + 0x18u, 0x08AC0290u);
    put(rt, inner + 0x1Cu, 0u);
    put(rt, self + 0x54u, inner);
    put(rt, self + 0x6Cu, 0x08B10588u);
    put(rt, self + 0x74u, 0x08B10590u);
    put(rt, self + 0x78u, 0u);
    put(rt, self + 0x70u, 0u);
    ctx.gpr[2] = 0x08B10588u;
    ctx.gpr[3] = 0x08B10590u;
}

// func_08B18A58(out, source): copies the pair source[0], source[4] and the byte source[8] to out; when the byte is set the object source[0] gets its virtual slot 6 (offset 0x18) called as the tail call
// with (object, source[4]); v0 = out otherwise.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the byte source[8] set (the virtual slot 6 tail call): every compared source had it clear and left with v0 = out.
void func_08B18A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5];
    const std::uint32_t object = word(rt, source);
    const std::uint32_t flag = rt.memory().load8(source + 8u);
    const std::uint32_t second = word(rt, source + 4u);
    rt.memory().store8(out + 8u, static_cast<std::uint8_t>(flag));
    put(rt, out, object);
    put(rt, out + 4u, second);
    ctx.gpr[7] = object;
    ctx.gpr[6] = flag;
    ctx.gpr[3] = second;
    ctx.gpr[2] = out;
    ctx.gpr[4] = object;
    ctx.gpr[5] = second;
    if (flag == 0u) return;
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x18u);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object, second});
}

// func_088A8B14(this, a1, a2, f12): this[0] = D_08CC2C68; the two pointers swap roles when the first is 0 (a3 = a1 or a2, a2 = a2 or a1 when a2 is 0); this[0x18] = a2 + 0xAC (0 for a null a2),
// this[4] = a3, this[0xC] = 1.0f - f12, this[0x10] = f12, this[0x14] = D_08CC4110, this[8] = a2. v0 = D_08CC4110, v1 = this[0x18].
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a1 == 0 (a3 takes a2), a2 == 0 (a2 takes a1) and a null final a2 (this[0x18] = 0).
void func_088A8B14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    std::uint32_t a2 = ctx.gpr[6];
    const float f12 = ctx.fpr[12];
    const float complement = 1.0f - f12;
    ctx.fpr[0] = complement;
    std::uint32_t a3 = a1;
    if (a1 == 0u) a3 = a2;
    const std::uint32_t original = a2;
    if (original == 0u) a2 = a1;
    std::uint32_t v1 = a2 + 0xACu;
    if (a2 == 0u) v1 = 0u;
    put(rt, self, 0x08CC2C68u);
    put(rt, self + 0x18u, v1);
    put(rt, self + 4u, a3);
    put_float(rt, self + 0xCu, complement);
    put_float(rt, self + 0x10u, f12);
    put(rt, self + 0x14u, 0x08CC4110u);
    put(rt, self + 8u, a2);
    ctx.gpr[7] = a3;
    ctx.gpr[6] = a2;
    ctx.gpr[3] = v1;
    ctx.gpr[2] = 0x08CC4110u;
}

// func_08C1ED14(tree, key): the lower bound in a binary tree (nodes: left +8, right +0xC, key +0x10 as signed words; root tree[8], header tree + 4): a node whose key is below *key goes right, otherwise it
// becomes the answer and the walk goes left. v0 = the last node that went left, or the header (tree + 4) when none did.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a node whose key is not below *key (the walk goes left and the node becomes the answer): every compared walk went right at every node it visited.
void func_08C1ED14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4];
    std::uint32_t node = word(rt, tree + 8u);
    std::uint32_t answer = tree + 4u;
    ctx.gpr[3] = node;
    if (node != 0u) {
        const std::uint32_t key = word(rt, ctx.gpr[5]);
        ctx.gpr[5] = key;
        std::uint32_t next;
        do {
            if (signed_of(word(rt, node + 0x10u)) < signed_of(key)) {
                next = word(rt, node + 0xCu);
            } else {
                next = word(rt, node + 8u);
                answer = node;
            }
            node = next;
        } while (next != 0u);
        ctx.gpr[3] = 0u;
    }
    ctx.gpr[4] = answer;
    ctx.gpr[2] = answer;
}

// func_08B487B8(this): when the byte this[4] is set and the object this[0] is non-null, func_08B35254(object) runs; then the byte this[4] and the word this[0] are cleared. v0 is 0 unless that call ran.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0] == 0 with the byte set (the call is skipped).
void func_08B487B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = rt.memory().load8(self + 4u);
    if (ctx.gpr[2] != 0u) {
        const std::uint32_t object = word(rt, self);
        ctx.gpr[2] = object;
        if (object != 0u) {
            ctx.gpr[4] = object;
            (void)call_keep(rt, ctx, 0x08B35254u, {object});
        }
    }
    rt.memory().store8(self + 4u, 0u);
    put(rt, self, 0u);
}

// func_08A0BE48(this, a1, a2): func_08A5BC20(0x24, 4) allocates, func_08A0BA2C(block, a1, a2) fills it, and func_08965854(this, &block) takes the block pointer (stored on the stack). v0 is the last call's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08A0BE48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = 0x24u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    ctx.gpr[4] = block;
    ctx.gpr[5] = a1;
    ctx.gpr[6] = a2;
    (void)call_keep(rt, ctx, 0x08A0BA2Cu, {block, a1, a2});
    ctx.gpr[4] = self;
    ctx.gpr[5] = frame.at(0u);
    put(rt, frame.at(0u), block);
    (void)call_keep(rt, ctx, 0x08965854u, {self, frame.at(0u)});
}

// func_08B71328(a, b, c, d): the two 64-bit values {a, b} and {c, d} are unpacked (func_08B72314 into locals), func_08B70F70(&first, &second, &result) combines them and func_08B72030(its answer) packs the
// result; v0 = that last answer.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B71328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6], d = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    put(rt, frame.at(0u), a);
    ctx.gpr[4] = frame.at(0u);
    put(rt, frame.at(4u), b);
    ctx.gpr[5] = frame.at(0x10u);
    put(rt, frame.at(0xCu), d);
    put(rt, frame.at(8u), c);
    (void)call_keep(rt, ctx, 0x08B72314u, {frame.at(0u), frame.at(0x10u)});
    ctx.gpr[4] = frame.at(8u);
    ctx.gpr[5] = frame.at(0x28u);
    (void)call_keep(rt, ctx, 0x08B72314u, {frame.at(8u), frame.at(0x28u)});
    ctx.gpr[4] = frame.at(0x10u);
    ctx.gpr[5] = frame.at(0x28u);
    ctx.gpr[6] = frame.at(0x40u);
    const std::uint32_t combined = call_keep(rt, ctx, 0x08B70F70u, {frame.at(0x10u), frame.at(0x28u), frame.at(0x40u)});
    ctx.gpr[4] = combined;
    (void)call_keep(rt, ctx, 0x08B72030u, {combined});
}

// func_089626C8(a0): the owner func_089624EC() answers a pointer cell: its content (if any) gets func_08B2D1A8(content + 8) and the count is that minus 0x80 (0 when the answer is at most 0x80 or there
// is no content); the owner of func_089624F4(a0) gets func_08BE3B38(content + 8, 0x80) the same way (0 without content). v0 = the first count, v1 = the second answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): no content in the first cell (count 0).
void func_089626C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argument = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first_cell = call_keep(rt, ctx, 0x089624ECu, {argument});
    std::uint32_t content = word(rt, first_cell);
    ctx.gpr[3] = content;
    ctx.gpr[4] = content + 8u;
    std::uint32_t count = 0u;
    if (content != 0u) {
        const std::uint32_t answer = call_keep(rt, ctx, 0x08B2D1A8u, {content + 8u});
        ctx.gpr[3] = signed_of(answer) < 0x81 ? 1u : 0u;
        if (signed_of(answer) >= 0x81) count = answer - 0x80u;
    }
    ctx.gpr[4] = argument;
    const std::uint32_t second_cell = call_keep(rt, ctx, 0x089624F4u, {argument});
    content = word(rt, second_cell);
    ctx.gpr[5] = 0x80u;
    ctx.gpr[4] = content + 8u;
    ctx.gpr[2] = 0u;
    if (content != 0u) (void)call_keep(rt, ctx, 0x08BE3B38u, {content + 8u, 0x80u});
    ctx.gpr[3] = ctx.gpr[2];
    ctx.gpr[2] = count;
}

// func_08B658FC(this): this[4] = 6, this[0xC] = 0x1002, this[8] = 0; a local (stack) object is built by func_08B5E0EC(&local), func_08B5D1E0(this + 0x6C, &local) uses it and func_08B5D34C(&local)
// destroys it. v0 is the last call's.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08B658FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self + 4u, 6u);
    ctx.gpr[2] = 0x1002u;
    ctx.gpr[4] = frame.at(0u);
    put(rt, self + 0xCu, 0x1002u);
    put(rt, self + 8u, 0u);
    (void)call_keep(rt, ctx, 0x08B5E0ECu, {frame.at(0u)});
    ctx.gpr[4] = self + 0x6Cu;
    ctx.gpr[5] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B5D1E0u, {self + 0x6Cu, frame.at(0u)});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08B5D34Cu, {frame.at(0u)});
}

// func_08BE40AC(this, a1, a2): with the list this + 0xC locked (func_08B344E0 .. func_08B3457C) the block func_08BE3C80(this, a1, a2) is found; v0 = block[0x14], or 0 when there is none.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): no block found (v0 = 0).
void func_08BE40AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08B344E0u, {self + 0xCu});
    ctx.gpr[4] = self;
    ctx.gpr[5] = a1;
    ctx.gpr[6] = a2;
    const std::uint32_t block = call_keep(rt, ctx, 0x08BE3C80u, {self, a1, a2});
    ctx.gpr[4] = self + 0xCu;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {self + 0xCu});
    ctx.gpr[2] = 0u;
    if (block != 0u) ctx.gpr[2] = word(rt, block + 0x14u);
}

// func_08962918(this, a1, a2): func_08BE43B0(this), then func_08BE3FB4(this + 8, this[0], a2, &local) and this[0x3C] = func_08BE3B38(this + 8, 0x80); func_08B28D34(this + 0x40) is the last call (v0 its answer).
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08962918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08BE43B0u, {self});
    const std::uint32_t inner = self + 8u;
    const std::uint32_t head = word(rt, self);
    ctx.gpr[5] = head;
    ctx.gpr[6] = a2;
    ctx.gpr[7] = frame.at(0u);
    ctx.gpr[4] = inner;
    (void)call_keep(rt, ctx, 0x08BE3FB4u, {inner, head, a2, frame.at(0u)});
    ctx.gpr[4] = inner;
    ctx.gpr[5] = 0x80u;
    const std::uint32_t biggest = call_keep(rt, ctx, 0x08BE3B38u, {inner, 0x80u});
    put(rt, self + 0x3Cu, biggest);
    ctx.gpr[4] = self + 0x40u;
    (void)call_keep(rt, ctx, 0x08B28D34u, {self + 0x40u});
}

// func_08C01DFC(this, first, last): erases [first, last) of the buffer whose end is this[4]: func_08B79BAC(first, last, this[4] - last) moves the tail, func_08C01DDC(first + that length, this[4], &local)
// follows, this[4] -= last - first. v0 = first, v1 = the new end.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08C01DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t removed = last - first;
    const std::uint32_t tail = word(rt, self + 4u) - last;
    ctx.gpr[4] = first;
    ctx.gpr[5] = last;
    ctx.gpr[6] = tail;
    (void)call_keep(rt, ctx, 0x08B79BACu, {first, last, tail});
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[4] = tail + first;
    ctx.gpr[6] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08C01DDCu, {tail + first, end, frame.at(0u)});
    const std::uint32_t new_end = word(rt, self + 4u) - removed;
    ctx.gpr[3] = new_end;
    put(rt, self + 4u, new_end);
    ctx.gpr[2] = first;
}

// func_08BE43E4(this, a1, a2): this[0] = a1; the 0x14-byte block func_08B62344(0x14) gets { D_08CC6958, 1, 1, a1, a2 } at +0, +4, +8, +0xC, +0x10, this[4] = block, and func_08B8E380(this + 4, a1, a1, 1) is
// the tail call.
// Branches: none that the compared cases skip (a straight line, or every branch took both ways).
void func_08BE43E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        put(rt, self, a1);
        ctx.gpr[4] = 0x14u;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x14u});
        put(rt, block + 4u, 1u);
        put(rt, block, 0x08CC6958u);
        put(rt, block + 0x10u, a2);
        put(rt, block + 8u, 1u);
        put(rt, block + 0xCu, a1);
        put(rt, self + 4u, block);
        ctx.gpr[3] = 0x08CC6958u;
        ctx.gpr[7] = 1u;
    }
    tail_call(ctx, 0x08B8E380u, {self + 4u, a1, a1, 1u});
}

// func_0890B2F4(out): a 0x18-byte settings object from func_08B62344(0x18): [0] = 0, byte [4] = 1, [8] = D_08CC57B0, [0xC] = 0x40333333 (2.8f), [0x10] = 0x461C4000, [0x14] = 0x3CBF258C; out[0] = it. out[4] is
// the guarded static D_08D769D8: while the guard byte D_08D769D0 is 0, func_08B6441C(guard) decides whether this call initialises it (D_08D769D8 = D_08CC5660 and func_08B64494(guard)); out[4] = D_08D769D8.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C answering 0 (the guard was taken by another caller: out[4] = D_08D769D8 without initialising).
void func_0890B2F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x18u});
    rt.memory().store8(block + 4u, 1u);
    put(rt, block, 0u);
    put(rt, block + 0x14u, 0x3CBF258Cu);
    put(rt, block + 8u, 0x08CC57B0u);
    put(rt, block + 0xCu, 0x40333333u);
    put(rt, block + 0x10u, 0x461C4000u);
    constexpr std::uint32_t kGuard = 0x08D769D0u, kStatic = 0x08D769D8u;
    put(rt, out, block);
    ctx.gpr[2] = static_cast<std::uint32_t>(byte_at(rt, kGuard));
    ctx.gpr[4] = kGuard;
    if (byte_at(rt, kGuard) == 0) {
        if (call_keep(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            put(rt, kStatic, 0x08CC5660u);
            ctx.gpr[4] = kGuard;
            (void)call_keep(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    put(rt, out + 4u, kStatic);
}

// func_08B66524(x) (f12): the cosine by range: |x| (bit pattern, sign cleared) up to 0x3F490FD8 (pi / 4): func_08B6DEF8(0, x, 0) (the cosine kernel); a non-finite x (above 0x7F7FFFFF) answers x - x; otherwise
// func_08B6BAFC(&y) reduces x into y0, y1 (the stack) and the quadrant (v0 & 3): 0 -> kernel_cos(1, y0, y1), 1 -> kernel_sin(y0, y1) (func_08B6D564), 2 -> -kernel_cos(1, y0, y1), 3 -> -kernel_sin(y0, y1).
// The same shape as batch 81's func_08B662C4 with the quadrants swapped. f0 is compared (ReturnKind::Float).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-finite x (x - x), the quadrants 0, 2 and 3 (func_08B6DEF8(1) plain / negated, func_08B6D564 negated): the compared cases take |x| below pi / 4 and quadrant 1.
void func_08B66524(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const std::uint32_t magnitude = std::bit_cast<std::uint32_t>(x) & 0x7FFFFFFFu;
    GuestFrame frame(ctx, 0x20u);
    if (!(signed_of(0x3F490FD8u) < signed_of(magnitude))) {
        ctx.fpr[13] = 0.0f;
        ctx.fpr[0] = call_keep(rt, ctx, 0x08B6DEF8u, {0u}, {x, 0.0f}).f0;
        return;
    }
    if (signed_of(0x7F7FFFFFu) < signed_of(magnitude)) {
        ctx.fpr[0] = x - x;
        return;
    }
    const std::uint32_t quadrant = call_keep(rt, ctx, 0x08B6BAFCu, {frame.at(0u)}, {}).v0 & 3u;
    const float y0 = float_at(rt, frame.at(0u)), y1 = float_at(rt, frame.at(4u));
    ctx.fpr[12] = y0;
    ctx.fpr[13] = y1;
    if ((quadrant & 1u) == 0u) {
        float f0 = call_keep(rt, ctx, 0x08B6DEF8u, {1u}, {y0, y1}).f0;
        if (quadrant == 2u) f0 = -f0;
        ctx.fpr[0] = f0;
    } else {
        float f0 = call_keep(rt, ctx, 0x08B6D564u, {}, {y0, y1}).f0;
        if (quadrant == 3u) f0 = -f0;
        ctx.fpr[0] = f0;
    }
}

// func_088693CC(this, a1, a2): with the list this locked (func_08B344E0 .. func_08B3457C) the object func_0886A020(this + 0x1B0, a1, a2) is asked through its virtual slot 6 (offset 0x18); v0 = that answer,
// 0 when there is no object.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): no object (the `beqz` with s0 = 0: v0 = 0).
void func_088693CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08B344E0u, {self});
    ctx.gpr[5] = a1;
    ctx.gpr[4] = self + 0x1B0u;
    const std::uint32_t object = call_keep(rt, ctx, 0x0886A020u, {self + 0x1B0u, a1, a2});
    ctx.gpr[4] = object;
    std::uint32_t answer = 0u;
    if (object != 0u) {
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 0x18u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        answer = call_keep(rt, ctx, function, {object});
    }
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {self});
    ctx.gpr[2] = answer;
}

// func_089445D8(this, kind): a permission test for a kind 1..6 against the flag bytes of the record this[8]: kinds of 0 or less answer 6, kinds of 7 or more answer -1; a record whose byte +3 is 2, or whose
// byte +0xE is set, answers 0. The bit of the kind (the jump table D_08C8B394: 1 -> 1, 2 -> 2, 3 -> 4, 4 -> 8, 5 and 6 -> 0x10) in the byte +0xB answers 0 when set; otherwise kind 3 answers 1 when
// func_08944588(this) is non-zero (else 0), kind 2 answers 1 when the byte +0x541 is non-zero (else 0), and every other kind answers 1. v0 = v1 = the answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): kinds of 0 or less (v0 = 6) and of 7 or more (v0 = -1), the record byte +3 == 2, byte +0xE set, the kind's bit set in byte +0xB (all 0), func_08944588 answering 0 and kind 2 (byte +0x541 decides): the compared cases only reach the answers 1 (kinds other than 2 / 3, or kind 3 with a non-zero func_08944588).
void func_089445D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], kind = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const auto leave = [&ctx](std::uint32_t answer) {
        ctx.gpr[3] = answer;
        ctx.gpr[2] = answer;
    };
    ctx.gpr[6] = self;
    if (signed_of(kind) <= 0) return leave(6u);
    if (!(signed_of(kind) < 7)) return leave(0xFFFFFFFFu);
    const std::uint32_t record = word(rt, self + 8u);
    ctx.gpr[7] = record;
    if (byte_at(rt, record + 3u) == 2) return leave(0u);
    if (rt.memory().load8(record + 0xEu) != 0u) return leave(0u);
    static constexpr std::uint32_t kMask[7] = {0u, 1u, 2u, 4u, 8u, 0x10u, 0x10u};
    if ((rt.memory().load8(record + 0xBu) & kMask[kind]) != 0u) return leave(0u);
    if (kind == 3u) {
        ctx.gpr[4] = self;
        return leave(call_keep(rt, ctx, 0x08944588u, {self}) != 0u ? 1u : 0u);
    }
    if (kind != 2u) return leave(1u);
    return leave(byte_at(rt, record + 0x541u) != 0 ? 1u : 0u);
}

// func_08A782D8(this, source): a map copy: func_08A5B860(this) first, this[0x10] = 0, this[0] = D_08CD3C58, this[0xC] = source[0xC]; with a root source[0x10] the 0x240-byte header node
// func_08A5BC20(0x240, 4) is built (empty links to itself at +4, +0xC, +0x10 and [8], [0x14] = 0), the subtree source[0x10][8] is copied by func_08C2605C(node, source[0x10][8]) into node[8],
// node[0xC] becomes its leftmost node (following [8]) and node[0x10] its rightmost (following [0xC]); node[0x14] = source[0x10][0x14] and this[0x10] = node. v0 as the instructions leave it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-empty source (the whole copy: the header node, func_08C2605C, the leftmost / rightmost walks, node[0x14]): every compared source was empty (v0 = source[0xC]); the body was read from the asm only.
void func_08A782D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x08A5B860u, {self});
    put(rt, self + 0x10u, 0u);
    std::uint32_t v0 = word(rt, source + 0xCu);
    const std::uint32_t root = word(rt, source + 0x10u);
    ctx.gpr[2] = v0;
    ctx.gpr[3] = 0x08CD3C58u;
    put(rt, self, 0x08CD3C58u);
    put(rt, self + 0xCu, v0);
    if (root == 0u) return;
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = 0x240u;
    const std::uint32_t node = call_keep(rt, ctx, 0x08A5BC20u, {0x240u, 4u});
    put(rt, node + 8u, 0u);
    const std::uint32_t links = node + 4u;
    const std::uint32_t subtree = word(rt, root + 8u);
    put(rt, node + 0x14u, 0u);
    put(rt, node + 4u, 0u);
    put(rt, node + 0xCu, links);
    put(rt, node + 0x10u, links);
    ctx.gpr[6] = links;
    ctx.gpr[5] = subtree;
    if (subtree != 0u) {
        ctx.gpr[4] = node;
        std::uint32_t copy = call_keep(rt, ctx, 0x08C2605Cu, {node, subtree});
        put(rt, node + 8u, copy);
        std::uint32_t at = copy;
        for (;;) {
            const std::uint32_t left = word(rt, at + 8u);
            ctx.gpr[3] = left;
            if (left == 0u) {
                put(rt, node + 0xCu, at);
                break;
            }
            at = left;
        }
        std::uint32_t walk = word(rt, node + 8u);
        for (;;) {
            const std::uint32_t right = word(rt, walk + 0xCu);
            ctx.gpr[2] = right;
            if (right == 0u) {
                put(rt, node + 0x10u, walk);
                break;
            }
            walk = right;
        }
        v0 = word(rt, root + 0x14u);
        put(rt, node + 0x14u, v0);
        ctx.gpr[2] = v0;
    }
    put(rt, self + 0x10u, node);
}

}  // namespace
}  // namespace hot_batch_83
}  // namespace gt::game

#define HOT83(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_83::name, gt::ReturnKind::kind)
HOT83(0x08B1C820u, func_08B1C820, Word);
HOT83(0x08BB9130u, func_08BB9130, Float);
HOT83(0x08A32618u, func_08A32618, Word);
HOT83(0x08B488BCu, func_08B488BC, Word);
HOT83(0x08ADBFE0u, func_08ADBFE0, Word);
HOT83(0x08B6441Cu, func_08B6441C, Word);
HOT83(0x08A9DD14u, func_08A9DD14, Word);
HOT83(0x08AE6B34u, func_08AE6B34, Word);
HOT83(0x08AE85E8u, func_08AE85E8, Word);
HOT83(0x08B344B0u, func_08B344B0, Word);
HOT83(0x08B0B36Cu, func_08B0B36C, Word);
HOT83(0x08B85E00u, func_08B85E00, Word);
HOT83(0x08BFD224u, func_08BFD224, Word);
HOT83(0x08C3E24Cu, func_08C3E24C, Word);
HOT83(0x08C3DB68u, func_08C3DB68, Word);
HOT83(0x08BAC284u, func_08BAC284, Word);
HOT83(0x08C465ECu, func_08C465EC, Word);
HOT83(0x08B52730u, func_08B52730, Word);
HOT83(0x08B77B1Cu, func_08B77B1C, Word);
HOT83(0x08B5E0ECu, func_08B5E0EC, Word);
HOT83(0x08B8D96Cu, func_08B8D96C, Word);
HOT83(0x08BEE778u, func_08BEE778, Word);
HOT83(0x08B89054u, func_08B89054, Word);
HOT83(0x08AFEAB8u, func_08AFEAB8, Word);
HOT83(0x08C49140u, func_08C49140, Word);
HOT83(0x08A63F1Cu, func_08A63F1C, Word);
HOT83(0x08C31920u, func_08C31920, Word);
HOT83(0x08B7C9E4u, func_08B7C9E4, Word);
HOT83(0x08AB85F0u, func_08AB85F0, Word);
HOT83(0x08BF913Cu, func_08BF913C, Word);
HOT83(0x08C30E0Cu, func_08C30E0C, Word);
HOT83(0x08B93B20u, func_08B93B20, Word);
HOT83(0x08BEEE28u, func_08BEEE28, Word);
HOT83(0x08BE3B38u, func_08BE3B38, Word);
HOT83(0x08C27DC8u, func_08C27DC8, Word);
HOT83(0x08A0BA5Cu, func_08A0BA5C, Word);
HOT83(0x08B71A90u, func_08B71A90, Word);
HOT83(0x08A34DA8u, func_08A34DA8, Word);
HOT83(0x08B515FCu, func_08B515FC, Word);
HOT83(0x08C3C6D4u, func_08C3C6D4, Word);
HOT83(0x08B18A58u, func_08B18A58, Word);
HOT83(0x088A8B14u, func_088A8B14, Word);
HOT83(0x08C1ED14u, func_08C1ED14, Word);
HOT83(0x08B487B8u, func_08B487B8, Word);
HOT83(0x08A0BE48u, func_08A0BE48, Word);
HOT83(0x08B71328u, func_08B71328, DoubleWord);
HOT83(0x089626C8u, func_089626C8, DoubleWord);
HOT83(0x08B658FCu, func_08B658FC, Word);
HOT83(0x08BE40ACu, func_08BE40AC, Word);
HOT83(0x08962918u, func_08962918, Word);
HOT83(0x08C01DFCu, func_08C01DFC, Word);
HOT83(0x08BE43E4u, func_08BE43E4, Word);
HOT83(0x0890B2F4u, func_0890B2F4, Word);
HOT83(0x08B66524u, func_08B66524, Float);
HOT83(0x088693CCu, func_088693CC, Word);
HOT83(0x089445D8u, func_089445D8, DoubleWord);
HOT83(0x08A782D8u, func_08A782D8, Word);
