// New batch 16: small functions the extra seeds made emittable, written by hand from `tools.disasm`.
// Delay slots are read as "before the branch / call". The callee-saved registers the original keeps values in are set
// where the asm sets them (a callee may read them) and put back at the end; `v0` (or `f0` for the float ones) ends as
// the asm leaves it on every path. No names: none has evidence.
// Audits: `tools.sp_loads` reports one load the C++ models (func_08A41414 reloads s0 from sp + 0, the word a callee
// stored there); `tools.literal_check` needs the eight `--allow` literals listed in the batch report.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_16 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call: v0 / v1 / f0 of the callee are what the next instruction sees.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// `jalr` through the object's vtable: v1 = the table, v0 = the slot, then the call.
inline u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
inline u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
inline u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// s0-s7, fp and ra are callee-saved: the body sets them where the asm sets them (a callee may read them) and the
// caller's values come back. f20-f31 are callee-saved too; the versions that touch them use SavedFpr.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

class SavedFpr {
public:
    explicit SavedFpr(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFpr() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFpr(const SavedFpr &) = delete;
    SavedFpr &operator=(const SavedFpr &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// 0x08AD6854 (a0 = object, a1 = count, a2 = stride): the count padded to a multiple of four times the stride, and
// three steps over it: 0x08C3E138(object, count, stride), 0x08B2AE10(object + 0x1C, stride), 0x08B2B5E0(bytes); then
// 0x08B2B760(object, answer, bytes, padded) finishes. v0 = its answer.
void func_08AD6854(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 stride = ctx.gpr[6];
    const u32 padded = (count + 3u) & ~3u;
    const u32 bytes = stride * padded;
    u32 answer = 0u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[16] = padded;
        ctx.gpr[17] = stride;
        ctx.gpr[18] = self;
        ctx.gpr[19] = bytes;
        call_keep(rt, ctx, 0x08C3E138u, {self, count, stride});
        call_keep(rt, ctx, 0x08B2AE10u, {self + 0x1Cu, stride});
        answer = call_keep(rt, ctx, 0x08B2B5E0u, {bytes});
    }
    ctx.gpr[2] = answer;
    tail_call(ctx, 0x08B2B760u, {self, answer, bytes, padded});
}

// 0x08B0A9F4 (a0 = object): locks object + 0x48 (0x08B344E0), walks a 16-slot ring at object + 8 from the index at
// object + 4 up to (not including) the one at object + 0, keeps the last slot read, unlocks (0x08B3457C) and returns
// that word. The load sits in the loop's branch delay slot, so it runs on the exit iteration too.

// 0x08B2DEE4: three guest calls on three stack objects (0x08C7B008(&first), 0x08C7B000(&first),
// 0x08C7B010(&third, &second)); a nonzero answer either time returns -1 in v0 and v1, everything zero goes on in
// 0x08B2DE94(&third) and returns its answer.

// 0x08B2F060: one-time setup behind the byte at 0x08D6915C. The byte gates the body: a nonzero byte returns it in
// v0; zero stores 1 there and runs 0x08B2E314(0, 0), 0x08B2E134(answer) and 0x08B1B92C(&block + 8, &block, answer, 0),
// keeping the last answer at 0x08D69158. The block is zero, 0x1000, 0x08B2EFF8 and zero.
void func_08B2F060(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 state = byte_u(rt, 0x08D6915Cu);
    ctx.gpr[2] = state;
    if (state != 0u) return;
    put8(rt, 0x08D6915Cu, 1u);
    put(rt, frame.at(4u), 0x1000u);
    put(rt, frame.at(8u), 0x08B2EFF8u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0u), 0u);
    const u32 first = call_set(rt, ctx, 0x08B2E314u, {0u, 0u});
    const u32 second = call_set(rt, ctx, 0x08B2E134u, {first});
    const u32 result = call_set(rt, ctx, 0x08B1B92Cu, {frame.at(8u), frame.at(0u), second, 0u});
    put(rt, 0x08D69158u, result);
}

// 0x08B56D4C (a0 = object): when bit 10 of the word at +0xD1C is set, clear it, take the matrix 0x08B56A88(object)
// and the one at *(object + 0xD00), multiply them (vmmul.q) and store the four columns at +0x840 .. +0x870 through the
// VFPU. v0 = object + 0x800 either way.

// 0x08B62498 (a0 = a block + 0x50, a1 / a2 = two words): fills the exception record at a0 - 0x50 (vtable 0x08B62450,
// the two words, the two globals 0x08CE0E08 / 0x08CE0E54 and the constant 0x432B2B00 / 0x474E5543), then calls
// 0x08B758A4(record + 0x30, a1, a2, record), 0x08B62198(record + 0x30) and 0x08B6459C(). The last two do not return.

// 0x08B643A8 (a0 = two bytes): returns 1 the first time (byte 0 is zero and byte 1 is zero, which becomes 1), 0 when
// byte 0 is set; a nonzero byte 1 means an initialization is already running: it is bumped, a four-byte exception
// object is built (0x08B61E98(4), vtable 0x08CDC0D0) and thrown (0x08B62498(object, 0x08CDC098, 0x08B64364)).
void func_08B643A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    if (sext8(byte_u(rt, self)) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const s32 state = static_cast<s32>(static_cast<std::int8_t>(byte_u(rt, self + 1u)));
    put8(rt, self + 1u, static_cast<u32>(state + 1));  // the branch's delay slot
    if (state != 0) {
        const u32 object = call_set(rt, ctx, 0x08B61E98u, {4u});
        put(rt, object, 0x08CDC0D0u);
        call_keep(rt, ctx, 0x08B62498u, {object, 0x08CDC098u, 0x08B64364u});
        return;
    }
    ctx.gpr[2] = 1u;
}

// 0x08B7B34C (a0 = a saved-register block, a1 = a value): reloads s0-s7, sp, fp, ra and f20-f31 from the block and
// returns a1 (1 when a1 is zero). The restored ra is the function's return.

// 0x08B862D8 (a0 = object): stores vtable 0x08CB7218, releases the string at +0x14 unless it is the shared empty
// representation 0x08D74FD0 (0x08B60804(data - 4, -1); at zero or below also 0x08B8542C(data - 0xC, sp)) and finishes
// with 0x08A780B0(object). v0 = its answer.
void func_08B862D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 data = word(rt, self + 0x14u);
    const u32 count = data - 0xCu;
    ctx.gpr[17] = count;
    put(rt, self, 0x08CB7218u);
    ctx.gpr[2] = 0x08CB7218u;
    if (count != 0x08D74FD0u) {
        const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
        if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {count, frame.at(0u)});
    }
    call_keep(rt, ctx, 0x08A780B0u, {self});
}

