// Hot batch 228: seventy-four- to ninety-four-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_228 {
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
    if (!(value >= -2287483648.0f && value < 2287483648.0f)) return 0x7FFFFFFF;
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

// 0x089B3BB0 (a0 = the object): the +0x38 holder replacement with the format 0x08CA567C, built at frame +4 by
// 0x08B85EB8(text, 0x08CA567C, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A003B4() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x089B4158) and the three registrations
// 0x08B87108(object, 0x08CA56C0, 0x089B3DC0), 0x08B87108(object, 0x08CA56D4, 0x089B3880) and 0x08B87108(object, 0x08CA56E0, 0x089B3CE4) run.
// 0x089B3BB0 (a0 = the object): the +0x38 holder replacement with the format 0x08CA567C, built at frame +4 by
// 0x08B85EB8(text, 0x08CA567C, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A003B4() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x089B4158) and the three registrations
// 0x08B87108(object, 0x08CA56C0, 0x089B3DC0), 0x08B87108(object, 0x08CA56D4, 0x089B3880) and 0x08B87108(object, 0x08CA56E0, 0x089B3CE4) run.
// 0x089B3BB0 (a0 = the object): the +0x38 holder replacement with the format 0x08CA567C, built at frame +4 by
// 0x08B85EB8(text, 0x08CA567C, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A003B4() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x089B4158) and the three registrations
// 0x08B87108(object, 0x08CA56B4, 0x089B3DC0), 0x08B87108(object, 0x08CA56C0, 0x089B3880) and 0x08B87108(object, 0x08CA56D4, 0x089B3CE4) run.
void func_089B3BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA567Cu, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A003B4u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x089B4158u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CA56B4u, 0x089B3DC0u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CA56C0u, 0x089B3880u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CA56D4u, 0x089B3CE4u});
}
void func_08ACB1C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[31]); G[2] = 1u; G[8] = G[4]; if (G[4] == G[2]) { G[2] = 0xFFFFu; if (G[5] == G[2]) { G[2] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[6] = G[4] + 1u; G[3] = G[6] < G[4] ? 1u : 0u; G[7] = G[3] + G[5]; put(rt, 0x08D771C0u, G[6]); put(rt, 0x08D771C4u, G[7]); if (G[6] == G[8] && G[7] == 0u) { G[5] = 0x08D70000u; G[4] = 0x08D77204u; call_here(rt, ctx, 0x08C3D5A8u); } G[5] = 0x08D70000u; G[2] = word(rt, 0x08D771B8u); G[3] = word(rt, 0x08D771BCu); G[6] = G[2] + 1u; G[4] = G[6] < G[2] ? 1u : 0u; G[7] = G[4] + G[3]; G[2] = 1u; put(rt, 0x08D771B8u, G[6]); put(rt, 0x08D771BCu, G[7]); if (G[6] == G[2] && G[7] == 0u) { G[4] = 0x08D771D0u; tail = 0x08C3D5D8u; } } } else { G[2] = 0xFFFFu; if (G[4] == 0u && G[5] == G[2]) { G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771B8u); G[5] = word(rt, 0x08D771BCu); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771B8u, G[2]); put(rt, 0x08D771BCu, G[3]); if (G[4] == 0u) { G[4] = 0x08D771D0u; call_here(rt, ctx, 0x08C3D628u); } G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771C0u, G[2]); put(rt, 0x08D771C4u, G[3]); if (G[4] == 0u) { G[4] = 0x08D77204u; tail = 0x08C3D638u; } } }  } if (tail != 0u) jump(ctx, tail); }
void func_08B97DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 1u; G[3] = load8(G[4] + 0x25u); G[16] = word(rt, G[5] + 4u); G[5] = word(rt, G[5]); G[2] -= G[3]; G[2] <<= 3; G[2] += G[4]; G[4] = G[2] + 0x14u; if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[2] = word(rt, G[4] + 4u); put(rt, G[4], G[5]); G[18] = G[4] + 4u; if (G[16] != G[2]) { if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[17] = word(rt, G[18]); if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[17]; call_here(rt, ctx, G[2]); } } } put(rt, G[18], G[16]); } if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail != 0u) jump(ctx, tail); }
void func_08BAF270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); G[18] = G[5]; G[17] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (G[17] != 0u) { G[3] = G[17] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[5] = word(rt, G[16] + 0x34u); G[2] = 1u; store8(G[16] + 0xCu, G[2]); G[3] = G[5] & 1u; store8(G[16] + 0xEu, 0u); G[6] = word(rt, G[16] + 0x28u); if (G[3] != 0u) { G[2] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x30u); G[3] += G[4]; G[3] = word(rt, G[3]); } else { G[3] = word(rt, G[16] + 0x30u); } G[4] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); call_here(rt, ctx, G[3]); G[2] = word(rt, G[18] + 0xA8u); bool skip; if (G[2] != 0u) { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } else { G[2] = load8(G[16] + 0xDu); if (G[2] != 0u) skip = true; else { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } } if (!skip) { G[4] = word(rt, G[16] + 8u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AC05F4u); put(rt, G[16] + 8u, 0u); } if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[17]); G[4] = G[17]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } if (tail != 0u) jump(ctx, tail); }
void func_08A61310(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CB01F0u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08A60F70u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CB01FCu; G[6] = 0x08A60FB0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB0208u; G[6] = 0x08A614A8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB0210u; G[6] = 0x08A611C0u; call_here(rt, ctx, 0x08B87108u); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); } }
// 0x08A69208 (a0 = the object): the +0x38 holder replacement with the format 0x08CB0428, built at frame +4 by
// 0x08B85EB8(text, 0x08CB0428, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A701B8() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x08A69144), 0x08B8A004(object, 0x08CF6F24, 0x08A68FF4),
// 0x08B87108(object, 0x08CB0434, 0x08A69420) and 0x08B87108(object, 0x08CB0440, 0x08A6933C) register it.
void func_08A69208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB0428u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A701B8u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A69144u});
    call_set(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x08A68FF4u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CB0434u, 0x08A69420u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CB0440u, 0x08A6933Cu});
}
// 0x08AB3034 (a0 = the object): the +0x38 holder replacement with the format 0x08CB3BD8, built at frame +4 by
// 0x08B85EB8(text, 0x08CB3BD8, frame +1) and handed to the virtual slot 0x108 (word at the vtable's +0x108) with the object. The counted text
// is released at once (header word - 0xC, the empty header 0x08D74FD0 skipped, 0x08B60804(word - 4, -1) and, at a count of 0, the header
// freed by 0x08B8542C(header, frame)). 0x08A70C44() makes the holder (released by 0x08A5BA08 when null), the old holder is released by
// 0x08A5B930 and the new one stored; then 0x08B8850C(object, 0x08CF6F14, 0x08AB2F70), 0x08B8A004(object, 0x08CF6F24, 0x08AB2E20),
// 0x08B87108(object, 0x08CB3BE4, 0x08AB3168) and 0x08B87108(object, 0x08CB3BF0, 0x08AB3420) register it.
void func_08AB3034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = text;
    const u32 assigned = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = assigned;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB3BD8u, frame.at(0x1u)});
    call_set(rt, ctx, assigned, {self, text});
    const u32 built = word(rt, text);
    const u32 header = built - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {built - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
    }
    const u32 holder = call_set(rt, ctx, 0x08A70C44u, {});
    ctx.gpr[16] = holder;
    if (holder != 0u) call_set(rt, ctx, 0x08A5BA08u, {holder});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, holder);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08AB2F70u});
    call_set(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x08AB2E20u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CB3BE4u, 0x08AB3168u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CB3BF0u, 0x08AB3420u});
}
void func_08ACB7CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[31]); G[2] = 1u; G[8] = G[4]; if (G[4] == G[2]) { G[2] = 0xFFFFu; if (G[5] == G[2]) { G[2] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[6] = G[4] + 1u; G[3] = G[6] < G[4] ? 1u : 0u; G[7] = G[3] + G[5]; put(rt, 0x08D771C0u, G[6]); put(rt, 0x08D771C4u, G[7]); if (G[6] == G[8] && G[7] == 0u) { G[5] = 0x08D70000u; G[4] = 0x08D77204u; call_here(rt, ctx, 0x08C3D5A8u); } G[5] = 0x08D70000u; G[2] = word(rt, 0x08D771B8u); G[3] = word(rt, 0x08D771BCu); G[6] = G[2] + 1u; G[4] = G[6] < G[2] ? 1u : 0u; G[7] = G[4] + G[3]; G[2] = 1u; put(rt, 0x08D771B8u, G[6]); put(rt, 0x08D771BCu, G[7]); if (G[6] == G[2] && G[7] == 0u) { G[4] = 0x08D771D0u; tail = 0x08C3D5D8u; } } } else { G[2] = 0xFFFFu; if (G[4] == 0u && G[5] == G[2]) { G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771B8u); G[5] = word(rt, 0x08D771BCu); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771B8u, G[2]); put(rt, 0x08D771BCu, G[3]); if (G[4] == 0u) { G[4] = 0x08D771D0u; call_here(rt, ctx, 0x08C3D628u); } G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771C0u, G[2]); put(rt, 0x08D771C4u, G[3]); if (G[4] == 0u) { G[4] = 0x08D77204u; tail = 0x08C3D638u; } } }  } if (tail != 0u) jump(ctx, tail); }
void func_08ACC314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[31]); G[2] = 1u; G[8] = G[4]; if (G[4] == G[2]) { G[2] = 0xFFFFu; if (G[5] == G[2]) { G[2] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[6] = G[4] + 1u; G[3] = G[6] < G[4] ? 1u : 0u; G[7] = G[3] + G[5]; put(rt, 0x08D771C0u, G[6]); put(rt, 0x08D771C4u, G[7]); if (G[6] == G[8] && G[7] == 0u) { G[5] = 0x08D70000u; G[4] = 0x08D77204u; call_here(rt, ctx, 0x08C3D5A8u); } G[5] = 0x08D70000u; G[2] = word(rt, 0x08D771B8u); G[3] = word(rt, 0x08D771BCu); G[6] = G[2] + 1u; G[4] = G[6] < G[2] ? 1u : 0u; G[7] = G[4] + G[3]; G[2] = 1u; put(rt, 0x08D771B8u, G[6]); put(rt, 0x08D771BCu, G[7]); if (G[6] == G[2] && G[7] == 0u) { G[4] = 0x08D771D0u; tail = 0x08C3D5D8u; } } } else { G[2] = 0xFFFFu; if (G[4] == 0u && G[5] == G[2]) { G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771B8u); G[5] = word(rt, 0x08D771BCu); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771B8u, G[2]); put(rt, 0x08D771BCu, G[3]); if (G[4] == 0u) { G[4] = 0x08D771D0u; call_here(rt, ctx, 0x08C3D628u); } G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771C0u, G[2]); put(rt, 0x08D771C4u, G[3]); if (G[4] == 0u) { G[4] = 0x08D77204u; tail = 0x08C3D638u; } } }  } if (tail != 0u) jump(ctx, tail); }
void func_08AD00F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[31]); G[2] = 1u; G[8] = G[4]; if (G[4] == G[2]) { G[2] = 0xFFFFu; if (G[5] == G[2]) { G[2] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[6] = G[4] + 1u; G[3] = G[6] < G[4] ? 1u : 0u; G[7] = G[3] + G[5]; put(rt, 0x08D771C0u, G[6]); put(rt, 0x08D771C4u, G[7]); if (G[6] == G[8] && G[7] == 0u) { G[5] = 0x08D70000u; G[4] = 0x08D77204u; call_here(rt, ctx, 0x08C3D5A8u); } G[5] = 0x08D70000u; G[2] = word(rt, 0x08D771B8u); G[3] = word(rt, 0x08D771BCu); G[6] = G[2] + 1u; G[4] = G[6] < G[2] ? 1u : 0u; G[7] = G[4] + G[3]; G[2] = 1u; put(rt, 0x08D771B8u, G[6]); put(rt, 0x08D771BCu, G[7]); if (G[6] == G[2] && G[7] == 0u) { G[4] = 0x08D771D0u; tail = 0x08C3D5D8u; } } } else { G[2] = 0xFFFFu; if (G[4] == 0u && G[5] == G[2]) { G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771B8u); G[5] = word(rt, 0x08D771BCu); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771B8u, G[2]); put(rt, 0x08D771BCu, G[3]); if (G[4] == 0u) { G[4] = 0x08D771D0u; call_here(rt, ctx, 0x08C3D628u); } G[7] = 0x08D70000u; G[4] = word(rt, 0x08D771C0u); G[5] = word(rt, 0x08D771C4u); G[2] = G[4] - 1u; G[6] = G[2] < G[4] ? 1u : 0u; G[3] = G[5] - 1u; G[3] = G[6] + G[3]; G[4] = G[2] | G[3]; put(rt, 0x08D771C0u, G[2]); put(rt, 0x08D771C4u, G[3]); if (G[4] == 0u) { G[4] = 0x08D77204u; tail = 0x08C3D638u; } } }  } if (tail != 0u) jump(ctx, tail); }
void func_08B97F58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 1u; G[3] = load8(G[4] + 0x25u); G[16] = word(rt, G[5] + 4u); G[5] = word(rt, G[5]); G[2] -= G[3]; G[2] <<= 3; G[2] += G[4]; G[4] = G[2] + 0x14u; if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[2] = word(rt, G[4] + 4u); put(rt, G[4], G[5]); G[18] = G[4] + 4u; if (G[16] != G[2]) { if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[17] = word(rt, G[18]); if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[17]; call_here(rt, ctx, G[2]); } } } put(rt, G[18], G[16]); } if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail != 0u) jump(ctx, tail); }
void func_08B997FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 1u; G[3] = load8(G[4] + 0x25u); G[16] = word(rt, G[5] + 4u); G[5] = word(rt, G[5]); G[2] -= G[3]; G[2] <<= 3; G[2] += G[4]; G[4] = G[2] + 0x14u; if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[2] = word(rt, G[4] + 4u); put(rt, G[4], G[5]); G[18] = G[4] + 4u; if (G[16] != G[2]) { if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[17] = word(rt, G[18]); if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[17]; call_here(rt, ctx, G[2]); } } } put(rt, G[18], G[16]); } if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail != 0u) jump(ctx, tail); }
void func_08BA3EEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 1u; G[3] = load8(G[4] + 0x25u); G[16] = word(rt, G[5] + 4u); G[5] = word(rt, G[5]); G[2] -= G[3]; G[2] <<= 3; G[2] += G[4]; G[4] = G[2] + 0x14u; if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[2] = word(rt, G[4] + 4u); put(rt, G[4], G[5]); G[18] = G[4] + 4u; if (G[16] != G[2]) { if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[17] = word(rt, G[18]); if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[17]; call_here(rt, ctx, G[2]); } } } put(rt, G[18], G[16]); } if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail != 0u) jump(ctx, tail); }
void func_08BA68F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 1u; G[3] = load8(G[4] + 0x25u); G[16] = word(rt, G[5] + 4u); G[5] = word(rt, G[5]); G[2] -= G[3]; G[2] <<= 3; G[2] += G[4]; G[4] = G[2] + 0x14u; if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[2] = word(rt, G[4] + 4u); put(rt, G[4], G[5]); G[18] = G[4] + 4u; if (G[16] != G[2]) { if (G[16] != 0u) { G[3] = G[16] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[17] = word(rt, G[18]); if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0xCu); G[4] = G[17]; call_here(rt, ctx, G[2]); } } } put(rt, G[18], G[16]); } if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail != 0u) jump(ctx, tail); }
void func_08BC0190(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); G[18] = G[5]; G[17] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (G[17] != 0u) { G[3] = G[17] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[5] = word(rt, G[16] + 0x34u); G[2] = 1u; store8(G[16] + 0xCu, G[2]); G[3] = G[5] & 1u; store8(G[16] + 0xEu, 0u); G[6] = word(rt, G[16] + 0x28u); if (G[3] != 0u) { G[2] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x30u); G[3] += G[4]; G[3] = word(rt, G[3]); } else { G[3] = word(rt, G[16] + 0x30u); } G[4] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); call_here(rt, ctx, G[3]); G[2] = word(rt, G[18] + 0xA8u); bool skip; if (G[2] != 0u) { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } else { G[2] = load8(G[16] + 0xDu); if (G[2] != 0u) skip = true; else { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } } if (!skip) { G[4] = word(rt, G[16] + 8u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AC05F4u); put(rt, G[16] + 8u, 0u); } if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[17]); G[4] = G[17]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } if (tail != 0u) jump(ctx, tail); }
void func_08BCFFF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); G[18] = G[5]; G[17] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (G[17] != 0u) { G[3] = G[17] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[5] = word(rt, G[16] + 0x34u); G[2] = 1u; store8(G[16] + 0xCu, G[2]); G[3] = G[5] & 1u; store8(G[16] + 0xEu, 0u); G[6] = word(rt, G[16] + 0x28u); if (G[3] != 0u) { G[2] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x30u); G[3] += G[4]; G[3] = word(rt, G[3]); } else { G[3] = word(rt, G[16] + 0x30u); } G[4] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); call_here(rt, ctx, G[3]); G[2] = word(rt, G[18] + 0xA8u); bool skip; if (G[2] != 0u) { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } else { G[2] = load8(G[16] + 0xDu); if (G[2] != 0u) skip = true; else { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } } if (!skip) { G[4] = word(rt, G[16] + 8u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AC05F4u); put(rt, G[16] + 8u, 0u); } if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[17]); G[4] = G[17]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } if (tail != 0u) jump(ctx, tail); }
void func_08BD6A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); G[18] = G[5]; G[17] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (G[17] != 0u) { G[3] = G[17] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[5] = word(rt, G[16] + 0x34u); G[2] = 1u; store8(G[16] + 0xCu, G[2]); G[3] = G[5] & 1u; store8(G[16] + 0xEu, 0u); G[6] = word(rt, G[16] + 0x28u); if (G[3] != 0u) { G[2] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x30u); G[3] += G[4]; G[3] = word(rt, G[3]); } else { G[3] = word(rt, G[16] + 0x30u); } G[4] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); call_here(rt, ctx, G[3]); G[2] = word(rt, G[18] + 0xA8u); bool skip; if (G[2] != 0u) { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } else { G[2] = load8(G[16] + 0xDu); if (G[2] != 0u) skip = true; else { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } } if (!skip) { G[4] = word(rt, G[16] + 8u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AC05F4u); put(rt, G[16] + 8u, 0u); } if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[17]); G[4] = G[17]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } if (tail != 0u) jump(ctx, tail); }
void func_08BD7070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); G[18] = G[5]; G[17] = word(rt, G[4] + 0x2Cu); G[16] = G[4]; if (G[17] != 0u) { G[3] = G[17] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[5] = word(rt, G[16] + 0x34u); G[2] = 1u; store8(G[16] + 0xCu, G[2]); G[3] = G[5] & 1u; store8(G[16] + 0xEu, 0u); G[6] = word(rt, G[16] + 0x28u); if (G[3] != 0u) { G[2] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); G[3] = word(rt, G[2]); G[4] = word(rt, G[16] + 0x30u); G[3] += G[4]; G[3] = word(rt, G[3]); } else { G[3] = word(rt, G[16] + 0x30u); } G[4] = G[6] + static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 1); call_here(rt, ctx, G[3]); G[2] = word(rt, G[18] + 0xA8u); bool skip; if (G[2] != 0u) { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } else { G[2] = load8(G[16] + 0xDu); if (G[2] != 0u) skip = true; else { G[2] = load8(G[16] + 0xEu); skip = G[2] != 0u; } } if (!skip) { G[4] = word(rt, G[16] + 8u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08AC05F4u); put(rt, G[16] + 8u, 0u); } if (G[17] != 0u) { G[3] = G[17] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 8u); G[4] = G[17]; call_here(rt, ctx, G[2]); G[3] = G[17] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[17]); G[4] = G[17]; G[25] = word(rt, G[2] + 0xCu); tail = G[25]; } } } G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[17] = word(rt, frame.at(0x4u)); } if (tail != 0u) jump(ctx, tail); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_228
} // namespace gt::game

