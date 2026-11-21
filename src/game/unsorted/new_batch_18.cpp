// New batch 18: the "property" families at the head of the batch and the standalone functions after them, written by
// hand from `tools/disasm`. Every step is the original's, in the original's order: a delay slot runs before its
// branch / call (a branch-likely delay slot only when the branch is taken), `lui [HI]` immediates are the relocated
// high half of the printed `[LO]` address, callee-saved registers are set where the asm sets them and put back at the
// end, and v0 / f0 end as the asm leaves them on every exit path. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_18 {
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

// A guest call with the argument registers written to ctx exactly as the original writes them before its `jal`
// (a0-a3, t0-t3 in order); a register the call does not name keeps what an earlier call left there. Afterwards v0
// holds the result, as after the original's `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee whose v1 the original reads after it: v0 and v1 come back as the call leaves them.
GuestResult call_set_both(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result;
}

u8 byte_at(Runtime &rt, u32 address) { return static_cast<u8>(rt.memory().load8(address)); }
s8 signed_byte_at(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
u16 half_at(Runtime &rt, u32 address) { return static_cast<u16>(rt.memory().load16(address)); }
s16 signed_half_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }

// MIPS `ins rd, rs, pos, size`: `size` bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}
// MIPS `ext rd, rs, pos, size`.
u32 extract_bits(u32 value, unsigned pos, unsigned size) { return (value >> pos) & ((size >= 32u) ? 0xFFFFFFFFu : ((1u << size) - 1u)); }

// s0-s7 and fp are callee-saved: the body sets them where the original sets them (a callee may read them) and the
// caller's values come back at the end.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};

// f20-f22 are callee-saved too.
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx) : ctx_(ctx), f20_(ctx.fpr[20]), f21_(ctx.fpr[21]), f22_(ctx.fpr[22]) {}
    ~SavedFloats() {
        ctx_.fpr[20] = f20_;
        ctx_.fpr[21] = f21_;
        ctx_.fpr[22] = f22_;
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float f20_;
    float f21_;
    float f22_;
};

// --- the convert-and-assign family (0x0884A07C .. 0x088565D8, 11 members) --------------------------------------
// a0 = the output object, a2 > 0 selects "set": the value is converted and handed to `set`, together with the answer
// of the object's virtual at +0x3C (its truth when the member takes a flag); a2 <= 0 selects "get": the converted
// value is turned into a string at sp+0, the caller's object is assigned from it and the temporary released.
// (ctor, convert, message, second message, set, get, flag).
void convert_and_assign(Runtime &rt, AllegrexContext &ctx, u32 ctor, u32 convert, u32 message, u32 second_message,
                        u32 set, u32 get, bool flag) {
    const u32 output = ctx.gpr[4];
    const s32 mode = static_cast<s32>(ctx.gpr[6]);
    const u32 other = ctx.gpr[7];
    u32 passed = 0u;
    u32 answer = 0u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x30u);
        ctx.gpr[17] = output;  // s1 = a0
        ctx.gpr[18] = other;   // s2 = a3
        if (mode > 0) {
            const u32 temporary = frame.at(8u);  // s3 = sp + 8
            ctx.gpr[19] = temporary;
            call_set(rt, ctx, ctor, {temporary, ctx.gpr[5]});     // jal ctor; addu a0, s3 (a1 is still the caller's)
            const u32 object = word(rt, temporary);               // lw s0, 8(sp)
            ctx.gpr[16] = object;
            if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
            const u32 converted = call_set(rt, ctx, convert, {object});
            ctx.gpr[17] = converted;                              // addu s1, v0 (the destructor's delay slot)
            call_set(rt, ctx, 0x08A5B9ECu, {temporary});
            const u32 target = word(rt, other);                   // lw s0, 0(s2)
            ctx.gpr[16] = target;
            if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {second_message});
            const u32 function = word(rt, word(rt, target) + 0x3Cu);
            const u32 result = call_indirect(rt, ctx, function, {target});
            passed = converted;
            answer = flag ? (result != 0u ? 1u : 0u) : result;
        } else {
            const u32 temporary = frame.at(4u);  // s2 = sp + 4
            ctx.gpr[18] = temporary;
            call_set(rt, ctx, ctor, {temporary, ctx.gpr[5]});  // a1 is still the caller's
            const u32 object = word(rt, temporary);
            ctx.gpr[16] = object;
            if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
            const u32 converted = call_set(rt, ctx, convert, {object});
            ctx.gpr[16] = converted;                              // addu s0, v0
            call_set(rt, ctx, 0x08A5B9ECu, {temporary});
            const u32 text = call_set(rt, ctx, get, {converted});
            call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), text});
            if (output != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(0u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
    }
    if (mode > 0) tail_call(ctx, set, {passed, answer});
}


// --- the three-halfword setter family (0x0884AD3C, 0x0884AE4C, 0x0884AF5C) -------------------------------------
// The converted value is split by 0x088D5D54 into three halfwords at sp+8, sp+0xA, sp+0xC. For a2 > 0 the object's
// virtual at +0x3C answers one of them (`slot`), the answer is stored there and all three go to 0x088D5AC8 with the
// value; for a2 <= 0 the halfword at `slot` becomes a string, assigned to a0.
void set_three_halves(Runtime &rt, AllegrexContext &ctx, unsigned slot) {
    const u32 output = ctx.gpr[4];
    const s32 mode = static_cast<s32>(ctx.gpr[6]);
    const u32 other = ctx.gpr[7];
    u32 halves[3]{};
    u32 passed = 0u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x30u);
        const u32 temporary = frame.at(4u);  // s3 = sp + 4
        ctx.gpr[19] = temporary;
        ctx.gpr[20] = other;   // s4 = a3
        ctx.gpr[21] = output;  // s5 = a0
        ctx.gpr[18] = static_cast<u32>(mode);  // s2 = a2
        call_set(rt, ctx, 0x08847008u, {temporary, ctx.gpr[5]});  // a1 is still the caller's
        const u32 object = word(rt, temporary);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CBBCu});
        const u32 converted = call_set(rt, ctx, 0x08846E50u, {object});
        ctx.gpr[17] = converted;  // s1
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        const u32 first = frame.at(8u);
        call_set(rt, ctx, 0x088D5D54u, {converted, first, frame.at(0x0Au), frame.at(0x0Cu)});
        halves[0] = half_at(rt, first);
        halves[1] = half_at(rt, frame.at(0x0Au));
        halves[2] = half_at(rt, frame.at(0x0Cu));
        if (mode > 0) {
            const u32 target = word(rt, other);
            ctx.gpr[16] = target;
            if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9CB80u});
            const u32 function = word(rt, word(rt, target) + 0x3Cu);
            const u32 answer = call_indirect(rt, ctx, function, {target}) & 0xFFFFu;
            put16(rt, first + slot * 2u, answer);  // the store runs before the call below (delay slot)
            halves[slot] = answer;
            passed = converted;
        } else {
            call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), halves[slot]});
            if (output != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(0u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
    }
    if (mode > 0) tail_call(ctx, 0x088D5AC8u, {passed, halves[0], halves[1], halves[2]});
}


// A guest call whose f0 the original reads afterwards (or that takes float arguments): the argument registers are
// written like call_set and v0 / v1 / f0 come back as the call leaves them.
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 19u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A shared block's release: the data pointer sits at `slot`, its control block 0xC bytes below it, and unless that is
// the shared empty holder (0x08D74FD0) the count at data - 4 decides whether 0x08B8542C frees it (its second argument
// is a stack slot the caller passes, `sink`).
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 sink) {
    const u32 data = word(rt, slot);
    const u32 holder = data - 0xCu;
    if (holder == 0x08D74FD0u) return;
    const u32 references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(references) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {holder, sink});
}

