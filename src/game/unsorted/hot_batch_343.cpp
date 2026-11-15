// Hot batch 343: functions still recompiled (picked with `tools.next_leaves --ready`), written as structured C from
// `tools.disasm` and verified by synthetic cases: record decoders (bytes into floats and integers), a
// curve evaluator, a power-of-two scaling, searches over arrays of words / pointers that unroll by four, a byte-table record conversion (two copies of
// one routine with a shifted argument list) and short functions that call others.
// The callee-saved registers are set where the asm sets them, because a callee may read them, and put back at the end (the
// original saves and restores them). `v0` ends as the asm leaves it: the last call's result or a loaded value.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_343 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal` (a callee that never writes v0 returns what
// v0 held when it was called, so the caller's v0 has to stay current).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with float arguments ($f12, ...); afterwards v0 / v1 / f0 are the callee's.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void set_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void set_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// The callee-saved registers s0-s7: the original saves and restores them, so the body may set them (a callee reads them
// through the context) and they come back as they were.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
};

// `div` followed by `mfhi`: the remainder, toward zero; INT_MIN / -1 leaves 0. The caller handles a zero divisor (the
// original's `break 7`).
u32 signed_remainder(u32 dividend, u32 divisor) {
    if (divisor == 0xFFFFFFFFu) return 0u;
    return static_cast<u32>(static_cast<s32>(dividend) % static_cast<s32>(divisor));
}
// The original's trap on a zero divisor (`break 7`); the recompiler stops the runtime there.
void divide_by_zero(Runtime &rt, u32 pc) { rt.unsupported(pc, 0x000001CDu, "special? not lowered yet"); }
void copy_words(Runtime &rt, u32 destination, u32 source, u32 count) {
    for (u32 index = 0u; index < count; ++index) put(rt, destination + 4u * index, word(rt, source + 4u * index));
}

// 0x08B4FA78: a blend of two four-word values by the weight f12: the first for 0, the second for 1, else each scaled by
// 0x08B4A6A0 (by 1 - weight and by weight) in a frame copy and combined with 0x08B4A890; the result goes to a0.
void func_08B4FA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(ctx);
    const u32 result = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const float weight = ctx.fpr[12];
    ctx.gpr[17] = result;
    ctx.gpr[16] = second;
    ctx.fpr[1] = 1.0f;
    if (weight == 0.0f) {
        copy_words(rt, result, first, 4u);
        return;
    }
    ctx.fpr[12] = 1.0f - weight;
    if (weight == 1.0f) {
        copy_words(rt, result, second, 4u);
        return;
    }
    const u32 scaled_first = frame.at(0x10u);
    const u32 scaled_second = frame.at(0x0u);
    const u32 sum = frame.at(0x20u);
    copy_words(rt, scaled_first, first, 4u);
    call_float(rt, ctx, 0x08B4A6A0u, {scaled_first}, {1.0f - weight});
    ctx.fpr[12] = weight;
    copy_words(rt, scaled_second, second, 4u);
    call_float(rt, ctx, 0x08B4A6A0u, {scaled_second}, {weight});
    copy_words(rt, sum, scaled_first, 4u);
    call(rt, ctx, 0x08B4A890u, {sum, sum, scaled_second});
    put(rt, result + 4u, word(rt, sum + 4u));
    put(rt, result, word(rt, sum));
    put(rt, result + 0xCu, word(rt, sum + 0xCu));
    put(rt, result + 8u, word(rt, sum + 8u));
}

// 0x08AE4814: packs its arguments into a four-word record in the frame (0x10, a1, then two words of bit fields: clamped values
// shifted into place) and hands it to 0x08AE85E8(halfword +0x1A of a0, record) or, when the stacked flag is set, to
// 0x08AE4568(a0, record). The last three arguments are on the caller's stack.
void func_08AE4814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 incoming = ctx.gpr[29];
    GuestFrame frame(ctx, 0x50u);
    const u32 object = ctx.gpr[4];
    const u32 x = ctx.gpr[5];
    const u32 flag_a = ctx.gpr[6] == 1u ? 1u : 0u;
    const s32 level = static_cast<s32>(ctx.gpr[7]);
    const s32 amount_a = static_cast<s32>(ctx.gpr[8]);
    const s32 amount_b = static_cast<s32>(ctx.gpr[9]);
    const u32 mode_a = ctx.gpr[10];
    const s32 offset = static_cast<s32>(ctx.gpr[11]);
    const u32 stacked_flag = word(rt, incoming);
    const s32 stacked_amount = static_cast<s32>(word(rt, incoming + 4u));
    const u32 stacked_extra = word(rt, incoming + 8u);
    const u32 level_bits = level < 0 ? 0u : static_cast<u32>(std::min<s32>(level, 0x7F));
    const u32 amount_a_bits = amount_a < 0 ? 0u : static_cast<u32>(std::min<s32>(amount_a, 0xF));
    const u32 amount_b_bits = amount_b < 0 ? 0u : static_cast<u32>(std::min<s32>(amount_b, 0xF));
    u32 mode_bits;
    u32 offset_bits;
    if (offset < 0) {
        mode_bits = mode_a == 1u ? 6u : 2u;
        offset_bits = static_cast<u32>(offset + 0x7F);
    } else {
        mode_bits = mode_a == 1u ? 4u : 0u;
        offset_bits = static_cast<u32>(0x7F - offset);
    }
    const u32 flag_b = stacked_flag == 1u ? 1u : 0u;
    const u32 tail_bits = stacked_amount < 0 ? 0u : static_cast<u32>(std::min<s32>(stacked_amount, 0x1F));
    const u32 second_word = (mode_bits << 13) | (offset_bits << 6) | (flag_b << 5) | tail_bits;
    const u32 first_word = (flag_a << 15) | (level_bits << 8) | (amount_a_bits << 4) | amount_b_bits;
    put(rt, frame.at(0u), 0x10u);
    put(rt, frame.at(4u), x);
    put(rt, frame.at(8u), first_word);
    put(rt, frame.at(0xCu), second_word);
    if (stacked_extra != 0u) {
        call(rt, ctx, 0x08AE4568u, {object, frame.at(0u)});
    } else {
        call(rt, ctx, 0x08AE85E8u, {static_cast<u32>(signed_half_at(rt, object + 0x1Au)), frame.at(0u)});
    }
}

