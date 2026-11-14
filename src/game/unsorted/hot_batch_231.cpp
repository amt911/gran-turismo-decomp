// Hot batch 231: sixty-seven- to seventy-seven-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (getters and setters through a null-checked temporary handle, a shared-pointer release with a tail call,
// registrations of handlers, string compares and pointer-to-member calls).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_231 {
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
    if (!(value >= -2317483648.0f && value < 2317483648.0f)) return 0x7FFFFFFF;
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

// Where one script property keeps its handle filler and the messages reported for a null object / null value holder.
struct PropertyHandles {
    u32 fill_handle;         // fills the handle slot with the object (a0 = slot)
    u32 null_object_message;
    u32 null_holder_message;
};

// Mode 0 of every accessor below: the value the caller reads is written into the result slot at sp + 8 by `write_result`, copied to *a0
// when that is another slot (0x08A5BAD8) and both slots are released (0x08A5B9EC). The handle slot is at sp + 4.
template <typename Fill>
void read_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, u32 out, const PropertyHandles &handles, Fill &&write_result) {
    const u32 handle_slot = frame.at(4u);
    const u32 result_slot = frame.at(8u);
    call_set(rt, ctx, handles.fill_handle, {handle_slot});
    const u32 object = word(rt, handle_slot);
    if (object == 0u) report_null(rt, ctx, handles.null_object_message);
    write_result(object, handle_slot, result_slot);
    const u32 value = word(rt, result_slot);
    ctx.gpr[5] = value;
    if (out != result_slot) call_set(rt, ctx, 0x08A5BAD8u, {out, value});
    call_set(rt, ctx, 0x08A5B9ECu, {result_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
}

// A boolean property (a0 the output slot, a2 the mode 0 = read / 1 = write, a3 a holder of the new value): reads the byte at object +
// `field` as 0 / 1 (0x08A85B1C); writing asks the holder's virtual member +0x3C and stores its truth value into the record at object +
// `record` (+0x34 and +0x35) together with the mode (+0xC4); any other mode returns 1.
void bool_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const PropertyHandles &handles, u32 field, u32 record) {
    const u32 out = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (mode == 0u) {
        read_property(rt, ctx, frame, out, handles, [&](u32 object, u32, u32 result_slot) {
            const u32 flag = rt.memory().load8(object + field) != 0u ? 1u : 0u;
            call_set(rt, ctx, 0x08A85B1Cu, {result_slot, flag});
        });
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const u32 handle_slot = frame.at(0u);
    call_set(rt, ctx, handles.fill_handle, {handle_slot});
    const u32 object = word(rt, handle_slot);
    if (object == 0u) report_null(rt, ctx, handles.null_object_message);
    const u32 member = word(rt, holder);
    if (member == 0u) report_null(rt, ctx, handles.null_holder_message);
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const u32 truth = call_set(rt, ctx, ctx.gpr[2], {member}) != 0u ? 1u : 0u;
    const u32 base = object + record;
    ctx.gpr[2] = truth;
    ctx.gpr[3] = base;
    rt.memory().store8(base + 0xC4u, static_cast<std::uint8_t>(mode));
    rt.memory().store8(base + 0x34u, static_cast<std::uint8_t>(truth));
    rt.memory().store8(base + 0x35u, static_cast<std::uint8_t>(truth));
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
}

// A float property read and written through the object's virtual members: reading calls virtual +0x1AC, writing asks the holder's
// member +0x54 for the float and hands it (f12) to the object's virtual +0x1B4.
void virtual_float_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const PropertyHandles &handles) {
    const u32 out = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (mode == 0u) {
        read_property(rt, ctx, frame, out, handles, [&](u32 object, u32, u32 result_slot) {
            ctx.gpr[3] = word(rt, object);
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x1ACu);
            call_set(rt, ctx, ctx.gpr[2], {object});
            ctx.fpr[12] = ctx.fpr[0];
            call_set(rt, ctx, 0x08A87EECu, {result_slot});
        });
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const u32 handle_slot = frame.at(0u);
    call_set(rt, ctx, handles.fill_handle, {handle_slot});
    const u32 object = word(rt, handle_slot);
    if (object == 0u) report_null(rt, ctx, handles.null_object_message);
    const u32 member = word(rt, holder);
    ctx.gpr[2] = word(rt, object);
    const u32 setter = word(rt, ctx.gpr[2] + 0x1B4u);
    if (member == 0u) report_null(rt, ctx, handles.null_holder_message);
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x54u);
    call_set(rt, ctx, ctx.gpr[2], {member});
    ctx.fpr[12] = ctx.fpr[0];
    call_set(rt, ctx, setter, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
}

// A string property (a0 the output slot, a1 kept for the filler, a2 the mode, a3 a holder): reading copies the string field at object +
// `field` into the result slot (0x08A042F4); writing copies the holder's string into the slot at sp + 0xC (0x08A03E58), asks the object
// for the handle (a1 as the caller had it) and hands the string's characters to its virtual +0x1BC (null message `null_string_message`).
void string_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const PropertyHandles &handles, u32 field, u32 null_string_message) {
    const u32 out = ctx.gpr[4];
    const u32 caller_a1 = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    if (mode == 0u) {
        const u32 handle_slot = frame.at(4u);
        const u32 result_slot = frame.at(8u);
        call_set(rt, ctx, handles.fill_handle, {handle_slot});
        const u32 object = word(rt, handle_slot);
        if (object == 0u) report_null(rt, ctx, handles.null_object_message);
        call_set(rt, ctx, 0x08A042F4u, {result_slot, object + field});
        const u32 value = word(rt, result_slot);
        ctx.gpr[5] = value;
        if (out != result_slot) call_set(rt, ctx, 0x08A5BAD8u, {out, value});
        call_set(rt, ctx, 0x08A5B9ECu, {result_slot});
        call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const u32 text_slot = frame.at(0xCu);
    call_set(rt, ctx, 0x08A03E58u, {text_slot, holder});
    const u32 handle_slot = frame.at(0u);
    call_set(rt, ctx, handles.fill_handle, {handle_slot, caller_a1});
    const u32 object = word(rt, handle_slot);
    if (object == 0u) report_null(rt, ctx, handles.null_object_message);
    ctx.gpr[2] = word(rt, object);
    const u32 setter = word(rt, ctx.gpr[2] + 0x1BCu);
    const u32 text = word(rt, text_slot);
    if (text == 0u) report_null(rt, ctx, null_string_message);
    call_set(rt, ctx, setter, {object, text + 0x20u});
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {text_slot});
}