// 0x08BD3008 (a0 = this, a1 / a2 = two values): 0x08AF8904(*(this + 4), 0), then 0x08BD2DC4(value, this[0x24])
// and 0x08AFA170(*(table), second, table, answer) where table = *(this + 4) finishes. v0 = its answer.
void func_08BD3008(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = second;
    ctx.gpr[18] = first;
    ctx.gpr[16] = self;
    const u32 table = word(rt, self + 4u);
    call_set(rt, ctx, 0x08AF8904u, {word(rt, table), 0u});
    ctx.gpr[17] = table;
    const u32 count = static_cast<u32>(static_cast<std::int8_t>(byte_u(rt, self + 0x24u)));
    const u32 answer = call_set(rt, ctx, 0x08BD2DC4u, {first, count});
    ctx.gpr[16] = word(rt, table);  // the delay slot of the call: s0 = *table
    tail_call(ctx, 0x08AFA170u, {word(rt, table), second, table, answer});
}

// 0x08BD311C: the same as 0x08BD3008 with the signed byte at this + 0x14 instead of + 0x24.
void func_08BD311C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = second;
    ctx.gpr[18] = first;
    ctx.gpr[16] = self;
    const u32 table = word(rt, self + 4u);
    call_set(rt, ctx, 0x08AF8904u, {word(rt, table), 0u});
    ctx.gpr[17] = table;
    const u32 count = static_cast<u32>(static_cast<std::int8_t>(byte_u(rt, self + 0x14u)));
    const u32 answer = call_set(rt, ctx, 0x08BD2DC4u, {first, count});
    ctx.gpr[16] = word(rt, table);
    tail_call(ctx, 0x08AFA170u, {word(rt, table), second, table, answer});
}

// 0x08BE6D44 (a0 = destination of `a1` 0x28-byte records, a2 = source): copies each source record's words +4 .. +0x24
// into the destination (word +0 = vtable 0x08CCE6E0), skipping the copy for a destination of zero. A count of zero
// returns immediately with v0 = 0x4D0000; otherwise v0 = the last word read from source + 4.

// 0x08C2C46C (a0 = first, a1 = end, a2 = output): copies 0x0C-byte records from a0 up to a1 into a2 (0x08A6972C
// prepares each, its word +4 is copied and 0x08A76ABC copies +8). The output pointer is returned; a destination of
// zero skips the copy but still advances, and a0 == a1 returns the output untouched.

// 0x08C48FC4 (a0 = the end of a halfword array, a1 = an index, a2 = two pointers): insertion of a1 into the sorted
// halfword run that ends at a0, using the floats at 0x98 of the records *(a2) and *(a2 + 4) index by record index.
// The run is walked backwards; the answer's slot is the first one whose float is not below the new one's.

// 0x08C7A730 (a0 = object): vtables 0x08CDC454 (at 0), 0x08CDC468 (at 8) and 0x08CDC47C (at 0xB0), 0x08C786B4(object + 0xC),
// 0x08C553B0(object, 0x08CDC424), vtable 0x08CDB088 at 0xB0, and 0x08B6543C(object + 0xB0) finishes.

// 0x0885E940 (a0 = this): ten steps on the object's word + 0x5AC, each `setter` taking the previous `getter`'s
// answer, then 0x08866FA8(*this) finishes: 0x088D6E9C(word(*this + 0x5AC)), 0x088CBFD4, 0x088CBE6C(answer),
// 0x088CA548, 0x088C9780(answer), 0x08916474, 0x08916194(answer), 0x088C7F64, 0x088C78B4(answer), 0x08B1C47C.

