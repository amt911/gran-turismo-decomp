// Hot batch 321: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/game/hot_v2.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_321 {
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

// The `bne v0, v1; addiu v0, v0, -1` loop the compiler emitted as a delay: it ends with v0 = -2 (and v1 = -1 as it was set up).
void delay_loop(psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0xFFFFFFFEu;
    ctx.gpr[3] = 0xFFFFFFFFu;
}

// 0x089AA334 (a0 = the object, a1 = the points, a2 = the count, a3 = the value handed to every copy, t0 = the address of a handle word, f12 = a float):
// a non-zero handle word is turned into a record at sp + 0x10 (0x089E4BC4, stored at +0x11C with the flag byte +0x118 set), the virtual +0x150 of
// the object opens, then for every pair of 8-byte points (a2 - 1 of them, two at a time) the points are copied to sp and sp + 8 (0x08B4AC54
// with the value) and 0x089A9E58(object, sp, record) draws them with f12; the virtual +0x154 closes. v0 = that call's.
void draw_point_pairs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    u32 points = ctx.gpr[5];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 value = ctx.gpr[7];
    const u32 handle_address = ctx.gpr[8];
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{52u, 0x70u}, {22u, 0x58u}, {19u, 0x4Cu}, {18u, 0x48u}, {17u, 0x44u}, {31u, 0x64u}, {30u, 0x60u}, {23u, 0x5Cu}, {21u, 0x54u}, {20u, 0x50u}, {16u, 0x40u}});
    ctx.fpr[20] = scale;
    ctx.gpr[22] = object;
    ctx.gpr[19] = points;
    ctx.gpr[18] = static_cast<u32>(count);
    ctx.gpr[17] = handle_address;
    put(rt, frame.at(0x34u), value);
    delay_loop(ctx);
    const u32 handle = word(rt, handle_address);
    put(rt, frame.at(0x30u), 0u);
    if (handle != 0u) {
        const u32 record = frame.at(0x10u);
        ctx.gpr[16] = record;
        call_set(rt, ctx, 0x089E4BC4u, {handle, record, 0u});
        rt.memory().store8(object + 0x118u, 1u);
        put(rt, frame.at(0x30u), record);
        put(rt, object + 0x11Cu, word(rt, handle_address));
    }
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x150u), {object});
    const s32 last = count - 1;
    ctx.gpr[30] = static_cast<u32>(last);
    if (last > 0) {
        s32 done = 0;
        ctx.gpr[23] = 0u;
        ctx.gpr[20] = 0xFFFFFFFFu;
        do {
            delay_loop(ctx);
            u32 source = points;
            u32 target = frame.at(0u);
            for (u32 pair = 0u; pair != 2u; ++pair) {
                call_set(rt, ctx, 0x08B4AC54u, {target, source, word(rt, frame.at(0x34u))});
                target += 8u;
                source += 8u;
            }
            ctx.fpr[12] = scale;
            done += 2;
            ctx.gpr[23] = static_cast<u32>(done);
            call_set(rt, ctx, 0x089A9E58u, {object, frame.at(0u), word(rt, frame.at(0x30u))});
            points += 0x10u;
            ctx.gpr[19] = points;
        } while (done < last);
    }
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x154u), {object});
}

// 0x088EDD5C: queues the request (a1 = the first word, a2 = the 28-byte payload) in the four-slot ring of the object a0 (slots of 0x28 bytes from +4, the
// next index in +0x4F0, the lock word at +0x4E8): it waits for the lock to be free (0 -> 3; while it is taken a state word of 4 at +0x4E8 gives up with
// v0 = 4, else 0x08B2E2B8(500) sleeps), claims the next slot index (counter + 1 modulo 4) and, when the slot's state word (+0x24) is clear, stores the word,
// the payload, 0x3C and the state 1 into it before the lock goes back to 0 (when it still held 3). v0 = 1 on a queued or aborted release, else the busy state.
void queue_ring_request(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 payload = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x18u}});
    const u32 lock = self + 0x4E8u;
    const u32 counter = self + 0x4F0u;
    ctx.gpr[21] = first;
    ctx.gpr[20] = payload;
    ctx.gpr[19] = 4u;
    ctx.gpr[18] = lock;
    ctx.gpr[17] = self;
    ctx.gpr[16] = 3u;
    for (;;) {
        // ll / sc: the lock word is taken (0 -> 3) when it is free
        const u32 state = word(rt, lock);
        ctx.gpr[3] = state;
        ctx.gpr[2] = 3u;
        if (state == 0u) {
            put(rt, lock, 3u);
            ctx.gpr[2] = 1u;
        }
        ctx.gpr[6] = counter;
        if (state == 0u) break;
        ctx.gpr[2] = word(rt, lock);
        ctx.gpr[4] = 0x1F4u;
        if (ctx.gpr[2] == 4u) return;
        call_set(rt, ctx, 0x08B2E2B8u, {0x1F4u});
    }
    u32 index;
    u32 current;
    do {
        index = word(rt, counter);
        const u32 next = static_cast<u32>(static_cast<s32>(index + 1u) % 4);
        current = word(rt, counter);
        ctx.gpr[2] = next;
        if (index == current) {
            put(rt, counter, next);
            ctx.gpr[2] = 1u;
        }
    } while (current != index);
    const u32 slot = self + index * 0x28u;
    const u32 entry = slot + 4u;
    ctx.gpr[9] = entry;
    ctx.gpr[2] = word(rt, entry + 0x24u);
    if (ctx.gpr[2] != 0u) return;
    put(rt, entry, first);
    put(rt, entry + 4u, word(rt, payload));
    put(rt, entry + 8u, word(rt, payload + 4u));
    put(rt, entry + 0xCu, word(rt, payload + 8u));
    put(rt, entry + 0x10u, word(rt, payload + 0xCu));
    put(rt, entry + 0x14u, word(rt, payload + 0x10u));
    put(rt, entry + 0x18u, word(rt, payload + 0x14u));
    put(rt, entry + 0x1Cu, word(rt, payload + 0x18u));
    put(rt, entry + 0x20u, 0x3Cu);
    put(rt, entry + 0x24u, 1u);
    for (;;) {
        ctx.gpr[2] = word(rt, lock) ^ 3u;
        ctx.gpr[2] = 0u < ctx.gpr[2] ? 1u : 0u;
        if (ctx.gpr[2] != 0u) return;
        put(rt, lock, ctx.gpr[2]);
        ctx.gpr[2] = 1u;
        break;
    }
}

