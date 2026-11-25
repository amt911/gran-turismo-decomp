// New batch 902 (main session, draft-only): constructors and float helpers the gate could not pose inputs for, written by hand
// from `tools.disasm`; role words describe access shapes, none is a game name. Input shapes live in
// shapes_main902.txt (config/synth_shapes format): one line per function, asm evidence next to it.
//
// Every call passes the argument registers the original sets before the jal (`call_set`); the delay slot of a `jal` runs
// before the call; v0 is set as the original leaves it on every path; frames that callees read are reproduced
// (`GuestFrame`, offsets as in the original), the dead stack is not.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_902 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// v0 / v1 / f0 of the callee are what the next instruction sees, like the original's registers.
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// The same with float argument registers (f12-f15) as well.
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
// `ins rt, rs, pos, size`: the size bits at pos of `base` become the low bits of `value`, the rest stand.
inline u32 ins(u32 base, u32 value, unsigned position, unsigned size) {
    const u32 low = size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u);
    const u32 mask = low << position;
    return (base & ~mask) | ((value << position) & mask);
}
// Signed half of a word: `srl v, x, 31; addu v, v, x; sra v, v, 1` — a division by two truncating toward zero.
inline s32 half_word(s32 value) { return value / 2; }

// 0x089F3A70: constructor — vtable words, eight sub-object initialisers and two 16.0f fields; v0 is the last initialiser's.
void func_089F3A70(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x089E20D0u, {self});
    put(rt, self, 0x08CC8288u);
    put(rt, self + 0x38u, 0x08CC84C4u);
    const u32 shared = 0x08D74FD0u + 0xCu;
    put(rt, self + 0x248u, shared);
    put(rt, self + 0x22Cu, shared);
    put(rt, self + 0x230u, shared);
    put(rt, self + 0x234u, shared);
    put(rt, self + 0x238u, shared);
    put(rt, self + 0x23Cu, shared);
    put(rt, self + 0x240u, shared);
    put(rt, self + 0x244u, shared);
    for (u32 offset = 0x24Cu; offset <= 0x264u; offset += 4u) {
        (void)call_set(rt, ctx, 0x089E4C14u, {self + offset, 0x08CF6ED8u});
    }
    (void)call_set(rt, ctx, 0x089E4C14u, {self + 0x268u, 0x08CF6ED8u});
    put_float(rt, self + 0x270u, 16.0f);
    put(rt, self + 0x274u, 0u);
    put_float(rt, self + 0x26Cu, 16.0f);
}

// 0x089AC110: four corner calls into 0x089AC0A0 (f12-f15), the last one a tail call; a0 is the same object throughout.
void func_089AC110(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const float in12 = ctx.fpr[12], in13 = ctx.fpr[13], in14 = ctx.fpr[14], in15 = ctx.fpr[15];
    const float two = 2.0f, four = 4.0f;
    const float low_x = in12 - two;
    const float low_y = in13 - two;
    const float high_x = in14 + four;
    const float high_y = in15 + four;
    (void)call_setf(rt, ctx, 0x089AC0A0u, {self}, {low_x, low_y, two, high_y});
    (void)call_setf(rt, ctx, 0x089AC0A0u, {self}, {low_x, low_y, high_x, two});
    (void)call_setf(rt, ctx, 0x089AC0A0u, {self}, {low_x, in13 + in15, high_x, two});
    ctx.fpr[12] = in12 + in14;
    ctx.fpr[13] = low_y;
    ctx.fpr[14] = two;
    ctx.fpr[15] = high_y;
    tail_call(ctx, 0x089AC0A0u, {self});
}

