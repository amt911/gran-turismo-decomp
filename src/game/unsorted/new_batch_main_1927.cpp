// New batch main 1927: functions drafted from tools.disasm (main session). Structured C: named typed locals,
// `G[n]` only at the call boundary, delay slots read as "before the call / branch", a branch-likely's delay slot
// only when the branch is taken, v0 as the original leaves it on every path. Draft batch, not verified by the
// differential gate; nothing here is claimed done until the gate passes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1927 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

// A guest call whose v0 / v1 / f0 stay in the context as the callee left them.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
inline s32 sext8(u32 value) { return static_cast<s32>(static_cast<std::int8_t>(value)); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }

// Releases one use of a shared object: use count (+4) down, then virtual slot 8; weak count (+8) down, then slot 0xC.
// The `ll/sc` pair is a plain read-modify-write here (single-threaded); true when the old count was 1.
inline bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}
inline void call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    const GuestResult result = call_guest(rt, ctx, function, {object}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
inline void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_slot(rt, ctx, object, 8u);
    if (count_down(rt, ctx, object + 8u)) call_slot(rt, ctx, object, 0xCu);
}

// func_08AB8DFC(blocks = a0, count = a1, floats = a2, stride = a3, out_abs = t0, out_ratio = t1): for each of the
// `count` pointers walks `floats >> 3` groups of eight floats (stride a3 * 32 between groups) and accumulates the
// square of their sum; every outer row adds its total to the running one (f15 is NOT reset per row) and the running
// maximum of the row totals is f16. Per group it also folds the pair min/max chains into f13 (max) and f14 (min);
// then out_abs = max(|f13|, |f14|) and out_ratio = sqrt(f16 / (float)(floats * 8)) (0 when that argument is negative).
void func_08AB8DFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 blocks = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 floats = ctx.gpr[6];
    const u32 stride = ctx.gpr[7] << 5;
    const u32 out_abs = ctx.gpr[8];
    const u32 out_ratio = ctx.gpr[9];
    GuestFrame frame(ctx, 0x10u);

    float f13 = 0.0f;
    float f14 = 0.0f;
    float f16 = 0.0f;
    if (count > 0) {
        const s32 groups = static_cast<s32>(floats) >> 3;
        float f15 = 0.0f;
        u32 row = blocks;
        s32 r = 0;
        do {
            const u32 block = word(rt, row);
            if (groups > 0) {
                u32 at = block;
                s32 g = 0;
                do {
                    const float x0 = float_at(rt, at + 0x00u);
                    const float x1 = float_at(rt, at + 0x04u);
                    const float x2 = float_at(rt, at + 0x08u);
                    const float x3 = float_at(rt, at + 0x0Cu);
                    const float x4 = float_at(rt, at + 0x10u);
                    const float x5 = float_at(rt, at + 0x14u);
                    const float x6 = float_at(rt, at + 0x18u);
                    const float x7 = float_at(rt, at + 0x1Cu);

                    const float f5 = (x0 < x1) ? x1 : x0;  // max(x0, x1)
                    const float f6 = (x0 < x1) ? x0 : x1;  // min(x0, x1)
                    float f4 = (x2 < x3) ? x2 : x3;        // min(x2, x3)
                    const float f8 = (f6 < f4) ? f6 : f4;  // min(x0..x3)
                    const float f7 = (f5 < f4) ? f4 : f5;  // max(x0..x3) with min(x2, x3)
                    const float e6 = (x4 < x5) ? x5 : x4;  // max(x4, x5)
                    float f5b = (x4 < x5) ? x4 : x5;       // min(x4, x5)
                    f4 = (x6 < x7) ? x6 : x7;              // min(x6, x7)
                    f5b = (f5b < f4) ? f5b : f4;           // min(min45, min67)
                    f4 = (e6 < f4) ? f4 : e6;              // max(max45, min67)
                    f14 = (f14 < f8) ? f14 : f8;           // min(f14, f8)
                    f13 = (f13 < f7) ? f7 : f13;           // max(f13, f7)
                    f14 = (f14 < f5b) ? f14 : f5b;         // min(f14, min45/67)
                    f13 = (f13 < f4) ? f4 : f13;           // max(f13, max45/67)

                    const float sum = ((x0 + x1) + (x2 + x3)) + ((x4 + x5) + (x6 + x7));
                    f15 += sum * sum;
                    at += stride;
                    g += 1;
                } while (g != groups);
            }
            if (f16 < f15) f16 = f15;
            row += 4u;
            r += 1;
        } while (r != count);
    }
    const float a = std::fabs(f13);
    const float b = std::fabs(f14);
    const float maxabs = (a < b) ? b : a;
    put_float(rt, out_abs, maxabs);

    const s32 scaled = static_cast<s32>(floats << 3);
    const float divisor = static_cast<float>(scaled);
    const GuestResult root = call_full(rt, ctx, 0x08B49D7Cu, {}, {f16 / divisor});
    ctx.fpr[0] = root.f0;
    put_float(rt, out_ratio, root.f0);
}

