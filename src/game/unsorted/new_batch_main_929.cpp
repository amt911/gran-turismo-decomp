// New batch main 929: thirty functions (100-104 bytes) the earlier batches drafted and the gate never verified, rewritten from
// tools.disasm as structured C (named locals, if/loops, G[n] only at the call boundary). A delay slot runs before its branch or
// call (a branch-likely's only when taken); the callee-saved registers the original keeps values in are set where the asm sets
// them (a callee may read them) and come back at exit; v0 ends as the asm leaves it on every path, including the early returns
// and the value a tail call inherits. Frames are the original prologues'. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_929 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A `jal`: afterwards `v0` holds the callee's result, as the instruction after the call sees it.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Saves the caller's s0-s3 and restores them on scope exit (the original's sw/lw of the callee-saved registers).
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

// The same for one callee-saved float register (f20 and up).
class SavedFloat {
public:
    SavedFloat(AllegrexContext &ctx, unsigned reg) : ctx_(ctx), reg_(reg), value_(ctx.fpr[reg]) {}
    ~SavedFloat() { ctx_.fpr[reg_] = value_; }
    SavedFloat(const SavedFloat &) = delete;
    SavedFloat &operator=(const SavedFloat &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned reg_;
    float value_;
};

// --- copying 36-byte records (nine floats) ------------------------------------------------------------------------
// 0x08BF0E8C and 0x08BF100C (a0 = first, a1 = end, a2 = destination): each 36-byte record of [a0, a1) is copied to the
// destination when it is not null (eight floats in pairs from +0, then the one at +0x20), source and destination advancing
// together. The destination pointer advances even on the skipped records (so a null destination makes the second record copy
// to 0x24, as the original does). v0 ends as the advanced destination (a2 itself when the range is empty).
void copy_records(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (begin != end) {
        u32 source = begin;
        do {
            if (destination != 0u) {
                for (unsigned i = 0u; i < 4u; ++i) {
                    put_float(rt, destination + i * 8u, float_at(rt, source + i * 8u));
                    put_float(rt, destination + i * 8u + 4u, float_at(rt, source + i * 8u + 4u));
                }
                put_float(rt, destination + 0x20u, float_at(rt, source + 0x20u));
            }
            source += 0x24u;
            destination += 0x24u;
        } while (source != end);
    }
    ctx.gpr[2] = destination;
}

// --- copying a range of elements, each handed to a release -----------------------------------------------------------------
// 0x08C026FC, 0x08C121C8, 0x08C25298, 0x08C2DC4C and 0x08C30A6C (a0 = first, a1 = end, a2 = destination): every element of
// [a0, a1), `stride` bytes each, is handed to `release` as (destination, element) when the destination is not null, both
// pointers advancing together. v0 ends as the advanced destination (a2 when the range is empty).
void copy_elements(Runtime &rt, AllegrexContext &ctx, u32 stride, u32 release) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    if (begin != end) {
        SavedRegisters saved(ctx);
        ctx.gpr[18] = end;
        u32 source = begin;
        do {
            const u32 next = source + stride;
            ctx.gpr[16] = next;
            ctx.gpr[17] = destination;
            if (destination != 0u) call(rt, ctx, release, {destination, source});
            source = next;
            destination += stride;
        } while (source != end);
    }
    ctx.gpr[2] = destination;
}

// 0x08961310 (a0 = a list's sentinel): every node before the sentinel hands its object at +8 to that object's virtual +0x10
// (called with the object), then 0x08BBC0A0(sentinel) runs and the sentinel is made an empty list. v0 tracks the node while
// walking, so an empty list answers the sentinel.

// 0x0898E41C and 0x08A35978 (a0 = a destination slot): the fill (0x0898E170 / 0x08A354EC) produces a local object (assert
// 0x08CA3CB8 / 0x08CACF04 when null) which is assigned into the destination unless the destination is the local itself, and
// the local is released.

// 0x08A23F00 (a0 = this): vtable word 0x08CD06F8, the base setup 0x08A23A2C(self), the helper held at +0x18 (with the word at
// +0x14) released through its first virtual and both words cleared, then 0x08A780B0(self) as a tail call.

// 0x08A4D37C: the singleton at 0x08CF68E8, when it is set: 0x08A4DD4C(it), the global is cleared and the instance (read
// again) goes to 0x08A4D308 and then to 0x08B64654 as a tail call (the release chain).

// 0x08A6538C (a0 = this, a2 passed on): the byte 7 at +4 and the table word 0x08CD29A8 at +0, the members at +8 (0x08A6972C)
// and +0xC (0x08A75D04 with a2) set up, and 0x08A5D438(self + 0x18) as the tail call.

// 0x08A78D20 (a0 = a destination slot, a1 = a slot holding the object): the object at word(a1) - assert 0x08CB0E38 when
// null - goes to 0x08A77444(&local, object); the destination receives the local's word unless it is the local itself, and the
// local is released.
void func_08A78D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 destination = ctx.gpr[4];
    SavedRegisters saved(ctx);
    ctx.gpr[17] = destination;
    const u32 object = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
    call(rt, ctx, 0x08A77444u, {frame.at(0u), object});
    if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08ACCE9C (a0 = object, a2 = a value): the member at this + 0x2C8 is offered 0x08C7AF90 with a0 only (a1 as it came);
// when that answers 0, 0x08C7AFB0 gets (member, the value, scratch at sp). v0 = 1 only when both answered 0.

