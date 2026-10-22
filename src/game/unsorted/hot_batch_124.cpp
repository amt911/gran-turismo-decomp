// Hot batch 124: functions of 400-700 instructions the 11-route gate reaches, written by hand from `tools.disasm`;
// role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <optional>

namespace gt::game {
namespace hot_batch_124 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// The same with float arguments in f12-f15 as well.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
inline std::uint32_t call_virtual_at(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t slot,
                                     std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
// The Allegrex div's quotient: -1 (numerator >= 0) or 1 when dividing by zero, 0x80000000 for 0x80000000 / -1.
inline std::int32_t allegrex_div(std::int32_t numerator, std::int32_t denominator) {
    if (denominator == 0) return numerator >= 0 ? -1 : 1;
    if (numerator == INT32_MIN && denominator == -1) return INT32_MIN;
    return numerator / denominator;
}

// The lock pair several of these wrap their body in: func_08B344E0(lock) before, a tail jump to func_08B3457C(lock) after.
constexpr std::uint32_t kLock = 0x08B344E0u;
constexpr std::uint32_t kUnlock = 0x08B3457Cu;
// The value-object pair of the 0x08A5xxxx library: func_08A5BAD8(dst, src->+0) assigns, func_08A5B9EC(object) destroys.
constexpr std::uint32_t kAssign = 0x08A5BAD8u;
constexpr std::uint32_t kDestroy = 0x08A5B9ECu;

// The end of a function that built a value object in its frame at `temporary`: assigned to `target` (skipped when the
// target is that temporary itself), then the temporary is destroyed.
inline void keep_temporary(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t temporary) {
    if (target != temporary) call_set(rt, ctx, kAssign, {target, word(rt, temporary)});
    call_set(rt, ctx, kDestroy, {temporary});
}

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep `empty`, func_08B60804(&count, -1) steps the count and
// `release`(rep, frame) frees it when the old count was not positive.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t empty,
                           std::uint32_t release, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == empty) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, release, {rep, frame_base});
}
} // namespace


