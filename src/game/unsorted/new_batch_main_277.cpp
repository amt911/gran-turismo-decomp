// ---- from main-session batch 193 (draft 23e283bb2eff2e47e2e7002362274e6f0e206e71) ----
// New batch main 193 (main session): functions the recompiler emitted that no src/ file defines or registers. Written by
// hand from `tools/disasm` as structured C. DRAFT: not verified by the differential gate yet, so nothing below is
// registered as done.
//
// The helpers cover the shapes the batch repeats: a guest call that leaves v0 / v1 / f0 / a1 as the original's `jal`
// leaves them, the compiler's indirect call, and the two families (twelve holder constructors that differ only in the
// vtable word they store, three convert-and-assign members). No names: none has evidence.
//
// Two quirks kept on purpose:
// - 0x08C15E8C passes the *advanced* copy pointer to its trailing call: the original advances a0 by 0x2C per record in
//   the loop's delay slot and only the `count <= 0` path leaves a0 = this.
// - 0x08C15E8C's record count is the compiler's fixed-point divide by 11 (`mult` by 0xBA2E8BA3, `mflo`), exact only
//   when the byte distance is a multiple of 11 words; a plain divide would differ on the garbage synthetic inputs.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_193 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The callee-saved registers the body writes are set where the original sets them (a callee may read them) and the
// caller's values come back when the body ends, as the original's prologue / epilogue pair does.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers)
        : ctx_(ctx), count_(registers.size()) {
        unsigned index = 0u;
        for (const unsigned reg : registers) {
            registers_[index] = reg;
            saved_[index] = ctx.gpr[reg];
            ++index;
        }
    }
    ~SavedRegisters() {
        for (unsigned index = 0u; index < count_; ++index) ctx_.gpr[registers_[index]] = saved_[index];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned count_;
    std::array<unsigned, 10> registers_{};
    std::array<u32, 10> saved_{};
};

// A guest call with the argument registers written as the original writes them before its `jal` (a0-a3, t0-t3 in
// order); afterwards v0, v1, f0 and a1 hold what the callee left, as after a `jal` (a callee that never writes v0 hands
// the previous one on, and a1 is whatever the callee leaves in it).
GuestResult call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_value(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call(rt, ctx, target, arguments).v0;
}
// The compiler's `lw v1, 0(object); lw v0, slot(v1); jalr v0`: v0 starts as the callee's own address, v1 as the vtable.
GuestResult call_member(Runtime &rt, AllegrexContext &ctx, u32 table, u32 slot, std::initializer_list<u32> arguments) {
    const u32 function = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call(rt, ctx, function, arguments);
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// --- the convert-and-assign family (three members) ----------------------------------------------------------------
// a0 = the output object, a2 > 0 selects "set": the value is converted (ctor, convert) and handed to `set` together with
// the answer of a3's object virtual +0x3C (its truth when the member takes a flag); a2 <= 0 selects "get": the converted
// value becomes text at sp+0, the caller's object is assigned from it and the temporary released. v0 ends as the last
// call leaves it (the "set" path tail-calls, the "get" path returns the release's result).
void convert_and_assign(Runtime &rt, AllegrexContext &ctx, u32 ctor, u32 convert, u32 message, u32 second_message,
                        u32 set, u32 get, bool flag) {
    const u32 output = ctx.gpr[4];
    const s32 mode = static_cast<s32>(ctx.gpr[6]);
    const u32 other = ctx.gpr[7];
    u32 passed = 0u;
    u32 answer = 0u;
    {
        SavedRegisters saved(ctx, {16u, 17u, 18u, 19u});
        GuestFrame frame(ctx, 0x30u);
        ctx.gpr[17] = output;  // s1 = a0
        ctx.gpr[18] = other;   // s2 = a3
        if (mode > 0) {
            const u32 temporary = frame.at(8u);  // s3 = sp + 8
            ctx.gpr[19] = temporary;
            call(rt, ctx, ctor, {temporary, ctx.gpr[5]});  // a1 is still the caller's
            const u32 object = word(rt, temporary);
            ctx.gpr[16] = object;  // s0
            if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
            const u32 converted = call_value(rt, ctx, convert, {object});
            ctx.gpr[17] = converted;  // the jal's delay slot: s1 = v0 (before the release call)
            call(rt, ctx, 0x08A5B9ECu, {temporary});
            const u32 target = word(rt, other);  // lw s0, 0(s2)
            ctx.gpr[16] = target;
            if (target == 0u) call(rt, ctx, 0x08A5BB7Cu, {second_message});
            const GuestResult result = call_member(rt, ctx, word(rt, target), 0x3Cu, {target});
            passed = converted;
            answer = flag ? (result.v0 != 0u ? 1u : 0u) : result.v0;
        } else {
            const u32 temporary = frame.at(4u);  // s2 = sp + 4
            ctx.gpr[18] = temporary;
            call(rt, ctx, ctor, {temporary, ctx.gpr[5]});
            const u32 object = word(rt, temporary);
            ctx.gpr[16] = object;
            if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
            const u32 converted = call_value(rt, ctx, convert, {object});
            ctx.gpr[16] = converted;  // addu s0, v0
            call(rt, ctx, 0x08A5B9ECu, {temporary});
            const u32 text = call_value(rt, ctx, get, {converted});
            call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), text});
            if (output != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(0u))});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
    }
    if (mode > 0) tail_call(ctx, set, {passed, answer});
}


// --- holder constructors (twelve members) -------------------------------------------------------------------------
// a0 = self, a1 = text, a2 / a3 / t0 = three pairs. 0x08A69B3C(&temp, text) builds a temporary, 0x08A5BC20(0x2C, 4)
// allocates the holder and 0x08A98220(holder, &temp) initialises it; the vtable goes in word 0, a2[0] -> +0x14,
// a2[1] -> +0x18, a3[1] -> +0x20, a3[0] -> +0x1C, t0[1] -> +0x28, t0[0] -> +0x24. 0x08A69D68(&temp) finishes the
// temporary, 0x08A5D414(&slot, &temp) moves the holder into the caller's slot, 0x08A7C5C8(self, &slot) takes it and
// 0x08A5B9EC(&slot) releases it. The original's 0x08B867AC / zero test picks values that are both plain copies of a2's
// words (its third branch stores 0 where a2[1] is 0), so the stores below are the plain ones.
void make_holder(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 pair = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 30u});
    GuestFrame frame(ctx, 0x50u);
    const u32 temporary = frame.at(8u);  // s0 = sp + 8
    const u32 slot = frame.at(4u);       // s4 = sp + 4
    ctx.gpr[16] = temporary;
    ctx.gpr[20] = slot;
    const u32 first = word(rt, pair);
    const u32 second = word(rt, pair + 4u);
    const u32 third_first = word(rt, third);
    const u32 fourth_first = word(rt, fourth);
    const u32 third_second = word(rt, third + 4u);
    const u32 fourth_second = word(rt, fourth + 4u);
    ctx.gpr[18] = first;          // s2
    ctx.gpr[19] = second;         // s3
    ctx.gpr[23] = third_first;    // s7
    ctx.gpr[30] = fourth_first;   // fp
    ctx.gpr[21] = third_second;   // s5
    ctx.gpr[22] = fourth_second;  // s6
    call(rt, ctx, 0x08A69B3Cu, {temporary, text});
    const u32 object = call_value(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    ctx.gpr[17] = object;  // s1, the jal's delay slot
    call(rt, ctx, 0x08A98220u, {object, temporary});
    put(rt, object, vtable);
    put(rt, object + 0x14u, first);
    put(rt, object + 0x18u, second);
    put(rt, object + 0x20u, third_second);
    put(rt, object + 0x1Cu, third_first);
    put(rt, object + 0x28u, fourth_second);
    put(rt, frame.at(0u), object);  // the delay slot of the finishing call
    put(rt, object + 0x24u, fourth_first);
    call(rt, ctx, 0x08A69D68u, {temporary});
    call(rt, ctx, 0x08A5D414u, {slot, frame.at(0u)});
    call(rt, ctx, 0x08A7C5C8u, {self, slot});
    call(rt, ctx, 0x08A5B9ECu, {slot});
}


// --- singletons ----------------------------------------------------------------------------------------------------
// 0x088F9B68 (a0 = a fresh record, a1 = the owner): stores the two vtables 0x08CC3F38 (at +0) and 0x08CC3F68 (at
// word(a0 + 8)), allocates 0x4C bytes (0x08B62344) as the sub-record: its vtables 0x08CC3F68 / 0x08CC3FA0 / 0x08CC3FBC,
// words +8..+0x10 and byte +0x14 cleared, nine words from +0x1C zeroed and 0x08B1C2BC(+0x40) run; the sub-record goes to
// word(a0 + 8). The lazily initialised global object at 0x08D76148 (guard struct 0x08D76138, 0x08B6441C / 0x08B64494)
// becomes its vtable 0x08CC4060, that address goes to a0 + 0xC and a1 to word(*(a0 + 8) + 0x10). v0 = the sub-record.
void func_088F9B68(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    constexpr u32 kGuard = 0x08D76138u;
    constexpr u32 kGlobal = 0x08D76148u;
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u});
    GuestFrame frame(ctx, 0x20u);
    const u32 pointer = self + 8u;  // s1
    ctx.gpr[19] = owner;            // s3 = a1
    ctx.gpr[17] = pointer;
    put(rt, self, 0x08CC3F38u);
    put(rt, self + 4u, 0u);  // the allocation call's delay slot
    const u32 record = call_value(rt, ctx, 0x08B62344u, {0x4Cu});
    ctx.gpr[16] = record;    // s0
    put(rt, record, 0x08CC3F68u);
    put(rt, record + 4u, 0x08CC3FA0u);
    put(rt, record + 0x18u, 0x08CC3FBCu);
    put(rt, record + 8u, 0u);
    put(rt, record + 0xCu, 0u);
    put(rt, record + 0x10u, 0u);
    put8(rt, record + 0x14u, 0u);
    for (u32 index = 0u, at = record + 0x1Cu; index < 3u; ++index, at += 0xCu) {
        put(rt, at, 0u);
        put(rt, at + 4u, 0u);
        put(rt, at + 8u, 0u);
    }
    call(rt, ctx, 0x08B1C2BCu, {record + 0x40u});
    put(rt, pointer, record);
    if (rt.memory().load8(kGuard) == 0u) {
        if (call_value(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            put(rt, kGlobal, 0x08CC4060u);
            call(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    const u32 sub_record = word(rt, pointer);  // reloaded after the guard calls
    put(rt, self + 0xCu, kGlobal);
    put(rt, sub_record + 0x10u, owner);
    ctx.gpr[2] = sub_record;
}

// 0x08BA9388 (a0 = first slot, a1 = end, a2 = source): assigns the source's 8-byte shared slot (a raw word, then the
// counted pointer at +4, whose count at +4 rises) to every slot of [a0, a1); each replaced pointer's count at +4 falls
// and, at zero, the object's virtual +8 runs and its count at +8 falls the same way (virtual +0xC at zero). The counts
// are the PSP's ll/sc pair, a plain add in a single-threaded run.

// 0x08C15E8C (a0 = this, a1 = a copy destination, a2 = a pointer into the vector): the 0x2C-byte records between a2 and
// word(this + 4) are copied to a1 (words at +0 and +4, eight floats at +8..+0x24, the word at +0x28), then
// 0x08C15E7C(the advanced copy pointer, word(this + 4), sp) runs and the vector's end word +4 drops by a2 - a1 with its
// low 2 bits cleared. v0 = a1.

// 0x088B0520 (a0 = a container): under the lock at a0 + 0x20, 0x088AFB3C walks the nodes of the container (the end is
// a0 + 0xC): each node takes 0x08B5F444(node, end), releases the counted block at +0x18 (count at +8, virtual +0xC at
// zero) and the shared block at +0x10, and 0x08B35254(node) finishes it while the count at a0 + 0x1C falls. The lock is
// dropped with 0x08B34B24.

// 0x08B3120C (a0 = this): a frame of word(this + 0x10) 16-byte entries is taken on the stack (plus one word), zeroed
// with 0x08B36018 and its first word set to the count; under the lock at this + 0x8FC the sixteen records at +0x928
// (stride 0x98, up to +0x980) whose first two words are not both zero are collected as three words each, up to the
// count. 0x08B308FC(this, sp, the distance of the collected part, 4, 1, 1) takes them, sp = fp restoring the frame.

// 0x08AC0F08 (a0 = this, a1 = ?, a2 = ?, a3 = a pair, t0 = a block, t1 = a flag byte): 0x08B0EA48(0) allocates. With an
// allocation, byte(allocated + 0x68) = 1, word + 0x7C = 0, the a3 pair goes to (allocated + 0xF8) + 0x28 / +0x2C, the
// flag byte to +0x10C, the block's four words to allocated + 0x6C..+0x78, then
// 0x08B0DE84(allocated, a2, block[2], block[3]) runs and 0x08B0E560(this, allocated) finishes (tail call). Without one,
// block[2] takes over (tail call with block[3]) when it is nonzero, else v0 = block + 8.

} // namespace
} // namespace new_batch_main_193
} // namespace gt::game

#define MAIN193(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_193::name, gt::ReturnKind::kind)
MAIN193(0x088F9B68u, func_088F9B68, Word);

// ---- from main-session batch 194 (draft 0d224fe789c588ee2df3a3290f85f447c6a76090) ----
// New batch main 194: functions no `src/` file registered (some drafted once as register-level bodies in an earlier
// batch and parked there as "no comparable synthetic case yet"), rewritten here as structured C from `tools.disasm`.
// This is a draft batch: correctness is the differential gate's to prove, not this file's.
//
// The families:
//   * 8 string-map constructors (0x08974F64 .. 0x08A0598C): a base constructor, then every node of a singly linked
//     list is turned into a name and inserted; both stack temporaries are released. They differ only in two callees.
//   * 2 fixed-point pair adders (0x08A3397C adds, 0x08A33AA8 subtracts) that share one body.
//   * 4 float rotation helpers (0x08C52A3C / 0x08C52B64 / 0x08C52C8C / 0x08C52DB8): one 08C5250C + 08B499B8 pair
//     then one or two 08B4A430 angle calls, each variant reading the same four floats in a different order.
//   * singletons: the font glyph-image fill (0x08A4F784), the table re-init (0x08ACE7A8), the global list drain
//     (0x08B2EAC8), a graphics-flag thunk (0x08B56250) and a linked-list deep copy (0x08C27F70).
//
// Conventions (as hot_batch_103 / new_batch_main_12): every call passes the argument registers the original sets
// before the jal; a delay slot runs BEFORE its call or branch, and a `bnel` / `blezl` delay slot only when the branch
// is taken. v0 (and v1 / f0 where a caller reads them) ends as the asm leaves it on every path. The callee-saved
// registers are never written here — the original restores them from its frame before it returns — and v0/v1 of a
// callee are propagated through `call_guest`, so a callee that sets no v0 hands the previous one on. Dead stack
// (stores to locals nothing reads back) is not reproduced; the frames a callee reads or writes are (`GuestFrame`,
// offsets as in the original). The float bodies keep one host float operation per PSP operation, in the asm's order,
// so each intermediate rounds exactly as `mul.s` / `add.s` / `sub.s` did.
//
// Not drafted: 0x08B2E620 stores `$sp` into `0x4($k0)` (the thread-state record) in a jal delay slot — the
// setjmp-shaped family this batch skips.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_194 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// Records the callee-saved registers the body writes and puts the caller's values back at the end (the original's
// prologue/epilogue pair). A function that never writes them keeps them as they came in.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx), count_(registers.size()) {
        unsigned index = 0u;
        for (const unsigned reg : registers) {
            registers_[index] = reg;
            saved_[index] = ctx.gpr[reg];
            ++index;
        }
    }
    ~SavedRegisters() {
        for (unsigned index = 0u; index < count_; ++index) ctx_.gpr[registers_[index]] = saved_[index];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned count_;
    std::array<unsigned, 10> registers_{};
    std::array<u32, 10> saved_{};
};

// The float registers the rotation helpers save at sp + 0x10 .. sp + 0x2C and reload at the end. The gate compares no
// fpr but f0, but the original restores them and a callee may rely on the caller's values.
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned index = 0u; index < 8u; ++index) saved_[index] = ctx.fpr[20u + index];
    }
    ~SavedFloats() {
        for (unsigned index = 0u; index < 8u; ++index) ctx_.fpr[20u + index] = saved_[index];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    std::array<float, 8> saved_{};
};

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same, keeping v1 and f0 as the callee left them (a caller here reads the second register of a 64-bit answer).
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with float arguments ($f12, $f13, ... in order); f0 is the answer.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<float> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, {}, arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// ---------------------------------------------------------------------------------------------------------------
// The string-map constructors.
//
// a0 = self. `base(self)` constructs the base, `first(0)` answers the head of a list whose next pointer is at +0x0C.
// For each node: its word at +0 is turned into a temporary string at sp + 4 (0x08B85EB8(text, node->word, &flag) with
// the flag byte at sp + 2), copied into the pair at sp + 0x10 (0x08B88F44(pair, text)), inserted into self's map
// (0x08BE9D90(self, pair)) and both temporaries are released with 0x08B60804 / 0x08B8542C: a COW cell whose data
// pointer is not the shared empty 0x08D74FD0 is dropped, and a non-positive reference count disposes it (the first
// with the scratch byte sp + 1, the second with sp).
// ---------------------------------------------------------------------------------------------------------------
constexpr u32 kEmptyString = 0x08D74FD0u;

