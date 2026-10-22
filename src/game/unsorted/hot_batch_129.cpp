// Hot batch 129: functions of 500+ instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <optional>

namespace gt::game {
namespace hot_batch_129 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline std::int32_t allegrex_div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}

} // namespace


// func_08A62DC8(vm, holder, self_value, argc, argv): the script VM calls the function in `holder` (its first word).
// A 0x24-byte call frame (vtable D_08CD36C0, func_08A5BC20) keeps the VM's state: +0x30 code, +0x34 function, +0x38,
// +0x3C locals, +0x48 stack mark, +0x18, and two empty strings. The VM then takes the function (+0x30 = its code
// object +0x14 when that has a +4, else 0; +0x34 = the function, func_08A5BA28), func_08A61DD4(vm, 0), new locals
// func_08A72818(code+0x18, captures, code+0x14, code+0x10) at +0x3C, depth +0x40 + 1, stack mark +0x48 and an empty
// result at +0x4C (func_08A76AA0). Each parameter (12-byte entries at function+0x18: +4 the local's index, +8 the
// default) gets its argument (func_08A84910(value, parameter, &argument, &scratch), func_08A729A4(locals, index,
// value)), or its default when the call is short; with the code's varargs flag (+0x39) the last one gets an array of
// the remaining arguments (func_08A98C1C / func_08A98A70). `self_value` goes to the local code+0xC and each capture
// (function+0x24) its value; +0x14 = the code's entry (+0x28). The frame is pushed on the vector vm+0x24
// (func_08C22ED8 when full) and func_08A615D4(vm, 3) runs it. Answers the frame's index.
void func_08A62DC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vm = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t self_value = ctx.gpr[6];
    const std::int32_t argc = signed_of(ctx.gpr[7]);
    const std::uint32_t argv = ctx.gpr[8];
    GuestFrame frame(ctx, 0xB0u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto checked = [&](std::uint32_t object, std::uint32_t message) {
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return object;
    };
    constexpr std::uint32_t kFunctionMessage = 0x08CB02F0u;
    constexpr std::uint32_t kCodeMessage = 0x08CB02A4u;
    const auto function = [&] { return checked(word(rt, holder), kFunctionMessage); };
    const auto code_of = [&](std::uint32_t object) { return checked(word(rt, object + 0x30u), kCodeMessage); };
    const std::uint32_t saved = call_set(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    put(rt, sp(0x5Cu), saved);
    put(rt, saved, 0x08CD36C0u);
    put(rt, saved + 4u, word(rt, vm + 0x30u));
    put(rt, saved + 8u, word(rt, vm + 0x34u));
    put(rt, saved + 0xCu, word(rt, vm + 0x38u));
    put(rt, saved + 0x10u, word(rt, vm + 0x3Cu));
    put(rt, saved + 0x1Cu, 0x08D74FDCu);
    put(rt, saved + 0x14u, word(rt, vm + 0x48u));
    put(rt, saved + 0x18u, 0x08D74FDCu);
    put(rt, saved + 0x20u, word(rt, vm + 0x18u));
    {
        const std::uint32_t entry = word(rt, function() + 0x14u);
        put(rt, vm + 0x30u, entry != 0u && word(rt, entry + 4u) != 0u ? entry : 0u);
    }
    {
        const std::uint32_t taken = function();
        put(rt, vm + 0x34u, taken);
        (void)call_set(rt, ctx, 0x08A5BA28u, {taken});
    }
    (void)call_set(rt, ctx, 0x08A61DD4u, {vm, 0u});
    const std::uint32_t captures = function() + 0x24u;  // a vector of 12-byte entries
    put(rt, sp(0x58u), captures);
    {
        const std::uint32_t code_a = code_of(word(rt, vm + 0x34u));
        const std::uint32_t count = ((word(rt, captures + 4u) - word(rt, captures)) >> 2) * 0xAAAAAAABu;
        const std::uint32_t code_b = code_of(word(rt, vm + 0x34u));
        const std::uint32_t code_c = code_of(word(rt, vm + 0x34u));
        put(rt, vm + 0x3Cu, call_set(rt, ctx, 0x08A72818u, {word(rt, code_a + 0x18u), count, word(rt, code_b + 0x14u), word(rt, code_c + 0x10u)}));
    }
    put(rt, vm + 0x40u, word(rt, vm + 0x40u) + 1u);
    const std::uint32_t stack = vm + 0x24u;
    put(rt, sp(0x60u), stack);
    put(rt, vm + 0x48u, ((word(rt, stack + 4u) - word(rt, vm + 0x24u)) >> 2) + 1u);
    (void)call_set(rt, ctx, 0x08A76AA0u, {sp(0x44u)});
    if (vm + 0x4Cu != sp(0x44u)) (void)call_set(rt, ctx, kAssign, {vm + 0x4Cu, word(rt, sp(0x44u))});
    (void)call_set(rt, ctx, kDestroy, {sp(0x44u)});
    // local `index` of the locals = func_08A84910(parameter, source, scratch): the value built in `value`
    const auto bind = [&](std::uint32_t parameter, std::uint32_t source, std::uint32_t value, std::uint32_t scratch) {
        const std::uint32_t locals = word(rt, vm + 0x3Cu);
        const std::uint32_t index = word(rt, parameter + 4u);
        (void)call_set(rt, ctx, 0x08A76AA0u, {scratch});
        (void)call_set(rt, ctx, 0x08A84910u, {value, parameter, source, scratch});
        (void)call_set(rt, ctx, 0x08A729A4u, {locals, index, value});
    };
    const auto release = [&](std::initializer_list<std::uint32_t> slots) {
        for (const std::uint32_t slot : slots) (void)call_set(rt, ctx, kDestroy, {slot});
    };
    const std::uint32_t parameters = word(rt, vm + 0x34u) + 0x18u;
    put(rt, sp(0x54u), parameters);
    if (word(rt, parameters) != word(rt, parameters + 4u)) {
        const std::int32_t count = signed_of(((word(rt, parameters + 4u) - word(rt, parameters)) >> 2) * 0xAAAAAAABu);
        const std::int32_t last = count - 1;
        put(rt, sp(0x50u), static_cast<std::uint32_t>(last));
        const auto parameter = [&](std::int32_t i) { return word(rt, parameters) + static_cast<std::uint32_t>(i) * 12u; };
        const auto varargs = [&] {
            (void)function();
            return byte_u(rt, code_of(word(rt, holder)) + 0x39u) != 0u;
        };
        if (argc >= count) {
            for (std::int32_t i = 0; i < last; ++i) {
                bind(parameter(i), argv + static_cast<std::uint32_t>(i) * 4u, sp(0x40u), sp(0x3Cu));
                release({sp(0x40u), sp(0x3Cu)});
            }
            if (varargs()) {
                const std::uint32_t extra = static_cast<std::uint32_t>(argc - last);
                (void)call_set(rt, ctx, 0x08A98C1Cu, {sp(0x48u), extra});
                for (std::uint32_t k = 0u; signed_of(extra) > 0 && k != extra; ++k) {
                    const std::uint32_t slot = word(rt, word(rt, sp(0x48u)) + 0x14u) + k * 4u;
                    const std::uint32_t source = argv + static_cast<std::uint32_t>(last) * 4u + k * 4u;
                    if (source != slot) (void)call_set(rt, ctx, kAssign, {slot, word(rt, source)});
                }
                bind(parameter(last), sp(0x48u), sp(0x30u), sp(0x2Cu));
                release({sp(0x30u), sp(0x2Cu), sp(0x48u)});
            } else {
                bind(parameter(last), argv + static_cast<std::uint32_t>(last) * 4u, sp(0x38u), sp(0x34u));
                release({sp(0x38u), sp(0x34u)});
            }
        } else {
            for (std::int32_t i = 0; i < argc; ++i) {
                bind(parameter(i), argv + static_cast<std::uint32_t>(i) * 4u, sp(0x28u), sp(0x24u));
                release({sp(0x28u), sp(0x24u)});
            }
            for (std::int32_t i = argc; i < last; ++i) {
                bind(parameter(i), parameter(i) + 8u, sp(0x20u), sp(0x1Cu));
                release({sp(0x20u), sp(0x1Cu)});
            }
            if (varargs()) {
                (void)call_set(rt, ctx, 0x08A98A70u, {sp(0xCu)});
                bind(parameter(last), sp(0xCu), sp(0x10u), sp(8u));
                release({sp(0x10u), sp(8u), sp(0xCu)});
            } else {
                bind(parameter(last), parameter(last) + 8u, sp(0x18u), sp(0x14u));
                release({sp(0x18u), sp(0x14u)});
            }
        }
    }
    {
        const std::uint32_t locals = word(rt, vm + 0x3Cu);
        (void)call_set(rt, ctx, 0x08A729A4u, {locals, word(rt, code_of(word(rt, vm + 0x34u)) + 0xCu), self_value});
    }
    if (word(rt, captures) != word(rt, captures + 4u)) {
        const std::int32_t count = signed_of(((word(rt, captures + 4u) - word(rt, captures)) >> 2) * 0xAAAAAAABu);
        for (std::int32_t k = 0; k < count; ++k) {
            const std::uint32_t capture = word(rt, captures) + static_cast<std::uint32_t>(k) * 12u;
            bind(capture, capture + 8u, sp(4u), sp(0u));
            release({sp(4u), sp(0u)});
        }
    }
    put(rt, vm + 0x14u, word(rt, code_of(word(rt, vm + 0x34u)) + 0x28u));
    put(rt, sp(0x4Cu), saved);
    const std::uint32_t end = word(rt, stack + 4u);
    const std::uint32_t index = static_cast<std::uint32_t>(signed_of(end - word(rt, stack)) >> 2);
    if (end == word(rt, stack + 8u)) {
        (void)call_set(rt, ctx, 0x08C22ED8u, {stack, end, sp(0x4Cu)});
    } else {
        if (end != 0u) put(rt, end, saved);
        put(rt, stack + 4u, word(rt, stack + 4u) + 4u);
    }
    (void)call_set(rt, ctx, 0x08A615D4u, {vm, 3u});
    ctx.gpr[2] = index;
}

} // namespace hot_batch_129
} // namespace gt::game

#define HOT129(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_129::name, gt::ReturnKind::kind)
HOT129(0x08A62DC8u, func_08A62DC8, Word);
