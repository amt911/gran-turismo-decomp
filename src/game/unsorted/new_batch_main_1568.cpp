// Draft batch main-session 1568 (30 functions, unverified: the verification scripts gate them later).
// Nineteen are list walks over a global head: node = *head; while (node) { obj = node.obj; obj->vtbl[slot](obj); node = node.next; }.
// There v0 is the lui of the head global when the list is empty, else the last callee's result.
// The rest are one-off shapes (a tail call, a counter, a store through a0) read straight from the asm.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1568 {
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

// lui/lw pair over the head global: v0 keeps the lui (the high half of the global's address, rounded for the low half's sign).
void list_walk(Runtime &rt, AllegrexContext &ctx, u32 head, u32 lui_value, u32 obj_offset, u32 next_offset, u32 slot) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = lui_value;
    u32 node = word(rt, head);
    while (node != 0u) {
        const u32 obj = word(rt, node + obj_offset);
        const u32 vtable = word(rt, obj);
        call(rt, ctx, word(rt, vtable + slot), {obj});
        node = word(rt, node + next_offset);
    }
}

// --- list walks: head global, object at node+obj, next at node+next, slot of the object's vtable ---
void func_08ABB204(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D77184u, 0x08D70000u, 0x4u, 0x0u, 0x6Cu); }
void func_08ABB504(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D77188u, 0x08D70000u, 0x4u, 0x0u, 0x14u); }
void func_08ABB88C(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D77188u, 0x08D70000u, 0x4u, 0x0u, 0xCu); }
void func_08ABB8D8(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D77188u, 0x08D70000u, 0x4u, 0x0u, 0x10u); }
void func_08ABC1D4(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D7718Cu, 0x08D70000u, 0x4u, 0x0u, 0xCu); }
void func_08ABC220(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D7718Cu, 0x08D70000u, 0x4u, 0x0u, 0x10u); }
void func_08ABC26C(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D7718Cu, 0x08D70000u, 0x4u, 0x0u, 0x14u); }
void func_08ABC6E8(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D7718Cu, 0x08D70000u, 0x4u, 0x0u, 0x14u); }
void func_08B0D46C(Runtime &rt, AllegrexContext &ctx) { list_walk(rt, ctx, 0x08D7734Cu, 0x08D70000u, 0x4u, 0x0u, 0x20u); }

// Tail call to 0x08B1A16C with this + 0x10 (the delay slot's addiu runs before the jump).
void func_08B1A174(Runtime &, AllegrexContext &ctx) { tail_call(ctx, 0x08B1A16Cu, {ctx.gpr[4] + 0x10u}); }

// Returns 0 when the byte at +0xC is set. Otherwise 1, and when limit (+4) < first (+0), 1 ^ the callee's answer (low byte).
void func_08B2082C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    if (rt.memory().load8(self + 0xCu) == 0u) {
        result = 1u;  // the delay slot of the test below runs on both paths
        const u32 first = word(rt, self);
        const u32 limit = word(rt, self + 0x4u);
        if (static_cast<s32>(limit) < static_cast<s32>(first)) {
            const u32 answer = call(rt, ctx, 0x08B20824u, {self, first - 1u});
            result = (answer ^ 1u) & 0xFFu;
        }
    }
    ctx.gpr[2] = result;
}

// A small ring of 8-byte entries: the count at +0x640 wraps to 0 once it reaches 100, the entry (a2, a3) goes at
// +0x644 index*8 and the count at +0x644 grows while it is below 100; then tail call 0x08B3E908 with (this, index, a2, a3).
void func_08B3EA3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 index = word(rt, self + 0x640u);
    if (static_cast<s32>(index) >= 0x64) {
        put(rt, self + 0x640u, 0u);
        index = 0u;
    }
    const u32 count = word(rt, self + 0x644u);
    const u32 entry = self + index * 8u;
    const u32 next_index = index + 1u;
    const bool room = static_cast<s32>(count) < 0x64;
    put(rt, entry, ctx.gpr[6]);
    put(rt, entry + 0x4u, ctx.gpr[7]);
    put(rt, self + 0x640u, next_index);
    if (room) put(rt, self + 0x644u, count + 1u);
    ctx.gpr[2] = room ? 1u : 0u;
    tail_call(ctx, 0x08B3E908u, {self, next_index, ctx.gpr[6], ctx.gpr[7]});
}

// Two calls to the lookup at the global 0x089624EC (first with this->+0x10 of the object at *this), then the
// answer's field 0x68 compared with the second call's result: v0 = !(equal), or 0 when the answer is null.
void func_088AA6F4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 holder = word(rt, ctx.gpr[4]);
    const u32 found = call(rt, ctx, 0x089624ECu, {holder + 0x10u});
    const u32 answer = word(rt, found);
    u32 v0 = 1u;
    if (answer != 0u) {
        const u32 second = call(rt, ctx, 0x08B2D1A8u, {answer + 0x8u});
        v0 = ((second ^ word(rt, answer + 0x68u)) == 0u) ? 1u : 0u;
    }
    ctx.gpr[2] = v0 ^ 1u;
}

