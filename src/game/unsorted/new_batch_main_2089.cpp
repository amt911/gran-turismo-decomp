// New batch (main session 2089): drafts from tools.disasm, structured C. Not gate-verified yet.
// Input shapes: gt-ops/shapes/shapes_main2089.txt.
// Not drafted (unsure, left for the next batch): 0x089C688C and 0x089C649C (list iteration over the container at sp+0xC),
// 0x08982D24 and 0x08A755B4 (long constructors with sp-held string vectors), 0x089AA038, 0x089773D0, 0x08871794 and 0x089E8964
// (float), 0x08B82B54 (64-bit multiply), 0x08B57420 and 0x08B57598 (jump tables), 0x08AF54BC (lwl/lwr).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2089 {
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

// The ll/sc reference drop: returns the old count and leaves v0 = 1 (the sc success flag the asm leaves in v0).
u32 decrement_reference(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    return old;
}

// 0x08A8DE58 (frame 0x30): tears down the object at a1 and returns a0. The blocks at +0x68, +0x60 and +0x58 go through 0x08B5C094
// and 0x08A5BC54(p, 0xC, 4); the count at +0x48 drops by one; the owner at +0x50 is the element before the list end at +0x48 (0 when
// the list is empty), released through 0x08C2BA70 and 0x08B64654; the list at +0x28 is unlinked through 0x08A90480, 0x08A5BAD8 and
// 0x08A5B9EC on a frame slot. v0 = a0 on every exit.

// 0x0890B7AC (frame 0x10): the owner's teardown. The base 0x0890B3D0 runs first. The object at +0x100 gives up one reference on +4;
// the last one runs its vtable slot 8, then one reference on +8 and, if that is the last, slot 0xC. The (value, object) pairs at
// +0x28, +0x20, +0x18, +0x10, +0x8 and +0 (object at +4) are released through the object's vtable slot 0 with the value as argument.
// v0 is the last call's result, or 1 after a reference drop with no call after it.

// 0x088845A0 (frame 0x20): tears down the object at a0 and ends in a tail call. Three held objects at +0x40 / +0x44 / +0x48 go through
// their vtable slot 0x2C and are cleared; +0x54 / +0x58 take the first vtable pair, then the elements from +0xDC to +0x12C (stride 0x14)
// go through their slot 0, the elements from +0x6C to +0xDC (stride 0x1C) through theirs; +0x54 / +0x58 then take the second pair.
// The owner at +0x50 loses two references; the last drop tail-calls its slot 0xC with the owner as a0. v0 = 0x08CBF428 when the owner
// is null, 1 after a reference drop with no call after it.

} // namespace
} // namespace new_batch_main_2089
} // namespace gt::game

#define NEW2089(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2089::name, gt::ReturnKind::kind)
