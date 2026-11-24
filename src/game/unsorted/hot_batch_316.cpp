// Hot batch 316: functions no route reaches (straight-line code with forward branches), drafted from
// `tools.disasm` with a register-level emitter (pipeline/bodyx.py), read against the asm and verified by
// synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_316 {
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

// 0x0884C03C (a0 = the object, a1 = an argument for 0x08846EC8; v0 = a0): 0x08846EC8(a1) and 0x08A813F4(sp + 0x1C) run first; the
// latter fills sp + 0x1C with a pointer whose virtual +0xA0 is looked up (0x08A5BB7C(name 0x08C9D044) runs when it is null).
// Twice the same sequence runs: 0x08A69B3C(frame slot, name) formats the name (0x08C9D04C, then 0x08C9D054) into a local; a
// 0x18-byte object is made (0x08A5BC20(0x18, 4), 0x08A76ACC), given the vtable 0x08CB6B88, 0x08A6972C(o + 0x14, slot), and
// registered with 0x08A76AB4(local, sp + slot); 0x08A69D68 frees the local; then the pointer's virtual +0xA0 is called with the
// second object and the local. The second round registers with 0x08A76ABC(o + 0x1C), and each local and object is released
// (0x08A5B9EC). In between, 0x088D5018(0x08846EC8's result) feeds 0x08A85B1C(sp + 0x14, r) for the first virtual call, and
// 0x088D47EC() feeds 0x08A950D0(sp + 0x20, 0, r, 0x10) whose result's +0x14 goes to 0x088D51BC(0x08846EC8's result).
void func_0884C03C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame,
                         {{31u, 0x54u}, {30u, 0x50u}, {23u, 0x4Cu}, {22u, 0x48u}, {21u, 0x44u}, {20u, 0x40u}, {19u, 0x3Cu}, {18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    const u32 first = call_set(rt, ctx, 0x08846EC8u, {argument});
    call_set(rt, ctx, 0x08A813F4u, {frame.at(0x1Cu)});
    const u32 target = word(rt, frame.at(0x1Cu));
    if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9D044u});
    const u32 virtual_call = word(rt, word(rt, target) + 0xA0u);
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x8u), 0x08C9D04Cu});
    const u32 made = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 0x4u});
    call_set(rt, ctx, 0x08A76ACCu, {made});
    put(rt, made + 0x0u, 0x08CB6B88u);
    call_set(rt, ctx, 0x08A6972Cu, {made + 0x14u, frame.at(0x8u)});
    put(rt, frame.at(0xCu), made);
    call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x18u), frame.at(0xCu)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0x8u)});
    const u32 prepared = call_set(rt, ctx, 0x088D5018u, {first});
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0x14u), prepared});
    call_set(rt, ctx, virtual_call, {target, frame.at(0x18u), frame.at(0x14u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
    const u32 table = call_set(rt, ctx, 0x088D47ECu, {});
    call_set(rt, ctx, 0x08A950D0u, {frame.at(0x20u), 0u, table, 0x10u});
    call_set(rt, ctx, 0x088D51BCu, {first, word(rt, word(rt, frame.at(0x20u)) + 0x14u)});
    const u32 second_target = word(rt, frame.at(0x1Cu));
    if (second_target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9D044u});
    const u32 second_virtual = word(rt, word(rt, second_target) + 0xA0u);
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08C9D054u});
    const u32 second_made = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 0x4u});
    call_set(rt, ctx, 0x08A76ACCu, {second_made});
    put(rt, second_made + 0x0u, 0x08CB6B88u);
    call_set(rt, ctx, 0x08A6972Cu, {second_made + 0x14u, frame.at(0u)});
    put(rt, frame.at(0x4u), second_made);
    call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x10u), frame.at(0x4u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    call_set(rt, ctx, second_virtual, {second_target, frame.at(0x10u), frame.at(0x20u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
    call_set(rt, ctx, 0x08A76ABCu, {object, frame.at(0x1Cu)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x20u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x1Cu)});
    ctx.gpr[2] = object;
}

// 0x0884C244 (a0 = the object, a1 = a pointer whose word is the target; v0 = the last release call's): word(a1) is asserted
// (0x08A5BB7C(name 0x08C9CB80)) and its virtual +0x9C looked up. A 0x18-byte object is made (0x08A5BC20(0x18, 4), 0x08A76ACC),
// given vtable 0x08CB6B88 and 0x08A6972C(o + 0x14, sp + 8) after 0x08A69B3C(sp + 8, 0x08C9D04C) formats that name; it is
// registered with 0x08A76AB4(sp + 0x14, sp + 0xC) and the local freed (0x08A69D68). The virtual then runs on the frame slot at
// sp + 0x18: virtual(sp + 0x18, word(a1), sp + 0x14), whose result is released. That slot's word is asserted again and its
// virtual +0x3C called; when that result equals 0x088D47E4()'s, the same sequence runs a second time with name 0x08C9D054,
// local sp + 0, slot sp + 0x1C and virtual +0x9C; then 0x08846E50(a0) feeds 0x088D5588(r, virtual +0xDC(slot, 0)). The slot at
// sp + 0x18 and the second local are released with 0x08A5B9EC.
void func_0884C244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[30] = G[4]; G[20] = word(rt, G[5] + 0x0u); { const bool tk = G[20] != 0u; G[21] = G[5]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C9CB80u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[2] = word(rt, G[20] + 0x0u); G[18] = frame.at(0x8u); G[5] = 0x08CA0000u; G[5] = 0x08C9D04Cu; G[4] = G[18]; G[19] = word(rt, G[2] + 0x9Cu); call_here(rt, ctx, 0x08A69B3Cu); G[5] = 0x4u; G[4] = 0x18u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08A76ACCu); G[2] = 0x08CB0000u; G[22] = 0x08CB6B88u; G[17] = frame.at(0x14u); G[4] = G[16] + 0x14u; put(rt, G[16] + 0x0u, G[22]); G[5] = G[18]; call_here(rt, ctx, 0x08A6972Cu); G[5] = frame.at(0xCu); G[4] = G[17]; put(rt, frame.at(0xCu), G[16]); G[23] = frame.at(0x18u); call_here(rt, ctx, 0x08A76AB4u); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[5] = G[20]; G[6] = G[17]; { const std::uint32_t tg = G[19]; G[4] = G[23]; call_here(rt, ctx, tg); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x18u)); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C9CB80u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = G[2]; call_here(rt, ctx, 0x088D47E4u); { const bool tk = G[16] != G[2]; if (!tk) { G[19] = word(rt, G[21] + 0x0u); { const bool tk = G[19] != 0u; if (tk) { G[2] = word(rt, G[19] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C9CB80u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[19] + 0x0u); } } G[5] = 0x08CA0000u; G[5] = 0x08C9D054u; G[4] = frame.at(0u); G[18] = word(rt, G[2] + 0x9Cu); call_here(rt, ctx, 0x08A69B3Cu); G[5] = 0x4u; G[4] = 0x18u; call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x08A76ACCu); G[17] = frame.at(0x10u); G[4] = G[16] + 0x14u; put(rt, G[16] + 0x0u, G[22]); G[5] = frame.at(0u); call_here(rt, ctx, 0x08A6972Cu); G[5] = frame.at(0x4u); G[4] = G[17]; put(rt, frame.at(0x4u), G[16]); G[20] = frame.at(0x1Cu); call_here(rt, ctx, 0x08A76AB4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[6] = G[17]; G[5] = G[19]; { const std::uint32_t tg = G[18]; G[4] = G[20]; call_here(rt, ctx, tg); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[30]; call_here(rt, ctx, 0x08846E50u); G[16] = word(rt, frame.at(0x1Cu)); { const bool tk = G[16] != 0u; G[17] = G[2]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C9CB80u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[2] = word(rt, G[3] + 0xDCu); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088D5588u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); } } G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu);  G[30] = word(rt, frame.at(0x40u)); G[21] = word(rt, frame.at(0x34u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }

// 0x08A8E85C (a0 = the object; v0 = 0x08B5C078's): an object constructor. 0x08A76ACC(a0) runs first, then the vtable 0x08CDD480
// (0x08CCD480) is stored and the fields at +0x14 .. +0x1C are cleared. 0x08AAE204(+0x20, 0x08CF6ED8) and (+0x24, same) build two
// node objects, +0x28 and +0x34 and +0x3C become self-linked pairs, +0x44 .. +0x50 are zero, +0x54 / +0x5C / +0x64 / +0x6C become
// self-linked pairs, and +0x74 gets the empty string header 0x08D74FDC with count 1 at +0x78. 0x08AAE320(sp + 4, 0x08CF6ED8) is a
// local node; a 0x48-byte object is made (0x08A5BC20(0x48, 4)) and built by 0x08AAEBF0(it, sp + 4, 0x08CF6F04, 5, +0x74, 1), given
// vtable 0x08CD5618 and its +0x24 .. +0x3C fields cleared; it is stored at sp + 0 and 0x08AAE204(sp + 8, sp) registers it. When
// +0x20 is not the local node at sp + 8 it is released (0x08A5BAD8(+0x20, word(sp + 8))) and +0x24 is released as well
// (0x08A5BAD8(+0x24, word(+0x20))). Both locals are freed (0x08A5B9EC), a 0xC-byte object is made (0x08A5BC20(0xC, 4)) whose byte
// +8 is cleared, and 0x08B5C078(it, +0x6C) runs last.
void func_08A8E85C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame,
                         {{31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    call_set(rt, ctx, 0x08A76ACCu, {object});
    put(rt, object + 0x0u, 0x08CCD480u);
    put(rt, object + 0x18u, 0u);
    put(rt, object + 0x1Cu, 0u);
    put(rt, object + 0x14u, 0u);
    call_set(rt, ctx, 0x08AAE204u, {object + 0x20u, 0x08CF6ED8u});
    call_set(rt, ctx, 0x08AAE204u, {object + 0x24u, 0x08CF6ED8u});
    put(rt, object + 0x28u, object + 0x28u);
    put(rt, object + 0x2Cu, object + 0x28u);
    call_set(rt, ctx, 0x08A90470u, {object + 0x30u, 0x08CF6ED8u});
    put(rt, object + 0x34u, object + 0x34u);
    put(rt, object + 0x38u, object + 0x34u);
    put(rt, object + 0x3Cu, object + 0x3Cu);
    put(rt, object + 0x40u, object + 0x3Cu);
    put(rt, object + 0x44u, 0u);
    put(rt, object + 0x48u, 0u);
    put(rt, object + 0x4Cu, 0u);
    put(rt, object + 0x50u, 0u);
    put(rt, object + 0x54u, object + 0x54u);
    put(rt, object + 0x58u, object + 0x54u);
    put(rt, object + 0x5Cu, object + 0x5Cu);
    put(rt, object + 0x60u, object + 0x5Cu);
    put(rt, object + 0x64u, object + 0x64u);
    put(rt, object + 0x68u, object + 0x64u);
    put(rt, object + 0x6Cu, object + 0x6Cu);
    put(rt, object + 0x70u, object + 0x6Cu);
    put(rt, object + 0x74u, 0x08D74FDCu);
    put(rt, object + 0x78u, 1u);
    call_set(rt, ctx, 0x08AAE320u, {frame.at(0x4u), 0x08CF6ED8u});
    const u32 made = call_set(rt, ctx, 0x08A5BC20u, {0x48u, 0x4u});
    call_set(rt, ctx, 0x08AAEBF0u, {made, frame.at(0x4u), 0x08CF6F04u, 5u, object + 0x74u, word(rt, object + 0x78u)});
    put(rt, made + 0x0u, 0x08CD5618u);
    put(rt, made + 0x34u, made + 0x28u);
    put(rt, made + 0x38u, 0u);
    put(rt, made + 0x28u, 0u);
    put(rt, made + 0x2Cu, 0u);
    put(rt, made + 0x30u, made + 0x28u);
    put(rt, made + 0x44u, 0u);
    put(rt, made + 0x3Cu, 0u);
    put(rt, made + 0x40u, 0u);
    put(rt, frame.at(0x0u), made);
    call_set(rt, ctx, 0x08AAE204u, {frame.at(0x8u), frame.at(0x0u)});
    if (object + 0x20u != frame.at(0x8u)) call_set(rt, ctx, 0x08A5BAD8u, {object + 0x20u, word(rt, frame.at(0x8u))});
    if (object + 0x24u != object + 0x20u) call_set(rt, ctx, 0x08A5BAD8u, {object + 0x24u, word(rt, object + 0x20u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
    const u32 footer = call_set(rt, ctx, 0x08A5BC20u, {0xCu, 0x4u});
    if (footer + 0x8u != 0u) store8(footer + 0x8u, 0u);
    call_set(rt, ctx, 0x08B5C078u, {footer, object + 0x6Cu});
}

void func_08AA15C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x64u), G[31]); put(rt, frame.at(0x60u), G[30]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); G[5] = 0x4u; G[23] = G[4]; G[4] = 0x7Cu; G[30] = frame.at(0x14u); G[21] = frame.at(0x18u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x08A8E85Cu); G[4] = G[30]; G[5] = frame.at(0x10u); put(rt, frame.at(0x10u), G[16]); call_here(rt, ctx, 0x08A8C1D4u); G[16] = frame.at(0xCu); G[5] = 0x08C90000u; G[4] = G[16]; G[5] = 0x08C8FBD8u; G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[21]; call_here(rt, ctx, 0x08A6B5E0u); G[4] = word(rt, frame.at(0xCu)); G[2] = 0x08D70000u; G[22] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[22]; G[20] = frame.at(0x1Cu); if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[20] = frame.at(0x1Cu); if (!tk) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[20] = frame.at(0x1Cu); } } } } G[4] = G[20]; G[5] = G[21]; G[19] = frame.at(0x2Cu); call_here(rt, ctx, 0x08A690E4u); G[4] = G[19]; G[5] = G[20]; call_here(rt, ctx, 0x08A76764u); G[18] = word(rt, frame.at(0x14u)); { const bool tk = G[18] != 0u; G[16] = frame.at(0x8u); if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2A4Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = frame.at(0x8u); } } G[5] = 0x08CB0000u; G[17] = frame.at(0x20u); G[4] = G[16]; G[6] = frame.at(0x2u); G[5] = 0x08CB2A58u; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A76724u); G[4] = G[18]; G[5] = G[19]; G[6] = G[17]; call_here(rt, ctx, 0x08A8D1E0u); G[4] = G[17]; call_here(rt, ctx, 0x08A75A88u); G[4] = word(rt, frame.at(0x8u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[22] == G[16]; if (tk) { G[16] = word(rt, frame.at(0x18u)); } else { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) { G[16] = word(rt, frame.at(0x18u)); } else { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[16] = word(rt, frame.at(0x18u)); } } } } { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CB0000u; G[4] = 0x08CB2A68u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[2] = word(rt, G[3] + 0xDCu); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x18u)); { const bool tk = G[16] != 0u; G[17] = G[2]; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2A68u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[2] = word(rt, G[3] + 0xD8u); { const std::uint32_t tg = G[2]; G[16] = frame.at(0x4u); call_here(rt, ctx, tg); } G[6] = G[2]; G[5] = G[17]; G[4] = G[16]; G[7] = 0x10u; call_here(rt, ctx, 0x08A950D0u); { const bool tk = G[23] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (!tk) { G[4] = G[23]; call_here(rt, ctx, 0x08A5BAD8u); } } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0x34u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[30]; call_here(rt, ctx, 0x08A5B9ECu);  G[23] = word(rt, frame.at(0x5Cu)); G[30] = word(rt, frame.at(0x60u)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x40u)); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); }  } }
// 0x088EAD68 (a0 = the object; v0 = the last stored value or the byte at entry): nothing runs when the byte at +0x588 is set.
// 0x08B3E73C(sp) starts a builder; six times 0x08B3E714(sp) resets it, the four words at sp + 0 / +4 and the two bytes +8 / +9 are
// filled and 0x08B3E778(sp, name, name2, object) builds a command, which is submitted as the virtual +0x64 of the object at
// +0x538 with a1 = sp. A negative answer stops the sequence; each answer is stored: +0x578 (0x40 / 0x3E8 / 1), +0x580 (0x120 /
// 0x3E8 / 1), +0x57C (0x1C / 0x3E8 / 1), +0x584 (0x40 / 0x1F4 / 1), the global 0x08CDD3AC (word(0x08CE9C54) / 0x3E8 / 1) and
// +0x58C (word(+0x590) / word(+0x594) / 0). With all six accepted the byte at +0x588 is set to 1.
void func_088EAD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    const u32 done = load8(object + 0x588u);
    if (done != 0u) {
        ctx.gpr[2] = done;
        return;
    }
    call_set(rt, ctx, 0x08B3E73Cu, {frame.at(0x0u)});
    // builds one command on the frame and submits it through word(object + 0x538)'s virtual +0x64
    const auto submit = [&](u32 name, u32 second_name, u32 first, u32 second, u32 flag) -> u32 {
        call_set(rt, ctx, 0x08B3E714u, {frame.at(0x0u)});
        put(rt, frame.at(0x0u), first);
        put(rt, frame.at(0x4u), second);
        store8(frame.at(0x8u), flag);
        store8(frame.at(0x9u), flag);
        call_set(rt, ctx, 0x08B3E778u, {frame.at(0x0u), name, second_name, object});
        const u32 container = word(rt, object + 0x538u);
        return call_set(rt, ctx, word(rt, word(rt, container) + 0x64u), {container, frame.at(0x0u)});
    };
    const u32 first_id = submit(0x088EC56Cu, 0x088EC61Cu, 0x40u, 0x3E8u, 1u);
    put(rt, object + 0x578u, first_id);
    if (static_cast<s32>(first_id) < 0) return;
    const u32 second_id = submit(0x088EBCBCu, 0u, 0x120u, 0x3E8u, 1u);
    put(rt, object + 0x580u, second_id);
    if (static_cast<s32>(second_id) < 0) return;
    const u32 third_id = submit(0x088EBCFCu, 0u, 0x1Cu, 0x3E8u, 1u);
    put(rt, object + 0x57Cu, third_id);
    if (static_cast<s32>(third_id) < 0) return;
    const u32 fourth_id = submit(0x088EC328u, 0x088EC3C4u, 0x40u, 0x1F4u, 1u);
    put(rt, object + 0x584u, fourth_id);
    if (static_cast<s32>(fourth_id) < 0) return;
    call_set(rt, ctx, 0x08B3E714u, {frame.at(0x0u)});
    put(rt, frame.at(0x0u), word(rt, 0x08CE9C54u));
    put(rt, frame.at(0x4u), 0x3E8u);
    store8(frame.at(0x8u), 1u);
    store8(frame.at(0x9u), 1u);
    call_set(rt, ctx, 0x08B3E778u, {frame.at(0x0u), 0u, 0u, object});
    const u32 fifth_container = word(rt, object + 0x538u);
    const u32 fifth_id = call_set(rt, ctx, word(rt, word(rt, fifth_container) + 0x64u), {fifth_container, frame.at(0x0u)});
    put(rt, 0x08CDD3ACu, fifth_id);
    if (static_cast<s32>(fifth_id) < 0) return;
    call_set(rt, ctx, 0x08B3E714u, {frame.at(0x0u)});
    const u32 sixth_first = word(rt, object + 0x590u), sixth_second = word(rt, object + 0x594u);
    put(rt, frame.at(0x0u), sixth_first);
    put(rt, frame.at(0x4u), sixth_second);
    store8(frame.at(0x8u), 0u);
    store8(frame.at(0x9u), 0u);
    call_set(rt, ctx, 0x08B3E778u, {frame.at(0x0u), 0u, 0u, object});
    const u32 sixth_container = word(rt, object + 0x538u);
    const u32 sixth_id = call_set(rt, ctx, word(rt, word(rt, sixth_container) + 0x64u), {sixth_container, frame.at(0x0u)});
    put(rt, object + 0x58Cu, sixth_id);
    if (static_cast<s32>(sixth_id) < 0) return;
    store8(object + 0x588u, 1u);
}