// 0x0886DF90 (a0 = this, a1 = a value): 0x0886A9A0(this, 6) gates a preparatory 0x0886AAA4(this, *(this + 0x70) + 0x28);
// then 0x08874BE0(*(this + 0x70) + 0x28, value) answers 1, or answers 0 after 0x0886AA0C(this, 6). v0 = that 1 or 0.

// 0x08882928 (a0 = object): stores vtable 0x08CC1460, 0x08882854(*(object + 0xC), *(object + 8)) prepares the member
// at +0xC, the helper at +0x10 is released through its first virtual (0x08A5BA08-like arguments: its table and
// *(object + 0xC)) and both words cleared, vtable 0x08CBF428 is stored and 0x08B64654(object) finishes.

// 0x0889844C (a0 = object): stores vtable 0x08CC20A8, releases the helpers at +0x38 and +0x30 through slot 4 of their
// tables (their word +4) and clears the slots, 0x088F897C(object) runs and 0x08B64654(object) finishes.
void func_0889844C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CC20A8u);
    ctx.gpr[2] = 0x08CC20A8u;
    const u32 first = word(rt, self + 0x38u);
    if (first != 0u) {
        const u32 table = word(rt, first);
        ctx.gpr[2] = table;
        ctx.gpr[3] = word(rt, table + 4u);
        call_keep(rt, ctx, word(rt, table + 4u), {first});
    }
    put(rt, self + 0x38u, 0u);
    const u32 second = word(rt, self + 0x30u);
    if (second != 0u) {
        const u32 table = word(rt, second);
        ctx.gpr[2] = table;
        ctx.gpr[3] = word(rt, table + 4u);
        call_keep(rt, ctx, word(rt, table + 4u), {second});
    }
    put(rt, self + 0x30u, 0u);
    call_set(rt, ctx, 0x088F897Cu, {self});
    tail_call(ctx, 0x08B64654u, {self});
}

// 0x088CFA9C (a0 = this, f12 / f13 = two floats): 0x088D390C(*(this + 0x61C), &first, &second, 6) reads a pair of
// floats, then 0x088D2DE0(*(this + 0x61C), f12 = the first argument minus the pair's second word) answers one, and
// f0 is its absolute value (c.lt.s against zero, so a NaN is left alone).

// 0x088E798C (a0 = this, a1 = a value, f13 = a float): 0x088D390C(*(this + 0x61C), &first, &second, 6) primes a pair,
// then 0x088E7894(this, value, &first, &second) runs with f12 = the float argument.

// 0x089B0044 (a0 = object, a1 = a new pointer): under the lock at 0x08CF4ACC, keeps one reference of the new pointer
// (0x08A5BA08), releases the old one at +0x40 (0x08A5B930), stores the new one, announces it (0x089AFE84(object, new))
// and unlocks (0x08B3457C(0x08CF4ACC)). v0 = the unlock's answer.
void func_089B0044(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    constexpr u32 kLock = 0x08CF4ACCu;
    ctx.gpr[17] = self;
    ctx.gpr[16] = value;
    call_set(rt, ctx, 0x08B344E0u, {kLock});
    if (value != 0u) call_set(rt, ctx, 0x08A5BA08u, {value});
    const u32 old = word(rt, self + 0x40u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, self + 0x40u, value);
    call_set(rt, ctx, 0x089AFE84u, {self, value});
    tail_call(ctx, 0x08B3457Cu, {kLock});
}

// 0x089CFD28 (a0 = this, a1 = a value, a2 = another): 0x08969188(&local, &word) prepares a stack pair (the word is the
// third argument), 0x089CDDF8(this, value, &local) runs, the local is released (0x08A5B9EC(&local)) and
// 0x089CFCCC(this, *(this + 0x198)) answers the state word stored at +0x1A0 (with +0x1C0 = 2).

// 0x089DA4E0 (a0 = this, a1 = a value): when the byte at +0x8C is set, slot 0x1C8(this) answers a frame, slot 0x1D0
// (this, answer - value - 1) a record and f0 = its float at +0xC; otherwise slot 0x1D0(this) answers a record and f0 =
// its float at +0x10.
void func_089DA4E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = value;
    ctx.gpr[17] = self;
    if (byte_u(rt, self + 0x8Cu) != 0u) {
        const u32 table = word(rt, self);
        ctx.gpr[3] = table;
        ctx.gpr[2] = word(rt, table + 0x1C8u);
        const u32 position = call_keep(rt, ctx, word(rt, table + 0x1C8u), {self}) - value;
        const u32 entry = word(rt, table + 0x1D0u);
        ctx.gpr[16] = entry;
        ctx.gpr[2] = position;  // v0 as the original leaves it before the jalr
        ctx.gpr[3] = table;
        const u32 record = call_keep(rt, ctx, entry, {self, position - 1u});
        ctx.fpr[0] = float_at(rt, record + 0xCu);
        return;
    }
    const u32 table = word(rt, self);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x1D0u);
    const u32 record = call_keep(rt, ctx, word(rt, table + 0x1D0u), {self});
    ctx.fpr[0] = float_at(rt, record + 0x10u);
}