// 0x088BD8F8 (a0 = the record, a1 = the source, a2 = the address of the table pointer, a3 = the scene, f12 = the ceiling; result in f0): the ceiling itself
// when the record's first byte is clear; otherwise the point value x * w + y clamped between the one-time constant at 0x08CE30A8 (5.5555553, set under the
// guard 0x08CE3050) and the ceiling, where w comes from 0x088BB664 and x is the source's float +8 (scaled by 0x089468C0 of the table's float +0x4B0 when
// the flag byte at the table + the record's signed byte +0x12 + 0x1318 is clear) and y the float +0x10 plus the record's signed byte +2 * 0.2777778 (not
// below -126). v0 = 0 or 0x08CE0000.
void clamp_weighted_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 table_address = ctx.gpr[6];
    const u32 scene = ctx.gpr[7];
    const float ceiling = ctx.fpr[12];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {55u, 0x2Cu}, {31u, 0x10u}, {54u, 0x28u}, {53u, 0x24u}, {52u, 0x20u}});
    ctx.gpr[19] = scene;
    ctx.gpr[18] = table_address;
    ctx.gpr[17] = record;
    ctx.gpr[16] = source;
    ctx.fpr[23] = ceiling;
    ctx.gpr[2] = rt.memory().load8(record);
    if (ctx.gpr[2] == 0u) {
        ctx.fpr[20] = ceiling;
        ctx.fpr[0] = ceiling;
        return;
    }
    ctx.gpr[8] = 0u;
    call_set(rt, ctx, 0x088BD6BCu, {record, source, table_address, scene, 0u});
    ctx.fpr[12] = ctx.fpr[0];
    call_set(rt, ctx, 0x088BB664u, {record, source});
    const float weight = ctx.fpr[0];
    ctx.fpr[22] = weight;
    const s32 angle = static_cast<s32>(signed_byte(rt, record + 2u));
    ctx.gpr[3] = static_cast<u32>(angle);
    float y = float_at(rt, source + 0x10u);
    float x = float_at(rt, source + 8u);
    ctx.fpr[21] = y;
    ctx.fpr[20] = x;
    if (!(angle < -126)) y = y + psp_mul(static_cast<float>(angle), std::bit_cast<float>(0x3E8E38E4u));
    ctx.fpr[21] = y;
    const u32 table = word(rt, table_address);
    ctx.gpr[6] = table;
    const u32 flag_address = signed_byte(rt, record + 0x12u) + table + 0x1318u;
    ctx.gpr[2] = 0x08CE0000u;
    if (rt.memory().load8(flag_address) == 0u) {
        ctx.fpr[12] = float_at(rt, table + 0x4B0u);
        call_set(rt, ctx, 0x089468C0u, {scene + 0x160u});
        x = psp_mul(x, ctx.fpr[0]);
        ctx.gpr[2] = 0x08CE0000u;
    }
    ctx.fpr[20] = x;
    float value = y + psp_mul(x, weight);
    ctx.fpr[20] = value;
    constexpr u32 kGuard = 0x08CE3050u;
    constexpr u32 kFloor = 0x08CE30A8u;
    ctx.gpr[3] = signed_byte(rt, kGuard);
    ctx.gpr[16] = kGuard;
    if (ctx.gpr[3] == 0u) {
        const u32 first_time = call_set(rt, ctx, 0x08B6441Cu, {kGuard});
        ctx.gpr[2] = 0x08CE0000u;
        if (first_time != 0u) {
            put_float(rt, kFloor, std::bit_cast<float>(0x40B1C71Du));
            call_set(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    ctx.gpr[2] = 0x08CE0000u;
    if (ceiling < value) {
        value = ceiling;
    } else {
        const float floor_value = float_at(rt, kFloor);
        if (value < floor_value) value = floor_value;
    }
    ctx.fpr[20] = value;
    ctx.fpr[0] = value;
}

// 0x08ABE8B8: with a0 | a1 = 0 tail-jumps to 0x08B7C830(a2, 0x08C906BC, a3); otherwise 0x08B1B740 gives a pair of words (v0, v1) kept at sp + 0x10 / +0x14
// (and sp + 0x20 / +0x24); its bytes (v1: +0x14..+0x16, v0: +0x12, +0x13 and the signed low halfword) become the arguments of 0x08B3767C(a2, a3, 0x08CB46C4,
// ...) with a ninth on the stack, in an order that depends on 0x08B49688(*0x08D69524, 0x08D70000): a zero answer passes (half, b12, b13, b14, b15), a
// non-zero one swaps the halfword with b13 (half <-> b13 in the a3 and t1 positions).
void forward_unpacked_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6];
    const u32 fourth = ctx.gpr[7];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x60u);
        SavedRegisters saved(rt, ctx, frame, {{17u, 0x34u}, {16u, 0x30u}, {31u, 0x50u}, {23u, 0x4Cu}, {22u, 0x48u}, {21u, 0x44u}, {20u, 0x40u}, {19u, 0x3Cu}, {18u, 0x38u}});
        ctx.gpr[2] = first | second;
        ctx.gpr[17] = third;
        ctx.gpr[6] = fourth;
        ctx.gpr[16] = fourth;
        if ((first | second) == 0u) {
            ctx.gpr[4] = third;
            ctx.gpr[5] = 0x08C906BCu;
            tail = true;
        } else {
            call_set(rt, ctx, 0x08B1B740u, {});
            const u32 low = ctx.gpr[2];
            const u32 high = ctx.gpr[3];
            put(rt, frame.at(0x10u), low);
            put(rt, frame.at(0x14u), high);
            put(rt, frame.at(0x20u), low);
            put(rt, frame.at(0x24u), high);
            const u32 b16 = rt.memory().load8(frame.at(0x16u));
            const u32 b15 = rt.memory().load8(frame.at(0x15u));
            const u32 b14 = rt.memory().load8(frame.at(0x14u));
            const u32 b13 = rt.memory().load8(frame.at(0x13u));
            const u32 b12 = rt.memory().load8(frame.at(0x12u));
            const u32 half = signed_half(rt, frame.at(0x10u));
            ctx.gpr[22] = b15;
            ctx.gpr[21] = b14;
            ctx.gpr[20] = b13;
            ctx.gpr[19] = b12;
            ctx.gpr[18] = half;
            ctx.gpr[23] = b16;
            ctx.gpr[6] = low;
            ctx.gpr[7] = high;
            const u32 selector = call_set(rt, ctx, 0x08B49688u, {word(rt, 0x08D69524u), 0x08D70000u});
            put(rt, frame.at(0u), b16);
            if (selector == 0u) {
                call_set(rt, ctx, 0x08B3767Cu, {third, fourth, 0x08CB46C4u, half, b12, b13, b14, b15});
            } else {
                call_set(rt, ctx, 0x08B3767Cu, {third, fourth, 0x08CB46C4u, b13, b12, half, b14, b15});
            }
        }
    }
    if (tail) jump(ctx, 0x08B7C830u);
}

// 0x08B349C4: releases one hold of the lock object a0 for the current thread (0x08B2E6DC) under the global lock (0x08C7A888 / 0x08C7A8A8): the nest count
// +0x20 must be non-zero and the owner (+0x18) the current thread, else v0 = 1 straight away; the count drops and at zero the owner and the waiter lists
// (+0x10, +0x14, else the list at +0 / +4) are cleared; with a1 set a pending request count (+0x24) equal to the new count hands over its word (+0x1C).
// Then each waiter (a linked list, next at +0, thread at +4) other than the current thread is woken (0x08C7A990), the first
// handle (0x08B2E0B0) and the handed word are released (0x08B2E0A4). v0 = 0 when the current thread was among the waiters, else 1.
void release_owned_lock(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 handover = ctx.gpr[5] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x18u}, {20u, 0x10u}, {18u, 0x8u}, {16u, 0x0u}, {21u, 0x14u}, {19u, 0xCu}, {17u, 0x4u}});
    ctx.gpr[18] = handover;
    ctx.gpr[16] = self;
    const u32 thread = call_set(rt, ctx, 0x08B2E6DCu, {});
    ctx.gpr[20] = thread;
    const u32 lock = call_set(rt, ctx, 0x08C7A888u, {});
    ctx.gpr[4] = lock;
    u32 count = word(rt, self + 0x20u);
    u32 waiters = 0u;
    u32 released = 0xFFFFFFFFu;
    u32 failed = 0u;
    if (count == 0u || thread != word(rt, self + 0x18u)) {
        failed = 1u;
    } else {
        count -= 1u;
        put(rt, self + 0x20u, count);
        if (count == 0u) {
            waiters = word(rt, self + 0x10u);
            put(rt, self + 0x18u, 0xFFFFFFFFu);
            put(rt, self + 0x10u, 0u);
            put(rt, self + 0x14u, 0u);
            if (waiters == 0u) {
                waiters = word(rt, self);
                put(rt, self + 4u, 0u);
                put(rt, self, 0u);
            }
        }
        if (handover != 0u && count == word(rt, self + 0x24u)) {
            released = word(rt, self + 0x1Cu);
            put(rt, self + 0x24u, 0xFFFFFFFFu);
            put(rt, self + 0x1Cu, 0xFFFFFFFFu);
        }
    }
    ctx.gpr[19] = released;
    ctx.gpr[18] = failed;
    ctx.gpr[17] = waiters;
    call_set(rt, ctx, 0x08C7A8A8u, {lock});
    ctx.gpr[3] = 1u;
    if (failed != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    u32 result = 1u;
    ctx.gpr[18] = 1u;
    if (waiters != 0u) {
        const u32 first_handle = call_set(rt, ctx, 0x08B2E0B0u, {});
        ctx.gpr[21] = first_handle;
        for (;;) {
            const u32 owner_thread = word(rt, waiters + 4u);
            const u32 next = word(rt, waiters);
            if (thread == owner_thread) {
                result = 0u;
                ctx.gpr[18] = 0u;
            } else {
                call_set(rt, ctx, 0x08C7A990u, {owner_thread});
            }
            waiters = next;
            ctx.gpr[17] = waiters;
            if (next == 0u) break;
        }
        ctx.gpr[3] = result;
        if (released == 0xFFFFFFFFu) {
            released = first_handle;
            ctx.gpr[19] = released;
        }
    }
    if (released != 0xFFFFFFFFu) {
        call_set(rt, ctx, 0x08B2E0A4u, {released});
        ctx.gpr[3] = result;
    }
    ctx.gpr[2] = ctx.gpr[3];
}

