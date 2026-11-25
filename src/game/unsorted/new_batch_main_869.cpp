// New batch, main session 869: functions a previous draft got wrong (the gate's register / memory difference is fixed
// against the asm) or that were skipped twice, written by hand as structured C from `tools.disasm`. Draft only: the
// differential gate (fast, sabotage, synthetic) is run by the verifier, not here.
//
// Conventions, as in hot_batch_102: every `call_set` writes the argument registers the original sets before the `jal`
// (a0-a3, t0-t3); the ones it leaves alone keep the caller's value. The delay slot of a `jal` runs BEFORE the call; a
// `bnel` / `beql` / `bc1fl` / `bc1tl` delay slot only runs when the branch is taken; a plain branch's delay slot runs
// on both paths. `v0` (and `v1` / `f0` where a caller reads them) is set as the original leaves it on every exit path.
// The dead stack is not reproduced; the frames callees read or write are (`GuestFrame`, size = the original's
// `addiu sp, sp, -N`). Callee-saved registers are restored (`SavedRegs`) where the original saves them.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_869 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The argument registers set as the original sets them: a0-a3, t0-t3 in order.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call(rt, ctx, address, arguments);
}

// Callee-saved registers (s0-s7, fp) as the original's prologue / epilogue save and restore them.
struct SavedRegs {
    explicit SavedRegs(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx_.gpr[16u + i];
        saved_fp_ = ctx_.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8]{};
    u32 saved_fp_ = 0u;
};

inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
inline u32 swapped(u32 value) { return (value >> 24) | ((value >> 8) & 0xFF00u) | ((value << 8) & 0xFF0000u) | (value << 24); }
// The 13-bit signed field the ins / sh pairs carry.
inline s32 sext13(u32 value) { return static_cast<s32>(value << 19) >> 19; }