// 0x0885CF00 (a0 = a holder of one object): walks the records of the object at word(a0) (count at +4, records at
// word(owner), stride 0x188) and, for a record whose bytes +0x16/+0x17 say "pending" and whose byte +4 is zero, builds a
// name (0x08B37640(sp, 0x08C9F57C, index)) and hands it to 0x0885D480; when the lookup 0x08862150(word(a0 + 4), index)
// answers nothing, 0x0885D5E0 finishes the record. Then the holder is rebuilt: the answer of 0x088622CC is kept and the
// object's word +0x10 goes to 0x0885E6E0 with a zero. v0 = the answer of 0x088622CC.

// 0x08872E18 (a0 = this, a1 = a drawable): clears bit 0 of word(word(this + 0x5C) + 0x100), takes a scale from a1's
// virtual +0x1C, multiplies it by 0x0886B0F8(word(this + 8)), wires the drawable in (0x088728C0(word(this + 0x5C),
// this + 0xC) and 0x08872054(word(this + 0x5C), 60 * scale)), and unless 0x0886B444(word(this + 8), 3) or
// 0x0886A9A0(word(this + 4)) say otherwise, sets bit 0 again around 0x0886F8CC / 0x0886F8E4 / 0x08872AC8. Ends with
// 0x0890AEBC(word(this + 0x5C) + 8) and f12 = the scale.

// 0x0887B0DC (a0 = this): releases the helper at +4 through its virtual +0x18 and then +4 (an empty slot only stores
// the zero), then the object at +0 through its virtual +0x6C and +4 the same way. When bit 30 of the word +0x24 is
// clear v0 = 0; otherwise the `count` (word +0xC, signed) entries at this + 0x2C (stride 0x38) are released with
// 0x0887B060, the count and the words at +0x10 / +0x14 are cleared, bit 30 is cleared and v0 = that cleared word.

// 0x088E57CC (a0 = this): twice, the child object at word(this + 4) - when it has its byte +0x521 set - gets word +0x50C
// cleared and, if its word +0x508 is 1, waits (0x08B2E2B8(0x1F4) while it stays 1) before its virtual +0xC runs; then
// 0x088EEEB0(word(child + 0x118)) feeds 0x088ECB14(child, answer), 0x088ED6F4(child, 0), 0x08B08D54(word(child + 0xA8))
// with word +0x530 and byte +0x525 cleared, 0x08B08D84 and 0x08B09040 on word(child + 0xA8), and byte +0x521 is
// cleared. Ends with 0x08B09918(0x08B0979C()).

// 0x0892052C (a0 = this): on the first use the block 0x08CF2890 gets the float 0x08CF2898 = 1.0473... when
// 0x08B6441C(0x08CF2890) says so (closed by 0x08B64494). Then the object 0x089260EC(word(this + 8)) is asked twice for
// its virtual +8 (a1 = 8, then a1 = 0x10): each nonzero answer sets f22 to 0x08CF2898 (the second one negated). Its
// virtual +0x10 gives f21 = f0 (f20 = -0x08CF2898), its virtual +0x14 gives f15. Ends with 0x089202A0 and
// f12 = f21, f13 = f20, f14 = f22, f15 = that answer.

// 0x089377C4 (a0 = a builder, a1 = a name or 0): the name (0x08CA21A8 when a1 is zero) and 0x08CA221C are appended to
// the builder's text (this + 0xC) and the size word +4 starts at zero, then 0x0893769C(this, index) is summed over 0x2C
// indexes. If the byte at 0x08CDDEF0 is not ',' the answer of 0x08937574(this) is added once per byte up to and including
// the first ',' (from 0x08CDDEF1); then those of 0x08937460, 0x08937098 and 0x089371B0 are added. v0 = the accumulated
// size.

// 0x08969770 (a0 = a stack object, a1 = passed on, a2 = a kind, a3 = a source): 0x08969244(sp, a1, a2, a3) fills the
// stack word at sp. Kind 2: the objects at sp+4 and sp+8 are built (0x089AB370, 0x08A0FD80) and the object at sp gets
// both as the virtual +0x1C4's two arguments; kind 1: only sp+8 is built and the virtual gets (object, 0, word(sp+8));
// any other kind does nothing. Every temporary is released; a zero object panics with 0x08CA2880.

// --- the "flag or factor" family (0x08981120, 0x0898BF40, 0x0898EF54) ------------------------------------------
// a0 = the output object, a2 > 0 selects "set": the object at word(a3) answers its virtual +0x3C, and the object found
// at sp gets 1.0f in its float `field` when that answer is zero (0.0f otherwise); a2 <= 0 selects "get": the found
// object's `field` becomes the flag (field <= 0.0f) assigned to a0. (finder, message, second message, field).
void flag_or_factor(Runtime &rt, AllegrexContext &ctx, u32 finder, u32 message, u32 second_message, u32 field) {
    const u32 output = ctx.gpr[4];
    const s32 mode = static_cast<s32>(ctx.gpr[6]);
    const u32 source = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = output;        // s1
    ctx.gpr[16] = source;        // s0
    ctx.gpr[18] = frame.at(4u);  // s2
    if (mode <= 0) {
        call_set(rt, ctx, finder, {frame.at(4u)});
        const u32 found = word(rt, frame.at(4u));
        ctx.gpr[16] = found;
        if (found == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        const float value = float_at(rt, found + field);
        const u32 temporary = frame.at(8u);
        ctx.gpr[16] = temporary;  // s0 = sp + 8
        call_set(rt, ctx, 0x08A85B1Cu, {temporary, value <= 0.0f ? 1u : 0u});
        if (output != temporary) call_set(rt, ctx, 0x08A5BAD8u, {output, word(rt, temporary)});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    } else {
        call_set(rt, ctx, finder, {frame.at(0u)});
        const u32 found = word(rt, frame.at(0u));
        ctx.gpr[17] = found;  // s1 (the original overwrites the output with it)
        if (found == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        const u32 target = word(rt, source);
        ctx.gpr[16] = target;
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {second_message});
        const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, target) + 0x3Cu), {target});
        put_float(rt, found + field, answer == 0u ? 1.0f : 0.0f);
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    }
}

// 0x08A43288 (a0 = this, a1 = passed on, a2 = passed on): 0x08A431F8(sp + 4) answers an object whose word +0x44 takes
// 0x08B855E0(object + 0x44, a2); 0x08B88F44(sp + 0x10, a1) and 0x08A40C24(sp + 0x14, sp + 4) build two temporaries, and
// the object 0x08C1C1A8(this + 0x2C, sp + 0x10) answers a word (sp + 8 and sp + 0x20) and a byte (sp + 0xC). Every
// temporary is released (sp + 0x14 through 0x08A5B9EC, sp + 0x10 as a shared block, sp + 4 through 0x08A5B9EC).

// 0x08AA97D4 (a0 = this): every node of the intrusive list at this + 0x14 (the end sentinel is this + 0xC, the next node
// comes from 0x08B5F100) has its object at node + 0x14 dispatched through the vtable's second function, and the node
// released. Then both lists (this + 8 ... 0x18 and this + 0x20 ... 0x34) are reset to empty, and the block at
// this + 0x38 is handed to 0x08C2FBF8(sp) with its buffer (sp + 4, sp + 0xC) freed through 0x08A5BC54 at element size 4.