// 0x088D6C00: for the entry a1 of the object a0 (when 0x088DAA8C's count is below 0x088D9F80's): a negative word at +0x5C of the 8-byte pair at
// a0 + a1 * 8 + 0x58 is first reset to (0, 0); with a word at +0x640 it tail-jumps to 0x088E469C(+0x640, a1); otherwise a positive or a non-zero
// pair is returned (v0 = that word) and an empty one becomes (-1, -1) and searches the first free signed byte (-1) of the table at +0x130 (+0x50) over
// the count: 0x088D9C70 stores its pair there, 0x088E7EBC(the 0x2F8-byte record a1 of +0xC0, slot) is called and the index is written into the slot.
// v0 = the slot's address, 0 without one, or the count when the check fails.
void claim_free_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x14u}, {18u, 0x8u}, {16u, 0x0u}, {20u, 0x10u}, {19u, 0xCu}});
        ctx.gpr[17] = self;
        ctx.gpr[18] = index;
        const s32 limit = static_cast<s32>(call_set(rt, ctx, 0x088D9F80u, {self}));
        ctx.gpr[16] = static_cast<u32>(limit);
        const s32 count = static_cast<s32>(call_set(rt, ctx, 0x088DAA8Cu, {self}));
        ctx.gpr[16] = count < limit ? 1u : 0u;
        ctx.gpr[19] = index << 3;
        if (!(count < limit)) return;
        const u32 pair = ctx.gpr[19] + self;
        ctx.gpr[5] = pair;
        ctx.gpr[2] = word(rt, pair + 0x5Cu);
        if (static_cast<s32>(ctx.gpr[2]) < 0) {
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = 0u;
            put(rt, pair + 0x58u, 0u);
            put(rt, pair + 0x5Cu, 0u);
        }
        ctx.gpr[4] = word(rt, self + 0x640u);
        if (ctx.gpr[4] != 0u) {
            ctx.gpr[5] = index;
            tail = true;
        } else {
            ctx.gpr[2] = word(rt, pair + 0x5Cu);
            if (static_cast<s32>(ctx.gpr[2]) > 0) return;
            if (ctx.gpr[2] == 0u) {
                ctx.gpr[2] = word(rt, pair + 0x58u);
                if (ctx.gpr[2] != 0u) return;
            }
            put(rt, pair + 0x58u, 0xFFFFFFFFu);
            put(rt, pair + 0x5Cu, 0xFFFFFFFFu);
            ctx.gpr[3] = 0xFFFFFFFFu;
            ctx.gpr[20] = 0xFFFFFFFFu;
            u32 slot = 0u;
            ctx.gpr[16] = slot;
            for (;;) {
                const s32 slots = static_cast<s32>(call_set(rt, ctx, 0x088DAA8Cu, {self}));
                ctx.gpr[2] = static_cast<s32>(slot) < slots ? 1u : 0u;
                if (!(static_cast<s32>(slot) < slots)) return;
                const u32 slot_address = slot + word(rt, self + 0x130u);
                ctx.gpr[2] = slot_address;
                ctx.gpr[3] = signed_byte(rt, slot_address + 0x50u);
                if (ctx.gpr[3] == 0xFFFFFFFFu) break;
                ++slot;
                ctx.gpr[16] = slot;
            }
            call_set(rt, ctx, 0x088D9C70u, {self});
            const u32 low = ctx.gpr[2];
            const u32 high = ctx.gpr[3];
            const u32 record = word(rt, self + 0xC0u) + ((index * 0x5Fu) << 3);
            put(rt, pair + 0x58u, low);
            put(rt, pair + 0x5Cu, high);
            call_set(rt, ctx, 0x088E7EBCu, {record, slot});
            const u32 slot_address = slot + word(rt, self + 0x130u);
            ctx.gpr[2] = slot_address;
            rt.memory().store8(slot_address + 0x50u, static_cast<std::uint8_t>(index));
        }
    }
    if (tail) jump(ctx, 0x088E469Cu);
}

// The 0x08B6441C / 0x08B64494 pair around a one-time static: when the guard byte is clear and 0x08B6441C(guard) says to, `initialise` runs and
// 0x08B64494(guard) closes it.
template <typename Initialise>
void run_once(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 guard, Initialise initialise) {
    if (signed_byte(rt, guard) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    initialise();
    call_set(rt, ctx, 0x08B64494u, {guard});
}

// The statics of the three 0x08BA... / 0x08B9A72C functions below: a guarded record 0x08D752A8 (words 0, 0x08D752B4, 0x08CBF670, guard 0x08D750F0) linked
// into `chain_slot` under the guard `outer_guard`, and a second record (words 0, `link`, `vtable`) under `inner_guard`.
struct StaticChain {
    u32 outer_guard;
    u32 chain_slot;
    u32 inner_guard;
    u32 record;
    u32 vtable;
    u32 tail;
};

// 0x08BA4A78 / 0x08BA415C / 0x08B9A72C: a null object returns 0; otherwise the object's virtual +8 gives a value, the one-time statics of `chain` are
// built, and 0x...(object, value, the inner record, 0) is tail-jumped to.
void initialise_statics_and_forward(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const StaticChain &chain) {
    const u32 object = ctx.gpr[4];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
        ctx.gpr[16] = object;
        if (object == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        const u32 value = call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
        ctx.gpr[18] = value;
        run_once(rt, ctx, chain.outer_guard, [&] {
            run_once(rt, ctx, 0x08D750F0u, [&] {
                put(rt, 0x08D752A8u, 0u);
                put(rt, 0x08D752ACu, 0x08D752B4u);
                put(rt, 0x08D752B0u, 0x08CBF670u);
            });
            put(rt, chain.chain_slot, 0x08D752A8u);
        });
        run_once(rt, ctx, chain.inner_guard, [&] {
            put(rt, chain.record, 0u);
            put(rt, chain.record + 4u, chain.chain_slot);
            put(rt, chain.record + 8u, chain.vtable);
        });
        ctx.gpr[4] = object;
        ctx.gpr[5] = value;
        ctx.gpr[6] = chain.record;
        ctx.gpr[7] = 0u;
        tail = true;
    }
    if (tail) jump(ctx, chain.tail);
}

// 0x08B804B0: makes the stdio-like stream a0 ready for writing: its flags (halfword +0xC) need bit 8 (write mode) or bit 0x10 (else v0 = -1); a stream in
// read mode with flag 4 gives back its pushed-back buffer (+0x30: 0x08B354A4 unless it is the inline one at +0x40), clears flags 0x24 and resets the
// cursor (+0) to the buffer (+0x10) with the count (+4) 0, then sets flag 8; a missing buffer is allocated (0x08B797F0); the space left (+8) is the size
// (+0x14), 0 for a line-buffered stream (flag 2), and with flag 1 (unbuffered) 0 and +0x18 = -size. v0 = 0.
void prepare_stream_for_writing(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    ctx.gpr[16] = stream;
    const u32 context = call_set(rt, ctx, 0x08B796C4u, {});
    if (word(rt, context + 0x38u) == 0u) {
        const u32 again = call_set(rt, ctx, 0x08B796C4u, {});
        call_set(rt, ctx, 0x08B78430u, {again});
    }
    u32 flags = rt.memory().load16(stream + 0xCu);
    if ((flags & 8u) == 0u) {
        ctx.gpr[5] = 0xFFFFFFFFu;
        if ((flags & 0x10u) == 0u) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        if ((flags & 4u) == 0u) {
            flags |= 8u;
            rt.memory().store16(stream + 0xCu, static_cast<std::uint16_t>(flags));
        } else {
            const u32 pushed_back = word(rt, stream + 0x30u);
            if (pushed_back != 0u) {
                if (pushed_back == stream + 0x40u) {
                    put(rt, stream + 0x30u, 0u);
                } else {
                    const u32 owner = call_set(rt, ctx, 0x08B796C4u, {});
                    call_set(rt, ctx, 0x08B354A4u, {owner, word(rt, stream + 0x30u)});
                    flags = rt.memory().load16(stream + 0xCu);
                    put(rt, stream + 0x30u, 0u);
                }
            }
            const u32 cleared = flags & 0xFFFFFFDBu;
            const u32 buffer = word(rt, stream + 0x10u);
            rt.memory().store16(stream + 0xCu, static_cast<std::uint16_t>(cleared));
            flags = cleared | 8u;
            put(rt, stream + 4u, 0u);
            put(rt, stream, buffer);
            rt.memory().store16(stream + 0xCu, static_cast<std::uint16_t>(flags));
        }
    }
    if (word(rt, stream + 0x10u) == 0u) {
        call_set(rt, ctx, 0x08B797F0u, {stream});
        flags = rt.memory().load16(stream + 0xCu);
    }
    ctx.gpr[5] = 0u;
    if ((flags & 1u) != 0u) {
        put(rt, stream + 8u, 0u);
        put(rt, stream + 0x18u, 0u - word(rt, stream + 0x14u));
    } else {
        put(rt, stream + 8u, (flags & 2u) != 0u ? 0u : word(rt, stream + 0x14u));
    }
    ctx.gpr[2] = 0u;
}

// 0x088D876C: releases the shared-pointer style holder of the object a0 (the handle at +0x14 must pass 0x088FE2A4 / 0x088FE2AC, which give their non-zero
// answer back): a1 is stored at +0x650, the holder at +0x6B8 and the word +0x6B4 are cleared and the holder's two counts (+4, +8) are decremented, calling its
// virtual +8 when the first and +0xC when the second reaches zero; 0x088FEB9C(handle, a0 + 0x6A4) runs and, without a callback id at +0x6C8, a
// callback record (0x088D8764, a0) is registered (0x08B2E314 / 0x08B2E134 / 0x08B1B96C) into +0x6C8; the pair at +0x700 / +0x704 is cleared. v0 = 0.
void release_binding(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = argument;
    ctx.gpr[16] = self;
    const u32 first_answer = call_set(rt, ctx, 0x088FE2A4u, {word(rt, self + 0x14u)});
    if (first_answer != 0u) return;
    const u32 second_answer = call_set(rt, ctx, 0x088FE2ACu, {word(rt, self + 0x14u)});
    if (second_answer != 0u) return;
    const u32 holder = word(rt, self + 0x6B8u);
    ctx.gpr[18] = holder;
    put(rt, self + 0x650u, argument);
    put(rt, self + 0x6B4u, 0u);
    put(rt, self + 0x6B8u, 0u);
    if (holder != 0u) {
        // ll / sc: each count is decremented in place and the old value tells whether it reached zero
        u32 count = word(rt, holder + 4u);
        put(rt, holder + 4u, count - 1u);
        ctx.gpr[3] = holder + 4u;
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
            ctx.gpr[3] = holder + 8u;
            count = word(rt, holder + 8u);
            put(rt, holder + 8u, count - 1u);
            if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
        }
    }
    ctx.gpr[4] = word(rt, self + 0x14u);
    call_set(rt, ctx, 0x088FEB9Cu, {word(rt, self + 0x14u), self + 0x6A4u});
    ctx.gpr[2] = word(rt, self + 0x6C8u);
    if (ctx.gpr[2] == 0u) {
        rt.memory().store8(self + 0x6F8u, 0u);
        const u32 thread = call_set(rt, ctx, 0x08B2E314u, {2u, 0u});
        const u32 event = call_set(rt, ctx, 0x08B2E134u, {thread});
        put(rt, frame.at(0u), 0x088D8764u);
        put(rt, frame.at(4u), self);
        const u32 callback = call_set(rt, ctx, 0x08B1B96Cu, {frame.at(0u), event, 0u});
        put(rt, self + 0x6C8u, callback);
    }
    ctx.gpr[2] = 0u;
    ctx.gpr[3] = 0u;
    put(rt, self + 0x700u, 0u);
    put(rt, self + 0x704u, 0u);
}

