// Hot batch 325: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
// Two of them (0x08B2A680, 0x08B44528) are rewritten as structured C from the disassembly (structured rewrites, batch 89), the ones the synthetic gate compares;
// the other emitter bodies were never compared by it (registrations commented out or not comparable) and stay as they are.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_325 {
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
// `trunc.w.s` as the recompiled baseline does it: NaN and +inf give 0x7FFFFFFF, -inf 0x80000000, finite values
// truncate toward zero and clamp to the int32 range.
std::uint32_t trunc_to_word(float value) {
    if (std::isnan(value)) return 0x7FFFFFFFu;
    const double t = std::trunc(static_cast<double>(value));
    if (t >= 2147483647.0) return 0x7FFFFFFFu;
    if (t <= -2147483648.0) return 0x80000000u;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(t));
}
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
std::int32_t trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
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

using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Where the prologue of a function stores a callee-saved register: `reg` 0-31 is a general register, 32-63 a floating one (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};

// The callee-saved registers (s0-s7, fp, f20-f31) a function keeps values in, and the stores of its prologue: the rewrite stores them where the asm
// does (a callee can read the stack below its caller's frame), sets the registers as the asm does because callees can read them too, and gives
// the caller's values back when it returns.
class SavedRegisters {
public:
    SavedRegisters(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        fp_ = ctx.gpr[30];
        for (unsigned i = 0; i < 12; ++i) fpr_[i] = ctx.fpr[20 + i];
        for (const SavedSlot &slot : slots) {
            if (slot.reg < 32u) put(rt, frame.at(slot.offset), ctx.gpr[slot.reg]);
            else put_float(rt, frame.at(slot.offset), ctx.fpr[slot.reg - 32u]);
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
        for (unsigned i = 0; i < 12; ++i) ctx_.fpr[20 + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

u32 signed_byte(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
u32 signed_half(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}

// The `bne v0, v1; addiu v0, v0, -1` loop the compiler emitted as a delay: it ends with v0 = -2 (and v1 = -1 as it was set up).
void delay_loop(psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0xFFFFFFFEu;
    ctx.gpr[3] = 0xFFFFFFFFu;
}


// 0x08A5BB7C: the null-handle report every checked handle load calls with its message in a0.
void report_null(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 message) { call_set(rt, ctx, 0x08A5BB7Cu, {message}); }

// Drops one reference of the string whose characters start at `data` (representation at data - 0xC, count at data - 4): the shared empty
// representation `empty` is left alone, otherwise 0x08B60804 decrements the count and `release` frees the representation (a1 = the stack
// address sp + release_slot) at zero or below. v0 is the shared representation's address until a call replaces it.
void drop_string_reference(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, u32 data, u32 empty, u32 release,
                           unsigned representation_register = 16u, u32 release_slot = 0u) {
    const u32 representation = data - 0xCu;
    ctx.gpr[2] = empty;
    ctx.gpr[4] = data - 4u;
    if (representation == empty) return;
    ctx.gpr[representation_register] = representation;
    const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
    ctx.gpr[4] = representation;
    if (remaining <= 0) call_set(rt, ctx, release, {representation, frame.at(release_slot)});
}

// ll / sc decrement of the word at `address` (nothing else runs here, so the store always succeeds and v0 is 1 afterwards): true when it was 1.
bool decrement_reaches_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    return old == 1u;
}

// Drops one use of a shared holder (use count +4, weak count +8): the last use calls the virtual +8, the last weak count the virtual +0xC.
void release_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 holder) {
    if (!decrement_reaches_zero(rt, ctx, holder + 4u)) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
    if (!decrement_reaches_zero(rt, ctx, holder + 8u)) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
}

// 0x0897AD70: for a count a2 > 0, with a0 the destination string and a3 the address of a checked handle (null reports 0x08CA2F84): 0x089C9C34 fills the checked word at
// sp + 0xC (null reports 0x08CA3268), the handle's virtual +0xC fills the temporary string at sp + 8, 0x089C5700(sp + 0x10, word, sp + 8) builds the string at sp + 0x10 and the
// temporary is dropped (empty representation 0x08D74FD0, freed by 0x08B8542C); a non-empty result is copied (0x08B2B5E0 / 0x08B79A88) and made into the string at sp + 4
// (0x08A44D9C), which is moved into a0 (0x08A5BAD8) unless it is that slot, released (0x08A5B9EC); the string at sp + 0x10 is dropped last.
void build_text_from_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 handle_address = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x30u}, {16u, 0x20u}, {31u, 0x34u}, {19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}});
    constexpr u32 kEmpty = 0x08D74FD0u;
    ctx.gpr[20] = destination;
    ctx.gpr[16] = handle_address;
    if (count <= 0) return;
    const u32 object_slot = frame.at(0xCu);
    ctx.gpr[19] = object_slot;
    call_set(rt, ctx, 0x089C9C34u, {object_slot});
    const u32 object = word(rt, object_slot);
    ctx.gpr[18] = object;
    if (object == 0u) report_null(rt, ctx, 0x08CA3268u);
    const u32 handle = word(rt, handle_address);
    ctx.gpr[17] = handle;
    if (handle == 0u) report_null(rt, ctx, 0x08CA2F84u);
    const u32 temporary = frame.at(8u);
    ctx.gpr[16] = temporary;
    ctx.gpr[3] = word(rt, handle);
    call_indirect(rt, ctx, word(rt, word(rt, handle) + 0xCu), {temporary, handle});
    call_set(rt, ctx, 0x089C5700u, {frame.at(0x10u), object, temporary});
    ctx.gpr[18] = kEmpty;
    drop_string_reference(rt, ctx, frame, word(rt, temporary), kEmpty, 0x08B8542Cu, 16u, 1u);
    call_set(rt, ctx, 0x08A5B9ECu, {object_slot});
    u32 data = word(rt, frame.at(0x10u));
    u32 representation = data - 0xCu;
    ctx.gpr[16] = representation;
    const u32 length = word(rt, representation);
    ctx.gpr[2] = length;
    if (length != 0u) {
        const u32 copy = call_set(rt, ctx, 0x08B2B5E0u, {length});
        ctx.gpr[16] = copy;
        call_set(rt, ctx, 0x08B79A88u, {copy, word(rt, frame.at(0x10u)), word(rt, word(rt, frame.at(0x10u)) - 0xCu)});
        const u32 slot = frame.at(4u);
        ctx.gpr[17] = slot;
        call_set(rt, ctx, 0x08A44D9Cu, {slot, copy, word(rt, word(rt, frame.at(0x10u)) - 0xCu), 1u});
        if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
        data = word(rt, frame.at(0x10u));
        representation = data - 0xCu;
        ctx.gpr[16] = representation;
    }
    drop_string_reference(rt, ctx, frame, data, kEmpty, 0x08B8542Cu, 16u, 0u);
}

// 0x08899BE4: sets the graphics library's state up in the usual order (0x08AFBF3C first, the unit scale 1.0 for 0x08B5A838, the switches 0x08B55AAC for 3, 4, 5, 6, 0 (on) and 8,
// the table 0x08CDD0A0 for 0x08B55AF8, ...) over the 480 x 272 screen and tail-jumps to 0x08AFBEF4.
void set_up_screen_state(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{31u, 0x0u}});
        call_set(rt, ctx, 0x08AFBF3Cu, {});
        ctx.fpr[12] = 1.0f;
        ctx.fpr[14] = 1.0f;
        ctx.fpr[15] = 1.0f;
        ctx.fpr[13] = 1.0f;
        call_set(rt, ctx, 0x08B5A838u, {});
        for (const u32 state : {3u, 4u, 5u, 6u}) call_set(rt, ctx, 0x08B55AACu, {state, 0u});
        call_set(rt, ctx, 0x08B55AACu, {0u, 1u});
        call_set(rt, ctx, 0x08B55AACu, {8u, 0u});
        call_set(rt, ctx, 0x08B55AF8u, {0u, 0x08CDD0A0u, 3u, 4u});
        call_set(rt, ctx, 0x08B55924u, {0u, 4u, 2u});
        ctx.fpr[12] = 1.0f;
        ctx.fpr[13] = 1.0f;
        call_set(rt, ctx, 0x08B55820u, {});
        ctx.fpr[12] = 0.0f;
        ctx.fpr[13] = 0.0f;
        call_set(rt, ctx, 0x08B557E0u, {});
        call_set(rt, ctx, 0x08B58874u, {0x6Fu});
        ctx.fpr[12] = 0.5f;
        call_set(rt, ctx, 0x08B5877Cu, {6u});
        call_set(rt, ctx, 0x08B58874u, {2u});
        call_set(rt, ctx, 0x08B58874u, {0x71u});
        call_set(rt, ctx, 0x08B583D0u, {1u, 0xFFu, 0xFFu});
        call_set(rt, ctx, 0x08B58388u, {0x8000u, 0x8023u, 0x8023u});
        call_set(rt, ctx, 0x08B58AD0u, {0u, 0u, 0u, 1u});
        call_set(rt, ctx, 0x08B55378u, {0u, 0u, 0x1E0u, 0x110u, 0u, 0u, 0x1E0u, 0x110u});
        call_set(rt, ctx, 0x08B58AD0u, {1u, 1u, 1u, 1u});
        call_set(rt, ctx, 0x08B583D0u, {2u, 0x80u, 0x80u});
        call_set(rt, ctx, 0x08B58388u, {0x8000u, 0x8000u, 0x8000u});
        tail = true;
    }
    if (tail) jump(ctx, 0x08AFBEF4u);
}

