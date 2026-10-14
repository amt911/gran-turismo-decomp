// Hot batch 59: the call-heavy functions batch 58 left (call probe as in batches 57/58). Four families make most of it:
// node initialisers that store three words and jump to a per-type routine (11 instances), wrappers that build a script
// value into a stack holder, assign it to the caller's holder and destroy the local one (11), float getters that call a
// routine of the 0x0896xxxx family with one of its out parameters on the stack (6), and what looks like range erases of a vector (4); the rest are
// destructors of small classes (set the vtable, destroy members, free) and argument shufflers that call or tail-call
// another function. Their classes aren't known yet, so they keep their addresses as names; each comment says what the
// code does. Written from the asm; the generated code was only a reading aid.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace gt::game {
namespace hot_batch_59 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kAssignHolder = 0x08A61584u;  // (dest *holder, source *holder)
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;  // gt::core::holder_destroy
constexpr std::uint32_t kNil = 0x08A76AA0u;            // (holder *out): out = the global nil (src/game/adhoc/array.cpp)
constexpr std::uint32_t kFree = 0x08A5BC54u;          // (object, size, alignment)

// The tail of the value-building wrappers: dest = holder (assign), then the local holder is destroyed; v0 is the destroy's.
void finish_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t destination, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kAssignHolder, {destination, holder});
    ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {holder});
}

} // namespace

// --- Node initialisers: this[0] = a, this[1] = *p, this[2] = p[1], this[3] = 0, then a per-type routine (tail call) ---

void node_init_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    static const std::unordered_map<std::uint32_t, std::uint32_t> kTargets = {
        {0x08BE8E0Cu, 0x08BE8DE8u}, {0x08BEF04Cu, 0x08BEF030u}, {0x08C0800Cu, 0x08C07FE8u}, {0x08C02424u, 0x08C02400u},
        {0x08C12044u, 0x08C12020u}, {0x08C09694u, 0x08C09670u}, {0x08BF5AC0u, 0x08BF5A9Cu}, {0x08C0645Cu, 0x08C06438u},
        {0x08C18EB0u, 0x08C18E94u}, {0x08C14738u, 0x08C14714u}, {0x08C0B44Cu, 0x08C0B428u},
    };
    const std::uint32_t self = ctx.gpr[4], pair = ctx.gpr[6];
    const std::uint32_t first = word(rt, pair), second = word(rt, pair + 4u);
    put(rt, self, ctx.gpr[5]);
    put(rt, self + 8u, second);
    put(rt, self + 4u, first);
    put(rt, self + 12u, 0u);
    tail_call(ctx, kTargets.at(ctx.pc), {});
}

// --- Wrappers that build a value into a local holder (sp + 0), assign it to a1's holder and destroy it ---

// func_08C207F4(this, dest): the value func_08A6D2D4 makes from this + 8.
void func_08C207F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A6D2D4u, {frame.at(0u), self + 8u});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C207B0(this, dest): func_08A85B1C of the word at this + 8.
void func_08C207B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A85B1Cu, {frame.at(0u), word(rt, self + 8u)});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C2038C(_, dest): the global nil.
void func_08C2038C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, kNil, {frame.at(0u)});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C20A90(this, dest): func_08A87EEC of the float at this + 8.
void func_08C20A90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {float_at(rt, self + 8u)});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C212FC(this, dest): func_08A97A6C of the byte at this + 8.
void func_08C212FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A97A6Cu, {frame.at(0u), rt.memory().load8(self + 8u)});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C210DC(_, dest): the value func_08A813F4 makes.
void func_08C210DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A813F4u, {frame.at(0u)});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C21214(this, dest): func_08A5DEE8 of this's words +8 and +12.
void func_08C21214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A5DEE8u, {frame.at(0u), destination, word(rt, self + 8u), word(rt, self + 12u)});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C2066C(this, dest): func_08A7858C(nil, this + 8, 1, func_08A6158C(dest)).
void func_08C2066C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t key = call_guest(rt, ctx, 0x08A6158Cu, {destination});
    (void)call_guest(rt, ctx, kNil, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A7858Cu, {frame.at(0u), self + 8u, 1u, key});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C2129C(_, dest): func_08A8A2DC(value, func_08A6158C(dest), func_08A6158C(dest)) (the first answer is the third argument).
