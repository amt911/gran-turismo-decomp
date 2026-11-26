// New batch main-921 (draft, unverified): 30 small functions (52-64 bytes) written by hand from `tools.disasm` (relocations
// applied, every branch read as the target it prints) as structured C, in the style of new_batch_3.cpp. The gate had no
// record of these (they were skipped as too hard or dropped without a detail line), so each body was re-derived from the asm.
// Delay slots are read as "before the branch / call"; a callee-saved register the body writes is set where the asm sets it
// (a callee may read it) and put back before the return or the tail call; `v0` (and `v1` / `f0` where a caller can read them)
// ends as the asm leaves it on every exit path. A call sets every argument register the asm sets before the `jal`, in the
// order of the EABI (`a0`-`a3`, `t0`-`t3`); a register the asm leaves alone keeps the caller's value. The frame of each body is
// the original's `addiu sp, sp, -N`, on the signature line so `tools.frame_audit` reads it. No names: none has evidence.
//
// Three bodies (0x08B5A9C8, 0x08B5A9FC, 0x08B61B90) end in a `jal` with no epilogue, and the bytes that follow are the next
// function's prologue: their callee (0x08B62498 / 0x08B5AFDC) never returns, so the compiler dropped the epilogue. The
// recompiled reference ends at that call too, and so does each body here.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_921 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The registers a callee's results stay in after a call, as the original's `jal` leaves them. A callee that never writes v0
// hands the caller's previous value on (call_guest starts it with the caller's).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments).v0;
}
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// s0-s1 are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), s0_(ctx.gpr[16]), s1_(ctx.gpr[17]) {}
    ~SavedRegisters() {
        ctx_.gpr[16] = s0_;
        ctx_.gpr[17] = s1_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 s0_;
    u32 s1_;
};

// --- 52 bytes ---

// 0x08B5A9C8: allocates 4 bytes (0x08B61E98), stores the vtable word 0x08CDBE58 in them and hands them to the constructor
// 0x08B62498(object, 0x08CDBE34, 0x08B6240C), which never returns: the original has no epilogue after that `jal`.

// 0x08B5A9FC: the same shape through the vtable word 0x08CDBF78 and the constructor 0x08B62498(object, 0x08CDBE80,
// 0x08B625DC), again with no epilogue.

// 0x08BEFE30 (a0 = this): 0x0896418C(this, 0) runs, then the word at absolute address 0x10 goes to this + 0x10 and v0 = this.

// 0x08C3181C (a1, a2): 0x08C31414(a2, a1) answers a 64-bit value (v0, v1) and leaves a1 as the callee left it; that goes on
// as (a2, a3, a1) with a1 as a0 in the tail call to 0x08C317F4.

// 0x08C319A4: the same through 0x08C31504 and the tail call to 0x08C3197C.

// 0x08C55560 / 0x08C55594 / 0x08C6FE74 / 0x08C6FEA8 (a0 = object, a1 = a function): the function at a1 is called with
// a0 = object + the word at (the object's vtable word - 0xC); then v0 = object. `jalr` leaves the target in v0, so a callee
// that never writes v0 answers it.
void call_through_second(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    ctx.gpr[16] = self;
    const u32 table = word(rt, self);
    call_indirect(rt, ctx, ctx.gpr[5], {self + word(rt, table - 0xCu)});
    ctx.gpr[2] = self;
}

// --- 56 bytes ---

// 0x0889A01C (a0 = an output float, a1 = a holder): the object at holder->0xB04's virtual +0x6C, called with
// (holder, object), and its f0 goes to the output.

// 0x089C6B58 (a0 = this): stores the vtable word 0x08CCD190, releases the member holder at this + 0x188 (0x08AFB0F8) and
// 0x089AE074(this) finishes.

// 0x08ABCE2C (an initializer): 0x08ABC650(caller's a0), 0x08ABC318(0x08CB438C, 1), its result stored at 0x08CFFAA8 (the
// store sits in the next call's delay slot, so before that call runs), 0x08ABBC10(...) and 0x08ABCDE0(...) finishing. The
// last two set no a0 of their own: they run with what the previous callee left there.

// 0x08AC1028 (a0-a3, t0 = a two-word source, t1 = a flag): builds a 16-byte block at sp (the source's two words at +8 and
// +0xC, zeros at +0 and +4), passes it in t0 with t1 masked to a byte, and calls 0x08AC0F08.

