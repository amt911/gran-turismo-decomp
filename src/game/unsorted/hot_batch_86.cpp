// Hot batch 86: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five routes with --decomp off;
// batch 85 = PR #210 excluded with --exclude-ref origin/decomp/hot-batch-85, and no address that #210 writes is taken here). Before reading any function its address was searched for in src/ and in the branch of batch 85.
// Candidates without VFPU, without imports that the harness cannot replay (calls into the import stubs from 0x08C70000 on were left out), without variadic spills, endless loops and multi-entry blobs; each was read by hand
// word by word with `tools/disasm.py` (branch targets and relocations resolved by the tool, every delay slot part of its branch's path).
//
//  - guarded-static constructors (func_08892A7C, func_08894574, func_088FEA94, func_088F5670, func_0887C7A0, func_0886E7A8), small constructors and forwarders (func_0884CC18, func_0884CD28, func_08A042F4, func_08A29588 and the
//    other class registrations, ...), destructors (func_0886A804, func_088742C8, func_08882DC0, func_0887104C, func_0886E980) and the tree walker func_08B9AC68;
//  - string temporaries with the copy-on-write release (func_0899CD50, func_0897FDE0, func_08A6BBE8, func_089AF5E8, func_08A17710, ...), a pointer-to-member call (func_08BEBECC), flag accessors (func_089E18BC, func_089E19BC),
//    the update of a menu object (func_08869444, func_0886968C) and a chain walk with a float (func_08B03F10).
//
// Written but not registered (the commented HOT86 lines and each function's comment say why): compared 0 (func_088733F4, func_0889844C, func_088A34BC, func_08B95080, func_08B95120, func_08BC1FE8, func_08C34270), waits and imports the harness does not replay (func_088A5E5C, func_0892FCE0, func_08A13844, func_08AC8FDC), compared only on an early exit or an empty range (func_08900350, func_08BE78E8, the vector erases func_08C49A28,
// func_08C49C28, func_08C49D08, func_08B89338, func_08B895E0, func_08C0DBBC, func_08C246E4), no compared case that changed memory (func_08B043D8, func_08872E18, func_08C49930), and func_0880BEF8, whose single call per menu route destroys the
// whole member tree: its cases swallow the compared calls of ~56 already decompiled descendants (the full gate's compared total fell by 55 with it registered; FINDINGS), so it stays recompiled.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest copies from ctx.
// Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's), and the callee-saved registers (s0-s7, fp) hold the original's values around the calls (`SavedRegs`
// restores the entry values on the way out, like the original's epilogue): a callee may store the caller's s0 (FINDINGS, batch 84).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_86 {
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

// The callee-saved registers s0-s7 and fp as they were at entry: the original saves them in its frame and restores them before it returns or tail-calls.
class SavedRegs {
public:
    explicit SavedRegs(psprecomp::AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_fp_ = ctx.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    std::uint32_t saved_[8];
    std::uint32_t saved_fp_;
};

inline float fbits(std::uint32_t bits) { return std::bit_cast<float>(bits); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }


// The slot at `holder` + 4 of a destructor: a non-null object o there has its vtable slot 0 called with (o, word at `value_address`) (`value_address` = the word the original reads in the delay slot of the call).
inline void drop_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder_slot, std::uint32_t value_address) {
    const std::uint32_t object = word(rt, holder_slot);
    ctx.gpr[3] = object;
    ctx.gpr[4] = object;
    if (object == 0u) return;
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    const std::uint32_t argument = word(rt, value_address);
    ctx.gpr[5] = argument;
    (void)call_keep(rt, ctx, function, {object, argument});
}

// func_088742C8(this): this[0] = D_08CBFA98, func_088740A8(this[0xC]); the object o = this[0x10], when set, has its vtable slot 0 called with (o, this[0xC]); this[0xC] = this[0x10] = 0; this[0] = D_08CBF428.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object this[0x10] is never null (the `beqz v1` at 0x088742F8 would skip its release).
void func_088742C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = 0x08CBFA98u;
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0xCu;
    const std::uint32_t member = word(rt, self + 0xCu);
    ctx.gpr[4] = member;
    put(rt, self, 0x08CBFA98u);
    (void)call_keep(rt, ctx, 0x088740A8u, {member});
    const std::uint32_t object = word(rt, self + 0x10u);
    if (object != 0u) {
        drop_holder(rt, ctx, self + 0x10u, self + 0xCu);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0xCu, 0u);
    }
    ctx.gpr[2] = 0x08CBF428u;
    put(rt, self, 0x08CBF428u);
}

// func_08882DC0(this): this[0] = D_08CC1460, func_08882854(this[0xC], this[8]); the object o = this[0x10], when set, has its vtable slot 0 called with (o, this[0xC]); this[0xC] = this[0x10] = 0; this[0] = D_08CBF428.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object this[0x10] is never null (the `beqz v1` at 0x08882DF4 would skip its release).
void func_08882DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = 0x08CC1460u;
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0xCu;
    const std::uint32_t member = word(rt, self + 0xCu);
    const std::uint32_t other = word(rt, self + 8u);
    ctx.gpr[4] = member;
    ctx.gpr[5] = other;
    put(rt, self, 0x08CC1460u);
    (void)call_keep(rt, ctx, 0x08882854u, {member, other});
    const std::uint32_t object = word(rt, self + 0x10u);
    if (object != 0u) {
        drop_holder(rt, ctx, self + 0x10u, self + 0xCu);
        put(rt, self + 0xCu, 0u);
        put(rt, self + 0x10u, 0u);
    }
    ctx.gpr[2] = 0x08CBF428u;
    put(rt, self, 0x08CBF428u);
}

// func_088733F4(this): this[0xC] = D_08CBFA2C, this[0] = D_08CBFA00; the object o = this[0x60], when set, has its vtable slot 0 called with (o, this[0x5C]); this[0x60] = this[0x5C] = 0; this[0xC] = D_08CD7310, func_08AF484C(this + 0xC); this[0] = D_08CBF428.
// Not registered: compared 0 in the fast gate.
void func_088733F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self + 0x5Cu;
    ctx.gpr[16] = self;
    put(rt, self + 0xCu, 0x08CBFA2Cu);
    put(rt, self, 0x08CBFA00u);
    ctx.gpr[3] = word(rt, self + 0x60u);
    ctx.gpr[4] = ctx.gpr[3];
    if (word(rt, self + 0x60u) != 0u) {
        drop_holder(rt, ctx, self + 0x60u, self + 0x5Cu);
        put(rt, self + 0x60u, 0u);
        put(rt, self + 0x5Cu, 0u);
    }
    ctx.gpr[2] = 0x08CD7310u;
    put(rt, self + 0xCu, 0x08CD7310u);
    ctx.gpr[4] = self + 0xCu;
    (void)call_keep(rt, ctx, 0x08AF484Cu, {self + 0xCu});
    ctx.gpr[2] = 0x08CBF428u;
    put(rt, self, 0x08CBF428u);
}

// func_088A34BC(this): this[0] = D_08CC2800, this[4] = D_08CC2898, func_088A322C(this); the object o = this[0x5C], when set, has its vtable slot 0 called with (o, this[0x58]); this[0x5C] = this[0x58] = 0; this[4] = D_08CC28E0, this[0] = D_08CC28F8;
// the tail call is func_08B64654(this).
// Not registered: compared 0 in the fast gate.
void func_088A34BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = 0x08CC2898u;
        ctx.gpr[17] = self + 0x58u;
        ctx.gpr[16] = self;
        put(rt, self, 0x08CC2800u);
        put(rt, self + 4u, 0x08CC2898u);
        (void)call_keep(rt, ctx, 0x088A322Cu, {self});
        if (word(rt, self + 0x5Cu) != 0u) {
            drop_holder(rt, ctx, self + 0x5Cu, self + 0x58u);
            put(rt, self + 0x5Cu, 0u);
            put(rt, self + 0x58u, 0u);
        } else {
            ctx.gpr[3] = 0u;
            ctx.gpr[4] = 0u;
        }
        ctx.gpr[2] = 0x08CC28E0u;
        put(rt, self + 4u, 0x08CC28E0u);
        ctx.gpr[2] = 0x08CC28F8u;
        put(rt, self, 0x08CC28F8u);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_0887104C(this): this[0] = D_08CBF948; the object p = (this[0xC])[0x80], when set, has its vtable slot 0x2C called with (p) and (this[0xC])[0x80] = 0; the object o = this[0x10], when set, has its vtable slot 0 called with (o, this[0xC]),
// this[0xC] = this[0x10] = 0; this[0] = D_08CBF428.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the child (this[0xC])[0x80] is never set (the vtable slot 0x2C call and the store of 0 at 0x08871080-0x08871094 are read from the asm only) and the object this[0x10] is never null (the `beqz v1` at 0x0887109C would skip its release).
void func_0887104C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = 0x08CBF948u;
    ctx.gpr[17] = self + 0xCu;
    ctx.gpr[16] = self;
    const std::uint32_t member = word(rt, self + 0xCu);
    ctx.gpr[3] = member;
    put(rt, self, 0x08CBF948u);
    const std::uint32_t child = word(rt, member + 0x80u);
    ctx.gpr[2] = child;
    ctx.gpr[4] = child;
    if (child != 0u) {
        const std::uint32_t vtable = word(rt, child);
        const std::uint32_t function = word(rt, vtable + 0x2Cu);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = function;
        (void)call_keep(rt, ctx, function, {child});
        ctx.gpr[2] = word(rt, self + 0xCu);
        put(rt, ctx.gpr[2] + 0x80u, 0u);
    }
    const std::uint32_t object = word(rt, self + 0x10u);
    ctx.gpr[3] = object;
    ctx.gpr[4] = object;
    if (object != 0u) {
        drop_holder(rt, ctx, self + 0x10u, self + 0xCu);
        put(rt, self + 0xCu, 0u);
        put(rt, self + 0x10u, 0u);
    }
    ctx.gpr[2] = 0x08CBF428u;
    put(rt, self, 0x08CBF428u);
}

// func_0889844C(this): this[0] = D_08CC20A8; the object a = this[0x38], when set, has its vtable slot 4 called with (a); this[0x38] = 0; the object b = this[0x30], when set, has its vtable slot 4 called with (b); func_088F897C(this) with this[0x30] = 0 in its delay slot;
// the tail call is func_08B64654(this).
// Not registered: compared 0 in the fast gate.
void func_0889844C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[16] = self;
        ctx.gpr[2] = 0x08CC20A8u;
        const std::uint32_t first = word(rt, self + 0x38u);
        ctx.gpr[3] = first;
        put(rt, self, 0x08CC20A8u);
        ctx.gpr[4] = first;
        if (first != 0u) {
            const std::uint32_t vtable = word(rt, first);
            const std::uint32_t function = word(rt, vtable + 4u);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {first});
        }
        const std::uint32_t second = word(rt, self + 0x30u);
        ctx.gpr[2] = second;
        put(rt, self + 0x38u, 0u);
        ctx.gpr[4] = second;
        if (second != 0u) {
            const std::uint32_t vtable = word(rt, second);
            const std::uint32_t function = word(rt, vtable + 4u);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {second});
        }
        ctx.gpr[4] = self;
        put(rt, self + 0x30u, 0u);
        (void)call_keep(rt, ctx, 0x088F897Cu, {self});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_0886E980(this): this[0] = D_08CBF7D8, func_0886AA0C(this, 5), func_0886AA0C(this, 2); the object o = this[0x74], when set, has its vtable slot 0 called with (o, this[0x70]); this[0x74] = this[0x70] = 0; func_0886A858(this); the tail call is func_08B64654(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object this[0x74] is never null (the `beqz v1` at 0x0886E9BC would skip its release).
void func_0886E980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = 0x08CBF7D8u;
        ctx.gpr[5] = 5u;
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x70u;
        put(rt, self, 0x08CBF7D8u);
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 5u});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 2u});
        const std::uint32_t object = word(rt, self + 0x74u);
        ctx.gpr[3] = object;
        ctx.gpr[4] = object;
        if (object != 0u) {
            drop_holder(rt, ctx, self + 0x74u, self + 0x70u);
            put(rt, self + 0x74u, 0u);
            put(rt, self + 0x70u, 0u);
        }
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886A858u, {self});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_08A83E98(this, out, a2, ptr): o = *ptr (null: func_08A5BB7C(D_08CB168C) runs first); lo:hi = o's vtable slot 0x50 called with (o); func_08A83160(sp, out, this[0x18] & lo, this[0x1C] & hi); unless `out` is sp func_08A5BAD8(out, [sp]); func_08A5B9EC(sp).
// v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object *ptr is never null (the func_08A5BB7C assertion call is skipped by the `bnez s0` at 0x08A83EC4) and `out` is never the holder at sp (the copy func_08A5BAD8 always runs; `beq s1, sp` at 0x08A83EF4 would skip it).
void func_08A83E98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5], ptr = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    const std::uint32_t object = word(rt, ptr);
    ctx.gpr[16] = object;
    const std::uint32_t low_mask = word(rt, self + 0x18u), high_mask = word(rt, self + 0x1Cu);
    ctx.gpr[18] = low_mask;
    ctx.gpr[19] = high_mask;
    ctx.gpr[4] = 0x08CB168Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB168Cu});
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x50u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = object;
    const GuestResult wide = call_keep(rt, ctx, function, {object}, {});
    const std::uint32_t lo = low_mask & wide.v0, hi = high_mask & wide.v1;
    ctx.gpr[6] = lo;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[7] = hi;
    (void)call_keep(rt, ctx, 0x08A83160u, {frame.at(0u), out, lo, hi});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08BEBECC(this, ref, a2): a pointer-to-member call: obj = *ref, adjusted = obj + ((this[0x18] as signed) >> 1); the function is this[0x14] itself, or when bit 0 of this[0x18] is set the vtable word at (vtable of adjusted) + this[0x14];
// r = function(adjusted, obj, this[0x14], this[0x18]); then s0 = r + (this[0x28] >> 1) and the second function is this[0x24] itself or, with bit 0 of this[0x28], the vtable word at (vtable of s0) + this[0x24];
// func_08BE9098(sp, a2) fills the frame and the second function is called with (s0, sp). v0 is its.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): bit 0 of this[0x18] and of this[0x28] is always 0 (a non-virtual pointer-to-member: the function is this[0x14] / this[0x24] itself); the virtual form (the vtable word at (vtable of the object) + the offset, 0x08BEBF14-0x08BEBF1C and 0x08BEBF40-0x08BEBF50) is read from the asm only.
void func_08BEBECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], ref = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = a2;
    ctx.gpr[17] = self;
    const std::uint32_t flags = word(rt, self + 0x18u);
    const std::uint32_t object = word(rt, ref);
    const std::uint32_t field = word(rt, self + 0x14u);
    const std::uint32_t adjusted = object + static_cast<std::uint32_t>(signed_of(flags) >> 1);
    std::uint32_t function = field;
    if ((flags & 1u) != 0u) function = word(rt, word(rt, adjusted) + field);
    ctx.gpr[2] = function;
    ctx.gpr[7] = flags;
    ctx.gpr[6] = field;
    ctx.gpr[5] = object;
    ctx.gpr[4] = adjusted;
    const std::uint32_t result = call_keep(rt, ctx, function, {adjusted, object, field, flags});
    ctx.gpr[4] = result;
    const std::uint32_t second_flags = word(rt, self + 0x28u);
    const std::uint32_t target = result + static_cast<std::uint32_t>(signed_of(second_flags) >> 1);
    ctx.gpr[16] = target;
    const std::uint32_t second_field = word(rt, self + 0x24u);
    std::uint32_t second = second_field;
    if ((second_flags & 1u) != 0u) second = word(rt, word(rt, target) + second_field);
    ctx.gpr[17] = second;
    ctx.gpr[5] = a2;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08BE9098u, {frame.at(0u), a2});
    ctx.gpr[4] = target;
    ctx.gpr[5] = frame.at(0u);
    ctx.gpr[17] = second;
    (void)call_keep(rt, ctx, second, {target, frame.at(0u)});
}

// func_08C34270(a0, sink, ref): o = *ref (null: func_08A5BB7C(D_08CB30B8) runs first); n = o's vtable slot 0x7C called with (o); func_08C31A00(sink, n); then for i = 0..n: o = *ref (read again, null: func_08A5BB7C first), the temporary at sp
// is filled by o's vtable slot 0x84 called with (sp, o, i), func_08C34248(sink, sp) and func_08A5B9EC(sp). v0 is the last call's.
// Not registered: compared 0 in the fast gate.
void func_08C34270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t sink = ctx.gpr[5], ref = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = ref;
    std::uint32_t object = word(rt, ref);
    ctx.gpr[16] = object;
    ctx.gpr[19] = sink;
    ctx.gpr[4] = 0x08CB30B8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t function = word(rt, vtable + 0x7Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = object;
    const std::uint32_t count = call_keep(rt, ctx, function, {object});
    ctx.gpr[18] = count;
    ctx.gpr[4] = sink;
    ctx.gpr[5] = count;
    (void)call_keep(rt, ctx, 0x08C31A00u, {sink, count});
    if (count == 0u) return;
    ctx.gpr[22] = frame.at(0u);
    std::uint32_t index = 0u;
    ctx.gpr[17] = 0u;
    ctx.gpr[21] = 0x4B0000u;
    object = word(rt, ref);
    for (;;) {
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08CB30B8u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
        vtable = word(rt, object);
        function = word(rt, vtable + 0x84u);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = function;
        ctx.gpr[4] = frame.at(0u);
        ctx.gpr[5] = object;
        ctx.gpr[6] = index;
        ctx.gpr[17] = index + 1u;
        (void)call_keep(rt, ctx, function, {frame.at(0u), object, index});
        ++index;
        ctx.gpr[4] = sink;
        ctx.gpr[5] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08C34248u, {sink, frame.at(0u)});
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        if (count == index) break;
        object = word(rt, ref);
    }
}

