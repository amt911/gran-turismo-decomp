// Hot batch 254: seventy-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (handle-guarded calls through temporaries, class registrations, destructors, reference-count
// releases, block copies and list walks).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_254 {
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

void func_0882FBDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); const std::uint32_t value = frame.at(0u); put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; put(rt, frame.at(0x14u), G[17]); G[17] = G[7]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[19] = frame.at(4u); G[4] = G[19]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[19]; G[18] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x50u); G[19] = frame.at(8u); call_here(rt, ctx, G[2]); G[6] = G[2]; G[4] = G[18]; G[7] = G[3]; call_here(rt, ctx, 0x0885E41Cu); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08829490u); G[16] = word(rt, frame.at(8u)); G[5] = G[16]; if (G[16] == 0u) { G[4] = 0x08C9B1D4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; } G[6] = G[17]; G[4] = value; G[7] = 0xFFFFFFFFu; G[8] = 0u; G[9] = 0u; call_here(rt, ctx, 0x08A52AB8u); G[5] = word(rt, value); if (G[20] != value) { G[4] = G[20]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = value; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } G[31] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_088330DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); const std::uint32_t value = frame.at(0u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x28u), G[22]); G[22] = G[4]; G[4] = G[17]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EE58u); G[4] = G[2]; call_here(rt, ctx, 0x08893768u); G[19] = sext(load16(G[2] + 6u), 16); G[21] = frame.at(8u); G[4] = G[21]; G[5] = G[19]; G[16] = G[2]; call_here(rt, ctx, 0x08A98C1Cu); G[17] = G[16]; if (static_cast<std::int32_t>(G[19]) > 0) { G[20] = value; G[18] = 0u; G[3] = word(rt, frame.at(8u)); do { G[5] = sext(load16(G[17] + 0x560u), 16); G[2] = G[18] << 2; G[16] = word(rt, G[3] + 0x14u); G[4] = value; G[18] += 1u; G[16] += G[2]; G[17] += 2u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != value) { G[5] = word(rt, value); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(8u)); } while (G[19] != G[18]); } G[5] = word(rt, frame.at(8u)); if (G[22] != G[21]) { G[4] = G[22]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[31] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0882EC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); const std::uint32_t value = frame.at(0u); put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); auto object = [&](std::uint32_t handle) { G[4] = handle; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, handle); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = handle; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; }; if (static_cast<std::int32_t>(G[6]) > 0) { G[17] = frame.at(8u); object(G[17]); call_here(rt, ctx, 0x0885EE58u); G[16] = word(rt, G[18]); G[17] = G[2]; if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x0889367Cu); } else { G[18] = frame.at(4u); object(G[18]); call_here(rt, ctx, 0x0885EE4Cu); G[4] = G[2]; call_here(rt, ctx, 0x08893674u); G[5] = G[2]; G[4] = value; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, value); if (G[17] != value) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = value; call_here(rt, ctx, 0x08A5B9ECu); } G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08857798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); const std::uint32_t value = frame.at(0u); put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(8u); put(rt, frame.at(0x24u), G[21]); G[21] = G[4]; G[4] = G[19]; put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(0xCu); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x18u), G[18]); call_here(rt, ctx, 0x088547C4u); G[4] = 0x08C9E188u; G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[19]; G[18] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = 2u; G[4] = G[20]; call_here(rt, ctx, 0x08A98C1Cu); auto assign = [&](std::uint32_t getter, std::uint32_t offset, std::uint32_t local) { G[2] = word(rt, frame.at(0xCu)); G[4] = G[18]; G[16] = word(rt, G[2] + 0x14u); call_here(rt, ctx, getter); G[5] = G[2]; G[4] = local; G[16] += offset; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[16]; if (G[16] != local) { G[5] = word(rt, local); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = local; call_here(rt, ctx, 0x08A5B9ECu); }; assign(0x088A60A8u, 0u, G[17]); assign(0x088A60C8u, 4u, value); G[4] = G[21]; if (G[21] != G[20]) { G[5] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[31] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0887A400(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); G[16] = frame.at(8u); put(rt, frame.at(0x24u), G[17]); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[18]); call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[4] = G[16]; G[18] = G[2]; call_here(rt, ctx, 0x08AB856Cu); G[5] = G[2]; G[2] = word(rt, frame.at(0x10u)); G[4] = word(rt, frame.at(8u)); bool tail = true; if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = G[17]; call_here(rt, ctx, 0x08875860u); G[16] = G[2]; if (G[2] == 0u) G[4] = word(rt, frame.at(8u)); else { G[5] = word(rt, G[17]); G[6] = G[18]; G[4] = frame.at(4u); call_here(rt, ctx, 0x0886C798u); G[5] = word(rt, frame.at(4u)); G[4] = G[16]; G[6] = 0u; call_here(rt, ctx, 0x0887F7E8u); G[4] = word(rt, frame.at(4u)); G[17] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); const bool live = static_cast<std::int32_t>(G[2]) > 0; G[4] = word(rt, frame.at(8u)); if (!live) { G[4] = G[16]; G[5] = frame.at(1u); call_here(rt, ctx, 0x08B93554u); G[4] = word(rt, frame.at(8u)); } } else G[4] = word(rt, frame.at(8u)); G[16] = G[4] - 0xCu; if (G[17] == G[16]) tail = false; else G[4] -= 4u; goto release; } } G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] == G[2]) tail = false; else G[4] -= 4u; release: if (tail) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } G[31] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_088E15BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[31]); G[2] = G[4] << 3; G[3] = G[4] << 5; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[5] + 0xC8u; put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[18] = word(rt, G[5] + 0x780u) + G[2]; put(rt, G[18], G[4]); G[2] = G[4] << 7; G[2] -= G[3]; put(rt, G[18] + 4u, G[5]); G[2] -= G[4]; G[2] <<= 3; G[16] = word(rt, G[5] + 0xC0u); G[4] = G[19]; G[16] += G[2]; call_here(rt, ctx, 0x08B344E0u); G[5] = word(rt, G[16] + 0x1Cu); G[4] = G[16]; G[6] = 0u; G[7] = 0u; call_here(rt, ctx, 0x088E7F94u); G[4] = G[19]; call_here(rt, ctx, 0x08B3457Cu); G[2] = 0x088DF4F8u; G[5] = G[17] + 0x718u; G[4] = frame.at(0u); G[6] = frame.at(8u); put(rt, frame.at(8u), G[2]); put(rt, frame.at(0xCu), G[18]); call_here(rt, ctx, 0x08B1A338u); G[16] = word(rt, frame.at(4u)); G[3] = G[16] + 4u; auto release = [&](std::uint32_t at) { G[3] = at; G[4] = word(rt, G[3]); put(rt, G[3], G[4] - 1u); G[2] = 1u; return G[4] == G[2]; }; if (G[16] != 0u && release(G[16] + 4u)) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (release(G[16] + 8u)) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[16]; call_here(rt, ctx, G[2]); } } G[31] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08937098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x1D0u); put(rt, frame.at(0x1C0u), G[16]); G[16] = frame.at(0x80u); G[5] = G[4] + 0xCu; put(rt, frame.at(0x1C4u), G[17]); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0x1CCu), G[31]); put(rt, frame.at(0x1C8u), G[18]); call_here(rt, ctx, 0x08B7BB6Cu); G[5] = 0x08CA21ECu; G[4] = G[16]; call_here(rt, ctx, 0x08B7B9ACu); G[18] = frame.at(0x40u); G[4] = G[17] + 0x1F4u; G[5] = G[16]; call_here(rt, ctx, 0x08939D50u); G[17] = frame.at(0x180u); G[4] = G[18]; call_here(rt, ctx, 0x08B0CE58u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08B0EC60u); auto copy = [&](std::uint32_t &from, std::uint32_t &to, std::uint32_t end) { do { G[2] = word(rt, from); G[3] = word(rt, from + 4u); G[4] = word(rt, from + 8u); G[5] = word(rt, from + 0xCu); put(rt, to, G[2]); from += 0x10u; to += 0x10u; put(rt, to - 0xCu, G[3]); put(rt, to - 8u, G[4]); put(rt, to - 4u, G[5]); } while (from != end); G[2] = word(rt, from + 8u); G[3] = word(rt, from); G[4] = word(rt, from + 4u); }; G[7] = frame.at(0u); G[6] = frame.at(0x1B0u); copy(G[17], G[7], G[6]); G[8] = G[18]; put(rt, G[7] + 8u, G[2]); G[6] = frame.at(0u); G[9] = frame.at(0x30u); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[4]); copy(G[6], G[8], G[9]); put(rt, G[8] + 8u, G[2]); put(rt, G[8], G[3]); put(rt, G[8] + 4u, G[4]); G[2] = word(rt, frame.at(0x50u)); G[31] = word(rt, frame.at(0x1CCu)); G[18] = word(rt, frame.at(0x1C8u)); G[17] = word(rt, frame.at(0x1C4u)); G[16] = word(rt, frame.at(0x1C0u)); } } }
void func_089371B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x1D0u); put(rt, frame.at(0x1C0u), G[16]); G[16] = frame.at(0x80u); G[5] = G[4] + 0xCu; put(rt, frame.at(0x1C4u), G[17]); G[17] = G[4]; G[4] = G[16]; put(rt, frame.at(0x1CCu), G[31]); put(rt, frame.at(0x1C8u), G[18]); call_here(rt, ctx, 0x08B7BB6Cu); G[5] = 0x08CA21FCu; G[4] = G[16]; call_here(rt, ctx, 0x08B7B9ACu); G[18] = frame.at(0x40u); G[4] = G[17] + 0x2D0u; G[5] = G[16]; call_here(rt, ctx, 0x08939D50u); G[17] = frame.at(0x180u); G[4] = G[18]; call_here(rt, ctx, 0x08B0CE58u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08B0EC60u); auto copy = [&](std::uint32_t &from, std::uint32_t &to, std::uint32_t end) { do { G[2] = word(rt, from); G[3] = word(rt, from + 4u); G[4] = word(rt, from + 8u); G[5] = word(rt, from + 0xCu); put(rt, to, G[2]); from += 0x10u; to += 0x10u; put(rt, to - 0xCu, G[3]); put(rt, to - 8u, G[4]); put(rt, to - 4u, G[5]); } while (from != end); G[2] = word(rt, from + 8u); G[3] = word(rt, from); G[4] = word(rt, from + 4u); }; G[7] = frame.at(0u); G[6] = frame.at(0x1B0u); copy(G[17], G[7], G[6]); G[8] = G[18]; put(rt, G[7] + 8u, G[2]); G[6] = frame.at(0u); G[9] = frame.at(0x30u); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[4]); copy(G[6], G[8], G[9]); put(rt, G[8] + 8u, G[2]); put(rt, G[8], G[3]); put(rt, G[8] + 4u, G[4]); G[2] = word(rt, frame.at(0x50u)); G[31] = word(rt, frame.at(0x1CCu)); G[18] = word(rt, frame.at(0x1C8u)); G[17] = word(rt, frame.at(0x1C4u)); G[16] = word(rt, frame.at(0x1C0u)); } } }
void func_08969658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); const std::uint32_t first = frame.at(0u); put(rt, frame.at(0x14u), G[17]); G[2] = 1u; G[17] = frame.at(4u); put(rt, frame.at(0x10u), G[16]); G[4] = G[17]; G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (G[6] == G[2]) { call_here(rt, ctx, 0x08969244u); G[4] = first; G[5] = G[16]; call_here(rt, ctx, 0x08A0FD80u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA2880u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[6] = word(rt, first); G[4] = G[16]; G[2] = word(rt, G[3] + 0x1C8u); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = first; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 2u; G[4] = first; G[17] = frame.at(4u); G[18] = frame.at(8u); if (G[6] == G[2]) { call_here(rt, ctx, 0x08969244u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x089AB370u); G[5] = G[16] + 4u; G[4] = G[18]; call_here(rt, ctx, 0x08A0FD80u); G[16] = word(rt, first); G[4] = 0x08CA2880u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = word(rt, frame.at(4u)); G[6] = word(rt, frame.at(8u)); G[2] = word(rt, G[3] + 0x1C8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = first; call_here(rt, ctx, 0x08A5B9ECu); } } G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08974180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); G[5] = 0x08CA2BF0u; put(rt, frame.at(0x18u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x14u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[5] = 0x08CA2C40u; G[6] = 0x08974058u; G[4] = G[18]; call_here(rt, ctx, 0x08B87108u); G[5] = 0x08CA2C48u; G[6] = 0x08974004u; G[4] = G[18]; call_here(rt, ctx, 0x08B87108u); G[5] = 0x08CA2C50u; G[6] = 0x089740A4u; G[4] = G[18]; G[7] = 0x08973F38u; call_here(rt, ctx, 0x08B878E8u); G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0897AFF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); G[2] = 1u; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); if (G[6] == G[2]) { G[21] = frame.at(0xCu); G[4] = G[21]; call_here(rt, ctx, 0x08978000u); G[17] = word(rt, frame.at(0xCu)); if (G[17] == 0u) { G[4] = 0x08CA2F94u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = word(rt, G[16]); G[20] = word(rt, G[2] + 0x108u); if (G[16] == 0u) { G[4] = 0x08CA2F84u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[16] = frame.at(4u); G[2] = word(rt, G[3] + 0x3Cu); G[18] = frame.at(8u); call_here(rt, ctx, G[2]); G[6] = G[2]; G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, G[20]); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(8u)); if (G[19] != G[18]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); } G[31] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08994E6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); G[5] = 0x08CA41DCu; put(rt, frame.at(0x18u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x14u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A003B4u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[5] = 0x08CF6F14u; G[6] = 0x08994B2Cu; G[4] = G[18]; call_here(rt, ctx, 0x08B8850Cu); G[5] = 0x08CA41E8u; G[6] = 0x08994C80u; G[4] = G[18]; G[7] = G[6]; call_here(rt, ctx, 0x08B878E8u); G[5] = 0x08CA41F8u; G[6] = 0x08994994u; G[4] = G[18]; G[7] = G[6]; call_here(rt, ctx, 0x08B878E8u); G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_089E2548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CBC4E0u; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, G[4], G[2]); G[2] = 0x08CBC71Cu; put(rt, G[16] + 0x38u, G[2]); for (std::uint32_t field = 0x220u; field >= 0x20Cu; field -= 4u) { G[4] = G[16] + field; call_here(rt, ctx, 0x08A5B9ECu); } G[18] = 0x08D74FD0u; auto release = [&](std::uint32_t field, unsigned loaded, std::uint32_t spare) { G[loaded] = word(rt, G[16] + field); G[17] = G[loaded] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[loaded] - 4u; if (G[17] == G[18]) return; call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = spare; if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); }; G[2] = 0x08D70000u; release(0x1ACu, 3, frame.at(2u)); release(0x1A8u, 2, frame.at(1u)); release(0x1A4u, 2, frame.at(0u)); G[4] = G[16]; call_here(rt, ctx, 0x08A08B2Cu); G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_089FDE5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); G[5] = 0x08CA8818u; put(rt, frame.at(0x18u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x14u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[5] = 0x08CA8820u; G[6] = 0x089FDC8Cu; G[4] = G[18]; G[7] = 0u; call_here(rt, ctx, 0x08B8D51Cu); G[5] = 0x08CA8828u; G[6] = 0x089FDD0Cu; G[4] = G[18]; G[7] = G[6]; call_here(rt, ctx, 0x08B878E8u); G[5] = 0x08CA8834u; G[6] = 0x089FDBBCu; G[4] = G[18]; call_here(rt, ctx, 0x08B87108u); G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_089FF904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); const std::uint32_t first = frame.at(0u); put(rt, frame.at(0x14u), G[17]); G[2] = 1u; G[17] = frame.at(4u); put(rt, frame.at(0x10u), G[16]); G[4] = G[17]; G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (G[6] == G[2]) { call_here(rt, ctx, 0x089FF65Cu); G[4] = first; G[5] = G[16]; call_here(rt, ctx, 0x08A0FD80u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA8A30u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[6] = word(rt, first); G[4] = G[16]; G[2] = word(rt, G[3] + 0x1CCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = first; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 2u; G[4] = first; G[17] = frame.at(4u); G[18] = frame.at(8u); if (G[6] == G[2]) { call_here(rt, ctx, 0x089FF65Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x089AB370u); G[5] = G[16] + 4u; G[4] = G[18]; call_here(rt, ctx, 0x08A0FD80u); G[16] = word(rt, first); G[4] = 0x08CA8A30u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[5] = word(rt, frame.at(4u)); G[6] = word(rt, frame.at(8u)); G[2] = word(rt, G[3] + 0x1CCu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = first; call_here(rt, ctx, 0x08A5B9ECu); } } G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// 0x08A22CF0 (a0 = the destination text, a2 = a count, a3 = the holder of the object, t0 = 0x08ABC2B8's argument, t1 = an offset): five words on the frame
// (+4..+0x14) are zeroed and 0x08ABC2B8(t0) gives a handle (null ends the function); 0x08ABC608(handle, sp + 4) then fills the block it points into (a zero
// answer ends it too). With the count above 0 the object at [a3] (a null one calls 0x08A5BB7C(0x08CAAA48)) is asked its virtual +0x54; its float answer,
// when it differs from the float at sp + 4 + t1, is stored there and 0x08ABBE30(handle, sp + 4) runs. With no count the float at sp + 4 + t1 goes to
// 0x08A87EEC(sp) whose text the destination takes (0x08A5BAD8 unless it is sp itself) and the slot is released (0x08A5B9EC).
void func_08A22CF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], count = ctx.gpr[6], holder = ctx.gpr[7];
    const u32 source = ctx.gpr[8], offset = ctx.gpr[9];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {17u, 0x24u}, {16u, 0x20u}, {31u, 0x38u}, {18u, 0x28u}});
    const u32 text = frame.at(0x0u);
    ctx.gpr[21] = destination;
    ctx.gpr[20] = offset;
    ctx.gpr[19] = holder;
    ctx.gpr[16] = count;
    put(rt, frame.at(0x4u), 0u);
    put(rt, frame.at(0x8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    const u32 handle = call_set(rt, ctx, 0x08ABC2B8u, {source});
    ctx.gpr[17] = handle;
    if (handle == 0u) return;
    const u32 slot = frame.at(0x4u);
    ctx.gpr[18] = slot;
    if (call_set(rt, ctx, 0x08ABC608u, {handle, slot}) == 0u) return;
    const u32 value_at = slot + offset;
    if (static_cast<s32>(count) > 0) {
        const u32 object = word(rt, holder);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAA48u});
        const u32 table = word(rt, object);
        const u32 method = word(rt, table + 0x54u);
        ctx.gpr[2] = table;
        call_set(rt, ctx, method, {object});
        const float answer = ctx.fpr[0];
        ctx.gpr[9] = value_at;
        if (!(answer == float_at(rt, value_at))) {
            put_float(rt, value_at, answer);
            call_set(rt, ctx, 0x08ABBE30u, {handle, slot});
        }
        return;
    }
    ctx.fpr[12] = float_at(rt, value_at);
    call_set(rt, ctx, 0x08A87EECu, {text});
    if (destination != text) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, text)});
    call_set(rt, ctx, 0x08A5B9ECu, {text});
}
void func_08A54718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); G[21] = G[4] + 0x3Cu; put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[18] = word(rt, G[4] + 0x44u); G[20] = G[5]; if (G[18] != G[21]) { G[23] = frame.at(0u); G[22] = G[5] + 0x18u; G[30] = 0x08CB0000u; do { G[16] = word(rt, G[18] + 0x14u); G[19] = G[18] + 0x14u; if (G[16] == 0u) { G[4] = 0x08CAF594u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16] + 0x10u); G[2] = word(rt, G[20] + 0x1Cu); G[17] = G[2] + G[16]; if (G[2] == 0u) { G[17] = word(rt, G[20] + 0x18u); if (G[17] == 0u) { G[2] = 0x08CB0000u; G[4] = 0x08CAF540u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17] + 0xCu); G[17] = G[2] + G[16]; } G[16] = word(rt, G[19]); G[5] = G[16] + 0xCu; if (G[16] == 0u) { G[4] = 0x08CAF594u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16] + 0xCu; } G[7] = G[17]; G[4] = G[23]; G[6] = G[22]; call_here(rt, ctx, 0x08A57B54u); G[16] = word(rt, G[23]); G[2] = 0x08CB0000u; if (G[16] == 0u) { G[4] = 0x08CAF5E0u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08A56974u); G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08B5F16Cu); G[18] = G[2]; } while (G[2] != G[21]); } G[31] = word(rt, frame.at(0x34u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A89B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); G[5] = 0x08CB1B08u; put(rt, frame.at(0x18u), G[18]); G[6] = frame.at(1u); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = frame.at(4u); put(rt, frame.at(0x14u), G[17]); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[5] = 0x08CF6F14u; G[6] = 0x08A89E08u; G[4] = G[18]; call_here(rt, ctx, 0x08B8850Cu); G[5] = 0x08CF6F24u; G[6] = 0x08A8A034u; G[4] = G[18]; call_here(rt, ctx, 0x08B8A004u); G[5] = 0x08CB1B14u; G[6] = 0x08A89C9Cu; G[4] = G[18]; G[7] = 0x08A89F5Cu; call_here(rt, ctx, 0x08B878E8u); G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A99864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = frame.at(4u); G[4] = G[18]; put(rt, frame.at(0x14u), G[17]); G[17] = G[7]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); if (G[6] == 0u) { call_here(rt, ctx, 0x08A98674u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CB23CCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = 0x08CD4890u; G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A997D0u); G[2] = 0x08CBE7C8u; G[4] = G[18]; put(rt, frame.at(0u), G[2]); call_here(rt, ctx, 0x08A5B9ECu); } else { G[19] = frame.at(4u); G[2] = 1u; G[20] = frame.at(0xCu); G[4] = G[19]; if (G[6] == G[2]) { call_here(rt, ctx, 0x08A98674u); G[18] = word(rt, frame.at(4u)); G[4] = 0x08CB23CCu; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = 0x08CD48A8u; G[5] = G[17]; G[4] = G[20]; put(rt, frame.at(8u), G[16]); call_here(rt, ctx, 0x08A76ABCu); G[4] = G[18]; G[5] = frame.at(8u); call_here(rt, ctx, 0x08A997D0u); G[4] = G[20]; put(rt, frame.at(8u), G[16]); call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08CBE7C8u; G[4] = G[19]; put(rt, frame.at(8u), G[2]); call_here(rt, ctx, 0x08A5B9ECu); } } G[31] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A9DDCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); G[2] = 0x08CD48C0u; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, G[4], G[2]); G[4] = 1u; call_here(rt, ctx, 0x08A9DCB0u); auto drop = [&](std::uint32_t target, std::uint32_t field) { G[4] = G[16] + field; call_here(rt, ctx, target); }; drop(0x08A5B9ECu, 0x78u); drop(0x08A5B9ECu, 0x74u); drop(0x08B28DC4u, 0x4Cu); G[18] = 0x08D74FD0u; auto release = [&](std::uint32_t field, unsigned loaded, std::uint32_t spare) { G[loaded] = word(rt, G[16] + field); G[17] = G[loaded] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[loaded] - 4u; if (G[17] == G[18]) return; call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = spare; if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); }; G[2] = 0x08D70000u; release(0x48u, 3, frame.at(1u)); release(0x44u, 2, frame.at(0u)); for (std::uint32_t field = 0x40u; field >= 0x34u; field -= 4u) drop(0x08A5B9ECu, field); drop(0x08C2F9F0u, 0x2Cu); drop(0x08C2F9A4u, 0x24u); G[4] = G[16] + 0xCu; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C301B0u); G[4] = G[16]; call_here(rt, ctx, 0x08A5BB5Cu); G[4] = G[16]; G[5] = 0x80u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_254
} // namespace gt::game

