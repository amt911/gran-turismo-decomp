// New batch main-1733 (main-session batch 1733, unverified draft). Redrafts of five batch-1620 gate mismatches.
// All five allocated a block and then used the *constructor's* v0 as the object; but `addu sN, v0, zero` is the
// constructor jal's delay slot, so it runs before the call and captures the allocator's block. The object is that
// block: the final callee (0x08A76AB4 -> 0x08A5BAB8) stores it into *a0 and bumps its refcount. Structured C from
// tools.disasm. Before registering: the four audits and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1733 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A direct `jal` whose callee takes float arguments ($f12, $f13): v0 enters as the allocator left it, not as the target.
u32 call_with_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                     std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Restores s0-s7 on exit, as the asm's epilogue does for the registers it saved.
struct SavedS {
    AllegrexContext &ctx;
    u32 s[8];
    explicit SavedS(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 8; ++i) s[i] = c.gpr[16 + i];
    }
    ~SavedS() {
        for (unsigned i = 0; i < 8; ++i) ctx.gpr[16 + i] = s[i];
    }
};

// 0x08808CC8 (a0 = slot the new object goes into, a1 / a2 = members): allocates 0x1C bytes, constructs it, sets the
// vtable and the two members at +0x14 / +0x18, then hands the block to 0x08A76AB4 with a0 and a frame word; v0 = its
// result.
void func_08808CC8(Runtime &rt, AllegrexContext &ctx) {
    const u32 slot = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0);
    ctx.gpr[19] = slot;
    ctx.gpr[17] = arg1;
    ctx.gpr[18] = arg2;
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x1Cu, 4u});
    ctx.gpr[16] = block;  // the constructor's delay slot: s0 = the block, not the constructor's v0
    call(rt, ctx, 0x08A76ACCu, {block});
    put(rt, block, 0x08CB8BD8u);
    call(rt, ctx, 0x08A80FA4u, {block + 0x14u, arg1});
    call(rt, ctx, 0x08A983E8u, {block + 0x18u, arg2});
    put(rt, sp, block);
    call(rt, ctx, 0x08A76AB4u, {slot, sp});
}

// 0x0884CC98 (a0 = slot, a2..t1 = builder arguments): allocates 0x28 bytes and builds it; a1 is the allocator's 4
// (the asm never reloads the caller's a1). The block goes to 0x08A76AB4 with a0; v0 = its result.
void func_0884CC98(Runtime &rt, AllegrexContext &ctx) {
    const u32 slot = ctx.gpr[4];
    const u32 arg2 = ctx.gpr[6];
    const u32 arg3 = ctx.gpr[7];
    const u32 arg4 = ctx.gpr[8];
    const u32 arg5 = ctx.gpr[9];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0);
    ctx.gpr[21] = slot;
    ctx.gpr[20] = arg5;
    ctx.gpr[19] = arg4;
    ctx.gpr[17] = arg3;
    ctx.gpr[16] = arg2;
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x28u, 4u});
    ctx.gpr[18] = block;  // the builder's delay slot: s2 = the block
    call(rt, ctx, 0x0884C664u, {block, 4u, arg2, arg3, arg4, arg5});
    put(rt, sp, block);
    call(rt, ctx, 0x08A76AB4u, {slot, sp});
}

// 0x0899C7F4 and 0x0899CA0C (a0 = slot, a1..t0 = builder arguments): allocate 0x2C bytes, build, then store the block
// into *a0 through 0x089BA130; the two differ only in the builder (0x0899C6E0 / 0x0899C900).
void build_2c_and_store(Runtime &rt, AllegrexContext &ctx, u32 builder) {
    const u32 slot = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    const u32 arg3 = ctx.gpr[7];
    const u32 arg4 = ctx.gpr[8];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0);
    ctx.gpr[21] = slot;
    ctx.gpr[17] = arg1;
    ctx.gpr[20] = arg4;
    ctx.gpr[19] = arg3;
    ctx.gpr[18] = arg2;
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    ctx.gpr[16] = block;  // the builder's delay slot: s0 = the block
    call(rt, ctx, builder, {block, arg1, arg2, arg3, arg4});
    put(rt, sp, block);
    call(rt, ctx, 0x089BA130u, {slot, sp});
}

void func_0899C7F4(Runtime &rt, AllegrexContext &ctx) { build_2c_and_store(rt, ctx, 0x0899C6E0u); }
void func_0899CA0C(Runtime &rt, AllegrexContext &ctx) { build_2c_and_store(rt, ctx, 0x0899C900u); }

// 0x089C3B74 (a0 = slot, a1 / a2 = builder arguments, f12 / f13 = floats): allocates 0x2C bytes, builds it with the
// two floats, then stores the block into *a0 through 0x08965854; f20 / f21 are kept as the asm keeps them.
void func_089C3B74(Runtime &rt, AllegrexContext &ctx) {
    const u32 slot = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    const float f12 = ctx.fpr[12];
    const float f13 = ctx.fpr[13];
    const float keep20 = ctx.fpr[20];
    const float keep21 = ctx.fpr[21];
    SavedS saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0);
    ctx.fpr[20] = f12;
    ctx.fpr[21] = f13;
    ctx.gpr[19] = slot;
    ctx.gpr[17] = arg1;
    ctx.gpr[18] = arg2;
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    ctx.gpr[16] = block;  // the builder's delay slot: s0 = the block
    call_with_floats(rt, ctx, 0x089C3684u, {block, arg1, arg2}, {f12, f13});
    put(rt, sp, block);
    call(rt, ctx, 0x08965854u, {slot, sp});
    ctx.fpr[20] = keep20;
    ctx.fpr[21] = keep21;
}

} // namespace
} // namespace new_batch_main_1733
} // namespace gt::game

#define NEW1733(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1733::name, gt::ReturnKind::kind)
NEW1733(0x08808CC8u, func_08808CC8, Word);
NEW1733(0x0884CC98u, func_0884CC98, Word);
NEW1733(0x0899C7F4u, func_0899C7F4, Word);
NEW1733(0x0899CA0Cu, func_0899CA0C, Word);
NEW1733(0x089C3B74u, func_089C3B74, Word);
