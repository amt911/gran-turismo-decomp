// New batch main-session 2126: two functions previous verdicts drafted and found mismatching (08AF623C, 08AFD7B0),
// written from `tools.disasm` as structured C (named locals, `G[n]` only at the call boundary).
// What changed from the old drafts (commits 5793c5ba / 1ddc69dc):
//   0x08AFD7B0  the zero-magnitude exit answers the pointer loaded by `lw v0, 0xD8(s2)` into the points array: the
//               leaf 0x08BBBEC4 (which jumps to 0x08BBBE94) never writes v0, so the original exits with that pointer,
//               not with the record the draft left in v0. On the working path 0x08C45E00 answers its a0 (the frame
//               slot +4) and the three callees after it (0x08C46718 / 0x08BBBECC / 0x08C46508) leave v0 alone.
//   0x08AF623C  transcribed exactly as the asm has it (the verdict's 1173-byte RAM diff kept no argument detail);
//               the +0x9C block table and the +0x98 block-count halfword drive a first pass that marks referenced
//               blocks and a tail pass that packs the unmarked ones.
// Delay slots are read as "before the call / branch"; v0 is the asm's on every exit path. The shapes handed over with
// this batch (/home/andres/repos/gt-ops/shapes/shapes_main2126.txt) give the objects valid memory.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2126 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose v0 / v1 / f0 stay in the context as the callee left them (the original's `jal` then reads them).
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).f0;
}

// 0x08AF623C (a0 = the object, a1 = the destination block array, a2 = the source record array, a3 = its count).
// The block table at +0x9C is written per source record's id (first pass, marking ids in the stack bitmap at sp+0x10),
// then the tail pass packs every block whose bit stayed clear into the destination in order; v0 ends as `limit`.

// 0x08AFD7B0 (a0 = object, f12 / f13 = two scales): 0x08BAC230(object) and 0x08C46020(&object, object + 0x38) only
// hand back their arguments (v0 = the frame slot, v1 = object + 0x38); the first four floats of the object (the first
// three negated) become a rotation (0x08BAC284) whose scaled axis is accumulated into object + 0x80. The magnitude
// from the object's +0xD8 points and that rotation's three floats (0x08BBBEC4) becomes f22; a zero stops here and the
// exit v0 is the pointer loaded from object + 0xD8. The working path ends on 0x08C45E00's v0 (its a0, the frame slot).
void func_08AFD7B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float first = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x90u);
    const u32 out = frame.at(0x1Cu);
    const u32 axis = frame.at(0x34u);
    const u32 point = frame.at(0x28u);
    const u32 slot = call(rt, ctx, 0x08BAC230u, {self});  // v0 = a0 = self
    put(rt, frame.at(0u), slot);
    const GuestResult pair =
        call_full(rt, ctx, 0x08C46020u, {frame.at(0u), self + 0x38u});  // v0 = the frame slot, v1 = self + 0x38
    const u32 record = word(rt, pair.v0);  // `lw v0, 0(v0)`: the value stored above (self)
    const float factor = psp_mul(-first, second);
    put_float(rt, axis, -float_at(rt, record));
    put_float(rt, axis + 4u, -float_at(rt, record + 4u));
    put_float(rt, axis + 8u, -float_at(rt, record + 8u));
    put_float(rt, axis + 0xCu, float_at(rt, record + 0xCu));
    ctx.gpr[2] = record;
    call_full(rt, ctx, 0x08BAC284u, {out, axis, pair.v1});
    const u32 points = word(rt, self + 0xD8u);
    ctx.gpr[2] = points;  // `lw v0, 0xD8(s2)`: a zero magnitude exits with this pointer in v0
    const float x0 = float_at(rt, points);
    const float y0 = float_at(rt, points + 4u);
    const float z0 = float_at(rt, points + 8u);
    const float dx = float_at(rt, points + 0x10u) - x0;
    const float dy = float_at(rt, points + 0x14u) - y0;
    const float dz = float_at(rt, points + 0x18u) - z0;
    const float px = float_at(rt, out);
    const float py = float_at(rt, out + 4u);
    const float pz = float_at(rt, out + 8u);
    const float f23 = (std::fabs(psp_mul(psp_mul(dx, dy), pz)) + std::fabs(psp_mul(psp_mul(dy, dz), px))) +
                      std::fabs(psp_mul(psp_mul(dx, dz), py));
    const float f22 = call_float(rt, ctx, 0x08BBBEC4u, {out, out});  // v0 of this call is the exit v0 when f22 == 0
    if (f22 == 0.0f) {
        ctx.fpr[0] = 0.0f;  // the original clears f0 before the compare (`mtc1 $zero, $f0`)
        return;
    }
    const float half = psp_mul(factor, 0.5f);
    const GuestResult quaternion =
        call_full(rt, ctx, 0x08C45E40u, {self + 0x38u}, {psp_mul(f22, psp_mul(f23, half))});
    put(rt, frame.at(4u), quaternion.v0);
    put(rt, frame.at(8u), quaternion.v1);
    const GuestResult scale = call_full(rt, ctx, 0x08C45E00u, {frame.at(4u)}, {1.0f / std::sqrt(f22)});
    put(rt, frame.at(0x10u), scale.v1);
    call_full(rt, ctx, 0x08C46718u, {axis, scale.v0, word(rt, scale.v0 + 4u)});
    call_full(rt, ctx, 0x08BBBECCu, {point, axis, frame.at(0x10u)});
    call_full(rt, ctx, 0x08C46508u, {self + 0x80u, point});
    ctx.gpr[2] = frame.at(4u);  // 0x08C45E00 answers its a0; the last three callees leave v0 alone
}

} // namespace
} // namespace new_batch_main_2126
} // namespace gt::game

#define NEW2126(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2126::name, gt::ReturnKind::kind)
NEW2126(0x08AFD7B0u, func_08AFD7B0, Word);
