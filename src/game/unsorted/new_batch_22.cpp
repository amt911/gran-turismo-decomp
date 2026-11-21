// New batch 22: functions of 360-400 bytes that the recompiler emits, written by hand from `tools.disasm` as structured C
// (if/while/switch, named typed locals). Not verified yet (draft): no case has been captured for these.
// Delay slots are read as "before the branch / call". The callee-saved registers the original keeps values in are set
// where the asm sets them (a callee may read them) and put back at the end; `v0` (or `f0` for the float ones) ends as the
// asm leaves it. A call leaves v0, v1 and a1 as the callee did (a recompiled callee may read the caller-saved ones).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_22 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

// A guest call; afterwards v0/v1 and a1 hold the callee's, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
         std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

s8 signed_byte_at(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
s16 signed_half_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
u8 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u16 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u8 value) { rt.memory().store8(address, value); }
void put_half(Runtime &rt, u32 address, u16 value) { rt.memory().store16(address, value); }

// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// s0-s7 and fp are callee-saved: the body sets them where the original does (a callee may read them) and the caller's
// values come back at the end.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), fp_(ctx.gpr[30]) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 fp_;
    u32 saved_[8];
};

// The counted block a `word + 8` record carries: the block at `pointer` (its base is pointer - 0xC) is skipped when it is
// the shared empty block 0x08D74FD0, or when 0x08B60804(pointer - 4, -1) answers a positive count; otherwise
// 0x08B8542C(base, marker) frees it.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 pointer, u32 marker) {
    if (pointer - 0xCu == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {pointer - 0xCu, marker});
}
void release_counted_block(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 marker) {
    release_counted(rt, ctx, word(rt, slot), marker);
}

// 0x08BE6244 (a0 = a container whose word +4 is an end pointer, a1 = destination, a2 = source): assigns
// ((end - source) >> 5) / 3 records of 0x60 bytes one by one — two words, five floats, then the sub-objects at +0x20 and
// +0x40 (each assigned through 0x089641C4 unless it already is that address, its word +0x10 copied after) — then
// 0x08BE59D4(destination cursor, end, &local) finishes and the container's end pointer is reduced by the distance from
// the destination to the source with its low five bits cleared. v0 = the destination start.

// 0x0885F938 (a0 = this, a1 = an index, a2 = two 0x30-byte records, a3 = a value, t0 = a count (a negative one is
// taken as zero), t1 and t2 = two flags kept as bytes): for both records, 0x088D698C(this + 0x5AC) counts something and
// a positive answer gets 0x08897130(record); unless 0x088977D4(record) answers -1, the record's flags are set
// (0x0889715C, 0x088975B4), the entry selected by the byte at this + 0x4120 + index * 0x50 + i + 4 (or the t2 flag for
// -1) is adjusted (0x0885DF24), and 0x088F8A84(this + 0x5B4, index, count) finishes (a tail call).

// 0x088622CC (a0 = a pointer to the state pointer): 0x08B1A844 decides when a pass may run; a pass reads
// 0x088D4C9C(state->word + 0x5AC + 4) and walks the table at state->word + 0x3F50 — every entry of 0x188 bytes whose
// word +0x4C is 1 and whose word +0x10 is -1 with 0x0885D4CC(entry) non-zero stores the entry's word +0x1C into the
// table's word +0x10; an entry whose word +0x4C is 3 stops with 0. A pass that saw a non-1 entry waits 100000 through
// 0x08B2E2B8 and passes again, up to 900 times. v0 = 1 (nothing changed, or the table is empty) or 0 (the 3 entry,
// the retries ran out, or the early exits, which wait a few cycles first).

// 0x088DFDA0 (a0 = this, a1 = a flag kept as a byte): with the flag set, 0x088E5148(this + 0x640, this + 0x138,
// 0x088D9F80(this)) and 0x088E4B80 are called; then, when this + 0x27 is set, the object at this + 0x618 either gives
// 0x088DC858 a non-zero answer or is handed to 0x088E4C28 through the local record; the records at this + 0xC0 (0x2F8
// bytes apart, as many as 0x088D9F80(this) answers) with a word +0xC and a byte +0x10 are released one by one
// (0x088E6864, or 0x088E68F8 when one of the two is zero); 0x088E6A14 and 0x088E698C finish with the two counts
// 0x088D9F80 and 0x088DA160 answer, after 0x088E6B0C/0x088E6B84 chose by 0x088FE2A4(this + 0x14).

// The low half of `value * factor`, taken as a signed 32-bit word and divided by 100 (the original's magic 0x51EB851F).
u32 hundredth_of(u16 value, u16 factor) {
    const s32 low = std::bit_cast<s32>(static_cast<u32>(value) * static_cast<u32>(factor));
    return static_cast<u32>(low / 100);
}

// The pair 0x0892CF9C / 0x0892D1C0 (a1 = the sums, a2 = the source, no frame): the 16 bytes at the sums' +0xC0 each take
// one source byte added, the byte +0xBD takes another, and the halfwords +0x10A and +0x10C become the low half of
// (their value times a source halfword) / 100. v0 = the first quotient (before the store truncates it).
void add_source_to_sums(Runtime &rt, AllegrexContext &ctx, u32 first_byte, u32 second_byte, u32 first_half,
                        u32 second_half) {
    const u32 sums = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u8 delta = byte_at(rt, source + first_byte);
    for (u32 offset = 0xC0u; offset < 0xD0u; ++offset) {
        put_byte(rt, sums + offset, static_cast<u8>(byte_at(rt, sums + offset) + delta));
    }
    put_byte(rt, sums + 0xBDu, static_cast<u8>(byte_at(rt, sums + 0xBDu) + byte_at(rt, source + second_byte)));
    const u32 first = hundredth_of(half_at(rt, sums + 0x10Au), half_at(rt, source + first_half));
    const u32 second = hundredth_of(half_at(rt, sums + 0x10Cu), half_at(rt, source + second_half));
    put_half(rt, sums + 0x10Au, static_cast<u16>(first));
    put_half(rt, sums + 0x10Cu, static_cast<u16>(second));
    ctx.gpr[2] = first;
}

// 0x08C3D12C (a0 = a begin/end/capacity container, a1 = the insertion position, a2 = a pointer to the value): with the
// end word +4 short of the capacity word +8, the in-place path moves one word up (the end's word -4 into the end, the word
// past that into +4 of the container), then memmoves `(((end - 4) - position) & ~3)` bytes into `end - that` from the s1
// the caller left, and stores the value at the position. Otherwise the end grows to twice the element count (capped at
// 0x3FFFFFFF, after the 0x3FFFFFFF length check through 0x08B5ADDC), 0x08B62344 allocates the block, the head and tail are
// memmoved in, the value stored between them, the old block released (0x08B64654) and begin/end/capacity updated.
// v0 = the memmove's answer (in place) or the new capacity end.