// A string-valued field with a change flag (a0 the object, a1 the new string): when the string differs from the field at object + `field`
// (compared by the 0x08B36170 byte compare over the shorter length, then by length) or the field is empty and the flag word at object +
// `flag` is clear, the field is replaced (0x08B855E0 for a non-empty string, otherwise 0x08BEEF60 from an empty one) and the object's
// virtual +0x230 is told (a1 = 1). A field already equal and non-empty with the flag set leaves v0 as the test read it.
void set_string_field(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 field, u32 flag) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 field_address = self + field;
    const u32 new_data = word(rt, text);
    const u32 old_data = word(rt, field_address);
    const u32 new_length = word(rt, new_data - 0xCu);
    const u32 old_length = word(rt, old_data - 0xCu);
    put(rt, frame.at(8u), new_length);
    const u32 shorter_slot = old_length < new_length ? frame.at(0xCu) : frame.at(8u);
    ctx.gpr[2] = frame.at(8u);
    const u32 order = call_set(rt, ctx, 0x08B36170u, {new_data, old_data, word(rt, shorter_slot), shorter_slot});
    u32 replacement_length = 0u;
    if (order != 0u || new_length != old_length) {
        replacement_length = word(rt, new_data - 0xCu);
    } else {
        ctx.gpr[2] = word(rt, old_data - 0xCu);
        if (ctx.gpr[2] == 0u) return;
        ctx.gpr[2] = word(rt, self + flag);
        if (ctx.gpr[2] != 0u) return;
        replacement_length = word(rt, new_data - 0xCu);
    }
    ctx.gpr[2] = replacement_length;
    if (replacement_length != 0u) {
        call_set(rt, ctx, 0x08B855E0u, {field_address, text});
    } else {
        const u32 slot = frame.at(4u);
        ctx.gpr[2] = kEmptyStreamText + 0xCu;
        put(rt, slot, kEmptyStreamText + 0xCu);
        call_set(rt, ctx, 0x08BEEF60u, {slot, field_address});
        const u32 data = word(rt, slot);
        const u32 representation = data - 0xCu;
        if (representation != kEmptyStreamText) {
            const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
            if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {representation, frame.at(0u)});
        }
    }
    ctx.gpr[3] = word(rt, self);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x230u);
    call_set(rt, ctx, ctx.gpr[2], {self, 1u});
}

