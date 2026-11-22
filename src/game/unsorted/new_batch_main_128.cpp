// New batch 128 (main session): functions emitted by the recompiler that no src/ file defines or
// registers. Written by hand from `tools/disasm` as structured C; this is a draft batch, not yet
// verified by the differential gate.
//
// Audits: frame_audit 0 wrong; literal_check clean (0x1F of func_08AFDCD8 is the mask `ext ...,25,5`
// implies); sp_loads lists two real stack loads and both are modelled — func_08A4E198's
// `lw s3,0(sp)` (the length slot 0x08A4DF6C writes) and func_08AFDCD8's `lw s6,0x168(sp)` (the third
// stack argument); arg_reads clean.
//
// The helpers below cover the shapes this batch repeats: a guest call that returns v0 (the batch
// files' convention), one that also hands back the callee's v1/f0 (several bodies here store a
// caller-saved register after a call, which is the callee's value), the compiler's virtual call
// (`lw v1, 0(a0); lw v0, off(v1); jalr v0`), and the sret store-and-release tail.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_128 {
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
// A guest call that leaves v0/v1/f0 as the callee left them: a callee that never writes v0 hands the
// previous one on, and a body that stores v1 after the call stores the callee's v1.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments (f12-f15).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The compiler's virtual call: v1 = the vtable (object[0]), v0 = vtable[offset], a0 = `arguments`.
// v0/v1 stay the callee's afterwards; a callee that never writes v0 returns the target, as the
// original's `jalr v0` does.
GuestResult call_method(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                        std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}
// The sret tail the stringbuilder-shaped bodies share: when the destination is not the temporary,
// move the constructed value there (the delay slot of the release call reads it), then release the
// temporary; v0 is the release's result, as the asm leaves it.
void release_result(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 temporary) {
    if (destination != temporary) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, temporary)});
    call(rt, ctx, 0x08A5B9ECu, {temporary});
}

