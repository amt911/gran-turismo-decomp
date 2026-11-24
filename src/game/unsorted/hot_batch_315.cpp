// Hot batch 315: functions no route reaches (straight-line code with forward branches), drafted from
// `tools.disasm` with a register-level emitter (pipeline/bodyx.py), read against the asm and verified by
// synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_315 {
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

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

// 0x08A4C2AC (a0 = the destination string, a1 = a count, a2 = the source object; v0 untouched when the count is <= 0):
// asks the object's virtuals +0xDC (the text) and +0xD8 (its length), formats it on the frame with 0x08C1C8F0, moves the
// result into the destination (0x08A6D2D4, then 0x08A5BAD8 unless the destination is the frame slot) and frees the buffer.
// A formatting failure leaves the destination as "!".
void func_08A4C2AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {17u, 0x14u}, {31u, 0x20u}, {18u, 0x18u}, {16u, 0x10u}});
    if (count <= 0) return;  // v0 stays as the caller left it
    u32 object = word(rt, source);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    const u32 text = call_set(rt, ctx, word(rt, word(rt, object) + 0xDCu), {object, 0u});
    object = word(rt, source);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    const u32 length = call_set(rt, ctx, word(rt, word(rt, object) + 0xD8u), {object});
    // one and a half times the length, never below 0x80
    const u32 scaled = ((length << 1) + length) >> 1;
    const u32 buffer = call_set(rt, ctx, 0x08B2B5E0u, {scaled < 0x80u ? 0x80u : scaled});
    const u32 written = call_set(rt, ctx, 0x08C1C8F0u, {frame.at(0x2u), buffer, text, length});
    if (static_cast<s32>(written) >= 0) {
        store8(buffer + written, 0u);
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x4u), buffer, frame.at(0x1u)});
        call_set(rt, ctx, 0x08A6D2D4u, {frame.at(0x8u), frame.at(0x4u)});
        if (destination != frame.at(0x8u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0x8u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
        const u32 data = word(rt, frame.at(0x4u));
        if (data - 0xCu != 0x08D74FD0u) {  // not the shared empty header
            if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) <= 0)
                call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, frame.at(0u)});
        }
        store8(buffer + 0x0u, 0x21u);
        store8(buffer + 0x1u, 0u);
    }
    call_set(rt, ctx, 0x08B35254u, {buffer});
}
void func_08821ADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); G[22] = frame.at(0xCu); G[17] = G[5]; G[5] = G[4]; G[4] = G[22]; call_here(rt, ctx, 0x08A6C410u); G[16] = word(rt, frame.at(0xCu)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C99698u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[5] = G[16]; G[4] = frame.at(0x10u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[21] = frame.at(0x4u); call_here(rt, ctx, tg); } G[5] = G[17]; G[4] = G[21]; call_here(rt, ctx, 0x08A6C410u); G[16] = word(rt, frame.at(0x4u)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C99698u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = frame.at(0x8u); call_here(rt, ctx, tg); } G[4] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[2] = frame.at(0x14u); G[16] = word(rt, G[4] + 0xFFFFFFF4u); G[7] = frame.at(0x18u); G[5] = G[18]; put(rt, frame.at(0x14u), G[16]); G[19] = G[18] + 0xFFFFFFF4u; G[17] = word(rt, G[18] + 0xFFFFFFF4u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[7] = G[2]; put(rt, frame.at(0x18u), G[17]); G[16] = G[16] - G[17]; G[6] = word(rt, G[7] + 0x0u); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; G[2] = 0x08D70000u; G[20] = 0x08D74FD0u; { const bool tk = G[19] == G[20]; G[16] = G[16] >> 31; if (!tk) { G[4] = G[18] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[19]; if (!tk) { G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x10u)); G[17] = G[4] + 0xFFFFFFF4u; { const bool tk = G[20] == G[17]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[17]; if (!tk) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[16];  G[22] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); }  } }
// 0x08876188 (a0 = the object (word +0 = a target), a2 = a byte, a3 = a byte; v0 = the last call's): 0x08AB85F0 sets up a vector on the frame (+4; its count word is at +0xC) and 0x08AB856C reads it five times,
// the results kept in order (s7 first, then s6; third, 0x088755B4(object, third) and 0x088755B4's result s5; fifth, 0x08875584(object, fifth) and its result s4; a last read s3). Each call's delay slot moves the previous result,
// so s7 = the first read, s6 = the second, s5 = the result of 0x088755B4, s4 = the result of 0x08875584 and s3 = the last read. With the count above 0 a 0x7C-byte object is made (0x08B62344(0x7C), the target's word first) and set up with
// 0x0887F4FC(new, s6, target), 0x08875418(object, new, s5, s4, s3), 0x0887EE40(new, s7, a2 byte, a3 byte), 0x0886ADEC(new, 1) and 0x0886B2E8(target, new). The vector's counted string (header = data - 0xC) is released at the end:
// unless it is the empty header 0x08D75298 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B93554(header, sp) frees it.
void func_08876188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    const u32 mode = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x30u}, {18u, 0x38u}, {31u, 0x54u}, {30u, 0x50u}, {23u, 0x4Cu}, {22u, 0x48u}, {21u, 0x44u}, {20u, 0x40u}, {19u, 0x3Cu}, {17u, 0x34u}});
    const u32 vector = frame.at(0x4u);
    ctx.gpr[16] = vector;
    ctx.gpr[18] = object;
    put(rt, frame.at(0x20u), mode);
    ctx.gpr[30] = flag;
    call_set(rt, ctx, 0x08AB85F0u, {vector});
    const u32 read_1 = call_set(rt, ctx, 0x08AB856Cu, {vector});
    const u32 read_2 = call_set(rt, ctx, 0x08AB856Cu, {vector});
    const u32 read_3 = call_set(rt, ctx, 0x08AB856Cu, {vector});
    ctx.gpr[23] = read_1;
    ctx.gpr[22] = read_2;
    const u32 first_result = call_set(rt, ctx, 0x088755B4u, {object, read_3});
    ctx.gpr[21] = first_result;
    const u32 read_4 = call_set(rt, ctx, 0x08AB856Cu, {vector});
    const u32 second_result = call_set(rt, ctx, 0x08875584u, {object, read_4});
    ctx.gpr[20] = second_result;
    const u32 read_5 = call_set(rt, ctx, 0x08AB856Cu, {vector});
    ctx.gpr[19] = read_5;
    if (static_cast<s32>(word(rt, frame.at(0xCu))) > 0) {
        const u32 target = word(rt, object);
        ctx.gpr[17] = target;
        const u32 made = call_set(rt, ctx, 0x08B62344u, {0x7Cu});
        ctx.gpr[16] = made;
        call_set(rt, ctx, 0x0887F4FCu, {made, read_2, target});
        call_set(rt, ctx, 0x08875418u, {object, made, first_result, second_result, read_5});
        call_set(rt, ctx, 0x0887EE40u, {made, read_1, flag, word(rt, frame.at(0x20u))});
        call_set(rt, ctx, 0x0886ADECu, {made, 1u});
        call_set(rt, ctx, 0x0886B2E8u, {word(rt, object), made});
    }
    const u32 data = word(rt, vector);
    const u32 header = data - 0xCu;
    ctx.gpr[2] = 0x08D75298u;
    ctx.gpr[16] = header;
    if (header != 0x08D75298u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B93554u, {header, frame.at(0u)});
    }
}
// 0x08A5CD48 (a0 = the object, t1 = a second object; v0 = the object): formats a name on the frame (0x08B85EB8) and splits
// it into two strings (0x08A6B5E0 / 0x08A690E4 / 0x08A76764); looks an entry up with 0x08A5CB28 and passes a number read
// from that entry (0 otherwise) to 0x08A767EC, stores the entry in the object with 0x08B88F44 and releases the strings.
void func_08A5CD48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 other = ctx.gpr[9];
    GuestFrame frame(ctx, 0xC0u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0xA0u}, {22u, 0xB8u}, {19u, 0xACu}, {31u, 0xBCu}, {21u, 0xB4u}, {20u, 0xB0u}, {18u, 0xA8u}, {17u, 0xA4u}});
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x4u), 0x08C8EA6Cu, frame.at(0x1u)});
    call_set(rt, ctx, 0x08A6B5E0u, {frame.at(0x8u), frame.at(0x4u)});
    const u32 name = word(rt, frame.at(0x4u));
    if (name - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {name - 0x4u, 0xFFFFFFFFu})) <= 0)
            call_set(rt, ctx, 0x08B8542Cu, {name - 0xCu, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08A690E4u, {frame.at(0xCu), frame.at(0x8u)});
    call_set(rt, ctx, 0x08A76764u, {frame.at(0x10u), frame.at(0xCu)});
    call_set(rt, ctx, 0x08A5CB28u, {});
    const u32 entry = word(rt, ctx.gpr[2]);
    u32 number = 0u;
    if (entry != 0u) {
        const u32 label = word(rt, entry + 0x14u);
        call_set(rt, ctx, 0x08B7C830u, {frame.at(0x1Cu), label != 0u ? label : 0x08CAFFC4u, 0x80u, ctx.gpr[7], 0u});
        store8(frame.at(0x9Bu), 0u);
        number = word(rt, entry + 0x18u);
    }
    call_set(rt, ctx, 0x08A767ECu,
             {frame.at(0x10u), 0x08CAFFD0u, word(rt, other), frame.at(0x1Cu), number, other, ctx.gpr[10], ctx.gpr[11]});
    if (entry != 0u) call_set(rt, ctx, 0x08A62908u, {entry, frame.at(0x10u)});
    call_set(rt, ctx, 0x08B88F44u, {object, word(rt, frame.at(0x8u)) + 0x1Cu});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
    ctx.gpr[2] = object;
}
// 0x08AA0690 (a0 = the event, a1 = a name and the second argument of the two calls below, a2 / a3 pass through;
// v0 = 1 when the handler ran): formats the name on the frame and turns it into a string (0x08B85EB8 / 0x08A69530), looks
// a handler up (0x08A9DD14 / 0x08A76AB4 / 0x08A9BD30), binds it to a fresh holder (0x08A69B3C / 0x08A7BFD0 / 0x08A69D68)
// and invokes it as (holder, a0, a2, a3) through its virtual +0xCC.
void func_08AA0690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 event = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 argument2 = ctx.gpr[6];
    const u32 argument3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x20u}, {22u, 0x38u}, {21u, 0x34u}, {31u, 0x40u}, {20u, 0x30u}, {19u, 0x2Cu}, {17u, 0x24u}, {23u, 0x3Cu}, {18u, 0x28u}});
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x8u), name, frame.at(0x1u)});
    const u32 found = call_set(rt, ctx, 0x08A69530u, {frame.at(0x8u), name});
    const u32 text = word(rt, frame.at(0x8u));
    if (text - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {text - 0x4u, 0xFFFFFFFFu})) <= 0)
            call_set(rt, ctx, 0x08B8542Cu, {text - 0xCu, frame.at(0u)});
    }
    u32 invoked = 0u;
    if (found != 0u) {
        call_set(rt, ctx, 0x08A9DD14u, {frame.at(0xCu)});
        call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x10u), 0x08CF6ED8u});
        const u32 owner = word(rt, frame.at(0xCu));
        if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB277Cu});
        const u32 handler = word(rt, call_set(rt, ctx, 0x08A9BD30u, {owner}));
        if (handler == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2784u});
        call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x4u), name});
        call_set(rt, ctx, 0x08A7BFD0u, {handler, frame.at(0x10u), frame.at(0x4u)});
        call_set(rt, ctx, 0x08A69D68u, {frame.at(0x4u)});
        const u32 target = word(rt, frame.at(0x10u));
        if (target != 0u) {
            invoked = 1u;
            call_set(rt, ctx, word(rt, word(rt, target) + 0xCCu), {target, event, argument2, argument3});
        }
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    }
    ctx.gpr[2] = invoked;
}
void func_089354CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); G[20] = G[4]; G[17] = G[7]; G[16] = G[6]; call_here(rt, ctx, 0x089363FCu); G[4] = G[2]; G[6] = G[16]; G[7] = G[17]; G[8] = frame.at(0u); call_here(rt, ctx, 0x08936584u); G[4] = G[20]; { const bool tk = G[2] == 0u; G[3] = 0u; if (!tk) { G[3] = load8(frame.at(0x14u)); G[5] = load8(frame.at(0xEu)); G[6] = load8(frame.at(0xFu)); G[7] = load8(frame.at(0x10u)); G[8] = load8(frame.at(0x11u)); G[9] = load8(frame.at(0x12u)); G[10] = load8(frame.at(0x13u)); G[11] = load8(frame.at(0xCu)); G[12] = load8(frame.at(0xDu)); G[13] = load8(frame.at(0x19u)); G[14] = load8(frame.at(0x1Au)); G[15] = load8(frame.at(0x1Bu)); G[2] = load16(frame.at(0x2u)); G[16] = load8(frame.at(0x1Cu)); G[17] = load8(frame.at(0x1Du)); G[18] = load8(frame.at(0x1Eu)); G[19] = load8(frame.at(0x1Fu)); store16(G[20] + 0x120u, G[2]); store8(G[20] + 0x122u, G[3]); store8(G[20] + 0x148u, G[5]); store8(G[20] + 0x14Au, G[6]); store8(G[20] + 0x14Cu, G[7]); store8(G[20] + 0x149u, G[8]); store8(G[20] + 0x14Bu, G[9]); store8(G[20] + 0x14Du, G[10]); store8(G[20] + 0x129u, G[11]); store8(G[20] + 0x12Au, G[12]); store8(G[20] + 0x13Eu, G[13]); store8(G[20] + 0x13Fu, G[14]); store8(G[20] + 0x140u, G[15]); store8(G[20] + 0x141u, G[16]); store8(G[20] + 0x142u, G[17]); store8(G[20] + 0x143u, G[18]); store8(G[20] + 0x144u, G[19]); G[3] = load8(frame.at(0x15u)); G[5] = load8(frame.at(0x16u)); G[6] = load8(frame.at(0x17u)); G[7] = load8(frame.at(0x18u)); G[8] = load8(frame.at(0x21u)); G[9] = load8(frame.at(0x22u)); G[10] = load16(frame.at(0x4u)); G[11] = load16(frame.at(0x6u)); G[12] = load16(frame.at(0x8u)); G[13] = load16(frame.at(0xAu)); G[14] = load8(frame.at(0x24u)); G[15] = load8(frame.at(0x23u)); G[2] = load8(frame.at(0x20u)); store8(G[20] + 0x131u, G[3]); store8(G[20] + 0x145u, G[2]); store8(G[20] + 0x132u, G[5]); store8(G[20] + 0x138u, G[6]); store8(G[20] + 0x139u, G[7]); store8(G[20] + 0x146u, G[8]); store8(G[20] + 0x147u, G[9]); store16(G[20] + 0x134u, G[10]); store16(G[20] + 0x136u, G[11]); store8(G[20] + 0x13Au, G[12]); store8(G[20] + 0x13Bu, G[13]); store8(G[20] + 0x14Eu, G[14]); store8(G[20] + 0x14Fu, G[15]); call_here(rt, ctx, 0x089324B4u); { const bool tk = G[2] == 0u; G[3] = 0x1u; if (!tk) { G[2] = load16(frame.at(0x0u)); store16(G[20] + 0x156u, G[2]); } } } } G[2] = G[3];  G[20] = word(rt, frame.at(0x40u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); }  } }
void func_088619A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xB0u); put(rt, frame.at(0xA0u), G[31]); put(rt, frame.at(0x9Cu), G[19]); put(rt, frame.at(0x98u), G[18]); put(rt, frame.at(0x94u), G[17]); put(rt, frame.at(0x90u), G[16]); G[19] = G[5]; G[5] = G[6]; G[18] = G[4]; G[17] = G[8]; G[16] = G[7]; G[4] = word(rt, G[4] + 0x0u); G[4] = G[4] + 0x3F88u; call_here(rt, ctx, 0x08894098u); G[4] = word(rt, G[18] + 0x0u); G[5] = G[16]; G[4] = G[4] + 0x3F90u; call_here(rt, ctx, 0x08893C40u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088D441Cu); G[5] = G[17]; { const bool tk = G[17] == 0u; G[4] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x088D44BCu); } } call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B0980Cu); G[4] = G[2]; G[5] = 0u; G[16] = G[2]; call_here(rt, ctx, 0x08B08A0Cu); G[5] = 0u; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[16]; if (!tk) { call_here(rt, ctx, 0x08B08A0Cu); put(rt, frame.at(0x7Cu), G[2]); } } G[4] = G[16]; call_here(rt, ctx, 0x08B08A58u); G[4] = word(rt, G[18] + 0x0u); G[5] = 0u; store8(frame.at(0x80u), G[2]); call_here(rt, ctx, 0x088618E8u); { const bool tk = G[19] == 0u; G[5] = G[19]; if (!tk) { G[2] = word(rt, G[18] + 0x0u); G[4] = word(rt, G[2] + 0x5ACu); G[4] = G[4] + 0x4u; call_here(rt, ctx, 0x088D4824u); } } G[2] = word(rt, G[18] + 0x0u); G[5] = frame.at(0u); G[6] = 0x1u; G[4] = word(rt, G[2] + 0x5ACu); call_here(rt, ctx, 0x088DFA68u); G[16] = G[2]; { const bool tk = G[2] == 0u; G[4] = G[18]; if (!tk) { G[5] = word(rt, G[18] + 0x0u); G[2] = 0x1u; store8(G[5] + 0x5A9u, G[2]); G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[3] + 0x5B8u); put(rt, G[2] + 0x30u, G[18]); G[2] = 0x08860000u; G[2] = 0x08860B3Cu; G[3] = word(rt, G[18] + 0x0u); G[5] = word(rt, G[3] + 0x5B8u); put(rt, G[5] + 0x2Cu, G[2]); G[2] = 0x08860000u; G[2] = 0x0885F550u; G[3] = word(rt, G[18] + 0x0u); G[6] = word(rt, G[3] + 0x5B8u); G[3] = 0x08860000u; G[3] = 0x08861FD0u; put(rt, G[6] + 0x24u, G[2]); G[2] = 0x08860000u; G[2] = 0x08860C2Cu; put(rt, G[6] + 0x20u, G[3]); G[5] = word(rt, G[18] + 0x0u); G[3] = word(rt, G[5] + 0x5B8u); put(rt, G[3] + 0x28u, G[2]); call_here(rt, ctx, 0x0885E7DCu); } } G[4] = frame.at(0u); call_here(rt, ctx, 0x088D435Cu); G[2] = G[16];  G[19] = word(rt, frame.at(0x9Cu)); G[18] = word(rt, frame.at(0x98u)); G[17] = word(rt, frame.at(0x94u)); G[16] = word(rt, frame.at(0x90u)); }  } }
// 0x08A52D3C (a0 = the object, a1 = a text, a2 = a count, a3 / a4 pass through; v0 = the object): builds a temporary list
// from a1 (0x08A52AA8); for a non-negative count it copies that many entries (0x08A5B70C / 0x08A52AB0). Then calls the
// object's virtuals +0x15C and +0x120, makes a holder of the second result (0x08A5BC20 / 0x08A56018) and adds it to the
// object with 0x08A57B54; the object at frame + 0x14 being null is an assert (0x08A5BB7C).
void func_08A52D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x20u), G[16]); G[19] = frame.at(0x14u); G[21] = G[4]; G[4] = G[19]; G[17] = G[6]; put(rt, frame.at(0x10u), G[5]); G[5] = frame.at(0x10u); G[23] = G[7]; G[22] = G[8]; G[20] = frame.at(0x8u); G[18] = frame.at(0xCu); call_here(rt, ctx, 0x08A52AA8u); G[4] = 0x44u; { const bool tk = static_cast<std::int32_t>(G[17]) < 0; G[5] = 0x4u; if (!tk) { call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[19]; G[6] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[4] = G[18]; G[5] = frame.at(0x4u); put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08A52AB0u); { const bool tk = G[19] == G[18]; G[4] = G[19]; if (!tk) { G[5] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = word(rt, frame.at(0x14u)); G[4] = 0x08CB0000u; { const bool tk = G[16] != 0u; G[4] = 0x08CAF4A8u; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[2] = word(rt, G[3] + 0x15Cu); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x14u)); G[4] = 0x08CB0000u; { const bool tk = G[16] != 0u; G[4] = 0x08CAF4A8u; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x120u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[5] = 0x4u; G[4] = 0x18u; G[17] = G[2]; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[8] = G[22]; G[7] = G[23]; G[6] = G[17]; G[5] = 0u; G[16] = G[2]; call_here(rt, ctx, 0x08A56018u); G[4] = G[20]; G[5] = frame.at(0u); put(rt, frame.at(0x0u), G[16]); call_here(rt, ctx, 0x08A5BAB8u); G[5] = G[19]; G[6] = G[20]; G[4] = G[21]; G[7] = 0u; call_here(rt, ctx, 0x08A57B54u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[21];  G[19] = word(rt, frame.at(0x2Cu)); G[21] = word(rt, frame.at(0x34u)); G[17] = word(rt, frame.at(0x24u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
// 0x08A8EC08 (a0 = the object): its destructor. Puts 0x08CCD480 in the vtable and releases the two strings built at +0x20
// and +0x24 (0x08AAE204 / 0x08A5BAD8 / 0x08A5B9EC), the string at +0x74, the members at +0x6C / +0x64 / +0x5C / +0x54
// (and the vector at +0x44), +0x3C / +0x34 / +0x30 / +0x28 / +0x24 / +0x20 (and the vector at +0x14), then 0x08A780B0.
void func_08A8EC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); G[2] = 0x08CD0000u; G[2] = 0x08CCD480u; G[18] = 0x08CF0000u; G[16] = frame.at(0x8u); G[5] = 0x08CF6ED8u; G[19] = G[4] + 0x20u; G[17] = G[4]; put(rt, G[4] + 0x0u, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AAE204u); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (!tk) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; G[16] = frame.at(0x4u); call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08CF6ED8u; G[4] = G[16]; G[18] = G[17] + 0x24u; call_here(rt, ctx, 0x08AAE204u); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x74u); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[17] + 0x6Cu; call_here(rt, ctx, 0x08C2AB00u); G[4] = G[17] + 0x64u; call_here(rt, ctx, 0x08C2AB4Cu); G[4] = G[17] + 0x5Cu; call_here(rt, ctx, 0x08C2AB4Cu); G[16] = G[17] + 0x44u; G[4] = G[17] + 0x54u; call_here(rt, ctx, 0x08C2AB98u); G[4] = word(rt, G[17] + 0x44u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08C2AA30u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x44u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0x3Cu; call_here(rt, ctx, 0x08C2B458u); G[4] = G[17] + 0x34u; call_here(rt, ctx, 0x08C2B458u); G[4] = G[17] + 0x30u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x28u; call_here(rt, ctx, 0x08C2AF5Cu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = G[17] + 0x14u; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x14u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08C2AD44u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x14u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08A2B6F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); G[20] = G[4]; G[4] = 0x08D70000u; G[4] = 0x08D69170u; G[5] = frame.at(0x1Cu); call_here(rt, ctx, 0x08B32138u); { const bool tk = G[2] == 0u; G[31] = word(rt, frame.at(0x44u)); if (!tk) { G[19] = frame.at(0x18u); G[4] = G[19]; call_here(rt, ctx, 0x08A6BA44u); G[5] = 0x08CB0000u; G[4] = G[19]; G[5] = 0x08CAC004u; call_here(rt, ctx, 0x08A6BBE8u); G[5] = word(rt, frame.at(0x1Cu)); G[16] = frame.at(0x14u); G[17] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); { const bool tk = G[17] == G[16]; G[5] = word(rt, frame.at(0x14u)); if (!tk) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08CB0000u; G[4] = G[19]; G[5] = 0x08CAC00Cu; G[16] = frame.at(0xCu); call_here(rt, ctx, 0x08A6BBE8u); G[17] = frame.at(0x10u); G[4] = G[16]; G[5] = frame.at(0x20u); G[6] = frame.at(0x1u); G[18] = G[2]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[18] == G[17]; G[5] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[5] = 0x08CB0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = 0x08CB0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[5] = 0x08CB0000u; } } } } G[5] = 0x08CAC018u; G[4] = G[19]; call_here(rt, ctx, 0x08A6BBE8u); G[16] = frame.at(0x8u); G[17] = G[2]; G[4] = G[16]; G[5] = frame.at(0x28u); G[6] = 0x6u; G[7] = 0x10u; call_here(rt, ctx, 0x08A950D0u); { const bool tk = G[17] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (!tk) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; G[16] = frame.at(0x4u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x08A76ABCu); { const bool tk = G[20] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } }  G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
// 0x08A8ED98 (a0 = the object): the destructor of 0x08A8EC08 and then one more 0x08A5BC54(object, 0x7C, 4) that frees the
// object itself (v0 is that call's).
void func_08A8ED98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); G[2] = 0x08CD0000u; G[2] = 0x08CCD480u; G[18] = 0x08CF0000u; G[16] = frame.at(0x8u); G[5] = 0x08CF6ED8u; G[19] = G[4] + 0x20u; G[17] = G[4]; put(rt, G[4] + 0x0u, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AAE204u); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (!tk) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; G[16] = frame.at(0x4u); call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08CF6ED8u; G[4] = G[16]; G[18] = G[17] + 0x24u; call_here(rt, ctx, 0x08AAE204u); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x74u); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[17] + 0x6Cu; call_here(rt, ctx, 0x08C2AB00u); G[4] = G[17] + 0x64u; call_here(rt, ctx, 0x08C2AB4Cu); G[4] = G[17] + 0x5Cu; call_here(rt, ctx, 0x08C2AB4Cu); G[16] = G[17] + 0x44u; G[4] = G[17] + 0x54u; call_here(rt, ctx, 0x08C2AB98u); G[4] = word(rt, G[17] + 0x44u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08C2AA30u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x44u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0x3Cu; call_here(rt, ctx, 0x08C2B458u); G[4] = G[17] + 0x34u; call_here(rt, ctx, 0x08C2B458u); G[4] = G[17] + 0x30u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x28u; call_here(rt, ctx, 0x08C2AF5Cu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = G[17] + 0x14u; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x14u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08C2AD44u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x14u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[17]; G[5] = 0x7Cu; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); }  } }
// 0x08AAF9C8 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CB3A60, sp + 1) formats the text on the frame (+4); the text is released
// (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: a handle is released
// (0x08A5BA08), then the old one at +0x38 (0x08A5B930) and the new one is stored at +0x38. Two pairs of words (0x08C8FFF4 / 0x08C8FFEC, and 0x08C8FFE4 / 0x08C8FFDC, each with its next word) are copied to the frame and registered with
// 0x08C36F84(object, 0x08CB3A68, copy, copy, ...) and 0x08C36E08(object, 0x08CB3A70, copy, copy, ...); then 0x08C261D8(object, 0x08CF6FC0 / 0x08CF6F9C / 0x08CF6FC4, 0x08AAF4F4 / 0x08AAF4FC / 0x08AAF504) three times.
void func_08AAF9C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x3Cu}, {18u, 0x38u}, {16u, 0x30u}, {17u, 0x34u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB3A60u, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    // a pair of words copied to two 8-byte slots on the frame
    auto copy_pair = [&](u32 first_from, u32 second_from, u32 first_to, u32 second_to) {
        const u32 first_high = word(rt, first_from + 0x4u), second_high = word(rt, second_from + 0x4u);
        ctx.gpr[8] = first_high;
        ctx.gpr[9] = second_high;
        const u32 first_low = word(rt, first_from), second_low = word(rt, second_from);
        put(rt, first_to, first_low);
        put(rt, first_to + 0x4u, first_high);
        put(rt, second_to, second_low);
        put(rt, second_to + 0x4u, second_high);
    };
    put(rt, object + 0x38u, handle);
    copy_pair(0x08C8FFF4u, 0x08C8FFECu, frame.at(0x20u), frame.at(0x18u));
    call_set(rt, ctx, 0x08C36F84u, {object, 0x08CB3A68u, frame.at(0x20u), frame.at(0x18u), ctx.gpr[8], ctx.gpr[9]});
    copy_pair(0x08C8FFE4u, 0x08C8FFDCu, frame.at(0x10u), frame.at(0x8u));
    call_set(rt, ctx, 0x08C36E08u, {object, 0x08CB3A70u, frame.at(0x10u), frame.at(0x8u), ctx.gpr[8], ctx.gpr[9]});
    call_set(rt, ctx, 0x08C261D8u, {object, 0x08CF6FC0u, 0x08AAF4F4u});
    call_set(rt, ctx, 0x08C261D8u, {object, 0x08CF6F9Cu, 0x08AAF4FCu});
    call_set(rt, ctx, 0x08C261D8u, {object, 0x08CF6FC4u, 0x08AAF504u});
}
void func_089AE074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC0000u; G[2] = 0x08CBB598u; G[17] = G[4]; G[16] = G[17] + 0x12Cu; G[4] = word(rt, G[4] + 0xB0u); put(rt, G[17] + 0x0u, G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[17] + 0x58u); call_here(rt, ctx, 0x08B35254u); G[4] = G[17] + 0x178u; call_here(rt, ctx, 0x08BF8B64u); G[4] = word(rt, G[17] + 0x12Cu); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BF919Cu); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x12Cu); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0xB8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x68u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x54u); { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x50u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x50u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x4Cu); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x4Cu); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x48u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x48u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x44u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x44u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x40u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x40u); } } { const bool tk = G[4] == 0u; G[16] = G[17] + 0x30u; if (!tk) { call_here(rt, ctx, 0x08A5B930u); } } G[4] = word(rt, G[17] + 0x30u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x30u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x24u; G[4] = word(rt, G[17] + 0x24u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x24u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x18u; G[4] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x18u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u);  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_089AE21C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC0000u; G[2] = 0x08CBB598u; G[17] = G[4]; G[16] = G[17] + 0x12Cu; G[4] = word(rt, G[4] + 0xB0u); put(rt, G[17] + 0x0u, G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[17] + 0x58u); call_here(rt, ctx, 0x08B35254u); G[4] = G[17] + 0x178u; call_here(rt, ctx, 0x08BF8B64u); G[4] = word(rt, G[17] + 0x12Cu); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BF919Cu); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x12Cu); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0xB8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x68u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x54u); { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x50u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x50u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x4Cu); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x4Cu); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x48u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x48u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x44u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x44u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x40u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x40u); } } { const bool tk = G[4] == 0u; G[16] = G[17] + 0x30u; if (!tk) { call_here(rt, ctx, 0x08A5B930u); } } G[4] = word(rt, G[17] + 0x30u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x30u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x24u; G[4] = word(rt, G[17] + 0x24u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x24u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x18u; G[4] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x18u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u);  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x08C77B74 (a0 = a result pair, a1 = the object, a2 / a3 / t0 pass through, t1 = six words; v0 = a0): clears the
// pair and the object's fields, copies the six words through the frame, asks 0x08C76B7C for a handle and, when it gives
// one, 0x08B618AC supplies two more words that fill the pair, the object's links and three further copies of the six.
void func_08C77B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 pair = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 argument2 = ctx.gpr[6];
    const u32 argument3 = ctx.gpr[7];
    const u32 argument4 = ctx.gpr[8];
    const u32 source = ctx.gpr[9];
    GuestFrame frame(ctx, 0x90u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x80u}, {19u, 0x7Cu}, {18u, 0x78u}, {17u, 0x74u}, {16u, 0x70u}, {31u, 0x84u}});
    u32 words[6];
    for (unsigned i = 0; i < 6u; ++i) words[i] = word(rt, source + i * 4u);
    for (unsigned i = 0; i < 6u; ++i) put(rt, frame.at(0x18u + i * 4u), words[i]);
    put(rt, pair + 0x0u, 0xFFFFFFFFu);
    for (unsigned i = 0; i < 6u; ++i) put(rt, pair + 0x8u + i * 4u, 0u);
    put(rt, pair + 0x4u, 0xFFFFFFFFu);
    if (call_set(rt, ctx, 0x08C76B7Cu, {object, object, argument2, argument3, argument4, source}) == 0u) {
        ctx.gpr[2] = pair;
        return;
    }
    call_set(rt, ctx, 0x08B618ACu, {object + 0x24u, object, argument2, argument3, argument4});
    const u32 low = ctx.gpr[2], high = ctx.gpr[3];
    put(rt, frame.at(0x48u), low);
    put(rt, frame.at(0x4Cu), high);
    u32 copied[6];
    for (unsigned i = 0; i < 6u; ++i) copied[i] = word(rt, frame.at(0x18u + i * 4u));
    const u32 field = word(rt, object + 0x78u);
    const u32 linked = word(rt, object + 0x94u);
    put(rt, pair + 0x0u, low);
    put(rt, pair + 0x4u, high);
    put(rt, object + 0x9Cu, linked);
    for (unsigned i = 0; i < 6u; ++i) put(rt, frame.at(0x50u + i * 4u), 0u);
    put(rt, object + 0xCu, field);
    store8(object + 0x81u, 0u);
    store8(object + 0x82u, 0u);
    put(rt, object + 0xA0u, linked);
    put(rt, object + 0x4u, field);
    put(rt, object + 0x8u, field);
    put(rt, object + 0x14u, 0u);
    put(rt, object + 0x10u, 0u);
    put(rt, object + 0x18u, 0u);
    for (unsigned i = 0; i < 6u; ++i) put(rt, object + 0x48u + i * 4u, copied[i]);
    for (unsigned i = 0; i < 6u; ++i) put(rt, pair + 0x8u + i * 4u, copied[i]);
    for (unsigned i = 0; i < 6u; ++i) put(rt, frame.at(0x30u + i * 4u), copied[i]);
    for (unsigned i = 0; i < 6u; ++i) put(rt, frame.at(i * 4u), copied[i]);
    ctx.gpr[2] = pair;
}
void func_089AE3C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC0000u; G[2] = 0x08CBB598u; G[17] = G[4]; G[16] = G[17] + 0x12Cu; G[4] = word(rt, G[4] + 0xB0u); put(rt, G[17] + 0x0u, G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[17] + 0x58u); call_here(rt, ctx, 0x08B35254u); G[4] = G[17] + 0x178u; call_here(rt, ctx, 0x08BF8B64u); G[4] = word(rt, G[17] + 0x12Cu); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BF919Cu); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x12Cu); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0xB8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x68u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x54u); { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x50u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x50u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x4Cu); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x4Cu); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x48u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x48u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x44u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x44u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x40u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x40u); } } { const bool tk = G[4] == 0u; G[16] = G[17] + 0x30u; if (!tk) { call_here(rt, ctx, 0x08A5B930u); } } G[4] = word(rt, G[17] + 0x30u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x30u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x24u; G[4] = word(rt, G[17] + 0x24u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x24u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x18u; G[4] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x18u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[17]; G[5] = 0x180u; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u);  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x08A2C594 (a0 = the object; v0 = it): formats a name on the frame (0x08A69B3C) and makes a 0x38-byte holder of it
// (0x08A5BC20 / 0x08A7E914) which goes into the object with 0x08A76AB4, then registers seven (name 0x08CAC170 .. 0x08CAC1B8,
// function 0x08A2CF30 .. 0x08A2C754) pairs through 0x08B86FB0, asserting the object's table with 0x08A5BB7C where it is null.
void func_08A2C594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {31u, 0x18u}, {16u, 0x10u}});
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CAC160u});
    const u32 holder = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 0x4u});
    call_set(rt, ctx, 0x08A7E914u, {holder, frame.at(0u)});
    put(rt, frame.at(0x4u), holder);
    call_set(rt, ctx, 0x08A76AB4u, {object, frame.at(0x4u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    const std::pair<u32, u32> entries[7] = {
        {0x08CAC170u, 0x08A2CF30u}, {0x08CAC180u, 0x08A2CDFCu}, {0x08CAC18Cu, 0x08A2CCC8u},
        {0x08CAC198u, 0x08A2CB48u}, {0x08CAC1A4u, 0x08A2C9A4u}, {0x08CAC1B0u, 0x08A2C87Cu},
        {0x08CAC1B8u, 0x08A2C754u},
    };
    for (const std::pair<u32, u32> &entry : entries) {
        const u32 table = word(rt, object);
        if (table == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC168u, 0x08CB0000u});
        call_set(rt, ctx, 0x08B86FB0u, {table, entry.first, entry.second});
    }
    ctx.gpr[2] = object;
}
// 0x08A0CDA4 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CA9454, sp + 1) formats the text on the frame (+4); the text is released (unless it is the empty header
// 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp) frees it). 0x08965418() gives a handle: a handle is released (0x08A5BA08), then the old one at +0x38 (0x08A5B930), and the new one is stored at +0x38.
// 0x08B8A004(object, 0x08CF6F24, 0x08A0CBA4) registers a handler, then five 8-byte values (at 0x08C8DF94, 0x08C8DF8C, 0x08C8DF84, 0x08C8DF7C, 0x08C8DF74) are copied to the frame (+0x28, +0x20, +0x18, +0x10, +8) and registered
// with 0x08C145E4 (twice), 0x08C1433C, 0x08C14490 and 0x08C1433C (object, name, copy, 0, low word, high word), the names 0x08CA9464, 0x08CA9468, 0x08CA946C, 0x08CA9474, 0x08CA947C.
void func_08A0CDA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x3Cu}, {18u, 0x38u}, {16u, 0x30u}, {17u, 0x34u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA9454u, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08965418u, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08A0CBA4u});
    struct Registration {
        u32 source, name, slot, callee;
    };
    const Registration registrations[5] = {
        {0x08C8DF94u, 0x08CA9464u, 0x28u, 0x08C145E4u}, {0x08C8DF8Cu, 0x08CA9468u, 0x20u, 0x08C145E4u}, {0x08C8DF84u, 0x08CA946Cu, 0x18u, 0x08C1433Cu},
        {0x08C8DF7Cu, 0x08CA9474u, 0x10u, 0x08C14490u}, {0x08C8DF74u, 0x08CA947Cu, 0x8u, 0x08C1433Cu},
    };
    for (const Registration &entry : registrations) {
        const u32 low = word(rt, entry.source), high = word(rt, entry.source + 0x4u);
        ctx.gpr[8] = low;
        ctx.gpr[9] = high;
        put(rt, frame.at(entry.slot), low);
        put(rt, frame.at(entry.slot + 0x4u), high);
        call_set(rt, ctx, entry.callee, {object, entry.name, frame.at(entry.slot), 0u, low, high});
    }
}
// 0x08AA0AA4 (a0 = the object (word +0 = a target); v0 = the object): two 0x38-byte objects are made (0x08A5BC20(0x38, 4)), set up with 0x08A7E914(object, 0x08CF6F04 / 0x08CF6FEC) and put in handles on the frame
// (0x08A76AB4(handle, frame slot)). The first one goes into the object (0x08A76AB4(object, frame + 8)); 0x08AA0DAC(frame + 0x14) fills a record (a null target calls the assert 0x08A5BB7C(0x08CB28AC)), the target gets 0x08A7CE50(target, record) and,
// after two texts are formatted into frame +0x10 (0x08B85EB8(.., 0x08CB28B4, sp + 3)) and +0xC (0x08B85EB8(.., 0x08CB28B8, sp + 2)), 0x08A7EAD8(target, record, text 1, text 2). Both texts are released (unless it is the empty header
// 0x08D74FD0 their count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp + 1 / sp) frees them). The second handle (frame + 0x18) goes to the target the same way (0x08A7CE50(target, handle)); then
// 0x08A5B9EC releases the second handle and the record.
void func_08AA0AA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x2Cu}, {31u, 0x34u}, {20u, 0x30u}, {16u, 0x20u}, {18u, 0x28u}, {17u, 0x24u}});
    const u32 empty = 0x08D74FD0u;
    const u32 record = frame.at(0x14u);
    ctx.gpr[19] = object;
    ctx.gpr[20] = record;
    // the target (word +0), the assert 0x08A5BB7C(0x08CB28AC) when it is null
    auto target_of_object = [&] {
        const u32 target = word(rt, object);
        ctx.gpr[16] = target;
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB28ACu});
        return target;
    };
    u32 made = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[16] = made;
    call_set(rt, ctx, 0x08A7E914u, {made, 0x08CF6F04u});
    put(rt, frame.at(0x8u), made);
    call_set(rt, ctx, 0x08A76AB4u, {object, frame.at(0x8u)});
    call_set(rt, ctx, 0x08AA0DACu, {record});
    u32 target = target_of_object();
    call_set(rt, ctx, 0x08A7CE50u, {target, record});
    const u32 target_again = word(rt, object);
    ctx.gpr[18] = target_again;
    const u32 first_text = frame.at(0x10u), second_text = frame.at(0xCu);
    ctx.gpr[17] = first_text;
    if (target_again == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB28ACu});
    call_set(rt, ctx, 0x08B85EB8u, {first_text, 0x08CB28B4u, frame.at(0x3u)});
    ctx.gpr[16] = second_text;
    call_set(rt, ctx, 0x08B85EB8u, {second_text, 0x08CB28B8u, frame.at(0x2u)});
    call_set(rt, ctx, 0x08A7EAD8u, {target_again, record, first_text, second_text});
    ctx.gpr[17] = empty;
    // releases the counted string at `text`; `pad` is the second argument of the free
    auto release = [&](u32 text, u32 pad) {
        const u32 data = word(rt, text);
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        ctx.gpr[4] = data;
        if (header == empty) return;
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        ctx.gpr[4] = word(rt, first_text);
        if (static_cast<s32>(count) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, pad});
            ctx.gpr[4] = word(rt, first_text);
        }
    };
    release(second_text, frame.at(0x1u));
    release(first_text, frame.at(0u));
    ctx.gpr[5] = 4u;
    made = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    const u32 handle = frame.at(0x18u);
    ctx.gpr[17] = handle;
    ctx.gpr[16] = made;
    call_set(rt, ctx, 0x08A7E914u, {made, 0x08CF6FECu});
    put(rt, frame.at(0x4u), made);
    call_set(rt, ctx, 0x08A76AB4u, {handle, frame.at(0x4u)});
    target = target_of_object();
    call_set(rt, ctx, 0x08A7CE50u, {target, handle});
    call_set(rt, ctx, 0x08A5B9ECu, {handle});
    call_set(rt, ctx, 0x08A5B9ECu, {record});
    ctx.gpr[2] = object;
}
// 0x08A9D0D4 (a0 = the object, a1 = a text, a2 = a name, a3 = a value; v0 = the object): compares the name with the
// text's length (0x08B8D96C / 0x08B85EB8 / 0x08B36170, the shorter one wins) and, when they match, stores the text in the
// object (0x08A9C7B8 / 0x08A5BAD8); then makes a holder on the frame (0x08A76AA0) and records it with 0x08A9C1C0.
void func_08A9D0D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 name = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x50u}, {22u, 0x48u}, {23u, 0x4Cu}, {19u, 0x3Cu}, {31u, 0x54u}, {21u, 0x44u}, {20u, 0x40u}, {18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    put(rt, frame.at(0x20u), value);
    call_set(rt, ctx, 0x08A90470u, {frame.at(0x14u), 0x08CF6ED8u});
    const u32 length = word(rt, word(rt, name) - 0xCu);
    u32 kept = 0u;
    if (static_cast<s32>(length) >= 4) {
        if (length < length - 4u) call_set(rt, ctx, 0x08B5AEDCu, {0x08CB2680u, 0x08CF6ED8u, length - 4u, 0x4u});
        call_set(rt, ctx, 0x08B8D96Cu, {frame.at(0x10u), name, length - 4u, 0x4u});
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x4u), 0x08CB2698u, frame.at(0x2u)});
        const u32 first = word(rt, frame.at(0x10u));
        const u32 second = word(rt, frame.at(0x4u));
        const u32 first_length = word(rt, first - 0xCu);
        const u32 second_length = word(rt, second - 0xCu);
        put(rt, frame.at(0x18u), first_length);
        put(rt, frame.at(0x1Cu), second_length);
        // 0x08B36170 compares the shorter of the two counts, handed to it as an address on the frame
        const u32 slot = second_length < first_length ? frame.at(0x1Cu) : frame.at(0x18u);
        const u32 common = call_set(rt, ctx, 0x08B36170u,
                                    {first, second, word(rt, slot), first_length, slot, ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
        kept = first_length - second_length;
        if (common != 0u) kept = common;
        if (second - 0xCu != 0x08D74FD0u) {
            if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {second - 0x4u, 0xFFFFFFFFu})) <= 0)
                call_set(rt, ctx, 0x08B8542Cu, {second - 0xCu, frame.at(0x1u)});
        }
        if (first - 0xCu != 0x08D74FD0u) {
            if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {first - 0x4u, 0xFFFFFFFFu})) <= 0)
                call_set(rt, ctx, 0x08B8542Cu, {first - 0xCu, frame.at(0u)});
        }
        if (kept == 0u) {
            call_set(rt, ctx, 0x08A9C7B8u, {frame.at(0xCu), text, name});
            call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x14u), word(rt, frame.at(0xCu))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        }
    }
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x8u)});
    call_set(rt, ctx, 0x08A9C1C0u, {object, text, frame.at(0x14u), word(rt, frame.at(0x20u)), frame.at(0x8u), 0u, 0u});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    ctx.gpr[2] = object;
}
void func_089C7CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[21] = G[4]; G[17] = word(rt, G[5] + 0x0u); { const bool tk = G[17] != 0u; G[18] = G[5]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08CA65F4u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[16] = frame.at(0xCu); G[5] = G[17]; G[4] = G[16]; G[20] = frame.at(0x10u); call_here(rt, ctx, 0x08A923F4u); G[5] = G[16]; G[4] = G[20]; call_here(rt, ctx, 0x089BB3A8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x10u)); { const bool tk = G[2] == 0u; if (!tk) { G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; G[4] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA65F4u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08A7BDB0u); G[17] = frame.at(0x14u); G[5] = G[2]; G[4] = G[17]; G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B85EB8u); G[16] = word(rt, frame.at(0x10u)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6444u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[2] = word(rt, G[3] + 0x108u); { const std::uint32_t tg = G[2]; G[5] = G[17]; call_here(rt, ctx, tg); } G[19] = word(rt, G[21] + 0x40u); { const bool tk = G[19] != 0u; if (tk) { G[16] = word(rt, G[18] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[18] + 0x0u); } } { const bool tk = G[16] != 0u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08CA65F4u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08A7BCC0u); G[16] = frame.at(0x8u); G[17] = G[2]; G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x089BAC2Cu); G[5] = G[17]; G[6] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08C00A44u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, frame.at(0x10u)); G[16] = frame.at(0x4u); G[5] = 0x08C90000u; G[4] = G[16]; G[5] = 0x08C8CC2Cu; G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B85EB8u); G[4] = G[21]; G[6] = G[17]; G[7] = G[16]; G[5] = 0x21u; call_here(rt, ctx, 0x089C79F0u); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[17]; if (tk) { G[4] = word(rt, frame.at(0x14u)); } else { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = word(rt, frame.at(0x14u)); if (!tk) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, frame.at(0x14u)); } } } } G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[17] == G[16]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } } } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu);  G[21] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_089CA288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); G[16] = frame.at(0xCu); G[20] = G[4]; G[4] = G[16]; G[19] = G[5]; G[5] = G[6]; G[6] = G[7]; G[17] = frame.at(0x4u); G[21] = frame.at(0x10u); call_here(rt, ctx, 0x089C8818u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08A69654u); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0x8u); G[4] = G[21]; put(rt, frame.at(0x8u), G[18]); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[17]; call_here(rt, ctx, 0x08A69D68u); G[4] = G[20]; call_here(rt, ctx, 0x089C843Cu); G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x089C9CE4u); { const bool tk = G[2] == 0u; G[17] = 0u; if (!tk) { G[16] = word(rt, frame.at(0x10u)); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[19]; call_here(rt, ctx, 0x08A7CE50u); G[16] = word(rt, G[20] + 0x3Cu); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[21]; call_here(rt, ctx, 0x08A7CE50u); G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6444u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x12Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6444u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x130u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[17] = word(rt, G[20] + 0x3Cu); { const bool tk = G[17] != 0u; G[16] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, frame.at(0x10u)); } } { const bool tk = G[16] != 0u; G[4] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08A7BCC0u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A7DE18u); G[17] = 0x1u; } } G[4] = G[20]; call_here(rt, ctx, 0x089C83A4u); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[2] = G[17]; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = G[17]; if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[2] = G[17]; } } } }  G[16] = word(rt, frame.at(0x20u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x28u)); }  } }
void func_088089D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[5] = 0x08CA0000u; G[5] = 0x08C9802Cu; G[18] = G[4]; G[4] = frame.at(0u); G[17] = frame.at(0x8u); call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = frame.at(0u); G[16] = G[2]; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0x4u); G[4] = G[17]; put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[4] = 0x08CA0000u; if (!tk) { G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); } } call_here(rt, ctx, 0x08808518u); G[5] = 0x08CA0000u; G[4] = G[16]; G[5] = 0x08C9803Cu; G[6] = G[2]; call_here(rt, ctx, 0x08A7CAD4u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; } } G[6] = 0x08810000u; G[4] = G[16]; G[5] = 0x08C98048u; G[6] = 0x08808E60u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; } } G[6] = 0x08810000u; G[4] = G[16]; G[5] = 0x08C98050u; G[6] = 0x08809608u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; } } G[6] = 0x08810000u; G[4] = G[16]; G[5] = 0x08C98064u; G[6] = 0x088095C0u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; } } G[6] = 0x08810000u; G[4] = G[16]; G[5] = 0x08C98070u; G[6] = 0x08809574u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; } } G[6] = 0x08810000u; G[4] = G[16]; G[5] = 0x08C9807Cu; G[6] = 0x08809528u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; } } G[6] = 0x08810000u; G[4] = G[16]; G[5] = 0x08C9808Cu; G[6] = 0x088094DCu; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08C98034u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[17]; call_here(rt, ctx, 0x08A7CE50u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu);  G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_089C9F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); G[18] = G[5]; G[5] = 0x08CA0000u; G[16] = frame.at(0x4u); G[5] = 0x08CA6680u; G[20] = frame.at(0x8u); G[19] = G[4]; G[4] = G[16]; G[17] = G[6]; G[6] = frame.at(0x2u); G[21] = G[7] & 0xFFu; call_here(rt, ctx, 0x08B85EB8u); G[6] = G[16]; G[4] = G[20]; G[5] = G[17]; call_here(rt, ctx, 0x089C8818u); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[22] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[22]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[17] = 0u; call_here(rt, ctx, 0x08B2E05Cu); G[4] = G[19]; call_here(rt, ctx, 0x089C843Cu); G[4] = G[20]; G[5] = G[18]; call_here(rt, ctx, 0x089C9CE4u); { const bool tk = G[2] == 0u; if (!tk) { G[16] = word(rt, G[19] + 0x3Cu); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[18]; call_here(rt, ctx, 0x08A7CE50u); G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[20]; call_here(rt, ctx, 0x089B47ECu); G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x12Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[2] = word(rt, G[3] + 0x130u); { const std::uint32_t tg = G[2]; G[17] = 0x1u; call_here(rt, ctx, tg); } { const bool tk = G[21] == 0u; if (!tk) { G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[5] = G[19]; G[2] = word(rt, G[3] + 0x150u); { const std::uint32_t tg = G[2]; G[17] = 0x1u; call_here(rt, ctx, tg); } } } } } G[4] = G[19]; call_here(rt, ctx, 0x089C83A4u); call_here(rt, ctx, 0x08B2E05Cu); G[4] = word(rt, frame.at(0x8u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[22] == G[16]; G[2] = G[17]; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = G[17]; if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[2] = G[17]; } } } }  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); }  } }
void func_088DE3F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[6] = 0u; G[18] = G[5]; G[5] = 0u; G[17] = G[4]; G[16] = 0u; call_here(rt, ctx, 0x088DE24Cu); { const bool tk = G[2] == 0u; G[3] = G[18] << 5; if (!tk) { G[2] = G[18] << 7; G[2] = G[2] - G[3]; G[4] = word(rt, G[17] + 0xC0u); G[2] = G[2] - G[18]; G[2] = G[2] << 3; G[19] = G[4] + G[2]; G[3] = word(rt, G[19] + 0x4u); G[2] = 0x1u; { const bool tk = G[3] != G[2]; G[4] = G[17]; if (!tk) { G[2] = word(rt, G[19] + 0x1Cu); G[4] = word(rt, G[17] + 0x14Cu); G[5] = G[18]; G[20] = 0u < G[2] ? 1u : 0u; G[21] = word(rt, G[19] + 0xCu); call_here(rt, ctx, 0x088F8D58u); G[4] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x088DA0BCu); { const bool tk = G[2] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x134u); } else { G[4] = word(rt, G[17] + 0x148u); G[5] = G[18]; G[6] = G[20]; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x40u); { const std::uint32_t tg = G[2]; G[7] = G[16]; call_here(rt, ctx, tg); } G[4] = word(rt, G[17] + 0x134u); } } { const bool tk = G[4] == 0u; G[6] = G[20]; if (!tk) { G[2] = word(rt, G[4] + 0x0u); G[7] = G[21]; G[3] = word(rt, G[2] + 0x14u); G[8] = G[16]; { const std::uint32_t tg = G[3]; G[5] = G[18]; call_here(rt, ctx, tg); } } } G[4] = word(rt, G[17] + 0x5CCu); { const bool tk = G[4] == 0u; if (!tk) { G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x18u); { const std::uint32_t tg = G[2]; G[5] = G[18]; call_here(rt, ctx, tg); } } } G[4] = G[19]; call_here(rt, ctx, 0x088E85E8u); { const bool tk = G[2] == 0u; G[4] = word(rt, G[17] + 0x148u); if (!tk) { G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x38u); { const std::uint32_t tg = G[2]; G[5] = G[18]; call_here(rt, ctx, tg); } G[4] = word(rt, G[17] + 0x148u); } } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x28u); { const std::uint32_t tg = G[2]; G[5] = G[18]; call_here(rt, ctx, tg); } G[4] = word(rt, G[17] + 0x14Cu); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[5] = G[18]; call_here(rt, ctx, tg); } G[4] = G[19]; call_here(rt, ctx, 0x088E82ACu); G[7] = word(rt, G[17] + 0x11Cu); { const bool tk = G[7] == 0u; G[4] = G[18]; if (!tk) { G[2] = word(rt, G[19] + 0x0u); G[6] = word(rt, G[17] + 0x128u); put(rt, frame.at(0x0u), G[2]); G[5] = frame.at(0u); G[3] = word(rt, G[19] + 0x1Cu); put(rt, frame.at(0x8u), G[6]); put(rt, frame.at(0x4u), G[3]); G[2] = word(rt, G[19] + 0xCu); { const std::uint32_t tg = G[7]; put(rt, frame.at(0xCu), G[2]); call_here(rt, ctx, tg); } } } G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x088DB3CCu); G[4] = word(rt, G[17] + 0x640u); { const bool tk = G[4] == 0u; G[5] = 0u; if (!tk) { G[2] = word(rt, G[19] + 0x1Cu); { const bool tk = G[2] != 0u; if (!tk) { G[5] = G[18]; call_here(rt, ctx, 0x088E46BCu); G[5] = 0u; } } } } G[4] = G[19]; call_here(rt, ctx, 0x088E8A84u); G[4] = G[19]; call_here(rt, ctx, 0x088E7F1Cu); G[4] = word(rt, G[17] + 0x130u); G[5] = G[18]; G[6] = 0u; G[7] = 0xFFFFFFFFu; call_here(rt, ctx, 0x088DC180u); G[16] = 0x1u; G[4] = G[17]; } } G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x088D9BBCu); } } G[2] = G[16];  G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_315
} // namespace gt::game

