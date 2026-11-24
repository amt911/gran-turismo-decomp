// Hot batch 318: functions no route reaches (straight-line code, forward branches, simple loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyx.py), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_318 {
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
// `trunc.w.s` as the recompiled baseline does it: NaN and +inf give 0x7FFFFFFF, -inf 0x80000000, finite values
// truncate toward zero and clamp to the int32 range.
std::uint32_t trunc_to_word(float value) {
    if (std::isnan(value)) return 0x7FFFFFFFu;
    const double t = std::trunc(static_cast<double>(value));
    if (t >= 2147483647.0) return 0x7FFFFFFFu;
    if (t <= -2147483648.0) return 0x80000000u;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(t));
}
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

// 0x08877090 (a0 = the object; v0 = the last call's): two 3-float vectors on the frame (+0x1C and +0x10, a third word at +4 is the position of the first) are set up by 0x08AB85F0(vector), then
// 0x088755E4(object, position, vector, 3) and 0x088755E4(object, vector2, vector, 3) fill them. 0x08AB856C(vector) feeds 0x08B77BF4 and its 64-bit result 0x08B71EC8, whose float result is kept in f20.
// With the count word at +0x24 at least 7 the lookup 0x088757B0(object, 0x08CA0074) may give an entry: the second vector is then scaled by 0.0174532925 (0x3C8EFA35) and 0x088726B8(entry, position, vector2) is
// called with f12 = f20. The vector's counted string (header = data - 0xC) is released: unless it is the empty header 0x08D75298 the count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B93554(header, sp) frees it.
void func_08877090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x30u}, {19u, 0x3Cu}, {17u, 0x34u}, {31u, 0x40u}, {18u, 0x38u}, {52u, 0x50u}});
    const u32 vector = frame.at(0x1Cu), position = frame.at(0x4u), vector2 = frame.at(0x10u);
    ctx.gpr[16] = vector;
    ctx.gpr[19] = position;
    ctx.gpr[17] = object;
    ctx.gpr[18] = vector2;
    call_set(rt, ctx, 0x08AB85F0u, {vector});
    call_set(rt, ctx, 0x088755E4u, {object, position, vector, 3u});
    call_set(rt, ctx, 0x088755E4u, {object, vector2, vector, 3u});
    const u32 length = call_set(rt, ctx, 0x08AB856Cu, {vector});
    call_set(rt, ctx, 0x08B77BF4u, {length});
    call_set(rt, ctx, 0x08B71EC8u, {ctx.gpr[2], ctx.gpr[3]});
    ctx.fpr[20] = ctx.fpr[0];
    if (static_cast<s32>(word(rt, frame.at(0x24u))) >= 7) {
        const u32 entry = call_set(rt, ctx, 0x088757B0u, {object, 0x08CA0074u});
        ctx.fpr[3] = std::bit_cast<float>(0x3C8EFA35u);
        ctx.fpr[12] = ctx.fpr[20];
        if (entry != 0u) {
            for (const u32 offset : {0x10u, 0x14u, 0x18u}) put_float(rt, frame.at(offset), psp_mul(float_at(rt, frame.at(offset)), ctx.fpr[3]));
            call_set(rt, ctx, 0x088726B8u, {entry, position, vector2});
        }
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
void func_08B062BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x48u), G[31]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); auto &F = ctx.fpr; G[21] = G[6]; G[20] = G[4] + 0xCu; G[19] = G[5]; G[18] = G[4] + 0x30u; G[16] = G[4]; G[17] = G[16] + 0x3Cu; put_float(rt, frame.at(0x54u), F[21]); put_float(rt, frame.at(0x50u), F[20]); G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); G[2] = word(rt, G[5] + 0xD8u); F[12] = float_at(rt, G[2] + 0x60u); call_here(rt, ctx, 0x08C45E40u); G[6] = G[3]; G[4] = G[18]; G[5] = frame.at(0xCu); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x20u), G[2]); put(rt, frame.at(0x24u), G[3]); put(rt, frame.at(0x10u), G[3]); call_here(rt, ctx, 0x08C46718u); G[5] = G[19] + 0x5Cu; G[4] = G[20]; call_here(rt, ctx, 0x08C4AC84u); G[5] = G[2]; G[6] = G[3]; G[4] = G[17]; put(rt, frame.at(0x20u), G[2]); put(rt, frame.at(0x24u), G[3]); put(rt, frame.at(0x4u), G[2]); put(rt, frame.at(0x8u), G[3]); call_here(rt, ctx, 0x08C4B5C8u); G[4] = G[18]; G[5] = G[16]; call_here(rt, ctx, 0x08BBBEC4u); F[21] = F[0]; G[4] = G[17]; G[5] = G[20]; call_here(rt, ctx, 0x08BBBEC4u); F[21] = F[21] + F[0]; G[4] = G[18]; G[5] = frame.at(0u); F[20] = F[20] / F[21]; put_float(rt, frame.at(0x0u), F[20]); call_here(rt, ctx, 0x08C4ACD0u); G[4] = G[17]; G[5] = frame.at(0u); put_float(rt, frame.at(0x0u), F[20]); call_here(rt, ctx, 0x08C4ACD0u); G[5] = G[16]; F[0] = float_at(rt, G[16] + 0x64u); G[2] = word(rt, G[16] + 0x6Cu); F[12] = float_at(rt, G[16] + 0x60u); F[20] = psp_mul(F[20], F[0]); G[2] = (G[2] >> 12) & 0x3FFu; G[4] = G[2] << 5; G[2] = G[2] << 3; G[4] = G[4] - G[2]; put_float(rt, G[16] + 0x64u, F[20]); G[4] = G[4] + G[21]; call_here(rt, ctx, 0x08B057F0u); F[21] = float_at(rt, frame.at(0x54u)); F[20] = float_at(rt, frame.at(0x50u));  G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
// 0x08AA027C (a0 = where the float result goes, a1 = the value the builder takes; v0 = 1 when the work ran, 0 when 0x08A69530 reported
// nothing to do). 0x08B85EB8(sp+8, a1, sp+1) builds a transient at sp+8 whose 0x08A69530(sp+8) result decides: the counted string at sp+8
// (header = data - 0xC) is released unless it is the empty header 0x08D74FD0 (the count 0x08B60804(data - 4, -1) drops and, with none left,
// 0x08B8542C(header, sp) frees it). With a result, the two temporaries sp+0xC (0x08A9DD14) and sp+0x10 (0x08A87EEC, f12 = 0.0) are
// null-checked (0x08CB277C, 0x08CB2784), 0x08A69B3C(sp+4, a1), 0x08A7BFD0(result, sp+0x10, sp+4) and 0x08A69D68(sp+4) do the work, and the
// word at sp+0x10 then feeds 0x08A87824, whose float result goes to *a0. Both temporaries are released with 0x08A5B9EC; v0 = 1 when that
// word was non-null.
void func_08AA027C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x20u}, {20u, 0x30u}, {31u, 0x38u}, {19u, 0x2Cu}, {17u, 0x24u}, {21u, 0x34u}, {18u, 0x28u}});
    const u32 out = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 built = frame.at(0x8u), first = frame.at(0xCu), second = frame.at(0x10u), argument = frame.at(0x4u);
    ctx.gpr[16] = built;
    ctx.gpr[20] = out;
    ctx.gpr[19] = source;
    call_set(rt, ctx, 0x08B85EB8u, {built, source, frame.at(0x1u)});
    ctx.gpr[16] = built;
    const u32 entry = call_set(rt, ctx, 0x08A69530u, {built});
    ctx.gpr[17] = entry;
    const u32 data = word(rt, frame.at(0x8u));
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
        }
    }
    u32 result = 0u;
    ctx.gpr[16] = 0u;
    if (entry != 0u) {
        ctx.gpr[21] = first;
        call_set(rt, ctx, 0x08A9DD14u, {first});
        ctx.fpr[12] = std::bit_cast<float>(0u);
        ctx.gpr[18] = second;
        call_set(rt, ctx, 0x08A87EECu, {second});
        const u32 handle = word(rt, frame.at(0xCu));
        ctx.gpr[16] = handle;
        if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB277Cu});
        const u32 value = call_set(rt, ctx, 0x08A9BD30u, {handle});
        const u32 shared = word(rt, value + 0x0u);
        ctx.gpr[17] = shared;
        if (shared == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2784u});
        ctx.gpr[16] = argument;
        call_set(rt, ctx, 0x08A69B3Cu, {argument, source});
        call_set(rt, ctx, 0x08A7BFD0u, {shared, second, argument});
        call_set(rt, ctx, 0x08A69D68u, {argument});
        ctx.gpr[16] = 0u;
        const u32 finish = word(rt, frame.at(0x10u));
        if (finish != 0u) {
            ctx.gpr[16] = 1u;
            call_set(rt, ctx, 0x08A87824u, {finish});
            put_float(rt, out + 0x0u, ctx.fpr[0]);
            result = 1u;
        }
        call_set(rt, ctx, 0x08A5B9ECu, {second});
        call_set(rt, ctx, 0x08A5B9ECu, {first});
    }
    ctx.gpr[2] = result;
}
void func_0889B0B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[18] = G[4] + 0x20u; G[17] = G[5]; G[16] = G[4]; G[4] = G[18]; put_float(rt, frame.at(0x20u), F[20]); G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); F[9] = float_at(rt, G[5] + 0x0u); F[10] = float_at(rt, G[5] + 0x4u); F[2] = float_at(rt, G[6] + 0x0u); F[3] = float_at(rt, G[6] + 0x4u); F[8] = float_at(rt, G[5] + 0x8u); F[2] = F[2] - F[9]; F[3] = F[3] - F[10]; F[1] = float_at(rt, G[6] + 0x8u); F[6] = float_at(rt, G[7] + 0x8u); F[0] = psp_mul(F[2], F[2]); F[1] = F[1] - F[8]; F[5] = psp_mul(F[3], F[3]); F[4] = float_at(rt, G[7] + 0x0u); F[6] = F[6] - F[8]; F[7] = psp_mul(F[1], F[1]); F[0] = F[0] + F[5]; F[5] = float_at(rt, G[7] + 0x4u); F[4] = F[4] - F[9]; put_float(rt, frame.at(0x8u), F[6]); F[0] = F[0] + F[7]; F[5] = F[5] - F[10]; put_float(rt, frame.at(0x0u), F[4]); G[5] = G[16]; F[0] = std::sqrt(F[0]); put_float(rt, frame.at(0x4u), F[5]); G[6] = frame.at(0u); F[0] = F[20] / F[0]; F[1] = psp_mul(F[1], F[0]); F[2] = psp_mul(F[2], F[0]); F[3] = psp_mul(F[3], F[0]); put_float(rt, G[16] + 0x8u, F[1]); put_float(rt, G[16] + 0x0u, F[2]); put_float(rt, G[16] + 0x4u, F[3]); call_here(rt, ctx, 0x088996D8u); G[5] = G[18]; F[2] = float_at(rt, G[16] + 0x20u); F[3] = float_at(rt, G[16] + 0x24u); F[1] = float_at(rt, G[16] + 0x28u); F[0] = psp_mul(F[2], F[2]); F[4] = psp_mul(F[3], F[3]); F[5] = psp_mul(F[1], F[1]); G[4] = G[16] + 0x10u; G[6] = G[16]; F[0] = F[0] + F[4]; F[0] = F[0] + F[5]; F[0] = std::sqrt(F[0]); F[0] = F[20] / F[0]; F[1] = psp_mul(F[0], F[1]); F[2] = psp_mul(F[0], F[2]); F[0] = psp_mul(F[0], F[3]); put_float(rt, G[16] + 0x28u, F[1]); put_float(rt, G[16] + 0x24u, F[0]); put_float(rt, G[16] + 0x20u, F[2]); call_here(rt, ctx, 0x088996D8u); put(rt, G[16] + 0xCu, 0u); put(rt, G[16] + 0x1Cu, 0u); put(rt, G[16] + 0x2Cu, 0u); F[0] = float_at(rt, G[17] + 0x0u); put_float(rt, G[16] + 0x30u, F[0]); F[1] = float_at(rt, G[17] + 0x4u); put_float(rt, G[16] + 0x34u, F[1]); F[0] = float_at(rt, G[17] + 0x8u); put_float(rt, G[16] + 0x3Cu, F[20]); put_float(rt, G[16] + 0x38u, F[0]); F[20] = float_at(rt, frame.at(0x20u));  G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
// 0x089DE7D4 (a0 = the object; v0 = its flag word at +0x140 after the update): constructor. The base 0x0896BDF8(object) runs first; the object gets two vtable words (+0 = 0x08CBC738, +0x38 = 0x08CBC970), the word 0x08C8CC04 at +0xE8,
// six zeroed 12-byte entries from +0xEC to +0x128, the empty-string data pointer (0x08D74FD0 + 0xC) at +0x134 and two members built with 0x089E4BB8 (+0x138, +0x13C). The flag word at +0x140 gets bits 0-1 set and bits 2-12
// cleared; +0x144 is cleared; three 16-byte records follow (+0x150, +0x170, +0x190: the last word -1, the float 1.0 in the first one's three words and in the others' last word, the rest 0) and the words +0x1B0..+0x1BC are cleared.
void func_089DE7D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x0896BDF8u, {self});
    put(rt, self, 0x08CBC738u);
    put(rt, self + 0x38u, 0x08CBC970u);
    put(rt, self + 0xE8u, word(rt, 0x08C8CC04u));
    for (u32 entry = self + 0xECu; entry < self + 0x134u; entry += 0xCu) {
        put(rt, entry, 0u);
        put(rt, entry + 0x4u, 0u);
        put(rt, entry + 0x8u, 0u);
    }
    put(rt, self + 0x134u, 0x08D74FD0u + 0xCu);
    call_set(rt, ctx, 0x089E4BB8u, {self + 0x138u});
    call_set(rt, ctx, 0x089E4BB8u, {self + 0x13Cu});
    const float one = 1.0f;
    ctx.fpr[0] = one;
    // bits 2-12 cleared (`ins flags, zero, 2, 11`)
    const u32 flags = (word(rt, self + 0x140u) | 0x3u) & ~(((1u << 11) - 1u) << 2);
    ctx.gpr[2] = flags;
    put(rt, self + 0x140u, flags);
    put(rt, self + 0x144u, 0u);
    const u32 first = self + 0x150u, second = self + 0x170u, third = self + 0x190u;
    put_float(rt, first, one);
    put(rt, first + 0x10u, 0xFFFFFFFFu);
    put_float(rt, first + 0x4u, one);
    put_float(rt, first + 0x8u, one);
    put_float(rt, first + 0xCu, one);
    put(rt, second, 0u);
    put(rt, second + 0x10u, 0xFFFFFFFFu);
    put(rt, second + 0x4u, 0u);
    put(rt, second + 0x8u, 0u);
    put_float(rt, second + 0xCu, one);
    put(rt, third, 0u);
    put(rt, third + 0x10u, 0xFFFFFFFFu);
    put_float(rt, third + 0xCu, one);
    put(rt, third + 0x4u, 0u);
    put(rt, third + 0x8u, 0u);
    for (const u32 offset : {0x1B8u, 0x1B0u, 0x1BCu, 0x1B4u}) put(rt, self + offset, 0u);
}
void func_0896ABF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x34u), G[31]); auto &F = ctx.fpr; G[5] = 0u; G[16] = G[4]; put_float(rt, frame.at(0x44u), F[21]); F[21] = F[13]; put_float(rt, frame.at(0x40u), F[20]); F[20] = F[12]; call_here(rt, ctx, 0x0896A178u); G[4] = G[16]; G[5] = frame.at(0u); G[6] = frame.at(0x4u); G[7] = frame.at(0x8u); G[8] = frame.at(0xCu); call_here(rt, ctx, 0x0896A35Cu); F[3] = float_at(rt, frame.at(0x0u)); F[2] = float_at(rt, frame.at(0x4u)); F[1] = float_at(rt, frame.at(0x8u)); F[0] = float_at(rt, frame.at(0xCu)); F[3] = psp_mul(F[3], F[20]); F[2] = psp_mul(F[2], F[21]); F[1] = psp_mul(F[20], F[1]); F[0] = psp_mul(F[21], F[0]); G[5] = frame.at(0x2Cu); G[6] = frame.at(0x28u); G[7] = frame.at(0x24u); G[8] = frame.at(0x20u); G[4] = G[16]; put_float(rt, frame.at(0x2Cu), F[3]); put_float(rt, frame.at(0x28u), F[2]); put_float(rt, frame.at(0x24u), F[1]); put_float(rt, frame.at(0x20u), F[0]); put_float(rt, frame.at(0x0u), F[3]); put_float(rt, frame.at(0x4u), F[2]); put_float(rt, frame.at(0x8u), F[1]); put_float(rt, frame.at(0xCu), F[0]); call_here(rt, ctx, 0x0896A9C4u); G[4] = G[16]; call_here(rt, ctx, 0x0896A4E0u); G[4] = G[16]; G[5] = frame.at(0x10u); G[6] = frame.at(0x14u); G[7] = frame.at(0x18u); G[8] = frame.at(0x1Cu); call_here(rt, ctx, 0x08966B20u); F[0] = float_at(rt, frame.at(0x10u)); F[1] = float_at(rt, frame.at(0x14u)); G[4] = G[16]; F[3] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[5] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); F[0] = float_at(rt, frame.at(0x18u)); F[1] = float_at(rt, frame.at(0x1Cu)); F[3] = psp_mul(F[20], F[3]); F[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[4] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); F[20] = psp_mul(F[20], F[5]); G[1] = 0x3F000000u; F[0] = std::bit_cast<float>(G[1]); F[2] = psp_mul(F[21], F[2]); F[21] = psp_mul(F[21], F[4]); F[3] = F[3] + F[0]; F[20] = F[20] + F[0]; F[2] = F[2] + F[0]; F[21] = F[21] + F[0]; F[3] = std::bit_cast<float>(trunc_to_word(F[3])); F[20] = std::bit_cast<float>(trunc_to_word(F[20])); F[2] = std::bit_cast<float>(trunc_to_word(F[2])); F[21] = std::bit_cast<float>(trunc_to_word(F[21])); G[5] = bits_of(F[3]); G[6] = bits_of(F[20]); G[7] = bits_of(F[2]); G[8] = bits_of(F[21]); put_float(rt, frame.at(0x14u), F[20]); put_float(rt, frame.at(0x1Cu), F[21]); put_float(rt, frame.at(0x10u), F[3]); put_float(rt, frame.at(0x18u), F[2]); call_here(rt, ctx, 0x089668D8u); F[21] = float_at(rt, frame.at(0x44u)); F[20] = float_at(rt, frame.at(0x40u));  G[16] = word(rt, frame.at(0x30u)); }  } }
void func_088785C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x38u), G[18]); auto &F = ctx.fpr; G[16] = frame.at(0x4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088755B4u); G[4] = G[16]; G[18] = G[2]; call_here(rt, ctx, 0x08AB856Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B77BF4u); G[5] = G[3]; G[4] = G[2]; call_here(rt, ctx, 0x08B71EC8u); G[4] = G[16]; put_float(rt, frame.at(0x14u), F[0]); call_here(rt, ctx, 0x08AB856Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B77BF4u); G[5] = G[3]; G[4] = G[2]; call_here(rt, ctx, 0x08B71EC8u); G[4] = G[16]; put_float(rt, frame.at(0x18u), F[0]); call_here(rt, ctx, 0x08AB856Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B77BF4u); G[5] = G[3]; G[4] = G[2]; call_here(rt, ctx, 0x08B71EC8u); G[4] = G[16]; put_float(rt, frame.at(0x1Cu), F[0]); call_here(rt, ctx, 0x08AB856Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B77BF4u); G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B71EC8u); G[4] = G[16]; put_float(rt, frame.at(0x20u), F[0]); call_here(rt, ctx, 0x08AB856Cu); G[6] = G[2]; G[2] = word(rt, frame.at(0xCu)); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[5] = 0x08CA0000u; if (!tk) { G[4] = G[17]; G[5] = 0x08CA0088u; call_here(rt, ctx, 0x088756F0u); { const bool tk = G[2] == 0u; G[4] = G[2]; if (!tk) { G[5] = 0x2u; call_here(rt, ctx, 0x0886A9A0u); { const bool tk = G[2] == 0u; G[5] = frame.at(0x14u); if (!tk) { if (G[18] == 0u) G[5] = 0u; G[4] = G[2]; call_here(rt, ctx, 0x088824F8u); } } } } } } G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D75298u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[31] = word(rt, frame.at(0x3Cu)); if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[31] = word(rt, frame.at(0x3Cu)); if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); } } } }  G[16] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x38u)); }  } }
void func_08996DF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); auto &F = ctx.fpr; G[2] = 0x1u; G[19] = G[4]; G[16] = G[5]; G[5] = G[7]; { const bool tk = G[6] != G[2]; if (!tk) { G[20] = frame.at(0x10u); G[4] = G[20]; G[17] = frame.at(0xCu); call_here(rt, ctx, 0x08969244u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08996570u); G[16] = word(rt, frame.at(0xCu)); { const bool tk = G[16] != 0u; if (tk) { G[5] = word(rt, frame.at(0x10u)); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA43BCu; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, frame.at(0x10u)); } } G[4] = G[16]; G[6] = frame.at(0x14u); G[7] = frame.at(0x18u); call_here(rt, ctx, 0x089966DCu); G[16] = frame.at(0x8u); G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; G[5] = 0x2u; call_here(rt, ctx, 0x08A98C1Cu); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (!tk) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, G[19] + 0x0u); { const bool tk = G[18] != 0u; if (tk) { G[2] = word(rt, G[18] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA42E0u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[18] + 0x0u); } } F[12] = float_at(rt, frame.at(0x14u)); G[16] = frame.at(0x4u); G[17] = word(rt, G[2] + 0x88u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); G[4] = G[18]; G[5] = 0u; { const std::uint32_t tg = G[17]; G[6] = G[16]; call_here(rt, ctx, tg); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[17] = word(rt, G[19] + 0x0u); { const bool tk = G[17] != 0u; if (tk) { G[2] = word(rt, G[17] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA42E0u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[17] + 0x0u); } } F[12] = float_at(rt, frame.at(0x14u)); G[4] = frame.at(0u); G[16] = word(rt, G[2] + 0x88u); call_here(rt, ctx, 0x08A87EECu); G[4] = G[17]; G[5] = 0x1u; { const std::uint32_t tg = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, tg); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } }  G[19] = word(rt, frame.at(0x2Cu)); G[16] = word(rt, frame.at(0x20u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); }  } }
void func_089780E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[18] = G[7]; G[17] = G[6]; put_float(rt, frame.at(0x34u), F[21]); { const bool tk = static_cast<std::int32_t>(G[6]) <= 0; put_float(rt, frame.at(0x30u), F[20]); if (!tk) { G[4] = frame.at(0u); G[20] = frame.at(0x4u); call_here(rt, ctx, 0x08978000u); G[4] = G[20]; G[5] = G[18]; call_here(rt, ctx, 0x08A6C410u); G[19] = frame.at(0x8u); G[5] = 0x08C90000u; G[5] = 0x08C8BDACu; G[4] = G[19]; call_here(rt, ctx, 0x08B20920u); G[16] = word(rt, frame.at(0x4u)); { const bool tk = G[16] != 0u; G[4] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA2F8Cu; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08A6BFFCu); G[5] = G[2]; G[4] = G[19]; G[6] = 0u; call_here(rt, ctx, 0x08B20A54u); G[19] = G[2]; G[2] = static_cast<std::int32_t>(G[17]) < 2 ? 1u : 0u; G[1] = 0x3F800000u; F[21] = std::bit_cast<float>(G[1]); { const bool tk = G[2] != 0u; F[20] = std::bit_cast<float>(0u); if (!tk) { G[16] = word(rt, G[18] + 0x4u); { const bool tk = G[16] != 0u; if (tk) { G[2] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA2F84u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[3] = word(rt, G[2] + 0x54u); { const std::uint32_t tg = G[3]; F[20] = std::bit_cast<float>(0u); call_here(rt, ctx, tg); } G[2] = static_cast<std::int32_t>(G[17]) < 3 ? 1u : 0u; { const bool tk = G[2] != 0u; F[21] = F[0]; if (!tk) { G[16] = word(rt, G[18] + 0x8u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA2F84u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } F[20] = F[0]; } } } } G[16] = word(rt, frame.at(0x0u)); { const bool tk = G[16] != 0u; F[12] = F[21]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08CA2F94u; call_here(rt, ctx, 0x08A5BB7Cu); F[12] = F[21]; } } F[13] = F[20]; G[5] = G[19]; G[4] = G[16]; call_here(rt, ctx, 0x089772C0u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } } F[21] = float_at(rt, frame.at(0x34u)); F[20] = float_at(rt, frame.at(0x30u));  G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0899FD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); auto &F = ctx.fpr; G[19] = frame.at(0x8u); G[17] = G[4]; G[4] = G[19]; G[18] = G[5]; call_here(rt, ctx, 0x08A36C3Cu); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08CA4A0Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; } } G[5] = 0x08CA4A14u; G[4] = G[16]; call_here(rt, ctx, 0x08A34DA8u); G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, G[17] + 0x13Cu); G[2] = G[3] & 0x10u; { const bool tk = G[2] == 0u; G[2] = G[3]; if (!tk) { F[12] = float_at(rt, G[17] + 0x150u); G[4] = G[17]; call_here(rt, ctx, 0x0899D06Cu); F[12] = float_at(rt, G[17] + 0x120u); G[4] = G[17]; call_here(rt, ctx, 0x0899D0D0u); F[12] = float_at(rt, G[17] + 0x120u); G[4] = G[17]; call_here(rt, ctx, 0x0899D108u); G[3] = word(rt, G[17] + 0x13Cu); G[2] = G[3]; } } G[2] = (G[2] & ~0x8u) | ((0u << 3) & 0x8u); G[5] = (G[2] >> 2) & 0x1u; put(rt, G[17] + 0x13Cu, G[2]); G[4] = G[18]; call_here(rt, ctx, 0x089AF5E8u); G[4] = G[18]; G[5] = 0u; call_here(rt, ctx, 0x089AB114u); G[2] = word(rt, G[17] + 0x164u); { const bool tk = G[2] == 0u; G[20] = frame.at(0xCu); if (!tk) { G[4] = G[20]; G[19] = frame.at(0x10u); call_here(rt, ctx, 0x08A76AA0u); G[16] = frame.at(0x4u); G[4] = G[19]; call_here(rt, ctx, 0x08A76AA0u); G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0x0u), G[18]); call_here(rt, ctx, 0x08A76AACu); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17] + 0x164u); { const bool tk = G[16] != 0u; if (tk) { G[2] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA485Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[5] = G[20]; G[3] = word(rt, G[2] + 0xCCu); G[6] = 0x1u; { const std::uint32_t tg = G[3]; G[7] = G[19]; call_here(rt, ctx, tg); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } }  G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); G[20] = word(rt, frame.at(0x30u)); }  } }
void func_08866B40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x68u), G[31]); put(rt, frame.at(0x64u), G[21]); put(rt, frame.at(0x60u), G[20]); put(rt, frame.at(0x5Cu), G[19]); put(rt, frame.at(0x58u), G[18]); put(rt, frame.at(0x54u), G[17]); put(rt, frame.at(0x50u), G[16]); auto &F = ctx.fpr; G[20] = G[4]; G[21] = G[20] + 0x4524u; G[19] = G[20] + 0x45D4u; G[18] = frame.at(0x14u); G[4] = word(rt, G[4] + 0x98u); G[17] = word(rt, G[5] + 0x0u); G[16] = word(rt, G[5] + 0x4u); call_here(rt, ctx, 0x0890B3D8u); G[4] = word(rt, G[20] + 0x98u); call_here(rt, ctx, 0x0890B56Cu); G[4] = word(rt, G[20] + 0x98u); G[5] = G[16]; G[1] = 0x3F800000u; F[12] = std::bit_cast<float>(G[1]); G[6] = 0u; call_here(rt, ctx, 0x0890B3E0u); G[4] = G[21]; call_here(rt, ctx, 0x08AF2640u); G[4] = G[20] + 0x45DCu; G[5] = G[17]; call_here(rt, ctx, 0x08916C58u); G[16] = G[2]; G[2] = word(rt, G[20] + 0x98u); G[3] = G[16] + 0x4u; if (G[16] == 0u) G[3] = 0u; put(rt, G[2] + 0x30u, G[3]); G[6] = G[19]; G[7] = G[17]; G[2] = word(rt, G[20] + 0x98u); G[4] = G[20] + 0x4584u; G[5] = G[18]; G[12] = word(rt, G[2] + 0x28u); G[13] = word(rt, G[2] + 0xE0u); G[8] = word(rt, G[2] + 0x20u); G[9] = word(rt, G[2] + 0x8u); G[10] = word(rt, G[2] + 0x10u); G[11] = word(rt, G[2] + 0x18u); put(rt, frame.at(0x18u), G[8]); put(rt, frame.at(0x1Cu), G[9]); put(rt, frame.at(0x20u), G[10]); put(rt, frame.at(0x24u), G[11]); put(rt, frame.at(0x28u), G[12]); put(rt, frame.at(0x2Cu), G[3]); put(rt, frame.at(0x38u), G[13]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x30u), 0u); put(rt, frame.at(0x34u), 0u); store8(frame.at(0x3Cu), 0u); put(rt, frame.at(0x40u), 0u); call_here(rt, ctx, 0x0888A0F4u); G[6] = G[19]; G[7] = G[17]; G[4] = G[16]; { const bool tk = G[16] == 0u; G[5] = G[18]; if (!tk) { call_here(rt, ctx, 0x08917060u); } } G[2] = word(rt, G[20] + 0x98u); G[5] = frame.at(0u); put(rt, G[2] + 0x34u, G[21]); G[3] = word(rt, G[20] + 0x3F98u); G[2] = word(rt, G[20] + 0x98u); G[6] = word(rt, G[20] + 0x94u); put(rt, frame.at(0x0u), G[3]); put(rt, frame.at(0x8u), G[2]); put(rt, frame.at(0x4u), 0u); G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[2] + 0x30u); { const std::uint32_t tg = G[3]; G[4] = G[6]; call_here(rt, ctx, tg); } G[4] = word(rt, G[20] + 0x94u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x14u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); }  G[21] = word(rt, frame.at(0x64u)); G[20] = word(rt, frame.at(0x60u)); G[19] = word(rt, frame.at(0x5Cu)); G[18] = word(rt, frame.at(0x58u)); G[17] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x50u)); }  } }
void func_08BA388C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); auto &F = ctx.fpr; G[18] = G[4]; G[4] = 0x3F0u; put_float(rt, frame.at(0x14u), F[21]); G[1] = 0x447A0000u; F[21] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x10u), F[20]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08BA0210u); G[4] = G[16]; call_here(rt, ctx, 0x08BA0140u); G[17] = G[16] + 0x4u; G[4] = G[16]; call_here(rt, ctx, 0x08BA1C34u); G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); G[4] = G[17]; call_here(rt, ctx, 0x08BA2360u); put_float(rt, G[16] + 0x4u, F[20]); G[4] = G[16] + 0x8u; call_here(rt, ctx, 0x08BA22D4u); G[1] = 0x3F660000u; G[1] = G[1] | 0x6666u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 0x8u, F[0]); G[4] = G[16] + 0xCu; call_here(rt, ctx, 0x08BA21E4u); put_float(rt, G[16] + 0xCu, F[21]); G[4] = G[16] + 0x10u; call_here(rt, ctx, 0x08BA20F4u); put_float(rt, G[16] + 0x10u, F[20]); G[4] = G[16] + 0x14u; call_here(rt, ctx, 0x08BA2004u); G[4] = G[16] + 0x18u; put(rt, G[16] + 0x14u, 0u); call_here(rt, ctx, 0x08BA1F14u); G[1] = 0x43480000u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 0x18u, F[0]); G[4] = G[16] + 0x1Cu; call_here(rt, ctx, 0x08BA1E24u); G[4] = G[17]; put(rt, G[16] + 0x1Cu, 0u); G[17] = G[16] + 0x20u; call_here(rt, ctx, 0x08BA1BA8u); G[4] = G[17]; call_here(rt, ctx, 0x08BA1704u); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08BA1678u); G[1] = 0x42C80000u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 0x30u, F[0]); G[4] = G[16] + 0x34u; call_here(rt, ctx, 0x08BA1588u); put_float(rt, G[16] + 0x34u, F[21]); G[4] = G[16] + 0x38u; call_here(rt, ctx, 0x08BA1498u); G[4] = G[17]; put(rt, G[16] + 0x38u, 0u); call_here(rt, ctx, 0x08BA1AB8u); G[17] = G[16] + 0x3DCu; G[4] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08BA3718u); G[4] = G[17]; call_here(rt, ctx, 0x08B9FE44u); G[4] = G[17]; call_here(rt, ctx, 0x08BA1964u); put(rt, G[16] + 0x3ECu, 0u); G[5] = G[16]; G[4] = G[18]; put(rt, G[18] + 0x0u, 0u); G[6] = 0u; put(rt, G[18] + 0x4u, 0u); store8(G[18] + 0x8u, 0u); call_here(rt, ctx, 0x08B9E2D4u); G[2] = 0x1u; store8(G[18] + 0x8u, G[2]); G[2] = G[18]; F[21] = float_at(rt, frame.at(0x14u)); F[20] = float_at(rt, frame.at(0x10u));  G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08871908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); auto &F = ctx.fpr; G[16] = G[4]; call_here(rt, ctx, 0x0887E1E0u); G[2] = 0x08CC0000u; G[2] = 0x08CBF9A0u; G[4] = 0x70u; put(rt, G[16] + 0x0u, G[2]); call_here(rt, ctx, 0x08B62344u); G[1] = 0x3F800000u; F[1] = std::bit_cast<float>(G[1]); put(rt, G[2] + 0x0u, 0u); G[6] = G[2] + 0x4u; G[3] = 0x08D70000u; put_float(rt, G[2] + 0x4u, F[1]); G[19] = 0x08D7537Cu; G[1] = 0x40000000u; F[0] = std::bit_cast<float>(G[1]); G[3] = word(rt, G[6] + 0x20u); put(rt, G[6] + 0x8u, 0u); G[5] = G[2] + 0x28u; G[3] = (G[3] & ~0x7u) | ((0u << 0) & 0x7u); put(rt, G[6] + 0xCu, 0u); G[7] = G[2] + 0x4Cu; G[10] = 0x08D70000u; G[8] = word(rt, G[6] + 0x8u); G[9] = word(rt, G[6] + 0xCu); put(rt, G[6] + 0x20u, G[3]); G[18] = 0x08D75370u; G[4] = G[18]; put(rt, G[6] + 0x18u, G[8]); put(rt, G[6] + 0x1Cu, G[9]); put(rt, G[6] + 0x10u, G[8]); put(rt, G[6] + 0x14u, G[9]); put_float(rt, G[6] + 0x4u, F[0]); put(rt, G[2] + 0x28u, 0u); G[3] = word(rt, G[5] + 0x20u); put(rt, G[5] + 0x8u, 0u); G[3] = (G[3] & ~0x7u) | ((0u << 0) & 0x7u); put(rt, G[5] + 0xCu, 0u); G[8] = word(rt, G[5] + 0x8u); G[9] = word(rt, G[5] + 0xCu); put(rt, G[5] + 0x20u, G[3]); put(rt, G[5] + 0x18u, G[8]); put(rt, G[5] + 0x10u, G[8]); put_float(rt, G[5] + 0x4u, F[1]); put(rt, G[5] + 0x1Cu, G[9]); put(rt, G[5] + 0x14u, G[9]); put(rt, G[2] + 0x4Cu, 0u); put(rt, G[7] + 0x8u, 0u); put(rt, G[7] + 0xCu, 0u); G[3] = word(rt, G[7] + 0x20u); G[5] = word(rt, G[7] + 0x8u); G[8] = word(rt, G[7] + 0xCu); G[3] = (G[3] & ~0x7u) | ((0u << 0) & 0x7u); put_float(rt, G[7] + 0x4u, F[1]); put(rt, G[7] + 0x18u, G[5]); put(rt, G[7] + 0x1Cu, G[8]); put(rt, G[7] + 0x20u, G[3]); put(rt, G[7] + 0x10u, G[5]); put(rt, G[7] + 0x14u, G[8]); G[3] = word(rt, G[6] + 0x20u); G[3] = G[3] | 0x4u; put(rt, G[6] + 0x20u, G[3]); put(rt, G[16] + 0xCu, G[2]); G[2] = sext(load8(0x08D75370u), 8); { const bool tk = G[2] != 0u; G[17] = G[16] + 0xCu; if (!tk) { call_here(rt, ctx, 0x08B6441Cu); G[5] = 0x08D70000u; G[19] = 0x08D7537Cu; { const bool tk = G[2] == 0u; G[4] = G[18]; if (!tk) { G[19] = 0x08D7537Cu; { const bool tk = G[19] == 0u; G[2] = 0x08CC0000u; if (!tk) { G[2] = 0x08CBF9C8u; put(rt, 0x08D7537Cu, G[2]); } } call_here(rt, ctx, 0x08B64494u); } } } } put(rt, G[17] + 0x4u, G[19]);  G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); }  } }
void func_08869514(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); auto &F = ctx.fpr; G[18] = G[4] + 0x90u; G[19] = G[4]; G[4] = G[18]; G[20] = G[19] + 0x1B0u; call_here(rt, ctx, 0x0890B56Cu); G[4] = G[18]; call_here(rt, ctx, 0x0890B3D8u); G[4] = G[20]; G[5] = 0u; call_here(rt, ctx, 0x08869E84u); G[5] = 0u; { const bool tk = G[2] == 0u; G[4] = G[20]; if (!tk) { call_here(rt, ctx, 0x08869EC0u); G[4] = G[20]; G[5] = 0u; G[17] = G[2]; call_here(rt, ctx, 0x08869ECCu); G[16] = G[2]; G[4] = G[18]; G[5] = G[17]; G[1] = 0x3F800000u; F[12] = std::bit_cast<float>(G[1]); G[6] = 0u; call_here(rt, ctx, 0x0890B3E0u); G[7] = G[17]; G[4] = G[16]; { const bool tk = G[16] == 0u; put(rt, G[18] + 0x30u, G[16]); if (!tk) { G[8] = word(rt, G[18] + 0xFCu); G[6] = word(rt, G[18] + 0x8u); G[5] = word(rt, G[18] + 0x20u); G[8] = 0u < G[8] ? 1u : 0u; call_here(rt, ctx, 0x0890B00Cu); } } } } G[4] = G[20]; G[5] = 0u; G[16] = word(rt, G[18] + 0xE0u); call_here(rt, ctx, 0x08869ECCu); G[10] = word(rt, G[18] + 0x28u); G[6] = word(rt, G[18] + 0x20u); G[7] = word(rt, G[18] + 0x8u); G[8] = word(rt, G[18] + 0x10u); G[9] = word(rt, G[18] + 0x18u); G[3] = G[19] + 0x1A0u; G[4] = frame.at(0u); G[5] = 0u; put(rt, frame.at(0x4u), G[6]); put(rt, frame.at(0x8u), G[7]); put(rt, frame.at(0xCu), G[8]); put(rt, frame.at(0x10u), G[9]); put(rt, frame.at(0x14u), G[10]); put(rt, frame.at(0x18u), G[2]); put(rt, frame.at(0x0u), G[3]); put(rt, frame.at(0x24u), G[16]); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x20u), 0u); store8(frame.at(0x28u), 0u); put(rt, frame.at(0x2Cu), 0u); call_here(rt, ctx, 0x089098B8u); G[4] = word(rt, G[19] + 0x1C0u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t tg = G[2]; G[5] = frame.at(0u); call_here(rt, ctx, tg); } G[4] = G[19]; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[19] + 0x1C0u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x14u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } G[4] = G[19]; call_here(rt, ctx, 0x08B3457Cu); G[4] = word(rt, G[19] + 0x1C0u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); }  G[18] = word(rt, frame.at(0x38u)); G[19] = word(rt, frame.at(0x3Cu)); G[20] = word(rt, frame.at(0x40u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
void func_088ACBA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x4Cu), G[31]); put(rt, frame.at(0x48u), G[22]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); auto &F = ctx.fpr; G[22] = G[7] + 0x8u; G[20] = frame.at(0x28u); G[19] = G[5]; G[5] = G[6]; G[18] = frame.at(0x20u); G[17] = G[4]; G[21] = G[17] + 0x8u; G[16] = G[7]; G[2] = word(rt, G[4] + 0x3D0u); G[4] = G[18]; G[3] = G[2] << 5; G[2] = G[2] << 3; G[2] = G[2] + G[3]; G[2] = G[2] + G[7]; G[6] = G[2] + 0x284u; call_here(rt, ctx, 0x08B4AD90u); F[0] = float_at(rt, G[16] + 0x8u); F[1] = float_at(rt, G[22] + 0x4u); G[4] = frame.at(0u); G[5] = frame.at(0u); G[6] = G[21]; put_float(rt, frame.at(0x0u), F[0]); G[16] = frame.at(0x18u); put_float(rt, frame.at(0x4u), F[1]); call_here(rt, ctx, 0x08B4AC30u); G[4] = frame.at(0x8u); F[0] = float_at(rt, frame.at(0x0u)); F[1] = float_at(rt, frame.at(0x4u)); G[5] = G[4]; G[6] = G[18]; put_float(rt, frame.at(0x8u), F[0]); put_float(rt, frame.at(0xCu), F[1]); call_here(rt, ctx, 0x08B4AC0Cu); G[5] = G[19]; G[6] = word(rt, G[17] + 0x3CCu); F[1] = float_at(rt, frame.at(0xCu)); F[0] = float_at(rt, frame.at(0x8u)); G[2] = G[6] << 5; G[6] = G[6] << 3; G[6] = G[6] + G[2]; G[6] = G[6] + G[17]; G[6] = G[6] + 0x294u; G[4] = G[16]; put_float(rt, frame.at(0x14u), F[1]); put_float(rt, frame.at(0x10u), F[0]); call_here(rt, ctx, 0x08B4AD90u); G[4] = G[16]; G[5] = frame.at(0x10u); call_here(rt, ctx, 0x08B4ABCCu); G[2] = word(rt, G[17] + 0x3CCu); G[1] = 0x3F000000u; F[12] = std::bit_cast<float>(G[1]); G[4] = G[20]; G[3] = G[2] << 5; G[2] = G[2] << 3; G[2] = G[2] + G[3]; G[2] = G[2] + G[17]; F[1] = float_at(rt, G[2] + 0x29Cu); F[0] = F[0] + F[1]; F[1] = std::bit_cast<float>(0u); F[2] = -F[0]; ctx.set_fpu_condition(F[0] <= F[1]); { const bool tk = ctx.fpu_condition(); F[12] = psp_mul(F[2], F[12]); if (!tk) { F[0] = float_at(rt, frame.at(0x18u)); F[1] = float_at(rt, frame.at(0x1Cu)); put_float(rt, frame.at(0x28u), F[0]); put_float(rt, frame.at(0x2Cu), F[1]); call_here(rt, ctx, 0x08B4A560u); G[4] = G[22]; G[5] = G[22]; G[6] = G[20]; call_here(rt, ctx, 0x08B4AC0Cu); G[4] = G[21]; G[5] = G[21]; G[6] = G[20]; call_here(rt, ctx, 0x08B4AC30u); } }  G[22] = word(rt, frame.at(0x48u)); G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }
// 0x088AD340 (a0 = the object; v0 = the entry address (the float-returning callees leave it) or 0x088AB798's result): the vector A on the frame (+0x18: x, -z of the object's word +0x3EC point, then the y of the entry) and the vector B (+0x28 / +0x2C and +0x24 from the entry
// object + 0x70 + index * 68 (index = word +0x3E8), floats +4 and +8) are set; 0x08B4AC30(B, B, A) combines them and 0x08B4ABEC(B, object + 0x3E0) must give a length above 0. Then the scale f22 = 1.0 is
// 0x08B4ABCC(B, object + 0x3E0) / 0x08B4A708(object + 0x3E0) when that second length is not 0. The two floats of A are copied to a vector (+8) combined with 0x08B4AC30(+8, +8, +0x20); the pair at object + 0x3E0 is copied to frame +0
// and scaled by 0x08B4A560(frame + 0) (f12 = the scale); +8 / +0xC are copied to +0x10 and 0x08B4AC0C(+0x10, +0x10, frame + 0) added; the result (+0x10, +0x14 copied to +0x30) is added to object + 8 with 0x08B4AC0C(object + 8, object + 8, +0x30)
// and 0x088AB798(object, object + 4, 1) finishes it. f20-f22 are the caller's afterwards.
void func_088AD340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x4Cu}, {18u, 0x48u}, {17u, 0x44u}, {16u, 0x40u}, {54u, 0x58u}, {53u, 0x54u}, {52u, 0x50u}});
    const u32 vector_a = frame.at(0x18u), vector_b = frame.at(0x28u);
    const u32 position = self + 0x3E0u;
    ctx.gpr[18] = vector_b;
    ctx.gpr[17] = self;
    ctx.gpr[16] = position;
    float scale = 1.0f;
    const float zero = 0.0f;
    ctx.fpr[22] = scale;
    ctx.fpr[21] = zero;
    const u32 point = word(rt, self + 0x3ECu);
    const u32 index = word(rt, self + 0x3E8u);
    const u32 entry = self + index * 68u + 0x70u;
    ctx.gpr[2] = entry;
    ctx.gpr[3] = point;
    put_float(rt, vector_a, float_at(rt, point));
    put_float(rt, vector_a + 0x4u, -float_at(rt, point + 0x8u));
    put_float(rt, vector_a + 0x8u, float_at(rt, entry + 0x4u));
    put_float(rt, vector_b, float_at(rt, entry + 0x4u));
    put_float(rt, vector_b + 0x4u, float_at(rt, entry + 0x8u));
    put_float(rt, frame.at(0x24u), float_at(rt, entry + 0x8u));
    call_set(rt, ctx, 0x08B4AC30u, {vector_b, vector_b, vector_a});
    call_set(rt, ctx, 0x08B4ABECu, {vector_b, position});
    if (zero < ctx.fpr[0]) {
        call_set(rt, ctx, 0x08B4A708u, {position});
        const float length = ctx.fpr[0];
        ctx.fpr[20] = length;
        if (!(length == zero)) {
            call_set(rt, ctx, 0x08B4ABCCu, {vector_b, position});
            scale = ctx.fpr[0] / length;
            ctx.fpr[22] = scale;
        }
        const u32 copy = frame.at(0x8u), direction = frame.at(0x0u), moved = frame.at(0x10u), target = frame.at(0x30u);
        put_float(rt, copy, float_at(rt, vector_a));
        put_float(rt, copy + 0x4u, float_at(rt, vector_a + 0x4u));
        call_set(rt, ctx, 0x08B4AC30u, {copy, copy, frame.at(0x20u)});
        ctx.fpr[12] = scale;
        put_float(rt, direction + 0x4u, float_at(rt, position + 0x4u));
        put_float(rt, direction, float_at(rt, position));
        call_set(rt, ctx, 0x08B4A560u, {direction});
        put_float(rt, moved, float_at(rt, copy));
        put_float(rt, moved + 0x4u, float_at(rt, copy + 0x4u));
        call_set(rt, ctx, 0x08B4AC0Cu, {moved, moved, direction});
        put_float(rt, target, float_at(rt, moved));
        put_float(rt, target + 0x4u, float_at(rt, moved + 0x4u));
        call_set(rt, ctx, 0x08B4AC0Cu, {self + 0x8u, self + 0x8u, target});
        call_set(rt, ctx, 0x088AB798u, {self, self + 0x4u, 1u});
    }
}
void func_0891B64C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xF0u); put(rt, frame.at(0xD0u), G[31]); put(rt, frame.at(0xCCu), G[19]); put(rt, frame.at(0xC8u), G[18]); put(rt, frame.at(0xC4u), G[17]); put(rt, frame.at(0xC0u), G[16]); auto &F = ctx.fpr; F[13] = std::bit_cast<float>(0u); G[7] = 0x6u; G[19] = G[6]; G[18] = frame.at(0x80u); G[17] = G[4]; G[16] = frame.at(0x40u); put_float(rt, frame.at(0xE0u), F[20]); G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); G[2] = word(rt, G[5] + 0x8u); F[12] = float_at(rt, G[6] + 0x4u); G[5] = frame.at(0u); G[4] = word(rt, G[2] + 0x61Cu); G[6] = frame.at(0xCu); call_here(rt, ctx, 0x088D390Cu); G[4] = G[16]; call_here(rt, ctx, 0x08B4E648u); F[0] = float_at(rt, frame.at(0x0u)); F[1] = float_at(rt, frame.at(0x4u)); G[5] = G[18]; put_float(rt, frame.at(0xB0u), F[0]); G[4] = G[16]; F[0] = float_at(rt, frame.at(0x8u)); put_float(rt, frame.at(0xB4u), F[1]); put_float(rt, frame.at(0xB8u), F[0]); put_float(rt, frame.at(0x80u), F[20]); put_float(rt, frame.at(0x94u), F[20]); put_float(rt, frame.at(0xA8u), F[20]); put_float(rt, frame.at(0xBCu), F[20]); put(rt, frame.at(0x90u), 0u); put(rt, frame.at(0xA0u), 0u); put(rt, frame.at(0x84u), 0u); put(rt, frame.at(0xA4u), 0u); put(rt, frame.at(0x88u), 0u); put(rt, frame.at(0x98u), 0u); put(rt, frame.at(0x8Cu), 0u); put(rt, frame.at(0x9Cu), 0u); put(rt, frame.at(0xACu), 0u); call_here(rt, ctx, 0x08B4E9D4u); G[4] = G[16]; F[12] = float_at(rt, frame.at(0x10u)); call_here(rt, ctx, 0x08B4F3B0u); F[12] = float_at(rt, frame.at(0xCu)); G[4] = G[16]; call_here(rt, ctx, 0x08B4F2E4u); F[12] = float_at(rt, frame.at(0x14u)); G[4] = G[16]; call_here(rt, ctx, 0x08B4F47Cu); F[0] = float_at(rt, G[19] + 0x0u); G[5] = G[18]; G[4] = G[16]; put_float(rt, frame.at(0xB0u), F[0]); put_float(rt, frame.at(0x80u), F[20]); put_float(rt, frame.at(0x94u), F[20]); put_float(rt, frame.at(0xA8u), F[20]); put_float(rt, frame.at(0xBCu), F[20]); put(rt, frame.at(0x90u), 0u); put(rt, frame.at(0xA0u), 0u); put(rt, frame.at(0x84u), 0u); put(rt, frame.at(0xA4u), 0u); put(rt, frame.at(0xB4u), 0u); put(rt, frame.at(0x88u), 0u); put(rt, frame.at(0x98u), 0u); put(rt, frame.at(0xB8u), 0u); put(rt, frame.at(0x8Cu), 0u); put(rt, frame.at(0x9Cu), 0u); put(rt, frame.at(0xACu), 0u); call_here(rt, ctx, 0x08B4E9D4u); G[5] = G[16]; G[4] = frame.at(0x28u); G[6] = frame.at(0x18u); put_float(rt, frame.at(0x24u), F[20]); put(rt, frame.at(0x18u), 0u); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x20u), 0u); call_here(rt, ctx, 0x08B4A774u); G[2] = G[17]; F[2] = float_at(rt, frame.at(0x30u)); F[1] = float_at(rt, frame.at(0x28u)); F[0] = float_at(rt, frame.at(0x2Cu)); put_float(rt, G[17] + 0x8u, F[2]); put_float(rt, G[17] + 0x4u, F[0]); put_float(rt, G[17] + 0x0u, F[1]); F[20] = float_at(rt, frame.at(0xE0u));  G[19] = word(rt, frame.at(0xCCu)); G[18] = word(rt, frame.at(0xC8u)); G[17] = word(rt, frame.at(0xC4u)); G[16] = word(rt, frame.at(0xC0u)); }  } }
// 0x08982EA8 (a0 = the object; v0 = object + 0x21C, v1 = object + 0x228): constructor. Base 0x0896BDF8(object); vtable words +0 = 0x08CCCAB8, +0x38 = 0x08CCCCF4; the member at +0xF0 is built with 0x08984824. A circular list head at +0x1E4 (pointing to itself,
// +0x1E0 = the empty-string data pointer 0x08D74FD0 + 0xC) gets a first node: 0x08A5BC20(0xC, 4) allocates it, its +8 part is built with 0x08B88F44(node + 8, frame + 4) when not null, and 0x08B5C078(node, head) links it. The temporary
// string on the frame (+4) is released (unless it is the empty header its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp) frees it). Then the flag +0x1EC = 1, +0x1F0 = 0, two members 0x08A76AA0(+0x1F4 / +0x1F8),
// the float 1.0 at +0x200, 10.0 at +0x238, and the other fields (bytes at +0x1FC, +0x204..+0x206, words +0x208..+0x21C, two 8-byte pairs at +0x21C / +0x228 (+4, +8 words), +0x23C, +0x234) and the member's bytes (+0xF0 + 0xC4 = 1, +0x38, +0x36, +0x37 = 0) are set.
void func_08982EA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[21] = 1u;
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x0896BDF8u, {self});
    put(rt, self, 0x08CCCAB8u);
    const u32 member = self + 0xF0u;
    ctx.gpr[19] = member;
    put(rt, self + 0x38u, 0x08CCCCF4u);
    call_set(rt, ctx, 0x08984824u, {member});
    const u32 empty = 0x08D74FD0u;
    ctx.gpr[20] = empty;
    const u32 text_data = empty + 0xCu;
    const u32 head = self + 0x1E4u;
    ctx.gpr[17] = head;
    put(rt, self + 0x1E0u, text_data);
    put(rt, self + 0x1E4u, head);
    put(rt, head + 0x4u, head);
    put(rt, frame.at(0x4u), text_data);
    const u32 node = call_set(rt, ctx, 0x08A5BC20u, {0xCu, 4u});
    ctx.gpr[18] = node;
    if (node + 0x8u != 0u) call_set(rt, ctx, 0x08B88F44u, {node + 0x8u, frame.at(0x4u)});
    call_set(rt, ctx, 0x08B5C078u, {node, head});
    const u32 data = word(rt, frame.at(0x4u));
    const u32 header = data - 0xCu;
    ctx.gpr[17] = header;
    if (header != empty) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    put(rt, self + 0x1ECu, 1u);
    put(rt, self + 0x1F0u, 0u);
    call_set(rt, ctx, 0x08A76AA0u, {self + 0x1F4u});
    call_set(rt, ctx, 0x08A76AA0u, {self + 0x1F8u});
    put_float(rt, self + 0x200u, 1.0f);
    const u32 pair_1 = self + 0x21Cu, pair_2 = self + 0x228u;
    ctx.gpr[2] = pair_1;
    ctx.gpr[3] = pair_2;
    ctx.fpr[0] = 10.0f;
    for (const u32 offset : {0x1FCu, 0x204u, 0x205u, 0x206u}) store8(self + offset, 0u);
    for (const u32 offset : {0x208u, 0x20Cu, 0x210u, 0x214u, 0x218u, 0x21Cu}) put(rt, self + offset, 0u);
    put(rt, pair_1 + 0x8u, 0u);
    put(rt, pair_1 + 0x4u, 0u);
    put(rt, pair_2 + 0x8u, 0u);
    put(rt, pair_2 + 0x4u, 0u);
    put(rt, self + 0x228u, 0u);
    put_float(rt, self + 0x238u, 10.0f);
    put(rt, self + 0x23Cu, 0u);
    put(rt, self + 0x234u, 0u);
    store8(member + 0xC4u, 1u);
    store8(member + 0x38u, 0u);
    store8(member + 0x36u, 0u);
    store8(member + 0x37u, 0u);
}
void func_088CB668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xD0u); put(rt, frame.at(0xACu), G[19]); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); put(rt, frame.at(0xA0u), G[16]); put(rt, frame.at(0xB8u), G[31]); put(rt, frame.at(0xB4u), G[21]); put(rt, frame.at(0xB0u), G[20]); auto &F = ctx.fpr; G[3] = G[5]; G[19] = frame.at(0x10u); G[18] = G[7]; G[17] = G[6]; G[16] = G[4]; put_float(rt, frame.at(0xC0u), F[20]); F[20] = F[12]; put_float(rt, frame.at(0xCCu), F[23]); put_float(rt, frame.at(0xC8u), F[22]); put_float(rt, frame.at(0xC4u), F[21]); G[5] = word(rt, G[5] + 0x0u); G[20] = word(rt, G[16] + 0x5F0u); G[21] = word(rt, G[16] + 0x5F4u); G[2] = word(rt, G[5] + 0x1Cu); G[5] = word(rt, G[4] + 0x2Cu); { const std::uint32_t tg = G[2]; G[4] = G[3]; call_here(rt, ctx, tg); } G[4] = G[2]; { const bool tk = G[2] == 0u; G[5] = frame.at(0u); if (!tk) { G[2] = word(rt, G[2] + 0x0u); G[3] = word(rt, G[2] + 0x10u); { const std::uint32_t tg = G[3]; call_here(rt, ctx, tg); } G[6] = word(rt, G[16] + 0x30u); G[7] = word(rt, G[16] + 0x63Cu); G[5] = 0x2u; G[2] = G[6] ^ 0x4u; if (G[2] != 0u) G[5] = 0u; G[3] = 0x3u; put(rt, frame.at(0x4Cu), G[5]); G[4] = G[7]; put(rt, frame.at(0x50u), G[6]); put_float(rt, frame.at(0x58u), F[20]); put(rt, frame.at(0x48u), G[3]); put(rt, frame.at(0x54u), 0u); put(rt, frame.at(0x5Cu), 0u); G[3] = word(rt, G[7] + 0x0u); G[2] = word(rt, G[3] + 0x1Cu); { const std::uint32_t tg = G[2]; F[21] = float_at(rt, frame.at(0x4u)); call_here(rt, ctx, tg); } G[3] = word(rt, G[2] + 0x0u); G[5] = word(rt, G[16] + 0x63Cu); G[4] = G[2]; G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[6] = frame.at(0x48u); call_here(rt, ctx, tg); } F[20] = float_at(rt, frame.at(0x74u)); G[4] = G[19]; F[22] = float_at(rt, frame.at(0x70u)); F[23] = float_at(rt, frame.at(0x78u)); call_here(rt, ctx, 0x089545BCu); G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); F[20] = -F[20]; F[21] = F[21] + F[0]; G[2] = 0x1u; store8(frame.at(0x3Cu), G[2]); G[4] = G[17]; G[5] = G[19]; put_float(rt, G[18] + 0x0u, F[22]); put_float(rt, G[18] + 0x8u, F[20]); put_float(rt, frame.at(0x14u), F[21]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x44u), G[21]); put_float(rt, frame.at(0x10u), F[22]); { const bool tk = G[17] == 0u; put_float(rt, frame.at(0x18u), F[20]); if (!tk) { G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x18u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } } } G[2] = load8(frame.at(0x3Cu)); { const bool tk = G[2] != 0u; if (tk) { put_float(rt, G[18] + 0x4u, F[23]); } else { F[0] = float_at(rt, frame.at(0x1Cu)); put_float(rt, G[18] + 0x4u, F[0]); } } } } F[23] = float_at(rt, frame.at(0xCCu)); F[22] = float_at(rt, frame.at(0xC8u)); F[21] = float_at(rt, frame.at(0xC4u)); F[20] = float_at(rt, frame.at(0xC0u));  G[19] = word(rt, frame.at(0xACu)); G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); G[21] = word(rt, frame.at(0xB4u)); G[20] = word(rt, frame.at(0xB0u)); }  } }
void func_088A3BD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); auto &F = ctx.fpr; G[7] = frame.at(0x8u); G[18] = G[6]; G[8] = frame.at(0xCu); G[17] = G[5]; G[5] = frame.at(0u); G[16] = G[4]; G[6] = frame.at(0x4u); G[3] = 0u; G[2] = word(rt, G[4] + 0x18u); { const bool tk = G[2] == 0u; G[4] = G[2]; if (!tk) { call_here(rt, ctx, 0x0893C320u); { const bool tk = G[17] == 0u; if (!tk) { G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08949E8Cu); F[0] = std::bit_cast<float>(G[2]); F[1] = float_at(rt, frame.at(0xCu)); F[3] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[0] = float_at(rt, frame.at(0x8u)); F[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); put_float(rt, G[17] + 0x0u, F[3]); put_float(rt, G[17] + 0x4u, F[2]); put_float(rt, G[17] + 0x8u, F[0]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955334u); F[1] = float_at(rt, frame.at(0x4u)); put_float(rt, G[17] + 0xCu, F[0]); F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); put_float(rt, G[17] + 0x10u, F[0]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x0895596Cu); put_float(rt, G[17] + 0x14u, F[0]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955960u); put_float(rt, G[17] + 0x18u, F[0]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955978u); put_float(rt, G[17] + 0x1Cu, F[0]); G[5] = 0u; G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x089558C8u); put_float(rt, G[17] + 0x20u, F[0]); F[12] = std::bit_cast<float>(0u); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x089559D0u); put_float(rt, G[17] + 0x24u, F[0]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x089564FCu); put_float(rt, G[17] + 0x28u, F[0]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955984u); put_float(rt, G[17] + 0x2Cu, F[0]); } } { const bool tk = G[18] == 0u; G[3] = 0x1u; if (!tk) { G[2] = word(rt, frame.at(0x0u)); put(rt, G[18] + 0x28u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x0895534Cu); put(rt, G[18] + 0x0u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955540u); put(rt, G[18] + 0x4u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x089554C8u); put(rt, G[18] + 0x8u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955760u); put(rt, G[18] + 0xCu, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x0895541Cu); put(rt, G[18] + 0x10u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x0895589Cu); put(rt, G[18] + 0x14u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x0895587Cu); put(rt, G[18] + 0x18u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955888u); put(rt, G[18] + 0x1Cu, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955894u); put(rt, G[18] + 0x20u, G[2]); G[4] = word(rt, G[16] + 0x18u); call_here(rt, ctx, 0x08955208u); put(rt, G[18] + 0x24u, G[2]); G[3] = 0x1u; } } } } G[2] = G[3];  G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0897837C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[20] = 0u; G[19] = G[5]; G[18] = G[7]; G[17] = G[6]; put_float(rt, frame.at(0x38u), F[22]); put_float(rt, frame.at(0x34u), F[21]); { const bool tk = static_cast<std::int32_t>(G[6]) <= 0; put_float(rt, frame.at(0x30u), F[20]); if (!tk) { G[16] = word(rt, G[7] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA2F84u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[20] = G[2]; } } G[2] = static_cast<std::int32_t>(G[17]) < 2 ? 1u : 0u; G[1] = 0x3F800000u; F[22] = std::bit_cast<float>(G[1]); { const bool tk = G[2] != 0u; G[2] = static_cast<std::int32_t>(G[17]) < 3 ? 1u : 0u; if (!tk) { G[16] = word(rt, G[18] + 0x4u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA2F84u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } F[22] = F[0]; G[2] = static_cast<std::int32_t>(G[17]) < 3 ? 1u : 0u; } } G[1] = 0x3F800000u; F[21] = std::bit_cast<float>(G[1]); { const bool tk = G[2] != 0u; G[2] = static_cast<std::int32_t>(G[17]) < 4 ? 1u : 0u; if (!tk) { G[16] = word(rt, G[18] + 0x8u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA2F84u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } F[21] = F[0]; G[2] = static_cast<std::int32_t>(G[17]) < 4 ? 1u : 0u; } } G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); { const bool tk = G[2] != 0u; G[5] = G[19]; if (!tk) { G[16] = word(rt, G[18] + 0xCu); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA2F84u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } F[20] = F[0]; G[5] = G[19]; } } G[4] = frame.at(0u); call_here(rt, ctx, 0x08978000u); G[16] = word(rt, frame.at(0x0u)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08CA2F94u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } F[12] = F[22]; F[13] = F[21]; F[14] = F[20]; G[2] = word(rt, G[3] + 0x138u); G[4] = G[16]; { const std::uint32_t tg = G[2]; G[5] = G[20]; call_here(rt, ctx, tg); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); F[22] = float_at(rt, frame.at(0x38u)); F[21] = float_at(rt, frame.at(0x34u)); F[20] = float_at(rt, frame.at(0x30u));  G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_0880CB68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); auto &F = ctx.fpr; G[2] = 0u; G[19] = G[4]; G[17] = G[7]; { const bool tk = static_cast<std::int32_t>(G[6]) <= 0; put_float(rt, frame.at(0x40u), F[20]); if (!tk) { G[21] = frame.at(0x8u); G[4] = G[21]; call_here(rt, ctx, 0x0880B8F8u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C98418u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[20] = frame.at(0xCu); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[20]; call_here(rt, ctx, tg); } G[16] = word(rt, G[17] + 0x4u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C98418u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[18] = frame.at(0x10u); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[18]; call_here(rt, ctx, tg); } G[16] = word(rt, G[17] + 0x8u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C98418u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; F[20] = F[0]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C9848Cu; call_here(rt, ctx, 0x08A5BB7Cu); } } F[12] = F[20]; G[4] = word(rt, G[16] + 0xE8u); G[5] = G[20]; G[6] = G[18]; call_here(rt, ctx, 0x0880BAD8u); G[4] = word(rt, frame.at(0x10u)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[17]; if (tk) { G[4] = word(rt, frame.at(0xCu)); } else { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = word(rt, frame.at(0xCu)); if (!tk) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, frame.at(0xCu)); } } } } G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[17] == G[16]; G[4] = G[4] + 0xFFFFFFFCu; if (!tk) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (!tk) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } } G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x1u; } } G[16] = frame.at(0x4u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); { const bool tk = G[19] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); F[20] = float_at(rt, frame.at(0x40u));  G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); }  } }
// 0x08882E34 (a0 = the output matrix, a2 = a plane (4 floats); v0 = the last call's): builds the 4 x 4 matrix on the frame (+0x50) from the plane (a, b, c, d): its row 0 is (1, -a / b, 0, 0), row 2 (0, -c / b, 1, 0), row 3 (0, -d / b, 0, 1),
// all the others zero; 0x08B4E9D4(matrix) works on it. Two vectors are taken from it, (+0x80, +0x84, +0x88, +0x8C) is copied to frame +0 .. +0xC and the plane's (a, b, c) to +0x10 .. +0x18 (+0x1C = 0) and (+0x70, +0x74, +0x78, +0x7C) to +0x20 .. +0x2C; each of
// the vectors at +0x10 and +0x20 is normalised (0x08B4A744 gives its squared length, f12 = 1.0 / sqrt of it, 0x08B4A6A0(vector)). 0x08B4A82C(frame + 0x40, frame + 0x10, frame + 0x20) combines them (a cross product) and the output gets
// its rows: +0..+8 and +0x3C from +0x40..+0x4C, +0x10..+0x2C the two vectors, +0x30..+0x38 the copy at frame +0..+8, +0xC = 0.
void func_08882E34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 unused = ctx.gpr[5];
    const u32 plane = ctx.gpr[6];
    GuestFrame frame(ctx, 0xC0u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0xA0u}, {19u, 0x9Cu}, {18u, 0x98u}, {17u, 0x94u}, {16u, 0x90u}, {52u, 0xB0u}});
    const u32 vector_1 = frame.at(0x10u), vector_2 = frame.at(0x20u), matrix = frame.at(0x50u);
    ctx.gpr[19] = vector_2;
    ctx.gpr[18] = vector_1;
    ctx.gpr[17] = plane;
    ctx.gpr[16] = out;
    const float one = 1.0f;
    ctx.fpr[20] = one;
    const float a = -float_at(rt, plane), c = -float_at(rt, plane + 0x8u), d = -float_at(rt, plane + 0xCu);
    const float b = float_at(rt, plane + 0x4u);
    put_float(rt, matrix, one);
    const float d_over_b = d / b, a_over_b = a / b, c_over_b = c / b;
    put_float(rt, frame.at(0x78u), one);
    put_float(rt, frame.at(0x8Cu), one);
    for (const u32 offset : {0x60u, 0x70u, 0x80u, 0x64u, 0x58u, 0x68u, 0x88u}) put(rt, frame.at(offset), 0u);
    put_float(rt, frame.at(0x84u), d_over_b);
    put_float(rt, frame.at(0x54u), a_over_b);
    put_float(rt, frame.at(0x74u), c_over_b);
    for (const u32 offset : {0x5Cu, 0x6Cu, 0x7Cu}) put(rt, frame.at(offset), 0u);
    call_set(rt, ctx, 0x08B4E9D4u, {matrix, unused});
    put_float(rt, frame.at(0x0u), float_at(rt, frame.at(0x80u)));
    put_float(rt, frame.at(0x4u), float_at(rt, frame.at(0x84u)));
    put_float(rt, vector_1, float_at(rt, plane));
    put_float(rt, vector_1 + 0x4u, float_at(rt, plane + 0x4u));
    put_float(rt, vector_1 + 0x8u, float_at(rt, plane + 0x8u));
    put_float(rt, frame.at(0xCu), float_at(rt, frame.at(0x8Cu)));
    put_float(rt, frame.at(0x8u), float_at(rt, frame.at(0x88u)));
    put(rt, frame.at(0x1Cu), 0u);
    call_set(rt, ctx, 0x08B4A744u, {vector_1});
    ctx.fpr[0] = std::sqrt(ctx.fpr[0]);
    ctx.fpr[12] = one / ctx.fpr[0];
    call_set(rt, ctx, 0x08B4A6A0u, {vector_1});
    put_float(rt, vector_2, float_at(rt, frame.at(0x70u)));
    put_float(rt, vector_2 + 0x4u, float_at(rt, frame.at(0x74u)));
    put_float(rt, vector_2 + 0x8u, float_at(rt, frame.at(0x78u)));
    put_float(rt, vector_2 + 0xCu, float_at(rt, frame.at(0x7Cu)));
    call_set(rt, ctx, 0x08B4A744u, {vector_2});
    ctx.fpr[0] = std::sqrt(ctx.fpr[0]);
    ctx.fpr[12] = one / ctx.fpr[0];
    call_set(rt, ctx, 0x08B4A6A0u, {vector_2});
    call_set(rt, ctx, 0x08B4A82Cu, {frame.at(0x40u), vector_1, vector_2});
    put_float(rt, out + 0x4u, float_at(rt, frame.at(0x44u)));
    put_float(rt, out + 0x0u, float_at(rt, frame.at(0x40u)));
    put_float(rt, out + 0x3Cu, float_at(rt, frame.at(0xCu)));
    put_float(rt, out + 0x8u, float_at(rt, frame.at(0x48u)));
    for (u32 offset = 0x10u; offset <= 0x2Cu; offset += 4u) put_float(rt, out + offset, float_at(rt, frame.at(offset)));
    put_float(rt, out + 0x30u, float_at(rt, frame.at(0x0u)));
    put_float(rt, out + 0x34u, float_at(rt, frame.at(0x4u)));
    put_float(rt, out + 0x38u, float_at(rt, frame.at(0x8u)));
    put(rt, out + 0xCu, 0u);
}
// 0x089D5E94 (a0 = the context for 0x089D5DF4, a2 = an opcode that must be 6, a3 = the values; v0 = 6 when the opcode differs, else the
// last release's). The context at sp is built (0x089D5DF4(sp)), the temporaries sp+4 (0x089AB370, from a3) and sp+8 (0x089A354C, from
// a3+4) are filled, the word at sp gets the sp+4 word at +0x1C and 0x089D5B7C(word at sp, word at sp+8) runs. The four values at a3+8,
// a3+0xC, a3+0x10 and a3+0x14 are then called through their own vtable (entry +0x3C for the first two, +0x54 for the last two, each with the
// value as a0) and what they return is stored on the word at sp: +0x28 and +0x2C as words, +0x34 and +0x38 as floats. Every object and
// temporary is null-checked first (0x08CA6E2C for the word at sp, 0x08CA6DE0 for the others), and the two temporaries and the context are
// released with 0x08A5B9EC.
void func_089D5E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 values = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}});
    const u32 context = frame.at(0u), first = frame.at(0x4u), second = frame.at(0x8u);
    ctx.gpr[2] = 6u;
    ctx.gpr[17] = values;
    if (ctx.gpr[6] != ctx.gpr[2]) return;
    ctx.gpr[20] = first;
    call_set(rt, ctx, 0x089D5DF4u, {context});
    ctx.gpr[19] = second;
    call_set(rt, ctx, 0x089AB370u, {first, values});
    call_set(rt, ctx, 0x089A354Cu, {second, values + 0x4u});
    u32 owner = word(rt, frame.at(0x0u));
    if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
    put(rt, owner + 0x1Cu, word(rt, frame.at(0x4u)));
    owner = word(rt, frame.at(0x0u));
    if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
    call_set(rt, ctx, 0x089D5B7Cu, {owner, word(rt, frame.at(0x8u))});
    ctx.gpr[18] = word(rt, frame.at(0x0u));
    if (ctx.gpr[18] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
    // a3+0x8: the value's vtable entry +0x3C, its word result kept at owner+0x28.
    ctx.gpr[16] = word(rt, values + 0x8u);
    if (ctx.gpr[16] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6DE0u});
    ctx.gpr[3] = word(rt, ctx.gpr[16] + 0x0u);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    ctx.gpr[4] = ctx.gpr[16];
    call_here(rt, ctx, ctx.gpr[2]);
    put(rt, ctx.gpr[18] + 0x28u, ctx.gpr[2]);
    // a3+0xC: vtable entry +0x3C, its word result kept at owner+0x2C.
    ctx.gpr[18] = word(rt, frame.at(0x0u));
    if (ctx.gpr[18] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
    ctx.gpr[16] = word(rt, values + 0xCu);
    if (ctx.gpr[16] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6DE0u});
    ctx.gpr[3] = word(rt, ctx.gpr[16] + 0x0u);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    ctx.gpr[4] = ctx.gpr[16];
    call_here(rt, ctx, ctx.gpr[2]);
    put(rt, ctx.gpr[18] + 0x2Cu, ctx.gpr[2]);
    // a3+0x10: vtable entry +0x54, its float result kept at owner+0x34.
    ctx.gpr[18] = word(rt, frame.at(0x0u));
    if (ctx.gpr[18] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
    ctx.gpr[16] = word(rt, values + 0x10u);
    if (ctx.gpr[16] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6DE0u});
    ctx.gpr[3] = word(rt, ctx.gpr[16] + 0x0u);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x54u);
    ctx.gpr[4] = ctx.gpr[16];
    call_here(rt, ctx, ctx.gpr[2]);
    put_float(rt, ctx.gpr[18] + 0x34u, ctx.fpr[0]);
    // a3+0x14: vtable entry +0x54, its float result kept at owner+0x38.
    ctx.gpr[18] = word(rt, frame.at(0x0u));
    if (ctx.gpr[18] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6E2Cu});
    ctx.gpr[16] = word(rt, values + 0x14u);
    if (ctx.gpr[16] == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6DE0u});
    ctx.gpr[3] = word(rt, ctx.gpr[16] + 0x0u);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x54u);
    ctx.gpr[4] = ctx.gpr[16];
    call_here(rt, ctx, ctx.gpr[2]);
    put_float(rt, ctx.gpr[18] + 0x38u, ctx.fpr[0]);
    call_set(rt, ctx, 0x08A5B9ECu, {second});
    call_set(rt, ctx, 0x08A5B9ECu, {first});
    call_set(rt, ctx, 0x08A5B9ECu, {context});
}
void func_08A12C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[16] = G[4]; G[17] = G[16] + 0x240u; put_float(rt, frame.at(0x20u), F[20]); call_here(rt, ctx, 0x089C1614u); G[2] = 0x08CD0000u; G[2] = 0x08CD0418u; put(rt, G[16] + 0x0u, G[2]); G[4] = G[16] + 0x10u; G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); put(rt, G[16] + 0xCu, 0u); call_here(rt, ctx, 0x08B28D34u); G[2] = 0x08A10000u; G[2] = 0x08A13BE0u; G[5] = 0u; put(rt, frame.at(0x8u), G[2]); G[4] = 0x3u; G[2] = 0x00010000u; put(rt, frame.at(0xCu), G[16]); put(rt, frame.at(0x4u), G[2]); put(rt, frame.at(0x0u), 0u); call_here(rt, ctx, 0x08B2E314u); G[4] = G[2]; call_here(rt, ctx, 0x08B2E134u); G[6] = G[2]; G[5] = frame.at(0u); G[7] = 0u; G[4] = frame.at(0x8u); call_here(rt, ctx, 0x08B1B92Cu); put(rt, G[16] + 0x3Cu, G[2]); G[4] = G[16] + 0x5Cu; store8(G[16] + 0x40u, 0u); store8(G[16] + 0x41u, 0u); put(rt, G[16] + 0x44u, 0u); put(rt, G[16] + 0x48u, 0u); put(rt, G[16] + 0x4Cu, 0u); put(rt, G[16] + 0x50u, 0u); put(rt, G[16] + 0x54u, 0u); put(rt, G[16] + 0x58u, 0u); call_here(rt, ctx, 0x08C15D40u); G[4] = G[16] + 0xA0u; put(rt, G[16] + 0x78u, 0u); put(rt, G[16] + 0x7Cu, 0u); put(rt, G[16] + 0x80u, 0u); put(rt, G[16] + 0x84u, 0u); put(rt, G[16] + 0x88u, 0u); put(rt, G[16] + 0x8Cu, 0u); put(rt, G[16] + 0x90u, 0u); put(rt, G[16] + 0x94u, 0u); put(rt, G[16] + 0x98u, 0u); put(rt, G[16] + 0x9Cu, 0u); call_here(rt, ctx, 0x08B2410Cu); G[4] = G[16] + 0x140u; call_here(rt, ctx, 0x08A19CC0u); G[4] = G[16] + 0x178u; call_here(rt, ctx, 0x08A19CC0u); G[2] = 0x4u; G[3] = 0u; put(rt, G[16] + 0x1B8u, G[2]); G[2] = 0u; G[4] = G[16] + 0x200u; put(rt, G[16] + 0x1C0u, G[2]); put(rt, G[16] + 0x1C4u, G[3]); put_float(rt, G[16] + 0x1B0u, F[20]); put_float(rt, G[16] + 0x1B4u, F[20]); put(rt, G[16] + 0x1BCu, 0u); put(rt, G[16] + 0x1C8u, 0u); store8(G[16] + 0x1F4u, 0u); store8(G[16] + 0x1F5u, 0u); store8(G[16] + 0x1F6u, 0u); store8(G[16] + 0x1F7u, 0u); store8(G[16] + 0x1F8u, 0u); store8(G[16] + 0x1F9u, 0u); store8(G[16] + 0x1FAu, 0u); store8(G[16] + 0x1FBu, 0u); put(rt, G[16] + 0x1FCu, 0u); call_here(rt, ctx, 0x08B28D34u); G[4] = G[17]; call_here(rt, ctx, 0x08ACECFCu); G[4] = G[16] + 0x78u; store8(G[16] + 0x780u, 0u); G[5] = 0x1u; G[6] = 0u; store8(G[16] + 0x781u, 0u); G[7] = 0u; store8(G[16] + 0x782u, 0u); put(rt, G[16] + 0x784u, 0u); call_here(rt, ctx, 0x08ACCF00u); G[7] = 0u; G[4] = G[16] + 0x8Cu; G[5] = 0x1u; G[6] = 0u; call_here(rt, ctx, 0x08ACCF00u); G[4] = G[16] + 0x1CCu; G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x08A128E0u); G[6] = 0u; G[4] = G[16] + 0x1E0u; G[5] = 0u; call_here(rt, ctx, 0x08A128E0u); F[12] = F[20]; G[4] = G[17]; call_here(rt, ctx, 0x08ACCC38u); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x08ACCBA0u); G[2] = 0x08D00000u; G[4] = word(rt, 0x08CFFAACu); G[5] = 0x1u; call_here(rt, ctx, 0x08ACF184u); F[20] = float_at(rt, frame.at(0x20u));  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_318
} // namespace gt::game

