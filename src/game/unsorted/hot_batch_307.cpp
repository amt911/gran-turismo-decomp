// Hot batch 307: eighty-six to ninety-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (script bindings, a file seek, a request allocator, a string-to-object bridge,
// a recursive tree copy, name lookups, a float setter and a pair collector).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_307 {
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

// 0x08A21054 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CAA7F4, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then the registrations: 0x08B8850C(object, 0x08CF6F14, 0x08A20E80), 0x08B87108(object, 0x08CAA848, 0x08A213C0),
// 0x08B87108(object, 0x08CAA850, 0x08A20D48), 0x08B87108(object, 0x08CAA858, 0x08A211BC), 0x08B87108(object, 0x08CAA860, 0x08A20D94) and
// 0x08B878E8(object, 0x08CAA868, 0x08A20EC0, 0x08A20EC0).
void func_08A21054(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CAA7F4u, frame.at(0x1u)});
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
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A20E80u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAA848u, 0x08A213C0u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAA850u, 0x08A20D48u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAA858u, 0x08A211BCu});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CAA860u, 0x08A20D94u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CAA868u, 0x08A20EC0u, 0x08A20EC0u});
}
// 0x08A31B54 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CACA44, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then 0x08B8850C(object, 0x08CF6F14, 0x08A31A88) and five 0x08B86FB0(object, name, handler) registrations: 0x08CACA4C / 0x08A31AC8,
// 0x08CACA54 / 0x08A31AC8, 0x08CACA58 / 0x08A31B0C, 0x08CACA68 / 0x08A31B0C and 0x08CACA78 / 0x08A31AC8.
void func_08A31B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CACA44u, frame.at(0x1u)});
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
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A31A88u});
    call_set(rt, ctx, 0x08B86FB0u, {object, 0x08CACA4Cu, 0x08A31AC8u});
    call_set(rt, ctx, 0x08B86FB0u, {object, 0x08CACA54u, 0x08A31AC8u});
    call_set(rt, ctx, 0x08B86FB0u, {object, 0x08CACA58u, 0x08A31B0Cu});
    call_set(rt, ctx, 0x08B86FB0u, {object, 0x08CACA68u, 0x08A31B0Cu});
    call_set(rt, ctx, 0x08B86FB0u, {object, 0x08CACA78u, 0x08A31AC8u});
}
// 0x08A71844 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CB0A24, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then 0x08B87108(object, name, handler) for the names 0x08CB0A34 / 0x08A71370, 0x08CB0A3C / 0x08A70F80, 0x08CB0A44 / 0x08A7150C,
// 0x08CB0A48 / 0x08A70FD4 and 0x08CB0A50 / 0x08A71594, and 0x08B878E8(object, 0x08CB0A58, 0x08A71294, 0x08A71294).
// 0x08A71844 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CB0A24, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then 0x08B87108(object, name, handler) for the names 0x08CB0A34 / 0x08A71370, 0x08CB0A3C / 0x08A70F80, 0x08CB0A44 / 0x08A7150C,
// 0x08CB0A48 / 0x08A70FD4 and 0x08CB0A50 / 0x08A71594, and 0x08B878E8(object, 0x08CB0A58, 0x08A71294, 0x08A71294).
void func_08A71844(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB0A24u, frame.at(0x1u)});
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
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB0A34u, 0x08A71370u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB0A3Cu, 0x08A70F80u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB0A44u, 0x08A7150Cu});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB0A48u, 0x08A70FD4u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB0A50u, 0x08A71594u});
    call_set(rt, ctx, 0x08B878E8u, {object, 0x08CB0A58u, 0x08A71294u, 0x08A71294u});
}
void func_08B12ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[20] = G[4]; G[2] = word(rt, G[5] + 0xCu); G[18] = G[5]; do { if (G[2] != 0u) { G[2] = 2u; G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; put(rt, G[18] + 4u, G[2]); break; } G[16] = word(rt, G[5] + 0x10u); G[3] = 1u; G[4] = word(rt, G[16] + 8u); G[2] = word(rt, G[16] + 0xCu); G[19] = G[4] + G[2]; bool chk = true; if (G[6] == G[3]) { G[17] = G[4] + G[8]; G[8] = G[17]; } else { G[2] = 2u; G[17] = G[19] + G[8]; if (G[6] == G[2]) { G[8] = G[17]; } else { G[17] = 0u; if (G[6] != 0u) chk = false; else G[17] = G[8]; } } if (chk) { G[2] = G[19] < G[17] ? 1u : 0u; if (static_cast<std::int32_t>(G[8]) < 0 || G[2] != 0u) { G[2] = 15u; G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; put(rt, G[18] + 4u, G[2]); break; } } G[2] = word(rt, G[16] + 0x10u); G[8] = 0u; bool ok = false; if (static_cast<std::int32_t>(G[2]) > 0) { G[6] = word(rt, G[16] + 4u); G[4] = word(rt, G[16]); G[6] = G[17] + G[6]; G[7] = 0u; call_here(rt, ctx, 0x08B33B30u); G[4] = G[2]; ok = static_cast<std::int32_t>(G[3]) >= 0; if (!ok) { G[4] = G[20]; G[5] = G[18]; call_here(rt, ctx, 0x08B1297Cu); } } if (!ok) { G[6] = word(rt, G[16] + 4u); G[4] = word(rt, G[16]); G[8] = 0u; G[6] = G[17] + G[6]; G[7] = 0u; call_here(rt, ctx, 0x08B33B30u); G[4] = G[2]; ok = static_cast<std::int32_t>(G[3]) >= 0; } if (!ok) { call_here(rt, ctx, 0x08B1830Cu); G[4] = 0xFFFFFFFFu; G[5] = 0xFFFFFFFFu; put(rt, G[18] + 4u, G[2]); break; } G[3] = word(rt, G[16] + 4u); G[5] = 0u; G[3] = G[4] - G[3]; G[2] = G[19] - G[3]; put(rt, G[16] + 0xCu, G[2]); G[4] = G[3]; put(rt, G[16] + 8u, G[3]); } while (false); G[2] = G[4]; G[3] = G[5]; G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08B402E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[2] = 0xFFFFFFFFu; G[30] = G[6]; G[23] = G[5]; G[22] = G[7] & 0xFFu; G[17] = G[4]; bool go = true; if (G[8] == G[2]) { G[19] = G[4] + 0x8DCu; G[4] = G[19]; call_here(rt, ctx, 0x08B3F3D0u); G[18] = G[2] & 0xFFFFu; G[4] = 0xFFFFFFFFu; G[20] = 0u; G[21] = 0u; if (static_cast<std::int32_t>(G[2]) < 0) go = false; } else if (G[8] == 0u) { G[4] = 0xFFFFFFFFu; if (G[22] == 0u) { go = false; } else { G[4] = word(rt, G[17] + 0xB0u); G[18] = 0u; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x3Cu); G[19] = G[17] + 0x8DCu; call_here(rt, ctx, G[2]); G[20] = G[2]; G[21] = G[3]; } } else { G[4] = 0xFFFFFFFFu; if (static_cast<std::int32_t>(G[8]) < 0) { go = false; } else { G[18] = G[8] & 0xFFFFu; G[20] = 0u; G[21] = 0u; G[19] = G[17] + 0x8DCu; } } if (go) { G[4] = G[17] + 0xCCu; G[5] = 0x50u; G[6] = 0x10u; call_here(rt, ctx, 0x08B2CB18u); G[16] = G[2]; if (G[2] != 0u) { G[4] = G[2]; call_here(rt, ctx, 0x08B3F478u); } G[5] = G[23]; G[4] = G[16] + 0x12u; store16(G[16], G[18]); put(rt, G[16] + 8u, G[30]); store8(G[16] + 4u, G[22]); call_here(rt, ctx, 0x08B3F7CCu); G[4] = G[17]; G[3] = word(rt, G[17]); G[2] = word(rt, G[3] + 0x8Cu); G[5] = 0u; call_here(rt, ctx, G[2]); put(rt, G[16] + 0x30u, G[2]); G[4] = G[19]; G[5] = G[16]; put(rt, G[16] + 0x34u, G[3]); put(rt, G[16] + 0x38u, G[20]); put(rt, G[16] + 0x3Cu, G[21]); call_here(rt, ctx, 0x08B3EE94u); G[4] = load16(G[16]); } G[2] = G[4]; G[30] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x4u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); }  } }
// 0x08A92DF8 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CB1FDC, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A7BC7C() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then 0x08B8850C(object, 0x08CF6F14, 0x08A92734) and 0x08B8A004(object, name, handler) for 0x08CF6F24 / 0x08A92F60,
// 0x08CF6FC0 / 0x08A927B8, 0x08CF6F9C / 0x08A92850, 0x08CF6FE0 / 0x08A928E8 and 0x08CF6F34 / 0x08A928E8.
void func_08A92DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB1FDCu, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08A7BC7Cu, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A92734u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08A92F60u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6FC0u, 0x08A927B8u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F9Cu, 0x08A92850u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6FE0u, 0x08A928E8u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F34u, 0x08A928E8u});
}
void func_08A6B2BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[2] = 1u; G[19] = G[5]; G[17] = G[7]; if (G[6] == G[2]) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08CB0504u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0xD4u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[16] = word(rt, G[17]); if (G[16] == 0u) { G[4] = 0x08CB0504u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xDCu); G[5] = 0u; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17]); G[18] = G[2]; if (G[16] == 0u) { G[4] = 0x08CB0504u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[17] = frame.at(8u); G[2] = word(rt, G[3] + 0xD8u); G[20] = frame.at(4u); call_here(rt, ctx, G[2]); G[6] = G[2]; G[5] = G[18]; G[4] = G[17]; G[7] = frame.at(1u); call_here(rt, ctx, 0x08C00AD4u); G[5] = G[19]; G[4] = G[20]; call_here(rt, ctx, 0x08A6AF48u); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08CB0584u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08A6AD7Cu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x08A62010 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CB033C, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then 0x08B8850C(object, 0x08CF6F14, 0x08A62B08), 0x08B8A004(object, 0x08CF6F24, 0x08A64340), 0x08B8A004(object, 0x08CF6F34,
// 0x08A64198), 0x08B8D51C(object, 0x08CB0344, 0x08A61E58, 0), 0x08B87108(object, 0x08CB0350, 0x08A64198) and 0x08B87108(object, 0x08CB0358, 0x08A642C8).
void func_08A62010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB033Cu, frame.at(0x1u)});
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
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A62B08u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08A64340u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F34u, 0x08A64198u});
    call_set(rt, ctx, 0x08B8D51Cu, {object, 0x08CB0344u, 0x08A61E58u, 0x0u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB0350u, 0x08A64198u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB0358u, 0x08A642C8u});
}
// 0x08AB4248 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CB3DA8, sp + 1)
// formats the text on the frame (+4); the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none
// left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: it is released again (0x08A5BA08), the old one at +0x38 is released (0x08A5B930) and the
// new one is stored there. Then 0x08B8850C(object, 0x08CF6F14, 0x08AB4910), 0x08B8A004(object, 0x08CF6F24, 0x08AB45B4), 0x08B87108(object, 0x08CB3DB0,
// 0x08AB44C0), 0x08B8A004(object, 0x08CF6F34, 0x08AB44C0), 0x08B87108(object, 0x08CB3DB8, 0x08AB43B4) and 0x08B8D51C(object, 0x08CB3DC0, 0x08AB4000, 0).
void func_08AB4248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB3DA8u, frame.at(0x1u)});
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
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08AB4910u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08AB45B4u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB3DB0u, 0x08AB44C0u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F34u, 0x08AB44C0u});
    call_set(rt, ctx, 0x08B87108u, {object, 0x08CB3DB8u, 0x08AB43B4u});
    call_set(rt, ctx, 0x08B8D51Cu, {object, 0x08CB3DC0u, 0x08AB4000u, 0x0u});
}
void func_089BB55C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[22] = G[4]; G[20] = G[5]; call_here(rt, ctx, 0x089BA708u); G[18] = word(rt, G[22] + 0x44u); G[19] = word(rt, G[20] + 0x44u); while (G[18] != 0u) { G[16] = word(rt, G[18] + 0xCu); G[17] = word(rt, G[19] + 0xCu); while (G[16] != 0u) { G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x089BB55Cu); G[16] = word(rt, G[16] + 0x54u); G[17] = word(rt, G[17] + 0x54u); } G[18] = word(rt, G[18] + 0x18u); G[19] = word(rt, G[19] + 0x18u); } G[21] = frame.at(8u); G[4] = G[21]; G[19] = frame.at(0xCu); call_here(rt, ctx, 0x08A76AA0u); G[4] = G[19]; call_here(rt, ctx, 0x08A5D438u); G[2] = word(rt, G[20]); G[6] = 0x08CF6F30u; G[4] = G[20]; G[3] = word(rt, G[2] + 0x10Cu); G[5] = G[19]; call_here(rt, ctx, G[3]); G[18] = word(rt, frame.at(0xCu)); G[16] = frame.at(4u); if (G[18] != 0u) { G[2] = word(rt, G[18]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x1Cu); G[5] = frame.at(0u); put(rt, frame.at(0u), G[22]); call_here(rt, ctx, 0x089BAC3Cu); G[4] = G[18]; G[5] = G[21]; G[6] = G[16]; call_here(rt, ctx, G[17]); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } G[2] = word(rt, frame.at(8u)); G[17] = frame.at(0x10u); if (G[2] != 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08A76AA0u); G[16] = word(rt, frame.at(8u)); if (G[16] == 0u) { G[4] = 0x08CA5CA8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[4] = G[16]; G[5] = G[17]; G[3] = word(rt, G[2] + 0xCCu); G[6] = 0u; G[7] = 0u; call_here(rt, ctx, G[3]); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x34u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_0882FF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[19] = G[4]; G[18] = G[5]; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[20] = frame.at(0x10u); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[20]; G[17] = G[2]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(0x10u)); if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; G[5] = G[17]; call_here(rt, ctx, 0x0885E524u); G[17] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = frame.at(8u); if (G[17] != 0u) { G[5] = G[17]; G[4] = G[16]; G[17] = frame.at(0xCu); G[6] = frame.at(1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0xCu)); if (G[19] != G[17]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } else { G[16] = frame.at(4u); G[5] = 0x08CF6ED8u; G[4] = G[16]; call_here(rt, ctx, 0x08A76AACu); G[5] = word(rt, frame.at(4u)); if (G[19] != G[16]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } } G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_08830100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[19] = G[4]; G[18] = G[5]; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); if (G[16] == 0u) { G[4] = 0x08C9A2B4u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0x3Cu); G[20] = frame.at(0x10u); call_here(rt, ctx, G[2]); G[5] = G[18]; G[4] = G[20]; G[17] = G[2]; call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(0x10u)); if (G[16] == 0u) { G[4] = 0x08C9A2F8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[2]; G[5] = G[17]; call_here(rt, ctx, 0x0885E54Cu); G[17] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = frame.at(8u); if (G[17] != 0u) { G[5] = G[17]; G[4] = G[16]; G[17] = frame.at(0xCu); G[6] = frame.at(1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); G[5] = word(rt, frame.at(0xCu)); if (G[19] != G[17]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[2]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } else { G[16] = frame.at(4u); G[5] = 0x08CF6ED8u; G[4] = G[16]; call_here(rt, ctx, 0x08A76AACu); G[5] = word(rt, frame.at(4u)); if (G[19] != G[16]) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } } G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
void func_0883728C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); G[5] = static_cast<std::int32_t>(G[5]) < 3 ? 1u : 0u; G[17] = G[6]; if (G[5] == 0u) { G[16] = word(rt, G[6]); if (G[16] == 0u) { G[4] = 0x08C9B798u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[5] = G[16]; G[3] = word(rt, G[2] + 0xCu); G[4] = frame.at(4u); call_here(rt, ctx, G[3]); G[5] = word(rt, frame.at(4u)); G[4] = 0x08CE2BDCu; G[6] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B20A54u); G[16] = word(rt, G[17] + 4u); G[19] = G[2]; if (G[16] == 0u) { G[4] = 0x08C9B798u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[5] = G[16]; G[3] = word(rt, G[2] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[3]); G[5] = word(rt, frame.at(8u)); G[4] = 0x08CE2BD4u; G[6] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B20A54u); G[16] = word(rt, G[17] + 8u); G[18] = G[2]; if (G[16] == 0u) { G[4] = 0x08C9B798u; call_here(rt, ctx, 0x08A5BB7Cu); } G[2] = word(rt, G[16]); G[3] = word(rt, G[2] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[3]); F[12] = F[0]; G[4] = G[19]; G[5] = G[18]; call_here(rt, ctx, 0x089107A0u); G[4] = 0u; call_here(rt, ctx, 0x089106A4u); G[4] = word(rt, frame.at(8u)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = word(rt, frame.at(4u)); G[16] = G[4] - 0xCu; if (G[16] != G[17]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0x0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[17] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); }  } } }
void func_0883976C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x220u); put(rt, frame.at(0x1F4u), G[17]); put(rt, frame.at(0x20Cu), G[23]); put(rt, frame.at(0x1F0u), G[16]); put(rt, frame.at(0x214u), G[31]); put(rt, frame.at(0x210u), G[30]); put(rt, frame.at(0x208u), G[22]); put(rt, frame.at(0x204u), G[21]); put(rt, frame.at(0x200u), G[20]); put(rt, frame.at(0x1FCu), G[19]); put(rt, frame.at(0x1F8u), G[18]); G[17] = frame.at(4u); G[23] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08838FBCu); G[16] = word(rt, frame.at(4u)); if (G[16] == 0u) { G[4] = 0x08C9BBF8u; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08838DC4u); G[22] = frame.at(8u); G[4] = G[17]; G[16] = G[2]; G[17] = frame.at(0x10u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; call_here(rt, ctx, 0x08A98A70u); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08930F8Cu); G[19] = G[17]; if (G[2] != 0u) { G[21] = frame.at(0x1F0u); G[20] = 0xFFFFFFFFu; G[30] = 0x08CA0000u; G[16] = word(rt, G[19]); do { G[17] = word(rt, G[19] + 4u); if (G[20] != G[16] || G[20] != G[17]) { G[18] = word(rt, frame.at(8u)); if (G[18] == 0u) { G[4] = 0x08C9BC1Cu; call_here(rt, ctx, 0x08A5BB7Cu); } G[18] += 0x14u; G[6] = G[16]; G[7] = G[17]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5DEE8u); G[5] = word(rt, G[18] + 4u); G[2] = word(rt, G[18] + 8u); if (G[5] == G[2]) { G[4] = G[18]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B8A03Cu); } else { G[4] = G[5]; if (G[5] != 0u) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08A76ABCu); G[5] = word(rt, G[18] + 4u); } G[5] += 4u; put(rt, G[18] + 4u, G[5]); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[19] += 8u; if (G[19] == G[21]) break; G[16] = word(rt, G[19]); } while (true); } G[5] = word(rt, frame.at(8u)); if (G[23] != G[22]) { G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, frame.at(0x1F4u)); G[23] = word(rt, frame.at(0x20Cu)); G[16] = word(rt, frame.at(0x1F0u)); G[30] = word(rt, frame.at(0x210u)); G[22] = word(rt, frame.at(0x208u)); G[21] = word(rt, frame.at(0x204u)); G[20] = word(rt, frame.at(0x200u)); G[19] = word(rt, frame.at(0x1FCu)); G[18] = word(rt, frame.at(0x1F8u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_307
} // namespace gt::game

#define HOT307(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_307::name, gt::ReturnKind::kind)
HOT307(0x08A21054u, func_08A21054, Word);
HOT307(0x08A31B54u, func_08A31B54, Word);
HOT307(0x08A71844u, func_08A71844, Word);
HOT307(0x08B12ABCu, func_08B12ABC, Word);
HOT307(0x08B402E4u, func_08B402E4, Word);
HOT307(0x08A92DF8u, func_08A92DF8, Word);
HOT307(0x08A6B2BCu, func_08A6B2BC, Word);
HOT307(0x08A62010u, func_08A62010, Word);
HOT307(0x08AB4248u, func_08AB4248, Word);
// HOT307(0x089BB55Cu, func_089BB55C, Word);  // no comparable synthetic case yet
HOT307(0x0882FF98u, func_0882FF98, Word);
HOT307(0x08830100u, func_08830100, Word);
HOT307(0x0883728Cu, func_0883728C, Word);
// HOT307(0x0883976Cu, func_0883976C, Word);  // no comparable synthetic case yet
