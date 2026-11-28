// New batch main-1000 (main session draft, unverified): functions the gate compared and mismatched (or skipped).
// Hand-written structured C from `tools.disasm`; shapes in gt-ops/shapes/shapes_main1000.txt.
// Delay slots run before the branch/call they belong to; v0 is the original's on every exit path.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1000 {
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
// A guest call with float arguments (f12 upwards) and a float answer (f0); v0 / v1 / f0 end as the callee left them.
float call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
             std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline float fbits(u32 bits) { return std::bit_cast<float>(bits); }
inline u32 bits_of(float value) { return std::bit_cast<u32>(value); }

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08ACD920 (a0 = owner, a1 = state block): submit/poll loop under the lock at word(state+0xAC)+0x164. Byte state+0xD4
// clear returns the unlock's result at once. Word state+0xCC picks the path: 1 runs the 0x08C7AD88 dispatch up to 16
// times (each null answer repeats it), 0 the 0x08C7ADD8 dispatch and its 0x08ACCD54 / 0x08B24580 follow-up, anything
// else sleeps 20 s and starts over. s7 counts the mode-1 attempts; the per-attempt record at sp+0xC is 0x140 bytes.
void func_08ACD920(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 state = ctx.gpr[5];
    const u32 lock = state + 0x80u;
    GuestFrame frame(ctx, 0x2050u);
    put(rt, frame.at(0u), 1u);
    u32 result = 0u;
    u32 attempts = 0u;
    u32 record = frame.at(0xCu);

    // The state+0xCC == 0 body: one dispatch, its follow-up, then back to the lock or out.
    const auto mode0 = [&]() -> bool {  // true: take the lock again; false: return
        const u32 guard = word(rt, state + 0xACu) + 0x164u;
        call(rt, ctx, 0x08B344E0u, {guard});
        const u32 owner_flag = byte_u(rt, owner + 0x304u);
        result = call(rt, ctx, 0x08C7ADD8u, {word(rt, state + 0xB0u), word(rt, state + 0xBCu),
                                              word(rt, state + 0xB8u), frame.at(8u)});
        const u32 answer = result;
        result = call(rt, ctx, 0x08B3457Cu, {guard});
        if (answer != 0u) {
            if (answer != 0x80618001u) {
                put8(rt, state + 0xD4u, 0u);
                return false;
            }
            if (owner_flag != 0u) {
                call(rt, ctx, 0x08B2E2B8u, {0x2710u});
                put8(rt, state + 0xC9u, 1u);
            } else {
                call(rt, ctx, 0x08ACD86Cu, {owner, state});
            }
            return true;
        }
        const u32 queue = word(rt, state + 0xB8u);
        const u32 value = word(rt, queue + 4u);
        if (value != 0xFFFFFFFFu) {
            put(rt, state + 0xC4u, value);
        } else {
            put(rt, state + 0xC4u, word(rt, state + 0xC4u) + 0x1053u);
        }
        const u32 progressed = call(rt, ctx, 0x08ACCD54u, {owner, state, frame.at(0xCu), frame.at(0u)});
        if (progressed != 0u) {
            put8(rt, state + 0xD4u, 0u);
            call(rt, ctx, 0x08B2E2B8u, {0x4E20u});
            return true;
        }
        put(rt, frame.at(0x200Cu), 0x2000u);
        put(rt, frame.at(0x2010u), word(rt, state + 0xC4u));
        put(rt, frame.at(0u), 0u);
        const u32 waited = call(rt, ctx, 0x08B24580u, {word(rt, state + 0xA8u)});
        if (waited != 0xFFFFFFFFu) {
            call(rt, ctx, 0x08B2440Cu, {word(rt, state + 0xA8u), frame.at(0xCu), 1u});
        } else {
            put8(rt, state + 0xD4u, 0u);
        }
        call(rt, ctx, 0x08B2E2B8u, {0x4E20u});
        return true;
    };

    // The state+0xCC == 1 body: up to 16 dispatch attempts; a null answer repeats, 0x80618001 may run 0x08ACD86C
    // (or sleep 10 s and raise +0xC9) and recompute the record from the attempt count.
    const auto mode1 = [&]() -> bool {
        for (;;) {
            const u32 guard = word(rt, state + 0xACu) + 0x164u;
            call(rt, ctx, 0x08B344E0u, {guard});
            const u32 owner_flag = byte_u(rt, owner + 0x304u);
            const u32 pending = byte_u(rt, state + 0xD4u);
            result = call(rt, ctx, 0x08C7AD88u, {word(rt, state + 0xB0u), word(rt, state + 0xBCu),
                                                  word(rt, state + 0xB8u), frame.at(4u)});
            const u32 answer = result;
            result = call(rt, ctx, 0x08B3457Cu, {guard});
            const u32 slot = record;
            record += 0x140u;
            if (pending == 0u) return false;
            if (answer == 0u) {
                const u32 queue = word(rt, state + 0xB8u);
                const u32 value = word(rt, queue + 4u);
                attempts += 1u;
                if (value != 0xFFFFFFFFu) {
                    put(rt, state + 0xC4u, value);
                } else {
                    put(rt, state + 0xC4u, word(rt, state + 0xC4u) + 0xA3u);
                }
                call(rt, ctx, 0x08B79A88u, {slot, word(rt, queue + 0x10u), word(rt, queue + 0x14u)});
                call(rt, ctx, 0x08B2E2B8u, {0x1388u});
                if (static_cast<s32>(attempts) < 0x10) continue;
                put(rt, frame.at(0x200Cu), 0x1400u);
                put(rt, frame.at(0x2010u), word(rt, state + 0xC4u));
                const u32 waited = call(rt, ctx, 0x08B24580u, {word(rt, state + 0xA8u)});
                if (waited != 0xFFFFFFFFu) {
                    call(rt, ctx, 0x08B2440Cu, {word(rt, state + 0xA8u), frame.at(0xCu), 1u});
                } else {
                    put8(rt, state + 0xD4u, 0u);
                }
                call(rt, ctx, 0x08B2E2B8u, {0x4E20u});
                return true;
            }
            if (answer != 0x80618001u) {
                put8(rt, state + 0xD4u, 0u);
                return false;
            }
            if (owner_flag != 0u) {
                call(rt, ctx, 0x08B2E2B8u, {0x2710u});
                put8(rt, state + 0xC9u, 1u);
            } else {
                call(rt, ctx, 0x08ACD86Cu, {owner, state});
            }
            record = frame.at(0xCu) + (attempts << 6) + (attempts << 8);
        }
    };

    for (;;) {
        result = call(rt, ctx, 0x08B344E0u, {lock});
        if (byte_u(rt, state + 0xD4u) == 0u) {
            result = call(rt, ctx, 0x08B3457Cu, {lock});
            break;
        }
        result = call(rt, ctx, 0x08B3457Cu, {lock});
        const u32 mode = word(rt, state + 0xCCu);
        bool again;
        if (mode == 0u) {
            again = mode0();
        } else if (mode == 1u) {
            attempts = 0u;
            record = frame.at(0xCu);
            again = mode1();
        } else {
            attempts = 0u;
            call(rt, ctx, 0x08B2E2B8u, {0x4E20u});
            again = true;
        }
        if (!again) break;
    }
    ctx.gpr[2] = result;
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08B5FB4C (a0 = stream, a1 = buffer, a2 = size, a3 = delimiter, sign-extended): istream::getline. The sentry at
// 0x08C55FDC(sp, stream, 1) decides; without it the buffer is only terminated and the flags merged. With it the
// streambuf at word(stream + word(word(stream)-0xC) + 0x78) is drained: runs of 2+ go through 0x08B79998 (find the
// delimiter) + 0x08B79A88 (copy), single chars one at a time, the vtable's 0x24 underflow / 0x28 advance slots when
// the buffer is empty. The delimiter is consumed and counted, end of file sets bit 2, a full buffer without the
// delimiter bit 4; 0x08C54CFC(stream + offset, state | flags) merges them. v0 = the stream on every exit.

// ---------------------------------------------------------------------------------------------------------------------------
// 0x088C686C (a1 = sampled object, a2 = state): keyframe lookup. a0 is not read. The object's vtable slot 0x14 answers the
// cycle length (f0); the state's +0x10 / +0x14 are the time inside it, wrapped into [0, length). 0x088C50B8 fills a range
// at sp+8 with 0x40-byte records read through 0x088C50C4, each with its key at +0x14; the segment holding the time is
// bisected and its fraction f20 set. Bit 0 of the state word interpolates +0x28 (+4) / +0x2C (+8) / +0x30 (+0xC), bit 2
// runs 0x08B4A430 (angle of the +4 / +8 delta) through 0x08B498AC into +0x34 = -sin, +0x38 = cos, +0x3C = 0, bit 1 copies
// the +0x38 / +0x3C pairs into +0x18..+0x24. v0 = the segment's start index, or -1 out of range.

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08C23B9C (a0 = holder, a1 = destination container): builds a 0x34-byte helper over word(a0+8), its ranges at +0x24 and
// +0x18 walked as 12-byte records (element+8 handed to 0x08A5BAD8 with the destination's current element), then wraps
// the result through 0x08A61624 / 0x08A90BF4 and the string/word pair at sp+4 / sp+0x10 into a 0x1C-byte block that
// 0x08A76AB4 takes. The count each loop compares against is the low word of the record count times 0xAAAAAAAB, exactly
// as the two `mult` / `mflo` pairs compute it. v0 = the last release call's answer.

// ---------------------------------------------------------------------------------------------------------------------------
// 0x08AC1B48 (a0 = self, a1 = flag byte): rebuilds self's record table. Bit 0 of a1 is written into bit 16 of the word at
// self+0x3C; when that bit is set the single record at word(self+0x34)[0] + word(self+0x30) goes through 0x08AC1934 and
// the four setters on self+4, then the half at self+0x3C becomes 1. Otherwise every entry of the table at word(self+0x34)
// (count = half at word(self+0x30)+0x10) is turned into a record at sp+0x10 and inserted, sorted by its first two words,
// into the vector at sp+4 (0x08C3C898 on a full vector); the vector's record count (low word of the count times
// 0xAAAAAAAB, masked to 16 bits) becomes self+0x3C, a 0x2C-byte block per record is built with 0x08AC263C and filled from
// the vector, and the vector is released with 0x08C3C840 / 0x08B35254. v0 = 1 on the first path, else the last release's.

// ---------------------------------------------------------------------------------------------------------------------------
// 0x0895F4EC (a0 = body, a1 = track object): impact accumulation along the track's 4 or 5 segments. 0x0895F174 copies the
// body's position into sp+0x10 and 0x0895F194(body, i, sp+4, sp) samples segment i into sp+4/+8/+0xC, its scalar into sp+0
// and returns that sample's own scalar. For each segment the step length is compared with the previous scalar, the
// segment's position is projected on the step (sp+0x10..+0x18, parameter t in [0, 1.1]), the impact scaled by
// t*(current-base) must stay positive, and the cross product's norm against the step feeds a falloff
// 1.1 - 0.6*(1-impact)^2 clamped to 1, scaled by the reach (half the sums of both +8 fields), then 1.2 - 1.2*x^2 clamped
// to 1, and added to float(word(body+8)+0x67C). Ends the first time it applies; v0 = 1 while looping, else the body word.
void func_0895F4EC(Runtime &rt, AllegrexContext &ctx) {
    const u32 body = ctx.gpr[4];
    const u32 track = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const u32 body_parts = word(rt, body + 0xCu);
    const u32 track_parts = word(rt, track + 0xCu);
    const float reach = psp_mul(float_at(rt, body_parts + 8u) + float_at(rt, track_parts + 8u), 0.5f);
    call(rt, ctx, 0x0895F174u, {body, frame.at(0x10u)});
    const s32 segments = static_cast<s32>(call(rt, ctx, 0x0895F158u, {track}));
    float base = call_f(rt, ctx, 0x0895F194u, {track, 0u, frame.at(4u), frame.at(0u)}, {});
    if (segments <= 0) return;
    for (u32 index = 1u;; ++index) {
        const float old_x = float_at(rt, frame.at(4u));
        const float old_y = float_at(rt, frame.at(8u));
        const float old_z = float_at(rt, frame.at(0xCu));
        float old_scale = float_at(rt, frame.at(0u));
        const float current = call_f(rt, ctx, 0x0895F194u, {track, index, frame.at(4u), frame.at(0u)}, {});
        const float dx = float_at(rt, frame.at(4u)) - old_x;
        const float dy = float_at(rt, frame.at(8u)) - old_y;
        const float dz = float_at(rt, frame.at(0xCu)) - old_z;
        const float length = call_f(rt, ctx, 0x08B49D7Cu, {},
                                    {psp_mul(dx, dx) + psp_mul(dy, dy) + psp_mul(dz, dz)});
        ctx.gpr[2] = 1u;
        if (index != 1u) old_scale = float_at(rt, frame.at(0u));
        const float ratio = length / old_scale;
        if (!(ratio < fbits(0x4131C71Du))) {
            const float ex = float_at(rt, frame.at(0x10u)) - old_x;
            const float ey = float_at(rt, frame.at(0x14u)) - old_y;
            const float ez = float_at(rt, frame.at(0x18u)) - old_z;
            const float dot = psp_mul(dx, ex) + psp_mul(dy, ey) + psp_mul(dz, ez);
            const float t = dot / psp_mul(length, length);
            if (!(t < 0.0f) && !(fbits(0x3F8CCCCDu) < t)) {
                const float impact = base + psp_mul(t, current - base);
                if (!(impact <= 0.0f)) {
                    const float c0 = psp_mul(dy, ez) - psp_mul(dz, ey);
                    const float c1 = psp_mul(dz, ex) - psp_mul(dx, ez);
                    const float c2 = psp_mul(dx, ey) - psp_mul(dy, ex);
                    const float cross = call_f(rt, ctx, 0x08B49D7Cu, {},
                                               {psp_mul(c0, c0) + psp_mul(c1, c1) + psp_mul(c2, c2)});
                    const float side = cross / length;
                    const float one = 1.0f;
                    const float rest = one - impact;
                    float falloff = psp_mul(psp_mul(rest, rest), fbits(0xBF19999Au)) + fbits(0x3F8CCCCDu);
                    if (one < falloff) falloff = one;
                    const float scaled = psp_mul(reach, falloff);
                    if (!(scaled < side)) {
                        const float amount = side / scaled;
                        float curve = psp_mul(psp_mul(amount, amount), fbits(0xBF99999Au)) + fbits(0x3F99999Au);
                        if (one < curve) curve = one;
                        const u32 entity = word(rt, body + 8u);
                        ctx.gpr[2] = entity;
                        const float total = psp_mul(curve, falloff) + float_at(rt, entity + 0x67Cu);
                        put_float(rt, entity + 0x67Cu, total);
                        return;
                    }
                }
            }
        }
        if (segments == static_cast<s32>(index)) break;
        base = current;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------
// 0x089164E8 (a0 = source, a1 = matrix out, f12 = angle): builds a 4x4 road/banking frame. 0x088D32E0 fills two 3-float
// positions at sp+0xC and sp (the second with f12 = angle + 1); their cross product goes to sp+0x78, the two normalized
// vectors and the position form the 4x4 matrix at sp+0x90 (cross, vector at sp+0x58, vector at sp+0x48, position with
// w = 1), the matrix's four rows are set to (0.5, 0.2, 0, 1) / (-0.5, 0.2, 0, 1) / (-0.5, 1.2, 0, 1) / (0.5, 1.2, 0, 1)
// and each is transformed by that matrix with 0x08B4A774 in place. v0 = the last transform's answer.
} // namespace
} // namespace new_batch_main_1000
} // namespace gt::game

#define NEW1000(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1000::name, gt::ReturnKind::kind)
NEW1000(0x08ACD920u, func_08ACD920, Word);
NEW1000(0x0895F4ECu, func_0895F4EC, Word);