// 0x08AB7260 (a0 = this, a1 = an output object): 0x08AB7004(&local, this) answers a counted block in the local; 0x08B36170
// is told the object at this + 0x14, the block and the smaller of the two words before them (its non-zero answer becomes the
// count), and the block is released; then the count selects 0x08CB428C (non-zero: 0x08AB6F7C(&local2) and
// 0x08A767EC(output, 0x08CB428C, local2, this + 0x14's object)) or 0x08CB42A0 (zero: the same with local2 and the object
// word from the entry), and the second block is released too.

// 0x08821ADC (a0 and a1 = two objects): 0x08A6C410(&local, object) then the object's virtual +0xC(&other, object) answer two
// blocks both times; 0x08B36170 is told the first block, the second and the smaller of their counts, its non-zero answer
// becomes the count, and the count's sign bit is the result. The second block, the second local, the first block and the
// first local are released in that order.

// 0x08830BFC (a0 = a destination object, a1 = passed on, a2 = a kind, a3 = a holder): 0x088297A8(&local, a1) answers an
// object, 0x088294D0(object) a value that is kept, and the local is released; kind >= 2 takes the third virtual (+0x3C) of
// the holder's word +4 as a count (0x08A5BB7C reports a missing object), kind 1 leaves the count zero, and kind <= 0 skips
// the holder's word check. A holder with a word +0 tests 0x08847008(&other, holder), 0x08846E50(other) and passes that
// word plus 0 to 0x08860500(value, word, 0, count != 0); without one it passes 0 and 1. Then 0x08A85B1C(&local, flag) runs
// with 1 when 0x08860500 answered non-zero, the destination is assigned the local (0x08A5BAD8) and both are released.

// 0x0885C3C8 (a0 = a pointer to the object pointer): 0x08B344E0(a0 + 8, a1) runs first, then the object's fields +4, +8,
// +0xC (= 1), the bytes +0x14..+0x19 (= 0), +0x10 and +0x28/+0x2C (= -1) are set, the objects at +0x7C, +0x80, +0x84 and
// +0x88 are released (0x088D5AFC, 0x0889406C, 0x08893D08, 0x088D43E4), the helper at +0x78 is released through its first
// virtual (with its word +0) and freed (0x08B64654), the four records of this->word (0x188 bytes apart) are reset
// (0x0885D5E0), the words +0x1C and +0x20 are set to -1, and 0x08B3457C(a0 + 8) finishes (a tail call with v0 = word(a0) + 4).

// 0x08860FF8 (a0 = this, a1 = an index, a2 = a value, a3 = an argument): 0x088A79AC(sp, 0) fills a record and this->word +
// index * 0x50 + 0x4120 is the record. Two of the three 0x30-byte locals at sp + 0x40 are built (0x08897B8C), then
// 0x08866510(record + 0x44, record + 0x48, &local, sp, sp + 0x40) answers the entry byte at sp + 0x12 that this keeps as
// its index when it is larger than that one (else the index becomes the byte and the byte its successor); the entry at
// sp + 0x40 + index * 0x40 gets 0x08897470(entry, argument) and, unless the record's byte +0x18 is set, the record the
// halfword +0x1E selects from this->word + 0x3F50 (0x188 bytes apart) gets 0x0885D868(record, index, argument); sp + 0x11
// becomes 1 (the byte +0x11) and 0x088661B8 runs with both counts and the two locals; then 0x088978B4(sp + 0x70) and
// 0x088A7548(sp) finish.

// 0x088D57A0 (a0 = a pointer to the destination pointer, a1 = the source): copies 0x60 bytes from a1 to the block word(a0)
// points at, 0x20 bytes from a1 + 0x60 to block + 0x60 (word by word; the original's lwl/lwr and swl/swr are its
// unaligned carries), then the two bytes a1 + 0x80 to block + 0x80; 0x088EF51C(block + 0x82) and 0x088D4800() finish.
// v0 = the source plus 0x88... 0x088D4800 answers 0x90, so v0 = a1 + 0x90, the end of what was copied.

// 0x08866B40 (a0 = this, a1 = a pointer to two words): 0x0890B3D8 and 0x0890B56C get the object at this + 0x98, which then
// gets 0x0890B3E0(object, word a1+4, 0, 1.0f); 0x08AF2640(this + 0x4524) and 0x08916C58(this + 0x45DC, word a1) run (its
// answer, 0 if none, is stored as (answer == 0 ? 0 : answer + 4) in the object's word +0x30); a 0x30-byte record built from
// the object's words +0x20, +8, +0x10, +0x18, +0x28, +0xE0, that count and this + 0x4524 is handed to
// 0x0888A0F4(this + 0x4584, &record, this + 0x45D4, word a1), and when the answer is non-zero also to
// 0x08917060(answer, &record, ...); a three-word record (this + 0x3F98, 0, this + 0x98) reaches the two virtuals at +0x30
// and +0x14 of this + 0x94.

// 0x089CA118 (a0 = a destination object, a1 = a key, a2 = a mode, a3 = a holder): 0x089B395C(&local, 0x08CF6ED8) runs
// first; a mode of 2 or less takes the 0x30 virtual of the holder's word +4 as the count (mode 1 leaves it at 1, 0 does
// nothing); 0x089C5850(&other, key) answers a source that (with the holder's word 0) is assigned through its +0xC virtual
// into a local, and 0x089C9F24(source, &local, &assigned, count) runs; then the assigned block, the destination (when it is
// not the first local) and the other local are released.

// One element of the 0xC-byte ranges 0x089DBCE8 destroys: `release(word, word + 4, marker)` and then 0x08B35254 for the
// element's first word when it is not null.
void destroy_element(Runtime &rt, AllegrexContext &ctx, u32 element, u32 marker, u32 release) {
    call(rt, ctx, release, {word(rt, element), word(rt, element + 4u), marker});
    const u32 held = word(rt, element);
    if (held != 0u) call(rt, ctx, 0x08B35254u, {held});
}

// 0x089DBCE8 and 0x089DBE58 (a0 = this, identical bodies): the vtable words 0x08CBC738 (at +0) and 0x08CBC970 (at +0x38)
// are stored, this + 0x13C and +0x138 are released (0x08A5B9EC), the counted block at this + 0x134 is freed, and three
// ranges of two 0xC-byte elements each — [this + 0x11C, this + 0x134) through 0x08C06684, [this + 0x104, this + 0x11C) and
// [this + 0xEC, this + 0x104) through 0x08BF0BF0 — are destroyed from their last element down; 0x0896BC0C(this) finishes.
void destroy_similar_object(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = self;  // s3
    put(rt, self, 0x08CBC738u);
    put(rt, self + 0x38u, 0x08CBC970u);
    call(rt, ctx, 0x08A5B9ECu, {self + 0x13Cu});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x138u});
    release_counted(rt, ctx, word(rt, self + 0x134u), frame.at(3u));
    for (u32 element = self + 0x128u;;) {
        destroy_element(rt, ctx, element, frame.at(2u), 0x08C06684u);
        if (element == self + 0x11Cu) break;
        element -= 0xCu;
    }
    for (u32 element = self + 0x110u;;) {
        destroy_element(rt, ctx, element, frame.at(1u), 0x08BF0BF0u);
        if (element == self + 0x104u) break;
        element -= 0xCu;
    }
    for (u32 element = self + 0xF8u;;) {
        destroy_element(rt, ctx, element, frame.at(0u), 0x08BF0BF0u);
        if (element == self + 0xECu) break;
        element -= 0xCu;
    }
    call(rt, ctx, 0x0896BC0Cu, {self});
}