// 0x08818F28 (a0, a1, a2 = a count, a3 = a record; v0 = 0x08818210's): only when a2 < 7. 0x08817FCC(sp + 0x18) makes a name
// (asserted through 0x08A5BB7C(0x08C98D6C)), 0x08817A54(name) is its id at sp + 0x24, and the name is released (0x08A5B9EC).
// Each of the record's targets at +0, +4, +8, +0xC, +0x10, +0x14, +0x18 is asserted (0x08A5BB7C(0x08C98D34)) and its virtual
// called: +0x50 twice (the second target's +0x4C result goes to sp + 0x20), +0xC(sp + 0x14, it), then 0x08B20A54(0x08CE2A9C,
// text, 0) and the text is dropped unless it is the empty header 0x08D74FD0 (0x08B60804(text - 4, -1) and, with none left,
// 0x08B8542C(header, sp + 0x10)). The last three targets run their virtual +0x44 / +0x44 (skipped when null, giving 0) / +0x3C.
// 0x08818210 then gets the id, its word, the two +0x50 results with their v1, word(sp + 0x20) & 0xFF, and on the stack at
// sp + 0 / 4 / 8 / 0xC: 0x08B20A54's result and the three results above. t1 is the one register the original leaves alone.
void func_08818F28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x50u), G[30]); put(rt, frame.at(0x4Cu), G[23]); put(rt, frame.at(0x48u), G[22]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x30u), G[16]); G[6] = static_cast<std::int32_t>(G[6]) < 7 ? 1u : 0u; G[17] = G[7]; { const bool tk = G[6] != 0u; if (!tk) { G[18] = frame.at(0x18u); G[4] = G[18]; call_here(rt, ctx, 0x08817FCCu); G[16] = word(rt, frame.at(0x18u)); { const bool tk = G[16] != 0u; G[4] = 0x08CA0000u; if (!tk) { G[4] = 0x08C98D6Cu; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x08817A54u); G[4] = G[18]; put(rt, frame.at(0x24u), G[2]); call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x50u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = word(rt, G[17] + 0x4u); G[22] = G[2]; { const bool tk = G[16] != 0u; G[23] = G[3]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x4Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, frame.at(0x20u), G[2]); G[16] = word(rt, G[17] + 0x8u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x50u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[16] = word(rt, G[17] + 0xCu); G[20] = G[2]; { const bool tk = G[16] != 0u; G[21] = G[3]; if (!tk) { G[4] = 0x08CA0000u; G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[3] = word(rt, G[16] + 0x0u); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = frame.at(0x14u); call_here(rt, ctx, tg); } G[5] = word(rt, frame.at(0x14u)); G[4] = 0x08CE0000u; G[4] = 0x08CE2A9Cu; G[6] = 0u; call_here(rt, ctx, 0x08B20A54u); G[4] = word(rt, frame.at(0x14u)); G[30] = G[2]; G[2] = 0x08D70000u; G[16] = G[4] + 0xFFFFFFF4u; G[2] = 0x08D74FD0u; { const bool tk = G[16] == G[2]; if (tk) { G[16] = word(rt, G[17] + 0x10u); } else { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) { G[16] = word(rt, G[17] + 0x10u); } else { G[4] = G[16]; G[5] = frame.at(0x10u); call_here(rt, ctx, 0x08B8542Cu); G[16] = word(rt, G[17] + 0x10u); } } } } { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[4] = G[16]; G[2] = word(rt, G[3] + 0x44u); { const std::uint32_t tg = G[2]; G[19] = 0u; call_here(rt, ctx, tg); } G[4] = word(rt, G[17] + 0x14u); { const bool tk = G[4] == 0u; G[18] = G[2]; if (!tk) { G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x44u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } G[19] = G[2]; } } G[16] = word(rt, G[17] + 0x18u); { const bool tk = G[16] != 0u; if (tk) { G[3] = word(rt, G[16] + 0x0u); } else { G[4] = 0x08CA0000u; G[4] = 0x08C98D34u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); } } G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, frame.at(0x24u)); G[6] = G[22]; G[7] = G[23]; G[5] = word(rt, G[3] + 0x0u); G[4] = G[3]; G[3] = word(rt, frame.at(0x20u)); G[10] = G[20]; G[11] = G[21]; G[8] = G[3] & 0xFFu; put(rt, frame.at(0x0u), G[30]); put(rt, frame.at(0x4u), G[18]); put(rt, frame.at(0x8u), G[19]); put(rt, frame.at(0xCu), G[2]); call_here(rt, ctx, 0x08818210u); } }  G[17] = word(rt, frame.at(0x34u)); G[30] = word(rt, frame.at(0x50u)); G[23] = word(rt, frame.at(0x4Cu)); G[22] = word(rt, frame.at(0x48u)); G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x30u)); }  } }

