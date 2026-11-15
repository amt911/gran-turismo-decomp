// Hot batch 344: functions still recompiled (picked with `tools.next_leaves --ready`), written as structured C from
// `tools.disasm` and verified by synthetic cases: structure copies and (de)serialisers whose block copies read a chunk of
// 16 bytes and write it before the next one, a slot swap, a decimal-digit accumulator.
// The callee-saved registers are set where the asm sets them, because a callee may read them, and put back at the end (the
// original saves and restores them). `v0` ends as the asm leaves it: the last call's result or a loaded value.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"
#include "gt/vfpu.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_344 {
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

// One block of a structure copy: the original reads `size` bytes (a chunk of 16 at most, as word loads or lwl / lwr pairs)
// and only then writes them, so a source and destination that overlap see the chunks in this order.
// Returns the first word of the chunk, which the original loads into v0.
u32 copy_chunk(Runtime &rt, u32 destination, u32 source, u32 size) {
    std::uint8_t bytes[16];
    for (u32 index = 0u; index < size; ++index) bytes[index] = static_cast<std::uint8_t>(byte_at(rt, source + index));
    for (u32 index = 0u; index < size; ++index) set_byte(rt, destination + index, bytes[index]);
    return size >= 4u ? static_cast<u32>(bytes[0]) | static_cast<u32>(bytes[1]) << 8 | static_cast<u32>(bytes[2]) << 16 | static_cast<u32>(bytes[3]) << 24 : 0u;
}
// `size` bytes in chunks of 16 (a size that is not a multiple of 16 ends in a smaller chunk).
u32 copy_chunks(Runtime &rt, u32 destination, u32 source, u32 size) {
    u32 first_word = 0u;
    for (u32 offset = 0u; offset < size; offset += 16u) first_word = copy_chunk(rt, destination + offset, source + offset, size - offset < 16u ? size - offset : 16u);
    return first_word;
}
// Two bytes, one after the other.
void copy_pair(Runtime &rt, u32 destination, u32 source) {
    set_byte(rt, destination, byte_at(rt, source));
    set_byte(rt, destination + 1u, byte_at(rt, source + 1u));
}

// 0x088972BC: 0x08896D94 on the object's field +8, then the 0xB8 bytes of a1 go to the block the object's first word points at (in
// the chunks the original reads: 0x8C, 16, 4, 16 and 8 bytes after the first 0x80); returns a1 + 0xC0.
void func_088972BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 object = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    ctx.gpr[17] = source;
    ctx.gpr[16] = object;
    call(rt, ctx, 0x08896D94u, {object + 8u});
    const u32 destination = word(rt, object);
    ctx.gpr[16] = destination;
    copy_chunks(rt, destination, source, 0x8Cu);
    copy_chunk(rt, destination + 0x8Cu, source + 0x8Cu, 16u);
    copy_chunk(rt, destination + 0x9Cu, source + 0x9Cu, 4u);
    copy_chunk(rt, destination + 0xA0u, source + 0xA0u, 16u);
    copy_chunk(rt, destination + 0xB0u, source + 0xB0u, 8u);
    ctx.gpr[2] = source + 0xC0u;
}

// 0x088D5588: copies 0x116 bytes from a1 to the block the word at a0 points at (read again before each of the three stages: 0x60 and
// 0x20 bytes, two bytes, 0x90 bytes, four bytes), then 0x088D47EC answers a size and the result is a1 + that size.
void func_088D5588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 holder = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    ctx.gpr[16] = source;
    copy_chunks(rt, word(rt, holder), source, 0x60u);
    u32 destination = word(rt, holder);
    copy_chunks(rt, destination + 0x60u, source + 0x60u, 0x20u);
    copy_pair(rt, destination + 0x80u, source + 0x80u);
    destination = word(rt, holder);
    copy_chunks(rt, destination + 0x82u, source + 0x82u, 0x90u);
    copy_chunk(rt, destination + 0x112u, source + 0x112u, 4u);
    const u32 size = call(rt, ctx, 0x088D47ECu);
    ctx.gpr[2] = source + size;
}

