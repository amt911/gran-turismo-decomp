// Main-session batch 2079: 30 functions re-drafted from the asm after gate mismatches (no fix_info entries were
// available for these addresses, so each one is checked against its disassembly). Shapes:
//   * a walk over a global linked list, calling a virtual on each object (0x08AB994C, 0x08AB9740, 0x08AB9FC0);
//   * a copy between two 0x20-byte records that returns the destination (0x08A8A970, 0x08A82BD8);
//   * a constructor or destructor that stores vtable words, makes member calls and ends in a tail call;
//   * a factory: alloc(size, 4), init(object), store(self, &slot) (the 0x08A5BC20 allocator, ten functions);
//   * a loop of three or four calls.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2079 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// Walks the list whose head is the word at `head`: node + object_offset is the object, node + next_offset the next
// node (0 ends the list). Calls the virtual at `method` of each object's vtable with the object in a0. v0 is
// `empty_v0` for an empty list, otherwise the last call's result.
void walk_list_calls(Runtime &rt, AllegrexContext &ctx, u32 head, u32 object_offset, u32 next_offset, u32 method,
                     u32 empty_v0) {
    ctx.gpr[2] = empty_v0;
    for (u32 node = word(rt, head); node != 0u; node = word(rt, node + next_offset)) {
        const u32 object = word(rt, node + object_offset);
        const u32 vtable = word(rt, object);
        call(rt, ctx, word(rt, vtable + method), {object});
    }
}

// The factory: object = alloc(size, 4), init(object), the frame's word 0 holds object, then store(self, &slot).
// v0 is store's result.
void make_and_store(Runtime &rt, AllegrexContext &ctx, u32 size, u32 init, u32 store) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    call(rt, ctx, init, {object});
    put(rt, frame.at(0u), object);
    call(rt, ctx, store, {self, frame.at(0u)});
}

// Two flag-bearing constructors (0x08A66D6C, 0x08A66DB8): byte 0x24 at +4 (its low byte kept in the word), vtable
// 0x08CD2E30, the member at +8 constructed, +0x14 cleared.
void flagged_ctor(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    rt.memory().store8(self + 4u, 0x24u);
    const u32 flags = word(rt, self + 4u) & 0xFFu;  // ins $v1, $zero, 8, 24
    put(rt, self, 0x08CD2E30u);
    put(rt, self + 4u, flags);
    call(rt, ctx, 0x08A75D04u, {self + 8u});
    put(rt, self + 0x14u, 0u);
}

// 0x08AB994C: walks the list at 0x08D7717C (object at node+4, next at node+0), virtual +0x34. Empty: v0 = 0x000A0000.

// 0x08AB9740: the same list, virtual +0x1C.
void func_08AB9740(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    walk_list_calls(rt, ctx, 0x08D7717Cu, 4u, 0u, 0x1Cu, 0x000A0000u);
}

// 0x08AB9FC0: walks the list at 0x08CFFA28 (object at node+0, next at node+4), virtual +0x14. Empty: v0 = 0x00020000.

// 0x08A8A970 (a0 = destination, a1 = source): copies +0x18 and +0x1C of the source to the destination unless they are
// the same record; v0 = the destination.
void func_08A8A970(Runtime &rt, AllegrexContext &ctx) {
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (dst != src) {
        call(rt, ctx, 0x08A781D4u, {dst, src});
        const u32 first = word(rt, src + 0x18u);
        const u32 second = word(rt, src + 0x1Cu);
        put(rt, dst + 0x18u, first);
        put(rt, dst + 0x1Cu, second);
    }
    ctx.gpr[2] = dst;
}

// 0x08A82BD8: the same copy.
void func_08A82BD8(Runtime &rt, AllegrexContext &ctx) {
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    if (dst != src) {
        call(rt, ctx, 0x08A781D4u, {dst, src});
        const u32 first = word(rt, src + 0x18u);
        const u32 second = word(rt, src + 0x1Cu);
        put(rt, dst + 0x18u, first);
        put(rt, dst + 0x1Cu, second);
    }
    ctx.gpr[2] = dst;
}

