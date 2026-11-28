// New batch main 1587: functions drafted before and re-derived from `tools.disasm` (main session writes, the verifier
// gates). Structured C: named locals, `G[n]` only at the call boundary, every exit's v0/v1 as the asm leaves them, the
// callee-saved registers the original saves restored, signed/unsigned as the instruction used.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1587 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using GuestResult = gt::GuestResult;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 / v1 / f0 are the callee's, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// The same with float arguments (EABI: they go in $f12-$f15).
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                  std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v1, 0(object); lw v0, offset(v1); jalr v0`: v1 holds the vtable and v0 the slot's address when the callee starts.
GuestResult call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + offset);
    return callf(rt, ctx, ctx.gpr[2], arguments, {});
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// `div` / `divu` as the recompiled original computes lo and hi (a zero divisor keeps the MIPS fallback; the original's
// generated `break` guard stops its own run there, so such a case is not comparable).
u32 signed_quotient(s32 dividend, s32 divisor) {
    if (divisor == 0) return dividend >= 0 ? 0xFFFFFFFFu : 1u;
    if (dividend == static_cast<s32>(0x80000000u) && divisor == -1) return 0x80000000u;
    return static_cast<u32>(dividend / divisor);
}
u32 signed_remainder(s32 dividend, s32 divisor) {
    if (divisor == 0) return static_cast<u32>(dividend);
    if (dividend == static_cast<s32>(0x80000000u) && divisor == -1) return 0u;
    return static_cast<u32>(dividend % divisor);
}

// The callee-saved registers the original stores in its prologue and reloads before it returns: their exit value is the
// entry one (the harness compares s0-s7 / fp / ra).
struct SavedRegs {
    AllegrexContext &ctx;
    u32 regs[10];
    u32 values[10];
    unsigned count = 0u;
    SavedRegs(AllegrexContext &c, std::initializer_list<unsigned> list) : ctx(c) {
        for (const unsigned r : list) {
            regs[count] = r;
            values[count] = c.gpr[r];
            ++count;
        }
    }
    ~SavedRegs() {
        for (unsigned i = 0u; i < count; ++i) ctx.gpr[regs[i]] = values[i];
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;
};

// The "release a counted block" tail four of these share: `v = *ptr; inner = v - 0xC; if (inner != 0x08D74FD0 &&
// 0x08B60804(v - 4, -1) <= 0) 0x08B8542C(inner, release_argument);`.
void release_block(Runtime &rt, AllegrexContext &ctx, u32 value, u32 release_argument) {
    const u32 inner = value - 0xCu;
    if (inner == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {value - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {inner, release_argument});
}

// 0x088E5AE8 (a0 = object with a member pointer at +0 and another at +4): the two setup calls, then the member at +0's
// word +0xA4 is torn down through its virtual +0x2C after 0x08B2E2B8(0x4E20) and cleared, the member at +4 goes through
// its virtual +4, and the member at +0 is disposed of by a tail call to its virtual +4. v0 = 0 when that member is null.

// 0x089CF190 (a0 = object, a1 = a value): a frame record is built (0x08969188(sp, 0x08CF6ED8)), 0x089CDDF8 fills it from
// (a0, a1) and 0x08A5B9EC releases it. When the pending index at +0x1A0 is non-negative and differs from the one at
// +0x198, 0x089CF134 gates an optional 0x089CE6F0(a0, pending), then 0x089CCA54's answer goes to 0x089CE6F0 with the low
// bit of +0x14C and +0x1C0 is raised. The pending index becomes -1 and v0 = -1 on every path.
void func_089CF190(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx, {16u, 17u});
    call(rt, ctx, 0x08969188u, {frame.at(0u), 0x08CF6ED8u});
    call(rt, ctx, 0x089CDDF8u, {self, argument, frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    const s32 pending = static_cast<s32>(word(rt, self + 0x1A0u));
    if (pending >= 0 && word(rt, self + 0x198u) != static_cast<u32>(pending)) {
        if (call(rt, ctx, 0x089CF134u, {self}) != 0u) {
            call(rt, ctx, 0x089CE6F0u, {self, word(rt, self + 0x1A0u), 0u});
        }
        const u32 value = call(rt, ctx, 0x089CCA54u, {self});
        call(rt, ctx, 0x089CE6F0u, {self, value, word(rt, self + 0x14Cu) & 1u});
        put(rt, self + 0x1C0u, 1u);
    }
    put(rt, self + 0x1A0u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x089E6EEC: the same shape as 0x089CF190 with the indices at +0x1A4 / +0x19C, its own gates (0x089E6218, 0x089E6E60,
// 0x089E6D0C twice with the low bit of +0x150) and the flag at +0x1D8.

// 0x08A1F520 (a1 = a selector, a2 = a pointer to the holder pointer): selector 0 answers 0x08A4D2F0(word 0x08CF68E8),
// any selector other than 1 answers 1, and 1 releases the holder's object: its virtual +0xC writes into sp + 4,
// 0x08A4D2F0(0x08CF68E8, sp[1]) runs, and the block at sp[1] (reported through 0x08A5BB7C(0x08CAA480) when the holder is
// null) is freed through 0x08B60804(-1) and 0x08B8542C unless it is the shared empty 0x08D74FD0 or 0x08B60804 answers > 0.
void func_08A1F520(Runtime &rt, AllegrexContext &ctx) {
    const u32 which = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx, {16u});
    if (which == 0u) {
        call(rt, ctx, 0x08A4D2F0u, {word(rt, 0x08CF68E8u)});
        return;
    }
    if (which != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 object = word(rt, ctx.gpr[6]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {frame.at(4u), object});
    call(rt, ctx, 0x08A4D2F0u, {word(rt, 0x08CF68E8u), word(rt, frame.at(4u))});
    release_block(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
}

// 0x08A24C50 (a0 = object): 0x08A237A8 runs, the blocks behind the pointers at +0x68 and +0x64 are released (the first
// with sp + 1 as the release argument, the second with sp), and the two members at +0x34 and +0x10 plus the object go
// through 0x08AB9194 / 0x08AB9194 / 0x08AB9118.
void func_08A24C50(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx, {16u, 17u, 18u});
    call(rt, ctx, 0x08A237A8u, {self});
    release_block(rt, ctx, word(rt, self + 0x68u), frame.at(1u));
    release_block(rt, ctx, word(rt, self + 0x64u), frame.at(0u));
    call(rt, ctx, 0x08AB9194u, {self + 0x34u});
    call(rt, ctx, 0x08AB9194u, {self + 0x10u});
    call(rt, ctx, 0x08AB9118u, {self});
}

// 0x08A9FDA0 (a0 = object, a1 = an optional argument): a null argument comes from the word at +4 of the holder at
// 0x08CF6FDC; 0x08B85EB8(sp + 4, argument, sp + 1) builds a record, 0x08A6D2D4(sp + 8, sp + 4) copies it,
// 0x08A76ABC(a0, sp + 8) assigns it, 0x08A5B9EC(sp + 8) releases it and the block at sp + 4 is released. v0 = a0.
void func_08A9FDA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx, {16u, 17u, 18u});
    if (argument == 0u) argument = word(rt, word(rt, 0x08CF6FDCu) + 4u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), argument, frame.at(1u)});
    call(rt, ctx, 0x08A6D2D4u, {frame.at(8u), frame.at(4u)});
    call(rt, ctx, 0x08A76ABCu, {self, frame.at(8u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    release_block(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
    ctx.gpr[2] = self;
}

// 0x08AD5C34 (a0 = object): the constructor: 0x08AD7CC0 runs, the vtable 0x08CD6EA0 goes at +0, 0x08C3E0BC(+0x48, a0)
// links the object, +0x10C is cleared, two pairs of -1 with their own addresses at +4 / +0x14, 32 zero bytes from +0x64
// with -1 words from +0x84, the float 1.0f at +0x104 and a zero at +0x108. v0 = a0 + 0x80 (the asm's last addiu).
void func_08AD5C34(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx, {16u});
    call(rt, ctx, 0x08AD7CC0u, {self});
    put(rt, self, 0x08CD6EA0u);
    call(rt, ctx, 0x08C3E0BCu, {self + 0x48u, self});
    put8(rt, self + 0x10Cu, 0u);
    for (u32 i = 0u; i < 2u; ++i) {
        put(rt, self + 0x54u + i * 8u, 0xFFFFFFFFu);
        put(rt, self + 0x58u + i * 8u, 0xFFFFFFFFu);
        put(rt, self + 4u + i * 0x10u, self + 0x54u + i * 8u);
    }
    for (u32 i = 0u; i < 0x20u; ++i) {
        put8(rt, self + 0x64u + i, 0u);
        put(rt, self + 0x84u + i * 4u, 0xFFFFFFFFu);
    }
    put(rt, self + 0x104u, 0x3F800000u);
    put(rt, self + 0x108u, 0u);
    ctx.gpr[2] = self + 4u * 0x20u;
}

// 0x08B2AD5C (a0 = a counted table, a1 = the count): 0x3FFFFFFF < ((a1 + 0x1F) >> 5) reports through 0x08B5A9C8; else the
// count and three zeroed words go in, 0x08B62344 allocates (count + 0x1F) >> 5 words, the end goes at +0xC and the base at
// +4 / +8, 0x08C503D4 fills the range with the frame words at sp + 4 / sp, the end moves to +8 and 0x08B2A8E0(a0) runs.
void func_08B2AD5C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx, {16u, 17u, 18u, 19u});
    const u32 groups = (count + 0x1Fu) >> 5;
    const bool too_big = 0x3FFFFFFFu < groups;
    put(rt, self, count);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 4u + 8u, 0u);
    if (too_big) call(rt, ctx, 0x08B5A9C8u, {self});
    const u32 bytes = groups << 2;
    const u32 base = call(rt, ctx, 0x08B62344u, {bytes});
    put(rt, self + 4u + 8u, base + bytes);
    put(rt, self + 4u, base);
    put(rt, self + 8u, base);
    put(rt, frame.at(4u), 0u);
    call(rt, ctx, 0x08C503D4u, {base, groups, frame.at(4u), frame.at(0u)});
    put(rt, self + 8u, base + bytes);
    call(rt, ctx, 0x08B2A8E0u, {self});
}

// 0x08B41FAC (a0 = object with a table at +0x8DC, a1 = a key): 0x08B3F3C8(+0x8DC) answers the first node; each node's word
// at +0 is a record whose field at +0x12 is tested with 0x08B3F9E0(record, key) and 0x08B3F980(record), and the walk
// continues through the node's word at +8. The first hit tail-calls 0x08B41F70(a0, the record's halfword at +0); an
// exhausted walk answers 0.
void func_08B41FAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx, {16u, 17u, 18u, 19u, 20u});
    u32 current = call(rt, ctx, 0x08B3F3C8u, {self + 0x8DCu});
    if (current == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 record = 0u;
    for (;;) {
        record = word(rt, current);
        const u32 field = record + 0x12u;
        if (call(rt, ctx, 0x08B3F9E0u, {field, key}) != 0u) break;
        if (call(rt, ctx, 0x08B3F980u, {field}) != 0u) break;
        current = word(rt, current + 8u);
        if (current == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    tail_call(ctx, 0x08B41F70u, {self, rt.memory().load16(record)});
}

// 0x08B63924 (a0 = object with a word at +8, a length at +0xC and a kind byte at +0x14, a1 = a count): the kind (0xFF
// means no scaling) selects the scale of the count (kind & 7: 0 and 3 by 4, 2 by 2, 4 by 8; 1 and 5..7 report through
// 0x08B77B04, which the original loops on — this returns after the call) and 0x08B636A0(kind, word +8, word +0xC - scale,
// sp) fills the frame word that answers.
void func_08B63924(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 kind = rt.memory().load8(self + 0x14u);
    u32 scale = 0u;
    if (kind != 0xFFu) {
        switch (kind & 7u) {
        case 0u:
        case 3u: scale = count << 2; break;
        case 2u: scale = count << 1; break;
        case 4u: scale = count << 3; break;
        default:
            call(rt, ctx, 0x08B77B04u, {self});
            return;  // the original loops on the report; it never returns
        }
    }
    call(rt, ctx, 0x08B636A0u, {kind, word(rt, self + 8u), word(rt, self + 0xCu) - scale, frame.at(0u)});
    ctx.gpr[2] = word(rt, frame.at(0u));
}

// The guarded construction three instances share (0x08848A48, 0x0884F2C4, 0x08857D64): 0x08A76ACC(a0), the vtable at +0,
// 0x08B62344(size) allocates and `ctor` builds it with (allocation, argument), the allocation goes at +0x14, and the static
// guard byte at `guard` gates 0x08B6441C / the guard word `guard_value` at `slot` / 0x08B64494. `slot` goes at +0x18 and
// v0 is 0x000A0000 when the guard was already set, else the guard call's answer.
void guarded_init(Runtime &rt, AllegrexContext &ctx, u32 argument, u32 vtable, u32 size, u32 ctor, u32 guard, u32 slot,
                  u32 guard_value) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx, {16u, 17u, 18u});
    call(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, vtable);
    const u32 allocated = call(rt, ctx, 0x08B62344u, {size});
    call(rt, ctx, ctor, {allocated, argument});
    put(rt, self + 0x14u, allocated);
    u32 result = 0x000A0000u;  // the asm's leftover `lui v0, 0xA` on the already-initialised path
    if (rt.memory().load8(guard) == 0u) {
        result = call(rt, ctx, 0x08B6441Cu, {guard});
        if (result != 0u) {
            put(rt, slot, guard_value);
            result = call(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, self + 0x18u, slot);
    ctx.gpr[2] = result;
}


// 0x0885DF24 (a0 = object with a member at +0x5B4 and a table at +0x5BC, a1/a2/a3/t0 = the descriptor, t1 = a byte):
// 0x08896CD8(a0 + 0x5BC, a3, a1, t0, t1) creates the node; on success its word +4 is a2, 0x088F9480(a0's +0x5B4) runs,
// 0x0889717C(a3) gates 0x088F8924(node), and the byte at +0x50 is t1.

// 0x0888ACDC (a0 = object): 20, 4, 3 and 4 members of 0xA8 bytes each from +0, +0xD78, +0x1018 and +0x12B8 go through
// 0x0888A86C, the byte at +0x1C34 is cleared, and the object tail-calls 0x0888AB4C with 0.

// 0x088F0A20 (a0 = object): the destructor: vtable 0x08CC3A50, the blocks at +0xDC and +0xD4 released (their counts at
// +0xE0 / +0xD8 cleared), the table at +0xD0 released through its virtual 0, 0x088AFB14(+0xBC) runs and the vtable becomes
// 0x08CC3AA8. v0 = 0x088AFB14's answer.

// 0x089BA420 (a0 = object, a1 = an argument object): when the object's virtual +0x178 answers non-zero, the argument
// object's word +0x17C is called with (a1, the same virtual's answer); the same for virtual +0x16C and the word at
// +0x170, which is the function's tail call.
void func_089BA420(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx, {16u, 17u, 18u});
    if (call_indirect(rt, ctx, word(rt, word(rt, self) + 0x178u), {self}) != 0u) {
        const u32 handler = word(rt, word(rt, other) + 0x17Cu);
        const u32 argument = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x178u), {self});
        call_indirect(rt, ctx, handler, {other, argument});
    }
    if (call_indirect(rt, ctx, word(rt, word(rt, self) + 0x16Cu), {self}) != 0u) {
        const u32 handler = word(rt, word(rt, other) + 0x170u);
        const u32 argument = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x16Cu), {self});
        tail_call(ctx, handler, {other, argument});
    }
}

