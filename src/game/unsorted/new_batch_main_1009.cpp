// Main-session batch 1009: functions earlier batches left unverified (no comparable synthetic case) or drafted with a
// mismatch. Written by hand from `tools/disasm`; each function's comment ends with the input shape a synthetic case
// needs (`ADDRESS reg:aN=struct:SIZE [field:+OFF=ptr|int:LO..HI]`, config/synth_shapes.txt syntax; this batch's lines
// travel with the PR rather than in that file).
// No names: none has evidence (the addresses are the names).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <limits>

namespace gt::game {
namespace new_batch_main_1009 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline s32 signed_of(u32 value) { return static_cast<s32>(value); }

// v0 / v1 / f0 of a callee stay in the context, as after the original's `jal`.
inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A call with no argument register set by hand (the callee only takes what the caller already has).
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A call with the argument registers set as the original sets them before the `jal` (a0-a3, t0-t3 in order): what the
// callee does not take keeps the caller's value.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// The compiler's multiply for an element count divided by 12 (the code takes the low half: `mult` then `mflo`):
// `lui 0xAAAA; ori 0xAAAB`.
constexpr u32 kElementEstimate = 0xAAAAAAABu;
// MIPS `ins rt, rs, pos, size` with rs = $zero: clears `size` bits at `pos`.
constexpr u32 insert_zero(u32 value, unsigned position, unsigned size) {
    return value & ~(((1u << size) - 1u) << position);
}
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// func_08B6A500(x = f12) -> f0: the library arc sine (fdlibm's asinf; the coefficients at 0x08CB6320..0x08CB6344 are its
// pS5..pS0 and qS4..qS1). |x| > 1 answers the PSP's default NaN (the asm's (x - x) / (x - x)); |x| == 1 answers
// x * pi/2 + x * -4.37e-8 (the split constant at 0x08CB6310 / 0x08CB6314). |x| <= 0x31FFFFFF answers huge + x > 1 -> x.
// |x| < 0.5 answers x + x * (p/q) over t = x*x. Otherwise z = (1 - |x|)/2, p and q over z, s = sqrtf(z): |x| > 0.975
// answers pi/2 - (2*(s + s*w) + 4.37e-8) with w = p/q, else pi/4 - (2*s*w - (-2*c - 4.37e-8) - (pi/4 - 2*s12)) with
// c = (z - s12*s12)/(s + s12) and s12 = s with its low 12 bits cleared; the bit pattern of x (s2 > 0) keeps or negates it.
// f12 and f0 end equal on every exit. The operands are bounded, so the PSP's inf * 0 NaN cannot arise here. The NaN of
// the |x| > 1 answer is the default one (0x7FC00000, from 0/0); a NaN *argument* comes back quieted by the original, and
// quiet_NaN() is the same bit pattern for every payload of the synthetic and captured inputs.


// 0x0891EB20(self = a0, ramp = a1, flag = a2): advances the four-phase ramp held on `ramp` (float value at +0x218,
// phase at +0x21C, "started" flag at +0x220, row at +0x224, table selector at +0x228), clamps the progress to 1.0 and
// calls 0x0891E2A8(self, ramp, &copy, progress) with one 0x48-byte row of the table the selector picks
// (1: 0x08CF2604, 2: 0x08CF252C, 3: 0x08CF2454, else 0x08CF26DC), row `ramp->+0x224`, copied onto the frame. Phase 0
// with a negative row first draws a seed (0x08B1ED60(0x08CF28A8) -> 0x08B1EFF8 -> x4, trunc.w.s) into +0x228, or 3 when
// the flag byte is set; phase 3 clears the ramp's +0x1E0 side (0x089281E4) and moves to phase 4. v0 = self.
// The compared cases need a1 = a ramp object whose +0x218..+0x228 and +0x1E0 live in guest memory (shapes: a1 struct 576).
void func_0891EB20(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 ramp = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x70u);
    const u32 state = word(rt, ramp + 0x21Cu);
    float progress = 0.0f;
    if (state == 0u) {
        const float value = float_at(rt, ramp + 0x218u);
        progress = value / 180.0f;
        if (static_cast<s32>(word(rt, ramp + 0x224u)) < 0) {
            put(rt, ramp + 0x224u, 0u);
            const u32 seed = call_set(rt, ctx, 0x08B1ED60u, {0x08CF28A8u});
            const float scaled = call_keep(rt, ctx, 0x08B1EFF8u, {seed}).f0 * 4.0f;
            put(rt, ramp + 0x228u, ctx.fpu_float_to_word(scaled, 1u));
            if (flag != 0u) put(rt, ramp + 0x228u, 3u);
        }
        if (180.0f <= value) {
            put(rt, ramp + 0x220u, 0u);
            put(rt, ramp + 0x21Cu, state + 1u);
        }
    } else if (state == 1u) {
        if (word(rt, ramp + 0x220u) == 0u) {
            put(rt, ramp + 0x220u, 1u);
            put8(rt, ramp + 0xC0u, 1u);
        }
        const float value = float_at(rt, ramp + 0x218u);
        put(rt, ramp + 0x224u, 1u);
        if (240.0f <= value) {
            put(rt, ramp + 0x21Cu, 2u);
            put(rt, ramp + 0x220u, 0u);
        }
        progress = (value - 180.0f) / 60.0f;
    } else if (state == 2u) {
        if (word(rt, ramp + 0x220u) == 0u) {
            put(rt, ramp + 0x220u, 1u);
            put8(rt, ramp + 0xC0u, 1u);
        }
        const float value = float_at(rt, ramp + 0x218u);
        put(rt, ramp + 0x224u, 2u);
        if (300.0f <= value) {
            put(rt, ramp + 0x21Cu, 3u);
            put(rt, ramp + 0x220u, 0u);
        }
        progress = (value - 240.0f) / 60.0f;
    } else if (state == 3u) {
        if (word(rt, ramp + 0x220u) == 0u) {
            put(rt, ramp + 0x220u, 1u);
            put8(rt, ramp + 0xC0u, 1u);
        }
        call_set(rt, ctx, 0x089281E4u, {word(rt, ramp + 0x1E0u), 0u});
        put(rt, ramp + 0x220u, 0u);
        put(rt, ramp + 0x21Cu, word(rt, ramp + 0x21Cu) + 1u);
    }
    if (1.0f < progress) progress = 1.0f;
    const u32 selector = word(rt, ramp + 0x228u);
    u32 base = 0x08CF26DCu;
    if (selector == 1u) base = 0x08CF2604u;
    else if (selector == 2u) base = 0x08CF252Cu;
    else if (selector == 3u) base = 0x08CF2454u;
    const u32 entry = base + word(rt, ramp + 0x224u) * 72u;
    for (u32 offset = 0u; offset <= 0x44u; offset += 4u) put(rt, frame.at(offset), word(rt, entry + offset));
    (void)call_setf(rt, ctx, 0x0891E2A8u, {self, ramp, frame.at(0u)}, {progress});
    ctx.gpr[2] = self;
}