void func_08C2129C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t first = call_guest(rt, ctx, 0x08A6158Cu, {destination});
    const std::uint32_t second = call_guest(rt, ctx, 0x08A6158Cu, {destination});
    (void)call_guest(rt, ctx, 0x08A8A2DCu, {frame.at(0u), second, first});
    finish_holder(rt, ctx, destination, frame.at(0u));
}
// func_08C204EC(this, other): func_08C2045C(func_08A61624(other), this + 8, nil), the value destroyed (no assign).
void func_08C204EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t key = call_guest(rt, ctx, 0x08A61624u, {other});
    (void)call_guest(rt, ctx, kNil, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08C2045Cu, {key, self + 8u, frame.at(0u)});
    ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {frame.at(0u)});
}
// func_08C1D7A4(a, _, c): func_08A535A4(a, value, c, -1, 0) with the value func_08A6972C makes, then destroyed.
void func_08C1D7A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A6972Cu, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A535A4u, {first, frame.at(0u), third, 0xFFFFFFFFu, 0u});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A69D68u, {frame.at(0u)});
}

// --- Float getters: func_0896A35C / func_08969B98 (they look like layout routines) fill the float at sp + 0 (the other out parameters are 0) ---

float out_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t function, std::uint32_t position) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t outs[4] = {position == 1u ? frame.at(0u) : 0u, position == 2u ? frame.at(0u) : 0u,
                                   position == 3u ? frame.at(0u) : 0u, position == 4u ? frame.at(0u) : 0u};
    (void)call_guest(rt, ctx, function, {self, outs[0], outs[1], outs[2], outs[3]});
    return float_at(rt, frame.at(0u));
}
void func_08BE92F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = out_float(rt, ctx, 0x0896A35Cu, 4u); }
void func_08BE9320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = out_float(rt, ctx, 0x0896A35Cu, 3u); }
void func_08BE9378(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = out_float(rt, ctx, 0x0896A35Cu, 1u); }
void func_08BE934C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = out_float(rt, ctx, 0x0896A35Cu, 2u); }
void func_08BE9280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = out_float(rt, ctx, 0x08969B98u, 1u); }
void func_08BE9254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = out_float(rt, ctx, 0x08969B98u, 2u); }

// --- What looks like range erases of a vector of words: (vec, first, last) -> first, a memmove of the tail then a per-type routine ---

void erase_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t after_move) {
    const std::uint32_t vec = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    const std::uint32_t bytes = (last - first) & ~3u;
    const std::uint32_t tail = word(rt, vec + 4u) - last;
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08B79BACu, {first, last, tail});
    (void)call_guest(rt, ctx, after_move, {tail + first, word(rt, vec + 4u), frame.at(0u)});
    put(rt, vec + 4u, word(rt, vec + 4u) - bytes);
    ctx.gpr[2] = first;
}
void func_08C3F598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_range(rt, ctx, 0x08C3F588u); }
void func_08C37A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_range(rt, ctx, 0x08C2BF60u); }
void func_08BF9054(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_range(rt, ctx, 0x08BF9044u); }
void func_08BF9B6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_range(rt, ctx, 0x08BF91D8u); }

// --- Destructors (vtable set, members destroyed, then freed or handed on) ---

