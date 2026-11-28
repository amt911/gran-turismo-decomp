// Main-session batch 1047 (functions earlier drafts left out or mismatched). Structured C from tools.disasm.
// 0x088AFBA4 is the script-thread's "run one compiled instruction" step: it asks the two handler objects at +0x38
// for their program/stack, builds the two smart-pointer slots the virtual machine consumes and then reports the
// machine's answer (1 = done, 2 = blocked, 3 = a call failed, 4 = the program ran out).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1047 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A direct call: v0, v1 and f0 afterwards are the callee's, as they are after the jal.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// `lw v1, 0(object); lw v0, offset(v1); jalr v0`: v1 holds the vtable word while the callee runs.
u32 vcall(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 vtable = word(rt, object);
    const u32 target = word(rt, vtable + offset);
    ctx.gpr[3] = vtable;
    return call_indirect(rt, ctx, target, arguments);
}

// The ll/sc pairs are a plain single-threaded read-modify-write here: +4 is the strong count and +8 the weak one.
void inc_count(Runtime &rt, u32 counter) { put(rt, counter, word(rt, counter) + 1u); }
u32 dec_count(Runtime &rt, u32 counter) {
    const u32 old = word(rt, counter);
    put(rt, counter, old - 1u);
    return old;
}

// One release of the referenced object at `object`: the strong count dropping to zero runs the destructor at vtable
// +8, the weak one dropping to zero the deallocation at vtable +0xC. Answers the v1 the caller's test then reads —
// the counter address the sequence last touched, or, when the object was freed, what the last call left in v1.
u32 release_reference(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return 4u;
    if (dec_count(rt, object + 4u) != 1u) return object + 4u;
    vcall(rt, ctx, object, 0x8u, {object});
    if (dec_count(rt, object + 8u) != 1u) return object + 8u;
    vcall(rt, ctx, object, 0xCu, {object});
    return ctx.gpr[3];
}

