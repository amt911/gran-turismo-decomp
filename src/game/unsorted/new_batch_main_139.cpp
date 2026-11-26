// New batch (main session 139): two long float/state functions, written as structured C from `tools.disasm`. Both were emitted for
// the first time and no route is known to call them, so they are registered for the synthetic gate to compare (drafted, unverified).
//
// 0x08948324 is a per-frame update of one object's two-sided motion state: it reads a frame scale, a blend factor and a distance
// through the accessors 0x0895549C / 0x08949128 / 0x08947E60 / 0x08947BA4 / 0x08955300, derives the two per-side values at
// state + 0x5F4 / + 0x9C, and writes them back through 0x08955300. Its `a1` is never read (the caller passes 7).
// 0x08B456D0 is a script-handler table: a 26-way switch on the id at *(a1 + 0x10) + 0x14 picks one of the handler functions, each
// called with this / the object / a field of the object / the lookup's answer plus the object's word, halfword and +8 word, and the
// result is finished by a virtual call whose answer selects a tail call.
//
// The f20-f25 registers are the original's own (saved and restored in its frame), so they are plain locals here; the FPU registers
// the caller sees ($f0, the VFPU) are set by the calls as the original leaves them. Every argument register a callee gets is passed
// (a0-a3 through `call_set`'s list, t0 / t2 / t3 written to ctx like the original), and every branch reads its `jal` delay slot as
// "runs before the call" and its likely-branch delay slot as "runs only when taken".
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_139 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s16 = std::int16_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;

// A guest call with the argument registers the original sets before the `jal` (a0-a3, and f12-f15 for the float ones): the
// callee starts from this context, so a register this call does not pass keeps what the C++ side had, as in the original.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                     std::initializer_list<float> float_arguments = {}) {
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

inline u8 load8(Runtime &rt, u32 address) { return static_cast<u8>(rt.memory().load8(address)); }
inline u16 load16(Runtime &rt, u32 address) { return static_cast<u16>(rt.memory().load16(address)); }
inline void store32(Runtime &rt, u32 address, u32 value) { rt.memory().store32(address, value); }
inline void store8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }

// --- 0x08948324 ---

// The two per-side values the original keeps in its frame at sp + 0xC (side 0) and sp + 0x10 (side 1), and the two sums at sp + 4
// and sp + 8.
struct Sides {
    float value[2];
    float sum[2];
};

// The band mapping each arm of the update has: `blend * (half + 1)` and `blend * (1 - half)` into value[0] / value[1] (the order
// differs between the arm that runs with blend above zero and the one that runs with it at or below zero), then the correction that
// clamps the larger-magnitude side at the band's edge and shoves the excess into the other. Each of the four blocks has its own
// edge and its own side, so they are spelled out rather than parameterised.
enum class Band { AboveZeroNear, AboveZeroFar, BelowZeroNear, BelowZeroFar };

void apply_band(Sides &sides, float half, float blend, Band which) {
    const float plus = psp_mul(blend, half + 1.0f);
    const float minus = psp_mul(blend, 1.0f - half);
    switch (which) {
    case Band::AboveZeroNear:  // 0x089485D0: value0 = blend * (1 - half), clamp it at -1 after value1's edge.
        sides.value[0] = minus;
        sides.value[1] = plus;
        if (1.0f < sides.value[1]) {
            const float excess = sides.value[1] - 1.0f;
            sides.value[1] = 1.0f;
            sides.value[0] = sides.value[0] - excess;
        }
        if (sides.value[0] < -1.0f) sides.value[0] = -1.0f;
        break;
    case Band::AboveZeroFar:  // 0x08948664: the other way round.
        sides.value[0] = minus;
        sides.value[1] = plus;
        if (1.0f < sides.value[0]) {
            const float excess = sides.value[0] - 1.0f;
            sides.value[0] = 1.0f;
            sides.value[1] = sides.value[1] - excess;
        }
        if (sides.value[1] < -1.0f) sides.value[1] = -1.0f;
        break;
    case Band::BelowZeroNear:  // 0x08948904: value0 = blend * (half + 1), clamped at -1, then value1 at 0.
        sides.value[0] = plus;
        sides.value[1] = minus;
        if (sides.value[0] < -1.0f) {
            const float excess = sides.value[0] + 1.0f;
            sides.value[0] = -1.0f;
            sides.value[1] = sides.value[1] - excess;
        }
        if (0.0f < sides.value[1]) sides.value[1] = 0.0f;
        break;
    case Band::BelowZeroFar:  // 0x08948998: the other way round.
        sides.value[0] = plus;
        sides.value[1] = minus;
        if (sides.value[1] < -1.0f) {
            const float excess = sides.value[1] + 1.0f;
            sides.value[1] = -1.0f;
            sides.value[0] = sides.value[0] - excess;
        }
        if (0.0f < sides.value[0]) sides.value[0] = 0.0f;
        break;
    }
}

