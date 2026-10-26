// Hot batch 143: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_143 {
namespace {

using namespace gt::game::hot;

// The tagged-object header: byte +4 = kind, the rest of the +4 word cleared, *this = vtable. Returns the cleared word.
std::uint32_t write_header(psprecomp::Runtime &rt, std::uint32_t self, std::uint32_t kind, std::uint32_t vtable) {
    rt.memory().store8(self + 4u, static_cast<std::uint8_t>(kind));
    const std::uint32_t low = word(rt, self + 4u) & 0xFFu;
    put(rt, self, vtable);
    put(rt, self + 4u, low);
    return low;
}

// tag_header(this) with the member's kind and vtable: the header; v0 = vtable, v1 = the +4 word.
void tag_header(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t kind, std::uint32_t vtable) {
    ctx.gpr[3] = write_header(rt, ctx.gpr[4], kind, vtable);
    ctx.gpr[2] = vtable;
}

// tag_and_construct(this, ...) with the member's kind, vtable and member constructor: the header, then (tail call)
// construct(this + 8, a1, the +4 word, a3); v0 = vtable and v1 = this, as the original leaves them for the callee.
void tag_and_construct(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t kind, std::uint32_t vtable,
                       std::uint32_t construct) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[6] = write_header(rt, self, kind, vtable);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = self;
    tail_call(ctx, construct, {self + 8u});
}

// adjust_and_jump(this, ...) with the member's target: this += the offset at the vtable's -0xC (a virtual base), then
// the target (tail call); v0 keeps the offset.
void adjust_and_jump(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const std::uint32_t offset = word(rt, word(rt, ctx.gpr[4]) - 0xCu);
    ctx.gpr[2] = offset;
    tail_call(ctx, target, {ctx.gpr[4] + offset});
}

} // namespace

// Templates of their families (the members are the batch 143 rows below).
void func_08A64D64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 1u, 0x08CD28B8u); }
void func_08A64E60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 3u, 0x08CD2908u, 0x08A75D04u); }
void func_08C553FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C5541Cu); }

void func_08A65560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 8u, 0x08CD29D0u); }
void func_08A6603C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 22u, 0x08CD2C00u); }
void func_08A660EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 23u, 0x08CD2C28u); }
void func_08A661E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 24u, 0x08CD2C50u); }
void func_08A6639C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 26u, 0x08CD2CA0u); }
void func_08A667EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 31u, 0x08CD2D68u); }
void func_08A678BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 44u, 0x08CD2F70u); }
void func_08A67B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 47u, 0x08CD2FE8u); }
void func_08A67F58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 51u, 0x08CD3088u); }
void func_08A6804C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 52u, 0x08CD30B0u); }
void func_08A680FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 53u, 0x08CD30D8u); }
void func_08A681F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 54u, 0x08CD3100u); }
void func_08A683AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 56u, 0x08CD3260u); }
void func_08A6845C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 57u, 0x08CD3288u); }
void func_08A687D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 61u, 0x08CD3548u); }
void func_08A688CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 62u, 0x08CD3570u); }
void func_08A68C18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_header(rt, ctx, 66u, 0x08CD3610u); }
void func_08A64FC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 4u, 0x08CD2930u, 0x08A6972Cu); }
void func_08A65128(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 5u, 0x08CD2958u, 0x08A6972Cu); }
void func_08A65EE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 21u, 0x08CD2BD8u, 0x08A75D04u); }
void func_08A6651C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 29u, 0x08CD2D18u, 0x08B88F44u); }
void func_08A6696C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 33u, 0x08CD2DB8u, 0x08A6972Cu); }
void func_08A66AD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 34u, 0x08CD2DE0u, 0x08A6972Cu); }
void func_08A66C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 35u, 0x08CD2E08u, 0x08A75D04u); }
void func_08A66F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 37u, 0x08CD2E58u, 0x08A75D04u); }
void func_08A67244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 39u, 0x08CD2EA8u, 0x08B88F44u); }
void func_08A67454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 40u, 0x08CD2ED0u, 0x08A90480u); }
void func_08A675B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 41u, 0x08CD2EF8u, 0x08A90480u); }
void func_08A67C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag_and_construct(rt, ctx, 48u, 0x08CD3010u, 0x08A6972Cu); }
void func_08C55470(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C55490u); }
void func_08C55D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C55D4Cu); }
void func_08C55D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C55D88u); }
void func_08C709A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C709B8u); }
void func_08C709DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C709F0u); }
void func_08C78724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C78744u); }
void func_08C79814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C79828u); }
void func_08C798E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C798FCu); }
void func_08C79EC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C79ED8u); }
void func_08C79F90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C79FA4u); }
void func_08C7A710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { adjust_and_jump(rt, ctx, 0x08C7A730u); }

} // namespace hot_batch_143
} // namespace gt::game