// 0x0892EEC0: clears a struct (three memset calls, zeros around them); v0 is the last memset's answer.
void func_0892EEC0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0u);
    for (u32 offset = 0x4u; offset <= 0x34u; offset += 4u) put(rt, self + offset, 0u);
    (void)call_set(rt, ctx, 0x08B36018u, {self + 0x38u, 0u, 0x1B0u});
    (void)call_set(rt, ctx, 0x08B36018u, {self + 0x1E8u, 0u, 0x60u});
    const u32 block258 = self + 0x258u;
    const u32 block248 = self + 0x248u;
    put(rt, self + 0x258u, 0u);
    put(rt, self + 0x248u, 0u);
    put(rt, block258 + 0x34u, 0u);
    for (u32 offset = 0x4u; offset <= 0x30u; offset += 4u) put(rt, block258 + offset, 0u);
    put(rt, block248 + 0xCu, 0u);
    put(rt, block248 + 0x4u, 0u);
    put(rt, block248 + 0x8u, 0u);
    (void)call_set(rt, ctx, 0x08B36018u, {self + 0x290u, 0u, 0x70u});
    const u32 block300 = self + 0x300u;
    put(rt, self + 0x300u, 0u);
    put(rt, block300 + 0x14u, 0u);
    for (u32 offset = 0x4u; offset <= 0x10u; offset += 4u) put(rt, block300 + offset, 0u);
}

// 0x08922BFC: constructor — five sub-objects at +0x1C..+0xCC, each initialised then given a name constant; the last is a tail call.
void func_08922BFC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 third = ctx.gpr[6];
    (void)call_set(rt, ctx, 0x08925FE4u, {self, ctx.gpr[5]});
    put(rt, self, 0x08CC5EC0u);
    for (const u32 offset : {0x1Cu, 0x48u, 0x74u, 0xA0u, 0xCCu}) {
        (void)call_set(rt, ctx, 0x08926398u, {self + offset});
    }
    put(rt, self + 0x8u, third);
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x1Cu, 0x08CDDD08u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x48u, 0x08CDDCD8u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x74u, 0x08CDDCF0u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0xA0u, 0x08CDDCF0u, 1u});
    tail_call(ctx, 0x08926488u, {self + 0xCCu, 0x08CDDCF0u, 1u});
}

// 0x08A064A0: constructor — a vtable, an 8-byte allocation stored three times, a stack object built by two callees and then
// handed on; v0 ends as the field at +0xA8.
void func_08A064A0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x90u);
    const u32 self = ctx.gpr[4];
    const u32 temp = frame.at(0x10u);
    (void)call_set(rt, ctx, 0x089D8E2Cu, {self});
    put(rt, self, 0x08CC71A0u);
    put(rt, self + 0x38u, 0x08CC738Cu);
    put(rt, self + 0xA8u, 0u);
    put(rt, self + 0xA8u + 0x4u, 0u);
    put(rt, self + 0xA8u + 0x8u, 0u);
    const u32 block = call_set(rt, ctx, 0x08A5BC20u, {0u, 4u});
    put(rt, self + 0xA8u, block);
    put(rt, self + 0xA8u + 0x4u, block);
    put(rt, self + 0xA8u + 0x8u, block);
    (void)call_set(rt, ctx, 0x089D76A8u, {temp});
    put(rt, frame.at(0x60u), 0xFFFFFFFFu);
    put(rt, frame.at(0x40u), 0xFFFFFFFFu);
    put_float(rt, frame.at(0x50u), 1.0f);
    put(rt, frame.at(0x10u), 0x08CCFF88u);
    put_float(rt, frame.at(0x3Cu), 1.0f);
    put_float(rt, frame.at(0x38u), 1.0f);
    put_float(rt, frame.at(0x34u), 1.0f);
    put_float(rt, frame.at(0x30u), 1.0f);
    put_float(rt, frame.at(0x5Cu), 1.0f);
    put_float(rt, frame.at(0x58u), 1.0f);
    put_float(rt, frame.at(0x54u), 1.0f);
    (void)call_set(rt, ctx, 0x08BE78E8u, {block, 0u, temp, frame.at(0u)});
    put(rt, frame.at(0x10u), 0x08CCFF88u);
    (void)call_set(rt, ctx, 0x089D6684u, {temp});
    put(rt, self + 0xA8u + 0x4u, word(rt, self + 0xA8u));
    put(rt, self + 0xB4u, 0u);
    ctx.gpr[2] = word(rt, self + 0xA8u);
}

