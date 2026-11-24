// New batch, main session 44: 19 short functions the recompiler emitted, written by hand as structured C from `tools.disasm`.
// Seventeen are three-instruction forwarders that end in a `j` (a tail call) with the real work in the
// delay slot — the arguments are built before the jump, exactly as the asm orders them — and two are `jr $ra` bodies that set
// v0 and write one word or byte through it. The asm ends each of them with the v0 it had at the jump (nothing in the delay
// slots writes v0 except the two `lw v0, 0(a0)` bodies, which are set here), so a callee that never writes v0 still hands the
// original's value on. No names: none has evidence.
//
// Most of these had a register-level (`G[n]`) body in an older hot_batch file, left unregistered for "no comparable synthetic
// case yet"; this file is the structured-C rewrite and the input shapes for the generator live in
// /tmp/claude-1000/shapes_main44.txt (12 of the 19). 0x08B2E224 also has an identical, unregistered body in
// src/game/unsorted/hot_leaves.cpp that was left out because its cases reach calls the isolated run cannot replay — the
// registration here is the one a gate would have to confirm or drop.
//
// Four functions of the batch the task listed are NOT here: 0x08C7B218, 0x08C7B228, 0x08C7B230 and 0x08C7B238 are entries of
// `.sceStub.text` (`jr $ra; nop`, i.e. the unresolved `scesupPreAcc::0x7ADA3927 / 0xA0EAF444 / 0xAD639111 / 0xB03FF882`
// import stubs, recomp/generated/generated_registry.cpp), not game code: tools.disasm cannot even read them (they sit outside
// .text), and the host layer registers those addresses as HLE imports, so a second registration of one address is refused at
// startup. Skip, as the task allows.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_44 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// 0x0880B774: a1 = *(a1); tail call 0x08869244(a0 + 0x30, a1).
void func_0880B774(Runtime &rt, AllegrexContext &ctx) {
    const u32 second = word(rt, ctx.gpr[5]);
    tail_call(ctx, 0x08869244u, {ctx.gpr[4] + 0x30u, second});
}

// 0x0885BBB0 (a0 = holder, a1 = a word): v0 = *a0; the word goes to *(v0 + 0xC).
void func_0885BBB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    put(rt, object + 0xCu, ctx.gpr[5]);
}

// 0x0885E788 (a0 = holder): v0 = *a0; clears the byte at *(v0 + 0x9C).
void func_0885E788(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = object;
    put_byte(rt, object + 0x9Cu, 0u);
}

// 0x0885E83C (a0 = holder): v0 = *a0 (the word the tail call starts with), and *(v0 + 0x5AC) goes on as a0 of 0x088D80D4.

// 0x08869230: tail call 0x0886A074(a0 + 0x1B0, a1, 1).
void func_08869230(Runtime &, AllegrexContext &ctx) {
    tail_call(ctx, 0x0886A074u, {ctx.gpr[4] + 0x1B0u, ctx.gpr[5], 1u});
}

// 0x08869E60: tail call 0x0886B30C(*(a0 + 4) + 0x4238).
void func_08869E60(Runtime &rt, AllegrexContext &ctx) {
    tail_call(ctx, 0x0886B30Cu, {word(rt, ctx.gpr[4] + 4u) + 0x4238u});
}

// 0x0886A020: tail call 0x0886B9A4(*(a0 + 4) + 0x4238).

// 0x08892468: tail call 0x088921E0(*a0, a1, a2, a3, t0, t1, 1) — only a0 and t2 change.

// 0x089B17AC: tail call 0x089B14F0(a0, a0 + 0x30, a0 + 0x18).

// 0x08ABFA0C: tail call 0x08B1FA64(*(a0 + 0xC), a0).

// 0x08ABFE64: tail call 0x08ABFDD0(*(a0 + 0xC), a0).

// 0x08AC0B50: tail call 0x08B0E6B0(0x08CFFB80) — the `lui`/`addiu` pair relocated to that address.
void func_08AC0B50(Runtime &, AllegrexContext &ctx) { tail_call(ctx, 0x08B0E6B0u, {0x08CFFB80u}); }

// 0x08AC0B5C: tail call 0x08B0E234(0x08CFFB80).

// 0x08AD0830: tail call 0x08B3252C(0x08D69170).

// 0x08B0DF0C: tail call 0x08B10C74(*(a0 + 0x38), a0).

// 0x08B2E224: tail call 0x08C7A9D8(*a0, 0).

// 0x08C75504 (a2 = holder): a2 = *a2, a3 = *(a2 - 0xC); tail call 0x08C752BC(a0, a1, a2, a3).

// 0x08C78738: tail call 0x08C78744(a0 - 8).

// 0x08C7A724: tail call 0x08C7A730(a0 - 8).

} // namespace
} // namespace new_batch_main_44
} // namespace gt::game

#define NEW_MAIN44(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_44::name, gt::ReturnKind::kind)
NEW_MAIN44(0x0880B774u, func_0880B774, Word);
NEW_MAIN44(0x0885BBB0u, func_0885BBB0, Word);
NEW_MAIN44(0x0885E788u, func_0885E788, Word);
NEW_MAIN44(0x08869230u, func_08869230, Word);
NEW_MAIN44(0x08869E60u, func_08869E60, Word);
NEW_MAIN44(0x08AC0B50u, func_08AC0B50, Word);