// 0x088CCE44 (a0 = state object, a1 = object whose +0x18 method is called, a2 = id, a3 = mode,
// t0 = argument for 0x088CB7F0/0x088CC988, t1 = object stepped through its +0x20 method). Copies the
// +0x2C/+0x30 slots, then walks the float window at +0x5E0..+0x5E8 one entry of a 40-byte-stride
// array up or down and queues the new entry through 0x088CC988; ends in a tail call of 0x08B3457C.
// v0 of every exit is what the asm leaves there: untouched when +0x63C is null, the call's result
// otherwise.
void func_088CCE44(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 id = ctx.gpr[6];
    const u32 mode = ctx.gpr[7];
    const u32 extra = ctx.gpr[8];
    const u32 stepper = ctx.gpr[9];
    GuestFrame frame(ctx, 0x30u);

    if (word(rt, self + 0x63Cu) == 0u) return;  // v0 is the caller's, as the original leaves it

    const float current = call_method(rt, ctx, word(rt, self + 0x63Cu), 0x14u,
                                      {word(rt, self + 0x63Cu)}).f0;
    const float half_step = psp_mul(current, 0.5f);  // the call's delay slot

    const u32 controller = word(rt, self + 0x63Cu);
    const GuestResult decided = call_method(rt, ctx, controller, 0x18u, {controller, mode});
    u32 step = mode;
    u32 changed = 0u;
    if (decided.v0 == 0u) {
        const u32 retry = word(rt, self + 0x63Cu);
        const GuestResult again = call_method(rt, ctx, retry, 0x18u, {retry, 6u});
        step = 6u;  // the beqz's delay slot
        if (again.v0 == 0u) return;
    }
    if (step != word(rt, self + 0x30u)) {
        put(rt, self + 0x30u, step);
        changed = 1u;
    }
    if (word(rt, self + 0x2Cu) != id) {
        put(rt, self + 0x2Cu, id);
        changed = 1u;
    }
    const GuestResult stepped = call_method(rt, ctx, stepper, 0x20u, {stepper});
    if (stepped.v0 == 0u) return;
    const u32 stepper_value = stepped.v0;

    if (changed != 0u || rt.memory().load8(self + 0x634u) != 0u) {
        call(rt, ctx, 0x08B344E0u, {self});
        call(rt, ctx, 0x088CCBCCu, {self, arg1, extra});
        rt.memory().store8(self + 0x634u, 0u);
        call(rt, ctx, 0x08B3457Cu, {self});
    }
    const u32 leaf_target = word(rt, word(rt, arg1) + 0x18u);
    ctx.gpr[2] = leaf_target;  // `jalr v0` leaves v0 = the target for a callee that never writes it
    const GuestResult found = call_keep(rt, ctx, leaf_target, {arg1, word(rt, self + 0x2Cu)});
    if (found.v0 == 0u) return;
    const u32 target = found.v0;
    const float window = call_method(rt, ctx, target, 0x50u, {target}).f0;

    const float high = float_at(rt, self + 0x5E8u);
    const float width = float_at(rt, self + 0x608u);
    const float reach = half_step + window;
    const float upper = high + width;
    const float lower = high - width;  // the bc1f's delay slot, always computed
    float value = window;
    if (reach < upper) {
        value = value + current;
    } else if ((half_step + lower) < value) {
        value = value - current;
    }

    if (upper <= value) {
        // One entry further along the 40-byte array at +0x5D8.
        call(rt, ctx, 0x08B344E0u, {self});
        const float delta = float_at(rt, self + 0x608u);
        const float e0 = float_at(rt, self + 0x5E0u) + delta;
        const float e1 = float_at(rt, self + 0x5E4u) + delta;
        const float e2 = float_at(rt, self + 0x5E8u) + delta;
        const u32 index = word(rt, self + 0x5D8u);
        const float sample = float_at(rt, self + index * 40u + 0x3Cu);
        put_float(rt, self + 0x5E0u, e0);
        put_float(rt, self + 0x5E4u, e1);
        put_float(rt, self + 0x5E8u, e2);
        put(rt, self + 0x5D8u, index + 1u);
        put(rt, self + 0x5DCu, word(rt, self + 0x5DCu) + 1u);
        call(rt, ctx, 0x088CBEB0u, {self});
        const GuestResult queued = call_keepf(
            rt, ctx, 0x088CC988u,
            {self, step, 0u, word(rt, self + 0x5D8u), word(rt, self + 0x5F0u), word(rt, self + 0x5F4u), extra},
            {float_at(rt, self + 0x5E0u), sample});
        put(rt, self + 0x5F0u, queued.v0);
        const u32 next = self + word(rt, self + 0x5DCu) * 40u;
        put(rt, self + 0x5F4u, queued.v1);
        put(rt, self + 0x5F8u, word(rt, next + 0x50u));
        put(rt, self + 0x5FCu, word(rt, next + 0x54u));
        rt.memory().store8(self + 0x635u, 1u);
        call(rt, ctx, 0x08B3457Cu, {self});
    } else if (value < lower) {
        // One entry back.
        call(rt, ctx, 0x08B344E0u, {self});
        const float delta = float_at(rt, self + 0x608u);
        const float e0 = float_at(rt, self + 0x5E0u) - delta;
        const float e1 = float_at(rt, self + 0x5E4u) - delta;
        const float e2 = float_at(rt, self + 0x5E8u) - delta;
        const u32 index = word(rt, self + 0x5DCu);
        const float sample = float_at(rt, self + index * 40u + 0x3Cu);
        put_float(rt, self + 0x5E0u, e0);
        put_float(rt, self + 0x5E4u, e1);
        put_float(rt, self + 0x5E8u, e2);
        put(rt, self + 0x5D8u, word(rt, self + 0x5D8u) - 1u);
        put(rt, self + 0x5DCu, index - 1u);
        call(rt, ctx, 0x088CBEB0u, {self});
        const u32 entry = self + word(rt, self + 0x5D8u) * 40u;
        const u32 t0 = word(rt, self + 0x5F8u);
        const u32 t1 = word(rt, self + 0x5FCu);
        put(rt, self + 0x5F0u, word(rt, entry + 0x50u));
        put(rt, self + 0x5F4u, word(rt, entry + 0x54u));
        const GuestResult queued = call_keepf(
            rt, ctx, 0x088CC988u,
            {self, step, 0u, word(rt, self + 0x5DCu), t0, t1, extra},
            {float_at(rt, self + 0x5E4u), sample});
        put(rt, self + 0x5F8u, queued.v0);
        put(rt, self + 0x5FCu, queued.v1);
        rt.memory().store8(self + 0x635u, 1u);
        call(rt, ctx, 0x08B3457Cu, {self});
    }

    call(rt, ctx, 0x08B344E0u, {self});
    call(rt, ctx, 0x088CB7F0u, {self, arg1, stepper_value, extra});
    tail_call(ctx, 0x08B3457Cu, {self});
}

