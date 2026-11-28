// New batch main 1040 (main session): the three functions of this batch, re-drafted by hand from `tools.disasm`
// (the batch-308 drafts of the same three were the starting point; every body was re-read against the asm).
// Structured C: named typed locals, `G[n]` only in the argument reads, delay slots read as "before the branch / call",
// the frame sizes are the prologues' `addiu sp, sp, -N` (0x40 / 0xA0 / 0xB0), s0-s7 / fp / ra are never written.
//
//   0x088DFF0C - the race's per-frame update (448 instructions). Pass 1 counts the 0x2F8-byte entries under +0xC0
//                into the three halfwords of the state at +0x130; the "next" mark is cleared when it has run past
//                "now"; 0x088DAA84 picks the running update (0x08DBB10 + the done-mark checks + the +0x640 object's
//                releases) or the start / advance of the run (the start callback slots 0x40 / 0x48, the 0x2F8-entry
//                walk through 0x088E84B8, the step advance). Both end in the one exit: the optional +0x134 slot-0x54
//                release and 0x088D9CCC, which when it answers 0 adds the frame duration (t0) to +0x98 -- that sum is
//                the v0 the original returns.
//   0x08A4E6F4 - the sprite-list update (453 instructions): scroll/box tests, a textured pass through the object at
//                +0xA4 and a sprite-table pass through +0x78, each with a shadow and a four-quad outline, and the
//                scroll accumulated into +0x28 / +0x2C.
//   0x08AACE40 - the container filler (436 instructions): mode 0 empties the record table's slots; any other mode
//                builds one node per 0xC-byte record of 0x08CDF80C plus the constant 0x08CB3374 node and inserts
//                them into the map at +0x68, with the inlined libstdc++ string release idiom around every temporary.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1040 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A call whose callee-saved result registers are kept available: $a1 as the callee left it (a caller's next call can
// read it) plus v0 / v1 / f0, as after the `jal`.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A call with float arguments ($f12-$f19) and a float result ($f0).
float call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                  std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// ---- 0x08A4E6F4's sprite helpers ---------------------------------------------------------------------------------

// The s16 sprite extents scale by 0x3F22E8BA (0.6363636) before the object's +0x20 / +0x24 factors.
constexpr float kTexelScale = 0.6363636f;

// $f20-$f31 are callee-saved: saved at entry and put back on the one return.
struct SavedFloats {
    AllegrexContext &ctx;
    float saved[12];
    explicit SavedFloats(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0; i < 12u; ++i) saved[i] = c.fpr[20u + i];
    }
    ~SavedFloats() {
        for (unsigned i = 0; i < 12u; ++i) ctx.fpr[20u + i] = saved[i];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;
};

// 0x08A50AB8(self, colour) is the graphics library's colour setter (it moves a1 into a0).
void set_colour(Runtime &rt, AllegrexContext &ctx, u32 self, u32 colour) {
    (void)call(rt, ctx, 0x08A50AB8u, {self, colour});
}
// 0x08A50CF4(self, x0, y0, x1, y1, u0, v0, u1, v1): a textured quad, the floats in $f12-$f19.
void textured_quad(Runtime &rt, AllegrexContext &ctx, u32 self, float x0, float y0, float x1, float y1, const float uv[4]) {
    (void)call_floats(rt, ctx, 0x08A50CF4u, {self}, {x0, y0, x1, y1, uv[0], uv[1], uv[2], uv[3]});
}
// 0x08A50EA8(self, x0, y0, x1, y1): the same quad without a texture.
void flat_quad(Runtime &rt, AllegrexContext &ctx, u32 self, float x0, float y0, float x1, float y1) {
    (void)call_floats(rt, ctx, 0x08A50EA8u, {self}, {x0, y0, x1, y1});
}