// 0x08AD9A04: appends an entry (a1, a2, a3, t0) to the ring of entry a0 (16 bytes each) in the table at 0x08D405D0 and calls
// 0x08AEB808 and 0x08AEB82C; with the table switched off (word +0xC zero) only 0x08AD98F0 runs, and with a full ring the
// message flag at 0x08CDFB3C decides whether 0x08AD83B8 reports it.
void func_08AD9A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    constexpr u32 kTable = 0x08D405D0u;
    const u32 index = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    const u32 ring = word(rt, kTable + 0x18u) + (index << 4);
    ctx.gpr[17] = kTable;
    ctx.gpr[16] = ring;
    ctx.gpr[11] = first;
    ctx.gpr[9] = second;
    ctx.gpr[10] = third;
    ctx.gpr[4] = first;
    ctx.gpr[5] = second;
    ctx.gpr[6] = third;
    ctx.gpr[7] = fourth;
    if (word(rt, kTable + 0xCu) == 0u) {
        call(rt, ctx, 0x08AD98F0u);
        return;
    }
    const u32 capacity = word(rt, kTable + 0x1Cu);
    if (!(static_cast<s32>(word(rt, ring + 4u)) < static_cast<s32>(capacity))) {
        ctx.gpr[2] = 0u;
        ctx.gpr[5] = 0x08C9085Cu;
        ctx.gpr[6] = 0x058Cu;
        ctx.gpr[7] = 0x08C90A34u;
        if ((word(rt, 0x08CDFB3Cu) & 1u) != 0u) call(rt, ctx, 0x08AD83B8u, {word(rt, 0x08CDFB40u)});
        return;
    }
    const u32 slot = word(rt, ring) + (word(rt, ring + 8u) << 4);
    put(rt, slot + 0xCu, fourth);
    put(rt, slot + 4u, second);
    put(rt, slot, first);
    put(rt, slot + 8u, third);
    call(rt, ctx, 0x08AEB808u, {first, second, third, fourth});
    const u32 next = word(rt, ring + 8u) + 1u;
    const u32 limit = word(rt, kTable + 0x1Cu);
    ctx.gpr[2] = next;
    if (limit == 0u) {
        divide_by_zero(rt, 0x08AD9AE0u);
        return;
    }
    put(rt, ring + 4u, word(rt, ring + 4u) + 1u);
    const u32 wrapped = signed_remainder(next, limit);
    put(rt, ring + 8u, wrapped);
    call(rt, ctx, 0x08AEB82Cu, {wrapped});
}

// 0x088F2F90: builds a list of t1 records of 24 bytes (an (x, y) pair from a2, the word a3 for the first two records and t0 for
// the rest, a vector of three words from a1) in the block 0x08B550E0 gives, then 0x08B555F8 takes the block, 0x08B556A0 runs with
// constants and the function ends in a jump to 0x08B55588(4, t1).
void func_088F2F90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 count;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegisters saved(ctx);
        u32 vectors = ctx.gpr[5];
        u32 pairs = ctx.gpr[6];
        const u32 early_word = ctx.gpr[7];
        const u32 late_word = ctx.gpr[8];
        count = ctx.gpr[9];
        ctx.gpr[20] = early_word;
        ctx.gpr[19] = late_word;
        ctx.gpr[18] = count;
        ctx.gpr[17] = pairs;
        ctx.gpr[16] = vectors;
        const u32 block = call(rt, ctx, 0x08B550E0u, {count * 24u});
        u32 record = block;
        for (u32 index = 0u; static_cast<s32>(count) > 0 && index != count; ++index) {
            put(rt, record, word(rt, pairs));
            put(rt, record + 4u, word(rt, pairs + 4u));
            put(rt, record + 8u, index < 2u ? early_word : late_word);
            put(rt, record + 0xCu, word(rt, vectors));
            put(rt, record + 0x10u, word(rt, vectors + 4u));
            put(rt, record + 0x14u, word(rt, vectors + 8u));
            pairs += 8u;
            vectors += 12u;
            record += 24u;
        }
        call(rt, ctx, 0x08B555F8u, {block});
        call(rt, ctx, 0x08B556A0u, {0u, 0u, 0x103u, 0u, 3u, 0x103u});
    }
    tail_call(ctx, 0x08B55588u, {4u, count});
}