// 0x08C42530: the first of the elements [a0, a1) (4-byte pointers to a pointer to a string) whose string 0x08B35FD8(**element, a2) finds equal to the key a2, or a1 when none
// (the search is unrolled by four with a remainder of up to three). v0 = that position.
void find_element_by_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 position = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {21u, 0x14u}, {18u, 0x8u}, {16u, 0x0u}, {31u, 0x18u}, {19u, 0xCu}, {17u, 0x4u}});
    const auto matches = [&](u32 element) {
        ctx.gpr[17] = element;
        return call_set(rt, ctx, 0x08B35FD8u, {word(rt, word(rt, element)), key}) == 0u;
    };
    const s32 blocks = static_cast<s32>(last - position) >> 4;
    ctx.gpr[20] = static_cast<u32>(blocks);
    ctx.gpr[21] = last;
    ctx.gpr[18] = key;
    ctx.gpr[16] = position;
    for (s32 block = 0; block < blocks; ++block) {
        ctx.gpr[19] = static_cast<u32>(block + 1);
        for (u32 offset = 0u; offset < 0x10u; offset += 4u) {
            if (matches(position + offset)) {
                ctx.gpr[2] = position + offset;
                return;
            }
        }
        position += 0x10u;
        ctx.gpr[16] = position;
    }
    const s32 remaining = static_cast<s32>(last - position) >> 2;
    u32 checks = remaining == 3 ? 3u : remaining == 2 ? 2u : remaining == 1 ? 1u : 0u;
    for (u32 offset = 0u; offset < checks * 4u; offset += 4u) {
        if (matches(position + offset)) {
            ctx.gpr[2] = position + offset;
            return;
        }
    }
    ctx.gpr[2] = last;
}

// 0x08B413F0 (a0 = the object, a3 = a block of 64-bit values, t0 = a kind, t2 / t3 = two arguments): only for kind 0x20 (else v0 = 0x20): 0x08B3F098(object + 0x8DC, a1, t2, t3) must
// find a record whose byte +4 is set (else v0 = 0); the handle at +0xB0 gives a 64-bit value R through its virtual +0x1C; the magnitude of block[+8] - R is handed to
// 0x08B3EA3C(*(a0 + 0x930), ...) and, when 0x08B3EA88(*(a0 + 0x930), 0x14) is below it, v0 = 1; otherwise block[+0x18] + (block[+0x10] - R) halved (rounding toward
// zero) is stored at +0x920 / +0x924 (v0 = its low word).
void store_midpoint_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 block = ctx.gpr[7];
    const u32 kind = ctx.gpr[8];
    const u32 argument_a = ctx.gpr[10];
    const u32 argument_b = ctx.gpr[11];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {18u, 0x8u}, {21u, 0x14u}, {20u, 0x10u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x18u}});
    ctx.gpr[2] = 0x20u;
    ctx.gpr[19] = block;
    ctx.gpr[18] = object;
    if (kind != 0x20u) return;
    const u32 record = call_set(rt, ctx, 0x08B3F098u, {object + 0x8DCu, ctx.gpr[5], argument_a, argument_b});
    if (record == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = rt.memory().load8(record + 4u);
    if (ctx.gpr[2] == 0u) return;
    const u32 handle = word(rt, object + 0xB0u);
    const u32 low = call_indirect(rt, ctx, word(rt, word(rt, handle) + 0x1Cu), {handle});
    const u32 high = ctx.gpr[3];
    ctx.gpr[20] = low;
    ctx.gpr[21] = high;
    const std::uint64_t reference = (static_cast<std::uint64_t>(high) << 32) | low;
    const auto pair_at = [&](u32 offset) { return (static_cast<std::uint64_t>(word(rt, block + offset + 4u)) << 32) | word(rt, block + offset); };
    const std::int64_t difference = static_cast<std::int64_t>(pair_at(8u) - reference);
    const std::uint64_t magnitude = static_cast<std::uint64_t>(difference < 0 ? -difference : difference);
    ctx.gpr[16] = static_cast<u32>(magnitude);
    ctx.gpr[17] = static_cast<u32>(magnitude >> 32);
    call_set(rt, ctx, 0x08B3EA3Cu, {word(rt, object + 0x930u), ctx.gpr[5], static_cast<u32>(magnitude), static_cast<u32>(magnitude >> 32)});
    call_set(rt, ctx, 0x08B3EA88u, {word(rt, object + 0x930u), 0x14u});
    const std::uint64_t limit = (static_cast<std::uint64_t>(ctx.gpr[3]) << 32) | ctx.gpr[2];
    if (static_cast<std::int64_t>(limit) < static_cast<std::int64_t>(magnitude)) {
        ctx.gpr[2] = 1u;
        return;
    }
    const std::int64_t sum = static_cast<std::int64_t>(pair_at(0x18u) + (pair_at(0x10u) - reference));
    const std::uint64_t half = static_cast<std::uint64_t>(sum / 2);
    put(rt, object + 0x920u, static_cast<u32>(half));
    put(rt, object + 0x924u, static_cast<u32>(half >> 32));
    ctx.gpr[2] = static_cast<u32>(half);
    ctx.gpr[3] = static_cast<u32>(half >> 32);
}

// 0x08B308FC (a0 = the object, a1 = a pass-through, a2 / a3 / t0 / t1 = four arguments, t0 / t1 their low bytes): without a negative word +0xC, 0x08B2FC48(object, sp + 0x18, 0x10, 0, 0)
// lists n 0x98-byte entries (first two words a pair); with the byte +5 set every entry whose 0x08B36170(sp + 0x10, object + 0xA0, 6) is non-zero is run through
// 0x08B30660(object, ..., pair, a1, arguments, stack: t1, 0) and counted when that returns 0, v0 = 0 when any was, else 1; without the byte one call with (word +0xA8 | 1, word +0xAC)
// decides v0 = 1 / 0.
void run_entries_for_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 pass_through = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    const u32 byte_a = ctx.gpr[8] & 0xFFu;
    const u32 byte_b = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0x9E0u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x9D0u}, {20u, 0x9C0u}, {23u, 0x9CCu}, {22u, 0x9C8u}, {21u, 0x9C4u}, {19u, 0x9BCu}, {18u, 0x9B8u}, {17u, 0x9B4u}, {16u, 0x9B0u}, {31u, 0x9D4u}});
    ctx.gpr[8] = byte_a;
    ctx.gpr[9] = byte_b;
    ctx.gpr[30] = pass_through;
    ctx.gpr[20] = object;
    put(rt, frame.at(0x9A0u), first);
    put(rt, frame.at(0x9A4u), second);
    put(rt, frame.at(0x9A8u), byte_a);
    put(rt, frame.at(0x9ACu), byte_b);
    ctx.gpr[2] = word(rt, object + 0xCu);
    if (static_cast<s32>(ctx.gpr[2]) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 entries = frame.at(0x18u);
    ctx.gpr[16] = entries;
    const s32 count = static_cast<s32>(call_set(rt, ctx, 0x08B2FC48u, {object, entries, 0x10u, 0u, 0u}));
    ctx.gpr[22] = static_cast<u32>(count);
    if (count <= 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (rt.memory().load8(object + 5u) != 0u) {
        u32 cursor = entries;
        s32 index = 0;
        s32 matched = 0;
        const u32 pattern = object + 0xA0u;
        ctx.gpr[21] = pattern;
        do {
            const u32 pair_low = word(rt, cursor);
            const u32 pair_high = word(rt, cursor + 4u);
            ctx.gpr[16] = pair_low;
            ctx.gpr[17] = pair_high;
            ++index;
            ctx.gpr[18] = static_cast<u32>(index);
            const u32 hit = call_set(rt, ctx, 0x08B36170u, {frame.at(0x10u), pattern, 6u});
            cursor += 0x98u;
            ctx.gpr[19] = cursor;
            if (hit != 0u) {
                put(rt, frame.at(0u), word(rt, frame.at(0x9ACu)));
                put(rt, frame.at(4u), 0u);
                const u32 result = call_set(rt, ctx, 0x08B30660u, {object, ctx.gpr[5], pair_low, pair_high, pass_through, word(rt, frame.at(0x9A0u)), word(rt, frame.at(0x9A4u)), word(rt, frame.at(0x9A8u))});
                ctx.gpr[2] = result < 1u ? 1u : 0u;
                matched += static_cast<s32>(ctx.gpr[2]);
                ctx.gpr[23] = static_cast<u32>(matched);
            }
        } while (index != count);
        ctx.gpr[2] = matched > 0 ? 0u : 1u;
        return;
    }
    const u32 low = word(rt, object + 0xA8u) | 1u;
    const u32 high = word(rt, object + 0xACu);
    put(rt, frame.at(0u), word(rt, frame.at(0x9ACu)));
    put(rt, frame.at(4u), 0u);
    const u32 result = call_set(rt, ctx, 0x08B30660u, {object, ctx.gpr[5], low, high, pass_through, word(rt, frame.at(0x9A0u)), word(rt, frame.at(0x9A4u)), word(rt, frame.at(0x9A8u))});
    ctx.gpr[2] = result != 0u ? 1u : 0u;
}

// 0x08A9A1A0: joins the strings of the handles in the list a1 (words from +0x14 to +0x18) with the separator a2 into the temporary at sp + 8 (0x08B89054; each handle's virtual +0xC
// makes its text at sp + 4, dropped after the append, empty representation 0x08D74FD0 freed by 0x08B8542C), assigns it to a0 (0x08A6D2D4) and drops it. v0 = a0.
void join_handle_texts(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 list = ctx.gpr[5];
    const u32 separator = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {30u, 0x40u}, {23u, 0x3Cu}, {17u, 0x24u}, {16u, 0x20u}, {31u, 0x44u}});
    constexpr u32 kEmpty = 0x08D74FD0u;
    const u32 joined = frame.at(8u);
    const u32 piece = frame.at(4u);
    ctx.gpr[22] = separator;
    ctx.gpr[21] = kEmpty;
    ctx.gpr[3] = kEmpty + 0xCu;
    ctx.gpr[20] = joined;
    ctx.gpr[18] = list;
    put(rt, joined, kEmpty + 0xCu);
    const s32 count = static_cast<s32>(word(rt, list + 0x18u) - word(rt, list + 0x14u)) >> 2;
    ctx.gpr[19] = static_cast<u32>(count);
    put(rt, frame.at(0x10u), destination);
    if (count > 0) {
        ctx.gpr[23] = piece;
        ctx.gpr[30] = 0x08CB0000u;
        for (s32 index = 0; index != count;) {
            if (index > 0) call_set(rt, ctx, 0x08B89054u, {joined, separator});
            const u32 handle = word(rt, word(rt, list + 0x14u) + (static_cast<u32>(index) << 2));
            ctx.gpr[16] = handle;
            ++index;
            ctx.gpr[17] = static_cast<u32>(index);
            if (handle == 0u) report_null(rt, ctx, 0x08CB2328u);
            call_indirect(rt, ctx, word(rt, word(rt, handle) + 0xCu), {piece, handle});
            call_set(rt, ctx, 0x08B89054u, {joined, piece});
            drop_string_reference(rt, ctx, frame, word(rt, piece), kEmpty, 0x08B8542Cu, 16u, 1u);
        }
    }
    call_set(rt, ctx, 0x08A6D2D4u, {word(rt, frame.at(0x10u)), joined});
    drop_string_reference(rt, ctx, frame, word(rt, joined), kEmpty, 0x08B8542Cu, 16u, 0u);
    ctx.gpr[2] = word(rt, frame.at(0x10u));
}

