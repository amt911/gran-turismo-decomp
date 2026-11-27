// New batch main-124: eleven mid-size functions (210-223 instructions each), written by hand from `tools.disasm`, DRAFT (unverified).
// Every step is the original's, in the original's order: a delay slot runs before its branch / call (a branch-likely delay slot only
// when the branch is taken), `lui [HI]` immediates are the relocated address printed as `[LO]`, callee-saved registers are put back at
// the end, and v0 ends as the asm leaves it on every exit path. No names: none has evidence.
// Not done (unsure): 0x08B7394C (a parser whose error path reads a0 after a call that clobbers it) and 0x08B6E920 (an expm1f-like libm
// routine on bit patterns, its float temporaries are part of the end state).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_124 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

// A guest call with the argument registers written to ctx exactly as the original writes them before its `jal`
// (a0-a3, t0-t3 in order); a register the call does not name keeps what an earlier call left there. Afterwards v0
// holds the result, as after the original's `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same for a callee whose v1 the original reads after it: v0 and v1 come back as the call leaves them.
GuestResult call_set_both(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with float arguments ($f12, $f13, ...) and / or a float result ($f0) the original reads afterwards.
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 19u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

u8 byte_at(Runtime &rt, u32 address) { return static_cast<u8>(rt.memory().load8(address)); }
s8 signed_byte_at(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
u16 half_at(Runtime &rt, u32 address) { return static_cast<u16>(rt.memory().load16(address)); }
s16 signed_half_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }
s32 signed16(u32 value) { return static_cast<s16>(static_cast<u16>(value)); }
u32 bswap(u32 value) { return __builtin_bswap32(value); }  // MIPS `wsbw`

// MIPS `ins rd, rs, pos, size`: `size` bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// s0-s7 and fp are callee-saved: the body sets them where the original sets them and the caller's values come back at the end.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};

// f20-f26 are callee-saved too (the originals spill the ones they use).
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 7u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFloats() {
        for (unsigned i = 0u; i < 7u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[7];
};

constexpr u32 kPanic = 0x08A5BB7Cu;       // called with a message when a pointer that must not be null is
constexpr u32 kRelease = 0x08A5B9ECu;     // releases a temporary string object
constexpr u32 kAssign = 0x08A5BAD8u;      // assigns a string object
constexpr u32 kMakeString = 0x08A85B1Cu;  // builds a string object from a value

// --- 0x088951FC (a0 = this, a1 = a destination): takes one 0x20-byte record from the queue at this+0x1C into the destination --------
// A byte table drives which fields of the record are refreshed: bit tests on the answer of 0x08894C6C(this) - 1, in the original's order.
struct FieldStep {
    u32 mask;
    u32 function;
    u32 offset;
};
constexpr FieldStep kFieldSteps[] = {
    {0x800u, 0x08BABCBCu, 0x00u}, {0x1000u, 0x08BABCBCu, 0x01u}, {0x8u, 0x08BABCF4u, 0x02u},   {0x2u, 0x08BABD2Cu, 0x04u},
    {0x10u, 0x08BABCF4u, 0x06u},  {0x20u, 0x08BABCBCu, 0x07u},   {0x1u, 0x08BABD2Cu, 0x08u},   {0x4u, 0x08BABD2Cu, 0x0Au},
    {0x80u, 0x08BABD2Cu, 0x0Cu},  {0x40u, 0x08BABD2Cu, 0x0Eu},   {0x2000u, 0x08BABCBCu, 0x14u}, {0x100u, 0x08BABD2Cu, 0x10u},
    {0x200u, 0x08BABCF4u, 0x12u}, {0x400u, 0x08BABCF4u, 0x13u},
};
void func_088951FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[17] = self;         // s1
    ctx.gpr[19] = destination;  // s3
    ctx.gpr[18] = 0u;           // s2 = the answer
    call_set(rt, ctx, 0x0892B440u, {destination, destination});
    if (byte_at(rt, self + 0x18u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 header = call_set(rt, ctx, 0x08894B60u, {word(rt, self)});
    const u32 header_tail = header + 0xCu;  // s4
    ctx.gpr[20] = header_tail;

    // The common end: the record counters move on and the destination is filled; `flag` is the answer.
    auto finish = [&](u32 flag) {
        ctx.gpr[18] = flag;
        const u32 next_position = word(rt, self + 8u) + 1u;
        const u32 remaining = word(rt, self + 0x24u) - 1u;
        const u32 record = word(rt, self + 0x1Cu);
        ctx.gpr[16] = record;
        put(rt, self + 0x24u, remaining);
        put(rt, self + 8u, next_position);  // the delay slot of the first call
        call_set(rt, ctx, 0x0892B4B0u, {record + 0x20u, record});
        call_set(rt, ctx, 0x0892B4B0u, {record, destination});
        const u32 limit = bswap(word(rt, header_tail));
        if (!(static_cast<s32>(word(rt, self + 8u)) < static_cast<s32>(limit))) call_set(rt, ctx, 0x08894E10u, {self});
        ctx.gpr[2] = flag;
    };

    bool take_record = false;
    if (static_cast<s32>(word(rt, self + 8u)) < static_cast<s32>(bswap(word(rt, header + 0xCu)))) {
        const u32 room = bswap(word(rt, header + 8u));
        const u32 used = word(rt, self + 0x2Cu) - word(rt, self + 0x28u);
        take_record = used < room;
    }
    if (!take_record) {
        call_set(rt, ctx, 0x08894E10u, {self});
        finish(0u);
        return;
    }

    const u32 record = word(rt, self + 0x1Cu);  // s0
    ctx.gpr[16] = record;
    call_set(rt, ctx, 0x0892B4B0u, {frame.at(0u), record});
    const s32 value = signed_half_at(rt, record + 4u);
    put(rt, record + 0x40u, static_cast<u32>(value));
    const s32 magnitude = value > -value ? value : -value;
    if (!(magnitude < 0x10)) {
        const s32 doubled = static_cast<s32>(static_cast<u32>(value) * 2u) - signed_half_at(rt, record + 0x24u);
        const s32 clamped = doubled >= 0x1001 ? 0x1000 : (doubled > -0x1000 ? doubled : -0x1000);
        put(rt, record + 0x40u, static_cast<u32>(clamped));
    }
    put16(rt, frame.at(4u), word(rt, record + 0x40u));
    if (static_cast<s32>(word(rt, self + 0x24u)) > 0) {
        call_set(rt, ctx, 0x0892B4B0u, {destination, frame.at(0u)});
        finish(1u);
        return;
    }
    const u32 flags = call_set(rt, ctx, 0x08894C6Cu, {self}) - 1u;
    ctx.gpr[16] = flags;
    for (const FieldStep &step : kFieldSteps) {
        if ((flags & step.mask) != 0u) call_set(rt, ctx, step.function, {self, frame.at(step.offset)});
    }
    if ((flags & 0x4000u) != 0u) {
        const s32 count = static_cast<s32>(call_set(rt, ctx, 0x08894C6Cu, {self}) - 1u);  // s5
        ctx.gpr[21] = static_cast<u32>(count);
        s32 index = 0;  // s0
        if (count > 0) {
            u32 cursor = frame.at(0x16u);  // s2
            do {
                ctx.gpr[18] = cursor;
                call_set(rt, ctx, 0x08BABCF4u, {self, cursor});
                ++index;
                ++cursor;
            } while (index != count);
        }
        if (count < 0xA) {  // (a count <= 0 jumps straight to the zero fill with the index at 0)
            do {
                put8(rt, frame.at(0x16u) + static_cast<u32>(index), 0u);
                ++index;
            } while (index != 0xA);
        }
        put8(rt, frame.at(3u), static_cast<u32>(count));
    }
    put(rt, self + 0x24u, call_set(rt, ctx, 0x08894C6Cu, {self}));
    call_set(rt, ctx, 0x0892B4B0u, {destination, frame.at(0u)});
    finish(1u);
}

// --- 0x08892C28 (a0 = this, a1 = rows, a2 = columns): fills three tables of 0x20-byte cells ---------------------------------------
void func_08892C28(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 rows = static_cast<s32>(ctx.gpr[5]);
    const u32 columns = ctx.gpr[6];
    constexpr u32 kFill = 0x481907FFu;
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[21] = self;  // s5
    put(rt, frame.at(4u), static_cast<u32>(rows));
    put(rt, frame.at(8u), columns);

    u32 half4 = half_at(rt, self + 4u);  // v1
    if (signed16(half4) > 0) {
        s32 index = 0;               // s2
        u32 low = self + 0x988u;     // s0
        u32 high = self + 0xA08u;    // s1
        do {
            call_set(rt, ctx, 0x08ABE178u, {kFill, low, 0x20u});
            call_set(rt, ctx, 0x08ABE178u, {kFill, high, 0x20u});
            half4 = half_at(rt, self + 4u);
            ++index;
            low += 0x20u;
            high += 0x20u;
        } while (index < signed16(half4));
    }
    if (signed_half_at(rt, self + 6u) > 0) {
        const u32 first_slot = static_cast<u32>(signed_half_at(rt, self + 0x560u));
        const u32 base = word(rt, self + (first_slot << 2) + 0x28u);  // s2, loaded in the call's delay slot
        ctx.gpr[18] = base;
        call_set(rt, ctx, 0x08ABE178u, {kFill, (first_slot << 5) + self + 0xA08u, 0x20u});
        if (signed_half_at(rt, self + 6u) >= 2) {
            s32 index = 1;  // s1
            u32 cursor = self;  // s0
            do {
                const u32 slot = static_cast<u32>(signed_half_at(rt, cursor + 0x562u));
                u32 value = word(rt, self + (slot << 2) + 0x28u);
                if (value != kFill) value -= base;
                call_set(rt, ctx, 0x08ABE178u, {value, (slot << 5) + self + 0xA08u, 0x20u});
                ++index;
                cursor += 2u;
            } while (index < signed_half_at(rt, self + 6u));
        }
        half4 = half_at(rt, self + 4u);
    }
    if (signed16(half4) > 0) {
        const u32 first_slot = static_cast<u32>(signed_half_at(rt, self + 0x558u));
        const u32 base = word(rt, self + (first_slot << 2) + 0x18u);  // s2
        ctx.gpr[18] = base;
        call_set(rt, ctx, 0x08ABE178u, {kFill, (first_slot << 5) + self + 0x988u, 0x20u});
        if (signed_half_at(rt, self + 4u) >= 2) {
            s32 index = 1;
            u32 cursor = self;
            do {
                const u32 slot = static_cast<u32>(signed_half_at(rt, cursor + 0x55Au));
                u32 value = word(rt, self + (slot << 2) + 0x18u);
                if (value != kFill) value -= base;
                call_set(rt, ctx, 0x08ABE178u, {value, (slot << 5) + self + 0x988u, 0x20u});
                ++index;
                cursor += 2u;
            } while (index < signed_half_at(rt, self + 4u));
        }
    }
    if (rows > 0) {
        s32 row = 0;
        do {
            const u32 u = static_cast<u32>(row);
            const u32 row_word = self + 4u * u;      // s6
            const u32 block = self + 0x200u * u;      // fp / sp+0x10 / sp+0x14
            const u32 cell = u << 5;                  // s0
            const u32 head = word(rt, row_word + 0x18u);
            if (head == kFill) {
                put8(rt, self + 0x20u * u + 0x788u, 0u);
            } else {
                call_set(rt, ctx, 0x08ABE090u, {head, cell + self + 0x788u, 0x20u});
            }
            call_set(rt, ctx, 0x08ABE090u, {word(rt, row_word + 0x28u), cell + self + 0x808u, 0x20u});
            call_set(rt, ctx, 0x08ABE090u, {word(rt, row_word + 0x38u), cell + self + 0x888u, 0x20u});
            call_set(rt, ctx, 0x08ABE090u, {word(rt, row_word + 0x48u), cell + self + 0x908u, 0x20u});
            if (static_cast<s32>(columns) > 0) {
                u32 source = self + 0x50u + 0x40u * u + 8u;  // s0
                u32 first = block + 0x1288u;                  // s1
                u32 second = block + 0x1A88u;                 // s2
                u32 third = block + 0xA88u;                   // s3
                u32 column = 0u;                              // s4
                do {
                    call_set(rt, ctx, 0x08ABE090u, {word(rt, source), first, 0x20u});
                    call_set(rt, ctx, 0x08ABE090u, {word(rt, source + 0x200u), second, 0x20u});
                    call_set(rt, ctx, 0x08ABE090u, {word(rt, source + 0x100u), third, 0x20u});
                    ++column;
                    first += 0x20u;
                    second += 0x20u;
                    third += 0x20u;
                    source += 4u;
                } while (column != columns);
            }
            ++row;
        } while (row != rows);
    }
    ctx.gpr[2] = static_cast<u32>(rows);
}

// --- 0x0896AD58 (a0 = this): applies translation / rotation / scale members of this to the object at this+0xA0 -----------------------
void func_0896AD58(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    SavedFloats saved_floats(ctx);
    GuestFrame frame(ctx, 0x60u);
    const float epsilon = std::bit_cast<float>(0x34000000u);
    const u32 slot = self + 0xA0u;  // s3
    ctx.gpr[16] = self;
    ctx.gpr[19] = slot;
    const u32 owner = word(rt, self + 0xA0u);
    put(rt, self + 0xE0u, insert_bits(word(rt, self + 0xE0u), 0u, 20u, 1u));
    if (owner != 0u) call_set(rt, ctx, 0x0897D738u, {owner});

    const float f20 = float_at(rt, self + 0xBCu);
    const float f21 = float_at(rt, self + 0xC0u);
    const float f25 = float_at(rt, self + 0xB4u);
    const float f26 = float_at(rt, self + 0xB8u);
    const float f24 = float_at(rt, self + 0xACu);
    const float f23 = float_at(rt, self + 0xB0u);
    const float f22 = float_at(rt, self + 0xC4u);
    const bool translate = epsilon <= std::fabs(f20) || epsilon <= std::fabs(f21);                     // s1
    const bool rotate_z = epsilon <= std::fabs(f22);                                                   // s4
    const bool scale = epsilon <= std::fabs(f25 - 1.0f) || epsilon <= std::fabs(f26 - 1.0f);          // s5
    const bool shear_x = epsilon <= std::fabs(f24);                                                    // s7
    const bool shear_y = epsilon <= std::fabs(f23);                                                    // s6
    const bool any_other = rotate_z || scale || shear_x || shear_y;                                    // s2
    if (!translate && !any_other) return;

    if (word(rt, slot) == 0u) {
        call_set(rt, ctx, 0x0897DFBCu, {frame.at(0u)});
        const u32 made = word(rt, frame.at(0u));
        if (slot != frame.at(0u)) call_set(rt, ctx, kAssign, {slot, made});
        call_set(rt, ctx, kRelease, {frame.at(0u)});
    }
    if (translate) {
        const u32 target = word(rt, self + 0xA0u);  // s1
        ctx.gpr[17] = target;
        ctx.fpr[12] = f20;
        if (target == 0u) {
            call_set(rt, ctx, kPanic, {0x08CA28B4u});
            ctx.fpr[12] = f20;
        }
        call_setf(rt, ctx, 0x0897D7F4u, {target}, {f20, f21});
    }
    if (!any_other) return;

    call_set(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(4u), frame.at(8u)});
    const float scale_a = float_at(rt, self + 0xA4u);
    const float scale_b = float_at(rt, self + 0xA8u);
    const float moved_x = float_at(rt, frame.at(4u));
    const float moved_y = float_at(rt, frame.at(8u));
    const u32 target = word(rt, self + 0xA0u);
    const float offset_x = scale_a * moved_x;  // f21
    const float offset_y = scale_b * moved_y;  // f20
    ctx.gpr[17] = target;
    if (target == 0u) call_set(rt, ctx, kPanic, {0x08CA28B4u});
    call_setf(rt, ctx, 0x0897D7F4u, {target}, {offset_x, offset_y});
    if (rotate_z) {
        const u32 object = word(rt, self + 0xA0u);
        ctx.gpr[16] = object;
        ctx.fpr[12] = f22;
        if (object == 0u) call_set(rt, ctx, kPanic, {0x08CA28B4u});
        call_setf(rt, ctx, 0x0897D7FCu, {object}, {f22});
    }
    if (scale) {
        const u32 object = word(rt, slot);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, kPanic, {0x08CA28B4u});
        call_setf(rt, ctx, 0x0897D704u, {object}, {f25, f26});
    }
    if (shear_y) {
        const u32 object = word(rt, slot);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, kPanic, {0x08CA28B4u});
        call_setf(rt, ctx, 0x0897D80Cu, {object}, {f23});
    }
    u32 object = word(rt, slot);  // the delay slot of the test of s7
    ctx.gpr[16] = object;
    if (shear_x) {
        if (object == 0u) call_set(rt, ctx, kPanic, {0x08CA28B4u});
        call_setf(rt, ctx, 0x0897D804u, {object}, {f24});
        object = word(rt, slot);
        ctx.gpr[16] = object;
    }
    if (object == 0u) call_set(rt, ctx, kPanic, {0x08CA28B4u});
    call_setf(rt, ctx, 0x0897D7F4u, {object}, {-offset_x, -offset_y});
}

