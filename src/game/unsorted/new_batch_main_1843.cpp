// New batch main 1843 (functions the extra seeds made emittable). Phase A functions, structured C from tools.disasm.
// Comparable by synthetic cases (likely): the lookup getters (a0 + a name constant, one load) and the destructor-shaped functions whose
// callees only read the object; to verify: B5A-style "call then tail call" shapes read through a0 only.
// Before registering: run the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1843 {
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

enum class Load { Word, Half, Byte, UnsignedByte };

// 0x08AF4B1C (a0 = object): lw v1,0(a0) is the vtable and lw v0,8(v1) its slot, called with a1 = the object and a0 = a
// 0x50-byte stack temporary (the delay slot sets a0 before the jalr). func_08AF4B0C(temporary) then answers the ratio of
// the temporary's floats at +0x14 and +0 in f0; it never writes v0, so v0 stays the virtual call's result.
void func_08AF4B1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    const u32 space = frame.at(0u);
    ctx.gpr[5] = self;
    const u32 slot = word(rt, word(rt, self) + 0x8u);
    call_indirect(rt, ctx, slot, {space}, {});
    const GuestResult ratio = call_guest(rt, ctx, 0x08AF4B0Cu, {space}, {});
    ctx.gpr[2] = ratio.v0;
    ctx.gpr[3] = ratio.v1;
    ctx.fpr[0] = ratio.f0;
}
} // namespace
} // namespace new_batch_main_1843
} // namespace gt::game

#define NEW1843(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1843::name, gt::ReturnKind::kind)
NEW1843(0x08AF4B1Cu, func_08AF4B1C, Word);
