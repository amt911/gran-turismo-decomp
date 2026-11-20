// New batch 22: functions of 360-400 bytes that the recompiler emits, written by hand from `tools.disasm` as structured C
// (if/while/switch, named typed locals). Not verified yet (draft): no case has been captured for these.
// Delay slots are read as "before the branch / call". The callee-saved registers the original keeps values in are set
// where the asm sets them (a callee may read them) and put back at the end; `v0` (or `f0` for the float ones) ends as the
// asm leaves it. A call leaves v0, v1 and a1 as the callee did (a recompiled callee may read the caller-saved ones).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_22 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

// A guest call; afterwards v0/v1 and a1 hold the callee's, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    return result.v0;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
         std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

s8 signed_byte_at(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
s16 signed_half_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
u8 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u16 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u8 value) { rt.memory().store8(address, value); }
void put_half(Runtime &rt, u32 address, u16 value) { rt.memory().store16(address, value); }

// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// s0-s7 and fp are callee-saved: the body sets them where the original does (a callee may read them) and the caller's
// values come back at the end.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), fp_(ctx.gpr[30]) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 fp_;
    u32 saved_[8];
};

// The counted block a `word + 8` record carries: the block at `pointer` (its base is pointer - 0xC) is skipped when it is
// the shared empty block 0x08D74FD0, or when 0x08B60804(pointer - 4, -1) answers a positive count; otherwise
// 0x08B8542C(base, marker) frees it.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 pointer, u32 marker) {
    if (pointer - 0xCu == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {pointer - 0xCu, marker});
}
void release_counted_block(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 marker) {
    release_counted(rt, ctx, word(rt, slot), marker);
}

// 0x08BE6244 (a0 = a container whose word +4 is an end pointer, a1 = destination, a2 = source): assigns
// ((end - source) >> 5) / 3 records of 0x60 bytes one by one — two words, five floats, then the sub-objects at +0x20 and
// +0x40 (each assigned through 0x089641C4 unless it already is that address, its word +0x10 copied after) — then
// 0x08BE59D4(destination cursor, end, &local) finishes and the container's end pointer is reduced by the distance from
// the destination to the source with its low five bits cleared. v0 = the destination start.
void func_08BE6244(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[30] = destination;  // move fp, a1
    ctx.gpr[16] = destination;  // move s1, a1: the destination cursor
    const u32 shifted = static_cast<u32>(static_cast<s32>(word(rt, self + 4u) - source) >> 5);
    const u32 count = shifted * 0xAAAAAAABu;  // the `mult`'s low word: (end - source) / 96
    u32 cursor = destination;
    if (static_cast<s32>(count) > 0) {
        for (u32 i = 0u; i != count; ++i) {
            const u32 from = source + i * 0x60u;
            const u32 first = word(rt, from + 4u);
            const u32 second = word(rt, from + 8u);
            const float f0 = float_at(rt, from + 0xCu);
            const float f1 = float_at(rt, from + 0x10u);
            const float f2 = float_at(rt, from + 0x14u);
            const float f3 = float_at(rt, from + 0x18u);
            const float f4 = float_at(rt, from + 0x1Cu);
            const u32 first_member = cursor + 0x20u;
            const u32 first_source = from + 0x20u;
            const u32 second_member = cursor + 0x40u;
            const u32 second_source = from + 0x40u;
            put(rt, cursor + 4u, first);
            put(rt, cursor + 8u, second);
            put_float(rt, cursor + 0xCu, f0);
            put_float(rt, cursor + 0x10u, f1);
            put_float(rt, cursor + 0x14u, f2);
            put_float(rt, cursor + 0x18u, f3);
            put_float(rt, cursor + 0x1Cu, f4);
            cursor += 0x60u;
            ctx.gpr[16] = cursor;      // s1 is the next record by the time the calls run
            ctx.gpr[18] = from + 0x60u;  // s0 likewise
            if (first_member != first_source) {
                call(rt, ctx, 0x089641C4u, {first_member, first_source});
                put(rt, first_member + 0x10u, word(rt, first_source + 0x10u));
            }
            if (second_member != second_source) {
                call(rt, ctx, 0x089641C4u, {second_member, second_source});
                put(rt, second_member + 0x10u, word(rt, second_source + 0x10u));
            }
        }
    }
    const u32 local = frame.at(0u);
    call(rt, ctx, 0x08BE59D4u, {cursor, word(rt, self + 4u), local});
    const u32 end = word(rt, self + 4u);
    put(rt, self + 4u, end - ((source - destination) & ~31u));
    ctx.gpr[2] = destination;
}

// 0x0885F938 (a0 = this, a1 = an index, a2 = two 0x30-byte records, a3 = a value, t0 = a count (a negative one is
// taken as zero), t1 and t2 = two flags kept as bytes): for both records, 0x088D698C(this + 0x5AC) counts something and
// a positive answer gets 0x08897130(record); unless 0x088977D4(record) answers -1, the record's flags are set
// (0x0889715C, 0x088975B4), the entry selected by the byte at this + 0x4120 + index * 0x50 + i + 4 (or the t2 flag for
// -1) is adjusted (0x0885DF24), and 0x088F8A84(this + 0x5B4, index, count) finishes (a tail call).
void func_0885F938(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    u32 record = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    const u32 count_input = ctx.gpr[8];
    const u32 first_flag = ctx.gpr[9] & 0xFFu;
    const u32 second_flag = ctx.gpr[10] & 0xFFu;
    const u32 negative = count_input >> 31;  // srl $s6, $t0, 31
    u32 count = count_input;
    if (negative != 0u) count = 0u;  // movn $t0, $zero, $s6
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = index;
    ctx.gpr[19] = first_flag;
    ctx.gpr[22] = negative;
    ctx.gpr[23] = 0xFFFFFFFFu;
    ctx.gpr[30] = second_flag;
    u32 entry = self + index * 0x50u + 0x4120u;  // s2
    for (u32 i = 0u; i != 2u; ++i) {
        ctx.gpr[18] = record;
        ctx.gpr[20] = i;
        const u32 found = call(rt, ctx, 0x088D698Cu, {word(rt, self + 0x5ACu)});
        ctx.gpr[5] = found;  // addu $a1, $v0, $zero
        if (static_cast<s32>(found) > 0) call(rt, ctx, 0x08897130u, {record, found});
        if (call(rt, ctx, 0x088977D4u, {record}) != 0xFFFFFFFFu) {
            call(rt, ctx, 0x0889715Cu, {record, first_flag});
            call(rt, ctx, 0x088975B4u, {record, negative});
            if (call(rt, ctx, 0x088D4D9Cu, {word(rt, self + 0x5ACu) + 4u}) == 1u) {
                if (index == 2u) call(rt, ctx, 0x088975B4u, {record, 1u});
            }
            const s32 selection = signed_byte_at(rt, entry + 4u);
            u32 argument = second_flag;
            if (selection != -1) argument = byte_at(rt, self + static_cast<u32>(selection) * 0x50u + 0x4138u);
            ctx.gpr[20] = i;
            call(rt, ctx, 0x0885DF24u, {self, index, value, record, i, argument});
        }
        ++entry;
        record += 0x30u;
    }
    tail_call(ctx, 0x088F8A84u, {word(rt, self + 0x5B4u), index, count});
}

// 0x088622CC (a0 = a pointer to the state pointer): 0x08B1A844 decides when a pass may run; a pass reads
// 0x088D4C9C(state->word + 0x5AC + 4) and walks the table at state->word + 0x3F50 — every entry of 0x188 bytes whose
// word +0x4C is 1 and whose word +0x10 is -1 with 0x0885D4CC(entry) non-zero stores the entry's word +0x1C into the
// table's word +0x10; an entry whose word +0x4C is 3 stops with 0. A pass that saw a non-1 entry waits 100000 through
// 0x08B2E2B8 and passes again, up to 900 times. v0 = 1 (nothing changed, or the table is empty) or 0 (the 3 entry,
// the retries ran out, or the early exits, which wait a few cycles first).
void func_088622CC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = self;          // s3
    ctx.gpr[20] = 1u;            // s4
    ctx.gpr[21] = 3u;            // s5
    ctx.gpr[30] = 0xFFFFFFFFu;   // fp
    u32 retries = 0x384u;        // s7
    u32 answer = 0u;
    for (;;) {
        ctx.gpr[23] = retries;
        if (call(rt, ctx, 0x08B1A844u, {}) != 0u) {  // the incoming a0 is not read by 0x08B1A844
            answer = 0u;                             // 088623D8: a short busy wait, v0 counted 4 -> 0
            break;
        }
        const u32 state = word(rt, self);
        const u32 ready = call(rt, ctx, 0x088D4C9Cu, {word(rt, state + 0x5ACu) + 4u});
        if (ready != 0u && byte_at(rt, word(rt, state + 0x5ACu) + 0x5BCu) == 0u) {
            answer = 0u;  // 08862338: the same busy wait
            break;
        }
        const u32 table = word(rt, state + 0x3F50u);
        if (static_cast<s32>(word(rt, table + 4u)) <= 0) {
            answer = 1u;
            break;
        }
        bool changed = false;
        bool aborted = false;
        u32 index = 0u;
        u32 offset = 0u;
        for (;;) {
            const u32 base = word(rt, word(rt, word(rt, self) + 0x3F50u));
            const u32 entry = word(rt, base) + offset;
            const u32 kind = word(rt, entry + 0x4Cu);
            if (kind == 1u) {
                if (word(rt, base + 0x10u) == 0xFFFFFFFFu &&
                    call(rt, ctx, 0x0885D4CCu, {entry}) != 0u) {
                    put(rt, word(rt, word(rt, self) + 0x3F50u) + 0x10u, word(rt, entry + 0x1Cu));
                }
            } else {
                changed = true;
                if (kind == 3u) {
                    aborted = true;
                    break;
                }
            }
            ++index;
            offset += 0x188u;
            if (!(static_cast<s32>(index) < static_cast<s32>(word(rt, base + 4u)))) break;
        }
        if (aborted) {
            answer = 0u;
            break;
        }
        if (!changed) {
            answer = 1u;
            break;
        }
        --retries;
        call(rt, ctx, 0x08B2E2B8u, {0x186A0u});
        if (retries == 0u) {
            answer = 0u;  // 088623D8: the same busy wait
            break;
        }
    }
    ctx.gpr[2] = answer;
}

// 0x088DFDA0 (a0 = this, a1 = a flag kept as a byte): with the flag set, 0x088E5148(this + 0x640, this + 0x138,
// 0x088D9F80(this)) and 0x088E4B80 are called; then, when this + 0x27 is set, the object at this + 0x618 either gives
// 0x088DC858 a non-zero answer or is handed to 0x088E4C28 through the local record; the records at this + 0xC0 (0x2F8
// bytes apart, as many as 0x088D9F80(this) answers) with a word +0xC and a byte +0x10 are released one by one
// (0x088E6864, or 0x088E68F8 when one of the two is zero); 0x088E6A14 and 0x088E698C finish with the two counts
// 0x088D9F80 and 0x088DA160 answer, after 0x088E6B0C/0x088E6B84 chose by 0x088FE2A4(this + 0x14).
void func_088DFDA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[18] = self;  // s2
    if (flag != 0u) {
        const u32 first = word(rt, self + 0x138u);
        const u32 second = word(rt, self + 0x640u);
        const u32 value = call(rt, ctx, 0x088D9F80u, {self});
        call(rt, ctx, 0x088E5148u, {second, first, value});
        call(rt, ctx, 0x088E4B80u, {word(rt, self + 0x640u), word(rt, self + 0x138u)});
    }
    if (byte_at(rt, self + 0x27u) != 0u) {
        const u32 holder = word(rt, self + 0x618u);
        ctx.gpr[16] = holder;
        if (call(rt, ctx, 0x088DC858u, {self, word(rt, holder + 0x22Cu)}) == 0u) {
            call(rt, ctx, 0x088D2674u, {holder, frame.at(0u)});
            call(rt, ctx, 0x088E4C28u, {word(rt, self + 0x640u), word(rt, holder + 0x22Cu),
                                        word(rt, holder + 0x230u), 0u, word(rt, holder + 0x234u), frame.at(0u)});
        }
    }
    u32 index = 0u;   // s0
    u32 offset = 0u;  // s1
    ctx.gpr[17] = offset;
    for (;;) {
        ctx.gpr[21] = index;
        const u32 count = call(rt, ctx, 0x088D9F80u, {self});
        if (!(static_cast<s32>(index) < static_cast<s32>(count))) break;
        const u32 record = word(rt, self + 0xC0u) + offset;
        if (word(rt, record + 0xCu) != 0u) {
            if (byte_at(rt, record + 0x10u) != 0u) {
                call(rt, ctx, 0x088E6864u, {word(rt, self + 0x640u), index});
            } else {
                call(rt, ctx, 0x088E68F8u, {word(rt, self + 0x640u)});
            }
        } else {
            call(rt, ctx, 0x088E68F8u, {word(rt, self + 0x640u)});
        }
        ++index;
        offset += 0x2F8u;
    }
    if (call(rt, ctx, 0x088FE2A4u, {word(rt, self + 0x14u)}) != 0u) {
        call(rt, ctx, 0x088E6B0Cu, {word(rt, self + 0x640u)});
    } else {
        call(rt, ctx, 0x088E6B84u, {word(rt, self + 0x640u)});
    }
    const u32 screen = word(rt, self + 0x640u);
    const u32 count = call(rt, ctx, 0x088D9F80u, {self});
    call(rt, ctx, 0x088E6A14u, {screen, count});
    call(rt, ctx, 0x088E698Cu, {word(rt, self + 0x640u), call(rt, ctx, 0x088DA160u, {self})});
}

// The low half of `value * factor`, taken as a signed 32-bit word and divided by 100 (the original's magic 0x51EB851F).
u32 hundredth_of(u16 value, u16 factor) {
    const s32 low = std::bit_cast<s32>(static_cast<u32>(value) * static_cast<u32>(factor));
    return static_cast<u32>(low / 100);
}

// The pair 0x0892CF9C / 0x0892D1C0 (a1 = the sums, a2 = the source, no frame): the 16 bytes at the sums' +0xC0 each take
// one source byte added, the byte +0xBD takes another, and the halfwords +0x10A and +0x10C become the low half of
// (their value times a source halfword) / 100. v0 = the first quotient (before the store truncates it).
void add_source_to_sums(Runtime &rt, AllegrexContext &ctx, u32 first_byte, u32 second_byte, u32 first_half,
                        u32 second_half) {
    const u32 sums = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u8 delta = byte_at(rt, source + first_byte);
    for (u32 offset = 0xC0u; offset < 0xD0u; ++offset) {
        put_byte(rt, sums + offset, static_cast<u8>(byte_at(rt, sums + offset) + delta));
    }
    put_byte(rt, sums + 0xBDu, static_cast<u8>(byte_at(rt, sums + 0xBDu) + byte_at(rt, source + second_byte)));
    const u32 first = hundredth_of(half_at(rt, sums + 0x10Au), half_at(rt, source + first_half));
    const u32 second = hundredth_of(half_at(rt, sums + 0x10Cu), half_at(rt, source + second_half));
    put_half(rt, sums + 0x10Au, static_cast<u16>(first));
    put_half(rt, sums + 0x10Cu, static_cast<u16>(second));
    ctx.gpr[2] = first;
}
void func_0892CF9C(Runtime &rt, AllegrexContext &ctx) { add_source_to_sums(rt, ctx, 0x7u, 0x8u, 0x0u, 0x2u); }
void func_0892D1C0(Runtime &rt, AllegrexContext &ctx) { add_source_to_sums(rt, ctx, 0x9u, 0xAu, 0x4u, 0x6u); }

// 0x08C3D12C (a0 = a begin/end/capacity container, a1 = the insertion position, a2 = a pointer to the value): with the
// end word +4 short of the capacity word +8, the in-place path moves one word up (the end's word -4 into the end, the word
// past that into +4 of the container), then memmoves `(((end - 4) - position) & ~3)` bytes into `end - that` from the s1
// the caller left, and stores the value at the position. Otherwise the end grows to twice the element count (capped at
// 0x3FFFFFFF, after the 0x3FFFFFFF length check through 0x08B5ADDC), 0x08B62344 allocates the block, the head and tail are
// memmoved in, the value stored between them, the old block released (0x08B64654) and begin/end/capacity updated.
// v0 = the memmove's answer (in place) or the new capacity end.
void func_08C3D12C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 value_pointer = ctx.gpr[6];
    const u32 caller_s1 = ctx.gpr[17];  // the original reads s1 as its caller left it in the in-place path
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = self;           // s2
    ctx.gpr[19] = position;       // s3
    ctx.gpr[21] = value_pointer;  // s5
    const u32 end = word(rt, self + 4u);
    if (end != word(rt, self + 8u)) {
        u32 base = self;     // a0
        u32 current = end;   // a3
        if (end != 0u) {
            put(rt, end, word(rt, end - 4u));
            current = word(rt, end + 4u);
            base = current;
        }
        const u32 count = ((base - 4u) - position) & ~3u;
        put(rt, self + 4u, current + 4u);
        const u32 value = word(rt, value_pointer);
        call(rt, ctx, 0x08B79BACu, {base - count, caller_s1, count});
        put(rt, position, value);
        return;
    }
    const u32 elements = static_cast<u32>(static_cast<s32>(end - word(rt, self)) >> 2);
    if (elements != 0x3FFFFFFFu) call(rt, ctx, 0x08B5ADDCu, {0x08CB4918u});  // length_error
    u32 capacity = 1u;
    if (elements != 0u) {
        capacity = elements << 1;
        if (capacity < elements) capacity = 0x3FFFFFFFu;  // movn
    }
    if (0x3FFFFFFFu < capacity) call(rt, ctx, 0x08B5A9C8u, {capacity});
    const u32 bytes = capacity << 2;  // s6
    ctx.gpr[22] = bytes;
    const u32 block = call(rt, ctx, 0x08B62344u, {bytes});
    ctx.gpr[20] = block;  // s4
    const u32 old_begin = word(rt, self);
    const u32 prefix = position - old_begin;  // s0
    const u32 middle = block + prefix;        // s1
    ctx.gpr[17] = middle;
    call(rt, ctx, 0x08B79BACu, {block, old_begin, prefix});
    u32 tail_start = middle;
    u32 tail = word(rt, self + 4u) - position;
    if (middle != 0u) {
        put(rt, middle, word(rt, value_pointer));
        tail_start = middle + 4u;
    }
    ctx.gpr[17] = tail_start;
    call(rt, ctx, 0x08B79BACu, {tail_start, position, tail});
    const u32 begin = word(rt, self);
    ctx.gpr[16] = tail_start + tail;  // s0, the new end
    if (begin != 0u) call(rt, ctx, 0x08B64654u, {begin});
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, tail_start + tail);
    put(rt, self, block);
    ctx.gpr[2] = block + bytes;
}

// 0x08AB7260 (a0 = this, a1 = an output object): 0x08AB7004(&local, this) answers a counted block in the local; 0x08B36170
// is told the object at this + 0x14, the block and the smaller of the two words before them (its non-zero answer becomes the
// count), and the block is released; then the count selects 0x08CB428C (non-zero: 0x08AB6F7C(&local2) and
// 0x08A767EC(output, 0x08CB428C, local2, this + 0x14's object)) or 0x08CB42A0 (zero: the same with local2 and the object
// word from the entry), and the second block is released too.
void func_08AB7260(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 output = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[18] = self;    // s2
    ctx.gpr[21] = output;  // s5
    call(rt, ctx, 0x08AB7004u, {frame.at(0xCu), self});
    const u32 table = word(rt, self + 0x14u);
    const u32 block = word(rt, frame.at(0xCu));  // s1
    const u32 element = word(rt, table - 0xCu);  // a3
    const u32 other = word(rt, block - 0xCu);    // v1
    put(rt, frame.at(0x10u), element);
    put(rt, frame.at(0x14u), other);
    const u32 selected = other < element ? other : element;
    u32 count = element - other;  // s0
    ctx.gpr[19] = block - 0xCu;   // s3
    ctx.gpr[20] = 0x08D74FD0u;    // s4
    const u32 grown = call(rt, ctx, 0x08B36170u, {table, block, selected});
    if (grown != 0u) count = grown;  // movn $s0, $v0, $v0
    release_counted_block(rt, ctx, frame.at(0xCu), frame.at(2u));
    if (count != 0u) {
        call(rt, ctx, 0x08AB6F7Cu, {frame.at(8u)});
        call(rt, ctx, 0x08A767ECu, {output, 0x08CB428Cu, word(rt, frame.at(8u)), word(rt, self + 0x14u)});
        release_counted_block(rt, ctx, frame.at(8u), frame.at(1u));
    } else {
        call(rt, ctx, 0x08AB6F7Cu, {frame.at(4u)});
        call(rt, ctx, 0x08A767ECu, {output, 0x08CB42A0u, word(rt, frame.at(4u)), element});
        release_counted_block(rt, ctx, frame.at(4u), frame.at(0u));
    }
}

// 0x08821ADC (a0 and a1 = two objects): 0x08A6C410(&local, object) then the object's virtual +0xC(&other, object) answer two
// blocks both times; 0x08B36170 is told the first block, the second and the smaller of their counts, its non-zero answer
// becomes the count, and the count's sign bit is the result. The second block, the second local, the first block and the
// first local are released in that order.
void func_08821ADC(Runtime &rt, AllegrexContext &ctx) {
    const u32 first_object = ctx.gpr[4];
    const u32 second_object = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[22] = frame.at(0xCu);  // s6
    call(rt, ctx, 0x08A6C410u, {frame.at(0xCu), first_object});
    const u32 first_holder = word(rt, frame.at(0xCu));
    if (first_holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99698u});
    call_indirect(rt, ctx, word(rt, word(rt, first_holder) + 0xCu), {frame.at(0x10u), first_holder});
    call(rt, ctx, 0x08A6C410u, {frame.at(4u), second_object});
    const u32 second_holder = word(rt, frame.at(4u));
    if (second_holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99698u});
    call_indirect(rt, ctx, word(rt, word(rt, second_holder) + 0xCu), {frame.at(8u), second_holder});
    const u32 first_block = word(rt, frame.at(0x10u));
    const u32 second_block = word(rt, frame.at(8u));
    const u32 first_count = word(rt, first_block - 0xCu);
    const u32 second_count = word(rt, second_block - 0xCu);
    const u32 selected = second_count < first_count ? second_count : first_count;
    u32 count = first_count - second_count;
    ctx.gpr[19] = second_block - 0xCu;  // s3
    ctx.gpr[20] = 0x08D74FD0u;          // s4
    const u32 grown = call(rt, ctx, 0x08B36170u, {first_block, second_block, selected});
    if (grown != 0u) count = grown;  // movn
    count >>= 31;                    // srl $s0, $s0, 31: the result is the sign bit
    release_counted(rt, ctx, second_block, frame.at(1u));
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    release_counted(rt, ctx, word(rt, frame.at(0x10u)), frame.at(0u));
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    ctx.gpr[2] = count;
}

