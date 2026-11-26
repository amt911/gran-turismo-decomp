// New batch, main session 1025: functions that earlier drafts either mismatched in the gate or left alone, written
// again by hand as structured C from `tools.disasm`.
//
//  0x08815DBC  mirrors a state object (a1) into an owner (a0): the mode word at +0x270 with its two setters, three
//              element callbacks at +0x180..+0x188, a tick delta that retimes the countdown at +0x278, a fade label
//              while the state's mode word is positive, a 20-per-unit tick sweep, then the four-slot retime of the
//              three owner slots (+0x64 / +0x68 / +0x6C) through 0x08812198. v0 is the value the original leaves.
//  0x0895DEC8  the gear / assist state machine of a car (word(a0+8)) with its state block (word(a0+0xC)): the
//              countdown at +0x512, the speed bands, the angle the next gear needs, and the four-slot counter search.
//              The previous draft dropped both returns of the counter search (the matched slot's zero sample and the
//              +0x3C < +0x38 * +0x164 test) and ran on; this one leaves -1 there as the original does.
//
// Skipped, not drafted here:
//  0x08BB4C74  libstdc++ `std::vector<T>::_M_insert_aux` (the string "vector::_M_insert_aux" is one call away)
//              over a static pool allocator at 0x08D75F5C: a container reorganisation, not game code, and its
//              free-list walk would be a transcription rather than a decompilation.
//  0x08B24DF8  a variable-length-code / Huffman table builder: a 17-entry length table on a 0x170 frame, seven
//              incoming argument registers (a0-a3, t1, t2) and no calls; the bit-reversal loops need the table
//              layout confirmed before a draft is worth the gate run.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1025 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                        std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_result(rt, ctx, target, arguments).v0;
}
u32 call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
               std::initializer_list<float> floats) {
    return call_result(rt, ctx, target, arguments, floats).v0;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }

// --- 0x08815DBC: a0 = the owner, a1 = the state object. v0 on every exit is the value the original leaves: the
// state's mode word (word(a1+0x110)) when the retime runs, the state byte (a1+0x108) when a slot already matched,
// and zero when the still-matching zero path stores it.

