// Hot batch 267: fifty-six- to fifty-eight-instruction functions no route reaches, written by hand from `tools.disasm`
// and verified by synthetic cases (Euler-to-quaternion variants, guarded static construction, a float ratio, heap
// pushes, element copies with and without alignment, list relinks and handle-guarded calls through temporaries).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_267 {
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

// The callee-saved registers (s0-s7, f20-f31) a function keeps values in: the rewrite sets them as the asm does, because the callees of the
// original can read them (FINDINGS, batch 65: 0x0882D838), and puts the caller's values back when it returns.
class SavedRegisters {
public:
    explicit SavedRegisters(psprecomp::AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        for (unsigned i = 0; i < 12; ++i) fpr_[i] = ctx.fpr[20 + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        for (unsigned i = 0; i < 12; ++i) ctx_.fpr[20 + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    u32 gpr_[8];
    float fpr_[12];
};

u32 signed_byte(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
u32 signed_half(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}

// 0x08A5BB7C: the null-handle report every checked handle load calls with its message in a0.
void report_null(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 message) { call_set(rt, ctx, 0x08A5BB7Cu, {message}); }

// Drops one reference of the string whose characters start at `data` unless its representation (data - 0xC) is `empty`: 0x08B60804
// decrements the count (data - 4) and `release` frees the representation at zero or below (a1 = the stack word at sp). v0 is `data`
// until a call replaces it; a0 / a1 as the original leaves them.
void drop_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, u32 data, u32 empty, u32 release) {
    const u32 representation = data - 0xCu;
    ctx.gpr[2] = data;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (representation == empty) return;
    const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
    ctx.gpr[4] = representation;
    ctx.gpr[5] = frame.at(0u);
    if (remaining <= 0) call_set(rt, ctx, release, {representation, frame.at(0u)});
}

// 0x08B4CA94 / 0x08B4CD24: the quaternion-like product of the half-angle sines and cosines of three angles (f12, f13, f14, each halved and
// converted by 0x08B498AC into a (sin, cos) pair on the stack), stored at a0 + 4, +0xC, +0 and +8; the two functions differ in which
// of the four sums are differences.
struct HalfAngles {
    float s0, c0, s1, c1, s2, c2;
};

HalfAngles half_angles(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame) {
    const float half = 0.5f;
    const float third = ctx.fpr[14];
    const float second = ctx.fpr[13];
    ctx.fpr[20] = half;
    ctx.fpr[22] = second;
    ctx.fpr[21] = third;
    ctx.fpr[12] = ctx.fpr[12] * half;
    call_set(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)});
    ctx.fpr[12] = second * half;
    call_set(rt, ctx, 0x08B498ACu, {frame.at(8u), frame.at(0xCu)});
    ctx.fpr[12] = third * half;
    call_set(rt, ctx, 0x08B498ACu, {frame.at(0x10u), frame.at(0x14u)});
    return {float_at(rt, frame.at(0u)), float_at(rt, frame.at(4u)), float_at(rt, frame.at(8u)), float_at(rt, frame.at(0xCu)),
            float_at(rt, frame.at(0x10u)), float_at(rt, frame.at(0x14u))};
}

void compose_angles(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool subtract_first, bool subtract_third, bool subtract_second,
                    bool subtract_fourth) {
    const u32 out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(ctx);
    const HalfAngles h = half_angles(rt, ctx, frame);
    const float f6 = h.s0 * h.c1;
    const float f5 = h.c1 * h.c0;
    const float f4 = h.s0 * h.s1;
    const float f1 = h.c0 * h.s1;
    const float f7 = h.c2 * f5;
    const float f3 = h.c2 * f1;
    const float f8 = f6 * h.s2;
    const float f9 = h.s2 * f4;
    const float f6b = f6 * h.c2;
    const float f1b = f1 * h.s2;
    const float f0 = h.c2 * f4;
    const float f2 = h.s2 * f5;
    put_float(rt, out + 4u, subtract_first ? f3 - f8 : f3 + f8);
    put_float(rt, out + 0xCu, subtract_second ? f7 - f9 : f7 + f9);
    put_float(rt, out, subtract_third ? f6b - f1b : f6b + f1b);
    put_float(rt, out + 8u, subtract_fourth ? f0 - f2 : f0 + f2);
}

// 0x0880A330: constructor (a0 = the object): base constructor 0x08A02B98, vtables 0x08CB9C78 / 0x08CB9ED0, a 28-byte member from 0x08B62344
// (zeroed words, 100.0 at +0x10 and +0x14, 1.0 at +0x18) kept in +0x118 and the one-time static at 0x08D74FE0 (guard 0x08B6441C /
// 0x08B64494, record at 0x08D74FE8 with vtable 0x08CBBCD0) linked into +0x11C.
void construct_with_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    constexpr u32 kGuard = 0x08D74FE0u;
    constexpr u32 kRecord = 0x08D74FE8u;
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x08A02B98u, {self});
    put(rt, self, 0x08CB9C78u);
    ctx.gpr[2] = 0x08CB9ED0u;
    put(rt, self + 0x38u, 0x08CB9ED0u);
    const u32 member = call_set(rt, ctx, 0x08B62344u, {0x1Cu});
    ctx.fpr[1] = 100.0f;
    ctx.fpr[0] = 1.0f;
    put_float(rt, member + 0x14u, 100.0f);
    ctx.gpr[17] = kGuard;
    ctx.gpr[18] = self + 0x118u;
    ctx.gpr[19] = kRecord;
    put_float(rt, member + 0x18u, 1.0f);
    put(rt, member, 0u);
    put(rt, member + 4u, 0u);
    put(rt, member + 8u, 0u);
    put(rt, member + 0xCu, 0u);
    put_float(rt, member + 0x10u, 100.0f);
    put(rt, self + 0x118u, member);
    ctx.gpr[2] = signed_byte(rt, kGuard);
    if (ctx.gpr[2] == 0u) {
        if (call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            ctx.gpr[2] = 0x08CBBCD0u;
            put(rt, kRecord, 0x08CBBCD0u);
            call_set(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    put(rt, self + 0x11Cu, kRecord);
}

// 0x088730B0: sets bit 2 of the flags at *(a0 + 0x5C) + 0x100 from a1, clears the four words at +0x110, and stores the string a2 into the
// string member at +0x108 (0x08B35F60 length, 0x08B945C4 assign) or, when a2 is null or empty, resets it from the empty string
// (0x08B958A8) and drops the temporary (representation compared with 0x08D75298, freed by 0x08B93554).
void set_flag_and_text(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 on = ctx.gpr[5] & 0xFFu;
    const u32 text = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    constexpr u32 kEmpty = 0x08D75298u;
    ctx.gpr[5] = on;
    ctx.gpr[2] = 0x08D70000u;
    ctx.gpr[18] = kEmpty;
    ctx.gpr[17] = text;
    const u32 settings = word(rt, self + 0x5Cu);
    ctx.gpr[4] = frame.at(4u);
    put(rt, settings + 0x100u, (word(rt, settings + 0x100u) & ~4u) | ((on & 1u) << 2));
    const u32 block = word(rt, self + 0x5Cu);
    ctx.gpr[2] = block + 0x110u;
    put(rt, block + 0x110u, 0u);
    put(rt, block + 0x110u + 0xCu, 0u);
    put(rt, block + 0x110u + 4u, 0u);
    put(rt, block + 0x110u + 8u, 0u);
    bool has_text = false;
    if (text != 0u) {
        ctx.gpr[2] = signed_byte(rt, text);
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[16] = word(rt, self + 0x5Cu);
            has_text = true;
        }
    }
    if (has_text) {
        const u32 length = call_set(rt, ctx, 0x08B35F60u, {text});
        ctx.gpr[16] += 0x108u;
        call_set(rt, ctx, 0x08B945C4u, {ctx.gpr[16], text, length});
    } else {
        const u32 member = word(rt, self + 0x5Cu) + 0x108u;
        ctx.gpr[5] = member;
        put(rt, frame.at(4u), kEmpty + 0xCu);
        call_set(rt, ctx, 0x08B958A8u, {frame.at(4u), member});
        drop_string(rt, ctx, frame, word(rt, frame.at(4u)), kEmpty, 0x08B93554u);
    }
}

// 0x08891194: initialises the record a0: fields +0x1C = 1, +0x20 = 2 and zeros, then three entries (+0x2C / +0x38 / +0x5C zero, a member
// from 0x08B62344(8) prepared by 0x0888F3CC at +0x44, the constant 0x481907FF at +0x50, a zero byte at +0x68 + i) and the tail fields
// (+0x74 = 1, bytes +0x6B / +0x6C and word +0x70 zero). v0 = 1.
void init_entry_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    ctx.gpr[22] = 0x481907FFu;
    ctx.gpr[21] = 3u;
    ctx.gpr[20] = 0u;
    ctx.gpr[19] = self;
    ctx.gpr[18] = self;
    ctx.gpr[17] = self;
    put(rt, self + 0x1Cu, 1u);
    ctx.gpr[2] = 2u;
    put(rt, self + 0x20u, 2u);
    put(rt, self + 0x18u, 0u);
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    do {
        put(rt, ctx.gpr[17] + 0x2Cu, 0u);
        ctx.gpr[20] += 1u;
        put(rt, ctx.gpr[17] + 0x38u, 0u);
        const u32 member = call_set(rt, ctx, 0x08B62344u, {8u});
        ctx.gpr[16] = member;
        call_set(rt, ctx, 0x0888F3CCu, {member});
        put(rt, ctx.gpr[17] + 0x44u, member);
        put(rt, ctx.gpr[17] + 0x50u, ctx.gpr[22]);
        put(rt, ctx.gpr[17] + 0x5Cu, 0u);
        ctx.gpr[17] += 4u;
        rt.memory().store8(ctx.gpr[19] + 0x68u, 0u);
        ctx.gpr[19] += 1u;
    } while (ctx.gpr[20] != ctx.gpr[21]);
    ctx.gpr[2] = 1u;
    put(rt, self + 0x74u, 1u);
    rt.memory().store8(self + 0x6Bu, 0u);
    rt.memory().store8(self + 0x6Cu, 0u);
    put(rt, self + 0x70u, 0u);
}

