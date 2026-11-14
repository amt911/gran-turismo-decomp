// Hot batch 230: sixty-one- to sixty-five-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (getters and setters through a null-checked temporary handle, a vector reserve, guarded static initialisers,
// a copy loop and pointer-to-member calls).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_230 {
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
    if (!(value >= -2307483648.0f && value < 2307483648.0f)) return 0x7FFFFFFF;
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

// How an accessor reads its value out of the object and how it writes one back.
enum class FieldKind { Word, Signed16, Float };
enum class StoreKind { Word, Call, Float, ByteAndFloat };

struct Accessor {
    u32 fill_handle;         // fills the handle slot with the object (a0 = slot)
    u32 null_handle_message; // reported when the object is null
    u32 null_holder_message; // reported when the value holder is null (set side)
    u32 read;                // turns the field into the result string (a0 = result slot, a1 = field; float: f12)
    FieldKind get_kind;
    u32 get_offset;
    u32 vcall_offset;        // the holder's virtual member that yields the new value
    StoreKind set_kind;
    u32 set_param;           // Word: field offset; Call: the setter; Float / ByteAndFloat: the base offset of the record
};

// Script property accessors (a0 the output string slot, a2 the mode, a3 a holder of the new value): mode 0 reads the object's field and
// assigns it to *a0 (0x08A5BAD8 unless it already is the result slot); mode 1 asks the holder's virtual member for a value and stores it
// into the object; any other mode returns 1 without a call. The handle slot is released at the end (0x08A5B9EC).
void property_accessor(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const Accessor &accessor) {
    const u32 out = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (mode == 0u) {
        const u32 handle_slot = frame.at(4u);
        const u32 result_slot = frame.at(8u);
        call_set(rt, ctx, accessor.fill_handle, {handle_slot});
        const u32 object = word(rt, handle_slot);
        if (object == 0u) report_null(rt, ctx, accessor.null_handle_message);
        if (accessor.get_kind == FieldKind::Float) {
            ctx.fpr[12] = float_at(rt, object + accessor.get_offset);
            call_set(rt, ctx, accessor.read, {result_slot});
        } else {
            u32 field = 0u;
            if (accessor.get_kind == FieldKind::Word) {
                field = word(rt, object + accessor.get_offset);
            } else {
                field = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(object + accessor.get_offset))));
            }
            call_set(rt, ctx, accessor.read, {result_slot, field});
        }
        const u32 value = word(rt, result_slot);
        ctx.gpr[5] = value;
        if (out != result_slot) call_set(rt, ctx, 0x08A5BAD8u, {out, value});
        call_set(rt, ctx, 0x08A5B9ECu, {result_slot});
        call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
    } else {
        ctx.gpr[2] = 1u;
        if (mode != 1u) return;
        const u32 handle_slot = frame.at(0u);
        call_set(rt, ctx, accessor.fill_handle, {handle_slot});
        const u32 object = word(rt, handle_slot);
        if (object == 0u) report_null(rt, ctx, accessor.null_handle_message);
        const u32 member = word(rt, holder);
        if (member == 0u) report_null(rt, ctx, accessor.null_holder_message);
        ctx.gpr[3] = word(rt, member);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + accessor.vcall_offset);
        const u32 produced = call_set(rt, ctx, ctx.gpr[2], {member});
        switch (accessor.set_kind) {
        case StoreKind::Word:
            put(rt, object + accessor.set_param, produced);
            break;
        case StoreKind::Call:
            call_set(rt, ctx, accessor.set_param, {object, produced});
            break;
        case StoreKind::Float: {
            const u32 record = object + accessor.set_param;
            ctx.gpr[3] = record;
            ctx.gpr[2] = 0xFFFFFFFFu;
            put(rt, record + 0x10u, 0xFFFFFFFFu);
            put_float(rt, record + 0xCu, ctx.fpr[0]);
            break;
        }
        case StoreKind::ByteAndFloat: {
            const u32 record = object + accessor.set_param;
            ctx.gpr[2] = record;
            rt.memory().store8(record + 0xC4u, static_cast<std::uint8_t>(mode));
            put_float(rt, record + 0xA0u, ctx.fpr[0]);
            break;
        }
        }
        call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
    }
}