// The value v0 holds after the release, exactly as the asm leaves it: the previous v0 when the cell is the shared
// empty one, the reference-count answer, or the dispose call's answer.
u32 release_cell(Runtime &rt, AllegrexContext &ctx, u32 data, u32 scratch) {
    const u32 cell = data - 0xCu;
    if (cell == kEmptyString) return ctx.gpr[2];
    const u32 references = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(references) > 0) return references;
    return call(rt, ctx, 0x08B8542Cu, {cell, scratch});
}

void walk_names(Runtime &rt, AllegrexContext &ctx, u32 base, u32 first) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u});
    GuestFrame frame(ctx, 0x50u);
    call(rt, ctx, base, {self});
    u32 node = call(rt, ctx, first, {0u});
    if (node == 0u) return;  // v0 = the list call's answer, 0
    for (;;) {
        call(rt, ctx, 0x08B85EB8u, {frame.at(4u), word(rt, node), frame.at(2u)});
        call(rt, ctx, 0x08B88F44u, {frame.at(0x10u), frame.at(4u)});
        call(rt, ctx, 0x08BE9D90u, {self, frame.at(0x10u)});
        (void)release_cell(rt, ctx, word(rt, frame.at(0x10u)), frame.at(1u));
        // The second temporary is re-read after the first release (that call may have replaced it), and the
        // `lw v0, 4(sp)` the asm does at the head of the second cleanup is what v0 ends with when it is empty.
        const u32 text = word(rt, frame.at(4u));
        ctx.gpr[2] = text;
        (void)release_cell(rt, ctx, text, frame.at(0u));
        node = word(rt, node + 0xCu);
        if (node == 0u) return;
    }
}


// ---------------------------------------------------------------------------------------------------------------
// 0x08A3397C / 0x08A33AA8: the fixed-point pair adders.
//
// a0 = this, a1 = a holder. 0x08B2DFB4(this, holder) answers a 64-bit pair, 0x08A33604(&temporary, holder) a second
// holder whose +0x14 0x08B1B668 answers another pair, and 0x08A337A8(&target) the destination object. 0x08B1B740
// combines the two pairs (the add, or the subtract with the borrow) and its answer goes to target + 0x14 / + 0x18
// verbatim (the asm re-inserts both halves of each word). `this` receives the target (0x08A5BAD8) unless it is the
// temporary itself; the temporary is always released (0x08A5B9EC). v0 = the last release's answer.
// ---------------------------------------------------------------------------------------------------------------
void combine_pairs(Runtime &rt, AllegrexContext &ctx, bool subtract) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u});
    GuestFrame frame(ctx, 0x40u);
    const GuestResult left = call_result(rt, ctx, 0x08B2DFB4u, {self, source});
    call(rt, ctx, 0x08A33604u, {frame.at(0u), source});
    const u32 holder = word(rt, frame.at(0u));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACCECu});
    const GuestResult right = call_result(rt, ctx, 0x08B1B668u, {holder + 0x14u});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call(rt, ctx, 0x08A337A8u, {frame.at(4u)});
    const u32 target = word(rt, frame.at(4u));
    if (target == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACCECu});
    u32 low;
    u32 high;
    if (subtract) {
        low = right.v0 - left.v0;
        high = (right.v1 - left.v1) - (right.v0 < low ? 1u : 0u);
    } else {
        low = left.v0 + right.v0;
        high = (left.v1 + right.v1) + (low < left.v0 ? 1u : 0u);
    }
    const GuestResult sum = call_result(rt, ctx, 0x08B1B740u, {low, high});
    // The asm's ins/ext pair rebuilds each answer word from its two halves; the net value is the word itself.
    put(rt, target + 0x18u, sum.v1);
    put(rt, target + 0x14u, sum.v0);
    if (self != frame.at(4u)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(4u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}


// ---------------------------------------------------------------------------------------------------------------
// The four rotation helpers.
//
// a0 = four floats at +0, +4, +8, +0xC; a1, a2, a3 are output floats (which of them receives what differs per
// variant). Each computes one 08C5250C argument, stores its answer, asks 08B499B8 about it, and then calls the
// angle helper 08B4A430(x, y) once or twice depending on whether that answer is zero. The `-2.0f` variants fold the
// negated product into the argument instead of into the angle input (`lui 0xC000` is -2.0f).
// ---------------------------------------------------------------------------------------------------------------
void store_float(Runtime &rt, u32 address, float value) { put_float(rt, address, value); }

// `s == 0` picks the one-call arm: the first output gets 0.0f and the second the angle; otherwise the first output
// gets the first angle and the second the second. The zero store happens before its call (the asm's delay slot).
void two_angles(Runtime &rt, AllegrexContext &ctx, bool zero, float x1, float y1, float x2, float y2, u32 first,
                u32 second) {
    if (zero) {
        store_float(rt, first, 0.0f);
        store_float(rt, second, call_floats(rt, ctx, 0x08B4A430u, {x1, y1}).f0);
    } else {
        store_float(rt, first, call_floats(rt, ctx, 0x08B4A430u, {x1, y1}).f0);
        store_float(rt, second, call_floats(rt, ctx, 0x08B4A430u, {x2, y2}).f0);
    }
}

// p0..p3 are the four inputs (read before any call: an output may alias a0). Each product and each difference is its
// own statement, so every one rounds once, as the PSP's scalar operations did.
struct Quat {
    float p0, p1, p2, p3;
    float p0p1, p0p2, p0p3, p1p2, p1p3, p2p3;
    float p0sq, p1sq, p2sq;
};

Quat load_quat(Runtime &rt, u32 base) {
    Quat q{};
    q.p0 = float_at(rt, base + 0u);
    q.p1 = float_at(rt, base + 4u);
    q.p2 = float_at(rt, base + 8u);
    q.p3 = float_at(rt, base + 0xCu);
    q.p0p1 = q.p0 * q.p1;
    q.p0p2 = q.p0 * q.p2;
    q.p0p3 = q.p0 * q.p3;
    q.p1p2 = q.p1 * q.p2;
    q.p1p3 = q.p1 * q.p3;
    q.p2p3 = q.p2 * q.p3;
    q.p0sq = q.p0 * q.p0;
    q.p1sq = q.p1 * q.p1;
    q.p2sq = q.p2 * q.p2;
    return q;
}

// 0x08C52A3C: 08C5250C gets 2*(p0*p1 + p2*p3) (its answer goes to a3) and the angle inputs are the p1*p2 - p0*p3
// diagonal pair; a1 receives 0 on the zero arm.
void func_08C52A3C(Runtime &rt, AllegrexContext &ctx) {
    SavedFloats saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 out1 = ctx.gpr[5];
    const u32 out2 = ctx.gpr[6];
    const u32 out3 = ctx.gpr[7];
    const Quat q = load_quat(rt, ctx.gpr[4]);
    float chord = q.p0p1 + q.p2p3;
    chord = chord + chord;
    const float first = call_floats(rt, ctx, 0x08C5250Cu, {chord}).f0;
    store_float(rt, out3, first);
    const float probe = call_floats(rt, ctx, 0x08B499B8u, {first}).f0;
    const float diagonal = q.p1p2 - q.p0p3;
    float y1 = 0.5f - q.p2sq;
    y1 = y1 - q.p0sq;
    if (probe == 0.0f) {
        float y = 0.5f - q.p0sq;
        y = y - q.p1sq;
        two_angles(rt, ctx, true, q.p0p2 + q.p1p3, y, 0.0f, 0.0f, out1, out2);
    } else {
        float y2 = 0.5f - q.p1sq;
        y2 = y2 - q.p2sq;
        two_angles(rt, ctx, false, -diagonal, y1, -(q.p0p2 - q.p1p3), y2, out1, out2);
    }
}

// 0x08C52B64: 08C5250C gets 2*(p0*p3 + p1*p2) (to a1, out1), the diagonal pair is p0*p2 - p1*p3, and a2 receives 0
// on the zero arm.
void func_08C52B64(Runtime &rt, AllegrexContext &ctx) {
    SavedFloats saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 out1 = ctx.gpr[5];
    const u32 out2 = ctx.gpr[6];
    const u32 out3 = ctx.gpr[7];
    const Quat q = load_quat(rt, ctx.gpr[4]);
    float chord = q.p0p3 + q.p1p2;
    chord = chord + chord;
    const float first = call_floats(rt, ctx, 0x08C5250Cu, {chord}).f0;
    store_float(rt, out1, first);
    const float probe = call_floats(rt, ctx, 0x08B499B8u, {first}).f0;
    const float diagonal = q.p0p2 - q.p1p3;
    float y1 = 0.5f - q.p0sq;
    y1 = y1 - q.p1sq;
    if (probe == 0.0f) {
        float y = 0.5f - q.p1sq;
        y = y - q.p2sq;
        two_angles(rt, ctx, true, q.p0p1 + q.p2p3, y, 0.0f, 0.0f, out2, out3);
    } else {
        float y2 = 0.5f - q.p2sq;
        y2 = y2 - q.p0sq;
        two_angles(rt, ctx, false, -diagonal, y1, -(q.p0p1 - q.p2p3), y2, out2, out3);
    }
}

// 0x08C52C8C: 08C5250C gets -2*(p0*p1 - p2*p3) (to a3), the two angle x values are p0*p2 + p1*p3 and p0*p3 + p1*p2,
// and a2 receives 0 on the zero arm.
void func_08C52C8C(Runtime &rt, AllegrexContext &ctx) {
    SavedFloats saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 out1 = ctx.gpr[5];
    const u32 out2 = ctx.gpr[6];
    const u32 out3 = ctx.gpr[7];
    const Quat q = load_quat(rt, ctx.gpr[4]);
    float diagonal = q.p0p1 - q.p2p3;
    diagonal = diagonal * -2.0f;
    const float first = call_floats(rt, ctx, 0x08C5250Cu, {diagonal}).f0;
    store_float(rt, out3, first);
    const float probe = call_floats(rt, ctx, 0x08B499B8u, {first}).f0;
    const float cross = q.p1p2 - q.p0p3;
    float y1 = 0.5f - q.p1sq;
    y1 = y1 - q.p2sq;
    if (probe == 0.0f) {
        float y = 0.5f - q.p0sq;
        y = y - q.p1sq;
        two_angles(rt, ctx, true, -cross, y, 0.0f, 0.0f, out2, out1);
    } else {
        float y2 = 0.5f - q.p2sq;
        y2 = y2 - q.p0sq;
        two_angles(rt, ctx, false, q.p0p2 + q.p1p3, y1, q.p0p3 + q.p1p2, y2, out2, out1);
    }
}

// 0x08C52DB8: 08C5250C gets -2*(p1*p2 - p0*p3) (to a1, out1), the two angle x values are p0*p1 + p2*p3 and
// p0*p2 + p1*p3, and a3 receives 0 on the zero arm.
void func_08C52DB8(Runtime &rt, AllegrexContext &ctx) {
    SavedFloats saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 out1 = ctx.gpr[5];
    const u32 out2 = ctx.gpr[6];
    const u32 out3 = ctx.gpr[7];
    const Quat q = load_quat(rt, ctx.gpr[4]);
    float diagonal = q.p1p2 - q.p0p3;
    diagonal = diagonal * -2.0f;
    const float first = call_floats(rt, ctx, 0x08C5250Cu, {diagonal}).f0;
    store_float(rt, out1, first);
    const float probe = call_floats(rt, ctx, 0x08B499B8u, {first}).f0;
    const float cross = q.p0p2 - q.p1p3;
    float y1 = 0.5f - q.p2sq;
    y1 = y1 - q.p0sq;
    if (probe == 0.0f) {
        float y = 0.5f - q.p1sq;
        y = y - q.p2sq;
        two_angles(rt, ctx, true, -cross, y, 0.0f, 0.0f, out3, out2);
    } else {
        float y2 = 0.5f - q.p0sq;
        y2 = y2 - q.p1sq;
        two_angles(rt, ctx, false, q.p0p1 + q.p2p3, y1, q.p0p2 + q.p1p3, y2, out3, out2);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08A4F784: the glyph-image fill.
//
// a0 = a char descriptor (byte flags at +4 / +5, word +8, word +0xC, halfword +0x12), a1 = a font handle, a2 = a
// 16-bit character code. 08A4F5C0(code, a1, code, a3) answers a glyph offset (v0) and a second word; the offset and
// its low halfword go to sp + 0x60 / sp + 4 for 08C7ACB0(font, code, sp + 0x20) (sceFontGetCharInfo), and the
// descriptor's word + 8 plus `(v0 & 0xFFFF) * halfword(+0x12)` (an mtlo/madd pair) is the text destination. That
// many bytes at the destination are cleared (08B36018), the glyph image record at sp + 8 is built and handed to
// 08C7ACA8(font, code, &image, 0, 0, byte + 4, byte + 5) (sceFontGetCharGlyphImage_Clip), and four bytes plus two
// halfwords go to word(descriptor + 0xC) + ((v0 & 0xFFFF) << 3). v0 = the word at sp + 0x50 (image + 0x48).
// ---------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------------------------
// 0x08ACE7A8: re-initialise a table object.
//
// a0 = this. 08C7ADA8(this + 0x1C0) looks a list entry up (a null answer just returns v0 = 0) and
// 08C7ADA0(this + 0x1C0, entry, entry) reports a code: a negative or zero one finishes with a tail call of
// 08ACE768(this). Otherwise the word at +0x328 records of 0xEC bytes from +0x334 are walked with 08ACD4F0 and the
// word at +0x32C records of 0xF0 bytes from +0x420 with 08ACD8B4 (both counts re-read per iteration, as the asm
// does). v0 = the +0x32C word when it is not positive, else 0.
// ---------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------------------------
// 0x08B2EAC8: drain the global callback list.
//
// The interrupts are suspended (08C7A9E8, the state kept at 0x08D690F0) and an alarm is set
// (08C7AA48(0x1046A, 0x08B2E964, 0), the handle tail-called later). While the byte at 0x08D690F4 is set, the
// 0x08D690C4 lock is taken (08B34888, and 08B34B2C when 0x08D690C0 is clear), the list at 0x08D690C0 is detached
// and every node runs its callback at +0x30 with word(node + 0x34), then is released through 08B28C74(node + 4);
// 08C7A9F0 (sceKernelCheckCallback) closes the section. 08C7AA60 takes over at the end with the alarm handle.
// ---------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------------------------
// 0x08B56250: set bit 1 of the graphics state flags (0x08D6CE6C = kGuFlags) and tail-call 0x08B56148 with the
// command block 0x08D6D0E0. The flag store is the jump's delay slot; v0 is the state block address the asm leaves.
// ---------------------------------------------------------------------------------------------------------------
void func_08B56250(Runtime &rt, AllegrexContext &ctx) {
    const u32 flags = word(rt, kGuFlags) | 2u;
    put(rt, kGuFlags, flags);
    ctx.gpr[2] = kGuState;
    ctx.gpr[3] = flags;
    tail_call(ctx, 0x08B56148u, {kGuCommandBlock});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08C27F70: deep-copy a node list.
//
// a0 = owner, a1 = node, a2 = a word stored in every copy's +0. 08A5BC20(0x18, 4, ...) allocates a copy; its +0x10
// and +0x14 are copied from the node's (0x08A6972C, then `second_copy` - 0x08A5D424 here), the node's word goes to
// +0, the given word to +4, +8 is cleared and the node's +0xC child (when set) is copied with a recursive call of
// this same address whose answer goes to the copy's +0xC. The node's +8 chain is copied the same way, each copy
// linked through +8 and back through the previous copy's +4. v0 = the first copy.
// ---------------------------------------------------------------------------------------------------------------
u32 clone_node_list(Runtime &rt, AllegrexContext &ctx, u32 address, u32 second_copy) {
    const u32 owner = ctx.gpr[4];
    const u32 node = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    const u32 copy = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    if (copy + 0x10u != 0u) {  // the asm tests the member address, not the allocation
        call(rt, ctx, 0x08A6972Cu, {copy + 0x10u, node + 0x10u});
        call(rt, ctx, second_copy, {copy + 0x14u, node + 0x14u});
    }
    put(rt, copy + 0xCu, 0u);
    put(rt, copy, word(rt, node));
    put(rt, copy + 4u, value);
    put(rt, copy + 8u, 0u);
    const u32 child = word(rt, node + 0xCu);
    if (child != 0u) put(rt, copy + 0xCu, call(rt, ctx, address, {owner, child, copy}));
    u32 next = word(rt, node + 8u);
    u32 previous = copy;
    while (next != 0u) {
        const u32 next_copy = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
        if (next_copy + 0x10u != 0u) {
            call(rt, ctx, 0x08A6972Cu, {next_copy + 0x10u, next + 0x10u});
            call(rt, ctx, second_copy, {next_copy + 0x14u, next + 0x14u});
        }
        put(rt, next_copy + 0xCu, 0u);
        const u32 grandchild = word(rt, next + 0xCu);
        put(rt, next_copy + 4u, previous);
        put(rt, next_copy + 8u, 0u);
        put(rt, next_copy, word(rt, next));
        put(rt, previous + 8u, next_copy);
        if (grandchild != 0u) put(rt, next_copy + 0xCu, call(rt, ctx, address, {owner, grandchild, next_copy}));
        previous = next_copy;
        next = word(rt, next + 8u);
    }
    ctx.gpr[2] = copy;
    return copy;
}

} // namespace
} // namespace new_batch_main_194
} // namespace gt::game

#define NEW194(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_194::name, gt::ReturnKind::kind)
NEW194(0x08B56250u, func_08B56250, Word);
NEW194(0x08C52A3Cu, func_08C52A3C, Float);
NEW194(0x08C52B64u, func_08C52B64, Float);
NEW194(0x08C52C8Cu, func_08C52C8C, Float);
NEW194(0x08C52DB8u, func_08C52DB8, Float);

// ---- from main-session batch 195 (draft 8a575b7e26772b5da57dffff4fa857ddfc9273ab) ----
// New batch (main session 195): eighteen 77-81 instruction functions the earlier seeds emitted but no gate could run with
// useful inputs, written as structured C from `tools/disasm` (named locals, `G[n]` only where a call boundary needs a
// register). Delay slots are read as "before the call / branch": a value stored in a `jal`'s delay slot is the caller's,
// never the callee's result. `v0` ends as the asm leaves it on every exit, including the early returns that keep the
// incoming v0 or answer a leftover. Every argument register a callee reads is passed; the callee-saved registers the
// original fills (s0-s7, fp, f20-f31) are set where it sets them and restored on the way out.
// Not drafted here: 0x08B7527C, which registers its frame with 0x08B7470C (setjmp) and resumes through longjmp.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_195 {
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
// The same, with `v1` and `f0` too (a callee that never writes `v1` leaves it, and a float callee answers in `f0`).
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A call whose arguments are floats ($f12, ...) and whose answer may be in $f0.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
// The bytes at `address` in memory order as a big-endian word (the original's `lwl rt, 3(a)` / `lwr rt, 0(a)` pair).
u32 unaligned_word(Runtime &rt, u32 address) {
    return (static_cast<u32>(rt.memory().load8(address)) << 24) |
           (static_cast<u32>(rt.memory().load8(address + 1u)) << 16) |
           (static_cast<u32>(rt.memory().load8(address + 2u)) << 8) | rt.memory().load8(address + 3u);
}
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
// `div.s` of two zeroes is the PSP's default NaN 0x7FC00000, not the host's 0xFFC00000.
float psp_div(float a, float b) {
    if (a == 0.0f && b == 0.0f) return std::bit_cast<float>(0x7FC00000u);
    return a / b;
}

// s0-s7 and fp are callee-saved: the body sets them like the original and the caller's values come back.
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

// $f20-$f31 are callee-saved the same way.
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFloats() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// --- 0x08B636A0, 0x08B72830, 0x08B75DA0: one value of a serialized stream -------------------------------------------
// a0's low byte picks the encoding through the function's own jump table (0x08C95EF8 / 0x08C967C8 / 0x08C96B60, whose
// entries are the blocks below): kinds 0, 3 and 11 read a big-endian word and advance the cursor by 4, kinds 4 and 12 the
// same word and advance by 8, kind 1 a varint through `read_short`, kind 9 one through `read_long`, kind 2 an unsigned
// half and kind 10 a sign-extended one (each advancing by 2). A kind above 12 calls 0x08B77B04 first. A nonzero value is
// offset by `base` — by the incoming cursor when (a0 & 0x70) == 0x10 — and, when the low byte as a signed char is negative
// (the 0x80 flag), read once through that pointer; the answer goes to *destination and v0 is the new cursor.
u32 read_stream_value(Runtime &rt, AllegrexContext &ctx, u32 opcode, u32 base, u32 cursor, u32 destination,
                      u32 scratch, u32 read_short, u32 read_long) {
    const u32 selector = opcode & 0xFFu;
    const u32 kind = selector & 0xFu;
    u32 value = 0u;
    u32 next = cursor;
    if (kind >= 0xDu) call(rt, ctx, 0x08B77B04u, {opcode});
    switch (kind) {
    case 0u:
    case 3u:
    case 11u:
        value = unaligned_word(rt, cursor);
        next = cursor + 4u;
        break;
    case 4u:
    case 12u:
        value = unaligned_word(rt, cursor);
        next = cursor + 8u;
        break;
    case 1u:
    case 9u:
        call(rt, ctx, kind == 1u ? read_short : read_long, {cursor, scratch});
        value = word(rt, scratch);
        next = ctx.gpr[2];
        break;
    case 2u:
        value = (static_cast<u32>(rt.memory().load8(cursor + 1u)) << 8) | rt.memory().load8(cursor);
        next = cursor + 2u;
        break;
    case 10u: {
        const u32 raw = (static_cast<u32>(rt.memory().load8(cursor + 1u)) << 8) | rt.memory().load8(cursor);
        value = static_cast<u32>(static_cast<std::int16_t>(raw));
        next = cursor + 2u;
        break;
    }
    default:
        // Kinds 5..8 reach the same 0x08B77B04 call through their table entries and kinds 13..15 have no entry (the
        // original jumps past its table): no route reaches either, and the value stays 0.
        if (kind < 0xDu) call(rt, ctx, 0x08B77B04u, {opcode});
        break;
    }
    if (value != 0u) {
        const u32 chosen = (selector & 0x70u) == 0x10u ? cursor : base;
        ctx.gpr[18] = chosen;
        const u32 at = value + chosen;
        if (static_cast<s32>(static_cast<std::int8_t>(selector)) < 0) value = word(rt, at);
        else value = at;
    }
    ctx.gpr[4] = value;  // the delay slot of the store: a0 ends as the answer
    put(rt, destination, value);
    return next;
}

// --- 0x089CB3F4: one released range of a container triple ----------------------------------------------------------
// The words at base + 0, + 4 and + 8 are the begin, end and capacity pointers: `walk` runs over the elements
// (0x08C01F00 or 0x08BEEA84 with begin, end - begin and a flag in the frame), and a non-null begin is released with
// 0x08A5BC54(begin, bytes, 4) — the byte count of the 4-byte-element triples comes back from (end - begin) / 4 * 4.
void release_range(Runtime &rt, AllegrexContext &ctx, u32 base, u32 walk, u32 flag, bool word_elements) {
    const u32 begin = word(rt, base);
    const u32 end = word(rt, base + 4u);
    const u32 capacity = word(rt, base + 8u);
    call(rt, ctx, walk, {begin, end - begin, flag});
    if (begin != 0u) {
        u32 bytes = capacity - begin;
        if (word_elements) bytes = static_cast<u32>(static_cast<s32>(bytes) >> 2) << 2;
        call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
    }
}

// 0x08BE427C (a0 = a list head): every node of the circular list that word(a0) starts is released. A node's children
// (word(node + 0x18), the next one at child + 4) are each announced to 0x08BBD1B8(node + 0x18, sp + 4) — the answer's
// v0/v1 are copied over the block at sp + 0xC — and then released through the virtual at +4 of their vtable. The
// refcounted word at node + 0x10 drops its count at +8 and, at one, releases through its virtual +0xC. The text at
// node + 8, unless it is the empty marker 0x08D75298, goes to 0x08B60804(text - 4, -1) and, when that answers <= 0, to
// 0x08B93554(text - 0xC, sp); then the node itself goes to 0x08B35254. An empty list (word(a0) == a0) changes nothing.

// 0x088331F4 (a0 = a record to rename, a1 = a name, a2 = a count, a3 = a pointer to the object): with a positive count the
// object's virtual +0x54 is asked for a float (kept in f20), the name is turned into text by 0x088297A8(sp + 8, name)
// (0x08A5BB7C(0x08C9A2F8) reports a missing one), 0x088294D0(slot) -> 0x0885EDF8 gives the record and 1000 * f20
// truncated goes to its word +0x648 (the asm stores the integer itself, `swc1 $f1`). Otherwise 0x088297A8(sp + 4, name) ->
// 0x088294D0 -> 0x0885EDEC gives it, whose word +0x648 is taken as an integer, converted to a float and divided by 1000
// (0x08A87EEC(sp, that)); a0, when it is not sp, receives the text with 0x08A5BAD8. Both paths release their slot.

// 0x0886A13C (a0 = a record whose ring of 132-byte entries starts at +0x420C): 0x08875088(a0 + 8) runs once, then
// 0x08875090(a0 + 8) until it answers nonzero (v0 = 1). Each round locks a0 + 0x4210 (0x08B344E0) and takes the ring's
// tail halfword at +0x4202 and head halfword at +0x4200: with a difference it copies 0x7E bytes from the entry's +5 into
// sp, keeps the entry's first word (a1 of the next calls), bumps the tail (mod 0x80) and reads the entry's signed byte +4
// (the tag, -1 when the ring is empty); then it unlocks (0x08B3457C). A negative tag ends the loop (v0 = s6). Otherwise
// 0x08875108(a0 + 8) runs and, for tag 0, 0x088750AC(a0 + 8, sp) — for tag 1, 0x088750C8(a0 + 8, sp) — whose zero answer
// clears s6; any other tag keeps it. 0x08875108(a0 + 8, 0) closes the round.

// 0x08905BE0 (a0 = this): word(this) is stored back as this and the byte distance self - old word(self) is the argument
// of 21 member initialisers (0x08BD2BD8 for +4, +8, +0xC, +0x10, +0x14 and +0x78, 0x08BD2C68 for +0x18, 0x08BD2BF0 for
// +0x4C, 0x08BD2C94 for +0x64 and +0x68, 0x08BD2CB8 for +0x58, 0x08BD2C08 for +0x40, 0x08BD2CDC for +0x28..+0x3C,
// 0x08BD2D00 for +0x74, 0x08BD2C20 for +0x7C, 0x08BD2D24 for +0x80); 0x08BD2C38(this + 0x84, distance) finishes by a jump.
void func_08905BE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    ctx.gpr[17] = word(rt, self);
    put(rt, self, self);
    const u32 distance = self - ctx.gpr[17];
    ctx.gpr[17] = distance;
    call(rt, ctx, 0x08BD2BD8u, {self + 4u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 8u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0xCu, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0x10u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0x14u, distance});
    call(rt, ctx, 0x08BD2C68u, {self + 0x18u, distance});
    call(rt, ctx, 0x08BD2BF0u, {self + 0x4Cu, distance});
    call(rt, ctx, 0x08BD2C94u, {self + 0x64u, distance});
    call(rt, ctx, 0x08BD2C94u, {self + 0x68u, distance});
    call(rt, ctx, 0x08BD2CB8u, {self + 0x58u, distance});
    call(rt, ctx, 0x08BD2C08u, {self + 0x40u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x28u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x2Cu, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x30u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x34u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x38u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x3Cu, distance});
    call(rt, ctx, 0x08BD2D00u, {self + 0x74u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0x78u, distance});
    call(rt, ctx, 0x08BD2C20u, {self + 0x7Cu, distance});
    call(rt, ctx, 0x08BD2D24u, {self + 0x80u, distance});
    tail_call(ctx, 0x08BD2C38u, {self + 0x84u, distance});
}