void func_08A2D930(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, ctx.gpr[5], 0x08CBE6A0u, 0x28u, 0x08B0A0CCu, 0x08D77148u, 0x08D77150u, 0x08CD1E00u);
}

// 0x08A90F2C (a0 = object, a1 = a word stored at +0x34 of the member at +0x30): vtable 0x08CB6C98, the members at +0x14
// and +0x18 / +0x24 (words +0x1C / +0x20 and +0x28 / +0x2C) cleared, 0x08A5BC20(0x3C, 4) and 0x08AB50FC(v0) build a value
// object, 0x08A5BAB8(+0x30, sp) assigns it, and the member at +0x30 (reported through 0x08A5BB7C(0x08CB1E08) when null) gets
// a1 at +0x34.

// 0x08AA0800 (a0 = object, a1/a2/a3/t0 = the source values): 0x08A6D2D4(sp, 0x08CFF348) builds an empty value,
// 0x08AA0690(sp, a2, a3, t0) inserts, and when it answered non-zero and sp holds a value, 0x08A6BFFC reads it and
// 0x08B7C830(a0, value, a1) converts it. v0 = 1 when that happened, else 0; 0x08A5B9EC(sp) releases the value.
void func_08AA0800(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx, {16u, 17u, 18u, 19u, 20u});
    call(rt, ctx, 0x08A6D2D4u, {frame.at(0u), 0x08CFF348u});
    u32 result = 0u;
    if (call(rt, ctx, 0x08AA0690u, {frame.at(0u), second, third, fourth}) != 0u && word(rt, frame.at(0u)) != 0u) {
        const u32 value = call(rt, ctx, 0x08A6BFFCu, {word(rt, frame.at(0u))});
        call(rt, ctx, 0x08B7C830u, {self, value, first});
        result = 1u;
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// 0x08AE10E8 (a0 = object, a1 = a value, f12 = a float): 0x08AEB808(a0) runs, a buffer of ((word 0x08D4069C) * 4 + 0xF)
// rounded down to 16 is carved off the stack, 0x08ADE328(buffer, a0, 0) fills it and 0x08AEB82C(buffer) prepares it; the
// value is clamped to [-0x80, 0x80], 0x08ADFC5C(clamped) converts it, and 0x08AE0F64(buffer, prepared, converted, float)
// is the tail of the work.

// 0x08AE1554: the same shape as 0x08AE10E8 with 0x08ADE4AC filling the buffer.

// 0x08B2C7E8 (a0 = object with a lock at +0x58 and a list between the words at +4 and +8, its first node at +0xC): under
// 0x08B34D40 / 0x08B34D80, the nodes from word +0xC while their address stays in [word +4, word +8) are counted when their
// word at +8 is not -1 and their word at +0xC is not -1, walking through the node's word at +4. v0 = the count.
void func_08B2C7E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx, {16u, 17u});
    call(rt, ctx, 0x08B34D40u, {self + 0x58u});
    u32 count = 0u;
    const u32 begin = word(rt, self + 4u);
    const u32 end = word(rt, self + 8u);
    u32 node = word(rt, self + 0xCu);
    if (node >= begin && node < end) {
        for (;;) {
            if (word(rt, node + 8u) != 0xFFFFFFFFu) count += word(rt, node + 0xCu) != 0xFFFFFFFFu ? 1u : 0u;
            node = word(rt, node + 4u);
            if (node < begin || node >= end) break;
        }
    }
    call(rt, ctx, 0x08B34D80u, {self + 0x58u});
    ctx.gpr[2] = count;
}