// 0x089F9578: the same shape with three stack objects (+0x04, +0x24, +0x34), four float fills and two releasing calls.
void func_089F9578(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    const u32 first = frame.at(0x04u);
    const u32 second = frame.at(0x24u);
    const u32 third = frame.at(0x34u);
    (void)call_set(rt, ctx, 0x089D8E2Cu, {self});
    put(rt, self, 0x08CC7C38u);
    put(rt, self + 0x38u, 0x08CC7E24u);
    put(rt, self + 0xA8u, 0u);
    put(rt, self + 0xA8u + 0x4u, 0u);
    put(rt, self + 0xA8u + 0x8u, 0u);
    const u32 block = call_set(rt, ctx, 0x08A5BC20u, {0u, 4u});
    put(rt, self + 0xA8u, block);
    put(rt, self + 0xA8u + 0x4u, block);
    put(rt, self + 0xA8u + 0x8u, block);
    (void)call_set(rt, ctx, 0x089D76A8u, {first});
    put(rt, first, 0x08CCFC90u);
    (void)call_set(rt, ctx, 0x089FA09Cu, {second});
    (void)call_set(rt, ctx, 0x089FA09Cu, {third});
    (void)call_set(rt, ctx, 0x08BE834Cu, {block, 0u, first, frame.at(0u)});
    put(rt, first, 0x08CCFC90u);
    (void)call_set(rt, ctx, 0x089F9B74u, {third});
    (void)call_set(rt, ctx, 0x089F9B74u, {second});
    (void)call_set(rt, ctx, 0x089D6684u, {first});
    put(rt, self + 0xA8u + 0x4u, word(rt, self + 0xA8u));
    put(rt, self + 0xB4u, 0u);
    ctx.gpr[2] = word(rt, self + 0xA8u);
}

// 0x088F7558: sizes a rectangle into a2 from a1[4] and a3's scale fields, then writes a1 as its position plus a scaled step.
void func_088F7558(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 position = ctx.gpr[5];
    const u32 size = ctx.gpr[6];
    const u32 scale = ctx.gpr[7];
    const float step_scale = ctx.fpr[12];
    const float level = float_at(rt, position + 0x4u);
    float offset = 0.0f;
    if (0.1f <= level) {
        const float denominator = float_at(rt, scale + 0x1Cu);
        (void)call_setf(rt, ctx, 0x08B67720u, {}, {level / denominator, float_at(rt, scale + 0x14u)});
        offset = float_at(rt, scale + 0x18u) * ctx.fpr[0];
    }
    const float scale_z = float_at(rt, scale + 0x28u);
    const float scale_x = float_at(rt, scale + 0x24u);
    float x = float_at(rt, size);
    float z = float_at(rt, size + 0x8u);
    const float sx = offset * scale_x;
    const float sz = offset * scale_z;
    const float zone = 0.15f;
    float y = float_at(rt, size + 0x4u);
    z = (z - sz) * zone;
    x = (x - sx) * zone;
    y = y * zone;
    put_float(rt, size + 0x4u, y);
    const float y_step = y * step_scale;
    const float sum_z = sz + z;
    const float sum_x = sx + x;
    put_float(rt, size + 0x8u, sum_z);
    const float x_step = sum_x * step_scale;
    const float z_step = sum_z * step_scale;
    put_float(rt, size, sum_x);
    const float base_y = float_at(rt, position + 0x4u);
    const float base_z = float_at(rt, position + 0x8u);
    const float base_x = float_at(rt, position);
    const float out_y = base_y + y_step;
    const float out_z = base_z + z_step;
    const float out_x = base_x + x_step;
    put_float(rt, position + 0xCu, 1.0f);
    put_float(rt, position, out_x);
    put_float(rt, position + 0x4u, out_y);
    put_float(rt, position + 0x8u, out_z);
}

