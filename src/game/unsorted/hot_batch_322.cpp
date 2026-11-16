// Hot batch 322: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
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
namespace hot_batch_322 {
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

void func_088D4014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); auto &F = ctx.fpr; G[19] = G[5]; G[18] = G[4]; G[4] = frame.at(0u); G[17] = G[6]; put_float(rt, frame.at(0x50u), F[20]); call_here(rt, ctx, 0x088D2F9Cu); G[4] = word(rt, G[19] + 0x0u); call_here(rt, ctx, 0x088DB5C4u); G[4] = word(rt, G[19] + 0x0u); G[16] = G[2]; call_here(rt, ctx, 0x088D9F80u); G[4] = G[16]; G[5] = G[19]; G[6] = G[17]; { const bool tk = G[16] != 0u; G[7] = 0xFFFFFFFFu; if (tk) goto L_088D407C; } G[4] = G[18]; call_here(rt, ctx, 0x088D3E98u); G[2] = G[18]; goto L_088D412C; L_088D407C: ; F[0] = std::bit_cast<float>(G[2]); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; F[20] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); if (tk) goto L_088D4094; } G[2] = 0x1u; G[1] = 0x3F800000u; F[20] = std::bit_cast<float>(G[1]); L_088D4094: ; G[3] = G[2] + 0xFFFFFFFFu; G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] == 0u) G[17] = G[3]; call_here(rt, ctx, 0x088FB028u); F[0] = F[0] / F[20]; F[2] = std::bit_cast<float>(G[17]); G[2] = word(rt, G[19] + 0x0u); F[13] = std::bit_cast<float>(0u); F[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[2]))); G[5] = frame.at(0u); G[6] = frame.at(0xCu); G[7] = 0x6u; F[0] = psp_mul(F[0], F[1]); put_float(rt, frame.at(0x18u), F[0]); F[12] = F[0]; G[4] = word(rt, G[2] + 0x61Cu); call_here(rt, ctx, 0x088D390Cu); F[5] = float_at(rt, frame.at(0x18u)); G[4] = word(rt, frame.at(0x1Cu)); F[2] = float_at(rt, frame.at(0x4u)); F[3] = float_at(rt, frame.at(0x8u)); F[4] = float_at(rt, frame.at(0xCu)); F[6] = float_at(rt, frame.at(0x10u)); F[1] = float_at(rt, frame.at(0x14u)); F[0] = float_at(rt, frame.at(0x0u)); G[3] = 0x1u; G[2] = G[18] + 0xCu; put_float(rt, G[18] + 0x0u, F[0]); put_float(rt, G[18] + 0x4u, F[2]); put_float(rt, G[18] + 0x8u, F[3]); put_float(rt, G[18] + 0xCu, F[4]); put_float(rt, G[2] + 0x8u, F[1]); put_float(rt, G[2] + 0x4u, F[6]); store8(G[18] + 0x20u, G[3]); put_float(rt, G[18] + 0x18u, F[5]); put(rt, G[18] + 0x1Cu, G[4]); store8(frame.at(0x20u), G[3]); G[2] = G[18]; L_088D412C: ; F[20] = float_at(rt, frame.at(0x50u)); goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0893B6EC (a0 = the owner, a1 = mode, a2 = a byte stored with the level, f12 = the input): raises the level kept at state + 0x740 (state = word at owner + 8),
// with the mode at +0x748 and the byte a2 at +0x749, when the new level is higher. Mode -1 does nothing (v0 = -1). Level: modes 0 and 1 give f12 / 10000, mode 2 gives
// |f12 / 50| (below 0.2 it becomes 0.2 and skips the clamp and the halving), any other mode 0. The clamp keeps it at most 1 (a level under 0.2 becomes 0). A level
// from a mode that is not 2 (a1 = 0 only, as a flag) is halved when 0x0894D050 (the owner's first word) answers non-zero. v0 = 2 unless that call ran (then its result).
void func_0893B6EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    const u32 byte_arg = ctx.gpr[6];
    const float input = ctx.fpr[12];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {16u, 0x0u}, {31u, 0xCu}, {17u, 0x4u}, {52u, 0x10u}});
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[18] = byte_arg;
    ctx.gpr[16] = mode;
    const u32 state = word(rt, owner + 8u);
    ctx.gpr[17] = state;
    if (mode == 0xFFFFFFFFu) return;
    ctx.gpr[2] = 2u;
    float level = 0.0f;
    bool clamp = true;
    if (mode == 0u || mode == 1u) {
        level = input / 10000.0f; // 0x461C4000
    } else if (mode == 2u) {
        level = input / 50.0f; // 0x42480000
        if (level < 0.0f) level = -level;
        if (level < 0.2f) { // 0x3E4CCCCD
            level = 0.2f;
            clamp = false;
        }
    }
    if (clamp) {
        if (1.0f < level) level = 1.0f;
        else if (level < 0.2f) level = 0.0f;
        if (mode == 0u && call_set(rt, ctx, 0x0894D050u, {word(rt, owner)}) != 0u) level = psp_mul(level, 0.5f);
    }
    if (float_at(rt, state + 0x740u) < level) {
        store8(state + 0x749u, static_cast<std::uint8_t>(byte_arg));
        put_float(rt, state + 0x740u, level);
        store8(state + 0x748u, static_cast<std::uint8_t>(mode));
    }
}
// 0x0894E87C (a0 = the object, a1 = index, a2 = flag (low byte), f12 = x, f13 = y; result in f0, v0 = object + (index >> 1) * 4 for an even index, 1 for an odd one): a mixed value from
// the object's tables of floats. index < 2: x * [+0x1390] times the float at +0x138 + side * 4 (side = index & 1; the other side when x < 0). Otherwise the
// byte +0x146C picks: 1 gives -x * [+0x1468] scaled the same way; 5 with the flag set gives y scaled the same way; anything else (5 with no flag too) starts from 0
// and is not scaled. The offset float [+0x1398 + (index >> 1) * 4] is subtracted for an even index and added for an odd one.
void func_0894E87C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const s32 index = static_cast<s32>(ctx.gpr[5]);
    const u32 flag = ctx.gpr[6] & 0xFFu;
    const float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    ctx.gpr[6] = flag;
    const u32 side = static_cast<u32>(index) & 1u;
    const auto side_scale = [&]() { return float_at(rt, object + 0x138u + 4u * ((0.0f <= x) ? side : 1u - side)); };
    float value = 0.0f;
    if (index < 2) {
        value = psp_mul(psp_mul(x, float_at(rt, object + 0x1390u)), side_scale());
    } else {
        const u32 kind = load8(object + 0x146Cu);
        if (kind == 1u) {
            value = psp_mul(psp_mul(-x, float_at(rt, object + 0x1468u)), side_scale());
        } else if (kind == 5u && flag != 0u) {
            value = psp_mul(y, side_scale());
        }
    }
    const u32 entry = object + ((static_cast<u32>(index >> 1)) << 2);
    const float offset = float_at(rt, entry + 0x1398u);
    if (side == 0u) value = value - offset;
    else value = value + offset;
    ctx.gpr[2] = (side == 0u) ? entry : 1u; // an odd index loads v0 = 1 before the add
    ctx.gpr[7] = side;
    ctx.fpr[1] = value;
    ctx.fpr[0] = value;
}
// 0x08B03170 (a0 = the object, a1 = a point, a2 = a second argument, a3 = optional float out): walks the 0x10-byte entries of the object from +0xE0 for ((word +0x1FC >> 12)
// & 0xF) of them, the entry address in the stack word at frame + 0 (the callees read it there). For each entry 0x08C46364(0x08C45D84(&cursor), a2) gives a value that
// ends the walk with -1 (nothing written) when it is above 0; otherwise 0x08BBBEC4(a1, 0x08C45D7C(&cursor)) is the distance: when it is above 0 and the value is higher than the
// best so far (-FLT_MAX at first) the best and its index are kept. v0 = the best index (-1 when none); *a3, when set, gets the negated best value.
void func_08B03170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 point = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    const u32 out = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {53u, 0x34u}, {31u, 0x28u}, {19u, 0x1Cu}, {54u, 0x38u}, {52u, 0x30u}});
    u32 best_index = 0xFFFFFFFFu;
    float best = std::bit_cast<float>(0xFF7FFFFFu);
    const u32 end = object + 0xE0u + (((word(rt, object + 0x1FCu) >> 12) & 0xFu) << 4);
    u32 cursor = object + 0xE0u;
    put(rt, frame.at(0u), cursor);
    if (cursor != end) {
        u32 index = 0u;
        do {
            const u32 handle = call_set(rt, ctx, 0x08C45D84u, {frame.at(0u)});
            call_set(rt, ctx, 0x08C46364u, {handle, argument});
            const float reach = ctx.fpr[0];
            if (0.0f < reach) {
                ctx.gpr[2] = 0xFFFFFFFFu;
                return;
            }
            const u32 second = call_set(rt, ctx, 0x08C45D7Cu, {frame.at(0u)});
            call_set(rt, ctx, 0x08BBBEC4u, {point, second});
            const float distance = ctx.fpr[0];
            cursor = word(rt, frame.at(0u)) + 0x10u;
            if (!(distance <= 0.0f) && !(reach <= best)) {
                best = reach;
                best_index = index;
            }
            ++index;
            put(rt, frame.at(0u), cursor);
        } while (cursor != end);
    }
    if (out != 0u) put_float(rt, out, -best);
    ctx.gpr[2] = best_index;
}
void func_08B03CE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); auto &F = ctx.fpr; G[9] = G[4]; G[10] = G[5]; G[20] = G[6]; G[2] = sext(load16(G[7] + 0x4u), 16); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[19] = G[8]; if (tk) goto L_08B03DFC; } G[3] = G[2] << 4; G[2] = G[2] << 2; G[5] = word(rt, G[4] + 0x60u); G[3] = G[3] - G[2]; G[7] = word(rt, G[4] + 0x0u); G[2] = G[3] << 3; G[6] = word(rt, frame.at(0x10u)); G[2] = G[2] - G[3]; G[4] = G[10] << 8; G[3] = G[10] << 4; G[5] = G[5] + G[2]; G[4] = G[4] - G[3]; G[17] = G[9] + 0x60u; G[18] = G[7] + G[4]; G[16] = G[5]; put(rt, frame.at(0x30u), G[6]); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x18u), G[5]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[6]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x28u), G[5]); put(rt, frame.at(0x2Cu), 0u); put(rt, frame.at(0x0u), G[6]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[5]); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x38u), G[5]); put(rt, frame.at(0x3Cu), 0u); goto L_08B03DE0; L_08B03D94: ; G[2] = load8(G[16] + 0x50u); G[2] = G[2] < 0x3u ? 1u : 0u; if (G[2] == 0u) { G[5] = sext(load16(G[16] + 0x52u), 16); goto L_08B03DC0; } G[3] = word(rt, G[19] + 0x0u); F[12] = float_at(rt, G[16] + 0x34u); F[13] = float_at(rt, G[16] + 0x24u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; F[14] = float_at(rt, G[16] + 0x2Cu); call_here(rt, ctx, tg); } G[5] = sext(load16(G[16] + 0x52u), 16); L_08B03DC0: ; G[2] = G[5] << 4; G[3] = G[5] << 2; G[2] = G[2] - G[3]; G[4] = G[2] << 3; { const bool tk = static_cast<std::int32_t>(G[5]) < 0; G[4] = G[4] - G[2]; if (tk) goto L_08B03DFC; } G[2] = word(rt, G[17] + 0x0u); G[16] = G[2] + G[4]; L_08B03DE0: ; G[7] = G[16] + 0x18u; G[8] = G[16] + 0xCu; G[9] = G[16]; G[4] = G[19]; G[5] = G[18]; { const bool tk = G[16] != 0u; G[6] = G[20]; if (tk) goto L_08B03D94; } L_08B03DFC: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B047D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); auto &F = ctx.fpr; G[9] = G[4]; G[20] = G[5]; G[10] = G[6]; G[2] = sext(load16(G[7] + 0x4u), 16); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[19] = G[8]; if (tk) goto L_08B048E8; } G[3] = G[2] << 4; G[2] = G[2] << 2; G[5] = word(rt, G[4] + 0x60u); G[3] = G[3] - G[2]; G[7] = word(rt, G[4] + 0x0u); G[2] = G[3] << 3; G[2] = G[2] - G[3]; G[4] = G[6] << 8; G[3] = G[10] << 4; G[6] = word(rt, frame.at(0x10u)); G[5] = G[5] + G[2]; G[4] = G[4] - G[3]; G[17] = G[9] + 0x60u; G[18] = G[7] + G[4]; G[16] = G[5]; put(rt, frame.at(0x30u), G[6]); put(rt, frame.at(0x1Cu), 0u); put(rt, frame.at(0x18u), G[5]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x20u), G[6]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x28u), G[5]); put(rt, frame.at(0x2Cu), 0u); put(rt, frame.at(0x0u), G[6]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[5]); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x38u), G[5]); put(rt, frame.at(0x3Cu), 0u); goto L_08B048CC; L_08B04880: ; G[2] = load8(G[16] + 0x50u); G[2] = G[2] < 0x3u ? 1u : 0u; if (G[2] == 0u) { G[5] = sext(load16(G[16] + 0x52u), 16); goto L_08B048AC; } G[3] = word(rt, G[19] + 0x0u); F[12] = float_at(rt, G[16] + 0x34u); F[13] = float_at(rt, G[16] + 0x24u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; F[14] = float_at(rt, G[16] + 0x2Cu); call_here(rt, ctx, tg); } G[5] = sext(load16(G[16] + 0x52u), 16); L_08B048AC: ; G[2] = G[5] << 4; G[3] = G[5] << 2; G[2] = G[2] - G[3]; G[4] = G[2] << 3; { const bool tk = static_cast<std::int32_t>(G[5]) < 0; G[4] = G[4] - G[2]; if (tk) goto L_08B048E8; } G[2] = word(rt, G[17] + 0x0u); G[16] = G[2] + G[4]; L_08B048CC: ; G[7] = G[16] + 0xCu; G[8] = G[16] + 0x18u; G[9] = G[16]; G[4] = G[19]; G[5] = G[20]; { const bool tk = G[16] != 0u; G[6] = G[18]; if (tk) goto L_08B04880; } L_08B048E8: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x088DEDE0 (a0 = the object, a1 = the slot index, a2 = flag (low byte)): when 0x088DE24C(object, 0, 0) answers non-zero, the slot is the record at
// word(object + 0xC0) + index * 0x2F8 (state word +4). A state-1 slot first asks 0x088DE3F0(object, index): on 0 the slot is released with 0x088D9BBC(object, 0, 0) and
// v0 = 0. Unless the state (re-read) is 3: the callback at object + 0x11C, when set, is called (index, &block) with the block {slot word 0, slot word 0x1C,
// object word 0x128, 0} on the frame; with the flag set and the object at +0x640 set, the virtual +0x2C of the object at +0x148 gets the index and
// 0x088E5FE4(object +0x640, slot word 0x2E4) runs; then 0x088E8184(slot). Last 0x088D9BBC(object, 0, 0); v0 = 1.
void func_088DEDE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x20u}, {18u, 0x18u}});
    ctx.gpr[6] = 0u;
    ctx.gpr[5] = 0u;
    if (call_set(rt, ctx, 0x088DE24Cu, {object, 0u, 0u}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 slot = word(rt, object + 0xC0u) + index * 0x2F8u;
    u32 state = word(rt, slot + 4u);
    if (state == 1u) {
        if (call_set(rt, ctx, 0x088DE3F0u, {object, index}) == 0u) {
            call_set(rt, ctx, 0x088D9BBCu, {object, 0u, 0u});
            ctx.gpr[2] = 0u;
            return;
        }
        state = word(rt, slot + 4u);
    }
    if (state != 3u) {
        const u32 callback = word(rt, object + 0x11Cu);
        if (callback != 0u) {
            put(rt, frame.at(0u), word(rt, slot));
            put(rt, frame.at(8u), word(rt, object + 0x128u));
            put(rt, frame.at(4u), word(rt, slot + 0x1Cu));
            put(rt, frame.at(0xCu), 0u);
            call_virtual(rt, ctx, callback, {index, frame.at(0u), callback});
        }
        if (flag != 0u && word(rt, object + 0x640u) != 0u) {
            const u32 owner = word(rt, object + 0x148u);
            call_virtual(rt, ctx, word(rt, word(rt, owner) + 0x2Cu), {owner, index});
            call_set(rt, ctx, 0x088E5FE4u, {word(rt, object + 0x640u), word(rt, slot + 0x2E4u)});
        }
        call_set(rt, ctx, 0x088E8184u, {slot});
    }
    call_set(rt, ctx, 0x088D9BBCu, {object, 0u, 0u});
    ctx.gpr[2] = 1u;
}
// 0x0893EB68 (a0 = the object, a1 = the float out; result in f0, v0 as left): with state = word(object + 8): when the floats at state + 0x538 or state + 0x5EC are 0
// *out = 1.0 and the result is 0 (v0 = state). Otherwise the side s = byte(word(object + 0xC) + 0x272) * 2 and its pair s ^ 1 index two 0x08955300(object, n)
// records: r = (value +0x34 of both added) / 2 (made positive) goes to *out as r * the constant 0x3C449BA5 + the constant 0x3E99999A, and the result is the
// sum of the two +0x84 values times the constant 0x3E4CCCCD.
void func_0893EB68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {17u, 0x4u}, {53u, 0x24u}, {52u, 0x20u}, {31u, 0x10u}, {19u, 0xCu}, {16u, 0x0u}, {54u, 0x28u}});
    const u32 state = word(rt, object + 8u);
    ctx.gpr[2] = state;
    if (float_at(rt, state + 0x538u) == 0.0f || float_at(rt, state + 0x5ECu) == 0.0f) {
        put_float(rt, out, 1.0f);
        ctx.fpr[0] = 0.0f;
        return;
    }
    const u32 side = static_cast<u32>(load8(word(rt, object + 0xCu) + 0x272u)) << 1;
    const u32 pair = side ^ 1u;
    call_set(rt, ctx, 0x08955300u, {object, side});
    const u32 first = ctx.gpr[2];
    call_set(rt, ctx, 0x08955300u, {object, pair});
    float sum = float_at(rt, first + 0x34u) + float_at(rt, ctx.gpr[2] + 0x34u);
    float half = psp_mul(sum, 0.5f);
    if (half < 0.0f) half = -half;
    call_set(rt, ctx, 0x08955300u, {object, side});
    const u32 third = ctx.gpr[2];
    call_set(rt, ctx, 0x08955300u, {object, pair});
    const float total = float_at(rt, third + 0x84u) + float_at(rt, ctx.gpr[2] + 0x84u);
    const float blend = psp_mul(half, std::bit_cast<float>(0x3C449BA5u)) + std::bit_cast<float>(0x3E99999Au);
    const float result = psp_mul(total, std::bit_cast<float>(0x3E4CCCCDu));
    put_float(rt, out, blend);
    ctx.fpr[0] = result;
}
// The two converters 0x0896DD54 and 0x0896DE90 (a0 = the destination object, a1 = the source, a2 = mode, a3 = extra) differ only in the two callees they
// run on a vector (`mode_0_call`, `mode_1_call`). Mode 0 builds a handle holder at frame + 8 with 0x08969244(holder, source...) (the string 0x08CA2880 reports a
// missing handle through 0x08A5BB7C), unpacks four floats into frame + 0x20 with `mode_0_call(handle, +0x20, +0x24, +0x28, +0x2C)`, releases the holder (0x08A5B9EC),
// copies the vector to frame + 0x10, rebuilds it into a temporary at frame + 4 with 0x08BE9708 and assigns it over the destination (0x08A5BAD8) unless it is that
// temporary, then releases it. Mode 1 builds the vector at frame + 0x20 with 0x08BE9114(vector, extra), makes the holder at frame + 0 with 0x08969244(holder, source) and calls
// `mode_1_call(handle, vector, +0x24, +0x28, +0x2C)`, then releases the holder. Any other mode returns v0 = 1; v0 otherwise is the last callee's.
namespace {
void convert_vector(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 mode_0_call, u32 mode_1_call) {
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 extra = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x34u}, {16u, 0x30u}, {31u, 0x3Cu}, {18u, 0x38u}});
    if (mode == 0u) {
        const u32 holder = frame.at(8u);
        put(rt, frame.at(0x20u), 0u);
        put(rt, frame.at(0x24u), 0u);
        put(rt, frame.at(0x28u), 0u);
        put(rt, frame.at(0x2Cu), 0u);
        call_set(rt, ctx, 0x08969244u, {holder});
        const u32 handle = word(rt, holder);
        if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2880u});
        call_set(rt, ctx, mode_0_call, {handle, frame.at(0x20u), frame.at(0x24u), frame.at(0x28u), frame.at(0x2Cu)});
        call_set(rt, ctx, 0x08A5B9ECu, {holder});
        const u32 temporary = frame.at(4u);
        put(rt, frame.at(0x10u), word(rt, frame.at(0x20u)));
        put(rt, frame.at(0x14u), word(rt, frame.at(0x24u)));
        put(rt, frame.at(0x18u), word(rt, frame.at(0x28u)));
        put(rt, frame.at(0x1Cu), word(rt, frame.at(0x2Cu)));
        call_set(rt, ctx, 0x08BE9708u, {temporary, frame.at(0x10u)});
        ctx.gpr[5] = word(rt, temporary);
        if (destination != temporary) call_set(rt, ctx, 0x08A5BAD8u, {destination, ctx.gpr[5]});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    } else if (mode == 1u) {
        const u32 vector = frame.at(0x20u);
        call_set(rt, ctx, 0x08BE9114u, {vector, extra});
        call_set(rt, ctx, 0x08969244u, {frame.at(0u), source});
        const u32 handle = word(rt, frame.at(0u));
        if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2880u});
        call_set(rt, ctx, mode_1_call, {handle, vector, frame.at(0x24u), frame.at(0x28u), frame.at(0x2Cu)});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    } else {
        ctx.gpr[2] = 1u;
    }
}
} // namespace
void func_0896DD54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_vector(rt, ctx, 0x0896A35Cu, 0x0896A9C4u); }
// Same shape as 0x0896DD54 (convert_vector), with the callees 0x08969B98 (mode 0) and 0x08969D2C (mode 1).
void func_0896DE90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_vector(rt, ctx, 0x08969B98u, 0x08969D2Cu); }
void func_08C1C8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { G[2] = G[7] < 0x3u ? 1u : 0u; G[10] = G[7]; G[9] = G[5]; G[8] = G[6]; G[11] = 0u; { const bool tk = G[2] != 0u; G[13] = 0u; if (tk) goto L_08C1C994; } G[12] = 0x08CE0000u; L_08C1C910: ; G[3] = load8(G[8] + 0x0u); G[2] = load8(G[8] + 0x1u); G[5] = load8(G[8] + 0x2u); G[3] = G[3] << 16; G[2] = G[2] << 8; G[4] = word(rt, 0x08CE1298u); G[3] = G[3] | G[2]; G[11] = G[3] | G[5]; G[2] = G[11] >> 18; G[2] = G[2] + G[4]; G[3] = load8(G[2] + 0x0u); G[4] = (G[11] >> 12) & 0x3Fu; G[5] = (G[11] >> 6) & 0x3Fu; store8(G[9] + 0x0u, G[3]); G[6] = G[11] & 0x3Fu; G[10] = G[10] + 0xFFFFFFFDu; G[2] = word(rt, 0x08CE1298u); G[7] = G[10] < 0x3u ? 1u : 0u; G[8] = G[8] + 0x3u; G[4] = G[4] + G[2]; G[3] = load8(G[4] + 0x0u); G[13] = G[13] + 0x4u; store8(G[9] + 0x1u, G[3]); G[2] = word(rt, 0x08CE1298u); G[5] = G[5] + G[2]; G[3] = load8(G[5] + 0x0u); store8(G[9] + 0x2u, G[3]); G[2] = word(rt, 0x08CE1298u); G[6] = G[6] + G[2]; G[3] = load8(G[6] + 0x0u); store8(G[9] + 0x3u, G[3]); { const bool tk = G[7] == 0u; G[9] = G[9] + 0x4u; if (tk) goto L_08C1C910; } L_08C1C994: ; { const bool tk = G[10] == 0u; G[2] = 0x2u; if (tk) goto L_08C1C9A8; } G[2] = sext(load8(G[8] + 0x0u), 8); G[11] = G[2] << 16; G[2] = 0x2u; L_08C1C9A8: ; { const bool tk = G[10] != G[2]; if (tk) goto L_08C1C9C0; } G[2] = sext(load8(G[8] + 0x1u), 8); G[2] = G[2] << 8; G[11] = G[11] | G[2]; goto L_08C1C9C8; L_08C1C9C0: ; { const bool tk = G[10] == 0u; if (tk) goto L_08C1CA24; } L_08C1C9C8: ; G[7] = 0x08CE0000u; G[5] = word(rt, 0x08CE1298u); G[2] = (G[11] >> 18) & 0x3Fu; G[4] = (G[11] >> 12) & 0x3Fu; G[2] = G[2] + G[5]; G[3] = load8(G[2] + 0x0u); G[8] = G[9] + 0x2u; G[5] = 0x3Du; store8(G[9] + 0x0u, G[3]); G[2] = word(rt, 0x08CE1298u); G[4] = G[4] + G[2]; G[3] = load8(G[4] + 0x0u); G[2] = 0x2u; { const bool tk = G[10] != G[2]; store8(G[9] + 0x1u, G[3]); if (tk) goto L_08C1CA14; } G[3] = word(rt, 0x08CE1298u); G[2] = (G[11] >> 6) & 0x3Fu; G[2] = G[2] + G[3]; G[5] = sext(load8(G[2] + 0x0u), 8); L_08C1CA14: ; G[2] = 0x3Du; store8(G[8] + 0x0u, G[5]); G[13] = G[13] + 0x4u; store8(G[9] + 0x3u, G[2]); L_08C1CA24: ; G[2] = G[13]; goto exit_;  exit_: ; } if (tail != 0u) jump(ctx, tail); } }
// 0x08C71F74 (a0 = the stream object (iostream-like virtual base at its vtable[-3]), a1 = a value; v0 = a0): after 0x08C70314(&sentry, stream) the byte at frame + 0
// says whether the stream was ready; the second frame word is the stream the sentry works on. Ready with a value: 0x08B6065C(value, [base + 0x78]) = 0 sets error
// bits 4; ready without one, or not ready with none, sets bit 1; both go through 0x08C54CFC(base, bits | [base + 0x14]). When the flag word at base + 0xC has
// the bit 0x2000 and 0x08B62318() answers 0, the virtual +0x18 of the object at [sentry stream base + 0x78] runs and an answer of -1 sets bit 1 the same way.
void func_08C71F74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x18u}});
    const auto virtual_base = [&](u32 object) { return object + word(rt, word(rt, object) - 0xCu); };
    call_set(rt, ctx, 0x08C70314u, {frame.at(0u), stream});
    const bool ready = load8(frame.at(0u)) != 0u;
    u32 error_bits = 0u;
    bool failed = false;
    if (ready) {
        if (value == 0u) {
            error_bits = 1u;
            failed = true;
        } else if (call_set(rt, ctx, 0x08B6065Cu, {value, word(rt, virtual_base(stream) + 0x78u)}) == 0u) {
            error_bits = 4u;
            failed = true;
        }
    } else if (value == 0u) {
        error_bits = 1u;
        failed = true;
    }
    ctx.gpr[2] = stream;
    if (failed) {
        const u32 base = virtual_base(stream);
        call_set(rt, ctx, 0x08C54CFCu, {base, error_bits | word(rt, base + 0x14u)});
        ctx.gpr[2] = stream;
    }
    const u32 sentry_stream = word(rt, frame.at(4u));
    if ((word(rt, virtual_base(sentry_stream) + 0xCu) & 0x2000u) == 0u) return;
    if (call_set(rt, ctx, 0x08B62318u, {}) != 0u) {
        ctx.gpr[2] = stream;
        return;
    }
    const u32 target = word(rt, virtual_base(word(rt, frame.at(4u))) + 0x78u);
    if (target == 0u) {
        ctx.gpr[2] = stream;
        return;
    }
    if (call_virtual(rt, ctx, word(rt, word(rt, target) + 0x18u), {target}) == 0xFFFFFFFFu) {
        const u32 base = virtual_base(word(rt, frame.at(4u)));
        call_set(rt, ctx, 0x08C54CFCu, {base, word(rt, base + 0x14u) | 1u});
    }
    ctx.gpr[2] = stream;
}
// 0x089E2210 (a0 = the object; v0 = 1): constructs the object after the base constructor 0x08A08C20: vtables 0x08CBC4E0 (+0) and 0x08CBC71C (+0x38); the word
// +0x1B0 loses its bits 3-5 and 7-9 and gets 0x46; the words +0x1A4 / +0x1A8 / +0x1AC point at 0x08D74FDC; the block at +0x1C0 gets 0, 0, 0, 1.0 and -1 (words +0x1C0,
// +0x1C4, +0x1C8, +0x1CC, +0x1D0); +0x1E0 / +0x1E4 clear; the bytes +0x208 / +0x209 become 1 and +0x20A / +0x20B 0; the five string members +0x20C..+0x220 are each
// set from the constant 0x08CF6ED8 by 0x089E4C14; then the floats +0x204, +0x1F0, +0x1F8, +0x1FC become 1.0, the word +0x224 1 and +0x1E8, +0x1EC, +0x1F4, +0x200, +0x228 clear.
void func_089E2210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    call_set(rt, ctx, 0x08A08C20u, {self});
    put(rt, self + 0x0u, 0x08CBC4E0u);
    u32 flags = word(rt, self + 0x1B0u);
    flags &= ~(7u << 3);
    flags |= 0x46u;
    flags &= ~(7u << 7);
    put(rt, self + 0x38u, 0x08CBC71Cu);
    const u32 shared = 0x08D74FD0u + 0xCu;
    const u32 block = self + 0x1C0u;
    put(rt, self + 0x1ACu, shared);
    put(rt, self + 0x1B0u, flags);
    put(rt, self + 0x1A4u, shared);
    put(rt, self + 0x1A8u, shared);
    put(rt, self + 0x1C0u, 0u);
    put(rt, block + 0x10u, 0xFFFFFFFFu);
    put_float(rt, block + 0xCu, 1.0f);
    put(rt, block + 0x4u, 0u);
    put(rt, block + 0x8u, 0u);
    put(rt, self + 0x1E0u, 0u);
    put(rt, self + 0x1E4u, 0u);
    ctx.gpr[6] = 0xFFFFFFFFu; // the empty delay loop leaves a2 = -1
    store8(self + 0x209u, 1u);
    store8(self + 0x208u, 1u);
    store8(self + 0x20Au, 0u);
    store8(self + 0x20Bu, 0u);
    for (u32 member = 0x20Cu; member <= 0x220u; member += 4u) call_set(rt, ctx, 0x089E4C14u, {self + member, 0x08CF6ED8u});
    put_float(rt, self + 0x204u, 1.0f);
    put(rt, self + 0x224u, 1u);
    put(rt, self + 0x228u, 0u);
    put(rt, self + 0x1E8u, 0u);
    put(rt, self + 0x1ECu, 0u);
    put_float(rt, self + 0x1F0u, 1.0f);
    put(rt, self + 0x1F4u, 0u);
    put_float(rt, self + 0x1F8u, 1.0f);
    put_float(rt, self + 0x1FCu, 1.0f);
    put(rt, self + 0x200u, 0u);
    ctx.gpr[2] = 1u;
}
void func_08B42710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x550u); put(rt, frame.at(0x53Cu), G[19]); put(rt, frame.at(0x538u), G[18]); put(rt, frame.at(0x534u), G[17]); put(rt, frame.at(0x530u), G[16]); put(rt, frame.at(0x540u), G[31]); G[17] = G[4]; G[4] = word(rt, G[4] + 0xB0u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x1Cu); { const std::uint32_t tg = G[2]; G[16] = G[5] & 0xFFu; call_here(rt, ctx, tg); } G[6] = word(rt, G[17] + 0x918u); G[7] = word(rt, G[17] + 0x91Cu); G[18] = G[2]; G[2] = G[2] - G[6]; G[5] = G[18] < G[2] ? 1u : 0u; G[19] = G[3]; G[3] = G[3] - G[7]; G[3] = G[3] - G[5]; G[6] = sra(G[3], 31); G[2] = G[6] ^ G[2]; G[3] = G[6] ^ G[3]; G[8] = G[2] - G[6]; G[5] = G[2] < G[8] ? 1u : 0u; G[9] = G[3] - G[6]; G[4] = frame.at(0u); { const bool tk = G[16] != 0u; G[9] = G[9] - G[5]; if (tk) goto L_08B427C8; } { const bool tk = static_cast<std::int32_t>(G[9]) > 0; if (tk) goto L_08B427C8; } { const bool tk = G[9] != 0u; G[2] = G[8] < 0x4E21u ? 1u : 0u; if (tk) goto L_08B4279C; } { const bool tk = G[2] == 0u; if (tk) goto L_08B427C8; } L_08B4279C: ; G[2] = word(rt, G[17] + 0x930u); G[3] = 0x64u; G[5] = word(rt, G[2] + 0x644u); { const bool tk = G[5] == G[3]; if (tk) goto L_08B42838; } { const bool tk = static_cast<std::int32_t>(G[9]) > 0; if (tk) goto L_08B427C8; } { const bool tk = G[9] != 0u; G[2] = G[8] < 0xBB9u ? 1u : 0u; if (tk) goto L_08B42838; } if (G[2] != 0u) { goto L_08B4283C; } L_08B427C8: ; call_here(rt, ctx, 0x08B47AB8u); G[2] = 0x2u; G[5] = 0x10u; G[4] = frame.at(0u); put(rt, frame.at(0x14u), G[2]); call_here(rt, ctx, 0x08B47AD4u); G[5] = 0u; G[4] = frame.at(0u); call_here(rt, ctx, 0x08B47B40u); put(rt, G[2] + 0x8u, G[18]); put(rt, G[2] + 0xCu, G[19]); put(rt, G[2] + 0x0u, 0u); put(rt, G[2] + 0x4u, 0u); G[3] = word(rt, G[17] + 0x0u); put(rt, G[17] + 0x918u, G[18]); put(rt, G[17] + 0x91Cu, G[19]); G[2] = word(rt, G[3] + 0x94u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[5] = G[2]; G[4] = G[17]; G[6] = frame.at(0u); G[7] = 0x1u; call_here(rt, ctx, 0x08B426A8u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08B47ACCu); L_08B42838: ; L_08B4283C: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x53Cu)); G[18] = word(rt, frame.at(0x538u)); G[17] = word(rt, frame.at(0x534u)); G[16] = word(rt, frame.at(0x530u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B58B5C (a0 = the object; v0 = the flag word after the update): mixes the floats +0xC8, +0xCC, +0x138 and +0x13C: with a = [+0xC8] * [+0x138], b = [+0xC8] * [+0x13C] and
// c = [+0xC8] * [+0xCC], the words +0x178 = b - a, +0x118 = (b - a) * k (k = the constant 0x08CB61F0), +0x128 = c + (a + b) * k and +0x188 = a + c. The limits [+0x138] and
// [+0x13C], clamped to [0, the constant 0x08CB61F4] and scaled by [+0xC8], are turned into fixed-point words (the constant 0x08CB61F8 or more is cut by it and sets bit 31
// of the word); the smaller goes to the halfword +0xD6 and the larger to +0xD8. The flag word +0x6C gets bit 0x20.
void func_08B58B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float scale = float_at(rt, self + 0xC8u);
    const float lower = float_at(rt, self + 0x138u);
    const float upper = float_at(rt, self + 0x13Cu);
    const float k = float_at(rt, 0x08CB61F0u);
    const float a = psp_mul(scale, lower);
    const float b = psp_mul(scale, upper);
    const float c = psp_mul(scale, float_at(rt, self + 0xCCu));
    const float difference = b - a;
    put_float(rt, self + 0x178u, difference);
    put_float(rt, self + 0x118u, psp_mul(difference, k));
    put_float(rt, self + 0x128u, c + psp_mul(a + b, k));
    put_float(rt, self + 0x188u, a + c);
    const float limit = float_at(rt, 0x08CB61F4u);
    const float wrap = float_at(rt, 0x08CB61F8u);
    const auto fixed_point = [&](float value) {
        float scaled = psp_mul(value, scale);
        if (wrap <= scaled) return trunc_to_word(scaled - wrap) | 0x80000000u;
        return trunc_to_word(scaled);
    };
    float low = lower;
    if (lower < 0.0f) low = 0.0f;
    else if (limit < lower) low = limit;
    const u32 first = fixed_point(low);
    float high = 0.0f;
    if (!(upper < 0.0f)) high = (limit < upper) ? limit : upper;
    const u32 second = fixed_point(high);
    if (first < second) {
        store16(self + 0xD6u, static_cast<std::uint16_t>(first));
        store16(self + 0xD8u, static_cast<std::uint16_t>(second));
    } else {
        store16(self + 0xD6u, static_cast<std::uint16_t>(second));
        store16(self + 0xD8u, static_cast<std::uint16_t>(first));
    }
    const u32 flags = word(rt, self + 0x6Cu) | 0x20u;
    put(rt, self + 0x6Cu, flags);
    ctx.gpr[2] = flags;
}
// 0x08B81874 (a0 = the context, a1 = the running value, a2 = the exponent; v0 = the value): applies an operation n times by repeated squaring. The low two bits of the
// exponent (when non-zero) pick a word of the table at 0x08C97708 (index bits - 1) for 0x08B81378(context, value, word, 0). The rest (exponent >> 2) walks a chain of
// nodes from the one at context + 0x48 (created on demand with 0x08B815FC(context, 0x271) and a zero first word): each set bit combines the value with the node through
// 0x08B81630(context, value, node) and runs 0x08B82318(context, old value); between bits the next node (word 0 of the node, created with 0x08B81630(context, node, node)
// and a zero first word) stands for the square.
void func_08B81874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 context = ctx.gpr[4];
    u32 value = ctx.gpr[5];
    const u32 exponent = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {16u, 0x0u}, {31u, 0x14u}, {18u, 0x8u}, {17u, 0x4u}});
    const u32 low = exponent & 3u;
    if (low != 0u) value = call_set(rt, ctx, 0x08B81378u, {context, value, word(rt, 0x08C97708u + (low << 2) - 4u), 0u});
    s32 rest = static_cast<s32>(exponent) >> 2;
    ctx.gpr[2] = value;
    if (rest == 0) return;
    u32 node = word(rt, context + 0x48u);
    if (node == 0u) {
        node = call_set(rt, ctx, 0x08B815FCu, {context, 0x271u});
        put(rt, context + 0x48u, node);
        put(rt, node, 0u);
    }
    for (;;) {
        const bool bit = (rest & 1) != 0;
        rest >>= 1;
        if (bit) {
            const u32 combined = call_set(rt, ctx, 0x08B81630u, {context, value, node});
            call_set(rt, ctx, 0x08B82318u, {context, value});
            value = combined;
        }
        if (rest == 0) break;
        ctx.gpr[4] = context;
        ctx.gpr[5] = node;
        ctx.gpr[6] = node;
        u32 next = word(rt, node);
        if (next == 0u) {
            next = call_set(rt, ctx, 0x08B81630u, {context, node, node});
            put(rt, node, next);
            put(rt, next, 0u);
        }
        node = next;
    }
    ctx.gpr[2] = value;
}
void func_08C64BB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x58u), G[31]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x40u), G[16]); G[2] = 0x08D70000u; G[21] = 0x08D77498u; G[2] = G[21] + 0xCu; G[7] = G[7] & 0xFFu; G[20] = G[10]; G[17] = G[9]; G[3] = word(rt, G[5] + 0x4u); G[11] = word(rt, G[5] + 0x0u); G[12] = word(rt, G[6] + 0x0u); G[5] = word(rt, G[6] + 0x4u); put(rt, frame.at(0x1Cu), G[11]); put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x18u), G[5]); put(rt, frame.at(0x14u), G[12]); { const bool tk = G[7] == 0u; put(rt, frame.at(0x8u), G[2]); if (tk) goto L_08C64CAC; } G[7] = G[8]; G[5] = frame.at(0x1Cu); G[8] = G[9]; G[6] = frame.at(0x14u); G[9] = frame.at(0x8u); call_here(rt, ctx, 0x08C637BCu); put(rt, frame.at(0x1Cu), G[2]); L_08C64C30: ; put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x30u), G[2]); put(rt, frame.at(0x34u), G[3]); G[16] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08B5C230u); put(rt, frame.at(0x4u), G[2]); G[4] = G[16]; G[5] = G[20]; G[6] = G[17]; G[7] = frame.at(0x4u); call_here(rt, ctx, 0x08B61A80u); G[4] = word(rt, frame.at(0x8u)); G[2] = word(rt, frame.at(0x1Cu)); G[3] = word(rt, frame.at(0x20u)); G[16] = G[4] + 0xFFFFFFF4u; put(rt, frame.at(0xCu), G[2]); { const bool tk = G[21] != G[16]; put(rt, frame.at(0x10u), G[3]); if (tk) goto L_08C64CCC; } G[18] = word(rt, frame.at(0xCu)); L_08C64C7C: ; G[19] = word(rt, frame.at(0x10u)); G[2] = G[18]; G[3] = G[19]; goto exit_; L_08C64CAC: ; G[7] = G[8]; G[5] = frame.at(0x1Cu); G[8] = G[9]; G[6] = frame.at(0x14u); G[9] = frame.at(0x8u); call_here(rt, ctx, 0x08C61C54u); put(rt, frame.at(0x1Cu), G[2]); goto L_08C64C30; L_08C64CCC: ; G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[18] = word(rt, frame.at(0xCu)); goto L_08C64C7C; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C74884u); G[18] = word(rt, frame.at(0xCu)); goto L_08C64C7C;  exit_: ; G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[17] = word(rt, frame.at(0x44u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0886A13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x420u); put(rt, frame.at(0x40Cu), G[19]); put(rt, frame.at(0x400u), G[16]); put(rt, frame.at(0x418u), G[22]); put(rt, frame.at(0x414u), G[21]); put(rt, frame.at(0x408u), G[18]); put(rt, frame.at(0x41Cu), G[31]); put(rt, frame.at(0x410u), G[20]); put(rt, frame.at(0x404u), G[17]); G[19] = G[4] + 0x8u; G[16] = G[4]; G[4] = G[19]; G[22] = 0x1u; G[21] = G[16] + 0x4210u; G[18] = G[16] + 0xCu; call_here(rt, ctx, 0x08875088u); L_0886A17C: ; G[4] = G[19]; call_here(rt, ctx, 0x08875090u); { const bool tk = G[2] != 0u; G[2] = G[22]; if (tk) goto L_0886A258; } G[4] = G[21]; call_here(rt, ctx, 0x08B344E0u); G[16] = load16(G[18] + 0x4202u); G[2] = load16(G[18] + 0x4200u); G[17] = 0xFFFFFFFFu; { const bool tk = G[2] == G[16]; G[20] = 0u; if (tk) goto L_0886A1E0; } G[2] = G[16] << 7; G[16] = G[16] << 2; G[16] = G[16] + G[2]; G[16] = G[18] + G[16]; G[4] = frame.at(0u); G[5] = G[16] + 0x5u; G[6] = 0x7Eu; call_here(rt, ctx, 0x08B7C830u); G[3] = load16(G[18] + 0x4202u); G[20] = word(rt, G[16] + 0x0u); G[3] = G[3] + 0x1u; G[3] = G[3] & 0x7Fu; store16(G[18] + 0x4202u, G[3]); G[17] = sext(load8(G[16] + 0x4u), 8); L_0886A1E0: ; G[4] = G[21]; call_here(rt, ctx, 0x08B3457Cu); { const bool tk = static_cast<std::int32_t>(G[17]) < 0; G[5] = G[20]; if (tk) goto L_0886A254; } G[4] = G[19]; call_here(rt, ctx, 0x08875108u); { const bool tk = G[17] == 0u; G[4] = G[19]; if (tk) goto L_0886A214; } G[2] = 0x1u; { const bool tk = G[17] != G[2]; if (tk) goto L_0886A244; } goto L_0886A22C; L_0886A214: ; G[5] = frame.at(0u); call_here(rt, ctx, 0x088750ACu); if (G[2] == 0u) { G[22] = 0u; goto L_0886A240; } G[4] = G[19]; goto L_0886A244; L_0886A22C: ; G[5] = frame.at(0u); call_here(rt, ctx, 0x088750C8u); if (G[2] != 0u) { G[4] = G[19]; goto L_0886A244; } G[22] = 0u; L_0886A240: ; G[4] = G[19]; L_0886A244: ; G[5] = 0u; call_here(rt, ctx, 0x08875108u); goto L_0886A17C; L_0886A254: ; G[2] = G[22]; L_0886A258: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x40Cu)); G[16] = word(rt, frame.at(0x400u)); G[22] = word(rt, frame.at(0x418u)); G[21] = word(rt, frame.at(0x414u)); G[18] = word(rt, frame.at(0x408u)); G[20] = word(rt, frame.at(0x410u)); G[17] = word(rt, frame.at(0x404u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088A8710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x4Cu), G[31]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x48u), G[18]); auto &F = ctx.fpr; G[2] = G[4]; G[17] = frame.at(0x20u); put_float(rt, frame.at(0x50u), F[20]); G[3] = word(rt, G[4] + 0x4u); put(rt, frame.at(0x30u), G[5]); G[4] = frame.at(0x10u); G[5] = G[3] + 0x34u; F[0] = float_at(rt, G[3] + 0x34u); G[18] = word(rt, G[2] + 0x8u); F[12] = float_at(rt, G[2] + 0xCu); put_float(rt, frame.at(0x10u), F[0]); G[16] = G[18] + 0x34u; F[1] = float_at(rt, G[5] + 0x4u); F[20] = float_at(rt, G[2] + 0x10u); put_float(rt, frame.at(0x14u), F[1]); F[0] = float_at(rt, G[5] + 0x8u); put_float(rt, frame.at(0x18u), F[0]); F[1] = float_at(rt, G[5] + 0xCu); put_float(rt, frame.at(0x1Cu), F[1]); call_here(rt, ctx, 0x08B4A6A0u); F[12] = F[20]; F[1] = float_at(rt, G[18] + 0x34u); G[4] = frame.at(0u); put_float(rt, frame.at(0x0u), F[1]); F[0] = float_at(rt, G[16] + 0x4u); put_float(rt, frame.at(0x4u), F[0]); F[1] = float_at(rt, G[16] + 0x8u); put_float(rt, frame.at(0x8u), F[1]); F[0] = float_at(rt, G[16] + 0xCu); put_float(rt, frame.at(0xCu), F[0]); call_here(rt, ctx, 0x08B4A6A0u); G[4] = G[17]; F[0] = float_at(rt, frame.at(0x10u)); F[1] = float_at(rt, frame.at(0x14u)); G[5] = G[17]; put_float(rt, frame.at(0x20u), F[0]); G[6] = frame.at(0u); F[0] = float_at(rt, frame.at(0x18u)); put_float(rt, frame.at(0x24u), F[1]); F[1] = float_at(rt, frame.at(0x1Cu)); put_float(rt, frame.at(0x28u), F[0]); put_float(rt, frame.at(0x2Cu), F[1]); call_here(rt, ctx, 0x08B4A890u); G[5] = G[17]; G[4] = frame.at(0x30u); call_here(rt, ctx, 0x08BADAA8u); G[3] = word(rt, frame.at(0x30u)); F[1] = std::bit_cast<float>(0u); F[0] = float_at(rt, G[3] + 0x0u); ctx.set_fpu_condition(F[0] == F[1]); { const bool tk = !ctx.fpu_condition(); G[2] = 0x1u; if (tk) goto L_088A8838; } F[0] = float_at(rt, G[3] + 0x4u); ctx.set_fpu_condition(F[0] == F[1]); { const bool tk = !ctx.fpu_condition(); if (tk) goto L_088A883C; } F[0] = float_at(rt, G[3] + 0x8u); ctx.set_fpu_condition(F[0] == F[1]); { const bool tk = !ctx.fpu_condition(); G[2] = 0u; if (tk) goto L_088A8834; } F[0] = float_at(rt, G[3] + 0xCu); ctx.set_fpu_condition(F[0] == F[1]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_088A8840; } L_088A8834: ; G[2] = 0x1u; L_088A8838: ; L_088A883C: ; L_088A8840: ; F[20] = float_at(rt, frame.at(0x50u)); goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); G[18] = word(rt, frame.at(0x48u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088DF4F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[20] = word(rt, G[4] + 0x0u); G[18] = word(rt, G[4] + 0x4u); G[3] = G[20] << 5; G[2] = G[20] << 7; G[2] = G[2] - G[3]; G[4] = word(rt, G[18] + 0xC0u); G[2] = G[2] - G[20]; G[2] = G[2] << 3; G[19] = G[4] + G[2]; G[3] = load8(G[19] + 0x11u); { const bool tk = G[3] == 0u; G[17] = G[18] + 0xF0u; if (tk) goto L_088DF588; } L_088DF544: ; call_here(rt, ctx, 0x08B1A844u); { const bool tk = G[2] != 0u; G[4] = 0x1F4u; if (tk) goto L_088DF61C; } call_here(rt, ctx, 0x08B2E2B8u); G[2] = load8(G[19] + 0x11u); { const bool tk = G[2] != 0u; if (tk) goto L_088DF544; } G[17] = G[18] + 0xF0u; goto L_088DF588; L_088DF570: ; call_here(rt, ctx, 0x08B1A844u); { const bool tk = G[2] != 0u; G[4] = 0x1F4u; if (tk) goto L_088DF61C; } call_here(rt, ctx, 0x08B2E2B8u); L_088DF588: ; G[4] = G[17]; call_here(rt, ctx, 0x08B3453Cu); { const bool tk = G[2] == 0u; G[5] = G[20]; if (tk) goto L_088DF570; } G[4] = word(rt, G[18] + 0x148u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x2Cu); { const std::uint32_t tg = G[2]; G[16] = G[18] + 0xC8u; call_here(rt, ctx, tg); } G[4] = G[16]; call_here(rt, ctx, 0x08B344E0u); G[2] = 0x3u; put(rt, G[19] + 0x2C8u, G[2]); G[4] = G[18]; G[6] = word(rt, G[18] + 0x90u); G[7] = word(rt, G[18] + 0x94u); call_here(rt, ctx, 0x088D9C24u); put(rt, G[19] + 0x2D0u, G[2]); G[4] = G[16]; put(rt, G[19] + 0x2D4u, G[3]); store8(G[19] + 0x2B4u, 0u); call_here(rt, ctx, 0x08B3457Cu); G[4] = word(rt, G[18] + 0x14Cu); if (G[4] == 0u) { G[4] = G[17]; goto L_088DF5FC; } G[5] = G[20]; call_here(rt, ctx, 0x088F8A20u); G[4] = G[17]; L_088DF5FC: ; tail = 0x08B3457Cu; goto exit_; L_088DF61C: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0894E738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); auto &F = ctx.fpr; put_float(rt, frame.at(0x28u), F[22]); F[22] = std::bit_cast<float>(0u); G[2] = 0x08CE0000u; G[21] = 0x08CDE508u; G[20] = 0x4u; G[19] = G[4]; G[18] = 0u; put_float(rt, frame.at(0x2Cu), F[23]); G[1] = 0x40490000u; G[1] = G[1] | 0xFDBu; F[23] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x24u), F[21]); G[1] = 0x3E8E0000u; G[1] = G[1] | 0x38E4u; F[21] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x20u), F[20]); G[4] = G[19]; L_0894E79C: ; G[5] = G[18]; call_here(rt, ctx, 0x08955300u); G[4] = G[19]; G[5] = G[18]; G[16] = G[2]; call_here(rt, ctx, 0x089554A8u); G[17] = G[2]; G[2] = load8(G[2] + 0x21u); if (G[2] != 0u) { F[0] = float_at(rt, G[16] + 0x40u); goto L_0894E82C; } F[0] = float_at(rt, G[16] + 0x24u); goto L_0894E834; L_0894E7CC: ; G[1] = 0x3FC90000u; G[1] = G[1] | 0xFDBu; F[1] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[1] < F[0]); if (ctx.fpu_condition()) { F[0] = F[23] - F[0]; goto L_0894E7E8; } L_0894E7E8: ; F[0] = F[0] / F[1]; G[2] = load8(G[21] + 0x167u); F[12] = float_at(rt, G[16] + 0x3Cu); F[1] = std::bit_cast<float>(G[2]); F[12] = psp_mul(F[12], F[12]); F[20] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); F[20] = psp_mul(F[20], F[21]); F[0] = psp_mul(F[0], F[0]); F[12] = F[0] + F[12]; call_here(rt, ctx, 0x08B49D7Cu); F[20] = psp_mul(F[20], F[0]); put_float(rt, G[17] + 0x1Cu, F[20]); L_0894E818: ; G[18] = G[18] + 0x1u; { const bool tk = G[18] != G[20]; G[4] = G[19]; if (tk) goto L_0894E79C; } goto L_0894E84C; L_0894E82C: ; put_float(rt, G[17] + 0x1Cu, F[0]); goto L_0894E818; L_0894E834: ; ctx.set_fpu_condition(F[0] < F[22]); { const bool tk = !ctx.fpu_condition(); if (tk) goto L_0894E7CC; } F[0] = -F[0]; goto L_0894E7CC; L_0894E84C: ; F[23] = float_at(rt, frame.at(0x2Cu)); F[22] = float_at(rt, frame.at(0x28u)); F[21] = float_at(rt, frame.at(0x24u)); F[20] = float_at(rt, frame.at(0x20u)); goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C69534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x60u), G[31]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); G[2] = 0x08D70000u; G[23] = 0x08D77498u; G[22] = G[9]; G[21] = frame.at(0x8u); G[20] = G[8]; G[17] = G[7]; G[16] = G[4]; G[4] = G[21]; G[11] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[5] + 0x4u); G[10] = word(rt, G[5] + 0x0u); G[5] = word(rt, G[6] + 0x4u); G[6] = G[23] + 0xCu; put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x18u), G[5]); G[5] = 0x20u; put(rt, frame.at(0x1Cu), G[10]); put(rt, frame.at(0x14u), G[11]); put(rt, frame.at(0x8u), G[6]); call_here(rt, ctx, 0x08C75840u); G[4] = G[16]; G[7] = G[17]; G[9] = G[21]; G[5] = frame.at(0x1Cu); G[6] = frame.at(0x14u); G[8] = G[20]; call_here(rt, ctx, 0x08C684A4u); put(rt, frame.at(0x1Cu), G[2]); put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x30u), G[2]); put(rt, frame.at(0x34u), G[3]); G[16] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08B5C230u); put(rt, frame.at(0x4u), G[2]); G[4] = G[16]; G[5] = G[22]; G[6] = G[20]; G[7] = frame.at(0x4u); call_here(rt, ctx, 0x08B61BD8u); G[4] = word(rt, frame.at(0x8u)); G[2] = word(rt, frame.at(0x1Cu)); G[3] = word(rt, frame.at(0x20u)); G[16] = G[4] + 0xFFFFFFF4u; put(rt, frame.at(0xCu), G[2]); { const bool tk = G[16] != G[23]; put(rt, frame.at(0x10u), G[3]); if (tk) goto L_08C69650; } G[18] = word(rt, frame.at(0xCu)); L_08C69618: ; G[19] = word(rt, frame.at(0x10u)); G[2] = G[18]; G[3] = G[19]; goto exit_; L_08C69650: ; G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[18] = word(rt, frame.at(0xCu)); goto L_08C69618; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C74884u); G[18] = word(rt, frame.at(0xCu)); goto L_08C69618;  exit_: ; G[23] = word(rt, frame.at(0x5Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B80668 (a0 = the lock argument, a1 = the name, a2 = a pointer for the index; v0 = the address of the text after the '=', or 0): under the lock 0x08B823BC / 0x08B823C4 (a0) searches the
// pointer array word(word(0x08CE1240)) of "NAME=value" strings for the entry whose first n characters equal the name (0x08B7C6BC(entry, name, n), n = the length of the name up to its
// first '=' or end) and whose next character is '='; the index of the entry goes to *a2 and v0 is the text after the '='. No entry (or an empty table) gives 0.
void func_08B80668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr u32 kEnvironment = 0x08CE1240u;
    const u32 lock_argument = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 index_out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{23u, 0x1Cu}, {22u, 0x18u}, {21u, 0x14u}, {18u, 0x8u}, {16u, 0x0u}, {31u, 0x20u}, {20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}});
    call_set(rt, ctx, 0x08B823BCu, {lock_argument});
    u32 slot = word(rt, word(rt, kEnvironment));
    if (slot != 0u) {
        u32 length = 0u;
        while (load8(name + length) != 0u && load8(name + length) != 0x3Du) ++length;
        while (word(rt, slot) != 0u) {
            if (call_set(rt, ctx, 0x08B7C6BCu, {word(rt, slot), name, length}) == 0u) {
                const u32 text = word(rt, slot) + length;
                if (load8(text) == 0x3Du) {
                    const u32 environment = word(rt, kEnvironment);
                    put(rt, index_out, static_cast<u32>(static_cast<s32>(slot - word(rt, environment)) >> 2));
                    call_set(rt, ctx, 0x08B823C4u, {lock_argument});
                    ctx.gpr[2] = text + 1u;
                    return;
                }
            }
            slot += 4u;
        }
    }
    call_set(rt, ctx, 0x08B823C4u, {lock_argument});
    ctx.gpr[2] = 0u;
}
void func_088DDA18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[16]); G[2] = word(rt, G[4] + 0x130u); G[3] = load8(G[2] + 0xFu); { const bool tk = G[3] != 0u; G[17] = G[4]; if (tk) goto L_088DDB38; } G[4] = word(rt, G[4] + 0x640u); call_here(rt, ctx, 0x088E4AA8u); { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_088DDB58; } G[4] = word(rt, G[17] + 0x640u); call_here(rt, ctx, 0x088E4A30u); { const bool tk = G[2] != 0u; G[2] = 0x1u; if (tk) goto L_088DDB58; } goto L_088DDB40; L_088DDA60: ; G[4] = G[17] + 0x4u; call_here(rt, ctx, 0x088D62FCu); { const bool tk = G[2] != 0u; G[5] = 0u; if (tk) goto L_088DDABC; } G[2] = word(rt, G[17] + 0x130u); G[4] = word(rt, G[17] + 0x640u); G[5] = 0xFFFFFFFFu; G[16] = load8(G[2] + 0x12u); call_here(rt, ctx, 0x088E6F94u); G[16] = G[16] + 0xFFFFFFFFu; G[16] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[16]) ? 1u : 0u; { const bool tk = G[16] != 0u; G[2] = 0x3u; if (tk) goto L_088DDB58; } G[2] = load8(G[17] + 0x5C0u); { const bool tk = G[2] != 0u; G[5] = 0u; if (tk) goto L_088DDABC; } G[4] = word(rt, G[17] + 0x640u); G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x088E6F94u); G[2] = static_cast<std::int32_t>(G[2]) < 2 ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = 0x1u; if (tk) goto L_088DDB58; } G[5] = 0u; L_088DDABC: ; G[4] = G[17]; call_here(rt, ctx, 0x088DD984u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[5] = G[2]; if (tk) goto L_088DDB38; } G[3] = G[2] << 2; G[2] = G[2] << 4; G[2] = G[2] - G[3]; G[4] = word(rt, G[17] + 0x644u); G[2] = G[2] + G[5]; G[2] = G[2] << 4; G[16] = G[4] + G[2]; G[5] = frame.at(0u); G[4] = G[17] + 0x4u; G[6] = frame.at(0x2u); G[7] = frame.at(0x4u); call_here(rt, ctx, 0x088D5D54u); G[2] = load16(frame.at(0x0u)); G[3] = word(rt, G[16] + 0x0u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = 0x2u; if (tk) goto L_088DDB58; } G[2] = load16(frame.at(0x2u)); G[3] = word(rt, G[16] + 0x8u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = 0x2u; if (tk) goto L_088DDB58; } G[3] = word(rt, G[16] + 0x10u); G[2] = load16(frame.at(0x4u)); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; if (G[2] != 0u) { G[2] = 0x2u; goto L_088DDB58; } L_088DDB38: ; G[2] = 0u; goto L_088DDB58; L_088DDB40: ; G[4] = G[17]; call_here(rt, ctx, 0x088DA0BCu); { const bool tk = G[2] == 0u; G[5] = 0u; if (tk) goto L_088DDABC; } goto L_088DDA60; L_088DDB58: ; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x088DF63C (a0 = the object; v0 = 0): when 0x088D9E58(object) answers non-zero and the word +0x134 is set, walks the 0x2F8-byte slots from word(object + 0xC0) for the count that
// 0x088D9F80(object) answers (asked again every step). With t = 0x088F8D58(word(object + 0x14C)) and the kind k = 0x088E85C4(slot): k = 4 and a clear flag byte at word(object + 0x598) + index * 8
// takes the slot over (0x088E8A44(slot, 5), the virtual +0x98 of the object at +0x134 and +0x64 of the one at +0x148 with (index, flag address), the flag byte set to 1, the virtual +0x10 of t
// with 1); k = 3 with the byte slot + 0x2A2 set and the byte t + 9 set calls the virtual +0x10 of t with 0.
void func_088DF63C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[18] = 0u;
    if (call_set(rt, ctx, 0x088D9E58u, {object}) == 0u) return;
    ctx.gpr[2] = word(rt, object + 0x134u);
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 index = 0u;
    u32 offset = 0u;
    for (;;) {
        const s32 count = static_cast<s32>(call_set(rt, ctx, 0x088D9F80u, {object}));
        ctx.gpr[5] = index;
        if (!(static_cast<s32>(index) < count)) break;
        const u32 base = word(rt, object + 0xC0u);
        const u32 tracker = call_set(rt, ctx, 0x088F8D58u, {word(rt, object + 0x14Cu)});
        const u32 slot = base + offset;
        const u32 kind = call_set(rt, ctx, 0x088E85C4u, {slot});
        if (kind == 4u) {
            const u32 flag = word(rt, object + 0x598u) + index * 8u;
            if (load8(flag) == 0u) {
                call_set(rt, ctx, 0x088E8A44u, {slot, 5u});
                const u32 first = word(rt, object + 0x134u);
                call_virtual(rt, ctx, word(rt, word(rt, first) + 0x98u), {first, index, flag});
                store8(flag, 1u);
                const u32 second = word(rt, object + 0x148u);
                call_virtual(rt, ctx, word(rt, word(rt, second) + 0x64u), {second, index, flag});
                call_virtual(rt, ctx, word(rt, word(rt, tracker) + 0x10u), {tracker, 1u});
            }
        } else if (kind == 3u) {
            if (load8(slot + 0x2A2u) != 0u && load8(tracker + 9u) != 0u) call_virtual(rt, ctx, word(rt, word(rt, tracker) + 0x10u), {tracker, 0u});
        }
        ++index;
        offset += 0x2F8u;
    }
    ctx.gpr[2] = 0u;
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_322
} // namespace gt::game

