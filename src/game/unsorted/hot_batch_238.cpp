// Hot batch 238: fifty-eight-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (handle-guarded calls through temporaries, string releases, curve lookups and table reads).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_238 {
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
    if (!(value >= -2387483648.0f && value < 2387483648.0f)) return 0x7FFFFFFF;
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

void func_0880E0F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); G[19] = G[6]; put(rt, frame.at(8u), G[18]); G[18] = G[5]; put(rt, frame.at(4u), G[17]); G[17] = G[4]; put_float(rt, frame.at(0x2Cu), ctx.fpr[23]); ctx.fpr[23] = ctx.fpr[12]; put_float(rt, frame.at(0x28u), ctx.fpr[22]); ctx.fpr[22] = ctx.fpr[13]; put_float(rt, frame.at(0x24u), ctx.fpr[21]); ctx.fpr[21] = ctx.fpr[14]; put_float(rt, frame.at(0x20u), ctx.fpr[20]); put(rt, frame.at(0u), G[16]); G[16] = word(rt, G[4]); G[4] = G[5]; G[5] = G[6]; ctx.fpr[0] = float_at(rt, G[16] + 8u); ctx.fpr[1] = std::bit_cast<float>(static_cast<std::uint32_t>(trunc_w_s(ctx.fpr[0]))); G[3] = bits_of(ctx.fpr[1]); G[2] = G[3] - 1u; G[7] -= G[2]; G[7] <<= 4; G[16] += G[7]; ctx.fpr[12] = float_at(rt, G[16]); ctx.fpr[13] = float_at(rt, G[16] + 4u); ctx.fpr[14] = float_at(rt, G[16] + 8u); ctx.fpr[15] = float_at(rt, G[16] + 0xCu); call_here(rt, ctx, 0x089DB078u); G[2] = word(rt, G[17]); ctx.fpr[20] = float_at(rt, G[16] + 8u); ctx.fpr[15] = float_at(rt, G[16] + 0xCu); ctx.fpr[0] = float_at(rt, G[2]); ctx.fpr[1] = float_at(rt, G[2] + 4u); ctx.fpr[13] = ctx.fpr[22]; ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[0]); ctx.fpr[15] = psp_mul(ctx.fpr[15], ctx.fpr[1]); ctx.fpr[12] = ctx.fpr[23]; G[4] = G[18]; ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[21]); ctx.fpr[15] = psp_mul(ctx.fpr[21], ctx.fpr[15]); G[5] = G[19]; ctx.fpr[14] = ctx.fpr[20]; call_here(rt, ctx, 0x089DAEA0u); ctx.fpr[0] = ctx.fpr[20] + ctx.fpr[23]; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); ctx.fpr[23] = float_at(rt, frame.at(0x2Cu)); ctx.fpr[22] = float_at(rt, frame.at(0x28u)); ctx.fpr[21] = float_at(rt, frame.at(0x24u)); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } } }
void func_08828BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(4u); put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; G[4] = G[19]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); G[17] = G[6]; call_here(rt, ctx, 0x088248E4u); G[4] = 0x08C99B4Cu; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[19]; G[19] = word(rt, G[16] + 0x14u); call_here(rt, ctx, 0x08A5B9ECu); if (static_cast<std::int32_t>(G[17]) > 0) { G[16] = word(rt, G[18]); G[4] = 0x08C99B24u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2] != 0u ? 1u : 0u; G[4] = G[19]; call_here(rt, ctx, 0x0885C368u); } else { G[4] = G[19]; call_here(rt, ctx, 0x0885C374u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[20]; if (G[20] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08830268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); G[4] = 0x08C9A2B4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[20]; G[5] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; call_here(rt, ctx, 0x0885EDF8u); G[16] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x088D6D58u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// 0x0883046C (a0 = the destination, a2 = the count, a3 = the source): when a2 > 0, builds a slot at frame+4 with
// 0x088297A8(frame+4), releases it with 0x088294D0 and takes it with 0x0885EDEC (asserting 0x08C9A2F8 when the slot is
// null), then calls the virtual slot +0x3C of word(a3) (asserting 0x08C9A2B4 when null) and 0x088DA9E0 with the result.
// When that returns >= 0, 0x08A85B1C stores the frame+0 text and 0x08A5B9EC releases it (0x08A5BAD8 when the destination
// is not the frame). v0 = the last call's result.
void func_0883046C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], count = ctx.gpr[6], source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {19u, 0x1Cu}, {18u, 0x18u}, {31u, 0x20u}, {16u, 0x10u}});
    ctx.gpr[17] = frame.at(4u);
    ctx.gpr[19] = destination;
    ctx.gpr[4] = ctx.gpr[17];
    ctx.gpr[18] = source;
    if (static_cast<s32>(count) > 0) {
        call_set(rt, ctx, 0x088297A8u, {ctx.gpr[17], ctx.gpr[5]});
        const u32 slot = word(rt, frame.at(4u));
        ctx.gpr[16] = slot;
        if (slot == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
        call_set(rt, ctx, 0x088294D0u, {slot});
        ctx.gpr[16] = ctx.gpr[2];
        call_set(rt, ctx, 0x08A5B9ECu, {ctx.gpr[17]});
        call_set(rt, ctx, 0x0885EDECu, {ctx.gpr[16]});
        const u32 held = word(rt, source);
        ctx.gpr[16] = held;
        ctx.gpr[17] = ctx.gpr[2];
        if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        const u32 target = word(rt, word(rt, held) + 0x3Cu);
        call_set(rt, ctx, target, {held});
        call_set(rt, ctx, 0x088DA9E0u, {ctx.gpr[17], ctx.gpr[2]});
        const u32 result = ctx.gpr[2];
        ctx.gpr[4] = frame.at(0u);
        if (static_cast<s32>(result) >= 0) {
            call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), result});
            if (destination != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
    }
}
void func_08833574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x1Cu), G[19]); G[19] = G[4]; G[4] = G[17]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885EE70u); G[4] = G[2]; call_here(rt, ctx, 0x08891644u); G[4] = G[2]; G[18] = G[2]; call_here(rt, ctx, 0x088920A0u); G[17] = G[2]; G[4] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { call_here(rt, ctx, 0x08B2B5E0u); G[4] = G[18]; G[16] = G[2]; call_here(rt, ctx, 0x088914A0u); G[5] = G[2]; G[4] = G[16]; G[6] = G[17]; call_here(rt, ctx, 0x08B79A88u); G[4] = frame.at(0u); G[5] = G[16]; G[6] = G[17]; G[7] = 1u; call_here(rt, ctx, 0x08A44D9Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08834104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x24u), G[21]); G[21] = G[4]; G[4] = G[20]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[7]; put(rt, frame.at(0x14u), G[17]); G[17] = G[6]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x18u), G[18]); call_here(rt, ctx, 0x088297A8u); G[4] = 0x08C9A2F8u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[20]; G[18] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0u; if (static_cast<std::int32_t>(G[17]) > 0) { G[16] = word(rt, G[19]); G[4] = 0x08C9A2B4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); } G[4] = G[18]; G[5] = G[2]; call_here(rt, ctx, 0x0885E22Cu); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[21]; if (G[21] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0884C8E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x0884C868u); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08C9D094u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08C9D08Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); store16(G[17] + 0x22u, G[2]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x0884C868u); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08C9D094u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = sext(load16(G[16] + 0x22u), 16); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0887A518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); G[16] = frame.at(8u); put(rt, frame.at(0x24u), G[17]); G[17] = G[4]; put(rt, frame.at(0x28u), G[31]); G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[6] = G[2]; G[2] = word(rt, frame.at(0x10u)); G[4] = word(rt, frame.at(8u)); if (static_cast<std::int32_t>(G[2]) > 0) { G[5] = word(rt, G[17]); G[4] = frame.at(4u); call_here(rt, ctx, 0x0886C798u); G[4] = word(rt, G[17]); G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x0886B180u); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D70000u; G[17] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] == G[17]) { G[4] = word(rt, frame.at(8u)); } else { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); const bool kept = static_cast<std::int32_t>(G[2]) > 0; G[4] = word(rt, frame.at(8u)); if (!kept) { G[4] = G[16]; G[5] = frame.at(1u); call_here(rt, ctx, 0x08B93554u); G[4] = word(rt, frame.at(8u)); } } G[16] = G[4] - 0xCu; if (G[17] == G[16]) goto done; G[4] -= 4u; } else { G[2] = 0x08D75298u; G[16] = G[4] - 0xCu; if (G[16] == G[2]) goto done; G[4] -= 4u; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } done: G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } }
void func_08890330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; G[5] = 0u; put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; G[4] = frame.at(0u); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); G[20] = G[7]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[6]; put(rt, frame.at(0x18u), G[18]); call_here(rt, ctx, 0x088A79ACu); G[4] = G[16]; G[5] = G[17]; G[6] = frame.at(0u); call_here(rt, ctx, 0x088902ECu); G[4] = G[16]; G[5] = G[17]; if (G[2] == 0u) goto fail; call_here(rt, ctx, 0x0888FEF0u); G[18] = G[2]; G[4] = G[16]; G[5] = G[17]; if (G[2] == 0u) goto fail; G[17] = 1u; call_here(rt, ctx, 0x0888FE78u); G[2] = static_cast<std::int32_t>(G[19]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[3] = G[19] << 4; G[4] = frame.at(0u); if (G[2] == 0u) goto fail; G[2] = word(rt, G[18] + 0x14u); G[2] = __builtin_bswap32(G[2]); G[2] += G[3]; G[16] = word(rt, G[2]); call_here(rt, ctx, 0x088A5EE0u); G[6] = G[2]; G[16] = __builtin_bswap32(G[16]); G[4] = G[20]; G[5] = G[16]; G[7] = G[3]; call_here(rt, ctx, 0x08897F60u); goto close; fail: G[17] = 0u; close: G[4] = frame.at(0u); call_here(rt, ctx, 0x088A7548u); G[2] = G[17]; G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_088BE160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[16] = word(rt, G[4] + 0x24u); G[5] = word(rt, G[4] + 0x1Cu); G[4] = frame.at(0u); G[6] = load8(G[16] + 0x10u); call_here(rt, ctx, 0x088C50B8u); G[4] = G[16]; G[17] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x088BB57Cu); G[5] = load8(G[16] + 0x11u); G[4] = frame.at(0u); G[5] -= 1u; G[3] = G[5] + G[17]; G[2] = static_cast<std::int32_t>(G[5]) < 0 ? 1u : 0u; if (G[2] != 0u) G[5] = G[3]; call_here(rt, ctx, 0x088C50C4u); G[3] = sext(load8(G[2] + 2u), 8); G[3] = static_cast<std::int32_t>(G[3]) < -0x7E ? 1u : 0u; G[4] = frame.at(0u); if (G[3] == 0u) { G[2] = load8(G[16] + 0x13u); ctx.fpr[0] = float_at(rt, G[16] + 0x1Cu); store8(G[16] + 0x12u, G[2]); put_float(rt, G[16] + 0x18u, ctx.fpr[0]); } G[5] = load8(G[16] + 0x11u); G[2] = G[17] ^ G[5]; if (G[2] == 0u) G[5] = 0u; call_here(rt, ctx, 0x088C5324u); G[5] = G[2]; G[4] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x088C50C4u); G[3] = load8(G[2]); G[2] = G[3] < 3u ? 1u : 0u; store8(G[16] + 0x13u, G[3]); if (G[3] == 0u) { G[2] = word(rt, G[18] + 4u); ctx.fpr[0] = float_at(rt, G[2] + 0x1360u); put_float(rt, G[16] + 0x1Cu, ctx.fpr[0]); } else if (G[2] != 0u) { G[4] = G[18]; G[5] = G[17]; call_here(rt, ctx, 0x088BDEE0u); put_float(rt, G[16] + 0x1Cu, ctx.fpr[0]); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_089668D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); G[20] = G[8]; put(rt, frame.at(0xCu), G[19]); G[19] = G[7]; put(rt, frame.at(8u), G[18]); G[18] = G[6]; put(rt, frame.at(4u), G[17]); G[17] = G[5]; put(rt, frame.at(0u), G[16]); put(rt, frame.at(0x14u), G[31]); G[16] = G[4]; call_here(rt, ctx, 0x08966174u); G[4] = 0u; G[17] = static_cast<std::int32_t>(G[17]) > 0 ? G[17] : 0u; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x80u); G[20] = static_cast<std::int32_t>(G[20]) > 0 ? G[20] : 0u; G[18] = static_cast<std::int32_t>(G[18]) > 0 ? G[18] : 0u; G[2] = sext(load16(G[3] + 0x28u), 16); G[19] = static_cast<std::int32_t>(G[19]) > 0 ? G[19] : 0u; const std::uint32_t values[4] = {G[17], G[18], G[19], G[20]}; for (std::uint32_t i = 0u; i < 4u; ++i) { const std::uint32_t offset = 0x28u + i * 2u; G[2] = sext(load16(G[3] + offset), 16); if (values[i] != G[2]) { store16(G[3] + offset, values[i]); G[2] = word(rt, G[16] + 0xE4u); if (i < 3u) G[3] = word(rt, G[16] + 0x80u); G[2] |= 4u; put(rt, G[16] + 0xE4u, G[2]); } } } G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_089860D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = frame.at(4u); put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; G[5] = G[18] + 0x30u; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); G[20] = G[6]; put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); call_here(rt, ctx, 0x08B88F44u); G[2] = frame.at(8u); G[4] = word(rt, G[17]); G[16] = word(rt, frame.at(4u)); G[8] = frame.at(0xCu); G[7] = word(rt, G[4] - 0xCu); G[5] = G[16]; G[19] = G[16] - 0xCu; put(rt, frame.at(8u), G[7]); G[3] = word(rt, G[16] - 0xCu); G[6] = G[3] < G[7] ? 1u : 0u; if (G[6] == 0u) G[8] = G[2]; put(rt, frame.at(0xCu), G[3]); G[17] = G[7] - G[3]; G[6] = word(rt, G[8]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[17] = G[2]; G[2] = 0x08D74FD0u; G[4] = G[16] - 4u; G[5] = 0xFFFFFFFFu; if (G[19] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[19]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[18]; G[5] = G[20]; G[2] = 0u; if (G[17] == 0u) { G[2] = word(rt, G[18]); G[3] = word(rt, G[2] + 8u); call_here(rt, ctx, G[3]); G[2] = 1u; } G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_089A361C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); G[6] = static_cast<std::int32_t>(G[6]) < 2 ? 1u : 0u; put(rt, frame.at(0x24u), G[17]); G[4] = frame.at(0u); G[17] = G[7]; put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put_float(rt, frame.at(0x30u), ctx.fpr[20]); if (G[6] == 0u) { call_here(rt, ctx, 0x089A354Cu); G[18] = word(rt, frame.at(0u)); G[4] = 0x08CA4D28u; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[17]); G[4] = 0x08CA4BF4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16]); G[3] = word(rt, G[2] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[3]); G[16] = word(rt, G[17] + 4u); G[4] = 0x08CA4BF4u; ctx.fpr[20] = ctx.fpr[0]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[4] = G[18]; G[5] = frame.at(0x10u); G[6] = frame.at(0xCu); G[7] = frame.at(8u); G[8] = frame.at(4u); put_float(rt, frame.at(8u), ctx.fpr[20]); put_float(rt, frame.at(4u), ctx.fpr[0]); put(rt, frame.at(0x10u), 0u); put(rt, frame.at(0xCu), 0u); call_here(rt, ctx, 0x0896A9C4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); ctx.fpr[20] = float_at(rt, frame.at(0x30u)); } } }
void func_089C0450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); if (static_cast<std::int32_t>(G[6]) > 0) { G[18] = frame.at(8u); G[4] = G[18]; call_here(rt, ctx, 0x089BEE4Cu); G[17] = word(rt, frame.at(8u)); if (G[17] == 0u) { G[4] = 0x08CA5EB4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16]); G[17] += 0x22Cu; if (G[16] == 0u) { G[4] = 0x08CA5E6Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x1Cu, G[2]); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); call_here(rt, ctx, 0x089BEE4Cu); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CA5EB4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[5] = word(rt, G[16] + 0x248u); G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(4u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A5B4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[2] = G[4]; G[20] = G[7]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[2] + 0x38u; put(rt, frame.at(0x18u), G[18]); G[18] = G[6]; put(rt, frame.at(0x14u), G[17]); G[17] = G[5]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x10u), G[16]); G[16] = word(rt, G[4] + 0x38u); G[4] = 0x08CAFB04u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x120u); G[4] = G[16]; call_here(rt, ctx, G[2]); { const std::int64_t p = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[18]); ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32); } G[2] = word(rt, G[17] + 0x1Cu); G[16] = ctx.lo; G[7] = G[2] + G[16]; if (G[2] == 0u) { G[18] = word(rt, G[17] + 0x18u); G[4] = 0x08CAFB0Cu; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[18] + 0xCu); G[7] = G[2] + G[16]; } G[4] = frame.at(0u); G[6] = G[17] + 0x18u; G[5] = G[19]; call_here(rt, ctx, 0x08A57B54u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAFBB4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x08A56824u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08A6B798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = G[5]; put(rt, frame.at(0x10u), G[20]); G[20] = G[6]; put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; put(rt, frame.at(4u), G[17]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(0u), G[16]); G[3] = word(rt, G[4] + 0x1Cu); G[16] = word(rt, G[4] + 0x20u); G[2] = word(rt, G[3] - 0xCu); G[2] = G[16] < G[2] ? 1u : 0u; G[17] = G[4] + 0x1Cu; if (G[2] == 0u) { G[2] = 1u; G[18] = 0u; put(rt, G[4] + 0x14u, G[2]); G[2] = G[18]; goto done; } G[18] = 0u; if (G[6] == 0u) { G[2] = G[18]; goto done; } for (;;) { G[2] = G[16] + 1u; put(rt, G[19] + 0x20u, G[2]); G[3] = word(rt, G[17]); G[2] = word(rt, G[3] - 4u); G[4] = G[17]; if (static_cast<std::int32_t>(G[2]) >= 0) { call_here(rt, ctx, 0x08B88FF8u); G[3] = word(rt, G[17]); } G[2] = G[3] + G[16]; G[3] = load8(G[2]); G[4] = G[21] + G[18]; G[18] += 1u; store8(G[4], G[3]); if (G[20] == G[18]) { G[2] = G[18]; break; } G[3] = word(rt, G[17]); G[16] = word(rt, G[19] + 0x20u); G[2] = word(rt, G[3] - 0xCu); G[2] = G[16] < G[2] ? 1u : 0u; { const bool more = G[2] != 0u; G[2] = G[18]; if (!more) break; } } done: G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08A7FBE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); G[20] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; G[4] = G[20]; put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(8u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); G[17] = frame.at(0xCu); call_here(rt, ctx, 0x08A7C3A4u); G[4] = 0x08CB1360u; G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[7] = 0x08CB14B4u; G[5] = G[16]; G[6] = 0u; G[4] = G[19]; call_here(rt, ctx, 0x08A7F8E8u); G[4] = G[17]; G[5] = G[19]; call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(0xCu)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// Builds a display string from the argument's +0x14 member: starts a builder on the frame buffer with the
// argument and the literal at 0x08CB18F8, renders the member (asserting it is non-null), appends it and the
// literal at 0x08CB191C, finishes the builder and passes the result to 0x08B88F44 with the target object.
// The frame string is released on the way out; returns the object.
void func_08A86E70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {19u, 0x1Cu}, {31u, 0x20u}, {17u, 0x14u}, {16u, 0x10u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = text;
    ctx.gpr[19] = object;
    ctx.gpr[16] = argument;
    call_set(rt, ctx, 0x08AAB5DCu, {text, argument});
    const u32 beginning = call_set(rt, ctx, 0x08AABA8Cu, {text, 0x08CB18F8u});
    ctx.gpr[17] = beginning;
    const u32 member = word(rt, argument + 0x14u);
    ctx.gpr[16] = member;
    if (member == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB190Cu});
    const u32 rendered = call_set(rt, ctx, 0x08A5D454u, {member});
    const u32 with_member = call_set(rt, ctx, 0x08AABA8Cu, {beginning, rendered});
    const u32 complete = call_set(rt, ctx, 0x08AABA8Cu, {with_member, 0x08CB191Cu});
    const u32 finished = call_set(rt, ctx, 0x08AAB540u, {text});
    call_set(rt, ctx, 0x08B88F44u, {object, finished});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    ctx.gpr[2] = object;
}
void func_08AC39B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x180u); put(rt, frame.at(0x170u), G[20]); G[20] = G[7]; put(rt, frame.at(0x16Cu), G[19]); G[19] = G[6]; put(rt, frame.at(0x168u), G[18]); G[18] = G[4] + 0x18Cu; put(rt, frame.at(0x164u), G[17]); G[17] = G[4]; put(rt, frame.at(0x160u), G[16]); G[16] = G[5]; put(rt, frame.at(0x174u), G[31]); store8(G[6], 0u); call_here(rt, ctx, 0x08AC38ACu); G[4] = G[18]; G[3] = 0u; if (static_cast<std::int32_t>(G[2]) < 0) goto done; if (static_cast<std::int32_t>(G[2]) > 0) { divide(ctx, G[16], G[2]); G[16] = ctx.hi; } call_here(rt, ctx, 0x08B344E0u); G[4] = G[17]; G[5] = frame.at(0u); G[6] = G[16]; put(rt, frame.at(0x158u), 0u); call_here(rt, ctx, 0x08AC35F0u); { const bool none = G[2] == 0u; G[2] = word(rt, frame.at(0u)); if (none) goto unlock; } G[16] = frame.at(0x58u); G[2] &= 0x2000u; G[4] = G[16]; if (G[2] == 0u) goto unlock; call_here(rt, ctx, 0x08B35F60u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[20]) ? 1u : 0u; G[5] = G[16]; G[4] = G[19]; if (G[2] == 0u) goto unlock; call_here(rt, ctx, 0x08B7BB6Cu); G[4] = G[18]; call_here(rt, ctx, 0x08B3457Cu); G[3] = 1u; goto done; unlock: G[4] = G[18]; call_here(rt, ctx, 0x08B3457Cu); G[3] = 0u; done: G[20] = word(rt, frame.at(0x170u)); G[19] = word(rt, frame.at(0x16Cu)); G[18] = word(rt, frame.at(0x168u)); G[17] = word(rt, frame.at(0x164u)); G[16] = word(rt, frame.at(0x160u)); G[2] = G[3]; } } }
void func_08ADABE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); G[21] = G[5]; G[5] = 0x08C90CDCu; put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); G[19] = G[6]; put(rt, frame.at(4u), G[17]); G[17] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08AD9E74u); G[20] = G[2]; G[2] = 0u; if (G[20] == 0u) goto done; G[18] = 0u; if (static_cast<std::int32_t>(G[19]) > 0) { G[22] = 1u; do { G[4] = G[17]; call_here(rt, ctx, 0x08ADAAC8u); G[3] = G[19] - G[18]; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[4] = G[17]; G[16] = G[3]; if (G[2] == 0u) { call_here(rt, ctx, 0x08ADAAC8u); G[16] = G[2]; } G[4] = G[17]; if (static_cast<std::int32_t>(G[16]) <= 0) break; call_here(rt, ctx, 0x08ADA968u); G[4] = G[2]; G[5] = G[21] + G[18]; G[6] = G[16]; call_here(rt, ctx, 0x08B79A88u); G[18] += G[16]; G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08ADA778u); G[3] = static_cast<std::int32_t>(G[18]) < static_cast<std::int32_t>(G[19]) ? 1u : 0u; store8(G[20] + 0x15u, G[22]); } while (G[3] != 0u); } G[2] = G[18]; done: G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_08AE4994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(8u), G[18]); G[2] = 0x08D40000u; G[18] = G[5]; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[6] = word(rt, 0x08D40938u); G[3] = word(rt, G[4] + 0x10u); G[4] = word(rt, G[6] + 8u); G[2] = word(rt, G[3] + 0x10u); G[3] = G[6]; G[19] = sext(load16(G[2] + 0x12u), 16); if (static_cast<std::int32_t>(G[4]) < 0) G[3] = 0u; G[17] = G[3]; if (G[3] == 0u) goto none; outer: G[2] = word(rt, G[17] + 0x10u); G[3] = sext(load16(G[2] + 0x12u), 16); if (G[3] == G[19]) { G[3] = word(rt, G[17] + 0x18u); G[16] = 0u; G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[16] = G[3]; if (G[16] != 0u) { do { G[4] = sext(load16(G[16] + 0x1Au), 16); G[5] = G[18]; call_here(rt, ctx, 0x08AE8684u); G[2] &= 1u; G[3] = 1u; if (G[2] != 0u) goto done; G[3] = word(rt, G[16] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[16] = G[3]; } while (G[3] != 0u); } } G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[3] + 8u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] != 0u) G[3] = 0u; G[17] = G[3]; if (G[3] != 0u) goto outer; none: G[3] = 0u; done: G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); G[2] = G[3]; } } }
void func_08AFA88C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); G[22] = G[5]; put(rt, frame.at(0x24u), G[21]); G[21] = G[6]; put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; put(rt, frame.at(0x1Cu), G[19]); G[19] = G[10]; put(rt, frame.at(0x18u), G[18]); G[18] = G[9]; put(rt, frame.at(0x14u), G[17]); G[17] = G[7]; put(rt, frame.at(0x10u), G[16]); G[2] = word(rt, G[4]); G[3] = word(rt, G[2] + 0x14u); G[16] = G[8]; call_here(rt, ctx, G[3]); G[2] = static_cast<std::int32_t>(G[16]) < 2 ? 1u : 0u; G[3] = G[16]; if (G[2] != 0u) G[3] = 0u; put(rt, G[20] + 0x28u, G[3]); G[8] = G[17]; G[9] = G[16]; put(rt, G[20] + 0xCu, G[17]); G[4] = G[20] + 0x2Cu; G[5] = G[22]; G[6] = G[21]; G[7] = 0u; G[11] = G[18]; G[10] = 0u; put(rt, frame.at(0u), G[19]); call_here(rt, ctx, 0x08B52B70u); G[16] = G[2] != 0u ? 1u : 0u; G[4] = G[20]; G[7] = G[22]; G[8] = G[21]; G[5] = 0u; G[6] = 0u; if (G[16] != 0u) { G[3] = word(rt, G[20]); put(rt, G[20] + 4u, G[22]); put(rt, G[20] + 8u, G[21]); G[2] = word(rt, G[3] + 0x34u); call_here(rt, ctx, G[2]); } G[2] = G[16]; G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_238
} // namespace gt::game

