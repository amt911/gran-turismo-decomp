// New batch main 993: nine functions hand-written from `tools/disasm` as structured C. Every delay slot is read as
// "before the call / branch", every argument register a callee reads is passed explicitly, and v0 ends where the asm
// leaves it on every exit path. No names: none has evidence. Shapes: /home/andres/repos/gt-ops/shapes/shapes_main993.txt.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_993 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next
// instruction sees.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    take(ctx, result);
    return result;
}
inline u32 call(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, address, arguments).v0;
}
// The argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order); the registers the list
// does not name keep what the previous callee left, as in the asm.
inline GuestResult call_set_keep(Runtime &rt, AllegrexContext &ctx, u32 address,
                                 std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_set_keep(rt, ctx, address, arguments).v0;
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    take(ctx, result);
    return result;
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }

// s0 and $fp are callee-saved: the body sets them like the original and the caller's values come back.
class SavedS0 {
public:
    explicit SavedS0(AllegrexContext &ctx) : ctx_(ctx), saved_(ctx.gpr[16]) {}
    ~SavedS0() { ctx_.gpr[16] = saved_; }
    SavedS0(const SavedS0 &) = delete;
    SavedS0 &operator=(const SavedS0 &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_;
};

// 0x088F587C (a0 = output record, a1 = object, a2 = stored halfword): with the object's +8 holder's +2 byte zero,
// 0x08949A08(object + 0xC) answers `base`, 0x08955300(object, tag) `klass` and 0x0893C780(object, tag) `model`. A set
// +0x10C byte on klass picks one of 0x08CE9C88's three 0x84-byte entries from the klass byte +0xE9 (bit masks 0x703C
// and 0x00700040 over 1 << byte), scales klass +0x104 by 0.8 when base is non-zero, folds the entry's +0x20 and +0x6C
// through 0x089467AC, 0x08B49D7C and the entry's +0x7C/+0x80/+0x78 terms (clamped to 1.0; klass byte 8 clears it),
// then fills the record from 0x089552F4 / 0x0893C780 and the two objects. v0 = 0x089552F4's.
void func_088F587C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 tag = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const u32 holder = word(rt, object + 8u);
    const u32 skip = byte_u(rt, holder + 2u);
    if (skip != 0u) {
        ctx.gpr[2] = skip;
        return;
    }
    float scale = 0.0f;
    const u32 base = call_set(rt, ctx, 0x08949A08u, {word(rt, object + 0xCu)});
    const u32 klass = call_set(rt, ctx, 0x08955300u, {object, tag});
    const u32 model = call_set(rt, ctx, 0x0893C780u, {object, tag});
    if (byte_u(rt, klass + 0x10Cu) != 0u) {
        const u32 category = byte_u(rt, klass + 0xE9u);
        u32 slot = 0u;
        if (category < 0x17u) {
            const u32 bit = 1u << (category & 31u);
            if ((bit & 0x703Cu) != 0u) slot = 1u;
            else if ((bit & 0x00700040u) != 0u) slot = 2u;
        }
        float angle = float_at(rt, klass + 0x104u);
        if (base != 0u) angle = psp_mul(angle, 0.8f);  // 0x3F4CCCCD
        const u32 entry = 0x08CE9C88u + (slot << 7) + (slot << 2);  // slot * 0x84 as two shifts
        scale = call_setf(rt, ctx, 0x089467ACu, {entry + 0x20u}, {angle}).f0;
        const float inner = call_setf(rt, ctx, 0x089467ACu, {entry + 0x6Cu},
                                      {float_at(rt, klass + 0x100u)}).f0;
        const float squared = psp_mul(inner, inner);
        scale = psp_mul(scale, scale);
        scale = call_setf(rt, ctx, 0x08B49D7Cu, {}, {squared + scale}).f0;
        const float delta = float_at(rt, klass + 0x4Cu) - float_at(rt, entry + 0x7Cu);
        if (0.0f < delta) scale = scale + psp_mul(delta, float_at(rt, entry + 0x80u));
        const u32 kind = byte_u(rt, klass + 0xE9u);
        if (kind == 8u) {
            scale = 0.0f;
        } else if (scale != 0.0f) {  // c.eq.s / bc1t: an exact zero skips the bias
            scale = scale + float_at(rt, entry + 0x78u);
        }
        if (1.0f < scale) scale = 1.0f;
    }

