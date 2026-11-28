// New batch (main session 1602): four functions drafted before that mismatched the original in the gate, rewritten
// from tools.disasm as structured C (the gate's finding for each one is in the batch notes). Draft batch: not yet
// verified by the differential gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1602 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The batch files' convention: a guest call whose result becomes v0.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A call with the float arguments in $f12.. like the original's; v0 is the answer (the float results are not read back).
u32 call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
           std::initializer_list<float> float_arguments) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments, float_arguments).v0;
    return ctx.gpr[2];
}
// The compiler's virtual call: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0; v0 is the answer.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    ctx.gpr[2] = call_guest(rt, ctx, function, arguments);
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// The 4x4 matrix 0x08B4E648 / 0x08B4E9D4 build: identity with (x, y, z, 1) in the last row.
void put_matrix(Runtime &rt, u32 at, float x, float y, float z) {
    put_float(rt, at + 0x00u, 1.0f);
    put_float(rt, at + 0x04u, 0.0f);
    put_float(rt, at + 0x08u, 0.0f);
    put_float(rt, at + 0x0Cu, 0.0f);
    put_float(rt, at + 0x10u, 0.0f);
    put_float(rt, at + 0x14u, 1.0f);
    put_float(rt, at + 0x18u, 0.0f);
    put_float(rt, at + 0x1Cu, 0.0f);
    put_float(rt, at + 0x20u, 0.0f);
    put_float(rt, at + 0x24u, 0.0f);
    put_float(rt, at + 0x28u, 1.0f);
    put_float(rt, at + 0x2Cu, 0.0f);
    put_float(rt, at + 0x30u, x);
    put_float(rt, at + 0x34u, y);
    put_float(rt, at + 0x38u, z);
    put_float(rt, at + 0x3Cu, 1.0f);
}

// 0x08A4EE1C (a0 = object, a1 = a two-word kind descriptor, a2 = a flag byte): the per-frame update of the state row at
// +0x90. The kind at info+0 picks the path; each path moves the row by a step taken from the object's own fields, calls
// the row helpers (0x08A4CEDC / 0x08A4CF24 move it and answer a status, 0x08A4E6F4 resets, 0x08A4EE08 sets its flag) and,
// on the common path, appends one 16-byte entry to the ring at +0x98 (count at +0x9C, step at +0x20/+0x24, interpolation
// factors f21/f22) before asking 0x08A4CEDC once more. v0 = the final mode test on the word at +0x58, 0 on every earlier
// return (the asm ends every one of them with `a0 = 0`).
// The 0x08A4F214 re-centre block runs when the descriptor's kind is an ASCII digit and the mode has no 0x800 bit, or when
// it is not a digit and the mode has no 0x400 bit.