// 0x08A3DBDC (a0 = object): stores vtable 0x08CD13B0, 0x08A3D718(*(object + 0x14)) and the helper at +0x18 is released
// through its first virtual (with *(object + 0x14)), both words cleared; 0x08A780B0(object) runs and
// 0x08A5BC54(object, 0x1C, 4) finishes.

// 0x08A41414 (a0 passed on, a1 / a3 = two values): 0x08A40E90(&local, a3) and 0x08A40CF8(&object, a1) prepare a pair,
// an empty object is reported (0x08A5BB7C(0x08CADBCC)), 0x08A413B0(object, &local) runs and both locals are released.

// 0x08A56630 (a0 = out, a1 = holder, a2 = value): the helper at *(holder + 0x14) (reported when null through
// 0x08A5BB7C(0x08CAF798)) answers through slot 0x140 with (out, helper, holder, value); v0 = out.
void func_08A56630(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = out;
    ctx.gpr[18] = holder;
    ctx.gpr[17] = value;
    const u32 helper = word(rt, holder + 0x14u);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    const u32 table = word(rt, helper);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x140u);
    call_keep(rt, ctx, word(rt, table + 0x140u), {out, helper, holder, value});
    ctx.gpr[2] = out;
}

// 0x08A56F08 (a0 = this, a1 = a holder, a2 = a value): the helper at *holder (reported when null through
// 0x08A5BB7C(0x08CAF778)) answers through slot 0x5C into a stack word; 0x08A565C0(this, &word, value) then
// 0x08A69D68(&word) release it.

// 0x08A5DF58 (a0 = out, a2 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB0080))
// answers through slot 0x50, 0x08A5DEE8(&word, a1, answer, table) builds a stack word, 0x08A76ABC(out, &word) copies it
// and 0x08A5B9EC(&word) releases it. v0 = out.

// 0x08A78F54 (a0 = this, a1 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB0E38))
// answers through slot 0x40, 0x08A83160(&word, a1, answer, table) builds a stack word, and unless this is the stack
// itself 0x08A5BAD8(this, word) copies it; 0x08A5B9EC(&word) releases it.

// 0x08A8B108 (a0 = out, a2 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB1BE8))
// answers through slot 0x58, 0x08A8B098(&word, a1, answer, table) builds a stack word, 0x08A76ABC(out, &word) copies it
// and 0x08A5B9EC(&word) releases it. v0 = out.

// 0x08A93500 (a0 = this, a1 = a holder): the object at *(holder + 4)[0x30] (reported when null through
// 0x08A5BB7C(0x08CB204C)), 0x08B88F44(this, object + 0x28) runs, and when *this[0xC] is zero 0x08B8A4C0(this,
// 0x08CB2074, 9) reports it. v0 = this.
void func_08A93500(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    const u32 object = word(rt, word(rt, ctx.gpr[5] + 4u) + 0x30u);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB204Cu});
    call_set(rt, ctx, 0x08B88F44u, {self, object + 0x28u});
    if (word(rt, word(rt, self) - 0xCu) == 0u)
        call_set(rt, ctx, 0x08B8A4C0u, {self, 0x08CB2074u, 9u});
    ctx.gpr[2] = self;
}

// 0x08A9893C (a0 = this): stores vtable 0x08CB7108, 0x08B856B0(*this[0x14], this[0x18], sp) prepares the vector at
// this + 0x14; when its base is nonzero its byte size (0x08A5BC54(base, ((this[0x1C] - base) >> 2) << 2, 4)) is
// released and 0x08A780B0(this) finishes.
void func_08A9893C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = self + 0x14u;
    put(rt, self, 0x08CB7108u);
    call_set(rt, ctx, 0x08B856B0u, {word(rt, self + 0x14u), word(rt, self + 0x18u), frame.at(0u)});
    const u32 base = word(rt, self + 0x14u);
    const u32 size = static_cast<u32>(static_cast<s32>(word(rt, self + 0x1Cu) - base) >> 2) << 2u;
    if (base != 0u) call_set(rt, ctx, 0x08A5BC54u, {base, size, 4u});
    call_set(rt, ctx, 0x08A780B0u, {self});
}

// 0x08A989B4: the same body as 0x08A9893C.

// 0x08A9BED4 (a0 = this, a1 / a2 = two values): under the lock at this + 0x4C, the object at *(this + 0x38)
// (reported when null through 0x08A5BB7C(0x08CB2570)) runs 0x08A7CBA8(object, a1, a2); v0 = the unlock's answer.

// 0x08AC2BD8: the three registered static teardown slots (0x08CFFCA8, 0x08CFFCA4, 0x08CFFCA0). The first one's
// function pointer (with its word +4 in a0) runs; when the second is set 0x08AC2BAC(second) finishes; otherwise the
// third object's slot +0x58 finishes (with the object in a0).

