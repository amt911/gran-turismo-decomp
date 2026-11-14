// Hot batch 233: fifty-six- to fifty-nine-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (stream extractors, setters through null-checked temporary handles, constructors, copies of fields,
// list walks that append to vectors and vector-of-vector copies).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_233 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
// `ins value, source, position, 1`: the bit at `position` replaced by source's lowest bit.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position) {
    return (value & ~(1u << position)) | ((source & 1u) << position);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
std::int32_t trunc_w_s(float value) {
    if (!(value >= -2337483648.0f && value < 2337483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}
// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// The same inside a frame of `bytes` (`addiu sp, sp, -bytes; sw ra; jal; ...; jr ra`).
void call_with(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t bytes) {
    GuestFrame frame(ctx, bytes);
    call_here(rt, ctx, target);
}

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

using u32 = std::uint32_t;
using s32 = std::int32_t;

// The `std::string` representation every empty string shares.
constexpr u32 kEmptyStreamText = 0x08D74FD0u;

// Drops one reference of the string whose characters start at `data` (representation at data - 0xC, count at data - 4): the shared
// empty representation is left alone, otherwise 0x08B60804 decrements the count and 0x08B8542C frees the representation at zero or
// below. a0 / a1 are left as the original leaves them (the pointer and -1, or the representation and the frame).
void drop_string_reference(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, u32 data) {
    const u32 representation = data - 0xCu;
    ctx.gpr[3] = data;
    ctx.gpr[2] = kEmptyStreamText;
    ctx.gpr[4] = data - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (representation == kEmptyStreamText) return;
    const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
    ctx.gpr[4] = representation;
    ctx.gpr[5] = frame.at(0u);
    if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {representation, frame.at(0u)});
}

// 0x08A5BB7C: the null-handle report every checked handle load calls with its message in a0.
void report_null(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 message) { call_set(rt, ctx, 0x08A5BB7Cu, {message}); }

// Formats `count` and `argument` through 0x0883B820 and the format string `format` (0x08B3767C: buffer, 0x100 bytes) into a string slot,
// builds the result slot from it (0x08A6D2D4), copies it to *a0 when that is another slot (0x08A5BAD8) and releases both strings. Returns
// (v0) the count when it is not positive.
void format_into_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 format) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x130u);
    const u32 text_slot = frame.at(4u);
    const u32 result_slot = frame.at(8u);
    const u32 buffer = frame.at(0xCu);
    ctx.gpr[2] = count;
    ctx.gpr[4] = count;
    ctx.gpr[5] = argument;
    if (static_cast<s32>(count) <= 0) return;
    const u32 value = call_set(rt, ctx, 0x0883B820u, {count, argument});
    call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x100u, format, value});
    call_set(rt, ctx, 0x08B85EB8u, {text_slot, buffer, frame.at(1u)});
    call_set(rt, ctx, 0x08A6D2D4u, {result_slot, text_slot});
    ctx.gpr[4] = out;
    if (out != result_slot) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, result_slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {result_slot});
    drop_string_reference(rt, ctx, frame, word(rt, text_slot));
}

// Copies a record (a0 = destination, a1 = source): the byte at +8, three 0x180-byte elements from +0x38 (0x08932450 each), eight words
// from +0xC and four halfwords from +0x4B8. All source fields are read before the destination ones are written. Returns (v0) the
// halfword at +0x4BE.
void copy_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 flag = rt.memory().load8(source + 8u);
    ctx.gpr[2] = flag;
    rt.memory().store8(destination + 8u, static_cast<std::uint8_t>(flag));
    for (u32 element = 0u; element != 3u; ++element) {
        call_set(rt, ctx, 0x08932450u, {destination + 0x38u + element * 0x180u, source + 0x38u + element * 0x180u});
    }
    u32 words[8];
    for (u32 i = 0u; i != 8u; ++i) words[i] = word(rt, source + 0xCu + i * 4u);
    const u32 last = rt.memory().load16(source + 0x4BEu);
    const u32 first = rt.memory().load16(source + 0x4B8u);
    const u32 second = rt.memory().load16(source + 0x4BAu);
    const u32 third = rt.memory().load16(source + 0x4BCu);
    rt.memory().store16(destination + 0x4BEu, static_cast<std::uint16_t>(last));
    for (u32 i = 0u; i != 8u; ++i) put(rt, destination + 0xCu + i * 4u, words[i]);
    rt.memory().store16(destination + 0x4B8u, static_cast<std::uint16_t>(first));
    rt.memory().store16(destination + 0x4BAu, static_cast<std::uint16_t>(second));
    rt.memory().store16(destination + 0x4BCu, static_cast<std::uint16_t>(third));
    ctx.gpr[2] = last;
}