// 0x08B2D61C (a0 = an argument kept in s1, a1 and a2 = two words stored in the module's state 0x08D68D00): clears the
// state block (0x08D68D00, 0x40 bytes, through 0x08B36018) ands its words +0x40, +0x44, +0x48, +0x4C, +0x50, +0x54, +0x58
// and 0x08D68CF8, copies the a0 through 0x08B7C830(state, a0, 4), opens a channel (0x08C7AA18) whose handle goes to +0x48
// (a negative one answers 1), allocates 0x14 bytes for +0x44 (0x08B62344, set up by 0x08B2EE10 and 0x08B2EEB8), starts it
// (0x08B2D4E0: a negative answer releases the block and the handle and returns 3) and registers 0x03415001 on the handle
// (0x08C7A820(a0 = 0x08CB542C, ..., &handle, 4, 0, 0): a negative answer releases both after 0x08B2D45C and returns 2).

// 0x08B48A74 (a0 = this, a1 = an argument of 0x08B0EC60, a2 = a count, a3 = a limit): with this's word +0 set the answer is
// 1; otherwise 0x08B0EC60(&record, a1, a2, a3) fills 0x3C bytes at sp + 0x80 that are copied to sp, the count becomes
// 0x08B35DA0(word sp+0x10) when a2 is zero (and this's byte +4 becomes 1) or the answer is 0 when the limit is smaller than
// that word; 0x08B100D0(&output, a1, &record-with-both-counts, 1) fills 0x3C bytes at sp + 0xC0 that are copied to sp + 0x40,
// a non-zero first word there answers 0, else 0x08B48A68(this, count) runs and the answer is 1.

// The free-list head 0x08BB2340 uses: the pool's word +0x34 while its word +0x10 is `size`, else 0x08BB1D0C(pool + 0x24,
// pool + 0x28, &size, 0x08BB1B40) answers a node whose word +0 is the new head (also stored back into pool + 0x34).
u32 pool_free_head(Runtime &rt, AllegrexContext &ctx, u32 pool, u32 size, u32 local) {
    put(rt, local, size);
    u32 head = word(rt, pool + 0x34u);
    if (head == 0u || word(rt, head + 0x10u) != size) {
        head = word(rt, call(rt, ctx, 0x08BB1D0Cu,
                             {word(rt, pool + 0x24u), word(rt, pool + 0x28u), local, 0x08BB1B40u}));
        put(rt, pool + 0x34u, head);
    }
    return head;
}
// The object goes to the head of the list at head + 0xC after the head's word +0x1C is decremented.
void link_into_pool(Runtime &rt, u32 head, u32 object) {
    const u32 next = word(rt, head + 0xCu);
    put(rt, head + 0x1Cu, word(rt, head + 0x1Cu) - 1u);
    put(rt, object, next);
    put(rt, head + 0xCu, object);
}

// 0x08BB2340 (a0 = begin, a1 = end, an array of object pointers): nothing when they are equal; for every non-null object
// the reference count at +0x10 is decremented and, when it reaches zero, the object's word +0xC goes into the first pool
// (0x08BB1F48, its head at +0x34 sized 4), the object's word +0xC is cleared and 0x08BB1FF0(object) runs, then the object
// itself is linked into the second pool (sized 0x14).

// 0x0899FD68 (a0 = this, a1 = an object): 0x08A36C3C(&local) answers an object that 0x08A34DA8 gets told is at
// 0x08CA4A14 (0x08A5BB7C reports when it is missing) and the local is released; when bit 4 of this + 0x13C is set, three
// float calls run (0x0899D06C with this + 0x150, then 0x0899D0D0 and 0x0899D108 with this + 0x120); bit 3 of the flags is
// cleared and its old bit 2 handed to 0x089AF5E8(a1, bit) and 0x089AB114(a1, 0); with this + 0x164 set, three locals are
// built (0x08A76AA0 twice, 0x08A76AAC with the a1), the second is assigned the third's word into a1 (0x08A5BAD8) and
// released, and the object's virtual +0xCC(second, 1, third) runs.

// 0x089D0BE0 (a0 = a destination object, a1 = passed on, a2 = a kind, a3 = a pointer to a pointer): 0x089CD62C(&local)
// runs first. Kind 0: the local's object, 0x089CD884(&local, object) and 0x08A6D2D4(&other, &local), then the other is
// assigned into a0 when it is not itself (0x08A5BAD8) and released; kind 1: a3's pointer's virtual +0xC(&local, pointer)
// and 0x089CD344(a2, &local). Both then release a counted block (`word + 8 - 0xC`): unless it is the shared empty block
// 0x08D74FD0 or 0x08B60804(block + 4, -1) answers a positive count, 0x08B8542C(block, &marker) frees it. v0 = the last
// release call's.

// The release 0x0887A28C ends with: the block at `pointer` (its base is pointer - 0xC) is skipped when it is the shared
// empty block 0x08D75298 or 0x08B60804(pointer - 4, -1) answers a positive count; otherwise 0x08B93554(base, marker) frees
// it.
void release_list_block(Runtime &rt, AllegrexContext &ctx, u32 pointer, u32 marker) {
    if (pointer - 0xCu == 0x08D75298u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B93554u, {pointer - 0xCu, marker});
}

// 0x0887A28C (a0 = this, a1 = passed on): 0x08AB85F0(&local, a1) then 0x08AB856C(&local) twice (the last answer is the
// count); with the local's word +0x10 positive, 0x08B62344(0x7C) answers a block that 0x0887FFBC gets with the count and
// this's word, 0x0886A9A0(block, 3) answers a record whose word +0xC becomes 0x24 and whose word +0x10 gets bits 0-1 set
// and bit 2 cleared, 0x0886C798(&described, this's word, count) and 0x0887F7C8(block, described) connect them,
// 0x0886ADEC(block, 1) and 0x0886B2E8(this's word, block) finish, and the described block is released; the local is
// released too.