// 0x08B08B60: the copy of a structure a1 into a0: scalar fields first (bytes +8 / +9 / +0xA / +0x38..+0x3A, words +0 / +4 / +0xC,
// halfwords +0x3C / +0x1A4), 0x08B09298 on the parts at +0x10, then the word +0x40, 0xA0 bytes from +0x44, 0xC0 bytes from +0xE4 and
// the word +0x1A8; returns that last word.
void func_08B08B60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    ctx.gpr[17] = destination;
    ctx.gpr[16] = source;
    const u32 byte_8 = byte_at(rt, source + 8u);
    const u32 word_0 = word(rt, source);
    const u32 word_4 = word(rt, source + 4u);
    const u32 byte_9 = byte_at(rt, source + 9u);
    const u32 byte_a = byte_at(rt, source + 0xAu);
    const u32 word_c = word(rt, source + 0xCu);
    const u32 byte_38 = byte_at(rt, source + 0x38u);
    const u32 byte_39 = byte_at(rt, source + 0x39u);
    const u32 byte_3a = byte_at(rt, source + 0x3Au);
    const u32 half_1a4 = half_at(rt, source + 0x1A4u);
    const u32 half_3c = half_at(rt, source + 0x3Cu);
    set_byte(rt, destination + 8u, byte_8);
    put(rt, destination, word_0);
    put(rt, destination + 4u, word_4);
    set_byte(rt, destination + 9u, byte_9);
    set_byte(rt, destination + 0xAu, byte_a);
    put(rt, destination + 0xCu, word_c);
    set_byte(rt, destination + 0x38u, byte_38);
    set_byte(rt, destination + 0x39u, byte_39);
    set_byte(rt, destination + 0x3Au, byte_3a);
    set_half(rt, destination + 0x1A4u, half_1a4);
    set_half(rt, destination + 0x3Cu, half_3c);
    ctx.gpr[4] = destination + 0x10u;
    ctx.gpr[5] = source + 0x10u;
    call(rt, ctx, 0x08B09298u, {destination + 0x10u, source + 0x10u});
    put(rt, destination + 0x40u, word(rt, source + 0x40u));
    copy_chunks(rt, destination + 0x44u, source + 0x44u, 0xA0u);
    copy_chunks(rt, destination + 0xE4u, source + 0xE4u, 0xC0u);
    const u32 last = word(rt, source + 0x1A8u);
    put(rt, destination + 0x1A8u, last);
    ctx.gpr[2] = last;
}

// 0x08B81488: builds a number from decimal digits: 0x08B82260(a0, k) makes the accumulator (k = ceil(log2((a3 + 8) / 9)), its fields
// +0x14 = t0 and +0x10 = 1), then 0x08B81378(a0, accumulator, 10, digit) takes each digit of the string a1: those from index 9 up to
// a2 (when a2 >= 10), then, after one skipped character, the remaining a3 - 9 (or a3 - a2) of them. Returns the accumulator.
void func_08B81488(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 context = ctx.gpr[4];
    const u32 digits = ctx.gpr[5];
    const s32 length = static_cast<s32>(ctx.gpr[6]);
    const s32 total = static_cast<s32>(ctx.gpr[7]);
    const u32 extra = ctx.gpr[8];
    ctx.gpr[18] = context;
    ctx.gpr[21] = digits;
    ctx.gpr[19] = static_cast<u32>(length);
    ctx.gpr[20] = static_cast<u32>(total);
    ctx.gpr[16] = extra;
    const s32 sum = total + 8;
    const s32 quotient = (static_cast<s32>((static_cast<std::int64_t>(sum) * static_cast<std::int32_t>(0x38E38E39)) >> 32) >> 1) - (sum >> 31);
    u32 shift_count = 0u;
    if (!(quotient < 2)) {
        s32 power = 1;
        do {
            power = power << 1;
            shift_count = shift_count + 1u;
        } while (power < quotient);
    }
    const u32 accumulator = call(rt, ctx, 0x08B82260u, {context, shift_count});
    put(rt, accumulator + 0x14u, extra);
    put(rt, accumulator + 0x10u, 1u);
    u32 value = accumulator;
    s32 consumed = 9;
    u32 cursor = digits + 0xAu;
    ctx.gpr[17] = 9u;
    if (!(length < 10)) {
        u32 pointer = digits + 9u;
        do {
            ctx.gpr[16] = pointer;
            value = call(rt, ctx, 0x08B81378u, {context, value, 10u, static_cast<u32>(signed_byte_at(rt, pointer)) - 0x30u});
            consumed = consumed + 1;
            ctx.gpr[17] = static_cast<u32>(consumed);
            if (consumed == length) {
                cursor = pointer + 2u;
                break;
            }
            pointer = pointer + 1u;
        } while (true);
    }
    if (consumed < total) {
        const u32 end = cursor + static_cast<u32>(total - consumed);
        ctx.gpr[17] = end;
        for (u32 pointer = cursor; pointer != end; ++pointer) {
            ctx.gpr[16] = pointer;
            value = call(rt, ctx, 0x08B81378u, {context, value, 10u, static_cast<u32>(signed_byte_at(rt, pointer)) - 0x30u});
        }
    }
    ctx.gpr[2] = value;
}