    const u32 id = call_set(rt, ctx, 0x089552F4u, {object});
    put16(rt, out, id);
    put16(rt, out + 2u, tag);
    put(rt, out + 4u, byte_u(rt, klass + 0xE9u));
    put_float(rt, out + 0x2Cu, scale);
    put_float(rt, out + 0x18u, float_at(rt, model + 0x10u));
    put_float(rt, out + 0x14u, float_at(rt, holder + 0x704u) + float_at(rt, klass + 0x10u));
    put_float(rt, out + 8u, float_at(rt, holder + 0x10u) + float_at(rt, klass));
    put_float(rt, out + 0xCu, float_at(rt, klass + 0xBCu));
    put_float(rt, out + 0x10u, -(float_at(rt, holder + 0x14u) + float_at(rt, klass + 4u)));
    for (u32 index = 0u; index != 4u; ++index) {
        put_float(rt, out + 0x1Cu + 4u * index, float_at(rt, model + 0x18u + 4u * index));
    }
    put_float(rt, out + 0x28u, float_at(rt, out + 0x28u) + psp_mul(float_at(rt, out + 0x20u), -0.05f));
    put_float(rt, out + 0x30u, float_at(rt, klass + 0x3Cu));
    put_float(rt, out + 0x34u, float_at(rt, klass + 0x30u));
    put_float(rt, out + 0x38u, float_at(rt, klass + 0x34u));
    ctx.gpr[2] = id;
    ctx.gpr[3] = out + 0x10u;  // the loop's destination register at the epilogue
}

// 0x08A14D6C (a0 = object): 0x08B2E924(object) then a tail call to 0x08A14B14(object); v0 is the tail call's.