// 0x088F4AB8: four rounds over the table at *(a0 + 4) (count word +0x404, 4-byte slots from the base): while the count + 0x10 is at most
// 0x100 the next slot is reserved (0x10 stored at slot + 4), 0x088F587C fills the entry at slot + 8 for (a1, round) and, when its float at
// +0x2C exceeds 0.01, the count is raised by the slot's halfword. v0 follows the last step as the asm leaves it.
void fill_table_entries(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    ctx.gpr[21] = 0x10u;
    ctx.gpr[20] = 4u;
    ctx.gpr[19] = argument;
    ctx.gpr[18] = self;
    ctx.gpr[17] = 0u;
    ctx.fpr[20] = 0.01f;
    u32 table = word(rt, self + 4u);
    ctx.gpr[2] = table;
    for (;;) {
        u32 count = word(rt, table + 0x404u);
        const u32 slot = table + (count << 2);
        ctx.gpr[2] = slot;
        count += 0x10u;
        const u32 entry = slot + 8u;
        ctx.gpr[16] = entry;
        if (count < 0x101u) {
            ctx.gpr[6] = ctx.gpr[17];
            ctx.gpr[4] = entry;
            ctx.gpr[5] = argument;
            put(rt, slot + 4u, ctx.gpr[21]);
            if (entry != 0u) {
                call_set(rt, ctx, 0x088F587Cu, {entry, argument, ctx.gpr[17]});
                if (ctx.fpr[20] < float_at(rt, entry + 0x2Cu)) {
                    const u32 current = word(rt, self + 4u);
                    const u32 now = word(rt, current + 0x404u);
                    const u32 address = (now << 2) + current;
                    ctx.gpr[2] = address;
                    put(rt, current + 0x404u, now + rt.memory().load16(address + 4u));
                }
            }
        }
        ctx.gpr[17] += 1u;
        if (ctx.gpr[17] == ctx.gpr[20]) break;
        table = word(rt, self + 4u);
        ctx.gpr[2] = table;
    }
}