// 0x088D0B0C (a0 = this, a1 and a2 = two arguments): 0x088FAC80(&result, a2, a1, this's float +0) answers a float at sp + 4
// that answers nothing when it is negative (else it is stored at this + 0); 0x088FB000(a2) and 0x088FB144(a2, angle,
// this + 4, this + 0) answer a count, and a count of 0 (or a positive one whose 0x088FB028(a2) - 20 is above this + 4)
// skips to the last check, while a negative one or one within that window adds the count to this + 0x18, asks
// 0x088FB17C for this + 0x28, and when this + 0x14 is below the new + 0x18 (and this + 0x27 is clear) makes +0x14 the new
// total and sets byte +0x24. The last check stores this + 0 as this + 0xC while this + 0 is above this + 0xC and
// 0x088FB100(a2, 0, this + 0, this + 4) answers a positive value with +0x14 equal to +0x18.

// 0x088D0DC4 (a0 = this, a1 = passed on, a2 = a source, t0 = an object, a3 = a holder whose byte +0 says whether to look
// at all): with the byte set, this + 0x1C (its bits read as a fixed-point word) times 0x088FB028(t0) plus this + 0x20 is
// compared with this + 0x18 (likewise) times 0x088FB028(t0) plus this + 0; while the second is not below the first, each
// of the four slots (0x088D6218(a2 + 4, index)'s answer read as a fixed-point word) that is not negative gets
// 0x088FB144(t0, value, this + 4, this + 0) and, when that answers positively and the slot's word +0x184 of this is
// 0x481907FF, 0x088FB17C(t0, ...) with this's byte +0x5A set and 0x088D0C80(this, a3), whose answer becomes that word.

// 0x088F9F00 (a0 = this): the object at this's word 0 holds two vectors of 4-byte elements: the first (words +0x10/+0x14)
// has each element's word released through 0x08B03A40(object + 0x1C, element) and the second (words +4/+8) each element's
// signed halfword through 0x08B03A04; 0x08BBB908(object + 0x10, begin, end) and 0x08BBB6F8(object + 4, begin, end) destroy
// the ranges, then both vectors' begin/end/capacity words are moved to the stack and cleared, and each old begin is freed
// (0x08B35254) after the range destructor ran.

// 0x089A54A0 (a0 = this, a1 = a key, a2 = an output): nothing when the output's first word is null or when
// 0x08BF63A8(this + 0x16C) is not this + 0x170; 0x08BF6308(this + 0x16C, key) answers the record: when a record came back,
// its block (word +0x10) and the key's are compared through 0x08B36170 (their counts' difference, the call's non-zero
// answer replacing it) and a non-negative difference skips the insertion — a negative one builds a local
// (0x08A5BAB8(&local, 0x08CF6ED8), 0x08B88F44(&other, key), 0x089FE2F0(&third, &local), 0x08BF76A8(this + 0x16C, record,
// &other)) whose other is released and whose third is released after its block. The record's word +0x14 is finally
// assigned the output's word unless that is the same address.

// 0x089AA1C0 (a0 = this, a1 = a row of 8-byte entries, a2 = a count, a3 = an argument, t0 = a holder, f12 = a scale):
// two short busy waits, then, when the holder's word 0 is set, 0x089E4BC4(word, &local, 0) and this's byte +0x118 becomes
// 1 while its word +0x11C takes the holder's word (the local at sp + 0x30 is the block's address, else 0); the virtual
// +0x150 of this runs, then for each of the a2 - 1 rows (8 bytes apart) two entries are built at sp + 0 and sp + 8
// (0x08B4AC54(entry, row + 8 * column, a3), except the first column of any row but the first, which is copied from the
// previous row's second entry) and 0x089A9E58(this, sp, block, scale) is called; the virtual +0x154 of this finishes.

// 0x08A816C4 (a0 = this, a1 = a holder): for every node of the circular list at this + 0x20 (until this + 0x18) a 4-word
// block from 0x08A5BC20(8, 4) is described at sp + 0xC, its two intrusive pointers (words +0 and +4) are assigned the
// node's words +0x10 and +0x14 (0x08A5BAD8), 0x08A76AA0(&local) and the holder's virtual +0xCC(object, &local, 2, block)
// run, the node is advanced through 0x08B5F100(node), the local is released and the described block handed back through
// 0x08B856B0(begin, end, sp) and freed (0x08A5BC54, as many bytes as the description holds).

// 0x08AC9B54 (a0 = this, a1 = a pointer to a 64-bit key): 0x08AC99A4(key) answers a record whose +0x28 is the answer when
// it is found; otherwise 0x08AC99F4(key) answers a new one, 0x08B62344(0x14) a block that 0x08AD5590 sets up and
// 0x08AD5524(block, key) fills: failing, the block is released through its virtual +4 and 0x08AC9240(record) runs with 0;
// succeeding, 0x08AD542C(block) and 0x08ADC988(block + 0x10, &local) answer an entry, the block is appended to the record's
// list at +0x30 (0x08C3D12C when it is full), 0x08AC963C(record, block + 0x10, &local, entry, 0, 0) runs and the record
// gets this at +0x28 and itself at +0x2C. v0 = record + 0x28, or 0.

// 0x088619A8 (a0 = this, a1 = a name kept aside, a2 = a name, a3 = an argument, t0 = a pointer that is optional): the
// objects at this's word + 0x3F88 and + 0x3F90 are looked up (0x08894098, 0x08893C40), a state at sp is opened
// (0x088D441C) and given t0 when it is set (0x088D44BC); 0x08B0979C() and 0x08B0980C(object) answer a manager whose
// 0x08B08A0C(manager, 0) count (stored at sp + 0x7C when positive) and byte (0x08B08A58, stored at sp + 0x80) are
// recorded, 0x088618E8(this's word, 0) runs and 0x088D4824(word + 0x5AC + 4, a1) when a1 is set; 0x088DFA68(word + 0x5AC, sp,
// 1) opens a session that, when non-null, marks this's byte +0x5A9 and sets the object at word +0x5B8's +0x30 (this), +0x2C,
// +0x24, +0x20 and +0x28 (0x08860B3C, 0x0885F550, 0x08861FD0, 0x08860C2C) before 0x0885E7DC(this) runs. The state is
// closed with 0x088D435C(sp) and v0 = the session.

// 0x08869514 (a0 = this): the state at this + 0x90 is reset (0x0890B56C, 0x0890B3D8) and, when 0x08869E84(this + 0x1B0,
// 0) answers non-zero, 0x08869EC0(this + 0x1B0) and 0x08869ECC(this + 0x1B0, 0) answer a count that 0x0890B3E0 takes with 1.0
// and that lands in the state's word +0x30; a non-zero count also reaches 0x0890B00C(count, state + 0x20, state + 8, count,
// state + 0xFC != 0). Then 0x08869ECC(this + 0x1B0, 0) answers what a 0x30-byte record at sp holds (this + 0x1A0, state's
// words +0x20, +8, +0x10, +0x18, +0x28, that answer, 0, 0, state + 0xE0, a zero byte, 0) that 0x089098B8 takes; the object at
// this + 0x1C0 gets its virtuals +0x30(&record) and +0x14(), and 0x08B344E0(this) / 0x08B3457C(this) bracket them, with a
// final +0x30(object, 0).