// func_089CC0E0(source = a0, destination = a1, limit = a2, width = a3, length = t0): a std::string-based line
// measurement (character widths from the byte table D_08C9316C). It builds the string through func_089CC040 and
// walks two counted byte runs (growing from `width`, then from t0 - width) while the accumulated width stays below
// the rep's length at data - 0xC; when the difference fits in `limit` the bytes are copied and the rep is released
// (func_08B60804 then func_08B8542C, skipped for the empty rep D_08D74FD0).
void func_089CC0E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 limit = ctx.gpr[6];
    const s32 width = static_cast<s32>(ctx.gpr[7]);
    const u32 length_arg = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    const u32 sp = ctx.gpr[29];
    constexpr u32 kEmptyRep = 0x08D74FD0u;
    constexpr u32 kWidthTable = 0x08C9316Cu;

    put8(rt, destination, 0u);
    call_full(rt, ctx, 0x089CC040u, {sp + 4u, source});
    const u32 data = word(rt, sp + 4u);
    const u32 rep = data - 0xCu;
    const s32 length = static_cast<s32>(word(rt, data - 0xCu));

    u32 cursor = data;
    u32 counter = 0;
    s32 t2 = 0;
    s32 t1 = 0;
    s32 v1 = 0;
    s32 a3v = 0;

    if (width > 0) {
        v1 = sext8(byte_u(rt, kWidthTable + byte_u(rt, data)));
        t2 = v1;
        if (v1 >= length) goto fail;
        counter = 1;
        for (;;) {
            if (counter == static_cast<u32>(width)) {
                t1 = t2;
                break;
            }
            cursor += static_cast<u32>(v1);
            v1 = sext8(byte_u(rt, kWidthTable + byte_u(rt, cursor)));
            t2 += v1;
            if (t2 >= length) goto fail;
            counter += 1;
        }
    }
    a3v = static_cast<s32>(length_arg - static_cast<u32>(width));
    if (a3v > 0) {
        v1 = sext8(byte_u(rt, kWidthTable + byte_u(rt, cursor)));
        t1 += v1;
        if (t1 >= length) goto fail;
        counter = 0;
        for (;;) {
            counter += 1;
            if (counter == static_cast<u32>(a3v)) break;
            cursor += static_cast<u32>(v1);
            v1 = sext8(byte_u(rt, kWidthTable + byte_u(rt, cursor)));
            t1 += v1;
            if (t1 >= length) goto fail;
        }
    }
    a3v = t1 - t2;
    if (signed_of(limit) < a3v) goto fail;
    if (a3v > 0) {
        u32 from = data + static_cast<u32>(t2);
        for (s32 i = 0; i != a3v; ++i) {
            put8(rt, destination + static_cast<u32>(i), byte_u(rt, from));
            from += 1u;
        }
    }

