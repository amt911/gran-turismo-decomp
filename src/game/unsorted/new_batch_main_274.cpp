// ---- from main-session batch 175 (draft 71264b9dcdf804895b3b15af64e3c8d71d892f28) ----
// New batch 175 (main session): functions that no src/ file registers, written by hand from `tools/disasm` as
// structured C (named locals, `G[n]` only at the call boundary). Draft batch, not yet verified by the differential
// gate; the input shapes of its arguments live in config/synth_shapes.txt (or GT_SYNTH_SHAPES). Three shapes repeat:
// the destructor body (vtables stored, members released, a tail call), the scope body (a holder opened on the frame,
// its object asserted non-null, a getter's value handed to an action) and the two virtual-call dispatchers.
// `tools.sp_loads` lists 21 `lw $s0, 0x0($sp)` in the scope bodies: s0 is reused there as the opened object (the
// caller's s0 is restored from 0x10($sp) at the end), which the scope helper models as a local, so the flag is benign.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_175 {
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
// The same where the code after the call reads v1 / f0: those are the callee's, as after the `jal`.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// Saves the caller's callee-saved registers (16 = s0 ... 18 = s2) and restores them on scope exit: a callee may read
// them, and they are part of what the function leaves behind.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx) {
        for (const unsigned reg : registers) {
            indexes_[count_] = reg;
            values_[count_] = ctx.gpr[reg];
            ++count_;
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < count_; ++i) ctx_.gpr[indexes_[i]] = values_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned indexes_[8]{};
    u32 values_[8]{};
    unsigned count_{};
};

// The scope shape: open(&holder), the object it produced (a null one asserts with `message`), the getter's value, the
// holder released (0x08A5B9EC), then the action; v0 is the action's result.
void scope_action(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, u32 open, u32 message, u32 getter, u32 action) {
    call(rt, ctx, open, {frame.at(0u)});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[4] = message;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 value = call(rt, ctx, getter, {object});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call(rt, ctx, action, {value});
}

// 0x08ABB298: for every node of the global list 0x08D77184 (next at +0, object at +4) the object's vtable slot 0x64 is
// called with the object; then 0x08ABA360 as a tail call, a0 still the last object the walk read.

// 0x089BFD4C (a0 = this): the vtable words 0x08CCA568 at +0 and 0x08CCA7A8 at +0x38 are stored, the container at +0x2B0
// and the members at +0x264 / +0x22C released (0x08B28DC4, 0x08A5B9EC, 0x08A19DD4), then 0x089E2430(this) as a tail call.

// 0x0881800C: 0x08817FCC(scope), 0x08817A54(scope) -> 0x08817A0C; the assert message is 0x08C98D6C.

// 0x0882994C: 0x088297A8(scope), 0x088294D0(scope) -> 0x0885EEE0; the assert message is 0x08C9A2F8.

// 0x08829E58: the same scope and getter, action 0x0885F46C.

// 0x08829EB0: action 0x0885EBB0.

// 0x08829F08: action 0x0885E414.

// 0x08829F60: action 0x0885F460.

// 0x08829FB8: action 0x0885F444.

// 0x0882A010: action 0x0885EA48.

// 0x0882A068: action 0x0885E940.

// 0x0882A118: action 0x0885E8DC.

// 0x0882A170: action 0x0885E878.

// 0x0882A220: action 0x0885E7DC.

// 0x0882A480: action 0x0885E764.

// 0x0882A4D8: action 0x0886006C.

// 0x0882A588: action 0x088622B8.

// 0x0882A5E0: action 0x08863340.

// 0x08847580: 0x08847008(scope), 0x08846E50(scope) -> 0x088D5AFC; the assert message is 0x08C9CBBC.

// 0x08847958: the same scope and getter, action 0x088D4D24.

// 0x088479B0: action 0x088D59B8.

// 0x088548B0: 0x088547C4(scope), 0x08854678(scope) -> 0x088A719C; the assert message is 0x08C9E188.

// 0x08A1C3B0: 0x08A1C370(scope), 0x08A1C0F4(scope) -> 0x08A1C0FC; the assert message is 0x08CAA224.

// 0x08AC0388 (a0 = this, a1 = an argument): the vtable slots 0x0C (this, a1) and 0x10 (this, vtable) are called,
// 0x08B15B10(this->+8, this + 0x10, -1) runs, then the vtable slot 8 of this->+8 as a tail call with that object in a0.

// 0x08B0E560 (a0 = this, a1 = other): 0x08B34888(this + 4), other->+0x58 = this, 0x08B1FBEC(this + 0xF8, other + 0x5C),
// this->+0xA4 = 0, then 0x08B34B24(this + 4) as a tail call.

// 0x088D2C30 (a0 = this, a1 = an argument): the object at this->vtable[0x14C] answers its vtable slot 0x2C with
// (that object, a1); when the answer is non-null its slot 0x18 runs with (answer, a1) and its v0 is the result, else 0.

// 0x0899E24C (a0 = this): the vtable words 0x08CCB5F8 at +0 and 0x08CCB850 at +0x38 are stored, the members at +0x16C,
// +0x168, +0x164 and +0x160 released (0x08A5B9EC), then 0x089A29E4(this) as a tail call.

// 0x089C69FC (a0 = this): the vtable word 0x08CCD338 is stored; the object at +0x240, unless it is the one at +0x4D0,
// is released through its vtable's first word with (it, this + 0x4D0); 0x08B22AF0(this + 0x208) runs and
// 0x08978B04(this) finishes as a tail call.

// 0x0883D988 (a0 = destination, a1 = count, a2 = key): with a positive count, 0x0883B848(key, count) answers a pair that
// 0x08930DD8 turns into a value; the value is built at sp through 0x08A85B1C and assigned to the destination through
// 0x08A5BAD8 unless the destination is sp itself; sp is then released. A count of 0 or less leaves v0 untouched.

// 0x0887E6F0 (a0 = this): the vtable word 0x08CC1058 is stored; when the member at +0x74 is non-null it is released
// through its vtable's first word with (it, this->+0x70) and both words are cleared; then 0x0886A858(this) as a tail call.
} // namespace
} // namespace new_batch_main_175
} // namespace gt::game

#define NEW175(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_175::name, gt::ReturnKind::kind)

// ---- from main-session batch 176 (draft 43074e8dd581fdd2254ad14bfd68883b33e4c120) ----
// New batch 176 (main session): functions the gate never had useful inputs for. Written by hand from
// `tools/disasm` as structured C — a draft batch, not yet verified. Names are the ELF addresses: none
// has evidence. The helpers cover the shapes the batch repeats (a destructor that releases a held
// object before its base finishes, the range walker that hands every element to a release helper, the
// two-lock member teardown, the "apply a virtual's answer into the caller's reference" family).
//
// Delay slots are read as "before the branch / call": a `jal`'s delay slot runs before the callee, a
// branch-likely's only when taken. `v0` is left as the asm leaves it on every exit path, including the
// early ones (the register a caller reads after the call), and every argument register a callee reads
// is passed. Frames are the original prologues' sizes.
//
// Not in this batch: 0x08BB1534 (`j` to itself, an endless loop) and 0x08B35530 (`break 0`, a trap the
// host has no helper for) are not expressible as C, and 0x08B7B2E4 stores s0-s7/sp/fp/ra/f20-f31 into
// a record (a setjmp-shaped function, excluded by the batch rules).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_176 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

// A `jal`: afterwards `v0` holds the callee's result, as the instruction after the call sees it.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same, with the registers the callee leaves behind: `v0`/`v1`/`f0` and `$a1` (caller-saved, but a
// later call that does not pass `a1` hands the callee whatever the previous one left there).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target,
                      std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result;
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate (the disasm prints the
// unrelocated half).
constexpr u32 high_half(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
// Allegrex `wsbw`: the four bytes of a word reversed (these records are big-endian).
u32 byte_swapped(u32 value) { return __builtin_bswap32(value); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// Saves the caller's callee-saved registers selected by `mask` (bit n = s_n) and restores them on
// scope exit.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, u32 mask) : ctx_(ctx), mask_(mask) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i)
            if (mask_ & (1u << i)) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 mask_;
};

// --- destructors ---------------------------------------------------------------------------------------------------
// 0x0892BC00 (a0 = this): the vtable word 0x08CC6740 is stored, the object held at +0x264 is released through the first
// virtual of its vtable with the word at +0x260 as its argument, both words are cleared and the base at 0x08B22550 runs as
// a tail call.

// 0x08885F14 (a0 = this): destructor — the vtable word 0x08CC16E8, the base setup 0x0888598C, the object held at +0xC
// released twice (0x08885CEC and 0x08B64654) and the field cleared, then the base vtable word 0x08CBF428 (v0).

// 0x08809FDC and 0x0880BD5C (a0 = this): two vtable words are stored (at +0 and +0x38), the helper at `offset` is
// released through the first virtual of its vtable with the word at `offset` as its argument, both words are cleared and
// `tail` finishes with this as its only argument.
void release_held_then_base(Runtime &rt, AllegrexContext &ctx, u32 first_vtable, u32 offset, u32 second_vtable,
                            u32 tail) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, 3u);  // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self + offset;
        ctx.gpr[16] = self;
        put(rt, self, first_vtable);
        put(rt, self + 0x38u, second_vtable);
        ctx.gpr[2] = second_vtable;
        const u32 held = word(rt, self + offset + 4u);
        ctx.gpr[3] = held;
        if (held != 0u) {
            const u32 entry = word(rt, word(rt, held));
            ctx.gpr[2] = entry;
            call_indirect(rt, ctx, entry, {held, word(rt, self + offset)});
            put(rt, self + offset + 4u, 0u);
            put(rt, self + offset, 0u);
        }
    }
    tail_call(ctx, tail, {self});
}
void func_0880BD5C(Runtime &rt, AllegrexContext &ctx) {
    release_held_then_base(rt, ctx, 0x08CBA148u, 0xE8u, 0x08CBA380u, 0x0896BC0Cu);
}