// 0x08931D70: updates the pair (a2, a3) kept at +8 / +0xC of a0 (-1, -1 = none): returns 0 for a request of (-1, -1), 1 when the pair is the
// stored one, and otherwise validates it with 0x089368DC (0 on failure), runs 0x08931B58, re-registers it (0x08936584) and returns 1 when
// the stored word pair at *(a0 + 4) is (-1, -1) and else whether 0x08931CA0 reports success.
void update_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    ctx.gpr[19] = 0xFFFFFFFFu;
    ctx.gpr[18] = self;
    ctx.gpr[17] = second;
    ctx.gpr[16] = first;
    if (first == 0xFFFFFFFFu) {
        ctx.gpr[2] = 0u;
        if (second == 0xFFFFFFFFu) return;
    }
    ctx.gpr[2] = word(rt, self + 8u);
    if (ctx.gpr[2] == first) {
        ctx.gpr[2] = word(rt, self + 0xCu);
        const bool same = ctx.gpr[2] == second;
        ctx.gpr[2] = 1u;
        if (same) return;
    }
    const u32 object = call_set(rt, ctx, 0x089363FCu, {});
    ctx.gpr[6] = first;
    ctx.gpr[7] = second;
    const bool valid = call_set(rt, ctx, 0x089368DCu, {object, ctx.gpr[5], first, second}) != 0u;
    ctx.gpr[2] = 0u;
    if (!valid) return;
    call_set(rt, ctx, 0x08931B58u, {self});
    const u32 target = call_set(rt, ctx, 0x089363FCu, {});
    const u32 stored = word(rt, self + 4u);
    call_set(rt, ctx, 0x08936584u, {target, ctx.gpr[5], first, second, stored});
    const u32 current = word(rt, self + 4u);
    put(rt, self + 8u, first);
    put(rt, self + 0xCu, second);
    const u32 stored_first = word(rt, current);
    const u32 stored_second = word(rt, current + 4u);
    if (stored_first == 0xFFFFFFFFu) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        const bool both = stored_second == 0xFFFFFFFFu;
        ctx.gpr[2] = 1u;
        if (both) return;
    }
    ctx.gpr[2] = call_set(rt, ctx, 0x08931CA0u, {self}) != 0u ? 1u : 0u;
}

// 0x08958410: a float lookup in the 24-byte records at 0x08CF3440, selected by the byte at *(a0 + 0xC) + 0x140E: kind 2 interpolates the
// object's float at +0x6C4 between the record's two limits (+0x10, +0x14) scaled by +0xC, kind 4 tail-jumps to 0x08954F18 with a1 halved,
// kind 3 asks 0x08955300 and returns 0 or the record's +0xC; the result is in f0, v0 follows the kind.
void interpolate_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(ctx);
        const u32 self = ctx.gpr[4];
        const u32 kind_index = rt.memory().load8(word(rt, self + 0xCu) + 0x140Eu);
        const u32 record = kind_index * 24u + 0x08CF3440u;
        ctx.gpr[16] = record;
        const u32 kind = rt.memory().load8(record);
        ctx.gpr[2] = 4u;
        if (kind == 2u) {
            const u32 object = word(rt, self + 8u);
            ctx.gpr[2] = object;
            const float high = float_at(rt, record + 0x14u);
            float value = float_at(rt, object + 0x6C4u);
            if (high <= value) {
                value = float_at(rt, record + 0xCu);
            } else {
                const float low = float_at(rt, record + 0x10u);
                const bool below = low < value;
                value = value - low;
                if (!below) {
                    value = 0.0f;
                } else {
                    float scaled = float_at(rt, record + 0xCu);
                    const float span = high - low;
                    scaled = scaled * value;
                    value = scaled / span;
                }
            }
            ctx.fpr[0] = value;
        } else if (kind == 4u) {
            ctx.gpr[5] = static_cast<u32>(static_cast<s32>(ctx.gpr[5]) >> 1);
            tail = true;
        } else {
            ctx.gpr[2] = 3u;
            ctx.fpr[0] = 0.0f;
            if (kind == 3u) {
                const u32 result = call_set(rt, ctx, 0x08955300u, {});
                const u32 flag_address = rt.memory().load8(result + 0xE9u) + 0x08CDE0E8u;
                const u32 flag = rt.memory().load8(flag_address);
                ctx.gpr[2] = 0x08CDE0E8u;
                ctx.gpr[3] = flag_address;
                ctx.gpr[4] = flag;
                ctx.fpr[0] = flag != 0u ? 0.0f : float_at(rt, record + 0xCu);
            }
        }
    }
    if (tail) jump(ctx, 0x08954F18u);
}