// 0x08948324 (a0 = object): per-frame update; a1 is not read. Returns the accessor's answer in v0, as the original leaves it.
void func_08948324(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const u32 state = word(rt, object + 8u);
    const u32 unit = word(rt, object + 0xCu) + 0x270u;

    const float scale = call_set(rt, ctx, 0x0895549Cu, {object}).f0;
    const u32 mode = load8(rt, state + 0x5F9u);
    store32(rt, state + 0x5F4u, 0u);
    const float target = mode == 3u ? float_at(rt, state + 0x530u) : float_at(rt, state + 0x534u);
    const float rate = call_set(rt, ctx, 0x08949128u, {object}, {target}).f0;
    const float facing = static_cast<float>(static_cast<s16>(load16(rt, state + 0x58Eu)));
    float blend = float_at(rt, 0x08CF2EF4u);
    float side = psp_mul(rate, facing);
    side = psp_mul(side, std::bit_cast<float>(0x38DBC304u));

    constexpr float kOne = 1.0f;
    const float turn = call_set(rt, ctx, 0x08947E60u, {object}).f0;
    const float decay = kOne - blend;
    const float curve = float_at(rt, 0x08CF2EA4u);
    const float divisor = float_at(rt, 0x08CF2EA8u);
    side = side + psp_mul(turn, curve);
    const float doubled = divisor + divisor;
    side = psp_mul(side, blend);
    side = side / decay;
    blend = side / doubled;
    if (kOne < blend) {
        blend = kOne;
    } else if (blend < -kOne) {
        blend = -kOne;
    }
    const float curvature = call_set(rt, ctx, 0x08947BA4u, {object}).f0;
    const float eased = curvature + psp_mul(kOne - curvature, float_at(rt, 0x08CF2EF0u));
    if (eased < blend) blend = eased;

    const float drag = std::bit_cast<float>(0x3A832920u);
    const float dead_zone = std::bit_cast<float>(0x3E8E38E4u);
    u32 flagged[2] = {0u, 0u};
    for (u32 i = 0u; i < 2u; ++i) {
        const u32 side_state = call_set(rt, ctx, 0x08955300u, {object, i}).v0;
        float sideways = float_at(rt, side_state + 0x34u);
        if (sideways < 0.0f) sideways = -sideways;
        flagged[i] = sideways < dead_zone ? 0u : 1u;
        const u32 same = call_set(rt, ctx, 0x08955300u, {object, i}).v0;
        const float limit = float_at(rt, 0x08CF2EECu);
        float slip = float_at(rt, same + 0x98u);
        if (slip < 0.0f) slip = -slip;
        float scaled = psp_mul(psp_mul(sideways, slip), drag);
        if (limit < scaled) scaled = limit;
        (void)call_set(rt, ctx, 0x08947C74u, {object}, {psp_mul(scaled, scale)});
    }

    const float spin = psp_mul(float_at(rt, unit + 0xA8u), std::bit_cast<float>(0x40C90FDBu));
    const float spun = psp_mul(psp_mul(spin, std::bit_cast<float>(0x411CCCCDu)), std::bit_cast<float>(0x44330CCDu));
    const float distance = spun / std::bit_cast<float>(0x4230851Eu);

    Sides sides{{blend, blend}, {0.0f, 0.0f}};
    if (0.0f < blend) {
        // 0x08948560: blend above zero.
        const float bound = psp_mul(float_at(rt, 0x08CF2EE8u), float_at(rt, 0x08CF2EE4u));
        const float pressed = call_set(rt, ctx, 0x089481FCu, {object}).f0;
        float position = psp_mul(pressed, float_at(rt, 0x08CF2EE0u));
        const float released = call_set(rt, ctx, 0x089564D8u, {object}).f0;
        position = position - psp_mul(released, float_at(rt, 0x08CF2EDCu));
        const float band_width = float_at(rt, 0x08CF2ED8u);
        if (0.0f < position) {
            position = position - band_width;
            if (!(position < 0.0f)) apply_band(sides, psp_mul(position, 0.5f), blend, Band::AboveZeroNear);
        } else if (!(position < 0.0f)) {
            position = position + band_width;
            if (!(0.0f < position)) apply_band(sides, psp_mul(position, 0.5f), blend, Band::AboveZeroFar);
        }
        // 0x089486C8: the lower clamp is -bound, the upper one bound (`f23` = bound).
        for (u32 i = 0u; i < 2u; ++i) {
            if (flagged[i] == 0u) {
                const u32 side_state = call_set(rt, ctx, 0x08955300u, {object, i}).v0;
                const float ratio = float_at(rt, side_state + 0x34u);
                const float factor = psp_mul(distance, float_at(rt, 0x08CF2EA8u)) / ratio;
                float scaled = psp_mul(sides.value[i], factor);
                sides.value[i] = scaled;
                if (ratio < 0.0f) {
                    scaled = -scaled;
                    sides.value[i] = scaled;
                }
                if (bound < scaled) {
                    scaled = bound;
                } else if (scaled < -bound) {
                    scaled = -bound;
                }
                float sum = psp_mul(ratio, scaled) / distance;
                if (ratio < 0.0f) sum = -sum;
                sides.sum[i] = sum;
            } else if (!(float_at(rt, state + 0x538u) == 0.0f)) {  // bc1fl: the stores run when it is NOT zero
                sides.sum[i] = 0.0f;
                sides.value[i] = bound;
            }
        }
        const float total = psp_mul(scale, sides.sum[0] + sides.sum[1]);
        if (0.0f < total) {
            const float shrink = call_set(rt, ctx, 0x08947C04u, {object}, {total}).f0 / total;
            sides.value[1] = psp_mul(shrink, sides.value[1]);
            sides.sum[0] = psp_mul(sides.sum[0], shrink);
            sides.value[0] = psp_mul(shrink, sides.value[0]);
            sides.sum[1] = psp_mul(shrink, sides.sum[1]);
        }
        const float sum = (0.0f < sides.sum[0] ? sides.sum[0] : 0.0f) + (0.0f < sides.sum[1] ? sides.sum[1] : 0.0f);
        put_float(rt, state + 0x5F4u, sum / float_at(rt, 0x08CF2EA8u));
        if (load8(rt, state + 0x5F8u) == 0u) {
            sides.value[0] = -sides.value[0];
            sides.value[1] = -sides.value[1];
        }
        const u32 first = call_set(rt, ctx, 0x08955300u, {object, 0u}).v0;
        const float first_sum = float_at(rt, first + 0x9Cu) + sides.value[0];
        const u32 second = call_set(rt, ctx, 0x08955300u, {object, 1u}).v0;
        put_float(rt, first + 0x9Cu, first_sum);
        put_float(rt, second + 0x9Cu, float_at(rt, second + 0x9Cu) + sides.value[1]);
        return;  // v0 = `second`
    }

    // 0x08948894: blend at or below zero.
    const float bound = psp_mul(float_at(rt, 0x08CF2EE8u), float_at(rt, 0x08CF2ED4u));
    const float pressed = call_set(rt, ctx, 0x089481FCu, {object}).f0;
    float position = psp_mul(pressed, float_at(rt, 0x08CF2ED0u));
    const float released = call_set(rt, ctx, 0x089564D8u, {object}).f0;
    position = position - psp_mul(released, float_at(rt, 0x08CF2ECCu));
    const float band_width = float_at(rt, 0x08CF2EC8u);
    if (0.0f < position) {
        position = position - band_width;
        if (!(position < 0.0f)) apply_band(sides, psp_mul(position, 0.5f), blend, Band::BelowZeroNear);
    } else if (!(position < 0.0f)) {
        position = position + band_width;
        if (!(0.0f < position)) apply_band(sides, psp_mul(position, 0.5f), blend, Band::BelowZeroFar);
    }
    // 0x089489FC: the same shape with -bound / bound as the clamp and the per-side value added at +0x9C.
    for (u32 i = 0u; i < 2u; ++i) {
        float value = sides.value[i];
        if (flagged[i] == 0u) {
            const u32 side_state = call_set(rt, ctx, 0x08955300u, {object, i}).v0;
            const float ratio = float_at(rt, side_state + 0x34u);
            float scaled = psp_mul(value, psp_mul(distance, float_at(rt, 0x08CF2EA8u)) / ratio);
            if (ratio < 0.0f) scaled = -scaled;
            if (scaled < -bound) {
                value = -bound;
            } else if (bound < scaled) {
                value = bound;
            } else {
                value = scaled;
            }
        }
        if (load8(rt, state + 0x5F8u) == 0u) value = -value;
        const u32 side_state = call_set(rt, ctx, 0x08955300u, {object, i}).v0;
        put_float(rt, side_state + 0x9Cu, value + float_at(rt, side_state + 0x9Cu));
    }
}