// 0x08A1D6B8 (a0 = this): the vtable word is stored, the object at +0x18 is released through the first virtual of its
// vtable (its argument the word at +0x14), both words are cleared, 0x08A780B0(this) runs and 0x08A5BC54(this, 0x1C, 4)
// frees the object as the tail call.
void destroy_then_free(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, 3u);  // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x14u;
        put(rt, self, vtable);
        ctx.gpr[2] = vtable;
        const u32 held = word(rt, self + 0x18u);
        ctx.gpr[3] = held;
        if (held != 0u) {
            const u32 entry = word(rt, word(rt, held));
            ctx.gpr[2] = entry;
            call_indirect(rt, ctx, entry, {held, word(rt, self + 0x14u)});
            put(rt, self + 0x18u, 0u);
            put(rt, self + 0x14u, 0u);
        }
        call(rt, ctx, 0x08A780B0u, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x1Cu, 4u});
}

// 0x08BF8B64 (a0 = the list's sentinel): every node before the sentinel has the object at +8 released (0x08A5B9EC) and
// the node itself freed by 0x08A5BC54(node, 0xC, 4), until the walk returns to the sentinel. An empty list (the sentinel
// first) returns without touching anything, v0 left as it came in.

// --- containers -----------------------------------------------------------------------------------------------------
// 0x08C4B740 and 0x08C4B7A0 (a0 = a three-word container): its three words are taken (begin, end, capacity), the
// container is emptied, the words go to a stack triple (sp+4, sp+8, sp+0xC) and `target` runs with
// (begin, end, sp, end, capacity); the word at sp+4 the call leaves behind is released by 0x08B35254 when it is set.
// v0 is that call's result, or the word at sp+4 when it stayed.
void drain_container(Runtime &rt, AllegrexContext &ctx, u32 target) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 second = word(rt, self + 4u);
    const u32 first = word(rt, self);
    const u32 third = word(rt, self + 8u);
    put(rt, self + 8u, 0u);
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, frame.at(0xCu), third);
    put(rt, frame.at(4u), first);
    put(rt, frame.at(8u), second);
    ctx.gpr[2] = self;
    call(rt, ctx, target, {first, second, frame.at(0u), second, third});
    const u32 held = word(rt, frame.at(4u));
    ctx.gpr[2] = held;
    if (held != 0u) call(rt, ctx, 0x08B35254u, {held});
}

// 0x08C026FC, 0x08C25298 and 0x08C30A6C (a0 = first, a1 = end, a2 = destination): every element of [a0, a1) — `stride`
// bytes each — is handed to `release` as (destination, element) when the destination is not null, both pointers advancing
// together. v0 ends as the advanced destination (a2 itself when the range is empty).
void copy_elements(Runtime &rt, AllegrexContext &ctx, u32 stride, u32 release) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    SavedRegisters saved(ctx, 7u);  // s0, s1, s2
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = end;
    if (begin != end) {
        u32 source = begin;
        do {
            ctx.gpr[16] = source + stride;  // s0, advanced before the destination is tested
            ctx.gpr[17] = destination;
            if (destination != 0u) call(rt, ctx, release, {destination, source});
            source += stride;
            destination += stride;
        } while (source != end);
    }
    ctx.gpr[2] = destination;
}

// --- members locked, used and released -------------------------------------------------------------------------------
// 0x089BFB70 (a0 = this): 0x089E0288(this) first, then the block at +0x2B0 is locked (0x08B344E0); with the word at
// +0x264 zero the record at +0x22C is moved to the stack by 0x089C5680(sp, this + 0x22C), the moved-to reference is
// updated by 0x08A5BAD8(this + 0x264, sp[0]) when it is not the temporary, and the temporary is released
// (0x08A5B9EC(sp)). The block is released last (0x08B3457C).

// 0x0885D010 (a0 = this): the member at +8 is locked (0x08B344E0); the byte at vtable +0x17 picks 0x0885BF5C(this) or
// 0x0885CF00(this), the member is released (0x08B3457C) and the pick's result is v0.

// --- the property-bag family: a virtual's answer into the caller's reference -----------------------------------------
// 0x08A78E6C, 0x08A79040 and 0x08A7972C (a0 = the caller's reference, a1 = the holder): the holder's object
// (a1[0], null reports through 0x08A5BB7C(0x08CB0E38)) is asked through the virtual at `member`, the answer is wrapped
// into a temporary at sp by `constructor`, copied into the caller's reference by 0x08A5BAD8 when that is not the
// temporary, and the temporary is released (0x08A5B9EC(sp)); v0 is the release's result.
void assign_member_from_holder(Runtime &rt, AllegrexContext &ctx, u32 member, u32 constructor) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + member), {object});
    call(rt, ctx, constructor, {frame.at(0u), value});
    if (self != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- small actions -----------------------------------------------------------------------------------------------------
// 0x088152F8 (a0 unused, a1 = a holder, a2 = first, a3 = second): the construction 0x0880F0C0(sp, holder) writes a new
// object to the stack (null is an error: the string 0x08C986F4); the object's owned member at +0xE8 then takes
// (first, second) through 0x08813284, and the record is released (0x08A5B9EC(sp)). v0 is that release's result.

// 0x089B17B8 (a0 = this): 0x089B17AC(this) then five members are reset (0x089B0044, 0x089AB008, 0x089B05F8,
// 0x089B08E4, 0x089AFF3C), each with a zero argument, and 0x089B0284(this, 0) finishes as the tail call.

// 0x08B180F4 (a0 = this): the first virtual (vtable +0x1C) runs with a1 = 0, the record at this + 0x1860 + index * 8
// (index at +0x1874) goes to 0x08B24D84(this, second, third), the index flips between 0 and 1, and the second virtual
// (vtable +0x18) finishes with this as the tail call.

// 0x08B09448 (a0 stored in the record at sp + 0xC): a four-word record {0, 0x2000, 0x08B098A8, a0} is built on the
// stack, 0x08B2E314(3, -6) and 0x08B2E134 feed 0x08B2E7DC(record + 8, record, size), and its result is stored to the
// global 0x08D4744C and handed to 0x08B2E214 (the store happens before that call). v0 = the last call's result.

// 0x0892EE54: 0x08936DBC and 0x089363FC set the table up and 0x089378D4(answer, word(0x08D69524), this) runs; then the
// two register arrays at 0x08CF296C and 0x08CF297C (2 words each) are cleared. v0 ends as the loop's bound 0x08CF2984.

// 0x08AC45F4: 0x08C1A1B8(0x08CFFD10) gives a count, count * 0x9A00 bytes are obtained (0x08B2B4D8(0x80, size)) and filled
// (0x08B2B760(0x08CFFCF4, buffer, size, 0x9A00)); 0x08B2EEB8(0x08CFFCB8) then 0x08C7ACD0(0x08CFFCB8) finish as the tail
// call.

// 0x08B58E84 (a0 = an object or 0): the object is a0, or the one the graphics state keeps at 0x08D6CE00 + 0x2D4 when a0
// is null (s0 holds the `lui` half until the add that sits in the first call's delay slot). 0x08C7AAD8() and
// 0x08C7AC48(0) run, then 0x08C7AC40(object + 8, 0, word(0x08D6CE00 + 0x30), 0) answers the word stored at
// 0x08D6CE00 + 0x34; v0 = that answer.

// 0x08B098A8 (a0 = a handle): while the word 0x08D4744C is non-zero, 0x08B2EFDC(-1, -1) runs; when the word is still
// non-zero the handle goes to 0x08B09824, and the loop repeats. v0 is the word that ended the loop (0 on every exit).

// 0x08978080 (a0 unused, a1 = a count, a2 = the holder): with count > 0 the object at a2[0] (null reports through
// 0x08A5BB7C(0x08CA2F84)) is asked for its float with the virtual at vtable +0x54 and a0 = trunc(f0 * 60.0f);
// otherwise a0 = 1. It ends in the tail jump 0x089C4558(a0).

// 0x08B0B7DC (a0 = a record, a1 = a callback): with a callback, the count at +0xC (big-endian: `wsbw` reverses it) words
// of the array at +0x24, 0x10 bytes apart, are handed to the callback one by one, each byte-swapped (`wsbw`);
// v0 is the raw count word when the count is not positive, the last callback's result otherwise, and untouched when
// there is no callback.
void func_08B0B7DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 callback = ctx.gpr[5];
    SavedRegisters saved(ctx, 15u);  // s0, s1, s2, s3
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = callback;
    if (callback == 0u) return;
    const u32 raw = word(rt, self + 0xCu);
    ctx.gpr[2] = raw;
    const u32 count = byte_swapped(raw);
    ctx.gpr[18] = count;
    if (static_cast<s32>(count) <= 0) return;
    u32 index = 0u;
    u32 address = self + 0x24u;
    for (;;) {
        const u32 value = byte_swapped(word(rt, address));
        ++index;
        address += 0x10u;
        ctx.gpr[16] = address;
        ctx.gpr[17] = index;
        call_indirect(rt, ctx, callback, {value});
        if (index == count) break;
    }
}

// 0x08B56270 (a0 = a matrix holder, f12 / f13 / f14 = three scales): the matrix is the word at
// a0 + 0xD00 + 4 * word(a0 + 0xD10); its column C020 is scaled by f12, C030 by f13 and C010 by f14, and the four words
// at +0xD18..+0xD24 are ORed in pairs (v0 = the first pair, v1 = the second). No frame.

} // namespace
} // namespace new_batch_main_176
} // namespace gt::game

