// New batch 130 (main session, drafted by a delegate): functions emitted by the recompiler that no src/ file defines or registers.
// Written by hand from `tools.disasm` as structured C; a draft, not yet verified by the differential gate.
//   0x08A24584 (a0 = holder, a1 = two groups of 8 bytes): for each group, builds one record per element of the group's array through
//     five string-keyed virtual calls (COW std::string temporaries released after each), pushes it on the holder's vector at +0x14, then
//     pushes the result object of 0x08A98A70 on the holder's vector.
//   0x0894AE54 (a0 = vehicle part holder, a1 = reset flag): sums four per-wheel force terms (floats), applies them to the state at +0x8.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_130 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
float bits(u32 value) { return std::bit_cast<float>(value); }

// --- 0x08A24584 -------------------------------------------------------------------------------------------------------------------

// One string-keyed virtual call on `object`: the method at vtable+0xA0 gets (object, converted key, converted value); the key string
// (built by 0x08B85EB8 from `name`) and both converted temporaries are released afterwards. The field read comes after the key
// conversion, as in the original; the string release (COW std::string: header at text-0xC) runs only when it is not the shared empty rep.
// Returns the object reloaded from `reload_at`.
u32 keyed_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 empty_rep, u32 name, u32 string_at, u32 converted_at, u32 value_at,
               u32 flag_at, u32 field_address, bool field_is_word, u32 release_at, u32 reload_at) {
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAAE44u});
    const u32 vtable = word(rt, object);
    const u32 method = word(rt, vtable + 0xA0u);
    call(rt, ctx, 0x08B85EB8u, {string_at, name, flag_at});
    call(rt, ctx, 0x08A6D2D4u, {converted_at, string_at});
    const u32 field = field_is_word ? word(rt, field_address)
                                    : static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(field_address))));
    call(rt, ctx, 0x08A85B1Cu, {value_at, field});
    call(rt, ctx, method, {object, converted_at, value_at});
    call(rt, ctx, 0x08A5B9ECu, {value_at});
    call(rt, ctx, 0x08A5B9ECu, {converted_at});
    const u32 text = word(rt, string_at);
    const u32 rep = text - 0xCu;
    if (empty_rep != rep) {
        const s32 references = static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}));
        if (references <= 0) call(rt, ctx, 0x08B8542Cu, {rep, release_at});
    }
    return word(rt, reload_at);
}

// vector push_back of the object at `item_at` on `holder`'s vector (begin +0x4, end +0x8 inside the sub-object at +0x14; 0x08B8A03C
// grows it); then the item is released through 0x08A5B9EC, so v0 is that call's result.
void push_item(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 item_at) {
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAAE80u});
    const u32 vec = holder + 0x14u;
    u32 first = word(rt, vec + 4u);
    const u32 last = word(rt, vec + 8u);
    if (first == last) {
        call(rt, ctx, 0x08B8A03Cu, {vec, word(rt, holder + 0x18u), item_at});
    } else {
        if (first != 0u) {
            call(rt, ctx, 0x08A76ABCu, {first, item_at});
            first = word(rt, vec + 4u);
        }
        put(rt, vec + 4u, first + 4u);
    }
    call(rt, ctx, 0x08A5B9ECu, {item_at});
}

