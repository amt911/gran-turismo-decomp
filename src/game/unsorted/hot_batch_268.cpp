// Hot batch 268: fifty-eight-instruction functions no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases (two jump-table dispatchers, static initialisers, a quaternion rotation, a logarithm kernel,
// grid spacing from byte counts, a heap compaction, list erases and block copies).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_268 {
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

void func_088A2934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[18]); G[18] = G[5]; put(rt, frame.at(4u), G[17]); G[17] = 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(0xCu), G[31]); G[4] = word(rt, G[4] + 4u); G[4] += 4u; call_here(rt, ctx, 0x08B344E0u); G[2] = word(rt, G[16] + 8u); if (G[2] != 0u) { G[2] = load8(G[16] + 0x78u); G[7] = G[18]; if (G[2] == 0u) { G[6] = G[16] + 0x10u; G[8] = G[16] + 0x40u; } else { G[6] = G[16] + 0x44u; G[8] = G[16] + 0x74u; } do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6]); G[17] = 1u; put(rt, G[7], G[2]); } G[4] = word(rt, G[16] + 4u); G[4] += 4u; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[17]; G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_088C35F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x10u); auto &F = ctx.fpr; G[2] = 1u; put(rt, frame.at(4u), G[31]); G[3] = G[4]; put(rt, frame.at(0u), G[16]); if (G[4] == G[2]) { G[2] = 0xFFFFu; G[8] = 0x08D70000u; if (G[5] == G[2]) { G[4] = word(rt, 0x08D75F48u); G[5] = word(rt, 0x08D75F4Cu); G[2] = 0x08CE0000u; G[6] = G[4] + 1u; G[1] = 0x3F0E38E4u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, 0x08CE304Cu, F[0]); G[2] = G[6] < G[4] ? 1u : 0u; G[7] = G[2] + G[5]; put(rt, 0x08D75F48u, G[6]); put(rt, 0x08D75F4Cu, G[7]); if (G[6] == G[3]) { G[4] = 0x08D70000u; if (G[7] == 0u) { G[2] = sext(load8(0x08D75F50u), 8); G[16] = 0x08D75F50u; G[4] = G[16]; if (G[2] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[6] = 0x08D70000u; G[3] = 0x08D70000u; G[5] = 0x08D75F5Cu; G[3] = 0x08D75F68u; G[4] = G[16]; if (G[2] != 0u) { G[2] = 0x1001u; put(rt, G[5] + 0x18u, G[2]); put(rt, G[5] + 0xCu, G[3]); put(rt, G[5] + 0x10u, 0u); put(rt, G[5] + 4u, 0u); put(rt, G[5] + 8u, 0u); put(rt, G[5] + 0x14u, G[3]); put(rt, G[5] + 0x1Cu, 0u); put(rt, G[5] + 0x20u, 0u); put(rt, G[5] + 0x24u, 0u); put(rt, 0x08D75F5Cu, 0u); call_here(rt, ctx, 0x08B64494u); G[4] = 0x088C3548u; tail = true; } } } } } } G[16] = word(rt, frame.at(0u)); } if (tail) jump(ctx, 0x08B77B1Cu); } }
// 0x089161F0 (a0 = 1 registers, 0 unregisters, with a1 = 0xFFFF either way; v0 = 1 or 0xFFFF on the invalid paths, 0x08CF205C after
// registering, the tail call's when unregistering): registering clears the words +0, +4, +0x10, +0x14 and +0x18 of 0x08CF205C, puts
// -1 at its +0xC, calls 0x08B28DCC(0x08CF2094), then sets its +0x34 to 1.0 (0x3F80), clears +0x1C to +0x30 and the four bytes at
// 0x08CF2064..0x08CF2067 (a three-pass spin loop after the call has no effect but time). Unregistering makes 0x08B28DC4(0x08CF2094)
// the tail call.
void func_089161F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 operation = ctx.gpr[4], second = ctx.gpr[5];
    u32 tail = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{31u, 0x4u}, {16u, 0x0u}});
        ctx.gpr[2] = 1u;
        if (operation == 1u) {
            ctx.gpr[2] = 0xFFFFu;
            if (second != 0xFFFFu) return;
            const u32 block = 0x08CF205Cu;
            put(rt, block + 0xCu, 0xFFFFFFFFu);
            put(rt, block + 0x18u, 0u);
            put(rt, block + 0x4u, 0u);
            put(rt, block + 0x10u, 0u);
            put(rt, block + 0x14u, 0u);
            put(rt, block, 0u);
            call_set(rt, ctx, 0x08B28DCCu, {0x08CF2094u});
            ctx.fpr[0] = std::bit_cast<float>(0x3F800000u);
            put_float(rt, block + 0x34u, ctx.fpr[0]);
            put(rt, block + 0x1Cu, 0u);
            put(rt, block + 0x20u, 0u);
            put(rt, block + 0x24u, 0u);
            put(rt, block + 0x28u, 0u);
            put(rt, block + 0x2C, 0u);
            put(rt, block + 0x30u, 0u);
            for (u32 address = 0x08CF2064u; address != 0x08CF2068u; ++address) store8(address, 0u);
            ctx.gpr[2] = block;
            return;
        }
        ctx.gpr[2] = 0xFFFFu;
        if (operation != 0u) return;
        if (second != 0xFFFFu) return;
        tail = 0x08B28DC4u;
        ctx.gpr[4] = 0x08CF2094u;
    }
    jump(ctx, tail);
}
void func_089664D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); auto &F = ctx.fpr; G[2] = G[5] < 5u ? 1u : 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(4u), G[31]); if (G[2] != 0u) { G[3] = 0x08C8B604u; G[2] = G[5] << 2; G[2] += G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08966508u: G[4] = G[16]; G[5] = 0u; target = 0x08966480u; break; case 0x08966520u: G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x08966480u); G[4] = G[16]; G[1] = 0x3F000000u; F[12] = std::bit_cast<float>(G[1]); target = 0x089661B8u; break; case 0x08966548u: G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x08966480u); F[12] = 0.0f; G[4] = G[16]; target = 0x089661B8u; break; case 0x0896656Cu: G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x08966480u); G[4] = G[16]; G[1] = 0x3F800000u; F[12] = std::bit_cast<float>(G[1]); target = 0x089661B8u; break; case 0x08966594u: G[4] = G[16]; G[5] = 1u; target = 0x089662F8u; break; } } G[16] = word(rt, frame.at(0u)); } if (target != 0u) jump(ctx, target); } }
void func_08966610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); auto &F = ctx.fpr; G[2] = G[5] < 5u ? 1u : 0u; put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(4u), G[31]); if (G[2] != 0u) { G[3] = 0x08C8B618u; G[2] = G[5] << 2; G[2] += G[3]; G[4] = word(rt, G[2]); switch (G[4]) { case 0x08966644u: G[4] = G[16]; G[5] = 0u; target = 0x089665BCu; break; case 0x0896665Cu: G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x089665BCu); G[4] = G[16]; G[1] = 0x3F000000u; F[12] = std::bit_cast<float>(G[1]); target = 0x08966204u; break; case 0x08966684u: G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x089665BCu); F[12] = 0.0f; G[4] = G[16]; target = 0x08966204u; break; case 0x089666A8u: G[4] = G[16]; G[5] = 1u; call_here(rt, ctx, 0x089665BCu); G[4] = G[16]; G[1] = 0x3F800000u; F[12] = std::bit_cast<float>(G[1]); target = 0x08966204u; break; case 0x089666D0u: G[4] = G[16]; G[5] = 1u; target = 0x089663BCu; break; } } G[16] = word(rt, frame.at(0u)); } if (target != 0u) jump(ctx, target); } }
void func_089E0188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; auto cvt = [](std::uint32_t v) { return static_cast<float>(static_cast<std::int32_t>(v)); }; G[3] = load8(G[4] + 0x208u); G[5] = load8(G[4] + 0x20Au); G[1] = 0x3F800000u; F[2] = std::bit_cast<float>(G[1]); F[1] = std::bit_cast<float>(G[3]); G[2] = G[5] & 0xFFu; F[0] = cvt(G[3]); G[3] -= 1u; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[7] = G[4]; if (G[2] != 0u) { store8(G[4] + 0x20Au, G[3]); G[5] = G[3]; } G[5] &= 0xFFu; G[3] = load8(G[7] + 0x209u); F[12] = F[2] / F[0]; G[8] = load8(G[7] + 0x20Bu); F[1] = cvt(G[5]); F[0] = std::bit_cast<float>(G[3]); G[2] = G[8] & 0xFFu; F[5] = cvt(G[3]); G[3] -= 1u; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { store8(G[7] + 0x20Bu, G[3]); G[8] = G[3]; } F[5] = F[2] / F[5]; G[5] = load8(G[7] + 0x20Au); G[2] = load8(G[7] + 0x20Bu); G[8] &= 0xFFu; F[10] = std::bit_cast<float>(G[5]); G[5] += 1u; G[2] += 1u; F[11] = F[12] * F[1]; F[8] = std::bit_cast<float>(G[2]); F[1] = std::bit_cast<float>(G[5]); F[6] = cvt(G[8]); F[3] = cvt(G[2] - 1u); F[0] = cvt(G[5]); F[9] = cvt(G[2]); F[2] = cvt(G[5] - 1u); put_float(rt, G[7] + 0x1E8u, F[11]); F[0] = F[12] * F[0]; F[2] = F[12] * F[2]; put_float(rt, G[7] + 0x1F0u, F[0]); put_float(rt, G[7] + 0x1F8u, F[0]); F[1] = F[5] * F[9]; F[6] = F[5] * F[6]; F[3] = F[5] * F[3]; put_float(rt, G[7] + 0x200u, F[2]); put_float(rt, G[7] + 0x204u, F[1]); put_float(rt, G[7] + 0x1ECu, F[6]); put_float(rt, G[7] + 0x1F4u, F[3]); put_float(rt, G[7] + 0x1FCu, F[1]); jump(ctx, 0x089E00E4u); } }
// 0x089F3B58 (a0 = the object; v0 = the last call's result): a constructor: 0x089E20D0(object) first, then the vtable 0x08CC8288 at
// +0, 0x08CC84C4 at +0x38, the empty counted string header 0x08D74FDC (0x08D74FD0 + 0xC) at +0x22C to +0x248, 0x089E4C14 for each of
// +0x24C, +0x250, +0x254, +0x258, +0x25C, +0x260, +0x264 and +0x268 with 0x08CF6ED8, then 128.0 (0x4180) at +0x26C and +0x270 and
// 0 at +0x274.
void func_089F3B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[16] = object;
    call_set(rt, ctx, 0x089E20D0u, {object});
    put(rt, object, 0x08CC8288u);
    const u32 empty = 0x08D74FDCu;
    put(rt, object + 0x38u, 0x08CC84C4u);
    put(rt, object + 0x248u, empty);
    put(rt, object + 0x22Cu, empty);
    put(rt, object + 0x230u, empty);
    put(rt, object + 0x234u, empty);
    put(rt, object + 0x238u, empty);
    put(rt, object + 0x23Cu, empty);
    put(rt, object + 0x240u, empty);
    put(rt, object + 0x244u, empty);
    for (u32 offset = 0x24Cu; offset <= 0x268u; offset += 0x4u) call_set(rt, ctx, 0x089E4C14u, {object + offset, 0x08CF6ED8u});
    ctx.fpr[0] = std::bit_cast<float>(0x41800000u);
    put_float(rt, object + 0x270u, ctx.fpr[0]);
    put(rt, object + 0x274u, 0u);
    put_float(rt, object + 0x26Cu, ctx.fpr[0]);
}
void func_089FB25C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); auto &F = ctx.fpr; put(rt, frame.at(0x20u), G[16]); G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); G[16] = G[4]; put(rt, frame.at(0x24u), G[31]); put(rt, G[4] + 0xCu, G[5]); if (static_cast<std::int32_t>(G[5]) >= 0) { G[4] = G[5]; call_here(rt, ctx, 0x089F9CA8u); G[5] = G[2]; G[4] = G[16]; if (G[16] != G[2]) call_here(rt, ctx, 0x08C0E654u); G[2] = word(rt, G[16]); G[3] = word(rt, G[16] + 4u); G[4] = G[2]; G[3] -= G[2]; G[3] = sra(G[3], 5); G[2] = 1u; if (G[3] == G[2]) { G[5] = word(rt, G[16] + 0xCu); call_here(rt, ctx, 0x08A0B050u); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0xCu, G[2]); } } else { G[8] = word(rt, G[4] + 4u); G[5] = word(rt, G[4]); G[7] = frame.at(0u); G[6] = G[8]; G[2] = G[8] - G[5]; G[9] = sra(G[2], 5); G[3] = G[9] < 2u ? 1u : 0u; G[2] = 0xFFFFFFFFu; G[5] += 0x20u; put_float(rt, frame.at(0u), F[0]); put(rt, frame.at(0x10u), G[2]); put_float(rt, frame.at(0xCu), F[0]); put_float(rt, frame.at(8u), F[0]); put_float(rt, frame.at(4u), F[0]); if (G[3] == 0u) call_here(rt, ctx, 0x08C0DBBCu); else { G[6] = 1u; G[6] -= G[9]; G[5] = G[8]; call_here(rt, ctx, 0x08C0E3B8u); } G[5] = word(rt, G[16] + 0xCu); G[4] = word(rt, G[16]); call_here(rt, ctx, 0x08A0B050u); } G[16] = word(rt, frame.at(0x20u)); } } }
void func_08A1EC18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); G[22] = G[9]; put(rt, frame.at(0x14u), G[21]); G[21] = G[6]; put(rt, frame.at(0x10u), G[20]); G[20] = G[8]; put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put(rt, frame.at(8u), G[18]); G[18] = G[7]; put(rt, frame.at(4u), G[17]); G[17] = G[5]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0u), G[16]); put(rt, G[6], 0u); put(rt, G[7], 0u); put(rt, G[8], 0u); put(rt, G[9], 0u); call_here(rt, ctx, 0x08AF6CE8u); G[3] = 0u; if (G[2] != 0u) { G[2] = word(rt, G[19] + 0x20u); G[16] = 0u; G[3] = G[17] - G[2]; if (G[2] != 0u) G[16] = G[3]; G[17] -= G[16]; G[4] = G[17]; put(rt, G[18], G[17]); call_here(rt, ctx, 0x08B2B5E0u); G[4] = G[2]; put(rt, G[21], G[2]); G[5] = G[19]; G[6] = word(rt, G[18]); call_here(rt, ctx, 0x08B79A88u); G[4] = G[16]; G[3] = 1u; if (G[16] != 0u) { call_here(rt, ctx, 0x08B59AC8u); put(rt, G[20], G[2]); G[5] = G[19] + G[17]; G[6] = G[16]; put(rt, G[22], G[16]); G[4] = word(rt, G[20]); call_here(rt, ctx, 0x08B79A88u); G[3] = 1u; } } G[2] = G[3]; G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08A4D420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; F[3] = 0.0f; G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); G[1] = 0x44200000u; F[1] = std::bit_cast<float>(G[1]); G[1] = 0x43F00000u; F[2] = std::bit_cast<float>(G[1]); G[3] = 0xFF000000u; G[2] = 0xFFFFFFFFu; put(rt, G[4] + 0x3Cu, G[2]); put(rt, G[4] + 0x44u, G[3]); put_float(rt, G[4] + 0x80u, F[0]); put_float(rt, G[4], F[3]); put_float(rt, G[4] + 4u, F[3]); put_float(rt, G[4] + 8u, F[1]); put_float(rt, G[4] + 0xCu, F[2]); put_float(rt, G[4] + 0x10u, F[3]); put_float(rt, G[4] + 0x14u, F[3]); put_float(rt, G[4] + 0x18u, F[1]); put_float(rt, G[4] + 0x1Cu, F[2]); put_float(rt, G[4] + 0x20u, F[0]); put_float(rt, G[4] + 0x24u, F[0]); for (std::uint32_t off = 0x28u; off <= 0x38u; off += 4u) put_float(rt, G[4] + off, F[3]); put(rt, G[4] + 0x40u, G[3]); for (std::uint32_t off = 0x5Cu; off <= 0x68u; off += 4u) put_float(rt, G[4] + off, F[3]); for (std::uint32_t off = 0x6Cu; off <= 0x78u; off += 4u) put(rt, G[4] + off, 0u); put_float(rt, G[4] + 0x7Cu, F[0]); for (std::uint32_t off = 0x84u; off <= 0x90u; off += 4u) put(rt, G[4] + off, 0u); store16(G[4] + 0x94u, 0u); put(rt, G[4] + 0x98u, 0u); store8(G[4] + 0x58u, 0u); G[2] = word(rt, G[4] + 0x58u); put_float(rt, G[4] + 0x50u, F[1]); G[2] &= ~0x300u; G[2] |= 0x1C00u; G[2] &= ~0x2000u; G[2] &= ~0x8000u; put(rt, G[4] + 0x58u, G[2]); put_float(rt, G[4] + 0x4Cu, F[3]); put_float(rt, G[4] + 0x54u, F[2]); put(rt, G[4] + 0x9Cu, 0u); put_float(rt, G[4] + 0xA0u, F[3]); put(rt, G[4] + 0xA4u, 0u); put_float(rt, G[4] + 0x48u, F[3]); } }
void func_08AC79B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); G[2] = 0x08D00000u; put(rt, frame.at(0x24u), G[21]); G[21] = G[5]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; put(rt, frame.at(0x1Cu), G[19]); G[19] = 6u; put(rt, frame.at(0x18u), G[18]); G[18] = 0u; put(rt, frame.at(0x14u), G[17]); G[17] = 0x08CFFD90u; put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x10u), G[16]); G[16] = word(rt, G[17] + 8u); bool found = false; while (true) { G[18] += 1u; G[4] = G[16]; call_here(rt, ctx, 0x08B34888u); G[2] = load8(G[17] + 0xDu); G[4] = G[16]; if (G[2] == 0u) { found = true; break; } G[17] += 0x1Cu; call_here(rt, ctx, 0x08B34B24u); if (G[18] == G[19]) break; G[16] = word(rt, G[17] + 8u); } if (found) { G[2] = 1u; store8(G[17] + 0xDu, G[2]); G[3] = word(rt, G[20] + 4u); G[2] = word(rt, G[20]); G[4] = word(rt, G[17] + 4u); put(rt, G[17] + 0x14u, G[3]); put(rt, G[17] + 0x10u, G[2]); put(rt, G[17] + 0x18u, G[21]); call_here(rt, ctx, 0x08B28C74u); G[4] = G[16]; G[16] = word(rt, G[17]); call_here(rt, ctx, 0x08B34B24u); G[2] = G[16]; } else { G[2] = 0x3000u; G[4] = G[20]; G[6] = G[21]; G[5] = frame.at(0u); G[7] = 0u; put(rt, frame.at(4u), G[2]); put(rt, frame.at(0u), 0u); call_here(rt, ctx, 0x08B1B92Cu); G[16] = G[2]; } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08AE740C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[17] = G[4]; G[4] += 0x14u; put(rt, frame.at(8u), G[18]); G[18] = 0u; put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08AE6E4Cu); G[3] = load8(G[17] + 0x36u); G[2] = sext(G[3], 8); if (static_cast<std::int32_t>(G[2]) > 0) { G[16] = G[17] + 0x30u; do { G[2] = sext(load16(G[16] + 8u), 16); G[5] = 3u; G[6] = 0u; G[7] = 0u; G[18] += 1u; G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08AEF874u); G[4] = sext(load16(G[16] + 8u), 16); call_here(rt, ctx, 0x08AEF1A8u); G[3] = load8(G[17] + 0x36u); } G[2] = sext(G[3], 8); G[2] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[16] += 2u; } while (G[2] != 0u); } G[4] = sext(load16(G[17] + 0x34u), 16); if (static_cast<std::int32_t>(G[4]) >= 0) call_here(rt, ctx, 0x08ADC230u); G[4] = word(rt, G[17]); G[5] = 0x08D40000u; put(rt, G[17] + 0xCu, 0u); G[6] = 0x08D409A4u; if (G[4] != G[17]) { G[2] = word(rt, G[17] + 4u); put(rt, G[4] + 4u, G[2]); G[3] = word(rt, G[17] + 4u); put(rt, G[17] + 4u, G[17]); put(rt, G[3], G[4]); put(rt, G[17], G[17]); } G[3] = word(rt, 0x08D409A4u); put(rt, G[17] + 4u, G[6]); put(rt, G[3] + 4u, G[17]); G[2] = word(rt, G[17] + 4u); put(rt, G[17], G[3]); put(rt, G[2], G[17]); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B0EA60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x150u); put(rt, frame.at(0x148u), G[18]); G[18] = frame.at(0x40u); put(rt, frame.at(0x140u), G[16]); G[16] = G[4]; G[4] = frame.at(0u); put(rt, frame.at(0x14Cu), G[31]); put(rt, frame.at(0x144u), G[17]); call_here(rt, ctx, 0x08B0CE58u); G[4] = G[18]; call_here(rt, ctx, 0x08B0E0CCu); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08B0EA48u); G[16] = G[2]; if (G[2] != 0u) { G[2] = 0xDu; put(rt, G[16] + 0x7Cu, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08B0DE3Cu); G[6] = G[16] + 0xA8u; G[7] = frame.at(0u); G[8] = G[16] + 0xD8u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6] + 8u); G[3] = word(rt, G[6]); G[5] = word(rt, G[6] + 4u); G[4] = G[16]; put(rt, G[7] + 8u, G[2]); put(rt, G[7], G[3]); put(rt, G[7] + 4u, G[5]); call_here(rt, ctx, 0x08B0EA58u); } G[2] = word(rt, frame.at(0u)); G[16] = 0u; G[17] = 0u; if (G[2] == 0u) { G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } G[4] = G[18]; call_here(rt, ctx, 0x08B0DDF0u); G[2] = G[16]; G[3] = G[17]; G[18] = word(rt, frame.at(0x148u)); G[17] = word(rt, frame.at(0x144u)); G[16] = word(rt, frame.at(0x140u)); } } }
void func_08B2BB74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); put(rt, frame.at(0x14u), G[31]); G[18] = 0u; call_here(rt, ctx, 0x08B2BB10u); G[2] = word(rt, G[19] + 0x48u); G[17] = word(rt, G[19] + 0x44u); G[16] = word(rt, G[19] + 0x2Cu); G[2] = G[2] << 3; G[20] = G[17] + G[2]; put(rt, G[19] + 0x38u, 0u); G[3] = word(rt, G[17] + 4u); while (true) { if (G[3] != 0u) { G[4] = word(rt, G[3]); if (G[16] != G[3]) { G[2] = G[3] - G[16]; G[7] = G[2] - 4u; G[2] = G[4] & 1u; G[6] = G[4] + 4u; G[5] = G[3]; G[4] = G[16]; if (G[2] != 0u) { put(rt, G[16], G[7]); put(rt, G[16] + 4u, 0u); if (G[18] == 0u) put(rt, G[19] + 0x38u, G[16]); else put(rt, G[18] + 4u, G[16]); G[18] = G[16]; G[16] = G[3]; } else { call_here(rt, ctx, 0x08B79BACu); put(rt, G[17] + 4u, G[16]); } } G[2] = word(rt, G[16]); G[3] = G[16] + 4u; G[2] &= ~3u; G[16] = G[3] + G[2]; } G[17] += 8u; if (G[20] == G[17]) break; G[3] = word(rt, G[17] + 4u); } put(rt, G[19] + 0x3Cu, G[16]); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B4B86C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; F[4] = float_at(rt, G[4] + 4u); F[5] = float_at(rt, G[4]); F[11] = float_at(rt, G[4] + 8u); F[15] = F[4] * F[4]; F[12] = float_at(rt, G[4] + 0xCu); G[1] = 0x3F000000u; F[10] = std::bit_cast<float>(G[1]); F[1] = F[5] * F[4]; F[13] = F[11] * F[12]; F[6] = F[11] * F[11]; F[0] = F[10] - F[15]; F[14] = F[4] * F[12]; F[2] = F[5] * F[11]; F[3] = F[1] - F[13]; F[0] = F[0] - F[6]; F[8] = float_at(rt, G[6]); F[9] = float_at(rt, G[6] + 4u); F[7] = F[2] + F[14]; F[0] = F[0] * F[8]; F[3] = F[3] * F[9]; F[8] = float_at(rt, G[6] + 8u); F[9] = F[5] * F[5]; F[6] = F[10] - F[6]; F[0] = F[0] + F[3]; F[7] = F[7] * F[8]; F[4] = F[4] * F[11]; F[5] = F[5] * F[12]; F[1] = F[1] + F[13]; F[0] = F[0] + F[7]; F[6] = F[6] - F[9]; F[3] = F[4] - F[5]; F[5] = F[5] + F[4]; F[0] = F[0] + F[0]; F[2] = F[2] - F[14]; F[10] = F[10] - F[9]; put_float(rt, G[5], F[0]); F[10] = F[10] - F[15]; F[0] = float_at(rt, G[6]); F[4] = float_at(rt, G[6] + 4u); F[7] = float_at(rt, G[6] + 8u); F[1] = F[1] * F[0]; F[6] = F[6] * F[4]; F[3] = F[3] * F[7]; F[1] = F[1] + F[6]; F[1] = F[1] + F[3]; F[1] = F[1] + F[1]; put_float(rt, G[5] + 4u, F[1]); F[0] = float_at(rt, G[6]); F[1] = float_at(rt, G[6] + 4u); F[3] = float_at(rt, G[6] + 8u); F[2] = F[2] * F[0]; F[5] = F[5] * F[1]; F[10] = F[10] * F[3]; F[2] = F[2] + F[5]; F[2] = F[2] + F[10]; F[2] = F[2] + F[2]; put_float(rt, G[5] + 8u, F[2]); } }
void func_08B6EE80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[4] = G[5]; put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); call_here(rt, ctx, 0x08B71CE0u); G[4] = 0x08CB0000u; G[18] = word(rt, 0x08CB6900u); G[19] = word(rt, 0x08CB6904u); G[5] = G[3]; G[6] = G[18]; G[7] = G[19]; G[4] = G[2]; call_here(rt, ctx, 0x08B71394u); G[4] = G[16]; G[20] = G[2]; G[21] = G[3]; call_here(rt, ctx, 0x08B71CE0u); G[4] = G[2]; G[5] = G[3]; G[6] = G[18]; G[7] = G[19]; if (static_cast<std::int32_t>(G[16]) >= 0) { G[4] = G[20]; G[5] = G[21]; G[6] = G[2]; G[7] = G[3]; call_here(rt, ctx, 0x08B71328u); } else { call_here(rt, ctx, 0x08B71328u); G[6] = G[2]; G[4] = G[20]; G[5] = G[21]; G[7] = G[3]; call_here(rt, ctx, 0x08B71328u); } G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08B846C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; auto cvt = [](std::uint32_t v) { return static_cast<float>(static_cast<std::int32_t>(v)); }; G[3] = 0x00350000u; G[4] = 0x003504F3u; G[3] = std::bit_cast<std::uint32_t>(F[12]); G[10] = G[3]; G[12] = G[3]; G[5] = 0x00B10000u; G[3] &= 0x007FFFFFu; G[11] = 0xFF800000u; G[2] = 0xA8A90000u; G[9] = 0x00B17218u; G[5] = G[3] + G[11]; G[8] = 0xA8A9D830u; G[10] &= 0x7FFFFFFFu; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[6] = G[12]; G[7] = G[5]; if (static_cast<std::int32_t>(G[12]) <= 0) { G[6] = 0xFF800000u; G[4] = 0xFF80FFFFu; if (G[10] == 0u) G[4] = 0xFF800000u; F[0] = std::bit_cast<float>(G[4]); return; } G[15] = 0x01800000u; if (G[2] == 0u) { G[5] = G[3]; G[6] = G[12] + G[11]; G[3] = G[7]; } G[14] = G[3] + G[15]; F[18] = std::bit_cast<float>(G[5]); F[17] = std::bit_cast<float>(G[14]); G[13] = sra(G[6], 23); F[15] = cvt(G[5]); F[14] = cvt(G[14]); const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[13])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[9])); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); G[9] = 0x08CB0000u; F[12] = float_at(rt, 0x08CB6214u); F[5] = F[15] / F[14]; G[7] = 0x08CB0000u; G[12] = ctx.lo; G[11] = G[12] + G[8]; G[8] = 0x08CB0000u; F[11] = float_at(rt, 0x08CB6218u); F[16] = std::bit_cast<float>(G[11]); F[7] = float_at(rt, 0x08CB621Cu); G[10] = 0x08CB0000u; F[2] = float_at(rt, 0x08CB6210u); F[13] = cvt(G[11]); F[9] = F[5] * F[5]; F[4] = F[13] * F[2]; F[10] = F[9] * F[12]; F[8] = F[10] + F[11]; F[6] = F[8] * F[9]; F[3] = F[6] + F[7]; F[1] = F[5] * F[3]; F[0] = F[4] + F[1]; } }
void func_08C286C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); G[20] = G[6]; put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put(rt, frame.at(0u), G[16]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); G[2] = word(rt, G[4] + 0xCu); G[16] = G[5]; bool loop = false; if (G[2] == G[5]) { G[21] = G[4] + 4u; G[18] = G[5]; if (G[6] == G[21]) { G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C28648u); put(rt, G[19] + 0x10u, G[21]); put(rt, G[19] + 0x14u, 0u); put(rt, G[19] + 0xCu, G[21]); put(rt, G[19] + 8u, 0u); } else { G[21] = G[19] + 4u; loop = G[20] != G[5]; } } else { G[18] = G[5]; G[21] = G[19] + 4u; loop = G[20] != G[5]; } if (loop) { do { G[4] = G[18]; call_here(rt, ctx, 0x08B5F100u); G[5] = G[21]; G[4] = G[16]; G[18] = G[2]; call_here(rt, ctx, 0x08B5F444u); G[17] = G[2] + 0x10u; G[4] = G[2] + 0x14u; G[16] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0x18u; G[6] = 4u; call_here(rt, ctx, 0x08A5BC54u); G[2] = word(rt, G[19] + 0x14u); G[16] = G[18]; G[2] -= 1u; put(rt, G[19] + 0x14u, G[2]); } while (G[18] != G[20]); } G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_268
} // namespace gt::game