// 0x08C49D08 (a0 = a vector of 6-byte records with its end word at +4, a1 = destination, a2 = source): the record count is
// ((end - source) >> 1) * 0xAAAAAAAB (the asm's division by 6); each record's three halfwords are copied down;
// 0x08C470A4(cursor, word +4, sp) builds the range and the end word shrinks by (source - destination) rounded down to 2;
// v0 = the destination.
void func_08C49D08(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx, {16u, 17u, 18u});
    const u32 halved = static_cast<u32>(static_cast<s32>(word(rt, self + 4u) - source) >> 1);
    const s32 count = static_cast<s32>(halved * 0xAAAAAAABu);
    u32 at = destination;
    u32 from = source;
    if (count > 0) {
        for (s32 i = 0; i < count; ++i) {
            for (u32 w = 0u; w < 6u; w += 2u) put16(rt, at + w, rt.memory().load16(from + w));
            at += 6u;
            from += 6u;
        }
    }
    call(rt, ctx, 0x08C470A4u, {at, word(rt, self + 4u), frame.at(0u)});
    put(rt, self + 4u, word(rt, self + 4u) - ((source - destination) & ~1u));
    ctx.gpr[2] = destination;
}

// A counted-range description (two instances: 0x0881888C subtracts the range's ends, 0x08818948 adds them; a0 =
// destination, a1 = a count, a2 = a pointer to the holder pointer): with a positive count the holder (reported through
// 0x08A5BB7C(0x08C98D34) when null) answers the range's end through its virtual +0x3C and the start through 0x08817924,
// and 0x08A85B1C(sp + 4, end +/- start) describes it; otherwise 0x08A76AAC(sp, 0x08CF6ED8) builds an empty description.
// 0x08A5BAD8 assigns it to a0 unless it is its own frame cell and 0x08A5B9EC releases it.
void counted_range(Runtime &rt, AllegrexContext &ctx, bool sum) {
    const u32 out = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 holder_ref = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx, {16u, 17u, 18u});
    u32 cell = frame.at(0u);
    if (count > 0) {
        cell = frame.at(4u);
        const u32 object = word(rt, holder_ref);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
        const u32 end = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
        const u32 begin = call(rt, ctx, 0x08817924u, {object});
        call(rt, ctx, 0x08A85B1Cu, {cell, sum ? end + begin : end - begin});
    } else {
        call(rt, ctx, 0x08A76AACu, {cell, 0x08CF6ED8u});
    }
    if (out != cell) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, cell)});
    call(rt, ctx, 0x08A5B9ECu, {cell});
}