// --- 0x08AFFECC (a0 = this): when bit 0 of the half at +0xE2 is set, composes a quaternion (this+0..0xC) from the parameter block at
// this+0xB0 and the answer of 0x08BAC248, then moves the float at +0x24 -----------------------------------------------------------------

// --- 0x08AD90AC (a0 = a host object): the one-time set-up of a subsystem whose state block is at 0x08D405D0 ----------------------------
void func_08AD90AC(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kState = 0x08D405D0u;
    constexpr u32 kFlags = 0x08CDFB3Cu;
    constexpr u32 kLog = 0x08CDFB40u;
    const u32 host = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = host;
    ctx.gpr[16] = kState;
    auto log_failure = [&](u32 line, u32 text) { call_set(rt, ctx, 0x08AD83B8u, {word(rt, kLog), 0x08C9085Cu, line, text}); };
    // The undo path shared by every failure: the module teardowns, then the three allocations.
    auto tear_down = [&]() {
        call_set(rt, ctx, 0x08AEA0A4u, {});
        call_set(rt, ctx, 0x08ADDF3Cu, {});
        call_set(rt, ctx, 0x08AE2EB4u, {});
        call_set(rt, ctx, 0x08AE687Cu, {});
        call_set(rt, ctx, 0x08ADB4BCu, {});
        call_set(rt, ctx, 0x08AD9FF0u, {});
        call_set(rt, ctx, 0x08AEB3BCu, {});
        const u32 last = word(rt, kState + 0x28u);
        if (last != 0u) call_set(rt, ctx, 0x08AEB8C8u, {last});
        const u32 middle = word(rt, kState + 0x24u);
        if (middle != 0u) call_set(rt, ctx, 0x08AEB8C8u, {middle});
        const u32 block = word(rt, kState + 0x18u);
        if (block != 0u) call_set(rt, ctx, 0x08AD85C8u, {block});
        ctx.gpr[2] = 0u;
    };

    if (word(rt, kState) != 0u) {
        if ((word(rt, kFlags) & 1u) != 0u) log_failure(0x266u, 0x08C908BCu);
        ctx.gpr[2] = 0u;
        return;
    }
    if ((word(rt, kFlags) & 4u) != 0u) {
        call_set(rt, ctx, 0x08AD8464u, {0x08C908D8u});
    }
    put(rt, kState + 4u, 0u);
    for (u32 offset = 8u; offset <= 0x28u; offset += 4u) put(rt, kState + offset, 0u);
    put(rt, kState, 0u);
    const u32 first = call_set(rt, ctx, 0x08AEB850u, {0x08C908FCu});
    put(rt, kState + 0x24u, first);
    u32 second = 0u;
    if (first != 0u) {
        second = call_set(rt, ctx, 0x08AEB850u, {0x08C9090Cu});
        put(rt, kState + 0x28u, second);
    }
    if (second == 0u) {
        if ((word(rt, kFlags) & 1u) != 0u) log_failure(0x273u, 0x08C9091Cu);
        tear_down();
        return;
    }
    put(rt, kState + 8u, call_set(rt, ctx, 0x08AEB954u, {}));
    put(rt, kState + 0x20u, 0x2000u);
    put(rt, kState + 0x14u, 0xFFFFFFFFu);
    call_set(rt, ctx, 0x08AD8960u, {host, 0x08C90934u, kState + 0xCu, 0u});
    call_set(rt, ctx, 0x08AD8960u, {host, 0x08C90944u, kState + 0x1Cu, 0x20u});
    const u32 block = call_set(rt, ctx, 0x08AD84B4u, {(word(rt, kState + 0x1Cu) << 5) + 0x20u, 1u});
    if (block == 0u) {
        if ((word(rt, kFlags) & 1u) != 0u) log_failure(0x284u, 0x08C9098Cu);
        tear_down();
        return;
    }
    put(rt, kState + 0x18u, block);
    u32 cursor = block + 0x20u;  // a1
    for (u32 k = 0u; k < 2u; ++k) {
        const u32 entry = word(rt, kState + 0x18u) + (k << 4);
        put(rt, entry, cursor);
        put(rt, entry + 0xCu, 0u);
        const u32 advance = word(rt, kState + 0x1Cu) << 4;
        put(rt, entry + 4u, 0u);
        put(rt, entry + 8u, 0u);
        cursor += advance;
    }
    constexpr u32 kSteps[] = {0x08AEAD38u, 0x08AD9F10u, 0x08ADB3E8u, 0x08AE6620u, 0x08AE2D38u, 0x08ADDE30u, 0x08AE9F98u};
    for (const u32 step : kSteps) {
        if (call_set(rt, ctx, step, {host}) == 0u) {
            tear_down();
            return;
        }
    }
    call_set(rt, ctx, 0x08AD97D8u, {1u});
    const u32 flags = word(rt, kFlags);
    put(rt, kState, 1u);
    if ((flags & 4u) != 0u) {
        call_set(rt, ctx, 0x08AD8464u, {0x08C90978u, word(rt, kState + 0x10u)});
        if ((word(rt, kFlags) & 4u) != 0u) call_set(rt, ctx, 0x08AD8464u, {0x08C90954u});
    }
    ctx.gpr[2] = 1u;
}