#define HOT318(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_318::name, gt::ReturnKind::kind)
HOT318(0x08877090u, func_08877090, Word);
// HOT318(0x08B062BCu, func_08B062BC, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT318(0x08AA027Cu, func_08AA027C, Word);
// HOT318(0x0889B0B0u, func_0889B0B0, Word);  // a synthetic case mismatched: to fix
HOT318(0x089DE7D4u, func_089DE7D4, Word);
HOT318(0x0896ABF4u, func_0896ABF4, Word);
HOT318(0x088785C8u, func_088785C8, Word);
HOT318(0x08996DF4u, func_08996DF4, Word);
HOT318(0x089780E0u, func_089780E0, Word);
// HOT318(0x0899FD68u, func_0899FD68, Word);  // no comparable synthetic case yet
// HOT318(0x08866B40u, func_08866B40, Word);  // no comparable synthetic case yet
HOT318(0x08BA388Cu, func_08BA388C, Word);
HOT318(0x08871908u, func_08871908, Word);
// HOT318(0x08869514u, func_08869514, Word);  // no comparable synthetic case yet
// HOT318(0x088ACBA0u, func_088ACBA0, Word);  // no comparable synthetic case yet
HOT318(0x088AD340u, func_088AD340, Word);
// HOT318(0x0891B64Cu, func_0891B64C, Word);  // no comparable synthetic case yet
HOT318(0x08982EA8u, func_08982EA8, Word);
// HOT318(0x088CB668u, func_088CB668, Word);  // no comparable synthetic case yet
HOT318(0x088A3BD0u, func_088A3BD0, Word);
// HOT318(0x0897837Cu, func_0897837C, Word);  // no comparable synthetic case yet
HOT318(0x0880CB68u, func_0880CB68, Word);
HOT318(0x08882E34u, func_08882E34, Word);
HOT318(0x089D5E94u, func_089D5E94, Word);
// HOT318(0x08A12C54u, func_08A12C54, Word);  // no comparable synthetic case yet