// Calls a pointer to member function kept in an object (this adjustment at +0x28 / +0x34, function or vtable offset at +0x30), clears the
// member at +8 when the byte at +0xE is zero (0x08AC05F4), and releases the shared count block at +0x2C (use count at +4, weak count at
// +8): at zero its virtual +8 runs, and at zero weak references too its virtual +0xC is tail-called with the block in a0.
void call_member_and_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 block = word(rt, self + 0x2Cu);
        if (block != 0u) put(rt, block + 4u, word(rt, block + 4u) + 1u);
        const u32 adjustment = word(rt, self + 0x34u);
        const u32 base = word(rt, self + 0x28u);
        const u32 this_pointer = base + static_cast<u32>(static_cast<s32>(adjustment) >> 1);
        u32 function = word(rt, self + 0x30u);
        ctx.gpr[2] = adjustment & 1u;
        if ((adjustment & 1u) != 0u) {
            ctx.gpr[2] = this_pointer;
            function = word(rt, word(rt, this_pointer) + function);
        }
        call_set(rt, ctx, function, {this_pointer});
        ctx.gpr[2] = rt.memory().load8(self + 0xEu);
        if (ctx.gpr[2] == 0u) {
            call_set(rt, ctx, 0x08AC05F4u, {self});
            put(rt, self + 8u, 0u);
        }
        if (block != 0u) {
            const u32 uses = word(rt, block + 4u);
            put(rt, block + 4u, uses - 1u);
            ctx.gpr[2] = 1u;
            if (uses == 1u) {
                ctx.gpr[3] = word(rt, block);
                ctx.gpr[2] = word(rt, ctx.gpr[3] + 8u);
                call_set(rt, ctx, ctx.gpr[2], {block});
                const u32 weak = word(rt, block + 8u);
                put(rt, block + 8u, weak - 1u);
                ctx.gpr[2] = 1u;
                if (weak == 1u) {
                    ctx.gpr[2] = word(rt, block);
                    ctx.gpr[4] = block;
                    ctx.gpr[25] = word(rt, ctx.gpr[2] + 0xCu);
                    tail = true;
                }
            }
        }
    }
    if (tail) jump(ctx, ctx.gpr[25]);
}

// Constructor-time registration of one script object (a0 the object): builds the string `text` into the slot at sp + 4 (0x08B85EB8), has
// the object's virtual +0x108 take it, drops it, asks `make_handle` for a handle (released 0x08A5BA08 when set), replaces the handle
// kept at +0x38 (old one 0x08A5B930) and registers three callbacks (`first_register` with the shared table 0x08CF6F14, then two more).
struct Registration {
    u32 text;
    u32 make_handle;
    u32 first_callback;
    u32 second_register;
    u32 second_table;
    u32 second_callback;
    u32 third_table;
    u32 third_callback;
};