// --- 0x08B456D0 ---

// The epilogue both the armed path and every switch case reach (0x08B45DFC): an indirect call through vtable + 0xA0, then either a
// plain return (v0 = the call's answer, or -1) or one of two tail calls selected by the id the object's record still holds.
void finish(Runtime &rt, AllegrexContext &ctx, u32 self, u32 record, u32 entry) {
    const u32 target = word(rt, word(rt, self) + 0xA0u);
    const u32 answered = call_indirect(rt, ctx, target, {self});
    if (answered == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (load8(rt, entry) == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 next = word(rt, entry + 4u);
    store8(rt, entry, 0u);
    const u32 id = word(rt, record + 0x14u);
    if (next == 0xFFFFFFFFu) {
        tail_call(ctx, 0x08B42A90u, {self, id, entry, load16(rt, record), 0u, 1u, word(rt, record + 8u),
                                     word(rt, record + 0xCu)});
        return;
    }
    tail_call(ctx, 0x08B411D4u, {self, next, id, entry, load16(rt, record), 1u});
}

// 0x08B456D0 (a0 = this, a1 = the object holding the record): dispatch, then finish.
void func_08B456D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 record = word(rt, object + 0x10u);
    const u32 entry = call_set(rt, ctx, 0x08B47B28u, {record, 0u}).v0;
    u32 id;
    if (load8(rt, entry) == 0u) {
        id = word(rt, record + 0x14u);
    } else {
        const u32 next = word(rt, entry + 4u);
        if (next == 0xFFFFFFFFu) {
            id = word(rt, record + 0x14u);
        } else {
            const u32 taken = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x90u), {self});
            if (next != taken) {
                finish(rt, ctx, self, record, entry);
                return;
            }
            id = word(rt, record + 0x14u);
        }
    }

    // The dispatch tree at 0x08B45740..0x08B458A4, as the id -> handler table it is; 0 means "no case matched" (straight to the
    // epilogue). Every case but 6 and 0x2711 passes the object's word +0xC as a2 — those two pass the caller's s0, which the original
    // never reloads on that path.
    u32 handler = 0u;
    bool from_memory = true;
    switch (id) {
    case 1u: handler = 0x08B4135Cu; break;
    case 2u: handler = 0x08B42850u; break;
    case 3u: handler = 0x08B43FC8u; break;
    case 4u: handler = 0x08B40D30u; break;
    case 5u: handler = 0x08B4257Cu; break;
    case 6u: handler = 0x08B40D54u; from_memory = false; break;
    case 7u: handler = 0x08B4173Cu; break;
    case 8u: handler = 0x08B44E64u; break;
    case 9u: handler = 0x08B434D0u; break;
    case 0x0Au: handler = 0x08B4341Cu; break;
    case 0x2711u: handler = 0x08B40D54u; from_memory = false; break;
    case 0x2712u: handler = 0x08B413F0u; break;
    case 0x2713u: handler = 0x08B40C48u; break;
    case 0x2714u: handler = 0x08B40B7Cu; break;
    case 0x2715u: handler = 0x08B436DCu; break;
    case 0x4E21u: handler = 0x08B441C4u; break;
    case 0x4E22u: handler = 0x08B40C18u; break;
    case 0x4E23u: handler = 0x08B40D54u; break;
    case 0x4E24u: handler = 0x08B44C10u; break;
    case 0x4E25u: handler = 0x08B42060u; break;
    case 0x4E26u: handler = 0x08B41320u; break;
    case 0x4E27u: handler = 0x08B452ECu; break;
    case 0x4E28u: handler = 0x08B42100u; break;
    case 0x4E29u: handler = 0x08B41834u; break;
    case 0x4E2Au: handler = 0x08B416B0u; break;
    case 0x4E2Bu: handler = 0x08B40BE0u; break;
    case 0x4E2Cu: handler = 0x08B40BA8u; break;
    default: break;
    }
    if (handler != 0u) {
        const u32 argument2 = from_memory ? word(rt, object + 0xCu) : ctx.gpr[16];
        const u32 found = call_set(rt, ctx, 0x08B47B28u, {record, 0u}).v0;
        ctx.gpr[8] = load16(rt, record);
        ctx.gpr[10] = word(rt, record + 8u);
        ctx.gpr[11] = word(rt, record + 0xCu);
        (void)call_set(rt, ctx, handler, {self, object, argument2, found});
    }
    finish(rt, ctx, self, record, entry);
}

} // namespace
} // namespace new_batch_main_139
} // namespace gt::game

#define MAIN139(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_139::name, gt::ReturnKind::kind)
MAIN139(0x08948324u, func_08948324, Word);
MAIN139(0x08B456D0u, func_08B456D0, Word);