// 0x08A30AC8 (a0 = the object; v0 = a0): 0x08A69B3C(sp, 0x08CAC870) formats a name, a 0x38-byte object is made
// (0x08A5BC20(0x38, 4), 0x08A7E914(it, sp)) and registered on a0 with 0x08A76AB4(a0, sp + 4) after the name is freed
// (0x08A69D68(sp)). Then word(a0) (asserted with 0x08A5BB7C(name 0x08CAC884)) gets ten handlers, each with its own name:
// 0x08B86FB0(word(a0), name, handler) for 0x08CAC88C -> 0x08A30D18, 0x08CAC8A0 -> 0x08A30D5C, 0x08CAC8A8 -> 0x08A314B0,
// 0x08CAC8B8 -> 0x08A31244, 0x08CAC8C8 -> 0x08A30DAC, 0x08CAC8DC -> 0x08A30E48, 0x08CAC8F0 -> 0x08A3119C,
// 0x08CAC8FC -> 0x08A30F24, 0x08CAC908 -> 0x08A30E98 and 0x08CAC914 -> 0x08A30EE0.
void func_08A30AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x0u), 0x08CAC870u});
    const u32 made = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 0x4u});
    call_set(rt, ctx, 0x08A7E914u, {made, frame.at(0x0u)});
    put(rt, frame.at(0x4u), made);
    call_set(rt, ctx, 0x08A76AB4u, {object, frame.at(0x4u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0x0u)});
    struct Handler {
        u32 name, entry;
    };
    const Handler handlers[10] = {
        {0x08CAC88Cu, 0x08A30D18u}, {0x08CAC8A0u, 0x08A30D5Cu}, {0x08CAC8A8u, 0x08A314B0u}, {0x08CAC8B8u, 0x08A31244u},
        {0x08CAC8C8u, 0x08A30DACu}, {0x08CAC8DCu, 0x08A30E48u}, {0x08CAC8F0u, 0x08A3119Cu}, {0x08CAC8FCu, 0x08A30F24u},
        {0x08CAC908u, 0x08A30E98u}, {0x08CAC914u, 0x08A30EE0u},
    };
    for (const Handler &handler : handlers) {
        const u32 target = word(rt, object + 0x0u);
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC884u});
        call_set(rt, ctx, 0x08B86FB0u, {target, handler.name, handler.entry});
    }
    ctx.gpr[2] = object;
}

// 0x08A957C4 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CB21BC, sp + 1) formats the text on the frame (+4); the text is released (unless it is the empty header
// 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp) frees it). 0x08A76A94() gives a handle: a handle is released (0x08A5BA08), then the old one at +0x38 (0x08A5B930) and the new one is stored at +0x38.
// 0x08B8850C(object, 0x08CF6F14, 0x08A955F8) and 0x08B8A004(object, 0x08CF6F24, 0x08A94DE0) register handlers. Then seven 8-byte values (at 0x08C8F80C, 0x08C8F7FC, ... see the table) are copied to the frame and registered
// with 0x08C2CB68 (twice: 0x08CB21C8 with +0x48 / +0x40 and 0x08CB21D0 with +0x38 / +0x30, the slots as the third and fourth argument), 0x08C2D520 (0x08CB21D8, +0x28), 0x08C2D384 (0x08CB21E0, +0x20) and 0x08C2D1E8 (0x08CF6FD0, +0x18; 0x08CF6FC0, +0x10; 0x08CF6F9C, +8):
// the later ones take the slot, the value's low word and its high word.
void func_08A957C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x5Cu}, {18u, 0x58u}, {16u, 0x50u}, {17u, 0x54u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CB21BCu, frame.at(0x1u)});
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
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x08A955F8u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x08A94DE0u});
    // an 8-byte value copied to a frame slot; its low and high words
    auto copy = [&](u32 source, u32 slot) {
        const u32 low = word(rt, source), high = word(rt, source + 0x4u);
        put(rt, frame.at(slot), low);
        put(rt, frame.at(slot + 0x4u), high);
        return std::pair<u32, u32>(low, high);
    };
    // the two-slot registrations: the high word of each value goes to t0 / t1
    const auto first = copy(0x08C8F80Cu, 0x48u);
    const auto first_b = copy(0x08C8F804u, 0x40u);
    ctx.gpr[8] = first.second;
    ctx.gpr[9] = first_b.second;
    call_set(rt, ctx, 0x08C2CB68u, {object, 0x08CB21C8u, frame.at(0x48u), frame.at(0x40u), first.second, first_b.second});
    const auto second = copy(0x08C8F7FCu, 0x38u);
    const auto second_b = copy(0x08C8F7F4u, 0x30u);
    ctx.gpr[8] = second.second;
    ctx.gpr[9] = second_b.second;
    call_set(rt, ctx, 0x08C2CB68u, {object, 0x08CB21D0u, frame.at(0x38u), frame.at(0x30u), second.second, second_b.second});
    struct Registration {
        u32 source, name, slot, callee;
    };
    const Registration registrations[5] = {
        {0x08C8F7ECu, 0x08CB21D8u, 0x28u, 0x08C2D520u}, {0x08C8F7E4u, 0x08CB21E0u, 0x20u, 0x08C2D384u}, {0x08C8F7DCu, 0x08CF6FD0u, 0x18u, 0x08C2D1E8u},
        {0x08C8F7D4u, 0x08CF6FC0u, 0x10u, 0x08C2D1E8u}, {0x08C8F7CCu, 0x08CF6F9Cu, 0x8u, 0x08C2D1E8u},
    };
    for (const Registration &entry : registrations) {
        const auto value = copy(entry.source, entry.slot);
        ctx.gpr[8] = value.second;
        call_set(rt, ctx, entry.callee, {object, entry.name, frame.at(entry.slot), value.first, value.second});
    }
}
// 0x08A14D94 (a0 = the object; v0 = the last release call's): 0x089C9C34(sp + 0xC) fills a name (asserted with 0x08A5BB7C(name
// 0x08CA9A60)), 0x089C4CC4(name, object + 0x10D) uses it, 0x08B85EB8(sp + 8, 0x08CA9B30, sp + 3) formats a second and
// 0x089C5D04(sp + 0x10, name, sp + 8) fills a third, which is released unless it is the empty header 0x08D74FD0
// (0x08B60804(sp + 4, -1), then 0x08B8542C(header, sp + 1)). When the word at sp + 0x10 is null, 0x08B3767C(sp + 0x24, 0x80,
// 0x08CA9B40, object + 0x10D) builds a path, 0x08B85EB8(sp + 4, sp + 0x24, sp + 2) formats it, 0x089C9F24(name, sp + 0x10, sp + 4,
// 1) uses it and the formatted text at sp + 4 is released the same way (0x08B8542C(header, sp)). The name is then passed to
// 0x089C6370 and 0x089C4FD4, whose result's virtual +0x124 is called, 0x08AD0018(object + 0x18) and 0x08A1AA24(object + 0x18,
// result) follow, and a handler record on the frame (0x00010000, 0x08A14D6C, object) goes through 0x08B2E314(1, 5),
// 0x08B2E134 and 0x08B2E7DC(sp + 0x1C, sp + 0x14, r), which is stored at object + 0x104 and given to 0x08B2E214. Both locals are
// released (0x08A5B9EC).
void func_08A14D94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xD0u); put(rt, frame.at(0xC4u), G[21]); put(rt, frame.at(0xB8u), G[18]); put(rt, frame.at(0xB0u), G[16]); put(rt, frame.at(0xCCu), G[31]); put(rt, frame.at(0xC8u), G[22]); put(rt, frame.at(0xC0u), G[20]); put(rt, frame.at(0xBCu), G[19]); put(rt, frame.at(0xB4u), G[17]); G[21] = frame.at(0xCu); G[18] = G[4]; G[4] = G[21]; call_here(rt, ctx, 0x089C9C34u); G[16] = word(rt, frame.at(0xCu)); { const bool tk = G[16] != 0u; G[19] = G[18] + 0x10Du; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CA9A60u; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = G[18] + 0x10Du; } } G[4] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x089C4CC4u); G[17] = word(rt, frame.at(0xCu)); { const bool tk = G[17] != 0u; G[16] = frame.at(0x8u); if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CA9A60u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = frame.at(0x8u); } } G[5] = 0x08CB0000u; G[4] = G[16]; G[5] = 0x08CA9B30u; G[6] = frame.at(0x3u); G[20] = frame.at(0x10u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[17]; G[6] = G[16]; G[4] = G[20]; call_here(rt, ctx, 0x089C5D04u); G[4] = word(rt, frame.at(0x8u)); G[2] = 0x08D70000u; G[17] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[17]; G[2] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = word(rt, frame.at(0x10u)); if (!tk) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[2] = word(rt, frame.at(0x10u)); } } } } { const bool tk = G[2] != 0u; G[16] = word(rt, frame.at(0xCu)); if (!tk) { G[22] = frame.at(0x24u); G[6] = 0x08CB0000u; G[7] = G[19]; G[6] = 0x08CA9B40u; G[4] = G[22]; G[5] = 0x80u; call_here(rt, ctx, 0x08B3767Cu); G[19] = word(rt, frame.at(0xCu)); { const bool tk = G[19] != 0u; G[16] = frame.at(0x4u); if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CA9A60u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = frame.at(0x4u); } } G[4] = G[16]; G[5] = G[22]; G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B85EB8u); G[4] = G[19]; G[6] = G[16]; G[5] = G[20]; G[7] = 0x1u; call_here(rt, ctx, 0x089C9F24u); G[4] = word(rt, frame.at(0x4u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[17] == G[16]; if (tk) { G[16] = word(rt, frame.at(0xCu)); } else { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) { G[16] = word(rt, frame.at(0xCu)); } else { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); G[16] = word(rt, frame.at(0xCu)); } } } } } } { const bool tk = G[16] != 0u; G[4] = 0x08CB0000u; if (!tk) { G[4] = 0x08CA9A60u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x089C6370u); G[16] = word(rt, frame.at(0xCu)); { const bool tk = G[16] != 0u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CA9A60u; call_here(rt, ctx, 0x08A5BB7Cu); } } G[4] = G[16]; call_here(rt, ctx, 0x089C4FD4u); G[3] = word(rt, G[2] + 0x0u); G[16] = G[2]; G[4] = G[2]; G[2] = word(rt, G[3] + 0x124u); { const std::uint32_t tg = G[2]; G[17] = G[18] + 0x18u; call_here(rt, ctx, tg); } G[4] = G[17]; call_here(rt, ctx, 0x08AD0018u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08A1AA24u); G[2] = 0x08A10000u; G[3] = 0x00010000u; G[5] = 0x5u; G[4] = 0x1u; G[2] = 0x08A14D6Cu; put(rt, frame.at(0x18u), G[3]); put(rt, frame.at(0x20u), G[18]); put(rt, frame.at(0x1Cu), G[2]); put(rt, frame.at(0x14u), 0u); call_here(rt, ctx, 0x08B2E314u); G[4] = G[2]; call_here(rt, ctx, 0x08B2E134u); G[6] = G[2]; G[5] = frame.at(0x14u); G[4] = frame.at(0x1Cu); call_here(rt, ctx, 0x08B2E7DCu); put(rt, G[18] + 0x104u, G[2]); G[4] = G[2]; call_here(rt, ctx, 0x08B2E214u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu);  G[21] = word(rt, frame.at(0xC4u)); G[18] = word(rt, frame.at(0xB8u)); G[16] = word(rt, frame.at(0xB0u)); G[22] = word(rt, frame.at(0xC8u)); G[20] = word(rt, frame.at(0xC0u)); G[19] = word(rt, frame.at(0xBCu)); G[17] = word(rt, frame.at(0xB4u)); }  } }

