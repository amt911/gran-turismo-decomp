// New batch main-1593 (draft, unverified): structured C from tools.disasm. Skipped on purpose: the refcount releases with
// ll/sc, the FPU loops, the shared_ptr assignment and the functions not yet read closely.
// Guard pattern (static local init): the guard byte must be 0 and the acquire call must return nonzero before the init runs,
// then the release call follows the init.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1593 {
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
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The callee-saved registers a function keeps values in: stored into its frame where its prologue stores them (a callee can
// read the stack below its caller's frame), given back when it returns.
class Saved {
public:
    Saved(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, std::initializer_list<std::pair<unsigned, u32>> slots)
        : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
        for (const auto &[reg, offset] : slots) put(rt, frame.at(offset), ctx.gpr[reg]);
    }
    ~Saved() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    Saved(const Saved &) = delete;
    Saved &operator=(const Saved &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// Node insertion (shared by 08B947FC and 08C4EA54): allocate, order the two keys, link through 0x08B5F2B8, bump the count.
// The ordering: sltu v1 = (second < first); movz v0 = sp when v1 == 0, so the chosen slot is sp+4 when second < first.
void node_insert(Runtime &rt, AllegrexContext &ctx, u32 allocate, u32 compare, u32 construct) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame,
                {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {19u, 0x1Cu}, {20u, 0x20u}, {21u, 0x24u}, {22u, 0x28u}, {31u, 0x2Cu}});
    const u32 self = ctx.gpr[4];
    const u32 side = ctx.gpr[5];
    const u32 other = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    u32 flag = 0u;
    if (side != 0u || other == self + 4u) {
        flag = 1u;
    } else {
        const u32 left = word(rt, holder);
        const u32 right = word(rt, other + 0x10u);
        const u32 first = word(rt, left - 0xCu);
        const u32 second = word(rt, right - 0xCu);
        put(rt, ctx.gpr[29], first);
        put(rt, ctx.gpr[29] + 4u, second);
        const u32 chosen = second < first ? ctx.gpr[29] + 4u : ctx.gpr[29];
        const u32 distance = first - second;
        const u32 answer = call(rt, ctx, compare, {left, right, word(rt, chosen)});
        const u32 measured = answer != 0u ? answer : distance;
        flag = static_cast<s32>(measured) < 0 ? 1u : 0u;
    }
    const u32 node = call(rt, ctx, allocate, {0x18u});
    const u32 payload = node + 0x10u;
    if (payload != 0u) {
        call(rt, ctx, construct, {payload, holder});
        call(rt, ctx, construct, {node + 0x14u, holder + 4u});
    }
    call(rt, ctx, 0x08B5F2B8u, {flag, node, other, self + 4u});
    put(rt, self + 0x14u, word(rt, self + 0x14u) + 1u);
    ctx.gpr[2] = node;
}

}  // namespace

// 0x08C10EE8: copy the 0x60-byte elements [a0, a1) forward to a2; the vtable 0x08CCFF88 at 0, then the sub-blocks through 0x0896418C.
void func_08C10EE8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{22u, 0x18u}, {16u, 0x0u}, {31u, 0x20u}, {23u, 0x1Cu}, {21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu},
                                 {18u, 0x8u}, {17u, 0x4u}});
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    for (u32 element = ctx.gpr[4]; element != end; element += 0x60u, destination += 0x60u) {
        if (destination == 0u) continue;
        put(rt, destination + 4u, word(rt, element + 4u));
        put(rt, destination + 8u, word(rt, element + 8u));
        for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, destination + offset, word(rt, element + offset));
        const u32 first = destination + 0x20u;
        const u32 second = destination + 0x40u;
        const u32 source_first = element + 0x20u;
        const u32 source_second = element + 0x40u;
        put(rt, destination, 0x08CCFF88u);
        call(rt, ctx, 0x0896418Cu, {first, source_first});
        put(rt, first + 0x10u, word(rt, source_first + 0x10u));
        call(rt, ctx, 0x0896418Cu, {second, source_second});
        put(rt, second + 0x10u, word(rt, source_second + 0x10u));
    }
    ctx.gpr[2] = destination;
}

void func_08B947FC(Runtime &rt, AllegrexContext &ctx) { node_insert(rt, ctx, 0x08B35DA0u, 0x08B36170u, 0x08B9432Cu); }
void func_08C4EA54(Runtime &rt, AllegrexContext &ctx) { node_insert(rt, ctx, 0x08B62344u, 0x08B36170u, 0x08C7464Cu); }

// 0x0892BD4C: constructor. Vtable 0x08CC6740 at self, a 0x9C node linked at self+0x260, then a guarded static (vtable 0x08CC6768).
// Returns -1 (the node's word at +0x98).

// 0x0887F600: constructor. Vtables 0x08CC1130 at self and 0x08CC116C at self+0x70; a 0x54 member; a guarded static
// (vtable 0x08CC1198); ends in a tail call to 0x0886AAA4 (self, member+0x28).

// 0x088FF120: constructor of a 0x64 record: word 0x30 = -1, the rest zero except the 0x20-byte block at +0x44 (memset),
// vtable 0x08CC5370 in a guarded static; ends in a tail call to 0x08B28DCC (self+0x10).

// 0x08897C94: allocates 0xB8 bytes; the 0x8C-byte block at +0x8C is initialised by 0x0894BF50/0x08B36018/0x0894BEFC; a guarded
// static (vtable 0x08CC1FF0); ends in a tail call to 0x08896E30 (self+8).

// 0x0887DD3C: constructor. Vtable 0x08CC0F88 on a 0x28 object, two-word tables, a 0x10 holder (vtable 0x08CC0FC8) and a
// registration call 0x08B8E380 (self+0x5C, object, object). Returns the callee's v0.

// 0x088711E8: constructor. Vtable 0x08CBF948 at self, a 0xC0 object (zeroed fields, three word slots set to 0x08D752A4 and
// three bytes at +0x98 cleared), a guarded static (vtable 0x08CBF970) stored at self+0x10. v0 follows the asm on every exit.

#define NEW1593(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1593::name, gt::ReturnKind::kind)
NEW1593(0x08C10EE8u, func_08C10EE8, Word);
NEW1593(0x08B947FCu, func_08B947FC, Word);
NEW1593(0x08C4EA54u, func_08C4EA54, Word);

}  // namespace new_batch_main_1593
}  // namespace gt::game