// 0x08905AE4: when the object's component (word +0x48, word +0x84 of it) is set, 0x08AF7CB8 takes it; otherwise the number of
// slots is min(4, word +0xC / word +0x10 of the block at +0xA8) and 0x08AF7C8C runs once per slot with a growing offset.
void func_08905AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    const u32 object = ctx.gpr[4];
    const u32 component = word(rt, word(rt, object + 0x48u) + 0x84u);
    ctx.gpr[4] = component;
    if (component != 0u) {
        call(rt, ctx, 0x08AF7CB8u, {component});
        return;
    }
    const u32 block = word(rt, object + 0xA8u);
    ctx.gpr[18] = block;
    u32 dividend = 0u;
    u32 divisor = 0u;
    if (block != 0u) {
        divisor = word(rt, block + 0x10u);
        dividend = word(rt, block + 0xCu);
    }
    ctx.gpr[19] = divisor;
    s32 quotient;
    if (divisor == 0u) quotient = static_cast<s32>(dividend) >= 0 ? -1 : 1;
    else if (divisor == 0xFFFFFFFFu) quotient = static_cast<s32>(0u - dividend);
    else quotient = static_cast<s32>(dividend) / static_cast<s32>(divisor);
    put(rt, frame.at(4u), 4u);
    put(rt, frame.at(0u), static_cast<u32>(quotient));
    const s32 slots = quotient < 4 ? quotient : 4;
    ctx.gpr[2] = quotient < 4 ? frame.at(0u) : frame.at(4u);
    ctx.gpr[20] = static_cast<u32>(slots);
    if (slots <= 0) return;
    const u32 step = divisor * divisor;
    ctx.gpr[21] = step;
    u32 index = 0u;
    u32 offset = 0u;
    do {
        const u32 half = static_cast<u32>(static_cast<s32>(offset + (offset >> 31)) >> 1);
        const u32 slot = block + half;
        const u32 entry = block != 0u ? slot + 0x80u : 0u;
        ctx.gpr[17] = index + 1u;
        ctx.gpr[16] = offset + step;
        ctx.gpr[8] = 8u;
        call(rt, ctx, 0x08AF7C8Cu, {index, entry, divisor, divisor, 8u});
        ++index;
        offset += step;
    } while (index != static_cast<u32>(slots));
}

// 0x089EDF44: the position of a value along a table of widths: the target is the mean of the first (n - 1) widths (words from
// +0xA6C, n = word +0x264) times (float +0x258 - 1); the result is 1 for each whole width the target passes plus the fraction of
// the one it falls in.
void func_089EDF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const s32 count = static_cast<s32>(word(rt, object + 0x264u));
    const s32 widths = count - 1;
    float sum = 0.0f;
    for (s32 index = 0; index < widths; ++index) sum = sum + float_at(rt, object + 0xA6Cu + 4u * static_cast<u32>(index));
    const float mean = sum / static_cast<float>(count);
    const float target = psp_mul(mean, float_at(rt, object + 0x258u) - 1.0f);
    float result = 1.0f;
    float lower = 0.0f;
    for (s32 index = 0; index < widths; ++index) {
        const float width = float_at(rt, object + 0xA6Cu + 4u * static_cast<u32>(index));
        const float upper = width + lower;
        if (target < upper) {
            result = result + (target - lower) / width;
            break;
        }
        lower = upper;
        result = result + 1.0f;
    }
    ctx.fpr[0] = result;
}

// The search over an array that unrolls by four: the first element at `first` (elements of 1 << shift bytes) that `matches`,
// else `last`. Fewer than four elements are looked at by their count (0 or less: none).
template <unsigned kShift, typename Match>
u32 search_unrolled(u32 first, u32 last, Match matches) {
    const s32 blocks = static_cast<s32>(last - first) >> (kShift + 2u);
    u32 cursor = first;
    for (s32 block = 0; block < blocks; ++block) {
        for (u32 lane = 0u; lane < 4u; ++lane) {
            if (matches(cursor)) return cursor;
            cursor += 1u << kShift;
        }
    }
    const s32 remaining = static_cast<s32>(last - cursor) >> kShift;
    for (s32 index = 0; index < remaining && index < 3; ++index) {
        if (matches(cursor)) return cursor;
        cursor += 1u << kShift;
    }
    return last;
}

// 0x08BB1180: the first word in [a0, a1) equal to the word a2 points at.
void func_08BB1180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 value_address = ctx.gpr[6];
    ctx.gpr[2] = search_unrolled<2u>(ctx.gpr[4], ctx.gpr[5], [&](u32 element) { return word(rt, element) == word(rt, value_address); });
}
// 0x08C40938: the first pointer in [a0, a1) whose target has the halfword a2 at +4.
void func_08C40938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 wanted = ctx.gpr[6];
    ctx.gpr[2] = search_unrolled<2u>(ctx.gpr[4], ctx.gpr[5], [&](u32 element) { return half_at(rt, word(rt, element) + 4u) == wanted; });
}
// 0x08C3FC14: the first element of 8 bytes in [a0, a1) whose word at the offset (word +4 of a2) equals the word a2 points at.
void func_08C3FC14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 wanted = word(rt, ctx.gpr[6]);
    const u32 offset = word(rt, ctx.gpr[6] + 4u);
    ctx.gpr[2] = search_unrolled<3u>(ctx.gpr[4], ctx.gpr[5], [&](u32 element) { return word(rt, element + offset) == wanted; });
}