// func_08B85BF0(this): vtable 0x08CB6B88, func_08A69D68(this + 20), func_08A780B0(this), then the free of 24 bytes (align 4).
void func_08B85BF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CB6B88u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08A69D68u, {self + 20u});
        (void)call_guest(rt, ctx, 0x08A780B0u, {self});
    }
    tail_call(ctx, kFree, {self, 24u, 4u});
}
// func_08BE74C8(this): vtable 0x08CC75A8, func_08965A3C(this), the free of 36 bytes.
void func_08BE74C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CC75A8u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08965A3Cu, {self});
    }
    tail_call(ctx, kFree, {self, 36u, 4u});
}
// func_08C1DB44(this): vtable 0x08CD1F48, func_08A5B9EC(this + 12), func_08A5BB5C(this), the free of 24 bytes.
void func_08C1DB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CD1F48u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kHolderDestroy, {self + 12u});
        (void)call_guest(rt, ctx, 0x08A5BB5Cu, {self});
    }
    tail_call(ctx, kFree, {self, 24u, 4u});
}
// func_08C298A4 / func_08C1EB60(this): vtable (0x08CD41C8 / 0x08CD22B8), the holder +8 destroyed, vtable 0x08CD1F30, the holder +4
// destroyed, then func_08B64654(this) (tail call).
void two_holder_destructor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, vtable);
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {self + 8u});
        put(rt, self, 0x08CD1F30u);
        ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {self + 4u});
    }
    tail_call(ctx, 0x08B64654u, {self});
}
void func_08C298A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_holder_destructor(rt, ctx, 0x08CD41C8u); }
void func_08C1EB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_holder_destructor(rt, ctx, 0x08CD22B8u); }
// func_08C0C284(this): vtable 0x08CCFC90, func_089F9B74(this + 48), func_089F9B74(this + 32), then func_089D6684(this) (tail call).
void func_08C0C284(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CCFC90u);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x089F9B74u, {self + 48u});
        (void)call_guest(rt, ctx, 0x089F9B74u, {self + 32u});
    }
    tail_call(ctx, 0x089D6684u, {self});
}

// --- Argument shufflers ----------------------------------------------------------------------------------------

