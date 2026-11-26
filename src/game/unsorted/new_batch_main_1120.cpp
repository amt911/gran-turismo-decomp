// New batch main 1120 (main-session drafts, structured C from tools.disasm, unverified by this session).
// Constructors that run a base constructor and then store a vtable plus a pair of words, allocate-and-assign helpers, and
// lock/body/unlock wrappers that end in a tail call. Registration kinds are Word: every one of them leaves a value in v0 on
// its exit path (the vtable address, a copied word, a callee's result, or the tail target's).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1120 {
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

// this = a0: the base constructor runs first, then the word pair at +0x18/+0x1C of the source (a1) is copied in and the
// vtable is stored at +0. v0 is the copied first word (0x08A82B8C, 0x08A8A8D8, 0x08A8A924).
void copy_pair_then_vtable(Runtime &rt, AllegrexContext &ctx, u32 base, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, base, {self, source});
    const u32 first = word(rt, source + 0x18u);
    const u32 second = word(rt, source + 0x1Cu);
    put(rt, self + 0x18u, first);
    put(rt, self + 0x1Cu, second);
    put(rt, self, vtable);
    ctx.gpr[2] = first;
}

// this = a0, the two words arrive in a2/a3 and are stored at +0x18/+0x1C after the base constructor; v0 = the vtable
// (0x08A5D750, 0x08A82AA0, 0x08A8A838).
void store_pair_then_vtable(Runtime &rt, AllegrexContext &ctx, u32 base, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, base, {self, ctx.gpr[5], second, third});
    put(rt, self + 0x18u, second);
    put(rt, self + 0x1Cu, third);
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
}

// this = a0: alloc(size, 4), construct the block, hand its address (the local at sp+0) to finish(this, &local); v0 = finish's
// (0x08A60ED0, 0x08A62B48, 0x08A81444, 0x08A9417C, 0x08A98AC0).
void allocate_then_assign(Runtime &rt, AllegrexContext &ctx, u32 size, u32 construct, u32 finish) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    call(rt, ctx, construct, {block});
    put(rt, frame.at(0u), block);
    const u32 result = call(rt, ctx, finish, {self, frame.at(0u)});
    ctx.gpr[2] = result;
}

// this = a0: lock(this + lock_offset), run body(this), unlock(lock); v0 = body's result
// (0x08B1F968, 0x08B1FB14, 0x08B1FB60).
void locked_body(Runtime &rt, AllegrexContext &ctx, u32 lock_offset, u32 body, u32 unlock) {
    const u32 self = ctx.gpr[4];
    const u32 lock = self + lock_offset;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B344E0u, {lock});
    const u32 result = call(rt, ctx, body, {self});
    call(rt, ctx, unlock, {lock});
    ctx.gpr[2] = result;
}

void func_08A82B8C(Runtime &rt, AllegrexContext &ctx) { copy_pair_then_vtable(rt, ctx, 0x08A782D8u, 0x08CBBD00u); }
void func_08A8A8D8(Runtime &rt, AllegrexContext &ctx) { copy_pair_then_vtable(rt, ctx, 0x08A782D8u, 0x08CD3300u); }
void func_08A8A924(Runtime &rt, AllegrexContext &ctx) { copy_pair_then_vtable(rt, ctx, 0x08A782D8u, 0x08CD3300u); }
void func_08A5D750(Runtime &rt, AllegrexContext &ctx) { store_pair_then_vtable(rt, ctx, 0x08A76ACCu, 0x08CB8688u); }
void func_08A82AA0(Runtime &rt, AllegrexContext &ctx) { store_pair_then_vtable(rt, ctx, 0x08A76ACCu, 0x08CBBD00u); }
void func_08A8A838(Runtime &rt, AllegrexContext &ctx) { store_pair_then_vtable(rt, ctx, 0x08A76ACCu, 0x08CD3300u); }

void func_08A60ED0(Runtime &rt, AllegrexContext &ctx) { allocate_then_assign(rt, ctx, 0x20u, 0x08A60E10u, 0x08A76AB4u); }
void func_08A62B48(Runtime &rt, AllegrexContext &ctx) { allocate_then_assign(rt, ctx, 0x5Cu, 0x08A629FCu, 0x08A76AB4u); }
void func_08A81444(Runtime &rt, AllegrexContext &ctx) { allocate_then_assign(rt, ctx, 0x34u, 0x08A81380u, 0x08A76AB4u); }
void func_08A9417C(Runtime &rt, AllegrexContext &ctx) { allocate_then_assign(rt, ctx, 0x28u, 0x08A9402Cu, 0x08A71E10u); }
void func_08A98AC0(Runtime &rt, AllegrexContext &ctx) { allocate_then_assign(rt, ctx, 0x24u, 0x08A98A2Cu, 0x08A76AB4u); }

