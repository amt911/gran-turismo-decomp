// New batch main-126 (draft, unverified): functions emitted by the recompiler that no src/ file defines or registers.
// Written by hand from `tools/disasm` as structured C. Helpers follow new_batch_main_12.cpp: a guest call that leaves
// v0/v1/f0/a1 as the callee left them, the compiler's virtual call, and the "null object" check that calls 0x08A5BB7C
// (and goes on, as the original does when it returns).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_126 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 and a1 as the callee left them (a 64-bit result, a stale a1 for a 64-bit argument pair).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The compiler's virtual call: the callee is vtable[offset] of `object`; the object is a0.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    return call_keep(rt, ctx, function, arguments);
}
// `if (!object) 0x08A5BB7C(message)` — the call returns in the original, so the code goes on.
void require(Runtime &rt, AllegrexContext &ctx, u32 object, u32 message) {
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
}
// The sret tail: when the destination is not the temporary, move the value there, then release the temporary; v0 = the release's result.
void release_result(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 temporary) {
    if (destination != temporary) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, temporary)});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {temporary});
}

// 0x0898BDB4 (a0 = destination, a2 = mode, a3 = pointer to an object): mode 0 builds a result from the byte at +0x128 / word at
// +0x12C of the record 0x089896CC returns (-1 when the byte is 0); mode 1 asks the object a3 points to (virtual 0x3C) and stores
// the answer into that record (byte 1 and the value, or byte 0 when negative); other modes return v0 = 1.
void func_0898BDB4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 destination = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    const u32 record_slot = frame.at(8);
    if (mode == 0u) {
        call(rt, ctx, 0x089896CCu, {record_slot});
        u32 record = word(rt, record_slot);
        require(rt, ctx, record, 0x08CA39F0u);
        const u32 enabled = rt.memory().load8(record + 0x128u);
        u32 result_slot;
        if (enabled != 0u) {
            record = word(rt, record_slot);
            require(rt, ctx, record, 0x08CA39F0u);
            const u32 value = word(rt, record + 0x12Cu);
            result_slot = frame.at(4);
            call(rt, ctx, 0x08A85B1Cu, {result_slot, value});
        } else {
            result_slot = frame.at(0);
            call(rt, ctx, 0x08A85B1Cu, {result_slot, 0xFFFFFFFFu});
        }
        if (destination != result_slot) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, result_slot)});
        call(rt, ctx, 0x08A5B9ECu, {result_slot});
        call(rt, ctx, 0x08A5B9ECu, {record_slot});
    } else if (mode == 1u) {
        call(rt, ctx, 0x089896CCu, {record_slot});
        const u32 object = word(rt, holder);
        require(rt, ctx, object, 0x08CA39E8u);
        const s32 answer = static_cast<s32>(call_virtual(rt, ctx, object, 0x3Cu, {object}).v0);
        u32 record = word(rt, record_slot);
        if (answer >= 0) {
            require(rt, ctx, record, 0x08CA39F0u);
            rt.memory().store8(record + 0x128u, static_cast<std::uint8_t>(mode));
            record = word(rt, record_slot);
            require(rt, ctx, record, 0x08CA39F0u);
            put(rt, record + 0x12Cu, static_cast<u32>(answer));
        } else {
            record = word(rt, record_slot);
            require(rt, ctx, record, 0x08CA39F0u);
            rt.memory().store8(record + 0x128u, 0u);
        }
        call(rt, ctx, 0x08A5B9ECu, {record_slot});
    } else {
        ctx.gpr[2] = 1u;
    }
}