struct MemberString {
    bool gate_on_mode_one;   // true: mode == 1 (v0 = 1 otherwise); false: mode > 0 (v0 untouched otherwise)
    u32 object_slot;         // frame offset of the handle slot
    u32 target_slot;         // frame offset of the slot the apply step fills
    u32 fill_handle;
    u32 null_object_message;
    u32 null_holder_message;
    u32 apply;               // a0 = target slot, a1 = object, a2 = string slot
};

// Script setters of one string-valued property (a0 the output slot, a2 the mode, a3 a holder): the holder's virtual member (+0xC) writes a
// string into the slot at sp + 4, `apply` turns object + string into the target slot, the target is copied to *a0 when it is another
// slot (0x08A5BAD8), released, the string's reference dropped and the handle slot released (0x08A5B9EC).
void apply_member_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const MemberString &p) {
    const u32 out = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    if (p.gate_on_mode_one) {
        ctx.gpr[2] = 1u;
        if (mode != 1u) return;
    } else if (static_cast<s32>(mode) <= 0) {
        return;
    }
    const u32 object_slot = frame.at(p.object_slot);
    const u32 string_slot = frame.at(4u);
    const u32 target_slot = frame.at(p.target_slot);
    call_set(rt, ctx, p.fill_handle, {object_slot});
    const u32 object = word(rt, object_slot);
    if (object == 0u) report_null(rt, ctx, p.null_object_message);
    const u32 member = word(rt, holder);
    if (member == 0u) report_null(rt, ctx, p.null_holder_message);
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu);
    call_set(rt, ctx, ctx.gpr[2], {string_slot, member});
    call_set(rt, ctx, p.apply, {target_slot, object, string_slot});
    const u32 produced = word(rt, target_slot);
    ctx.gpr[5] = produced;
    if (out != target_slot) call_set(rt, ctx, 0x08A5BAD8u, {out, produced});
    call_set(rt, ctx, 0x08A5B9ECu, {target_slot});
    drop_string_reference(rt, ctx, frame, word(rt, string_slot));
    call_set(rt, ctx, 0x08A5B9ECu, {object_slot});
}

// A function-local static pair (two `static` objects guarded by flag bytes): the guard 0x08B6441C accepts a flag byte that is clear and
// 0x08B64494 releases it. The outer pair is `shared_flag` / `shared_record` (record: +0 zero, +4 link, +8 vtable), built at most once; the
// first object (flag `first_flag`) stores `shared_record` into its own record's link cell (+0xC); the second (`second_flag`) builds
// `second_record` with `second_vtable`. Returns (v0) the second record.
struct StaticPair {
    u32 first_flag;
    u32 second_flag;
    u32 shared_flag;
    u32 shared_record;
    u32 shared_vtable;
    u32 second_record;
    u32 second_vtable;
};

void guard_static_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const StaticPair &p) {
    GuestFrame frame(ctx, 0x10u);
    auto flag_clear = [&](u32 flag) { return static_cast<std::int8_t>(rt.memory().load8(flag)) == 0; };
    auto build_record = [&](u32 record, u32 vtable) {
        put(rt, record + 8u, vtable);
        put(rt, record + 4u, record + 0xCu);
        put(rt, record, 0u);
    };
    if (flag_clear(p.first_flag)) {
        if (call_set(rt, ctx, 0x08B6441Cu, {p.first_flag}) != 0u) {
            if (flag_clear(p.shared_flag)) {
                if (call_set(rt, ctx, 0x08B6441Cu, {p.shared_flag}) != 0u) {
                    build_record(p.shared_record, p.shared_vtable);
                    call_set(rt, ctx, 0x08B64494u, {p.shared_flag});
                }
            }
            put(rt, p.second_record + 0xCu, p.shared_record);
            call_set(rt, ctx, 0x08B64494u, {p.first_flag});
        }
    }
    if (flag_clear(p.second_flag)) {
        if (call_set(rt, ctx, 0x08B6441Cu, {p.second_flag}) != 0u) {
            build_record(p.second_record, p.second_vtable);
            call_set(rt, ctx, 0x08B64494u, {p.second_flag});
        }
    }
    ctx.gpr[2] = p.second_record;
}