// 0x08AC7F94 (a0 = this): under the lock at this + 8 (0x08B34888 / 0x08B34B24), the byte at +0x566 becomes 1 and a
// one-time record is built through 0x08B2E314(6, 1), 0x08B2E134(answer) and 0x08AC79B0(&block, answer) with the block
// {0x08AC8484, this}; the answer is kept at +0x56C.

// 0x08AF2D20: rotates the three words of the record 0x08AF296C() answers through the three stack words at sp, then
// 0x08C3F8BC(first, second) runs and a nonzero first word is released (0x08B35254).

// 0x08B1297C (a0 = this, a1 = a holder): the state record at *(holder + 0x10); a non-negative first word is released
// (0x08B33A24), a positive word +0x10 adds this[0x254] to word +4, and 0x08B11F8C(this, this + 0xAC,
// word + 4 + word + 8) answers: word +0x10 becomes -1 and word 0 the answer.

// 0x08B32228 (a0 = a value): one-time path behind the byte at 0x08D6916C. A nonzero byte returns 1; zero runs
// 0x08B2EEB8(0x08D69124), and 0x08B321C8(value) answers: nonzero finishes 0x08B2EEB8(0x08D69110) and returns 1,
// zero finishes 0x08B2EE2C(0x08D69124) and returns 0.
void func_08B32228(Runtime &rt, AllegrexContext &ctx) {
    const u32 value = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = value;
    if (byte_u(rt, 0x08D6916Cu) != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    call_set(rt, ctx, 0x08B2EEB8u, {0x08D69124u});
    if (call_set(rt, ctx, 0x08B321C8u, {value}) != 0u) {
        call_set(rt, ctx, 0x08B2EEB8u, {0x08D69110u});
        ctx.gpr[2] = 1u;
        return;
    }
    call_set(rt, ctx, 0x08B2EE2Cu, {0x08D69124u});
    ctx.gpr[2] = 0u;
}

// 0x08B38C78 (a0 = the record, a1 = a halfword, a2 / a3 = two shared records): vtables 0x08CDA928, 0x08CDA8E8 and
// 0x08CDA8F8, the halfword at +8, zeros at +4 and +0x10, the shared records at +0x18 / +0x20 with one more reference
// on each (their word +4) and the vtable 0x08CDA908 at +0x14 / +0x1C.
void func_08B38C78(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 half = ctx.gpr[5];
    const u32 first = word(rt, ctx.gpr[6] + 4u);
    const u32 second = word(rt, ctx.gpr[7] + 4u);
    put(rt, self, 0x08CDA928u);
    ctx.gpr[2] = 0x08CDA928u;
    rt.memory().store16(self + 8u, static_cast<std::uint16_t>(half));
    put(rt, self + 4u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0xCu, 0x08CDA8E8u);
    put(rt, self + 0x14u, 0x08CDA8F8u);
    put(rt, self + 0x18u, first);
    ctx.gpr[2] = 0x08CDA8E8u;
    if (first != 0u) {
        const u32 count = word(rt, first + 4u) + 1u;
        put(rt, first + 4u, count);
        ctx.gpr[2] = count;
    }
    put(rt, self + 0x1Cu, 0x08CDA8F8u);
    put(rt, self + 0x14u, 0x08CDA908u);
    put(rt, self + 0x20u, second);
    if (second != 0u) {
        const u32 count = word(rt, second + 4u) + 1u;
        put(rt, second + 4u, count);
        ctx.gpr[2] = count;
    }
    put(rt, self + 0x1Cu, 0x08CDA908u);
}

// 0x08B628BC (a0 = this, a1 / a2 / a3 = three values): 0x08B62884(this, a1, a2, a3) first; when it answers zero the
// object at *(this + 8) runs its slot 0x18 with itself. v0 / v1 = 1, or the slot's answer.

// 0x08B75204 (a0 = table, a1 = this): 0x08B7450C(table) first; the index at this + 0x9BC below 0x133 selects a byte of
// 0x08D72BB4: kind 4 clears bit 0 of the word at table + index * 4 and stores it at table + 0x4D0 and back; every other
// index or kind goes on in 0x08B77B04 (which does not return).

// 0x08C201AC (a0 passed on, a1 = a value): 0x08A6158C(value) twice; the object at the second answer's word 0 (reported
// when null through 0x08A5BB7C(0x08CB0284)) runs slot 0xC4 (object, first answer), then 0x08A61584(value, first
// answer) finishes.

// 0x08C79ED8 (a0 = object): vtables 0x08CDC374 (at 0) and 0x08CDC388 (at 0xA8), 0x08C786B4(object + 4), then the two
// words at 0x08CDC35C / 0x08CDC360 are used to fix the object's own table: word 0 = first, word at first - 0xC = second,
// vtable 0x08CDB088 at 0xA8 and 0x08B6543C(object + 0xA8) finishes.

// 0x088056F8 (a0 / a1 passed on): the teardown chain 0x088057CC(a0), 0x08805B1C(a0, a1), 0x08805774, 0x08805B88,
// 0x08805B90, 0x08AC2BD8, 0x088057A8, 0x08805578, 0x088057F8, 0x08ABCE2C and 0x08AC4398 finishes.

// 0x08805C70: the start-up sequence 0x08B2DBF0(0x14D), FPU control 31 cleared, then 0x08AD0974, 0x08AD0900,
// 0x08B33218, 0x08AD08A4, 0x08AD0820, 0x08B2DD2C, 0x08B33688, 0x08B34060, 0x08AD083C, 0x08B0EBF0, 0x08AB9B04 and
// 0x08B1F178 finishes.

// 0x0880AF30 (a0 = object): vtables 0x08CBA3A0 (at 0) and 0x08CBA5D8 (at 0x38), the helper at +0xEC is released
// through its first virtual (with *(object + 0xE8)), both words cleared, 0x0896BC0C(object) runs and
// 0x08A5BC54(object, 0xF0, 4) finishes.
void func_0880AF30(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0xE8u;
    put(rt, self, 0x08CBA3A0u);
    put(rt, self + 0x38u, 0x08CBA5D8u);
    const u32 helper = word(rt, self + 0xECu);
    if (helper != 0u) {
        const u32 table = word(rt, helper);
        ctx.gpr[3] = table;
        ctx.gpr[2] = word(rt, table);
        call_keep(rt, ctx, word(rt, table), {helper, word(rt, self + 0xE8u)});
        put(rt, self + 0xECu, 0u);
        put(rt, self + 0xE8u, 0u);
    }
    call_set(rt, ctx, 0x0896BC0Cu, {self});
    tail_call(ctx, 0x08A5BC54u, {self, 0xF0u, 4u});
}

// 0x08860500 (a0 = this, a1 / a2 / a3 = three values): marks *this[0x3FDB] = 1, runs 0x08860080(*this), then
// 0x088D7DFC(*(*this + 0x5AC), a1, a2 & 0xFF, *this + 0x3E7C) and clears the mark.

// 0x0887B060 (a0 = object): 0x08B9AB4C(&pair) and 0x08B9A958(object + 0x10, &pair), 0x08B8ED98(&other), then
// 0x08B8FE44(pair[0], pair[1], sp) and a nonzero pair[0] is released (0x08B35254); 0x0896275C(object) runs and the
// words at +0x30 / +0x2C are cleared.

// 0x088A117C (a0 = this): under the lock at this + 4, the record at *(this + 0x3C) plus i * 0x7C runs
// 0x088A1004 for every i below *(this + 0x38); v0 = the unlock's answer.
void func_088A117C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 lock = self + 4u;
    ctx.gpr[19] = lock;
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    if (static_cast<s32>(word(rt, self + 0x38u)) > 0) {
        u32 index = 0u;
        u32 offset = 0u;
        do {
            const u32 record = word(rt, self + 0x3Cu) + offset;
            ++index;
            call_set(rt, ctx, 0x088A1004u, {record});
            offset += 0x7Cu;
        } while (static_cast<s32>(index) < static_cast<s32>(word(rt, self + 0x38u)));
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// 0x088D2C8C (a0 = this, a1 = a value, f12 = a scale): a zero 0x088DB5C4(*this) returns f0 = 0; otherwise the second
// answer feeds 0x088FAC80(&pair, answer, value) with the scale, and f0 is the pair's word +4 clamped at zero from below.
void func_088D2C8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const float argument = ctx.fpr[12];
    SavedRegisters saved(ctx);
    SavedFpr saved_fpr(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[17] = value;
    ctx.gpr[16] = self;
    ctx.fpr[20] = argument;
    if (call_set(rt, ctx, 0x088DB5C4u, {word(rt, self)}) == 0u) {
        ctx.fpr[0] = 0.0f;
        return;
    }
    const u32 second = call_set(rt, ctx, 0x088DB5C4u, {word(rt, self)});
    call_setf(rt, ctx, 0x088FAC80u, {frame.at(0u), second, value}, {argument});
    const float value_at = float_at(rt, frame.at(4u));
    ctx.fpr[0] = value_at < 0.0f ? 0.0f : value_at;
}

// 0x088E43D0 (a0 = object, a1 / a2 = two values): the object at *a0 runs its slot 0x1C (itself, a1); the helper at
// *(a0 + 4), when it exists, runs slot 0x1C of its own table (helper, a1, a2) as the tail.

// 0x088FE99C (a0 = object): under the lock at +0x10 (0x08B344E0 / 0x08B3457C): the byte at +0xB is cleared,
// 0x088FE6BC(object) and 0x088FE308(object) run and 0x08B28DC4(object + 0x10) finishes; then the helper at +4 is
// released through its first virtual (with *object) and both words cleared.

// 0x08927A0C (a0 = this, a1 = a value): 0x08927120() answers an object whose slot 0x24 answers a number, which
// 0x08B4E734(value, number) receives; f12 becomes pi / 2 and, when the byte at this + 0x2107 is set, 0x08B4F47C(value)
// finishes.

// 0x08986058 (a0 = object): stores vtable 0x08CCE1A8, releases the string at +0x30 unless it is the shared empty
// representation 0x08D74FD0 (0x08B60804(data - 4, -1); at zero or below also 0x08B8542C(data - 0xC, sp)), then
// 0x089ECC78(object) and 0x08B64654(object).

// 0x08A2E748 (a0 = object): stores vtable 0x08CBE590 and, when both the pointer at 0x08CF6648 and the byte at
// 0x08CF664C are set, 0x08B097AC(object) and 0x08B64654(object) run and both globals are cleared; then
// 0x08A780B0(object) finishes.

// 0x08A4D308 (a0 = object): the member at +0x98, when set, runs 0x08B61FC0; the member at +0x78 is released
// (0x08A5058C, and 0x08C1CC2C(member + 0x14, member[0x1C]) then 0x08B64654(member)), cleared, and 0x08B2EE2C(
// 0x08CF68F0) runs before 0x08A50AB0(object) finishes.

// 0x08A70A48 (a0 = object): the same string release as 0x08B862D8 on the word at +0x18, vtable 0x08CCD860, then
// 0x08A5B9EC(object + 0x14) and 0x08A780B0(object).
void func_08A70A48(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 data = word(rt, self + 0x18u);
    const u32 count = data - 0xCu;
    ctx.gpr[17] = count;
    put(rt, self, 0x08CCD860u);
    ctx.gpr[3] = 0x08CCD860u;  // the asm keeps the vtable in v1 (the store's source)
    if (count != 0x08D74FD0u) {
        const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
        if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {count, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {self + 0x14u});
    call_set(rt, ctx, 0x08A780B0u, {self});
}

// 0x08AA3568 (a0 = out, a2 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB2BB8))
// answers through slot 0x54; 0x08B66524(f0) -> f12, 0x08A87EEC(&word) with f12 = the answer, and unless out is the
// stack itself 0x08A5BAD8(out, word) copies it; 0x08A5B9EC(&word) releases it.

// 0x08AA3758: the same as 0x08AA3568 with slot 0x54 and 0x08B66D54 in place of 0x08B66524.

// 0x08AA37D4: the same as 0x08AA3568 with slot 0x54 and 0x08B65F0C in place of 0x08B66524.

// 0x08AA3CD8: the same as 0x08AA3568 with slot 0x54 and 0x08B6759C.

// 0x08AA3DD0: the same as 0x08AA3568 with slot 0x54 and 0x08B663B4.
void func_08AA3DD0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 holder = word(rt, ctx.gpr[6]);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2BB8u});
    const u32 table = word(rt, holder);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x54u);
    call_keep(rt, ctx, word(rt, table + 0x54u), {holder});
    ctx.fpr[12] = ctx.fpr[0];
    call_setf(rt, ctx, 0x08B663B4u, {}, {ctx.fpr[0]});
    ctx.fpr[12] = ctx.fpr[0];
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {ctx.fpr[12]});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08AA3E4C: the same as 0x08AA3568 with slot 0x54 and 0x08B664A0.