// 0x0892A454 (a0 = a vector of 28-byte elements, a1 = a record, a2 and a3 unused): 0x0892A424(sp + 4, a1) prepares the
// visitor, then the record's index word(a1) is compared (unsigned) with (word(a0 + 4) - word(a0)) / 28: below it
// 0x08BDEE7C(a0, word(a0) + index * 28, word(a0 + 4)) runs, otherwise 0x08BDF434(a0, index - count, sp + 4). Both
// visitors are closed with 0x08BDD7C8(word(sp + 0x14), word(sp + 0x18), sp + 1) and 0x08BDD7B8(word(sp + 8),
// word(sp + 0xC), sp), each releasing its pointer (0x08B35254) when it is set. Finally, for each of the record's count
// word(a1) entries, 0x0892A37C(word(a0) + i * 28, word(word(a1 + 4) + i * 4)) runs; v0 is the count word when the loop is
// skipped, else the last call's answer.

// 0x0898B0E0 (a0 = an output string, a1 = a holder): 0x089896CC(sp + 0xC, a1) answers the record at sp + 0xC
// (0x08CA39F0 reports a missing one) whose floats +0x1EC (f20) and +0x1F0 (f21) are kept. 0x08A98C1C(sp + 8, 2) formats
// f20 into sp + 8, the output is assigned it (0x08A5BAD8 unless it is sp + 8) and sp + 8 is released. Then word(a0) —
// the holder object, 0x08CA39E8 reported when null — is asked for the virtual at +0x88 of its vtable: 0x08A87EEC(sp + 4,
// f20) and that virtual(holder, 0, sp + 4), sp + 4 released; the same with f21 into sp and the virtual(holder, 1, sp),
// then sp, sp + 8 and sp + 0xC released.

// 0x0899000C (a0 = this): bit 14 of word(this + 0xE0) is taken out and kept (fp), the words +0x220 and +0x224 cleared,
// the block +0x4A8..+0x4BC zeroed (the word +0x4B4 is 1), the byte +0x4C0 cleared, the bytes +0x1F4 and +0x166 set to 1,
// the byte +0x168 and the word +0x16C cleared, the word +0x120 set to 1 and 0x40000 ORed into +0xE0. Then each of the
// 0x80 slots from this + 0x228 gets 0x08D74FDC in sp + 4, 0x08B855E0(slot, sp + 4) and its text released (the empty
// marker 0x08D74FD0 skipped; 0x08B60804(text - 4, -1) and, when that answers <= 0, 0x08B8542C(text - 0xC, sp)). Bit 14
// comes back at the end, and v0 is the word stored at +0xE0.

// --- 0x089EC820: a printf-like formatter on a record --------------------------------------------------------------
// (a0 = the record, a1 = the format, a2..t3 = its arguments, spilled to sp + 0x238 as the va_list)
void format_record(Runtime &rt, AllegrexContext &ctx, u32 frame_base) {
    const u32 self = ctx.gpr[4];
    const u32 format = ctx.gpr[5];
    ctx.gpr[17] = self;
    ctx.gpr[19] = format;
    if (rt.memory().load8(self + 0x24u) != 0u) {
        if (static_cast<s32>(word(rt, self + 0x20u)) > 0) {
            u32 done = 0u;
            do {
                const u32 sink = word(rt, self + 0x10u);
                ++done;
                ctx.gpr[16] = sink;
                ctx.gpr[18] = done;
                if (sink == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA7CD0u});
                call(rt, ctx, 0x08A71AB8u, {sink, 9u});
                put(rt, self + 0x28u, word(rt, self + 0x28u) + 1u);
            } while (static_cast<s32>(done) < static_cast<s32>(word(rt, self + 0x20u)));
        }
    }
    const u32 args = frame_base + 0x238u;
    const u32 buffer = frame_base + 4u;
    ctx.gpr[20] = buffer;
    put(rt, frame_base, args);
    const u32 length = call(rt, ctx, 0x08B375C8u, {buffer, 0x200u, format, args});
    ctx.gpr[18] = length;
    const u32 sink = word(rt, self + 0x10u);
    ctx.gpr[16] = sink;
    if (sink == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA7CD0u});
    call_indirect(rt, ctx, word(rt, word(rt, sink) + 0x10Cu), {sink, buffer, length});
    put(rt, self + 0x28u, word(rt, self + 0x28u) + length);
    const u32 format_length = call(rt, ctx, 0x08B35F60u, {format});
    const u32 end = format + format_length;
    const u32 last = static_cast<u32>(signed_byte_at(rt, end - 1u));
    const u32 newline = (last ^ 0xAu) < 1u ? 1u : 0u;
    ctx.gpr[3] = newline;
    rt.memory().store8(self + 0x24u, static_cast<std::uint8_t>(newline));
    ctx.gpr[2] = end;
}

// 0x08A39E64 (a0 = this, a1 = a byte buffer): 0x08B35F60(a1) answers the length. Up to 10 times: the room left is
// 0x100 - word(this + 0x20); when the whole buffer fits it is copied into this + 0x14 under the lock at this + 0x124
// (0x08C1B60C) and 0x08B2E2B8(0x4E20) finishes by a jump; otherwise 0x08B35DA0(room) takes that much room, 0x08B7C830
// (copy, buffer, room) fills it, this + 0x14 gets it under the lock, 0x08B35254(copy) releases it, the buffer moves up and
// the loop asks again after 0x08B2E2B8(0x4E20); the tenth round returns without asking. v0 is the last ask otherwise.

// 0x08ACECFC (a0 = this): the constructor of a large record. The vtable 0x08CD6B28 goes to word(this); 0x08B2EE10
// (this + 4, 2, 0x300), 0x08B2EE10 (this + 0x18, 2, 0x303) and 0x08B2EDD4 (this + 0x2C, 0x08CB49D4) set three members up,
// 0x08B0CC04(this + 0x40) another, the bytes +0x160 and +0x162 are cleared and 0x08B28DCC(this + 0x164) runs.
// word(this + 0x244) = this + 0x240, the words +0x300, +0x30C, +0x314, +0x31C, +0x328, +0x32C and +0x330 are cleared,
// 0x08B28D34 runs on +0x334, +0x360, +0x38C, +0x448 and +0x474, 0x08B28DCC on +0x3B8, +0x420 and +0x4A0,
// word(this + 0x510) = 4, +0x514 = -1 and +0x518 = 0, 0x08B2EEB8 runs on +4, +0x18 and +0x2C, 0x08C7AD70 runs and
// word(this + 0x240) is cleared. v0 is 0x08C7AD70's answer.