void func_08818948(Runtime &rt, AllegrexContext &ctx) { counted_range(rt, ctx, true); }

// 0x088B42C8 (a0 = object, a1 = a count): the words at +0, +4, +8 and +0xC are cleared with the halfword at +0x10 and the
// bytes at +0x12 / +0x13; a non-zero count sets it at +4 and +0xC and allocates the two tables (a1 bytes and
// ((a1 + 0x1F) >> 5) words) through 0x08B6461C. v0 is the last allocation's answer, 0 when the count is zero.
void func_088B42C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx, {16u, 17u, 18u});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 8u, 0u);
    put16(rt, self + 0x10u, 0u);
    put8(rt, self + 0x12u, 0u);
    put8(rt, self + 0x13u, 0u);
    if (count == 0u) {
        ctx.gpr[2] = 0u;  // the asm's `lw v0, 8(s0)` after the words above were zeroed
        return;
    }
    put(rt, self + 4u, count);
    put(rt, self, call(rt, ctx, 0x08B6461Cu, {count}));
    put(rt, self + 0xCu, count);
    put(rt, self + 8u, call(rt, ctx, 0x08B6461Cu, {((count + 0x1Fu) >> 5) << 2}));
}

// 0x088D7158 (a0 = object, a1 = a byte): two precondition checks, the byte `a1 ^ 1` written at +0x13 of the word at +0x130
// (after 0x088E4120 on the member at +0x640 when the byte is 0), then either the 0x088D6FD4 path (which sets +0x1F) or
// 0x088DDB6C; v0 = v1 = 1 on both, 0 when a check fails.
void func_088D7158(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx, {16u, 17u});
    if (call(rt, ctx, 0x088D9E58u, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    if (call(rt, ctx, 0x088DA0BCu, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    const u32 inner = word(rt, self + 0x640u);
    if (inner != 0u && flag == 0u) call(rt, ctx, 0x088E4120u, {inner});
    const u32 holder = word(rt, self + 0x130u);
    put8(rt, holder + 0x13u, flag ^ 1u);
    if (call(rt, ctx, 0x088DAA84u, {self}) != 0u) {
        call(rt, ctx, 0x088D6FD4u, {self, 0u, 0u, 0xFFFFFFFFu});
        put8(rt, self + 0x1Fu, 1u);
    } else {
        call(rt, ctx, 0x088DDB6Cu, {self});
    }
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = 1u;
}

// Divide the field at +0x14 of a0 by a virtual getter's answer (two instances: 0x08A737EC reads the signed halfword and
// slot 0x38, 0x08A96560 the signed byte and slot 0x34). a3 points at the holder pointer; a null holder is reported through
// 0x08A5BB7C(guard_text) and read anyway. a1's remainder is built by 0x08A85B1C(sp, value), assigned to a1 (a1 = b) unless
// a1 is that cell, and released.
enum class Field { SignedHalf, SignedByte };

void divide_through(Runtime &rt, AllegrexContext &ctx, u32 vtable_offset, u32 guard_text, Field kind) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 holder_ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx, {16u, 17u, 18u, 19u});
    u32 holder = word(rt, holder_ref);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    call_slot(rt, ctx, holder, vtable_offset, {holder});
    holder = word(rt, holder_ref);
    const u32 dividend = kind == Field::SignedHalf ? sext16(rt.memory().load16(self + 0x14u))
                                                   : sext8(rt.memory().load8(self + 0x14u));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {guard_text});
    const u32 divisor = call_slot(rt, ctx, holder, vtable_offset, {holder}).v0;
    const u32 value = signed_remainder(static_cast<s32>(dividend), static_cast<s32>(divisor));
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), value});
    if (out != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

void func_08A737EC(Runtime &rt, AllegrexContext &ctx) {
    divide_through(rt, ctx, 0x38u, 0x08CB0BA8u, Field::SignedHalf);
}
void func_08A96560(Runtime &rt, AllegrexContext &ctx) {
    divide_through(rt, ctx, 0x34u, 0x08CB21E8u, Field::SignedByte);
}

// 0x08A9C594 (a0 = object with a container at +0x4C and a member pointer at +0x78, a1 = a value): under
// 0x08B344E0 / 0x08B3457C, the member at +0x78 (reported through 0x08A5BB7C(0x08CB25E0) when null) plus 0x14 holds a
// vector whose words at +4 / +8 are compared: at the end 0x08B8A03C(member, first, sp) appends, else 0x08A76ABC moves the
// cell and the cursor at +4 advances. 0x08A6D2D4(sp, a1) builds the value and 0x08A5B9EC(sp) releases it.

} // namespace
} // namespace new_batch_main_1587
} // namespace gt::game

