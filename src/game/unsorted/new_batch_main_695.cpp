// New batch (main session 695): functions the extra seeds made emittable, written as structured C from `tools.disasm`.
// Draft only: the differential gate decides which of them land. Every body keeps the original's exits (`v0` on each
// path), the call order, and the memory it writes; arguments go in a0-a3 / t0-t3, float arguments in f12-f15.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_695 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

// A guest call; afterwards `v0`, `v1`, `f0` and `a1` hold what the callee left, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
// The same with float arguments ($f12, $f13, ...).
u32 call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
           std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
// `lw v0, slot(v); jalr v0`: the callee starts with v0 = its own address.
u32 call_target(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
// `lw v0, 0(a0); lw v1, slot(v0); jalr v1`: v0 still holds the vtable word when the callee starts.
u32 call_keeping_v0(Runtime &rt, AllegrexContext &ctx, u32 v0, u32 target,
                    std::initializer_list<u32> arguments) {
    ctx.gpr[2] = v0;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    ctx.gpr[5] = result.a1;
    return result.v0;
}

// Callee-saved registers a body keeps its arguments in and puts back.
struct SavedS0 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS0(AllegrexContext &c) : ctx(c), value(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = value; }
};
struct SavedS1 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS1(AllegrexContext &c) : ctx(c), value(c.gpr[17]) {}
    ~SavedS1() { ctx.gpr[17] = value; }
};
struct SavedS2 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS2(AllegrexContext &c) : ctx(c), value(c.gpr[18]) {}
    ~SavedS2() { ctx.gpr[18] = value; }
};
struct SavedS3 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS3(AllegrexContext &c) : ctx(c), value(c.gpr[19]) {}
    ~SavedS3() { ctx.gpr[19] = value; }
};
struct SavedS4 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS4(AllegrexContext &c) : ctx(c), value(c.gpr[20]) {}
    ~SavedS4() { ctx.gpr[20] = value; }
};
struct SavedS5 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS5(AllegrexContext &c) : ctx(c), value(c.gpr[21]) {}
    ~SavedS5() { ctx.gpr[21] = value; }
};
struct SavedS6 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS6(AllegrexContext &c) : ctx(c), value(c.gpr[22]) {}
    ~SavedS6() { ctx.gpr[22] = value; }
};
struct SavedS7 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS7(AllegrexContext &c) : ctx(c), value(c.gpr[23]) {}
    ~SavedS7() { ctx.gpr[23] = value; }
};
struct SavedFp {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedFp(AllegrexContext &c) : ctx(c), value(c.gpr[30]) {}
    ~SavedFp() { ctx.gpr[30] = value; }
};
struct SavedF20 {
    AllegrexContext &ctx;
    u32 bits;
    explicit SavedF20(AllegrexContext &c) : ctx(c), bits(c.fpr_bits(20)) {}
    ~SavedF20() { ctx.set_fpr_bits(20, bits); }
};

s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }

// `trunc.w.s` toward zero. The routine only reaches it with values it means (a ratio times 255); an out-of-range
// or NaN operand, which the hardware leaves undefined, answers 0 here.
u32 trunc_to_word(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0u;
    return static_cast<u32>(static_cast<s32>(value));
}
// The `div`/`mfhi` remainder of the vertex loop; a zero divisor would have trapped on the PSP.
s32 signed_remainder(s32 value, s32 divisor) {
    if (divisor == 0) return value;
    return value % divisor;
}

// The PSP's `ll`/`sc` refcount decrement: one word, minus one, always stored here (a real `sc` retries until it
// stores). The old count comes back.
u32 refcount_decrement(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}
// `lw v0, slot(vtable); jalr v0` on a refcounted object (v0/v1/f0 are the callee's afterwards).
void virtual_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    call_target(rt, ctx, word(rt, word(rt, object) + slot), arguments);
}
// Release of one intrusive reference: -1 at +4; on the last one the virtual destructor at +8 runs, then -1 at +8;
// on that last one the deallocator at +0xC runs. The vtable word is re-read before each virtual call, as the
// original's `lw v1, 0(s0)` does (the destructor may replace it).
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (refcount_decrement(rt, object + 4u) != 1u) return;
    virtual_call(rt, ctx, object, 8u, {object});
    if (refcount_decrement(rt, object + 8u) != 1u) return;
    virtual_call(rt, ctx, object, 0xCu, {object});
}
// The same release for an object whose +4 count is not touched: only +8, and the deallocator at +0xC.
void release_object_second_count(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (refcount_decrement(rt, object + 8u) != 1u) return;
    virtual_call(rt, ctx, object, 0xCu, {object});
}

