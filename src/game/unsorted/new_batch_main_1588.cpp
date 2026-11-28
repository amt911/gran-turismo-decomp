// Main-session batch 1588: 20 functions drafted from tools.disasm (unverified, draft only).
// Conventions as in the other hot batches: `call_set` writes the argument registers the original sets before the `jal` and
// keeps v0/v1/f0 of the callee; a `jal`'s delay slot runs BEFORE the call; a register the original does not set before a call
// is passed as the value it holds at that point (ctx.gpr), never a guess.
// Not drafted (read, left for another pass): 0x0886A3A4 (ll/sc reference counts), 0x088941A0 (swl/swr copy), 0x088DAC34 (64-bit
// subtract), 0x088B4CF4 (busy-wait loop), 0x08900350, 0x089FA130, 0x08ABF24C, 0x089A8F34 (float), 0x0892FCE0 (callee leftovers
// in a0/a1), 0x08B0EC60 (not reached in this pass).

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1588 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with a0.. set like the original sets them before the jal; v0/v1/f0 of the callee stay in the registers.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// `object->slot(args)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}

inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 signed_half(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}
inline u32 signed_byte(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}

// Callee-saved registers the original saves in its prologue (s0..s7), restored on every exit.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 regs[8];
    explicit SavedRegs(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 8u; ++i) regs[i] = c.gpr[16u + i];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx.gpr[16u + i] = regs[i];
    }
};

constexpr u32 kGuardAcquire = 0x08B6441Cu;
constexpr u32 kGuardRelease = 0x08B64494u;
constexpr u32 kAssertNull = 0x08A5BB7Cu;

// A function-local static built once: while the guard byte is zero, the acquire decides; when it returns non-zero the
// vtable goes into `storage` and the release runs.
void build_static_once(Runtime &rt, AllegrexContext &ctx, u32 guard, u32 storage, u32 vtable) {
    if (rt.memory().load8(guard) != 0u) return;
    if (call_set(rt, ctx, kGuardAcquire, {guard}) == 0u) return;
    put(rt, storage, vtable);
    (void)call_set(rt, ctx, kGuardRelease, {guard});
}

enum class Field { UnsignedHalf, SignedHalf, SignedByte, Word };

// The four `object->slot()` then divide shapes. The object is `*a3` (a null one calls the assert), the virtual `slot` is
// called on it twice around the read of the field at self+0x14; the field is divided by the second answer, and the quotient
// (or remainder) goes to `callee` as an out-parameter at the frame's sp, which is then copied to `out` (a1) unless `out` is
// the frame itself, and `release` runs on the frame. A zero divisor leaves HI/LO unspecified on the PSP; drafted as 0.
void divide_field_by_virtual(Runtime &rt, AllegrexContext &ctx, u32 guard, u32 slot, Field field, bool is_signed,
                             bool remainder, u32 callee) {
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 pointer = ctx.gpr[7];
    const u32 sp = frame.at(0u);

    u32 object = word(rt, pointer);
    if (object == 0u) (void)call_set(rt, ctx, kAssertNull, {guard});
    (void)call_virtual(rt, ctx, object, slot, {object});

    object = word(rt, pointer);
    u32 numerator = 0u;
    switch (field) {
    case Field::UnsignedHalf: numerator = rt.memory().load16(self + 0x14u); break;
    case Field::SignedHalf: numerator = signed_half(rt, self + 0x14u); break;
    case Field::SignedByte: numerator = signed_byte(rt, self + 0x14u); break;
    case Field::Word: numerator = word(rt, self + 0x14u); break;
    }
    if (object == 0u) (void)call_set(rt, ctx, kAssertNull, {guard});
    (void)call_virtual(rt, ctx, object, slot, {object});

    const u32 divisor = ctx.gpr[2];
    u32 result = 0u;
    if (divisor != 0u) {
        if (is_signed) {
            const s32 n = static_cast<s32>(numerator);
            const s32 d = static_cast<s32>(divisor);
            if (d == -1) {
                result = remainder ? 0u : 0u - numerator;
            } else {
                result = static_cast<u32>(remainder ? n % d : n / d);
            }
        } else {
            result = remainder ? numerator % divisor : numerator / divisor;
        }
    }
    (void)call_set(rt, ctx, callee, {sp, result});
    if (out != sp) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, sp)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {sp});
}