// The four border quads (at +-half_x / +-half_y around the box, in the colour at +0x44) and then the sprite itself in
// the colour at +0x3C. `textured` picks the callee; `uv` is only read when it is set.
void draw_outlined_sprite(Runtime &rt, AllegrexContext &ctx, u32 self, bool textured, float x0, float y0, float x1,
                          float y1, float half_x, float half_y, const float uv[4]) {
    const float left0 = x0 - half_x;
    const float right0 = x1 - half_x;
    const float right1 = x1 + half_x;
    const float left1 = x0 + half_x;
    const float top0 = y0 - half_y;
    const float bottom0 = y1 - half_y;
    const float bottom1 = y1 + half_y;
    const float top1 = y0 + half_y;
    set_colour(rt, ctx, self, word(rt, self + 0x44u));
    if (textured) {
        textured_quad(rt, ctx, self, left0, top0, right0, bottom0, uv);
        textured_quad(rt, ctx, self, left1, top0, right1, bottom0, uv);
        textured_quad(rt, ctx, self, left1, top1, right1, bottom1, uv);
        textured_quad(rt, ctx, self, left0, top1, right0, bottom1, uv);
    } else {
        flat_quad(rt, ctx, self, left0, top0, right0, bottom0);
        flat_quad(rt, ctx, self, left1, top0, right1, bottom0);
        flat_quad(rt, ctx, self, left1, top1, right1, bottom1);
        flat_quad(rt, ctx, self, left0, top1, right0, bottom1);
    }
    set_colour(rt, ctx, self, word(rt, self + 0x3Cu));
    if (textured) {
        textured_quad(rt, ctx, self, x0, y0, x1, y1, uv);
    } else {
        flat_quad(rt, ctx, self, x0, y0, x1, y1);
    }
}

// ---- 0x08AACE40's string / map helpers ---------------------------------------------------------------------------

constexpr u32 kRecordTable = 0x08CDF80Cu;  // 0xC-byte records {index, text1, text2}, the loop key at +0x10
constexpr u32 kEmptyRep = 0x08D74FD0u;     // the shared empty string's rep (data - 0xC)
constexpr u32 kConstantText = 0x08CB3374u;

// The libstdc++ string release idiom of this binary: `data` points into a rep whose header sits at data-0xC; the shared
// empty rep skips it, otherwise 0x08B60804(data-4, -1) drops the count and 0x08B8542C(data-0xC, scratch) frees the rep
// when it reaches zero.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 scratch) {
    if (data - 0xCu == kEmptyRep) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    (void)call(rt, ctx, 0x08B8542Cu, {data - 0xCu, scratch});
}
void string_from(Runtime &rt, AllegrexContext &ctx, u32 dest, u32 text, u32 scratch) {
    (void)call(rt, ctx, 0x08B85EB8u, {dest, text, scratch});
}
void string_copy(Runtime &rt, AllegrexContext &ctx, u32 dest, u32 source) {
    (void)call(rt, ctx, 0x08B88F44u, {dest, source});
}
// The hash of the string at `data` (its length is the rep word at data-0xC): 0x08B29E88(0, data, length).
u32 string_hash(Runtime &rt, AllegrexContext &ctx, u32 data) {
    return call(rt, ctx, 0x08B29E88u, {0u, data, word(rt, data - 0xCu)});
}