// func_08B03F10(this, f12): walks a chain of 6-byte nodes: the first node is (this[0xA0] list base)[0] + 6 * (int16 this[0x98]) (none for a negative index, and v0 then keeps the word at the frame's sp that the original reads there);
// each node n: an int16 flags word n[2] that is non-negative with bit 14 clear gets func_08B07F6C(this + 0xC8, this, n, f12 = the float the function was entered with); the next index is the int16 n[4] (negative ends the walk, v0 = that index),
// the next node (this[0xA0] list)[0] + 6 * index (a null node ends the walk). f20 is saved and holds the float in between like the original.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the flags word n[2] is always non-negative (the `bltzl` at 0x08B03F8C, whose taken path only loads the next index, is never taken) and bit 14 is always 0 (the `beqz v1` at 0x08B03FA0 is always taken, to the call of func_08B07F6C; with bit 14 set it would fall through and skip the call).
void func_08B03F10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float saved_f20 = ctx.fpr[20];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.fpr[20] = ctx.fpr[12];
    const float f20 = ctx.fpr[12];
    const std::int32_t first_index = static_cast<std::int16_t>(rt.memory().load16(self + 0x98u));
    const std::uint32_t list = word(rt, self + 0xA0u);
    std::uint32_t node = 0u;
    if (first_index >= 0) node = word(rt, list) + static_cast<std::uint32_t>(first_index) * 6u;
    ctx.gpr[18] = list;
    ctx.gpr[2] = word(rt, frame.at(0u));
    ctx.gpr[16] = node;
    // The original also stores seven words into the dead part of its frame here (sp + 4 .. 0x1C); nothing reads them afterwards.
    while (node != 0u) {
        const std::uint32_t flags = rt.memory().load16(node + 2u);
        ctx.gpr[2] = flags;
        ctx.gpr[3] = (flags >> 14) & 1u;
        if (static_cast<std::int16_t>(flags) >= 0) {
            ctx.fpr[12] = f20;
            ctx.gpr[4] = self + 0xC8u;
            ctx.gpr[5] = self;
            ctx.gpr[6] = node;
            if (((flags >> 14) & 1u) == 0u) (void)call_keep(rt, ctx, 0x08B07F6Cu, {self + 0xC8u, self, node}, {f20});
        }
        const std::uint32_t index = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(node + 4u))));
        const std::uint32_t offset = (index << 3) - (index << 1);
        ctx.gpr[2] = index;
        ctx.gpr[4] = offset;
        ctx.gpr[3] = index << 1;
        if (signed_of(index) < 0) break;
        const std::uint32_t array = word(rt, list);
        ctx.gpr[2] = array;
        node = array + offset;
        ctx.gpr[16] = node;
    }
    ctx.fpr[20] = saved_f20;
}

// func_08869444(this, f12): the update of a menu object (f20 = f12 kept across the calls): func_08AF2640(this + 0x1A0), this[0xC4] = this + 0x1A0, func_0886A280(this + 0x1B0, f12), func_0887AC3C(this + 0x74, 0, f12), func_08B046B8(this + 0x80, f12);
// with c = this[0x1BC]: vtable slot 0x30 of c called with (c, 0), slot 0x20 with (c, f12); under the lock this (func_08B344E0 .. func_08B3457C) slot 0x14 with (c); then slot 0x30 with (c, 0) again; the tail call is func_088B0520(this + 0x28).
// Branches: none (a straight line).
void func_08869444(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const float saved_f20 = ctx.fpr[20];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.fpr[20] = ctx.fpr[12];
        const float f20 = ctx.fpr[12];
        ctx.gpr[17] = self + 0x1A0u;
        ctx.gpr[16] = self;
        ctx.gpr[4] = self + 0x1A0u;
        (void)call_keep(rt, ctx, 0x08AF2640u, {self + 0x1A0u});
        ctx.fpr[12] = f20;
        put(rt, self + 0xC4u, self + 0x1A0u);
        ctx.gpr[4] = self + 0x1B0u;
        (void)call_keep(rt, ctx, 0x0886A280u, {self + 0x1B0u}, {f20});
        ctx.fpr[12] = f20;
        ctx.gpr[5] = 0u;
        ctx.gpr[4] = self + 0x74u;
        (void)call_keep(rt, ctx, 0x0887AC3Cu, {self + 0x74u, 0u}, {f20});
        ctx.fpr[12] = f20;
        ctx.gpr[4] = self + 0x80u;
        (void)call_keep(rt, ctx, 0x08B046B8u, {self + 0x80u}, {f20});
        // c = this[0x1BC]: slot 0x30 with (c, 0).
        std::uint32_t object = word(rt, self + 0x1BCu);
        std::uint32_t vtable = word(rt, object);
        std::uint32_t function = word(rt, vtable + 0x30u);
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0u;
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {object, 0u});
        // slot 0x20 with (c, f12 = f20).
        object = word(rt, self + 0x1BCu);
        vtable = word(rt, object);
        function = word(rt, vtable + 0x20u);
        ctx.gpr[4] = object;
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.fpr[12] = f20;
        (void)call_keep(rt, ctx, function, {object}, {f20});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08B344E0u, {self});
        // slot 0x14 with (c).
        object = word(rt, self + 0x1BCu);
        vtable = word(rt, object);
        function = word(rt, vtable + 0x14u);
        ctx.gpr[4] = object;
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {object});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08B3457Cu, {self});
        // slot 0x30 with (c, 0) again.
        object = word(rt, self + 0x1BCu);
        vtable = word(rt, object);
        function = word(rt, vtable + 0x30u);
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0u;
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {object, 0u});
        ctx.gpr[4] = self + 0x28u;
        ctx.fpr[12] = f20;
    }
    ctx.fpr[20] = saved_f20;
    tail_call(ctx, 0x088B0520u, {self + 0x28u});
}

// func_0886968C(this): s = func_08AFAA04(this); the 0x34-byte value at sp + 0x40 is built by func_08AF4DF4(sp + 0x40, s, 0) and copied (13 words) to sp; func_0890B4FC(this + 0x90); this[0x170] = 0 with func_08869514(this);
// a zero byte this[0x1C4] adds func_08AF12D0(this[0xB0], 0, 1, 1); func_0890B92C(this + 0x90, sp, 0, 0); the object at sp[0] has its vtable slot 0x18 called, a non-null one at sp[4] its slot 0x1C; the last call is func_0886B600().
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0x1C4] (byte) is always 0 (func_08AF12D0 always runs; the `bnez v0` at 0x08869710 would skip it) and sp[4] is always non-null (its vtable slot 0x1C is always called; the `beqz a0` at 0x08869758 would skip it).
void func_0886968C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    const std::uint32_t s = call_keep(rt, ctx, 0x08AFAA04u, {self});
    ctx.gpr[16] = frame.at(0x40u);
    ctx.gpr[6] = 0u;
    ctx.gpr[5] = s;
    ctx.gpr[4] = frame.at(0x40u);
    (void)call_keep(rt, ctx, 0x08AF4DF4u, {frame.at(0x40u), s, 0u});
    std::uint32_t from = frame.at(0x40u), to = frame.at(0u);
    while (from != frame.at(0x70u)) {
        const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
        put(rt, to, w0);
        from += 0x10u;
        to += 0x10u;
        put(rt, to - 0xCu, w1);
        put(rt, to - 8u, w2);
        put(rt, to - 4u, w3);
    }
    const std::uint32_t last = word(rt, from);
    ctx.gpr[2] = last;
    ctx.gpr[16] = self + 0x90u;
    ctx.gpr[4] = self + 0x90u;
    put(rt, to, last);
    (void)call_keep(rt, ctx, 0x0890B4FCu, {self + 0x90u});
    ctx.gpr[4] = self;
    put(rt, self + 0x90u + 0xE0u, 0u);
    (void)call_keep(rt, ctx, 0x08869514u, {self});
    ctx.gpr[2] = rt.memory().load8(self + 0x1C4u);
    ctx.gpr[4] = self + 0x90u;
    if (ctx.gpr[2] == 0u) {
        const std::uint32_t target = word(rt, self + 0xB0u);
        ctx.gpr[4] = target;
        ctx.gpr[5] = 0u;
        ctx.gpr[6] = 1u;
        ctx.gpr[7] = 1u;
        (void)call_keep(rt, ctx, 0x08AF12D0u, {target, 0u, 1u, 1u});
    }
    ctx.gpr[4] = self + 0x90u;
    ctx.gpr[5] = frame.at(0u);
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = 0u;
    (void)call_keep(rt, ctx, 0x0890B92Cu, {self + 0x90u, frame.at(0u), 0u, 0u});
    std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[4] = object;
    std::uint32_t vtable = word(rt, object);
    std::uint32_t function = word(rt, vtable + 0x18u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {object});
    object = word(rt, frame.at(4u));
    ctx.gpr[4] = object;
    if (object != 0u) {
        vtable = word(rt, object);
        function = word(rt, vtable + 0x1Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {object});
    }
    (void)call_keep(rt, ctx, 0x0886B600u, {ctx.gpr[4]});
}

// func_08872E18(this, obj): o = this[0x5C]: o[0x100] &= ~1; f20 = obj's vtable slot 0x1C called with (obj) (a float) * func_0886B0F8(this[8]); func_088728C0(o, this + 0xC); func_08872054(o, f12 = f20 * 60.0);
// when func_0886B444(this[8], 3) answers zero and p = func_0886A9A0(this[4], 5) is non-null: func_0886F8CC(p), func_0886F8E4(p), func_08872AC8(o, this + 0xC, func_0886F8D4(p)) and o[0x100] |= 1; the tail call is func_0890AEBC(this[0x5C] + 8, f12 = f20).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_0886B444(this[8], 3) always answers 0 (the `bnez v0` at 0x08872EA0 would go to the tail call) and p = func_0886A9A0(this[4], 5) is always null (`beqz v0` at 0x08872EB4 -> 0x08872EF8): the block that calls func_0886F8CC, func_0886F8E4, func_0886F8D4, func_08872AC8 and sets o[0x100] |= 1 (0x08872EB8-0x08872EF4) is read from the asm only.
// Not registered: no compared case changed memory.
void func_08872E18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], obj = ctx.gpr[5];
    const float saved_f20 = ctx.fpr[20];
    std::uint32_t target;
    float f20;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        ctx.gpr[19] = self + 0xCu;
        ctx.gpr[20] = self + 0x5Cu;
        const std::uint32_t a2 = word(rt, self + 0x5Cu);
        ctx.gpr[6] = a2;
        std::uint32_t flags = word(rt, a2 + 0x100u) & ~1u;
        ctx.gpr[2] = flags;
        put(rt, a2 + 0x100u, flags);
        const std::uint32_t vtable = word(rt, obj);
        const std::uint32_t function = word(rt, vtable + 0x1Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = obj;
        const GuestResult first = call_keep(rt, ctx, function, {obj}, {});
        f20 = first.f0;
        ctx.fpr[20] = f20;
        ctx.gpr[4] = word(rt, self + 8u);
        const GuestResult second = call_keep(rt, ctx, 0x0886B0F8u, {word(rt, self + 8u)}, {});
        f20 = psp_mul(f20, second.f0);
        ctx.fpr[20] = f20;
        ctx.gpr[4] = word(rt, self + 0x5Cu);
        ctx.gpr[5] = self + 0xCu;
        (void)call_keep(rt, ctx, 0x088728C0u, {word(rt, self + 0x5Cu), self + 0xCu});
        const float scaled = psp_mul(f20, fbits(0x42700000u));
        ctx.fpr[12] = scaled;
        ctx.gpr[4] = word(rt, self + 0x5Cu);
        (void)call_keep(rt, ctx, 0x08872054u, {word(rt, self + 0x5Cu)}, {scaled});
        ctx.gpr[4] = word(rt, self + 8u);
        ctx.gpr[5] = 3u;
        const std::uint32_t answer = call_keep(rt, ctx, 0x0886B444u, {word(rt, self + 8u), 3u});
        ctx.gpr[5] = 5u;
        if (answer == 0u) {
            ctx.gpr[4] = word(rt, self + 4u);
            const std::uint32_t p = call_keep(rt, ctx, 0x0886A9A0u, {word(rt, self + 4u), 5u});
            ctx.gpr[18] = p;
            ctx.gpr[4] = p;
            if (p != 0u) {
                (void)call_keep(rt, ctx, 0x0886F8CCu, {p});
                ctx.gpr[4] = p;
                (void)call_keep(rt, ctx, 0x0886F8E4u, {p});
                const std::uint32_t o = word(rt, self + 0x5Cu);
                ctx.gpr[16] = o;
                ctx.gpr[4] = p;
                const std::uint32_t count = call_keep(rt, ctx, 0x0886F8D4u, {p});
                ctx.gpr[6] = count;
                ctx.gpr[4] = o;
                ctx.gpr[5] = self + 0xCu;
                (void)call_keep(rt, ctx, 0x08872AC8u, {o, self + 0xCu, count});
                const std::uint32_t v1 = word(rt, self + 0x5Cu);
                ctx.gpr[3] = v1;
                flags = word(rt, v1 + 0x100u) | 1u;
                ctx.gpr[2] = flags;
                put(rt, v1 + 0x100u, flags);
            }
        }
        target = word(rt, self + 0x5Cu) + 8u;
        ctx.gpr[4] = target;
        ctx.fpr[12] = f20;
    }
    ctx.fpr[20] = saved_f20;
    tail_call(ctx, 0x0890AEBCu, {target});
}

// func_08C23080(this, key): func_08A5D438(sp + 4), func_08A76AB4(sp + 8, D_08CF6ED8), then func_08A7FD4C(func_08A61624(key), sp + 4, this + 8); o = word at sp + 4 (null: func_08A5BB7C(D_08CB028C) runs first):
// func_08A76AA0(sp) and o's vtable slot 0x1C called with (o, sp + 8, sp); func_08A5B9EC(sp), func_08A62C54(key, [sp + 8]), func_08A5B9EC(sp + 8), func_08A5B9EC(sp + 4). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object at sp + 4 is never null (the func_08A5BB7C assertion call is skipped by the `bnez s1` at 0x08C230E4).
void func_08C23080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = frame.at(4u);
    ctx.gpr[16] = self;
    ctx.gpr[4] = frame.at(4u);
    ctx.gpr[20] = key;
    ctx.gpr[18] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5D438u, {frame.at(4u)});
    ctx.gpr[5] = 0x08CF6ED8u;
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A76AB4u, {frame.at(8u), 0x08CF6ED8u});
    ctx.gpr[4] = key;
    const std::uint32_t converted = call_keep(rt, ctx, 0x08A61624u, {key});
    ctx.gpr[4] = converted;
    ctx.gpr[5] = frame.at(4u);
    ctx.gpr[6] = self + 8u;
    (void)call_keep(rt, ctx, 0x08A7FD4Cu, {converted, frame.at(4u), self + 8u});
    const std::uint32_t object = word(rt, frame.at(4u));
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CB028Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB028Cu});
    const std::uint32_t vtable = word(rt, object);
    ctx.gpr[2] = vtable;
    ctx.gpr[4] = frame.at(0u);
    const std::uint32_t function = word(rt, vtable + 0x1Cu);
    ctx.gpr[16] = function;
    (void)call_keep(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    ctx.gpr[6] = frame.at(0u);
    ctx.gpr[5] = frame.at(8u);
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, function, {object, frame.at(8u), frame.at(0u)});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[5] = word(rt, frame.at(8u));
    ctx.gpr[4] = key;
    (void)call_keep(rt, ctx, 0x08A62C54u, {key, ctx.gpr[5]});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}

// func_089E18BC / func_089E19BC (out, a1, mode, ref): a flag of bit `bit` in the word o[0x1B0] of the object o that func_089E1104 answers through its holder (null: func_08A5BB7C(D_08CA77A4) first). With mode == 0 `out` receives the flag through
// func_08A97A6C(sp + 8, (o[0x1B0] >> bit) & 1) (copied with func_08A5BAD8 unless it is sp + 8) and both holders are destroyed (func_08A5B9EC). With mode < 0 nothing happens (v0 stays); with mode > 0 the flag is set to the low bit of the answer
// of the vtable slot 0x30 of *ref (a null *ref runs func_08A5BB7C(D_08CA7770) first): o[0x1B0] = o[0x1B0] with bit `bit` replaced, and the holder is destroyed. v0 is the last call's.
template <unsigned bit>
inline void flag_accessor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = ref;
    if (mode == 0u) {
        ctx.gpr[18] = frame.at(4u);
        ctx.gpr[4] = frame.at(4u);
        (void)call_keep(rt, ctx, 0x089E1104u, {frame.at(4u)});
        const std::uint32_t object = word(rt, frame.at(4u));
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08CA77A4u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA77A4u});
        const std::uint32_t flags = word(rt, object + 0x1B0u);
        ctx.gpr[5] = flags;
        ctx.gpr[16] = frame.at(8u);
        ctx.gpr[4] = frame.at(8u);
        const std::uint32_t flag = (flags >> bit) & 1u;
        ctx.gpr[5] = flag;
        (void)call_keep(rt, ctx, 0x08A97A6Cu, {frame.at(8u), flag});
        ctx.gpr[4] = out;
        if (out != frame.at(8u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(8u))});
        ctx.gpr[4] = frame.at(8u);
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        ctx.gpr[4] = frame.at(4u);
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        return;
    }
    if (signed_of(mode) <= 0) return;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x089E1104u, {frame.at(0u)});
    const std::uint32_t target = word(rt, frame.at(0u));
    ctx.gpr[17] = target;
    ctx.gpr[4] = 0x08CA77A4u;
    if (target == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA77A4u});
    const std::uint32_t source = word(rt, ref);
    ctx.gpr[16] = source;
    ctx.gpr[4] = 0x08CA7770u;
    if (source == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA7770u});
    const std::uint32_t vtable = word(rt, source);
    const std::uint32_t function = word(rt, vtable + 0x30u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = source;
    const std::uint32_t answer = call_keep(rt, ctx, function, {source}) & 0xFFu;
    const std::uint32_t flags = word(rt, target + 0x1B0u);
    ctx.gpr[2] = answer;
    ctx.gpr[3] = (flags & ~(1u << bit)) | ((answer & 1u) << bit);
    ctx.gpr[4] = frame.at(0u);
    put(rt, target + 0x1B0u, ctx.gpr[3]);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_089E18BC(out, a1, mode, ref): flag_accessor with bit 3.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): mode is always > 0, so only the setter ran. Read from the asm only: mode == 0 (the getter: the flag bit through func_08A97A6C(sp + 8, bit), the copy func_08A5BAD8 unless out is sp + 8, two func_08A5B9EC; 0x089E18DC-0x089E1938), mode < 0 (returns at once with v0 untouched, `blez a2` at 0x089E193C) and the two null checks of the setter (func_08A5BB7C assertion calls).
void func_089E18BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_accessor<3u>(rt, ctx); }

// func_089E19BC(out, a1, mode, ref): flag_accessor with bit 2.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): as func_089E18BC (bit 2 instead of bit 3): only the setter (mode > 0) ran; the getter (mode == 0), mode < 0 and the null-check calls are read from the asm only.
void func_089E19BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_accessor<2u>(rt, ctx); }