// 0x08B821A4: a table lookup of a pair for a0 < 0x18; otherwise the pair from the globals is passed through a0 times
// (each call's v0/v1 become the next call's a0/a1). Returns the pair in v0/v1.
void func_08B821A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 count = ctx.gpr[4];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<s32>(count) < 0x18) {
        const u32 entry = 0x08C97640u + (count << 3);
        ctx.gpr[2] = word(rt, entry);
        ctx.gpr[3] = word(rt, entry + 4u);
        return;
    }
    u32 lo = word(rt, 0x08CB6998u);
    u32 hi = word(rt, 0x08CB699Cu);
    const u32 k2 = word(rt, 0x08CB69A0u);
    const u32 k3 = word(rt, 0x08CB69A4u);
    for (u32 i = 0u; i != count; ++i) {
        (void)call_set(rt, ctx, 0x08B71394u, {lo, hi, k2, k3});
        lo = ctx.gpr[2];
        hi = ctx.gpr[3];
    }
    ctx.gpr[2] = lo;
    ctx.gpr[3] = hi;
}

// 0x088452D4: fills the object at a0 (+0x14 the new object, +0x4 the shared static at 0x08D751A8).

// 0x088618E8: resets the state block at a0 (a1 & 0xFF is the mode: 1 runs one extra setup call first).
void func_088618E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 mode = ctx.gpr[5] & 0xFFu;
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x0888B208u, {word(rt, self + 0x5B8u)});
    (void)call_set(rt, ctx, 0x088DAFBCu, {word(rt, self + 0x5ACu), word(rt, self + 0x5B8u)});
    if (mode == 1u) (void)call_set(rt, ctx, 0x08891534u, {self + 0x3F80u});
    u32 cell = self + 0x608u;
    for (u32 i = 0u; i != 2u; ++i) {
        (void)call_set(rt, ctx, 0x0888AB4Cu, {cell, 0u});
        cell += 0x1C38u;
    }
    for (u32 i = 0u; i != 4u; ++i) {
        put16(rt, self + 0x3E84u + 2u * i, 0xFFFFu);
        put8(rt, self + i + 0x3E8Cu, 0u);
    }
    put(rt, self + 0x3E78u, 0u);
    put(rt, self + 0x3FD4u, 0u);
    put8(rt, self + 0x3FDBu, 0u);
    ctx.gpr[2] = 0u;
}

// 0x08A69BFC: a0 = object word, a1 = a name string. A non-empty name is looked up through 0x08AACAF8 (the word at a0 is the
// answer); the header word of the temporary decides the release of its block; then the object is handed to 0x08A69638.
void func_08A69BFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0u);
    const u32 sp = frame.at(0u);
    if (text != 0u && rt.memory().load8(text) != 0u) {
        const u32 slot = frame.at(4u);
        (void)call_set(rt, ctx, 0x08B85EB8u, {slot, text, frame.at(1u)});
        put(rt, self, call_set(rt, ctx, 0x08AACAF8u, {0x08CFF060u, slot}));
        const u32 header = word(rt, slot);
        const u32 block = header - 0xCu;
        if (block != 0x08D74FD0u) {
            const s32 r = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {header - 4u, 0xFFFFFFFFu}));
            if (r <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {block, sp});
        }
    } else {
        put(rt, self, word(rt, 0x08CFF0E0u));
    }
    const u32 obj = word(rt, self);
    ctx.gpr[2] = obj;
    if (obj != 0u) (void)call_set(rt, ctx, 0x08A69638u, {obj});
}

// 0x08A6BB28: a0 = the object (its word at +0 is the owner), a1 = a text; builds into a temporary and releases it.

// 0x08ADCC14: flag lookup: obj = find(a0, key); two tables inside it are indexed by a1 then a2; returns the low bit of the
// final entry, or 0 when any index or entry is out of range.
void func_08ADCC14(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const s32 index1 = static_cast<s32>(ctx.gpr[5]);
    const s32 index2 = static_cast<s32>(ctx.gpr[6]);
    const u32 a2 = ctx.gpr[6];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 obj = call_set(rt, ctx, 0x08ADB278u, {a0, 0x08C90F68u, a2});
    u32 result = 0u;
    do {
        if (obj == 0u) break;
        const u32 table = word(rt, obj + 0x34u);
        if (table == 0u) break;
        if (index1 < 0) break;
        if (static_cast<s32>(word(rt, obj + 0x38u)) - 1 < index1) break;
        const u32 entry1 = word(rt, table + 4u * static_cast<u32>(index1) + 8u);
        if (entry1 == 0u) break;
        const u32 base = word(rt, obj + 0x14u);
        if (index2 < 0) break;
        const u32 cell = base + entry1;
        if (static_cast<s32>(word(rt, cell + 4u)) - 1 < index2) break;
        const u32 entry2 = word(rt, cell + 4u * static_cast<u32>(index2) + 8u);
        if (entry2 != 0u) result = word(rt, base + entry2) & 1u;
    } while (false);
    ctx.gpr[2] = result;
}