#define NEW176(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_176::name, gt::ReturnKind::kind)
NEW176(0x0880BD5Cu, func_0880BD5C, Word);
NEW176(0x08B0B7DCu, func_08B0B7DC, Word);

// ---- from main-session batch 177 (draft 7b2fd177f629c05fe261b4fc7a1584b750208041) ----
// New batch main 177: 27 functions the extra seeds made emittable, hand-written as structured C from `tools.disasm`
// (named locals; `G[n]` only at the call boundary, every callee scratch register set where the asm sets it and `v0`
// left as the asm leaves it on every path). Every function here was drafted once and never compared, so the batch
// also writes the input shapes the synthetic generator needs (one line per address that dereferences an argument
// register or a global; see /tmp/claude-1000/shapes_main177.txt):
//  - the object reporters 08AA22BC / 08AA2494 and the five 08AA35xx / 08AA37D4 / 08AA3Cxx: a holder's object answers
//    through vtable slot 0x54 (0x3C for 08AA2494), the answer is transformed by 0x08B66524 and friends (float) or
//    reported as a word, built into a stack temporary and handed to the caller's holder;
//  - the boxed-value reporters 0896DCD4 / 089740A4 / 089C3A14 / 089FDC8C / 089FFCDC: a constructor fills a stack
//    temporary, one build call reports a word / float field of the object it leaves there, then 0x08A5BAD8 /
//    0x08A5B9EC finish;
//  - the rest are single-purpose leaves: start-up sequences, table lookups and teardown chains.
// No names: none of these has evidence. Skipped (as in the earlier drafts): 0x08B7B34C and 0x08B17104 (a longjmp and
// its constructor: they reload s0-s7 / sp / fp / ra out of a saved block) and 0x08B54104 (a VFPU ray computation).
// Audits: `tools.sp_loads` lists the six `lw s0, 0x0/0x4($sp)` of the reporters — they are not saves being reloaded,
// they read the object a constructor just left on the stack, modelled by `word(rt, object_slot)`.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_177 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 / v1 / f0 of a callee are what the next instruction sees (the harness compares v0 only for a "Word" function, but
// a callee reads the registers a caller leaves, so the local state has to match too).
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// `jalr` through a register that is not v0: the callee starts with whatever v0 the caller already holds.
inline u32 call_through(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, target, arguments);
}
// `lw v0, slot(vtable); jalr v0` on an object: v1 = the table, v0 = the slot (the callee starts with v0 = its own
// address, which a virtual that never writes v0 returns).
inline GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                                std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments, {});
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// s0-s7, fp and ra are callee-saved: the body sets them where the asm sets them (a callee may read them) and the
// caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// --- the object reporters ---

enum class Report { Word, Float };

// a1 = a holder whose first word is the object (reported through 0x08A5BB7C(message) when null); slot `slot` of the
// object's vtable answers, the answer is built into a stack temporary by 0x08A85B1C (word, a1) or 0x08A87EEC (float,
// $f12) and handed to `out` by `finish`, then the temporary is released; the asm leaves v0 = out.
void report_holder(Runtime &rt, AllegrexContext &ctx, u32 slot, Report reported, u32 message, u32 finish) {
    const u32 out = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = out;
    const u32 holder = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult answer = call_virtual(rt, ctx, holder, slot, {holder});
    if (reported == Report::Float) {
        call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {answer.f0});
    } else {
        call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), answer.v0});
    }
    call_set(rt, ctx, finish, {out, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = out;
}

// a2 = a holder whose first word is the object (reported through 0x08A5BB7C(0x08CB2BB8) when null); vtable slot 0x54
// answers a float, the `transform` function (0x08B66524 and friends) takes it in $f12 and answers in $f0, that answer
// is reported by 0x08A87EEC into a stack temporary, copied to `out` when out is not the temporary itself and the
// temporary is released; v0 is the last call's answer, as in the asm.
void report_transformed_float(Runtime &rt, AllegrexContext &ctx, u32 transform) {
    const u32 out = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = out;
    const u32 holder = word(rt, ctx.gpr[6]);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2BB8u});
    const GuestResult answer = call_virtual(rt, ctx, holder, 0x54u, {holder});
    ctx.fpr[12] = answer.f0;  // the transform call's delay slot
    const GuestResult transformed = call_setf(rt, ctx, transform, {}, {answer.f0});
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {transformed.f0});
    if (out != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// --- the boxed-value reporters ---

enum class Reported { Word, Float };

// A constructor (`constructor`) fills a stack temporary; the object it leaves there (`object_below`: at sp, its value
// at sp+4; otherwise at sp+4, its value at sp) is reported through 0x08A85B1C (word) or 0x08A87EEC (float, the field at
// `offset`); a null object goes through 0x08A5BB7C(error_text). The value is moved into the caller's holder unless
// the holder is the temporary itself (0x08A5BAD8) and both temporaries are released (0x08A5B9EC), the value first.
void report_value(Runtime &rt, AllegrexContext &ctx, u32 constructor, u32 error_text, Reported reported, u32 offset,
                  bool object_below) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 object_slot = frame.at(object_below ? 0u : 4u);
    const u32 value_slot = frame.at(object_below ? 4u : 0u);
    call_set(rt, ctx, constructor, {object_slot});
    const u32 object = word(rt, object_slot);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {error_text});
    if (reported == Reported::Word) {
        call_set(rt, ctx, 0x08A85B1Cu, {value_slot, word(rt, object + offset)});
    } else {
        call_setf(rt, ctx, 0x08A87EECu, {value_slot}, {float_at(rt, object + offset)});
    }
    if (holder != value_slot) call_set(rt, ctx, 0x08A5BAD8u, {holder, word(rt, value_slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {value_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {object_slot});
}

// 0x08AA22BC (a0 = out, a1 = a holder): slot 0x54 of the holder's object answers a float, reported through
// 0x08A87EEC and handed to out by 0x08A76ABC; v0 = out.
void func_08AA22BC(Runtime &rt, AllegrexContext &ctx) {
    report_holder(rt, ctx, 0x54u, Report::Float, 0x08CB2B50u, 0x08A76ABCu);
}

// 0x08AA2494 (a0 = out, a1 = a holder): slot 0x3C answers a word, reported through 0x08A85B1C and handed to out by
// 0x08A76ABC; v0 = out.
void func_08AA2494(Runtime &rt, AllegrexContext &ctx) {
    report_holder(rt, ctx, 0x3Cu, Report::Word, 0x08CB2B50u, 0x08A76ABCu);
}

// 0x08B2DEE4: 0x08C7B008(sp) and 0x08C7B010(sp+0x10, sp+8) gate the body — a nonzero answer either time returns -1 in
// v0 and v1; 0x08C7B000(sp, sp+8) runs in between and 0x08B2DE94(sp+0x10) finishes, its 64-bit answer returned.

// 0x08B62498 (a0 = a block + 0x50, a1 / a2 = two words): fills the exception record at a0 - 0x50 (vtable 0x08B62450 at
// +0x38, the two words, the globals 0x08CE0E08 / 0x08CE0E54 and the constants 0x432B2B00 / 0x474E5543 at +0x30), then
// calls 0x08B758A4(record + 0x30, a1, a2, record), 0x08B62198(record + 0x30) and 0x08B6459C(). The last one does not
// return: the original has no epilogue.

// 0x0885E940 (a0 = this): ten steps on the object's word + 0x5AC, each `setter` taking the previous `getter`'s answer
// in a0, then 0x08866FA8(*(this)) finishes: 0x088D6E9C(*(this)[0x5AC]), 0x088CBFD4, 0x088CBE6C(v0), 0x088CA548,
// 0x088C9780(v0), 0x08916474, 0x08916194(v0), 0x088C7F64, 0x088C78B4(v0), 0x08B1C47C. The calls whose a0 the asm does
// not set pass the previous answer, the closest model of a scratch register the harness does not replay.

// 0x08AC2BD8: the three registered static teardown slots (0x08CFFCA8, 0x08CFFCA4, 0x08CFFCA0). The first one's
// function pointer (with its word +4 in a0) runs; when the second is set 0x08AC2BAC(second) finishes (a tail call);
// otherwise the third object's vtable slot 0x58 finishes (with the object in a0).

// 0x08B75204 (a0 = table, a1 = state): 0x08B7450C(table) runs first; the index at state + 0x9BC, while it is below
// 0x133 and the byte table 0x08D72BB4 selects kind 4, picks an entry of the table (a0 + index * 4) whose low bit is
// cleared and stored both in it and at table + 0x4D0; every other index goes on in 0x08B77B04, which does not return.

// 0x08805C70: the start-up sequence 0x08B2DBF0(0x14D), FPU control 31 cleared, then 0x08AD0974, 0x08AD0900,
// 0x08B33218, 0x08AD08A4, 0x08AD0820, 0x08B2DD2C, 0x08B33688, 0x08B34060, 0x08AD083C, 0x08B0EBF0 and 0x08AB9B04;
// 0x08B1F178 finishes (a tail call). The calls after the first do not set a0, so they keep the value 0x14D the caller
// put there.

// 0x08AA3568 (a0 = out, a2 = a holder): slot 0x54 of the holder's object answers a float, 0x08B66524 transforms it
// ($f12), 0x08A87EEC reports the answer into a stack temporary, 0x08A5BAD8 copies it to out (unless out is the
// temporary) and 0x08A5B9EC releases it; v0 is the last call's answer.

// 0x08AA3758: the same as 0x08AA3568 with 0x08B66D54 in place of 0x08B66524.
void func_08AA3758(Runtime &rt, AllegrexContext &ctx) {
    report_transformed_float(rt, ctx, 0x08B66D54u);
}

// 0x08AA37D4: the same as 0x08AA3568 with 0x08B65F0C in place of 0x08B66524.

// 0x08AA3CD8: the same as 0x08AA3568 with 0x08B6759C in place of 0x08B66524.
void func_08AA3CD8(Runtime &rt, AllegrexContext &ctx) {
    report_transformed_float(rt, ctx, 0x08B6759Cu);
}

// 0x08AA3E4C: the same as 0x08AA3568 with 0x08B664A0 in place of 0x08B66524.
void func_08AA3E4C(Runtime &rt, AllegrexContext &ctx) {
    report_transformed_float(rt, ctx, 0x08B664A0u);
}

// 0x08B33218: 0x08C7ABE8(1) and 0x08C7ABF8(0) run, then the one-time block at 0x08D691D8: when its word +8 is zero,
// the record {0x08B33534, &block} is built at sp, its byte +0xC is cleared, 0x08B2E314(3, 0), 0x08B2E134(answer) and
// 0x08B1BA54(&block, answer) run (the answer is kept at +8) and 0x08B2E214(answer) finishes.

// 0x08B5D6B0 (a0 = out, a1 = state, a2 = a value): the index 0x08B5C2B0(value) must stay below the count at state + 8
// minus one and select a nonzero entry of the table at *(state + 4); 0x08B5D3DC(out, value, entry) finishes (a tail
// call). Every invalid index goes on in 0x08B5AFDC(0x08CB57AC), which does not return.

// 0x08864A74 (a0 = object): a teardown — 0x088D9B1C on the word at +0x5AC of the object's first word with a1 = 1,
// then four calls on the object, then 0x0885F428 on it (a tail call).

// 0x0889D6B8 (a0 = object, a1 = argument): calls every (function, argument) pair of the 8-byte entries of the array at
// +0x28 of the object a0 points at — the count at +0x30 is re-read after each call and the array base too — then
// 0x08B3457C on it (a tail call).

// 0x08918068 (a0 = array of 4-byte entries, a1 = index): the entry at a0 + a1 * 4 + 8 is looked up twice through
// 0x089277BC (a1 = the entry's address the first time, 0 after). When the first lookup answers 0 and the object
// 0x08918040(array, 0) returns has a null word at +0x9C the answer is 1; otherwise the answer is 1 only when the second
// lookup answers 0 and 0x0891FF94(entry) answers non-zero.

// 0x0896DCD4 (a0 = the caller's holder): 0x08969244 builds the object at sp + 4, its word at +0x8C is reported as a
// word, the value is moved to the holder unless it is the temporary, and both temporaries are released.

// 0x089740A4: the same shape as 0x0896DCD4 with 0x08973EF8, 0x08CA2BF0 and the word at +0x18.

// 0x08991E44 (a0 = object): releases the chain the object's word at +0x84 heads (each node through the virtual at
// +0x19C of its own vtable, the next one at its +0x54), then asks the object's virtual at +0x1A4 and stores the word at
// +0x10 of the object's +0x78 area when it answered non-zero, the word at (+0x78)+0xC otherwise, into +0x9C.

// 0x089B22A4 (a0 = object, a1 = value): 0x089AB800 on the object, then three calls on the pair (+0x30, +0x18) — the
// value goes into the second one as a3 with a2 = 0 — then 0x089AB8A8 on the object (a tail call).

// 0x089C3A14: the same shape as 0x0896DCD4 with 0x089C39D4, 0x08CA62A0 and the float at +0x28 (0x08A87EEC).

// 0x089C5680 (a0 = object, a1 = value): builds a value at sp (0x08A12E38), passes it to the virtual at +0x1C of the
// object it holds (with a1 = value), reports the pair through 0x089C160C(object, sp), releases sp; v0 = a0.

// 0x089FDC8C: the same shape as 0x0896DCD4 with 0x089FDB7C, 0x08CA8818 and the word at +0x14, but the object is
// built at sp and the value at sp + 4 (that temporary is the one released first).

// 0x089FFCDC: the same shape as 0x0896DCD4 with 0x089FF65C, 0x08CA8A30 and the word at +0x80.

// 0x08A1369C (a0 = object, a1 = state): locks the object's +0x10 area with 0x08B34888; the state is stored into +0xC —
// always, unless it is 2 and one of the bytes at +0x40 and +0x41 is set — and v0 = 1 only when it was stored; unlock
// with 0x08B34B24.
} // namespace
} // namespace new_batch_main_177
} // namespace gt::game

