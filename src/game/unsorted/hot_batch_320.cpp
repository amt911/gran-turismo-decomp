// Hot batch 320: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
// Two of them (0x08B31000, 0x08B40DD8) are rewritten as structured C from the disassembly (structured rewrites, batch 89), the ones the synthetic gate compares;
// the other emitter bodies were never compared by it (registrations commented out or not comparable) and stay as they are.
// 0x08C72194 are rewritten as structured C from the disassembly (structured rewrites, batch 90) and registered: the synthetic gate compares them in the garage / challenge / dirt / manual / challenge2 scenes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_320 {
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

// ll / sc decrement of the word at `address` (nothing else runs here, so the store always succeeds and v0 is 1 afterwards): true when it was 1.
bool decrement_reaches_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    return old == 1u;
}

// Drops one use of a shared holder (use count +4, weak count +8): the last use calls the virtual +8, the last weak count the virtual +0xC.
void release_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 holder) {
    if (!decrement_reaches_zero(rt, ctx, holder + 4u)) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
    if (!decrement_reaches_zero(rt, ctx, holder + 8u)) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
}

// A guest call with integer arguments (a0-a3, t0-t3) and float ones ($f12, $f13, ...), and a float result in $f0.
float call_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    reg = 12u;
    for (const float value : floats) ctx.fpr[reg++] = value;
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}



