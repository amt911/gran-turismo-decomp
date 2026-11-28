// Main-session batch 1037: three functions the earlier passes mismatched or skipped. Written from tools.disasm,
// structured C; role words in the comments describe access shapes, none is a game name.
// Delay slots run before the jal / branch, v0 is set as the original leaves it on every path, and every argument
// register a callee reads is passed.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1037 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

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
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// Copies the six floats of `entry` to `slot` and its three floats at +0x18 to slot +0x18, then calls func_088CB910
// with the vector at slot + `vector_offset` and the output at `out`.
void project(Runtime &rt, AllegrexContext &ctx, u32 self, u32 entry, u32 slot, u32 vector_offset, u32 out) {
    for (u32 k = 0u; k != 6u; ++k) put_float(rt, slot + k * 4u, float_at(rt, entry + k * 4u));
    put_float(rt, slot + 0x18u, float_at(rt, entry + 0x18u));
    put_float(rt, slot + 0x1Cu, float_at(rt, entry + 0x1Cu));
    put_float(rt, slot + 0x20u, float_at(rt, entry + 0x20u));
    (void)call(rt, ctx, 0x088CB910u, {self, slot + vector_offset, out});
}

// 0x0890171C (object = a0, out = a1, from = a2, to = a3, cluster = t0): clips the triangle named by `cluster`'s
// index list against the plane (from, to - from): the vertex whose edge functions are all positive and whose
// plane parameter is the smallest gives the intersection point stored at out +0 / +4 / +8 (lerp of from and to).
// out +0xC / +0x10 take the record's bytes +0xC (masked to 5 bits) and +0xD, out +0x14 / +0x18 the record's
// vertex bytes +0xE scaled by 1/255 and +0xF (masked to 7 bits) scaled by 1/127, both interpolated by the three
// edge-function weights and clamped to [0, 1]; out +0x1C / +0x20 / +0x24 a normalised vector, out +0x28 its negated
// dot with the vertex at +0 and out +0x2C the plane parameter; out +0x30 = the chosen vertex index, +0x34 = the
// `cluster` argument. Each three-float point of out +0 / +4 / +8 is first clamped into the box built from the
// chosen triangle's vertices (lo / hi as the asm builds them). v0 (and v1) = 1, or 0 when the cluster's count is
// not positive or no vertex qualifies. All arithmetic is single precision, in the asm's order.

// 0x088CC114 (self = a0, source = a1): returns 0 when byte self +0x28 is clear and -1 when word self +0x2C is
// negative or word self +0x30 is -1, after the first call. Otherwise it emits records for the 34 slots: for each
// slot the index word of *self +0xB50 plus the slot (mod 0x24, sign-aware compare) names an entry of 9 floats at
// self + 0x640 + index * 0x24 and the next entry names a second one. Two orientations are built at frame +0x30 and
// +0x20 (func_08B4A6A0 with the entry's +0x1C as angle, axis (1,0,0) and (0.125, 0.3671875, 0.99609375)) and
// multiplied (func_08B4A890), each tagged by func_088CB53C; four projected copies of the entry pair go through
// func_088CB910 into frame +0x16C / +0x17C / +0x18C / +0x19C, and each of the four is rewritten by
// func_088CC058(self, frame +0x40, slot) and func_08B5A7F0(slot, slot) and stored as 16 bytes (three floats and
// the first pair's tag for the first two, the second pair's for the last two) at the record buffer
// func_08B550E0(0x880) + n * 16, n counting every stored record. Then the library's frame helpers run and v0 is
// the last call's.