// 0x089EE49C (a0 = object, a1 = object with a vtable at +0x0). Builds three stack objects through
// 0x089E4BB8, asks the a1 object for a context (0x08A06E94) and calls its +0x108 method, releases
// each stack object whose virtual +0x28 says it is done, then dispatches on the int at +0x22C into
// one of four value writers; the tail releases the three objects and leaves v0 as the last release's.
void func_089EE49C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x80u);

    ctx.gpr[2] = rt.memory().load8(self + 0xA68u);
    if (ctx.gpr[2] != 0u) return;

    const u32 temporary = frame.at(0x2Cu);
    const u32 scratch = frame.at(0x08u);

    const u32 method = word(rt, word(rt, object) + 0x108u);  // the first call's delay slot
    const u32 context_arg = call(rt, ctx, 0x08A06E94u, {self, 0u});
    ctx.gpr[2] = context_arg;  // v0 at the jalr is the lookup's result, not the target
    call_keep(rt, ctx, method, {object, context_arg});

    for (u32 slot = temporary; slot != frame.at(0x38u); slot += 4u) call(rt, ctx, 0x089E4BB8u, {slot});
    call(rt, ctx, 0x089E0570u, {frame.at(0x04u), self});
    release_result(rt, ctx, temporary, frame.at(0x04u));

    if (frame.at(0x30u) != self + 0x240u)
        call(rt, ctx, 0x08A5BAD8u, {frame.at(0x30u), word(rt, self + 0x240u)});
    if (frame.at(0x34u) != self + 0x244u)
        call(rt, ctx, 0x08A5BAD8u, {frame.at(0x34u), word(rt, self + 0x244u)});

    // Drop the three stack objects whose virtual +0x28 answers 0, releasing each one.
    u32 index = 0u;
    u32 slot = temporary;
    do {
        const u32 element = word(rt, slot);
        index += 1u;
        if (element != 0u) {
            const GuestResult alive = call_method(rt, ctx, element, 0x28u, {element});
            if (alive.v0 == 0u) {
                call(rt, ctx, 0x089E4C14u, {frame.at(0x00u), 0x08CF6ED8u});
                release_result(rt, ctx, slot, frame.at(0x00u));
            }
        }
        slot += 4u;
    } while (index != 3u);

    float scale = static_cast<float>(static_cast<s32>(word(rt, self + 0x264u)));
    if (rt.memory().load8(self + 0x262u) != 0u) scale = float_at(rt, self + 0x258u);
    if (word(rt, self + 0x230u) == 1u) scale = call_keep(rt, ctx, 0x089EDF44u, {self}).f0;
    call_method(rt, ctx, object, 0x110u, {object, (word(rt, self + 0x1A0u) >> 6) & 3u});

    const u32 point = frame.at(0x24u);
    switch (word(rt, self + 0x22Cu)) {
    case 0: {
        const float level = float_at(rt, self + 0x234u);
        const u32 count = static_cast<u32>(static_cast<s32>(scale));
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, scratch, 0u});
        const float first = float_at(rt, scratch);
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, 0u, scratch});
        const float second = float_at(rt, scratch);
        put_float(rt, point, first);
        put_float(rt, frame.at(0x28u), second);
        call_keepf(rt, ctx, 0x089AA1C0u, {object, self + 0x268u, count, point, temporary}, {level});
        break;
    }
    case 1: {
        const float level = float_at(rt, self + 0x234u);
        const u32 count = static_cast<u32>(static_cast<s32>(scale));
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, scratch, 0u});
        const float first = float_at(rt, scratch);
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, 0u, scratch});
        const float second = float_at(rt, scratch);
        put_float(rt, frame.at(0x1Cu), first);
        put_float(rt, frame.at(0x20u), second);
        call_keepf(rt, ctx, 0x089AA334u, {object, self + 0x268u, count, frame.at(0x1Cu), temporary}, {level});
        break;
    }
    case 2: {
        const float level = float_at(rt, self + 0x234u);
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, scratch, 0u});
        const float first = float_at(rt, scratch);
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, 0u, scratch});
        const float second = float_at(rt, scratch);
        put_float(rt, frame.at(0x14u), first);
        put_float(rt, frame.at(0x18u), second);
        call_keepf(rt, ctx, 0x089AC618u,
                   {object, self + 0x268u, frame.at(0x14u), temporary},
                   {scale, level, float_at(rt, self + 0x248u), float_at(rt, self + 0x24Cu),
                    float_at(rt, self + 0x250u), float_at(rt, self + 0x254u)});
        break;
    }
    case 3: {
        // No float argument here: this path sets only a2 (the truncated value), not f12.
        const u32 count = static_cast<u32>(static_cast<s32>(scale));
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, scratch, 0u});
        const float first = float_at(rt, scratch);
        call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, 0u, scratch});
        const float second = float_at(rt, scratch);
        put_float(rt, frame.at(0x0Cu), first);
        put_float(rt, frame.at(0x10u), second);
        call_keep(rt, ctx, 0x089A90B4u, {object, self + 0x268u, count, frame.at(0x0Cu), temporary});
        break;
    }
    default: break;
    }

    call_method(rt, ctx, object, 0x114u, {object});
    for (u32 release = frame.at(0x34u);; release -= 4u) {
        call(rt, ctx, 0x08A5B9ECu, {release});
        if (release == temporary) break;
    }
}

