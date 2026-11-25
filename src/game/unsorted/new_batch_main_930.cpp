// Main-session batch 930: functions an earlier draft (batch 14) got wrong in the gate or never verified, rewritten by hand as
// structured C from `tools.disasm`. Delay slots are read as "before the branch / call"; every argument register a callee
// reads is passed (a1 is threaded through from the previous call where the original left it there); v0 ends as the asm
// leaves it on every exit path. `GuestFrame` sits on the signature line so tools.frame_audit reads it. No names: none has
// evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_930 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values, and a1 too — a caller's next call reads the
// a1 the callee left when the asm does not set it again.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
// `lw v0, slot(vtable); jalr v0` and `lw v1, slot(v0); jalr v1`: the callee starts with the v0 / v1 the asm has at the jalr
// (a callee that never writes v0 returns that value to its caller).
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, u32 v0_at_call, u32 v1_at_call,
                         std::initializer_list<u32> arguments) {
    ctx.gpr[2] = v0_at_call;
    ctx.gpr[3] = v1_at_call;
    return call_full(rt, ctx, target, arguments);
}

// wsbw: the bytes of a word reversed (these records are big-endian).
u32 swapped(u32 value) { return __builtin_bswap32(value); }

// s0-s3 are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 4u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 4u; ++i) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[4];
};

// 0x0880AEC4 (a0 = this): the vtables 0x08CBA3A0 (at +0) and 0x08CBA5D8 (at +0x38) are stored; the object the member at +0xE8
// points to runs its first virtual with (it, the member's pointer) and both member words go to 0; then a tail call finishes.
void func_0880AEC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    const u32 member = self + 0xE8u;
    ctx.gpr[17] = member;
    put(rt, self, 0x08CBA3A0u);
    put(rt, self + 0x38u, 0x08CBA5D8u);
    const u32 held = word(rt, member + 4u);
    if (held != 0u) {
        const u32 inner = word(rt, held);
        call_virtual(rt, ctx, word(rt, inner), word(rt, inner), inner, {held, word(rt, member)});
        put(rt, member + 4u, 0u);
        put(rt, member, 0u);
    }
    tail_call(ctx, 0x0896BC0Cu, {self});
}

// 0x088152F8 (a1 = a holder, a2 = first, a3 = second): 0x0880F0C0(sp, holder) builds a new object into the stack word (a null
// one is the error string 0x08C986F4, reported through 0x08A5BB7C); its member at +0xE8 takes (first, second) through
// 0x08813284 and the stack record is released (0x08A5B9EC). v0 = that release's result.

// 0x0885BDA8 (a0 = this): the member at +8 is locked (0x08B344E0); the fifth argument is the word at vtable +0x88 when the byte
// at vtable +0x19 is set, else 0; 0x088619A8(word(this + 4), +0x7C, +0x80, +0x84, fifth) runs, the member is released
// (0x08B3457C) and its result is v0.

// 0x0885D010 (a0 = this): the member at +8 is locked (0x08B344E0); the byte at vtable +0x17 picks 0x0885BF5C(this) or
// 0x0885CF00(this), the member is released (0x08B3457C) and the pick's result is v0.

// 0x08890C5C (a0 = this): 0x08890AC0(this, a1) runs and its answer goes to s0; 0x0888F548(this) answers again, and the word at
// answer + 4 (byte-reversed) decides: zero takes the word at s0-answer + 8, byte-reversed again and added to word(this); else
// that word verbatim. The stack word is read back byte-reversed into v0.

// 0x088E0B78 (a0 = this, a1 = a value): when the words at +0x5D0 and +0x5D4 are not both set, the member at +0x5E8 is
// locked and released (0x08B344E0, 0x08B3457C) and the value re-read; then the word at +0x5D4 takes the value through
// 0x088F9E38 as a tail call.
void func_088E0B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 member = self + 0x5E8u;
    ctx.gpr[18] = value;
    ctx.gpr[17] = member;
    ctx.gpr[16] = self;
    u32 target = word(rt, self + 0x5D4u);
    if (word(rt, self + 0x5D0u) == 0u || target == 0u) {
        call_full(rt, ctx, 0x08B344E0u, {member});
        call_full(rt, ctx, 0x08B3457Cu, {member});
        target = word(rt, self + 0x5D4u);
    }
    tail_call(ctx, 0x088F9E38u, {target, value});
}

// 0x088E66C0 (a0 = this): the object word(this) has its member at +0xEC locked (0x08B344E0), its virtual at vtable +0x50 runs
// with -1 and a found entry gets the byte +7 set to 1, the member is released (0x08B3457C) and 0x088E511C(this) answers; v0 is
// that answer with its low bit flipped (`xori 1`), masked to a byte.