// A record conversion by byte tables: runs of bytes copied from the first source (kind 0) or the second (kind 2) to the
// destination, and constant bytes (kind 1: the value is in `source`). 31 runs, 127 bytes; the same for both functions.
struct ByteRun {
    std::uint8_t destination;
    std::uint8_t source;
    std::uint8_t count;
    std::uint8_t kind;
};
constexpr ByteRun kByteRuns[] = {
    {0, 1, 1, 0}, {2, 10, 1, 0}, {8, 8, 1, 1}, {1, 0, 1, 0}, {16, 11, 15, 0}, {9, 6, 1, 1}, {31, 26, 1, 0}, {32, 39, 6, 0}, {48, 45, 6, 0}, {64, 57, 5, 0}, {10, 6, 1, 1}, {69, 62, 1, 0}, {40, 63, 6, 0}, {56, 69, 6, 0}, {72, 81, 5, 0}, {11, 4, 1, 1}, {77, 86, 1, 0}, {92, 2, 7, 0}, {12, 6, 1, 1}, {99, 9, 1, 0}, {80, 27, 12, 0}, {3, 87, 2, 0}, {15, 8, 1, 1}, {5, 89, 2, 0}, {148, 91, 16, 0}, {164, 115, 8, 0}, {100, 0, 10, 2}, {110, 13, 5, 2}, {120, 10, 1, 2}, {121, 12, 1, 2}, {122, 11, 1, 2}
};
// Returns the last byte read, which the original leaves in v0.
u32 convert_record(Runtime &rt, u32 destination, u32 first_source, u32 second_source) {
    u32 last_read = 0u;
    for (const ByteRun &run : kByteRuns) {
        for (u32 index = 0u; index < run.count; ++index) {
            if (run.kind == 1u) {
                set_byte(rt, destination + run.destination + index, run.source);
            } else {
                last_read = byte_at(rt, (run.kind == 0u ? first_source : second_source) + run.source + index);
                set_byte(rt, destination + run.destination + index, last_read);
            }
        }
    }
    return last_read;
}
// 0x0892C0C8(dest, source, second source) and 0x0892DD3C(object, dest, source, second source).
void func_0892C0C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = convert_record(rt, ctx.gpr[4], ctx.gpr[5], ctx.gpr[6]); }
void func_0892DD3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = convert_record(rt, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]); }