// 0x088B0AA0 (a0 = a string object, a1 = a source that may be null): the object's word 0 becomes the shared empty string
// 0x08D752A4 (and nothing else runs when the source is null); otherwise 0x088B04F4(&local, source) and
// 0x08B957D8(object, &local) build the string, and the local's block is released. Then, refreshing the object's word after
// every 0x08BAFF30(object) call (the word -4's sign says one is needed), a leading '/' skips the '~' check that would else
// run 0x08BAFD74(object, 0, 0x08CA0A24, 10); while the last character (0x08B99E48(object, length - 1, 0) shortens the
// string) is '/' or ' ' the object is trimmed, and any other character answers the object itself.

// 0x08AE1D6C (a0 = an argument the callback gets, a1 = a limit kept aside, a2 = an end position, a3 = a table, t0 = a
// count, t1 = a start position, t2 = a length, t3 = a flags word that may be null): when both a1 and a2 are zero they
// become -1; then while the position is below the length, the span is the rest (capped by a2 - start when a2 is set),
// every table entry (0xC bytes apart, its word +4) at or after t1 and before start + span is reported through
// 0x08AE0CD8(a0, index) while the table, the span and the count are all positive, and when a2 was -1 and the span reaches
// it the start is either the a1 limit (with bit 8 set in the t3 word) or the span's end. v0 = the final start.

// 0x08AE6268 (a0 = this, a1 = an argument): the node at this + 0x18 (when its word +8 is negative) walks its chain through
// word +4: for each node, this + 0x10's float +0xFC scaled by 0x08CB6130 is truncated and taken off the node's word +0x4C,
// the node's bytes +0x84..+0x89 take the state's bytes +0xE4..+0xE7 split into their high and low nibbles, 0x08D40974 is
// set to 1 while the node's virtual +0x28(this) runs, then 0x08AE8658(node's halfword +0x1A)'s bit 6 sets this's word +0xC
// bit 6, and when the node's byte +0x55 is set with bit 1 clear 0x08AE34A8(node) runs. v0 = the byte / that call's answer,
// or 0 at the end of a node whose byte is clear.

// 0x08B8D5D8 (a0 = first, a1 = last, a2 = a needle, a3 = the needle's end, t0 = a predicate(position, needle)): the first
// position in [first, last) where the needle matches under the predicate (it returns non-zero on a match). A one-character
// needle is a plain scan; a longer one scans for the first character and, when the rest fails, retries from the position
// after that match. v0 = the position found, or last.

// 0x08C4B5C8 (a0 = this, a1 = a 3-float vector, a2 = a 3x3 matrix): the matrix times the vector, row after row
// (`v0 * m[0] + v1 * m[1] + v2 * m[2]`, the first product added to zero), stored into this. The original has two copies
// because this may be the matrix itself: it reads every element first. v0 is the first result's word when this is the
// matrix (the original loads it back from the stack), untouched otherwise.

// 0x088726B8 (a0 = this, a1 = a position, a2 = three floats, f12 = an angle that is negated first): a 4x4 matrix at sp
// (a translation of the negated angle at +8, ones on its diagonal) is built with 0x08B4E648(matrix, a1) and a second at
// sp + 0x60 (ones on its diagonal, the three floats in its last column) with 0x08B4E9D4(matrix, &second); the three
// rotations 0x08B4F3B0(matrix, a2 + 4), 0x08B4F2E4(matrix, a2), 0x08B4F47C(matrix, a2 + 8) and
// 0x08B4A774(&first, matrix, &first) compose them, then the same rotations and 0x08B4A774(&second, matrix, &second) do it
// again; 0x08AF43B4(this + 0xC, a1), 0x08AF43D0(this + 0xC, &first) and 0x08AF43EC(this + 0xC, &second) finish.

// 0x088C896C (a0 = this, a1 = a 3-float point): the point moved 20 up is looked up through
// 0x088D2D10(this's word +4, &result, &point); found, the point becomes the result's first three floats and the three
// floats at result + 0x1C (0x4C..0x54) are normalized (0x08B4A720, then 0x08B4A5E8 with 1 / sqrt) and scaled by 0.04 into
// the local at sp + 0xC, else the local at sp is (0, 1, 0) normalized and scaled the same way; 0x08B4AA34(point, point,
// &local) applies it.

// 0x0890FC28 (a0 = a destination kept in s2, a1 = an object, a2 unused, a3 = a source object, f12 = a scale): the source's
// virtual +0xC(source, 0, 0) answers a node that, when its +8 virtual answers a positive count, is replaced by the answer
// of its +0xC virtual; 0x0890FAD0(a0, a1, node, scale) runs, then the node's +0x60 virtual decides 0x08AB8774(a1, 2 or 0);
// the node's +0x74 virtual answers another node whose +0x18 virtual (2.0) or, failing, whose +0x1C (20.0 when set, 1.5) or
// +0x20 (30.0 and 1.0 when clear, else 1.5) settle the scale; 0x08AB8970(a1, a0's byte +0x28, scale) runs and
// 0x08AB8768(a1, that scale) finishes (a tail call).

// 0x089B8F38 (a0 = this, a1 = a list node, a2 = a holder): 0x08A5BA28(node, node) runs and the node's word +0x48, when
// set, goes through 0x089B8BAC(node + 0x48, node); this's word +4 gets its virtual +0x18C(node, holder) while
// 0x08B344EC(0x08CF4C2C) holds a lock, the node is spliced between holder's neighbours (word +0x50 and +0x54: the two ends
// are rewired, this's words +0xC and +0x10 take the node when they are missing, and the node's +0x50/+0x54 take the
// holder's, which are cleared) with 0x089BA6CC(node, this) and 0x089BA6CC(holder, 0) on either side;
// 0x08B34584(0x08CF4C2C) releases the lock, 0x08A7DE18(this's word +4, 0x08A7BCC0(holder)) registers, 0x08A5B970(holder)
// and 0x08A7BDB0(node) run, a local description (0x08B85EB8(&local, 0x08A7BDB0(node), &marker)) is handed to
// word(0x08A7BDB0(node) + 0x108), the local's block is released and this's word +4 gets virtual +0x188(node).