// 0x08AB4724 (a0 = object): the same string release on the word at +0x24, vtable 0x08CD4A58, then
// 0x08B1E9F0(object + 0x14) and 0x08A780B0(object).
void func_08AB4724(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 data = word(rt, self + 0x24u);
    const u32 count = data - 0xCu;
    ctx.gpr[17] = count;
    put(rt, self, 0x08CD4A58u);
    ctx.gpr[3] = 0x08CD4A58u;  // the asm keeps the vtable in v1 (the store's source)
    if (count != 0x08D74FD0u) {
        const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
        if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {count, frame.at(0u)});
    }
    call_set(rt, ctx, 0x08B1E9F0u, {self + 0x14u});
    call_set(rt, ctx, 0x08A780B0u, {self});
}

// 0x08ACC070 (a0 = this): when the word at 0x08D69170 is zero, 0x08B327AC(0x08D69170, 1) must open it; then
// 0x08ACB910(this, &block) must answer nonzero for 0x08ACBC78(this, &block) to run with the block word at +0x30
// cleared. Every failing gate returns 0 in v0.

// 0x08AF1124 (a0 = a begin / end pair): the element count ((end - begin) >> 3) is halved to one; twice the number of
// halvings goes to 0x08C3EEE0(begin, end, twice), which 0x08C3E9B0(begin, end, twice) finishes. An empty range
// returns at once.