// 0x08ADD1EC: a pair-valued call chain; a0..a3 and t0/t1 are the original's; returns the pair of the last call.
void func_08ADD1EC(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 obj = call_set(rt, ctx, 0x08ADBE98u, {a0, a1, 0u, a3});
    if (obj == 0u) {
        ctx.gpr[2] = word(rt, 0x08CB65D0u);
        ctx.gpr[3] = word(rt, 0x08CB65D4u);
        return;
    }
    const u32 field = word(rt, obj + 0xCu);
    u32 lo = a2;
    u32 hi = a3;
    if (t0 != 0u) {
        const u32 r = call_set(rt, ctx, 0x08ADCF48u, {obj, a1, a2, a3, t1});
        lo = r;
        hi = static_cast<u32>(static_cast<s32>(r) >> 31);
    }
    (void)call_set(rt, ctx, 0x08B6EE80u, {lo, hi});
    lo = ctx.gpr[2];
    hi = ctx.gpr[3];
    (void)call_set(rt, ctx, 0x08B71CE0u, {field});
    const u32 r0 = ctx.gpr[2];
    const u32 r1 = ctx.gpr[3];
    (void)call_set(rt, ctx, 0x08B71720u, {lo, hi, r0, r1});
}

// 0x08B47648: constructor of a 0x4800-byte block: vtable, three sub-objects, eight 0x16-byte records, clears a flag run and
// ends in a tail call with the block at +0x8D.
void func_08B47648(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegs saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        (void)call_set(rt, ctx, 0x08B485C8u, {self});
        put(rt, self, 0x08CDAAB8u);
        (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0x48u});
        (void)call_set(rt, ctx, 0x08B3FA98u, {self + 0x76u});
        put8(rt, self + 0x8Cu, 0u);
        (void)call_set(rt, ctx, 0x08B2E05Cu, {ctx.gpr[4]});
        put(rt, self + 0x4890u, ctx.gpr[2]);
        put(rt, self + 0x4894u, ctx.gpr[3]);
        put(rt, self + 0x4898u, 0xFFFFFFFFu);
        u32 record = self + 0x489Cu;
        for (u32 i = 0u; i != 8u; ++i) {
            (void)call_set(rt, ctx, 0x08B3FA98u, {record});
            record += 0x16u;
        }
        (void)call_set(rt, ctx, 0x08B1FDA4u, {self + 0x2Cu});
        put8(rt, self + 0x70u, 0u);
        for (u32 k = 1u; k <= 5u; ++k) put8(rt, self + 0x70u + k, 0u);
    }
    tail_call(ctx, 0x08B36018u, {self + 0x8Du, 0u, 0x4800u});
}

// 0x0888A490: a0 = owner (its word is a block), a1 = a record, a2 = an optional object with a virtual test at 0x68.

// 0x088E4DFC: copies a 0x30-byte record out of the owner (a half, two words, a half, sixteen words) and returns whether the
// owner's update call answer plus one is non-zero.

// 0x08927ED8: a chain of three virtual tests on the objects of a0; returns 1 when all pass and the final steps ran, else 0.
void func_08927ED8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 first = call_set(rt, ctx, 0x089273D8u, {self});
    const u32 r1 = call_virtual(rt, ctx, first, 0x28u, {first, 8u});
    if (r1 == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 second = call_set(rt, ctx, 0x089273D8u, {self});
    const u32 r2 = call_virtual(rt, ctx, second, 0x1Cu, {second, 0x13u});
    if (r2 == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 kind = call_set(rt, ctx, 0x089277BCu, {self});
    if (kind == 2u) {
        (void)call_set(rt, ctx, 0x08927148u, {self, word(rt, self + 0x1FD0u)});
    } else {
        put(rt, self + 0x1FD0u, kind);
        (void)call_set(rt, ctx, 0x08927148u, {self, 2u});
    }
    const u32 obj = call_set(rt, ctx, 0x089273D8u, {self});
    (void)call_set(rt, ctx, 0x08925BCCu, {obj, 1u});
    ctx.gpr[2] = 1u;
}

// 0x0892B180: walks the 16-bit records between the two words at a1 (+0 cursor, +4 end): each record's two bytes feed two
// calls; returns the end word.
void func_0892B180(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 list = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 a1 = call_set(rt, ctx, 0x08B21894u, {a2});
    const u32 s3 = call_set(rt, ctx, 0x0892B0A0u, {list, a1});
    u32 cursor = word(rt, list);
    if (cursor != word(rt, list + 4u)) {
        do {
            const u32 b1 = rt.memory().load8(cursor + 1u);
            const u32 b0 = rt.memory().load8(cursor);
            const u32 res = call_set(rt, ctx, 0x08B21590u, {a2 + 4u, b1});
            (void)call_set(rt, ctx, 0x0892AE14u, {s3, a0, a3, b0, res, 0u});
            cursor += 2u;
        } while (word(rt, list + 4u) != cursor);
    }
    ctx.gpr[2] = word(rt, list + 4u);
}

// 0x08962520: two owned sub-objects of a0 are each refreshed (the first at +0x6C, the second, tail, at +0x40).

// 0x0887EA84: builds the sub-object at +0x70 of a0 from a fresh object, then the function-local static at 0x08D75DF0.
void func_0887EA84(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegs saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        (void)call_set(rt, ctx, 0x0886AD04u, {self});
        put(rt, self, 0x08CC10C8u);
        const u32 obj = call_set(rt, ctx, 0x08B62344u, {0x14u});
        (void)call_set(rt, ctx, 0x08881248u, {obj});
        put(rt, self + 0x70u, obj);
        build_static_once(rt, ctx, 0x08D75DE8u, 0x08D75DF0u, 0x08CC1100u);
        put(rt, self + 0x74u, 0x08D75DF0u);
    }
    tail_call(ctx, 0x0886AAA4u, {self, word(rt, self + 0x70u)});
}