#define HOT254(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_254::name, gt::ReturnKind::kind)
HOT254(0x0882FBDCu, func_0882FBDC, Word);
// HOT254(0x088330DCu, func_088330DC, Word);  // no comparable synthetic case yet
// HOT254(0x0882EC60u, func_0882EC60, Word);  // no comparable synthetic case yet
// HOT254(0x08857798u, func_08857798, Word);  // no comparable synthetic case yet
HOT254(0x0887A400u, func_0887A400, Word);
// HOT254(0x088E15BCu, func_088E15BC, Word);  // no comparable synthetic case yet
// HOT254(0x08937098u, func_08937098, Word);  // no comparable synthetic case yet
// HOT254(0x089371B0u, func_089371B0, Word);  // no comparable synthetic case yet
HOT254(0x08969658u, func_08969658, Word);
HOT254(0x08974180u, func_08974180, Word);
HOT254(0x0897AFF0u, func_0897AFF0, Word);
HOT254(0x08994E6Cu, func_08994E6C, Word);
// HOT254(0x089E2548u, func_089E2548, Word);  // no comparable synthetic case yet
HOT254(0x089FDE5Cu, func_089FDE5C, Word);
HOT254(0x089FF904u, func_089FF904, Word);
HOT254(0x08A22CF0u, func_08A22CF0, Word);
// HOT254(0x08A54718u, func_08A54718, Word);  // no comparable synthetic case yet
HOT254(0x08A89B78u, func_08A89B78, Word);
HOT254(0x08A99864u, func_08A99864, Word);
// HOT254(0x08A9DDCCu, func_08A9DDCC, Word);  // no comparable synthetic case yet