// 0x08B09448 (a0 stored at sp + 0xC): a four-word record {0, 0x2000, 0x08B098A8, a0} is built on the stack, 0x08B2E314(3, -6)
// and 0x08B2E134 feed 0x08B2E7DC(record + 8, record, size); its result is stored to the global 0x08D4744C and handed to
// 0x08B2E214 (the store sits in that call's delay slot). v0 = the last call's result.

// 0x08B5A270 (a0 = a table): the slot number comes from 0x08B5A018(table, a1); unless it is 0xFF the record is prepared
// (0x08B59B38(table, table + 0x202, 2, slot)) and the word at table + slot * 16 + 8 is the answer. v0 = 0 for slot 0xFF.

// 0x08C52288 (a0 = a circular list's head): every node's member at +0x14 is released (0x08B61FC0) when present and the node
// itself is freed (0x08B35254), walking the ring until the next node is the head again. v0 ends as the last free's result
// (or the head's first word when the ring is empty).

// 0x08C79E44 (a0 = destination, a1 = source object): the primary vtable of the source is stored at the destination with its
// secondary pointer at the displacement the vtable names (from source +0xC), the shared part is initialised (0x08C786B4) with
// the destination's sub-object at +4, and the same for the source's second vtable (its pointer from source +8 at its own
// displacement). v0 ends as the source's second vtable pointer.

// 0x0885E430 (a0 = an object): four getters are chained (0x08916474, 0x08916194, 0x088C7F64, 0x088C78B4), the word at
// +0x5AC of word(self) is flipped to 1 (0x088D7214) and back to 0 (0x088DAFBC), and the member at
// word(word(self) + 0x3F50) + 0x7C finishes as a tail call (v0 = word(self)).

// 0x08869D70 (a0 = an object): 0x08869CCC(object, 0), 0x08869CE8(object, 0), then the block at word(object + 4) + 0x4238 is
// initialised twice (0x0886BFC0, 0x0886BF44) and stepped four times (0x0886B600, no argument registers read).

// 0x08885E48 (a0 = this): the vtable 0x08CC16E8 is stored, 0x0888598C(this) runs, the owned object at +0xC is released
// (0x08885CEC, then 0x08B64654) and cleared, then the vtable 0x08CBF428 is stored and 0x08B64654(this) finishes.

// 0x0892B974 (a0 = this): vtable 0x08CC6740, the inline member at +0x260 (pointer + owned) released through its first virtual
// when present and both words cleared, 0x08B22550(this), then 0x08B64654(this) as a tail call.

// 0x089B17B8 (a0 = this): 0x089B17AC(this), then five members are reset (0x089B0044, 0x089AB008, 0x089B05F8, 0x089B08E4,
// 0x089AFF3C), each with a zero argument, and 0x089B0284(this, 0) finishes.

// 0x089BF568 (a0 = this, f12 = a scale): the member at +0x2B0 is locked (0x08B344E0), the scale is kept at +0x244 and in
// f20, the object at +0x264 runs its virtual at +0x6C with the scale in f12, then the member is released (0x08B3457C). f20
// comes back as the caller left it; v0 is the held object (0 when there is none).
void func_089BF568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const float scale = ctx.fpr[12];
    SavedFloat saved_f20(ctx, 20u);
    {
        SavedRegisters saved(ctx);
        ctx.gpr[17] = self + 0x2B0u;
        ctx.gpr[16] = self;
        ctx.fpr[20] = scale;
        call(rt, ctx, 0x08B344E0u, {self + 0x2B0u});
        ctx.fpr[12] = scale;
        const u32 held = word(rt, self + 0x264u);
        ctx.gpr[2] = held;
        put_float(rt, self + 0x244u, scale);
        if (held != 0u) {
            const u32 function = word(rt, word(rt, held) + 0x6Cu);
            ctx.fpr[12] = scale;
            call_indirect(rt, ctx, function, {held});
        }
    }
    tail_call(ctx, 0x08B3457Cu, {self + 0x2B0u});
}

// 0x089DA558 (a0 = this, a1 = an index): the byte at +0x8C may first correct the index through the virtual at vtable +0x1C8
// (answer - index - 1), then the virtual at +0x1D0(this, index) names an object whose float at +0x14 is the answer, in f0.

// 0x08A2703C (a0 = this): the callback at +0x30, when present, runs with a0 = this and a1 as it came, and its answer (0 or a
// value) is kept; 0x08B344E0(this) locks, the byte answer goes to +0x2D, a nonzero answer arms the timer 0x000F4240 at +0x34,
// the byte +0x2C becomes 1 and 0x08B3457C(this) finishes. v0 = 1.

// 0x08A3DB74 (a0 = this): vtable 0x08CD13B0, 0x08A3D718(word(self + 0x14)), then the inline member at +0x14 (pointer + owned)
// released through its first virtual when present and both words cleared, and 0x08A780B0(self) as a tail call.

// 0x08A562F0 (a0 = an answer object, a1 = holder): the holder's object at +0x14, or 0x08CAF798 when there is none, gets the
// virtual at vtable +0x128 with (answer, object, holder). v0 = a0.

// 0x08A60FB0 (a0 = an object): a smart pointer is filled from 0x08A60D90(sp, a1) (null is an error, 0x08CB01D8), the object
// is looked up (0x08A60BF4) and, unless it is `self`, released (0x08A5BAD8(self, its word)); the temporary is released
// (0x08A5B9EC(sp)). v0 = that last release's result.

} // namespace
} // namespace new_batch_main_929
} // namespace gt::game

#define NEW929(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_929::name, gt::ReturnKind::kind)
NEW929(0x089BF568u, func_089BF568, Word);
NEW929(0x08A78D20u, func_08A78D20, Word);
