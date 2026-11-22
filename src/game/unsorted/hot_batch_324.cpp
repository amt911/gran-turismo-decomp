// Hot batch 324: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
// 0x08A6F888, 0x08A6FA6C are rewritten as structured C from the disassembly (structured rewrites, batch 90) and registered: the synthetic gate compares them in the garage / challenge / dirt / manual / challenge2 scenes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_324 {
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
// The two element-insert functions below (0x08C3F648 for 4-byte elements, 0x08C47EA4 for 2-byte ones) are one routine: a vector (begin +0, end +4, capacity end +8) insert
// of one element at a position. With room the last element moves to the end (when the end is not null), the elements from the position up move one slot with
// 0x08B79BAC(position + size, position, bytes) and the value (read before the move) lands at the position, v0 = the move's. Without room the capacity doubles (1 when
// empty; a doubled count that wraps gives -size bytes, a count of `max_count` calls 0x08B5ADDC(limit_message)) in a block of 0x08B35DA0(bytes); `copy_range` (0x08C3F618 /
// 0x08C46FA4) copies the prefix, the value goes in the new slot, `copy_range` copies the rest, `destroy_range` (0x08C3F588 / 0x08C46F94) clears the old range, the old block is freed
// with 0x08B35254 and the vector takes the new block; v0 = the new capacity end.
struct InsertLayout {
    u32 size;
    u32 max_count;
    u32 limit_message;
    u32 copy_range;
    u32 destroy_range;
};

void insert_element(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const InsertLayout &layout) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    const u32 size = layout.size;
    const unsigned shift = size == 4u ? 2u : 1u;
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}});
    const auto load_element = [&](u32 address) { return size == 4u ? word(rt, address) : signed_half(rt, address); };
    const auto store_element = [&](u32 address, u32 element) {
        if (size == 4u) put(rt, address, element);
        else rt.memory().store16(address, static_cast<std::uint16_t>(element));
    };
    const u32 end = word(rt, self + 0x4u);
    if (end != word(rt, self + 0x8u)) {
        if (end != 0u) store_element(end, load_element(end - size));
        const u32 old_end = end;
        put(rt, self + 0x4u, end + size);
        const u32 moving = (old_end - size - position) & ~(size - 1u);
        const u32 element = load_element(value);
        call_set(rt, ctx, 0x08B79BACu, {old_end - moving, position, moving});
        store_element(position, element);
        return;
    }
    const u32 count = static_cast<u32>(static_cast<s32>(end - word(rt, self)) >> shift);
    if (count == layout.max_count) call_set(rt, ctx, 0x08B5ADDCu, {layout.limit_message});
    u32 elements = 1u;
    u32 bytes = size;
    if (count != 0u) {
        elements = count << 1;
        if (elements < count) {
            bytes = 0u - size;
        } else {
            bytes = elements << shift;
        }
    }
    const u32 block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
    const u32 slot = call_set(rt, ctx, layout.copy_range, {word(rt, self), position, block, frame.at(0x2u)});
    if (slot != 0u) store_element(slot, load_element(value));
    const u32 new_end = call_set(rt, ctx, layout.copy_range, {position, word(rt, self + 0x4u), slot + size, frame.at(0x1u)});
    call_set(rt, ctx, layout.destroy_range, {word(rt, self), word(rt, self + 0x4u), frame.at(0x0u)});
    if (word(rt, self) != 0u) call_set(rt, ctx, 0x08B35254u, {word(rt, self)});
    ctx.gpr[2] = block + bytes;
    put(rt, self + 0x8u, block + bytes);
    put(rt, self + 0x4u, new_end);
    put(rt, self, block);
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08C3F648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    insert_element(rt, ctx, {4u, 0x3FFFFFFFu, 0x08CB4AACu, 0x08C3F618u, 0x08C3F588u});
}
void func_08C47468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[3] == G[2]; G[16] = G[4]; if (tk) goto L_08C474DC; } if (G[3] == 0u) { G[4] = G[3]; goto L_08C474B4; } G[2] = word(rt, G[3] + 0xFFFFFFFCu); put(rt, G[3] + 0x0u, G[2]); G[3] = word(rt, G[4] + 0x4u); G[4] = G[3]; L_08C474B4: ; G[6] = G[4] + 0xFFFFFFFCu; G[3] = G[3] + 0x4u; put(rt, G[16] + 0x4u, G[3]); G[6] = G[6] - G[17]; G[6] = (G[6] & ~0x3u) | ((0u << 0) & 0x3u); G[16] = word(rt, G[18] + 0x0u); G[4] = G[4] - G[6]; call_here(rt, ctx, 0x08B79BACu); put(rt, G[17] + 0x0u, G[16]); goto L_08C475A8; L_08C474DC: ; G[4] = word(rt, G[4] + 0x0u); G[2] = 0x3FFF0000u; G[2] = G[2] | 0xFFFFu; G[3] = G[3] - G[4]; G[3] = sra(G[3], 2); { const bool tk = G[3] != G[2]; if (tk) goto L_08C47504; } G[4] = 0x08CB0000u; G[4] = 0x08CB4AD4u; call_here(rt, ctx, 0x08B5ADDCu); L_08C47504: ; { const bool tk = G[3] == 0u; G[5] = 0x1u; if (tk) goto L_08C47528; } G[5] = G[3] << 1; G[2] = G[5] < G[3] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFCu; if (tk) goto L_08C4752C; } G[19] = 0u; { const bool tk = G[5] == 0u; G[20] = 0u; if (tk) goto L_08C4753C; } L_08C47528: ; G[20] = G[5] << 2; L_08C4752C: ; G[4] = G[20]; call_here(rt, ctx, 0x08B35DA0u); G[4] = word(rt, G[16] + 0x0u); G[19] = G[2]; L_08C4753C: ; G[6] = G[19]; G[5] = G[17]; G[7] = frame.at(0x2u); call_here(rt, ctx, 0x08C46FE4u); { const bool tk = G[2] == 0u; G[6] = G[2]; if (tk) goto L_08C4755C; } G[2] = word(rt, G[18] + 0x0u); put(rt, G[6] + 0x0u, G[2]); L_08C4755C: ; G[5] = word(rt, G[16] + 0x4u); G[4] = G[17]; G[6] = G[6] + 0x4u; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C46FE4u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C46FD4u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[19] + G[20]; if (tk) goto L_08C4759C; } call_here(rt, ctx, 0x08B35254u); G[2] = G[19] + G[20]; L_08C4759C: ; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C475A8: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C475C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[3] == G[2]; G[16] = G[4]; if (tk) goto L_08C4763C; } if (G[3] == 0u) { G[4] = G[3]; goto L_08C47614; } G[2] = word(rt, G[3] + 0xFFFFFFFCu); put(rt, G[3] + 0x0u, G[2]); G[3] = word(rt, G[4] + 0x4u); G[4] = G[3]; L_08C47614: ; G[6] = G[4] + 0xFFFFFFFCu; G[3] = G[3] + 0x4u; put(rt, G[16] + 0x4u, G[3]); G[6] = G[6] - G[17]; G[6] = (G[6] & ~0x3u) | ((0u << 0) & 0x3u); G[16] = word(rt, G[18] + 0x0u); G[4] = G[4] - G[6]; call_here(rt, ctx, 0x08B79BACu); put(rt, G[17] + 0x0u, G[16]); goto L_08C47708; L_08C4763C: ; G[4] = word(rt, G[4] + 0x0u); G[2] = 0x3FFF0000u; G[2] = G[2] | 0xFFFFu; G[3] = G[3] - G[4]; G[3] = sra(G[3], 2); { const bool tk = G[3] != G[2]; if (tk) goto L_08C47664; } G[4] = 0x08CB0000u; G[4] = 0x08CB4AD4u; call_here(rt, ctx, 0x08B5ADDCu); L_08C47664: ; { const bool tk = G[3] == 0u; G[5] = 0x1u; if (tk) goto L_08C47688; } G[5] = G[3] << 1; G[2] = G[5] < G[3] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFCu; if (tk) goto L_08C4768C; } G[19] = 0u; { const bool tk = G[5] == 0u; G[20] = 0u; if (tk) goto L_08C4769C; } L_08C47688: ; G[20] = G[5] << 2; L_08C4768C: ; G[4] = G[20]; call_here(rt, ctx, 0x08B35DA0u); G[4] = word(rt, G[16] + 0x0u); G[19] = G[2]; L_08C4769C: ; G[6] = G[19]; G[5] = G[17]; G[7] = frame.at(0x2u); call_here(rt, ctx, 0x08C46F64u); { const bool tk = G[2] == 0u; G[6] = G[2]; if (tk) goto L_08C476BC; } G[2] = word(rt, G[18] + 0x0u); put(rt, G[6] + 0x0u, G[2]); L_08C476BC: ; G[5] = word(rt, G[16] + 0x4u); G[4] = G[17]; G[6] = G[6] + 0x4u; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C46F64u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08BBB8F8u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[19] + G[20]; if (tk) goto L_08C476FC; } call_here(rt, ctx, 0x08B35254u); G[2] = G[19] + G[20]; L_08C476FC: ; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C47708: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C47EA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[3] == G[2]; G[16] = G[4]; if (tk) goto L_08C47F18; } if (G[3] == 0u) { G[4] = G[3]; goto L_08C47EF0; } G[2] = load16(G[3] + 0xFFFFFFFEu); store16(G[3] + 0x0u, G[2]); G[3] = word(rt, G[4] + 0x4u); G[4] = G[3]; L_08C47EF0: ; G[6] = G[4] + 0xFFFFFFFEu; G[3] = G[3] + 0x2u; put(rt, G[16] + 0x4u, G[3]); G[6] = G[6] - G[17]; G[6] = (G[6] & ~0x1u) | ((0u << 0) & 0x1u); G[16] = sext(load16(G[18] + 0x0u), 16); G[4] = G[4] - G[6]; call_here(rt, ctx, 0x08B79BACu); store16(G[17] + 0x0u, G[16]); goto L_08C47FE4; L_08C47F18: ; G[4] = word(rt, G[4] + 0x0u); G[2] = 0x7FFF0000u; G[2] = G[2] | 0xFFFFu; G[3] = G[3] - G[4]; G[3] = sra(G[3], 1); { const bool tk = G[3] != G[2]; if (tk) goto L_08C47F40; } G[4] = 0x08CB0000u; G[4] = 0x08CB4AD4u; call_here(rt, ctx, 0x08B5ADDCu); L_08C47F40: ; { const bool tk = G[3] == 0u; G[5] = 0x1u; if (tk) goto L_08C47F64; } G[5] = G[3] << 1; G[2] = G[5] < G[3] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFEu; if (tk) goto L_08C47F68; } G[19] = 0u; { const bool tk = G[5] == 0u; G[20] = 0u; if (tk) goto L_08C47F78; } L_08C47F64: ; G[20] = G[5] << 1; L_08C47F68: ; G[4] = G[20]; call_here(rt, ctx, 0x08B35DA0u); G[4] = word(rt, G[16] + 0x0u); G[19] = G[2]; L_08C47F78: ; G[6] = G[19]; G[5] = G[17]; G[7] = frame.at(0x2u); call_here(rt, ctx, 0x08C46FA4u); { const bool tk = G[2] == 0u; G[6] = G[2]; if (tk) goto L_08C47F98; } G[2] = load16(G[18] + 0x0u); store16(G[6] + 0x0u, G[2]); L_08C47F98: ; G[5] = word(rt, G[16] + 0x4u); G[4] = G[17]; G[6] = G[6] + 0x2u; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C46FA4u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C46F94u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[19] + G[20]; if (tk) goto L_08C47FD8; } call_here(rt, ctx, 0x08B35254u); G[2] = G[19] + G[20]; L_08C47FD8: ; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C47FE4: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x0881D2EC (a0 = the destination string slot, a1 = unused here, a2 = the argument count, a3 = the argument array): fewer than 3 arguments return 1. Otherwise 0x0881BD98(frame + 4)
// fills a holder whose object (null: 0x08A5BB7C(0x08C98F48)) gives a handle through 0x0881B068, the holder is released with 0x08A5B9EC, and 0x0881BF5C(handle, count, array,
// frame + 0x10, frame + 8) answers an index; when both of its outputs (frame +0x10, +0x14) are -1 the destination gets 0x08A97A6C(frame, 0) (assigned with 0x08A5BAD8(destination, frame
// word) unless it is the frame itself) and the frame is released; otherwise the two array entries at 4 * index (null: 0x08A5BB7C(0x08C98EFC)) answer their virtual +0x3C (first) and +0x30
// (second) and 0x0881AA10(handle, count, first output, second output, frame + 8 word, +0x3C answer, +0x30 answer) runs. v0 = the last callee's.
void func_0881D2EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 array = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x30u}, {18u, 0x28u}, {17u, 0x24u}, {31u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {19u, 0x2Cu}, {16u, 0x20u}});
    ctx.gpr[2] = count < 3 ? 1u : 0u;
    if (count < 3) return;
    const u32 holder = frame.at(0x4u);
    call_set(rt, ctx, 0x0881BD98u, {holder});
    const u32 object = word(rt, frame.at(0x4u));
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    const u32 handle = call_set(rt, ctx, 0x0881B068u, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
    const u32 index = call_set(rt, ctx, 0x0881BF5Cu, {handle, static_cast<u32>(count), array, frame.at(0x10u), frame.at(0x8u)});
    const u32 first_output = word(rt, frame.at(0x10u));
    const u32 second_output = word(rt, frame.at(0x14u));
    if (first_output == 0xFFFFFFFFu && second_output == 0xFFFFFFFFu) {
        call_set(rt, ctx, 0x08A97A6Cu, {frame.at(0x0u), 0u});
        if (destination != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0x0u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
        return;
    }
    const u32 entries = array + (index << 2);
    const u32 saved_pointer = word(rt, frame.at(0x8u));
    u32 first = word(rt, entries);
    if (first == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98EFCu});
    const u32 first_answer = call_indirect(rt, ctx, word(rt, word(rt, first) + 0x3Cu), {first});
    u32 second = word(rt, entries + 0x4u);
    if (second == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98EFCu});
    const u32 second_answer = call_indirect(rt, ctx, word(rt, word(rt, second) + 0x30u), {second});
    call_set(rt, ctx, 0x0881AA10u, {handle, static_cast<u32>(count), first_output, second_output, saved_pointer, first_answer, second_answer});
}