// Vector<8-byte element>::reserve(this = a0, count = a1): a count above 0x1FFFFFFF is reported with `length_message` (0x08B5ADDC; the
// original goes on reading `this` from a0, which then holds the message); a larger capacity than the current one allocates count * 8
// (0x08B35DA0), moves the old elements with `move_elements`, destroys the old ones with `destroy_elements`, frees the old block
// (0x08B35254) and stores the new begin / end / capacity.
void vector_reserve(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 length_message, u32 move_elements, u32 destroy_elements) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = 0x1FFFFFFFu < count ? 1u : 0u;
    if (0x1FFFFFFFu < count) call_set(rt, ctx, 0x08B5ADDCu, {length_message});
    const u32 vector = ctx.gpr[4];
    const u32 begin = word(rt, vector);
    const u32 capacity = static_cast<u32>(static_cast<s32>(word(rt, vector + 8u) - begin) >> 3);
    ctx.gpr[2] = capacity < count ? 1u : 0u;
    if (!(capacity < count)) return;
    const u32 end = word(rt, vector + 4u);
    const u32 old_count = static_cast<u32>(static_cast<s32>(end - begin) >> 3);
    ctx.gpr[2] = end - begin;
    u32 bytes = 0u;
    u32 block = 0u;
    if (count != 0u) {
        bytes = count << 3;
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    }
    ctx.gpr[2] = count != 0u ? block : end - begin;
    call_set(rt, ctx, move_elements, {begin, end, block, frame.at(0u)});
    call_set(rt, ctx, destroy_elements, {word(rt, self), word(rt, self + 4u), frame.at(1u)});
    const u32 old_begin = word(rt, self);
    ctx.gpr[4] = old_begin;
    ctx.gpr[2] = old_count << 3;
    if (old_begin != 0u) call_set(rt, ctx, 0x08B35254u, {old_begin});
    ctx.gpr[2] = block + (old_count << 3);
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, block + (old_count << 3));
    put(rt, self, block);
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_089FDD0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x089FDB7Cu, 0x08CA8818u, 0x08CA87B8u, 0x08A85B1Cu, FieldKind::Word, 0x18u, 0x3Cu, StoreKind::Word, 0x18u}); }
void func_08B8E998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve(rt, ctx, 0x08C9F59Cu, 0x08B8E770u, 0x08B8E584u); }
void func_08B95FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guard_static_pair(rt, ctx, {0x08D75028u, 0x08D75020u, 0x08D750F0u, 0x08D752A8u, 0x08CBF670u, 0x08D752FCu, 0x08CBF928u}); }
void func_0884E278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x24u), G[21]); G[21] = G[4]; G[4] = G[20]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[6]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x14u), G[17]); call_here(rt, ctx, 0x0884D660u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9D198u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884D3D8u); G[17] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; if (static_cast<std::int32_t>(G[18]) > 0) { G[16] = word(rt, G[19]); G[4] = 0x08C9D158u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2] != 0u ? 1u : 0u; G[4] = G[17]; call_here(rt, ctx, 0x08897070u); } else { call_here(rt, ctx, 0x08897060u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[21]; if (G[21] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08C269C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[5]; G[5] = G[8]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; G[4] = G[18]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[16] = G[6]; call_here(rt, ctx, 0x08A76ABCu); G[5] = word(rt, G[17] + 0x18u); G[16] = word(rt, G[16]); G[6] = word(rt, G[17] + 0x14u); G[4] = sra(G[5], 1u); G[3] = G[5] & 1u; G[7] = G[6]; G[4] = G[16] + G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4]) + G[6]; G[7] = word(rt, G[2]); } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[17] + 0x20u); G[3] = sra(G[2], 1u); G[2] = G[2] & 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[17] + 0x1Cu); G[2] = word(rt, G[4]) + G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[17] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[18]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B8C564u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0884FF84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A5D0Cu); G[4] = 0x1B0u; G[16] = G[2]; call_here(rt, ctx, 0x08B2B5E0u); G[6] = G[2]; G[8] = G[16] + 0x1E8u; G[7] = G[2]; G[16] = G[16] + 0x38u; do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[7], G[2]); G[16] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[16] != G[8]); G[2] = 0x08CE0000u; G[5] = word(rt, 0x08CE2D04u); G[8] = 0x0884F734u; G[4] = frame.at(0u); G[7] = 0xFFFFFFFFu; G[9] = 0u; call_here(rt, ctx, 0x08A52AB8u); G[5] = word(rt, frame.at(0u)); if (G[18] != frame.at(0u)) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[3] = 0x08CE0000u; G[2] = word(rt, 0x08CE2CECu) + 1u; put(rt, 0x08CE2CECu, G[2]); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A0DA4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x08A0D7D0u, 0x08CA9574u, 0x08CA9484u, 0x08A85B1Cu, FieldKind::Signed16, 0x118u, 0x3Cu, StoreKind::Call, 0x08A0D4B0u}); }
void func_0897A7BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_string(rt, ctx, {false, 0x8u, 0xCu, 0x089C9C34u, 0x08CA3268u, 0x08CA2F84u, 0x089C66F4u}); }
void func_08981A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x0897E844u, 0x08CA3400u, 0x08CA3390u, 0x08A87EECu, FieldKind::Float, 0x14Cu, 0x54u, StoreKind::Float, 0x140u}); }
void func_08981D40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x0897E844u, 0x08CA3400u, 0x08CA3390u, 0x08A87EECu, FieldKind::Float, 0x190u, 0x54u, StoreKind::ByteAndFloat, 0xF0u}); }
void func_089C7314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_string(rt, ctx, {true, 0xCu, 0x8u, 0x089C5850u, 0x08CA63A0u, 0x08CA6304u, 0x089C7234u}); }
void func_08BF7448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = G[6]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4] + 4u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); bool one = G[5] != 0u; if (!one) { G[22] = 0u; one = G[6] == G[20]; if (!one) { G[4] = word(rt, G[7]); G[2] = word(rt, G[6] + 0x10u); G[16] = word(rt, G[4] - 0xCu); G[5] = G[2]; put(rt, frame.at(0u), G[16]); G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(4u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[2] = frame.at(0u); put(rt, frame.at(4u), G[17]); G[16] = G[16] - G[17]; G[6] = word(rt, G[2]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; one = static_cast<std::int32_t>(G[16]) < 0; } } if (one) G[22] = 1u; G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[2] = G[2] + 0x10u; G[5] = G[19]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B88F44u); G[5] = G[19] + 4u; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x089FE2F0u); } G[4] = G[22]; G[6] = G[21]; G[7] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08B5F2B8u); G[3] = word(rt, G[18] + 0x14u) + 1u; G[2] = G[16]; put(rt, G[18] + 0x14u, G[3]); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08BA8600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve(rt, ctx, 0x08CA0708u, 0x08BA7FD4u, 0x08BA76B8u); }
void func_08BA9CC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve(rt, ctx, 0x08CA0730u, 0x08BA91A8u, 0x08BA8D90u); }
void func_08B96658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guard_static_pair(rt, ctx, {0x08D75068u, 0x08D75060u, 0x08D750F0u, 0x08D752A8u, 0x08CBF670u, 0x08D752B8u, 0x08CBF670u}); }
void func_08B96D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guard_static_pair(rt, ctx, {0x08D75078u, 0x08D75070u, 0x08D750F0u, 0x08D752A8u, 0x08CBF670u, 0x08D75380u, 0x08CBF9F0u}); }
void func_08B981C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guard_static_pair(rt, ctx, {0x08D750B8u, 0x08D750B0u, 0x08D750F0u, 0x08D752A8u, 0x08CBF670u, 0x08D753D4u, 0x08CBFCD0u}); }
void func_08BA5D8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guard_static_pair(rt, ctx, {0x08D750D8u, 0x08D750D0u, 0x08D750F0u, 0x08D752A8u, 0x08CBF670u, 0x08D75DD4u, 0x08CC0FD8u}); }
void func_08BA706C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guard_static_pair(rt, ctx, {0x08D750E8u, 0x08D750E0u, 0x08D750F0u, 0x08D752A8u, 0x08CBF670u, 0x08D75E58u, 0x08CC1850u}); }
void func_08C44A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[16] = 0x08D70000u; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); G[2] = sext(load8(0x08D74FA8u), 8u); G[4] = 0x08D74FA8u; if (G[2] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[4] = 0x08D70000u; if (G[2] != 0u) { G[2] = sext(load8(0x08D74FC8u), 8u); G[17] = 0x08D74FC8u; G[4] = G[17]; G[18] = 0x08D70000u; if (G[2] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[4] = G[17]; if (G[2] != 0u) { G[3] = 0x08CD7988u; G[5] = 0x08D772A8u; G[2] = 0x08D7729Cu; put(rt, G[2] + 8u, G[3]); put(rt, G[2] + 4u, G[5]); put(rt, 0x08D7729Cu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[3] = 0x08D7729Cu; G[2] = 0x08D70000u; G[4] = 0x08D74FA8u; put(rt, 0x08D772B8u, G[3]); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D70000u; G[3] = sext(load8(0x08D74FA0u), 8u); G[16] = 0x08D74FA0u; G[17] = 0x08D70000u; G[4] = G[16]; if (G[3] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[4] = G[16]; if (G[2] != 0u) { G[3] = 0x08CD7988u; G[5] = 0x08D772B8u; G[2] = 0x08D772ACu; put(rt, G[2] + 8u, G[3]); put(rt, G[2] + 4u, G[5]); put(rt, 0x08D772ACu, 0u); call_here(rt, ctx, 0x08B64494u); } } G[2] = 0x08D772ACu; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } }
void func_0884E7E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x24u), G[21]); G[21] = G[4]; G[4] = G[20]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[6]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x14u), G[17]); call_here(rt, ctx, 0x0884D660u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9D198u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0884D3D8u); G[17] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; if (static_cast<std::int32_t>(G[18]) > 0) { G[16] = word(rt, G[19]); G[4] = 0x08C9D158u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2] != 0u ? 1u : 0u; G[4] = G[17]; call_here(rt, ctx, 0x088970C8u); } else { call_here(rt, ctx, 0x088970B8u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[21]; if (G[21] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_088560B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x24u), G[21]); G[21] = G[4]; G[4] = G[20]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[6]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x14u), G[17]); call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9E188u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[17] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; if (static_cast<std::int32_t>(G[18]) > 0) { G[16] = word(rt, G[19]); G[4] = 0x08C9E14Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2] != 0u ? 1u : 0u; G[4] = G[17]; call_here(rt, ctx, 0x088A5938u); } else { call_here(rt, ctx, 0x088A5928u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[21]; if (G[21] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08C282B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[5]; G[5] = G[8]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; G[4] = G[18]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[16] = G[6]; call_here(rt, ctx, 0x08A80BACu); G[5] = word(rt, G[17] + 0x18u); G[16] = word(rt, G[16]); G[6] = word(rt, G[17] + 0x14u); G[4] = sra(G[5], 1u); G[3] = G[5] & 1u; G[7] = G[6]; G[4] = G[16] + G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4]) + G[6]; G[7] = word(rt, G[2]); } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[17] + 0x20u); G[3] = sra(G[2], 1u); G[2] = G[2] & 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[17] + 0x1Cu); G[2] = word(rt, G[4]) + G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[17] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[18]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B8C564u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08C2A304(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[5]; G[5] = G[8]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; G[4] = G[18]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[16] = G[6]; call_here(rt, ctx, 0x08A87380u); G[5] = word(rt, G[17] + 0x18u); G[16] = word(rt, G[16]); G[6] = word(rt, G[17] + 0x14u); G[4] = sra(G[5], 1u); G[3] = G[5] & 1u; G[7] = G[6]; G[4] = G[16] + G[4]; if (G[3] != 0u) { G[2] = word(rt, G[4]) + G[6]; G[7] = word(rt, G[2]); } call_here(rt, ctx, G[7]); G[5] = G[2]; G[2] = word(rt, G[17] + 0x20u); G[3] = sra(G[2], 1u); G[2] = G[2] & 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[17] + 0x1Cu); G[2] = word(rt, G[4]) + G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[17] + 0x1Cu); G[4] = G[3] + G[5]; } G[5] = G[18]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B8C564u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08850080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A5D0Cu); G[4] = 0x60u; G[16] = G[2]; call_here(rt, ctx, 0x08B2B5E0u); G[6] = G[2]; G[8] = G[16] + 0x248u; G[7] = G[2]; G[16] = G[16] + 0x1E8u; do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[7], G[2]); G[16] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[16] != G[8]); G[2] = 0x08CE0000u; G[5] = word(rt, 0x08CE2D00u); G[8] = 0x0884F734u; G[4] = frame.at(0u); G[7] = 0xFFFFFFFFu; G[9] = 0u; call_here(rt, ctx, 0x08A52AB8u); G[5] = word(rt, frame.at(0u)); if (G[18] != frame.at(0u)) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[3] = 0x08CE0000u; G[2] = word(rt, 0x08CE2CECu) + 1u; put(rt, 0x08CE2CECu, G[2]); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08850370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A5D0Cu); G[4] = 0x70u; G[16] = G[2]; call_here(rt, ctx, 0x08B2B5E0u); G[6] = G[2]; G[8] = G[16] + 0x300u; G[7] = G[2]; G[16] = G[16] + 0x290u; do { G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[7], G[2]); G[16] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[16] != G[8]); G[2] = 0x08CE0000u; G[5] = word(rt, 0x08CE2CF4u); G[8] = 0x0884F734u; G[4] = frame.at(0u); G[7] = 0xFFFFFFFFu; G[9] = 0u; call_here(rt, ctx, 0x08A52AB8u); G[5] = word(rt, frame.at(0u)); if (G[18] != frame.at(0u)) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[3] = 0x08CE0000u; G[2] = word(rt, 0x08CE2CECu) + 1u; put(rt, 0x08CE2CECu, G[2]); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A0DB48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x08A0D7D0u, 0x08CA9574u, 0x08CA9484u, 0x08A85B1Cu, FieldKind::Signed16, 0x11Au, 0x3Cu, StoreKind::Call, 0x08A0D4D4u}); }
void func_08A0DC44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x08A0D7D0u, 0x08CA9574u, 0x08CA9484u, 0x08A85B1Cu, FieldKind::Signed16, 0x11Cu, 0x3Cu, StoreKind::Call, 0x08A0D4F8u}); }
void func_089C7610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_string(rt, ctx, {false, 0x8u, 0xCu, 0x089C5850u, 0x08CA63A0u, 0x08CA6304u, 0x089C66F4u}); }
void func_08A41E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_string(rt, ctx, {false, 0x8u, 0xCu, 0x08A40E90u, 0x08CADBFCu, 0x08CADB98u, 0x08A41348u}); }
void func_0898C848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x089896CCu, 0x08CA39F0u, 0x08CA39E8u, 0x08A87EECu, FieldKind::Float, 0x14Cu, 0x54u, StoreKind::Float, 0x140u}); }
void func_0898F85C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x0898E170u, 0x08CA3CB8u, 0x08CA3C7Cu, 0x08A87EECu, FieldKind::Float, 0x18Cu, 0x54u, StoreKind::Float, 0x180u}); }
void func_0898CB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x089896CCu, 0x08CA39F0u, 0x08CA39E8u, 0x08A87EECu, FieldKind::Float, 0x190u, 0x54u, StoreKind::ByteAndFloat, 0xF0u}); }
void func_0898FB74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { property_accessor(rt, ctx, {0x0898E170u, 0x08CA3CB8u, 0x08CA3C7Cu, 0x08A87EECu, FieldKind::Float, 0x1D0u, 0x54u, StoreKind::ByteAndFloat, 0x130u}); }
void func_08A9A09C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_string(rt, ctx, {true, 0xCu, 0x8u, 0x08A98674u, 0x08CB23CCu, 0x08CB2328u, 0x08A9A048u}); }
void func_08A9A310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_member_string(rt, ctx, {true, 0xCu, 0x8u, 0x08A98674u, 0x08CB23CCu, 0x08CB2328u, 0x08A9A1A0u}); }
void func_08C016FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = G[6]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4] + 4u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); bool one = G[5] != 0u; if (!one) { G[22] = 0u; one = G[6] == G[20]; if (!one) { G[4] = word(rt, G[7]); G[2] = word(rt, G[6] + 0x10u); G[16] = word(rt, G[4] - 0xCu); G[5] = G[2]; put(rt, frame.at(0u), G[16]); G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(4u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[2] = frame.at(0u); put(rt, frame.at(4u), G[17]); G[16] = G[16] - G[17]; G[6] = word(rt, G[2]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; one = static_cast<std::int32_t>(G[16]) < 0; } } if (one) G[22] = 1u; G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[2] = G[2] + 0x10u; G[5] = G[19]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B88F44u); G[5] = G[19] + 4u; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x089F09ECu); } G[4] = G[22]; G[6] = G[21]; G[7] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08B5F2B8u); G[3] = word(rt, G[18] + 0x14u) + 1u; G[2] = G[16]; put(rt, G[18] + 0x14u, G[3]); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08C1C0A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = G[6]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4] + 4u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); bool one = G[5] != 0u; if (!one) { G[22] = 0u; one = G[6] == G[20]; if (!one) { G[4] = word(rt, G[7]); G[2] = word(rt, G[6] + 0x10u); G[16] = word(rt, G[4] - 0xCu); G[5] = G[2]; put(rt, frame.at(0u), G[16]); G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(4u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[2] = frame.at(0u); put(rt, frame.at(4u), G[17]); G[16] = G[16] - G[17]; G[6] = word(rt, G[2]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; one = static_cast<std::int32_t>(G[16]) < 0; } } if (one) G[22] = 1u; G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[2] = G[2] + 0x10u; G[5] = G[19]; G[4] = G[2]; if (G[2] != 0u) { call_here(rt, ctx, 0x08B88F44u); G[5] = G[19] + 4u; G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08A40C24u); } G[4] = G[22]; G[6] = G[21]; G[7] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08B5F2B8u); G[3] = word(rt, G[18] + 0x14u) + 1u; G[2] = G[16]; put(rt, G[18] + 0x14u, G[3]); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_230
} // namespace gt::game