// --- 0x08B6C020: the library square root on single floats ----------------------------------------------------------
// (f12 = x) |x| above 0x7F7FFFFF (infinity or a NaN) answers x + x * x; +-0 answers x; a negative x answers
// (x - x) / (x - x), the PSP's default NaN. Otherwise the biased exponent (a denormal is shifted up first, counting the
// shifts) and the 24-bit mantissa give 25 rounds of the restoring square root; a remainder left over rounds the result up
// when its last bit is set; the result bits are (root >> 1) + 0x3F000000 + (half exponent << 23).
void sqrt_bits(Runtime &rt, AllegrexContext &ctx) {
    (void)rt;
    const float x = ctx.fpr[12];
    const u32 bits = std::bit_cast<u32>(x);
    const u32 magnitude = (bits << 1) >> 1;  // `ins v1, zero, 31, 1`: the sign cleared
    if (magnitude > 0x7F7FFFFFu) {
        const float square = psp_mul(x, x);
        ctx.fpr[12] = x + square;
        ctx.fpr[0] = ctx.fpr[12];
        return;
    }
    if (magnitude == 0u) {
        ctx.fpr[0] = x;
        return;
    }
    if (static_cast<s32>(bits) < 0) {
        const float zero = x - x;
        ctx.fpr[12] = psp_div(zero, zero);
        ctx.fpr[0] = ctx.fpr[12];
        return;
    }
    s32 exponent = static_cast<s32>(bits) >> 23;
    u32 mantissa = bits;
    if (magnitude <= 0x7FFFFFu) {
        u32 shifts = 0u;
        if ((bits & 0x800000u) == 0u) {
            do {
                mantissa <<= 1;
                ++shifts;
            } while ((mantissa & 0x800000u) == 0u);
        }
        exponent = exponent - static_cast<s32>(shifts) + 1;
    }
    u32 scaled = (mantissa & 0x7FFFFFu) | 0x800000u;
    const s32 unbiased = exponent - 0x7F;
    if ((unbiased & 1) != 0) scaled <<= 1;
    u32 remainder = scaled << 1;
    const s32 half = unbiased >> 1;
    u32 root = 0u;
    u32 answer = 0u;
    u32 step = 0x1000000u;
    for (u32 rounds = 0x19u; rounds != 0u; --rounds) {
        const u32 trial = root + step;
        if (!(static_cast<s32>(remainder) < static_cast<s32>(trial))) {
            remainder -= trial;
            answer += step;
            root = trial + step;
        }
        remainder <<= 1;
        step >>= 1;
    }
    if (remainder != 0u) answer += answer & 1u;
    const u32 result =
        static_cast<u32>(static_cast<s32>(answer) >> 1) + 0x3F000000u + (static_cast<u32>(half) << 23);
    ctx.fpr[12] = std::bit_cast<float>(result);
    ctx.fpr[0] = ctx.fpr[12];
}

// --- 0x08B31D40: one block of a save channel -----------------------------------------------------------------------
// The 0x80 bytes from `source` go to `destination` in 16-byte blocks (the original's aligned `lw`/`sw` pairs).
void copy_block(Runtime &rt, u32 destination, u32 source) {
    for (u32 offset = 0u; offset != 0x80u; offset += 0x10u) {
        put(rt, destination + offset, word(rt, source + offset));
        put(rt, destination + offset + 4u, word(rt, source + offset + 4u));
        put(rt, destination + offset + 8u, word(rt, source + offset + 8u));
        put(rt, destination + offset + 0xCu, word(rt, source + offset + 0xCu));
    }
}

// 0x08B31D40 (a0 = an object, a1 = a byte for a flag, a2 = a block of 0x80 bytes, t0 = a word): the object's virtual
// +0x64 must answer nonzero. Then channel 0xD reads a block into sp: its first word must be 1, which also goes to *a1 (a
// word that is not 1 ends the call with v0 = 1). Channel 0xE fills the block again and its 0x80 bytes are copied to a2
// (16-byte words when a2 is 4-aligned, bytes otherwise), then channel 0xF's first halfword goes to *t0. v0 = 1 on success
// and 0 when a channel answers negative.
void func_08B31D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x90u);
    SavedRegisters saved(ctx);
    const u32 object = ctx.gpr[4];
    const u32 flag_out = ctx.gpr[5];
    const u32 block = ctx.gpr[6];
    const u32 word_out = ctx.gpr[8];
    ctx.gpr[18] = word_out;
    ctx.gpr[17] = block;
    ctx.gpr[16] = flag_out;
    if (call_indirect(rt, ctx, word(rt, word(rt, object) + 0x64u), {object}) == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    const auto channel = [&](u32 number) {
        return static_cast<s32>(call(rt, ctx, 0x08C7AF20u, {number, frame.at(0u)})) >= 0;
    };
    u32 result = 0u;
    if (channel(0xDu)) {
        const u32 present = word(rt, frame.at(0u)) == 1u ? 1u : 0u;
        rt.memory().store8(flag_out, static_cast<std::uint8_t>(present));  // the store sits in the branch's delay slot
        result = 1u;
        if (present != 0u && channel(0xEu)) {
            if ((block & 3u) == 0u) {
                copy_block(rt, block, frame.at(0u));
            } else {
                for (u32 offset = 0u; offset != 0x80u; ++offset)
                    rt.memory().store8(block + offset, rt.memory().load8(frame.at(0u) + offset));
            }
            if (channel(0xFu)) put(rt, word_out, rt.memory().load16(frame.at(0u)));
            else result = 0u;
        }
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// 0x08B42710 (a0 = this, a1 = a mode byte): a 64-bit stamp throttle. The object at word(this + 0xB0) is asked for the
// stamp in (v0, v1) through the virtual at +0x1C of its vtable; the distance from the words at +0x918 / +0x91C becomes a
// magnitude (its sign bit applied to both halves, then a two's-complement negation with a borrow). A nonzero mode, a
// distance whose high word is positive, or a low word from 0x4E21 on runs the work at once; a smaller one asks the object
// at word(this + 0x930) and the work runs only when its word +0x644 is not 0x64 and the low word reaches at least 0xBB9,
// else v0 is that object (or the leftover comparison's answer). The work puts 2 in the frame's word +0x14, calls
// 0x08B47AD4(sp, 0x10), takes 0x08B47B40(sp, 0) for a record and fills its words 0, 4, 8 and 0xC with 0, 0, the stamp's v0
// and its v1, records the stamp at +0x918 / +0x91C, calls 0x08B426A8(this, this's virtual +0x94, sp, 1) and ends the frame
// with 0x08B47ACC(sp); v0 is that answer.

// --- 0x08B636A0, 0x08B72830, 0x08B75DA0: the four-argument wrappers of read_stream_value -------------------------
// (a0 = the kind byte, a1 = the base, a2 = the cursor into the serialized stream, a3 = where the value goes)



// 0x089CB3F4 (a0 = this): the destructor of a record with four container triples. The vtable 0x08CC9FD8 goes to word(this)
// and 0x08CCA230 to word(this + 0x38); the triples at +0x150 and +0x144 are walked by 0x08C01F00 and those at +0x138 and
// +0x12C by 0x08BEEA84 (the flag each one writes sits at sp + 3, + 2, + 1 and + 0), each released through 0x08A5BC54 with
// 4-byte elements for the latter two; 0x08A0DD40(this) finishes.

// 0x08C64BB4 (a0 unused, a1 and a2 = two word pairs, a3 = a byte flag, t0, t1, t2): the pairs' words go to the block at
// sp + 0x14 / sp + 0x1C, sp + 8 gets the empty marker 0x08D774A4 and 0x08C637BC (flag set) or 0x08C61C54 (clear) is called
// with (a0, sp + 0x1C, sp + 0x14, t0, t1, sp + 8); its (v0, v1) go over the blocks at sp + 0x1C and sp + 0x30. Then
// word(sp + 8) is kept, 0x08B5C230() answers the word stored at sp + 4, 0x08B61A80(kept, t2, t1, sp + 4) runs and the pair
// at sp + 0x1C is copied to sp + 0xC: unless word(sp + 8) - 0xC is still the marker, 0x08B60804(word(sp + 8) - 4, -1) and,
// when that answers <= 0, 0x08C74884(word(sp + 8) - 0xC, sp) release it. v0 / v1 are the pair at sp + 0xC / sp + 0x10.
void func_08C64BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(ctx);
    const u32 own_a0 = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    ctx.gpr[21] = 0x08D77498u;
    ctx.gpr[20] = ctx.gpr[10];
    ctx.gpr[17] = ctx.gpr[9];
    put(rt, frame.at(0x1Cu), word(rt, first));
    put(rt, frame.at(0x20u), word(rt, first + 4u));
    put(rt, frame.at(0x14u), word(rt, second));
    put(rt, frame.at(0x18u), word(rt, second + 4u));
    put(rt, frame.at(8u), ctx.gpr[21] + 0xCu);  // the marker block + 0xC
    const u32 convert = flag != 0u ? 0x08C637BCu : 0x08C61C54u;
    const GuestResult pair =
        call_result(rt, ctx, convert, {own_a0, frame.at(0x1Cu), frame.at(0x14u), ctx.gpr[8], ctx.gpr[9], frame.at(8u)});
    put(rt, frame.at(0x1Cu), pair.v0);
    put(rt, frame.at(0x20u), pair.v1);
    put(rt, frame.at(0x30u), pair.v0);
    put(rt, frame.at(0x34u), pair.v1);
    const u32 kept = word(rt, frame.at(8u));
    ctx.gpr[16] = kept;
    const u32 answer = call(rt, ctx, 0x08B5C230u, {});
    put(rt, frame.at(4u), answer);
    call(rt, ctx, 0x08B61A80u, {kept, ctx.gpr[20], ctx.gpr[17], frame.at(4u)});
    const u32 holder = word(rt, frame.at(8u));
    put(rt, frame.at(0xCu), word(rt, frame.at(0x1Cu)));
    put(rt, frame.at(0x10u), word(rt, frame.at(0x20u)));
    if (holder - 0xCu != 0x08D77498u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {holder - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08C74884u, {holder - 0xCu, frame.at(0u)});
    }
    ctx.gpr[2] = word(rt, frame.at(0xCu));
    ctx.gpr[3] = word(rt, frame.at(0x10u));
}



} // namespace
} // namespace new_batch_main_195
} // namespace gt::game

#define NEW195(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_195::name, gt::ReturnKind::kind)
NEW195(0x08905BE0u, func_08905BE0, Word);
NEW195(0x08B31D40u, func_08B31D40, Word);
NEW195(0x08C64BB4u, func_08C64BB4, DoubleWord);

// ---- from main-session batch 196 (draft 6a8432d484ec434210ffeae8580f1f294163a339) ----
// New batch main 196 (main session): functions the recompiler emitted that no src/ file defines or registers, written by
// hand from `tools/disasm` as structured C. DRAFT: not verified by the differential gate yet, so nothing below is
// registered as done.
//
// Twelve of the seventeen functions of the batch are drafted below; the other five are listed at the end of this comment
// with the reason. Most of the twelve dereference an argument register, which is why the gate never got a comparable case
// out of them: a pointer register the synthetic generator draws at random faults on the first load. The input shapes
// that give each struct a valid address are written beside each body and collected in
// /tmp/claude-1000/shapes_main196.txt (format:
// `ADDRESS reg:aN=struct:SIZE [field:+OFFSET=ptr|int:LO..HI]...`, host/synth_case.cpp parse_shapes).
//
// Read but NOT drafted, and why (left for a batch that can model them):
// - 0x08A63C5C: a four-state machine that jumps to its own handlers through a table of code addresses it writes into
//   0x08CFF278 on the first call (`jr v1` into its own middle), and the emitted extent stops at 0x08A63DBC while the
//   handler at 0x08A63D90 and the states after it belong to the same body. No `if`/`switch` body reproduces jumps into
//   its own middle without inventing entry points the original does not have.
// - 0x08B65A7C: every path ends in func_08B77B04, which has no `jr $ra` after its last call (it switches two library
//   functions and returns to nobody). A differential case can never compare a result.
// - 0x08ADCF48: a 64-bit clamp whose middle is a signed 64-bit division (func_08B6FBA4) on values built with mixed
//   `slt` / `sltu` and a 32x32->64 `mult` carry; skipped rather than guessed.
// - 0x08B01934, 0x08B0225C: libstdc++ vector inserts whose iterator block is built from *uninitialised* frame words
//   (sp + 0x38 and sp + 0x48 are read into the block handed to func_08C46854 / func_08C47DFC) and whose callee needs a
//   consistent begin/end/capacity triple; the synthetic arena cannot express that, and a guess would diverge on data
//   the original never writes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_196 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// The callee-saved registers a body writes are set where the original sets them (a callee may read them) and the
// caller's values come back when the body ends, as the original's prologue / epilogue pair does.
class Saved {
public:
    Saved(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx), count_(registers.size()) {
        unsigned index = 0u;
        for (const unsigned reg : registers) {
            registers_[index] = reg;
            saved_[index] = ctx.gpr[reg];
            ++index;
        }
    }
    ~Saved() {
        for (unsigned index = 0u; index < count_; ++index) ctx_.gpr[registers_[index]] = saved_[index];
    }
    Saved(const Saved &) = delete;
    Saved &operator=(const Saved &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned count_;
    std::array<unsigned, 8> registers_{};
    std::array<u32, 8> saved_{};
};

// A guest call with the argument registers written as the original writes them before its `jal` (a0-a3, t0-t3 in
// order, then $f12 upwards); afterwards v0 / v1 / f0 / a1 hold what the callee left, as after the call.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float value : floats) {
        if (freg <= 19u) ctx.fpr[freg++] = value;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_value(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
// `lw v1, 0(object); lw v0, slot(v1); jalr v0`: v1 = the vtable, v0 = the callee's address (what a callee that never
// writes v0 hands back), a0 from `arguments`.
GuestResult call_member(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_full(rt, ctx, function, arguments);
}
void copy_bytes(Runtime &rt, u32 destination, u32 source, u32 count) {
    for (u32 index = 0u; index < count; ++index) rt.memory().store8(destination + index, rt.memory().load8(source + index));
}

// 0x088DF4F8 (a0 = record): record[0] picks one of the 0x2F8-byte entries hanging off record[1] + 0xC0 (index * 760).
// While that entry's byte +0x11 is set the function waits, and every poll is func_08B1A844() followed by a
// func_08B2E2B8(0x1F4) sleep; a non-zero func_08B1A844 answer ends the function with that answer. Then the queue object
// record[1] + 0xF0 is polled with func_08B3453C (sleeping the same way while it answers 0); once it answers, the
// object's virtual slot 0x2C runs with (object, index), func_08B344E0 rolls the state at record[1] + 0xC8 over, the
// entry is marked 3 at +0x2C8, func_088D9C24(record[1], index, +0x90, +0x94) fills the pair at +0x2D0 / +0x2D4, the byte
// +0x2B4 is cleared and func_08B3457C(state) runs. A non-null record[1][0x14C] gets func_088F8A20(that, index); the
// function ends with a tail call of func_08B3457C(record[1] + 0xF0).

// 0x08942548 (a0 = object, a1 = mode): objects a0 + k*0x268 carry a float at +8 that the function scales. Mode 0 walks
// slots 0..2, mode 1 slots 3..5, any other mode slots 0..5 (the loop bound and the start index both come from the mode
// switch). A slot whose float is exactly 0 is left alone; otherwise the float is put through the call pair
// func_08946C58(object, 2*(i/3), 0.5 * float(object + 0x13D0 + 4*(i/3)) + float(object + 0x3A4 + 116*(i/3))) and
// func_089467AC(slot + 0x22C, result) and the product with the slot's old float goes back to +8. Finally the byte at
// +0x13E2 becomes 1 unless the floats at +0x4B0 and +0xBE8 are equal (0 then). v0 is whatever the last call returned on
// the last body iteration, or i >> 31 when the last iteration skipped the body, or 1 on the not-equal end.
void func_08942548(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    Saved saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u});
    GuestFrame frame(ctx, 0x30u);
    u32 first = 0u;
    u32 count = 3u;
    if (mode != 0u) {
        first = 3u;
        count = 6u;
        if (mode != 1u) first = 0u;
    }
    ctx.gpr[18] = object;
    ctx.gpr[19] = count;
    ctx.gpr[17] = first;
    ctx.gpr[20] = first;
    ctx.gpr[16] = object + first * 0x268u + 0x4A0u;
    u32 result = 0x268u;
    u32 index = first;
    u32 slot = object + first * 0x268u + 0x4A0u;
    while (index != count) {
        const float stored = float_at(rt, slot + 8u);
        result = static_cast<u32>(static_cast<s32>(index) >> 31);
        if (stored != 0.0f) {
            const s32 group = static_cast<s32>(index) / 3;
            float argument = psp_mul(float_at(rt, object + 0x13D0u + static_cast<u32>(group) * 4u), 0.5f);
            argument = argument + float_at(rt, object + 0x3A4u + static_cast<u32>(group) * 116u);
            const GuestResult mixed = call_full(rt, ctx, 0x08946C58u, {object, static_cast<u32>(group) * 2u}, {argument});
            const float before = float_at(rt, slot + 8u);  // read before the second call, as the original does
            const GuestResult scaled = call_full(rt, ctx, 0x089467ACu, {slot + 0x22Cu}, {mixed.f0});
            result = scaled.v0;
            put_float(rt, slot + 8u, psp_mul(before, scaled.f0));
        }
        index += 1u;
        slot += 0x268u;
    }
    const bool equal = float_at(rt, object + 0x4B0u) == float_at(rt, object + 0xBE8u);
    rt.memory().store8(object + 0x13E2u, 0u);
    if (equal) {
        ctx.gpr[2] = result;
    } else {
        rt.memory().store8(object + 0x13E2u, 1u);
        ctx.gpr[2] = 1u;
    }
}