// ---- 0x088F2D98 ----
// a0 = an object whose signed half at +8 is the first record index and whose +4 word is the array of 0x40-byte records;
// f12 = a time delta. Walks the record chain (each record's word 0 bits 0..12 are the next index, negative ends it),
// integrating the object's per-limb velocity into each record's position / rotation, wrapping the angle at +0x14 into
// [0, 360), and relinking the two records it touched. v0 is the last word the finalisation wrote (or the half at +0x1C
// when the last step skipped it); a negative first index returns v0 untouched.
void func_088F2D98(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float f12 = ctx.fpr[12];
    s32 index = signed_half(rt, self + 8u);
    if (index < 0) return;
    const float gravity = std::bit_cast<float>(0xC11CF5C3u);
    const float scaled_gravity = f12 * gravity;
    const u32 table = 0x08C8A364u;
    const float step_small = std::bit_cast<float>(0x3C000000u);
    const float step_neg = std::bit_cast<float>(0xBB800000u);
    const float lift = std::bit_cast<float>(0x3F9A827Au);
    const float turn = std::bit_cast<float>(0x43B40000u);
    s32 previous = -1;
    u32 exit_v0 = 0u;
    bool ran = false;
    for (;;) {
        ran = true;
        const u32 base = word(rt, self + 4u);
        const u32 record = base + (static_cast<u32>(index) << 6);
        const u32 limb_bytes = byte_u(rt, record + 3u);
        const u32 record_word = word(rt, record);
        const u32 flags = byte_u(rt, record + 2u);
        const u32 limb = self + limb_bytes * 56u;
        const s32 next = sext13(record_word);
        float f8 = float_at(rt, limb + 0x5Cu);
        if ((flags & 0x20u) == 0u) f8 = -f8;
        const float f7 = float_at(rt, record + 8u);
        const float divisor = f7 < 1.0f ? 1.0f : f7;
        const float forward = float_at(rt, table + (flags & 7u) * 4u) / divisor;
        float angle = float_at(rt, record + 0x14u);
        float roll = float_at(rt, record + 0xCu);
        const float y_step = static_cast<float>(static_cast<s32>(byte_u(rt, record + 0x1Eu))) * step_small;
        const float z_step = static_cast<float>(static_cast<s32>(half_u(rt, record + 0x1Cu))) * step_neg;
        float raise = float_at(rt, record + 0x18u);
        angle = angle + (f8 * forward * f12);
        put_float(rt, record + 0x14u, angle);
        const bool angle_high = turn < angle;
        const float f0b = f12 * float_at(rt, limb + 0x54u) * y_step;
        const float f7b = f7 + f0b;
        put_float(rt, record + 8u, f7b);
        const float f0c = f12 * float_at(rt, limb + 0x60u) * z_step;
        roll = roll + f0c;
        put_float(rt, record + 0xCu, roll);
        const float f0d = float_at(rt, limb + 0x6Cu) * scaled_gravity;
        raise = raise + f0d;
        const float f1b = f12 * raise;
        put_float(rt, record + 0x18u, raise);
        const float heading = float_at(rt, record + 0x24u) + f1b;
        put_float(rt, record + 0x24u, heading);
        if (angle_high) {
            angle = angle - turn;
            put_float(rt, record + 0x14u, angle);
        }
        if (angle < 0.0f) put_float(rt, record + 0x14u, angle + turn);
        bool skip = false;
        if (0.0f < roll) {
            const float test = heading + f7b * lift;
            if (!(test < float_at(rt, record + 0x2Cu))) {
                previous = index;
                skip = true;
            }
        }
        if (skip) {
            exit_v0 = half_u(rt, record + 0x1Cu);
        } else {
            if (previous >= 0) {
                const u32 target = word(rt, self + 4u) + (static_cast<u32>(previous) << 6);
                u32 value = word(rt, target);
                value = (value & ~0x1FFFu) | (record_word & 0x1FFFu);
                put(rt, target, value);
            } else {
                put16(rt, self + 8u, static_cast<u32>(sext13(record_word)));
            }
            const u32 link = half_u(rt, self + 0xAu);
            u32 value = word(rt, record);
            value = (value & ~0x1FFFu) | (link & 0x1FFFu);
            put(rt, record, value);
            put16(rt, self + 0xAu, static_cast<u32>(index));
            exit_v0 = value;
        }
        if (next < 0) break;
        index = next;
    }
    if (ran) ctx.gpr[2] = exit_v0;
}

// ---- 0x08890068 ----
// Applies the load delta (the header's address minus its byte-swapped link address at +4) to the pointer fields of the
// module header and to the three section-record tables it points at, then records the header's new address. Returns 1
// when there was work (a non-zero delta) and 0 when the module sits where it was linked. Every pointer in these tables
// is stored byte-swapped.
u32 swap_relocate(Runtime &rt, u32 address, u32 delta) {
    const u32 value = swapped(word(rt, address)) + delta;
    put(rt, address, swapped(value));
    return value;
}

// ---- 0x0892AB78 ----
// a0 = the object, a1 = a selector 0..0x2C, a2 = a flag: the object's setter switch. When a2 is set and the selector
// is in range, the 45-entry table at 0x08C8A8E0 picks one small setter: a bit of the flag word at +0x20 / +0x24 /
// +0x28 / +0x38, a byte flag at +7, a halfword at +8 / +0xA / +0xC / +0xE, a signed byte at +2, or a bit of the byte at
// +0 together with the 16-bit counter at +4. v0 = the value written; the no-op entries leave v0 alone.