// 0x08830BFC (a0 = a destination object, a1 = passed on, a2 = a kind, a3 = a holder): 0x088297A8(&local, a1) answers an
// object, 0x088294D0(object) a value that is kept, and the local is released; kind >= 2 takes the third virtual (+0x3C) of
// the holder's word +4 as a count (0x08A5BB7C reports a missing object), kind 1 leaves the count zero, and kind <= 0 skips
// the holder's word check. A holder with a word +0 tests 0x08847008(&other, holder), 0x08846E50(other) and passes that
// word plus 0 to 0x08860500(value, word, 0, count != 0); without one it passes 0 and 1. Then 0x08A85B1C(&local, flag) runs
// with 1 when 0x08860500 answered non-zero, the destination is assigned the local (0x08A5BAD8) and both are released.
void func_08830BFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 kind = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[21] = destination;         // s5
    ctx.gpr[19] = kind;                // s3
    ctx.gpr[17] = holder;              // s1
    call(rt, ctx, 0x088297A8u, {frame.at(8u), ctx.gpr[5]});
    const u32 object = word(rt, frame.at(8u));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 value = call(rt, ctx, 0x088294D0u, {object});
    ctx.gpr[20] = value;  // s4
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    u32 count = 0u;  // s2
    bool use_holder = true;
    if (kind >= 2) {
        const u32 source = word(rt, holder + 4u);
        if (source == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        count = call_indirect(rt, ctx, word(rt, word(rt, source) + 0x3Cu), {source});
    } else if (kind <= 0) {
        use_holder = false;
    }
    u32 result;
    if (use_holder && word(rt, holder) != 0u) {
        call(rt, ctx, 0x08847008u, {frame.at(4u), holder});
        const u32 other = word(rt, frame.at(4u));
        if (other == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A30Cu});
        const u32 word_argument = call(rt, ctx, 0x08846E50u, {other});
        result = call(rt, ctx, 0x08860500u, {value, word_argument, 0u, count != 0u ? 1u : 0u});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    } else {
        result = call(rt, ctx, 0x08860500u, {value, 0u, 1u, count != 0u ? 1u : 0u});
    }
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0u), result != 0u ? 1u : 0u});
    if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x0885C3C8 (a0 = a pointer to the object pointer): 0x08B344E0(a0 + 8, a1) runs first, then the object's fields +4, +8,
// +0xC (= 1), the bytes +0x14..+0x19 (= 0), +0x10 and +0x28/+0x2C (= -1) are set, the objects at +0x7C, +0x80, +0x84 and
// +0x88 are released (0x088D5AFC, 0x0889406C, 0x08893D08, 0x088D43E4), the helper at +0x78 is released through its first
// virtual (with its word +0) and freed (0x08B64654), the four records of this->word (0x188 bytes apart) are reset
// (0x0885D5E0), the words +0x1C and +0x20 are set to -1, and 0x08B3457C(a0 + 8) finishes (a tail call with v0 = word(a0) + 4).
void func_0885C3C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;       // s1
    ctx.gpr[19] = self + 8u;  // s3
    call(rt, ctx, 0x08B344E0u, {self + 8u, ctx.gpr[5]});
    u32 object = word(rt, self);
    put(rt, object + 4u, 0u);
    put(rt, object + 8u, 0u);
    put(rt, object + 0xCu, 1u);
    put_byte(rt, object + 0x14u, 0u);
    put_byte(rt, object + 0x16u, 0u);
    put_byte(rt, object + 0x15u, 0u);
    put_byte(rt, object + 0x17u, 0u);
    put_byte(rt, object + 0x18u, 0u);
    put(rt, object + 0x10u, 0xFFFFFFFFu);
    put_byte(rt, object + 0x19u, 0u);
    put(rt, object + 0x2Cu, 0xFFFFFFFFu);
    put(rt, object + 0x28u, 0xFFFFFFFFu);
    call(rt, ctx, 0x088D5AFCu, {word(rt, word(rt, self) + 0x7Cu)});
    call(rt, ctx, 0x0889406Cu, {word(rt, word(rt, self) + 0x80u)});
    call(rt, ctx, 0x08893D08u, {word(rt, word(rt, self) + 0x84u)});
    call(rt, ctx, 0x088D43E4u, {word(rt, word(rt, self) + 0x88u)});
    object = word(rt, self);
    const u32 helper = word(rt, object + 0x78u);
    put(rt, object + 0x78u, 0u);
    if (helper != 0u) {
        const u32 held = word(rt, helper + 4u);
        if (held != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, held)), {held, word(rt, helper)});
            put(rt, helper + 4u, 0u);
            put(rt, helper, 0u);
        }
        call(rt, ctx, 0x08B64654u, {helper});
        put(rt, word(rt, self) + 0x78u, 0u);
    }
    for (u32 offset = 0u; offset != 0x620u; offset += 0x188u) {
        call(rt, ctx, 0x0885D5E0u, {word(rt, word(rt, self)) + offset});
    }
    u32 address = 0u;
    for (u32 index = 0u; index != 2u; ++index) {
        address = word(rt, self) + index * 4u;
        put(rt, address + 0x1Cu, 0xFFFFFFFFu);
    }
    ctx.gpr[2] = address;  // the asm's last v0 is word(self) + 4
    tail_call(ctx, 0x08B3457Cu, {self + 8u});
}

// 0x08860FF8 (a0 = this, a1 = an index, a2 = a value, a3 = an argument): 0x088A79AC(sp, 0) fills a record and this->word +
// index * 0x50 + 0x4120 is the record. Two of the three 0x30-byte locals at sp + 0x40 are built (0x08897B8C), then
// 0x08866510(record + 0x44, record + 0x48, &local, sp, sp + 0x40) answers the entry byte at sp + 0x12 that this keeps as
// its index when it is larger than that one (else the index becomes the byte and the byte its successor); the entry at
// sp + 0x40 + index * 0x40 gets 0x08897470(entry, argument) and, unless the record's byte +0x18 is set, the record the
// halfword +0x1E selects from this->word + 0x3F50 (0x188 bytes apart) gets 0x0885D868(record, index, argument); sp + 0x11
// becomes 1 (the byte +0x11) and 0x088661B8 runs with both counts and the two locals; then 0x088978B4(sp + 0x70) and
// 0x088A7548(sp) finish.
void func_08860FF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xC0u);
    ctx.gpr[20] = self;                 // s4
    ctx.gpr[22] = frame.at(0xA0u);      // s6
    ctx.gpr[21] = argument;             // s5
    ctx.gpr[17] = value;                // s1
    ctx.gpr[19] = frame.at(0x40u);      // s3
    const u32 record = word(rt, self) + ctx.gpr[5] * 0x50u + 0x4120u;  // s2
    ctx.gpr[18] = record;
    call(rt, ctx, 0x088A79ACu, {frame.at(0u), 0u});
    u32 offset = 0x40u;
    for (;;) {
        ctx.gpr[16] = frame.at(offset);
        call(rt, ctx, 0x08897B8Cu, {frame.at(offset)});
        offset += 0x30u;
        ctx.gpr[16] = frame.at(offset);
        if (offset == 0xA0u) break;
    }
    call(rt, ctx, 0x08866510u, {word(rt, record + 0x44u), word(rt, record + 0x48u), frame.at(8u),
                                frame.at(0u), frame.at(0x40u)});
    const s32 highest = signed_byte_at(rt, frame.at(0x12u));
    u32 index = value;  // s1
    if (!(static_cast<s32>(value) < highest)) {
        put_byte(rt, frame.at(0x12u), static_cast<u8>(highest + 1));
        index = static_cast<u32>(highest);
    }
    const u32 entry = frame.at(0x40u) + index * 0x40u;
    call(rt, ctx, 0x08897470u, {entry, argument});
    if (byte_at(rt, record + 0x18u) == 0u) {
        const s32 selected = signed_half_at(rt, record + 0x1Eu);
        const u32 base = word(rt, word(rt, self) + 0x3F50u);
        call(rt, ctx, 0x0885D868u, {base + static_cast<u32>(selected * 0x188), index, argument});
    }
    put_byte(rt, frame.at(0x11u), 1u);
    call(rt, ctx, 0x088661B8u, {word(rt, record + 0x44u), word(rt, record + 0x48u), frame.at(8u), frame.at(0u),
                                static_cast<u32>(signed_byte_at(rt, frame.at(0x12u))), frame.at(0x40u)});
    call(rt, ctx, 0x088978B4u, {frame.at(0x70u)});
    call(rt, ctx, 0x088A7548u, {frame.at(0u)});
}

// 0x088D57A0 (a0 = a pointer to the destination pointer, a1 = the source): copies 0x60 bytes from a1 to the block word(a0)
// points at, 0x20 bytes from a1 + 0x60 to block + 0x60 (word by word; the original's lwl/lwr and swl/swr are its
// unaligned carries), then the two bytes a1 + 0x80 to block + 0x80; 0x088EF51C(block + 0x82) and 0x088D4800() finish.
// v0 = the source plus 0x88... 0x088D4800 answers 0x90, so v0 = a1 + 0x90, the end of what was copied.
void func_088D57A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = source;  // s0
    u32 destination = word(rt, holder);
    for (u32 offset = 0u; offset < 0x60u; offset += 4u) {
        put(rt, destination + offset, word(rt, source + offset));
    }
    destination = word(rt, holder);
    for (u32 offset = 0u; offset < 0x20u; offset += 4u) {
        put(rt, destination + 0x60u + offset, word(rt, source + 0x60u + offset));
    }
    put_byte(rt, destination + 0x80u, byte_at(rt, source + 0x80u));
    put_byte(rt, destination + 0x81u, byte_at(rt, source + 0x81u));
    call(rt, ctx, 0x088EF51Cu, {word(rt, holder) + 0x82u});
    const u32 length = call(rt, ctx, 0x088D4800u, {});
    ctx.gpr[2] = source + length;
}

// 0x08866B40 (a0 = this, a1 = a pointer to two words): 0x0890B3D8 and 0x0890B56C get the object at this + 0x98, which then
// gets 0x0890B3E0(object, word a1+4, 0, 1.0f); 0x08AF2640(this + 0x4524) and 0x08916C58(this + 0x45DC, word a1) run (its
// answer, 0 if none, is stored as (answer == 0 ? 0 : answer + 4) in the object's word +0x30); a 0x30-byte record built from
// the object's words +0x20, +8, +0x10, +0x18, +0x28, +0xE0, that count and this + 0x4524 is handed to
// 0x0888A0F4(this + 0x4584, &record, this + 0x45D4, word a1), and when the answer is non-zero also to
// 0x08917060(answer, &record, ...); a three-word record (this + 0x3F98, 0, this + 0x98) reaches the two virtuals at +0x30
// and +0x14 of this + 0x94.
void func_08866B40(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 words = ctx.gpr[5];
    const u32 first = word(rt, words);
    const u32 second = word(rt, words + 4u);
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x70u);
    ctx.gpr[20] = self;            // s4
    ctx.gpr[21] = self + 0x4524u;  // s5
    ctx.gpr[19] = self + 0x45D4u;  // s3
    ctx.gpr[18] = frame.at(0x14u); // s2, the record
    ctx.gpr[17] = first;           // s1
    call(rt, ctx, 0x0890B3D8u, {word(rt, self + 0x98u)});
    call(rt, ctx, 0x0890B56Cu, {word(rt, self + 0x98u)});
    call(rt, ctx, 0x0890B3E0u, {word(rt, self + 0x98u), second, 0u}, {1.0f});
    call(rt, ctx, 0x08AF2640u, {self + 0x4524u});
    const u32 answer = call(rt, ctx, 0x08916C58u, {self + 0x45DCu, first});
    ctx.gpr[16] = answer;
    const u32 count = answer == 0u ? 0u : answer + 4u;  // addiu/movz
    const u32 object = word(rt, self + 0x98u);
    put(rt, object + 0x30u, count);
    const u32 record = frame.at(0x14u);
    put(rt, frame.at(0x18u), word(rt, object + 0x20u));
    put(rt, frame.at(0x1Cu), word(rt, object + 8u));
    put(rt, frame.at(0x20u), word(rt, object + 0x10u));
    put(rt, frame.at(0x24u), word(rt, object + 0x18u));
    put(rt, frame.at(0x28u), word(rt, object + 0x28u));
    put(rt, frame.at(0x2Cu), count);
    put(rt, frame.at(0x38u), word(rt, object + 0xE0u));
    put(rt, frame.at(0x14u), self + 0x4524u);
    put(rt, frame.at(0x30u), 0u);
    put(rt, frame.at(0x34u), 0u);
    put_byte(rt, frame.at(0x3Cu), 0u);
    put(rt, frame.at(0x40u), 0u);
    call(rt, ctx, 0x0888A0F4u, {self + 0x4584u, record, self + 0x45D4u, first});
    if (answer != 0u) call(rt, ctx, 0x08917060u, {answer, record, self + 0x45D4u, first});
    const u32 state = word(rt, self + 0x98u);
    put(rt, state + 0x34u, self + 0x4524u);
    put(rt, frame.at(0u), word(rt, self + 0x3F98u));
    put(rt, frame.at(8u), word(rt, self + 0x98u));
    put(rt, frame.at(4u), 0u);
    const u32 listener = word(rt, self + 0x94u);
    call_indirect(rt, ctx, word(rt, word(rt, listener) + 0x30u), {listener, frame.at(0u)});
    const u32 other = word(rt, self + 0x94u);
    call_indirect(rt, ctx, word(rt, word(rt, other) + 0x14u), {other});
}

// 0x089CA118 (a0 = a destination object, a1 = a key, a2 = a mode, a3 = a holder): 0x089B395C(&local, 0x08CF6ED8) runs
// first; a mode of 2 or less takes the 0x30 virtual of the holder's word +4 as the count (mode 1 leaves it at 1, 0 does
// nothing); 0x089C5850(&other, key) answers a source that (with the holder's word 0) is assigned through its +0xC virtual
// into a local, and 0x089C9F24(source, &local, &assigned, count) runs; then the assigned block, the destination (when it is
// not the first local) and the other local are released.
void func_089CA118(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 kind = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[20] = frame.at(8u);    // s4
    ctx.gpr[21] = destination;     // s5
    ctx.gpr[16] = kind;            // s0
    ctx.gpr[19] = holder;          // s3
    call(rt, ctx, 0x089B395Cu, {frame.at(8u), 0x08CF6ED8u});
    if (static_cast<s32>(kind) <= 0) {
        call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        return;
    }
    ctx.gpr[23] = frame.at(0xCu);  // s7
    call(rt, ctx, 0x089C5850u, {frame.at(0xCu), key});
    u32 count = 1u;  // s6
    if (kind == 2u) {
        const u32 object = word(rt, holder + 4u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
        count = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x30u), {object});
    }
    const u32 source = word(rt, frame.at(0xCu));
    if (source == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA63A0u});
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {frame.at(4u), object});
    call(rt, ctx, 0x089C9F24u, {source, frame.at(8u), frame.at(4u), count});
    release_counted(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
    if (destination != frame.at(8u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(8u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
}

// One element of the 0xC-byte ranges 0x089DBCE8 destroys: `release(word, word + 4, marker)` and then 0x08B35254 for the
// element's first word when it is not null.
void destroy_element(Runtime &rt, AllegrexContext &ctx, u32 element, u32 marker, u32 release) {
    call(rt, ctx, release, {word(rt, element), word(rt, element + 4u), marker});
    const u32 held = word(rt, element);
    if (held != 0u) call(rt, ctx, 0x08B35254u, {held});
}

// 0x089DBCE8 and 0x089DBE58 (a0 = this, identical bodies): the vtable words 0x08CBC738 (at +0) and 0x08CBC970 (at +0x38)
// are stored, this + 0x13C and +0x138 are released (0x08A5B9EC), the counted block at this + 0x134 is freed, and three
// ranges of two 0xC-byte elements each — [this + 0x11C, this + 0x134) through 0x08C06684, [this + 0x104, this + 0x11C) and
// [this + 0xEC, this + 0x104) through 0x08BF0BF0 — are destroyed from their last element down; 0x0896BC0C(this) finishes.
void destroy_similar_object(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = self;  // s3
    put(rt, self, 0x08CBC738u);
    put(rt, self + 0x38u, 0x08CBC970u);
    call(rt, ctx, 0x08A5B9ECu, {self + 0x13Cu});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x138u});
    release_counted(rt, ctx, word(rt, self + 0x134u), frame.at(3u));
    for (u32 element = self + 0x128u;;) {
        destroy_element(rt, ctx, element, frame.at(2u), 0x08C06684u);
        if (element == self + 0x11Cu) break;
        element -= 0xCu;
    }
    for (u32 element = self + 0x110u;;) {
        destroy_element(rt, ctx, element, frame.at(1u), 0x08BF0BF0u);
        if (element == self + 0x104u) break;
        element -= 0xCu;
    }
    for (u32 element = self + 0xF8u;;) {
        destroy_element(rt, ctx, element, frame.at(0u), 0x08BF0BF0u);
        if (element == self + 0xECu) break;
        element -= 0xCu;
    }
    call(rt, ctx, 0x0896BC0Cu, {self});
}
void func_089DBCE8(Runtime &rt, AllegrexContext &ctx) { destroy_similar_object(rt, ctx); }
void func_089DBE58(Runtime &rt, AllegrexContext &ctx) { destroy_similar_object(rt, ctx); }

// 0x08B2D61C (a0 = an argument kept in s1, a1 and a2 = two words stored in the module's state 0x08D68D00): clears the
// state block (0x08D68D00, 0x40 bytes, through 0x08B36018) ands its words +0x40, +0x44, +0x48, +0x4C, +0x50, +0x54, +0x58
// and 0x08D68CF8, copies the a0 through 0x08B7C830(state, a0, 4), opens a channel (0x08C7AA18) whose handle goes to +0x48
// (a negative one answers 1), allocates 0x14 bytes for +0x44 (0x08B62344, set up by 0x08B2EE10 and 0x08B2EEB8), starts it
// (0x08B2D4E0: a negative answer releases the block and the handle and returns 3) and registers 0x03415001 on the handle
// (0x08C7A820(a0 = 0x08CB542C, ..., &handle, 4, 0, 0): a negative answer releases both after 0x08B2D45C and returns 2).
void func_08B2D61C(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kState = 0x08D68D00u;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = ctx.gpr[4];  // s1
    put(rt, kState + 0x54u, ctx.gpr[5]);
    put(rt, kState + 0x50u, ctx.gpr[6]);
    put(rt, kState + 0x58u, 0u);
    put(rt, kState + 0x40u, 0u);
    put_byte(rt, kState + 0x4Cu, 0u);
    put(rt, kState - 8u, 0u);
    put(rt, kState + 0x44u, 0u);
    put(rt, kState + 0x48u, 0u);
    ctx.gpr[16] = kState;  // s0
    call(rt, ctx, 0x08B36018u, {kState, 0u, 0x40u});
    call(rt, ctx, 0x08B7C830u, {kState, ctx.gpr[17], 4u});
    const u32 handle = call(rt, ctx, 0x08C7AA18u, {0x08CB5438u, 0x08B2D314u, 0x20u, 0x1000u, 0x200000u, 0u});
    put(rt, kState + 0x48u, handle);
    if (static_cast<s32>(handle) < 0) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 block = call(rt, ctx, 0x08B62344u, {0x14u});
    call(rt, ctx, 0x08B2EE10u, {block, 2u, 0x200u});
    put(rt, kState + 0x44u, block);
    call(rt, ctx, 0x08B2EEB8u, {block});
    if (static_cast<s32>(call(rt, ctx, 0x08B2D4E0u, {})) < 0) {
        call(rt, ctx, 0x08B2EE2Cu, {word(rt, kState + 0x44u)});
        (void)call(rt, ctx, 0x08C7AA00u, {word(rt, kState + 0x48u)});
        ctx.gpr[2] = 3u;
        return;
    }
    if (static_cast<s32>(call(rt, ctx, 0x08C7A820u,
                              {0x08CB542Cu, 0x03415001u, kState + 0x48u, 4u, 0u, 0u})) < 0) {
        call(rt, ctx, 0x08B2D45Cu, {});
        call(rt, ctx, 0x08B2EE2Cu, {word(rt, kState + 0x44u)});
        (void)call(rt, ctx, 0x08C7AA00u, {word(rt, kState + 0x48u)});
        ctx.gpr[2] = 2u;
        return;
    }
    ctx.gpr[2] = 0u;
}

// 0x08B48A74 (a0 = this, a1 = an argument of 0x08B0EC60, a2 = a count, a3 = a limit): with this's word +0 set the answer is
// 1; otherwise 0x08B0EC60(&record, a1, a2, a3) fills 0x3C bytes at sp + 0x80 that are copied to sp, the count becomes
// 0x08B35DA0(word sp+0x10) when a2 is zero (and this's byte +4 becomes 1) or the answer is 0 when the limit is smaller than
// that word; 0x08B100D0(&output, a1, &record-with-both-counts, 1) fills 0x3C bytes at sp + 0xC0 that are copied to sp + 0x40,
// a non-zero first word there answers 0, else 0x08B48A68(this, count) runs and the answer is 1.
void func_08B48A74(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 count_input = ctx.gpr[6];
    const u32 limit = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x120u);
    ctx.gpr[17] = self;
    ctx.gpr[21] = argument;
    ctx.gpr[19] = count_input;
    ctx.gpr[20] = limit;
    if (word(rt, self) != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    call(rt, ctx, 0x08B0EC60u, {frame.at(0x80u), argument, count_input, limit});
    for (u32 offset = 0u; offset < 0x3Cu; offset += 4u) {
        put(rt, frame.at(offset), word(rt, frame.at(0x80u + offset)));
    }
    const u32 count = word(rt, frame.at(0x10u));  // s2
    ctx.gpr[18] = count;
    u32 first = count_input;  // s3
    if (count_input == 0u) {
        first = call(rt, ctx, 0x08B35DA0u, {count});
        ctx.gpr[19] = first;
        put_byte(rt, self + 4u, 1u);
    } else if (static_cast<s32>(limit) < static_cast<s32>(count)) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, frame.at(4u), count);
    put(rt, frame.at(0u), first);
    call(rt, ctx, 0x08B100D0u, {frame.at(0xC0u), argument, frame.at(0u), 1u});
    for (u32 offset = 0u; offset < 0x3Cu; offset += 4u) {
        put(rt, frame.at(0x40u + offset), word(rt, frame.at(0xC0u + offset)));
    }
    if (word(rt, frame.at(0x40u)) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B48A68u, {self, first});
    ctx.gpr[2] = 1u;
}