// 0x08AFDA58 (a0 = object, a1..a3 = record fields, t0 = vector, t1 = count, t2 = arg, t3 = second object; the caller's
// 9th..12th words at entry sp + 0/4/8/0xC): for each of the count entries (0x10 bytes apart in the vector) 0x08B03170
// fills the frame's +0x18, a negative answer skipping the entry; 0x08C46034's word pair, its second word's first
// pointer's three floats (+0x6C in the frame) and 0x08BDCCB8 + 0x08BAE2FC, 0x08C45D8C + 0x08BAE278 + 0x08BAE28C,
// 0x08BAC284, 0x08BAE364 and 0x08BAC270 + 0x08BAC428 build the matrices, and 0x08B01F0C (eight register arguments plus
// four words on the stack) returns the record to store: the +0x18 float at +0x30, half of the two objects' +0xD8 table
// floats +0x64/+0x68 at +0x24/+0x28 and their +0x6C product at +0x2C, with 1 << index ORed into the 12th argument's
// word. v0 = the caller's 11th word.
void func_08AFDA58(Runtime &rt, AllegrexContext &ctx) {
    const u32 entry_sp = ctx.gpr[29];
    const u32 object = ctx.gpr[4];
    const u32 arg_a = ctx.gpr[5];
    const u32 arg_b = ctx.gpr[6];
    const u32 arg_c = ctx.gpr[7];
    const u32 vector = ctx.gpr[8];
    const u32 count = ctx.gpr[9];
    const u32 arg_e = ctx.gpr[10];
    const u32 second = ctx.gpr[11];
    const u32 stacked_9 = word(rt, entry_sp);           // 0xE0(sp) after the prologue
    const u32 stacked_10 = word(rt, entry_sp + 4u);     // 0xE4(sp)
    const u32 stacked_11 = word(rt, entry_sp + 8u);     // 0xE8(sp), the answer
    const u32 mask_out = word(rt, entry_sp + 0xCu);     // 0xEC(sp)
    GuestFrame frame(ctx, 0xE0u);
    const u32 table_base = word(rt, object + 0xD8u);
    const u32 table = table_base + ((arg_c << 9) + 0x80u);
    if (signed_of(count) <= 0) {
        ctx.gpr[2] = stacked_11;
        return;
    }
    const float half = 0.5f;
    for (u32 index = 0u; index != count; ++index) {
        const s32 found = signed_of(call(rt, ctx, 0x08B03170u,
                                         {table, arg_e, vector + 0x10u * index, frame.at(0x18u)}));
        if (found < 0) continue;
        // 0x08C46034 takes the vector element's address through sp + 0x14.
        put(rt, frame.at(0x14u), vector + 0x10u * index);
        const GuestResult position = call_keep(rt, ctx, 0x08C46034u, {object, frame.at(0x14u)});
        const u32 pointer = word(rt, position.v1);
        put_float(rt, frame.at(0x6Cu), float_at(rt, pointer));
        put_float(rt, frame.at(0x70u), float_at(rt, pointer + 4u));
        put_float(rt, frame.at(0x74u), float_at(rt, pointer + 8u));
        (void)call_set(rt, ctx, 0x08BAC284u, {frame.at(0x48u), position.v0, frame.at(0x6Cu)});
        const GuestResult pair_one = call_set_keep(rt, ctx, 0x08BDCCB8u, {frame.at(0x48u), object + 0x20u});
        (void)call_set(rt, ctx, 0x08BAE2FCu, {frame.at(0x54u), pair_one.v0, pair_one.v1});
        const u32 part = call(rt, ctx, 0x08C45D8Cu,
                              {table + 0xE0u + 0x10u * static_cast<u32>(found)});
        const GuestResult pair_two = call_set_keep(rt, ctx, 0x08BAE278u, {object, part});
        put(rt, frame.at(0x24u), pair_two.v0);
        put(rt, frame.at(0x28u), pair_two.v1);
        const u32 coordinates = call(rt, ctx, 0x08BAE28Cu, {frame.at(0x24u)});
        (void)call_set(rt, ctx, 0x08BAC284u,
                       {frame.at(0x6Cu), word(rt, coordinates), word(rt, coordinates + 4u)});
        (void)call_set(rt, ctx, 0x08BAE364u, {frame.at(0x60u), frame.at(0x6Cu)});
        const GuestResult pair_three = call_set_keep(rt, ctx, 0x08BAC270u, {frame.at(0x54u), second + 0x20u});
        (void)call_set(rt, ctx, 0x08BAC428u, {frame.at(0x3Cu), pair_three.v0, pair_three.v1});
        // The callee's four stack words live at this frame's start, as the original's outgoing argument area does.
        put(rt, frame.at(0u), stacked_10);
        put(rt, frame.at(4u), index);
        put(rt, frame.at(8u), frame.at(0x3Cu));
        put(rt, frame.at(0xCu), frame.at(0x60u));
        const GuestResult record =
            call_set_keep(rt, ctx, 0x08B01F0Cu,
                          {arg_a, frame.at(0xE8u), 1u, arg_b, arg_c, static_cast<u32>(found),
                           frame.at(0x48u), stacked_9});
        if (record.v0 == 0u) {
            ctx.gpr[2] = stacked_11;
            return;
        }
        const u32 bit = 1u << (index & 31u);
        put_float(rt, record.v0 + 0x30u, float_at(rt, frame.at(0x18u)));
        const u32 table_a = word(rt, object + 0xD8u);
        const u32 table_b = word(rt, second + 0xD8u);
        const float lower = float_at(rt, table_a + 0x68u) + float_at(rt, table_b + 0x68u);
        const float upper = float_at(rt, table_a + 0x64u) + float_at(rt, table_b + 0x64u);
        put_float(rt, record.v0 + 0x24u, psp_mul(lower, half));
        put_float(rt, record.v0 + 0x28u, psp_mul(upper, half));
        const u32 table_c = word(rt, object + 0xD8u);
        const u32 table_d = word(rt, second + 0xD8u);
        put_float(rt, record.v0 + 0x2Cu, psp_mul(float_at(rt, table_c + 0x6Cu), float_at(rt, table_d + 0x6Cu)));
        put(rt, mask_out, word(rt, mask_out) | bit);
    }
    ctx.gpr[2] = stacked_11;
}