void register_script_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const Registration &r) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(4u);
    ctx.gpr[2] = word(rt, self);
    const u32 take = word(rt, ctx.gpr[2] + 0x108u);
    call_set(rt, ctx, 0x08B85EB8u, {slot, r.text, frame.at(1u)});
    call_set(rt, ctx, take, {self, slot});
    drop_string_reference(rt, ctx, frame, word(rt, slot));
    const u32 handle = call_set(rt, ctx, r.make_handle, {});
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 previous = word(rt, self + 0x38u);
    ctx.gpr[2] = previous;
    ctx.gpr[4] = previous;
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, handle);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, r.first_callback});
    call_set(rt, ctx, r.second_register, {self, r.second_table, r.second_callback});
    call_set(rt, ctx, 0x08B87108u, {self, r.third_table, r.third_callback});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08980870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool_property(rt, ctx, {0x0897E844u, 0x08CA3400u, 0x08CA3390u}, 0x124u, 0xF0u); }
void func_08BAF064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_member_and_release(rt, ctx); }
void func_0880A130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, {0x08C98254u, 0x08A003B4u, 0x0880A464u, 0x08B87108u, 0x08C98264u, 0x08809E90u, 0x08C9826Cu, 0x08809DFCu}); }
void func_0898060C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float_property(rt, ctx, {0x0897E844u, 0x08CA3400u, 0x08CA3390u}); }
void func_08981120(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (static_cast<std::int32_t>(G[6]) <= 0) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0897E844u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[1] = float_at(rt, G[16] + 0x160u); ctx.fpr[0] = 0.0f; G[16] = frame.at(8u); G[5] = ctx.fpr[1] <= ctx.fpr[0] ? 1u : 0u; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x0897E844u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3400u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3390u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[0] = 0.0f; if (G[2] == 0u) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; } put_float(rt, G[17] + 0x160u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_089804F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_property(rt, ctx, {0x0897E844u, 0x08CA3400u, 0u}, 0x100u, 0x08CA36A8u); }
void func_08BF6B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); G[18] = G[5]; put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); G[4] = word(rt, G[4] + 0x18u); G[6] = word(rt, G[6]); G[2] = G[4] & 1u; G[5] = word(rt, G[16] + 0x14u); G[4] = G[6] + sra(G[4], 1u); if (G[2] == 0u) { G[2] = G[5]; } else { G[2] = word(rt, G[4]) + G[5]; G[2] = word(rt, G[2]); } call_here(rt, ctx, G[2]); G[5] = word(rt, G[16] + 0x20u); G[6] = G[2]; G[2] = G[5] & 1u; if (G[2] == 0u) { G[3] = word(rt, G[16] + 0x1Cu); } else { G[2] = G[6] + sra(G[5], 1u); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x1Cu); G[3] = word(rt, G[3] + G[4]); } G[4] = G[6] + sra(G[5], 1u); call_here(rt, ctx, G[3]); G[16] = frame.at(4u); G[5] = G[2]; G[4] = G[16]; G[17] = frame.at(8u); call_here(rt, ctx, 0x08B88F44u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08BE9EECu); G[5] = word(rt, frame.at(8u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] = G[4] - 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_089E2F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_string_field(rt, ctx, 0x1A8u, 0x214u); }
void func_0898B690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool_property(rt, ctx, {0x089896CCu, 0x08CA39F0u, 0x08CA39E8u}, 0x124u, 0xF0u); }
void func_0898E6A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { bool_property(rt, ctx, {0x0898E170u, 0x08CA3CB8u, 0x08CA3C7Cu}, 0x164u, 0x130u); }
void func_08BD6860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); put(rt, frame.at(8u), G[31]); put(rt, frame.at(4u), G[17]); G[17] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (G[17] != 0u) { G[3] = G[17] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[5] = word(rt, G[16] + 0x34u); G[2] = G[5] & 1u; G[6] = word(rt, G[16] + 0x28u); if (G[2] != 0u) { G[2] = G[6] + sra(G[5], 1u); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x30u); G[3] = word(rt, G[3] + G[4]); } else { G[3] = word(rt, G[16] + 0x30u); } G[4] = G[6] + sra(G[5], 1u); call_here(rt, ctx, G[3]); G[2] = load8(G[16] + 0xEu); if (G[2] == 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08AC05F4u); put(rt, G[16] + 8u, 0u); } G[3] = G[17] + 4u; if (G[17] != 0u) { G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == 1u) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == 1u) { G[2] = word(rt, G[17]); G[4] = G[17]; G[25] = word(rt, G[2] + 0xCu); tail = true; } } } G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } if (tail) jump(ctx, G[25]); } }
void func_08BD6E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); put(rt, frame.at(8u), G[31]); put(rt, frame.at(4u), G[17]); G[17] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (G[17] != 0u) { G[3] = G[17] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[5] = word(rt, G[16] + 0x34u); G[2] = G[5] & 1u; G[6] = word(rt, G[16] + 0x28u); if (G[2] != 0u) { G[2] = G[6] + sra(G[5], 1u); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x30u); G[3] = word(rt, G[3] + G[4]); } else { G[3] = word(rt, G[16] + 0x30u); } G[4] = G[6] + sra(G[5], 1u); call_here(rt, ctx, G[3]); G[2] = load8(G[16] + 0xEu); if (G[2] == 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08AC05F4u); put(rt, G[16] + 8u, 0u); } G[3] = G[17] + 4u; if (G[17] != 0u) { G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == 1u) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == 1u) { G[2] = word(rt, G[17]); G[4] = G[17]; G[25] = word(rt, G[2] + 0xCu); tail = true; } } } G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } if (tail) jump(ctx, G[25]); } }
void func_08858AC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); G[5] = 0x08C9E5D0u; put(rt, frame.at(0x18u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x14u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08858D90u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08C9E5DCu; G[6] = 0x08858958u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9E5E8u; G[6] = 0x08858870u; call_here(rt, ctx, 0x08B87108u); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0898B42C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float_property(rt, ctx, {0x089896CCu, 0x08CA39F0u, 0x08CA39E8u}); }
void func_0898E594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { virtual_float_property(rt, ctx, {0x0898E170u, 0x08CA3CB8u, 0x08CA3C7Cu}); }
void func_0898BF40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (static_cast<std::int32_t>(G[6]) <= 0) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x089896CCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[1] = float_at(rt, G[16] + 0x160u); ctx.fpr[0] = 0.0f; G[16] = frame.at(8u); G[5] = ctx.fpr[1] <= ctx.fpr[0] ? 1u : 0u; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x089896CCu); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA39F0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA39E8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[0] = 0.0f; if (G[2] == 0u) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; } put_float(rt, G[17] + 0x160u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0898EF54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (static_cast<std::int32_t>(G[6]) <= 0) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0898E170u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } ctx.fpr[1] = float_at(rt, G[16] + 0x1A0u); ctx.fpr[0] = 0.0f; G[16] = frame.at(8u); G[5] = ctx.fpr[1] <= ctx.fpr[0] ? 1u : 0u; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x0898E170u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CA3CB8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CA3C7Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[0] = 0.0f; if (G[2] == 0u) { G[1] = 0x3F800000u; ctx.fpr[0] = 1.0f; } put_float(rt, G[17] + 0x1A0u, ctx.fpr[0]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0898B318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_property(rt, ctx, {0x089896CCu, 0x08CA39F0u, 0u}, 0x100u, 0x08CA3C54u); }
void func_0898E480(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_property(rt, ctx, {0x0898E170u, 0x08CA3CB8u, 0u}, 0x140u, 0x08CA3CF4u); }
void func_08BFF7B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); G[18] = G[5]; put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); G[4] = word(rt, G[4] + 0x18u); G[6] = word(rt, G[6]); G[2] = G[4] & 1u; G[5] = word(rt, G[16] + 0x14u); G[4] = G[6] + sra(G[4], 1u); if (G[2] == 0u) { G[2] = G[5]; } else { G[2] = word(rt, G[4]) + G[5]; G[2] = word(rt, G[2]); } call_here(rt, ctx, G[2]); G[5] = word(rt, G[16] + 0x20u); G[6] = G[2]; G[2] = G[5] & 1u; if (G[2] == 0u) { G[3] = word(rt, G[16] + 0x1Cu); } else { G[2] = G[6] + sra(G[5], 1u); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x1Cu); G[3] = word(rt, G[3] + G[4]); } G[4] = G[6] + sra(G[5], 1u); call_here(rt, ctx, G[3]); G[16] = frame.at(4u); G[5] = G[2]; G[4] = G[16]; G[17] = frame.at(8u); call_here(rt, ctx, 0x08B88F44u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08BE9EECu); G[5] = word(rt, frame.at(8u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] = G[4] - 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08C0FC84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); G[18] = G[5]; put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); G[4] = word(rt, G[4] + 0x18u); G[6] = word(rt, G[6]); G[2] = G[4] & 1u; G[5] = word(rt, G[16] + 0x14u); G[4] = G[6] + sra(G[4], 1u); if (G[2] == 0u) { G[2] = G[5]; } else { G[2] = word(rt, G[4]) + G[5]; G[2] = word(rt, G[2]); } call_here(rt, ctx, G[2]); G[5] = word(rt, G[16] + 0x20u); G[6] = G[2]; G[2] = G[5] & 1u; if (G[2] == 0u) { G[3] = word(rt, G[16] + 0x1Cu); } else { G[2] = G[6] + sra(G[5], 1u); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x1Cu); G[3] = word(rt, G[3] + G[4]); } G[4] = G[6] + sra(G[5], 1u); call_here(rt, ctx, G[3]); G[16] = frame.at(4u); G[5] = G[2]; G[4] = G[16]; G[17] = frame.at(8u); call_here(rt, ctx, 0x08B88F44u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08BE9EECu); G[5] = word(rt, frame.at(8u)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] = G[4] - 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_089E3200(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_string_field(rt, ctx, 0x1A4u, 0x20Cu); }
void func_089E3478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_string_field(rt, ctx, 0x1ACu, 0x21Cu); }
void func_08A94338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, {0x08CB2108u, 0x08A719ACu, 0x08A9421Cu, 0x08B8A004u, 0x08CF6F24u, 0x08A93F44u, 0x08CB2114u, 0x08A93E5Cu}); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_231
} // namespace gt::game