// The free-list head 0x08BB2340 uses: the pool's word +0x34 while its word +0x10 is `size`, else 0x08BB1D0C(pool + 0x24,
// pool + 0x28, &size, 0x08BB1B40) answers a node whose word +0 is the new head (also stored back into pool + 0x34).
u32 pool_free_head(Runtime &rt, AllegrexContext &ctx, u32 pool, u32 size, u32 local) {
    put(rt, local, size);
    u32 head = word(rt, pool + 0x34u);
    if (head == 0u || word(rt, head + 0x10u) != size) {
        head = word(rt, call(rt, ctx, 0x08BB1D0Cu,
                             {word(rt, pool + 0x24u), word(rt, pool + 0x28u), local, 0x08BB1B40u}));
        put(rt, pool + 0x34u, head);
    }
    return head;
}
// The object goes to the head of the list at head + 0xC after the head's word +0x1C is decremented.
void link_into_pool(Runtime &rt, u32 head, u32 object) {
    const u32 next = word(rt, head + 0xCu);
    put(rt, head + 0x1Cu, word(rt, head + 0x1Cu) - 1u);
    put(rt, object, next);
    put(rt, head + 0xCu, object);
}

// 0x08BB2340 (a0 = begin, a1 = end, an array of object pointers): nothing when they are equal; for every non-null object
// the reference count at +0x10 is decremented and, when it reaches zero, the object's word +0xC goes into the first pool
// (0x08BB1F48, its head at +0x34 sized 4), the object's word +0xC is cleared and 0x08BB1FF0(object) runs, then the object
// itself is linked into the second pool (sized 0x14).
void func_08BB2340(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    if (begin == end) return;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[18] = begin;  // s2
    ctx.gpr[20] = end;    // s4
    u32 cursor = begin;
    do {
        const u32 object = word(rt, cursor);  // s0
        ctx.gpr[16] = object;
        cursor += 4u;
        ctx.gpr[18] = cursor;
        if (object != 0u) {
            const u32 count = word(rt, object + 0x10u) - 1u;
            put(rt, object + 0x10u, count);
            if (count == 0u) {
                const u32 first = word(rt, object + 0xCu);  // s3
                ctx.gpr[19] = first;
                const u32 pool = call(rt, ctx, 0x08BB1F48u, {});
                ctx.gpr[17] = pool;  // s1
                const u32 head = pool_free_head(rt, ctx, pool, 4u, frame.at(0u));
                if (first != 0u) link_into_pool(rt, head, first);
                put(rt, object + 0xCu, 0u);
                call(rt, ctx, 0x08BB1FF0u, {object});
                const u32 second_pool = call(rt, ctx, 0x08BB1F48u, {});
                ctx.gpr[17] = second_pool;
                const u32 second_head = pool_free_head(rt, ctx, second_pool, 0x14u, frame.at(0u));
                if (object != 0u) link_into_pool(rt, second_head, object);
            }
        }
    } while (cursor != end);
}

// 0x0899FD68 (a0 = this, a1 = an object): 0x08A36C3C(&local) answers an object that 0x08A34DA8 gets told is at
// 0x08CA4A14 (0x08A5BB7C reports when it is missing) and the local is released; when bit 4 of this + 0x13C is set, three
// float calls run (0x0899D06C with this + 0x150, then 0x0899D0D0 and 0x0899D108 with this + 0x120); bit 3 of the flags is
// cleared and its old bit 2 handed to 0x089AF5E8(a1, bit) and 0x089AB114(a1, 0); with this + 0x164 set, three locals are
// built (0x08A76AA0 twice, 0x08A76AAC with the a1), the second is assigned the third's word into a1 (0x08A5BAD8) and
// released, and the object's virtual +0xCC(second, 1, third) runs.
void func_0899FD68(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[17] = self;              // s1
    ctx.gpr[18] = argument;          // s2
    ctx.gpr[19] = frame.at(8u);      // s3
    ctx.gpr[20] = frame.at(0xCu);    // s4
    call(rt, ctx, 0x08A36C3Cu, {frame.at(8u)});
    ctx.gpr[16] = word(rt, frame.at(8u));  // s0
    if (ctx.gpr[16] == 0u) {
        call(rt, ctx, 0x08A5BB7Cu, {0x08CA4A0Cu});
        ctx.gpr[16] = 0u;
    }
    call(rt, ctx, 0x08A34DA8u, {ctx.gpr[16], 0x08CA4A14u});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    u32 flags = word(rt, self + 0x13Cu);
    if ((flags & 0x10u) != 0u) {
        call(rt, ctx, 0x0899D06Cu, {self}, {float_at(rt, self + 0x150u)});
        call(rt, ctx, 0x0899D0D0u, {self}, {float_at(rt, self + 0x120u)});
        call(rt, ctx, 0x0899D108u, {self}, {float_at(rt, self + 0x120u)});
        flags = word(rt, self + 0x13Cu);
    }
    const u32 cleared = flags & ~8u;
    put(rt, self + 0x13Cu, cleared);
    call(rt, ctx, 0x089AF5E8u, {argument, (cleared >> 2) & 1u});
    call(rt, ctx, 0x089AB114u, {argument, 0u});
    if (word(rt, self + 0x164u) != 0u) {
        const u32 first = frame.at(0xCu);   // s4
        const u32 second = frame.at(0x10u); // s3
        const u32 third = frame.at(4u);     // s0
        call(rt, ctx, 0x08A76AA0u, {first});
        call(rt, ctx, 0x08A76AA0u, {second});
        put(rt, frame.at(0u), argument);
        call(rt, ctx, 0x08A76AACu, {third, frame.at(0u)});
        if (second != third) call(rt, ctx, 0x08A5BAD8u, {second, word(rt, third)});
        call(rt, ctx, 0x08A5B9ECu, {third});
        const u32 object = word(rt, self + 0x164u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA485Cu});
        const u32 function = word(rt, word(rt, object) + 0xCCu);
        call_indirect(rt, ctx, function, {object, first, 1u, second});
        call(rt, ctx, 0x08A5B9ECu, {second});
        call(rt, ctx, 0x08A5B9ECu, {first});
    }
}

// 0x089D0BE0 (a0 = a destination object, a1 = passed on, a2 = a kind, a3 = a pointer to a pointer): 0x089CD62C(&local)
// runs first. Kind 0: the local's object, 0x089CD884(&local, object) and 0x08A6D2D4(&other, &local), then the other is
// assigned into a0 when it is not itself (0x08A5BAD8) and released; kind 1: a3's pointer's virtual +0xC(&local, pointer)
// and 0x089CD344(a2, &local). Both then release a counted block (`word + 8 - 0xC`): unless it is the shared empty block
// 0x08D74FD0 or 0x08B60804(block + 4, -1) answers a positive count, 0x08B8542C(block, &marker) frees it. v0 = the last
// release call's.
void func_089D0BE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 kind = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[18] = self;                    // s2
    ctx.gpr[19] = frame.at(0x10u);         // s3
    ctx.gpr[16] = kind;                    // s0
    ctx.gpr[17] = holder;                  // s1
    call(rt, ctx, 0x089CD62Cu, {frame.at(0x10u)});
    if (kind == 0u) {
        const u32 local = word(rt, frame.at(0x10u));
        if (local == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        call(rt, ctx, 0x089CD884u, {frame.at(8u), local});
        call(rt, ctx, 0x08A6D2D4u, {frame.at(0xCu), frame.at(8u)});
        if (self != frame.at(0xCu)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0xCu))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        release_counted_block(rt, ctx, frame.at(8u), frame.at(1u));
    } else if (kind == 1u) {
        const u32 object = word(rt, frame.at(0x10u));
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        const u32 pointer = word(rt, holder);
        if (pointer == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA697Cu});
        const u32 function = word(rt, word(rt, pointer) + 0xCu);
        call_indirect(rt, ctx, function, {frame.at(4u), pointer});
        call(rt, ctx, 0x089CD344u, {object, frame.at(4u)});
        release_counted_block(rt, ctx, frame.at(4u), frame.at(0u));
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
}

// The release 0x0887A28C ends with: the block at `pointer` (its base is pointer - 0xC) is skipped when it is the shared
// empty block 0x08D75298 or 0x08B60804(pointer - 4, -1) answers a positive count; otherwise 0x08B93554(base, marker) frees
// it.
void release_list_block(Runtime &rt, AllegrexContext &ctx, u32 pointer, u32 marker) {
    if (pointer - 0xCu == 0x08D75298u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B93554u, {pointer - 0xCu, marker});
}

// 0x0887A28C (a0 = this, a1 = passed on): 0x08AB85F0(&local, a1) then 0x08AB856C(&local) twice (the last answer is the
// count); with the local's word +0x10 positive, 0x08B62344(0x7C) answers a block that 0x0887FFBC gets with the count and
// this's word, 0x0886A9A0(block, 3) answers a record whose word +0xC becomes 0x24 and whose word +0x10 gets bits 0-1 set
// and bit 2 cleared, 0x0886C798(&described, this's word, count) and 0x0887F7C8(block, described) connect them,
// 0x0886ADEC(block, 1) and 0x0886B2E8(this's word, block) finish, and the described block is released; the local is
// released too.
void func_0887A28C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[19] = self;          // s3
    ctx.gpr[16] = frame.at(8u);  // s0
    call(rt, ctx, 0x08AB85F0u, {frame.at(8u), ctx.gpr[5]});
    call(rt, ctx, 0x08AB856Cu, {frame.at(8u)});
    const u32 count = call(rt, ctx, 0x08AB856Cu, {frame.at(8u)});
    ctx.gpr[20] = count;  // s4
    ctx.gpr[18] = count;  // s2
    if (static_cast<s32>(word(rt, frame.at(0x10u))) > 0) {
        const u32 owner = word(rt, self);
        ctx.gpr[16] = owner;
        const u32 block = call(rt, ctx, 0x08B62344u, {0x7Cu});
        ctx.gpr[17] = block;  // s1
        call(rt, ctx, 0x0887FFBCu, {block, count, owner});
        const u32 record = call(rt, ctx, 0x0886A9A0u, {block, 3u});
        if (record != 0u) {
            const u32 flags = word(rt, record + 0x10u);
            put(rt, record + 0xCu, 0x24u);
            put(rt, record + 0x10u, (flags | 3u) & ~4u);
        }
        call(rt, ctx, 0x0886C798u, {frame.at(4u), word(rt, self), count});
        call(rt, ctx, 0x0887F7C8u, {block, word(rt, frame.at(4u))});
        call(rt, ctx, 0x0886ADECu, {block, 1u});
        call(rt, ctx, 0x0886B2E8u, {word(rt, self), block});
        release_list_block(rt, ctx, word(rt, frame.at(4u)), frame.at(1u));
    }
    release_list_block(rt, ctx, word(rt, frame.at(8u)), frame.at(0u));
}

// 0x088D0B0C (a0 = this, a1 and a2 = two arguments): 0x088FAC80(&result, a2, a1, this's float +0) answers a float at sp + 4
// that answers nothing when it is negative (else it is stored at this + 0); 0x088FB000(a2) and 0x088FB144(a2, angle,
// this + 4, this + 0) answer a count, and a count of 0 (or a positive one whose 0x088FB028(a2) - 20 is above this + 4)
// skips to the last check, while a negative one or one within that window adds the count to this + 0x18, asks
// 0x088FB17C for this + 0x28, and when this + 0x14 is below the new + 0x18 (and this + 0x27 is clear) makes +0x14 the new
// total and sets byte +0x24. The last check stores this + 0 as this + 0xC while this + 0 is above this + 0xC and
// 0x088FB100(a2, 0, this + 0, this + 4) answers a positive value with +0x14 equal to +0x18.
void func_088D0B0C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = self;   // s0
    ctx.gpr[18] = first;  // s2
    call(rt, ctx, 0x088FAC80u, {frame.at(0u), first, second}, {float_at(rt, self)});
    const float value = float_at(rt, frame.at(4u));
    if (value < 0.0f) return;
    put_float(rt, self, value);
    const float angle = call(rt, ctx, 0x088FB000u, {first});
    const s32 count = static_cast<s32>(call(rt, ctx, 0x088FB144u, {first},
                                            {angle, float_at(rt, self + 4u), float_at(rt, self)}));
    ctx.gpr[17] = static_cast<u32>(count);  // s1
    bool advance = false;
    if (count > 0) {
        const float start = call(rt, ctx, 0x088FB028u, {first});
        advance = (start - 20.0f) <= float_at(rt, self + 4u);
    } else if (count < 0) {
        advance = true;
    }
    if (advance) {
        put(rt, self + 0x18u, word(rt, self + 0x18u) + static_cast<u32>(count));
        const float scaled = call(rt, ctx, 0x088FB000u, {first});
        const float measured = call(rt, ctx, 0x088FB17Cu, {first},
                                    {scaled, float_at(rt, self + 4u), float_at(rt, self)});
        const u32 total = word(rt, self + 0x18u);
        const u32 previous_angle = word(rt, self + 0x14u);
        put_float(rt, self + 0x28u, measured);
        ctx.gpr[2] = static_cast<s32>(previous_angle) < static_cast<s32>(total) ? 1u : 0u;
        if (static_cast<s32>(previous_angle) < static_cast<s32>(total)) {
            ctx.gpr[2] = byte_at(rt, self + 0x27u);
            if (ctx.gpr[2] == 0u) {
                put(rt, self + 0x14u, total);
                put_byte(rt, self + 0x24u, 1u);
                ctx.gpr[2] = 1u;
            }
            put(rt, self + 0xCu, 0u);
        }
    }
    if (float_at(rt, self) <= float_at(rt, self + 0xCu)) return;
    const float length = call(rt, ctx, 0x088FB100u, {first, 0u},
                             {float_at(rt, self), float_at(rt, self + 4u)});
    if (length <= 0.0f) return;
    const u32 previous = word(rt, self + 0x14u);
    const u32 total = word(rt, self + 0x18u);
    ctx.gpr[2] = total;
    if (previous != total) return;
    put_float(rt, self + 0xCu, float_at(rt, self));
}

// 0x088D0DC4 (a0 = this, a1 = passed on, a2 = a source, t0 = an object, a3 = a holder whose byte +0 says whether to look
// at all): with the byte set, this + 0x1C (its bits read as a fixed-point word) times 0x088FB028(t0) plus this + 0x20 is
// compared with this + 0x18 (likewise) times 0x088FB028(t0) plus this + 0; while the second is not below the first, each
// of the four slots (0x088D6218(a2 + 4, index)'s answer read as a fixed-point word) that is not negative gets
// 0x088FB144(t0, value, this + 4, this + 0) and, when that answers positively and the slot's word +0x184 of this is
// 0x481907FF, 0x088FB17C(t0, ...) with this's byte +0x5A set and 0x088D0C80(this, a3), whose answer becomes that word.
void func_088D0DC4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    const u32 object = ctx.gpr[8];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[16] = self;    // s0
    ctx.gpr[18] = object;  // s2
    ctx.gpr[21] = holder;  // s5
    if (byte_at(rt, holder) == 0u) return;
    float limit = static_cast<float>(std::bit_cast<s32>(float_at(rt, self + 0x1Cu)));
    const float first_scale = call(rt, ctx, 0x088FB028u, {object});
    limit = psp_mul(limit, first_scale);
    const float reference = static_cast<float>(std::bit_cast<s32>(float_at(rt, self + 0x18u)));
    limit = limit + float_at(rt, self + 0x20u);  // the second call's delay slot
    const float second_scale = call(rt, ctx, 0x088FB028u, {object});
    const float measured = reference * second_scale + float_at(rt, self);
    if (measured < limit) return;
    ctx.gpr[20] = source + 4u;  // s4
    for (u32 index = 0u; index != 4u; ++index) {
        ctx.gpr[19] = index;
        ctx.gpr[17] = self + index * 4u;  // s1
        const u32 raw = call(rt, ctx, 0x088D6218u, {source + 4u, index});
        const float value = static_cast<float>(std::bit_cast<s32>(raw));
        if (value < 0.0f) continue;
        const s32 answer = static_cast<s32>(call(rt, ctx, 0x088FB144u, {object},
                                                 {value, float_at(rt, self + 4u), float_at(rt, self)}));
        if (answer <= 0) continue;
        if (word(rt, self + index * 4u + 0x184u) != 0x481907FFu) continue;
        const float scaled = call(rt, ctx, 0x088FB17Cu, {object},
                                  {static_cast<float>(answer), float_at(rt, self + 4u), float_at(rt, self)});
        put_byte(rt, self + 0x5Au, 1u);
        const u32 next = call(rt, ctx, 0x088D0C80u, {self, holder}, {scaled});
        put(rt, self + index * 4u + 0x184u, next);
    }
}

// 0x088F9F00 (a0 = this): the object at this's word 0 holds two vectors of 4-byte elements: the first (words +0x10/+0x14)
// has each element's word released through 0x08B03A40(object + 0x1C, element) and the second (words +4/+8) each element's
// signed halfword through 0x08B03A04; 0x08BBB908(object + 0x10, begin, end) and 0x08BBB6F8(object + 4, begin, end) destroy
// the ranges, then both vectors' begin/end/capacity words are moved to the stack and cleared, and each old begin is freed
// (0x08B35254) after the range destructor ran.
void func_088F9F00(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[17] = self;  // s1
    u32 object = word(rt, self);
    u32 element = word(rt, object + 0x10u);
    if (element != word(rt, object + 0x14u)) {
        do {
            ctx.gpr[16] = element;  // s0
            call(rt, ctx, 0x08B03A40u, {word(rt, object + 0x1Cu), word(rt, element)});
            element += 4u;
            object = word(rt, self);
        } while (element != word(rt, object + 0x14u));
    }
    call(rt, ctx, 0x08BBB908u, {object + 0x10u, word(rt, object + 0x10u), word(rt, object + 0x14u)});
    object = word(rt, self);
    element = word(rt, object + 4u);
    if (element != word(rt, object + 8u)) {
        do {
            ctx.gpr[16] = element;
            call(rt, ctx, 0x08B03A04u, {word(rt, object + 0x1Cu), static_cast<u32>(signed_half_at(rt, element))});
            element += 4u;
            object = word(rt, self);
        } while (element != word(rt, object + 8u));
    }
    call(rt, ctx, 0x08BBB6F8u, {object + 4u, word(rt, object + 4u), word(rt, object + 8u)});
    object = word(rt, self);
    const u32 first_begin = word(rt, object + 0x10u);
    const u32 first_end = word(rt, object + 0x14u);
    const u32 first_capacity = word(rt, object + 0x18u);
    put(rt, frame.at(0x10u), first_begin);
    put(rt, object + 0x10u, 0u);
    put(rt, frame.at(0x14u), first_end);
    put(rt, object + 0x14u, 0u);
    put(rt, frame.at(0x18u), first_capacity);
    put(rt, object + 0x18u, 0u);
    call(rt, ctx, 0x08BBB838u, {first_begin, first_end, frame.at(1u)});
    if (first_begin != 0u) call(rt, ctx, 0x08B35254u, {first_begin});
    object = word(rt, self);
    const u32 second_begin = word(rt, object + 4u);
    const u32 second_end = word(rt, object + 8u);
    const u32 second_capacity = word(rt, object + 0xCu);
    put(rt, frame.at(4u), second_begin);
    put(rt, object + 4u, 0u);
    put(rt, frame.at(8u), second_end);
    put(rt, object + 8u, 0u);
    put(rt, frame.at(0xCu), second_capacity);
    put(rt, object + 0xCu, 0u);
    call(rt, ctx, 0x08BBBA34u, {second_begin, second_end, frame.at(0u)});
    if (second_begin != 0u) call(rt, ctx, 0x08B35254u, {second_begin});
}

// 0x089A54A0 (a0 = this, a1 = a key, a2 = an output): nothing when the output's first word is null or when
// 0x08BF63A8(this + 0x16C) is not this + 0x170; 0x08BF6308(this + 0x16C, key) answers the record: when a record came back,
// its block (word +0x10) and the key's are compared through 0x08B36170 (their counts' difference, the call's non-zero
// answer replacing it) and a non-negative difference skips the insertion — a negative one builds a local
// (0x08A5BAB8(&local, 0x08CF6ED8), 0x08B88F44(&other, key), 0x089FE2F0(&third, &local), 0x08BF76A8(this + 0x16C, record,
// &other)) whose other is released and whose third is released after its block. The record's word +0x14 is finally
// assigned the output's word unless that is the same address.
void func_089A54A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 output = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[16] = self;    // s0
    ctx.gpr[19] = key;     // s3
    ctx.gpr[20] = output;  // s4
    if (word(rt, output) == 0u) return;
    const u32 map = self + 0x16Cu;  // s2
    ctx.gpr[18] = map;
    if (self + 0x170u != call(rt, ctx, 0x08BF63A8u, {map})) return;
    const u32 found = call(rt, ctx, 0x08BF6308u, {map, key});
    ctx.gpr[22] = found;  // s6
    u32 member = found + 0x14u;  // s5
    if (self + 0x170u != found) {
        const u32 key_block = word(rt, key);
        const u32 key_count = word(rt, key_block - 0xCu);
        const u32 record_block = word(rt, found + 0x10u);
        const u32 record_count = word(rt, record_block - 0xCu);
        const u32 smaller = record_count < key_count ? record_count : key_count;
        ctx.gpr[21] = found;  // s5
        s32 difference = static_cast<s32>(key_count - record_count);
        const u32 grown = call(rt, ctx, 0x08B36170u, {key_block, record_block, smaller});
        if (grown != 0u) difference = static_cast<s32>(grown);
        if (difference < 0) {
            const u32 local = frame.at(4u);      // s5
            const u32 other = frame.at(0x10u);   // s1
            call(rt, ctx, 0x08A5BAB8u, {local, 0x08CF6ED8u});
            call(rt, ctx, 0x08B88F44u, {other, key});
            call(rt, ctx, 0x089FE2F0u, {frame.at(0x14u), local});
            const u32 inserted = call(rt, ctx, 0x08BF76A8u, {map, found, other});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
            release_counted(rt, ctx, word(rt, frame.at(0x10u)), frame.at(0u));
            call(rt, ctx, 0x08A5B9ECu, {local});
            member = inserted + 0x14u;
        }
    }
    if (output != member) call(rt, ctx, 0x08A5BAD8u, {member, word(rt, output)});
}

