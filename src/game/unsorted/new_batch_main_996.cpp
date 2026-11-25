// Main-session batch 996 (draft only — the gate runs elsewhere): eight functions of 166-167 instructions written by
// hand as structured C from `tools.disasm`. No name has evidence, so they keep the recompiler's func_XXXXXXXX names.
// 0x088FC794 and 0x088FCA30 are the same constructor body byte for byte (two entry points); they share one helper.
// Every exit path leaves v0 as the asm does: a call's result where the asm ends with a call, the value it keeps in
// s0 across the epilogue, the word it loads before returning, or (Void) whatever the last call left there.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_996 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call through `jal`: afterwards v0 / v1 / f0 are the callee's, as the instruction after the jal sees them.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
u32 callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
          std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// A `lw v1, slot(v0); jalr v1` call: v0 keeps the vtable it held, so the callee does not start with v0 = its address.
GuestResult call_via(Runtime &rt, AllegrexContext &ctx, u32 v0, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = v0;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
float fbits(u32 bits) { return std::bit_cast<float>(bits); }
s32 sar(s32 value, unsigned bits) { return static_cast<s32>(value >> bits); }
u32 wsbw(u32 value) { return __builtin_bswap32(value); }  // Allegrex wsbw: the bytes of the word reversed
void put8(Runtime &rt, u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); }
void put16(Runtime &rt, u32 a, u32 v) { rt.memory().store16(a, static_cast<std::uint16_t>(v)); }

// The release every temporary holder goes through: one less at +4; when that was the last, slot 8 of its vtable, then
// one less at +8 and, when that was the last too, slot 0xC.
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 holder) {
    if (holder == 0u) return;
    const u32 outer = word(rt, holder + 0x4u);
    put(rt, holder + 0x4u, outer - 1u);
    if (outer != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x8u), {holder});
    const u32 inner = word(rt, holder + 0x8u);
    put(rt, holder + 0x8u, inner - 1u);
    if (inner != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
}

// --- 0x0895BD2C ---
// a0 = owner: state = word(a0+8), sub = word(a0+0xC) + 0x270. Two queries (0x0895549C, 0x0895BC94) run first; the
// first one's f0 is the gain used at the end. The byte at state+0x5F8 picks an entry of the table at sub+0x58, the
// byte at state+0x5F9 = 1 turns on the step computation: a ratio (state+0x590 / state+0x5E8) clamped between the
// table entry at sub+0x58+4*byte(sub+0x10) and 1.0 is either used directly or, when 0x0895BBA4 answers non-zero,
// derived from the halfword at sub+0x48 by lerping two constants with state+0x530. The result moves the value at
// state+0x5F0 towards that target by gain*rate, setting the flag byte at state+0x512 when it lands short, and f0 =
// the new value * 0x089491A0(a0, f12 = state+0x534).