// 0x0899B224 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CA46AC, sp + 1) formats the text on the frame (+4); the text is released (unless it is the empty header
// 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp) frees it). 0x08994488() gives a handle: a handle is released (0x08A5BA08), then the old one at +0x38 (0x08A5B930), and the new one is stored at +0x38.
// Six pairs of 8-byte values (the table below) are copied to frame slots and registered with 0x08BF3968(object, name, first slot, second slot, high word of the first value, high word of the second value).
void func_0899B224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x7Cu}, {18u, 0x78u}, {16u, 0x70u}, {17u, 0x74u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA46ACu, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08994488u, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    struct Registration {
        u32 first_source, second_source, first_slot, second_slot, name;
    };
    const Registration registrations[6] = {
        {0x08C8C418u, 0x08C8C410u, 0x60u, 0x58u, 0x08CA4600u}, {0x08C8C408u, 0x08C8C400u, 0x50u, 0x48u, 0x08CA4610u},
        {0x08C8C3F8u, 0x08C8C3F0u, 0x40u, 0x38u, 0x08CA4620u}, {0x08C8C3E8u, 0x08C8C3E0u, 0x30u, 0x28u, 0x08CA4630u},
        {0x08C8C3D8u, 0x08C8C3D0u, 0x20u, 0x18u, 0x08CA46B8u}, {0x08C8C3C8u, 0x08C8C3C0u, 0x10u, 0x8u, 0x08CA46C4u},
    };
    put(rt, object + 0x38u, handle);
    for (const Registration &entry : registrations) {
        const u32 first_high = word(rt, entry.first_source + 0x4u), second_high = word(rt, entry.second_source + 0x4u);
        ctx.gpr[8] = first_high;
        ctx.gpr[9] = second_high;
        put(rt, frame.at(entry.first_slot), word(rt, entry.first_source));
        put(rt, frame.at(entry.first_slot + 0x4u), first_high);
        put(rt, frame.at(entry.second_slot), word(rt, entry.second_source));
        put(rt, frame.at(entry.second_slot + 0x4u), second_high);
        call_set(rt, ctx, 0x08BF3968u, {object, entry.name, frame.at(entry.first_slot), frame.at(entry.second_slot), first_high, second_high});
    }
}
// 0x089B7A08 (a0 = the object, a1 = a value; v0 = the last call's): 0x089B5E10(object, value) gives a handle (zero ends it); with the byte +0x15A of the object set a child is made: 0x08969160(frame + 0x10), 0x08A69B3C(frame + 0xC, 0x08CA59BC),
// 0x08A7BFD0(object, frame + 0x10, frame + 0xC), 0x08A69D68(frame + 0xC), 0x08A77444(frame + 8, handle), 0x08969244(frame + 0x14, frame + 8) and the temporary +8 is released (0x08A5B9EC). The child's virtual +0x108 is called with (child, text) after
// 0x08B85EB8(text, 0x08CA59BC, sp + 1) formats the text on the frame (+4) and the text is released (unless it is the empty header 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp) frees it).
// The child (frame +0x14; a null one calls the assert 0x08A5BB7C(0x08CA59E4) at every use) gets 0x0896A9C4(child, frame + 0x1C, frame + 0x18, 0, 0) with both words zeroed, 0x089687D8(child, 0), bit 17 of its flags +0xE0 cleared,
// 0x0896A824(child, 1) and 0x089663BC(child, 1). A non-null word at +0x10 makes the object's virtual +0x234 run with (object, value); then the virtual +0x230 with (object, value, child, the object's word +0x120), 0x089B6344(child) and the releases
// of +0x14 and +0x10. 0x089B5C4C(object, handle) ends it.
void func_089B7A08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x38u}, {20u, 0x30u}, {19u, 0x2Cu}, {31u, 0x44u}, {30u, 0x40u}, {23u, 0x3Cu}, {21u, 0x34u}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    ctx.gpr[22] = value;
    ctx.gpr[19] = object;
    const u32 handle = call_set(rt, ctx, 0x089B5E10u, {object, value});
    ctx.gpr[20] = handle;
    if (handle == 0u) return;
    if (load8(object + 0x15Au) != 0u) {
        const u32 temporary = frame.at(0x10u), name = frame.at(0xCu), child_slot = frame.at(0x14u), copy = frame.at(0x8u), text = frame.at(0x4u);
        ctx.gpr[23] = temporary;
        ctx.gpr[17] = name;
        ctx.gpr[21] = high_half(0x08CA59E4u);
        call_set(rt, ctx, 0x08969160u, {temporary});
        call_set(rt, ctx, 0x08A69B3Cu, {name, 0x08CA59BCu});
        call_set(rt, ctx, 0x08A7BFD0u, {object, temporary, name});
        ctx.gpr[16] = copy;
        call_set(rt, ctx, 0x08A69D68u, {name});
        ctx.gpr[30] = child_slot;
        call_set(rt, ctx, 0x08A77444u, {copy, handle});
        call_set(rt, ctx, 0x08969244u, {child_slot, copy});
        call_set(rt, ctx, 0x08A5B9ECu, {copy});
        // the child (word at the frame's +0x14); a null one calls the assert
        auto child_of_slot = [&] {
            const u32 child = word(rt, child_slot);
            ctx.gpr[16] = child;
            if (child == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA59E4u});
            return child;
        };
        const u32 first_child = word(rt, child_slot);
        ctx.gpr[18] = first_child;
        if (first_child == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA59E4u});
        const u32 virtual_call = word(rt, word(rt, first_child) + 0x108u);
        ctx.gpr[16] = text;
        ctx.gpr[17] = virtual_call;
        call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA59BCu, frame.at(0x1u)});
        call_set(rt, ctx, virtual_call, {first_child, text});
        const u32 data = word(rt, text);
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        if (header != 0x08D74FD0u) {
            const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
            if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
        }
        u32 child = child_of_slot();
        put(rt, frame.at(0x1Cu), 0u);
        put(rt, frame.at(0x18u), 0u);
        call_set(rt, ctx, 0x0896A9C4u, {child, frame.at(0x1Cu), frame.at(0x18u), 0u, 0u});
        child = child_of_slot();
        call_set(rt, ctx, 0x089687D8u, {child, 0u});
        child = child_of_slot();
        ctx.gpr[2] = word(rt, child + 0xE0u);
        put(rt, child + 0xE0u, word(rt, child + 0xE0u) & ~(1u << 17));
        child = child_of_slot();
        call_set(rt, ctx, 0x0896A824u, {child, 1u});
        child = child_of_slot();
        call_set(rt, ctx, 0x089663BCu, {child, 1u});
        if (word(rt, temporary) != 0u) call_set(rt, ctx, word(rt, word(rt, object) + 0x234u), {object, value});
        const u32 virtual_after = word(rt, word(rt, object) + 0x230u);
        call_set(rt, ctx, virtual_after, {object, value, word(rt, child_slot), word(rt, object + 0x120u)});
        call_set(rt, ctx, 0x089B6344u, {word(rt, child_slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {child_slot});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }
    call_set(rt, ctx, 0x089B5C4Cu, {object, handle});
}
void func_088CD73C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[17] = G[4]; G[4] = G[5]; G[3] = word(rt, G[5] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[16] = G[5]; call_here(rt, ctx, tg); } store8(G[17] + 0x24u, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } store8(G[17] + 0x25u, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x10u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } store8(G[17] + 0x26u, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x14u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } store8(G[17] + 0x27u, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x38u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, G[17] + 0x0u, G[2]); put(rt, G[17] + 0x4u, G[3]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } store8(G[17] + 0x34u, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x40u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x10000u) | ((G[2] << 16) & 0x10000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x44u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x20000u) | ((G[2] << 17) & 0x20000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x48u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x1F00u) | ((G[2] << 8) & 0x1F00u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x4Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x2000u) | ((G[2] << 13) & 0x2000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x50u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } store16(G[17] + 0x28u, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } store16(G[17] + 0x2Au, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x58u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x4000u) | ((G[2] << 14) & 0x4000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x5Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, G[17] + 0x8u, G[2]); put(rt, G[17] + 0xCu, G[3]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x60u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } store16(G[17] + 0x2Cu, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x64u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, G[17] + 0x18u, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x68u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x40000u) | ((G[2] << 18) & 0x40000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x6Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x8000u) | ((G[2] << 15) & 0x8000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x70u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x80000u) | ((G[2] << 19) & 0x80000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x78u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x100000u) | ((G[2] << 20) & 0x100000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x7Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[17] + 0x34u); G[3] = (G[3] & ~0x200000u) | ((G[2] << 21) & 0x200000u); put(rt, G[17] + 0x34u, G[3]); G[5] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[5] + 0x80u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, G[17] + 0x10u, G[2]); put(rt, G[17] + 0x14u, G[3]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x84u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, G[17] + 0x38u, G[2]); G[6] = word(rt, G[16] + 0x0u); put(rt, frame.at(0x4u), G[3]); put(rt, frame.at(0x14u), G[3]); put(rt, frame.at(0x10u), G[2]); put(rt, frame.at(0x0u), G[2]); G[2] = word(rt, G[6] + 0x88u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, G[17] + 0x3Cu, G[2]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x74u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put(rt, G[17] + 0x30u, G[2]);  G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); }  } }
// 0x089F7298 (a0 = the object; v0 = the last call's): the object's virtual +0x108 is called with (object, text) after 0x08B85EB8(text, 0x08CA83EC, sp + 1) formats the text on the frame (+4); the text is released (unless it is the empty header
// 0x08D74FD0 its count 0x08B60804(data - 4, -1) drops and, with none left, 0x08B8542C(header, sp) frees it). 0x08A0F03C() gives a handle: a handle is released (0x08A5BA08), then the old one at +0x38 (0x08A5B930), and the new one is stored at +0x38.
// 0x08B8850C(object, 0x08CF6F14, 0x089F6EA0) and 0x08B8A004(object, 0x08CF6F24, 0x089F6C34) register handlers. Four pairs of 8-byte values (the first table) are copied to frame slots and registered with 0x08C0B958 (three times) and
// 0x08C0B7DC (object, name, first slot, second slot, high words); three single values (the second table) with 0x08C0C1EC(object, name, 0, slot, low word, high word).
void func_089F7298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x70u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x6Cu}, {18u, 0x68u}, {16u, 0x60u}, {17u, 0x64u}});
    const u32 text = frame.at(0x4u);
    ctx.gpr[18] = object;
    ctx.gpr[16] = text;
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    ctx.gpr[17] = virtual_call;
    call_set(rt, ctx, 0x08B85EB8u, {text, 0x08CA83ECu, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, text});
    const u32 data = word(rt, text);
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
    const u32 handle = call_set(rt, ctx, 0x08A0F03Cu, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 old = word(rt, object + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, object + 0x38u, handle);
    call_set(rt, ctx, 0x08B8850Cu, {object, 0x08CF6F14u, 0x089F6EA0u});
    call_set(rt, ctx, 0x08B8A004u, {object, 0x08CF6F24u, 0x089F6C34u});
    struct Pair {
        u32 first_source, second_source, first_slot, second_slot, name, callee;
    };
    const Pair pairs[4] = {
        {0x08C8DA54u, 0x08C8DA4Cu, 0x58u, 0x50u, 0x08CA8398u, 0x08C0B958u}, {0x08C8DA44u, 0x08C8DA3Cu, 0x48u, 0x40u, 0x08CA8480u, 0x08C0B958u},
        {0x08C8DA34u, 0x08C8DA2Cu, 0x38u, 0x30u, 0x08CA848Cu, 0x08C0B958u}, {0x08C8DA24u, 0x08C8DA1Cu, 0x28u, 0x20u, 0x08CA83B8u, 0x08C0B7DCu},
    };
    for (const Pair &entry : pairs) {
        const u32 first_high = word(rt, entry.first_source + 0x4u), second_high = word(rt, entry.second_source + 0x4u);
        ctx.gpr[8] = first_high;
        ctx.gpr[9] = second_high;
        put(rt, frame.at(entry.first_slot), word(rt, entry.first_source));
        put(rt, frame.at(entry.first_slot + 0x4u), first_high);
        put(rt, frame.at(entry.second_slot), word(rt, entry.second_source));
        put(rt, frame.at(entry.second_slot + 0x4u), second_high);
        call_set(rt, ctx, entry.callee, {object, entry.name, frame.at(entry.first_slot), frame.at(entry.second_slot), first_high, second_high});
    }
    struct Single {
        u32 source, slot, name;
    };
    const Single singles[3] = {{0x08C8DA14u, 0x18u, 0x08CA838Cu}, {0x08C8DA0Cu, 0x10u, 0x08CA8498u}, {0x08C8DA04u, 0x8u, 0x08CA84A0u}};
    for (const Single &entry : singles) {
        const u32 high = word(rt, entry.source + 0x4u), low = word(rt, entry.source);
        ctx.gpr[8] = low;
        ctx.gpr[9] = high;
        put(rt, frame.at(entry.slot), low);
        put(rt, frame.at(entry.slot + 0x4u), high);
        call_set(rt, ctx, 0x08C0C1ECu, {object, entry.name, 0u, frame.at(entry.slot), low, high});
    }
}
// 0x088057F8 (a0 unused on entry; v0 = the last release call's): 0x08A9DD14(sp + 0x24) makes a container whose word is asserted
// (0x08A5BB7C(name 0x08C97D2C)) and six registrations follow, one per slot of the frame: 0x08B85EB8(slot, format, sp + tag)
// formats a name into the slot, 0x08A9CB38(word(sp + 0x24), slot, handler) registers it, and the name is released unless it is
// the empty header 0x08D74FD0 (0x08B60804(text - 4, -1); with no count left 0x08B8542C(header, sp + base)). The slots, formats,
// tag offsets, handlers and free bases are the table below (sp + 0x20 / 0x1C / 0x18 / 0x14 / 0x10 / 0xC, tags 0xB .. 0x6, bases
// 5 .. 0). The container at sp + 0x24 is released last (0x08A5B9EC).
void func_088057F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x40u}, {19u, 0x3Cu}, {18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    call_set(rt, ctx, 0x08A9DD14u, {frame.at(0x24u)});
    struct Registration {
        u32 slot, format, tag, handler, base;
    };
    const Registration registrations[6] = {
        {0x20u, 0x08C97D34u, 0xBu, 0x08A2C2B4u, 0x5u}, {0x1Cu, 0x08C97D3Cu, 0xAu, 0x08A1AC08u, 0x4u}, {0x18u, 0x08C97D44u, 0x9u, 0x089645C4u, 0x3u},
        {0x14u, 0x08C97D4Cu, 0x8u, 0x088175B8u, 0x2u}, {0x10u, 0x08C97D58u, 0x7u, 0x08809650u, 0x1u}, {0x0Cu, 0x08C97D60u, 0x6u, 0x08805D60u, 0x0u},
    };
    for (const Registration &entry : registrations) {
        const u32 holder = word(rt, frame.at(0x24u));
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97D2Cu});
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(entry.slot), entry.format, frame.at(entry.tag)});
        call_set(rt, ctx, 0x08A9CB38u, {holder, frame.at(entry.slot), entry.handler});
        const u32 text = word(rt, frame.at(entry.slot));
        const u32 header = text - 0xCu;
        if (header != 0x08D74FD0u) {
            const u32 left = call_set(rt, ctx, 0x08B60804u, {text - 0x4u, 0xFFFFFFFFu});
            if (static_cast<s32>(left) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(entry.base)});
        }
    }
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x24u)});
}

