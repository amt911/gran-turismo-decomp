// New batch 2019: hand-written structured C from `tools.disasm`, at the addresses the batch-1930 draft
// (commit 5e393ceb, new_batch_main_1930.cpp) mismatched. Draft only — nothing here is gate-verified yet; the
// input shapes live in /home/andres/repos/gt-ops/shapes/shapes_main2019.txt.
//
// v0 on every exit path is the asm's v0, not "whatever the last callee returned": a jal into a function that
// never writes v0 leaves the caller's v0 in place (0x0895E63C is a field-clearing leaf with no v0 store), and
// `lui v0, %hi(...)` leaves the high half there until the load that follows overwrites it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2019 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 unsigned_byte(Runtime &rt, u32 a) { return rt.memory().load8(a); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B05084 (out = a0, size = a1, a = a2, b = a3): builds the 0x120-byte object o = 0x08B62344(0x120), the twin of
// 0x08B04D98 (this body is its instructions word for word; only one relocated `j` differs). Writes o[0] = o and
// o[4] = the address of a function-local static, sets the tuning floats and the count 5, and goes on in the tail call
// 0x08B039F8(o, size or 0x60, 0x10) -> 0x08B05DC4(o + 0xC8, ...). Frame 0x40.
// NOT REGISTERED: same body as 0x08B04D98, which mismatched 11 of 11 compared cases because 0x08B05DC4 grows the
// vector at o + 0xC8 with records copied from a dead stack slot of its own frame (FINDINGS: "A function whose tail
// callee reads uninitialised stack cannot be proven"); the batch-1930 gate saw the same on this address. Kept for
// when that class is solvable.