// 0x0893AA4C: decodes the record at a0 (bytes from +0x10 and +0x0 and a few at +0x30) into the float table at 0x08CF2990 (values
// scaled by constants, two of them products).
void func_0893AA4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kTable = 0x08CF2990u;
    const u32 record = ctx.gpr[4];
    const u32 byte_1 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x1u));
    const float value_2 = std::bit_cast<float>(0x41200000u);
    const float value_3 = std::bit_cast<float>(0x42C80000u);
    const u32 byte_5 = static_cast<u32>(byte_at(rt, record + 0x10u));
    const float value_6 = std::bit_cast<float>(0x4031C71Du);
    const float value_7 = static_cast<float>(static_cast<s32>(byte_1));
    const float value_9 = value_7 / value_2;
    const float value_10 = static_cast<float>(static_cast<s32>(byte_5));
    const float value_11 = value_10 / value_2;
    const float value_12 = value_9 / value_3;
    const float value_13 = psp_mul(value_11, value_12);
    const float value_14 = psp_mul(value_12, value_13);
    put_float(rt, kTable + 0x0u, value_14);
    const u32 byte_15 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x2u));
    const float value_17 = static_cast<float>(static_cast<s32>(byte_15));
    const float value_18 = psp_mul(value_17, value_6);
    put_float(rt, kTable + 0x4u, value_18);
    const u32 byte_19 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x3u));
    const float value_21 = static_cast<float>(static_cast<s32>(byte_19));
    const float value_22 = psp_mul(value_21, value_6);
    put_float(rt, kTable + 0x8u, value_22);
    const u32 byte_23 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x4u));
    const float value_25 = static_cast<float>(static_cast<s32>((byte_23 * 100u)));
    put_float(rt, kTable + 0xCu, value_25);
    const u32 byte_26 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x5u));
    const float value_28 = static_cast<float>(static_cast<s32>((byte_26 * 100u)));
    put_float(rt, kTable + 0x10u, value_28);
    const u32 byte_29 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x6u));
    const float value_31 = static_cast<float>(static_cast<s32>(byte_29));
    const float value_32 = std::bit_cast<float>(0x411CCCCDu);
    const float value_33 = value_31 / value_2;
    put_float(rt, kTable + 0x14u, value_33);
    const u32 byte_34 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x7u));
    const float value_36 = static_cast<float>(static_cast<s32>(byte_34));
    const float value_37 = value_36 / value_3;
    put_float(rt, kTable + 0x18u, value_37);
    const u32 byte_38 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0x9u));
    const float value_40 = static_cast<float>(static_cast<s32>(byte_38));
    const float value_41 = value_40 / value_2;
    const float value_42 = value_41 / value_3;
    const float value_43 = psp_mul(value_11, value_42);
    const float value_44 = psp_mul(value_42, value_43);
    put_float(rt, kTable + 0x1Cu, value_44);
    const u32 byte_45 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0xAu));
    const float value_47 = static_cast<float>(static_cast<s32>(byte_45));
    const float value_48 = psp_mul(value_47, value_6);
    put_float(rt, kTable + 0x20u, value_48);
    const u32 byte_49 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0xBu));
    const float value_51 = static_cast<float>(static_cast<s32>(byte_49));
    const float value_52 = psp_mul(value_51, value_6);
    put_float(rt, kTable + 0x24u, value_52);
    const u32 byte_53 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0xCu));
    const float value_55 = static_cast<float>(static_cast<s32>(byte_53));
    const float value_56 = value_55 / value_3;
    put_float(rt, kTable + 0x28u, value_56);
    const u32 byte_57 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0xDu));
    const float value_59 = static_cast<float>(static_cast<s32>(byte_57));
    const float value_60 = value_59 / value_3;
    put_float(rt, kTable + 0x2Cu, value_60);
    const u32 byte_61 = static_cast<u32>(byte_at(rt, (record + 0x10u) + 0xEu));
    const float value_63 = static_cast<float>(static_cast<s32>(byte_61));
    const float value_64 = value_63 / value_2;
    put_float(rt, kTable + 0x30u, value_64);
    const u32 byte_65 = static_cast<u32>(byte_at(rt, record + 0x8u));
    const float value_67 = static_cast<float>(static_cast<s32>(byte_65));
    const float value_68 = value_67 / value_3;
    put_float(rt, kTable + 0x34u, value_68);
    const u32 byte_69 = static_cast<u32>(byte_at(rt, record + 0x9u));
    const float value_71 = static_cast<float>(static_cast<s32>(byte_69));
    const float value_72 = value_71 / value_3;
    put_float(rt, kTable + 0x38u, value_72);
    const u32 byte_73 = static_cast<u32>(byte_at(rt, record + 0xAu));
    const float value_75 = static_cast<float>(static_cast<s32>(byte_73));
    const float value_76 = value_75 / value_3;
    put_float(rt, kTable + 0x3Cu, value_76);
    const u32 byte_77 = static_cast<u32>(byte_at(rt, record + 0xBu));
    const float value_79 = static_cast<float>(static_cast<s32>(byte_77));
    const float value_80 = psp_mul(value_79, value_6);
    put_float(rt, kTable + 0x40u, value_80);
    const u32 byte_81 = static_cast<u32>(byte_at(rt, record + 0xCu));
    const float value_83 = static_cast<float>(static_cast<s32>(byte_81));
    const float value_84 = psp_mul(value_83, value_6);
    put_float(rt, kTable + 0x44u, value_84);
    const u32 byte_85 = static_cast<u32>(byte_at(rt, record + 0xDu));
    const float value_87 = static_cast<float>(static_cast<s32>(byte_85));
    const float value_88 = value_87 / value_3;
    put_float(rt, kTable + 0x48u, value_88);
    const u32 byte_89 = static_cast<u32>(byte_at(rt, record + 0xEu));
    const float value_91 = static_cast<float>(static_cast<s32>(byte_89));
    const float value_92 = value_91 / value_3;
    put_float(rt, kTable + 0x4Cu, value_92);
    const u32 byte_93 = static_cast<u32>(byte_at(rt, record + 0xFu));
    const float value_95 = static_cast<float>(static_cast<s32>(byte_93));
    const float value_96 = value_95 / value_2;
    put_float(rt, kTable + 0x50u, value_96);
    const u32 byte_97 = static_cast<u32>(byte_at(rt, record + 0x0u));
    const float value_99 = static_cast<float>(static_cast<s32>(byte_97));
    const float value_100 = value_99 / value_3;
    put_float(rt, kTable + 0x54u, value_100);
    const u32 byte_101 = static_cast<u32>(byte_at(rt, record + 0x1u));
    const float value_103 = static_cast<float>(static_cast<s32>(byte_101));
    const float value_104 = value_103 / value_3;
    put_float(rt, kTable + 0x58u, value_104);
    const u32 byte_105 = static_cast<u32>(byte_at(rt, record + 0x2u));
    const float value_107 = static_cast<float>(static_cast<s32>(byte_105));
    const float value_108 = psp_mul(value_107, value_32);
    put_float(rt, kTable + 0x5Cu, value_108);
    const u32 byte_109 = static_cast<u32>(byte_at(rt, record + 0x4u));
    const u32 byte_111 = static_cast<u32>(byte_at(rt, record + 0x3u));
    const float value_112 = static_cast<float>(static_cast<s32>(byte_109));
    const float value_114 = std::bit_cast<float>(0x4482E653u);
    const float value_115 = static_cast<float>(static_cast<s32>(byte_111));
    const float value_116 = value_112 / value_2;
    const float value_117 = value_115 / value_2;
    put_float(rt, kTable + 0x64u, value_116);
    const float value_118 = value_117 - value_116;
    const float value_119 = value_118 / value_114;
    put_float(rt, kTable + 0x60u, value_119);
    const u32 byte_120 = static_cast<u32>(byte_at(rt, record + 0x30u));
    const float value_122 = static_cast<float>(static_cast<s32>(byte_120));
    const float value_123 = value_122 / value_3;
    put_float(rt, kTable + 0x68u, value_123);
    const u32 byte_124 = static_cast<u32>(byte_at(rt, record + 0x31u));
    const float value_126 = static_cast<float>(static_cast<s32>(byte_124));
    const float value_127 = value_126 / value_3;
    put_float(rt, kTable + 0x6Cu, value_127);
    const u32 byte_128 = static_cast<u32>(byte_at(rt, record + 0x32u));
    const float value_130 = static_cast<float>(static_cast<s32>(byte_128));
    const float value_131 = value_130 / value_3;
    put_float(rt, kTable + 0x70u, value_131);
    const u32 byte_132 = static_cast<u32>(byte_at(rt, record + 0x33u));
    const float value_134 = static_cast<float>(static_cast<s32>(byte_132));
    const float value_135 = value_134 / value_2;
    put_float(rt, kTable + 0x74u, value_135);
    const u32 byte_136 = static_cast<u32>(byte_at(rt, record + 0x3Au));
    const float value_138 = static_cast<float>(static_cast<s32>(byte_136));
    const float value_139 = value_138 / value_3;
    put_float(rt, kTable + 0x78u, value_139);
    const u32 byte_140 = static_cast<u32>(byte_at(rt, record + 0x3Cu));
    const float value_142 = static_cast<float>(static_cast<s32>(byte_140));
    const float value_143 = value_142 / value_3;
    put_float(rt, kTable + 0x7Cu, value_143);
    const u32 byte_144 = static_cast<u32>(byte_at(rt, record + 0x3Du));
    const float value_146 = static_cast<float>(static_cast<s32>(byte_144));
    const float value_147 = value_146 / value_3;
    put_float(rt, kTable + 0x80u, value_147);
    ctx.gpr[2] = byte_140;   // the last byte the original loads into v0
}