// 0x08A1AEDC (a0 = the object; v0 = a0): 0x08A69B3C(sp, 0x08CA9EC8) formats a name, a 0x38-byte object (0x08A5BC20(0x38, 4),
// 0x08A7E914(it, sp)) is registered on a0 with 0x08A76AB4(a0, sp + 4) and the name is freed (0x08A69D68(sp)). Then
// word(a0) (asserted with 0x08A5BB7C(name 0x08CA9ED8)) gets nineteen handlers through 0x08B86FB0(word(a0), name, handler): the
// table below.
void func_08A1AEDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x0u), 0x08CA9EC8u});
    const u32 made = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 0x4u});
    call_set(rt, ctx, 0x08A7E914u, {made, frame.at(0x0u)});
    put(rt, frame.at(0x4u), made);
    call_set(rt, ctx, 0x08A76AB4u, {object, frame.at(0x4u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0x0u)});
    struct Handler {
        u32 name, entry;
    };
    const Handler handlers[19] = {
        {0x08CA9EE0u, 0x08A2AB38u}, {0x08CA9EF8u, 0x08A2ABF0u}, {0x08CA9F14u, 0x08A2ACA8u}, {0x08CA9F2Cu, 0x08A2AD60u},
        {0x08CA9F48u, 0x08A2ADB4u}, {0x08CA9F58u, 0x08A2AEDCu}, {0x08CA9F64u, 0x08A2B004u}, {0x08CA9F70u, 0x08A2B5DCu},
        {0x08CA9F80u, 0x08A2B650u}, {0x08CA9F94u, 0x08A2BD58u}, {0x08CA9FA0u, 0x08A2B6A4u}, {0x08CA9FB4u, 0x08A2AAE0u},
        {0x08CA9FC8u, 0x08A2B6F4u}, {0x08CA9FD8u, 0x08A2B890u}, {0x08CA9FECu, 0x08A2BD50u}, {0x08CAA000u, 0x08A2B8D8u},
        {0x08CAA01Cu, 0x08A2AA7Cu}, {0x08CAA03Cu, 0x08A2BDF0u}, {0x08CAA050u, 0x08A2B928u},
    };
    for (const Handler &handler : handlers) {
        const u32 target = word(rt, object + 0x0u);
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA9ED8u});
        call_set(rt, ctx, 0x08B86FB0u, {target, handler.name, handler.entry});
    }
    ctx.gpr[2] = object;
}

// 0x08862C90 (a0 = the object; v0 = the last call's): a large constructor. 0x08866908(a0) runs first, then two name objects are
// made with 0x08ABF3CC(name, 8) into +0x90 and +0x94 and their virtual +0xC called; the first also gets a0 + 0x3F80, kept at
// sp + 0x20. Both get their virtual +0x30 with 0. A 0x110-byte buffer (0x08B62344(0x110), 0x0890BD30) goes to +0x98 with byte
// +0x9C cleared, +0xA0 is stored at +0x58, 0x0885DDC8(it) feeds 0x0892BAA8(a0 + 0xD8, a0 + 0xA0, it, 0), and
// 0x08B21938(0x0892B71C(0)) goes to +0x36C. Three handlers go into a0 + 0x5BC (0x08896CC8 with 0 / 0x088612B8, 1 / 0x0885DEE8,
// 2 / 0x0885DEF0 after +0x5BC holds a0). A 0x788-byte renderer (0x088E29DC(it, +0x90), 0x088D9C34(it, 0, 0x1C38)) goes to +0x5AC;
// 0x64 bytes (0x088A31B4(it, 4)) to +0x5B0; 0x44 bytes (0x088982C4(it), 0x08898664(it, +0x54)) to +0x5B4.
// 0x088914B4(word(sp + 0x20), +0x5AC, +0x5B0, +0x5B4, +0x3F88, +0x3F90) and 0x0889364C(+0x600) run, then
// 0x08865F04(a0 + 0x3F58, a0 + 0xD8, +0x5AC, +0x5B4, +0x600) and 0x08866174(a0 + 0x3F58), whose result goes through
// 0x088DA0AC(+0x5AC, r), 0x088DAEB0(+0x5AC, +0x5B0), 0x088DAF70(+0x5AC, +0x5B4), 0x088DA770(+0x5AC, 0x088627E4, a0) and
// 0x088DA77C(+0x5AC, 0x08861528, a0); 0x08896CB8(+0x5BC), 0x08917EE4(+0x5CC), 0x08917E44(+0x5CC, +0xD8) and
// 0x088D9E60(+0x5AC, +0x5CC) follow. 0x088C53C8(sp + 2) lands at +0x5F0 (0x0889867C(+0x5B4, r)), 0x088B1350(sp + 1,
// 0x088B1340(+0x5B4), +0x5AC) at +0x5F4 (0x08898684(+0x5B4, r)), and 0x088EF7B4(sp, 4) at +0x5F8: when it is not null its virtual
// +0x8 is called with (it, sp + 0xC), sp + 0xC / +0x10 / +0x14 holding +0x5AC / +0x5F0 / +0x5F4. +0x5AC and +0x5F8 go to
// 0x088DB5CC; 8 bytes (0x0889DA58(it, 4)) to +0x5FC, registered with 0x0889D8A0(it, +0x5F4, 0x088B1550) and, with +0x5F8, again
// with 0x08B8E500; 0x088A1408(+0x5B0, 0x0889D738, +0x5FC), 0x088DAF80(+0x5AC, a0 + 0x3FDC), 0x088DAF78(+0x5AC, a0 + 0x3FE4) and
// 0x088663A0(a0 + 0x401C) run. +0x45D8 takes +0x5AC, and a 0x38-byte object (sp + 8 = +0x5AC, sp + 4 = 0x08CBF368) goes through
// 0x0888E7BC(it, +0x5AC, +0x5CC, +0x3F88, +0x3F90, word(sp + 0x20), +0x600) into +0x5B8.
void func_08862C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x50u), G[30]); put(rt, frame.at(0x4Cu), G[23]); put(rt, frame.at(0x48u), G[22]); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); G[20] = G[4]; G[30] = G[20] + 0x3F88u; G[19] = G[20] + 0xD8u; G[23] = G[20] + 0x3F90u; G[18] = G[20] + 0xA0u; G[22] = G[20] + 0x600u; G[17] = G[20] + 0x5BCu; G[21] = G[20] + 0x5CCu; call_here(rt, ctx, 0x08866908u); G[4] = 0x08CA0000u; G[5] = 0x8u; G[4] = 0x08C9F5ACu; call_here(rt, ctx, 0x08ABF3CCu); put(rt, G[20] + 0x90u, G[2]); G[4] = G[2]; G[3] = word(rt, G[2] + 0x0u); G[2] = word(rt, G[3] + 0xCu); G[3] = G[20] + 0x3F80u; { const std::uint32_t tg = G[2]; put(rt, frame.at(0x20u), G[3]); call_here(rt, ctx, tg); } G[4] = 0x08CA0000u; G[5] = 0x8u; G[4] = 0x08C9F5C8u; call_here(rt, ctx, 0x08ABF3CCu); put(rt, G[20] + 0x94u, G[2]); G[4] = G[2]; G[3] = word(rt, G[2] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } G[4] = word(rt, G[20] + 0x94u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[4] = word(rt, G[20] + 0x90u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t tg = G[2]; G[5] = 0u; call_here(rt, ctx, tg); } G[4] = 0x110u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x0890BD30u); put(rt, G[20] + 0x98u, G[16]); store8(G[20] + 0x9Cu, 0u); put(rt, G[20] + 0x58u, G[18]); call_here(rt, ctx, 0x0885DDC8u); G[6] = G[2]; G[7] = 0u; G[5] = G[18]; G[4] = G[19]; call_here(rt, ctx, 0x0892BAA8u); G[4] = 0u; call_here(rt, ctx, 0x0892B71Cu); G[4] = G[2]; call_here(rt, ctx, 0x08B21938u); G[6] = 0x08860000u; put(rt, G[20] + 0x36Cu, G[2]); G[6] = 0x088612B8u; G[5] = 0u; G[4] = G[17]; put(rt, G[20] + 0x5BCu, G[20]); call_here(rt, ctx, 0x08896CC8u); G[6] = 0x08860000u; G[4] = G[17]; G[6] = 0x0885DEE8u; G[5] = 0x1u; call_here(rt, ctx, 0x08896CC8u); G[6] = 0x08860000u; G[6] = 0x0885DEF0u; G[5] = 0x2u; G[4] = G[17]; call_here(rt, ctx, 0x08896CC8u); G[4] = 0x788u; call_here(rt, ctx, 0x08B62344u); G[5] = word(rt, G[20] + 0x90u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x088E29DCu); G[6] = 0x1C38u; G[4] = G[16]; G[5] = 0u; put(rt, G[20] + 0x5ACu, G[16]); call_here(rt, ctx, 0x088D9C34u); G[4] = 0x64u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[5] = 0x4u; G[4] = G[2]; call_here(rt, ctx, 0x088A31B4u); put(rt, G[20] + 0x5B0u, G[16]); G[4] = 0x44u; call_here(rt, ctx, 0x08B62344u); G[4] = G[2]; G[16] = G[2]; call_here(rt, ctx, 0x088982C4u); G[2] = word(rt, G[20] + 0x5B0u); put(rt, G[20] + 0x5B4u, G[16]); G[4] = G[16]; G[5] = word(rt, G[2] + 0x54u); call_here(rt, ctx, 0x08898664u); G[5] = word(rt, G[20] + 0x5ACu); G[6] = word(rt, G[20] + 0x5B0u); G[7] = word(rt, G[20] + 0x5B4u); G[4] = word(rt, frame.at(0x20u)); G[9] = G[23]; G[8] = G[30]; call_here(rt, ctx, 0x088914B4u); G[4] = G[22]; call_here(rt, ctx, 0x0889364Cu); G[7] = word(rt, G[20] + 0x5B4u); G[6] = word(rt, G[20] + 0x5ACu); G[8] = G[22]; G[4] = G[20] + 0x3F58u; G[5] = G[19]; call_here(rt, ctx, 0x08865F04u); G[16] = word(rt, G[20] + 0x5ACu); call_here(rt, ctx, 0x08866174u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x088DA0ACu); G[4] = word(rt, G[20] + 0x5ACu); G[5] = word(rt, G[20] + 0x5B0u); call_here(rt, ctx, 0x088DAEB0u); G[4] = word(rt, G[20] + 0x5ACu); G[5] = word(rt, G[20] + 0x5B4u); call_here(rt, ctx, 0x088DAF70u); G[4] = word(rt, G[20] + 0x5ACu); G[5] = 0x08860000u; G[5] = 0x088627E4u; G[6] = G[20]; call_here(rt, ctx, 0x088DA770u); G[4] = word(rt, G[20] + 0x5ACu); G[5] = 0x08860000u; G[6] = G[20]; G[5] = 0x08861528u; call_here(rt, ctx, 0x088DA77Cu); G[4] = G[17]; call_here(rt, ctx, 0x08896CB8u); G[4] = G[21]; call_here(rt, ctx, 0x08917EE4u); G[4] = G[21]; G[5] = G[19]; call_here(rt, ctx, 0x08917E44u); G[4] = word(rt, G[20] + 0x5ACu); G[5] = G[21]; call_here(rt, ctx, 0x088D9E60u); G[4] = frame.at(0x2u); call_here(rt, ctx, 0x088C53C8u); G[4] = word(rt, G[20] + 0x5B4u); G[5] = G[2]; put(rt, G[20] + 0x5F0u, G[2]); call_here(rt, ctx, 0x0889867Cu); G[16] = word(rt, G[20] + 0x5ACu); call_here(rt, ctx, 0x088B1340u); G[5] = G[2]; G[6] = G[16]; G[4] = frame.at(0x1u); call_here(rt, ctx, 0x088B1350u); G[4] = word(rt, G[20] + 0x5B4u); G[5] = G[2]; put(rt, G[20] + 0x5F4u, G[2]); call_here(rt, ctx, 0x08898684u); G[4] = frame.at(0u); G[5] = 0x4u; call_here(rt, ctx, 0x088EF7B4u); G[7] = G[2]; { const bool tk = G[2] == 0u; put(rt, G[20] + 0x5F8u, G[2]); if (!tk) { G[2] = word(rt, G[20] + 0x5ACu); G[3] = word(rt, G[20] + 0x5F0u); G[6] = word(rt, G[20] + 0x5F4u); put(rt, frame.at(0xCu), G[2]); G[4] = G[7]; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(0x14u), G[6]); G[2] = word(rt, G[7] + 0x0u); G[3] = word(rt, G[2] + 0x8u); { const std::uint32_t tg = G[3]; G[5] = frame.at(0xCu); call_here(rt, ctx, tg); } } } G[5] = word(rt, G[20] + 0x5F8u); G[4] = word(rt, G[20] + 0x5ACu); call_here(rt, ctx, 0x088DB5CCu); G[4] = 0x8u; call_here(rt, ctx, 0x08B62344u); G[4] = G[2]; G[5] = 0x4u; G[16] = G[2]; call_here(rt, ctx, 0x0889DA58u); G[5] = word(rt, G[20] + 0x5F4u); G[6] = 0x088B0000u; put(rt, G[20] + 0x5FCu, G[16]); G[4] = G[16]; G[6] = 0x088B1550u; call_here(rt, ctx, 0x0889D8A0u); G[5] = word(rt, G[20] + 0x5F8u); { const bool tk = G[5] == 0u; G[6] = 0x08B90000u; if (!tk) { G[4] = word(rt, G[20] + 0x5FCu); G[6] = 0x08B8E500u; call_here(rt, ctx, 0x0889D8A0u); } } G[6] = word(rt, G[20] + 0x5FCu); G[4] = word(rt, G[20] + 0x5B0u); G[5] = 0x088A0000u; G[5] = 0x0889D738u; call_here(rt, ctx, 0x088A1408u); G[4] = word(rt, G[20] + 0x5ACu); G[5] = G[20] + 0x3FDCu; call_here(rt, ctx, 0x088DAF80u); G[4] = word(rt, G[20] + 0x5ACu); G[5] = G[20] + 0x3FE4u; call_here(rt, ctx, 0x088DAF78u); G[4] = G[20] + 0x401Cu; call_here(rt, ctx, 0x088663A0u); G[3] = word(rt, G[20] + 0x5ACu); G[2] = 0x08CC0000u; G[4] = 0x38u; put(rt, G[20] + 0x45D8u, G[3]); G[2] = 0x08CBF368u; put(rt, frame.at(0x8u), G[3]); put(rt, frame.at(0x4u), G[2]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[5] = word(rt, G[20] + 0x5ACu); G[9] = word(rt, frame.at(0x20u)); G[6] = G[21]; G[7] = G[30]; G[8] = G[23]; G[10] = G[22]; G[4] = G[2]; call_here(rt, ctx, 0x0888E7BCu); put(rt, G[20] + 0x5B8u, G[16]);  G[30] = word(rt, frame.at(0x50u)); G[23] = word(rt, frame.at(0x4Cu)); G[22] = word(rt, frame.at(0x48u)); G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); }  } }