// 0x08AC4658 (an initializer): 0x08C7ACD8(caller's a0), 0x08B2EE2C(0x08CFFCB8), 0x08B2B6F4(0x08CFFCF4) and 0x08B35254(its
// result) finishing.

// 0x08B1213C: 0x08B343D0() answers a 64-bit value; a negative high word is replaced by the pair (0, 0).

// 0x08B13358 (a0 = this): 0x08B2EEB8(this + 0x4D8), 0x08B1D7F0(0x08CB4B88, 0) and 0x08C7B220(its result) finishing.

// 0x08B61B90 (a0 = an output word, a1 = a name): *a0 = 0 and 0x08B35FD8(a1, 0x08CB5B14) looks the name up; a nonzero answer
// goes to 0x08B5AFDC(0x08CB5B1C), after which the original has no epilogue (that function never returns).

// 0x08BF97A4 (a0 = this): stores the vtable word 0x08CCED68, releases the member holder at this + 0x1C and 0x089FD82C(this)
// finishes.

// --- 60 bytes ---

// 0x0880BEF8 (a0 = this): releases the holders at this + 0x208, + 0x30 and + 0x28, then 0x08B28DC4(this) finishes.

// 0x0887E978 (a0 = this, f12 = a scale): 0x0887C5A4(this + 0x64), then 0x0888119C(this->0x70) with f12 = the scale finishes;
// f20 carries the scale across the first call and comes back as the caller's.
void func_0887E978(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    const float scale = ctx.fpr[12];
    const float saved_f20 = ctx.fpr[20];
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        ctx.gpr[16] = self;
        ctx.fpr[20] = scale;
        call(rt, ctx, 0x0887C5A4u, {self + 0x64u});
        ctx.fpr[12] = ctx.fpr[20];
        ctx.fpr[20] = saved_f20;
    }
    tail_call(ctx, 0x0888119Cu, {word(rt, self + 0x70u)});
}

// 0x0889968C (a0 = an output float, a1 = a holder): 0x0889968C's holder is the object at holder->0xB04; its virtual +0x74
// is called with (holder, 1, holder) — a1 is overwritten with 1 and a2 still holds the holder — and its f0 goes to the
// output.

// 0x0891352C (a0 = this): 0x08B344E0(this), 0x08912F40(this), 0x08AB93B4(this + 0xB0), the byte at this + 0x29 cleared and
// 0x08B3457C(this) finishing.

// 0x08A7CB20 (a0 = this): fills a 0x20-byte local (0x08A91A38), hands it to 0x08A7C5C8(this, &local) and releases it
// (0x08A5B9EC); v0 is the last call's.

// 0x08AA6298 (a0 = this, a1 = an output pair): 0x08AA623C(this) answers a 64-bit value that is stored at *a1 and +4; v0 =
// this.

// 0x08AC0124 (a0 = this): stores the vtable word 0x08CD6450 (in the first call's delay slot), 0x08AC00D8(this, ...),
// 0x08B28BE0(this + 4) and 0x08B64654(this) finishing.

// 0x08AEEEB8 (a0-a3, t0, t1): 0x08AEED10 sees the eight arguments shifted up by one, with 2 in a1: (a0, 2, a1, a2, a3, t0,
// t1) and t3, which the shuffle leaves holding t0.

// 0x08B0E31C (a0 = this): 0x08B34888(this + 4), the byte at this + 0xE8 cleared, 0x08B34B24(this + 4) finishing.

// 0x08B0EBF0: five no-argument initializers in order, then 0x08B0E9DC(...) finishing. None of the calls sets an argument
// register, so they all run with the caller's a0.

// 0x08B0EC2C: as 0x08B0EBF0 with four initializers (0x08B17F10, 0x08B17F00, 0x08B10FE8, 0x08B17F08) and 0x08B17F18 finishing.

// 0x08B10474 (a0 = this): the byte at this + 0x9C set to 1, 0x08B28C74(this + 0x64), 0x08B2E224(this->0x90) and
// 0x08B103EC(this) finishing.

} // namespace
} // namespace new_batch_main_921
} // namespace gt::game

#define NEW921(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_921::name, gt::ReturnKind::kind)
NEW921(0x0887E978u, func_0887E978, Word);