// 0x0899DBA8: constructor — vtable, a flag word at +0x13C, float setup, four sub-objects and a two-float stack pair; v0 is
// the +0x110 word it stores last.
void func_0899DBA8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x089A2528u, {self});
    const u32 flags = ins(word(rt, self + 0x13Cu) | 3u, 0u, 2u, 5u);
    put(rt, self, 0x08CCB5F8u);
    put(rt, self + 0x38u, 0x08CCB850u);
    put(rt, self + 0x13Cu, flags);
    put_float(rt, self + 0x130u, 100.0f);
    put_float(rt, self + 0x134u, 1.0f);
    put_float(rt, self + 0x120u, 0.0f);
    put_float(rt, self + 0x124u, 0.0f);
    put_float(rt, self + 0x128u, 100.0f);
    put_float(rt, self + 0x12Cu, 0.0f);
    put_float(rt, self + 0x138u, 0.0f);
    put_float(rt, self + 0x140u, 0.0f);
    put_float(rt, self + 0x140u + 0x4u, 0.0f);
    put_float(rt, self + 0x140u + 0x8u, 0.0f);
    put_float(rt, self + 0x150u, 0.0f);
    put_float(rt, self + 0x14Cu, 0.0f);
    put(rt, self + 0x154u, 0u);
    put(rt, self + 0x158u, 0u);
    put(rt, self + 0x15Cu, 0u);
    (void)call_set(rt, ctx, 0x08A76AA0u, {self + 0x160u});
    (void)call_set(rt, ctx, 0x08A76AA0u, {self + 0x164u});
    (void)call_set(rt, ctx, 0x08A76AA0u, {self + 0x168u});
    (void)call_set(rt, ctx, 0x08A76AA0u, {self + 0x16Cu});
    put_float(rt, frame.at(0x4u), 256.0f);
    put_float(rt, frame.at(0x0u), 24.0f);
    (void)call_set(rt, ctx, 0x0896A9C4u, {self, 0u, 0u, frame.at(0x4u), frame.at(0x0u)});
    const u32 mode = ins(word(rt, self + 0x110u), 0u, 0u, 1u);
    put(rt, self + 0x110u, mode);
    put(rt, self + 0xE0u, word(rt, self + 0xE0u) | 0x20000u);
    ctx.gpr[2] = mode;
}

// 0x089057C8: builds a rotation matrix at a0 from a1's yaw (+0x24), two half-word extents (+0x10/+0x14) and a scale pair
// (+0x18..+0x20); v0 ends as a0+4.
void func_089057C8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const s32 extent_x = static_cast<s32>(word(rt, source + 0x10u));
    const s32 extent_y = static_cast<s32>(word(rt, source + 0x14u));
    put(rt, self + 0x28u, static_cast<u32>(extent_x));
    put(rt, self + 0x2Cu, static_cast<u32>(extent_y));
    const float radians = 0.017453292f;
    const float angle = float_at(rt, source + 0x24u) * radians;
    (void)call_setf(rt, ctx, 0x08B67C24u, {frame.at(0u), frame.at(0x4u)}, {angle});
    const float sine = float_at(rt, frame.at(0u));
    const float cosine = float_at(rt, frame.at(0x4u));
    const float neg_x = -float_at(rt, source + 0x18u);
    const float scale_y = float_at(rt, source + 0x20u);
    const float scale_z = float_at(rt, source + 0x1Cu);
    const float cosine_y = scale_y * cosine;
    const float sine_y = scale_y * sine;
    const float z_cosine = scale_z * cosine_y;
    const float x_sine = sine_y * neg_x;
    const float z_sine = scale_z * sine_y;
    const float x_cosine = cosine_y * neg_x;
    const float diff = x_sine - z_cosine;
    const float sum = x_cosine + z_sine;
    const float half_extent_x = static_cast<float>(half_word(extent_x));
    const float half_extent_y = static_cast<float>(half_word(extent_y));
    put_float(rt, self + 0x4u, cosine_y);
    put_float(rt, self + 0x10u, -sine_y);
    put_float(rt, self + 0x1Cu, sum + half_extent_x);
    put_float(rt, self + 0x20u, diff + half_extent_y);
    put(rt, self + 0x24u, 0u);
    put_float(rt, self + 0x8u, sine_y);
    put_float(rt, self + 0x14u, cosine_y);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x18u, 0u);
    ctx.gpr[2] = self + 0x4u;
}