// 0x08B42A90 (a0 = manager, a1 = command, a2 = output byte, a3/t2/t3 = ids, t0/t1 masked flags): the manager's virtual
// +0xA4 answers zero to stop; its virtual +0xA0 and then either the +0xB0 object's virtual +0x98 and 0x08B429D0, or the
// node list 0x08B3F360 walks (node word +0x38/+0x3C against t2/t3, its unsigned halfword against the +0x90 virtual,
// and 0x08B40950 for the entries that pass the byte +5 / +0x28 tests), counting matches in two frame words.
// v0 = 0 when a match count is set, else 1 when any other entry did, else t2/t3 decide.
void func_08B42A90(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg_a1 = ctx.gpr[5];
    const u32 arg_a2 = ctx.gpr[6];
    const u32 arg_a3 = ctx.gpr[7];
    const u32 flag_t0 = ctx.gpr[8] & 0xFFu;
    const u32 flag_t1 = ctx.gpr[9] & 0xFFu;
    const u32 id_t2 = ctx.gpr[10];
    const u32 id_t3 = ctx.gpr[11];
    GuestFrame frame(ctx, 0x40u);

    const GuestResult first = call_set_keep(rt, ctx, word(rt, word(rt, self) + 0xA4u),
                                            {self, arg_a1, arg_a2, arg_a3});
    if (first.v0 == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    // The byte is read in the delay slot of the next virtual call.
    const u32 target_byte = byte_u(rt, arg_a2);
    const GuestResult second = call_set_keep(rt, ctx, word(rt, word(rt, self) + 0xA0u),
                                             {self, arg_a1, arg_a2, arg_a3});
    if (second.v0 == 0u && target_byte != 0u) {
        if (flag_t0 != 0u) {
            const u32 holder = word(rt, self + 0xB0u);
            (void)call_set_keep(rt, ctx, word(rt, word(rt, holder) + 0x98u),
                                {holder, arg_a1, arg_a2, arg_a3});
        }
        ctx.gpr[2] = call_set(rt, ctx, 0x08B429D0u, {self, arg_a1, arg_a2, arg_a3, 1u});
        return;
    }
    if (second.v0 != 0u) put8(rt, arg_a2, 0u);

    put(rt, frame.at(8u), self + 0x12Cu);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0u), 0u);  // the second zero store is the jalr's delay slot
    const GuestResult type_answer = call_set_keep(rt, ctx, word(rt, word(rt, self) + 0x90u), {self});
    const u32 wanted_type = type_answer.v0;
    (void)call(rt, ctx, 0x08B344E0u, {frame.at(8u)});
    u32 node = call(rt, ctx, 0x08B3F360u, {self + 0x8DCu});
    u32 other = 0u;
    u32 matched = 0u;
    while (node != 0u) {
        const u32 entry = word(rt, node);
        bool skip = false;
        if (word(rt, entry + 0x38u) == id_t2 && word(rt, entry + 0x3Cu) == id_t3) {
            skip = true;
        }
        if (!skip && wanted_type == (word(rt, entry) & 0xFFFFu)) {
            if (flag_t0 != 0u) {
                const u32 holder = word(rt, self + 0xB0u);
                (void)call_set_keep(rt, ctx, word(rt, word(rt, holder) + 0x98u),
                                    {holder, arg_a1, arg_a2, arg_a3});
                ++other;
                put(rt, frame.at(0u), other);
            }
            skip = true;
        }
        if (!skip) {
            bool call_entry = false;
            if (word(rt, self + 0x944u) != 0u || word(rt, entry + 0x28u) == 4u) {
                const u32 flag = byte_u(rt, entry + 5u);
                if (flag != 0u) {
                    call_entry = true;
                } else {
                    call_entry = !(arg_a1 == 0x4E27u || arg_a1 == 0x4E28u || arg_a1 == 0x4E29u ||
                                   arg_a1 == 0x2715u || arg_a1 == 0x4E2Au || arg_a1 == 6u || arg_a1 == 7u);
                }
            }
            if (call_entry) {
                if (call(rt, ctx, 0x08B40950u, {self, entry, arg_a1, arg_a2, arg_a3, flag_t1}) != 0u) {
                    ++other;
                    put(rt, frame.at(0u), other);
                } else {
                    ++matched;
                    put(rt, frame.at(4u), matched);
                }
            }
        }
        node = word(rt, node + 8u);
    }
    (void)call(rt, ctx, 0x08B3457Cu, {frame.at(8u)});
    if (matched != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (other != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[2] = (id_t2 | id_t3) == 0u ? 0u : 1u;
}

// 0x08B8B0BC (first, last): libstdc++'s __insertion_sort over 24-byte elements {word +0, word +4, halfword vector at
// +8 (begin, end, capacity)}, compared by that vector (0x08B8A72C). Each element from the second on is copied (its
// vector duplicated through 0x08A5BC20 / 0x08B8A654), placed either in front of everything (the range rotates right by
// one) or by 0x08B8AFE8, and the copies' vectors are released (0x08B8A644, 0x08A5BC54).

// 0x08C2B7F0 (vector, position, value): insert a copy of `value` before `position` in a vector of 12-byte elements
// {refcounted pointer +0 (0x08A6972C / 0x08A6A23C), word +4, smart pointer +8 (0x08A76ABC / 0x08A5BAD8)}. With room,
// the last element is copied one slot further, [position, finish - 1) slides back, the copy of `value` goes into
// `position` and the temporary is destroyed. Without room the capacity becomes 1, twice the size or 0xFFFFFFFC bytes
// on overflow; 0x08C2B76C copies the head and tail around the gap, 0x08C2B500 destroys the old range and it is freed.

// 0x08C77D1C (self, out, length): read up to `length` decompressed bytes into `out`, returning how many went there. A
// pending byte is handed out first, then the pending window is committed (the read pointers reset to the buffer base,
// the saved end restored). Either 0x08C7327C writes from the stream, or - with a short window, a filter object whose
// slot 0x18 answers, flag 0x8 set and block mode clear - the pending tail is copied out and 0x08B61718 fills the rest
// over the work block at +0x24, resetting the window when it runs out of input.
void func_08C77D1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 out = ctx.gpr[5];
    s32 length = signed_of(ctx.gpr[6]);
    u32 total = 0u;
    if (byte_u(rt, self + 0x8Cu) != 0u) {
        if (length > 0) {
            const u32 position = word(rt, self + 8u);
            if (position == word(rt, self + 4u)) {
                const u32 pending = byte_u(rt, position);
                length -= 1;
                total = 1u;
                put8(rt, out, pending);
                out += 1u;
                put(rt, self + 8u, position + 1u);
            }
        }
        const u32 advanced =
            (word(rt, self + 8u) ^ word(rt, self + 4u)) != 0u ? 1u : 0u;
        const u32 committed = advanced + word(rt, self + 0x84u);
        put(rt, self + 4u, word(rt, self + 0x78u));
        put(rt, self + 8u, committed);
        put(rt, self + 0xCu, word(rt, self + 0x88u));
        put(rt, self + 0x84u, committed);
        put8(rt, self + 0x8Cu, 0u);
    }
    const u32 window = word(rt, self + 0x7Cu);
    const u32 limit = window < 2u ? 1u : window - 1u;
    bool through_reader = false;
    if (signed_of(limit) < length) {
        const u32 filter = word(rt, self + 0x90u);
        if (filter == 0u) {
            // The asm's assert: it never returns to this function.
            call(rt, ctx, 0x08B5A9FCu, {0u});
            return;
        }
        if (call_indirect(rt, ctx, word(rt, word(rt, filter) + 0x18u), {filter}) != 0u)
            through_reader = (word(rt, self + 0x2Cu) & 8u) != 0u && byte_u(rt, self + 0x82u) == 0u;
    }
    if (!through_reader) {
        ctx.gpr[2] = total + call(rt, ctx, 0x08C7327Cu, {self, out, static_cast<u32>(length)});
        return;
    }
    const u32 available = word(rt, self + 0xCu) - word(rt, self + 8u);
    if (available != 0u) {
        u32 chunk;
        if (available == 1u) {
            put8(rt, out, byte_u(rt, word(rt, self + 8u)));
            chunk = 1u;
        } else {
            call(rt, ctx, 0x08B79A88u, {out, word(rt, self + 8u), available});
            chunk = available;
        }
        out += chunk;
        length -= signed_of(available);
        total += available;
        put(rt, self + 8u, word(rt, self + 8u) + chunk);
    }
    for (;;) {
        const u32 got = call(rt, ctx, 0x08B61718u, {self + 0x24u, out, static_cast<u32>(length)});
        if (got == 0xFFFFFFFFu) {
            call(rt, ctx, 0x08B5AA30u, {0x08CB5D6Cu});
            call(rt, ctx, 0x08B5A9FCu, {0x08CB5D6Cu});
            return;
        }
        if (got != 0u) {
            length -= signed_of(got);
            total += got;
            if (length == 0) break;
            out += got;
            continue;
        }
        if (length == 0) break;
        const u32 base = word(rt, self + 0x78u);
        put8(rt, self + 0x81u, 0u);
        put(rt, self + 0xCu, base);
        put(rt, self + 4u, base);
        put(rt, self + 8u, base);
        put(rt, self + 0x14u, 0u);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x18u, 0u);
        ctx.gpr[2] = total;
        return;
    }
    const u32 flags = word(rt, self + 0x2Cu);
    const u32 base = word(rt, self + 0x78u);
    put(rt, self + 4u, base);
    put(rt, self + 8u, base);
    put(rt, self + 0xCu, base);
    if ((flags & 0x10u) != 0u) {
        const u32 restart_window = word(rt, self + 0x7Cu);
        if (restart_window >= 2u) {
            put(rt, self + 0x18u, base + restart_window - 1u);
            put(rt, self + 0x14u, base);
            put(rt, self + 0x10u, base);
            put8(rt, self + 0x81u, 1u);
            ctx.gpr[2] = total;
            return;
        }
    }
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x18u, 0u);
    put8(rt, self + 0x81u, 1u);
    ctx.gpr[2] = total;
}