void func_088CF168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); auto &F = ctx.fpr; G[23] = G[4]; G[4] = G[5]; G[3] = word(rt, G[5] + 0x0u); G[2] = word(rt, G[3] + 0x8Cu); { const std::uint32_t tg = G[2]; G[16] = G[5]; call_here(rt, ctx, tg); } { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[18] = G[2]; if (tk) goto L_088CF294; } G[17] = 0u; G[22] = 0u; G[21] = 0u; G[20] = 0u; G[19] = 0u; L_088CF1C4: ; G[3] = word(rt, G[16] + 0x0u); G[5] = G[17]; G[2] = word(rt, G[3] + 0x9Cu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, G[16] + 0x0u); G[6] = 0u < G[2] ? 1u : 0u; G[5] = G[17]; G[2] = word(rt, G[3] + 0x90u); G[4] = G[16]; { const std::uint32_t tg = G[2]; G[19] = G[19] + G[6]; call_here(rt, ctx, tg); } G[3] = word(rt, G[16] + 0x0u); G[6] = 0u < G[2] ? 1u : 0u; G[5] = G[17]; G[2] = word(rt, G[3] + 0x94u); G[4] = G[16]; { const std::uint32_t tg = G[2]; G[20] = G[20] + G[6]; call_here(rt, ctx, tg); } G[3] = word(rt, G[16] + 0x0u); G[6] = 0u < G[2] ? 1u : 0u; G[5] = G[17]; G[2] = word(rt, G[3] + 0x98u); G[4] = G[16]; { const std::uint32_t tg = G[2]; G[21] = G[21] + G[6]; call_here(rt, ctx, tg); } G[2] = 0u < G[2] ? 1u : 0u; G[17] = G[17] + 0x1u; { const bool tk = G[18] != G[17]; G[22] = G[22] + G[2]; if (tk) goto L_088CF1C4; } { const bool tk = G[19] == 0u; G[2] = 0x1u; if (tk) goto L_088CF2A0; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x88u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = 0x2u; goto L_088CF2A0; } { const bool tk = G[18] == G[20]; G[2] = 0x2u; if (tk) goto L_088CF2A0; } { const bool tk = G[18] == G[21]; if (tk) goto L_088CF2A0; } { const bool tk = G[18] == G[22]; if (tk) goto L_088CF2A4; } F[1] = float_at(rt, G[23] + 0x28u); F[0] = float_at(rt, G[23] + 0x140u); ctx.set_fpu_condition(F[1] <= F[0]); { const bool tk = ctx.fpu_condition(); G[2] = 0u; if (tk) goto L_088CF29C; } goto L_088CF2A4; L_088CF294: ; G[2] = 0x1u; goto L_088CF2A0; L_088CF29C: ; G[2] = 0x2u; L_088CF2A0: ; L_088CF2A4: ; goto exit_;  exit_: ; G[23] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_089BEA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[21] = G[6]; G[20] = G[5]; G[19] = G[8]; G[18] = G[7]; G[17] = G[4]; put_float(rt, frame.at(0x30u), F[20]); G[16] = word(rt, G[4] + 0x264u); G[4] = 0x08CA0000u; G[4] = 0x08CA5EA4u; put(rt, frame.at(0x0u), 0u); { const bool tk = G[16] != 0u; put(rt, frame.at(0x4u), 0u); if (tk) goto L_089BEAD0; } call_here(rt, ctx, 0x08A5BB7Cu); L_089BEAD0: ; G[3] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[5] = frame.at(0u); G[2] = word(rt, G[3] + 0x74u); { const std::uint32_t tg = G[2]; G[6] = frame.at(0x4u); call_here(rt, ctx, tg); } G[4] = G[17]; G[5] = 0u; G[6] = 0u; G[7] = frame.at(0x8u); G[8] = frame.at(0xCu); call_here(rt, ctx, 0x0896A35Cu); G[4] = G[17] + 0x22Cu; F[0] = float_at(rt, G[4] + 0x8u); F[20] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); call_here(rt, ctx, 0x08A197F0u); F[0] = std::bit_cast<float>(G[2]); G[3] = word(rt, frame.at(0x0u)); G[2] = 0x780u; G[1] = 0x3F000000u; F[5] = std::bit_cast<float>(G[1]); { const bool tk = G[3] != G[2]; F[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); if (tk) goto L_089BEB64; } G[2] = word(rt, frame.at(0x4u)); G[1] = 0x44880000u; F[0] = std::bit_cast<float>(G[1]); G[2] = static_cast<std::int32_t>(G[2]) < 1080 ? 1u : 0u; { const bool tk = G[2] != 0u; F[1] = psp_mul(F[2], F[0]); if (tk) goto L_089BEB64; } G[1] = 0x44870000u; F[0] = std::bit_cast<float>(G[1]); F[0] = F[1] / F[0]; put(rt, G[21] + 0x0u, 0u); put(rt, G[20] + 0x0u, 0u); put_float(rt, G[18] + 0x0u, F[20]); put_float(rt, G[19] + 0x0u, F[0]); goto L_089BEBB4; L_089BEB64: ; F[4] = float_at(rt, frame.at(0xCu)); F[3] = float_at(rt, frame.at(0x8u)); F[0] = F[4] / F[2]; F[1] = F[3] / F[20]; ctx.set_fpu_condition(F[0] < F[1]); if (!ctx.fpu_condition()) { F[0] = F[1]; goto L_089BEB84; } L_089BEB84: ; F[1] = psp_mul(F[0], F[2]); F[0] = psp_mul(F[0], F[20]); put_float(rt, G[18] + 0x0u, F[0]); put_float(rt, G[19] + 0x0u, F[1]); F[0] = float_at(rt, G[18] + 0x0u); F[0] = F[3] - F[0]; F[0] = psp_mul(F[0], F[5]); put_float(rt, G[20] + 0x0u, F[0]); F[1] = float_at(rt, G[19] + 0x0u); F[1] = F[4] - F[1]; F[1] = psp_mul(F[1], F[5]); put_float(rt, G[21] + 0x0u, F[1]); L_089BEBB4: ; F[20] = float_at(rt, frame.at(0x30u)); goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_089E02C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[31]); auto &F = ctx.fpr; G[2] = 0x1u; G[19] = G[7]; G[3] = 0xFFFFFFFFu; G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; put_float(rt, frame.at(0x48u), F[22]); put_float(rt, frame.at(0x44u), F[21]); put_float(rt, frame.at(0x40u), F[20]); G[2] = G[2] + 0xFFFFFFFFu; L_089E0304: ; { const bool tk = G[2] != G[3]; G[2] = G[2] + 0xFFFFFFFFu; if (tk) goto L_089E0304; } G[2] = 0x2u; G[2] = G[2] + 0xFFFFFFFFu; L_089E0314: ; { const bool tk = G[2] != 0u; G[2] = G[2] + 0xFFFFFFFFu; if (tk) goto L_089E0314; } G[4] = G[16]; F[21] = float_at(rt, G[18] + 0x18u); F[20] = float_at(rt, G[18] + 0x1Cu); call_here(rt, ctx, 0x089A8B68u); G[4] = G[16]; F[22] = F[0]; call_here(rt, ctx, 0x089A8B80u); F[4] = float_at(rt, G[17] + 0x1ECu); F[1] = float_at(rt, G[17] + 0x1F8u); F[2] = float_at(rt, G[17] + 0x1E8u); F[3] = float_at(rt, G[17] + 0x1FCu); G[3] = word(rt, G[16] + 0x0u); F[2] = psp_mul(F[21], F[2]); F[3] = psp_mul(F[20], F[3]); F[21] = psp_mul(F[21], F[1]); F[20] = psp_mul(F[20], F[4]); put_float(rt, frame.at(0x10u), F[2]); G[5] = G[19]; put_float(rt, frame.at(0x1Cu), F[3]); put_float(rt, frame.at(0xCu), F[0]); put_float(rt, frame.at(0x8u), F[22]); put_float(rt, frame.at(0x14u), F[20]); put_float(rt, frame.at(0x18u), F[21]); put(rt, frame.at(0x0u), 0u); put(rt, frame.at(0x4u), 0u); G[2] = word(rt, G[3] + 0x110u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = 0x1u; store8(G[16] + 0x118u, G[3]); G[4] = G[16] + 0x160u; G[5] = G[17] + 0x1C0u; G[2] = word(rt, G[17] + 0x1B0u); G[2] = (G[2] >> 7) & 0x1u; store8(G[16] + 0x150u, G[2]); G[3] = word(rt, G[17] + 0x1B0u); G[3] = (G[3] >> 8) & 0x1u; store8(G[16] + 0x151u, G[3]); G[2] = word(rt, G[17] + 0x1B0u); G[2] = (G[2] >> 9) & 0x1u; store8(G[16] + 0x152u, G[2]); call_here(rt, ctx, 0x089641C4u); G[6] = frame.at(0x10u); F[1] = float_at(rt, G[17] + 0x1E0u); G[5] = frame.at(0u); G[4] = G[16]; put_float(rt, G[16] + 0x170u, F[1]); F[0] = float_at(rt, G[17] + 0x1E4u); put(rt, G[16] + 0x11Cu, G[18]); put_float(rt, G[16] + 0x174u, F[0]); call_here(rt, ctx, 0x089A932Cu); G[4] = G[16]; call_here(rt, ctx, 0x089A8C24u); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x114u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } F[22] = float_at(rt, frame.at(0x48u)); F[21] = float_at(rt, frame.at(0x44u)); F[20] = float_at(rt, frame.at(0x40u)); goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089F99C0 (a0 = the object, a1 = an index): the vector of 0x40-byte elements at object + 0xA8 (begin +0, end +4, capacity +8) is edited: with the index in range (0 <= index <
// the object's virtual +0x1C8 count) the element is erased by 0x08C0D458(vector, element + 0x40, element). Otherwise a default element is appended: 0x089D76A8(frame) builds it
// (frame + 0 is then set to 0x08CCFC90, the two members at frame + 0x20 / + 0x30 are built by 0x089FA09C), with room (end != capacity) its first 0x20 bytes are copied to the end slot
// (word 0 = 0x08CCFC90, when the end is not null) and the members are copy-built by 0x089FA280(slot + 0x20 / + 0x30, frame + 0x20 / + 0x30), the end moving up 0x40;
// without room 0x08C0D100(vector, end, frame) grows it. The temporaries are then destroyed by 0x089F9B74 (members) and 0x089D6684(frame) after frame + 0 is set to 0x08CCFC90 again.
// v0 = the last callee's.
void func_089F99C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 index = static_cast<s32>(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x44u}, {16u, 0x40u}, {31u, 0x58u}, {21u, 0x54u}, {20u, 0x50u}, {19u, 0x4Cu}, {18u, 0x48u}});
    const u32 vector = self + 0xA8u;
    if (index >= 0 && index < static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, self) + 0x1C8u), {self}))) {
        const u32 element = word(rt, vector) + (static_cast<u32>(index) << 6);
        call_set(rt, ctx, 0x08C0D458u, {vector, element + 0x40u, element});
        return;
    }
    const u32 vtable = 0x08CCFC90u;
    const u32 members = frame.at(0x20u);
    const u32 second_members = frame.at(0x30u);
    call_set(rt, ctx, 0x089D76A8u, {frame.at(0x0u)});
    put(rt, frame.at(0x0u), vtable);
    call_set(rt, ctx, 0x089FA09Cu, {members});
    call_set(rt, ctx, 0x089FA09Cu, {second_members});
    u32 end = word(rt, vector + 0x4u);
    if (end == word(rt, vector + 0x8u)) {
        call_set(rt, ctx, 0x08C0D100u, {vector, end, frame.at(0x0u)});
    } else {
        if (end != 0u) {
            put(rt, end + 0xCu, word(rt, frame.at(0xCu)));
            put(rt, end + 0x4u, word(rt, frame.at(0x4u)));
            put(rt, end + 0x10u, word(rt, frame.at(0x10u)));
            put(rt, end + 0x14u, word(rt, frame.at(0x14u)));
            put(rt, end + 0x8u, word(rt, frame.at(0x8u)));
            put(rt, end + 0x18u, word(rt, frame.at(0x18u)));
            put(rt, end + 0x1Cu, word(rt, frame.at(0x1Cu)));
            put(rt, end, vtable);
            call_set(rt, ctx, 0x089FA280u, {end + 0x20u, members});
            call_set(rt, ctx, 0x089FA280u, {end + 0x30u, second_members});
            end = word(rt, vector + 0x4u);
        }
        put(rt, vector + 0x4u, end + 0x40u);
    }
    put(rt, frame.at(0x0u), vtable);
    call_set(rt, ctx, 0x089F9B74u, {second_members});
    call_set(rt, ctx, 0x089F9B74u, {members});
    call_set(rt, ctx, 0x089D6684u, {frame.at(0x0u)});
}
void func_08A4F8B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xD0u); put(rt, frame.at(0xBCu), G[19]); put(rt, frame.at(0xB8u), G[18]); put(rt, frame.at(0xC8u), G[31]); put(rt, frame.at(0xC4u), G[21]); put(rt, frame.at(0xC0u), G[20]); put(rt, frame.at(0xB4u), G[17]); put(rt, frame.at(0xB0u), G[16]); auto &F = ctx.fpr; G[19] = G[4]; G[18] = G[5]; { const bool tk = G[6] == 0u; if (tk) goto L_08A4F940; } G[4] = G[5]; G[7] = frame.at(0u); G[5] = G[6]; G[6] = 0u; call_here(rt, ctx, 0x08C7AC90u); G[3] = G[2]; G[2] = word(rt, frame.at(0x0u)); { const bool tk = G[2] == 0u; if (tk) goto L_08A4F9F4; } G[3] = 0u; goto L_08A4F9F4; L_08A4F90C: ; G[2] = G[17] << 2; G[2] = G[2] + G[4]; G[3] = G[20] + 0x314Cu; G[2] = G[2] + G[3]; G[5] = word(rt, G[2] + 0xCu); F[0] = float_at(rt, G[2] + 0x4u); G[3] = word(rt, G[2] + 0x0u); G[4] = word(rt, G[2] + 0x8u); put_float(rt, frame.at(0x4u), F[0]); store16(frame.at(0x1Eu), G[3]); store16(frame.at(0x18u), G[4]); store16(frame.at(0x1Au), G[5]); goto L_08A4F9AC; L_08A4F940: ; G[21] = frame.at(0x4u); G[5] = 0u; G[4] = G[21]; G[6] = 0xA8u; G[20] = 0x08CE0000u; call_here(rt, ctx, 0x08B36018u); G[3] = 0x08CDF64Cu; G[5] = word(rt, G[3] + 0x10u); G[1] = 0x41200000u; F[0] = std::bit_cast<float>(G[1]); G[2] = 0x2u; put_float(rt, frame.at(0x4u), F[0]); store16(frame.at(0x1Eu), G[2]); store16(frame.at(0x18u), 0u); { const bool tk = G[5] == 0u; store16(frame.at(0x1Au), 0u); if (tk) goto L_08A4F9AC; } G[2] = 0x08CE0000u; G[16] = 0x08CDF670u; G[17] = 0u; L_08A4F98C: ; G[4] = G[19]; call_here(rt, ctx, 0x08C1CDCCu); { const bool tk = G[2] == 0u; G[4] = G[17] << 4; if (tk) goto L_08A4F90C; } G[5] = word(rt, G[16] + 0x0u); G[17] = G[17] + 0x1u; { const bool tk = G[5] != 0u; G[16] = G[16] + 0x14u; if (tk) goto L_08A4F98C; } L_08A4F9AC: ; G[5] = G[21]; G[4] = G[18]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08C7AC70u); G[5] = G[2]; G[2] = word(rt, frame.at(0x0u)); { const bool tk = G[2] != 0u; G[3] = 0u; if (tk) goto L_08A4F9F0; } G[4] = G[18]; G[6] = 0u; G[7] = frame.at(0u); call_here(rt, ctx, 0x08C7ACA0u); G[3] = G[2]; G[2] = word(rt, frame.at(0x0u)); { const bool tk = G[2] == 0u; if (tk) goto L_08A4F9F4; } G[3] = 0u; L_08A4F9F0: ; L_08A4F9F4: ; G[2] = G[3]; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xBCu)); G[18] = word(rt, frame.at(0xB8u)); G[21] = word(rt, frame.at(0xC4u)); G[20] = word(rt, frame.at(0xC0u)); G[17] = word(rt, frame.at(0xB4u)); G[16] = word(rt, frame.at(0xB0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08A6F888(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_case(rt, ctx, true); }
void func_08A6FA6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_case(rt, ctx, false); }
// 0x08AD85F0 (a0 = the text, a1 = the output buffer or 0, a2 = the stop characters, a3 = the skip characters; v0 = the pointer after the token): a strtok-like step. Characters in the skip set (a3,
// 0x08B7BA50(set, character) non-zero) are passed over first; the token then runs up to a character of the stop set (a2) or the end, copied to the buffer when there is one (a
// byte with its high bit set is a lead byte: it and the next byte are taken together, skipping two). The copy ends with a zero after trailing skip characters are cut off. Past the token, one stop character
// is passed over and then any skip characters; the pointer there is returned.
void func_08AD85F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 text = ctx.gpr[4], buffer = ctx.gpr[5], stop_set = ctx.gpr[6], skip_set = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x14u}});
    ctx.gpr[20] = buffer;
    ctx.gpr[19] = stop_set;
    ctx.gpr[18] = skip_set;
    ctx.gpr[17] = buffer;
    ctx.gpr[16] = text;
    u32 read = text, write = buffer;
    auto in_set = [&](u32 set, u32 character) { return call_set(rt, ctx, 0x08B7BA50u, {set, character}) != 0u; };
    auto here = [&] { return signed_byte(rt, read); };
    auto sync = [&] {
        ctx.gpr[16] = read;
        ctx.gpr[17] = write;
    };
    // characters of the skip set at the start
    for (;;) {
        sync();
        const u32 c = here();
        ctx.gpr[2] = c;
        if (c == 0u || !in_set(skip_set, c)) break;
        ++read;
    }
    // the token
    for (;;) {
        sync();
        const u32 c = here();
        ctx.gpr[2] = c;
        if (c == 0u || in_set(stop_set, c)) break;
        const u32 byte = load8(read);
        if (static_cast<s32>(c) >= 0) {
            if (write != 0u) {
                store8(write, byte);
                ++write;
            }
            ++read;
        } else {
            ++read;
            if (write != 0u) {
                store8(write, byte);
                ++write;
                store8(write, load8(read));
                ++write;
            }
            ++read;
        }
    }
    sync();
    if (write != 0u) {
        // cut trailing skip characters off the copy
        for (;;) {
            if (write == buffer) {
                store8(write, 0u);
                break;
            }
            sync();
            if (!in_set(skip_set, signed_byte(rt, write - 1u))) {
                store8(write, 0u);
                break;
            }
            --write;
        }
        sync();
    }
    const u32 next = here();
    ctx.gpr[3] = load8(read);
    ctx.gpr[5] = next;
    if (next != 0u && in_set(stop_set, next)) ++read;
    for (;;) {
        sync();
        const u32 c = here();
        ctx.gpr[2] = c;
        if (c == 0u || !in_set(skip_set, c)) break;
        ++read;
    }
    ctx.gpr[2] = read;
}
// 0x08B59D6C (a0 = the table, a1 = an entry index): frees a 16-byte entry of a doubly linked table (entry: [+0] state, [+3] chain byte, [+4] previous index, [+5] next index, [+6] size halfword, [+8] a word; 0xFF is none; the
// free chain head is the byte at +0x206, the largest size at +0x208). A previous entry whose state is 1 (free) absorbs this one (its size grows, its next index becomes this one's, the next entry's previous index is
// fixed, the entry goes on the chain and the previous one becomes the current), then a next entry whose state is 1 absorbs the current one too (size added, its word copied, links fixed; 0x08B59BC0(table,
// table + 0x200, index) first when the current one's state is 1 as well). A final current entry whose state is not 1 goes through 0x08B59B7C(table, table + 0x200, 1, index). The largest size at +0x208 takes the maximum with
// the final entry's.
void func_08B59D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {16u, 0x0u}, {31u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}});
    u32 index = ctx.gpr[5];
    ctx.gpr[18] = index;
    ctx.gpr[16] = table;
    u32 offset = index << 4u;
    const u32 none = 0xFFu;
    const u32 entry = offset + table;
    u32 previous = load8(entry + 0x4u), next = load8(entry + 0x5u);
    ctx.gpr[17] = next;
    ctx.gpr[8] = none;
    if (previous != none) {
        const u32 before = (previous << 4u) + table;
        if (load8(before + 0x0u) == 1u) {
            const u32 size = load16(before + 0x6u) + load16(entry + 0x6u);
            store8(before + 0x5u, next);
            store16(before + 0x6u, size);
            if (next != none) store8(table + (next << 4u) + 0x4u, previous);
            store8(entry + 0x3u, load8(table + 0x206u));
            offset = previous << 4u;
            store8(table + 0x206u, index);
            index = previous;
            ctx.gpr[18] = index;
            previous = load8(before + 0x4u);
        }
    }
    ctx.gpr[9] = none;
    if (next != none) {
        const u32 next_offset = next << 4u;
        ctx.gpr[20] = next_offset;
        const u32 after = next_offset + table;
        const u32 after_state = load8(after + 0x0u);
        ctx.gpr[8] = after_state;
        if (after_state == 1u) {
            const u32 current = table + offset;
            ctx.gpr[19] = current;
            const u32 size = load16(after + 0x6u) + load16(current + 0x6u);
            const u32 carried = word(rt, current + 0x8u);
            store8(after + 0x4u, previous);
            store16(after + 0x6u, size);
            put(rt, after + 0x8u, carried);
            if (previous != none) store8(table + (previous << 4u) + 0x5u, next);
            if (load8(current + 0x0u) == after_state) call_set(rt, ctx, 0x08B59BC0u, {table, table + 0x200u, index});
            store8(current + 0x3u, load8(table + 0x206u));
            offset = next_offset;
            store8(table + 0x206u, index);
            index = next;
            ctx.gpr[18] = index;
        }
    }
    const u32 final_entry = offset + table;
    ctx.gpr[17] = final_entry;
    ctx.gpr[7] = index;
    if (load8(final_entry + 0x0u) != 1u) call_set(rt, ctx, 0x08B59B7Cu, {table, table + 0x200u, 1u, index});
    const u32 size = load16(final_entry + 0x6u), largest = load16(table + 0x208u);
    ctx.gpr[3] = size;
    ctx.gpr[2] = static_cast<s32>(largest) > static_cast<s32>(size) ? largest : size;
    store16(table + 0x208u, ctx.gpr[2]);
}
// The two case converters 0x08A6F888 (upper-case: 'a'..'z' lose 0x20) and 0x08A6FA6C (lower-case: 'A'..'Z' gain 0x20), a0 = the result string, a1 = the source; v0 = a0: build a copy of the source's text (0x08A6BF80(source) answers the text holder, 0x08B85EB8(frame + 4, its first word,
// frame + 1) makes the copy-on-write string at frame + 4: data pointer with the reference count at data - 4 and the length at data - 0xC) and convert it in place: for each index below the
// length read at the start, a character of the range becomes the character +- 0x20 (written after 0x08B88FF8 unshares the string when its reference count is not negative; an index at or above
// the current length reports 0x08B5AEDC(0x08CB0794) and goes on as the asm does). Then 0x08A6D2D4(result, frame + 4) assigns it and the temporary is released (0x08B60804(data - 4, -1)
// not above 0 calls 0x08B8542C(data - 0xC, frame)) unless it is the shared empty string at 0x08D74FD0.
static void convert_case(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool to_upper) {
    const u32 range_start = to_upper ? 0x61u : 0x41u;
    const u32 delta = to_upper ? 0xFFFFFFE0u : 0x20u;
    const u32 result = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {31u, 0x28u}, {20u, 0x20u}, {19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}, {18u, 0x18u}});
    const u32 string_slot = frame.at(4u);
    ctx.gpr[21] = result;
    ctx.gpr[20] = string_slot;
    ctx.gpr[17] = 0u;
    ctx.gpr[16] = 0u;
    const u32 holder = call_set(rt, ctx, 0x08A6BF80u, {source});
    call_set(rt, ctx, 0x08B85EB8u, {string_slot, word(rt, holder), frame.at(1u)});
    u32 data = word(rt, string_slot);
    const s32 length = static_cast<s32>(word(rt, data - 0xCu));
    ctx.gpr[19] = static_cast<u32>(length);
    ctx.gpr[4] = data;
    if (length > 0) {
        u32 index = 0u;
        bool at_body = true;
        const auto unshare_and_store = [&](u32 reference_holder) {
            u32 target;
            if (static_cast<s32>(word(rt, reference_holder + 8u)) < 0) {
                target = ctx.gpr[17] + ctx.gpr[4];
            } else {
                call_set(rt, ctx, 0x08B88FF8u, {string_slot});
                ctx.gpr[4] = word(rt, string_slot);
                target = ctx.gpr[17] + ctx.gpr[4];
            }
            store8(target, ctx.gpr[18]);
            ++index;
            ctx.gpr[16] = index;
        };
        for (;;) {
            u32 rep = data - 0xCu;
            bool store_through_throw = false;
            if (!at_body) {
                ctx.gpr[17] = index;
                if (!(index < word(rt, data - 0xCu))) {
                    call_set(rt, ctx, 0x08B5AEDCu, {0x08CB0794u});
                    store_through_throw = true;
                    unshare_and_store(ctx.gpr[2]);
                }
            }
            at_body = false;
            if (!store_through_throw) {
                if (static_cast<s32>(word(rt, rep + 8u)) >= 0) {
                    call_set(rt, ctx, 0x08B88FF8u, {string_slot});
                    data = word(rt, string_slot);
                    ctx.gpr[4] = data;
                }
                const u32 character = signed_byte(rt, data + ctx.gpr[17]);
                ctx.gpr[5] = character;
                if (((character - range_start) & 0xFFu) < 0x1Au) {
                    const u32 current_length = word(rt, data - 0xCu);
                    ctx.gpr[18] = sext(character + delta, 8u);
                    if (ctx.gpr[17] < current_length) {
                        unshare_and_store(data - 0xCu);
                    } else {
                        call_set(rt, ctx, 0x08B5AEDCu, {0x08CB0794u});
                        unshare_and_store(ctx.gpr[2]);
                    }
                } else {
                    ++index;
                    ctx.gpr[16] = index;
                }
            }
            data = word(rt, string_slot);
            ctx.gpr[4] = data;
            if (static_cast<u32>(length) == index) break;
            ctx.gpr[17] = index;
        }
    }
    call_set(rt, ctx, 0x08A6D2D4u, {result, string_slot});
    data = word(rt, string_slot);
    const u32 representation = data - 0xCu;
    ctx.gpr[16] = representation;
    if (representation != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) <= 0) call_set(rt, ctx, 0x08B8542Cu, {representation, frame.at(0u)});
    }
    ctx.gpr[2] = result;
}
void func_08862150(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); G[2] = G[5] << 3; G[3] = G[5] << 6; G[20] = G[4]; G[3] = G[3] - G[2]; G[2] = G[3] << 3; G[2] = G[2] - G[3]; G[16] = G[5]; G[4] = word(rt, G[4] + 0x0u); G[6] = word(rt, G[4] + 0x3F50u); G[4] = G[4] + 0x401Cu; G[7] = word(rt, G[6] + 0x0u); G[17] = G[7] + G[2]; call_here(rt, ctx, 0x08866434u); G[4] = G[17]; G[19] = G[2]; call_here(rt, ctx, 0x0885D5B0u); put(rt, frame.at(0x0u), G[16]); G[18] = G[2]; G[4] = frame.at(0u); G[2] = load8(G[17] + 0x15u); G[5] = G[17] + 0x4u; store8(frame.at(0xAu), G[18]); G[2] = 0u < G[2] ? 1u : 0u; put(rt, frame.at(0x4u), G[2]); call_here(rt, ctx, 0x08866654u); G[4] = frame.at(0u); G[5] = G[17]; call_here(rt, ctx, 0x088666B4u); G[2] = word(rt, G[17] + 0x28u); G[5] = frame.at(0x20u); G[7] = frame.at(0x22u); store8(frame.at(0xBu), G[2]); G[6] = 0xFFFFFFFFu; G[2] = word(rt, G[17] + 0x38u); G[3] = word(rt, G[17] + 0x3Cu); put(rt, frame.at(0x10u), G[2]); put(rt, frame.at(0x14u), G[3]); G[4] = word(rt, G[17] + 0x18u); put(rt, frame.at(0x18u), G[4]); store8(G[5] + 0x0u, G[6]); L_0886220C: ; G[5] = G[5] + 0x1u; if (G[7] != G[5]) { store8(G[5] + 0x0u, G[6]); goto L_0886220C; } G[2] = load8(G[17] + 0x15u); { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08862230; } store8(frame.at(0x8u), 0u); store8(frame.at(0x9u), 0u); goto L_08862238; L_08862230: ; store8(frame.at(0x9u), G[2]); store8(frame.at(0x8u), G[2]); L_08862238: ; G[4] = G[17]; call_here(rt, ctx, 0x0885D54Cu); G[16] = G[2]; G[2] = 0x2u; put(rt, G[17] + 0x4Cu, G[2]); call_here(rt, ctx, 0x08866174u); G[5] = G[2]; G[7] = G[16]; G[4] = G[19]; G[6] = frame.at(0u); G[8] = G[18]; G[9] = G[17] + 0x58u; call_here(rt, ctx, 0x088661B8u); G[2] = word(rt, G[20] + 0x0u); G[16] = word(rt, G[2] + 0x5ACu); call_here(rt, ctx, 0x08866174u); G[6] = G[2]; G[4] = G[16]; G[5] = G[19]; G[7] = 0u; call_here(rt, ctx, 0x088DEA1Cu); { const bool tk = G[2] != 0u; G[3] = 0x3u; if (tk) goto L_08862298; } put(rt, G[17] + 0x4Cu, G[3]); L_08862298: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08862A90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[2] = word(rt, G[4] + 0x0u); G[16] = G[2] + 0x3F80u; G[4] = G[16]; G[17] = word(rt, G[2] + 0x5ACu); call_here(rt, ctx, 0x08891608u); if (G[2] == 0u) { G[16] = 0u; goto L_08862BD4; } G[4] = G[16]; call_here(rt, ctx, 0x08891544u); if (G[2] == 0u) { G[16] = 0u; goto L_08862BD4; } G[4] = G[16]; call_here(rt, ctx, 0x088915C8u); G[18] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x088D65DCu); G[4] = G[18]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08890478u); G[4] = G[17] + 0x4u; G[5] = frame.at(0u); call_here(rt, ctx, 0x088D6114u); if (G[2] == 0u) { G[16] = 0u; goto L_08862BB4; } G[4] = G[16]; call_here(rt, ctx, 0x088915D4u); G[3] = word(rt, G[2] + 0xD8u); G[4] = G[17]; G[16] = ((G[3] & 0xFFu) << 24u) | ((G[3] & 0xFF00u) << 8u) | ((G[3] & 0xFF0000u) >> 8u) | ((G[3] & 0xFF000000u) >> 24u); call_here(rt, ctx, 0x088DD870u); if (G[16] != G[2]) { G[16] = 0u; goto L_08862BB4; } goto L_08862BC4; L_08862B34: ; G[20] = frame.at(0x8u); G[4] = G[20]; L_08862B3C: ; G[5] = 0u; call_here(rt, ctx, 0x088A79ACu); G[5] = G[19]; G[6] = G[20]; G[4] = G[18]; call_here(rt, ctx, 0x088902ECu); G[5] = G[19]; G[4] = G[18]; call_here(rt, ctx, 0x088909BCu); G[5] = word(rt, G[2] + 0x0u); G[4] = G[17]; G[19] = G[19] + 0x1u; G[5] = ((G[5] & 0xFFu) << 24u) | ((G[5] & 0xFF00u) << 8u) | ((G[5] & 0xFF0000u) >> 8u) | ((G[5] & 0xFF000000u) >> 24u); call_here(rt, ctx, 0x088DCD90u); G[4] = G[2]; { const bool tk = G[2] == 0u; G[5] = G[20]; if (tk) goto L_08862BA8; } call_here(rt, ctx, 0x088A41CCu); { const bool tk = G[2] == 0u; G[4] = G[20]; if (tk) goto L_08862BA8; } call_here(rt, ctx, 0x088A7548u); { const bool tk = G[16] != G[19]; G[4] = G[20]; if (tk) goto L_08862B3C; } G[16] = 0x1u; goto L_08862BB4; L_08862BA8: ; G[4] = G[20]; call_here(rt, ctx, 0x088A7548u); G[16] = 0u; L_08862BB4: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x088D5CF0u); G[2] = G[16]; goto L_08862BD8; L_08862BC4: ; if (static_cast<std::int32_t>(G[16]) <= 0) { G[16] = 0x1u; goto L_08862BB4; } G[19] = 0u; goto L_08862B34; L_08862BD4: ; G[2] = G[16]; L_08862BD8: ; goto exit_;  exit_: ; G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0894FCA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[17] = G[4]; G[2] = word(rt, G[4] + 0x0u); G[16] = word(rt, G[4] + 0x8u); G[4] = word(rt, G[2] + 0x10u); if (G[4] == 0u) { G[2] = load8(G[16] + 0x7u); goto L_0894FD08; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x20u); { const std::uint32_t tg = G[2]; G[5] = load8(G[16] + 0x0u); call_here(rt, ctx, tg); } if (static_cast<std::int32_t>(G[2]) > 0) { F[1] = std::bit_cast<float>(G[2]); goto L_0894FDBC; } G[2] = load8(G[16] + 0x7u); goto L_0894FD08; L_0894FCF0: ; G[2] = load8(G[16] + 0x5F8u); G[3] = G[3] < 0x1u ? 1u : 0u; G[2] = G[2] < 0x1u ? 1u : 0u; if (G[3] != G[2]) { put_float(rt, G[16] + 0x534u, F[1]); goto L_0894FD04; } L_0894FD04: ; G[2] = load8(G[16] + 0x7u); L_0894FD08: ; G[2] = G[2] & 0x2u; { const bool tk = G[2] == 0u; if (tk) goto L_0894FE00; } G[2] = sext(load8(G[16] + 0x3u), 8); if (G[2] != 0u) { goto L_0894FE04; } G[2] = load8(G[16] + 0xEu); if (G[2] != 0u) { goto L_0894FE04; } F[12] = float_at(rt, G[16] + 0x6C4u); G[4] = G[17]; G[5] = 0u; call_here(rt, ctx, 0x0894F514u); G[2] = word(rt, G[17] + 0x0u); F[1] = float_at(rt, G[16] + 0x6C0u); F[4] = float_at(rt, G[16] + 0x538u); F[3] = float_at(rt, G[2] + 0x3B0u); F[1] = psp_mul(F[1], F[1]); F[5] = F[0]; F[3] = psp_mul(F[3], F[4]); F[2] = float_at(rt, G[2] + 0x3ACu); F[0] = float_at(rt, G[2] + 0x3A8u); F[2] = psp_mul(F[2], F[1]); F[0] = F[0] - F[3]; G[1] = 0x3F800000u; F[1] = std::bit_cast<float>(G[1]); F[0] = F[0] + F[2]; ctx.set_fpu_condition(F[1] < F[0]); if (ctx.fpu_condition()) { F[0] = F[1]; goto L_0894FD9C; } F[1] = std::bit_cast<float>(0u); ctx.set_fpu_condition(F[0] < F[1]); if (!ctx.fpu_condition()) { F[1] = psp_mul(F[0], F[5]); goto L_0894FDA0; } F[0] = F[1]; L_0894FD9C: ; F[1] = psp_mul(F[0], F[5]); L_0894FDA0: ; F[0] = float_at(rt, G[16] + 0x530u); ctx.set_fpu_condition(F[0] < F[1]); if (ctx.fpu_condition()) { put_float(rt, G[16] + 0x534u, F[1]); goto L_0894FDFC; } goto L_0894FE00; L_0894FDBC: ; G[1] = 0x3E8E0000u; G[1] = G[1] | 0x38E4u; F[12] = std::bit_cast<float>(G[1]); G[4] = G[17]; F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); G[5] = frame.at(0u); F[12] = psp_mul(F[0], F[12]); call_here(rt, ctx, 0x0894F514u); F[1] = F[0]; F[0] = float_at(rt, G[16] + 0x534u); ctx.set_fpu_condition(F[1] < F[0]); if (ctx.fpu_condition()) { G[3] = sext(load8(G[16] + 0x510u), 8); goto L_0894FCF0; } G[2] = load8(G[16] + 0x7u); goto L_0894FD08; L_0894FDFC: ; L_0894FE00: ; L_0894FE04: ; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x089AF064 / 0x089AF1CC share this body (a2 = 1, a3 = a string; any other a2 does nothing and v0 = 1): a registry (0x089AB370 on frame +4; its ordered map on frame +0x10 with the sentinel at +0x18)
// is searched with the key 0x089FD934() (0x08BF9104(map, &key) gives the node nearest; no node, or a key below the node's key (+0x10), inserts 0x08BF99BC(map, node, {key, 0})). The node's
// +0x14 gets a new 0xC-byte object (vtables 0x08CCED78 / 0x08BFAD84, a zero). A string copy (0x08A76ABC(frame +0x2C, string)) is then handed to `finish`(the registry's +0x14 word, frame +0x10) (a null
// registry calls 0x08A5BB7C(0x08CA50C4)): 0x0897C0E4 for 0x089AF064, 0x0897876C for 0x089AF1CC; the strings and the map are released (0x08A5B9EC, 0x089FD82C).
void register_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 finish) {
    const u32 mode = ctx.gpr[6], string = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x44u}, {19u, 0x3Cu}, {18u, 0x38u}, {16u, 0x30u}, {31u, 0x48u}, {20u, 0x40u}, {17u, 0x34u}});
    const u32 registry = frame.at(0x4u), map = frame.at(0x14u), sentinel = frame.at(0x18u), table = frame.at(0x10u);
    ctx.gpr[2] = 0x1u;
    ctx.gpr[21] = registry;
    ctx.gpr[4] = registry;
    ctx.gpr[19] = map;
    ctx.gpr[18] = string;
    ctx.gpr[16] = sentinel;
    if (mode != 1u) return;
    call_here(rt, ctx, 0x089AB370u);
    put(rt, table, 0x08CCFD88u);
    put(rt, frame.at(0x28u), 0u);
    put(rt, sentinel, 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), sentinel);
    put(rt, frame.at(0x24u), sentinel);
    call_here(rt, ctx, 0x089FD934u);
    const u32 key = ctx.gpr[2];
    put(rt, frame.at(0x0u), key);
    const u32 node = call_set(rt, ctx, 0x08BF9104u, {map, frame.at(0x0u)});
    ctx.gpr[5] = node;
    ctx.gpr[17] = node;
    u32 entry = node;
    bool insert = true;
    if (sentinel != node) insert = word(rt, frame.at(0x0u)) < word(rt, node + 0x10u);
    if (insert) {
        const u32 wanted = word(rt, frame.at(0x0u));
        put(rt, frame.at(0x8u), wanted);
        put(rt, frame.at(0xCu), 0u);
        entry = call_set(rt, ctx, 0x08BF99BCu, {map, node, frame.at(0x8u)});
        ctx.gpr[17] = entry;
    }
    const u32 object = call_set(rt, ctx, 0x08B62344u, {0xCu});
    put(rt, object + 0x0u, 0x08CCED78u);
    put(rt, object + 0x4u, 0x08BFAD84u);
    const u32 copy = frame.at(0x2Cu), clean_table = 0x08CCED68u;
    ctx.gpr[20] = copy;
    ctx.gpr[19] = clean_table;
    ctx.gpr[18] = table;
    put(rt, object + 0x8u, 0u);
    put(rt, entry + 0x14u, object);
    put(rt, table, clean_table);
    call_set(rt, ctx, 0x08A76ABCu, {copy, string});
    const u32 holder = word(rt, registry);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    call_set(rt, ctx, finish, {word(rt, holder + 0x14u), table});
    put(rt, table, clean_table);
    call_set(rt, ctx, 0x08A5B9ECu, {copy});
    call_set(rt, ctx, 0x089FD82Cu, {table});
    call_set(rt, ctx, 0x08A5B9ECu, {registry});
}
void func_089AF064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_string(rt, ctx, 0x0897C0E4u); }
void func_089AF1CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_string(rt, ctx, 0x0897876Cu); }
void func_08A508E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[21] = G[4] + 0x14u; G[2] = word(rt, G[21] + 0x14u); { const bool tk = G[2] == 0u; G[20] = G[5]; if (tk) goto L_08A50A24; } { const bool tk = G[5] == 0u; G[19] = G[4] + 0x18u; if (tk) goto L_08A50998; } G[18] = word(rt, G[21] + 0xCu); { const bool tk = G[19] == G[18]; G[16] = G[18]; if (tk) goto L_08A50A24; } L_08A50924: ; G[17] = word(rt, G[16] + 0x10u); G[5] = G[20]; G[4] = word(rt, G[17] + 0x0u); call_here(rt, ctx, 0x08B35FD8u); { const bool tk = G[2] != 0u; G[4] = G[16]; if (tk) goto L_08A5097C; } if (G[17] == 0u) { G[4] = G[18]; goto L_08A50958; } G[4] = G[17]; call_here(rt, ctx, 0x08A4FC60u); G[4] = G[17]; call_here(rt, ctx, 0x08B64654u); G[4] = G[18]; L_08A50958: ; G[5] = G[21] + 0x4u; put(rt, frame.at(0x0u), G[18]); call_here(rt, ctx, 0x08B5F444u); G[4] = G[2]; call_here(rt, ctx, 0x08B35254u); G[2] = word(rt, G[21] + 0x14u); G[2] = G[2] + 0xFFFFFFFFu; put(rt, G[21] + 0x14u, G[2]); goto L_08A50A24; L_08A5097C: ; call_here(rt, ctx, 0x08B5F16Cu); G[16] = G[2]; { const bool tk = G[19] != G[2]; G[18] = G[2]; if (tk) goto L_08A50924; } goto L_08A50A28; L_08A50998: ; G[4] = word(rt, G[21] + 0xCu); G[18] = G[21] + 0x4u; { const bool tk = G[4] == G[18]; G[17] = G[4]; if (tk) goto L_08A509D8; } L_08A509A8: ; G[16] = word(rt, G[4] + 0x10u); { const bool tk = G[16] == 0u; G[4] = G[16]; if (tk) goto L_08A509C4; } call_here(rt, ctx, 0x08A4FC60u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); L_08A509C4: ; G[4] = G[17]; call_here(rt, ctx, 0x08B5F16Cu); G[17] = G[2]; { const bool tk = G[2] != G[18]; G[4] = G[2]; if (tk) goto L_08A509A8; } L_08A509D8: ; G[5] = word(rt, G[21] + 0x8u); G[4] = G[21]; call_here(rt, ctx, 0x08C1CC2Cu); put(rt, G[21] + 0x10u, G[18]); G[2] = frame.at(0x4u); G[4] = G[21]; put(rt, G[21] + 0xCu, G[18]); G[5] = frame.at(0u); put(rt, G[21] + 0x8u, 0u); put(rt, G[21] + 0x14u, 0u); put(rt, frame.at(0x10u), G[2]); put(rt, frame.at(0x14u), 0u); put(rt, frame.at(0x4u), 0u); put(rt, frame.at(0x8u), 0u); put(rt, frame.at(0xCu), G[2]); call_here(rt, ctx, 0x08C1CE44u); G[4] = frame.at(0u); G[5] = word(rt, frame.at(0x8u)); call_here(rt, ctx, 0x08C1CC2Cu); L_08A50A24: ; L_08A50A28: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B29ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { auto &F = ctx.fpr; ctx.set_fpu_condition(F[13] < F[12]); { const bool tk = !ctx.fpu_condition(); F[2] = F[12]; if (tk) goto L_08B29EF4; } ctx.set_fpu_condition(F[14] < F[12]); { const bool tk = ctx.fpu_condition(); F[1] = F[12]; if (tk) goto L_08B29F08; } F[1] = F[14]; goto L_08B29F08; L_08B29EF4: ; ctx.set_fpu_condition(F[14] < F[13]); { const bool tk = ctx.fpu_condition(); F[1] = F[13]; if (tk) goto L_08B29F08; } F[1] = F[14]; L_08B29F08: ; ctx.set_fpu_condition(F[2] < F[13]); { const bool tk = !ctx.fpu_condition(); if (tk) goto L_08B29F30; } ctx.set_fpu_condition(F[2] < F[14]); { const bool tk = ctx.fpu_condition(); F[0] = F[2]; if (tk) goto L_08B29F44; } F[0] = F[14]; goto L_08B29F44; L_08B29F30: ; ctx.set_fpu_condition(F[13] < F[14]); { const bool tk = ctx.fpu_condition(); F[0] = F[13]; if (tk) goto L_08B29F44; } F[0] = F[14]; L_08B29F44: ; F[3] = F[1] - F[0]; F[0] = std::bit_cast<float>(0u); ctx.set_fpu_condition(F[1] == F[0]); { const bool tk = ctx.fpu_condition(); put_float(rt, G[6] + 0x0u, F[1]); if (tk) goto L_08B29F60; } F[0] = F[3] / F[1]; L_08B29F60: ; F[12] = std::bit_cast<float>(0u); put_float(rt, G[5] + 0x0u, F[0]); put_float(rt, G[4] + 0x0u, F[12]); F[0] = float_at(rt, G[5] + 0x0u); ctx.set_fpu_condition(F[0] == F[12]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_08B2A020; } ctx.set_fpu_condition(F[1] == F[2]); { const bool tk = !ctx.fpu_condition(); F[0] = F[13] - F[14]; if (tk) goto L_08B29F9C; } F[12] = F[0] / F[3]; put_float(rt, G[4] + 0x0u, F[12]); goto L_08B29FEC; L_08B29F9C: ; ctx.set_fpu_condition(F[1] == F[13]); { const bool tk = !ctx.fpu_condition(); F[0] = F[14] - F[2]; if (tk) goto L_08B29FC4; } G[1] = 0x40000000u; F[1] = std::bit_cast<float>(G[1]); F[0] = F[0] / F[3]; F[12] = F[0] + F[1]; put_float(rt, G[4] + 0x0u, F[12]); goto L_08B29FEC; L_08B29FC4: ; ctx.set_fpu_condition(F[1] == F[14]); if (!ctx.fpu_condition()) { F[12] = float_at(rt, G[4] + 0x0u); goto L_08B29FF0; } F[0] = F[2] - F[13]; G[1] = 0x40800000u; F[1] = std::bit_cast<float>(G[1]); F[0] = F[0] / F[3]; F[12] = F[0] + F[1]; put_float(rt, G[4] + 0x0u, F[12]); L_08B29FEC: ; F[12] = float_at(rt, G[4] + 0x0u); L_08B29FF0: ; G[1] = 0x42700000u; F[0] = std::bit_cast<float>(G[1]); F[1] = std::bit_cast<float>(0u); F[12] = psp_mul(F[12], F[0]); ctx.set_fpu_condition(F[12] < F[1]); { const bool tk = !ctx.fpu_condition(); put_float(rt, G[4] + 0x0u, F[12]); if (tk) goto L_08B2A020; } G[1] = 0x43B40000u; F[0] = std::bit_cast<float>(G[1]); F[12] = F[12] + F[0]; put_float(rt, G[4] + 0x0u, F[12]); L_08B2A020: ; G[1] = 0x43B40000u; F[0] = std::bit_cast<float>(G[1]); F[0] = F[12] / F[0]; put_float(rt, G[4] + 0x0u, F[0]); goto exit_;  exit_: ; } if (tail != 0u) jump(ctx, tail); } }
void func_08B37C24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[3] = static_cast<std::int32_t>(G[6]) < 0 ? 1u : 0u; G[2] = G[6] + 0x7u; if (G[3] == 0u) G[2] = G[6]; G[19] = G[5]; G[2] = sra(G[2], 3); G[7] = word(rt, G[5] + 0x0u); G[5] = sra(G[6], 31); G[5] = G[5] >> 29; G[7] = G[7] + G[2]; G[3] = G[6] + G[5]; G[8] = load8(G[7] + 0x0u); G[3] = G[3] & 0x7u; G[3] = G[3] - G[5]; G[2] = 0x1u; G[2] = G[2] << (G[3] & 31u); G[2] = G[2] | G[8]; store8(G[7] + 0x0u, G[2]); G[6] = G[6] << 2; G[2] = word(rt, G[4] + 0x4u); G[6] = G[6] + G[2]; G[17] = word(rt, G[6] + 0x0u); { const bool tk = G[17] == 0u; G[20] = G[4]; if (tk) goto L_08B37D6C; } G[2] = 0x08D70000u; G[18] = 0x08D694D4u; G[3] = word(rt, G[17] + 0x4u); L_08B37CA8: ; G[2] = word(rt, G[18] + 0x4u); if (G[3] == G[2]) { G[6] = word(rt, G[17] + 0x8u); goto L_08B37D14; } if (G[3] == 0u) { G[17] = word(rt, G[17] + 0xCu); goto L_08B37D64; } { const bool tk = G[2] == 0u; G[4] = G[3]; if (tk) goto L_08B37D60; } G[3] = word(rt, G[3] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } G[4] = word(rt, G[18] + 0x4u); G[16] = G[2]; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } if (G[16] != G[2]) { G[17] = word(rt, G[17] + 0xCu); goto L_08B37D64; } G[4] = word(rt, G[17] + 0x4u); G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[5] = word(rt, G[18] + 0x4u); call_here(rt, ctx, tg); } if (G[2] == 0u) { G[17] = word(rt, G[17] + 0xCu); goto L_08B37D64; } G[6] = word(rt, G[17] + 0x8u); L_08B37D14: ; G[5] = word(rt, G[19] + 0x0u); G[4] = G[20]; G[3] = static_cast<std::int32_t>(G[6]) < 0 ? 1u : 0u; G[2] = G[6] + 0x7u; if (G[3] == 0u) G[2] = G[6]; G[2] = sra(G[2], 3); G[3] = sra(G[6], 31); G[5] = G[5] + G[2]; G[3] = G[3] >> 29; G[7] = load8(G[5] + 0x0u); G[2] = G[6] + G[3]; G[2] = G[2] & 0x7u; G[2] = G[2] - G[3]; G[7] = sra(G[7], G[2] & 31u); G[7] = G[7] & 0x1u; { const bool tk = G[7] != 0u; G[5] = G[19]; if (tk) goto L_08B37D60; } call_here(rt, ctx, 0x08B37C24u); L_08B37D60: ; G[17] = word(rt, G[17] + 0xCu); L_08B37D64: ; if (G[17] != 0u) { G[3] = word(rt, G[17] + 0x4u); goto L_08B37CA8; } L_08B37D6C: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BA7B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[30] = G[4]; G[23] = G[5]; G[17] = G[5]; G[3] = word(rt, G[4] + 0x4u); G[2] = G[3] - G[6]; G[22] = sra(G[2], 3); { const bool tk = static_cast<std::int32_t>(G[22]) <= 0; put(rt, frame.at(0x10u), G[6]); if (tk) goto L_08BA7C18; } G[19] = G[6]; G[21] = 0u; L_08BA7B60: ; G[2] = word(rt, G[19] + 0x0u); G[3] = word(rt, G[17] + 0x4u); put(rt, G[17] + 0x0u, G[2]); G[18] = word(rt, G[19] + 0x4u); { const bool tk = G[18] == G[3]; G[20] = G[17] + 0x4u; if (tk) goto L_08BA7C04; } if (G[18] == 0u) { G[16] = word(rt, G[20] + 0x0u); goto L_08BA7B9C; } G[3] = G[18] + 0x4u; L_08BA7B84: ; G[2] = word(rt, G[3] + 0x0u); G[2] = G[2] + 0x1u; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_08BA7B84; } G[16] = word(rt, G[20] + 0x0u); L_08BA7B9C: ; { const bool tk = G[16] == 0u; G[3] = G[16] + 0x4u; if (tk) goto L_08BA7C00; } L_08BA7BA4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA7BA4; } if (G[4] != G[2]) { put(rt, G[20] + 0x0u, G[18]); goto L_08BA7C04; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_08BA7BD4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA7BD4; } if (G[4] != G[2]) { put(rt, G[20] + 0x0u, G[18]); goto L_08BA7C04; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_08BA7C00: ; put(rt, G[20] + 0x0u, G[18]); L_08BA7C04: ; G[21] = G[21] + 0x1u; G[19] = G[19] + 0x8u; { const bool tk = G[22] != G[21]; G[17] = G[17] + 0x8u; if (tk) goto L_08BA7B60; } G[3] = word(rt, G[30] + 0x4u); L_08BA7C18: ; G[4] = G[17]; G[5] = G[3]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08BA7AC8u); G[4] = word(rt, frame.at(0x10u)); G[3] = word(rt, G[30] + 0x4u); G[2] = G[4] - G[23]; G[2] = (G[2] & ~0x7u) | ((0u << 0) & 0x7u); G[3] = G[3] - G[2]; put(rt, G[30] + 0x4u, G[3]); G[2] = G[23]; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BA7D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[30] = G[4]; G[23] = G[5]; G[17] = G[5]; G[3] = word(rt, G[4] + 0x4u); G[2] = G[3] - G[6]; G[22] = sra(G[2], 3); { const bool tk = static_cast<std::int32_t>(G[22]) <= 0; put(rt, frame.at(0x10u), G[6]); if (tk) goto L_08BA7E38; } G[19] = G[6]; G[21] = 0u; L_08BA7D80: ; G[2] = word(rt, G[19] + 0x0u); G[3] = word(rt, G[17] + 0x4u); put(rt, G[17] + 0x0u, G[2]); G[18] = word(rt, G[19] + 0x4u); { const bool tk = G[18] == G[3]; G[20] = G[17] + 0x4u; if (tk) goto L_08BA7E24; } if (G[18] == 0u) { G[16] = word(rt, G[20] + 0x0u); goto L_08BA7DBC; } G[3] = G[18] + 0x4u; L_08BA7DA4: ; G[2] = word(rt, G[3] + 0x0u); G[2] = G[2] + 0x1u; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_08BA7DA4; } G[16] = word(rt, G[20] + 0x0u); L_08BA7DBC: ; { const bool tk = G[16] == 0u; G[3] = G[16] + 0x4u; if (tk) goto L_08BA7E20; } L_08BA7DC4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA7DC4; } if (G[4] != G[2]) { put(rt, G[20] + 0x0u, G[18]); goto L_08BA7E24; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_08BA7DF4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA7DF4; } if (G[4] != G[2]) { put(rt, G[20] + 0x0u, G[18]); goto L_08BA7E24; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_08BA7E20: ; put(rt, G[20] + 0x0u, G[18]); L_08BA7E24: ; G[21] = G[21] + 0x1u; G[19] = G[19] + 0x8u; { const bool tk = G[22] != G[21]; G[17] = G[17] + 0x8u; if (tk) goto L_08BA7D80; } G[3] = word(rt, G[30] + 0x4u); L_08BA7E38: ; G[4] = G[17]; G[5] = G[3]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08BA7C74u); G[4] = word(rt, frame.at(0x10u)); G[3] = word(rt, G[30] + 0x4u); G[2] = G[4] - G[23]; G[2] = (G[2] & ~0x7u) | ((0u << 0) & 0x7u); G[3] = G[3] - G[2]; put(rt, G[30] + 0x4u, G[3]); G[2] = G[23]; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BA9710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[30] = G[4]; G[23] = G[5]; G[17] = G[5]; G[3] = word(rt, G[4] + 0x4u); G[2] = G[3] - G[6]; G[22] = sra(G[2], 3); { const bool tk = static_cast<std::int32_t>(G[22]) <= 0; put(rt, frame.at(0x10u), G[6]); if (tk) goto L_08BA981C; } G[19] = G[6]; G[21] = 0u; L_08BA9764: ; G[2] = word(rt, G[19] + 0x0u); G[3] = word(rt, G[17] + 0x4u); put(rt, G[17] + 0x0u, G[2]); G[18] = word(rt, G[19] + 0x4u); { const bool tk = G[18] == G[3]; G[20] = G[17] + 0x4u; if (tk) goto L_08BA9808; } if (G[18] == 0u) { G[16] = word(rt, G[20] + 0x0u); goto L_08BA97A0; } G[3] = G[18] + 0x4u; L_08BA9788: ; G[2] = word(rt, G[3] + 0x0u); G[2] = G[2] + 0x1u; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_08BA9788; } G[16] = word(rt, G[20] + 0x0u); L_08BA97A0: ; { const bool tk = G[16] == 0u; G[3] = G[16] + 0x4u; if (tk) goto L_08BA9804; } L_08BA97A8: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA97A8; } if (G[4] != G[2]) { put(rt, G[20] + 0x0u, G[18]); goto L_08BA9808; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_08BA97D8: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA97D8; } if (G[4] != G[2]) { put(rt, G[20] + 0x0u, G[18]); goto L_08BA9808; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_08BA9804: ; put(rt, G[20] + 0x0u, G[18]); L_08BA9808: ; G[21] = G[21] + 0x1u; G[19] = G[19] + 0x8u; { const bool tk = G[22] != G[21]; G[17] = G[17] + 0x8u; if (tk) goto L_08BA9764; } G[3] = word(rt, G[30] + 0x4u); L_08BA981C: ; G[4] = G[17]; G[5] = G[3]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08BA8FB0u); G[4] = word(rt, frame.at(0x10u)); G[3] = word(rt, G[30] + 0x4u); G[2] = G[4] - G[23]; G[2] = (G[2] & ~0x7u) | ((0u << 0) & 0x7u); G[3] = G[3] - G[2]; put(rt, G[30] + 0x4u, G[3]); G[2] = G[23]; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x24u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_324
} // namespace gt::game