// 0x088CEA7C: decodes the record at a1 (bytes) into the structure at a0 (floats from +0x14, flag and byte copies from +0x34,
// halfwords and words as multiples of the bytes).
void func_088CEA7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 dest = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    const u32 byte_1 = static_cast<u32>(byte_at(rt, src + 0x0u));
    const float value_2 = std::bit_cast<float>(0x3C8EFA35u);
    const float value_3 = std::bit_cast<float>(0x3C23D70Au);
    const float value_5 = std::bit_cast<float>(0x3E8E38E4u);
    const float value_6 = std::bit_cast<float>(0x41200000u);
    const float value_7 = static_cast<float>(static_cast<s32>(byte_1));
    const float value_8 = std::bit_cast<float>(0x3DCCCCCDu);
    const float value_9 = psp_mul(value_7, value_2);
    put_float(rt, dest + 0x14u, value_9);
    const u32 byte_10 = static_cast<u32>(byte_at(rt, src + 0x1u));
    const float value_12 = static_cast<float>(static_cast<s32>(byte_10));
    const float value_13 = psp_mul(value_12, value_3);
    put_float(rt, dest + 0x18u, value_13);
    const u32 byte_14 = static_cast<u32>(byte_at(rt, src + 0x2u));
    const float value_16 = static_cast<float>(static_cast<s32>(byte_14));
    const float value_17 = psp_mul(value_16, value_2);
    put_float(rt, dest + 0x1Cu, value_17);
    const u32 byte_18 = static_cast<u32>(byte_at(rt, src + 0x3u));
    const float value_20 = static_cast<float>(static_cast<s32>(byte_18));
    const float value_21 = psp_mul(value_20, value_2);
    put_float(rt, dest + 0x20u, value_21);
    const u32 byte_22 = static_cast<u32>(byte_at(rt, src + 0x4u));
    const float value_24 = static_cast<float>(static_cast<s32>(byte_22));
    const float value_25 = psp_mul(value_24, value_2);
    put_float(rt, dest + 0x24u, value_25);
    const u32 byte_26 = static_cast<u32>(byte_at(rt, src + 0x5u));
    const float value_28 = static_cast<float>(static_cast<s32>(byte_26));
    const float value_29 = psp_mul(value_28, value_2);
    put_float(rt, dest + 0x28u, value_29);
    const u32 byte_30 = static_cast<u32>(byte_at(rt, src + 0x6u));
    const float value_32 = static_cast<float>(static_cast<s32>(byte_30));
    const float value_33 = psp_mul(value_32, value_2);
    put_float(rt, dest + 0x2Cu, value_33);
    const u32 byte_34 = static_cast<u32>(byte_at(rt, src + 0x7u));
    const float value_36 = static_cast<float>(static_cast<s32>(byte_34));
    const float value_37 = psp_mul(value_36, value_2);
    put_float(rt, dest + 0x30u, value_37);
    const u32 byte_38 = static_cast<u32>(byte_at(rt, src + 0x8u));
    const u32 byte_39 = byte_38 != 0u ? 1u : 0u;
    set_byte(rt, dest + 0x34u, byte_39);
    const u32 byte_40 = static_cast<u32>(byte_at(rt, src + 0x9u));
    const float value_42 = static_cast<float>(static_cast<s32>(byte_40));
    const float value_43 = psp_mul(value_42, value_5);
    put_float(rt, dest + 0x38u, value_43);
    const u32 byte_44 = static_cast<u32>(byte_at(rt, src + 0xAu));
    const float value_46 = static_cast<float>(static_cast<s32>(byte_44));
    const float value_47 = psp_mul(value_46, value_3);
    put_float(rt, dest + 0x3Cu, value_47);
    const u32 byte_48 = static_cast<u32>(byte_at(rt, src + 0xBu));
    const float value_50 = static_cast<float>(static_cast<s32>(byte_48));
    const float value_51 = psp_mul(value_50, value_5);
    put_float(rt, dest + 0x40u, value_51);
    const u32 byte_52 = static_cast<u32>(byte_at(rt, src + 0xCu));
    set_byte(rt, dest + 0x44u, byte_52);
    const u32 byte_53 = static_cast<u32>(byte_at(rt, src + 0xDu));
    set_byte(rt, dest + 0x45u, byte_53);
    const u32 byte_54 = static_cast<u32>(byte_at(rt, src + 0xEu));
    set_byte(rt, dest + 0x46u, byte_54);
    const u32 byte_55 = static_cast<u32>(byte_at(rt, src + 0xFu));
    set_byte(rt, dest + 0x47u, byte_55);
    const u32 byte_56 = static_cast<u32>(byte_at(rt, src + 0x10u));
    set_byte(rt, dest + 0x48u, byte_56);
    const u32 byte_57 = static_cast<u32>(byte_at(rt, src + 0x11u));
    set_byte(rt, dest + 0x49u, byte_57);
    const u32 byte_58 = static_cast<u32>(byte_at(rt, src + 0x13u));
    set_half(rt, dest + 0x4Au, (byte_58 * 60u));
    const u32 byte_59 = static_cast<u32>(byte_at(rt, src + 0x15u));
    set_half(rt, dest + 0x4Cu, (byte_59 * 60u));
    const u32 byte_60 = static_cast<u32>(byte_at(rt, src + 0x17u));
    set_half(rt, dest + 0x4Eu, (byte_60 * 60u));
    const u32 byte_61 = static_cast<u32>(byte_at(rt, src + 0x12u));
    const float value_63 = static_cast<float>(static_cast<s32>(byte_61));
    const float value_64 = value_63 / value_6;
    put_float(rt, dest + 0x50u, value_64);
    const u32 byte_65 = static_cast<u32>(byte_at(rt, src + 0x14u));
    const float value_67 = static_cast<float>(static_cast<s32>(byte_65));
    const float value_68 = value_67 / value_6;
    put_float(rt, dest + 0x54u, value_68);
    const u32 byte_69 = static_cast<u32>(byte_at(rt, src + 0x16u));
    const float value_71 = static_cast<float>(static_cast<s32>(byte_69));
    const float value_72 = value_71 / value_6;
    put_float(rt, dest + 0x58u, value_72);
    const u32 byte_73 = static_cast<u32>(byte_at(rt, src + 0x18u));
    put(rt, dest + 0x5Cu, (byte_73 * 100u));
    const u32 byte_74 = static_cast<u32>(byte_at(rt, src + 0x19u));
    put(rt, dest + 0x60u, (byte_74 * 6u));
    const u32 byte_75 = static_cast<u32>(byte_at(rt, src + 0x1Au));
    put(rt, dest + 0x64u, (byte_75 * 60u));
    const u32 byte_76 = static_cast<u32>(byte_at(rt, src + 0x1Bu));
    put(rt, dest + 0x68u, (byte_76 * 60u));
    const u32 byte_77 = static_cast<u32>(byte_at(rt, src + 0x1Cu));
    put(rt, dest + 0x6Cu, (byte_77 * 6u));
    const u32 byte_78 = static_cast<u32>(byte_at(rt, src + 0x1Du));
    put(rt, dest + 0x70u, (byte_78 * 10u));
    const u32 byte_79 = static_cast<u32>(byte_at(rt, src + 0x1Eu));
    put(rt, dest + 0x74u, (byte_79 * 10u));
    const u32 byte_80 = static_cast<u32>(byte_at(rt, src + 0x1Fu));
    put(rt, dest + 0x78u, (byte_80 * 10u));
    const u32 byte_81 = static_cast<u32>(byte_at(rt, src + 0x20u));
    const float value_83 = static_cast<float>(static_cast<s32>(byte_81));
    const float value_84 = psp_mul(value_83, value_8);
    put_float(rt, dest + 0x7Cu, value_84);
    const u32 byte_85 = static_cast<u32>(byte_at(rt, src + 0x21u));
    const float value_87 = static_cast<float>(static_cast<s32>(byte_85));
    const float value_88 = psp_mul(value_87, value_3);
    put_float(rt, dest + 0x80u, value_88);
    const u32 byte_89 = static_cast<u32>(byte_at(rt, src + 0x22u));
    const float value_91 = static_cast<float>(static_cast<s32>(byte_89));
    const float value_92 = psp_mul(value_91, value_8);
    put_float(rt, dest + 0x84u, value_92);
    const u32 byte_93 = static_cast<u32>(byte_at(rt, src + 0x23u));
    const float value_95 = static_cast<float>(static_cast<s32>(byte_93));
    const float value_96 = psp_mul(value_95, value_3);
    put_float(rt, dest + 0x88u, value_96);
    const u32 byte_97 = static_cast<u32>(byte_at(rt, src + 0x24u));
    const float value_99 = static_cast<float>(static_cast<s32>(byte_97));
    const float value_100 = psp_mul(value_99, value_8);
    put_float(rt, dest + 0x8Cu, value_100);
    const u32 byte_101 = static_cast<u32>(byte_at(rt, src + 0x25u));
    const float value_103 = static_cast<float>(static_cast<s32>(byte_101));
    const float value_104 = psp_mul(value_103, value_3);
    put_float(rt, dest + 0x90u, value_104);
    ctx.gpr[2] = byte_97;   // the last byte the original loads into v0
}