// 0x08AAEE04 (out = a0, container = a1, other = a2): container-assign body. It returns out on every path (v0 =
// word of the frame's +0x60 slot, which holds a0). Word container +0x14 == 7 returns at once. An empty word
// container +0x38 (word +0x38 is the +0x14 of container + 0x24) builds one node from the temporary pair at frame
// +0x34 (func_08AAE20C / func_08AAE214 copy it into the node allocated by func_08A5BC20 and set up by
// func_08A5B828, vtables 0x08CD5E30 / 0x08CD5E80), hands it to the slot at frame +0x38 through func_08AB6F54 and
// releases both temporaries. Otherwise the old list (container + 0x3C / +0x40, 12-byte elements counted through
// the modular-inverse divide by 12 the asm does) is walked down (func_08B5F100) into frame +0x50 with a node from
// func_08AB6F54 when it was non-empty, sorted through func_08C366E8 / 0x08C35C80 when it is a single group, then
// every not-yet-flagged element (byte +8 clear) gets one: func_08AAE358 + a virtual call (vtable +0x20) on the
// element's object into frame +0x40, or the pair at +0x1C / +0x18 when it is not the same object, plus the
// func_08B85EB8 key copy and a node from func_08A5BC20 / func_08A5B828 (vtable 0x08CD5E58) built with
// func_08B88F44 and func_08AB6F54; the temporaries are then released (func_08A5B9EC / func_08AB70EC / func_08B5C094
// / func_08A5BC54 / func_08B60804 / func_08B8542C) and the element is flagged. Null pointers reach the library's
// diagnostic at 0x08A5BB7C with the message addresses 0x08CB39CC / 0x08CB39D8 / 0x08CB39A4.
void func_08AAEE04(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xA0u);
    const u32 sp = frame.at(0u);
    const u32 s3 = ctx.gpr[5];
    const u32 s5 = ctx.gpr[6];
    const u32 s6 = s3 + 0x3Cu;
    put(rt, sp + 0x60u, ctx.gpr[4]);
    (void)call(rt, ctx, 0x08A5BAB8u, {ctx.gpr[4], 0x08CF6ED8u});
    if (word(rt, s3 + 0x14u) == 7u) {
        ctx.gpr[2] = word(rt, sp + 0x60u);
        return;
    }
    if (word(rt, s3 + 0x38u) == 0u) {
        const u32 temp = sp + 0x34u;
        put(rt, sp + 0x30u, s3);
        (void)call(rt, ctx, 0x08AAE20Cu, {temp, sp + 0x30u});
        const u32 node = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
        (void)call(rt, ctx, 0x08A5B828u, {node});
        put(rt, node, 0x08CD5E30u);
        (void)call(rt, ctx, 0x08AAE214u, {node + 0xCu, temp});
        put(rt, node, 0x08CD5E80u);
        put8(rt, node + 0x10u, 0u);
        put(rt, node + 0x14u, 0u);
        put(rt, node + 0x1Cu, 0u);
        put(rt, node + 0x18u, 0u);
        const u32 slot = sp + 0x38u;
        put(rt, sp + 0x10u, node);
        (void)call(rt, ctx, 0x08AB6F54u, {slot, sp + 0x10u});
        const u32 out = word(rt, sp + 0x60u);
        if (out != slot) (void)call(rt, ctx, 0x08A5BAD8u, {out, word(rt, slot)});
        (void)call(rt, ctx, 0x08A5B9ECu, {slot});
        (void)call(rt, ctx, 0x08A5B9ECu, {temp});
        ctx.gpr[2] = word(rt, sp + 0x60u);
        return;
    }

    const u32 self_link = s3 + 0x28u;
    if (word(rt, s3 + 0x3Cu) == word(rt, s6 + 4u)) {
        u32 s1 = word(rt, s3 + 0x30u);
        if (s1 != self_link) {
            const u32 temp = sp + 0x50u;
            const u32 word_slot = sp + 0x4Cu;
            for (;;) {
                const u32 table = word(rt, s1 + 0x10u);
                put(rt, word_slot, word(rt, table + 4u));
                (void)call(rt, ctx, 0x08AAE214u, {temp, s1 + 0x14u});
                u32 node = word(rt, s6 + 4u);
                if (node == word(rt, s6 + 8u)) {
                    (void)call(rt, ctx, 0x08C356C8u, {s6, word(rt, s3 + 0x40u)});
                } else {
                    put8(rt, sp + 0x54u, 0u);
                    if (node != 0u) {
                        put(rt, node, word(rt, word_slot));
                        (void)call(rt, ctx, 0x08AAE214u, {node + 4u, temp});
                        put8(rt, node + 8u, byte_u(rt, sp + 0x54u));
                        node = word(rt, s6 + 4u);
                    }
                    put(rt, s6 + 4u, node + 0xCu);
                }
                (void)call(rt, ctx, 0x08A5B9ECu, {temp});
                const u32 next = call(rt, ctx, 0x08B5F100u, {s1});
                if (self_link == next) break;
                s1 = next;
            }
        }
        const u32 begin = word(rt, s3 + 0x3Cu);
        const u32 end = word(rt, s3 + 0x40u);
        if (begin != end) {
            const u32 span = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
            const u32 elements = span * 0xAAAAAAABu;
            u32 depth = 0u;
            if (elements != 1u) {
                u32 shifted = elements;
                do {
                    shifted >>= 1;
                    ++depth;
                } while (shifted != 1u);
                depth <<= 1;
            }
            (void)call(rt, ctx, 0x08C366E8u, {begin, end, depth, begin});
            (void)call(rt, ctx, 0x08C35C80u, {begin, end, begin});
        }
    }

    const u32 list_temp = sp + 0x2Cu;
    const u32 slot = sp + 0x3Cu;
    const u32 pair_temp = sp + 0x48u;
    const u32 first_temp = sp + 0x40u;
    put(rt, sp + 0x28u, s3);
    (void)call(rt, ctx, 0x08AAE20Cu, {list_temp, sp + 0x28u});
    u32 node = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    (void)call(rt, ctx, 0x08A5B828u, {node});
    put(rt, node, 0x08CD5E30u);
    (void)call(rt, ctx, 0x08AAE214u, {node + 0xCu, list_temp});
    put(rt, node, 0x08CD5E80u);
    put8(rt, node + 0x10u, 0u);
    put(rt, node + 0x14u, 0u);
    put(rt, node + 0x1Cu, 0u);
    put(rt, node + 0x18u, 0u);
    put(rt, sp + 0xCu, node);
    (void)call(rt, ctx, 0x08AB6F54u, {slot, sp + 0xCu});
    (void)call(rt, ctx, 0x08A5B9ECu, {list_temp});

    u32 index = 0u;
    u32 offset = 0u;
    for (;;) {
        const u32 begin = word(rt, s3 + 0x3Cu);
        const u32 span = static_cast<u32>(static_cast<s32>(word(rt, s6 + 4u) - begin) >> 2);
        const u32 elements = span * 0xAAAAAAABu;
        if (!(index < elements)) break;
        const u32 element = begin + offset;
        if (byte_u(rt, element + 8u) == 0u) {
            put8(rt, element + 8u, 1u);
            u32 target = word(rt, word(rt, s3 + 0x3Cu) + offset + 4u);
            if (target == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu, target});
            (void)call(rt, ctx, 0x08AAE358u, {sp + 0x24u, target});
            target = word(rt, sp + 0x24u);
            (void)call(rt, ctx, 0x08A5B9ECu, {sp + 0x24u});
            if (target == s3) {
                u32 object = word(rt, word(rt, s3 + 0x3Cu) + offset + 4u);
                if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu, object});
                (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 0x20u), {first_temp, object, s5});
                if (word(rt, first_temp) != 0u) (void)call(rt, ctx, 0x08AB70ECu, {object, first_temp});
            } else {
                if (target == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB39D8u});
                if (word(rt, target + 0x14u) != word(rt, target + 0x18u)) {
                    const u32 made = call(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
                    if (made + 8u != 0u) (void)call(rt, ctx, 0x08AB6F6Cu, {made + 8u, slot});
                    (void)call(rt, ctx, 0x08B5C078u, {made, s5});
                }
                u32 object = word(rt, word(rt, s3 + 0x3Cu) + offset + 4u);
                if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu, object});
                (void)call(rt, ctx, 0x08AAE358u, {first_temp, object});
                object = word(rt, first_temp);
                if (object == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB39A4u});
                (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 0x20u), {sp + 0x44u, object, s5});
                if (word(rt, sp + 0x44u) != 0u) {
                    const u32 made = call(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
                    if (made + 8u != 0u) (void)call(rt, ctx, 0x08AB6F6Cu, {made + 8u, sp + 0x44u});
                    (void)call(rt, ctx, 0x08B5C078u, {made, s5});
                }
                if (word(rt, s5) != s5) {
                    const u32 entry = word(rt, s5 + 4u);
                    if (word(rt, entry + 8u) == word(rt, slot)) {
                        (void)call(rt, ctx, 0x08B5C094u, {entry});
                        (void)call(rt, ctx, 0x08A5B9ECu, {entry + 8u});
                        (void)call(rt, ctx, 0x08A5BC54u, {entry, 0xCu, 4u});
                    } else {
                        put(rt, sp + 0x18u, s3);
                        (void)call(rt, ctx, 0x08AAE20Cu, {sp + 0x1Cu, sp + 0x18u});
                        const u32 copy = call(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
                        (void)call(rt, ctx, 0x08A5B828u, {copy});
                        put(rt, copy, 0x08CD5E30u);
                        (void)call(rt, ctx, 0x08AAE214u, {copy + 0xCu, sp + 0x1Cu});
                        put(rt, copy, 0x08CD5E80u);
                        put8(rt, copy + 0x10u, 0u);
                        put(rt, copy + 0x14u, 0u);
                        put(rt, copy + 0x1Cu, 0u);
                        put(rt, copy + 0x18u, 0u);
                        put(rt, sp + 0x8u, copy);
                        (void)call(rt, ctx, 0x08AB6F54u, {sp + 0x20u, sp + 0x8u});
                        (void)call(rt, ctx, 0x08A5BAD8u, {slot, word(rt, sp + 0x20u)});
                        (void)call(rt, ctx, 0x08A5B9ECu, {sp + 0x20u});
                        (void)call(rt, ctx, 0x08A5B9ECu, {sp + 0x1Cu});
                    }
                }
                const u32 key = word(rt, s3 + 0x3Cu) + offset;
                (void)call(rt, ctx, 0x08B85EB8u, {sp + 0x14u, word(rt, key), sp + 1u});
                const u32 key_next = key + 4u;
                const u32 made = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
                (void)call(rt, ctx, 0x08A5B828u, {made});
                put(rt, made, 0x08CD5E30u);
                (void)call(rt, ctx, 0x08AAE214u, {made + 0xCu, key_next});
                put(rt, made, 0x08CD5E58u);
                put8(rt, made + 0x10u, 0u);
                (void)call(rt, ctx, 0x08B88F44u, {made + 0x14u, sp + 0x14u});
                put(rt, sp + 0x4u, made);
                (void)call(rt, ctx, 0x08AB6F54u, {pair_temp, sp + 0x4u});
                const u32 owner = word(rt, sp + 0x14u);
                if (owner - 0xCu != 0x08D74FD0u) {
                    if (!(static_cast<s32>(call(rt, ctx, 0x08B60804u, {owner - 4u, 0xFFFFFFFFu})) > 0)) {
                        (void)call(rt, ctx, 0x08B8542Cu, {owner - 0xCu, sp});
                    }
                }
                const u32 held = word(rt, slot);
                if (held == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB39D8u});
                (void)call(rt, ctx, 0x08AB70ECu, {held, pair_temp});
                (void)call(rt, ctx, 0x08A5B9ECu, {pair_temp});
                (void)call(rt, ctx, 0x08A5B9ECu, {sp + 0x44u});
            }
            (void)call(rt, ctx, 0x08A5B9ECu, {first_temp});
        }
        ++index;
        offset += 0xCu;
    }

    const u32 final = word(rt, slot);
    if (final == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB39D8u});
    if (word(rt, final + 0x14u) != word(rt, final + 0x18u)) {
        const u32 out = word(rt, sp + 0x60u);
        if (out != slot) (void)call(rt, ctx, 0x08A5BAD8u, {out, final});
    }
    (void)call(rt, ctx, 0x08A5B9ECu, {slot});
    ctx.gpr[2] = word(rt, sp + 0x60u);
}

} // namespace
} // namespace new_batch_main_1037
} // namespace gt::game

#define NEW1037(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1037::name, gt::ReturnKind::kind)
NEW1037(0x08AAEE04u, func_08AAEE04, Word);