// Allegrex `wsbh`: the bytes of each halfword swapped.
u32 wsbh(u32 value) { return ((value & 0x00FF00FFu) << 8) | ((value & 0xFF00FF00u) >> 8); }

// 0x088D7DFC: for an object a0 with a word at +0x640 (else v0 = 0): fills the record at sp (0x088D671C from a0 + 4, then 0x088D4988 from a0 + 0xC when
// a2 is non-zero, else from a1), and for a non-zero table a3 (halfwords, -1 ending it) the 0x088D9F80 entries of the buffer at sp + 8 are set to 0xFFFF and
// then entry (a3[i]) gets i with its bytes swapped (0x088D4D48 applies the buffer); the record is written (0x088D5028 with +0x130 + 0x5C), the
// two calls 0x088E52CC(*(a0 + 0x640), *(a0 + 0x130)) / 0x088E65E0(*(a0 + 0x640), a2, a3) run and 0x088D5CF0(sp) closes. v0 = 1.
void apply_slot_table(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 mode = ctx.gpr[6] & 0xFFu;
    const u32 table = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x28u}, {21u, 0x24u}, {18u, 0x18u}, {16u, 0x10u}, {31u, 0x2Cu}, {20u, 0x20u}, {19u, 0x1Cu}, {17u, 0x14u}});
    ctx.gpr[3] = 0u;
    ctx.gpr[22] = mode;
    ctx.gpr[21] = table;
    ctx.gpr[18] = self;
    ctx.gpr[2] = word(rt, self + 0x640u);
    ctx.gpr[16] = source;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x088D671Cu, {frame.at(0u), self + 4u});
    if (mode != 0u) call_set(rt, ctx, 0x088D4988u, {frame.at(0u), self + 0xCu});
    else call_set(rt, ctx, 0x088D4988u, {frame.at(0u), source});
    const u32 slots = frame.at(8u);
    ctx.gpr[20] = slots;
    if (table != 0u) {
        ctx.gpr[19] = wsbh(0xFFFFFFFFu);
        u32 cursor = slots;
        for (u32 index = 0u; static_cast<s32>(index) < static_cast<s32>(call_set(rt, ctx, 0x088D9F80u, {self})); ++index) {
            rt.memory().store16(cursor, 0xFFFFu);
            cursor += 2u;
            ctx.gpr[16] = index + 1u;
            ctx.gpr[17] = cursor;
        }
        u32 entries = table;
        u32 index = 0u;
        for (;;) {
            const s32 count = static_cast<s32>(call_set(rt, ctx, 0x088D9F80u, {self}));
            const u32 swapped = wsbh(index);
            ctx.gpr[4] = swapped;
            const bool more = static_cast<s32>(index) < count;
            ++index;
            ctx.gpr[17] = index;
            if (!more) break;
            const u32 slot = signed_half(rt, entries);
            entries += 2u;
            ctx.gpr[16] = entries;
            if (slot == 0xFFFFFFFFu) break;
            rt.memory().store16(slots + slot * 2u, static_cast<std::uint16_t>(swapped));
        }
        call_set(rt, ctx, 0x088D4D48u, {frame.at(0u), slots});
    }
    call_set(rt, ctx, 0x088D5028u, {frame.at(0u), word(rt, self + 0x130u) + 0x5Cu});
    call_set(rt, ctx, 0x088E52CCu, {word(rt, self + 0x640u), word(rt, self + 0x130u)});
    call_set(rt, ctx, 0x088E65E0u, {word(rt, self + 0x640u), mode, table});
    call_set(rt, ctx, 0x088D5CF0u, {frame.at(0u)});
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

// ll / sc decrement of the word at `address` (nothing else runs here, so the store always succeeds and v0 is 1 afterwards): true when it was 1.
bool decrement_reaches_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    return old == 1u;
}

// The same increment.
void increment(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[2] = 1u;
}

// Drops one use of a shared holder (use count +4, weak count +8): the last use calls the virtual +8, the last weak count the virtual +0xC.
void release_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 holder) {
    if (!decrement_reaches_zero(rt, ctx, holder + 4u)) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
    if (!decrement_reaches_zero(rt, ctx, holder + 8u)) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
}

// 0x08A060F4: for the 0x60-byte elements of the array at *(a0 + 0xA8) (state word +8, a payload at +0x40 copied with 0x089641C4(target, a2) plus the word
// +0x10): the payload of element a1 takes a2; then, when that element's state is 1, the previous element's when its state is 0, and, when the
// element itself ends at state 0, the next one's when its state is 1. v0 follows the asm (the last call's, or constants of the tests).
void propagate_payload(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 index = static_cast<s32>(ctx.gpr[5]);
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x18u}});
    const u32 table = self + 0xA8u;
    const u32 stride = static_cast<u32>(index) * 0x60u;
    ctx.gpr[20] = static_cast<u32>(index);
    ctx.gpr[19] = source;
    ctx.gpr[17] = self;
    ctx.gpr[18] = stride;
    ctx.gpr[21] = table;
    ctx.gpr[2] = static_cast<u32>(index) << 5;
    const auto copy_payload = [&](u32 target) {
        call_set(rt, ctx, 0x089641C4u, {target, source});
        put(rt, target + 0x10u, word(rt, source + 0x10u));
    };
    u32 base = word(rt, table);
    u32 payload = base + stride + 0x40u;
    ctx.gpr[16] = payload;
    if (source != payload) {
        copy_payload(payload);
        base = word(rt, table);
    }
    ctx.gpr[2] = stride + base;
    if (word(rt, stride + base + 8u) == 1u) {
        const s32 previous = index - 1;
        ctx.gpr[2] = static_cast<u32>(previous) << 7;
        if (previous >= 0) {
            const u32 element = base + static_cast<u32>(previous) * 0x60u;
            ctx.gpr[2] = element;
            if (word(rt, element + 8u) == 0u) {
                payload = element + 0x40u;
                ctx.gpr[16] = payload;
                if (source != payload) copy_payload(payload);
            }
        }
    }
    base = word(rt, table);
    ctx.gpr[2] = stride + base;
    if (word(rt, stride + base + 8u) != 0u) {
        ctx.gpr[2] = 0xAAAA0000u;
        return;
    }
    const u32 count = static_cast<u32>(static_cast<s32>(word(rt, table + 4u) - base) >> 5) * 0xAAAAAAABu;
    const s32 next = index + 1;
    ctx.gpr[2] = 0xAAAAAAABu;
    if (!(next < static_cast<s32>(count))) return;
    const u32 next_element = base + static_cast<u32>(next) * 0x60u;
    ctx.gpr[2] = 1u;
    if (word(rt, next_element + 8u) != 1u) return;
    payload = next_element + 0x40u;
    ctx.gpr[16] = payload;
    if (source == payload) return;
    copy_payload(payload);
}