// 0x0891F110 (a0 = entity, a1 = the asset object the index is resolved on, a2 = a step record: a float at +0 and a signed
// word at +0xC, a3 = an index, signed): advances the entity's phase counter (+0x23C) through 0..3 with the height at
// +0x238 as the timer, blends the two table rows the phase picks out of 0x08CF27B4 (7 floats each) by a 0..1 factor, writes
// them to +0x21C..+0x234 and rebuilds the entity's transform at +0x10 from the referenced node's own two vectors. When the
// index does not resolve, the transform row at +0x1E4 is cleared instead. Both exits leave v0 = entity + 0x1E4 (the row
// the asm loads into v0 before the stores) and, on the node path, v1 = the row at +0x50 it copied there.
void func_0891F110(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const s32 index = static_cast<s32>(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x140u);
    const float blend = float_at(rt, source);
    const float radians = std::bit_cast<float>(0x3C8EFA35u);  // degrees to radians
    const u32 row_a = self + 0x1F8u;
    const u32 row_b = self + 0x204u;
    const u32 row_c = self + 0x210u;

    const s32 steps = static_cast<s32>(word(rt, source + 0xCu));
    put_float(rt, self + 0x238u, float_at(rt, self + 0x238u) + static_cast<float>(steps));
    call_f(rt, ctx, 0x08B28DE4u, {row_a}, {-10.0f, 2.0f});
    call_f(rt, ctx, 0x08B28DE4u, {row_b}, {180.0f, 2.0f});
    call_f(rt, ctx, 0x08B28DE4u, {row_c}, {4.0f, 1.0f});
    call_f(rt, ctx, 0x08B28F50u, {row_a}, {blend, 3.0f});
    call_f(rt, ctx, 0x08B28F50u, {row_b}, {blend, 2.0f});
    call_f(rt, ctx, 0x08B28F50u, {row_c}, {blend, 2.0f});

    // The phase: 0 starts at the bottom (a factor of the 180-unit ramp), 1 and 2 sit on one table row each, 3 hands over to
    // the node's own state (0x089281E4) or sits on the last row.
    float factor = 0.0f;
    u32 previous = 0u;
    u32 current = 1u;
    const u32 phase = word(rt, self + 0x23Cu);
    if (phase == 0u) {
        const float height = float_at(rt, self + 0x238u);
        factor = height / 180.0f;
        if (180.0f <= height) {
            put(rt, self + 0x23Cu, 1u);
            put(rt, self + 0x240u, 0u);
        }
        if (1.0f < factor) factor = 1.0f;
    } else if (phase == 1u) {
        if (word(rt, self + 0x240u) == 0u) {
            put(rt, self + 0x240u, 1u);
            put_byte(rt, self + 0xC0u, 1u);
        }
        if (240.0f <= float_at(rt, self + 0x238u)) {
            put(rt, self + 0x23Cu, 2u);
            put(rt, self + 0x240u, 0u);
        }
        previous = 1u;
        current = 1u;
    } else if (phase == 2u) {
        if (word(rt, self + 0x240u) == 0u) {
            put(rt, self + 0x240u, 1u);
            put_byte(rt, self + 0xC0u, 1u);
        }
        if (300.0f <= float_at(rt, self + 0x238u)) {
            put(rt, self + 0x23Cu, 3u);
            put(rt, self + 0x240u, 0u);
        }
        previous = 2u;
        current = 2u;
    } else if (phase == 3u) {
        if (word(rt, self + 0x240u) == 0u) {
            put(rt, self + 0x240u, 1u);
            put_byte(rt, self + 0xC0u, 1u);
        }
        const u32 node = word(rt, self + 0x1E0u);
        if (rt.memory().load8(node + 0x1FBCu) == 0u && rt.memory().load8(node + 0x1FBDu) == 0u) {
            call(rt, ctx, 0x089281E4u, {node, 0u});
            put(rt, self + 0x23Cu, word(rt, self + 0x23Cu) + 1u);
            put(rt, self + 0x240u, 0u);
        } else if (420.0f <= float_at(rt, self + 0x238u)) {
            call(rt, ctx, 0x089281E4u, {node, 3u});
            put(rt, self + 0x23Cu, word(rt, self + 0x23Cu) + 1u);
            put(rt, self + 0x240u, 0u);
            previous = 3u;
            current = 3u;
        } else {
            previous = 3u;
            current = 3u;
        }
    } else if (1.0f < factor) {
        factor = 1.0f;
    }

    // The blend of the two 28-byte table rows, then the seven results into the entity.
    const u32 table = 0x08CF27B4u;
    call_f(rt, ctx, 0x0891C7C4u, {frame.at(0x20), self, table + previous * 28u, table + current * 28u}, {factor});
    put_float(rt, self + 0x21Cu, float_at(rt, frame.at(0x20)));
    put_float(rt, self + 0x220u, float_at(rt, frame.at(0x24)));
    put_float(rt, self + 0x224u, float_at(rt, frame.at(0x28)));
    put_float(rt, self + 0x228u, float_at(rt, frame.at(0x2C)));
    put_float(rt, self + 0x22Cu, float_at(rt, frame.at(0x30)));
    put_float(rt, self + 0x230u, float_at(rt, frame.at(0x34)));
    put_float(rt, self + 0x234u, float_at(rt, frame.at(0x38)));
    call(rt, ctx, 0x08B4E648u, {self + 0x10u});
    call(rt, ctx, 0x08B4E648u, {frame.at(0x40)});
    call(rt, ctx, 0x08B4E648u, {frame.at(0x80)});

    if (index >= 0 && call(rt, ctx, 0x088DC90Cu, {owner, static_cast<u32>(index)}) == 1u) {
        const u32 node = call(rt, ctx, 0x088DCD90u, {owner, static_cast<u32>(index)});
        if (node != 0u) {
            // The node's two vectors: +0 is the translation row, +0x10 the three angles. a1 is handed on as the index the
            // two resolutions above were given (neither callee writes it).
            const u32 index_value = static_cast<u32>(index);
            const u32 origin = call_virtual(rt, ctx, node, 0x38u, {node, index_value});
            put_float(rt, frame.at(0x00), float_at(rt, origin));
            put_float(rt, frame.at(0x04), float_at(rt, origin + 4u));
            put_float(rt, frame.at(0x08), float_at(rt, origin + 8u));
            put_float(rt, frame.at(0x0C), float_at(rt, origin + 0xCu));
            const u32 angles = call_virtual(rt, ctx, node, 0x3Cu, {node, index_value});
            put_float(rt, frame.at(0x10), float_at(rt, angles));
            put_float(rt, frame.at(0x14), float_at(rt, angles + 4u));
            put_float(rt, frame.at(0x18), float_at(rt, angles + 8u));
            put_float(rt, frame.at(0x1C), float_at(rt, angles + 0xCu));

            put_matrix(rt, frame.at(0xC0), float_at(rt, frame.at(0x00)), float_at(rt, frame.at(0x04)),
                       float_at(rt, frame.at(0x08)));
            call(rt, ctx, 0x08B4E9D4u, {frame.at(0x40), frame.at(0xC0)});
            call_f(rt, ctx, 0x08B4F3B0u, {frame.at(0x40)}, {float_at(rt, frame.at(0x14))});
            call_f(rt, ctx, 0x08B4F2E4u, {frame.at(0x40)}, {float_at(rt, frame.at(0x10))});
            call_f(rt, ctx, 0x08B4F47Cu, {frame.at(0x40)}, {-float_at(rt, frame.at(0x18))});
            call_f(rt, ctx, 0x08B4F3B0u, {frame.at(0x80)}, {float_at(rt, self + 0x22Cu) * radians});
            call_f(rt, ctx, 0x08B4F2E4u, {frame.at(0x80)}, {float_at(rt, self + 0x228u) * radians});
            put_matrix(rt, frame.at(0xC0), float_at(rt, self + 0x21Cu), float_at(rt, self + 0x220u),
                       float_at(rt, self + 0x224u));
            call(rt, ctx, 0x08B4E9D4u, {frame.at(0x80), frame.at(0xC0)});
            call(rt, ctx, 0x08B4E9D4u, {self + 0x10u, frame.at(0x40)});
            call(rt, ctx, 0x08B4E9D4u, {self + 0x10u, frame.at(0x80)});

            // The transform's last row becomes the position row at +0x1E4 and +0x50, the two angles the Y and X rotation.
            put_float(rt, self + 0x1E4u, float_at(rt, self + 0x40u));
            put_float(rt, self + 0x1E8u, float_at(rt, self + 0x44u));
            put_float(rt, self + 0x1ECu, float_at(rt, self + 0x48u));
            put_float(rt, self + 0x1F0u, float_at(rt, self + 0x4Cu));
            put_float(rt, self + 0x50u, float_at(rt, self + 0x1E4u));
            put_float(rt, self + 0x54u, float_at(rt, self + 0x1E8u));
            put_float(rt, self + 0x58u, float_at(rt, self + 0x1ECu));
            put_float(rt, self + 0x5Cu, float_at(rt, self + 0x1F0u));
            put_float(rt, self + 0x60u, float_at(rt, self + 0x228u) * radians);
            put_float(rt, self + 0x64u, float_at(rt, self + 0x22Cu) * radians);
            put(rt, self + 0x6Cu, 0u);
            put(rt, self + 0x68u, 0u);
            ctx.gpr[3] = self + 0x50u;
            ctx.gpr[2] = self + 0x1E4u;
            return;
        }
    }
    // No node: clear the position row (the asm zeroes +0x1E4, +0x1F0, +0x1E8 and +0x1EC in that order).
    put(rt, self + 0x1E4u, 0u);
    put(rt, self + 0x1F0u, 0u);
    put(rt, self + 0x1E8u, 0u);
    put(rt, self + 0x1ECu, 0u);
    ctx.gpr[2] = self + 0x1E4u;
}