// 0x088A693C: serialises the record a0 into the buffer a1: 0x088A5740 gives a size n, the buffer is cleared with 0x08B36018(a1, 0, n),
// 0x30 bytes from +4 go to the start and 0x180 bytes from +0x38 + 384 * (signed byte +8) to +0x30. Returns a1 + n.
void func_088A693C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    const u32 record = ctx.gpr[4];
    const u32 buffer = ctx.gpr[5];
    ctx.gpr[17] = buffer;
    ctx.gpr[16] = record;
    const u32 size = call(rt, ctx, 0x088A5740u);
    ctx.gpr[18] = size;
    call(rt, ctx, 0x08B36018u, {buffer, 0u, size});
    copy_chunks(rt, buffer, record + 4u, 0x30u);
    const s32 index = signed_byte_at(rt, record + 8u);
    const u32 block = record + static_cast<u32>(index) * 384u + 0x30u;
    copy_chunks(rt, buffer + 0x30u, block + 8u, 0x180u);
    ctx.gpr[2] = buffer + size;
}

// 0x088A6ABC: checks a buffer a1 of 0x184 bytes against the record a0 (a2 is the buffer size and must be 0x184): the buffer's first
// word must be 0x65, its other 0x180 bytes are copied to a frame structure that 0x0893600C initialised, the record's word +4 (byte
// swapped) must be 0x65 too and the two-word results of 0x08932558 on the frame structure and on the record's block (+0x38 + 384 *
// signed byte +8) must be equal; 0x08932448 releases the structure. Returns 1 when everything matches, else 0.
void func_088A6ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x1A0u);
    SavedRegisters saved(ctx);
    const u32 record_holder = ctx.gpr[4];
    const u32 buffer = ctx.gpr[5];
    const u32 buffer_size = ctx.gpr[6];
    u32 outcome = 0u;
    ctx.gpr[17] = record_holder;
    ctx.gpr[16] = buffer;
    ctx.gpr[3] = 0u;
    if (buffer_size == 0x184u) {
        const u32 structure = frame.at(0u);
        call(rt, ctx, 0x0893600Cu, {structure});
        if (word(rt, buffer) == 0x65u) {
            copy_chunks(rt, structure, buffer + 4u, 0x180u);
            const u32 swapped = word(rt, word(rt, record_holder) + 4u);
            const u32 version = (swapped >> 24) | ((swapped >> 8) & 0xFF00u) | ((swapped << 8) & 0xFF0000u) | (swapped << 24);
            ctx.gpr[3] = version;
            if (version == 0x65u) {
                const GuestResult first = call_guest(rt, ctx, 0x08932558u, {structure}, {});
                ctx.gpr[2] = first.v0;
                ctx.gpr[3] = first.v1;
                ctx.gpr[18] = first.v0;
                const u32 record = word(rt, record_holder);
                ctx.gpr[19] = first.v1;
                const s32 index = signed_byte_at(rt, record + 8u);
                const GuestResult second = call_guest(rt, ctx, 0x08932558u, {record + static_cast<u32>(index) * 384u + 0x38u}, {});
                ctx.gpr[2] = second.v0;
                ctx.gpr[3] = second.v1;
                if (second.v0 == first.v0 && second.v1 == first.v1) {
                    call(rt, ctx, 0x08932448u, {structure});
                    outcome = 1u;
                    ctx.gpr[2] = outcome;
                    return;
                }
            }
        }
        call(rt, ctx, 0x08932448u, {structure});
    }
    ctx.gpr[2] = outcome;
}