// --- 0x08806C44 (a0 = a string out, a1 = a key, a2 = a kind, a3 = a holder of seven objects): inserts a record into a sorted table of
// ten 0x18-byte entries -------------------------------------------------------------------------------------------------------------
void func_08806C44(Runtime &rt, AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const s32 kind = static_cast<s32>(ctx.gpr[6]);
    const u32 holder = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    ctx.gpr[30] = key;     // fp
    ctx.gpr[17] = holder;  // s1
    put(rt, frame.at(0x20u), output);
    if (kind >= 7) {
        // A member of the holder: a null one is reported, then used anyway, as the original does.
        auto member = [&](u32 offset) {
            const u32 object = word(rt, holder + offset);
            ctx.gpr[16] = object;
            if (object == 0u) call_set(rt, ctx, kPanic, {0x08C97DA8u});
            return object;
        };
        auto ask = [&](u32 object, u32 slot) { return call_indirect(rt, ctx, word(rt, word(rt, object) + slot), {object}); };
        const u32 s5 = ask(member(0u), 0x3Cu);
        ctx.gpr[21] = s5;
        const u32 s2 = ask(member(4u), 0x50u);
        const u32 s3 = ctx.gpr[3];
        ctx.gpr[18] = s2;
        ctx.gpr[19] = s3;
        put(rt, frame.at(0x1Cu), ask(member(8u), 0x3Cu));
        put(rt, frame.at(0x18u), ask(member(0xCu), 0x3Cu));
        put(rt, frame.at(0x14u), ask(member(0x10u), 0x3Cu));
        put(rt, frame.at(0x10u), ask(member(0x14u), 0x3Cu));
        const u32 s6 = ask(member(0x18u), 0x40u);
        const u32 s7 = ctx.gpr[3];
        ctx.gpr[22] = s6;
        ctx.gpr[23] = s7;
        call_set(rt, ctx, 0x08A57854u, {frame.at(4u), key});

        u32 object = word(rt, frame.at(4u));
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, kPanic, {0x08C97DD8u});
        u32 table = word(rt, object + 0x1Cu);
        if (table == 0u) {
            object = word(rt, object + 0x18u);
            ctx.gpr[16] = object;
            if (object == 0u) call_set(rt, ctx, kPanic, {0x08C97DBCu});
            table = word(rt, object + 0xCu);
        }
        ctx.gpr[17] = table;
        call_set(rt, ctx, kRelease, {frame.at(4u)});

        // The last entry equal to (s2, s3) and the first one whose word +8 is above s5.
        s32 match = 9;     // a3
        s32 position = 10;  // t6
        for (s32 i = 0; i != 10; ++i) {
            const u32 entry = table + 0x18u * static_cast<u32>(i);
            const bool earlier = i < position;
            if (word(rt, entry) == s2 && word(rt, entry + 4u) == s3) match = i;
            if (earlier && static_cast<s32>(s5) < static_cast<s32>(word(rt, entry + 8u))) position = i;
        }
        s32 shown = -1;  // a1 of the string builder
        if (!(match < position)) {
            if (position < match) {
                s32 at = match;  // t7
                do {
                    const u32 from = table + 0x18u * static_cast<u32>(at - 1);
                    const u32 to = table + 0x18u * static_cast<u32>(at);
                    const u32 w0 = word(rt, from);
                    const u32 w1 = word(rt, from + 4u);
                    const u32 w4 = word(rt, from + 0x10u);
                    const u32 w5 = word(rt, from + 0x14u);
                    const u32 w2 = word(rt, from + 8u);
                    const u32 b0 = byte_at(rt, from + 0xCu);
                    const u32 b1 = byte_at(rt, from + 0xDu);
                    const u32 b2 = byte_at(rt, from + 0xEu);
                    const u32 b3 = byte_at(rt, from + 0xFu);
                    --at;
                    put(rt, to + 8u, w2);
                    put(rt, to, w0);
                    put(rt, to + 4u, w1);
                    put8(rt, to + 0xCu, b0);
                    put8(rt, to + 0xDu, b1);
                    put8(rt, to + 0xEu, b2);
                    put8(rt, to + 0xFu, b3);
                    put(rt, to + 0x10u, w4);
                    put(rt, to + 0x14u, w5);
                } while (position < at);
            }
            const u32 entry = table + 0x18u * static_cast<u32>(position);
            put(rt, entry + 0x10u, s6);
            put(rt, entry + 0x14u, s7);
            put(rt, entry + 8u, s5);
            put(rt, entry, s2);
            put(rt, entry + 4u, s3);
            put8(rt, entry + 0xCu, word(rt, frame.at(0x1Cu)));
            put8(rt, entry + 0xDu, word(rt, frame.at(0x18u)));
            put8(rt, entry + 0xEu, word(rt, frame.at(0x14u)));
            put8(rt, entry + 0xFu, word(rt, frame.at(0x10u)));
            shown = position;
        }
        call_set(rt, ctx, kMakeString, {frame.at(0u), static_cast<u32>(shown)});
        const u32 target = word(rt, frame.at(0x20u));
        const u32 text = word(rt, frame.at(0u));
        if (target != frame.at(0u)) call_set(rt, ctx, kAssign, {target, text});
        call_set(rt, ctx, kRelease, {frame.at(0u)});
    }
}