// func_08B7DF7C(a, b, c) / func_08B7DC1C(a, b) / func_08B7E4CC(a, b, c): func_08B796C4() (it looks like the current thread record) is put in front and
// func_08B7DD9C / func_08B7CABC / func_08B7E314 is called with it (tail call).
void func_08B7DF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    std::uint32_t thread;
    { GuestFrame frame(ctx, 0x10u); thread = call_guest(rt, ctx, 0x08B796C4u, {}); }
    ctx.gpr[2] = thread;
    tail_call(ctx, 0x08B7DD9Cu, {thread, a, b, c});
}
void func_08B7DC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    std::uint32_t thread;
    { GuestFrame frame(ctx, 0x10u); thread = call_guest(rt, ctx, 0x08B796C4u, {}); }
    ctx.gpr[2] = thread;
    tail_call(ctx, 0x08B7CABCu, {thread, a, b});
}
void func_08B7E4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5], c = ctx.gpr[6];
    std::uint32_t thread;
    { GuestFrame frame(ctx, 0x10u); thread = call_guest(rt, ctx, 0x08B796C4u, {}); }
    ctx.gpr[2] = thread;
    tail_call(ctx, 0x08B7E314u, {thread, a, b, c});
}
// func_08B28CD4(l): func_08B34880(l), func_08B28C04(l), then func_08B34B1C(l) (tail call).
void func_08B28CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t l = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B34880u, {l});
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B28C04u, {l});
    }
    tail_call(ctx, 0x08B34B1Cu, {l});
}
// func_08B1B99C(o): func_08B2E224(o), then func_08B2E28C(o) (tail call).
void func_08B1B99C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, 0x08B2E224u, {o});
    }
    tail_call(ctx, 0x08B2E28Cu, {o});
}
// func_08AD95A0(): func_08AD9958(0), func_08AEB908 / func_08ADE1C8 / func_08AEB930 around the state's word +0x24, func_08AD9958(1).
void func_08AD95A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kState = 0x08D405D0u;
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08AD9958u, {0u});
    (void)call_guest(rt, ctx, 0x08AEB908u, {word(rt, kState + 36u)});
    (void)call_guest(rt, ctx, 0x08ADE1C8u, {});
    (void)call_guest(rt, ctx, 0x08AEB930u, {word(rt, kState + 36u)});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AD9958u, {1u});
}
// func_08C00FC0(o, p) -> o: func_08B88F44(o, p + 28).
void func_08C00FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B88F44u, {o, ctx.gpr[5] + 28u});
    ctx.gpr[2] = o;
}
// func_08C31A00(o) -> o: func_08AA9240(o).
void func_08C31A00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08AA9240u, {o});
    ctx.gpr[2] = o;
}
// func_08BF7C60(o) -> o: a value func_08A76ABC makes, func_08A76ABC(o, value), the value destroyed.
void func_08BF7C60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A76ABCu, {frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A76ABCu, {o, frame.at(0u)});
    (void)call_guest(rt, ctx, kHolderDestroy, {frame.at(0u)});
    ctx.gpr[2] = o;
}
// func_08BF7C30(o, x) -> o: func_08A76AAC(o, &x).
void func_08BF7C30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), ctx.gpr[5]);
    (void)call_guest(rt, ctx, 0x08A76AACu, {o, frame.at(0u)});
    ctx.gpr[2] = o;
}
// func_08B8C564(o, flag) -> o: func_08AA20B0(o, flag & 0xFF).
void func_08B8C564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08AA20B0u, {o, ctx.gpr[5] & 0xFFu});
    ctx.gpr[2] = o;
}
// func_08C31AA8(_, a, c): func_08C31A80(a, func_08BE9068(c)) (tail call).
void func_08C31AA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[5];
    std::uint32_t converted;
    { GuestFrame frame(ctx, 0x10u); converted = call_guest(rt, ctx, 0x08BE9068u, {ctx.gpr[6]}); }
    ctx.gpr[2] = converted;
    tail_call(ctx, 0x08C31A80u, {a, converted});
}
// func_08B887E4(o) -> whether the virtual +0x114 answers nonzero.
void func_08B887E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, o) + 276u), {o}) != 0u ? 1u : 0u;
}
// func_08B851D0 / func_08B87514(o, p) -> o: the virtual +0x9C of p, called with (o, p).
void virtual_of_second(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4], p = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, word(rt, word(rt, p) + 156u), {o, p});
    ctx.gpr[2] = o;
}
// func_088A8E08(o): the virtual +0x64 of the object at o + 8.
void func_088A8E08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t inner = word(rt, ctx.gpr[4] + 8u);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, word(rt, word(rt, inner) + 100u), {inner});
}
// func_08B34DC0() -> whether the import 0x08C7AC58() answers nonzero.
void func_08B34DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7AC58u, {}) != 0u ? 1u : 0u;
}
// func_0880B750(o, p) -> 1: func_08869220(o + 48, *p).
void func_0880B750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08869220u, {ctx.gpr[4] + 48u, word(rt, ctx.gpr[5])});
    ctx.gpr[2] = 1u;
}
// func_0880BB38(o, p, q) -> r: under the lock o (gt::libc::page_heap::heap_lock / heap_unlock), r = func_088693CC(o + 48, *p, *q).
void func_0880BB38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4], p = ctx.gpr[5], q = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B344E0u, {o});
    const std::uint32_t result = call_guest(rt, ctx, 0x088693CCu, {o + 48u, word(rt, p), word(rt, q)});
    (void)call_guest(rt, ctx, 0x08B3457Cu, {o});
    ctx.gpr[2] = result;
}
// func_08C2486C(this, a, b, c): this[0] = func_08B85E00(a, b, c); func_08C00AD4(this, a, b, c): the same with b + a.
void func_08C2486C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B85E00u, {ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, ctx.gpr[4], ctx.gpr[2]);
}
void func_08C00AD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B85E00u, {ctx.gpr[5], ctx.gpr[6] + ctx.gpr[5], ctx.gpr[7]});
    put(rt, ctx.gpr[4], ctx.gpr[2]);
}
// func_088A05D4(o, out): out = func_08B4FC34(o's words +8 and +12, the float +20) as four floats.
void func_088A05D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4], out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08B4FC34u, {frame.at(0u), word(rt, o + 8u), word(rt, o + 12u)}, {float_at(rt, o + 20u)});
    put(rt, out + 4u, word(rt, frame.at(4u)));
    put(rt, out, word(rt, frame.at(0u)));
    put(rt, out + 12u, word(rt, frame.at(12u)));
    put(rt, out + 8u, word(rt, frame.at(8u)));
}
// func_08AF4604(o, p) -> o: func_08B4E360(floats p[1] * 0.0174533, p[4], p[2], p[3]).
void func_08AF4604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4], p = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const float radians = psp_mul(float_at(rt, p + 4u), std::bit_cast<float>(0x3C8EFA35u));
    (void)call_guest(rt, ctx, 0x08B4E360u, {}, {radians, float_at(rt, p + 16u), float_at(rt, p + 8u), float_at(rt, p + 12u)});
    ctx.gpr[2] = o;
}
// func_0886EB08(o, x): func_0887C5A4(o + 100) with x, then func_0886F86C(the object at o + 0x70, + 100) with x (tail call).
void func_0886EB08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    const float x = ctx.fpr[12];
    { GuestFrame frame(ctx, 0x20u); ctx.gpr[2] = call_guest(rt, ctx, 0x0887C5A4u, {o + 100u}, {x}).v0; }
    tail_call(ctx, 0x0886F86Cu, {word(rt, o + 112u) + 100u});
}
// func_0886DEB8(o, x): func_0887C5A4(o + 100) and func_0886F86C(inner + 20) with x, then func_08873E94(inner + 40) with x (tail call).
void func_0886DEB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    const float x = ctx.fpr[12];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, 0x0887C5A4u, {o + 100u}, {x});
        ctx.gpr[2] = call_guest(rt, ctx, 0x0886F86Cu, {word(rt, o + 112u) + 20u}, {x}).v0;  // v0 holds this call's result at the tail call
    }
    tail_call(ctx, 0x08873E94u, {word(rt, o + 112u) + 40u});
}
// func_0887EDF4(o, x): func_0887C5A4(o + 100), func_088824B8(inner) with x, then func_0886F86C(inner + 20) with x (tail call), inner = the word +116.
void func_0887EDF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t o = ctx.gpr[4];
    const float x = ctx.fpr[12];
    {
        GuestFrame frame(ctx, 0x20u);
        (void)call_guest(rt, ctx, 0x0887C5A4u, {o + 100u}, {x});
        ctx.gpr[2] = call_guest(rt, ctx, 0x088824B8u, {word(rt, o + 116u)}, {x}).v0;  // v0 holds this call's result at the tail call
    }
    tail_call(ctx, 0x0886F86Cu, {word(rt, o + 116u) + 20u});
}

} // namespace hot_batch_59
} // namespace gt::game