// Constructor (a0 = the object, a1 & 0xFF = whether a block is allocated): base constructor 0x088EE6C0, vtable 0x08CC38B8, a table of
// -1 / 0 fields from +0x558, a handle from 0x08B0979C (a 4-byte block from 0x08B0A458 into +0x538 when asked; 0x08B0980C into +0xA8)
// and four (-1, 0) pairs at +0x598 / +0x5A8. Returns (v0) 4.
void construct_slot_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const bool allocate = (ctx.gpr[5] & 0xFFu) != 0u;
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x088EE6C0u, {object});
    put(rt, object, 0x08CC38B8u);
    put(rt, object + 0x58Cu, 0xFFFFFFFFu);
    put(rt, object + 0x594u, 0x64u);
    put(rt, object + 0x558u, 0xFFFFFFFFu);
    put(rt, object + 0x55Cu, 0xFFFFFFFFu);
    for (u32 offset = 0x560u; offset <= 0x574u; offset += 4u) put(rt, object + offset, 0u);
    for (u32 offset = 0x578u; offset <= 0x584u; offset += 4u) put(rt, object + offset, 0xFFFFFFFFu);
    rt.memory().store8(object + 0x588u, 0u);
    put(rt, object + 0x590u, 0u);
    rt.memory().store8(object + 0x5B8u, 0u);
    rt.memory().store8(object + 0x5B9u, 0u);
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[2] = 0x64u;
    const u32 handle = call_set(rt, ctx, 0x08B0979Cu, {});
    if (allocate) put(rt, object + 0x538u, call_set(rt, ctx, 0x08B0A458u, {handle, 4u}));
    put(rt, object + 0xA8u, call_set(rt, ctx, 0x08B0980Cu, {handle}));
    for (u32 i = 0u; i != 4u; ++i) {
        put(rt, object + 0x598u + i * 4u, 0xFFFFFFFFu);
        put(rt, object + 0x5A8u + i * 4u, 0u);
    }
    ctx.gpr[2] = 4u;
}

// For a1 == 2 (v0 = 2 otherwise): asks the holder's member (virtual +0xC, null message 0x08CB2F34) for a string into the slot at sp + 8,
// builds the slot at sp + 4 from it and the holder's next element with `build`, copies it to *a0 when that is another slot (0x08A5BAD8)
// and releases both slots (0x08A5B9EC, then the string reference).
void bind_member_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 build) {
    const u32 out = ctx.gpr[4];
    const u32 kind = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const u32 build_slot = frame.at(4u);
    const u32 string_slot = frame.at(8u);
    ctx.gpr[2] = 2u;
    if (kind != 2u) return;
    const u32 member = word(rt, holder);
    ctx.gpr[4] = 0x08CB2F34u;
    if (member == 0u) report_null(rt, ctx, 0x08CB2F34u);
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    call_set(rt, ctx, ctx.gpr[2], {string_slot, member});
    call_set(rt, ctx, build, {build_slot, string_slot, holder + 4u});
    ctx.gpr[4] = out;
    if (out != build_slot) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, build_slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {build_slot});
    drop_string_reference(rt, ctx, frame, word(rt, string_slot));
}

// For a count a2 >= 2 (signed; a1 is kept for the build): asks the two holder members (virtual +0x3C, null message 0x08C9CB80) for a value
// each, builds the object into the slot at sp (0x08847008 from a1, null message 0x08C9CBBC), takes its handle (0x08846E50), releases
// the slot (0x08A5B9EC) and calls `apply` with the handle and both values.
void apply_two_member_values(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 apply) {
    const u32 argument = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 slot = frame.at(0u);
    ctx.gpr[6] = static_cast<s32>(count) < 2 ? 1u : 0u;
    if (ctx.gpr[6] != 0u) return;
    const u32 first = word(rt, holder);
    ctx.gpr[4] = 0x08C9CB80u;
    if (first == 0u) report_null(rt, ctx, 0x08C9CB80u);
    ctx.gpr[3] = word(rt, first);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const u32 first_value = call_set(rt, ctx, ctx.gpr[2], {first});
    const u32 second = word(rt, holder + 4u);
    ctx.gpr[4] = 0x08C9CB80u;
    if (second == 0u) report_null(rt, ctx, 0x08C9CB80u);
    ctx.gpr[3] = word(rt, second);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const u32 second_value = call_set(rt, ctx, ctx.gpr[2], {second});
    call_set(rt, ctx, 0x08847008u, {slot, argument});
    const u32 object = word(rt, slot);
    ctx.gpr[4] = 0x08C9CBBCu;
    if (object == 0u) report_null(rt, ctx, 0x08C9CBBCu);
    const u32 handle = call_set(rt, ctx, 0x08846E50u, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, apply, {handle, first_value, second_value});
}