// 0x08A7210C (a0 = destination, a1 = name, a2 = mode, a3 = pair of objects): mode 3 clamps the length of one object (+4's virtual
// 0x3C) plus the other's (+8) against the size (virtual 0xD8) of the one at sp+8 built by 0x08A76ABC, and hands the slice (virtual
// 0xDC at the start) to the builder 0x08A71E70 makes; the result (0 when the start is past the end) goes into the destination.
void func_08A7210C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 destination = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 pair = ctx.gpr[7];
    if (mode != 3u) {
        ctx.gpr[2] = 3u;
        return;
    }
    const u32 source_slot = frame.at(8);
    call(rt, ctx, 0x08A76ABCu, {source_slot, pair});
    u32 first = word(rt, pair + 4u);
    require(rt, ctx, first, 0x08CB0A60u);
    const u32 start = call_virtual(rt, ctx, first, 0x3Cu, {first}).v0;
    u32 second = word(rt, pair + 8u);
    require(rt, ctx, second, 0x08CB0A60u);
    u32 length = call_virtual(rt, ctx, second, 0x3Cu, {second}).v0;
    u32 source = word(rt, source_slot);
    require(rt, ctx, source, 0x08CB0A60u);
    u32 size = call_virtual(rt, ctx, source, 0xD8u, {source}).v0;
    u32 value = 0u;
    if (start < size) {
        source = word(rt, source_slot);
        require(rt, ctx, source, 0x08CB0A60u);
        size = call_virtual(rt, ctx, source, 0xD8u, {source}).v0;
        if (size < start + length) {
            source = word(rt, source_slot);
            require(rt, ctx, source, 0x08CB0A60u);
            length = call_virtual(rt, ctx, source, 0xD8u, {source}).v0 - start;
        }
        const u32 builder_slot = frame.at(4);
        call(rt, ctx, 0x08A71E70u, {builder_slot, name});
        const u32 builder = word(rt, builder_slot);
        require(rt, ctx, builder, 0x08CB0A94u);
        const u32 build = word(rt, word(rt, builder) + 0x10Cu);
        source = word(rt, source_slot);
        require(rt, ctx, source, 0x08CB0A60u);
        const u32 piece = call_virtual(rt, ctx, source, 0xDCu, {source, start}).v0;
        value = call(rt, ctx, build, {builder, piece, length});
        call(rt, ctx, 0x08A5B9ECu, {builder_slot});
    }
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0), value});
    release_result(rt, ctx, destination, frame.at(0));
    call(rt, ctx, 0x08A5B9ECu, {source_slot});
}

// trunc.w.s: round toward zero, saturating like the FPU.
u32 trunc_word(float value) {
    if (!(value < 2147483648.0f)) return 0x7FFFFFFFu;
    if (value < -2147483648.0f) return 0x80000000u;
    return static_cast<u32>(static_cast<s32>(value));
}

// 0x088DEF1C (a0 = object): walks the object's 0x2F8-byte slots (array at +0xC0, count 0x088D9F80); a ready slot is advanced by
// 0x088DE24C / 0x088DEDE0 / 0x088DE3F0 by its state word (+0x2C8) and reset on success; an idle one (when allowed) is started
// between 0x08B344E0 / 0x08B3457C; ends by a tail call of 0x088DD8E0(object).
void func_088DEF1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x30u);
        const u32 ready = call(rt, ctx, 0x088D4C9Cu, {self + 4u});
        u32 may_start = 0u;
        if (ready != 0u) may_start = rt.memory().load8(self + 0x5BCu) == 0u ? 1u : 0u;
        const u32 timer = self + 0xC8u;
        s32 index = 0;
        u32 offset = 0u;
        for (;;) {
            const s32 count = static_cast<s32>(call(rt, ctx, 0x088D9F80u, {self}));
            if (!(index < count)) break;
            const u32 slot = word(rt, self + 0xC0u) + offset;
            const u32 time_low = word(rt, self + 0x90u);
            const u32 time_high = word(rt, self + 0x94u);
            const u32 idle = word(rt, slot + 0x1Cu) == 0u ? 1u : 0u;
            const GuestResult active = call_keep(rt, ctx, 0x088E88C4u, {slot, ctx.gpr[5], time_low, time_high});
            bool reset = false;
            if (active.v0 != 0u) {
                const u32 state = word(rt, slot + 0x2C8u);
                if (state == 1u) {
                    if (call(rt, ctx, 0x088DE24Cu, {self, 0u, 0u}) != 0u) {
                        u32 handler = word(rt, self + 0x5CCu);
                        if (handler != 0u) call_virtual(rt, ctx, handler, 0x18u, {handler, static_cast<u32>(index)});
                        const u32 done = call(rt, ctx, 0x088E85E8u, {slot});
                        u32 display = word(rt, self + 0x148u);
                        if (done != 0u) {
                            call_virtual(rt, ctx, display, 0x38u, {display, static_cast<u32>(index)});
                            display = word(rt, self + 0x148u);
                        }
                        call_virtual(rt, ctx, display, 0x28u, {display, static_cast<u32>(index)});
                        const u32 sound = word(rt, self + 0x14Cu);
                        call_virtual(rt, ctx, sound, 0xCu, {sound, static_cast<u32>(index)});
                        call(rt, ctx, 0x088E82ACu, {slot});
                        call(rt, ctx, 0x088D9BBCu, {self, 0u, 0u});
                        reset = true;
                    }
                } else {
                    const u32 advanced = state == 3u ? call(rt, ctx, 0x088DEDE0u, {self, static_cast<u32>(index), idle})
                                                     : call(rt, ctx, 0x088DE3F0u, {self, static_cast<u32>(index)});
                    reset = advanced != 0u;
                }
                if (reset) {
                    ctx.gpr[2] = 0u;
                    ctx.gpr[3] = 0u;
                    put(rt, slot + 0x2D0u, 0u);
                    put(rt, slot + 0x2D4u, 0u);
                    rt.memory().store8(slot + 0x2B4u, 1u);
                }
            } else if (may_start != 0u && call(rt, ctx, 0x088D9E58u, {self}) == 0u && word(rt, slot + 4u) != 3u) {
                call(rt, ctx, 0x08B344E0u, {timer});
                if (rt.memory().load8(slot + 0x2B4u) != 0u) {
                    put(rt, slot + 0x2C8u, 3u);
                    const u32 low = word(rt, self + 0x90u);
                    const u32 high = word(rt, self + 0x94u);
                    const GuestResult started = call_keep(rt, ctx, 0x088D9C24u, {self, ctx.gpr[5], low, high});
                    put(rt, slot + 0x2D0u, started.v0);
                    put(rt, slot + 0x2D4u, started.v1);
                    rt.memory().store8(slot + 0x2B4u, 0u);
                }
                call(rt, ctx, 0x08B3457Cu, {timer});
            }
            ++index;
            offset += 0x2F8u;
        }
    }
    tail_call(ctx, 0x088DD8E0u, {self});
}