// 0x089AA1C0 (a0 = this, a1 = a row of 8-byte entries, a2 = a count, a3 = an argument, t0 = a holder, f12 = a scale):
// two short busy waits, then, when the holder's word 0 is set, 0x089E4BC4(word, &local, 0) and this's byte +0x118 becomes
// 1 while its word +0x11C takes the holder's word (the local at sp + 0x30 is the block's address, else 0); the virtual
// +0x150 of this runs, then for each of the a2 - 1 rows (8 bytes apart) two entries are built at sp + 0 and sp + 8
// (0x08B4AC54(entry, row + 8 * column, a3), except the first column of any row but the first, which is copied from the
// previous row's second entry) and 0x089A9E58(this, sp, block, scale) is called; the virtual +0x154 of this finishes.
void func_089AA1C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 rows = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    const u32 holder = ctx.gpr[8];
    const float scale = ctx.fpr[12];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x80u);
    ctx.gpr[23] = self;      // s7
    ctx.gpr[22] = argument;  // s6
    ctx.gpr[19] = rows;      // s3
    ctx.gpr[18] = count;     // s2
    ctx.gpr[17] = holder;    // s1
    // 089AA210 and 089AA250 count $v0 down (3 -> -2, 2 -> -1) doing nothing; v0 is overwritten before it is read
    put(rt, frame.at(0x30u), 0u);
    if (word(rt, holder) != 0u) {
        call(rt, ctx, 0x089E4BC4u, {word(rt, holder), frame.at(0x10u), 0u});
        put_byte(rt, self + 0x118u, 1u);
        put(rt, frame.at(0x30u), frame.at(0x10u));
        put(rt, self + 0x11Cu, word(rt, holder));
    }
    const u32 limit = count - 1u;  // fp
    ctx.gpr[30] = limit;
    call_indirect(rt, ctx, word(rt, word(rt, self) + 0x150u), {self});
    if (static_cast<s32>(limit) > 0) {
        u32 row = rows;  // s4
        u32 index = 0u;  // s3
        do {
            ctx.gpr[20] = row;
            for (u32 column = 0u; column != 2u; ++column) {
                const u32 local = frame.at(column * 8u);
                if (index != 0u && column == 0u) {
                    const float first = float_at(rt, frame.at(8u));
                    const float second = float_at(rt, frame.at(0xCu));
                    put_float(rt, frame.at(0u), first);
                    put_float(rt, frame.at(4u), second);
                } else {
                    call(rt, ctx, 0x08B4AC54u, {local, row + column * 8u, argument});
                }
            }
            call(rt, ctx, 0x089A9E58u, {self, frame.at(0u), word(rt, frame.at(0x30u))}, {scale});
            ++index;
            row += 8u;
        } while (limit != index);
    }
    call_indirect(rt, ctx, word(rt, word(rt, self) + 0x154u), {self});
}

// 0x08A816C4 (a0 = this, a1 = a holder): for every node of the circular list at this + 0x20 (until this + 0x18) a 4-word
// block from 0x08A5BC20(8, 4) is described at sp + 0xC, its two intrusive pointers (words +0 and +4) are assigned the
// node's words +0x10 and +0x14 (0x08A5BAD8), 0x08A76AA0(&local) and the holder's virtual +0xCC(object, &local, 2, block)
// run, the node is advanced through 0x08B5F100(node), the local is released and the described block handed back through
// 0x08B856B0(begin, end, sp) and freed (0x08A5BC54, as many bytes as the description holds).
void func_08A816C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[21] = holder;        // s5
    ctx.gpr[20] = self + 0x18u;  // s4
    ctx.gpr[19] = frame.at(4u);  // s3
    ctx.gpr[18] = frame.at(8u);  // s2
    u32 node = word(rt, self + 0x20u);  // s1
    ctx.gpr[17] = node;
    while (node != self + 0x18u) {
        put(rt, frame.at(0xCu), 0u);
        put(rt, frame.at(0x10u), 0u);
        put(rt, frame.at(0x14u), 0u);
        const u32 block = call(rt, ctx, 0x08A5BC20u, {8u, 4u});
        ctx.gpr[16] = block;
        put(rt, frame.at(0xCu), block);
        put(rt, frame.at(0x10u), block);
        put(rt, frame.at(0x14u), block + 8u);
        call(rt, ctx, 0x08A76AA0u, {frame.at(4u)});
        call(rt, ctx, 0x08B85AF4u, {block, 2u, frame.at(4u), frame.at(1u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        u32 target = word(rt, frame.at(0xCu));
        ctx.gpr[16] = target;
        if (target != node + 0x10u) {
            call(rt, ctx, 0x08A5BAD8u, {target, word(rt, node + 0x10u)});
            target = word(rt, frame.at(0xCu));
        }
        if (target + 4u != node + 0x14u) call(rt, ctx, 0x08A5BAD8u, {target + 4u, word(rt, node + 0x14u)});
        call(rt, ctx, 0x08A76AA0u, {frame.at(8u)});
        const u32 object = word(rt, holder);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCCu),
                      {object, frame.at(8u), 2u, word(rt, frame.at(0xCu))});
        node = call(rt, ctx, 0x08B5F100u, {node});
        ctx.gpr[17] = node;
        call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        call(rt, ctx, 0x08B856B0u, {word(rt, frame.at(0xCu)), word(rt, frame.at(0x10u)), frame.at(0u)});
        const u32 begin = word(rt, frame.at(0xCu));
        const u32 size = static_cast<u32>(static_cast<s32>(word(rt, frame.at(0x14u)) - begin) >> 2) << 2;
        if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, size});
    }
}

// 0x08AC9B54 (a0 = this, a1 = a pointer to a 64-bit key): 0x08AC99A4(key) answers a record whose +0x28 is the answer when
// it is found; otherwise 0x08AC99F4(key) answers a new one, 0x08B62344(0x14) a block that 0x08AD5590 sets up and
// 0x08AD5524(block, key) fills: failing, the block is released through its virtual +4 and 0x08AC9240(record) runs with 0;
// succeeding, 0x08AD542C(block) and 0x08ADC988(block + 0x10, &local) answer an entry, the block is appended to the record's
// list at +0x30 (0x08C3D12C when it is full), 0x08AC963C(record, block + 0x10, &local, entry, 0, 0) runs and the record
// gets this at +0x28 and itself at +0x2C. v0 = record + 0x28, or 0.
void func_08AC9B54(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x830u);
    ctx.gpr[19] = self;      // s3
    ctx.gpr[17] = argument;  // s1
    const u32 value = word(rt, argument);
    const u32 existing = call(rt, ctx, 0x08AC99A4u, {value, value >> 31});  // sra $a1, $a1, 31
    ctx.gpr[16] = existing;
    if (existing != 0u) {
        ctx.gpr[2] = existing + 0x28u;
        return;
    }
    const u32 key = word(rt, argument);
    const u32 record = call(rt, ctx, 0x08AC99F4u, {key, key >> 31});
    ctx.gpr[16] = record;  // s0
    const u32 block = call(rt, ctx, 0x08B62344u, {0x14u});
    ctx.gpr[18] = block;   // s2
    call(rt, ctx, 0x08AD5590u, {block});
    if (call(rt, ctx, 0x08AD5524u, {block, argument}) == 0u) {
        if (block != 0u) call_indirect(rt, ctx, word(rt, word(rt, block) + 4u), {block});
        call(rt, ctx, 0x08AC9240u, {record});
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08AD542Cu, {block});
    ctx.gpr[20] = word(rt, block + 0x10u);  // s4
    ctx.gpr[21] = frame.at(4u);             // s5
    const u32 entry = call(rt, ctx, 0x08ADC988u, {word(rt, block + 0x10u), frame.at(4u)});
    ctx.gpr[17] = entry;  // s1
    put(rt, frame.at(0u), block);
    const u32 list = record + 0x30u;
    if (word(rt, list + 4u) == word(rt, list + 8u)) {
        call(rt, ctx, 0x08C3D12Cu, {list, word(rt, list + 4u), frame.at(0u)});
    } else {
        u32 end = word(rt, list + 4u);
        if (end == 0u) {
            end = 4u;  // `beql $a1, $zero` runs `$a1 += 4` and skips the store
        } else {
            put(rt, end, block);
            end = word(rt, list + 4u) + 4u;
        }
        put(rt, list + 4u, end);
    }
    call(rt, ctx, 0x08AC963Cu, {record, word(rt, block + 0x10u), frame.at(4u), entry, 0u, 0u});
    if (record != 0u) {
        put(rt, record + 0x28u, self);
        put(rt, record + 0x2Cu, record);
        ctx.gpr[2] = record + 0x28u;
        return;
    }
    ctx.gpr[2] = 0u;
}

// 0x088619A8 (a0 = this, a1 = a name kept aside, a2 = a name, a3 = an argument, t0 = a pointer that is optional): the
// objects at this's word + 0x3F88 and + 0x3F90 are looked up (0x08894098, 0x08893C40), a state at sp is opened
// (0x088D441C) and given t0 when it is set (0x088D44BC); 0x08B0979C() and 0x08B0980C(object) answer a manager whose
// 0x08B08A0C(manager, 0) count (stored at sp + 0x7C when positive) and byte (0x08B08A58, stored at sp + 0x80) are
// recorded, 0x088618E8(this's word, 0) runs and 0x088D4824(word + 0x5AC + 4, a1) when a1 is set; 0x088DFA68(word + 0x5AC, sp,
// 1) opens a session that, when non-null, marks this's byte +0x5A9 and sets the object at word +0x5B8's +0x30 (this), +0x2C,
// +0x24, +0x20 and +0x28 (0x08860B3C, 0x0885F550, 0x08861FD0, 0x08860C2C) before 0x0885E7DC(this) runs. The state is
// closed with 0x088D435C(sp) and v0 = the session.
void func_088619A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];    // s3
    const u32 name = ctx.gpr[6];
    const u32 third = ctx.gpr[7];    // s0
    const u32 fourth = ctx.gpr[8];   // s1
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xB0u);
    ctx.gpr[18] = self;
    ctx.gpr[19] = first;
    ctx.gpr[17] = fourth;
    ctx.gpr[16] = third;
    call(rt, ctx, 0x08894098u, {word(rt, self) + 0x3F88u, name});
    call(rt, ctx, 0x08893C40u, {word(rt, self) + 0x3F90u, third});
    call(rt, ctx, 0x088D441Cu, {frame.at(0u)});
    if (fourth != 0u) call(rt, ctx, 0x088D44BCu, {frame.at(0u), fourth});
    const u32 manager = call(rt, ctx, 0x08B0980Cu, {call(rt, ctx, 0x08B0979Cu, {})});
    ctx.gpr[16] = manager;
    if (static_cast<s32>(call(rt, ctx, 0x08B08A0Cu, {manager, 0u})) > 0) {
        put(rt, frame.at(0x7Cu), call(rt, ctx, 0x08B08A0Cu, {manager, 0u}));
    }
    put_byte(rt, frame.at(0x80u), static_cast<u8>(call(rt, ctx, 0x08B08A58u, {manager})));
    call(rt, ctx, 0x088618E8u, {word(rt, self), 0u});
    if (first != 0u) call(rt, ctx, 0x088D4824u, {word(rt, word(rt, self) + 0x5ACu) + 4u, first});
    const u32 session = call(rt, ctx, 0x088DFA68u, {word(rt, word(rt, self) + 0x5ACu), frame.at(0u), 1u});
    ctx.gpr[16] = session;
    if (session != 0u) {
        put_byte(rt, word(rt, self) + 0x5A9u, 1u);
        const u32 target = word(rt, word(rt, self) + 0x5B8u);
        put(rt, target + 0x30u, self);
        put(rt, target + 0x2Cu, 0x08860B3Cu);
        put(rt, target + 0x24u, 0x0885F550u);
        put(rt, target + 0x20u, 0x08861FD0u);
        put(rt, target + 0x28u, 0x08860C2Cu);
        call(rt, ctx, 0x0885E7DCu, {self});
    }
    call(rt, ctx, 0x088D435Cu, {frame.at(0u)});
    ctx.gpr[2] = session;
}

// 0x08869514 (a0 = this): the state at this + 0x90 is reset (0x0890B56C, 0x0890B3D8) and, when 0x08869E84(this + 0x1B0,
// 0) answers non-zero, 0x08869EC0(this + 0x1B0) and 0x08869ECC(this + 0x1B0, 0) answer a count that 0x0890B3E0 takes with 1.0
// and that lands in the state's word +0x30; a non-zero count also reaches 0x0890B00C(count, state + 0x20, state + 8, count,
// state + 0xFC != 0). Then 0x08869ECC(this + 0x1B0, 0) answers what a 0x30-byte record at sp holds (this + 0x1A0, state's
// words +0x20, +8, +0x10, +0x18, +0x28, that answer, 0, 0, state + 0xE0, a zero byte, 0) that 0x089098B8 takes; the object at
// this + 0x1C0 gets its virtuals +0x30(&record) and +0x14(), and 0x08B344E0(this) / 0x08B3457C(this) bracket them, with a
// final +0x30(object, 0).
void func_08869514(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 state = self + 0x90u;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[19] = self;            // s3
    ctx.gpr[18] = state;           // s2
    ctx.gpr[20] = self + 0x1B0u;   // s4
    call(rt, ctx, 0x0890B56Cu, {state});
    call(rt, ctx, 0x0890B3D8u, {state});
    if (call(rt, ctx, 0x08869E84u, {self + 0x1B0u, 0u}) != 0u) {
        call(rt, ctx, 0x08869EC0u, {self + 0x1B0u});
        const u32 count = call(rt, ctx, 0x08869ECCu, {self + 0x1B0u, 0u});
        ctx.gpr[16] = count;  // s0
        ctx.gpr[17] = count;  // s1
        call(rt, ctx, 0x0890B3E0u, {state, count, 0u}, {1.0f});
        put(rt, state + 0x30u, count);
        if (count != 0u) {
            call(rt, ctx, 0x0890B00Cu,
                 {count, word(rt, state + 0x20u), word(rt, state + 8u), count,
                  word(rt, state + 0xFCu) != 0u ? 1u : 0u});
        }
    }
    const u32 result = call(rt, ctx, 0x08869ECCu, {self + 0x1B0u, 0u});
    put(rt, frame.at(0u), self + 0x1A0u);
    put(rt, frame.at(4u), word(rt, state + 0x20u));
    put(rt, frame.at(8u), word(rt, state + 8u));
    put(rt, frame.at(0xCu), word(rt, state + 0x10u));
    put(rt, frame.at(0x10u), word(rt, state + 0x18u));
    put(rt, frame.at(0x14u), word(rt, state + 0x28u));
    put(rt, frame.at(0x18u), result);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    put(rt, frame.at(0x24u), word(rt, state + 0xE0u));
    put_byte(rt, frame.at(0x28u), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    call(rt, ctx, 0x089098B8u, {frame.at(0u), 0u});
    const u32 listener = word(rt, self + 0x1C0u);
    call_indirect(rt, ctx, word(rt, word(rt, listener) + 0x30u), {listener, frame.at(0u)});
    call(rt, ctx, 0x08B344E0u, {self});
    const u32 other = word(rt, self + 0x1C0u);
    call_indirect(rt, ctx, word(rt, word(rt, other) + 0x14u), {other});
    call(rt, ctx, 0x08B3457Cu, {self});
    const u32 last = word(rt, self + 0x1C0u);
    call_indirect(rt, ctx, word(rt, word(rt, last) + 0x30u), {last, 0u});
}

// 0x088B0AA0 (a0 = a string object, a1 = a source that may be null): the object's word 0 becomes the shared empty string
// 0x08D752A4 (and nothing else runs when the source is null); otherwise 0x088B04F4(&local, source) and
// 0x08B957D8(object, &local) build the string, and the local's block is released. Then, refreshing the object's word after
// every 0x08BAFF30(object) call (the word -4's sign says one is needed), a leading '/' skips the '~' check that would else
// run 0x08BAFD74(object, 0, 0x08CA0A24, 10); while the last character (0x08B99E48(object, length - 1, 0) shortens the
// string) is '/' or ' ' the object is trimmed, and any other character answers the object itself.
void func_088B0AA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;        // s1
    ctx.gpr[18] = 0x08D75298u; // s2
    put(rt, self, 0x08D752A4u);
    if (source == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    call(rt, ctx, 0x088B04F4u, {frame.at(4u), source});
    call(rt, ctx, 0x08B957D8u, {self, frame.at(4u)});
    release_list_block(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
    u32 text = word(rt, self);
    if (static_cast<s32>(word(rt, text - 4u)) >= 0) {
        call(rt, ctx, 0x08BAFF30u, {self});
        text = word(rt, self);
    }
    if (signed_byte_at(rt, text) != '/') {
        if (static_cast<s32>(word(rt, text - 4u)) >= 0) {
            call(rt, ctx, 0x08BAFF30u, {self});
            text = word(rt, self);
        }
        if (signed_byte_at(rt, text) != '~') {
            call(rt, ctx, 0x08BAFD74u, {self, 0u, 0x08CA0A24u, 0xAu});
            text = word(rt, self);
        }
    }
    u32 last = word(rt, text - 0xCu) - 1u;  // s0
    if (static_cast<s32>(word(rt, text - 4u)) >= 0) {
        call(rt, ctx, 0x08BAFF30u, {self});
        text = word(rt, self);
    }
    for (;;) {
        const s8 character = signed_byte_at(rt, text + last);
        if (character != '/') {
            if (character != ' ') {
                ctx.gpr[2] = self;
                return;
            }
        }
        call(rt, ctx, 0x08B99E48u, {self, word(rt, text - 0xCu) - 1u, 0u});
        text = word(rt, self);
        last = word(rt, text - 0xCu) - 1u;
        if (static_cast<s32>(word(rt, text - 4u)) >= 0) {
            call(rt, ctx, 0x08BAFF30u, {self});
            text = word(rt, self);
        }
    }
}

// 0x08AE1D6C (a0 = an argument the callback gets, a1 = a limit kept aside, a2 = an end position, a3 = a table, t0 = a
// count, t1 = a start position, t2 = a length, t3 = a flags word that may be null): when both a1 and a2 are zero they
// become -1; then while the position is below the length, the span is the rest (capped by a2 - start when a2 is set),
// every table entry (0xC bytes apart, its word +4) at or after t1 and before start + span is reported through
// 0x08AE0CD8(a0, index) while the table, the span and the count are all positive, and when a2 was -1 and the span reaches
// it the start is either the a1 limit (with bit 8 set in the t3 word) or the span's end. v0 = the final start.
void func_08AE1D6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 callback_argument = ctx.gpr[4];
    u32 from = ctx.gpr[5];
    u32 end = ctx.gpr[6];
    const u32 table = ctx.gpr[7];
    const u32 count = ctx.gpr[8];
    u32 start = ctx.gpr[9];
    const u32 length = ctx.gpr[10];
    const u32 flags = ctx.gpr[11];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[30] = count;   // fp
    ctx.gpr[22] = length;  // s6
    ctx.gpr[21] = end;     // s5
    ctx.gpr[20] = start;   // s4
    if ((from | end) == 0u) {
        from = 0xFFFFFFFFu;
        end = 0xFFFFFFFFu;
    }
    if (static_cast<s32>(length) > 0) {
        u32 position = 0u;  // s7
        const u32 endIsUnset = end == 0xFFFFFFFFu ? 1u : 0u;  // nor + srl 31
        const u32 hasTable = table != 0u ? 1u : 0u;           // sltu zero, table
        const u32 hasCount = static_cast<s32>(count) > 0 ? 1u : 0u;
        do {
            u32 span = length - position;
            if (static_cast<s32>(end) >= 0) {
                const u32 room = end - start;
                if (static_cast<s32>(room) < static_cast<s32>(span)) span = room;  // min
            }
            const u32 span_end = start + span;  // s3
            if (hasTable != 0u && static_cast<s32>(span) > 0 && hasCount != 0u) {
                for (u32 index = 0u; static_cast<s32>(index) < static_cast<s32>(count); ++index) {
                    const u32 value = word(rt, table + 4u + index * 0xCu);
                    if (static_cast<s32>(value) >= static_cast<s32>(start) &&
                        static_cast<s32>(value) < static_cast<s32>(span_end)) {
                        call(rt, ctx, 0x08AE0CD8u, {callback_argument, index});
                    }
                }
            }
            u32 next = span_end;
            if (endIsUnset != 0u && static_cast<s32>(span_end) >= static_cast<s32>(end) && flags != 0u) {
                put(rt, flags, word(rt, flags) | 0x100u);
                next = from;
            }
            start = next;
            ctx.gpr[20] = start;
            position += span;
            if (!(static_cast<s32>(position) < static_cast<s32>(length))) break;
        } while (true);
    }
    ctx.gpr[2] = start;
}

// 0x08AE6268 (a0 = this, a1 = an argument): the node at this + 0x18 (when its word +8 is negative) walks its chain through
// word +4: for each node, this + 0x10's float +0xFC scaled by 0x08CB6130 is truncated and taken off the node's word +0x4C,
// the node's bytes +0x84..+0x89 take the state's bytes +0xE4..+0xE7 split into their high and low nibbles, 0x08D40974 is
// set to 1 while the node's virtual +0x28(this) runs, then 0x08AE8658(node's halfword +0x1A)'s bit 6 sets this's word +0xC
// bit 6, and when the node's byte +0x55 is set with bit 1 clear 0x08AE34A8(node) runs. v0 = the byte / that call's answer,
// or 0 at the end of a node whose byte is clear.
void func_08AE6268(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = self;
    ctx.gpr[21] = argument;
    ctx.gpr[23] = 0x08CB6130u;
    u32 node = word(rt, self + 0x18u);
    const u32 selected = static_cast<s32>(word(rt, node + 8u)) < 0 ? 1u : 0u;
    if (selected == 0u) node = 0u;
    ctx.gpr[17] = node;
    if (node == 0u) {
        ctx.gpr[2] = selected;
        return;
    }
    u32 state = word(rt, self + 0x10u);
    ctx.gpr[18] = state;
    while (node != 0u) {
        const float scaled = psp_mul(float_at(rt, state + 0xFCu), float_at(rt, 0x08CB6130u));
        u32 next = word(rt, node + 4u);
        if (static_cast<s32>(word(rt, next + 8u)) >= 0) next = 0u;
        ctx.gpr[20] = next;
        put(rt, node + 0x4Cu, word(rt, node + 0x4Cu) - static_cast<u32>(trunc_w_s(scaled)));
        put_byte(rt, node + 0x84u, byte_at(rt, state + 0xE4u) & 0xF0u);
        put_byte(rt, node + 0x85u, byte_at(rt, state + 0xE5u) & 0xF0u);
        put_byte(rt, node + 0x86u, byte_at(rt, state + 0xE4u) & 0x0Fu);
        put_byte(rt, node + 0x87u, byte_at(rt, state + 0xE5u) & 0x0Fu);
        put_byte(rt, node + 0x88u, byte_at(rt, state + 0xE6u));
        put_byte(rt, node + 0x89u, byte_at(rt, state + 0xE7u));
        put(rt, 0x08D40974u, 1u);
        call_indirect(rt, ctx, word(rt, node + 0x28u), {self});
        put(rt, 0x08D40974u, 0u);
        const u32 flags = call(rt, ctx, 0x08AE8658u, {static_cast<u32>(signed_half_at(rt, node + 0x1Au))});
        if ((flags & 0x40u) != 0u) put(rt, self + 0xCu, word(rt, self + 0xCu) | 0x40u);
        ctx.gpr[2] = byte_at(rt, node + 0x55u);
        if (byte_at(rt, node + 0x55u) != 0u && (flags & 2u) == 0u) {
            call(rt, ctx, 0x08AE34A8u, {node});
        }
        node = next;
        ctx.gpr[17] = node;
        if (node == 0u) break;
        state = word(rt, self + 0x10u);
        ctx.gpr[18] = state;
    }
}

// 0x08B8D5D8 (a0 = first, a1 = last, a2 = a needle, a3 = the needle's end, t0 = a predicate(position, needle)): the first
// position in [first, last) where the needle matches under the predicate (it returns non-zero on a match). A one-character
// needle is a plain scan; a longer one scans for the first character and, when the rest fails, retries from the position
// after that match. v0 = the position found, or last.
void func_08B8D5D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    const u32 needle = ctx.gpr[6];
    const u32 needle_end = ctx.gpr[7];
    const u32 predicate = ctx.gpr[8];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[16] = first;       // s0
    ctx.gpr[19] = last;        // s3
    ctx.gpr[18] = needle;      // s2
    ctx.gpr[21] = needle_end;  // s5
    ctx.gpr[20] = predicate;   // s4
    if (first == last || needle == needle_end) {
        ctx.gpr[2] = first;
        return;
    }
    if (needle_end == needle + 1u) {
        u32 position = first;  // s0
        ctx.gpr[17] = last;    // s1
        while (position != last) {
            if (call_indirect(rt, ctx, predicate, {position, needle}) != 0u) break;
            ++position;
        }
        ctx.gpr[2] = position;
        return;
    }
    u32 candidate = first;  // s0
    for (;;) {
        while (candidate != last && call_indirect(rt, ctx, predicate, {candidate, needle}) == 0u) ++candidate;
        if (candidate == last) break;
        if (call_indirect(rt, ctx, predicate, {candidate, needle}) == 0u) {  // 08B8D680 tests the first character again
            ++candidate;
            continue;
        }
        if (last == candidate) break;
        const u32 after = candidate + 1u;
        if (last == after) {
            candidate = last;
            break;
        }
        const u32 stop = (needle_end - after) + after - 1u;
        u32 position = after;
        u32 character = needle + 1u;
        bool matched = true;
        for (;;) {
            if (call_indirect(rt, ctx, predicate, {position, character}) == 0u) {
                matched = false;
                break;
            }
            if (position == stop) break;
            ++character;
            ++position;
            if (position == last) {
                ctx.gpr[2] = last;
                return;
            }
        }
        if (!matched) {
            candidate = after;
            continue;
        }
        ctx.gpr[2] = candidate;
        return;
    }
    ctx.gpr[2] = candidate;
}