// ---------------------------------------------------------------------------------------------------------------------
// 0x08929104 (a0 = the object, "fp"): the big constructor. Runs the eighteen sub-initialisers, writes their vtable-like
// pointers, then fills the 0x1F70.. blocks: the member pointers of the five sub-objects, the flags, the two constants
// 11.0f / 50.0f of the block at +0x2058, and the two 16-byte zero records after +0x2184. v0 = 3 on exit (the `addiu
// v0, 3` before the writes); 2 is only the value stored at +0x1FD0. Frame 0x40.
void func_08929104(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 fp = ctx.gpr[4];
    const u32 a_290 = fp + 0x290u;

    call(rt, ctx, 0x08924DB8u, {fp});
    const u32 a_4e0 = fp + 0x4E0u;
    call(rt, ctx, 0x0891E0F8u, {a_290});
    const u32 a_900 = fp + 0x900u;
    call(rt, ctx, 0x0891A088u, {a_4e0});
    call(rt, ctx, 0x0891A484u, {a_900});
    call(rt, ctx, 0x0891C4D8u, {fp + 0xB00u});
    call(rt, ctx, 0x0891C614u, {fp + 0xD00u});
    call(rt, ctx, 0x0891A088u, {fp + 0xF00u});
    call(rt, ctx, 0x08926D60u, {fp + 0x1320u});
    put(rt, fp + 0x1320u, 0x08CC5A28u);
    call(rt, ctx, 0x08926D60u, {fp + 0x1520u});
    put(rt, fp + 0x1520u, 0x08CC59D0u);
    call(rt, ctx, 0x08926D60u, {fp + 0x1720u});
    put(rt, fp + 0x1720u, 0x08CC6020u);
    call(rt, ctx, 0x08918F30u, {fp + 0x1920u});
    call(rt, ctx, 0x0891C428u, {fp + 0x1B20u});
    call(rt, ctx, 0x0891E220u, {fp + 0x1D30u});
    call(rt, ctx, 0x08925AF4u, {fp + 0x1FD4u});
    call(rt, ctx, 0x08925A90u, {fp + 0x2010u});
    put(rt, fp + 0x2158u, 0xFFFFFFFFu);
    put(rt, fp + 0x2010u, 0x08CC5990u);
    put(rt, fp + 0x2154u, 0u);
    const u32 answer = call(rt, ctx, 0x08927128u, {fp + 0x2010u});
    put(rt, fp + 0x215Cu, answer);

    put(rt, fp + 0x2164u, 0x08CC6288u);
    put(rt, fp + 0x2160u, 0x08CC6200u);
    put(rt, fp + 0x2168u, fp);
    put(rt, fp + 0x216Cu, 0x08CC62A0u);
    put(rt, fp + 0x2170u, fp);
    put(rt, fp + 0x217Cu, fp + 0x2164u);
    put(rt, fp + 0x2178u, fp + 0x2160u);
    put(rt, fp + 0x2174u, 0x08CC5928u);

    put(rt, fp + 0x1F74u, a_290);
    put(rt, fp + 0x1F78u, a_900);
    put(rt, fp + 0x1F7Cu, a_4e0);
    put(rt, fp + 0x1F84u, fp + 0xD00u);
    put(rt, fp + 0x1F88u, fp + 0x1320u);
    put(rt, fp + 0x1F8Cu, fp + 0x1520u);
    put(rt, fp + 0x1F90u, fp + 0x1720u);
    put(rt, fp + 0x1FA0u, fp + 0xF00u);
    put(rt, fp + 0x1F94u, fp + 0x1920u);
    put(rt, fp + 0x1F98u, fp + 0x1B20u);
    put(rt, fp + 0x1F9Cu, fp + 0x1D30u);
    put(rt, fp + 0x1F70u, fp);
    put(rt, fp + 0x1F80u, fp + 0xF00u);
    put(rt, fp + 0x1FA4u, fp);
    put(rt, fp + 0x1FB4u, fp);
    put(rt, fp + 0x1FB0u, 0u);
    put(rt, fp + 0x1FB8u, 0u);
    put8(rt, fp + 0x1FBCu, 0u);
    put8(rt, fp + 0x1FBDu, 0u);
    put8(rt, fp + 0x1FBEu, 0u);
    put(rt, fp + 0x1FC0u, 0u);
    put8(rt, fp + 0x1FC4u, 0u);
    put(rt, fp + 0x1FD0u, 2u);
    put8(rt, fp + 0x1FC5u, 0u);
    put8(rt, fp + 0x1FC6u, 0u);
    put8(rt, fp + 0x1FC7u, 1u);
    put8(rt, fp + 0x204Cu, 1u);
    put(rt, fp + 0x1FC8u, 0u);
    put8(rt, fp + 0x1FCCu, 0u);
    put8(rt, fp + 0x1FCDu, 0u);
    put8(rt, fp + 0x1FCEu, 1u);
    put8(rt, fp + 0x1FCFu, 0u);
    put(rt, fp + 0x2050u, 0u);
    put8(rt, fp + 0x2054u, 0u);
    put(rt, fp + 0x20B4u, 0u);
    put(rt, fp + 0x20C0u, 0u);
    put(rt, fp + 0x20B8u, 0u);
    put(rt, fp + 0x20BCu, 0u);

    const u32 block = fp + 0x2058u;
    put_float(rt, block + 0x7Cu, 11.0f);
    put_float(rt, block + 0x80u, 50.0f);
    put(rt, block + 0x84u, 3u);
    put(rt, block + 0x88u, 0u);
    put(rt, block + 0x6Cu, 0u);
    put(rt, block + 0x70u, 0u);
    put(rt, block + 0x74u, 0u);
    put(rt, block + 0x78u, 0u);

    put8(rt, fp + 0x2104u, 0u);
    put8(rt, fp + 0x2105u, 0u);
    put8(rt, fp + 0x2106u, 0u);
    put8(rt, fp + 0x2107u, 0u);
    put8(rt, fp + 0x2114u, 0u);
    put(rt, fp + 0x2118u, 0u);
    put(rt, fp + 0x2120u, 0u);
    put(rt, fp + 0x211Cu, 0u);
    put8(rt, fp + 0x2180u, 0u);
    for (u32 base = fp; base != fp + 0x20u; base += 0x10u) {
        put(rt, base + 0x2184u, 0u);
        put(rt, base + 0x2188u, 0u);
        put(rt, base + 0x218Cu, 0u);
        put(rt, base + 0x2190u, 0u);
    }
    put(rt, fp + 0x2108u, 0u);
    put(rt, fp + 0x2110u, 0u);
    put(rt, fp + 0x210Cu, 0u);

    ctx.gpr[2] = 3u;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0895E6A8 (self = a0, in = a1, a2's low byte gates the tuning block): clears/floats the block at self from the
// 0x08CDE6xx tables when the global flag at 0x08CF35EE is set, then, for an ordered 9-byte record at `in`
// (b0 != 0, b7 < b0, b8 < b1, b5 < b7, b6 < b8, b2 < b1), writes the scaled values and the three bytes at
// 0x08CDE612 / 0x08CDE616 / 0x08CDE617. Frame 0x10.
// v0: the clearing call 0x0895E63C writes no v0, so the entries' v0 survives it; on the flag == 0 path v0 is the
// `lui v0, %hi(0x08CF35EE)` half (the batch-1930 gate saw exactly that: 0x08CF0000 against the draft's 0), on the
// flag != 0 path the block's last `lbu v0` (0x08CDE6EC), the failed `sltu` answers 0, and the full path ends with v0 = 1.

}  // namespace
}  // namespace new_batch_main_2019
}  // namespace gt::game

#define NEW2019(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2019::name, gt::ReturnKind::kind)
// dead frame slot into the new vector records (FINDINGS 2026-10-16), so the gate cannot prove it — see the comment above.
NEW2019(0x08929104u, func_08929104, Word);