// 0x08B2A0EC (a0, a1, a2 = float outputs, $f12 = phase, $f13 = weight, $f14 = amplitude): a three-way blend whose branch
// is trunc(phase * 6) mod 6, with phase wrapped into [0, 1] first. A zero weight stores the amplitude in all three
// outputs. Otherwise g = trunc(phase*6)* - style fraction, a = amplitude*(1 - weight), and the two inner values are
// amplitude*(1 - weight*(1-g)) and amplitude*(1 - weight*g); the remainder picks which of {amplitude, a, those two} goes
// where, and case 5 writes the third output twice (amplitude, then its own value), as the table does. v0 keeps the
// table entry address (+4*remainder) or 0 when the remainder is negative.

// 0x08965ED4 (a0 = object, a1 = selector): the nine-entry table at 0x08C8B5E0 writes three floats at +0xD4, +0xD8 and
// +0xDC of the object (selector 3 uses the out-of-range default). Only one selector's path stores the fields in a
// different order; the values are 45.0, 22.5, 0.5, 1.0, -45.0 and -22.5, and a selector of 9 or more takes the default
// (-22.5, 1.0, 1.0). v0 keeps the table entry address (+4*selector, 0 out of range).
void func_08965ED4(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 selector = ctx.gpr[5];
    if (selector >= 9u) {
        put_float(rt, object + 0xDCu, -22.5f);
        put_float(rt, object + 0xD8u, 1.0f);
        put_float(rt, object + 0xD4u, 1.0f);
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = 0x08C8B5E0u + 4u * selector;
    switch (selector) {
    case 0u:
        put_float(rt, object + 0xDCu, 45.0f);
        put_float(rt, object + 0xD8u, 0.5f);
        put(rt, object + 0xD4u, 0u);
        break;
    case 1u:
        put_float(rt, object + 0xDCu, 22.5f);
        put_float(rt, object + 0xD8u, 1.0f);
        put(rt, object + 0xD4u, 0u);
        break;
    case 2u:
        put_float(rt, object + 0xD4u, 0.5f);
        put_float(rt, object + 0xD8u, 1.0f);
        put(rt, object + 0xDCu, 0u);
        break;
    case 4u:
        put_float(rt, object + 0xDCu, -45.0f);
        put_float(rt, object + 0xD4u, 1.0f);
        put_float(rt, object + 0xD8u, 0.5f);
        break;
    case 5u:
        put_float(rt, object + 0xDCu, -45.0f);
        put_float(rt, object + 0xD4u, 1.0f);
        put(rt, object + 0xD8u, 0u);
        break;
    case 6u:
        put_float(rt, object + 0xDCu, 45.0f);
        put_float(rt, object + 0xD4u, 0.5f);
        put(rt, object + 0xD8u, 0u);
        break;
    case 7u:
        put_float(rt, object + 0xDCu, 45.0f);
        put(rt, object + 0xD4u, 0u);
        put(rt, object + 0xD8u, 0u);
        break;
    case 8u:
        put_float(rt, object + 0xDCu, -22.5f);
        put_float(rt, object + 0xD8u, 0.5f);
        put_float(rt, object + 0xD4u, 0.5f);
        break;
    default:  // 3 and the table's own out-of-range entry
        put_float(rt, object + 0xDCu, -22.5f);
        put_float(rt, object + 0xD8u, 1.0f);
        put_float(rt, object + 0xD4u, 1.0f);
        break;
    }
}

// 0x089B439C (a0 = owner, a1 = record): a holder is built at sp + 8 by func_08A0387C; the four-word reader at sp + 0xC =
// {0x08CCED90, D_08D74FD0 + 0xC, -1, sp + 8} goes to the virtual slot 8 of the record (a1 = sp + 0xC). The slot's answer
// at sp + 0x10 (the cell) is then either the shared empty cell D_08D74FD0 or a real cell: the reader's vtable word is set
// to 0x08CCD990 and a real cell is released (func_08B60804(cell - 4, -1) and, when that answer is not positive,
// func_08B8542C(cell - 0xC, sp)). Then func_089B396C(owner, sp + 8), func_08966040(owner, 2); the object h = sp[8] (a null
// one asserts through func_08A5BB7C(0x08CA5670) first) calls its virtual slot 0x12C, func_089C9C34(sp + 4) builds the
// second holder and the object g = sp[4] (null: func_08A5BB7C(0x08CA56DC)) takes func_089C8294(g, sp + 8). Both holders
// are destroyed with func_08A5B9EC, sp + 4 first, and v0 is that second call's answer.
void func_089B439C(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 record = ctx.gpr[5];
    Saved saved(ctx, {16u, 17u, 18u, 19u, 20u});
    GuestFrame frame(ctx, 0x40u);
    constexpr u32 kEmpty = 0x08D74FD0u;
    const u32 at4 = frame.at(4u), at8 = frame.at(8u), atC = frame.at(0xCu);
    ctx.gpr[16] = kEmpty;
    ctx.gpr[19] = owner;
    ctx.gpr[20] = at4;
    ctx.gpr[18] = at8;
    ctx.gpr[17] = record;
    call_value(rt, ctx, 0x08A0387Cu, {at8});
    put(rt, frame.at(0x10u), kEmpty + 0xCu);
    put(rt, frame.at(0x14u), 0xFFFFFFFFu);
    put(rt, atC, 0x08CCED90u);
    put(rt, frame.at(0x18u), at8);
    call_member(rt, ctx, record, 8u, {record, atC});
    const u32 cell = word(rt, frame.at(0x10u));
    ctx.gpr[17] = cell - 0xCu;
    put(rt, atC, 0x08CCD990u);
    if (cell - 0xCu != kEmpty) {
        const s32 count = static_cast<s32>(call_value(rt, ctx, 0x08B60804u, {cell - 4u, 0xFFFFFFFFu}));
        if (count <= 0) call_value(rt, ctx, 0x08B8542Cu, {cell - 0xCu, frame.at(0u)});
    }
    call_value(rt, ctx, 0x089B396Cu, {owner, at8});
    call_value(rt, ctx, 0x08966040u, {owner, 2u});
    const u32 first = word(rt, at8);
    if (first == 0u) call_value(rt, ctx, 0x08A5BB7Cu, {0x08CA5670u});
    call_member(rt, ctx, first, 0x12Cu, {first});
    call_value(rt, ctx, 0x089C9C34u, {at4});
    const u32 second = word(rt, at4);
    if (second == 0u) call_value(rt, ctx, 0x08A5BB7Cu, {0x08CA56DCu});
    call_value(rt, ctx, 0x089C8294u, {second, at8});
    call_value(rt, ctx, 0x08A5B9ECu, {at4});
    call_value(rt, ctx, 0x08A5B9ECu, {at8});
}

// 0x08B3E45C (a0 = object, then eleven more arguments, four of them on the caller's stack): builds the record at a0.
// Stack argument at +0 is a u16 half (tenth), +4 a byte (eleventh), +8 a signed half (twelfth) and +0xC a word
// (thirteenth); a1 is stored sign-extended as a half at +0, t2 / t3 are cut to bytes at +0x14 / +0x15, t0 goes to +0xC,
// t1 to +0x10 and the stack word to +0x5C. func_08B3E280(object) runs first, then two func_08B2CB18(stack_word, t0, 0x10)
// blocks whose answers func_08B36018 zeroes; a non-null a3 is copied in with func_08B79A88 instead. The stack half goes
// to +0x16, the signed stack half to +0x28 and a non-null a2 is copied in with func_08B7C830(object + 0x34, a2, 0x20).
// v0 is func_08B7C830's answer when a2 is non-null, the func_08B36018 answer otherwise.

// 0x08B82420 (no arguments): a one-shot initializer of the memory-pool globals. A non-zero word at 0x08D72E4C means it
// already ran and the function answers 0x80440001. Otherwise it stores the pool descriptors 0x08D72E54 and 0x08D72EC4 at
// 0x08D740E4 / 0x08D740E0, clears 0x1FF bytes at 0x08D73CD4 and 0x1FF at 0x08D73ED4 (the loop stops one short of 0x200),
// creates the 0x1C0-byte block at 0x08D740D8 with func_08C7ABB0(-1, 0x1C0, 0) and three 0x100-byte blocks at
// 0x08D740EC + 4*i. A negative answer unwinds: the blocks 0..i-1 and then the first one are released with func_08C7ABB8,
// and the answer is 0x80440001 as well. Success stores 1 at 0x08D72E4C and answers 0.
void func_08B82420(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx, {16u, 17u, 18u, 19u, 20u});
    GuestFrame frame(ctx, 0x20u);
    constexpr u32 kSentinels = 0x08D72E4Cu;
    constexpr u32 kFailure = 0x80440000u | 1u;
    if (word(rt, kSentinels) != 0u) {
        ctx.gpr[2] = kFailure;
        return;
    }
    put(rt, 0x08D740E4u, 0x08D72E54u);
    put(rt, 0x08D740E0u, 0x08D72EC4u);
    for (u32 index = 0u; index + 1u < 0x200u; ++index) rt.memory().store8(0x08D73CD4u + index, 0u);
    for (u32 index = 0u; index + 1u < 0x200u; ++index) rt.memory().store8(0x08D73ED4u + index, 0u);
    const u32 first = call_value(rt, ctx, 0x08C7ABB0u, {0xFFFFFFFFu, 0x1C0u, 0u});
    put(rt, 0x08D740D4u + 4u, first);
    if (static_cast<s32>(first) < 0) {
        ctx.gpr[2] = kFailure;
        return;
    }
    for (u32 index = 0u;; ++index) {
        const u32 block = call_value(rt, ctx, 0x08C7ABB0u, {0xFFFFFFFFu, 0x100u, 0u});
        put(rt, 0x08D740ECu + 4u * index, block);
        if (static_cast<s32>(block) < 0) {
            for (s32 release = static_cast<s32>(index) - 1; release >= 0; --release)
                call_value(rt, ctx, 0x08C7ABB8u, {static_cast<u32>(release)});
            call_value(rt, ctx, 0x08C7ABB8u, {first});
            ctx.gpr[2] = kFailure;
            return;
        }
        if (index + 1u >= 3u) break;
    }
    put(rt, kSentinels, 1u);
    ctx.gpr[2] = 0u;
}

// 0x08AEF714 (a0 = self, a1 = event, a2 = scope): with event 0 the node at scope + 0x48 picks the report: its word +0 is 1
// and the drop is func_08AED98C, 2 and it is func_08AED9D0 (with word(node + 4) + scope[0x2C] as the fourth argument),
// anything else clears bits 1, 4 and 0x10 of self[4] and answers the new byte. Those calls get (scope[0], scope[0x24],
// scope[0x20], the low sum, scope[0x28], scope[0x40] when scope[0x38] is not negative else -1, scope[0x4C]) and their
// answer is stored at self + 0x1C; a negative answer ends the function, otherwise the byte at self[4] is masked with 0xED
// and that is the answer. With an event, a negative handle at self + 0x1C ends the function (v0 = event < 7), and events
// 1..6 go through the table at 0x08C92574: 1 does nothing, 2 and 3 call func_08AEDAC4(handle) and set the handle to -1,
// 4 runs func_08AEF1D0(self, scope), 5 stores (u16)scope at 0x08D41EF4 + 2*handle and runs func_08AEDBF8(handle, scope),
// 6 runs func_08AEDC44(handle, scope).
void func_08AEF714(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 event = ctx.gpr[5];
    const u32 scope = ctx.gpr[6];
    Saved saved(ctx, {16u});
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    if (event == 0u) {
        const u32 node = word(rt, scope + 0x48u);
        const u32 kind = word(rt, node);
        if (kind != 1u && kind != 2u) {
            const u32 flags = rt.memory().load8(self + 4u) & 0xEDu;
            rt.memory().store8(self + 4u, static_cast<std::uint8_t>(flags));
            ctx.gpr[2] = flags;
            return;
        }
        const u32 limit = word(rt, scope + 0x38u);
        const u32 high = static_cast<s32>(limit) < 0 ? 0xFFFFFFFFu : word(rt, scope + 0x40u);
        const u32 sum = (kind == 2u ? word(rt, node + 4u) : word(rt, node + 8u)) + word(rt, scope + 0x2Cu);
        const u32 answer = kind == 2u
                               ? call_value(rt, ctx, 0x08AED9D0u,
                                            {word(rt, scope), word(rt, scope + 0x24u), word(rt, scope + 0x20u),
                                             word(rt, node + 8u), sum, word(rt, scope + 0x28u), high, word(rt, scope + 0x4Cu)})
                               : call_value(rt, ctx, 0x08AED98Cu,
                                            {word(rt, scope), word(rt, scope + 0x24u), word(rt, scope + 0x20u), sum,
                                             word(rt, scope + 0x28u), high});
        put(rt, self + 0x1Cu, answer);
        if (static_cast<s32>(answer) < 0) {
            ctx.gpr[2] = answer;
            return;
        }
        const u32 flags = rt.memory().load8(self + 4u) & 0xEDu;
        rt.memory().store8(self + 4u, static_cast<std::uint8_t>(flags));
        ctx.gpr[2] = flags;
        return;
    }
    const u32 handle = word(rt, self + 0x1Cu);
    const u32 in_range = event < 7u ? 1u : 0u;
    ctx.gpr[2] = in_range;
    if (static_cast<s32>(handle) < 0) return;
    if (in_range == 0u) return;
    ctx.gpr[2] = 0x08C92574u + 4u * event;
    switch (event) {
    case 1u:
        return;
    case 2u:
    case 3u:
        call_value(rt, ctx, 0x08AEDAC4u, {handle});
        put(rt, self + 0x1Cu, 0xFFFFFFFFu);
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    case 4u:
        call_value(rt, ctx, 0x08AEF1D0u, {self, scope});
        return;
    case 5u:
        rt.memory().store16(0x08D41AD0u + 0x424u + 2u * handle, static_cast<std::uint16_t>(scope));
        call_value(rt, ctx, 0x08AEDBF8u, {handle, scope});
        return;
    default:
        call_value(rt, ctx, 0x08AEDC44u, {handle, scope});
        return;
    }
}

// 0x08A3FE08 (a0 = destination record): copy-constructs the 0xED bytes at source + 0x18 (source = sp[0], built by
// func_08A3F76C) into a0 + 0x18 after func_08A3F988 built sp[4], the record that receives them. Both are checked non-null
// with func_08A5BB7C(0x08CADA50 / 0x08CADA84). The source's member +0x14 has its virtual slot 0x10 called and that answer
// becomes the destination's +0x14. Then func_08A5BAD8(destination, sp[4]) runs when destination != sp[4], and both
// temporary records are released with func_08A5B9EC (v0 is the second release's answer).

// 0x0894419C (no arguments): fills the timer tables. Six bytes at 0x08CF2AF0 get 0, six at 0x08CF2AE8 get 10 and six at
// 0x08CF2AE0 get 1; the loop walks the index 1, 2, 3 and again 5, 6, 7 (it increments once more before testing `>= 6`),
// which is exactly six stores per table. Then func_0894F158() and func_08944304() fill the level tables. Eleven rounds
// store 0.01 * (50 + 10k - 40) at 0x08CF2B8C + 4k, 0.01 * (50 + 10k) at 0x08CF2BBC + 4k and 0.01 * (44 + 7k) at both
// 0x08CF2B2C + 4k and 0x08CF2B5C + 4k; the last round clears 0x08CF2AF8 and stores k/10 at 0x08CF2AFC + 4k for k = 0..10.
// v0 is 0.
void func_0894419C(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx, {31u});
    GuestFrame frame(ctx, 0x10u);
    for (u32 index = 0u; index < 6u; ++index) rt.memory().store8(0x08CF2AF0u + index, 0u);
    for (u32 index = 0u; index < 6u; ++index) rt.memory().store8(0x08CF2AE8u + index, 0x0Au);
    for (u32 index = 0u; index < 6u; ++index) rt.memory().store8(0x08CF2AE0u + index, 1u);
    call_value(rt, ctx, 0x0894F158u);
    call_value(rt, ctx, 0x08944304u);
    for (u32 round = 0u; round < 11u; ++round) {
        const u32 high = 50u + 10u * round;
        const u32 low = 44u + 7u * round;
        const float third = psp_mul(static_cast<float>(high - 40u), 0.01f);
        const float second = psp_mul(static_cast<float>(high), 0.01f);
        const float first = psp_mul(static_cast<float>(low), 0.01f);
        put_float(rt, 0x08CF2B8Cu + 4u * round, third);
        put_float(rt, 0x08CF2BBCu + 4u * round, second);
        put_float(rt, 0x08CF2B2Cu + 4u * round, first);
        put_float(rt, 0x08CF2B5Cu + 4u * round, first);
    }
    put(rt, 0x08CF2AF8u, 0u);
    for (u32 index = 0u; index < 11u; ++index)
        put_float(rt, 0x08CF2AF8u + 4u + 4u * index, static_cast<float>(index) / 10.0f);
    ctx.gpr[2] = 0u;
}