#define HOT228(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_228::name, gt::ReturnKind::kind)
HOT228(0x089B3BB0u, func_089B3BB0, Word);
HOT228(0x08ACB1C8u, func_08ACB1C8, Word);
HOT228(0x08B97DE0u, func_08B97DE0, Word);
// HOT228(0x08BAF270u, func_08BAF270, Word);  // no comparable synthetic case yet
HOT228(0x08A61310u, func_08A61310, Word);
HOT228(0x08A69208u, func_08A69208, Word);
HOT228(0x08AB3034u, func_08AB3034, Word);
HOT228(0x08ACB7CCu, func_08ACB7CC, Word);
HOT228(0x08ACC314u, func_08ACC314, Word);
HOT228(0x08AD00F8u, func_08AD00F8, Word);
HOT228(0x08B97F58u, func_08B97F58, Word);
HOT228(0x08B997FCu, func_08B997FC, Word);
HOT228(0x08BA3EECu, func_08BA3EEC, Word);
HOT228(0x08BA68F0u, func_08BA68F0, Word);
// HOT228(0x08BC0190u, func_08BC0190, Word);  // no comparable synthetic case yet
// HOT228(0x08BCFFF8u, func_08BCFFF8, Word);  // no comparable synthetic case yet
HOT228(0x08BD6A6Cu, func_08BD6A6C, Word);
HOT228(0x08BD7070u, func_08BD7070, Word);