// 0x08B0A13C (a0 = object): under the lock at +0x48, packs the bytes of the 16-slot ring at object + 8 from the
// index at +4 up to the one at +0 into a word (byte n << 8 * n), unlocks and returns it.

// 0x08B10B38 (a0 = this, a1 = other): locks this + 0x14 and other + 4 (0x08B34888), 0x08B1FCD4(this + 0x4C,
// other + 0x3C) moves the pair, other + 0xA4 is cleared, the second lock is released (0x08B34B24), 0x08B10A7C(this,
// other) runs and 0x08B34B24(this + 0x14) finishes.

// 0x08B1728C (a0 = object): 0x08B0CC04(sp) primes a 0x130-byte block, then the object's state words are set (-1 at
// +0x19F8, sp at +0x19E0, bytes +0x19DC / +0x19EC / +0x19F0 / +0x19FC zero) and the six members at +0x18D4 .. +0x19B0
// are primed (0x08B28C74); 0x08B0C808(sp) finishes.

// 0x08B2DFB4: 0x08B2DEE4() answers a 64-bit value in v0 / v1 and 0x08B2DF58() another; the loop repeats until the
// 64-bit difference is the same twice in a row, then returns the low word in v0 and the high one in v1. The previous
// difference starts at -1 / -1.

// 0x08B323E4 (a0 = out): 0x08B2E0B0(), the lock 0x08B35214(), under it 0x08B344E0(lock) and 0x08C7AF08(), then
// 0x08B318E4(0x2C00) answers a block kept at *out, the lock is released (0x08B3457C) and 0x08B2E0A4(lock) finishes.
// v0 = the 0x08C7AF08 answer.

