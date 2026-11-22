// New batch, main session 22: functions the recompiler emitted that no file had decompiled yet, written by hand as
// structured C from `tools.disasm`. Draft only: the differential gate (fast, sabotage, synthetic) is run by the
// verifier, not here.
//
// Conventions, as in hot_batch_102: every `call_set` / `call_setf` writes the argument registers the original sets
// before the `jal` (a0-a3, t0-t3, f12-f15); the ones it leaves alone keep the caller's value. The delay slot of a
// `jal` runs BEFORE the call; a `bnel` / `beql` / `bc1fl` delay slot only runs when the branch is taken; a plain
// branch's delay slot runs on both paths. `v0` (and `v1` / `f0` where a caller reads them) is set as the original
// leaves it on every exit path. The dead stack is not reproduced; the frames callees read or write are
// (`GuestFrame`, size = the original's `addiu sp, sp, -N`).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_22 {
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
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The argument registers set as the original sets them: a0-a3, t0-t3 in order.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
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
// A virtual call as the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then `jalr v0`.
inline GuestResult call_virtual_full(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                                     std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}
// `lw v0, 0(object); lw v1, off(v0); jalr v1`: v0 = the vtable, v1 = the slot's function (the shape where the
// callee's address goes in v1, not v0).
inline GuestResult call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                             std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    ctx.gpr[3] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}

inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
inline u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
// The value `lui` leaves for an address built by a relocated lui/addiu pair: the high half, rounded up when the low
// half is negative.
inline constexpr u32 lui_of(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline u32 swapped(u32 value) { return (value >> 24) | ((value >> 8) & 0xFF00u) | ((value << 8) & 0xFF0000u) | (value << 24); }
// `trunc.w.s`, exactly as the recompiled original converts (saturating, NaN -> INT_MAX).
inline u32 trunc_word(AllegrexContext &ctx, float value) { return ctx.fpu_float_to_word_ct<1u>(value); }
// `div`: what PSP's divider leaves in lo / hi (a zero divisor gives lo = -1 or 1 by the dividend's sign, hi = it).
inline s32 divide_quotient(s32 dividend, s32 divisor) {
    if (divisor == 0) return dividend < 0 ? 1 : -1;
    return dividend / divisor;
}
inline s32 divide_remainder(s32 dividend, s32 divisor) {
    if (divisor == 0) return dividend;
    return dividend % divisor;
}

// ---- functions ----

// 0x08890068 (a0 = the address of the module header word): applies the load delta (the header's address minus its
// byte-swapped link address at +4) to the pointer fields of the module header and to the three section-record tables
// it points at, then records the header's new address. Returns 1 when there was work (a non-zero delta) and 0 when
// the module sits where it was linked. Every pointer in these tables is stored byte-swapped.
u32 swap_relocate(Runtime &rt, u32 address, u32 delta) {
    const u32 value = swapped(word(rt, address)) + delta;
    put(rt, address, swapped(value));
    return value;
}

// 0x088C7D50 (a0 = the mesh state, a1 = an object handed to 0x08AF26F0 unchanged in a0 too, f12 = a ratio): draws the
// state's vertex list through the graphics library. f12 * 255 becomes the alpha byte of a colour pair at sp + 0x10 /
// sp + 0x14 (the pair is 0xFFFF5E20 / 0xFFFFFFFF once the byte no longer fits); then, for each of the state's
// + 0xACC vertices, one 0xC-byte entry from the list at state + 0x254 is turned into the scratch block at sp by
// 0x088C7920, 0x08B5A7F0 writes three floats into it, and a 0x10-byte record (the colour word, then those three
// floats) goes to the buffer 0x08B56A00 returned. The colour flips for the first vertex of every fourth one (the
// column counter at + 0xAC4), whose colour word is white. v0 = the last graphics-library call's result.
void func_088C7D50(Runtime &rt, AllegrexContext &ctx) {
    const u32 state = ctx.gpr[4];
    const u32 passed = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const float ratio = ctx.fpr[12];
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = frame.at(0u);
    call_set(rt, ctx, 0x08AF26F0u, {passed, passed, a2, a3});
    const float two_to_31 = std::bit_cast<float>(0x4F000000u);
    const float scaled = psp_mul(ratio, 255.0f);
    u32 alpha = trunc_word(ctx, scaled);
    if (two_to_31 <= scaled) alpha = trunc_word(ctx, scaled - two_to_31) | 0x80000000u;
    if (alpha < 0x100u) {
        put(rt, sp + 0x14u, (alpha << 24) | 0xFFFFu);
        put(rt, sp + 0x10u, (alpha << 24) | 0x5E20u);
    } else {
        put(rt, sp + 0x10u, 0xFFFF5E20u);
        put(rt, sp + 0x14u, 0xFFFFFFFFu);
    }
    const u32 count = word(rt, state + 0xACCu);
    put(rt, sp + 0x18u, call_set(rt, ctx, 0x08B550E0u, {count << 4}));
    call_set(rt, ctx, 0x08B56A00u, {0u});
    u32 cursor = word(rt, sp + 0x18u);
    if (static_cast<s32>(count) > 0) {
        u32 column = 0u;
        u32 alternate = 0u;
        u32 entry = state + 0x254u;
        for (u32 vertex = 0; vertex != count; ++vertex) {
            const u32 stride = word(rt, state + 0xAC4u);
            const u32 remainder =
                static_cast<u32>(divide_remainder(static_cast<s32>(column), static_cast<s32>(stride + 2u)));
            u32 colour = word(rt, sp + 0x10u);
            if ((alternate & 1u) == 0u) colour = word(rt, sp + 0x14u);
            const bool highlight = remainder == stride + 1u;
            if (highlight) colour = (0xFFu << 16) | 0xFFFFu;
            const u32 record = entry;
            entry += 0xCu;
            call_set(rt, ctx, 0x088C7920u, {record, sp});
            call_set(rt, ctx, 0x08B5A7F0u, {sp, sp});
            put(rt, cursor, colour);
            put_float(rt, cursor + 4u, float_at(rt, sp));
            put_float(rt, cursor + 8u, float_at(rt, sp + 4u));
            put_float(rt, cursor + 0xCu, float_at(rt, sp + 8u));
            cursor += 0x10u;
            if ((vertex % 4u) == 3u) {
                alternate += highlight ? 0u : 1u;
                column += 1u;
            }
        }
    }
    call_set(rt, ctx, 0x08B569A0u, {});
    call_set(rt, ctx, 0x08B56510u, {});
    call_set(rt, ctx, 0x08B555F8u, {word(rt, sp + 0x18u)});
    call_set(rt, ctx, 0x08B556A0u, {0u, 0u, 0x103u, 0u, 3u, 0u});
    call_set(rt, ctx, 0x08B55588u, {4u, count});
    call_set(rt, ctx, 0x08B56940u, {});
}

// 0x08A0F5D4 (a0 = a mode, a1 = a selector): an initialisation pair. mode 1 with selector 0xFFFF runs the big init
// (0x08A92114 with six arguments, 0x08B28DCC, 0x08B2095C, 0x0897D534 and six statistic strings formatted by
// 0x08C14850 from the 64-bit constants at 0x08C8E110..0x08C8E138); mode 0 with 0xFFFF ends in a tail call of
// 0x08B28DC4 with the state 0x08CF61B0; every other combination returns 0xFFFF.
void func_08A0F5D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[4];
    const u32 selector = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0u);
    ctx.gpr[2] = 1u;
    if (mode == 1u) {
        if (selector != 0xFFFFu) {
            ctx.gpr[2] = 0xFFFFu;
            return;
        }
        call_set(rt, ctx, 0x08A92114u, {0x08CF612Cu, 0x08CA96B4u, 0x78u, 0x08A0F050u, 0x08A11700u, 0x08A0F074u});
        call_set(rt, ctx, 0x08B28DCCu, {0x08CF61B0u});
        call_set(rt, ctx, 0x08B2095Cu, {0x08CF61A8u, 0x08C8E004u, 0x10u});
        call_set(rt, ctx, 0x0897D534u, {0x08CF611Cu, 0x08CA96B4u, 0x08A101D8u, 0x08A10194u});
        static constexpr u32 kValues[] = {0x08C8E138u, 0x08C8E130u, 0x08C8E128u,
                                          0x08C8E120u, 0x08C8E118u, 0x08C8E110u};
        static constexpr u32 kDestinations[] = {0x08CF6198u, 0x08CF6188u, 0x08CF6178u,
                                                0x08CF6168u, 0x08CF6158u, 0x08CF6148u};
        static constexpr u32 kFormats[] = {0x08CA96BCu, 0x08CA96CCu, 0x08CA96D4u,
                                           0x08CA9604u, 0x08CA96E4u, 0x08CA96FCu};
        static constexpr u32 kSlots[] = {0x28u, 0x20u, 0x18u, 0x10u, 0x8u, 0x0u};
        for (unsigned i = 0; i < 6u; ++i) {
            put(rt, sp + kSlots[i], word(rt, kValues[i]));
            put(rt, sp + kSlots[i] + 4u, word(rt, kValues[i] + 4u));
            call_set(rt, ctx, 0x08C14850u, {kDestinations[i], kFormats[i], sp + kSlots[i], 0x08CF611Cu});
        }
        return;
    }
    if (mode == 0u && selector == 0xFFFFu) {
        ctx.gpr[2] = 0xFFFFu;
        tail_call(ctx, 0x08B28DC4u, {0x08CF61B0u});
        return;
    }
    ctx.gpr[2] = 0xFFFFu;
}