// 0x08960BE8: evaluates the curve at a0 (kind word 0: 1 quadratic, 2 a curve through 0x08B49D7C, 3 an eighth-power one, else
// the identity) at |f12|, mirrored for a negative f12; when a1 is non-zero the slope goes to its word.
void func_08960BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 curve = ctx.gpr[4];
    const u32 slope_out = ctx.gpr[5];
    const float x = ctx.fpr[12];
    ctx.gpr[17] = slope_out;
    ctx.gpr[16] = curve;
    const float magnitude = x < 0.0f ? -x : x;
    float value;
    switch (word(rt, curve)) {
    case 1u: {
        const float centre = float_at(rt, curve + 4u);
        const float scale = float_at(rt, curve + 0xCu);
        const float offset = float_at(rt, curve + 8u);
        const float delta = magnitude - centre;
        const float square = psp_mul(delta, scale);
        value = psp_mul(delta, square) + offset;
        if (slope_out != 0u) put_float(rt, slope_out, psp_mul(delta, scale + scale));
        break;
    }
    case 2u: {
        const float shaped = call_float(rt, ctx, 0x08B49D7Cu, {}, {magnitude - float_at(rt, curve + 4u)});
        const float scale = float_at(rt, curve + 0xCu);
        const float offset = float_at(rt, curve + 8u);
        value = psp_mul(shaped, scale) + offset;
        if (slope_out != 0u) {
            const float divisor = shaped < std::bit_cast<float>(0x3C23D70Au) ? std::bit_cast<float>(0x3CA3D70Au) : shaped + shaped;
            put_float(rt, slope_out, scale / divisor);
        }
        break;
    }
    case 3u: {
        const float scale = float_at(rt, curve + 0xCu);
        const float offset = float_at(rt, curve + 8u);
        const float square = psp_mul(magnitude, magnitude);
        const float fourth = psp_mul(square, square);
        const float linear = psp_mul(magnitude, offset);
        const float cube = psp_mul(magnitude, square);
        const float high = psp_mul(fourth, psp_mul(fourth, scale));
        value = high + linear;
        if (slope_out != 0u) put_float(rt, slope_out, offset + psp_mul(cube, psp_mul(fourth, psp_mul(scale, std::bit_cast<float>(0x41000000u)))));
        break;
    }
    default:
        value = magnitude;
        if (slope_out != 0u) put_float(rt, slope_out, 1.0f);
        break;
    }
    if (x < 0.0f) value = -value;
    ctx.fpr[0] = value;
}