// The color packers (the VFPU holds a vector of four floats): clamp to 0..1 (vsat0), scale by 255, convert (toward zero or to the
// nearest) with 23 fractional bits, pack the four bytes (vi2uc) and move the word to v0.
// 0x0896422C: the vector comes from the four floats at a0.
void func_0896422C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    v::load_quad(rt, ctx, 0u, ctx.gpr[4]);
    v::unary(ctx, 4u, 0u, 0u, 4u);
    ctx.gpr[1] = 0x437F0000u;
    v::move_to(ctx, 0x437F0000u, 1u);
    v::scale(ctx, 0u, 0u, 1u, 4u);
    v::float_to_int(ctx, 17u, 0u, 0u, 23u, 4u);
    v::pack_ints(ctx, 0u, 0u, 0u, 4u);
    ctx.gpr[2] = ctx.vfpu_scalar_bits(0u);
}
// 0x08B57DF4: the vector is already in the VFPU (C000); rounds to the nearest.
void func_08B57DF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    (void)rt;
    v::load_half(ctx, 1u, 0x5BF8u);
    v::unary(ctx, 4u, 0u, 0u, 4u);
    v::scale(ctx, 0u, 0u, 1u, 4u);
    v::float_to_int(ctx, 16u, 0u, 0u, 23u, 4u);
    v::pack_ints(ctx, 0u, 0u, 0u, 4u);
    ctx.gpr[2] = ctx.vfpu_scalar_bits(0u);
}
// 0x08A4CDF4: the vector is f12..f15, passed through the frame.
void func_08A4CDF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    u32 packed;
    {
        GuestFrame frame(ctx, 0x10u);
        put_float(rt, frame.at(0u), ctx.fpr[12]);
        put_float(rt, frame.at(4u), ctx.fpr[13]);
        put_float(rt, frame.at(8u), ctx.fpr[14]);
        put_float(rt, frame.at(0xCu), ctx.fpr[15]);
        v::load_scalar(rt, ctx, 0x00u, frame.at(0u));
        v::load_scalar(rt, ctx, 0x20u, frame.at(4u));
        v::load_scalar(rt, ctx, 0x40u, frame.at(8u));
        v::load_scalar(rt, ctx, 0x60u, frame.at(0xCu));
        v::unary(ctx, 4u, 0u, 0u, 4u);
        const float value[1]{255.0f};
        ctx.write_vfpu_vector_with_destination_prefix(value, 1u, 1u);
        v::scale(ctx, 0u, 0u, 1u, 4u);
        v::float_to_int(ctx, 17u, 0u, 0u, 23u, 4u);
        v::pack_ints(ctx, 0u, 0u, 0u, 4u);
        packed = ctx.vfpu_scalar_bits(0u);
    }
    ctx.gpr[2] = packed;
}

// 0x08A4F5C0: classifies the character code a0 (16 bits): a search (0x08B77C14) in the table at 0x08CDF570 (word 0x08CDF56C records of 6
// bytes, compared by 0x08A4F4A8) for the key (code, code + 1, halfword 0x08CDF648) finds a record that gives the converted code
// (code - record halfword +0 + halfword +4), else ranges of codes (0x3131..0x318E, 0x3200..0x321C, 0x3260..0x327B, 0x327F, 0xAC00..0xD7A3,
// 0xFFE6) answer category 3 and every other code category 1. The result pair is v0 = converted << 16 | code and v1 = category (0 when found).
void func_08A4F5C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    const u32 character = ctx.gpr[4];
    const u32 signed_character = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(character)));
    const u32 low = character & 0xFFFFu;
    ctx.gpr[17] = signed_character;
    ctx.gpr[16] = character;
    ctx.gpr[20] = low;
    const u32 tail = half_at(rt, 0x08CDF648u);
    const u32 count = word(rt, 0x08CDF56Cu);
    const u32 key = frame.at(6u);
    set_half(rt, key + 4u, tail);
    set_half(rt, key + 2u, signed_character + 1u);
    set_half(rt, key, low);
    const u32 found = call(rt, ctx, 0x08B77C14u, {key, 0x08CDF570u, count, 6u, 0x08A4F4A8u});
    u32 converted = 0u;
    u32 category = 1u;
    if (found != 0u) {
        converted = (signed_character - half_at(rt, found) + half_at(rt, found + 4u)) & 0xFFFFu;
        category = 0u;
    } else if (character - 0x3131u < 0x5Eu || character - 0x3200u < 0x1Du || character - 0x3260u < 0x1Cu || character == 0x327Fu ||
               character - 0xAC00u < 0x2BA4u || character == 0xFFE6u) {
        category = 3u;
    }
    ctx.gpr[2] = (converted << 16) | low;
    ctx.gpr[3] = category;
}