// 0x089AD88C (a0 = the destination string, a2 = a kind, a3 = four checked handles): only for kind 4 (else nothing): 0x089AB370 fills the checked object at sp + 4 (null reports
// 0x08CA50C4), the floats of the four handles' virtual +0x54 (null ones report 0x08CA50BC) are the arguments f12..f15 of 0x089A88FC(object, 0) whose result is made into a string
// (0x08A85B1C at sp), moved into a0 (0x08A5BAD8) unless it is that slot, and both strings are released (0x08A5B9EC).
void format_four_floats(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 kind = ctx.gpr[6];
    const u32 handles = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {16u, 0x10u}, {54u, 0x38u}, {53u, 0x34u}, {52u, 0x30u}});
    ctx.gpr[2] = 4u;
    ctx.gpr[18] = destination;
    ctx.gpr[17] = handles;
    if (kind != 4u) return;
    const u32 object_slot = frame.at(4u);
    ctx.gpr[20] = object_slot;
    call_set(rt, ctx, 0x089AB370u, {object_slot});
    const u32 object = word(rt, object_slot);
    ctx.gpr[19] = object;
    if (object == 0u) report_null(rt, ctx, 0x08CA50C4u);
    float values[4];
    for (unsigned i = 0; i < 4; ++i) {
        const u32 handle = word(rt, handles + i * 4u);
        ctx.gpr[16] = handle;
        if (handle == 0u) report_null(rt, ctx, 0x08CA50BCu);
        call_indirect(rt, ctx, word(rt, word(rt, handle) + 0x54u), {handle});
        values[i] = ctx.fpr[0];
        if (i < 3) ctx.fpr[22 - i] = values[i];
    }
    ctx.fpr[15] = values[3];
    ctx.fpr[12] = values[0];
    ctx.fpr[13] = values[1];
    ctx.fpr[14] = values[2];
    const u32 text = call_set(rt, ctx, 0x089A88FCu, {object, 0u});
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), text});
    if (destination != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {object_slot});
}

// 0x08B62EA0 (a0 = the table, a1 = a mode, a2 = a base, a3 = another table, t0 = an extra argument kept at sp): equal a2 and t0 with equal words +4 of a0 and a3 give 6; otherwise the
// entries of a0 (count +0xC, 8-byte entries with a flags word at +0x14 + 8 * i, walked from the last) whose flag 2 is set have their object (word +0x10 of the entry) asked, through its virtual +0x20,
// with (a1, a2 + offset, a3, t0), the offset being the flags >> 8 (read through a2 when flag 1 is set; an a1 of -3 skips such an entry): the first answer of 4 or more is returned (|1 for
// flag 1); no answer gives 1.
void query_flagged_entries(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    const u32 base = ctx.gpr[6];
    const u32 other = ctx.gpr[7];
    const u32 extra = ctx.gpr[8];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {31u, 0x34u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    put(rt, frame.at(0u), extra);
    ctx.gpr[30] = other;
    ctx.gpr[23] = mode;
    ctx.gpr[22] = base;
    ctx.gpr[21] = table;
    if (base == extra && word(rt, table + 4u) == word(rt, other + 4u)) {
        ctx.gpr[4] = 6u;
        ctx.gpr[2] = 6u;
        return;
    }
    const u32 count = word(rt, table + 0xCu);
    ctx.gpr[19] = count;
    s32 slot = static_cast<s32>(count) - 1;
    u32 object_cursor = table + count * 8u + 8u;
    for (u32 done = 0u; done != count;) {
        ctx.gpr[16] = static_cast<u32>(slot);
        ctx.gpr[18] = object_cursor;
        const u32 flags = word(rt, table + (static_cast<u32>(slot) << 3) + 0x14u);
        ++done;
        ctx.gpr[17] = done;
        const bool direct = (flags & 1u) != 0u;
        ctx.gpr[20] = flags & 1u;
        bool answered = false;
        if ((flags & 2u) != 0u) {
            u32 offset = static_cast<u32>(static_cast<s32>(flags) >> 8);
            bool skip = false;
            if (direct) {
                if (mode == 0xFFFFFFFDu) {
                    skip = true;
                    --slot;
                } else {
                    offset = word(rt, word(rt, base) + offset);
                }
            }
            if (!skip) {
                const u32 object = word(rt, object_cursor);
                ctx.gpr[4] = object;
                const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x20u), {object, mode, base + offset, other, word(rt, frame.at(0u))});
                if (!(static_cast<s32>(answer) < 4)) {
                    ctx.gpr[2] = direct ? (answer | 1u) : answer;
                    return;
                }
                answered = true;
            }
            if (skip) {
                object_cursor -= 8u;
                continue;
            }
        }
        (void)answered;
        --slot;
        object_cursor -= 8u;
    }
    ctx.gpr[4] = 1u;
    ctx.gpr[2] = 1u;
}

// 0x08A99CB0 (a0 = the list object (handles from +0x14, end +0x18), a1 = a string, a2 = the first index, a3 = a count or negative for all): for each index from a2 up to the end
// (a2 + a3, or the list size when a3 is negative) a temporary string pair is made (0x08A76AA0, 0x08A76ABC from a1 and from the entry's handle word), 0x08A7858C(sp, 0x08CF6FC0, 2, sp + 4) gives a
// checked object whose virtual +0x30 decides: non-zero returns that index, zero goes on; the temporary strings are released (0x08A5B9EC). v0 = the index or -1.
void find_matching_index(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 list = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const s32 first = static_cast<s32>(ctx.gpr[6]);
    const s32 span = static_cast<s32>(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {20u, 0x20u}, {31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    s32 end = span + first;
    ctx.gpr[3] = static_cast<u32>(end);
    ctx.gpr[21] = text;
    ctx.gpr[20] = list;
    if (span < 0) end = static_cast<s32>(word(rt, list + 0x18u) - word(rt, list + 0x14u)) >> 2;
    ctx.gpr[3] = static_cast<u32>(end);
    ctx.gpr[18] = 0xFFFFFFFFu;
    if (!(first < end)) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    s32 index = first;
    ctx.gpr[18] = static_cast<u32>(index);
    ctx.gpr[22] = static_cast<u32>(end);
    u32 offset = static_cast<u32>(first) << 2;
    ctx.gpr[17] = offset;
    const u32 pair_a = frame.at(4u);
    const u32 pair_b = frame.at(8u);
    const u32 slot_end = frame.at(0xCu);
    ctx.gpr[19] = pair_a;
    ctx.gpr[23] = 0x08CF0000u;
    ctx.gpr[30] = slot_end;
    do {
        call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
        call_set(rt, ctx, 0x08A76ABCu, {pair_a, text});
        call_set(rt, ctx, 0x08A76ABCu, {pair_b, word(rt, list + 0x14u) + offset});
        call_set(rt, ctx, 0x08A7858Cu, {frame.at(0u), 0x08CF6FC0u, 2u, pair_a});
        const u32 object = word(rt, frame.at(0u));
        ctx.gpr[16] = object;
        if (object == 0u) report_null(rt, ctx, 0x08CB2328u);
        const bool found = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x30u), {object}) != 0u;
        u32 cursor = slot_end - 4u;
        ctx.gpr[16] = cursor;
        for (;;) {
            call_set(rt, ctx, 0x08A5B9ECu, {cursor});
            if (cursor == pair_a) break;
            cursor -= 4u;
            ctx.gpr[16] = cursor;
        }
        ctx.gpr[4] = frame.at(0u);
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        if (found) {
            ctx.gpr[2] = static_cast<u32>(index);
            return;
        }
        ++index;
        ctx.gpr[18] = static_cast<u32>(index);
        offset += 4u;
        ctx.gpr[17] = offset;
    } while (index != end);
    ctx.gpr[18] = 0xFFFFFFFFu;
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x08A98E14: moves the string elements (words from +0x14 of the object a0) of the index range between a1 and a2 by one place: for a1 < a2 each of the elements a1 + 1..a2 is assigned (0x08A5BAD8,
// unless it is already that slot) to its predecessor, for a2 < a1 each of a1 - 1 down to a2 to its successor, and the string kept from element a1 (made by 0x08A76ABC) is assigned into slot a2 (and
// released, 0x08A5B9EC). v0 = 0 without a downward range, else the last call's.
void shift_string_elements(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 first = static_cast<s32>(ctx.gpr[5]);
    const s32 last = static_cast<s32>(ctx.gpr[6]);
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x28u}, {21u, 0x24u}, {19u, 0x1Cu}, {31u, 0x30u}, {23u, 0x2Cu}, {20u, 0x20u}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[22] = self;
    ctx.gpr[21] = static_cast<u32>(first);
    ctx.gpr[19] = static_cast<u32>(last);
    ctx.gpr[2] = first < last ? 1u : 0u;
    const u32 kept = frame.at(0u);
    const auto move_kept_into_last = [&](u32 base) {
        const u32 slot = base + (static_cast<u32>(last) << 2);
        if (slot != kept) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, kept)});
        call_set(rt, ctx, 0x08A5B9ECu, {kept});
    };
    if (first < last) {
        call_set(rt, ctx, 0x08A76ABCu, {kept, word(rt, self + 0x14u) + (static_cast<u32>(first) << 2)});
        u32 base = word(rt, self + 0x14u);
        u32 destination = static_cast<u32>(first) << 2;
        u32 source = destination + 4u;
        ctx.gpr[23] = self + 0x14u;
        for (s32 step = 0; step != last - first;) {
            const u32 destination_slot = base + destination;
            const u32 source_slot = base + source;
            ++step;
            source += 4u;
            destination += 4u;
            if (source_slot != destination_slot) {
                call_set(rt, ctx, 0x08A5BAD8u, {destination_slot, word(rt, source_slot)});
                base = word(rt, self + 0x14u);
            }
        }
        move_kept_into_last(base);
    }
    ctx.gpr[2] = last < first ? 1u : 0u;
    if (last < first) {
        call_set(rt, ctx, 0x08A76ABCu, {kept, word(rt, self + 0x14u) + (static_cast<u32>(first) << 2)});
        u32 base = word(rt, self + 0x14u);
        u32 destination = static_cast<u32>(first) << 2;
        u32 source = destination - 4u;
        s32 index = first;
        ctx.gpr[20] = self + 0x14u;
        do {
            const u32 destination_slot = base + destination;
            const u32 source_slot = base + source;
            source -= 4u;
            destination -= 4u;
            --index;
            if (source_slot != destination_slot) {
                call_set(rt, ctx, 0x08A5BAD8u, {destination_slot, word(rt, source_slot)});
                base = word(rt, self + 0x14u);
            }
        } while (last < index);
        move_kept_into_last(base);
    }
}