// 0x089CF9D4: walks backwards from the index kept at +0x198 over the entries the flags at +0x14C allow (bit 2 skips the check, bit 1 allows
// wrapping through 0x089CE124): for a1 accepted entries 0x089CF084 / 0x089CF978 decide, and the final index is applied with 0x089CE914;
// returns 0 when the index did not move and 1 otherwise.
void step_back_entries(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 limit = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    ctx.gpr[19] = limit;
    ctx.gpr[18] = 0u;
    ctx.gpr[17] = self;
    u32 index = word(rt, self + 0x198u);
    ctx.gpr[16] = index;
    ctx.gpr[2] = index;
    u32 current = index;
    if (static_cast<s32>(limit) > 0) {
        u32 flags = word(rt, self + 0x14Cu);
        for (;;) {
            ctx.gpr[2] = flags & 4u;
            index -= 1u;
            ctx.gpr[16] = index;
            if ((flags & 4u) == 0u) {
                ctx.gpr[4] = self;
                ctx.gpr[2] = flags & 2u;
                if (static_cast<s32>(index) < 0) {
                    if ((flags & 2u) == 0u) {
                        current = word(rt, self + 0x198u);
                        ctx.gpr[2] = current;
                        index = 0u;
                        break;
                    }
                    index = call_set(rt, ctx, 0x089CE124u, {self}) - 1u;
                    ctx.gpr[16] = index;
                }
                current = word(rt, self + 0x198u);
                ctx.gpr[2] = current;
                const bool same = index == current;
                ctx.gpr[2] = 0u;
                if (same) return;
            }
            ctx.gpr[16] = index;
            const u32 accepted = call_set(rt, ctx, 0x089CF084u, {self, index});
            if (accepted != 0u) {
                const u32 confirmed = call_set(rt, ctx, 0x089CF978u, {self, index});
                if (confirmed == 0u) {
                    flags = word(rt, self + 0x14Cu);
                    continue;
                }
            }
            ctx.gpr[18] += 1u;
            if (limit == ctx.gpr[18]) {
                current = word(rt, self + 0x198u);
                ctx.gpr[2] = current;
                break;
            }
            flags = word(rt, self + 0x14Cu);
        }
    }
    ctx.gpr[16] = index;
    ctx.gpr[4] = self;
    if (index == current) {
        ctx.gpr[2] = 0u;
    } else {
        call_set(rt, ctx, 0x089CE914u, {self, index});
        ctx.gpr[2] = 1u;
    }
}

// 0x08A175B4: tears down the static at 0x08CF6240 (0x08B344E0 on the lock 0x08CF6250 first, 0x08B3457C last): when the object pointer is
// set it is cleared, the string at +0x08CF6244 is reset (0x08BEEF60, dropped), the handle at 0x08CF6248 is released (0x08A5B930), the object
// is shut down (0x08B0EA58) and its virtual +4 is called.
void tear_down_static(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    constexpr u32 kPointer = 0x08CF6240u;
    constexpr u32 kText = 0x08CF6244u;
    ctx.gpr[20] = 0x08CF0000u;
    ctx.gpr[16] = 0x08CF0000u;
    call_set(rt, ctx, 0x08B344E0u, {0x08CF6250u});
    ctx.gpr[2] = 0x08CF0000u;
    ctx.gpr[18] = 0x08D74FD0u;
    const u32 object = word(rt, kPointer);
    ctx.gpr[17] = object;
    ctx.gpr[19] = kText;
    if (object != 0u) {
        put(rt, kPointer, 0u);
        put(rt, frame.at(4u), 0x08D74FD0u + 0xCu);
        call_set(rt, ctx, 0x08BEEF60u, {frame.at(4u), kText});
        drop_string(rt, ctx, frame, word(rt, frame.at(4u)), 0x08D74FD0u, 0x08B8542Cu);
        const u32 handle = word(rt, kText + 4u);
        ctx.gpr[2] = handle;
        ctx.gpr[4] = handle;
        if (handle != 0u) call_set(rt, ctx, 0x08A5B930u, {handle});
        put(rt, kText + 4u, 0u);
        call_set(rt, ctx, 0x08B0EA58u, {object});
        ctx.gpr[2] = word(rt, object);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + 4u);
        call_set(rt, ctx, ctx.gpr[3], {object});
    }
    call_set(rt, ctx, 0x08B3457Cu, {0x08CF6250u});
}

// 0x08A23ABC: for an object with a target handle (+0x58) runs 0x08A23838 / 0x08AC0B5C and, for each of the two entries at +0x10 and +0x34
// whose flag byte (+0xD) is set, creates a handle (0x08ABB550 into +0x5C / +0x60), configures it (0x08ABB61C, 0x08ABB7B8 with 0 and 3)
// and starts it (0x08ABB64C); then tail-jumps to 0x08A238A8 with the object.
void start_handles(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(ctx);
        ctx.gpr[18] = self + 0x34u;
        ctx.gpr[17] = self + 0x10u;
        const u32 target = word(rt, self + 0x58u);
        ctx.gpr[2] = target;
        ctx.gpr[16] = self;
        if (target != 0u) {
            call_set(rt, ctx, 0x08A23838u, {});
            call_set(rt, ctx, 0x08AC0B5Cu, {});
            ctx.gpr[2] = rt.memory().load8(self + 0x10u + 0xDu);
            if (ctx.gpr[2] != 0u) {
                const u32 handle = call_set(rt, ctx, 0x08ABB550u, {word(rt, self + 0x58u)});
                put(rt, self + 0x5Cu, handle);
                call_set(rt, ctx, 0x08ABB61Cu, {handle, word(rt, self + 0x10u + 4u)});
                call_set(rt, ctx, 0x08ABB7B8u, {word(rt, self + 0x5Cu), 0u, 3u});
                call_set(rt, ctx, 0x08ABB64Cu, {word(rt, self + 0x5Cu)});
            }
            ctx.gpr[2] = rt.memory().load8(self + 0x34u + 0xDu);
            if (ctx.gpr[2] != 0u) {
                const u32 handle = call_set(rt, ctx, 0x08ABB550u, {word(rt, self + 0x58u)});
                put(rt, self + 0x60u, handle);
                call_set(rt, ctx, 0x08ABB61Cu, {handle, word(rt, self + 0x34u + 4u)});
                call_set(rt, ctx, 0x08ABB7B8u, {word(rt, self + 0x60u), 0u, 3u});
                call_set(rt, ctx, 0x08ABB64Cu, {word(rt, self + 0x60u)});
            }
            ctx.gpr[4] = self;
            tail = true;
        }
    }
    if (tail) jump(ctx, 0x08A238A8u);
}