// ---- 0x0895F194 ----
// a0 = the object whose word at +8 is the track data, a1 = the segment index, a2 = the out xyz, a3 = a float out:
// samples the 12-byte point records at data + 0x610. With the state byte at +0x67A equal to 20 it copies the record of
// (byte + 0x678 - index, wrapped by 5) and returns (4 - index) * 20 / 80; with index 0 it runs 0x0895F174(a0, out) and
// returns 1; past index 4 it interpolates the records of (byte + 0x678 - index + 2, wrapped by 7) and (byte + 0x679) by
// state / 20 and returns 0; otherwise it copies the record of (byte + 0x678 - index + 1, wrapped by 6) and returns
// ((4 - index) * 20 + state) / 80. The float out is state / 60 (1/3 in the copy paths). v0 is never set: the result is
// in f0.
void func_0895F194(Runtime &rt, AllegrexContext &ctx) {
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    const u32 out_float = ctx.gpr[7];
    const u32 data = word(rt, object + 8u);
    const u32 state = byte_u(rt, data + 0x67Au);
    const auto record = [&](u32 slot) { return data + 0x610u + slot * 12u; };
    float result;
    if (state == 0x14u) {
        u32 slot = byte_u(rt, data + 0x678u) - index;
        if (static_cast<s32>(slot) < 0) slot += 5u;
        const u32 base = record(slot);
        put_float(rt, out, float_at(rt, base + 8u));
        put_float(rt, out + 4u, float_at(rt, base + 0xCu));
        put_float(rt, out + 8u, float_at(rt, base + 0x10u));
        put_float(rt, out_float, std::bit_cast<float>(0x3EAAAAABu));
        result = static_cast<float>(static_cast<s32>((4u - index) * 20u)) / 80.0f;
    } else if (index == 0u) {
        call_set(rt, ctx, 0x0895F174u, {object, out});
        const u32 fresh = byte_u(rt, data + 0x67Au);
        put_float(rt, out_float, static_cast<float>(static_cast<s32>(0x14u - fresh)) / 60.0f);
        result = 1.0f;
    } else if (index >= 5u) {
        const u32 offset = byte_u(rt, data + 0x678u) - index;
        u32 slot = offset + 2u;
        if (static_cast<s32>(slot) < 0) slot = offset + 7u;
        const u32 first = record(slot);
        const u32 second = record(byte_u(rt, data + 0x679u));
        const float ratio = static_cast<float>(static_cast<s32>(state)) / 20.0f;
        put_float(rt, out, float_at(rt, first + 8u) +
                              psp_mul(ratio, float_at(rt, second + 8u) - float_at(rt, first + 8u)));
        put_float(rt, out + 4u, float_at(rt, first + 0xCu) +
                                    psp_mul(ratio, float_at(rt, second + 0xCu) - float_at(rt, first + 0xCu)));
        put_float(rt, out + 8u, float_at(rt, first + 0x10u) +
                                    psp_mul(ratio, float_at(rt, second + 0x10u) - float_at(rt, first + 0x10u)));
        put_float(rt, out_float, static_cast<float>(static_cast<s32>(byte_u(rt, data + 0x67Au))) / 60.0f);
        result = 0.0f;
    } else {
        const u32 offset = byte_u(rt, data + 0x678u) - index;
        u32 slot = offset + 1u;
        if (static_cast<s32>(slot) < 0) slot = offset + 6u;
        const u32 base = record(slot);
        put_float(rt, out, float_at(rt, base + 8u));
        put_float(rt, out + 4u, float_at(rt, base + 0xCu));
        put_float(rt, out + 8u, float_at(rt, base + 0x10u));
        put_float(rt, out_float, std::bit_cast<float>(0x3EAAAAABu));
        result = static_cast<float>(static_cast<s32>((4u - index) * 20u + state)) / 80.0f;
    }
    ctx.fpr[0] = result;
}

// ---- 0x08890554 ----
// The inverse of func_08890068: undoes the load relocation. The byte-swapped address at +4 is the delta to subtract,
// the field is cleared, and the three tables and the header's pointer fields get the delta removed (the first table
// with 0x10-byte records at +8, the second with 0x20-byte records at +8, the third with 0x20-byte records whose +0x14
// pointer leads to 0x10-byte records of two pointers, plus the record's own +4 / +8 / +0xC / +0x14). Returns 0 when
// the header was not relocated, 1 otherwise.
inline void unswap_sub(Runtime &rt, u32 address, u32 delta) {
    put(rt, address, swapped(swapped(word(rt, address)) + delta));
}

