// ---- from main-session batch 1128 ----
// New batch main 1128: ten functions the gate could never run with useful inputs, written by hand from `tools.disasm`.
// Nothing here is verified yet; the synthetic shapes handed over with this batch (/home/andres/repos/gt-ops/shapes/
// shapes_main1128.txt) are what give the vector/quaternion arguments valid memory. Registered at the bottom, one address
// one body.
//
// Every call_guest / call_set / call_setf passes the argument registers the original sets before the jal (a0-a3 and the
// float ones from f12); a callee that writes v0 / v1 / f0 hands them on to the caller's next instruction. The delay slot of
// a `jal` runs before the call and the delay slot of a non-likely branch on both paths. Callee-saved registers (s0-s7,
// f20-f23) are never written here (locals stand in), so they keep the caller's values like the original's restored ones.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1128 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose v0 / v1 / f0 stay in the context like the original's: the next instruction sees them.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with the integer argument registers set in order from a0 (a0-a3, t0-t3).
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// The same with the float argument registers set in order from f12 (f12-f19).
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : floats) {
        if (freg <= 19u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, floats);
}

constexpr float kPi = 3.14159274101257324f;        // 0x40490FDB
constexpr float kHalfPi = 1.57079637050628662f;    // 0x3FC90FDB
constexpr float kTwoPi = 6.28318548202514648f;     // 0x40C90FDB

// ---------------------------------------------------------------------------------------------------------------
// 0x08B4AF8C (a0 = destination quaternion, a1 / a2 = the two quaternions, f12 = the weight): the frame copies are
// scaled by 1 - weight (a1's) and weight (a2's), added by 0x08B4A890 and written to a0.
void func_08B4AF8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 dest = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const float weight = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    for (unsigned i = 0; i < 4u; ++i) put_float(rt, frame.at(0x10u + 4u * i), float_at(rt, first + 4u * i));
    call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x10u)}, {1.0f - weight});
    for (unsigned i = 0; i < 4u; ++i) put_float(rt, frame.at(4u * i), float_at(rt, second + 4u * i));
    call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0u)}, {weight});
    for (unsigned i = 0; i < 4u; ++i) put_float(rt, frame.at(0x20u + 4u * i), float_at(rt, frame.at(0x10u + 4u * i)));
    call_set(rt, ctx, 0x08B4A890u, {frame.at(0x20u), frame.at(0x20u), frame.at(0u)});
    for (unsigned i = 0; i < 4u; ++i) put_float(rt, dest + 4u * i, float_at(rt, frame.at(0x20u + 4u * i)));
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08AFB814 (a0 = object): the base constructor 0x08AFB4E8 runs, then the vtable 0x08CD7848 is stored at +0, which is
// also what v0 answers.
void func_08AFB814(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        call_set(rt, ctx, 0x08AFB4E8u, {self});
    }
    put(rt, self, 0x08CD7848u);
    ctx.gpr[2] = 0x08CD7848u;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08A5CFFC (a0 = object, a1 = source): base 0x08A76ACC(object, source), the vtable 0x08CCB870 at +0, then the tail
// call 0x08A6972C(object + 0x14, source) whose answer is v0.
void func_08A5CFFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        call_set(rt, ctx, 0x08A76ACCu, {self, source});
        put(rt, self, 0x08CCB870u);
    }
    tail_call(ctx, 0x08A6972Cu, {self + 0x14u, source});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B4FFDC (a0 = destination quaternion, a1 / a2 = two vectors, f12 = a scale): quaternion between them. The cross