// 0x089B6584: constructor — vtable, four zeroed sub-objects, a byte flag block at +0x138 and a two-float stack pair;
// v0 is the +0x110 word it stores last.
void func_089B6584(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 flags = self + 0x138u;
    (void)call_set(rt, ctx, 0x089A2528u, {self});
    put(rt, self, 0x08CCAC10u);
    put(rt, self + 0x38u, 0x08CCAE6Cu);
    put(rt, self + 0x120u, 0u);
    put(rt, self + 0x124u, 0u);
    put(rt, self + 0x128u, 0u);
    (void)call_set(rt, ctx, 0x08A76AA0u, {self + 0x12Cu});
    (void)call_set(rt, ctx, 0x08A76AA0u, {self + 0x130u});
    (void)call_set(rt, ctx, 0x08A76AA0u, {self + 0x134u});
    put(rt, flags + 0x8u, 1u);
    put8(rt, flags + 0xCu, 1u);
    put(rt, flags + 0x4u, 0u);
    put8(rt, flags + 0xDu, 0u);
    put(rt, self + 0x138u, 0u);
    put(rt, self + 0x148u, 0u);
    put(rt, self + 0x148u + 0x8u, 0u);
    put(rt, self + 0x148u + 0x4u, 0u);
    put(rt, self + 0x168u, 0u);
    put8(rt, self + 0x15Au, 1u);
    put(rt, self + 0x154u, 0u);
    put8(rt, self + 0x158u, 0u);
    put8(rt, self + 0x159u, 0u);
    put(rt, self + 0x15Cu, 0u);
    put(rt, self + 0x160u, 0u);
    put(rt, self + 0x164u, 0u);
    put(rt, self + 0x168u + 0x8u, 0u);
    put(rt, self + 0x168u + 0x4u, 0u);
    put(rt, self + 0x174u, 0u);
    put_float(rt, frame.at(0x4u), 128.0f);
    put_float(rt, frame.at(0x0u), 24.0f);
    (void)call_set(rt, ctx, 0x0896A9C4u, {self, 0u, 0u, frame.at(0x4u), frame.at(0x0u)});
    const u32 mode = ins(word(rt, self + 0x110u), 0u, 0u, 1u);
    put(rt, self + 0x110u, mode);
    put(rt, self + 0xE0u, word(rt, self + 0xE0u) | 0x20000u);
    put8(rt, flags + 0xCu, 0u);
    ctx.gpr[2] = mode;
}

// 0x089A5DD0: constructor — two vtables, a name/descriptor call at +0x128, a flag word at +0x138 (bit 7 from the global byte
// at 0x08CF38F0), two two-way lists and a flag word at +0x60; v0 is that last word.

// 0x08918498: two matrix products over a stack matrix built from an array element (a0 + 4*a1 + 8) and two stack vectors;
// the projected point goes to a3 and v0 is whether the depth was positive.

// 0x088F7660: scales a0's scrolling step (+0x18) by its rate pair (+0x1C..+0x28) and a1's fields, nudges a0's +0x10 run
// pointer and the +0x14/+0x18 pair, then tails into 0x088F6804 with a0, a1 and f12.
void func_088F7660(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 params = ctx.gpr[5];
    const float step_scale = ctx.fpr[12];
    const u32 pair = self + 0x10u;
    const float level = float_at(rt, params + 0x4u);
    const float rate = level / float_at(rt, self + 0x1Cu);
    const float span = (float_at(rt, self + 0x28u) * 0.5f) * float_at(rt, self + 0x20u);
    const float width = float_at(rt, params);
    const float amount = (span * level) / float_at(rt, self + 0x24u);
    float extra = 0.0f;
    const float one_minus = 1.0f - rate;
    const float offset = width * one_minus;
    if (0.1f <= float_at(rt, self + 0x4u)) {
        const float denominator = float_at(rt, params + 0x1Cu);
        (void)call_setf(rt, ctx, 0x08B67720u, {}, {float_at(rt, self + 0x4u) / denominator, float_at(rt, params + 0x14u)});
        extra = float_at(rt, params + 0x18u) * ctx.fpr[0];
    }
    const float scaled_a = extra * float_at(rt, params + 0x24u);
    const float scaled_b = extra * float_at(rt, params + 0x28u);
    const float moved = float_at(rt, self + 0x10u) - scaled_a;
    put_float(rt, self + 0x10u, moved);
    const float moved_z = float_at(rt, pair + 0x8u) - scaled_b;
    put_float(rt, pair + 0x8u, moved_z);
    (void)call_setf(rt, ctx, 0x088F6A74u, {pair}, {amount, offset, step_scale});
    const float run = scaled_a + float_at(rt, self + 0x10u);
    put_float(rt, self + 0x10u, run);
    put_float(rt, pair + 0x4u, float_at(rt, pair + 0x4u) + 0.0f);
    put_float(rt, pair + 0x8u, scaled_b + float_at(rt, pair + 0x8u));
    ctx.fpr[12] = step_scale;
    tail_call(ctx, 0x088F6804u, {self, pair});
}

