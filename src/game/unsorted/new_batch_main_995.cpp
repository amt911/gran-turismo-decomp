// New batch main 995 (draft, unverified): functions earlier drafts mismatched or skipped, hand-written as structured C from
// `tools.disasm`. One function per address; the input shapes for the synthetic generator are in
// /home/andres/repos/gt-ops/shapes/shapes_main995.txt.
//
// The shapes that repeat here: the libstdc++-style value-object helpers (0x08A5BAD8 assigns a holder, 0x08A5B9EC destroys
// one, 0x08A5BB7C is the null assert), and the "virtual call through the vtable word at +0" step (call_method).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_995 {
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
// Keeps v1 / f0 as the callee left them: after a `jal` they are the callee's, and the original reads them.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                        std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v1, 0(a0); lw v0, slot(v1); jalr v0`: the callee starts with v0 = its own address.
u32 call_method(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments = {}) {
    return call_indirect(rt, ctx, word(rt, word(rt, object) + slot), arguments);
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }

// The original's epilogue reloads s0-s7 and fp from its frame: put them back as it does.
struct CalleeSaved {
    AllegrexContext &ctx;
    u32 saved[8];
    u32 saved_fp;
    explicit CalleeSaved(AllegrexContext &c) : ctx(c), saved_fp(c.gpr[30]) {
        for (u32 i = 0; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~CalleeSaved() {
        for (u32 i = 0; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
        ctx.gpr[30] = saved_fp;
    }
};

// ---- 0x088CA2BC ---------------------------------------------------------------------------------------------------
// a0 = object, a1 = mode, a2/a3 = the first 64-bit pair, t0 (a4) = a three-float output vector, t1 (a5) = a direction
// output, f12/f13 = two floats. The record at sp (0x089545BC) carries the query; the record at sp+0x38 (fp) is the query:
// +0 is 3, +4 is 2 when the mode is 4 else 0. The object at self+0xC answers slot 0x18(object, mode); a zero answer leaves
// 6 in the query's +8, else its slot 0x24(object, mode) fills it. Slot 0x1C(object) answers the object whose slot
// 0x8(object, query) fills the query; its +0x18/+0x1C words and the floats +0x28 (s6), +0x2C (negated, s5) and +0x30
// (s7) are lifted out, t0+0 / t0+8 get f21 and -f20 and the record gets +0 = f21, +4 = f12, +8 = -f20, +0xC = 1.
// The record's +0x30/+0x34 pair is then tried up to three times: (a2, a3), then the query's +0x20/+0x24, then the words
// saved at sp+0x90/+0x94; after each try the object at self+0x14's slot 0x18(record) may clear the record's +0x2C byte.
// Cleared: out_vector+4 = record+0xC and, when t1 is given, the record's +0x18/+0x1C/+0x20 are normalised into t1
// (0x08B4A744 length, 0x08B4A6A0 scale with 1/length, f0 = the first component). Not cleared after the third try:
// out_vector+4 = f23, t1 gets (0, 1, 0) and f0 = 1. v0/v1 end as the record's +0x30/+0x34 pair, or a2/a3 on that path.

// ---- 0x088A322C ---------------------------------------------------------------------------------------------------
// a0 = object: teardown. +0x1C is cleared; the objects at +0x54 (after 0x0894CE2C), +0x3C and +0x44 (with byte +0x40)
// are released through their virtual +4 and cleared; the one at +0x58's second word (+0x5C) through its virtual +0, then
// +0x58 is cleared and +0x5C gets the shared object 0x08D75F20 (built on first use behind the guard byte 0x08D75F18:
// vtable 0x08CC28B0, 0x08B6441C / 0x08B64494); +0x50 gets its virtual +0xC then +4 (and +0x24 .. +0x30 cleared); the
// blocks at +0x10, +0xC, +0x20 are freed (0x08B61FC0), the array of 0x430-byte objects at +8 destroyed from the end
// (virtual +0) and its block header freed (begin - 0x10), then +0x4C, and the array of 0x14-byte records at +0x60
// (0x0892C0B8 each) freed with its header (begin - 4), with +0x14 cleared.

// ---- 0x089B2014 / 0x089B2324 --------------------------------------------------------------------------------------
// a0 = a vector-like container, a1 = an argument, a2 = the element count, a3 = the source object slot. Nothing happens
// while the count is not positive. The object at *a3 answers slot 0x78: non-zero, its slot 0x7C is the count and each
// element comes from its slot 0x84(holder, object, index) copied out of 0x089A354C; zero, the count stays a2 and the
// elements are copied from the a3 array (4 bytes apart). The storage grows through 0x08BF9B6C (enough room) or
// 0x08BFB68C (insert; flag word at sp+8 / sp+4). Then 0x089AB370(sp+0xC, a1) answers the container object, whose null
// check is 0x08A5BB7C(0x08CA50C4); 0x089B1FCC(container, &vector) fills it, the temporary is destroyed, the range is
// destroyed (0x08BF8B48) and the storage freed (0x08A5BC54(begin, (end - begin) & ~3, 4)) when there is one.
void push_page_body(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    CalleeSaved saved(ctx);
    const u32 argument = ctx.gpr[5];
    const u32 source = ctx.gpr[7];
    u32 count = ctx.gpr[6];
    if (static_cast<s32>(count) <= 0) return;
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    put(rt, frame.at(0x24u), 0u);
    u32 object = word(rt, source);
    if (object == 0u) {
        call(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
        object = word(rt, source);
    }
    if (call_method(rt, ctx, object, 0x78u, {object}) != 0u) {
        object = word(rt, source);
        if (object == 0u) {
            call(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
            object = word(rt, source);
        }
        count = call_method(rt, ctx, object, 0x7Cu, {object});
        const u32 end = word(rt, frame.at(0x20u));
        const u32 begin = word(rt, frame.at(0x1Cu));
        const u32 capacity = (end - begin) >> 2;
        if (count < capacity) {
            call(rt, ctx, 0x08BF9B6Cu, {frame.at(0x1Cu), begin + count * 4u});
        } else {
            call(rt, ctx, 0x08BFB68Cu, {frame.at(0x1Cu), end, count - capacity, frame.at(0x08u)});
        }
        for (u32 index = 0u; count != 0u;) {
            const u32 element = word(rt, source);
            const u32 slot = word(rt, frame.at(0x1Cu)) + index * 4u;
            if (element == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
            call_method(rt, ctx, element, 0x84u, {frame.at(0x14u), element, index});
            call(rt, ctx, 0x089A354Cu, {frame.at(0x18u), frame.at(0x14u)});
            put(rt, slot, word(rt, frame.at(0x18u)));
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
            ++index;
            if (count == index) break;
        }
    } else {
        const u32 end = word(rt, frame.at(0x20u));
        const u32 begin = word(rt, frame.at(0x1Cu));
        const u32 capacity = (end - begin) >> 2;
        if (count < capacity) {
            call(rt, ctx, 0x08BF9B6Cu, {frame.at(0x1Cu), begin + count * 4u});
        } else {
            call(rt, ctx, 0x08BFB68Cu, {frame.at(0x1Cu), end, count - capacity, frame.at(0x04u)});
        }
        u32 cursor = source;
        for (u32 index = 0u; count != 0u;) {
            const u32 slot = word(rt, frame.at(0x1Cu)) + index * 4u;
            call(rt, ctx, 0x089A354Cu, {frame.at(0x10u), cursor});
            put(rt, slot, word(rt, frame.at(0x10u)));
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
            ++index;
            cursor += 4u;
            if (count == index) break;
        }
    }
    call(rt, ctx, 0x089AB370u, {frame.at(0x0Cu), argument});
    const u32 container = word(rt, frame.at(0x0Cu));
    if (container == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    call(rt, ctx, 0x089B1FCCu, {container, frame.at(0x1Cu)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x0Cu)});
    const u32 begin = word(rt, frame.at(0x1Cu));
    call(rt, ctx, 0x08BF8B48u, {begin, word(rt, frame.at(0x20u)), frame.at(0u)});
    const s32 bytes = static_cast<s32>(word(rt, frame.at(0x24u)) - begin) >> 2;
    if (begin == 0u) {
        ctx.gpr[2] = static_cast<u32>(bytes);
    } else {
        call(rt, ctx, 0x08A5BC54u, {begin, static_cast<u32>(bytes) << 2, 4u});
    }
}
void func_089B2014(Runtime &rt, AllegrexContext &ctx) { push_page_body(rt, ctx); }

// ---- 0x0896E0EC ---------------------------------------------------------------------------------------------------
// a0 = the four-element output holder, a1 = a tag, a2 = mode, a3 = a source array. 0x08969244 first builds the object at
// sp+0x10. Mode 0: 0x0896B260(sp+0x18, object) and 0x08A98C1C(sp+0x14, 4) build a four-value list; each of its elements
// (list->+0x14 + 4k) takes 0x08A87EEC(holder, float at sp+0x18+4k) assigned over through 0x08A5BAD8 and released; out
// gets the list through 0x08A5BAD8 unless out is the list itself; both temporaries are destroyed. Mode 1:
// 0x08A98674(sp+0x14, source) builds the list, the four elements' virtual +0x54 answers land in the floats at
// sp+0x18..0x24, and 0x0896B324(object, sp+0x18) consumes them (the null check is 0x08A5BB7C(0x08CA2880)); out is not
// written. Any other mode only destroys the sp+0x10 object. v0 is the last destroy's.

// ---- 0x088C8330 ---------------------------------------------------------------------------------------------------
// a0 = a scene object, a1 = an argument object. Returns 0 (v0) while a0+4 or the byte at a0+8 is zero, or when a0+0 is
// zero; a negative signed a0+0xC is returned as is. Otherwise it sets up the graphics state (the 0x08B5711C / 0x08B58874 /
// 0x08B5877C / 0x08B54FB8 / 0x08B58A74 / 0x08B58AD0 / 0x08B55AAC / 0x08B55B90 run with the original's constants), calls
// 0x08AF6CE0(entries + index x 0x98, 0, entries, table) with index = a0+0xC, table = word(a0) and
// entries = word(table+0x18), then — once, with the float at a0+0xC4 ahead of it — 0x088C7948(a0, a0+0x44, 1) and
// 0x088C7948(a0, a0+0x84, 0); 0x08B584D0(0x400, 0x302, 0x303); and, for two non-negative floats at a0+0xC4 / a0+0x148,
// 0x08B54FB8(2) and 0x088C7D50(a0, a1, 0.3). Ends in the tail jump to 0x08AFBEF4.
void func_088C8330(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        CalleeSaved saved(ctx);
        if (word(rt, self + 0x04u) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (rt.memory().load8(self + 0x08u) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (word(rt, self) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 index = word(rt, self + 0x0Cu);
        if (static_cast<s32>(index) < 0) {
            ctx.gpr[2] = index;
            return;
        }
        call(rt, ctx, 0x08B5711Cu, {0x481u});
        call(rt, ctx, 0x08B58874u, {0x6Fu});
        call_result(rt, ctx, 0x08B5877Cu, {6u}, {0.05f});
        call(rt, ctx, 0x08B54FB8u, {0x6Au});
        call(rt, ctx, 0x08B54FB8u, {0x64u});
        call(rt, ctx, 0x08B54FB8u, {0x78u});
        call(rt, ctx, 0x08B58874u, {0x8002u});
        call(rt, ctx, 0x08B58874u, {0x70u});
        call(rt, ctx, 0x08B54FB8u, {0x6Cu});
        call(rt, ctx, 0x08B58A74u, {0u});
        call(rt, ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
        call(rt, ctx, 0x08B54FB8u, {0x6Du});
        call(rt, ctx, 0x08B55AACu, {0u, 1u});
        call(rt, ctx, 0x08B55AACu, {2u, 0x8000u});
        call(rt, ctx, 0x08B55AACu, {1u, 0u});
        call(rt, ctx, 0x08B55AACu, {5u, 1u});
        call(rt, ctx, 0x08B55AACu, {6u, 1u});
        call(rt, ctx, 0x08B55AACu, {4u, 1u});
        call(rt, ctx, 0x08B55AACu, {3u, 1u});
        call(rt, ctx, 0x08B55B90u);
        call(rt, ctx, 0x08B584D0u, {0x400u, 0x302u, 0x303u});
        call(rt, ctx, 0x08B58874u, {2u});
        const u32 table = word(rt, self);
        const u32 entries = word(rt, table + 0x18u);
        const u32 entry = (((index << 2) + (index << 4)) - index) << 3;
        call(rt, ctx, 0x08AF6CE0u, {entries + entry, 0u, entries, table});
        float value = float_at(rt, self + 0xC4u);
        if (0.0f <= value) {  // the body of the counter loop (four steps) runs once, on its first step
            call(rt, ctx, 0x088C7948u, {self, self + 0x44u, 1u});
            call(rt, ctx, 0x088C7948u, {self, self + 0x84u, 0u});
            value = float_at(rt, self + 0xC4u);
        }
        if (0.0f <= value && float_at(rt, self + 0x148u) < 0.0f) {
            call(rt, ctx, 0x08B54FB8u, {2u});
            call(rt, ctx, 0x088C8130u, {self, argument});
        }
        call(rt, ctx, 0x08B584D0u, {0x400u, 0x302u, 0x303u});
        if (0.0f <= float_at(rt, self + 0xC4u) && 0.0f <= float_at(rt, self + 0x148u)) {
            call(rt, ctx, 0x08B54FB8u, {2u});
            call_result(rt, ctx, 0x088C7D50u, {self, argument}, {0.3f});
        }
        call(rt, ctx, 0x08B54FB8u, {0x8002u});
        call(rt, ctx, 0x08B58A74u, {1u});
        call(rt, ctx, 0x08B58874u, {0x6Au});
    }
    tail_call(ctx, 0x08AFBEF4u, {});
}

// ---- 0x08AF77DC ---------------------------------------------------------------------------------------------------
// a0 = a state block: mode at +1 (0, 1 or 2), entry limit at +4, the 8-byte-entry array at +8 { float value; ... },
// cursor at +0x0C, lookahead at +0x0E, the float accumulator at +0x10, the counter at +0x14, a signed byte at +0x16 and
// the "finished" byte at +0x17. Nothing to do while the counter's signed half is positive (v0 = it). Otherwise the
// accumulator takes $f12 and entries are consumed while |value| <= accumulator; the mode picks how the two indices
// advance at limit edges (the direction byte at +0x16 is +1 / -1), and the counter grows by one for a negative entry
// when the previous total was non-negative (which also resets the accumulator once it is positive). When an entry no
// longer fits, the second loop walks back until the accumulator turns non-negative; v0 = 1 and +0x17 = 1 then.
void func_08AF77DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float step = ctx.fpr[12];
    const s32 counter = signed_half(rt, self + 0x14u);
    if (counter > 0) {
        ctx.gpr[2] = static_cast<u32>(counter);
        return;
    }
    const u32 table = word(rt, self + 0x08u);
    u32 index = rt.memory().load16(self + 0x0Cu);
    u16 timer = rt.memory().load16(self + 0x14u);
    float accumulated = float_at(rt, self + 0x10u);
    float previous = accumulated;
    accumulated += step;
    put_float(rt, self + 0x10u, accumulated);
    const u32 minus_one = 0xFFFFFFFFu;
    const auto entry = [&](u32 at) { return float_at(rt, table + (at & 0xFFFFu) * 8u); };
    const auto store_direction = [&](u32 value) { rt.memory().store8(self + 0x16u, static_cast<std::uint8_t>(value)); };
    for (;;) {
        // Consume entries while they fit.
        for (;;) {
            const float magnitude = std::fabs(entry(index));
            if (!(magnitude <= accumulated)) break;
            accumulated -= magnitude;
            const u32 lookahead = rt.memory().load16(self + 0x0Eu);
            const u32 mode = rt.memory().load8(self + 0x01u);
            put_float(rt, self + 0x10u, accumulated);
            index = lookahead;
            rt.memory().store16(self + 0x0Cu, static_cast<u16>(lookahead));
            const u32 limit = rt.memory().load16(self + 0x04u);
            if (mode == 1u) {
                if (lookahead == 0u) rt.memory().store8(self + 0x16u, 1u);
                else if (lookahead == limit - 1u) store_direction(minus_one);
                const u32 next = rt.memory().load16(self + 0x0Cu) + static_cast<u32>(signed_byte(rt, self + 0x16u));
                rt.memory().store16(self + 0x0Eu, static_cast<u16>(next));
            } else if (mode == 0u) {
                rt.memory().store16(self + 0x0Eu, static_cast<u16>(lookahead == limit - 1u ? 0u : lookahead + 1u));
            } else if (mode == 2u) {
                if (lookahead == limit - 1u) put_float(rt, self + 0x10u, 0.0f);
                else rt.memory().store16(self + 0x0Eu, static_cast<u16>(lookahead + 1u));
            } else {
                // No mode: a negative entry at the lookahead steps the counter (the previous total decides), and a
                // positive counter drops the accumulator.
                if (entry(index) < 0.0f && 0.0f <= previous) {
                    timer = static_cast<u16>(timer + 1u);
                    rt.memory().store16(self + 0x14u, timer);
                }
                if (signed_half(rt, self + 0x14u) > 0) {
                    accumulated = 0.0f;
                    put_float(rt, self + 0x10u, 0.0f);
                } else {
                    accumulated = float_at(rt, self + 0x10u);
                }
                continue;
            }
            accumulated += std::fabs(entry(index));
            put_float(rt, self + 0x10u, accumulated);
            previous = accumulated;
            if (!(accumulated < 0.0f)) {
                rt.memory().store8(self + 0x17u, 1u);
                ctx.gpr[2] = 1u;
                return;
            }
            break;
        }
        if (!(accumulated < 0.0f)) {
            rt.memory().store8(self + 0x17u, 1u);
            ctx.gpr[2] = 1u;
            return;
        }
        // Walk back until the running total is non-negative.
        for (;;) {
            if (entry(index) < 0.0f && 0.0f <= previous) {
                timer = static_cast<u16>(timer + 1u);
                rt.memory().store16(self + 0x14u, timer);
            }
            if (signed_half(rt, self + 0x14u) > 0) {
                put_float(rt, self + 0x10u, 0.0f);
                rt.memory().store8(self + 0x17u, 1u);
                ctx.gpr[2] = 1u;
                return;
            }
            rt.memory().store16(self + 0x0Eu, static_cast<u16>(index));
            const u32 mode = rt.memory().load8(self + 0x01u);
            const u32 limit = rt.memory().load16(self + 0x04u);
            if (mode == 1u) {
                const u32 cursor = index & 0xFFFFu;
                if (cursor == 0u) store_direction(minus_one);
                else if (cursor == limit - 1u) rt.memory().store8(self + 0x16u, 1u);
                index -= static_cast<u32>(signed_byte(rt, self + 0x16u));
                rt.memory().store16(self + 0x0Cu, static_cast<u16>(index));
            } else if (mode == 0u) {
                index = index == 0u ? limit - 1u : index - 1u;
                rt.memory().store16(self + 0x0Cu, static_cast<u16>(index));
            } else if (mode == 2u) {
                if (index == 0u) {
                    rt.memory().store16(self + 0x0Eu, static_cast<u16>(limit < 2u ? 0u : 1u));
                    put_float(rt, self + 0x10u, 0.0f);
                    accumulated = 0.0f;
                } else {
                    index -= 1u;
                    rt.memory().store16(self + 0x0Cu, static_cast<u16>(index));
                }
            }
            accumulated += std::fabs(entry(index));
            put_float(rt, self + 0x10u, accumulated);
            previous = accumulated;
            if (!(accumulated < 0.0f)) {
                rt.memory().store8(self + 0x17u, 1u);
                ctx.gpr[2] = 1u;
                return;
            }
        }
    }
}

// ---- 0x0887D6FC ---------------------------------------------------------------------------------------------------
// a0 = object. First, for each of the entries of the collection at a0+0x60, 0x0886BC5C(collection, index) answers an
// entry and 0x0886A9E0(entry, a0+0x50) consumes it. Then the seven words of 0x08C8984C are copied to the frame and the
// object at a0+0x50 is walked: while the index is below its slot 0x0C answer, slot 0x18(object, index) answers a key;
// non-zero, slot 0x10 and slot 0x14 answer two words and the frame words are scanned (0x08B7B8C4(word, second) == 0)
// from the last to the first. The index that matched picks the work: 1 and 3 build through 0x08B62344(0x7C) and
// 0x0887FFBC / 0x0887F4FC(made, second, a0+0x60), then 0x0886A9A0(object, 3) / 0x08BA415C / (when that answers)
// 0x08884720 and its +0xC = 0x90, then 0x0886ADEC(object, 1); 5 and 6 build through 0x08B62344(0x78),
// 0x0887E750 / 0x0887E4F0(made, second, a0+0x60) and 0x0886ADEC(made, 1); any other index does nothing. v0 is the
// loop's last slot 0x0C answer.

// ---- 0x08A79AEC ---------------------------------------------------------------------------------------------------
// a0 = out, a1 = object: the object's virtual +0x78 chooses. Non-zero: n = its virtual +0x7C, 0x08A98C1C(sp+0xC, n)
// builds a list whose elements (word(list+0x14) + 4 x index) each take the virtual +0x84 answer of the object (assigned
// with 0x08A5BAD8, then released), and out gets the list through 0x08A76ABC. Zero: when the virtual +0x90 says no,
// 0x08A76AA0(out); otherwise 0x08A813F4(sp+0xC) builds the list and the virtual +0xA8 gives an iterator at sp+0x10,
// whose element (word(iterator+0x14)) is stepped through its virtual +8 / +0xC answers; each step feeds
// 0x08A76ABC(sp+0x14, element + 4), builds two holders from the element's virtual +0x84 (indices 0 and 1) and runs the
// list's own +0xA0 function on (list, first, second), releasing the three temporaries; then out gets the list. v0 = out.
void func_08A79AEC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    CalleeSaved saved(ctx);
    const u32 out = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    if (call_method(rt, ctx, object, 0x78u, {object}) != 0u) {
        const u32 count = call_method(rt, ctx, object, 0x7Cu, {object});
        call(rt, ctx, 0x08A98C1Cu, {frame.at(0x0Cu), count});
        for (u32 index = 0u; index != count; ++index) {
            const u32 element = word(rt, word(rt, frame.at(0x0Cu)) + 0x14u) + index * 4u;
            call_indirect(rt, ctx, word(rt, word(rt, object) + 0x84u), {frame.at(0x08u), object, index});
            if (element != frame.at(0x08u)) call(rt, ctx, 0x08A5BAD8u, {element, word(rt, frame.at(0x08u))});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x08u)});
        }
        call(rt, ctx, 0x08A76ABCu, {out, frame.at(0x0Cu)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x0Cu)});
        ctx.gpr[2] = out;
        return;
    }
    if (call_method(rt, ctx, object, 0x90u, {object}) == 0u) {
        call(rt, ctx, 0x08A76AA0u, {out});
        ctx.gpr[2] = out;
        return;
    }
    call(rt, ctx, 0x08A813F4u, {frame.at(0x0Cu)});
    call_method(rt, ctx, object, 0xA8u, {frame.at(0x10u), object});
    for (;;) {
        u32 iterator = word(rt, frame.at(0x10u));
        if (iterator == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E90u});
        const u32 element = word(rt, iterator + 0x14u);
        if (call_method(rt, ctx, element, 0x08u) == 0u) break;
        call_method(rt, ctx, word(rt, iterator + 0x14u), 0x0Cu);
        iterator = word(rt, frame.at(0x10u));
        if (iterator == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E90u});
        call(rt, ctx, 0x08A76ABCu, {frame.at(0x14u), word(rt, iterator + 0x14u) + 4u});
        const u32 list = word(rt, frame.at(0x0Cu));
        if (list == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0F20u});
        const u32 handler = word(rt, word(rt, list) + 0xA0u);
        u32 holder = word(rt, frame.at(0x14u));
        if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
        call_method(rt, ctx, holder, 0x84u, {frame.at(0x04u), holder, 0u});
        holder = word(rt, frame.at(0x14u));
        if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
        call_method(rt, ctx, holder, 0x84u, {frame.at(0x00u), holder, 1u});
        call(rt, ctx, handler, {list, frame.at(0x04u), frame.at(0x00u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x00u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x04u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    }
    call(rt, ctx, 0x08A76ABCu, {out, frame.at(0x0Cu)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x0Cu)});
    ctx.gpr[2] = out;
}

} // namespace
} // namespace new_batch_main_995
} // namespace gt::game

#define NEW995(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_995::name, gt::ReturnKind::kind)
NEW995(0x089B2014u, func_089B2014, Word);
NEW995(0x088C8330u, func_088C8330, Word);
NEW995(0x08AF77DCu, func_08AF77DC, Word);
NEW995(0x08A79AECu, func_08A79AEC, Word);