// 0x08B5E2CC (this = a0, a1 = a value stored at this + 0): fills a registry descriptor and installs the fourteen table
// entries it points at. this + 0 is the value, +4 / +0xC are the two 14-word tables, +8 is the count 14 and +0x10 the second
// table object (0x08D726C0) whose 1 fixed word and 5 zero words are filled here. Each table object below is a
// {vtable, flag = 1, one-word payload} triple whose payload gets its own initialiser (0x08B64CD0, 0x08B647C0, 0x08B60DB8,
// 0x08B60910, 0x08B60C98, 0x08C5E298, 0x08C5DC90) and is then registered by 0x08B5D3DC(this, key, object); every object is
// re-initialised here on each call, the flag word included (a stale flag made the register step count up). v0 = the address
// of the last table slot written (0x08D72644 + 4 * the last key's index), v1 = 0x08D721DC.
void func_08B5E2CC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first_table = 0x08D72644u;
    const u32 second_table = 0x08D7267Cu;
    const u32 table_object = 0x08D726C0u;
    const u32 table_text = 0x08D726B4u;
    const u32 stream = 0x08D72424u;
    const u32 flag_object = 0x08D723F0u;

    put(rt, self, ctx.gpr[5]);
    put(rt, self + 8u, 0xEu);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 4u, second_table);
    put(rt, self + 0x0Cu, first_table);
    for (u32 i = 0u; i != 0xEu; ++i) {
        put(rt, first_table + 4u * i, 0u);
        put(rt, second_table + 4u * i, 0u);
    }

    // The second table object: its vtable word, then the helper's constant moved into the byte pair and five words cleared.
    put(rt, self + 0x10u, table_object);
    put(rt, table_object, table_text);
    const u32 text = call(rt, ctx, 0x08B5C274u);
    put_byte(rt, table_text, rt.memory().load8(text));
    put_byte(rt, table_text + 1u, rt.memory().load8(text + 1u));
    for (u32 offset = 4u; offset != 0x18u; offset += 4u) put(rt, table_object + offset, 0u);

    // The stream-like object at 0x08D72424 (a1 = a2 = 0, a3 = 1).
    call(rt, ctx, 0x08B64CD0u, {stream, 0u, 0u, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D721D4u, stream});
    // A flag-only object at 0x08D723F0 (a1 = 1).
    call(rt, ctx, 0x08B647C0u, {flag_object, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D721C8u, flag_object});

    // 0x08D7240C: vtable 0x08CDB798, one-word payload at 0x08D7232C (vtable 0x08CDBAD8, flag 1, the rest cleared to +0x64).
    const u32 word_payload = 0x08D7232Cu;
    const u32 word_object = 0x08D7240Cu;
    put(rt, word_payload, 0x08CDBAD8u);
    put(rt, word_object + 4u, 1u);
    put(rt, word_object + 8u, word_payload);
    put(rt, word_payload + 4u, 1u);
    put(rt, word_payload + 8u, 0u);
    put(rt, word_payload + 0x0Cu, 0u);
    put_byte(rt, word_payload + 0x10u, 0u);
    put(rt, word_payload + 0x14u, 0u);
    put(rt, word_payload + 0x18u, 0u);
    put(rt, word_payload + 0x1Cu, 0u);
    put(rt, word_payload + 0x20u, 0u);
    put_byte(rt, word_payload + 0x24u, 0u);
    put_byte(rt, word_payload + 0x25u, 0u);
    put_byte(rt, word_payload + 0x64u, 0u);
    put(rt, word_object, 0x08CDB798u);
    call(rt, ctx, 0x08B60DB8u, {word_object, 0u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77484u, word_object});

    // Two more {vtable, flag = 1} objects.
    const u32 plain_a = 0x08D72404u;
    put(rt, plain_a + 4u, 1u);
    put(rt, plain_a, 0x08CDB958u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77480u, plain_a});
    const u32 plain_b = 0x08D723FCu;
    put(rt, plain_b + 4u, 1u);
    put(rt, plain_b, 0x08CDB998u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D7747Cu, plain_b});

    // 0x08D72418: {vtable 0x08CDB7C0, flag 1, the shared word 0x08B5C230()}.
    const u32 shared_object = 0x08D72418u;
    put(rt, shared_object + 4u, 1u);
    put(rt, shared_object, 0x08CDB7C0u);
    put(rt, shared_object + 8u, call(rt, ctx, 0x08B5C230u));
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77468u, shared_object});

    // 0x08D723D8: vtable 0x08CDB838, one-word payload at 0x08D722A4 (vtable 0x08CDBAB8, flag 1, bytes +0x10..+0x12 and
    // +0x43 and words +8..+0x34 cleared).
    const u32 byte_payload = 0x08D722A4u;
    const u32 byte_object = 0x08D723D8u;
    put(rt, byte_payload, 0x08CDBAB8u);
    put(rt, byte_object + 4u, 1u);
    put(rt, byte_object + 8u, byte_payload);
    put(rt, byte_payload + 4u, 1u);
    put(rt, byte_payload + 8u, 0u);
    put(rt, byte_payload + 0x0Cu, 0u);
    put_byte(rt, byte_payload + 0x10u, 0u);
    put_byte(rt, byte_payload + 0x11u, 0u);
    put_byte(rt, byte_payload + 0x12u, 0u);
    put(rt, byte_payload + 0x14u, 0u);
    put(rt, byte_payload + 0x18u, 0u);
    put(rt, byte_payload + 0x1Cu, 0u);
    put(rt, byte_payload + 0x20u, 0u);
    put(rt, byte_payload + 0x24u, 0u);
    put(rt, byte_payload + 0x28u, 0u);
    put(rt, byte_payload + 0x2Cu, 0u);
    put(rt, byte_payload + 0x30u, 0u);
    put(rt, byte_payload + 0x34u, 0u);
    put_byte(rt, byte_payload + 0x43u, 0u);
    put(rt, byte_object, 0x08CDB838u);
    call(rt, ctx, 0x08B60910u, {byte_object, 0u, 0u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77494u, byte_object});

    // 0x08D723E4: the same shape with 0x08CDB800 / 0x08D722E8 (vtable 0x08CDBAC8).
    const u32 second_payload = 0x08D722E8u;
    const u32 second_object = 0x08D723E4u;
    put(rt, second_payload, 0x08CDBAC8u);
    put(rt, second_object + 4u, 1u);
    put(rt, second_object + 8u, second_payload);
    put(rt, second_payload + 4u, 1u);
    put(rt, second_payload + 8u, 0u);
    put(rt, second_payload + 0x0Cu, 0u);
    put_byte(rt, second_payload + 0x10u, 0u);
    put_byte(rt, second_payload + 0x11u, 0u);
    put_byte(rt, second_payload + 0x12u, 0u);
    put(rt, second_payload + 0x14u, 0u);
    put(rt, second_payload + 0x18u, 0u);
    put(rt, second_payload + 0x1Cu, 0u);
    put(rt, second_payload + 0x20u, 0u);
    put(rt, second_payload + 0x24u, 0u);
    put(rt, second_payload + 0x28u, 0u);
    put(rt, second_payload + 0x2Cu, 0u);
    put(rt, second_payload + 0x30u, 0u);
    put(rt, second_payload + 0x34u, 0u);
    put_byte(rt, second_payload + 0x43u, 0u);
    put(rt, second_object, 0x08CDB800u);
    call(rt, ctx, 0x08B60C98u, {second_object, 0u, 0u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77490u, second_object});

    // Two more {vtable, flag = 1} objects.
    const u32 plain_c = 0x08D723D0u;
    put(rt, plain_c + 4u, 1u);
    put(rt, plain_c, 0x08CDB900u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D7748Cu, plain_c});
    const u32 plain_d = 0x08D723C8u;
    put(rt, plain_d + 4u, 1u);
    put(rt, plain_d, 0x08CDB918u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77488u, plain_d});

    // 0x08D721DC: {vtable 0x08CDB7E0, flag 1, zeros to +0xC0, a zero byte at +0xC4}, then 0x08C5DC90(0x08D723B4, it, 1).
    const u32 wide_object = 0x08D721DCu;
    const u32 wide_payload = 0x08D723B4u;
    put(rt, wide_object + 4u, 1u);
    put(rt, wide_object, 0x08CDB7E0u);
    for (u32 offset = 8u; offset != 0xC4u; offset += 4u) put(rt, wide_object + offset, 0u);
    put_byte(rt, wide_object + 0xC4u, 0u);
    call(rt, ctx, 0x08C5DC90u, {wide_payload, wide_object, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77478u, wide_payload});

    // Two more {vtable, flag = 1} objects (their vtable and flag word are written here too) and the 0x08C5E298 one.
    const u32 plain_e = 0x08D723ACu;
    put(rt, plain_e + 4u, 1u);
    put(rt, plain_e, 0x08CDB9F8u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77470u, plain_e});
    const u32 plain_f = 0x08D723A4u;
    put(rt, plain_f + 4u, 1u);
    put(rt, plain_f, 0x08CDB9C8u);
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D77474u, plain_f});
    const u32 last_object = 0x08D72394u;
    call(rt, ctx, 0x08C5E298u, {last_object, 1u});
    call(rt, ctx, 0x08B5D3DCu, {self, 0x08D7746Cu, last_object});

    // The four lookups: the index of each key from 0x08B5C2B0 picks the slot in this + 0xC that receives the object.
    const u32 a = call(rt, ctx, 0x08B5C2B0u, {0x08D77484u});
    put(rt, word(rt, self + 0x0Cu) + 4u * a, word_payload);
    const u32 b = call(rt, ctx, 0x08B5C2B0u, {0x08D77494u});
    put(rt, word(rt, self + 0x0Cu) + 4u * b, byte_payload);
    const u32 c = call(rt, ctx, 0x08B5C2B0u, {0x08D77490u});
    put(rt, word(rt, self + 0x0Cu) + 4u * c, second_payload);
    const u32 d = call(rt, ctx, 0x08B5C2B0u, {0x08D77478u});
    const u32 slot = word(rt, self + 0x0Cu) + 4u * d;
    put(rt, slot, wide_object);
    ctx.gpr[3] = wide_object;
    ctx.gpr[2] = slot;
}