// ---- 0x0888504C ----
// a0 = the owner, a1 = a mode (0 or 1), a2 = a candidate object. Slot 0x190 of the container at owner + 0xC is used
// for mode 0 and slot 0x194 for mode 1: the object already there is released through its slot 0x2C and the slot is
// cleared; when a2 is not null the slot takes what a2's slot 0x18 builds, and that new object is named
// (0x0887BD94(id) then slot 8), given the owner (slot 0x14), labelled (slot 0x18) and handed to its own slot 0x28
// as the function's tail call. v0 = 1 for a mode other than 0/1, else the container word, or the last object's
// vtable on the tail-call path. The "this" argument the virtual calls get is the object in the slot, not the owner.
void func_0888504C(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    const u32 candidate = ctx.gpr[6];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    GuestFrame frame(ctx, 0x10u);
    if (mode > 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[18] = owner;
    const bool second = mode == 1u;
    const u32 offset = second ? 0x194u : 0x190u;
    const u32 slot_id = second ? 0x10u : 0xBu;
    const u32 name = second ? 0x08CA0680u : 0x08CA0670u;
    const u32 mask = second ? 0xFF8080FFu : 0x80FF00FFu;
    const u32 label = second ? 0x08BA60B4u : 0x08BA60ACu;

    u32 container = word(rt, owner + 0xCu);
    const u32 existing = word(rt, container + offset);
    if (existing != 0u) {
        const u32 vtable = word(rt, existing);
        call_keeping_v0(rt, ctx, vtable, word(rt, vtable + 0x2Cu), {existing});
        container = word(rt, owner + 0xCu);
        put(rt, container + offset, 0u);
    }
    if (candidate == 0u) {
        ctx.gpr[2] = word(rt, owner + 0xCu);
        return;
    }
    const u32 vtable = word(rt, candidate);
    container = word(rt, owner + 0xCu);
    ctx.gpr[16] = container;  // s0: the slot's container, read before the call and stored into after it
    const u32 built = call_target(rt, ctx, word(rt, vtable + 0x18u), {candidate});
    put(rt, container + offset, built);
    container = word(rt, owner + 0xCu);
    const u32 stored = word(rt, container + offset);
    if (stored == 0u) {
        ctx.gpr[2] = container;
        return;
    }
    const u32 destructor = word(rt, word(rt, stored) + 8u);  // read before the lookup, as the delay slot does
    const u32 helper = call(rt, ctx, 0x0887BD94u, {slot_id});
    call_keeping_v0(rt, ctx, helper, destructor, {stored, helper});
    container = word(rt, owner + 0xCu);
    const u32 named = word(rt, container + offset);
    call_target(rt, ctx, word(rt, word(rt, named) + 0xCu), {named, name, mask});
    container = word(rt, owner + 0xCu);
    const u32 owned = word(rt, container + offset);
    call_target(rt, ctx, word(rt, word(rt, owned) + 0x14u), {owned, owner});
    container = word(rt, owner + 0xCu);
    const u32 labelled = word(rt, container + offset);
    call_target(rt, ctx, word(rt, word(rt, labelled) + 0x18u), {labelled, label});
    container = word(rt, owner + 0xCu);
    const u32 tail = word(rt, container + offset);
    const u32 tail_vtable = word(rt, tail);
    ctx.gpr[2] = tail_vtable;
    tail_call(ctx, word(rt, tail_vtable + 0x28u), {tail});
}

// ---- 0x088C7D50 ----
// a0 = the mesh state, a1 = an object handed to 0x08AF26F0 untouched, f12 = a ratio. f12 * 255 becomes the alpha
// byte of two colour words at sp + 0x10 / sp + 0x14 (0xFFFF5E20 / 0xFFFFFFFF once the byte no longer fits); then
// each of the state's +0xACC vertices, 0xC bytes each from the list at +0x254, is turned into the scratch block at
// sp by 0x088C7920, 0x08B5A7F0 writes three floats into it, and a 0x10-byte record (a colour word, then those
// floats) goes to the buffer 0x08B56A00 returned. The colour flips for the first vertex of every fourth level: the
// counter at +0xAC4 gives the level width (0x00FFFFFF for its last column every other time). v0 = the last
// graphics-library call's result.
void func_088C7D50(Runtime &rt, AllegrexContext &ctx) {
    const u32 state = ctx.gpr[4];
    const u32 passed = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    const float ratio = ctx.fpr[12];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    SavedS4 saved4(ctx);
    SavedS5 saved5(ctx);
    SavedS6 saved6(ctx);
    SavedS7 saved7(ctx);
    SavedFp saved_fp(ctx);
    SavedF20 saved_f20(ctx);
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = frame.at(0u);
    ctx.gpr[23] = state;  // s7
    call(rt, ctx, 0x08AF26F0u, {passed, passed, third, fourth});
    const float two_to_31 = std::bit_cast<float>(0x4F000000u);
    const float scaled = psp_mul(ratio, 255.0f);
    u32 alpha = trunc_to_word(scaled);
    if (two_to_31 <= scaled) alpha = trunc_to_word(scaled - two_to_31) | 0x80000000u;
    if (alpha < 0x100u) {
        put(rt, sp + 0x14u, (alpha << 24) | 0x00FFFFFFu);
        put(rt, sp + 0x10u, (alpha << 24) | 0x00FF5E20u);
    } else {
        put(rt, sp + 0x10u, 0xFFFF5E20u);
        put(rt, sp + 0x14u, 0xFFFFFFFFu);
    }
    const u32 count = word(rt, state + 0xACCu);
    put(rt, sp + 0x18u, call(rt, ctx, 0x08B550E0u, {count << 4}));
    call(rt, ctx, 0x08B56A00u, {0u});
    u32 cursor = word(rt, sp + 0x18u);
    if (static_cast<s32>(count) > 0) {
        u32 column = 0u;
        u32 alternate = 0u;
        u32 entry = state + 0x254u;
        for (u32 vertex = 0; vertex != count; ++vertex) {
            const u32 stride = word(rt, state + 0xAC4u);
            u32 colour = word(rt, sp + 0x10u);
            if ((alternate & 1u) == 0u) colour = word(rt, sp + 0x14u);
            const s32 remainder = signed_remainder(static_cast<s32>(column), static_cast<s32>(stride + 2u));
            const bool highlight = remainder == static_cast<s32>(stride + 1u);
            if (highlight) colour = 0x00FFFFFFu;
            const u32 record = entry;
            entry += 0xCu;
            call(rt, ctx, 0x088C7920u, {record, sp});
            call(rt, ctx, 0x08B5A7F0u, {sp, sp});
            put(rt, cursor, colour);
            put(rt, cursor + 4u, word(rt, sp));
            put(rt, cursor + 8u, word(rt, sp + 4u));
            put(rt, cursor + 0xCu, word(rt, sp + 8u));
            cursor += 0x10u;
            if (vertex % 4u == 3u) {
                if (!highlight) alternate += 1u;
                column += 1u;
            }
        }
    }
    call(rt, ctx, 0x08B569A0u, {});
    call(rt, ctx, 0x08B56510u, {});
    call(rt, ctx, 0x08B555F8u, {word(rt, sp + 0x18u)});
    call(rt, ctx, 0x08B556A0u, {0u, 0u, 0x103u, 0u, 3u, 0u});
    call(rt, ctx, 0x08B55588u, {4u, count});
    call(rt, ctx, 0x08B56940u, {});
}

// ---- 0x08924690 ----
// a0 = the state, a1 = the destination. With the flag byte at +0x35 clear it only runs 0x08921584(state). Otherwise
// it resets the destination's scale / translation words, advances the angle at state + 0x10 by 0.005 wrapped into
// [-pi, pi], stores -0.396, 0.5, 3.8 and 58 at state + 0xC / + 0x20 / + 0x2C / + 0x30, and sends two 4x4 transforms
// (built in the frame, 0x08B4E9D4 each; the first scales and offsets by the state's words, the second by the state's
// +0x1C / +0x20 / +0x2C) to the destination through 0x08B4F3B0 / 0x08B4F2E4 / 0x08B4F47C; the state's word at +0x30
// ends at destination + 0x70. v0 = the last 0x08B4E9D4's result.
void func_08924690(Runtime &rt, AllegrexContext &ctx) {
    const u32 state = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    SavedF20 saved_f20(ctx);
    GuestFrame frame(ctx, 0x70u);
    const u32 sp = frame.at(0u);
    ctx.gpr[17] = state;      // s1
    ctx.gpr[19] = state + 0xCu;  // s3
    ctx.gpr[18] = state + 0x1Cu; // s2
    const float one = 1.0f;
    const float two_pi = std::bit_cast<float>(0x40C90FDBu);
    const float pi = std::bit_cast<float>(0x40490FDBu);
    if (rt.memory().load8(state + 0x35u) == 0u) {
        call(rt, ctx, 0x08921584u, {state});
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
        if (pi < advanced) {
            put_float(rt, state + 0x10u, advanced - two_pi);
        } else if (advanced < -pi) {
            put_float(rt, state + 0x10u, advanced + two_pi);
        }
        put_float(rt, state + 0xCu, std::bit_cast<float>(0xBECAD962u));
        put_float(rt, state + 0x20u, std::bit_cast<float>(0x3F000000u));
        put_float(rt, state + 0x2Cu, std::bit_cast<float>(0x40733333u));
        put_float(rt, state + 0x30u, std::bit_cast<float>(0x42680000u));
    }
    // The first matrix: unit scale, 0.6 at +0x34.
    put_float(rt, sp + 0x34u, std::bit_cast<float>(0x3F19999Au));
    put_float(rt, sp, one);
    put_float(rt, sp + 0x14u, one);
    put_float(rt, sp + 0x28u, one);
    put_float(rt, sp + 0x3Cu, one);
    for (const u32 offset : {0x4u, 0x8u, 0xCu, 0x10u, 0x18u, 0x1Cu, 0x20u, 0x24u, 0x2Cu, 0x30u, 0x38u}) {
        put(rt, sp + offset, 0u);
    }
    call(rt, ctx, 0x08B4E9D4u, {out, sp});
    call_f(rt, ctx, 0x08B4F3B0u, {out}, {float_at(rt, state + 0x10u)});
    call_f(rt, ctx, 0x08B4F2E4u, {out}, {float_at(rt, state + 0xCu)});
    call_f(rt, ctx, 0x08B4F47Cu, {out}, {float_at(rt, state + 0x14u)});
    // The second matrix: unit scale with the state's words at +0x1C / +0x20 / +0x2C in the translation column.
    put_float(rt, sp + 0x30u, float_at(rt, state + 0x1Cu));
    put_float(rt, sp + 0x3Cu, one);
    put_float(rt, sp, one);
    put_float(rt, sp + 0x14u, one);
    put_float(rt, sp + 0x28u, one);
    put_float(rt, sp + 0x34u, float_at(rt, state + 0x20u));
    put_float(rt, sp + 0x38u, float_at(rt, state + 0x2Cu));
    for (const u32 offset : {0x4u, 0x8u, 0xCu, 0x10u, 0x18u, 0x1Cu, 0x20u, 0x24u, 0x2Cu}) {
        put(rt, sp + offset, 0u);
    }
    call(rt, ctx, 0x08B4E9D4u, {out, sp});
    put_float(rt, out + 0x70u, float_at(rt, state + 0x30u));
}

// ---- 0x08A6A6F0 ----
// Appends one character to the copy-on-write string `out` (the library's push_back): the buffer is cloned first
// when it is too small (`capacity` at -8 of the data) or shared (`refcount` at -4 being positive), the byte goes at
// the current length (at -0xC), the length becomes length + 1, the refcount drops to 0 and the terminator follows.
void append_character(Runtime &rt, AllegrexContext &ctx, u32 out, u32 character) {
    u32 data = word(rt, out);
    u32 length = word(rt, data - 0xCu);
    const u32 needed = length + 1u;
    if (word(rt, data - 8u) < needed || static_cast<s32>(word(rt, data - 4u)) > 0) {
        call(rt, ctx, 0x08B864ECu, {out, needed});
        data = word(rt, out);
        length = word(rt, data - 0xCu);
    }
    rt.memory().store8(data + length, static_cast<u8>(character));
    const u32 base = word(rt, out);
    put(rt, base - 4u, 0u);
    put(rt, base - 0xCu, needed);
    rt.memory().store8(base + needed, 0u);
}

// ---- 0x08A6A6F0 (continued) ----
// a0 = the string being built, a1 = an object whose slot 0xC hands out the source text. Every byte of the source is
// appended: the control bytes 7..0xD as their two-character escapes (0x08CB04D0 + 4 * (byte - 7)), the printable
// ASCII range as itself, and anything else formatted by 0x08B3767C into the frame then appended. A closing
// 0x08CB04CC (two bytes, count 1) ends the work, the object at sp + 4 is released, and v0 = the string.
void func_08A6A6F0(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    SavedS4 saved4(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[17] = out;  // s1
    ctx.gpr[20] = 0x08CB04CCu;  // s4, the closing two-character string
    call_target(rt, ctx, word(rt, word(rt, source) + 0xCu), {frame.at(4u)});
    const u32 text = word(rt, frame.at(4u));  // s2
    call(rt, ctx, 0x08B85EB8u, {out, 0x08CB04CCu, frame.at(1u)});
    u32 cursor = text;
    u32 byte = rt.memory().load8(cursor);
    while (byte != 0u) {
        const s32 character = static_cast<std::int8_t>(byte);
        const u32 escaped = static_cast<u32>(character - 7);
        if (escaped < 7u) {
            call(rt, ctx, 0x08BEED1Cu, {out, 0x08CB04D0u + 4u * escaped, 2u});
        } else if (static_cast<u32>(character - 0x20) < 0x5Fu) {
            append_character(rt, ctx, out, static_cast<u32>(character));
        } else {
            call(rt, ctx, 0x08B3767Cu, {frame.at(8u), 0xAu, 0x08CB04ECu});
            const u32 length = call(rt, ctx, 0x08B35F60u, {frame.at(8u)});
            call(rt, ctx, 0x08BEED1Cu, {out, frame.at(8u), length});
        }
        ++cursor;
        byte = rt.memory().load8(cursor);
    }
    call(rt, ctx, 0x08BEED1Cu, {out, 0x08CB04CCu, 1u});
    // The source object's release: its shared block is below the 0x08D74FD0 sentinel here, so the reference count
    // at -4 decides whether 0x08B8542C deallocates it.
    const u32 object = word(rt, frame.at(4u));
    const u32 block = object - 0xCu;
    if (block != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {object - 4u, 0xFFFFFFFFu})) <= 0) {
            call(rt, ctx, 0x08B8542Cu, {block, frame.at(0u)});
        }
    }
    ctx.gpr[2] = out;
}