// product a1 x a2 gives |cross| = sin(angle) and a1 . a2 = cos(angle) decides the half turn; the angle (0x08B49D98) is
// scaled by f12 and handed to 0x08B4BB00 with the normalized cross. Parallel vectors take the perpendicular axis from
// 0x08B4F678 (half turn) or answer the identity quaternion. v0 stays as the entry v0: no callee writes it.
void func_08B4FFDC(Runtime &rt, AllegrexContext &ctx) {
    const u32 dest = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0x60u);
    const u32 cross = frame.at(0x18u);
    const u32 axis = frame.at(0xCu);
    call_set(rt, ctx, 0x08B4A9D0u, {cross, first, second});
    put_float(rt, axis, float_at(rt, cross));
    put_float(rt, axis + 4u, float_at(rt, cross + 4u));
    put_float(rt, axis + 8u, float_at(rt, cross + 8u));
    const float cross_length = std::sqrt(call_set(rt, ctx, 0x08B4A720u, {axis}).f0);
    const float dot = call_set(rt, ctx, 0x08B4A9A0u, {first, second}).f0;
    if (cross_length == 0.0f) {
        if (dot < 0.0f) {
            put_float(rt, frame.at(0u), 1.0f);
            put_float(rt, frame.at(4u), 0.0f);
            put_float(rt, frame.at(8u), 0.0f);
            call_set(rt, ctx, 0x08B4F678u, {axis, second, frame.at(0u)});
            const float axis_length = std::sqrt(call_set(rt, ctx, 0x08B4A720u, {axis}).f0);
            call_setf(rt, ctx, 0x08B4A5E8u, {axis}, {1.0f / axis_length});
            call_setf(rt, ctx, 0x08B4BB00u, {dest, axis}, {psp_mul(kPi, scale)});
        } else {
            put_float(rt, dest, 0.0f);
            put_float(rt, dest + 4u, 0.0f);
            put_float(rt, dest + 8u, 0.0f);
            put_float(rt, dest + 0xCu, 1.0f);
        }
        return;
    }
    float angle = call_setf(rt, ctx, 0x08B49D98u, {first, second}, {cross_length}).f0;
    if (dot < 0.0f) angle = kPi - angle;
    const float axis_length = std::sqrt(call_set(rt, ctx, 0x08B4A720u, {axis}).f0);
    call_setf(rt, ctx, 0x08B4A5E8u, {axis}, {1.0f / axis_length});
    call_setf(rt, ctx, 0x08B4BB00u, {dest, axis}, {psp_mul(angle, scale)});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B4CE38 (a0 = quaternion, f12 / f13 / f14 = three angles): 0x08B4CD24 builds a quaternion in the frame from the
// angles and 0x08B4B864 multiplies a0 by it in place; v0 is that call's answer.
void func_08B4CE38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float first = ctx.fpr[12];
    const float second = ctx.fpr[13];
    const float third = ctx.fpr[14];
    GuestFrame frame(ctx, 0x20u);
    call_setf(rt, ctx, 0x08B4CD24u, {frame.at(0u)}, {first, second, third});
    call_set(rt, ctx, 0x08B4B864u, {self, frame.at(0u)});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B4F8C4 (a0 = destination quaternion, a1 = a rotation object): the magnitude of a1's float at +8 (0x08C524EC)
