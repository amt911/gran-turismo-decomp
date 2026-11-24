// Hot batch 269: fifty-nine- and sixty-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (handle-guarded calls through temporaries, a variable-length integer encoder,
// a flag-table parser, float rounding by bit masks, unsigned-to-float conversion loops and a lock with retries).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_269 {
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

void func_0883F1FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[18]); G[18] = G[4]; put(rt, frame.at(0x30u), G[16]); G[16] = G[6]; put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x34u), G[17]); if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = frame.at(0x10u); G[4] = G[17]; call_here(rt, ctx, 0x08931B74u); G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08C9BD48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[5] = word(rt, frame.at(8u)); G[4] = G[17]; call_here(rt, ctx, 0x08931E54u); G[4] = word(rt, frame.at(8u)); if (G[2] != 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08931790u); G[16] = frame.at(4u); G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); G[5] = word(rt, frame.at(4u)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); } G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[17]; call_here(rt, ctx, 0x08931C38u); } G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_08895D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(4u), G[17]); G[17] = G[4]; G[4] = 0x44u; put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); G[18] = G[17] + 0x1Cu; put(rt, frame.at(0u), G[16]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x0892B440u); G[4] = G[16] + 0x20u; call_here(rt, ctx, 0x0892B440u); put(rt, G[16] + 0x40u, 0u); G[3] = 0x08D70000u; G[2] = 0x08D70000u; put(rt, G[17] + 0x1Cu, G[16]); G[19] = 0x08D75EE0u; G[16] = 0x08D75ED8u; G[2] = sext(load8(0x08D75ED8u), 8); G[4] = G[16]; if (G[2] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[5] = 0x08D70000u; G[19] = 0x08D75EE0u; G[4] = G[16]; if (G[2] != 0u) { G[2] = 0x08CC1F30u; put(rt, 0x08D75EE0u, G[2]); call_here(rt, ctx, 0x08B64494u); } } G[16] = G[17] + 0x28u; put(rt, G[18] + 4u, G[19]); G[4] = G[16]; call_here(rt, ctx, 0x08B1C920u); G[2] = word(rt, G[17] + 0x28u); put(rt, G[17] + 0x14u, 0u); put(rt, G[17] + 4u, 0u); put(rt, G[17], 0u); put(rt, G[17] + 8u, 0u); put(rt, G[17] + 0xCu, 0u); put(rt, G[17] + 0x24u, 0u); store8(G[17] + 0x18u, 0u); put(rt, G[16] + 4u, G[2]); put(rt, G[16] + 0xCu, 0u); store8(G[16] + 8u, 0u); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_088BCA94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { auto &F = ctx.fpr; G[3] = word(rt, G[4] + 0x14u); G[5] = word(rt, G[4] + 8u); G[4] = word(rt, G[4] + 0x24u); G[2] = load8(G[3]); F[3] = float_at(rt, G[5] + 0x4F4u); if (G[2] != 0u) G[3] = load16(G[4] + 2u); else { G[2] = load8(G[4] + 0x12u); G[2] -= 1u; G[2] &= 0xFFu; G[2] = G[2] < 2u ? 1u : 0u; G[3] = 0u; if (G[2] != 0u) { G[2] = sext(load8(G[4] + 0x96u), 8); G[3] = G[2] != 0u ? 1u : 0u; } F[2] = float_at(rt, G[4] + 0x28u); if (G[3] == 0u) { G[3] = load16(G[4] + 2u); put_float(rt, G[4] + 0x28u, F[2]); } else { F[1] = float_at(rt, G[5] + 0x530u); const bool lt = F[1] < F[2]; G[3] = load16(G[4] + 2u); if (lt) { F[0] = F[3] + F[1]; if (!(F[2] < F[0])) F[2] = F[0]; put_float(rt, G[4] + 0x28u, F[2]); } else { G[2] = G[3] & 0x100u; if (G[2] != 0u) put_float(rt, G[4] + 0x28u, F[2]); else { G[1] = 0xC0000000u; F[0] = std::bit_cast<float>(G[1]); F[0] = F[3] * F[0]; F[0] = F[1] + F[0]; if (!(F[0] < F[2])) F[2] = F[0]; put_float(rt, G[4] + 0x28u, F[2]); } } } } G[2] = sext(load16(G[4] + 0x38u), 16); if (static_cast<std::int32_t>(G[2]) > 0) { G[2] -= 1u; store16(G[4] + 0x38u, G[2]); } else G[2] -= 1u; G[2] = G[3] & 8u; if (G[2] != 0u) { G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[4] + 0x30u, F[0]); } } }
// 0x088CF328 (a0 = the object; v0 = 4, the +0x95 byte, 1, or the last call's): in mode 2 (the object's word +0x12C) the counter at +0x128 is advanced; past the
// limit at +0x124 it restarts at 0 when the object's byte +0x94 is set, and otherwise the mode becomes 4 and the function ends. With the object's byte +0x95
// set the +0x13C float becomes 100.0 and +0x138 is zeroed. Otherwise 0x088FE2FC(object's word +0xC) gives a handle (null ends the function) and the mode picks
// the reading: 0 or 2 take 0x088FDC78(handle, +0x128) into +0x138 and 0x088FDCF4(handle, +0x128) into +0x13C, mode 1 takes 0x088FDB9C(handle, +0x128) into
// +0x138 and clears the object's word +0xE4; any other mode leaves v0 = 1.
void func_088CF328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x8u}, {17u, 0x4u}});
    ctx.gpr[16] = self;
    if (word(rt, self + 0x12Cu) == 2u) {
        const u32 next = word(rt, self + 0x128u) + 1u;
        const bool below = static_cast<s32>(next) < static_cast<s32>(word(rt, self + 0x124u));
        put(rt, self + 0x128u, next);
        if (!below) {
            if (load8(self + 0x94u) == 0u) {
                put(rt, self + 0x12Cu, 4u);
                ctx.gpr[2] = 4u;
                return;
            }
            put(rt, self + 0x128u, 0u);
        }
    }
    const u32 quiet = load8(self + 0x95u);
    if (quiet != 0u) {
        put_float(rt, self + 0x13Cu, 100.0f);
        put(rt, self + 0x138u, 0u);
        ctx.gpr[2] = quiet;
        return;
    }
    const u32 handle = call_set(rt, ctx, 0x088FE2FCu, {word(rt, self + 0xCu)});
    ctx.gpr[17] = handle;
    if (handle == 0u) return;
    const u32 mode = word(rt, self + 0x12Cu);
    if (mode == 0u || mode == 2u) {
        call_set(rt, ctx, 0x088FDC78u, {handle, word(rt, self + 0x128u)});
        put_float(rt, self + 0x138u, ctx.fpr[0]);
        call_set(rt, ctx, 0x088FDCF4u, {handle, word(rt, self + 0x128u)});
        put_float(rt, self + 0x13Cu, ctx.fpr[0]);
        return;
    }
    if (mode != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    call_set(rt, ctx, 0x088FDB9Cu, {handle, word(rt, self + 0x128u)});
    put_float(rt, self + 0x138u, ctx.fpr[0]);
    put(rt, self + 0xE4u, 0u);
}
void func_08928794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); auto &F = ctx.fpr; put(rt, frame.at(0x34u), G[17]); G[17] = frame.at(8u); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x38u), G[31]); G[16] = G[4]; call_here(rt, ctx, 0x089277BCu); G[3] = 3u; G[4] = G[16]; if (G[2] != G[3]) { call_here(rt, ctx, 0x089277B4u); F[0] = float_at(rt, G[2] + 0xA8u); } else { for (std::uint32_t off = 0x14u; off <= 0x20u; off += 4u) put(rt, frame.at(off), 0u); call_here(rt, ctx, 0x089277B4u); G[4] = G[2]; G[5] = frame.at(0x14u); call_here(rt, ctx, 0x08926AB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08928194u); F[3] = float_at(rt, frame.at(8u)); F[0] = float_at(rt, frame.at(0x14u)); F[2] = float_at(rt, frame.at(0xCu)); F[1] = float_at(rt, frame.at(0x10u)); F[3] = F[3] - F[0]; F[0] = float_at(rt, frame.at(0x18u)); G[4] = G[17]; F[2] = F[2] - F[0]; F[0] = float_at(rt, frame.at(0x1Cu)); put_float(rt, frame.at(8u), F[3]); F[1] = F[1] - F[0]; G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0xCu), F[2]); put_float(rt, frame.at(0x10u), F[1]); put_float(rt, frame.at(4u), F[0]); call_here(rt, ctx, 0x08B4A720u); F[0] = std::sqrt(F[0]); G[1] = 0x40000000u; F[1] = std::bit_cast<float>(G[1]); F[2] = float_at(rt, frame.at(4u)); G[2] = frame.at(0u); F[0] = F[0] - F[1]; const bool lt = F[2] < F[0]; put_float(rt, frame.at(0u), F[0]); if (!lt) G[2] = frame.at(4u); F[0] = float_at(rt, G[2]); } G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
// 0x08958F20 (a0 = the object): loops over the count at object+8, calling 0x0894CF40(object, index) and, when the
// element's +8 is non-null, 0x0894CFF8(object, index) then reading the +3 byte of its +8 word. When that byte is 4, it
// calls the object's +0x1C virtual slot with (index, frame) and, when that returns non-zero, 0x08958D40/0x08959C9C; then
// 0x08958D40/0x0895A2F8 run with the element. v0 = the last call's result.
void func_08958F20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x40u}, {31u, 0x54u}, {20u, 0x50u}, {19u, 0x4Cu}, {18u, 0x48u}, {17u, 0x44u}});
    const u32 count = word(rt, object + 8u);
    ctx.gpr[19] = count;
    ctx.gpr[16] = object;
    if (static_cast<s32>(count) > 0) {
        u32 index = 0u;
        ctx.gpr[20] = 4u;
        ctx.gpr[5] = index;
        do {
            call_set(rt, ctx, 0x0894CF40u, {object, index});
            const u32 held = word(rt, ctx.gpr[2] + 8u);
            if (held != 0u) {
                call_set(rt, ctx, 0x0894CFF8u, {object, index});
                const u32 entry = ctx.gpr[2];
                ctx.gpr[18] = entry;
                const u32 value = word(rt, entry + 8u);
                const u32 kind = sext(load8(value + 3u), 8);
                if (kind == 4u) {
                    const u32 target = word(rt, object + 0x1Cu);
                    ctx.gpr[6] = frame.at(0u);
                    if (target != 0u) {
                        const u32 slot = word(rt, word(rt, target) + 0xCu);
                        call_set(rt, ctx, slot, {target, index, frame.at(0u)});
                        if (ctx.gpr[2] != 0u) {
                            call_set(rt, ctx, 0x08958D40u, {object});
                            call_set(rt, ctx, 0x08959C9Cu, {entry, frame.at(0u), ctx.gpr[2]});
                        }
                    }
                    call_set(rt, ctx, 0x08958D40u, {object});
                    call_set(rt, ctx, 0x0895A2F8u, {entry, ctx.gpr[2]});
                }
            }
            index += 1u;
            ctx.gpr[17] = index;
            ctx.gpr[5] = index;
        } while (count != index);
    }
}
void func_089ACBB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[31]); G[3] = word(rt, G[5] + 0xC0u); G[16] = G[5]; bool plain = true; if (G[3] != 0u) { G[2] = load8(G[5] + 0xFCu); G[5] = 0x08C90000u; if (G[2] != 0u) { plain = false; G[4] = G[3]; G[5] = frame.at(8u); G[6] = frame.at(0xCu); call_here(rt, ctx, 0x089DFAACu); G[2] = word(rt, G[16] + 0xF0u); bool f0 = G[2] != 0u; G[2] = word(rt, G[16] + 0xF4u); G[5] = 0x08CA0000u; std::uint32_t off; if (!f0) { if (G[2] != 0u) { G[5] = 0x08CA5114u; off = 3u; } else { G[5] = 0x08CA510Cu; off = 4u; } } else { if (G[2] != 0u) { G[5] = 0x08CA5128u; off = 1u; } else { G[5] = 0x08CA511Cu; off = 2u; } } G[4] = G[17]; G[6] = frame.at(off); call_here(rt, ctx, 0x08B85EB8u); } } else G[5] = 0x08C90000u; if (plain) { G[5] = 0x08C8C74Cu; G[4] = G[17]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B85EB8u); } G[2] = G[17]; G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
// 0x08A205FC (a0 = the object, a1 = a count, a2 = a handle): with a count above 0, 0x08AC32A4(object, count, handle) is tried and a nonzero answer
// takes the handle's first word (0x08A5BB7C(0x08CAA6B4) when null) and runs its virtual +0xC(frame + 8, node); 0x08AC4050(answer, word(frame + 8))
// then closes the text, whose counted string at frame + 8 is released through 0x08B60804 / 0x08B8542C unless its header is 0x08D74FD0. Finally the
// buffer at frame + 4 is filled by 0x08A97A6C and moved into the object with 0x08A5BAD8 / 0x08A5B9EC.
void func_08A205FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], count = ctx.gpr[5], handle = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {16u, 0x10u}, {31u, 0x1Cu}, {17u, 0x14u}});
    ctx.gpr[18] = object;
    ctx.gpr[16] = handle;
    if (static_cast<s32>(count) <= 0) return;
    const u32 found = call_set(rt, ctx, 0x08AC32A4u, {object, count, handle});
    ctx.gpr[17] = found;
    if (found != 0u) {
        u32 node = word(rt, handle);
        ctx.gpr[16] = node;
        if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA6B4u});
        call_set(rt, ctx, word(rt, word(rt, node) + 0xCu), {frame.at(0x8u), node});
        const u32 result = call_set(rt, ctx, 0x08AC4050u, {found, word(rt, frame.at(0x8u))});
        const u32 text = word(rt, frame.at(0x8u));
        ctx.gpr[16] = result;
        const u32 block = text - 0xCu;
        ctx.gpr[17] = block;
        if (block != 0x08D74FD0u) {
            const u32 freed = call_set(rt, ctx, 0x08B60804u, {text - 0x4u, 0xFFFFFFFFu});
            if (static_cast<s32>(freed) <= 0) call_set(rt, ctx, 0x08B8542Cu, {block, frame.at(0x0u)});
        }
    }
    const u32 buffer = frame.at(0x4u);
    ctx.gpr[16] = buffer;
    call_set(rt, ctx, 0x08A97A6Cu, {buffer});
    if (object != buffer) call_set(rt, ctx, 0x08A5BAD8u, {object, word(rt, frame.at(0x4u))});
    call_set(rt, ctx, 0x08A5B9ECu, {buffer});
}
void func_08AA5E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 writer = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x10u}});
    // `ext rt, rs, position, size`: the `size` bits of rs from `position`
    const auto extract = [](u32 source, unsigned position, unsigned size) {
        return (source >> position) & ((1u << size) - 1u);
    };
    if (value < 0x80u) {
        store8(frame.at(0u), value & 0x7Fu);
        call_set(rt, ctx, 0x08AA5D2Cu, {writer, frame.at(0u), 1u});
    } else if (value < 0x4000u) {
        store8(frame.at(0u), extract(value, 8u, 6u) | 0xFFFFFF80u);
        store8(frame.at(1u), value);
        call_set(rt, ctx, 0x08AA5D2Cu, {writer, frame.at(0u), 2u});
    } else if (value <= 0x1FFFFFu) {
        store8(frame.at(0u), extract(value, 16u, 5u) | 0xFFFFFFC0u);
        store8(frame.at(1u), value >> 8);
        store8(frame.at(2u), value);
        call_set(rt, ctx, 0x08AA5D2Cu, {writer, frame.at(0u), 3u});
    } else if (value <= 0x0FFFFFFFu) {
        store8(frame.at(0u), extract(value, 24u, 4u) | 0xFFFFFFE0u);
        store8(frame.at(1u), value >> 16);
        store8(frame.at(2u), value >> 8);
        store8(frame.at(3u), value);
        call_set(rt, ctx, 0x08AA5D2Cu, {writer, frame.at(0u), 4u});
    } else {
        ctx.gpr[2] = value >> 8;
    }
}
// Decodes one arithmetic-coded value into a length table: a byte below 0x81 ends the run, otherwise its low
// seven bits index the two signed-byte record at 0x08CDFB84 (destination, bit count) and the value that
// follows the byte is read either as base-128 digits terminated by a byte with the high bit set, or -- for the
// kind 4 -- as a run of non-negative bytes whose start pointer is the value. A destination index >= 0 then sets
// the 1 << count bit of the word before that entry and stores the value at entry + count; the next byte
// decides whether to go on.
// 0x08AA5E90 (a0 = the writer, a1 = a value; v0 = the write call's result, or the value >> 8 when it is too large): writes the value as a
// 1- to 4-byte sequence on the frame and 0x08AA5D2C(writer, sp, length) emits it: below 0x80 one byte with the low 7 bits, below 0x4000 two
// bytes (the top byte 0xFFFFFF80 with bits 8..13), at or below 0x1FFFFF three (0xFFFFFFC0 with bits 16..20), at or below 0x0FFFFFFF four
// (0xFFFFFFE0 with bits 24..27); anything larger is not written.
void func_08ADDBE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4];
    u32 cursor = ctx.gpr[5];
    u32 key = load8(cursor);
    const auto negative = [](u32 byte) { return static_cast<std::int8_t>(byte) < 0; };
    ctx.gpr[6] = cursor;
    ctx.gpr[9] = table;
    ctx.gpr[2] = key < 0x81u ? 1u : 0u;
    if (key < 0x81u) return;
    const u32 table_base = 0x08CDFB84u, band_kind = 0x4u, one = 0x1u;
    ctx.gpr[10] = band_kind;
    ctx.gpr[11] = table_base;
    ctx.gpr[12] = one;
    for (;;) {
        const u32 index = key & 0x7Fu;
        ctx.gpr[7] = index;
        ++cursor;
        ctx.gpr[6] = cursor;
        u32 value = 0u;
        if (index == band_kind) {
            value = cursor;
            while (!negative(load8(cursor))) ++cursor;
        } else {
            u32 previous = load8(cursor);
            while (!negative(previous)) {
                ++cursor;
                const u32 next = load8(cursor);
                value = (value << 7) | (previous & 0xFFu);
                previous = next;
            }
        }
        const u32 record = (index << 1) + table_base;
        const u32 destination = signed_byte(rt, record);
        const u32 length = signed_byte(rt, record + 0x1u);
        if (static_cast<s32>(destination) >= 0) {
            const u32 slot = table + (destination << 2);
            put(rt, slot - 0x4u, word(rt, slot - 0x4u) | (one << (length & 31u)));
            put(rt, table + ((destination + length) << 2), value);
        }
        key = load8(cursor);
        ctx.gpr[2] = key;
        if (key < 0x81u) return;
    }
}
// Decodes one arithmetic-coded value into a length table: a byte below 0x81 ends the run, otherwise its low
// seven bits index the two signed-byte record at 0x08CDFB84 (destination, bit count) and the value that
// follows the byte is read either as base-128 digits terminated by a byte with the high bit set, or -- for the
// kind 4 -- as a run of non-negative bytes whose start pointer is the value. A destination index >= 0 then sets
// the 1 << count bit of the word before that entry and stores the value at entry + count; the next byte
// decides whether to go on.
void func_08B50D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { (void)rt; { G[5] = G[4]; jump(ctx, 0x08B50C8Cu); } }
void func_08B64260(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(4u), G[17]); G[17] = 0x08D70000u; put(rt, frame.at(8u), G[31]); put(rt, frame.at(0u), G[16]); G[2] = sext(load8(0x08D721C4u), 8); if (G[2] == 0u) { call_here(rt, ctx, 0x08C7A888u); G[4] = G[2]; G[2] = 0x08D721C4u; G[3] = load8(G[2] + 1u); G[3] += 1u; G[16] = sext(G[3], 8); G[3] = 1u; store8(G[2] + 1u, G[16]); if (G[16] == G[3]) { call_here(rt, ctx, 0x08C7A8A8u); call_here(rt, ctx, 0x08B641D0u); store8(0x08D721C4u, G[16]); } else { call_here(rt, ctx, 0x08C7A8A8u); G[2] = sext(load8(0x08D721C4u), 8); if (G[2] == 0u) { do { G[4] = 0x64u; call_here(rt, ctx, 0x08C7A988u); G[3] = sext(load8(0x08D721C4u), 8); } while (G[3] == 0u); } } } G[2] = 0x08D70000u; G[4] = word(rt, 0x08D721B8u); G[5] = 1u; G[6] = 0u; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08C7AAC8u); } }
// `floorf` by bit masks: below 1 in magnitude it answers +0, -0 or -1 depending on the sign and the magnitude,
// an integral value comes back unchanged, a fraction is cleared (magnitude increased by one unit when the
// value is negative), and a NaN or infinity is doubled and returned. The mask shifted by the exponent picks
// the fraction bits, and the +0.5-style guard at 0x08CB62B0 is the "inexact" addition of the original.
// `floorf` by bit masks: below 1 in magnitude it answers +0, -0 or -1 depending on the sign and the magnitude,
// an integral value comes back unchanged, a fraction is cleared (the magnitude increased by one unit when the
// value is negative), and a NaN or infinity is doubled and returned. The mask shifted by the exponent picks the
// fraction bits; the constant at 0x08CB62B0 is the "inexact" addition of the original, kept as a load.
void func_08B663B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const u32 bits = std::bit_cast<u32>(x);
    const u32 magnitude = bits << 1u >> 1u; // the sign bit cleared, as the `ins` at the head does
    const s32 exponent = static_cast<s32>(magnitude >> 23) - 0x7F;
    const float guard_constant = float_at(rt, 0x08CB62B0u);
    if (exponent >= 0x17) {
        if (0x7F7FFFFFu < magnitude) ctx.fpr[12] = x + x; // a NaN or infinity comes back doubled
        ctx.fpr[0] = ctx.fpr[12];
        return;
    }
    u32 result = bits;
    if (exponent < 0) {
        if (0.0f < x + guard_constant) {
            if (static_cast<s32>(bits) < 0) result = magnitude == 0u ? bits : 0xBF800000u;
            else result = 0u;
        }
    } else {
        const u32 mask = 0x007FFFFFu >> exponent;
        if ((bits & mask) != 0u && 0.0f < x + guard_constant) {
            u32 rounded = bits;
            if (static_cast<s32>(bits) < 0) rounded += 0x00800000u >> exponent;
            result = rounded & ~mask;
        }
    }
    ctx.fpr[12] = std::bit_cast<float>(result);
    ctx.fpr[0] = ctx.fpr[12];
}
void func_08C4FC50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); auto &F = ctx.fpr; put(rt, frame.at(4u), G[17]); G[17] = G[5] - 1u; put(rt, frame.at(0x14u), G[21]); G[21] = G[4]; put(rt, frame.at(0x10u), G[20]); G[20] = G[6]; put(rt, frame.at(0xCu), G[19]); G[19] = G[7]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(0u), G[16]); if (G[17] != 0u) { G[16] = G[17]; G[18] = 0u; G[22] = 0x80000000u; do { G[4] = G[20]; call_here(rt, ctx, 0x08B1EF70u); G[2] = G[16] + 1u; F[3] = std::bit_cast<float>(G[2]); G[3] = G[2] >> 1; G[6] = G[2] & 1u; F[1] = F[0]; G[5] = G[16]; G[18] += 1u; G[1] = 0x4F000000u; F[2] = std::bit_cast<float>(G[1]); G[4] = G[21]; G[6] |= G[3]; G[16] -= 1u; F[0] = static_cast<float>(static_cast<std::int32_t>(G[2])); if (static_cast<std::int32_t>(G[2]) < 0) { F[3] = std::bit_cast<float>(G[6]); F[0] = static_cast<float>(static_cast<std::int32_t>(G[6])); F[0] = F[0] + F[0]; } F[0] = F[1] * F[0]; F[1] = F[0] - F[2]; const bool ge = F[2] <= F[0]; G[6] = trunc_w_s(F[0]); F[0] = std::bit_cast<float>(G[6]); if (ge) { G[6] = trunc_w_s(F[1]); F[1] = std::bit_cast<float>(G[6]); G[6] |= G[22]; } call_here(rt, ctx, G[19]); } while (G[17] != G[18]); } G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_0883F10C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[18]); G[18] = G[4]; put(rt, frame.at(0x30u), G[16]); G[16] = G[6]; put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x34u), G[17]); if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = frame.at(0x10u); G[4] = G[17]; call_here(rt, ctx, 0x08931B74u); G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08C9BD48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[5] = word(rt, frame.at(8u)); G[4] = G[17]; call_here(rt, ctx, 0x08931E54u); G[4] = word(rt, frame.at(8u)); if (G[2] != 0u) { G[4] = G[17]; call_here(rt, ctx, 0x08931970u); G[16] = frame.at(4u); G[6] = G[2]; G[7] = G[3]; G[4] = G[16]; call_here(rt, ctx, 0x08A5DEE8u); G[5] = word(rt, frame.at(4u)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); } G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[17]; call_here(rt, ctx, 0x08931C38u); } G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_08850B84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x430u); auto &F = ctx.fpr; put(rt, frame.at(0x41Cu), G[19]); G[19] = G[4]; G[4] = 0x804u; put(rt, frame.at(0x418u), G[18]); G[18] = frame.at(4u); put(rt, frame.at(0x410u), G[16]); G[16] = G[5]; put(rt, frame.at(0x420u), G[31]); put(rt, frame.at(0x414u), G[17]); call_here(rt, ctx, 0x08B2B5E0u); G[5] = G[16]; G[4] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA0000u; if (G[16] == 0u) { G[4] = 0x08C9D3BCu; call_here(rt, ctx, 0x08A5BB7Cu); } G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = frame.at(8u); G[4] = G[16]; call_here(rt, ctx, 0x088A5B3Cu); G[5] = word(rt, frame.at(8u)); put(rt, G[17], G[5]); if (static_cast<std::int32_t>(G[5]) > 0) { G[4] = 0u; G[3] = frame.at(0xCu); G[2] = G[17]; do { F[0] = float_at(rt, G[3]); G[4] += 1u; put_float(rt, G[2] + 4u, F[0]); F[1] = float_at(rt, G[3] + 4u); G[3] += 8u; put_float(rt, G[2] + 8u, F[1]); G[2] += 8u; } while (G[5] != G[4]); } G[5] = G[17]; G[4] = frame.at(0u); G[6] = 0x804u; G[7] = 1u; call_here(rt, ctx, 0x08A44D9Cu); G[5] = word(rt, frame.at(0u)); if (G[19] != frame.at(0u)) { G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x41Cu)); G[18] = word(rt, frame.at(0x418u)); G[17] = word(rt, frame.at(0x414u)); G[16] = word(rt, frame.at(0x410u)); } } }
void func_08851670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[19] = frame.at(8u); put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; G[4] = G[19]; put(rt, frame.at(0x24u), G[21]); G[21] = frame.at(4u); put(rt, frame.at(0x14u), G[17]); G[17] = G[7]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x18u), G[18]); call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08C9D3BCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[4] = G[19]; G[18] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[21]; G[5] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9D3BCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; G[5] = G[16]; call_here(rt, ctx, 0x088A78B4u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[20]; if (G[20] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_08860B3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0xC0u); put(rt, frame.at(0xB8u), G[22]); G[2] = G[4] << 6; G[3] = G[4] << 4; put(rt, frame.at(0xB0u), G[20]); G[3] += G[2]; G[3] += 0x4120u; put(rt, frame.at(0xACu), G[19]); G[20] = frame.at(0x40u); G[19] = G[4]; put(rt, frame.at(0xA8u), G[18]); G[4] = frame.at(0u); G[22] = frame.at(0xA0u); put(rt, frame.at(0xA4u), G[17]); G[17] = G[5]; G[5] = 0u; put(rt, frame.at(0xA0u), G[16]); G[16] = G[20]; put(rt, frame.at(0xBCu), G[31]); put(rt, frame.at(0xB4u), G[21]); G[21] = word(rt, G[6]); G[18] = G[21] + G[3]; call_here(rt, ctx, 0x088A79ACu); G[4] = G[16]; do { G[16] += 0x30u; call_here(rt, ctx, 0x08897B8Cu); G[4] = G[16]; } while (G[16] != G[22]); G[5] = word(rt, G[18] + 0x48u); G[4] = word(rt, G[18] + 0x44u); G[6] = frame.at(8u); G[7] = frame.at(0u); G[8] = G[20]; call_here(rt, ctx, 0x08866510u); G[2] = G[17] << 4; G[4] = word(rt, G[21] + 0x5B4u); G[7] = G[17] << 6; G[7] -= G[2]; G[5] = G[19]; G[6] = G[17]; G[7] = G[20] + G[7]; call_here(rt, ctx, 0x08898570u); G[16] = G[22]; G[16] -= 0x30u; while (true) { G[4] = G[16]; call_here(rt, ctx, 0x088978B4u); bool more = G[20] != G[16]; G[16] -= 0x30u; if (!more) break; } G[4] = frame.at(0u); call_here(rt, ctx, 0x088A7548u); G[22] = word(rt, frame.at(0xB8u)); G[21] = word(rt, frame.at(0xB4u)); G[20] = word(rt, frame.at(0xB0u)); G[19] = word(rt, frame.at(0xACu)); G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); } } }
void func_08891984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[16] = G[4]; G[5] = word(rt, G[16] + 0x18u); G[4] = word(rt, G[4] + 0xCu); call_here(rt, ctx, 0x088984D4u); G[4] = word(rt, G[16] + 0xCu); G[5] = word(rt, G[16] + 0x1Cu); put(rt, G[16] + 0x24u, G[2]); call_here(rt, ctx, 0x088984D4u); G[3] = word(rt, G[16] + 0x70u); G[7] = word(rt, G[16] + 0xCu); G[8] = word(rt, G[16] + 0x10u); G[3] = (G[3] << 2) + G[16]; G[4] = word(rt, G[3] + 0x44u); G[9] = word(rt, G[16] + 0x14u); G[11] = word(rt, G[16] + 0x18u); G[6] = word(rt, G[16] + 8u); G[5] = word(rt, G[16] + 4u); G[10] = 1u; put(rt, G[16] + 0x28u, G[2]); call_here(rt, ctx, 0x0888FBD8u); G[5] = 0u; G[2] = word(rt, G[16] + 0x70u); G[18] = word(rt, G[16] + 0x24u); G[2] = (G[2] << 2) + G[16]; G[4] = word(rt, G[2] + 0x44u); call_here(rt, ctx, 0x08890C5Cu); G[17] = G[2]; G[2] = word(rt, G[16] + 0x70u); G[5] = 0u; G[2] = (G[2] << 2) + G[16]; G[4] = word(rt, G[2] + 0x44u); call_here(rt, ctx, 0x08890B20u); G[6] = G[2]; G[4] = G[18]; G[5] = G[17]; call_here(rt, ctx, 0x0889605Cu); G[4] = word(rt, G[16] + 0x24u); call_here(rt, ctx, 0x088963C4u); G[3] = word(rt, G[16] + 0x70u); G[2] = 0x481907FFu; G[3] = (G[3] << 2) + G[16]; G[4] = 1u; put(rt, G[3] + 0x50u, G[2]); store8(G[16] + 0x6Bu, G[4]); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_0889608C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t target = 0u; { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(8u), G[18]); G[18] = G[5]; put(rt, frame.at(0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(4u), G[17]); G[2] = load8(G[4] + 0xF0u); G[16] = G[4]; if (G[2] == 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x24u); call_here(rt, ctx, G[2]); G[4] = G[18]; if (G[2] == 0u) target = 0x0892B440u; } else { G[2] = word(rt, G[4] + 0xF4u); G[17] = word(rt, G[4] + 0xECu); G[2] += 1u; put(rt, G[4] + 0xF4u, G[2]); if (G[17] == 0u) { G[17] = G[4] + 0xB4u; G[4] = G[17]; call_here(rt, ctx, 0x08895A88u); if (G[2] == 0u) { G[5] = word(rt, G[16] + 0xF4u); store8(G[16] + 0xF0u, 0u); G[4] = G[17]; target = 0x08894E8Cu; } } else { G[2] = 1u; if (G[17] == G[2]) { G[4] += 0xB4u; call_here(rt, ctx, 0x08895544u); if (G[2] == 0u) { store8(G[16] + 0xF1u, G[17]); G[4] = G[18]; store8(G[16] + 0xF0u, 0u); target = 0x0892B440u; } } } } G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } if (target != 0u) jump(ctx, target); } }
void func_088A2274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0u), G[16]); G[16] = G[4]; put(rt, frame.at(4u), G[31]); G[4] = word(rt, G[4] + 4u); G[4] += 4u; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16] + 8u); G[3] = G[16] + 0x10u; G[2] = G[16] + 0x44u; put(rt, G[16] + 0x10u, 0u); put(rt, G[16] + 0x44u, 0u); store8(G[16] + 0x78u, 0u); put(rt, G[2] + 0x30u, 0u); for (std::uint32_t off = 4u; off <= 0x30u; off += 4u) put(rt, G[3] + off, 0u); for (std::uint32_t off = 4u; off <= 0x2Cu; off += 4u) put(rt, G[2] + off, 0u); if (G[4] != 0u) { G[6] = G[3]; G[7] = G[4]; G[8] = G[16] + 0x40u; do { G[2] = word(rt, G[6]); G[3] = word(rt, G[6] + 4u); G[4] = word(rt, G[6] + 8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7], G[2]); G[6] += 0x10u; G[7] += 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 8u, G[4]); put(rt, G[7] - 4u, G[5]); } while (G[6] != G[8]); G[2] = word(rt, G[6]); put(rt, G[7], G[2]); } G[4] = word(rt, G[16] + 4u); G[4] += 4u; G[16] = word(rt, frame.at(0u)); } jump(ctx, 0x08B3457Cu); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_269
} // namespace gt::game