fail:
    if (rep == kEmptyRep) return;
    if (signed_of(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {rep, sp});
}

// func_08B00484(self = a0, owner = a1, position = a2, orientation = a3, extra = t0, source = t1, flags_lo = t2,
// flags_hi = t3, arg8 = stack +0x60, arg9 = stack +0x64, arg10 = stack +0x68): fills the object's transform,
// flags (a half at +0xE2 from the masked bytes), the -1 halves, and the three sub-objects at +0x80 / +0x8C; the
// orientation/position blocks go to func_08BAE278 -> func_08BAC284, then a further 8 calls. two flags are
// `movz`/`movn` defaults (0x10 only when t3 != 0, 0x20 only when arg8 == 0, 0x80 only when arg10 == 0).

// func_08876664(self = a0): fills five pairs of shared pointers (self + 4 .. + 0x28): each pair is a 0x38-byte
// object (vtable 0x08CBFCF8, the empty wide-string data 0x08D7529C at +0xC / +0x10 / +0x30) and a 0x10-byte
// control block (vtable 0x08CBFD78, counts 1 / 1) pointing back at it; func_08B8E380 links the pair, the previous
// pair is released. v0 = 5.
void func_08876664(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    constexpr u32 kEmptyData = 0x08D75298u + 0xCu;

    put(rt, self, 0u);
    for (u32 offset = 4u; offset <= 0x28u; offset += 4u) put(rt, self + offset, 0u);

    u32 pair = self + 4u;
    u32 index = 0;
    do {
        const u32 object = call(rt, ctx, 0x08B62344u, {0x38u});
        put(rt, object, 0x08CBFD38u);
        put(rt, object + 0x04u, 0u);
        put(rt, object + 0x08u, 0u);
        put(rt, object + 0x0Cu, kEmptyData);
        put(rt, object + 0x10u, kEmptyData);
        put(rt, object + 0x14u, 0u);
        put(rt, object + 0x18u, 0u);
        put(rt, object + 0x1Cu, 0u);
        put(rt, object + 0x20u, 0u);
        put8(rt, object + 0x24u, 0u);
        put8(rt, object + 0x25u, 0u);
        const u32 flags = word(rt, object + 0x24u);
        put(rt, object, 0x08CBFCF8u);
        put(rt, object + 0x24u, flags & ~0x00010000u);  // ins v0, zero, 16, 1
        put(rt, object + 0x30u, kEmptyData);

        put(rt, frame.at(0u), object);
        const u32 control = call(rt, ctx, 0x08B62344u, {0x10u});
        put(rt, control, 0x08CBFD78u);
        put(rt, control + 0x04u, 1u);
        put(rt, control + 0x08u, 1u);
        put(rt, control + 0x0Cu, object);
        put(rt, frame.at(4u), control);
        call_full(rt, ctx, 0x08B8E380u, {frame.at(4u), object, object});

        const u32 old_object = word(rt, pair);
        const u32 old_control = word(rt, pair + 4u);
        put(rt, pair, object);
        put(rt, pair + 4u, control);
        put(rt, frame.at(0u), old_object);
        put(rt, frame.at(4u), old_control);
        if (old_control != 0u) {
            if (count_down(rt, ctx, old_control + 4u)) {
                call_slot(rt, ctx, old_control, 8u);
                if (count_down(rt, ctx, old_control + 8u)) call_slot(rt, ctx, old_control, 0xCu);
            }
        }
        index += 1;
        pair += 8u;
    } while (index != 5u);
    ctx.gpr[2] = 5u;
}

