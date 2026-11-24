// New batch main-932: functions drafted before and either mismatched the gate or were skipped as too hard, rewritten
// by hand from `tools.disasm` (relocations applied, branches resolved). Delay slots are read as "before the branch /
// call"; the callee-saved registers the original keeps values in are set where the asm sets them (a callee may read
// them) and put back at the end; `v0` ends as the asm leaves it on every exit path. Guest memory goes through
// word()/put()/rt.memory(), calls through call_set()/call_indirect() with every argument register the original sets,
// and a function that ends with `j target` pops its frame and finishes with tail_call().
// Fixed against the old drafts: 0x08B56D4C loaded the second matrix into M000 instead of M100 (the VFPU mismatch the
// gate saw), 0x08B0A9F4 had no "index == end at entry" guard (16 loads instead of none) and 0x08B2DEE4 passed the
// wrong $a1 to two callees. 0x08B17104 is skipped: it hands this + 0x2C to 0x08B7B34C, the longjmp half of a
// setjmp record (it reloads s0-s7, sp, fp and ra from memory), and then relies on the restored ra and a0, which native
// code cannot follow. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_932 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call whose 64-bit answer the caller reads in both `v0` and `v1` (0x08B2DEE4 returns one).
u32 call_set_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result.v0;
}
// `lh` / `lhu`-style reads: the 16-bit field sign-extended to a 32-bit word.
u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }

// s0-s7 and $fp are callee-saved: the body sets them where the asm sets them and the caller's values come back. $ra is
// left to call_guest and the harness, which own it natively.
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

// --- 0x088ECF3C: locks this + 0x1C8, lets the virtual at +0x74 of *this see an offset whose flag byte is set and
// clears that byte, then finishes on 0x08B3457C(this + 0x1C8). v0 = the tail call's.

// --- 0x08927278: calls the virtual at +0x10 of each of the 13 objects at this + 0x1F70 + 4 * i (the counter goes to
// 13 and the pointer past the last slot before each call), then stores -1 at +0x1FB0 and the first object at +0x1FA4.

// --- 0x089BFB70: tears this down (0x089E0288), and while the member at +0x264 is still empty moves a temporary built
// from +0x22C into it and releases that temporary; both steps run between two locks of the block at +0x2B0.

// --- 0x089C12F4: hands the argument to the member at +0x230 and to the held object's virtual at +0x1C (with +0x22C),
// between two locks of the block at +0x2B0.

// --- 0x089D8474: stores the float argument at +0x1C of the entry the virtual at +0x1D4 of *this answers for it; in
// reverse order (the byte at +0x8C set) the index is mirrored through the virtual at +0x1C8 first.

// --- 0x089DA370: whether the entry the virtual at +0x1D0 of *this answers has an empty word at +8 — or, in reverse
// order (the byte at +0x8C set, the index mirrored through +0x1C8 first), whether it is no longer empty.

// --- 0x08A60E5C: resets the list head at +0x18 (through 0x08C1F510) and links it to itself, after moving a temporary
// built from +0x14 around; v0 = 0x08C1F510's answer.

// --- The wrappers around the property bag: a0 = destination, the object comes from the holder pointer (a1, or a2 in
// the 08A733AC / 08A96120 / 08AB* ones), the object is asked through the virtual at `member_offset`, the answer is
// wrapped by `constructor` into a temporary at sp, applied with 0x08A76ABC and the temporary released at the end.
// "apply" answers the destination in v0; "assign" starts with `beq a0, sp`, so it copies into the caller's reference
// only when the destination is not the temporary, and answers what the release returns.
void apply_member_from_holder(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 member_offset, u32 constructor,
                              u32 assert_message) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {assert_message});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + member_offset), {object});
    call_set(rt, ctx, constructor, {frame.at(0u), value});
    call_set(rt, ctx, 0x08A76ABCu, {self, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = self;
}
void assign_member_from_holder(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 member_offset, u32 constructor,
                               u32 assert_message) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {assert_message});
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + member_offset), {object});
    call_set(rt, ctx, constructor, {frame.at(0u), value});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_08A79040(Runtime &rt, AllegrexContext &ctx) {
    assign_member_from_holder(rt, ctx, ctx.gpr[5], 0x48u, 0x08AB022Cu, 0x08CB0E38u);
}
void func_08AA2508(Runtime &rt, AllegrexContext &ctx) {
    apply_member_from_holder(rt, ctx, ctx.gpr[5], 0x48u, 0x08AB022Cu, 0x08CB2B50u);
}
void func_08AA257C(Runtime &rt, AllegrexContext &ctx) {
    apply_member_from_holder(rt, ctx, ctx.gpr[5], 0x38u, 0x08A7334Cu, 0x08CB2B50u);
}