// 0x089D291C(self = a0, value = a1): rebuilds the self's two object lists under the heap lock at 0x08CF5134.
// Lock; take first = self->+0x198 and last = self->+0x19C (storing `first` back to +0x19C when they differ, else
// last = -1); unlock; 0x089CE09C(self, first) zero returns 0. A map at sp+0x20 (0x08A983D8(..., 0x08CF6ED8)) is
// filled: when 0x089CFC00(self, first) is zero, 0x089CEE5C(self, first, 1), 0x089CE9B8(self, first, 0), 0x089CF978 and
// 0x089CF8C8 (both non-zero to go on), then holder slots sp+0x1C / sp+0x18: 0x08A98C1C(&slot, 1) -> reset the map's
// value, reset slot +0x14 of the map's word to the new holder; every path continues where the map's word at +0 and the
// holder at sp+0x20 are non-zero: two holders at sp+0x28 / sp+0x2C (0x08A76AA0), a holder from `value` at sp+0x14 with
// 0x08A76AAC, the map reset, a holder at sp+0x24 and the virtual at vtable +0xCC of self->+0x180 with (self->+0x180,
// &sp+0x24, 2, &sp+0x28); then, when self->+0x184 is set and last != first, the same with three holders (sp+0x28,
// sp+0x2C, sp+0x30), two integer holders from `first` / `last` (0x08A85B1C), slot 0xCC with (..., &sp+0x24, 3,
// &sp+0x28). Every holder is destroyed (0x08A5B9EC) and the map's holder too; v0 is 0x089CE09C's zero or the last
// destroy's.
// The compared cases need a0 = a live self with +0x180/+0x184 object pointers (shapes: a0 struct 416, those + ptr).
void func_089D291C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const u32 map = frame.at(0x20u);
    (void)call_set(rt, ctx, 0x08B344E0u, {0x08CF5134u});
    const u32 first = word(rt, self + 0x198u);
    u32 last = word(rt, self + 0x19Cu);
    if (first != last) put(rt, self + 0x19Cu, first);
    else last = 0xFFFFFFFFu;
    (void)call_set(rt, ctx, 0x08B3457Cu, {0x08CF5134u});
    if (call_set(rt, ctx, 0x089CE09Cu, {self, first}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    (void)call_set(rt, ctx, 0x08A983D8u, {map, 0x08CF6ED8u});
    bool skip_construction = call_set(rt, ctx, 0x089CFC00u, {self, first}) != 0u;
    if (!skip_construction) {
        (void)call_set(rt, ctx, 0x089CEE5Cu, {self, first, 1u});
        (void)call_set(rt, ctx, 0x089CE9B8u, {self, first, 0u});
        skip_construction = call_set(rt, ctx, 0x089CF978u, {self, first}) == 0u ||
                            call_set(rt, ctx, 0x089CF8C8u, {self, first}) == 0u;
    }
    if (!skip_construction) {
        const u32 temp = frame.at(0x1Cu);
        (void)call_set(rt, ctx, 0x08A98C1Cu, {temp, 1u});
        (void)call_set(rt, ctx, 0x08A5BAD8u, {map, word(rt, temp)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
        const u32 next = word(rt, word(rt, map) + 0x14u);
        (void)call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0x18u), first});
        if (next != frame.at(0x18u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {next, word(rt, frame.at(0x18u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
    }
    u32 first_slot = 0u;
    if (word(rt, self + 0x180u) != 0u && word(rt, map) != 0u) {
        first_slot = frame.at(0x28u);
        for (u32 slot = first_slot; slot != frame.at(0x30u); slot += 4u) (void)call_set(rt, ctx, 0x08A76AA0u, {slot});
        put(rt, frame.at(0x10u), value);
        (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(0x14u), frame.at(0x10u)});
        if (first_slot != frame.at(0x14u))
            (void)call_set(rt, ctx, 0x08A5BAD8u, {first_slot, word(rt, frame.at(0x14u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
        if (frame.at(0x2Cu) != map) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x2Cu), word(rt, map)});
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x24u)});
        const u32 object = word(rt, self + 0x180u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA697Cu});
        const u32 table = word(rt, object);
        ctx.gpr[3] = word(rt, table + 0xCCu);
        ctx.gpr[2] = table;
        (void)call_keep(rt, ctx, ctx.gpr[3], {object, frame.at(0x24u), 2u, first_slot});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x24u)});
        for (u32 slot = frame.at(0x2Cu);;) {
            (void)call_set(rt, ctx, 0x08A5B9ECu, {slot});
            if (slot == first_slot) break;
            slot -= 4u;
        }
    }
    if (word(rt, self + 0x184u) != 0u && last != first) {
        const u32 second_slot = frame.at(0x28u);
        for (u32 slot = second_slot; slot != frame.at(0x34u); slot += 4u) (void)call_set(rt, ctx, 0x08A76AA0u, {slot});
        put(rt, frame.at(8u), value);
        (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(0xCu), frame.at(8u)});
        if (second_slot != frame.at(0xCu))
            (void)call_set(rt, ctx, 0x08A5BAD8u, {second_slot, word(rt, frame.at(0xCu))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        (void)call_set(rt, ctx, 0x08A85B1Cu, {frame.at(4u), first});
        if (frame.at(0x2Cu) != frame.at(4u))
            (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x2Cu), word(rt, frame.at(4u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        (void)call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), last});
        if (frame.at(0x30u) != frame.at(0u))
            (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x30u), word(rt, frame.at(0u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x24u)});
        const u32 object = word(rt, self + 0x184u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA697Cu});
        const u32 table = word(rt, object);
        ctx.gpr[3] = word(rt, table + 0xCCu);
        ctx.gpr[2] = table;
        (void)call_keep(rt, ctx, ctx.gpr[3], {object, frame.at(0x24u), 3u, second_slot});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x24u)});
        for (u32 slot = frame.at(0x30u);;) {
            (void)call_set(rt, ctx, 0x08A5B9ECu, {slot});
            if (slot == second_slot) break;
            slot -= 4u;
        }
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {map});
}