// func_08A0B264(self, lexer): reads one colour attribute of a style (self+4 its name, self+0xC the colour {r, g, b, a,
// mode +0x10}). The name is compared with four literals (a string copy func_08B88F44 against a string built by
// func_08B85EB8, memcmp func_08B36170 over the shorter length, then the length difference; both released like
// release_string). 0x08CA924C reads up to four integers (tokens of func_08987A74 through atoi func_08B77BFC, defaults
// from 0x08C8DF1C) clamped to 0..255 and scaled by 1/255; 0x08CA9254 up to four floats (func_08B77BF4, narrowed by
// func_08B71EC8, defaults 0x08C8DF0C); both set mode -1 and stop early at token 0x7D. 0x08CA925C takes a named colour
// token (0x102) through func_08A0B0CC / func_089641C4, 0x08CA9268 a palette index token (0x101) through func_089F9ED4
// first. The lexer is closed with func_0898682C(lexer, 1) except after token 0x7D.
void func_08A0B264(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t lexer = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    constexpr std::uint32_t kEmpty = 0x08D74FD0u;
    constexpr std::uint32_t kRelease = 0x08B8542Cu;
    const std::uint32_t token = frame.at(0x40u);
    // name == literal; the temporaries live at sp+copy / sp+text, released with the allocator bytes at the given offsets
    const auto named = [&](std::uint32_t copy, std::uint32_t text, std::uint32_t literal, std::uint32_t scratch,
                           std::uint32_t free_text, std::uint32_t free_copy) {
        call_set(rt, ctx, 0x08B88F44u, {frame.at(copy), self + 4u});
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(text), literal, frame.at(scratch)});
        const std::uint32_t a = word(rt, frame.at(copy));
        const std::uint32_t b = word(rt, frame.at(text));
        const std::uint32_t length_a = word(rt, a - 0xCu);
        const std::uint32_t length_b = word(rt, b - 0xCu);
        std::uint32_t difference = length_a - length_b;
        const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {a, b, length_b < length_a ? length_b : length_a, length_a});
        if (compared != 0u) difference = compared;
        release_string(rt, ctx, frame.at(text), kEmpty, kRelease, frame.at(free_text));
        release_string(rt, ctx, frame.at(copy), kEmpty, kRelease, frame.at(free_copy));
        return difference == 0u;
    };
    const auto next = [&] { return call_set(rt, ctx, 0x08987A74u, {lexer, token}); };
    // up to four values at sp+0x44 (defaults copied from `defaults`): the first token is read and dropped, then each
    // token until -1, 0x7D (the lexer stays open) or the fourth value goes through `read`
    const auto read_values = [&](std::uint32_t defaults, const auto &read) {
        const std::uint32_t values = frame.at(0x44u);
        next();
        for (std::uint32_t k = 0u; k < 16u; k += 4u) put(rt, values + k, word(rt, defaults + k));
        for (std::uint32_t slot = 0u; slot != 16u; slot += 4u) {
            const std::uint32_t kind = next();
            if (kind == 0xFFFFFFFFu) break;
            if (kind == 0x7Du) return values;
            read(values + slot);
        }
        call_set(rt, ctx, 0x0898682Cu, {lexer, 1u});
        return values;
    };
    const auto store = [&](std::uint32_t colour, const float (&channel)[4]) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        put(rt, colour + 0x10u, ctx.gpr[2]);
        for (std::uint32_t k = 0u; k < 4u; ++k) put_float(rt, colour + k * 4u, channel[k]);
    };
    if (named(0x34u, 0x20u, 0x08CA924Cu, 0x10u, 0xFu, 0xEu)) {
        put(rt, token, kEmpty + 0xCu);
        const std::uint32_t values = read_values(0x08C8DF1Cu, [&](std::uint32_t slot) {
            put(rt, slot, call_set(rt, ctx, 0x08B77BFCu, {word(rt, token)}));
        });
        float channel[4];
        for (std::uint32_t k = 0u; k < 4u; ++k) {
            const std::int32_t value = signed_of(word(rt, values + k * 4u));
            const float level = value < 0 ? 0.0f : (value < 0x100 ? static_cast<float>(value) : 255.0f);
            channel[k] = level / 255.0f;
        }
        store(word(rt, self + 0xCu), channel);
        release_string(rt, ctx, token, kEmpty, kRelease, frame.at(0xDu));
        return;
    }
    if (named(0x30u, 0x1Cu, 0x08CA9254u, 0xCu, 0xBu, 0xAu)) {
        put(rt, token, kEmpty + 0xCu);
        const std::uint32_t values = read_values(0x08C8DF0Cu, [&](std::uint32_t slot) {
            const std::uint32_t low = call_set(rt, ctx, 0x08B77BF4u, {word(rt, token)});
            put_float(rt, slot, call_setf(rt, ctx, 0x08B71EC8u, {low, ctx.gpr[3]}, {}).f0);
        });
        float channel[4];
        for (std::uint32_t k = 0u; k < 4u; ++k) channel[k] = float_at(rt, values + k * 4u);
        store(word(rt, self + 0xCu), channel);
        release_string(rt, ctx, token, kEmpty, kRelease, frame.at(9u));
        return;
    }
    if (named(0x2Cu, 0x18u, 0x08CA925Cu, 8u, 7u, 6u)) {
        put(rt, token, kEmpty + 0xCu);
        next();
        if (next() == 0x102u) {
            const std::uint32_t colour = word(rt, self + 0xCu);
            const std::uint32_t parsed = frame.at(0x80u);
            call_set(rt, ctx, 0x08A0B0CCu, {parsed, token});
            if (colour != parsed) {
                call_set(rt, ctx, 0x089641C4u, {colour, parsed});
                put(rt, colour + 0x10u, word(rt, parsed + 0x10u));
            }
        }
        call_set(rt, ctx, 0x0898682Cu, {lexer, 1u});
        release_string(rt, ctx, token, kEmpty, kRelease, frame.at(5u));
        return;
    }
    if (named(0x28u, 0x14u, 0x08CA9268u, 4u, 3u, 2u)) {
        put(rt, token, kEmpty + 0xCu);
        next();
        if (next() == 0x101u) {
            const std::uint32_t colour = word(rt, self + 0xCu);
            const std::uint32_t index = call_set(rt, ctx, 0x08B77BFCu, {word(rt, token)});
            const std::uint32_t name = frame.at(0x24u);
            call_set(rt, ctx, 0x089F9ED4u, {name, index});
            const std::uint32_t parsed = frame.at(0x60u);
            call_set(rt, ctx, 0x08A0B0CCu, {parsed, name});
            if (colour != parsed) {
                call_set(rt, ctx, 0x089641C4u, {colour, parsed});
                put(rt, colour + 0x10u, word(rt, parsed + 0x10u));
            }
            release_string(rt, ctx, name, kEmpty, kRelease, frame.at(1u));
        }
        call_set(rt, ctx, 0x0898682Cu, {lexer, 1u});
        release_string(rt, ctx, token, kEmpty, kRelease, frame.at(0u));
    }
}