// 0x08ACD5D4 (a0 = the owner, a1 = the object): a state-machine step of a loader: while the flag byte at object + 0xE8
// is set, it locks the object's mutex (0x08B344E0 / 0x08B3457C around the work), asks 0x08C7ADE8 for a status from
// the words at + 0xB4 / + 0xBC / + 0xC0, and either appends a 0xC-byte record to the list its arithmetic on the words
// at + 4 / + 0x10 / + 0x14 addresses (0x08B79A88 sizes the buffer), waits (0x08B2E2B8) and repeats, or clears the
// flag and returns. Status 0x80618001 with the owner's byte at + 0x304 set raises the hurry flag at + 0xE1 and waits
// 10000, otherwise it waits 1000 or runs 0x08ACD4A8. v0 = the last callee's result.
void func_08ACD5D4(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 mutex = object + 0x84u;
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = frame.at(0u);
    for (;;) {
        call_set(rt, ctx, 0x08B344E0u, {mutex});
        if (byte_u(rt, object + 0xE8u) == 0u) {
            call_set(rt, ctx, 0x08B3457Cu, {mutex});
            return;
        }
        call_set(rt, ctx, 0x08B3457Cu, {mutex});
        const u32 inner = word(rt, object + 0xB0u) + 0x164u;
        call_set(rt, ctx, 0x08B344E0u, {inner});
        const u32 hurry = byte_u(rt, owner + 0x304u) & 0xFFu;
        const u32 status = call_set(rt, ctx, 0x08C7ADE8u,
                                    {word(rt, object + 0xB4u), word(rt, object + 0xC0u), word(rt, object + 0xBCu), sp});
        call_set(rt, ctx, 0x08B3457Cu, {inner});
        if (status != 0u) {
            if (status != 0x80618001u) {
                put8(rt, object + 0xE8u, 0u);
                return;
            }
            if (hurry != 0u) {
                call_set(rt, ctx, 0x08B2E2B8u, {0x2710u});
                put8(rt, object + 0xE1u, 1u);
            } else {
                call_set(rt, ctx, 0x08ACD4A8u, {owner, object});
            }
            continue;
        }
        const u32 list = word(rt, object + 0xBCu);
        u32 record = word(rt, list + 4u);
        const u32 extra = word(rt, object + 0xDCu);
        if (record == 0xFFFFFFFFu) record = word(rt, object + 0xD4u) + 0xBBBu;
        put(rt, object + 0xD4u, record);
        if (call_set(rt, ctx, 0x08ACCD34u, {owner, object, extra, sp + 4u}) != 0u) {
            put8(rt, object + 0xE8u, 0u);
            return;
        }
        if (word(rt, sp + 4u) == 0u) {
            call_set(rt, ctx, 0x08B2E2B8u, {0x7D0u});
            continue;
        }
        call_set(rt, ctx, 0x08B34888u, {object});
        u32 table = word(rt, object + 0xACu);
        if (word(rt, table + 0x18u) == word(rt, table + 0xCu)) {
            call_set(rt, ctx, 0x08B34B2Cu, {object});
            call_set(rt, ctx, 0x08B344E0u, {mutex});
            if (byte_u(rt, object + 0xE8u) == 0u) {
                call_set(rt, ctx, 0x08B3457Cu, {mutex});
                call_set(rt, ctx, 0x08B34B24u, {object});
                return;
            }
            call_set(rt, ctx, 0x08B3457Cu, {mutex});
            table = word(rt, object + 0xACu);
        }
        const u32 slot = word(rt, table + 0x10u) + word(rt, table + 4u) * word(rt, table + 0x14u);
        const u32 stride = (word(rt, owner + 0x518u) << 17) + (word(rt, owner + 0x518u) << 13);
        call_set(rt, ctx, 0x08B79A88u, {word(rt, slot), word(rt, object + 0xDCu), stride});
        put(rt, slot + 8u, record);
        put(rt, slot + 0xCu, word(rt, object + 0xD8u));
        put(rt, object + 0xD8u, word(rt, object + 0xD8u) + 1u);
        call_set(rt, ctx, 0x08B24A8Cu, {word(rt, object + 0xACu), 1u});
        call_set(rt, ctx, 0x08B34B24u, {object});
        call_set(rt, ctx, 0x08B2E2B8u, {0x7D0u});
    }
}