// One 44-byte (11-word) element: two words, eight floats at +8..+0x24, a word at +0x28. The copies
// below repeat `*dst = *src` field by field, as the original's inlined assignment does.
void copy_element(Runtime &rt, u32 dst, u32 src) {
    const u32 w0 = word(rt, src);
    const u32 w1 = word(rt, src + 4u);
    put(rt, dst, w0);
    put(rt, dst + 4u, w1);
    for (u32 offset = 8u; offset <= 0x24u; offset += 4u) put_float(rt, dst + offset, float_at(rt, src + offset));
    put(rt, dst + 0x28u, word(rt, src + 0x28u));
}

// 0x08C174D4 (a0 = array, a1 = hole index, a2 = length, a3 = the value to put back, t0 = comparator
// object). The libstdc++ heap-repair shape: walk the hole down to its larger child (the comparison
// function at 0x08A15D54 takes two element pointers), taking the last even node when the length ends
// on one, then copy the value into a stack temporary and hand everything to 0x08C16480
// (__push_heap), releasing the temporary's vector. v0 is the deallocation's result, or the byte count
// when nothing was allocated.
void func_08C174D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 hole_index = ctx.gpr[5];
    const u32 length = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    const u32 comparator = ctx.gpr[8];
    GuestFrame frame(ctx, 0xB0u);

    u32 hole = hole_index;
    u32 child = 2u * hole + 2u;
    while (static_cast<s32>(child) < static_cast<s32>(length)) {
        u32 candidate = child - 1u;
        if (call(rt, ctx, 0x08A15D54u, {comparator, first + child * 44u, first + candidate * 44u}) != 0u)
            candidate = child;
        copy_element(rt, first + hole * 44u, first + candidate * 44u);
        hole = candidate;
        child = 2u * hole + 2u;
    }
    if (child == length) {
        copy_element(rt, first + hole * 44u, first + (length - 1u) * 44u);
        hole = length - 1u;
    }

    // The value and the comparator, copied onto the frame (the value at +4, the comparator at +0x30
    // and its second half at +0x44).
    const float value_tail_1 = float_at(rt, value + 0x14u);
    const float value_tail_2 = float_at(rt, value + 0x20u);
    const float value_tail_3 = float_at(rt, value + 0x24u);
    const u32 value_word = word(rt, value + 0x28u);
    const u32 comparand_0 = word(rt, comparator);
    const std::uint8_t comparator_16 = rt.memory().load8(comparator + 0x10u);
    const float comparand_1 = float_at(rt, comparator + 4u);
    const float comparand_2 = float_at(rt, comparator + 8u);
    const float comparand_3 = float_at(rt, comparator + 0xCu);
    const float value_1 = float_at(rt, value + 0xCu);
    const float value_2 = float_at(rt, value + 0x10u);
    const float value_4 = float_at(rt, value + 0x1Cu);
    const float value_0 = float_at(rt, value + 8u);
    const float value_3 = float_at(rt, value + 0x18u);
    const u32 value_first = word(rt, value);
    const u32 value_second = word(rt, value + 4u);

    put_float(rt, frame.at(0x18u), value_tail_1);
    put_float(rt, frame.at(0x24u), value_tail_2);
    put_float(rt, frame.at(0x28u), value_tail_3);
    put(rt, frame.at(0x2Cu), value_word);
    put(rt, frame.at(0x30u), comparand_0);
    rt.memory().store8(frame.at(0x40u), comparator_16);
    put(rt, frame.at(0x04u), value_first);
    put(rt, frame.at(0x08u), value_second);
    put_float(rt, frame.at(0x0Cu), value_0);
    put_float(rt, frame.at(0x10u), value_1);
    put_float(rt, frame.at(0x14u), value_2);
    put_float(rt, frame.at(0x1Cu), value_3);
    put_float(rt, frame.at(0x20u), value_4);
    put_float(rt, frame.at(0x34u), comparand_1);
    put_float(rt, frame.at(0x38u), comparand_2);
    put_float(rt, frame.at(0x3Cu), comparand_3);
    rt.memory().store8(frame.at(0x41u), rt.memory().load8(comparator + 0x11u));
    put_float(rt, frame.at(0x44u), float_at(rt, comparator + 0x14u));
    put_float(rt, frame.at(0x48u), float_at(rt, comparator + 0x18u));
    put_float(rt, frame.at(0x4Cu), float_at(rt, comparator + 0x1Cu));
    put_float(rt, frame.at(0x50u), float_at(rt, comparator + 0x20u));
    put_float(rt, frame.at(0x54u), float_at(rt, comparator + 0x24u));
    put_float(rt, frame.at(0x58u), float_at(rt, comparator + 0x28u));
    put_float(rt, frame.at(0x5Cu), float_at(rt, comparator + 0x2Cu));
    put_float(rt, frame.at(0x60u), float_at(rt, comparator + 0x30u));
    put(rt, frame.at(0x68u), word(rt, comparator + 0x38u));
    put(rt, frame.at(0x64u), word(rt, comparator + 0x34u));
    put(rt, frame.at(0x6Cu), 0u);
    put(rt, frame.at(0x70u), 0u);
    put(rt, frame.at(0x74u), 0u);

    const u32 bytes = (word(rt, comparator + 0x40u) - word(rt, comparator + 0x3Cu)) & ~15u;
    const u32 allocated = call(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    put(rt, frame.at(0x74u), allocated + bytes);
    put(rt, frame.at(0x6Cu), allocated);
    put(rt, frame.at(0x70u), allocated);
    const u32 end = call(rt, ctx, 0x08C15F9Cu,
                         {word(rt, comparator + 0x3Cu), word(rt, comparator + 0x40u), allocated, frame.at(0x01u)});
    put(rt, frame.at(0x70u), end);
    put(rt, frame.at(0x78u), word(rt, comparator + 0x48u));

    call(rt, ctx, 0x08C16480u, {first, hole, hole_index, frame.at(0x04u), frame.at(0x30u)});
    call(rt, ctx, 0x08BF5B28u, {word(rt, frame.at(0x6Cu)), word(rt, frame.at(0x70u)), frame.at(0x00u)});
    const u32 block = word(rt, frame.at(0x6Cu));
    const u32 size = word(rt, frame.at(0x74u)) - block;
    if (block != 0u) {
        const s32 rounded = static_cast<s32>(size) >> 4;
        call(rt, ctx, 0x08A5BC54u, {block, static_cast<u32>(rounded) << 4, 4u});
    } else {
        ctx.gpr[2] = size;
    }
}

