// New batch, main session 1023: the five large functions (294-297 instructions) that were drafted before and either failed a
// gate route or were dropped twice as too hard. Written as structured C from `tools/disasm`; a draft batch, not verified here.
//
// Four of them are one family — the reference-counted "take a counted object and store it in a pair" shape (the strong count
// at +4, the weak count at +8, the vtable words at +8 / +0xC running when a count reaches zero, the ll / sc pairs read as a
// plain load and store: one host thread, and the `sc` success flag is 1 in v0 either way). Their earlier drafts lived in
// hot_batch_117 (0x0890F1C0, 0x08BE476C, 0x0894106C) and hot_batch_118 (0x08BC728C), registered nowhere ("compared 0 in the
// full gate": no case reached them with valid arguments, which is what the input shapes below are for). Each line was re-read
// against the asm here; where the earlier draft passed a register it had itself computed, the asm's live value is passed.
// 0x08B50EFC comes from the draft in 82aa861 (new_batch_main_47.cpp) that a5061323 dropped after a fast-gate route answered
// f0 = 0.187 where the original answered 1.0. Its determinant chain (asm up to the store at 08B51080) and the exit are
// re-checked against the asm; the answer is the word the function stores at sp+0 (08B51364 reads it back), not a local, and
// that store was missing from the old draft.
// No names: none of the five has evidence for one.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1023 {
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
// A guest call with float arguments ($f12, ...) whose $f0 is read afterwards.
GuestResult call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                   std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The `ll / sc` pair the originals use for every count step: read, add, write, retry while the store-condition failed.
void count_up(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
// The same, decrementing; true when the old value was 1, i.e. the step that takes the count to zero.
bool count_down(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old == 1u;
}

// A counted block's release: the strong count at +4 steps down and, when that was the last, virtual +8 runs; then the weak
// count at +8 steps down and its virtual +0xC runs when that was the last.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (count_down(rt, object + 4u)) ctx.gpr[2] = call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (count_down(rt, object + 8u)) ctx.gpr[2] = call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// The saved registers the long bodies touch: s0-s7, fp and f20-f31 (f17 of 0x08B50EFC is a scratch spill, not a save).
struct SavedAll {
    AllegrexContext &ctx;
    u32 gpr[9];
    float fpr[12];
    explicit SavedAll(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) gpr[i] = c.gpr[16u + i];
        gpr[8] = c.gpr[30];
        for (unsigned i = 0u; i < 12u; ++i) fpr[i] = c.fpr[20u + i];
    }
    ~SavedAll() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = gpr[i];
        ctx.gpr[30] = gpr[8];
        for (unsigned i = 0u; i < 12u; ++i) ctx.fpr[20u + i] = fpr[i];
    }
};

// 0x08BC728C (result = a0, holder = a1, key = a2, item = a3, tag = t0) -> result: with the owner O = *holder (and {0, 0}
// returned when it is null) locked through O + 0x6C, the list whose sentinel is O + 0x98 (nodes: next +0, a string object +8
// whose length sits at -0xC) is walked comparing each node's text with the key's (*key, length at -0xC) over the shorter
// length (0x08B36170) and then the lengths. A node found with item == 0 is erased (0x08BBFE68) and {0, 0} returned; with an
// item, 0x08B2CB18(O + 8, item, tag) is the new handle h, a 0x18-byte record {vtable 0x08CC52B8, 1, 1, h, *holder, shared} is
// allocated, 0x08B8E380 links it, the node's +0xC becomes h and its +0x10 shared slot is reassigned (weak counts stepped);
// result = {h, record}. Ends with 0x08BBCFA0(holder); v0 = result.

