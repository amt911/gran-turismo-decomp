// Hot batch 331: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <initializer_list>
#include <cmath>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_331 {
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

// 0x08879E14 / 0x0887A0D8 share this body (a0 = the owner; `attach` differs): a record is read from the owner's stream (a reader on the frame,
// 0x08AB85F0, one value per 0x08AB856C: a name index, a kind, two references resolved by 0x088755B4 / 0x08875584, a flag) and, when the
// reader holds data (+0x10 > 0), a 0x7C-byte object is made (0x08B62344, 0x0887FFBC), linked (0x08875418), named (0x0886C798 + `attach`),
// enabled (0x0886ADEC(object, 1)) and added to the owner's list (0x0886B2E8). The name and the reader's text are released after.
void read_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 attach) {
    const u32 owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x20u}, {18u, 0x28u}, {31u, 0x40u}, {23u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {17u, 0x24u}});
    const u32 reader = frame.at(0x8u);
    ctx.gpr[16] = reader;
    ctx.gpr[18] = owner;
    call_set(rt, ctx, 0x08AB85F0u, {reader});
    auto next = [&] { return call_set(rt, ctx, 0x08AB856Cu, {reader}); };
    const u32 name_index = next();
    ctx.gpr[23] = name_index;
    const u32 kind = next();
    ctx.gpr[22] = kind;
    const u32 first = call_set(rt, ctx, 0x088755B4u, {owner, next()});
    ctx.gpr[21] = first;
    const u32 second = call_set(rt, ctx, 0x08875584u, {owner, next()});
    ctx.gpr[20] = second;
    const u32 flag = next();
    ctx.gpr[19] = flag;
    const u32 empty = 0x08D75298u;
    auto release = [&](u32 slot, u32 tag) {
        const u32 data = word(rt, slot), header = data - 0xCu;
        ctx.gpr[4] = data;
        ctx.gpr[16] = header;
        if (header == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B93554u, {header, tag});
    };
    if (static_cast<s32>(word(rt, frame.at(0x10u))) > 0) {
        const u32 list = word(rt, owner);
        ctx.gpr[17] = list;
        const u32 object = call_set(rt, ctx, 0x08B62344u, {0x7Cu});
        ctx.gpr[16] = object;
        call_set(rt, ctx, 0x0887FFBCu, {object, kind, list});
        call_set(rt, ctx, 0x08875418u, {owner, object, first, second, flag});
        call_set(rt, ctx, 0x0886C798u, {frame.at(0x4u), word(rt, owner), name_index});
        call_set(rt, ctx, attach, {object, word(rt, frame.at(0x4u))});
        call_set(rt, ctx, 0x0886ADECu, {object, 1u});
        call_set(rt, ctx, 0x0886B2E8u, {word(rt, owner), object});
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[17] = empty;
        release(frame.at(0x4u), frame.at(0x1u));
    } else {
        ctx.gpr[2] = empty;
    }
    release(frame.at(0x8u), frame.at(0u));
}

// 0x08BDDFDC / 0x08BDEC3C / 0x08C4F79C share this body: std::vector<T>::operator=(a0 = this, a1 = other) for 2- and 8-byte elements. Too small
// a capacity: new storage (0x08B35DA0) filled by `copy`, the old elements destroyed (`destroy`) and freed (0x08B35254). Enough elements: the
// first ones assigned and the rest destroyed (`destroy_tail`). Fewer: the existing ones assigned and the rest copied in (`copy`). v0 = this.
struct VecAssign {
    u32 bytes, shift, copy, destroy, destroy_tail;
};
void vector_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const VecAssign &k) {
    const u32 self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}});
    ctx.gpr[16] = self;
    ctx.gpr[2] = self;
    if (other == self) return;
    // count elements from `from` to `to`, in the asm's order (2 bytes: low then high; 8 bytes: both words read, then stored).
    auto assign = [&](u32 from, u32 to, u32 count) {
        for (u32 i = 0u; i < count; ++i, from += k.bytes, to += k.bytes) {
            if (k.bytes == 2u) {
                store8(to + 0x0u, load8(from + 0x0u));
                store8(to + 0x1u, load8(from + 0x1u));
            } else {
                const u32 lo = word(rt, from + 0x0u), hi = word(rt, from + 0x4u);
                put(rt, to + 0x0u, lo);
                put(rt, to + 0x4u, hi);
            }
        }
        return to;
    };
    u32 begin = word(rt, other + 0x0u), end = word(rt, other + 0x4u);
    ctx.gpr[18] = begin;
    ctx.gpr[19] = end;
    const u32 count = sra(end - begin, k.shift);
    ctx.gpr[17] = count;
    u32 grown;
    if (sra(word(rt, self + 0x8u) - word(rt, self + 0x0u), k.shift) < count) {
        const u32 bytes = count == 0u ? 0u : count << k.shift;
        const u32 storage = count == 0u ? 0u : call_set(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[21] = bytes;
        ctx.gpr[20] = storage;
        call_set(rt, ctx, k.copy, {begin, end, storage, frame.at(0u)});
        call_set(rt, ctx, k.destroy, {word(rt, self + 0x0u), word(rt, self + 0x4u), frame.at(0x3u)});
        const u32 old = word(rt, self + 0x0u);
        if (old != 0u) call_set(rt, ctx, 0x08B35254u, {old});
        put(rt, self + 0x8u, storage + bytes);
        put(rt, self + 0x0u, storage);
        grown = bytes;
    } else {
        const u32 first = word(rt, self + 0x0u), last = word(rt, self + 0x4u);
        const u32 size = sra(last - first, k.shift);
        if (!(size < count)) {
            const u32 stop = static_cast<s32>(count) > 0 ? assign(begin, first, count) : first;
            call_set(rt, ctx, k.destroy_tail, {stop, word(rt, self + 0x4u), frame.at(0x2u)});
        } else {
            if (static_cast<s32>(size) > 0) {
                assign(begin, first, size);
                begin = word(rt, other + 0x0u);
                end = word(rt, other + 0x4u);
            }
            const u32 now_first = word(rt, self + 0x0u), now_last = word(rt, self + 0x4u);
            call_set(rt, ctx, k.copy, {begin + ((now_last - now_first) & ~(k.bytes - 1u)), end, now_last, frame.at(0x1u)});
        }
        grown = count << k.shift;
    }
    ctx.gpr[3] = grown;
    put(rt, self + 0x4u, word(rt, self + 0x0u) + grown);
    ctx.gpr[2] = self;
}