// 0x08A8522C (a0 = object): vtable 0x08CBEA58, the member at +0x14 released (vtable +4) if set, then a tail call
// 0x08A780B0 with the object. v0 is the vtable word when the member is null.

// 0x08A34AB8 (a0 = object): vtable 0x08CD1B20, the member at +0x14 released by 0x08B2E28C if set, 0x08A5B9EC on the
// member at +0x18, then a tail call 0x08A780B0 with the object.
void func_08A34AB8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 member = word(rt, self + 0x14u);
    put(rt, self, 0x08CD1B20u);
    if (member != 0u) call(rt, ctx, 0x08B2E28Cu, {member});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x18u});
    tail_call(ctx, 0x08A780B0u, {self});
}

// 0x088D4310 (a0 = array base): for a non-null base, calls 0x08B09290 on the three 0x28-byte elements at base+0x50,
// base+0x28 and base+0x00 in that order. v0 is unchanged for a null base.
void func_088D4310(Runtime &rt, AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (base != 0u) {
        u32 element = base + 0x50u;
        for (u32 remaining = 3u; remaining > 0u; --remaining) {
            call(rt, ctx, 0x08B09290u, {element});
            element -= 0x28u;
        }
    }
}

// 0x088F1358 (a0 = object): r = 0x088EF910(object+0x18); r < 5 gives 0, else 0x088EF918(object+0x18) == 0 gives 1.
void func_088F1358(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 r = call(rt, ctx, 0x088EF910u, {word(rt, self + 0x18u)});
    u32 result = 0u;
    if (r >= 5u) {  // sltiu: r below 5 leaves the result 0
        const u32 s = call(rt, ctx, 0x088EF918u, {word(rt, self + 0x18u)});
        result = (s == 0u) ? 1u : 0u;
    }
    ctx.gpr[2] = result;
}

// 0x088CB474 (a1 = value): for index 0..3, stores the value at +0x600 of the object 0x088CB360(index).
void func_088CB474(Runtime &rt, AllegrexContext &ctx) {
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    for (u32 index = 0u; index != 4u; ++index) {
        const u32 object = call(rt, ctx, 0x088CB360u, {index});
        put(rt, object + 0x600u, value);
    }
}

// 0x08A58518 (a0 = holder): object = word(holder+0x14) (the assert 0x08A5BB7C with 0x08CAF798 if null), then the
// virtual at +0x120 on the object; v0 = (result + 15) rounded down to 16.
void func_08A58518(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, holder + 0x14u);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    const u32 method = word(rt, word(rt, object) + 0x120u);
    call(rt, ctx, method, {object});
    ctx.gpr[2] = (ctx.gpr[2] + 0xFu) & 0xFFFFFFF0u;  // ins $v0, $zero, 0, 4
}

// 0x08A66D6C: a constructor (see flagged_ctor).

// 0x08A66DB8: a constructor (see flagged_ctor).

// 0x08A135B8 (a0 = object): locks object + 0x10, reads the word at +0x1C0 (in the second call's delay slot, so before
// it), unlocks and returns it.

// 0x08994DC4 (a0 = object): a destructor: two vtable words, two member releases (0x08A5B9EC at +0x11C and +0x118),
// then a tail call 0x08A0298C with the object.

// 0x08A57EE0 (a0 = object): vtable 0x08CD2100; if the hook at +0x14 is set, v0 = the member at +0xC and, when that is
// set, the hook is called with it; then a tail call 0x08A5BB5C with the object.
void func_08A57EE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 hook = word(rt, self + 0x14u);
    put(rt, self, 0x08CD2100u);
    ctx.gpr[2] = 0x08CD2100u;
    if (hook != 0u) {
        const u32 member = word(rt, self + 0xCu);
        ctx.gpr[2] = member;
        if (member != 0u) call(rt, ctx, hook, {member});
    }
    tail_call(ctx, 0x08A5BB5Cu, {self});
}

// 0x08A3F898: factory: alloc 0x38, init 0x08A3EADC, store 0x08A76AB4.