// --- 0x088175B8 (a0 = a host object): builds the table of 17 subsystem handles at 0x08CE2A38 once, registers each with the host and
// runs the host's per-subsystem set-up calls ---------------------------------------------------------------------------------------------
void func_088175B8(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kGuard = 0x08CE2A30u;
    constexpr u32 kTable = 0x08CE2A38u;
    constexpr u32 kBuilders[] = {0x0884D394u, 0x088586B4u, 0x08854634u, 0x0884C458u, 0x088292D0u, 0x0882462Cu,
                                 0x088350A0u, 0x08846E0Cu, 0x08845754u, 0x088445F0u, 0x088433C0u, 0x08841738u,
                                 0x08835934u, 0x088373F4u, 0x08838D64u, 0x0881A768u, 0x08817918u};
    constexpr u32 kSetups[] = {
        0x0882222Cu, 0x08822920u, 0x08823BBCu, 0x088541F0u, 0x08828D74u, 0x08834670u, 0x08834A8Cu, 0x0885B4CCu, 0x0885B5F8u, 0x0885B724u,
        0x0885B850u, 0x0885B3A0u, 0x088592D0u, 0x088593FCu, 0x088591A4u, 0x0885A7E8u, 0x0885A914u, 0x0885AA40u, 0x0885AB6Cu, 0x0885AC98u,
        0x0885ADC4u, 0x0885B01Cu, 0x0885B274u, 0x0885B148u, 0x0885A590u, 0x0885A464u, 0x0885A338u, 0x08859D5Cu, 0x08859E88u, 0x0885A20Cu,
        0x08859FB4u, 0x0885A0E0u, 0x08859C30u, 0x088598ACu, 0x088599D8u, 0x08859B04u, 0x08859780u, 0x08859654u, 0x08859528u, 0x08859078u,
        0x08858F4Cu, 0x08858E20u};
    constexpr u32 kHostMessage = 0x08C98C78u;
    const u32 host = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = host;
    ctx.gpr[18] = kGuard;
    ctx.gpr[17] = kTable;
    if (signed_byte_at(rt, kGuard) == 0) {
        if (call_set(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            ctx.gpr[16] = kTable;
            u32 slot = kTable;
            for (const u32 builder : kBuilders) {
                put(rt, slot, call_set(rt, ctx, builder, {}));
                slot += 4u;
            }
            call_set(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    if (word(rt, kTable) != 0u) {
        u32 cursor = kTable;  // s1
        u32 previous;         // s2
        do {
            const u32 object = word(rt, host);
            ctx.gpr[16] = object;
            previous = cursor;
            ctx.gpr[18] = previous;
            if (object == 0u) call_set(rt, ctx, kPanic, {kHostMessage});
            call_set(rt, ctx, 0x08A7CC30u, {object, word(rt, cursor)});
            cursor += 4u;
            ctx.gpr[17] = cursor;
        } while (word(rt, previous + 4u) != 0u);
    }
    u32 object = word(rt, host);
    ctx.gpr[17] = object;
    ctx.gpr[16] = frame.at(8u);
    if (object == 0u) call_set(rt, ctx, kPanic, {kHostMessage});
    call_set(rt, ctx, 0x0883A4D8u, {frame.at(8u)});
    call_set(rt, ctx, 0x08A7CE50u, {object, frame.at(8u)});
    call_set(rt, ctx, kRelease, {frame.at(8u)});
    for (const u32 setup : kSetups) call_set(rt, ctx, setup, {host});

    object = word(rt, host);
    ctx.gpr[17] = object;
    ctx.gpr[16] = frame.at(4u);
    if (object == 0u) call_set(rt, ctx, kPanic, {kHostMessage});
    call_set(rt, ctx, kMakeString, {frame.at(4u), 4u});
    call_set(rt, ctx, 0x08B86E98u, {object, 0x08C98C80u, frame.at(4u)});
    call_set(rt, ctx, kRelease, {frame.at(4u)});
    object = word(rt, host);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, kPanic, {kHostMessage});
    call_set(rt, ctx, kMakeString, {frame.at(0u), 2u});
    call_set(rt, ctx, 0x08B86E98u, {object, 0x08C98C8Cu, frame.at(0u)});
    call_set(rt, ctx, kRelease, {frame.at(0u)});
    call_set(rt, ctx, 0x088213BCu, {host});
    call_set(rt, ctx, 0x0881FDECu, {host});
    call_set(rt, ctx, 0x08820C98u, {host});
}

// --- 0x08A4AFB4 (a0 = a string out, a1 = a count, a2 = a smart pointer): when the count is positive, asks the pointed object (virtual +0xC)
// for a block, copies its 0x3C-byte descriptor and writes the descriptor's fields as strings into a temporary, assigned to a0 --------------------
void func_08A4AFB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 output = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 pointer = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x110u);
    ctx.gpr[20] = output;  // s4
    if (count <= 0) return;
    u32 object = word(rt, pointer);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, kPanic, {0x08CAED38u});
    const u32 vtable = word(rt, object);
    const u32 method = word(rt, vtable + 0xCu);
    ctx.gpr[18] = frame.at(0x70u);
    ctx.gpr[17] = frame.at(0xB0u);
    call_indirect(rt, ctx, method, {frame.at(0x1Cu), object});
    const u32 block = word(rt, frame.at(0x1Cu));
    ctx.gpr[16] = block;
    call_set(rt, ctx, 0x08B0CE58u, {frame.at(0x70u)});
    call_set(rt, ctx, 0x08B10204u, {frame.at(0xB0u), block, 1u});
    for (u32 i = 0u; i < 60u; i += 4u) put(rt, frame.at(0x30u) + i, word(rt, frame.at(0xB0u) + i));
    for (u32 i = 0u; i < 60u; i += 4u) put(rt, frame.at(0x70u) + i, word(rt, frame.at(0x30u) + i));
    const u32 text = frame.at(0x20u);  // s3
    ctx.gpr[19] = text;
    call_set(rt, ctx, 0x08A76AACu, {text, 0x08CF6ED8u, word(rt, frame.at(100u))});
    u32 builder = frame.at(0x24u);  // s2
    if (word(rt, frame.at(0x70u)) == 0u) {
        const u32 a1 = word(rt, frame.at(0x90u));
        const u32 a2 = word(rt, frame.at(0x80u));
        const u32 temporary = frame.at(0x28u);
        ctx.gpr[16] = temporary;
        call_set(rt, ctx, 0x08A44D9Cu, {temporary, a1, a2, 1u});
        if (text != temporary) call_set(rt, ctx, kAssign, {text, word(rt, frame.at(0x28u))});
        call_set(rt, ctx, kRelease, {temporary});
        builder = frame.at(0x24u);
    }
    ctx.gpr[18] = builder;
    call_set(rt, ctx, 0x08A6BA44u, {builder});
    // Four labelled fields: the slot answers a string builder entry, the value comes from the descriptor.
    struct Field {
        u32 label;
        u32 source;
        u32 slot;
    };
    constexpr Field kFields[] = {{0x08CAEFD8u, 0x70u, 0x18u}, {0x08CAEFE0u, 0x74u, 0x14u}, {0x08CAEFE8u, 0x78u, 0x10u}, {0x08CAEFF4u, 0x88u, 0x0Cu}};
    for (const Field &field : kFields) {
        const u32 entry = call_set(rt, ctx, 0x08A6BBE8u, {builder, field.label});
        const u32 value = word(rt, frame.at(field.source));
        const u32 temporary = frame.at(field.slot);
        ctx.gpr[16] = temporary;
        ctx.gpr[17] = entry;
        call_set(rt, ctx, kMakeString, {temporary, value});
        if (entry != temporary) call_set(rt, ctx, kAssign, {entry, word(rt, temporary)});
        call_set(rt, ctx, kRelease, {temporary});
    }
    const u32 entry = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAF014u});
    if (entry != text) call_set(rt, ctx, kAssign, {entry, word(rt, frame.at(0x20u))});
    const u32 last = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAF00Cu});
    ctx.gpr[16] = last;
    ctx.gpr[17] = frame.at(8u);
    call_set(rt, ctx, kMakeString, {frame.at(8u), 0u});
    if (last != frame.at(8u)) call_set(rt, ctx, kAssign, {last, word(rt, frame.at(8u))});
    call_set(rt, ctx, kRelease, {frame.at(8u)});
    ctx.gpr[16] = frame.at(4u);
    call_set(rt, ctx, 0x08A76ABCu, {frame.at(4u), builder});
    if (output != frame.at(4u)) call_set(rt, ctx, kAssign, {output, word(rt, frame.at(4u))});
    call_set(rt, ctx, kRelease, {frame.at(4u)});
    call_set(rt, ctx, kRelease, {builder});
    call_set(rt, ctx, kRelease, {text});

    const u32 data = word(rt, frame.at(0x1Cu));
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) <= 0) {
            call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
        }
    }
}