void func_08AA30EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[16]); G[5] = 0x08CB0000u; G[5] = 0x08CB2BCCu; G[17] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = frame.at(0u); G[16] = G[2]; call_here(rt, ctx, 0x08A7E914u); G[5] = frame.at(0x4u); G[4] = G[17]; put(rt, frame.at(0x4u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2BDCu; G[6] = 0x08AA34F0u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2BE0u; G[6] = 0x08AA3568u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2BE4u; G[6] = 0x08AA35E4u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2BE8u; G[6] = 0x08AA3660u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2BECu; G[6] = 0x08AA36DCu; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2BF4u; G[6] = 0x08AA3758u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2BFCu; G[6] = 0x08AA37D4u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C04u; G[6] = 0x08AA3850u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C08u; G[6] = 0x08AA3928u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C0Cu; G[6] = 0x08AA39FCu; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C14u; G[6] = 0x08AA3AF8u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C1Cu; G[6] = 0x08AA3BE8u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C24u; G[6] = 0x08AA3C5Cu; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C28u; G[6] = 0x08AA3CD8u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C30u; G[6] = 0x08AA3D54u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C38u; G[6] = 0x08AA3DD0u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C40u; G[6] = 0x08AA3E4Cu; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C48u; G[6] = 0x08AA3EC8u; call_here(rt, ctx, 0x08B86FB0u); G[16] = word(rt, G[17] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08CB0000u; if (!tk) { G[4] = 0x08CB0000u; G[4] = 0x08CB2BD4u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CB0000u; } } G[6] = 0x08AA0000u; G[4] = G[16]; G[5] = 0x08CB2C50u; G[6] = 0x08AA30A0u; G[7] = 0u; call_here(rt, ctx, 0x08C307DCu); G[2] = G[17];  G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); }  } }
void func_08AA9D84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x5Cu), G[31]); put(rt, frame.at(0x58u), G[18]); put(rt, frame.at(0x54u), G[17]); put(rt, frame.at(0x50u), G[16]); G[17] = G[4]; G[17] = G[17] + 0x8u; call_here(rt, ctx, 0x08AA97D4u); G[2] = 0x08CE0000u; G[5] = word(rt, 0x08CDF804u); G[16] = frame.at(0x40u); G[6] = frame.at(0x15u); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD53A8u; put(rt, G[2] + 0x0u, G[3]); G[4] = 0x08D70000u; G[18] = 0x08D74FD0u; put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x40u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[18]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0xAu); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7FCu); G[16] = frame.at(0x3Cu); G[6] = frame.at(0x14u); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD53E8u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x3Cu)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x9u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7F8u); G[16] = frame.at(0x38u); G[6] = frame.at(0x13u); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD5408u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x38u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x8u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7F4u); G[16] = frame.at(0x34u); G[6] = frame.at(0x12u); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD5428u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x34u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x7u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7F0u); G[16] = frame.at(0x30u); G[6] = frame.at(0x11u); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD5448u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x30u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x6u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7DCu); G[16] = frame.at(0x2Cu); G[6] = frame.at(0x10u); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD54E8u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x2Cu)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x5u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7D4u); G[16] = frame.at(0x28u); G[6] = frame.at(0xFu); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD5528u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x28u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x4u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7D0u); G[16] = frame.at(0x24u); G[6] = frame.at(0xEu); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD5548u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x24u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x3u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7CCu); G[16] = frame.at(0x20u); G[6] = frame.at(0xDu); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD5568u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x20u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7C8u); G[16] = frame.at(0x1Cu); G[6] = frame.at(0xCu); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD55C8u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x1Cu)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = 0x08CE0000u; if (!tk) { G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[2] = 0x08CE0000u; } } } } G[5] = word(rt, 0x08CDF7C4u); G[16] = frame.at(0x18u); G[6] = frame.at(0xBu); G[4] = G[16]; call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08C338D4u); G[4] = 0x8u; G[16] = G[2]; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CD55A8u; put(rt, G[2] + 0x0u, G[3]); put(rt, G[2] + 0x4u, 0u); put(rt, G[16] + 0x0u, G[2]); G[4] = word(rt, frame.at(0x18u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[18] == G[16]; G[31] = word(rt, frame.at(0x5Cu)); if (!tk) { G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[31] = word(rt, frame.at(0x5Cu)); if (!tk) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } }  G[18] = word(rt, frame.at(0x58u)); G[17] = word(rt, frame.at(0x54u)); G[16] = word(rt, frame.at(0x50u)); }  } }
// 0x089D9080 (a0 = the object; v0 = the last registration call's): 0x08B85EB8(sp + 4, 0x08CA72D4, sp + 1) formats a name and the
// object's virtual +0x108 is called with it; the text is released unless it is the empty header 0x08D74FD0 (0x08B60804(text - 4,
// -1); with none left 0x08B8542C(header, sp)). 0x08A0F03C()'s result is released with 0x08A5BA08 and stored at +0x38, whose old
// value goes to 0x08A5B930 when non-null. Then ten registrations copy two 8-byte values from the constant table onto the frame and
// call target(object, name, first slot, second slot), followed by ten more with a single 8-byte value and
// target(object, name, slot): the tables below.
void func_089D9080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x110u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x10Cu}, {18u, 0x108u}, {17u, 0x104u}, {16u, 0x100u}});
    const u32 virtual_call = word(rt, word(rt, object) + 0x108u);
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x4u), 0x08CA72D4u, frame.at(0x1u)});
    call_set(rt, ctx, virtual_call, {object, frame.at(0x4u)});
    const u32 text = word(rt, frame.at(0x4u));
    const u32 header = text - 0xCu;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {text - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
    }
    const u32 sound = call_set(rt, ctx, 0x08A0F03Cu, {});
    if (sound != 0u) call_set(rt, ctx, 0x08A5BA08u, {sound});
    const u32 previous = word(rt, object + 0x38u);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, object + 0x38u, sound);
    struct Double {
        u32 first_source, second_source, first_slot, second_slot, name, target;
    };
    const Double doubles[10] = {
        {0x08C8D2F8u, 0x08C8D2F0u, 0xF0u, 0xE8u, 0x08CA71D8u, 0x08C04EF8u}, {0x08C8D2E8u, 0x08C8D2E0u, 0xE0u, 0xD8u, 0x08CA71E8u, 0x08C05158u},
        {0x08C8D2D8u, 0x08C8D2D0u, 0xD0u, 0xC8u, 0x08CA71FCu, 0x08C04A84u}, {0x08C8D2C8u, 0x08C8D2C0u, 0xC0u, 0xB8u, 0x08CA7208u, 0x08C04C00u},
        {0x08C8D2B8u, 0x08C8D2B0u, 0xB0u, 0xA8u, 0x08CA7214u, 0x08C04C00u}, {0x08C8D2A8u, 0x08C8D2A0u, 0xA0u, 0x98u, 0x08CA7228u, 0x08C04D7Cu},
        {0x08C8D298u, 0x08C8D290u, 0x90u, 0x88u, 0x08CA72E4u, 0x08C04C00u}, {0x08C8D288u, 0x08C8D280u, 0x80u, 0x78u, 0x08CA72F0u, 0x08C04C00u},
        {0x08C8D278u, 0x08C8D270u, 0x70u, 0x68u, 0x08CA72F8u, 0x08C04C00u}, {0x08C8D268u, 0x08C8D260u, 0x60u, 0x58u, 0x08CA7304u, 0x08C04A84u},
    };
    for (const Double &entry : doubles) {
        const u32 low = word(rt, entry.first_source), high = word(rt, entry.first_source + 0x4u);
        const u32 second_low = word(rt, entry.second_source), second_high = word(rt, entry.second_source + 0x4u);
        put(rt, frame.at(entry.first_slot), low);
        put(rt, frame.at(entry.first_slot + 0x4u), high);
        put(rt, frame.at(entry.second_slot), second_low);
        put(rt, frame.at(entry.second_slot + 0x4u), second_high);
        call_set(rt, ctx, entry.target, {object, entry.name, frame.at(entry.first_slot), frame.at(entry.second_slot)});
    }
    struct Single {
        u32 source, slot, name, target;
    };
    const Single singles[10] = {
        {0x08C8D258u, 0x50u, 0x08CA7314u, 0x08C0491Cu}, {0x08C8D250u, 0x48u, 0x08CA7328u, 0x08C05354u},
        {0x08C8D248u, 0x40u, 0x08CA733Cu, 0x08C0491Cu}, {0x08C8D240u, 0x38u, 0x08CA734Cu, 0x08C05354u},
        {0x08C8D238u, 0x30u, 0x08CA735Cu, 0x08C0491Cu}, {0x08C8D230u, 0x28u, 0x08CA7374u, 0x08C05354u},
        {0x08C8D228u, 0x20u, 0x08CA738Cu, 0x08C0491Cu}, {0x08C8D220u, 0x18u, 0x08CA7398u, 0x08C05354u},
        {0x08C8D218u, 0x10u, 0x08CA73A4u, 0x08C0491Cu}, {0x08C8D210u, 0x8u, 0x08CA73B0u, 0x08C05354u},
    };
    for (const Single &entry : singles) {
        const u32 low = word(rt, entry.source);
        put(rt, frame.at(entry.slot), low);
        put(rt, frame.at(entry.slot + 0x4u), word(rt, entry.source + 0x4u));
        // a3 carries the same low word in the original (a leftover the callee ignores)
        call_set(rt, ctx, entry.target, {object, entry.name, frame.at(entry.slot), low});
    }
}

