// Hot batch 304: eighty-one to eighty-three-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (script bridges and property accessors, a number parser, string ordering, an
// exception-table header reader, a double-to-unsigned conversion, a file loader and a child-object classifier).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_304 {
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

void func_089E35AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[17] = G[4]; G[16] = G[7]; if (G[6] == 0u) { G[18] = frame.at(0xCu); G[4] = G[18]; call_here(rt, ctx, 0x089E1104u); G[16] = word(rt, frame.at(0xCu)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08CA77A4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x089DFA38u); G[16] = frame.at(0x8u); G[4] = G[16]; G[5] = G[2]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0x8u)); if (G[17] != G[16]) { G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[2] = 1u; if (G[6] == G[2]) { G[19] = frame.at(0xCu); G[4] = G[19]; call_here(rt, ctx, 0x089E1104u); G[18] = word(rt, frame.at(0xCu)); if (G[18] == 0u) { G[4] = 0x08CA77A4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[16]); if (G[17] == 0u) { G[4] = 0x08CA7770u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[17]); G[16] = frame.at(4u); G[4] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[5] = G[17]; call_here(rt, ctx, G[2]); G[4] = G[18]; G[5] = G[16]; call_here(rt, ctx, 0x089E3478u); G[4] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); } } G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); }  } }
// 0x08A21AF8 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CAA91C, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then four registrations on the object: 0x08B8850C(object, 0x08CF6F14, 0x08A218E8), 0x08B8A004(object, 0x08CF6F24, 0x08A21D5C),
// 0x08B87108(object, 0x08CAA944, 0x08A21C48), 0x08B878E8(object, 0x08CAA94C, 0x08A21928, 0x08A21928) and 0x08B87108(object, 0x08CAA95C, 0x08A21A04).
void func_08A21AF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAA91Cu, frame.at(0x1u)});
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
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A218E8u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08A21D5Cu});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAA944u, 0x08A21C48u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CAA94Cu, 0x08A21928u, 0x08A21928u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAA95Cu, 0x08A21A04u});
}
void func_08A424A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); G[19] = G[4]; G[16] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { G[20] = frame.at(8u); G[4] = G[20]; call_here(rt, ctx, 0x08A40CF8u); G[18] = word(rt, frame.at(8u)); if (G[18] == 0u) { G[4] = 0x08CADBCCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[17] = word(rt, G[16]); if (G[17] == 0u) { G[4] = 0x08CADB98u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[17]); G[16] = frame.at(4u); G[5] = G[17]; G[3] = word(rt, G[2] + 0xCu); G[4] = G[16]; G[17] = frame.at(0xCu); call_here(rt, ctx, G[3]); G[5] = G[18]; G[6] = G[16]; G[4] = G[17]; G[16] = frame.at(0x10u); G[7] = 0u; call_here(rt, ctx, 0x08A42370u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0x10u)); if (G[19] != G[16]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = word(rt, frame.at(4u)); G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[17] != G[16]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x2Cu)); G[16] = word(rt, frame.at(0x20u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); }  } }
void func_08A46F64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[18] = G[4]; if (static_cast<std::int32_t>(G[5]) > 0) { G[16] = word(rt, G[6]); if (G[16] == 0u) { G[4] = 0x08CAEA9Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = frame.at(0x10u); G[2] = word(rt, G[3] + 0xCu); G[5] = G[16]; call_here(rt, ctx, G[2]); G[4] = word(rt, frame.at(0x10u)); call_here(rt, ctx, 0x08B1E528u); G[16] = frame.at(8u); if (G[2] != 0u) { G[5] = G[2]; G[4] = G[16]; G[17] = frame.at(0xCu); G[6] = frame.at(2u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0xCu)); if (G[18] != G[17]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); } } } else { G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08A76AA0u); G[5] = word(rt, frame.at(4u)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; } G[4] = word(rt, frame.at(0x10u)); G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08A72364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CB0AC8u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CB0AD0u; G[6] = 0x08A71F70u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB0AD8u; G[6] = 0x08A7210Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB0AE0u; G[6] = 0x08A71EC8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB0AE8u; G[6] = 0x08A71F1Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB0AF0u; G[6] = 0x08A724B4u; G[7] = 0u; call_here(rt, ctx, 0x08B8D51Cu); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08ABAA00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x4u), G[17]); G[22] = 0x08D00000u; G[21] = G[4] + 0x14u; G[19] = G[4]; G[4] = G[21]; G[16] = 0x08CFFA54u; call_here(rt, ctx, 0x08B34888u); G[2] = word(rt, G[16] + 0x18u); G[4] = word(rt, G[16] + 0x14u); G[4] = G[2] + G[4]; call_here(rt, ctx, 0x08B6461Cu); G[5] = G[2]; G[4] = G[16]; G[18] = G[2]; call_here(rt, ctx, 0x08B2B828u); G[2] = word(rt, G[16] + 0x14u); G[3] = word(rt, G[16] + 0x18u); G[2] += G[3]; if (G[2] != 0u) { G[20] = G[16]; G[17] = 0u; G[2] = G[18] + G[17]; do { G[3] = load8(G[2]); G[5] = G[17]; G[4] = 0x08CFFA54u; G[17] += 1u; if (G[3] == 0u) { G[2] = word(rt, G[19]); G[16] = word(rt, G[2] + 0x28u); call_here(rt, ctx, 0x08C3C3C8u); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, G[16]); } G[2] = word(rt, G[20] + 0x14u); G[3] = word(rt, G[20] + 0x18u); G[2] += G[3]; G[2] = G[17] < G[2] ? 1u : 0u; bool more = G[2] != 0u; G[2] = G[18] + G[17]; if (!more) break; } while (true); } if (G[18] != 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B61FC0u); } G[4] = G[21]; call_here(rt, ctx, 0x08B34B24u); G[16] = word(rt, G[19] + 0x40u); put(rt, G[19] + 0x40u, 0u); if (G[16] != 0u) { G[4] = G[21]; store8(G[19] + 0x4Cu, 0u); call_here(rt, ctx, 0x08B28C74u); G[4] = G[16]; tail = true; } G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0xCu)); G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); G[20] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x4u)); } if (tail) jump(ctx, 0x08B1B99Cu); } } }
void func_08AFBDB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[22] = G[5]; G[21] = G[6]; G[17] = load16(G[4] + 0x16u); G[18] = G[4]; if (G[17] != 0u) { G[20] = 0u; G[19] = 0u; do { G[2] = word(rt, G[18] + 0x1Cu); G[16] = G[2] + G[19]; bool skip = false; if (G[21] != 0u) { G[4] = word(rt, G[16]); G[5] = G[21]; call_here(rt, ctx, 0x08B35FD8u); if (G[2] != 0u) { G[20] += 1u; skip = true; } } if (!skip) { G[5] = sext(load16(G[16] + 0xAu), 16); G[4] = G[18]; call_here(rt, ctx, 0x08AFC8C8u); G[4] = G[2]; G[5] = 0x47u; G[16] = G[2]; call_here(rt, ctx, 0x08AFC8B8u); if (G[2] != 0u) store8(G[2] + 0xDu, G[22]); G[4] = G[16]; if (G[22] != 0u) { G[4] = G[16]; G[5] = 0x55u; call_here(rt, ctx, 0x08AFC8C0u); G[3] = G[2]; if (G[2] != 0u) { G[2] = 0x69u; store16(G[3] + 0xEu, G[2]); } G[4] = G[16]; G[5] = 0x56u; call_here(rt, ctx, 0x08AFC8C0u); G[3] = G[2]; if (G[2] != 0u) { G[2] = 0x6Au; store16(G[3] + 0xEu, G[2]); } } else { G[4] = G[16]; G[5] = 0x69u; call_here(rt, ctx, 0x08AFC8C0u); G[3] = G[2]; if (G[2] != 0u) { G[2] = 0x55u; store16(G[3] + 0xEu, G[2]); } G[4] = G[16]; G[5] = 0x6Au; call_here(rt, ctx, 0x08AFC8C0u); G[3] = G[2]; if (G[2] != 0u) { G[2] = 0x56u; store16(G[3] + 0xEu, G[2]); } } G[17] = load16(G[18] + 0x16u); G[20] += 1u; } G[2] = G[17] & 0xFFFFu; G[2] = static_cast<std::int32_t>(G[20]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[19] += 0xCu; } while (G[2] != 0u); } G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x8u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08C2DF68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[16] = word(rt, G[5]); G[17] = G[6]; if (G[16] == 0u) { G[4] = 0x08CB2328u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08CB2328u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(4u); call_here(rt, ctx, G[2]); G[19] = word(rt, frame.at(8u)); G[18] = word(rt, frame.at(4u)); G[2] = frame.at(0xCu); G[16] = word(rt, G[19] - 0xCu); G[7] = frame.at(0x10u); G[4] = G[19]; put(rt, frame.at(0xCu), G[16]); G[5] = G[18]; G[20] = G[18] - 0xCu; G[17] = word(rt, G[18] - 0xCu); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[7] = G[2]; put(rt, frame.at(0x10u), G[17]); G[16] -= G[17]; G[6] = word(rt, G[7]); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; G[2] = 0x08D70000u; G[21] = 0x08D74FD0u; G[16] >>= 31; if (G[20] != G[21]) { G[4] = G[18] - 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[20]; G[5] = frame.at(1u); call_here(rt, ctx, 0x08B8542Cu); } } G[19] = word(rt, frame.at(8u)); G[17] = G[19] - 0xCu; G[2] = G[16]; if (G[21] != G[17]) { G[4] = G[19] - 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } G[2] = G[16]; } G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
// 0x08A2D610 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CAC2A8, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then the registrations: 0x08B8850C(object, 0x08CF6F14, 0x08A2DAE8), 0x08B8A004(object, 0x08CF6F24, 0x08A2D154),
// 0x08B87108(object, 0x08CAC2E8, 0x08A2D378), 0x08B87108(object, 0x08CAC2F4, 0x08A2D434) and 0x08B878E8(object, 0x08CAC2FC, 0x08A2D840, 0x08A2D764).
void func_08A2D610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAC2A8u, frame.at(0x1u)});
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
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A2DAE8u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08A2D154u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAC2E8u, 0x08A2D378u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAC2F4u, 0x08A2D434u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CAC2FCu, 0x08A2D840u, 0x08A2D764u});
}
void func_08A9FB90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); G[17] = G[5]; G[16] = G[4]; G[3] = 0u; if (G[4] != 0u) { G[5] = 0x08CB278Cu; G[6] = 4u; call_here(rt, ctx, 0x08B7C6BCu); G[3] = 1u; if (G[2] != 0u) { G[5] = 0x08CB2794u; G[4] = G[16]; G[6] = 4u; call_here(rt, ctx, 0x08B7C6BCu); G[3] = 0u; if (G[2] != 0u) { G[2] = 0x30u; G[3] = sext(load8(G[16]), 8); G[4] = G[16]; int base = 10; if (G[3] == G[2]) { G[3] = sext(load8(G[16] + 1u), 8); G[2] = 0x78u; G[4] = G[16] + 1u; bool hex = G[3] == G[2]; if (!hex) { G[2] = 0x58u; hex = G[3] == G[2]; } if (hex) { G[2] = sext(load8(G[16] + 2u), 8); G[4] = G[2] - 0x30u; G[3] = G[2] - 0x41u; G[3] = G[3] < 6u ? 1u : 0u; G[4] = G[4] < 10u ? 1u : 0u; G[2] -= 0x61u; G[3] |= G[4]; G[2] = G[2] < 6u ? 1u : 0u; G[2] |= G[3]; G[5] = G[16] + 2u; if (G[2] != 0u) { G[4] = G[5]; base = 16; } else { G[4] = G[16]; } } else { G[2] = 0x62u; bool bin = G[3] == G[2]; G[2] = 0x42u; if (!bin) { bin = G[3] == G[2]; G[2] = G[3] - 0x30u; } if (bin) { G[2] = sext(load8(G[16] + 2u), 8); G[2] -= 0x30u; G[2] = G[2] < 2u ? 1u : 0u; G[4] = G[16] + 2u; if (G[2] != 0u) base = 2; else G[4] = G[16]; } else { G[2] = G[2] < 8u ? 1u : 0u; if (G[2] != 0u) base = 8; else G[4] = G[16]; } } } G[5] = G[17]; G[6] = static_cast<std::uint32_t>(base); call_here(rt, ctx, 0x08B7E4CCu); G[3] = G[2] != 0u ? 1u : 0u; } } } G[2] = G[3]; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B0A1F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x48u), G[31]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); G[2] = 1u; G[20] = G[4]; G[3] = word(rt, G[4] + 4u); G[16] = G[5]; bool post = false; if (G[3] == G[2]) { G[4] = G[5]; call_here(rt, ctx, 0x08B0980Cu); G[17] = G[2]; G[2] = word(rt, G[20] + 0x14u); if (G[2] != 0u) { G[16] = word(rt, G[20] + 0x10u); G[4] = G[17]; call_here(rt, ctx, 0x08B089D4u); if (G[16] == G[2]) { G[5] = word(rt, G[20] + 0x14u); G[4] = G[17]; call_here(rt, ctx, 0x08B08D30u); G[3] = 0xFFFFFFFFu; post = true; } } } else { G[2] = 2u; bool two = G[3] == G[2]; G[3] = 0xFFFFFFFFu; if (!two) { post = true; } else { G[4] = G[5]; call_here(rt, ctx, 0x08B0980Cu); G[4] = G[2]; G[21] = G[2]; call_here(rt, ctx, 0x08B08898u); if (G[2] != 0u) { G[4] = G[16]; call_here(rt, ctx, 0x08B09A04u); G[18] = word(rt, G[20] + 8u); G[19] = word(rt, G[20] + 0xCu); G[17] = word(rt, G[21]); G[4] = G[21]; G[16] = G[2]; call_here(rt, ctx, 0x08B089D4u); G[9] = G[2]; G[4] = G[16]; G[6] = G[18]; G[7] = G[19]; G[8] = G[17]; G[10] = 1u; G[11] = 1u; put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); call_here(rt, ctx, 0x08B30660u); G[3] = 0xFFFFFFFFu; post = true; } } } if (post) { G[2] = 0x20u; G[4] = frame.at(0x10u); put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x24u), G[20]); put(rt, frame.at(0x10u), G[2]); put(rt, frame.at(0x14u), 0u); put(rt, frame.at(0x18u), 0u); put(rt, frame.at(0x1Cu), G[3]); put(rt, frame.at(0x28u), 0u); put(rt, frame.at(0x2Cu), 0u); call_here(rt, ctx, 0x08B09538u); } G[20] = word(rt, frame.at(0x40u)); G[16] = word(rt, frame.at(0x30u)); G[21] = word(rt, frame.at(0x44u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); }  } }
void func_08B637DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[19] = G[5]; G[18] = G[5] + 1u; G[17] = G[6]; G[6] = 0u; G[16] = G[4]; if (G[4] != 0u) { call_here(rt, ctx, 0x08B726FCu); G[6] = G[2]; } put(rt, G[17], G[6]); G[5] = G[16]; G[2] = 0xFFu; G[3] = load8(G[19]); G[16] = G[3] & 0xFFu; G[4] = G[16]; if (G[3] == G[2]) { put(rt, G[17] + 4u, G[6]); G[2] = 0xFFu; } else { call_here(rt, ctx, 0x08B63574u); G[5] = G[2]; G[6] = G[18]; G[4] = G[16]; G[7] = G[17] + 4u; call_here(rt, ctx, 0x08B636A0u); G[18] = G[2]; G[2] = 0xFFu; } G[3] = load8(G[18]); G[6] = G[18] + 1u; G[5] = frame.at(0u); G[4] = G[6]; store8(G[17] + 0x14u, G[3]); if (G[3] == G[2]) { put(rt, G[17] + 0xCu, 0u); } else { call_here(rt, ctx, 0x08B63614u); G[6] = G[2]; G[2] = word(rt, frame.at(0u)); G[2] = G[6] + G[2]; put(rt, G[17] + 0xCu, G[2]); } G[4] = G[6] + 1u; G[5] = frame.at(0u); G[2] = load8(G[6]); store8(G[17] + 0x15u, G[2]); call_here(rt, ctx, 0x08B63614u); G[3] = word(rt, frame.at(0u)); G[4] = G[2] + G[3]; put(rt, G[17] + 0x10u, G[4]); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x08B6F1B8 (a0:a1 = a double, v0:v1 = its unsigned 64-bit value): the double to unsigned 64-bit split, through the
// soft-float pair helpers. y = func_08B71394 (multiply) of the double by 2^-32 (the pair at 0x08CB6918); z = func_08B6F110
// (the double to unsigned 32-bit conversion) of y; w = func_08B71CE0 (int to double) of z. When z's sign bit is set (the
// asm tests s0 = z with bltz), w = func_08B71328 (add) of w and 2^32 (the pair at 0x08CB6920) first. u = func_08B71394
// (multiply) of w and -2^32 (the pair at 0x08CB6928); t = func_08B71328 (add) of the double and u; v0 = func_08B6F110
// (the double to unsigned 32-bit conversion) of t, the remainder's word, and v1 = z, the quotient's word. The original
// ORs s2 = 0 into v0 and s3 = z into v1 (both no-ops: s2 is 0 and s3 = z).
void func_08B6F1B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 double_low = ctx.gpr[4];
    const u32 double_high = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x14u}, {21u, 0x10u}, {20u, 0xCu}, {19u, 0x8u}, {18u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[21] = double_high;
    ctx.gpr[20] = double_low;
    ctx.gpr[18] = 0u;
    ctx.gpr[2] = high_half(0x08CB6918u);
    const u32 scaled_low = call_set(rt, ctx, 0x08B71394u, {double_low, double_high, word(rt, 0x08CB6918u), word(rt, 0x08CB691Cu)});
    const u32 scaled_high = ctx.gpr[3];
    const u32 quotient = call_set(rt, ctx, 0x08B6F110u, {scaled_low, scaled_high});
    ctx.gpr[16] = quotient;
    ctx.gpr[19] = quotient;
    const u32 shifted_low = call_set(rt, ctx, 0x08B71CE0u, {quotient});
    const u32 shifted_high = ctx.gpr[3];
    ctx.gpr[8] = shifted_low;
    ctx.gpr[9] = shifted_high;
    u32 base_low = shifted_low;
    u32 base_high = shifted_high;
    if (static_cast<s32>(quotient) < 0) {
        ctx.gpr[2] = high_half(0x08CB6920u);
        base_low = call_set(rt, ctx, 0x08B71328u, {base_low, base_high, word(rt, 0x08CB6920u), word(rt, 0x08CB6924u)});
        base_high = ctx.gpr[3];
        ctx.gpr[8] = base_low;
        ctx.gpr[9] = base_high;
    }
    ctx.gpr[2] = high_half(0x08CB6928u);
    const u32 product_low = call_set(rt, ctx, 0x08B71394u, {base_low, base_high, word(rt, 0x08CB6928u), word(rt, 0x08CB692Cu)});
    const u32 product_high = ctx.gpr[3];
    const u32 remainder_low = call_set(rt, ctx, 0x08B71328u, {double_low, double_high, product_low, product_high});
    const u32 remainder_high = ctx.gpr[3];
    const u32 result = call_set(rt, ctx, 0x08B6F110u, {remainder_low, remainder_high});
    ctx.gpr[5] = 0u;
    ctx.gpr[2] = result;
    ctx.gpr[3] = quotient;
}
void func_08936748(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x240u); put(rt, frame.at(0x22Cu), G[19]); put(rt, frame.at(0x224u), G[17]); put(rt, frame.at(0x220u), G[16]); put(rt, frame.at(0x238u), G[31]); put(rt, frame.at(0x234u), G[21]); put(rt, frame.at(0x230u), G[20]); put(rt, frame.at(0x228u), G[18]); G[19] = G[4]; G[17] = G[7]; G[16] = G[6]; G[21] = G[8]; G[20] = G[4] + 0x2D0u; call_here(rt, ctx, 0x0893656Cu); G[4] = G[19]; G[6] = G[16]; G[7] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x08936578u); G[5] = G[2]; G[2] = 0x23u; G[4] = G[20]; G[16] = 0u; if (G[18] == G[2]) { call_here(rt, ctx, 0x08939920u); G[3] = 0xFFFFFFFFu; G[17] = frame.at(0x108u); G[16] = frame.at(8u); G[5] = G[2]; G[4] = G[20]; if (G[2] != G[3]) { call_here(rt, ctx, 0x08938EF0u); G[4] = G[17]; G[18] = G[2]; call_here(rt, ctx, 0x08B0CC04u); G[4] = G[16]; G[5] = G[19] + 0xCu; call_here(rt, ctx, 0x08B7BB6Cu); G[5] = 0x08CA21B8u; G[4] = G[16]; call_here(rt, ctx, 0x08B7B9ACu); G[5] = G[16]; G[4] = G[17]; G[6] = 0u; G[7] = 0u; call_here(rt, ctx, 0x08B0C99Cu); G[16] = 0u; G[4] = G[17]; G[5] = 0u; if (static_cast<std::int32_t>(G[2]) >= 0) { G[6] = word(rt, G[18] + 4u); G[7] = 0u; call_here(rt, ctx, 0x08B0C5C0u); G[4] = word(rt, G[18] + 8u); call_here(rt, ctx, 0x08937AF4u); G[16] = G[2]; G[2] = word(rt, G[18] + 8u); G[5] = frame.at(0u); G[4] = G[17]; put(rt, frame.at(4u), G[2]); put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B0C650u); G[4] = G[17]; call_here(rt, ctx, 0x08B0C778u); G[3] = word(rt, G[18] + 8u); put(rt, G[21], G[3]); } G[4] = G[17]; call_here(rt, ctx, 0x08B0C808u); } else { G[16] = 0u; } } G[2] = G[16]; G[19] = word(rt, frame.at(0x22Cu)); G[17] = word(rt, frame.at(0x224u)); G[16] = word(rt, frame.at(0x220u)); G[21] = word(rt, frame.at(0x234u)); G[20] = word(rt, frame.at(0x230u)); G[18] = word(rt, frame.at(0x228u)); }  } }
void func_089928A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[18] = G[5]; G[17] = G[4]; G[16] = word(rt, G[4] + 0xF4u); put(rt, G[4] + 0x120u, 0u); put(rt, G[4] + 0x124u, 0u); put(rt, G[4] + 0x128u, 0u); while (G[16] != 0u) { if (G[16] != G[18]) { G[2] = word(rt, G[17] + 0x120u); if (G[2] == 0u) { call_here(rt, ctx, 0x08994488u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A76F20u); if (G[2] != 0u) { put(rt, G[17] + 0x120u, G[16]); goto next; } } G[2] = word(rt, G[17] + 0x124u); if (G[2] == 0u) { call_here(rt, ctx, 0x089ECF9Cu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A76F20u); if (G[2] != 0u) { put(rt, G[17] + 0x124u, G[16]); goto next; } } G[2] = word(rt, G[17] + 0x128u); if (G[2] == 0u) { call_here(rt, ctx, 0x089767ACu); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A76F20u); if (G[2] != 0u) { put(rt, G[17] + 0x128u, G[16]); goto next; } } } next: G[4] = G[16]; call_here(rt, ctx, 0x08968A38u); G[16] = G[2]; } G[2] = word(rt, G[17] + 0x120u); if (G[2] != 0u) { G[2] = word(rt, G[17] + 0x124u); if (G[2] != 0u) { G[4] = frame.at(0u); call_here(rt, ctx, 0x08A0EE4Cu); G[4] = word(rt, G[17] + 0x120u); G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08994C00u); G[4] = word(rt, G[17] + 0x124u); G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08993480u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[2] = word(rt, G[17] + 0x128u); if (G[2] != 0u) { G[4] = frame.at(0u); call_here(rt, ctx, 0x08A0EE4Cu); G[4] = word(rt, G[17] + 0x120u); G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08994574u); G[4] = word(rt, G[17] + 0x128u); G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08993480u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_304
} // namespace gt::game

#define HOT304(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_304::name, gt::ReturnKind::kind)
HOT304(0x089E35ACu, func_089E35AC, Word);
HOT304(0x08A21AF8u, func_08A21AF8, Word);
HOT304(0x08A424A0u, func_08A424A0, Word);
HOT304(0x08A46F64u, func_08A46F64, Word);
// HOT304(0x08A72364u, func_08A72364, Word);  // no comparable synthetic case yet
// HOT304(0x08ABAA00u, func_08ABAA00, Word);  // no comparable synthetic case yet
HOT304(0x08AFBDB0u, func_08AFBDB0, Word);
// HOT304(0x08C2DF68u, func_08C2DF68, Word);  // no comparable synthetic case yet
HOT304(0x08A2D610u, func_08A2D610, Word);
HOT304(0x08A9FB90u, func_08A9FB90, Word);
HOT304(0x08B0A1F0u, func_08B0A1F0, Word);
HOT304(0x08B637DCu, func_08B637DC, Word);
HOT304(0x08B6F1B8u, func_08B6F1B8, Word);
HOT304(0x08936748u, func_08936748, Word);
HOT304(0x089928A0u, func_089928A0, Word);
