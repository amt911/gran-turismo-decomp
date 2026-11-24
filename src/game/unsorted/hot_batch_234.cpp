// Hot batch 234: fifty-four- to one-hundred-fourteen-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (component registrations through a temporary handle, two-way setters, guarded table
// lookups, polling loops, record copies and list insertions).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_234 {
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
    if (!(value >= -2347483648.0f && value < 2347483648.0f)) return 0x7FFFFFFF;
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

void func_089757A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x3Cu), G[31]); G[5] = 0x08CA2D44u; put(rt, frame.at(0x38u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x30u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x34u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089D8374u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08975B80u; call_here(rt, ctx, 0x08B8850Cu); G[6] = 0x08974C20u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA2CC0u; call_here(rt, ctx, 0x08B878E8u); G[6] = 0x08974D24u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA2CCCu; call_here(rt, ctx, 0x08B878E8u); G[3] = 0x08C90000u; G[2] = 0x08C8BD84u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8BD84u); G[4] = G[18]; G[6] = frame.at(0x20u); G[5] = 0x08CA2D70u; put(rt, frame.at(0x20u), G[7]); put(rt, frame.at(0x24u), G[8]); call_here(rt, ctx, 0x08BECD5Cu); G[3] = 0x08C90000u; G[2] = 0x08C8BD7Cu; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8BD7Cu); G[4] = G[18]; G[6] = frame.at(0x18u); G[5] = 0x08CA2D80u; put(rt, frame.at(0x18u), G[7]); put(rt, frame.at(0x1Cu), G[8]); call_here(rt, ctx, 0x08BED2FCu); G[3] = 0x08C90000u; G[2] = 0x08C8BD74u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8BD74u); G[4] = G[18]; G[6] = frame.at(0x10u); G[5] = 0x08CA2D90u; put(rt, frame.at(0x10u), G[7]); put(rt, frame.at(0x14u), G[8]); call_here(rt, ctx, 0x08BECD5Cu); G[3] = 0x08C90000u; G[2] = 0x08C8BD6Cu; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8BD6Cu); G[4] = G[18]; G[6] = frame.at(8u); G[5] = 0x08CA2D9Cu; put(rt, frame.at(8u), G[7]); put(rt, frame.at(0xCu), G[8]); call_here(rt, ctx, 0x08BED2FCu); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_089A1AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x3Cu), G[31]); G[5] = 0x08CA4AE0u; put(rt, frame.at(0x38u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x30u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x34u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089D8374u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089A1EC8u; call_here(rt, ctx, 0x08B8850Cu); G[6] = 0x089A0F88u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA4A5Cu; call_here(rt, ctx, 0x08B878E8u); G[6] = 0x089A1084u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA4A68u; call_here(rt, ctx, 0x08B878E8u); G[3] = 0x08C90000u; G[2] = 0x08C8C550u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8C550u); G[4] = G[18]; G[6] = frame.at(0x20u); G[5] = 0x08CA4B0Cu; put(rt, frame.at(0x20u), G[7]); put(rt, frame.at(0x24u), G[8]); call_here(rt, ctx, 0x08BF4B20u); G[3] = 0x08C90000u; G[2] = 0x08C8C548u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8C548u); G[4] = G[18]; G[6] = frame.at(0x18u); G[5] = 0x08CA4B1Cu; put(rt, frame.at(0x18u), G[7]); put(rt, frame.at(0x1Cu), G[8]); call_here(rt, ctx, 0x08BF508Cu); G[3] = 0x08C90000u; G[2] = 0x08C8C540u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8C540u); G[4] = G[18]; G[6] = frame.at(0x10u); G[5] = 0x08CA4B2Cu; put(rt, frame.at(0x10u), G[7]); put(rt, frame.at(0x14u), G[8]); call_here(rt, ctx, 0x08BF4B20u); G[3] = 0x08C90000u; G[2] = 0x08C8C538u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8C538u); G[4] = G[18]; G[6] = frame.at(8u); G[5] = 0x08CA4B38u; put(rt, frame.at(8u), G[7]); put(rt, frame.at(0xCu), G[8]); call_here(rt, ctx, 0x08BF508Cu); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_089F8F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x3Cu), G[31]); G[5] = 0x08CA8554u; put(rt, frame.at(0x38u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x30u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x34u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089D8374u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089F97D0u; call_here(rt, ctx, 0x08B8850Cu); G[6] = 0x089F83A8u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA84CCu; call_here(rt, ctx, 0x08B878E8u); G[6] = 0x089F84C4u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA84D8u; call_here(rt, ctx, 0x08B878E8u); G[3] = 0x08C90000u; G[2] = 0x08C8DAD8u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DAD8u); G[4] = G[18]; G[6] = frame.at(0x20u); G[5] = 0x08CA8584u; put(rt, frame.at(0x20u), G[7]); put(rt, frame.at(0x24u), G[8]); call_here(rt, ctx, 0x08C0C758u); G[3] = 0x08C90000u; G[2] = 0x08C8DAD0u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DAD0u); G[4] = G[18]; G[6] = frame.at(0x18u); G[5] = 0x08CA8594u; put(rt, frame.at(0x18u), G[7]); put(rt, frame.at(0x1Cu), G[8]); call_here(rt, ctx, 0x08C0C9FCu); G[3] = 0x08C90000u; G[2] = 0x08C8DAC8u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DAC8u); G[4] = G[18]; G[6] = frame.at(0x10u); G[5] = 0x08CA85A4u; put(rt, frame.at(0x10u), G[7]); put(rt, frame.at(0x14u), G[8]); call_here(rt, ctx, 0x08C0C758u); G[3] = 0x08C90000u; G[2] = 0x08C8DAC0u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DAC0u); G[4] = G[18]; G[6] = frame.at(8u); G[5] = 0x08CA85B0u; put(rt, frame.at(8u), G[7]); put(rt, frame.at(0xCu), G[8]); call_here(rt, ctx, 0x08C0C9FCu); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_08A06244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x3Cu), G[31]); G[5] = 0x08CA8EB0u; put(rt, frame.at(0x38u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x30u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x34u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089D8374u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08A066F0u; call_here(rt, ctx, 0x08B8850Cu); G[6] = 0x08A05528u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA8E2Cu; call_here(rt, ctx, 0x08B878E8u); G[6] = 0x08A056D8u; G[4] = G[18]; G[7] = G[6]; G[5] = 0x08CA8E38u; call_here(rt, ctx, 0x08B878E8u); G[3] = 0x08C90000u; G[2] = 0x08C8DD94u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DD94u); G[4] = G[18]; G[6] = frame.at(0x20u); G[5] = 0x08CA8EDCu; put(rt, frame.at(0x20u), G[7]); put(rt, frame.at(0x24u), G[8]); call_here(rt, ctx, 0x08C10D98u); G[3] = 0x08C90000u; G[2] = 0x08C8DD8Cu; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DD8Cu); G[4] = G[18]; G[6] = frame.at(0x18u); G[5] = 0x08CA8EECu; put(rt, frame.at(0x18u), G[7]); put(rt, frame.at(0x1Cu), G[8]); call_here(rt, ctx, 0x08C11138u); G[3] = 0x08C90000u; G[2] = 0x08C8DD84u; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DD84u); G[4] = G[18]; G[6] = frame.at(0x10u); G[5] = 0x08CA8EFCu; put(rt, frame.at(0x10u), G[7]); put(rt, frame.at(0x14u), G[8]); call_here(rt, ctx, 0x08C10D98u); G[3] = 0x08C90000u; G[2] = 0x08C8DD7Cu; G[8] = word(rt, G[2] + 4u); G[7] = word(rt, 0x08C8DD7Cu); G[4] = G[18]; G[6] = frame.at(8u); G[5] = 0x08CA8F08u; put(rt, frame.at(8u), G[7]); put(rt, frame.at(0xCu), G[8]); call_here(rt, ctx, 0x08C11138u); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_08AB57E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x08AB57A4u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CB3ECCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = word(rt, G[16] + 0x1Cu); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == 1u) { G[4] = frame.at(0u); call_here(rt, ctx, 0x08AB57A4u); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CB3ECCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CB3EC4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x1Cu, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08AB6414(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x08AB61DCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CB4034u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = word(rt, G[16] + 0x24u); G[16] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == 1u) { G[4] = frame.at(0u); call_here(rt, ctx, 0x08AB61DCu); G[17] = word(rt, frame.at(0u)); if (G[17] == 0u) { G[4] = 0x08CB4034u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08CB3FF8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x24u, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_088C969C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x4Cu), G[19]); G[1] = 0x43480000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[19] = G[7]; put(rt, frame.at(0x48u), G[18]); G[18] = G[6]; put(rt, frame.at(0x44u), G[17]); G[17] = G[8]; put(rt, frame.at(0x40u), G[16]); G[16] = G[4]; G[4] = frame.at(0u); put_float(rt, frame.at(0x64u), ctx.fpr[21]); put(rt, frame.at(0x50u), G[31]); put_float(rt, frame.at(0x68u), ctx.fpr[22]); put_float(rt, frame.at(0x60u), ctx.fpr[20]); ctx.fpr[20] = float_at(rt, G[8] + 4u); ctx.fpr[21] = float_at(rt, G[8]); ctx.fpr[22] = float_at(rt, G[8] + 8u); ctx.fpr[20] = ctx.fpr[20] + ctx.fpr[0]; call_here(rt, ctx, 0x089545BCu); G[3] = word(rt, G[16] + 0x14u); G[2] = 1u; G[5] = frame.at(0u); G[4] = G[3]; store8(frame.at(0x2Cu), 1u); put_float(rt, frame.at(0u), ctx.fpr[21]); put_float(rt, frame.at(4u), ctx.fpr[20]); put_float(rt, frame.at(8u), ctx.fpr[22]); put(rt, frame.at(0x30u), G[18]); put(rt, frame.at(0x34u), G[19]); if (G[3] != 0u) { G[3] = word(rt, G[3]); G[2] = word(rt, G[3] + 0x18u); call_here(rt, ctx, G[2]); } G[2] = load8(frame.at(0x2Cu)); ctx.fpr[0] = float_at(rt, frame.at(0xCu)); if (G[2] == 0u) { G[18] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x34u)); put_float(rt, G[17] + 4u, ctx.fpr[0]); } G[2] = G[18]; G[3] = G[19]; G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); ctx.fpr[22] = float_at(rt, frame.at(0x68u)); ctx.fpr[21] = float_at(rt, frame.at(0x64u)); ctx.fpr[20] = float_at(rt, frame.at(0x60u)); } } }
// Builds a description string from a script table: 0x08B32FDC fills the 0x110-byte block at frame + 0x110 from the
// table at 0x08D691B4, whose first 0x110 bytes are copied word by word (four half words at a time) to frame + 0x22
// plus its two trailing words and a byte. The second half word is then read back and the object at 0x08D691B4's
// method at vtable + 0x10 answers another method, which fills the text through 0x08B85EB8 (or 0x08C8CC2C when
// empty). Answers the object.
void func_089C6BD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x210u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x204u}, {16u, 0x200u}, {18u, 0x208u}, {31u, 0x20Cu}});
    u32 from = frame.at(0x110u);
    const u32 source_end = frame.at(0x1F8u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = from;
    call_set(rt, ctx, 0x08B32FDCu, {from, 0x08D691B4u});
    u32 to = frame.at(0x22u);
    u32 half = 0u;
    do {
        const u32 first = load16(from), second = load16(from + 0x2u);
        const u32 third = load16(from + 0x4u);
        half = load16(from + 0x6u);
        store16(to, first);
        store16(to + 0x2u, second);
        store16(to + 0x4u, third);
        store16(to + 0x6u, half);
        from += 0x8u;
        to += 0x8u;
    } while (from != source_end);
    store16(to, load16(from));
    store16(to + 0x2u, load16(from + 0x2u));
    store8(to + 0x4u, load8(from + 0x4u));
    const u32 virtual_target = word(rt, word(rt, 0x08D691B4u) + 0x10u);
    const u32 reader = call_set(rt, ctx, virtual_target, {0x08D691B4u, half});
    const u32 text = call_set(rt, ctx, reader, {load16(frame.at(0x24u)), frame.at(0x1u), 0x20u});
    call_set(rt, ctx, 0x08B85EB8u, {object, text != 0u ? text : 0x08C8CC2Cu, frame.at(0u)});
    ctx.gpr[2] = object;
}
void func_08ADD800(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = 0x08D40000u; G[3] = 0x08D40628u; put(rt, frame.at(0x10u), G[16]); G[2] = (~G[4]) >> 31; put(rt, frame.at(0x18u), G[31]); G[16] = G[5]; G[5] = 0u; G[6] = word(rt, G[3] + 0x40u); G[3] = 0xFFFFFFFFu; G[6] = static_cast<std::int32_t>(G[6]) > 0 ? 1u : 0u; G[6] &= G[2]; put(rt, frame.at(4u), G[3]); if (G[6] != 0u) goto from_list; table: G[3] = 0x08D40628u; lookup: G[2] = word(rt, G[3] + 0x44u); G[4] = G[3] + 0x2Cu; G[5] = G[16]; G[6] = frame.at(0u); G[7] = frame.at(4u); if (static_cast<std::int32_t>(G[2]) <= 0) goto fallback; call_here(rt, ctx, 0x08ADD6FCu); G[3] = G[2]; if (G[2] == 0u) goto fallback; found: G[2] = word(rt, frame.at(0u)); G[2] = G[3] + G[2]; G[4] = load8(G[2]); goto done; fallback: G[3] = word(rt, frame.at(4u)); G[4] = 0x80u; G[2] = static_cast<std::int32_t>(G[3]) < 0 ? 1u : 0u; if (G[2] == 0u) G[4] = G[3]; goto done; from_list: call_here(rt, ctx, 0x08ADB278u); G[4] = G[2] + 0x78u; G[6] = frame.at(0u); G[7] = frame.at(4u); G[5] = G[16]; if (G[2] == 0u) goto table; call_here(rt, ctx, 0x08ADD6FCu); G[3] = G[2]; if (G[2] != 0u) goto found; G[3] = 0x08D40628u; goto lookup; done: G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[2] = G[4]; } } }
void func_08AEE608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[31]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08C7B118u); G[2] &= 0x20u; G[3] = 1u; if (G[2] != 0u) goto done; G[16] = 0x08CE0000u; G[2] = word(rt, 0x08CDFB3Cu) & 1u; { const bool set = G[2] != 0u; G[2] = 0x08CE0000u; if (set) goto report_bcb; } poll: call_here(rt, ctx, 0x08C7B118u); G[2] &= 0x20u; { const bool set = G[2] != 0u; G[2] = word(rt, 0x08CDFB3Cu); if (set) goto flagged; } call_here(rt, ctx, 0x08AD9E64u); G[4] = 0xC350u; G[3] = 0u; if (G[2] != 0u) goto done; call_here(rt, ctx, 0x08C7A988u); goto poll; flagged: G[2] &= 1u; { const bool set = G[2] != 0u; G[2] = 0x08CE0000u; if (set) goto report_bd4; } G[3] = 1u; goto done; report_bd4: G[4] = word(rt, 0x08CDFB40u); G[5] = 0x08C91F50u; G[7] = 0x08C924C4u; G[6] = 0xBD4u; call_here(rt, ctx, 0x08AD83B8u); G[3] = 1u; goto done; report_bcb: G[4] = word(rt, 0x08CDFB40u); G[5] = 0x08C91F50u; G[7] = 0x08C924D4u; G[6] = 0xBCBu; call_here(rt, ctx, 0x08AD83B8u); goto poll; done: G[16] = word(rt, frame.at(0u)); G[2] = G[3]; } } }
// 0x08B31B3C (a0 = the object, a1 = the destination, a2 = its size; v0 = 1 when 16 bytes were copied, else 0): the object's virtual
// +0x64 must be nonzero and its virtual +0x10 too, and 0x08C7AF20(8, sp) must answer a non-negative word; then, only when the size is at
// least 16, the four words at sp are laid down at the destination (byte by byte, as the lwl/lwr-swl/swr pairs do) and v0 = 1.
void func_08B31B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 size = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x88u}, {17u, 0x84u}, {16u, 0x80u}, {31u, 0x8Cu}});
    ctx.gpr[18] = size;
    ctx.gpr[17] = destination;
    ctx.gpr[16] = self;
    const u32 state = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x64u), {self});
    u32 result = 0u;
    if (state != 0u && call_indirect(rt, ctx, word(rt, word(rt, self) + 0x10u), {self}) != 0u) {
        const u32 available = call_set(rt, ctx, 0x08C7AF20u, {8u, frame.at(0u)});
        if (static_cast<s32>(available) >= 0 && size >= 0x10u) {
            for (unsigned i = 0u; i != 4u; ++i) {
                const u32 value = word(rt, frame.at(i * 4u));
                for (unsigned byte = 0u; byte != 4u; ++byte) store8(destination + i * 4u + byte, value >> (byte * 8u));
            }
            result = 1u;
        }
    }
    ctx.gpr[2] = result;
}
void func_08B41084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = G[6]; put(rt, frame.at(0x10u), G[20]); G[20] = G[7]; put(rt, frame.at(0xCu), G[19]); G[19] = G[8]; put(rt, frame.at(8u), G[18]); G[18] = G[9] & 0xFFu; put(rt, frame.at(4u), G[17]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0u), G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x24u); G[17] = G[4]; call_here(rt, ctx, G[2]); G[6] = G[18]; G[5] = 0x100u; G[3] = 0u; if (G[2] != 0u) { G[16] = word(rt, G[21]); G[4] = G[16]; call_here(rt, ctx, 0x08B089A8u); G[2] = 1u; G[4] = G[16]; G[5] = G[18]; store8(G[16] + 0x38u, 1u); call_here(rt, ctx, 0x08B08998u); G[4] = G[2]; G[5] = G[17] + 0xBB8u; call_here(rt, ctx, 0x08B3F7CCu); G[5] = G[20]; if (G[20] != 0u) { G[4] = G[17] + 0xBD2u; G[6] = G[19]; if (G[19] != 0u) { call_here(rt, ctx, 0x08B79A88u); put(rt, G[17] + 0xCD4u, G[19]); } } store8(G[17] + 0xACu, G[18]); G[4] = G[21]; call_here(rt, ctx, 0x08B08A58u); store8(G[17] + 0x936u, G[2]); G[3] = 1u; } G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); G[2] = G[3]; } } }
// Activates one of the object's 0x20 16-bit slots: with the sign-extended index below 0x20 and the slot's +0x948
// half word equal to what the object's virtual +0x90 method answers, it asks the +0xA0 method whether room is
// available -- yes: 0x08B40B48 (object, index, 0, 0xC, 1) then 0x08B43370 with the slot value, answering 1; no: a
// four-word request at frame + 0 (the index at +8) goes to 0x08B429D0 (object, 0xA, frame). Otherwise answers 0.
// Activates one of the object's 0x20 16-bit slots: with the sign-extended index below 0x20 and the slot's +0x948
// half word equal to what the object's virtual +0x90 method answers, it asks the +0xA0 method whether room is
// available -- yes: 0x08B40B48 (object, index, 0, 0xC, 1) then 0x08B43370 with the slot value, answering 1; no: a
// four-word request at frame + 0 (the index at +8) goes to 0x08B429D0 (object, 0xA, frame). Otherwise answers 0.
void func_08B44348(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(ctx.gpr[5])));
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {19u, 0x1Cu}, {17u, 0x14u}, {31u, 0x20u}, {16u, 0x10u}});
    const u32 slot = (index << 1) + object;
    ctx.gpr[18] = index;
    ctx.gpr[19] = slot;
    ctx.gpr[17] = object;
    if (static_cast<s32>(index) >= 0x20) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 expected = signed_half(rt, slot + 0x948u);
    ctx.gpr[16] = expected;
    if (expected != call_set(rt, ctx, word(rt, word(rt, object) + 0x90u), {object})) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 available = call_set(rt, ctx, word(rt, word(rt, object) + 0xA0u), {object});
    u32 result = 0u;
    if (available != 0u) {
        call_set(rt, ctx, 0x08B40B48u, {object, index, 0u, 0xCu, 1u});
        call_set(rt, ctx, 0x08B43370u, {object, index, signed_half(rt, slot + 0x948u)});
        result = 1u;
    } else {
        put(rt, frame.at(0x8u), 0u);
        put(rt, frame.at(0u), 0u);
        store16(frame.at(0x8u), index);
        put(rt, frame.at(0x4u), 0u);
        store16(frame.at(0xAu), 0u);
        call_set(rt, ctx, 0x08B429D0u, {object, 0xAu, frame.at(0u), 0xCu, 1u});
    }
    ctx.gpr[2] = result;
}
// Allocates and links one entry into a list: bails out with -1 when the object's +0x8C flag is clear or the
// resource helper on the +0x70 block returns a negative handle; otherwise it takes a node from the object's
// virtual allocator at vtable + 0xA0, stores the handle at its head, initialises the node's +4 block with the
// handle and the 16-bit value, sets the node's +0x30/+0x32 flags, registers it and answers its +0 link.
// Allocates and links one entry into a list: bails out with -1 when the object's +0x8C flag is clear or the
// resource helper on the +0x70 block returns a negative handle; otherwise it takes a node from the object's
// virtual allocator at vtable + 0xA0, stores the handle at its head, initialises the node's +4 block with the
// handle and the 16-bit value, sets the node's +0x30/+0x32 flags, registers it and answers its +0 link.
// Allocates and links one entry into a list: bails out with -1 when the object's +0x8C flag is clear or the
// resource helper on the +0x70 block returns a negative handle; otherwise it takes a node from the object's
// virtual allocator at vtable + 0xA0, stores the handle at its head, initialises the node's +4 block with the
// handle and the 16-bit value, sets the node's +0x30/+0x32 flags, registers it and answers its +0 link.
void func_08B46ED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], value = ctx.gpr[5] & 0xFFFFu;
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}, {31u, 0x18u}, {21u, 0x14u}, {17u, 0x4u}});
    const u32 context_block = object + 0x70u;
    ctx.gpr[20] = context_block;
    ctx.gpr[19] = value;
    ctx.gpr[18] = object;
    ctx.gpr[16] = 0xFFFFFFFFu;
    if (load8(object + 0x8Cu) == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 handle = call_set(rt, ctx, 0x08C7AE10u, {context_block});
    ctx.gpr[21] = handle;
    if (static_cast<s32>(handle) < 0) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 virtual_call = word(rt, word(rt, object) + 0xA0u);
    const u32 node = call_set(rt, ctx, virtual_call, {object, 0u});
    const u32 block = node + 0x4u;
    ctx.gpr[17] = block;
    put(rt, node, handle);
    ctx.gpr[16] = node;
    call_set(rt, ctx, 0x08B3F8C0u, {block, context_block});
    call_set(rt, ctx, 0x08B3FA3Cu, {block, value});
    store8(node + 0x30u, 1u);
    store8(node + 0x32u, 1u);
    call_set(rt, ctx, 0x08B46904u, {object, node});
    const u32 result = word(rt, node);
    ctx.gpr[16] = result;
    ctx.gpr[2] = result;
}
void func_08B76274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); G[22] = G[4]; put(rt, frame.at(0x14u), G[21]); G[21] = G[5]; put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(0u), G[16]); G[18] = word(rt, G[6] + 4u); G[20] = G[18] >> 1; G[19] = G[20] - 1u; G[17] = G[6] + 8u; if (static_cast<std::int32_t>(G[19]) >= 0) { G[16] = 0u; G[7] = G[19] - G[16]; do { G[4] = G[22]; G[16] += 1u; G[5] = G[21]; G[6] = G[17]; G[8] = G[18]; call_here(rt, ctx, 0x08B76150u); G[7] = G[19] - G[16]; } while (G[16] != G[20]); } G[18] -= 1u; G[2] = G[18] << 2; if (static_cast<std::int32_t>(G[18]) > 0) { G[16] = G[17] + G[2]; G[19] = 0u; do { G[2] = word(rt, G[16]); G[3] = word(rt, G[17]); G[8] = G[18] - G[19]; put(rt, G[17], G[2]); G[4] = G[22]; G[5] = G[21]; put(rt, G[16], G[3]); G[6] = G[17]; G[7] = 0u; G[19] += 1u; call_here(rt, ctx, 0x08B76150u); G[16] -= 4u; } while (G[19] != G[18]); } G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
// 0x08B7F148 (a0 = a 256-byte member table, a1 = a bracket expression; v0 = the position after it): clears the table with 0 (with 1 when
// the expression starts with '^', whose members then take 0 and follow at a1 + 2). Each member `c` sets table[c]; `a-b` (signed, b not
// ']' and not below a) sets the range a+1..b and `-` stands for itself when the range is malformed. The scan stops at ']' (v0 = one past
// it) or at the end (v0 = the zero byte's position); an expression starting with a zero answers a1 - 1.
void func_08B7F148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4];
    const u32 pattern = ctx.gpr[5];
    u32 current = signed_byte(rt, pattern);
    u32 cursor = pattern + 1u;
    u32 inverted = 0u;
    u32 fill = 0u;
    if (current == 0x5Eu) {  // '^'
        current = signed_byte(rt, pattern + 1u);
        cursor = pattern + 2u;
        inverted = 1u;
        fill = 1u;
    }
    for (u32 i = 0u; i != 0x100u; ++i) store8(table + i, fill);
    if (current == 0u) {
        ctx.gpr[2] = cursor - 1u;
        return;
    }
    fill = 1u - inverted;
    bool store_member = true;
    for (;;) {
        if (store_member) {
            store8(table + current, fill);
            store_member = false;
        }
        // the byte after the member and the position after it
        const u32 next = signed_byte(rt, cursor);
        const u32 advanced = cursor + 1u;
        if (next == 0x2Du) {  // '-'
            const u32 last = signed_byte(rt, cursor + 1u);
            if (last == 0x5Du || static_cast<s32>(last) < static_cast<s32>(current)) {
                current = 0x2Du;  // a malformed range: '-' itself, then the scan continues
                cursor = advanced;
                store_member = true;
                continue;
            }
            cursor += 2u;
            do {
                current += 1u;
                store8(table + current, fill);
            } while (static_cast<s32>(current) < static_cast<s32>(last));
            continue;  // the byte at the new position decides, without a member store
        }
        if (next == 0x5Du) {  // ']'
            ctx.gpr[2] = advanced;
            return;
        }
        if (next == 0u) {
            ctx.gpr[2] = cursor;
            return;
        }
        current = next;
        cursor = advanced;
        store_member = true;
    }
}
void func_08BAABC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { G[12] = G[5]; G[8] = G[4]; G[9] = G[6]; if (G[4] != G[5]) { G[2] = 0x08CC0000u; G[13] = 0x08CC3528u; do { G[11] = G[8]; if (G[9] != 0u) { G[2] = (G[8] | G[9]) & 3u; G[7] = G[8]; G[6] = G[9]; G[10] = G[8] + 0xA0u; if (G[2] != 0u) { do { G[2] = word(rt, G[7]); G[3] = word(rt, G[7] + 4u); G[4] = word(rt, G[7] + 8u); G[5] = word(rt, G[7] + 0xCu); G[7] += 0x10u; for (std::uint32_t i = 0u; i < 4u; ++i) { store8(G[6] + i, G[2] >> (i * 8u)); store8(G[6] + 4u + i, G[3] >> (i * 8u)); store8(G[6] + 8u + i, G[4] >> (i * 8u)); store8(G[6] + 0xCu + i, G[5] >> (i * 8u)); } G[6] += 0x10u; } while (G[7] != G[10]); G[2] = word(rt, G[7]); for (std::uint32_t i = 0u; i < 4u; ++i) store8(G[6] + i, G[2] >> (i * 8u)); } else { do { G[2] = word(rt, G[7]); G[3] = word(rt, G[7] + 4u); G[4] = word(rt, G[7] + 8u); G[5] = word(rt, G[7] + 0xCu); put(rt, G[6], G[2]); G[7] += 0x10u; G[6] += 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 8u, G[4]); put(rt, G[6] - 4u, G[5]); } while (G[7] != G[10]); G[2] = word(rt, G[7]); put(rt, G[6], G[2]); } G[2] = word(rt, G[8] + 0xA8u); put(rt, G[9] + 0xA4u, G[13]); put(rt, G[9] + 0xA8u, G[2]); } G[8] = G[11] + 0xACu; G[9] += 0xACu; } while (G[8] != G[12]); } G[2] = G[9]; } }
// Copies a run of 0xAC-byte records: each record moves 0xA0 bytes in 0x10-byte pieces plus one trailing word,
// word by word when both pointers are word-aligned and byte by byte (the original's unaligned `swl`/`swr`
// pair) when they are not, and then gets the type word 0x08CC3528 at +0xA4 with the source's +0xA8 word.
// Answers the advanced destination pointer.
void func_08BAAFC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 source = ctx.gpr[4], source_end = ctx.gpr[5], destination = ctx.gpr[6];
    if (source == source_end) {
        ctx.gpr[2] = destination;
        return;
    }
    u32 cursor = source, output = destination;
    do {
        if (output != 0u) {
            const bool unaligned = ((cursor | output) & 0x3u) != 0u;
            // an unaligned 32-bit store is the original's `swl`/`swr` pair: four bytes, little-endian
            auto store_word = [&](u32 to, u32 value) {
                if (unaligned) {
                    for (unsigned byte = 0; byte < 4u; ++byte) store8(to + byte, value >> (byte * 8u));
                } else {
                    put(rt, to, value);
                }
            };
            u32 from = cursor, to = output;
            for (;;) {
                const u32 first = word(rt, from), second = word(rt, from + 0x4u);
                const u32 third = word(rt, from + 0x8u), fourth = word(rt, from + 0xCu);
                store_word(to, first);
                store_word(to + 0x4u, second);
                store_word(to + 0x8u, third);
                store_word(to + 0xCu, fourth);
                from += 0x10u;
                to += 0x10u;
                if (from == cursor + 0xA0u) break;
            }
            store_word(to, word(rt, from));
            put(rt, output + 0xA4u, 0x08CC3528u);
            put(rt, output + 0xA8u, word(rt, cursor + 0xA8u));
        }
        cursor += 0xACu;
        output += 0xACu;
    } while (cursor != source_end);
    ctx.gpr[2] = output;
}
void func_08C50794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); G[19] = G[5]; put(rt, frame.at(8u), G[18]); G[18] = G[4]; put(rt, frame.at(0u), G[16]); G[16] = G[6]; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(4u), G[17]); if (G[4] != G[5]) { G[17] = G[4]; for (;;) { if (G[16] != 0u) { put(rt, G[16] + 4u, 0u); G[4] = word(rt, G[17] + 4u); put(rt, G[16], 0u); put(rt, G[16] + 4u, G[4]); if (static_cast<std::int32_t>(G[4]) > 0) { call_here(rt, ctx, 0x08B6461Cu); G[3] = word(rt, G[16] + 4u); put(rt, G[16], G[2]); if (static_cast<std::int32_t>(G[3]) > 0) { G[5] = 0u; G[2] = word(rt, G[17]); for (;;) { G[3] = word(rt, G[16]); G[2] = G[5] + G[2]; G[4] = load8(G[2]); G[3] = G[5] + G[3]; G[5] += 1u; store8(G[3], G[4]); G[2] = word(rt, G[16] + 4u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) break; G[2] = word(rt, G[17]); } } } G[2] = word(rt, G[17] + 0xCu); G[3] = load8(G[17] + 8u); G[4] = load8(G[17] + 9u); put(rt, G[16] + 0xCu, G[2]); store8(G[16] + 8u, G[3]); store8(G[16] + 9u, G[4]); } G[17] = G[18] + 0x10u; G[16] += 0x10u; if (G[19] == G[17]) break; G[18] = G[17]; } } G[2] = G[16]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
// 0x08C6F6B4 (a0 = the 0x20-byte object, a1 = a holder whose word +0x20 is a stream, a2 = a value, t0 = a mode; v0 = the object): the
// object's words +0 and +4 take -1 and +8..+0x1C zero; a mode of 0 asks 0x08B78E7C(stream, value, 0), a mode of 1 asks it with 1 and any
// other with 2 — a nonzero answer ends the function — and 0x08B79048(stream) gives the pair stored at +0 / +4 (its high word the sign of
// the low one) with +8..+0x1C zeroed again.
void func_08C6F6B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    const u32 mode = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x20u}, {17u, 0x24u}, {31u, 0x28u}});
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[16] = self;
    put(rt, self, 0xFFFFFFFFu);
    put(rt, self + 0x4u, 0xFFFFFFFFu);
    for (u32 offset = 0x8u; offset < 0x20u; offset += 0x4u) put(rt, self + offset, 0u);
    ctx.gpr[17] = owner;
    u32 kind = 0u;
    if (mode != 0u) kind = (mode ^ 1u) != 0u ? 2u : 1u;
    if (call_set(rt, ctx, 0x08B78E7Cu, {word(rt, owner + 0x20u), value, kind}) != 0u) {
        ctx.gpr[2] = self;
        return;
    }
    const u32 low = call_set(rt, ctx, 0x08B79048u, {word(rt, owner + 0x20u)});
    const u32 high = static_cast<u32>(static_cast<s32>(low) >> 31);
    put(rt, self, low);
    put(rt, self + 0x4u, high);
    for (u32 offset = 0x8u; offset < 0x20u; offset += 0x4u) put(rt, self + offset, 0u);
    ctx.gpr[2] = self;
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_234
} // namespace gt::game