// 0x0887B768 (a0 = the pool, a1 = an argument for 0x088A5820): takes the first free bit of the 0x18-slot bitmap at
// + 0x28 (0x18 when all are used), builds a slot record (0x38 bytes each, the word at + 0x60 is the handle
// 0x088A26E0 returns) and, when the slot is real, sets its bit, bumps the 24-bit sequence at + 0x24 and hands the
// record to 0x0887B40C, two constant 3-word blocks to the virtual at vtable + 0x24, a 7-argument call to the virtual
// at + 0x30 and a float to 0x088A1854. v0 = v1 = the slot index (or -1).
void func_0887B768(Runtime &rt, AllegrexContext &ctx) {
    const u32 pool = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const u32 sp = frame.at(0u);
    const u32 count = word(rt, pool + 0xCu);
    if (static_cast<s32>(count) <= 0) {
        ctx.gpr[3] = 0xFFFFFFFFu;
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 bitmap = pool + 0x28u;
    u32 index = 0u;
    u32 slot;
    for (;;) {
        if (index == 0x18u) {
            slot = 0x18u;
            break;
        }
        if (((word(rt, bitmap + ((index >> 5) * 4u)) >> (index & 31u)) & 1u) == 0u) {
            slot = index;
            break;
        }
        index += 1u;
        if (index == count) {
            ctx.gpr[3] = 0xFFFFFFFFu;
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
    }
    const u32 scratch = sp + 0xCu;
    call_set(rt, ctx, 0x088A79ACu, {scratch, 0u});
    call_set(rt, ctx, 0x088A5820u, {scratch, argument});
    put(rt, sp, 0u);
    put8(rt, sp + 8u, 0u);
    put8(rt, sp + 9u, 1u);
    put(rt, sp + 4u, scratch);
    const u32 handle = call_set(rt, ctx, 0x088A26E0u, {word(rt, pool), slot, sp});
    const u32 record = pool + slot * 0x38u;
    put(rt, record + 0x60u, handle);
    call_set(rt, ctx, 0x088A7548u, {scratch});
    if (word(rt, record + 0x60u) == 0u) {
        ctx.gpr[3] = 0xFFFFFFFFu;
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    u32 field = word(rt, pool + 0x24u);
    if (slot < 0x18u) {
        const u32 cell = bitmap + ((slot >> 5) * 4u);
        put(rt, cell, word(rt, cell) | (1u << (slot & 31u)));
        field = word(rt, pool + 0x24u);
    }
    const u32 kept = (1u << 30u) - 1u;
    const u32 bumped = static_cast<u32>((static_cast<s32>(field << 2) >> 2) + 1);
    put(rt, pool + 0x24u, (field & ~kept) | (bumped & kept));
    call_set(rt, ctx, 0x0887B40Cu, {record + 0x2Cu, 0u, 0u});
    put(rt, sp, word(rt, 0x08C89830u));
    put(rt, sp + 4u, word(rt, 0x08C89830u + 4u));
    put(rt, sp + 8u, word(rt, 0x08C89830u + 8u));
    put(rt, sp + 0xCu, word(rt, 0x08C89824u));
    put(rt, sp + 0x10u, word(rt, 0x08C89824u + 4u));
    put(rt, sp + 0x14u, word(rt, 0x08C89824u + 8u));
    const u32 owner = word(rt, pool);
    call_slot(rt, ctx, owner, 0x24u, {owner, slot, word(rt, 0x08C89830u), owner});
    call_slot(rt, ctx, owner, 0x30u, {owner, slot, sp, scratch, 0u, 0u, 0u, word(rt, pool + 8u)});
    call_setf(rt, ctx, 0x088A1854u, {word(rt, pool), slot, sp}, {float_at(rt, sp + 0x10u)});
    ctx.gpr[3] = index;
    ctx.gpr[2] = index;
}

// A 4x3 float matrix (0x3C bytes) copied word by word.
inline void copy_block(Runtime &rt, u32 destination, u32 source) {
    for (u32 i = 0; i < 0x3Cu / 4u; ++i) put(rt, destination + i * 4u, word(rt, source + i * 4u));
}

// 0x088FE08C (a0 = the out object, a1 = the source, a2 = a matrix or 0, a3 = another matrix): composes a 4x3 matrix
// in the frame: 0x08B0CE58 initialises it at sp + 0x80, 0x088FDF98 initialises the out object, then either
// 0x08B10170 builds sp + 0xC0 from a1 / a2 / a3 (flag 1) or 0x08B10204 builds sp + 0x100 from a1 (flag 1) — which
// also stores the word at sp + 0xA0 into out + 4 — and the result is copied through a second scratch block to
// sp + 0x80. When the two words at sp + 0x90 / sp + 0x94 are both zero, out's first word is cleared, otherwise it gets
// the word at sp + 0xA0; a non-zero first word is also passed to 0x088FDFE0. v0 = out's first word.
void func_088FE08C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x160u);
    const u32 sp = frame.at(0u);
    call_set(rt, ctx, 0x08B0CE58u, {sp + 0x80u});
    call_set(rt, ctx, 0x088FDF98u, {out});
    if (a2 != 0u) {
        call_set(rt, ctx, 0x08B10170u, {sp + 0xC0u, source, a2, a3, 1u});
        copy_block(rt, sp + 0x40u, sp + 0xC0u);
        copy_block(rt, sp + 0x80u, sp + 0x40u);
    } else {
        call_set(rt, ctx, 0x08B10204u, {sp + 0x100u, source, 1u});
        copy_block(rt, sp, sp + 0x100u);
        copy_block(rt, sp + 0x80u, sp);
        put(rt, out + 4u, word(rt, sp + 0xA0u));
    }
    if ((word(rt, sp + 0x90u) | word(rt, sp + 0x94u)) == 0u) put(rt, out, 0u);
    else put(rt, out, word(rt, sp + 0xA0u));
    if (word(rt, out) != 0u) call_set(rt, ctx, 0x088FDFE0u, {word(rt, out)});
    ctx.gpr[2] = word(rt, out);
}

// 0x08924690 (a0 = the state, a1 = the destination): refreshes the state's animation angle and pushes two 4x4
// transforms into the destination. With the flag byte at + 0x35 clear it only runs 0x08921584(state). Otherwise it
// resets the destination's scale / translation words, advances the angle at + 0x10 by 0.005 wrapped into [-pi, pi],
// stores -0.396, 0.5, 3.8 and 58 at + 0xC / + 0x20 / + 0x2C / + 0x30, and sends two matrices (built in the frame,
// 0x08B4E9D4 each; scales and offsets from the state, then three axis setters 0x08B4F3B0 / 0x08B4F2E4 / 0x08B4F47C)
// to the destination; the state's word at + 0x30 ends at destination + 0x70. v0 = the last call's result.
void func_08924690(Runtime &rt, AllegrexContext &ctx) {
    const u32 state = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const u32 sp = frame.at(0u);
    const float one = 1.0f;
    const float two_pi = std::bit_cast<float>(0x40C90FDBu);
    const float pi = std::bit_cast<float>(0x40490FDBu);
    if (byte_u(rt, state + 0x35u) == 0u) {
        call_set(rt, ctx, 0x08921584u, {state});
    } else {
        put_float(rt, out + 0x28u, one);
        put_float(rt, out, one);
        put_float(rt, out + 0x14u, one);
        put(rt, out + 0x10u, 0u);
        put(rt, out + 0x20u, 0u);
        put(rt, out + 4u, 0u);
        put(rt, out + 0x24u, 0u);
        put(rt, out + 8u, 0u);
        put(rt, out + 0x18u, 0u);
        const float advanced = float_at(rt, state + 0x10u) + std::bit_cast<float>(0x3BA3D70Au);
        put_float(rt, state + 0x10u, advanced);
        if (pi < advanced) put_float(rt, state + 0x10u, advanced - two_pi);
        else if (advanced < -pi) put_float(rt, state + 0x10u, advanced + two_pi);
        put_float(rt, state + 0xCu, std::bit_cast<float>(0xBECAD962u));
        put_float(rt, state + 0x20u, std::bit_cast<float>(0x3F000000u));
        put_float(rt, state + 0x2Cu, std::bit_cast<float>(0x40733333u));
        put_float(rt, state + 0x30u, std::bit_cast<float>(0x42680000u));
    }
    put_float(rt, sp + 0x34u, std::bit_cast<float>(0x3F19999Au));
    put_float(rt, sp, one);
    put_float(rt, sp + 0x14u, one);
    put_float(rt, sp + 0x28u, one);
    put_float(rt, sp + 0x3Cu, one);
    for (const u32 offset : {0x4u, 0x8u, 0xCu, 0x10u, 0x18u, 0x1Cu, 0x20u, 0x24u, 0x2Cu, 0x30u, 0x38u}) put(rt, sp + offset, 0u);
    call_set(rt, ctx, 0x08B4E9D4u, {out, sp});
    call_setf(rt, ctx, 0x08B4F3B0u, {out}, {float_at(rt, state + 0x10u)});
    call_setf(rt, ctx, 0x08B4F2E4u, {out}, {float_at(rt, state + 0xCu)});
    call_setf(rt, ctx, 0x08B4F47Cu, {out}, {float_at(rt, state + 0x14u)});
    put_float(rt, sp + 0x30u, float_at(rt, state + 0x1Cu));
    put_float(rt, sp + 0x3Cu, one);
    put_float(rt, sp, one);
    put_float(rt, sp + 0x14u, one);
    put_float(rt, sp + 0x28u, one);
    put_float(rt, sp + 0x34u, float_at(rt, state + 0x20u));
    put_float(rt, sp + 0x38u, float_at(rt, state + 0x2Cu));
    for (const u32 offset : {0x4u, 0x8u, 0xCu, 0x10u, 0x18u, 0x1Cu, 0x20u, 0x24u, 0x2Cu}) put(rt, sp + offset, 0u);
    call_set(rt, ctx, 0x08B4E9D4u, {out, sp});
    put_float(rt, out + 0x70u, float_at(rt, state + 0x30u));
}

// The setters of func_0892AB78: the value written ends in v0 (and v1 for the byte-insert ones).
inline void or_byte(Runtime &rt, AllegrexContext &ctx, u32 address, u32 bits) {
    const u32 value = byte_u(rt, address) | bits;
    put8(rt, address, value);
    ctx.gpr[2] = value;
}
inline void or_word(Runtime &rt, AllegrexContext &ctx, u32 address, u32 bits) {
    const u32 value = word(rt, address) | bits;
    put(rt, address, value);
    ctx.gpr[2] = value;
}

// 0x0892AB78 (a0 = the object, a1 = a selector 0..0x2C, a2 = a flag): the object's setter switch. When a2 is set and
// the selector is in range, the 45-entry table at 0x08C8A8E0 picks one small setter: a bit of the flag word at
// + 0x20 / + 0x24 / + 0x28 / + 0x38, a byte flag at + 7, a halfword at + 8 / + 0xA / + 0xC / + 0xE, a signed byte at + 2,
// or a bit of the byte at + 0 together with the 16-bit counter at + 4. v0 = the value written; the no-op entries leave
// v0 alone.

// 0x0899FAB0 (a0 = the owner, a1 = an argument): rebuilds the owner's list through 0x08A36C3C when 0x0899D044 and
// 0x0899D3CC both accept it (two orders of the three 0x0896A9C4 / 0x0896A35C list steps, chosen by 0x0899D3CC), then
// walks the list: a missing head logs through 0x08A5BB7C, 0x08A34DA8 and 0x08A5B9EC close it, the flags at + 0x13C
// get bit 3 set and bit 4 cleared, and when the object at + 0x160 exists the argument goes through 0x08A76AAC and that
// object's virtual at + 0xCC receives the two temporary holders. Then 0x089AA910's result sets flag bit 2, and
// 0x089AF5E8 / 0x089AB114 finish with the word at + 0x154; the float at + 0x120 is copied to + 0x150.

// 0x08B144F0 (a0 = the object): checks the 0x40-byte block the object's virtual at vtable + 0x24 identifies. The
// virtual's two words seed 0x08B136A8 (which fills sp + 8); the name from 0x08B1262C is looked up by 0x08B23A70 and
// 0x08C7A848 returns a status and a length, then 0x08B33F44 reads the block into a 64-byte aligned frame buffer. The
// block's 0x40 bytes are folded with 0xCC, 0x08B13DC8 runs and a CRC-32 with the table at 0x08CD7EC8 must come out
// zero; the first 16 bytes must equal 0x08B136A8's output, the next 16 the comparison at sp + 0x18, and the two words
// at + 0x20 / + 0x24 must match sp + 0x28 / sp + 0x2C, as must the virtual's two words + 0x28 / + 0x2C. Returns 1 when
// every check holds, 0 otherwise.

// 0x08C52070 (a0 = first, a1 = last of a list range): a list copy-insert: walks the destination nodes from *a0 and the
// source nodes from *a1, copying each source node's value (a byte key at + 8, two words at + 0xC / + 0x10 and the
// buffer at + 0x14 with its length at + 0x18, allocated by 0x08B6461C and released by 0x08B61FC0) into the destination
// node while the two node keys differ. When the source runs into a1 the remaining nodes are freshly allocated
// (0x08B35DA0 with 0x1C bytes) and 0x08B5C078 links each after a0; when the destination runs into a0 the rest is
// erased node by node with 0x08C5201C. v0 = a0.

// 0x088DBF0C (a0 = the object): the object's step machine, keyed by the word at + 0x5C4 (six states in the table at
// 0x08C89EC8) and gated by the flag byte at + 0x5BF. State 0 clears the inner object's counters and ends with a tail
// call of 0x088E4380 with the word at + 0x640; 1 waits on the 64-bit deadline the pair 0x088E48A4 / 0x088E48E8 returns
// against 0x088D9C70 (state 2 at 1000 ticks); 2 runs 0x088D7158 and moves to 3; 3 gates on 0x088DA0BC / 0x088DA3EC,
// runs 0x088DB4A8 with 3 and stores a 1500-tick deadline at the inner object's + 0x28 (state 4); 4 sets the inner byte
// + 0x18 and runs 0x088D78EC (state 5); 5 gates again, runs 0x088DB4A8 with 1 and clears the state unless the inner byte
// at + 0x1A is 3.
void func_088DBF0C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (byte_u(rt, object + 0x5BFu) == 0u) return;
    const u32 state = word(rt, object + 0x5C4u);
    if (state >= 6u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const auto back = [&] { ctx.gpr[2] = 0u; };
    switch (state) {
    case 0u: {
        const u32 inner = word(rt, object + 0x130u);
        put8(rt, object + 0x5BFu, 0u);
        put8(rt, inner + 0x18u, 0u);
        put(rt, inner + 0x28u, 0u);
        put(rt, inner + 0x2Cu, 0u);
        put8(rt, word(rt, object + 0x130u) + 0x13u, 0u);
        tail_call(ctx, 0x088E4380u, {word(rt, object + 0x640u), 0u});
        return;
    }
    case 1u: {
        const u32 inner = word(rt, object + 0x130u);
        if (byte_u(rt, inner + 0x19u) == 0u) return back();
        if (call_set(rt, ctx, 0x088E48A4u, {word(rt, object + 0x640u)}) == 0u) {
            put(rt, object + 0x5C4u, 2u);
            ctx.gpr[2] = 2u;
            return;
        }
        const GuestResult deadline = call_setf(rt, ctx, 0x088E48E8u, {word(rt, object + 0x640u)}, {});
        const u32 low = deadline.v0;
        const u32 high = deadline.v1;
        if ((low | high) == 0u) return back();
        const GuestResult now = call_setf(rt, ctx, 0x088D9C70u, {object}, {});
        const u32 carried = low + 0x3E8u;
        const u32 limit_high = (carried < low ? 1u : 0u) + high;
        if (limit_high < now.v1) {
            put(rt, object + 0x5C4u, 2u);
            ctx.gpr[2] = 2u;
            return;
        }
        if (limit_high != now.v1) return back();
        if (carried >= now.v0) return back();
        put(rt, object + 0x5C4u, 2u);
        ctx.gpr[2] = 2u;
        return;
    }
    case 2u: {
        call_set(rt, ctx, 0x088D7158u, {object, 0u});
        put(rt, object + 0x5C4u, 3u);
        return;
    }
    case 3u: {
        if (call_set(rt, ctx, 0x088DA0BCu, {object}) != 0u) return;
        if (call_set(rt, ctx, 0x088DA3ECu, {object}) == 0u) return back();
        call_set(rt, ctx, 0x088DB4A8u, {object, 3u});
        const u32 inner = word(rt, object + 0x130u);
        if (byte_u(rt, inner + 0x1Au) != 3u) {
            ctx.gpr[2] = 3u;
            return;
        }
        const GuestResult deadline = call_setf(rt, ctx, 0x088D9C70u, {object}, {});
        const u32 carried = deadline.v0 + 0x5DCu;
        put(rt, inner + 0x28u, carried);
        put(rt, inner + 0x2Cu, (carried < deadline.v0 ? 1u : 0u) + deadline.v1);
        put(rt, object + 0x5C4u, 4u);
        ctx.gpr[2] = 4u;
        return;
    }
    case 4u: {
        const u32 inner = word(rt, object + 0x130u);
        put8(rt, inner + 0x18u, 1u);
        const u32 ok = call_set(rt, ctx, 0x088D78ECu, {object, 0u, 0u});
        if (ok != 0u) put(rt, object + 0x5C4u, 5u);
        ctx.gpr[2] = 5u;
        return;
    }
    default: {
        if (call_set(rt, ctx, 0x088DA0BCu, {object}) == 0u) return;
        if (call_set(rt, ctx, 0x088DA3ECu, {object}) == 0u) return back();
        call_set(rt, ctx, 0x088DB4A8u, {object, 1u});
        const u32 inner = word(rt, object + 0x130u);
        if (byte_u(rt, inner + 0x1Au) != 3u) put(rt, object + 0x5C4u, 0u);
        ctx.gpr[2] = inner;
        return;
    }
    }
}

// 0x08AF5830 (a0 = the out matrix, a1 = a position, a2 = the source vector, a3 = the axis vector or 0, t0 = an axis
// order 0..5): builds a translation matrix from a1 at sp + 0x50 (0x08B4E9D4) and the second one at sp + 0x10 from a2
// through 0x08B4B114, then sends the second to the out matrix. With a3 and an order under 6, the three axis setters
// 0x08B4F2E4 (+0), 0x08B4F3B0 (+4) and 0x08B4F47C (+8) follow in that order's permutation. v0 = the last call's result
// (or whether the order was in range when a3 is 0).
void func_08AF5830(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 vector = ctx.gpr[6];
    const u32 axis = ctx.gpr[7];
    const u32 order = ctx.gpr[8];
    GuestFrame frame(ctx, 0xB0u);
    const u32 sp = frame.at(0u);
    const float one = 1.0f;
    put(rt, sp + 0x80u, word(rt, position));
    put(rt, sp + 0x84u, word(rt, position + 4u));
    put(rt, sp + 0x88u, word(rt, position + 8u));
    put_float(rt, sp + 0x8Cu, one);
    put_float(rt, sp + 0x50u, one);
    put_float(rt, sp + 0x64u, one);
    put_float(rt, sp + 0x78u, one);
    for (const u32 offset : {0x60u, 0x70u, 0x54u, 0x74u, 0x58u, 0x68u, 0x5Cu, 0x6Cu, 0x7Cu}) put(rt, sp + offset, 0u);
    call_set(rt, ctx, 0x08B4E9D4u, {out, sp + 0x50u});
    put(rt, sp, word(rt, vector));
    put(rt, sp + 4u, word(rt, vector + 4u));
    put(rt, sp + 8u, word(rt, vector + 8u));
    put(rt, sp + 0xCu, word(rt, vector + 0xCu));
    call_setf(rt, ctx, 0x08B4B114u, {sp, sp + 0x10u}, {0.0f, 0.0f, 0.0f});
    call_set(rt, ctx, 0x08B4E9D4u, {out, sp + 0x10u});
    if (axis == 0u) {
        ctx.gpr[2] = order < 6u ? 1u : 0u;
        return;
    }
    if (order >= 6u) {
        ctx.gpr[2] = 0u;
        return;
    }
    static constexpr u32 kSetters[3] = {0x08B4F2E4u, 0x08B4F3B0u, 0x08B4F47Cu};
    static constexpr u32 kOffsets[3] = {0u, 4u, 8u};
    static constexpr unsigned kOrders[6][3] = {{2u, 1u, 0u}, {0u, 2u, 1u}, {1u, 0u, 2u},
                                              {1u, 2u, 0u}, {2u, 0u, 1u}, {0u, 1u, 2u}};
    for (unsigned step = 0; step < 3u; ++step) {
        const unsigned slot = kOrders[order][step];
        call_setf(rt, ctx, kSetters[slot], {out}, {float_at(rt, axis + kOffsets[slot])});
    }
}

// 0x08C54E74 (a0 = this, a1 = the source): copy-assigns a container that keeps its 8-byte entries inline at + 0x24
// (at most 8; more go to a buffer 0x08B6461C allocates) through the data pointer at + 0x68, its count at + 0x64 and the
// reference-counted holder at + 0x18 (0x08B6085C). The old buffer is released (0x08B61FC0) unless it is the inline one,
// the entries are copied, the words at + 4 / + 8 / + 0xC / + 0x10 / + 0x70 follow, the lazy flag at + 0x75 of both
// sides is set through 0x08C5477C with 0x20, and the 0x6C sub-object is copied through a frame temporary
// (0x08B5C450 / 0x08B5D1E0 / 0x08B5D34C), then 0x08C548C4, 0x08B6534C with 2 and 0x08C54CFC finish. v0 = this.

// The inverse of func_08890068: a byte-swapped pointer field gets the delta subtracted.
inline void unswap_sub(Runtime &rt, u32 address, u32 delta) {
    put(rt, address, swapped(swapped(word(rt, address)) + delta));
}

// 0x08890554 (a0 = the address of the module header word): undoes the load relocation: the byte-swapped address at +4
// is the delta to subtract, the field is cleared, and the three tables and the header's pointer fields get the delta
// removed (the first table with 0x10-byte records at +8, the second with 0x20-byte records at +8, the third with
// 0x20-byte records whose + 0x14 pointer leads to 0x10-byte records of two pointers, plus the record's own +4 / +8 /
// +0xC / +0x14). Returns 0 when the header was not relocated, 1 otherwise.

// 0x08905594 (a0 = the result, a1 = the point, a2 / a3 / t0 / t1 = four vertices, 12-byte xyz triples): the segment /
// triangle intersection: the first two vertices of the plane at a2 with the edges to a3 / t0 / t1 give two barycentric
// parameters (the degenerate case divides directly), stored at out[0] / out[1], and the point's missing y is
// interpolated into out[2]; v0 = 1 when the point lies inside. An early exit leaves v0 as it was.

// 0x08AFBFEC (a0 = a vertex array of 12-byte xyz triples, a1..t1 = five indices): builds the four edge vectors from the
// vertex at a1 (to the ones at a2, a3, t0 and t1) in the frame and folds them with 0x08AFBF8C three times (into the
// frame at sp, sp + 0xC and sp + 0x18, against sp + 0x24), which gives three values. The global block at 0x08D47308
// holds a cached tolerance, refreshed from the values' magnitude times -0.0001 through 0x08B6441C / 0x08B64494; the
// point is inside when each value lies between -tolerance and +tolerance. v0 = 1 or 0 (one exit leaves v0 as it was).

// 0x088D51BC (a0 = the holder of the source blob, a1 = the destination): 0x088D47EC gives the blob's size, the
// destination is zeroed when that is longer than the 0x116 bytes copied, the blob (the word the holder points at) is
// copied and v0 = destination + size.
void func_088D51BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 size = call_set(rt, ctx, 0x088D47ECu, {holder});
    if (static_cast<s32>(size) > 0x116) call_set(rt, ctx, 0x08B36018u, {destination, 0u, size});
    const u32 source = word(rt, holder);
    for (u32 i = 0; i < 0x116u; ++i) put8(rt, destination + i, byte_u(rt, source + i));
    ctx.gpr[2] = destination + size;
}