// --- 0x088C2724 (a0 = this): sets up the record at this+0x24 from the three members at this+4.. and steers a follower object -----------
void func_088C2724(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 members = self + 4u;  // s4
    SavedRegisters saved(ctx);
    SavedFloats saved_floats(ctx);
    GuestFrame frame(ctx, 0x90u);
    ctx.gpr[17] = self;
    ctx.gpr[20] = members;
    const u32 record = word(rt, self + 0x24u);  // s0
    ctx.gpr[16] = record;
    put16(rt, record + 0x68u, 1u);
    u32 first = word(rt, self + 4u);
    s32 mode;  // s3
    if (first == 0u || word(rt, members + 4u) == 0u || word(rt, members + 8u) == 0u) {
        const float far = std::bit_cast<float>(0x429C0000u);
        mode = 6;
        put_float(rt, record + 0x1Cu, far);
        put_float(rt, record + 0x18u, far);
    } else {
        if (signed_byte_at(rt, word(rt, members + 4u) + 3u) == 2) {
            put16(rt, record + 0x68u, word(rt, members + 0xCu) + 1u);
            first = word(rt, self + 4u);
        }
        const float level = float_at(rt, first + 0x1360u);
        mode = 7;
        put_float(rt, record + 0x1Cu, level);
        put_float(rt, record + 0x18u, level);
        if (signed_byte_at(rt, word(rt, members + 4u) + 3u) != 0) mode = -1;
    }
    ctx.gpr[19] = static_cast<u32>(mode);

    const u32 owner = word(rt, self);
    if (owner != 0u) put8(rt, record + 0xEu, call_set(rt, ctx, 0x088B1A18u, {owner}));
    if (byte_at(rt, record + 0xEu) == 0u) {
        const u32 kind = call_set(rt, ctx, 0x088B1A0Cu, {word(rt, self)});
        if (kind - 2u < 2u) put8(rt, record + 0xEu, 1u);
    }
    put_float(rt, record + 0x78u, 1.0f);
    put_float(rt, record + 0x70u, 1.0f);
    put_float(rt, record + 0x74u, 1.0f);
    call_set(rt, ctx, 0x088BB57Cu, {record});
    put16(rt, record + 0x6Au, half_at(rt, record + 0x68u) | 1u);
    call_set(rt, ctx, 0x088BB5A0u, {record});
    call_set(rt, ctx, 0x088C4D20u, {self});
    const u32 follower = word(rt, self + 0x1Cu);  // s2
    ctx.gpr[18] = follower;
    if (follower == 0u) return;
    const u32 alive = call_indirect(rt, ctx, word(rt, word(rt, follower) + 8u), {follower});
    if (alive == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (mode == -1) {
        if (word(rt, members) == 0u || word(rt, members + 4u) == 0u || word(rt, members + 8u) == 0u) {
            mode = -1;
        } else {
            mode = static_cast<s32>(call_set(rt, ctx, 0x088BF760u, {follower, members}));
        }
        ctx.gpr[19] = static_cast<u32>(mode);
    }
    mode = static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, follower) + 0x24u), {follower, static_cast<u32>(mode)}));
    ctx.gpr[19] = static_cast<u32>(mode);
    call_set(rt, ctx, 0x088BF94Cu, {static_cast<u32>(mode), self});
    if (mode == 3) put8(rt, record + 0xEu, 1u);
    if (byte_at(rt, record + 0xEu) != 0u) {
        ctx.gpr[2] = byte_at(rt, record + 0xEu);
        return;
    }

    const u32 sequence = frame.at(8u);  // s1
    ctx.gpr[17] = sequence;
    call_set(rt, ctx, 0x088C50B8u, {sequence, follower, byte_at(rt, record + 0x10u)});
    const s32 length = static_cast<s32>(word(rt, frame.at(8u)));
    s32 wrapped = length + static_cast<s32>(byte_at(rt, record + 0x11u)) - 1;
    if (!(wrapped < length)) {
        wrapped -= length;
        while (!(wrapped < length)) wrapped -= length;
    }
    const u32 step = call_set(rt, ctx, 0x088C50C4u, {sequence, static_cast<u32>(wrapped)});
    float span = float_at(rt, step + 0x18u);
    if (span < 0.0f) span = -span;
    if (span != 0.0f && !(1200.0f <= span)) {
        put8(rt, record + 0xEu, 1u);
        ctx.gpr[2] = 1u;
        return;
    }
    put(rt, frame.at(0x18u), static_cast<u32>(mode));
    put(rt, frame.at(0x10u), 1u);
    put(rt, frame.at(0x14u), 1u);
    const u32 from = word(rt, members + 0xCu);
    const u32 to = word(rt, members + 8u);
    put(rt, frame.at(0x1Cu), byte_at(rt, record + 0x11u));
    const u32 position = call_set(rt, ctx, 0x088DF13Cu, {to, from});
    const float target_x = float_at(rt, position);
    const u32 anchor = word(rt, members + 4u);
    put_float(rt, frame.at(0x24u), 200.0f);
    put_float(rt, frame.at(0x20u), target_x);
    put_float(rt, frame.at(0x38u), float_at(rt, anchor + 0x10u));
    put_float(rt, frame.at(0x3Cu), float_at(rt, anchor + 0x14u));
    put_float(rt, frame.at(0x40u), float_at(rt, anchor + 0x18u));
    const u32 holder = call_indirect(rt, ctx, word(rt, word(rt, follower) + 0x1Cu), {follower});
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder, follower, frame.at(0x10u)});
    const u32 anchor_now = word(rt, members + 4u);
    float dx = float_at(rt, frame.at(0x38u));
    float dy = float_at(rt, frame.at(0x3Cu));
    dx = dx - float_at(rt, anchor_now + 0x10u);
    dy = dy - float_at(rt, anchor_now + 0x14u);
    const float angle = call_setf(rt, ctx, 0x08B4A430u, {}, {-dx, dy}).f0;
    const u32 anchor_last = word(rt, members + 4u);
    put_float(rt, frame.at(4u), angle);
    put_float(rt, frame.at(0u), float_at(rt, anchor_last + 0x704u));
    float turn = call_setf(rt, ctx, 0x0893C60Cu, {frame.at(4u), frame.at(0u)}).f0;
    if (turn < 0.0f) turn = -turn;
    if (std::bit_cast<float>(0x3DD67750u) < turn) put8(rt, record + 0xEu, 1u);
}