#define NEWMAIN177(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_177::name, gt::ReturnKind::kind)
NEWMAIN177(0x08AA22BCu, func_08AA22BC, Word);
NEWMAIN177(0x08AA2494u, func_08AA2494, Word);
NEWMAIN177(0x08AA3758u, func_08AA3758, Word);
NEWMAIN177(0x08AA3CD8u, func_08AA3CD8, Word);
NEWMAIN177(0x08AA3E4Cu, func_08AA3E4C, Word);

// ---- from main-session batch 178 (draft fde014bc1bfd71e7030a01d0522f6c9be7f425ff) ----
// New batch main 178 (main session): the thirty functions of the two reported-object families that the earlier batch
// (new_batch_main_7 / hot_batch_197) had to leave unregistered — "no comparable synthetic case yet". They were
// unreachable by every real route, and the synthetic generator's random pointers made the original fault before any
// comparison, so neither a route nor a synthetic case could prove them. This batch is the same code rewritten with the
// input shapes the generator needs (listed at the bottom), which is what makes a case comparable at all.
//
// Both families were decoded instruction by instruction from `tools/disasm` against the landed, gate-passing members of
// the same families (new_batch_main_7.cpp), so the helper below is the accepted shape and only the per-function columns
// (constructor, message, report call, field, slot, operator) come from each function's own words.
//  - the *reporters* (10): a constructor fills a 0x20-byte stack temporary, its object is read from that slot (a null
//    object first goes through 0x08A5BB7C with the function's message), one build call reports a value read from the
//    object (a word/half/byte field, the object itself, an address into it, or a float in $f12 0x08A87EEC takes), the
//    result is moved into the caller's holder (a0) when the holder is not that temporary, and both temporaries are
//    released with 0x08A5B9EC — the one built first released last.
//  - the *boxed binary functors* (20): a field at +0x14 of a0, the object word(a3) points at answered through the
//    virtual at a slot of its vtable (0x34..0x4C), an operator over field and answer (add, sub, and/or/xor, the two
//    shifts, signed/unsigned compares) and the same temporary reporting, this time always at sp.
// v0 at every exit is the last call's result (0x08A5B9EC on the second temporary), as the asm's last `jal` leaves it.
// No names: none of these has evidence. A handler pointer in an Adhoc method table (0x08B87108) is only a reference.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_178 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The shared calls of both families: 0x08A5B9EC releases a temporary, 0x08A5BAD8 moves a temporary's word into the
// caller's holder, 0x08A5BB7C reports the function's message when the built object came back null.
constexpr u32 kRelease = 0x08A5B9ECu;
constexpr u32 kMove = 0x08A5BAD8u;
constexpr u32 kNullObject = 0x08A5BB7Cu;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee that takes its value in $f12 (the float reporter's 0x08A87EEC).
u32 call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, float value) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments, {value}).v0;
    return ctx.gpr[2];
}

// --- the reporters ---

enum class Reported { Word, UnsignedHalf, UnsignedByte, Object, Address, Float };

// The reporting shape every function of the family has: a 0x20-byte frame, a constructor on a temporary (`object_below`
// false: it builds at sp+4 and the report builds at sp — true: the constructor builds at sp and the report at sp+4),
// the object read back from that slot, one build call with the value, then 0x08A5BAD8 for the caller's holder a0
// (unless the holder *is* the temporary: `beq a0, sp+4`) and 0x08A5B9EC on both temporaries, the first-built one last.
void report_value(Runtime &rt, AllegrexContext &ctx, u32 constructor, u32 error_text, u32 report, Reported reported,
                  u32 offset, bool object_below) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 object_slot = frame.at(object_below ? 0u : 4u);
    const u32 value_slot = frame.at(object_below ? 4u : 0u);
    call(rt, ctx, constructor, {object_slot});
    const u32 object = word(rt, object_slot);
    if (object == 0u) call(rt, ctx, kNullObject, {error_text});
    switch (reported) {
    case Reported::Word: call(rt, ctx, report, {value_slot, word(rt, object + offset)}); break;
    case Reported::UnsignedHalf: call(rt, ctx, report, {value_slot, rt.memory().load16(object + offset)}); break;
    case Reported::UnsignedByte: call(rt, ctx, report, {value_slot, rt.memory().load8(object + offset)}); break;
    case Reported::Object: call(rt, ctx, report, {value_slot, object}); break;
    case Reported::Address: call(rt, ctx, report, {value_slot, object + offset}); break;
    case Reported::Float: call_float(rt, ctx, report, {value_slot}, float_at(rt, object + offset)); break;
    }
    if (holder != value_slot) call(rt, ctx, kMove, {holder, word(rt, value_slot)});
    call(rt, ctx, kRelease, {value_slot});
    call(rt, ctx, kRelease, {object_slot});
}


// --- the boxed binary functors ---

enum class Operand { Word, SignedHalf, SignedByte, UnsignedHalf, UnsignedByte };
enum class Binary {
    Add,
    Subtract,
    Xor,
    And,
    Or,
    ShiftLeft,
    ShiftRight,
    ShiftRightArithmetic,
    ValueBelowField,
    FieldBelowValue,
    ValueSignedBelowField,
    FieldSignedBelowValue,
};

// The field at +0x14 of a0, loaded with the width and signedness the function's own instruction uses (lw / lh / lb /
// lhu / lbu): a sign-extending load is read back as the signed value it is, as the register holds it.
u32 operand_at(Runtime &rt, u32 address, Operand operand) {
    switch (operand) {
    case Operand::Word: return word(rt, address);
    case Operand::SignedHalf:
        return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
    case Operand::SignedByte:
        return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
    case Operand::UnsignedHalf: return rt.memory().load16(address);
    case Operand::UnsignedByte: return rt.memory().load8(address);
    }
    return 0u;
}