#define HOT143(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_143::name, gt::ReturnKind::kind)
HOT143(0x08A64D64u, func_08A64D64, Word);
// HOT143(0x08A64E60u, func_08A64E60, Word);  // no comparable synthetic case yet
// HOT143(0x08C553FCu, func_08C553FC, Word);  // no comparable synthetic case yet
HOT143(0x08A65560u, func_08A65560, Word);
HOT143(0x08A6603Cu, func_08A6603C, Word);
HOT143(0x08A660ECu, func_08A660EC, Word);
HOT143(0x08A661E0u, func_08A661E0, Word);
HOT143(0x08A6639Cu, func_08A6639C, Word);
HOT143(0x08A667ECu, func_08A667EC, Word);
HOT143(0x08A678BCu, func_08A678BC, Word);
HOT143(0x08A67B58u, func_08A67B58, Word);
HOT143(0x08A67F58u, func_08A67F58, Word);
HOT143(0x08A6804Cu, func_08A6804C, Word);
HOT143(0x08A680FCu, func_08A680FC, Word);
HOT143(0x08A681F0u, func_08A681F0, Word);
HOT143(0x08A683ACu, func_08A683AC, Word);
HOT143(0x08A6845Cu, func_08A6845C, Word);
HOT143(0x08A687D8u, func_08A687D8, Word);
HOT143(0x08A688CCu, func_08A688CC, Word);
// HOT143(0x08A68C18u, func_08A68C18, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT143(0x08A64FC4u, func_08A64FC4, Word);
// HOT143(0x08A65128u, func_08A65128, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT143(0x08A65EE0u, func_08A65EE0, Word);  // no comparable synthetic case yet
HOT143(0x08A6651Cu, func_08A6651C, Word);
HOT143(0x08A6696Cu, func_08A6696C, Word);
// HOT143(0x08A66AD0u, func_08A66AD0, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT143(0x08A66C34u, func_08A66C34, Word);
// HOT143(0x08A66F3Cu, func_08A66F3C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT143(0x08A67244u, func_08A67244, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT143(0x08A67454u, func_08A67454, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT143(0x08A675B8u, func_08A675B8, Word);
HOT143(0x08A67C54u, func_08A67C54, Word);
HOT143(0x08C55470u, func_08C55470, Word);
// HOT143(0x08C55D38u, func_08C55D38, Word);  // no comparable synthetic case yet
HOT143(0x08C55D74u, func_08C55D74, Word);
HOT143(0x08C709A4u, func_08C709A4, Word);
HOT143(0x08C709DCu, func_08C709DC, Word);
// HOT143(0x08C78724u, func_08C78724, Word);  // no comparable synthetic case yet
// HOT143(0x08C79814u, func_08C79814, Word);  // no comparable synthetic case yet
// HOT143(0x08C798E8u, func_08C798E8, Word);  // no comparable synthetic case yet
// HOT143(0x08C79EC4u, func_08C79EC4, Word);  // no comparable synthetic case yet
// HOT143(0x08C79F90u, func_08C79F90, Word);  // no comparable synthetic case yet
// HOT143(0x08C7A710u, func_08C7A710, Word);  // no comparable synthetic case yet