// 0x08AEE144(owner = a0, index = a1): creates the indexed sub-manager of the global audio state at 0x08D41AD0.
// 0x08AD8960(owner, 0x08C92378, 0x08D41FB0, 0x10) and 0x08AD8960(owner, 0x08C9238C, 0x08D41FB4, 0x4000) register it,
// the fields +0x4E8..+0x530 are reset, then handle = 0x08AD84B4(index << 5, 1) goes to +0x500. With a handle, five
// sub-objects are created in order (+0x4F0, +0x4F4, +0x4F8, +0x4FC, +0x4EC, then 0x08C7A9D0 on +0x4EC); the first four
// failures log line 0xB37, the fifth 0xB3F, the last 0xB45 (only when word(0x08CDFB3C) bit 0 is set) and fall to the
// cleanup, which releases every non-negative handle (+0x4EC, +0x4FC, +0x4F8, +0x4F4, +0x4F0 with -1) and destroys
// +0x500. Success sets word(0x08D41AD0 + 0x4DC) = 1 and returns 1, every other path returns 0.
// The compared cases need a0 = a live string/struct 0x08AD8960 can read (shapes: a0 struct 64); a1 is an index.

// 0x08C0E874(container = a0, position = a1, count = a2, elements = a3): inserts `count` copies of the 12-byte element
// range at a3 into the container's block at `position`. The container is {block at +0, position field at +4, end at +8}
// of 12-byte elements. The count estimate is `((end - pos) >> 2) * 0xAAAAAAAB` (the compiler's multiply, the low half
// as the code takes it). Not below `count` grows the block: bytes = (est + max(est, count)) * 12 through
// 0x08A5BC20, then 0x08C0E0F8(block, position, new_block, &flag), 0x08C0E1AC(new_block, count, a3, &flag),
// 0x08C0E0F8(position, pos, count * 12 + temp, &flag), 0x08C0DC90(block, pos, &flag), the old block freed with
// 0x08A5BC54, and the three fields replaced. Otherwise a temporary 12-byte guard object is set on the frame (its block
// from 0x08B35840(0x10, size), size = ((a3+4 - *a3) & ~0x1F), filled by 0x08C0D7F8), the tail moved when the distance
// to `position` is below `count` (0x08C0E054 then a backward 0x08C0E654 loop) or the head filled (0x08C0E30C,
// 0x08C0E258), 0x08C0E814 closes it, 0x08BE5888 destroys the guard and 0x08B35254 frees it. v0 is the entry v0 with
// count == 0, else the last call's.
// The compared cases need a0 = a container whose +0 is a live block (shapes: a0 struct 16, +0 ptr) and a3 = a live
// 12-byte range (shapes: a3 struct 16).