// picks the builder: below 0.9 0x08C52868 + 0x08B4C6BC, otherwise 0x08C527C0 + 0x08B4C804, both reading the frame
// triple they fill. v0 is the last call's answer.
void func_08B4F8C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const float magnitude = call_set(rt, ctx, 0x08C524ECu, {source}).f0;
    const bool below = magnitude < std::bit_cast<float>(0x3F666666u);  // 0.9
    if (below) {
        call_set(rt, ctx, 0x08C52868u, {source, frame.at(0u), frame.at(4u), frame.at(8u)});
        call_setf(rt, ctx, 0x08B4C6BCu, {self},
                  {float_at(rt, frame.at(0u)), float_at(rt, frame.at(4u)), float_at(rt, frame.at(8u))});
    } else {
        call_set(rt, ctx, 0x08C527C0u, {source, frame.at(0u), frame.at(4u), frame.at(8u)});
        call_setf(rt, ctx, 0x08B4C804u, {self},
                  {float_at(rt, frame.at(0u)), float_at(rt, frame.at(4u)), float_at(rt, frame.at(8u))});
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B20168: 0x08B200F0 answers the singleton record; v1 = its first word (the vtable) and the tail call is the
// vtable's slot 8 with a0 = the record. v0 stays the record, as the original leaves it before `jr $t9`.
void func_08B20168(Runtime &rt, AllegrexContext &ctx) {
    GuestResult got;
    {
        GuestFrame frame(ctx, 0x10u);
        got = call_set(rt, ctx, 0x08B200F0u, {});
    }
    const u32 record = got.v0;
    const u32 table = word(rt, record);
    const u32 target = word(rt, table + 8u);
    ctx.gpr[3] = table;
    ctx.gpr[5] = got.a1;  // a1 as 0x08B200F0 left it reaches the virtual
    ctx.gpr[25] = target;
    tail_call(ctx, target, {record});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08B4D480 (a0 = destination matrix, a1 = source matrix): 0x08B4D434 copies the source's nine floats into the frame
// in row-major order, then the frame is copied to a0 in the same order.
void func_08B4D480(Runtime &rt, AllegrexContext &ctx) {
    const u32 dest = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    call_set(rt, ctx, 0x08B4D434u, {frame.at(0u), source});
    for (unsigned i = 0; i < 9u; ++i) put_float(rt, dest + 4u * i, float_at(rt, frame.at(4u * i)));
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08AFD7B0 (a0 = object, f12 / f13 = two scales): 0x08BAC230(object) and 0x08C46020(&object, object + 0x38) only
// hand back their arguments (v0 = the frame slot, v1 = object + 0x38); the first four floats of the object (the first
// three negated) become a rotation (0x08BAC284) whose scaled axis is accumulated into object + 0x80. The magnitude
// from the object's +0xD8 points and that rotation's three floats (0x08BBBEC4) becomes f22; a zero stops here. The
// answer's v0 is the frame slot +4 on the working path (0x08C45E00 returns its a0) and the object itself when the
// magnitude is zero.

// ---------------------------------------------------------------------------------------------------------------
// 0x08B50174 (a0 = destination quaternion, a1 / a2 / a3 / t0 = four vectors, f12 = a scale): a1 x a2 and a3 x t0.
// The first cross is normalized and, with the dot of a1 and a2 (above -0.5) 0x08B4FE74 combines it with the second
// cross; below -0.5 a half turn is built around a1 (0x08B4BB00) and 0x08B4BA68/0x08B4FE74/0x08B4B864 compose it.
// The dot of a3 with the combined cross is clamped to [-1, 1], its angle (0x08B49F3C) is scaled by f12 (2*pi minus the
// angle when the second dot is negative) and 0x08B4BCC0 applies it; v0 is that call's answer.
void func_08B50174(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0x70u);
    const u32 cross = frame.at(0x30u);
    const u32 axis = frame.at(0xCu);
    const u32 other = frame.at(0x18u);
    call_set(rt, ctx, 0x08B4A9D0u, {cross, first, second});
    put_float(rt, frame.at(0u), float_at(rt, cross));
    put_float(rt, frame.at(4u), float_at(rt, cross + 4u));
    put_float(rt, frame.at(8u), float_at(rt, cross + 8u));
    call_set(rt, ctx, 0x08B4A9D0u, {cross, third, fourth});
    put_float(rt, axis, float_at(rt, cross));
    put_float(rt, axis + 4u, float_at(rt, cross + 4u));
    put_float(rt, axis + 8u, float_at(rt, cross + 8u));
    const float first_length = std::sqrt(call_set(rt, ctx, 0x08B4A720u, {frame.at(0u)}).f0);
    call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0u)}, {1.0f / first_length});
    const float second_length = std::sqrt(call_set(rt, ctx, 0x08B4A720u, {axis}).f0);
    call_setf(rt, ctx, 0x08B4A5E8u, {axis}, {1.0f / second_length});
    const float dot = call_set(rt, ctx, 0x08B4A9A0u, {frame.at(0u), axis}).f0;
    if (dot > -0.5f) {
        call_set(rt, ctx, 0x08B4FE74u, {self, frame.at(0u), axis});
    } else {
        call_setf(rt, ctx, 0x08B4BB00u, {self, first}, {kHalfPi});
        call_set(rt, ctx, 0x08B4BA68u, {self, other, frame.at(0u)});
        call_set(rt, ctx, 0x08B4FE74u, {cross, other, axis});
        call_set(rt, ctx, 0x08B4B864u, {self, cross});
    }
    const u32 combined = frame.at(0x24u);
    call_set(rt, ctx, 0x08B4BA68u, {self, combined, first});
    const float value = call_set(rt, ctx, 0x08B4A9A0u, {combined, third}).f0;
    float clamped;
    if (value < -1.0f) clamped = -1.0f;             // bc1tl: below -1 takes -1
    else if (!(value < 1.0f)) clamped = 1.0f;       // bc1fl: 1 and above (NaN too) take 1
    else clamped = value;
    float angle = call_setf(rt, ctx, 0x08B49F3Cu, {}, {clamped}).f0;
    call_set(rt, ctx, 0x08B4A9D0u, {cross, combined, third});
    put_float(rt, other, float_at(rt, cross));
    put_float(rt, other + 4u, float_at(rt, cross + 4u));
    put_float(rt, other + 8u, float_at(rt, cross + 8u));
    const float sign = call_set(rt, ctx, 0x08B4A9A0u, {axis, other}).f0;
    float weight = psp_mul(angle, scale);
    if (sign < 0.0f) {
        angle = kTwoPi - angle;
        weight = psp_mul(angle, scale);
    }
    call_setf(rt, ctx, 0x08B4BCC0u, {self, axis}, {weight});
}

} // namespace
} // namespace new_batch_main_1128
} // namespace gt::game

#define NEW1128(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1128::name, gt::ReturnKind::kind)

NEW1128(0x08B4AF8Cu, func_08B4AF8C, Word);
NEW1128(0x08AFB814u, func_08AFB814, Word);
NEW1128(0x08A5CFFCu, func_08A5CFFC, Word);
NEW1128(0x08B4FFDCu, func_08B4FFDC, Word);
NEW1128(0x08B4CE38u, func_08B4CE38, Word);
NEW1128(0x08B4F8C4u, func_08B4F8C4, Word);
NEW1128(0x08B20168u, func_08B20168, Word);
NEW1128(0x08B4D480u, func_08B4D480, Word);
NEW1128(0x08B50174u, func_08B50174, Word);