// --- 0x08ABAB9C: links the node into the list at +0x44/+0x48 of this once (its byte at +0x10 guards it), then
// finishes on the lock at +0x14 with v0 = its answer.

// --- 0x08ACB484 and 0x08ACC640: locks the block at +0x60, waits out the flag byte at +0x8D (each pass releases and
// takes the lock again), runs the virtual at +0x20, takes the word at +0x90 of this as the answer and unlocks.
void run_while_flag_and_take(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0x60u;
    call_set(rt, ctx, 0x08B34888u, {self + 0x60u});
    while (rt.memory().load8(self + 0x8Du) != 0u) call_set(rt, ctx, 0x08B34B2Cu, {self + 0x60u});
    const u32 table = word(rt, self);
    ctx.gpr[2] = table;
    call_indirect(rt, ctx, word(rt, table + 0x20u), {self});
    const u32 result = word(rt, self + 0x90u);
    ctx.gpr[16] = result;  // s0 = word(this + 0x90), right after the virtual's call
    call_set(rt, ctx, 0x08B34B24u, {self + 0x60u});
    ctx.gpr[2] = result;
}

// --- 0x08B0A9F4: locks the block at +0x48, walks the 16-slot ring at +8 from the index at +4 up to (not including)
// the one at +0, keeps the last slot read (each read sits in its branch's delay slot, so the exit iteration reads
// too), unlocks and answers that word.
void func_08B0A9F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[18] = self + 0x48u;
        ctx.gpr[17] = 0u;
        call_set(rt, ctx, 0x08B344E0u, {self + 0x48u});
        const u32 first = word(rt, self + 4u);
        const u32 end = word(rt, self);
        if (first != end) {
            u32 index = first;
            for (;;) {
                result = word(rt, self + index * 4u + 8u);
                ctx.gpr[17] = result;
                index = (index + 1u) & 0xFu;
                if (index == end) break;
            }
        }
        call_set(rt, ctx, 0x08B3457Cu, {self + 0x48u});
    }
    ctx.gpr[2] = result;
}

// --- 0x08B2DEE4: three guest calls on three stack objects (0x08C7B008(&first), 0x08C7B000(&first, &second),
// 0x08C7B010(&third, &second)). A nonzero answer either time returns -1 in v0 and v1; all zero goes on in
// 0x08B2DE94(&third, &second) and answers its result.

// --- 0x08B56D4C: when bit 10 of the word at +0xD1C is set, clears it, multiplies the matrix 0x08B56A88(this) by the
// one at *(this + 0xD00) (vmmul.q M000, M200, M100) and stores the four columns of the product at +0x840 .. +0x870.
// v0 = this + 0x800 either way.

// --- 0x08B62498: fills the exception record at a0 - 0x50 (vtable 0x08B62450, the two arguments, the globals
// 0x08CE0E08 / 0x08CE0E54 and the constants 0x432B2B00 / 0x474E5543), then 0x08B758A4(record + 0x30, a1, a2, record),
// 0x08B62198(record + 0x30) and 0x08B6459C(record + 0x30), which does not return.