#define HOT322(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_322::name, gt::ReturnKind::kind)
// HOT322(0x088D4014u, func_088D4014, Word);  // no comparable synthetic case yet
HOT322(0x0893B6ECu, func_0893B6EC, Word);
HOT322(0x0894E87Cu, func_0894E87C, Word);
HOT322(0x08B03170u, func_08B03170, Word);
// HOT322(0x08B03CE4u, func_08B03CE4, Word);  // no comparable synthetic case yet
// HOT322(0x08B047D0u, func_08B047D0, Word);  // no comparable synthetic case yet
HOT322(0x088DEDE0u, func_088DEDE0, Word);
HOT322(0x0893EB68u, func_0893EB68, Word);
HOT322(0x0896DD54u, func_0896DD54, Word);
HOT322(0x0896DE90u, func_0896DE90, Word);
HOT322(0x08C1C8F0u, func_08C1C8F0, Word);
HOT322(0x08C71F74u, func_08C71F74, Word);
HOT322(0x089E2210u, func_089E2210, Word);
// HOT322(0x08B42710u, func_08B42710, Word);  // no comparable synthetic case yet
HOT322(0x08B58B5Cu, func_08B58B5C, Word);
HOT322(0x08B81874u, func_08B81874, Word);
// HOT322(0x08C64BB4u, func_08C64BB4, Word);  // no comparable synthetic case yet
// HOT322(0x0886A13Cu, func_0886A13C, Word);  // no comparable synthetic case yet
HOT322(0x088A8710u, func_088A8710, Word);
// HOT322(0x088DF4F8u, func_088DF4F8, Word);  // no comparable synthetic case yet
HOT322(0x0894E738u, func_0894E738, Word);
HOT322(0x08C69534u, func_08C69534, Word);
HOT322(0x08B80668u, func_08B80668, Word);
HOT322(0x088DDA18u, func_088DDA18, Word);
HOT322(0x088DF63Cu, func_088DF63C, Word);
