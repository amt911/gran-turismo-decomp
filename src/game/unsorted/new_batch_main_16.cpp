// Main-session batch 16: functions the recompiler emitted and no `src/` file has taken (the batch list handed to this
// session was already decompiled on main, so this batch is the pending pool of 192-216-byte functions instead).
// Written by hand from `tools.disasm`, structured C: named locals, `G[n]` only at the call boundary, v0 as the original
// leaves it on every path. Role words in the comments describe access shapes; none of these is a game name.
//
// Conventions used throughout (the same the hot batches use): `call_set` writes the argument registers the original sets
// before the `jal` and keeps v0/v1/f0 of the callee; a `jal`'s delay slot runs BEFORE the call; a branch delay slot runs
// on the path that takes the branch (and, for a plain branch, on both paths); `lui` leftovers are `lui_of(address)`, the
// value of the relocated pair's high half, which is what the register holds when no addiu follows it.
//
// Audits: frame_audit 0 wrong, literal_check 0 missing. sp_loads lists func_0882689C's `lw $s0, 0x4($sp)`: that is the
// temporary object func_088248E4 builds in the frame (read here with `word(rt, frame.at(4u))`), not a callee-saved reload.
// arg_reads lists func_08866830 (t2) and func_088975D0 (t1, t2, t3) in func_088662A8: both callees load those registers
// (lwl/lwr) before reading them, so the call passing only the arguments the original sets is exact.
//
// Not written here (read, left for another pass): 0x0886A3A4 and 0x08BA8AA8 take reference counts with ll/sc, whose
// loop the host's memory model does not model; the rest of the 192-216-byte pending pool was not reached in this pass.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_16 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The value the relocated high half of a `lui` loads for `address` (what the register holds when no addiu follows).
inline constexpr u32 lui_of(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next
// instruction sees, and a callee that sets no v0 hands the previous one on (call_guest starts it with the caller's).
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    take(ctx, result);
    return result;
}
// A guest call with a0-a3, t0-t3 set like the original sets them before the jal: what the callee does not take stays as
// the caller had it.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original.
inline GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot,
                                std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_keep(rt, ctx, ctx.gpr[2], arguments);
}
// A function-local static built once (`func_08B6441C` is the guard's acquire, `func_08B64494` its release): a guard byte
// that is non-zero once built, the object at `storage`, whose vtable is `vtable`. v0 ends as the original leaves it: the
// high half of the pair when the guard was already set, the release's answer when this call built it.
constexpr u32 kGuardAcquire = 0x08B6441Cu;
constexpr u32 kGuardRelease = 0x08B64494u;
inline u32 singleton_once(Runtime &rt, AllegrexContext &ctx, u32 guard, u32 storage, u32 vtable) {
    ctx.gpr[2] = lui_of(storage);
    if (sext8(byte_u(rt, guard)) != 0u) return ctx.gpr[2];
    if (call_set(rt, ctx, kGuardAcquire, {guard}) != 0u) {
        put(rt, storage, vtable);
        (void)call_set(rt, ctx, kGuardRelease, {guard});
    }
    return ctx.gpr[2];
}

// The copy-on-write string release: the data pointer `text` has its rep 0xC bytes before it and its count at -4; unless the
// rep is the empty rep 0x08D75298, func_08B60804(&count, -1) steps the count and func_08B93554(rep, frame) frees it when the
// old count was not positive. v0 ends as `text` (the original loads the slot into v0 before comparing the rep).
inline void release_shared_string(Runtime &rt, AllegrexContext &ctx, u32 text, u32 frame_base) {
    const u32 rep = text - 0xCu;
    ctx.gpr[2] = text;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == 0x08D75298u) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    (void)call_set(rt, ctx, 0x08B93554u, {rep, frame_base});
}

// Two 0x30-byte elements are destroyed by func_088978B4, the first at `address` and the second 0x30 bytes below it (the
// original walks a pointer down by 0x30 between the calls).
inline void destroy_two(Runtime &rt, AllegrexContext &ctx, u32 address) {
    for (u32 count = 2u;;) {
        --count;
        (void)call_set(rt, ctx, 0x088978B4u, {address});
        if (count == 0u) break;
        address -= 0x30u;
    }
}