// 0x08AC3A98 (a0 = this, a1 = an output string, a2 = a length limit): when word(this + 0xC98) is negative and
// 0x08AC391C() answers nothing, nothing happens. Otherwise, under the lock at this + 0xB68, 0x08AC39B0(this + 0x9DC,
// word(this + 0xC98), sp, 0x100) reads a name into the stack; a name of 5 bytes or more whose last 4 bytes are
// 0x08CB46FC and whose length plus the length of the text at this + 0xC12 stays below a2 is appended to a1
// (0x08B7BB6C, then the stack text). Unlocks and returns 1 when it appended, 0 otherwise.

// 0x08AEB3BC: the game-state block 0x08D41AD0, when its first word is set: 0x08AE94DC(), its word +0x8C (signed) entries
// at +0x3C (stride 0x28) released with 0x08AEBCB8 and word +0x90 cleared; then, depending on the words +0x468 and +0x498,
// the matching teardown (0x08AECD60 / 0x08AEC1A4) runs before 0x08AEFF5C, 0x08AEE490 and 0x08B82FFC, the handle at +0xC
// is closed (0x08C7A9E0) and the whole block zeroed with 0x08B36018(base, 0, 0x568).

// 0x08AEE490: with the block 0x08D41AD0's word +0x4DC set, its handle +0x4EC (through 0x08C7AA08 / 0x08C7A9D8 /
// 0x08C7AAA0, with word +0x4E8 cleared), then +0x4FC, +0x4F8, +0x4F4 and +0x4F0 (0x08C7A9C8 / 0x08C7A9E0) are closed
// when they are not negative. The last one, +0x500, is handed to 0x08AD85C8 when nonzero. Finally the 8 words at +0x514
// are cleared. v0 = the word +0x4DC the early exit tests.

// 0x08BA9388 (a0 = first slot, a1 = end, a2 = source): assigns the source's 8-byte shared slot (a raw word, then the
// counted pointer at +4, whose count at +4 rises) to every slot of [a0, a1); each old pointer's count at +4 falls, and
// when it reaches zero the object's virtual +8 runs and its count at +8 falls the same way (virtual +0xC at zero). The
// counts are the PSP's ll/sc pair, a plain add in a single-threaded run.

// 0x08BAD7F0 (a0 = this): the two vtable words 0x08CC2460 (at +0) and 0x08CC2590 / 0x08CC26B4 (at +0x3698), then the 4
// objects at this + 0x375C (stride -0x14) and the 4 at this + 0x3758 + 0x6C (stride -0x1C) are destroyed through their
// first virtual (the ranges are never empty, so both loops always run). The block's own words become 0x08CBF428 /
// 0x08CC16A0, 0x0889E9F4(this + 4) finishes and the vtable at +0 becomes 0x08CC24F0.

// 0x08BEAE1C (a0 = this, a1 = the output string, a2 = pointer to the object, t0 = a source object): two scales come from
// 0x08AA2898(t0) and 0x08AA2898(t0 + 4) (f21 and f20). The object (word(a2)) goes to the member described by the pair at
// this + 0x14 / +0x18 (bit 0 of the second word = a virtual whose vtable is at word(object + (pair >> 1))), whose answer
// feeds the pair at this + 0x1C / +0x20 the same way, called with f12 = the first scale and f13 = the second. Its answer
// becomes a string (0x08B8C564(sp, answer)) assigned to a1, and the temporary released.