// --- 0x08AE5D60 ---
// a0 = a value only the 0x80 / 0x90 cases forward, a1 = the object, a2 = a flag that suppresses the store at +0x55.
// v0 = the first virtual slot's answer (0x2C); with bit 7 set, the slot at 0x34 answers a new code byte that is
// written to +0x54. The byte's high nibble (hi) and low nibble (low) are range-checked against the four bytes at
// +0x84..+0x87; inside the range the slots at 0x30 are asked for the two halves of a word handed to 0x08AE0CD8 and
// the dispatch runs with hi = 0xB0 anyway. hi selects a handler (0x80/0x90 take a0, 0xB0 answers v0 = 0 when
// 0x08AE5878 fails and restores +0x48/+0x4C), and every other hi logs through 0x08AD83B8 when the flag word at
// 0x08CDFB3C has bit 0 and then calls slot 0x34. v0 = 1 except on the 0xB0 failure path.
void func_08AE5D60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 value = ctx.gpr[4];  // s6
    const u32 object = ctx.gpr[5]; // s2
    const u32 flag = ctx.gpr[6];   // s7
    const u32 saved48 = word(rt, object + 0x48u);  // s5, loaded in the jalr's delay slot
    const u32 first = call_indirect(rt, ctx, word(rt, object + 0x2Cu), {object});
    u32 code;
    if ((first & 0x80u) != 0u) {
        code = call_indirect(rt, ctx, word(rt, object + 0x34u), {object});
        put8(rt, object + 0x54u, code);
        code &= 0xFFu;  // s3
    } else {
        code = rt.memory().load8(object + 0x54u);
    }
    const u32 low = code & 0xFu;    // s4
    const u32 hi = code & 0xF0u;    // s1
    bool in_range = !(hi < rt.memory().load8(object + 0x84u));
    if (in_range) in_range = !(rt.memory().load8(object + 0x85u) < hi);
    if (in_range) in_range = !(low < rt.memory().load8(object + 0x86u));
    if (in_range) in_range = !(rt.memory().load8(object + 0x87u) < low);
    if (in_range) {
        const u32 slot_a = word(rt, object + 0x30u);
        const u32 high_byte = call_indirect(rt, ctx, slot_a, {object, 1u});
        const u32 slot_b = word(rt, object + 0x30u);
        const u32 low_byte = call_via(rt, ctx, high_byte << 8, slot_b, {object, 0u}).v0;
        const u32 argument = (code << 16) | (high_byte << 8) | low_byte;
        const u32 table = word(rt, word(rt, object + 0x10u) + 0x10u);
        call(rt, ctx, 0x08AE0CD8u, {word(rt, table + 0x8u), argument});
    }
    if (hi == 0x80u) {
        call(rt, ctx, 0x08AE577Cu, {value, object, low, 0u});
        ctx.gpr[2] = 1u;
        return;
    }
    if (hi == 0x90u) {
        call(rt, ctx, 0x08AE577Cu, {value, object, low, 1u});
        ctx.gpr[2] = 1u;
        return;
    }
    if (hi == 0xB0u) {
        if (call(rt, ctx, 0x08AE5878u, {object, low}) != 0u) {
            ctx.gpr[2] = 1u;
            return;
        }
        put(rt, object + 0x48u, saved48);
        put(rt, object + 0x4Cu, 0u);
        ctx.gpr[2] = 0u;
        return;
    }
    if (hi == 0xC0u) {
        call(rt, ctx, 0x08AE5B50u, {object, low});
        ctx.gpr[2] = 1u;
        return;
    }
    if (hi == 0xD0u) {
        call(rt, ctx, 0x08AE5BDCu, {object, low});
        ctx.gpr[2] = 1u;
        return;
    }
    if (hi == 0xE0u) {
        call(rt, ctx, 0x08AE5BFCu, {object, low});
        ctx.gpr[2] = 1u;
        return;
    }
    if (hi == 0xF0u) {
        call(rt, ctx, 0x08AE5C60u, {object, low});
        ctx.gpr[2] = 1u;
        return;
    }
    if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) {
        call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u), 0x08C9145Cu, 0x9AAu, 0x08C918ECu, code});
    }
    if (flag != 0u) put8(rt, object + 0x55u, 1u);
    call_indirect(rt, ctx, word(rt, object + 0x34u), {object});
    ctx.gpr[2] = 1u;
}