// 0x08C4B5C8 (a0 = this, a1 = a 3-float vector, a2 = a 3x3 matrix): the matrix times the vector, row after row
// (`v0 * m[0] + v1 * m[1] + v2 * m[2]`, the first product added to zero), stored into this. The original has two copies
// because this may be the matrix itself: it reads every element first. v0 is the first result's word when this is the
// matrix (the original loads it back from the stack), untouched otherwise.
void func_08C4B5C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 vector = ctx.gpr[5];
    const u32 matrix = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    float elements[9];
    for (unsigned index = 0u; index < 9u; ++index) elements[index] = float_at(rt, matrix + index * 4u);
    const float x = float_at(rt, vector);
    const float y = float_at(rt, vector + 4u);
    const float z = float_at(rt, vector + 8u);
    float result[3];
    for (unsigned row = 0u; row < 3u; ++row) {
        result[row] = psp_mul(x, elements[row * 3u]) + 0.0f;
        result[row] = result[row] + psp_mul(y, elements[row * 3u + 1u]);
        result[row] = result[row] + psp_mul(z, elements[row * 3u + 2u]);
    }
    for (unsigned row = 0u; row < 3u; ++row) put_float(rt, self + row * 4u, result[row]);
    if (self == matrix) {
        ctx.gpr[2] = std::bit_cast<u32>(result[0]);
        ctx.gpr[3] = std::bit_cast<u32>(result[2]);
    }
}

// 0x088726B8 (a0 = this, a1 = a position, a2 = three floats, f12 = an angle that is negated first): a 4x4 matrix at sp
// (a translation of the negated angle at +8, ones on its diagonal) is built with 0x08B4E648(matrix, a1) and a second at
// sp + 0x60 (ones on its diagonal, the three floats in its last column) with 0x08B4E9D4(matrix, &second); the three
// rotations 0x08B4F3B0(matrix, a2 + 4), 0x08B4F2E4(matrix, a2), 0x08B4F47C(matrix, a2 + 8) and
// 0x08B4A774(&first, matrix, &first) compose them, then the same rotations and 0x08B4A774(&second, matrix, &second) do it
// again; 0x08AF43B4(this + 0xC, a1), 0x08AF43D0(this + 0xC, &first) and 0x08AF43EC(this + 0xC, &second) finish.
void func_088726B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 angles = ctx.gpr[6];
    const float value = -ctx.fpr[12];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xD0u);
    ctx.gpr[18] = self + 0xCu;      // s2
    ctx.gpr[19] = argument;         // s3
    ctx.gpr[17] = angles;           // s1
    ctx.gpr[16] = frame.at(0x20u);  // s0, the matrix
    ctx.gpr[20] = frame.at(0x10u);  // s4, the second matrix
    put_float(rt, frame.at(8u), value);
    put_float(rt, frame.at(0xCu), 1.0f);
    put_float(rt, frame.at(0x14u), 1.0f);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    call(rt, ctx, 0x08B4E648u, {frame.at(0x20u), argument});
    put_float(rt, frame.at(0x90u), float_at(rt, angles));
    put_float(rt, frame.at(0x94u), float_at(rt, angles + 4u));
    put_float(rt, frame.at(0x98u), float_at(rt, angles + 8u));
    put_float(rt, frame.at(0x9Cu), 1.0f);
    put_float(rt, frame.at(0x60u), 1.0f);
    put_float(rt, frame.at(0x74u), 1.0f);
    put_float(rt, frame.at(0x88u), 1.0f);
    put(rt, frame.at(0x70u), 0u);
    put(rt, frame.at(0x80u), 0u);
    put(rt, frame.at(0x64u), 0u);
    put(rt, frame.at(0x84u), 0u);
    put(rt, frame.at(0x68u), 0u);
    put(rt, frame.at(0x78u), 0u);
    put(rt, frame.at(0x6Cu), 0u);
    put(rt, frame.at(0x7Cu), 0u);
    put(rt, frame.at(0x8Cu), 0u);
    call(rt, ctx, 0x08B4E9D4u, {frame.at(0x20u), frame.at(0x60u)});
    call(rt, ctx, 0x08B4F3B0u, {frame.at(0x20u)}, {float_at(rt, angles + 4u)});
    call(rt, ctx, 0x08B4F2E4u, {frame.at(0x20u)}, {float_at(rt, angles)});
    call(rt, ctx, 0x08B4F47Cu, {frame.at(0x20u)}, {float_at(rt, angles + 8u)});
    call(rt, ctx, 0x08B4A774u, {frame.at(0u), frame.at(0x20u), frame.at(0u)});
    call(rt, ctx, 0x08B4E648u, {frame.at(0x20u), frame.at(0x20u)});
    call(rt, ctx, 0x08B4F3B0u, {frame.at(0x20u)}, {float_at(rt, angles + 4u)});
    call(rt, ctx, 0x08B4F2E4u, {frame.at(0x20u)}, {float_at(rt, angles)});
    call(rt, ctx, 0x08B4F47Cu, {frame.at(0x20u)}, {float_at(rt, angles + 8u)});
    call(rt, ctx, 0x08B4A774u, {frame.at(0x10u), frame.at(0x20u), frame.at(0x10u)});
    call(rt, ctx, 0x08AF43B4u, {self + 0xCu, argument});
    call(rt, ctx, 0x08AF43D0u, {self + 0xCu, frame.at(0u)});
    call(rt, ctx, 0x08AF43ECu, {self + 0xCu, frame.at(0x10u)});
}

// 0x088C896C (a0 = this, a1 = a 3-float point): the point moved 20 up is looked up through
// 0x088D2D10(this's word +4, &result, &point); found, the point becomes the result's first three floats and the three
// floats at result + 0x1C (0x4C..0x54) are normalized (0x08B4A720, then 0x08B4A5E8 with 1 / sqrt) and scaled by 0.04 into
// the local at sp + 0xC, else the local at sp is (0, 1, 0) normalized and scaled the same way; 0x08B4AA34(point, point,
// &local) applies it.
void func_088C896C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 point = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xA0u);
    ctx.gpr[19] = frame.at(0xCu);   // s3
    ctx.gpr[18] = frame.at(0x24u);  // s2
    ctx.gpr[16] = point;            // s0
    put(rt, frame.at(0x64u), 0xFFFFFFFFu);
    put_float(rt, frame.at(0x18u), float_at(rt, point));
    put_float(rt, frame.at(0x1Cu), float_at(rt, point + 4u) + 20.0f);
    put_float(rt, frame.at(0x20u), float_at(rt, point + 8u));
    put(rt, frame.at(0x60u), 0u);
    u32 local = frame.at(0xCu);
    if (call(rt, ctx, 0x088D2D10u, {word(rt, self + 4u), frame.at(0x30u), frame.at(0x18u)}) != 0u) {
        put_float(rt, point, float_at(rt, frame.at(0x30u)));
        put_float(rt, point + 4u, float_at(rt, frame.at(0x34u)));
        put_float(rt, point + 8u, float_at(rt, frame.at(0x38u)));
        put_float(rt, frame.at(0x24u), float_at(rt, frame.at(0x4Cu)));
        put_float(rt, frame.at(0x28u), float_at(rt, frame.at(0x50u)));
        put_float(rt, frame.at(0x2Cu), float_at(rt, frame.at(0x54u)));
    } else {
        local = frame.at(0u);
        put(rt, frame.at(0x24u), 0u);
        put_float(rt, frame.at(0x28u), 1.0f);
        put(rt, frame.at(0x2Cu), 0u);
    }
    const u32 buffer = frame.at(0x24u);  // s2
    const float length = std::sqrt(call(rt, ctx, 0x08B4A720u, {buffer}));
    call(rt, ctx, 0x08B4A5E8u, {buffer}, {1.0f / length});
    put_float(rt, local, float_at(rt, frame.at(0x24u)));
    put_float(rt, local + 4u, float_at(rt, frame.at(0x28u)));
    put_float(rt, local + 8u, float_at(rt, frame.at(0x2Cu)));
    call(rt, ctx, 0x08B4AA34u, {point, point, local}, {0.04f});
}

// 0x0890FC28 (a0 = a destination kept in s2, a1 = an object, a2 unused, a3 = a source object, f12 = a scale): the source's
// virtual +0xC(source, 0, 0) answers a node that, when its +8 virtual answers a positive count, is replaced by the answer
// of its +0xC virtual; 0x0890FAD0(a0, a1, node, scale) runs, then the node's +0x60 virtual decides 0x08AB8774(a1, 2 or 0);
// the node's +0x74 virtual answers another node whose +0x18 virtual (2.0) or, failing, whose +0x1C (20.0 when set, 1.5) or
// +0x20 (30.0 and 1.0 when clear, else 1.5) settle the scale; 0x08AB8970(a1, a0's byte +0x28, scale) runs and
// 0x08AB8768(a1, that scale) finishes (a tail call).
void func_0890FC28(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];   // s2
    const u32 second = ctx.gpr[5];  // s1
    const u32 source = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = first;
    ctx.gpr[17] = second;
    float scale = ctx.fpr[12];
    u32 node = call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {source, 0u, 0u});
    ctx.gpr[16] = node;
    if (node != 0u) {
        if (static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, node) + 8u), {node})) > 0) {
            node = call_indirect(rt, ctx, word(rt, word(rt, node) + 0xCu), {node});
        } else {
            node = 0u;
        }
    }
    ctx.gpr[16] = node;
    call(rt, ctx, 0x0890FAD0u, {first, second, node}, {scale});
    const s8 index = signed_byte_at(rt, first + 0x28u);
    const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, node) + 0x60u), {node});
    call(rt, ctx, 0x08AB8774u, {second, answer == 1u ? 2u : 0u});
    const u32 next = call_indirect(rt, ctx, word(rt, word(rt, node) + 0x74u), {node});
    ctx.gpr[16] = next;
    ctx.fpr[12] = 2.0f;
    scale = 20.0f;
    if (call_indirect(rt, ctx, word(rt, word(rt, next) + 0x18u), {next}) == 0u) {
        scale = 30.0f;
        if (call_indirect(rt, ctx, word(rt, word(rt, next) + 0x1Cu), {next}) != 0u) {
            scale = 20.0f;
            ctx.fpr[12] = 1.5f;
        } else {
            const u32 third = call_indirect(rt, ctx, word(rt, word(rt, next) + 0x20u), {next});
            ctx.fpr[12] = 1.0f;
            if (third != 0u) {
                scale = 20.0f;
                ctx.fpr[12] = 1.5f;
            }
        }
    }
    call(rt, ctx, 0x08AB8970u, {second, static_cast<u32>(index)}, {ctx.fpr[12]});
    ctx.fpr[12] = scale;
    tail_call(ctx, 0x08AB8768u, {second});
}

// 0x089B8F38 (a0 = this, a1 = a list node, a2 = a holder): 0x08A5BA28(node, node) runs and the node's word +0x48, when
// set, goes through 0x089B8BAC(node + 0x48, node); this's word +4 gets its virtual +0x18C(node, holder) while
// 0x08B344EC(0x08CF4C2C) holds a lock, the node is spliced between holder's neighbours (word +0x50 and +0x54: the two ends
// are rewired, this's words +0xC and +0x10 take the node when they are missing, and the node's +0x50/+0x54 take the
// holder's, which are cleared) with 0x089BA6CC(node, this) and 0x089BA6CC(holder, 0) on either side;
// 0x08B34584(0x08CF4C2C) releases the lock, 0x08A7DE18(this's word +4, 0x08A7BCC0(holder)) registers, 0x08A5B970(holder)
// and 0x08A7BDB0(node) run, a local description (0x08B85EB8(&local, 0x08A7BDB0(node), &marker)) is handed to
// word(0x08A7BDB0(node) + 0x108), the local's block is released and this's word +4 gets virtual +0x188(node).
void func_089B8F38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 node = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[20] = self;    // s4
    ctx.gpr[19] = node;    // s3
    ctx.gpr[18] = holder;  // s2
    call(rt, ctx, 0x08A5BA28u, {node, node});
    if (word(rt, node + 0x48u) != 0u) call(rt, ctx, 0x089B8BACu, {word(rt, node + 0x48u), node});
    const u32 manager = word(rt, self + 4u);
    call_indirect(rt, ctx, word(rt, word(rt, manager) + 0x18Cu), {manager, holder});
    call(rt, ctx, 0x08B344ECu, {0x08CF4C2Cu});
    const u32 next = word(rt, holder + 0x50u);
    if (next != 0u) {
        put(rt, next + 0x54u, node);
    } else {
        put(rt, self + 0xCu, node);
    }
    const u32 previous = word(rt, holder + 0x54u);
    if (previous != 0u) {
        put(rt, previous + 0x50u, node);
    } else {
        put(rt, self + 0x10u, node);
    }
    put(rt, node + 0x54u, word(rt, holder + 0x54u));
    call(rt, ctx, 0x089BA6CCu, {node, self});
    put(rt, node + 0x50u, word(rt, holder + 0x50u));
    put(rt, holder + 0x50u, 0u);
    call(rt, ctx, 0x089BA6CCu, {holder, 0u});
    put(rt, holder + 0x54u, 0u);
    call(rt, ctx, 0x08B34584u, {0x08CF4C2Cu});
    const u32 object = word(rt, self + 4u);
    const u32 described = call(rt, ctx, 0x08A7BCC0u, {holder});
    call(rt, ctx, 0x08A7DE18u, {object, described});
    call(rt, ctx, 0x08A5B970u, {holder});
    const u32 registry = call(rt, ctx, 0x08A7BDB0u, {node});
    const u32 function = word(rt, registry + 0x108u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), registry, frame.at(1u)});
    call_indirect(rt, ctx, function, {node, frame.at(4u)});
    release_counted(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
    const u32 other = word(rt, self + 4u);
    call_indirect(rt, ctx, word(rt, word(rt, other) + 0x188u), {other, node});
}

// 0x08ABEF54 (a0 = this): the vtable word 0x08CD6390 is stored, 0x08ABF6C0(this + 0x144) and 0x08B28DC4(this + 0x11C) run,
// and the 16 entries this + 0x110 / +0x114 / +0x118 (0x10 bytes apart, downwards from the highest) whose +0x118 word is set
// are destroyed: the +0x110 word cleared, the array of 0x20-byte elements at +0x114 (their count in its word -4) each
// released through its first virtual from the last element down and its storage freed (0x08B61FC0), the +0x118 block too,
// and both words cleared. The vtable word 0x08CD63D8 is stored last.
void destroy_entry_array(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    put(rt, self, 0x08CD6390u);
    call(rt, ctx, 0x08ABF6C0u, {self + 0x144u});
    call(rt, ctx, 0x08B28DC4u, {self + 0x11Cu});
    for (u32 index = 0u; index != 16u; ++index) {
        const u32 third = self + 0x110u - index * 0x10u;
        const u32 second = self + 0x114u - index * 0x10u;
        const u32 first = self + 0x118u - index * 0x10u;
        if (word(rt, first) == 0u) continue;
        put(rt, third, 0u);
        const u32 elements = word(rt, second);
        if (elements != 0u) {
            const u32 end = elements + (word(rt, elements - 4u) << 5);
            if (elements != end) {
                u32 element = end - 0x20u;
                do {
                    const u32 destroyed = element;
                    call_indirect(rt, ctx, word(rt, word(rt, element)), {element});
                    element -= 0x20u;
                    (void)destroyed;
                } while (word(rt, second) != element + 0x20u);
            }
            call(rt, ctx, 0x08B61FC0u, {elements - 4u});
        }
        if (word(rt, first) != 0u) call(rt, ctx, 0x08B61FC0u, {word(rt, first)});
        put(rt, second, 0u);
        put(rt, first, 0u);
    }
    put(rt, self, 0x08CD63D8u);
}
void func_08ABEF54(Runtime &rt, AllegrexContext &ctx) { destroy_entry_array(rt, ctx); }
void func_08ABF0D0(Runtime &rt, AllegrexContext &ctx) { destroy_entry_array(rt, ctx); }
// 0x08ABF410: the same body, then 0x08B64654(this) frees the object itself (a tail call).
void func_08ABF410(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    destroy_entry_array(rt, ctx);
    tail_call(ctx, 0x08B64654u, {self});
}

// 0x08B65728 (a0 = this, a1 = an index, a2 = a flag kept as a byte): a table of 8-byte entries starts inline at this + 0x24
// (8 of them); an index below 8 answers its slot there, a larger one reallocates (0x08B6461C((index + 1) * 8), the old
// entries copied, the old block freed with 0x08B61FC0 unless it is the inline one) and answers slot `index` of the new
// block. The index 0x7FFFFFFF marks this's word +0x14 (bit 0) and, unless word +0x10 shares that bit (0x08B5AA30 asserts),
// clears the word +0x1C (or +0x20 when the flag is set) and answers its address.
void func_08B65728(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[20] = self + 0x24u;  // s4, the inline table
    ctx.gpr[21] = flag;          // s5
    ctx.gpr[19] = 8u;            // s3
    ctx.gpr[18] = self + 0x24u;  // s2
    ctx.gpr[17] = self;          // s1
    ctx.gpr[16] = index;         // s0
    if (static_cast<s32>(index) >= 8) {
        if (index == 0x7FFFFFFFu) {
            const u32 value = word(rt, self + 0x14u) | 1u;
            const u32 shared = value & word(rt, self + 0x10u);
            put(rt, self + 0x14u, value);
            if (shared != 0u) {
                call(rt, ctx, 0x08B5AA30u, {0x08CB5BA8u});  // the original ends here: the report does not return
                return;
            }
            if (flag != 0u) {
                put(rt, self + 0x20u, 0u);
            } else {
                put(rt, self + 0x1Cu, 0u);
            }
            ctx.gpr[2] = self + 0x1Cu;
            return;
        }
        const u32 count = index + 1u;  // s3
        ctx.gpr[19] = count;
        const u32 block = call(rt, ctx, 0x08B6461Cu, {count << 3});
        ctx.gpr[18] = block;  // s2
        if (index != 0xFFFFFFFFu) {
            u32 slot = block;
            for (u32 i = 0u; i != count; ++i) {
                put(rt, slot, 0u);
                put(rt, slot + 4u, 0u);
                slot += 8u;
            }
        }
        const s32 stored = static_cast<s32>(word(rt, self + 0x64u));  // t0
        const u32 entries = word(rt, self + 0x68u);                   // t1
        if (stored > 0) {
            u32 target = block;
            u32 source = entries;
            for (u32 i = 0u; i != static_cast<u32>(stored); ++i) {
                put(rt, target, word(rt, source));
                put(rt, target + 4u, word(rt, source + 4u));
                target += 8u;
                source += 8u;
            }
        }
        if (entries != 0u) {
            if (entries != self + 0x24u) {
                call(rt, ctx, 0x08B61FC0u, {entries});
                put(rt, self + 0x68u, 0u);
            }
            put(rt, self + 0x64u, count);
        } else {
            put(rt, self + 0x64u, count);
        }
    } else {
        put(rt, self + 0x64u, 8u);
    }
    put(rt, self + 0x68u, ctx.gpr[18]);
    ctx.gpr[2] = ctx.gpr[18] + index * 8u;
}

