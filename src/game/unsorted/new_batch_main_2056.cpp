// New batch (main session 2056): drafts from tools.disasm, structured C. Not gate-verified yet.
// Every function keeps the input shapes listed in gt-ops/shapes/shapes_main2056.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2056 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
void store8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }

// A shared_ptr-style release: the strong count at +4 goes down by one and, when it was the last, the object's virtual at
// +8 runs; then the weak count at +8 goes down and its virtual at +0xC runs when that was the last.
void release_object(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    const u32 strong = word(rt, object + 4u) - 1u;
    put(rt, object + 4u, strong);
    if (strong != 0u) return;
    (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    const u32 weak = word(rt, object + 8u) - 1u;
    put(rt, object + 8u, weak);
    if (weak != 0u) return;
    (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// 0x089773D0: bump the four counters selected by the bit mask, decay the others, then hand a direction to 0x089AAA94.
// The v0 left in the register by the decay loop is the answer when the mask is empty (sext8(last byte - 3)).
void func_089773D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 shifted = index << 2;
    const u32 selected = call(rt, ctx, 0x08977270u, {self, index});
    const u32 base = shifted + self;
    u32 cursor = base + 0x158u;
    u32 flags = 0u;
    for (u32 i = 0u; i < 4u; ++i) {
        if ((selected & word(rt, 0x08CDE7A8u + 4u * i)) != 0u) {
            flags |= 1u << i;
            const s32 grown = static_cast<s8>(static_cast<u8>(rt.memory().load8(cursor) + 1u));
            store8(rt, cursor, grown < 0x1F ? static_cast<u32>(grown) : 0x1Eu);
        }
        cursor += 1u;
    }
    u32 at = base + 0x158u;
    u32 leftover = 0u;
    for (u32 i = 0u; i < 4u; ++i) {
        leftover = 0u;
        if (((flags >> i) & 1u) == 0u) {
            const s32 shrunk = static_cast<s8>(static_cast<u8>(rt.memory().load8(at) - 3u));
            leftover = static_cast<u32>(shrunk);
            store8(rt, at, shrunk >= 0 ? static_cast<u32>(shrunk) : 0u);
        }
        at += 1u;
    }
    if (flags == 0u) {
        ctx.gpr[2] = leftover;
        return;
    }
    const float second_x = static_cast<float>(signed_byte(rt, base + 0x15Au));
    const float second_y = static_cast<float>(signed_byte(rt, base + 0x159u));
    const float first_x = static_cast<float>(signed_byte(rt, base + 0x158u));
    const float first_y = static_cast<float>(signed_byte(rt, base + 0x15Bu));
    float dx = (second_y - first_x) / 30.0f;
    float dy = (first_y - second_x) / 30.0f;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (1.0f < length) {
        dy = dy / length;
        dx = dx / length;
    }
    ctx.fpr[12] = dx * 10.0f;
    ctx.fpr[13] = dy * 10.0f;
    tail_call(ctx, 0x089AAA94u, {out});
}

// 0x089AA038: intersect two rectangles (the second one from 0x089A8B98), write the result through a1 and answer
// whether it has an area.
void func_089AA038(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    u32 result = 0u;
    if (call(rt, ctx, 0x089A8C70u, {self}) != 0u) {
        const u32 first = call(rt, ctx, 0x089A8C94u, {self});
        const u32 second = call(rt, ctx, 0x089A8B98u, {self});
        const float other_x0 = float_at(rt, second);
        const float own_x0 = float_at(rt, first);
        const float left = (own_x0 < other_x0) ? other_x0 : own_x0;
        const float other_y0 = float_at(rt, second + 4u);
        const float own_y0 = float_at(rt, first + 4u);
        const float top = (own_y0 < other_y0) ? other_y0 : own_y0;
        float width = own_x0 + float_at(rt, first + 8u);
        const float other_width = other_x0 + float_at(rt, second + 8u);
        width = (width < other_width) ? width : other_width;
        float height = own_y0 + float_at(rt, first + 0xCu);
        const float other_height = other_y0 + float_at(rt, second + 0xCu);
        const float bottom = (height < other_height) ? height : other_height;
        // The width is shortened by `left` on both branches of the asm; only `bottom` is picked.
        width = width - left;
        height = bottom - top;
        if (width <= 0.0f || height <= 0.0f) {
            width = 0.0f;
            height = 0.0f;
        }
        put_float(rt, frame.at(0), left);
        put_float(rt, frame.at(4), top);
        put_float(rt, frame.at(8), width);
        put_float(rt, frame.at(0xCu), height);
        if (out != frame.at(0)) {
            put_float(rt, out, left);
            put_float(rt, out + 4u, top);
            put_float(rt, out + 8u, width);
            put_float(rt, out + 0xCu, height);
        }
        if (float_at(rt, out + 8u) > 0.0f) result = (float_at(rt, out + 0xCu) > 0.0f) ? 1u : 0u;
    } else {
        const u32 source = call(rt, ctx, 0x089A8B98u, {self});
        result = 1u;
        if (source != out) {
            const float at_c = float_at(rt, source + 0xCu);
            const float at_0 = float_at(rt, source);
            const float at_4 = float_at(rt, source + 4u);
            const float at_8 = float_at(rt, source + 8u);
            put_float(rt, out + 0xCu, at_c);
            put_float(rt, out, at_0);
            put_float(rt, out + 4u, at_4);
            put_float(rt, out + 8u, at_8);
        }
    }
    ctx.gpr[2] = result;
}

// 0x089614D8: under the embedded lock at +0x18, take the pending pointer at +0xC into +4, publish the pointer at +0x10
// over the one at +8 (a reference for the new, a release for the old), clear the flag byte at +0x14 and run the virtual
// at +8 of the object's table; when that answers 0 the pending pointers are dropped, and the lock is released (tail call).

} // namespace
} // namespace new_batch_main_2056
} // namespace gt::game

#define NEW2056(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2056::name, gt::ReturnKind::kind)
NEW2056(0x089773D0u, func_089773D0, Word);
NEW2056(0x089AA038u, func_089AA038, Word);