// 0x08951944 (a0 = output record, a1 = parameter bytes): decodes the parameter block into the record.
//   +0 / +4 / +8       flag byte and two durations: 0 / 1.0f / 1.0f when byte 3 or 4 is zero or byte 4 is 100,
//                      else 1 and byte 4 / byte 5 over 100.0f (byte 5 zero repeats the byte 4 value).
//   +0xC..+0x14        flag byte = 1 when byte 2 and the halfword at byte 0 are non-zero; byte 0 / 1000.0f and
//                      byte 1 * 0.027777778f in that case, else 0 / 0.
//   +0x18, +0x1C..     boolean "any of bytes 6, 7, 8" and three of them * 0.01f.
//   +0x28..+0x3C       byte 9 as a word, byte 0xC as a word, bytes 0xA/0xB/0xD/0xE * 0.01f.
//   +0x40..+0x4C       byte 0x2E != 0 and bytes 0x2F/0x30/0x31 as floats.
//   +0x50..+0x60       byte 0x33 != 0 && byte 0x32 < byte 0x33; 0x32 and 0x33 * 10 * 0.2777778f; 0x34 and 0x35
//                      * 10 * 0.1f.
//   +0x64..+0x6C       byte 0x3A < byte 0x3B and both * 10 * 0.2777778f.
//   +0x70..+0x9C       two passes of two 12-byte angle blocks: the signed halfwords at 0x3C/0x40/0x44 and
//                      0x48/0x4C/0x50 of a1 + 2 * i, * pi/180, at a0 + 4 * i (stride 4, each pass fills 0x14 bytes).
//   +0xA0..+0xAC       byte 0xD of the table at 0x08CDE230, 15 * its byte 0x1CF * pi/180, and bytes 0x10/0x11
//                      (each != 0) as two booleans.
//   +0xAC..+0x10C      two passes of a fixed 10-field block (bytes 0x14..0x26 * 0.01f, stride 8 and 4); bytes
//                      0x28 and 0x2A of a1 + i: 0 writes 0 / 0 / 0 at +0xFC (byte, stride 1) / +0x100 / +0x108, else 1 at
//                      +0xFC and (below 100: * 0.01f, else 1.0f) at +0x100, and at +0x108 the same for byte 0x2A, the
//                      +0x28 value when byte 0x2A is zero and 1.0f when it is 100 or more.
// v0 is the byte 0x2A of the last pass (0 when that pass's byte 0x28 or 0x2A was zero), v1 the last "below 100" test
// (1 unless that byte 0x2A was 100 or more). No frame and no callee.
void func_08951944(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 in = ctx.gpr[5];
    const float kOne = std::bit_cast<float>(0x3F800000u);              // 1.0f
    const float kHundredth = std::bit_cast<float>(0x3C23D70Au);        // 0.01f, the asm's own rounding
    const float kTenth = std::bit_cast<float>(0x3DCCCCCDu);            // 0.1f
    const float kThirtySixth = std::bit_cast<float>(0x3CE38E3Au);      // 0.027777778f (one ulp over 1/36)
    const float kTenThirtySixths = std::bit_cast<float>(0x3E8E38E4u);  // 0.2777778f
    const float kRadians = std::bit_cast<float>(0x3C8EFA35u);          // 0.017453292f

    bool scaled = false;
    if (rt.memory().load8(in + 3u) != 0u) {
        const u32 fourth = rt.memory().load8(in + 4u);
        if (fourth != 0u && fourth != 100u) {
            const float per_hundred = static_cast<float>(fourth) / 100.0f;
            put_byte(rt, out, 1u);
            put_float(rt, out + 4u, per_hundred);
            const u32 fifth = rt.memory().load8(in + 5u);
            // Byte 5 zero leaves the byte 4 value in $f1: the same number is stored at +8.
            put_float(rt, out + 8u, fifth != 0u ? static_cast<float>(fifth) / 100.0f : per_hundred);
            scaled = true;
        }
    }
    if (!scaled) {
        put_byte(rt, out, 0u);
        put_float(rt, out + 4u, kOne);
        put_float(rt, out + 8u, kOne);
    }

    if (rt.memory().load8(in + 2u) != 0u && rt.memory().load16(in) != 0u) {
        put_byte(rt, out + 0xCu, 1u);
        put_float(rt, out + 0x10u, static_cast<float>(rt.memory().load8(in)) / 1000.0f);
        put_float(rt, out + 0x14u, static_cast<float>(rt.memory().load8(in + 1u)) * kThirtySixth);
    } else {
        put_byte(rt, out + 0xCu, 0u);
        put(rt, out + 0x10u, 0u);
        put(rt, out + 0x14u, 0u);
    }

    put_byte(rt, out + 0x18u, 0u);
    put(rt, out + 0x1Cu, 0u);
    put(rt, out + 0x20u, 0u);
    put(rt, out + 0x24u, 0u);
    for (u32 index = 0u; index < 3u; ++index) {
        const u32 factor = rt.memory().load8(in + 6u + index);
        if (factor == 0u) continue;
        put_byte(rt, out + 0x18u, 1u);
        put_float(rt, out + 0x1Cu + 4u * index, static_cast<float>(factor) * kHundredth);
    }

    put(rt, out + 0x28u, rt.memory().load8(in + 9u));
    put_float(rt, out + 0x2Cu, static_cast<float>(rt.memory().load8(in + 0xAu)) * kHundredth);
    put_float(rt, out + 0x30u, static_cast<float>(rt.memory().load8(in + 0xBu)) * kHundredth);
    put(rt, out + 0x34u, rt.memory().load8(in + 0xCu));
    put_float(rt, out + 0x38u, static_cast<float>(rt.memory().load8(in + 0xDu)) * kHundredth);
    put_float(rt, out + 0x3Cu, static_cast<float>(rt.memory().load8(in + 0xEu)) * kHundredth);
    put_byte(rt, out + 0x40u, rt.memory().load8(in + 0x2Eu) != 0u ? 1u : 0u);
    put_float(rt, out + 0x44u, static_cast<float>(rt.memory().load8(in + 0x2Fu)));
    put_float(rt, out + 0x48u, static_cast<float>(rt.memory().load8(in + 0x30u)));
    put_float(rt, out + 0x4Cu, static_cast<float>(rt.memory().load8(in + 0x31u)));
    put_float(rt, out + 0x5Cu, static_cast<float>(rt.memory().load8(in + 0x34u) * 10u) * kTenth);
    put_float(rt, out + 0x60u, static_cast<float>(rt.memory().load8(in + 0x35u) * 10u) * kTenth);

    const u32 high = rt.memory().load8(in + 0x33u);
    const u32 low = rt.memory().load8(in + 0x32u);
    put_byte(rt, out + 0x50u, high != 0u && low < high ? 1u : 0u);
    put_float(rt, out + 0x54u, static_cast<float>(low * 10u) * kTenThirtySixths);
    put_float(rt, out + 0x58u, static_cast<float>(high * 10u) * kTenThirtySixths);

    // First angle pass: a3 starts at a1 and moves 2 bytes per pass, a2 at a0 and 4 bytes per pass.
    for (u32 index = 0u; index < 2u; ++index) {
        const u32 source = in + 2u * index;
        const u32 target = out + 4u * index;
        put_float(rt, target + 0x70u, static_cast<float>(signed_half(rt, source + 0x3Cu)) * kRadians);
        put_float(rt, target + 0x78u, static_cast<float>(signed_half(rt, source + 0x40u)) * kRadians);
        put_float(rt, target + 0x80u, static_cast<float>(signed_half(rt, source + 0x44u)) * kRadians);
    }

    put_byte(rt, out + 0x64u, 1u);
    const u32 before = rt.memory().load8(in + 0x3Au);
    const u32 after = rt.memory().load8(in + 0x3Bu);
    if (!(before < after)) put_byte(rt, out + 0x64u, 0u);
    put_float(rt, out + 0x68u, static_cast<float>(before * 10u) * kTenThirtySixths);
    put_float(rt, out + 0x6Cu, static_cast<float>(after * 10u) * kTenThirtySixths);

    // Second angle pass: the same shape with the halfwords at 0x48 / 0x4C / 0x50 into 0x88 / 0x90 / 0x98.
    for (u32 index = 0u; index < 2u; ++index) {
        const u32 source = in + 2u * index;
        const u32 target = out + 4u * index;
        put_float(rt, target + 0x88u, static_cast<float>(signed_half(rt, source + 0x48u)) * kRadians);
        put_float(rt, target + 0x90u, static_cast<float>(signed_half(rt, source + 0x4Cu)) * kRadians);
        put_float(rt, target + 0x98u, static_cast<float>(signed_half(rt, source + 0x50u)) * kRadians);
    }

    constexpr u32 kTable = 0x08CDE230u;
    put(rt, out + 0xA0u, rt.memory().load8(kTable + 0xDu));
    put_float(rt, out + 0xA4u, static_cast<float>(rt.memory().load8(kTable + 0x1CFu) * 15u) * kRadians);
    put_byte(rt, out + 0xA8u, rt.memory().load8(in + 0x10u) != 0u ? 1u : 0u);
    put_byte(rt, out + 0xA9u, rt.memory().load8(in + 0x11u) != 0u ? 1u : 0u);

    // The two final passes: t0 (the +0xFC byte) moves 1 byte per pass, a2 (the fields) 4 bytes, so each pass writes the
    // 0xAC..0xF4 block 8 bytes apart and its own flag byte.
    u32 last = 0u;
    u32 last_below_hundred = 1u;
    for (u32 index = 0u; index < 2u; ++index) {
        const u32 source = in + index;
        const u32 target = out + 4u * index;
        float scale = kOne;
        constexpr u32 kSourceOffsets[10] = {0x14u, 0x16u, 0x18u, 0x1Au, 0x1Cu, 0x1Eu, 0x20u, 0x22u, 0x24u, 0x26u};
        for (u32 field = 0u; field < 10u; ++field) {
            put_float(rt, target + 0xACu + 8u * field,
                      static_cast<float>(rt.memory().load8(source + kSourceOffsets[field])) * kHundredth);
        }
        const u32 first = rt.memory().load8(source + 0x28u);
        if (first == 0u) {
            put_byte(rt, out + 0xFCu + index, 0u);
            put(rt, target + 0x100u, 0u);
            put(rt, target + 0x108u, 0u);
            last = 0u;
            last_below_hundred = 1u;
            continue;
        }
        if (first < 100u) scale = static_cast<float>(first) * kHundredth;
        put_byte(rt, out + 0xFCu + index, 1u);
        put_float(rt, target + 0x100u, scale);
        const u32 second = rt.memory().load8(source + 0x2Au);
        if (second == 0u) {
            put_float(rt, target + 0x108u, scale);
            last = 0u;
            last_below_hundred = 1u;
        } else if (second < 100u) {
            put_float(rt, target + 0x108u, static_cast<float>(second) * kHundredth);
            last = second;
            last_below_hundred = 1u;
        } else {
            put_float(rt, target + 0x108u, kOne);
            last = second;
            last_below_hundred = 0u;
        }
    }
    ctx.gpr[2] = last;
    ctx.gpr[3] = last_below_hundred;
}

} // namespace
} // namespace new_batch_main_1602
} // namespace gt::game

#define NEW1602(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1602::name, gt::ReturnKind::kind)
NEW1602(0x0891F110u, func_0891F110, Word);
NEW1602(0x08B5E2CCu, func_08B5E2CC, Word);
NEW1602(0x08951944u, func_08951944, Word);