// The inlined std::map insertion: find `key` in the map ending at `end_marker` (0x08C34770); when its hash (at hash) and
// the string compare say it is absent, copy the key into `copy` with the hash and a zero, let 0x08C34C58 make room, and
// store `value` at node+0x18. The compare's length scratch is len_a / len_b (the shorter of the two lengths).
struct MapSlots {
    u32 key;
    u32 copy;
    u32 hash;
    u32 release;
    u32 len_a;
    u32 len_b;
};
void map_set(Runtime &rt, AllegrexContext &ctx, u32 map, u32 end_marker, const MapSlots &slots, u32 value) {
    const u32 found = call(rt, ctx, 0x08C34770u, {map, slots.key});
    bool insert = found == end_marker;
    if (!insert) {
        const u32 hash = word(rt, slots.hash);
        const u32 node_hash = word(rt, found + 0x14u);
        if (hash < node_hash) {
            insert = true;
        } else if (!(node_hash < hash)) {
            const u32 key_data = word(rt, slots.key);
            const u32 key_length = word(rt, key_data - 0xCu);
            const u32 node_key = word(rt, found + 0x10u);
            const u32 node_length = word(rt, node_key - 0xCu);
            put(rt, slots.len_a, key_length);
            put(rt, slots.len_b, node_length);
            const u32 shortest = key_length <= node_length ? key_length : node_length;
            const s32 compared = static_cast<s32>(call(rt, ctx, 0x08B36170u, {key_data, node_key, shortest}));
            const u32 order = compared != 0 ? static_cast<u32>(compared) : key_length - node_length;
            insert = static_cast<s32>(order) < 0;
        }
    }
    u32 node = found;
    if (insert) {
        string_copy(rt, ctx, slots.copy, slots.key);
        put(rt, slots.copy + 4u, word(rt, slots.hash));
        put(rt, slots.copy + 8u, 0u);
        node = call(rt, ctx, 0x08C34C58u, {map, found, slots.copy});
        release_string(rt, ctx, word(rt, slots.copy), slots.release);
    }
    put(rt, node + 0x18u, value);
}

// The race's per-frame update. Everything below is one exit: the original's branches all reach 0x088E0510.