// 0x088EABC8 (a0 = this, a1 = a byte): the member at +0x538 answers its virtual at +0xA0 (byte(this, a1, byte) as arguments);
// when that is zero it answers its virtual at +0x38 with (word(this + 0xA8), byte). Either nonzero clears the word at +0x560
// and makes v0 = 2, otherwise v0 = 0.

// 0x088EC13C (a0 = this): the virtual at vtable +0xA4 runs; the member at +0x538 then answers its virtual at +0x44 and, when
// that is nonzero, the word at +0x56C is cleared and v0 = 5, otherwise the virtual at vtable +0x10 runs and v0 = 0.

// 0x089176EC (a0 = a three-float destination, a1 = a holder): the holder's object at +4 fills three floats on the stack
// through its virtual at vtable +0x58 (with (object, sp, a2)); they are copied into the destination (which is zeroed when
// there is no object). v0 = the destination.
void func_089176EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 destination = ctx.gpr[4];
    const u32 object = word(rt, ctx.gpr[5] + 4u);
    ctx.gpr[16] = destination;
    if (object != 0u) {
        const u32 vtable = word(rt, object);
        call_virtual(rt, ctx, word(rt, vtable + 0x58u), vtable, word(rt, vtable + 0x58u),
                     {object, frame.at(0u), ctx.gpr[6]});
        put_float(rt, destination + 4u, float_at(rt, frame.at(4u)));
        put_float(rt, destination, float_at(rt, frame.at(0u)));
        put_float(rt, destination + 8u, float_at(rt, frame.at(8u)));
    } else {
        put(rt, destination, 0u);
        put(rt, destination + 4u, 0u);
        put(rt, destination + 8u, 0u);
    }
    ctx.gpr[2] = destination;
}

// 0x0892EE54 (a0 = this): 0x08936DBC(this) and 0x089363FC() run and 0x089378D4(answer, word(0x08D69524), this) follows; then the
// two-word arrays at 0x08CF297C and 0x08CF296C are cleared. v0 ends as 0x08CF2984 (the loop's upper bound).

// 0x0897E8DC (a1 = a holder, a2 = first, a3 = second): like 0x088152F8 with 0x0897E844 building into the stack word, the
// string 0x08CA3400 and the sub-object at object + 0xF0 answering 0x089833D0(first, second). v0 = the record's release.

// 0x08989764: the same with 0x089896CC, the string 0x08CA39F0 and 0x08983A18 on the sub-object at object + 0xF0.

// 0x08989824: the same with 0x089896CC, the string 0x08CA39F0 and 0x089833D0 on the sub-object at object + 0xF0.

// 0x089C66F4 (a0 = this, a1 = first, a2 = second): 0x089C6610(first, second, sp, sp + 4) fills two stack words; when it
// answers 0, 0x08A44960(this, 0x08CF6ED8) runs, otherwise 0x08A44D9C(this, answer, word(sp + 4), byte(sp)). v0 = this.

// 0x089C6AB4 (a0 = this): the vtable 0x08CCD338 is stored and the object at +0x240 released through its first virtual (unless
// it is the inline sub-object at +0x4D0); 0x08B22AF0(this + 0x208) and 0x08978B04(this) run, then 0x08A5BC54(this, 0x4D0, 4)
// finishes as a tail call.
void func_089C6AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 inline_member = self + 0x4D0u;
    ctx.gpr[16] = self;
    ctx.gpr[5] = inline_member;
    put(rt, self, 0x08CCD338u);
    const u32 member = self + 0x240u;
    if (member != 0u && member != inline_member) {
        const u32 object = word(rt, member);
        const u32 vtable = word(rt, object);
        call_virtual(rt, ctx, vtable, object, vtable, {member, inline_member});
    }
    call_full(rt, ctx, 0x08B22AF0u, {self + 0x208u});
    call_full(rt, ctx, 0x08978B04u, {self});
    tail_call(ctx, 0x08A5BC54u, {self, 0x4D0u, 4u});
}

// 0x08A9C70C (a0 = this, a1 = a key): the member at +0x4C is locked (0x08B344E0), the lookup 0x08C2FA50(this + 0xC, key)
// answers; unless it is this + 0x10, its word at +0x14 is the answer (0 otherwise), and the member is released
// (0x08B3457C). v0 = that answer.