// 0x08ACAC58: opens the object a1 for a0: a null object or a failed check (0x08ACA4BC) gives the error code 0x80001000; otherwise the
// steps 0x08ACA460, 0x08AD3F40 and 0x08ACA754 (a0, the context, a stack record) run and, when they succeed, the record is turned into a
// handle (0x08C7AFF0; an error goes through 0x08ACA144), registered at +4 (0x08B3412C) and linked into +0x22C (0x08ACAC0C, 0x08ACA3C8). v0 is the code.
void open_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 context = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    ctx.gpr[17] = object;
    ctx.gpr[16] = context;
    u32 code = context;
    if (object == 0u) {
        ctx.gpr[2] = 0x80000000u;
        code = 0x80001000u;
    } else {
        const bool missing = call_set(rt, ctx, 0x08ACA4BCu, {object}) == 0u;
        ctx.gpr[2] = 0x80000000u;
        if (missing) {
            code = 0x80001000u;
        } else {
            call_set(rt, ctx, 0x08ACA460u, {object});
            const u32 step = call_set(rt, ctx, 0x08AD3F40u, {});
            code = call_set(rt, ctx, 0x08ACA754u, {context, step, frame.at(0u)});
            ctx.gpr[16] = code;
            ctx.gpr[4] = frame.at(0u);
            if (static_cast<s32>(code) >= 0) {
                ctx.gpr[18] = object + 8u;
                const u32 handle = call_set(rt, ctx, 0x08C7AFF0u, {frame.at(0u)});
                if (static_cast<s32>(handle) < 0) {
                    code = call_set(rt, ctx, 0x08ACA144u, {handle});
                } else {
                    const u32 registered = call_set(rt, ctx, 0x08B3412Cu, {handle});
                    put(rt, object + 4u, registered);
                    code = call_set(rt, ctx, 0x08ACAC0Cu, {object + 8u});
                    ctx.gpr[16] = code;
                    if (static_cast<s32>(code) >= 0) call_set(rt, ctx, 0x08ACA3C8u, {object + 0x22Cu, object + 8u});
                }
            }
        }
    }
    ctx.gpr[16] = code;
    ctx.gpr[2] = code;
}

// 0x08ADE0E4: destructor-like unlink (a0 = the object, a1 = the delete flag): when bit 0 of the byte at +0x8F is set the object at +0x90 is
// notified (virtual +0x18 with a1 = 1 when a1 is set, then its field +4 is called with a0), the handles at +0x12 / +0x22 (halfwords, signed)
// are released (0x08ADC230 / 0x08ADA528), +0xC is cleared, 0x08AEB808 runs, the node is unlinked from its list and re-inserted after the
// sentinel at 0x08D40688 (0x08AEB82C last).
void destroy_node(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(ctx);
    ctx.gpr[2] = rt.memory().load8(self + 0x8Fu) & 1u;
    ctx.gpr[16] = self;
    if (ctx.gpr[2] != 0u) {
        if (flag == 0u) {
            ctx.gpr[2] = word(rt, self + 0x90u);
        } else {
            ctx.gpr[2] = word(rt, self + 0x90u);
            ctx.gpr[3] = word(rt, ctx.gpr[2] + 0x18u);
            if (ctx.gpr[3] != 0u) {
                ctx.gpr[5] = 1u;
                call_set(rt, ctx, ctx.gpr[3], {self});
                ctx.gpr[2] = word(rt, self + 0x90u);
            }
        }
        ctx.gpr[2] = word(rt, ctx.gpr[2] + 4u);
        if (ctx.gpr[2] != 0u) call_set(rt, ctx, ctx.gpr[2], {self});
    }
    ctx.gpr[4] = signed_half(rt, self + 0x12u);
    if (static_cast<s32>(ctx.gpr[4]) >= 0) call_set(rt, ctx, 0x08ADC230u, {ctx.gpr[4]});
    ctx.gpr[4] = signed_half(rt, self + 0x22u);
    if (static_cast<s32>(ctx.gpr[4]) >= 0) call_set(rt, ctx, 0x08ADA528u, {ctx.gpr[4]});
    put(rt, self + 0xCu, 0u);
    call_set(rt, ctx, 0x08AEB808u, {});
    const u32 next = word(rt, self);
    ctx.gpr[4] = next;
    constexpr u32 kSentinel = 0x08D40688u;
    ctx.gpr[5] = 0x08D40000u;
    ctx.gpr[6] = kSentinel;
    if (next != self) {
        ctx.gpr[2] = word(rt, self + 4u);
        put(rt, next + 4u, ctx.gpr[2]);
        const u32 previous = word(rt, self + 4u);
        ctx.gpr[3] = previous;
        put(rt, self + 4u, self);
        put(rt, previous, next);
        put(rt, self, self);
    }
    const u32 head = word(rt, kSentinel);
    ctx.gpr[3] = head;
    put(rt, self + 4u, kSentinel);
    put(rt, head + 4u, self);
    ctx.gpr[2] = word(rt, self + 4u);
    put(rt, self, head);
    put(rt, ctx.gpr[2], self);
    call_set(rt, ctx, 0x08AEB82Cu, {});
}