// --- 0x089F2028 (a0 = a model, a1 = a sink object, a2 = a pair of floats at +0x18 / +0x1C, a3 / t0 = two weight tables): builds eight
// (and for a four-corner model eight bilinear-weighted) vertex pairs and hands them to the sink's virtual +0x10C -----------------------------------
void func_089F2028(Runtime &rt, AllegrexContext &ctx) {
    const u32 model = ctx.gpr[4];
    const u32 sink = ctx.gpr[5];
    const u32 pair = ctx.gpr[6];
    const u32 table_a = ctx.gpr[7];   // s6
    const u32 table_b = ctx.gpr[8];   // s7
    SavedRegisters saved(ctx);
    SavedFloats saved_floats(ctx);
    GuestFrame frame(ctx, 0x160u);
    ctx.gpr[16] = model;
    ctx.gpr[30] = sink;
    ctx.gpr[23] = table_b;
    ctx.gpr[22] = table_a;
    // (two empty seven-pass counting loops sit here in the original; they only leave v0 behind)
    for (u32 offset = 0x90u; offset != 0x110u; offset += 4u) put_float(rt, frame.at(offset), 1.0f);
    const float one = 1.0f;  // f20
    const float f21 = float_at(rt, pair + 0x1Cu);
    const float f22 = float_at(rt, pair + 0x18u);
    const u32 kind = call_set(rt, ctx, 0x08A06EC8u, {model});
    constexpr u32 kClassAddress = 0x08C8CC14u;
    const u32 out_a = frame.at(0x10u);  // a2 / the first output table
    const u32 out_b = frame.at(0x50u);  // t0
    if (kind == 1u) {
        for (u32 i = 0u; i != 8u; ++i) {
            const u32 high = ((i + (i >> 31)) >> 1) << 2;
            const u32 low = (i & 1u) << 2;
            const u32 row_b = table_b + low;
            const u32 row_a = table_a + high;
            float scaled_b = float_at(rt, row_b + 0x10u);
            float scaled_a = float_at(rt, row_a + 0x10u);
            const float base_b = float_at(rt, row_b);
            scaled_b = f21 * scaled_b;
            scaled_a = f22 * scaled_a;
            const float base_a = float_at(rt, row_a);
            const u32 slot = i << 3;
            put_float(rt, out_a + slot + 4u, base_b);
            put_float(rt, out_a + slot, base_a);
            put_float(rt, out_b + slot + 4u, scaled_b);
            put_float(rt, out_b + slot, scaled_a);
        }
        const u32 vtable = word(rt, sink);
        const u32 method = word(rt, vtable + 0x10Cu);
        const u32 label = word(rt, kClassAddress);
        call_indirect(rt, ctx, method, {sink, label, 8u, out_a, out_b, 0u});
        return;
    }
    if (call_set(rt, ctx, 0x08A06EC8u, {model}) != 4u) return;
    const u32 s4 = call_set(rt, ctx, 0x08A06E94u, {model, 0u});
    const u32 s3 = call_set(rt, ctx, 0x08A06E94u, {model, 1u});
    const u32 s2 = call_set(rt, ctx, 0x08A06E94u, {model, 2u});
    const u32 s1 = call_set(rt, ctx, 0x08A06E94u, {model, 3u});
    ctx.gpr[20] = s4;
    ctx.gpr[19] = s3;
    ctx.gpr[18] = s2;
    ctx.gpr[17] = s1;
    u32 target = frame.at(0x90u);  // s5
    for (u32 i = 0u; i != 8u; ++i) {
        ctx.gpr[16] = i;
        const u32 high = ((i + (i >> 31)) >> 1) << 2;
        const u32 low = (i & 1u) << 2;
        const u32 row_b = table_b + low;
        const u32 row_a = table_a + high;
        const float p = float_at(rt, row_a + 0x10u);    // f1
        const float q = float_at(rt, row_b + 0x10u);    // f8
        const float base_b = float_at(rt, row_b);       // f2
        const float f6a = f22 * p;
        const float f4a = f21 * q;
        const float base_a = float_at(rt, row_a);       // f3
        const u32 slot = i << 3;
        put_float(rt, out_a + slot + 4u, base_b);
        const float inv_p = one - p;                    // f0
        const float inv_q = one - q;                    // f17
        put_float(rt, out_a + slot, base_a);
        put_float(rt, out_b + slot + 4u, f4a);
        put_float(rt, out_b + slot, f6a);
        const float a0 = float_at(rt, s4), a1 = float_at(rt, s4 + 4u), a2 = float_at(rt, s4 + 8u), a3 = float_at(rt, s4 + 0xCu);
        const float b0 = float_at(rt, s3), b1 = float_at(rt, s3 + 4u), b2 = float_at(rt, s3 + 8u), b3 = float_at(rt, s3 + 0xCu);
        const float c0 = float_at(rt, s1), c1 = float_at(rt, s1 + 4u), c2 = float_at(rt, s1 + 8u), c3 = float_at(rt, s1 + 0xCu);
        const float d0 = float_at(rt, s2), d1 = float_at(rt, s2 + 4u), d2 = float_at(rt, s2 + 8u), d3 = float_at(rt, s2 + 0xCu);
        const float pb0 = p * b0;
        const float pd0 = p * d0;
        const float pd3 = p * d3;
        const float pb1 = p * b1;
        const float pd1 = p * d1;
        const float pb2 = p * b2;
        const float pd2 = p * d2;
        float x0 = inv_p * a0;
        float y3 = inv_p * c3;
        float y0 = inv_p * c0;
        float x1 = inv_p * a1;
        float y1 = inv_p * c1;
        float x2 = inv_p * a2;
        float y2 = inv_p * c2;
        const float pb3 = p * b3;
        float x3 = inv_p * a3;
        x0 = x0 + pb0;
        y0 = y0 + pd0;
        x1 = x1 + pb1;
        y1 = y1 + pd1;
        x2 = x2 + pb2;
        y2 = y2 + pd2;
        x3 = x3 + pb3;
        y3 = y3 + pd3;
        y0 = q * y0;
        y1 = q * y1;
        x0 = x0 * inv_q;
        y3 = q * y3;
        x3 = x3 * inv_q;
        x1 = x1 * inv_q;
        x2 = x2 * inv_q;
        y2 = q * y2;
        x0 = x0 + y0;
        x1 = x1 + y1;
        x3 = x3 + y3;
        x2 = x2 + y2;
        put(rt, frame.at(0x110u), out_a);
        put(rt, frame.at(0x114u), out_b);
        put_float(rt, frame.at(0u), x0);
        put_float(rt, frame.at(4u), x1);
        put_float(rt, frame.at(8u), x2);
        put_float(rt, frame.at(0xCu), x3);
        call_set(rt, ctx, 0x089641C4u, {target, frame.at(0u)});
        target += 0x10u;
    }
    const u32 vtable = word(rt, sink);
    const u32 method = word(rt, vtable + 0x10Cu);
    const u32 label = word(rt, kClassAddress);
    call_indirect(rt, ctx, method, {sink, label, 8u, out_a, out_b, frame.at(0x90u)});
}