// 0x08A4FA18 (a0 = record, a1 / a2 = arguments of func_08A4F8B4): builds the index array. The record count is
// (u16)a0[0x10] * (u16)a0[0x12]; func_08B35840(0x10, count) gives the block stored at a0 + 8 (null answers 0), and
// func_08B6461C((u16)a0[0x10] * 8) the entry block stored at a0 + 0xC (also null answers 0) whose (u16)a0[0x10]
// 8-byte entries are zeroed. func_08A4F8B4(a0, a1, a2) must answer non-null (else 0), and then for each row of the
// global table at 0x08CDF570 (0x08CDF56C rows of {u16 from, u16 to, u16 pad}) every index in [from, to) goes through
// func_08A4F784(a0, built, index) — `to` is re-read from the row on every round. func_08A4F76C(a0, built) ends it and the
// answer is 1.
void func_08A4FA18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first_argument = ctx.gpr[5];
    const u32 second_argument = ctx.gpr[6];
    Saved saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = self;
    ctx.gpr[19] = first_argument;
    ctx.gpr[17] = second_argument;
    const u32 count = static_cast<u32>(rt.memory().load16(self + 0x10u)) * rt.memory().load16(self + 0x12u);
    const u32 block = call_value(rt, ctx, 0x08B35840u, {0x10u, count});
    put(rt, self + 8u, block);
    if (block == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 entries = rt.memory().load16(self + 0x10u);
    const u32 table = call_value(rt, ctx, 0x08B6461Cu, {entries * 8u});
    if (entries != 0u) {
        u32 at = table;
        for (u32 index = 0u; index < entries; ++index) {
            for (u32 byte = 0u; byte < 4u; ++byte) rt.memory().store8(at + byte, 0u);
            rt.memory().store16(at + 4u, 0u);
            rt.memory().store16(at + 6u, 0u);
            at += 8u;
        }
    }
    put(rt, self + 0xCu, table);
    if (table == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 built = call_value(rt, ctx, 0x08A4F8B4u, {self, first_argument, second_argument});
    if (built == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (static_cast<s32>(word(rt, 0x08CDF56Cu)) > 0) {
        for (u32 row = 0u;; ++row) {
            const u32 row_at = 0x08CDF56Cu + 4u + 6u * row;
            u32 from = rt.memory().load16(row_at);
            const u32 to = rt.memory().load16(row_at + 2u);
            if (static_cast<s32>(from) < static_cast<s32>(to)) {
                for (;;) {
                    call_value(rt, ctx, 0x08A4F784u, {self, built, from});
                    const u32 limit = rt.memory().load16(row_at + 2u);
                    from += 1u;
                    if (!(static_cast<s32>(from) < static_cast<s32>(limit))) break;
                }
            }
            if (!(static_cast<s32>(row + 1u) < static_cast<s32>(word(rt, 0x08CDF56Cu)))) break;
        }
    }
    call_value(rt, ctx, 0x08A4F76Cu, {self, built});
    ctx.gpr[2] = 1u;
}

// 0x08AC50DC (a0 = object): the object's frame loop. While the byte at +0x25B3 is set and the byte at +0x25B1 clear it
// keeps running the pass at +0x30: func_08B34888(stream), the amount (+0x25C0[0x10] + +0x25C0[8] * +0x25C0[0x14]) and the
// count +0x25C0[0xC], func_08B34B24(stream); a zero count jumps to the tail. Otherwise the product of the four floats at
// +0x2598, +0x259C, +0x25A0 and +0x25A4 goes to func_08AF0A74(+0x25E0) as $f12 and $f13, func_08AF0B78(+0x25E0, amount,
// 0x1000, 1) runs, then func_08B34888, func_08B24A60(+0x25C0, 1), and when +0x25DC is set it is cleared and
// func_08B34C1C(stream) runs. func_08B34B24(stream), func_08AC4B78(object, $f12 = 0.023200...) and
// func_08AC508C(object, 0x400) end the pass, and the loop repeats while +0x25B3 is set and +0x25B1 clear. A cleared
// +0x25B3 leaves the function; otherwise func_08B2EFDC(-1, -1) runs first. v0 is 0.
void func_08AC50DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx, {16u, 17u, 18u, 19u, 20u});
    GuestFrame frame(ctx, 0x20u);
    const u32 stream = self + 0x30u;
    const u32 list = self + 0x25C0u;
    ctx.gpr[19] = stream;
    ctx.gpr[18] = list;
    ctx.gpr[17] = self;
    const auto pass = [&]() -> bool {
        call_value(rt, ctx, 0x08B34888u, {stream});
        const u32 base = word(rt, list + 0x10u);
        const u32 factor = word(rt, list + 8u);
        const u32 other = word(rt, list + 0x14u);
        const u32 count = word(rt, list + 0xCu);
        const u32 amount = base + factor * other;
        call_value(rt, ctx, 0x08B34B24u, {stream});
        if (count == 0u) return false;
        float value = psp_mul(float_at(rt, self + 0x2598u), float_at(rt, self + 0x259Cu));
        value = psp_mul(value, float_at(rt, self + 0x25A0u));
        value = psp_mul(value, float_at(rt, self + 0x25A4u));
        call_full(rt, ctx, 0x08AF0A74u, {word(rt, self + 0x25E0u)}, {value, value});
        call_value(rt, ctx, 0x08AF0B78u, {word(rt, self + 0x25E0u), amount, 0x1000u, 1u});
        call_value(rt, ctx, 0x08B34888u, {stream});
        call_value(rt, ctx, 0x08B24A60u, {list, 1u});
        if (rt.memory().load8(self + 0x25DCu) != 0u) {
            rt.memory().store8(self + 0x25DCu, 0u);
            call_value(rt, ctx, 0x08B34C1Cu, {stream});
        }
        call_value(rt, ctx, 0x08B34B24u, {stream});
        call_full(rt, ctx, 0x08AC4B78u, {self}, {std::bit_cast<float>(0x3CBE37C6u)});
        call_value(rt, ctx, 0x08AC508Cu, {self, 0x400u});
        return true;
    };
    for (;;) {
        if (rt.memory().load8(self + 0x25B3u) == 0u) break;
        if (rt.memory().load8(self + 0x25B1u) == 0u) {
            for (;;) {
                if (!pass()) break;
                if (rt.memory().load8(self + 0x25B3u) == 0u) break;
                if (rt.memory().load8(self + 0x25B1u) != 0u) break;
            }
        }
        if (rt.memory().load8(self + 0x25B3u) == 0u) break;
        call_value(rt, ctx, 0x08B2EFDCu, {0xFFFFFFFFu, 0xFFFFFFFFu});
    }
    ctx.gpr[2] = 0u;
}

} // namespace
} // namespace new_batch_main_196
} // namespace gt::game

#define MAIN196(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_196::name, gt::ReturnKind::kind)
MAIN196(0x08942548u, func_08942548, Word);
MAIN196(0x08965ED4u, func_08965ED4, Word);
MAIN196(0x089B439Cu, func_089B439C, Word);
MAIN196(0x08B82420u, func_08B82420, Word);
MAIN196(0x08AEF714u, func_08AEF714, Word);
MAIN196(0x0894419Cu, func_0894419C, Word);
MAIN196(0x08A4FA18u, func_08A4FA18, Word);
MAIN196(0x08AC50DCu, func_08AC50DC, Word);

// ---- from main-session batch 197 (draft 3630eb6494b9e60c6d57b3a793e50534d30593b2) ----
// Main-session batch 197: structured C from tools.disasm, one function at a time.
//
// Each function is written by hand from its own instructions: named locals, guest memory through the hot helpers, G[n] only at a
// call boundary. Callee-saved registers are never written (the original restores them before it returns or tail-calls), so there is
// no SavedRegs. The delay slot of a jal/jalr runs BEFORE the call, a bnel/beql/bgtzl/blezl delay slot only when the branch is taken,
// the delay slot of a plain branch on both paths. v0 (and v1/f0 where a caller could read them) is left as the original leaves it on
// every path. Frames reproduce the original's `addiu $sp, $sp, -N`.
//
// Skipped on purpose (unsure / not comparable): 0x08B754C4, 0x08B75710, 0x08B75B04 build a frame and then return through
// `addu $sp, $sp, $v1` with a restored $ra from the frame (setjmp/longjmp-shaped: the saved return address is reloaded from the
// frame), and the synthetic harness cannot compare those.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_197 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call keeping v0 / v1 / f0 as the callee left them (a callee that writes no v0 hands the caller's own v0 on).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments).v0;
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// The string-block release the SDK's string class uses (0x08B60804 / 0x08B8542C): a block whose header is the static 0x08D74FD0 stays.
void release_block(Runtime &rt, AllegrexContext &ctx, u32 block, u32 flag) {
    const u32 header = block - 0xCu;
    if (header == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {block - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {header, flag});
}

// --- 0x08B51BF8: the next path segment of a character lexer ---------------------------------------------------------------
// The lexer at this + 0xC20 (0x08B51EE8 advances it: text at +0x18, length at +0x1C, position at +0x20, current character at +8,
// a has-character flag at +0xC, an ASCII flag at +0x10). 0x08B51F38 answers the current character entry (lexer + 4), or 0 at the end.
// The 0x800-byte buffer at this + 0x20 accumulates the characters of one segment; this + 0x18 holds its kind and this + 0x1C its
// length. A '"' toggles the quoted state and is copied like any other character; a '>' outside quotes ends the segment, and when the
// character copied before it was a '/' the segment is closed by a zero byte and the kind becomes 3.
void func_08B51BF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 buffer = self + 0x20u;
    call(rt, ctx, 0x08B36018u, {buffer, 0u, 0x800u});
    const u32 entry = word(rt, self + 0xC20u);
    call(rt, ctx, 0x08B79A88u, {buffer, entry + 8u, word(rt, entry + 0xCu)});
    u32 length = word(rt, entry + 0xCu);
    put(rt, self + 0x1Cu, 0u);
    const s32 first_character = rt.memory().load8(entry + 0x10u) != 0u ? signed_byte(rt, entry + 8u) : 0;
    put(rt, self + 0x18u, first_character == 0x2F ? 2u : 1u);
    u32 quoted = 0u;
    u32 previous = 0u;
    u32 previous_start = 0u;
    for (;;) {
        const u32 current = call(rt, ctx, 0x08B51F38u, {word(rt, self + 0xC20u)});
        if (current == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const s32 character = rt.memory().load8(current + 0xCu) != 0u ? signed_byte(rt, current + 4u) : 0;
        if (character == 0x22) quoted ^= 1u;
        if (character == 0x3E && quoted == 0u) {
            if (previous != 0x2F) break;
            rt.memory().store8(buffer + previous_start, 0u);
            length -= 1u;
            put(rt, self + 0x18u, 3u);
            break;
        }
        previous = static_cast<u32>(character);
        previous_start = length;
        const u32 lexer = word(rt, self + 0xC20u);
        call(rt, ctx, 0x08B79A88u, {buffer + length, lexer + 8u, word(rt, lexer + 0xCu)});
        length += word(rt, word(rt, self + 0xC20u) + 0xCu);
    }
    put(rt, self + 0x1Cu, length);
    ctx.gpr[2] = word(rt, self + 0x18u);
}

// --- 0x08B8B630: std::__push_heap over 24-byte elements -------------------------------------------------------------------
// Element = {two words at +0 and +4, a vector of u16 at +8 (begin / end at +8 / +0xC, compared by 0x08B8A72C)}. a0 = base, a1 = hole
// index, a2 = top index, a3 = the value; the parent index is (hole - 1) / 2 (signed), and 0x08B8AD88 assigns the vector member. The
// loop moves the parent down while parent < value and stops at the top index, then puts the value into the hole. That last assignment
// is the original's `j 0x08B8AD88` (the epilogue sits in its delay slot), so the frame is closed and the function tail-calls it; only
// a value that already lies in the hole's slot skips the assignment and answers the value's first word.

// --- 0x08BF9C34: merge of two ranges of 8-byte elements, backwards ---------------------------------------------------------
// a0 / a1 = first range [begin, end), a2 / a3 = second range, t0 = one past the destination; v0 = the new start. An element is two
// words; its key is the signed word at +0x134 of the object its first word points at. Equal keys compare the elements' second words
// (signed) and the larger one goes first, so the merge runs from the highest key down.
namespace {
void copy_elements_backward(Runtime &rt, u32 end, u32 destination, s32 count) {
    for (s32 i = 0; i < count; ++i) {
        end -= 8u;
        const u32 first = word(rt, end);
        const u32 second = word(rt, end + 4u);
        destination -= 8u;
        put(rt, destination, first);
        put(rt, destination + 4u, second);
    }
}
} // namespace

// --- 0x08916AC0: the pad status bits of an object written into a flag block -----------------------------------------------
// a0 = this, a1 = object, a2 = a byte. The virtual at +0x5C of the object's vtable answers a status word: bit 0x400 disables and so
// does a zero byte. The virtual at +0x50 answers a flag word whose bits 1, 4, 8, 0x10, 0x20 are written as the five values 2, 7, 3,
// 0xA, 0xB of the block at this + 4 (0x0890B030), then 0x0890AEAC gets (enabled, status & 1) and 0x0890B014's answer for index 2 goes
// to the byte at this + 0x15.
void func_08916AC0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 entered = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    const u32 state = self + 4u;
    call(rt, ctx, 0x0890AEC4u, {state});
    const u32 status = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x5Cu), {object});
    const u32 low_bit = status & 1u;
    const bool enabled = (status & 0x400u) == 0u && entered != 0u;
    const u32 flags = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x50u), {object});
    call(rt, ctx, 0x0890B030u, {state, 0u, (flags >> 1) & 1u});
    call(rt, ctx, 0x0890B030u, {state, 4u, enabled ? 1u : 0u});
    const auto write_flag = [&](u32 index, u32 mask) {
        call(rt, ctx, 0x0890B030u, {state, index, (enabled && (flags & mask) != 0u) ? 1u : 0u});
    };
    write_flag(2u, 1u);
    write_flag(7u, 4u);
    write_flag(3u, 8u);
    write_flag(0xAu, 0x10u);
    write_flag(0xBu, 0x20u);
    call(rt, ctx, 0x0890AEACu, {state, enabled ? 1u : 0u, low_bit});
    rt.memory().store8(self + 0x15u, static_cast<std::uint8_t>(call(rt, ctx, 0x0890B014u, {state, 2u})));
}

// --- 0x08B40514: reload a 0x230-byte parameter block into this ------------------------------------------------------------
// a0 = this, a1 = the block. The virtual at +0x9C of *this gates the whole thing; then, when the byte at block + 4 is set, the
// virtual at +0x54 of the object at this + 0xB0 is asked for (0x4E9B, 0, 1) and a negative answer aborts. The 0x348 bytes at
// this + 0x990 are cleared, the words at block + 5 (0x20 bytes) go to this + 0x998 and the 0x200 bytes at block + 0x25 to
// this + 0x9B8; the halfword at block + 0x22C becomes this + 0xBCE and 1 this + 0xBD0. Then this + 0x12C is locked, 0x08B402E4
// runs on (this, this + 0xBB8, -1, 1, 0), unlocked again, and the bytes at this + 0xB6, +0xB7, +0xBA become 1 with this + 0x93C = 0.
// --- 0x0884F138: copy a 0x80-byte message of a named object ---------------------------------------------------------------
// a0 = the destination string, a1 = passed on to 0x0884D660 (the string constructor's source), a2 = a signed count, a3 = a pointer to
// the object pointer. The string looked up in 0x0884D660 is released right after 0x0884D3D8 takes the message object out of it.
// With a positive count the object's virtual +0xC is asked into this frame's +0x10 and 0x08B7C830 copies 0x80 bytes from the buffer
// 0x08896FB0 answers to it; otherwise 0x08B85EB8 / 0x08A6D2D4 rebuild the destination. Each temporary block is released the SDK way.

// --- 0x088F024C: whether a table row's flag applies to a (row, column) key -------------------------------------------------
// a0 = this, a1 = a pointer to a two-word key. Every word at this + 4 .. +0x1C must be set, the byte at +0x24 must be set and the
// word at +0xE8 must not be zero; then the two checks 0x088D9CCC and 0x088DAA84 must answer 0 and non-zero. The row word of the key
// indexes the table object at this + 0xCC (row count at +4), the second word of the key is a column; a valid column answers 1 and a
// row whose byte in the array at this + 0xD4 is zero answers the array address. 0x088F12C0 then answers how many 8-byte entries at
// the table base + row * 84 + 4 to hand to 0x088EFD54.
void func_088F024C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 handle = word(rt, self + 4u);
    if (handle == 0u) return;
    const u32 fields = self + 4u;
    const auto absent = [&](u32 field) { ctx.gpr[2] = field; };
    if (word(rt, fields + 4u) == 0u) { absent(0u); return; }
    if (word(rt, fields + 8u) == 0u) { absent(0u); return; }
    if (word(rt, fields + 0xCu) == 0u) { absent(0u); return; }
    if (word(rt, fields + 0x10u) == 0u) { absent(0u); return; }
    if (word(rt, fields + 0x14u) == 0u) { absent(0u); return; }
    if (word(rt, fields + 0x18u) == 0u) { absent(0u); return; }
    if (rt.memory().load8(self + 0x24u) == 0u) { absent(0u); return; }
    if (word(rt, self + 0xE8u) == 0u) { absent(0u); return; }
    if (call(rt, ctx, 0x088D9CCCu, {handle}) != 0u) return;
    if (call(rt, ctx, 0x088DAA84u, {handle}) == 0u) return;
    const u32 table = word(rt, self + 0xCCu);
    const s32 row = static_cast<s32>(word(rt, key));
    const s32 column = static_cast<s32>(word(rt, key + 4u));
    const s32 rows = static_cast<s32>(word(rt, table + 4u));
    if (row < 0) return;
    if (row >= rows) { ctx.gpr[2] = 0u; return; }
    if (column >= 0 && column < rows) { ctx.gpr[2] = 1u; return; }
    const u32 flag_address = word(rt, self + 0xD4u) + static_cast<u32>(row);
    const s32 flag = signed_byte(rt, flag_address);
    if (flag == 0) { ctx.gpr[2] = flag_address; return; }
    const u32 row_bytes = static_cast<u32>(row) * 12u;
    const u32 row_offset = (row_bytes << 3) - row_bytes;
    const u32 table_base = word(rt, table);
    const u32 count = call(rt, ctx, 0x088F12C0u,
                           {table_base + row_offset, static_cast<u32>(row), static_cast<u32>(flag), self + 4u, key});
    if (count == 0u) return;
    for (u32 index = 0u; index < count; ++index) {
        const u32 base = word(rt, word(rt, self + 0xCCu));
        const u32 element = base + row_offset + 4u + index * 8u;
        call(rt, ctx, 0x088EFD54u, {self, word(rt, element + 4u), word(rt, element), static_cast<u32>(row)});
    }
}