#define HOT238(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_238::name, gt::ReturnKind::kind)
// HOT238(0x0880E0F8u, func_0880E0F8, Float);  // no comparable synthetic case yet
// HOT238(0x08828BA4u, func_08828BA4, Word);  // no comparable synthetic case yet
HOT238(0x08830268u, func_08830268, Word);
HOT238(0x0883046Cu, func_0883046C, Word);
// HOT238(0x08833574u, func_08833574, Word);  // no comparable synthetic case yet
// HOT238(0x08834104u, func_08834104, Word);  // no comparable synthetic case yet
// HOT238(0x0884C8E8u, func_0884C8E8, Word);  // no comparable synthetic case yet
HOT238(0x0887A518u, func_0887A518, Word);
HOT238(0x08890330u, func_08890330, Word);
// HOT238(0x088BE160u, func_088BE160, Word);  // no comparable synthetic case yet
HOT238(0x089668D8u, func_089668D8, Word);
HOT238(0x089860D4u, func_089860D4, Word);
HOT238(0x089A361Cu, func_089A361C, Word);
// HOT238(0x089C0450u, func_089C0450, Word);  // no comparable synthetic case yet
HOT238(0x08A5B4D8u, func_08A5B4D8, Word);
HOT238(0x08A6B798u, func_08A6B798, Word);
// HOT238(0x08A7FBE8u, func_08A7FBE8, Word);  // no comparable synthetic case yet
HOT238(0x08A86E70u, func_08A86E70, Word);
HOT238(0x08AC39B0u, func_08AC39B0, Word);
HOT238(0x08ADABE0u, func_08ADABE0, Word);
HOT238(0x08AE4994u, func_08AE4994, Word);
HOT238(0x08AFA88Cu, func_08AFA88C, Word);