// Callee-saved registers: the original saves them in its frame and restores them before it returns.
struct Saved {
    AllegrexContext &ctx;
    u32 gpr[8]{};
    explicit Saved(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) gpr[i] = c.gpr[16u + i];
    }
    ~Saved() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = gpr[i];
    }
};
// 0x0880B34C (a0 = object): a constructor: base init, two vtables, then a 0x1C-byte node whose head is zeroed and whose
// four words at +0xC.. are cleared, stored at +0xE8; then the +0xEC slot gets the function-local static (guard
// 0x08D74FF8, storage 0x08D75000, vtable 0x08CBCBD0).

// 0x0882689C (a0 = destination value, a1 unused, a2 = count, a3 = pointer to the source object): count <= 0 does nothing
// (v0 untouched). Otherwise builds a value object at sp+4 (func_088248E4), takes its word at +0x14 (the member, read
// before the temporary is destroyed by func_08A5B9EC), asserts a non-null source (func_08A5BB7C(0x08C99B4C / 0x08C99B24)),
// calls the source's virtual +0x3C, func_0885BBF0(member, that) and builds the answer at sp with func_08A85B1C; when the
// destination is not the temporary itself it is assigned (func_08A5BAD8) and the temporary destroyed.

// 0x088452D4 (a0 = object, a1 = argument): base init (func_08A76ACC), vtable 0x08CBD788, an 8-byte node from
// func_08B62344 initialised by func_08893F7C and func_08893C40(node, argument) stored at +0x14, then the +0x18 slot gets
// the function-local static (guard 0x08D751A0, storage 0x08D751A8, vtable 0x08CBEC38).
// 0x08857B24 (a0 = object, a1 = argument, a2 = a byte): the same constructor shape as 0x088452D4: base init, vtable
// 0x08CBD238, an 8-byte node initialised by func_088A7B3C(node, argument, byte) at +0x14, and the function-local static
// (guard 0x08D751E0, storage 0x08D751E8, vtable 0x08CBF2D8) at +0x18.

// 0x0885D2C8 (a0 = object): a destructor: func_088A7548 over the +0x120 member, then three runs of two 0x30-byte
// elements destroyed each by func_088978B4 (at +0x158/+0x128, +0xE8/+0xB8 and +0x88/+0x58), and the frame is popped for a
// tail call of func_088A7548 on the +0x50 member.

// 0x088662A8 (a0 = object, a1 = argument, a2 = argument): func_08B36018(object, 0, argument), then under the lock
// 0x08CE2DCC (func_08B344E0 / func_08B3457C) func_08866830(argument, object, func_088666E8()) is built into a value object
// at sp (func_088A79AC) by func_088A6AB4, and func_088975D0(func_08897B8C(sp+8) after func_08897574) is called twice on it;
// the temporaries are destroyed (func_088978B4, func_088A7548) and v0 is the last call's.
// 0x0886F450 (a0 = object): two shared strings (the +0x130 and +0x134 members of the object at +0x70) are released:
// func_08B958A8 fills a slot with each rep, then the copy-on-write release runs (rep compared with the empty rep
// 0x08D75298, func_08B60804 steps the count, func_08B93554 frees).
void func_0886F450(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(8u), 0x08D75298u + 0xCu);
    (void)call_set(rt, ctx, 0x08B958A8u, {frame.at(8u), word(rt, self + 0x70u) + 0x130u});
    release_shared_string(rt, ctx, word(rt, frame.at(8u)), frame.at(1u));
    put(rt, frame.at(4u), 0x08D75298u + 0xCu);
    (void)call_set(rt, ctx, 0x08B958A8u, {frame.at(4u), word(rt, self + 0x70u) + 0x134u});
    release_shared_string(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
}

// 0x08875A18 (a0 = object, a1 unused, a2 = argument): builds the string-like temporary at sp+4 (func_08AB85F0) and fills
// it with func_088755E4(object, sp+0x14, temp, 4); when its count at +8 is 3 or more, func_0887C914(func_0886AFA8(*object) + 4,
// argument, sp+0x14) runs; the temporary is released last.

// 0x0887EA84 (a0 = object): base init (func_0886AD04), vtable 0x08CC10C8, a 0x14-byte node from func_08B62344 initialised
// by func_08881248 at +0x70, the +0x74 slot gets the function-local static (guard 0x08D75DE8, storage 0x08D75DF0, vtable
// 0x08CC1100), and the frame is popped for a tail call of func_0886AAA4(object, that node).

// 0x088844E0 (a0 = object): the same shape: base init (func_0887E1E0), vtable 0x08CC1460, a 0x1980-byte node from
// func_08B62344 initialised by func_08884314 at +0xC, the +0x10 slot gets the function-local static (guard 0x08D75E38,
// storage 0x08D75E40, vtable 0x08CC14C0), then a tail call of func_088824F8(object, 0).