void func_08B94ED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x3Cu), G[31]); G[2] = G[4] + 0x4u; G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; { const bool tk = G[2] != G[5]; if (tk) goto L_08B94F44; } G[2] = word(rt, G[4] + 0x14u); { const bool tk = G[2] == 0u; if (tk) goto L_08B94F1C; } G[6] = word(rt, G[4] + 0x10u); G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[6] + 0x10u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[7] = G[18]; if (tk) goto L_08B9501C; } L_08B94F1C: ; G[5] = G[18]; call_here(rt, ctx, 0x08B94DF8u); put(rt, frame.at(0x24u), G[3]); G[16] = G[2]; G[3] = G[2]; put(rt, frame.at(0x20u), G[2]); G[2] = load8(frame.at(0x24u)); put(rt, frame.at(0x10u), G[3]); store8(frame.at(0x14u), G[2]); goto L_08B95064; L_08B94F44: ; G[6] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[5] + 0x10u); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; if (tk) goto L_08B94FD0; } G[2] = word(rt, G[4] + 0xCu); { const bool tk = G[5] != G[2]; G[6] = G[5]; if (tk) goto L_08B94F74; } G[7] = G[18]; call_here(rt, ctx, 0x08B94D30u); G[16] = G[2]; goto L_08B95064; L_08B94F74: ; G[4] = G[5]; call_here(rt, ctx, 0x08B5F174u); G[6] = G[2]; G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[2] + 0x10u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[4] = G[17]; if (tk) goto L_08B94FA8; } G[2] = word(rt, G[6] + 0xCu); { const bool tk = G[2] != 0u; G[5] = G[16]; if (tk) goto L_08B9502C; } G[7] = G[18]; goto L_08B9501C; L_08B94FA8: ; G[5] = G[18]; call_here(rt, ctx, 0x08B94DF8u); put(rt, frame.at(0x24u), G[3]); G[16] = G[2]; G[3] = G[2]; put(rt, frame.at(0x20u), G[2]); G[2] = load8(frame.at(0x24u)); put(rt, frame.at(0x8u), G[3]); store8(frame.at(0xCu), G[2]); goto L_08B95064; L_08B94FD0: ; { const bool tk = G[2] == 0u; G[2] = G[16]; if (tk) goto L_08B95068; } G[2] = word(rt, G[4] + 0x10u); { const bool tk = G[2] == G[5]; G[6] = G[16]; if (tk) goto L_08B95018; } G[4] = G[5]; call_here(rt, ctx, 0x08B5F100u); G[5] = G[2]; G[3] = word(rt, G[5] + 0x10u); G[2] = word(rt, G[18] + 0x0u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[4] = G[17]; if (tk) goto L_08B95040; } G[2] = word(rt, G[16] + 0xCu); { const bool tk = G[2] != 0u; G[7] = G[18]; if (tk) goto L_08B95030; } G[4] = G[17]; G[6] = G[16]; L_08B95018: ; G[7] = G[18]; L_08B9501C: ; G[5] = 0u; call_here(rt, ctx, 0x08B94D30u); G[16] = G[2]; goto L_08B95064; L_08B9502C: ; G[7] = G[18]; L_08B95030: ; G[6] = G[5]; call_here(rt, ctx, 0x08B94D30u); G[16] = G[2]; goto L_08B95064; L_08B95040: ; G[5] = G[18]; call_here(rt, ctx, 0x08B94DF8u); put(rt, frame.at(0x24u), G[3]); G[16] = G[2]; G[3] = G[2]; put(rt, frame.at(0x20u), G[2]); G[2] = load8(frame.at(0x24u)); put(rt, frame.at(0x0u), G[3]); store8(frame.at(0x4u), G[2]); L_08B95064: ; G[2] = G[16]; L_08B95068: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BDDFDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[16] = G[4]; { const bool tk = G[5] == G[4]; if (tk) goto L_08BDE164; } G[2] = word(rt, G[4] + 0x8u); G[19] = word(rt, G[5] + 0x4u); G[18] = word(rt, G[5] + 0x0u); G[4] = word(rt, G[4] + 0x0u); G[3] = G[19] - G[18]; G[2] = G[2] - G[4]; G[17] = sra(G[3], 1); G[2] = sra(G[2], 1); G[2] = G[2] < G[17] ? 1u : 0u; if (G[2] == 0u) { G[8] = word(rt, G[16] + 0x4u); goto L_08BDE098; } G[21] = 0u; { const bool tk = G[17] == 0u; G[20] = 0u; if (tk) goto L_08BDE04C; } G[21] = G[17] << 1; G[4] = G[21]; call_here(rt, ctx, 0x08B35DA0u); G[20] = G[2]; L_08BDE04C: ; G[4] = G[18]; G[5] = G[19]; G[6] = G[20]; G[7] = frame.at(0u); call_here(rt, ctx, 0x08BDD8D8u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BDD7B8u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[20] + G[21]; if (tk) goto L_08BDE088; } call_here(rt, ctx, 0x08B35254u); G[2] = G[20] + G[21]; L_08BDE088: ; G[3] = G[21]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x0u, G[20]); goto L_08BDE158; L_08BDE098: ; G[2] = G[8] - G[4]; G[2] = sra(G[2], 1); G[3] = G[2] < G[17] ? 1u : 0u; { const bool tk = G[3] != 0u; G[9] = G[2]; if (tk) goto L_08BDE0F8; } { const bool tk = static_cast<std::int32_t>(G[17]) <= 0; G[6] = G[4]; if (tk) goto L_08BDE0E0; } G[4] = G[18]; G[5] = 0u; L_08BDE0BC: ; G[2] = load8(G[4] + 0x0u); G[5] = G[5] + 0x1u; store8(G[6] + 0x0u, G[2]); G[3] = load8(G[4] + 0x1u); G[4] = G[4] + 0x2u; store8(G[6] + 0x1u, G[3]); { const bool tk = G[17] != G[5]; G[6] = G[6] + 0x2u; if (tk) goto L_08BDE0BC; } G[8] = word(rt, G[16] + 0x4u); L_08BDE0E0: ; G[4] = G[6]; G[5] = G[8]; G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B8CF98u); G[3] = G[17] << 1; goto L_08BDE158; L_08BDE0F8: ; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[7] = G[4]; if (tk) goto L_08BDE138; } G[4] = G[18]; G[6] = 0u; L_08BDE108: ; G[2] = load8(G[4] + 0x0u); G[6] = G[6] + 0x1u; store8(G[7] + 0x0u, G[2]); G[3] = load8(G[4] + 0x1u); G[4] = G[4] + 0x2u; store8(G[7] + 0x1u, G[3]); { const bool tk = G[9] != G[6]; G[7] = G[7] + 0x2u; if (tk) goto L_08BDE108; } G[18] = word(rt, G[5] + 0x0u); G[19] = word(rt, G[5] + 0x4u); G[4] = word(rt, G[16] + 0x0u); G[8] = word(rt, G[16] + 0x4u); L_08BDE138: ; G[4] = G[8] - G[4]; G[4] = (G[4] & ~0x1u) | ((0u << 0) & 0x1u); G[4] = G[18] + G[4]; G[5] = G[19]; G[6] = G[8]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08BDD8D8u); G[3] = G[17] << 1; L_08BDE158: ; G[2] = word(rt, G[16] + 0x0u); G[2] = G[2] + G[3]; put(rt, G[16] + 0x4u, G[2]); L_08BDE164: ; G[2] = G[16]; goto exit_;  exit_: ; G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BDEC3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[16] = G[4]; { const bool tk = G[5] == G[4]; if (tk) goto L_08BDEDC4; } G[2] = word(rt, G[4] + 0x8u); G[19] = word(rt, G[5] + 0x4u); G[18] = word(rt, G[5] + 0x0u); G[4] = word(rt, G[4] + 0x0u); G[3] = G[19] - G[18]; G[2] = G[2] - G[4]; G[17] = sra(G[3], 1); G[2] = sra(G[2], 1); G[2] = G[2] < G[17] ? 1u : 0u; if (G[2] == 0u) { G[8] = word(rt, G[16] + 0x4u); goto L_08BDECF8; } G[21] = 0u; { const bool tk = G[17] == 0u; G[20] = 0u; if (tk) goto L_08BDECAC; } G[21] = G[17] << 1; G[4] = G[21]; call_here(rt, ctx, 0x08B35DA0u); G[20] = G[2]; L_08BDECAC: ; G[4] = G[18]; G[5] = G[19]; G[6] = G[20]; G[7] = frame.at(0u); call_here(rt, ctx, 0x08BDD910u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08BDD7C8u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[20] + G[21]; if (tk) goto L_08BDECE8; } call_here(rt, ctx, 0x08B35254u); G[2] = G[20] + G[21]; L_08BDECE8: ; G[3] = G[21]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x0u, G[20]); goto L_08BDEDB8; L_08BDECF8: ; G[2] = G[8] - G[4]; G[2] = sra(G[2], 1); G[3] = G[2] < G[17] ? 1u : 0u; { const bool tk = G[3] != 0u; G[9] = G[2]; if (tk) goto L_08BDED58; } { const bool tk = static_cast<std::int32_t>(G[17]) <= 0; G[6] = G[4]; if (tk) goto L_08BDED40; } G[4] = G[18]; G[5] = 0u; L_08BDED1C: ; G[2] = load8(G[4] + 0x0u); G[5] = G[5] + 0x1u; store8(G[6] + 0x0u, G[2]); G[3] = load8(G[4] + 0x1u); G[4] = G[4] + 0x2u; store8(G[6] + 0x1u, G[3]); { const bool tk = G[17] != G[5]; G[6] = G[6] + 0x2u; if (tk) goto L_08BDED1C; } G[8] = word(rt, G[16] + 0x4u); L_08BDED40: ; G[4] = G[6]; G[5] = G[8]; G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B8CFE0u); G[3] = G[17] << 1; goto L_08BDEDB8; L_08BDED58: ; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[7] = G[4]; if (tk) goto L_08BDED98; } G[4] = G[18]; G[6] = 0u; L_08BDED68: ; G[2] = load8(G[4] + 0x0u); G[6] = G[6] + 0x1u; store8(G[7] + 0x0u, G[2]); G[3] = load8(G[4] + 0x1u); G[4] = G[4] + 0x2u; store8(G[7] + 0x1u, G[3]); { const bool tk = G[9] != G[6]; G[7] = G[7] + 0x2u; if (tk) goto L_08BDED68; } G[18] = word(rt, G[5] + 0x0u); G[19] = word(rt, G[5] + 0x4u); G[4] = word(rt, G[16] + 0x0u); G[8] = word(rt, G[16] + 0x4u); L_08BDED98: ; G[4] = G[8] - G[4]; G[4] = (G[4] & ~0x1u) | ((0u << 0) & 0x1u); G[4] = G[18] + G[4]; G[5] = G[19]; G[6] = G[8]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08BDD910u); G[3] = G[17] << 1; L_08BDEDB8: ; G[2] = word(rt, G[16] + 0x0u); G[2] = G[2] + G[3]; put(rt, G[16] + 0x4u, G[2]); L_08BDEDC4: ; G[2] = G[16]; goto exit_;  exit_: ; G[16] = word(rt, frame.at(0x10u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C4F79C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_assign(rt, ctx, VecAssign{8u, 3u, 0x08C4F134u, 0x08C4F124u, 0x08C4F16Cu});
}
void func_08C752BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0x0u), G[16]); G[8] = G[6]; G[19] = G[4]; G[18] = G[7]; G[16] = word(rt, G[4] + 0x0u); G[3] = word(rt, G[16] + 0xFFFFFFF4u); G[2] = G[3] < G[5] ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = G[5]; if (tk) goto L_08C753E0; } G[2] = 0x3FFF0000u; G[2] = G[2] | 0xFFFCu; G[2] = G[2] - G[3]; G[2] = G[2] < G[7] ? 1u : 0u; if (G[2] != 0u) { G[4] = 0x08CB0000u; goto L_08C753EC; } G[2] = G[6] < G[16] ? 1u : 0u; { const bool tk = G[2] != 0u; G[4] = G[19]; if (tk) goto L_08C75334; } G[2] = G[16] + G[3]; G[2] = G[2] < G[6] ? 1u : 0u; { const bool tk = G[2] != 0u; G[5] = G[17]; if (tk) goto L_08C75338; } G[2] = word(rt, G[16] + 0xFFFFFFFCu); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; if (tk) goto L_08C75360; } L_08C75334: ; G[5] = G[17]; L_08C75338: ; G[7] = G[8]; G[8] = G[18]; G[6] = 0u; tail = 0x08C74D38u; goto exit_; L_08C75360: ; G[6] = 0u; G[7] = G[18]; G[16] = G[8] - G[16]; call_here(rt, ctx, 0x08C74B68u); G[2] = word(rt, G[19] + 0x0u); G[5] = G[2] + G[16]; G[3] = G[5] + G[18]; G[16] = G[2] + G[17]; G[2] = G[16] < G[3] ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = G[5] < G[16] ? 1u : 0u; if (tk) goto L_08C753C0; } G[2] = 0x1u; { const bool tk = G[18] != G[2]; G[4] = G[16]; if (tk) goto L_08C753F4; } G[2] = load8(G[5] + 0x0u); store8(G[16] + 0x0u, G[2]); L_08C753A0: ; G[2] = G[19]; L_08C753A4: ; goto exit_; L_08C753C0: ; { const bool tk = G[2] != 0u; G[17] = G[16] - G[5]; if (tk) goto L_08C75404; } G[2] = 0x1u; { const bool tk = G[18] != G[2]; G[4] = G[16]; if (tk) goto L_08C75438; } G[2] = load8(G[3] + 0x0u); store8(G[16] + 0x0u, G[2]); goto L_08C753A0; L_08C753E0: ; G[4] = 0x08CB0000u; G[4] = 0x08CB58F0u; call_here(rt, ctx, 0x08B5AEDCu); L_08C753EC: ; G[4] = 0x08CB58F0u; call_here(rt, ctx, 0x08B5ADDCu); L_08C753F4: ; G[6] = G[18]; call_here(rt, ctx, 0x08B79A88u); G[2] = G[19]; goto L_08C753A4; L_08C75404: ; G[2] = 0x1u; { const bool tk = G[17] != G[2]; G[4] = G[16]; if (tk) goto L_08C7545C; } G[2] = load8(G[5] + 0x0u); store8(G[16] + 0x0u, G[2]); G[6] = G[18] - G[17]; L_08C7541C: ; G[2] = 0x1u; G[5] = G[16] + G[18]; { const bool tk = G[6] != G[2]; G[4] = G[16] + G[17]; if (tk) goto L_08C7544C; } G[2] = load8(G[5] + 0x0u); store8(G[4] + 0x0u, G[2]); goto L_08C753A0; L_08C75438: ; G[5] = G[3]; G[6] = G[18]; call_here(rt, ctx, 0x08B79A88u); G[2] = G[19]; goto L_08C753A4; L_08C7544C: ; call_here(rt, ctx, 0x08B79A88u); G[2] = G[19]; goto L_08C753A4; L_08C7545C: ; G[6] = G[17]; call_here(rt, ctx, 0x08B79A88u); G[6] = G[18] - G[17]; goto L_08C7541C;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0884D1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xC0u); put(rt, frame.at(0xA8u), G[18]); put(rt, frame.at(0xA4u), G[17]); put(rt, frame.at(0xB4u), G[31]); put(rt, frame.at(0xB0u), G[20]); put(rt, frame.at(0xACu), G[19]); put(rt, frame.at(0xA0u), G[16]); G[18] = G[4]; G[17] = G[5]; { const bool tk = static_cast<std::int32_t>(G[6]) <= 0; if (tk) goto L_0884D2BC; } G[16] = word(rt, G[7] + 0x0u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_0884D224; } G[4] = 0x08CA0000u; G[4] = 0x08C9D08Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_0884D224: ; G[5] = G[16]; G[4] = frame.at(0x14u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[20] = frame.at(0x10u); call_here(rt, ctx, tg); } G[4] = word(rt, frame.at(0x14u)); call_here(rt, ctx, 0x08930170u); G[5] = G[17]; G[4] = G[20]; G[18] = G[2]; G[19] = G[3]; call_here(rt, ctx, 0x0884C868u); G[16] = word(rt, frame.at(0x10u)); if (G[16] != 0u) { put(rt, G[16] + 0x18u, G[18]); goto L_0884D270; } G[4] = 0x08CA0000u; G[4] = 0x08C9D094u; call_here(rt, ctx, 0x08A5BB7Cu); put(rt, G[16] + 0x18u, G[18]); L_0884D270: ; G[4] = G[20]; put(rt, G[16] + 0x1Cu, G[19]); call_here(rt, ctx, 0x08A5B9ECu); G[2] = 0x08D70000u; G[4] = word(rt, frame.at(0x14u)); G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; if (tk) goto L_0884D378; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { goto L_0884D378; } G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); goto L_0884D378; L_0884D2BC: ; G[17] = frame.at(0xCu); G[4] = G[17]; call_here(rt, ctx, 0x0884C868u); G[16] = word(rt, frame.at(0xCu)); if (G[16] != 0u) { G[5] = word(rt, G[16] + 0x1Cu); goto L_0884D2E4; } G[4] = 0x08CA0000u; G[4] = 0x08C9D094u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[16] + 0x1Cu); L_0884D2E4: ; G[4] = word(rt, G[16] + 0x18u); G[16] = frame.at(0x14u); G[6] = G[16]; G[7] = 0x80u; call_here(rt, ctx, 0x0892FFE0u); G[4] = G[17]; G[17] = frame.at(0x4u); call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[16]; G[4] = G[17]; G[16] = frame.at(0x8u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_0884D334; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_0884D334: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; if (tk) goto L_0884D378; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) goto L_0884D378; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_0884D378: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[20] = word(rt, frame.at(0xB0u)); G[19] = word(rt, frame.at(0xACu)); G[16] = word(rt, frame.at(0xA0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08879E14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { read_record(rt, ctx, 0x0887F7C8u); }
void func_0887A0D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { read_record(rt, ctx, 0x0887F870u); }
// 0x0889E534 (a0 = the owner, a1 = a source): the owner's selector (+0x4C) is set from the source (+0x10, 0x08B1C218); its two indices (+8, +0xC;
// negative = none) pick slots (+0x60 + i x 0xAD8) that 0x088A0BD8 blends at the source's weight (+0x20) into a frame object; that object's
// header, its four 0x1C-byte and four 0x14-byte entries are copied into the owner's +0x3698 block, and the frame object is destroyed (vtables
// 0x08CC2590 / 0x08CC26B4, each entry's virtual +0, last first).
void func_0889E534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x100u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0xE8u}, {17u, 0xE4u}, {16u, 0xE0u}, {31u, 0xF0u}, {19u, 0xECu}});
    auto &F = ctx.fpr;
    ctx.gpr[18] = source;
    ctx.gpr[17] = owner;
    const u32 selector = owner + 0x4Cu;
    call_set(rt, ctx, 0x08B1C218u, {selector, word(rt, source + 0x10u)});
    const u32 i = word(rt, selector + 0x8u), j = word(rt, selector + 0xCu);
    ctx.gpr[3] = i;
    ctx.gpr[4] = j;
    // slot of index n (mult leaves its 64-bit product in lo / hi, as the asm's mult does)
    auto slot = [&](u32 n) {
        if (static_cast<s32>(n) < 0) return 0u;
        const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(n)) * 0xAD8;
        ctx.lo = static_cast<u32>(product);
        ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
        ctx.gpr[2] = ctx.lo + 0x60u;
        return owner + ctx.lo + 0x60u;
    };
    const u32 first = slot(i), second = slot(j);
    const u32 block = owner + 0x3698u;
    ctx.gpr[16] = block;
    F[12] = float_at(rt, source + 0x20u);
    const u32 blended = frame.at(0u);
    ctx.gpr[19] = frame.at(0x18u);
    call_set(rt, ctx, 0x088A0BD8u, {blended, first, second});
    put(rt, block + 0x8u, word(rt, frame.at(0x8u)));
    put(rt, block + 0xCu, word(rt, frame.at(0xCu)));
    put_float(rt, block + 0x10u, float_at(rt, frame.at(0x10u)));
    put_float(rt, block + 0x14u, float_at(rt, frame.at(0x14u)));
    for (u32 k = 0u; k < 4u; ++k) {
        const u32 from = frame.at(0x18u + 0x1Cu * k), to = block + 0x18u + 0x1Cu * k;
        const u32 w18 = word(rt, from + 0x18u), w4 = word(rt, from + 0x4u), w8 = word(rt, from + 0x8u);
        F[0] = float_at(rt, from + 0xCu);
        F[1] = float_at(rt, from + 0x10u);
        put(rt, to + 0x4u, w4);
        put(rt, to + 0x8u, w8);
        put_float(rt, to + 0xCu, F[0]);
        put_float(rt, to + 0x10u, F[1]);
        put(rt, to + 0x18u, w18);
    }
    for (u32 k = 0u; k < 4u; ++k) {
        const u32 from = frame.at(0x88u + 0x14u * k), to = block + 0x88u + 0x14u * k;
        F[1] = float_at(rt, from + 0x10u);
        const u32 w4 = word(rt, from + 0x4u), w8 = word(rt, from + 0x8u);
        F[0] = float_at(rt, from + 0xCu);
        put(rt, to + 0x4u, w4);
        put(rt, to + 0x8u, w8);
        put_float(rt, to + 0xCu, F[0]);
        put_float(rt, to + 0x10u, F[1]);
    }
    put(rt, frame.at(0x0u), 0x08CC2590u);
    put(rt, frame.at(0x4u), 0x08CC26B4u);
    ctx.gpr[18] = frame.at(0x88u);
    for (u32 at = frame.at(0xC4u);; at -= 0x14u) {
        ctx.gpr[16] = at;
        ctx.gpr[17] = at - 0x14u;
        const u32 destroy = word(rt, word(rt, at));
        ctx.gpr[2] = word(rt, at);
        ctx.gpr[3] = destroy;
        call_set(rt, ctx, destroy, {at});
        if (at == frame.at(0x88u)) break;
    }
    for (u32 at = frame.at(0x6Cu);; at -= 0x1Cu) {
        ctx.gpr[16] = at;
        ctx.gpr[17] = at - 0x1Cu;
        const u32 destroy = word(rt, word(rt, at));
        ctx.gpr[2] = word(rt, at);
        ctx.gpr[3] = destroy;
        call_set(rt, ctx, destroy, {at});
        if (at == frame.at(0x18u)) break;
    }
}
// 0x08916D48 (a0 = the object, a1 = a source, a2 = a flag, a3 = a provider, t0 = a receiver): with the flag and a provider, the object's
// helper (+4) is re-keyed when the source's handle (+0xC, its virtual +8) changed (0x0890AF70, 0x0890AEC4; +0x10 / +0x14 remember it), the
// provider's count of points (virtual +0x6C into the frame) is averaged with the helper's point (x 1/(count + 1); 0x0890AF40 / 0x0890AF48),
// the source's scale (+0x1C) and handle set (0x0890AEBC / 0x0890AEB4), and the receiver (when given) gets the result through its virtuals
// +8 (the frame's point after 0x0890AF20) and +0xC (the helper's point).
void func_08916D48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], source = ctx.gpr[5], flag = ctx.gpr[6], provider = ctx.gpr[7], receiver = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {31u, 0x28u}, {21u, 0x24u}, {17u, 0x14u}});
    auto &F = ctx.fpr;
    ctx.gpr[20] = receiver;
    ctx.gpr[19] = source;
    ctx.gpr[18] = provider;
    ctx.gpr[16] = self;
    if (flag == 0u || provider == 0u) return;
    const u32 helper = self + 0x4u;
    ctx.gpr[17] = helper;
    const u32 handle = word(rt, source + 0xCu);
    if (handle != 0u) {
        const u32 method = word(rt, word(rt, handle) + 0x8u);
        ctx.gpr[3] = word(rt, handle);
        ctx.gpr[2] = method;
        const u32 key = call_set(rt, ctx, method, {handle});
        ctx.gpr[21] = key;
        if (load8(self + 0x14u) == 0u || key != word(rt, self + 0x10u)) {
            call_set(rt, ctx, 0x0890AF70u, {helper, word(rt, source + 0xCu)});
            store8(self + 0x14u, 1u);
            put(rt, self + 0x10u, key);
            call_set(rt, ctx, 0x0890AEC4u, {helper});
        }
    }
    const u32 count_method = word(rt, word(rt, provider) + 0x6Cu);
    ctx.gpr[3] = word(rt, provider);
    ctx.gpr[2] = count_method;
    const u32 count = call_set(rt, ctx, count_method, {provider, frame.at(0u)});
    ctx.gpr[16] = count;
    const u32 point = call_set(rt, ctx, 0x0890AF40u, {helper});
    if (count != 0u && point != 0u) {
        ctx.gpr[16] = count + 1u;
        const float inverse = 1.0f / static_cast<float>(static_cast<s32>(count + 1u));
        float sum = 0.0f, mean = 0.0f;
        for (u32 i = 0u; i < 3u; ++i) {
            sum = float_at(rt, frame.at(4u * i)) + float_at(rt, point + 4u * i);
            mean = psp_mul(inverse, sum);
            put_float(rt, frame.at(4u * i), mean);
        }
        F[3] = float_at(rt, point + 0x0u);  // the FPU registers as the asm leaves them
        F[2] = float_at(rt, point + 0x8u);
        F[0] = sum;
        F[1] = mean;
        call_set(rt, ctx, 0x0890AF48u, {helper, frame.at(0u)});
    } else {
        if (count != 0u) ctx.gpr[16] = count + 1u;
        call_set(rt, ctx, 0x0890AF48u, {helper, 0u});
    }
    F[12] = float_at(rt, source + 0x1Cu);
    call_set(rt, ctx, 0x0890AEBCu, {helper});
    call_set(rt, ctx, 0x0890AEB4u, {helper, word(rt, source + 0xCu)});
    if (receiver == 0u) return;
    call_set(rt, ctx, 0x0890AF20u, {helper, frame.at(0u)});
    const u32 first = word(rt, word(rt, receiver) + 0x8u);
    ctx.gpr[3] = word(rt, receiver);
    ctx.gpr[2] = first;
    call_set(rt, ctx, first, {receiver, frame.at(0u)});
    const u32 second = word(rt, word(rt, receiver) + 0xCu);
    ctx.gpr[3] = word(rt, receiver);
    ctx.gpr[16] = second;
    const u32 helper_point = call_set(rt, ctx, 0x0890AF40u, {helper});
    call_set(rt, ctx, second, {receiver, helper_point});
}
void func_089B80AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x34u), G[31]); G[20] = frame.at(0x8u); G[17] = G[4]; G[4] = G[20]; G[19] = G[5]; G[18] = G[6] & 0xFFu; call_here(rt, ctx, 0x08A36C3Cu); G[16] = word(rt, frame.at(0x8u)); { const bool tk = G[16] != 0u; G[5] = 0x08CA0000u; if (tk) goto L_089B80FC; } G[4] = 0x08CA0000u; G[4] = 0x08CA5910u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = 0x08CA0000u; L_089B80FC: ; G[5] = 0x08CA5918u; G[4] = G[16]; call_here(rt, ctx, 0x08A34DA8u); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[17]; call_here(rt, ctx, 0x089BD4DCu); { const bool tk = G[18] != 0u; G[16] = G[2]; if (tk) goto L_089B8184; } G[4] = G[19]; call_here(rt, ctx, 0x089AA910u); { const bool tk = G[2] == 0u; G[4] = G[17]; if (tk) goto L_089B8168; } G[2] = word(rt, G[16] + 0x138u); G[2] = G[2] & 0x80u; { const bool tk = G[2] == 0u; if (tk) goto L_089B8168; } G[4] = G[16]; call_here(rt, ctx, 0x089A3130u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x089B5CD4u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[5] = G[2]; if (tk) goto L_089B8164; } G[4] = G[17] + 0x138u; call_here(rt, ctx, 0x08B207A4u); L_089B8164: ; G[4] = G[17]; L_089B8168: ; G[5] = G[19]; call_here(rt, ctx, 0x089B7A08u); G[4] = G[17]; G[5] = G[19]; call_here(rt, ctx, 0x089B6E88u); G[4] = G[16]; goto L_089B8194; L_089B8184: ; G[5] = word(rt, G[17] + 0x15Cu); G[4] = G[17] + 0x138u; call_here(rt, ctx, 0x08B207A4u); G[4] = G[16]; L_089B8194: ; G[5] = G[17]; call_here(rt, ctx, 0x089A301Cu); G[4] = G[19]; G[5] = 0u; call_here(rt, ctx, 0x089AB114u); G[2] = word(rt, G[17] + 0x130u); store8(G[17] + 0x158u, 0u); { const bool tk = G[2] == 0u; store8(G[17] + 0x159u, 0u); if (tk) goto L_089B8240; } G[20] = frame.at(0xCu); G[4] = G[20]; G[18] = frame.at(0x10u); call_here(rt, ctx, 0x08A76AA0u); G[16] = frame.at(0x4u); G[4] = G[18]; call_here(rt, ctx, 0x08A76AA0u); G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0x0u), G[19]); call_here(rt, ctx, 0x08A76AACu); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_089B81F4; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_089B81F4: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[17] + 0x130u); if (G[16] != 0u) { G[2] = word(rt, G[16] + 0x0u); goto L_089B8218; } G[4] = 0x08CA0000u; G[4] = 0x08CA5908u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x0u); L_089B8218: ; G[4] = G[16]; G[5] = G[20]; G[3] = word(rt, G[2] + 0xCCu); G[6] = 0x1u; { const std::uint32_t tg = G[3]; G[7] = G[18]; call_here(rt, ctx, tg); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); L_089B8240: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_089DC10C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x40u), G[31]); G[19] = frame.at(0xCu); G[18] = G[4]; G[4] = G[19]; G[16] = G[6]; G[17] = G[7]; call_here(rt, ctx, 0x089DB3B4u); { const bool tk = G[16] != 0u; G[2] = 0x1u; if (tk) goto L_089DC1F4; } G[17] = frame.at(0x1Cu); G[5] = 0x08CE0000u; G[5] = 0x08CDEAB8u; G[4] = G[17]; call_here(rt, ctx, 0x08B20920u); G[16] = word(rt, frame.at(0xCu)); if (G[16] != 0u) { G[5] = word(rt, G[16] + 0xE8u); goto L_089DC174; } G[4] = 0x08CA0000u; G[4] = 0x08CA752Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[16] + 0xE8u); L_089DC174: ; G[4] = G[17]; call_here(rt, ctx, 0x08B20968u); { const bool tk = G[2] == 0u; G[16] = frame.at(0x4u); if (tk) goto L_089DC29C; } G[5] = G[2]; G[4] = G[16]; G[17] = frame.at(0x8u); G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08A6D2D4u); { const bool tk = G[18] == G[17]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_089DC1B4; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_089DC1B4: ; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[4] = G[4] + 0xFFFFFFFCu; if (tk) goto L_089DC29C; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_089DC29C; } G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); goto L_089DC29C; L_089DC1F4: ; { const bool tk = G[16] != G[2]; if (tk) goto L_089DC29C; } G[16] = word(rt, G[17] + 0x0u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_089DC218; } G[4] = 0x08CA0000u; G[4] = 0x08CA74F8u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_089DC218: ; G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = frame.at(0x10u); call_here(rt, ctx, tg); } G[17] = word(rt, frame.at(0xCu)); { const bool tk = G[17] != 0u; G[16] = frame.at(0x14u); if (tk) goto L_089DC244; } G[4] = 0x08CA0000u; G[4] = 0x08CA752Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = frame.at(0x14u); L_089DC244: ; G[5] = 0x08CE0000u; G[4] = G[16]; G[5] = 0x08CDEAB8u; call_here(rt, ctx, 0x08B20920u); G[2] = 0x08C90000u; G[6] = word(rt, 0x08C8CC04u); G[5] = word(rt, frame.at(0x10u)); G[4] = G[16]; call_here(rt, ctx, 0x08B20A54u); put(rt, G[17] + 0xE8u, G[2]); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[4] = word(rt, frame.at(0x10u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[4] = G[4] + 0xFFFFFFFCu; if (tk) goto L_089DC29C; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_089DC29C; } G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_089DC29C: ; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x34u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A50CF4 (a0 = a sprite, f12..f19 = two corners' coordinates): draws a quad through the global renderer 0x08CF68E8 (0x08B59838 begins,
// 0x08B53D2C draws). With the renderer's texture (+0x6C): a texture whose +4 or +0xC is not ~0 draws 4 vertices of the full corner sets,
// otherwise a 2-vertex sprite. Without one: a 2-vertex sprite of the template 0x08CDF758 scaled by the sprite's size (+0x7C, +0x80) and the
// renderer's scale (+0x48, +0x4C).
void func_08A50CF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 sprite = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x74u}, {16u, 0x70u}});
    auto &F = ctx.fpr;
    const u32 renderer = word(rt, 0x08CF68E8u);
    ctx.gpr[2] = renderer;
    const u32 texture = word(rt, renderer + 0x6Cu);
    ctx.gpr[16] = texture;
    auto store = [&](u32 at, unsigned reg) { put_float(rt, frame.at(at), F[reg]); };
    if (texture != 0u) {
        F[1] = std::bit_cast<float>(0x34000000u);
        F[0] = std::fabs(float_at(rt, texture + 0x4u));
        ctx.set_fpu_condition(F[1] <= F[0]);
        bool wide = ctx.fpu_condition();
        if (!wide) {
            F[0] = std::fabs(float_at(rt, texture + 0xCu));
            ctx.set_fpu_condition(F[1] <= F[0]);
            wide = ctx.fpu_condition();
        }
        if (!wide) {
            for (unsigned i = 0u; i < 4u; ++i) store(4u * i, 12u + i);
            for (unsigned i = 0u; i < 4u; ++i) store(0x20u + 4u * i, 16u + i);
            call_set(rt, ctx, 0x08B59838u, {});
            call_set(rt, ctx, 0x08B53D2Cu, {6u, texture, 2u, frame.at(0u), frame.at(0x20u)});
            return;
        }
        store(0x2Cu, 17u);
        store(0xCu, 13u); store(0x30u, 16u); store(0x10u, 12u); store(0x38u, 18u); store(0x3Cu, 19u); store(0x18u, 14u); store(0x1Cu, 15u);
        store(0x20u, 16u); store(0x24u, 17u); store(0x0u, 12u); store(0x4u, 13u); store(0x28u, 18u); store(0x8u, 14u); store(0x34u, 19u);
        store(0x14u, 15u);
        call_set(rt, ctx, 0x08B59838u, {});
        call_set(rt, ctx, 0x08B53D2Cu, {4u, texture, 4u, frame.at(0u), frame.at(0x20u)});
        return;
    }
    F[1] = float_at(rt, renderer + 0x4Cu);
    F[0] = float_at(rt, renderer + 0x48u);
    F[3] = float_at(rt, sprite + 0x80u);
    F[2] = float_at(rt, sprite + 0x7Cu);
    F[1] = psp_mul(F[1], F[3]);
    F[0] = psp_mul(F[0], F[2]);
    const u32 vertices = frame.at(0x40u), source = 0x08CDF758u;
    ctx.gpr[16] = vertices;
    for (unsigned i = 0u; i < 4u; ++i) store(0x20u + 4u * i, 12u + i);
    for (unsigned i = 0u; i < 4u; ++i) store(4u * i, 16u + i);
    for (u32 at = 0u; at <= 0x20u; at += 4u) put(rt, vertices + at, word(rt, source + at));
    ctx.gpr[2] = word(rt, source + 0x20u);
    ctx.gpr[3] = word(rt, source + 0x14u);
    ctx.gpr[4] = word(rt, source + 0x18u);
    ctx.gpr[5] = word(rt, source + 0x1Cu);
    put_float(rt, frame.at(0x40u), F[2]);
    put_float(rt, frame.at(0x50u), F[3]);
    put_float(rt, frame.at(0x58u), F[0]);
    put_float(rt, frame.at(0x5Cu), F[1]);
    call_set(rt, ctx, 0x08B59838u, {});
    call_set(rt, ctx, 0x08B53D2Cu, {6u, vertices, 2u, frame.at(0x20u), frame.at(0u)});
}
void func_08B4D24C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { auto &F = ctx.fpr; F[3] = float_at(rt, G[6] + 0x0u); F[4] = float_at(rt, G[6] + 0x4u); F[1] = float_at(rt, G[5] + 0x0u); F[2] = float_at(rt, G[5] + 0xCu); F[0] = float_at(rt, G[5] + 0x18u); F[1] = psp_mul(F[1], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x8u); F[1] = F[1] + F[2]; F[0] = psp_mul(F[0], F[3]); F[1] = F[1] + F[0]; put_float(rt, G[4] + 0x0u, F[1]); F[3] = float_at(rt, G[6] + 0xCu); F[4] = float_at(rt, G[6] + 0x10u); F[0] = float_at(rt, G[5] + 0x0u); F[2] = float_at(rt, G[5] + 0xCu); F[1] = float_at(rt, G[5] + 0x18u); F[0] = psp_mul(F[0], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x14u); F[0] = F[0] + F[2]; F[1] = psp_mul(F[1], F[3]); F[0] = F[0] + F[1]; put_float(rt, G[4] + 0xCu, F[0]); F[3] = float_at(rt, G[6] + 0x18u); F[4] = float_at(rt, G[6] + 0x1Cu); F[1] = float_at(rt, G[5] + 0x0u); F[2] = float_at(rt, G[5] + 0xCu); F[0] = float_at(rt, G[5] + 0x18u); F[1] = psp_mul(F[1], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x20u); F[1] = F[1] + F[2]; F[0] = psp_mul(F[0], F[3]); F[1] = F[1] + F[0]; put_float(rt, G[4] + 0x18u, F[1]); F[3] = float_at(rt, G[6] + 0x0u); F[4] = float_at(rt, G[6] + 0x4u); F[0] = float_at(rt, G[5] + 0x4u); F[2] = float_at(rt, G[5] + 0x10u); F[1] = float_at(rt, G[5] + 0x1Cu); F[0] = psp_mul(F[0], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x8u); F[0] = F[0] + F[2]; F[1] = psp_mul(F[1], F[3]); F[0] = F[0] + F[1]; put_float(rt, G[4] + 0x4u, F[0]); F[3] = float_at(rt, G[6] + 0xCu); F[4] = float_at(rt, G[6] + 0x10u); F[1] = float_at(rt, G[5] + 0x4u); F[2] = float_at(rt, G[5] + 0x10u); F[0] = float_at(rt, G[5] + 0x1Cu); F[1] = psp_mul(F[1], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x14u); F[1] = F[1] + F[2]; F[0] = psp_mul(F[0], F[3]); F[1] = F[1] + F[0]; put_float(rt, G[4] + 0x10u, F[1]); F[3] = float_at(rt, G[6] + 0x18u); F[4] = float_at(rt, G[6] + 0x1Cu); F[0] = float_at(rt, G[5] + 0x4u); F[2] = float_at(rt, G[5] + 0x10u); F[1] = float_at(rt, G[5] + 0x1Cu); F[0] = psp_mul(F[0], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x20u); F[0] = F[0] + F[2]; F[1] = psp_mul(F[1], F[3]); F[0] = F[0] + F[1]; put_float(rt, G[4] + 0x1Cu, F[0]); F[3] = float_at(rt, G[6] + 0x0u); F[4] = float_at(rt, G[6] + 0x4u); F[1] = float_at(rt, G[5] + 0x8u); F[2] = float_at(rt, G[5] + 0x14u); F[0] = float_at(rt, G[5] + 0x20u); F[1] = psp_mul(F[1], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x8u); F[1] = F[1] + F[2]; F[0] = psp_mul(F[0], F[3]); F[1] = F[1] + F[0]; put_float(rt, G[4] + 0x8u, F[1]); F[3] = float_at(rt, G[6] + 0xCu); F[4] = float_at(rt, G[6] + 0x10u); F[0] = float_at(rt, G[5] + 0x8u); F[2] = float_at(rt, G[5] + 0x14u); F[1] = float_at(rt, G[5] + 0x20u); F[0] = psp_mul(F[0], F[3]); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x14u); F[0] = F[0] + F[2]; F[1] = psp_mul(F[1], F[3]); F[0] = F[0] + F[1]; put_float(rt, G[4] + 0x14u, F[0]); F[3] = float_at(rt, G[6] + 0x18u); F[1] = float_at(rt, G[5] + 0x8u); F[2] = float_at(rt, G[5] + 0x14u); F[4] = float_at(rt, G[6] + 0x1Cu); F[1] = psp_mul(F[1], F[3]); F[0] = float_at(rt, G[5] + 0x20u); F[2] = psp_mul(F[2], F[4]); F[3] = float_at(rt, G[6] + 0x20u); F[1] = F[1] + F[2]; F[0] = psp_mul(F[0], F[3]); F[1] = F[1] + F[0]; put_float(rt, G[4] + 0x20u, F[1]); goto exit_;  exit_: ; } if (tail != 0u) jump(ctx, tail); } }
void func_08C2F130(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[21] = G[5]; G[19] = G[4]; { const bool tk = G[5] == G[4]; if (tk) goto L_08C2F2B8; } G[2] = word(rt, G[4] + 0x8u); G[18] = word(rt, G[5] + 0x4u); G[16] = word(rt, G[5] + 0x0u); G[4] = word(rt, G[4] + 0x0u); G[3] = G[18] - G[16]; G[2] = G[2] - G[4]; G[20] = sra(G[3], 2); G[2] = sra(G[2], 2); G[2] = G[2] < G[20] ? 1u : 0u; if (G[2] == 0u) { G[6] = word(rt, G[19] + 0x4u); goto L_08C2F1F8; } G[20] = G[20] << 2; G[4] = G[20]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[6] = G[2]; G[4] = G[16]; G[5] = G[18]; G[7] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C2DC4Cu); G[4] = word(rt, G[19] + 0x0u); G[5] = word(rt, G[19] + 0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B856B0u); G[4] = word(rt, G[19] + 0x0u); G[2] = word(rt, G[19] + 0x8u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2); if (tk) goto L_08C2F1E4; } G[5] = G[2] << 2; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C2F1E4: ; G[2] = G[17] + G[20]; G[3] = G[20]; put(rt, G[19] + 0x8u, G[2]); put(rt, G[19] + 0x0u, G[17]); goto L_08C2F2AC; L_08C2F1F8: ; G[2] = G[6] - G[4]; G[2] = sra(G[2], 2); G[3] = G[2] < G[20] ? 1u : 0u; { const bool tk = G[3] != 0u; G[22] = G[2]; if (tk) goto L_08C2F254; } { const bool tk = static_cast<std::int32_t>(G[20]) <= 0; G[17] = G[4]; if (tk) goto L_08C2F23C; } G[18] = 0u; L_08C2F218: ; G[18] = G[18] + 0x1u; { const bool tk = G[16] == G[17]; G[4] = G[17]; if (tk) goto L_08C2F22C; } G[5] = word(rt, G[16] + 0x0u); call_here(rt, ctx, 0x08A5BAD8u); L_08C2F22C: ; G[16] = G[16] + 0x4u; { const bool tk = G[20] != G[18]; G[17] = G[17] + 0x4u; if (tk) goto L_08C2F218; } G[6] = word(rt, G[19] + 0x4u); L_08C2F23C: ; G[5] = G[6]; G[4] = G[17]; G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B85754u); G[3] = G[20] << 2; goto L_08C2F2AC; L_08C2F254: ; { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[17] = G[4]; if (tk) goto L_08C2F290; } G[18] = 0u; L_08C2F260: ; G[18] = G[18] + 0x1u; { const bool tk = G[16] == G[17]; G[4] = G[17]; if (tk) goto L_08C2F274; } G[5] = word(rt, G[16] + 0x0u); call_here(rt, ctx, 0x08A5BAD8u); L_08C2F274: ; G[16] = G[16] + 0x4u; { const bool tk = G[22] != G[18]; G[17] = G[17] + 0x4u; if (tk) goto L_08C2F260; } G[16] = word(rt, G[21] + 0x0u); G[18] = word(rt, G[21] + 0x4u); G[4] = word(rt, G[19] + 0x0u); G[6] = word(rt, G[19] + 0x4u); L_08C2F290: ; G[4] = G[6] - G[4]; G[4] = (G[4] & ~0x3u) | ((0u << 0) & 0x3u); G[4] = G[16] + G[4]; G[5] = G[18]; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C2DC4Cu); G[3] = G[20] << 2; L_08C2F2AC: ; G[2] = word(rt, G[19] + 0x0u); G[2] = G[2] + G[3]; put(rt, G[19] + 0x4u, G[2]); L_08C2F2B8: ; G[2] = G[19]; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x28u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08820404 (a0 = the result, a1 = the argument count, a2 = the arguments): with three arguments (a float key, a start and a count), the
// table row 0x0881FC44 finds for the key (its base at +0xC) gives the first index, start + base clamped to [0, rows - 1] of the global table
// (0x08CE2ACC, 0x08CE2AD0 rows of 16 bytes); an array of `count` values (0x08A98C1C) is filled from consecutive rows (clamped too, each
// made by 0x08A5DEE8 from the row's two words and assigned by 0x08A5BAD8) and assigned to the result. Missing arguments report 0x08C99294.
void func_08820404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[5], arguments = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {17u, 0x14u}, {31u, 0x34u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}});
    ctx.gpr[5] = static_cast<s32>(count) < 3 ? 1u : 0u;
    ctx.gpr[30] = result;
    ctx.gpr[17] = arguments;
    if (static_cast<s32>(count) < 3) return;
    auto call_virtual = [&](u32 argument, u32 offset) {
        ctx.gpr[16] = argument;
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99294u});
        const u32 method = word(rt, word(rt, argument) + offset);
        ctx.gpr[3] = word(rt, argument);
        ctx.gpr[2] = method;
        return call_set(rt, ctx, method, {argument});
    };
    call_virtual(word(rt, arguments + 0x0u), 0x54u);
    ctx.fpr[12] = ctx.fpr[0];
    const u32 row = call_set(rt, ctx, 0x0881FC44u, {});
    ctx.gpr[18] = row;
    if (row == 0u) return;
    const u32 start = call_virtual(word(rt, arguments + 0x4u), 0x3Cu);
    ctx.gpr[20] = start;
    const u32 array = frame.at(0x4u);
    ctx.gpr[22] = array;
    ctx.gpr[23] = 0x08CE0000u;
    const u32 wanted = call_virtual(word(rt, arguments + 0x8u), 0x3Cu);
    ctx.gpr[19] = wanted;
    call_set(rt, ctx, 0x08A98C1Cu, {array, wanted});
    const u32 rows = word(rt, 0x08CE2AD0u);
    s32 first = static_cast<s32>(start + word(rt, row + 0xCu));
    if (first < 0) first = 0;
    if (!(first < static_cast<s32>(rows))) first = static_cast<s32>(rows - 1u);
    ctx.gpr[18] = static_cast<u32>(first);
    if (static_cast<s32>(wanted) > 0) {
        const u32 made = frame.at(0u);
        ctx.gpr[21] = made;
        ctx.gpr[20] = 0x08CE0000u;
        for (u32 i = 0u; i != wanted;) {
            const u32 total = word(rt, 0x08CE2AD0u);
            u32 index = i + static_cast<u32>(first);
            if (!(static_cast<s32>(index) < static_cast<s32>(total))) index = total - 1u;
            const u32 table = word(rt, 0x08CE2ACCu), entry = table + (index << 4);
            const u32 slot = word(rt, word(rt, frame.at(0x4u)) + 0x14u) + (i << 2);
            ctx.gpr[16] = slot;
            call_set(rt, ctx, 0x08A5DEE8u, {made, table, word(rt, entry + 0x0u), word(rt, entry + 0x4u)});
            ++i;
            ctx.gpr[17] = i;
            ctx.gpr[4] = slot;
            if (slot != made) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, made)});
            call_set(rt, ctx, 0x08A5B9ECu, {made});
            ctx.gpr[3] = word(rt, 0x08CE2AD0u);
        }
    }
    if (result != array) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, array)});
    call_set(rt, ctx, 0x08A5B9ECu, {array});
}
// 0x0883E228 (a0 = the result, a1 = the argument count, a2 = the arguments): with an argument, its string (virtual +0xC) is searched for the
// literal 0x08C9C4A8 (0x08B85EB8 makes it, 0x08B35F60 measures it, 0x08B8D750 finds it); not found, the string itself becomes the result,
// found, the literal joined with it (0x08B89124). The value (0x08A6D2D4) is assigned to the result (0x08A5BAD8) and every string released
// (shared count 0x08B60804, freed by 0x08B8542C unless it is the empty string 0x08D74FD0).
void func_0883E228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[5], arguments = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x28u}, {16u, 0x20u}, {31u, 0x30u}, {19u, 0x2Cu}, {17u, 0x24u}});
    ctx.gpr[18] = result;
    ctx.gpr[16] = arguments;
    if (static_cast<s32>(count) <= 0) return;
    const u32 literal = frame.at(0x10u), text = frame.at(0x14u), empty = 0x08D74FD0u;
    ctx.gpr[19] = literal;
    call_set(rt, ctx, 0x08B85EB8u, {literal, 0x08C9C4A8u, frame.at(0x3u)});
    const u32 argument = word(rt, arguments);
    ctx.gpr[16] = argument;
    if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
    ctx.gpr[17] = text;
    const u32 method = word(rt, word(rt, argument) + 0xCu);
    ctx.gpr[2] = word(rt, argument);
    ctx.gpr[3] = method;
    call_set(rt, ctx, method, {text, argument});
    const u32 chars = word(rt, frame.at(0x10u));
    ctx.gpr[16] = chars;
    const u32 length = call_set(rt, ctx, 0x08B35F60u, {chars});
    const u32 found = call_set(rt, ctx, 0x08B8D750u, {text, chars, 0u, length});
    auto release = [&](u32 slot, u32 tag) {
        const u32 data = word(rt, slot), header = data - 0xCu;
        ctx.gpr[4] = data;
        ctx.gpr[16] = header;
        if (header == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, tag});
    };
    auto assign = [&](u32 from, u32 value) {
        call_set(rt, ctx, 0x08A6D2D4u, {value, from});
        if (result != value) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, value)});
        call_set(rt, ctx, 0x08A5B9ECu, {value});
    };
    if (found == 0u) {
        ctx.gpr[16] = frame.at(0x4u);
        assign(text, frame.at(0x4u));
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[17] = empty;
    } else {
        const u32 joined = frame.at(0x8u), value = frame.at(0xCu);
        ctx.gpr[16] = joined;
        ctx.gpr[17] = value;
        call_set(rt, ctx, 0x08B89124u, {joined, literal, text});
        assign(joined, value);
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[17] = empty;
        release(joined, frame.at(0x2u));
    }
    release(text, frame.at(0x1u));
    release(literal, frame.at(0u));
}
// 0x0887347C (a0 = the object): a constructor. The base (0x0887E1E0) and the member at +0xC (0x08AF464C) are built, vtables 0x08CBFA00 / 0x08CBFA2C
// set, a 0x170-byte state made (0x08B62344; its +8 member by 0x0890B2F4, scale 3.0, two 1.0 transforms, empty strings 0x08D752A4) and kept
// at +0x5C, the shared static 0x08D753AC (vtable 0x08CBFA60, guard 0x08D75398) linked at +0x60, the member's three parameters set (1.0,
// 200.0, 1.76: 0x08AF4468 / 0x08AF4470 / 0x08AF4460) and 0x08872548(state, member) ends it as a tail call.
void func_0887347C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {31u, 0x18u}, {21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}});
        auto &F = ctx.fpr;
        const u32 member = self + 0xCu;
        ctx.gpr[18] = member;
        ctx.gpr[17] = self;
        ctx.gpr[19] = self + 0x5Cu;
        call_set(rt, ctx, 0x0887E1E0u, {self});
        call_set(rt, ctx, 0x08AF464Cu, {member});
        put(rt, self + 0x0u, 0x08CBFA00u);
        put(rt, self + 0xCu, 0x08CBFA2Cu);
        const u32 state = call_set(rt, ctx, 0x08B62344u, {0x170u});
        put(rt, state + 0x0u, 0u);
        ctx.gpr[16] = state;
        put(rt, state + 0x4u, 0u);
        call_set(rt, ctx, 0x0890B2F4u, {state + 0x8u});
        const u32 t = state + 0x24u, empty = 0x08D752A4u;
        put_float(rt, state + 0x14u, 3.0f);
        put(rt, state + 0x10u, 0u);
        put(rt, t + 0x0u, 0u);
        put_float(rt, t + 0x8u, 1.0f);
        put(rt, t + 0xCu, 0u);
        const u32 one = word(rt, t + 0x8u), zero = word(rt, t + 0xCu);
        put(rt, t + 0x20u, word(rt, t + 0x20u) & ~0x7u);
        put(rt, t + 0x10u, one);
        put(rt, t + 0x14u, zero);
        put_float(rt, t + 0x4u, 1.0f);
        put(rt, t + 0x18u, one);
        put(rt, t + 0x1Cu, zero);
        put(rt, state + 0x10Cu, empty);
        put(rt, state + 0x100u, word(rt, state + 0x100u) & ~0xFu);
        put(rt, state + 0x104u, empty);
        put(rt, state + 0x108u, empty);
        store8(state + 0x74u, 0u);
        put(rt, state + 0x18u, 0u);
        put(rt, state + 0x20u, 0u);
        put(rt, state + 0x1Cu, 0u);
        put(rt, t + 0x20u, word(rt, t + 0x20u) | 0x4u);
        put(rt, self + 0x5Cu, state);
        F[0] = 1.0f;
        ctx.gpr[3] = t;
        ctx.gpr[5] = empty;
        ctx.gpr[6] = one;
        ctx.gpr[7] = zero;
        ctx.gpr[8] = state + 0x18u;
        ctx.gpr[9] = 0x08D70000u;
        const u32 guard = 0x08D75398u, shared = 0x08D753ACu;
        ctx.gpr[20] = guard;
        ctx.gpr[21] = shared;
        ctx.gpr[4] = guard;
        if (signed_byte(rt, guard) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            ctx.gpr[5] = 0x08D70000u;
            put(rt, shared, 0x08CBFA60u);
            call_set(rt, ctx, 0x08B64494u, {guard});
        }
        put(rt, self + 0x60u, shared);
        F[12] = 1.0f;
        call_set(rt, ctx, 0x08AF4468u, {member});
        F[12] = 200.0f;
        call_set(rt, ctx, 0x08AF4470u, {member});
        F[12] = std::bit_cast<float>(0x3FE1E1E2u);
        call_set(rt, ctx, 0x08AF4460u, {member});
        ctx.gpr[4] = word(rt, self + 0x5Cu);
        ctx.gpr[5] = member;
    }
    jump(ctx, 0x08872548u);
}
// 0x08879B00 (a0 = the owner): reads a record from the owner's stream (reader on the frame: an id through 0x08B77BFC, two doubles turned to
// floats by 0x08B77BF4 + 0x08B71EC8) and, when the reader holds data, hands (id, x, y) to the owner's handler (0x0886B080, its virtual +8).
// x and y are the two values x 1024 as unsigned words, shifted 10 more (fixed point) and capped at 0x800000; mode 1 gives zeros, modes below
// 3 a zero y. The reader's text is released after.
void func_08879B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x20u}, {18u, 0x28u}, {31u, 0x30u}, {19u, 0x2Cu}, {17u, 0x24u}, {52u, 0x40u}});
    auto &F = ctx.fpr;
    const u32 reader = frame.at(0x4u);
    ctx.gpr[16] = reader;
    ctx.gpr[18] = owner;
    call_set(rt, ctx, 0x08AB85F0u, {reader});
    const u32 id = call_set(rt, ctx, 0x08B77BFCu, {call_set(rt, ctx, 0x08AB856Cu, {reader})});
    ctx.gpr[19] = id;
    auto next_float = [&] {
        const u32 lo = call_set(rt, ctx, 0x08B77BF4u, {call_set(rt, ctx, 0x08AB856Cu, {reader})});
        call_set(rt, ctx, 0x08B71EC8u, {lo, ctx.gpr[3]});
        return F[0];
    };
    F[20] = next_float();
    const float second = next_float();
    F[2] = second;
    const u32 mode = word(rt, frame.at(0xCu));
    ctx.gpr[5] = mode;
    if (static_cast<s32>(mode) > 0) {
        // float x 1024 to an unsigned word: values of 2^31 and more lose 2^31 first and get the top bit back.
        auto to_word = [&](float value, u32 &out) {
            F[0] = psp_mul(value, 1024.0f);
            F[1] = std::bit_cast<float>(0x4F000000u);
            out = trunc_to_word(F[0]);
            ctx.set_fpu_condition(F[1] <= F[0]);
            if (ctx.fpu_condition()) {
                F[0] = F[0] - F[1];
                out = trunc_to_word(F[0]) | 0x80000000u;
                F[0] = std::bit_cast<float>(trunc_to_word(F[0]));
            }
        };
        u32 x = 0u, y = 0u;
        to_word(F[20], x);
        to_word(second, y);
        u32 sx = x << 10, sy = y << 10;
        bool clamp = true;
        if (static_cast<s32>(mode) < 3) {
            sy = 0u;
            if (mode == 1u) {
                sx = 0u;
                clamp = false;
            }
        }
        if (clamp) {
            if (0x00800000u < sy) sy = 0x00800000u;
            if (0x00800000u < sx) sx = 0x00800000u;
        }
        ctx.gpr[17] = sx;
        ctx.gpr[16] = sy;
        const u32 handler = call_set(rt, ctx, 0x0886B080u, {word(rt, owner)});
        if (handler != 0u) {
            const u32 method = word(rt, word(rt, handler) + 0x8u);
            ctx.gpr[2] = word(rt, handler);
            ctx.gpr[3] = method;
            call_set(rt, ctx, method, {handler, id, sx, sy});
        }
    }
    const u32 data = word(rt, frame.at(0x4u)), header = data - 0xCu;
    ctx.gpr[2] = 0x08D75298u;
    ctx.gpr[16] = header;
    ctx.gpr[4] = data;
    if (header == 0x08D75298u) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B93554u, {header, frame.at(0u)});
}
// 0x088E1404 (a0 = the owner): when the owner has room (+0x65C..+0x660, 16-byte entries) for 0x088DAA8C's count, each of the 0x088D9F80
// entries (+0x77C, 0x38 bytes; paired state in +0xC0, 0x2F8 bytes) that is in use (+0x20 not -1) and not done (+0x2C) is checked under the
// lock +0xC8: it gets a time stamp (0x08B2E05C) once, and when 0x088E8348 accepts its state a task (0x088DCFD0, the entry) is queued on
// +0x718 (0x08B1A338) and the returned handle dropped (use and weak counts, virtuals +8 / +0xC); the entry is then marked done.
void func_088E1404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[18] = owner;
    const u32 room = sra(word(rt, owner + 0x660u) - word(rt, owner + 0x65Cu), 4);
    const u32 needed = call_set(rt, ctx, 0x088DAA8Cu, {owner});
    ctx.gpr[16] = static_cast<s32>(room) < static_cast<s32>(needed) ? 1u : 0u;
    ctx.gpr[20] = 0u;
    if (static_cast<s32>(room) < static_cast<s32>(needed)) return;
    const u32 lock = owner + 0xC8u;
    ctx.gpr[23] = lock;
    ctx.gpr[30] = 1u;
    auto drop = [&](u32 address) {
        const u32 old = word(rt, address);
        put(rt, address, old - 1u);
        ctx.gpr[4] = old;
        ctx.gpr[2] = 1u;
        return old == 1u;
    };
    u32 done_count = 0u, state_offset = 0u, entry_offset = 0u;
    for (;;) {
        ctx.gpr[22] = state_offset;
        ctx.gpr[21] = entry_offset;
        ctx.gpr[20] = done_count;
        const u32 total = call_set(rt, ctx, 0x088D9F80u, {owner});
        ctx.gpr[2] = static_cast<s32>(done_count) < static_cast<s32>(total) ? 1u : 0u;
        if (!(static_cast<s32>(done_count) < static_cast<s32>(total))) return;
        const u32 entry = word(rt, owner + 0x77Cu) + entry_offset, state = word(rt, owner + 0xC0u) + state_offset;
        ctx.gpr[17] = entry;
        ctx.gpr[19] = state;
        call_set(rt, ctx, 0x08B344E0u, {lock});
        const bool active = word(rt, entry + 0x20u) + 1u != 0u && load8(entry + 0x2Cu) == 0u;
        ctx.gpr[16] = active ? 1u : 0u;
        call_set(rt, ctx, 0x08B3457Cu, {lock});
        if (active) {
            if (word(rt, entry + 0x30u) == 0xFFFFFFFFu && word(rt, entry + 0x34u) == 0xFFFFFFFFu) {
                const u32 lo = call_set(rt, ctx, 0x08B2E05Cu, {});
                put(rt, entry + 0x30u, lo);
                put(rt, entry + 0x34u, ctx.gpr[3]);
            }
            if (call_set(rt, ctx, 0x088E8348u, {state}) == 0u) {
                call_set(rt, ctx, 0x08B2E05Cu, {});
            } else {
                put(rt, frame.at(0x8u), 0x088DCFD0u);
                put(rt, frame.at(0xCu), entry);
                call_set(rt, ctx, 0x08B1A338u, {frame.at(0u), owner + 0x718u, frame.at(0x8u)});
                const u32 held = word(rt, frame.at(0x4u));
                ctx.gpr[16] = held;
                ctx.gpr[3] = held + 0x4u;
                if (held != 0u && drop(held + 0x4u)) {
                    const u32 dispose = word(rt, word(rt, held) + 0x8u);
                    ctx.gpr[3] = word(rt, held);
                    ctx.gpr[2] = dispose;
                    call_set(rt, ctx, dispose, {held});
                    ctx.gpr[3] = held + 0x8u;
                    if (drop(held + 0x8u)) {
                        const u32 destroy = word(rt, word(rt, held) + 0xCu);
                        ctx.gpr[3] = word(rt, held);
                        ctx.gpr[2] = destroy;
                        call_set(rt, ctx, destroy, {held});
                    }
                }
                store8(entry + 0x2Cu, 1u);
            }
        }
        ++done_count;
        state_offset += 0x2F8u;
        entry_offset += 0x38u;
    }
}
// 0x0898ACF0 (a0 = the result, a2 = the argument count, a3 = the arguments): with no argument, the result is the value of the current object
// (0x089896CC into the frame, 0x08989414 its value, 0x08A6D2D4 / 0x08A5BAD8 assign it); with arguments, that object receives
// (0x0898A220) the first argument's string (its virtual +0xC), or the literal 0x08C8C0C4 when the first argument is null. Missing values
// report through 0x08A5BB7C; strings are released (0x08B60804 / 0x08B8542C, empty string 0x08D74FD0).
void func_0898ACF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[6], arguments = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x24u}, {16u, 0x20u}, {31u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}});
    ctx.gpr[17] = result;
    ctx.gpr[16] = arguments;
    const u32 current = frame.at(0x10u);
    auto release = [&](u32 slot, u32 tag) {
        const u32 data = word(rt, slot), header = data - 0xCu;
        ctx.gpr[2] = 0x08D74FD0u;
        ctx.gpr[16] = header;
        ctx.gpr[4] = data - 0x4u;
        if (header == 0x08D74FD0u) return;
        const u32 left = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        ctx.gpr[4] = header;
        if (static_cast<s32>(left) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, tag});
    };
    if (count == 0u) {
        ctx.gpr[18] = current;
        call_set(rt, ctx, 0x089896CCu, {current});
        const u32 object = word(rt, current);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08CA0000u;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
        const u32 value = call_set(rt, ctx, 0x08989414u, {object});
        const u32 made = frame.at(0xCu);
        ctx.gpr[16] = made;
        call_set(rt, ctx, 0x08A6D2D4u, {made, value});
        if (result != made) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, made)});
        call_set(rt, ctx, 0x08A5B9ECu, {made});
        call_set(rt, ctx, 0x08A5B9ECu, {current});
        return;
    }
    ctx.gpr[19] = current;
    if (static_cast<s32>(count) <= 0) return;
    call_set(rt, ctx, 0x089896CCu, {current});
    u32 argument = word(rt, arguments);
    ctx.gpr[17] = argument;
    const u32 object = word(rt, current);
    ctx.gpr[18] = object;
    if (argument != 0u) {
        if (object == 0u) {
            call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
            argument = word(rt, arguments);
            ctx.gpr[17] = argument;
        }
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39E8u});
        const u32 text = frame.at(0x8u);
        ctx.gpr[16] = text;
        const u32 method = word(rt, word(rt, argument) + 0xCu);
        ctx.gpr[3] = word(rt, argument);
        ctx.gpr[2] = method;
        call_set(rt, ctx, method, {text, argument});
        call_set(rt, ctx, 0x0898A220u, {object, text});
        release(text, frame.at(0x1u));
    } else {
        const u32 target = word(rt, current), text = frame.at(0x4u);
        ctx.gpr[17] = target;
        ctx.gpr[16] = text;
        if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
        call_set(rt, ctx, 0x08B85EB8u, {text, 0x08C8C0C4u, frame.at(0x2u)});
        call_set(rt, ctx, 0x0898A220u, {target, text});
        release(text, frame.at(0u));
    }
    call_set(rt, ctx, 0x08A5B9ECu, {current});
}
void func_08B1837C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x100u); put(rt, frame.at(0xE4u), G[17]); put(rt, frame.at(0xF0u), G[31]); put(rt, frame.at(0xECu), G[19]); put(rt, frame.at(0xE0u), G[16]); put(rt, frame.at(0xE8u), G[18]); G[17] = G[5]; G[16] = G[4]; call_here(rt, ctx, 0x08B0CE58u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C7A848u); G[6] = word(rt, frame.at(0x8u)); G[7] = word(rt, frame.at(0xCu)); { const bool tk = G[2] != 0u; G[19] = G[6]; if (tk) goto L_08B184C0; } G[2] = word(rt, frame.at(0x0u)); G[3] = 0x1000u; G[5] = 0x3u; G[2] = G[2] & 0xF000u; { const bool tk = G[2] == G[3]; G[4] = 0u; if (tk) goto L_08B184E4; } if (static_cast<std::int32_t>(G[7]) > 0) { G[4] = G[17]; goto L_08B183F4; } { const bool tk = G[7] != 0u; G[5] = 0x1u; if (tk) goto L_08B184E4; } G[2] = G[6] < 0x9u ? 1u : 0u; { const bool tk = G[2] != 0u; G[3] = 0u; if (tk) goto L_08B184E8; } G[4] = G[17]; L_08B183F4: ; G[5] = 0x1u; G[6] = 0u; call_here(rt, ctx, 0x08B33C0Cu); G[17] = frame.at(0x97u); G[18] = G[2]; { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[17] = (G[17] & ~0x3Fu) | ((0u << 0) & 0x3Fu); if (tk) goto L_08B184D4; } G[4] = G[2]; G[5] = G[17]; G[6] = 0x8u; call_here(rt, ctx, 0x08B33D3Cu); G[3] = 0x8u; if (G[2] != G[3]) { G[17] = 0u; goto L_08B184A0; } G[2] = load8(G[17] + 0x1u); G[4] = load8(G[17] + 0x0u); G[3] = load8(G[17] + 0x2u); G[5] = load8(G[17] + 0x3u); G[2] = G[2] << 8; G[4] = G[4] | G[2]; G[3] = G[3] << 16; G[4] = G[4] | G[3]; G[5] = G[5] << 24; G[2] = 0xFFF70000u; G[4] = G[4] | G[5]; G[2] = G[2] | 0xEEC5u; { const bool tk = G[4] == G[2]; G[2] = 0xFEFD0000u; if (tk) goto L_08B18470; } G[2] = G[2] | 0xDB45u; if (G[4] != G[2]) { G[17] = 0u; goto L_08B184A0; } L_08B18470: ; G[3] = load8(G[17] + 0x5u); G[2] = load8(G[17] + 0x4u); G[4] = load8(G[17] + 0x6u); G[5] = load8(G[17] + 0x7u); G[3] = G[3] << 8; G[2] = G[2] | G[3]; G[4] = G[4] << 16; G[2] = G[2] | G[4]; G[5] = G[5] << 24; G[2] = G[2] | G[5]; G[19] = 0u - G[2]; G[17] = 0x1u; L_08B184A0: ; G[4] = G[18]; call_here(rt, ctx, 0x08B33A24u); { const bool tk = G[17] == 0u; G[6] = word(rt, frame.at(0x8u)); if (tk) goto L_08B184D8; } G[7] = word(rt, frame.at(0xCu)); G[5] = 0x2u; G[4] = 0x1u; goto L_08B184E4; L_08B184C0: ; G[5] = 0u; { const bool tk = static_cast<std::int32_t>(G[2]) >= 0; G[4] = 0u; if (tk) goto L_08B184E4; } goto L_08B18508; L_08B184D4: ; G[6] = word(rt, frame.at(0x8u)); L_08B184D8: ; G[7] = word(rt, frame.at(0xCu)); G[5] = 0x1u; G[4] = 0u; L_08B184E4: ; G[3] = 0u; L_08B184E8: ; put(rt, G[16] + 0x4u, G[5]); put(rt, G[16] + 0x8u, G[4]); put(rt, G[16] + 0x10u, G[19]); put(rt, G[16] + 0x14u, G[3]); put(rt, G[16] + 0x18u, G[6]); put(rt, G[16] + 0x1Cu, G[7]); put(rt, G[16] + 0x0u, 0u); goto L_08B18514; L_08B18508: ; G[4] = G[2]; call_here(rt, ctx, 0x08B1830Cu); put(rt, G[16] + 0x0u, G[2]); L_08B18514: ; G[2] = G[16]; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0xE4u)); G[19] = word(rt, frame.at(0xECu)); G[16] = word(rt, frame.at(0xE0u)); G[18] = word(rt, frame.at(0xE8u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B3A1C4 (a0 = the result handle, a1 = a context, a2 = a node): after 0x08B39B80(result, context, node), a result without a value (+4)
// gets one: a node of kind 14 keeps it empty; otherwise a 0x24-byte value is built (0x08B38C78(kind 5) from two empty handles, vtable
// 0x08CDA908) and wrapped in a handle (vtable 0x08CDA8F8). With a value, the function recurses on a frame handle and combines both
// (0x08B395AC). The result's value is swapped in reference-counted (+4 use count, virtual +4 at zero) and the temporaries destroyed
// (0x08C50CCC). v0 = the result.
void func_08B3A1C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], context = ctx.gpr[5], node = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}, {31u, 0x44u}, {20u, 0x40u}, {19u, 0x3Cu}});
    ctx.gpr[18] = result;
    ctx.gpr[17] = context;
    ctx.gpr[16] = node;
    call_here(rt, ctx, 0x08B39B80u);
    // The result takes `value` (the handle's +4 in the frame): the old value drops a use (virtual +4 at zero), the new one gains one.
    auto take = [&](u32 handle) {
        if (result == handle) return;
        const u32 old = word(rt, result + 0x4u);
        u32 value = word(rt, handle + 0x4u);
        ctx.gpr[3] = value;
        if (old != 0u) {
            const u32 uses = word(rt, old + 0x4u) - 1u;
            ctx.gpr[2] = uses;
            put(rt, old + 0x4u, uses);
            if (uses == 0u) {
                const u32 method = word(rt, word(rt, old) + 0x4u);
                ctx.gpr[3] = word(rt, old);
                ctx.gpr[2] = method;
                call_set(rt, ctx, method, {old});
            }
            value = word(rt, handle + 0x4u);
            ctx.gpr[3] = value;
        }
        put(rt, result + 0x4u, value);
        if (value != 0u) {
            ctx.gpr[2] = word(rt, value + 0x4u) + 1u;
            put(rt, value + 0x4u, ctx.gpr[2]);
        }
    };
    ctx.gpr[6] = node;
    if (word(rt, result + 0x4u) != 0u) {
        const u32 inner = frame.at(0u), combined = frame.at(0x8u);
        ctx.gpr[16] = combined;
        call_set(rt, ctx, 0x08B3A1C4u, {inner, context});
        call_set(rt, ctx, 0x08B395ACu, {combined, context, result, inner});
        take(combined);
        call_set(rt, ctx, 0x08C50CCCu, {combined});
        call_set(rt, ctx, 0x08C50CCCu, {inner});
        ctx.gpr[2] = result;
        return;
    }
    if (word(rt, node + 0x0u) == 0xEu) {
        ctx.gpr[3] = 0xEu;
        ctx.gpr[2] = result;
        return;
    }
    const u32 empty = 0x08CDA908u, first = frame.at(0x18u), second = frame.at(0x10u), wrapper = frame.at(0x20u);
    ctx.gpr[17] = empty;
    ctx.gpr[20] = first;
    ctx.gpr[19] = second;
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x18u), empty);
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x10u), empty);
    const u32 value = call_set(rt, ctx, 0x08B62344u, {0x24u});
    ctx.gpr[16] = value;
    call_set(rt, ctx, 0x08B38C78u, {value, 5u, first, second});
    put(rt, frame.at(0x20u), 0x08CDA8F8u);
    put(rt, frame.at(0x24u), value);
    if (value != 0u) put(rt, value + 0x4u, word(rt, value + 0x4u) + 1u);
    ctx.gpr[16] = wrapper;
    put(rt, frame.at(0x20u), empty);
    take(wrapper);
    call_set(rt, ctx, 0x08C50CCCu, {wrapper});
    call_set(rt, ctx, 0x08C50CCCu, {second});
    call_set(rt, ctx, 0x08C50CCCu, {first});
    ctx.gpr[2] = result;
}
void func_0887B980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x20u), G[16]); G[20] = frame.at(0x8u); G[2] = G[5]; G[17] = G[4]; G[4] = G[20]; G[19] = G[17] + 0x4u; put(rt, G[17] + 0x2Cu, G[5]); G[5] = frame.at(0u); put(rt, G[17] + 0x30u, G[6]); put(rt, frame.at(0x0u), G[2]); put(rt, frame.at(0x4u), G[6]); call_here(rt, ctx, 0x08963418u); G[2] = word(rt, frame.at(0x8u)); G[18] = word(rt, frame.at(0xCu)); G[3] = word(rt, G[17] + 0x4u); { const bool tk = G[18] == G[3]; put(rt, G[17] + 0x0u, G[2]); if (tk) goto L_0887BA64; } { const bool tk = G[18] == 0u; G[3] = G[18] + 0x4u; if (tk) goto L_0887B9F8; } L_0887B9E4: ; G[2] = word(rt, G[3] + 0x0u); G[2] = G[2] + 0x1u; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_0887B9E4; } L_0887B9F8: ; G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] == 0u; G[3] = G[16] + 0x4u; if (tk) goto L_0887BA60; } L_0887BA04: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_0887BA04; } if (G[4] != G[2]) { put(rt, G[19] + 0x0u, G[18]); goto L_0887BA64; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_0887BA34: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_0887BA34; } if (G[4] != G[2]) { put(rt, G[19] + 0x0u, G[18]); goto L_0887BA64; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_0887BA60: ; put(rt, G[19] + 0x0u, G[18]); L_0887BA64: ; G[2] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x14u)); G[3] = word(rt, G[17] + 0xCu); put(rt, G[17] + 0x8u, G[2]); { const bool tk = G[18] == G[3]; G[19] = G[17] + 0xCu; if (tk) goto L_0887BB04; } { const bool tk = G[18] == 0u; G[3] = G[18] + 0x4u; if (tk) goto L_0887BA98; } L_0887BA84: ; G[2] = word(rt, G[3] + 0x0u); G[2] = G[2] + 0x1u; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; if (tk) goto L_0887BA84; } L_0887BA98: ; G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] == 0u; G[3] = G[16] + 0x4u; if (tk) goto L_0887BB00; } L_0887BAA4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_0887BAA4; } if (G[4] != G[2]) { put(rt, G[19] + 0x0u, G[18]); goto L_0887BB04; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_0887BAD4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_0887BAD4; } if (G[4] != G[2]) { put(rt, G[19] + 0x0u, G[18]); goto L_0887BB04; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_0887BB00: ; put(rt, G[19] + 0x0u, G[18]); L_0887BB04: ; G[4] = G[20]; call_here(rt, ctx, 0x08B8ED98u); G[4] = G[17]; G[5] = 0u; G[6] = 0u; call_here(rt, ctx, 0x0887B40Cu); goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08885994 (a0 = the object, a1 = an argument): with the double-buffered state (+0x68) active (bit 0 of +0x26) and its back slot (+0x14 +
// (1 - front) x 8) ready (bit 0 of its +0x18): a slot also flagged bit 1 only has its back handle (+0x18 / +0x1C pair) dropped (use and weak
// counts, virtuals +8 / +0xC at zero); otherwise the buffers swap (0x088858B0, 0x08884B98, front byte +0x25 flipped, 0x088853F0 with the new
// front's value). Both clear bit 16 of +0x24 and tail-call 0x0886BAF8(argument).
void func_08885994(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], argument = ctx.gpr[5];
    u32 tail = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {16u, 0x0u}, {31u, 0xCu}, {17u, 0x4u}});
        ctx.gpr[16] = self;
        ctx.gpr[18] = argument;
        u32 state = word(rt, self + 0x68u);
        ctx.gpr[4] = state;
        ctx.gpr[2] = load16(state + 0x26u) & 0x1u;
        if (ctx.gpr[2] == 0u) return;
        const u32 back = word(rt, state + ((1u - load8(state + 0x25u)) << 3) + 0x14u);
        ctx.gpr[2] = back;
        if (back == 0u) return;
        const u32 flags = word(rt, back + 0x18u);
        ctx.gpr[2] = flags & 0x1u;
        if ((flags & 0x1u) == 0u) return;
        // ll / sc: the decrement always succeeds here; true when the count was 1.
        auto drop = [&](u32 address) {
            const u32 old = word(rt, address);
            put(rt, address, old - 1u);
            ctx.gpr[4] = old;
            ctx.gpr[2] = 1u;
            return old == 1u;
        };
        if (((flags >> 1) & 0x1u) != 0u) {
            const u32 front = load8(state + 0x25u);
            ctx.gpr[3] = front;
            const u32 entry = state + (((1u - front) << 3) + 0x10u);
            const u32 held = word(rt, entry + 0x8u);
            ctx.gpr[17] = held;
            put(rt, entry + 0x4u, 0u);
            put(rt, entry + 0x8u, 0u);
            ctx.gpr[2] = entry;
            ctx.gpr[3] = held + 0x4u;
            if (held != 0u && drop(held + 0x4u)) {
                const u32 dispose = word(rt, word(rt, held) + 0x8u);
                ctx.gpr[3] = word(rt, held);
                ctx.gpr[2] = dispose;
                call_set(rt, ctx, dispose, {held});
                ctx.gpr[3] = held + 0x8u;
                if (drop(held + 0x8u)) {
                    const u32 destroy = word(rt, word(rt, held) + 0xCu);
                    ctx.gpr[3] = word(rt, held);
                    ctx.gpr[2] = destroy;
                    call_set(rt, ctx, destroy, {held});
                }
            }
        } else {
            call_set(rt, ctx, 0x088858B0u, {self});
            call_set(rt, ctx, 0x08884B98u, {self});
            state = word(rt, self + 0x68u);
            store8(state + 0x25u, 1u - load8(state + 0x25u));
            state = word(rt, self + 0x68u);
            const u32 front = word(rt, state + (load8(state + 0x25u) << 3) + 0x14u);
            ctx.gpr[2] = front;
            call_set(rt, ctx, 0x088853F0u, {self, front != 0u ? word(rt, front + 0x8u) : 0u});
        }
        state = word(rt, self + 0x68u);
        ctx.gpr[3] = state;
        ctx.gpr[2] = word(rt, state + 0x24u) & ~0x10000u;
        put(rt, state + 0x24u, ctx.gpr[2]);
        ctx.gpr[4] = argument;
        tail = 0x0886BAF8u;
    }
    jump(ctx, tail);
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_331
} // namespace gt::game

