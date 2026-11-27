// New batch, main session 1013: functions the main-session drafters skipped twice or that mismatched the original in the
// gate. Hand-written structured C from `tools.disasm`; v0 is the original's on every exit path (the call helpers keep
// the callee's v0 / v1 / f0, so a callee that never writes v0 hands the caller's value on, as after the `jal`). No names:
// none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1013 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats = {}) {
    return call_full(rt, ctx, target, arguments, floats).f0;
}
// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }
// `lh` then `sll v0, v0, 4`: the signed half as a byte offset scaled by 16.
u32 scaled_half_at(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<std::int16_t>(rt.memory().load16(address))) << 4;
}

// `subu` then `sra 2` over a pair of vector pointers at the two addresses: the element count.
u32 element_count(Runtime &rt, u32 begin, u32 end) {
    return static_cast<u32>(static_cast<s32>(word(rt, end) - word(rt, begin)) >> 2);
}
// The same for the begin/end words at base+0x14 / base+0x18.
u32 element_count(Runtime &rt, u32 base) { return element_count(rt, base + 0x14u, base + 0x18u); }

// 0x08AC8110 (a0 = object): the stream pump. It registers the channel at a0+0x438 with the buffer descriptor
// {a0+0x640, 0x20000}, reads one record per round from the reader at a0+0x5BC, hands it to the consumer created at
// a0+0x570, releases it, and drains the task at a0+0x574; the round repeats while a0+0x564 says so. v0 = 1.

// 0x088C2A9C (a0 = object): the per-frame state step of the object at a0. s1 = the actor at a0+0x24; the flags halfword
// at s1+2 selects the state byte stored at s1+0xA5 (a jump table of ten entries at 0x08C89E24), and f21/f22 carry the
// frame's elapsed time. v0 = the flags that are left (a1 & 0xFFAF).

// 0x088AD9E0 (a0 = object): the per-frame pass over the eight 0x44-byte slots at a0+0x44*k (flags word +0x84, node at
// +0x98, the frame's point at +0x70 and chain at +0x80). Each live slot walks its node path, normalises the segment
// direction, and accumulates into the object's +0x48. v0 = 8 (the loop counter's last value).

// 0x0891E5BC (a0 = result, a1 = object, a2 = name, a3 = owner): compose the object's placement from the two things the
// name resolves to (a2/a3 through 0x088DA1C8 / 0x088DA170 / 0x088DCD90), pick a 72-byte pose record out of the table at
// 0x08CF213C by the object's side (+0x218 / +0x224), and hand it to 0x0891E2A8. v0 = a0.

// 0x08A54B50 (a0 = result, a1 = container, a2 = owner): when the container's +0x58 is set, write its one entry (the
// name at owner+0x1C/0x18 and the object at owner+0x14 through its virtual +0x120) with 0x08A950D0 and publish it;
// otherwise run over the [container+0x3C, container+0x44) ring, building one 0x18 record per element. v0 = a0.

// 0x08AB73CC (a0 = object, a1 = list): with a0+0x10 set, publish the two handles the object resolves to (+0x14/+0x18)
// and then each element of its vector at +0x14 through the element's virtual +0x18; with it clear, build the vector from
// the object first and publish that. v0 = the last publish (a0+0x10 set) or the vector's element count / free result.

} // namespace
} // namespace new_batch_main_1013
} // namespace gt::game

#define NEW_MAIN1013(address, name, kind) \
    GT_DECOMP(address, gt::game::new_batch_main_1013::name, gt::ReturnKind::kind)