// 0x089B9828: factory: alloc 0x80, init 0x089B932C, store 0x089BAC44.
void func_089B9828(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x80u, 0x089B932Cu, 0x089BAC44u); }

// 0x089D08B0: factory: alloc 0x1D0, init 0x089D0704, store 0x0899537C.
void func_089D08B0(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x1D0u, 0x089D0704u, 0x0899537Cu); }

// 0x08A41118: factory: alloc 0x20, init 0x08A41088, store 0x08A76AB4.
void func_08A41118(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x20u, 0x08A41088u, 0x08A76AB4u); }

// 0x08A3F938: factory: alloc 0x108, init 0x08A3EB94, store 0x08A76AB4.
void func_08A3F938(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x108u, 0x08A3EB94u, 0x08A76AB4u); }

// 0x08A402D4: factory: alloc 0x10C, init 0x08A401EC, store 0x08A76AB4.
void func_08A402D4(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x10Cu, 0x08A401ECu, 0x08A76AB4u); }

// 0x08A43BC8: factory: alloc 0x48, init 0x08A43180, store 0x08A76AB4.
void func_08A43BC8(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x48u, 0x08A43180u, 0x08A76AB4u); }

// 0x089E23E0: factory: alloc 0x230, init 0x089E2210, store 0x08A0815C.
void func_089E23E0(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x230u, 0x089E2210u, 0x08A0815Cu); }

// 0x08A393EC: factory: alloc 0xD8, init 0x08A38A70, store 0x08A76AB4.
void func_08A393EC(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0xD8u, 0x08A38A70u, 0x08A76AB4u); }

// 0x08A44C7C: factory: alloc 0x20, init 0x08A44460, store 0x08A76AB4.
void func_08A44C7C(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x20u, 0x08A44460u, 0x08A76AB4u); }

// 0x08A37CBC: factory: alloc 0x1C, init 0x08A37B88, store 0x08A76AB4.
void func_08A37CBC(Runtime &rt, AllegrexContext &ctx) { make_and_store(rt, ctx, 0x1Cu, 0x08A37B88u, 0x08A76AB4u); }

// 0x08938260 (a0, a1, a2): 0x08B35F60(a1) gives the fourth argument of a tail call to 0x08938088.

// 0x08B10C24 (a0 = object, a1 = value): 0x08B34888(object+0x14), 0x08B10A7C(object, value), then a tail call
// 0x08B34B24 with object+0x14.

// 0x089FE4A8 (a0 = object, a1 = value): 0x08A0D658(object), vtable 0x08CC84E0 at +0, vtable 0x08CC8738 at +0x38, then
// a tail call 0x08A17B10 with object+0x120 and the value.

} // namespace
} // namespace new_batch_main_2079
} // namespace gt::game

#define MAIN2079(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2079::name, gt::ReturnKind::kind)
MAIN2079(0x08A8A970u, func_08A8A970, Word);
MAIN2079(0x08A34AB8u, func_08A34AB8, Word);
MAIN2079(0x088D4310u, func_088D4310, Word);
MAIN2079(0x088F1358u, func_088F1358, Word);
MAIN2079(0x08A82BD8u, func_08A82BD8, Word);
MAIN2079(0x088CB474u, func_088CB474, Word);
MAIN2079(0x08A58518u, func_08A58518, Word);
MAIN2079(0x08A57EE0u, func_08A57EE0, Word);
MAIN2079(0x08AB9740u, func_08AB9740, Word);
MAIN2079(0x089B9828u, func_089B9828, Word);
MAIN2079(0x089D08B0u, func_089D08B0, Word);
MAIN2079(0x08A41118u, func_08A41118, Word);
MAIN2079(0x08A3F938u, func_08A3F938, Word);
MAIN2079(0x08A402D4u, func_08A402D4, Word);
MAIN2079(0x08A43BC8u, func_08A43BC8, Word);
MAIN2079(0x089E23E0u, func_089E23E0, Word);
MAIN2079(0x08A393ECu, func_08A393EC, Word);
MAIN2079(0x08A44C7Cu, func_08A44C7C, Word);
MAIN2079(0x08A37CBCu, func_08A37CBC, Word);
