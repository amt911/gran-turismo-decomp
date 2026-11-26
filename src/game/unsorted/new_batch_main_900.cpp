// New batch main 898: constructors/registrars of the object-building family, structured C from tools.disasm.
// Draft only: no gate has run these yet. Shapes for the synthetic generator are in
// /home/andres/repos/gt-ops/shapes/shapes_main898.txt (one line per dereferenced-argument function).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_898 {
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

// The "build then register" constructor family: allocate `size` bytes through the pool allocator (0x08A5BC20(size, 4)),
// construct the object in place (ctor: a0 = the object, a1 = the last a1 the caller wrote, 4 for the allocator's size
// argument), then register it: 0x08A76AB4(container, &slot) with the object in the slot at 0(sp). v0 = the registrar's.
void make_construct_register(Runtime &rt, AllegrexContext &ctx, u32 size, u32 second, u32 ctor) {
    GuestFrame frame(ctx, 0x20u);
    const u32 container = ctx.gpr[4];
    const u32 object = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    call(rt, ctx, ctor, {object, second});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A76AB4u, {container, frame.at(0u)});
}


// 0x0881A708: the same shape, but the constructor's second argument is this function's own a1 (s1), not the allocator's 4.

// 0x08A4DE78 (a0 = object, a1 = member stored at +0x20, a2 = byte stored at +0x24): base construct, vtable, two members.

// 0x0886A7B0 (a0 = object): base at +0x4238, its vtable word at +0x4240, two library calls, then the tail call on +8.

// 0x08935044 (a0 = object): hand this+0 and this+4 to 0x08934F78, copy four words of the global at 0x08CF2968 (+0xC0..+0xCC)
// into +0x58..+0x64, then tail call 0x0893388C with the last two copied words (a0/a1 as the asm leaves them).

// 0x0899A984 (a0 = object): base construct, two vtable words, six halfwords cleared.

// 0x089FF1C0 (a0 = object): base construct, two vtable words, a member built at +0x78 from this, mode 1, byte at +0x98 = 0.

// 0x08A03D58 (a0 = object): base construct, vtable, four floats of 1.0 at +0x20..+0x2C, -1 at +0x30.

// 0x08A1A8C0 (a0 = object): base construct, vtable, two members built at +0x68 and +0x44, then a tail call on +0x30.

// 0x08A32B74 (a0 = object): base construct, vtable, +0x14 built and +0x18 built from it, +0x42C cleared.

// 0x0880BFC8 (a0 = object, a1 = the object at +0x204): base constructs both, +0x28 is built from a1's child, +0x30 is
// initialised, +0x200 cleared, then the tail call on +0x208.

// 0x0887CC50 (a0 = object): a smart-pointer copy out of the word at +4, two temporaries destroyed, v0 = the copy's target.

// 0x0891C480 (a0 = object): base construct, vtable, a 0xF0-byte member constructed and stored at +0x1F8 (+0x1FC cleared),
// then the tail call on this.

// 0x08A18764 (a0 = object): base construct with zeroed arguments, vtable, +0x260 built, +0x280..+0x28C cleared; v0 = 0.

// 0x08A337F8 (a0 = object, a1 = a name/handle): convert it, format 0x40 bytes at sp+1, build this from them, v0 = this.

// 0x08A40324 (a0 = object): base construct, vtable, +0x108 built with the table 0x08CF6848 (+0x14 cleared), then the tail
// call on +0x18 with four zero arguments.

// 0x088D2D2C (a0 = object, a1 = a handle, f13 and f12 = a pair of floats): copy out of the handle's target into the block
// at sp, then build this from it; v0 = this. f13 goes first (the block is {f13, f12}).

// 0x08A18F74 (a0 = object, a1 = the object at +0x14): convert a1, base construct, vtable, then +0x14 is built from a1.

// 0x08A4D9EC (a0 = object, a1/a2 = two output floats, a3 = an argument): run 0x08A4D79C(this, a3, 0, 0, 1), then hand out the
// floats at +0x34 and +0x38.

} // namespace
} // namespace new_batch_main_898
} // namespace gt::game

#define NEW898(address, name) GT_DECOMP(address, gt::game::new_batch_main_898::name, gt::ReturnKind::Word)