#define HOT234(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_234::name, gt::ReturnKind::kind)
HOT234(0x089757A4u, func_089757A4, Word);
HOT234(0x089A1AFCu, func_089A1AFC, Word);
HOT234(0x089F8F9Cu, func_089F8F9C, Word);
HOT234(0x08A06244u, func_08A06244, Word);
HOT234(0x08AB57E4u, func_08AB57E4, Word);
HOT234(0x08AB6414u, func_08AB6414, Word);
HOT234(0x088C969Cu, func_088C969C, DoubleWord);
HOT234(0x089C6BD8u, func_089C6BD8, Word);
HOT234(0x08ADD800u, func_08ADD800, Word);
// HOT234(0x08AEE608u, func_08AEE608, Word);  // no comparable synthetic case yet
HOT234(0x08B31B3Cu, func_08B31B3C, Word);
HOT234(0x08B41084u, func_08B41084, Word);
HOT234(0x08B44348u, func_08B44348, Word);
HOT234(0x08B46ED8u, func_08B46ED8, Word);
HOT234(0x08B76274u, func_08B76274, Word);
HOT234(0x08B7F148u, func_08B7F148, Word);
HOT234(0x08BAABC8u, func_08BAABC8, Word);
HOT234(0x08BAAFC8u, func_08BAAFC8, Word);
HOT234(0x08C50794u, func_08C50794, Word);
HOT234(0x08C6F6B4u, func_08C6F6B4, Word);