// 0x0896BCAC: constructor — vtable, a 128.0f pair at +0x78, a name/descriptor call, a wide flag word at +0xE0 and the float
// block at +0xA4..+0xDC; v0 is the +0xE0 word.
void func_0896BCAC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x089BB804u, {self});
    put(rt, self, 0x08CB92F8u);
    put(rt, self + 0x38u, 0x08CB9530u);
    put_float(rt, self + 0x70u + 0xCu, 128.0f);
    put_float(rt, self + 0x70u + 0x8u, 128.0f);
    put(rt, self + 0x70u + 0x4u, 0u);
    put(rt, self + 0x70u, 0u);
    put(rt, self + 0x80u, 0u);
    (void)call_set(rt, ctx, 0x089B883Cu, {self + 0x84u, self});
    (void)call_set(rt, ctx, 0x0897D9C0u, {self + 0xA0u, 0x08CF6ED8u});
    u32 flags = ins(word(rt, self + 0xE0u), 0u, 0u, 14u);
    flags |= 0x4000u;
    flags = ins(flags, 0u, 15u, 1u);
    flags |= 0x10000u;
    flags = ins(flags, 0u, 17u, 4u);
    flags = ins(flags, 2u, 21u, 2u);
    flags = ins(flags, 0u, 23u, 1u);
    flags |= 0x1000000u;
    flags = ins(flags, 0u, 25u, 1u);
    flags |= 0x04000000u;
    flags |= 0x8000000u;
    flags |= 0x10000000u;
    flags |= 0x20000000u;
    flags |= 0x40000000u;
    flags |= 0x80000000u;
    const u32 control = ins(word(rt, self + 0xE4u), 0u, 0u, 9u);
    put(rt, self + 0xE4u, control);
    put_float(rt, self + 0xC8u, 1.0f);
    put(rt, self + 0xCCu, 0x08D74FD0u + 0xCu);
    put_float(rt, self + 0xD8u, 0.5f);
    put_float(rt, self + 0xDCu, -22.5f);
    put(rt, self + 0xE0u, flags);
    put_float(rt, self + 0xA4u, 0.5f);
    put_float(rt, self + 0xA8u, 0.5f);
    put(rt, self + 0xACu, 0u);
    put(rt, self + 0xB0u, 0u);
    put_float(rt, self + 0xB4u, 1.0f);
    put_float(rt, self + 0xB8u, 1.0f);
    put(rt, self + 0xBCu, 0u);
    put(rt, self + 0xC0u, 0u);
    put(rt, self + 0xC4u, 0u);
    put(rt, self + 0xD0u, 0u);
    put_float(rt, self + 0xD4u, 0.5f);
    ctx.gpr[2] = flags;
}