// --- 0x08904A8C (a0 = an output vector, a1 = a track object, a2 = a vector): finds the segment of the track the float at a2+4 falls in
// and writes a point on it (x, y + 3.0, z) into a0 ---------------------------------------------------------------------------------------
void func_08904A8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 track = ctx.gpr[5];
    const u32 input = ctx.gpr[6];
    SavedRegisters saved(ctx);
    SavedFloats saved_floats(ctx);
    GuestFrame frame(ctx, 0x100u);
    ctx.gpr[18] = out;
    ctx.gpr[19] = track;
    ctx.gpr[20] = input;
    // a pure copy of one float word, as lwc1 / swc1 do
    auto copy = [&](u32 from, u32 to) { put(rt, to, word(rt, from)); };

    float ratio = call_setf(rt, ctx, 0x08903504u, {track}, {float_at(rt, input + 4u)}).f0;  // f20
    const u32 index = call_setf(rt, ctx, 0x0890345Cu, {track}, {ratio}).v0;
    const u32 records = word(rt, track + 0x60u);  // a2
    const u32 total = half_at(rt, track + 0x3Cu);
    const u32 next = index + 1u;
    const u32 current = records + (index << 3) + (index << 5);  // s0
    u32 next_offset = 0u;
    if (next != total) next_offset = (next << 5) + (next << 3);
    const u32 following = next_offset + records;  // s1
    ctx.gpr[16] = current;
    ctx.gpr[17] = following;
    const float width = float_at(rt, current + 0x24u);  // f21
    const u32 s5 = frame.at(0x78u);
    ctx.gpr[21] = s5;
    ratio = call_setf(rt, ctx, 0x08903568u, {track, 0u}, {ratio, width}).f0;
    const float nearer = call_setf(rt, ctx, 0x08903568u, {track, 0u}, {float_at(rt, following + 0x24u), width}).f0;
    ratio = ratio / nearer;

    copy(current + 0x14u, frame.at(0x80u));
    copy(current + 0x10u, frame.at(0x7Cu));
    copy(current + 0xCu, frame.at(0x78u));
    copy(following + 0x10u, frame.at(0x64u));
    copy(following + 0xCu, frame.at(0x60u));
    copy(following + 0x14u, frame.at(0x68u));
    copy(following + 0xCu, frame.at(0x6Cu));
    copy(following + 0x10u, frame.at(0x70u));
    copy(following + 0x14u, frame.at(0x74u));
    call_set(rt, ctx, 0x08B4AA68u, {frame.at(0x60u), frame.at(0x60u), s5});
    copy(frame.at(0x68u), frame.at(0x50u));
    copy(frame.at(0x68u), frame.at(0x74u));
    copy(frame.at(0x60u), frame.at(0x48u));
    copy(frame.at(0x64u), frame.at(0x4Cu));
    copy(frame.at(0x60u), frame.at(0x6Cu));
    copy(frame.at(0x64u), frame.at(0x70u));
    call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0x48u)}, {ratio});
    copy(frame.at(0x48u), frame.at(0x54u));
    copy(frame.at(0x4Cu), frame.at(0x58u));
    copy(frame.at(0x50u), frame.at(0x5Cu));
    call_set(rt, ctx, 0x08B4AA34u, {frame.at(0x54u), frame.at(0x54u), s5});
    copy(frame.at(0x54u), frame.at(0x6Cu));
    copy(frame.at(0x58u), frame.at(0x70u));
    copy(frame.at(0x5Cu), frame.at(0x74u));
    // Which end of the segment the second vector points at: the first (+0x18..) set of points when the float at a2 is not negative.
    const u32 side = (0.0f <= float_at(rt, input)) ? 0x18u : 0u;
    copy(current + side, frame.at(0x78u));
    copy(current + side + 4u, frame.at(0x7Cu));
    copy(current + side + 8u, frame.at(0x80u));
    copy(following + side + 8u, frame.at(0x44u));
    copy(following + side, frame.at(0x3Cu));
    copy(following + side + 4u, frame.at(0x40u));
    const u32 s1 = frame.at(0x6Cu);
    const u32 s0 = frame.at(0x84u);
    ctx.gpr[17] = s1;
    ctx.gpr[16] = s0;
    call_set(rt, ctx, 0x08B4AA68u, {frame.at(0x3Cu), frame.at(0x3Cu), s5});
    copy(frame.at(0x40u), frame.at(0x28u));
    copy(frame.at(0x3Cu), frame.at(0x24u));
    copy(frame.at(0x44u), frame.at(0x2Cu));
    call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0x24u)}, {ratio});
    copy(frame.at(0x24u), frame.at(0x30u));
    copy(frame.at(0x28u), frame.at(0x34u));
    copy(frame.at(0x2Cu), frame.at(0x38u));
    call_set(rt, ctx, 0x08B4AA34u, {frame.at(0x30u), frame.at(0x30u), s5});
    copy(frame.at(0x30u), frame.at(0x18u));
    copy(frame.at(0x34u), frame.at(0x1Cu));
    copy(frame.at(0x38u), frame.at(0x20u));
    call_set(rt, ctx, 0x08B4AA68u, {frame.at(0x18u), frame.at(0x18u), s1});
    copy(frame.at(0x18u), frame.at(0x84u));
    copy(frame.at(0x1Cu), frame.at(0x88u));
    copy(frame.at(0x20u), frame.at(0x8Cu));
    float magnitude = call_setf(rt, ctx, 0x08B4A720u, {s0}).f0;
    magnitude = std::sqrt(magnitude);
    call_setf(rt, ctx, 0x08B4A5E8u, {s0}, {1.0f / magnitude});
    copy(frame.at(0x84u), frame.at(0xCu));
    const float spread = std::fabs(float_at(rt, input));
    copy(frame.at(0x88u), frame.at(0x10u));
    copy(frame.at(0x8Cu), frame.at(0x14u));
    call_setf(rt, ctx, 0x08B4A5E8u, {frame.at(0xCu)}, {spread});
    const u32 x_word = word(rt, frame.at(0xCu));
    const u32 y_word = word(rt, frame.at(0x10u));
    const u32 z_word = word(rt, frame.at(0x14u));
    put(rt, out, x_word);
    put(rt, out + 4u, y_word);
    put(rt, out + 8u, z_word);
    put(rt, frame.at(0u), x_word);
    put(rt, frame.at(4u), y_word);
    put(rt, frame.at(8u), z_word);
    call_set(rt, ctx, 0x08B4AA34u, {frame.at(0u), frame.at(0u), s1});
    const float lift = 3.0f;
    float y = float_at(rt, frame.at(4u));
    const u32 z_final = word(rt, frame.at(8u));
    const u32 x_final = word(rt, frame.at(0u));
    y = y + lift;
    put(rt, out + 8u, z_final);
    put(rt, out, x_final);
    put_float(rt, out + 4u, y);
    put(rt, frame.at(0xC4u), 0xFFFFFFFFu);
    put(rt, frame.at(0xC0u), 0u);
    const u32 found = call_set(rt, ctx, 0x08904A04u, {track, frame.at(0x90u), out});
    if (found != 0u) copy(frame.at(0x94u), out + 4u);
    ctx.gpr[2] = out;
}

} // namespace
} // namespace new_batch_main_124
} // namespace gt::game

#define NEWMAIN124(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_124::name, gt::ReturnKind::kind)

NEWMAIN124(0x088951FCu, func_088951FC, Word);
NEWMAIN124(0x08892C28u, func_08892C28, Word);
NEWMAIN124(0x0896AD58u, func_0896AD58, Void);
NEWMAIN124(0x08AD90ACu, func_08AD90AC, Word);
NEWMAIN124(0x08806C44u, func_08806C44, Void);
NEWMAIN124(0x088175B8u, func_088175B8, Word);
NEWMAIN124(0x08A4AFB4u, func_08A4AFB4, Void);
NEWMAIN124(0x088C2724u, func_088C2724, Void);
NEWMAIN124(0x089F2028u, func_089F2028, Word);
NEWMAIN124(0x08904A8Cu, func_08904A8C, Word);