// 0x08B436DC: for t0 == 12 and a non-zero byte at a3 + 0xA, searches the table at a0 + 0x454 (count at +0x480, used flags from +0x400, keys
// from +0x200) for the signed halfword at a3 + 8; for a hit with a non-null entry the object's virtual +0x90 is asked and its low halfword
// stored into the entry (+0x16) before tail-jumping to 0x08B43554 with a2 = that value.
void find_entry_and_update(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 record = ctx.gpr[7];
    const u32 kind = ctx.gpr[8];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(ctx);
        ctx.gpr[2] = 0xCu;
        ctx.gpr[16] = self;
        if (kind == 0xCu) {
            ctx.gpr[2] = rt.memory().load8(record + 0xAu);
            const u32 table = self + 0x454u;
            ctx.gpr[8] = table;
            if (ctx.gpr[2] != 0u) {
                const u32 count = word(rt, table + 0x480u);
                const u32 wanted = signed_half(rt, record + 8u);
                ctx.gpr[6] = count;
                ctx.gpr[7] = wanted;
                if (count != 0u) {
                    bool found = false;
                    u32 used = table;
                    u32 key = table;
                    u32 index = 0u;
                    do {
                        ctx.gpr[2] = rt.memory().load8(used + 0x400u);
                        used += 1u;
                        if (ctx.gpr[2] != 0u) {
                            ctx.gpr[2] = word(rt, key + 0x200u);
                            if (wanted == ctx.gpr[2]) {
                                ctx.gpr[2] = index << 2;
                                found = static_cast<s32>(index) >= 0;
                                break;
                            }
                        }
                        index += 1u;
                        key += 4u;
                    } while (count != index);
                    if (found) {
                        ctx.gpr[2] += table;
                        ctx.gpr[2] = word(rt, ctx.gpr[2]);
                        if (ctx.gpr[2] != 0u) {
                            const u32 entry = word(rt, ctx.gpr[2]);
                            ctx.gpr[17] = entry;
                            if (entry != 0u) {
                                ctx.gpr[3] = word(rt, self);
                                ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x90u);
                                const u32 produced = static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(call_set(rt, ctx, ctx.gpr[2], {self}))));
                                ctx.gpr[2] = produced;
                                ctx.gpr[5] = signed_half(rt, entry);
                                rt.memory().store16(entry + 0x16u, static_cast<std::uint16_t>(produced));
                                ctx.gpr[4] = self;
                                ctx.gpr[6] = produced;
                                tail = true;
                            }
                        }
                    }
                }
            }
        }
    }
    if (tail) jump(ctx, 0x08B43554u);
}

// 0x08BB191C: sift-up of a heap of word pointers (a0 = array, a1 = the hole index, a2 = the root index, a3 = the value, t0 = the compare
// function): while the hole is below the root and compare(parent, value) holds, the parent moves down; the value is stored at the final
// slot.
void heap_sift_up(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 array = ctx.gpr[4];
    const u32 hole_start = ctx.gpr[5];
    const u32 root = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    const u32 compare = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 adjusted = (hole_start - 1u) + ((hole_start - 1u) >> 31);
    ctx.gpr[2] = adjusted;
    ctx.gpr[19] = array;
    ctx.gpr[22] = compare;
    ctx.gpr[21] = root;
    ctx.gpr[20] = value;
    u32 slot = array + (hole_start << 2);
    ctx.gpr[18] = slot;
    u32 parent = static_cast<u32>(static_cast<s32>(adjusted) >> 1);
    ctx.gpr[17] = parent;
    u32 hole = hole_start;
    ctx.gpr[16] = hole;
    if (static_cast<s32>(root) < static_cast<s32>(hole_start)) {
        ctx.gpr[2] = parent << 2;
        for (;;) {
            slot = array + (parent << 2);
            ctx.gpr[18] = slot;
            const u32 verdict = call_set(rt, ctx, compare, {word(rt, slot), value});
            const u32 before = parent - 1u;
            const u32 hole_address = (hole << 2) + array;
            const u32 next_parent = static_cast<u32>(static_cast<s32>(before + (before >> 31)) >> 1);
            ctx.gpr[5] = hole_address;
            if (verdict == 0u) {
                ctx.gpr[2] = hole << 2;
                slot = array + (hole << 2);
                ctx.gpr[18] = slot;
                break;
            }
            const u32 moved = word(rt, slot);
            const bool continue_up = static_cast<s32>(root) < static_cast<s32>(parent);
            ctx.gpr[2] = continue_up ? 1u : 0u;
            hole = parent;
            ctx.gpr[16] = hole;
            put(rt, hole_address, moved);
            if (!continue_up) break;
            parent = next_parent;
            ctx.gpr[17] = parent;
            ctx.gpr[2] = parent << 2;
        }
    }
    put(rt, slot, value);
}