// ---- 0x0886DB78 ----
// Constructor of a large object (vtable 0x08CBF748, then 0x08CC6850): a 0x338-byte body is allocated with
// 0x08B62344, its member blocks go through their constructors (0x08B93A70, 0x0887D080, 0x0890B2F4, 0x0887DE44,
// 0x08B28DCC, 0x08B36018), its arrays and small blocks are zeroed, a 14-byte run is copied from 0x08C9F754, and the
// body is parked at owner + 4. v0 = the body's +0x330 block.
void func_0886DB78(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    SavedS0 saved0(ctx);
    SavedS1 saved1(ctx);
    SavedS2 saved2(ctx);
    SavedS3 saved3(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = owner;  // s2
    put(rt, owner, 0x08CBF748u);
    const u32 body = call(rt, ctx, 0x08B62344u, {0x338u});
    ctx.gpr[17] = body;  // s1
    call(rt, ctx, 0x08B93A70u, {body});
    const u32 block = body + 0x60u;
    ctx.gpr[19] = block;  // s3
    put(rt, body + 0xCu, 0u);
    put(rt, body + 0x28u, 0u);
    put(rt, body + 0x2Cu, 0u);
    rt.memory().store8(body + 0x30u, 0u);
    put(rt, body + 0x34u, 0u);
    call(rt, ctx, 0x0887D080u, {body + 0x38u});
    put(rt, body + 0x44u, 0u);
    call(rt, ctx, 0x0890B2F4u, {block});
    rt.memory().store8(body + 0x68u, 0u);
    put(rt, body + 0x6Cu, 0u);
    put(rt, body + 0x70u, 0u);
    put(rt, body + 0x74u, 0u);
    put(rt, body + 0x78u, 0u);
    put(rt, body + 0x7Cu, 0u);
    put(rt, body + 0x80u, 0u);
    put(rt, body + 0x84u, 0u);
    call(rt, ctx, 0x0887DE44u, {body + 0x88u, owner});
    put(rt, body + 0xF8u, 0x08CC6838u);
    put(rt, body + 0xECu, 0u);
    put(rt, body + 0xF0u, 0u);
    put(rt, body + 0xF4u, 0u);
    put(rt, body + 0xFCu, 0u);
    put(rt, body + 0x100u, 0u);
    put(rt, body + 0x104u, 0u);
    put(rt, body + 0x108u, 0u);
    call(rt, ctx, 0x08B28DCCu, {body + 0x110u});
    const u32 one = std::bit_cast<u32>(1.0f);
    put(rt, body + 0x150u, 0u);
    const u32 node = body + 0x15Cu;
    const u32 array = body + 0x184u;
    put(rt, body + 0xF8u, 0x08CC6850u);
    put(rt, body + 0x138u, 0u);
    put(rt, body + 0x13Cu, 0u);
    put(rt, body + 0x144u, 0u);
    put(rt, body + 0x148u, one);
    rt.memory().store8(body + 0x14Cu, 0u);
    put(rt, body + 0x154u, 0u);
    put(rt, body + 0x158u, 0u);
    put(rt, body + 0x15Cu, 0u);
    // The 0x15C block: its flags word at +0x20 loses its low three bits, the unit value at +8 is copied to the
    // diagonal entries +0x10 / +0x18 and the zero at +0xC to +0x14 / +0x1C.
    const u32 flags = word(rt, node + 0x20u) & ~7u;  // `ins $v0, $zero, 0, 3`
    put(rt, node + 8u, one);
    put(rt, node + 0xCu, 0u);
    const u32 copied_unit = word(rt, node + 8u);
    const u32 copied_zero = word(rt, node + 0xCu);
    put(rt, node + 0x20u, flags);
    put(rt, node + 0x10u, copied_unit);
    put(rt, node + 0x14u, copied_zero);
    put(rt, node + 4u, one);
    put(rt, node + 0x18u, copied_unit);
    put(rt, node + 0x1Cu, copied_zero);
    put(rt, body + 0x180u, 0u);
    for (u32 i = 0; i < 16u; ++i) put(rt, array + 4u * i, 0u);
    // Four small intrusive list nodes: 0x1C4 points its two links at 0x1C8, 0x1DC at 0x1E0 and 0x1F4 at 0x1F8.
    const u32 node1 = body + 0x1C4u;
    const u32 node2 = body + 0x1C8u;
    const u32 node3 = body + 0x1E0u;
    const u32 node4 = body + 0x1F8u;
    put(rt, node1 + 0x14u, 0u);
    put(rt, node1 + 0x10u, node2);
    put(rt, node1 + 0xCu, node2);
    put(rt, node1 + 4u, 0u);
    put(rt, node1 + 8u, 0u);
    put(rt, body + 0x1DCu + 0x10u, node3);
    put(rt, body + 0x1DCu + 0xCu, node3);
    put(rt, body + 0x1DCu + 4u, 0u);
    put(rt, body + 0x1DCu + 0x14u, 0u);
    put(rt, body + 0x1DCu + 8u, 0u);
    put(rt, body + 0x1F4u + 0x10u, node4);
    put(rt, body + 0x1F4u + 4u, 0u);
    put(rt, body + 0x1F4u + 0x14u, 0u);
    put(rt, body + 0x1F4u + 8u, 0u);
    put(rt, body + 0x1F4u + 0xCu, node4);
    put(rt, body + 0x32Cu, 0u);
    call(rt, ctx, 0x08B36018u, {body + 0x20Cu, 0u, 0x100u});
    call(rt, ctx, 0x08B36018u, {body + 0x30Cu, 0u, 0x20u});
    const u32 tail = body + 0x30Cu;  // s0
    for (u32 i = 0; i < 14u; ++i) {
        rt.memory().store8(tail + i, rt.memory().load8(0x08C9F754u + i));
    }
    for (u32 i = 0; i < 3u; ++i) {
        put(rt, body + 4u * i + 0x10u, 0u);
        put(rt, body + 4u * i + 0x1Cu, 0u);
    }
    for (u32 i = 0; i < 2u; ++i) {
        put(rt, body + 4u * i + 0x48u, 0u);
        put(rt, body + 4u * i + 0x50u, 0u);
        put(rt, body + 4u * i + 0x58u, block);
    }
    const u32 result = body + 0x330u;
    rt.memory().store16(result, 0u);
    rt.memory().store16(result + 4u, 0u);
    rt.memory().store16(result + 2u, 0u);
    put(rt, owner + 4u, body);
    ctx.gpr[2] = result;
}

// ---- 0x08BC12AC ----
// Class destructor: five refcounted members (0xA0, 0x98, 0x8C, 0x84, then 0x2C) released in that order, the vtable
// moving to the base's between the fourth and the fifth, and a sixth member (0x24) released on its +8 count only.
// v0 ends as the final vtable.
void func_08BC12AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved0(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;  // s0
    put(rt, self, 0x08CC5200u);
    release_object(rt, ctx, word(rt, self + 0xA0u));
    release_object(rt, ctx, word(rt, self + 0x98u));
    release_object(rt, ctx, word(rt, self + 0x8Cu));
    release_object(rt, ctx, word(rt, self + 0x84u));
    put(rt, self, 0x08CC5220u);
    release_object(rt, ctx, word(rt, self + 0x2Cu));
    release_object_second_count(rt, ctx, word(rt, self + 0x24u));
    put(rt, self, 0x08CC3000u);
    ctx.gpr[2] = 0x08CC3000u;
}

// ---- 0x089F1850 ----
// One-time registration (a0 == 1 and a1 == 0xFFFF): two registry calls, then ten 0x08C0B44C(descriptor, name,
// {word, word}, this) rows in order; v0 ends as the last row's result. The early exits keep the asm's v0: 1 for
// a0 != 1, 0xFFFF for a1 != 0xFFFF.
void func_089F1850(Runtime &rt, AllegrexContext &ctx) {
    if (ctx.gpr[4] != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (ctx.gpr[5] != 0xFFFFu) {
        ctx.gpr[2] = 0xFFFFu;
        return;
    }
    GuestFrame frame(ctx, 0x60u);
    call(rt, ctx, 0x08A92114u, {0x08CF58D8u, 0x08CA808Cu, 0x280u, 0x089F16F4u, 0x089F4500u, 0x089F1718u});
    call(rt, ctx, 0x0897D534u, {0x08CF58C8u, 0x08CA808Cu, 0x089F46C4u, 0x089F4680u});
    struct Row {
        u32 pair;
        u32 descriptor;
        u32 name;
        u32 stack_offset;
    };
    static constexpr Row rows[] = {
        {0x08C8D944u, 0x08CF5984u, 0x08CA809Cu, 0x48u}, {0x08C8D93Cu, 0x08CF5974u, 0x08CA80ACu, 0x40u},
        {0x08C8D934u, 0x08CF5964u, 0x08CA80BCu, 0x38u}, {0x08C8D92Cu, 0x08CF5954u, 0x08CA80CCu, 0x30u},
        {0x08C8D924u, 0x08CF5944u, 0x08CA80DCu, 0x28u}, {0x08C8D91Cu, 0x08CF5934u, 0x08CA80ECu, 0x20u},
        {0x08C8D914u, 0x08CF5924u, 0x08CA80FCu, 0x18u}, {0x08C8D90Cu, 0x08CF5914u, 0x08CA810Cu, 0x10u},
        {0x08C8D904u, 0x08CF5904u, 0x08CA811Cu, 0x8u},  {0x08C8D8FCu, 0x08CF58F4u, 0x08CA8128u, 0x0u},
    };
    for (const Row &row : rows) {
        const u32 first = word(rt, row.pair);
        const u32 second = word(rt, row.pair + 4u);
        put(rt, frame.at(row.stack_offset), first);
        put(rt, frame.at(row.stack_offset + 4u), second);
        call(rt, ctx, 0x08C0B44Cu, {row.descriptor, row.name, frame.at(row.stack_offset), 0x08CF58C8u});
    }
}

} // namespace
} // namespace new_batch_main_695
} // namespace gt::game

#define NEW695(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_695::name, gt::ReturnKind::kind)
NEW695(0x0888504Cu, func_0888504C, Word);
NEW695(0x088C7D50u, func_088C7D50, Word);
NEW695(0x08924690u, func_08924690, Word);
NEW695(0x08A6A6F0u, func_08A6A6F0, Word);
NEW695(0x0886DB78u, func_0886DB78, Word);
NEW695(0x08BC12ACu, func_08BC12AC, Word);
NEW695(0x089F1850u, func_089F1850, Word);