// --- 0x088627F4 ---
// a0 = owner. state = word(owner) + 0x3F80; two probes (0x08891608, 0x08891544) must answer non-zero. 0x088915C8
// and 0x088915D4 answer two objects, three 8-byte temporaries are built at sp+0/8/0x10 and handed to 0x08861B20;
// the record count is the byte-reversed word at state+0xD8 (also written to the state's +4) and the walk over it runs
// 0x188-byte records: the two 0x30-byte temporaries at sp+0x28/0x58 are built, filled from slots 0..1 when the index
// is below 0x0888FE78's answer, copied out through 0x0885DBF4, and destroyed. When 0x08862150 answers 0 for an index
// the walk stops with v0 = 0; otherwise v0 = 0x088622CC(owner).
void func_088627F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xD0u);
    const u32 self = ctx.gpr[4];
    u32 state = word(rt, self) + 0x3F80u;
    if (call(rt, ctx, 0x08891608u, {state}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call(rt, ctx, 0x08891544u, {state}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 list = call(rt, ctx, 0x088915C8u, {state});
    state = call(rt, ctx, 0x088915D4u, {state});
    call(rt, ctx, 0x088D65DCu, {frame.at(0x0u)});
    call(rt, ctx, 0x08890478u, {list, frame.at(0x0u)});
    call(rt, ctx, 0x08894444u, {frame.at(0x8u)});
    call(rt, ctx, 0x08890448u, {list, frame.at(0x8u)});
    call(rt, ctx, 0x08893F7Cu, {frame.at(0x10u)});
    call(rt, ctx, 0x08890418u, {list, frame.at(0x10u)});
    call(rt, ctx, 0x08861B20u, {self, frame.at(0x0u), frame.at(0x8u), frame.at(0x10u), 0u});
    const u32 table = word(rt, word(rt, self) + 0x3F50u);
    const s32 count = static_cast<s32>(wsbw(word(rt, state + 0xD8u)));
    put(rt, frame.at(0x94u), static_cast<u32>(count));
    put(rt, table + 0x4u, static_cast<u32>(count));
    const u32 temp = frame.at(0x18u);
    u32 result = 0u;
    bool stopped = false;
    if (count > 0) {
        u32 index = 0u;
        u32 offset = 0u;
        put(rt, frame.at(0x90u), 0u);
        for (;;) {
            call(rt, ctx, 0x088A79ACu, {temp, 0u});
            call(rt, ctx, 0x088902ECu, {list, index, temp});
            const u32 elements = call(rt, ctx, 0x0888FE78u, {list, index});
            for (u32 i = 0u; i != 2u; ++i) call(rt, ctx, 0x08897B8Cu, {frame.at(0x28u + i * 0x30u)});
            for (u32 i = 0u; i != 2u; ++i) {
                const u32 slot = frame.at(0x28u + i * 0x30u);
                if (i < elements) {
                    call(rt, ctx, 0x08890330u, {list, index, i, slot});
                    put(rt, frame.at(0x20u + i * 0x4u), slot);
                } else {
                    put(rt, frame.at(0x20u + i * 0x4u), 0u);
                }
            }
            const u32 block = word(rt, word(rt, self) + 0x3F50u);
            const u32 destination = word(rt, block) + offset;
            const u32 source = call(rt, ctx, 0x088A5928u, {temp});
            call(rt, ctx, 0x0885DBF4u, {destination, source, temp, frame.at(0x20u)});
            const bool accepted = call(rt, ctx, 0x08862150u, {self, index}) != 0u;
            // The two temporaries are destroyed from the top down: sp+0x88 - 0x30, then sp+0x28.
            call(rt, ctx, 0x088978B4u, {frame.at(0x88u - 0x30u)});
            call(rt, ctx, 0x088978B4u, {frame.at(0x28u)});
            call(rt, ctx, 0x088A7548u, {temp});
            if (!accepted) {
                stopped = true;
                result = 0u;
                break;
            }
            offset += 0x188u;
            ++index;
            put(rt, frame.at(0x90u), offset);
            if (index == static_cast<u32>(count)) {
                result = call(rt, ctx, 0x088622CCu, {self});
                break;
            }
        }
    }
    if (!stopped && count <= 0) result = call(rt, ctx, 0x088622CCu, {self});
    call(rt, ctx, 0x08893CC0u, {frame.at(0x10u)});
    call(rt, ctx, 0x08894028u, {frame.at(0x8u)});
    call(rt, ctx, 0x088D5CF0u, {frame.at(0x0u)});
    ctx.gpr[2] = result;
}

// --- 0x08882030 ---
// a0 = state, a1 = a context, a2 = a pointer to the object. Two 38-byte records at state+0x1710+38*0x0886F8DC(a2) are
// marked (byte +0xC); the first free one is filled by walking the seven pointers copied from 0x08C8994C (and, past
// the seven, whatever the frame holds) until a null or an empty first byte, storing a halfword per entry from slots
// 0x54 (or 0x60 when slot 0x44 answers non-zero). The second phase walks the seven halfwords at record+2: a negative
// one is skipped, otherwise slots 0x94 / 0xC4 answer, and per index 0..5 one of three stores runs — 0x08B4E9D4(a1,
// slot 0x94's answer) for index 0, the float at a1+0x30 for index 1, and the pair at the answer's +0/+4 scaled by
// 0x3C8EFA35 into state+0x1018+0x24*index for the rest (setting bit 3 of state+0x16E0). v0 is 0 when a2 or
// 0x0886F8D4 answers 0, and untouched (the entry value) when a2 itself is null.
void func_08882030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 state = ctx.gpr[4];   // s5
    const u32 context = ctx.gpr[5]; // s6
    const u32 holder = ctx.gpr[6];  // s0
    if (holder == 0u) return;
    const u32 object = call(rt, ctx, 0x0886F8D4u, {holder});
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    // The seven pointers at 0x08C8994C are copied into the frame; the walk below reaches past them into the spilled
    // registers, so those go back to the values the original's prologue wrote there.
    for (u32 i = 0u; i != 7u; ++i) put(rt, frame.at(i * 0x4u), word(rt, 0x08C8994Cu + i * 0x4u));
    put(rt, frame.at(0x20u), ctx.gpr[16]);
    put(rt, frame.at(0x24u), ctx.gpr[17]);
    put(rt, frame.at(0x28u), ctx.gpr[18]);
    put(rt, frame.at(0x2Cu), ctx.gpr[19]);
    put(rt, frame.at(0x30u), ctx.gpr[20]);
    put(rt, frame.at(0x34u), ctx.gpr[21]);
    put(rt, frame.at(0x38u), ctx.gpr[22]);
    put(rt, frame.at(0x3Cu), ctx.gpr[31]);
    const u32 first_record = state + 0x1710u + 38u * call(rt, ctx, 0x0886F8DCu, {holder});
    const u32 slot = first_record + 0xCu;  // s2: phase two always reads this record's array
    if (rt.memory().load8(slot) == 0u) {
        const u32 second_record = state + 0x1710u + 38u * call(rt, ctx, 0x0886F8DCu, {holder});
        if (rt.memory().load8(second_record + 0xCu) == 0u) {
            put8(rt, second_record + 0xCu, 1u);
            // The two walks differ only in the slot they call for each entry (0x54, or 0x60 when slot 0x44 answers
            // non-zero while 0x40 does not); the frame words past the seven copied pointers are walked too.
            const auto fill = [&](u32 fill_slot) {
                u32 out = second_record + 0xCu;
                for (u32 offset = 0u; offset != 0x48u; offset += 0x4u) {
                    const u32 entry = word(rt, frame.at(offset));
                    if (entry == 0u) break;
                    if (static_cast<std::int8_t>(rt.memory().load8(entry)) == 0) break;
                    put16(rt, out + 0x2u,
                          call_indirect(rt, ctx, word(rt, word(rt, object) + fill_slot), {object, entry}));
                    out += 0x2u;
                }
            };
            if (static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, object) + 0x40u), {object})) > 0) {
                fill(0x54u);
            } else if (static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, object) + 0x44u), {object})) > 0) {
                fill(0x60u);
            }
        }
    }
    u32 target = slot;  // s0
    u32 base = state;   // s1
    for (u32 index = 0u; index != 7u; ++index) {
        const s32 half = static_cast<std::int16_t>(rt.memory().load16(target + 0x2u));
        if (half >= 0) {
            const u32 first = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x94u), {object});
            const u32 second = call_indirect(rt, ctx, word(rt, word(rt, object) + 0xC4u), {object, 0u,
                                                                                           static_cast<u32>(half)});
            if (index < 6u) {
                if (index == 0u) {
                    call(rt, ctx, 0x08B4E9D4u, {context, first});
                } else if (index == 1u) {
                    put_float(rt, state + 0x16E4u, float_at(rt, context + 0x30u));
                } else {
                    put_float(rt, state + 0x16E4u, float_at(rt, second + 0x4u));
                    const float scale = fbits(0x3C8EFA35u);
                    put(rt, state + 0x16E0u, word(rt, state + 0x16E0u) | 0x8u);
                    put_float(rt, base + 0x1018u, psp_mul(float_at(rt, second), scale));
                    put(rt, base + 0x101Cu, 0u);
                    put_float(rt, base + 0x1020u, psp_mul(float_at(rt, second + 0x4u), scale));
                }
            }
        }
        target += 0x2u;
        base += 0x24u;
    }
}