// `field` is the loaded +0x14 word, `value` what the virtual answered. The shifts are the v-instructions': the count is
// the answer's low five bits, and srav shifts the field arithmetically. The compares are sltu / slt exactly as the asm
// orders them (`sltu a1, v0, s2` is value < field, `slt a1, s2, v0` is field < value).
u32 apply_binary(Binary binary, u32 field, u32 value) {
    switch (binary) {
    case Binary::Add: return field + value;
    case Binary::Subtract: return field - value;
    case Binary::Xor: return field ^ value;
    case Binary::And: return field & value;
    case Binary::Or: return field | value;
    case Binary::ShiftLeft: return field << (value & 31u);
    case Binary::ShiftRight: return field >> (value & 31u);
    case Binary::ShiftRightArithmetic: return static_cast<u32>(static_cast<s32>(field) >> (value & 31u));
    case Binary::ValueBelowField: return value < field ? 1u : 0u;
    case Binary::FieldBelowValue: return field < value ? 1u : 0u;
    case Binary::ValueSignedBelowField:
        return static_cast<s32>(value) < static_cast<s32>(field) ? 1u : 0u;
    case Binary::FieldSignedBelowValue:
        return static_cast<s32>(field) < static_cast<s32>(value) ? 1u : 0u;
    }
    return 0u;
}

// The functor shape: the 0x20-byte frame, the object word(a3) read first (it lands in s0), the field at +0x14 of a0 in
// s2, the message address left in a0 for the null-object report, then the virtual at `slot` of the object's vtable
// called with that object (0x...8: `lw v1, 0(s0); lw v0, slot(v1); jalr v0`), the operator's result reported through
// the temporary at sp, the caller's holder a1 moved from it and the temporary released.
void boxed_binary(Runtime &rt, AllegrexContext &ctx, Operand operand, u32 slot, Binary binary, u32 report, u32 error_text) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 box = word(rt, ctx.gpr[7]);
    const u32 field = operand_at(rt, self + 0x14u, operand);
    if (box == 0u) call(rt, ctx, kNullObject, {error_text});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, box) + slot), {box});
    call(rt, ctx, report, {frame.at(0), apply_binary(binary, field, value)});
    if (holder != frame.at(0)) call(rt, ctx, kMove, {holder, word(rt, frame.at(0))});
    call(rt, ctx, kRelease, {frame.at(0)});
}

void func_08A6081C(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::Word, 0x4Cu, Binary::ShiftRight, 0x08A5FA8Cu, 0x08CB00F8u);
}
void func_08A73628(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedHalf, 0x38u, Binary::Subtract, 0x08A85B1Cu, 0x08CB0BA8u);
}
void func_08A73A4C(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedHalf, 0x38u, Binary::ValueSignedBelowField, 0x08A97A6Cu, 0x08CB0BA8u);
}
void func_08A74084(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedHalf, 0x4Cu, Binary::ShiftRightArithmetic, 0x08A85B1Cu, 0x08CB0BA8u);
}
void func_08A967C0(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedByte, 0x34u, Binary::ValueSignedBelowField, 0x08A97A6Cu, 0x08CB21E8u);
}
void func_08A96DF8(Runtime &rt, AllegrexContext &ctx) {
    boxed_binary(rt, ctx, Operand::SignedByte, 0x4Cu, Binary::ShiftRightArithmetic, 0x08A85B1Cu, 0x08CB21E8u);
}

// Input shapes for the synthetic generator (config/synth_shapes.txt, one line per address; the generator takes one
// `reg:` per function, so the register whose dereference decides whether the original returns at all is the one laid
// out). Every function of this file stores through a0 — the reporters always (0x08A5BAD8 when the holder is not the
// temporary), the functors when a1 is not the temporary at sp — and the functors also read word(a0 + 0x14); but the
// register that has to be an object is a3 of the functors, whose word(a3) is read on every path:
//   0x08A39C4C reg:a0=struct:16   # a0 is the caller's holder; only written (0x08A5BAD8), then compared with sp+4
//   0x08A3B160 reg:a0=struct:16   # same (float reporter, 0x08A87EEC takes the value in $f12)
//   0x08A3F9D8 reg:a0=struct:16   # same, temporary at sp+4
//   0x08A3FA58 reg:a0=struct:16   # same, temporary at sp+4
//   0x08A3FC30 reg:a0=struct:16   # same; reports object + 0x108
//   0x08A4163C reg:a0=struct:16   # same, temporary at sp
//   0x08A417A4 reg:a0=struct:16   # same, temporary at sp
//   0x08A611C0 reg:a0=struct:16   # same, temporary at sp+4
//   0x08A9922C reg:a0=struct:16   # same, temporary at sp+4
//   0x08A992AC reg:a0=struct:16   # same, temporary at sp+4
//   0x08A5FCE8 reg:a3=struct:16 field:+0x0=ptr   # lw s0,0(a3) is the object whose vtable slot 0x4C answers
//   0x08A601EC reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A6071C reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A6079C reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A6081C reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A735A8 reg:a3=struct:16 field:+0x0=ptr   # slot 0x38
//   0x08A73628 reg:a3=struct:16 field:+0x0=ptr   # slot 0x38
//   0x08A73A4C reg:a3=struct:16 field:+0x0=ptr   # slot 0x38
//   0x08A73B50 reg:a3=struct:16 field:+0x0=ptr   # slot 0x38
//   0x08A74004 reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A74084 reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A86748 reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A86848 reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A9639C reg:a3=struct:16 field:+0x0=ptr   # slot 0x34
//   0x08A967C0 reg:a3=struct:16 field:+0x0=ptr   # slot 0x34
//   0x08A96CF8 reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A96D78 reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08A96DF8 reg:a3=struct:16 field:+0x0=ptr   # slot 0x4C
//   0x08AB0488 reg:a3=struct:16 field:+0x0=ptr   # slot 0x48
//   0x08AB0508 reg:a3=struct:16 field:+0x0=ptr   # slot 0x48
} // namespace
} // namespace new_batch_main_178
} // namespace gt::game

#define NEWMAIN178(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_178::name, gt::ReturnKind::kind)
NEWMAIN178(0x08A6081Cu, func_08A6081C, Word);
NEWMAIN178(0x08A73628u, func_08A73628, Word);
NEWMAIN178(0x08A73A4Cu, func_08A73A4C, Word);
NEWMAIN178(0x08A74084u, func_08A74084, Word);
NEWMAIN178(0x08A967C0u, func_08A967C0, Word);
NEWMAIN178(0x08A96DF8u, func_08A96DF8, Word);