// 0x08C705A4 / 0x08C5592C (width 0x10 / 8; a0 = the stream object, a1 = a block of eight words): the block is copied around the frame; with a bad / fail state (flags 5 of the word +0x14 of the virtual base at
// a0 + *(*a0 - 0xC)) it returns a0 at once; otherwise the stream buffer (+0x78 of that base) is asked through its virtual +0x14(sp + 0xA0, buffer, sp, 0x10) and a result whose first two words are both -1
// sets the fail flag 4 (0x08C54CFC). v0 = a0.
void check_stream_block(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 width) {
    const u32 self = ctx.gpr[4];
    const u32 block = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0xC0u}, {31u, 0xC4u}});
    u32 words[8];
    for (u32 i = 0; i < 8u; ++i) words[i] = word(rt, block + i * 4u);
    for (u32 i = 0; i < 8u; ++i) put(rt, frame.at(0x20u + i * 4u), words[i]);
    const u32 base = self + word(rt, word(rt, self) - 0xCu);
    ctx.gpr[16] = self;
    ctx.gpr[2] = self;
    if ((word(rt, base + 0x14u) & 5u) != 0u) return;
    const u32 buffer = word(rt, base + 0x78u);
    for (u32 i = 0; i < 8u; ++i) {
        put(rt, frame.at(i * 4u), words[i]);
        put(rt, frame.at(0x80u + i * 4u), words[i]);
    }
    call_indirect(rt, ctx, word(rt, word(rt, buffer) + 0x14u), {frame.at(0xA0u), buffer, frame.at(0u), width});
    u32 result[8];
    for (u32 i = 0; i < 8u; ++i) result[i] = word(rt, frame.at(0xA0u + i * 4u));
    for (u32 i = 0; i < 8u; ++i) {
        put(rt, frame.at(0x40u + i * 4u), result[i]);
        put(rt, frame.at(0x60u + i * 4u), result[i]);
    }
    ctx.gpr[2] = self;
    if (result[0] == 0xFFFFFFFFu && result[1] == 0xFFFFFFFFu) {
        const u32 stream = self + word(rt, word(rt, self) - 0xCu);
        call_set(rt, ctx, 0x08C54CFCu, {stream, word(rt, stream + 0x14u) | 4u});
        ctx.gpr[2] = self;
    }
}