void func_08B1F968(Runtime &rt, AllegrexContext &ctx) { locked_body(rt, ctx, 0xCu, 0x08B1FDB0u, 0x08B3457Cu); }
void func_08B1FB14(Runtime &rt, AllegrexContext &ctx) { locked_body(rt, ctx, 0xCu, 0x08B1FDD0u, 0x08B3457Cu); }
void func_08B1FB60(Runtime &rt, AllegrexContext &ctx) { locked_body(rt, ctx, 0xCu, 0x08B1FE04u, 0x08B3457Cu); }

// 0x08A86FF4: base constructor, vtable at +0, then the word from a1 is written to the local at sp+0 and the sub-object at
// +0x14 is initialised from it (its own call's result is what comes back).
void func_08A86FF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A76ACCu, {self, value});
    put(rt, self, 0x08CBEF70u);
    put(rt, frame.at(0u), value);
    const u32 result = call(rt, ctx, 0x08A86D00u, {self + 0x14u, frame.at(0u)});
    ctx.gpr[2] = result;
}

// 0x08A99FFC: v0 = a0 after a0's container has been given the element found by a binary search over a1's table: the count is
// the arithmetic (end - begin) >> 2 of the words at +0x18 / +0x14, searched with key (a2, a3) and bounds (0, count).
void func_08A99FFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 container = ctx.gpr[5];
    const u32 key_low = ctx.gpr[6];
    const u32 key_high = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const u32 end = word(rt, container + 0x18u);
    const u32 begin = word(rt, container + 0x14u);
    const u32 count = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    const u32 found = call(rt, ctx, 0x08A986F4u, {container, container, key_low, key_high, 0u, count});
    call(rt, ctx, 0x08A85B1Cu, {self, found});
    ctx.gpr[2] = self;
}

// 0x08AC6928: base constructor, then the flags byte at +0x28, -1 at +0x2C and the words +0x30..+0x44 cleared; v0 = -1.
void func_08AC6928(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B28DCCu, {self});
    rt.memory().store8(self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0xFFFFFFFFu);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x30u, 0u);
    rt.memory().store8(self + 0x29u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x40u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08AD4100: base constructor, vtable at +0, -1 at +0x68, zero at +0x70/+0x64/+0x6C; v0 = the vtable.
void func_08AD4100(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AD43C0u, {self});
    put(rt, self, 0x08CD6DA8u);
    put(rt, self + 0x68u, 0xFFFFFFFFu);
    put(rt, self + 0x70u, 0u);
    put(rt, self + 0x64u, 0u);
    put(rt, self + 0x6Cu, 0u);
    ctx.gpr[2] = 0x08CD6DA8u;
}

// 0x08B0305C: the first call's answer becomes the second argument of the tail call (a0 = this, a2/a3 pass through).
void func_08B0305C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08B023C4u, {self, second, third});
    ctx.gpr[2] = answer;
    tail_call(ctx, 0x08B02FD4u, {self, answer, second, third});
}

// 0x08B240C0: base constructor, vtable at +0, three sub-objects reset at +0x1C/+0x44/+0x70 and the byte at +0x9C cleared;
// v0 is the last sub-object reset's result.
void func_08B240C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B248FCu, {self});
    put(rt, self, 0x08CD9600u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x1Cu});
    call(rt, ctx, 0x08B28D34u, {self + 0x44u});
    const u32 result = call(rt, ctx, 0x08B28D34u, {self + 0x70u});
    rt.memory().store8(self + 0x9Cu, 0u);
    ctx.gpr[2] = result;
}

// 0x08A6B748: base constructor, vtable at +0, the member at +0x1C built from a1, then +0x14/+0x20 cleared; v0 is that
// member's result.
void func_08A6B748(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A719F0u, {self, source});
    put(rt, self, 0x08CD2338u);
    const u32 result = call(rt, ctx, 0x08B88F44u, {self + 0x1Cu, source});
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x20u, 0u);
    ctx.gpr[2] = result;
}

// 0x08A9BDB0: lock(this + 0x4C), body(this + 0x44, a1), then the tail call is the unlock with the lock's address.
void func_08A9BDB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 lock = self + 0x4Cu;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B344E0u, {lock});
    call(rt, ctx, 0x08B855E0u, {self + 0x44u, source});
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// 0x08AC2E2C: base constructor, the sub-object at +0x18C reset, two flag bytes cleared and two 0x80-byte buffers at
// +0x1B6 / +0x236 zeroed (the second one in the tail call).
void func_08AC2E2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08AC2CB8u, {self});
    call(rt, ctx, 0x08B28DCCu, {self + 0x18Cu});
    rt.memory().store8(self + 0x1B4u, 0u);
    rt.memory().store8(self + 0x1B5u, 0u);
    call(rt, ctx, 0x08B36018u, {self + 0x1B6u, 0u, 0x80u});
    tail_call(ctx, 0x08B36018u, {self + 0x236u, 0u, 0x80u});
}