// func_0886B600(): under the lock D_08CE2E30 (func_08B344E0 .. func_08B3457C): the ring of four 12-byte queues at D_08CE2E58 (+0x30 the index) takes the next slot s = D_08CE2E58 + 12 * ((D_08CE2E58[0x30] + 1) % 4) (C remainder);
// while s[4] is non-zero node n: func_08B1FC48(s, n) and the object o = *n (loaded before the call), when non-null, has its vtable slot 4 called with (o); then D_08CE2E58[0x30] = (D_08CE2E58[0x30] + 1) % 4 and the tail call is func_08B3457C(lock).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the slot s is always empty (s[4] == 0): the draining loop (func_08B1FC48 and the vtable call at 0x0886B65C-0x0886B678) never runs and is read from the asm only.
void func_0886B600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t lock = 0x08CE2E30u, table = 0x08CE2E58u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = 0x08CE0000u;
        ctx.gpr[18] = 0x08CE0000u;
        ctx.gpr[4] = lock;
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        ctx.gpr[5] = table;
        const std::int32_t next = (signed_of(word(rt, table + 0x30u)) + 1) % 4;
        ctx.gpr[2] = static_cast<std::uint32_t>(next);
        std::uint32_t slot = table + static_cast<std::uint32_t>(next) * 12u;
        ctx.gpr[17] = slot;
        for (;;) {
            const std::uint32_t node = word(rt, slot + 4u);
            ctx.gpr[2] = node;
            ctx.gpr[4] = slot;
            if (node == 0u) break;
            ctx.gpr[5] = node;
            const std::uint32_t object = word(rt, node);
            ctx.gpr[16] = object;
            (void)call_keep(rt, ctx, 0x08B1FC48u, {slot, node});
            if (object != 0u) {
                const std::uint32_t vtable = word(rt, object);
                const std::uint32_t function = word(rt, vtable + 4u);
                ctx.gpr[4] = object;
                ctx.gpr[3] = function;
                ctx.gpr[2] = vtable;
                (void)call_keep(rt, ctx, function, {object});
            }
        }
        ctx.gpr[2] = table;
        const std::int32_t wrapped = (signed_of(word(rt, table + 0x30u)) + 1) % 4;
        ctx.gpr[3] = static_cast<std::uint32_t>(wrapped);
        ctx.gpr[4] = lock;
        put(rt, table + 0x30u, static_cast<std::uint32_t>(wrapped));
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// func_0880BEF8(this): a destructor: func_08A5B9EC(this + 0x208), func_088697E8(this + 0x30), func_0880BEE8(this + 0x28); the tail call is func_08B28DC4(this).
// Not registered (equal in its 4 cases, sabotage detected): its one call per menu route destroys the whole member tree, so its cases swallow the compared calls of about 56 already decompiled descendants (their calls run inside the case, not through the dispatcher) and of this batch's destructors; the full gate's compared total fell by 55. Left recompiled so they stay compared.
void func_0880BEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[16] = self;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {self + 0x208u});
        (void)call_keep(rt, ctx, 0x088697E8u, {self + 0x30u});
        (void)call_keep(rt, ctx, 0x0880BEE8u, {self + 0x28u});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B28DC4u, {self});
}

// func_0884CC18(out, a1, a2, a3, t0): p = func_08A5BC20(0x28, 4); func_0884C590(p, 4, a2, a3, t0) constructs it, the frame word [sp] = p, and the tail-less last call is func_08A76AB4(out, &[sp]). v0 is that call's.
// a1 reaches func_0884C590 as 4 (the value the original left in it before the allocator call).
// Branches: none (a straight line).
void func_0884CC18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = out;
    ctx.gpr[16] = a2;
    ctx.gpr[17] = a3;
    ctx.gpr[19] = t0;
    ctx.gpr[5] = 4u;
    const std::uint32_t object = call_keep(rt, ctx, 0x08A5BC20u, {0x28u, 4u});
    ctx.gpr[18] = object;
    ctx.gpr[5] = 4u;
    (void)call_keep(rt, ctx, 0x0884C590u, {object, 4u, a2, a3, t0});
    put(rt, frame.at(0u), object);
    (void)call_keep(rt, ctx, 0x08A76AB4u, {out, frame.at(0u)});
}

// func_0884CD28(out, a1, a2, a3, t0, t1): as func_0884CC18 with one more argument: func_0884C664(p, 4, a2, a3, t0, t1) constructs the 0x28-byte object, then func_08A76AB4(out, &[sp]).
// Branches: none (a straight line).
void func_0884CD28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], t1 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = out;
    ctx.gpr[20] = t1;
    ctx.gpr[19] = t0;
    ctx.gpr[17] = a3;
    ctx.gpr[16] = a2;
    const std::uint32_t object = call_keep(rt, ctx, 0x08A5BC20u, {0x28u, 4u});
    ctx.gpr[18] = object;
    ctx.gpr[5] = 4u;
    (void)call_keep(rt, ctx, 0x0884C664u, {object, 4u, a2, a3, t0, t1});
    put(rt, frame.at(0u), object);
    (void)call_keep(rt, ctx, 0x08A76AB4u, {out, frame.at(0u)});
}

// func_0886A804(this): func_0886BFC0(this + 0x4238), this[0x4240] (word) = D_08CBF6E8, func_0886D88C(this + 0x4238), func_08B28DC4(this + 0x4210); the tail call is func_08875138(this + 8).
// Branches: none (a straight line).
void func_0886A804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self + 0x4238u;
        ctx.gpr[16] = self;
        (void)call_keep(rt, ctx, 0x0886BFC0u, {self + 0x4238u});
        ctx.gpr[2] = 0x08CBF6E8u;
        put(rt, self + 0x4240u, 0x08CBF6E8u);
        (void)call_keep(rt, ctx, 0x0886D88Cu, {self + 0x4238u});
        (void)call_keep(rt, ctx, 0x08B28DC4u, {self + 0x4210u});
        ctx.gpr[4] = self + 8u;
    }
    tail_call(ctx, 0x08875138u, {self + 8u});
}

// func_088A5E5C(this, target): func_088A5D50(this[0]), func_0892BF7C(target, 9), then for i = 0..8: func_0892C4B8(target, (this[0])[0x4C0], i, i + 0x64, -1); the tail call is func_088A5748(this[0]).
// Branches: none that the compared cases skip (a loop of nine, a straight line otherwise).
void func_088A5E5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], target = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = 9u;
        ctx.gpr[18] = target;
        ctx.gpr[17] = self;
        ctx.gpr[16] = 0u;
        ctx.gpr[4] = word(rt, self);
        (void)call_keep(rt, ctx, 0x088A5D50u, {word(rt, self)});
        ctx.gpr[4] = target;
        (void)call_keep(rt, ctx, 0x0892BF7Cu, {target, 9u});
        for (std::uint32_t i = 0; i < 9u; ++i) {
            const std::uint32_t owner = word(rt, self);
            ctx.gpr[2] = owner;
            const std::uint32_t value = word(rt, owner + 0x4C0u);
            ctx.gpr[16] = i + 1u;
            ctx.gpr[4] = target;
            (void)call_keep(rt, ctx, 0x0892C4B8u, {target, value, i, i + 0x64u, 0xFFFFFFFFu});
        }
        ctx.gpr[4] = word(rt, self);
    }
    tail_call(ctx, 0x088A5748u, {word(rt, self)});
}

// func_088D9BBC(this, clear, stop): this[0x5B4] (byte) = 0; with `clear` (low byte) non-zero this[0x640] (word) = 0 (the old value s0 is kept); func_08B3457C(this + 0xC8); when the old value is non-zero and `stop`
// (low byte) is zero the tail call is func_088E4160(this + 0xC8), else the function returns. v0 is func_08B3457C's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): `clear` is always 0 (the `beqz a1` at 0x088D9BE0 skips the store this[0x640] = 0; the sb of this[0x5B4] in its delay slot always runs) and the old word this[0x640] is always 0 (the `beqz s0` at 0x088D9BF4 returns after the unlock: the `bnez s1` at 0x088D9BFC and the tail call func_088E4160 at 0x088D9C0C never run).
void func_088D9BBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], clear = ctx.gpr[5] & 0xFFu, stop = ctx.gpr[6] & 0xFFu;
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = self;
        ctx.gpr[5] = clear;
        ctx.gpr[17] = stop;
        const std::uint32_t old = word(rt, self + 0x640u);
        ctx.gpr[16] = old;
        ctx.gpr[4] = self + 0xC8u;
        rt.memory().store8(self + 0x5B4u, 0u);
        if (clear != 0u) put(rt, self + 0x640u, 0u);
        (void)call_keep(rt, ctx, 0x08B3457Cu, {self + 0xC8u});
        tail = old != 0u && stop == 0u;
    }
    if (tail) tail_call(ctx, 0x088E4160u, {self + 0xC8u});
}

// func_08944100(a0, a1, a2, a3): r = func_08944A70(a0..a3); func_089466A0(r, a3, a3 + 1 != 0); func_08957478(a1, 0x7F, a3, D_08CA2298); the tail call is func_089426C4(a0, a1, 0, 0, 0x7F, r, a2).
// Branches: none that the compared cases skip (a straight line; a3 + 1 != 0 is a plain value).
void func_08944100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    std::uint32_t r;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[16] = a3;
        ctx.gpr[20] = a2;
        ctx.gpr[19] = a0;
        ctx.gpr[18] = a1;
        r = call_keep(rt, ctx, 0x08944A70u, {a0, a1, a2, a3});
        ctx.gpr[17] = r;
        const std::uint32_t flag = (a3 + 1u) != 0u ? 1u : 0u;
        ctx.gpr[6] = flag;
        (void)call_keep(rt, ctx, 0x089466A0u, {r, a3, flag});
        ctx.gpr[7] = 0x08CA2298u;
        ctx.gpr[6] = a3;
        ctx.gpr[5] = 0x7Fu;
        ctx.gpr[4] = a1;
        (void)call_keep(rt, ctx, 0x08957478u, {a1, 0x7Fu, a3, 0x08CA2298u});
        ctx.gpr[9] = r;
        ctx.gpr[10] = a2;
        ctx.gpr[8] = 0x7Fu;
        ctx.gpr[6] = 0u;
        ctx.gpr[7] = 0u;
    }
    tail_call(ctx, 0x089426C4u, {a0, a1, 0u, 0u, 0x7Fu, r, a2});
}

// func_08A96B50(this, out): c = (int8) this[0x14]; this[0x14] = c + 1; func_08A85B1C(holder, c) fills the frame holder; unless `out` is the holder func_08A5BAD8(out, holder[0]) copies it; func_08A5B9EC(holder) destroys it. v0 is that call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): `out` is never the holder at sp: the copy func_08A5BAD8 always runs (the `beq s0, sp` at 0x08A96B7C would skip it).
void func_08A96B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = out;
    const std::uint32_t c = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, self + 0x14u)));
    ctx.gpr[6] = c;
    rt.memory().store8(self + 0x14u, static_cast<std::uint8_t>(c + 1u));
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {frame.at(0u), c});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08A042F4(out, a1): p = func_08A5BC20(0x40, 4); func_08A03BB8(p, a1) constructs it, the frame word [sp] = p and func_08A76AB4(out, &[sp]). v0 is that call's.
// Branches: none (a straight line).
void func_08A042F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    ctx.gpr[17] = a1;
    const std::uint32_t object = call_keep(rt, ctx, 0x08A5BC20u, {0x40u, 4u});
    ctx.gpr[16] = object;
    ctx.gpr[5] = 4u;
    (void)call_keep(rt, ctx, 0x08A03BB8u, {object, a1});
    put(rt, frame.at(0u), object);
    (void)call_keep(rt, ctx, 0x08A76AB4u, {out, frame.at(0u)});
}

// func_0881B758(this, a1, key, c, base): n = func_0881A8C0(this, a1, key, c); dup = func_0881A8F0(this, a1, key, c); when n < 10 (signed) entry = func_0881A880(this, 0, key, c) is bumped:
// the halfword at entry = key, the byte (entry + base)[2] += 1, this[4] += 1 only when dup is zero, this[8] += 1; v0 = 1. Else v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): n = func_0881A8C0 is always below 10 (the `beqz s0` at 0x0881B7B4 -> 0x0881B7F8 answering 0 is never taken) and func_0881A8F0 always answers 0 (this[4] += 1 always runs; with a non-zero answer the `bnez s4` at 0x0881B7D8 skips it, its delay slot's sb still runs).
void func_0881B758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], key = ctx.gpr[6], c = ctx.gpr[7], base = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = base;
    ctx.gpr[19] = c;
    ctx.gpr[18] = key;
    ctx.gpr[17] = self;
    const std::uint32_t n = call_keep(rt, ctx, 0x0881A8C0u, {self, a1, key, c});
    ctx.gpr[16] = n;
    const std::uint32_t dup = call_keep(rt, ctx, 0x0881A8F0u, {self, a1, key, c});
    ctx.gpr[16] = signed_of(n) < 10 ? 1u : 0u;
    ctx.gpr[20] = dup;
    ctx.gpr[5] = 0u;
    if (signed_of(n) >= 10) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = call_keep(rt, ctx, 0x0881A880u, {self, 0u, key, c});
    const std::uint32_t target = entry + base;
    std::uint32_t count = rt.memory().load8(target + 2u);
    rt.memory().store16(entry, static_cast<std::uint16_t>(key));
    count += 1u;
    if (dup == 0u) put(rt, self + 4u, word(rt, self + 4u) + 1u);
    rt.memory().store8(target + 2u, static_cast<std::uint8_t>(count));
    put(rt, self + 8u, word(rt, self + 8u) + 1u);
    ctx.gpr[4] = target;
    ctx.gpr[3] = count;
    ctx.gpr[2] = 1u;
}

// func_0886AD04(this, name, index): this[0] = D_08CBF710; func_08B93A24(this + 4, this); this[0x1C] = index; this[0x20] (byte) = 0 around func_0887C7A0(this + 0x64, this); the words this[0x10..0x18] and this[0x24..0x40] = 0;
// then the name is copied: when `name` is non-null and its first byte is non-zero func_08B7C830(this + 0x44, name, 0x1F) and this[0x63] (byte) = 0; else func_08B37640(this + 0x44, D_08C9F65C, W) with W = word D_08CE2E28, which is then incremented.
// v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): `name` is always non-null with a non-zero first byte: the copy func_08B7C830 + this[0x63] = 0 always runs; the other path (a null or empty name: func_08B37640(this + 0x44, D_08C9F65C, W) with the counter word D_08CE2E28 incremented after it, 0x0886ADB4-0x0886ADD0) is read from the asm only.
void func_0886AD04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], name = ctx.gpr[5], index = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = 0x08CBF710u;
    ctx.gpr[17] = self;
    ctx.gpr[18] = name;
    ctx.gpr[16] = index;
    put(rt, self, 0x08CBF710u);
    ctx.gpr[5] = self;
    (void)call_keep(rt, ctx, 0x08B93A24u, {self + 4u, self});
    put(rt, self + 0x1Cu, index);
    ctx.gpr[5] = self;
    rt.memory().store8(self + 0x20u, 0u);
    (void)call_keep(rt, ctx, 0x0887C7A0u, {self + 0x64u, self});
    for (std::uint32_t i = 0; i < 3u; ++i) put(rt, self + 0x10u + (i << 2), 0u);
    for (std::uint32_t i = 0; i < 8u; ++i) put(rt, self + 0x24u + (i << 2), 0u);
    ctx.gpr[16] = 0x08CE0000u;
    const std::uint32_t counter = 0x08CE2E28u;
    if (name != 0u && byte_at(rt, name) != 0) {
        ctx.gpr[6] = word(rt, counter);
        ctx.gpr[5] = name;
        ctx.gpr[4] = self + 0x44u;
        ctx.gpr[6] = 0x1Fu;
        (void)call_keep(rt, ctx, 0x08B7C830u, {self + 0x44u, name, 0x1Fu});
        rt.memory().store8(self + 0x63u, 0u);
        return;
    }
    const std::uint32_t value = word(rt, counter);
    ctx.gpr[6] = value;
    ctx.gpr[5] = 0x08C9F65Cu;
    ctx.gpr[4] = self + 0x44u;
    (void)call_keep(rt, ctx, 0x08B37640u, {self + 0x44u, 0x08C9F65Cu, value});
    ctx.gpr[3] = word(rt, counter);
    put(rt, counter, ctx.gpr[3] + 1u);
    ctx.gpr[3] += 1u;
}

// func_0887C7A0(this, a1): this[0] = D_08CBFE88; p = func_08B62344(0x368), p[0] = a1; for 24 entries (p + 4 + 12 i) func_08AC26A4(entry + 8, 0), then for 48 entries (p + 0x124 + 12 i) the same; p[0x364] and p[0x366] (halfwords) = 0; this[4] = p;
// the guarded static D_08D753F8 (guard byte, func_08B6441C / func_08B64494) holds D_08D75400 (its first word set to D_08CBFED8 on the first call); this[8] = D_08D75400. v0 is the last call's, or 0x08D70000 (the hi half left by the guard-set path) when the guard was set.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C (the guard acquire) always answers non-zero when the guard byte D_08D753F8 is 0; the answer-0 path (0x0887C848-0x0887C850: v0 = hi half 0x08D70000, the holder is not initialised) is read from the asm only.
void func_0887C7A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[3] = 0x08CBFE88u;
    ctx.gpr[19] = self + 4u;
    ctx.gpr[17] = 0x18u;
    ctx.gpr[16] = a1;
    put(rt, self, 0x08CBFE88u);
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x368u});
    put(rt, block, a1);
    ctx.gpr[18] = block;
    std::uint32_t entry = block + 4u;
    for (std::uint32_t n = 0x18u; n != 0u;) {
        ctx.gpr[16] = entry;
        ctx.gpr[4] = entry + 8u;
        ctx.gpr[5] = 0u;
        --n;
        ctx.gpr[17] = n;
        (void)call_keep(rt, ctx, 0x08AC26A4u, {entry + 8u, 0u});
        entry += 0xCu;
    }
    entry = block + 0x124u;
    ctx.gpr[17] = 0x30u;
    for (std::uint32_t n = 0x30u; n != 0u;) {
        ctx.gpr[16] = entry;
        ctx.gpr[4] = entry + 8u;
        ctx.gpr[5] = 0u;
        --n;
        ctx.gpr[17] = n;
        (void)call_keep(rt, ctx, 0x08AC26A4u, {entry + 8u, 0u});
        entry += 0xCu;
    }
    rt.memory().store16(block + 0x364u, 0u);
    rt.memory().store16(block + 0x366u, 0u);
    put(rt, self + 4u, block);
    const std::uint32_t guard = 0x08D753F8u, holder = 0x08D75400u;
    ctx.gpr[2] = static_cast<std::uint32_t>(byte_at(rt, guard));
    ctx.gpr[16] = guard;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[4] = guard;
        const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
        ctx.gpr[3] = 0x08D70000u;
        if (acquired != 0u) {
            ctx.gpr[17] = holder;
            put(rt, holder, 0x08CBFED8u);
            ctx.gpr[4] = guard;
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
        } else {
            ctx.gpr[2] = 0x08D70000u;
        }
    } else {
        ctx.gpr[2] = 0x08D70000u;
    }
    ctx.gpr[17] = holder;
    put(rt, self + 8u, holder);
}

// func_089CD2C8(this, a1, rec): r = func_0896EFAC(this, a1, rec); when r is non-zero it is the answer. Else with k = rec[0x28]: func_089CD228(this, k) or, when that answers zero, func_089CC960(this, k): if either
// answered non-zero this[0x1A8] = this[0x198] and the answer is 1, else 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_0896EFAC always answers 0 (the early answer `bnez v0` at 0x089CD2EC is never taken), func_089CD228 always answers 0 (so func_089CC960 always runs) and func_089CC960 always answers non-zero (the final answer is always 1 with this[0x1A8] = this[0x198]); an answer of 0 from both ends in 0 (0x089CD314).
void func_089CD2C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], rec = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    ctx.gpr[16] = rec;
    std::uint32_t answer = call_keep(rt, ctx, 0x0896EFACu, {self, a1, rec});
    ctx.gpr[17] = answer;
    ctx.gpr[4] = self;
    if (answer == 0u) {
        const std::uint32_t k = word(rt, rec + 0x28u);
        ctx.gpr[16] = k;
        ctx.gpr[5] = k;
        std::uint32_t found = call_keep(rt, ctx, 0x089CD228u, {self, k});
        ctx.gpr[5] = k;
        ctx.gpr[4] = self;
        if (found == 0u) found = call_keep(rt, ctx, 0x089CC960u, {self, k});
        if (found != 0u) {
            ctx.gpr[2] = word(rt, self + 0x198u);
            answer = 1u;
            ctx.gpr[17] = 1u;
            put(rt, self + 0x1A8u, ctx.gpr[2]);
        }
    }
    ctx.gpr[2] = answer;
}