#define HOT268(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_268::name, gt::ReturnKind::kind)
// HOT268(0x088A2934u, func_088A2934, Word);  // no comparable synthetic case yet
HOT268(0x088C35F0u, func_088C35F0, Word);
HOT268(0x089161F0u, func_089161F0, Word);
HOT268(0x089664D4u, func_089664D4, Word);
HOT268(0x08966610u, func_08966610, Word);
HOT268(0x089E0188u, func_089E0188, Word);
HOT268(0x089F3B58u, func_089F3B58, Word);
HOT268(0x089FB25Cu, func_089FB25C, Word);
// HOT268(0x08A1EC18u, func_08A1EC18, Word);  // no comparable synthetic case yet
HOT268(0x08A4D420u, func_08A4D420, Word);
// HOT268(0x08AC79B0u, func_08AC79B0, Word);  // no comparable synthetic case yet
// HOT268(0x08AE740Cu, func_08AE740C, Word);  // no comparable synthetic case yet
HOT268(0x08B0EA60u, func_08B0EA60, DoubleWord);
// HOT268(0x08B2BB74u, func_08B2BB74, Word);  // no comparable synthetic case yet
HOT268(0x08B4B86Cu, func_08B4B86C, Word);
HOT268(0x08B6EE80u, func_08B6EE80, DoubleWord);
HOT268(0x08B846C0u, func_08B846C0, Float);
// HOT268(0x08C286C4u, func_08C286C4, Word);  // no comparable synthetic case yet
