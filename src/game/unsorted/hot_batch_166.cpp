// Hot batch 166: two-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases. Most are a tail call with one argument register set in the delay slot; the rest are one-line
// leaves. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_166 {
namespace {

using namespace gt::game::hot;

// `j target` with register `reg` set to `value` in the delay slot; every other register as it came.
void tail_with(psprecomp::AllegrexContext &ctx, std::uint32_t target, unsigned reg, std::uint32_t value) {
    ctx.gpr[reg] = value;
    ctx.pc = target;
}

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}

} // namespace

// Tail calls with a constant argument.
void func_0885DDC8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x0892A07Cu, 4u, 0u); }
void func_088B40F8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x088B4080u, 5u, 0u); }
void func_08930908(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08930888u, 4u, 1u); }
void func_08930910(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08930888u, 4u, 0u); }
void func_08931098(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08931044u, 4u, 0x22u); }
void func_0898930C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x089892ECu, 5u, 1u); }
void func_08A69528(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08A5BC54u, 6u, 4u); }
void func_08A8DB8C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08A8DAB4u, 6u, 5u); }
void func_08AC2C8C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08AC1060u, 8u, 0u); }
void func_08B0F824(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08B0F780u, 7u, 2u); }
void func_08B0F838(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08B0F780u, 7u, 3u); }
void func_08B19354(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08B1928Cu, 6u, 1u); }
void func_08B33948(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08C7B128u, 4u, 0x20u); }  // an import stub
void func_08C723C0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08C72194u, 5u, 0u); }
void func_08C75AF0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08C75A68u, 6u, 0u); }

// Tail calls with an argument derived from another.
void func_089B8BAC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x089B8A6Cu, 6u, ctx.gpr[5]); }
void func_08AA1E6C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {  // `mfc1 a1, f12`: the float's bits
    tail_with(ctx, 0x08AA1BE4u, 5u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
}
void func_08C3132C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08AA1A8Cu, 5u, ctx.gpr[5] & 0xFFFFu); }
void func_08C70FE0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08C70D64u, 5u, ctx.gpr[5] & 0xFFFFu); }
void func_08C728BC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08C72614u, 5u, sext(ctx.gpr[5], 8u)); }
void func_08C728C4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { tail_with(ctx, 0x08C72614u, 5u, sext(ctx.gpr[5], 8u)); }

// Tail calls that store first.
void func_089D9B88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put(rt, ctx.gpr[4] + 0x90u, std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    ctx.pc = 0x089D9A84u;
}
void func_08AC22EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put(rt, ctx.gpr[4] + 8u, ctx.gpr[5]);
    ctx.pc = 0x08AC223Cu;
}

// Float setters: this->[offset] = f12.
void func_0891A87C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, ctx.gpr[4] + 0x1Cu, std::bit_cast<std::uint32_t>(ctx.fpr[12])); }
void func_0899D16C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, ctx.gpr[4] + 0x134u, std::bit_cast<std::uint32_t>(ctx.fpr[12])); }
void func_08A3A330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, ctx.gpr[4] + 0x20u, std::bit_cast<std::uint32_t>(ctx.fpr[12])); }
void func_08A87814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { put(rt, ctx.gpr[4] + 0x14u, std::bit_cast<std::uint32_t>(ctx.fpr[12])); }

// Leaves.
void func_088D0974(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[5] - ctx.gpr[6]; }
void func_088D46B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sext(rt.memory().load8(ctx.gpr[4] + 0x78u), 8u); }
void func_08959344(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 0.0f; }
void func_08A76DF8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = 0.0f; }
void func_08ADFC5C(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] << 5; }
void func_08B815DC(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {  // `clz`: 32 for 0
    ctx.gpr[2] = static_cast<std::uint32_t>(std::countl_zero(ctx.gpr[4]));
}
void func_08C46D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sext(rt.memory().load16(ctx.gpr[4] + 0xEEu), 16u); }

} // namespace hot_batch_166
} // namespace gt::game

#define HOT166(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_166::name, gt::ReturnKind::kind)
HOT166(0x0885DDC8u, func_0885DDC8, Word);
HOT166(0x088B40F8u, func_088B40F8, Word);
HOT166(0x08930908u, func_08930908, Word);
HOT166(0x08930910u, func_08930910, Word);
HOT166(0x08931098u, func_08931098, Word);
HOT166(0x0898930Cu, func_0898930C, Word);
HOT166(0x08A69528u, func_08A69528, Word);
// HOT166(0x08A8DB8Cu, func_08A8DB8C, Word);  // no comparable synthetic case yet
HOT166(0x08AC2C8Cu, func_08AC2C8C, Word);
HOT166(0x08B0F824u, func_08B0F824, Word);
HOT166(0x08B0F838u, func_08B0F838, Word);
HOT166(0x08B19354u, func_08B19354, Word);
HOT166(0x08B33948u, func_08B33948, Word);
// HOT166(0x08C723C0u, func_08C723C0, Word);  // no comparable synthetic case yet
HOT166(0x08C75AF0u, func_08C75AF0, Word);
HOT166(0x089B8BACu, func_089B8BAC, Word);
HOT166(0x08AA1E6Cu, func_08AA1E6C, Word);
HOT166(0x08C3132Cu, func_08C3132C, Word);
HOT166(0x08C70FE0u, func_08C70FE0, Word);
HOT166(0x08C728BCu, func_08C728BC, Word);
HOT166(0x08C728C4u, func_08C728C4, Word);
HOT166(0x089D9B88u, func_089D9B88, Word);
HOT166(0x08AC22ECu, func_08AC22EC, Word);
HOT166(0x0891A87Cu, func_0891A87C, Word);
HOT166(0x0899D16Cu, func_0899D16C, Word);
HOT166(0x08A3A330u, func_08A3A330, Word);
HOT166(0x08A87814u, func_08A87814, Word);
HOT166(0x088D0974u, func_088D0974, Word);
HOT166(0x088D46B8u, func_088D46B8, Word);
HOT166(0x08959344u, func_08959344, Float);
HOT166(0x08A76DF8u, func_08A76DF8, Float);
HOT166(0x08ADFC5Cu, func_08ADFC5C, Word);
HOT166(0x08B815DCu, func_08B815DC, Word);
HOT166(0x08C46D6Cu, func_08C46D6C, Word);