// func_08B905CC(self = a0): destructor of the shared-pointer list: func_08B8FE44(self + 0x20 word, self + 0x24 word,
// frame) runs first, func_08B35254 frees the +0x20 object, then the four owned controls at +0x1C / +0x14 / +0xC / +4
// are released in that order. Return value left as the original leaves it.
void func_08B905CC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 sp = frame.at(0u);

    call_full(rt, ctx, 0x08B8FE44u, {word(rt, self + 0x20u), word(rt, self + 0x24u), sp});
    const u32 object = word(rt, self + 0x20u);
    if (object != 0u) call_full(rt, ctx, 0x08B35254u, {object});

    u32 owned = word(rt, self + 0x1Cu);
    if (owned != 0u) release_shared(rt, ctx, owned);
    owned = word(rt, self + 0x14u);
    if (owned != 0u) release_shared(rt, ctx, owned);
    owned = word(rt, self + 0x0Cu);
    if (owned != 0u) release_shared(rt, ctx, owned);
    owned = word(rt, self + 0x04u);
    if (owned != 0u) release_shared(rt, ctx, owned);
}

// func_088A09DC(self = a0, first = a1, second = a2, t = f12): constructor. The two pointers default to each other
// (movz), 1 - t and t go to +0x10 / +0x14, four 0x1C-byte rows at +0x18 with vtable 0x08CC2C68 and four 0x14-byte
// rows at +0x88 with vtable 0x08CC3630. Four rounds then fill each row pair from func_088A8B14 (a0 = frame +0x14,
// the row's two sources at + (0x170 + i * 0xB8) + 8, f12 = t). v0 = 4.
void func_088A09DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first_arg = ctx.gpr[5];
    const u32 second_arg = ctx.gpr[6];
    const float t = ctx.fpr[12];
    GuestFrame frame(ctx, 0x70u);
    const u32 sp = frame.at(0u);

    const float one_minus_t = 1.0f - t;
    const float f20 = t;
    const u32 first = (first_arg == 0u) ? second_arg : first_arg;
    const u32 second = (second_arg == 0u) ? first_arg : second_arg;

    put(rt, self, 0x08CC2590u);
    put(rt, self + 0x04u, 0x08CC26B4u);
    put(rt, self + 0x0Cu, second);
    put(rt, self + 0x08u, first);
    put_float(rt, self + 0x10u, one_minus_t);
    put_float(rt, self + 0x14u, t);

    u32 row = self + 0x18u;
    for (u32 i = 0; i != 4u; ++i, row += 0x1Cu) {
        put(rt, row, 0x08CC2C68u);
        put(rt, row + 0x14u, 0x08CC4110u);
        put(rt, row + 0x18u, 0u);
    }
    u32 small = self + 0x88u;
    for (u32 i = 0; i != 4u; ++i, small += 0x14u) put(rt, small, 0x08CC3630u);

    u32 source_offset = 0x170u;
    u32 aux_offset = 0x450u;
    u32 row2 = self + 0x18u;
    u32 small2 = self + 0x88u;
    for (u32 i = 0; i != 4u; ++i) {
        const u32 source1 = word(rt, self + 0x08u) + source_offset + 8u;
        const u32 source2 = word(rt, self + 0x0Cu) + source_offset + 8u;
        call_full(rt, ctx, 0x088A8B14u, {sp + 0x14u, source1, source2}, {f20});

        put(rt, row2 + 0x04u, word(rt, sp + 0x18u));
        put_float(rt, row2 + 0x0Cu, float_at(rt, sp + 0x20u));
        put(rt, row2 + 0x08u, word(rt, sp + 0x1Cu));
        put_float(rt, row2 + 0x10u, float_at(rt, sp + 0x24u));
        put(rt, row2 + 0x18u, word(rt, sp + 0x2Cu));
        row2 += 0x1Cu;

        const u32 cand1 = word(rt, self + 0x08u) + aux_offset + 8u;
        const u32 cand2 = word(rt, self + 0x0Cu) + aux_offset + 8u;
        const u32 chosen1 = (cand1 == 0u) ? cand2 : cand1;  // movz
        const u32 chosen2 = (cand2 != 0u) ? cand2 : cand1;  // movn
        put(rt, small2 + 0x04u, chosen1);
        put(rt, small2 + 0x08u, chosen2);
        put_float(rt, small2 + 0x0Cu, one_minus_t);
        put_float(rt, small2 + 0x10u, f20);
        small2 += 0x14u;

        put(rt, sp + 0x28u, 0x08CC2760u);
        put(rt, sp + 0x00u, 0x08CC2790u);
        put(rt, sp + 0x04u, chosen1);
        put(rt, sp + 0x08u, chosen2);
        put_float(rt, sp + 0x0Cu, one_minus_t);
        put_float(rt, sp + 0x10u, f20);
        put(rt, sp + 0x14u, 0x08CBF428u);

        source_offset += 0xB8u;
        aux_offset += 0x24u;
    }
    ctx.gpr[2] = 4u;
}