// A property with a check (a1 passed to the filler, a2 the mode, a3 a holder): mode 0 fills the handle slot (0x08A93C88) and hands the
// object to 0x08A93DD4; mode 1 asks the holder's member (virtual +0xD4, null message 0x08CB208C) and, when that is non-zero, fills the
// slot again from a1 and hands the object and the holder to 0x08A93CC8; any other mode returns 1.
void checked_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 argument = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (mode == 0u) {
        const u32 slot = frame.at(4u);
        call_set(rt, ctx, 0x08A93C88u, {slot});
        const u32 object = word(rt, slot);
        ctx.gpr[4] = 0x08CB20FCu;
        if (object == 0u) report_null(rt, ctx, 0x08CB20FCu);
        call_set(rt, ctx, 0x08A93DD4u, {object});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const u32 member = word(rt, holder);
    ctx.gpr[4] = 0x08CB208Cu;
    if (member == 0u) report_null(rt, ctx, 0x08CB208Cu);
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xD4u);
    if (call_set(rt, ctx, ctx.gpr[2], {member}) == 0u) return;
    const u32 slot = frame.at(0u);
    call_set(rt, ctx, 0x08A93C88u, {slot, argument});
    const u32 object = word(rt, slot);
    ctx.gpr[4] = 0x08CB20FCu;
    if (object == 0u) report_null(rt, ctx, 0x08CB20FCu);
    call_set(rt, ctx, 0x08A93CC8u, {object, holder});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
}