// 0x08B452EC (a0 = object, a1/a2 kept for the callees, a3 = descriptor object, t0 = an index compared
// with 24, t1 unused). Two keyed tables at +0x154 and +0x454, each with a byte of "used" flags at
// +0x400 and keys at +0x200; a match whose entry is set hands its address back, otherwise the routine
// allocates a 0x60-byte object through 0x08B2CB18, inserts it into a free slot of the +0x454 table
// (linking it into the neighbouring slots' chains) and finishes with 0x08B3F098 on the +0x8DC map.
void func_08B452EC(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 argument1 = ctx.gpr[5];
    const u32 descriptor = ctx.gpr[7];
    const u32 limit = ctx.gpr[8];
    GuestFrame frame(ctx, 0x50u);

    if (limit < 0x18u) {
        ctx.gpr[2] = 1u;  // the sltiu's result
        return;
    }
    const s32 lookup_key = static_cast<s32>(static_cast<std::int16_t>(word(rt, descriptor + 0xAu)));
    const u32 found = call(rt, ctx, 0x08B3EBD0u, {owner + 0x154u, static_cast<u32>(lookup_key)});
    if (found == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 object = word(rt, found);
    const u32 key = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(word(rt, descriptor + 8u))));
    if (object == 0u) {
        // The lookup found an empty entry: nothing more to do, v0 is the caller's a0.
        ctx.gpr[2] = owner;
        return;
    }

    // Table at +0x454: a hit whose entry is set is the answer.
    const u32 table = owner + 0x454u;
    const u32 count = word(rt, table + 0x480u);
    for (u32 index = 0u; index != count; ++index) {
        if (rt.memory().load8(table + index + 0x400u) == 0u) continue;
        if (word(rt, table + 0x200u + index * 4u) != key) continue;
        const u32 entry = table + index * 4u;
        if (word(rt, entry) != 0u) {
            ctx.gpr[2] = entry;
            return;
        }
        break;
    }

    // The range pointer the insertion object gets: null while the descriptor asks for it and the
    // caller's index is below the object's +0x18.
    u32 range = 0u;
    if (rt.memory().load8(descriptor + 0xEu) != 0u && !(limit < object + 0x18u)) range = descriptor + 0x18u;

    const u32 handle = owner + 0xCCu;
    const u32 created = call(rt, ctx, 0x08B2CB18u, {handle, 0x60u, 0x10u});
    if (created != 0u) {
        // The last four arguments of 0x08B3E45C go on the frame, as the original's stores at sp.
        put(rt, frame.at(0x00u), static_cast<u32>(rt.memory().load16(descriptor + 0xCu)));
        put(rt, frame.at(0x04u), rt.memory().load8(descriptor + 0xFu));
        put(rt, frame.at(0x08u), static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(
                                     word(rt, descriptor + 0xAu)))));
        put(rt, frame.at(0x0Cu), handle);
        call(rt, ctx, 0x08B3E45Cu,
             {created, key, 0u, range, object, word(rt, found + 4u),
              rt.memory().load8(found + 8u), rt.memory().load8(found + 9u)});
    }
    put(rt, created + 0x18u, word(rt, descriptor + 0x10u));
    put(rt, created + 0x1Cu, word(rt, descriptor + 0x14u));
    call(rt, ctx, 0x08B3E64Cu,
         {created, word(rt, found + 0xCu), word(rt, found + 0x10u), word(rt, found + 0x14u)});

    const u32 created_key = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(word(rt, created))));
    const u32 link = call(rt, ctx, 0x08B2CB18u, {handle, 0xCu, 0x10u});
    if (link != 0u) {
        put(rt, link, created);
        put(rt, link + 4u, 0u);
        put(rt, link + 8u, 0u);
    }

    // Table at +0x454: the same key, this time keeping whatever entry it points at.
    u32 free_index = 0xFFFFFFFFu;
    bool insert = true;
    for (u32 index = 0u; index != count; ++index) {
        if (rt.memory().load8(table + index + 0x400u) == 0u) continue;
        if (word(rt, table + 0x200u + index * 4u) != created_key) continue;
        if (word(rt, table + index * 4u) != 0u) insert = false;  // the entry is already there
        break;
    }
    if (insert) {
        // A free flag byte: the first zero in the +0x400 block.
        bool have_slot = false;
        if (count != 0u) {
            if (rt.memory().load8(table + 0x400u) == 0u) {
                free_index = 0u;
                have_slot = true;
            } else {
                for (u32 index = 1u; index != count; ++index) {
                    if (rt.memory().load8(table + index + 0x400u) == 0u) {
                        free_index = index;
                        have_slot = true;
                        break;
                    }
                }
            }
            if (have_slot) rt.memory().store8(table + free_index + 0x400u, 1u);
        }
        const u32 slot = table + free_index * 4u;
        const u32 slot_count = word(rt, table + 0x484u) + 1u;
        put(rt, slot + 0x200u, created_key);
        put(rt, table + 0x484u, slot_count);
        put(rt, slot, link);
        put(rt, link + 4u, 0u);

        if (static_cast<s32>(free_index) > 0) {
            // Link backwards through the occupied neighbours whose entry is set.
            for (u32 step = 0u; step != free_index; ++step) {
                const u32 index = free_index - 1u - step;
                if (rt.memory().load8(table + index + 0x400u) == 0u) continue;
                const u32 next = word(rt, table + index * 4u);
                if (next == 0u) continue;
                put(rt, next + 8u, link);
                put(rt, link + 4u, next);
                break;
            }
        }
        if (free_index + 1u < 0x80u) {
            put(rt, link + 8u, 0u);
            for (u32 index = free_index + 1u; index != 0x80u; ++index) {
                if (index >= count) continue;
                if (rt.memory().load8(table + index + 0x400u) == 0u) continue;
                const u32 next = word(rt, table + index * 4u);
                if (next == 0u) continue;
                put(rt, next + 4u, link);
                put(rt, link + 8u, next);
                break;
            }
        }
    }

    // 0x08B45604: the neighbour's selector decides whether 0x08B3ED00 runs.
    const s32 selector = static_cast<s32>(static_cast<std::int16_t>(word(rt, created + 0x28u)));
    if (selector >= 0 && word(rt, found + 0xCu) != 0u) {
        call(rt, ctx, 0x08B3ED00u,
             {found, static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(word(rt, descriptor + 0xCu)))),
              static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(word(rt, created))))});
    }
    const s32 offset = static_cast<s32>(static_cast<std::int16_t>(word(rt, created + 0x16u)));
    const u32 handle2 = owner + 0x8DCu;
    // a1 is not set on this path in the original (the caller-saved register holds whatever
    // 0x08B3ED00 or the earlier code left); the callee does not read it.
    const u32 handle3 = call(rt, ctx, 0x08B3F098u, {handle2, argument1,
                                                    static_cast<u32>(offset),
                                                    static_cast<u32>(offset >> 31)});
    if (handle3 == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (rt.memory().load8(handle3 + 5u) != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    rt.memory().store8(handle3 + 5u, 1u);
    ctx.gpr[2] = 1u;
}

// 0x08AFDCD8 (a0 = object, a1/a2/a3 = passed on, t0 = the vertex table, t1, t2 = the second body, t3,
// stack: +0 = a row index, +4 = the return slot, +8 = a pair bitmask). Walks the flag pairs at
// +0x1C0 of the requested row of `word(t2+0xD8)`, skips the pairs the mask already has, and for each
// remaining pair tests the two vertices with 0x08B032A8/0x08B036F8 and builds a contact through
// 0x08B01F0C. v0 on every exit is the word the caller keeps at stack +4 (the slot 0x08B01F0C was
// handed as its output).

// 0x08A4E198 (a0 = object, a1 = a char** in the caller's frame). A printf-style format walker: it
// reads a character, then dispatches on it and its successor ('B', '%', 'C', "[C" vs any other
// character), writing each piece through 0x08A4F3CC; a writer that reports failure ends the walk with
// -1. Otherwise it returns the accumulated length.

} // namespace
} // namespace new_batch_main_128
} // namespace gt::game

#define NEW128(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_128::name, gt::ReturnKind::kind)
NEW128(0x088CCE44u, func_088CCE44, Word);
NEW128(0x089EE49Cu, func_089EE49C, Word);
NEW128(0x08B452ECu, func_08B452EC, Word);
NEW128(0x08C174D4u, func_08C174D4, Word);