// 0x08BAF6D4: assigns the pair (value, holder) at a1 to the object a0 (value +0x28, shared holder +0x2C, which counts its uses at +4 and weak users at +8):
// a null a1 clears both and releases the old holder, the last weak count tail-jumping to the holder's virtual +0xC; otherwise the new holder
// gains a use, the old one is released (virtual +8 / +0xC as in release_holder) and the new one is stored. v0 follows the asm.
void assign_shared_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 pair = ctx.gpr[5];
    u32 tail = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
        if (pair == 0u) {
            const u32 holder = word(rt, self + 0x2Cu);
            ctx.gpr[16] = holder;
            put(rt, self + 0x28u, 0u);
            put(rt, self + 0x2Cu, 0u);
            if (holder == 0u) return;
            if (!decrement_reaches_zero(rt, ctx, holder + 4u)) return;
            call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
            if (!decrement_reaches_zero(rt, ctx, holder + 8u)) return;
            ctx.gpr[2] = word(rt, holder);
            ctx.gpr[4] = holder;
            tail = word(rt, ctx.gpr[2] + 0xCu);
            ctx.gpr[25] = tail;
        } else {
            const u32 value = word(rt, pair);
            ctx.gpr[2] = value;
            const u32 old_holder = word(rt, self + 0x2Cu);
            put(rt, self + 0x28u, value);
            const u32 new_holder = word(rt, pair + 4u);
            ctx.gpr[17] = new_holder;
            ctx.gpr[18] = self + 0x2Cu;
            if (new_holder == old_holder) return;
            if (new_holder != 0u) increment(rt, ctx, new_holder + 4u);
            const u32 current = word(rt, self + 0x2Cu);
            ctx.gpr[16] = current;
            if (current != 0u) {
                if (!decrement_reaches_zero(rt, ctx, current + 4u)) {
                    put(rt, self + 0x2Cu, new_holder);
                    return;
                }
                call_indirect(rt, ctx, word(rt, word(rt, current) + 8u), {current});
                if (!decrement_reaches_zero(rt, ctx, current + 8u)) {
                    put(rt, self + 0x2Cu, new_holder);
                    return;
                }
                call_indirect(rt, ctx, word(rt, word(rt, current) + 0xCu), {current});
            }
            put(rt, self + 0x2Cu, new_holder);
        }
    }
    if (tail != 0u) jump(ctx, tail);
}

// 0x08BD3FC4: destructor (a0 = the object): the vtable 0x08CC5530 is set and the holder at +0x84 released, the vtable 0x08CC5550 and the holder at +0x2C
// released, then the holder at +0x24 loses a weak count (the last one calls its virtual +0xC) and the final vtable 0x08CC3000 is stored. v0 = 0x08CC3000.
void destroy_with_holders(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[17] = self;
    ctx.gpr[2] = 0x08CC5530u;
    const u32 first = word(rt, self + 0x84u);
    ctx.gpr[16] = first;
    put(rt, self, 0x08CC5530u);
    if (first != 0u) release_holder(rt, ctx, first);
    const u32 second = word(rt, self + 0x2Cu);
    ctx.gpr[16] = second;
    ctx.gpr[2] = 0x08CC5550u;
    put(rt, self, 0x08CC5550u);
    if (second != 0u) release_holder(rt, ctx, second);
    const u32 third = word(rt, self + 0x24u);
    ctx.gpr[4] = third;
    if (third != 0u) {
        if (decrement_reaches_zero(rt, ctx, third + 8u)) call_indirect(rt, ctx, word(rt, word(rt, third) + 0xCu), {third});
    }
    ctx.gpr[2] = 0x08CC3000u;
    put(rt, self, 0x08CC3000u);
}