// 0x08843978: for a count a2 > 0 copies a 0x88-byte block: the object behind the handle slot of 0x088435A8 (null message 0x08C9C7CC) is
// asked for the destination (0x08843404), the slot released (0x08A5B9EC), the same is done for the source with a3, and the block is copied
// 16 bytes at a time with the last eight bytes at +0x80. v0 = the source word at +0x84.
void copy_block(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    ctx.gpr[17] = holder;
    if (static_cast<s32>(count) <= 0) return;
    const u32 first_slot = frame.at(4u);
    ctx.gpr[18] = first_slot;
    call_set(rt, ctx, 0x088435A8u, {first_slot});
    u32 object = word(rt, first_slot);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CA0000u;
    if (object == 0u) report_null(rt, ctx, 0x08C9C7CCu);
    const u32 destination = call_set(rt, ctx, 0x08843404u, {object});
    ctx.gpr[19] = destination;
    call_set(rt, ctx, 0x08A5B9ECu, {first_slot});
    call_set(rt, ctx, 0x088435A8u, {frame.at(0u), holder});
    object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CA0000u;
    if (object == 0u) report_null(rt, ctx, 0x08C9C7CCu);
    u32 source = call_set(rt, ctx, 0x08843404u, {object});
    ctx.gpr[16] = source;
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    u32 to = destination;
    const u32 end = source + 0x80u;
    do {
        const u32 w0 = word(rt, source);
        const u32 w1 = word(rt, source + 4u);
        const u32 w2 = word(rt, source + 8u);
        const u32 w3 = word(rt, source + 0xCu);
        put(rt, to, w0);
        source += 0x10u;
        to += 0x10u;
        put(rt, to - 0xCu, w1);
        put(rt, to - 8u, w2);
        put(rt, to - 4u, w3);
    } while (source != end);
    const u32 last_high = word(rt, source + 4u);
    const u32 last_low = word(rt, source);
    ctx.gpr[2] = last_high;
    put(rt, to + 4u, last_high);
    put(rt, to, last_low);
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08B4CA94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { compose_angles(rt, ctx, true, true, false, false); }
void func_08B4CD24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { compose_angles(rt, ctx, true, false, true, false); }
void func_0880A330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_with_member(rt, ctx); }
void func_088730B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { set_flag_and_text(rt, ctx); }
void func_08891194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_entry_record(rt, ctx); }
void func_088F4AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fill_table_entries(rt, ctx); }
void func_08931D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { update_pair(rt, ctx); }
void func_08958410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { interpolate_record(rt, ctx); }
void func_089CF9D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { step_back_entries(rt, ctx); }
void func_08A175B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tear_down_static(rt, ctx); }
void func_08A23ABC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { start_handles(rt, ctx); }
void func_08AC98C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { bool tail = false; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(8u), G[18]); put(rt, frame.at(4u), G[17]); put(rt, frame.at(0u), G[16]); G[19] = word(rt, G[5] + 4u); G[17] = G[19] + 0x30u; if (G[19] != 0u) { G[16] = word(rt, G[19] + 0x30u); G[2] = word(rt, G[19] + 0x34u); G[18] = G[19] + 0x34u; if (G[16] != G[2]) { G[2] = G[16]; do { G[2] = word(rt, G[2]); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08AD5358u); G[3] = word(rt, G[18]); G[2] = G[16] + 4u; G[16] = G[2]; } while (G[3] != G[2]); G[2] = word(rt, G[17]); } else { G[3] = word(rt, G[18]); G[2] = word(rt, G[17]); } G[17] = G[2]; if (G[2] != G[3]) { do { G[16] = word(rt, G[2]); G[17] += 4u; G[4] = G[16]; if (G[16] != 0u) { put(rt, G[2], 0u); call_here(rt, ctx, 0x08AD5394u); G[2] = word(rt, G[16]); G[3] = word(rt, G[2] + 4u); G[4] = G[16]; call_here(rt, ctx, G[3]); G[3] = word(rt, G[18]); } G[2] = G[17]; } while (G[3] != G[17]); } G[4] = G[19]; tail = true; } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(8u)); G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } if (tail) jump(ctx, 0x08AC9240u); } }
void func_08ACAC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { open_object(rt, ctx); }
void func_08ADE0E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_node(rt, ctx); }
void func_08B436DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_entry_and_update(rt, ctx); }
void func_08BB191C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { heap_sift_up(rt, ctx); }
void func_08BBF7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); G[21] = G[5]; put(rt, frame.at(0x14u), G[17]); G[17] = G[4]; put(rt, frame.at(0x10u), G[16]); G[16] = G[6]; put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); if (G[4] != G[5]) { do { G[4] = G[16]; G[5] = G[17]; G[19] = G[17] + 4u; G[18] = G[16] + 4u; if (G[16] != 0u) { G[20] = 0u; call_here(rt, ctx, 0x08B9432Cu); G[3] = word(rt, G[17] + 4u); G[2] = word(rt, G[19] + 4u); put(rt, G[16] + 4u, 0u); G[4] = 0u; G[2] -= G[3]; G[2] = sra(G[2], 4); put(rt, G[18] + 4u, 0u); put(rt, G[18] + 8u, 0u); if (G[2] != 0u) { G[20] = G[2] << 4; G[4] = G[20]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; } put(rt, G[18] + 4u, G[4]); G[2] = G[4] + G[20]; G[6] = G[4]; put(rt, G[18], G[4]); G[7] = frame.at(0u); G[5] = word(rt, G[19] + 4u); G[4] = word(rt, G[19]); put(rt, G[18] + 8u, G[2]); call_here(rt, ctx, 0x08BBEC68u); put(rt, G[18] + 4u, G[2]); G[2] = word(rt, G[17] + 0x10u); put(rt, G[16] + 0x10u, G[2]); } G[17] += 0x14u; G[16] += 0x14u; } while (G[21] != G[17]); } G[2] = G[16]; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }
void func_0881B324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x10u); auto su = [&](std::uint32_t a, std::uint32_t v) { store8(a, v); store8(a + 1u, v >> 8); store8(a + 2u, v >> 16); store8(a + 3u, v >> 24); }; G[2] = (G[4] | G[5]) & 3u; put(rt, frame.at(4u), G[17]); G[7] = G[4]; G[17] = G[4]; put(rt, frame.at(0u), G[16]); G[6] = G[5]; G[16] = G[5]; put(rt, frame.at(8u), G[31]); G[8] = G[4] + 0x30u; do { G[2] = word(rt, G[7]); G[3] = word(rt, G[7] + 4u); G[4] = word(rt, G[7] + 8u); G[5] = word(rt, G[7] + 0xCu); G[7] += 0x10u; if ((((G[17] | G[16]) & 3u)) != 0u) { su(G[6], G[2]); su(G[6] + 4u, G[3]); su(G[6] + 8u, G[4]); su(G[6] + 0xCu, G[5]); } else { put(rt, G[6], G[2]); put(rt, G[6] + 4u, G[3]); put(rt, G[6] + 8u, G[4]); put(rt, G[6] + 0xCu, G[5]); } G[6] += 0x10u; } while (G[7] != G[8]); G[6] = word(rt, G[17]); G[5] = word(rt, G[17] + 0x30u); G[16] += 0x30u; G[2] = G[6] << 5; G[6] = G[6] << 1; G[4] = G[16]; G[6] += G[2]; call_here(rt, ctx, 0x08B79A88u); G[2] = word(rt, G[17]); G[3] = G[2] << 5; G[2] = G[2] << 1; G[2] += G[3]; G[2] = G[16] + G[2]; G[17] = word(rt, frame.at(4u)); G[16] = word(rt, frame.at(0u)); } } }
void func_0883F654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[18]); G[18] = G[4]; put(rt, frame.at(0x30u), G[16]); G[16] = G[6]; put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x34u), G[17]); if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = frame.at(0x10u); G[4] = G[17]; call_here(rt, ctx, 0x08931B74u); G[16] = word(rt, G[16]); if (G[16] == 0u) { G[4] = 0x08C9BD48u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[5] = G[16]; G[2] = word(rt, G[3] + 0xCu); G[4] = frame.at(8u); call_here(rt, ctx, G[2]); G[5] = word(rt, frame.at(8u)); G[4] = G[17]; call_here(rt, ctx, 0x08931E54u); G[4] = word(rt, frame.at(8u)); if (G[2] != 0u) { G[16] = frame.at(4u); G[4] = G[16]; G[5] = G[17]; G[6] = 0u; call_here(rt, ctx, 0x0883F508u); G[5] = word(rt, frame.at(4u)); if (G[18] != G[16]) { G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(8u)); } G[2] = 0x08D74FD0u; G[16] = G[4] - 0xCu; G[4] -= 4u; if (G[16] != G[2]) { G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; if (static_cast<std::int32_t>(G[2]) <= 0) { G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } G[4] = G[17]; call_here(rt, ctx, 0x08931C38u); } G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } }
void func_08843978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { copy_block(rt, ctx); }
void func_0885017C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x14u), G[17]); G[17] = frame.at(4u); put(rt, frame.at(0x18u), G[18]); G[18] = G[4]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[4] = G[17]; call_here(rt, ctx, 0x088547C4u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9D3BCu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08854678u); G[16] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x088A5D0Cu); G[16] = G[2]; G[4] = 0x10u; call_here(rt, ctx, 0x08B2B5E0u); G[9] = word(rt, G[16] + 0x254u); G[4] = word(rt, G[16] + 0x24Cu); G[5] = word(rt, G[16] + 0x250u); G[3] = word(rt, G[16] + 0x248u); G[6] = G[2]; put(rt, G[2] + 4u, G[4]); G[4] = frame.at(0u); put(rt, G[2] + 0xCu, G[9]); G[8] = 0x0884F734u; G[7] = 0xFFFFFFFFu; put(rt, G[2], G[3]); G[9] = 0u; put(rt, G[2] + 8u, G[5]); G[2] = 0x08CE0000u; G[5] = word(rt, 0x08CE2CFCu); call_here(rt, ctx, 0x08A52AB8u); G[4] = G[18]; if (G[18] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[3] = 0x08CE0000u; G[2] = word(rt, 0x08CE2CECu); G[2] += 1u; put(rt, 0x08CE2CECu, G[2]); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_267
} // namespace gt::game