// ---- from main-session batch 179 (draft 968a7bdfed063d66cee1c8201b3cd634de0af37f) ----
// New batch main 179: functions the extra seeds made emittable, written by hand from `tools/disasm` as structured C. This
// is a draft batch — nothing here is verified yet; the synthetic cases (input shapes in shapes_main179.txt) and the
// differential gate come later. Families first, one-off shapes after:
//  - the *boxed operators*: the left operand is the half or byte at a0+0x14, the right one the virtual at slot 0x44..0x4C
//    of the object word(a3) points at (called with that object); an operator over both is boxed by 0x08A85B1C / 0x08A97A6C
//    into a stack holder, moved into the caller's holder unless that *is* the frame address, and the frame holder is
//    released. A null object reports the function's message first and is then read anyway, as the original does.
//  - the *insert-range helpers*: the low one or two bits of (end - start) are masked off, two segment moves run, the
//    masked size is taken off the word at this+4, and v0 = start.
//  - the *element copy loop*: 0x28-byte elements with the vtable word 0x08CCE6E0 stored at each destination.
//  - one-offs: destructors that store two vtable words, release a member or a vector's storage and chain into a base
//    destructor, a value-to-bounds table pick, and a release loop over a container.
// Left out of the batch: 0x08B6250C, because it calls 0x08B75B04 — the setjmp body that stores fp/s0-s7/ra and f0-f31
// into the record at head+0x30 — and its control flow (`j` back into the release path after a call that never returns in
// the original) cannot be written with confidence.
// No names: none of these has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_179 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee that takes its value in $f12; v0/v1/f0 stay as the callee left them.
u32 call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, float value) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {value});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// A `jalr` through a register that is not $v0 (`jalr $v1`, `jalr $s3`, ...): the callee starts with the $v0 the caller
// holds, which is *not* its own address — only `lw $v0, slot(...); jalr $v0` gives it that (call_indirect above).
u32 call_through(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// --- the boxed operators ---

// Which kind of field a0+0x14 holds (`lhu` or `lbu` in the original).
enum class Operand { UnsignedHalf, UnsignedByte };
// The operator applied to the field and the virtual's answer, each as its instruction does it: `subu`, `and`, `or`, `xor`,
// `sllv` (the shift count masked to five bits by the hardware), `sltu $a1, $v0, $s2` (value below field) and
// `sltu $a1, $s2, $v0` (field below value).
enum class Operator { Subtract, And, Or, Xor, ShiftLeft, ValueBelowField, FieldBelowValue };

u32 operand_at(Runtime &rt, u32 address, Operand operand) {
    return operand == Operand::UnsignedHalf ? rt.memory().load16(address) : rt.memory().load8(address);
}

u32 apply(Operator op, u32 field, u32 value) {
    switch (op) {
    case Operator::Subtract: return field - value;
    case Operator::And: return field & value;
    case Operator::Or: return field | value;
    case Operator::Xor: return field ^ value;
    case Operator::ShiftLeft: return field << (value & 31u);
    case Operator::ValueBelowField: return value < field ? 1u : 0u;
    case Operator::FieldBelowValue: return field < value ? 1u : 0u;
    }
    return 0u;
}

// The shape every member of the family has: a 0x20-byte frame; the field at a0+0x14 and the box word(a3) read before the
// null test; a null box reports `message` (and is read anyway); the box's vtable entry `slot` answers the right operand
// (a0 = the box, and v0 = the entry before the call as after `lw v0, slot($v1); jalr $v0`); `emit(frame, value)` boxes the
// result; the frame's holder is moved into the caller's (a1) unless it already is that address; the frame holder is
// released. v0 = the release's result.
void boxed_operator(Runtime &rt, AllegrexContext &ctx, Operand operand, u32 slot, Operator op, u32 emit, u32 message) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 box = word(rt, ctx.gpr[7]);
    const u32 field = operand_at(rt, self + 0x14u, operand);
    if (box == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, box) + slot), {box});
    call(rt, ctx, emit, {frame.at(0u), apply(op, field, value)});
    if (holder != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {holder, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

void func_08AB092C(Runtime &rt, AllegrexContext &ctx) {
    boxed_operator(rt, ctx, Operand::UnsignedHalf, 0x48u, Operator::ValueBelowField, 0x08A97A6Cu, 0x08CB3A78u);
}
void func_08AB0DE4(Runtime &rt, AllegrexContext &ctx) {
    boxed_operator(rt, ctx, Operand::UnsignedHalf, 0x4Cu, Operator::Or, 0x08A85B1Cu, 0x08CB3A78u);
}
void func_08AB0E64(Runtime &rt, AllegrexContext &ctx) {
    boxed_operator(rt, ctx, Operand::UnsignedHalf, 0x4Cu, Operator::Xor, 0x08A85B1Cu, 0x08CB3A78u);
}
void func_08AB1C8C(Runtime &rt, AllegrexContext &ctx) {
    boxed_operator(rt, ctx, Operand::UnsignedByte, 0x44u, Operator::Subtract, 0x08A85B1Cu, 0x08CB3AF0u);
}
void func_08AB20D8(Runtime &rt, AllegrexContext &ctx) {
    boxed_operator(rt, ctx, Operand::UnsignedByte, 0x44u, Operator::ValueBelowField, 0x08A97A6Cu, 0x08CB3AF0u);
}
void func_08AB2590(Runtime &rt, AllegrexContext &ctx) {
    boxed_operator(rt, ctx, Operand::UnsignedByte, 0x4Cu, Operator::Or, 0x08A85B1Cu, 0x08CB3AF0u);
}
void func_08AB2690(Runtime &rt, AllegrexContext &ctx) {
    boxed_operator(rt, ctx, Operand::UnsignedByte, 0x4Cu, Operator::ShiftLeft, 0x08A85B1Cu, 0x08CB3AF0u);
}

// --- the insert-range helpers ---

// `ins $s0, $zero, 0, N` clears the low N bits of (end - start); 0x08B79BAC(start, end, this->4 - end) moves the tail;
// `move(tail + start, this->4, sp)` moves the head; this->4 loses the masked size (the word is re-read after the first
// call, as the original reloads it); v0 = start.
void insert_range_move(Runtime &rt, AllegrexContext &ctx, u32 move, unsigned mask_bits) {
    const u32 self = ctx.gpr[4];
    const u32 start = ctx.gpr[5];
    const u32 end = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 masked = (end - start) & ~((1u << mask_bits) - 1u);
    const u32 tail = word(rt, self + 4u) - end;
    call(rt, ctx, 0x08B79BACu, {start, end, tail});
    call(rt, ctx, move, {tail + start, word(rt, self + 4u), frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - masked);
    ctx.gpr[2] = start;
}


// --- the element copy loop ---

// 0x08BE6CC4 (a0 = first source element, a1 = one past the last, a2 = destination): no frame of its own. Each 0x28-byte
// element is read into registers first (the two words at +4/+8, the seven floats) and then stored with the class's vtable
// word 0x08CCE6E0 at +0, so overlapping copies behave as the original's own load-then-store order. The destination is
// advanced in the branch's delay slot, i.e. only when the loop continues: a null destination therefore skips the first
// element and writes the following ones at 0x28, 0x50, ... as the original does. v0 = the destination of the element just
// handled (or the initial one when the ranges were empty).

// --- one-off shapes ---

// 0x08AC7488 (a0 = object): the object's lock released (0x08B344E0), the word at +0x2C asked through 0x08C7AB68(handle,
// &count) into a zeroed word; a non-zero answer sets the byte at +0x28 and leaves 0; otherwise a positive count below the
// signed word at +0x38 answers 1. Then 0x08B3457C(object) and v0 = the answer kept in s1.

// 0x08ACBF70 / 0x08ACBFF0 (a0 = object): gated by 0x08B319A4(0x08D69170) or 0x08B327AC(0x08D69170, 1) — the second is
// only asked when the first answered 0. 0x08ACB910(object, sp) then decides: non-zero puts the type word into sp+0x30 and
// answers what 0x08ACBC78(object, sp) answers, zero answers 0.
void describe_scene(Runtime &rt, AllegrexContext &ctx, u32 type) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    u32 result = 0u;
    if (call(rt, ctx, 0x08B319A4u, {0x08D69170u}) != 0u ||
        call(rt, ctx, 0x08B327ACu, {0x08D69170u, 1u}) != 0u) {
        if (call(rt, ctx, 0x08ACB910u, {self, frame.at(0u)}) != 0u) {
            put(rt, frame.at(0x30u), type);
            result = call(rt, ctx, 0x08ACBC78u, {self, frame.at(0u)});
        }
    }
    ctx.gpr[2] = result;
}


// 0x08B26D94 (a0 = array of 0x10-byte entries, a1 = count): for each of the first `count` entries whose first word is set,
// 0x08B26D0C(array, previous answer, index) runs and its answer becomes the next previous one (the index is already
// incremented when it is passed, the count compared unsigned). v0 = the last answer, -1 when none ran.

// 0x08B2DBF0 (a0 = a signed value): picks the (a0, a2) constant pair by the value's range — 0x14D/0xA6 above 0x14D and in
// [0x10B, 0x14D], 0x10A/0x85 in [0xDF, 0x10B), 0xDE/0x6F in [0xBF, 0xDF), 0xBE/0x5F below 0xBF — and calls
// 0x08C7AF70(first, value, second); a value at or below zero is passed as 1 instead. v0 = 1 when the answer was 0.

// 0x08B46904 (a0 = object, a1 = argument): the object's virtual at +0x14 (v0 holds the vtable before the call, as the
// original's `lw $v1, 0x14($v0); jalr $v1` leaves it), a 0x10-byte record from 0x08B2CB18(word(a0+0x28), 0xC, 0x10) filled by
// 0x08C52354(record, a1) when the allocation answered, 0x08B1FBEC(&a0->0x2C, record), then the object's virtual at +0x18
// as a tail call (v0 = the vtable again, t9 = the target).

// 0x08B89E7C (a0 = result object, a1 = argument, a2 = a slot whose word is an object): the object's vtable entry 0x4C is
// called with the object, then a1's own vtable entry 0x84 (read before the null test) with (result object, argument, the
// first answer). The second call is a `jalr $s3`, so v0 still holds the first answer when it starts. v0 = the result object.

// 0x08C79FA4 (a0 = object): a derived constructor: the two vtable words at +0 and +0xA8; the base constructor 0x08C786B4 on
// this+4; the class's base table 0x08CDC358 gives the vtable word (its +4) and the second word (its +8), stored at this and
// at this + word(first - 0xC); the interface vtable 0x08CDB088 at +0xA8, its constructor 0x08B6543C on +0xA8, then the base
// destructor 0x08B64654(object) as a tail call.

// 0x0886BC7C (a0 = object): 0x0886B2E0(object) answers the entry count (a tail call into 0x08B1FDB0(word(a0+4))); for each
// of the first `count` entries, 0x0886BC5C(object, index) answers the entry, whose 5th sub-entry (0x0886A9A0(entry, 5))
// gets 0x0886F89C(sub, 0.0f) when both are set. Then 0x0886B1A0(object) as a tail call. The count is compared unsigned and
// the index is incremented before the test, as the `bne` at the loop's end does.

// 0x088A35C8 (a0 = object): a derived destructor: the vtable word 0x08CC2800 at +0 and 0x08CC2898 at +4 (stored in the
// call's delay slot, before the base destructor runs); 0x088A322C(object); the member at +0x5C released through its own
// vtable slot 0 with +0x58 as the argument (both cleared after), then the class's base vtable words 0x08CC28F8 at +0 and
// 0x08CC28E0 at +4. v0 = 0x08CC28F8, the word the original ends with.

// 0x089A1298 / 0x089F86F8 (a0 = object): a destructor of an object with an embedded vector at +0xA8 (begin, finish, end
// of storage). The two vtable words at +0 and +0x38; the element range destroyed by 0x08BE5C0C(begin, finish, sp); the
// storage freed with 0x08A5BC54(begin, (end - begin) rounded down to the element size, 4) when begin is set; then
// 0x089D8D30(object), whose answer is the result. The size comes from `sra` then `sll`, i.e. the difference truncated to a
// multiple of the element size (8 or 0x40 bytes).
void destroy_with_vector(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 secondary, u32 destroy_range, unsigned element_shift) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, vtable);
    put(rt, self + 0x38u, secondary);
    call(rt, ctx, destroy_range, {word(rt, self + 0xA8u), word(rt, self + 0xACu), frame.at(0u)});
    const u32 begin = word(rt, self + 0xA8u);
    const u32 bytes = ((word(rt, self + 0xB0u) - begin) >> element_shift) << element_shift;
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
    call(rt, ctx, 0x089D8D30u, {self});
}


} // namespace
} // namespace new_batch_main_179
} // namespace gt::game