#define HOT269(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_269::name, gt::ReturnKind::kind)
HOT269(0x0883F1FCu, func_0883F1FC, Word);
HOT269(0x08895D88u, func_08895D88, Word);
HOT269(0x088BCA94u, func_088BCA94, Word);
HOT269(0x088CF328u, func_088CF328, Word);
HOT269(0x08928794u, func_08928794, Float);
HOT269(0x08958F20u, func_08958F20, Word);
HOT269(0x089ACBB4u, func_089ACBB4, Word);
HOT269(0x08A205FCu, func_08A205FC, Word);
HOT269(0x08AA5E90u, func_08AA5E90, Word);
HOT269(0x08ADDBE8u, func_08ADDBE8, Word);
HOT269(0x08B50D70u, func_08B50D70, Word);
HOT269(0x08B64260u, func_08B64260, Word);
HOT269(0x08B663B4u, func_08B663B4, Float);
HOT269(0x08C4FC50u, func_08C4FC50, Word);
HOT269(0x0883F10Cu, func_0883F10C, Word);
// HOT269(0x08850B84u, func_08850B84, Word);  // no comparable synthetic case yet
// HOT269(0x08851670u, func_08851670, Word);  // no comparable synthetic case yet
HOT269(0x08860B3Cu, func_08860B3C, Word);
// HOT269(0x08891984u, func_08891984, Word);  // no comparable synthetic case yet
HOT269(0x0889608Cu, func_0889608C, Word);
// HOT269(0x088A2274u, func_088A2274, Word);  // no comparable synthetic case yet