// 0x08983CE8: fills the graphics state object at 0x08CF68E8 from a0 (position, four sub-matrices, flag bits) and hands it to
// three submit calls; v0 ends as the flag word at state+0x58 with bit 16 set from a0+0xA9.
void func_08983CE8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    const float in12 = ctx.fpr[12], in13 = ctx.fpr[13], in14 = ctx.fpr[14], in15 = ctx.fpr[15];
    const float high_x = in13 + in15;
    const float high_y = in12 + in14;
    const u32 state = word(rt, 0x08CF68E8u);
    (void)call_set(rt, ctx, 0x08A4D57Cu, {state, word(rt, self + 0x30u), arg2, ctx.gpr[7]});
    put_float(rt, state + 0x20u, float_at(rt, self + 0xBCu));
    put_float(rt, state + 0x24u, float_at(rt, self + 0xC0u));
    put_float(rt, state + 0x5Cu, float_at(rt, self + 0x48u));
    const u32 first = call_setf(rt, ctx, 0x08964254u, {self + 0x50u}, {1.0f}).v0;
    put(rt, state + 0x40u, first);
    put_float(rt, state + 0x60u, float_at(rt, self + 0x70u));
    const u32 second = call_setf(rt, ctx, 0x08964254u, {self + 0x80u}, {1.0f}).v0;
    put(rt, state + 0x44u, second);
    put_float(rt, state + 0x68u, float_at(rt, self + 0xA0u));
    put_float(rt, state + 0x64u, float_at(rt, self + 0xA4u));
    u32 flags = word(rt, state + 0x58u);
    flags = ins(flags, byte_u(rt, self + 0x34u), 10u, 1u);
    put(rt, state + 0x58u, flags);
    flags = ins(flags, byte_u(rt, self + 0x35u), 11u, 1u);
    put(rt, state + 0x58u, flags);
    flags = ins(flags, byte_u(rt, self + 0x39u), 14u, 1u);
    put(rt, state + 0x58u, flags);
    flags = ins(flags, byte_u(rt, self + 0x36u), 9u, 1u);
    put(rt, state + 0x58u, flags);
    const u32 third = call_setf(rt, ctx, 0x08964254u, {self + 0x10u}, {1.0f}).v0;
    put(rt, state + 0x3Cu, third);
    flags = ins(word(rt, state + 0x58u), byte_u(rt, self + 0xA8u), 12u, 1u);
    put(rt, state + 0x58u, flags);
    put_float(rt, state + 0x50u, high_y);
    put_float(rt, state + 0x54u, high_x);
    put_float(rt, state + 0x48u, in12);
    put_float(rt, state + 0x4Cu, in13);
    (void)call_set(rt, ctx, 0x08A4CF58u, {state, 1u});
    const u32 slot = call_set(rt, ctx, 0x08A4CCE8u, {state, arg2});
    put8(rt, state + 0x58u, slot);
    const u32 bits = word(rt, state + 0x58u) | 0x100u;
    put(rt, state + 0x58u, bits);
    (void)call_set(rt, ctx, 0x08A4D79Cu, {state, arg1, 0u, 0u, 0u});
    ctx.gpr[2] = ins(word(rt, state + 0x58u), byte_u(rt, self + 0xA9u), 16u, 1u);
    put(rt, state + 0x58u, ctx.gpr[2]);
}

// 0x08A13FC8: constructor — a base call, a vtable, a descriptor looked up through two calls and a stack record, four
// sub-objects, a float pair and a long block of zeroed fields; v0 is the last registration call's answer.