// 0x08ADD2C4: after 0x08ADD2AC runs, searches the 32-byte entries of the array at *(a1 + 0x70) (count +0x74, one pass only) for those whose word
// +0x10 is 0 and whose name (the word +0xC rebased by *(a1 + 0x14), or 0x08C90E14 when empty) 0x08B35FD8 finds equal to a2: each match bumps the
// counter at sp and fills the record a0 from the entry (words +0x14 -> +0, +0x1C -> +4, +0x20 -> +8 and the rebased +0x20 / +0x24 -> +0xC / +0x10,
// zero when the offset is zero). v0 = the counter (0 without the array).
void find_entries_by_name(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 list = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {21u, 0x24u}, {20u, 0x20u}, {31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[18] = list;
    ctx.gpr[21] = out;
    ctx.gpr[20] = key;
    call_set(rt, ctx, 0x08ADD2ACu, {out, list, key});
    ctx.gpr[2] = word(rt, list + 0x70u);
    ctx.gpr[3] = 0u;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    s32 count = static_cast<s32>(word(rt, list + 0x74u));
    ctx.gpr[6] = static_cast<u32>(count);
    put(rt, frame.at(0u), 0u);
    s32 pass = 0;
    u32 pass_offset = 0u;
    ctx.gpr[19] = 0u;
    ctx.gpr[23] = 0u;
    do {
        ctx.gpr[17] = 0u;
        if (count > 0) {
            ctx.gpr[22] = 0x08C90000u;
            ctx.gpr[30] = pass_offset;
            for (s32 index = 0; index < count;) {
                ctx.gpr[17] = static_cast<u32>(index);
                const u32 entry = word(rt, list + 0x70u) + (static_cast<u32>(index) << 5) + 8u;
                ctx.gpr[16] = entry;
                if (static_cast<s32>(word(rt, entry + 8u)) == pass) {
                    const u32 name_offset = word(rt, entry + 4u);
                    const u32 name = name_offset != 0u ? word(rt, list + 0x14u) + name_offset : 0x08C90E14u;
                    ctx.gpr[4] = name;
                    if (call_set(rt, ctx, 0x08B35FD8u, {name, key}) == 0u && pass == 0) {
                        const u32 record = pass_offset + out;
                        ctx.gpr[5] = record;
                        put(rt, frame.at(0u), word(rt, frame.at(0u)) + 1u);
                        put(rt, out, word(rt, list + 0x14u));
                        put(rt, out + 4u, word(rt, entry + 0xCu));
                        put(rt, out + 8u, word(rt, entry + 0x14u));
                        const u32 first_offset = word(rt, entry + 0x18u);
                        put(rt, out + 0xCu, first_offset == 0u ? 0u : word(rt, list + 0x14u) + first_offset);
                        const u32 second_offset = word(rt, entry + 0x1Cu);
                        put(rt, record + 0x10u, second_offset == 0u ? 0u : word(rt, list + 0x14u) + second_offset);
                    }
                    count = static_cast<s32>(word(rt, list + 0x74u));
                    ctx.gpr[6] = static_cast<u32>(count);
                }
                ++index;
            }
        }
        ++pass;
        ctx.gpr[19] = static_cast<u32>(pass);
        pass_offset += 0x14u;
        ctx.gpr[23] = pass_offset;
    } while (pass <= 0);
    ctx.gpr[3] = word(rt, frame.at(0u));
    ctx.gpr[2] = ctx.gpr[3];
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08B136A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x100u); put(rt, frame.at(0xF0u), G[16]); put(rt, frame.at(0xFCu), G[31]); put(rt, frame.at(0xF8u), G[19]); put(rt, frame.at(0xF4u), G[18]); G[2] = frame.at(0x64u); G[16] = G[4]; G[4] = frame.at(0x58u); put(rt, frame.at(0x58u), G[2]); G[2] = 0x80u; G[19] = G[7]; G[18] = G[6]; put(rt, frame.at(0x5Cu), G[2]); put(rt, frame.at(0x60u), 0u); store8(frame.at(0x64u), 0u); call_here(rt, ctx, 0x08B1262Cu); G[5] = 0x08CB0000u; G[4] = G[2]; G[5] = 0x08CB4BD4u; call_here(rt, ctx, 0x08B23A70u); G[4] = word(rt, frame.at(0x58u)); G[5] = frame.at(0u); call_here(rt, ctx, 0x08C7A848u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[3] = 0u; if (tk) goto L_08B137C4; } G[4] = G[16]; G[5] = 0u; G[6] = 0x40u; call_here(rt, ctx, 0x08B36018u); G[2] = word(rt, frame.at(0x8u)); G[4] = word(rt, frame.at(0x10u)); G[5] = word(rt, frame.at(0x14u)); G[6] = word(rt, frame.at(0x18u)); G[7] = word(rt, frame.at(0x1Cu)); G[8] = word(rt, frame.at(0x30u)); G[9] = word(rt, frame.at(0x34u)); G[10] = word(rt, frame.at(0x38u)); G[11] = word(rt, frame.at(0x3Cu)); G[3] = word(rt, frame.at(0xCu)); put(rt, G[16] + 0x20u, G[2]); G[2] = 0x08CD0000u; G[12] = G[16]; put(rt, G[16] + 0x0u, G[4]); G[13] = 0x08CD7EC8u; G[14] = G[16] + 0x3Cu; put(rt, G[16] + 0x4u, G[5]); G[4] = 0u; G[5] = 0x3Cu; put(rt, G[16] + 0x24u, G[3]); put(rt, G[16] + 0x8u, G[6]); put(rt, G[16] + 0xCu, G[7]); put(rt, G[16] + 0x10u, G[8]); put(rt, G[16] + 0x14u, G[9]); put(rt, G[16] + 0x18u, G[10]); put(rt, G[16] + 0x1Cu, G[11]); put(rt, G[16] + 0x28u, G[18]); put(rt, G[16] + 0x2Cu, G[19]); L_08B1378C: ; G[2] = load8(G[12] + 0x0u); G[3] = G[4] >> 24; G[5] = G[5] + 0xFFFFFFFFu; G[2] = G[2] ^ G[3]; G[2] = G[2] << 2; G[2] = G[2] + G[13]; G[3] = word(rt, G[2] + 0x0u); G[4] = G[4] << 8; G[12] = G[12] + 0x1u; { const bool tk = G[5] != 0u; G[4] = G[3] ^ G[4]; if (tk) goto L_08B1378C; } G[2] = ((G[4] & 0xFFu) << 24u) | ((G[4] & 0xFF00u) << 8u) | ((G[4] & 0xFF0000u) >> 8u) | ((G[4] & 0xFF000000u) >> 24u); put(rt, G[14] + 0x0u, G[2]); G[3] = 0x1u; L_08B137C4: ; G[2] = G[3]; goto exit_;  exit_: ; G[16] = word(rt, frame.at(0xF0u)); G[19] = word(rt, frame.at(0xF8u)); G[18] = word(rt, frame.at(0xF4u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BA7990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); G[2] = 0x08CC0000u; G[2] = 0x08CC1918u; G[17] = G[4] + 0x28u; G[18] = G[4]; { const bool tk = G[17] == 0u; put(rt, G[4] + 0x0u, G[2]); if (tk) goto L_08BA7A00; } G[16] = G[4] + 0x4Cu; if (G[16] == G[17]) { G[16] = word(rt, G[18] + 0x1Cu); goto L_08BA7A04; } G[19] = frame.at(0x1u); G[16] = G[16] + 0xFFFFFFF4u; L_08BA79D4: ; G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = G[19]; call_here(rt, ctx, 0x08BA76B8u); G[2] = word(rt, G[16] + 0x0u); { const bool tk = G[2] == 0u; G[4] = G[2]; if (tk) goto L_08BA79F8; } call_here(rt, ctx, 0x08B35254u); L_08BA79F8: ; { const bool tk = G[16] != G[17]; G[16] = G[16] + 0xFFFFFFF4u; if (tk) goto L_08BA79D4; } L_08BA7A00: ; G[16] = word(rt, G[18] + 0x1Cu); L_08BA7A04: ; G[2] = 0x08CC0000u; G[2] = 0x08CC18E0u; { const bool tk = G[16] == 0u; put(rt, G[18] + 0x0u, G[2]); if (tk) goto L_08BA7A74; } G[3] = G[16] + 0x4u; L_08BA7A18: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA7A18; } if (G[4] != G[2]) { G[4] = word(rt, G[18] + 0x8u); goto L_08BA7A78; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_08BA7A48: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BA7A48; } if (G[4] != G[2]) { G[4] = word(rt, G[18] + 0x8u); goto L_08BA7A78; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_08BA7A74: ; G[4] = word(rt, G[18] + 0x8u); L_08BA7A78: ; G[5] = word(rt, G[18] + 0xCu); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B8E5D8u); G[4] = word(rt, G[18] + 0x8u); { const bool tk = G[4] == 0u; G[2] = 0x08CC0000u; if (tk) goto L_08BA7A9C; } call_here(rt, ctx, 0x08B35254u); G[2] = 0x08CC0000u; L_08BA7A9C: ; G[2] = 0x08CBF518u; put(rt, G[18] + 0x0u, G[2]); G[4] = G[18]; call_here(rt, ctx, 0x08B64654u); goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BC207C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[18] = word(rt, G[4] + 0xCu); G[2] = word(rt, G[18] + 0x110u); { const bool tk = G[2] == 0u; G[20] = G[4] + 0x10u; if (tk) goto L_08BC20B4; } G[4] = G[18]; G[5] = 0u; call_here(rt, ctx, 0x08905A94u); L_08BC20B4: ; G[4] = word(rt, G[18] + 0x118u); G[5] = word(rt, G[18] + 0x11Cu); G[6] = frame.at(0u); call_here(rt, ctx, 0x08BC0750u); G[4] = word(rt, G[18] + 0x118u); if (G[4] == 0u) { G[16] = word(rt, G[18] + 0x114u); goto L_08BC20DC; } call_here(rt, ctx, 0x08B35254u); G[16] = word(rt, G[18] + 0x114u); L_08BC20DC: ; { const bool tk = G[16] == 0u; G[3] = G[16] + 0x4u; if (tk) goto L_08BC2140; } L_08BC20E4: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BC20E4; } if (G[4] != G[2]) { G[16] = word(rt, G[20] + 0x0u); goto L_08BC2144; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = G[16] + 0x8u; L_08BC2114: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08BC2114; } if (G[4] != G[2]) { G[16] = word(rt, G[20] + 0x0u); goto L_08BC2144; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } L_08BC2140: ; G[16] = word(rt, G[20] + 0x0u); L_08BC2144: ; G[19] = G[16] + 0x6Cu; G[4] = G[19]; G[17] = G[16] + 0x8u; call_here(rt, ctx, 0x08B34888u); G[5] = G[18]; G[4] = G[17]; call_here(rt, ctx, 0x08B2CE7Cu); G[4] = G[17]; call_here(rt, ctx, 0x08B2D1A8u); G[3] = word(rt, G[16] + 0x68u); { const bool tk = G[2] != G[3]; if (tk) goto L_08BC217C; } G[4] = G[19]; call_here(rt, ctx, 0x08B34CA0u); L_08BC217C: ; G[4] = G[19]; call_here(rt, ctx, 0x08B34B24u); G[4] = word(rt, G[20] + 0x0u); G[5] = G[16]; G[4] = G[4] + 0x98u; call_here(rt, ctx, 0x08BBEA18u); goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x088740A8 (a0 = the object; v0 = as the asm leaves it): for the records at word(object + 0xC) and word(object + 0x14), the slot at record + 0x10 + byte(record + 0x25) * 8
// is emptied (words +4 and +8 become 0) and the holder that was at +8 is released like a shared pointer: the use count (+4) drops, the last use calls the holder's virtual +8,
// then the weak count (+8) drops and the last one calls the virtual +0xC (for the second record that last call is a tail call after the frame is gone).
static u32 clear_slots(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 object) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    ctx.gpr[17] = object;
    u32 record = word(rt, object + 0xCu);
    u32 slot = record + (load8(record + 0x25u) << 3) + 0x10u;
    u32 holder = word(rt, slot + 8u);
    put(rt, slot + 4u, 0u);
    put(rt, slot + 8u, 0u);
    ctx.gpr[16] = holder;
    if (holder != 0u) hot_v2::release_holder(rt, ctx, holder, false);
    record = word(rt, object + 0x14u);
    ctx.gpr[3] = record;
    ctx.gpr[2] = (load8(record + 0x25u) << 3) + 0x10u;
    slot = record + (load8(record + 0x25u) << 3) + 0x10u;
    holder = word(rt, slot + 8u);
    put(rt, slot + 4u, 0u);
    put(rt, slot + 8u, 0u);
    ctx.gpr[16] = holder;
    if (holder == 0u) return 0u;
    return hot_v2::release_holder(rt, ctx, holder, true);
}