// 0x08990A1C(reader = a0): splits the text the reader's stream (a0->+0x12C) delivers into lines. 0x08A39BE0(stream,
// buffer, 0x101) reads up to 0x101 bytes onto the frame (NUL-terminated at the returned length); the line counter is
// reader->+0x224, reader->+0x220 the pending one, the lines live in the smart-pointer array at +0x228 (4 bytes each) and
// the byte marks at +0x428. When the two counters differ, 0x0898D9E8(reader, &+0x224, 0x80, -1) resets the pending
// line; otherwise the current line's holder is re-acquired (0x08B855E0) and released (0x08B60804 / 0x08B8542C), the mark
// byte set and 0x0898D9E8(reader, &+0x224, 0x80, 1) advances the counter (then &+0x220 when both became equal). Each
// line: the text up to the '\n' is built by 0x08B85EB8 and assigned into the line's slot (0x08B89054), its holder
// released; a line with a '\n' also advances the counters as above. Both paths take the slot index from the word at
// sp+0x18 (the counter the last advance stored), loaded in the `bne` delay slot at 0x08990B90.
// Returns the number of lines that advanced the counter (the caller's s6 when neither branch ran).
// The compared cases need a0 = a reader whose +0x12C stream and +0x220../+0x228 counters are live (shapes: a0 struct 1024, +0x12C ptr, the two counters small ints).
void func_08990A1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 reader = ctx.gpr[4];
    GuestFrame frame(ctx, 0x150u);
    const auto release_holder = [&](u32 holder, u32 flag) {
        if (holder - 0xCu != 0x08D74FD0u &&
            static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {holder - 4u, 0xFFFFFFFFu})) <= 0)
            (void)call_set(rt, ctx, 0x08B8542Cu, {holder - 0xCu, flag});
    };
    const u32 stream = word(rt, reader + 0x12Cu);
    if (stream == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 stream_length = word(rt, stream + 0x20u);
    const u32 length = call_set(rt, ctx, 0x08A39BE0u, {stream, frame.at(0x1Cu), 0x101u});
    if (static_cast<s32>(length) <= 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    rt.memory().store8(frame.at(0x1Cu + length), 0u);
    u32 lines = ctx.gpr[22];
    const u32 index0 = word(rt, reader + 0x224u);
    if (index0 != word(rt, reader + 0x220u)) {
        put(rt, frame.at(0x18u), index0);
        (void)call_set(rt, ctx, 0x0898D9E8u, {reader, frame.at(0x18u), 0x80u, 0xFFFFFFFFu});
        lines = 0u;
    } else {
        put(rt, frame.at(0x14u), (0x08D74FD0u + 0xCu));
        (void)call_set(rt, ctx, 0x08B855E0u, {reader + 0x228u + index0 * 4u, frame.at(0x14u)});
        release_holder(word(rt, frame.at(0x14u)), frame.at(3u));
        const u32 current = word(rt, reader + 0x224u);
        rt.memory().store8(reader + 0x428u + current, 1u);
        put(rt, frame.at(0x18u), current);
        (void)call_set(rt, ctx, 0x0898D9E8u, {reader, reader + 0x224u, 0x80u, 1u});
        if (word(rt, reader + 0x224u) != word(rt, reader + 0x220u)) {
            lines = 1u;
        } else {
            (void)call_set(rt, ctx, 0x0898D9E8u, {reader, reader + 0x220u, 0x80u, 1u});
            lines = 1u;  // `addiu s6, zero, 1` sits in the `bne` delay slot: both answers leave 1
        }
    }
    if (static_cast<s32>(stream_length) > 0) {
        u32 index = 0u;
        u32 start = 0u;
        for (;;) {
            u32 end = index;
            while (end < stream_length && static_cast<std::int8_t>(rt.memory().load8(frame.at(0x1Cu + end))) != '\n') ++end;
            if (end < stream_length) {
                rt.memory().store8(frame.at(0x1Cu + end), 0u);
                const u32 slot = reader + 0x228u + word(rt, frame.at(0x18u)) * 4u;
                (void)call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x10u), frame.at(0x1Cu) + start, frame.at(5u)});
                (void)call_set(rt, ctx, 0x08B89054u, {slot, frame.at(0x10u)});
                release_holder(word(rt, frame.at(0x10u)), frame.at(2u));
                put(rt, frame.at(0xCu), (0x08D74FD0u + 0xCu));
                const u32 current_slot = reader + 0x228u + word(rt, reader + 0x224u) * 4u;
                (void)call_set(rt, ctx, 0x08B855E0u, {current_slot, frame.at(0xCu)});
                release_holder(word(rt, frame.at(0xCu)), frame.at(1u));
                const u32 current = word(rt, reader + 0x224u);
                rt.memory().store8(reader + 0x428u + current, 1u);
                put(rt, frame.at(0x18u), current);
                (void)call_set(rt, ctx, 0x0898D9E8u, {reader, reader + 0x224u, 0x80u, 1u});
                if (word(rt, reader + 0x224u) != word(rt, reader + 0x220u)) {
                    lines += 1u;
                } else {
                    (void)call_set(rt, ctx, 0x0898D9E8u, {reader, reader + 0x220u, 0x80u, 1u});
                    lines += 1u;  // the increment sits in the `bne` delay slot at 0x08990C80
                }
                index = end + 1u;
            } else {
                const u32 slot = reader + 0x228u + word(rt, frame.at(0x18u)) * 4u;  // s1 = the word at 0x08990B90
                (void)call_set(rt, ctx, 0x08B85EB8u, {frame.at(8u), frame.at(0x1Cu) + start, frame.at(4u)});
                (void)call_set(rt, ctx, 0x08B89054u, {slot, frame.at(8u)});
                release_holder(word(rt, frame.at(8u)), frame.at(0u));
                index = end + 1u;
            }
            if (!(static_cast<s32>(index) < static_cast<s32>(stream_length))) break;
            start = index;
        }
    }
    ctx.gpr[2] = lines;
}