#define HOT324(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_324::name, gt::ReturnKind::kind)
HOT324(0x08C3F648u, func_08C3F648, Word);
// HOT324(0x08C47468u, func_08C47468, Word);  // no comparable synthetic case yet
// HOT324(0x08C475C8u, func_08C475C8, Word);  // no comparable synthetic case yet
HOT324(0x08C47EA4u, func_08C47EA4, Word);
HOT324(0x0881D2ECu, func_0881D2EC, Word);
// HOT324(0x088CF168u, func_088CF168, Word);  // no comparable synthetic case yet
// HOT324(0x089BEA78u, func_089BEA78, Word);  // no comparable synthetic case yet
// HOT324(0x089E02C4u, func_089E02C4, Word);  // no comparable synthetic case yet
HOT324(0x089F99C0u, func_089F99C0, Word);
// HOT324(0x08A4F8B4u, func_08A4F8B4, Word);  // no comparable synthetic case yet
HOT324(0x08A6F888u, func_08A6F888, Word);
HOT324(0x08A6FA6Cu, func_08A6FA6C, Word);
HOT324(0x08AD85F0u, func_08AD85F0, Word);
HOT324(0x08B59D6Cu, func_08B59D6C, Word);
// HOT324(0x08862150u, func_08862150, Word);  // no comparable synthetic case yet
HOT324(0x08862A90u, func_08862A90, Word);
// HOT324(0x0894FCA8u, func_0894FCA8, Word);  // no comparable synthetic case yet
HOT324(0x089AF064u, func_089AF064, Word);
HOT324(0x089AF1CCu, func_089AF1CC, Word);
HOT324(0x08A508E0u, func_08A508E0, Word);
HOT324(0x08B29ECCu, func_08B29ECC, Word);
HOT324(0x08B37C24u, func_08B37C24, Word);
// HOT324(0x08BA7B0Cu, func_08BA7B0C, Word);  // no comparable synthetic case yet
HOT324(0x08BA7D2Cu, func_08BA7D2C, Word);
// HOT324(0x08BA9710u, func_08BA9710, Word);  // no comparable synthetic case yet