// 0x08AC45F4: 0x08C1A1B8(0x08CFFD10) gives a count, count * 0x9A00 bytes are obtained (0x08B2B4D8(0x80, size)) and filled
// (0x08B2B760(0x08CFFCF4, buffer, size, 0x9A00)); 0x08B2EEB8(0x08CFFCB8) then 0x08C7ACD0(0x08CFFCB8) finish as a tail call.

// 0x08AEEE50 (a0 = this, a1 = a count, a2 = a value): 0x08AEEEF4(this, count, value) sets the scene, then the seven-argument
// 0x08AEED10(this, 1, count, value, 0, 0, 0) and 0x08AEEFD4(this, 1) finish. v0 = the last call's result.

// 0x08AF0910 (a0 = a value): every entry of the table at 0x08D41AD0 (count at +0x544, re-read each round, signed `slt`) takes
// the value through 0x08AF07C8(index, value). v0 = the table address when there is nothing to do, else the loop test's 0.

// 0x08AF4294 (a0 = an object, a1 = holder): the holder's object (word at +4, else the word at +8) runs its virtual at
// vtable + 8 with (self, object); without one, a record is built (0x08B4E648(sp)) and applied (0x08B4E734(self, sp)).
// v0 = self.
void func_08AF4294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    ctx.gpr[16] = self;
    u32 object = word(rt, holder + 4u);
    if (object == 0u) object = word(rt, holder + 8u);
    if (object != 0u) {
        const u32 vtable = word(rt, object);
        call_virtual(rt, ctx, word(rt, vtable + 8u), word(rt, vtable + 8u), vtable, {self, object});
    } else {
        call_full(rt, ctx, 0x08B4E648u, {frame.at(0u)});
        call_full(rt, ctx, 0x08B4E734u, {self, frame.at(0u)});
    }
    ctx.gpr[2] = self;
}

// 0x08B174D4: the table 0x08D6112C is prepared (0x08B1728C) and the globals 0x08D62B40 then 0x08D62B3C are released
// (0x08B1B99C) and cleared when present. v0 ends as the last value loaded (0) or as the second release's result.

// 0x08B180F4 (a0 = this): the virtual at vtable +0x1C runs with (this, 0), the 8-byte record at this + 0x1860 + index * 8
// (index at +0x1874) goes to 0x08B24D84(this, record + 4, record + 8), the index flips with 1 (re-read) and the virtual at
// vtable +0x18 finishes as a tail call.

// 0x08B319FC: an import status (record on the stack, filled by 0x08C7AEF0) maps to 3 on 0x80410B08, else 0 on any other
// failure; from the word it filled: 1, 2, 4 for other nonzero values, 0 for zero. v0 = that value.

// 0x08B6594C (a0 = an answer, a1 = an object, a2 = a value): the member at object + 0x6C is set up (0x08B5C450(answer,
// member)) and takes the value (0x08B5D1E0(member, value)), then the object is flagged (0x08B6534C(object, 1)). v0 = a0.

// 0x08B89238 (a0 = first, a1 = end, a2 = destination): every 32-byte element of [first, end) is handed to 0x0896418C as
// (destination, element) when the destination is not null, and the element's word at +0x10 is copied after it. v0 = the
// advanced destination.

// 0x08C66D44 (a0 = this, a1 = a five-byte record, a2, a3 = a signed byte, t0/t1 = the sixth and seventh arguments): the
// record {word at a1, byte at a1 + 4} is copied onto the stack and handed to 0x08C66844 as (this, copy, a2, sext(a3), 0x4C,
// t1, t0, t1). Afterwards the callee's v1 goes to sp + 0x14 and its low byte to sp + 4, which v1 returns (the three bytes
// already there are whatever the frame held).

// 0x08C6843C: the same call through 0x08C67F3C with a zero fifth argument.

// 0x08C7978C (a0 = destination, a1 = source object): the source's primary vtable goes to the destination with the word at
// source +0xC at the displacement the vtable names, the sub-object at destination + 8 is initialised (0x08C786B4) with the
// same word; the source's second vtable goes to the destination, +4 is cleared and the word at source + 8 sits at its
// displacement. v0 = the source's second vtable.

} // namespace
} // namespace new_batch_main_930
} // namespace gt::game

#define NEW930(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_930::name, gt::ReturnKind::kind)
NEW930(0x0880AEC4u, func_0880AEC4, Word);
NEW930(0x088E0B78u, func_088E0B78, Word);
NEW930(0x089176ECu, func_089176EC, Word);
NEW930(0x089C6AB4u, func_089C6AB4, Word);
NEW930(0x08AF4294u, func_08AF4294, Word);