// --- 0x088E06E8 ---
// a0 = owner. 0x088DAA84(a0) must answer non-zero; when either word at the +0x130 object's +0x40/+0x44 is set the
// function answers that object. Otherwise a count ((a0+0x660 - a0+0x65C) >> 4) is compared against 0x088DAA8C's
// answer, and when the count is smaller that answer is the result. With a0+0x640 null or 0x088E5218 accepting it, the
// first walk visits each 0x2F8-byte record at word(a0+0xC0) and passes the accepted ones to 0x088DC180. The second
// walk over the same records (index = a0+0x130 + i, record = word(a0+0xC0)+0x2F8*i) handles the ones whose +4 word is
// 1: two probes (0x088E85E8, 0x088E8778) accept, 0x088F8D58 and 0x088F8A84 answer two objects, the object at
// a0+0x148 is asked through its slot 0x60 (a2 = (word(record for byte at +0x54)+0x1C) == 0, t1 = that record's
// +0x2C), the second 0x088F8D58 answer gets its slots 0x10 / 0x1C called and byte +0xC set / +0xB cleared, and the
// +0x134 object is called through slot 0x18. v0 = 0 after the walk.
void func_088E06E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    if (call(rt, ctx, 0x088DAA84u, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 holder = word(rt, self + 0x130u);
    if ((word(rt, holder + 0x40u) | word(rt, holder + 0x44u)) != 0u) {
        ctx.gpr[2] = holder;
        return;
    }
    const u32 span = static_cast<u32>(
        sar(static_cast<s32>(word(rt, self + 0x660u) - word(rt, self + 0x65Cu)), 4));
    const u32 limit = call(rt, ctx, 0x088DAA8Cu, {self});
    if (static_cast<s32>(span) < static_cast<s32>(limit)) {
        ctx.gpr[2] = limit;
        return;
    }
    const u32 gate = word(rt, self + 0x640u);
    bool walk = true;
    if (gate != 0u && call(rt, ctx, 0x088E5218u, {gate}) == 0u) walk = false;
    if (walk) {
        u32 index = 0u;
        u32 offset = 0u;
        while (static_cast<s32>(index) < static_cast<s32>(call(rt, ctx, 0x088D9F80u, {self}))) {
            if (call(rt, ctx, 0x088E86D0u, {word(rt, self + 0xC0u) + offset}) != 0u) {
                call(rt, ctx, 0x088DC180u,
                     {word(rt, self + 0x130u), index, word(rt, frame.at(0x0u)), word(rt, frame.at(0x4u))});
            }
            ++index;
            offset += 0x2F8u;
        }
    }
    for (u32 index = 0u, offset = 0u; static_cast<s32>(index) < static_cast<s32>(call(rt, ctx, 0x088D9F80u, {self}));
         ++index, offset += 0x2F8u) {
        const u32 record = word(rt, self + 0xC0u) + offset;
        const u32 flags = word(rt, self + 0x130u) + index;
        const u32 second = static_cast<u32>(static_cast<std::int8_t>(rt.memory().load8(flags + 0x58u)));
        const u32 first = static_cast<u32>(static_cast<std::int8_t>(rt.memory().load8(flags + 0x54u)));
        if (word(rt, record + 0x4u) == 1u && call(rt, ctx, 0x088E85E8u, {record}) != 0u &&
            call(rt, ctx, 0x088E8778u, {record, second, first}) != 0u) {
            call(rt, ctx, 0x088F8D58u, {word(rt, self + 0x14Cu), index});
            const u32 owner = ctx.gpr[2];  // fp: the first query's answer
            call(rt, ctx, 0x088F8A84u, {word(rt, self + 0x14Cu), index, second});
            const u32 accessory = call(rt, ctx, 0x088F8D58u, {word(rt, self + 0x14Cu), index});
            const u32 own_record = word(rt, self + 0xC0u) + 0x2F8u * first;
            const u32 carrier = word(rt, self + 0x148u);
            const u32 flag = (word(rt, own_record + 0x1Cu) ^ 1u) < 1u ? 1u : 0u;
            call_indirect(rt, ctx, word(rt, word(rt, carrier) + 0x60u),
                          {carrier, index, flag, accessory, first, word(rt, own_record + 0x2Cu)});
            if (accessory != 0u) {
                const u32 kind = call(rt, ctx, 0x088E85C4u, {record});
                if (kind - 4u < 2u) {
                    call_indirect(rt, ctx, word(rt, word(rt, accessory) + 0x10u), {accessory, 1u});
                    call_indirect(rt, ctx, word(rt, word(rt, accessory) + 0x1Cu), {accessory, 1u});
                } else {
                    call_indirect(rt, ctx, word(rt, word(rt, accessory) + 0x10u),
                                  {accessory, static_cast<u32>(rt.memory().load8(owner + 0x9u))});
                    call_indirect(rt, ctx, word(rt, word(rt, accessory) + 0x1Cu), {accessory, 0u});
                }
                put8(rt, accessory + 0xCu, 1u);
                put8(rt, accessory + 0xBu, 0u);
            }
            const u32 sink = word(rt, self + 0x134u);
            if (sink != 0u) {
                const u32 sink_vtable = word(rt, sink);
                call_via(rt, ctx, sink_vtable, word(rt, sink_vtable + 0x18u), {sink, index, second, first});
            }
        }
    }
    ctx.gpr[2] = 0u;
}

// --- 0x088FC794 / 0x088FCA30 (the same body, two entry points) ---
// a0 = the object, a1 = a pair of reference-counted holders, a2 = an object, a3 = a flag byte. The vtable 0x08CC4310
// goes to a0+0, a 0x3C-byte block is allocated and initialised at a0+4, the two holders are copied from a1 (+8/+0xC
// and +0x10/+0x14, each non-null one's count at +4 incremented), the sub-object at a0+0x34 is built from a1 / a2 /
// a0+4 and the ones at a0+0x3C (vtable 0x08CC5470), +0x6C and +0x528 are initialised. a2's slot 0x38 and the vtable it
// came from land at a0+0x530 / a0+0x534, the ring at a0+0x540 is closed, and 0x08BC9A34 / 0x08BC7ED8 / 0x089312DC /
// 0x08B3767C / 0x08BC0808 wire the rest up; each temporary holder they hand back is released (count at +4, and at +8
// once the first hits zero, then slots 8 and 0xC of its vtable). No result register is defined.
void construct_088FC794(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame) {
    const u32 self = ctx.gpr[4];   // s1
    const u32 source = ctx.gpr[5]; // s3
    const u32 other = ctx.gpr[6];  // s2
    const u32 flag = ctx.gpr[7] & 0xFFu;
    put(rt, self + 0x0u, 0x08CC4310u);
    const u32 block = call(rt, ctx, 0x08B62344u, {0x3Cu});
    call(rt, ctx, 0x08B28D34u, {block});
    put(rt, block + 0x2Cu, 0u);
    put8(rt, block + 0x30u, 0u);
    put(rt, block + 0x34u, 0u);
    put(rt, block + 0x38u, 0u);
    put(rt, self + 0x4u, block);
    put(rt, self + 0x8u, word(rt, source));
    const u32 holder1 = word(rt, source + 0x4u);
    put(rt, self + 0xCu, holder1);
    if (holder1 != 0u) put(rt, holder1 + 0x4u, word(rt, holder1 + 0x4u) + 1u);
    put(rt, self + 0x10u, word(rt, source + 0x8u));
    const u32 holder2 = word(rt, source + 0xCu);
    put(rt, self + 0x14u, holder2);
    if (holder2 != 0u) put(rt, holder2 + 0x4u, word(rt, holder2 + 0x4u) + 1u);
    const u32 self_block = self + 0x4u;  // s4
    put8(rt, self + 0x30u, flag);
    for (u32 offset = 0x18u; offset <= 0x2Cu; offset += 0x4u) put(rt, self + offset, 0u);
    call(rt, ctx, 0x08906A44u, {self + 0x34u, source, other, self_block});
    put(rt, self + 0x3Cu, 0x08CC5470u);
    call(rt, ctx, 0x08900A38u, {self + 0x6Cu});
    call(rt, ctx, 0x08910FA0u, {self + 0x528u});
    const u32 vtable = word(rt, other);
    const u32 answer = call_indirect(rt, ctx, word(rt, vtable + 0x38u), {other});
    put(rt, self + 0x530u, answer);
    put(rt, self + 0x534u, ctx.gpr[3]);  // v1 after the call, as the original stores it
    put(rt, self + 0x538u, 0u);
    put(rt, self + 0x53Cu, 0u);
    const u32 ring = self + 0x540u;
    put(rt, ring + 0x4u, ring);
    put(rt, self + 0x540u, ring);
    call(rt, ctx, 0x08BC9A34u,
         {frame.at(0x8u), self, word(rt, self + 0x530u), word(rt, self + 0x534u), self_block});
    call(rt, ctx, 0x08BC7ED8u, {word(rt, frame.at(0x8u))});
    release_holder(rt, ctx, word(rt, frame.at(0xCu)));
    const u32 pair = call(rt, ctx, 0x089312DCu, {word(rt, self + 0x530u), word(rt, self + 0x534u)});
    call(rt, ctx, 0x08B3767Cu, {frame.at(0x10u), 0x40u, 0x08CA0F4Cu, pair});
    put(rt, frame.at(0x0u), 0u);
    put(rt, frame.at(0x4u), 0u);
    call(rt, ctx, 0x08BC0808u,
         {self + 0x538u, source, frame.at(0x10u), self_block, 0x10u, frame.at(0x0u)});
    release_holder(rt, ctx, word(rt, frame.at(0x4u)));
}

// --- 0x089DE324 ---
// a0 = owner (the two 12-byte vectors at +0x100 (bit 8 of +0x140 clear) and +0x10C), a1 = a pointer to the object.
// The object's slot 0x7C answers a count. With a count the vector is shortened or grown to it, then per element the
// object at *(a1) is asked through slot 0x84 into the temporary at sp+0xC, that temporary's object through slot 0x84
// into sp+8 (slot 0x54's float lands at element+0), and through 0x84 into sp+4 with index 1 (slot 0x54's float at
// element+4); every temporary is released through 0x08A5B9EC. With a zero count the vector's three words are zeroed
// and 0x08BF0BF0 / 0x08B35254 dispose of them. A null where an object is expected takes the 0x08A5BB7C assert path.

} // namespace

#define NEW996(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_996::name, gt::ReturnKind::kind)
NEW996(0x08AE5D60u, func_08AE5D60, Word);
NEW996(0x088627F4u, func_088627F4, Word);
NEW996(0x08882030u, func_08882030, Word);
NEW996(0x088E06E8u, func_088E06E8, Word);

} // namespace new_batch_main_996
} // namespace gt::game
