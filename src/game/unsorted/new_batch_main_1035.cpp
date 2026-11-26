// New batch main 1035: hand-written drafts from `tools/disasm`, one function at a time. Delay slots are read as
// "before the call / branch"; `v0` is written wherever the asm leaves it (it is compared on every exit path) and the
// callee-saved registers are never touched, as the asm leaves them as it found them. Not verified yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1035 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call that keeps v0 / v1 / f0: the caller sees the callee's answers.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// The argument registers are set like the original sets them before the `jal` (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
inline u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot,
                           std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}

// The shared-pointer counts of the 0x08Bxxxxx library: `ll/sc` steps on guest words (single-threaded here). A release
// drops the use count (+4); at zero the object's slot 8 runs, then the weak count (+8) drops and at zero its slot 0xC.
inline void count_up(Runtime &rt, u32 control) { put(rt, control + 4u, word(rt, control + 4u) + 1u); }
inline void release_control(Runtime &rt, AllegrexContext &ctx, u32 control) {
    if (control == 0u) return;
    const u32 uses = word(rt, control + 4u);
    put(rt, control + 4u, uses - 1u);
    if (uses != 1u) return;
    (void)call_virtual_at(rt, ctx, control, 8u, {control});
    const u32 weak = word(rt, control + 8u);
    put(rt, control + 8u, weak - 1u);
    if (weak != 1u) return;
    (void)call_virtual_at(rt, ctx, control, 0xCu, {control});
}
// `slot`'s control word takes `control`: counted up first, the old one released, then stored.
inline void assign_control(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 control) {
    if (control == word(rt, slot)) return;
    if (control != 0u) count_up(rt, control);
    release_control(rt, ctx, word(rt, slot));
    put(rt, slot, control);
}

// func_08BD4A8C(request): a request's sending step (state +4: 0 starts it with func_08AC04B4 and becomes 0xFA; 0xFA
// waits until the transfer (+8)->+0xA8 is done, then hands self->+0x7C (the size at +0xB8 of the transfer) and the
// buffer pair built from the store func_089624EC(self->+0x38) to self->+0x80, moves the state to 0x107 through
// func_08AC0448; 0x107 finishes through func_08AF771C / func_08AF7B0C and moves self->+0x84 into the holder
// func_08AF7B0C's object at +0x13C / +0x140). The frame's own pair (func_08BD3C3C at sp+8) is released on every exit.

inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline u32 call_virtual_atf(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments,
                            std::initializer_list<float> floats) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, ctx.gpr[2], arguments, floats).v0;
}