void func_088740A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = clear_slots(rt, ctx, ctx.gpr[4]);
    if (target != 0u) jump(ctx, target);
}
void func_088D876C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_binding(rt, ctx); }
void func_08B804B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { prepare_stream_for_writing(rt, ctx); }
void func_08C111CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[30] = G[4]; G[6] = word(rt, G[4] + 0x4u); G[4] = G[5] + 0x60u; { const bool tk = G[4] == G[6]; put(rt, frame.at(0x0u), G[5]); if (tk) goto L_08C112CC; } G[3] = G[6] - G[4]; G[2] = 0xAAAA0000u; G[3] = sra(G[3], 5); G[2] = G[2] | 0xAAABu; { const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32u); } G[23] = ctx.lo; { const bool tk = static_cast<std::int32_t>(G[23]) <= 0; G[17] = G[4]; if (tk) goto L_08C112CC; } G[16] = G[5]; G[22] = 0u; G[2] = word(rt, G[17] + 0x4u); L_08C11238: ; G[3] = word(rt, G[17] + 0x8u); F[0] = float_at(rt, G[17] + 0xCu); F[1] = float_at(rt, G[17] + 0x10u); F[2] = float_at(rt, G[17] + 0x14u); F[3] = float_at(rt, G[17] + 0x18u); F[4] = float_at(rt, G[17] + 0x1Cu); G[20] = G[16] + 0x20u; G[18] = G[17] + 0x20u; G[21] = G[16] + 0x40u; G[19] = G[17] + 0x40u; put(rt, G[16] + 0x4u, G[2]); G[22] = G[22] + 0x1u; G[4] = G[20]; put(rt, G[16] + 0x8u, G[3]); G[5] = G[18]; G[17] = G[17] + 0x60u; put_float(rt, G[16] + 0xCu, F[0]); put_float(rt, G[16] + 0x10u, F[1]); put_float(rt, G[16] + 0x14u, F[2]); put_float(rt, G[16] + 0x18u, F[3]); put_float(rt, G[16] + 0x1Cu, F[4]); { const bool tk = G[20] == G[18]; G[16] = G[16] + 0x60u; if (tk) goto L_08C112A4; } call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[18] + 0x10u); put(rt, G[20] + 0x10u, G[3]); L_08C112A4: ; G[4] = G[21]; { const bool tk = G[21] == G[19]; G[5] = G[19]; if (tk) goto L_08C112C0; } call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, G[19] + 0x10u); put(rt, G[21] + 0x10u, G[3]); L_08C112C0: ; if (G[23] != G[22]) { G[2] = word(rt, G[17] + 0x4u); goto L_08C11238; } G[6] = word(rt, G[30] + 0x4u); L_08C112CC: ; G[4] = G[6] + 0xFFFFFFA0u; put(rt, G[30] + 0x4u, G[4]); G[2] = word(rt, G[6] + 0xFFFFFFA0u); G[3] = word(rt, G[2] + 0x0u); { const std::uint32_t tg = G[3]; call_here(rt, ctx, tg); } G[2] = word(rt, frame.at(0x0u)); goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A05FA4 (a0 = the object, a1 = an index, a2 = the source sub-object; v0 = as the asm leaves it): the object's vector of 96-byte records (begin word(object + 0xA8), end +0xAC) gets the source copied
// into the sub-object at +0x20 of record `index` (0x089641C4(dest, source) and the word at +0x10; skipped when dest is the source). A record whose state word (+8) is 0 also copies it
// into the previous record (index - 1) when that one's state is 1; then, when record `index` has state 1, the next record (index + 1, below the count) with state 0 gets it too.
void func_08A05FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x18u}});
    const u32 offset = index * 96u;
    ctx.gpr[20] = index;
    ctx.gpr[19] = source;
    ctx.gpr[18] = offset;
    ctx.gpr[17] = object;
    const u32 vector = object + 0xA8u;
    ctx.gpr[21] = vector;
    const auto copy_into = [&](u32 record) {
        const u32 sub = record + 0x20u;
        ctx.gpr[16] = sub;
        if (source != sub) {
            call_set(rt, ctx, 0x089641C4u, {sub, source});
            ctx.gpr[3] = word(rt, source + 0x10u);
            put(rt, sub + 0x10u, ctx.gpr[3]);
        }
    };
    u32 begin = word(rt, vector);
    copy_into(begin + offset);
    begin = word(rt, object + 0xA8u);
    ctx.gpr[5] = begin;
    if (word(rt, offset + begin + 8u) == 0u && static_cast<s32>(index - 1u) >= 0) {
        const u32 previous = begin + (index - 1u) * 96u;
        if (word(rt, previous + 8u) == 1u) copy_into(previous);
    }
    begin = word(rt, vector);
    ctx.gpr[6] = begin;
    ctx.gpr[2] = offset + begin;
    if (word(rt, offset + begin + 8u) != 1u) return;
    const u32 end = word(rt, vector + 4u);
    const s32 count = static_cast<s32>(static_cast<u32>(static_cast<s32>(end - begin) >> 5) * 0xAAAAAAABu);
    ctx.gpr[2] = 0xAAAAAAABu;
    const u32 next_index = index + 1u;
    ctx.gpr[5] = next_index;
    if (!(static_cast<s32>(next_index) < count)) return;
    const u32 next = begin + next_index * 96u;
    ctx.gpr[2] = next;
    if (word(rt, next + 8u) != 0u) return;
    copy_into(next);
    if (source == next + 0x20u) ctx.gpr[2] = next;
}
void func_08A060F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { propagate_payload(rt, ctx); }
void func_08B9A72C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { static constexpr StaticChain chain{0x08D75098u, 0x08D75344u, 0x08D75090u, 0x08D75338u, 0x08CBF934u, 0x08B9A4C4u}; initialise_statics_and_forward(rt, ctx, chain); } }
void func_08BA415C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { static constexpr StaticChain chain{0x08D750D8u, 0x08D75DE0u, 0x08D750D0u, 0x08D75DD4u, 0x08CC0FD8u, 0x08BA3AD4u}; initialise_statics_and_forward(rt, ctx, chain); } }
void func_08BA4A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { static constexpr StaticChain chain{0x08D75088u, 0x08D75330u, 0x08D75080u, 0x08D75324u, 0x08CBF930u, 0x08BA4830u}; initialise_statics_and_forward(rt, ctx, chain); } }
void func_08BAF6D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_shared_pair(rt, ctx); }
void func_08BD3FC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_with_holders(rt, ctx); }
void func_08836124(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); auto &F = ctx.fpr; G[5] = G[7]; G[19] = G[4]; G[17] = G[8]; { const bool tk = static_cast<std::int32_t>(G[6]) <= 0; if (tk) goto L_088361D4; } G[20] = frame.at(0x8u); G[4] = G[20]; call_here(rt, ctx, 0x08A98674u); G[2] = word(rt, frame.at(0x8u)); G[3] = word(rt, G[2] + 0x14u); G[16] = word(rt, G[3] + 0x0u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08836180; } G[4] = 0x08CA0000u; G[4] = 0x08C9B798u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08836180: ; G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[3] = word(rt, frame.at(0x8u)); put_float(rt, G[17] + 0x4u, F[0]); G[2] = word(rt, G[3] + 0x14u); G[16] = word(rt, G[2] + 0x4u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_088361B4; } G[4] = 0x08CA0000u; G[4] = 0x08C9B798u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_088361B4: ; G[2] = word(rt, G[3] + 0x54u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } put_float(rt, G[17] + 0x8u, F[0]); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); goto L_08836260; L_088361D4: ; G[20] = frame.at(0x8u); G[4] = G[20]; G[5] = 0x2u; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0x8u)); F[12] = float_at(rt, G[17] + 0x4u); G[16] = frame.at(0x4u); G[18] = word(rt, G[2] + 0x14u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_0883620C; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_0883620C: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x8u)); F[12] = float_at(rt, G[17] + 0x8u); G[4] = frame.at(0u); G[3] = word(rt, G[2] + 0x14u); G[16] = G[3] + 0x4u; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[16] == frame.at(0u); G[5] = word(rt, frame.at(0x0u)); if (tk) goto L_0883623C; } G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); L_0883623C: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); { const bool tk = G[19] == G[20]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_08836254; } G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); L_08836254: ; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); L_08836260: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088BC0C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xA0u); put(rt, frame.at(0x70u), G[31]); put(rt, frame.at(0x6Cu), G[19]); put(rt, frame.at(0x68u), G[18]); put(rt, frame.at(0x64u), G[17]); put(rt, frame.at(0x60u), G[16]); auto &F = ctx.fpr; G[2] = G[5] ^ 0x4u; G[3] = 0x2u; if (G[2] != 0u) G[3] = 0u; G[2] = 0x1u; G[19] = G[7]; G[18] = G[4]; G[17] = 0xAu; put_float(rt, frame.at(0x94u), F[25]); put_float(rt, frame.at(0x90u), F[24]); G[1] = 0x3F000000u; F[24] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x8Cu), F[23]); G[1] = 0x3D230000u; G[1] = G[1] | 0xD70Bu; F[23] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x88u), F[22]); put_float(rt, frame.at(0x84u), F[21]); put_float(rt, frame.at(0x80u), F[20]); G[8] = word(rt, G[4] + 0x0u); put(rt, frame.at(0xCu), G[6]); put(rt, frame.at(0x4u), G[3]); put(rt, frame.at(0x8u), G[5]); put_float(rt, frame.at(0x10u), F[12]); put(rt, frame.at(0x0u), G[2]); put(rt, frame.at(0x14u), 0u); G[2] = word(rt, G[8] + 0x1Cu); { const std::uint32_t tg = G[2]; F[20] = F[12]; call_here(rt, ctx, tg); } G[3] = word(rt, G[2] + 0x0u); G[4] = G[2]; G[16] = G[2]; G[2] = word(rt, G[3] + 0x8u); G[5] = G[18]; { const std::uint32_t tg = G[2]; G[6] = frame.at(0u); call_here(rt, ctx, tg); } F[22] = float_at(rt, frame.at(0x28u)); F[21] = float_at(rt, frame.at(0x2Cu)); F[25] = float_at(rt, frame.at(0x30u)); F[20] = F[20] + F[24]; L_088BC178: ; G[4] = G[16]; G[5] = G[18]; G[6] = frame.at(0u); put_float(rt, frame.at(0x10u), F[20]); G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[17] = G[17] + 0xFFFFFFFFu; call_here(rt, ctx, tg); } F[0] = float_at(rt, frame.at(0x28u)); F[1] = float_at(rt, frame.at(0x2Cu)); F[12] = F[0] - F[22]; F[13] = F[1] - F[21]; F[0] = psp_mul(F[12], F[12]); F[1] = psp_mul(F[13], F[13]); F[0] = F[0] + F[1]; ctx.set_fpu_condition(F[23] <= F[0]); { const bool tk = ctx.fpu_condition(); if (tk) goto L_088BC1CC; } { const bool tk = G[17] != 0u; F[20] = F[20] + F[24]; if (tk) goto L_088BC178; } L_088BC1CC: ; { const bool tk = G[19] == 0u; if (tk) goto L_088BC1E0; } put_float(rt, G[19] + 0x8u, F[25]); put_float(rt, G[19] + 0x0u, F[22]); put_float(rt, G[19] + 0x4u, F[21]); L_088BC1E0: ; F[12] = -F[12]; call_here(rt, ctx, 0x08B4A430u); F[25] = float_at(rt, frame.at(0x94u)); F[24] = float_at(rt, frame.at(0x90u)); F[23] = float_at(rt, frame.at(0x8Cu)); F[22] = float_at(rt, frame.at(0x88u)); F[21] = float_at(rt, frame.at(0x84u)); F[20] = float_at(rt, frame.at(0x80u)); goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x6Cu)); G[18] = word(rt, frame.at(0x68u)); G[17] = word(rt, frame.at(0x64u)); G[16] = word(rt, frame.at(0x60u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088BD8F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_weighted_value(rt, ctx); }
void func_088D6C00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { claim_free_slot(rt, ctx); }
void func_088D7DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_slot_table(rt, ctx); }
void func_088EDD5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { queue_ring_request(rt, ctx); }
// 0x0890FAD0 (a0 = the object, a1 = the target, a2 = the source object; v0 = the last call's): the source's virtual +0x24 answers a value kept at object + 0x80. When the signed byte at +0x28 is negative or the value
// changed, the byte becomes 0, the byte at +0x29 -1, and unless the source's virtual +0x60 answers 4 the floats at +0x8 / +0x10 / +0x14 become 0.1 / 5.0 / 5.0; +0xC becomes 1.0 and 0x08AB8720(target, 0, object + 8)
// runs. Then the source's virtuals +0x10 (frame), +0x14 (frame + 0xC) and +0x18 (frame) fill vectors that go through 0x08AB8A28, 0x08AB8930 and 0x08AB8A04 (target, 0, vector), and last
// 0x08AB897C(target, 0, 0, f12 = float(object + 8)).
void func_0890FAD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}, {31u, 0x30u}, {16u, 0x20u}});
    ctx.gpr[19] = target;
    ctx.gpr[18] = object;
    ctx.gpr[17] = source;
    const auto virtual_call = [&](u32 slot, std::initializer_list<u32> arguments) {
        return call_indirect(rt, ctx, word(rt, word(rt, source) + slot), arguments);
    };
    const u32 value = virtual_call(0x24u, {source});
    const u32 state = signed_byte(rt, object + 0x28u);
    const u32 old = word(rt, object + 0x80u);
    put(rt, object + 0x80u, value);
    if (static_cast<s32>(state) < 0 || value != old) {
        store8(object + 0x28u, 0u);
        store8(object + 0x29u, 0xFFu);
        const u32 kind = virtual_call(0x60u, {source});
        ctx.gpr[4] = target;
        ctx.gpr[5] = 0u;
        ctx.gpr[6] = object + 8u;
        ctx.gpr[3] = 4u;
        if (kind != 4u) {
            put_float(rt, object + 0x14u, 5.0f);
            put_float(rt, object + 8u, std::bit_cast<float>(0x3DCCCCCDu));
            put_float(rt, object + 0x10u, 5.0f);
        }
        put_float(rt, object + 0xCu, 1.0f);
        call_set(rt, ctx, 0x08AB8720u, {target, 0u, object + 8u});
    }
    virtual_call(0x10u, {source, frame.at(0u)});
    ctx.gpr[16] = frame.at(0xCu);
    call_set(rt, ctx, 0x08AB8A28u, {target, 0u, frame.at(0u)});
    virtual_call(0x14u, {source, frame.at(0xCu)});
    call_set(rt, ctx, 0x08AB8930u, {target, 0u, frame.at(0xCu)});
    virtual_call(0x18u, {source, frame.at(0u)});
    call_set(rt, ctx, 0x08AB8A04u, {target, 0u, frame.at(0u)});
    const float width = float_at(rt, object + 8u);
    const GuestResult done = call_guest(rt, ctx, 0x08AB897Cu, {target, 0u, 0u}, {width});
    ctx.gpr[2] = done.v0;
    ctx.gpr[3] = done.v1;
    ctx.fpr[0] = done.f0;
}
void func_089AA334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { draw_point_pairs(rt, ctx); }
void func_08ABE8B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_unpacked_pair(rt, ctx); }
void func_08ADD2C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_entries_by_name(rt, ctx); }
void func_08B349C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_owned_lock(rt, ctx); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_321
} // namespace gt::game