// 0x0887D0F4: constructor — a long chain of one-argument initialisers for each member, the floats written between them;
// v0 is 1.
void func_0887D0F4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 at_3c = self + 0x3Cu;
    const u32 at_3dc = self + 0x3DCu;
    const u32 at_4 = self + 0x4u;
    (void)call_set(rt, ctx, 0x08BA0210u, {self});
    (void)call_set(rt, ctx, 0x08BA0140u, {self});
    (void)call_set(rt, ctx, 0x08BA1C34u, {self});
    (void)call_set(rt, ctx, 0x08BA2360u, {at_4});
    put_float(rt, self + 0x4u, 1.0f);
    (void)call_set(rt, ctx, 0x08BA22D4u, {self + 0x8u});
    put_float(rt, self + 0x8u, 0.9f);
    (void)call_set(rt, ctx, 0x08BA21E4u, {self + 0xCu});
    put_float(rt, self + 0xCu, 1000.0f);
    (void)call_set(rt, ctx, 0x08BA20F4u, {self + 0x10u});
    put_float(rt, self + 0x10u, 1.0f);
    (void)call_set(rt, ctx, 0x08BA2004u, {self + 0x14u});
    put(rt, self + 0x14u, 0u);
    (void)call_set(rt, ctx, 0x08BA1F14u, {self + 0x18u});
    put_float(rt, self + 0x18u, 200.0f);
    (void)call_set(rt, ctx, 0x08BA1E24u, {self + 0x1Cu});
    put(rt, self + 0x1Cu, 0u);
    (void)call_set(rt, ctx, 0x08BA1BA8u, {at_4});
    (void)call_set(rt, ctx, 0x08BA1704u, {self + 0x20u});
    (void)call_set(rt, ctx, 0x08BA1678u, {self + 0x30u});
    put_float(rt, self + 0x30u, 100.0f);
    (void)call_set(rt, ctx, 0x08BA1588u, {self + 0x34u});
    put_float(rt, self + 0x34u, 1000.0f);
    (void)call_set(rt, ctx, 0x08BA1498u, {self + 0x38u});
    put(rt, self + 0x38u, 0u);
    (void)call_set(rt, ctx, 0x08BA1AB8u, {self + 0x20u});
    (void)call_set(rt, ctx, 0x08BA3718u, {at_3c});
    (void)call_set(rt, ctx, 0x08B9FE44u, {at_3dc});
    (void)call_set(rt, ctx, 0x08BA1964u, {at_3dc});
    (void)call_set(rt, ctx, 0x08B9FC30u, {self + 0x3ECu});
    put(rt, self + 0x3ECu + 0x8u, 0u);
    put(rt, self + 0x3ECu + 0x4u, 0u);
    put(rt, self + 0x3ECu, 0u);
    (void)call_set(rt, ctx, 0x08B9FB40u, {self + 0x3ECu});
    (void)call_set(rt, ctx, 0x08B9F960u, {self + 0x3F8u});
    put(rt, self + 0x3ECu, 0u);
    put(rt, self + 0x3ECu + 0x4u, 0u);
    put(rt, self + 0x3ECu + 0x8u, 0u);
    put8(rt, self + 0x3ECu + 0xCu, 1u);
    (void)call_set(rt, ctx, 0x08B9FA50u, {self + 0x3ECu});
    const u32 block = self + 0x5Cu;
    const u32 other = self + 0x4Cu;
    const u32 quad = self + 0x6Cu;
    put(rt, self, 1u);
    put_float(rt, at_3c + 0x4u, 1.0f);
    put_float(rt, self + 0x5Cu, 0.7f);
    put_float(rt, block + 0x8u, 0.7f);
    put_float(rt, block + 0x4u, 0.7f);
    put_float(rt, block + 0xCu, 1.0f);
    put_float(rt, self + 0x6Cu, 1.0f);
    put_float(rt, quad + 0xCu, 1.0f);
    put_float(rt, quad + 0x4u, 1.0f);
    put_float(rt, quad + 0x8u, 1.0f);
    put(rt, self + 0x4Cu, 0u);
    put_float(rt, other + 0xCu, 1.0f);
    put(rt, other + 0x4u, 0u);
    put(rt, other + 0x8u, 0u);
    put_float(rt, self + 0x3DCu, 0.3f);
    put_float(rt, at_3dc + 0x8u, 0.3f);
    put_float(rt, at_3dc + 0xCu, 1.0f);
    put_float(rt, at_3dc + 0x4u, 0.3f);
    put_float(rt, self + 0x4u, 1.0f);
    put_float(rt, self + 0xCu, 400.0f);
    put_float(rt, self + 0x3ECu, 0.2f);
    put_float(rt, self + 0x3ECu + 0x8u, 0.2f);
    put_float(rt, self + 0x3ECu + 0x4u, 0.2f);
    ctx.gpr[2] = 1u;
}

// 0x08984608: constructor — a vtable at +0, three two-word float/flag records built through 0x08964224 + 0x0896418C, a byte
// flag block and two 32-byte allocations split into two halves; v0 is the second half's end.

} // namespace
} // namespace new_batch_main_902
} // namespace gt::game

#define NEW902(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_902::name, gt::ReturnKind::kind)
NEW902(0x089F3A70u, func_089F3A70, Word);
NEW902(0x089AC110u, func_089AC110, Word);
NEW902(0x0892EEC0u, func_0892EEC0, Word);
NEW902(0x08922BFCu, func_08922BFC, Word);
NEW902(0x08A064A0u, func_08A064A0, Word);
NEW902(0x089F9578u, func_089F9578, Word);
NEW902(0x088F7558u, func_088F7558, Word);
NEW902(0x0899DBA8u, func_0899DBA8, Word);
NEW902(0x089057C8u, func_089057C8, Word);
NEW902(0x089B6584u, func_089B6584, Word);
NEW902(0x088F7660u, func_088F7660, Word);
NEW902(0x0896BCACu, func_0896BCAC, Word);
NEW902(0x08983CE8u, func_08983CE8, Word);
NEW902(0x0887D0F4u, func_0887D0F4, Word);