void func_08A24584(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x90u);
    const u32 self = ctx.gpr[4];
    u32 group = ctx.gpr[5];
    for (u32 pass = 0u; pass != 2u; ++pass, group += 8u) {
        call(rt, ctx, 0x08A98A70u, {frame.at(0x48u)});
        if (static_cast<s32>(word(rt, group + 0xCu)) > 0) {
            const u32 empty_rep = word(rt, 0x08CB606Cu);
            s32 index = 0;
            do {
                const u32 element = word(rt, group + 0x10u) + (static_cast<u32>(index) << 4);
                call(rt, ctx, 0x08A813F4u, {frame.at(0x4Cu)});
                u32 object = word(rt, frame.at(0x4Cu));
                object = keyed_call(rt, ctx, object, empty_rep, 0x08CAAE4Cu, frame.at(0x40u), frame.at(0x44u), frame.at(0x3Cu), frame.at(9u),
                                    element + 0u, false, frame.at(4u), frame.at(0x4Cu));
                object = keyed_call(rt, ctx, object, empty_rep, 0x08CAAE58u, frame.at(0x34u), frame.at(0x38u), frame.at(0x30u), frame.at(8u),
                                    element + 2u, false, frame.at(3u), frame.at(0x4Cu));
                object = keyed_call(rt, ctx, object, empty_rep, 0x08CAAE60u, frame.at(0x28u), frame.at(0x2Cu), frame.at(0x24u), frame.at(7u),
                                    element + 4u, false, frame.at(2u), frame.at(0x4Cu));
                object = keyed_call(rt, ctx, object, empty_rep, 0x08CAAE68u, frame.at(0x1Cu), frame.at(0x20u), frame.at(0x18u), frame.at(6u),
                                    element + 6u, false, frame.at(1u), frame.at(0x4Cu));
                object = keyed_call(rt, ctx, object, empty_rep, 0x08CAAE70u, frame.at(0x10u), frame.at(0x14u), frame.at(0xCu), frame.at(5u),
                                    element + 8u, true, frame.at(0u), frame.at(0x48u));
                push_item(rt, ctx, object, frame.at(0x4Cu));
                ++index;
            } while (index < static_cast<s32>(word(rt, group + 0xCu)));
        }
        push_item(rt, ctx, word(rt, self), frame.at(0x48u));
    }
    ctx.gpr[2] = 2u;
    ctx.gpr[3] = 2u;
}

// --- 0x0894AE54 -------------------------------------------------------------------------------------------------------------------