// --- 0x0895DEC8: a0 = the object (word(a0+8) is the car, word(a0+0xC) the state block), a1 = a mode. v0 is the
// value the original leaves in a0: a gear to select, -1 for none, or the next counter value.
void func_0895DEC8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const u32 car = word(rt, self + 8u);
    const u32 state = word(rt, self + 0xCu);
    const u32 tuned = state + 0x270u;
    const u32 countdown = byte_at(rt, car + 0x512u);
    u32 index = byte_at(rt, car + 0x5F8u);

    if (countdown != 0u) {
        // The countdown runs: one step down, and a step that lands on zero restarts it when +0x511 is set.
        index = (countdown - 1u) & 0xFFu;
        rt.memory().store8(car + 0x512u, static_cast<std::uint8_t>(index));
        if (call(rt, ctx, 0x0895DC6Cu, {car}) == 2u && index == 0u &&
            static_cast<std::int8_t>(byte_at(rt, car + 0x511u)) != 0)
            rt.memory().store8(car + 0x512u, 1u);
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    if (static_cast<std::int8_t>(byte_at(rt, car + 0x510u)) != 0) {  // a latched state: 0 when the index is set
        ctx.gpr[2] = index != 0u ? 0u : 0xFFFFFFFFu;
        return;
    }
    if (call(rt, ctx, 0x089557BCu, {self}) != 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }

    const float speed = float_at(rt, car + 0x6C4u);
    const float slow = std::bit_cast<float>(0x3E8E38E4u);  // 1/3.6: km/h to m/s
    const bool index_is_zero = index == 0u;
    if (slow < speed && index_is_zero) {
        ctx.gpr[2] = 1u;
        return;
    }
    const float oversteer = float_at(rt, car + 0x530u);
    if (0.0f < oversteer) {
        if (speed < slow) {
            if (index != 1u) {
                ctx.gpr[2] = 1u;
                return;
            }
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
    } else if (speed < -slow) {
        if (index != 0u) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
    }
    if (index > 0u) {
        // The angle the next gear would need: (0x1F4 + halfword at state+0x23C) * (pi/60) over the per-gear rate.
        const u32 limit = byte_at(rt, tuned + 0x10u);
        if (index < limit) {
            const float needed = static_cast<float>(static_cast<s32>(rt.memory().load16(state + 0x23Cu)) + 0x1F4) *
                                 std::bit_cast<float>(0x3DD67750u);
            if (needed / float_at(rt, tuned + 0x80u + index * 4u) < speed) {
                ctx.gpr[2] = index + 1u;
                return;
            }
        }
    }

    const u32 kind = call(rt, ctx, 0x0895DC6Cu, {car});
    if (kind == 1u) {
        if (static_cast<std::int8_t>(byte_at(rt, car + 0x511u)) != 0) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
    } else if (kind == 2u) {
        const s32 gate = static_cast<std::int8_t>(byte_at(rt, car + 0x511u));
        if (gate > 0 && index < byte_at(rt, state + 0x280u)) {
            rt.memory().store8(car + 0x512u, 1u);
            ctx.gpr[2] = index + 1u;
            return;
        }
        if (gate < 0 && index >= 2u) {
            rt.memory().store8(car + 0x512u, 1u);
            ctx.gpr[2] = index - 1u;
            return;
        }
    }
    if (byte_at(rt, car + 0x5F9u) != 1u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }

    if (mode != 0u) {
        // The assist asks for a gear itself; it must differ from the current one.
        if (index_is_zero) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        const u32 answer = call_float(rt, ctx, 0x0895DD80u, {self, 0u}, {speed});
        const s32 limit = static_cast<s32>(byte_at(rt, state + 0x280u));
        const s32 clamped = static_cast<s32>(answer) < limit ? static_cast<s32>(answer) : limit;
        ctx.gpr[2] = clamped == static_cast<s32>(index) ? 0xFFFFFFFFu : static_cast<u32>(clamped);
        return;
    }
    if (index == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }

    s32 delta;
    if (index == 1u) {
        if (byte_at(rt, tuned + 0x10u) == index) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        delta = static_cast<s32>(call(rt, ctx, 0x0895DD1Cu, {self, 1u}));
    } else {
        delta = index < byte_at(rt, tuned + 0x10u)
                    ? static_cast<s32>(call(rt, ctx, 0x0895DD1Cu, {self, index}))
                    : 20000;
    }
    s32 score = static_cast<s32>(call(rt, ctx, 0x0895DCB8u, {self, index}));
    // The three assist weights (bytes in .data) nudge the score by 100 each.
    u32 score_bits = static_cast<u32>(score);
    if (oversteer < 0.05f) score_bits += byte_at(rt, 0x08CDE236u) * 100u;
    if (0.8f < float_at(rt, car + 0x538u)) score_bits += byte_at(rt, 0x08CDE237u) * 100u;
    if (!(0.25f < oversteer)) score_bits -= byte_at(rt, 0x08CDE235u) * 100u;
    score = static_cast<s32>(score_bits);

    // The state byte at tuned+0 selects which counter slots are acceptable (bit 0xA06: none, 0x508: the upper two).
    // The lower-slot flag starts as the incoming mode argument: the original leaves it in s5 when the 0xF1 bit is
    // the one that fires.
    bool upper = false;      // s7
    bool lower = mode != 0u; // s5
    const u32 state_kind = byte_at(rt, tuned);
    if (state_kind < 0xCu) {
        const u32 bits = 1u << state_kind;
        upper = true;
        if ((bits & 0xA06u) != 0u) {
            lower = false;
        } else if ((bits & 0x508u) != 0u) {
            lower = true;
        } else if ((bits & 0xF1u) != 0u) {
            upper = false;
        } else {
            upper = false;
            lower = false;
        }
    } else {
        lower = false;
    }

    const s32 boundary = static_cast<std::int16_t>(rt.memory().load16(car + 0x58Eu));
    if (boundary < score) {
        // Downshift candidate: the previous gear, gated by the sub-state and the pedal.
        const u32 previous = byte_at(rt, car + 0x5F8u) - 1u;
        if (byte_at(rt, tuned + 2u) != 1u) {
            ctx.gpr[2] = previous;
            return;
        }
        if (call(rt, ctx, 0x0895BBB4u, {tuned}) == 0u) {
            ctx.gpr[2] = previous;
            return;
        }
        if (float_at(rt, car + 0x53Cu) <= 0.8f) {
            ctx.gpr[2] = previous;
            return;
        }
        const u32 answer = call_float(rt, ctx, 0x0895DD80u, {self, 0u}, {float_at(rt, car + 0x6C4u)});
        ctx.gpr[2] = static_cast<s32>(answer) < static_cast<s32>(previous) ? answer : 0xFFFFFFFFu;
        return;
    }
    if (delta >= boundary) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }

    // The four-slot counter search; none of the four selects the next counter value.
    const u32 counter_limit = byte_at(rt, car + 0x5F8u) + 1u;
    for (u32 slot = 0u;;) {
        const bool below_two = slot < 2u;
        if (!(upper && below_two) && !(lower && !below_two)) {
            ++slot;
            if (slot != 4u) continue;
            ctx.gpr[2] = counter_limit;
            return;
        }
        // The matched slot: its sample's float at +0x54 must be non-zero, then the scaled value is compared.
        const u32 sample = call(rt, ctx, 0x08955300u, {self});
        if (float_at(rt, sample + 0x54u) == 0.0f) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        const u32 scaled = call(rt, ctx, 0x0894268Cu, {state, slot, byte_at(rt, sample + 0xE9u)});
        if (float_at(rt, sample + 0x3Cu) < float_at(rt, scaled + 0x164u) * float_at(rt, sample + 0x38u)) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        ++slot;
        if (slot != 4u) continue;
        ctx.gpr[2] = counter_limit;
        return;
    }
}

} // namespace
} // namespace new_batch_main_1025
} // namespace gt::game

#define NEW1025(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1025::name, gt::ReturnKind::kind)
NEW1025(0x0895DEC8u, func_0895DEC8, Word);