// func_08B712B0(a, b, c, d): the four words are stored at the frame (sp + 0 .. 0xC); func_08B72314(sp, sp + 0x10), func_08B72314(sp + 8, sp + 0x28); the word sp[0x2C] is flipped (^ 1) and func_08B70F70(sp + 0x10, sp + 0x28, sp + 0x40);
// the answer goes through func_08B72030. v0 is that call's.
// Branches: none (a straight line).
void func_08B712B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = frame.at(0x10u);
    ctx.gpr[16] = frame.at(0x28u);
    put(rt, frame.at(0u), a0);
    put(rt, frame.at(4u), a1);
    put(rt, frame.at(0xCu), a3);
    put(rt, frame.at(8u), a2);
    (void)call_keep(rt, ctx, 0x08B72314u, {frame.at(0u), frame.at(0x10u)});
    (void)call_keep(rt, ctx, 0x08B72314u, {frame.at(8u), frame.at(0x28u)});
    const std::uint32_t flag = word(rt, frame.at(0x2Cu)) ^ 1u;
    put(rt, frame.at(0x2Cu), flag);
    const std::uint32_t v = call_keep(rt, ctx, 0x08B70F70u, {frame.at(0x10u), frame.at(0x28u), frame.at(0x40u)});
    (void)call_keep(rt, ctx, 0x08B72030u, {v});
}

// func_08C49930(this, dst, mid): s0 = (mid - dst) with bit 0 cleared; n = this[4] - mid; func_08B79BAC(dst, mid, n); func_08C46F94(n + dst, this[4], sp); this[4] -= s0; v0 = dst.
// Branches: none (a straight line).
// Not registered: compared 0 as a top-level call, and its only nested case (func_08B043D8) is unregistered; it changed no memory.
void func_08C49930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], dst = ctx.gpr[5], mid = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = self;
    ctx.gpr[18] = dst;
    const std::uint32_t span = (mid - dst) & ~1u;
    ctx.gpr[16] = span;
    const std::uint32_t n = word(rt, self + 4u) - mid;
    ctx.gpr[17] = n;
    ctx.gpr[6] = n;
    (void)call_keep(rt, ctx, 0x08B79BACu, {dst, mid, n});
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[4] = n + dst;
    (void)call_keep(rt, ctx, 0x08C46F94u, {n + dst, end, frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - span);
    ctx.gpr[2] = dst;
}

// func_088F5670(out): p = func_08B62344(0x828) is set up: p[0] = p[0x408] = D_08CC3BA8, p[0x404] = p[0x80C] = 0, p[0x810] = D_08CC3DF0, p[0x81C] = 0x12345678, p[0x814] = p + 0x408, p[0x818] = p, p[0x820] = 0, p[0x824] (byte) = 1, out[0] = p;
// the guarded static D_08D76128 (guard byte, func_08B6441C / func_08B64494) holds D_08D76130 (set to D_08CC3E20 on the first call); out[1] = D_08D76130; the tail call is func_088F574C(D_08D76128).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte D_08D76128 is always 0 (the `bnez v0` at 0x088F56F4 skipping the initialisation is never taken) and func_08B6441C always answers non-zero (the `beqz v0` at 0x088F570C is never taken).
void func_088F5670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t guard = 0x08D76128u, holder = 0x08D76130u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = out;
        const std::uint32_t p = call_keep(rt, ctx, 0x08B62344u, {0x828u});
        ctx.gpr[18] = holder;
        put(rt, p + 0x404u, 0u);
        put(rt, p + 0x80Cu, 0u);
        put(rt, p + 0x810u, 0x08CC3DF0u);
        put(rt, p + 0x408u, 0x08CC3BA8u);
        ctx.gpr[16] = guard;
        put(rt, p, 0x08CC3BA8u);
        put(rt, p + 0x81Cu, 0x12345678u);
        put(rt, p + 0x814u, p + 0x408u);
        put(rt, p + 0x820u, 0u);
        put(rt, p + 0x818u, p);
        rt.memory().store8(p + 0x824u, 1u);
        put(rt, out, p);
        ctx.gpr[2] = static_cast<std::uint32_t>(byte_at(rt, guard));
        ctx.gpr[4] = guard;
        if (ctx.gpr[2] == 0u) {
            const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
            ctx.gpr[4] = guard;
            if (acquired != 0u) {
                put(rt, holder, 0x08CC3E20u);
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            }
        }
        put(rt, out + 4u, holder);
        ctx.gpr[4] = guard;
    }
    tail_call(ctx, 0x088F574Cu, {guard});
}

// func_08ABEA40(this, key): list = this + 0x144; node = func_08ABF868(list, key); no node: the tail call is func_08ABF770(list, 0, key). When the halfword key[6] is below node[6] (unsigned) the tail call is func_08ABF7D4(list, key);
// else the chain is walked with func_08ABF6F0(list, node): a next node with key[6] below its halfword [6], or none, ends in the tail call func_08ABF770(list, node, key).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): key[6] is never below node[6] on the first test (the tail call func_08ABF7D4(list, key) at 0x08ABEA94 is read from the asm only).
void func_08ABEA40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    const std::uint32_t list = self + 0x144u;
    std::uint32_t node;
    std::uint32_t target = 0u;
    std::uint32_t second = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = list;
        ctx.gpr[4] = list;
        ctx.gpr[18] = a1;
        node = call_keep(rt, ctx, 0x08ABF868u, {list, a1});
        ctx.gpr[16] = node;
        if (node == 0u) {
            target = 0x08ABF770u;
        } else if (rt.memory().load16(a1 + 6u) < rt.memory().load16(node + 6u)) {
            target = 0x08ABF7D4u;
        } else {
            for (;;) {
                ctx.gpr[4] = list;
                ctx.gpr[5] = node;
                const std::uint32_t next = call_keep(rt, ctx, 0x08ABF6F0u, {list, node});
                if (next == 0u) {
                    target = 0x08ABF770u;
                    break;
                }
                ctx.gpr[4] = next;
                if (rt.memory().load16(a1 + 6u) < rt.memory().load16(next + 6u)) {
                    ctx.gpr[4] = list;
                    target = 0x08ABF770u;
                    break;
                }
                node = next;
                ctx.gpr[16] = node;
            }
        }
        ctx.gpr[4] = list;
        second = node;
    }
    if (target == 0x08ABF7D4u) tail_call(ctx, target, {list, a1});
    else tail_call(ctx, target, {list, second, a1});
}

// func_08AE7E28(this, q): a lookup in the chain that starts at this[0x1C]: shift = 0x18 (0 when q[0] == 6), k = q[4], dir = q[8]; the first node n (a node with a negative word [8] ends the chain) whose word [8] >> shift equals k and whose signed byte [0x36] > 0
// has its halfword slots n[0x30 + 2 i] (i < byte [0x36]): a slot >= 0 gets func_08AEF874(slot, 2, 0, 0) and, when dir is non-zero, also func_08AEF1A8(slot), after which the slot is -1. The walk goes on with n[4]; v0 is the last sign flag.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): every compared case has q[0] == 6 (shift 0; the shift of 0x18 is read from the asm only), a head node with a non-negative word [8] that matches (the no-match step to n[4] is read from the asm only), a positive byte [0x36] (the `blez` at 0x08AE7ED0 is never taken), exactly one slot (the slot loop never repeats; the repeat at 0x08AE7F18 is read from the asm only), a non-negative slot (the `bltz` at 0x08AE7EF0 skipping the call is not taken), q[8] != 0 (func_08AEF1A8 and the store of -1 always run; with q[8] == 0 the slot is left, 0x08AE7F00), and a chain that ends after the first node (the next node has a negative word [8]).
void func_08AE7E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], q = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    std::uint32_t shift = 0x18u;
    const std::uint32_t head = word(rt, self + 0x1Cu);
    const std::uint32_t kind = word(rt, q);
    const std::uint32_t dir = word(rt, q + 8u);
    ctx.gpr[19] = dir;
    std::uint32_t flag = (word(rt, head + 8u) >> 31) & 1u;
    if ((kind ^ 6u) == 0u) shift = 0u;
    ctx.gpr[21] = shift;
    std::uint32_t node = flag == 0u ? head : 0u;
    ctx.gpr[17] = node;
    const std::uint32_t k = word(rt, q + 4u);
    ctx.gpr[20] = k;
    while (node != 0u) {
        const std::uint32_t field = word(rt, node + 8u);
        std::uint32_t count = 0u;
        bool matched = (static_cast<std::uint32_t>(signed_of(field) >> (shift & 31u))) == k;
        if (matched) {
            count = rt.memory().load8(node + 0x36u);
            ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(count)));
            if (static_cast<std::int8_t>(count) <= 0) matched = false;
        }
        if (matched) {
            std::uint32_t slot = node + 0x30u;
            ctx.gpr[22] = 0xFFFFFFFFu;
            for (std::uint32_t i = 0;;) {
                ctx.gpr[16] = slot;
                ctx.gpr[18] = i;
                const std::int32_t index = static_cast<std::int16_t>(rt.memory().load16(slot + 8u));
                ctx.gpr[2] = static_cast<std::uint32_t>(index);
                ctx.gpr[4] = static_cast<std::uint32_t>(index);
                ctx.gpr[5] = 2u;
                ctx.gpr[6] = 0u;
                ctx.gpr[7] = 0u;
                if (index >= 0) {
                    (void)call_keep(rt, ctx, 0x08AEF874u, {static_cast<std::uint32_t>(index), 2u, 0u, 0u});
                    if (dir == 0u) {
                        count = rt.memory().load8(node + 0x36u);
                    } else {
                        (void)call_keep(rt, ctx, 0x08AEF1A8u, {static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(slot + 8u))))});
                        count = rt.memory().load8(node + 0x36u);
                        rt.memory().store16(slot + 8u, 0xFFFFu);
                    }
                }
                ++i;
                ctx.gpr[18] = i;
                ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(count)));
                const bool more = signed_of(i) < static_cast<std::int8_t>(count);
                ctx.gpr[16] = slot + 2u;
                slot += 2u;
                if (!more) break;
            }
        }
        // 0x08AE7E8C / 0x08AE7E90: next = node[4]; a next node whose word [8] is negative (or none) ends the walk.
        const std::uint32_t next = word(rt, node + 4u);
        flag = (word(rt, next + 8u) >> 31) & 1u;
        ctx.gpr[2] = flag;
        node = flag != 0u ? 0u : next;
        ctx.gpr[17] = node;
        ctx.gpr[3] = node;
    }
    ctx.gpr[2] = flag;
}

// func_08BE78E8(dst, count, src): for count records of 0x60 bytes at dst (a null dst is skipped): the words src[4], src[8] and the floats src[0xC..0x1C] are copied to dst[4..0x1C], dst[0] = D_08CCFF88; func_0896418C(dst + 0x20, src + 0x20),
// dst[0x30] = src[0x30], func_0896418C(dst + 0x40, src + 0x40), dst[0x50] = src[0x50]. v0 is the last word loaded (the entry v0 for count 0).
// Not registered: every compared case has count 0 (returns at once); the record loop is unverified.
void func_08BE78E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst0 = ctx.gpr[4], count = ctx.gpr[5], src = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[23] = count;
    ctx.gpr[17] = src;
    if (count == 0u) return;
    std::uint32_t dst = dst0;
    for (std::uint32_t n = 1;; ++n) {
        ctx.gpr[16] = dst;
        ctx.gpr[30] = 0x08CCFF88u;
        ctx.gpr[19] = n;
        ctx.gpr[21] = src + 0x20u;
        ctx.gpr[22] = src + 0x40u;
        ctx.gpr[18] = dst + 0x20u;
        ctx.gpr[4] = dst + 0x20u;
        ctx.gpr[5] = src + 0x20u;
        ctx.gpr[20] = dst + 0x40u;
        if (dst != 0u) {
            const std::uint32_t w4 = word(rt, src + 4u), w8 = word(rt, src + 8u);
            const std::uint32_t f0 = word(rt, src + 0xCu), f1 = word(rt, src + 0x10u), f2 = word(rt, src + 0x14u), f3 = word(rt, src + 0x18u), f4 = word(rt, src + 0x1Cu);
            ctx.gpr[2] = w4;
            ctx.gpr[3] = w8;
            ctx.fpr[0] = fbits(f0);
            ctx.fpr[1] = fbits(f1);
            ctx.fpr[2] = fbits(f2);
            ctx.fpr[3] = fbits(f3);
            ctx.fpr[4] = fbits(f4);
            put(rt, dst + 4u, w4);
            put(rt, dst + 8u, w8);
            put(rt, dst + 0xCu, f0);
            put(rt, dst + 0x10u, f1);
            put(rt, dst + 0x14u, f2);
            put(rt, dst + 0x18u, f3);
            put(rt, dst + 0x1Cu, f4);
            put(rt, dst, 0x08CCFF88u);
            (void)call_keep(rt, ctx, 0x0896418Cu, {dst + 0x20u, src + 0x20u});
            ctx.gpr[4] = dst + 0x40u;
            ctx.gpr[2] = word(rt, src + 0x30u);
            ctx.gpr[5] = src + 0x40u;
            put(rt, dst + 0x30u, ctx.gpr[2]);
            (void)call_keep(rt, ctx, 0x0896418Cu, {dst + 0x40u, src + 0x40u});
            ctx.gpr[2] = word(rt, src + 0x40u + 0x10u);
            put(rt, dst + 0x50u, ctx.gpr[2]);
        }
        dst += 0x60u;
        if (n == count) break;
    }
}

// func_08892A7C(out): p = func_08B62344(0xB0); p[0] = p[4] = 0, func_0888F3CC(p + 8), p[0x10] and p[0x11] (bytes) = 0, func_08891194(p + 0x14), the words p[0x8C..0x9C], p[0xA4], p[0xA8] and the bytes p[0xA0], p[0xAC], p[0xAD] = 0, out[0] = p;
// the guarded static D_08D75E98 (guard byte, func_08B6441C / func_08B64494) holds D_08D75EA0 (set to D_08CC1D48 on the first call); out[1] = D_08D75EA0, (out[0])[0x14] = out. v0 = out[0] = p.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte D_08D75E98 is always 0 on entry and func_08B6441C always answers non-zero (the guard-set path and the answer-0 path skip the holder initialisation; 0x08892B04 and 0x08892B1C).
void func_08892A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t guard = 0x08D75E98u, holder = 0x08D75EA0u;
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    const std::uint32_t p = call_keep(rt, ctx, 0x08B62344u, {0xB0u});
    ctx.gpr[16] = p;
    put(rt, p, 0u);
    put(rt, p + 4u, 0u);
    (void)call_keep(rt, ctx, 0x0888F3CCu, {p + 8u});
    rt.memory().store8(p + 0x10u, 0u);
    rt.memory().store8(p + 0x11u, 0u);
    (void)call_keep(rt, ctx, 0x08891194u, {p + 0x14u});
    put(rt, p + 0x8Cu, 0u);
    ctx.gpr[19] = holder;
    put(rt, p + 0x90u, 0u);
    ctx.gpr[18] = guard;
    put(rt, p + 0x94u, 0u);
    put(rt, p + 0x98u, 0u);
    put(rt, p + 0x9Cu, 0u);
    rt.memory().store8(p + 0xA0u, 0u);
    put(rt, p + 0xA4u, 0u);
    put(rt, p + 0xA8u, 0u);
    rt.memory().store8(p + 0xACu, 0u);
    rt.memory().store8(p + 0xADu, 0u);
    put(rt, out, p);
    ctx.gpr[2] = static_cast<std::uint32_t>(byte_at(rt, guard));
    ctx.gpr[4] = guard;
    if (ctx.gpr[2] == 0u) {
        const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
        ctx.gpr[4] = guard;
        if (acquired != 0u) {
            put(rt, holder, 0x08CC1D48u);
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
        }
    }
    const std::uint32_t object = word(rt, out);
    ctx.gpr[2] = object;
    put(rt, out + 4u, holder);
    put(rt, object + 0x14u, out);
}

// func_0887DE44(this, a1): this[0x50] = this[0x54] = 0; a = func_08B62344(0x28): a[0] = D_08CC0F88, a[4] = a[8] = 0, a[0xC] = a[0x10] = D_08D752A4, a[0x14..0x20] = 0, bytes a[0x24] and a[0x25] = 0, bit 16 of the word a[0x24] cleared; this[0x58] = a;
// b = func_08B62344(0x10): b[0] = D_08CC0FC8, b[4] = b[8] = 1, b[0xC] = a; this[0x5C] = b; func_08B8E380(this + 0x5C, a, a, 1); this[0x60] = a1. v0 is the last call's.
// Branches: none (a straight line).
void func_0887DE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = self;
    ctx.gpr[20] = a1;
    ctx.gpr[18] = self + 0x58u;
    put(rt, self + 0x50u, 0u);
    put(rt, self + 0x54u, 0u);
    const std::uint32_t a = call_keep(rt, ctx, 0x08B62344u, {0x28u});
    ctx.gpr[17] = a;
    put(rt, a, 0x08CC0F88u);
    put(rt, a + 4u, 0u);
    put(rt, a + 8u, 0u);
    put(rt, a + 0xCu, 0x08D752A4u);
    put(rt, a + 0x10u, 0x08D752A4u);
    for (std::uint32_t i = 0; i < 2u; ++i) {
        put(rt, a + 0x14u + (i << 3), 0u);
        put(rt, a + 0x18u + (i << 3), 0u);
    }
    rt.memory().store8(a + 0x24u, 0u);
    ctx.gpr[16] = self + 0x5Cu;
    rt.memory().store8(a + 0x25u, 0u);
    put(rt, a + 0x24u, word(rt, a + 0x24u) & ~0x10000u);
    put(rt, self + 0x58u, a);
    const std::uint32_t b = call_keep(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, b + 4u, 1u);
    put(rt, b + 0xCu, a);
    put(rt, b, 0x08CC0FC8u);
    put(rt, b + 8u, 1u);
    put(rt, self + 0x5Cu, b);
    (void)call_keep(rt, ctx, 0x08B8E380u, {self + 0x5Cu, a, a, 1u});
    put(rt, self + 0x60u, a1);
}