// 0x08B33218: 0x08C7ABE8(1) and 0x08C7ABF8(0), then the one-time block at 0x08D691D8: when its word +8 is zero,
// 0x08B2E314(3, 0), 0x08B2E134(answer) and 0x08B1BA54(&block, answer) build it (the answer is kept at +8) and
// 0x08B2E214(answer) runs with the bytes at +0xC cleared.

// 0x08B35E3C (a0 = size, a1 = a source): 0x08B35DA0(source, source) answers a block; a nonzero block and size copy
// the source into it (0x08B79A88(block, size, source)) and a nonzero size is released (0x08B35254(size)). v0 = block.
void func_08B35E3C(Runtime &rt, AllegrexContext &ctx) {
    const u32 size = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = size;
    ctx.gpr[18] = source;
    ctx.gpr[19] = size != 0u ? 1u : 0u;
    const u32 block = call_set(rt, ctx, 0x08B35DA0u, {source, source});
    ctx.gpr[16] = block;
    if (block != 0u && size != 0u) call_set(rt, ctx, 0x08B79A88u, {block, size, source});
    if (size != 0u) call_set(rt, ctx, 0x08B35254u, {size});
    ctx.gpr[2] = block;
}

// 0x08B5D6B0 (a0 = out, a1 = state, a2 = a value): the index 0x08B5C2B0(value) must stay below the count at state + 8
// minus one and select a nonzero entry of the table at *(state + 4); 0x08B5D3DC(out, value, entry) finishes. Every
// invalid index goes on in 0x08B5AFDC(0x08CB57AC), which does not return.

// 0x08B85948 (a0 = this, a1 = a holder, a2 = a value): the object at *holder (reported when null through
// 0x08A5BB7C(0x08C97BA4)) answers through slot 0x4C, and the function at this[0x88] finishes (this, answer, value).

// 0x08B89BB0: the same as 0x08B85948 with the report message 0x08C98D34.

// ---- end of batch 16 bodies ----
} // namespace
} // namespace new_batch_16
} // namespace gt::game

#define NEW16(address, name, kind) GT_DECOMP(address, gt::game::new_batch_16::name, gt::ReturnKind::kind)
NEW16(0x08AD6854u, func_08AD6854, Word);
NEW16(0x08B2F060u, func_08B2F060, Word);
NEW16(0x08B643A8u, func_08B643A8, Word);
NEW16(0x08B862D8u, func_08B862D8, Word);
NEW16(0x08BD3008u, func_08BD3008, Word);
NEW16(0x08BD311Cu, func_08BD311C, Word);
NEW16(0x0889844Cu, func_0889844C, Word);
NEW16(0x089B0044u, func_089B0044, Word);
NEW16(0x089DA4E0u, func_089DA4E0, Float);
NEW16(0x08A56630u, func_08A56630, Word);
NEW16(0x08A93500u, func_08A93500, Word);
NEW16(0x08A9893Cu, func_08A9893C, Word);
NEW16(0x08B32228u, func_08B32228, Word);
NEW16(0x08B38C78u, func_08B38C78, Word);
NEW16(0x0880AF30u, func_0880AF30, Word);
NEW16(0x088A117Cu, func_088A117C, Word);
NEW16(0x088D2C8Cu, func_088D2C8C, Float);
NEW16(0x08B35E3Cu, func_08B35E3C, Word);
NEW16(0x08A70A48u, func_08A70A48, Word);
NEW16(0x08AA3DD0u, func_08AA3DD0, Word);
NEW16(0x08AB4724u, func_08AB4724, Word);
// func_08B54104 skipped: a VFPU ray/segment computation (lwl/lwr scalars into S010-S012, vhtfm4.q / vtfm4.q /
// vadd.q / vrcp.s / vscl.t / vsub.t / vdot.t / vsqrt.s) whose intermediate matrix layout the vfpu helpers cannot
// express without inventing it; not readable structured C.
// func_08B17104 skipped: its first five instructions hand this + 0x2C to 0x08B7B34C, which reloads sp/ra from that
// block and returns through the restored ra (never to the next instruction); the remaining 24 instructions are a
// separate constructor body (no functions.tsv entry at 0x08B17118) that the recompiler's boundary swept into this unit.