#define HOT59(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_59::name, gt::ReturnKind::kind)
HOT59(0x08BE8E0Cu, node_init_then, Word);
HOT59(0x08BEF04Cu, node_init_then, Word);
HOT59(0x08C0800Cu, node_init_then, Word);
HOT59(0x08C02424u, node_init_then, Word);
HOT59(0x08C12044u, node_init_then, Word);
HOT59(0x08C09694u, node_init_then, Word);
HOT59(0x08BF5AC0u, node_init_then, Word);
HOT59(0x08C0645Cu, node_init_then, Word);
HOT59(0x08C18EB0u, node_init_then, Word);
HOT59(0x08C14738u, node_init_then, Word);
HOT59(0x08C0B44Cu, node_init_then, Word);
HOT59(0x08C207F4u, func_08C207F4, Word);
HOT59(0x08C207B0u, func_08C207B0, Word);
HOT59(0x08C2038Cu, func_08C2038C, Word);
HOT59(0x08C20A90u, func_08C20A90, Word);
HOT59(0x08C212FCu, func_08C212FC, Word);
HOT59(0x08C210DCu, func_08C210DC, Word);
HOT59(0x08C21214u, func_08C21214, Word);
HOT59(0x08C2066Cu, func_08C2066C, Word);
HOT59(0x08C2129Cu, func_08C2129C, Word);
HOT59(0x08C204ECu, func_08C204EC, Word);
HOT59(0x08C1D7A4u, func_08C1D7A4, Word);
HOT59(0x08BE92F4u, func_08BE92F4, Float);
HOT59(0x08BE9320u, func_08BE9320, Float);
HOT59(0x08BE9378u, func_08BE9378, Float);
HOT59(0x08BE934Cu, func_08BE934C, Float);
HOT59(0x08BE9280u, func_08BE9280, Float);
HOT59(0x08BE9254u, func_08BE9254, Float);
HOT59(0x08C3F598u, func_08C3F598, Word);
HOT59(0x08C37A54u, func_08C37A54, Word);
HOT59(0x08BF9054u, func_08BF9054, Word);
HOT59(0x08BF9B6Cu, func_08BF9B6C, Word);
HOT59(0x08B85BF0u, func_08B85BF0, Word);
HOT59(0x08BE74C8u, func_08BE74C8, Word);
HOT59(0x08C1DB44u, func_08C1DB44, Word);
HOT59(0x08C298A4u, func_08C298A4, Word);
HOT59(0x08C1EB60u, func_08C1EB60, Word);
HOT59(0x08C0C284u, func_08C0C284, Word);
HOT59(0x08B7DF7Cu, func_08B7DF7C, Word);
HOT59(0x08B7DC1Cu, func_08B7DC1C, DoubleWord);
HOT59(0x08B7E4CCu, func_08B7E4CC, Word);
HOT59(0x08C00FC0u, func_08C00FC0, Word);
HOT59(0x08C31A00u, func_08C31A00, Word);
HOT59(0x08BF7C60u, func_08BF7C60, Word);
HOT59(0x08BF7C30u, func_08BF7C30, Word);
HOT59(0x08B8C564u, func_08B8C564, Word);
HOT59(0x08C31AA8u, func_08C31AA8, Word);
HOT59(0x08B887E4u, func_08B887E4, Word);
HOT59(0x088A8E08u, func_088A8E08, Word);
HOT59(0x0880B750u, func_0880B750, Word);
HOT59(0x0880BB38u, func_0880BB38, Word);
HOT59(0x08C2486Cu, func_08C2486C, Word);
HOT59(0x08C00AD4u, func_08C00AD4, Word);
HOT59(0x088A05D4u, func_088A05D4, Void);
HOT59(0x08AF4604u, func_08AF4604, Word);
HOT59(0x0886EB08u, func_0886EB08, Word);
HOT59(0x0886DEB8u, func_0886DEB8, Word);
HOT59(0x0887EDF4u, func_0887EDF4, Word);
HOT59(0x08B851D0u, virtual_of_second, Word);
HOT59(0x08B87514u, virtual_of_second, Word);
// Written but not registered: compared 0 in every fast run (no route calls func_08AD95A0, func_08B28CD4, func_08B34DC0 or
// func_08B1B99C from outside their generated unit), so nothing could verify them.
// HOT59(0x08AD95A0u, func_08AD95A0, Word);
// HOT59(0x08B28CD4u, func_08B28CD4, Word);
// HOT59(0x08B34DC0u, func_08B34DC0, Word);
// HOT59(0x08B1B99Cu, func_08B1B99C, Word);
#undef HOT59
