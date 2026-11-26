// Main-session batch 1122 (draft, unverified). Structured C from tools.disasm; no gate has run on these.
// Skipped (not drafted): 08ADFA18, 08ADFA70 and 08A5BD34 (varargs spills), 08B28FDC and 08A87E8C (float arguments),
// 08ABDEEC (vtable dispatch with a variadic tail; no input shape can make the vtable slot valid).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1122 {
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

// Allocate a block (0x08A5BC20, size and align 4), build the value in it with `build`, store the result in a local
// and pass &local with a0 (the object) to `store`; v0 is that last call's result.
void alloc_build_store(Runtime &rt, AllegrexContext &ctx, u32 size, u32 build, u32 argument, u32 store) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {size, 4u});
    const u32 built = call(rt, ctx, build, {block, argument});
    put(rt, frame.at(0u), built);
    call(rt, ctx, store, {self, frame.at(0u)});
}

// Build a text object in a local (0x08B37640 takes the value as a third argument), read the owner's word at +0x20
// before the measure call, measure the local (0x08B35F60) and hand the owner, the local and the measure to 0x08BEED1C.
void text_value_then_pass(Runtime &rt, AllegrexContext &ctx, u32 text, u32 value) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const u32 temp = frame.at(0u);
    call(rt, ctx, 0x08B37640u, {temp, text, value});
    const u32 owner = word(rt, self + 0x20u);
    const u32 measured = call(rt, ctx, 0x08B35F60u, {temp});
    call(rt, ctx, 0x08BEED1Cu, {owner, temp, measured});
    ctx.gpr[2] = self;
}

} // namespace

void func_08AF74AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x50u);
    const u32 count = static_cast<std::uint16_t>(rt.memory().load16(self + 0x16u));
    const u32 begin = word(rt, self + 0x1Cu);
    const u32 end = begin + (count << 5);
    const u32 vtable = 0x08AF7374u;
    put(rt, frame.at(0x00u), vtable);
    put(rt, frame.at(0x04u), 0u);
    rt.memory().store8(frame.at(0x08u), static_cast<std::uint8_t>(flag));
    put(rt, frame.at(0x18u), vtable);
    put(rt, frame.at(0x1Cu), 0u);
    rt.memory().store8(frame.at(0x20u), static_cast<std::uint8_t>(flag));
    call(rt, ctx, 0x08C40088u, {frame.at(0x30u), begin, end, frame.at(0u)});
}


// The member-init then tail shape: first call on this, vtable word stored at this, the member at +0x18 built from a2,
// then a tail call with this + 0x1C and a3.
void func_08A7B030(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A5CFB8u, {self});
        put(rt, self, 0x08CD2758u);
        call(rt, ctx, 0x08A76ABCu, {self + 0x18u, a2});
    }
    tail_call(ctx, 0x08A5D424u, {self + 0x1Cu, a3});
}

void func_08A805AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08A76ACCu, {self});
        put(rt, self, 0x08CCFA70u);
        call(rt, ctx, 0x08A76ABCu, {self + 0x14u, a1});
    }
    tail_call(ctx, 0x08A802CCu, {self + 0x18u, a2});
}

void func_08A9E2A4(Runtime &rt, AllegrexContext &ctx) { text_value_then_pass(rt, ctx, 0x08CB26FCu, ctx.gpr[5]); }
void func_08A9E428(Runtime &rt, AllegrexContext &ctx) {
    text_value_then_pass(rt, ctx, 0x08CB2700u, static_cast<u32>(static_cast<std::int8_t>(ctx.gpr[5])));
}
void func_08A9E484(Runtime &rt, AllegrexContext &ctx) { text_value_then_pass(rt, ctx, 0x08CB2704u, ctx.gpr[5] & 0xFFu); }
void func_08A9E4E0(Runtime &rt, AllegrexContext &ctx) {
    text_value_then_pass(rt, ctx, 0x08CB2700u, static_cast<u32>(static_cast<std::int16_t>(ctx.gpr[5])));
}
void func_08A9E53C(Runtime &rt, AllegrexContext &ctx) { text_value_then_pass(rt, ctx, 0x08CB2704u, ctx.gpr[5] & 0xFFFFu); }
void func_08A9E598(Runtime &rt, AllegrexContext &ctx) { text_value_then_pass(rt, ctx, 0x08CB2700u, ctx.gpr[5]); }
void func_08A9E5F4(Runtime &rt, AllegrexContext &ctx) { text_value_then_pass(rt, ctx, 0x08CB2704u, ctx.gpr[5]); }


void func_08B1965C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x10u);
    const u32 found = call(rt, ctx, 0x08B1935Cu, {a1, a2});
    call(rt, ctx, 0x08B195ECu, {self, a1, found, a3});
    ctx.gpr[2] = self;
}

void func_08B2CABC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 lock = self + 0x58u;
    call(rt, ctx, 0x08B34D40u, {lock});
    const u32 previous = word(rt, self + 0x54u);
    put(rt, self + 0x54u, value);
    call(rt, ctx, 0x08B34D80u, {lock});
    ctx.gpr[2] = previous;
}



} // namespace new_batch_main_1122
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1122::name, gt::ReturnKind::kind)
NEWM(0x08AF74ACu, func_08AF74AC, Word);
NEWM(0x08A7B030u, func_08A7B030, Word);
NEWM(0x08A805ACu, func_08A805AC, Word);
NEWM(0x08A9E2A4u, func_08A9E2A4, Word);
NEWM(0x08A9E428u, func_08A9E428, Word);
NEWM(0x08A9E484u, func_08A9E484, Word);
NEWM(0x08A9E4E0u, func_08A9E4E0, Word);
NEWM(0x08A9E53Cu, func_08A9E53C, Word);
NEWM(0x08A9E598u, func_08A9E598, Word);
NEWM(0x08A9E5F4u, func_08A9E5F4, Word);
NEWM(0x08B1965Cu, func_08B1965C, Word);
NEWM(0x08B2CABCu, func_08B2CABC, Word);