// 0x08BF6E48 (a0 = a container, a1 = a flag, a2 = an iterator, a3 = a value): unless the flag is set or the iterator is
// the container's own first slot, 0x08B36170(word(a3), word(a2 + 0x10), one of two stack words) positions the new
// entry and its negative answer asks for one. The 0x08A5BC20(0x18, 4) block's object at +0x10 takes 0x08B88F44(object,
// a3) plus the counted pointer at a3 + 4, 0x08B5F2B8(flag, block, a2, a0 + 4) links it and the count at +0x14 rises.
// v0 = the block.
void func_08BF6E48(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    const u32 iterator = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 first_slot = self + 4u;  // s4
    ctx.gpr[21] = iterator;            // s5
    ctx.gpr[20] = first_slot;
    ctx.gpr[19] = value;               // s3
    ctx.gpr[18] = self;                // s2
    u32 new_entry = 1u;                // s6
    if (flag == 0u && iterator != first_slot) {
        new_entry = 0u;
        const u32 left = word(rt, word(rt, value) - 0xCu);
        const u32 right = word(rt, word(rt, iterator + 0x10u) - 0xCu);
        put(rt, frame.at(0u), left);
        put(rt, frame.at(4u), right);
        const u32 chosen = (right < left) ? frame.at(4u) : frame.at(0u);
        const u32 answer = call_set(rt, ctx, 0x08B36170u,
                                   {word(rt, value), word(rt, iterator + 0x10u), word(rt, chosen), ctx.gpr[7], ctx.gpr[8],
                                    ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
        const u32 difference = left - right;
        const s32 placed = static_cast<s32>(answer != 0u ? answer : difference);
        ctx.gpr[16] = static_cast<u32>(placed);  // s0
        if (placed < 0) new_entry = 1u;
    }
    const u32 block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    const u32 object = block + 0x10u;  // s0
    ctx.gpr[17] = block;               // s1
    ctx.gpr[16] = object;
    if (object != 0u) {
        call_set(rt, ctx, 0x08B88F44u, {object, value});
        const u32 counted = word(rt, value + 4u);
        put(rt, object + 4u, counted);
        if (counted != 0u) call_set(rt, ctx, 0x08A5BA08u, {counted});
    }
    call_set(rt, ctx, 0x08B5F2B8u, {new_entry, block, iterator, first_slot});
    put(rt, self + 0x14u, word(rt, self + 0x14u) + 1u);
    ctx.gpr[2] = block;
}

// 0x08BF9DBC (first1 = a0, last1 = a1, first2 = a2, last2 = a3, out = t0): the merge of two sorted ranges of 8-byte
// elements (a pointer and a word) into `out`: the element with the smaller key at +0x134 of the pointed-to object goes
// first, equal keys compare their second word, and the rest of each range is copied. v0 = the end of the output.

// 0x08C15E8C (a0 = this, a1 = a copy destination, a2 = a pointer into the vector): the records of 0x2C bytes between a2
// and word(this + 4) (the word distance / 11, the compiler's divide by 11) are copied to a1 (words at +0 and +4, eight
// floats at +8..+0x24, the word at +0x28), 0x08C15E7C(destination, word(this + 4), sp) runs, and the vector's end word +4
// drops by the distance between a2 and the destination with its low 2 bits cleared. v0 = the destination.

// 0x0880410C (a0 = a thread entry argument, a1 = the other): sets the module's memory size (0x08C7A940(0x05050010) and
// 0x08C7A958(0x00030306)), then 0x08C7AA18 builds a thread named 0x08C87C78 that starts at 0x08804220 with the stack
// size at 0x08CDC59C, the priority word 0x08CDC598 shifted by 10, the attributes word 0x08CDC594 with bit 31 and t1 = 0.
// 0x08C7A9D0(thread, a0, a1) starts it. v0 = 0.
// The block the asm guards with a pointer read as `lui 0x0000` / `addiu +0` (0x08C7A9F8, 0x08C7A9E8 and a store through
// that pointer) is statically dead: the two `beqz` tests on it always take their branch, so it is left out here.

// 0x08833EB8 (a0 = the output object, a2 > 0 = set, a3 = the source): the object 0x0885EDF8(0x088294D0(...)) answers its
// byte +0x624 from the source's virtual +0x3C (set: stored there) or hands that flag to a0 as a string through
// 0x08A85B1C (get: 0x08A5BAD8 assigns it).

// 0x0883932C (a0 = the output object): 0x08838FBC(sp + 4) answers a descriptor whose object is 0x08838DC4(descriptor) and
// 0x0893205C(object) a count. For each of them 0x08932100(object, index) answers a key and 0x08A5DEE8(sp, element, key,
// descriptor) a string (element = word(descriptor + 0x14) + index * 4), assigned over the same element (0x08A5BAD8).
// Finally a0 is assigned from the descriptor's first string and sp + 8 released.

// 0x088DCFD0 (a0 = a request): with the object word(a0 + 0x28) and the id word(a0 + 0x20) under the lock at object + 0xC8,
// 0x088DC8C0(object, id) decides. When it answers, the lock is dropped and the handler word(a0 + 0x24) runs with
// (id, a0); then the id, the words +0x30 / +0x34 (-1) and byte +0x2C are reset and 0x088D9C24(object, id, object + 0x90,
// object + 0x94) records its answer at word(a0 + 0x1C) + 0x2D0 / +0x2D4 with 3 at +0x2C8 and byte +0x2B4 cleared.
// Otherwise, with 0x088DC8C0 still refusing: 0x088E7F1C(word(a0 + 0x1C)) and 0x088E8688 finish. Ends with 0x08B3457C.

// 0x08937460 (a0 = a builder): builds the texts of the first two parts (0x08B7BB6C + 0x08B7B9AC from a0 + 0xC) around
// 0x08CA220C / 0x08CDDE0C[word(a0 + 8)], keeps the sizes 0x08B62344(0xC) with 0x08937B44 at word(a0 + 0x1EC) and
// word(a0 + 0x1F0), and returns the size 0x08937BFC(word(a0 + 0x1EC), sp) plus the answer of
// 0x08937CA0(word(a0 + 0x1F0), sp, 0x08937AFC(0x10, 0x089372C8(a0, a0 + 0xC, 0x08CA21D8)), that size).

// 0x08938CB4 (a0 = this, a1 = a key, a2 = a value): under the lock at this + 0x3C, 0x0893A414(this + 0x28, a1, a2)
// answers whether the entry exists; when it does, its word +4 is returned. Otherwise 0x08938684(this, a1) gives an index
// whose element at word(this + 4) + index * 8 (with the count at word(this + 4) + 8) has a byte +4 flag: set, its value
// goes to 0x08938940(this, word(this + 0x24) + element + 0x14, a2); clear, 0x08B79A88(a2, same, 0) runs and 0x0893A4AC
// then stores. v0 = the entry's word +4 (or 0).

// 0x08983A18 (a0 = this, a2 = five source objects): each of the five sources (words +0, +4, +8, +0xC, +0x10) must exist
// (0x08CA383C). The first answers a word through its virtual +0x3C, kept at word(this + 0xE0); the other four answer
// floats through virtual +0x54, stored at word(this + 0xE4) + 0, 4, 8 and 0xC.

// 0x08B0DAB8: four scans over the global list 0x08D7734C whose nodes hold an object at +4: the object's virtual +0x1C
// must not answer zero (else 0), the virtual +0x24 and then the virtual +0x28 must not answer zero either (the flag),
// and no object's virtual +0x2C may answer zero. v0 = the flag, or 0 when a scan failed.
bool scan_nodes(Runtime &rt, AllegrexContext &ctx, u32 head, u32 offset) {
    for (u32 node = word(rt, head); node != 0u; node = word(rt, node)) {
        const u32 object = word(rt, node + 4u);
        if (call_indirect(rt, ctx, word(rt, word(rt, object) + offset), {object}) == 0u) return false;
    }
    return true;
}

// 0x08B33D3C (a0 = destination, a1 = source, a2 = length): the 64-byte-aligned ranges of the copy are opened with
// 0x08C7AAF0 / 0x08C7AAE8, 0x08B33AF4, 0x08B33ACC(a0) and 0x08C7A830(a0, a1, a2) copy, and 0x08B33A7C(a0) closes them.
// A nonzero answer that is negative is retried; when the answer is zero, 0x08B2DE00() may replace it with 0x80020321.

// 0x08B44F40 (a0 = this, a1 = passed to 0x08B3F098): word(this + 0xBC) is cleared and, when the object's virtual +0xA0
// answers, under the lock at this + 0x12C the object at word(this + 0xB0) answers its virtual +0x3C and
// 0x08B3F098(this + 0x8DC, a1, that answer, its vtable word) decides: on success byte(v0 + 5) and byte(this + 0xB9) are
// set and 1 returned, on failure word(this + 0xBC) becomes -1 and 0 is. Without that answer the virtual +0x1C's word and
// v1 go to the stack, 0x08B429D0(this, 3, sp, 0x10, 1) runs, and its refusal again sets -1. v0 = 1 or 0.

// 0x08C037D0 (a0 = this, a1 = the output string, a2 = pointer to the object): the object (word(a2)) goes to the member
// described by the pair at this + 0x14 / +0x18 (bit 0 of the second word = a virtual whose vtable is at
// word(object + (pair >> 1)); the word at +0x14 is the adjust and also the plain function) with (object, adjust), and its
// answer feeds the pair at this + 0x1C / +0x20 the same way, called with (sp + 4, answer + (pair >> 1)). The answer at
// sp + 4 becomes the string 0x08BE9EEC(sp + 8, sp + 4), assigned to a1, and both temporaries are released (sp + 4 as a
// shared block).

// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
// Allegrex `wsbh` (swap the bytes of each halfword) and `wsbw` (swap the bytes of the word).
u32 swap_halves(u32 value) { return ((value & 0x00FF00FFu) << 8) | ((value & 0xFF00FF00u) >> 8); }
u32 swap_bytes(u32 value) { return __builtin_bswap32(value); }
// The unrolled 16-byte copies of the batch, as one loop over words.
void copy_words(Runtime &rt, u32 destination, u32 source, u32 words) {
    for (u32 index = 0u; index < words; ++index) put(rt, destination + index * 4u, word(rt, source + index * 4u));
}

// 0x088287C4 (a0 = the output object, a2 > 0 = set, a3 = the source): 0x088248E4(sp + 8) answers an object whose word
// +0x14 is taken before the temporary is released. Set: word(source) non-zero converts through 0x088435A8(sp + 4, source)
// and 0x08843404, else the member is cleared (0x0885BEE0(member, 0)); get: 0x0885BF50(member) becomes a string at sp that
// a0 is assigned from.

// 0x0882EC60 (a0 = the output object, a2 > 0 = set, a3 = the source): the object 0x0885EE58(0x088294D0(...)) carries the
// member. Set: the source's virtual +0x3C feeds 0x0889367C(member, answer); get: 0x08893674(0x0885EE4C(member)) becomes a
// string assigned to a0.

// 0x088330DC (a0 = the output object): the object 0x08893768(0x0885EE58(...)) answers a signed halfword at +6: that many
// entries (each its own halfword at +0x560, stride 2) become strings through 0x08A85B1C and are assigned element by
// element of the array at word(sp + 8) + 0x14; the whole array is assigned to a0.

// 0x08857798 (a0 = the output object): 0x08854678(0x088547C4(sp + 8)) answers the object, whose array (sp + 0xC) of 2
// entries has the first assigned from 0x088A60A8(object) and the second from 0x088A60C8(object). The array is assigned to
// a0 and both temporaries are released.

// 0x0885CA48 (a0 = a holder, a1 = an index): the record at word(word(a0)) + 392 * index (392 = 7 * 56), whose id at
// +0x1C is not -1, has its two entries (0x0885D56C, index 0 and 1) offered to 0x08860FF8(word(a0 + 4), id, count,
// answer) while they answer, counting up; when fewer than two did, two more entries (0x0885DA7C) are offered the same way
// and the accepted ones stored with 0x0885D8CC(record, count, index). v0 = -1 for a record without id, 0 when both rounds
// were full, else the last call's answer.

// 0x08890E8C (a0 = passed to 0x08890CC8, a1 = a source object, a2 = a flag byte): 0x0888F548(a0) answers a block whose
// flagged words and 4 by 8-byte entries (0x088DF13C for the words, 0x088DA1C8 for the halves) are byte-swapped in. The
// block's words +0x18 / +0x1C come from 0x08B2DEE4()'s two answers, and word +0x6C from 0x088CEF68(word(a1 + 0x628))
// when the flag is set (0 when that word is empty).

// 0x088A6FB4 (a0 = this): the record at word(this) + 384 * byte(word(this) + 8) + 0x38 answers 0x08932558's pair, then
// 0x088A61A4(this) and 0x088A5A48(this, sp + 4, sp). The float at sp + 4 times 10, truncated and byte-swapped, goes to
// halfword +0x4BA; the f0 0x088A5AE4 leaves, times 10000, to +0x4BC; and 0x08930EBC(pair, sp, sp + 4) decides whether
// +0x4BE takes the byte-swapped word at sp + 4 or 0xFFFF.
void func_088A6FB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = self;         // s3
    ctx.gpr[18] = frame.at(4u); // s2
    const u32 table = word(rt, self);
    const s32 count = signed_byte_at(rt, table + 8u);
    const GuestResult pair = call_set_both(rt, ctx, 0x08932558u, {table + static_cast<u32>(count) * 384u + 0x38u});
    ctx.gpr[16] = pair.v0;  // s0
    ctx.gpr[17] = pair.v1;  // s1, in the next jal's delay slot
    call_set(rt, ctx, 0x088A61A4u, {self});
    call_set(rt, ctx, 0x088A5A48u, {self, frame.at(4u), frame.at(0u)});
    const float scaled = psp_mul(float_at(rt, frame.at(4u)), 10.0f);
    const s32 tenths = trunc_w_s(scaled);
    const u32 field = word(rt, self);
    put16(rt, field + 0x4BAu, swap_halves(static_cast<u32>(tenths) & 0xFFFFu));
    const float after = call_setf(rt, ctx, 0x088A5AE4u, {self, static_cast<u32>(tenths)}).f0;
    const s32 hundredths = trunc_w_s(psp_mul(after, 10000.0f));
    const u32 second_field = word(rt, self);
    put16(rt, second_field + 0x4BCu, swap_halves(static_cast<u32>(hundredths) & 0xFFFFu));
    call_set(rt, ctx, 0x088A5A0Cu, {self, static_cast<u32>(hundredths)});
    const u32 answer = call_set(rt, ctx, 0x08930EBCu, {ctx.gpr[16], ctx.gpr[17], frame.at(0u), frame.at(4u)});
    const u32 third_field = word(rt, self);
    if (answer == 0u) {
        put16(rt, third_field + 0x4BEu, 0xFFFFu);
        ctx.gpr[2] = third_field;
        return;
    }
    const u32 swapped = swap_halves(word(rt, frame.at(4u)));
    put16(rt, third_field + 0x4BEu, swapped);
    ctx.gpr[2] = swapped;
}

// 0x088B0520 (a0 = a container): under the lock at a0 + 0x20, 0x088AFB3C walks the nodes of the container (the end is
// a0 + 0xC): each node takes 0x08B5F444(node, end), releases the counted block at +0x18 (count at +8, virtual +0xC at
// zero) and the shared block at +0x10, and 0x08B35254(node) finishes it while the count at a0 + 0x1C falls. The lock is
// dropped with 0x08B34B24.

// 0x088E15BC (a0 = an index, a1 = this): the slot at word(this + 0x780) + 8 * index takes the index and this, and the
// record at word(this + 0xC0) + 760 * index is started (0x088E7F94(record, word(record + 0x1C), 0, 0)) under the lock at
// this + 0xC8. 0x08B1A338(sp, this + 0x718, sp + 8) then answers the block at sp + 4 (a shared count pair at +4 / +8).

// 0x08937098 and 0x089371B0 (a0 = a builder): the text at a0 + 0xC with 0x08CA21EC (or 0x08CA21FC) is handed to
// 0x08939D50(a0 + 0x1F4 / a0 + 0x2D0), then 0x08B0CE58(sp + 0x40) and 0x08B0EC60(sp + 0x180, sp + 0x80) fill a 60-byte
// record that is copied down to sp and again to sp + 0x40; v0 = word(sp + 0x50).
void copy_built_text(Runtime &rt, AllegrexContext &ctx, u32 field, u32 suffix) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x1D0u);
    ctx.gpr[16] = frame.at(0x80u);  // s0
    ctx.gpr[17] = self;             // s1
    call_set(rt, ctx, 0x08B7BB6Cu, {frame.at(0x80u), self + 0xCu});
    call_set(rt, ctx, 0x08B7B9ACu, {frame.at(0x80u), suffix});
    ctx.gpr[18] = frame.at(0x40u);  // s2
    call_set(rt, ctx, 0x08939D50u, {self + field, frame.at(0x80u)});
    call_set(rt, ctx, 0x08B0CE58u, {frame.at(0x40u)});
    call_set(rt, ctx, 0x08B0EC60u, {frame.at(0x180u), frame.at(0x80u)});
    copy_words(rt, frame.at(0u), frame.at(0x180u), 15u);  // 60 bytes, as the unrolled 4-word and 3-word copies do
    copy_words(rt, frame.at(0x40u), frame.at(0u), 15u);
    ctx.gpr[2] = word(rt, frame.at(0x50u));
}

