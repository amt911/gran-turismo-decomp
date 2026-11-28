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
namespace new_batch_main_1877 {
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
void func_088AD9E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    const u32 aim = self + 0x3Cu;     // +0x38(sp)
    const u32 base = self + 0x8u;     // +0x30(sp)
    u32 chain = self + 0x80u;
    u32 point = self + 0x70u;
    u32 slot = self;
    u32 node = 0u;
    for (u32 round = 0u;;) {
        if ((word(rt, slot + 0x84u) & 1u) != 0u) {
            const u32 flags = slot + 0x84u;
            node = word(rt, slot + 0x98u);
            u32 next = node + 0x10u;
            if (signed_half_at(rt, node + 0xCu) < 0) next += scaled_half_at(rt, node + 0xCu);
            const u32 direction = chain + 0x10u;
            const u32 forward = point + 4u;
            put_float(rt, frame.at(0xCu), float_at(rt, point + 8u) + float_at(rt, node + 8u));
            put_float(rt, frame.at(8u), float_at(rt, point + 4u) - float_at(rt, node));
            float f20 = call_float(rt, ctx, 0x08B4ABCCu, {frame.at(8u), direction});
            bool have_segment = false;
            if (!(f20 < 0.0f)) {
                const float near = float_at(rt, next);
                const float far = float_at(rt, next + 8u);
                const float cross = psp_mul(near - float_at(rt, node), float_at(rt, direction)) -
                                    psp_mul(far - float_at(rt, node + 8u), float_at(rt, direction + 4u));
                if (cross < f20) {
                    u32 other = next + 0x10u;
                    if (signed_half_at(rt, next + 0xCu) < 0) other += scaled_half_at(rt, next + 0xCu);
                    put_float(rt, frame.at(0x10u), float_at(rt, other) - near);
                    put_float(rt, frame.at(0x14u), far - float_at(rt, other + 8u));
                    node = next;
                    have_segment = true;
                }
            } else {
                const u32 link = word(rt, slot + 0x98u);
                u32 other = link - 0x10u;
                if (signed_half_at(rt, link + 0xCu) > 0) other += scaled_half_at(rt, link + 0xCu);
                put_float(rt, frame.at(0x14u), float_at(rt, other + 8u) - float_at(rt, node + 8u));
                put_float(rt, frame.at(0x10u), float_at(rt, node) - float_at(rt, other));
                node = other;
                have_segment = true;
            }
            if (have_segment) {
                const float norm = std::sqrt(call_float(rt, ctx, 0x08B4A708u, {frame.at(0x10u)}));
                call_float(rt, ctx, 0x08B4A560u, {frame.at(0x10u)}, {1.0f / norm});
                put_float(rt, chain + 0x10u, float_at(rt, frame.at(0x10u)));
                put_float(rt, direction + 4u, float_at(rt, frame.at(0x14u)));
                put(rt, slot + 0x98u, node);
                put_float(rt, frame.at(0xCu), float_at(rt, forward + 4u) + float_at(rt, node + 8u));
                put_float(rt, frame.at(8u), float_at(rt, point + 4u) - float_at(rt, node));
                f20 = call_float(rt, ctx, 0x08B4ABCCu, {frame.at(8u), frame.at(0x10u)});
            }
            bool clear = false;
            const float reach = call_float(rt, ctx, 0x08B4ABECu, {aim, direction});
            if (0.0f <= reach) {
                const float ahead = call_float(rt, ctx, 0x08B4ABECu, {frame.at(8u), direction});
                if (0.0f < ahead) {
                    put_float(rt, frame.at(0x14u), psp_mul(f20, float_at(rt, direction + 4u)) -
                                                        float_at(rt, node + 8u) - float_at(rt, forward + 4u));
                    put_float(rt, frame.at(0x10u), psp_mul(f20, float_at(rt, direction)) + float_at(rt, node) -
                                                        float_at(rt, forward));
                    call(rt, ctx, 0x08B4AC0Cu, {base, base, frame.at(0x10u)});
                    put_float(rt, frame.at(0x1Cu), float_at(rt, aim + 4u));
                    put_float(rt, frame.at(0x18u), float_at(rt, self + 0x3Cu));
                    const float length = call_float(rt, ctx, 0x08B4ABCCu, {direction, aim});
                    put_float(rt, frame.at(0u), float_at(rt, direction));
                    put_float(rt, frame.at(4u), float_at(rt, direction + 4u));
                    call_float(rt, ctx, 0x08B4A560u, {frame.at(0u)}, {length});
                    put_float(rt, self + 0x3Cu, float_at(rt, frame.at(0u)));
                    put_float(rt, aim + 4u, float_at(rt, frame.at(4u)));
                    call(rt, ctx, 0x08B4AC30u, {frame.at(0x18u), frame.at(0x18u), aim});
                    put_float(rt, frame.at(0x24u), float_at(rt, forward + 4u));
                    put_float(rt, frame.at(0x20u), float_at(rt, forward));
                    call(rt, ctx, 0x08B4AC30u, {frame.at(0x20u), frame.at(0x20u), base});
                    const float energy = float_at(rt, self + 0x48u);
                    const float ahead_gain = call_float(rt, ctx, 0x08B4ABECu, {frame.at(0x20u), frame.at(0x18u)});
                    const float scale = psp_mul(ahead_gain, float_at(rt, self + 0x42Cu)) *
                                        (-1.0f / float_at(rt, self + 0x430u));
                    put_float(rt, self + 0x48u, energy + scale);
                } else {
                    clear = true;
                }
            } else {
                clear = true;
            }
            if (clear) {
                const u32 live = word(rt, flags);
                put(rt, flags, live & ~1u);
            }
        }
        ++round;
        chain += 0x44u;
        point += 0x44u;
        slot += 0x44u;
        if (round == 8u) break;
    }
    ctx.gpr[2] = 8u;
}

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
} // namespace new_batch_main_1877
} // namespace gt::game

#define NEW_MAIN1013(address, name, kind) \
    GT_DECOMP(address, gt::game::new_batch_main_1877::name, gt::ReturnKind::kind)
NEW_MAIN1013(0x088AD9E0u, func_088AD9E0, Word);