#define HOT321(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_321::name, gt::ReturnKind::kind)
// HOT321(0x08B136A8u, func_08B136A8, Word);  // no comparable synthetic case yet
// HOT321(0x08BA7990u, func_08BA7990, Word);  // no comparable synthetic case yet
// HOT321(0x08BC207Cu, func_08BC207C, Word);  // no comparable synthetic case yet
HOT321(0x088740A8u, func_088740A8, Word);
HOT321(0x088D876Cu, func_088D876C, Word);
HOT321(0x08B804B0u, func_08B804B0, Word);
HOT321(0x08C111CCu, func_08C111CC, Word);
HOT321(0x08A05FA4u, func_08A05FA4, Word);
HOT321(0x08A060F4u, func_08A060F4, Word);
HOT321(0x08B9A72Cu, func_08B9A72C, Word);
HOT321(0x08BA415Cu, func_08BA415C, Word);
HOT321(0x08BA4A78u, func_08BA4A78, Word);
HOT321(0x08BAF6D4u, func_08BAF6D4, Word);
HOT321(0x08BD3FC4u, func_08BD3FC4, Word);
HOT321(0x08836124u, func_08836124, Word);
// HOT321(0x088BC0C4u, func_088BC0C4, Word);  // no comparable synthetic case yet
HOT321(0x088BD8F8u, func_088BD8F8, Word);
HOT321(0x088D6C00u, func_088D6C00, Word);
HOT321(0x088D7DFCu, func_088D7DFC, Word);
HOT321(0x088EDD5Cu, func_088EDD5C, Word);
HOT321(0x0890FAD0u, func_0890FAD0, Word);
HOT321(0x089AA334u, func_089AA334, Word);
HOT321(0x08ABE8B8u, func_08ABE8B8, Word);
HOT321(0x08ADD2C4u, func_08ADD2C4, Word);
HOT321(0x08B349C4u, func_08B349C4, Word);