#define NEWMAIN179(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_179::name, gt::ReturnKind::kind)
// The boxed operators.
NEWMAIN179(0x08AB092Cu, func_08AB092C, Word);
NEWMAIN179(0x08AB0DE4u, func_08AB0DE4, Word);
NEWMAIN179(0x08AB0E64u, func_08AB0E64, Word);
NEWMAIN179(0x08AB1C8Cu, func_08AB1C8C, Word);
NEWMAIN179(0x08AB20D8u, func_08AB20D8, Word);
NEWMAIN179(0x08AB2590u, func_08AB2590, Word);
NEWMAIN179(0x08AB2690u, func_08AB2690, Word);
// The insert-range helpers.
// The element copy loop.
// The one-offs.

// ---- from main-session batch 180 (draft 342d8230a8835bbe950de36477cd7561dbc52fb1) ----
// New batch 180 (main session): 30 functions the recompiler emitted that no src/ file defines or registers
// (the two that had a body -- 0x08ADD674, 0x08B10EF0 -- were never registered). Hand-written structured C from
// `tools/disasm`, one function per address, no recompiler output copied. Draft batch: unverified, the synthetic
// gate decides which of them register.
//
// The shapes the batch repeats: the 0x08A5xxxx value-object library (func_08A5BAD8 assigns a holder,
// func_08A5B9EC destroys one, func_08A5BB7C is its null check), which is how the "format a value into a string"
// wrappers read; and the destructor shape -- vtable stores, a range destroyed, its storage released, then the base
// class's destructor.
//
// Three delay slots carry a value, and the C keeps them as the asm has them:
//   * 0x08ADD674: the word at table+0x44 is loaded in the delay slot of the blezl, so v0 holds +0x44 on *both*
//     exits (not +0x40).
//   * 0x08B10EF0: on a list that is empty the returned v0 is the address the `lui` built (0x08D4C500, which the
//     second walk's `lw ... -0x50A0(v0)` is based on), not a callee's result.
//   * 0x08BEE82C: the next link is loaded before the comparison call and v0 gets it before the loop branch.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_180 {
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
// The same with the argument registers set as the original sets them before the `jal` (a0-a3, t0-t3 in order):
// what the call does not pass stays as the caller had it.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call(rt, ctx, target, arguments);
}
// A call whose 64-bit answer v0/v1 is read afterwards (the pair wrappers format it).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, ctx.gpr[2], arguments).v0;
}

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }

// The value-object library (the same helpers the earlier new_batch_* files use).
constexpr u32 kAssign = 0x08A5BAD8u;   // holder_reset(dst, word(src))
constexpr u32 kDestroy = 0x08A5B9ECu;  // holder_destroy(object)
constexpr u32 kCheck = 0x08A5BB7Cu;    // the empty release-build null check, called with a class name

// A producer call fills an object at `object_address`; a null answer trips the check at `message`.
u32 produce(Runtime &rt, AllegrexContext &ctx, u32 producer, u32 object_address, u32 message) {
    call_set(rt, ctx, producer, {object_address});
    const u32 object = word(rt, object_address);
    if (object == 0u) call_set(rt, ctx, kCheck, {message});
    return object;
}
// The end of a wrapper: assigned to `target` (skipped when the target is the temporary itself), then destroyed.
void keep_temporary(Runtime &rt, AllegrexContext &ctx, u32 target, u32 temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

// ---------------------------------------------------------------------------------------------------------------
// Destructors: vtable stores, a container destroyed, its storage released, then the base destructor.

// 0x08A05908 (a0 = the object, a destructor): vtable 0x08CC71A0 at +0 and 0x08CC738C at +0x38; the 0x20-byte-element
// range at +0xA8 (end +0xAC, limit +0xB0) destroyed through func_08BE5C58(begin, end, sp) and its storage released by
// func_08A5BC54(begin, (limit - begin) rounded down to 0x20, 4) when begin is non-null; func_089D8D30(self) ends it.
void func_08A05908(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC71A0u);
    put(rt, self + 0x38u, 0x08CC738Cu);
    const u32 range = self + 0xA8u;  // s0 in the asm: begin at +0, end at +4, limit at +8
    const u32 begin = word(rt, range);
    const u32 end = word(rt, range + 4u);
    call_set(rt, ctx, 0x08BE5C58u, {begin, end, frame.at(0u)});
    const u32 limit = word(rt, range + 8u);
    if (begin != 0u) {
        const u32 size = static_cast<u32>(static_cast<s32>(limit - begin) >> 5) << 5;
        call_set(rt, ctx, 0x08A5BC54u, {begin, size, 4u});
    }
    call_set(rt, ctx, 0x089D8D30u, {self});
}

// 0x08AAEAAC (a0 = the object, a destructor): vtable 0x08CD5618 at +0; the 4-byte-element range at +0x3C (end +0x40,
// limit +0x44) through func_08C2ADCC(begin, end, sp) and its storage released (size rounded down to 4) when begin is
// non-null; then func_08C2AE10(self + 0x24, word +0x2C) and func_08AAE904(self).

// 0x089FCF30 (a0 = the object, a destructor): vtable 0x08CC7808 at +0 and 0x08CC79A4 at +0x38, the holder at +0x68
// destroyed, and the block at +0x64 released (its -0xC not being the shared empty 0x08D74FD0: func_08B60804(block - 4,
// -1) and, unless that answered positive, func_08B8542C(block - 0xC, sp)); func_089BBAC8(self) ends it.

// 0x08B87CAC (a0 = the object, a destructor): vtable 0x08CB88A8 at +0 (in the delay slot of the block's test, so on
// both paths); the block at +0x44 released when its -0xC is not the shared empty 0x08D74FD0 (func_08B60804(block - 4,
// -1) and, unless that answered positive, func_08B8542C(block - 0xC, sp)); then func_08B87B60(self + 0x2C, word +0x34),
// func_08B87AB0(self + 0x14, word +0x1C) and func_08A780B0(self).

// 0x08918734 (a0 = the object, a constructor): vtable 0x08CC58E0 at +0 and 0x08CC5AD0 at +0x1C, +0xC cleared, +0x20
// pointing at the object; func_08B62344(0x21B0) allocates the record stored at +8 and +0xC, func_08928E24(record)
// constructs it, its byte at +0x2180 is set to 1, the words at +0x10 / +0x14 to 1 and the bytes at +0x18..+0x1B
// cleared; v0 = 1.
void func_08918734(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC58E0u);
    put(rt, self + 0x1Cu, 0x08CC5AD0u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x20u, self);  // the jal's delay slot
    const u32 record = call_set(rt, ctx, 0x08B62344u, {0x21B0u});
    call_set(rt, ctx, 0x08928E24u, {record});
    put(rt, self + 8u, record);
    put(rt, self + 0xCu, record);
    put8(rt, record + 0x2180u, 1u);
    put(rt, self + 0x14u, 1u);
    put8(rt, self + 0x1Bu, 0u);
    put(rt, self + 0x10u, 1u);
    put8(rt, self + 0x18u, 0u);
    put8(rt, self + 0x19u, 0u);
    put8(rt, self + 0x1Au, 0u);
    ctx.gpr[2] = 1u;
}

// 0x08ADD674 (a0 = the object): the table at 0x08D40628's counter at +0x40 drops by one when self->+0x78 is set; when
// self->+0x70 is set its counter at +0x48 drops by one and func_08ADD504(self) runs; then, unless +0x40 is positive or
// +0x44 is, func_08ADEB40(0) runs. v0 is the +0x44 word (the blezl delay slot overwrites v0 on both exits), or the
// last call's result.
void func_08ADD674(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kCounters = 0x08D40628u;
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (word(rt, self + 0x78u) != 0u) put(rt, kCounters + 0x40u, word(rt, kCounters + 0x40u) - 1u);
    if (word(rt, self + 0x70u) != 0u) {
        const u32 dropped = word(rt, kCounters + 0x48u) - 1u;
        put(rt, kCounters + 0x48u, dropped);
        call_set(rt, ctx, 0x08ADD504u, {self});
    }
    const u32 first = word(rt, kCounters + 0x40u);
    ctx.gpr[2] = word(rt, kCounters + 0x44u);
    if (static_cast<s32>(first) > 0) return;
    ctx.gpr[4] = 0u;
    if (static_cast<s32>(ctx.gpr[2]) > 0) return;
    call_set(rt, ctx, 0x08ADEB40u, {0u});
}

// 0x08ADFC64 (a0 = the object): func_08ADDCD4(a0, 0x08C9113C) answers a record; a null one leaves v0 = 0. When bit 3
// of its byte at +0x8B is clear: func_08AEB808, bit 3 set in the bytes at +0x8A and +0x8B, func_08AEB82C. Then
// func_08AEB808, bit 3 set in the word at +0xC, func_08AEB82C.
void func_08ADFC64(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 record = call_set(rt, ctx, 0x08ADDCD4u, {ctx.gpr[4], 0x08C9113Cu});
    if (record == 0u) return;
    const u32 flags = record + 0x8Au;
    if ((byte_u(rt, flags + 1u) & 8u) == 0u) {
        call_set(rt, ctx, 0x08AEB808u, {});
        put8(rt, flags, byte_u(rt, flags) | 8u);
        put8(rt, flags + 1u, byte_u(rt, flags + 1u) | 8u);
        call_set(rt, ctx, 0x08AEB82Cu, {});
    }
    call_set(rt, ctx, 0x08AEB808u, {});
    put(rt, record + 0xCu, word(rt, record + 0xCu) | 8u);
    call_set(rt, ctx, 0x08AEB82Cu, {});
}

// ---------------------------------------------------------------------------------------------------------------
// The std::string wrappers (the 0x08A5xxxx value-object library builds the string, which is assigned and released).