// 0x08863348 (self, a1, a2): 0x089624FC(self + 0x44BC) non-zero answers at once; otherwise 0x08963418 builds two
// reference-counted pairs on the frame and they are assigned to self + 0x44BC/+0x44C0 and self + 0x44C4/+0x44C8. An
// assignment increments the new count (+4), decrements the old one and, on the last use, runs the old object's virtual
// +8 then its +0xC. The two temporaries at frame +0xC and +0x14 are released the same way.
void func_08863348(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg1 = ctx.gpr[5];
    const u32 arg2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const u32 first = self + 0x44BCu;
    const u32 second = self + 0x44C0u;
    const u32 third = self + 0x44C4u;
    const u32 fourth = self + 0x44C8u;
    const GuestResult check = call_set_keep(rt, ctx, 0x089624FCu, {first});
    if (check.v0 != 0u) {
        ctx.gpr[2] = check.v0;
        return;
    }
    put(rt, frame.at(0u), arg1);
    put(rt, frame.at(4u), arg2);
    call_set(rt, ctx, 0x08963418u, {frame.at(8u), frame.at(0u)});
    const u32 first_data = word(rt, frame.at(8u));
    const u32 first_ctrl = word(rt, frame.at(0xCu));
    put(rt, first, first_data);
    // The first assignment (its v0 is overwritten by the second block's 0x10 load below).
    if (first_ctrl != word(rt, second)) {
        if (first_ctrl != 0u) put(rt, first_ctrl + 4u, word(rt, first_ctrl + 4u) + 1u);
        const u32 old = word(rt, second);
        if (old != 0u && word(rt, old + 4u) == 1u) {
            put(rt, old + 4u, 0u);
            (void)call_indirect(rt, ctx, word(rt, word(rt, old) + 8u), {old});
            if (word(rt, old + 8u) == 1u) {
                put(rt, old + 8u, 0u);
                (void)call_indirect(rt, ctx, word(rt, word(rt, old) + 0xCu), {old});
            } else {
                put(rt, old + 8u, word(rt, old + 8u) - 1u);
            }
        } else if (old != 0u) {
            put(rt, old + 4u, word(rt, old + 4u) - 1u);
        }
        put(rt, second, first_ctrl);
    }
    const u32 second_data = word(rt, frame.at(0x10u));
    const u32 second_ctrl = word(rt, frame.at(0x14u));
    put(rt, third, second_data);
    u32 v0 = second_data;
    if (second_ctrl != word(rt, fourth)) {
        if (second_ctrl != 0u) {
            put(rt, second_ctrl + 4u, word(rt, second_ctrl + 4u) + 1u);
            v0 = 1u;  // the sc of the increment
        }
        const u32 old = word(rt, fourth);
        if (old != 0u) {
            v0 = 1u;
            if (word(rt, old + 4u) == 1u) {
                put(rt, old + 4u, 0u);
                v0 = call_indirect(rt, ctx, word(rt, word(rt, old) + 8u), {old});
                if (word(rt, old + 8u) == 1u) {
                    put(rt, old + 8u, 0u);
                    v0 = call_indirect(rt, ctx, word(rt, word(rt, old) + 0xCu), {old});
                } else {
                    put(rt, old + 8u, word(rt, old + 8u) - 1u);
                    v0 = 1u;
                }
            } else {
                put(rt, old + 4u, word(rt, old + 4u) - 1u);
            }
        }
        put(rt, fourth, second_ctrl);
    }
    // Release the pair's second control block, then its first.
    if (second_ctrl != 0u) {
        v0 = 1u;
        if (word(rt, second_ctrl + 4u) == 1u) {
            put(rt, second_ctrl + 4u, 0u);
            v0 = call_indirect(rt, ctx, word(rt, word(rt, second_ctrl) + 8u), {second_ctrl});
            if (word(rt, second_ctrl + 8u) == 1u) {
                put(rt, second_ctrl + 8u, 0u);
                v0 = call_indirect(rt, ctx, word(rt, word(rt, second_ctrl) + 0xCu), {second_ctrl});
            } else {
                put(rt, second_ctrl + 8u, word(rt, second_ctrl + 8u) - 1u);
                v0 = 1u;
            }
        } else {
            put(rt, second_ctrl + 4u, word(rt, second_ctrl + 4u) - 1u);
        }
    }
    if (first_ctrl != 0u) {
        v0 = 1u;
        if (word(rt, first_ctrl + 4u) == 1u) {
            put(rt, first_ctrl + 4u, 0u);
            v0 = call_indirect(rt, ctx, word(rt, word(rt, first_ctrl) + 8u), {first_ctrl});
            if (word(rt, first_ctrl + 8u) == 1u) {
                put(rt, first_ctrl + 8u, 0u);
                v0 = call_indirect(rt, ctx, word(rt, word(rt, first_ctrl) + 0xCu), {first_ctrl});
            } else {
                put(rt, first_ctrl + 8u, word(rt, first_ctrl + 8u) - 1u);
                v0 = 1u;
            }
        } else {
            put(rt, first_ctrl + 4u, word(rt, first_ctrl + 4u) - 1u);
        }
    }
    ctx.gpr[2] = v0;
}