#define HOT231(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_231::name, gt::ReturnKind::kind)
HOT231(0x08980870u, func_08980870, Word);
HOT231(0x08BAF064u, func_08BAF064, Word);
HOT231(0x0880A130u, func_0880A130, Word);
HOT231(0x0898060Cu, func_0898060C, Word);
// HOT231(0x08981120u, func_08981120, Word);  // no comparable synthetic case yet
HOT231(0x089804F8u, func_089804F8, Word);
// HOT231(0x08BF6B98u, func_08BF6B98, Word);  // no comparable synthetic case yet
HOT231(0x089E2F88u, func_089E2F88, Word);
HOT231(0x0898B690u, func_0898B690, Word);
HOT231(0x0898E6A4u, func_0898E6A4, Word);
HOT231(0x08BD6860u, func_08BD6860, Word);
HOT231(0x08BD6E64u, func_08BD6E64, Word);
HOT231(0x08858AC4u, func_08858AC4, Word);
HOT231(0x0898B42Cu, func_0898B42C, Word);
HOT231(0x0898E594u, func_0898E594, Word);
// HOT231(0x0898BF40u, func_0898BF40, Word);  // no comparable synthetic case yet
// HOT231(0x0898EF54u, func_0898EF54, Word);  // no comparable synthetic case yet
HOT231(0x0898B318u, func_0898B318, Word);
HOT231(0x0898E480u, func_0898E480, Word);
// HOT231(0x08BFF7B8u, func_08BFF7B8, Word);  // no comparable synthetic case yet
// HOT231(0x08C0FC84u, func_08C0FC84, Word);  // no comparable synthetic case yet
HOT231(0x089E3200u, func_089E3200, Word);
HOT231(0x089E3478u, func_089E3478, Word);
HOT231(0x08A94338u, func_08A94338, Word);