// 0x0886C92C (a0 = destination of four floats, a1 = text "a b,c d,..."): zeroes the destination, then for up to four comma-separated
// fields strips the blanks, converts the text to a double (0x08B77BF4) and stores it as a float; releases the std::string reps;
// v0 = the destination.
void release_rep(Runtime &rt, AllegrexContext &ctx, u32 data, u32 empty_rep, u32 allocator) {
    const u32 rep = data - 0xCu;
    if (rep == empty_rep) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, 0x08B93554u, {rep, allocator});
}
void func_0886C92C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 destination = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    put(rt, destination, 0u);
    put(rt, destination + 4u, 0u);
    put(rt, destination + 8u, 0u);
    put(rt, destination + 12u, 0u);
    if (text == 0u || static_cast<std::int8_t>(rt.memory().load8(text)) == 0) {
        ctx.gpr[2] = destination;
        return;
    }
    const u32 whole = frame.at(4);
    const u32 field = frame.at(8);
    call(rt, ctx, 0x08B93CD8u, {whole, text, frame.at(2)});
    u32 start = 0u;
    u32 fields = 0u;
    u32 out = destination;
    u32 empty_rep;
    for (;;) {
        const u32 comma = call(rt, ctx, 0x08B93BCCu, {whole, 0x2Cu, start});
        const u32 span = comma - start;
        if (word(rt, word(rt, whole) - 0xCu) < start) call(rt, ctx, 0x08B5AEDCu, {0x08C9F728u});
        const u32 count = comma != 0xFFFFFFFFu ? span : 0xFFFFFFFFu;
        call(rt, ctx, 0x08B9479Cu, {field, whole, start, count});
        start = 0xFFFFFFFFu;
        u32 space;
        while ((space = call(rt, ctx, 0x08B93BCCu, {field, 0x20u, 0u})) != start) {
            const u32 length = word(rt, word(rt, field) - 0xCu);
            u32 rest = length - space;
            if (length < space) rest = call(rt, ctx, 0x08B5AEDCu, {0x08C9F740u});
            call(rt, ctx, 0x08B943A0u, {field, space, rest < 2u ? rest : 1u, 0u});
        }
        const GuestResult number = call_keep(rt, ctx, 0x08B77BF4u, {word(rt, field)});
        const GuestResult single = call_keep(rt, ctx, 0x08B71EC8u, {number.v0, number.v1});
        put_float(rt, out, single.f0);
        if (comma == space) {
            empty_rep = 0x08D75298u;
            release_rep(rt, ctx, word(rt, field), empty_rep, frame.at(1));
            break;
        }
        empty_rep = word(rt, 0x08CB6050u);
        start = comma + 1u;
        release_rep(rt, ctx, word(rt, field), empty_rep, frame.at(1));
        ++fields;
        out += 4u;
        if (fields == 4u) break;
    }
    release_rep(rt, ctx, word(rt, whole), empty_rep, frame.at(0));
    ctx.gpr[2] = destination;
}