// Calls 0x08B09F98 three times with this, this + 0x28 and this + 0x50; then tail call 0x088D43E4 with this.
void func_088D446C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 node = self;
    for (u32 n = 0u; n < 3u; ++n) {
        call(rt, ctx, 0x08B09F98u, {node});
        node += 0x28u;
    }
    tail_call(ctx, 0x088D43E4u, {self});
}

// Calls 0x088FDF98 with (this, value); stores value at *this in its delay slot; then if value != 0, tail call 0x088FDFE0 with value,
// else return the callee's v0.
void func_088FE03C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    call(rt, ctx, 0x088FDF98u, {self, value});
    put(rt, self, value);
    if (value != 0u) tail_call(ctx, 0x088FDFE0u, {value});
}

// flag = a1 & 0xFF; if flag, call 0x08917AF0 (this, flag); if it answered nonzero, store 1 at owner+0x2107 and return 1.
// Otherwise store 0 at owner+0x2107 and return the owner (owner = *(this+0xC)).
void func_08917B08(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    if (flag != 0u) {
        call(rt, ctx, 0x08917AF0u, {self, flag});
        if (ctx.gpr[2] != 0u) {
            rt.memory().store8(word(rt, self + 0xCu) + 0x2107u, 1u);
            ctx.gpr[2] = 1u;
            return;
        }
    }
    const u32 owner = word(rt, self + 0xCu);
    rt.memory().store8(owner + 0x2107u, 0u);
    ctx.gpr[2] = owner;
}

// Object with a vtable word (0x08CC5BF0) at +0; child at +0x1F8: its vtable slot 4 is called with the child, the field cleared;
// then tail call 0x08925FF8 with this.
void func_0891A568(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[2] = 0x08CC5BF0u;
    put(rt, self, 0x08CC5BF0u);
    const u32 child = word(rt, self + 0x1F8u);
    if (child != 0u) {
        call(rt, ctx, word(rt, word(rt, child) + 0x4u), {child});
        put(rt, self + 0x1F8u, 0u);
    }
    tail_call(ctx, 0x08925FF8u, {self});
}

// The same as func_0891A568 with the vtable word 0x08CC5C48.
void func_0891A6D4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[2] = 0x08CC5C48u;
    put(rt, self, 0x08CC5C48u);
    const u32 child = word(rt, self + 0x1F8u);
    if (child != 0u) {
        call(rt, ctx, word(rt, word(rt, child) + 0x4u), {child});
        put(rt, self + 0x1F8u, 0u);
    }
    tail_call(ctx, 0x08925FF8u, {self});
}

// Walks the arguments: cursor = word(a1) + 8, then a2 times cursor += byte at cursor+2; call 0x08AC26D4 (this, cursor); v0 = this.
void func_08AC26E4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    u32 cursor = word(rt, ctx.gpr[5]) + 0x8u;
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    for (s32 i = 0; i < count; ++i) cursor += rt.memory().load8(cursor + 0x2u);
    call(rt, ctx, 0x08AC26D4u, {self, cursor});
    ctx.gpr[2] = self;
}

// A 0x410-byte buffer on the frame: call 0x08AD8754 (argument, buffer) repeatedly; count the answers while their first byte
// is nonzero, and stop when both the answer's byte and the buffer's first byte are zero. v0 = count.
void func_08AD87B4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x410u);
    const u32 buffer = ctx.gpr[29];
    u32 argument = ctx.gpr[4];
    u32 count = 0u;
    for (;;) {
        const u32 answer = call(rt, ctx, 0x08AD8754u, {argument, buffer});
        argument = answer;
        if (rt.memory().load8(answer) != 0u) {
            ++count;
            continue;
        }
        if (rt.memory().load8(buffer) == 0u) {
            ctx.gpr[2] = count;
            return;
        }
        ++count;
    }
}

#define NEW1568(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1568::name, gt::ReturnKind::kind)
NEW1568(0x08ABB204u, func_08ABB204, Word);
NEW1568(0x08ABB504u, func_08ABB504, Word);
NEW1568(0x08ABB88Cu, func_08ABB88C, Word);
NEW1568(0x08ABB8D8u, func_08ABB8D8, Word);
NEW1568(0x08ABC1D4u, func_08ABC1D4, Word);
NEW1568(0x08ABC220u, func_08ABC220, Word);
NEW1568(0x08ABC26Cu, func_08ABC26C, Word);
NEW1568(0x08ABC6E8u, func_08ABC6E8, Word);
NEW1568(0x08AC26E4u, func_08AC26E4, Word);
NEW1568(0x08AD87B4u, func_08AD87B4, Word);
NEW1568(0x08B0D46Cu, func_08B0D46C, Word);
NEW1568(0x08B1A174u, func_08B1A174, Word);
NEW1568(0x08B2082Cu, func_08B2082C, Word);
NEW1568(0x08B3EA3Cu, func_08B3EA3C, Word);
NEW1568(0x088AA6F4u, func_088AA6F4, Word);
NEW1568(0x088D446Cu, func_088D446C, Word);
NEW1568(0x088FE03Cu, func_088FE03C, Word);
NEW1568(0x08917B08u, func_08917B08, Word);
NEW1568(0x0891A568u, func_0891A568, Word);
NEW1568(0x0891A6D4u, func_0891A6D4, Word);

}  // namespace
}  // namespace new_batch_main_1568
}  // namespace gt::game
