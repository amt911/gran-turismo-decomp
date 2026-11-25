// New batch main 903 (main-session functions that were never gated with useful inputs). Structured C from tools.disasm.
// Both are constructors: they only store into the object they get in a0 and call their bases / member initialisers, so the
// frames are the prologue's and the return value is the last call's that the asm lets through (v0 untouched to the epilogue).
// To verify: the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_903 {
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
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// 0x08919E50 (a0 = object, 0x40-byte frame): constructor. Base 0x08926D60(this); the two vtable words at +0x20C /
// +0x268 and the derived ones at +0x238 / +0x294 / +0x3CC are stored right after their 0x089263EC call; the six
// 0x08926398 members are +0x2C4, +0x2F0, the two frame slots sp+4 / sp+0 (re-read from the stack for the calls, as the
// asm does) which hold +0x31C / +0x348, then +0x374 and +0x3A0. Six 0x08926488 / two 0x08926514 string setters follow
// (the byte global 0x08CF212C = 0 sits in the first one's delay slot), then 0x08B62344(0xF0), 0x0891C35C(that result) —
// its v0 is the function's — and the flags of 0x08C8A87C into +0x410.
void func_08919E50(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const u32 member_20C = self + 0x20Cu;
    const u32 member_268 = self + 0x268u;
    const u32 member_2C4 = self + 0x2C4u;
    const u32 member_2F0 = self + 0x2F0u;
    const u32 member_374 = self + 0x374u;
    const u32 member_3A0 = self + 0x3A0u;
    const u32 member_3CC = self + 0x3CCu;
    call(rt, ctx, 0x08926D60u, {self});
    put(rt, self, 0x08CC5AE8u);
    put(rt, self + 0x1F8u, 0u);
    put(rt, self + 0x1FCu, 0u);
    put(rt, frame.at(4u), self + 0x31Cu);
    put(rt, frame.at(0u), self + 0x348u);
    put(rt, self + 0x200u, 0u);
    call(rt, ctx, 0x089263ECu, {member_20C});
    put(rt, self + 0x20Cu, 0x08CC6198u);
    call(rt, ctx, 0x089263ECu, {self + 0x238u});
    put(rt, self + 0x238u, 0x08CC6150u);
    call(rt, ctx, 0x089263ECu, {member_268});
    put(rt, self + 0x268u, 0x08CC6198u);
    call(rt, ctx, 0x089263ECu, {self + 0x294u});
    call(rt, ctx, 0x08926398u, {member_2C4});
    put(rt, self + 0x294u, 0x08CC6150u);
    call(rt, ctx, 0x08926398u, {member_2F0});
    call(rt, ctx, 0x08926398u, {word(rt, frame.at(4u))});
    call(rt, ctx, 0x08926398u, {word(rt, frame.at(0u))});
    call(rt, ctx, 0x08926398u, {member_374});
    call(rt, ctx, 0x08926398u, {member_3A0});
    call(rt, ctx, 0x089263ECu, {member_3CC});
    put(rt, self + 0x3CCu, 0x08CC61E0u);
    put8(rt, 0x08CF212Cu, 0u);
    call(rt, ctx, 0x08926514u, {member_20C, 0x08CDDB38u, 1u});
    call(rt, ctx, 0x08926514u, {member_268, 0x08CDDB38u, 1u});
    call(rt, ctx, 0x08926488u, {member_2C4, 0x08CDDB68u, 1u});
    call(rt, ctx, 0x08926488u, {member_2F0, 0x08CDDB50u, 1u});
    call(rt, ctx, 0x08926488u, {word(rt, frame.at(4u)), 0x08CDDB98u, 1u});
    call(rt, ctx, 0x08926488u, {word(rt, frame.at(0u)), 0x08CDDB98u, 1u});
    call(rt, ctx, 0x08926488u, {member_374, 0x08CDDB80u, 1u});
    call(rt, ctx, 0x08926488u, {member_3A0, 0x08CDDB80u, 1u});
    call(rt, ctx, 0x08926488u, {member_3CC, 0x08CDDB20u, 1u});
    const u32 flags = word(rt, 0x08C8A87Cu);
    put_float(rt, self + 0x3F8u, 1.0f);
    put(rt, self + 0x410u, flags);
    put8(rt, self + 0x3FCu, 1u);
    put(rt, self + 0x400u, 0u);
    put(rt, self + 0x404u, 0u);
    put8(rt, self + 0x408u, 0u);
    put(rt, self + 0x40Cu, 0u);
    const u32 made = call(rt, ctx, 0x08B62344u, {0xF0u});
    const u32 result = call(rt, ctx, 0x0891C35Cu, {made});
    put_float(rt, self + 0x414u, 0.1f);
    put(rt, self + 0x204u, made);
    put(rt, self + 0x208u, 0u);
    ctx.gpr[2] = result;
}

// 0x089B1124 (a0 = object, a1 kept at +0x14, 0x20-byte frame): constructor. Base 0x08A76ACC(this, a1), the vtable at +0,
// then a flat block of words/bytes up to +0x17C (the two floats at +0xA8 / +0xAC come from 0x08CDE764 / 0x08CDE760 and the
// +0xE4 / +0xE8 pair is those globals times 0.5), the 0x089A7644 member at +0x68 and the 0x089B4108 member at +0xB8 with
// +0x74 / +0x80 / +0x8C as its other three arguments, a self-linked pair at +0x178 / +0x17C, then two 0x08B35840
// allocations (0x10 x 0x400 into +0x58, 0x10 x 0x1000 into +0xB0) whose second result is v0.
} // namespace
} // namespace new_batch_main_903
} // namespace gt::game

#define NEW903(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_903::name, gt::ReturnKind::kind)
NEW903(0x08919E50u, func_08919E50, Word);