// Shape 1: the object func_08817FCC / func_0881BD98 produces at frame+4, `getter(object)`'s answer in a register across
// the object's release, then the word at `field` of it through a formatter into the string at frame+0.
void string_of_destroyed_field(Runtime &rt, AllegrexContext &ctx, u32 producer, u32 getter, u32 field, u32 formatter,
                               u32 message, const GuestFrame &frame) {
    const u32 out = ctx.gpr[4];
    const u32 object = produce(rt, ctx, producer, frame.at(4u), message);
    const u32 record = call_set(rt, ctx, getter, {object});
    call_set(rt, ctx, kDestroy, {frame.at(4u)});
    call_set(rt, ctx, formatter, {frame.at(0u), word(rt, record + field)});
    keep_temporary(rt, ctx, out, frame.at(0u));
}

// Shape 2: the word at +0x14 of the produced object, read before its release, then `reader(member)`'s answer through a
// formatter into the string at frame+0.
void string_of_destroyed_member(Runtime &rt, AllegrexContext &ctx, u32 producer, u32 reader, u32 formatter, u32 message,
                                const GuestFrame &frame) {
    const u32 out = ctx.gpr[4];
    const u32 object = produce(rt, ctx, producer, frame.at(4u), message);
    const u32 member = word(rt, object + 0x14u);
    call_set(rt, ctx, kDestroy, {frame.at(4u)});
    const u32 value = call_set(rt, ctx, reader, {member});
    call_set(rt, ctx, formatter, {frame.at(0u), value});
    keep_temporary(rt, ctx, out, frame.at(0u));
}

// Shape 3: the object at frame+0, its value at frame+4 and the string at frame+8; `getter(object)` goes through
// func_08A76AAC(string, &value), then the string is assigned and both temporaries released.
void string_of_int_getter(Runtime &rt, AllegrexContext &ctx, u32 producer, u32 getter, u32 message,
                          const GuestFrame &frame) {
    const u32 out = ctx.gpr[4];
    const u32 object = produce(rt, ctx, producer, frame.at(0u), message);
    put(rt, frame.at(4u), call_set(rt, ctx, getter, {object}));
    call_set(rt, ctx, 0x08A76AACu, {frame.at(8u), frame.at(4u)});
    keep_temporary(rt, ctx, out, frame.at(8u));
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
}

// Shape 4: `getter(object)` answers a 64-bit value (v0/v1) that func_08A83160(string, a1, low, high) formats; the string
// is built at frame+4, the object lives at frame+0.
void string_of_pair(Runtime &rt, AllegrexContext &ctx, u32 producer, u32 getter, u32 message, const GuestFrame &frame) {
    const u32 out = ctx.gpr[4];
    const u32 object = produce(rt, ctx, producer, frame.at(0u), message);
    const GuestResult value = call_keep(rt, ctx, getter, {object});
    call_set(rt, ctx, 0x08A83160u, {frame.at(4u), ctx.gpr[5], value.v0, value.v1});
    keep_temporary(rt, ctx, out, frame.at(4u));
    call_set(rt, ctx, kDestroy, {frame.at(0u)});
}

// 0x088185F4: func_08817FCC builds the object, func_08817A54(object) answers a record whose word at +0x10 goes into
// func_08A85B1C(string, value); null record -> check 0x08C98D6C.

// 0x0881D87C: the same with func_0881BD98 / func_0881B068 and the word at +4 of the record.

// 0x0881D908: the same with the word at +0 of the record.

// 0x088258D4: func_088248E4 builds the object, the word at +0x14 of it goes through func_0885BDA8 into
// func_08A97A6C(string, value); null object -> check 0x08C99B4C.

// 0x08825AE8: the same through func_0885D010.

// 0x0882629C: the same through func_0885BCC0.

// 0x08826328: the same through func_0885BD24 and func_08A85B1C.

// 0x088263B4: the same through func_0885BD30 and func_08A85B1C.

// 0x0896DA20: func_08969244 builds the object, func_08965BCC(object) answers the value; check 0x08CA2880.

// 0x0896DAAC: the same with func_08968A98.

// 0x0896DB38: the same with func_08968770.

// 0x089BFEF0: func_089BEE4C builds the object, func_089BF1B0(object) answers a pair; check 0x08CA5EB4.

// 0x089BFF7C: the same with func_089BF228.

// 0x089C0008: the same with func_089BF2A0.

// 0x0880C020 (a0 = the object): under the lock on it, func_08A76AAC builds a value object from 0x08CF6ED8 that is
// assigned to the string at +0x208 and destroyed; +0x200 is cleared, func_0886924C(self + 0x30, {0, 0}) and
// func_088691D8(self + 0x30) run, and the unlock ends the function.

// 0x08A3F458 (a0 = the output string, a1 = the record, a2 = a pointer to the object): the object's virtual slot 0x3C
// (null -> check 0x08CADA48) goes through func_08B21590(record + 0x14, it) and func_08A85B1C(string, it);
// func_08A76ABC(a0, string) hands the string over and it is destroyed; v0 = a0.

// 0x08A79420 (a0 = the output string, a1 = a pointer to the object, a2 / a3 = arguments): the object's virtual slot
// 0xB0 (null -> check 0x08CB0E38) builds a value object at frame+0 with {frame, object, a2, a3}; it is assigned to a0
// and destroyed.

// ---------------------------------------------------------------------------------------------------------------
// The rest.

// 0x088FAD1C (a0 = the output record, a1 = the source): a 12-byte value is copied into a0 from a frame temporary that
// func_08900D94(frame, source->+4, a2) builds when source->+4 is set, else func_089032FC(frame + 0x10, source->+0, a2)
// when source->+0 is set (neither set leaves a0 untouched); v0 = a0.
void func_088FAD1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 pointer = word(rt, source + 4u);
    if (pointer != 0u) {
        call_set(rt, ctx, 0x08900D94u, {frame.at(0u), pointer, argument});
        put(rt, target + 4u, word(rt, frame.at(4u)));
        put(rt, target, word(rt, frame.at(0u)));
        put(rt, target + 8u, word(rt, frame.at(8u)));
    } else {
        const u32 other = word(rt, source);
        if (other != 0u) {
            call_set(rt, ctx, 0x089032FCu, {frame.at(0x10u), other, argument});
            put(rt, target + 4u, word(rt, frame.at(0x14u)));
            put(rt, target, word(rt, frame.at(0x10u)));
            put(rt, target + 8u, word(rt, frame.at(0x18u)));
        }
    }
    ctx.gpr[2] = target;
}

// 0x089FA398 (a0 = the output record, a1 = the source range): a buffer of (source->+4 - source->+0) bytes rounded down
// to 0x20 is allocated by func_08B35840(0x10, size); the record gets {buffer, buffer, buffer + size, -1} and
// func_08C0D7F8(source->+0, source->+4, buffer, sp) copies the range in, its answer going to +4; v0 = that answer.
void func_089FA398(Runtime &rt, AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 range = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 begin = word(rt, range);
    const u32 end = word(rt, range + 4u);
    const u32 size = (end - begin) & ~31u;
    put(rt, target + 4u, 0u);
    put(rt, target, 0u);
    put(rt, target + 8u, 0u);
    const u32 buffer = call_set(rt, ctx, 0x08B35840u, {0x10u, size});
    put(rt, target, buffer);
    put(rt, target + 4u, buffer);
    put(rt, target + 8u, buffer + size);
    const u32 written = call_set(rt, ctx, 0x08C0D7F8u, {begin, end, buffer, frame.at(0u)});
    put(rt, target + 0xCu, 0xFFFFFFFFu);
    put(rt, target + 4u, written);
}

// 0x08B10EF0: a one-shot table walk. When the flag byte at 0x08D47494 is already set it is a tail jump to
// func_08B2E064 (v0 = the byte). Otherwise it is set, func_08B17924 runs, and the lists at 0x08D77350 (next at +0,
// value at +4 through func_08B10E58) and at 0x08D47460 (next at +8, value at +0 through func_08B10598) are walked.
// An empty second list returns the address the `lui` built (0x08D4C500), as the original does.

// 0x08B14E34 (a0 = the destination, a1 = a2, a2 = a3, a3 = t0, t0 = t1 & 0xFF): func_08B18534 builds a 0x3C-byte record
// in the frame from them and it is copied to a0 -- 0x30 bytes in 16-byte blocks, then the three words at frame+0x30;
// v0 = a0.

// 0x08BDA64C (a0 = first, a1 = last): a range shorter than 0x88 bytes is only handed on in a tail call to
// func_08BDA574(a0, a1, a2). Otherwise func_08BDA574(first, first + 0x80, a2) runs and every 8-byte element from
// first + 0x80 on goes through func_08BDA4F0(element, sp, element + 8) with the element's pair copied to sp first.

// 0x08BEE82C (a0 = the list head, a1 = the key): a head that is the list itself ends the walk with v0 = it. Otherwise
// the list is walked from word(a0) while func_089FD6E0(key, node's word at +8) answers nonzero; such a node is
// released by func_08B5C094(node), func_08A5B9EC(node + 8) and func_08A5BC54(node, 0xC, 4). v0 = the link that closed
// the walk (the list itself), loaded before the loop branch decides.

} // namespace
} // namespace new_batch_main_180
} // namespace gt::game

#define NEW180(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_180::name, gt::ReturnKind::kind)
NEW180(0x088FAD1Cu, func_088FAD1C, Word);
NEW180(0x08918734u, func_08918734, Word);
NEW180(0x089FA398u, func_089FA398, Word);
NEW180(0x08A05908u, func_08A05908, Word);
NEW180(0x08ADD674u, func_08ADD674, Word);
NEW180(0x08ADFC64u, func_08ADFC64, Word);