// 0x08865FF4 (a0 = this): nothing when this's word +8 or its byte +0x18 is clear; with the byte +0x19 set,
// 0x0892BB10() answers a state whose words +0x20 and +0x28 are taken, and bit 7 of (the state's +0x20 except this's
// +0x1C) selects whether 0x088DE9CC(this's word +8) and 0x088D9CCC answer a positive flag (else the byte +0x24); without
// it the same two calls answer that flag and this's +0x1C/+0x20 are cleared. The flag then runs 0x088DA0BC(this's word +8)
// and, when that answers non-zero, 0x08865F7C(this, the other flag) or 0x088D7744(this's word +8, flag) and, when this's
// word +0x10 is set, 0x088936A4(word, &record of the flag and the 0x088DE9CC answer).
void func_08865FF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xC0u);
    ctx.gpr[16] = self;  // s0
    u32 object = word(rt, self + 8u);  // v1
    bool flag;                         // s1
    u32 kind = 0u;                     // s3
    if (word(rt, self + 8u) == 0u) return;
    if (byte_at(rt, self + 0x18u) == 0u) return;
    if (word(rt, self + 4u) == 0u) return;
    if (byte_at(rt, self + 0x19u) != 0u) {
        const u32 state = call(rt, ctx, 0x0892BB10u, {});
        const u32 value = word(rt, state + 0x20u);  // s2
        const u32 other = word(rt, state + 0x28u);  // s4
        ctx.gpr[18] = value;
        ctx.gpr[20] = other;
        flag = ((value & (value ^ word(rt, self + 0x1Cu))) >> 7) & 1u;
        if (flag) {
            if (call(rt, ctx, 0x088DE9CCu, {word(rt, self + 8u)}) != 0u) {
                kind = byte_at(rt, self + 0x24u) == 0u ? 1u : 0u;
            } else if (call(rt, ctx, 0x088D9CCCu, {word(rt, self + 8u)}) != 0u) {
                kind = 0u;
            } else {
                kind = 1u;
            }
        } else {
            kind = 0u;
        }
        put(rt, self + 0x1Cu, value);
        put(rt, self + 0x20u, other);
    } else {
        if (call(rt, ctx, 0x088DE9CCu, {object}) != 0u) {
            flag = byte_at(rt, self + 0x24u) != 0u;
        } else if (call(rt, ctx, 0x088D9CCCu, {word(rt, self + 8u)}) != 0u) {
            flag = true;
        } else {
            flag = false;
        }
        put(rt, self + 0x1Cu, 0u);
        put(rt, self + 0x20u, 0u);
    }
    ctx.gpr[19] = kind;
    if (!flag) return;
    if (call(rt, ctx, 0x088DA0BCu, {word(rt, self + 8u)}) == 0u) return;
    if (call(rt, ctx, 0x088DE9CCu, {word(rt, self + 8u)}) != 0u) {
        call(rt, ctx, 0x08865F7Cu, {self, kind});
    } else {
        call(rt, ctx, 0x088D7744u, {word(rt, self + 8u), kind});
    }
    if (word(rt, self + 0x10u) == 0u) return;
    put_byte(rt, frame.at(8u), static_cast<u8>(kind));
    const u32 answer = call(rt, ctx, 0x088DE9CCu, {word(rt, self + 8u)});
    put(rt, frame.at(0u), 0u);
    put_byte(rt, frame.at(9u), static_cast<u8>(answer));
    call(rt, ctx, 0x088936A4u, {word(rt, self + 0x10u), frame.at(0u)});
}

// 0x08891F04 (a0 = this): nothing when its word 0 is clear (v0 = 0); 0x088F8BAC(word + 0x94, 0, 0) and
// 0x0888F53C(this + 8, this's two words) answer a count, and 0x0888FBF8(this + 8, the six words +0x8C..+0x9C, count, -1
// when this's byte +0x10 is clear else 0) runs; a buffer of ((count * 4) + 0x1E) with the low four bits cleared is taken
// from the stack and filled by 0x088F8BAC(word + 0x94, buffer, count), then for every entry 0x08890C5C(this + 8, index),
// 0x08890B20(this + 8, index) and 0x08896040(entry, answer, answer) run; the object at word +0x94 gets its virtual +0x1C,
// this's byte +0xA0 becomes 1 and 0x088910B8(this + 0x14) runs. v0 = 1 (0 when there was nothing to do).
void func_08891F04(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[30] = ctx.gpr[29];  // fp = sp
    ctx.gpr[18] = self;
    u32 result = 0u;
    if (word(rt, self) != 0u) {
        call(rt, ctx, 0x088F8BACu, {word(rt, self + 0x94u), 0u, 0u});
        const u32 count = call(rt, ctx, 0x0888F53Cu, {self + 8u, word(rt, self), word(rt, self + 4u)});
        ctx.gpr[22] = count;      // s6
        ctx.gpr[21] = self + 8u;  // s5
        const u32 last = byte_at(rt, self + 0x10u) != 0u ? 0u : 0xFFFFFFFFu;
        call(rt, ctx, 0x0888FBF8u, {self + 8u, word(rt, self + 0x8Cu), word(rt, self + 0x90u), word(rt, self + 0x94u),
                                    word(rt, self + 0x98u), word(rt, self + 0x9Cu), count, last});
        const u32 size = ((count << 2) + 0x1Eu) & ~15u;
        {
            GuestFrame array(ctx, size);
            const u32 base = array.at(0u);
            call(rt, ctx, 0x088F8BACu, {word(rt, self + 0x94u), base, count});
            if (static_cast<s32>(count) > 0) {
                u32 element = base;
                for (u32 index = 0u; index != count; ++index) {
                    const u32 first = word(rt, element);
                    ctx.gpr[19] = element;
                    call(rt, ctx, 0x08890C5Cu, {self + 8u, index});
                    const u32 second = call(rt, ctx, 0x08890B20u, {self + 8u, index});
                    ctx.gpr[16] = second;
                    call(rt, ctx, 0x08896040u, {first, second, second});
                    element += 4u;
                }
            }
        }
        const u32 listener = word(rt, self + 0x94u);
        call_indirect(rt, ctx, word(rt, word(rt, listener) + 0x1Cu), {listener});
        put_byte(rt, self + 0xA0u, 1u);
        call(rt, ctx, 0x088910B8u, {self + 0x14u});
        result = 1u;
    }
    ctx.gpr[2] = result;
}

// 0x08A9C1C0 (a0 = a destination object, a1 = a source object, a2 and a3 = pointers to two objects, t0, t1 and t2 =
// three arguments): 0x08A76AA0(a0) runs first; with the object at (a2's object) + 0x30 holding a non-empty list (+0x1C to
// +0x20, 4-byte entries), the object at a3 is checked (0x08A7BDBC) and, when that answers 0, gets
// 0x08A7BE18(object, a1 + 0x34); 0x08A90BF4(object at a2, object at a3) runs, 0x08A62AB8(&local) answers a block that
// 0x08A64008(&record, block, a2, t0, t1, t2) takes, the destination is assigned the record (0x08A5BAD8) and both the record
// and the local are released. v0 = the destination.
void func_08A9C1C0(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    const u32 fifth = ctx.gpr[8];
    const u32 sixth = ctx.gpr[9];
    const u32 seventh = ctx.gpr[10];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[19] = destination;  // s3
    ctx.gpr[20] = second;       // s4
    ctx.gpr[17] = third;        // s1
    ctx.gpr[18] = fourth;       // s2
    ctx.gpr[21] = fifth;        // s5
    ctx.gpr[22] = sixth;        // s6
    ctx.gpr[23] = seventh;      // s7
    call(rt, ctx, 0x08A76AA0u, {destination});
    if (word(rt, third) == 0u) {
        ctx.gpr[2] = destination;
        return;
    }
    const u32 holder = word(rt, word(rt, third) + 0x30u);  // s0
    ctx.gpr[16] = holder;
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2580u});
    if ((word(rt, holder + 0x20u) - word(rt, holder + 0x1Cu)) >> 2 == 0u) {
        ctx.gpr[2] = destination;
        return;
    }
    const u32 object = word(rt, fourth);
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    if (call(rt, ctx, 0x08A7BDBCu, {object}) == 0u) {
        const u32 other = word(rt, fourth);
        ctx.gpr[16] = other;
        if (other == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
        call(rt, ctx, 0x08A7BE18u, {other, word(rt, second + 0x34u)});
    }
    const u32 first_object = word(rt, third);
    ctx.gpr[16] = first_object;
    if (first_object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2588u});
    call(rt, ctx, 0x08A90BF4u, {first_object, word(rt, fourth)});
    call(rt, ctx, 0x08A62AB8u, {frame.at(4u)});
    const u32 block = word(rt, frame.at(4u));
    ctx.gpr[16] = block;
    if (block == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2590u});
    call(rt, ctx, 0x08A64008u, {frame.at(0u), block, third, fifth, sixth, seventh});
    if (destination != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[2] = destination;
}

// 0x08AF5164 (a0 = this, a1 = a flag kept as a byte): when this's byte +0x426 is set and the flag is clear, the objects at
// this + 0x1A8 and this + 0x27C get their virtual +0x14; when the byte is clear and the flag set, both get 0x08AFAD8C with
// (0x1E0, 0x110, 3, 0, 0), their virtual +0x18 runs, this's virtual +0x3C(0, 0) and 0x08AF5040(this, 0, 0, the 1440 x 272
// viewport: 0, 0, 1440, 272, 0, 0, 1, 1) run, then this + 0x27C's +0x18 and this + 0xD4's +0x3C(0, 0) with
// 0x08AF5040(this + 0xD4, ...) the same. 0x08B344E0(0x08D470C4) runs, this's byte +0x427 takes the flag and
// 0x08B3457C(0x08D470C4) finishes (a tail call).
void func_08AF5164(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = flag;  // s2
    ctx.gpr[17] = self;  // s1
    if (byte_at(rt, self + 0x426u) != 0u) {
        if (flag == 0u) {
            const u32 first = word(rt, self + 0x1A8u);
            call_indirect(rt, ctx, word(rt, first + 0x14u), {self + 0x1A8u});
            const u32 second = word(rt, self + 0x27Cu);
            call_indirect(rt, ctx, word(rt, second + 0x14u), {self + 0x27Cu});
        }
    } else if (flag != 0u) {
        call(rt, ctx, 0x08AFAD8Cu, {self + 0x1A8u, 0x1E0u, 0x110u, 3u, 0u, 0u});
        call(rt, ctx, 0x08AFAD8Cu, {self + 0x27Cu, 0x1E0u, 0x110u, 3u, 0u, 0u});
        const u32 first = word(rt, self + 0x1A8u);
        call_indirect(rt, ctx, word(rt, first + 0x18u), {self + 0x1A8u});
        call_indirect(rt, ctx, word(rt, word(rt, self) + 0x3Cu), {self, 0u, 0u});
        call(rt, ctx, 0x08AF5040u, {self, 0u, 0u}, {0.0f, 0.0f, 1440.0f, 272.0f, 0.0f, 0.0f, 1.0f, 1.0f});
        const u32 second = word(rt, self + 0x27Cu);
        call_indirect(rt, ctx, word(rt, second + 0x18u), {self + 0x27Cu});
        const u32 third = word(rt, self + 0xD4u);
        call_indirect(rt, ctx, word(rt, third + 0x3Cu), {self + 0xD4u, 0u, 0u});
        call(rt, ctx, 0x08AF5040u, {self + 0xD4u, 0u, 0u}, {0.0f, 0.0f, 1440.0f, 272.0f, 0.0f, 0.0f, 1.0f, 1.0f});
    }
    call(rt, ctx, 0x08B344E0u, {0x08D470C4u});
    put_byte(rt, self + 0x427u, static_cast<u8>(flag));
    tail_call(ctx, 0x08B3457Cu, {0x08D470C4u});
}

// 0x08B0BB3C (a0 = this, a1 = passed on, a2 = an object): when this's byte +8 is set, 0x08B0C9AC(object, a1, &header,
// this + 0x10) with the header {1, this + 0xC, this + 0x10} runs, else 0x08B0C99C(object, a1, 0, 0); a non-zero word +4 of
// the object ends it with 0. Two 16-byte reads (0x08B0C650) then check the size and the byte-swapped word 0x33627067;
// failing, 0x08B0C778(object) runs and the answer is 0. Otherwise 0x08B2B4D8(0x10, the byte-swapped size word) answers a
// record that takes the first read's 4 words and the second read's 4 words (its word +0x1C is the size word), and
// 0x08B0C650(object, &record + 0x20, size - 0x20) reads the payload before 0x08B0B1F8(record, record) runs; this's word +4
// becomes the record, this's word 0 is cleared and the answer is 1.
void func_08B0BB3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    ctx.gpr[18] = self;   // s2
    ctx.gpr[17] = third;  // s1
    if (byte_at(rt, self + 8u) != 0u) {
        put(rt, frame.at(0x18u), 1u);
        put(rt, frame.at(0x1Cu), word(rt, self + 0xCu));
        put(rt, frame.at(0x20u), word(rt, self + 0x10u));
        call(rt, ctx, 0x08B0C9ACu, {third, second, frame.at(0x18u), word(rt, self + 0x10u)});
    } else {
        call(rt, ctx, 0x08B0C99Cu, {third, second, 0u, 0u});
    }
    if (word(rt, third + 4u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, frame.at(0x10u), frame.at(0x24u));
    put(rt, frame.at(0x14u), 0x10u);
    if (static_cast<s32>(call(rt, ctx, 0x08B0C650u, {third, frame.at(0x10u)})) < 0x10 ||
        __builtin_bswap32(word(rt, frame.at(0x24u))) != 0x33627067u) {
        call(rt, ctx, 0x08B0C778u, {third});
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, frame.at(8u), frame.at(0x34u));
    put(rt, frame.at(0xCu), 0x10u);
    if (static_cast<s32>(call(rt, ctx, 0x08B0C650u, {third, frame.at(8u)})) < 0x10) {
        call(rt, ctx, 0x08B0C778u, {third});
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 size = __builtin_bswap32(word(rt, frame.at(0x3Cu)));
    const u32 record = call(rt, ctx, 0x08B2B4D8u, {0x10u, size});
    ctx.gpr[16] = record;
    put(rt, record + 4u, word(rt, frame.at(0x28u)));
    put(rt, record + 8u, word(rt, frame.at(0x2Cu)));
    put(rt, record + 0xCu, word(rt, frame.at(0x30u)));
    put(rt, record + 0x10u, word(rt, frame.at(0x34u)));
    put(rt, record + 0x14u, word(rt, frame.at(0x38u)));
    put(rt, record + 0x18u, word(rt, frame.at(0x3Cu)));
    put(rt, record + 0x1Cu, word(rt, frame.at(0x40u)));
    put(rt, record, word(rt, frame.at(0x24u)));
    put(rt, frame.at(0u), record + 0x20u);
    put(rt, frame.at(4u), size - 0x20u);
    call(rt, ctx, 0x08B0C650u, {third, frame.at(0u)});
    call(rt, ctx, 0x08B0B1F8u, {record, record});
    put(rt, self + 4u, record);
    put(rt, self, 0u);
    ctx.gpr[2] = 1u;
}

// 0x08B4665C (a0 = this, a1 = a packet with a header and a payload at this + 0x8D, a2 = a flag kept as a byte):
// 0x08B3FA98(&state, packet) runs, then 0x08C7AE30(packet's first word, &this + 0x8D, &length, 0, 1, 0x2400) with the
// length 0x2400: the end answers nothing, 0x80410709, a negative answer, a set flag or a clear this's word +4 answer
// nothing. 0x08B3F7CC(&state, packet + 0x1A) runs; when the packet's word +0x34 is set the buffer's tail is moved into
// place (0x08B79BAC) and cleared (0x08B79A88) and the length grows by it. Then for every entry of the buffer (its first
// unsigned halfword plus 0x1C, masked to 16 bits) 0x08B79A88 fills the packet's word +0x40 and this's virtual +0xB0(this,
// &state, entry, size) runs; an entry longer than what is left stops it, 0x08B79A88(packet + 0x40, entry, left) filling the
// packet's word +0x34 unless that is already set.
void func_08B4665C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 packet = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[20] = frame.at(8u);    // s4
    ctx.gpr[19] = packet;          // s3
    ctx.gpr[17] = self;            // s1
    ctx.gpr[18] = self + 0x8Du;    // s2
    ctx.gpr[16] = flag;            // s0
    call(rt, ctx, 0x08B3FA98u, {frame.at(8u), packet});
    put(rt, frame.at(4u), 0x2400u);
    const s32 answer = static_cast<s32>(call(rt, ctx, 0x08C7AE30u,
        {word(rt, packet), self + 0x8Du, frame.at(4u), 0u, 1u, 0x2400u}));
    if (static_cast<u32>(answer) == 0x80410709u || answer < 0 || flag != 0u) return;
    call(rt, ctx, 0x08B3F7CCu, {frame.at(8u), packet + 0x1Au});
    if (word(rt, self + 4u) == 0u) return;
    u32 length = word(rt, frame.at(4u));  // t1
    const u32 extra = word(rt, packet + 0x34u);
    if (extra != 0u) {
        call(rt, ctx, 0x08B79BACu, {self + 0x8Du + extra, self + 0x8Du, length});
        call(rt, ctx, 0x08B79A88u, {self + 0x8Du, word(rt, packet + 0x40u), word(rt, packet + 0x34u)});
        length = word(rt, packet + 0x34u) + length;
        put(rt, packet + 0x34u, 0u);
        put(rt, frame.at(4u), length);
    }
    u32 position = 0u;
    for (;;) {
        const u32 entry = self + 0x8Du + position;
        const u32 remaining = length - position;
        put_byte(rt, frame.at(0u), byte_at(rt, entry));
        put_byte(rt, frame.at(1u), byte_at(rt, entry + 1u));
        const u32 size = (static_cast<u32>(half_at(rt, frame.at(0u))) + 0x1Cu) & 0xFFFFu;
        position += size;
        if (static_cast<s32>(remaining) < static_cast<s32>(size)) {
            if (word(rt, packet + 0x34u) != 0u) return;
            put(rt, packet + 0x34u, remaining);
            call(rt, ctx, 0x08B79A88u, {word(rt, packet + 0x40u), entry, remaining});
            return;
        }
        call_indirect(rt, ctx, word(rt, word(rt, self) + 0xB0u), {self, frame.at(8u), entry, size});
        length = word(rt, frame.at(4u));
        if (!(static_cast<s32>(position) < static_cast<s32>(length))) return;
    }
}

// 0x08C58700 (a0 = this): 0x08B5C450(&state, this + word(this's first word - 0xC) + 0x6C), 0x08B64C80(&state) and
// 0x08B5D34C(&state) answer a table whose word +0x18 holds character classes; the reader at this + word(-0xC) + 0x78 gives a
// character (its byte +8 while below its byte +0xC, else its virtual +0x24). A 0xFF marks the end: this's +0x14 word gets
// bit 1 through 0x08C54CFC and the answer is this; otherwise the character's class byte (signed) with bit 3 clear answers
// this too, and while it is set the next character is taken the same way (its position advanced) until a 0xFF or a class
// without bit 3.
void func_08C58700(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    call(rt, ctx, 0x08B5C450u, {frame.at(0u), self + word(rt, word(rt, self) - 0xCu) + 0x6Cu});
    call(rt, ctx, 0x08B64C80u, {frame.at(0u)});
    const u32 table = call(rt, ctx, 0x08B5D34Cu, {frame.at(0u)});
    ctx.gpr[18] = table;
    const u32 reader = word(rt, self + word(rt, word(rt, self) - 0xCu) + 0x78u);
    ctx.gpr[16] = reader;
    const u32 classes = word(rt, table + 0x18u);
    const u32 first = word(rt, reader + 8u);
    u32 character;
    if (first < word(rt, reader + 0xCu)) {
        character = byte_at(rt, first);
    } else {
        character = call_indirect(rt, ctx, word(rt, word(rt, reader) + 0x24u), {reader});
    }
    for (;;) {
        if (character == 0xFFu) {
            const u32 base = self + word(rt, word(rt, self) - 0xCu);
            call(rt, ctx, 0x08C54CFCu, {base, word(rt, base + 0x14u) | 2u});
            ctx.gpr[2] = self;
            return;
        }
        if ((static_cast<u32>(signed_byte_at(rt, classes + character)) & 8u) == 0u) {
            ctx.gpr[2] = self;
            return;
        }
        const u32 next = word(rt, reader + 8u);
        if (next < word(rt, reader + 0xCu)) {
            character = byte_at(rt, next);
            put(rt, reader + 8u, next + 1u);
        } else {
            character = call_indirect(rt, ctx, word(rt, word(rt, reader) + 0x24u), {reader});
        }
    }
}

// 0x088ACBA0 (a0 = this, a1 = an argument of 0x08B4AD90, a2 = a vector, a3 = a vector + 8): 0x08B4AD90(&a, a2,
// a3 + this's word +0x3D0 * 0x28 + 0x284) and 0x08B4AC30(&a, &a, this + 8) run, then 0x08B4AC0C(&b, &b, &a) and
// 0x08B4AD90(&c, a1, this + this's word +0x3CC * 0x28 + 0x294) with 0x08B4ABCC(&c, &b) answering a float that takes this's
// word +0x3CC element's +0x29C added; a value not above zero ends it, else 0x08B4A560(&d, (value * -0.5)) runs with
// 0x08B4AC0C(a3 + 8, a3 + 8, &d) and 0x08B4AC30(this + 8, this + 8, &d).
void func_088ACBA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[22] = fourth + 8u;       // s6
    ctx.gpr[20] = frame.at(0x28u);   // s4
    ctx.gpr[19] = second;            // s3
    ctx.gpr[18] = frame.at(0x20u);   // s2
    ctx.gpr[17] = self;              // s1
    ctx.gpr[21] = self + 8u;         // s5
    ctx.gpr[16] = fourth;            // s0
    const u32 index = word(rt, self + 0x3D0u);
    call(rt, ctx, 0x08B4AD90u, {frame.at(0x20u), third, fourth + index * 0x28u + 0x284u});
    put_float(rt, frame.at(0u), float_at(rt, fourth + 8u));
    put_float(rt, frame.at(4u), float_at(rt, fourth + 0xCu));
    call(rt, ctx, 0x08B4AC30u, {frame.at(0u), frame.at(0u), self + 8u});
    put_float(rt, frame.at(8u), float_at(rt, frame.at(0u)));
    put_float(rt, frame.at(0xCu), float_at(rt, frame.at(4u)));
    call(rt, ctx, 0x08B4AC0Cu, {frame.at(8u), frame.at(8u), frame.at(0x20u)});
    const u32 other = word(rt, self + 0x3CCu);
    put_float(rt, frame.at(0x14u), float_at(rt, frame.at(0xCu)));
    put_float(rt, frame.at(0x10u), float_at(rt, frame.at(8u)));
    call(rt, ctx, 0x08B4AD90u, {frame.at(0x18u), second, self + other * 0x28u + 0x294u});
    call(rt, ctx, 0x08B4ABCCu, {frame.at(0x18u), frame.at(0x10u)});
    const u32 third_index = word(rt, self + 0x3CCu);
    const float total = ctx.fpr[0] + float_at(rt, self + third_index * 0x28u + 0x29Cu);
    if (total <= 0.0f) return;
    const float scale = psp_mul(-total, 0.5f);
    put_float(rt, frame.at(0x28u), float_at(rt, frame.at(0x18u)));
    put_float(rt, frame.at(0x2Cu), float_at(rt, frame.at(0x1Cu)));
    call(rt, ctx, 0x08B4A560u, {frame.at(0x28u)}, {scale});
    call(rt, ctx, 0x08B4AC0Cu, {fourth + 8u, fourth + 8u, frame.at(0x28u)});
    call(rt, ctx, 0x08B4AC30u, {self + 8u, self + 8u, frame.at(0x28u)});
}