#define HOT267(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_267::name, gt::ReturnKind::kind)
HOT267(0x08B4CA94u, func_08B4CA94, Word);
HOT267(0x08B4CD24u, func_08B4CD24, Word);
HOT267(0x0880A330u, func_0880A330, Word);
HOT267(0x088730B0u, func_088730B0, Word);
HOT267(0x08891194u, func_08891194, Word);
HOT267(0x088F4AB8u, func_088F4AB8, Word);
HOT267(0x08931D70u, func_08931D70, Word);
HOT267(0x08958410u, func_08958410, Float);
HOT267(0x089CF9D4u, func_089CF9D4, Word);
HOT267(0x08A175B4u, func_08A175B4, Word);
HOT267(0x08A23ABCu, func_08A23ABC, Word);
HOT267(0x08AC98C0u, func_08AC98C0, Word);
HOT267(0x08ACAC58u, func_08ACAC58, Word);
HOT267(0x08ADE0E4u, func_08ADE0E4, Word);
HOT267(0x08B436DCu, func_08B436DC, Word);
HOT267(0x08BB191Cu, func_08BB191C, Word);
// HOT267(0x08BBF7E8u, func_08BBF7E8, Word);  // no comparable synthetic case yet
HOT267(0x0881B324u, func_0881B324, Word);
HOT267(0x0883F654u, func_0883F654, Word);
HOT267(0x08843978u, func_08843978, Word);
// HOT267(0x0885017Cu, func_0885017C, Word);  // no comparable synthetic case yet