// The sprite-list update. a1 / a2 are masked flag bytes; +0x58's 0x300 field picks the scroll handling.
void func_08A4E6F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag_a = ctx.gpr[5] & 0xFFu;
    const u32 flag_b = ctx.gpr[6] & 0xFFu;
    const SavedFloats saved_floats(ctx);
    GuestFrame frame(ctx, 0xA0u);
    ctx.gpr[5] = flag_a;

    float shift_x = 0.0f;
    float shift_y = 0.0f;
    if ((word(rt, self + 0x58u) & 0x300u) == 0x200u) {
        if (flag_a == 0u) {
            shift_y = 0.0f;
            shift_x = shift_y;
            put_float(rt, self + 0xA0u, float_at(rt, self + 0x28u));
        } else {
            const float pending = float_at(rt, self + 0xA0u);
            if (0.0f < pending) {
                const float base_x = float_at(rt, self + 0x68u);
                const float scale_y = float_at(rt, self + 0x24u);
                shift_x = -pending;
                put_float(rt, self + 0xA0u, 0.0f);
                shift_y = psp_mul(base_x + 14.0f, scale_y);
            } else {
                const float base_x = float_at(rt, self + 0x68u);
                const float scale_y = float_at(rt, self + 0x24u);
                put_float(rt, self + 0x28u, 0.0f);
                (void)call_floats(rt, ctx, 0x08A4CF24u, {self}, {psp_mul(base_x + 14.0f, scale_y)});
            }
        }
    } else {
        put_float(rt, self + 0xA0u, 0.0f);
    }

    if (flag_b != 0u) {
        const float origin_x = float_at(rt, self + 0x00u);
        const float edge_right = origin_x + float_at(rt, self + 0x08u);
        const float bound_x = float_at(rt, self + 0x04u);
        const float bound_y = float_at(rt, self + 0x0Cu);
        const float base_x = float_at(rt, self + 0x4Cu);
        const float base_y = float_at(rt, self + 0x48u);
        const u32 sprite_table = word(rt, self + 0x78u);
        u32 index = 0u;
        if (static_cast<s32>(word(rt, self + 0x9Cu)) > 0) {
            do {
                const u32 entry = word(rt, self + 0x98u) + (index << 4);
                put(rt, frame.at(8u), word(rt, entry + 8u));
                const u32 kind = rt.memory().load16(entry + 0xCu);
                rt.memory().store16(frame.at(0xCu), kind);
                const u32 texture = word(rt, self + 0xA4u);
                if (kind == 2u && texture != 0u) {
                    const u32 info = call(rt, ctx, 0x08A4DC8Cu, {texture, rt.memory().load16(frame.at(8u))});
                    const float entry_x = float_at(rt, entry);
                    const float entry_y = float_at(rt, entry + 4u);
                    const float centre_x = shift_x + entry_x;
                    const float centre_y = shift_y + entry_y;
                    const float texel_x = psp_mul(static_cast<float>(signed_half(rt, info + 6u)), kTexelScale);
                    const float texel_y = psp_mul(static_cast<float>(signed_half(rt, info + 8u)), kTexelScale);
                    const float far_x = centre_x + psp_mul(texel_x, float_at(rt, self + 0x20u));
                    const float far_y = centre_y + psp_mul(texel_y, float_at(rt, self + 0x24u));
                    const bool in_view = base_y + centre_x < edge_right && origin_x < base_y + far_x &&
                                         base_x + centre_y < bound_x + bound_y && bound_x < base_x + far_y;
                    if (in_view) {
                        const float width = call_floats(rt, ctx, 0x08A4DD0Cu, {texture}, {});
                        const float height = call_floats(rt, ctx, 0x08A4DD24u, {texture}, {});
                        const float u0 = static_cast<float>(signed_half(rt, info + 2u)) / width;
                        const float v0 = static_cast<float>(signed_half(rt, info + 4u)) / height;
                        const float u1 = u0 + static_cast<float>(signed_half(rt, info + 6u)) / width;
                        const float v1 = v0 + static_cast<float>(signed_half(rt, info + 8u)) / height;
                        const float uv[4] = {u0, v0, u1, v1};
                        (void)call(rt, ctx, 0x08A4DD3Cu, {texture});
                        put(rt, self + 0x84u, 0u);
                        put(rt, self + 0x8Cu, 0u);
                        const float shadow = float_at(rt, self + 0x5Cu);
                        const float shadow_x = psp_mul(shadow, float_at(rt, self + 0x20u));
                        const float shadow_y = psp_mul(shadow, float_at(rt, self + 0x24u));
                        if (0.0f < shadow_x || 0.0f < shadow_y) {
                            set_colour(rt, ctx, self, word(rt, self + 0x40u));
                            textured_quad(rt, ctx, self, centre_x + shadow_x, centre_y + shadow_y, far_x + shadow_x,
                                          far_y + shadow_y, uv);
                        }
                        const float outline = float_at(rt, self + 0x60u);
                        const float outline_x = psp_mul(outline, float_at(rt, self + 0x20u));
                        const float outline_y = psp_mul(outline, float_at(rt, self + 0x24u));
                        if (0.0f < outline_x || 0.0f < outline_y) {
                            draw_outlined_sprite(rt, ctx, self, true, centre_x, centre_y, far_x, far_y, outline_x,
                                                 outline_y, uv);
                        } else {
                            set_colour(rt, ctx, self, word(rt, self + 0x3Cu));
                            textured_quad(rt, ctx, self, centre_x, centre_y, far_x, far_y, uv);
                        }
                    }
                } else {
                    (void)call(rt, ctx, 0x08A50634u, {sprite_table, frame.at(8u), frame.at(0u), frame.at(4u)});
                    const float entry_x = float_at(rt, entry);
                    const float entry_y = float_at(rt, entry + 4u);
                    const float centre_x = shift_x + entry_x;
                    const float centre_y = shift_y + entry_y;
                    const float ratio = 14.0f / static_cast<float>(static_cast<s32>(word(rt, frame.at(4u))));
                    const float extent_x =
                        psp_mul(psp_mul(ratio, static_cast<float>(static_cast<s32>(word(rt, frame.at(0u))))),
                                float_at(rt, self + 0x20u));
                    const float far_x = centre_x + extent_x;
                    const float far_y = centre_y + psp_mul(float_at(rt, self + 0x24u), 14.0f);
                    const bool in_view = base_y + centre_x < edge_right && origin_x < base_y + far_x &&
                                         base_x + centre_y < bound_x + bound_y && bound_x < base_x + far_y;
                    if (in_view) {
                        (void)call(rt, ctx, 0x08A50AC0u, {self, sprite_table, frame.at(8u)});
                        const float shadow = float_at(rt, self + 0x5Cu);
                        const float shadow_x = psp_mul(shadow, float_at(rt, self + 0x20u));
                        const float shadow_y = psp_mul(shadow, float_at(rt, self + 0x24u));
                        if (0.0f < shadow_x || 0.0f < shadow_y) {
                            set_colour(rt, ctx, self, word(rt, self + 0x40u));
                            flat_quad(rt, ctx, self, centre_x + shadow_x, centre_y + shadow_y, far_x + shadow_x,
                                      far_y + shadow_y);
                        }
                        const float outline = float_at(rt, self + 0x60u);
                        const float outline_x = psp_mul(outline, float_at(rt, self + 0x20u));
                        const float outline_y = psp_mul(outline, float_at(rt, self + 0x24u));
                        if (0.0f < outline_x || 0.0f < outline_y) {
                            draw_outlined_sprite(rt, ctx, self, false, centre_x, centre_y, far_x, far_y, outline_x,
                                                 outline_y, nullptr);
                        } else {
                            set_colour(rt, ctx, self, word(rt, self + 0x3Cu));
                            flat_quad(rt, ctx, self, centre_x, centre_y, far_x, far_y);
                        }
                    }
                }
            } while (static_cast<s32>(++index) < static_cast<s32>(word(rt, self + 0x9Cu)));
        }
    }

    const float next_x = shift_x + float_at(rt, self + 0x28u);
    const float next_y = shift_y + float_at(rt, self + 0x2Cu);
    put(rt, self + 0x9Cu, 0u);
    put_float(rt, self + 0x28u, next_x);
    put_float(rt, self + 0x2Cu, next_y);
}