// 0x08B6ECC4: x * 2^n for the float f12 and the integer a0: zero, infinities and NaNs come back (doubled
// for the last two), a subnormal is scaled up by 2^25 first, the exponent field is rewritten when the result is normal, a result
// below the smallest normal is built by scaling the exponent (+ 25) and a huge or tiny one goes through 0x08B6EE30 with a constant.
void func_08B6ECC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    float x = ctx.fpr[12];
    const s32 n = static_cast<s32>(ctx.gpr[4]);
    const auto finish = [&](float value) {
        ctx.fpr[12] = value;
        ctx.fpr[0] = value;
    };
    u32 pattern = std::bit_cast<u32>(x);
    const u32 magnitude = pattern & 0x7FFFFFFFu;
    if (magnitude == 0u) {
        ctx.fpr[0] = x;
        return;
    }
    const u32 exponent = magnitude >> 23;
    if (magnitude > 0x7F7FFFFFu) {
        finish(x + x);
        return;
    }
    s32 combined;
    if (magnitude > 0x007FFFFFu) {
        combined = static_cast<s32>(exponent) + n;
    } else {
        x = psp_mul(x, float_at(rt, 0x08CB65BCu));
        ctx.fpr[12] = x;
        pattern = std::bit_cast<u32>(x);
        const s32 scaled_exponent = static_cast<s32>((pattern >> 23) & 0xFFu) - 25;
        if (n < -50000) {
            finish(psp_mul(x, float_at(rt, 0x08CB65C0u)));
            return;
        }
        combined = scaled_exponent + n;
    }
    bool overflow = false;
    float constant = 0.0f;
    if (combined < 255) {
        if (combined > 0) {
            pattern = (pattern & ~(0xFFu << 23)) | (static_cast<u32>(combined) << 23);
            finish(std::bit_cast<float>(pattern));
            return;
        }
        if (combined < -22) {
            overflow = 50000 < n;
            constant = float_at(rt, overflow ? 0x08CB65C4u : 0x08CB65C0u);
        } else {
            pattern = (pattern & ~(0xFFu << 23)) | (static_cast<u32>(combined + 25) << 23);
            finish(psp_mul(std::bit_cast<float>(pattern), float_at(rt, 0x08CB65C8u)));
            return;
        }
    } else {
        constant = float_at(rt, 0x08CB65C4u);
    }
    ctx.fpr[20] = constant;
    ctx.fpr[13] = x;
    const float adjusted = call_float(rt, ctx, 0x08B6EE30u, {}, {constant, x});
    finish(psp_mul(adjusted, constant));
}

} // namespace
} // namespace hot_batch_343
} // namespace gt::game

#define HOT343(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_343::name, gt::ReturnKind::kind)
HOT343(0x08B4FA78u, func_08B4FA78, Word);
HOT343(0x08AE4814u, func_08AE4814, Word);
HOT343(0x08AD9A04u, func_08AD9A04, Word);
HOT343(0x088F2F90u, func_088F2F90, Word);
HOT343(0x08905AE4u, func_08905AE4, Word);
HOT343(0x089EDF44u, func_089EDF44, Float);
HOT343(0x08BB1180u, func_08BB1180, Word);
HOT343(0x08C40938u, func_08C40938, Word);
HOT343(0x08C3FC14u, func_08C3FC14, Word);
HOT343(0x0892C0C8u, func_0892C0C8, Word);
HOT343(0x0892DD3Cu, func_0892DD3C, Word);
HOT343(0x0893AA4Cu, func_0893AA4C, Word);
HOT343(0x088CEA7Cu, func_088CEA7C, Word);
HOT343(0x08960BE8u, func_08960BE8, Float);
HOT343(0x08B6ECC4u, func_08B6ECC4, Float);