// 0x089E2548 (a0 = this): the vtable words 0x08CBC4E0 (at +0) and 0x08CBC71C (at +0x38), then the six temporaries at
// +0x220, +0x21C, +0x218, +0x214, +0x210 and +0x20C are released, then the three shared blocks at +0x1AC, +0x1A8 and
// +0x1A4 (sinks sp + 2, sp + 1 and sp), and 0x08A08B2C(this) finishes.
void func_089E2548(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;  // s0
    put(rt, self, 0x08CBC4E0u);
    put(rt, self + 0x38u, 0x08CBC71Cu);
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x220u});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x21Cu});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x218u});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x214u});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x210u});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x20Cu});
    release_shared(rt, ctx, self + 0x1ACu, frame.at(2u));
    release_shared(rt, ctx, self + 0x1A8u, frame.at(1u));
    release_shared(rt, ctx, self + 0x1A4u, frame.at(0u));
    call_set(rt, ctx, 0x08A08B2Cu, {self});
}

// 0x08A4AE9C (a0 = the output object, a1 > 0, a2 = pointer to a source object): the source (word(a2)) is passed to its
// virtual third function with sp + 8, that temporary becomes a 60-byte record (0x08B0EC60(sp + 0x50, sp + 8)) copied to
// sp + 0x10, and the flag (word(sp + 0x10) == 0) is assigned to a0. Both temporaries are released (sp + 8 as a shared
// block). Without a1 > 0 nothing happens.
void func_08A4AE9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const s32 enabled = static_cast<s32>(ctx.gpr[5]);
    const u32 holder = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xA0u);
    ctx.gpr[17] = output;  // s1
    if (enabled <= 0) return;
    const u32 source = word(rt, holder);
    ctx.gpr[16] = source;  // s0 = sp + 0x50 later
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    const u32 function = word(rt, word(rt, source) + 0xCu);
    (void)call_indirect(rt, ctx, function, {frame.at(8u), source});
    call_set(rt, ctx, 0x08B0EC60u, {frame.at(0x50u), word(rt, frame.at(8u))});
    copy_words(rt, frame.at(0x10u), frame.at(0x50u), 15u);
    const u32 temporary = frame.at(4u);
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, word(rt, frame.at(0x10u)) == 0u ? 1u : 0u});
    if (output != temporary) call_set(rt, ctx, 0x08A5BAD8u, {output, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    release_shared(rt, ctx, frame.at(8u), frame.at(0u));
}

// 0x08A54718 (a0 = a map, a1 = a key holder): every node of the list at a0 + 0x44 (end a0 + 0x3C, next from
// 0x08B5F16C) has its object at +0x14: the key is that object's word +0x10 plus word(a1 + 0x1C), or plus
// word(word(a1 + 0x18) + 0xC) when that word is zero (0x08CAF540 when the holder is missing, 0x08CAF594 when the object
// is). 0x08A57B54(sp, object + 0xC, a1 + 0x18, key) builds an entry at sp whose object 0x08A56974 takes, then sp is
// released (0x08CAF5E0 when it is empty).
void func_08A54718(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 end = self + 0x3Cu;
    ctx.gpr[21] = end;              // s5
    ctx.gpr[20] = holder;           // s4
    ctx.gpr[22] = frame.at(0u);     // s7 = sp
    ctx.gpr[30] = 0x08CB0000u;      // fp (the base of the 0x08CAF5xx messages)
    u32 node = word(rt, self + 0x44u);
    if (node == end) return;
    do {
        const u32 object = word(rt, node + 0x14u);  // s0
        ctx.gpr[19] = node + 0x14u;                 // s3
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
        const u32 base = word(rt, object + 0x10u);
        u32 key;
        if (word(rt, holder + 0x1Cu) != 0u) {
            key = word(rt, holder + 0x1Cu) + base;
        } else {
            const u32 table = word(rt, holder + 0x18u);
            if (table == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF540u});
            key = word(rt, table + 0xCu) + base;
        }
        ctx.gpr[17] = key;  // s1
        const u32 entry_object = word(rt, node + 0x14u);
        ctx.gpr[16] = entry_object;  // s0
        if (entry_object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
        call_set(rt, ctx, 0x08A57B54u, {frame.at(0u), entry_object + 0xCu, holder + 0x18u, key});
        const u32 built = word(rt, frame.at(0u));
        ctx.gpr[16] = built;
        if (built == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF5E0u});
        call_set(rt, ctx, 0x08A56974u, {built});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        node = call_set(rt, ctx, 0x08B5F16Cu, {node});
    } while (node != end);
}