#define NEW1587(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1587::name, gt::ReturnKind::kind)
NEW1587(0x08818948u, func_08818948, Word);
NEW1587(0x088B42C8u, func_088B42C8, Word);
NEW1587(0x088D7158u, func_088D7158, Word);
NEW1587(0x089BA420u, func_089BA420, Word);
NEW1587(0x089CF190u, func_089CF190, Word);
NEW1587(0x08A1F520u, func_08A1F520, Word);
NEW1587(0x08A24C50u, func_08A24C50, Word);
NEW1587(0x08A2D930u, func_08A2D930, Word);
NEW1587(0x08A737ECu, func_08A737EC, Word);
NEW1587(0x08A96560u, func_08A96560, Word);
NEW1587(0x08A9FDA0u, func_08A9FDA0, Word);
NEW1587(0x08AA0800u, func_08AA0800, Word);
NEW1587(0x08AD5C34u, func_08AD5C34, Word);
NEW1587(0x08B2AD5Cu, func_08B2AD5C, Word);
NEW1587(0x08B2C7E8u, func_08B2C7E8, Word);
NEW1587(0x08B41FACu, func_08B41FAC, Word);
NEW1587(0x08B63924u, func_08B63924, Word);
NEW1587(0x08C49D08u, func_08C49D08, Word);