// --- 0x08B7B34C: reloads the saved s0-s7, sp, fp, ra and f20-f31 from the block at a0 (the longjmp half of a setjmp
// record — the restored ra is where this returns) and answers a1, or 1 when a1 is zero.

// --- 0x08C2C46C: copies 0xC-byte records from a0 up to a1 into a2 (0x08A6972C prepares each destination, its word +4
// is copied and 0x08A76ABC copies +8). Answers the output pointer; a zero destination skips the copy but still
// advances, and a0 == a1 answers the output untouched.

// --- 0x08C48FC4: inserts the halfword index a1 into the sorted run that ends at a0, comparing against the floats at
// +0x98 of the records *(a2) (a count, times four) in the table *(a2 + 4) index by record index. The run is walked
// backwards from a0 - 2, shifting a neighbour up while the new element's float is below it. v0 = the last
// neighbour's record address (table + neighbour * 0xF0 + count * 4), as the original leaves it.
void func_08C48FC4(Runtime &rt, AllegrexContext &ctx) {
    const u32 end = ctx.gpr[4];
    const u32 index = sext16(ctx.gpr[5]);
    const u32 count_offset = word(rt, ctx.gpr[6]) << 2u;
    const u32 table_base = word(rt, ctx.gpr[6] + 4u);
    const u32 new_offset = (index << 8u) - (index << 4u);  // the asm's `sll 8` / `sll 4` difference
    u32 hole = end;
    u32 position = end - 2u;
    for (;;) {
        const u32 neighbour = sext16(rt.memory().load16(position));
        const u32 table = word(rt, table_base);  // reloaded every pass, as the original does
        const u32 theirs_at = table + ((neighbour << 8u) - (neighbour << 4u)) + count_offset;
        const u32 ours_at = table + new_offset + count_offset;
        ctx.gpr[2] = theirs_at;
        ctx.gpr[3] = ours_at;
        const float theirs = float_at(rt, theirs_at + 0x98u);
        const float ours = float_at(rt, ours_at + 0x98u);
        rt.memory().store16(hole, static_cast<std::uint16_t>(neighbour));  // the branch's delay slot
        if (!(ours < theirs)) break;
        hole = position;
        position -= 2u;
    }
    rt.memory().store16(hole, static_cast<std::uint16_t>(index));  // the jr's delay slot
}

// --- 0x0885E940: ten steps on the handle at +0x5AC of *(this), each taking the previous one's answer, then
// 0x08866FA8(*this) finishes: 0x088D6E9C(handle), 0x088CBFD4, 0x088CBE6C(v0), 0x088CA548, 0x088C9780(v0),
// 0x08916474, 0x08916194(v0), 0x088C7F64, 0x088C78B4(v0), 0x08B1C47C.

// --- 0x0886DF90: 0x0886A9A0(this, 6) gates a preparatory 0x0886AAA4(this, *(this + 0x70) + 0x28); then
// 0x08874BE0(*(this + 0x70) + 0x28, value) answers 1, or answers 0 after 0x0886AA0C(this, 6). v0 = that 1 or 0.

// --- 0x08882928: stores vtable 0x08CC1460 and prepares the member at +0xC (0x08882854(*(this + 0xC), *(this + 8)));
// the helper at +0x10 and the member are released through the first virtual of the helper's table (a1 = *(this +
// 0xC)) and both words cleared; vtable 0x08CBF428 is stored and 0x08B64654(this) finishes.

} // namespace
} // namespace new_batch_main_932
} // namespace gt::game

#define NEW932(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_932::name, gt::ReturnKind::kind)
NEW932(0x08A79040u, func_08A79040, Word);
NEW932(0x08AA2508u, func_08AA2508, Word);
NEW932(0x08AA257Cu, func_08AA257C, Word);
NEW932(0x08B0A9F4u, func_08B0A9F4, Word);
NEW932(0x08C48FC4u, func_08C48FC4, Word);