// A `lw v0, slot(vtable); jalr v0` call with the argument registers set as the original sets them: the callee starts with v0 = its own address.
u32 call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_indirect(rt, ctx, target, arguments);
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08978D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[17] = G[4] + 0x20u; G[18] = G[4]; G[4] = G[17]; G[16] = G[5]; put(rt, frame.at(0x0u), frame.at(0u)); put(rt, frame.at(0x4u), frame.at(0u)); call_here(rt, ctx, 0x08B344E0u); G[5] = G[16]; G[6] = frame.at(0u); G[4] = G[18] + 0x18u; call_here(rt, ctx, 0x08BEDF34u); G[4] = G[17]; call_here(rt, ctx, 0x08B3457Cu); G[17] = word(rt, frame.at(0x0u)); { const bool tk = frame.at(0u) == G[17]; G[20] = G[18] + 0x74u; if (tk) goto L_08978E10; } G[19] = G[18] + 0x7Cu; G[6] = word(rt, G[18] + 0x7Cu); L_08978DA0: ; G[2] = word(rt, G[18] + 0x74u); G[4] = word(rt, G[20] + 0x4u); { const bool tk = G[2] != G[6]; G[3] = word(rt, G[19] + 0x4u); if (tk) goto L_08978E38; } { const bool tk = G[4] != G[3]; G[2] = G[3] + 0xFFFFFFFFu; if (tk) goto L_08978E3C; } G[17] = word(rt, G[17] + 0x0u); goto L_08978E08; L_08978DC0: ; G[3] = G[3] + 0xFFFFFFFCu; L_08978DC4: ; G[3] = word(rt, G[3] + 0x0u); G[2] = 0x1u; G[2] = G[2] << (G[4] & 31u); G[2] = G[2] & G[3]; if (G[2] == 0u) { G[17] = word(rt, G[17] + 0x0u); goto L_08978E08; } G[16] = word(rt, G[17] + 0x8u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08978DF8; } G[4] = 0x08CA0000u; G[4] = 0x08CA2F7Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08978DF8: ; G[2] = word(rt, G[3] + 0x108u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[17] = word(rt, G[17] + 0x0u); L_08978E08: ; if (frame.at(0u) != G[17]) { G[6] = word(rt, G[18] + 0x7Cu); goto L_08978DA0; } L_08978E10: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08BEDD64u); goto exit_; L_08978E38: ; G[2] = G[3] + 0xFFFFFFFFu; L_08978E3C: ; G[5] = sra(G[2], 31); G[4] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; G[5] = G[5] >> 27; G[3] = G[3] + 0x1Eu; if (G[4] == 0u) G[3] = G[2]; G[2] = G[2] + G[5]; G[3] = sra(G[3], 5); G[2] = G[2] & 0x1Fu; G[2] = G[2] - G[5]; G[3] = G[3] << 2; G[3] = G[6] + G[3]; { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[4] = G[2]; if (tk) goto L_08978DC4; } G[4] = G[2] + 0x20u; goto L_08978DC0;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089D15E0 (a0 = the destination object, a1 unused, a2 = mode, a3 = a pointer to a handle): mode 0 makes a handle at frame + 4 with 0x089CD62C (the string 0x08CA69B8
// reports a missing object through 0x08A5BB7C), converts the float at object + 0x138 with 0x08A87EEC(&temporary, f12) into a temporary at frame + 0, assigns it over the
// destination (0x08A5BAD8) unless it is that temporary, and releases both with 0x08A5B9EC. Mode 1 makes the handle at frame + 4 and stores the virtual +0x54 results of
// the object word(a3) (the string 0x08CA697C reports a missing one) at object + 0x138 and + 0x13C, then releases the handle. Any other mode returns v0 = 1; v0 otherwise is the
// last callee's.
void func_089D15E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {31u, 0x20u}, {19u, 0x1Cu}, {16u, 0x10u}});
    if (mode == 0u) {
        const u32 holder = frame.at(4u);
        const u32 temporary = frame.at(0u);
        call_set(rt, ctx, 0x089CD62Cu, {holder});
        const u32 object = word(rt, holder);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
        call_float(rt, ctx, 0x08A87EECu, {temporary}, {float_at(rt, object + 0x138u)});
        ctx.gpr[5] = word(rt, temporary);
        if (destination != temporary) call_set(rt, ctx, 0x08A5BAD8u, {destination, ctx.gpr[5]});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        call_set(rt, ctx, 0x08A5B9ECu, {holder});
    } else if (mode == 1u) {
        const u32 holder = frame.at(4u);
        call_set(rt, ctx, 0x089CD62Cu, {holder});
        const auto virtual_float = [&]() {
            u32 owner = word(rt, holder);
            if (owner == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA69B8u});
            const u32 object = word(rt, source);
            if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA697Cu});
            call_virtual(rt, ctx, word(rt, word(rt, object) + 0x54u), {object});
            return owner;
        };
        u32 owner = virtual_float();
        put_float(rt, owner + 0x138u, ctx.fpr[0]);
        owner = virtual_float();
        put_float(rt, owner + 0x13Cu, ctx.fpr[0]);
        call_set(rt, ctx, 0x08A5B9ECu, {holder});
    } else {
        ctx.gpr[2] = 1u;
    }
}
void func_08C35920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[21] = 0x08CB0000u; G[20] = G[5]; G[19] = G[4]; G[18] = G[4] + 0xFFFFFFF4u; goto L_08C3596C; L_08C35954: ; G[5] = word(rt, G[18] + 0x4u); call_here(rt, ctx, 0x08A5BAD8u); G[2] = load8(G[18] + 0x8u); L_08C35960: ; store8(G[19] + 0x8u, G[2]); G[19] = G[18]; G[18] = G[18] + 0xFFFFFFF4u; L_08C3596C: ; G[17] = word(rt, G[20] + 0x4u); if (G[17] != 0u) { G[16] = word(rt, G[18] + 0x4u); goto L_08C35984; } G[4] = 0x08CB39CCu; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[18] + 0x4u); L_08C35984: ; { const bool tk = G[16] != 0u; G[17] = word(rt, G[17] + 0x14u); if (tk) goto L_08C35994; } G[4] = 0x08CB39CCu; call_here(rt, ctx, 0x08A5BB7Cu); L_08C35994: ; G[2] = word(rt, G[16] + 0x14u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[16] = word(rt, G[18] + 0x0u); goto L_08C359FC; } G[17] = word(rt, G[20] + 0x4u); if (G[17] != 0u) { G[16] = word(rt, G[18] + 0x4u); goto L_08C359BC; } G[4] = 0x08CB39CCu; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[18] + 0x4u); L_08C359BC: ; { const bool tk = G[16] != 0u; G[17] = word(rt, G[17] + 0x14u); if (tk) goto L_08C359CC; } G[4] = 0x08CB39CCu; call_here(rt, ctx, 0x08A5BB7Cu); L_08C359CC: ; G[2] = word(rt, G[16] + 0x14u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = word(rt, G[20] + 0x0u); if (tk) goto L_08C35A48; } G[16] = word(rt, G[18] + 0x0u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08B35FD8u); { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[4] = G[19] + 0x4u; if (tk) goto L_08C35A4C; } G[2] = G[18] + 0x4u; goto L_08C35A04; L_08C359FC: ; G[4] = G[19] + 0x4u; G[2] = G[18] + 0x4u; L_08C35A04: ; { const bool tk = G[4] != G[2]; put(rt, G[19] + 0x0u, G[16]); if (tk) goto L_08C35954; } G[2] = load8(G[18] + 0x8u); goto L_08C35960; L_08C35A14: ; G[5] = word(rt, G[5] + 0x0u); call_here(rt, ctx, 0x08A5BAD8u); G[2] = load8(G[20] + 0x8u); L_08C35A20: ; store8(G[19] + 0x8u, G[2]); goto exit_; L_08C35A48: ; G[4] = G[19] + 0x4u; L_08C35A4C: ; G[5] = G[20] + 0x4u; { const bool tk = G[4] != G[5]; put(rt, G[19] + 0x0u, G[17]); if (tk) goto L_08C35A14; } G[2] = load8(G[20] + 0x8u); goto L_08C35A20;  exit_: ; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08959B58 (a0 = the object; v0 = the last record address): for i = 0, 1, 2: a target value (i = 0: 0x089564E4(object); 1: 0x089564D8(object); 2: the float at
// word(object + 8) + 0x6E0) and the record 0x08959180(word(object), byte [word(object + 8)]) + i * 4 whose floats +0x7C (position) and +0x8C (rate) are advanced: with
// the gains g1 = [0x08CF3570 + i * 4] and g2 = [0x08CF3588 + i * 4], d = (g1 * (target - position) - g2 * rate) * k (k = the constant 0x3C88AB7C): rate += d, position += rate * k.
void func_08959B58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x20u}, {23u, 0x1Cu}, {20u, 0x10u}, {17u, 0x4u}, {31u, 0x24u}, {22u, 0x18u}, {21u, 0x14u}, {19u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}, {52u, 0x30u}});
    const float k = std::bit_cast<float>(0x3C88AB7Cu);
    const u32 state = word(rt, object + 8u);
    const u32 state_byte = load8(state);
    for (u32 i = 0; i < 3u; ++i) {
        float target = 0.0f;
        if (i == 0u) {
            call_float(rt, ctx, 0x089564E4u, {object}, {});
            target = ctx.fpr[0];
        } else if (i == 1u) {
            call_float(rt, ctx, 0x089564D8u, {object}, {});
            target = ctx.fpr[0];
        } else {
            target = float_at(rt, state + 0x6E0u);
        }
        const u32 record = call_set(rt, ctx, 0x08959180u, {word(rt, object), state_byte}) + 4u * i;
        const float position = float_at(rt, record + 0x7Cu);
        const float gain_position = float_at(rt, 0x08CF3570u + 4u * i);
        const float gain_rate = float_at(rt, 0x08CF3588u + 4u * i);
        const float rate = float_at(rt, record + 0x8Cu);
        float delta = psp_mul(gain_position, target - position) - psp_mul(gain_rate, rate);
        delta = psp_mul(delta, k);
        const float new_rate = rate + delta;
        put_float(rt, record + 0x8Cu, new_rate);
        put_float(rt, record + 0x7Cu, position + psp_mul(new_rate, k));
        ctx.gpr[2] = record;
    }
}
// 0x08B76940 (a0 = the key; v0 = the removed node, or 0 for a null key or a key whose first word is 0): under the lock 0x08C7A888 / 0x08C7A8A8 (the token kept in the word
// 0x08D72CEC) unlinks the first node whose word +0xC equals the key from the list at 0x08D72CF8 (next link at +0x14), or else from the list at 0x08D72CF4: there a node
// with bit 0 of its word +0x10 set matches when the key equals the first word of the object at +0xC, and its object is released with 0x08B35254 after the node is unlinked.
// A key found in neither list calls 0x08C7A8A8, 0x08B77B04 (stores its result in the last link slot and goes on if it returns), releases word(0xC) and unlocks.
void func_08B76940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kToken = 0x08D72CECu;
    constexpr u32 kFirstList = 0x08D72CF8u;
    constexpr u32 kSecondList = 0x08D72CF4u;
    const u32 key = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[17] = key;
    if (key == 0u || word(rt, key) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 token = call_set(rt, ctx, 0x08C7A888u, {});
    ctx.gpr[4] = token;
    put(rt, kToken, token);
    u32 slot = kFirstList;
    u32 node = word(rt, slot);
    while (node != 0u) {
        if (word(rt, node + 0xCu) == key) {
            put(rt, slot, word(rt, node + 0x14u));
            call_set(rt, ctx, 0x08C7A8A8u, {word(rt, kToken)});
            ctx.gpr[2] = node;
            return;
        }
        slot = node + 0x14u;
        node = word(rt, node + 0x14u);
    }
    slot = kSecondList;
    node = word(rt, slot);
    while (node != 0u) {
        if ((word(rt, node + 0x10u) & 1u) != 0u) {
            if (word(rt, word(rt, node + 0xCu)) == key) {
                put(rt, slot, word(rt, node + 0x14u));
                call_set(rt, ctx, 0x08B35254u, {word(rt, node + 0xCu)});
                call_set(rt, ctx, 0x08C7A8A8u, {word(rt, kToken)});
                ctx.gpr[2] = node;
                return;
            }
        } else if (word(rt, node + 0xCu) == key) {
            put(rt, slot, word(rt, node + 0x14u));
            call_set(rt, ctx, 0x08C7A8A8u, {word(rt, kToken)});
            ctx.gpr[2] = node;
            return;
        }
        slot = node + 0x14u;
        node = word(rt, node + 0x14u);
    }
    call_set(rt, ctx, 0x08C7A8A8u, {token});
    call_set(rt, ctx, 0x08B77B04u, {});
    put(rt, slot, ctx.gpr[2]);
    call_set(rt, ctx, 0x08B35254u, {word(rt, node + 0xCu)});
    call_set(rt, ctx, 0x08C7A8A8u, {word(rt, kToken)});
    ctx.gpr[2] = node;
}
void func_08C696CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x60u), G[31]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); G[2] = 0x08D70000u; G[23] = 0x08D77498u; G[22] = G[9]; G[21] = frame.at(0x8u); G[20] = G[8]; G[17] = G[7]; G[16] = G[4]; G[4] = G[21]; G[11] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[5] + 0x4u); G[10] = word(rt, G[5] + 0x0u); G[5] = word(rt, G[6] + 0x4u); G[6] = G[23] + 0xCu; put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x18u), G[5]); G[5] = 0x20u; put(rt, frame.at(0x1Cu), G[10]); put(rt, frame.at(0x14u), G[11]); put(rt, frame.at(0x8u), G[6]); call_here(rt, ctx, 0x08C75840u); G[4] = G[16]; G[7] = G[17]; G[9] = G[21]; G[5] = frame.at(0x1Cu); G[6] = frame.at(0x14u); G[8] = G[20]; call_here(rt, ctx, 0x08C684A4u); put(rt, frame.at(0x1Cu), G[2]); put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x30u), G[2]); put(rt, frame.at(0x34u), G[3]); G[16] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08B5C230u); put(rt, frame.at(0x4u), G[2]); G[4] = G[16]; G[5] = G[22]; G[6] = G[20]; G[7] = frame.at(0x4u); call_here(rt, ctx, 0x08B61CDCu); G[4] = word(rt, frame.at(0x8u)); G[2] = word(rt, frame.at(0x1Cu)); G[3] = word(rt, frame.at(0x20u)); G[16] = G[4] + 0xFFFFFFF4u; put(rt, frame.at(0xCu), G[2]); { const bool tk = G[16] != G[23]; put(rt, frame.at(0x10u), G[3]); if (tk) goto L_08C697E8; } G[18] = word(rt, frame.at(0xCu)); L_08C697B0: ; G[19] = word(rt, frame.at(0x10u)); G[2] = G[18]; G[3] = G[19]; goto exit_; L_08C697E8: ; G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[18] = word(rt, frame.at(0xCu)); goto L_08C697B0; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C74884u); G[18] = word(rt, frame.at(0xCu)); goto L_08C697B0;  exit_: ; G[23] = word(rt, frame.at(0x5Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C69864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x60u), G[31]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); G[2] = 0x08D70000u; G[23] = 0x08D77498u; G[22] = G[9]; G[21] = frame.at(0x8u); G[20] = G[8]; G[17] = G[7]; G[16] = G[4]; G[4] = G[21]; G[11] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[5] + 0x4u); G[10] = word(rt, G[5] + 0x0u); G[5] = word(rt, G[6] + 0x4u); G[6] = G[23] + 0xCu; put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x18u), G[5]); G[5] = 0x20u; put(rt, frame.at(0x1Cu), G[10]); put(rt, frame.at(0x14u), G[11]); put(rt, frame.at(0x8u), G[6]); call_here(rt, ctx, 0x08C75840u); G[4] = G[16]; G[7] = G[17]; G[9] = G[21]; G[5] = frame.at(0x1Cu); G[6] = frame.at(0x14u); G[8] = G[20]; call_here(rt, ctx, 0x08C684A4u); put(rt, frame.at(0x1Cu), G[2]); put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x30u), G[2]); put(rt, frame.at(0x34u), G[3]); G[16] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08B5C230u); put(rt, frame.at(0x4u), G[2]); G[4] = G[16]; G[5] = G[22]; G[6] = G[20]; G[7] = frame.at(0x4u); call_here(rt, ctx, 0x08B61A80u); G[4] = word(rt, frame.at(0x8u)); G[2] = word(rt, frame.at(0x1Cu)); G[3] = word(rt, frame.at(0x20u)); G[16] = G[4] + 0xFFFFFFF4u; put(rt, frame.at(0xCu), G[2]); { const bool tk = G[16] != G[23]; put(rt, frame.at(0x10u), G[3]); if (tk) goto L_08C69980; } G[18] = word(rt, frame.at(0xCu)); L_08C69948: ; G[19] = word(rt, frame.at(0x10u)); G[2] = G[18]; G[3] = G[19]; goto exit_; L_08C69980: ; G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[18] = word(rt, frame.at(0xCu)); goto L_08C69948; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C74884u); G[18] = word(rt, frame.at(0xCu)); goto L_08C69948;  exit_: ; G[23] = word(rt, frame.at(0x5Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089B7CF8 (a0 = the object, a1 = a pass-through argument, a2 = a pointer to a byte array, a3 = a limit; v0 as the registers leave it, or a tail call to 0x089B7C80(object,
// a1, 0)): with word(object + 0x124) set, walks the nodes of the list headed by word(word(object + 0x124) + 0xF4) (next node 0x08968A38) for at most min(a3, 0x08B2080C(object + 0x138))
// steps; the node whose index equals the word +0x138 and whose byte is set marks the walk. Every node whose word +0xE0 lacks bit 0x1000 is given its byte of the array through
// 0x08968850(node, byte) and counts a step. When a byte at the marked index was non-zero the walk ends with the tail call. v0 is the head word when the step count is 0 or less,
// 1 when the nodes ran out first, 0 when the steps did.
namespace {
bool walk_flagged_nodes(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 &tail_object, u32 &tail_argument) {
    const u32 object = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 bytes = ctx.gpr[6];
    const s32 limit = static_cast<s32>(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{23u, 0x1Cu}, {21u, 0x14u}, {19u, 0xCu}, {16u, 0x0u}, {31u, 0x20u}, {22u, 0x18u}, {20u, 0x10u}, {18u, 0x8u}, {17u, 0x4u}});
    ctx.gpr[2] = word(rt, object + 0x124u);
    if (ctx.gpr[2] == 0u) return false;
    const u32 list = object + 0x138u;
    const u32 marked_index = word(rt, list);
    const s32 count = static_cast<s32>(call_set(rt, ctx, 0x08B2080Cu, {list}));
    s32 steps = limit;
    if (!(limit < count)) steps = static_cast<s32>(call_set(rt, ctx, 0x08B2080Cu, {list}));
    u32 node = word(rt, word(rt, object + 0x124u) + 0xF4u);
    ctx.gpr[2] = node;
    if (steps <= 0) return false;
    if (node == 0u) return false;
    u32 index = 0u;
    bool marked = false;
    for (;;) {
        if ((word(rt, node + 0xE0u) & 0x1000u) == 0u) {
            const u32 address = bytes + index;
            if (index == marked_index && load8(address) != 0u) marked = true;
            const u32 value = load8(address);
            ++index;
            call_set(rt, ctx, 0x08968850u, {node, value});
        }
        node = call_set(rt, ctx, 0x08968A38u, {node});
        if (!(static_cast<s32>(index) < steps)) {
            ctx.gpr[2] = 0u;
            break;
        }
        ctx.gpr[2] = 1u;
        if (node == 0u) break;
    }
    if (!marked) return false;
    tail_object = object;
    tail_argument = argument;
    return true;
}
} // namespace
void func_089B7CF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 tail_object = 0u;
    u32 tail_argument = 0u;
    if (walk_flagged_nodes(rt, ctx, tail_object, tail_argument)) call_set(rt, ctx, 0x089B7C80u, {tail_object, tail_argument, 0u});
}
void func_08AA9BEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); G[18] = G[4] + 0x20u; G[2] = G[4]; G[4] = G[18]; G[19] = G[5]; G[16] = G[2] + 0x24u; G[20] = word(rt, G[18] + 0x14u); call_here(rt, ctx, 0x08C315C4u); G[4] = G[18]; { const bool tk = G[16] == G[2]; G[5] = G[19]; if (tk) goto L_08AA9C44; } G[20] = word(rt, G[2] + 0x14u); goto L_08AA9D04; L_08AA9C44: ; call_here(rt, ctx, 0x08C31F14u); G[23] = frame.at(0xCu); G[22] = G[2]; G[21] = G[2]; { const bool tk = G[16] == G[2]; G[6] = frame.at(0x8u); if (tk) goto L_08AA9CA0; } G[4] = word(rt, G[19] + 0x0u); G[16] = word(rt, G[4] + 0xFFFFFFF4u); put(rt, frame.at(0x4u), G[16]); G[2] = word(rt, G[2] + 0x10u); G[17] = word(rt, G[2] + 0xFFFFFFF4u); G[2] = frame.at(0x4u); G[3] = G[17] < G[16] ? 1u : 0u; put(rt, frame.at(0x8u), G[17]); if (G[3] == 0u) G[6] = G[2]; G[16] = G[16] - G[17]; G[6] = word(rt, G[6] + 0x0u); G[5] = word(rt, G[22] + 0x10u); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; if (static_cast<std::int32_t>(G[16]) >= 0) { put(rt, G[21] + 0x14u, G[20]); goto L_08AA9D04; } L_08AA9CA0: ; G[5] = G[19]; G[4] = G[23]; call_here(rt, ctx, 0x08B88F44u); G[4] = G[18]; G[5] = G[22]; G[6] = G[23]; put(rt, frame.at(0x10u), 0u); call_here(rt, ctx, 0x08C33C44u); G[17] = G[2]; G[2] = word(rt, frame.at(0xCu)); G[5] = 0xFFFFFFFFu; G[4] = G[2] + 0xFFFFFFFCu; G[16] = G[2] + 0xFFFFFFF4u; G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; G[21] = G[17]; if (tk) goto L_08AA9D00; } call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0u); if (tk) goto L_08AA9D00; } call_here(rt, ctx, 0x08B8542Cu); L_08AA9D00: ; put(rt, G[21] + 0x14u, G[20]); L_08AA9D04: ; G[2] = G[20]; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x28u)); G[19] = word(rt, frame.at(0x2Cu)); G[16] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B2D314 (a1 = a pointer whose word +4 holds event bits; v0 = 0): the state word 0x08D68D40 and the handle word 0x08D68D58 of a worker, the busy byte 0x08D68D4C. Unless the state word
// 0x08D68CF8 is 1 the busy byte is set; bits without 0x80 clear it again. Bit 1 starts the worker: state = 1, the handle 0x08C7AA18(0x08CB53E8, 0x08B2D824, 0x20, 0x1000, 0x200000, 0,
// stack word 0xFF) is stored (a negative one sets the state 2 and leaves the byte set), else 0x08C7A9D0(handle, 4, &0xFF) runs. The other bits set the state 3 (bit 2), 4 (0x40),
// 5 (0x20), 6 (8), 7 (0x10) or 8, in that order of priority.
void func_08B2D314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kState = 0x08D68D40u;
    constexpr u32 kBusy = 0x08D68D4Cu;
    constexpr u32 kHandle = 0x08D68D58u;
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x18u}, {17u, 0x14u}});
    ctx.gpr[2] = 1u;
    ctx.gpr[10] = 1u;
    const u32 running = word(rt, 0x08D68CF8u);
    store8(kBusy, 1u);
    ctx.gpr[2] = 0u;
    if (running == 1u) {
        store8(kBusy, 0u);
        return;
    }
    const u32 events = word(rt, ctx.gpr[5] + 4u);
    ctx.gpr[5] = events;
    if ((events & 0x80u) == 0u) {
        store8(kBusy, 0u);
        return;
    }
    if ((events & 1u) != 0u) {
        put(rt, frame.at(0u), 0xFFu);
        put(rt, kState, 1u);
        const s32 handle = static_cast<s32>(call_set(rt, ctx, 0x08C7AA18u, {0x08CB53E8u, 0x08B2D824u, 0x20u, 0x1000u, 0x00200000u, 0u}));
        put(rt, kHandle, static_cast<u32>(handle));
        if (handle < 0) {
            put(rt, kState, 2u);
        } else {
            call_set(rt, ctx, 0x08C7A9D0u, {static_cast<u32>(handle), 4u, frame.at(0u)});
            store8(kBusy, 0u);
        }
        ctx.gpr[2] = 0u;
        return;
    }
    u32 state = 8u;
    if ((events & 2u) != 0u) state = 3u;
    else if ((events & 0x40u) != 0u) state = 4u;
    else if ((events & 0x20u) != 0u) state = 5u;
    else if ((events & 8u) != 0u) state = 6u;
    else if ((events & 0x10u) != 0u) state = 7u;
    put(rt, kState, state);
    store8(kBusy, 0u);
    ctx.gpr[2] = 0u;
}
// 0x08C72194 (a0 = the stream, a1 = a character (low byte, sign-extended); v0 = a0): `ostream::put`. A sentry is built on the stack by 0x08C70314(frame, stream) (byte at frame + 0 = ok, the
// stream pointer it keeps at frame + 4). When it is ok the stream buffer (word at stream + virtual-base offset + 0x78; the offset is word(vtable - 0xC)) takes the byte: room (put pointer +0x14
// below the end +0x18) stores it and moves the pointer, otherwise the buffer's virtual +0x34 (overflow) answers, and -1 sets the error bit with 0x08C54CFC(stream part, state | 1).
// Then, like the sentry's destructor: with the unit-buffer flag (0x2000 of the flags word at +0xC) set and no 0x08B62318() exception, the buffer's virtual +0x18 (sync) answers and -1 sets the error bit too.
void func_08C72194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    const u32 character = sext(ctx.gpr[5], 8u);
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x18u}});
    ctx.gpr[17] = stream;
    ctx.gpr[16] = character;
    const auto stream_part = [&](u32 object) { return object + word(rt, word(rt, object) - 0xCu); };
    const auto set_error = [&](u32 part) { call_set(rt, ctx, 0x08C54CFCu, {part, word(rt, part + 0x14u) | 1u}); };
    call_set(rt, ctx, 0x08C70314u, {frame.at(0u), stream});
    ctx.gpr[2] = load8(frame.at(0u));
    if (load8(frame.at(0u)) != 0u) {
        const u32 buffer = word(rt, stream_part(stream) + 0x78u);
        const u32 put_pointer = word(rt, buffer + 0x14u);
        const u32 end_pointer = word(rt, buffer + 0x18u);
        bool end_of_file = false;
        if (put_pointer < end_pointer) {
            store8(put_pointer, character);
            put(rt, buffer + 0x14u, word(rt, buffer + 0x14u) + 1u);
        } else {
            ctx.gpr[5] = character & 0xFFu;
            end_of_file = call_virtual(rt, ctx, word(rt, word(rt, buffer) + 0x34u), {buffer, character & 0xFFu}) == 0xFFFFFFFFu;
        }
        if (end_of_file) set_error(stream_part(stream));
    }
    const u32 kept = word(rt, frame.at(4u));
    ctx.gpr[2] = stream;
    if ((word(rt, stream_part(kept) + 0xCu) & 0x2000u) != 0u && call_set(rt, ctx, 0x08B62318u, {}) == 0u) {
        const u32 buffer = word(rt, stream_part(word(rt, frame.at(4u))) + 0x78u);
        if (buffer != 0u && call_virtual(rt, ctx, word(rt, word(rt, buffer) + 0x18u), {buffer}) == 0xFFFFFFFFu) set_error(stream_part(word(rt, frame.at(4u))));
    }
    ctx.gpr[2] = stream;
}
// 0x088F6C78 (a0 = a vector (x, y, z) updated in place, a1 / a2 / a3 / t0 = float records, f12 = a scale, f13 = a step; v0 = 1 when the vector was reset to zero or is
// already at rest): the horizontal length of (x, z) with y cleared; zero returns 1 with nothing changed. A limit f1 = [a3] * f12 * [t0 + 0x10] * f13 at or above that length
// leaves (0, 0, 0) and 1. Otherwise (x, z) shrink along themselves by the limit times the step (a component whose product with its shrunk value turns negative goes to 0);
// a result shorter than the limit again gives (0, 0, 0) and 1, the rest is stored with a push from the records (a2 + 0xC / a2 + 8 times a1 + 0 / a1 + 8, plus [a3] * [t0 + 0x14] and [a3] * [t0 + 0x18])
// times the step, and v0 = 0.
void func_088F6C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4];
    const u32 push_a = ctx.gpr[5];
    const u32 push_b = ctx.gpr[6];
    const u32 scale_pointer = ctx.gpr[7];
    const u32 table = ctx.gpr[8];
    const float step = ctx.fpr[13];
    const float x = float_at(rt, vector);
    const float z = float_at(rt, vector + 8u);
    const float gain = float_at(rt, scale_pointer);
    const float table_limit = float_at(rt, table + 0x10u);
    const float scaled = psp_mul(gain, ctx.fpr[12]);
    put_float(rt, vector + 4u, 0.0f);
    const float length = std::sqrt((psp_mul(x, x) + 0.0f) + psp_mul(z, z));
    const float limit = psp_mul(scaled, table_limit);
    ctx.gpr[2] = 1u;
    if (length == 0.0f) return;
    const float threshold = psp_mul(limit, step);
    if (length < threshold) {
        put_float(rt, vector, 0.0f);
        put_float(rt, vector + 8u, 0.0f);
        return;
    }
    const float along_x = psp_mul(step, psp_mul(limit, x) / length);
    const float along_z = psp_mul(step, psp_mul(limit, z) / length);
    float shrunk_x = x - along_x;
    float shrunk_z = z - along_z;
    float square_x = 0.0f;
    float square_z = 0.0f;
    if (psp_mul(x, shrunk_x) < 0.0f) shrunk_x = 0.0f;
    else square_x = psp_mul(shrunk_x, shrunk_x);
    if (psp_mul(z, shrunk_z) < 0.0f) shrunk_z = 0.0f;
    else square_z = psp_mul(shrunk_z, shrunk_z);
    if (std::sqrt((square_x + 0.0f) + square_z) < threshold) {
        put_float(rt, vector, 0.0f);
        put_float(rt, vector + 8u, 0.0f);
        put_float(rt, vector + 4u, 0.0f);
        return;
    }
    ctx.gpr[2] = 0u;
    put_float(rt, vector, shrunk_x);
    put_float(rt, vector + 8u, shrunk_z);
    const float ratio = float_at(rt, push_b + 0xCu) / float_at(rt, push_b + 8u);
    const float push_x = psp_mul(ratio, float_at(rt, push_a));
    const float push_z = psp_mul(ratio, float_at(rt, push_a + 8u));
    const float base_x = psp_mul(gain, float_at(rt, table + 0x14u)) + push_x;
    put_float(rt, vector, shrunk_x + psp_mul(step, base_x));
    const float base_z = psp_mul(gain, float_at(rt, table + 0x18u)) + push_z;
    put_float(rt, vector + 8u, shrunk_z + psp_mul(step, base_z));
}
void func_08926AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); auto &F = ctx.fpr; G[20] = G[4]; G[4] = G[4] + 0x10u; G[21] = G[20] + 0x50u; G[19] = G[6]; G[18] = G[5]; G[17] = G[7]; call_here(rt, ctx, 0x08926008u); G[16] = G[20] + 0xB0u; F[0] = float_at(rt, G[20] + 0x50u); G[7] = G[17]; G[5] = G[18]; put_float(rt, G[20] + 0xB0u, F[0]); G[6] = G[19]; G[4] = G[20]; F[0] = float_at(rt, G[21] + 0x4u); put_float(rt, G[16] + 0x4u, F[0]); F[0] = float_at(rt, G[21] + 0x8u); put_float(rt, G[16] + 0x8u, F[0]); F[1] = float_at(rt, G[21] + 0xCu); put_float(rt, G[16] + 0xCu, F[1]); G[2] = word(rt, G[20] + 0x1F4u); G[8] = word(rt, G[20] + 0x0u); G[2] = G[2] + 0x1u; put(rt, G[20] + 0x1F4u, G[2]); G[3] = word(rt, G[8] + 0x48u); { const std::uint32_t tg = G[3]; G[17] = G[20] + 0x70u; call_here(rt, ctx, tg); } G[2] = load8(G[20] + 0xC0u); G[6] = G[16]; G[4] = frame.at(0u); { const bool tk = G[2] != 0u; G[5] = frame.at(0u); if (tk) goto L_08926BF8; } F[3] = float_at(rt, G[21] + 0xCu); F[2] = float_at(rt, G[21] + 0x8u); F[0] = float_at(rt, G[20] + 0x50u); F[1] = float_at(rt, G[21] + 0x4u); put_float(rt, frame.at(0x8u), F[2]); put_float(rt, frame.at(0xCu), F[3]); put_float(rt, frame.at(0x0u), F[0]); put_float(rt, frame.at(0x4u), F[1]); call_here(rt, ctx, 0x08B4A8D4u); G[2] = 0x08CE0000u; F[0] = float_at(rt, frame.at(0x0u)); F[1] = float_at(rt, frame.at(0x4u)); F[12] = float_at(rt, 0x08CDDD68u); put_float(rt, frame.at(0x10u), F[0]); G[4] = frame.at(0x10u); F[0] = float_at(rt, frame.at(0x8u)); put_float(rt, frame.at(0x14u), F[1]); F[1] = float_at(rt, frame.at(0xCu)); put_float(rt, frame.at(0x18u), F[0]); put_float(rt, frame.at(0x1Cu), F[1]); call_here(rt, ctx, 0x08B4A6D4u); F[2] = float_at(rt, frame.at(0x14u)); F[3] = float_at(rt, frame.at(0x18u)); F[1] = float_at(rt, frame.at(0x1Cu)); F[0] = float_at(rt, frame.at(0x10u)); put_float(rt, G[20] + 0x70u, F[0]); put_float(rt, G[17] + 0xCu, F[1]); put_float(rt, G[17] + 0x4u, F[2]); put_float(rt, G[17] + 0x8u, F[3]); goto L_08926C08; L_08926BF8: ; put(rt, G[20] + 0x70u, 0u); put(rt, G[17] + 0xCu, 0u); put(rt, G[17] + 0x4u, 0u); put(rt, G[17] + 0x8u, 0u); L_08926C08: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x30u)); G[21] = word(rt, frame.at(0x34u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0893EEC4 (a0 = the object; v0 = 1 when the tables were filled, else the value that stopped it): when 0x0893ECA4(object) answers non-zero the record at word(object + 8) + 0x778 is
// reset (word 0, float +4 = 1.0, bytes +8 / +9 = 0) and, when 0x0893ECA4 still answers non-zero and the byte 0x08CF2AAC is 0, the class byte [word(object + 0xC) + 0x282] equal to 3 fills
// the tables once: n = min(the halfword at +0x170 + 1, 12) is stored as the halfword 0x08CDDF3C, the words 0x08CF2A7C / 0x08CF2A4C are cleared and for n - 1 entries i the float
// at +0x17C + i * 4 goes to 0x08CF2A50 + i * 4 and (f * 60 / 2pi) * ([+0x1DC + i * 4] / 9.8) / the constant 0x44330CCD * the constant 0x3F3C49BA to 0x08CF2A80 + i * 4; the byte 0x08CF2AAC becomes 1.
void func_0893EEC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    ctx.gpr[16] = object;
    if (call_set(rt, ctx, 0x0893ECA4u, {object}) == 0u) return;
    const u32 state = word(rt, object + 8u);
    const u32 record = state + 0x778u;
    put(rt, record, 0u);
    put_float(rt, record + 4u, 1.0f);
    store8(record + 9u, 0u);
    store8(record + 8u, 0u);
    if (call_set(rt, ctx, 0x0893ECA4u, {object}) == 0u) return;
    constexpr u32 kDone = 0x08CF2AACu;
    const u32 done = load8(kDone);
    ctx.gpr[2] = done;
    if (done != 0u) return;
    const u32 owner = word(rt, object + 0xCu);
    const u32 klass = load8(owner + 0x282u);
    ctx.gpr[2] = 3u;
    if (klass != 3u) return;
    const u32 table = owner + 0x170u;
    s32 count = static_cast<s32>(signed_half(rt, table)) + 1;
    s32 stored = static_cast<std::int16_t>(count);
    if (!(count < 0xD)) {
        count = 0xC;
        stored = 0xC;
    }
    store16(0x08CDDF3Cu, stored);
    put(rt, 0x08CF2A7Cu, 0u);
    put(rt, 0x08CF2A4Cu, 0u);
    if (!(count < 2)) {
        const float sixty = std::bit_cast<float>(0x42700000u);
        const float two_pi = std::bit_cast<float>(0x40C90FDBu);
        const float gravity = std::bit_cast<float>(0x411CCCCDu);
        const float divisor = std::bit_cast<float>(0x44330CCDu);
        const float factor = std::bit_cast<float>(0x3F3C49BAu);
        for (s32 i = 0; i < count - 1; ++i) {
            const float speed = float_at(rt, table + 0xCu + 4u * static_cast<u32>(i));
            const float load = float_at(rt, table + 0x6Cu + 4u * static_cast<u32>(i)) / gravity;
            float value = psp_mul(speed, sixty) / two_pi;
            value = psp_mul(value, load);
            value = value / divisor;
            value = psp_mul(value, factor);
            put_float(rt, 0x08CF2A50u + 4u * static_cast<u32>(i), speed);
            put_float(rt, 0x08CF2A80u + 4u * static_cast<u32>(i), value);
        }
    }
    store8(kDone, 1u);
    ctx.gpr[2] = 1u;
}
// 0x08A265F8 (a0 = the destination object, a1 = a count, a2 = a pointer to a handle; v0 = the last callee's): nothing for a count of 0 or less. Otherwise the virtual +0x54 of the object word(a2)
// (the string 0x08CAAFBC reports a missing one through 0x08A5BB7C) gives a value f; with the object at 0x08D6950C in use f is replaced by 0x08B48EAC(0x08D6950C, f * the constant 0x3E8E38E4);
// 0x08B48BFC(0x08D6950C) and the text 0x08CAB284 format (0x08B37640) the text (int)(f * 10 + 0.5) into the temporary at frame + 0xC; it is copied into frame + 4 (0x08B85EB8), converted with
// 0x08A6D2D4 into frame + 8 and assigned over the destination (0x08A5BAD8) unless it is that temporary; the temporaries are released (0x08A5B9EC, and 0x08B60804 / 0x08B8542C
// unless the string is the static 0x08D74FD0).
void func_08A265F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 handle_slot = ctx.gpr[6];
    GuestFrame frame(ctx, 0xC0u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x9Cu}, {31u, 0xA0u}, {18u, 0x98u}, {17u, 0x94u}, {16u, 0x90u}, {52u, 0xB0u}});
    if (count <= 0) return;
    const u32 object = word(rt, handle_slot);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAFBCu});
    call_virtual(rt, ctx, word(rt, word(rt, object) + 0x54u), {object});
    float value = ctx.fpr[0];
    constexpr u32 kSource = 0x08D6950Cu;
    if (word(rt, kSource) != 0u) {
        value = call_float(rt, ctx, 0x08B48EACu, {kSource}, {psp_mul(value, std::bit_cast<float>(0x3E8E38E4u))});
    }
    const u32 extra = call_set(rt, ctx, 0x08B48BFCu, {kSource});
    const u32 rounded = trunc_to_word(psp_mul(value, std::bit_cast<float>(0x41200000u)) + 0.5f);
    const u32 text = frame.at(0xCu);
    const u32 converted = frame.at(4u);
    const u32 result = frame.at(8u);
    call_set(rt, ctx, 0x08B37640u, {text, 0x08CAB284u, rounded, extra});
    call_set(rt, ctx, 0x08B85EB8u, {converted, text, frame.at(1u)});
    call_set(rt, ctx, 0x08A6D2D4u, {result, converted});
    ctx.gpr[5] = word(rt, result);
    if (destination != result) call_set(rt, ctx, 0x08A5BAD8u, {destination, ctx.gpr[5]});
    call_set(rt, ctx, 0x08A5B9ECu, {result});
    const u32 string = word(rt, converted);
    if (string - 0xCu == 0x08D74FD0u) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {string - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {string - 0xCu, frame.at(0u)});
}
void func_08B059D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x3Cu), G[31]); auto &F = ctx.fpr; G[18] = 0x1u; G[17] = G[6]; G[16] = G[4]; G[3] = sext(load16(G[6] + 0x0u), 16); store8(frame.at(0x0u), G[2]); G[2] = G[3] << 8; G[6] = word(rt, G[5] + 0x0u); G[7] = word(rt, frame.at(0x0u)); G[3] = G[3] << 4; G[2] = G[2] - G[3]; G[6] = G[6] + G[2]; put(rt, frame.at(0x20u), G[7]); put(rt, frame.at(0x24u), G[5]); put(rt, frame.at(0x28u), G[6]); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x8u), G[6]); put(rt, frame.at(0x4u), G[5]); put(rt, frame.at(0x10u), G[7]); put(rt, frame.at(0x14u), G[5]); put(rt, frame.at(0x18u), G[6]); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x2Cu), 0u); goto L_08B05AE0; L_08B05A40: ; call_here(rt, ctx, 0x08BBBEC4u); F[1] = float_at(rt, G[16] + 0x44u); ctx.set_fpu_condition(F[1] <= F[0]); { const bool tk = ctx.fpu_condition(); G[3] = word(rt, frame.at(0x28u)); if (tk) goto L_08B05A80; } G[4] = word(rt, frame.at(0x28u)); G[4] = G[4] + 0x50u; G[5] = G[4]; call_here(rt, ctx, 0x08BBBEC4u); F[1] = float_at(rt, G[16] + 0x48u); ctx.set_fpu_condition(F[1] <= F[0]); { const bool tk = !ctx.fpu_condition(); G[3] = word(rt, frame.at(0x28u)); if (tk) goto L_08B05A94; } L_08B05A80: ; G[18] = 0u; G[2] = load16(G[3] + 0xE2u); G[2] = (G[2] & ~0x4u) | ((0u << 2) & 0x4u); store16(G[3] + 0xE2u, G[2]); goto L_08B05AA0; L_08B05A94: ; G[2] = load16(G[3] + 0xE2u); G[2] = G[2] | 0x4u; store16(G[3] + 0xE2u, G[2]); L_08B05AA0: ; G[2] = word(rt, frame.at(0x28u)); G[5] = frame.at(0x20u); G[6] = G[16]; G[4] = G[2]; put(rt, frame.at(0x2Cu), G[2]); call_here(rt, ctx, 0x08C46D74u); G[2] = sext(G[2] & 0xFFFFu, 16); G[3] = G[2] << 4; G[4] = G[2] << 8; G[4] = G[4] - G[3]; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[3] = 0u; if (tk) goto L_08B05ADC; } G[2] = word(rt, frame.at(0x24u)); G[3] = word(rt, G[2] + 0x0u); G[3] = G[3] + G[4]; L_08B05ADC: ; put(rt, frame.at(0x28u), G[3]); L_08B05AE0: ; G[2] = word(rt, frame.at(0x28u)); G[4] = G[2] + 0x38u; { const bool tk = G[2] != 0u; G[5] = G[4]; if (tk) goto L_08B05A40; } { const bool tk = G[18] == 0u; if (tk) goto L_08B05B08; } G[2] = load16(G[17] + 0x2u); G[2] = G[2] | 0x4000u; store16(G[17] + 0x2u, G[2]); L_08B05B08: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08C5B850 (a0 = the end of the buffer, a2 / a3 = the low / high word of the value, t0 = the digit table, t1 = the flags; v0 = the length written): writes the digits of the 64-bit value
// backwards from a0 down: flags & 0x4A equal to 0x40 give octal (table + 4 + the low 3 bits), equal to 8 hexadecimal (table + 4, or table + 0x14 with the flag 0x4000, + the low nibble),
// anything else decimal (0x08B708C4 gives the remainder and 0x08B702B4 the quotient of the value by 10, the digit at table + 4 + the remainder). At least one digit is written.
void func_08C5B850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 end = ctx.gpr[4];
    u32 low = ctx.gpr[6];
    u32 high = ctx.gpr[7];
    const u32 table = ctx.gpr[8];
    const u32 flags = ctx.gpr[9];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x14u}, {18u, 0x8u}});
    const u32 kind = flags & 0x4Au;
    u32 cursor = end;
    if (kind == 0x40u || kind == 8u) {
        const unsigned shift = (kind == 0x40u) ? 3u : 4u;
        const u32 mask = (1u << shift) - 1u;
        u32 base = table + 4u;
        if (kind == 8u && (flags & 0x4000u) != 0u) base = table + 0x14u;
        u32 value;
        do {
            const u32 digit = load8(base + (low & mask));
            low = (high << (32u - shift)) | (low >> shift);
            high >>= shift;
            --cursor;
            value = low | high;
            store8(cursor, digit);
        } while (value != 0u);
    } else {
        do {
            const u32 remainder = call_set(rt, ctx, 0x08B708C4u, {low, high, 10u, 0u});
            const u32 digit = load8(table + remainder + 4u);
            --cursor;
            store8(cursor, digit);
            const u32 quotient_low = call_set(rt, ctx, 0x08B702B4u, {low, high, 10u, 0u});
            const u32 quotient_high = ctx.gpr[3];
            low = quotient_low;
            high = quotient_high;
            ctx.gpr[2] = quotient_low | quotient_high;
        } while (ctx.gpr[2] != 0u);
    }
    ctx.gpr[2] = end - cursor;
}
// 0x088708E8 (a0 = the object, a1 = a handle, a2 = a byte, a3 = a pointer to 3 floats (set at +0x70, +0x74, +0x78 of word(object + 0xC)), t0 = a pointer to 3 floats (applied to the
// record at +0x40 through 0x08B4F174 (y), 0x08B4F47C (z) and 0x08B4F2E4 (x)); v0 = the last callee's): with a handle, 0x08B945C4(record + 0x88, handle, 0x08B35F60(handle)) is called, the
// byte goes to record + 0x8C, 0x08B4E648(record + 0x40) runs and the float sets follow. With no handle the temporary at frame + 4 (the static 0x08D752A4) is rebuilt with 0x08B958A8(&temporary,
// record + 0x88) and released (0x08B60804 / 0x08B93554 unless it is the static 0x08D75298).
void func_088708E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 handle = ctx.gpr[5];
    const u32 byte = ctx.gpr[6] & 0xFFu;
    const u32 position = ctx.gpr[7];
    const u32 rotation = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {31u, 0x2Cu}, {16u, 0x10u}});
    constexpr u32 kStatic = 0x08D75298u;
    ctx.gpr[6] = byte;
    if (handle != 0u) {
        const u32 record_before = word(rt, object + 0xCu);
        const u32 size = call_set(rt, ctx, 0x08B35F60u, {handle});
        call_set(rt, ctx, 0x08B945C4u, {record_before + 0x88u, handle, size});
        store8(word(rt, object + 0xCu) + 0x8Cu, byte);
        call_set(rt, ctx, 0x08B4E648u, {word(rt, object + 0xCu) + 0x40u});
        if (rotation != 0u) {
            call_float(rt, ctx, 0x08B4F174u, {word(rt, object + 0xCu) + 0x40u}, {float_at(rt, rotation + 4u)});
            call_float(rt, ctx, 0x08B4F47Cu, {word(rt, object + 0xCu) + 0x40u}, {float_at(rt, rotation + 8u)});
            call_float(rt, ctx, 0x08B4F2E4u, {word(rt, object + 0xCu) + 0x40u}, {float_at(rt, rotation)});
        }
        if (position != 0u) {
            put(rt, word(rt, object + 0xCu) + 0x70u, word(rt, position));
            put(rt, word(rt, object + 0xCu) + 0x74u, word(rt, position + 4u));
            ctx.gpr[3] = word(rt, object + 0xCu);
            put(rt, word(rt, object + 0xCu) + 0x78u, word(rt, position + 8u));
            ctx.gpr[2] = word(rt, object + 0xCu);
        }
        return;
    }
    const u32 temporary = frame.at(4u);
    put(rt, temporary, kStatic + 0xCu);
    ctx.gpr[2] = kStatic + 0xCu;
    call_set(rt, ctx, 0x08B958A8u, {temporary, word(rt, object + 0xCu) + 0x88u});
    const u32 string = word(rt, temporary);
    if (string - 0xCu == kStatic) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {string - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B93554u, {string - 0xCu, frame.at(0u)});
}
// 0x08952400 (a0 = a flag (low byte), f12 / f13 / f14 = floats x, y, z; v0 = a class 0-4): y = 0, or y * x above 0 with x * (y - x) below 0, answers 4 when the flag is set and x * z is above 0, else 1.
// Otherwise x is placed against 0 and y (the limit register is 0 there: the delay slot of the branch clears it): "span" is x above 0 with y < x, or x below 0 with x < y (3 for a clear flag or
// y * z <= 0, else 2); "compare" is x above 0 with x <= y, or x = 0 (y <= x gives the "upper" test, else 2 when x < z, else 0); "upper" is x below 0 with y <= x (0 when x <= y, else 2 when z < x, else 0).
void func_08952400(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const u32 flag = ctx.gpr[4] & 0xFFu;
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    const float z = ctx.fpr[14];
    ctx.gpr[4] = flag;
    const bool positive = flag != 0u && (0.0f < psp_mul(x, z));
    const auto finish = [&](u32 code) {
        ctx.gpr[2] = code;
    };
    const auto upper_case = [&]() -> u32 {
        if (x <= y) return 0u;
        return (z < x) ? 2u : 0u;
    };
    const auto compare_case = [&]() -> u32 {
        if (y <= x) return upper_case();
        return (x < z) ? 2u : 0u;
    };
    if (y == 0.0f) {
        finish(positive ? 4u : 1u);
        return;
    }
    // The delay slot of the branch on x * (y - x) clears the limit register: the limit is 0 whenever x is placed.
    const float limit = 0.0f;
    if (!(psp_mul(y, x) <= 0.0f) && !(0.0f <= psp_mul(x, y - x))) {
        finish(positive ? 4u : 1u);
        return;
    }
    enum class Route { kSpan, kCompare, kUpper };
    Route route = Route::kCompare;
    if (x <= limit) {
        if (limit <= x) route = Route::kCompare;
        else if (!(x < y)) route = Route::kUpper;
        else route = Route::kSpan;
    } else if (y < x) {
        route = Route::kSpan;
    }
    if (route == Route::kSpan) {
        if (flag == 0u) finish(3u);
        else finish((psp_mul(y, z) <= 0.0f) ? 3u : 2u);
    } else if (route == Route::kCompare) {
        finish(compare_case());
    } else {
        finish(upper_case());
    }
}
// 0x08A010D0 (a0 = the object, a1 = the target object; v0 = the last callee's): nothing without the member word(object + 0x10C). With it: the frame block {0, 0, f, 0, f, 0, g, g, 0} at
// frame + 0x10 (f = the float member +0x18, g = the float +0x1C) is built, a = 0x089A8B68(target) and b = 0x089A8B80(target) are read, the virtual +0x110 of the target runs (target, 2), the byte
// +0x118 of the target becomes 1 and its word +0x11C the member, the virtual +0x108 runs (target, frame block of ones, 0x...), then 0x089A9654(target, frame + 0x10, 0, f12 = a, f13 = b, f14 = 0),
// 0x089A8C24(target) and the virtual +0x114 (target).
void func_08A010D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x3Cu}, {17u, 0x34u}, {31u, 0x40u}, {18u, 0x38u}, {16u, 0x30u}, {53u, 0x54u}, {52u, 0x50u}});
    const u32 member = word(rt, object + 0x10Cu);
    if (member == 0u) return;
    ctx.gpr[2] = 0u;
    const float first = float_at(rt, member + 0x18u);
    const float second = float_at(rt, member + 0x1Cu);
    put_float(rt, frame.at(0x20u), first);
    put_float(rt, frame.at(0x18u), first);
    put_float(rt, frame.at(0x2Cu), second);
    put(rt, frame.at(0x10u), 0u);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put_float(rt, frame.at(0x24u), second);
    put(rt, frame.at(0x28u), 0u);
    const float width = call_float(rt, ctx, 0x089A8B68u, {target}, {});
    const float height = call_float(rt, ctx, 0x089A8B80u, {target}, {});
    call_virtual(rt, ctx, word(rt, word(rt, target) + 0x110u), {target, 2u});
    store8(target + 0x118u, 1u);
    const u32 held = word(rt, object + 0x10Cu);
    const u32 slot = word(rt, target);
    put(rt, target + 0x11Cu, held);
    const u32 callback = word(rt, slot + 0x108u);
    for (u32 i = 0; i < 4u; ++i) put_float(rt, frame.at(4u * i), 1.0f);
    call_virtual(rt, ctx, callback, {target, frame.at(0u), callback});
    call_float(rt, ctx, 0x089A9654u, {target, frame.at(0x10u), 0u}, {width, height, 0.0f});
    call_set(rt, ctx, 0x089A8C24u, {target});
    call_virtual(rt, ctx, word(rt, word(rt, target) + 0x114u), {target});
}
// 0x08B31000 (a0 = the object, a1 = a word stored first in the request, a2 = a data pointer, a3 = its length; v0 = 1 sent, else 0): refuses (0) a length above 0x100, a positive length with
// a null pointer, and when 0x08B2F5AC() answers 0. Builds a 0x1A8-byte request at frame + 0x110 (cleared by 0x08B36018): word +0 = a1, +4 = the length, +8 / +0xC = the words at object + 0xA0 / + 0xA4, 0x80
// bytes from object + 0x1C at +0x18 (0x08B7C830), the data copied to +0xA8 (0x08B79A88, for a positive length). 0x08B2E05C() answers a 64-bit value (v0, v1) stored at request + 0xA0 / +0xA4 and at frame + 8 / +0xC; the
// header (a1, length, that value) at frame + 0 and the data (length bytes, 0x08B79A88 into frame + 0x10) make a second message sent by 0x08B308FC(object, frame, length + 0x10, 3, 1, 0)
// (0 gives 0); then 0x08B2F950(object, request) and 1.
void func_08B31000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 first_word = ctx.gpr[5];
    const u32 data = ctx.gpr[6];
    const u32 length = ctx.gpr[7];
    GuestFrame frame(ctx, 0x2E0u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x2D0u}, {19u, 0x2CCu}, {17u, 0x2C4u}, {16u, 0x2C0u}, {31u, 0x2D8u}, {21u, 0x2D4u}, {18u, 0x2C8u}});
    ctx.gpr[20] = object;
    ctx.gpr[19] = first_word;
    ctx.gpr[17] = data;
    ctx.gpr[16] = length;
    if (length >= 0x101u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const bool positive = static_cast<s32>(length) > 0;
    ctx.gpr[18] = positive ? 1u : 0u;
    if (positive && data == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (call_set(rt, ctx, 0x08B2F5ACu, {}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 request = frame.at(0x110u);
    ctx.gpr[21] = request;
    call_set(rt, ctx, 0x08B36018u, {request, 0u, 0x1A8u});
    put(rt, frame.at(0x110u), first_word);
    const u32 payload = frame.at(0x1B8u);
    ctx.gpr[19] = payload;
    if (positive && data != 0u) call_set(rt, ctx, 0x08B79A88u, {payload, data, length});
    const u32 high = word(rt, object + 0xA4u);
    const u32 low = word(rt, object + 0xA0u);
    ctx.gpr[3] = high;
    put(rt, frame.at(0x11Cu), high);
    put(rt, frame.at(0x114u), length);
    put(rt, frame.at(0x118u), low);
    call_set(rt, ctx, 0x08B7C830u, {frame.at(0x128u), object + 0x1Cu, 0x80u});
    const u32 stamp_low = call_set(rt, ctx, 0x08B2E05Cu, {});
    const u32 stamp_high = ctx.gpr[3];
    const u32 stored_length = word(rt, frame.at(0x114u));
    const u32 stored_first = word(rt, frame.at(0x110u));
    put(rt, frame.at(0u), stored_first);
    put(rt, frame.at(4u), stored_length);
    put(rt, frame.at(8u), stamp_low);
    put(rt, frame.at(0xCu), stamp_high);
    put(rt, frame.at(0x1B0u), stamp_low);
    put(rt, frame.at(0x1B4u), stamp_high);
    call_set(rt, ctx, 0x08B79A88u, {frame.at(0x10u), payload, stored_length});
    const u32 total = word(rt, frame.at(4u)) + 0x10u;
    if (call_set(rt, ctx, 0x08B308FCu, {object, frame.at(0u), total, 3u, 1u, 0u}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08B2F950u, {object, request});
    ctx.gpr[2] = 1u;
}
// 0x08B40DD8 (a0 = the object; v0 = 1 when the list is empty or every record's byte +4 is 0, else 0): walks the list from 0x08B3F3C8(object + 0x8DC) (node: record word at +0, next at +8). A
// record whose byte +4 is non-zero makes the result 0. State (word +0x28) 2: 0x08B3FA3C(record + 0x12, 20000), then the virtual +0x44 of word(object + 0xB0) with (that object, record + 0x12, 1): a
// non-negative answer is stored at +0xC and the state becomes 3, a negative one makes the state 6 with +0xC = -1 and ends the record. State 3 with +0xC non-negative: the virtual +0x48 with (object, +0xC):
// 1 makes the state 4 and copies +0xC to +8, a negative answer makes the state 6 with +0xC = -1, 0 and above 1 leave it.
void func_08B40DD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {18u, 0x8u}, {31u, 0x24u}, {30u, 0x20u}, {23u, 0x1Cu}, {22u, 0x18u}, {21u, 0x14u}, {19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[20] = object;
    u32 node = call_set(rt, ctx, 0x08B3F3C8u, {object + 0x8DCu});
    ctx.gpr[18] = node;
    ctx.gpr[2] = 1u;
    if (node == 0u) return;
    u32 result = 1u;
    ctx.gpr[19] = 1u;
    ctx.gpr[22] = 2u;
    ctx.gpr[30] = 0xFFFFFFFFu;
    ctx.gpr[21] = 3u;
    ctx.gpr[23] = 1u;
    while (node != 0u) {
        const u32 record = word(rt, node);
        ctx.gpr[17] = record;
        if (load8(record + 4u) != 0u) {
            result = 0u;
            ctx.gpr[19] = 0u;
        }
        u32 state = word(rt, record + 0x28u);
        bool ended = false;
        if (state == 2u) {
            const u32 text = record + 0x12u;
            ctx.gpr[16] = text;
            call_set(rt, ctx, 0x08B3FA3Cu, {text, 0x4E20u});
            const u32 holder = word(rt, object + 0xB0u);
            const u32 answer = call_virtual(rt, ctx, word(rt, word(rt, holder) + 0x44u), {holder, text, 1u});
            if (static_cast<s32>(answer) >= 0) {
                put(rt, record + 0xCu, answer);
                put(rt, record + 0x28u, 3u);
                state = 3u;
            } else {
                put(rt, record + 0x28u, 6u);
                put(rt, record + 0xCu, 0xFFFFFFFFu);
                ended = true;
            }
        }
        if (!ended && state == 3u) {
            const u32 position = word(rt, record + 0xCu);
            if (static_cast<s32>(position) >= 0) {
                const u32 holder = word(rt, object + 0xB0u);
                const u32 answer = call_virtual(rt, ctx, word(rt, word(rt, holder) + 0x48u), {holder, position});
                if (answer == 1u) {
                    put(rt, record + 0x28u, 4u);
                    put(rt, record + 8u, word(rt, record + 0xCu));
                } else if (static_cast<s32>(answer) < 0) {
                    put(rt, record + 0xCu, 0xFFFFFFFFu);
                    put(rt, record + 0x28u, 6u);
                }
            }
        }
        node = word(rt, node + 8u);
        ctx.gpr[18] = node;
    }
    ctx.gpr[2] = result;
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_320
} // namespace gt::game

#define HOT320(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_320::name, gt::ReturnKind::kind)
// HOT320(0x08978D3Cu, func_08978D3C, Word);  // no comparable synthetic case yet
HOT320(0x089D15E0u, func_089D15E0, Word);
// HOT320(0x08C35920u, func_08C35920, Word);  // no comparable synthetic case yet
HOT320(0x08959B58u, func_08959B58, Word);
HOT320(0x08B76940u, func_08B76940, Word);
HOT320(0x08C696CCu, func_08C696CC, Word);
// HOT320(0x08C69864u, func_08C69864, Word);  // no comparable synthetic case yet
HOT320(0x089B7CF8u, func_089B7CF8, Word);
HOT320(0x08AA9BECu, func_08AA9BEC, Word);
HOT320(0x08B2D314u, func_08B2D314, Word);
HOT320(0x08C72194u, func_08C72194, Word);
HOT320(0x088F6C78u, func_088F6C78, Word);
HOT320(0x08926AE0u, func_08926AE0, Word);
HOT320(0x0893EEC4u, func_0893EEC4, Word);
HOT320(0x08A265F8u, func_08A265F8, Word);
HOT320(0x08B059D0u, func_08B059D0, Word);
HOT320(0x08C5B850u, func_08C5B850, Word);
HOT320(0x088708E8u, func_088708E8, Word);
HOT320(0x08952400u, func_08952400, Word);
HOT320(0x08A010D0u, func_08A010D0, Word);
HOT320(0x08B31000u, func_08B31000, Word);
HOT320(0x08B40DD8u, func_08B40DD8, Word);