// 0x08A9DDCC (a0 = this): the vtable word 0x08CD48C0 at +0, 0x08A9DCB0(1), then the temporaries at +0x78, +0x74 and the
// block at +0x4C (0x08B28DC4) are released, then the shared blocks at +0x48 (sink sp + 1) and +0x44 (sink sp), the four
// temporaries at +0x40, +0x3C, +0x38 and +0x34, the blocks at +0x2C (0x08C2F9F0) and +0x24 (0x08C2F9A4), the list at
// +0xC (0x08C301B0), and finally this itself goes to 0x08A5BB5C and to 0x08A5BC54(0x80, 4).

// 0x08AAF720 (a0 = the output string, a1 = a source): builds a text from the pieces 0x08CB3A44, the answer of its virtual
// +0x24 through 0x08A7BDB0, 0x08CB3A48, the text at source + 0x14, 0x08CB3A58, the word at source + 0x18, 0x08CB3A5C (all
// through 0x08AABA8C / 0x08AAB710 / 0x08AAB8F0) and assigns 0x08AAB540(sp + 4) to a0, releasing sp + 4.

// 0x08ACD2EC (a0 = this, a1 = a flag byte): for a nonzero flag every entry (count word(this + 0x32C), stride 0xF0) gets
// byte +0x50C set to 1 under its own lock at +0x4A0; for a zero flag they are cleared instead and each entry at
// this + 0x448 is finished with 0x08B28C74.
void func_08ACD2EC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;  // s1
    const s32 count = static_cast<s32>(word(rt, self + 0x32Cu));
    if (count <= 0) return;
    ctx.gpr[19] = 0u;   // s3
    ctx.gpr[20] = 1u;   // s4
    if (flag != 0u) {
        u32 lock = self + 0x4A0u;  // s0
        for (s32 index = 0; index < count; ++index) {
            call_set(rt, ctx, 0x08B344E0u, {lock});
            put8(rt, self + static_cast<u32>(index) * 0xF0u + 0x50Cu, 1u);
            call_set(rt, ctx, 0x08B3457Cu, {lock});
            lock += 0xF0u;
        }
        return;
    }
    u32 lock = self + 0x4A0u;
    for (s32 index = 0; index < count; ++index) {
        call_set(rt, ctx, 0x08B344E0u, {lock});
        put8(rt, self + static_cast<u32>(index) * 0xF0u + 0x50Cu, 0u);
        call_set(rt, ctx, 0x08B3457Cu, {lock});
        lock += 0xF0u;
    }
    u32 entry = self + 0x448u;
    for (s32 index = 0; index < count; ++index) {
        call_set(rt, ctx, 0x08B28C74u, {entry});
        entry += 0xF0u;
    }
}

// 0x08B44698 (a0 = this, a1 = a request): 0x08B3F210(this + 0x8DC, word(a1)) or, when that answers nothing,
// 0x08B3F144(this + 0x8DC, a1 + 0x1A) answers the entry; when the entry's word +8 differs from word(a1) the answer is 0.
// Otherwise, with the object's virtual +0xA0 answering, the entry is filled (0x08B3F478(sp + 8), 0x08B36018(sp, 0, 0x58),
// 0x08B3E7FC(sp + 8, entry)), 0x08B42A90(this, 0x4E25, sp, 0x58, 0, 1, 0, 0) sends it and 0x08B41F70(this, the entry's
// halfword) follows, then 0x08B402AC(this) and 0x08B43A38(this). v0 = 1 or 0.