// The container filler: mode 0 empties the record slots, any other mode fills them (nodes + two map insertions each).
void func_08AACE40(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 mode = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0xB0u);
    ctx.gpr[5] = mode;

    if (mode == 0u) {
        // 0x08AAD494: drop every slot self[record.index] and then the node at self+0x80.
        const u32 holder = frame.at(0x10u);
        if (word(rt, kRecordTable + 4u) != 0u) {
            u32 record = kRecordTable;
            for (;;) {
                const u32 slot = self + (word(rt, record) << 2);
                (void)call(rt, ctx, 0x08A69798u, {holder, 0u});
                (void)call(rt, ctx, 0x08A6A23Cu, {slot, holder});
                (void)call(rt, ctx, 0x08A69D68u, {holder});
                const u32 next = word(rt, record + 0x10u);
                record += 0xCu;
                if (next == 0u) break;
            }
        }
        (void)call(rt, ctx, 0x08A69D3Cu, {word(rt, self + 0x80u)});
        return;
    }

    // 0x08AAD4B0: the map at self+0x68 (its end marker is the address self+0x6C).
    const u32 map = self + 0x68u;
    const u32 map_end = self + 0x6Cu;
    const u32 key = frame.at(0x3Cu);
    const MapSlots first_slots{key, frame.at(0x5Cu), frame.at(0x40u), frame.at(8u), frame.at(0x2Cu), frame.at(0x30u)};
    const MapSlots second_slots{key, frame.at(0x50u), frame.at(0x40u), frame.at(5u), frame.at(0x2Cu), frame.at(0x30u)};

    u32 text = word(rt, kRecordTable + 4u);
    if (text != 0u) {
        u32 record = kRecordTable;
        for (;;) {
            // The node under self[record.index]: {refcount 0, string text1, index}.
            string_from(rt, ctx, frame.at(0x28u), text, frame.at(0xFu));
            const u32 created = call(rt, ctx, 0x08A69520u, {0xCu});
            put(rt, created, 0u);
            string_copy(rt, ctx, created + 4u, frame.at(0x28u));
            put(rt, created + 8u, word(rt, record));
            release_string(rt, ctx, word(rt, frame.at(0x28u)), frame.at(0xAu));
            const u32 holder = frame.at(0x24u);
            (void)call(rt, ctx, 0x08A69798u, {holder, created});
            (void)call(rt, ctx, 0x08A6A23Cu, {self + (word(rt, record) << 2), holder});
            (void)call(rt, ctx, 0x08A69D68u, {holder});

            // The map insertion keyed by the record's first text.
            string_from(rt, ctx, frame.at(0x20u), word(rt, record + 4u), frame.at(0xEu));
            string_copy(rt, ctx, key, frame.at(0x20u));
            put(rt, frame.at(0x40u), string_hash(rt, ctx, word(rt, frame.at(0x20u))));
            release_string(rt, ctx, word(rt, frame.at(0x20u)), frame.at(9u));
            map_set(rt, ctx, map, map_end, first_slots, created);
            release_string(rt, ctx, word(rt, key), frame.at(7u));

            // The map insertion keyed by the record's second text.
            string_from(rt, ctx, frame.at(0x1Cu), word(rt, record + 8u), frame.at(0xDu));
            string_copy(rt, ctx, key, frame.at(0x1Cu));
            put(rt, frame.at(0x40u), string_hash(rt, ctx, word(rt, frame.at(0x1Cu))));
            release_string(rt, ctx, word(rt, frame.at(0x1Cu)), frame.at(6u));
            map_set(rt, ctx, map, map_end, second_slots, created);
            release_string(rt, ctx, word(rt, key), frame.at(4u));

            text = word(rt, record + 0x10u);
            record += 0xCu;
            if (text == 0u) break;
        }
    }

    // 0x08AAD230: the constant node at self+0x80 {refcount 0, 0x08CB3374, 0x1A}, inserted under the same text.
    string_from(rt, ctx, frame.at(0x18u), kConstantText, frame.at(0xCu));
    const u32 special = call(rt, ctx, 0x08A69520u, {0xCu});
    put(rt, special, 0u);
    string_copy(rt, ctx, special + 4u, frame.at(0x18u));
    put(rt, special + 8u, 0x1Au);
    put(rt, self + 0x80u, special);
    release_string(rt, ctx, word(rt, frame.at(0x18u)), frame.at(3u));
    (void)call(rt, ctx, 0x08A69638u, {word(rt, self + 0x80u)});

    string_from(rt, ctx, frame.at(0x14u), kConstantText, frame.at(0xBu));
    string_copy(rt, ctx, frame.at(0x34u), frame.at(0x14u));
    put(rt, frame.at(0x38u), string_hash(rt, ctx, word(rt, frame.at(0x14u))));
    release_string(rt, ctx, word(rt, frame.at(0x14u)), frame.at(2u));
    map_set(rt, ctx, map, map_end,
            MapSlots{frame.at(0x34u), frame.at(0x44u), frame.at(0x38u), frame.at(1u), frame.at(0x2Cu), frame.at(0x30u)},
            word(rt, self + 0x80u));
    release_string(rt, ctx, word(rt, frame.at(0x34u)), frame.at(0u));
}

} // namespace
} // namespace new_batch_main_1040
} // namespace gt::game

#define NEWMAIN1040(address, name, kind)                                                                          \
    GT_DECOMP(address, gt::game::new_batch_main_1040::name, gt::ReturnKind::kind)
NEWMAIN1040(0x08A4E6F4u, func_08A4E6F4, Void);
NEWMAIN1040(0x08AACE40u, func_08AACE40, Void);