// 0x08ABEF54 (a0 = this): the vtable word 0x08CD6390 is stored, 0x08ABF6C0(this + 0x144) and 0x08B28DC4(this + 0x11C) run,
// and the 16 entries this + 0x110 / +0x114 / +0x118 (0x10 bytes apart, downwards from the highest) whose +0x118 word is set
// are destroyed: the +0x110 word cleared, the array of 0x20-byte elements at +0x114 (their count in its word -4) each
// released through its first virtual from the last element down and its storage freed (0x08B61FC0), the +0x118 block too,
// and both words cleared. The vtable word 0x08CD63D8 is stored last.
void destroy_entry_array(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    put(rt, self, 0x08CD6390u);
    call(rt, ctx, 0x08ABF6C0u, {self + 0x144u});
    call(rt, ctx, 0x08B28DC4u, {self + 0x11Cu});
    for (u32 index = 0u; index != 16u; ++index) {
        const u32 third = self + 0x110u - index * 0x10u;
        const u32 second = self + 0x114u - index * 0x10u;
        const u32 first = self + 0x118u - index * 0x10u;
        if (word(rt, first) == 0u) continue;
        put(rt, third, 0u);
        const u32 elements = word(rt, second);
        if (elements != 0u) {
            const u32 end = elements + (word(rt, elements - 4u) << 5);
            if (elements != end) {
                u32 element = end - 0x20u;
                do {
                    const u32 destroyed = element;
                    call_indirect(rt, ctx, word(rt, word(rt, element)), {element});
                    element -= 0x20u;
                    (void)destroyed;
                } while (word(rt, second) != element + 0x20u);
            }
            call(rt, ctx, 0x08B61FC0u, {elements - 4u});
        }
        if (word(rt, first) != 0u) call(rt, ctx, 0x08B61FC0u, {word(rt, first)});
        put(rt, second, 0u);
        put(rt, first, 0u);
    }
    put(rt, self, 0x08CD63D8u);
}
// 0x08ABF410: the same body, then 0x08B64654(this) frees the object itself (a tail call).

// 0x08B65728 (a0 = this, a1 = an index, a2 = a flag kept as a byte): a table of 8-byte entries starts inline at this + 0x24
// (8 of them); an index below 8 answers its slot there, a larger one reallocates (0x08B6461C((index + 1) * 8), the old
// entries copied, the old block freed with 0x08B61FC0 unless it is the inline one) and answers slot `index` of the new
// block. The index 0x7FFFFFFF marks this's word +0x14 (bit 0) and, unless word +0x10 shares that bit (0x08B5AA30 asserts),
// clears the word +0x1C (or +0x20 when the flag is set) and answers its address.

// 0x08865FF4 (a0 = this): nothing when this's word +8 or its byte +0x18 is clear; with the byte +0x19 set,
// 0x0892BB10() answers a state whose words +0x20 and +0x28 are taken, and bit 7 of (the state's +0x20 except this's
// +0x1C) selects whether 0x088DE9CC(this's word +8) and 0x088D9CCC answer a positive flag (else the byte +0x24); without
// it the same two calls answer that flag and this's +0x1C/+0x20 are cleared. The flag then runs 0x088DA0BC(this's word +8)
// and, when that answers non-zero, 0x08865F7C(this, the other flag) or 0x088D7744(this's word +8, flag) and, when this's
// word +0x10 is set, 0x088936A4(word, &record of the flag and the 0x088DE9CC answer).

// 0x08891F04 (a0 = this): nothing when its word 0 is clear (v0 = 0); 0x088F8BAC(word + 0x94, 0, 0) and
// 0x0888F53C(this + 8, this's two words) answer a count, and 0x0888FBF8(this + 8, the six words +0x8C..+0x9C, count, -1
// when this's byte +0x10 is clear else 0) runs; a buffer of ((count * 4) + 0x1E) with the low four bits cleared is taken
// from the stack and filled by 0x088F8BAC(word + 0x94, buffer, count), then for every entry 0x08890C5C(this + 8, index),
// 0x08890B20(this + 8, index) and 0x08896040(entry, answer, answer) run; the object at word +0x94 gets its virtual +0x1C,
// this's byte +0xA0 becomes 1 and 0x088910B8(this + 0x14) runs. v0 = 1 (0 when there was nothing to do).

// 0x08A9C1C0 (a0 = a destination object, a1 = a source object, a2 and a3 = pointers to two objects, t0, t1 and t2 =
// three arguments): 0x08A76AA0(a0) runs first; with the object at (a2's object) + 0x30 holding a non-empty list (+0x1C to
// +0x20, 4-byte entries), the object at a3 is checked (0x08A7BDBC) and, when that answers 0, gets
// 0x08A7BE18(object, a1 + 0x34); 0x08A90BF4(object at a2, object at a3) runs, 0x08A62AB8(&local) answers a block that
// 0x08A64008(&record, block, a2, t0, t1, t2) takes, the destination is assigned the record (0x08A5BAD8) and both the record
// and the local are released. v0 = the destination.

// 0x08AF5164 (a0 = this, a1 = a flag kept as a byte): when this's byte +0x426 is set and the flag is clear, the objects at
// this + 0x1A8 and this + 0x27C get their virtual +0x14; when the byte is clear and the flag set, both get 0x08AFAD8C with
// (0x1E0, 0x110, 3, 0, 0), their virtual +0x18 runs, this's virtual +0x3C(0, 0) and 0x08AF5040(this, 0, 0, the 1440 x 272
// viewport: 0, 0, 1440, 272, 0, 0, 1, 1) run, then this + 0x27C's +0x18 and this + 0xD4's +0x3C(0, 0) with
// 0x08AF5040(this + 0xD4, ...) the same. 0x08B344E0(0x08D470C4) runs, this's byte +0x427 takes the flag and
// 0x08B3457C(0x08D470C4) finishes (a tail call).

// 0x08B0BB3C (a0 = this, a1 = passed on, a2 = an object): when this's byte +8 is set, 0x08B0C9AC(object, a1, &header,
// this + 0x10) with the header {1, this + 0xC, this + 0x10} runs, else 0x08B0C99C(object, a1, 0, 0); a non-zero word +4 of
// the object ends it with 0. Two 16-byte reads (0x08B0C650) then check the size and the byte-swapped word 0x33627067;
// failing, 0x08B0C778(object) runs and the answer is 0. Otherwise 0x08B2B4D8(0x10, the byte-swapped size word) answers a
// record that takes the first read's 4 words and the second read's 4 words (its word +0x1C is the size word), and
// 0x08B0C650(object, &record + 0x20, size - 0x20) reads the payload before 0x08B0B1F8(record, record) runs; this's word +4
// becomes the record, this's word 0 is cleared and the answer is 1.

// 0x08B4665C (a0 = this, a1 = a packet with a header and a payload at this + 0x8D, a2 = a flag kept as a byte):
// 0x08B3FA98(&state, packet) runs, then 0x08C7AE30(packet's first word, &this + 0x8D, &length, 0, 1, 0x2400) with the
// length 0x2400: the end answers nothing, 0x80410709, a negative answer, a set flag or a clear this's word +4 answer
// nothing. 0x08B3F7CC(&state, packet + 0x1A) runs; when the packet's word +0x34 is set the buffer's tail is moved into
// place (0x08B79BAC) and cleared (0x08B79A88) and the length grows by it. Then for every entry of the buffer (its first
// unsigned halfword plus 0x1C, masked to 16 bits) 0x08B79A88 fills the packet's word +0x40 and this's virtual +0xB0(this,
// &state, entry, size) runs; an entry longer than what is left stops it, 0x08B79A88(packet + 0x40, entry, left) filling the
// packet's word +0x34 unless that is already set.