// --- 0x08AAC96C: compare a named string with the one held by an object, then its table row ---------------------------------
// a0 = this, a1 = a pointer to the object pointer. The object's size word sits at -0xC of it. 0x08B85EB8 builds the temporary string
// for the name 0x08CB3374; 0x08B36170 compares both with the shorter of the two sizes and the first difference (or the size gap)
// decides. A zero result answers 1 without touching the this-side tables; otherwise this + 0x84 is locked, 0x08B88F44 copies the
// object into the frame, 0x08B29E88 builds the row handle, 0x08C34FE8 looks it up among the entries at this + 0x68 and the answer is
// whether it differs from the word at this + 0x6C. v0 = that answer (1 when the strings are equal).
void func_08AAC96C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    if (word(rt, word(rt, holder) - 0xCu) == 0u) { ctx.gpr[2] = 0u; return; }
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CB3374u, frame.at(2u)});
    const u32 temporary = word(rt, frame.at(4u));
    const u32 wanted = word(rt, word(rt, holder) - 0xCu);
    const u32 available = word(rt, temporary - 0xCu);
    put(rt, frame.at(8u), wanted);
    put(rt, frame.at(0xCu), available);
    const u32 length = available < wanted ? available : wanted;
    const u32 compared = call(rt, ctx, 0x08B36170u, {word(rt, holder), temporary, length, wanted});
    u32 result = wanted - available;
    if (compared != 0u) result = compared;
    release_block(rt, ctx, temporary, frame.at(1u));
    if (result == 0u) { ctx.gpr[2] = 1u; return; }
    call(rt, ctx, 0x08B344E0u, {self + 0x84u});
    call(rt, ctx, 0x08B88F44u, {frame.at(0x10u), holder});
    put(rt, frame.at(0x14u), call(rt, ctx, 0x08B29E88u, {0u, word(rt, holder), word(rt, word(rt, holder) - 0xCu)}));
    const u32 found = call(rt, ctx, 0x08C34FE8u, {self + 0x68u, frame.at(0x10u)});
    release_block(rt, ctx, word(rt, frame.at(0x10u)), frame.at(0u));
    const bool differs = (self + 0x6Cu) != found;
    call(rt, ctx, 0x08B3457Cu, {self + 0x84u});
    ctx.gpr[2] = differs ? 1u : 0u;
}

// --- 0x089372C8: the largest low word of nine table entries read as a 64-bit value -----------------------------------------
// a0 = unused here, a1 = a name, a2 = a second key. Each of the nine words from 0x08CDDE0C to 0x08CDDE30 builds a temporary: a
// string from (a1, table word, a2), 0x08B0CE58 on this frame's +0x40, then 0x08B0EC60 into +0x180 whose block (0x30 bytes in
// 0x10-byte steps plus three words, as the original copies it) is moved to the frame and back onto +0x40. The 64-bit value at +0x50
// then wins when its high word is positive, or when it is zero and the low word is larger than the best so far. v0 = the best low
// word.
namespace {
void copy_block(Runtime &rt, u32 source, u32 destination) {
    for (u32 offset = 0u; offset != 0x30u; offset += 0x10u) {
        for (u32 inner = 0u; inner != 0x10u; inner += 4u)
            put(rt, destination + offset + inner, word(rt, source + offset + inner));
    }
    for (u32 offset = 0u; offset != 0xCu; offset += 4u)
        put(rt, destination + 0x30u + offset, word(rt, source + 0x30u + offset));
}
} // namespace

// --- 0x08B67C70: a converted float through the state at 0x08C96698 ---------------------------------------------------------
// f12 = the value. 0x08B6BE8C converts it and that is the answer when the state is -1; otherwise the state must pass 0x08B6EC94 for
// the converted value and for the incoming one, and the buffer 0x08B70F28 answers goes twice into the frame. The word pair at
// 0x08C96690 / 0x08C96694 (the low word's sign flipped when the incoming value is not positive) or, for state 0, 0x08CB6630 /
// 0x08CB6634 and 0x08CB6638 / 0x08CB663C go to the frame's +0x18. For state 2 the error 0x22 is stored through 0x08B77D40 first.
// 0x08B6E524 either accepts the pair or rejects it (then the frame's word +0x20 is stored through 0x08B77D40, if set) and
// 0x08B71EC8 answers the pair as a float; f0 = that answer (or the converted value on the early returns).
void func_08B67C70(Runtime &rt, AllegrexContext &ctx) {
    const float incoming = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    const float converted = call_keep(rt, ctx, 0x08B6BE8Cu, {}, {incoming}).f0;
    const u32 state = word(rt, 0x08C96698u);
    if (state != 0xFFFFFFFFu) {
        if (call_keep(rt, ctx, 0x08B6EC94u, {}, {converted}).v0 != 0u) {
            ctx.fpr[0] = converted;
            return;
        }
        if (call_keep(rt, ctx, 0x08B6EC94u, {}, {incoming}).v0 == 0u) {
            ctx.fpr[0] = converted;
            return;
        }
        put(rt, frame.at(0u), 3u);
        put(rt, frame.at(4u), 0x08CB5E7Cu);
        put(rt, frame.at(0x20u), 0u);
        const GuestResult pair = call_keep(rt, ctx, 0x08B70F28u, {}, {incoming});
        put(rt, frame.at(8u), pair.v0);
        put(rt, frame.at(0xCu), pair.v1);
        put(rt, frame.at(0x10u), pair.v0);
        put(rt, frame.at(0x14u), pair.v1);
    }
    const bool positive = incoming > 0.0f;
    const auto answer = [&]() {
        return call_keep(rt, ctx, 0x08B71EC8u, {word(rt, frame.at(0x18u)), word(rt, frame.at(0x1Cu))}).f0;
    };
    const auto store_error = [&](u32 value) { put(rt, call(rt, ctx, 0x08B77D40u), value); };
    float result;
    if (state == 0u) {
        if (positive) {
            put(rt, frame.at(0x18u), word(rt, 0x08CB6630u));
            put(rt, frame.at(0x1Cu), word(rt, 0x08CB6634u));
        } else {
            put(rt, frame.at(0x18u), word(rt, 0x08CB6638u));
            put(rt, frame.at(0x1Cu), word(rt, 0x08CB663Cu));
        }
        if (call(rt, ctx, 0x08B6E524u, {frame.at(0u)}) == 0u) {
            store_error(0x22u);
            if (word(rt, frame.at(0x20u)) != 0u) store_error(word(rt, frame.at(0x20u)));
            result = answer();
        } else if (word(rt, frame.at(0x20u)) == 0u) {
            result = answer();
        } else {
            store_error(word(rt, frame.at(0x20u)));
            result = answer();
        }
    } else {
        put(rt, frame.at(0x18u), word(rt, 0x08C96690u));
        put(rt, frame.at(0x1Cu), positive ? word(rt, 0x08C96694u) : (word(rt, 0x08C96694u) ^ 0x80000000u));
        if (state != 2u) {
            if (call(rt, ctx, 0x08B6E524u, {frame.at(0u)}) == 0u) {
                store_error(0x22u);
                if (word(rt, frame.at(0x20u)) != 0u) store_error(word(rt, frame.at(0x20u)));
                result = answer();
            } else if (word(rt, frame.at(0x20u)) == 0u) {
                result = answer();
            } else {
                store_error(word(rt, frame.at(0x20u)));
                result = answer();
            }
        } else {
            store_error(0x22u);
            if (word(rt, frame.at(0x20u)) != 0u) store_error(word(rt, frame.at(0x20u)));
            result = answer();
        }
    }
    ctx.fpr[0] = result;
}
// --- 0x08C77F9C: read up to a count of bytes through a stream object ------------------------------------------------------
// a0 = the stream, a1 = the destination, a2 = the count. Without the stream object at +0x90, or when its virtual +0x18 answers zero,
// or without the flag 0x10 at +0x2C, or when the byte at +0x81 is set, the plain reader 0x08C73160 does the work. Otherwise the
// window at +0x10..+0x18 is examined: its limit is the smaller of the remaining window (the byte at +0x82 and the block size at
// +0x7C adjust it) and 0x400, and a count below that falls back to the plain reader too; 0x08B617E4 reads into the window and a
// short read answers the bytes read beyond the window. A full read moves the window (the position at +0x78, and with flag 0x10
// either a span from +0x7C or zeros) and answers the count.
// --- 0x08832E2C: build a 0x14C-byte string block from a key and a converted value -----------------------------------------
// a0 = the destination string, a1 = the key string, a2 = a signed count, a3 = a pointer to the object pointer. A positive count has
// the object's virtual +0x3C converted first. The temporary built by 0x088297A8 (from a1) is released through 0x08A5B9EC after
// 0x088294D0 / 0x0885EE58 turn it into a source block; 0x08B62344 allocates 0x14C bytes (cleared by 0x08B36018) and the 0x14C bytes
// 0x088936F8 answers are copied into it. 0x088221F4 builds a second temporary key whose block 0x08A52AB8 formats with 0x08829350 into
// this frame's +0; the destination takes it when it is not already that frame slot. Both temporaries are released last.
} // namespace
} // namespace new_batch_main_197
} // namespace gt::game

#define NEW197(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_197::name, gt::ReturnKind::kind)
NEW197(0x08B51BF8u, func_08B51BF8, Word);
NEW197(0x08916AC0u, func_08916AC0, Word);
NEW197(0x088F024Cu, func_088F024C, Word);
NEW197(0x08AAC96Cu, func_08AAC96C, Word);
NEW197(0x08B67C70u, func_08B67C70, Float);

// ---- from main-session batch 198 (draft cdc8aa79eae75c24489b3940d387377e98553bca) ----
// New batch main 198: ten functions the gate could never compare because the synthetic inputs never reached their bodies
// (each had an unregistered draft in an earlier batch with a comment saying so: func_089ABDCC / func_08AFF62C in batch 112,
// func_088FEB9C in batch 92, func_08C07E24 / func_08C070FC in batches 90 and 100, func_08B14A18 in batch 121). They are
// written here from `tools.disasm` as structured C, with the synthetic input shapes the batch proposes (one line per
// address with the asm evidence, so the generator builds the records these functions walk).
//
// Skipped, unsure of a faithful structured shape (drafts left to a later batch): func_08B42E74 (a variadic formatter: it
// lowers $sp by a runtime-sized amount with `subu $sp,$sp,v0`, which GuestFrame cannot express), func_08C34ABC (the red-black
// tree insert with rotations inline), func_08C76B7C (a token reader whose loop enters from five places), func_08AD45F4 (VFPU:
// it builds two matrices and leaves C000/C010/C400 for the next call).
//
// The delay slot of a `jal` / `jalr` runs BEFORE the call and a `beql` / `bnel` delay slot only when the branch is taken.
// v0 is set as the original leaves it on every path (the harness compares it), v1 / f0 where the next code reads them.
// The callee-saved registers are never written (the original restores them), so there is no SavedRegs.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_198 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next
// instruction sees, and a callee that sets no v0 hands the previous one on to the code after it.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments).v0;
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (one that never writes v0 returns it).
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_keep(rt, ctx, target, arguments);
}
s32 signed_of(u32 value) { return static_cast<s32>(value); }
// The 32-bit wrap-around of the original's arithmetic (a signed overflow is UB in C++, not on the PSP).
u32 wrap_add(s32 a, s32 b) { return static_cast<u32>(a) + static_cast<u32>(b); }
s32 wrap_sub(s32 a, s32 b) { return signed_of(static_cast<u32>(a) - static_cast<u32>(b)); }
// The low word of a signed 32-bit product, as `mult` / `mflo` leave it.
s32 mult_lo(s32 a, s32 b) {
    return signed_of(static_cast<u32>(static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b)));
}
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 slot_of(Runtime &rt, u32 object, u32 offset) { return word(rt, word(rt, object) + offset); }

// 0x08AA72CC(kind = a0, holder = a1, destination = a2): writes one value of the type `kind` into `destination` through
// 0x08BEED1C(destination, bytes, size). The value comes from the object at *holder: its virtual slot 0x50 gives the pair
// (low = v0, high = v1); 'i' / 'l' ('s' is not in the table) reuse it, 'B' / 'b' write its low word as one byte,
// 'F' / 'f' take virtual slot 0x54 (the float whose bits go out), 'D' / 'd' take slot 0x58 (a 64-bit pair). The bytes are
// written from the top when the type is lower case ('f', 'l', 'd'; 'B' / 'b' both) and from the bottom otherwise; an
// unknown type writes nothing (size 0) and the object's pair goes nowhere.
void func_08AA72CC(Runtime &rt, AllegrexContext &ctx) {
    const u32 kind = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 destination = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    u32 low = 0u;   // $t2
    u32 high = 0u;  // $t3
    const u32 object = word(rt, holder);
    if (object != 0u) {
        low = call_virtual(rt, ctx, slot_of(rt, object, 0x50u), {object}).v0;
        high = ctx.gpr[3];
    }
    u32 size = 0u;        // $t4
    bool from_top = false;  // $s0
    if (kind - 'B' < 0x32u) {  // 'B' .. 's' through the table at 0x08C8FDC4
        switch (kind) {
        case 'B':
        case 'b':
            from_top = true;
            size = 1u;
            break;
        case 'i':
            from_top = true;
            size = 4u;
            break;
        case 'l':
            from_top = true;
            size = 8u;
            break;
        case 'F':
        case 'f': {  // the float at virtual slot 0x54
            from_top = kind == 'f';
            size = 4u;
            u32 bits = 0u;
            const u32 floating = word(rt, holder);
            if (floating != 0u) {
                const GuestResult result = call_virtual(rt, ctx, slot_of(rt, floating, 0x54u), {floating});
                bits = std::bit_cast<u32>(result.f0);
            }
            low = bits;
            high = 0u;
            break;
        }
        case 'D':
        case 'd': {  // the 64-bit pair at virtual slot 0x58 (the target sits in a1 at the jalr)
            from_top = kind == 'd';
            size = 8u;
            u32 value_low = 0u;
            u32 value_high = 0u;
            const u32 wide = word(rt, holder);
            if (wide != 0u) {
                const u32 target = slot_of(rt, wide, 0x58u);
                value_low = call_virtual(rt, ctx, target, {wide, target}).v0;
                value_high = ctx.gpr[3];
            }
            low = value_low;
            high = value_high;
            break;
        }
        default:
            break;  // a size of 0: nothing is written and the pair from slot 0x50 goes nowhere
        }
    }
    // The 64-bit shift of the pair: (low >> shift) | (high << (32 - shift)), with the high word alone from bit 32 on.
    for (u32 index = 0u; index != size; ++index) {
        const u32 shift = (from_top ? size - 1u - index : index) * 8u;
        const u32 byte = shift >= 32u ? high >> (shift - 32u)
                                      : (low >> shift) | (shift == 0u ? 0u : high << (32u - shift));
        put8(rt, frame.at(index), byte);
    }
    if (size != 0u) ctx.gpr[7] = frame.at(size - 1u);  // the loop's last `a3 = t1 + sp`
    ctx.gpr[2] = call(rt, ctx, 0x08BEED1Cu, {destination, frame.at(0u), size});
}

