// Main-session batch 1148 (5 functions, draft only, unverified): structured C from tools.disasm.
// Shape: block = allocate(size, 4); ctor(block, arg); local = block; sink(this, &local).
// The constructors return their vtable in v0, not the object: the asm's `addu $s0, $v0, $zero` sits in the
// delay slot of the ctor `jal`, so it captures the *allocator's* v0 (the block) before the call runs.
// The local handed to the sink is the block, and v0 is the sink's result.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1148 {
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

// block = allocate(size, 4); ctor(block, arg); local = block; sink(this, &local); v0 = the sink's v0.
void allocate_construct_store(Runtime &rt, AllegrexContext &ctx, u32 size, u32 ctor, u32 arg, u32 sink) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    call(rt, ctx, ctor, {block, arg});
    put(rt, frame.at(0u), block);
    call(rt, ctx, sink, {self, frame.at(0u)});
}

// 0x08A97A0C: allocate 0x18, construct with a1 & 0xFF, sink 0x08A76AB4.
void func_08A97A0C(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_store(rt, ctx, 0x18u, 0x08A973BCu, ctx.gpr[5] & 0xFFu, 0x08A76AB4u);
}

// 0x08A98C7C: allocate 0x24, construct with a1, sink 0x08A76AB4.
void func_08A98C7C(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_store(rt, ctx, 0x24u, 0x08A98B54u, ctx.gpr[5], 0x08A76AB4u);
}

// 0x08AB01CC: allocate 0x18, construct with a1 & 0xFFFF, sink 0x08A76AB4.
void func_08AB01CC(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_store(rt, ctx, 0x18u, 0x08AAFBF8u, ctx.gpr[5] & 0xFFFFu, 0x08A76AB4u);
}

// 0x08AB1950: allocate 0x18, construct with a1 & 0xFF, sink 0x08A76AB4.
void func_08AB1950(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_store(rt, ctx, 0x18u, 0x08AB137Cu, ctx.gpr[5] & 0xFFu, 0x08A76AB4u);
}

// 0x08AB2EB0: allocate 0x28, construct with a1, sink 0x08A70EC8.
void func_08AB2EB0(Runtime &rt, AllegrexContext &ctx) {
    allocate_construct_store(rt, ctx, 0x28u, 0x08AB2B1Cu, ctx.gpr[5], 0x08A70EC8u);
}

} // namespace
} // namespace new_batch_main_1148
} // namespace gt::game

#define NEW1148(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1148::name, gt::ReturnKind::kind)
NEW1148(0x08A97A0Cu, func_08A97A0C, Word);
NEW1148(0x08A98C7Cu, func_08A98C7C, Word);
NEW1148(0x08AB01CCu, func_08AB01CC, Word);
NEW1148(0x08AB1950u, func_08AB1950, Word);
NEW1148(0x08AB2EB0u, func_08AB2EB0, Word);