// func_08894574(out, src): p = func_08B62344(0x24), p[0x1C] = 0, out[0] = p; the guarded static D_08D75EC8 (guard byte, func_08B6441C / func_08B64494) holds D_08D75ED0 (set to D_08CC1DD8 on the first call); out[1] = D_08D75ED0;
// then the nine words of **src (0x24 bytes) are copied to *out in 16-byte chunks. v0 = the last word copied.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte D_08D75EC8 is always already set: the initialisation (func_08B6441C, the holder word D_08CC1DD8, func_08B64494; 0x088945B4-0x088945E8) is read from the asm only.
void func_08894574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], src_ref = ctx.gpr[5];
    const std::uint32_t guard = 0x08D75EC8u, holder = 0x08D75ED0u;
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = out;
    ctx.gpr[17] = src_ref;
    const std::uint32_t p = call_keep(rt, ctx, 0x08B62344u, {0x24u});
    put(rt, p + 0x1Cu, 0u);
    put(rt, out, p);
    ctx.gpr[18] = guard;
    ctx.gpr[2] = static_cast<std::uint32_t>(byte_at(rt, guard));
    ctx.gpr[19] = holder;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[4] = guard;
        const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
        if (acquired != 0u) {
            put(rt, holder, 0x08CC1DD8u);
            ctx.gpr[4] = guard;
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
        } else {
            ctx.gpr[2] = 0x08D70000u;
        }
    }
    std::uint32_t from = word(rt, src_ref);
    std::uint32_t to = word(rt, out);
    put(rt, out + 4u, holder);
    for (std::uint32_t chunk = 0; chunk < 2u; ++chunk) {
        const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
        put(rt, to, w0);
        put(rt, to + 4u, w1);
        put(rt, to + 8u, w2);
        put(rt, to + 0xCu, w3);
        ctx.gpr[2] = w0;
        ctx.gpr[3] = w1;
        ctx.gpr[4] = w2;
        ctx.gpr[5] = w3;
        from += 0x10u;
        to += 0x10u;
    }
    ctx.gpr[2] = word(rt, from);
    put(rt, to, ctx.gpr[2]);
}

// func_088FEA94(out): p = func_08B62344(0x64): p[0x30] = -1, the words p[0..0x2C] and p[0x34..0x40] = 0, func_08B36018(p + 0x44, 0, 0x20); out[0] = p;
// the guarded static D_08D76988 (guard byte, func_08B6441C / func_08B64494) holds D_08D76990 (set to D_08CC5370 on the first call); out[1] = D_08D76990, out[9] (byte) = 1, out[8], out[0xA], out[0xB], out[0xC] (bytes) = 0;
// the tail call is func_08B28DCC(out + 0x10).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte D_08D76988 is always 0 on entry and func_08B6441C always answers non-zero (the guard-set and answer-0 paths skip the holder initialisation; 0x088FEB28 and 0x088FEB40).
void func_088FEA94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t guard = 0x08D76988u, holder = 0x08D76990u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = out;
        const std::uint32_t p = call_keep(rt, ctx, 0x08B62344u, {0x64u});
        ctx.gpr[16] = p;
        put(rt, p + 0x30u, 0xFFFFFFFFu);
        for (std::uint32_t offset = 0; offset <= 0x2Cu; offset += 4u) put(rt, p + offset, 0u);
        put(rt, p + 0x34u, 0u);
        put(rt, p + 0x38u, 0u);
        put(rt, p + 0x3Cu, 0u);
        put(rt, p + 0x40u, 0u);
        (void)call_keep(rt, ctx, 0x08B36018u, {p + 0x44u, 0u, 0x20u});
        put(rt, out, p);
        ctx.gpr[16] = guard;
        ctx.gpr[18] = holder;
        ctx.gpr[3] = static_cast<std::uint32_t>(byte_at(rt, guard));
        ctx.gpr[4] = guard;
        if (ctx.gpr[3] == 0u) {
            const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
            ctx.gpr[4] = guard;
            if (acquired != 0u) {
                put(rt, holder, 0x08CC5370u);
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            }
        }
        ctx.gpr[2] = 1u;
        put(rt, out + 4u, holder);
        ctx.gpr[4] = out + 0x10u;
        rt.memory().store8(out + 9u, 1u);
        rt.memory().store8(out + 8u, 0u);
        rt.memory().store8(out + 0xAu, 0u);
        rt.memory().store8(out + 0xBu, 0u);
        rt.memory().store8(out + 0xCu, 0u);
    }
    tail_call(ctx, 0x08B28DCCu, {out + 0x10u});
}

// func_08AF464C(this): this[0] = D_08CD72C0, this[4] = 60.0, this[8] = 0.1, this[0xC] = 5000.0, this[0x10] = 1.7777778; func_08AF42FC(this, f12 = 0, f13 = 1, f14 = 0); func_08AF430C(this, 0, 0, 0); func_08AF431C(this, 30, 10, 0);
// then this[0x4C] = 5500.0, this[0x38] = 10.0, this[0x3C] = 8.0, this[0x40] = 0.001, this[0x44] = 0.0350, this[0x48] = 1.0 (constants are the words in the code). v0 is the last call's.
// Branches: none (a straight line).
void func_08AF464C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = 0x08CD72C0u;
    ctx.fpr[12] = 0.0f;
    ctx.fpr[0] = fbits(0x42700000u);
    ctx.fpr[14] = ctx.fpr[12];
    ctx.fpr[1] = fbits(0x3DCCCCCDu);
    ctx.fpr[13] = fbits(0x3F800000u);
    ctx.gpr[16] = self;
    put(rt, self + 4u, 0x42700000u);
    put(rt, self + 8u, 0x3DCCCCCDu);
    put(rt, self + 0xCu, 0x459C4000u);
    put(rt, self + 0x10u, 0x3FE38E39u);
    put(rt, self, 0x08CD72C0u);
    ctx.fpr[0] = fbits(0x459C4000u);
    ctx.fpr[1] = fbits(0x3FE38E39u);
    (void)call_keep(rt, ctx, 0x08AF42FCu, {self});
    ctx.fpr[12] = 0.0f;
    ctx.fpr[13] = ctx.fpr[12];
    ctx.fpr[14] = ctx.fpr[12];
    (void)call_keep(rt, ctx, 0x08AF430Cu, {self});
    ctx.fpr[14] = 0.0f;
    ctx.fpr[12] = fbits(0x41F00000u);
    ctx.fpr[13] = fbits(0x41200000u);
    (void)call_keep(rt, ctx, 0x08AF431Cu, {self});
    put(rt, self + 0x4Cu, 0x45ABE000u);
    put(rt, self + 0x38u, 0x41200000u);
    put(rt, self + 0x3Cu, 0x41000000u);
    put(rt, self + 0x40u, 0x3A83126Fu);
    put(rt, self + 0x44u, 0x3D0F5C29u);
    put(rt, self + 0x48u, 0x3F800000u);
    ctx.fpr[0] = fbits(0x3D0F5C29u);
    ctx.fpr[1] = fbits(0x3F800000u);
}

// func_08AC8FDC(a0, a1, key): q = func_08AD337C(D_08D00000-based table, a1); none: v0 = 0. Else the record at frame + 0x40 is filled by func_08B10204(frame + 0x40, key, 1) and copied (0x3C bytes) to the frame at sp; when its first word is zero:
// s = word at sp + 0x20: func_08AD35C8(s), q[0xC] = q[0xD] (bytes) = 1, q[4] = s, q[8] = word at sp + 0x10, v0 = 1; else v0 = 0.
// Not registered: every case would sleep (not comparable).
void func_08AC8FDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a1 = ctx.gpr[5], key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = key;
    ctx.gpr[4] = 0x08D00000u;
    const std::uint32_t q = call_keep(rt, ctx, 0x08AD337Cu, {0x08D00000u, a1});
    ctx.gpr[18] = q;
    if (q == 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[16] = frame.at(0x40u);
    ctx.gpr[5] = key;
    ctx.gpr[6] = 1u;
    (void)call_keep(rt, ctx, 0x08B10204u, {frame.at(0x40u), key, 1u});
    std::uint32_t from = frame.at(0x40u), to = frame.at(0u);
    while (from != frame.at(0x70u)) {
        const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
        put(rt, to, w0);
        from += 0x10u;
        to += 0x10u;
        put(rt, to - 0xCu, w1);
        put(rt, to - 8u, w2);
        put(rt, to - 4u, w3);
    }
    const std::uint32_t t0 = word(rt, from + 8u), t1 = word(rt, from), t2 = word(rt, from + 4u);
    put(rt, to + 8u, t0);
    put(rt, to, t1);
    put(rt, to + 4u, t2);
    ctx.gpr[16] = from;
    if (word(rt, frame.at(0u)) != 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t s = word(rt, frame.at(0x20u));
    ctx.gpr[16] = s;
    ctx.gpr[4] = s;
    (void)call_keep(rt, ctx, 0x08AD35C8u, {s});
    const std::uint32_t extra = word(rt, frame.at(0x10u));
    rt.memory().store8(q + 0xDu, 1u);
    put(rt, q + 4u, s);
    put(rt, q + 8u, extra);
    rt.memory().store8(q + 0xCu, 1u);
    ctx.gpr[4] = 1u;
    ctx.gpr[2] = 1u;
}

// func_08C49A28(this, first, last): a vector erase of 0xF0-byte trivially copyable elements: n = ((this[4] - last) >> 4) * 0xEEEEEEEF (low 32 bits, signed) elements are copied from `last` down to `first` in 16-byte chunks (none when n <= 0);
// func_08C47014(end of the copy, this[4], sp) follows; this[4] -= (last - first) & ~0xF; v0 = first.
// Not registered: compared only on the empty range (no element copied); the copy loops are unverified.
void func_08C49A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = last;
    ctx.gpr[17] = self;
    ctx.gpr[16] = first;
    const std::uint32_t count = (static_cast<std::uint32_t>(signed_of(word(rt, self + 4u) - last) >> 4)) * 0xEEEEEEEFu;
    std::uint32_t to = first;
    if (signed_of(count) > 0) {
        std::uint32_t from = last;
        for (std::uint32_t n = 0; n != count; ++n) {
            for (std::uint32_t chunk = 0; chunk < 15u; ++chunk) {
                const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
                put(rt, to, w0);
                put(rt, to + 4u, w1);
                put(rt, to + 8u, w2);
                put(rt, to + 0xCu, w3);
                from += 0x10u;
                to += 0x10u;
            }
        }
    }
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[4] = to;
    (void)call_keep(rt, ctx, 0x08C47014u, {to, end, frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((last - first) & ~0xFu));
    ctx.gpr[2] = first;
}

// func_08C49D08(this, first, last): a vector erase of 6-byte elements (three halfwords): n = ((this[4] - last) >> 1) * 0xAAAAAAAB (low 32 bits, signed) elements are copied from `last` down to `first`; func_08C470A4(end of the copy, this[4], sp)
// follows (the end is `last` itself when n <= 0); this[4] -= (last - first) & ~1; v0 = first.
// Not registered: compared only on the empty range (no element copied); the copy loop is unverified.
void func_08C49D08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = last;
    ctx.gpr[17] = self;
    ctx.gpr[16] = first;
    const std::uint32_t count = (static_cast<std::uint32_t>(signed_of(word(rt, self + 4u) - last) >> 1)) * 0xAAAAAAABu;
    std::uint32_t to = last;
    if (signed_of(count) > 0) {
        std::uint32_t from = last;
        to = first;
        for (std::uint32_t n = 0; n != count; ++n) {
            const std::uint32_t h0 = rt.memory().load16(from), h1 = rt.memory().load16(from + 2u), h2 = rt.memory().load16(from + 4u);
            rt.memory().store16(to, static_cast<std::uint16_t>(h0));
            from += 6u;
            rt.memory().store16(to + 2u, static_cast<std::uint16_t>(h1));
            rt.memory().store16(to + 4u, static_cast<std::uint16_t>(h2));
            to += 6u;
        }
    }
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[4] = to;
    (void)call_keep(rt, ctx, 0x08C470A4u, {to, end, frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((last - first) & ~1u));
    ctx.gpr[2] = first;
}

// func_08B89338(this, first, last): a vector erase of 16-byte elements: n = (this[4] - last) >> 4 elements are copied from `last` down to `first` (none when n <= 0); func_08B89328(end of the copy, this[4], sp) follows;
// this[4] -= (last - first) & ~0xF; v0 = first.
// Not registered: compared only on the empty range (no element copied); the copy loop is unverified.
void func_08B89338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = last;
    ctx.gpr[17] = self;
    ctx.gpr[16] = first;
    const std::int32_t count = signed_of(word(rt, self + 4u) - last) >> 4;
    std::uint32_t to = first;
    if (count > 0) {
        std::uint32_t from = last;
        for (std::int32_t n = 0; n != count; ++n) {
            const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
            put(rt, to, w0);
            put(rt, to + 4u, w1);
            put(rt, to + 8u, w2);
            put(rt, to + 0xCu, w3);
            from += 0x10u;
            to += 0x10u;
        }
    }
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[4] = to;
    (void)call_keep(rt, ctx, 0x08B89328u, {to, end, frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((last - first) & ~0xFu));
    ctx.gpr[2] = first;
}

// func_08B9AC68(a0, p): when p is non-null: the 48 entries p[0x124 + 12 i] are destroyed backwards from p + 0x364 with func_08AC26B4(entry + 8) (when p + 0x124 differs from p + 0x364), then the 24 entries before p + 0x124 (when p + 4 differs from p + 0x124)
// the same way; the tail call is func_08B64654(p). A null p returns at once.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): p (a1) is never null (the `beqz a1` at 0x08B9AC80 returns at once), both ranges are always walked (the `beqz`/`beq` tests at 0x08B9AC8C, 0x08B9AC98, 0x08B9ACC0 and 0x08B9ACC8 never skip a loop).
void func_08B9AC68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t p = ctx.gpr[5];
    if (p == 0u) return;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = p;
        const std::uint32_t array = p + 0x124u;
        ctx.gpr[18] = array;
        ctx.gpr[2] = p + 4u;
        if (array != 0u) {
            std::uint32_t entry = p + 0x364u;
            ctx.gpr[16] = entry;
            if (array != entry) {
                std::uint32_t n = 0x30u;
                entry -= 0xCu;
                for (;;) {
                    ctx.gpr[16] = entry;
                    --n;
                    ctx.gpr[17] = n;
                    ctx.gpr[4] = entry + 8u;
                    (void)call_keep(rt, ctx, 0x08AC26B4u, {entry + 8u});
                    if (n == 0u) break;
                    entry -= 0xCu;
                }
            }
            ctx.gpr[2] = p + 4u;
        }
        ctx.gpr[4] = p;
        if (ctx.gpr[2] != 0u && ctx.gpr[2] != array) {
            std::uint32_t entry = array - 0xCu;
            std::uint32_t n = 0x18u;
            for (;;) {
                ctx.gpr[16] = entry;
                --n;
                ctx.gpr[17] = n;
                ctx.gpr[4] = entry + 8u;
                (void)call_keep(rt, ctx, 0x08AC26B4u, {entry + 8u});
                entry -= 0xCu;
                if (n == 0u) break;
            }
            ctx.gpr[4] = p;
        }
    }
    tail_call(ctx, 0x08B64654u, {p});
}

// func_08B95080(a0, node): the destroyer of a tree: for each node n on the chain (n[8] is the next, n[0xC] the left subtree): func_08B95080(a0, n[0xC]); the holder s = n[0x14] - 0xC: unless it is D_08D75298, a refcount func_08B60804(n[0x14] - 4, -1)
// that answers <= 0 releases it with func_08B93554(s, sp); then func_08B35254(n). A null node returns at once.
// Not registered: compared 0 in the fast gate.
void func_08B95080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    if (node == 0u) return;
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = a0;
    ctx.gpr[20] = 0x08D75298u;
    for (;;) {
        ctx.gpr[17] = node;
        ctx.gpr[4] = a0;
        (void)call_keep(rt, ctx, 0x08B95080u, {a0, word(rt, node + 0xCu)});
        const std::uint32_t holder = word(rt, node + 0x14u);
        const std::uint32_t next = word(rt, node + 8u);
        ctx.gpr[5] = 0xFFFFFFFFu;
        ctx.gpr[18] = next;
        const std::uint32_t slot = holder - 0xCu;
        ctx.gpr[16] = slot;
        ctx.gpr[4] = holder - 4u;
        ctx.gpr[2] = holder;
        if (slot != 0x08D75298u) {
            const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {holder - 4u, 0xFFFFFFFFu});
            ctx.gpr[4] = slot;
            ctx.gpr[5] = frame.at(0u);
            if (signed_of(count) <= 0) (void)call_keep(rt, ctx, 0x08B93554u, {slot, frame.at(0u)});
        }
        ctx.gpr[4] = node;
        (void)call_keep(rt, ctx, 0x08B35254u, {node});
        ctx.gpr[17] = next;
        if (next == 0u) break;
        node = next;
    }
}

// func_08C246E4(str, chars, pos, mode): a reverse search in a string: len = the word before **str (-0xC); an empty string answers -1. Else i = min(pos, len - 1) (unsigned) counts down: func_08B79998(chars, (int8) str_data[i], mode) answering non-zero returns i;
// after len attempts the answer is -1.
// Not registered: compared only on the first attempt (the character is found at the starting position and the function returns at once); the loop continuation and the -1 answer are unverified.
void func_08C246E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t str = ctx.gpr[4], chars = ctx.gpr[5], pos = ctx.gpr[6], mode = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = str;
    ctx.gpr[20] = chars;
    ctx.gpr[19] = mode;
    std::uint32_t data = word(rt, str);
    const std::uint32_t length = word(rt, data - 0xCu);
    if (length == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[16] = 0xFFFFFFFFu;
        return;
    }
    const std::uint32_t last = length - 1u;
    std::uint32_t at = pos < last ? pos : last;
    ctx.gpr[18] = at;
    std::uint32_t tried = 0u;
    for (;;) {
        ctx.gpr[16] = at;
        ctx.gpr[17] = tried;
        const std::uint32_t character = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, data + at)));
        ctx.gpr[5] = character;
        ctx.gpr[4] = chars;
        ctx.gpr[6] = mode;
        const std::uint32_t found = call_keep(rt, ctx, 0x08B79998u, {chars, character, mode});
        ++tried;
        ctx.gpr[17] = tried;
        if (found == 0u) {
            ctx.gpr[2] = at;
            return;
        }
        ctx.gpr[3] = last + 1u;
        --at;
        ctx.gpr[16] = at;
        if (last + 1u == tried) break;
        data = word(rt, str);
    }
    at = 0xFFFFFFFFu;
    ctx.gpr[16] = at;
    ctx.gpr[2] = at;
}

// func_08C49C28(this, first, last): a vector erase of 0x54-byte trivially copyable elements: n = ((this[4] - last) >> 2) * 0x3CF3CF3D (low 32 bits, signed) elements (five 16-byte chunks and one word each) are copied from `last` down to `first` (none when n <= 0);
// func_08C47024(end of the copy, this[4], sp) follows; this[4] -= (last - first) & ~3; v0 = first.
// Not registered: compared only on the empty range (no element copied); the copy loops are unverified.
void func_08C49C28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = last;
    ctx.gpr[17] = self;
    ctx.gpr[16] = first;
    const std::uint32_t count = (static_cast<std::uint32_t>(signed_of(word(rt, self + 4u) - last) >> 2)) * 0x3CF3CF3Du;
    std::uint32_t to = first;
    if (signed_of(count) > 0) {
        std::uint32_t from = last;
        for (std::uint32_t n = 0; n != count; ++n) {
            for (std::uint32_t chunk = 0; chunk < 5u; ++chunk) {
                const std::uint32_t w0 = word(rt, from), w1 = word(rt, from + 4u), w2 = word(rt, from + 8u), w3 = word(rt, from + 0xCu);
                put(rt, to, w0);
                put(rt, to + 4u, w1);
                put(rt, to + 8u, w2);
                put(rt, to + 0xCu, w3);
                from += 0x10u;
                to += 0x10u;
            }
            const std::uint32_t tail = word(rt, from);
            put(rt, to, tail);
            from += 4u;
            to += 4u;
        }
    }
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[4] = to;
    (void)call_keep(rt, ctx, 0x08C47024u, {to, end, frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((last - first) & ~3u));
    ctx.gpr[2] = first;
}