// 0x088AFBA4 (a0 = the thread object).
void func_088AFBA4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);

    // 0x088AFBD4: the word at +0xA8 of the object at +8 being set means "no more instructions": answer 2.
    const u32 state = word(rt, self + 8u);
    const u32 blocked = word(rt, state + 0xA8u);
    if (blocked != 0u) {
        ctx.gpr[3] = blocked;
        ctx.gpr[2] = 2u;
        return;
    }

    // 0x088AFBE0: tell the first handler the word at +0xB8 of that state, then ask it whether it has work.
    const u32 handler = word(rt, self + 0x38u);
    vcall(rt, ctx, handler, 0x14u, {handler, word(rt, state + 0xB8u)});
    const u32 iface0 = word(rt, self + 0x38u);
    const u32 pending = vcall(rt, ctx, iface0, 0x18u, {iface0});
    if (pending != 0u) {
        // 0x088AFC14: two numbers the machine wants, and 0x08AC03E0 validating the pair; a zero answer is 3.
        const u32 iface1 = word(rt, self + 0x38u);
        const u32 first = vcall(rt, ctx, iface1, 0x1Cu, {iface1});
        const u32 iface2 = word(rt, self + 0x38u);
        const u32 count = vcall(rt, ctx, iface2, 0x18u, {iface2});
        if (call(rt, ctx, 0x08AC03E0u, {self, first, count, 0u}) == 0u) {
            ctx.gpr[2] = 3u;
            return;
        }
    }

    // 0x088AFC5C: the three buffers of the machine, each asked for afresh after the previous one ran.
    const u32 iface3 = word(rt, self + 0x38u);
    const u32 program = vcall(rt, ctx, iface3, 0x24u, {iface3});
    const u32 iface4 = word(rt, self + 0x38u);
    put(rt, self + 0x40u, program);
    const u32 stack = vcall(rt, ctx, iface4, 0x28u, {iface4});
    put(rt, self + 0x44u, stack);
    const u32 current = vcall(rt, ctx, iface4, 0x2Cu, {iface4});
    put(rt, self + 0x5Cu, current);
    if (current == 0u) {
        // 0x088AFCA4: no instruction to resume at — build the two {value, reference} slots at +0x1C / +0x24 of the
        // holder from the program's +0x100 and the stack's +0x80, then let the temporary die.
        const u32 program_base = word(rt, self + 0x40u);
        const u32 stack_base = word(rt, self + 0x44u);
        put(rt, frame.at(0x10u), program_base == 0u ? 0u : program_base + 0x100u);
        put(rt, frame.at(0x14u), stack_base == 0u ? 0u : stack_base + 0x80u);
        call(rt, ctx, 0x08963418u, {frame.at(0x18u), frame.at(0x10u)});
        const u32 holder = word(rt, self + 0x48u);
        put(rt, holder + 0x1Cu, word(rt, frame.at(0x18u)));
        call(rt, ctx, 0x08BAEF98u, {holder + 0x20u, frame.at(0x1Cu)});
        put(rt, holder + 0x24u, word(rt, frame.at(0x20u)));
        call(rt, ctx, 0x08BAEF98u, {holder + 0x28u, frame.at(0x24u)});
        call(rt, ctx, 0x08B8ED98u, {frame.at(0x18u)});
        put(rt, self + 0x5Cu, word(rt, self + 0x48u) + 0x1Cu);
    }

    // 0x088AFD20: resolve the instruction record, copy its 0x80-byte payload under the record's own lock and wrap
    // the copy together with the record's owner into a new node.
    const u32 record = word(rt, self + 0x48u);
    const u32 slot = record + 8u;
    const u32 resolved = call(rt, ctx, 0x089624ECu, {word(rt, self + 0x5Cu)});
    const u32 payload = word(rt, resolved);
    u32 parent = resolved;
    const u32 copy_of = word(rt, self + 0x40u);
    if (payload == 0u) {
        // 0x088AFF44: an empty record stores a null slot and skips the copy.
        put(rt, frame.at(0x08u), 0u);
        put(rt, frame.at(0x0Cu), 0u);
        put(rt, slot, 0u);
        call(rt, ctx, 0x08BAEF98u, {slot + 4u, frame.at(0x0Cu)});
    } else {
        const u32 mutex = payload + 0x6Cu;
        call(rt, ctx, 0x08B34888u, {mutex});
        const u32 copied = call(rt, ctx, 0x08B2CB18u, {payload + 8u, copy_of, 0x80u});
        call(rt, ctx, 0x08B34B24u, {mutex});
        const u32 owner = word(rt, parent + 4u);
        parent = word(rt, parent);
        if (owner != 0u) {
            inc_count(rt, owner + 4u);
            inc_count(rt, owner + 4u);
            inc_count(rt, owner + 4u);
        }
        const u32 node = call(rt, ctx, 0x08B62344u, {0x18u});
        put(rt, node + 0x00u, 0x08CC2F90u);
        put(rt, node + 0x04u, 1u);
        put(rt, node + 0x08u, 1u);
        put(rt, node + 0x0Cu, copied);
        put(rt, node + 0x10u, parent);
        put(rt, node + 0x14u, owner);
        if (owner != 0u) inc_count(rt, owner + 4u);
        release_reference(rt, ctx, owner);
        if (node == 0u) {
            // 0x088AFE7C: the allocation failed — hold the record's lock, undo the copy and drop the cache.
            const u32 pin = parent + 0x6Cu;
            call(rt, ctx, 0x08B34888u, {pin});
            call(rt, ctx, 0x08B2CE7Cu, {parent + 8u, copied});
            const u32 cache = call(rt, ctx, 0x08B2D1A8u, {parent + 8u});
            if (cache == word(rt, parent + 0x68u)) call(rt, ctx, 0x08B34CA0u, {pin});
            call(rt, ctx, 0x08B34B24u, {pin});
        }
        if (owner != 0u) release_reference(rt, ctx, owner);
        call(rt, ctx, 0x08B8E380u, {frame.at(0x0Cu), copied, copied});
        if (owner != 0u) release_reference(rt, ctx, owner);
        put(rt, slot, word(rt, frame.at(0x08u)));
        call(rt, ctx, 0x08BAEF98u, {slot + 4u, frame.at(0x0Cu)});
    }

    // 0x088AFFC4: the slot's reference is only borrowed; drop the temporary and check the record's own stack.
    u32 after_release = release_reference(rt, ctx, word(rt, frame.at(0x0Cu)));
    const u32 record2 = word(rt, self + 0x48u);
    const u32 frame_object = word(rt, record2 + 8u);
    if (frame_object == 0u) {
        ctx.gpr[3] = after_release;
        ctx.gpr[2] = 4u;
        return;
    }

    // 0x088B003C: 0x08B79A88(frame_object, handler's position, its length).
    const u32 iface5 = word(rt, self + 0x38u);
    const u32 position = vcall(rt, ctx, iface5, 0x1Cu, {iface5});
    const u32 iface6 = word(rt, self + 0x38u);
    const u32 length = vcall(rt, ctx, iface6, 0x18u, {iface6});
    call(rt, ctx, 0x08B79A88u, {frame_object, position, length});

    // 0x088B0078: 0x08AC03E0(self, base + position, limit - base_position) must still answer non-zero.
    const u32 iface7 = word(rt, self + 0x38u);
    const u32 base = word(rt, word(rt, self + 0x48u) + 8u);
    const u32 position2 = vcall(rt, ctx, iface7, 0x18u, {iface7});
    const u32 iface8 = word(rt, self + 0x38u);
    const u32 limit = vcall(rt, ctx, iface8, 0x24u, {iface8});
    const u32 iface9 = word(rt, self + 0x38u);
    const u32 base_position = vcall(rt, ctx, iface9, 0x18u, {iface9});
    if (call(rt, ctx, 0x08AC03E0u, {self, base + position2, limit - base_position, 0u}) == 0u) {
        ctx.gpr[2] = 3u;
        return;
    }
    if (word(rt, self + 0x44u) == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }

    // 0x088B00EC: the last slot, mirroring the first one at +0x10 of the record.
    const u32 slot2 = word(rt, self + 0x48u) + 0x10u;
    const u32 resolved2 = call(rt, ctx, 0x089624F4u, {word(rt, self + 0x5Cu)});
    const u32 payload2 = word(rt, resolved2);
    if (payload2 == 0u) {
        put(rt, frame.at(0x00u), 0u);
        put(rt, frame.at(0x04u), 0u);
        put(rt, slot2, 0u);
        call(rt, ctx, 0x08BAEF98u, {slot2 + 4u, frame.at(0x04u)});
    } else {
        const u32 copied = call(rt, ctx, 0x089625E0u, {payload2, word(rt, self + 0x44u), 0x80u});
        const u32 owner = word(rt, resolved2 + 4u);
        const u32 parent = word(rt, resolved2);
        put(rt, frame.at(0x00u), copied);
        if (owner != 0u) {
            inc_count(rt, owner + 4u);
            inc_count(rt, owner + 4u);
            inc_count(rt, owner + 4u);
        }
        const u32 node = call(rt, ctx, 0x08B62344u, {0x18u});
        put(rt, node + 0x00u, 0x08CC2FA8u);
        put(rt, node + 0x04u, 1u);
        put(rt, node + 0x08u, 1u);
        put(rt, node + 0x0Cu, copied);
        put(rt, node + 0x10u, parent);
        put(rt, node + 0x14u, owner);
        if (owner != 0u) inc_count(rt, owner + 4u);
        release_reference(rt, ctx, owner);
        if (node == 0u) call(rt, ctx, 0x0896264Cu, {parent, copied});
        if (owner != 0u) release_reference(rt, ctx, owner);
        call(rt, ctx, 0x08B8E380u, {frame.at(0x04u), copied, copied});
        if (owner != 0u) release_reference(rt, ctx, owner);
        put(rt, slot2, word(rt, frame.at(0x00u)));
        call(rt, ctx, 0x08BAEF98u, {slot2 + 4u, frame.at(0x04u)});
    }

    // 0x088B0340: drop the temporary and run the machine on the frame's second half.
    const u32 after_release2 = release_reference(rt, ctx, word(rt, frame.at(0x04u)));
    const u32 record3 = word(rt, self + 0x48u);
    const u32 second_object = word(rt, record3 + 0x10u);
    if (second_object == 0u) {
        ctx.gpr[3] = after_release2;
        ctx.gpr[2] = 4u;
        return;
    }
    const u32 iface10 = word(rt, self + 0x38u);
    const u32 span = vcall(rt, ctx, iface10, 0x28u, {iface10});
    if (call(rt, ctx, 0x08AC03E0u, {self, second_object, span, 1u}) == 0u) {
        ctx.gpr[2] = 3u;
        return;
    }
    ctx.gpr[2] = 1u;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return byte_at(rt, address) | (byte_at(rt, address + 1u) << 8u); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(half_at(rt, address)); }