// func_08912A10(self = a0): constructor. func_08B28DCC(self), the flag bytes / words, four func_08AB9284 rows at
// +0x44 / +0x68 / +0x8C / +0xB0, five func_0891156C blocks at +0x120 / +0x138 / +0x150 / +0x168 / +0x180 (f12 = 0),
// 1.0f at +0x114 / +0x118 / +0x1A8 / +0x7B0, sixteen zeroed 0x40-byte rows at +0x800 and +0x11C cleared. v0 = 0.
void func_08912A10(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);

    call_full(rt, ctx, 0x08B28DCCu, {self});
    put8(rt, self + 0x2Eu, 0xFFu);
    put8(rt, self + 0x28u, 0u);
    put8(rt, self + 0x2Bu, 0u);
    put8(rt, self + 0x29u, 0u);
    put8(rt, self + 0x2Cu, 0u);
    put8(rt, self + 0x2Du, 0u);
    put8(rt, self + 0x2Fu, 0u);
    put8(rt, self + 0x30u, 0u);
    put8(rt, self + 0x31u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x40u, 0u);
    call_full(rt, ctx, 0x08AB9284u, {self + 0x44u});
    call_full(rt, ctx, 0x08AB9284u, {self + 0x68u});
    call_full(rt, ctx, 0x08AB9284u, {self + 0x8Cu});
    call_full(rt, ctx, 0x08AB9284u, {self + 0xB0u});

    for (u32 offset = 0xD4u; offset <= 0x110u; offset += 4u) put(rt, self + offset, 0u);
    put_float(rt, self + 0x114u, 1.0f);
    put_float(rt, self + 0x118u, 1.0f);
    call_full(rt, ctx, 0x0891156Cu, {self + 0x120u}, {0.0f});
    call_full(rt, ctx, 0x0891156Cu, {self + 0x138u}, {0.0f});
    call_full(rt, ctx, 0x0891156Cu, {self + 0x150u}, {0.0f});
    call_full(rt, ctx, 0x0891156Cu, {self + 0x168u}, {0.0f});
    call_full(rt, ctx, 0x0891156Cu, {self + 0x180u}, {0.0f});
    put_float(rt, self + 0x1A8u, 1.0f);
    put(rt, self + 0x198u, 0u);
    put(rt, self + 0x19Cu, 0u);
    put(rt, self + 0x1A0u, 0u);
    put(rt, self + 0x1A4u, 0u);
    put8(rt, self + 0x1ACu, 0u);
    // The original spins a two-decrement delay loop (0x08912B28) here; it has no side effect.

    put_float(rt, self + 0x7B0u, 1.0f);
    for (u32 offset = 0x7B4u; offset <= 0x7F8u; offset += 4u) put(rt, self + offset, 0u);
    for (u32 block = 0u; block != 4u; ++block) {
        const u32 base = self + 0x800u + block * 0x40u;
        for (u32 offset = 0u; offset <= 0x34u; offset += 4u) put(rt, base + offset, 0u);
    }
    put(rt, self + 0x11Cu, 0u);
    ctx.gpr[2] = 0u;
}