// The vector erase of 32-byte elements (a 16-byte value, a pad and a word at +0x10) that func_08B895E0 and func_08C0DBBC share: n = (this[4] - last) >> 5 elements are assigned from `last` down to `first` (func_089641C4(dst, src) when they differ, then dst[0x10] = src[0x10]);
// `destroy`(end of the copy, this[4], sp) follows; this[4] -= (last - first) & ~0x1F; v0 = first.
inline void vector_erase_32(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t destroy) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[23] = self + 4u;
    ctx.gpr[22] = last;
    ctx.gpr[21] = self;
    ctx.gpr[20] = first;
    const std::int32_t count = signed_of(word(rt, self + 4u) - last) >> 5;
    ctx.gpr[19] = static_cast<std::uint32_t>(count);
    std::uint32_t to = first;
    if (count > 0) {
        std::uint32_t from = last;
        for (std::int32_t n = 1;; ++n) {
            ctx.gpr[16] = to;
            ctx.gpr[17] = from;
            ctx.gpr[18] = static_cast<std::uint32_t>(n);
            ctx.gpr[4] = to;
            ctx.gpr[5] = from;
            if (from != to) {
                (void)call_keep(rt, ctx, 0x089641C4u, {to, from});
                const std::uint32_t value = word(rt, from + 0x10u);
                ctx.gpr[3] = value;
                put(rt, to + 0x10u, value);
            }
            from += 0x20u;
            to += 0x20u;
            if (n == count) break;
        }
    }
    ctx.gpr[16] = to;
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[5] = end;
    ctx.gpr[4] = to;
    (void)call_keep(rt, ctx, destroy, {to, end, frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((last - first) & ~0x1Fu));
    ctx.gpr[2] = first;
}

// func_08B895E0(this, first, last): vector_erase_32 with the destroyer func_08B892A0.
// Not registered: compared only on the empty range (vector_erase_32 copies no element); the assignment loop is unverified.
void func_08B895E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_erase_32(rt, ctx, 0x08B892A0u); }

// func_08C0DBBC(this, first, last): vector_erase_32 with the destroyer func_08C0D870.
// Not registered: compared only on the empty range (vector_erase_32 copies no element); the assignment loop is unverified.
void func_08C0DBBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_erase_32(rt, ctx, 0x08C0D870u); }

// func_08B8AA60(dst, count, src): count 24-byte records at dst (a null dst is skipped) are built from src: dst[0..8) = src[0..8), dst[8] = dst[0xC] = dst[0x10] = 0, then with size = (src[0xC] - src[8]) & ~1 and p = func_08A5BC20(size, 4):
// dst[8] = p, dst[0xC] = p, func_08B8A654(src[8], src[0xC], p, sp) and dst[0x10] = p + size, dst[0xC] = its answer. A count of 0 returns at once; v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): count is always 1 (the loop runs once; the `bne s4, s6` at 0x08B8AB08 would repeat it with the next record at +0x18), dst is never null (the `beqz s1` at 0x08B8AAA4 would skip a record) and count 0 returns at once only in the asm.
void func_08B8AA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dst0 = ctx.gpr[4], count = ctx.gpr[5], src = ctx.gpr[6];
    if (count == 0u) return;
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[22] = count;
    ctx.gpr[19] = src;
    ctx.gpr[21] = src + 8u;
    std::uint32_t dst = dst0;
    for (std::uint32_t n = 1;; ++n) {
        ctx.gpr[17] = dst;
        ctx.gpr[18] = dst + 8u;
        ctx.gpr[5] = 4u;
        ctx.gpr[20] = n;
        if (dst != 0u) {
            const std::uint32_t w0 = word(rt, src), w1 = word(rt, src + 4u);
            put(rt, dst, w0);
            put(rt, dst + 4u, w1);
            const std::uint32_t end = word(rt, src + 0xCu);
            const std::uint32_t begin = word(rt, src + 8u);
            put(rt, dst + 0xCu, 0u);
            const std::uint32_t size = (end - begin) & ~1u;
            ctx.gpr[16] = size;
            ctx.gpr[4] = size;
            put(rt, dst + 8u, 0u);
            put(rt, dst + 0x10u, 0u);
            const std::uint32_t p = call_keep(rt, ctx, 0x08A5BC20u, {size, 4u});
            ctx.gpr[16] = p + size;
            put(rt, dst + 8u, p);
            put(rt, dst + 0xCu, p);
            ctx.gpr[4] = word(rt, src + 8u);
            ctx.gpr[5] = word(rt, src + 0xCu);
            ctx.gpr[6] = p;
            ctx.gpr[7] = frame.at(0u);
            put(rt, dst + 0x10u, p + size);
            const std::uint32_t last = call_keep(rt, ctx, 0x08B8A654u, {word(rt, src + 8u), word(rt, src + 0xCu), p, frame.at(0u)});
            put(rt, dst + 0xCu, last);
        }
        dst += 0x18u;
        if (n == count) break;
    }
}

// func_0898DF5C(this, other): func_08995138(this); a null `other` returns. Else func_0898449C(this + 0x130, other + 0x130), this[0x220] and this[0x224] = other's, 128 times func_08B855E0(this + 0x228 + 4 i, other + 0x228 + 4 i) each followed by the byte this[0x428 + i] = other[0x428 + i],
// then this[0x4A8..0x4BC] = other's (a byte at 0x4C0, words at 0x4A8, 0x4AC, 0x4B0 (float), 0x4B4, 0x4B8 (float), 0x4BC). v0 is the byte at 0x4C0 (the call's for a null other).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): `other` is never null (the `beqz s6` at 0x0898DF90 returns right after func_08995138).
void func_0898DF5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[23] = self;
    ctx.gpr[22] = other;
    (void)call_keep(rt, ctx, 0x08995138u, {self, other});
    if (other == 0u) return;
    ctx.gpr[4] = self + 0x130u;
    ctx.gpr[5] = other + 0x130u;
    (void)call_keep(rt, ctx, 0x0898449Cu, {self + 0x130u, other + 0x130u});
    const std::uint32_t v1 = word(rt, other + 0x220u), v0 = word(rt, other + 0x224u);
    put(rt, self + 0x220u, v1);
    put(rt, self + 0x224u, v0);
    ctx.gpr[21] = 0x80u;
    for (std::uint32_t i = 0; i < 0x80u; ++i) {
        ctx.gpr[19] = other + i;
        ctx.gpr[18] = self + i;
        ctx.gpr[20] = i;
        ctx.gpr[17] = other + 0x228u + (i << 2);
        ctx.gpr[16] = self + 0x228u + (i << 2);
        ctx.gpr[4] = self + 0x228u + (i << 2);
        ctx.gpr[5] = other + 0x228u + (i << 2);
        (void)call_keep(rt, ctx, 0x08B855E0u, {self + 0x228u + (i << 2), other + 0x228u + (i << 2)});
        const std::uint32_t byte = rt.memory().load8(other + 0x428u + i);
        ctx.gpr[3] = byte;
        rt.memory().store8(self + 0x428u + i, static_cast<std::uint8_t>(byte));
    }
    const std::uint32_t flag = rt.memory().load8(other + 0x4C0u);
    const std::uint32_t w4A8 = word(rt, other + 0x4A8u), w4AC = word(rt, other + 0x4ACu), w4B0 = word(rt, other + 0x4B0u);
    const std::uint32_t w4B4 = word(rt, other + 0x4B4u), w4B8 = word(rt, other + 0x4B8u), w4BC = word(rt, other + 0x4BCu);
    ctx.gpr[2] = flag;
    ctx.gpr[3] = w4A8;
    ctx.gpr[4] = w4AC;
    ctx.fpr[0] = fbits(w4B0);
    ctx.gpr[5] = w4B4;
    ctx.fpr[1] = fbits(w4B8);
    ctx.gpr[6] = w4BC;
    rt.memory().store8(self + 0x4C0u, static_cast<std::uint8_t>(flag));
    put(rt, self + 0x4A8u, w4A8);
    put(rt, self + 0x4ACu, w4AC);
    put(rt, self + 0x4B0u, w4B0);
    put(rt, self + 0x4B4u, w4B4);
    put(rt, self + 0x4B8u, w4B8);
    put(rt, self + 0x4BCu, w4BC);
}

// The release of a reference-counted string body that the temporary at `holder` points to (its data pointer p; body = p - 0xC; D_08D74FD0 is the shared empty body): unless it is the empty one,
// func_08B60804(p - 4, -1) takes a reference away and when that answers <= 0 func_08B8542C(body, sp) frees it.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder, std::uint32_t frame_sp) {
    const std::uint32_t data = word(rt, holder);
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[3] = data;
    ctx.gpr[16] = body;
    ctx.gpr[2] = 0x08D74FD0u;  // the shared empty body is in v0 for the compare: the function leaves it there when the body is shared
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (body != 0x08D74FD0u) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = frame_sp;
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {body, frame_sp});
    }
}

// func_089AF5E8(this, flag): under the lock D_08CF4ACC (func_08B344E0 .. func_08B3457C) with the temporary string t at the frame (sp + 4): with (flag & 0xFF) non-zero and this[0xFC] (byte) zero this[0xFD] = 0; this[0xFC] = flag;
// func_089ACBB4(t, this), func_089AF334(this, t) and t's release (func_08B60804 / func_08B8542C). v0 is func_08B3457C's.
// Branches: none that the compared cases skip (every branch took both ways).
void func_089AF5E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5] & 0xFFu;
    const std::uint32_t lock = 0x08CF4ACCu;
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = 0x08CF0000u;
    ctx.gpr[18] = frame.at(4u);
    ctx.gpr[17] = flag;
    ctx.gpr[16] = self;
    (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
    ctx.gpr[4] = frame.at(4u);
    ctx.gpr[5] = self;
    if (flag != 0u && rt.memory().load8(self + 0xFCu) == 0u) rt.memory().store8(self + 0xFDu, 0u);
    rt.memory().store8(self + 0xFCu, static_cast<std::uint8_t>(flag));
    (void)call_keep(rt, ctx, 0x089ACBB4u, {frame.at(4u), self});
    ctx.gpr[4] = self;
    ctx.gpr[5] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x089AF334u, {self, frame.at(4u)});
    release_string(rt, ctx, frame.at(4u), frame.at(0u));
    ctx.gpr[4] = lock;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
}

// func_08A6BBE8(this, name): o = this[0] (null: func_08A5BB7C(D_08CB05D8) runs first); the temporary string t (sp + 4) is built by func_08B85EB8(t, name, sp + 1), u (sp + 8) by func_08A6D2D4(u, t), r = func_08A8220C(o, u), u is destroyed (func_08A5B9EC)
// and t released (func_08B60804 / func_08B8542C). v0 = r.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0] is never null (the func_08A5BB7C assertion call is skipped by the `bnez s1` at 0x08A6BC14).
void func_08A6BBE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t this_ref = ctx.gpr[4], name = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = frame.at(4u);
    ctx.gpr[18] = frame.at(8u);
    ctx.gpr[16] = name;
    const std::uint32_t object = word(rt, this_ref);
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CB05D8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB05D8u});
    ctx.gpr[6] = frame.at(1u);
    ctx.gpr[5] = name;
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08B85EB8u, {frame.at(4u), name, frame.at(1u)});
    ctx.gpr[5] = frame.at(4u);
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A6D2D4u, {frame.at(8u), frame.at(4u)});
    ctx.gpr[4] = object;
    ctx.gpr[5] = frame.at(8u);
    const std::uint32_t answer = call_keep(rt, ctx, 0x08A8220Cu, {object, frame.at(8u)});
    ctx.gpr[4] = frame.at(8u);
    ctx.gpr[17] = answer;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    release_string(rt, ctx, frame.at(4u), frame.at(0u));
    ctx.gpr[2] = answer;
}

// func_0899CD50(out): the temporary string u (sp + 8) is built by func_08B85EB8(u, D_08C8C454, sp + 1), func_08A98A70(sp + 4), then func_0899C764(sp + 0xC, 0, 0, u, sp + 4) fills the holder at sp + 0xC;
// unless `out` is that holder func_08A5BAD8(out, holder[0]) copies it; the holder (func_08A5B9EC) and sp + 4 (func_08A5B9EC) are destroyed and u released (func_08B60804 / func_08B8542C). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): `out` is never the holder at sp + 0xC (the copy func_08A5BAD8 always runs; `beq s1, s2` at 0x0899CDAC would skip it).
void func_0899CD50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = frame.at(8u);
    ctx.gpr[19] = frame.at(4u);
    ctx.gpr[17] = out;
    ctx.gpr[4] = frame.at(8u);
    ctx.gpr[5] = 0x08C8C454u;
    ctx.gpr[6] = frame.at(1u);
    (void)call_keep(rt, ctx, 0x08B85EB8u, {frame.at(8u), 0x08C8C454u, frame.at(1u)});
    ctx.gpr[18] = frame.at(0xCu);
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A98A70u, {frame.at(4u)});
    ctx.gpr[4] = frame.at(0xCu);
    ctx.gpr[7] = frame.at(8u);
    ctx.gpr[6] = 0u;
    ctx.gpr[8] = frame.at(4u);
    ctx.gpr[5] = 0u;
    (void)call_keep(rt, ctx, 0x0899C764u, {frame.at(0xCu), 0u, 0u, frame.at(8u), frame.at(4u)});
    ctx.gpr[4] = out;
    if (out != frame.at(0xCu)) {
        ctx.gpr[5] = word(rt, frame.at(0xCu));
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, ctx.gpr[5]});
    }
    ctx.gpr[4] = frame.at(0xCu);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    release_string(rt, ctx, frame.at(8u), frame.at(0u));
}

// func_0897FDE0(out): func_0897E844(sp + 4); o = word at sp + 4 (null: func_08A5BB7C(D_08CA3400) runs first); func_0897F134(sp + 8, o) builds the temporary u, func_08A6D2D4(sp + 0xC, u) the holder; unless `out` is the holder func_08A5BAD8(out, holder[0]) copies it;
// the holder is destroyed (func_08A5B9EC), u released (func_08B60804 / func_08B8542C) and sp + 4 destroyed (func_08A5B9EC). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object at sp + 4 is never null (the func_08A5BB7C assertion call is skipped by the `bnez s0` at 0x0897FE1C) and `out` is never the holder at sp + 0xC (the copy func_08A5BAD8 always runs).
void func_0897FDE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = frame.at(4u);
    ctx.gpr[18] = out;
    ctx.gpr[4] = frame.at(4u);
    ctx.gpr[19] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x0897E844u, {frame.at(4u)});
    ctx.gpr[17] = frame.at(0xCu);
    const std::uint32_t object = word(rt, frame.at(4u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CA3400u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA3400u});
    ctx.gpr[5] = object;
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x0897F134u, {frame.at(8u), object});
    ctx.gpr[4] = frame.at(0xCu);
    ctx.gpr[5] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A6D2D4u, {frame.at(0xCu), frame.at(8u)});
    ctx.gpr[4] = out;
    if (out != frame.at(0xCu)) {
        ctx.gpr[5] = word(rt, frame.at(0xCu));
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, ctx.gpr[5]});
    }
    ctx.gpr[4] = frame.at(0xCu);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    release_string(rt, ctx, frame.at(8u), frame.at(0u));
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}

// func_08A777CC(out, a1): func_08A941CC(sp + 4); func_08AA9960(sp + 0xC, sp + 4); [sp] = a1 and func_08A76AAC(sp + 8, sp); func_08AAADC8(sp + 0xC, sp + 8); o = word at sp + 4 (null: func_08A5BB7C(D_08CB0E7C) runs first);
// func_08A76ABC(out, o + 0x20); then func_08A5B9EC(sp + 8), func_08AA96B4(sp + 0xC), func_08A5B9EC(sp + 4). v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object at sp + 4 is never null (the func_08A5BB7C assertion call is skipped by the `bnez s0` at 0x08A77834).
void func_08A777CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x80u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = frame.at(4u);
    ctx.gpr[20] = out;
    ctx.gpr[18] = frame.at(0xCu);
    ctx.gpr[17] = frame.at(8u);
    ctx.gpr[4] = frame.at(4u);
    ctx.gpr[16] = a1;
    (void)call_keep(rt, ctx, 0x08A941CCu, {frame.at(4u)});
    ctx.gpr[4] = frame.at(0xCu);
    ctx.gpr[5] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08AA9960u, {frame.at(0xCu), frame.at(4u)});
    ctx.gpr[4] = frame.at(8u);
    ctx.gpr[5] = frame.at(0u);
    put(rt, frame.at(0u), a1);
    (void)call_keep(rt, ctx, 0x08A76AACu, {frame.at(8u), frame.at(0u)});
    ctx.gpr[4] = frame.at(0xCu);
    ctx.gpr[5] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08AAADC8u, {frame.at(0xCu), frame.at(8u)});
    const std::uint32_t object = word(rt, frame.at(4u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CB0E7Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0E7Cu});
    ctx.gpr[5] = object + 0x20u;
    ctx.gpr[4] = out;
    (void)call_keep(rt, ctx, 0x08A76ABCu, {out, object + 0x20u});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    ctx.gpr[4] = frame.at(0xCu);
    (void)call_keep(rt, ctx, 0x08AA96B4u, {frame.at(0xCu)});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[2] = out;
}