// For a count a2 > 0 (signed): builds an object into the slot at sp + 4 (0x088297A8, null message 0x08C9A2F8), asks it for a value
// (0x088294D0), releases the slot, converts the value (0x0885E710), asks the holder's member (virtual +0xC, null message 0x08C9A2B4) for a
// string into the slot at sp + 8, hands the converted value and the string to `apply` and drops the string reference.
void apply_member_text(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 apply) {
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(4u);
    const u32 text_slot = frame.at(8u);
    ctx.gpr[4] = slot;
    if (static_cast<s32>(count) <= 0) return;
    call_set(rt, ctx, 0x088297A8u, {slot});
    const u32 object = word(rt, slot);
    ctx.gpr[4] = 0x08C9A2F8u;
    if (object == 0u) report_null(rt, ctx, 0x08C9A2F8u);
    const u32 value = call_set(rt, ctx, 0x088294D0u, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    const u32 converted = call_set(rt, ctx, 0x0885E710u, {value});
    const u32 member = word(rt, holder);
    ctx.gpr[4] = 0x08C9A2B4u;
    if (member == 0u) report_null(rt, ctx, 0x08C9A2B4u);
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    call_set(rt, ctx, ctx.gpr[2], {text_slot, member});
    call_set(rt, ctx, apply, {converted, word(rt, text_slot)});
    drop_string_reference(rt, ctx, frame, word(rt, text_slot));
}

// Copy-constructs `count` 0x14-byte elements at a0 (a2 = the source element, kept for every one): for a non-null element 0x08B9432C copies
// the head, the vector at +4 / +8 / +0xC (begin / end / capacity end) is rebuilt as a copy of the source's (sixteen-byte items: 0x08B35DA0
// for the storage, 0x08BBEC68 for the items; it returns the new end) and the word at +0x10 is copied.
void construct_vector_array(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    if (count == 0u) return;
    u32 element = first;
    u32 done = 0u;
    do {
        ctx.gpr[4] = element;
        ctx.gpr[5] = source;
        ++done;
        if (element != 0u) {
            call_set(rt, ctx, 0x08B9432Cu, {element, source});
            const u32 begin = word(rt, source + 4u);
            const u32 end = word(rt, source + 8u);
            const u32 items = static_cast<u32>(static_cast<s32>(end - begin) >> 4);
            put(rt, element + 8u, 0u);
            put(rt, element + 4u, 0u);
            put(rt, element + 0xCu, 0u);
            u32 bytes = 0u;
            u32 block = 0u;
            ctx.gpr[2] = items;
            if (items != 0u) {
                bytes = items << 4;
                block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
            }
            put(rt, element + 8u, block);
            put(rt, element + 4u, block);
            ctx.gpr[2] = block + bytes;
            put(rt, element + 0xCu, block + bytes);
            const u32 copied_end = call_set(rt, ctx, 0x08BBEC68u, {begin, end, block, frame.at(0u)});
            put(rt, element + 8u, copied_end);
            ctx.gpr[2] = word(rt, source + 0x10u);
            put(rt, element + 0x10u, ctx.gpr[2]);
        }
        element += 0x14u;
    } while (done != count);
}

// A stream sentry (a0 = the stream, a virtual base at -0xC of its vtable): when none of the state bits 5 are set in the base's flags (+0x14),
// the buffer object at base + 0x78 fills the 0x20-byte record at sp + 0x40 (its virtual +0x10, t1 = `size`); a record whose first two
// words are both -1 sets bit 4 in the flags (0x08C54CFC). Returns (v0) the stream.
void stream_sentry(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 size) {
    const u32 stream = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    const u32 vtable = word(rt, stream);
    const u32 base = stream + word(rt, vtable - 0xCu);
    if ((word(rt, base + 0x14u) & 5u) == 0u) {
        const u32 buffer = word(rt, base + 0x78u);
        ctx.gpr[3] = word(rt, buffer);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x10u);
        ctx.gpr[9] = size;
        call_set(rt, ctx, ctx.gpr[2], {frame.at(0x40u), buffer});
        const u32 first = word(rt, frame.at(0x40u));
        const u32 second = word(rt, frame.at(0x44u));
        if (first == 0xFFFFFFFFu && second == first) {
            const u32 flags_base = stream + word(rt, word(rt, stream) - 0xCu);
            call_set(rt, ctx, 0x08C54CFCu, {flags_base, word(rt, flags_base + 0x14u) | 4u});
        }
    }
    ctx.gpr[2] = stream;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0883DCD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { format_into_string(rt, ctx, 0x08C9C490u); }
void func_088A5580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_record(rt, ctx); }
void func_088EAFA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_slot_object(rt, ctx); }
void func_08AA8088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bind_member_pair(rt, ctx, 0x08AA7464u); }
void func_08C0C518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = G[8]; put(rt, frame.at(0x10u), G[16]); G[16] = G[8]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[17] = G[6]; call_here(rt, ctx, 0x08AA2A30u); G[4] = frame.at(0u); G[5] = G[16] + 4u; G[19] = G[2]; call_here(rt, ctx, 0x089FA424u); G[5] = word(rt, G[18] + 0x18u); G[17] = word(rt, G[17]); G[6] = word(rt, G[18] + 0x14u); G[4] = G[17] + sra(G[5], 1u); G[3] = G[5] & 1u; G[7] = G[6]; if (G[3] != 0u) { G[2] = word(rt, G[4]) + G[6]; G[7] = word(rt, G[2]); } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[18] + 0x20u); G[3] = sra(G[2], 1u); G[2] = G[2] & 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[18] + 0x1Cu); G[2] = word(rt, G[4]) + G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[18] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[19]; G[6] = frame.at(0u); call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x089F9B74u); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_088475D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_two_member_values(rt, ctx, 0x088D4FD8u); }
void func_08C55AEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { stream_sentry(rt, ctx, 8u); }
void func_08A41554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x18u), G[18]); G[18] = G[16] + 0x30u; put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); call_here(rt, ctx, 0x08A98A70u); G[17] = word(rt, G[16] + 0x38u); G[21] = frame.at(0u); if (G[18] != G[17]) { G[20] = 0x08CB0000u; do { G[2] = word(rt, G[19]); G[4] = 0x08CADBF4u; G[16] = G[2] + 0x14u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[17] + 0x10u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A6D2D4u); G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[16] + 8u); G[4] = G[16]; G[5] = G[3]; G[6] = frame.at(0u); if (G[3] != G[2]) { G[4] = G[3]; G[5] = frame.at(0u); if (G[3] != 0u) { call_here(rt, ctx, 0x08A76ABCu); G[3] = word(rt, G[16] + 4u); } G[3] += 4u; put(rt, G[16] + 4u, G[3]); } else { call_here(rt, ctx, 0x08B8A03Cu); } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08B5F16Cu); G[17] = G[2]; } while (G[2] != G[18]); } G[2] = G[19]; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A93E5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { checked_property(rt, ctx); }
void func_08B0EE34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x140u); put(rt, frame.at(0x13Cu), G[31]); put(rt, frame.at(0x138u), G[18]); G[18] = G[4]; put(rt, frame.at(0x134u), G[17]); G[17] = G[6]; put(rt, frame.at(0x130u), G[16]); G[16] = G[5]; call_here(rt, ctx, 0x08B0CE58u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0DC34u); G[5] = G[16]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0EA48u); G[16] = G[2]; if (G[2] != 0u) { G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[17]); G[4] = G[16]; put(rt, frame.at(0x124u), G[3]); G[3] = 9u; put(rt, frame.at(0x120u), G[2]); G[2] = 1u; store8(frame.at(0x128u), 0u); put(rt, G[16] + 0x7Cu, G[2]); put(rt, G[16] + 0x94u, G[3]); call_here(rt, ctx, 0x08B0DE3Cu); G[6] = G[16] + 0xA8u; G[7] = G[18]; G[8] = G[16] + 0xD8u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6] + 8u); G[3] = word(rt, G[6]); G[5] = word(rt, G[6] + 4u); G[4] = G[16]; put(rt, G[7] + 8u, G[2]); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[5]); call_here(rt, ctx, 0x08B0EA58u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0DCA8u); G[2] = G[18]; G[18] = word(rt, frame.at(0x138u)); G[17] = word(rt, frame.at(0x134u)); G[16] = word(rt, frame.at(0x130u)); } }
void func_0882D580(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_text(rt, ctx, 0x08917960u); }
void func_08BBF554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_vector_array(rt, ctx); }
void func_0883DDB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { format_into_string(rt, ctx, 0x08C9C478u); }
void func_088A5660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_record(rt, ctx); }
void func_088EB088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_slot_object(rt, ctx); }
void func_08AA8D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bind_member_pair(rt, ctx, 0x08AA8168u); }
void func_08C26414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = G[8]; put(rt, frame.at(0x10u), G[16]); G[16] = G[8]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); G[17] = G[6]; call_here(rt, ctx, 0x08AA2920u); G[4] = frame.at(0u); G[5] = G[16] + 4u; G[19] = G[2]; call_here(rt, ctx, 0x08A76ABCu); G[5] = word(rt, G[18] + 0x18u); G[17] = word(rt, G[17]); G[6] = word(rt, G[18] + 0x14u); G[4] = G[17] + sra(G[5], 1u); G[3] = G[5] & 1u; G[7] = G[6]; if (G[3] != 0u) { G[2] = word(rt, G[4]) + G[6]; G[7] = word(rt, G[2]); } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[18] + 0x20u); G[3] = sra(G[2], 1u); G[2] = G[2] & 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[18] + 0x1Cu); G[2] = word(rt, G[4]) + G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[18] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[19]; G[6] = frame.at(0u); call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_088476BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_two_member_values(rt, ctx, 0x088D4FE4u); }
void func_08A93F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { checked_property(rt, ctx); }
void func_0882D66C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_text(rt, ctx, 0x08917958u); }
void func_08BBF8CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[22]); G[22] = G[5]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[6]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); if (G[5] != 0u) { G[16] = G[4]; G[21] = 0u; G[20] = G[6] + 4u; do { G[4] = G[16]; G[5] = G[19]; G[21] += 1u; G[17] = G[16] + 4u; if (G[16] != 0u) { G[18] = 0u; call_here(rt, ctx, 0x08B9432Cu); G[2] = word(rt, G[20] + 4u); G[3] = word(rt, G[19] + 4u); put(rt, G[17] + 4u, 0u); G[4] = 0u; G[2] = sra(G[2] - G[3], 4u); put(rt, G[16] + 4u, 0u); put(rt, G[17] + 8u, 0u); if (G[2] != 0u) { G[18] = G[2] << 4; G[4] = G[18]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } put(rt, G[17] + 4u, G[4]); G[2] = G[4] + G[18]; G[6] = G[4]; put(rt, G[17], G[4]); G[7] = frame.at(0u); G[4] = word(rt, G[20]); G[5] = word(rt, G[20] + 4u); put(rt, G[17] + 8u, G[2]); call_here(rt, ctx, 0x08BBEC68u); put(rt, G[17] + 4u, G[2]); G[2] = word(rt, G[19] + 0x10u); put(rt, G[16] + 0x10u, G[2]); } G[16] += 0x14u; } while (G[21] != G[22]); } G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08C70764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { stream_sentry(rt, ctx, 0x10u); }
void func_08A416BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[16]); G[16] = G[5]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x18u), G[18]); G[18] = G[16] + 0x18u; put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); call_here(rt, ctx, 0x08A98A70u); G[17] = word(rt, G[16] + 0x20u); G[21] = frame.at(0u); if (G[18] != G[17]) { G[20] = 0x08CB0000u; do { G[2] = word(rt, G[19]); G[4] = 0x08CADBF4u; G[16] = G[2] + 0x14u; if (G[2] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[17] + 0x10u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A6D2D4u); G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[16] + 8u); G[4] = G[16]; G[5] = G[3]; G[6] = frame.at(0u); if (G[3] != G[2]) { G[4] = G[3]; G[5] = frame.at(0u); if (G[3] != 0u) { call_here(rt, ctx, 0x08A76ABCu); G[3] = word(rt, G[16] + 4u); } G[3] += 4u; put(rt, G[16] + 4u, G[3]); } else { call_here(rt, ctx, 0x08B8A03Cu); } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08B5F16Cu); G[17] = G[2]; } while (G[2] != G[18]); } G[2] = G[19]; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08B0F608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x140u); put(rt, frame.at(0x13Cu), G[31]); put(rt, frame.at(0x138u), G[18]); G[18] = G[4]; put(rt, frame.at(0x134u), G[17]); G[17] = G[6]; put(rt, frame.at(0x130u), G[16]); G[16] = G[5]; call_here(rt, ctx, 0x08B0CE58u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0DC34u); G[5] = G[16]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0EA48u); G[16] = G[2]; if (G[2] != 0u) { G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[17]); G[4] = G[16]; put(rt, frame.at(0x124u), G[3]); G[3] = 5u; put(rt, frame.at(0x120u), G[2]); G[2] = 1u; store8(frame.at(0x128u), 0u); put(rt, G[16] + 0x7Cu, G[2]); put(rt, G[16] + 0x94u, G[3]); call_here(rt, ctx, 0x08B0DE3Cu); G[6] = G[16] + 0xA8u; G[7] = G[18]; G[8] = G[16] + 0xD8u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6] + 8u); G[3] = word(rt, G[6]); G[5] = word(rt, G[6] + 4u); G[4] = G[16]; put(rt, G[7] + 8u, G[2]); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[5]); call_here(rt, ctx, 0x08B0EA58u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08B0DCA8u); G[2] = G[18]; G[18] = word(rt, frame.at(0x138u)); G[17] = word(rt, frame.at(0x134u)); G[16] = word(rt, frame.at(0x130u)); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_233
} // namespace gt::game