// func_089A9654(drawer, uv_source, colour_source, f12 = width, f13 = height, f14 = radius): draws a rectangle, rounded
// when radius > 0, through the drawer's slot 0x10C(drawer, mode, count, points, uvs or 0, colours or 0). The corner
// table (0x08CF4B08 -> 0x08CF4B0C: (0, 0), then func_08B662C4 / func_08B66524 of 0 .. 7 pi/16, then (0, 1)) is built
// once under the guard 0x08CF4AF8 (and a second, empty guard 0x08CF4B00). A rounded rectangle is four 10-point fans
// (corner centres inset by the radius, clamped to half the smaller side) plus the 14- or 6-point fill of the middle,
// each point's uv / colour sampled by func_089A900C / func_089A9064 at its position over the size (mode word
// 0x08C8CC10). A square one is a 6-point fan with the source's four uvs / colours (func_089641C4) around a sampled centre,
// or a 4-point strip (mode 0x08C8CC14) without colours.
void func_089A9654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t drawer = ctx.gpr[4];
    const std::uint32_t uv_source = ctx.gpr[5];
    const std::uint32_t colour_source = ctx.gpr[6];
    const float width = ctx.fpr[12];
    const float height = ctx.fpr[13];
    float radius = ctx.fpr[14];
    if (width <= 0.0f || height <= 0.0f) return;
    GuestFrame frame(ctx, 0x260u);
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto draw = [&](std::uint32_t mode, std::uint32_t count, std::uint32_t points, std::uint32_t uvs, std::uint32_t colours) {
        const std::uint32_t table = word(rt, drawer);
        ctx.gpr[2] = table;
        call_set(rt, ctx, word(rt, table + 0x10Cu), {drawer, word(rt, mode), count, points, uvs, colours});
    };
    const auto sample_uv = [&](std::uint32_t out, float x, float y) {
        call_setf(rt, ctx, 0x089A900Cu, {out, uv_source}, {x / width, y / height});
    };
    const auto sample_colour = [&](std::uint32_t out, float x, float y) {
        call_setf(rt, ctx, 0x089A9064u, {out, colour_source}, {x / width, y / height});
    };
    const float half_width = psp_mul(width, 0.5f);
    const float half_height = psp_mul(height, 0.5f);
    if (!(0.0f < radius)) {
        if (colour_source != 0u) {
            const float fan[12] = {half_width, half_height, 0.0f, 0.0f, width, 0.0f, width, height, 0.0f, height, 0.0f, 0.0f};
            for (std::uint32_t k = 0u; k < 12u; ++k) put_float(rt, sp(0xA0u + k * 4u), fan[k]);
            for (std::uint32_t k = 0u; k < 0x60u; k += 4u) put_float(rt, sp(0x30u + k), 1.0f);
            if (uv_source != 0u) {
                sample_uv(sp(0u), f(sp(0xA0u)), f(sp(0xA4u)));
                for (std::uint32_t k = 0u; k < 0x20u; k += 4u) put_float(rt, sp(8u + k), f(uv_source + k));
                put_float(rt, sp(0x28u), f(uv_source));
                put_float(rt, sp(0x2Cu), f(uv_source + 4u));
            }
            sample_colour(sp(0x30u), f(sp(0xA0u)), f(sp(0xA4u)));
            for (std::uint32_t k = 0u; k < 4u; ++k) call_set(rt, ctx, 0x089641C4u, {sp(0x40u + k * 0x10u), colour_source + k * 0x10u});
            call_set(rt, ctx, 0x089641C4u, {sp(0x80u), colour_source});
            draw(0x08C8CC10u, 6u, sp(0xA0u), uv_source != 0u ? sp(0u) : 0u, sp(0x30u));
            return;
        }
        const float strip[8] = {0.0f, 0.0f, width, 0.0f, 0.0f, height, width, height};
        for (std::uint32_t k = 0u; k < 8u; ++k) put_float(rt, sp(k * 4u), strip[k]);
        if (uv_source != 0u) {
            constexpr std::uint32_t kOrder[8] = {0u, 4u, 8u, 0xCu, 0x18u, 0x1Cu, 0x10u, 0x14u};
            for (std::uint32_t k = 0u; k < 8u; ++k) put_float(rt, sp(0xA0u + k * 4u), f(uv_source + kOrder[k]));
            draw(0x08C8CC14u, 4u, sp(0u), sp(0xA0u), 0u);
        } else {
            draw(0x08C8CC14u, 4u, sp(0u), 0u, 0u);
        }
        return;
    }
    const float limit = half_width < half_height ? half_width : half_height;
    if (!(radius < limit)) radius = limit;
    const float inner_width = width - radius;
    const float inner_height = height - radius;
    if (static_cast<std::int8_t>(byte_u(rt, 0x08CF4B00u)) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08CF4B00u}) != 0u) {
        call_set(rt, ctx, 0x08B64494u, {0x08CF4B00u});
    }
    if (static_cast<std::int8_t>(byte_u(rt, 0x08CF4AF8u)) == 0 && call_set(rt, ctx, 0x08B6441Cu, {0x08CF4AF8u}) != 0u) {
        constexpr std::uint32_t kTable = 0x08CF4B0Cu;
        put_float(rt, kTable + 4u, 0.0f);
        put_float(rt, kTable, 0.0f);
        float angle = 0.0f;
        for (std::uint32_t entry = kTable + 8u; entry != kTable + 0x48u; entry += 8u) {
            const float second = call_setf(rt, ctx, 0x08B66524u, {}, {angle}).f0;
            const float first = call_setf(rt, ctx, 0x08B662C4u, {}, {angle}).f0;
            angle = angle + std::bit_cast<float>(0x3E490FD8u);
            put_float(rt, entry, first);
            put_float(rt, entry + 4u, second);
        }
        put_float(rt, kTable + 0x4Cu, 1.0f);
        put(rt, kTable + 0x48u, 0u);
        put(rt, 0x08CF4B08u, kTable);
        call_set(rt, ctx, 0x08B64494u, {0x08CF4AF8u});
    }
    const std::uint32_t colours = sp(0x110u);
    for (std::uint32_t k = 0u; k < 0xE0u; k += 4u) put_float(rt, colours + k, 1.0f);
    const float centres[8] = {radius, radius, inner_width, radius, inner_width, inner_height, radius, inner_height};
    for (std::uint32_t k = 0u; k < 8u; ++k) put_float(rt, sp(k * 4u), centres[k]);
    const std::uint32_t points = sp(0x30u);
    const std::uint32_t uvs = sp(0xA0u);
    for (std::uint32_t corner = 0u; corner < 4u; ++corner) {
        const float cx = f(sp(corner * 8u));
        const float cy = f(sp(corner * 8u + 4u));
        const std::uint32_t sign = 0x08CDE87Cu + corner * 8u;
        for (std::uint32_t i = 0u; i < 10u; ++i) {
            const std::uint32_t entry = word(rt, 0x08CF4B08u) + i * 8u;
            const float x = psp_mul(psp_mul(radius, f(entry)), f(sign)) + cx;
            const float y = psp_mul(psp_mul(f(entry + 4u), radius), f(sign + 4u)) + cy;
            put_float(rt, points + i * 8u, x);
            put_float(rt, points + i * 8u + 4u, y);
            if (uv_source != 0u) sample_uv(uvs + i * 8u, x, y);
            if (colour_source != 0u) sample_colour(colours + i * 0x10u, f(points + i * 8u), f(points + i * 8u + 4u));
        }
        draw(0x08C8CC10u, 10u, points, uv_source != 0u ? uvs : 0u, colour_source != 0u ? colours : 0u);
    }
    std::uint32_t count = 0u;
    std::vector<std::pair<std::uint32_t, float>> fill;
    if (radius < half_width) {
        if (radius < half_height) {
            count = 14u;
            fill = {{0x30, half_width}, {0x34, half_height}, {0x78, inner_width}, {0x94, inner_height}, {0x9C, radius},
                    {0x38, 0.0f}, {0x3C, radius}, {0x40, radius}, {0x44, radius}, {0x48, radius}, {0x4C, 0.0f},
                    {0x50, inner_width}, {0x54, 0.0f}, {0x58, inner_width}, {0x5C, radius}, {0x60, width}, {0x64, radius},
                    {0x68, width}, {0x6C, inner_height}, {0x70, inner_width}, {0x74, inner_height}, {0x7C, height},
                    {0x80, radius}, {0x84, height}, {0x88, radius}, {0x8C, inner_height}, {0x90, 0.0f}, {0x98, 0.0f}};
        } else {
            count = 6u;
            fill = {{0x30, half_width}, {0x34, half_height}, {0x48, inner_width}, {0x58, radius}, {0x38, radius},
                    {0x3C, 0.0f}, {0x40, inner_width}, {0x44, 0.0f}, {0x4C, height}, {0x50, radius}, {0x54, height},
                    {0x5C, 0.0f}};
        }
    } else if (radius < half_height) {
        count = 6u;
        fill = {{0x30, half_width}, {0x34, half_height}, {0x54, inner_height}, {0x5C, radius}, {0x38, 0.0f},
                {0x3C, radius}, {0x40, width}, {0x44, radius}, {0x48, width}, {0x4C, inner_height}, {0x50, 0.0f},
                {0x58, 0.0f}};
    }
    for (const auto &[offset, value] : fill) put_float(rt, sp(offset), value);
    for (std::uint32_t i = 0u; i < count; ++i) {
        if (uv_source != 0u) sample_uv(uvs + i * 8u, f(points + i * 8u), f(points + i * 8u + 4u));
        if (colour_source != 0u) sample_colour(colours + i * 0x10u, f(points + i * 8u), f(points + i * 8u + 4u));
    }
    draw(0x08C8CC10u, count, points, uv_source != 0u ? uvs : 0u, colour_source != 0u ? colours : 0u);
}