void func_088072F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x1C0u); put(rt, frame.at(0x190u), G[16]); put(rt, frame.at(0x1B4u), G[31]); put(rt, frame.at(0x1B0u), G[30]); put(rt, frame.at(0x1ACu), G[23]); put(rt, frame.at(0x1A8u), G[22]); put(rt, frame.at(0x1A4u), G[21]); put(rt, frame.at(0x1A0u), G[20]); put(rt, frame.at(0x19Cu), G[19]); put(rt, frame.at(0x198u), G[18]); put(rt, frame.at(0x194u), G[17]); G[5] = 0x08C90000u; G[6] = 0x00010000u; put(rt, frame.at(0x180u), G[4]); G[5] = 0x08C97E60u; G[6] = G[6] | 0x488u; G[4] = frame.at(0x168u); call_here(rt, ctx, 0x08B86F3Cu); G[2] = word(rt, frame.at(0x180u)); G[16] = word(rt, G[2] + 0x0u); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08C90000u; G[4] = 0x08C97E68u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = frame.at(0x168u); call_here(rt, ctx, 0x08A7CE50u); G[5] = 0x08C90000u; G[5] = 0x08C97E70u; G[4] = frame.at(0x16Cu); G[6] = 0xF0u; G[23] = frame.at(0x17Cu); G[30] = 0x08C90000u; call_here(rt, ctx, 0x08B86F3Cu); G[4] = G[23]; G[5] = 0x08C97E7Cu; G[6] = 0x18u; call_here(rt, ctx, 0x08B86F3Cu); G[19] = word(rt, frame.at(0x17Cu)); { const bool tk = G[19] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[18] = frame.at(0x164u); G[5] = 0x08C97E88u; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0xACu); G[17] = G[2]; G[2] = 0x08C90000u; G[5] = 0x08C97E8Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[3] = 0x08CB0000u; G[2] = 0x08CB7E20u; G[16] = frame.at(0x160u); put(rt, G[17] + 0x0u, G[2]); G[4] = G[16]; G[5] = frame.at(0xB0u); put(rt, frame.at(0xB0u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[19]; G[5] = G[18]; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08A535A4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[19] = word(rt, frame.at(0x17Cu)); { const bool tk = G[19] != 0u; G[18] = frame.at(0x15Cu); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[18] = frame.at(0x15Cu); } } G[5] = 0x08C90000u; G[5] = 0x08C97E94u; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0xA4u); G[17] = G[2]; G[2] = 0x08C90000u; G[5] = 0x08C97E9Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[3] = 0x08CB0000u; G[2] = 0x08CB79B8u; G[16] = frame.at(0x158u); put(rt, G[17] + 0x0u, G[2]); G[4] = G[16]; G[5] = frame.at(0xA8u); put(rt, frame.at(0xA8u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[19]; G[5] = G[18]; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0x8u; call_here(rt, ctx, 0x08A535A4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[19] = word(rt, frame.at(0x17Cu)); { const bool tk = G[19] != 0u; G[18] = frame.at(0x154u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[18] = frame.at(0x154u); } } G[2] = 0x08C90000u; G[5] = 0x08C97EA4u; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x9Cu); G[22] = 0x08C90000u; G[17] = G[2]; G[4] = G[16]; G[5] = 0x08C97EACu; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[2] = 0x08CB0000u; G[21] = 0x08CB7840u; G[16] = frame.at(0x150u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[16]; G[5] = frame.at(0xA0u); put(rt, frame.at(0xA0u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = G[18]; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0xCu; G[4] = G[19]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x17Cu)); { const bool tk = G[20] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[19] = frame.at(0x14Cu); G[5] = 0x08C97EB4u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x94u); G[17] = G[2]; G[4] = G[16]; G[5] = 0x08C97EACu; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0x148u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[18]; G[5] = frame.at(0x98u); put(rt, frame.at(0x98u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0xDu; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x17Cu)); { const bool tk = G[20] != 0u; G[19] = frame.at(0x144u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = frame.at(0x144u); } } G[2] = 0x08C90000u; G[5] = 0x08C97EBCu; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x8Cu); G[17] = G[2]; G[4] = G[16]; G[5] = 0x08C97EACu; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0x140u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[18]; G[5] = frame.at(0x90u); put(rt, frame.at(0x90u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0xEu; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x17Cu)); { const bool tk = G[20] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[19] = frame.at(0x13Cu); G[5] = 0x08C97EC8u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x84u); G[17] = G[2]; G[4] = G[16]; G[5] = 0x08C97EACu; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0x138u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[18]; G[5] = frame.at(0x88u); put(rt, frame.at(0x88u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0xFu; G[4] = G[20]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[19] = word(rt, frame.at(0x17Cu)); { const bool tk = G[19] != 0u; G[18] = frame.at(0x134u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[18] = frame.at(0x134u); } } G[2] = 0x08C90000u; G[4] = G[18]; G[5] = 0x08C97ED0u; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x7Cu); G[3] = 0x08C90000u; G[17] = G[2]; G[5] = 0x08C97ED8u; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[3] = 0x08CB0000u; G[2] = 0x08CB7CA8u; G[16] = frame.at(0x130u); put(rt, G[17] + 0x0u, G[2]); G[4] = G[16]; G[5] = frame.at(0x80u); put(rt, frame.at(0x80u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = G[18]; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0x10u; G[4] = G[19]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[18] = word(rt, frame.at(0x16Cu)); { const bool tk = G[18] != 0u; if (tk) { G[4] = 0x44u; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = 0x44u; } } G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = frame.at(0x12Cu); G[6] = 0xAu; G[5] = G[23]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[4] = G[16]; G[5] = frame.at(0x78u); put(rt, frame.at(0x78u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[2] = 0x08C90000u; G[6] = G[16]; G[4] = G[18]; G[5] = 0x08C97EE0u; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08B86BE8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x16Cu)); { const bool tk = G[16] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[6] = 0x08800000u; G[5] = 0x08C97EE8u; G[6] = 0x08806C44u; G[4] = G[16]; call_here(rt, ctx, 0x08B87108u); G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = 0x08C90000u; G[5] = 0x08C97EF0u; G[6] = 0x1E0u; G[4] = frame.at(0x170u); call_here(rt, ctx, 0x08B86F3Cu); G[4] = G[23]; G[5] = 0x08C97E7Cu; G[6] = 0x18u; call_here(rt, ctx, 0x08B86F3Cu); G[16] = word(rt, frame.at(0x170u)); { const bool tk = G[16] != 0u; if (tk) { G[4] = G[16]; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; } } G[5] = G[23]; call_here(rt, ctx, 0x08A7CE50u); G[19] = word(rt, frame.at(0x17Cu)); { const bool tk = G[19] != 0u; G[18] = frame.at(0x128u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[18] = frame.at(0x128u); } } G[2] = 0x08C90000u; G[5] = 0x08C97E88u; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x70u); G[3] = 0x08C90000u; G[17] = G[2]; G[5] = 0x08C97E8Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[3] = 0x08CB0000u; G[2] = 0x08CB7E20u; G[16] = frame.at(0x124u); put(rt, G[17] + 0x0u, G[2]); G[4] = G[16]; G[5] = frame.at(0x74u); put(rt, frame.at(0x74u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[19]; G[5] = G[18]; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08A535A4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[19] = word(rt, frame.at(0x17Cu)); { const bool tk = G[19] != 0u; G[18] = frame.at(0x120u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[18] = frame.at(0x120u); } } G[2] = 0x08C90000u; G[5] = 0x08C97EFCu; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x68u); G[3] = 0x08C90000u; G[5] = 0x08C97E9Cu; G[17] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[2] = 0x08CB0000u; G[30] = 0x08CB79B8u; G[4] = G[16]; G[16] = frame.at(0x11Cu); call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[30]); G[4] = G[16]; G[5] = frame.at(0x6Cu); put(rt, frame.at(0x6Cu), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = G[18]; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0x8u; G[4] = G[19]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x17Cu)); { const bool tk = G[20] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[19] = frame.at(0x118u); G[5] = 0x08C97EA4u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x60u); G[17] = G[2]; G[4] = G[16]; G[5] = 0x08C97EACu; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0x114u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[18]; G[5] = frame.at(0x64u); put(rt, frame.at(0x64u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0xCu; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x17Cu)); { const bool tk = G[20] != 0u; G[19] = frame.at(0x110u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = frame.at(0x110u); } } G[2] = 0x08C90000u; G[5] = 0x08C97EB4u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x58u); G[17] = G[2]; G[4] = G[16]; G[5] = 0x08C97EACu; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0x10Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[18]; G[5] = frame.at(0x5Cu); put(rt, frame.at(0x5Cu), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0xDu; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x17Cu)); { const bool tk = G[20] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[19] = frame.at(0x108u); G[5] = 0x08C97EBCu; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x50u); G[17] = G[2]; G[4] = G[16]; G[5] = 0x08C97EACu; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0x104u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[18]; G[5] = frame.at(0x54u); put(rt, frame.at(0x54u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0xEu; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x17Cu)); { const bool tk = G[20] != 0u; G[19] = frame.at(0x100u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = frame.at(0x100u); } } G[2] = 0x08C90000u; G[5] = 0x08C97EC8u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x48u); G[17] = G[2]; G[5] = 0x08C97EACu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0xFCu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[21]); G[4] = G[18]; G[5] = frame.at(0x4Cu); put(rt, frame.at(0x4Cu), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0xFu; G[4] = G[20]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[19] = word(rt, frame.at(0x17Cu)); { const bool tk = G[19] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[18] = frame.at(0xF8u); G[5] = 0x08C97ED0u; G[4] = G[18]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x40u); G[17] = G[2]; G[2] = 0x08C90000u; G[5] = 0x08C97ED8u; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[3] = 0x08CB0000u; G[2] = 0x08CB7CA8u; G[16] = frame.at(0xF4u); put(rt, G[17] + 0x0u, G[2]); G[4] = G[16]; G[5] = frame.at(0x44u); put(rt, frame.at(0x44u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = G[18]; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0x10u; G[4] = G[19]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A69D68u); G[18] = word(rt, frame.at(0x170u)); { const bool tk = G[18] != 0u; if (tk) { G[4] = 0x44u; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = 0x44u; } } G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[6] = 0xAu; G[16] = frame.at(0xF0u); G[5] = G[23]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[5] = frame.at(0x3Cu); G[4] = G[16]; put(rt, frame.at(0x3Cu), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[2] = 0x08C90000u; G[4] = G[18]; G[5] = 0x08C97EE0u; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08B86BE8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, frame.at(0x170u)); { const bool tk = G[18] != 0u; if (tk) { G[4] = 0x44u; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = 0x44u; } } G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = frame.at(0xECu); G[6] = 0xAu; G[5] = G[23]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[4] = G[16]; G[5] = frame.at(0x38u); put(rt, frame.at(0x38u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = 0x08C90000u; G[6] = G[16]; G[4] = G[18]; G[5] = 0x08C97F04u; G[7] = 0xFFFFFFFFu; G[8] = 0xF0u; call_here(rt, ctx, 0x08B86BE8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, frame.at(0x170u)); { const bool tk = G[16] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[6] = 0x08800000u; G[5] = 0x08C97EE8u; G[6] = 0x088068B0u; G[4] = G[16]; call_here(rt, ctx, 0x08B87108u); G[4] = G[23]; call_here(rt, ctx, 0x08A5B9ECu); G[22] = frame.at(0x174u); G[5] = 0x08C90000u; G[5] = 0x08C97F14u; G[4] = G[22]; G[6] = 0x8u; call_here(rt, ctx, 0x08B86F3Cu); G[20] = word(rt, frame.at(0x174u)); { const bool tk = G[20] != 0u; G[19] = frame.at(0xE8u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = frame.at(0xE8u); } } G[5] = 0x08C90000u; G[5] = 0x08C97F20u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[17] = G[2]; G[16] = frame.at(0x30u); G[2] = 0x08C90000u; G[5] = 0x08C97E9Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0xE4u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[30]); G[4] = G[18]; G[5] = frame.at(0x34u); put(rt, frame.at(0x34u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x174u)); { const bool tk = G[20] != 0u; G[19] = frame.at(0xE0u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = frame.at(0xE0u); } } G[5] = 0x08C90000u; G[5] = 0x08C97F28u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x28u); G[3] = 0x08C90000u; G[17] = G[2]; G[5] = 0x08C97E9Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0xDCu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[30]); G[4] = G[18]; G[5] = frame.at(0x2Cu); put(rt, frame.at(0x2Cu), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0x4u; G[4] = G[20]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[16] = word(rt, frame.at(0x174u)); { const bool tk = G[16] != 0u; G[5] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08C90000u; } } G[6] = 0x08800000u; G[4] = G[16]; G[5] = 0x08C97F34u; G[6] = 0x08806730u; call_here(rt, ctx, 0x08B87108u); G[21] = frame.at(0x178u); G[5] = 0x08C90000u; G[5] = 0x08C97F40u; G[4] = G[21]; G[6] = 0xCu; call_here(rt, ctx, 0x08B86F3Cu); G[20] = word(rt, frame.at(0x178u)); { const bool tk = G[20] != 0u; G[19] = frame.at(0xD8u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = frame.at(0xD8u); } } G[5] = 0x08C90000u; G[5] = 0x08C97F4Cu; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[17] = G[2]; G[16] = frame.at(0x20u); G[2] = 0x08C90000u; G[5] = 0x08C97E9Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0xD4u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[30]); G[4] = G[18]; G[5] = frame.at(0x24u); put(rt, frame.at(0x24u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x178u)); { const bool tk = G[20] != 0u; G[3] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = 0x08C90000u; } } G[19] = frame.at(0xD0u); G[5] = 0x08C97EFCu; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[17] = G[2]; G[16] = frame.at(0x18u); G[2] = 0x08C90000u; G[5] = 0x08C97E9Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0xCCu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); put(rt, G[17] + 0x0u, G[30]); G[4] = G[18]; G[5] = frame.at(0x1Cu); put(rt, frame.at(0x1Cu), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[4] = G[20]; G[5] = G[19]; G[6] = G[18]; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[20] = word(rt, frame.at(0x178u)); { const bool tk = G[20] != 0u; G[19] = frame.at(0xC8u); if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[19] = frame.at(0xC8u); } } G[5] = 0x08C90000u; G[5] = 0x08C97F54u; G[4] = G[19]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x38u; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[16] = frame.at(0x10u); G[3] = 0x08C90000u; G[17] = G[2]; G[5] = 0x08C97E9Cu; G[4] = G[16]; call_here(rt, ctx, 0x08A69B3Cu); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A52660u); G[18] = frame.at(0xC4u); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[4] = G[18]; put(rt, G[17] + 0x0u, G[30]); G[5] = frame.at(0x14u); put(rt, frame.at(0x14u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[6] = G[18]; G[5] = G[19]; G[7] = 0xFFFFFFFFu; G[8] = 0u; G[4] = G[20]; call_here(rt, ctx, 0x08A535A4u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[19]; call_here(rt, ctx, 0x08A69D68u); G[4] = G[21]; call_here(rt, ctx, 0x088061D8u); G[18] = word(rt, frame.at(0x168u)); { const bool tk = G[18] != 0u; if (tk) { G[4] = 0x44u; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = 0x44u; } } G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[6] = 0x5Au; G[16] = frame.at(0xC0u); G[5] = frame.at(0x16Cu); G[17] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[5] = frame.at(0xCu); G[4] = G[16]; put(rt, frame.at(0xCu), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = 0x08C90000u; G[4] = G[18]; G[5] = 0x08C97E48u; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0u; call_here(rt, ctx, 0x08B86BE8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, frame.at(0x168u)); { const bool tk = G[18] != 0u; if (tk) { G[4] = 0x44u; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = 0x44u; } } G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[6] = 0x5Au; G[16] = frame.at(0xBCu); G[5] = frame.at(0x170u); G[17] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[5] = frame.at(0x8u); G[4] = G[16]; put(rt, frame.at(0x8u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = 0x08C90000u; G[4] = G[18]; G[5] = 0x08C97E54u; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0x5460u; call_here(rt, ctx, 0x08B86BE8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, frame.at(0x168u)); { const bool tk = G[18] != 0u; if (tk) { G[4] = 0x44u; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = 0x44u; } } G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[6] = 0x66u; G[16] = frame.at(0xB8u); G[5] = G[22]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[5] = frame.at(0x4u); G[4] = G[16]; put(rt, frame.at(0x4u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = 0x08C90000u; G[4] = G[18]; G[5] = 0x08C97F68u; G[6] = G[16]; G[7] = 0xFFFFFFFFu; G[8] = 0u | 0xFD20u; call_here(rt, ctx, 0x08B86BE8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[18] = word(rt, frame.at(0x168u)); { const bool tk = G[18] != 0u; if (tk) { G[4] = 0x44u; } else { G[4] = 0x08C90000u; G[4] = 0x08C97DCCu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = 0x44u; } } G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[16] = frame.at(0xB4u); G[6] = 0x5Au; G[5] = G[21]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B70Cu); G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0x0u), G[17]); call_here(rt, ctx, 0x08A52AB0u); G[5] = 0x08C90000u; G[8] = 0x00010000u; G[6] = G[16]; G[5] = 0x08C97DE4u; G[8] = G[8] | 0x50u; G[7] = 0xFFFFFFFFu; G[4] = G[18]; call_here(rt, ctx, 0x08B86BE8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0x168u); call_here(rt, ctx, 0x088062D0u); G[3] = word(rt, frame.at(0x168u)); G[2] = 0x08CE0000u; put(rt, 0x08CE28E4u, G[3]); G[2] = word(rt, frame.at(0x180u)); G[16] = word(rt, G[2] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97E68u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08C90000u; } } G[6] = 0x08800000u; G[4] = G[16]; G[5] = 0x08C97F74u; G[6] = 0x08805EFCu; call_here(rt, ctx, 0x08B86FB0u); G[3] = word(rt, frame.at(0x180u)); G[16] = word(rt, G[3] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97E68u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08C90000u; } } G[6] = 0x08800000u; G[4] = G[16]; G[5] = 0x08C97F8Cu; G[6] = 0x08805FD0u; call_here(rt, ctx, 0x08B86FB0u); G[2] = word(rt, frame.at(0x180u)); G[16] = word(rt, G[2] + 0x0u); { const bool tk = G[16] != 0u; G[5] = 0x08C90000u; if (!tk) { G[4] = 0x08C90000u; G[4] = 0x08C97E68u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08C90000u; } } G[6] = 0x08800000u; G[5] = 0x08C97FA8u; G[6] = 0x08806384u; G[4] = G[16]; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0x170u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0x16Cu); call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0x168u); call_here(rt, ctx, 0x08A5B9ECu);  G[16] = word(rt, frame.at(0x190u)); G[30] = word(rt, frame.at(0x1B0u)); G[23] = word(rt, frame.at(0x1ACu)); G[22] = word(rt, frame.at(0x1A8u)); G[21] = word(rt, frame.at(0x1A4u)); G[20] = word(rt, frame.at(0x1A0u)); G[19] = word(rt, frame.at(0x19Cu)); G[18] = word(rt, frame.at(0x198u)); G[17] = word(rt, frame.at(0x194u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_316
} // namespace gt::game

#define HOT316(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_316::name, gt::ReturnKind::kind)
HOT316(0x0884C03Cu, func_0884C03C, Word);
// HOT316(0x0884C244u, func_0884C244, Word);  // no comparable synthetic case yet
HOT316(0x08A8E85Cu, func_08A8E85C, Word);
// HOT316(0x08AA15C0u, func_08AA15C0, Word);  // no comparable synthetic case yet
HOT316(0x088EAD68u, func_088EAD68, Word);
HOT316(0x08818F28u, func_08818F28, Word);
HOT316(0x08A30AC8u, func_08A30AC8, Word);
HOT316(0x08A957C4u, func_08A957C4, Word);
// HOT316(0x08A14D94u, func_08A14D94, Word);  // no comparable synthetic case yet
HOT316(0x0899B224u, func_0899B224, Word);
HOT316(0x089B7A08u, func_089B7A08, Word);
HOT316(0x088CD73Cu, func_088CD73C, Word);
HOT316(0x089F7298u, func_089F7298, Word);
HOT316(0x088057F8u, func_088057F8, Word);
HOT316(0x08A1AEDCu, func_08A1AEDC, Word);
// HOT316(0x08862C90u, func_08862C90, Word);  // no comparable synthetic case yet
HOT316(0x08AA30ECu, func_08AA30EC, Word);
// HOT316(0x08AA9D84u, func_08AA9D84, Word);  // no comparable synthetic case yet
HOT316(0x089D9080u, func_089D9080, Word);
// HOT316(0x088072F8u, func_088072F8, Word);  // no comparable synthetic case yet