// 0x08959C9C (a0 = the two pointers 0x08959180 needs, a1 = the packet whose first word is its id, a2 = the new id):
// when the packet's id is neither 0 nor -1, 0x08959180 prepares the record (a0 = a0[0], a1 = byte of a0[8]) and, if the
// record's id at +4 differs, the packet's 0x34 bytes move to the record at +4 while the previous blob at +4 goes to
// +0x3C, the record's +0x38 gets the new id and a -1 id at +0 is replaced. Then the record's state at +0xA0 selects:
// 1 moves to state 2 and returns, 0 and 3 return, anything else accumulates the counters at +0xA4..+0xD0 (the deltas
// beyond 0x258 / 0x5DC / 0x708 counts included). v0 = the last counter + 1.

// 0x0895F194 (a0 = the object whose word at +8 is the track data, a1 = the segment index, a2 = the out xyz, a3 = a
// float out): samples the 12-byte point records at data + 0x610. With the state byte at + 0x67A equal to 20 it copies
// the record of (byte + 0x678 - index, wrapped by 5) and returns (4 - index) * 20 / 80; with index 0 it runs
// 0x0895F174(a0, out) and returns 1; past index 4 it copies the record of (byte + 0x678 - index + 1, wrapped by 6) and
// returns ((4 - index) * 20 + state) / 80; otherwise it interpolates the records of (byte + 0x678 - index + 2, wrapped
// by 7) and (byte + 0x679) by state / 20 and returns 0. The float out is the state / 60. v0 is never set: the result is
// in f0.