// ---- 0x088800F4 ----
// a0 = the object (whose word at +0x74 is a resource), a1 / a2 = two arguments for the builders. Two probes refuse the
// call (their non-zero answer is returned). Otherwise a local holder is set up (0x08AC26D4), the word at self+0x38 is
// fetched and the tag of a2 selects a builder: MVAR walks it through 0x088852B4, SLOD / TWLM through the string
// helpers, ANIM sets bits 5..7 of a stack word and (when the fetched object is live) patches a string, and anything
// else returns the tag it compared. v0 = self on the paths that changed nothing.
void func_088800F4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x80u);
    const u32 self = ctx.gpr[4];
    const u32 argument1 = ctx.gpr[5];
    const u32 argument2 = ctx.gpr[6];
    u32 result = call_set(rt, ctx, 0x08870A38u, {word(rt, self + 0x74u) + 0x3Cu, argument1, argument2});
    if (result != 0u) {
        ctx.gpr[2] = result;
        return;
    }
    result = call_set(rt, ctx, 0x08884788u, {word(rt, self + 0x74u) + 0x28u, argument1, argument2});
    if (result != 0u) {
        ctx.gpr[2] = result;
        return;
    }
    const u32 holder = frame.at(0x14u);
    call_set(rt, ctx, 0x08AC26D4u, {holder, 0u});
    const u32 fetched = call_set(rt, ctx, 0x0886A9A0u, {self, 5u});
    const u32 live = call_set(rt, ctx, 0x08BA4A78u, {fetched});
    const u32 tag = call_set(rt, ctx, 0x08AC2690u, {argument2});
    u32 value;
    if (tag == 0x4D564152u) {
        call_set(rt, ctx, 0x08AC2730u, {frame.at(4u), argument2, 0u});
        call_set(rt, ctx, 0x08AC2964u, {frame.at(4u), frame.at(0x18u), 0u});
        value = call_set(rt, ctx, 0x088852B4u, {word(rt, self + 0x74u) + 4u, word(rt, frame.at(0x18u))});
    } else if (tag > 0x4D564152u) {
        if (tag == 0x534C4F44u) {
            call_set(rt, ctx, 0x08AC2730u, {frame.at(0xCu), argument2, 0u});
            call_set(rt, ctx, 0x08AC2910u, {frame.at(0xCu), frame.at(0u), 0u});
            call_set(rt, ctx, 0x08AC2730u, {frame.at(8u), argument2, 1u});
            call_set(rt, ctx, 0x08AC2964u, {frame.at(8u), frame.at(0x18u), 0u});
            const u32 has_value = byte_u(rt, frame.at(0u));
            const u32 stored = word(rt, frame.at(0x18u));
            const u32 argument = has_value != 0u ? stored : 0xFFFFFFFFu;
            value = call_set(rt, ctx, 0x088852DCu, {word(rt, self + 0x74u) + 4u, argument});
        } else if (tag == 0x54574C4Du) {
            call_set(rt, ctx, 0x08AC2730u, {frame.at(0x10u), argument2, 0u});
            call_set(rt, ctx, 0x08AC2910u, {frame.at(0x10u), frame.at(0u), 1u});
            value = call_set(rt, ctx, 0x08885288u, {word(rt, self + 0x74u) + 4u, byte_u(rt, frame.at(0u))});
        } else {
            value = 0x54574C4Du;
        }
    } else {
        if (tag == 0x414E494Du) {
            if (live == 0u) {
                value = 0x414E494Du;
            } else {
                const u32 patched = (word(rt, frame.at(0x1Cu)) & ~0xE0u) | 0x20u;
                put(rt, frame.at(0x1Cu), patched);
                value = patched;
            }
        } else {
            value = 0x414E494Du;
        }
    }
    ctx.gpr[2] = value;
}