// 0x08A543C8(container = a0, parameter = a1, holder = a2): walks the tree of `holder`'s object and applies it to
// `container`. object = *holder; slot 0x90(object) zero takes the other shape: slot 0xD4(object) zero returns,
// otherwise the virtual at word(word(parameter) + 0xE0) with (parameter, 0, slot 0xDC(object, 0), slot 0xD8(object)).
// The walking shape builds an iterator at sp+8 (slot 0xA8(&iterator, object)) and loops: slot 8 of the iterator's
// +0x14 object zero ends it (0x08A5B9EC(&iterator)); otherwise slot 0xC advances it and the body builds a holder from
// +0x10 of the entry's +0x14 via 0x08A76ABC, fills sp+4 with slot 0x84(&sp+4, object, 0), sp+0x10 with slot 0x5C(...,
// object, 0), destroys sp+4, fills sp+0x14 with slot 0x84(&sp+0x14, object, 1), looks the entry up (0x08C1D1AC of
// container + 0x38 and sp+0x10) and, unless it is container + 0x3C, calls 0x08A57B54(&sp, &entry->+0x14 object + 0xC,
// parameter + 0x18, entry->+0x14 object->+0x10 + (parameter->+0x1C or parameter->+0x18->+0xC)) and slot 0xF4 of the
// result with (&sp, sp+0x14); then destroys sp, sp+0x14, 0x08A69D68(sp+0x10) and sp+0xC. Every call is a null-checked
// virtual (`lw v1, 0(object); lw v0, slot(v1); jalr v0`), 0x08A5BB7C only being called with the class name of a null one.
// The compared cases need a0 = the container and a2 = a live holder (shapes: a2 struct 16, +0 ptr).
void func_08A543C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 container = ctx.gpr[4];
    const u32 parameter = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    // `lw v1, 0(object); lw v0, slot(v1); jalr v0`: v1 holds the vtable at the jalr, v0 the target.
    const auto virtual_call = [&](u32 object, u32 slot, std::initializer_list<u32> arguments) -> u32 {
        const u32 table = word(rt, object);
        ctx.gpr[3] = table;
        return call_indirect(rt, ctx, word(rt, table + slot), arguments);
    };
    // The other allocation: `lw v0, 0(object); lw v1, slot(v0); jalr v1` - the target in v1, the vtable left in v0.
    const auto virtual_call_v1 = [&](u32 object, u32 slot, std::initializer_list<u32> arguments) -> u32 {
        const u32 table = word(rt, object);
        ctx.gpr[2] = table;
        ctx.gpr[3] = word(rt, table + slot);
        return call_keep(rt, ctx, ctx.gpr[3], arguments).v0;
    };
    const u32 object = word(rt, holder);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
    if (virtual_call(object, 0x90u, {object}) == 0u) {
        u32 current = word(rt, holder);
        if (current == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        if (virtual_call(current, 0xD4u, {current}) == 0u) return;
        const u32 source = word(rt, word(rt, parameter) + 0xE0u);  // the delay slot of the test above
        current = word(rt, holder);
        if (current == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        const u32 middle = virtual_call(current, 0xDCu, {current, 0u});
        current = word(rt, holder);
        if (current == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        const u32 last = virtual_call(current, 0xD8u, {current});
        ctx.gpr[2] = last;
        (void)call_keep(rt, ctx, source, {parameter, 0u, middle, last});
        return;
    }
    (void)virtual_call_v1(word(rt, holder), 0xA8u, {frame.at(8u), word(rt, holder)});
    for (;;) {
        u32 iterator = word(rt, frame.at(8u));
        if (iterator == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF5ECu});
        if (virtual_call(word(rt, iterator + 0x14u), 8u, {word(rt, iterator + 0x14u)}) == 0u) break;
        (void)virtual_call(word(rt, iterator + 0x14u), 0xCu, {word(rt, iterator + 0x14u)});
        iterator = word(rt, frame.at(8u));
        if (iterator == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF5ECu});
        (void)call_set(rt, ctx, 0x08A76ABCu, {frame.at(0xCu), word(rt, iterator + 0x14u) + 4u});
        const u32 first = word(rt, frame.at(0xCu));
        if (first == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        (void)virtual_call(first, 0x84u, {frame.at(4u), first, 0u});
        const u32 second = word(rt, frame.at(4u));
        if (second == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        (void)virtual_call(second, 0x5Cu, {frame.at(0x10u), second, 0u});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        const u32 third = word(rt, frame.at(0xCu));
        if (third == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF520u});
        (void)virtual_call(third, 0x84u, {frame.at(0x14u), third, 1u});
        const u32 found = call_set(rt, ctx, 0x08C1D1ACu, {container + 0x38u, frame.at(0x10u)});
        if (found != container + 0x3Cu) {
            u32 entry = word(rt, found + 0x14u);
            if (entry == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
            entry = word(rt, entry + 0x10u);
            u32 extra;
            if (word(rt, parameter + 0x1Cu) != 0u) {
                extra = word(rt, parameter + 0x1Cu) + entry;
            } else {
                const u32 base = word(rt, parameter + 0x18u);
                if (base == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF540u});
                extra = word(rt, base + 0xCu) + entry;
            }
            const u32 key = word(rt, found + 0x14u);
            if (key == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
            (void)call_set(rt, ctx, 0x08A57B54u, {frame.at(0u), key + 0xCu, parameter + 0x18u, extra});
            const u32 value = word(rt, frame.at(0u));
            if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF5E0u});
            (void)virtual_call(value, 0xF4u, {value, frame.at(0x14u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x10u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
}

} // namespace
} // namespace new_batch_main_1009
} // namespace gt::game

#define NEW1009(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1009::name, gt::ReturnKind::kind)
NEW1009(0x0891EB20u, func_0891EB20, Word);
NEW1009(0x089D291Cu, func_089D291C, Word);
NEW1009(0x08990A1Cu, func_08990A1C, Word);
NEW1009(0x08A543C8u, func_08A543C8, Word);