// 0x08947CF0 (a0 = the object, f0 = the result): with the fraction of 0x08947AA0 at 1.0 or more, tail-jumps to 0x089491A0(object) with f12 = the float +0x534 of *(a0 + 8); otherwise that float
// is raised (never above 1.0, never below its old value) towards a target from the constants at 0x08CF2EF8..0x08CF2F04, the floats +0x57C / +0x580 and the halfword +0x58E, stored back, and
// 0x089491A0 / 0x0895549C / 0x08947C74 (with the difference of the first two scaled by the second's result) run with it. f0 = the difference.
void ease_towards_target(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {54u, 0x18u}, {31u, 0x8u}, {16u, 0x0u}, {53u, 0x14u}, {52u, 0x10u}});
        ctx.gpr[17] = self;
        const u32 state = word(rt, self + 8u);
        ctx.gpr[16] = state;
        float current = float_at(rt, state + 0x534u);
        const float count = static_cast<float>(static_cast<s32>(signed_half(rt, state + 0x58Eu)));
        ctx.fpr[22] = count;
        ctx.fpr[20] = current;
        call_set(rt, ctx, 0x08947AA0u, {self});
        const float fraction = ctx.fpr[0];
        if (1.0f <= fraction) {
            ctx.fpr[12] = current;
            ctx.gpr[4] = self;
            tail = true;
        } else {
            const float rate = psp_mul(float_at(rt, 0x08CF2EF8u), std::bit_cast<float>(0x46151B5Eu));
            const float limit_a = rate / count;
            const float base = float_at(rt, state + 0x57Cu);
            float limit = psp_mul(base, float_at(rt, 0x08CF2EFCu));
            const float offset = psp_mul(float_at(rt, 0x08CF2F04u) - count, float_at(rt, 0x08CF2F00u));
            if (limit_a < limit) limit = limit_a;
            const float extra = float_at(rt, state + 0x580u);
            float target = offset + (limit + extra) / (base + extra);
            if (target < 0.0f) target = 0.0f;
            if (current < target) current = target;
            if (1.0f < current) current = 1.0f;
            ctx.fpr[21] = limit;
            ctx.fpr[20] = current;
            ctx.fpr[12] = current;
            put_float(rt, state + 0x534u, current);
            call_set(rt, ctx, 0x089491A0u, {self});
            const float difference = ctx.fpr[0] - limit;
            ctx.fpr[20] = difference;
            call_set(rt, ctx, 0x0895549Cu, {self});
            float scaled = psp_mul(limit, count);
            scaled = psp_mul(scaled, std::bit_cast<float>(0x38DBC304u));
            ctx.fpr[12] = psp_mul(ctx.fpr[0], scaled);
            call_set(rt, ctx, 0x08947C74u, {self});
            ctx.fpr[0] = difference;
        }
    }
    if (tail) jump(ctx, 0x089491A0u);
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08BE6244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); auto &F = ctx.fpr; G[2] = 0xAAAA0000u; G[2] = G[2] | 0xAAABu; G[30] = G[5]; G[17] = G[5]; put(rt, frame.at(0x18u), G[4]); G[3] = word(rt, G[4] + 0x4u); put(rt, frame.at(0x10u), G[6]); G[3] = G[3] - G[6]; G[3] = sra(G[3], 5); { const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32u); } G[2] = G[4] + 0x4u; G[23] = ctx.lo; { const bool tk = static_cast<std::int32_t>(G[23]) <= 0; put(rt, frame.at(0x14u), G[2]); if (tk) goto L_08BE6344; } G[16] = G[6]; G[22] = 0u; G[2] = word(rt, G[16] + 0x4u); L_08BE62B4: ; G[3] = word(rt, G[16] + 0x8u); F[0] = float_at(rt, G[16] + 0xCu); F[1] = float_at(rt, G[16] + 0x10u); F[2] = float_at(rt, G[16] + 0x14u); F[3] = float_at(rt, G[16] + 0x18u); F[4] = float_at(rt, G[16] + 0x1Cu); G[20] = G[17] + 0x20u; G[18] = G[16] + 0x20u; G[21] = G[17] + 0x40u; G[19] = G[16] + 0x40u; put(rt, G[17] + 0x4u, G[2]); G[22] = G[22] + 0x1u; G[4] = G[20]; put(rt, G[17] + 0x8u, G[3]); G[5] = G[18]; G[16] = G[16] + 0x60u; put_float(rt, G[17] + 0xCu, F[0]); put_float(rt, G[17] + 0x10u, F[1]); put_float(rt, G[17] + 0x14u, F[2]); put_float(rt, G[17] + 0x18u, F[3]); put_float(rt, G[17] + 0x1Cu, F[4]); { const bool tk = G[20] == G[18]; G[17] = G[17] + 0x60u; if (tk) goto L_08BE6320; } call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[18] + 0x10u); put(rt, G[20] + 0x10u, G[3]); L_08BE6320: ; G[4] = G[21]; { const bool tk = G[21] == G[19]; G[5] = G[19]; if (tk) goto L_08BE633C; } call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[19] + 0x10u); put(rt, G[21] + 0x10u, G[3]); L_08BE633C: ; if (G[23] != G[22]) { G[2] = word(rt, G[16] + 0x4u); goto L_08BE62B4; } L_08BE6344: ; G[4] = word(rt, frame.at(0x14u)); G[6] = frame.at(0u); G[5] = word(rt, G[4] + 0x0u); G[4] = G[17]; call_here(rt, ctx, 0x08BE59D4u); G[2] = word(rt, frame.at(0x18u)); G[4] = word(rt, frame.at(0x10u)); G[3] = word(rt, G[2] + 0x4u); G[2] = G[4] - G[30]; G[2] = (G[2] & ~0x1Fu) | ((0u << 0) & 0x1Fu); G[3] = G[3] - G[2]; G[2] = word(rt, frame.at(0x18u)); put(rt, G[2] + 0x4u, G[3]); G[2] = G[30]; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[17] = word(rt, frame.at(0x24u)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C13880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); auto &F = ctx.fpr; G[18] = G[6]; G[17] = G[5]; put_float(rt, frame.at(0x30u), F[20]); G[3] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[3] == G[2]; G[16] = G[4]; if (tk) goto L_08C138F8; } if (G[3] == 0u) { G[4] = G[3]; goto L_08C138D0; } F[0] = float_at(rt, G[3] + 0xFFFFFFFCu); put_float(rt, G[3] + 0x0u, F[0]); G[3] = word(rt, G[4] + 0x4u); G[4] = G[3]; L_08C138D0: ; G[6] = G[4] + 0xFFFFFFFCu; G[3] = G[3] + 0x4u; put(rt, G[16] + 0x4u, G[3]); G[6] = G[6] - G[17]; G[6] = (G[6] & ~0x3u) | ((0u << 0) & 0x3u); F[20] = float_at(rt, G[18] + 0x0u); G[4] = G[4] - G[6]; call_here(rt, ctx, 0x08B79BACu); put_float(rt, G[17] + 0x0u, F[20]); goto L_08C139C4; L_08C138F8: ; G[2] = word(rt, G[4] + 0x0u); G[2] = G[3] - G[2]; G[3] = 0x3FFF0000u; G[2] = sra(G[2], 2); G[3] = G[3] | 0xFFFFu; { const bool tk = G[2] != G[3]; G[4] = 0x08CB0000u; if (tk) goto L_08C1391C; } G[4] = 0x08CA91F0u; call_here(rt, ctx, 0x08B5ADDCu); L_08C1391C: ; { const bool tk = G[2] == 0u; G[3] = 0x1u; if (tk) goto L_08C13934; } G[3] = G[2] << 1; G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFCu; if (tk) goto L_08C13938; } L_08C13934: ; G[20] = G[3] << 2; L_08C13938: ; G[4] = G[20]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = word(rt, G[16] + 0x0u); G[5] = G[17]; G[6] = G[2]; G[7] = frame.at(0x2u); G[19] = G[2]; call_here(rt, ctx, 0x08BEF178u); if (G[2] == 0u) { G[5] = word(rt, G[16] + 0x4u); goto L_08C13970; } F[0] = float_at(rt, G[18] + 0x0u); put_float(rt, G[2] + 0x0u, F[0]); G[5] = word(rt, G[16] + 0x4u); L_08C13970: ; G[4] = G[17]; G[6] = G[2] + 0x4u; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08BEF178u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08BEF140u); G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[16] + 0x8u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (tk) goto L_08C139B4; } G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C139B4: ; G[2] = G[19] + G[20]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C139C4: ; F[20] = float_at(rt, frame.at(0x30u)); goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0885F938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = G[5] << 4; G[3] = G[5] << 6; G[2] = G[2] + G[3]; G[22] = G[8] >> 31; G[2] = G[2] + 0x4120u; if (G[22] != 0u) G[8] = 0u; G[30] = G[10] & 0xFFu; G[23] = 0xFFFFFFFFu; G[21] = G[9] & 0xFFu; G[20] = 0u; G[19] = G[5]; G[18] = G[4] + G[2]; G[17] = G[4]; G[16] = G[6]; put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x4u), G[7]); put(rt, frame.at(0x0u), G[8]); L_0885F9A4: ; G[4] = word(rt, G[17] + 0x5ACu); call_here(rt, ctx, 0x088D698Cu); G[5] = G[2]; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[16]; if (tk) goto L_0885F9C0; } call_here(rt, ctx, 0x08897130u); L_0885F9C0: ; G[4] = G[16]; call_here(rt, ctx, 0x088977D4u); G[4] = G[16]; { const bool tk = G[2] == G[23]; G[5] = G[21]; if (tk) goto L_0885FA54; } call_here(rt, ctx, 0x0889715Cu); G[4] = G[16]; G[5] = G[22]; call_here(rt, ctx, 0x088975B4u); G[4] = word(rt, G[17] + 0x5ACu); G[4] = G[4] + 0x4u; call_here(rt, ctx, 0x088D4D9Cu); G[3] = 0x1u; if (G[2] != G[3]) { G[9] = sext(load8(G[18] + 0x4u), 8); goto L_0885FA1C; } G[2] = 0x2u; G[4] = G[16]; { const bool tk = G[19] != G[2]; G[5] = 0x1u; if (tk) goto L_0885FA18; } call_here(rt, ctx, 0x088975B4u); L_0885FA18: ; G[9] = sext(load8(G[18] + 0x4u), 8); L_0885FA1C: ; G[7] = G[16]; G[8] = G[20]; G[3] = G[9] << 6; G[2] = G[9] << 4; G[2] = G[2] + G[3]; G[4] = G[17]; G[5] = G[19]; G[2] = G[2] + G[17]; G[6] = word(rt, frame.at(0x4u)); { const bool tk = G[9] == G[23]; G[3] = G[30]; if (tk) goto L_0885FA4C; } G[3] = load8(G[2] + 0x4138u); L_0885FA4C: ; G[9] = G[3]; call_here(rt, ctx, 0x0885DF24u); L_0885FA54: ; G[20] = G[20] + 0x1u; G[3] = 0x2u; G[18] = G[18] + 0x1u; { const bool tk = G[20] != G[3]; G[16] = G[16] + 0x30u; if (tk) goto L_0885F9A4; } G[4] = word(rt, G[17] + 0x5B4u); G[6] = word(rt, frame.at(0x0u)); G[5] = G[19]; G[31] = word(rt, frame.at(0x34u)); tail = 0x088F8A84u; goto exit_;  exit_: ; G[22] = word(rt, frame.at(0x28u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088622CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[23] = 0x384u; G[19] = G[4]; L_08862300: ; call_here(rt, ctx, 0x08B1A844u); { const bool tk = G[2] != 0u; G[2] = 0x4u; if (tk) goto L_088623D8; } G[2] = word(rt, G[19] + 0x0u); G[4] = word(rt, G[2] + 0x5ACu); G[4] = G[4] + 0x4u; call_here(rt, ctx, 0x088D4C9Cu); { const bool tk = G[2] == 0u; G[5] = word(rt, G[19] + 0x0u); if (tk) goto L_088623F4; } G[2] = word(rt, G[5] + 0x5ACu); G[3] = load8(G[2] + 0x5BCu); if (G[3] != 0u) { G[3] = word(rt, G[5] + 0x3F50u); goto L_088623F8; } G[2] = 0x4u; goto L_088623D8; L_08862340: ; G[22] = 0u; G[17] = 0u; G[20] = 0x1u; G[21] = 0x3u; G[30] = 0xFFFFFFFFu; L_08862354: ; G[2] = word(rt, G[3] + 0x0u); G[16] = G[2] + G[17]; G[2] = word(rt, G[16] + 0x4Cu); if (G[2] == G[20]) { G[2] = word(rt, G[3] + 0x10u); goto L_08862378; } { const bool tk = G[2] != G[21]; G[22] = 0x1u; if (tk) goto L_088623A4; } G[2] = 0u; goto L_08862408; L_08862378: ; { const bool tk = G[2] != G[30]; G[4] = G[16]; if (tk) goto L_088623A4; } call_here(rt, ctx, 0x0885D4CCu); if (G[2] == 0u) { G[5] = word(rt, G[19] + 0x0u); goto L_088623A4; } G[2] = word(rt, G[19] + 0x0u); G[4] = word(rt, G[16] + 0x1Cu); G[3] = word(rt, G[2] + 0x3F50u); put(rt, G[3] + 0x10u, G[4]); G[5] = word(rt, G[19] + 0x0u); L_088623A4: ; G[3] = word(rt, G[5] + 0x3F50u); G[18] = G[18] + 0x1u; G[2] = word(rt, G[3] + 0x4u); G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = G[17] + 0x188u; if (tk) goto L_08862354; } { const bool tk = G[22] == 0u; G[4] = 0x00010000u; if (tk) goto L_088623EC; } G[23] = G[23] + 0xFFFFFFFFu; G[4] = G[4] | 0x86A0u; call_here(rt, ctx, 0x08B2E2B8u); { const bool tk = G[23] != 0u; G[2] = 0x4u; if (tk) goto L_08862300; } L_088623D8: ; G[2] = G[2] + 0xFFFFFFFFu; L_088623DC: ; if (G[2] != 0u) { G[2] = G[2] + 0xFFFFFFFFu; goto L_088623DC; } goto L_0886240C; L_088623EC: ; G[2] = 0x1u; goto L_08862408; L_088623F4: ; G[3] = word(rt, G[5] + 0x3F50u); L_088623F8: ; G[2] = word(rt, G[3] + 0x4u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[18] = 0u; if (tk) goto L_08862340; } G[2] = 0x1u; L_08862408: ; L_0886240C: ; goto exit_;  exit_: ; G[23] = word(rt, frame.at(0x1Cu)); G[19] = word(rt, frame.at(0xCu)); G[30] = word(rt, frame.at(0x20u)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08875DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); G[18] = 0x1u; G[17] = G[4]; G[16] = G[5]; G[2] = word(rt, G[5] + 0x0u); G[4] = word(rt, G[4] + 0x0u); G[5] = word(rt, G[2] + 0x30u); call_here(rt, ctx, 0x0886B30Cu); goto L_08875EF4; L_08875DE8: ; G[5] = word(rt, G[3] + 0x28u); G[2] = word(rt, G[3] + 0x2Cu); { const bool tk = G[5] != G[2]; if (tk) goto L_08875E94; } G[2] = load8(G[3] + 0x25u); G[2] = G[2] << 3; G[2] = G[2] + 0x10u; G[2] = G[3] + G[2]; G[16] = word(rt, G[2] + 0x8u); put(rt, G[2] + 0x4u, 0u); { const bool tk = G[16] == 0u; put(rt, G[2] + 0x8u, 0u); if (tk) goto L_08875E78; } G[3] = G[16] + 0x4u; L_08875E1C: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_08875E1C; } { const bool tk = G[4] != G[18]; if (tk) goto L_08875E78; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_08875E4C: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_08875E4C; } { const bool tk = G[4] != G[18]; if (tk) goto L_08875E78; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_08875E78: ; G[4] = word(rt, G[17] + 0x0u); call_here(rt, ctx, 0x0886BB1Cu); G[2] = word(rt, G[17] + 0x82Cu); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[2] = G[2] + 0xFFFFFFFFu; if (tk) goto L_08875F04; } put(rt, G[17] + 0x82Cu, G[2]); goto L_08875F04; L_08875E94: ; G[4] = G[17]; call_here(rt, ctx, 0x08875AD8u); G[4] = word(rt, G[16] + 0x0u); G[5] = 0x1u; G[2] = word(rt, G[4] + 0x28u); L_08875EA8: ; G[2] = G[2] + 0x1u; { const bool tk = G[5] == 0u; put(rt, G[4] + 0x28u, G[2]); if (tk) goto L_08875ED0; } G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[4] + 0x28u); G[3] = sext(load8(G[2] + 0x0u), 8); { const bool tk = G[3] != 0u; G[5] = 0x1u; if (tk) goto L_08875EE4; } G[5] = 0u; goto L_08875EE4; L_08875ED0: ; G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[4] + 0x28u); G[3] = sext(load8(G[2] + 0x0u), 8); { const bool tk = G[3] != 0u; G[5] = 0u; if (tk) goto L_08875EF4; } L_08875EE4: ; G[3] = word(rt, G[4] + 0x28u); G[2] = word(rt, G[4] + 0x2Cu); if (G[3] != G[2]) { G[2] = word(rt, G[4] + 0x28u); goto L_08875EA8; } L_08875EF4: ; G[4] = word(rt, G[17] + 0x0u); call_here(rt, ctx, 0x0886BAECu); if (G[2] == 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08875DE8; } L_08875F04: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088DFDA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x4Cu), G[31]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); G[5] = G[5] & 0xFFu; G[18] = G[4]; { const bool tk = G[5] == 0u; if (tk) goto L_088DFDE8; } G[17] = word(rt, G[4] + 0x138u); G[16] = word(rt, G[4] + 0x640u); call_here(rt, ctx, 0x088D9F80u); G[6] = G[2]; G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x088E5148u); G[4] = word(rt, G[18] + 0x640u); G[5] = word(rt, G[18] + 0x138u); call_here(rt, ctx, 0x088E4B80u); L_088DFDE8: ; G[2] = load8(G[18] + 0x27u); { const bool tk = G[2] == 0u; G[16] = 0u; if (tk) goto L_088DFE3C; } G[16] = word(rt, G[18] + 0x618u); G[4] = G[18]; G[5] = word(rt, G[16] + 0x22Cu); G[17] = G[16] + 0x22Cu; call_here(rt, ctx, 0x088DC858u); if (G[2] != 0u) { G[16] = 0u; goto L_088DFE3C; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x088D2674u); G[5] = word(rt, G[16] + 0x22Cu); G[8] = word(rt, G[17] + 0x8u); G[4] = word(rt, G[18] + 0x640u); G[6] = word(rt, G[17] + 0x4u); G[7] = 0u; G[9] = frame.at(0u); call_here(rt, ctx, 0x088E4C28u); G[16] = 0u; L_088DFE3C: ; G[17] = 0u; goto L_088DFE78; L_088DFE44: ; G[2] = word(rt, G[18] + 0xC0u); G[2] = G[2] + G[17]; G[3] = word(rt, G[2] + 0xCu); { const bool tk = G[3] == 0u; if (tk) goto L_088DFEFC; } G[2] = load8(G[2] + 0x10u); { const bool tk = G[2] == 0u; if (tk) goto L_088DFEFC; } G[4] = word(rt, G[18] + 0x640u); G[5] = G[16]; call_here(rt, ctx, 0x088E6864u); G[16] = G[16] + 0x1u; L_088DFE74: ; G[17] = G[17] + 0x2F8u; L_088DFE78: ; G[4] = G[18]; call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[5] = G[16]; if (tk) goto L_088DFE44; } G[4] = word(rt, G[18] + 0x14u); call_here(rt, ctx, 0x088FE2A4u); { const bool tk = G[2] == 0u; if (tk) goto L_088DFEAC; } G[4] = word(rt, G[18] + 0x640u); call_here(rt, ctx, 0x088E6B0Cu); G[16] = word(rt, G[18] + 0x640u); goto L_088DFEB8; L_088DFEAC: ; G[4] = word(rt, G[18] + 0x640u); call_here(rt, ctx, 0x088E6B84u); G[16] = word(rt, G[18] + 0x640u); L_088DFEB8: ; G[4] = G[18]; call_here(rt, ctx, 0x088D9F80u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x088E6A14u); G[16] = word(rt, G[18] + 0x640u); G[4] = G[18]; call_here(rt, ctx, 0x088DA160u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x088E698Cu); goto exit_; L_088DFEFC: ; G[4] = word(rt, G[18] + 0x640u); call_here(rt, ctx, 0x088E68F8u); G[16] = G[16] + 0x1u; goto L_088DFE74;  exit_: ; G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08A98E14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shift_string_elements(rt, ctx); }
void func_08A99CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_matching_index(rt, ctx); }
// A guest call with integer arguments (a0-a3, t0-t3) and float ones ($f12, $f13, ...), and a float result in $f0.
static float call_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    reg = 12u;
    for (const float value : floats) ctx.fpr[reg++] = value;
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// 0x08B2A680 (a0 = the object (two byte-swapped words), a1 = the end of a buffer, a2 = a key, a3 = a flag (low byte), f12 = a scale, f13 = a second scale; v0 = a1 - 8 when the checksum matches,
// else -1): two passes of 0x08B2A370(object, a1, object, 4 / f12 = f13), then (flag set) 0x08B1EDE0(frame + 4, key ^ bswap(word(object))) and 0x08B2A634(object + 4, a1 - 4, frame + 4, 0, 0);
// 0x08B2A370(object + 4, a1 - 4, object + 4, 4 / f12 = f12). The words at object + 0 / + 4 (swapped) then feed 0x08B1EDE0(frame + 0x9CC, bswap(w4) + bswap(w0)) and, with the pair
// (bswap(w4) ^ bswap(w0), w0) stored at frame + 0 / +4, 0x08C4FE98(object + 8, a1 - 8, frame + 0x9CC, frame). The word at object + 4 becomes bswap(key ^ bswap(w4)) and
// 0x08B29E88(0, object + 8, a1 - 8) is compared (xor) with bswap(word(object + 4)): equal gives a1 - 8.
void func_08B2A680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 limit = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    const bool flag = (ctx.gpr[7] & 0xFFu) != 0u;
    const float scale = ctx.fpr[12];
    const float second_scale = ctx.fpr[13];
    GuestFrame frame(ctx, 0x13E0u);
    SavedRegisters saved(rt, ctx, frame, {{52u, 0x13D0u}, {30u, 0x13C0u}, {23u, 0x13BCu}, {18u, 0x13A8u}, {17u, 0x13A4u}, {16u, 0x13A0u}, {22u, 0x13B8u}, {21u, 0x13B4u}, {20u, 0x13B0u}, {19u, 0x13ACu}, {31u, 0x13C4u}});
    const u32 buffer = frame.at(4u);
    const u32 end_minus_8 = limit - 8u;
    const u32 end_minus_4 = limit - 4u;
    const u32 second_word = object + 4u;
    const u32 third_word = object + 8u;
    ctx.fpr[20] = scale;
    ctx.gpr[30] = key;
    ctx.gpr[23] = buffer;
    ctx.gpr[18] = object;
    ctx.gpr[17] = limit;
    ctx.gpr[16] = flag ? 1u : 0u;
    ctx.gpr[22] = third_word;
    ctx.gpr[21] = end_minus_8;
    ctx.gpr[20] = end_minus_4;
    ctx.gpr[19] = second_word;
    call_float(rt, ctx, 0x08B2A370u, {object, limit, object, 4u}, {second_scale});
    ctx.gpr[4] = buffer;
    if (flag) {
        call_set(rt, ctx, 0x08B1EDE0u, {buffer, key ^ __builtin_bswap32(word(rt, object))});
        call_set(rt, ctx, 0x08B2A634u, {second_word, end_minus_4, buffer, 0u, 0u});
    }
    call_float(rt, ctx, 0x08B2A370u, {second_word, end_minus_4, second_word, 4u}, {scale});
    const u32 first = word(rt, object);
    const u32 second = word(rt, object + 4u);
    put(rt, buffer, first);
    call_set(rt, ctx, 0x08B1EDE0u, {frame.at(0x9CCu), __builtin_bswap32(second) + __builtin_bswap32(first)});
    const u32 repeat_first = word(rt, object);
    const u32 repeat_second = word(rt, object + 4u);
    put(rt, frame.at(0u), __builtin_bswap32(repeat_second) ^ __builtin_bswap32(repeat_first));
    put(rt, frame.at(4u), repeat_first);
    call_set(rt, ctx, 0x08C4FE98u, {third_word, end_minus_8, frame.at(0x9CCu), frame.at(0u)});
    const u32 updated = __builtin_bswap32(key ^ __builtin_bswap32(word(rt, object + 4u)));
    put(rt, object + 4u, updated);
    const u32 answer = call_set(rt, ctx, 0x08B29E88u, {0u, third_word, end_minus_8});
    ctx.gpr[2] = (answer ^ __builtin_bswap32(word(rt, object + 4u))) == 0u ? end_minus_8 : 0xFFFFFFFFu;
}
void func_08B62EA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { query_flagged_entries(rt, ctx); }
void func_08C5592C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { check_stream_block(rt, ctx, 8u); }
void func_08C705A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { check_stream_block(rt, ctx, 0x10u); }
void func_08861538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[23] = G[4]; G[22] = G[5]; G[2] = load8(G[6] + 0x1C34u); { const bool tk = G[2] == 0u; G[19] = G[6]; if (tk) goto L_0886167C; } G[3] = word(rt, G[6] + 0xD70u); G[2] = 0x1u; { const bool tk = static_cast<std::int32_t>(G[3]) <= 0; store8(G[5] + 0x1C34u, G[2]); if (tk) goto L_088615C4; } G[16] = G[6]; G[17] = 0u; L_0886158C: ; G[3] = word(rt, G[16] + 0xD20u); G[4] = G[22]; G[17] = G[17] + 0x1u; G[2] = G[3] << 5; G[3] = G[3] << 3; G[2] = G[2] - G[3]; G[5] = G[2] << 3; G[5] = G[5] - G[2]; G[5] = G[19] + G[5]; call_here(rt, ctx, 0x0888AF70u); G[3] = word(rt, G[19] + 0xD70u); G[3] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[3] != 0u; G[16] = G[16] + 0x4u; if (tk) goto L_0886158C; } L_088615C4: ; G[20] = 0u; G[21] = 0x1560u; G[17] = G[19] + 0x12B8u; G[16] = G[19] + 0x1018u; G[18] = G[22] + 0x1018u; goto L_08861668; L_088615DC: ; { const bool tk = G[2] == 0u; G[4] = G[22]; if (tk) goto L_08861600; } G[2] = load8(G[18] + 0x11u); { const bool tk = G[2] != 0u; G[5] = G[20]; if (tk) goto L_08861604; } G[4] = G[18]; G[5] = G[16]; call_here(rt, ctx, 0x0888A878u); G[4] = G[22]; L_08861600: ; G[5] = G[20]; L_08861604: ; G[6] = G[17]; call_here(rt, ctx, 0x0888AD94u); G[4] = G[22] + G[21]; G[5] = G[19] + G[21]; G[6] = load16(G[5] + 0x0u); G[2] = load16(G[4] + 0x0u); G[3] = load16(G[4] + 0x2u); G[7] = load16(G[5] + 0x2u); G[2] = G[2] + G[6]; store16(G[4] + 0x0u, G[2]); G[3] = G[3] + G[7]; G[6] = 0x64u; store16(G[4] + 0x2u, G[3]); G[3] = 0u; L_0886163C: ; G[2] = word(rt, G[5] + 0x1Cu); G[3] = G[3] + 0x1u; G[5] = G[5] + 0x4u; put(rt, G[4] + 0x1Cu, G[2]); { const bool tk = G[3] != G[6]; G[4] = G[4] + 0x4u; if (tk) goto L_0886163C; } G[20] = G[20] + 0x1u; G[18] = G[18] + 0xA8u; G[16] = G[16] + 0xA8u; G[17] = G[17] + 0xA8u; G[21] = G[21] + 0x1B0u; L_08861668: ; G[4] = word(rt, G[23] + 0x5ACu); call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[20]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[2] = load8(G[16] + 0x11u); goto L_088615DC; } L_0886167C: ; goto exit_;  exit_: ; G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0xCu)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088779F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x30u), G[16]); auto &F = ctx.fpr; G[17] = G[5]; G[5] = 0u; call_here(rt, ctx, 0x08875878u); { const bool tk = G[2] == 0u; G[18] = G[2]; if (tk) goto L_08877B50; } G[16] = frame.at(0x14u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[17] = G[2]; put(rt, frame.at(0x8u), 0u); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); goto L_08877A70; L_08877A4C: ; if (G[5] == 0u) { G[5] = G[5] + 0x4u; goto L_08877A60; } put_float(rt, G[5] + 0x0u, F[0]); G[5] = word(rt, frame.at(0xCu)); G[5] = G[5] + 0x4u; L_08877A60: ; put(rt, frame.at(0xCu), G[5]); goto L_08877A70; L_08877A68: ; G[6] = frame.at(0x4u); call_here(rt, ctx, 0x08B9A5C4u); L_08877A70: ; G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); { const bool tk = G[2] == 0u; G[4] = G[2]; if (tk) goto L_08877B38; } G[2] = sext(load8(G[2] + 0x0u), 8); { const bool tk = G[2] == 0u; G[2] = word(rt, frame.at(0x1Cu)); if (tk) goto L_08877B3C; } call_here(rt, ctx, 0x08B77BF4u); G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B71EC8u); G[5] = word(rt, frame.at(0xCu)); G[2] = word(rt, frame.at(0x10u)); { const bool tk = G[5] != G[2]; put_float(rt, frame.at(0x4u), F[0]); if (tk) goto L_08877A4C; } G[4] = frame.at(0x8u); goto L_08877A68; L_08877AB8: ; G[5] = word(rt, frame.at(0xCu)); G[4] = G[18]; G[6] = G[2]; G[5] = G[5] - G[2]; G[5] = sra(G[5], 2); G[7] = G[17]; call_here(rt, ctx, 0x0890AF90u); G[4] = word(rt, frame.at(0x8u)); L_08877AD8: ; G[5] = word(rt, frame.at(0xCu)); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08B9A574u); G[4] = word(rt, frame.at(0x8u)); if (G[4] == 0u) { G[4] = word(rt, frame.at(0x14u)); goto L_08877AFC; } call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, frame.at(0x14u)); L_08877AFC: ; G[2] = 0x08D70000u; G[2] = 0x08D75298u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; if (tk) goto L_08877B54; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { goto L_08877B54; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); goto L_08877B54; L_08877B38: ; G[2] = word(rt, frame.at(0x1Cu)); L_08877B3C: ; G[2] = static_cast<std::int32_t>(G[2]) < 2 ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = word(rt, frame.at(0x8u)); if (tk) goto L_08877AB8; } G[4] = word(rt, frame.at(0x8u)); goto L_08877AD8; L_08877B50: ; L_08877B54: ; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x30u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08947CF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ease_towards_target(rt, ctx); }
void func_089AD88C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { format_four_floats(rt, ctx); }
void func_08A9A1A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { join_handle_texts(rt, ctx); }
void func_08B2D61C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08D70000u; G[3] = 0x08D70000u; G[19] = 0x08D70000u; G[18] = 0x08D70000u; G[17] = G[4]; G[16] = 0x08D70000u; G[16] = 0x08D68D00u; put(rt, 0x08D68D54u, G[5]); G[2] = 0x08D70000u; G[4] = G[16]; put(rt, 0x08D68D50u, G[6]); G[2] = 0x08D70000u; G[6] = 0x40u; G[5] = 0u; put(rt, 0x08D68D58u, 0u); G[2] = 0x08D70000u; put(rt, 0x08D68D40u, 0u); G[3] = 0x08D70000u; store8(0x08D68D4Cu, 0u); put(rt, 0x08D68CF8u, 0u); put(rt, 0x08D68D44u, 0u); put(rt, 0x08D68D48u, 0u); call_here(rt, ctx, 0x08B36018u); G[4] = G[16]; G[5] = G[17]; G[6] = 0x4u; call_here(rt, ctx, 0x08B7C830u); G[5] = 0x08B30000u; G[4] = 0x08CB0000u; G[4] = 0x08CB5438u; G[5] = 0x08B2D314u; G[7] = 0x1000u; G[8] = 0x00200000u; G[9] = 0u; G[6] = 0x20u; call_here(rt, ctx, 0x08C7AA18u); put(rt, 0x08D68D48u, G[2]); G[4] = 0x14u; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[3] = 0x1u; if (tk) goto L_08B2D76C; } call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; G[5] = 0x2u; G[6] = 0x200u; call_here(rt, ctx, 0x08B2EE10u); G[4] = G[16]; put(rt, 0x08D68D44u, G[16]); call_here(rt, ctx, 0x08B2EEB8u); call_here(rt, ctx, 0x08B2D4E0u); G[5] = 0x03410000u; G[4] = 0x08CB0000u; G[5] = G[5] | 0x5001u; G[4] = 0x08CB542Cu; G[6] = 0x08D68D48u; G[7] = 0x4u; G[8] = 0u; { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[9] = 0u; if (tk) goto L_08B2D740; } G[4] = word(rt, 0x08D68D44u); call_here(rt, ctx, 0x08B2EE2Cu); G[4] = word(rt, 0x08D68D48u); call_here(rt, ctx, 0x08C7AA00u); G[3] = 0x3u; goto L_08B2D76C; L_08B2D740: ; call_here(rt, ctx, 0x08C7A820u); { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[3] = 0u; if (tk) goto L_08B2D76C; } call_here(rt, ctx, 0x08B2D45Cu); G[4] = word(rt, 0x08D68D44u); call_here(rt, ctx, 0x08B2EE2Cu); G[4] = word(rt, 0x08D68D48u); call_here(rt, ctx, 0x08C7AA00u); G[3] = 0x2u; L_08B2D76C: ; G[2] = G[3]; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B308FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { run_entries_for_pair(rt, ctx); }
void func_08B413F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_midpoint_value(rt, ctx); }
// 0x08B44528 (a0 = the object, a1 = an id (low halfword, sign-extended); v0 = a signed halfword or -1): under the lock 0x08B344E0(object + 0x12C), the table at object + 0x454 (count
// at +0x480; flag bytes at +0x400, ids at +0x200 words, entry pointers at +0 words) is searched for the first flagged slot whose id word equals the sign-extended id; its entry pointer and the record
// it holds must be non-zero (else -1). The record's signed halfword at +0x16 is compared with the object's virtual +0x90: when it differs and is 0, the virtual +0xA0 decides: non-zero stores the
// virtual +0x90 answer (sign-extended) in the halfword and calls 0x08B43554(object, id, that), zero reports 0x08B429D0(object, 7, frame, 0xC, 1) with a block (words 0, 0, halfword id) on the
// stack. The result is the halfword read at the end, sign-extended; the lock is released with 0x08B3457C(object + 0x12C) on every path.
void func_08B44528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 id = sext(ctx.gpr[5], 16u);
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {17u, 0x14u}, {18u, 0x18u}, {31u, 0x24u}, {20u, 0x20u}, {16u, 0x10u}});
    const u32 lock = object + 0x12Cu;
    ctx.gpr[19] = lock;
    ctx.gpr[17] = object;
    ctx.gpr[18] = id;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    const auto finish = [&](u32 value) {
        ctx.gpr[16] = value;
        call_set(rt, ctx, 0x08B3457Cu, {lock});
        ctx.gpr[2] = value;
    };
    const u32 table = object + 0x454u;
    const u32 count = word(rt, table + 0x480u);
    u32 slot = 0u;
    bool found = false;
    for (; slot < count; ++slot) {
        if (load8(table + 0x400u + slot) != 0u && word(rt, table + 0x200u + slot * 4u) == id) {
            found = true;
            break;
        }
    }
    if (!found) {
        finish(0xFFFFFFFFu);
        return;
    }
    const u32 entry = word(rt, table + slot * 4u);
    if (entry == 0u) {
        finish(0xFFFFFFFFu);
        return;
    }
    const u32 record = word(rt, entry);
    ctx.gpr[20] = record;
    if (record == 0u) {
        finish(0xFFFFFFFFu);
        return;
    }
    const u32 vtable = word(rt, object);
    ctx.gpr[3] = vtable;
    const u32 stored = signed_half(rt, record + 0x16u);
    ctx.gpr[16] = stored;
    const u32 current = call_indirect(rt, ctx, word(rt, vtable + 0x90u), {object});
    if (stored != current && load16(record + 0x16u) == 0u) {
        if (call_indirect(rt, ctx, word(rt, word(rt, object) + 0xA0u), {object}) == 0u) {
            put(rt, frame.at(8u), 0u);
            put(rt, frame.at(0u), 0u);
            store16(frame.at(8u), id);
            put(rt, frame.at(4u), 0u);
            call_set(rt, ctx, 0x08B429D0u, {object, 7u, frame.at(0u), 0xCu, 1u});
        } else {
            const u32 value = sext(call_indirect(rt, ctx, word(rt, word(rt, object) + 0x90u), {object}), 16u);
            store16(record + 0x16u, value);
            call_set(rt, ctx, 0x08B43554u, {object, id, value});
        }
    }
    finish(sext(load16(record + 0x16u), 16u));
}
void func_08C42530(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_element_by_string(rt, ctx); }
void func_08899BE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_up_screen_state(rt, ctx); }
void func_088D0B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); auto &F = ctx.fpr; G[18] = G[6]; G[6] = G[5]; G[5] = G[18]; G[16] = G[4]; put_float(rt, frame.at(0x20u), F[20]); F[12] = float_at(rt, G[4] + 0x0u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088FAC80u); F[1] = float_at(rt, frame.at(0x4u)); F[0] = std::bit_cast<float>(0u); ctx.set_fpu_condition(F[1] < F[0]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_088D0C68; } put_float(rt, G[16] + 0x0u, F[1]); G[4] = G[18]; call_here(rt, ctx, 0x088FB000u); F[12] = F[0]; F[13] = float_at(rt, G[16] + 0x4u); F[14] = float_at(rt, G[16] + 0x0u); G[4] = G[18]; call_here(rt, ctx, 0x088FB144u); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[17] = G[2]; if (tk) goto L_088D0BB0; } G[4] = G[18]; F[20] = float_at(rt, G[16] + 0x4u); call_here(rt, ctx, 0x088FB028u); G[1] = 0x41200000u; F[1] = std::bit_cast<float>(G[1]); F[0] = F[0] - F[1]; ctx.set_fpu_condition(F[0] <= F[20]); if (ctx.fpu_condition()) { G[2] = word(rt, G[16] + 0x18u); goto L_088D0BBC; } F[12] = float_at(rt, G[16] + 0x0u); goto L_088D0C14; L_088D0BB0: ; if (G[2] == 0u) { F[12] = float_at(rt, G[16] + 0x0u); goto L_088D0C14; } G[2] = word(rt, G[16] + 0x18u); L_088D0BBC: ; G[4] = G[18]; G[2] = G[17] + G[2]; put(rt, G[16] + 0x18u, G[2]); call_here(rt, ctx, 0x088FB000u); F[12] = F[0]; F[13] = float_at(rt, G[16] + 0x4u); F[14] = float_at(rt, G[16] + 0x0u); G[4] = G[18]; call_here(rt, ctx, 0x088FB17Cu); G[3] = word(rt, G[16] + 0x18u); G[2] = word(rt, G[16] + 0x14u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] == 0u; put_float(rt, G[16] + 0x28u, F[0]); if (tk) goto L_088D0C10; } G[2] = load8(G[16] + 0x27u); if (G[2] != 0u) { put(rt, G[16] + 0xCu, 0u); goto L_088D0C10; } G[2] = 0x1u; put(rt, G[16] + 0x14u, G[3]); store8(G[16] + 0x24u, G[2]); put(rt, G[16] + 0xCu, 0u); L_088D0C10: ; F[12] = float_at(rt, G[16] + 0x0u); L_088D0C14: ; F[0] = float_at(rt, G[16] + 0xCu); ctx.set_fpu_condition(F[12] <= F[0]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_088D0C68; } F[13] = float_at(rt, G[16] + 0x4u); G[4] = G[18]; G[5] = 0u; call_here(rt, ctx, 0x088FB100u); F[1] = std::bit_cast<float>(0u); ctx.set_fpu_condition(F[0] <= F[1]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_088D0C68; } G[3] = word(rt, G[16] + 0x14u); G[2] = word(rt, G[16] + 0x18u); { const bool tk = G[3] != G[2]; if (tk) goto L_088D0C6C; } F[0] = float_at(rt, G[16] + 0x0u); put_float(rt, G[16] + 0xCu, F[0]); L_088D0C68: ; L_088D0C6C: ; F[20] = float_at(rt, frame.at(0x20u)); goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0897AD70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_text_from_handle(rt, ctx); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_325
} // namespace gt::game