// 0x08BAF828 (a0 = a container, a1 = a flag, a2 = an iterator, a3 = a value): the same insert as 0x08BF6E48 but the entry
// comes from 0x08B35DA0(0x1C), its object at +0x10 from 0x08B9432C, and the counted pointer at a3 + 4 / +8 is stored at
// the block's +0x14 / +0x18, its count at +8 rising. 0x08B5F2B8(flag, block, a2, a0 + 4) links it, the count at +0x14
// rises. v0 = the block.
void func_08BAF828(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    const u32 iterator = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 first_slot = self + 4u;  // s4
    ctx.gpr[21] = iterator;            // s5
    ctx.gpr[20] = first_slot;
    ctx.gpr[19] = self;                // s3
    ctx.gpr[18] = value;               // s2
    u32 new_entry = 1u;                // s6
    if (flag == 0u && iterator != first_slot) {
        new_entry = 0u;
        const u32 left = word(rt, word(rt, value) - 0xCu);
        const u32 right = word(rt, word(rt, iterator + 0x10u) - 0xCu);
        put(rt, frame.at(0u), left);
        put(rt, frame.at(4u), right);
        const u32 chosen = (right < left) ? frame.at(4u) : frame.at(0u);
        const u32 answer = call_set(rt, ctx, 0x08B36170u,
                                   {word(rt, value), word(rt, iterator + 0x10u), word(rt, chosen), ctx.gpr[7], ctx.gpr[8],
                                    ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
        const u32 difference = left - right;
        const s32 placed = static_cast<s32>(answer != 0u ? answer : difference);
        ctx.gpr[16] = static_cast<u32>(placed);  // s0
        if (placed < 0) new_entry = 1u;
    }
    const u32 block = call_set(rt, ctx, 0x08B35DA0u, {0x1Cu});
    const u32 object = block + 0x10u;  // s0
    ctx.gpr[16] = object;
    if (object != 0u) {
        call_set(rt, ctx, 0x08B9432Cu, {object, value});
        put(rt, block + 0x18u, word(rt, value + 8u));
        put(rt, block + 0x14u, word(rt, value + 4u));  // the store sits in the beqz's delay slot
        const u32 counted = word(rt, value + 8u);
        if (counted != 0u) put(rt, counted + 8u, word(rt, counted + 8u) + 1u);
    }
    call_set(rt, ctx, 0x08B5F2B8u, {new_entry, block, iterator, first_slot});
    put(rt, self + 0x14u, word(rt, self + 0x14u) + 1u);
    ctx.gpr[2] = block;
}

// 0x08BDD028 (a0 = this): the vtable word 0x08CC5EE0 at +0 and three runs of three objects of 0x2C bytes destroyed through
// their first virtual: at this + 0x1A0, this + 0x11C and this + 0x98 (each run down to this + 0x148, +0xC4 and +0x40).
// Ends with 0x08925F98(this).
void func_08BDD028(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[21] = self + 0x148u;  // s5 (the end of the first two runs)
    ctx.gpr[18] = self;           // s2
    put(rt, self, 0x08CC5EE0u);
    for (u32 index = 0u; index < 3u; ++index) {
        const u32 entry = self + 0x1A0u - index * 0x2Cu;
        (void)call_indirect(rt, ctx, word(rt, word(rt, entry)), {entry});
    }
    for (u32 index = 0u; index < 3u; ++index) {
        const u32 entry = self + 0x11Cu - index * 0x2Cu;
        (void)call_indirect(rt, ctx, word(rt, word(rt, entry)), {entry});
    }
    for (u32 index = 0u; index < 3u; ++index) {
        const u32 entry = self + 0x98u - index * 0x2Cu;
        (void)call_indirect(rt, ctx, word(rt, word(rt, entry)), {entry});
    }
    tail_call(ctx, 0x08925F98u, {self});
}

// 0x0882EE14 and 0x0882EF30 (a0 = the output object): the object 0x0885E710(0x088294D0(...)) fills an array at sp + 0xC
// with 3 (or 4) floats (0x08917F78 / 0x08917FAC), each turned into a string with 0x08A87EEC(sp, float) and assigned to the
// same element of the array at word(sp + 8) + 0x14. The array is assigned to a0.
void build_float_strings(Runtime &rt, AllegrexContext &ctx, u32 filler, u32 count) {
    const u32 output = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 values = frame.at(0xCu);  // s5
    const u32 array = frame.at(8u);     // s2
    const u32 temporary = frame.at(4u); // s1
    const u32 incoming_a1 = ctx.gpr[5];
    ctx.gpr[22] = output;   // s6
    ctx.gpr[17] = temporary;
    call_set(rt, ctx, 0x088297A8u, {temporary, incoming_a1});  // a1 is still the caller's
    const u32 object = word(rt, temporary);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 converted = call_set(rt, ctx, 0x088294D0u, {object});
    ctx.gpr[16] = converted;
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    const u32 filled = call_set(rt, ctx, 0x0885E710u, {converted});
    call_set(rt, ctx, filler, {values, filled, 0u});
    ctx.gpr[18] = array;
    call_set(rt, ctx, 0x08A98C1Cu, {array, count});
    ctx.gpr[19] = count;     // s3
    ctx.gpr[20] = frame.at(0u);  // s4
    u32 index = 0u;          // s1
    do {
        const u32 entry = word(rt, frame.at(8u)) + 0x14u;
        const u32 offset = index << 2;
        const u32 element = word(rt, entry) + offset;
        ctx.gpr[16] = element;
        ++index;
        (void)call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {float_at(rt, values + offset)});
        if (element != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {element, word(rt, frame.at(0u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    } while (index != count);
    if (output != array) call_set(rt, ctx, 0x08A5BAD8u, {output, word(rt, array)});
    call_set(rt, ctx, 0x08A5B9ECu, {array});
}

// 0x08830350 (a0 = the output object): 0x0885EC1C(0x088294D0(...)) answers a count; 0x088D6294(object + 4, index) fills an
// array at sp + 8 with that many strings, each assigned to the same element of the array at word(sp + 8) + 0x14. The
// array is assigned to a0.

// 0x088511E0 (a0 = the output object): the object 0x088A5A90(0x08854678(...)) fills five floats (0x088A5CC0(object, sp + 0xC,
// sp + 0x10, sp + 0x14, sp + 0x18, sp + 0x1C)), each turned into a string with 0x08A87EEC(sp, float) and assigned to the
// same element of the array at word(sp + 8) + 0x14. The array is assigned to a0.

// 0x08856D40 (a0 = the output string): 0x088A5A90(0x08854678(...)) formats 0x08C9E518 with 0x08C9E52C into the 0x80-byte
// buffer at sp + 0x10 (0x08B3767C(buffer, 0x80, 0x08C9E518, answer, 0x08C9E52C)), 0x08B85EB8(sp + 4, buffer, sp + 1)
// converts it, 0x08A6D2D4(sp + 8, sp + 4) assigns the text to a0, and sp + 4 is released as a shared block.

// 0x0885F550 (a0 = a holder): word(a0) and its words +0x5B8 / +0x3FD4 start cleared (0x0885F46C when byte +0x3FDB is
// set). With the block's byte +0x26 set, the eight entries of 0x18 bytes at +0x3E90 (count word +0xA / +0x10 / +0x12
// signed, index byte +8) are updated: a byte +8 of -1 steps the halfword at +0xA, otherwise the halfword at +0x12 rises and
// 0x088DB118(block, index) decides whether byte +9 takes its answer or the halfword at +0x10 steps. Ends with
// 0x088D4D24(word(word(this) + 0x3F50) + 0x7C).

// 0x0888EC10 (a0 = the holder of the new object): 0x08B62344(0x1D8) is the object: its parts at +0x188 (0x088D65DC),
// +0x190 (0x08894444) and +0x198 (0x08893F7C) are started, the 0x178 bytes at +8 cleared, the words +0 / +0x180 / +0x184
// zeroed, four words at +0x1A0 and four pairs of words at +0x1B0 cleared, word +0x1D0 set to -1 and byte +0x1D4 cleared.
// The holder takes the object and the object's word +4 the block 0x08D75E90 (0x08CC1D18 when it was empty, under
// 0x08B6441C / 0x08B64494 with the flag at 0x08D75E88).

// 0x088B0638 (a0 = the result pair, a1 = a container, a2 = a key): under the lock at a1 + 0x20 the map at a1 + 8 with the
// key 0x08B93CD8(sp + 4, a2, sp + 1) answers a node whose word +0x18 is a counted pointer: when it exists the pair takes
// word(node + 0x14) and the pointer (its count at +4 rising). The key block at sp + 4 is released as a shared block and the
// lock dropped with 0x08B34B24. v0 = a0.

// 0x0897B6E4 (a0 = this): 0x08A76AB4(sp, 0x08CF6ED8) sets the scan up. While 0x0897B5C0(this, sp) answers a node, the
// node's virtual +0x108 runs and the bit (word(this + 0x7C) - 1) of the bitset at word(this + 0x7C) + 8 must be clear to
// stop (a set bit loops). Then 0x08977260(this) with 0x089ABF68 / 0x089A9570 and byte +0x1B5 cleared.

// 0x089A5AB0 (a0 = this): the vtable words 0x08CBAD20 at +0 and 0x08CBAF78 at +0x38, then the parts at +0x16C
// (0x08BF7980), +0x154 (0x08BF738C), +0x150, the object at +0x144 (0x08A5B930), +0x13C (0x08BF5ADC), the three shared
// blocks at +0x130 (sink sp + 2), +0x12C (sink sp + 1) and +0x128 (sink sp), and 0x089A29E4(this).
void func_089A5AB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;  // s0
    put(rt, self, 0x08CBAD20u);
    put(rt, self + 0x38u, 0x08CBAF78u);
    call_set(rt, ctx, 0x08BF7980u, {self + 0x16Cu, word(rt, self + 0x174u)});
    call_set(rt, ctx, 0x08BF738Cu, {self + 0x154u, word(rt, self + 0x15Cu)});
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x150u});
    if (word(rt, self + 0x144u) != 0u) call_set(rt, ctx, 0x08A5B930u, {word(rt, self + 0x144u)});
    call_set(rt, ctx, 0x08BF5ADCu, {self + 0x13Cu});
    release_shared(rt, ctx, self + 0x130u, frame.at(2u));
    release_shared(rt, ctx, self + 0x12Cu, frame.at(1u));
    release_shared(rt, ctx, self + 0x128u, frame.at(0u));
    call_set(rt, ctx, 0x089A29E4u, {self});
}

// 0x08AF5A4C (a0 = a tree, a1 = an index, a2 = a destination, a3 = a base, t0 = a source): the record at word(a0 + 0x9C) +
// 128 * a1: its signed halfword +8 names a sub tree that is walked first (0x08B4E734(destination, a2) prepares it), a
// text is built with 0x08AF5830(a2, t0 + 24 * a1, record + 0x40, t0 + 24 * a1 + 0xC, byte(record + 0xC)) and copied to
// a3 + 64 * a1 as 0x40 bytes, and the signed halfword +6 names another sub tree walked the same way. Recursive.

// 0x08B3120C (a0 = this): a frame of word(this + 0x10) 16-byte entries is taken on the stack (plus one word), zeroed with
// 0x08B36018 and its first word set to the count; under the lock at this + 0x8FC the records at +0x928 (stride 0x98 up to
// +0x980) whose first two words are not both zero are collected as three words each. 0x08B308FC(this, sp, the distance of
// the collected part, 4, 1, 1) takes them, sp = fp restoring the frame.

// 0x08BF6B98 (a0 = this, a1 = the output string, a2 = pointer to the object): the object (word(a2)) goes to the member
// described by the pair at this + 0x14 / +0x18 (bit 0 of the second word = a virtual whose vtable is at
// word(object + (pair >> 1)); +0x14 is the adjust, called with (object, adjust)), whose answer feeds the pair at
// this + 0x1C / +0x20 the same way, called with (answer + (pair >> 1), pair). That answer becomes a string
// (0x08B88F44(sp + 4, answer), then 0x08BE9EEC(sp + 8, sp + 4)) assigned to a1, and sp + 4 is released as a shared block.
void func_08BF6B98(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 output = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = output;  // s2
    ctx.gpr[16] = self;    // s0
    const u32 object = word(rt, holder);
    const u32 adjust = word(rt, self + 0x14u);
    const u32 member = word(rt, self + 0x18u);
    const u32 target = object + (member >> 1);
    const u32 function = (member & 1u) != 0u ? word(rt, word(rt, target) + adjust) : adjust;
    const u32 answer = call_set(rt, ctx, function, {target, adjust});
    const u32 second_member = word(rt, self + 0x20u);
    const u32 second_target = answer + (second_member >> 1);
    const u32 second_function = (second_member & 1u) != 0u
                                    ? word(rt, word(rt, second_target) + word(rt, self + 0x1Cu))
                                    : word(rt, self + 0x1Cu);
    ctx.gpr[16] = frame.at(4u);  // s0 = sp + 4
    const u32 result = call_set(rt, ctx, second_function, {second_target, second_member});
    call_set(rt, ctx, 0x08B88F44u, {frame.at(4u), result});
    call_set(rt, ctx, 0x08BE9EECu, {frame.at(8u), frame.at(4u)});
    if (output != frame.at(8u)) call_set(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(8u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    release_shared(rt, ctx, frame.at(4u), frame.at(0u));
}

// 0x08BFF7B8: the same as 0x08BF6B98, byte for byte.
void func_08BFF7B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 output = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = output;  // s2
    ctx.gpr[16] = self;    // s0
    const u32 object = word(rt, holder);
    const u32 adjust = word(rt, self + 0x14u);
    const u32 member = word(rt, self + 0x18u);
    const u32 target = object + (member >> 1);
    const u32 function = (member & 1u) != 0u ? word(rt, word(rt, target) + adjust) : adjust;
    const u32 answer = call_set(rt, ctx, function, {target, adjust});
    const u32 second_member = word(rt, self + 0x20u);
    const u32 second_target = answer + (second_member >> 1);
    const u32 second_function = (second_member & 1u) != 0u
                                    ? word(rt, word(rt, second_target) + word(rt, self + 0x1Cu))
                                    : word(rt, self + 0x1Cu);
    ctx.gpr[16] = frame.at(4u);  // s0 = sp + 4
    const u32 result = call_set(rt, ctx, second_function, {second_target, second_member});
    call_set(rt, ctx, 0x08B88F44u, {frame.at(4u), result});
    call_set(rt, ctx, 0x08BE9EECu, {frame.at(8u), frame.at(4u)});
    if (output != frame.at(8u)) call_set(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(8u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    release_shared(rt, ctx, frame.at(4u), frame.at(0u));
}

} // namespace
} // namespace new_batch_18
} // namespace gt::game

#define NEW18(address, name, kind) GT_DECOMP(address, gt::game::new_batch_18::name, gt::ReturnKind::kind)

NEW18(0x08BF6E48u, func_08BF6E48, Word);
NEW18(0x088A6FB4u, func_088A6FB4, Word);
NEW18(0x089E2548u, func_089E2548, Word);
NEW18(0x08A4AE9Cu, func_08A4AE9C, Word);
NEW18(0x08A54718u, func_08A54718, Word);
NEW18(0x08ACD2ECu, func_08ACD2EC, Word);
NEW18(0x08BAF828u, func_08BAF828, Word);
NEW18(0x08BDD028u, func_08BDD028, Word);
NEW18(0x089A5AB0u, func_089A5AB0, Word);
NEW18(0x08BF6B98u, func_08BF6B98, Word);
NEW18(0x08BFF7B8u, func_08BFF7B8, Word);