// 0x088E5E60 (a0 = this, a1 and a2 = two words, a3 = a pointer to five words that may be null): nothing when this's first
// word's +0x534 word is 1 (v0 = 0); 0x08B344E0(word + 0xEC) locks, the object's virtual +0x50(word + 0x548) and
// +0x50(-1) answer two records that must agree on their words +0x28 and +0x2C (else the second is dropped), the lock is
// released, and a zero record answers 0. Otherwise the +0xC4 locks (this's word 0's and, when set, its word +4's) are taken,
// 0x088ED374(this's word, 1) answers a record {a1, a2, byte +8 = 1, byte +9 = 0} that also takes a3's five words with its
// byte +9 set to 1, the locks are released and v0 = 1 (or 0 when the record is missing).
void func_088E5E60(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[21] = first;   // s5
    ctx.gpr[20] = second;  // s4
    ctx.gpr[17] = self;    // s1
    ctx.gpr[19] = third;   // s3
    if (word(rt, word(rt, self) + 0x534u) == 1u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B344E0u, {word(rt, self) + 0xECu});
    const u32 main_object = word(rt, self);
    const u32 found = call_indirect(rt, ctx, word(rt, word(rt, main_object) + 0x50u),
                                    {main_object, word(rt, main_object + 0x548u)});
    ctx.gpr[18] = found;  // s2
    const u32 second_object = word(rt, self);
    u32 other = call_indirect(rt, ctx, word(rt, word(rt, second_object) + 0x50u), {second_object, 0xFFFFFFFFu});
    if (found != 0u && other != 0u &&
        (word(rt, found + 0x28u) != word(rt, other + 0x28u) || word(rt, found + 0x2Cu) != word(rt, other + 0x2Cu))) {
        other = 0u;
    }
    ctx.gpr[16] = other;
    call(rt, ctx, 0x08B3457Cu, {word(rt, self) + 0xECu});
    if (other == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B344E0u, {word(rt, self) + 0xC4u});
    if (word(rt, self + 4u) != 0u) call(rt, ctx, 0x08B344E0u, {word(rt, self + 4u) + 0xC4u});
    const u32 record = call(rt, ctx, 0x088ED374u, {word(rt, self), 1u});
    ctx.gpr[16] = record;
    if (record != 0u) {
        put(rt, record, first);
        put(rt, record + 4u, second);
        put_byte(rt, record + 8u, 1u);
        put_byte(rt, record + 9u, 0u);
        if (third != 0u) {
            put(rt, record + 0xCu, word(rt, third));
            put(rt, record + 0x10u, word(rt, third + 4u));
            put(rt, record + 0x14u, word(rt, third + 8u));
            put(rt, record + 0x18u, word(rt, third + 0xCu));
            put(rt, record + 0x1Cu, word(rt, third + 0x10u));
            put_byte(rt, record + 9u, 1u);
        }
    }
    if (word(rt, self + 4u) != 0u) call(rt, ctx, 0x08B3457Cu, {word(rt, self + 4u) + 0xC4u});
    call(rt, ctx, 0x08B3457Cu, {word(rt, self) + 0xC4u});
    ctx.gpr[2] = record != 0u ? 1u : 0u;
}

// The 0x40-byte identity 0x0891B64C writes at sp + 0x80: ones at +0, +0x14, +0x28, +0x3C and zeros at the other slots below
// +0x30 (the translation column is the caller's).
void put_identity_matrix(Runtime &rt, u32 address) {
    put_float(rt, address, 1.0f);
    put_float(rt, address + 0x14u, 1.0f);
    put_float(rt, address + 0x28u, 1.0f);
    put_float(rt, address + 0x3Cu, 1.0f);
    put(rt, address + 0x10u, 0u);
    put(rt, address + 0x20u, 0u);
    put(rt, address + 4u, 0u);
    put(rt, address + 0x24u, 0u);
    put(rt, address + 8u, 0u);
    put(rt, address + 0x18u, 0u);
    put(rt, address + 0xCu, 0u);
    put(rt, address + 0x1Cu, 0u);
    put(rt, address + 0x2Cu, 0u);
}

// 0x0891B64C (a0 = a 3-float output, a1 = an object whose word +8's +0x61C is a matrix, a2 = three floats): 0x088D390C(matrix,
// sp, sp + 0xC, 6, a2 + 4) fills three floats and three angles; 0x08B4E648(&matrix) starts it, the three floats go into its
// translation column and an identity with the three angles (0x08B4E9D4, 0x08B4F3B0, 0x08B4F2E4, 0x08B4F47C) composes it; a
// second identity with a2's first float as its translation goes through 0x08B4E9D4 again and 0x08B4A774(&result, &matrix,
// &origin) answers the output.
void func_0891B64C(Runtime &rt, AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xF0u);
    ctx.gpr[19] = source;           // s3
    ctx.gpr[18] = frame.at(0x80u);  // s2
    ctx.gpr[17] = output;           // s1
    ctx.gpr[16] = frame.at(0x40u);  // s0
    call(rt, ctx, 0x088D390Cu, {word(rt, word(rt, holder + 8u) + 0x61Cu), frame.at(0u), frame.at(0xCu), 6u},
         {float_at(rt, source + 4u)});
    call(rt, ctx, 0x08B4E648u, {frame.at(0x40u)});
    put_float(rt, frame.at(0xB0u), float_at(rt, frame.at(0u)));
    put_float(rt, frame.at(0xB4u), float_at(rt, frame.at(4u)));
    put_float(rt, frame.at(0xB8u), float_at(rt, frame.at(8u)));
    put_identity_matrix(rt, frame.at(0x80u));
    call(rt, ctx, 0x08B4E9D4u, {frame.at(0x40u), frame.at(0x80u)});
    call(rt, ctx, 0x08B4F3B0u, {frame.at(0x40u)}, {float_at(rt, frame.at(0x10u))});
    call(rt, ctx, 0x08B4F2E4u, {frame.at(0x40u)}, {float_at(rt, frame.at(0xCu))});
    call(rt, ctx, 0x08B4F47Cu, {frame.at(0x40u)}, {float_at(rt, frame.at(0x14u))});
    put_float(rt, frame.at(0xB0u), float_at(rt, source));
    put_identity_matrix(rt, frame.at(0x80u));
    put(rt, frame.at(0xB4u), 0u);
    put(rt, frame.at(0xB8u), 0u);
    call(rt, ctx, 0x08B4E9D4u, {frame.at(0x40u), frame.at(0x80u)});
    put_float(rt, frame.at(0x24u), 1.0f);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    call(rt, ctx, 0x08B4A774u, {frame.at(0x28u), frame.at(0x40u), frame.at(0x18u)});
    put_float(rt, output, float_at(rt, frame.at(0x28u)));
    put_float(rt, output + 4u, float_at(rt, frame.at(0x2Cu)));
    put_float(rt, output + 8u, float_at(rt, frame.at(0x30u)));
    ctx.gpr[2] = output;
}

// 0x08A75738 (a0 = this, a1 = a holder whose word 0's word +4 is an object, a2 = a name, a3 = an argument):
// 0x08A98220(this, holder, name, argument) runs and this's vtable word becomes 0x08CD3AE8; 0x08A7F8E8(&local, name, 0,
// 0x08CB0D94) then 0x08B88F44(&second, &local) and 0x08BEED1C(&second, 0x08CB0D94, 2) run, the same for the object (with its
// 0x08B35F60 size) into a third local that 0x08A69654(this + 0x14, &third) takes, the three locals' blocks are released
// (markers sp + 2, sp + 1, sp) and 0x08A76ABC(this + 0x18, argument) finishes.
void func_08A75738(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 name = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[21] = argument;      // s5
    ctx.gpr[20] = self;          // s4
    ctx.gpr[19] = holder;        // s3
    ctx.gpr[17] = frame.at(4u);  // s1
    ctx.gpr[16] = name;          // s0
    call(rt, ctx, 0x08A98220u, {self, holder, name, argument});
    put(rt, self, 0x08CD3AE8u);
    call(rt, ctx, 0x08A7F8E8u, {frame.at(4u), name, 0u, 0x08CB0D94u});
    call(rt, ctx, 0x08B88F44u, {frame.at(8u), frame.at(4u)});
    call(rt, ctx, 0x08BEED1Cu, {frame.at(8u), 0x08CB0D94u, 2u});
    const u32 object = word(rt, word(rt, holder) + 4u);
    ctx.gpr[18] = object;
    call(rt, ctx, 0x08B88F44u, {frame.at(0xCu), frame.at(8u)});
    const u32 size = call(rt, ctx, 0x08B35F60u, {object});
    call(rt, ctx, 0x08BEED1Cu, {frame.at(0xCu), object, size});
    call(rt, ctx, 0x08A69654u, {self + 0x14u, frame.at(0xCu)});
    release_counted(rt, ctx, word(rt, frame.at(0xCu)), frame.at(2u));
    release_counted(rt, ctx, word(rt, frame.at(8u)), frame.at(1u));
    release_counted(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
    call(rt, ctx, 0x08A76ABCu, {self + 0x18u, argument});
}

// 0x08A9C340 (a0 = this): nothing when this's byte +0x7C is already set; else it is set, 0x08A920E0(0) answers a list whose
// nodes (through their word +0x10) get their +4 virtuals and then their +8 virtuals called, 0x08AA0AA4(&local) answers an
// object that replaces this + 0x34 when it is not already that address, 0x08A5BC20(0x38, 4) answers an object that
// 0x08A7E914 initializes with 0x08CF6F0C and this + 0x38 takes (0x08A76AB4(&second, object) before the assignment), then
// 0x08A7BE18(this + 0x38, this + 0x34) and 0x08A7BFD0(this + 0x34, this + 0x3C, 0x08CF6F38) /
// 0x08A7BFD0(this + 0x34, this + 0x40, 0x08CF6FEC) run (0x08A5BB7C reporting a missing object at 0x08CB2570).
void func_08A9C340(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[18] = self;
    if (byte_at(rt, self + 0x7Cu) != 0u) return;
    put_byte(rt, self + 0x7Cu, 1u);
    const u32 list = call(rt, ctx, 0x08A920E0u, {0u});
    ctx.gpr[17] = list;
    if (list != 0u) {
        u32 node = list;
        for (;;) {
            ctx.gpr[16] = node;
            call_indirect(rt, ctx, word(rt, node + 4u), {});
            node = word(rt, node + 0x10u);
            if (node == 0u) break;
        }
        node = list;
        for (;;) {
            ctx.gpr[16] = node;
            call_indirect(rt, ctx, word(rt, node + 8u), {});
            node = word(rt, node + 0x10u);
            if (node == 0u) break;
        }
    }
    call(rt, ctx, 0x08AA0AA4u, {frame.at(8u)});
    if (self + 0x34u != frame.at(8u)) call(rt, ctx, 0x08A5BAD8u, {self + 0x34u, word(rt, frame.at(8u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[16] = object;
    call(rt, ctx, 0x08A7E914u, {object, 0x08CF6F0Cu});
    put(rt, frame.at(0u), object);
    call(rt, ctx, 0x08A76AB4u, {frame.at(4u), frame.at(0u)});
    if (self + 0x38u != frame.at(4u)) call(rt, ctx, 0x08A5BAD8u, {self + 0x38u, word(rt, frame.at(4u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    if (word(rt, self + 0x38u) == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    call(rt, ctx, 0x08A7BE18u, {word(rt, self + 0x38u), word(rt, self + 0x34u)});
    if (word(rt, self + 0x34u) == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    call(rt, ctx, 0x08A7BFD0u, {word(rt, self + 0x34u), self + 0x3Cu, 0x08CF6F38u});
    if (word(rt, self + 0x34u) == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    call(rt, ctx, 0x08A7BFD0u, {word(rt, self + 0x34u), self + 0x40u, 0x08CF6FECu});
}

// 0x0885FAA4 (a0 = this, a1 = an index, a2 = an argument): the record this + index * 0x50 + 0x4120 is taken,
// 0x088A79AC(sp, 0) fills a record and two of the three 0x30-byte locals at sp + 0x40 are built (0x08897B8C);
// 0x08866510(record + 0x44, record + 0x48, &local, sp, sp + 0x40) answers the byte at sp + 0x11 that, when set, runs
// 0x0885F938(this, index, sp + 0x40, 0x088DCD28(this + 0x5AC, index), (a2 == -1 ? record + 0x24 : -1), record + 0x1B,
// record + 0x18); when a2 is -1 the object 0x088F8CC0(this + 0x5B4, index, record + 0x24) gets its +0x10(0) and +0x1C(0)
// virtuals and its byte +0xB cleared; the record's byte +0x19 becomes 1, 0x088F8A84(this + 0x5B4, index, record + 0x24)
// runs and the other local is released. v0 = 1 when the byte was set, else 0.
void func_0885FAA4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xD0u);
    ctx.gpr[21] = frame.at(0x40u);  // s5
    ctx.gpr[19] = self;             // s3
    ctx.gpr[18] = index;            // s2
    ctx.gpr[23] = frame.at(0xA0u);  // s7
    ctx.gpr[22] = argument;         // s6
    const u32 record = self + index * 0x50u + 0x4120u;
    ctx.gpr[17] = record;           // s1
    call(rt, ctx, 0x088A79ACu, {frame.at(0u), 0u});
    for (u32 offset = 0x40u;;) {
        ctx.gpr[16] = frame.at(offset);
        call(rt, ctx, 0x08897B8Cu, {frame.at(offset)});
        offset += 0x30u;
        ctx.gpr[16] = frame.at(offset);
        if (offset == 0xA0u) break;
    }
    call(rt, ctx, 0x08866510u, {word(rt, record + 0x44u), word(rt, record + 0x48u), frame.at(8u), frame.at(0u),
                                frame.at(0x40u)});
    u32 result = 0u;  // s4
    if (signed_byte_at(rt, frame.at(0x11u)) != 0) {
        const u32 value = call(rt, ctx, 0x088DCD28u, {word(rt, self + 0x5ACu), index});
        const u32 limit = argument == 0xFFFFFFFFu ? word(rt, record + 0x24u) : 0xFFFFFFFFu;
        call(rt, ctx, 0x0885F938u, {self, index, frame.at(0x40u), value, limit, byte_at(rt, record + 0x1Bu),
                                    byte_at(rt, record + 0x18u)});
        if (argument == 0xFFFFFFFFu) {
            const u32 object = call(rt, ctx, 0x088F8CC0u, {word(rt, self + 0x5B4u), index, word(rt, record + 0x24u)});
            ctx.gpr[16] = object;
            call_indirect(rt, ctx, word(rt, word(rt, object) + 0x10u), {object, 0u});
            call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, 0u});
            put_byte(rt, object + 0xBu, 0u);
        }
        put_byte(rt, record + 0x19u, 1u);
        result = 1u;
        call(rt, ctx, 0x088F8A84u, {word(rt, self + 0x5B4u), index, word(rt, record + 0x24u)});
    }
    call(rt, ctx, 0x088978B4u, {frame.at(0x70u)});
    call(rt, ctx, 0x088A7548u, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// 0x08872940 (a0 = a 3-float output, a1 = this): 0x0886B920(this's word +8, 0x08C9F8A0, 0) answers an object and this's
// word +0x5C a record whose word +0x10 selects the output: 2 takes this + 0x2C's three floats, 3 the same after
// 0x08B4AA68(output, output, this + 0x20) and a normalize (0x08B4A720, 0x08B4A5E8 with the record's word +0x14 over the
// length), the others (and a missing object for a selector below 2) leave it zero, and a selector below 2 with an object
// takes its +0x28 virtual(object, sp, 0x08C9F8A4) answer's three floats at sp + 0x30. 0x08B4AA34(output, output,
// record + 0x18) applies the record's matrix last. v0 = the output.
void func_08872940(Runtime &rt, AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    ctx.gpr[18] = holder + 0x5Cu;  // s2
    ctx.gpr[17] = holder;          // s1
    ctx.gpr[16] = output;          // s0
    const u32 found = call(rt, ctx, 0x0886B920u, {word(rt, holder + 8u), 0x08C9F8A0u, 0u});
    const u32 record = word(rt, holder + 0x5Cu);
    const u32 kind = word(rt, record + 0x10u);
    bool zero = false;
    if (kind >= 2u) {
        if (kind == 2u || kind == 3u) {
            put_float(rt, output, float_at(rt, holder + 0x2Cu));
            put_float(rt, output + 4u, float_at(rt, holder + 0x30u));
            put_float(rt, output + 8u, float_at(rt, holder + 0x34u));
            if (kind == 3u) {
                put_float(rt, frame.at(0u), float_at(rt, holder + 0x20u));
                put_float(rt, frame.at(4u), float_at(rt, holder + 0x24u));
                put_float(rt, frame.at(8u), float_at(rt, holder + 0x28u));
                call(rt, ctx, 0x08B4AA68u, {output, output, frame.at(0u)});
                const float scale = float_at(rt, word(rt, holder + 0x5Cu) + 0x14u);
                call(rt, ctx, 0x08B4A720u, {output});
                call(rt, ctx, 0x08B4A5E8u, {output}, {scale / std::sqrt(ctx.fpr[0])});
                call(rt, ctx, 0x08B4AA34u, {output, output, frame.at(0u)});
            }
        } else {
            zero = true;
        }
    } else if (found == 0u) {
        zero = true;
    } else {
        call_indirect(rt, ctx, word(rt, word(rt, found) + 0x28u), {found, frame.at(0u), 0x08C9F8A4u});
        put_float(rt, output, float_at(rt, frame.at(0x30u)));
        put_float(rt, output + 4u, float_at(rt, frame.at(0x34u)));
        put_float(rt, output + 8u, float_at(rt, frame.at(0x38u)));
    }
    if (zero) {
        put(rt, output, 0u);
        put(rt, output + 4u, 0u);
        put(rt, output + 8u, 0u);
    }
    call(rt, ctx, 0x08B4AA34u, {output, output, word(rt, word(rt, holder + 0x5Cu)) + 0x18u});
    ctx.gpr[2] = output;
}

// 0x088BE9C4 (a0 = this, a1 = a float output that may be null): 0x088D5910(this + 0x10 + 4) then
// 0x088DF13C(this + 0xC, this + 0x10) answer two counts; nothing (0) when this's word +0x18's byte +0x98 is clear, the
// first count is not positive, the second is not positive or the first is below the second. Otherwise the object at
// this + 0x1C's virtual +0x14 answers a unit, the counts weight it (0x088DF13C's first float added), the ratio over the
// first count (0 when not positive) goes through 0x089468C0(this's word +0x14 + 0x27C), 0x088DA1C8's answer + 1 scales it
// by this's word +0x14's +0x288 when it reaches 2, and this's word +0x18's byte +0x99 (when positive) scales it by
// 0x089468C0(word + 0x14 + 0x2B0, (byte - 1) / 100); the output takes it and v0 = 1.
void func_088BE9C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 output = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[21] = output;        // s5
    ctx.gpr[19] = self;          // s3
    const u32 field = self + 4u;  // s0
    ctx.gpr[16] = field;
    const u32 state = word(rt, self + 0x18u);  // s4
    ctx.gpr[20] = state;
    call(rt, ctx, 0x088D5910u, {word(rt, field + 8u) + 4u});
    const u32 record = call(rt, ctx, 0x088DF13Cu, {word(rt, field + 8u), word(rt, field + 0xCu)});
    const s32 first = static_cast<s32>(ctx.gpr[2]);  // the call's v0
    ctx.gpr[17] = ctx.gpr[2];
    const s32 second = static_cast<s32>(word(rt, record + 0x18u));
    ctx.gpr[18] = static_cast<u32>(second);
    if (byte_at(rt, state + 0x98u) == 0u || first <= 0 || second <= 0 || first < second) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (output != 0u) {
        const u32 object = word(rt, self + 0x1Cu);
        const float unit = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x14u), {object});
        (void)unit;
        const float measured = ctx.fpr[0];
        const float weighted = psp_mul(measured, static_cast<float>(first));
        const u32 other = call(rt, ctx, 0x088DF13Cu, {word(rt, field + 8u), word(rt, field + 0xCu)});
        const float sum = psp_mul(measured, static_cast<float>(second)) + float_at(rt, other);
        const float ratio = weighted > 0.0f ? sum / weighted : 0.0f;
        const u32 scale_state = word(rt, self + 0x14u);
        call(rt, ctx, 0x089468C0u, {scale_state + 0x27Cu}, {ratio});
        float result = ctx.fpr[0];
        const s32 count = static_cast<s32>(call(rt, ctx, 0x088DA1C8u, {word(rt, field + 8u), word(rt, field + 0xCu)}));
        const s32 steps = count + 1;
        if (steps >= 2) result = psp_mul(result, float_at(rt, scale_state + 0x288u) / static_cast<float>(steps));
        const u32 limit = byte_at(rt, state + 0x99u);
        if (static_cast<s32>(limit) > 0) {
            call(rt, ctx, 0x089468C0u, {scale_state + 0x2B0u},
                 {static_cast<float>(limit - 1u) / 100.0f});
            result = psp_mul(result, ctx.fpr[0]);
        }
        put_float(rt, output, result);
    }
    ctx.gpr[2] = 1u;
}

// 0x088CB668 (a0 = this, a1 = a pointer to an object, a2 = an object that may be null, a3 = a 3-float output, f12 = a
// scale): the object's virtual +0x1C(this + 0x2C) answers another whose +0x10 virtual fills sp; this's word +0x30 and
// 0x63C word make a record at sp + 0x48 (3, the 2 when the word is 4, the word, 0, the scale, 0) that
// word(this + 0x63C)'s +0x1C then the new object's +8 virtual take; 0x089545BC(sp + 0x10) runs with the record's floats
// (its +0x70, +0x74, +0x78), the output gets the first, the third negated and, when present, the second after
// 0x089545BC moved it (else the +0x1C float); a2's virtual +0x18 runs (with the record 0x40/0x44 = this's +0x5F0/+0x5F4
// and the byte 0x3C = 1).
void func_088CB668(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 output = ctx.gpr[7];
    const float scale = ctx.fpr[12];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xD0u);
    ctx.gpr[19] = frame.at(0x10u);            // s3
    ctx.gpr[18] = output;                     // s2
    ctx.gpr[17] = third;                      // s1
    ctx.gpr[16] = self;                       // s0
    ctx.gpr[20] = word(rt, self + 0x5F0u);    // s4
    ctx.gpr[21] = word(rt, self + 0x5F4u);    // s5
    const u32 object = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x1Cu),
                                    {holder, word(rt, self + 0x2Cu)});
    if (object == 0u) return;
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x10u), {object, frame.at(0u)});
    const u32 kind = word(rt, self + 0x30u);
    put(rt, frame.at(0x4Cu), kind == 4u ? 2u : 0u);
    put(rt, frame.at(0x50u), kind);
    put_float(rt, frame.at(0x58u), scale);
    put(rt, frame.at(0x48u), 3u);
    put(rt, frame.at(0x54u), 0u);
    put(rt, frame.at(0x5Cu), 0u);
    const float larger = float_at(rt, frame.at(4u));
    const u32 state = word(rt, self + 0x63Cu);
    const u32 inner = call_indirect(rt, ctx, word(rt, word(rt, state) + 0x1Cu), {state});
    call_indirect(rt, ctx, word(rt, word(rt, inner) + 8u), {inner, frame.at(0x48u)});
    const float first = float_at(rt, frame.at(0x70u));
    const float second = float_at(rt, frame.at(0x74u));
    const float third_value = float_at(rt, frame.at(0x78u));
    call(rt, ctx, 0x089545BCu, {frame.at(0x10u)});
    put_float(rt, frame.at(0x14u), larger + 1.0f);
    put(rt, frame.at(0x40u), ctx.gpr[20]);
    put(rt, frame.at(0x44u), ctx.gpr[21]);
    put_float(rt, frame.at(0x10u), first);
    put_float(rt, frame.at(0x18u), -second);
    put_byte(rt, frame.at(0x3Cu), 1u);
    put_float(rt, output, first);
    put_float(rt, output + 8u, -second);
    if (third != 0u) call_indirect(rt, ctx, word(rt, word(rt, third) + 0x18u), {third});
    if (byte_at(rt, frame.at(0x3Cu)) != 0u) {
        put_float(rt, output + 4u, third_value);
    } else {
        put_float(rt, output + 4u, float_at(rt, frame.at(0x1Cu)));
    }
}

// 0x088E6E14 (a0 = this, a1 = a value): 0x088E6D90(this, -1) answers a 64-bit pair that 0x088EF438(this's word 0's word
// +0xAC, 0x50, pair) takes; then 0x088EF430(command, 3, 0x08B09974(0x08B0979C())), 0x088EF438(command, 0x18,
// 0x08B08A88, 0), 0x088EF438(command, 5, 0x08B08A70, high), 0x088EF438(command, 6, this's +0x550, high),
// 0x088EF438(command, 4, (this's +0x548 == -1 or == +0x54C) ? 1 : 0, 0), 0x088EF438(command, 1, this's +0x548, high),
// 0x088EF430(command, 0x2E, 0x08B08AA0), 0x088EF438(command, 2, this's +0x54C, high) and 0x088EF438(command, 0x17, a1,
// high) run, and 0x088EF440(command) finishes (a tail call).
void func_088E6E14(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = value;  // s2
    ctx.gpr[17] = self;   // s1
    const u32 command = word(rt, word(rt, self) + 0xACu);  // s0
    ctx.gpr[16] = command;
    call(rt, ctx, 0x088E6D90u, {self, 0xFFFFFFFFu});
    call(rt, ctx, 0x088EF438u, {command, 0x50u, ctx.gpr[2], ctx.gpr[3]});
    const u32 manager = call(rt, ctx, 0x08B0979Cu, {});
    call(rt, ctx, 0x088EF430u, {command, 3u, call(rt, ctx, 0x08B09974u, {manager})});
    call(rt, ctx, 0x088EF438u, {command, 0x18u,
                                call(rt, ctx, 0x08B08A88u, {word(rt, word(rt, self) + 0xA8u)}), 0u});
    const u32 first = call(rt, ctx, 0x08B08A70u, {word(rt, word(rt, self) + 0xA8u)});
    call(rt, ctx, 0x088EF438u, {command, 5u, first, first >> 31});
    const u32 second = word(rt, word(rt, self) + 0x550u);
    call(rt, ctx, 0x088EF438u, {command, 6u, second, second >> 31});
    const u32 object = word(rt, self);
    const u32 current = word(rt, object + 0x548u);
    u32 flag = 0u;
    if (current != 0xFFFFFFFFu && current == word(rt, object + 0x54Cu)) flag = 1u;
    call(rt, ctx, 0x088EF438u, {command, 4u, flag, 0u});
    const u32 third = word(rt, word(rt, self) + 0x548u);
    call(rt, ctx, 0x088EF438u, {command, 1u, third, third >> 31});
    call(rt, ctx, 0x088EF430u, {command, 0x2Eu,
                                call(rt, ctx, 0x08B08AA0u, {word(rt, word(rt, self) + 0xA8u)})});
    const u32 fourth = word(rt, word(rt, self) + 0x54Cu);
    call(rt, ctx, 0x088EF438u, {command, 2u, fourth, fourth >> 31});
    call(rt, ctx, 0x088EF438u, {command, 0x17u, value, value >> 31});
    tail_call(ctx, 0x088EF440u, {command});
}

// 0x089AFC78 (a0 = this, a1 = an object): 0x08B344E0(0x08CF4A04) locks, 0x08A5BA28(object) runs, object's word +0x138 gets
// bit 9 set, 0x089AFA10(this, this + 0x30, this + 0x18) runs and a node record at sp (0x08CCFD88, the local twice) answers
// 0x089B9E60's key that 0x08BF9104(&tree, &key) looks up: a different node with a bigger key (0x10) gets a new node through
// 0x08BF99BC(&tree, {key, 0}). A 0xC-byte block (0x08CCEA88, 0x08BF8A7C, 0) goes to the node's word +0x14,
// 0x089A931C(this, 0x08CCEA78) and object's virtual +0x164(this) run, then 0x089A9314(this, 0x08CCEA78), 0x08A5B970(object),
// 0x08A5BC6C() and 0x089FD82C(0x08CCEA78) and 0x08B3457C(0x08CF4A04) finish.
void func_089AFC78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[21] = 0x08CF4A04u;      // s5
    ctx.gpr[20] = self;             // s4
    ctx.gpr[18] = second;           // s2
    ctx.gpr[17] = frame.at(0x10u);  // s1
    call(rt, ctx, 0x08B344E0u, {0x08CF4A04u});
    call(rt, ctx, 0x08A5BA28u, {second});
    put(rt, second + 0x138u, word(rt, second + 0x138u) | 0x200u);
    call(rt, ctx, 0x089AFA10u, {self, self + 0x30u, self + 0x18u});
    const u32 local = frame.at(0x14u);  // s0
    put(rt, frame.at(0xCu), 0x08CCFD88u);
    put(rt, frame.at(0x24u), 0u);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), local);
    put(rt, frame.at(0x20u), local);
    const u32 answer = call(rt, ctx, 0x089B9E60u, {self});
    put(rt, frame.at(0u), answer);
    const u32 found = call(rt, ctx, 0x08BF9104u, {frame.at(0x10u), frame.at(0u)});
    ctx.gpr[19] = found;  // s3
    u32 node = found;
    if (local == found || answer < word(rt, found + 0x10u)) {
        put(rt, frame.at(4u), answer);
        put(rt, frame.at(8u), 0u);
        node = call(rt, ctx, 0x08BF99BCu, {frame.at(0x10u), frame.at(4u)});
        ctx.gpr[19] = node;
    }
    const u32 block = call(rt, ctx, 0x08B62344u, {0xCu});
    put(rt, block, 0x08CCEA88u);
    put(rt, block + 4u, 0x08BF8A7Cu);
    put(rt, block + 8u, 0u);
    put(rt, node + 0x14u, block);
    put(rt, frame.at(0xCu), 0x08CCEA78u);
    ctx.gpr[16] = 0x08CCEA78u;  // s0
    call(rt, ctx, 0x089A931Cu, {self, frame.at(0xCu)});
    put(rt, frame.at(0x28u), second);
    const u32 listener = word(rt, second);
    call_indirect(rt, ctx, word(rt, listener + 0x164u), {second, self});
    call(rt, ctx, 0x089A9314u, {self, frame.at(0xCu)});
    call(rt, ctx, 0x08A5B970u, {second});
    call(rt, ctx, 0x08A5BC6Cu, {});
    put(rt, frame.at(0xCu), 0x08CCEA78u);
    call(rt, ctx, 0x089FD82Cu, {frame.at(0xCu)});
    call(rt, ctx, 0x08B3457Cu, {0x08CF4A04u});
}