// 0x08BE476C (out = a0, pair = a1, argument = a2, extra = a3) -> out: pair is {object +0, counted block +4}. An empty pair
// zeroes the two frame slots; otherwise, under the block's own lock at object + 0x6C (0x08B34888 / 0x08B34B24),
// 0x08B2CB18(object + 8, argument, extra) is the handle h and a 0x18-byte record {vtable 0x08CC2F90, 1, 1, h, object, block}
// is allocated (0x08B62344) with the block's use count stepped up four times and down three (the releases), 0x08B8E380 links
// it and frame +8 = h, frame +0xC = the record. Then 0x08BE43B0(frame +0, frame +8) fills a second pair, a 0xA0-byte object is
// allocated and initialised by 0x08BE45C4(object, frame +0, argument, 0), stored at out +0, and a 0x10-byte record {vtable
// 0x08CC6928, 1, 1, object} at out +4; 0x08B8E380(out + 4, object, object) links it, with the object destroyed through
// 0x08BE427C / 0x08B28BE0 / 0x08B2C3BC / 0x08B64654 when that record is null. Both frame blocks (+4, +0xC) are released.

// 0x0890F1C0 (out = a0, owner = a1, key = a2) -> 0 or 1: 0x08B0EC60(frame + 0x60) fills a 0x3C-byte record that is copied to
// frame +0x18; a non-zero first word returns 0. 0x089624EC(key) answers a pair {object +0, counted block +4} (empty leaves
// frame +0x10 / +0x14 zero). Otherwise, under the lock at object + 0x6C, 0x08B2CB18(object + 8, record word +0x28, 0x80) is
// the handle h; a 0x18-byte record {vtable 0x08CC2F90, 1, 1, h, object, block} is allocated (0x08B62344), the block's count is
// stepped three times (one up before the record, two after) and the record linked by 0x08B8E380(frame +0x14, h, h); frame
// +0x10 = h. out is assigned the pair {frame +0x10, frame +0x14} the way a shared pointer is (old block released). When
// out's word is non-zero a closure {0x0890EF6C, out, that word, record word +0x28} is registered with
// 0x08AC0C38(owner, closure pointer, closure, 1) and 0x08AC0B50() runs, answering 1; otherwise 0.

// 0x0894106C (self = a0, a1, first_word = a2, second_word = a3, mode = t0, f12-f15 = x, y, z, angle) -> 0: the object's frame
// setup. s = *self (its +0x14 is the listener whose virtual +8 takes the vector) and b = self->+8, with self->+0xC the source
// of the offsets; 0x089545BC(sp + 8) fills the four floats the pass starts from. For mode 2 the offset is b->+0, for mode 1 the
// negated b->+0x70; it rotates x / y through 0x08B498AC(angle). The vector {x, z, -y} plus first_word / second_word goes to the
// listener; s +0x10..+0x18 and +0x690 / +0x694 are written and 0x0893C3E4 / 0x08954870 run. For the four records
// 0x08955300(self, i) with the transform 0x0893C780: the position, the words +0x690 / +0x694, the listener, the flag byte
// +0xE9..+0xED / floats +0xBC / +0xD0 (0x08954618 / 0x08954670 / 0x08954688) and the two counters s +0x69D / +0x69E; bit 4 of
// s +0xC is cleared. A second pass fills each record's floats +0x54 / +0x58 / +0xCC / +0x10 / +0x14 / +0x18 / +0x1C / +0xC
// from b through 0x0894E9B4 / 0x0894E714 / 0x0893D0F4 / 0x0893D064. Then 0x08945F6C(self, 1), the angle 0x08B4A430 adds to
// s +0x6FC, 0x0893C3E4 again, s +0x18, 0x08954870 and the copies into s +0x764..+0x774; unless byte s +3 is 4, 0x0895A004
// gets *self and byte s +0.

// 0x08B50EFC: the inverse of a 4x4 matrix given as 16 floats — f12-f19 are rows 0 and 1, the eight stack arguments at the
// caller's sp+0x20..0x3C (this frame's +0xA0..+0xBC) are rows 2 and 3 — written through the 16 pointers a0-a3, t0-t3 and the
// eight stack words at this frame's +0x80..+0x9C; f0 = the determinant. The body is one straight-line cofactor expansion, so
// its locals are named after the FPU registers it moves through: the order of the float operations is the identity of the
// function and has to be read against `tools/disasm`. The registers it spills to its frame are locals.
} // namespace
} // namespace new_batch_main_1023
} // namespace gt::game

#define NEW1023(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1023::name, gt::ReturnKind::kind)