// 0x08C58700 (a0 = this): 0x08B5C450(&state, this + word(this's first word - 0xC) + 0x6C), 0x08B64C80(&state) and
// 0x08B5D34C(&state) answer a table whose word +0x18 holds character classes; the reader at this + word(-0xC) + 0x78 gives a
// character (its byte +8 while below its byte +0xC, else its virtual +0x24). A 0xFF marks the end: this's +0x14 word gets
// bit 1 through 0x08C54CFC and the answer is this; otherwise the character's class byte (signed) with bit 3 clear answers
// this too, and while it is set the next character is taken the same way (its position advanced) until a 0xFF or a class
// without bit 3.

// 0x088ACBA0 (a0 = this, a1 = an argument of 0x08B4AD90, a2 = a vector, a3 = a vector + 8): 0x08B4AD90(&a, a2,
// a3 + this's word +0x3D0 * 0x28 + 0x284) and 0x08B4AC30(&a, &a, this + 8) run, then 0x08B4AC0C(&b, &b, &a) and
// 0x08B4AD90(&c, a1, this + this's word +0x3CC * 0x28 + 0x294) with 0x08B4ABCC(&c, &b) answering a float that takes this's
// word +0x3CC element's +0x29C added; a value not above zero ends it, else 0x08B4A560(&d, (value * -0.5)) runs with
// 0x08B4AC0C(a3 + 8, a3 + 8, &d) and 0x08B4AC30(this + 8, this + 8, &d).

// 0x088E5E60 (a0 = this, a1 and a2 = two words, a3 = a pointer to five words that may be null): nothing when this's first
// word's +0x534 word is 1 (v0 = 0); 0x08B344E0(word + 0xEC) locks, the object's virtual +0x50(word + 0x548) and
// +0x50(-1) answer two records that must agree on their words +0x28 and +0x2C (else the second is dropped), the lock is
// released, and a zero record answers 0. Otherwise the +0xC4 locks (this's word 0's and, when set, its word +4's) are taken,
// 0x088ED374(this's word, 1) answers a record {a1, a2, byte +8 = 1, byte +9 = 0} that also takes a3's five words with its
// byte +9 set to 1, the locks are released and v0 = 1 (or 0 when the record is missing).

// The 0x40-byte identity 0x0891B64C writes at sp + 0x80: ones at +0, +0x14, +0x28, +0x3C and zeros at the other slots below
// +0x30 (the translation column is the caller's).
void put_identity_matrix(Runtime &rt, u32 address) {
    put_float(rt, address, 1.0f);
    put_float(rt, address + 0x14u, 1.0f);
    put_float(rt, address + 0x28u, 1.0f);
    put_float(rt, address + 0x3Cu, 1.0f);
    put(rt, address + 0x10u, 0u);
    put(rt, address + 0x20u, 0u);
    put(rt, address + 4u, 0u);
    put(rt, address + 0x24u, 0u);
    put(rt, address + 8u, 0u);
    put(rt, address + 0x18u, 0u);
    put(rt, address + 0xCu, 0u);
    put(rt, address + 0x1Cu, 0u);
    put(rt, address + 0x2Cu, 0u);
}

// 0x0891B64C (a0 = a 3-float output, a1 = an object whose word +8's +0x61C is a matrix, a2 = three floats): 0x088D390C(matrix,
// sp, sp + 0xC, 6, a2 + 4) fills three floats and three angles; 0x08B4E648(&matrix) starts it, the three floats go into its
// translation column and an identity with the three angles (0x08B4E9D4, 0x08B4F3B0, 0x08B4F2E4, 0x08B4F47C) composes it; a
// second identity with a2's first float as its translation goes through 0x08B4E9D4 again and 0x08B4A774(&result, &matrix,
// &origin) answers the output.

// 0x08A75738 (a0 = this, a1 = a holder whose word 0's word +4 is an object, a2 = a name, a3 = an argument):
// 0x08A98220(this, holder, name, argument) runs and this's vtable word becomes 0x08CD3AE8; 0x08A7F8E8(&local, name, 0,
// 0x08CB0D94) then 0x08B88F44(&second, &local) and 0x08BEED1C(&second, 0x08CB0D94, 2) run, the same for the object (with its
// 0x08B35F60 size) into a third local that 0x08A69654(this + 0x14, &third) takes, the three locals' blocks are released
// (markers sp + 2, sp + 1, sp) and 0x08A76ABC(this + 0x18, argument) finishes.

// 0x08A9C340 (a0 = this): nothing when this's byte +0x7C is already set; else it is set, 0x08A920E0(0) answers a list whose
// nodes (through their word +0x10) get their +4 virtuals and then their +8 virtuals called, 0x08AA0AA4(&local) answers an
// object that replaces this + 0x34 when it is not already that address, 0x08A5BC20(0x38, 4) answers an object that
// 0x08A7E914 initializes with 0x08CF6F0C and this + 0x38 takes (0x08A76AB4(&second, object) before the assignment), then
// 0x08A7BE18(this + 0x38, this + 0x34) and 0x08A7BFD0(this + 0x34, this + 0x3C, 0x08CF6F38) /
// 0x08A7BFD0(this + 0x34, this + 0x40, 0x08CF6FEC) run (0x08A5BB7C reporting a missing object at 0x08CB2570).

// 0x0885FAA4 (a0 = this, a1 = an index, a2 = an argument): the record this + index * 0x50 + 0x4120 is taken,
// 0x088A79AC(sp, 0) fills a record and two of the three 0x30-byte locals at sp + 0x40 are built (0x08897B8C);
// 0x08866510(record + 0x44, record + 0x48, &local, sp, sp + 0x40) answers the byte at sp + 0x11 that, when set, runs
// 0x0885F938(this, index, sp + 0x40, 0x088DCD28(this + 0x5AC, index), (a2 == -1 ? record + 0x24 : -1), record + 0x1B,
// record + 0x18); when a2 is -1 the object 0x088F8CC0(this + 0x5B4, index, record + 0x24) gets its +0x10(0) and +0x1C(0)
// virtuals and its byte +0xB cleared; the record's byte +0x19 becomes 1, 0x088F8A84(this + 0x5B4, index, record + 0x24)
// runs and the other local is released. v0 = 1 when the byte was set, else 0.