// 0x08A243FC (a0 = a destination object, a1 = a count, a2 = two objects): nothing when the count (signed) is 2 or more;
// 0x08A6C410(&first, a2) and 0x08A6C410(&second, a2 + 4) then 0x08ABB9E8(0) answer a record that the first's +0xC virtual
// fills a local with, the second's +0xC virtual a second local, 0x08ABBAE0(record, &first-local, &second-local) answers
// another whose 0x08A97A6C(&result, record) the destination is assigned from (0x08A5BAD8), and the two blocks, the result
// and both locals are released.
void func_08A243FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 objects = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[19] = destination;  // s3
    ctx.gpr[16] = objects;      // s0
    if (static_cast<s32>(count) < 2) {
        ctx.gpr[21] = frame.at(0x10u);  // s5
        ctx.gpr[20] = frame.at(0x14u);  // s4
        call(rt, ctx, 0x08A6C410u, {frame.at(0x10u), objects});
        call(rt, ctx, 0x08A6C410u, {frame.at(0x14u), objects + 4u});
        u32 result = call(rt, ctx, 0x08ABB9E8u, {0u});
        ctx.gpr[18] = result;
        const u32 first = word(rt, frame.at(0x10u));
        if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAACD8u});
        call_indirect(rt, ctx, word(rt, word(rt, first) + 0xCu), {frame.at(0xCu), first});
        const u32 second = word(rt, frame.at(0x14u));
        if (second == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAACD8u});
        const u32 first_local = word(rt, frame.at(0xCu));
        ctx.gpr[17] = first_local;  // s1
        call_indirect(rt, ctx, word(rt, word(rt, second) + 0xCu), {frame.at(8u), second});
        result = call(rt, ctx, 0x08ABBAE0u, {result, first_local, word(rt, frame.at(8u))});
        release_counted(rt, ctx, word(rt, frame.at(8u)), frame.at(1u));
        release_counted(rt, ctx, word(rt, frame.at(0xCu)), frame.at(0u));
        const u32 local = frame.at(4u);
        call(rt, ctx, 0x08A97A6Cu, {local, result});
        if (destination != local) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, local)});
        call(rt, ctx, 0x08A5B9ECu, {local});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
    }
}

// 0x08A52D3C (a0 = a destination object, a1 = a word kept in a local, a2 = a size that may be negative, a3 = an argument,
// t0 = another argument): 0x08A52AA8(&first, &word) runs; with a non-negative size, 0x08A5BC20(0x44, 4) answers a block that
// 0x08A5B70C(block, &first, size) fills and 0x08A52AB0(&second, block) adopts (the first is assigned the second then
// released). The record at &first gets its +0x15C virtual(0) and its +0x120 virtual, then 0x08A5BC20(0x18, 4) answers a
// block for 0x08A56018(block, 0, block, a3, t0) that 0x08A5BAB8(&third, block) adopts,
// 0x08A57B54(destination, &first, &third, 0) runs and both locals are released. v0 = the destination.
void func_08A52D3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 word_argument = ctx.gpr[5];
    const s32 size = static_cast<s32>(ctx.gpr[6]);
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[19] = frame.at(0x14u);  // s3
    ctx.gpr[21] = destination;      // s5
    ctx.gpr[17] = static_cast<u32>(size);  // s1
    ctx.gpr[23] = third;            // s7
    ctx.gpr[22] = fourth;           // s6
    ctx.gpr[20] = frame.at(8u);     // s4
    ctx.gpr[18] = frame.at(0xCu);   // s2
    put(rt, frame.at(0x10u), word_argument);
    call(rt, ctx, 0x08A52AA8u, {frame.at(0x14u), frame.at(0x10u)});
    if (size >= 0) {
        const u32 block = call(rt, ctx, 0x08A5BC20u, {0x44u, 4u});
        ctx.gpr[16] = block;
        call(rt, ctx, 0x08A5B70Cu, {block, frame.at(0x14u), static_cast<u32>(size)});
        put(rt, frame.at(4u), block);
        call(rt, ctx, 0x08A52AB0u, {frame.at(0xCu), frame.at(4u)});
        if (frame.at(0x14u) != frame.at(0xCu)) {
            call(rt, ctx, 0x08A5BAD8u, {frame.at(0x14u), word(rt, frame.at(0xCu))});
        }
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    }
    const u32 record = word(rt, frame.at(0x14u));
    if (record == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF4A8u});
    call_indirect(rt, ctx, word(rt, word(rt, record) + 0x15Cu), {record, 0u});
    const u32 other = word(rt, frame.at(0x14u));
    if (other == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CAF4A8u});
    call_indirect(rt, ctx, word(rt, word(rt, other) + 0x120u), {other});
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    ctx.gpr[17] = block;
    call(rt, ctx, 0x08A56018u, {block, 0u, block, third, fourth});
    put(rt, frame.at(0u), block);
    call(rt, ctx, 0x08A5BAB8u, {frame.at(8u), frame.at(0u)});
    call(rt, ctx, 0x08A57B54u, {destination, frame.at(0x14u), frame.at(8u), 0u});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    ctx.gpr[2] = destination;
}

// 0x08AAAB6C (a0 = this, a1 = a key): this's word +4 (0x08A5BB7C reporting 0x08CB30B0 when missing) is the holder;
// 0x08B85EB8(&local, key, &marker) then 0x08C31D6C(this + 8, &local) answer a node: when it differs from this + 0xC, its
// block (word +0x10) and the key's are compared through 0x08B36170 (the counts' difference, the call's non-zero answer
// replacing it) and a negative difference makes a new node: 0x08B88F44(&other, &local), then
// 0x08C335FC(this + 8, node, &other) (its +0x14 word cleared first) and the other's block released. Finally
// 0x08A71B00(holder, word(word(node + 0x14) + 4)) runs and the local's block is released.
void func_08AAAB6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[17] = key;                       // s1
    const u32 holder = word(rt, self + 4u);  // s6
    ctx.gpr[22] = holder;
    ctx.gpr[16] = self;                      // s0
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    const u32 local = frame.at(4u);  // s4
    call(rt, ctx, 0x08B85EB8u, {local, key, frame.at(2u)});
    const u32 found = call(rt, ctx, 0x08C31D6Cu, {self + 8u, local});
    ctx.gpr[19] = found;  // s3
    u32 node = found;
    bool insert = true;
    if (self + 0xCu != found) {
        const u32 first_block = word(rt, word(rt, local));
        const u32 first_count = word(rt, first_block - 0xCu);
        const u32 second_block = word(rt, found + 0x10u);
        const u32 second_count = word(rt, second_block - 0xCu);
        const u32 smaller = second_count < first_count ? second_count : first_count;
        ctx.gpr[18] = found;
        s32 difference = static_cast<s32>(first_count - second_count);
        const u32 grown = call(rt, ctx, 0x08B36170u, {first_block, second_block, smaller});
        if (grown != 0u) difference = static_cast<s32>(grown);
        if (difference >= 0) insert = false;
    }
    if (insert) {
        const u32 other = frame.at(0x10u);  // s0
        call(rt, ctx, 0x08B88F44u, {other, local});
        put(rt, frame.at(0x14u), 0u);
        node = call(rt, ctx, 0x08C335FCu, {self + 8u, found, other});
        ctx.gpr[18] = node;
        release_counted(rt, ctx, word(rt, frame.at(0x10u)), frame.at(1u));
    }
    call(rt, ctx, 0x08A71B00u, {holder, word(rt, word(rt, node + 0x14u) + 4u)});
    release_counted(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
}

// 0x0884EFAC (a0 = a destination object, a1 = passed on, a2 = a count, a3 = an object): 0x0884D660(&local, a1) answers an
// object (0x08A5BB7C reports 0x08C9D198 when missing) that 0x0884D3D8 answers a record for, the local being released; with a
// positive count the object's +0xC virtual fills a local whose 8 bytes are copied (0x08B7C830) to
// 0x08896FB0(record) + 0x80 and released; otherwise 0x08B85EB8(&second, 0x08896FB0(record) + 0x80, &marker) and
// 0x08A6D2D4(&third, &second) build a value the destination takes (0x08A5BAD8), and both locals are released.
void func_0884EFAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[21] = destination;     // s5
    ctx.gpr[20] = frame.at(0xCu);  // s4
    ctx.gpr[18] = source;          // s2
    ctx.gpr[17] = count;           // s1
    call(rt, ctx, 0x0884D660u, {frame.at(0xCu), ctx.gpr[5]});
    const u32 object = word(rt, frame.at(0xCu));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D198u});
    const u32 record = call(rt, ctx, 0x0884D3D8u, {object});
    ctx.gpr[19] = record;  // s3
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    if (static_cast<s32>(count) > 0) {
        const u32 value = word(rt, source);
        if (value == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D158u});
        call_indirect(rt, ctx, word(rt, word(rt, value) + 0xCu), {frame.at(0x10u), value});
        const u32 target = call(rt, ctx, 0x08896FB0u, {record});
        call(rt, ctx, 0x08B7C830u, {target + 0x80u, word(rt, frame.at(0x10u)), 8u});
        release_counted(rt, ctx, word(rt, frame.at(0x10u)), frame.at(1u));
    } else {
        const u32 target = call(rt, ctx, 0x08896FB0u, {record});
        call(rt, ctx, 0x08B85EB8u, {frame.at(4u), target + 0x80u, frame.at(2u)});
        call(rt, ctx, 0x08A6D2D4u, {frame.at(8u), frame.at(4u)});
        if (destination != frame.at(8u)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(8u))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        release_counted(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
    }
}

} // namespace
} // namespace new_batch_22
} // namespace gt::game

#define NEW22(address, name, kind) GT_DECOMP(address, gt::game::new_batch_22::name, gt::ReturnKind::kind)
NEW22(0x08821ADCu, func_08821ADC, Word);
NEW22(0x08830BFCu, func_08830BFC, Word);
NEW22(0x0884EFACu, func_0884EFAC, Word);
NEW22(0x0885C3C8u, func_0885C3C8, Word);
NEW22(0x0885F938u, func_0885F938, Word);
NEW22(0x0885FAA4u, func_0885FAA4, Word);
NEW22(0x08860FF8u, func_08860FF8, Word);
NEW22(0x088619A8u, func_088619A8, Word);
NEW22(0x088622CCu, func_088622CC, Word);
NEW22(0x08865FF4u, func_08865FF4, Word);
NEW22(0x08866B40u, func_08866B40, Word);
NEW22(0x08869514u, func_08869514, Word);
NEW22(0x088726B8u, func_088726B8, Word);
NEW22(0x08872940u, func_08872940, Word);
NEW22(0x0887A28Cu, func_0887A28C, Word);
NEW22(0x08891F04u, func_08891F04, Word);
NEW22(0x088ACBA0u, func_088ACBA0, Word);
NEW22(0x088B0AA0u, func_088B0AA0, Word);
NEW22(0x088BE9C4u, func_088BE9C4, Word);
NEW22(0x088C896Cu, func_088C896C, Word);
NEW22(0x088CB668u, func_088CB668, Word);
NEW22(0x088D0B0Cu, func_088D0B0C, Word);
NEW22(0x088D0DC4u, func_088D0DC4, Word);
NEW22(0x088D57A0u, func_088D57A0, Word);
NEW22(0x088DFDA0u, func_088DFDA0, Word);
NEW22(0x088E5E60u, func_088E5E60, Word);
NEW22(0x088E6E14u, func_088E6E14, Word);
NEW22(0x088F9F00u, func_088F9F00, Word);
NEW22(0x0890FC28u, func_0890FC28, Word);
NEW22(0x0891B64Cu, func_0891B64C, Word);
NEW22(0x0892CF9Cu, func_0892CF9C, Word);
NEW22(0x0892D1C0u, func_0892D1C0, Word);
NEW22(0x0899FD68u, func_0899FD68, Word);
NEW22(0x089A54A0u, func_089A54A0, Word);
NEW22(0x089AA1C0u, func_089AA1C0, Word);
NEW22(0x089AFC78u, func_089AFC78, Word);
NEW22(0x089B8F38u, func_089B8F38, Word);
NEW22(0x089CA118u, func_089CA118, Word);
NEW22(0x089D0BE0u, func_089D0BE0, Word);
NEW22(0x089DBCE8u, func_089DBCE8, Word);
NEW22(0x089DBE58u, func_089DBE58, Word);
NEW22(0x08A243FCu, func_08A243FC, Word);
NEW22(0x08A52D3Cu, func_08A52D3C, Word);
NEW22(0x08A75738u, func_08A75738, Word);
NEW22(0x08A816C4u, func_08A816C4, Word);
NEW22(0x08A9C1C0u, func_08A9C1C0, Word);
NEW22(0x08A9C340u, func_08A9C340, Word);
NEW22(0x08AAAB6Cu, func_08AAAB6C, Word);
NEW22(0x08AB7260u, func_08AB7260, Word);
NEW22(0x08ABEF54u, func_08ABEF54, Word);
NEW22(0x08ABF0D0u, func_08ABF0D0, Word);
NEW22(0x08ABF410u, func_08ABF410, Word);
NEW22(0x08AC9B54u, func_08AC9B54, Word);
NEW22(0x08AE1D6Cu, func_08AE1D6C, Word);
NEW22(0x08AE6268u, func_08AE6268, Word);
NEW22(0x08AF5164u, func_08AF5164, Word);
NEW22(0x08B0BB3Cu, func_08B0BB3C, Word);
NEW22(0x08B2D61Cu, func_08B2D61C, Word);
NEW22(0x08B4665Cu, func_08B4665C, Word);
NEW22(0x08B48A74u, func_08B48A74, Word);
NEW22(0x08B65728u, func_08B65728, Word);
NEW22(0x08B8D5D8u, func_08B8D5D8, Word);
NEW22(0x08BB2340u, func_08BB2340, Word);
NEW22(0x08BE6244u, func_08BE6244, Word);
NEW22(0x08C3D12Cu, func_08C3D12C, Word);
NEW22(0x08C4B5C8u, func_08C4B5C8, Word);
NEW22(0x08C58700u, func_08C58700, Word);