#define HOT331(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_331::name, gt::ReturnKind::kind)
// HOT331(0x08B94ED0u, func_08B94ED0, Word);  // no comparable synthetic case yet
HOT331(0x08BDDFDCu, func_08BDDFDC, Word);
HOT331(0x08BDEC3Cu, func_08BDEC3C, Word);
HOT331(0x08C4F79Cu, func_08C4F79C, Word);
// HOT331(0x08C752BCu, func_08C752BC, Word);  // no comparable synthetic case yet
// HOT331(0x0884D1E0u, func_0884D1E0, Word);  // no comparable synthetic case yet
HOT331(0x08879E14u, func_08879E14, Word);
HOT331(0x0887A0D8u, func_0887A0D8, Word);
HOT331(0x0889E534u, func_0889E534, Word);
HOT331(0x08916D48u, func_08916D48, Word);
// HOT331(0x089B80ACu, func_089B80AC, Word);  // no comparable synthetic case yet
// HOT331(0x089DC10Cu, func_089DC10C, Word);  // no comparable synthetic case yet
HOT331(0x08A50CF4u, func_08A50CF4, Word);
HOT331(0x08B4D24Cu, func_08B4D24C, Word);
// HOT331(0x08C2F130u, func_08C2F130, Word);  // no comparable synthetic case yet
HOT331(0x08820404u, func_08820404, Word);
HOT331(0x0883E228u, func_0883E228, Word);
HOT331(0x0887347Cu, func_0887347C, Word);
HOT331(0x08879B00u, func_08879B00, Word);
HOT331(0x088E1404u, func_088E1404, Word);
HOT331(0x0898ACF0u, func_0898ACF0, Word);
// HOT331(0x08B1837Cu, func_08B1837C, Word);  // no comparable synthetic case yet
HOT331(0x08B3A1C4u, func_08B3A1C4, Word);
// HOT331(0x0887B980u, func_0887B980, Word);  // no comparable synthetic case yet
HOT331(0x08885994u, func_08885994, Word);