// 0x088A7F1C (self, first, second, third, extra = t0, fourth = t1, f12/f13/f14): when the local matrix at self + 0x104
// has its +8 word equal to its +4, its holder's vtable float +0xC below f12 returns holder/vtable/float unchanged.
// Otherwise two matrix halves of `second` and `first` are crossed and added, the quaternion 0x08BAC230(first)'s xyz
// negated rotates `third`, `fourth` and the relative position, and three 4-vectors (rotated third/position/fourth with
// f12/f13/f14) go to 0x0896108C(self + 0x104, records).
void func_088A7F1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 extra = ctx.gpr[8];
    const u32 fourth = ctx.gpr[9];
    const float f12 = ctx.fpr[12];
    const float f13 = ctx.fpr[13];
    const float f14 = ctx.fpr[14];
    GuestFrame frame(ctx, 0x110u);
    const u32 local = self + 0x104u;
    if (word(rt, local + 8u) == word(rt, local + 4u)) {
        const u32 holder = word(rt, local);
        const u32 vtable = word(rt, holder);
        const float probe = float_at(rt, vtable + 0xCu);
        ctx.gpr[2] = holder;
        ctx.gpr[3] = vtable;
        ctx.fpr[0] = probe;
        if (!(probe < f12)) return;
    }
    GuestResult high = call_set_keep(rt, ctx, 0x08BAE264u, {second + 0x50u, extra});
    put(rt, frame.at(0x2Cu), high.v0);
    put(rt, frame.at(0x30u), high.v1);
    GuestResult low = call_set_keep(rt, ctx, 0x08BAE250u, {second + 0x38u, frame.at(0x2Cu)});
    put(rt, frame.at(0x34u), low.v0);
    put(rt, frame.at(0x38u), low.v1);
    (void)call_set(rt, ctx, 0x08BAE2B0u,
                   {frame.at(0x6Cu), word(rt, low.v1), word(rt, low.v1 + 4u)});
    (void)call_set(rt, ctx, 0x08BAE2FCu, {frame.at(0x3Cu), low.v0, frame.at(0x6Cu)});

    high = call_set_keep(rt, ctx, 0x08BAE264u, {first + 0x50u, third});
    put(rt, frame.at(0x1Cu), high.v0);
    put(rt, frame.at(0x20u), high.v1);
    low = call_set_keep(rt, ctx, 0x08BAE250u, {first + 0x38u, frame.at(0x1Cu)});
    put(rt, frame.at(0x24u), low.v0);
    put(rt, frame.at(0x28u), low.v1);
    (void)call_set(rt, ctx, 0x08BAE2B0u,
                   {frame.at(0x6Cu), word(rt, low.v1), word(rt, low.v1 + 4u)});
    (void)call_set(rt, ctx, 0x08BAE2FCu, {frame.at(0x48u), low.v0, frame.at(0x6Cu)});
    (void)call_set(rt, ctx, 0x08BAE330u, {frame.at(0x3Cu), frame.at(0x48u)});

    const u32 quaternion = call(rt, ctx, 0x08BAC230u, {first});
    put_float(rt, frame.at(0x78u), -float_at(rt, quaternion));
    put_float(rt, frame.at(0x7Cu), -float_at(rt, quaternion + 4u));
    put_float(rt, frame.at(0x80u), -float_at(rt, quaternion + 8u));
    put_float(rt, frame.at(0x84u), float_at(rt, quaternion + 0xCu));

    GuestResult rotated = call_set_keep(rt, ctx, 0x08BAE278u, {frame.at(0x78u), third});
    (void)call_set(rt, ctx, 0x08BAC284u, {frame.at(0x54u), rotated.v0, rotated.v1});
    rotated = call_set_keep(rt, ctx, 0x08BAE278u, {frame.at(0x78u), fourth});
    (void)call_set(rt, ctx, 0x08BAC284u, {frame.at(0x60u), rotated.v0, rotated.v1});
    rotated = call_set_keep(rt, ctx, 0x08BAE278u, {frame.at(0x78u), frame.at(0x3Cu)});
    (void)call_set(rt, ctx, 0x08BAC284u, {frame.at(0x3Cu), rotated.v0, rotated.v1});

    (void)call_set(rt, ctx, 0x08BAE294u, {frame.at(0x88u), frame.at(0x54u)});
    (void)call_set(rt, ctx, 0x08BAE294u, {frame.at(0x98u), frame.at(0x3Cu)});
    (void)call_set(rt, ctx, 0x08BAE294u, {frame.at(0xA8u), frame.at(0x60u)});
    put_float(rt, frame.at(0x94u), f12);
    put_float(rt, frame.at(0xA4u), f13);
    put_float(rt, frame.at(0xB4u), f14);
    ctx.gpr[2] = call_set(rt, ctx, 0x0896108Cu, {local, frame.at(0x88u)});
}

} // namespace
} // namespace new_batch_main_993
} // namespace gt::game

#define NEW993(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_993::name, gt::ReturnKind::kind)
NEW993(0x088F587Cu, func_088F587C, Word);
NEW993(0x08AFDA58u, func_08AFDA58, Word);
NEW993(0x08B42A90u, func_08B42A90, Word);
NEW993(0x08C77D1Cu, func_08C77D1C, Word);
NEW993(0x08863348u, func_08863348, Void);
NEW993(0x088A7F1Cu, func_088A7F1C, Word);
