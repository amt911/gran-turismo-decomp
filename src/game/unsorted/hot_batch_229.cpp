// Hot batch 229: sixty-three- to eighty-six-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_229 {
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
    if (!(value >= -2297483648.0f && value < 2297483648.0f)) return 0x7FFFFFFF;
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

void func_0881F1DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); G[18] = G[7]; G[17] = G[4]; if (static_cast<std::int32_t>(G[6]) > 0) { G[19] = frame.at(8u); G[4] = G[19]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C98EFCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x20u, G[2]); } else { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = word(rt, G[16] + 0x20u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); } }
// 0x0880AFAC (a0 = the object): the +0x38 holder replacement with the format 0x08C98334, built at frame +4 by
// 0x08B85EB8(text, 0x08C98334, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08965B18() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x0880B53C) and the four registrations 0x08B87108(object,
// 0x08C9839C, 0x0880AD64), 0x08B87108(object, 0x08C983A4, 0x0880B2D8), 0x08B87108(object, 0x08C983AC, 0x0880AE0C) and
// 0x08B87108(object, 0x08C983B0, 0x0880B0F8) run.
void func_0880AFAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08C98334u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08965B18u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x0880B53Cu});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08C9839Cu, 0x0880AD64u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08C983A4u, 0x0880B2D8u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08C983ACu, 0x0880AE0Cu});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08C983B0u, 0x0880B0F8u});
}
void func_08A1917C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[17] = frame.at(8u); G[19] = G[4]; G[4] = G[17]; G[18] = frame.at(4u); G[16] = G[5]; call_here(rt, ctx, 0x08AAB5DCu); G[5] = 0x08CA9CB0u; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A5D454u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[2]; G[5] = 0x08CA9CBCu; call_here(rt, ctx, 0x08AABA8Cu); G[2] = word(rt, G[16] + 0x14u); G[4] = G[18]; G[5] = 0x08CA9CC0u; if (G[2] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[3] = 0x08D70000u; G[18] = 0x08D74FD0u; } else { G[3] = word(rt, G[2]); G[5] = G[2]; G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x08AAB710u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D70000u; G[18] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[5] = 0x08CA9CC8u; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[17]; call_here(rt, ctx, 0x08AAB540u); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08B88F44u); G[3] = word(rt, frame.at(8u)); G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[19]; G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A37998(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CAD1C4u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08A37C7Cu; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CAD1D0u; G[6] = 0x08A37634u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD1D8u; G[6] = 0x08A3763Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD1E0u; G[6] = 0x08A37644u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD1E8u; G[6] = 0x08A3764Cu; call_here(rt, ctx, 0x08B87108u); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
// 0x08A39FA4 (a0 = the object): the +0x38 holder replacement with the format 0x08CAD480, built at frame +4 by
// 0x08B85EB8(text, 0x08CAD480, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A76A94() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x08A39D6C) and the four registrations 0x08B87108(object,
// 0x08CAD4B4, 0x08A39C4C), 0x08B87108(object, 0x08CAD4BC, 0x08A3A1F8), 0x08B87108(object, 0x08CAD4C0, 0x08A3A0F0) and
// 0x08B87108(object, 0x08CAD4C4, 0x08A39B70) run.
void func_08A39FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAD480u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A39D6Cu});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAD4B4u, 0x08A39C4Cu});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAD4BCu, 0x08A3A1F8u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAD4C0u, 0x08A3A0F0u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAD4C4u, 0x08A39B70u});
}
void func_08A7481C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[17] = frame.at(8u); G[19] = G[4]; G[4] = G[17]; G[18] = frame.at(4u); G[16] = G[5]; call_here(rt, ctx, 0x08AAB5DCu); G[5] = 0x08CB0C40u; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A5D454u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[2]; G[5] = 0x08CB0C50u; call_here(rt, ctx, 0x08AABA8Cu); G[2] = word(rt, G[16] + 0x14u); G[4] = G[18]; G[5] = 0x08CB0C54u; if (G[2] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[3] = 0x08D70000u; G[18] = 0x08D74FD0u; } else { G[3] = word(rt, G[2]); G[5] = G[2]; G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x08AAB710u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D70000u; G[18] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[5] = 0x08CB0C5Cu; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[17]; call_here(rt, ctx, 0x08AAB540u); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08B88F44u); G[3] = word(rt, frame.at(8u)); G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[19]; G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A7B770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[17] = frame.at(8u); G[19] = G[4]; G[4] = G[17]; G[18] = frame.at(4u); G[16] = G[5]; call_here(rt, ctx, 0x08AAB5DCu); G[5] = 0x08CB12E0u; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A5D454u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[2]; G[5] = 0x08CB12F0u; call_here(rt, ctx, 0x08AABA8Cu); G[2] = word(rt, G[16] + 0x14u); G[4] = G[18]; G[5] = 0x08CB12F4u; if (G[2] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[3] = 0x08D70000u; G[18] = 0x08D74FD0u; } else { G[3] = word(rt, G[2]); G[5] = G[2]; G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x08AAB710u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D70000u; G[18] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[5] = 0x08CB12FCu; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[17]; call_here(rt, ctx, 0x08AAB540u); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08B88F44u); G[3] = word(rt, frame.at(8u)); G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[19]; G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A91F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[17] = frame.at(8u); G[19] = G[4]; G[4] = G[17]; G[18] = frame.at(4u); G[16] = G[5]; call_here(rt, ctx, 0x08AAB5DCu); G[5] = 0x08CB1EF4u; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A5D454u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[2]; G[5] = 0x08CB1F04u; call_here(rt, ctx, 0x08AABA8Cu); G[2] = word(rt, G[16] + 0x14u); G[4] = G[18]; G[5] = 0x08CB1F08u; if (G[2] == 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[3] = 0x08D70000u; G[18] = 0x08D74FD0u; } else { G[3] = word(rt, G[2]); G[5] = G[2]; G[2] = word(rt, G[3] + 0xCu); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x08AAB710u); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D70000u; G[18] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[5] = 0x08CB1F10u; G[4] = G[17]; call_here(rt, ctx, 0x08AABA8Cu); G[4] = G[17]; call_here(rt, ctx, 0x08AAB540u); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08B88F44u); G[3] = word(rt, frame.at(8u)); G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[19]; G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0881F2D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); G[18] = G[7]; G[17] = G[4]; if (static_cast<std::int32_t>(G[6]) > 0) { G[19] = frame.at(8u); G[4] = G[19]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C98EFCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x24u, G[2]); } else { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = word(rt, G[16] + 0x24u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0881F3D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); G[18] = G[7]; G[17] = G[4]; if (static_cast<std::int32_t>(G[6]) > 0) { G[19] = frame.at(8u); G[4] = G[19]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C98EFCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x28u, G[2]); } else { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = word(rt, G[16] + 0x28u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0881F4D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); G[18] = G[7]; G[17] = G[4]; if (static_cast<std::int32_t>(G[6]) > 0) { G[19] = frame.at(8u); G[4] = G[19]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); if (G[16] == 0u) { G[4] = 0x08C98EFCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); put(rt, G[17] + 0x2Cu, G[2]); } else { G[18] = frame.at(4u); G[4] = G[18]; call_here(rt, ctx, 0x0881BD98u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C98F48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = word(rt, G[16] + 0x2Cu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(0u)); if (G[17] != frame.at(0u)) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); } }
// 0x08A37258 (a0 = the object): the +0x38 holder replacement with the format 0x08CAD0CC, built at frame +4 by
// 0x08B85EB8(text, 0x08CAD0CC, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A76A94() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x08A370FC), 0x08B8A004(object, 0x08CF6F24, 0x08A36DD8) and the
// three registrations 0x08B87108(object, 0x08CAD100, 0x08A37510), 0x08B87108(object, 0x08CAD108, 0x08A37010) and
// 0x08B87108(object, 0x08CAD110, 0x08A373A4) run.
void func_08A37258(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAD0CCu, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A370FCu});
    call_set(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x08A36DD8u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAD100u, 0x08A37510u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAD108u, 0x08A37010u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAD110u, 0x08A373A4u});
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_229
} // namespace gt::game

#define HOT229(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_229::name, gt::ReturnKind::kind)
// HOT229(0x0881F1DCu, func_0881F1DC, Word);  // no comparable synthetic case yet
HOT229(0x0880AFACu, func_0880AFAC, Word);
HOT229(0x08A1917Cu, func_08A1917C, Word);
HOT229(0x08A37998u, func_08A37998, Word);
HOT229(0x08A39FA4u, func_08A39FA4, Word);
// HOT229(0x08A7481Cu, func_08A7481C, Word);  // no comparable synthetic case yet
HOT229(0x08A7B770u, func_08A7B770, Word);
HOT229(0x08A91F88u, func_08A91F88, Word);
// HOT229(0x0881F2D8u, func_0881F2D8, Word);  // no comparable synthetic case yet
// HOT229(0x0881F3D4u, func_0881F3D4, Word);  // no comparable synthetic case yet
// HOT229(0x0881F4D0u, func_0881F4D0, Word);  // no comparable synthetic case yet
HOT229(0x08A37258u, func_08A37258, Word);