// `div` / `divu` leave their results undefined for a zero divisor; the R4000's convention (all ones, the dividend) is
// kept so a bytecode that divides by zero does not trap on the host.
s32 signed_quotient(s32 a, s32 b) {
    if (b == 0) return -1;
    if (a == static_cast<s32>(0x80000000u) && b == -1) return a;
    return a / b;
}
s32 signed_remainder(s32 a, s32 b) {
    if (b == 0) return a;
    if (a == static_cast<s32>(0x80000000u) && b == -1) return 0;
    return a % b;
}

// A call of a guest function that takes its arguments in $f12/$f13 and answers in $f0.
float float_call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, {}, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// 0x08AF6474 (a0 = the machine, a1 = the entry offset, a2 = the host object or 0): the bytecode interpreter.
// The word at +0 of the machine is the code base, +4 the global page table; locals and the value stack live in the
// 0x400-byte area at sp+0x10 (s4 = the current locals base, s1 = the stack top). 0x2711 steps, a stack underflow or an
// overflow past sp+0x410 end the run. The 178-entry jump table at 0x08C92764 selects the handlers; every opcode the
// table sends to 0x08AF64D8 is a one-byte no-op.

} // namespace

#define NEW1047(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1047::name, gt::ReturnKind::kind)
NEW1047(0x088AFBA4u, func_088AFBA4, Word);

} // namespace new_batch_main_1047
} // namespace gt::game
