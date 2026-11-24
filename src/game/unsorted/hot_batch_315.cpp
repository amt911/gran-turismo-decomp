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

void func_08A4C2AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[19] = G[4]; G[17] = G[6]; { const bool tk = static_cast<std::int32_t>(G[5]) <= 0; if (!tk) { G[16] = word(rt, G[6] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CB0000u; G[4] = 0x08CAED38u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[2] = word(rt, G[3] + 0xDCu); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[18] = G[2]; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAED38u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xD8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[4] = G[2] << 1; G[4] = G[4] + G[2]; G[4] = G[4] >> 1; G[16] = G[2]; G[3] = G[4] < 0x80u ? 1u : 0u; G[2] = 0x80u; if (G[3] != 0u) G[4] = G[2]; call_here(rt, ctx, 0x08B2B5E0u); G[5] = G[2]; G[6] = G[18]; G[7] = G[16]; G[4] = frame.at(0x2u); G[17] = G[2]; call_here(rt, ctx, 0x08C1C8F0u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[2] = G[17] + G[2]; if (!tk) { G[16] = frame.at(0x4u); G[4] = G[16]; G[5] = G[17]; G[18] = frame.at(0x8u); store8(G[2] + 0x0u, 0u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[19] == G[18]; G[5] = word(rt, frame.at(0x8u)); if (!tk) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[2] = 0x21u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x21u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x21u; } } } } store8(G[17] + 0x0u, G[2]); store8(G[17] + 0x1u, 0u); } } G[4] = G[17]; call_here(rt, ctx, 0x08B35254u); } }  G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x08821ADC (a0, a1 = two objects; v0 = 1 when a0's string sorts before a1's): both arguments are copied into counted
// strings on the frame (0x08A6C410(sp + 0xC, a0) and 0x08A6C410(sp + 4, a1), each handle checked, 0x08A5BB7C(0x08C99698)
// when null) and turned into strings at sp + 0x10 and sp + 8 by their virtual +0xC(string slot, handle). The two counted
// strings are compared: 0x08B36170(a's data, b's data, shorter count) decides a common prefix (its result replaces the
// length difference when non-zero) and the sign of that difference is the answer. The frame strings are released unless
// one is the empty header 0x08D74FD0 (its count 0x08B60804(data - 4, -1) drops and 0x08B8542C(header, sp) frees it), and
// both handles are released with 0x08A5B9EC.
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
void func_08A5CD48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xC0u); put(rt, frame.at(0xA0u), G[16]); put(rt, frame.at(0xB8u), G[22]); put(rt, frame.at(0xACu), G[19]); put(rt, frame.at(0xBCu), G[31]); put(rt, frame.at(0xB4u), G[21]); put(rt, frame.at(0xB0u), G[20]); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); G[5] = 0x08C90000u; G[16] = frame.at(0x4u); G[5] = 0x08C8EA6Cu; G[6] = frame.at(0x1u); G[22] = G[4]; G[19] = frame.at(0x8u); G[4] = G[16]; G[21] = G[9]; G[20] = frame.at(0xCu); G[18] = frame.at(0x10u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08A6B5E0u); G[3] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[3] + 0xFFFFFFF4u; G[5] = 0xFFFFFFFFu; { const bool tk = G[16] == G[2]; G[4] = G[3] + 0xFFFFFFFCu; if (!tk) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[20]; G[5] = G[19]; call_here(rt, ctx, 0x08A690E4u); G[4] = G[18]; G[5] = G[20]; call_here(rt, ctx, 0x08A76764u); call_here(rt, ctx, 0x08A5CB28u); G[16] = word(rt, G[2] + 0x0u); G[6] = 0x80u; G[8] = 0u; { const bool tk = G[16] == 0u; G[17] = frame.at(0x1Cu); if (!tk) { G[2] = word(rt, G[16] + 0x14u); G[4] = G[17]; { const bool tk = G[2] != 0u; G[5] = G[2]; if (!tk) { G[2] = 0x08CB0000u; G[5] = 0x08CAFFC4u; } } call_here(rt, ctx, 0x08B7C830u); store8(frame.at(0x9Bu), 0u); G[8] = word(rt, G[16] + 0x18u); } } G[6] = word(rt, G[21] + 0x0u); G[5] = 0x08CB0000u; G[5] = 0x08CAFFD0u; G[4] = G[18]; G[7] = G[17]; call_here(rt, ctx, 0x08A767ECu); G[4] = G[16]; { const bool tk = G[16] == 0u; G[5] = G[18]; if (!tk) { call_here(rt, ctx, 0x08A62908u); } } G[5] = word(rt, frame.at(0x8u)); G[4] = G[22]; G[5] = G[5] + 0x1Cu; call_here(rt, ctx, 0x08B88F44u); G[4] = frame.at(0x18u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[22];  G[16] = word(rt, frame.at(0xA0u)); G[22] = word(rt, frame.at(0xB8u)); G[19] = word(rt, frame.at(0xACu)); G[21] = word(rt, frame.at(0xB4u)); G[20] = word(rt, frame.at(0xB0u)); G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); }  } }
void func_08AA0690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x28u), G[18]); G[16] = frame.at(0x8u); G[22] = G[6]; G[6] = frame.at(0x1u); G[21] = G[4]; G[4] = G[16]; G[20] = G[7]; G[19] = G[5]; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[16]; call_here(rt, ctx, 0x08A69530u); G[4] = word(rt, frame.at(0x8u)); G[17] = G[2]; G[2] = 0x08D70000u; G[16] = G[4] + 0xFFFFFFF4u; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } } { const bool tk = G[17] == 0u; G[16] = 0u; if (!tk) { G[23] = frame.at(0xCu); G[4] = G[23]; call_here(rt, ctx, 0x08A9DD14u); G[18] = frame.at(0x10u); G[5] = 0x08CF0000u; G[5] = 0x08CF6ED8u; G[4] = G[18]; call_here(rt, ctx, 0x08A76AB4u); G[16] = word(rt, frame.at(0xCu)); { const bool tk = G[16] != 0u; G[4] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB277Cu; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08A9BD30u); G[17] = word(rt, G[2] + 0x0u); { const bool tk = G[17] != 0u; if (tk) { G[16] = frame.at(0x4u); } else { G[4] = 0x08CB0000u; G[4] = 0x08CB2784u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = frame.at(0x4u); } } G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[6] = G[16]; G[4] = G[17]; G[5] = G[18]; call_here(rt, ctx, 0x08A7BFD0u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[4] = word(rt, frame.at(0x10u)); { const bool tk = G[4] == 0u; G[16] = 0u; if (!tk) { G[2] = word(rt, G[4] + 0x0u); G[5] = G[21]; G[6] = G[22]; G[3] = word(rt, G[2] + 0xCCu); G[7] = G[20]; { const std::uint32_t tg = G[3]; G[16] = 0x1u; call_here(rt, ctx, tg); } } } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); } } G[2] = G[16];  G[16] = word(rt, frame.at(0x20u)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[23] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x28u)); }  } }
// 0x089354CC (a0 = the object, a1-a3 = arguments; v0 = 1 when the copies ran): 0x089363FC(a0, a1) gives a handle,
// 0x08936584(handle, a1, a2, a3, sp) reads 0x28 bytes into the frame, and when it answers non-zero they are copied onto the
// object in the caller's field order: sp + 2 (16-bit) to +0x120, +0x14 to +0x122, +0xE/+0xF/+0x10/+0x11/+0x12/+0x13 to
// +0x148/+0x14A/+0x14C/+0x149/+0x14B/+0x14D, +0xC/+0xD to +0x129/+0x12A, +0x19/+0x1A/+0x1B to +0x13E/+0x13F/+0x140, +0x1C..+0x1F
// to +0x141..+0x144; then +0x15/+0x16/+0x17/+0x18 to +0x131/+0x132/+0x138/+0x139, +0x21/+0x22 to +0x146/+0x147, +4/+6 (16-bit)
// to +0x134/+0x136, +8/+0xA (16-bit reads kept to one byte) to +0x13A/+0x13B, +0x24/+0x23 to +0x14E/+0x14F and +0x20 to +0x145.
// 0x089324B4(object) follows and, when it answers non-zero, sp + 0 (16-bit) goes to +0x156; the answer is 1 either way.
void func_089354CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x44u}, {20u, 0x40u}, {17u, 0x34u}, {16u, 0x30u}, {19u, 0x3Cu},
                                          {18u, 0x38u}});
    ctx.gpr[20] = object;
    ctx.gpr[17] = third;
    ctx.gpr[16] = second;
    const u32 handle = call_set(rt, ctx, 0x089363FCu, {object, first});
    u32 done = 0u;
    if (call_set(rt, ctx, 0x08936584u, {handle, first, second, third, frame.at(0u)}) != 0u) {
        const u32 byte_14 = load8(frame.at(0x14u));
        const u32 byte_0e = load8(frame.at(0xEu));
        const u32 byte_0f = load8(frame.at(0xFu));
        const u32 byte_10 = load8(frame.at(0x10u));
        const u32 byte_11 = load8(frame.at(0x11u));
        const u32 byte_12 = load8(frame.at(0x12u));
        const u32 byte_13 = load8(frame.at(0x13u));
        const u32 byte_0c = load8(frame.at(0xCu));
        const u32 byte_0d = load8(frame.at(0xDu));
        const u32 byte_19 = load8(frame.at(0x19u));
        const u32 byte_1a = load8(frame.at(0x1Au));
        const u32 byte_1b = load8(frame.at(0x1Bu));
        const u32 half_02 = load16(frame.at(0x2u));
        const u32 byte_1c = load8(frame.at(0x1Cu));
        const u32 byte_1d = load8(frame.at(0x1Du));
        const u32 byte_1e = load8(frame.at(0x1Eu));
        const u32 byte_1f = load8(frame.at(0x1Fu));
        store16(object + 0x120u, half_02);
        store8(object + 0x122u, byte_14);
        store8(object + 0x148u, byte_0e);
        store8(object + 0x14Au, byte_0f);
        store8(object + 0x14Cu, byte_10);
        store8(object + 0x149u, byte_11);
        store8(object + 0x14Bu, byte_12);
        store8(object + 0x14Du, byte_13);
        store8(object + 0x129u, byte_0c);
        store8(object + 0x12Au, byte_0d);
        store8(object + 0x13Eu, byte_19);
        store8(object + 0x13Fu, byte_1a);
        store8(object + 0x140u, byte_1b);
        store8(object + 0x141u, byte_1c);
        store8(object + 0x142u, byte_1d);
        store8(object + 0x143u, byte_1e);
        store8(object + 0x144u, byte_1f);
        const u32 byte_15 = load8(frame.at(0x15u));
        const u32 byte_16 = load8(frame.at(0x16u));
        const u32 byte_17 = load8(frame.at(0x17u));
        const u32 byte_18 = load8(frame.at(0x18u));
        const u32 byte_21 = load8(frame.at(0x21u));
        const u32 byte_22 = load8(frame.at(0x22u));
        const u32 half_04 = load16(frame.at(0x4u));
        const u32 half_06 = load16(frame.at(0x6u));
        const u32 half_08 = load16(frame.at(0x8u));
        const u32 half_0a = load16(frame.at(0xAu));
        const u32 byte_24 = load8(frame.at(0x24u));
        const u32 byte_23 = load8(frame.at(0x23u));
        const u32 byte_20 = load8(frame.at(0x20u));
        store8(object + 0x131u, byte_15);
        store8(object + 0x145u, byte_20);
        store8(object + 0x132u, byte_16);
        store8(object + 0x138u, byte_17);
        store8(object + 0x139u, byte_18);
        store8(object + 0x146u, byte_21);
        store8(object + 0x147u, byte_22);
        store16(object + 0x134u, half_04);
        store16(object + 0x136u, half_06);
        store8(object + 0x13Au, half_08); // the 16-bit reads at +8 and +0xA land in one byte each
        store8(object + 0x13Bu, half_0a);
        store8(object + 0x14Eu, byte_24);
        store8(object + 0x14Fu, byte_23);
        const u32 accepted = call_set(rt, ctx, 0x089324B4u, {object});
        done = 1u;
        if (accepted != 0u) store16(object + 0x156u, load16(frame.at(0u)));
    }
    ctx.gpr[2] = done;
}
// 0x088619A8 (a0 = the object, a1 = a handle, a2/a3/t0 = arguments; v0 = the 0x088DFA68 result): word(object) + 0x3F88 gets
// (a2) through 0x08894098 and word(object) + 0x3F90 gets (a3) through 0x08893C40; a frame block is started (0x088D441C(sp)) and,
// with a1 set, filled from it (0x088D44BC(sp, a1)). 0x08B0979C(sp) and 0x08B0980C give a lookup whose 0x08B08A0C(.., 0) result,
// when above zero, is saved at sp + 0x7C from a second call, and its 0x08B08A58 byte goes to sp + 0x80; 0x088618E8(word(object), 0)
// runs, with a1 set word(word(object) + 0x5AC) + 4 goes to 0x088D4824 and 0x088DFA68(word(word(object) + 0x5AC), sp, 1) gives the
// result. When that is non-zero word(object) + 0x5A9 becomes 1, word(word(object) + 0x5B8) gets the object at +0x30 and the
// addresses 0x08860B3C, 0x0885F550, 0x08861FD0 and 0x08860C2C at +0x2C, +0x24, +0x20 and +0x28, and 0x0885E7DC(object) runs.
// 0x088D435C(sp) closes the frame block.
void func_088619A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xB0u); put(rt, frame.at(0xA0u), G[31]); put(rt, frame.at(0x9Cu), G[19]); put(rt, frame.at(0x98u), G[18]); put(rt, frame.at(0x94u), G[17]); put(rt, frame.at(0x90u), G[16]); G[19] = G[5]; G[5] = G[6]; G[18] = G[4]; G[17] = G[8]; G[16] = G[7]; G[4] = word(rt, G[4] + 0x0u); G[4] = G[4] + 0x3F88u; call_here(rt, ctx, 0x08894098u); G[4] = word(rt, G[18] + 0x0u); G[5] = G[16]; G[4] = G[4] + 0x3F90u; call_here(rt, ctx, 0x08893C40u); G[4] = frame.at(0u); call_here(rt, ctx, 0x088D441Cu); G[5] = G[17]; { const bool tk = G[17] == 0u; G[4] = frame.at(0u); if (!tk) { call_here(rt, ctx, 0x088D44BCu); } } call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B0980Cu); G[4] = G[2]; G[5] = 0u; G[16] = G[2]; call_here(rt, ctx, 0x08B08A0Cu); G[5] = 0u; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[16]; if (!tk) { call_here(rt, ctx, 0x08B08A0Cu); put(rt, frame.at(0x7Cu), G[2]); } } G[4] = G[16]; call_here(rt, ctx, 0x08B08A58u); G[4] = word(rt, G[18] + 0x0u); G[5] = 0u; store8(frame.at(0x80u), G[2]); call_here(rt, ctx, 0x088618E8u); { const bool tk = G[19] == 0u; G[5] = G[19]; if (!tk) { G[2] = word(rt, G[18] + 0x0u); G[4] = word(rt, G[2] + 0x5ACu); G[4] = G[4] + 0x4u; call_here(rt, ctx, 0x088D4824u); } } G[2] = word(rt, G[18] + 0x0u); G[5] = frame.at(0u); G[6] = 0x1u; G[4] = word(rt, G[2] + 0x5ACu); call_here(rt, ctx, 0x088DFA68u); G[16] = G[2]; { const bool tk = G[2] == 0u; G[4] = G[18]; if (!tk) { G[5] = word(rt, G[18] + 0x0u); G[2] = 0x1u; store8(G[5] + 0x5A9u, G[2]); G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[3] + 0x5B8u); put(rt, G[2] + 0x30u, G[18]); G[2] = 0x08860000u; G[2] = 0x08860B3Cu; G[3] = word(rt, G[18] + 0x0u); G[5] = word(rt, G[3] + 0x5B8u); put(rt, G[5] + 0x2Cu, G[2]); G[2] = 0x08860000u; G[2] = 0x0885F550u; G[3] = word(rt, G[18] + 0x0u); G[6] = word(rt, G[3] + 0x5B8u); G[3] = 0x08860000u; G[3] = 0x08861FD0u; put(rt, G[6] + 0x24u, G[2]); G[2] = 0x08860000u; G[2] = 0x08860C2Cu; put(rt, G[6] + 0x20u, G[3]); G[5] = word(rt, G[18] + 0x0u); G[3] = word(rt, G[5] + 0x5B8u); put(rt, G[3] + 0x28u, G[2]); call_here(rt, ctx, 0x0885E7DCu); } } G[4] = frame.at(0u); call_here(rt, ctx, 0x088D435Cu); G[2] = G[16];  G[19] = word(rt, frame.at(0x9Cu)); G[18] = word(rt, frame.at(0x98u)); G[17] = word(rt, frame.at(0x94u)); G[16] = word(rt, frame.at(0x90u)); }  } }
void func_08A52D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x20u), G[16]); G[19] = frame.at(0x14u); G[21] = G[4]; G[4] = G[19]; G[17] = G[6]; put(rt, frame.at(0x10u), G[5]); G[5] = frame.at(0x10u); G[23] = G[7]; G[22] = G[8]; G[20] = frame.at(0x8u); G[18] = frame.at(0xCu); call_here(rt, ctx, 0x08A52AA8u); G[4] = 0x44u; { const bool tk = static_cast<std::int32_t>(G[17]) < 0; G[5] = 0x4u; if (!tk) { call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[19]; G[6] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[4] = G[18]; G[5] = frame.at(0x4u); put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08A52AB0u); { const bool tk = G[19] == G[18]; G[4] = G[19]; if (!tk) { G[5] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } } G[16] = word(rt, frame.at(0x14u)); G[4] = 0x08CB0000u; { const bool tk = G[16] != 0u; G[4] = 0x08CAF4A8u; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[2] = word(rt, G[3] + 0x15Cu); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x14u)); G[4] = 0x08CB0000u; { const bool tk = G[16] != 0u; G[4] = 0x08CAF4A8u; if (!tk) { call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x120u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[5] = 0x4u; G[4] = 0x18u; G[17] = G[2]; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[8] = G[22]; G[7] = G[23]; G[6] = G[17]; G[5] = 0u; G[16] = G[2]; call_here(rt, ctx, 0x08A56018u); G[4] = G[20]; G[5] = frame.at(0u); put(rt, frame.at(0x0u), G[16]); call_here(rt, ctx, 0x08A5BAB8u); G[5] = G[19]; G[6] = G[20]; G[4] = G[21]; G[7] = 0u; call_here(rt, ctx, 0x08A57B54u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[21];  G[19] = word(rt, frame.at(0x2Cu)); G[21] = word(rt, frame.at(0x34u)); G[17] = word(rt, frame.at(0x24u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08A8EC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); G[2] = 0x08CD0000u; G[2] = 0x08CCD480u; G[18] = 0x08CF0000u; G[16] = frame.at(0x8u); G[5] = 0x08CF6ED8u; G[19] = G[4] + 0x20u; G[17] = G[4]; put(rt, G[4] + 0x0u, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AAE204u); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (!tk) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; G[16] = frame.at(0x4u); call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08CF6ED8u; G[4] = G[16]; G[18] = G[17] + 0x24u; call_here(rt, ctx, 0x08AAE204u); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x74u); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[17] + 0x6Cu; call_here(rt, ctx, 0x08C2AB00u); G[4] = G[17] + 0x64u; call_here(rt, ctx, 0x08C2AB4Cu); G[4] = G[17] + 0x5Cu; call_here(rt, ctx, 0x08C2AB4Cu); G[16] = G[17] + 0x44u; G[4] = G[17] + 0x54u; call_here(rt, ctx, 0x08C2AB98u); G[4] = word(rt, G[17] + 0x44u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08C2AA30u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x44u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0x3Cu; call_here(rt, ctx, 0x08C2B458u); G[4] = G[17] + 0x34u; call_here(rt, ctx, 0x08C2B458u); G[4] = G[17] + 0x30u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x28u; call_here(rt, ctx, 0x08C2AF5Cu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = G[17] + 0x14u; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x14u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08C2AD44u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x14u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); }  } }
// 0x08A2B6F4 (a0 = the object; v0 = the last call's): 0x08B32138(0x08D69170, sp + 0x1C) gates everything. Then a text is started
// at sp + 0x18 (0x08A6BA44) and 0x08A6BBE8 gives three strings from it (0x08CAC004, 0x08CAC00C, 0x08CAC018): each goes through a
// temporary on the frame (0x08A85B1C at sp + 0x14 with sp + 0x1C, 0x08B85EB8 at sp + 0xC with sp + 0x20 and sp + 1 then
// 0x08A6D2D4 at sp + 0x10, and 0x08A950D0 at sp + 8 with sp + 0x28, 6 and 0x10), a temporary that got its own allocation is
// released (0x08A5BAD8 when the pointer is not the frame slot itself), the slot is released with 0x08A5B9EC, and the string at
// sp + 0xC drops through its count 0x08B60804(data - 4, -1) with 0x08B8542C(header, sp) when it reaches none. 0x08A76ABC(sp + 4,
// sp + 0x18) closes the text (with 0x08A5BAD8(object, ...) when the object had its own), and both the text at sp + 4 and the one at
// sp + 0x18 are released with 0x08A5B9EC. The empty counted header is 0x08D74FD0.
void func_08A2B6F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); G[20] = G[4]; G[4] = 0x08D70000u; G[4] = 0x08D69170u; G[5] = frame.at(0x1Cu); call_here(rt, ctx, 0x08B32138u); { const bool tk = G[2] == 0u; G[31] = word(rt, frame.at(0x44u)); if (!tk) { G[19] = frame.at(0x18u); G[4] = G[19]; call_here(rt, ctx, 0x08A6BA44u); G[5] = 0x08CB0000u; G[4] = G[19]; G[5] = 0x08CAC004u; call_here(rt, ctx, 0x08A6BBE8u); G[5] = word(rt, frame.at(0x1Cu)); G[16] = frame.at(0x14u); G[17] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); { const bool tk = G[17] == G[16]; G[5] = word(rt, frame.at(0x14u)); if (!tk) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08CB0000u; G[4] = G[19]; G[5] = 0x08CAC00Cu; G[16] = frame.at(0xCu); call_here(rt, ctx, 0x08A6BBE8u); G[17] = frame.at(0x10u); G[4] = G[16]; G[5] = frame.at(0x20u); G[6] = frame.at(0x1u); G[18] = G[2]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[18] == G[17]; G[5] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[5] = 0x08CB0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = 0x08CB0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[5] = 0x08CB0000u; } } } } G[5] = 0x08CAC018u; G[4] = G[19]; call_here(rt, ctx, 0x08A6BBE8u); G[16] = frame.at(0x8u); G[17] = G[2]; G[4] = G[16]; G[5] = frame.at(0x28u); G[6] = 0x6u; G[7] = 0x10u; call_here(rt, ctx, 0x08A950D0u); { const bool tk = G[17] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (!tk) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; G[16] = frame.at(0x4u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x08A76ABCu); { const bool tk = G[20] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } }  G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
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
// One of the type's destructors (0x089AE074 and 0x089AE21C are the same code; 0x089AE3C4 adds the object's own free): the
// vtable word 0x08CBB598 goes back to +0 and the members go: 0x08B35254(field at +0xB0), 0x08B35254(+0x58), 0x08BF8B64(+0x178),
// the vector at +0x12C torn down by 0x08BF919C(begin, next, sp + 3) with its buffer freed by 0x08A5BC54(begin, ((end - begin) >>
// 2) << 2, 4), 0x08A5B9EC on +0xB8 and +0x68, 0x08A5B930 on each pointer at +0x54, +0x50, +0x4C, +0x48, +0x44 and +0x40, the
// vectors at +0x30, +0x24 and +0x18 the same way as +0x12C but through 0x08BF8B48 (sp + 2, sp + 1, sp), and 0x08A780B0(object).
void func_089AE074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC0000u; G[2] = 0x08CBB598u; G[17] = G[4]; G[16] = G[17] + 0x12Cu; G[4] = word(rt, G[4] + 0xB0u); put(rt, G[17] + 0x0u, G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[17] + 0x58u); call_here(rt, ctx, 0x08B35254u); G[4] = G[17] + 0x178u; call_here(rt, ctx, 0x08BF8B64u); G[4] = word(rt, G[17] + 0x12Cu); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BF919Cu); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x12Cu); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0xB8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x68u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x54u); { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x50u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x50u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x4Cu); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x4Cu); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x48u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x48u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x44u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x44u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x40u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x40u); } } { const bool tk = G[4] == 0u; G[16] = G[17] + 0x30u; if (!tk) { call_here(rt, ctx, 0x08A5B930u); } } G[4] = word(rt, G[17] + 0x30u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x30u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x24u; G[4] = word(rt, G[17] + 0x24u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x24u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x18u; G[4] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x18u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u);  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x089AE21C (a0 = the object; v0 = the closing 0x08A780B0's): the same destructor as 0x089AE074, byte for byte: class word
// 0x08CBB598 to word(object), 0x08B35254(object + 0xB0) and (object + 0x58), 0x08BF8B64(object + 0x178), the container at
// object + 0x12C emptied (0x08BF919C(begin, end, sp + 3) and 0x08A5BC54(begin, size, 4)), 0x08A5B9EC(object + 0xB8) and
// (object + 0x68), the handles at +0x54, +0x50, +0x4C, +0x48, +0x44 and +0x40 released with 0x08A5B930 when set, the
// containers at +0x30, +0x24 and +0x18 emptied with 0x08BF8B48 (sp + 2, sp + 1, sp), and 0x08A780B0(object).
void func_089AE21C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC0000u; G[2] = 0x08CBB598u; G[17] = G[4]; G[16] = G[17] + 0x12Cu; G[4] = word(rt, G[4] + 0xB0u); put(rt, G[17] + 0x0u, G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[17] + 0x58u); call_here(rt, ctx, 0x08B35254u); G[4] = G[17] + 0x178u; call_here(rt, ctx, 0x08BF8B64u); G[4] = word(rt, G[17] + 0x12Cu); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BF919Cu); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x12Cu); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0xB8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x68u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x54u); { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x50u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x50u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x4Cu); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x4Cu); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x48u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x48u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x44u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x44u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x40u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x40u); } } { const bool tk = G[4] == 0u; G[16] = G[17] + 0x30u; if (!tk) { call_here(rt, ctx, 0x08A5B930u); } } G[4] = word(rt, G[17] + 0x30u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x30u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x24u; G[4] = word(rt, G[17] + 0x24u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x24u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x18u; G[4] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x18u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u);  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08C77B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x90u); put(rt, frame.at(0x80u), G[20]); put(rt, frame.at(0x7Cu), G[19]); put(rt, frame.at(0x78u), G[18]); put(rt, frame.at(0x74u), G[17]); put(rt, frame.at(0x70u), G[16]); put(rt, frame.at(0x84u), G[31]); G[2] = 0xFFFFFFFFu; G[20] = G[4]; G[3] = 0xFFFFFFFFu; G[19] = G[5]; G[18] = G[8]; G[17] = G[7]; G[16] = G[6]; G[15] = word(rt, G[9] + 0x14u); G[10] = word(rt, G[9] + 0x0u); G[11] = word(rt, G[9] + 0x4u); G[12] = word(rt, G[9] + 0x8u); G[13] = word(rt, G[9] + 0xCu); G[14] = word(rt, G[9] + 0x10u); put(rt, G[4] + 0x0u, G[2]); put(rt, G[20] + 0x8u, 0u); put(rt, G[20] + 0xCu, 0u); put(rt, G[20] + 0x10u, 0u); put(rt, G[20] + 0x14u, 0u); put(rt, G[20] + 0x18u, 0u); put(rt, G[20] + 0x1Cu, 0u); put(rt, G[4] + 0x4u, G[3]); G[4] = G[5]; put(rt, frame.at(0x18u), G[10]); put(rt, frame.at(0x1Cu), G[11]); put(rt, frame.at(0x20u), G[12]); put(rt, frame.at(0x24u), G[13]); put(rt, frame.at(0x28u), G[14]); put(rt, frame.at(0x2Cu), G[15]); call_here(rt, ctx, 0x08C76B7Cu); G[6] = G[16]; G[7] = G[17]; G[8] = G[18]; { const bool tk = G[2] == 0u; G[4] = G[19] + 0x24u; if (!tk) { call_here(rt, ctx, 0x08B618ACu); put(rt, frame.at(0x48u), G[2]); put(rt, frame.at(0x4Cu), G[3]); G[10] = word(rt, frame.at(0x2Cu)); G[5] = word(rt, frame.at(0x28u)); G[6] = word(rt, frame.at(0x18u)); G[7] = word(rt, frame.at(0x1Cu)); G[8] = word(rt, frame.at(0x20u)); G[9] = word(rt, frame.at(0x24u)); G[3] = word(rt, frame.at(0x4Cu)); G[2] = word(rt, frame.at(0x48u)); G[4] = word(rt, G[19] + 0x78u); G[11] = word(rt, G[19] + 0x94u); put(rt, G[20] + 0x0u, G[2]); put(rt, G[20] + 0x4u, G[3]); put(rt, G[19] + 0x9Cu, G[11]); put(rt, frame.at(0x50u), 0u); put(rt, frame.at(0x54u), 0u); put(rt, frame.at(0x58u), 0u); put(rt, frame.at(0x5Cu), 0u); put(rt, frame.at(0x60u), 0u); put(rt, frame.at(0x64u), 0u); put(rt, G[19] + 0xCu, G[4]); store8(G[19] + 0x81u, 0u); store8(G[19] + 0x82u, 0u); put(rt, G[19] + 0xA0u, G[11]); put(rt, G[19] + 0x4u, G[4]); put(rt, G[19] + 0x8u, G[4]); put(rt, G[19] + 0x14u, 0u); put(rt, G[19] + 0x10u, 0u); put(rt, G[19] + 0x18u, 0u); put(rt, G[19] + 0x48u, G[6]); put(rt, G[19] + 0x4Cu, G[7]); put(rt, G[19] + 0x50u, G[8]); put(rt, G[19] + 0x54u, G[9]); put(rt, G[19] + 0x58u, G[5]); put(rt, G[19] + 0x5Cu, G[10]); put(rt, G[20] + 0x8u, G[6]); put(rt, G[20] + 0xCu, G[7]); put(rt, G[20] + 0x10u, G[8]); put(rt, G[20] + 0x14u, G[9]); put(rt, G[20] + 0x18u, G[5]); put(rt, G[20] + 0x1Cu, G[10]); put(rt, frame.at(0x30u), G[6]); put(rt, frame.at(0x34u), G[7]); put(rt, frame.at(0x38u), G[8]); put(rt, frame.at(0x3Cu), G[9]); put(rt, frame.at(0x40u), G[5]); put(rt, frame.at(0x44u), G[10]); put(rt, frame.at(0x0u), G[6]); put(rt, frame.at(0x4u), G[7]); put(rt, frame.at(0x8u), G[8]); put(rt, frame.at(0xCu), G[9]); put(rt, frame.at(0x10u), G[5]); put(rt, frame.at(0x14u), G[10]); } } G[2] = G[20];  G[20] = word(rt, frame.at(0x80u)); G[19] = word(rt, frame.at(0x7Cu)); G[18] = word(rt, frame.at(0x78u)); G[17] = word(rt, frame.at(0x74u)); G[16] = word(rt, frame.at(0x70u)); }  } }
// 0x089AE3C4 (a0 = the object; v0 = the closing 0x08A5BC54's): the destructor of 0x089AE074 plus the object's own release:
// class word 0x08CBB598 to word(object), 0x08B35254(object + 0xB0) and (object + 0x58), 0x08BF8B64(object + 0x178), the
// container at object + 0x12C emptied (0x08BF919C(begin, end, sp + 3) and 0x08A5BC54(begin, size, 4)), 0x08A5B9EC(object +
// 0xB8) and (object + 0x68), the handles at +0x54, +0x50, +0x4C, +0x48, +0x44 and +0x40 released with 0x08A5B930 when set,
// the containers at +0x30, +0x24 and +0x18 emptied with 0x08BF8B48 (sp + 2, sp + 1, sp), 0x08A780B0(object) and finally
// 0x08A5BC54(object, 0x180, 4).
void func_089AE3C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC0000u; G[2] = 0x08CBB598u; G[17] = G[4]; G[16] = G[17] + 0x12Cu; G[4] = word(rt, G[4] + 0xB0u); put(rt, G[17] + 0x0u, G[2]); call_here(rt, ctx, 0x08B35254u); G[4] = word(rt, G[17] + 0x58u); call_here(rt, ctx, 0x08B35254u); G[4] = G[17] + 0x178u; call_here(rt, ctx, 0x08BF8B64u); G[4] = word(rt, G[17] + 0x12Cu); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BF919Cu); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x12Cu); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17] + 0xB8u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17] + 0x68u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[17] + 0x54u); { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x50u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x50u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x4Cu); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x4Cu); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x48u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x48u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x44u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x44u); } } { const bool tk = G[4] == 0u; if (tk) { G[4] = word(rt, G[17] + 0x40u); } else { call_here(rt, ctx, 0x08A5B930u); G[4] = word(rt, G[17] + 0x40u); } } { const bool tk = G[4] == 0u; G[16] = G[17] + 0x30u; if (!tk) { call_here(rt, ctx, 0x08A5B930u); } } G[4] = word(rt, G[17] + 0x30u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x30u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x24u; G[4] = word(rt, G[17] + 0x24u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x24u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[16] = G[17] + 0x18u; G[4] = word(rt, G[17] + 0x18u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BF8B48u); G[2] = word(rt, G[16] + 0x8u); G[4] = word(rt, G[17] + 0x18u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (!tk) { G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); } } G[4] = G[17]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[17]; G[5] = 0x180u; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u);  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08A2C594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[16]); G[5] = 0x08CB0000u; G[5] = 0x08CAC160u; G[17] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = frame.at(0u); G[16] = G[2]; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0x4u); G[4] = G[17]; put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAC168u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08A30000u; G[4] = G[16]; G[5] = 0x08CAC170u; G[6] = 0x08A2CF30u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAC168u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08A30000u; G[4] = G[16]; G[5] = 0x08CAC180u; G[6] = 0x08A2CDFCu; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAC168u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08A30000u; G[4] = G[16]; G[5] = 0x08CAC18Cu; G[6] = 0x08A2CCC8u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAC168u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08A30000u; G[4] = G[16]; G[5] = 0x08CAC198u; G[6] = 0x08A2CB48u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAC168u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08A30000u; G[4] = G[16]; G[5] = 0x08CAC1A4u; G[6] = 0x08A2C9A4u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAC168u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08A30000u; G[4] = G[16]; G[5] = 0x08CAC1B0u; G[6] = 0x08A2C87Cu; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CAC168u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08A30000u; G[4] = G[16]; G[5] = 0x08CAC1B8u; G[6] = 0x08A2C754u; call_here(rt, ctx, 0x08B86FB0u); G[2] = G[17];  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
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
void func_08A9D0D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x50u), G[30]); put(rt, frame.at(0x48u), G[22]); put(rt, frame.at(0x4Cu), G[23]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); G[30] = G[5]; G[5] = 0x08CF0000u; G[22] = frame.at(0x14u); G[5] = 0x08CF6ED8u; G[23] = G[4]; G[4] = G[22]; G[19] = G[6]; put(rt, frame.at(0x20u), G[7]); call_here(rt, ctx, 0x08A90470u); G[2] = word(rt, G[19] + 0x0u); G[2] = word(rt, G[2] + 0xFFFFFFF4u); G[3] = static_cast<std::int32_t>(G[2]) < 4 ? 1u : 0u; { const bool tk = G[3] != 0u; G[16] = frame.at(0x8u); if (!tk) { G[6] = G[2] + 0xFFFFFFFCu; G[2] = G[2] < G[6] ? 1u : 0u; { const bool tk = G[2] == 0u; G[7] = 0x4u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2680u; call_here(rt, ctx, 0x08B5AEDCu); } } G[4] = frame.at(0x10u); G[5] = G[19]; call_here(rt, ctx, 0x08B8D96Cu); G[5] = 0x08CB0000u; G[5] = 0x08CB2698u; G[4] = frame.at(0x4u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B85EB8u); G[17] = word(rt, frame.at(0x10u)); G[16] = word(rt, frame.at(0x4u)); G[2] = frame.at(0x18u); G[7] = word(rt, G[17] + 0xFFFFFFF4u); G[8] = frame.at(0x1Cu); G[4] = G[17]; put(rt, frame.at(0x18u), G[7]); G[5] = G[16]; G[20] = G[16] + 0xFFFFFFF4u; G[3] = word(rt, G[16] + 0xFFFFFFF4u); G[6] = G[3] < G[7] ? 1u : 0u; put(rt, frame.at(0x1Cu), G[3]); if (G[6] == 0u) G[8] = G[2]; G[18] = G[7] - G[3]; G[6] = word(rt, G[8] + 0x0u); call_here(rt, ctx, 0x08B36170u); G[3] = 0x08D70000u; G[21] = 0x08D74FD0u; { const bool tk = G[20] == G[21]; if (G[2] != 0u) G[18] = G[2]; if (!tk) { G[4] = G[16] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) { G[17] = word(rt, frame.at(0x10u)); } else { G[4] = G[20]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[17] = word(rt, frame.at(0x10u)); } } } } G[16] = G[17] + 0xFFFFFFF4u; { const bool tk = G[16] == G[21]; G[4] = G[17] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } } { const bool tk = G[18] != 0u; G[16] = frame.at(0x8u); if (!tk) { G[16] = frame.at(0xCu); G[6] = G[19]; G[5] = G[30]; G[4] = G[16]; call_here(rt, ctx, 0x08A9C7B8u); G[5] = word(rt, frame.at(0xCu)); G[4] = G[22]; call_here(rt, ctx, 0x08A5BAD8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = frame.at(0x8u); } } } } G[4] = G[16]; call_here(rt, ctx, 0x08A76AA0u); G[7] = word(rt, frame.at(0x20u)); G[5] = G[30]; G[6] = G[22]; G[8] = G[16]; G[4] = G[23]; G[9] = 0u; G[10] = 0u; call_here(rt, ctx, 0x08A9C1C0u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = G[23];  G[30] = word(rt, frame.at(0x50u)); G[22] = word(rt, frame.at(0x48u)); G[23] = word(rt, frame.at(0x4Cu)); G[19] = word(rt, frame.at(0x3Cu)); G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
// 0x089C7CD4 (a0 = the object, a1 = an argument word; v0 = the closing 0x08A5B9EC's): a1's word must be non-zero
// (0x08A5BB7C(0x08CA65F4)) and is printed into a counted string on the frame (0x08A923F4(sp + 0xC, that word), then
// 0x089BB3A8(sp + 0x10, sp + 0xC)), which is stored at sp + 0x10 and the copy released (0x08A5B9EC(sp + 0xC)). When that word
// is non-zero: 0x08A7BDB0(the word) formats it again (0x08B85EB8(sp + 0x14, printed, sp + 3)) and the word's virtual +0x108(sp + 0x14)
// gets it, object + 0x40 and the word (both null-checked: 0x08A5BB7C(0x08CA6384) / (0x08CA65F4) / (0x08CA6444) for the vtable)
// feed 0x08A7BCC0(word), 0x089BAC2C(sp + 8, word) and 0x08C00A44(object + 0x40, that, sp + 8), whose string is released
// (0x08A5B9EC(sp + 8)). A last text is formatted (0x08B85EB8(sp + 4, 0x08C8CC2C, sp + 2)) and 0x089C79F0(object, 0x21, word,
// sp + 4) runs; the strings on sp + 4 and sp + 0x14 are released (unless empty header 0x08D74FD0: count 0x08B60804(data - 4, -1)
// and 0x08B8542C(header, sp + 1) / (header, sp)). The word at sp + 0x10 is released last.
void func_089C7CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x34u}, {18u, 0x28u}, {31u, 0x38u}, {20u, 0x30u},
                                          {19u, 0x2Cu}, {17u, 0x24u}, {16u, 0x20u}});
    const u32 head = frame.at(0xCu);
    const u32 word_slot = frame.at(0x10u);
    const u32 line = frame.at(0x14u);
    ctx.gpr[21] = object;
    u32 value = word(rt, argument);
    if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA65F4u});
    ctx.gpr[18] = argument;
    call_set(rt, ctx, 0x08A923F4u, {head, value});
    call_set(rt, ctx, 0x089BB3A8u, {word_slot, head});
    call_set(rt, ctx, 0x08A5B9ECu, {head});
    if (word(rt, word_slot) != 0u) {
        value = word(rt, argument);
        if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA65F4u});
        const u32 printed = call_set(rt, ctx, 0x08A7BDB0u, {value});
        call_set(rt, ctx, 0x08B85EB8u, {line, printed, frame.at(0x3u)});
        const u32 owner = word(rt, word_slot);
        if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6444u});
        call_indirect(rt, ctx, word(rt, word(rt, owner) + 0x108u), {owner, line});
        const u32 entry = word(rt, object + 0x40u);
        if (entry == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6384u});
        value = word(rt, argument);
        if (value == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA65F4u});
        const u32 serialized = call_set(rt, ctx, 0x08A7BCC0u, {value});
        const u32 output = frame.at(0x8u);
        call_set(rt, ctx, 0x089BAC2Cu, {output, word_slot});
        call_set(rt, ctx, 0x08C00A44u, {entry, serialized, output});
        call_set(rt, ctx, 0x08A5B9ECu, {output});
        const u32 keep = word(rt, word_slot);
        const u32 tail = frame.at(0x4u);
        call_set(rt, ctx, 0x08B85EB8u, {tail, 0x08C8CC2Cu, frame.at(0x2u)});
        call_set(rt, ctx, 0x089C79F0u, {object, 0x21u, keep, tail});
        const u32 tail_data = word(rt, tail);
        const u32 tail_header = tail_data - 0xCu;
        if (tail_header != 0x08D74FD0u) {
            const u32 left = call_set(rt, ctx, 0x08B60804u, {tail_data - 0x4u, 0xFFFFFFFFu});
            if (static_cast<s32>(left) <= 0) call_set(rt, ctx, 0x08B8542Cu, {tail_header, frame.at(0x1u)});
        }
        const u32 line_data = word(rt, line);
        const u32 line_header = line_data - 0xCu;
        if (line_header != 0x08D74FD0u) {
            const u32 left = call_set(rt, ctx, 0x08B60804u, {line_data - 0x4u, 0xFFFFFFFFu});
            if (static_cast<s32>(left) <= 0) call_set(rt, ctx, 0x08B8542Cu, {line_header, frame.at(0u)});
        }
    }
    call_set(rt, ctx, 0x08A5B9ECu, {word_slot});
}
// 0x089CA288 (a0 = the object, a1 = the target, a2/a3 = two arguments of a text; v0 = 1 when the second part ran): the text is
// built on the frame (0x089C8818(sp + 0xC, a2, a3)), copied (0x08A69654(sp + 4, sp + 0xC)) and put in a string on a 0x38-byte
// block (0x08A5BC20(0x38, 4), then 0x08A7E914(block, sp + 4)); a handle for it is made at sp + 0x10 (0x08A76AB4(sp + 0x10,
// block in sp + 8)). The copy is released (0x08A69D68) and 0x089C843C(object) runs; when 0x089C9CE4(sp + 0xC, target) answers
// non-zero, 0x08A7CE50(handle, target), 0x08A7CE50(object + 0x3C, handle), the target's virtual +0x12C and +0x130 (both with the
// target's word) and 0x08A7DE18(object + 0x3C, 0x08A7BCC0(handle)) run, each word behind a null check (0x08A5BB7C(0x08CA6384),
// 0x08CA6444 for the target's). Then 0x089C83A4(object), 0x08A5B9EC(sp + 0x10) and the release of the text on sp + 0xC (unless it
// is the empty header 0x08D74FD0: count 0x08B60804(data - 4, -1) then 0x08B8542C(header, sp)).
void func_089CA288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); G[16] = frame.at(0xCu); G[20] = G[4]; G[4] = G[16]; G[19] = G[5]; G[5] = G[6]; G[6] = G[7]; G[17] = frame.at(0x4u); G[21] = frame.at(0x10u); call_here(rt, ctx, 0x089C8818u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08A69654u); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0x8u); G[4] = G[21]; put(rt, frame.at(0x8u), G[18]); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[17]; call_here(rt, ctx, 0x08A69D68u); G[4] = G[20]; call_here(rt, ctx, 0x089C843Cu); G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x089C9CE4u); { const bool tk = G[2] == 0u; G[17] = 0u; if (!tk) { G[16] = word(rt, frame.at(0x10u)); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[19]; call_here(rt, ctx, 0x08A7CE50u); G[16] = word(rt, G[20] + 0x3Cu); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[21]; call_here(rt, ctx, 0x08A7CE50u); G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6444u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x12Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6444u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x130u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[17] = word(rt, G[20] + 0x3Cu); { const bool tk = G[17] != 0u; G[16] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, frame.at(0x10u)); } } { const bool tk = G[16] != 0u; G[4] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08A7BCC0u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A7DE18u); G[17] = 0x1u; } } G[4] = G[20]; call_here(rt, ctx, 0x089C83A4u); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[2] = G[17]; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = G[17]; if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[2] = G[17]; } } } }  G[16] = word(rt, frame.at(0x20u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x28u)); }  } }
// 0x088089D4 (a0 = an object; v0 = the last call's, 0x08A5B9EC's): builds a counted string on the frame (+0, empty header
// 0x08C9802C) with 0x08A69B3C, makes a 0x38-byte block (0x08A5BC20(0x38, 4)) and fills it from the string (0x08A7E914); the
// block goes to frame +4 and 0x08A76AB4(frame +8, it) gives a handle for it. Once the string is released (0x08A69D68) six
// (name, function) pairs go on the handle with 0x08B86FB0, each behind a null check of the handle reloaded from frame +8
// (0x08A5BB7C(0x08C98034) when it is null), then 0x08A7CAD4(handle, 0x08C9803C, 0x08808518()) runs and 0x08A7CE50(a0's word,
// handle) is followed by the handle's release 0x08A5B9EC(handle).
void func_088089D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {31u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}});
    const u32 handle_slot = frame.at(0x8u);
    const u32 text = frame.at(0u);
    ctx.gpr[18] = object;
    call_set(rt, ctx, 0x08A69B3Cu, {text, 0x08C9802Cu});
    ctx.gpr[16] = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 0x4u});
    const u32 block = ctx.gpr[16];
    call_set(rt, ctx, 0x08A7E914u, {block, text});
    put(rt, frame.at(0x4u), block);
    call_set(rt, ctx, 0x08A76AB4u, {handle_slot, frame.at(0x4u)});
    call_set(rt, ctx, 0x08A69D68u, {text});
    u32 handle = word(rt, handle_slot);
    if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98034u});
    // 0x08808518 reads no argument (the asm leaves a half-built address in a0 here).
    const u32 created = call_set(rt, ctx, 0x08808518u, {});
    call_set(rt, ctx, 0x08A7CAD4u, {handle, 0x08C9803Cu, created});
    constexpr u32 kMethods[6][2] = {{0x08C98048u, 0x08808E60u}, {0x08C98050u, 0x08809608u},
                                    {0x08C98064u, 0x088095C0u}, {0x08C98070u, 0x08809574u},
                                    {0x08C9807Cu, 0x08809528u}, {0x08C9808Cu, 0x088094DCu}};
    for (const auto &method : kMethods) {
        const u32 current = word(rt, handle_slot);
        if (current == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98034u});
        call_set(rt, ctx, 0x08B86FB0u, {current, method[0], method[1]});
    }
    const u32 target = word(rt, object);
    if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98034u});
    call_set(rt, ctx, 0x08A7CE50u, {target, handle_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
}
// 0x089C9F24 (a0 = the object, a1 = the target, a2 = a handle, a3 = a byte; v0 = 1 when the inner body ran): a name is
// formatted on the frame (0x08B85EB8(sp + 4, 0x08CA6680, sp + 2)) and 0x089C8818(sp + 8, a2, sp + 4) makes a counted string of
// it, released unless it is the empty header 0x08D74FD0 (count 0x08B60804(data - 4, -1) then 0x08B8542C(header, sp + 1)). After
// 0x08B2E05C() and 0x089C843C(object), 0x089C9CE4(sp + 8, target) gates the body: 0x08A7CE50(object + 0x3C, target),
// 0x089B47EC(target's word, sp + 8), then the target's virtual +0x12C(word), +0x130(word) and (with the byte set) +0x150(word,
// object), each word read behind a null check (0x08A5BB7C(0x08CA6378), 0x08CA6384 for object + 0x3C). 0x089C83A4(object) and
// 0x08B2E05C() close the body, and the string on sp + 8 is released like the one above with sp as the free slot.
void func_089C9F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); G[18] = G[5]; G[5] = 0x08CA0000u; G[16] = frame.at(0x4u); G[5] = 0x08CA6680u; G[20] = frame.at(0x8u); G[19] = G[4]; G[4] = G[16]; G[17] = G[6]; G[6] = frame.at(0x2u); G[21] = G[7] & 0xFFu; call_here(rt, ctx, 0x08B85EB8u); G[6] = G[16]; G[4] = G[20]; G[5] = G[17]; call_here(rt, ctx, 0x089C8818u); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[22] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[22]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[17] = 0u; call_here(rt, ctx, 0x08B2E05Cu); G[4] = G[19]; call_here(rt, ctx, 0x089C843Cu); G[4] = G[20]; G[5] = G[18]; call_here(rt, ctx, 0x089C9CE4u); { const bool tk = G[2] == 0u; if (!tk) { G[16] = word(rt, G[19] + 0x3Cu); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6384u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[18]; call_here(rt, ctx, 0x08A7CE50u); G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[20]; call_here(rt, ctx, 0x089B47ECu); G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x12Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[2] = word(rt, G[3] + 0x130u); { const std::uint32_t tg = G[2]; G[17] = 0x1u; call_here(rt, ctx, tg); } { const bool tk = G[21] == 0u; if (!tk) { G[16] = word(rt, G[18] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA6378u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[5] = G[19]; G[2] = word(rt, G[3] + 0x150u); { const std::uint32_t tg = G[2]; G[17] = 0x1u; call_here(rt, ctx, tg); } } } } } G[4] = G[19]; call_here(rt, ctx, 0x089C83A4u); call_here(rt, ctx, 0x08B2E05Cu); G[4] = word(rt, frame.at(0x8u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[22] == G[16]; G[2] = G[17]; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = G[17]; if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[2] = G[17]; } } } }  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); }  } }
// 0x088DE3F0 (a0 = the object, a1 = an index; v0 = 1 when the entry's body ran): 0x088DE24C(object, 0, 0) must answer non-zero
// and the entry at word(object + 0xC0) + index * 0x2F8 must have a word +4 of 1. The entry's +0x1C != 0 and +0xC go along to
// 0x088F8D58(object + 0x14C, index) (kept for the callbacks), then 0x088DA0BC(object) gates a virtual +0x40 on object + 0x148
// (index, the flag, the kept value); virtual +0x14 on object + 0x134 (index, flag, entry + 0xC, kept) and +0x18 on
// object + 0x5CC (index) follow, 0x088E85E8(entry) gates a virtual +0x38 on object + 0x148, then +0x28 (same, index) and +0xC
// on object + 0x14C (index). 0x088E82AC(entry) and, when object + 0x11C is set, that callback with (index, sp, object + 0x128)
// and the entry's four words on the stack; then 0x088DB3CC(object, index), 0x088E46BC(object + 0x640, index) only when the
// entry's +0x1C is clear, 0x088E8A84(entry), 0x088E7F1C(entry) and 0x088DC180(object + 0x130, index, 0, -1). Whether or not
// that body ran, 0x088D9BBC(object, 0, 0) follows.
void func_088DE3F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x28u}, {21u, 0x24u},
                                          {20u, 0x20u}, {19u, 0x1Cu}});
    ctx.gpr[17] = object;
    u32 ran = 0u;
    if (call_set(rt, ctx, 0x088DE24Cu, {object, 0u, 0u}) != 0u) {
        // The index's record, 760 bytes: ((index << 7) - (index << 5) - index) << 3.
        const u32 stride = ((index << 7) - (index << 5) - index) << 3;
        const u32 entry = word(rt, object + 0xC0u) + stride;
        if (word(rt, entry + 0x4u) == 1u) {
            const u32 flag = word(rt, entry + 0x1Cu) != 0u ? 1u : 0u;
            const u32 extra = word(rt, entry + 0xCu);
            const u32 kept = call_set(rt, ctx, 0x088F8D58u, {word(rt, object + 0x14Cu), index});
            if (call_set(rt, ctx, 0x088DA0BCu, {object}) != 0u) {
                const u32 handler = word(rt, object + 0x148u);
                call_indirect(rt, ctx, word(rt, word(rt, handler) + 0x40u), {handler, index, flag, kept});
            }
            const u32 first_handler = word(rt, object + 0x134u);
            if (first_handler != 0u) {
                // `jalr v1`: unlike the v0 calls below it leaves v0 alone, so no call_indirect here.
                call_set(rt, ctx, word(rt, word(rt, first_handler) + 0x14u),
                         {first_handler, index, flag, extra, kept});
            }
            const u32 second_handler = word(rt, object + 0x5CCu);
            if (second_handler != 0u) {
                call_indirect(rt, ctx, word(rt, word(rt, second_handler) + 0x18u), {second_handler, index});
            }
            if (call_set(rt, ctx, 0x088E85E8u, {entry}) != 0u) {
                const u32 handler = word(rt, object + 0x148u);
                call_indirect(rt, ctx, word(rt, word(rt, handler) + 0x38u), {handler, index});
            }
            const u32 handler = word(rt, object + 0x148u);
            call_indirect(rt, ctx, word(rt, word(rt, handler) + 0x28u), {handler, index});
            const u32 third_handler = word(rt, object + 0x14Cu);
            call_indirect(rt, ctx, word(rt, word(rt, third_handler) + 0xCu), {third_handler, index});
            call_set(rt, ctx, 0x088E82ACu, {entry});
            const u32 callback = word(rt, object + 0x11Cu);
            if (callback != 0u) {
                const u32 argument = word(rt, object + 0x128u);
                put(rt, frame.at(0x0u), word(rt, entry));
                put(rt, frame.at(0x8u), argument);
                put(rt, frame.at(0x4u), word(rt, entry + 0x1Cu));
                put(rt, frame.at(0xCu), word(rt, entry + 0xCu));
                call_indirect(rt, ctx, callback, {index, frame.at(0u), argument});
            }
            call_set(rt, ctx, 0x088DB3CCu, {object, index});
            const u32 gate = word(rt, object + 0x640u);
            if (gate != 0u && word(rt, entry + 0x1Cu) == 0u) call_set(rt, ctx, 0x088E46BCu, {gate, index});
            call_set(rt, ctx, 0x088E8A84u, {entry});
            call_set(rt, ctx, 0x088E7F1Cu, {entry});
            call_set(rt, ctx, 0x088DC180u, {word(rt, object + 0x130u), index, 0u, 0xFFFFFFFFu});
            ran = 1u;
        }
        call_set(rt, ctx, 0x088D9BBCu, {object, 0u, 0u});
    }
    ctx.gpr[2] = ran;
}

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