// 0x08B10C24: lock(this + 0x14), body(this, a1), then the tail call is the unlock with the lock's address.

// 0x08B22CC4: base constructor, then a2 at +8, the vtable at +0 and a1 at +4; v0 = the vtable.
void func_08B22CC4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B219B0u, {self, first, second});
    put(rt, self + 8u, second);
    put(rt, self, 0x08CD95C0u);
    put(rt, self + 4u, first);
    ctx.gpr[2] = 0x08CD95C0u;
}

// 0x08B3744C: base constructor, then a2 at +0x24, the vtable at +0 and a1 at +0x20; v0 = the vtable.
void func_08B3744C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B36290u, {self, first, second});
    put(rt, self + 0x24u, second);
    put(rt, self, 0x08CDA8B8u);
    put(rt, self + 0x20u, first);
    ctx.gpr[2] = 0x08CDA8B8u;
}

// 0x08B3A65C: helper(0x08CB5640, a0, a1, a3), then the tail call gets (a2, a3).
void func_08B3A65C(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08B3A5E8u, {0x08CB5640u, first, second, fourth});
    tail_call(ctx, 0x08B3A584u, {third, fourth});
}

// 0x08C73CF4: v0 = lookup(a1) becomes the fourth argument of the tail call (a0, a1, a2 pass through).
void func_08C73CF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08B35F60u, {key});
    ctx.gpr[2] = answer;
    tail_call(ctx, 0x08C73920u, {self, key, third, answer});
}

// 0x08C73D44: the same shape as 0x08C73CF4, a different tail target.
void func_08C73D44(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08B35F60u, {key});
    ctx.gpr[2] = answer;
    tail_call(ctx, 0x08C73A98u, {self, key, third, answer});
}

// 0x08C73D94: the same shape as 0x08C73CF4, a different tail target.
void func_08C73D94(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 answer = call(rt, ctx, 0x08B35F60u, {key});
    ctx.gpr[2] = answer;
    tail_call(ctx, 0x08C73B40u, {self, key, third, answer});
}

} // namespace
} // namespace new_batch_main_1120
} // namespace gt::game

#define NEW1120(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1120::name, gt::ReturnKind::kind)
NEW1120(0x08A5D750u, func_08A5D750, Word);
NEW1120(0x08A60ED0u, func_08A60ED0, Word);
NEW1120(0x08A62B48u, func_08A62B48, Word);
NEW1120(0x08A6B748u, func_08A6B748, Word);
NEW1120(0x08A81444u, func_08A81444, Word);
NEW1120(0x08A82AA0u, func_08A82AA0, Word);
NEW1120(0x08A82B8Cu, func_08A82B8C, Word);
NEW1120(0x08A86FF4u, func_08A86FF4, Word);
NEW1120(0x08A8A838u, func_08A8A838, Word);
NEW1120(0x08A8A8D8u, func_08A8A8D8, Word);
NEW1120(0x08A8A924u, func_08A8A924, Word);
NEW1120(0x08A9417Cu, func_08A9417C, Word);
NEW1120(0x08A98AC0u, func_08A98AC0, Word);
NEW1120(0x08A99FFCu, func_08A99FFC, Word);
NEW1120(0x08A9BDB0u, func_08A9BDB0, Word);
NEW1120(0x08AC2E2Cu, func_08AC2E2C, Word);
NEW1120(0x08AC6928u, func_08AC6928, Word);
NEW1120(0x08AD4100u, func_08AD4100, Word);
NEW1120(0x08B0305Cu, func_08B0305C, Word);
NEW1120(0x08B1F968u, func_08B1F968, Word);
NEW1120(0x08B1FB14u, func_08B1FB14, Word);
NEW1120(0x08B1FB60u, func_08B1FB60, Word);
NEW1120(0x08B22CC4u, func_08B22CC4, Word);
NEW1120(0x08B240C0u, func_08B240C0, Word);
NEW1120(0x08B3744Cu, func_08B3744C, Word);
NEW1120(0x08B3A65Cu, func_08B3A65C, Word);
NEW1120(0x08C73CF4u, func_08C73CF4, Word);
NEW1120(0x08C73D44u, func_08C73D44, Word);
NEW1120(0x08C73D94u, func_08C73D94, Word);