// 0x08A1CB1C (a0 = the object): 0x08ACB4F8(a0) first, then the word at +0x24 (a 0x8011xxxx kind) selects a small
// number through the binary decision tree the compiler built: 0x80110382 / 0x80110342 / 0x80110302 -> 4, 0x8011030B /
// 0x8011038B / 0x8011034B / 0x801103CB -> 0xB, 0x80110306 / 0x80110386 / 0x801103C6 -> 8, 0x80110305 / 0x80110345 /
// 0x801103C5 / 0x80110384's upper side -> 7, 0x80110301 / 0x80110341 / 0x80110381 / 0x801103C1 -> 3, 0x80110308 /
// 0x80110348 / 0x801103C8 / 0x80110388 -> 0xA, 0x80110309 / 0x801103C5-side -> 0xC / 7, 0x80110344 / 0x80110384 -> 6,
// 0x80110347 / 0x801103C6's upper side -> 9, 1 / 2 / 0 -> 1 / 2 / 0, 0x80110384's lower side -> 5, and 0xD otherwise.
// v0 = that number.

} // namespace
} // namespace new_batch_main_22
} // namespace gt::game

#define MAIN22(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_22::name, gt::ReturnKind::kind)
MAIN22(0x088C7D50u, func_088C7D50, Word);
MAIN22(0x0887B768u, func_0887B768, Word);
MAIN22(0x088FE08Cu, func_088FE08C, Word);
MAIN22(0x08A0F5D4u, func_08A0F5D4, Word);
MAIN22(0x08ACD5D4u, func_08ACD5D4, Word);
MAIN22(0x08924690u, func_08924690, Word);
MAIN22(0x088DBF0Cu, func_088DBF0C, Word);
MAIN22(0x08AF5830u, func_08AF5830, Word);
MAIN22(0x088D51BCu, func_088D51BC, Word);