// func_08A13844(this): this[0] = D_08CD0418; func_08ACF184(word D_08CFFAAC, 0), this[0x41] (byte) = 1, then func_08A1371C(this), func_08B28C74(this + 0x10), func_08B1B99C(this[0x3C]), func_08A12AE0(this), func_08ACEF04(this + 0x240),
// func_08B28BE0(this + 0x200), func_08A19DD4(this + 0x178), func_08A19DD4(this + 0x140), func_08B241A0(this + 0xA0), func_08C15D30(this + 0x5C), func_08B28BE0(this + 0x10), func_089C16C8(this); the tail call is func_08A5BC54(this, 0x7C0, 4).
// Branches: none (a straight line).
void func_08A13844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = 0x08CD0418u;
        ctx.gpr[5] = 0u;
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x10u;
        put(rt, self, 0x08CD0418u);
        ctx.gpr[2] = 0x08D00000u;
        const std::uint32_t static_word = word(rt, 0x08CFFAACu);
        ctx.gpr[4] = static_word;
        (void)call_keep(rt, ctx, 0x08ACF184u, {static_word, 0u});
        ctx.gpr[2] = 1u;
        rt.memory().store8(self + 0x41u, 1u);
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08A1371Cu, {self});
        ctx.gpr[4] = self + 0x10u;
        (void)call_keep(rt, ctx, 0x08B28C74u, {self + 0x10u});
        ctx.gpr[4] = word(rt, self + 0x3Cu);
        (void)call_keep(rt, ctx, 0x08B1B99Cu, {ctx.gpr[4]});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08A12AE0u, {self});
        ctx.gpr[4] = self + 0x240u;
        (void)call_keep(rt, ctx, 0x08ACEF04u, {self + 0x240u});
        ctx.gpr[4] = self + 0x200u;
        (void)call_keep(rt, ctx, 0x08B28BE0u, {self + 0x200u});
        ctx.gpr[4] = self + 0x178u;
        (void)call_keep(rt, ctx, 0x08A19DD4u, {self + 0x178u});
        ctx.gpr[4] = self + 0x140u;
        (void)call_keep(rt, ctx, 0x08A19DD4u, {self + 0x140u});
        ctx.gpr[4] = self + 0xA0u;
        (void)call_keep(rt, ctx, 0x08B241A0u, {self + 0xA0u});
        ctx.gpr[4] = self + 0x5Cu;
        (void)call_keep(rt, ctx, 0x08C15D30u, {self + 0x5Cu});
        ctx.gpr[4] = self + 0x10u;
        (void)call_keep(rt, ctx, 0x08B28BE0u, {self + 0x10u});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x089C16C8u, {self});
        ctx.gpr[5] = 0x7C0u;
        ctx.gpr[6] = 4u;
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x7C0u, 4u});
}

// func_08A17710(a0, a1): under the lock D_08CF6250 (func_08B344E0 .. func_08B3457C): r = func_08A17698(); the temporary t (sp + 4) is built by func_08B88F44(t, a0), [sp + 8] = a1 (func_08A5BA08(a1) when non-null);
// p = func_08A5BC20(0x10, 4): func_08B88F44(p + 8, t), p[0xC] = [sp + 8] (func_08A5BA08 on it when non-null); func_08B5C078(p, r); the [sp + 8] holder is released (func_08A5B930 when non-null) and t freed (func_08B60804 / func_08B8542C).
// v0 is func_08B3457C's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a1 is never null (func_08A5BA08(a1) always runs), p + 8 is never null (the `beqz a0` at 0x0887777C would skip the member initialisation), and the held pointer [sp + 8] is never null (func_08A5BA08 and func_08A5B930 on it always run); the null forms are read from the asm only (0x08A1775C, 0x08A1777C, 0x08A17790, 0x08A177B0).
void func_08A17710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5];
    const std::uint32_t lock = 0x08CF6250u;
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = 0x08CF0000u;
    ctx.gpr[19] = frame.at(4u);
    ctx.gpr[16] = a0;
    ctx.gpr[4] = lock;
    ctx.gpr[17] = a1;
    (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
    const std::uint32_t r = call_keep(rt, ctx, 0x08A17698u, {lock});
    ctx.gpr[18] = r;
    ctx.gpr[5] = a0;
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08B88F44u, {frame.at(4u), a0});
    put(rt, frame.at(8u), a1);
    ctx.gpr[4] = a1;
    if (a1 != 0u) (void)call_keep(rt, ctx, 0x08A5BA08u, {a1});
    ctx.gpr[5] = 4u;
    const std::uint32_t p = call_keep(rt, ctx, 0x08A5BC20u, {0x10u, 4u});
    ctx.gpr[16] = p;
    ctx.gpr[4] = p + 8u;
    if (p + 8u != 0u) {
        ctx.gpr[5] = frame.at(4u);
        (void)call_keep(rt, ctx, 0x08B88F44u, {p + 8u, frame.at(4u)});
        const std::uint32_t held = word(rt, frame.at(8u));
        ctx.gpr[4] = held;
        put(rt, p + 0xCu, held);
        if (held != 0u) (void)call_keep(rt, ctx, 0x08A5BA08u, {held});
    }
    ctx.gpr[4] = p;
    ctx.gpr[5] = r;
    (void)call_keep(rt, ctx, 0x08B5C078u, {p, r});
    const std::uint32_t held = word(rt, frame.at(8u));
    ctx.gpr[4] = held;
    if (held != 0u) (void)call_keep(rt, ctx, 0x08A5B930u, {held});
    release_string(rt, ctx, frame.at(4u), frame.at(0u));
    ctx.gpr[4] = lock;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
}

// func_0892FCE0(this, a1, lo, hi): r = func_0893656C(func_089363FC(this, a1, lo, hi), a1, lo, hi); r == 0x26 answers 0. Else when this[8] == lo and this[0xC] == hi the answer is 1; otherwise func_0892FBE0(this) and q = func_08937918(func_089363FC(this, 1, lo, hi), 1, lo, hi, sp)
// (a zero q answers 0): this[0x10] = q, this[0x14] = [sp], this[8] = lo, this[0xC] = hi, answer 1.
// Not registered: every case stops at a blocking import (not comparable: sceIoRead).
void func_0892FCE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], lo = ctx.gpr[6], hi = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    ctx.gpr[17] = hi;
    ctx.gpr[16] = lo;
    std::uint32_t result = 0u;
    const std::uint32_t first = call_keep(rt, ctx, 0x089363FCu, {self, a1, lo, hi});
    ctx.gpr[4] = first;
    ctx.gpr[6] = lo;
    ctx.gpr[7] = hi;
    const std::uint32_t code = call_keep(rt, ctx, 0x0893656Cu, {first, a1, lo, hi});
    ctx.gpr[3] = 0x26u;
    ctx.gpr[4] = self;
    if (code == 0x26u) {
        ctx.gpr[5] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, self + 8u);
    ctx.gpr[5] = 1u;
    if (word(rt, self + 8u) == lo) {
        ctx.gpr[2] = word(rt, self + 0xCu);
        if (word(rt, self + 0xCu) == hi) {
            ctx.gpr[2] = 1u;
            return;
        }
    }
    (void)call_keep(rt, ctx, 0x0892FBE0u, {self});
    const std::uint32_t again = call_keep(rt, ctx, 0x089363FCu, {self, 1u, lo, hi});
    ctx.gpr[4] = again;
    ctx.gpr[6] = lo;
    ctx.gpr[7] = hi;
    ctx.gpr[8] = frame.at(0u);
    const std::uint32_t q = call_keep(rt, ctx, 0x08937918u, {again, 1u, lo, hi, frame.at(0u)});
    ctx.gpr[3] = q;
    ctx.gpr[5] = 1u;
    if (q == 0u) {
        ctx.gpr[5] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t extra = word(rt, frame.at(0u));
    ctx.gpr[2] = extra;
    put(rt, self + 0x10u, q);
    put(rt, self + 0x14u, extra);
    put(rt, self + 8u, lo);
    put(rt, self + 0xCu, hi);
    ctx.gpr[2] = 1u;
}

// func_088D3424(this, p, flag, a3): func_088D2F9C(this, p, flag, a3); s2 = (*p)[0x14], s5 = (*p)[0x61C]; this[0x20] (byte) = func_088FE470(s2, a3, this, this + 0xC) when `flag` is non-zero, else
// w = func_088FE388(s2, a3, this, this + 0xC) and, unless w < func_088DA160(*p) (signed), func_088FE3C0(s2, a3, this, this + 0xC); when that byte is non-zero this[4] += 3.0 (float), [sp + 0x30] = 0, [sp + 0x34] = -1 and func_088D2D10(s5, sp, this).
// v0 = this.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): `flag` (a2) is always 0 (func_088FE470 is read from the asm only), func_088FE388 never answers below func_088DA160 (func_088FE3C0 always runs; with a smaller answer the `bnez s0` at 0x088D3498 skips it) and the byte this[0x20] is always 0 afterwards: the final block (this[4] += 3.0, [sp + 0x30] = 0, [sp + 0x34] = -1 and func_088D2D10(s5, sp, this), 0x088D34D0-0x088D34F0) is read from the asm only.
void func_088D3424(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], p = ctx.gpr[5], flag = ctx.gpr[6], a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = p;
    ctx.gpr[20] = a3;
    ctx.gpr[17] = self;
    ctx.gpr[16] = flag;
    (void)call_keep(rt, ctx, 0x088D2F9Cu, {self, p, flag, a3});
    const std::uint32_t object = word(rt, p);
    const std::uint32_t s2 = word(rt, object + 0x14u);
    const std::uint32_t s5 = word(rt, object + 0x61Cu);
    ctx.gpr[18] = s2;
    ctx.gpr[21] = s5;
    ctx.gpr[5] = a3;
    ctx.gpr[6] = self;
    ctx.gpr[7] = self + 0xCu;
    ctx.gpr[4] = s2;
    if (flag != 0u) {
        const std::uint32_t answer = call_keep(rt, ctx, 0x088FE470u, {s2, a3, self, self + 0xCu});
        rt.memory().store8(self + 0x20u, static_cast<std::uint8_t>(answer));
    } else {
        const std::uint32_t first = call_keep(rt, ctx, 0x088FE388u, {s2, a3, self, self + 0xCu});
        ctx.gpr[16] = first;
        ctx.gpr[4] = word(rt, p);
        const std::uint32_t limit = call_keep(rt, ctx, 0x088DA160u, {word(rt, p)});
        const bool less = signed_of(first) < signed_of(limit);
        ctx.gpr[16] = less ? 1u : 0u;
        ctx.gpr[4] = s2;
        ctx.gpr[5] = a3;
        ctx.gpr[6] = self;
        ctx.gpr[7] = self + 0xCu;
        if (!less) {
            const std::uint32_t answer = call_keep(rt, ctx, 0x088FE3C0u, {s2, a3, self, self + 0xCu});
            rt.memory().store8(self + 0x20u, static_cast<std::uint8_t>(answer));
        }
    }
    ctx.gpr[2] = rt.memory().load8(self + 0x20u);
    ctx.gpr[4] = s5;
    ctx.gpr[5] = frame.at(0u);
    ctx.gpr[6] = self;
    if (ctx.gpr[2] != 0u) {
        const float moved = float_at(rt, self + 4u) + fbits(0x40400000u);
        ctx.fpr[1] = fbits(0x40400000u);
        ctx.fpr[0] = moved;
        ctx.gpr[2] = 0xFFFFFFFFu;
        put(rt, frame.at(0x34u), 0xFFFFFFFFu);
        put(rt, frame.at(0x30u), 0u);
        put_float(rt, self + 4u, moved);
        (void)call_keep(rt, ctx, 0x088D2D10u, {s5, frame.at(0u), self});
    }
    ctx.gpr[2] = self;
}

// func_08A29588 / func_08AA6D68 / func_08AB6AD0 (this): a class registration. The holder h (sp) is built by func_08A69B3C(h, text); p = func_08A5BC20(0x38, 4), func_08A7E914(p, h), [sp + 4] = p and func_08A76AB4(this, sp + 4);
// func_08A69D68(h) releases it; then each registration at this[0] (null: func_08A5BB7C(message) runs first): func_08B86FB0(o, name, handler) with o = this[0] read again each time. v0 = this.
inline void class_registration(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t text, std::uint32_t message, const std::uint32_t *names, const std::uint32_t *handlers, unsigned count) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.gpr[5] = text;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {frame.at(0u), text});
    const std::uint32_t p = call_keep(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[16] = p;
    ctx.gpr[5] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A7E914u, {p, frame.at(0u)});
    put(rt, frame.at(4u), p);
    ctx.gpr[5] = frame.at(4u);
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08A76AB4u, {self, frame.at(4u)});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    for (unsigned i = 0; i < count; ++i) {
        const std::uint32_t object = word(rt, self);
        ctx.gpr[16] = object;
        ctx.gpr[4] = message;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {message});
        ctx.gpr[5] = names[i];
        ctx.gpr[6] = handlers[i];
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, 0x08B86FB0u, {object, names[i], handlers[i]});
    }
    ctx.gpr[2] = self;
}

// func_08A29588(this): the text D_08CAB93C, the message D_08CAB950 and the three registrations (D_08CAB958, D_08A29540), (D_08CAB964, D_08A29530), (D_08CAB970, D_08A29538).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[0] is never null (the func_08A5BB7C assertion calls are skipped by the `bnez`).
void func_08A29588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kNames[3] = {0x08CAB958u, 0x08CAB964u, 0x08CAB970u};
    static constexpr std::uint32_t kHandlers[3] = {0x08A29540u, 0x08A29530u, 0x08A29538u};
    class_registration(rt, ctx, 0x08CAB93Cu, 0x08CAB950u, kNames, kHandlers, 3u);
}

// func_08A3D394(this): the text D_08CAD800, the message D_08CAD808 and the three registrations (D_08CAD810, D_08A3D61C), (D_08CAD81C, D_08A3D588), (D_08CAD828, D_08A3D494).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): as func_08A29588: this[0] is never null.
void func_08A3D394(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kNames[3] = {0x08CAD810u, 0x08CAD81Cu, 0x08CAD828u};
    static constexpr std::uint32_t kHandlers[3] = {0x08A3D61Cu, 0x08A3D588u, 0x08A3D494u};
    class_registration(rt, ctx, 0x08CAD800u, 0x08CAD808u, kNames, kHandlers, 3u);
}

// func_08AA6D68(this): the text D_08CB2F60, the message D_08CB2F68 and the two registrations (D_08CB2F70, D_08AA8088), (D_08CB2F78, D_08AA8D20).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): as func_08A29588: this[0] is never null.
void func_08AA6D68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kNames[2] = {0x08CB2F70u, 0x08CB2F78u};
    static constexpr std::uint32_t kHandlers[2] = {0x08AA8088u, 0x08AA8D20u};
    class_registration(rt, ctx, 0x08CB2F60u, 0x08CB2F68u, kNames, kHandlers, 2u);
}

// func_08AB6AD0(this): the text D_08CB41BC, the message D_08CB41C4 and the two registrations (D_08CB41CC, D_08AB6E80), (D_08CB41D8, D_08AB6BA0).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): as func_08A29588: this[0] is never null.
void func_08AB6AD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static constexpr std::uint32_t kNames[2] = {0x08CB41CCu, 0x08CB41D8u};
    static constexpr std::uint32_t kHandlers[2] = {0x08AB6E80u, 0x08AB6BA0u};
    class_registration(rt, ctx, 0x08CB41BCu, 0x08CB41C4u, kNames, kHandlers, 2u);
}

// The release of a reference-counted body whose data pointer is `data` (body = data - 0xC, `shared` the shared empty body): unless it is the shared one, func_08B60804(data - 4, -1) takes a reference away and an answer <= 0 frees it with
// `free_function`(body, free_argument).
inline std::uint32_t release_body(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t shared, std::uint32_t free_function, std::uint32_t free_argument,
                                  unsigned body_register) {
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[body_register] = body;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (body != shared) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = free_argument;
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, free_function, {body, free_argument});
    }
    return body;
}

// func_08BC1FE8(this): s0 = this[0x10], key = this[0xC], lock = s0 + 0x6C, table = s0 + 8: func_08B34888(lock), func_08B2CE7C(table, key), then when func_08B2D1A8(table) equals s0[0x68] also func_08B34CA0(lock), func_08B34B24(lock);
// the tail call is func_08BBEA18(this[0x10] + 0x98, s0).
// Not registered: compared 0 in the fast gate.
void func_08BC1FE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t owner, next_owner;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[20] = self;
        owner = word(rt, self + 0x10u);
        ctx.gpr[16] = owner;
        const std::uint32_t key = word(rt, self + 0xCu);
        ctx.gpr[18] = key;
        const std::uint32_t lock = owner + 0x6Cu;
        ctx.gpr[19] = lock;
        ctx.gpr[4] = lock;
        ctx.gpr[17] = owner + 8u;
        (void)call_keep(rt, ctx, 0x08B34888u, {lock});
        ctx.gpr[5] = key;
        ctx.gpr[4] = owner + 8u;
        (void)call_keep(rt, ctx, 0x08B2CE7Cu, {owner + 8u, key});
        ctx.gpr[4] = owner + 8u;
        const std::uint32_t answer = call_keep(rt, ctx, 0x08B2D1A8u, {owner + 8u});
        ctx.gpr[3] = word(rt, owner + 0x68u);
        ctx.gpr[4] = lock;
        if (answer == ctx.gpr[3]) (void)call_keep(rt, ctx, 0x08B34CA0u, {lock});
        ctx.gpr[4] = lock;
        (void)call_keep(rt, ctx, 0x08B34B24u, {lock});
        next_owner = word(rt, self + 0x10u);
        ctx.gpr[4] = next_owner;
        ctx.gpr[5] = owner;
        ctx.gpr[4] = next_owner + 0x98u;
    }
    tail_call(ctx, 0x08BBEA18u, {next_owner + 0x98u, owner});
}

// func_08BBF734(first, last): the 20-byte elements [first, last) are destroyed in turn (nothing for an empty range): func_08BBEBE4(e[4], e[8], sp + 1), func_08B35254(e[4]) when e[4] is non-zero, and the string body of e[0]
// (shared empty body D_08D75298: func_08B60804(p - 4, -1), an answer <= 0 frees it with func_08B93554(body, sp)).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): e[4] is never null (func_08B35254 always runs; the `beqz v0` at 0x08BBF77C would skip it), the string body is never the shared empty one (the `beq s1, s4` at 0x08BBF79C would skip the release) and the refcount call always answers <= 0 (the body is always freed; the `bgtz v0` at 0x08BBF7B0 would skip the free).
void func_08BBF734(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = last;
    ctx.gpr[16] = first;
    if (first == last) return;
    ctx.gpr[20] = 0x08D75298u;
    ctx.gpr[19] = frame.at(1u);
    std::uint32_t element = first;
    for (;;) {
        ctx.gpr[16] = element;
        ctx.gpr[4] = word(rt, element + 4u);
        ctx.gpr[5] = word(rt, element + 8u);
        ctx.gpr[6] = frame.at(1u);
        (void)call_keep(rt, ctx, 0x08BBEBE4u, {word(rt, element + 4u), word(rt, element + 8u), frame.at(1u)});
        ctx.gpr[2] = word(rt, element + 4u);
        ctx.gpr[4] = ctx.gpr[2];
        if (ctx.gpr[2] != 0u) (void)call_keep(rt, ctx, 0x08B35254u, {ctx.gpr[2]});
        const std::uint32_t data = word(rt, element);
        ctx.gpr[2] = data;
        element += 0x14u;
        ctx.gpr[16] = element;
        ctx.gpr[17] = release_body(rt, ctx, data, 0x08D75298u, 0x08B93554u, frame.at(0u), 17);
        if (element == last) break;
    }
}