#define HOT233(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_233::name, gt::ReturnKind::kind)
HOT233(0x0883DCD8u, func_0883DCD8, Word);
HOT233(0x088A5580u, func_088A5580, Word);
HOT233(0x088EAFA8u, func_088EAFA8, Word);
HOT233(0x08AA8088u, func_08AA8088, Word);
// HOT233(0x08C0C518u, func_08C0C518, Word);  // no comparable synthetic case yet
HOT233(0x088475D8u, func_088475D8, Word);
HOT233(0x08C55AECu, func_08C55AEC, Word);
// HOT233(0x08A41554u, func_08A41554, Word);  // no comparable synthetic case yet
HOT233(0x08A93E5Cu, func_08A93E5C, Word);
HOT233(0x08B0EE34u, func_08B0EE34, Word);
HOT233(0x0882D580u, func_0882D580, Word);
HOT233(0x08BBF554u, func_08BBF554, Word);
HOT233(0x0883DDB8u, func_0883DDB8, Word);
HOT233(0x088A5660u, func_088A5660, Word);
HOT233(0x088EB088u, func_088EB088, Word);
HOT233(0x08AA8D20u, func_08AA8D20, Word);
// HOT233(0x08C26414u, func_08C26414, Word);  // no comparable synthetic case yet
HOT233(0x088476BCu, func_088476BC, Word);
HOT233(0x08A93F44u, func_08A93F44, Word);
HOT233(0x0882D66Cu, func_0882D66C, Word);
HOT233(0x08BBF8CCu, func_08BBF8CC, Word);
HOT233(0x08C70764u, func_08C70764, Word);
// HOT233(0x08A416BCu, func_08A416BC, Word);  // no comparable synthetic case yet
HOT233(0x08B0F608u, func_08B0F608, Word);
