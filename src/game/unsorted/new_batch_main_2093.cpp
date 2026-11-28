// New batch main 2093: re-drafts of three functions the gate mismatched in batches 1128, 1927 and 1928, written from
// `tools.disasm` (main session, draft only, not verified by the differential gate yet).
//
// What changed from the old drafts is the return value on each exit path: v0 is the callee's v0 when the last thing
// before the exit is a call (call_full leaves the callee's v0 / v1 / f0 in the context), or the value the original
// computed last. The delay slot of a `jal` / branch runs before the call / branch; a branch-likely's delay slot runs
// only when the branch is taken. Callee-saved registers are never written here (locals stand in), so they keep the
// caller's values like the original's restored ones. Structured C: named locals, `G[n]` only at the call boundary.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2093 {
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
s32 signed_of(u32 value) { return static_cast<s32>(value); }
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------
// 0x08AFD7B0 (a0 = object, f12 / f13 = two scales): 0x08BAC230(object) and 0x08C46020(&object, object + 0x38) only
// hand back their arguments (v0 = the frame slot, v1 = object + 0x38); the first four floats of the object (the first
// three negated) become a rotation (0x08BAC284) whose scaled axis is accumulated into object + 0x80. The magnitude
// from the object's +0xD8 points and that rotation's three floats (0x08BBBEC4) becomes f22; a zero stops here and
// v0 is that call's v0. The working path ends on 0x08C46508's v0.

// ---------------------------------------------------------------------------------------------------------------
// 0x0887C0A8 (a0 = state, a1 = owner): two 0..1 lookups from owner+0x60's +0xF0 table become the floats at +8 / +0xC;
// then owner+0x60's +0xF4 word is diffed against the state's old +0x18 (changed bits at +0x20, shared at +0x1C), the
// per-bit counters at +0x34..+0x40 are bumped, and two deltas are squared, clamped to 0.25 and signed into the
// floats at +0x10 / +0x14. v0 ends as the second delta: the original's `movz` sign is -1 on the first pass and +1 on
// the second, and v0 is the `mflo` product of that sign and (high - low).

// ---------------------------------------------------------------------------------------------------------------
// 0x08950438 (a0 = self, a1 / a2 / a3 = flag bytes, t0 = state word): reads the block at (self + 0xC) + 0x270, runs
// func_08949E8C(self) (its v0 is the time), func_0895534C (count), func_08955960 (a float) and func_0895596C (another
// float), then sets the state word and picks the flag bytes from time windows of 1000 / 300 / 500 / 1000 units and the
// two floats (0.2f / 0.7f). The original returns 1 on every path (v0 is set to 1 before each exit).

} // namespace
} // namespace new_batch_main_2093
} // namespace gt::game

#define NEW2093(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2093::name, gt::ReturnKind::kind)