// func_089763C0(self = a0, stream = a1): compares the string at self + 4 with the literal D_08CA2E30 (both built by
// func_08B88F44 / func_08B85EB8, compared by func_08B36170 with the min length; the result falls back to the length
// difference when func_08B36170 answers 0), releases the two temporaries, and unless they differ parses three floats
// with func_08987A74 / func_08B77BF4 / func_08B71EC8, stopping at -1 or code 0x7D, and writes them to the block at
// self + 0xC. Return value left as the original leaves it.
void func_089763C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = frame.at(0u);
    constexpr u32 kEmptyRep = 0x08D74FD0u;

    call_full(rt, ctx, 0x08B88F44u, {sp + 0x08u, self + 0x04u});
    call_full(rt, ctx, 0x08B85EB8u, {sp + 0x04u, 0x08CA2E30u, sp + 0x03u});
    const u32 text_a = word(rt, sp + 0x08u);
    const u32 text_b = word(rt, sp + 0x04u);
    const u32 length_a = word(rt, text_a - 0x0Cu);
    const u32 length_b = word(rt, text_b - 0x0Cu);
    put(rt, sp + 0x0Cu, length_a);
    put(rt, sp + 0x10u, length_b);
    const u32 shorter = (length_b < length_a) ? length_b : length_a;
    const GuestResult compared = call_full(rt, ctx, 0x08B36170u, {text_a, text_b, shorter});
    s32 difference = static_cast<s32>(length_a - length_b);
    const u32 rep_b = text_b - 0x0Cu;
    if (rep_b == kEmptyRep) {
        if (compared.v0 != 0u) difference = static_cast<s32>(compared.v0);
    } else if (signed_of(call(rt, ctx, 0x08B60804u, {text_b - 0x04u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {rep_b, sp + 0x02u});
    }
    const u32 rep_a = text_a - 0x0Cu;
    if (rep_a != kEmptyRep) {
        if (signed_of(call(rt, ctx, 0x08B60804u, {text_a - 0x04u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {rep_a, sp + 0x01u});
    }
    if (difference != 0) return;

    put(rt, sp + 0x14u, kEmptyRep + 0x0Cu);
    call_full(rt, ctx, 0x08987A74u, {stream, sp + 0x14u});
    constexpr u32 kDefaultTriple = 0x08C8BD90u;
    put(rt, sp + 0x18u, word(rt, kDefaultTriple + 0x00u));
    put(rt, sp + 0x1Cu, word(rt, kDefaultTriple + 0x04u));
    put(rt, sp + 0x20u, word(rt, kDefaultTriple + 0x08u));

    u32 slot = sp + 0x18u;
    const u32 slots_end = sp + 0x24u;
    for (;;) {
        const u32 code = call(rt, ctx, 0x08987A74u, {stream, sp + 0x14u});
        if (code == 0xFFFFFFFFu) {
            call_full(rt, ctx, 0x0898682Cu, {stream, 1u});
            break;
        }
        if (code == 0x7Du) break;
        const GuestResult number = call_full(rt, ctx, 0x08B77BF4u, {word(rt, sp + 0x14u)});
        const GuestResult value = call_full(rt, ctx, 0x08B71EC8u, {number.v0, number.v1});
        put_float(rt, slot, value.f0);
        slot += 4u;
        if (slot == slots_end) {
            call_full(rt, ctx, 0x0898682Cu, {stream, 1u});
            break;
        }
    }

    put_float(rt, word(rt, self + 0x0Cu), float_at(rt, sp + 0x18u));
    put_float(rt, word(rt, self + 0x0Cu) + 0x04u, float_at(rt, sp + 0x1Cu));
    put_float(rt, word(rt, self + 0x0Cu) + 0x08u, float_at(rt, sp + 0x20u));

    const u32 parsed = word(rt, sp + 0x14u);
    const u32 parsed_rep = parsed - 0x0Cu;
    if (parsed_rep != kEmptyRep) {
        if (signed_of(call(rt, ctx, 0x08B60804u, {parsed - 0x04u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {parsed_rep, sp});
    }
}

// func_08907E44(self = a0, results = a1, limit = a2, first = a3, f12 / f13 / f14): grid walk. func_08907D64(self,
// first, f12) gives the base index, func_08907D5C(self, first) the span and func_08907D6C(self, first, f12) the start
// step; the cell table is at self + 0x30 with 0x30-byte cells. The backward walk (+ a3) then the forward acceptance
// walk (a1) fill `results` (s5 + 4 * n) with matching cell pointers; v0 = the number stored.
void func_08907E44(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 results = ctx.gpr[5];
    const u32 limit = ctx.gpr[6];
    const u32 first = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);

    const float f21_init = ctx.fpr[12];
    const float f20 = ctx.fpr[13];
    const float f22 = ctx.fpr[14];
    float f21 = f21_init;

    const GuestResult r_base = call_full(rt, ctx, 0x08907D64u, {self, first}, {f21_init});
    const GuestResult r_span = call_full(rt, ctx, 0x08907D5Cu, {self, first});
    const GuestResult r_start = call_full(rt, ctx, 0x08907D6Cu, {self, first}, {f21_init});
    const u32 base = r_base.v0;
    const u32 span = r_span.v0;
    u32 a2v = r_start.v0;

    float f1 = psp_mul(f22, 0.25f);
    if (!(f1 <= f20)) f1 = f20;
    u32 t1 = 0;
    if (signed_of(a2v) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 t0 = word(rt, self + 0x30u);
    u32 a1v = base + a2v;
    u32 a0v = a1v;
    u32 a3v = a2v;
    u32 output = results;
    float f2 = float_at(rt, t0 + ((a0v << 6) - (a0v << 4)) + 0x10u);
    float f3 = f21;

    for (;;) {
        {
            const u32 cell = t0 + ((a0v << 6) - (a0v << 4));
            const float value = float_at(rt, cell + 0x10u);
            a3v -= 1u;
            if ((f2 - value) < f1 && signed_of(t1) < signed_of(limit)) {
                if (signed_of(a3v) < 0) {
                    f2 = f2 + f22;
                    f3 = f3 + f22;
                    a3v += span;
                }
                a0v = a3v + base;
                const u32 cell2 = t0 + ((a0v << 6) - (a0v << 4));
                if (float_at(rt, cell2 + 0x10u) <= f3) {
                    const u32 offset = a0v << 4;
                    if (f3 < float_at(rt, offset + 0x14u)) {
                        put(rt, output, offset);
                        t1 += 1u;
                        output += 4u;
                        t0 = word(rt, self + 0x30u);
                    }
                }
                continue;
            }
        }
        for (;;) {
            const u32 cell = t0 + ((a1v << 6) - (a1v << 4));
            const float low = float_at(rt, cell + 0x10u);
            a2v += 1u;
            const bool more = signed_of(a2v) < signed_of(span);
            if (!(low <= f21)) {
                ctx.gpr[2] = t1;
                return;
            }
            if (!(signed_of(t1) < signed_of(limit))) {
                ctx.gpr[2] = t1;
                return;
            }
            if (f21 < float_at(rt, cell + 0x14u)) {
                put(rt, results + (t1 << 2), cell);
                t1 += 1u;
                t0 = word(rt, self + 0x30u);
            }
            if (!more) {
                f21 = f21 - f22;
                a2v -= span;
                a1v = base + a2v;
            }
        }
    }
}

// func_08C45068(): the one-time registration guards of the six typeinfo blocks D_08D74F78, D_08D74F98, D_08D74FA8,
// D_08D74FC8 (inside D_08D74FA8's), D_08D74FA0, D_08D74F90 and D_08D74F70: each byte guard, then func_08B6441C and
// func_08B64494, and the block at D_08D7729C / D_08D772AC / D_08D772D8 / D_08D77300 gets its vtable word (+8), its
// next word (+4) and a zero. v0 = D_08D77300 on the way out.
void func_08C45068(Runtime &rt, AllegrexContext &ctx) {
    if (sext8(byte_u(rt, 0x08D74F78u)) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D74F78u}) != 0u) {
        if (sext8(byte_u(rt, 0x08D74F98u)) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D74F98u}) != 0u) {
            if (sext8(byte_u(rt, 0x08D74FA8u)) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D74FA8u}) != 0u) {
                if (sext8(byte_u(rt, 0x08D74FC8u)) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D74FC8u}) != 0u) {
                    put(rt, 0x08D7729Cu, 0u);
                    put(rt, 0x08D772A4u, 0x08CD7988u);
                    put(rt, 0x08D772A0u, 0x08D772A8u);
                    call(rt, ctx, 0x08B64494u, {0x08D74FC8u});
                }
                put(rt, 0x08D772B8u, 0x08D7729Cu);
                call(rt, ctx, 0x08B64494u, {0x08D74FA8u});
            }
            if (sext8(byte_u(rt, 0x08D74FA0u)) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D74FA0u}) != 0u) {
                put(rt, 0x08D772ACu, 0u);
                put(rt, 0x08D772B4u, 0x08CD7988u);
                put(rt, 0x08D772B0u, 0x08D772B8u);
                call(rt, ctx, 0x08B64494u, {0x08D74FA0u});
            }
            put(rt, 0x08D772E4u, 0x08D772ACu);
            call(rt, ctx, 0x08B64494u, {0x08D74F98u});
        }
        if (sext8(byte_u(rt, 0x08D74F90u)) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D74F90u}) != 0u) {
            put(rt, 0x08D772D8u, 0u);
            put(rt, 0x08D772E0u, 0x08CD7994u);
            put(rt, 0x08D772DCu, 0x08D772E4u);
            call(rt, ctx, 0x08B64494u, {0x08D74F90u});
        }
        put(rt, 0x08D7730Cu, 0x08D772D8u);
        call(rt, ctx, 0x08B64494u, {0x08D74F78u});
    }
    if (sext8(byte_u(rt, 0x08D74F70u)) == 0 && call(rt, ctx, 0x08B6441Cu, {0x08D74F70u}) != 0u) {
        put(rt, 0x08D77300u, 0u);
        put(rt, 0x08D77308u, 0x08CD799Cu);
        put(rt, 0x08D77304u, 0x08D7730Cu);
        call(rt, ctx, 0x08B64494u, {0x08D74F70u});
    }
    ctx.gpr[2] = 0x08D77300u;
}

// func_08950438(self = a0, flag_a = a1, flag_b = a2, flag_c = a3, state = t0): reads the block at (self + 0xC) +
// 0x270, runs func_08949E8C(self) (its v0 is the time), func_0895534C (count), func_08955960 (a float) and
// func_0895596C (another float), then sets a state word and picks one of three byte flags from time windows of
// 1000 / 300 / 500 / 1000 units and the two floats (0.1f / 0.7f). v0 = 1 when a flag was set on the last test, else 0.

} // namespace
} // namespace new_batch_main_1927
} // namespace gt::game

#define NEW1927(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1927::name, gt::ReturnKind::kind)
NEW1927(0x08AB8DFCu, func_08AB8DFC, Float);
NEW1927(0x089CC0E0u, func_089CC0E0, Void);
NEW1927(0x08876664u, func_08876664, Word);
NEW1927(0x08B905CCu, func_08B905CC, Void);
NEW1927(0x088A09DCu, func_088A09DC, Word);
NEW1927(0x08912A10u, func_08912A10, Word);
NEW1927(0x089763C0u, func_089763C0, Void);
NEW1927(0x08907E44u, func_08907E44, Word);
NEW1927(0x08C45068u, func_08C45068, Word);