// 0x0888A490 (a0 = holder, a1 = source, a2 = object): func_08889D90(*holder + 8); when the object and its virtual +0x68 and
// the source's word at +0x10 are all non-zero, func_08B1BEEC(*holder + 0x100, that word) and func_0888A2A8(*holder + 0x10 +
// index * 0x50, index) with the index read at *holder + 0x104, then the frame is popped for a tail call of
// func_08B1C324(*holder + 0x100, index). Any of the three zero ends the function right there.

// 0x0889436C (a0 = out pair, a1 = pointer to the source record): allocates a 0x24-byte record, zeroes its +0x1C, stores it
// at *out, sets the +0x4 slot to the function-local static (guard 0x08D75EC8, storage 0x08D75ED0, vtable 0x08CC1DD8), then
// copies the source's 0x24 bytes into the record; v0 ends as the last word copied (the source's +0x20).

// 0x0889D984 (a0 = out pair, a1 = count): allocates a 0x34-byte object, func_08B28DCC(object), count at +0x2C and zero at
// +0x28 and +0x30, then under the object's own lock (func_08B344E0 / func_08B3457C) the count * 8 bytes are allocated by
// func_08B6461C into +0x28; the object goes to *out and the +0x4 slot gets the function-local static (guard 0x08D75EF8,
// storage 0x08D75F00, vtable 0x08CC23F0).
void func_0889D984(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 object = call_set(rt, ctx, 0x08B62344u, {0x34u});
    (void)call_set(rt, ctx, 0x08B28DCCu, {object});
    put(rt, object + 0x2Cu, count);
    put(rt, object + 0x28u, 0u);
    (void)call_set(rt, ctx, 0x08B344E0u, {object});
    put(rt, object + 0x30u, 0u);
    const u32 entries = word(rt, object + 0x2Cu);
    if (entries != 0u) {
        const u32 allocated = call_set(rt, ctx, 0x08B6461Cu, {entries << 3u});
        put(rt, object + 0x28u, allocated);
    }
    (void)call_set(rt, ctx, 0x08B3457Cu, {object});
    put(rt, out, object);
    (void)singleton_once(rt, ctx, 0x08D75EF8u, 0x08D75F00u, 0x08CC23F0u);
    put(rt, out + 4u, 0x08D75F00u);
}

// 0x088A2FA4 (a0 = object): a destructor: vtable 0x08CC27E8, frees the member at +0xC (func_08B61FC0), runs the virtual at
// +4 of the member at +8 when set, then destroys the 0x7C-byte elements of the array at +4 (count at base - 4, each through
// the first vtable slot, last element first; the stored base is re-read after each one), frees it (base - 4) and finally
// stores vtable 0x08CC2990.

// 0x088A78D4 (a0 = out pair, a1 = holder, a2 = flag byte): a 0x4D8-byte object built by func_08B2B3EC and func_088A73E4
// (vtable 0x08CC2BE8) when the flag is set, otherwise by func_08B62344 and func_088A7310; it goes to *out, the +0x4 slot gets
// the function-local static (guard 0x08D75F28, storage 0x08D75F30, vtable 0x08CC2C00) and the frame is popped for a tail call
// of func_088A5450(*out, *holder).

// 0x088A7A74 (a0 = out pair, a1 = flag byte): the same object as 0x088A78D4 (0x4D8 bytes, either func_08B2B3EC +
// func_088A73E4 with vtable 0x08CC2BE8 or func_08B62344 + func_088A7310), the function-local static at +0x4 (guard
// 0x08D75F28, storage 0x08D75F30, vtable 0x08CC2C00), then a tail call of func_088A719C(out).

// 0x088ABBE4 (a0 = object): a constructor: vtable 0x08CC2ED8, two spin delays the compiler emitted as self-loops, 1.0f at
// +0x3C4 and 2.0f at +0x3C8, zeros at +0x3CC, +0x3D4, +0x3D8, +0x3DC, +0x3E8, +0x3EC, +0x3F8, +0x3FC and +0x418 and the
// bytes +0x438..+0x43D and +0x43F, func_08B36018(object + 4, 0xBD, 0x48), and a tail call of func_08B36018(object + 0x4C,
// 0xBD, 0x48).
} // namespace
} // namespace new_batch_main_16
} // namespace gt::game

#define MAIN16(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_16::name, gt::ReturnKind::kind)
MAIN16(0x0886F450u, func_0886F450, Word);
MAIN16(0x0889D984u, func_0889D984, Word);