// 0x0888D5E4 (a0 = object, a1 = view, a2 = u16 ids, a3 = count): for each id whose record 0x088DA788 is of kind 1 and whose
// positions (the object's virtual 0xA0 samples, through 0x088D2C8C) all lie in [f21, f22] (the view's two 0x088D6218 numbers),
// takes the distance of the virtual 0x24 vector (scaled by 50 / 0.2777778), and hands it on to 0x0888C228 / 0x0888D088.
void func_0888D5E4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 self = ctx.gpr[4];
    const u32 view = ctx.gpr[5];
    const u32 ids = ctx.gpr[6];
    const s32 count = static_cast<s32>(ctx.gpr[7]);
    const u32 vector = frame.at(0);
    const u32 flag = frame.at(0x10);
    for (s32 i = 0; i < count; ++i) {
        const u32 id = rt.memory().load16(ids + 2u * static_cast<u32>(i));
        const u32 record = call(rt, ctx, 0x088DA788u, {word(rt, self + 4u), id});
        const float f20 = float_at(rt, record);
        const u32 kind_source = call(rt, ctx, 0x0889414Cu, {word(rt, self + 0xCu)});
        if (word(rt, kind_source) != 1u) continue;
        const float low = static_cast<float>(static_cast<s32>(call(rt, ctx, 0x088D6218u, {view, 0u})));
        const float high = static_cast<float>(static_cast<s32>(call(rt, ctx, 0x088D6218u, {view, 1u})));
        u32 object = call(rt, ctx, 0x088DCD28u, {word(rt, self + 4u), id});
        const s32 samples = static_cast<s32>(call_virtual(rt, ctx, object, 0x8Cu, {object}).v0);
        bool inside = true;
        if (samples > 0) {
            put(rt, flag, 1u);
            for (s32 k = 0; k < samples; ++k) {
                object = call(rt, ctx, 0x088DCD28u, {word(rt, self + 4u), id});
                call_virtual(rt, ctx, object, 0xA0u, {object, vector, static_cast<u32>(k)});
                const u32 table = word(rt, word(rt, self + 4u) + 0x61Cu);
                const float measured = call_keep(rt, ctx, 0x088D2C8Cu, {table, vector}, {f20}).f0;
                if (measured < low) put(rt, flag, 0u);
                else if (high < measured) put(rt, flag, 0u);
            }
            inside = word(rt, flag) != 0u;
        }
        if (!inside) continue;
        object = call(rt, ctx, 0x088DCD28u, {word(rt, self + 4u), id});
        call_virtual(rt, ctx, object, 0x24u, {vector, object});
        const float x = float_at(rt, vector);
        const float y = float_at(rt, vector + 4u);
        const float z = float_at(rt, vector + 8u);
        const float xx = x * x;
        const float yy = y * y;
        const float zz = z * z;
        const float sum_xy = xx + yy;
        const float sum = sum_xy + zz;
        const float root = std::sqrt(sum);
        const float scaled = root / std::bit_cast<float>(0x3E8E38E4u);
        const float scaled_up = scaled * 50.0f;
        const u32 stretch = trunc_word(scaled_up);
        const u32 amount = word(rt, record + 0x74u) + stretch;
        const u32 extra = call(rt, ctx, 0x088DB2C8u, {word(rt, self + 4u), id});
        call(rt, ctx, 0x0888C228u, {self, record, amount, extra});
        const u32 owner = word(rt, word(rt, self + 4u) + 0x618u);
        rt.memory().store8(owner, 0u);
        call(rt, ctx, 0x0888D088u, {self, view, id, amount, 0u});
    }
}

} // namespace
} // namespace new_batch_main_126
} // namespace gt::game

#define NEW126(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_126::name, gt::ReturnKind::kind)
NEW126(0x0898BDB4u, func_0898BDB4, Word);
NEW126(0x08A7210Cu, func_08A7210C, Word);
NEW126(0x088DEF1Cu, func_088DEF1C, Word);
NEW126(0x0886C92Cu, func_0886C92C, Word);
NEW126(0x0888D5E4u, func_0888D5E4, Word);