// 0x08ADC348(self = a0, selector = a1, second = a2, key = a3): an attribute of the record 0x08ADB278(self, 0x08C90EC4),
// whose offset 0x14 is a table base. Selector 0: 0x08ADBE98(self, second, 0) names a record whose word +4 is an offset into
// base (the table 0x08C90E14 with no offset); selector 1: 0x08ADBF4C(self, second, sp) gives a count in sp and a run of
// records {signed byte low +0x18, signed byte high +0x19, word +4 offset, word +8 stride}, the first record whose
// [low, high] holds `key` and whose +4 is non-zero gives its offset (none gives 0); selector 2: 0x08ADBFE0(self, second,
// key) gives a record whose +4 is the offset. Any other selector returns 0, and with bit 0 of 0x08CDFB3C set it reports the
// bad selector through 0x08AD83B8 first (the string 0x08C90D44, line 0x446).
void func_08ADC348(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 selector = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 key = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 record = call(rt, ctx, 0x08ADB278u, {self, 0x08C90EC4u});
    if (record == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 base = word(rt, record + 0x14u);
    if (base == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    u32 result = 0u;
    if (selector == 0u) {
        const u32 named = call(rt, ctx, 0x08ADBE98u, {self, second, 0u});
        if (named != 0u) {
            const u32 offset = word(rt, named + 4u);
            result = offset != 0u ? word(rt, record + 0x14u) + offset : 0x08C90E14u;  // the tail re-reads the base
        }
    } else if (selector == 1u) {
        const u32 table = call(rt, ctx, 0x08ADBF4Cu, {self, second, frame.at(0u)});
        if (table != 0u) {
            const s32 count = signed_of(word(rt, frame.at(0u)));
            u32 cursor = table;
            for (s32 index = 0; index < count; ++index) {
                const s32 low = signed_byte(rt, cursor + 0x18u);
                const s32 high = signed_byte(rt, cursor + 0x19u);
                if (!(signed_of(key) < low) && !(high < signed_of(key)) && word(rt, cursor + 4u) != 0u) {
                    result = word(rt, record + 0x14u) + word(rt, cursor + 4u);
                    break;
                }
                cursor += word(rt, cursor + 8u);
            }
        }
    } else if (selector == 2u) {
        const u32 named = call(rt, ctx, 0x08ADBFE0u, {self, second, key});
        if (named != 0u) {
            const u32 offset = word(rt, named + 4u);
            result = offset != 0u ? word(rt, record + 0x14u) + offset : 0x08C90E14u;
        }
    } else if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) {
        (void)call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C90D44u, 0x446u, 0x08C90ED0u, selector});
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// 0x088E60B4(self): a frame value of the 0x08CExxxx value library. It locks the two holders self[0] and self[4] (their
// +0xC4), asks 0x088ECA80(self[0]) for the current object and, when that object's byte +8 is not zero, 0x088ECA38(self[0])
// picks the path: without it the object's virtual slot 0x54(self[0], object, sp, sp + 4) makes the second value, else the
// update is taken from self's own word 0 (the frame cursor) and the virtual slot 0xB4 (or 0x088EDD5C when both values
// exist). The record at sp + 8 {+0 word 0, +4 self[0]->+0xC0, +8 the object's +0, +0xC its +4, +0x10 -1, +0x14 the second
// value or 0, +0x18 byte 0} is what those take. 0x088ECAA4(self[0]) closes the update, then both holders unlock. A zero
// object only runs 0x088ECA54(self[0]) and the close.

// 0x089ABDCC(self, args): a script object's update. A frame value object (0x089A6A7C at sp + 4) copies self[0x68] under
// the lock 0x08CF4ACC (0x08B344E0 / 0x08B3457C); when the copy's word 0 is set, self[0x6C] (byte) is 0x089A7120(that word);
// an empty object (0x089A7644 at sp) replaces self[0x68] and is destroyed. After 0x08B1C47C, the copy's object (when set)
// gets 0x089A81C4(object, self), then 0x089A6C20(object, args[0] ? 0x089A70D4(args[0]) : 0) (a null object is reported
// through 0x08A5BB7C(0x08CA50AC)) and 0x089A71C8(object, self); under the lock it goes back to self[0x68] and the copy is
// destroyed. (tools.sp_loads lists the two `lw $s1, 4($sp)`: that word is the frame object's own first word, written by
// 0x089A6A7C and read back as word(frame.at(4)) here — not a spilled s1.)

// The counted-pointer pieces of 0x088FEB9C (the intrusive pair strong count +4 / weak count +8): the counters are ll / sc
// loops in the original (one thread here, so a load and a store), and a release runs the object's virtual slots 8 and, for
// the last weak count, 0xC — both with the incoming pointer in a1, as the original leaves it there.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object, u32 incoming) {
    (void)call_virtual(rt, ctx, slot_of(rt, object, 8u), {object, incoming});
    const u32 weak = word(rt, object + 8u);
    put(rt, object + 8u, weak - 1u);
    if (weak == 1u) (void)call_virtual(rt, ctx, slot_of(rt, object, 0xCu), {object, incoming});
}
void assign_counted(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 incoming) {
    if (incoming != 0u) put(rt, incoming + 4u, word(rt, incoming + 4u) + 1u);
    const u32 old = word(rt, slot);
    if (old != 0u) {
        const u32 strong = word(rt, old + 4u);
        put(rt, old + 4u, strong - 1u);
        if (strong == 1u) release_counted(rt, ctx, old, incoming);
    }
    put(rt, slot, incoming);
}

// 0x088FEB9C(self, source): w = self[0]; w[0x10] = source[0]; when it differs from w[0x14], that member becomes
// source[4] through the counted assignment; w[0x18] = source[8]; when it differs from w[0x1C], that member becomes
// source[0xC] the same way; self[0xB] (byte) = 1, v0 = 1.
void func_088FEB9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 w = word(rt, self);
    const u32 members = w + 0x10u;
    const u32 old_first = word(rt, members + 4u);
    put(rt, members, word(rt, source));
    const u32 first = word(rt, source + 4u);
    if (first != old_first) assign_counted(rt, ctx, members + 4u, first);
    const u32 old_second = word(rt, members + 0xCu);
    put(rt, members + 8u, word(rt, source + 8u));
    const u32 second = word(rt, source + 0xCu);
    if (second != old_second) assign_counted(rt, ctx, members + 0xCu, second);
    put8(rt, self + 0xBu, 1u);
    ctx.gpr[2] = 1u;
}

// 0x08C07E24 / 0x08C070FC: the assignment of a vector whose elements are 8 / 16 bytes {begin +0, end +4, capacity end
// +8}; `other == self` returns this. With n = the element count of `other`:
//   - n above this's capacity: a new block of element * n bytes comes from 0x08B35840(0x10, bytes), UninitCopy(other.begin,
//     other.end, block, sp) fills it, DestroyRange(this.begin, this.end, sp + 3) destroys the old elements, a non-null old
//     block is released with 0x08B35254, this[8] = block + bytes, this[0] = block;
//   - n not above this's size: the n elements are assigned (8-byte: two word copies; 16-byte: 0x089641C4) and DestroyTail
//     (the end of them, this.end, sp + 2) destroys the rest;
//   - otherwise the first `size` elements are assigned and UninitCopy(other.begin + (this.end - this.begin & ~(element-1)),
//     other.end, this.end, sp + 1) copies the rest.
// Then this[4] = this[0] + element * n; v0 = this.
template <u32 Element, u32 UninitCopy, u32 DestroyRange, u32 DestroyTail, bool AssignByCall>
void vector_assign(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kShift = Element == 8u ? 3u : 4u;
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    u32 source = word(rt, other);
    u32 source_end = word(rt, other + 4u);
    const u32 begin = word(rt, self);
    const u32 capacity = static_cast<u32>(signed_of(word(rt, self + 8u) - begin) >> kShift);
    u32 count = static_cast<u32>(signed_of(source_end - source) >> kShift);
    const auto assign = [&](u32 destination, u32 from) {
        if (AssignByCall) {
            (void)call(rt, ctx, 0x089641C4u, {destination, from});
        } else {
            put(rt, destination, word(rt, from));
            put(rt, destination + 4u, word(rt, from + 4u));
        }
    };
    u32 bytes;
    if (capacity < count) {
        count <<= kShift;
        const u32 block = call(rt, ctx, 0x08B35840u, {0x10u, count});
        (void)call(rt, ctx, UninitCopy, {source, source_end, block, frame.at(0u)});
        (void)call(rt, ctx, DestroyRange, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
        const u32 old = word(rt, self);
        if (old != 0u) (void)call(rt, ctx, 0x08B35254u, {old});
        put(rt, self + 8u, block + count);
        put(rt, self, block);
        bytes = count;
    } else {
        u32 end = word(rt, self + 4u);
        const u32 size = static_cast<u32>(signed_of(end - begin) >> kShift);
        u32 destination = begin;
        if (!(size < count)) {
            if (signed_of(count) > 0) {
                for (u32 index = 0u; index != count; ++index) {
                    assign(destination, source);
                    source += Element;
                    destination += Element;
                }
                end = word(rt, self + 4u);
            }
            (void)call(rt, ctx, DestroyTail, {destination, end, frame.at(2u)});
        } else {
            if (signed_of(size) > 0) {
                for (u32 index = 0u; index != size; ++index) {
                    assign(destination, source);
                    source += Element;
                    destination += Element;
                }
                source = word(rt, other);
                source_end = word(rt, other + 4u);
                end = word(rt, self + 4u);
            }
            const u32 used = (end - begin) & ~(Element - 1u);
            (void)call(rt, ctx, UninitCopy, {source + used, source_end, end, frame.at(1u)});
        }
        bytes = count << kShift;
    }
    put(rt, self + 4u, word(rt, self) + bytes);
    ctx.gpr[2] = self;
}
void func_08C07E24(Runtime &rt, AllegrexContext &ctx) {
    vector_assign<8u, 0x08C066F4u, 0x08BF0BF0u, 0x08BF1070u, false>(rt, ctx);
}
void func_08C070FC(Runtime &rt, AllegrexContext &ctx) {
    vector_assign<16u, 0x08C0672Cu, 0x08C06684u, 0x08C06560u, true>(rt, ctx);
}

// 0x08B14A18(owner, file, request) -> bytes read, 0 or -1: one read of a buffered stream. request = {buffer +0, length
// +4}; the stream state file[0x10] holds {handle +0, base +4, offset +8, left +0xC, generation +0x10}. The length is
// capped to what is left; a generation 0x08B14724 reports that differs from the stream's is resynced by 0x08B129F4.
// With a positive generation the 64 KiB pages the read touches (base >> 16 .. (base + length - 1) >> 16) are checked with
// 0x08B12D50; when all are ready, 0x08B33E50 reads them and a full read whose generation is unchanged is kept, otherwise
// (and with generation <= 0, which skips the check) 0x08B1297C runs first and the read is retried. A negative result
// stores 0x08B1830C's code at file + 4 and returns -1; a positive one is copied (0x08B13DC8 from base + offset, then
// 0x08C7AAF8 over the buffer) and advances offset / left.

// 0x08AE8874(self = a0, records = a1 (four 12-byte records), values = a2 (four floats)): for each of the four records
// whose signed halfword +0x20 is not zero, the value at values[i] times the scale 0x08CB6148 is truncated and added to
// the signed halfword +0x1E; the entry of the 129-column table 0x08CE02A0 at row +0x1C, column (that sum) >> 5 is
// interpolated by (sum & 0x1F) / 32 and the result scaled by the halfword +0x20 (>> 12); that is 0x08AE69A0(self, +0x1A)
// times a per-kind factor: kind 2/3/4/6/7/16/22/28/34 take ((amount + 0x1000) >> 1 - weight + 0x1000) >> 12, kinds 5/8/11
// add 0x08AE69A0's answer, kinds 9/10/17/23/29/35 run 0x08AD986C((amount * 3 << 10) >> 12) first. The result goes to
// 0x08AE6920(self, +0x1A, result); the other kinds call nothing. v0 = 0.
void func_08AE8874(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 records = ctx.gpr[5];
    const u32 values = ctx.gpr[6];
    const float scale = float_at(rt, 0x08CB6148u);
    for (u32 index = 0u; index < 4u; ++index) {
        const u32 record = records + index * 12u;
        const u32 fields = record + 0x1Au;  // the record's four signed halfwords: key, row, sum, weight
        const s32 weight = signed_half(rt, fields + 6u);
        if (weight == 0) continue;
        const float scaled = psp_mul(float_at(rt, values + index * 4u), scale);
        const s32 key = signed_half(rt, fields);
        const s32 sum = signed_of(wrap_add(signed_half(rt, fields + 4u),
                                           static_cast<s32>(ctx.fpu_float_to_word(scaled, 1u))));
        const s32 row = signed_half(rt, fields + 2u);
        const u32 entry = 0x08CE02A0u + static_cast<u32>((row * 129 + (sum >> 5)) * 2);
        const s32 low = signed_half(rt, entry);
        const s32 high = signed_half(rt, entry + 2u);
        const s32 fraction = sum & 0x1F;
        s32 amount = wrap_add(low, mult_lo(wrap_sub(high, low), fraction) >> 5);
        amount = mult_lo(amount, weight) >> 12;
        const u32 product = call(rt, ctx, 0x08AE69A0u, {self, static_cast<u32>(key)});
        u32 argument;
        switch (key) {
        case 2: case 3: case 4: case 6: case 7: case 16: case 22: case 28: case 34:
            // ((amount + 0x1000) >> 1) - weight + 0x1000, then the product with 0x08AE69A0's answer
            amount = wrap_add(wrap_sub(signed_of(wrap_add(amount, 0x1000)) >> 1, weight), 0x1000);
            argument = static_cast<u32>(mult_lo(signed_of(product), amount) >> 12);
            break;
        case 5: case 8: case 11:
            argument = product + static_cast<u32>(amount);
            break;
        case 9: case 10: case 17: case 23: case 29: case 35: {
            const s32 step = signed_of(static_cast<u32>(amount) * 3u << 10u) >> 12;
            const u32 factor = call(rt, ctx, 0x08AD986Cu, {static_cast<u32>(step)});
            argument = static_cast<u32>(mult_lo(signed_of(product), signed_of(factor)) >> 12);
            break;
        }
        default:
            continue;
        }
        (void)call(rt, ctx, 0x08AE6920u, {self, static_cast<u32>(key), argument});
    }
    ctx.gpr[2] = 0u;  // the last `slti` of the loop
}

// 0x08AFF62C(self, a1, f12 = dt): one integration step; nothing when the scale self[0xD8][0x60] is zero (v0 = self[0xD8]).
// The 64-bit values 0x08C45E40(self + 0x80; scale), 0x08C45FB8(&that, a1) and 0x08C45DC0(&that; dt) build a step through
// 0x08C46718 / 0x08BAE2FC into frame vectors that 0x08C46508 adds to self + 0x38; then 0x08C45E40(self + 0x8C; dt)
// through 0x08C46718 is added to self + 0x44, and 0x08C468E4(self + 0x50, 0x08C45E2C(self + 0x5C, self + 0x44)); the
// accumulators self + 0x80 .. + 0x94 are cleared.
void func_08AFF62C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const float step = ctx.fpr[12];
    const u32 force = self + 0x80u;   // the accumulators the matrices add into
    const u32 torque = self + 0x8Cu;
    const u32 scale_holder = word(rt, self + 0xD8u);
    const float scale = float_at(rt, scale_holder + 0x60u);
    ctx.gpr[4] = force;  // the delay slot of the branch
    ctx.fpr[12] = scale;
    ctx.fpr[0] = scale;  // the `lwc1 f0, 0x60(v0)` before the compare
    ctx.gpr[2] = scale_holder;
    if (scale == 0.0f) return;
    GuestFrame frame(ctx, 0xA0u);
    const auto keep_pair = [&](u32 slot, u32 low, u32 high) {
        put(rt, frame.at(slot), low);
        put(rt, frame.at(slot + 4u), high);
        put(rt, frame.at(0x50u), low);
        put(rt, frame.at(0x54u), high);
    };
    u32 low = call_keep(rt, ctx, 0x08C45E40u, {force}, {scale}).v0;
    keep_pair(0x10u, low, ctx.gpr[3]);
    low = call_keep(rt, ctx, 0x08C45FB8u, {frame.at(0x10u), second}).v0;
    keep_pair(0x18u, low, ctx.gpr[3]);
    low = call_keep(rt, ctx, 0x08C45DC0u, {frame.at(0x18u)}, {step}).v0;
    const u32 handle = ctx.gpr[3];
    put(rt, frame.at(0x20u), low);
    put(rt, frame.at(0x50u), low);
    put(rt, frame.at(0x54u), handle);
    put(rt, frame.at(0x24u), handle);
    const u32 first = word(rt, handle);
    (void)call(rt, ctx, 0x08C46718u, {frame.at(0x40u), first, word(rt, first + 4u)});
    (void)call(rt, ctx, 0x08BAE2FCu, {frame.at(0x34u), frame.at(0x40u), word(rt, handle + 4u)});
    (void)call(rt, ctx, 0x08C46718u, {frame.at(0x28u), frame.at(0x20u), frame.at(0x34u)});
    (void)call(rt, ctx, 0x08C46508u, {self + 0x38u, frame.at(0x28u)});
    low = call_keep(rt, ctx, 0x08C45E40u, {torque}, {step}).v0;
    const u32 high = ctx.gpr[3];
    put(rt, frame.at(8u), low);
    put(rt, frame.at(0x50u), low);
    put(rt, frame.at(0x54u), high);
    put(rt, frame.at(0xCu), high);
    (void)call(rt, ctx, 0x08C46718u, {frame.at(0x40u), frame.at(8u), high});
    (void)call(rt, ctx, 0x08C46508u, {self + 0x44u, frame.at(0x40u)});
    low = call_keep(rt, ctx, 0x08C45E2Cu, {self + 0x5Cu, self + 0x44u}).v0;
    keep_pair(0u, low, ctx.gpr[3]);
    (void)call(rt, ctx, 0x08C468E4u, {self + 0x50u, low, ctx.gpr[3]});
    for (const u32 offset : {0u, 8u, 4u}) put_float(rt, force + offset, 0.0f);
    for (const u32 offset : {0u, 8u, 4u}) put_float(rt, torque + offset, 0.0f);
}
} // namespace
} // namespace new_batch_main_198
} // namespace gt::game

#define NEWMAIN198(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_198::name, gt::ReturnKind::kind)
NEWMAIN198(0x08AA72CCu, func_08AA72CC, Word);
NEWMAIN198(0x08ADC348u, func_08ADC348, Word);
NEWMAIN198(0x088FEB9Cu, func_088FEB9C, Word);
NEWMAIN198(0x08C07E24u, func_08C07E24, Word);
NEWMAIN198(0x08C070FCu, func_08C070FC, Word);
NEWMAIN198(0x08AE8874u, func_08AE8874, Word);
NEWMAIN198(0x08AFF62Cu, func_08AFF62C, Word);