// func_08B95120(a0, node): the destroyer of a tree with two string members: for each node n on the chain (n[8] the next, n[0xC] the left subtree): func_08B95120(a0, n[0xC]), then the bodies of n[0x14] (with func_08B93554(body, sp + 1)) and of n[0x10]
// (with func_08B93554(body, sp)) are released unless shared (D_08D75298), then func_08B35254(n). A null node returns at once.
// Not registered: compared 0 in the fast gate.
void func_08B95120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    if (node == 0u) return;
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = a0;
    ctx.gpr[21] = 0x08D75298u;
    for (;;) {
        ctx.gpr[16] = node;
        ctx.gpr[5] = word(rt, node + 0xCu);
        ctx.gpr[4] = a0;
        ctx.gpr[18] = node + 0x10u;
        (void)call_keep(rt, ctx, 0x08B95120u, {a0, word(rt, node + 0xCu)});
        const std::uint32_t second = word(rt, node + 0x14u);
        const std::uint32_t next = word(rt, node + 8u);
        ctx.gpr[2] = second;
        ctx.gpr[19] = next;
        ctx.gpr[17] = release_body(rt, ctx, second, 0x08D75298u, 0x08B93554u, frame.at(1u), 17);
        const std::uint32_t first = word(rt, node + 0x10u);
        ctx.gpr[2] = first;
        ctx.gpr[17] = release_body(rt, ctx, first, 0x08D75298u, 0x08B93554u, frame.at(0u), 17);
        ctx.gpr[4] = node;
        (void)call_keep(rt, ctx, 0x08B35254u, {node});
        ctx.gpr[16] = next;
        if (next == 0u) break;
        node = next;
    }
}

// func_08B043D8(this): o = this[0]: the halfwords o[0x98], o[0x9A], o[0x1C], o[0x1E], o[0xC], o[0xE], o[0x50], o[0x52], o[0x6C], o[0x6E], o[0x88], o[0x8A] = -1 and the words o[0x9C], o[0x20], o[0x10], o[0x54], o[0x70], o[0x8C] = 0, around the vector erases
// func_08C49930(o + 0x2C, o[0x2C], o[0x30]), func_08C49DC0(o + 0x38, o[0x38], o[0x3C]), func_08C49A28(o, o[0], o[4]), func_08C49B00(o + 0x44, o[0x44], o[0x48]), func_08C49C28(o + 0x60, o[0x60], o[0x64]);
// the tail call is func_08C49D08(o + 0x7C, o[0x7C], o[0x80]).
// Branches: none (a straight line).
// Not registered: no compared case changed memory (its stores wrote the values already there), so they are unverified.
void func_08B043D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t this_ref = ctx.gpr[4];
    std::uint32_t o;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = 0xFFFFFFFFu;
        o = word(rt, this_ref);
        ctx.gpr[16] = o;
        ctx.gpr[3] = o + 0x1Cu;
        ctx.gpr[2] = o + 0x98u;
        rt.memory().store16(o + 0x9Au, 0xFFFFu);
        put(rt, o + 0x9Cu, 0u);
        rt.memory().store16(o + 0x98u, 0xFFFFu);
        rt.memory().store16(o + 0x1Cu, 0xFFFFu);
        rt.memory().store16(o + 0x1Eu, 0xFFFFu);
        put(rt, o + 0x20u, 0u);
        ctx.gpr[4] = o + 0x2Cu;
        ctx.gpr[6] = word(rt, o + 0x30u);
        ctx.gpr[5] = word(rt, o + 0x2Cu);
        (void)call_keep(rt, ctx, 0x08C49930u, {o + 0x2Cu, ctx.gpr[5], ctx.gpr[6]});
        ctx.gpr[4] = o + 0x38u;
        ctx.gpr[6] = word(rt, o + 0x3Cu);
        ctx.gpr[5] = word(rt, o + 0x38u);
        (void)call_keep(rt, ctx, 0x08C49DC0u, {o + 0x38u, ctx.gpr[5], ctx.gpr[6]});
        ctx.gpr[3] = o + 0xCu;
        rt.memory().store16(o + 0xEu, 0xFFFFu);
        ctx.gpr[4] = o;
        put(rt, o + 0x10u, 0u);
        ctx.gpr[5] = word(rt, o);
        ctx.gpr[6] = word(rt, o + 4u);
        rt.memory().store16(o + 0xCu, 0xFFFFu);
        (void)call_keep(rt, ctx, 0x08C49A28u, {o, ctx.gpr[5], ctx.gpr[6]});
        ctx.gpr[3] = o + 0x50u;
        rt.memory().store16(o + 0x52u, 0xFFFFu);
        ctx.gpr[4] = o + 0x44u;
        put(rt, o + 0x54u, 0u);
        rt.memory().store16(o + 0x50u, 0xFFFFu);
        ctx.gpr[6] = word(rt, o + 0x48u);
        ctx.gpr[5] = word(rt, o + 0x44u);
        (void)call_keep(rt, ctx, 0x08C49B00u, {o + 0x44u, ctx.gpr[5], ctx.gpr[6]});
        ctx.gpr[3] = o + 0x6Cu;
        rt.memory().store16(o + 0x6Eu, 0xFFFFu);
        ctx.gpr[4] = o + 0x60u;
        put(rt, o + 0x70u, 0u);
        rt.memory().store16(o + 0x6Cu, 0xFFFFu);
        ctx.gpr[6] = word(rt, o + 0x64u);
        ctx.gpr[5] = word(rt, o + 0x60u);
        (void)call_keep(rt, ctx, 0x08C49C28u, {o + 0x60u, ctx.gpr[5], ctx.gpr[6]});
        ctx.gpr[3] = o + 0x88u;
        rt.memory().store16(o + 0x8Au, 0xFFFFu);
        ctx.gpr[4] = o + 0x7Cu;
        put(rt, o + 0x8Cu, 0u);
        rt.memory().store16(o + 0x88u, 0xFFFFu);
        ctx.gpr[5] = word(rt, o + 0x7Cu);
        ctx.gpr[6] = word(rt, o + 0x80u);
    }
    tail_call(ctx, 0x08C49D08u, {o + 0x7Cu, word(rt, o + 0x7Cu), word(rt, o + 0x80u)});
}

// func_0886E7A8(this, a1, a2): func_0886AD04(this, a1, a2); this[0] = D_08CBF770; p = func_08B62344(0x3C): func_08871908(p), func_0886FC04(p + 0x14), func_088744FC(p + 0x28), func_0886F8B4(p + 0x14, 1); this[0x70] = p;
// the guarded static D_08D752E0 (guard byte, func_08B6441C / func_08B64494) holds D_08D752E8 (set to D_08CBF7A8 on the first call); this[0x74] = D_08D752E8; the tail call is func_0886AAA4(this, this[0x70]).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08B6441C always answers non-zero when the guard byte D_08D752E0 is 0 (the answer-0 path at 0x0886E83C skips the holder initialisation).
void func_0886E7A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    const std::uint32_t guard = 0x08D752E0u, holder = 0x08D752E8u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self;
        (void)call_keep(rt, ctx, 0x0886AD04u, {self, a1, a2});
        ctx.gpr[2] = 0x08CBF770u;
        ctx.gpr[4] = 0x3Cu;
        put(rt, self, 0x08CBF770u);
        const std::uint32_t p = call_keep(rt, ctx, 0x08B62344u, {0x3Cu});
        ctx.gpr[17] = p + 0x14u;
        ctx.gpr[4] = p;
        ctx.gpr[16] = p;
        (void)call_keep(rt, ctx, 0x08871908u, {p});
        ctx.gpr[4] = p + 0x14u;
        (void)call_keep(rt, ctx, 0x0886FC04u, {p + 0x14u});
        ctx.gpr[4] = p + 0x28u;
        (void)call_keep(rt, ctx, 0x088744FCu, {p + 0x28u});
        ctx.gpr[4] = p + 0x14u;
        ctx.gpr[5] = 1u;
        (void)call_keep(rt, ctx, 0x0886F8B4u, {p + 0x14u, 1u});
        put(rt, self + 0x70u, p);
        ctx.gpr[17] = guard;
        ctx.gpr[19] = holder;
        ctx.gpr[5] = static_cast<std::uint32_t>(byte_at(rt, guard));
        ctx.gpr[4] = guard;
        ctx.gpr[16] = self + 0x70u;
        if (ctx.gpr[5] == 0u) {
            const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
            ctx.gpr[4] = guard;
            if (acquired != 0u) {
                put(rt, holder, 0x08CBF7A8u);
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            }
        }
        ctx.gpr[5] = word(rt, self + 0x70u);
        put(rt, self + 0x74u, holder);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x0886AAA4u, {self, word(rt, self + 0x70u)});
}

// func_08900350(o): a float leaf: when o[0x41C] < 0.0 then o[0x41C] = o[0x414] and o[0x420] = o[0x418]; when o[4] == 1.0 then o[4] = 1.3 (0x3FA66666); when o[0x49C] == 100.0, o[0x4A0] == 1000.0 and o[0x4A4] == 0.0 (all three, in that order)
// then o[0x4A0] = o[0xC], o[0x49C] = o[0x30], o[0x4A4] = o[0x38] and o[0x34] = o[0xC]. v0 stays as the caller had it; f0 / f1 / f2 as the path leaves them.
// Not registered: compared only on the path where o[0x49C] != 100.0 (the early return after the first two tests); the stores after the three equalities are unverified.
void func_08900350(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    float f0 = float_at(rt, o + 0x41Cu);
    const float zero = 0.0f;
    float f1 = 0.0f, f2 = zero;
    auto finish = [&]() {
        ctx.fpr[0] = f0;
        ctx.fpr[1] = f1;
        ctx.fpr[2] = f2;
    };
    if (f0 < f2) {
        f0 = float_at(rt, o + 0x414u);
        f1 = float_at(rt, o + 0x418u);
        put_float(rt, o + 0x41Cu, f0);
        put_float(rt, o + 0x420u, f1);
    }
    f1 = float_at(rt, o + 4u);
    f0 = 1.0f;
    if (f1 == f0) {
        f0 = fbits(0x3FA66666u);
        put_float(rt, o + 4u, f0);
    }
    f1 = float_at(rt, o + 0x49Cu);
    f0 = fbits(0x42C80000u);
    if (!(f1 == f0)) return finish();
    f1 = float_at(rt, o + 0x4A0u);
    f0 = fbits(0x447A0000u);
    if (!(f1 == f0)) return finish();
    f0 = float_at(rt, o + 0x4A4u);
    if (!(f0 == f2)) return finish();
    f0 = float_at(rt, o + 0xCu);
    f1 = float_at(rt, o + 0x30u);
    f2 = float_at(rt, o + 0x38u);
    put_float(rt, o + 0x4A0u, f0);
    put_float(rt, o + 0x49Cu, f1);
    put_float(rt, o + 0x4A4u, f2);
    put_float(rt, o + 0x34u, f0);
    finish();
}

// func_08827E7C / func_08827F50 (out): the holder h (sp + 4) is filled by func_088248E4(h); o = h[0] (null: func_08A5BB7C(D_08C99B4C) runs first), s = o[0x14]; func_08A5B9EC(h); r = `convert`(s); func_08824774(sp + 8);
// q = word at sp + 8 (null: func_08A5BB7C(D_08C9A1A8) runs first); func_08A52AB8(sp, q, r, -1, 0, 0); unless `out` is sp func_08A5BAD8(out, [sp]) copies; func_08A5B9EC(sp) and func_08A5B9EC(sp + 8) end it. v0 is the last call's.
inline void convert_and_copy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t convert) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = frame.at(4u);
    ctx.gpr[18] = out;
    ctx.gpr[4] = frame.at(4u);
    ctx.gpr[19] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x088248E4u, {frame.at(4u)});
    const std::uint32_t holder_object = word(rt, frame.at(4u));
    ctx.gpr[16] = holder_object;
    ctx.gpr[4] = 0x08C99B4Cu;
    if (holder_object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    const std::uint32_t source = word(rt, holder_object + 0x14u);
    ctx.gpr[16] = source;
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[4] = source;
    const std::uint32_t converted = call_keep(rt, ctx, convert, {source});
    ctx.gpr[4] = frame.at(8u);
    ctx.gpr[17] = converted;
    (void)call_keep(rt, ctx, 0x08824774u, {frame.at(8u)});
    const std::uint32_t target = word(rt, frame.at(8u));
    ctx.gpr[16] = target;
    ctx.gpr[4] = 0x08C9A1A8u;
    if (target == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9A1A8u});
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = target;
    ctx.gpr[6] = converted;
    ctx.gpr[7] = 0xFFFFFFFFu;
    ctx.gpr[8] = 0u;
    ctx.gpr[9] = 0u;
    (void)call_keep(rt, ctx, 0x08A52AB8u, {frame.at(0u), target, converted, 0xFFFFFFFFu, 0u, 0u});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[4] = frame.at(8u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
}

// func_08827E7C(out): convert_and_copy with func_0885C380.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the holder object and the object at sp + 8 are never null (the func_08A5BB7C assertion calls are skipped by the `bnez` at 0x08827EB0 and 0x08827EE8) and `out` is never sp (the copy func_08A5BAD8 always runs; `beq s2, sp` at 0x08827F14 would skip it).
void func_08827E7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_and_copy(rt, ctx, 0x0885C380u); }

// func_08827F50(out): convert_and_copy with func_0885C38C.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): as func_08827E7C (the same words with func_0885C38C): the null checks are not hit and `out` is never sp.
void func_08827F50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_and_copy(rt, ctx, 0x0885C38Cu); }

}  // namespace
}  // namespace hot_batch_86
}  // namespace gt::game

#define HOT86(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_86::name, gt::ReturnKind::kind)
#define HOT86N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_86::name, gt::ReturnKind::kind)
HOT86(0x088742C8u, func_088742C8, Word);
HOT86(0x08882DC0u, func_08882DC0, Word);
// Not registered (compared 0 in the fast gate): HOT86(0x088733F4u, func_088733F4, Word);
// Not registered (compared 0 in the fast gate): HOT86(0x088A34BCu, func_088A34BC, Word);
HOT86(0x0887104Cu, func_0887104C, Word);
// Not registered (compared 0 in the fast gate): HOT86(0x0889844Cu, func_0889844C, Word);
HOT86(0x0886E980u, func_0886E980, Word);
HOT86(0x08A83E98u, func_08A83E98, Word);
HOT86(0x08BEBECCu, func_08BEBECC, Word);
// Not registered (compared 0 in the fast gate): HOT86(0x08C34270u, func_08C34270, Word);
HOT86(0x08B03F10u, func_08B03F10, Word);
HOT86(0x08869444u, func_08869444, Word);
HOT86(0x0886968Cu, func_0886968C, Word);
// Not registered (no compared case changed memory in 22097 calls' samples): HOT86(0x08872E18u, func_08872E18, Word);
HOT86(0x08C23080u, func_08C23080, Word);
HOT86(0x089E18BCu, func_089E18BC, Word);
HOT86(0x089E19BCu, func_089E19BC, Word);
HOT86(0x0886B600u, func_0886B600, Word);
// Not registered (its one call per menu route destroys the whole member tree: its cases swallow the compared calls of ~56 already decompiled descendants and of the destructors of this batch, so it is left recompiled to keep them compared): HOT86(0x0880BEF8u, func_0880BEF8, Word);
HOT86(0x0884CC18u, func_0884CC18, Word);
HOT86(0x0884CD28u, func_0884CD28, Word);
HOT86(0x0886A804u, func_0886A804, Word);
// Not registered (every case stops at a blocking import (not comparable: sceIoRead)): HOT86(0x088A5E5Cu, func_088A5E5C, Word);
HOT86(0x088D9BBCu, func_088D9BBC, Word);
HOT86(0x08944100u, func_08944100, Word);
HOT86(0x08A96B50u, func_08A96B50, Word);
HOT86(0x08A042F4u, func_08A042F4, Word);
HOT86(0x0881B758u, func_0881B758, Word);
HOT86(0x0886AD04u, func_0886AD04, Word);
HOT86N(0x0887C7A0u, func_0887C7A0, Word);
HOT86(0x089CD2C8u, func_089CD2C8, Word);
HOT86(0x08B712B0u, func_08B712B0, DoubleWord);
// Not registered (its only caller case, func_08B043D8, is unregistered; compared 0 and no memory change): HOT86N(0x08C49930u, func_08C49930, Word);
HOT86(0x088F5670u, func_088F5670, Word);
HOT86(0x08ABEA40u, func_08ABEA40, Word);
HOT86(0x08AE7E28u, func_08AE7E28, Word);
// Not registered (every compared case has count 0): HOT86(0x08BE78E8u, func_08BE78E8, Word);
HOT86(0x08892A7Cu, func_08892A7C, Word);
HOT86(0x0887DE44u, func_0887DE44, Word);
HOT86(0x08894574u, func_08894574, Word);
HOT86(0x088FEA94u, func_088FEA94, Word);
HOT86(0x08AF464Cu, func_08AF464C, Word);
// Not registered (every case would sleep (not comparable)): HOT86(0x08AC8FDCu, func_08AC8FDC, Word);
// Not registered (compared only on the empty range): HOT86(0x08C49A28u, func_08C49A28, Word);
// Not registered (compared only on the empty range): HOT86N(0x08C49D08u, func_08C49D08, Word);
// Not registered (compared only on the empty range): HOT86(0x08B89338u, func_08B89338, Word);
HOT86(0x08B9AC68u, func_08B9AC68, Word);
// Not registered (compared 0 in the fast gate): HOT86(0x08B95080u, func_08B95080, Word);
// Not registered (compared only on the first attempt: found at the starting position): HOT86(0x08C246E4u, func_08C246E4, Word);
// Not registered (compared only on the empty range): HOT86(0x08C49C28u, func_08C49C28, Word);
// Not registered (compared only on the empty range): HOT86(0x08B895E0u, func_08B895E0, Word);
// Not registered (compared only on the empty range): HOT86(0x08C0DBBCu, func_08C0DBBC, Word);
HOT86(0x08B8AA60u, func_08B8AA60, Word);
HOT86(0x0898DF5Cu, func_0898DF5C, Word);
HOT86(0x089AF5E8u, func_089AF5E8, Word);
HOT86(0x08A6BBE8u, func_08A6BBE8, Word);
HOT86(0x0899CD50u, func_0899CD50, Word);
HOT86(0x0897FDE0u, func_0897FDE0, Word);
HOT86(0x08A777CCu, func_08A777CC, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT86(0x08A13844u, func_08A13844, Word);
HOT86(0x08A17710u, func_08A17710, Word);
// Not registered (every case stops at a blocking import (not comparable: sceIoRead)): HOT86(0x0892FCE0u, func_0892FCE0, Word);
HOT86(0x088D3424u, func_088D3424, Word);
HOT86(0x08A29588u, func_08A29588, Word);
HOT86(0x08A3D394u, func_08A3D394, Word);
HOT86(0x08AA6D68u, func_08AA6D68, Word);
HOT86(0x08AB6AD0u, func_08AB6AD0, Word);
// Not registered (compared 0 in the fast gate): HOT86(0x08BC1FE8u, func_08BC1FE8, Word);
HOT86(0x08BBF734u, func_08BBF734, Word);
// Not registered (compared 0 in the fast gate): HOT86(0x08B95120u, func_08B95120, Word);
// Not registered (no compared case changed memory: its stores wrote the values already there): HOT86(0x08B043D8u, func_08B043D8, Word);
HOT86(0x0886E7A8u, func_0886E7A8, Word);
// Not registered (compared only on the early return after the first two tests): HOT86(0x08900350u, func_08900350, Word);
HOT86(0x08827E7Cu, func_08827E7C, Word);
HOT86(0x08827F50u, func_08827F50, Word);