void func_0894AE54(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const bool reset = (ctx.gpr[5] & 0xFFu) != 0u;
    const u32 state = word(rt, self + 8u);
    const u32 shared = word(rt, self + 0xCu);
    const float f28 = call_guest(rt, ctx, 0x0895549Cu, {self}, {}).f0;
    ctx.fpr[0] = f28;
    if (reset) {
        put(rt, state + 0x6B8u, 0u);
        for (u32 wheel = 0u; wheel != 4u; ++wheel) {
            const u32 record = call(rt, ctx, 0x08955300u, {self, wheel});
            put(rt, record + 0x80u, 0u);
        }
        return;
    }
    const float kTwoPointTwo = bits(0x400E38E4u);
    const float kOne = bits(0x3F800000u);
    const float kHalfPi = bits(0x3FC90FDBu);
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    for (u32 wheel = 0u; wheel != 4u; ++wheel) {
        const u32 record = call(rt, ctx, 0x08955300u, {self, wheel});
        const u32 contact = call(rt, ctx, 0x089554A8u, {self, wheel});
        const float dir_y = float_at(rt, contact + 0x10u); // f9
        const float dir_x = float_at(rt, contact + 0xCu);  // f8
        const float f2 = -dir_y;
        const float f3 = -dir_x;
        const float f7 = float_at(rt, record + 0x7Cu);
        const float f1 = psp_mul(float_at(rt, record + 0x74u), f2) - float_at(rt, record + 0x44u);
        const float f0 = psp_mul(float_at(rt, record + 0x74u), f3) - float_at(rt, record + 0x48u);
        const float base_x = f1 + psp_mul(f2, f7);        // f23
        const float base_y = f0 + psp_mul(f3, f7);        // f24
        const float lateral = float_at(rt, record + 0x88u) + float_at(rt, record + 0x8Cu); // f22
        float slip = 0.0f;                                // f20
        const float grip = float_at(rt, record + 0x58u);
        if (grip != 0.0f) {
            const float angle = float_at(rt, record + 0x34u); // f21
            const float f2_value = float_at(rt, record + 0x30u);
            if (!(angle < bits(0xBFB1C71Du))) {
                const float delta = f2_value - angle; // f1
                if (!(bits(0x3FB1C71Du) < angle)) {
                    const float f4 = bits(0xBF0E38E4u);
                    const float f3_limit = bits(0x3F0E38E4u);
                    if (!(delta < f4) && !(f3_limit < delta)) {
                        float f12 = psp_mul(dir_y, float_at(rt, record + 0x2Cu)) + psp_mul(dir_x, f2_value);
                        if (!(bits(0x4031C71Du) < f12) && !(f12 < bits(0xC031C71Du))) {
                            if (f3_limit < f12) {
                                f12 = psp_mul((f12 - f3_limit) / kTwoPointTwo + kOne, kHalfPi);
                            } else if (f12 < f4) {
                                f12 = psp_mul((f12 + f3_limit) / kTwoPointTwo - kOne, kHalfPi);
                            } else {
                                f12 = psp_mul(f12, bits(0x4034F4ABu));
                            }
                            const float first = call_guest(rt, ctx, 0x08B49CC8u, {}, {f12}).f0;
                            const float second = call_guest(rt, ctx, 0x08B499B8u, {}, {psp_mul(angle, bits(0x3F90C3BCu))}).f0;
                            slip = psp_mul(psp_mul(first, second), float_at(rt, record + 0x58u));
                        }
                    }
                }
            }
        }
        // The call results are caller-saved: the original reloads the direction from the contact record.
        const float dir_y_after = float_at(rt, contact + 0x10u);
        const float dir_x_after = float_at(rt, contact + 0xCu);
        float force_x = base_x - psp_mul(slip, dir_y_after);
        force_x = force_x - psp_mul(lateral, dir_x_after);
        float force_y = base_y - psp_mul(slip, dir_x_after);
        force_y = force_y + psp_mul(lateral, dir_y_after);
        put_float(rt, record + 0x80u, force_x);
        sum_x += force_x;
        const float torque = psp_mul(force_y, float_at(rt, contact + 0x14u)) - psp_mul(force_x, float_at(rt, contact + 0x18u));
        sum_y += torque;
    }
    const u32 flags = rt.memory().load8(state + 1u);
    ctx.gpr[2] = flags < 0x7Du ? 1u : 0u;
    if (flags >= 0x7Du) {
        const float load = float_at(rt, state + 0x698u);
        if (0.0f < load) {
            float f0 = psp_mul(float_at(rt, state + 0x6C8u), bits(0x3E3851EBu));
            if (1.0f < f0) f0 = 1.0f;
            else if (f0 < bits(0xBF800000u)) f0 = bits(0xBF800000u);
            const float steer = float_at(rt, state + 0x6F8u);
            float f2 = psp_mul(steer, bits(0x3E22F983u));
            sum_x += psp_mul(psp_mul(f0, load), bits(0xC6992000u));
            if (1.0f < f2) f2 = 1.0f;
            else if (f2 < bits(0xBF800000u)) f2 = bits(0xBF800000u);
            sum_y += psp_mul(psp_mul(f2, load), bits(0xC765B000u));
        }
    }
    const float scale = float_at(rt, shared + 0x14Cu);
    float steer_value = float_at(rt, state + 0x6F8u);
    const float previous = float_at(rt, state + 0x568u);
    float second_state = float_at(rt, state + 0x56Cu);
    const float f2 = psp_mul(sum_y, scale);
    const float damping = psp_mul(second_state, bits(0xC1A00000u));
    const float step = psp_mul(f28, f2);
    float delta = psp_mul(f2 - previous, bits(0x43FA0000u));
    put_float(rt, state + 0x6E0u, f2);
    steer_value = steer_value + step;
    delta = delta + damping;
    put_float(rt, state + 0x6F8u, steer_value);
    delta = psp_mul(delta, bits(0x3C88AB7Cu));
    second_state = second_state + delta;
    const float f4 = psp_mul(second_state, bits(0x3C88AB7Cu));
    put_float(rt, state + 0x56Cu, second_state);
    put_float(rt, state + 0x568u, previous + f4);
    if (bits(0x40C90FDBu) < steer_value) {
        put_float(rt, state + 0x6F8u, bits(0x40C90FDBu));
    } else if (steer_value < bits(0xC0C90FDBu)) {
        put_float(rt, state + 0x6F8u, bits(0xC0C90FDBu));
    }
    ctx.fpr[0] = sum_x / float_at(rt, state + 0x5D8u);
    put_float(rt, state + 0x6B8u, ctx.fpr[0]);
}

} // namespace
} // namespace new_batch_main_130
} // namespace gt::game

#define NEW130(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_130::name, gt::ReturnKind::kind)
NEW130(0x08A24584u, func_08A24584, Word);
NEW130(0x0894AE54u, func_0894AE54, Word);