// func_08811684(self, text, time, unsigned_tail, signed_tail, sign_mode): lays out a lap time (milliseconds) as glyphs
// of the font self+0x224 in the text object (nothing without one): func_089DDC94, and when its slot count is not
// the wanted one (20 with a sign column: mode 1 '+'/'-', mode 2 an arrow; else 18) it rebuilds the layout
// (func_089DE054 / func_089DE1BC, glyph widths from func_0880E024) and sets flag 0x4000 in text+0xE0. Times
// past 336 hours show seven dashes; otherwise the time (capped at 99:59.999) is drawn as m:ss.mmm digit by digit
// with func_089DB078(text, slot, record) where record = font data + 16 * (code - first + 1) (+0x300 for digits,
// first = the font's +8); a zero tens-of-minutes digit is the blank record 0x08CE29F0 and moves the sign
// (func_089DAEA0). It ends in func_089DB5CC(text, self+0x23C for dashes, else the tail for the sign).
void func_08811684(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    const std::int32_t time = signed_of(ctx.gpr[6]);
    const std::uint32_t unsigned_tail = ctx.gpr[7];
    const std::uint32_t signed_tail = ctx.gpr[8];
    const std::uint32_t sign_mode = ctx.gpr[9];
    if (text == 0u) return;
    GuestFrame frame(ctx, 0x50u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const std::uint32_t slots = sign_mode != 0u ? 0x14u : 0x12u;
    std::int32_t rest = std::max(time, signed_of(0u - ctx.gpr[6]));  // the asm's max(t, -t): INT32_MIN stays
    const std::uint32_t font = self + 0x224u;
    if (call_set(rt, ctx, 0x089DDC94u, {text}) != slots) {
        put(rt, text + 0xE8u, 6u);
        call_set(rt, ctx, 0x089DE054u, {text, slots});
        call_set(rt, ctx, 0x089DE1BCu, {text, slots});
        const auto width = [&](std::uint32_t slot, std::uint32_t code, float x, float y) {
            return call_setf(rt, ctx, 0x0880E024u, {font, text, slot, code}, {x, y}).f0;
        };
        float x = width(0xCu, 0x30u, 0.0f, 0.0f) + 0.0f;
        x = x + width(0xAu, 0x30u, x, 0.0f);
        x = x + width(0xEu, 0x27u, x, 0.0f);
        x = x + width(8u, 0x30u, x, 0.0f);
        x = x + width(6u, 0x30u, x, 0.0f);
        x = x + width(0x10u, 0x2Eu, x, 0.0f);
        x = x + width(4u, 0x30u, x, 0.0f);
        const float last = x + width(2u, 0x30u, x, 0.0f);
        width(0u, 0x30u, last, 0.0f);
        if (sign_mode == 1u) {
            width(0x12u, 0x2Du, 0.0f, 0.0f);
        } else if (sign_mode == 2u) {
            width(0x12u, 0x2Au, std::bit_cast<float>(0xC1980000u), 0.5f);
        }
        put(rt, text + 0xE0u, word(rt, text + 0xE0u) | 0x4000u);
    }
    const auto first_code = [&] { return ctx.fpu_float_to_word(f(word(rt, font) + 8u), 1u); };
    const auto record_of = [&](std::uint32_t code, std::uint32_t first) { return word(rt, font) + (code - (first - 1u)) * 16u; };
    // func_089DB078(text, slot, ...) with the record's four floats; `arguments` are the registers the asm sets
    const auto glyph = [&](std::initializer_list<std::uint32_t> arguments, std::uint32_t record) {
        call_setf(rt, ctx, 0x089DB078u, arguments, {f(record), f(record + 4u), f(record + 8u), f(record + 0xCu)});
    };
    if (0x481907FE < rest) {
        for (std::uint32_t slot = 0u; slot != 0xEu; slot += 2u) {
            const std::uint32_t first = first_code();
            glyph({text, slot, first}, record_of(0x2Du, first));
        }
        tail_call(ctx, 0x089DB5CCu, {text, word(rt, self + 0x23Cu)});
        return;
    }
    std::int32_t digit;
    if (0x005B8D7F < rest) {
        rest = 0x927BF;
        digit = 9;
    } else {
        digit = rest % 10;
        rest = rest / 10;
    }
    float sign_x = -8.0f;
    {
        const std::uint32_t first = first_code();
        const std::uint32_t base = record_of(static_cast<std::uint32_t>(digit), first);
        glyph({text, 0u, base, first}, base + 0x300u);
    }
    {
        const std::uint32_t first = first_code();
        const std::int32_t next = rest / 10;
        const std::uint32_t base = record_of(static_cast<std::uint32_t>(rest - next * 10), first);
        glyph({text, 2u, base, first}, base + 0x300u);
        rest = next;
    }
    {
        const std::uint32_t first = first_code();
        const std::int32_t next = rest / 10;
        const std::uint32_t base = record_of(static_cast<std::uint32_t>(rest - next * 10), first);
        glyph({text, 4u, base}, base + 0x300u);
        rest = next;
    }
    {
        const std::uint32_t first = first_code();
        const std::int32_t next = rest / 10;
        const std::uint32_t base = record_of(static_cast<std::uint32_t>(rest - next * 10), first);
        glyph({text, 6u, base}, base + 0x300u);
        rest = next;
    }
    {
        const std::uint32_t first = first_code();
        const std::int32_t next = rest / 6;
        const std::uint32_t base = record_of(static_cast<std::uint32_t>(rest - next * 6), first);
        glyph({text, 8u}, base + 0x300u);
        rest = next;
    }
    {
        const std::uint32_t first = first_code();
        const std::int32_t next = rest / 10;
        const std::uint32_t base = record_of(static_cast<std::uint32_t>(rest - next * 10), first);
        glyph({text, 0xAu, base}, base + 0x300u);
        rest = next;
    }
    if (rest == 0) sign_x = 2.0f;
    {
        const std::int32_t tens = rest % 10;
        std::uint32_t record = 0x08CE29F0u;
        if (tens != 0) record = record_of(static_cast<std::uint32_t>(tens), first_code()) + 0x300u;
        glyph({text, 0xCu}, record);
    }
    if (sign_mode == 1u) {
        const std::uint32_t first = first_code();
        const std::uint32_t base = record_of(time < 0 ? 0x2Du : 0x2Bu, first);
        glyph({text, 0x12u, base, first}, base);
        call_setf(rt, ctx, 0x089DAEA0u, {text, 0x12u}, {sign_x, 0.0f, 9.0f, 16.0f});
    }
    tail_call(ctx, 0x089DB5CCu, {text, time < 0 ? signed_tail : unsigned_tail});
}


// func_08812198(self, text, value, tail): lays out a number of up to seven digits in the text object (nothing
// without one) with the font self+0x224 (data: scale +0, height +4, first code +8; glyph records 16 bytes, +0x300 for
// digits). Unless func_089DDC94 says the 14-slot layout is in place it builds it (func_089DE054 / func_089DE1BC) from
// the '0' glyph's widths (func_089DB078 / func_089DAEA0 per slot, right to left) and sets flag 0x4000 in text+0xE0. A
// negative value shows dashes (three full, four flat); otherwise min(value, 9999999) is split by func_08B77CD8 (div)
// into digits, the ones above the highest non-zero digit drawn flat. It ends in func_089DB5CC(text, self+0x23C for
// dashes, else tail).
void func_08812198(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    const std::int32_t value = signed_of(ctx.gpr[6]);
    const std::uint32_t tail = ctx.gpr[7];
    if (text == 0u) return;
    GuestFrame frame(ctx, 0x70u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const std::uint32_t font = self + 0x224u;
    const auto data = [&] { return word(rt, font); };
    const auto first_of = [&](std::uint32_t at) { return ctx.fpu_float_to_word(f(at + 8u), 1u); };
    const auto put_glyph = [&](std::initializer_list<std::uint32_t> arguments, float x, float y, float w, float h) {
        call_setf(rt, ctx, 0x089DB078u, arguments, {x, y, w, h});
    };
    if (call_set(rt, ctx, 0x089DDC94u, {text}) != 0xEu) {
        const std::uint32_t table = data();
        const float height_scale = f(table + 4u);
        const float scale = f(table);
        const std::uint32_t zero = table - (first_of(table) - 1u) * 16u;
        const float glyph_height = psp_mul(f(zero + 0x30Cu), height_scale);
        put(rt, text + 0xE8u, 6u);
        call_set(rt, ctx, 0x089DE054u, {text, 0xEu});
        call_set(rt, ctx, 0x089DE1BCu, {text, 0xEu});
        float x = 0.0f;
        float before_last = 0.0f;
        for (std::uint32_t slot = 0xEu; slot != 0u;) {
            slot -= 2u;
            const std::uint32_t now = data();
            const std::uint32_t first = first_of(now);
            const std::uint32_t record = now - (first - 1u) * 16u + 0x300u;
            put_glyph({text, slot, first}, f(record), f(record + 4u), 0.0f, 0.0f);
            const float width = psp_mul(scale, f(record + 8u));
            if (slot == 0xCu) {
                call_setf(rt, ctx, 0x089DAEA0u, {text, slot}, {0.0f, 0.0f, width, glyph_height});
                x = width + 0.0f;
            } else if (slot == 2u) {
                before_last = width;
                call_setf(rt, ctx, 0x089DAEA0u, {text, slot}, {x, 0.0f, width, glyph_height});
            } else if (slot == 0u) {
                call_setf(rt, ctx, 0x089DAEA0u, {text, slot}, {x + before_last, 0.0f, width, glyph_height});
            } else {
                call_setf(rt, ctx, 0x089DAEA0u, {text, slot}, {x, 0.0f, width, glyph_height});
                x = x + width;
            }
        }
        put(rt, text + 0xE0u, word(rt, text + 0xE0u) | 0x4000u);
    }
    if (value < 0) {
        for (std::uint32_t slot = 0u; slot != 0xEu; slot += 2u) {
            const std::uint32_t now = data();
            const std::uint32_t first = first_of(now);
            const std::uint32_t record = now + (0x2Du - (first - 1u)) * 16u;
            if (slot < 6u) {
                put_glyph({text, slot, first}, f(record), f(record + 4u), f(record + 8u), f(record + 0xCu));
            } else {
                put_glyph({text, slot, first}, f(record), f(record + 4u), 0.0f, 0.0f);
            }
        }
        tail_call(ctx, 0x089DB5CCu, {text, word(rt, self + 0x23Cu)});
        return;
    }
    std::uint32_t rest = static_cast<std::uint32_t>(std::min(value, 0x98967F));
    bool leading = false;
    for (std::uint32_t slot = 0u; slot != 0xEu; slot += 2u) {
        const std::uint32_t quotient = call_set(rt, ctx, 0x08B77CD8u, {rest, 10u});
        const std::uint32_t digit = ctx.gpr[3];
        rest = quotient;
        const std::uint32_t now = data();
        const std::uint32_t first = first_of(now);
        const std::uint32_t base = now + (digit - (first - 1u)) * 16u;
        if (leading) {
            put_glyph({text, slot, first, digit}, f(base + 0x300u), f(base + 0x304u), 0.0f, 0.0f);
        } else {
            put_glyph({text, slot, base, digit}, f(base + 0x300u), f(base + 0x304u), f(base + 0x308u), f(base + 0x30Cu));
        }
        if (!(0 < signed_of(quotient))) leading = true;
    }
    tail_call(ctx, 0x089DB5CCu, {text, tail});
}


// func_08997D38(self, cursor): one step of a scroll view (content self+0x130, mode +0x120, rate +0x124, target
// +0x134 / +0x138) after func_08A00FA4. Mode 1, or a focused item (func_089BD4DC with bit 0x80 of its +0x138 and
// func_089AA910): the cursor's position (func_089AA95C) is kept inside the middle of the view (func_08969B98: 3/8 margin,
// 1/4 band) by scrolling the content (func_0896A35C / func_0896A9C4 per axis, clamped to its size), then handed back
// (func_089AA9BC). Modes 0 and 2 ease the content toward the target by the rate (snapping inside +-0.01); mode 0 ends,
// once still and func_08969330 agrees, by releasing self+0x154 (func_08A5B930). Mode 3, under the lock 0x08CF4598,
// steps the flag state self+0x12C (bits 3-4: 0 start (flips bit 2, restarts the animation self+0x184, func_089D87E0 /
// func_08A0F0CC), 1 running (progress +0x178 by +0x128 over the path length +0x170), 2 done (func_0899C97C through
// func_0897783C)), then places the content at the point (func_08996A18) of the path list self+0x150 + 12 * bit 2
// (36-byte segments, lengths func_08995CB0) reached by func_08996308's fraction, centred in the view.
void func_08997D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t cursor = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const auto f = [&](std::uint32_t address) { return float_at(rt, address); };
    const auto sp = [&](std::uint32_t offset) { return frame.at(offset); };
    call_set(rt, ctx, 0x08A00FA4u, {self});
    if (cursor == 0u) return;
    ctx.gpr[2] = word(rt, self + 0x130u);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t mode = word(rt, self + 0x120u);
    const std::uint32_t focus = call_set(rt, ctx, 0x089BD4DCu, {self});
    bool follow = false;
    if (focus != 0u && call_set(rt, ctx, 0x089AA910u, {cursor}) != 0u && (word(rt, focus + 0x138u) & 0x80u) != 0u) follow = true;
    if (mode == 1u) follow = true;
    const auto content = [&] { return word(rt, self + 0x130u); };
    const auto set_position = [&](std::uint32_t x, std::uint32_t y) { call_set(rt, ctx, 0x0896A9C4u, {content(), x, y, 0u, 0u}); };
    const auto ease = [&](float target, float current, float rate) {
        const float step = psp_mul(rate, target - current);
        if (step < std::bit_cast<float>(0xBC23D70Au) || std::bit_cast<float>(0x3C23D70Au) < step) return current + step;
        return target;
    };
    if (follow) {
        const std::uint32_t item = call_set(rt, ctx, 0x089AAC8Cu, {cursor});
        if (item != call_set(rt, ctx, 0x089BD4DCu, {self})) return;
        call_set(rt, ctx, 0x089AA95Cu, {cursor, sp(4u), sp(8u)});
        call_set(rt, ctx, 0x08969B98u, {self, sp(0xCu), sp(0x10u), sp(0x14u), sp(0x2Cu)});
        const float view_w = f(sp(0x14u));
        const float view_h = f(sp(0x2Cu));
        const float left = psp_mul(view_w, 0.375f) + f(sp(0xCu));
        const float top = psp_mul(view_h, 0.375f) + f(sp(0x10u));
        const float band_w = psp_mul(view_w, 0.25f);
        const float band_h = psp_mul(view_h, 0.25f);
        call_set(rt, ctx, 0x0896A35Cu, {content(), sp(0x28u), sp(0x24u), sp(0x20u), sp(0x1Cu)});
        const float tx = f(sp(4u));
        float dx = 0.0f;
        if (tx < left) dx = tx - left;
        if (left + band_w < tx) dx = tx - (left + band_w);
        const float ty = f(sp(8u));
        float dy = 0.0f;
        if (ty < top) dy = ty - top;
        if (top + band_h < ty) dy = ty - (top + band_h);
        const auto scroll = [&](std::uint32_t position, std::uint32_t target, float delta, float view, std::uint32_t size) {
            const float now = f(sp(position));
            if (now < 0.0f && view - f(sp(size)) < now) put_float(rt, sp(target), f(sp(target)) - delta);
            float moved = now - delta;
            put_float(rt, sp(position), moved);
            if (0.0f < moved) {
                moved = 0.0f;
                put_float(rt, sp(position), 0.0f);
            }
            const float lowest = view - f(sp(size));
            if (moved < lowest) {
                moved = lowest;
                put_float(rt, sp(position), lowest);
            }
            put_float(rt, sp(0x18u), moved);
        };
        scroll(0x28u, 4u, dx, view_w, 0x20u);
        set_position(sp(0x18u), 0u);
        scroll(0x24u, 8u, dy, view_h, 0x1Cu);
        set_position(0u, sp(0x18u));
        call_setf(rt, ctx, 0x089AA9BCu, {cursor}, {f(sp(4u)), f(sp(8u))});
        return;
    }
    if (mode == 0u) {
        if (call_set(rt, ctx, 0x08997C1Cu, {self, self + 0x134u, self + 0x138u}) == 0u) return;
        call_set(rt, ctx, 0x0896A35Cu, {content(), sp(0x18u), sp(0x1Cu), 0u, 0u});
        const float x = ease(f(self + 0x134u), f(sp(0x18u)), f(self + 0x124u));
        const float y = ease(f(self + 0x138u), f(sp(0x1Cu)), f(self + 0x124u));
        if (!(x == f(sp(0x18u))) || !(y == f(sp(0x1Cu)))) {
            put_float(rt, sp(0x24u), x);
            put_float(rt, sp(0x20u), y);
            set_position(sp(0x24u), sp(0x20u));
            return;
        }
        if (call_set(rt, ctx, 0x08969330u, {self}) != 0u) return;
        const std::uint32_t owned = word(rt, self + 0x154u);
        ctx.gpr[2] = owned;
        if (owned != 0u) call_set(rt, ctx, 0x08A5B930u, {owned});
        put(rt, self + 0x154u, 0u);
        return;
    }
    if (mode == 2u) {
        call_set(rt, ctx, 0x0896A35Cu, {content(), sp(0x24u), sp(0x20u), 0u, 0u});
        const float x = ease(f(self + 0x134u), f(sp(0x24u)), f(self + 0x124u));
        put_float(rt, sp(0x24u), x);
        const float y = ease(f(self + 0x138u), f(sp(0x20u)), f(self + 0x124u));
        put_float(rt, sp(0x18u), f(sp(0x24u)));
        put_float(rt, sp(0x1Cu), y);
        put_float(rt, sp(0x20u), y);
        set_position(sp(0x18u), sp(0x1Cu));
        return;
    }
    ctx.gpr[2] = 3u;
    if (mode != 3u) return;
    call_set(rt, ctx, 0x08969B98u, {self, sp(0x18u), sp(0x1Cu), sp(0x20u), sp(0x24u)});
    constexpr std::uint32_t kLock = 0x08CF4598u;
    call_set(rt, ctx, 0x08B344E0u, {kLock});
    std::uint32_t flags = word(rt, self + 0x12Cu);
    const std::uint32_t state = flags & 0x18u;
    if (state == 0u) {
        flags = (flags & ~4u) | (((1u - ((flags >> 2) & 1u)) & 1u) << 2);
        flags = (flags & ~0x18u) | (1u << 3);
        put_float(rt, self + 0x178u, 0.0f);
        put(rt, self + 0x12Cu, flags);
        std::uint32_t animation = word(rt, self + 0x184u);
        if (animation != 0u) {
            const float speed = f(self + 0x128u);
            if (0.0f < speed) {
                const float length = f(self + 0x170u + (flags & 4u));
                call_setf(rt, ctx, 0x089D87E0u, {animation}, {psp_mul(length / speed, f(self + 0x188u))});
                animation = word(rt, self + 0x184u);
            }
            if (animation == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA43F8u});
            call_set(rt, ctx, 0x08A0F0CCu, {animation});
        }
    } else if (state == 8u) {
        const std::uint32_t animation = word(rt, self + 0x184u);
        if (animation != 0u) {
            if ((word(rt, animation + 0x60u) & 0x40000u) == 0u) {
                flags = (flags & ~0x18u) | (2u << 3);
                put_float(rt, self + 0x178u, 1.0f);
                put(rt, self + 0x12Cu, flags);
            }
        } else {
            const float length = f(self + 0x170u + (flags & 4u));
            float progress;
            if (0.0f < length) {
                progress = f(self + 0x178u) + f(self + 0x128u) / length;
                put_float(rt, self + 0x178u, progress);
            } else {
                progress = 1.0f;
                put_float(rt, self + 0x178u, 1.0f);
            }
            if (1.0f <= progress) {
                flags = (flags & ~0x18u) | (2u << 3);
                put_float(rt, self + 0x178u, 1.0f);
                put(rt, self + 0x12Cu, flags);
            }
        }
        if ((flags & 0x18u) == 0x10u && word(rt, self + 0x180u) != 0u) {
            call_set(rt, ctx, 0x08A98A70u, {sp(0u)});
            call_set(rt, ctx, 0x0899C97Cu, {sp(0x30u), cursor, self, self + 0x180u, sp(0u)});
            call_set(rt, ctx, kDestroy, {sp(0u)});
            call_set(rt, ctx, 0x0897783Cu, {word(rt, cursor + 0x14u), sp(0x30u)});
            call_set(rt, ctx, kDestroy, {sp(0x30u)});
        }
    }
    call_set(rt, ctx, 0x08B3457Cu, {kLock});
    const auto path = [&] { return self + 0x150u + ((word(rt, self + 0x12Cu) >> 2) & 1u) * 12u; };
    ctx.gpr[2] = path();
    if (word(rt, ctx.gpr[2] + 8u) == word(rt, ctx.gpr[2] + 0xCu)) return;
    const float fraction = call_setf(rt, ctx, 0x08996308u, {self}, {}).f0;
    const std::uint32_t list = path();
    const std::uint32_t begin = word(rt, list + 8u);
    const std::uint32_t count = ((word(rt, list + 0xCu) - begin) >> 2) * 0x38E38E39u;
    const float total = psp_mul(fraction, f(self + 0x170u + ((word(rt, self + 0x12Cu) >> 2) & 1u) * 4u));
    float walked = 0.0f;
    std::uint32_t segment = begin;
    for (std::uint32_t index = 0u; index + 1u < count + 0u && count - 1u != 0u;) {
        segment = begin + index * 0x24u;
        const float reach = walked + call_setf(rt, ctx, 0x08995CB0u, {segment}, {}).f0;
        ++index;
        if (total < reach) break;
        walked = reach;
        if (index == count - 1u) {
            segment = begin + index * 0x24u;
            break;
        }
    }
    const float inside = (total - walked) / call_setf(rt, ctx, 0x08995CB0u, {segment}, {}).f0;
    call_setf(rt, ctx, 0x08996A18u, {sp(0x30u), segment}, {inside});
    const float x = -(f(sp(0x30u)) + psp_mul(f(sp(0x20u)), -0.5f));
    const float y = -(f(sp(0x34u)) + psp_mul(f(sp(0x24u)), -0.5f));
    put_float(rt, self + 0x134u, x);
    put_float(rt, self + 0x138u, y);
    put_float(rt, sp(0x2Cu), x);
    put_float(rt, sp(0x28u), y);
    set_position(sp(0x2Cu), sp(0x28u));
}

} // namespace hot_batch_124
} // namespace gt::game

#define HOT124(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_124::name, gt::ReturnKind::kind)
HOT124(0x08A0B264u, func_08A0B264, Word);
HOT124(0x089A9654u, func_089A9654, Word);
HOT124(0x08811684u, func_08811684, Word);
HOT124(0x08812198u, func_08812198, Word);
HOT124(0x08997D38u, func_08997D38, Word);