// 0x08872940 (a0 = a 3-float output, a1 = this): 0x0886B920(this's word +8, 0x08C9F8A0, 0) answers an object and this's
// word +0x5C a record whose word +0x10 selects the output: 2 takes this + 0x2C's three floats, 3 the same after
// 0x08B4AA68(output, output, this + 0x20) and a normalize (0x08B4A720, 0x08B4A5E8 with the record's word +0x14 over the
// length), the others (and a missing object for a selector below 2) leave it zero, and a selector below 2 with an object
// takes its +0x28 virtual(object, sp, 0x08C9F8A4) answer's three floats at sp + 0x30. 0x08B4AA34(output, output,
// record + 0x18) applies the record's matrix last. v0 = the output.

// 0x088BE9C4 (a0 = this, a1 = a float output that may be null): 0x088D5910(this + 0x10 + 4) then
// 0x088DF13C(this + 0xC, this + 0x10) answer two counts; nothing (0) when this's word +0x18's byte +0x98 is clear, the
// first count is not positive, the second is not positive or the first is below the second. Otherwise the object at
// this + 0x1C's virtual +0x14 answers a unit, the counts weight it (0x088DF13C's first float added), the ratio over the
// first count (0 when not positive) goes through 0x089468C0(this's word +0x14 + 0x27C), 0x088DA1C8's answer + 1 scales it
// by this's word +0x14's +0x288 when it reaches 2, and this's word +0x18's byte +0x99 (when positive) scales it by
// 0x089468C0(word + 0x14 + 0x2B0, (byte - 1) / 100); the output takes it and v0 = 1.

// 0x088CB668 (a0 = this, a1 = a pointer to an object, a2 = an object that may be null, a3 = a 3-float output, f12 = a
// scale): the object's virtual +0x1C(this + 0x2C) answers another whose +0x10 virtual fills sp; this's word +0x30 and
// 0x63C word make a record at sp + 0x48 (3, the 2 when the word is 4, the word, 0, the scale, 0) that
// word(this + 0x63C)'s +0x1C then the new object's +8 virtual take; 0x089545BC(sp + 0x10) runs with the record's floats
// (its +0x70, +0x74, +0x78), the output gets the first, the third negated and, when present, the second after
// 0x089545BC moved it (else the +0x1C float); a2's virtual +0x18 runs (with the record 0x40/0x44 = this's +0x5F0/+0x5F4
// and the byte 0x3C = 1).

// 0x088E6E14 (a0 = this, a1 = a value): 0x088E6D90(this, -1) answers a 64-bit pair that 0x088EF438(this's word 0's word
// +0xAC, 0x50, pair) takes; then 0x088EF430(command, 3, 0x08B09974(0x08B0979C())), 0x088EF438(command, 0x18,
// 0x08B08A88, 0), 0x088EF438(command, 5, 0x08B08A70, high), 0x088EF438(command, 6, this's +0x550, high),
// 0x088EF438(command, 4, (this's +0x548 == -1 or == +0x54C) ? 1 : 0, 0), 0x088EF438(command, 1, this's +0x548, high),
// 0x088EF430(command, 0x2E, 0x08B08AA0), 0x088EF438(command, 2, this's +0x54C, high) and 0x088EF438(command, 0x17, a1,
// high) run, and 0x088EF440(command) finishes (a tail call).

// 0x089AFC78 (a0 = this, a1 = an object): 0x08B344E0(0x08CF4A04) locks, 0x08A5BA28(object) runs, object's word +0x138 gets
// bit 9 set, 0x089AFA10(this, this + 0x30, this + 0x18) runs and a node record at sp (0x08CCFD88, the local twice) answers
// 0x089B9E60's key that 0x08BF9104(&tree, &key) looks up: a different node with a bigger key (0x10) gets a new node through
// 0x08BF99BC(&tree, {key, 0}). A 0xC-byte block (0x08CCEA88, 0x08BF8A7C, 0) goes to the node's word +0x14,
// 0x089A931C(this, 0x08CCEA78) and object's virtual +0x164(this) run, then 0x089A9314(this, 0x08CCEA78), 0x08A5B970(object),
// 0x08A5BC6C() and 0x089FD82C(0x08CCEA78) and 0x08B3457C(0x08CF4A04) finish.

// 0x08A243FC (a0 = a destination object, a1 = a count, a2 = two objects): nothing when the count (signed) is 2 or more;
// 0x08A6C410(&first, a2) and 0x08A6C410(&second, a2 + 4) then 0x08ABB9E8(0) answer a record that the first's +0xC virtual
// fills a local with, the second's +0xC virtual a second local, 0x08ABBAE0(record, &first-local, &second-local) answers
// another whose 0x08A97A6C(&result, record) the destination is assigned from (0x08A5BAD8), and the two blocks, the result
// and both locals are released.

// 0x08A52D3C (a0 = a destination object, a1 = a word kept in a local, a2 = a size that may be negative, a3 = an argument,
// t0 = another argument): 0x08A52AA8(&first, &word) runs; with a non-negative size, 0x08A5BC20(0x44, 4) answers a block that
// 0x08A5B70C(block, &first, size) fills and 0x08A52AB0(&second, block) adopts (the first is assigned the second then
// released). The record at &first gets its +0x15C virtual(0) and its +0x120 virtual, then 0x08A5BC20(0x18, 4) answers a
// block for 0x08A56018(block, 0, block, a3, t0) that 0x08A5BAB8(&third, block) adopts,
// 0x08A57B54(destination, &first, &third, 0) runs and both locals are released. v0 = the destination.

// 0x08AAAB6C (a0 = this, a1 = a key): this's word +4 (0x08A5BB7C reporting 0x08CB30B0 when missing) is the holder;
// 0x08B85EB8(&local, key, &marker) then 0x08C31D6C(this + 8, &local) answer a node: when it differs from this + 0xC, its
// block (word +0x10) and the key's are compared through 0x08B36170 (the counts' difference, the call's non-zero answer
// replacing it) and a negative difference makes a new node: 0x08B88F44(&other, &local), then
// 0x08C335FC(this + 8, node, &other) (its +0x14 word cleared first) and the other's block released. Finally
// 0x08A71B00(holder, word(word(node + 0x14) + 4)) runs and the local's block is released.

// 0x0884EFAC (a0 = a destination object, a1 = passed on, a2 = a count, a3 = an object): 0x0884D660(&local, a1) answers an
// object (0x08A5BB7C reports 0x08C9D198 when missing) that 0x0884D3D8 answers a record for, the local being released; with a
// positive count the object's +0xC virtual fills a local whose 8 bytes are copied (0x08B7C830) to
// 0x08896FB0(record) + 0x80 and released; otherwise 0x08B85EB8(&second, 0x08896FB0(record) + 0x80, &marker) and
// 0x08A6D2D4(&third, &second) build a value the destination takes (0x08A5BAD8), and both locals are released.

} // namespace
} // namespace new_batch_22
} // namespace gt::game

#define NEW22(address, name, kind) GT_DECOMP(address, gt::game::new_batch_22::name, gt::ReturnKind::kind)