// 0x088844E0: builds the sub-object at +0xC of a0, then the function-local static at 0x08D75E40; tail call with 0.
void func_088844E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegs saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        (void)call_set(rt, ctx, 0x0887E1E0u, {self});
        put(rt, self, 0x08CC1460u);
        const u32 obj = call_set(rt, ctx, 0x08B62344u, {0x1980u});
        (void)call_set(rt, ctx, 0x08884314u, {obj});
        put(rt, self + 0xCu, obj);
        build_static_once(rt, ctx, 0x08D75E38u, 0x08D75E40u, 0x08CC14C0u);
        put(rt, self + 0x10u, 0x08D75E40u);
    }
    tail_call(ctx, 0x088824F8u, {self, 0u});
}

// 0x08875A18: a0 = the owner, a1 = an input, a2 = an output; a three-word result is copied through 0x0886AFA8; the
// temporary is released through the header word.
void func_08875A18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 input = ctx.gpr[5];
    const u32 output = ctx.gpr[6];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = frame.at(0u);
    const u32 slot = frame.at(4u);
    const u32 record = frame.at(0x14u);
    (void)call_set(rt, ctx, 0x08AB85F0u, {slot, input});
    (void)call_set(rt, ctx, 0x088755E4u, {self, record, slot, 4u});
    if (static_cast<s32>(word(rt, frame.at(0xCu))) >= 3) {
        const u32 v = call_set(rt, ctx, 0x0886AFA8u, {word(rt, self)});
        (void)call_set(rt, ctx, 0x0887C914u, {v + 4u, output, record});
    }
    const u32 header = word(rt, slot);
    const u32 block = header - 0xCu;
    if (block != 0x08D75298u) {
        const s32 r = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {header - 4u, 0xFFFFFFFFu}));
        if (r <= 0) (void)call_set(rt, ctx, 0x08B93554u, {block, sp});
    }
}

// 0x08AB06CC: remainder of the unsigned half field (0x08A85B1C takes the remainder) over the virtual slot +0x48.

// 0x08A5FE6C: unsigned word field over the virtual slot +0x4C; quotient to 0x08A5FA8C.
void func_08A5FE6C(Runtime &rt, AllegrexContext &ctx) {
    divide_field_by_virtual(rt, ctx, 0x08CB00F8u, 0x4Cu, Field::Word, false, false, 0x08A5FA8Cu);
}

// 0x08A7372C: signed half field over the virtual slot +0x38; signed quotient to 0x08A85B1C.

// 0x08A964A0: signed byte field over the virtual slot +0x34; signed quotient to 0x08A85B1C.

}  // namespace
}  // namespace new_batch_main_1588
}  // namespace gt::game

#define NEW1588(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1588::name, gt::ReturnKind::kind)
NEW1588(0x08B821A4u, func_08B821A4, DoubleWord);
NEW1588(0x088618E8u, func_088618E8, Word);
NEW1588(0x08A69BFCu, func_08A69BFC, Word);
NEW1588(0x08ADCC14u, func_08ADCC14, Word);
NEW1588(0x08ADD1ECu, func_08ADD1EC, DoubleWord);
NEW1588(0x08B47648u, func_08B47648, Word);
NEW1588(0x08927ED8u, func_08927ED8, Word);
NEW1588(0x0892B180u, func_0892B180, Word);
NEW1588(0x0887EA84u, func_0887EA84, Word);
NEW1588(0x088844E0u, func_088844E0, Word);
NEW1588(0x08875A18u, func_08875A18, Void);
NEW1588(0x08A5FE6Cu, func_08A5FE6C, Word);