#define HOT325(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_325::name, gt::ReturnKind::kind)
// HOT325(0x08BE6244u, func_08BE6244, Word);  // no comparable synthetic case yet
HOT325(0x08C13880u, func_08C13880, Word);
// HOT325(0x0885F938u, func_0885F938, Word);  // no comparable synthetic case yet
// HOT325(0x088622CCu, func_088622CC, Word);  // no comparable synthetic case yet
HOT325(0x08875DB0u, func_08875DB0, Word);
// HOT325(0x088DFDA0u, func_088DFDA0, Word);  // no comparable synthetic case yet
HOT325(0x08A98E14u, func_08A98E14, Word);
HOT325(0x08A99CB0u, func_08A99CB0, Word);
HOT325(0x08B2A680u, func_08B2A680, Word);
HOT325(0x08B62EA0u, func_08B62EA0, Word);
HOT325(0x08C5592Cu, func_08C5592C, Word);
HOT325(0x08C705A4u, func_08C705A4, Word);
HOT325(0x08861538u, func_08861538, Word);
HOT325(0x088779F8u, func_088779F8, Word);
HOT325(0x08947CF0u, func_08947CF0, Word);
HOT325(0x089AD88Cu, func_089AD88C, Word);
HOT325(0x08A9A1A0u, func_08A9A1A0, Word);
// HOT325(0x08B2D61Cu, func_08B2D61C, Word);  // no comparable synthetic case yet
HOT325(0x08B308FCu, func_08B308FC, Word);
HOT325(0x08B413F0u, func_08B413F0, Word);
HOT325(0x08B44528u, func_08B44528, Word);
HOT325(0x08C42530u, func_08C42530, Word);
HOT325(0x08899BE4u, func_08899BE4, Word);
// HOT325(0x088D0B0Cu, func_088D0B0C, Word);  // no comparable synthetic case yet
HOT325(0x0897AD70u, func_0897AD70, Word);