// The state block of the graphics library (a table of matrix pointers at +0xD00, the stack index at +0xD10, dirty words at +0xD18 /
// +0xD1C merged with the words at +0xD20 / +0xD24): multiplies the current matrix by a rotation about one axis through the angle f12
// (degrees; the angle is reduced modulo 4 turns of 0.25 with vf2iz, then vrot builds sine and cosine into two columns of the identity)
// and marks the block dirty. v0 is the constant 0x3C360B61 (the angle scale, left by the original).
void rotate_current_matrix(Runtime &rt, AllegrexContext &ctx, u32 identity_column, u32 first_column, u32 first_pattern, u32 second_column,
                           u32 second_pattern) {
    namespace v = gt::vfpu;
    const u32 angle = std::bit_cast<u32>(ctx.fpr[12]);
    const u32 block = ctx.gpr[4];
    const u32 matrix = word(rt, block + (word(rt, block + 0xD10u) << 2) + 0xD00u);
    const u32 pending = word(rt, block + 0xD18u) | word(rt, block + 0xD20u);
    v::move_to(ctx, angle, 0x08u);
    for (unsigned column = 0u; column < 4u; ++column) v::load_column(rt, ctx, 3u, column, matrix + 16u * column);
    v::unit(ctx, identity_column, 4u);
    put(rt, block + 0xD18u, pending);
    v::unit(ctx, 3u, 4u);
    v::move_to(ctx, 0x3C360B61u, 0x28u);
    v::binary(ctx, v::Binary::Mul, 0x48u, 0x08u, 0x28u, 1u);
    v::load_half(ctx, 0x08u, 0x3400u);
    v::binary(ctx, v::Binary::Mul, 0x28u, 0x48u, 0x08u, 1u);
    v::float_to_int(ctx, 17u, 0x28u, 0x28u, 0u, 1u);
    v::int_to_float(ctx, 0x28u, 0x28u, 0u, 1u);
    v::load_half(ctx, 0x08u, 0x4400u);
    v::binary(ctx, v::Binary::Mul, 0x28u, 0x28u, 0x08u, 1u);
    v::binary(ctx, v::Binary::Sub, 0x48u, 0x48u, 0x28u, 1u);
    v::rotation(ctx, first_column, 0x48u, 4u, first_pattern);
    v::rotation(ctx, second_column, 0x48u, 4u, second_pattern);
    v::matrix_multiply(ctx, 4u, 0x2Cu, 0u, 4u);
    put(rt, block + 0xD1Cu, word(rt, block + 0xD1Cu) | word(rt, block + 0xD24u));
    for (unsigned column = 0u; column < 4u; ++column) v::store_quad(rt, ctx, 4u + column, matrix + 16u * column);
    ctx.gpr[2] = 0x3C360B61u;   // the constant the original leaves in v0
}
// 0x08B5608C rotates in the plane of the columns 1 and 2 ([0, C, S] and [0, -S, C]); the same routine with columns 0 and 1 is 0x08B55F14,
// which no synthetic case reached and is left recompiled.
void func_08B5608C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rotate_current_matrix(rt, ctx, 0u, 1u, 9u, 2u, 22u); }

// 0x0880451C: a static-initialiser dispatch: phase a0 = 1 or 0 with priority a1 = 0xFFFF jumps to 0x08B2AFE0 or 0x08B2B020 with the
// table at 0x08CE141C, any other call returns (v0 = 0xFFFF in every case).
void func_0880451C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const u32 phase = ctx.gpr[4];
    const u32 priority = ctx.gpr[5];
    ctx.gpr[2] = 0xFFFFu;
    if (phase == 1u) {
        ctx.gpr[4] = 0x08CE141Cu;
        if (priority == 0xFFFFu) tail_call(ctx, 0x08B2AFE0u, {0x08CE141Cu});
        return;
    }
    ctx.gpr[4] = 0u;
    if (phase != 0u) return;
    ctx.gpr[4] = 0x08CE141Cu;
    if (priority == 0xFFFFu) tail_call(ctx, 0x08B2B020u, {0x08CE141Cu});
}

} // namespace
} // namespace hot_batch_344
} // namespace gt::game

#define HOT344(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_344::name, gt::ReturnKind::kind)
HOT344(0x088972BCu, func_088972BC, Word);
HOT344(0x088D5588u, func_088D5588, Word);
HOT344(0x08B08B60u, func_08B08B60, Word);
HOT344(0x08B81488u, func_08B81488, Word);
HOT344(0x088A693Cu, func_088A693C, Word);
HOT344(0x088A6ABCu, func_088A6ABC, Word);
HOT344(0x0896422Cu, func_0896422C, Word);
HOT344(0x08B57DF4u, func_08B57DF4, Word);
HOT344(0x08A4CDF4u, func_08A4CDF4, Word);
HOT344(0x08A4F5C0u, func_08A4F5C0, DoubleWord);
HOT344(0x08B5608Cu, func_08B5608C, Word);
HOT344(0x0880451Cu, func_0880451C, Word);