#define HOT315(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_315::name, gt::ReturnKind::kind)
HOT315(0x08A4C2ACu, func_08A4C2AC, Word);
// HOT315(0x08821ADCu, func_08821ADC, Word);  // no comparable synthetic case yet
HOT315(0x08876188u, func_08876188, Word);
HOT315(0x08A5CD48u, func_08A5CD48, Word);
HOT315(0x08AA0690u, func_08AA0690, Word);
HOT315(0x089354CCu, func_089354CC, Word);
// HOT315(0x088619A8u, func_088619A8, Word);  // no comparable synthetic case yet
// HOT315(0x08A52D3Cu, func_08A52D3C, Word);  // no comparable synthetic case yet
// HOT315(0x08A8EC08u, func_08A8EC08, Word);  // no comparable synthetic case yet
// HOT315(0x08A2B6F4u, func_08A2B6F4, Word);  // no comparable synthetic case yet
// HOT315(0x08A8ED98u, func_08A8ED98, Word);  // no comparable synthetic case yet
HOT315(0x08AAF9C8u, func_08AAF9C8, Word);
// HOT315(0x089AE074u, func_089AE074, Word);  // no comparable synthetic case yet
// HOT315(0x089AE21Cu, func_089AE21C, Word);  // no comparable synthetic case yet
HOT315(0x08C77B74u, func_08C77B74, Word);
// HOT315(0x089AE3C4u, func_089AE3C4, Word);  // no comparable synthetic case yet
HOT315(0x08A2C594u, func_08A2C594, Word);
HOT315(0x08A0CDA4u, func_08A0CDA4, Word);
HOT315(0x08AA0AA4u, func_08AA0AA4, Word);
HOT315(0x08A9D0D4u, func_08A9D0D4, Word);
HOT315(0x089C7CD4u, func_089C7CD4, Word);
// HOT315(0x089CA288u, func_089CA288, Word);  // no comparable synthetic case yet
HOT315(0x088089D4u, func_088089D4, Word);
// HOT315(0x089C9F24u, func_089C9F24, Word);  // no comparable synthetic case yet
HOT315(0x088DE3F0u, func_088DE3F0, Word);
