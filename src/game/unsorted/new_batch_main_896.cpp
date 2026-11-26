// Main-session batch 896: the object-factory copies at their second (duplicate) addresses, one lock-guarded bit read,
// and one tail-call wrapper. Each body is written from tools.disasm; none of these was reached with useful inputs
// before (the factories need a holder pointer for their `out` argument, see shapes_main896.txt).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_896 {
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

// The allocator every factory here calls first (size in a0, align 4 in a1) and the lock word the two calls of the
// guarded getter take in a0.
constexpr u32 kAllocate = 0x08A5BC20u;
constexpr u32 kLockWord = 0x08CF3EF0u;

// make_instance(out, size, construct, init): a fresh object of `size` bytes, constructed in place, then handed to
// init(out, &object) through one stack slot — the store sits in init's call delay slot, and v0 is init's answer.
void make_instance(Runtime &rt, AllegrexContext &ctx, u32 size, u32 construct, u32 init) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = frame.at(0u);
    const u32 object = call(rt, ctx, kAllocate, {size, 4u});
    call(rt, ctx, construct, {object});
    put(rt, block, object);
    ctx.gpr[2] = call(rt, ctx, init, {out, block});
}

// 0x08938260 (a0 = output, a1 = bitfield selector, a2 = index): a3 = the word func_08B35F60 picks out of
// (a1 & ~3), and the work continues in a tail call of func_08938088 with a0..a3 as they were.

// 0x089693E8 (a0 = object): bit 2 of the word at +0xE0, read between the two calls on the lock word.
void func_089693E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B344E0u, {kLockWord});
    const u32 flags = word(rt, object + 0xE0u);
    call(rt, ctx, 0x08B3457Cu, {kLockWord});
    ctx.gpr[2] = (flags >> 2u) & 1u;
}

void func_08975BC0(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0xB8u, 0x08975A54u, 0x089D8A60u); }
void func_08976A84(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x150u, 0x08976830u, 0x08993BF4u); }
void func_08976FA8(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x140u, 0x08976D54u, 0x089FE8D8u); }
void func_0897DF6C(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x38u, 0x0897D8D0u, 0x08A76AB4u); }
void func_089830BC(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x240u, 0x08982EA8u, 0x08969190u); }
void func_08991A20(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x1Cu, 0x0899166Cu, 0x08A76AB4u); }
void func_08992090(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0xA4u, 0x08991B54u, 0x089FFC94u); }
void func_089930A8(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x140u, 0x08992FC0u, 0x08A0D868u); }
void func_08994A8C(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x120u, 0x08994660u, 0x08A013D0u); }
void func_089987E8(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x190u, 0x0899865Cu, 0x089948F4u); }
void func_0899A3F0(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x140u, 0x0899A300u, 0x089FC5D0u); }
void func_0899BD1C(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x48u, 0x0899BC2Cu, 0x08973D98u); }
void func_0899DFD4(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x170u, 0x0899D908u, 0x089A274Cu); }
void func_089A1F08(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0xB8u, 0x089A1DA4u, 0x089D8A60u); }
void func_089A2904(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x120u, 0x089A2564u, 0x08A0D868u); }
void func_089A5D80(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x190u, 0x089A5BCCu, 0x089A274Cu); }
void func_089B4198(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x130u, 0x089B4098u, 0x08A013D0u); }
void func_089B4C70(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x240u, 0x089B4A84u, 0x089E119Cu); }
void func_089BFB20(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x2E0u, 0x089BEC50u, 0x089E119Cu); }
void func_089C29E8(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0xE0u, 0x089C1D64u, 0x08A0FD20u); }
void func_089CBBD8(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x170u, 0x089CBAB4u, 0x08A0D868u); }
void func_089DE9B0(Runtime &rt, AllegrexContext &ctx) { make_instance(rt, ctx, 0x1C0u, 0x089DE7D4u, 0x08969190u); }

} // namespace
} // namespace new_batch_main_896
} // namespace gt::game

#define NEW896(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_896::name, gt::ReturnKind::kind)
NEW896(0x089693E8u, func_089693E8, Word);
NEW896(0x08975BC0u, func_08975BC0, Word);
NEW896(0x08976A84u, func_08976A84, Word);
NEW896(0x08976FA8u, func_08976FA8, Word);
NEW896(0x0897DF6Cu, func_0897DF6C, Word);
NEW896(0x089830BCu, func_089830BC, Word);
NEW896(0x08991A20u, func_08991A20, Word);
NEW896(0x08992090u, func_08992090, Word);
NEW896(0x089930A8u, func_089930A8, Word);
NEW896(0x08994A8Cu, func_08994A8C, Word);
NEW896(0x089987E8u, func_089987E8, Word);
NEW896(0x0899A3F0u, func_0899A3F0, Word);
NEW896(0x0899BD1Cu, func_0899BD1C, Word);
NEW896(0x0899DFD4u, func_0899DFD4, Word);
NEW896(0x089A1F08u, func_089A1F08, Word);
NEW896(0x089A2904u, func_089A2904, Word);
NEW896(0x089A5D80u, func_089A5D80, Word);
NEW896(0x089B4198u, func_089B4198, Word);
NEW896(0x089B4C70u, func_089B4C70, Word);
NEW896(0x089BFB20u, func_089BFB20, Word);
NEW896(0x089C29E8u, func_089C29E8, Word);
NEW896(0x089CBBD8u, func_089CBBD8, Word);
NEW896(0x089DE9B0u, func_089DE9B0, Word);