#define HOT230(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_230::name, gt::ReturnKind::kind)
HOT230(0x089FDD0Cu, func_089FDD0C, Word);
HOT230(0x08B8E998u, func_08B8E998, Word);
HOT230(0x08B95FB4u, func_08B95FB4, Word);
// HOT230(0x0884E278u, func_0884E278, Word);  // no comparable synthetic case yet
HOT230(0x08C269C8u, func_08C269C8, Word);
// HOT230(0x0884FF84u, func_0884FF84, Word);  // no comparable synthetic case yet
HOT230(0x08A0DA4Cu, func_08A0DA4C, Word);
HOT230(0x0897A7BCu, func_0897A7BC, Word);
HOT230(0x08981A28u, func_08981A28, Word);
HOT230(0x08981D40u, func_08981D40, Word);
HOT230(0x089C7314u, func_089C7314, Word);
// HOT230(0x08BF7448u, func_08BF7448, Word);  // no comparable synthetic case yet
HOT230(0x08BA8600u, func_08BA8600, Word);
HOT230(0x08BA9CC4u, func_08BA9CC4, Word);
HOT230(0x08B96658u, func_08B96658, Word);
HOT230(0x08B96D3Cu, func_08B96D3C, Word);
HOT230(0x08B981C8u, func_08B981C8, Word);
HOT230(0x08BA5D8Cu, func_08BA5D8C, Word);
HOT230(0x08BA706Cu, func_08BA706C, Word);
HOT230(0x08C44A58u, func_08C44A58, Word);
// HOT230(0x0884E7E4u, func_0884E7E4, Word);  // no comparable synthetic case yet
// HOT230(0x088560B8u, func_088560B8, Word);  // no comparable synthetic case yet
// HOT230(0x08C282B0u, func_08C282B0, Word);  // no comparable synthetic case yet
// HOT230(0x08C2A304u, func_08C2A304, Word);  // no comparable synthetic case yet
// HOT230(0x08850080u, func_08850080, Word);  // no comparable synthetic case yet
// HOT230(0x08850370u, func_08850370, Word);  // no comparable synthetic case yet
HOT230(0x08A0DB48u, func_08A0DB48, Word);
HOT230(0x08A0DC44u, func_08A0DC44, Word);
HOT230(0x089C7610u, func_089C7610, Word);
HOT230(0x08A41E4Cu, func_08A41E4C, Word);
HOT230(0x0898C848u, func_0898C848, Word);
HOT230(0x0898F85Cu, func_0898F85C, Word);
HOT230(0x0898CB60u, func_0898CB60, Word);
HOT230(0x0898FB74u, func_0898FB74, Word);
HOT230(0x08A9A09Cu, func_08A9A09C, Word);
HOT230(0x08A9A310u, func_08A9A310, Word);
// HOT230(0x08C016FCu, func_08C016FC, Word);  // no comparable synthetic case yet
HOT230(0x08C1C0A4u, func_08C1C0A4, Word);