inline u32 swap_bytes(u32 value) {
    return (value >> 24) | ((value >> 8) & 0xFF00u) | ((value << 8) & 0xFF0000u) | (value << 24);
}
inline void put16(Runtime &rt, u32 address, u32 value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline s32 allegrex_div(s32 numerator, s32 denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

// func_0888F558(request, session, ...): builds a 0x178-byte header and a table of 0x20-byte entries in the block at
// *request (the pair counts come from func_08AC0448's helpers: func_089624F4 and friends below). Every offset stored in
// the block is `pointer - block` byte-swapped, hence the `wsbw` pairs; the two closing loops write the tail entries.

// func_08B00770(self, request, arg2, flag): walks the flag word +0xE2 (0x2000 gates the whole body, bit 0 / 0x200 pick
// the first index) of `self`; the objects named by request->+0x38 (through their slot 0x10) whose answer is non-zero are
// remembered as a bit set, made real through func_08B01804 / func_08B01154. Then for every entry of self->+0xD8's table
// (+0x7E count) it builds two index tables (func_08B0066C / func_08B00764), visits the remembered objects through
// func_08B01D1C and the linked list at request->+0x60 (0x54-byte nodes, next at +0x52, label +0x4C, bit +0x4D), fills
// their vectors and halfword flag, and finally calls func_08B012D8 for the remembered objects that still need it.
void func_08B00770(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x380u);
    const auto sp = [&](u32 offset) { return frame.at(offset); };
    const u32 self = ctx.gpr[4];
    const u32 request = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    const u32 flag = ctx.gpr[7] & 0xFFu;
    const u32 flags = rt.memory().load16(self + 0xE2u);
    if ((flags & 0x2000u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 list = request + 0x38u;
    const u32 count = static_cast<u32>(signed_of(word(rt, list + 4u) - word(rt, list)) >> 2);
    const u32 start = ((flags & 1u) != 0u || (flags & 0x200u) != 0u) ? 1u : 0u;
    u32 limit = 0xFFFFFFFFu;
    if (flag != 0u) {
        limit = word(rt, self + 0xD0u);
        if (signed_of(limit) < 0) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    if (!(signed_of(start) < signed_of(count))) {
        ctx.gpr[2] = 0u;
        return;
    }

    const u32 base = self + 0x20u;
    u32 bits = 0u;  // sp+0x320
    {
        u32 offset = start << 2;
        u32 which = start;
        do {
            const u32 object = word(rt, word(rt, request + 0x38u) + offset);
            const float f12 = float_at(rt, word(rt, self + 0xD8u) + 0x20u);
            const u32 answer = call_virtual_atf(rt, ctx, object, 0x10u, {object, base, limit}, {f12});
            const u32 bit = 1u << which;
            which += 1u;
            offset += 4u;
            if (answer != 0u) bits |= bit;
        } while (which != count);
    }
    if (bits == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }

    {
        u32 which = start;                       // s6, runs to `count`
        u32 digits = 0xFFFFFFFFu - start;        // s0 = -(start+1)
        u32 slot = sp(0xA0u) + (start << 2);     // s2
        for (;;) {
            const u32 mask = 1u << which;
            const u32 pass = digits;
            which += 1u;
            const bool set = (bits & mask) != 0u;
            digits -= 1u;
            if (set) {
                const u32 made = call_set(rt, ctx, 0x08B01804u, {request, arg2, pass});
                if (made != 0u) {
                    put(rt, slot, made);
                    put16(rt, made, rt.memory().load16(made) & ~1u);
                    (void)call_set(rt, ctx, 0x08B01154u, {made, request});
                }
            }
            if (which == count) break;
            slot += 4u;
        }
    }

    u32 any = 0u;  // sp+0x314
    const u32 entries = rt.memory().load16(word(rt, self + 0xD8u) + 0x7Eu);
    if (entries != 0u) {
        u32 outer = 0u;                        // sp+0x30C
        for (;;) {
            (void)call_set(rt, ctx, 0x08B0066Cu, {self, sp(0x120u), outer, self + 0x10u, self + 0x2Cu});
            const u32 units = call_set(rt, ctx, 0x08B00764u, {self, sp(0x200u), outer});
            u32 left = 0xFFFFFFFFu - start;    // sp+0x2F4, reset from sp+0x2F0 each pass
            u32 offset = start << 2;
            u32 which = start;
            while (which != count) {
                if ((bits & (1u << which)) != 0u) {
                    const u32 slot = sp(0xA0u) + 4u * which;
                    const u32 listed = word(rt, slot);
                    const u32 entry = word(rt, word(rt, request + 0x38u) + offset);
                    const s32 index = listed != 0u ? static_cast<std::int16_t>(rt.memory().load16(listed + 4u)) : -1;
                    put(rt, sp(0x300u), slot);
                    if (index >= 0) {
                        u32 processed = 0u;   // sp+0x304
                        if (signed_of(units) > 0) {
                            for (u32 part = 0u; part != units; ++part) {
                                const u32 first = call_set(rt, ctx, 0x08C45D94u, {sp(0x120u) + (part << 4)});
                                const u32 second = call_set(rt, ctx, 0x08C45D94u, {sp(0x200u) + (part << 4)});
                                const u32 verdict =
                                    call_virtual_at(rt, ctx, entry, 0xCu, {entry, sp(0x60u), first, second, limit});
                                if (signed_of(verdict) < 0) continue;
                                const GuestResult span2 = call_keep(rt, ctx, 0x08BAC270u, {second, base}, {});
                                put(rt, sp(0x2E0u), span2.v0);
                                put(rt, sp(0x2E4u), span2.v1);
                                put(rt, sp(0x20u), span2.v0);
                                put(rt, sp(0x24u), span2.v1);
                                (void)call_set(rt, ctx, 0x08BAC428u, {sp(0x34u), span2.v0, span2.v1});
                                put(rt, sp(0x28u), word(rt, sp(0x60u)));
                                put(rt, sp(0x2Cu), word(rt, sp(0x64u)));
                                put(rt, sp(0x30u), word(rt, sp(0x68u)));
                                put(rt, sp(0x0Cu), sp(0x28u));
                                put(rt, sp(0x14u), sp(0x60u));
                                put(rt, sp(0x0u), 0u);
                                put(rt, sp(0x4u), 0u);
                                put(rt, sp(0x8u), second);
                                const u32 made = call_set(rt, ctx, 0x08B01D1Cu,
                                                          {request, slot, 0u, arg2, outer, part, sp(0x34u), left});
                                if (made == 0u) {
                                    ctx.gpr[2] = any;
                                    return;
                                }
                                const u32 direction = call_set(rt, ctx, 0x08C45D9Cu, {second});
                                const GuestResult turn = call_keep(rt, ctx, 0x08C46364u, {sp(0x60u), direction}, {});
                                const float flipped = -turn.f0;
                                processed |= 1u << part;
                                put(rt, sp(0x304u), processed);
                                put_float(rt, made + 0x30u, flipped);
                                any = 1u;
                                put_float(rt, made + 0x24u,
                                          psp_mul(float_at(rt, word(rt, self + 0xD8u) + 0x68u) + float_at(rt, sp(0x74u)),
                                                  0.5f));
                                put_float(rt, made + 0x28u,
                                          psp_mul(float_at(rt, word(rt, self + 0xD8u) + 0x64u) + float_at(rt, sp(0x70u)),
                                                  0.5f));
                                put_float(rt, made + 0x2Cu,
                                          psp_mul(float_at(rt, word(rt, self + 0xD8u) + 0x6Cu), float_at(rt, sp(0x78u))));
                                const u32 target = word(rt, slot);
                                put16(rt, target, rt.memory().load16(target) | 1u);
                            }
                        }
                        // The linked list request->+0x60, a node per 0x54 bytes.
                        const u32 link = request + 0x60u;
                        const u32 stale = word(rt, sp(0x40u));
                        const u32 head = word(rt, link) + 84u * static_cast<u32>(index);
                        put(rt, sp(0x60u), stale);
                        put(rt, sp(0x4Cu), 0u);
                        put(rt, sp(0x48u), head);
                        put(rt, sp(0x44u), link);
                        put(rt, sp(0x50u), stale);
                        put(rt, sp(0x54u), link);
                        put(rt, sp(0x58u), head);
                        put(rt, sp(0x5Cu), 0u);
                        put(rt, sp(0x64u), link);
                        put(rt, sp(0x68u), head);
                        put(rt, sp(0x6Cu), 0u);
                        u32 node = head;
                        while (node != 0u) {
                            const u32 part = 1u << rt.memory().load8(node + 0x4Du);
                            const s32 label = static_cast<std::int8_t>(rt.memory().load8(node + 0x4Cu));
                            if (signed_of(outer) == label && (processed & part) == 0u) {
                                const u32 index2 = rt.memory().load8(node + 0x4Du);
                                const u32 made2 = call_set(rt, ctx, 0x08C45DA4u, {sp(0x200u) + (index2 << 4)});
                                const u32 sized = call_set(rt, ctx, 0x08C467D4u, {made2});
                                const u32 verdict = call_virtual_at(rt, ctx, entry, 0xCu,
                                                                    {entry, sp(0x80u), base, sized, limit});
                                if (signed_of(verdict) >= 0) {
                                    put8(rt, node + 0x51u, 1u);
                                    const GuestResult turn = call_keep(rt, ctx, 0x08C46364u, {sp(0x80u), made2}, {});
                                    const float flipped = -turn.f0;
                                    put_float(rt, node + 0x30u, flipped);
                                    any = 1u;
                                    const GuestResult span2 = call_keep(rt, ctx, 0x08BAC270u, {sized, base}, {});
                                    put(rt, sp(0x2E0u), span2.v0);
                                    put(rt, sp(0x2E4u), span2.v1);
                                    put(rt, sp(0x18u), span2.v0);
                                    put(rt, sp(0x1Cu), span2.v1);
                                    (void)call_set(rt, ctx, 0x08BAC428u, {node + 0xCu, span2.v0, span2.v1});
                                    put(rt, sp(0x10u), sp(0x80u));
                                    put_float(rt, node, float_at(rt, sp(0x80u)));
                                    put_float(rt, node + 4u, float_at(rt, sp(0x84u)));
                                    put_float(rt, node + 8u, float_at(rt, sp(0x88u)));
                                    const u32 target = word(rt, word(rt, sp(0x300u)));
                                    put16(rt, target, rt.memory().load16(target) | 1u);
                                }
                            }
                            const s32 next = static_cast<std::int16_t>(rt.memory().load16(node + 0x52u));
                            if (next < 0) break;
                            node = word(rt, request + 0x60u) + 84u * static_cast<u32>(next);
                        }
                    }
                }
                which += 1u;
                offset += 4u;
                left -= 1u;
            }
            outer += 1u;
            if (entries == outer) break;
        }
    }
    for (u32 which = start; which != count; ++which) {
        if ((bits & (1u << which)) == 0u) continue;
        const u32 object = word(rt, sp(0xA0u) + 4u * which);
        if (object != 0u && (rt.memory().load16(object) & 1u) != 0u)
            (void)call_set(rt, ctx, 0x08B012D8u, {object, request});
    }
    ctx.gpr[2] = any;
}

} // namespace
} // namespace new_batch_main_1035
} // namespace gt::game

#define NEWMAIN1035(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1035::name, gt::ReturnKind::kind)
NEWMAIN1035(0x08B00770u, func_08B00770, Word);