// ---- 0x08995F54 ----
// a0 / a1 are a two-word command (1, 0xFFFF sets up the seven callback registrations through 0x08BF0BD4, 0, 0xFFFF
// tears them down through 0x08B28DC4). Every other pair answers 0xFFFF. v0 = the last registration's answer.
void func_08995F54(Runtime &rt, AllegrexContext &ctx) {
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 command = ctx.gpr[4];
    const u32 parameter = ctx.gpr[5];
    if (command == 1u) {
        if (parameter != 0xFFFFu) {
            ctx.gpr[2] = 0xFFFFu;
            return;
        }
        call_set(rt, ctx, 0x08B28DCCu, {0x08CF4598u});
        call_set(rt, ctx, 0x08A92114u,
                 {0x08CF4504u, 0x08CA42E8u, 0x190u, 0x08995C80u, 0x08999208u, 0x08995CA4u});
        call_set(rt, ctx, 0x08B2095Cu, {0x08CF4590u, 0x08C8C1C4u, 4u});
        call_set(rt, ctx, 0x0897D534u, {0x08CF44F4u, 0x08CA42E8u, 0x0899794Cu, 0x08997908u});
        u32 value;
        put(rt, frame.at(0x30u), word(rt, 0x08C8C220u));
        put(rt, frame.at(0x34u), word(rt, 0x08C8C224u));
        value = call_set(rt, ctx, 0x08BF0BD4u, {0x08CF4580u, 0x08CA42F4u, frame.at(0x30u), 0x08CF44F4u});
        put(rt, frame.at(0x28u), word(rt, 0x08C8C218u));
        put(rt, frame.at(0x2Cu), word(rt, 0x08C8C21Cu));
        value = call_set(rt, ctx, 0x08BF0BD4u, {0x08CF4570u, 0x08CA4300u, frame.at(0x28u), 0x08CF44F4u});
        put(rt, frame.at(0x20u), word(rt, 0x08C8C210u));
        put(rt, frame.at(0x24u), word(rt, 0x08C8C214u));
        value = call_set(rt, ctx, 0x08BF0BD4u, {0x08CF4560u, 0x08CA4314u, frame.at(0x20u), 0x08CF44F4u});
        put(rt, frame.at(0x18u), word(rt, 0x08C8C208u));
        put(rt, frame.at(0x1Cu), word(rt, 0x08C8C20Cu));
        value = call_set(rt, ctx, 0x08BF0BD4u, {0x08CF4550u, 0x08CA4328u, frame.at(0x18u), 0x08CF44F4u});
        put(rt, frame.at(0x10u), word(rt, 0x08C8C200u));
        put(rt, frame.at(0x14u), word(rt, 0x08C8C204u));
        value = call_set(rt, ctx, 0x08BF0BD4u, {0x08CF4540u, 0x08CA433Cu, frame.at(0x10u), 0x08CF44F4u});
        put(rt, frame.at(8u), word(rt, 0x08C8C1F8u));
        put(rt, frame.at(0xCu), word(rt, 0x08C8C1FCu));
        value = call_set(rt, ctx, 0x08BF0BD4u, {0x08CF4530u, 0x08CA434Cu, frame.at(8u), 0x08CF44F4u});
        put(rt, frame.at(0u), word(rt, 0x08C8C1F0u));
        put(rt, frame.at(4u), word(rt, 0x08C8C1F4u));
        value = call_set(rt, ctx, 0x08BF0BD4u, {0x08CF4520u, 0x08CA435Cu, frame.at(0u), 0x08CF44F4u});
        ctx.gpr[2] = value;
        return;
    }
    ctx.gpr[2] = 0xFFFFu;
    if (command == 0u && parameter == 0xFFFFu) tail_call(ctx, 0x08B28DC4u, {0x08CF4598u});
}

} // namespace
} // namespace new_batch_main_869
} // namespace gt::game

#define MAIN869(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_869::name, gt::ReturnKind::kind)
MAIN869(0x088F2D98u, func_088F2D98, Word);
MAIN869(0x0895F194u, func_0895F194, Float);
MAIN869(0x088800F4u, func_088800F4, Word);
MAIN869(0x08995F54u, func_08995F54, Word);
